#ifndef BUFFER_H
#define BUFFER_H 1

#include <stddef.h>

typedef struct {
    char* ptr;
    size_t size;
    size_t used;
} buffer_t;

/* Never raise, so they are safe without the GVL. create_buffer returns NULL
 * and append_buffer returns -1 when allocation fails. */
buffer_t* create_buffer(size_t initial);
void delete_buffer(buffer_t* buffer);
int append_buffer(buffer_t* buffer, const void* ptr, size_t size);

#endif /* BUFFER_H */
