#include "buffer.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_INITIAL_SIZE 1024

/*
 * append_buffer runs inside rb_thread_call_without_gvl. A failing Ruby
 * allocator there raises NoMemoryError past the caller's cleanup, so this
 * buffer uses malloc/realloc/free and reports failure through return values.
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
    size_t size = buffer_size_for(buffer->size, required);
    char* ptr = realloc(buffer->ptr, size);

    if (!ptr) {
        return -1;
    }
    buffer->ptr = ptr;
    buffer->size = size;
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
