require 'spec_helper'
require 'tempfile'

RSpec.describe Brotli::Writer do
  let!(:data) { File.binread File.expand_path(File.join(File.dirname(__FILE__), '..', 'vendor', 'brotli', 'tests', 'testdata', 'alice29.txt'), __FILE__) }
  let(:tempfile) { Tempfile.new }
  let(:writer) { Brotli::Writer.new tempfile }

  before do
    allow(tempfile).to receive(:write).and_call_original
    allow(tempfile).to receive(:flush).and_call_original
    allow(tempfile).to receive(:close).and_call_original
  end

  after { tempfile.close }

  describe "#write" do
    it "invokes write method" do
      writer.write data
      writer.flush
      expect(tempfile).to have_received(:write)
    end

    it "returns wrote byte length" do
      expect(writer.write(data[0, 100])).to eq 100
      expect(writer.write(data[100, 128])).to eq 128
    end
  end

  describe "#close" do
    before do
      writer.write data
    end

    it "invokes flush and close method" do
      writer.close
      expect(tempfile).to have_received(:close)
    end

    it "returns io object" do
      expect(writer.close).to eq tempfile
    end
  end
end
