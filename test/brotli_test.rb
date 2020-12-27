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

  sub_test_case ".deflate" do
    def testdata
      @testdata ||= File.binread(
        File.expand_path(File.join("..", "vendor", "brotli", "tests", "testdata", "alice29.txt"), __dir__)
      )
    end

    test "works" do
      property_of {
        [choose(nil, :generic, :text, :font), range(0, 11), range(10, 24), range(16, 24)]
      }.check do |(mode, quality, lgwin, lgblock)|
        assert_nothing_raised ArgumentError do
          Brotli.deflate(
            testdata,
            mode: mode, quality: quality, lgwin: lgwin, lgblock: lgblock
          )
        end
      end
    end

    test "raise ArgumentError if given invalid options" do
      assert_raise ArgumentError do
        Brotli.deflate(testdata, mode: "generic")
      end
      assert_raise ArgumentError do
        Brotli.deflate(testdata, quality: 12)
      end
      assert_raise ArgumentError do
        Brotli.deflate(testdata, lgwin: 9)
      end
      assert_raise ArgumentError do
        Brotli.deflate(testdata, lgwin: 25)
      end
      assert_raise ArgumentError do
        Brotli.deflate(testdata, lgblock: 15)
      end
      assert_raise ArgumentError do
        Brotli.deflate(testdata, lgblock: 25)
      end
    end
  end

  sub_test_case ".inflate" do
    data do
      path = Pathname.new(
        File.expand_path(File.join("..", "vendor", "brotli", "tests", "testdata"), __dir__)
      )
      {
        "alice29.txt" => [
          (path + "alice29.txt").binread,
          (path + "alice29.txt.compressed").binread
        ]
      }
    end
    test "works" do |(raw, compressed)|
      assert_equal raw, Brotli.inflate(compressed)
    end
  end
end
