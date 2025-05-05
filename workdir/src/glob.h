#ifndef GLOB_H_
#define GLOB_H_
#include "dynamic_string.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdint.h>


wchar_t** glob_command_line(const wchar_t* args, int* argc);

wchar_t** glob_command_line_with(const wchar_t* args, int* argc,
                                 unsigned options);

/**
 * Get current dir.
 */
bool get_workdir(WString* str);

bool find_file_relative(wchar_t* buf, size_t size, const wchar_t *filename, bool exist);

bool is_file(const wchar_t* str);

bool is_directory(const wchar_t* str);

bool make_absolute(const wchar_t* path, WString* dest);

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

typedef struct _GlobCtx {
    HANDLE handle;
    Path p;
    struct _GlobCtxNode {
        uint32_t pattern_offset;
        wchar_t* pattern;
    }* stack;
    uint32_t stack_size;
    uint32_t stack_capacity;
    uint32_t last_segment;
} GlobCtx;

typedef struct _LineCtx {
    HANDLE file;
    bool eof;
    bool ended_cr;
    uint32_t str_offset;
    uint64_t offset;
    String buffer;
    String line;
    OVERLAPPED o;
} LineCtx;

bool LineIter_begin(LineCtx* ctx, const wchar_t* filename);

char* LineIter_next(LineCtx* ctx, uint64_t* len);

void LineIter_abort(LineCtx* ctx);

bool matches_glob(const wchar_t* pattern, const wchar_t* str);

bool Glob_begin(const wchar_t* pattern, GlobCtx* ctx);

bool Glob_next(GlobCtx* ctx, Path** path);

void Glob_abort(GlobCtx* ctx);

DWORD get_file_attrs(const wchar_t* path);

bool WalkDir_begin(WalkCtx* ctx, const wchar_t* dir, bool absolute_path);

int WalkDir_next(WalkCtx* ctx, Path** path);

void WalkDir_abort(WalkCtx* ctx);

bool read_text_file(String_noinit* str, const wchar_t* filename);

bool read_utf16_file(WString_noinit* str, const wchar_t* filename);

#endif
