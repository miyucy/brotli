#include "brotli.h"

static VALUE rb_mBrotli;
static VALUE rb_eBrotli;

int
brotli_inflate_cb(void* data, const uint8_t* buf, size_t count)
{
    VALUE dst = (VALUE)data;
    rb_str_cat(dst, (const char*)buf, count);
    return count;
}

static VALUE
brotli_inflate(VALUE self, VALUE str)
{
    StringValue(str);

    VALUE dst = rb_str_buf_new(RSTRING_LEN(str));
    BrotliMemInput memin;
    BrotliInput in = BrotliInitMemInput((uint8_t*)RSTRING_PTR(str), (size_t)RSTRING_LEN(str), &memin);
    BrotliOutput out;
    out.cb_ = &brotli_inflate_cb;
    out.data_ = (void*)dst;

    switch(BrotliDecompress(in, out)) {
    case BROTLI_RESULT_SUCCESS:
        return dst;
    case BROTLI_RESULT_ERROR:
        rb_raise(rb_eBrotli, "ERROR");
        break;
    case BROTLI_RESULT_NEEDS_MORE_INPUT:
        rb_raise(rb_eBrotli, "Needs more input");
        break;
    case BROTLI_RESULT_NEEDS_MORE_OUTPUT:
        rb_raise(rb_eBrotli, "Needs more output");
        break;
    }
}

static VALUE
brotli_deflate(VALUE self, VALUE str)
{
    StringValue(str);

    brotli::BrotliParams params;
    brotli::BrotliMemIn in(RSTRING_PTR(str), RSTRING_LEN(str));
    std::string buf;
    buf.reserve(RSTRING_LEN(str) * 2 + 1);
    brotli::BrotliStringOut out(&buf, buf.capacity());
    if (!brotli::BrotliCompress(params, &in, &out)) {
        rb_raise(rb_eBrotli, "ERROR");
    }

    return rb_str_new(buf.c_str(), buf.size());
}

extern "C" {
void
Init_brotli(void)
{
    rb_mBrotli = rb_define_module("Brotli");
    rb_eBrotli = rb_define_class_under(rb_mBrotli, "Error", rb_eStandardError);
    rb_define_singleton_method(rb_mBrotli, "deflate", RUBY_METHOD_FUNC(brotli_deflate), 1);
    rb_define_singleton_method(rb_mBrotli, "inflate", RUBY_METHOD_FUNC(brotli_inflate), 1);
}
}
