# frozen_string_literal: true

require "test_helper"
require "tempfile"

class BrotliWriterTest < Test::Unit::TestCase
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

  test "raise" do
    assert_raise do
      Brotli::Writer.new nil
    end
  end

  test "works with dictionary" do
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
end
