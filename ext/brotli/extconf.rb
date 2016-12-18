require 'mkmf'
require 'fileutils'
require 'rbconfig'

$CPPFLAGS << ' -DOS_MACOSX' if RbConfig::CONFIG['host_os'] =~ /darwin|mac os/
$INCFLAGS << ' -I./enc -I./dec -I./common'
create_makefile('brotli/brotli')

def acopy(dir)
  # source dir
  FileUtils.mkdir_p File.expand_path(File.join(__dir__, dir))
  # object dir
  FileUtils.mkdir_p dir

  files = Dir[File.expand_path(File.join(__dir__, '..', '..', 'vendor', 'brotli', dir, '*.[ch]'))]
  FileUtils.cp files, dir, verbose: true

  srcs = files.map { |e| File.basename e }.select { |e| e.end_with? '.c' }.map { |e| File.join(dir, e) }
  objs = srcs.map { |e| e.sub(/\.c\z/, '.' + $OBJEXT) }
  [srcs, objs]
end

srcs = []
objs = []
%w(enc dec common).each do |dir|
  a, b = acopy dir
  srcs.concat a
  objs.concat b
end

File.open('Makefile', 'r+') do |f|
  src = 'ORIG_SRCS = brotli.c buffer.c'
  obj = 'OBJS = brotli.o buffer.o'
  txt = f.read
        .sub(/^#{src}$/, src + ' ' + srcs.join(' '))
        .sub(/^#{obj}$/, obj + ' ' + objs.join(' '))
  f.rewind
  f.write txt
end
