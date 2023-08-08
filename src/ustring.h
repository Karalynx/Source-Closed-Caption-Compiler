#ifndef USTRING_H_INCLUDED
#define USTRING_H_INCLUDED

#include <unicode/ustring.h>
#include <unicode/ustdio.h>

typedef struct _UString {
    UChar* data;
    uint32_t size;
    uint32_t capacity;
} UString;

UString* ustring_init(const UChar* str);
UString* ustring_init_prealloced(const uint32_t n);
UString* ustring_getline(UFILE* stream);

int32_t ustring_compare(const UString* self, const UString* str);

void ustring_shrink_to_fit(UString* str);
void ustring_destroy(UString** self);

#endif