#define WIN32_LEAN_AND_MEAN
#define _NO_CRT_STDIO_INLINE
#include <windows.h>
#include "args.h"
#include "regex.h"
#include "glob.h"
#include "printf.h"

#define OPTION_INVALID 0
#define OPTION_VALID 1
#define OPTION_LIST_NAMES 2
#define OPTION_RECURSIVE 4
#define OPTION_LINENUMBER 8
#define OPTION_HELP 16
#define OPTION_FLUSH 32
#define OPTION_NO_FLUSH 64

#define OPTION_IF(opt, cond, flag)                                           \
    if (cond) {                                                              \
        opt |= flag;                                                         \
    }

typedef char* (*next_line_fn_t)(LineCtx*, uint64_t*);
typedef void (*abort_fn_t)(LineCtx*);

const wchar_t *HELP_MESSAGE =
    L"Usage: %s [OPTION]... PATTERN [FILE]...\n"
    L"Search for PATTERN in each FILE.\n"
    L"    --help              display this help message and exit\n"
    L"-H, --with-filename     print filenames with output lines\n"
    L"-h, --no-filename       suppress filenames on output lines\n"
    L"-n, --line-number       print line numbers with output lines\n";

uint32_t parse_options(int* argc, wchar_t** argv) {
    if (argv == NULL || *argc == 0) {
        return OPTION_INVALID;
    }
    FlagInfo flags[] = {
        {L'n', L"line-number", NULL},
        {L'H', L"with-filename", NULL},
        {L'h', L"no-filename", NULL},
        {L'\0', L"help", NULL},
        {L'r', L"recursive", NULL}
    };
    const uint64_t flag_count = sizeof(flags) / sizeof(FlagInfo);
    ErrorInfo errors;
    if (!find_flags(argv, argc, flags, flag_count, &errors)) {
        wchar_t *err_msg = format_error(&errors, flags, flag_count);
        _wprintf_e(L"%s\n", err_msg);
        _wprintf_e(L"Run '%s --help' for more information\n", argv[0]);
        return OPTION_INVALID;
    }

    if (flags[3].count > 0) {
        _wprintf(HELP_MESSAGE, argv[0]);
        return OPTION_HELP;
    }

    uint32_t opts = OPTION_VALID;

    if (flags[1].count > 0) {
        if (flags[2].count > 0) {
            if (flags[1].ord > flags[2].ord) {
                opts |= OPTION_LIST_NAMES;
            } 
        } else {
            opts |= OPTION_LIST_NAMES;
        }
    } else if (flags[2].count == 0) {
        if (*argc > 3 || flags[4].count > 0) {
            opts |= OPTION_LIST_NAMES;
        }
    }
    OPTION_IF(opts, flags[0].count > 0, OPTION_LINENUMBER);
    OPTION_IF(opts, flags[4].count > 0, OPTION_RECURSIVE);

    return opts;
}

void file_open_error(DWORD e, wchar_t* arg) {
    if (e == ERROR_FILE_NOT_FOUND || e == ERROR_BAD_ARGUMENTS ||
        e == ERROR_PATH_NOT_FOUND) {
        _wprintf_e(L"Cannot access '%s': No such file or directory\n",
            arg);
    } else if (e == ERROR_ACCESS_DENIED) {
        _wprintf_e(
            L"Cannot open '%s': Permission denied\n", arg);
    } else {
        wchar_t buf[256];
        if (FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM |
                           FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, e, 0, buf, 256, NULL)) {
            _wprintf_e(L"Cannot read '%s': %s", arg, buf);
        } else {
            _wprintf_e(L"Cannot read '%s'\n", arg);
        }
    }
}

typedef struct Line {
    uint64_t lineno;
    uint32_t len;
    uint8_t bytes[];
} Line;

typedef struct LineBuffer {
    uint32_t capacity;
    uint32_t size;
    Line* lines;
} LineBuffer;

bool LineBuffer_create(LineBuffer* b) {
    b->capacity = 64;
    b->size = 0;
    b->lines = Mem_alloc(64);
    if (b->lines == NULL) {
        b->capacity = 0;
        return false;
    }
    return true;
}

void LineBuffer_free(LineBuffer* b) {
    Mem_free(b->lines);
    b->size = 0;
    b->capacity = 0;
}

void LineBuffer_clear(LineBuffer* b) {
    b->size = 0;
}

bool LineBuffer_append(LineBuffer* b, uint64_t lineno, uint32_t len, const char* line) {
    uint64_t total_size = b->size;
    total_size += len + sizeof(uint32_t) + sizeof(uint64_t);
    if (total_size > b->capacity) {
        uint64_t new_cap = ((uint64_t)b->capacity) * 2;
        if (total_size > new_cap) {
            new_cap = total_size;
        }
        if (new_cap > 0xffffffff) {
            return false;
        }
        Line* newlines = Mem_realloc(b->lines, new_cap);
        if (newlines == NULL) {
            return false;
        }
        b->capacity = new_cap;
        b->lines = newlines;
    }
    Line* l = (Line*)(((uint8_t*) b->lines) + b->size);
    l->len = len;
    l->lineno = lineno;
    memcpy(l->bytes, line, len);
    b->size = total_size;
    return true;
}

Line* LineBuffer_get(LineBuffer* b, uint32_t offset, uint32_t* new_offset) {
    if (offset >= b->size) {
        return NULL;
    }
    Line* l = (Line*)(((uint8_t*) b->lines) + offset);
    *new_offset = offset + l->len + sizeof(uint32_t) + sizeof(uint64_t);
    return l;
}

void output_line(const char* line, uint32_t len, uint64_t lineno, uint32_t opts,
                 const wchar_t* name, WString* utf16_buf) {
    bool list_name = opts & OPTION_LIST_NAMES;
    bool list_lineno = opts & OPTION_LINENUMBER;

    if (WString_from_utf8_bytes(utf16_buf, line, len)) {
        if (list_name) {
            if (list_lineno) {
                _wprintf(L"%s:%llu:%s\n", name, lineno,
                         utf16_buf->buffer);
            } else {
                _wprintf(L"%s:%s\n", name, utf16_buf->buffer);
            }
        } else if (list_lineno) {
            _wprintf(L"%llu:%s\n", lineno, utf16_buf->buffer);
        } else {
            _wprintf(L"%s\n", utf16_buf->buffer);
        }
    }
}


bool match_file_flush(Regex* reg, LineCtx* ctx, next_line_fn_t next_line,
                      abort_fn_t abort, LineBuffer* line_buf, WString* utf16_buf,
                      const wchar_t* name, uint32_t opts) {
    uint64_t len;
    char* line;

    LineBuffer_clear(line_buf);

    uint64_t lineno = 0;
    while ((line = next_line(ctx, &len)) != NULL) {
        ++lineno;
        const char* match;
        uint64_t match_len;
        RegexAllCtx regctx;
        Regex_allmatch_init(reg, line, len, &regctx);
        while (Regex_allmatch(&regctx, &match, 
                    &match_len) == REGEX_MATCH) {
            uint32_t line_len = (uint32_t) len;
            output_line(line, line_len, lineno, opts, name, utf16_buf);
            break;
        }
    }
    return true;
}

bool match_file(Regex* reg, LineCtx* ctx, next_line_fn_t next_line,
                abort_fn_t abort, LineBuffer* line_buf, WString* utf16_buf,
                const wchar_t* name, uint32_t opts) {
    uint64_t len;
    char* line;

    LineBuffer_clear(line_buf);

    uint64_t lineno = 0;
    while ((line = next_line(ctx, &len)) != NULL) {
        ++lineno;
        const char* match;
        uint64_t match_len;
        RegexAllCtx regctx;
        Regex_allmatch_init(reg, line, len, &regctx);
        while (Regex_allmatch(&regctx, &match, 
                    &match_len) == REGEX_MATCH) {
            uint32_t line_len = (uint32_t) len;
            if (!LineBuffer_append(line_buf, lineno, len, line)) {
                abort(ctx);
                return false;
            }
            break;
        }
    }
    return true;
}

static const wchar_t* STDIN_STR = L"(standard input)";

bool start_iteration(LineCtx* ctx, wchar_t* arg, const wchar_t** name, next_line_fn_t* next_line,
                     abort_fn_t* abort, bool* flush) {
    if (arg[0] == L'-' && arg[1] == L'\0') {
        HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
        if (GetFileType(in) == FILE_TYPE_CHAR) {
            if (!ConsoleLineIter_begin(ctx, in)) {
                _wprintf_e(L"Failed reading from stdin\n");
                return false;
            }
            *next_line = ConsoleLineIter_next;
            *abort = ConsoleLineIter_abort;
            *flush = true;
        } else {
            if (!SyncLineIter_begin(ctx, in)) {
                wchar_t buf[256];
                DWORD e = GetLastError();
                if (FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM |
                                   FORMAT_MESSAGE_IGNORE_INSERTS,
                                   NULL, e, 0, buf, 256, NULL)) {
                    _wprintf_e(L"Failed reading from pipe: %s", buf);
                } else {
                    _wprintf_e(L"Cannot reading from pipe\n", arg);
                }
                return false;
            }
            *next_line = SyncLineIter_next;
            *abort = SyncLineIter_abort;
        }
        *name = STDIN_STR;
        return true;
    }

    if (is_directory(arg)) {
        _wprintf_e(L"Cannot read '%s': Is a directory\n", arg);
        return false;
    }
    if (!LineIter_begin(ctx, arg)) {
        DWORD e = GetLastError();
        file_open_error(e, arg);
        return false;
    }
    *next_line = LineIter_next;
    *abort = LineIter_abort;
    *name = arg;
    return true;
}

typedef enum MatchResult {
    MATCH_OK, MATCH_FAIL, MATCH_ABORT
} MatchResult;

MatchResult iterate_file(Regex* reg, wchar_t* file, WString *utf16_buf, LineBuffer* line_buf, uint32_t opts) {
    LineCtx ctx;
    next_line_fn_t next_line;
    abort_fn_t abort;
    bool flush = (opts & OPTION_FLUSH) != 0;
    const wchar_t* name;
    if (!start_iteration(&ctx, file, &name, &next_line, &abort, &flush)) {
        return MATCH_FAIL;
    }
    flush = true;
    if (opts & OPTION_NO_FLUSH) {
        flush = false;
    }
    if (flush) {
        if (!match_file_flush(reg, &ctx, next_line, abort, line_buf, utf16_buf, name, opts)) {
            return MATCH_ABORT;
        }
    } else {
        if (!match_file(reg, &ctx, next_line, abort, line_buf, utf16_buf, name, opts)) {
            return MATCH_ABORT;
        }
    }

    bool list_name = opts & OPTION_LIST_NAMES;
    bool list_lineno = opts & OPTION_LINENUMBER;

    Line* line;
    uint32_t offset = 0;
    while ((line = LineBuffer_get(line_buf, offset, &offset)) != NULL) {
        output_line((char*)line->bytes, line->len, line->lineno,
                    opts, name, utf16_buf);
    }

    return MATCH_OK;
}

MatchResult recurse_dir(Regex* reg, wchar_t* dir, uint32_t opts) {
    uint32_t name_offset = 0;
    wchar_t* filename = dir;
    if (dir == NULL) {
        name_offset = 2;
        filename = L".";
    }
    DWORD attr = GetFileAttributesW(filename);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        file_open_error(GetLastError(), filename);
        return MATCH_FAIL;
    }
    WString utf16_buf;
    if (!WString_create(&utf16_buf)) {
        return MATCH_ABORT;
    }
    LineBuffer line_buf;
    if (!LineBuffer_create(&line_buf)) {
        WString_free(&utf16_buf);
        return MATCH_ABORT;
    }
    if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        MatchResult success = iterate_file(reg, filename, &utf16_buf, &line_buf,
                                           opts);
        LineBuffer_free(&line_buf);
        WString_free(&utf16_buf);
        return success;
    }

    uint32_t stack_cap = 8;
    WalkCtx* stack = Mem_alloc(stack_cap * sizeof(WalkCtx));
    if (stack == NULL) {
        WString_free(&utf16_buf);
        return false;
    }
    MatchResult success = MATCH_OK;

    uint32_t stack_size = 1;
    WalkDir_begin(&stack[0], filename, true);
    Path* path;
    while (stack_size > 0) {
        if (WalkDir_next(&stack[stack_size - 1], &path)) {
            if (path->is_dir) {
                if (stack_size == stack_cap) {
                    stack_cap *= 2;
                    WalkCtx* new_stack = Mem_realloc(stack, 
                            stack_cap * sizeof(WalkCtx));
                    if (new_stack == NULL) {
                        _wprintf_e(L"out of memory\n");
                        success = MATCH_ABORT;
                        break;
                    }
                    stack = new_stack;
                }
                WalkDir_begin(&stack[stack_size], path->path.buffer, true);
                ++stack_size;
            } else {
                if (!iterate_file(reg, path->path.buffer + name_offset,
                                  &utf16_buf, &line_buf, opts)) {
                    success = MATCH_FAIL;
                }
            }
        } else {
            --stack_size;
        }
    }
    for (uint32_t ix = 0; ix < stack_size; ++ix) {
        WalkDir_abort(&stack[ix]);
    }
    Mem_free(stack);
    LineBuffer_free(&line_buf);
    WString_free(&utf16_buf);
    return success;
}

MatchResult recurse_files(Regex* reg, int argc, wchar_t** argv, uint32_t opts) {
    WalkCtx ctx;
    if (argc == 0) {
        return recurse_dir(reg, NULL, opts);
    }

    MatchResult status = MATCH_OK;
    for (int ix = 0; ix < argc; ++ix) {
        MatchResult res = recurse_dir(reg, argv[ix], opts);
        if (res == MATCH_ABORT) {
            return MATCH_ABORT;
        }
        if (res != MATCH_OK) {
            status = res;
        }
    }

    return status;
}

bool pattern_match(int argc, wchar_t** argv, uint32_t opts) {
    if (argc < 2) {
        if (argc > 0) {
            _wprintf_e(L"Usage: %s [OPTION]... PATTERN [FILE]...\n", argv[0]);
            _wprintf_e(L"Run '%s --help' for more information\n", argv[0]);
        }
        return false;
    }

    String s;
    if (!String_create(&s)) {
        _wprintf_e(L"Out of memory\n");
        return false;
    }
    if (!String_from_utf16_str(&s, argv[1])) {
        String_free(&s);
        _wprintf_e(L"Illegal pattern '%s'\n", argv[1]);
        return false;
    }
    Regex* reg = Regex_compile(s.buffer);
    String_free(&s);
    if (reg == NULL || reg->dfa == NULL) {
        _wprintf_e(L"Illegal pattern '%s'\n", argv[1]);
        return false;
    }

    if (opts & OPTION_RECURSIVE) {
        MatchResult status = recurse_files(reg, argc - 2, argv + 2, opts);
        Regex_free(reg);
        return status == MATCH_OK;
    }

    WString utf16_buf;
    if (!WString_create_capacity(&utf16_buf, 100)) {
        Regex_free(reg);
        _wprintf_e(L"Out of memory\n");
        return false;
    }
    LineBuffer line_buf;
    if (!LineBuffer_create(&line_buf)) {
        Regex_free(reg);
        WString_free(&utf16_buf);
        _wprintf_e(L"Out of memory\n");
        return false;
    }

    MatchResult status = MATCH_OK;

    if (argc < 3) {
        status = iterate_file(reg, L"-", &utf16_buf, &line_buf, opts);
    }

    for (int ix = 2; ix < argc; ++ix) {
        MatchResult res = iterate_file(reg, argv[ix], &utf16_buf, &line_buf, 
                                       opts);
        if (res != MATCH_OK) {
            status = res;
            if (res == MATCH_ABORT) {
                break;
            }
        }
    }

    LineBuffer_free(&line_buf);
    WString_free(&utf16_buf);
    Regex_free(reg);
    return status == MATCH_OK;
}

int main() {
    wchar_t* args = GetCommandLineW();
    int argc;
    wchar_t** argv = parse_command_line(args, &argc);

    uint32_t opts = parse_options(&argc, argv);
    if (opts == OPTION_INVALID) {
        Mem_free(argv);
        ExitProcess(1);
    }
    if (opts == OPTION_HELP) {
        Mem_free(argv);
        ExitProcess(0);
    }

    bool success = pattern_match(argc, argv, opts);
    Mem_free(argv);

    ExitProcess(success ? 0 : 1);
}
