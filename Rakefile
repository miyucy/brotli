require "bundler/setup"
require "bundler/gem_tasks"
require "rake/clean"
require "rake/testtask"
require "rake/extensiontask"
require "shellwords"

CLEAN.include("ext/brotli/common")
CLEAN.include("ext/brotli/dec")
CLEAN.include("ext/brotli/enc")
CLEAN.include("ext/brotli/include")

test_config = lambda do |t|
  t.libs << "test"
  t.test_files = FileList["test/**/*_test.rb"]
  t.warning = true
  t.verbose = true
end

Rake::TestTask.new(:test, &test_config)

# Valgrind-backed test task (ruby_memcheck). Only available when the gem is
# installed (it lives in the :development group, present in the leak image).
begin
  require "ruby_memcheck"
  RubyMemcheck.config(valgrind_options: ["--error-exitcode=99"])
  namespace :test do
    RubyMemcheck::TestTask.new(valgrind: :compile, &test_config)
  end
rescue LoadError
  # ruby_memcheck not installed; `rake test:valgrind` is simply unavailable.
end

Rake::ExtensionTask.new("brotli") do |ext|
  ext.lib_dir = "lib/brotli"
end

task build: :compile
task test: :compile
task default: :test

task :docker do
  gcc_versions = ["14", "15"]
  brotli_configs = [true, false]
  gcc_versions.product(brotli_configs).each do |gcc_version, use_system_brotli|
    command = "docker build "\
              "--progress=plain "\
              "--build-arg GCC_VERSION=#{gcc_version} "\
              "--build-arg USE_SYSTEM_BROTLI=#{use_system_brotli} "\
              "-t brotli:#{gcc_version}#{use_system_brotli ? "-use_system_brotli" : ""} ."
    puts "Running: #{command}"
    system command
    unless $?.exited?
      raise "Docker build failed for GCC version #{gcc_version} with use_system_brotli=#{use_system_brotli}"
    end
  end
end

# ---------------------------------------------------------------------------
# Local memory-leak detection harness (see leak/README.md). Local-only, not CI.
# Most methods need Linux, so they run in the leak/ Docker image; macOS `leaks`
# runs natively.
# ---------------------------------------------------------------------------
namespace :leak do
  IMAGE = "brotli-leak".freeze

  # Run a command inside the leak image (repo baked in at /app).
  def drun(cmd, docker_opts = "")
    sh "docker run --rm #{docker_opts} #{IMAGE} bash -lc #{Shellwords.escape(cmd)}"
  end

  desc "Build the leak-detection toolchain image"
  task :image do
    sh "docker build -f leak/Dockerfile -t #{IMAGE} ."
  end

  desc "Valgrind memcheck over the test suite (ruby_memcheck)"
  task valgrind: :image do
    drun "bundle exec rake test:valgrind"
  end

  desc "Valgrind massif heap profiler over the workload"
  task massif: :image do
    drun "valgrind --tool=massif --time-unit=B --massif-out-file=/tmp/massif.out " \
         "ruby -Ilib leak/workload.rb >/dev/null 2>&1; ms_print /tmp/massif.out | head -100"
  end

  desc "AddressSanitizer corruption scan (overflow/UAF) over the workload"
  task asan: :image do
    # detect_leaks=0: LeakSanitizer's stop-the-world scan deadlocks with Ruby's
    # threads on a stock (non-ASan) Ruby, so ASan is used here for memory
    # corruption only. Leaks are covered by valgrind/massif/leaks/memlimit.
    drun "BROTLI_ASAN=1 bundle exec rake clobber compile -- --enable-vendor >/dev/null 2>&1 && " \
         "ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 " \
         "LD_PRELOAD=$(gcc -print-file-name=libasan.so) " \
         "LEAK_ITER=#{ENV.fetch('LEAK_ITER', '50')} ruby -Ilib leak/workload.rb && " \
         "echo 'ASan: no corruption detected across the workload'"
  end

  desc "libfiu fault injection (force malloc/realloc failures)"
  task fiu: :image do
    spec = ENV.fetch("FIU", "enable_random name=libc/mm/*,probability=0.02")
    drun "LEAK_FAULT=1 LEAK_ITER=#{ENV.fetch('LEAK_ITER', '300')} " \
         "fiu-run -x -c #{Shellwords.escape(spec)} ruby -Ilib leak/workload.rb"
  end

  desc "Custom LD_PRELOAD shim fault injection (FAIL_REALLOC_N / FAIL_SIZE)"
  task faults: :image do
    env = %w[FAIL_MALLOC_N FAIL_REALLOC_N FAIL_SIZE].filter_map { |k| "#{k}=#{ENV[k]}" if ENV[k] }
    env = ["FAIL_REALLOC_N=1"] if env.empty?
    drun "gcc -shared -fPIC -o /tmp/failmalloc.so leak/failmalloc.c -ldl && " \
         "LEAK_FAULT=1 LEAK_ITER=#{ENV.fetch('LEAK_ITER', '50')} #{env.join(' ')} " \
         "LD_PRELOAD=/tmp/failmalloc.so ruby -Ilib leak/workload.rb"
  end

  desc "Memory-limited growth canary (exit 137 == OOM-killed == growth)"
  task memlimit: :image do
    iter = ENV.fetch("LEAK_ITER", "200000")
    sh "docker run --rm --memory=64m --memory-swap=64m -e LEAK_ITER=#{iter} " \
       "#{IMAGE} ruby -Ilib leak/workload.rb; echo \"exit=$?\""
  end

  desc "macOS native leaks (no Docker; runs on the host)"
  task :macos do
    sh "leak/macos-leaks.sh"
  end

  desc "Self-test: prove every detector catches a planted leak"
  task selftest: :image do
    sh "leak/selftest.sh #{IMAGE}"
  end

  desc "Run all Docker leak methods in sequence"
  task all: %i[valgrind massif asan fiu faults memlimit]
end
