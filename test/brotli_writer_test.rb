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
    assert_equal testdata.bytesize, writer.write(testdata)
    assert_equal @tempfile, writer.close
    assert_equal true, @tempfile.closed?

    @tempfile.open
    assert_equal testdata, Brotli.inflate(@tempfile.read)
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
