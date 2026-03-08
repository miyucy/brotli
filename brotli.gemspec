# frozen_string_literal: true

require_relative "lib/brotli/version"

Gem::Specification.new do |spec|
  spec.name          = "brotli"
  spec.version       = Brotli::VERSION
  spec.authors       = ["miyucy"]
  spec.email         = ["fistfvck@gmail.com"]

  spec.summary       = "Ruby bindings for Brotli compression"
  spec.description   = "Native Ruby bindings for Brotli compression and decompression."
  spec.homepage      = "https://github.com/miyucy/brotli"
  spec.license       = "MIT"
  spec.required_ruby_version = ">= 3.1"

  spec.files = Dir.chdir(__dir__) do
    (
      Dir["lib/**/*.rb", "vendor/brotli/c/{common,dec,enc,include}/**/*.{c,h}"] +
      %w[
        LICENSE.txt
        README.md
        ext/brotli/brotli.c
        ext/brotli/brotli.h
        ext/brotli/buffer.c
        ext/brotli/buffer.h
        ext/brotli/extconf.rb
        vendor/brotli/LICENSE
      ]
    ).sort
  end
  spec.require_paths = ["lib"]
  spec.extensions    = ["ext/brotli/extconf.rb"]
end
