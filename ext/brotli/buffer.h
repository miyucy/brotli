#ifndef __BUFFER_HEADER__
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

typedef struct {
    char* ptr;
    size_t size;
    size_t used;
    size_t expand_ratio;
    size_t expand_count;
} buffer_t;

buffer_t* create_buffer(const size_t initial);
void delete_buffer(buffer_t* buffer);
buffer_t* append_buffer(buffer_t* buffer, const void* ptr, const size_t size);

#endif // __BUFFER_HEADER__
