source "https://rubygems.org"

# Specify your gem's dependencies in brotli.gemspec
gemspec

gem "rake", "~> 13.0"
gem "rake-compiler"
gem "test-unit", "~> 3.0"
gem "test-unit-rr"
gem "rantly"

group :development do
  # Valgrind wrapper for the leak-detection harness (see leak/). Only needed
  # inside the leak Docker image; the Rakefile guard-requires it.
  gem "ruby_memcheck"
  # Used by leak/workload.rb to plant the positive-control raw-C leak.
  # Bundled (not default) gem since Ruby 4.0, so it must be listed for bundler.
  gem "fiddle"
end
