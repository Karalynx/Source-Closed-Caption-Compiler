#include <stdlib.h>
#include <errno.h>
#include "caption.h"
#include "valve_crc32.h"

Caption* caption_init()
{
    Caption* caption = (Caption*)malloc(sizeof(Caption));
    if(__builtin_expect(caption != NULL, 0))
    {
        caption->hash = 0;
        caption->key = NULL;
        caption->value = NULL;
    }

    return caption;
}

Caption* caption_hash(Caption* caption)
{
    int32_t buff_capacity = (caption->key->size + 1) * UTF8_MAX_CHAR_LENGTH;
    int32_t buff_size = 0;
    char* buffer = (char*)malloc(buff_capacity);
    if(!buffer)
        return NULL;

    UErrorCode error_code = 0;
    u_strToUTF8(buffer, buff_capacity, &buff_size, caption->key->data, caption->key->size, &error_code);
    
    if(!U_FAILURE(error_code))
        caption->hash = CRC32_ProcessSingleBuffer(buffer, buff_size);
    else
    {
        errno = EIO;
        caption = NULL;
    }

    free(buffer);

    return caption;
}

void caption_destroy(Caption** caption)
{
    Caption* temp_caption = *caption;
    if(temp_caption->key)
        ustring_destroy(&temp_caption->key);

    if(temp_caption->value)
        ustring_destroy(&temp_caption->value);

    free(temp_caption);
    *caption = NULL;
}