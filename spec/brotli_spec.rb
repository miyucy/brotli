require 'spec_helper'
require 'zlib'

describe Brotli do
  let!(:data) { File.binread File.expand_path(File.join(File.dirname(__FILE__), '..', 'vendor', 'brotli', 'tests', 'testdata', 'alice29.txt'), __FILE__) }

  context 'deflate/inflate' do
    it 'with example data' do
      bkup = data.dup
      compressed = Brotli.deflate(data)
      expect(data).to eq bkup
      bkup = compressed.dup
      decompressed = Brotli.inflate(compressed)
      expect(compressed).to eq bkup
      expect(decompressed).to eq data
    end

    it 'with random data' do
      property_of {
        string
      }.check { |str|
        bkup = str.dup
        compressed = Brotli.deflate(str)
        expect(str).to eq bkup
        bkup = compressed.dup
        decompressed = Brotli.inflate(compressed)
        expect(compressed).to eq bkup
        expect(decompressed).to eq str
      }
    end
  end

  it 'deflate options' do
    expect { Brotli.deflate(data, mode: :generic) }.to_not raise_error
    expect { Brotli.deflate(data, mode: :text) }.to_not raise_error
    expect { Brotli.deflate(data, mode: :font) }.to_not raise_error
    expect { Brotli.deflate(data, mode: 'generic') }.to raise_error ArgumentError
    expect { Brotli.deflate(data, mode: 'text') }.to raise_error ArgumentError
    expect { Brotli.deflate(data, mode: 'font') }.to raise_error ArgumentError

    expect { Brotli.deflate(data, quality: -1) }.to raise_error ArgumentError
    expect { Brotli.deflate(data, quality: 0) }.to_not raise_error
    expect { Brotli.deflate(data, quality: 11) }.to_not raise_error
    expect { Brotli.deflate(data, quality: 12) }.to raise_error ArgumentError

    expect { Brotli.deflate(data, lgwin: 9) }.to raise_error ArgumentError
    expect { Brotli.deflate(data, lgwin: 10) }.to_not raise_error
    expect { Brotli.deflate(data, lgwin: 24) }.to_not raise_error
    expect { Brotli.deflate(data, lgwin: 25) }.to raise_error ArgumentError

    expect { Brotli.deflate(data, lgblock: 15) }.to raise_error ArgumentError
    expect { Brotli.deflate(data, lgblock: 16) }.to_not raise_error
    expect { Brotli.deflate(data, lgblock: 24) }.to_not raise_error
    expect { Brotli.deflate(data, lgblock: 25) }.to raise_error ArgumentError
    expect { Brotli.deflate(data, lgblock: -1) }.to raise_error ArgumentError
    expect { Brotli.deflate(data, lgblock: 0) }.to_not raise_error
    expect { Brotli.deflate(data, lgblock: 1) }.to raise_error ArgumentError
  end

  context 'benchmark' do
    it 'ratio' do
      compressed = Zlib.deflate(data, Zlib::BEST_COMPRESSION)
      zlib_compressed_bytesize = compressed.bytesize
      zlib_compression_ratio = (compressed.bytesize / data.bytesize.to_f * 100).round(3)
      puts "Zlib size: #{zlib_compressed_bytesize}, compress ratio: #{zlib_compression_ratio} %"

      compressed = Brotli.deflate(data)
      brotli_compressed_bytesize = compressed.bytesize
      brotli_compression_ratio = (compressed.bytesize / data.bytesize.to_f * 100).round(3)
      puts "Brotli size: #{brotli_compressed_bytesize}, compress ratio: #{brotli_compression_ratio} %"

      expect(brotli_compressed_bytesize).to be < zlib_compressed_bytesize
      expect(brotli_compression_ratio).to be < zlib_compression_ratio

      compressed = Brotli.deflate(data, mode: :text, quality: 11, lgwin: 24, lgblock: 0)
      puts "Brotli(text/11/24/0) size: #{compressed.bytesize}, compress ratio: #{(compressed.bytesize / data.bytesize.to_f * 100).round(3)} %"
      compressed = Brotli.deflate(data, mode: :generic, quality: 11, lgwin: 24, lgblock: 24)
      puts "Brotli(generic/11/24/24) size: #{compressed.bytesize}, compress ratio: #{(compressed.bytesize / data.bytesize.to_f * 100).round(3)} %"
      compressed = Brotli.deflate(data, mode: :text, quality: 11, lgwin: 24, lgblock: 24)
      puts "Brotli(text/11/24/24) size: #{compressed.bytesize}, compress ratio: #{(compressed.bytesize / data.bytesize.to_f * 100).round(3)} %"
    end
  end

  it 'inflate' do
    compressed = File.binread File.expand_path(File.join(File.dirname(__FILE__), '..', 'vendor', 'brotli', 'tests', 'testdata', 'alice29.txt.compressed'), __FILE__)
    expect(Brotli.inflate(compressed)).to eq data
  end
end
