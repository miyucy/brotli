# frozen_string_literal: true
require "test_helper"

class LargeWindowTest < Test::Unit::TestCase
  STREAMS = [
    "1159fd7f047e67821d3bc54a87d99742506f452497204674a990c656043d56216084fc931cf52200",
    "115efd7f047e67821d3bc54a87d99742506f452497204674a990c656043d56216084fc931cf52200"
  ].map { |hex| [hex].pack("H*").freeze }.freeze
  EXPECTED = ("Brotli PDF stream\n" * 4096).freeze

  test "large-window decoding is opt-in for one-shot and streaming APIs" do
    STREAMS.each do |data|
      assert_raise(Brotli::Error) { Brotli.inflate(data) }
      assert_raise(Brotli::Error) { Brotli.inflate(data, large_window: false) }
      assert_equal EXPECTED, Brotli.inflate(data, large_window: true)
      reader = Brotli::Reader.new(StringIO.new(data), large_window: true)
      assert_equal EXPECTED, reader.read
      assert reader.eof?
      reader.close
      decoder = Brotli::Decompressor.new(large_window: true)
      result = +""
      data.each_byte { |byte| result << decoder.process(byte.chr) }
      assert decoder.finished?
      assert_equal EXPECTED, result
      assert_raise(Brotli::Error) { Brotli.inflate(data) }
    end
  end

  test "ordinary streams still decode with the option" do
    data = Brotli.deflate(EXPECTED)
    assert_equal EXPECTED, Brotli.inflate(data, large_window: true)
  end

  test "truncated and invalid large-window streams are rejected" do
    STREAMS.each do |data|
      (0...data.bytesize).each do |length|
        assert_raise(Brotli::Error) { Brotli.inflate(data.byteslice(0, length), large_window: true) }
      end
    end
    assert_raise(Brotli::Error) { Brotli.inflate([0x11, 0x9f].pack("C*"), large_window: true) }
  end
end
