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
    BrotliEncoderPreparedDictionary* prepared_dict;
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
    args.prepared_dict = NULL;
    if (!NIL_P(dict)) {
#if defined(HAVE_BROTLIENCODERPREPAREDICTIONARY) && defined(HAVE_BROTLIENCODERATTACHPREPAREDDICTIONARY)
        StringValue(dict);
        args.prepared_dict = BrotliEncoderPrepareDictionary(
            BROTLI_SHARED_DICTIONARY_RAW,
            (size_t)RSTRING_LEN(dict),
            (const uint8_t*)RSTRING_PTR(dict),
            BROTLI_MAX_QUALITY,
            brotli_alloc,
            brotli_free,
            NULL);
        if (!args.prepared_dict) {
            delete_buffer(args.buffer);
            BrotliEncoderDestroyInstance(args.s);
            rb_raise(rb_eBrotli, "Failed to prepare dictionary for compression");
        }
        if (!BrotliEncoderAttachPreparedDictionary(args.s, args.prepared_dict)) {
            BrotliEncoderDestroyPreparedDictionary(args.prepared_dict);
            args.prepared_dict = NULL;
            delete_buffer(args.buffer);
            BrotliEncoderDestroyInstance(args.s);
            rb_raise(rb_eBrotli, "Failed to attach dictionary for compression");
        }
#else
        delete_buffer(args.buffer);
        BrotliEncoderDestroyInstance(args.s);
        rb_raise(rb_eBrotli, "Dictionary support not available in this build");
#endif
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
#if defined(HAVE_BROTLIENCODERPREPAREDICTIONARY) && defined(HAVE_BROTLIENCODERATTACHPREPAREDDICTIONARY)
    if (args.prepared_dict) {
        BrotliEncoderDestroyPreparedDictionary(args.prepared_dict);
        args.prepared_dict = NULL;
    }
#endif

    if (args.finished != BROTLI_TRUE) {
        rb_raise(rb_eBrotli, "Failed to compress");
    }

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
 * Streaming APIs
 ******************************************************************************/

static ID id_write, id_flush, id_close, id_process, id_finish, id_is_finished, id_can_accept_more_data;
static VALUE rb_cBrotliCompressor;
static VALUE rb_cBrotliDecompressor;

typedef struct {
    BrotliEncoderState* state;
    BrotliEncoderPreparedDictionary* prepared_dict;
    VALUE dict;
    BROTLI_BOOL finished;
} brotli_encoder_t;

typedef struct {
    VALUE io;
    VALUE compressor;
    BROTLI_BOOL closed;
} brotli_writer_t;

typedef struct {
    VALUE io;
    VALUE decompressor;
    VALUE output_buffer;
    BROTLI_BOOL closed;
    BROTLI_BOOL finished;
} brotli_reader_t;

typedef struct {
    brotli_encoder_t encoder;
} brotli_compressor_t;

typedef struct {
    BrotliDecoderState* state;
    VALUE dict;
    VALUE pending_input;
    BROTLI_BOOL finished;
} brotli_decompressor_t;

typedef struct {
    BrotliEncoderState* state;
    BrotliEncoderOperation op;
    size_t available_in;
    const uint8_t* next_in;
    BROTLI_BOOL ok;
} brotli_encoder_args_t;

static void* compress_no_gvl(void *ptr) {
    brotli_encoder_args_t *args = ptr;
    size_t zero = 0;
    args->ok = BrotliEncoderCompressStream(args->state, args->op,
                                           &args->available_in, &args->next_in,
                                           &zero, NULL, NULL);
    return NULL;
}

static void
brotli_encoder_step(brotli_encoder_args_t *args)
{
#ifdef HAVE_RUBY_THREAD_H
    rb_thread_call_without_gvl(compress_no_gvl, (void*)args, NULL, NULL);
#else
    compress_no_gvl((void*)args);
#endif
}

static void
brotli_encoder_destroy(brotli_encoder_t* encoder)
{
    if (encoder->state) {
        BrotliEncoderDestroyInstance(encoder->state);
        encoder->state = NULL;
    }
#if defined(HAVE_BROTLIENCODERPREPAREDICTIONARY) && defined(HAVE_BROTLIENCODERATTACHPREPAREDDICTIONARY)
    if (encoder->prepared_dict) {
        BrotliEncoderDestroyPreparedDictionary(encoder->prepared_dict);
        encoder->prepared_dict = NULL;
    }
#endif
    encoder->dict = Qnil;
    encoder->finished = BROTLI_FALSE;
}

static void
brotli_encoder_take_output_to_string(BrotliEncoderState* state, VALUE output)
{
    while (BrotliEncoderHasMoreOutput(state)) {
        size_t len = 0;
        const uint8_t* out = BrotliEncoderTakeOutput(state, &len);
        if (len > 0) {
            rb_str_cat(output, (const char*)out, len);
        }
    }
}

static void
brotli_encoder_attach_dictionary(brotli_encoder_t* encoder, VALUE opts)
{
    VALUE dict = Qnil;

    if (NIL_P(opts)) {
        return;
    }

    Check_Type(opts, T_HASH);
    dict = rb_hash_aref(opts, CSTR2SYM("dictionary"));
    if (NIL_P(dict)) {
        return;
    }

#if defined(HAVE_BROTLIENCODERPREPAREDICTIONARY) && defined(HAVE_BROTLIENCODERATTACHPREPAREDDICTIONARY)
    StringValue(dict);
    encoder->prepared_dict = BrotliEncoderPrepareDictionary(
        BROTLI_SHARED_DICTIONARY_RAW,
        (size_t)RSTRING_LEN(dict),
        (const uint8_t*)RSTRING_PTR(dict),
        BROTLI_MAX_QUALITY,
        brotli_alloc,
        brotli_free,
        NULL);

    if (!encoder->prepared_dict) {
        rb_raise(rb_eBrotli, "Failed to prepare dictionary for compression");
    }

    if (!BrotliEncoderAttachPreparedDictionary(encoder->state, encoder->prepared_dict)) {
        BrotliEncoderDestroyPreparedDictionary(encoder->prepared_dict);
        encoder->prepared_dict = NULL;
        rb_raise(rb_eBrotli, "Failed to attach dictionary for compression");
    }

    encoder->dict = dict;
#else
    rb_raise(rb_eBrotli, "Dictionary support not available in this build");
#endif
}

static VALUE
brotli_encoder_stream_to_string(brotli_encoder_t* encoder,
                                BrotliEncoderOperation op,
                                const uint8_t* input,
                                size_t input_len)
{
    VALUE output = rb_str_new("", 0);
    brotli_encoder_args_t args = {
        .state = encoder->state,
        .op = op,
        .available_in = input_len,
        .next_in = input,
        .ok = BROTLI_FALSE
    };

    if (op == BROTLI_OPERATION_PROCESS && input_len == 0) {
        return output;
    }

    for (;;) {
        brotli_encoder_step(&args);
        if (args.ok == BROTLI_FALSE) {
            rb_raise(rb_eBrotli, "BrotliEncoderCompressStream failed");
        }

        brotli_encoder_take_output_to_string(encoder->state, output);

        if (op == BROTLI_OPERATION_PROCESS) {
            if (args.available_in == 0) {
                break;
            }
        } else if (op == BROTLI_OPERATION_FLUSH) {
            if (args.available_in == 0 && !BrotliEncoderHasMoreOutput(encoder->state)) {
                break;
            }
        } else {
            if (BrotliEncoderIsFinished(encoder->state) &&
                !BrotliEncoderHasMoreOutput(encoder->state)) {
                break;
            }
        }
    }

    return output;
}

static void
brotli_writer_mark(void *p)
{
    brotli_writer_t *br = p;
    rb_gc_mark(br->io);
    rb_gc_mark(br->compressor);
}

static void
brotli_writer_free(void *p)
{
    brotli_writer_t* br = p;
    br->io = Qnil;
    br->compressor = Qnil;
    br->closed = BROTLI_TRUE;
    ruby_xfree(br);
}

static size_t
brotli_writer_memsize(const void *p)
{
    return sizeof(brotli_writer_t);
}

static const rb_data_type_t brotli_writer_data_type = {
    "brotli_writer",
    { brotli_writer_mark, brotli_writer_free, brotli_writer_memsize },
    0, 0, RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE
rb_writer_alloc(VALUE klass)
{
    brotli_writer_t *br;
    VALUE obj = TypedData_Make_Struct(klass, brotli_writer_t, &brotli_writer_data_type, br);
    br->io = Qnil;
    br->compressor = Qnil;
    br->closed = BROTLI_FALSE;
    return obj;
}

static void
brotli_writer_ensure_open(brotli_writer_t* br)
{
    if (br->closed) {
        rb_raise(rb_eBrotli, "Writer is closed");
    }
}

static VALUE
rb_writer_initialize(int argc, VALUE* argv, VALUE self)
{
    VALUE io = Qnil;
    VALUE opts = Qnil;
    VALUE compressor = Qnil;

    rb_scan_args(argc, argv, "11", &io, &opts);
    if (NIL_P(io)) {
        rb_raise(rb_eArgError, "io should not be nil");
        return Qnil;
    }

    brotli_writer_t *br;
    TypedData_Get_Struct(self, brotli_writer_t, &brotli_writer_data_type, br);
    if (NIL_P(opts)) {
        compressor = rb_class_new_instance(0, NULL, rb_cBrotliCompressor);
    } else {
        VALUE args[1] = { opts };
        compressor = rb_class_new_instance(1, args, rb_cBrotliCompressor);
    }

    br->io = io;
    br->compressor = compressor;
    br->closed = BROTLI_FALSE;

    return self;
}

static VALUE
rb_writer_write(VALUE self, VALUE buf)
{
    brotli_writer_t* br;
    VALUE output;
    TypedData_Get_Struct(self, brotli_writer_t, &brotli_writer_data_type, br);
    brotli_writer_ensure_open(br);

    StringValue(buf);
    output = rb_funcall(br->compressor, id_process, 1, buf);
    if (RSTRING_LEN(output) > 0) {
        rb_funcall(br->io, id_write, 1, output);
    }

    return SIZET2NUM((size_t)RSTRING_LEN(buf));
}

static VALUE
rb_writer_finish(VALUE self)
{
    brotli_writer_t* br;
    VALUE output;
    TypedData_Get_Struct(self, brotli_writer_t, &brotli_writer_data_type, br);
    brotli_writer_ensure_open(br);

    output = rb_funcall(br->compressor, id_finish, 0);
    if (RSTRING_LEN(output) > 0) {
        rb_funcall(br->io, id_write, 1, output);
    }

    return br->io;
}

static VALUE
rb_writer_flush(VALUE self)
{
    brotli_writer_t *br;
    VALUE output;
    TypedData_Get_Struct(self, brotli_writer_t, &brotli_writer_data_type, br);
    brotli_writer_ensure_open(br);

    output = rb_funcall(br->compressor, id_flush, 0);
    if (RSTRING_LEN(output) > 0) {
        rb_funcall(br->io, id_write, 1, output);
    }

    if (rb_respond_to(br->io, id_flush)) {
        rb_funcall(br->io, id_flush, 0);
    }
    return self;
}

static VALUE
rb_writer_close(VALUE self)
{
    brotli_writer_t* br;
    TypedData_Get_Struct(self, brotli_writer_t, &brotli_writer_data_type, br);

    if (br->closed) {
        return br->io;
    }

    rb_writer_finish(self);

    if (rb_respond_to(br->io, id_close)) {
        rb_funcall(br->io, id_close, 0);
    }

    br->closed = BROTLI_TRUE;
    br->compressor = Qnil;
    return br->io;
}

/*******************************************************************************
 * Reader
 ******************************************************************************/

static void
brotli_reader_mark(void *p)
{
    brotli_reader_t *br = p;
    rb_gc_mark(br->io);
    rb_gc_mark(br->decompressor);
    rb_gc_mark(br->output_buffer);
}

static void
brotli_reader_free(void *p)
{
    brotli_reader_t* br = p;
    br->io = Qnil;
    br->decompressor = Qnil;
    br->output_buffer = Qnil;
    br->closed = BROTLI_TRUE;
    br->finished = BROTLI_TRUE;
    ruby_xfree(br);
}

static size_t
brotli_reader_memsize(const void *p)
{
    return sizeof(brotli_reader_t);
}

static const rb_data_type_t brotli_reader_data_type = {
    "brotli_reader",
    { brotli_reader_mark, brotli_reader_free, brotli_reader_memsize },
    0, 0, RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE
rb_reader_alloc(VALUE klass)
{
    brotli_reader_t *br;
    VALUE obj = TypedData_Make_Struct(klass, brotli_reader_t, &brotli_reader_data_type, br);
    br->io = Qnil;
    br->decompressor = Qnil;
    br->output_buffer = rb_str_new("", 0);
    br->closed = BROTLI_FALSE;
    br->finished = BROTLI_FALSE;
    return obj;
}

static void
brotli_reader_ensure_open(brotli_reader_t* br)
{
    if (br->closed) {
        rb_raise(rb_eBrotli, "Reader is closed");
    }
}

static VALUE
brotli_reader_with_outbuf(VALUE outbuf, VALUE str)
{
    if (NIL_P(outbuf)) {
        return str;
    }

    StringValue(outbuf);
    rb_str_replace(outbuf, str);
    return outbuf;
}

static VALUE
brotli_reader_take_output(brotli_reader_t* br, size_t len)
{
    size_t available = (size_t)RSTRING_LEN(br->output_buffer);
    VALUE output;

    if (len > available) {
        len = available;
    }
    if (len == 0) {
        return rb_str_new("", 0);
    }

    output = rb_str_new(RSTRING_PTR(br->output_buffer), (long)len);
    if (len == available) {
        rb_str_resize(br->output_buffer, 0);
        return output;
    }

    rb_str_modify(br->output_buffer);
    memmove(RSTRING_PTR(br->output_buffer),
            RSTRING_PTR(br->output_buffer) + len,
            available - len);
    rb_str_set_len(br->output_buffer, (long)(available - len));

    return output;
}

static void
brotli_reader_update_finished(brotli_reader_t* br)
{
    if (RTEST(rb_funcall(br->decompressor, id_is_finished, 0))) {
        br->finished = BROTLI_TRUE;
    }
}

static void
brotli_reader_feed_chunk(brotli_reader_t* br, VALUE chunk, size_t output_limit)
{
    VALUE output;
    VALUE opts = Qnil;

    StringValue(chunk);
    if (output_limit > 0) {
        opts = rb_hash_new();
        rb_hash_aset(opts, CSTR2SYM("output_buffer_limit"), SIZET2NUM(output_limit));
        output = rb_funcall(br->decompressor, id_process, 2, chunk, opts);
    } else {
        output = rb_funcall(br->decompressor, id_process, 1, chunk);
    }
    if (RSTRING_LEN(output) > 0) {
        rb_str_cat(br->output_buffer, RSTRING_PTR(output), RSTRING_LEN(output));
    }
    brotli_reader_update_finished(br);
}

static void
brotli_reader_fill_buffer(brotli_reader_t* br,
                          size_t wanted,
                          size_t output_limit,
                          BROTLI_BOOL stop_after_output)
{
    while ((size_t)RSTRING_LEN(br->output_buffer) < wanted && !br->finished) {
        VALUE chunk;
        size_t remaining_limit = 0;

        if (RTEST(rb_funcall(br->decompressor, id_can_accept_more_data, 0))) {
            chunk = rb_funcall(br->io, id_read, 1, SIZET2NUM(BUFSIZ));
            if (NIL_P(chunk)) {
                size_t buffered = (size_t)RSTRING_LEN(br->output_buffer);

                brotli_reader_feed_chunk(br, rb_str_new("", 0), remaining_limit);
                if (br->finished) {
                    break;
                }
                if ((size_t)RSTRING_LEN(br->output_buffer) > buffered ||
                    !RTEST(rb_funcall(br->decompressor, id_can_accept_more_data, 0))) {
                    if (stop_after_output && RSTRING_LEN(br->output_buffer) > 0) {
                        break;
                    }
                    continue;
                }
                rb_raise(rb_eBrotli, "Unexpected end of compressed stream");
            }

            StringValue(chunk);
            if (RSTRING_LEN(chunk) == 0) {
                size_t buffered = (size_t)RSTRING_LEN(br->output_buffer);

                brotli_reader_feed_chunk(br, rb_str_new("", 0), remaining_limit);
                if (br->finished) {
                    break;
                }
                if ((size_t)RSTRING_LEN(br->output_buffer) > buffered ||
                    !RTEST(rb_funcall(br->decompressor, id_can_accept_more_data, 0))) {
                    if (stop_after_output && RSTRING_LEN(br->output_buffer) > 0) {
                        break;
                    }
                    continue;
                }
                rb_raise(rb_eBrotli, "Unexpected end of compressed stream");
            }
        } else {
            chunk = rb_str_new("", 0);
        }

        if (output_limit > 0) {
            size_t buffered = (size_t)RSTRING_LEN(br->output_buffer);
            if (output_limit > buffered) {
                remaining_limit = output_limit - buffered;
            }
        }

        brotli_reader_feed_chunk(br, chunk, remaining_limit);
        if (stop_after_output && RSTRING_LEN(br->output_buffer) > 0) {
            break;
        }
    }
}

static VALUE
rb_reader_initialize(int argc, VALUE* argv, VALUE self)
{
    VALUE io = Qnil;
    VALUE opts = Qnil;
    VALUE decompressor = Qnil;
    brotli_reader_t *br;

    rb_scan_args(argc, argv, "11", &io, &opts);
    if (NIL_P(io)) {
        rb_raise(rb_eArgError, "io should not be nil");
        return Qnil;
    }

    TypedData_Get_Struct(self, brotli_reader_t, &brotli_reader_data_type, br);
    if (NIL_P(opts)) {
        decompressor = rb_class_new_instance(0, NULL, rb_cBrotliDecompressor);
    } else {
        VALUE args[1] = { opts };
        decompressor = rb_class_new_instance(1, args, rb_cBrotliDecompressor);
    }

    br->io = io;
    br->decompressor = decompressor;
    rb_str_resize(br->output_buffer, 0);
    br->closed = BROTLI_FALSE;
    br->finished = BROTLI_FALSE;
    return self;
}

static VALUE
rb_reader_read(int argc, VALUE* argv, VALUE self)
{
    VALUE length = Qnil;
    VALUE outbuf = Qnil;
    brotli_reader_t *br;
    VALUE output;
    long len;

    rb_scan_args(argc, argv, "02", &length, &outbuf);
    TypedData_Get_Struct(self, brotli_reader_t, &brotli_reader_data_type, br);
    brotli_reader_ensure_open(br);

    if (NIL_P(length)) {
        while (!br->finished) {
            size_t wanted = (size_t)RSTRING_LEN(br->output_buffer) + 1;
            brotli_reader_fill_buffer(br, wanted, 0, BROTLI_FALSE);
        }
        output = brotli_reader_take_output(br, (size_t)RSTRING_LEN(br->output_buffer));
        return brotli_reader_with_outbuf(outbuf, output);
    }

    len = NUM2LONG(length);
    if (len < 0) {
        rb_raise(rb_eArgError, "negative length %ld given", len);
    }
    if (len == 0) {
        return brotli_reader_with_outbuf(outbuf, rb_str_new("", 0));
    }

    brotli_reader_fill_buffer(br, (size_t)len, (size_t)len, BROTLI_FALSE);
    if (RSTRING_LEN(br->output_buffer) == 0 && br->finished) {
        return Qnil;
    }

    output = brotli_reader_take_output(br, (size_t)len);
    return brotli_reader_with_outbuf(outbuf, output);
}

static VALUE
rb_reader_readpartial(int argc, VALUE* argv, VALUE self)
{
    VALUE maxlen = Qnil;
    VALUE outbuf = Qnil;
    brotli_reader_t *br;
    VALUE output;
    long len;

    rb_scan_args(argc, argv, "11", &maxlen, &outbuf);
    TypedData_Get_Struct(self, brotli_reader_t, &brotli_reader_data_type, br);
    brotli_reader_ensure_open(br);

    len = NUM2LONG(maxlen);
    if (len <= 0) {
        rb_raise(rb_eArgError, "max length must be positive");
    }

    while (RSTRING_LEN(br->output_buffer) == 0) {
        if (br->finished) {
            rb_raise(rb_eEOFError, "end of file reached");
        }
        brotli_reader_fill_buffer(br, 1, (size_t)len, BROTLI_TRUE);
        if (RSTRING_LEN(br->output_buffer) == 0 && br->finished) {
            rb_raise(rb_eEOFError, "end of file reached");
        }
    }

    output = brotli_reader_take_output(br, (size_t)len);
    return brotli_reader_with_outbuf(outbuf, output);
}

static VALUE
rb_reader_gets(int argc, VALUE* argv, VALUE self)
{
    VALUE sep = Qnil;
    VALUE idx = Qnil;
    brotli_reader_t *br;
    long sep_len;

    rb_scan_args(argc, argv, "01", &sep);
    TypedData_Get_Struct(self, brotli_reader_t, &brotli_reader_data_type, br);
    brotli_reader_ensure_open(br);

    if (argc == 0) {
        sep = rb_rs;
    }

    if (NIL_P(sep)) {
        return rb_reader_read(0, NULL, self);
    }

    StringValue(sep);
    sep_len = RSTRING_LEN(sep);
    if (sep_len == 0) {
        rb_raise(rb_eArgError, "empty separator is not supported");
    }

    while (!br->finished) {
        idx = rb_funcall(br->output_buffer, rb_intern("index"), 1, sep);
        if (!NIL_P(idx)) {
            break;
        }

        {
            size_t wanted = (size_t)RSTRING_LEN(br->output_buffer) + 1;
            brotli_reader_fill_buffer(br, wanted, 0, BROTLI_FALSE);
        }
    }

    if (RSTRING_LEN(br->output_buffer) == 0 && br->finished) {
        return Qnil;
    }

    idx = rb_funcall(br->output_buffer, rb_intern("index"), 1, sep);
    if (!NIL_P(idx)) {
        long end = NUM2LONG(idx) + sep_len;
        return brotli_reader_take_output(br, (size_t)end);
    }
    return brotli_reader_take_output(br, (size_t)RSTRING_LEN(br->output_buffer));
}

static VALUE
rb_reader_each_line(int argc, VALUE* argv, VALUE self)
{
    VALUE line;

    if (!rb_block_given_p()) {
        return rb_enumeratorize(self, ID2SYM(rb_intern("each_line")), argc, argv);
    }

    for (;;) {
        line = rb_reader_gets(argc, argv, self);
        if (NIL_P(line)) {
            break;
        }
        rb_yield(line);
    }
    return self;
}

static VALUE
rb_reader_eof_p(VALUE self)
{
    brotli_reader_t *br;
    TypedData_Get_Struct(self, brotli_reader_t, &brotli_reader_data_type, br);
    brotli_reader_ensure_open(br);

    if (RSTRING_LEN(br->output_buffer) > 0) {
        return Qfalse;
    }
    if (br->finished) {
        return Qtrue;
    }

    brotli_reader_fill_buffer(br, 1, 1, BROTLI_TRUE);
    if (RSTRING_LEN(br->output_buffer) > 0) {
        return Qfalse;
    }
    return br->finished ? Qtrue : Qfalse;
}

static VALUE
rb_reader_close(VALUE self)
{
    brotli_reader_t *br;
    TypedData_Get_Struct(self, brotli_reader_t, &brotli_reader_data_type, br);

    if (br->closed) {
        return br->io;
    }

    if (rb_respond_to(br->io, id_close)) {
        rb_funcall(br->io, id_close, 0);
    }

    br->closed = BROTLI_TRUE;
    br->finished = BROTLI_TRUE;
    br->decompressor = Qnil;
    rb_str_resize(br->output_buffer, 0);
    return br->io;
}

static VALUE
rb_reader_closed_p(VALUE self)
{
    brotli_reader_t *br;
    TypedData_Get_Struct(self, brotli_reader_t, &brotli_reader_data_type, br);
    return br->closed ? Qtrue : Qfalse;
}

/*******************************************************************************
 * Compressor
 ******************************************************************************/

static void
brotli_compressor_mark(void *p)
{
    brotli_compressor_t *br = p;
    rb_gc_mark(br->encoder.dict);
}

static void
brotli_compressor_free(void *p)
{
    brotli_compressor_t* br = p;
    brotli_encoder_destroy(&br->encoder);
    ruby_xfree(br);
}

static size_t
brotli_compressor_memsize(const void *p)
{
    return sizeof(brotli_compressor_t);
}

static const rb_data_type_t brotli_compressor_data_type = {
    "brotli_compressor",
    { brotli_compressor_mark, brotli_compressor_free, brotli_compressor_memsize },
    0, 0, RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE
rb_compressor_alloc(VALUE klass)
{
    brotli_compressor_t *br;
    VALUE obj = TypedData_Make_Struct(klass, brotli_compressor_t, &brotli_compressor_data_type, br);
    br->encoder.state = BrotliEncoderCreateInstance(brotli_alloc, brotli_free, NULL);
    br->encoder.prepared_dict = NULL;
    br->encoder.dict = Qnil;
    br->encoder.finished = BROTLI_FALSE;
    if (!br->encoder.state) {
        rb_raise(rb_eNoMemError, "BrotliEncoderCreateInstance failed");
        return Qnil;
    }
    return obj;
}

static void
brotli_compressor_ensure_open(brotli_compressor_t* br)
{
    if (!br->encoder.state) {
        rb_raise(rb_eBrotli, "Compressor is closed");
    }
}

static VALUE
rb_compressor_initialize(int argc, VALUE* argv, VALUE self)
{
    VALUE opts = Qnil;
    brotli_compressor_t *br;

    rb_scan_args(argc, argv, "01", &opts);

    TypedData_Get_Struct(self, brotli_compressor_t, &brotli_compressor_data_type, br);
    brotli_deflate_parse_options(br->encoder.state, opts);
    brotli_encoder_attach_dictionary(&br->encoder, opts);

    return self;
}

static VALUE
rb_compressor_process(VALUE self, VALUE input)
{
    brotli_compressor_t *br;
    TypedData_Get_Struct(self, brotli_compressor_t, &brotli_compressor_data_type, br);
    brotli_compressor_ensure_open(br);
    if (br->encoder.finished) {
        rb_raise(rb_eBrotli, "Compressor is finished");
    }

    StringValue(input);
    return brotli_encoder_stream_to_string(&br->encoder,
                                           BROTLI_OPERATION_PROCESS,
                                           (const uint8_t*)RSTRING_PTR(input),
                                           (size_t)RSTRING_LEN(input));
}

static VALUE
rb_compressor_flush(VALUE self)
{
    brotli_compressor_t *br;
    TypedData_Get_Struct(self, brotli_compressor_t, &brotli_compressor_data_type, br);
    brotli_compressor_ensure_open(br);
    if (br->encoder.finished) {
        rb_raise(rb_eBrotli, "Compressor is finished");
    }

    return brotli_encoder_stream_to_string(&br->encoder, BROTLI_OPERATION_FLUSH, NULL, 0);
}

static VALUE
rb_compressor_finish(VALUE self)
{
    brotli_compressor_t *br;
    VALUE output;

    TypedData_Get_Struct(self, brotli_compressor_t, &brotli_compressor_data_type, br);
    brotli_compressor_ensure_open(br);

    if (br->encoder.finished) {
        return rb_str_new("", 0);
    }

    output = brotli_encoder_stream_to_string(&br->encoder, BROTLI_OPERATION_FINISH, NULL, 0);
    br->encoder.finished = BROTLI_TRUE;
    return output;
}

/*******************************************************************************
 * Decompressor
 ******************************************************************************/

static void
brotli_decompressor_mark(void *p)
{
    brotli_decompressor_t *br = p;
    rb_gc_mark(br->dict);
    rb_gc_mark(br->pending_input);
}

static void
brotli_decompressor_free(void *p)
{
    brotli_decompressor_t *br = p;
    if (br->state) {
        BrotliDecoderDestroyInstance(br->state);
        br->state = NULL;
    }
    br->dict = Qnil;
    br->pending_input = Qnil;
    br->finished = BROTLI_FALSE;
    ruby_xfree(br);
}

static size_t
brotli_decompressor_memsize(const void *p)
{
    return sizeof(brotli_decompressor_t);
}

static const rb_data_type_t brotli_decompressor_data_type = {
    "brotli_decompressor",
    { brotli_decompressor_mark, brotli_decompressor_free, brotli_decompressor_memsize },
    0, 0, RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE
rb_decompressor_alloc(VALUE klass)
{
    brotli_decompressor_t *br;
    VALUE obj = TypedData_Make_Struct(klass, brotli_decompressor_t, &brotli_decompressor_data_type, br);
    br->state = BrotliDecoderCreateInstance(brotli_alloc, brotli_free, NULL);
    br->dict = Qnil;
    br->pending_input = Qnil;
    br->finished = BROTLI_FALSE;
    if (!br->state) {
        rb_raise(rb_eNoMemError, "BrotliDecoderCreateInstance failed");
        return Qnil;
    }
    return obj;
}

static void
brotli_decompressor_attach_dictionary(brotli_decompressor_t* br, VALUE opts)
{
    VALUE dict = Qnil;

    if (NIL_P(opts)) {
        return;
    }

    Check_Type(opts, T_HASH);
    dict = rb_hash_aref(opts, CSTR2SYM("dictionary"));
    if (NIL_P(dict)) {
        return;
    }

#ifdef HAVE_BROTLIDECODERATTACHDICTIONARY
    StringValue(dict);
    if (!BrotliDecoderAttachDictionary(br->state,
                                       BROTLI_SHARED_DICTIONARY_RAW,
                                       (size_t)RSTRING_LEN(dict),
                                       (const uint8_t*)RSTRING_PTR(dict))) {
        rb_raise(rb_eBrotli, "Failed to attach dictionary for decompression");
    }
    br->dict = dict;
#else
    rb_raise(rb_eBrotli, "Dictionary support not available in this build");
#endif
}

static VALUE
rb_decompressor_initialize(int argc, VALUE* argv, VALUE self)
{
    VALUE opts = Qnil;
    brotli_decompressor_t *br;

    rb_scan_args(argc, argv, "01", &opts);
    TypedData_Get_Struct(self, brotli_decompressor_t, &brotli_decompressor_data_type, br);
    brotli_decompressor_attach_dictionary(br, opts);

    return self;
}

static BROTLI_BOOL
brotli_decompressor_has_pending_input(const brotli_decompressor_t *br)
{
    return !NIL_P(br->pending_input) && RSTRING_LEN(br->pending_input) > 0;
}

static void
brotli_decompressor_store_pending_input(brotli_decompressor_t* br,
                                        const uint8_t* next_in,
                                        size_t available_in)
{
    if (available_in == 0) {
        br->pending_input = Qnil;
        return;
    }

    br->pending_input = rb_str_new((const char*)next_in, (long)available_in);
}

static VALUE
rb_decompressor_process(int argc, VALUE* argv, VALUE self)
{
    brotli_decompressor_t *br;
    VALUE input = Qnil;
    VALUE opts = Qnil;
    VALUE input_source = Qnil;
    VALUE limit_value = Qnil;
    VALUE output;
    size_t available_in;
    const uint8_t* next_in;
    size_t output_buffer_limit = 0;
    BROTLI_BOOL limit_output = BROTLI_FALSE;
    BrotliDecoderResult result = BROTLI_DECODER_RESULT_ERROR;
    uint8_t outbuf[BUFSIZ];

    rb_scan_args(argc, argv, "11", &input, &opts);
    TypedData_Get_Struct(self, brotli_decompressor_t, &brotli_decompressor_data_type, br);
    if (!br->state) {
        rb_raise(rb_eBrotli, "Decompressor is closed");
    }

    if (!NIL_P(opts)) {
        Check_Type(opts, T_HASH);
        limit_value = rb_hash_aref(opts, CSTR2SYM("output_buffer_limit"));
        if (!NIL_P(limit_value)) {
            output_buffer_limit = NUM2SIZET(limit_value);
            limit_output = BROTLI_TRUE;
        }
    }

    if (br->finished) {
        StringValue(input);
        available_in = (size_t)RSTRING_LEN(input);
        if (available_in == 0) {
            return rb_str_new("", 0);
        }
        rb_raise(rb_eBrotli, "Decompressor is finished");
    }

    StringValue(input);
    if (brotli_decompressor_has_pending_input(br)) {
        if (RSTRING_LEN(input) > 0) {
            rb_raise(rb_eBrotli,
                     "Decompressor cannot accept more data until pending output is drained");
        }
        input_source = br->pending_input;
    } else {
        input_source = input;
    }

    available_in = (size_t)RSTRING_LEN(input_source);
    next_in = (const uint8_t*)RSTRING_PTR(input_source);
    br->pending_input = Qnil;
    output = rb_str_new("", 0);

    for (;;) {
        size_t chunk_size = BUFSIZ;
        size_t available_out;
        uint8_t* next_out = outbuf;
        size_t produced;

        if (limit_output) {
            size_t used = (size_t)RSTRING_LEN(output);
            if (used >= output_buffer_limit) {
                brotli_decompressor_store_pending_input(br, next_in, available_in);
                break;
            }
            if (output_buffer_limit - used < chunk_size) {
                chunk_size = output_buffer_limit - used;
            }
        }

        available_out = chunk_size;
        result = BrotliDecoderDecompressStream(br->state,
                                               &available_in,
                                               &next_in,
                                               &available_out,
                                               &next_out,
                                               NULL);

        produced = chunk_size - available_out;
        if (produced > 0) {
            rb_str_cat(output, (const char*)outbuf, produced);
        }

        if (result == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
            if (limit_output && (size_t)RSTRING_LEN(output) >= output_buffer_limit) {
                brotli_decompressor_store_pending_input(br, next_in, available_in);
                break;
            }
            continue;
        }
        if (result == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) {
            br->pending_input = Qnil;
            break;
        }
        if (result == BROTLI_DECODER_RESULT_SUCCESS) {
            br->finished = BROTLI_TRUE;
            br->pending_input = Qnil;
            if (available_in > 0) {
                rb_raise(rb_eBrotli, "Excessive input");
            }
            break;
        }

        rb_raise(rb_eBrotli, "%s",
                 BrotliDecoderErrorString(BrotliDecoderGetErrorCode(br->state)));
    }

    RB_GC_GUARD(input_source);
    return output;
}

static VALUE
rb_decompressor_is_finished(VALUE self)
{
    brotli_decompressor_t *br;

    TypedData_Get_Struct(self, brotli_decompressor_t, &brotli_decompressor_data_type, br);
    if (!br->state) {
        rb_raise(rb_eBrotli, "Decompressor is closed");
    }

    if (br->finished || BrotliDecoderIsFinished(br->state)) {
        br->finished = BROTLI_TRUE;
        return Qtrue;
    }
    return Qfalse;
}

static VALUE
rb_decompressor_can_accept_more_data(VALUE self)
{
    brotli_decompressor_t *br;
    TypedData_Get_Struct(self, brotli_decompressor_t, &brotli_decompressor_data_type, br);
    if (!br->state) {
        rb_raise(rb_eBrotli, "Decompressor is closed");
    }
    return (br->finished || brotli_decompressor_has_pending_input(br)) ? Qfalse : Qtrue;
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
    VALUE rb_Reader;
    rb_mBrotli = rb_define_module("Brotli");
    rb_eBrotli = rb_define_class_under(rb_mBrotli, "Error", rb_eStandardError);
    rb_global_variable(&rb_eBrotli);
    rb_define_singleton_method(rb_mBrotli, "deflate", brotli_deflate, -1);
    rb_define_singleton_method(rb_mBrotli, "inflate", brotli_inflate, -1);
    rb_define_singleton_method(rb_mBrotli, "version", brotli_version, 0);
    id_read = rb_intern("read");
    id_write = rb_intern("write");
    id_flush = rb_intern("flush");
    id_close = rb_intern("close");
    id_process = rb_intern("process");
    id_finish = rb_intern("finish");
    id_is_finished = rb_intern("is_finished");
    id_can_accept_more_data = rb_intern("can_accept_more_data");

    rb_cBrotliCompressor = rb_define_class_under(rb_mBrotli, "Compressor", rb_cObject);
    rb_define_alloc_func(rb_cBrotliCompressor, rb_compressor_alloc);
    rb_define_method(rb_cBrotliCompressor, "initialize", rb_compressor_initialize, -1);
    rb_define_method(rb_cBrotliCompressor, "process", rb_compressor_process, 1);
    rb_define_method(rb_cBrotliCompressor, "flush", rb_compressor_flush, 0);
    rb_define_method(rb_cBrotliCompressor, "finish", rb_compressor_finish, 0);

    rb_cBrotliDecompressor = rb_define_class_under(rb_mBrotli, "Decompressor", rb_cObject);
    rb_define_alloc_func(rb_cBrotliDecompressor, rb_decompressor_alloc);
    rb_define_method(rb_cBrotliDecompressor, "initialize", rb_decompressor_initialize, -1);
    rb_define_method(rb_cBrotliDecompressor, "process", rb_decompressor_process, -1);
    rb_define_method(rb_cBrotliDecompressor, "is_finished", rb_decompressor_is_finished, 0);
    rb_define_method(rb_cBrotliDecompressor, "finished?", rb_decompressor_is_finished, 0);
    rb_define_method(rb_cBrotliDecompressor, "can_accept_more_data", rb_decompressor_can_accept_more_data, 0);

    rb_Writer = rb_define_class_under(rb_mBrotli, "Writer", rb_cObject);
    rb_define_alloc_func(rb_Writer, rb_writer_alloc);
    rb_define_method(rb_Writer, "initialize", rb_writer_initialize, -1);
    rb_define_method(rb_Writer, "write", rb_writer_write, 1);
    rb_define_method(rb_Writer, "finish", rb_writer_finish, 0);
    rb_define_method(rb_Writer, "flush", rb_writer_flush, 0);
    rb_define_method(rb_Writer, "close", rb_writer_close, 0);

    rb_Reader = rb_define_class_under(rb_mBrotli, "Reader", rb_cObject);
    rb_define_alloc_func(rb_Reader, rb_reader_alloc);
    rb_define_method(rb_Reader, "initialize", rb_reader_initialize, -1);
    rb_define_method(rb_Reader, "read", rb_reader_read, -1);
    rb_define_method(rb_Reader, "readpartial", rb_reader_readpartial, -1);
    rb_define_method(rb_Reader, "gets", rb_reader_gets, -1);
    rb_define_method(rb_Reader, "each_line", rb_reader_each_line, -1);
    rb_define_method(rb_Reader, "each", rb_reader_each_line, -1);
    rb_define_method(rb_Reader, "eof?", rb_reader_eof_p, 0);
    rb_define_method(rb_Reader, "close", rb_reader_close, 0);
    rb_define_method(rb_Reader, "closed?", rb_reader_closed_p, 0);
}
