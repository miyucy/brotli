require 'mkmf'
require 'fileutils'
require 'rbconfig'

$CPPFLAGS << ' -DOS_MACOSX' if RbConfig::CONFIG['host_os'] =~ /darwin|mac os/
$INCFLAGS << ' -I$(srcdir)/enc -I$(srcdir)/dec -I$(srcdir)/common -I$(srcdir)/include'
create_makefile('brotli/brotli')

__DIR__ = File.expand_path(File.dirname __FILE__)

%w[enc dec common include].each do |dirname|
  FileUtils.mkdir_p dirname
  FileUtils.cp_r File.expand_path(File.join(__DIR__, '..', '..', 'vendor', 'brotli', 'c', dirname), __DIR__), __DIR__, verbose: true
end

srcs = []
objs = []
Dir[File.expand_path(File.join('{enc,dec,common,include}', '**', '*.c'), __DIR__)].sort.each do |file|
  file[__DIR__ + File::SEPARATOR] = ''
  srcs << file
  objs << file.sub(/\.c\z/, '.' + RbConfig::CONFIG['OBJEXT'])
end

File.open('Makefile', 'r+') do |f|
  src = 'ORIG_SRCS = brotli.c buffer.c'
  obj = 'OBJS = brotli.o buffer.o'
  txt = f.read
        .sub(/^ORIG_SRCS = .*$/, src + ' ' + srcs.join(' '))
        .sub(/^OBJS = .*$/, obj + ' ' + objs.join(' '))
  f.rewind
  f.write txt
end
