#ifndef GLOB_H_
#define GLOB_H_
#include "dynamic_string.h"
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include <stdint.h>

/**
 * Get current dir.
 */
bool get_workdir(WString* str);

bool find_file_relative(wchar_t* buf, size_t size, const wchar_t *filename, bool exist);

bool is_file(const wchar_t* str);

typedef struct _Path {
    WString path;
    uint32_t name_len;
    bool is_dir;
    bool is_link;
} Path;

typedef struct _WalkCtx {
    HANDLE handle;
    Path p;

    bool first;
    bool absolute_path;
} WalkCtx;

DWORD get_file_attrs(const wchar_t* path);

bool WalkDir_begin(WalkCtx* ctx, const wchar_t* dir, bool absolute_path);

int WalkDir_next(WalkCtx* ctx, Path** path);

void WalkDir_abort(WalkCtx* ctx);

bool read_text_file(String_noinit* str, const wchar_t* filename);

bool read_utf16_file(WString_noinit* str, const wchar_t* filename);

#endif
