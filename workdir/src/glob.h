#ifndef GLOB_H_
#define GLOB_H_
#include "dynamic_string.h"
#define WIN32_LEAN_AND_MEAN
#include "windows.h"

/**
 * Get current dir.
 */
bool get_workdir(WString* str);

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

#endif
