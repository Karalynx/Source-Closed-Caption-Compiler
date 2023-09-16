#ifndef CAPTION_LIST_H_INCLUDED
#define CAPTION_LIST_H_INCLUDED

#include "caption.h"

typedef struct _CaptionNode {
    Caption* caption;
    struct _CaptionNode* next;
} CaptionNode;

typedef struct _CaptionList {
    CaptionNode* head;
    CaptionNode* tail;
    uint64_t size;
} CaptionList;

CaptionList* caption_list_init();
CaptionList* caption_list_push(CaptionList* list, Caption* caption);
CaptionList* caption_list_sort(CaptionList* list);

Caption* caption_list_pop(CaptionList* list);

void caption_list_empty(CaptionList* list);
void caption_list_destroy(CaptionList** list);

#endif