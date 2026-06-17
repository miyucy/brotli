#include "buffer.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_INITIAL_SIZE 1024

/*
 * buffer_t is backed by malloc/realloc/free, NOT the Ruby allocators. This is
 * deliberate: append_buffer runs inside rb_thread_call_without_gvl, where
 * calling a Ruby allocator is undefined behaviour (it may trigger GC or raise
 * NoMemoryError, both of which require the GVL). Allocation failures are
 * reported through return values; the GVL-holding caller raises after cleanup.
 */

buffer_t*
create_buffer(size_t initial)
{
    buffer_t *buffer = malloc(sizeof(*buffer));

    if (!buffer) {
        return NULL;
    }
    buffer->used = 0;
    buffer->size = initial > 0 ? initial : BUFFER_INITIAL_SIZE;
    buffer->ptr = malloc(buffer->size);
    if (!buffer->ptr) {
        free(buffer);
        return NULL;
    }
    return buffer;
}

void
delete_buffer(buffer_t* buffer)
{
    if (!buffer) {
        return;
    }

    free(buffer->ptr);
    free(buffer);
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

static int
expand_buffer(buffer_t* buffer, size_t required)
{
    size_t new_size = buffer_size_for(buffer->size, required);
    char* new_ptr = realloc(buffer->ptr, new_size);

    if (!new_ptr) {
        return -1;
    }
    buffer->ptr = new_ptr;
    buffer->size = new_size;
    return 0;
}

int
append_buffer(buffer_t* buffer, const void* ptr, size_t size)
{
    size_t required;

    if (size == 0) {
        return 0;
    }

    required = buffer->used + size;
    if (required > buffer->size) {
        if (expand_buffer(buffer, required) != 0) {
            return -1;
        }
    }

    memcpy(buffer->ptr + buffer->used, ptr, size);
    buffer->used += size;
    return 0;
}
