# frozen_string_literal: true

require "test_helper"

class BrotliStreamTest < Test::Unit::TestCase
  def testdata
    @testdata ||= File.binread(
      File.expand_path(File.join("..", "vendor", "brotli", "tests", "testdata", "alice29.txt"), __dir__)
    )
  end

  def fixture(name)
    File.expand_path(File.join("..", "vendor", "brotli", "tests", "testdata", name), __dir__)
  end

  test "compressor process and finish" do
    compressor = Brotli::Compressor.new
    compressed = compressor.process(testdata)
    compressed << compressor.finish

    assert_equal testdata, Brotli.inflate(compressed)
  end

  test "compressor process, flush and finish across chunks" do
    compressor = Brotli::Compressor.new
    compressed = +""

    testdata.bytes.each_slice(1024) do |chunk|
      compressed << compressor.process(chunk.pack("C*"))
      compressed << compressor.flush
    end
    compressed << compressor.finish

    assert_equal testdata, Brotli.inflate(compressed)
  end

  test "compressor rejects process and flush after finish" do
    compressor = Brotli::Compressor.new
    compressor.process("abc")
    compressor.finish

    assert_raise Brotli::Error do
      compressor.process("def")
    end

    assert_raise Brotli::Error do
      compressor.flush
    end

    assert_equal "", compressor.finish
  end

  test "decompressor process in chunks" do
    compressed = Brotli.deflate(testdata)
    decompressor = Brotli::Decompressor.new
    decompressed = +""

    compressed.bytes.each_slice(7) do |chunk|
      decompressed << decompressor.process(chunk.pack("C*"))
    end

    assert_equal true, decompressor.is_finished
    assert_equal true, decompressor.finished?
    assert_equal false, decompressor.can_accept_more_data
    assert_equal testdata, decompressed
  end

  test "decompressor detects excessive input" do
    decompressor = Brotli::Decompressor.new

    assert_raise Brotli::Error do
      decompressor.process(Brotli.deflate("abc") + "x")
    end
  end

  test "decompressor accepts empty input after finish and rejects non-empty" do
    decompressor = Brotli::Decompressor.new
    compressed = Brotli.deflate("abc")

    assert_equal "abc", decompressor.process(compressed)
    assert_equal true, decompressor.is_finished
    assert_equal "", decompressor.process("")

    assert_raise Brotli::Error do
      decompressor.process("x")
    end
  end

  test "decompressor output_buffer_limit can bound streaming output" do
    compressed = File.binread(fixture("zerosukkanooa.compressed"))
    expected = File.binread(fixture("zerosukkanooa"))
    decompressor = Brotli::Decompressor.new
    decompressed = +""
    chunk_size = 10 * 1024
    offset = 0

    until decompressor.finished?
      input = +""
      if decompressor.can_accept_more_data
        input = compressed.byteslice(offset, chunk_size) || +""
        offset += input.bytesize
      end

      output = decompressor.process(input, output_buffer_limit: 10_240)
      assert_operator output.bytesize, :<=, 10_240
      decompressed << output
    end

    assert_equal compressed.bytesize, offset
    assert_equal expected, decompressed
  end

  test "decompressor rejects new input while output is still pending" do
    compressed = File.binread(fixture("zerosukkanooa.compressed"))
    decompressor = Brotli::Decompressor.new

    output = decompressor.process(compressed.byteslice(0, compressed.bytesize - 1), output_buffer_limit: 10_240)

    assert_operator output.bytesize, :<=, 10_240
    assert_equal false, decompressor.can_accept_more_data

    assert_raise Brotli::Error do
      decompressor.process(compressed.byteslice(-1, 1))
    end
  end

  test "decompressor can resume with a different output_buffer_limit" do
    compressed = File.binread(fixture("zerosukkanooa.compressed"))
    expected = File.binread(fixture("zerosukkanooa"))
    decompressor = Brotli::Decompressor.new
    decompressed = +""

    output = decompressor.process(compressed.byteslice(0, compressed.bytesize - 1), output_buffer_limit: 10_240)
    assert_operator output.bytesize, :<=, 10_240
    decompressed << output

    until decompressor.can_accept_more_data
      output = decompressor.process("", output_buffer_limit: 2_048)
      assert_operator output.bytesize, :<=, 2_048
      decompressed << output
    end

    decompressed << decompressor.process(compressed.byteslice(-1, 1))

    assert_equal expected, decompressed
  end

  sub_test_case "dictionary support" do
    def setup
      omit "Dictionary tests are skipped" if Brotli.version < "1.1.0"
    end

    test "stream compressor and decompressor work with dictionary" do
      dictionary = "The quick brown fox jumps over the lazy dog"
      data = dictionary * 12
      compressor = Brotli::Compressor.new(dictionary: dictionary)
      compressed = compressor.process(data) + compressor.finish

      decompressor = Brotli::Decompressor.new(dictionary: dictionary)
      decompressed = decompressor.process(compressed)

      assert_equal true, decompressor.is_finished
      assert_equal data, decompressed
    end

    test "decompressor without dictionary fails for dictionary stream" do
      dictionary = "The quick brown fox jumps over the lazy dog"
      data = dictionary * 12
      compressor = Brotli::Compressor.new(dictionary: dictionary)
      compressed = compressor.process(data) + compressor.finish
      decompressor = Brotli::Decompressor.new

      assert_raise Brotli::Error do
        decompressor.process(compressed)
      end
    end
  end
end
