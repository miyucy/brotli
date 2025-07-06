#include "brotli.h"

#define CSTR2SYM(x) ID2SYM(rb_intern(x))

static VALUE rb_eBrotli;

static inline void*
brotli_alloc(void* opaque, size_t size)
{
    return ruby_xmalloc(size);
}

static inline void
brotli_free(void* opaque, void* address)
{
    if (address) {
        ruby_xfree(address);
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
    uint8_t* dict;
    size_t dict_len;
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

#ifdef HAVE_BROTLIDECODERATTACHDICTIONARY
    /* Attach dictionary if provided */
    if (args->dict && args->dict_len > 0) {
        BrotliDecoderAttachDictionary(s, BROTLI_SHARED_DICTIONARY_RAW,
                                      args->dict_len, args->dict);
    }
#endif

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

static ID id_read;

static VALUE
brotli_inflate(int argc, VALUE *argv, VALUE self)
{
    VALUE str = Qnil, opts = Qnil, value = Qnil, dict = Qnil;
    brotli_inflate_args_t args;

    rb_scan_args(argc, argv, "11", &str, &opts);

    if (rb_respond_to(str, id_read)) {
      str = rb_funcall(str, id_read, 0, 0);
    }

    StringValue(str);

    /* Extract dictionary from options if provided */
    if (!NIL_P(opts)) {
        Check_Type(opts, T_HASH);
        dict = rb_hash_aref(opts, CSTR2SYM("dictionary"));
    }

    args.str = (uint8_t*)RSTRING_PTR(str);
    args.len = (size_t)RSTRING_LEN(str);
    args.buffer = create_buffer(BUFSIZ);
    args.s = BrotliDecoderCreateInstance(brotli_alloc,
                                         brotli_free,
                                         NULL);
    args.r = BROTLI_DECODER_RESULT_ERROR;

    /* Set dictionary parameters */
    if (!NIL_P(dict)) {
#ifdef HAVE_BROTLIDECODERATTACHDICTIONARY
        StringValue(dict);
        args.dict = (uint8_t*)RSTRING_PTR(dict);
        args.dict_len = (size_t)RSTRING_LEN(dict);
#else
        rb_raise(rb_eBrotli, "Dictionary support not available in this build");
#endif
    } else {
        args.dict = NULL;
        args.dict_len = 0;
    }

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
        Check_Type(opts, T_HASH);
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
    uint8_t *dict;
    size_t dict_len;
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

#if defined(HAVE_BROTLIENCODERPREPAREDICTIONARY) && defined(HAVE_BROTLIENCODERATTACHPREPAREDDICTIONARY)
    /* Attach dictionary if provided */
    if (args->dict && args->dict_len > 0) {
        BrotliEncoderPreparedDictionary* dict = BrotliEncoderPrepareDictionary(
            BROTLI_SHARED_DICTIONARY_RAW, args->dict_len, args->dict,
            BROTLI_MAX_QUALITY, brotli_alloc, brotli_free, NULL);
        if (dict) {
            BrotliEncoderAttachPreparedDictionary(s, dict);
            /* Note: dict is owned by encoder after attach, no need to free */
        }
    }
#endif

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

static VALUE
brotli_deflate(int argc, VALUE *argv, VALUE self)
{
    VALUE str = Qnil, opts = Qnil, value = Qnil, dict = Qnil;
    brotli_deflate_args_t args;

    rb_scan_args(argc, argv, "11", &str, &opts);
    if (NIL_P(str)) {
        rb_raise(rb_eArgError, "input should not be nil");
        return Qnil;
    }
    StringValue(str);

    /* Extract dictionary from options if provided */
    if (!NIL_P(opts)) {
        Check_Type(opts, T_HASH);
        dict = rb_hash_aref(opts, CSTR2SYM("dictionary"));
    }

    args.str = (uint8_t*)RSTRING_PTR(str);
    args.len = (size_t)RSTRING_LEN(str);
    args.s = brotli_deflate_parse_options(
        BrotliEncoderCreateInstance(brotli_alloc, brotli_free, NULL),
        opts);
    size_t max_compressed_size = BrotliEncoderMaxCompressedSize(args.len);
    args.buffer = create_buffer(max_compressed_size);
    args.finished = BROTLI_FALSE;

    /* Set dictionary parameters */
    if (!NIL_P(dict)) {
#if defined(HAVE_BROTLIENCODERPREPAREDICTIONARY) && defined(HAVE_BROTLIENCODERATTACHPREPAREDDICTIONARY)
        StringValue(dict);
        args.dict = (uint8_t*)RSTRING_PTR(dict);
        args.dict_len = (size_t)RSTRING_LEN(dict);
#else
        rb_raise(rb_eBrotli, "Dictionary support not available in this build");
#endif
    } else {
        args.dict = NULL;
        args.dict_len = 0;
    }

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
 * version
 ******************************************************************************/

static VALUE brotli_version(VALUE klass) {
    uint32_t ver = BrotliEncoderVersion();
    char version[255];
    snprintf(version, sizeof(version), "%u.%u.%u", ver >> 24, (ver >> 12) & 0xFFF, ver & 0xFFF);
    return rb_str_new2(version);
}

/*******************************************************************************
 * Writer
 ******************************************************************************/

static ID id_write, id_flush, id_close;

struct brotli {
    VALUE io;
    BrotliEncoderState* state;
};

static void br_mark(void *p)
{
    struct brotli *br = p;
    rb_gc_mark(br->io);
}

static void br_free(void *p)
{
    struct brotli* br = p;
    BrotliEncoderDestroyInstance(br->state);
    br->state = NULL;
    br->io = Qnil;
    ruby_xfree(br);
}

static size_t br_memsize(const void *p)
{
    return sizeof(struct brotli);
}

static const rb_data_type_t brotli_data_type = {
    "brotli",
    { br_mark, br_free, br_memsize },
    0, 0, RUBY_TYPED_FREE_IMMEDIATELY
};

typedef struct {
    BrotliEncoderState* state;
    BrotliEncoderOperation op;
    size_t available_in;
    const uint8_t* next_in;
} brotli_encoder_args_t;

static void* compress_no_gvl(void *ptr) {
    brotli_encoder_args_t *args = ptr;
    size_t zero = 0;
    if (!BrotliEncoderCompressStream(args->state, args->op,
                                     &args->available_in, &args->next_in,
                                     &zero, NULL, NULL)) {
        rb_raise(rb_eBrotli, "BrotliEncoderCompressStream failed");
    }
    return NULL;
}

static size_t push_output(struct brotli *br) {
    size_t len = 0;
    if (BrotliEncoderHasMoreOutput(br->state)) {
        const uint8_t* out = BrotliEncoderTakeOutput(br->state, &len);
        if (len > 0) {
            rb_funcall(br->io, id_write, 1, rb_str_new((const char*)out, len));
        }
    }
    return len;
}

static VALUE rb_writer_alloc(VALUE klass) {
    struct brotli *br;
    VALUE obj = TypedData_Make_Struct(klass, struct brotli, &brotli_data_type, br);
    br->io = Qnil;
    br->state = BrotliEncoderCreateInstance(brotli_alloc, brotli_free, NULL);
    if (!br->state) {
        rb_raise(rb_eNoMemError, "BrotliEncoderCreateInstance failed");
        return Qnil;
    }
    return obj;
}

static VALUE rb_writer_initialize(int argc, VALUE* argv, VALUE self) {
    VALUE io = Qnil;
    VALUE opts = Qnil;
    VALUE dict = Qnil;
    rb_scan_args(argc, argv, "11", &io, &opts);
    if (NIL_P(io)) {
        rb_raise(rb_eArgError, "io should not be nil");
        return Qnil;
    }

    struct brotli *br;
    TypedData_Get_Struct(self, struct brotli, &brotli_data_type, br);
    brotli_deflate_parse_options(br->state, opts);
    br->io = io;

#if defined(HAVE_BROTLIENCODERPREPAREDICTIONARY) && defined(HAVE_BROTLIENCODERATTACHPREPAREDDICTIONARY)
    /* Extract and attach dictionary if provided */
    if (!NIL_P(opts)) {
        Check_Type(opts, T_HASH);
        dict = rb_hash_aref(opts, CSTR2SYM("dictionary"));
        if (!NIL_P(dict)) {
            StringValue(dict);
            BrotliEncoderPreparedDictionary* prepared_dict = BrotliEncoderPrepareDictionary(
                BROTLI_SHARED_DICTIONARY_RAW,
                (size_t)RSTRING_LEN(dict),
                (uint8_t*)RSTRING_PTR(dict),
                BROTLI_MAX_QUALITY,
                brotli_alloc,
                brotli_free,
                NULL);
            if (prepared_dict) {
                BrotliEncoderAttachPreparedDictionary(br->state, prepared_dict);
                /* Note: dict is owned by encoder after attach, no need to free */
            } else {
                rb_raise(rb_eBrotli, "Failed to prepare dictionary for compression");
            }
        }
    }
#else
    /* Check if dictionary is requested but not supported */
    if (!NIL_P(opts)) {
        Check_Type(opts, T_HASH);
        dict = rb_hash_aref(opts, CSTR2SYM("dictionary"));
        if (!NIL_P(dict)) {
            rb_raise(rb_eBrotli, "Dictionary support not available in this build");
        }
    }
#endif

    return self;
}

static VALUE rb_writer_write(VALUE self, VALUE buf) {
    struct brotli* br;
    TypedData_Get_Struct(self, struct brotli, &brotli_data_type, br);
    StringValue(buf);

    const size_t total = (size_t)RSTRING_LEN(buf);

    brotli_encoder_args_t args = {
        .state = br->state,
        .op = BROTLI_OPERATION_PROCESS,
        .available_in = total,
        .next_in = (uint8_t*)RSTRING_PTR(buf)
    };

    while (args.available_in > 0) {
        rb_thread_call_without_gvl(compress_no_gvl, (void*)&args, NULL, NULL);
        push_output(br);
    }

    return SIZET2NUM(total);
}

static VALUE rb_writer_finish(VALUE self) {
    struct brotli* br;
    TypedData_Get_Struct(self, struct brotli, &brotli_data_type, br);

    brotli_encoder_args_t args = {
        .state = br->state,
        .op = BROTLI_OPERATION_FINISH,
        .available_in = 0,
        .next_in = NULL
    };

    while (!BrotliEncoderIsFinished(br->state)) {
        rb_thread_call_without_gvl(compress_no_gvl, (void*)&args, NULL, NULL);
        push_output(br);
    }
    return br->io;
}

static VALUE rb_writer_flush(VALUE self) {
    struct brotli *br;
    TypedData_Get_Struct(self, struct brotli, &brotli_data_type, br);

    brotli_encoder_args_t args = {
        .state = br->state,
        .op = BROTLI_OPERATION_FLUSH,
        .available_in = 0,
        .next_in = NULL
    };

    do  {
        rb_thread_call_without_gvl(compress_no_gvl, (void*)&args, NULL, NULL);
        push_output(br);
    } while (BrotliEncoderHasMoreOutput(br->state));

    if (rb_respond_to(br->io, id_flush)) {
        rb_funcall(br->io, id_flush, 0);
    }
    return self;
}

static VALUE rb_writer_close(VALUE self) {
    struct brotli* br;
    TypedData_Get_Struct(self, struct brotli, &brotli_data_type, br);

    rb_writer_finish(self);

    if (rb_respond_to(br->io, id_close)) {
        rb_funcall(br->io, id_close, 0);
    }
    return br->io;
}

/*******************************************************************************
 * entry
 ******************************************************************************/

void
Init_brotli(void)
{
#if HAVE_RB_EXT_RACTOR_SAFE
    rb_ext_ractor_safe(true);
#endif

    VALUE rb_mBrotli;
    VALUE rb_Writer;
    rb_mBrotli = rb_define_module("Brotli");
    rb_eBrotli = rb_define_class_under(rb_mBrotli, "Error", rb_eStandardError);
    rb_global_variable(&rb_eBrotli);
    rb_define_singleton_method(rb_mBrotli, "deflate", brotli_deflate, -1);
    rb_define_singleton_method(rb_mBrotli, "inflate", brotli_inflate, -1);
    rb_define_singleton_method(rb_mBrotli, "version", brotli_version, 0);
    id_read = rb_intern("read");
    // Brotli::Writer
    id_write = rb_intern("write");
    id_flush = rb_intern("flush");
    id_close = rb_intern("close");
    rb_Writer = rb_define_class_under(rb_mBrotli, "Writer", rb_cObject);
    rb_define_alloc_func(rb_Writer, rb_writer_alloc);
    rb_define_method(rb_Writer, "initialize", rb_writer_initialize, -1);
    rb_define_method(rb_Writer, "write", rb_writer_write, 1);
    rb_define_method(rb_Writer, "finish", rb_writer_finish, 0);
    rb_define_method(rb_Writer, "flush", rb_writer_flush, 0);
    rb_define_method(rb_Writer, "close", rb_writer_close, 0);
}
