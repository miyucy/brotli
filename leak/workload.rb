# frozen_string_literal: true
#
# Exerciser for the leak-detection harness. See leak/README.md.
#
# Env:
#   LEAK_ITER=<n>   iterations / probe allocations            (default 2000)
#   LEAK_WORK=0     skip the brotli work loop (probe only; fast detector test)
#   LEAK_FAULT=1    tolerate injected NoMemoryError, count rescues, report RSS
#   LEAK_PROBE=1    plant a known raw-C leak (positive control for detectors)

$LOAD_PATH.unshift(File.expand_path("../lib", __dir__))
require "brotli"

ITER  = Integer(ENV.fetch("LEAK_ITER", "2000"))
WORK  = ENV.fetch("LEAK_WORK", "1") != "0"
FAULT = !ENV["LEAK_FAULT"].nil?
PROBE = ENV.fetch("LEAK_PROBE", "")
PROBE = nil if PROBE.empty?

def rss_kb
  `ps -o rss= -p #{$$}`.to_i
rescue StandardError
  -1
end

# Positive controls — deliberate, known defects used to prove a detector
# actually detects (a "0 leaks"/"clean" verdict is worthless otherwise):
#
#   LEAK_PROBE=1        raw-C leak: unfreed malloc, invisible to Ruby's GC, so
#                       every malloc-level leak detector must flag it.
#   LEAK_PROBE=corrupt  heap-buffer-overflow: a 64-byte memcpy into a 16-byte
#                       block, which AddressSanitizer must catch and abort on.
if PROBE
  require "fiddle"
  h = Fiddle::Handle::DEFAULT
  malloc = Fiddle::Function.new(h["malloc"], [Fiddle::TYPE_SIZE_T], Fiddle::TYPE_VOIDP)
  if PROBE == "corrupt"
    memcpy = Fiddle::Function.new(h["memcpy"],
                                  [Fiddle::TYPE_VOIDP, Fiddle::TYPE_VOIDP, Fiddle::TYPE_SIZE_T],
                                  Fiddle::TYPE_VOIDP)
    src = malloc.call(64)
    dst = malloc.call(16)
    memcpy.call(dst, src, 64) # overflow: ASan aborts here
  else
    ITER.times { malloc.call(4096) } # never freed
  end
end

# Retry helper for one-shot setup under random fault injection.
def attempt(tries = 100)
  yield
rescue NoMemoryError, Brotli::Error
  tries -= 1
  retry if tries.positive?
  raise
end

# Large, highly compressible payload. Its decompressed size is far above
# BUFSIZ (8 KiB), so inflate's expand_buffer -> ruby_xrealloc (the F1 path)
# actually fires when we exercise it.
BIG_PLAIN = ("The quick brown fox jumps over the lazy dog. " * 4000).freeze
BIG_COMP  = attempt { Brotli.deflate(BIG_PLAIN) }
SMALL     = ("x" * 200).freeze

def guard(rescued)
  yield
rescue NoMemoryError, Brotli::Error
  rescued[:n] += 1
end

def one_round(rescued, fault)
  if fault
    guard(rescued) { Brotli.inflate(Brotli.deflate(SMALL)) }   # create_buffer path
    guard(rescued) { Brotli.inflate(BIG_COMP) }                # expand_buffer / F1 path
    guard(rescued) { c = Brotli::Compressor.new;   c.process(BIG_PLAIN); c.finish }
    guard(rescued) { d = Brotli::Decompressor.new; d.process(BIG_COMP) }
  else
    Brotli.inflate(Brotli.deflate(SMALL))
    Brotli.inflate(BIG_COMP)
    c = Brotli::Compressor.new;   c.process(BIG_PLAIN); c.finish
    d = Brotli::Decompressor.new; d.process(BIG_COMP)
  end
end

# Run inside a method so transient objects leave the machine stack before the
# final GC -> cleaner macOS `leaks` signal (fewer conservative-root artifacts).
def run(iter, fault, rescued)
  iter.times { one_round(rescued, fault) }
end

rescued = { n: 0 }
before = rss_kb
run(ITER, FAULT, rescued) if WORK
GC.start
GC.start
after = rss_kb

puts "ITER=#{ITER} WORK=#{WORK} PROBE=#{PROBE} " \
     "RSS_BEFORE=#{before}KB RSS_AFTER=#{after}KB RSS_DELTA=#{after - before}KB"
puts "RESCUED=#{rescued[:n]}" if FAULT
