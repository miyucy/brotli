# frozen_string_literal: true

require "mkmf"
require "fileutils"
require "rbconfig"
require "rubygems/version"

VENDORED_DIRS = %w[common dec enc include].freeze
MINIMUM_BROTLI_VERSION = Gem::Version.new("1.2.0")
BUILD_DIR = Dir.pwd
VENDORED_SOURCE_DIR = File.expand_path("../../vendor/brotli/c", __dir__)

def generated_vendored_dirs
  VENDORED_DIRS.map { |dirname| File.join(BUILD_DIR, dirname) }
end

def remove_generated_vendored_dirs
  generated_vendored_dirs.each { |path| FileUtils.rm_rf(path) }
end

def copy_vendored_sources
  Dir[File.join(VENDORED_SOURCE_DIR, "{common,dec,enc,include}", "**", "*.{c,h}")].sort.each do |source|
    relative_path = source.delete_prefix("#{VENDORED_SOURCE_DIR}/")
    destination = File.join(BUILD_DIR, relative_path)

    FileUtils.mkdir_p(File.dirname(destination))
    FileUtils.cp(source, destination)
  end
end

def vendored_source_files
  Dir[File.join(BUILD_DIR, "{common,dec,enc,include}", "**", "*.c")].sort.map do |path|
    path.delete_prefix("#{BUILD_DIR}/")
  end
end

def add_vendored_sources_to_makefile
  objext = RbConfig::CONFIG["OBJEXT"]
  source_files = vendored_source_files
  object_files = source_files.map { |path| path.sub(/\.c\z/, ".#{objext}") }

  makefile = File.read("Makefile")
  makefile = makefile
             .sub(/^ORIG_SRCS = .*$/, "ORIG_SRCS = brotli.c buffer.c #{source_files.join(" ")}")
             .sub(/^OBJS = .*$/, "OBJS = brotli.#{objext} buffer.#{objext} #{object_files.join(" ")}")
  File.write("Makefile", makefile)
end

def mkmf_flags
  {
    cflags: $CFLAGS.dup,
    cxxflags: $CXXFLAGS.dup,
    cppflags: $CPPFLAGS.dup,
    defs: $defs.dup,
    incflags: $INCFLAGS.dup,
    ldflags: $LDFLAGS.dup,
    libs: $libs.dup,
    local_libs: $LOCAL_LIBS.dup
  }
end

def restore_mkmf_flags(flags)
  $CFLAGS = flags[:cflags]
  $CXXFLAGS = flags[:cxxflags]
  $CPPFLAGS = flags[:cppflags]
  $defs = flags[:defs]
  $INCFLAGS = flags[:incflags]
  $LDFLAGS = flags[:ldflags]
  $libs = flags[:libs]
  $LOCAL_LIBS = flags[:local_libs]
end

def system_brotli_version
  version = pkg_config("libbrotlicommon", "modversion")
  Gem::Version.new(version) if version
rescue ArgumentError
  nil
end

def system_brotli_available?
  flags = mkmf_flags
  version = system_brotli_version

  unless version && version >= MINIMUM_BROTLI_VERSION
    Logging.message(
      "System Brotli version %s is too old; need >= %s\n",
      version || "unknown",
      MINIMUM_BROTLI_VERSION
    )
    restore_mkmf_flags(flags)
    return false
  end

  available = pkg_config("libbrotlicommon") &&
              pkg_config("libbrotlidec") &&
              pkg_config("libbrotlienc") &&
              have_header("brotli/decode.h") &&
              have_header("brotli/encode.h") &&
              have_header("brotli/shared_dictionary.h") &&
              have_func("BrotliEncoderPrepareDictionary", "brotli/encode.h") &&
              have_func("BrotliEncoderAttachPreparedDictionary", "brotli/encode.h") &&
              have_func("BrotliDecoderAttachDictionary", "brotli/decode.h")

  restore_mkmf_flags(flags) unless available
  available
end

dir_config("brotli")
have_func("rb_gc_mark_movable")

$CPPFLAGS << " -DOS_MACOSX" if RbConfig::CONFIG["host_os"] =~ /darwin|mac os/

remove_generated_vendored_dirs

use_vendored_brotli = enable_config("vendor", false) || !system_brotli_available?

if use_vendored_brotli
  Logging.message "Using vendored Brotli sources\n"
  copy_vendored_sources
  $INCFLAGS << " -Ienc -Idec -Icommon -Iinclude"
end

create_makefile("brotli/brotli")
add_vendored_sources_to_makefile if use_vendored_brotli
