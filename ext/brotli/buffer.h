#ifndef BUFFER_H
#define BUFFER_H 1

#include <stddef.h>

typedef struct {
    char* ptr;
    size_t size;
    size_t used;
} buffer_t;

/* buffer_t is a plain C growable buffer (malloc/realloc/free), safe to use
 * without the GVL. create_buffer returns NULL on allocation failure, and
 * append_buffer returns 0 on success or -1 on allocation failure -- they never
 * raise, so they can be called from inside rb_thread_call_without_gvl. */
buffer_t* create_buffer(size_t initial);
void delete_buffer(buffer_t* buffer);
int append_buffer(buffer_t* buffer, const void* ptr, size_t size);

#endif /* BUFFER_H */
