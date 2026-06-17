#include "buffer.h"
#include "ruby.h"

#include <stdint.h>
#include <string.h>

#define BUFFER_INITIAL_SIZE 1024

struct buffer_alloc {
    size_t size;
    char *ptr;
};

static VALUE
create_buffer_data(VALUE arg)
{
    struct buffer_alloc *a = (struct buffer_alloc *)arg;
    a->ptr = ruby_xmalloc(a->size);
    return Qnil;
}

buffer_t*
create_buffer(size_t initial)
{
    buffer_t *buffer = ruby_xmalloc(sizeof(*buffer));
    struct buffer_alloc a;
    int state = 0;

    a.size = initial > 0 ? initial : BUFFER_INITIAL_SIZE;
    a.ptr = NULL;
    rb_protect(create_buffer_data, (VALUE)&a, &state);
    if (state) {
        ruby_xfree(buffer);
        rb_jump_tag(state);
    }
    buffer->used = 0;
    buffer->size = a.size;
    buffer->ptr = a.ptr;
    return buffer;
}

void
delete_buffer(buffer_t* buffer)
{
    if (!buffer) {
        return;
    }

    ruby_xfree(buffer->ptr);
    ruby_xfree(buffer);
}

static size_t
buffer_size_for(size_t current, size_t required)
{
    size_t size = current > 0 ? current : BUFFER_INITIAL_SIZE;

    while (size < required) {
        if (size > SIZE_MAX / 2) {
            return required;
        }
        size *= 2;
    }

    return size;
}

static void
expand_buffer(buffer_t* buffer, size_t required)
{
    buffer->size = buffer_size_for(buffer->size, required);
    buffer->ptr = ruby_xrealloc(buffer->ptr, buffer->size);
}

void
append_buffer(buffer_t* buffer, const void* ptr, size_t size)
{
    size_t required;

    if (size == 0) {
        return;
    }

    required = buffer->used + size;
    if (required > buffer->size) {
        expand_buffer(buffer, required);
    }

    memcpy(buffer->ptr + buffer->used, ptr, size);
    buffer->used += size;
}
