#include "brotli.h"

#define CSTR2SYM(x) ID2SYM(rb_intern(x))

static VALUE rb_mBrotli;
static VALUE rb_eBrotli;

static void*
brotli_alloc(void* opaque, size_t size)
{
    return malloc(size);
}

static void
brotli_free(void* opaque, void* address)
{
    if (address) {
        free(address);
    }
}

/*******************************************************************************
 * inflate
 ******************************************************************************/

typedef struct {
    uint8_t* str;
    size_t len;
    BrotliDecoderState* s;
    buffer_t* buffer;
    BrotliDecoderResult r;
} brotli_inflate_args_t;

static void*
brotli_inflate_no_gvl(void *arg)
{
    brotli_inflate_args_t *args = (brotli_inflate_args_t*)arg;
    uint8_t         output[BUFSIZ];
    BrotliDecoderResult  r = BROTLI_DECODER_RESULT_ERROR;
    size_t    available_in = args->len;
    const uint8_t* next_in = args->str;
    size_t   available_out = BUFSIZ;
    uint8_t*      next_out = output;
    size_t       total_out = 0;
    buffer_t*       buffer = args->buffer;
    BrotliDecoderState*  s = args->s;

    for (;;) {
        r = BrotliDecoderDecompressStream(s,
                                          &available_in, &next_in,
                                          &available_out, &next_out,
                                          &total_out);
        /* success, error or needs more input */
        if (r != BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
            break;
        }
        append_buffer(buffer, output, BUFSIZ);
        available_out = BUFSIZ;
        next_out = output;
    }

    if (r == BROTLI_DECODER_RESULT_SUCCESS) {
        if (next_out != output) {
            append_buffer(buffer, output, next_out - output);
        }
    }
    args->r = r;

    return arg;
}

static VALUE
brotli_inflate(VALUE self, VALUE str)
{
    VALUE value = Qnil;
    brotli_inflate_args_t args;

    StringValue(str);

    args.str = (uint8_t*)RSTRING_PTR(str);
    args.len = (size_t)RSTRING_LEN(str);
    args.buffer = create_buffer(BUFSIZ);
    args.s = BrotliDecoderCreateInstance(brotli_alloc,
                                         brotli_free,
                                         NULL);
    args.r = BROTLI_DECODER_RESULT_ERROR;

#ifdef HAVE_RUBY_THREAD_H
    rb_thread_call_without_gvl(brotli_inflate_no_gvl, (void *)&args, NULL, NULL);
#else
    brotli_inflate_no_gvl((void *)&args);
#endif
    if (args.r == BROTLI_DECODER_RESULT_SUCCESS) {
        value = rb_str_new(args.buffer->ptr, args.buffer->used);
        delete_buffer(args.buffer);
        BrotliDecoderDestroyInstance(args.s);
    } else if (args.r == BROTLI_DECODER_RESULT_ERROR) {
        const char * error = BrotliDecoderErrorString(BrotliDecoderGetErrorCode(args.s));
        delete_buffer(args.buffer);
        BrotliDecoderDestroyInstance(args.s);
        rb_raise(rb_eBrotli, "%s", error);
    } else if (args.r == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) {
        delete_buffer(args.buffer);
        BrotliDecoderDestroyInstance(args.s);
        rb_raise(rb_eBrotli, "Needs more input");
    } else if (args.r == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
        /* never reach to this block */
        delete_buffer(args.buffer);
        BrotliDecoderDestroyInstance(args.s);
        rb_raise(rb_eBrotli, "Needs more output");
    }

    return value;
}

/*******************************************************************************
 * deflate
 ******************************************************************************/

static void
brotli_deflate_set_mode(BrotliEncoderState* s, VALUE value)
{
    if (NIL_P(value)) {
        return;
    } else {
        if (value == CSTR2SYM("generic")) {
            BrotliEncoderSetParameter(s, BROTLI_PARAM_MODE, BROTLI_MODE_GENERIC);
        } else if (value == CSTR2SYM("text")) {
            BrotliEncoderSetParameter(s, BROTLI_PARAM_MODE, BROTLI_MODE_TEXT);
        } else if (value == CSTR2SYM("font")) {
            BrotliEncoderSetParameter(s, BROTLI_PARAM_MODE, BROTLI_MODE_FONT);
        } else {
            rb_raise(rb_eArgError, "invalid mode");
        }
    }
}

static void
brotli_deflate_set_quality(BrotliEncoderState* s, VALUE value)
{
    if (NIL_P(value)) {
        return;
    } else {
        int32_t param = NUM2INT(value);
        if (0 <= param && param <= 11) {
            BrotliEncoderSetParameter(s, BROTLI_PARAM_QUALITY, param);
        } else {
            rb_raise(rb_eArgError, "invalid quality value. Should be 0 to 11.");
        }
    }
}

static void
brotli_deflate_set_lgwin(BrotliEncoderState* s, VALUE value)
{
    if (NIL_P(value)) {
        return;
    } else {
        int32_t param = NUM2INT(value);
        if (10 <= param && param <= 24) {
            BrotliEncoderSetParameter(s, BROTLI_PARAM_LGWIN, param);
        } else {
            rb_raise(rb_eArgError, "invalid lgwin value. Should be 10 to 24.");
        }
    }
}

static void
brotli_deflate_set_lgblock(BrotliEncoderState* s, VALUE value)
{
    if (NIL_P(value)) {
        return;
    } else {
        int32_t param = NUM2INT(value);
        if ((param == 0) || (16 <= param && param <= 24)) {
            BrotliEncoderSetParameter(s, BROTLI_PARAM_LGBLOCK, param);
        } else {
            rb_raise(rb_eArgError, "invalid lgblock value. Should be 0 or 16 to 24.");
        }
    }
}

static BrotliEncoderState*
brotli_deflate_parse_options(BrotliEncoderState* s, VALUE opts)
{
    if (!NIL_P(opts)) {
        brotli_deflate_set_mode(s, rb_hash_aref(opts, CSTR2SYM("mode")));
        brotli_deflate_set_quality(s, rb_hash_aref(opts, CSTR2SYM("quality")));
        brotli_deflate_set_lgwin(s, rb_hash_aref(opts, CSTR2SYM("lgwin")));
        brotli_deflate_set_lgblock(s, rb_hash_aref(opts, CSTR2SYM("lgblock")));
    }

    return s;
}

typedef struct {
    uint8_t *str;
    size_t len;
    BrotliEncoderState* s;
    buffer_t* buffer;
    BROTLI_BOOL finished;
} brotli_deflate_args_t;

static void*
brotli_deflate_no_gvl(void *arg)
{
    brotli_deflate_args_t *args = (brotli_deflate_args_t *)arg;
    uint8_t         output[BUFSIZ];
    BROTLI_BOOL          r = BROTLI_FALSE;
    size_t    available_in = args->len;
    const uint8_t* next_in = args->str;
    size_t   available_out = BUFSIZ;
    uint8_t*      next_out = output;
    size_t       total_out = 0;
    buffer_t*       buffer = args->buffer;
    BrotliEncoderState*  s = args->s;

    for (;;) {
        r = BrotliEncoderCompressStream(s,
                                        BROTLI_OPERATION_FINISH,
                                        &available_in, &next_in,
                                        &available_out, &next_out, &total_out);
        if (r == BROTLI_FALSE) {
            args->finished = BROTLI_FALSE;
            break;
        } else {
            append_buffer(buffer, output, next_out - output);
            available_out = BUFSIZ;
            next_out = output;

            if (BrotliEncoderIsFinished(args->s)) {
                args->finished = BROTLI_TRUE;
                break;
            }
        }
    }

    return arg;
}

/*
 * call-seq:
 *     Brotli.deflate(str, opts=nil) -> String
 * @param [String] str
 *   string
 * @param [Hash] opts
 *   options
 * @option opts [Symbol] :mode
 *   Deflate mode
 *   * :generic
 *   * :text
 *   * :font
 * @option opts [Integer] :quality
 *   quality 0-11
 * @option opts [Integer] :lgwin
 *   lgwin 10-24
 * @option opts [Integer] :lgblock
 *   lgblock 16-24 or 0
 * @return [String] Deflated string
 */
static VALUE
brotli_deflate(int argc, VALUE *argv, VALUE self)
{
    VALUE str = Qnil, opts = Qnil, value = Qnil;
    brotli_deflate_args_t args;

    rb_scan_args(argc, argv, "11", &str, &opts);
    StringValue(str);

    args.str = (uint8_t*)RSTRING_PTR(str);
    args.len = (size_t)RSTRING_LEN(str);
    args.s = brotli_deflate_parse_options(
        BrotliEncoderCreateInstance(brotli_alloc, brotli_free, NULL),
        opts);
    args.buffer = create_buffer(BUFSIZ);
    args.finished = BROTLI_FALSE;

#ifdef HAVE_RUBY_THREAD_H
    rb_thread_call_without_gvl(brotli_deflate_no_gvl, (void *)&args, NULL, NULL);
#else
    brotli_deflate_no_gvl((void *)&args);
#endif
    if (args.finished == BROTLI_TRUE) {
        value = rb_str_new(args.buffer->ptr, args.buffer->used);
    }

    delete_buffer(args.buffer);
    BrotliEncoderDestroyInstance(args.s);

    return value;
}

/*******************************************************************************
 * entry
 ******************************************************************************/

void
Init_brotli(void)
{
    rb_mBrotli = rb_define_module("Brotli");
    rb_eBrotli = rb_define_class_under(rb_mBrotli, "Error", rb_eStandardError);
    rb_define_singleton_method(rb_mBrotli, "deflate", RUBY_METHOD_FUNC(brotli_deflate), -1);
    rb_define_singleton_method(rb_mBrotli, "inflate", RUBY_METHOD_FUNC(brotli_inflate), 1);
}
