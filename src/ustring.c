#include <stdlib.h>
#include "ustring.h"

#define USTR_INITIAL_CAPACITY 15

UString* ustring_init(const UChar* str)
{
    UString* string = (UString*)malloc(sizeof(UString));
    if(!string)
        return NULL;

    if(__builtin_expect(str != NULL, 0))
    {
        const uint32_t length = u_strlen(str);
        const uint32_t capacity = (length >= USTR_INITIAL_CAPACITY) ? length << 1 : USTR_INITIAL_CAPACITY;
    
        string->data = (UChar*)malloc((capacity + 1) * sizeof(UChar));
        if(!string->data)
        {
            free(string);
            return NULL;
        }

        string->size = length;
        string->capacity = capacity;

        u_memcpy(string->data, str, length + 1);
    }
    else
    {
        string->data = (UChar*)malloc((USTR_INITIAL_CAPACITY + 1) * sizeof(UChar));
        if(!string->data)
        {
            free(string);
            return NULL;
        }

        string->size = 0;
        string->capacity = USTR_INITIAL_CAPACITY;
    }
    
    return string;
}

UString* ustring_init_prealloced(const uint32_t n)
{
    UString* string = (UString*)malloc(sizeof(UString));
    if(!string)
        return NULL;

    string->size = 0;
    string->capacity = (n >= USTR_INITIAL_CAPACITY) ? n : USTR_INITIAL_CAPACITY;
    string->data = (UChar*)malloc((string->capacity + 1) * sizeof(UChar));
    if(!string->data)
    {
        free(string);
        string = NULL;
    }

    return string;
}

void ustring_shrink_to_fit(UString* str)
{
    str->data = (UChar*)realloc(str->data, (str->size + 1) * sizeof(UChar));
    str->capacity = str->size;
}

UString* ustring_getline(UFILE* stream)
{
    UString* str = ustring_init_prealloced(USTR_INITIAL_CAPACITY << 2);
    if(!str)
        return NULL;

    register UChar ch = 0;
    register UChar* s = str->data;
    while((ch = u_fgetc(stream)) && ch != u'\n' && ch != U_EOF)
    {
        if(ch == u'\r')
            continue;

        ++str->size;
        if(str->size > str->capacity)
        {
            const uint32_t new_capacity = str->capacity << 1;
            UChar* new_data = (UChar*)realloc(str->data, (str->capacity << 1) * sizeof(UChar));
            if(!new_data)
            {
                ustring_destroy(&str);
                return NULL;
            }

            str->data = new_data;
            str->capacity = new_capacity;
            s = str->data + str->size - 1;
        }

        *s = ch;
        ++s;
    }
    *s = '\0';

    return str;
}

int32_t ustring_compare(const UString* self, const UString* str)
{
	const uint32_t min_length = (self->size < str->size) ? self->size : str->size;

	int32_t result = u_memcmp(self->data, str->data, min_length);
	if(result == 0)
		result = (int32_t)self->size - (int32_t)str->size;

    return result;
}

void ustring_destroy(UString** self)
{
    free((*self)->data);
    free((*self));
    *self = NULL;
}