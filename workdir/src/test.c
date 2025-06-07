#define WIN32_LEAN_AND_MEAN
#define _NO_CRT_STDIO_INLINE
#include <windows.h>
#include <stdio.h>
#include "args.h"
#include "regex.h"
#include "glob.h"
#include "printf.h"
#include "mutex.h"

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
#define OPTION_WHOLELINE 512

#define OPTION_MATCH_MASK (OPTION_INVERSE | OPTION_WHOLELINE)

// TODO: console stdin
// TODO: -b, -L, -l, -c, --color, --exclude-dir, --include, --max-depth
// Maybe: -s, -z, --version, -U, --exclude-from, --line-buffered, --label

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
    L"-m, --max-count=N          stop after N matched lines\n"
    L"-n, --line-number          print line numbers with output lines\n"
    L"-r, --recursive            recurse into any directories\n"
    L"-v, --invert-match         select non-matching lines\n"
    L"-x, --line-regexp          only match full lines\n\n"
    L"Context control:\n"
    L"-A, --after-context=N      print N lines of trailing context\n"
    L"-B, --before-context=N     print N lines of leading context\n"
    L"-C, --context=N            print N lines of leading and trailing context\n";

uint32_t parse_options(int* argc, wchar_t** argv, uint32_t* before, uint32_t* after,
                       uint64_t* max_count) {
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
    FlagValue max = {FLAG_REQUIRED_VALUE | FLAG_UINT};

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
        {L'i', L"ignore-case", NULL},           // 12
        {L'x', L"line-regexp", NULL},           // 13
        {L'm', L"max-count", &max}              // 14
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

    if (flags[14].count > 0) {
        *max_count = max.uint;
    } else {
        *max_count = 0xffffffffffffffff;
    }

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
    OPTION_IF(opts, flags[13].count > 0, OPTION_WHOLELINE);

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

void LineBuffer_take(LineBuffer* a, LineBuffer* b) {
    LineBuffer tmp = *a;
    *a = *b;
    *b = tmp;
}

bool LineBuffer_copy(LineBuffer* a, LineBuffer* b) {
    if (a->capacity < b->size) {
        Line* new_lines = Mem_alloc(b->size);
        if (new_lines == NULL) {
            return false;
        }
        Mem_free(a->lines);
        a->size = b->size;
        a->capacity = b->size;
        a->lines = new_lines;
    } else {
        a->size = b->size;
    }
    memcpy(a->lines, b->lines, a->size);
    return true;
}

// TODO: This converts to utf16 and then back into utf8 if stdout is a file
bool output_line(const char* line, uint32_t len, uint64_t lineno, uint32_t opts,
                 const wchar_t* name, WString* utf16_buf, wchar_t sep) {
    bool list_name = opts & OPTION_LIST_NAMES;
    bool list_lineno = opts & OPTION_LINENUMBER;
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);

    bool success = true;
    if (GetFileType(out) == FILE_TYPE_CHAR) {
        WString_clear(utf16_buf);
        if (list_name) {
            if (list_lineno) {
                success = WString_format_append(utf16_buf, L"%s%c%llu%c", name, sep, lineno, sep);
            } else {
                success = WString_format_append(utf16_buf, L"%s%c", name, sep);
            }
        } else if (list_lineno) {
            success = WString_format_append(utf16_buf, L"%llu%c", lineno, sep);
        }

        if (success && WString_append_utf8_bytes(utf16_buf, line, len)) {
            utf16_buf->buffer[utf16_buf->length] = '\n';
            if (!WriteConsoleW(out, utf16_buf->buffer, utf16_buf->length + 1, NULL, NULL)) {
                success = false;
            }
        } else {
            success = false;
        }
    } else {
        String s;
        s.buffer = (char*)utf16_buf->buffer;
        s.capacity = utf16_buf->capacity * sizeof(wchar_t);
        s.length = 0;

        if (list_name) {
            if (list_lineno) {
                success = String_from_utf16_bytes(&s, name, wcslen(name));
                char nbuf[25];
                int len = _snprintf_s(nbuf, 25, 25, "%c%llu%c", sep, lineno, sep);
                success = success && String_append_count(&s, nbuf, len);
            } else {
                success = String_from_utf16_bytes(&s, name, wcslen(name));
                success = success && String_append(&s, sep);
            }
        } else {
            char nbuf[25];
            int len = _snprintf_s(nbuf, 25, 25, "%llu%c", lineno, sep);
            success = String_append_count(&s, nbuf, len);
        }
        if (success && String_append_count(&s, line, len)) {
            s.buffer[s.length] = '\n';
            DWORD w;
            uint32_t written = 0;
            while (written <= s.length) {
                if (!WriteFile(out, s.buffer + written, s.length - written + 1, &w, NULL)) {
                    success = false;
                    break;
                }
                written += w;
            }
        } else {
            success = false;
        }

        utf16_buf->buffer = (wchar_t*)s.buffer;
        utf16_buf->capacity = s.capacity / sizeof(wchar_t);
        utf16_buf->length = 0;
    }
    return success;
}

Condition* output_cond;

typedef struct OutputSlot {
    LineBuffer buffer;
    bool free;
    bool binary;
    bool failed;
    WString name;
} OutputSlot;

#define OUTPUT_SLOTS 16

OutputSlot output_queue[OUTPUT_SLOTS];

uint64_t current_ix;
bool output_abort; // true if threads should abort early
bool output_done; // true when all threads except the output_thread are done.

void submit_failure(uint64_t ix, WString* name) {
    WString_free(name);
    Condition_aquire(output_cond);
    if (output_abort) {
        Condition_release(output_cond);
        return;
    }
    while (ix - current_ix > OUTPUT_SLOTS ||
           !output_queue[ix - current_ix].free) {
        Condition_wait(output_cond, INFINITE);
        if (output_abort) {
            Condition_release(output_cond);
            return;
        }
    }

    OutputSlot* slot = output_queue + ix - current_ix;
    slot->free = false;
    slot->failed = true;

    Condition_notify_all(output_cond);

    Condition_release(output_cond);
}

bool submit_linebuffer(LineBuffer* buffer, uint64_t ix, WString* name, bool binary) {
    Condition_aquire(output_cond);

    if (output_abort) {
        Condition_release(output_cond);
        WString_free(name);
        return false;
    }

    while (ix - current_ix > OUTPUT_SLOTS ||
           !output_queue[ix - current_ix].free) {
        Condition_wait(output_cond, INFINITE);
        if (output_abort) {
            Condition_release(output_cond);
            WString_free(name);
            return false;
        }
    }

    OutputSlot* slot = output_queue + ix - current_ix;

    slot->free = false;
    if (!LineBuffer_copy(&slot->buffer, buffer)) {
        Condition_release(output_cond);
        WString_free(name);
        return false;
    }
    slot->name = *name;
    slot->binary = binary;

    Condition_notify_all(output_cond);

    Condition_release(output_cond);

    return true;
}

bool output_thread(uint32_t opts, uint64_t max_count, bool output_context) {
    LineBuffer slot;
    if (!LineBuffer_create(&slot)) {
        return false;
    }

    WString utf16_buf;
    if (!WString_create(&utf16_buf)) {
        LineBuffer_free(&slot);
        return false;
    }

    while (1) {
        Condition_aquire(output_cond);

        while (output_queue[0].free && !output_abort && !output_done) {
            Condition_wait(output_cond, INFINITE);
        }

        if (output_abort) {
            Condition_release(output_cond);
            LineBuffer_free(&slot);
            WString_free(&utf16_buf);
            return false;
        }
        if (output_queue[0].free) {
            Condition_release(output_cond);
            LineBuffer_free(&slot);
            WString_free(&utf16_buf);
            return true;
        }

        LineBuffer_take(&slot, &output_queue[0].buffer);
        WString name = output_queue[0].name;
        bool binary = output_queue[0].binary;
        bool failed = output_queue[0].failed;

        ++current_ix;
        LineBuffer first = output_queue[0].buffer;
        memmove(output_queue, output_queue + 1, (OUTPUT_SLOTS - 1)* sizeof(OutputSlot));
        output_queue[OUTPUT_SLOTS - 1].buffer = first;
        output_queue[OUTPUT_SLOTS - 1].free = true;
        Condition_notify_all(output_cond);

        Condition_release(output_cond);

        if (slot.lines == NULL || failed) {
            continue;
        }

        static uint64_t lineno = 0;
        Line* line;
        uint32_t offset = 0;

        if (binary && !(opts & OPTION_BINARY_TEXT)) {
            if (!(opts & OPTION_BINARY_NOMATCH)) {
                if (LineBuffer_get(&slot, offset, &offset) != NULL) {
                    _wprintf(L"Binary file %s matches\n", name.buffer);
                }
            }
        } else if (output_context) {
            while ((line = LineBuffer_get(&slot, offset, &offset)) != NULL) {
                if (max_count == 0) {
                    break;
                }
                max_count -= 1;
                if (line->lineno != lineno && lineno) {
                    outputw(L"--\n", 3);
                }
                lineno = line->lineno + 1;
                wchar_t sep = L"-:"[line->match];
                output_line((char*)line->bytes, line->len, line->lineno,
                            opts, name.buffer, &utf16_buf, sep);
            }
            lineno = 0xffffffffffffffff;
        } else {
            while ((line = LineBuffer_get(&slot, offset, &offset)) != NULL) {
                if (max_count == 0) {
                    break;
                }
                max_count -= 1;
                wchar_t sep = L"-:"[line->match];
                output_line((char*)line->bytes, line->len, line->lineno,
                            opts, name.buffer, &utf16_buf, sep);
            }
        }
        WString_free(&name);
        if (max_count == 0) {
            WString_free(&utf16_buf);
            LineBuffer_free(&slot);
            Condition_aquire(output_cond);
            output_abort = true;
            Condition_notify_all(output_cond);
            Condition_release(output_cond);
            return true;
        }
    }
}


RegexResult Regex_fullmatch_ctx(RegexAllCtx* ctx, const char**match, uint64_t* len) {
    if (ctx->str == NULL) {
        return REGEX_NO_MATCH;
    }
    RegexResult res = Regex_fullmatch(ctx->regex, (const char*)ctx->str,
                                      ctx->len);
    *match = (const char*)ctx->str;
    *len = ctx->len;
    ctx->str = NULL;
    return res;
}

#define REGEX_MATCH_NOT(fn) RegexResult fn##_not(RegexAllCtx* ctx, \
      const char **match, uint64_t* len) { \
    if (ctx->str == NULL) {                \
        return REGEX_NO_MATCH;             \
    }                                      \
    RegexResult res = fn(ctx, match, len); \
    if (res == REGEX_NO_MATCH) {           \
        *len = 0;                          \
        *match = (const char*)ctx->str;    \
        return REGEX_MATCH;                \
    } else if (res == REGEX_MATCH) {       \
        return REGEX_NO_MATCH;             \
    }                                      \
    return res; \
}

REGEX_MATCH_NOT(Regex_allmatch)
REGEX_MATCH_NOT(Regex_fullmatch_ctx)

bool match_file_context(RegexAllCtx* regctx, LineCtx* ctx,
                        next_line_fn_t next_line, abort_fn_t abort,
                        match_init_fn_t reg_init, match_fn_t reg_match,
                        LineBuffer* line_buf,
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
            if (before_count > 0) {
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
            } else if (context->before > 0 ){
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
    }
    return true;
}

bool match_file(RegexAllCtx* regctx, LineCtx* ctx, next_line_fn_t next_line, 
                abort_fn_t abort, match_init_fn_t reg_init,
                match_fn_t reg_match, LineBuffer* line_buf,
                uint32_t opts) {
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

void get_match_funcs(match_init_fn_t* init, match_fn_t* match, uint32_t opts) {
    *init = Regex_allmatch_init;
    switch (opts & OPTION_MATCH_MASK) {
    case (OPTION_WHOLELINE):
        *match = Regex_fullmatch_ctx;
        break;
    case (OPTION_INVERSE):
        *match = Regex_allmatch_not;
        break;
    case (OPTION_WHOLELINE | OPTION_INVERSE):
        *match = Regex_fullmatch_ctx_not;
        break;
    default:
        *match = Regex_allmatch;
        break;
    }
}

MatchResult iterate_file_async(Regex *reg, WString* name, LineBuffer* line_buf, uint32_t opts,
                               LineContext* line_context, uint64_t ix) {
    LineCtx ctx;
    next_line_fn_t next_line;
    abort_fn_t abort;
    const wchar_t* filename;
    if (!start_iteration(&ctx, name->buffer, &filename, &next_line, &abort)) {
        submit_failure(ix, name);
        return MATCH_FAIL;
    }
    if (name->buffer != filename) {
        WString_clear(name);
        if (!WString_extend(name, filename)) {
            WString_free(name);
            return MATCH_ABORT;
        }
    }

    match_fn_t reg_match;
    match_init_fn_t reg_init;
    get_match_funcs(&reg_init, &reg_match, opts);

    RegexAllCtx regctx;
    regctx.regex = reg;

    if (line_context != NULL) {
        if (!match_file_context(&regctx, &ctx, next_line, abort, reg_init,
                        reg_match, line_buf, opts, line_context)) {
            return MATCH_ABORT;
        }
    } else {
        if (!match_file(&regctx, &ctx, next_line, abort, reg_init, reg_match, line_buf, opts)) {
            return MATCH_ABORT;
        }
    }

    if (!submit_linebuffer(line_buf, ix, name, ctx.binary)) {
        return MATCH_ABORT;
    }

    return MATCH_OK;
} 

#define MAX_THREADS 8

typedef struct ThreadData {
    Regex* reg;                //
    uint32_t opts;             // 
    uint32_t before;           //
    uint32_t after;            // Only thread after creation
    LineBuffer line_buf;       //
    LineContext* line_context; //

    uint64_t ix;        // 
    WString name;       //
    MatchResult status; // Take thead_cond to read / write
    bool complete;      //

    bool created;  // Only main thread
    HANDLE handle; //
} ThreadData;

ThreadData thread_data[MAX_THREADS];
Condition* thread_cond;

enum {
    THREADS_RUNNING, THREADS_ABORT, THREADS_COMPLETE
} threads_status;

DWORD thread_entry(void* param) {
    ThreadData* data = param;

    Regex* reg = data->reg;
    uint32_t opts = data->opts;
    Condition_aquire(thread_cond);
    uint64_t ix = data->ix;
    WString name = data->name;
    Condition_release(thread_cond);

    while (1) {
        MatchResult res = iterate_file_async(reg, &name, &data->line_buf, opts, data->line_context, ix);

        Condition_aquire(thread_cond);
        data->status = res;
        data->complete = true;
        Condition_notify_all(thread_cond);

        while (threads_status == THREADS_RUNNING && data->complete) {
            Condition_wait(thread_cond, INFINITE);
        }
        if (threads_status == THREADS_ABORT ||
            (threads_status == THREADS_COMPLETE && data->complete)) {
            Condition_release(thread_cond);
            return 0;
        }

        ix = data->ix;
        name = data->name;

        Condition_release(thread_cond);
    }
}

typedef struct OutputParams {
    uint32_t opts;
    bool context;
    uint64_t max_count;
} OutputParams;


DWORD output_thread_entry(void *param) {
    OutputParams* o = param;
    uint32_t opts = o->opts;
    bool context = o->context;
    uint64_t max_count = o->max_count;
    Mem_free(o);

    Condition_aquire(output_cond);

    for (uint32_t i = 0; i < OUTPUT_SLOTS; ++i) {
        if (!LineBuffer_create(&output_queue[i].buffer)) {
            for (uint32_t j = 0; j < i; ++j) {
                LineBuffer_free(&output_queue[j].buffer);
            }
            output_abort = true;
            Condition_notify_all(output_cond);
            Condition_release(output_cond);
            return 1;
        }
        output_queue[i].free = true;
        output_queue[i].failed = false;
    }

    Condition_notify_all(output_cond);
    Condition_release(output_cond);

    bool res = output_thread(opts, max_count, context);
    Condition_aquire(output_cond);
    if (!res && !output_abort) {
        output_abort = true;
        Condition_notify_all(output_cond);
    }
    for (uint32_t i = 0; i < OUTPUT_SLOTS; ++i) {
        LineBuffer_free(&output_queue[i].buffer);
    }

    Condition_release(output_cond);
    return res ? 0 : 1;
}


HANDLE setup_threads(Regex* reg, uint32_t opts, uint32_t before, 
                     uint32_t after, uint64_t max_count) {
    output_cond = Condition_create();
    output_done = false;

    if (output_cond == NULL) {
        return INVALID_HANDLE_VALUE;
    }

    thread_cond = Condition_create();
    threads_status = THREADS_RUNNING;
    if (thread_cond == NULL) {
        Condition_free(output_cond);
        return INVALID_HANDLE_VALUE;
    }

    for (uint32_t i = 0; i < OUTPUT_SLOTS; ++i) {
        output_queue[i].free = false;
    }

    OutputParams* out_param = Mem_alloc(sizeof(OutputParams));
    if (out_param == NULL) {
        Condition_free(output_cond);
        Condition_free(thread_cond);
        return INVALID_HANDLE_VALUE;
    }
    out_param->context = before > 0 || after > 0;
    out_param->max_count = max_count;
    out_param->opts = opts;

    for (uint32_t ix = 0; ix < MAX_THREADS; ++ix) {
        thread_data[ix].reg = reg;
        thread_data[ix].created = false;
        thread_data[ix].opts = opts;
        thread_data[ix].before = before;
        thread_data[ix].after = after;
        thread_data[ix].complete = false;
        thread_data[ix].status = MATCH_OK;
    }

    HANDLE out_thread = CreateThread(NULL, 0, output_thread_entry, out_param, 0, 0);
    if (out_thread == NULL || out_thread == INVALID_HANDLE_VALUE) {
        Mem_free(out_param);
        Condition_free(output_cond);
        Condition_free(thread_cond);
        return INVALID_HANDLE_VALUE;
    }

    return out_thread;
}

MatchResult schedule_thread(uint64_t id, WString* name) {
    static uint32_t created_threads = 0;
    for (uint32_t ix = created_threads; ix < MAX_THREADS; ++ix) {
        if (!thread_data[ix].created) {
            if (!LineBuffer_create(&thread_data[ix].line_buf)) {
                WString_free(name);
                return MATCH_ABORT;
            }
            if (thread_data[ix].before > 0 || thread_data[ix].after > 0) {
                thread_data[ix].line_context = LineContext_create(thread_data[ix].before, thread_data[ix].after);
                if (thread_data[ix].line_context == NULL) {
                    LineBuffer_free(&thread_data[ix].line_buf);
                    WString_free(name);
                    return MATCH_ABORT;
                }
            }
            thread_data[ix].ix = id;
            thread_data[ix].name = *name;
            thread_data[ix].handle = CreateThread(NULL, 0, thread_entry, &thread_data[ix], 0, 0);
            if (thread_data[ix].handle == NULL || thread_data[ix].handle == INVALID_HANDLE_VALUE) {
                LineBuffer_free(&thread_data[ix].line_buf);
                LineContext_free(thread_data[ix].line_context);
                if (created_threads == 0) {
                    WString_free(name);
                    return MATCH_ABORT;
                }
                break;
            }
            ++created_threads;
            thread_data[ix].created = true;
            return MATCH_OK;
        }
    }

    Condition_aquire(thread_cond);
    MatchResult res;
    while (1) {
        for (uint32_t i = 0; i < created_threads; ++i) {
            if (thread_data[i].complete) {
                thread_data[i].name = *name;
                thread_data[i].ix = id;
                thread_data[i].complete = false;
                res = thread_data[i].status;
                Condition_notify_all(thread_cond);
                Condition_release(thread_cond);
                return res;
            }
        }
        Condition_wait(thread_cond, INFINITE);
    }
}

MatchResult wait_for_threads(MatchResult res, HANDLE out_thread) {
    Condition_aquire(thread_cond);
    if (res == MATCH_ABORT) {
        threads_status = THREADS_ABORT;
    } else {
        threads_status = THREADS_COMPLETE;
    }
    Condition_notify_all(thread_cond);
    Condition_release(thread_cond);
    for (uint32_t ix = 0; ix < MAX_THREADS; ++ix) {
        if (!thread_data[ix].created) {
            break;
        }
        WaitForSingleObject(thread_data[ix].handle, INFINITE);
        CloseHandle(thread_data[ix].handle);
        LineBuffer_free(&thread_data[ix].line_buf);
        LineContext_free(thread_data[ix].line_context);
        if (thread_data[ix].status != MATCH_OK) {
            if (res != MATCH_ABORT) {
                res = thread_data[ix].status;
                if (res == MATCH_ABORT) {
                    Condition_aquire(output_cond);
                    output_abort = true;
                    Condition_notify_all(output_cond);
                    Condition_release(output_cond);
                }
            }
        }
    }
    Condition_free(thread_cond);

    Condition_aquire(output_cond);
    if (!output_abort) {
        output_done = true;
        Condition_notify_all(output_cond);
    } else {
        res = MATCH_ABORT;
    }
    Condition_release(output_cond);
    WaitForSingleObject(out_thread, INFINITE);
    CloseHandle(out_thread);

    Condition_free(output_cond);

    return res;
}

MatchResult iterate_file(Regex* reg, wchar_t* file, WString *utf16_buf, LineBuffer* line_buf, uint32_t opts,
                         LineContext* line_context, uint64_t* max_count) {
    LineCtx ctx;
    next_line_fn_t next_line;
    abort_fn_t abort;
    const wchar_t* name;
    if (!start_iteration(&ctx, file, &name, &next_line, &abort)) {
        return MATCH_FAIL;
    }
    match_fn_t reg_match;
    match_init_fn_t reg_init;
    get_match_funcs(&reg_init, &reg_match, opts);

    RegexAllCtx regctx;
    regctx.regex = reg;

    if (line_context != NULL) {
        if (!match_file_context(&regctx, &ctx, next_line, abort, reg_init,
                        reg_match, line_buf, opts, line_context)) {
            return MATCH_ABORT;
        }
    } else {
        if (!match_file(&regctx, &ctx, next_line, abort, reg_init, reg_match, line_buf, opts)) {
            return MATCH_ABORT;
        }
    }

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
            if (*max_count == 0) {
                return MATCH_OK;
            }
            *max_count -= 1;
            if (line->lineno != lineno && lineno) {
                outputw(L"--\n", 3);
            }
            lineno = line->lineno + 1;
            wchar_t sep = L"-:"[line->match];
            output_line((char*)line->bytes, line->len, line->lineno,
                        opts, name, utf16_buf, sep);
        }
        lineno = 0xffffffffffffffff;
        return MATCH_OK;
    }

    while ((line = LineBuffer_get(line_buf, offset, &offset)) != NULL) {
        if (*max_count == 0) {
            return MATCH_OK;
        }
        *max_count -= 1;
        wchar_t sep = L"-:"[line->match];
        output_line((char*)line->bytes, line->len, line->lineno,
                    opts, name, utf16_buf, sep);
    }

    return MATCH_OK;
}

MatchResult recurse_dir(Regex* reg, wchar_t* dir, uint32_t opts,
                        uint32_t before, uint32_t after, uint64_t *ix) {
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

    if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        WString name;
        if (!WString_create_capacity(&name, wcslen(filename))) {
            return MATCH_ABORT;
        }
        WString_extend(&name, filename);
        MatchResult success = schedule_thread(*ix, &name);
        ++(*ix);
        return success;
    }

    uint32_t stack_cap = 8;
    WalkCtx* stack = Mem_alloc(stack_cap * sizeof(WalkCtx));
    if (stack == NULL) {
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
                        _wprintf_e(L"Out of memory\n");
                        success = MATCH_ABORT;
                        break;
                    }
                    stack = new_stack;
                }
                WalkDir_begin(&stack[stack_size], path->path.buffer, true);
                ++stack_size;
            } else {
                Condition_aquire(output_cond);
                if (output_abort) {
                    success = MATCH_ABORT;
                    break;
                }
                Condition_release(output_cond);

                WString name;
                if (!WString_create_capacity(&name, wcslen(path->path.buffer + name_offset))) {
                    success = MATCH_ABORT;
                    break;
                }
                HeapSize(GetProcessHeap(), 0, stack);
                WString_extend(&name, path->path.buffer + name_offset);
                MatchResult r = schedule_thread(*ix, &name);
                ++(*ix);
                if (r == MATCH_ABORT) {
                    success = MATCH_ABORT;
                    break;
                } else if (r == MATCH_FAIL) {
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
    return success;
}

MatchResult recurse_files(Regex* reg, int argc, wchar_t** argv, uint32_t opts,
                          uint32_t before, uint32_t after, uint64_t max_count) {

    HANDLE out_thread = setup_threads(reg, opts, before, after, max_count);
    if (out_thread == INVALID_HANDLE_VALUE) {
        return MATCH_ABORT;
    }
    
    uint64_t id = 0;

    if (argc == 0) {
        MatchResult res = recurse_dir(reg, NULL, opts, before, after, &id);
        if (res == MATCH_ABORT) {
            Condition_aquire(output_cond);
            output_abort = true;
            Condition_notify_all(output_cond);
            Condition_release(output_cond);
        }
        res = wait_for_threads(res, out_thread);
        return res;
    }

    MatchResult status = MATCH_OK;
    for (int ix = 0; ix < argc; ++ix) {
        MatchResult res = recurse_dir(reg, argv[ix], opts, before, after, &id);
        Condition_aquire(output_cond);
        if (output_abort) {
            res = MATCH_ABORT;
            Condition_release(output_cond);
            break;
        } else if (res == MATCH_ABORT) {
            output_abort = true;
            Condition_notify_all(output_cond);
            Condition_release(output_cond);
            break;
        }
        Condition_release(output_cond);
        if (res != MATCH_OK) {
            status = res;
        }
    }

    status = wait_for_threads(status, out_thread);

    return status;
}

bool pattern_match(int argc, wchar_t** argv, uint32_t opts, uint32_t before, uint32_t after, uint64_t max_count) {
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

    if (max_count == 0) {
        Regex_free(reg);
        return MATCH_OK;
    }

    if (opts & OPTION_RECURSIVE) {
        MatchResult status = recurse_files(reg, argc - 2, argv + 2,
                                           opts, before, after, max_count);
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
        status = iterate_file(reg, L"-", &utf16_buf, &line_buf, opts, line_ctx, &max_count);
    }

    for (int ix = 2; ix < argc; ++ix) {
        MatchResult res = iterate_file(reg, argv[ix], &utf16_buf, &line_buf, 
                                       opts, line_ctx, &max_count);
        if (res != MATCH_OK) {
            status = res;
            if (res == MATCH_ABORT) {
                break;
            }
        }
        if (max_count == 0) {
            break;
        }
    }

    LineContext_free(line_ctx);
    LineBuffer_free(&line_buf);
    WString_free(&utf16_buf);
    Regex_free(reg);
    return status == MATCH_OK;
}

#ifdef PENTER
extern void Trace_display();
#endif

int main() {
    wchar_t* args = GetCommandLineW();
    int argc;
    wchar_t** argv = parse_command_line(args, &argc);

    uint32_t before, after;
    uint64_t max_count;
    uint32_t opts = parse_options(&argc, argv, &before, &after, &max_count);
    if (opts == OPTION_INVALID) {
        Mem_free(argv);
        ExitProcess(1);
    }
    if (opts == OPTION_HELP) {
        Mem_free(argv);
        ExitProcess(0);
    }

    bool success = pattern_match(argc, argv, opts, before, after, max_count);
    Mem_free(argv);

#ifdef PENTER
    Trace_display();
#endif
    ExitProcess(success ? 0 : 1);
}
