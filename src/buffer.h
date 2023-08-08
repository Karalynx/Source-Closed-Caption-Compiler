#ifndef BUFFER_H_INCLUDED
#define BUFFER_H_INCLUDED

#include <stdint.h>

typedef struct _Buffer {
    char* data;
    uint32_t size;
    uint32_t capacity;
} Buffer;

Buffer* buffer_init(const uint32_t size);
Buffer* buffer_dup(Buffer* self, const char ch, const uint32_t n);
Buffer* buffer_append(Buffer* self, const void* data, const uint32_t data_size);

void buffer_destroy(Buffer** self);

#endif