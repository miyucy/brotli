# coding: utf-8
lib = File.expand_path('../lib', __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require 'brotli/version'

Gem::Specification.new do |spec|
  spec.name          = "brotli"
  spec.version       = Brotli::VERSION
  spec.authors       = ["miyucy"]
  spec.email         = ["fistfvck@gmail.com"]

  spec.summary       = %q{Brotli compressor/decompressor}
  spec.description   = %q{Brotli compressor/decompressor}
  spec.homepage      = "https://github.com/miyucy/brotli"

  spec.files         = `git ls-files -z`.split("\x0").reject { |f| f.match(%r{^(test|spec|features)/}) }
  spec.files        -= ['vendor/brotli']
  spec.files        += Dir['vendor/brotli/{enc,dec}/*'] + Dir['vendor/brotli/LICENSE']
  spec.bindir        = "exe"
  spec.executables   = spec.files.grep(%r{^exe/}) { |f| File.basename(f) }
  spec.require_paths = ["lib"]
  spec.extensions    = ["ext/brotli/extconf.rb"]

  spec.add_development_dependency "bundler", "~> 1.10"
  spec.add_development_dependency "rake", "~> 10.0"
  spec.add_development_dependency "rake-compiler"
  spec.add_development_dependency "rspec"
  spec.add_development_dependency "rantly"
end
