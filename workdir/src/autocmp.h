#ifndef AUTOCMD_H_00
#define AUTOCMD_H_00

#include <stdint.h>
#include "match_node.h"

typedef struct ParenList {
    char* str;
    struct ParenList* next;
} ParenList;

typedef struct Command {
    uint32_t count;
    char* parts[];
} Command;

typedef struct Line {
    Command* command;
    Command* loop;
} Line;

#endif
