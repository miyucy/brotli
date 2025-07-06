#ifndef BROTLI_H
#define BROTLI_H 1

#include "ruby.h"

// ruby/thread.h is ruby 2.x
#ifdef HAVE_RUBY_THREAD_H
#include "ruby/thread.h"
#endif

#include "brotli/encode.h"
#include "brotli/decode.h"
#include "brotli/shared_dictionary.h"
#include "buffer.h"

#endif /* BROTLI_H */
