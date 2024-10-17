#include "dynamic_string.h"
#include <Windows.h>


typedef struct _RowData {
    DWORD end_ix;
    SHORT width;
} RowData;

typedef struct _CliListNode {
    WString data;
    DWORD flags;
    DWORD rows;
    RowData *row_data;
    DWORD row_capacity;
} CliListNode;

typedef struct _CliList {
    const wchar_t* header;

    CliListNode* elements;
    DWORD element_count;

    DWORD elements_capacity;
    DWORD options;
    WString scratch_buffer;
} CliList;

static const DWORD CLI_MOVE = 1;
static const DWORD CLI_EDIT = 2;
static const DWORD CLI_DELETE = 4;
static const DWORD CLI_INSERT = 8;
static const DWORD CLI_COPY = 16;
static const DWORD CLI_KEEP_TABS = 32;
static const DWORD CLI_OBSCURED = 64;
static const DWORD CLI_INVALID_UNICODE = 128;

CliList* CliList_create(const wchar_t* header, WString* elements, DWORD count, DWORD flags);

DWORD CliList_run(CliList* list);

void CliList_free(CliList* list);

