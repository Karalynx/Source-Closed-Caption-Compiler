/*
 * Author:    Ernest King
 * Created:   7/8/2023
 */

#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include "buffer.h"
#include "caption_list.h"

#define VCCD 1145258838
#define VERSION 1
#define BLOCK_SIZE 8192
#define DIR_ENTRY_SIZE (4+4+2+2)
#define HEADER_SIZE 24

#define INITIAL_BUFFER_SIZE 5000

typedef struct _Header {
    int32_t vccd, version;
    int32_t block_count, block_size;
    int32_t dir_size;
    int32_t data_offset;
} Header;

Caption* extract_strings(Caption* caption, const UString* line, int8_t* error)
{
    // Key
    caption->key = ustring_init_prealloced(line->size);
    if(!caption->key)
    {
        *error = 1;
        return NULL;
    }

    register const UChar* data = line->data;
    register UChar* new_data = caption->key->data;

    // Skip any whitespace characters until start of key
    while(u_isspace(*data))
        ++data;

    if(*data == '\0' || *data == '/' || *data == '{' || *data == '}')
        return NULL;

    if(*data == '\"')
        ++data;

    // Copy lowercase char to dest until end of key is found
    while(*data != '\"' && !u_isspace(*data) && *data != '\0')
        *new_data++ = u_tolower(*data++);

    if(*data == '\"')
        ++data;
    
    *new_data = '\0';
    caption->key->size = new_data - caption->key->data;
    ustring_shrink_to_fit(caption->key);

    if(caption->key->size == 0)
    {
        *error = 1;
        errno = EINVAL;
        return NULL;
    }
    else if(u_strncmp(caption->key->data, u"[english]", 9) == 0)
        return NULL;

    // Value
    caption->value = ustring_init_prealloced(line->size - caption->key->size);
    if(!caption->value)
    {
        *error = 1;
        return NULL;
    }

    new_data = caption->value->data;

    // Skip any whitespace characters until start of value
    while(u_isspace(*data))
        ++data;

    if(*data == '\"')
        ++data;

    // Copy char to dest until end of value is found
    while(*data != '\"' && *data != '\0')
        *new_data++ = *data++;

    *new_data = '\0';
    caption->value->size = new_data - caption->value->data;
    ustring_shrink_to_fit(caption->value);

    if(caption->value->size == 0)
        caption = NULL;
    else if(caption->value->size + 1 > (BLOCK_SIZE >> 1))
    {
        *error = 1;
        errno = EOVERFLOW;
        caption = NULL;
    }

    return caption;
}

Caption* parse_caption(const UString* line, int8_t* error)
{
    *error = 0;
    Caption* caption = caption_init();
    if(!caption)
    {
        *error = 1;
        goto caption_parse_success;
    }

    if(!extract_strings(caption, line, error))
        goto caption_parse_error;

    if(!caption_hash(caption))
    {
        *error = 1;
        goto caption_parse_error;
    }

    goto caption_parse_success;

    caption_parse_error:
        caption_destroy(&caption);

    caption_parse_success:
        ;

    return caption;
}

static CaptionList* read_captions(const char* filename)
{
    UFILE* txt_file = NULL;
    CaptionList* list = NULL;
    
    txt_file = u_fopen(filename, "r", NULL, "utf_16_le");
    if(!txt_file)
        goto caption_read_error;

    list = caption_list_init();
    if(!list)
        goto caption_read_error;

    uint32_t line_count = 0;
    int8_t error = 0;

    u_fgetc(txt_file);

    while(!u_feof(txt_file))
    {
        ++line_count;
        UString* line = ustring_getline(txt_file);
        if(!line)
            goto caption_read_error;

        register UChar* data = line->data;
        while(u_isspace(*data) && *data != '\0')
            ++data;

        if(*data == '\"')
            ++data;

        if(u_strncmp(data, u"Tokens", 6) == 0)
        {
            ustring_destroy(&line);
            break;
        }
        ustring_destroy(&line);
    }

    if(u_feof(txt_file))
    {
        errno = ENODATA;
        goto caption_read_error;
    }
    
    while(!u_feof(txt_file))
    {
        ++line_count;
        UString* line = ustring_getline(txt_file);
        if(!line)
            goto caption_read_error;
        
        Caption* caption = parse_caption(line, &error);
        ustring_destroy(&line);

        if(__builtin_expect(caption != NULL, 0))
        {    
            if(!caption_list_push(list, caption))
            {
                caption_destroy(&caption);
                goto caption_read_error;
            }
        }
        else if(error)
            goto caption_read_error;
    }

    fprintf(stdout, "Found %u entries\n", list->size);

    caption_list_sort(list);
    u_fclose(txt_file);
    goto caption_read_success;

    caption_read_error:
    {
        switch(errno) 
        {
            case EOVERFLOW:
                fprintf(stderr, "An error occured while reading file '%s': Value at line %u exceeds maximum length of %u\n", filename, line_count, (BLOCK_SIZE >> 1) - 1);
            break;
            case ENODATA:
                fprintf(stderr, "An error occured while reading file '%s': Could not find token declaration\n", filename);
            break;
            case EINVAL:
                fprintf(stderr, "An error occured while reading file '%s': Line %u has a key of length 0\n", filename, line_count);
            break;
            default:
                fprintf(stderr, "An error occured while reading file '%s': %s\n", filename, strerror(errno));
            break;
        }

        if(txt_file)
            u_fclose(txt_file);
        if(list)
            caption_list_destroy(&list);
    }

    caption_read_success:
        ;

    return list;
};

static int8_t compile(CaptionList* captions, const char* filepath)
{
    FILE* out_file = NULL;
    Buffer* caption_buffer = NULL;
    
    out_file = fopen(filepath, "wb");
    if(!out_file)
        goto caption_compile_error;

    int32_t dict_padding = 512 - ((HEADER_SIZE + captions->size * DIR_ENTRY_SIZE) % 512);
    
    Header header;
    header.vccd = VCCD;
    header.version = VERSION;
    header.block_count = 0; // Write later
    header.block_size = BLOCK_SIZE;
    header.dir_size = captions->size;
    header.data_offset = HEADER_SIZE + captions->size * DIR_ENTRY_SIZE + dict_padding;
    
    fwrite(&header, HEADER_SIZE, 1, out_file);

    caption_buffer = buffer_init(INITIAL_BUFFER_SIZE * sizeof(UChar));
    if(!caption_buffer)
        goto caption_compile_error;

    uint32_t expected_buffer_size = 0;
    int16_t current_offset = 0;

    while(captions->size != 0)
    {
        Caption* caption = caption_list_pop(captions);
        const int16_t length_bytes = (caption->value->size + 1) * sizeof(UChar);
        
        if(current_offset + length_bytes > BLOCK_SIZE)
        {
            int32_t leftover = BLOCK_SIZE - current_offset;
            buffer_dup(caption_buffer, 0, leftover);

            ++header.block_count;
            expected_buffer_size += leftover;
            current_offset = 0;
        }
        
        // Directory entry
        fwrite(&caption->hash, sizeof(uint32_t), 1, out_file);
        fwrite(&header.block_count, sizeof(int32_t), 1, out_file);
        fwrite(&current_offset, sizeof(int16_t), 1, out_file);
        fwrite(&length_bytes, sizeof(int16_t), 1, out_file);
        
        // Append buffer together with '\0'
        buffer_append(caption_buffer, caption->value->data, length_bytes);

        expected_buffer_size += length_bytes;
        current_offset += length_bytes;
        caption_destroy(&caption);
    }

    int32_t leftover = BLOCK_SIZE - current_offset;
    buffer_dup(caption_buffer, 0, leftover);
    expected_buffer_size += leftover;

    // Pad dictionary with 0
    while(--dict_padding >= 0)
        fputc(0, out_file);

    // Write caption buffer
    fwrite(caption_buffer->data, sizeof(char), caption_buffer->size, out_file);
    buffer_destroy(&caption_buffer);

    if(ftell(out_file) != expected_buffer_size + header.data_offset)
    {
        errno = EIO;
        goto caption_compile_error;
    }
    
    // Go back to header and write the number of blocks
    ++header.block_count;
    fseek(out_file, 2 * sizeof(int32_t), SEEK_SET);
    fwrite(&header.block_count, sizeof(int32_t), 1, out_file);
    
    fclose(out_file);

    fprintf(stdout, "Successfully compiled to '%s'\n", filepath);
    goto caption_compile_success;

    caption_compile_error:
    {
        fprintf(stderr, "An error occured while writing to file '%s': %s\n", filepath, strerror(errno));

        if(out_file)
            fclose(out_file);
        if(caption_buffer)
            buffer_destroy(&caption_buffer);
        if(captions)
            caption_list_empty(captions);

        return 0;
    }

    caption_compile_success:
        ;

    return 1;
}

int main(int argc, char** argv)
{
    const char help_message[] = "Usage: ./Main [source].txt\n\
        \nExample: ./Main closecaption_english.txt";

    if(argc != 2 || (argv[1][0] == '-' && (argv[1][1] == 'h' || argv[1][1] == 'H') && argv[1][2] == '\0'))
    {
        fprintf(stdout, "%s\n", help_message);
        return 0;
    }

    const char* src_filepath = argv[1];
    const uint32_t src_length = strlen(src_filepath);
    if(src_length < 4 || memcmp(src_filepath + (src_length - 4), ".txt", 4) != 0)
    {
        fprintf(stderr, "Only .txt files are accepted.\n");
        return -1;
    }

    char out_filepath[src_length + 1];
    memcpy(out_filepath, src_filepath, src_length - 4);
    memcpy(out_filepath + (src_length - 4), ".dat", 5);


    CaptionList* list = read_captions(src_filepath);
    if(!list)
        return -1;

    if(!compile(list, out_filepath))
    {
        caption_list_destroy(&list);
        return -1;
    }

    caption_list_destroy(&list);

    return 0;
}