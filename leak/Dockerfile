# Leak-detection toolchain image. Runs natively as linux/arm64 under Docker
# Desktop on Apple Silicon (no qemu). See leak/README.md.
#
# Ruby >= 3.4 is required so RUBY_FREE_AT_EXIT=1 frees the interpreter's own
# memory at exit, which is what makes Valgrind/ASan output readable.
FROM ruby:3.4-bookworm

RUN apt-get update && apt-get install -y --no-install-recommends \
        valgrind \
        libfiu-dev fiu-utils \
        build-essential cmake git pkg-config procps \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# Drop any host (darwin) bundle/build artifacts that slipped through so the
# Linux image installs/compiles cleanly.
RUN rm -rf vendor/bundle .bundle tmp pkg && gem install bundler && bundle install

# Build the vendored Brotli + extension with debug symbols so brotli.c /
# buffer.c / vendored frames show up in Valgrind, massif and leaks output.
RUN BROTLI_DEBUG=1 bundle exec rake compile -- --enable-vendor

CMD ["bash"]
