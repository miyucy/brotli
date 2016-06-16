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
  'dec/bit_reader.c',
  'dec/bit_reader.h',
  'dec/context.h',
  'dec/decode.c',
  'dec/decode.h',
  'dec/dictionary.c',
  'dec/dictionary.h',
  'dec/huffman.c',
  'dec/huffman.h',
  'dec/port.h',
  'dec/prefix.h',
  'dec/state.c',
  'dec/state.h',
  'dec/transform.h',
  'dec/types.h',
  'enc/backward_references.cc',
  'enc/backward_references.h',
  'enc/bit_cost.h',
  'enc/block_splitter.cc',
  'enc/block_splitter.h',
  'enc/brotli_bit_stream.cc',
  'enc/brotli_bit_stream.h',
  'enc/cluster.h',
  'enc/command.h',
  'enc/compress_fragment.cc',
  'enc/compress_fragment.h',
  'enc/compress_fragment_two_pass.cc',
  'enc/compress_fragment_two_pass.h',
  'enc/compressor.h',
  'enc/context.h',
  #'enc/dictionary.cc',
  'enc/dictionary.h',
  'enc/dictionary_hash.h',
  'enc/encode.cc',
  'enc/encode.h',
  'enc/encode_parallel.cc',
  'enc/encode_parallel.h',
  'enc/entropy_encode.cc',
  'enc/entropy_encode.h',
  'enc/entropy_encode_static.h',
  'enc/fast_log.h',
  'enc/find_match_length.h',
  'enc/hash.h',
  'enc/histogram.cc',
  'enc/histogram.h',
  'enc/literal_cost.cc',
  'enc/literal_cost.h',
  'enc/metablock.cc',
  'enc/metablock.h',
  'enc/port.h',
  'enc/prefix.h',
  'enc/ringbuffer.h',
  'enc/static_dict.cc',
  'enc/static_dict.h',
  'enc/static_dict_lut.h',
  'enc/streams.cc',
  'enc/streams.h',
  'enc/transform.h',
  'enc/types.h',
  'enc/utf8_util.cc',
  'enc/utf8_util.h',
  'enc/write_bits.h',
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
