require "mkmf"
require "fileutils"
require "rbconfig"

dir_config("brotli")

# libbrotli-dev
have_dev_pkg = [
  have_header("brotli/decode.h"),
  have_header("brotli/encode.h"),
  pkg_config("libbrotlicommon"),
  pkg_config("libbrotlidec"),
  pkg_config("libbrotlienc")
].all? { |e| e }

have_header("brotli/shared_dictionary.h")
have_func("BrotliEncoderPrepareDictionary", "brotli/encode.h")
have_func("BrotliEncoderAttachPreparedDictionary", "brotli/encode.h")
have_func("BrotliDecoderAttachDictionary", "brotli/decode.h")

if enable_config("vendor")
  have_dev_pkg = false
  Logging::message "Use vendor brotli\n"
  $defs << "-DHAVE_BROTLI_SHARED_DICTIONARY_H"
  $defs << "-DHAVE_BROTLIENCODERPREPAREDICTIONARY"
  $defs << "-DHAVE_BROTLIENCODERATTACHPREPAREDDICTIONARY"
  $defs << "-DHAVE_BROTLIDECODERATTACHDICTIONARY"
end

$CPPFLAGS << " -DOS_MACOSX" if RbConfig::CONFIG["host_os"] =~ /darwin|mac os/
$INCFLAGS << " -I$(srcdir)/enc -I$(srcdir)/dec -I$(srcdir)/common -I$(srcdir)/include" unless have_dev_pkg

create_makefile("brotli/brotli")

unless have_dev_pkg
  __DIR__ = File.expand_path(File.dirname(__FILE__))

  %w[enc dec common include].each do |dirname|
    FileUtils.rm_rf dirname
    FileUtils.mkdir_p dirname
    FileUtils.cp_r(
      File.expand_path(File.join(__DIR__, "..", "..", "vendor", "brotli", "c", dirname), __DIR__),
      __DIR__,
      verbose: true
    )
  end

  srcs = []
  objs = []
  Dir[File.expand_path(File.join("{enc,dec,common,include}", "**", "*.c"), __DIR__)].sort.each do |file|
    file[__DIR__ + File::SEPARATOR] = ""
    srcs << file
    objs << file.sub(/\.c\z/, "." + RbConfig::CONFIG["OBJEXT"])
  end

  File.open("Makefile", "r+") do |f|
    obj_ext = RbConfig::CONFIG["OBJEXT"]
    src = "ORIG_SRCS = brotli.c buffer.c"
    obj = "OBJS = brotli.#{obj_ext} buffer.#{obj_ext}"
    txt = f.read
           .sub(/^ORIG_SRCS = .*$/, "#{src} #{srcs.join(" ")}")
           .sub(/^OBJS = .*$/, "#{obj} #{objs.join(" ")}")
    f.rewind
    f.write txt
  end
end
