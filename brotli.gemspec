# coding: utf-8
lib = File.expand_path('../lib', __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require "brotli/version"

Gem::Specification.new do |spec|
  spec.name          = "brotli"
  spec.version       = Brotli::VERSION
  spec.authors       = ["miyucy"]
  spec.email         = ["fistfvck@gmail.com"]

  spec.summary       = "Brotli compressor/decompressor"
  spec.description   = "Brotli compressor/decompressor"
  spec.homepage      = "https://github.com/miyucy/brotli"
  spec.license       = "MIT"

  spec.test_files    = `git ls-files -z -- test`.split("\x0")
  spec.files         = `git ls-files -z`.split("\x0")
  spec.files        -= spec.test_files
  spec.files        -= ["vendor/brotli"]
  spec.files        += Dir["vendor/brotli/c/{common,enc,dec,include}/**/*"]
  spec.files        += ["vendor/brotli/LICENSE"]
  spec.bindir        = "exe"
  spec.executables   = spec.files.grep(%r{^exe/}) { |f| File.basename(f) }
  spec.require_paths = ["lib"]
  spec.extensions    = ["ext/brotli/extconf.rb"]
end
