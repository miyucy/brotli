#include "brotli.h"
#include "ruby/thread.h"

#define CSTR2SYM(x) ID2SYM(rb_intern(x))

static VALUE rb_mBrotli;
static VALUE rb_eBrotli;

static void*
brotli_alloc(void* opaque, size_t size)
{
    return xmalloc(size);
}

static void
brotli_free(void* opaque, void* address)
{
    if (address) {
        xfree(address);
    }
}

static VALUE
brotli_inflate(VALUE self, VALUE str)
{
    StringValue(str);

    uint8_t         output[BUFSIZ];
    BrotliResult    result = BROTLI_RESULT_ERROR;
    size_t    available_in = (size_t)RSTRING_LEN(str);
    const uint8_t* next_in = (uint8_t*)RSTRING_PTR(str);
    size_t   available_out = BUFSIZ;
    uint8_t*      next_out = output;
    size_t       total_out = 0;
    VALUE            value = rb_str_buf_new(available_in);

    BrotliState* s = BrotliCreateState(brotli_alloc, brotli_free, NULL);

    for (;;) {
        result = BrotliDecompressStream(&available_in,
                                        &next_in,
                                        &available_out,
                                        &next_out,
                                        &total_out,
                                        s);
        // success, error or needs more input
        if (result != BROTLI_RESULT_NEEDS_MORE_OUTPUT) {
            break;
        }
        rb_str_buf_cat(value, (const char*)output, BUFSIZ);
        available_out = BUFSIZ;
        next_out = output;
    }

    if (next_out != output) {
        rb_str_buf_cat(value, (const char*)output, next_out - output);
    }

    if (result == BROTLI_RESULT_ERROR) {
        const char* error = BrotliErrorString(BrotliGetErrorCode(s));
        BrotliDestroyState(s);
        rb_raise(rb_eBrotli, "%s", error);
    }

    BrotliDestroyState(s);

    if (result == BROTLI_RESULT_NEEDS_MORE_INPUT) {
        rb_raise(rb_eBrotli, "Needs more input");
    } else if (result == BROTLI_RESULT_NEEDS_MORE_OUTPUT) {
        rb_raise(rb_eBrotli, "Needs more output");
    }

    return value;
}

static void
brotli_deflate_parse_options(brotli::BrotliParams& params, VALUE opts)
{
    VALUE tmp;

    tmp = rb_hash_aref(opts, CSTR2SYM("mode"));
    if (!NIL_P(tmp)) {
        if (tmp == CSTR2SYM("generic")) {
            params.mode = brotli::BrotliParams::MODE_GENERIC;
        } else if (tmp == CSTR2SYM("text")) {
            params.mode = brotli::BrotliParams::MODE_TEXT;
        } else if (tmp == CSTR2SYM("font")) {
            params.mode = brotli::BrotliParams::MODE_FONT;
        } else {
            rb_raise(rb_eArgError, "invalid mode");
        }
    }

    tmp = rb_hash_aref(opts, CSTR2SYM("quality"));
    if (!NIL_P(tmp)) {
        int value = NUM2INT(tmp);
        if (0 <= value && value <= 11) {
            params.quality = value;
        } else {
            rb_raise(rb_eArgError, "invalid quality value. Should be 0 to 11.");
        }
    }

    tmp = rb_hash_aref(opts, CSTR2SYM("lgwin"));
    if (!NIL_P(tmp)) {
        int value = NUM2INT(tmp);
        if (10 <= value && value <= 24) {
            params.lgwin = value;
        } else {
            rb_raise(rb_eArgError, "invalid lgwin value. Should be 10 to 24.");
        }
    }

    tmp = rb_hash_aref(opts, CSTR2SYM("lgblock"));
    if (!NIL_P(tmp)) {
        int value = NUM2INT(tmp);
        if ((value == 0) || (16 <= value && value <= 24)) {
            params.lgblock = value;
        } else {
            rb_raise(rb_eArgError, "invalid lgblock value. Should be 0 or 16 to 24.");
        }
    }
}

struct brotli_deflate_t
{
    char *str;
    size_t str_length;
    std::string buf;
    brotli::BrotliParams *params;

} brotli_deflate_t;

static void *
brotli_deflate_no_gvl(void *arg)
{
    struct brotli_deflate_t *args = (struct brotli_deflate_t *)arg;

    brotli::BrotliMemIn in(args->str, args->str_length);
    args->buf.reserve(args->str_length * 2 + 1);
    brotli::BrotliStringOut out(&args->buf, args->buf.capacity());

    return (void *)brotli::BrotliCompress(*(args->params), &in, &out);
}

static VALUE
brotli_deflate(int argc, VALUE *argv, VALUE self)
{
    VALUE str, opts;
    rb_scan_args(argc, argv, "11", &str, &opts);

    StringValue(str);

    brotli::BrotliParams params;
    if (!NIL_P(opts)) {
        brotli_deflate_parse_options(params, opts);
    }

    struct brotli_deflate_t args = {
        .str = RSTRING_PTR(str),
        .str_length = RSTRING_LEN(str),
        .params = &params
    };
    if (!rb_thread_call_without_gvl(brotli_deflate_no_gvl, (void *)&args, NULL, NULL)) {
        rb_raise(rb_eBrotli, "ERROR");
    }


    return rb_str_new(args.buf.c_str(), args.buf.size());
}

extern "C" {
void
Init_brotli(void)
{
    rb_mBrotli = rb_define_module("Brotli");
    rb_eBrotli = rb_define_class_under(rb_mBrotli, "Error", rb_eStandardError);
    rb_define_singleton_method(rb_mBrotli, "deflate", RUBY_METHOD_FUNC(brotli_deflate), -1);
    rb_define_singleton_method(rb_mBrotli, "inflate", RUBY_METHOD_FUNC(brotli_inflate), 1);
}
}
