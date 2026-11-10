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
    puts "Brotli version: #{Brotli.version}"
    assert_operator Gem::Version.new(Brotli.version), :>=, Gem::Version.new("1.2.0")
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

    test "raise ArgumentError if numeric option is non-integer" do
      assert_raise ArgumentError do
        Brotli.deflate(testdata, quality: "foo")
      end
      assert_raise ArgumentError do
        Brotli.deflate(testdata, lgwin: "foo")
      end
      assert_raise ArgumentError do
        Brotli.deflate(testdata, lgblock: Object.new)
      end
    end

    test "raise ArgumentError if quality is a Float" do
      assert_raise ArgumentError do
        Brotli.deflate(testdata, quality: 5.0)
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

  sub_test_case "Ractor safe" do
    test "deflate and inflate work in non-main Ractor" do
      omit "Ractor not defined" unless defined?(::Ractor)

      ractors = Array.new(2) do
        Ractor.new(testdata) do |testdata|
          Brotli.inflate(Brotli.deflate(testdata)) == testdata
        end
      end
      result_method = Ractor.instance_method(:value) rescue Ractor.instance_method(:take)
      assert_equal [true, true], ractors.map { |r| result_method.bind_call(r) }
    end
  end

  sub_test_case "interrupt while the GVL is released" do
    GVLInterrupt = Class.new(StandardError)

    # Queues Thread#raise while it is masked, then unmasks it only for blocking
    # operations, so it fires when the C call releases the GVL rather than
    # before or after the call. Returns whether the code after the call ran.
    def raise_inside(&call)
      ready = Queue.new
      finished = false
      thread = Thread.new do
        Thread.current.report_on_exception = false
        Thread.handle_interrupt(GVLInterrupt => :never) do
          ready << true
          Thread.pass until Thread.pending_interrupt?
          Thread.handle_interrupt(GVLInterrupt => :on_blocking) do
            call.call
            finished = true
          end
        end
      end
      ready.pop
      thread.raise(GVLInterrupt, "interrupt")
      assert_raise(GVLInterrupt) { thread.join }
      finished
    end

    test "inflate re-raises the interrupt" do
      compressed = Brotli.deflate(testdata)

      assert_false raise_inside { Brotli.inflate(compressed) }
      assert_equal testdata, Brotli.inflate(compressed)
    end

    test "deflate re-raises the interrupt" do
      assert_false raise_inside { Brotli.deflate(testdata) }
      assert_equal testdata, Brotli.inflate(Brotli.deflate(testdata))
    end
  end

  sub_test_case "dictionary support" do
    def dictionary_data
      "The quick brown fox jumps over the lazy dog"
    end

    def repetitive_data
      dictionary_data * 10
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

    test "deflate raises TypeError if dictionary is not a string" do
      assert_raise TypeError do
        Brotli.deflate(repetitive_data, dictionary: Object.new)
      end
    end

    test "inflate raises TypeError if dictionary is not a string" do
      compressed = Brotli.deflate(repetitive_data, dictionary: dictionary_data)

      assert_raise TypeError do
        Brotli.inflate(compressed, dictionary: Object.new)
      end
    end
  end
end
