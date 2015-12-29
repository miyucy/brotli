require 'spec_helper'
require 'zlib'

describe Brotli do
  it 'has a version number' do
    expect(Brotli::VERSION).not_to be nil
  end

  let(:data) { File.binread __FILE__ }

  it 'deflate/inflate' do
    compressed = Brotli.deflate(data)
    decompressed = Brotli.inflate(compressed)
    expect(decompressed).to eq data
  end

  it 'benchmark' do
    compressed = Zlib.deflate(data, Zlib::BEST_COMPRESSION)
    puts "Zlib compress ratio: #{(compressed.bytesize / data.bytesize.to_f * 100).round(3)} %"

    compressed = Brotli.deflate(data)
    puts "Brotli compress ratio: #{(compressed.bytesize / data.bytesize.to_f * 100).round(3)} %"
  end

  it 'inflate' do
    file = File.binread File.join(__dir__, '..', 'vendor', 'brotli', 'tests', 'testdata', 'alice29.txt')
    compressed = File.binread File.join(__dir__, '..', 'vendor', 'brotli', 'tests', 'testdata', 'alice29.txt.compressed')
    expect(Brotli.inflate compressed).to eq file
  end
end
