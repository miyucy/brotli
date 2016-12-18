require 'spec_helper'
require 'benchmark'
require 'thread'

describe Brotli do
  context 'deflate' do
    let(:sample) { 10 }
    let(:datum) { File.binread File.join(__dir__, '..', 'vendor', 'brotli', 'tests', 'testdata', 'lcet10.txt') }
    let!(:data) { sample.times.map { datum.dup } }

    it 'seq' do
      t = Benchmark.realtime do
        data.each { |datum| Brotli.deflate datum }
      end
      puts t
      # 7.183561000041664
    end

    it '5 threads' do
      q = Queue.new
      data.each { |datum| q.push datum }
      q.close

      w = 5.times.map do
        Thread.new do
          while data = q.pop
            Brotli.deflate data
          end
        end
      end

      t = Benchmark.realtime do
        w.each(&:join)
      end
      puts t
      # 1.7900010000448674
    end
  end

  context 'inflate' do
    let(:sample) { 1000 }
    let(:datum) { File.binread File.join(__dir__, '..', 'vendor', 'brotli', 'tests', 'testdata', 'lcet10.txt.compressed') }
    let!(:data) { sample.times.map { datum.dup } }

    it 'seq' do
      t = Benchmark.realtime do
        data.each { |datum| Brotli.inflate datum }
      end
      puts t
      # w/ gvl 1.6123949999455363
      # wo/ gvl 1.5788109998684376
    end

    it '5 threads' do
      q = Queue.new
      data.each { |datum| q.push datum }
      q.close

      w = 5.times.map do
        Thread.new do
          while data = q.pop
            Brotli.inflate data
          end
        end
      end

      t = Benchmark.realtime do
        w.each(&:join)
      end
      puts t
      # w/ gvl 1.0620850000996143
      # wo/ gvl 0.40698900003917515
    end
  end
end
