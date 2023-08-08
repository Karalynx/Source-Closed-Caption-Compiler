#ifndef CAPTION_H_INCLUDED
#define CAPTION_H_INCLUDED

#include "ustring.h"

typedef struct _Caption {
    UString* key;
    UString* value;
    uint32_t hash;
} Caption;

Caption* caption_init();
Caption* caption_hash(Caption* caption);

void caption_destroy(Caption** caption);

#endif