#ifndef GLOB_H_
#define GLOB_H_
#include "dynamic_string.h"
#define WIN32_LEAN_AND_MEAN
#include "windows.h"

/**
 * Get current dir.
 */
bool get_workdir(WString* str);

bool find_file_relative(wchar_t* buf, size_t size, const wchar_t *filename, bool exist);

bool is_file(const wchar_t* str);

typedef struct _Path {
    WString path;
    bool is_dir;
} Path;

typedef struct _WalkCtx {
    HANDLE handle;
    Path p;

    bool first;
} WalkCtx;

bool WalkDir_begin(WalkCtx* ctx, const wchar_t* dir);

int WalkDir_next(WalkCtx* ctx, Path** path);

void WalkDir_abort(WalkCtx* ctx);

bool read_text_file(String_noinit* str, const wchar_t* filename);

bool append_file(const char* str, const wchar_t* filename);

#endif
