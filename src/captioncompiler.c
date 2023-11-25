/*
 * Author:    Ernest King
 * Created:   7/8/2023
 */

#include <ctype.h>
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

typedef enum _CompilerFlags {
    Verbose = 0x01,
} CompilerFlags;

typedef enum _ParserErrors {
    ArgCount = 0x01,
    InvalidArg = 0x02,
    MissingArg = 0x04,
} ParserErrors;

typedef struct _ParserErrorData {
    ParserErrors code;
    char* argument;
} ParserErrorData;

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

static CaptionList* read_captions(const char* filename, uint8_t flags)
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

        if(flags & Verbose)
            u_fprintf(u_get_stdout(), "Parsing line \'%S\'\n", line->data);
        
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

    if(flags & Verbose)
        u_fprintf(u_get_stdout(), "Found %lu entries\n\n", list->size);

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

static int8_t compile(CaptionList* captions, const char* filepath, uint8_t flags)
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
    
    if(flags & Verbose)
        u_fprintf(u_get_stdout(), "Writing VPK Header\nVCCD: %d\nVersion: %d\nBlock Count: %d\nBlock Size: %d\nDIR Size: %d\nData Offset: %d\n\n", header.vccd, header.version, header.block_count, header.block_size, header.dir_size, header.data_offset);
    
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

        if(flags & Verbose)
            u_fprintf(u_get_stdout(), "Writing Caption data for \'%S\'\nHash: %u\nBlock: %d\nOffset: %hd\nLength: %hd\n\n", caption->value->data, caption->hash, header.block_count, current_offset, length_bytes);
        
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

    if(flags & Verbose)
        u_fprintf(u_get_stdout(), "Padding Dictionary with %d zeroes\n\n", dict_padding);

    // Pad dictionary with 0
    while(--dict_padding >= 0)
        fputc(0, out_file);

    if(flags & Verbose)
        u_fprintf(u_get_stdout(), "Writing caption strings of length %u\n", caption_buffer->size);

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

    if(flags & Verbose)
        u_fprintf(u_get_stdout(), "Updating Block Size from 0 to %d\n\n", header.block_count);

    fseek(out_file, 2 * sizeof(int32_t), SEEK_SET);
    fwrite(&header.block_count, sizeof(int32_t), 1, out_file);
    
    fclose(out_file);

    if(flags & Verbose)
        u_fprintf(u_get_stdout(), "Successfully compiled to '%s'\n", filepath);

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

    const char* src_filepath = "";
    uint8_t flags = 0;

    ParserErrorData error_data = {ArgCount, ""};
    int i = 1;

    if(argc < 2)
        goto PRINT_HELP;

    while(i < argc)
    {
        if(argv[i][0] != '-')
        {
            if(i == argc - 1)
            {
                src_filepath = argv[i];
                goto VALID_ARGS;
            }

            error_data = (ParserErrorData){InvalidArg, argv[i]};
            goto PARSER_ERROR;
        }

        switch (tolower(argv[i][1])) 
        {
            case '\0':
            {
                error_data = (ParserErrorData){InvalidArg, argv[i]};
                goto PARSER_ERROR;
            }
            case 'h':
                goto PRINT_HELP;
            case 'v':
                flags |= Verbose;
                break;
        }

        if(argv[i][2] != '\0')
        {
            error_data = (ParserErrorData){InvalidArg, argv[i]};
            goto PARSER_ERROR;
        }

        ++i;
    }

    goto VALID_ARGS;

    PRINT_HELP:
    {
        fprintf(stdout, "%s\n", help_message);
        return 0;
    }

    PARSER_ERROR:
    {
        switch (error_data.code) 
        {
            case ArgCount:
                fprintf(stderr, "Must specify VPK Archive directory\n");
                return 1;
            case InvalidArg:
                fprintf(stderr, "Invalid argument \'%s\'\n", error_data.argument);
                return 1;
            case MissingArg:
                fprintf(stderr, "Missing argument(s) for \'%s\'\n", error_data.argument);
                return 1;
            default:
                fprintf(stderr, "An error occured during parsing\n");
                return 1;
        }
    }

    VALID_ARGS:
    {
        const uint32_t src_length = strlen(src_filepath);
        if(src_length < 4 || memcmp(src_filepath + (src_length - 4), ".txt", 4) != 0)
        {
            fprintf(stderr, "Only .txt files are accepted.\n");
            return -1;
        }

        char out_filepath[src_length + 1];
        memcpy(out_filepath, src_filepath, src_length - 4);
        memcpy(out_filepath + (src_length - 4), ".dat", 5);


        CaptionList* list = read_captions(src_filepath, flags);
        if(!list)
            return -1;

        if(!compile(list, out_filepath, flags))
        {
            caption_list_destroy(&list);
            return -1;
        }

        caption_list_destroy(&list);
    }

    return 0;
}