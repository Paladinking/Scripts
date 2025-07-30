#include "dynamic_string.h"
#include "args.h"
#include "glob.h"
#include "printf.h"
#include "subprocess.h"


typedef struct Defered {
    String code;
    int64_t par;
} Defered;

typedef struct ParseCtx {
    String data;
    uint64_t ix;

    bool eof;
    bool err;
} ParseCtx;

bool write_to_file(const wchar_t* filename, String* buf) {
    HANDLE file =
        CreateFileW(filename, GENERIC_WRITE | DELETE, 0, NULL,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD w;
    uint32_t written = 0;
    while (written < buf->length) {
        if (!WriteFile(file, buf->buffer + written, buf->length - written, &w, NULL) || w == 0) {
            CloseHandle(file);
            return false;
        }
        written += w;
    }
    CloseHandle(file);
    return true;
}

bool should_abort(ParseCtx* ctx) {
    if (ctx->err) {
        return true;
    }
    if (ctx->eof) {
        return true;
    }
    return false;
}

bool skip_spaces(ParseCtx* ctx) {
    do {
        if (ctx->ix >= ctx->data.length) {
            ctx->eof = true;
            return false;
        }
        char c = ctx->data.buffer[ctx->ix];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            return true;
        }
        ctx->ix += 1;
    } while (1);
}

bool get_ident(ParseCtx* ctx, String* buf) {
    String_clear(buf);
    uint64_t ix = ctx->ix;
    do {
        if (ctx->ix >= ctx->data.length) {
            ctx->eof = true;
            return false;
        }
        char c = ctx->data.buffer[ctx->ix];
        if ((c < 'a' || c > 'z') && (c < 'A' || c > 'Z') && c != '_') {
            if (!String_append_count(buf, ctx->data.buffer + ix, 
                                     ctx->ix - ix)) {
                ctx->err = true;
                return false;
            }
            return true;
        }
        ctx->ix += 1;

    } while (1);
}

bool skip_literal(ParseCtx* ctx, char literal) {
    ctx->ix += 1;
    do {
        if (ctx->ix >= ctx->data.length) {
            ctx->eof = true;
            return false;
        }
        if (ctx->data.buffer[ctx->ix] == literal) {
            ctx->ix += 1;
            return true;
        } else if (ctx->data.buffer[ctx->ix] == '\\') {
            ctx->ix += 2;
        } else {
            ctx->ix += 1;
        }
    } while (1);
}

bool skip_paren(ParseCtx* ctx, char open, char close) {
    uint64_t paren_count = 1;
    ctx->ix += 1;
    while (1) {
        if (ctx->ix >= ctx->data.length) {
            ctx->eof = true;
            return false;
        }
        char c = ctx->data.buffer[ctx->ix];
        if (c == open) {
            paren_count += 1;
            ctx->ix += 1;
        } else if (c == close) {
            paren_count -= 1;
            ctx->ix += 1;
            if (paren_count == 0) {
                ctx->ix += 1;
                return true;
            }
        } else if (c == '"' || c == '\'') {
            if (!skip_literal(ctx, c) && should_abort(ctx)) {
                return false;
            }
        } else {
            ctx->ix += 1;
        }
    }
}

bool extract_defer(ParseCtx* ctx, String* buf) {
    if (!skip_spaces(ctx) && should_abort(ctx)) {
        return false;
    }
    String_clear(buf);
    uint64_t ix = ctx->ix;
    if (ctx->data.buffer[ix] == '{') {
        if (!skip_paren(ctx, '{', '}') && should_abort(ctx)) {
            return false;
        }
        if (!String_append_count(buf, ctx->data.buffer + ix, ctx->ix - ix)) {
            ctx->err = true;
            return false;
        }
        if (!String_append(buf, ' ')) {
            ctx->err = true;
            return false;
        }
    } else {
        while (1) {
            if (ctx->ix >= ctx->data.length) {
                ctx->eof = true;
                return false;
            }
            char c = ctx->data.buffer[ctx->ix];
            if (c == '"' || c == '\'') {
                if (!skip_literal(ctx, c) && should_abort(ctx)) {
                    return false;
                }
            } else if (c == '(') {
                if (!skip_paren(ctx, '(', ')') && should_abort(ctx)) {
                    return false;
                }
            } else if (c == ';') {
                ctx->ix += 1;
                uint64_t count = ctx->ix - ix;
                if (!String_reserve(buf, count + 5)) {
                    ctx->err = true;
                    return false;
                }
                String_append(buf, '{');
                String_append(buf, ' ');
                String_append_count(buf, ctx->data.buffer + ix, count);
                String_append(buf, ' ');
                String_append(buf, '}');
                String_append(buf, ' ');
                break;
            } else {
                ctx->ix += 1;
            }
        }
    }
    uint64_t diff = ctx->ix - ix;
    String_remove(&ctx->data, ix, diff);
    ctx->ix -= diff;
    return true;
}


bool get_defered(ParseCtx* ctx, Defered* defered,
                     uint64_t defered_count, int64_t edge,
                     String* buf, uint64_t* end_count) {
    String_clear(buf);
    uint64_t ix = defered_count - 1;
    while (defered_count > 0) {
        if (defered[ix].par <= edge) {
            if (end_count != NULL) {
                *end_count = ix + 1;
            }
            return true;
        }
        if (!String_append_count(buf, defered[ix].code.buffer, 
                                 defered[ix].code.length)) {
            ctx->err = true;
            return false;
        }
        if (ix == 0) {
            break;
        }
        --ix;
    }
    if (end_count != NULL) {
        *end_count = 0;
    }
    return true;
}

bool parse_func(ParseCtx* ctx, String* buf) {
    uint64_t open_count = 1;
    bool loop_start = false;
    Defered* deferd_code = Mem_alloc(4 * sizeof(Defered));
    uint64_t defered_count = 0;
    uint64_t defered_cap = 4;
    if (deferd_code == NULL) {
        ctx->err = true;
        return false;
    }

    uint64_t* loop_scope = Mem_alloc(4 * sizeof(uint64_t));
    uint64_t loop_count = 0;
    uint64_t loop_cap = 4;
    if (loop_scope == NULL) {
        Mem_free(deferd_code);
        ctx->err = true;
        return false;
    }

    while (1) {
        if (!skip_spaces(ctx) && should_abort(ctx)) {
            Mem_free(deferd_code);
            Mem_free(loop_scope);
            return false;
        }
        if (!get_ident(ctx, buf)) {
            Mem_free(deferd_code);
            Mem_free(loop_scope);
            return false;
        }
        if (buf->length == 0) {
            if (ctx->ix >= ctx->data.length) {
                ctx->eof = true;
                Mem_free(deferd_code);
                Mem_free(loop_scope);
                return false;
            }
            char c = ctx->data.buffer[ctx->ix];
            if (c == ';') {
                loop_start = false;
                ctx->ix += 1;
            } else if (c == '"' || c == '\'') {
                if (!skip_literal(ctx, c) && should_abort(ctx)) {
                    Mem_free(deferd_code);
                    Mem_free(loop_scope);
                    return false;
                }
            } else if (c == '(') {
                if (!skip_paren(ctx, '(', ')') && should_abort(ctx)) {
                    Mem_free(deferd_code);
                    Mem_free(loop_scope);
                    return false;
                }
            } else if (c == '{') {
                open_count += 1;
                if (loop_start) {
                    loop_start = false;
                    if (loop_count == loop_cap) {
                        loop_cap *= 2;
                        uint64_t* new_loop = 
                            Mem_realloc(loop_scope, loop_cap * sizeof(uint64_t));
                        if (new_loop == NULL) {
                            ctx->err = true;
                            Mem_free(loop_scope);
                            Mem_free(deferd_code);
                            return false;
                        }
                        loop_scope = new_loop;
                    }
                    loop_scope[loop_count] = open_count;
                    ++loop_count;
                }
                ctx->ix += 1;
            } else if (c == '}') {
                loop_start = false;
                open_count -= 1;
                while (loop_count > 0 && loop_scope[loop_count - 1] < open_count) {
                    --loop_count;
                }
                uint64_t new_size;
                if (!get_defered(ctx, deferd_code, defered_count, 
                                 open_count, buf, &new_size)) {
                    Mem_free(loop_scope);
                    Mem_free(deferd_code);
                    return false;
                }
                defered_count = new_size;
                if (!String_insert_count(&ctx->data, ctx->ix,
                            buf->buffer, buf->length)) {
                    ctx->err = true;
                    Mem_free(loop_scope);
                    Mem_free(deferd_code);
                    return false;
                }
                ctx->ix += buf->length + 1;
                if (open_count == 0) {
                    break;
                }
            } else {
                ctx->ix += 1;
            }
        } else {
            if (String_equals_str(buf, "for") || 
                String_equals_str(buf, "while")) {
                loop_start = true;
            } else if ((String_equals_str(buf, "break") ||
                       String_equals_str(buf, "continue")) && 
                       loop_count > 0
                    ) {
                uint64_t count = loop_scope[loop_count - 1];
                uint64_t ix = ctx->ix - buf->length;
                if (!get_defered(ctx, deferd_code, defered_count, 
                                 count - 1, buf, NULL)) {
                    Mem_free(loop_scope);
                    Mem_free(deferd_code);
                    return false;
                }
                if (!String_insert_count(&ctx->data, ix, buf->buffer, buf->length)) {
                    ctx->err = true;
                    Mem_free(loop_scope);
                    Mem_free(deferd_code);
                    return false;
                }
                ctx->ix += buf->length;
            } else if (String_equals_str(buf, "return")) {
                uint64_t ix = ctx->ix - 6;
                if (!get_defered(ctx, deferd_code, defered_count,
                     -1, buf, NULL)) {
                    Mem_free(loop_scope);
                    Mem_free(deferd_code);
                    return false;
                }
                if (!String_insert_count(&ctx->data, ix, buf->buffer, buf->length)) {
                    ctx->err = true;
                    Mem_free(loop_scope);
                    Mem_free(deferd_code);
                    return false;
                }
                ctx->ix += buf->length;
            } else if (String_equals_str(buf, "defer")) {
                String_remove(&ctx->data, ctx->ix - 6, 6);
                ctx->ix -= 6;
                uint64_t ix = ctx->ix;
                while ((ctx->data.buffer[ix] == ' ' ||
                       ctx->data.buffer[ix] == '\n' || 
                       ctx->data.buffer[ix] == '\r' ||
                       ctx->data.buffer[ix] == '\t') && ix > 0) {
                    --ix;
                }
                if (ctx->data.buffer[ix] != ')') {
                    if (!extract_defer(ctx, buf)) {
                        Mem_free(loop_scope);
                        Mem_free(deferd_code);
                        return false;
                    }
                    if (defered_count == defered_cap) {
                        defered_cap *= 2;
                        Defered* new = Mem_realloc(deferd_code, defered_cap * sizeof(Defered));
                        if (new == NULL) {
                            Mem_free(loop_scope);
                            Mem_free(deferd_code);
                            return false;
                        }
                        deferd_code = new;
                    }
                    String s;
                    if (!String_copy(&s, buf)) {
                        Mem_free(loop_scope);
                        Mem_free(deferd_code);
                        return false;
                    }
                    deferd_code[defered_count].code = s;
                    deferd_code[defered_count].par = open_count;
                    ++defered_count;
                }
            }
        }
    }
    Mem_free(loop_scope);
    Mem_free(deferd_code);
    return true;
}

bool func_start(ParseCtx* ctx, String* buf) {
    if (!skip_spaces(ctx) && should_abort(ctx)) {
        return false;
    }
    if (!get_ident(ctx, buf) && should_abort(ctx)) {
        return false;
    }
    if (buf->length == 0 || ctx->data.buffer[ctx->ix] != '(') {
        return false;
    }
    if (!skip_paren(ctx, '(', ')')) {
        return false;
    }
    if (!skip_spaces(ctx) && should_abort(ctx)) {
        return false;
    }
    if (ctx->ix >= ctx->data.length) {
        ctx->eof = true;
        return false;
    }
    if (ctx->data.buffer[ctx->ix] != '{') {
        return false;
    }
    ctx->ix += 1;
    return true;
}


bool parse(String* data) {
    ParseCtx ctx;
    String buf;
    if (!String_create(&buf)) {
        return false;
    }
    ctx.data = *data;
    ctx.ix = 0;
    ctx.err = false;
    ctx.eof = false;

    while (1) {
        if (func_start(&ctx, &buf)) {
            if (!parse_func(&ctx, &buf)) {
                String_free(&buf);
                *data = ctx.data;
                if (ctx.eof) {
                    return true;
                } else {
                    return false;
                }
            }
        } else {
            if (should_abort(&ctx)) {
                String_free(&buf);
                *data = ctx.data;
                return !ctx.err;
            }
            ctx.ix += 1;
        }
    }
} 


int main() {
    wchar_t* args = GetCommandLineW();
    int argc;
    wchar_t** argv = parse_command_line(args, &argc);

    int status = 1;

    if (argc < 2) {
        _wprintf_e(L"Missing argument\n");
        Mem_free(argv);
        return 1;
    }

    uint32_t cmp_ix = 0;
    wchar_t** cmp_args = Mem_alloc(argc * sizeof(wchar_t*));
    if (cmp_args == NULL) {
        goto end;
    }
    wchar_t *build_dir = NULL;
    for (uint32_t ix = 1; ix < argc; ++ix) {
        if (wcscmp(argv[ix], L"--defer-out") == 0) {
            if (ix + 1 >= argc) {
                _wprintf_e(L"Missing dir argument for '--defer-out\n'");
                goto end;
            }
            build_dir = argv[ix + 1];
            ++ix;
        }
    }
    if (build_dir == NULL) {
        build_dir = L".";
    } else {
        if (!is_directory(build_dir)) {
            if (!CreateDirectoryW(build_dir, NULL)) {
                _wprintf_e(L"Failed making direcory '%s'\n", build_dir);
                goto end;
            }
        }
    }

    for (uint32_t ix = 1; ix < argc; ++ix) {
        if (wcscmp(argv[ix], L"--defer-out") == 0) {
            ++ix;
        } else if (wcscmp(argv[ix], L"--defer-source") == 0) {
            if (ix + 1 >= argc) {
                _wprintf_e(L"Missing source argument for '--defer-source'\n");
                goto end;
            }

            wchar_t* filename = argv[ix + 1];
            String data;
            if (!read_text_file(&data, filename)) {
                _wprintf_e(L"Failed reading file '%s'\n", filename);
                goto end;
            }
            if (!parse(&data)) {
                String_free(&data);
                _wprintf_e(L"Failed parsing file '%s'\n", filename);
                goto end;
            }
            wchar_t* filesep = wcsrchr(filename, L'\\');
            wchar_t* filesep2 = wcsrchr(filename, L'/');
            if (filesep == NULL || filesep2 > filesep) {
                filesep = filesep2;
            }
            if (filesep == NULL) {
                filesep = filename;
            } else {
                filesep = filesep + 1;
            }
            WString name_buf;
            if (!WString_create(&name_buf)) {
                String_free(&data);
                goto end;
            }
            if (!WString_extend(&name_buf, build_dir) ||
                !WString_append(&name_buf, L'\\')) {
                String_free(&data);
                goto end;
            }
            wchar_t* ext = wcsrchr(filesep, L'.');
            if (ext == NULL) {
                if (!WString_extend(&name_buf, filesep) ||
                    !WString_extend(&name_buf, L"_precompiled") ||
                    !WString_append(&name_buf, L'\0')) {
                    String_free(&data);
                    goto end;

                }
            } else {
                if (!WString_append_count(&name_buf, filesep, ext - filesep) ||
                    !WString_extend(&name_buf, L"_precompiled") ||
                    !WString_extend(&name_buf, ext) ||
                    !WString_append(&name_buf, L'\0')) {
                    String_free(&data);
                    goto end;
                }
            }
            if (!write_to_file(name_buf.buffer, &data)) {
                _wprintf_e(L"Failed writing '%s'\n", name_buf.buffer);
                String_free(&data);
                goto end;
            }
            String_free(&data);
            cmp_args[cmp_ix] = name_buf.buffer;
            ++cmp_ix;
            ++ix;
        } else {
            cmp_args[cmp_ix] = argv[ix];
            ++cmp_ix;
        }
    }

    WString cmd;
    if (!WString_create(&cmd)) {
        goto end;
    }
    for (uint64_t ix = 0; ix < cmp_ix; ++ix) {
        if (!WString_extend(&cmd, cmp_args[ix]) ||
            !WString_append(&cmd, ' ')) {
            goto end;
        }
    }
    unsigned long exit_code;
    String out;
    String_create(&out);
    if (!subprocess_run(cmd.buffer, &out, 20 * 1000, &exit_code, 0)) {
        _wprintf_e(L"Failed running '%s'\n", cmd.buffer);
        goto end;
    }
    _wprintf(L"%S\n", out.buffer);
    String_free(&out);

    status = exit_code;
end:
    Mem_free(cmp_args);
    Mem_free(argv);
    return status;
}
