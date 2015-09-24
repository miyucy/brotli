require 'mkmf'
require 'fileutils'
require 'rbconfig'

$CXXFLAGS << ' -std=c++11'
$CPPFLAGS << ' -DOS_MACOSX' if RbConfig::CONFIG['host_os'] =~ /darwin|mac os/
$INCFLAGS << ' -I./enc -I./dec'

def make_srcobj(prefix, files)
  FileUtils.mkdir_p File.join(__dir__, prefix)
  FileUtils.cp(Dir[File.expand_path(File.join('..', '..', 'vendor', 'brotli', prefix, '*.{c,cc,h}'), __dir__)], File.join(__dir__, prefix))
  [
    files.flat_map { |e| Dir[File.join(__dir__, prefix, e + '.{c,cc}')] }.map { |e| e.sub(__dir__ + '/', '') },
    files.map { |e| File.join(prefix, e + '.' + $OBJEXT) },
  ]
end

FileUtils.mkdir_p 'enc'
FileUtils.mkdir_p 'dec'
encsrc, encobj = make_srcobj 'enc', %w(backward_references block_splitter brotli_bit_stream encode encode_parallel entropy_encode histogram literal_cost metablock static_dict streams)
decsrc, decobj = make_srcobj 'dec', %w(bit_reader decode huffman state streams)

have_library('stdc++')
create_makefile('brotli/brotli')

File.open('Makefile', 'r+') do |f|
  src = 'ORIG_SRCS = brotli.cc'
  obj = 'OBJS = brotli.o'
  txt = f.read
        .sub(/^#{src}$/, src + ' ' + (encsrc + decsrc).join(' '))
        .sub(/^#{obj}$/, obj + ' ' + (encobj + decobj).join(' '))
  f.rewind
  f.write txt
end
