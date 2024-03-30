#include "dynamic_string.h"
#include <Windows.h>

typedef struct _CliList {
    const wchar_t* header;

    DynamicWString* elements;
    DWORD element_count;

    DWORD elements_capacity;
    DWORD options;
} CliList;

static const DWORD CLI_MOVE = 1;
static const DWORD CLI_EDIT = 2;
static const DWORD CLI_DELETE = 4;
static const DWORD CLI_INSERT = 8;
static const DWORD CLI_COPY = 16;
static const DWORD CLI_KEEP_TABS = 32;

CliList* CliList_create(const wchar_t* header, DynamicWString* elements, DWORD count, DWORD flags);

DWORD CliList_run(CliList* list);

void CliList_free(CliList* list);

