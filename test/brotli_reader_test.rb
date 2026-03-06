# frozen_string_literal: true

require "test_helper"

class BrotliReaderTest < Test::Unit::TestCase
  class IncrementalCompressedIO
    def initialize(chunks)
      @chunks = chunks.dup
      @closed = false
    end

    def read(_length = nil)
      raise "unexpected blocking read"
    end

    def readpartial(_length)
      raise EOFError if @chunks.empty?

      @chunks.shift
    end

    def close
      @closed = true
    end

    def closed?
      @closed
    end
  end

  def testdata
    @testdata ||= File.binread(
      File.expand_path(File.join("..", "vendor", "brotli", "tests", "testdata", "alice29.txt"), __dir__)
    )
  end

  def incremental_compressed_io(data, slice_size: 3)
    chunks = Brotli.deflate(data).bytes.each_slice(slice_size).map { |chunk| chunk.pack("C*") }
    IncrementalCompressedIO.new(chunks)
  end

  test "read all" do
    io = StringIO.new(Brotli.deflate(testdata))
    reader = Brotli::Reader.new(io)

    assert_equal testdata, reader.read
    assert_equal "", reader.read
    assert_equal true, reader.eof?
  end

  test "read with length and outbuf" do
    io = StringIO.new(Brotli.deflate("abcdefghij"))
    reader = Brotli::Reader.new(io)
    out = +"x"

    assert_equal "abc", reader.read(3)
    assert_equal "", reader.read(0)
    assert_same out, reader.read(4, out)
    assert_equal "defg", out
    assert_equal "hij", reader.read
    assert_nil reader.read(1, out)
    assert_equal "", out
  end

  test "readpartial" do
    io = StringIO.new(Brotli.deflate("hello world"))
    reader = Brotli::Reader.new(io)
    out = +"x"

    assert_same out, reader.readpartial(5, out)
    assert_equal "hello", out
    assert_equal " worl", reader.readpartial(5)
    assert_equal "d", reader.readpartial(5)
    assert_raise EOFError do
      reader.readpartial(1, out)
    end
    assert_equal "", out
  end

  test "repeated small reads drain decompressor output before reading more input" do
    data = ("hello world\n" * 2_000)
    reader = Brotli::Reader.new(StringIO.new(Brotli.deflate(data)))
    output = +""

    while (chunk = reader.read(1))
      output << chunk
    end

    assert_equal data, output
  end

  test "repeated readpartial calls work with buffered decompressor output" do
    data = ("hello world\n" * 2_000)
    reader = Brotli::Reader.new(StringIO.new(Brotli.deflate(data)))
    output = +""

    loop do
      output << reader.readpartial(7)
    rescue EOFError
      break
    end

    assert_equal data, output
  end

  test "gets and each_line" do
    text = "alpha\nbeta\ngamma\n"
    io = StringIO.new(Brotli.deflate(text))
    reader = Brotli::Reader.new(io)

    assert_equal "alpha\n", reader.gets
    assert_equal "beta\n", reader.gets
    assert_equal "gamma\n", reader.gets
    assert_nil reader.gets

    io2 = StringIO.new(Brotli.deflate(text))
    reader2 = Brotli::Reader.new(io2)
    assert_equal ["alpha\n", "beta\n", "gamma\n"], reader2.each_line.to_a
  end

  test "small reads use readpartial on incremental io" do
    reader = Brotli::Reader.new(incremental_compressed_io("alpha\nbeta\n"))

    assert_equal false, reader.eof?
    assert_equal "a", reader.read(1)
    assert_equal "l", reader.readpartial(1)
    assert_equal "pha\n", reader.gets
    assert_equal "beta\n", reader.gets
    assert_nil reader.gets
    assert_equal true, reader.eof?
  end

  test "gets nil returns remaining data once and then nil" do
    reader = Brotli::Reader.new(StringIO.new(Brotli.deflate("hello")))

    assert_equal "hello", reader.gets(nil)
    assert_nil reader.gets(nil)

    reader2 = Brotli::Reader.new(StringIO.new(Brotli.deflate("hello")))
    assert_equal ["hello"], reader2.each_line(nil).first(2)
  end

  test "gets nil returns nil for an empty stream" do
    reader = Brotli::Reader.new(StringIO.new(Brotli.deflate("")))

    assert_nil reader.gets(nil)
  end

  test "close and closed?" do
    io = StringIO.new(Brotli.deflate("hello"))
    reader = Brotli::Reader.new(io)

    assert_equal false, reader.closed?
    assert_equal io, reader.close
    assert_equal true, reader.closed?
    assert_equal true, io.closed?
    assert_equal io, reader.close

    assert_raise Brotli::Error do
      reader.read
    end
  end

  test "raise when initialized with nil io" do
    assert_raise ArgumentError do
      Brotli::Reader.new(nil)
    end
  end

  test "raise on truncated compressed stream" do
    compressed = Brotli.deflate(testdata)
    truncated = compressed[0, compressed.bytesize / 2]
    reader = Brotli::Reader.new(StringIO.new(truncated))

    assert_raise Brotli::Error do
      reader.read
    end
  end

  test "reader ignores trailing bytes in the same compressed chunk" do
    reader = Brotli::Reader.new(StringIO.new(Brotli.deflate("abc") + "trailer"))

    assert_equal "abc", reader.read
    assert_equal "", reader.read
    assert_equal true, reader.eof?
  end

  sub_test_case "dictionary support" do
    def setup
      omit "Dictionary tests are skipped" if Brotli.version < "1.1.0"
    end

    test "reader works with dictionary" do
      dictionary = "The quick brown fox jumps over the lazy dog"
      data = dictionary * 10
      compressed = Brotli.deflate(data, dictionary: dictionary)
      reader = Brotli::Reader.new(StringIO.new(compressed), dictionary: dictionary)

      assert_equal data, reader.read
    end

    test "reader fails without required dictionary" do
      dictionary = "The quick brown fox jumps over the lazy dog"
      data = dictionary * 10
      compressed = Brotli.deflate(data, dictionary: dictionary)
      reader = Brotli::Reader.new(StringIO.new(compressed))

      assert_raise Brotli::Error do
        reader.read
      end
    end
  end
end
