#include <stdlib.h>
#include "caption_list.h"

CaptionList* caption_list_init()
{
    CaptionList* caption_list = (CaptionList*)malloc(sizeof(CaptionList));

    if(caption_list)
    {
        caption_list->head = NULL;
        caption_list->tail = NULL;
        caption_list->size = 0;
    }

    return caption_list;
}

static CaptionNode* create_caption_node(Caption* caption)
{
    CaptionNode* node = (CaptionNode*)malloc(sizeof(CaptionNode));
    if(node)
    {
        node->caption = caption;
        node->next = NULL;
    }

    return node;
}

CaptionList* caption_list_push(CaptionList* list, Caption* caption)
{
    CaptionNode* new_node = create_caption_node(caption);
    if(__builtin_expect(new_node != NULL, 0))
    {
        if(list->tail)
            list->tail->next = new_node;
        else
            list->head = new_node;

        list->tail = new_node;
        ++list->size;
    }
    else
        list = NULL;

    return list;
}

Caption* caption_list_pop(CaptionList* list)
{
    Caption* caption = NULL;
    if(__builtin_expect(list->head != NULL, 0))
    {
        caption = list->head->caption;
        
        CaptionNode* temp_node = list->head;
        list->head = list->head->next;

        if(list->size == 1)
            list->tail = NULL;

        free(temp_node);
        --list->size;
    }

    return caption;
}

CaptionList* caption_list_sort(CaptionList* list)
{
    if(!list->head)
        return list;

    CaptionNode* left, *right;
    CaptionNode* next;
    uint32_t list_size = 1, merge_amount = 0;
    uint32_t left_size = 0, right_size = 0;

    do {
        left = list->head;
        list->tail = NULL;
        list->head = NULL;
        merge_amount = 0;

        while(left) 
        {
            right = left;
            right_size = list_size;
            left_size = 0;
            ++merge_amount;

            while(right && left_size < list_size) 
            {
                ++left_size;
                right = right->next;
            }

            while(left_size > 0 || (right_size > 0 && right)) 
            {
                if(!left_size)
                {
                    next = right;
                    right = right->next;
                    --right_size;
                }

                else if(!right_size || !right)
                {
                    next = left;
                    left = left->next;
                    --left_size;
                }

                // Key will already be lowercase
                else if(ustring_compare(left->caption->key, right->caption->key) < 0)
                {
                    next = left;
                    left = left->next;
                    --left_size;
                }

                else
                {
                    next = right;
                    right = right->next;
                    --right_size;
                }

                if(list->tail) 
                    list->tail->next = next;  
                else 
                    list->head = next;

                list->tail = next;          
            }
            left = right;
        }
        list->tail->next = NULL;
        list_size <<= 1;

    } while(merge_amount > 1);

    return list;
}

void caption_list_empty(CaptionList* list)
{
    CaptionNode* current_node = list->head;
    while(current_node)
    {
        CaptionNode* temp_node = current_node;
        current_node = current_node->next;

        caption_destroy(&temp_node->caption);
        free(temp_node);
    }
    list->size = 0;
}

void caption_list_destroy(CaptionList** list)
{
    caption_list_empty(*list);
    free(*list);
    *list = NULL;
}