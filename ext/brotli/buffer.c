#include "buffer.h"
#include "ruby/ruby.h"
#define INITIAL (1024)

buffer_t*
create_buffer(const size_t initial) {
    buffer_t *buffer = ruby_xmalloc(sizeof(buffer_t));
    if (buffer == NULL) {
        return NULL;
    }

    buffer->used = 0;
    buffer->expand_count = 0;
    buffer->expand_ratio = 130;
    buffer->size = (size_t) (initial > 0 ? initial : INITIAL);
    buffer->ptr = ruby_xmalloc(buffer->size);
    if (buffer->ptr == NULL) {
        delete_buffer(buffer);
        return NULL;
    }

    return buffer;
}

void
delete_buffer(buffer_t* buffer) {
    if (buffer->ptr != NULL) {
        ruby_xfree(buffer->ptr);
        buffer->ptr = NULL;
    }
    ruby_xfree(buffer);
}

static
buffer_t*
expand_buffer(buffer_t* const buffer, const size_t need) {
    size_t size = need * buffer->expand_ratio / 100;
    buffer->ptr = ruby_xrealloc(buffer->ptr, size);
    buffer->size = size;
    buffer->expand_count += 1;
    return buffer;
}

buffer_t*
append_buffer(buffer_t* buffer, const void* ptr, const size_t size) {
    if (buffer->used + size > buffer->size) {
        if (expand_buffer(buffer, buffer->used + size) == NULL) {
            return NULL;
        }
    }
    memcpy(buffer->ptr + buffer->used, ptr, size);
    buffer->used += size;
    return buffer;
}

#if 0
#include <stdio.h>

void
inspect_buffer(buffer_t* buffer) {
    printf("ptr=%p size=%d used=%d expc=%d\n",
           buffer->ptr,
           buffer->size,
           buffer->used,
           buffer->expand_count);
}

int
main(int argc, char *argv[])
{
    buffer_t* b = create_buffer(8);

    inspect_buffer(b);
    append_buffer(b, "1111", 4);
    inspect_buffer(b);
    append_buffer(b, "2222", 4);
    inspect_buffer(b);
    append_buffer(b, "3333", 4);
    inspect_buffer(b);
    append_buffer(b, "4444", 4);
    inspect_buffer(b);
    append_buffer(b, "5555", 4);
    inspect_buffer(b);
    append_buffer(b, "66", 2);
    inspect_buffer(b);

    delete_buffer(b);
    return 0;
}
#endif
