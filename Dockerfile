ARG GCC_VERSION=14
FROM gcc:${GCC_VERSION}

ARG USE_SYSTEM_BROTLI=true

RUN apt-get update && apt-get install -y \
    cmake \
    git \
    pkg-config \
    ruby \
    ruby-dev \
    && rm -rf /var/lib/apt/lists/*

RUN gcc --version

WORKDIR /app
COPY . .

# Build and install Brotli 1.2.0 as a system library from vendored source
RUN if [ "${USE_SYSTEM_BROTLI}" = "true" ]; then \
        cmake -S vendor/brotli -B vendor/brotli/build \
              -DCMAKE_INSTALL_PREFIX=/usr \
              -DBUILD_SHARED_LIBS=ON && \
        cmake --build vendor/brotli/build && \
        cmake --install vendor/brotli/build && \
        ldconfig; \
    fi

RUN gem install bundler
RUN bundle install

RUN if [ "${USE_SYSTEM_BROTLI}" = "true" ]; then \
        echo "Building with system Brotli" && \
        bundle exec rake compile; \
    else \
        echo "Building with vendor Brotli" && \
        bundle exec rake compile -- --enable-vendor; \
    fi
RUN bundle exec rake test

CMD ["bash"]
