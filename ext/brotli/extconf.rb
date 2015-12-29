require 'mkmf'
require 'fileutils'
require 'rbconfig'

$CPPFLAGS << ' -DOS_MACOSX' if RbConfig::CONFIG['host_os'] =~ /darwin|mac os/
$INCFLAGS << ' -I./enc -I./dec'

FileUtils.mkdir_p 'enc'
FileUtils.mkdir_p 'dec'
FileUtils.mkdir_p File.join(__dir__, 'enc')
FileUtils.mkdir_p File.join(__dir__, 'dec')

srcs = []
objs = []
[
  'enc/backward_references.cc',
  'enc/block_splitter.cc',
  'enc/brotli_bit_stream.cc',
  'enc/encode.cc',
  'enc/entropy_encode.cc',
  'enc/histogram.cc',
  'enc/literal_cost.cc',
  'enc/metablock.cc',
  'enc/static_dict.cc',
  'enc/streams.cc',
  'enc/utf8_util.cc',
  'dec/bit_reader.c',
  'dec/decode.c',
  'dec/dictionary.c',
  'dec/huffman.c',
  'dec/streams.c',
  'dec/state.c',
  'enc/backward_references.h',
  'enc/bit_cost.h',
  'enc/block_splitter.h',
  'enc/brotli_bit_stream.h',
  'enc/cluster.h',
  'enc/command.h',
  'enc/context.h',
  'enc/dictionary.h',
  'enc/dictionary_hash.h',
  'enc/encode.h',
  'enc/entropy_encode.h',
  'enc/fast_log.h',
  'enc/find_match_length.h',
  'enc/hash.h',
  'enc/histogram.h',
  'enc/literal_cost.h',
  'enc/metablock.h',
  'enc/port.h',
  'enc/prefix.h',
  'enc/ringbuffer.h',
  'enc/static_dict.h',
  'enc/static_dict_lut.h',
  'enc/streams.h',
  'enc/transform.h',
  'enc/types.h',
  'enc/utf8_util.h',
  'enc/write_bits.h',
  'dec/bit_reader.h',
  'dec/context.h',
  'dec/decode.h',
  'dec/dictionary.h',
  'dec/huffman.h',
  'dec/prefix.h',
  'dec/port.h',
  'dec/streams.h',
  'dec/transform.h',
  'dec/types.h',
  'dec/state.h',
].each do |file|
  FileUtils.cp(File.expand_path(File.join(__dir__, '..', '..', 'vendor', 'brotli', file)),
               File.expand_path(File.join(__dir__, file)),
               verbose: true)
  if file.end_with?('.c') or file.end_with?('.cc')
    srcs.push file
    objs.push file.sub(/\.cc?\z/, '.' + $OBJEXT)
  end
end

have_library('stdc++')
create_makefile('brotli/brotli')

File.open('Makefile', 'r+') do |f|
  src = 'ORIG_SRCS = brotli.cc'
  obj = 'OBJS = brotli.o'
  txt = f.read
        .sub(/^#{src}$/, src + ' ' + srcs.join(' '))
        .sub(/^#{obj}$/, obj + ' ' + objs.join(' '))
  f.rewind
  f.write txt
end
