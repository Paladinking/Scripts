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
#define OPTION_INVERSE 32
#define OPTION_IGNORE_CASE 64
#define OPTION_BINARY_TEXT 128
#define OPTION_BINARY_NOMATCH 256

// TODO: Use multiple threads
// TODO: -i, -x, -s, -m, -b, -L, -l, -c, --color, --exclude-dir, --include, --max-depth
// Maybe: -z, --version, -U, --exclude-from, --line-buffered, --label

#define OPTION_IF(opt, cond, flag)                                           \
    if (cond) {                                                              \
        opt |= flag;                                                         \
    }

typedef char* (*next_line_fn_t)(LineCtx*, uint64_t*);
typedef void (*abort_fn_t)(LineCtx*);
typedef void (*match_init_fn_t)(Regex*, const char*, uint64_t, RegexAllCtx*);
typedef RegexResult (*match_fn_t)(RegexAllCtx*, const char**, uint64_t*);

const wchar_t *HELP_MESSAGE =
    L"Usage: %s [OPTION]... PATTERN [FILE]...\n"
    L"Search for PATTERN in each FILE.\n"
    L"Miscellaneous:\n"
    L"-a, --text                 treat binary files as text, equivalent to\n"
    L"                           --binary-files=text\n"
    L"    --help                 display this help message and exit\n"
    L"    --binary-files=TYPE    treat binary files as TYPE;\n"
    L"                           TYPE is 'binary', 'text' or 'without-match'\n"
    L"-H, --with-filename        print filenames with output lines\n"
    L"-h, --no-filename          suppress filenames on output lines\n"
    L"-i, --ignore-case          ignore case distinctions\n"
    L"-I                         ignore binary files, equivalent to as\n"
    L"                           --binary-files=without-match\n"
    L"-n, --line-number          print line numbers with output lines\n"
    L"-r, --recursive            recurse into any directories\n"
    L"-v, --invert-match         select non-matching lines\n\n"
    L"Context control:\n"
    L"-A, --after-context=N      print N lines of trailing context\n"
    L"-B, --before-context=N     print N lines of leading context\n"
    L"-C, --context=N            print N lines of leading and trailing context\n";

uint32_t parse_options(int* argc, wchar_t** argv, uint32_t* before, uint32_t* after) {
    if (argv == NULL || *argc == 0) {
        return OPTION_INVALID;
    }

    const wchar_t *bin_binary[] = {L"binary"};
    const wchar_t *bin_text[] = {L"text"};
    const wchar_t *bin_nomatch[] = {L"without-match"};
    EnumValue bin_enum[] = {{bin_binary, 1}, {bin_text, 1}, {bin_nomatch, 1}};
    FlagValue bin_val = {FLAG_REQUIRED_VALUE | FLAG_ENUM, bin_enum, 3};

    FlagValue before_ctx = {FLAG_REQUIRED_VALUE | FLAG_UINT};
    FlagValue after_ctx = {FLAG_REQUIRED_VALUE | FLAG_UINT};
    FlagValue full_ctx = {FLAG_REQUIRED_VALUE | FLAG_UINT};

    FlagInfo flags[] = {
        {L'n', L"line-number", NULL},           // 0
        {L'H', L"with-filename", NULL},         // 1
        {L'h', L"no-filename", NULL},           // 2
        {L'\0', L"help", NULL},                 // 3
        {L'r', L"recursive", NULL},             // 4
        {L'I', NULL, NULL},                     // 5
        {L'a', L"text", NULL},                  // 6
        {L'\0', L"binary-files", &bin_val},     // 7
        {L'v', L"invert-match", NULL},          // 8
        {L'A', L"after-context", &after_ctx},   // 9
        {L'B', L"before-context", &before_ctx}, // 10
        {L'C', L"context", &full_ctx},          // 11
        {L'i', L"ignore-case", NULL}            // 12
    };
    const uint64_t flag_count = sizeof(flags) / sizeof(FlagInfo);
    ErrorInfo errors;
    if (!find_flags(argv, argc, flags, flag_count, &errors)) {
        wchar_t *err_msg = format_error(&errors, flags, flag_count);
        if (err_msg) {
            _wprintf_e(L"%s\n", err_msg);
            _wprintf_e(L"Run '%s --help' for more information\n", argv[0]);
            Mem_free(err_msg);
            return OPTION_INVALID;
        }
    }

    if (flags[3].count > 0) {
        _wprintf(HELP_MESSAGE, argv[0]);
        return OPTION_HELP;
    }

    uint32_t opts = OPTION_VALID;

    uint64_t before_val, after_val;
    if (flags[9].ord > flags[11].ord) {
        after_val = after_ctx.uint;
    } else if (flags[11].count > 0) {
        after_val = full_ctx.uint;
    } else {
        after_val = 0;
    }

    if (flags[10].ord > flags[11].ord) {
        before_val = before_ctx.uint;
    } else if (flags[11].count > 0) {
        before_val = full_ctx.uint;
    } else {
        before_val = 0;
    }
    if (after_val + before_val > 65536) {
        _wprintf_e(L"Context length too big\n");
        return OPTION_INVALID;
    }
    *before = before_val;
    *after = after_val;

    if (flags[7].ord > flags[6].ord && flags[7].ord > flags[5].ord) {
        if (bin_val.uint == 1) {
            opts |= OPTION_BINARY_TEXT;
        } else if (bin_val.uint == 2) {
            opts |= OPTION_BINARY_NOMATCH;
        }
    } else if (flags[6].ord > flags[7].ord && flags[6].ord > flags[5].ord) {
        opts |= OPTION_BINARY_TEXT;
    } else if (flags[5].ord > flags[7].ord && flags[5].ord > flags[6].ord) {
        opts |= OPTION_BINARY_NOMATCH;
    }

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
    OPTION_IF(opts, flags[8].count > 0, OPTION_INVERSE);
    OPTION_IF(opts, flags[12].count > 0, OPTION_IGNORE_CASE);

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
    uint32_t match; // 0 or 1, uint32_t for alignment of bytes
    uint8_t bytes[];
} Line;

typedef struct LineContext {
    uint32_t before;
    uint32_t after;
    String before_lines[];
} LineContext;

typedef struct LineBuffer {
    uint32_t capacity;
    uint32_t size;
    Line* lines;
} LineBuffer;

LineContext* LineContext_create(uint32_t before, uint32_t after) {
    LineContext* ptr = Mem_alloc(3 * sizeof(uint32_t) + before * sizeof(String));
    if (ptr != NULL) {
        ptr->before = before;
        ptr->after = after;
        for (uint32_t ix = 0; ix < before; ++ix) {
            if (!String_create(&ptr->before_lines[ix])) {
                for (uint32_t i = 0; i < ix; ++i) {
                    String_free(&ptr->before_lines[i]);
                }
                Mem_free(ptr);
                return NULL;
            }
        }
    }
    return ptr;
}

void LineContext_free(LineContext* ptr) {
    if (ptr == NULL) {
        return;
    }
    for (uint32_t ix = 0; ix < ptr->before; ++ix) {
        String_free(&ptr->before_lines[ix]);
    }
    Mem_free(ptr);
}

bool LineBuffer_create(LineBuffer* b) {
    LineContext c;
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

bool LineBuffer_append(LineBuffer* b, uint64_t lineno, uint32_t len, const char* line, uint32_t match) {
    uint64_t total_size = b->size;
    total_size += len + 2 * sizeof(uint32_t) + sizeof(uint64_t);
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
    l->match = match;
    memcpy(l->bytes, line, len);
    b->size = total_size;
    return true;
}

Line* LineBuffer_get(LineBuffer* b, uint32_t offset, uint32_t* new_offset) {
    if (offset >= b->size) {
        return NULL;
    }
    Line* l = (Line*)(((uint8_t*) b->lines) + offset);
    *new_offset = offset + l->len + 2 * sizeof(uint32_t) + sizeof(uint64_t);
    return l;
}

// TODO: This converts to utf16 and then back into utf8 if stdout is a file
void output_line(const char* line, uint32_t len, uint64_t lineno, uint32_t opts,
                 const wchar_t* name, WString* utf16_buf, wchar_t sep) {
    bool list_name = opts & OPTION_LIST_NAMES;
    bool list_lineno = opts & OPTION_LINENUMBER;
    WString_clear(utf16_buf);
    if (list_name) {
        if (list_lineno) {
            WString_format_append(utf16_buf, L"%s%c%llu%c", name, sep, lineno, sep);
        } else {
            WString_format_append(utf16_buf, L"%s%c", name, sep);
        }
    } else if (list_lineno) {
        WString_format_append(utf16_buf, L"%llu%c", lineno, sep);
    }

    if (WString_append_utf8_bytes(utf16_buf, line, len)) {
        utf16_buf->buffer[utf16_buf->length] = '\n';
        outputw(utf16_buf->buffer, utf16_buf->length + 1);
    }
}

RegexResult Regex_allmatch_nocase_not(RegexAllCtx* ctx, const char **match, uint64_t* len) {
    if (ctx->str == NULL) {
        return REGEX_NO_MATCH;
    }
    RegexResult res = Regex_allmatch_nocase(ctx, match, len);
    if (res == REGEX_NO_MATCH) {
        // Empty match for '--color'
        *len = 0;
        *match = (const char*)ctx->str;
        ctx->str = NULL;
        return REGEX_MATCH;
    } else if (res == REGEX_MATCH) {
        return REGEX_NO_MATCH;
    }
    return res;

}

// Match function that matches once if no match exists.
RegexResult Regex_allmatch_not(RegexAllCtx *ctx, const char **match, uint64_t *len) {
    if (ctx->str == NULL) {
        return REGEX_NO_MATCH;
    }
    RegexResult res = Regex_allmatch(ctx, match, len);
    if (res == REGEX_NO_MATCH) {
        // Empty match for '--color'
        *len = 0;
        *match = (const char*)ctx->str;
        ctx->str = NULL;
        return REGEX_MATCH;
    } else if (res == REGEX_MATCH) {
        return REGEX_NO_MATCH;
    }
    return res;
}

bool match_file_context(RegexAllCtx* regctx, LineCtx* ctx,
                        next_line_fn_t next_line, abort_fn_t abort,
                        match_init_fn_t reg_init, match_fn_t reg_match,
                        LineBuffer* line_buf, const wchar_t* name,
                        uint32_t opts, LineContext* context) {
    uint64_t len;
    char* line;
    uint32_t before_count = 0;
    uint32_t before_ix = 0;

    LineBuffer_clear(line_buf);

    uint64_t lineno = 0;
    uint64_t after_matches = 0;
    while ((line = next_line(ctx, &len)) != NULL) {
        ++lineno;
        const char* match;
        uint64_t match_len;
        reg_init(regctx->regex, line, len, regctx);
        if (reg_match(regctx, &match, &match_len) == REGEX_MATCH) {
            uint64_t before_lno = lineno - before_count;
            before_ix = (before_count * (before_ix + context->before - 1)) % context->before;
            while (before_count > 0) {
                String* s = &context->before_lines[before_ix];
                before_ix = (before_ix + 1) % context->before;
                if (!LineBuffer_append(line_buf, before_lno, s->length, s->buffer, 0)) {
                    abort(ctx);
                    return false;
                }
                ++before_lno;
                --before_count;
            }

            uint32_t line_len = (uint32_t) len;
            if (!LineBuffer_append(line_buf, lineno, len, line, 1)) {
                abort(ctx);
                return false;
            }
            after_matches = context->after;
        } else {
            if (after_matches > 0) {
                --after_matches;
                if (!LineBuffer_append(line_buf, lineno, len, line, 0)) {
                    abort(ctx);
                    return false;
                }
                continue;
            }
            before_ix = (before_ix + 1) % context->before;
            String_clear(&context->before_lines[before_ix]);
            if (!String_append_count(&context->before_lines[before_ix], line, len)) {
                abort(ctx);
                return false;
            }
            if (before_count < context->before) {
                ++before_count;
            }
        }
    }
    return true;
}

bool match_file(RegexAllCtx* regctx, LineCtx* ctx, next_line_fn_t next_line, 
                abort_fn_t abort, match_init_fn_t reg_init,
                match_fn_t reg_match, LineBuffer* line_buf,
                const wchar_t* name, uint32_t opts) {
    uint64_t len;
    char* line;

    LineBuffer_clear(line_buf);

    uint64_t lineno = 0;
    while ((line = next_line(ctx, &len)) != NULL) {
        ++lineno;
        const char* match;
        uint64_t match_len;
        reg_init(regctx->regex, line, len, regctx);
        while (reg_match(regctx, &match, &match_len) == REGEX_MATCH) {
            uint32_t line_len = (uint32_t) len;
            if (!LineBuffer_append(line_buf, lineno, len, line, 1)) {
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
                     abort_fn_t* abort) {
    if (arg[0] == L'-' && arg[1] == L'\0') {
        HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
        if (GetFileType(in) == FILE_TYPE_CHAR) {
            if (!ConsoleLineIter_begin(ctx, in)) {
                _wprintf_e(L"Failed reading from stdin\n");
                return false;
            }
            *next_line = ConsoleLineIter_next;
            *abort = ConsoleLineIter_abort;
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

MatchResult iterate_file(Regex* reg, wchar_t* file, WString *utf16_buf, LineBuffer* line_buf, uint32_t opts,
                         LineContext* line_context) {
    LineCtx ctx;
    next_line_fn_t next_line;
    abort_fn_t abort;
    const wchar_t* name;
    if (!start_iteration(&ctx, file, &name, &next_line, &abort)) {
        return MATCH_FAIL;
    }
    match_fn_t reg_match = Regex_allmatch;
    match_init_fn_t reg_init = Regex_allmatch_init;
    RegexAllCtx regctx;
    regctx.regex = reg;
    if (opts & OPTION_IGNORE_CASE) {
        regctx.buffer = Mem_alloc(128);
        if (regctx.buffer == NULL) {
            abort(&ctx);
            return MATCH_ABORT;
        }
        regctx.buffer_cap = 128;
        reg_init = Regex_allmatch_init_nocase;
        if (opts & OPTION_INVERSE) {
            reg_match = Regex_allmatch_nocase_not;
        } else {
            reg_match = Regex_allmatch_nocase;
        }
    } else if (opts & OPTION_INVERSE) {
        reg_match = Regex_allmatch_not;
    }


    if (line_context != NULL) {
        if (!match_file_context(&regctx, &ctx, next_line, abort, reg_init,
                        reg_match, line_buf, name, opts, line_context)) {
            return MATCH_ABORT;
        }
    } else {
        if (!match_file(&regctx, &ctx, next_line, abort, reg_init, reg_match, line_buf, name, opts)) {
            return MATCH_ABORT;
        }
    }

    if (opts & OPTION_IGNORE_CASE) {
        Mem_free(regctx.buffer);
    }

    bool list_name = opts & OPTION_LIST_NAMES;
    bool list_lineno = opts & OPTION_LINENUMBER;
    Line* line;
    uint32_t offset = 0;

    if (ctx.binary && !(opts & OPTION_BINARY_TEXT)) {
        if (!(opts & OPTION_BINARY_NOMATCH)) {
            if (LineBuffer_get(line_buf, offset, &offset) != NULL) {
                _wprintf(L"Binary file %s matches\n", name);
            }
        }
        return MATCH_OK;
    }

    // TODO: this is not threadsafe
    static uint64_t lineno = 0;
    if (line_context != NULL) {
        while ((line = LineBuffer_get(line_buf, offset, &offset)) != NULL) {
            if (line->lineno != lineno && lineno) {
                outputw(L"--\n", 3);
            }
            lineno = line->lineno + 1;
            wchar_t sep = L"-:"[line->match];
            output_line((char*)line->bytes, line->len, line->lineno,
                        opts, name, utf16_buf, sep);
        }
        return MATCH_OK;
    }
    lineno = 0xffffffffffffffff;

    while ((line = LineBuffer_get(line_buf, offset, &offset)) != NULL) {
        wchar_t sep = L"-:"[line->match];
        output_line((char*)line->bytes, line->len, line->lineno,
                    opts, name, utf16_buf, sep);
    }

    return MATCH_OK;
}

MatchResult recurse_dir(Regex* reg, wchar_t* dir, uint32_t opts,
                        uint32_t before, uint32_t after) {
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
    LineContext* context = NULL;
    if (before > 0 || after > 0) {
        context = LineContext_create(before, after);
        if (context == NULL) {
            LineBuffer_free(&line_buf);
            WString_free(&utf16_buf);
            return MATCH_ABORT;
        }
    }

    if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        MatchResult success = iterate_file(reg, filename, &utf16_buf, &line_buf,
                                           opts, context);
        LineContext_free(context);
        LineBuffer_free(&line_buf);
        WString_free(&utf16_buf);
        return success;
    }

    uint32_t stack_cap = 8;
    WalkCtx* stack = Mem_alloc(stack_cap * sizeof(WalkCtx));
    if (stack == NULL) {
        LineContext_free(context);
        LineBuffer_free(&line_buf);
        WString_free(&utf16_buf);
        return MATCH_ABORT;
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
                                  &utf16_buf, &line_buf, opts, context)) {
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
    LineContext_free(context);
    Mem_free(stack);
    LineBuffer_free(&line_buf);
    WString_free(&utf16_buf);
    return success;
}

MatchResult recurse_files(Regex* reg, int argc, wchar_t** argv, uint32_t opts,
                          uint32_t before, uint32_t after) {
    WalkCtx ctx;
    if (argc == 0) {
        return recurse_dir(reg, NULL, opts, before, after);
    }

    MatchResult status = MATCH_OK;
    for (int ix = 0; ix < argc; ++ix) {
        MatchResult res = recurse_dir(reg, argv[ix], opts, before, after);
        if (res == MATCH_ABORT) {
            return MATCH_ABORT;
        }
        if (res != MATCH_OK) {
            status = res;
        }
    }

    return status;
}

bool pattern_match(int argc, wchar_t** argv, uint32_t opts, uint32_t before, uint32_t after) {
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
    Regex* reg = Regex_compile_with(s.buffer, opts & OPTION_IGNORE_CASE);
    String_free(&s);
    if (reg == NULL || reg->dfa == NULL) {
        _wprintf_e(L"Illegal pattern '%s'\n", argv[1]);
        return false;
    }

    if (opts & OPTION_RECURSIVE) {
        MatchResult status = recurse_files(reg, argc - 2, argv + 2, opts, before, after);
        Regex_free(reg);
        return status == MATCH_OK;
    }


    LineContext* line_ctx = NULL;
    if (before > 0 || after > 0) {
        line_ctx = LineContext_create(before, after);
        if (line_ctx == NULL) {
            _wprintf_e(L"Out of memory\n");
            Regex_free(reg);
            return false;
        }
    }

    WString utf16_buf;
    if (!WString_create_capacity(&utf16_buf, 100)) {
        LineContext_free(line_ctx);
        Regex_free(reg);
        _wprintf_e(L"Out of memory\n");
        return false;
    }
    LineBuffer line_buf;
    if (!LineBuffer_create(&line_buf)) {
        LineContext_free(line_ctx);
        Regex_free(reg);
        WString_free(&utf16_buf);
        _wprintf_e(L"Out of memory\n");
        return false;
    }

    MatchResult status = MATCH_OK;

    if (argc < 3) {
        status = iterate_file(reg, L"-", &utf16_buf, &line_buf, opts, line_ctx);
    }

    for (int ix = 2; ix < argc; ++ix) {
        MatchResult res = iterate_file(reg, argv[ix], &utf16_buf, &line_buf, 
                                       opts, line_ctx);
        if (res != MATCH_OK) {
            status = res;
            if (res == MATCH_ABORT) {
                break;
            }
        }
    }

    LineContext_free(line_ctx);
    LineBuffer_free(&line_buf);
    WString_free(&utf16_buf);
    Regex_free(reg);
    return status == MATCH_OK;
}

int main() {
    wchar_t* args = GetCommandLineW();
    int argc;
    wchar_t** argv = parse_command_line(args, &argc);

    uint32_t before, after;
    uint32_t opts = parse_options(&argc, argv, &before, &after);
    if (opts == OPTION_INVALID) {
        Mem_free(argv);
        ExitProcess(1);
    }
    if (opts == OPTION_HELP) {
        Mem_free(argv);
        ExitProcess(0);
    }

    bool success = pattern_match(argc, argv, opts, before, after);
    Mem_free(argv);

    ExitProcess(success ? 0 : 1);
}
