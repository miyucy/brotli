require "bundler/setup"
require "bundler/gem_tasks"
require "rake/clean"
require "rake/testtask"
require "rake/extensiontask"

CLEAN.include("ext/brotli/common")
CLEAN.include("ext/brotli/dec")
CLEAN.include("ext/brotli/enc")
CLEAN.include("ext/brotli/include")

Rake::TestTask.new(:test) do |t|
  t.libs << "test"
  t.test_files = FileList["test/**/*_test.rb"]
  t.warning = true
  t.verbose = true
end

Rake::ExtensionTask.new("brotli") do |ext|
  ext.lib_dir = "lib/brotli"
end

task :build => :compile
task :test => :compile
task :default => :test
