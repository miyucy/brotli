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
end
