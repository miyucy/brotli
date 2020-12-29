# Brotli

Brotli is a Ruby implementation of the Brotli generic-purpose lossless
compression algorithm that compresses data using a combination of a modern
variant of the LZ77 algorithm, Huffman coding and 2nd order context modeling,
with a compression ratio comparable to the best currently available
general-purpose compression methods. It is similar in speed with deflate but
offers more dense compression.

## Installation

Add this line to your application's Gemfile:

```ruby
gem 'brotli'
```

And then execute:

    $ bundle

Or install it yourself as:

    $ gem install brotli

## Usage

```ruby
require 'brotli'
compressed = Brotli.deflate(string)
decompressed = Brotli.inflate(compressed)
```

See test/brotli_test.rb

## Development

After checking out the repo, run `bin/setup` to install bundle and Brotli C library dependencies.

Run `rake build` to build brotli extension for ruby. Then, run `rake spec` to run the tests. You can also run `bin/console` for an interactive prompt that will allow you to experiment.

To install this gem onto your local machine, run `bundle exec rake install`. To release a new version, update the version number in `version.rb`, and then run `bundle exec rake release`, which will create a git tag for the version, push git commits and tags, and push the `.gem` file to [rubygems.org](https://rubygems.org).

## Contributing

Bug reports and pull requests are welcome on GitHub at https://github.com/miyucy/brotli.
