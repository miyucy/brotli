# frozen_string_literal: true

require "test_helper"

class BrotliTest < Test::Unit::TestCase
  def testdata
    @testdata ||= File.binread(
      File.expand_path(File.join("..", "vendor", "brotli", "tests", "testdata", "alice29.txt"), __dir__)
    )
  end

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

  test ".version" do
    assert do
      puts "Brotli version: #{Brotli.version}"
      ["1.1.0", "1.0.9"].include? Brotli.version
    end
  end

  sub_test_case ".deflate" do
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
      assert_raise TypeError do
        Brotli.deflate(testdata, 42)
      end
    end
  end

  sub_test_case ".inflate" do
    def testdata2
      @testdata2 ||= File.binread(
        File.expand_path(File.join("..", "vendor", "brotli", "tests", "testdata", "alice29.txt.compressed"), __dir__)
      )
    end

    test "works" do
      assert_equal testdata, Brotli.inflate(testdata2)
    end

    test "works with StringIO" do
      testdata3 = StringIO.new testdata2
      assert_equal testdata, Brotli.inflate(testdata3)
    end

    test "works with File" do
      f = File.open(File.expand_path(File.join("..", "vendor", "brotli", "tests", "testdata", "alice29.txt.compressed"), __dir__))
      assert_equal testdata, Brotli.inflate(f)
    end

    test "raise error when pass insufficient data" do
      assert_raise Brotli::Error do
        Brotli.inflate(testdata2[0, 64])
      end
    end

    test "raise error when pass invalid data" do
      assert_raise Brotli::Error do
        Brotli.inflate(testdata2.reverse)
      end
    end
  end

  sub_test_case "dictionary support" do
    def setup
      omit "Dictionary tests are skipped" if Brotli.version < "1.1.0"
    end

    def dictionary_data
      "The quick brown fox jumps over the lazy dog"
    end

    def repetitive_data
      dictionary_data * 10
    end

    test "deflate with dictionary produces smaller output" do
      compressed_no_dict = Brotli.deflate(repetitive_data)
      compressed_with_dict = Brotli.deflate(repetitive_data, dictionary: dictionary_data)

      assert compressed_with_dict.bytesize < compressed_no_dict.bytesize
    end

    test "inflate with matching dictionary works" do
      compressed = Brotli.deflate(repetitive_data, dictionary: dictionary_data)
      decompressed = Brotli.inflate(compressed, dictionary: dictionary_data)

      assert_equal repetitive_data, decompressed
    end

    test "inflate without dictionary fails on dictionary-compressed data" do
      compressed = Brotli.deflate(repetitive_data, dictionary: dictionary_data)

      assert_raise Brotli::Error do
        Brotli.inflate(compressed)
      end
    end

    test "inflate with wrong dictionary fails" do
      compressed = Brotli.deflate(repetitive_data, dictionary: dictionary_data)
      wrong_dict = "A completely different dictionary"

      assert_raise Brotli::Error do
        Brotli.inflate(compressed, dictionary: wrong_dict)
      end
    end

    test "deflate and inflate work with binary dictionary" do
      binary_dict = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09" * 10
      data = "Test data " * 100 + binary_dict

      compressed = Brotli.deflate(data, dictionary: binary_dict)
      decompressed = Brotli.inflate(compressed, dictionary: binary_dict)

      assert_equal data, decompressed
    end

    test "deflate with dictionary and other options" do
      compressed = Brotli.deflate(repetitive_data,
                                 dictionary: dictionary_data,
                                 quality: 11,
                                 mode: :text)
      decompressed = Brotli.inflate(compressed, dictionary: dictionary_data)

      assert_equal repetitive_data, decompressed
    end
  end

  sub_test_case "Ractor safe" do
    test "able to invoke non-main ractor" do
      unless defined? ::Ractor
        notify "Ractor not defined"
        omit
      end
      ractors = Array.new(2) do
        Ractor.new(testdata) do |testdata|
          Brotli.inflate(Brotli.deflate(testdata)) == testdata
        end
      end
      assert_equal [true, true], ractors.map(&:take)
    end
  end
end
