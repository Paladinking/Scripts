#ifndef GLOB_H_
#define GLOB_H_
#include "dynamic_string.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdint.h>
#include "args.h"

#ifndef NARROW_OCHAR

wchar_t** glob_command_line(const wchar_t* args, int* argc);

wchar_t** glob_command_line_with(const wchar_t* args, int* argc,
                                 unsigned options);

#endif

/**
 * Get current dir.
 */
bool get_workdir(WString* str);

bool find_file_relative(wchar_t* buf, size_t size, const wchar_t *filename, bool exist);

bool is_file(const wchar_t* str);

bool is_directory(const wchar_t* str);

bool make_absolute(const wchar_t* path, WString* dest);

bool to_windows_path(const wchar_t* path, WString* s);

typedef struct _Path {
    WString path;
    uint32_t name_len;
    DWORD attrs;
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
    bool binary;
    uint32_t str_offset;
    uint64_t offset;
    union {
        String buffer;
        WString wbuffer;
    };
    String line;
    OVERLAPPED o;
} LineCtx;

bool ConsoleLineIter_begin(LineCtx* ctx, HANDLE in);

char* ConsoleLineIter_next(LineCtx* ctx, uint64_t* len);

void ConsoleLineIter_abort(LineCtx* ctx);

bool SyncLineIter_begin(LineCtx* ctx, HANDLE file);

char* SyncLineIter_next(LineCtx* ctx, uint64_t* len);

void SyncLineIter_abort(LineCtx* ctx);

bool LineIter_begin(LineCtx* ctx, const ochar_t* filename);

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

HANDLE open_file_write(const ochar_t* filename);

bool write_file(const ochar_t* filename, const uint8_t* buf, uint64_t len);

bool read_text_file(String_noinit* str, const ochar_t* filename);

bool read_utf16_file(WString_noinit* str, const wchar_t* filename);

#endif
