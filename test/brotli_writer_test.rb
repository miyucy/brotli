# frozen_string_literal: true

require "test_helper"
require "tempfile"

class BrotliWriterTest < Test::Unit::TestCase
  class CloseErrorIO
    attr_reader :close_calls

    def initialize
      @io = StringIO.new
      @close_calls = 0
    end

    def write(data)
      @io.write(data)
    end

    def flush
      @io.flush
    end

    def close
      @close_calls += 1
      @io.close
      raise IOError, "close failed"
    end

    def closed?
      @io.closed?
    end
  end

  def setup
    @tempfile = Tempfile.new
  end

  def cleanup
    @tempfile.close!
  end

  def testdata
    @testdata ||= File.binread(
      File.expand_path(File.join("..", "vendor", "brotli", "tests", "testdata", "alice29.txt"), __dir__)
    )
  end

  test "works" do
    writer = Brotli::Writer.new @tempfile
    assert_equal false, writer.closed?
    assert_equal testdata.bytesize, writer.write(testdata)
    assert_equal @tempfile, writer.close
    assert_equal true, writer.closed?
    assert_equal true, @tempfile.closed?

    @tempfile.open
    assert_equal testdata, Brotli.inflate(@tempfile.read)
  end

  test "flush writes a fully decodable prefix" do
    data = ("hello world\n" * 5_000).b
    writer = Brotli::Writer.new(@tempfile)

    assert_equal data.bytesize, writer.write(data)
    assert_equal writer, writer.flush

    @tempfile.rewind
    decompressor = Brotli::Decompressor.new
    assert_equal data, decompressor.process(@tempfile.read)
    assert_equal false, decompressor.finished?

    writer.close
  end

  test "close still closes io when finish raises" do
    original_finish = Brotli::Compressor.instance_method(:finish)
    Brotli::Compressor.class_eval do
      remove_method :finish
      define_method(:finish) do
        raise Brotli::Error, "finish failed"
      end
    end

    writer = Brotli::Writer.new(@tempfile)

    error = assert_raise(Brotli::Error) do
      writer.close
    end

    assert_equal "finish failed", error.message
    assert_equal true, writer.closed?
    assert_equal true, @tempfile.closed?

    closed_error = assert_raise(Brotli::Error) do
      writer.write("abc")
    end
    assert_equal "Writer is closed", closed_error.message
  ensure
    Brotli::Compressor.class_eval do
      remove_method :finish
      define_method(:finish, original_finish)
    end
  end

  test "close marks writer closed when io.close raises" do
    io = CloseErrorIO.new
    writer = Brotli::Writer.new(io)

    assert_equal 5, writer.write("hello")

    error = assert_raise(IOError) do
      writer.close
    end

    assert_equal "close failed", error.message
    assert_equal true, writer.closed?
    assert_equal true, io.closed?
    assert_equal 1, io.close_calls
    assert_equal io, writer.close

    closed_error = assert_raise(Brotli::Error) do
      writer.write("abc")
    end
    assert_equal "Writer is closed", closed_error.message
  end

  test "raise" do
    assert_raise do
      Brotli::Writer.new nil
    end
  end

  test "works with dictionary" do
    omit_if(Brotli.version < "1.1.0", "Dictionary tests are skipped")

    dictionary = "The quick brown fox jumps over the lazy dog"
    data = dictionary * 10

    writer = Brotli::Writer.new @tempfile, dictionary: dictionary
    assert_equal data.bytesize, writer.write(data)
    assert_equal @tempfile, writer.close

    @tempfile.open
    compressed = @tempfile.read
    decompressed = Brotli.inflate(compressed, dictionary: dictionary)
    assert_equal data, decompressed
  end

  test "writer with dictionary produces smaller output" do
    omit_if(Brotli.version < "1.1.0", "Dictionary tests are skipped")

    dictionary = "The quick brown fox jumps over the lazy dog"
    data = dictionary * 10

    # Without dictionary
    @tempfile.rewind
    writer_no_dict = Brotli::Writer.new @tempfile
    writer_no_dict.write(data)
    writer_no_dict.close
    size_no_dict = @tempfile.size

    # With dictionary
    tempfile2 = Tempfile.new
    writer_with_dict = Brotli::Writer.new tempfile2, dictionary: dictionary
    writer_with_dict.write(data)
    writer_with_dict.close
    size_with_dict = tempfile2.size

    assert size_with_dict < size_no_dict

    tempfile2.close!
  end

  test "writer with dictionary and other options" do
    omit_if(Brotli.version < "1.1.0", "Dictionary tests are skipped")

    dictionary = "compression dictionary"
    data = "test data " * 100 + dictionary * 5

    writer = Brotli::Writer.new @tempfile, dictionary: dictionary, quality: 11, mode: :text
    writer.write(data)
    writer.finish
    writer.close

    @tempfile.open
    decompressed = Brotli.inflate(@tempfile.read, dictionary: dictionary)
    assert_equal data, decompressed
  end
end
