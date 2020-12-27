# frozen_string_literal: true

require "test_helper"

class BrotliTest < Test::Unit::TestCase
  test "VERSION" do
    assert do
      ::Brotli.const_defined?(:VERSION)
    end
  end

  test "well done" do
    property_of { string }.check do |string|
      assert_equal string, Brotli.inflate(Brotli.deflate(string.dup))
    end
  end

  sub_test_case ".version" do
    test "returns string" do
      assert_equal "1.0.9", Brotli.version
    end
  end
end
