#include <stdlib.h>
#include <memory.h>
#include "buffer.h"

Buffer* buffer_init(const uint32_t size)
{
    Buffer* buffer = (Buffer*)malloc(sizeof(Buffer));
    if(__builtin_expect(buffer != NULL, 0))
    {
        buffer->data = (char*)malloc(size);
        if(!buffer->data)
        {
            free(buffer);
            return NULL;
        }

        buffer->size = 0;
        buffer->capacity = size;
    }
    
    return buffer; 
}

Buffer* buffer_append(Buffer* self, const void* data, const uint32_t data_size)
{
	const uint32_t new_length = self->size + data_size;
	if(new_length > self->capacity)
	{
		const uint32_t new_capacity = new_length + ((data_size + self->size) >> 1);
        char* new_data = (char*)realloc(self->data, new_capacity);
        if(!new_data)
            return NULL;

		self->data = (char*)new_data;
		self->capacity = new_capacity;
	}

	memcpy(self->data + self->size, data, data_size);
	self->size = new_length;

	return self;
}

Buffer* buffer_dup(Buffer* self, const char ch, const uint32_t n)
{
	const uint32_t new_length = self->size + n;
	if(new_length > self->capacity)
	{
		const uint32_t new_capacity = new_length + ((n + self->size) >> 1);
        char* new_data = (char*)realloc(self->data, new_capacity);
        if(!new_data)
            return NULL;

		self->data = new_data;
		self->capacity = new_capacity;
	}
    
    memset(self->data + self->size, ch, n);
	self->size = new_length;

	return self;
}

void buffer_destroy(Buffer** self)
{
    free((*self)->data);
    free((*self));
    *self = NULL;
}
