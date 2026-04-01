#include "args.h"
#include "printf.h"
#include "glob.h"
#include "subprocess.h"
#include "path_utils.h"

WString pathext;
WString pathbuf;
WString workdir;

const wchar_t* find_program(const wchar_t* cmd, WString* dest) {
    unsigned ix = 0;
    bool in_quote = false;
    WString prog;
    if (!WString_create(&prog)) {
        return NULL;
    }
    WString_clear(dest);
    while (1) {
        if (cmd[ix] == L'\0' || (cmd[ix] == L' ' && !in_quote)) {
            break;
        }
        if (cmd[ix] == L'"') {
            in_quote = !in_quote;
        } else {
            if (!WString_append(&prog, cmd[ix])) {
                WString_free(&prog);
                return NULL;
            }
        }
        ++ix;
    }

    wchar_t *ext = pathext.buffer;
    DWORD count = 0;
    do {
        wchar_t* sep = wcschr(ext, L';');
        if (sep == NULL) {
            sep = pathext.buffer + pathext.length;
        }
        do {
            if (!WString_reserve(dest, count)) {
                WString_free(&prog);
                return NULL;
            }
            wchar_t old_sep = *sep;
            *sep = L'\0';
            count = SearchPathW(workdir.buffer,
                                prog.buffer, ext, dest->capacity,
                                dest->buffer, NULL);
            *sep = old_sep;
        } while (count > dest->capacity);
        dest->length = count;
        dest->buffer[dest->length] = L'\0';
        if (count > 0) {
            WString_free(&prog);
            return dest->buffer;
        }
        ext = sep + 1;
    } while (ext < pathext.buffer + pathext.length);
    ext = pathext.buffer;
    count = 0;
    do {
        wchar_t* sep = wcschr(ext, L';');
        if (sep == NULL) {
            sep = pathext.buffer + pathext.length;
        }
        *sep = L'\0';
        do {
            if (!WString_reserve(dest, count)) {
                WString_free(&prog);
                return NULL;
            }
            count = SearchPathW(pathbuf.buffer,
                                prog.buffer, ext, dest->capacity,
                                dest->buffer, NULL);
        } while (count > dest->capacity);
        dest->length = count;
        dest->buffer[dest->length] = L'\0';
        if (count > 0) {
            WString_free(&prog);
            return dest->buffer;
        }
        *sep = L';';
        ext = sep + 1;
    } while (ext < pathext.buffer + pathext.length);
    WString_free(&prog);
    return NULL;
}

int run(wchar_t** argv, wchar_t* str) {
    size_t i = 0;
    size_t pos = 0;
    size_t len = 0;
    bool quoted;
    WString cmd, prog;
    if (!WString_create(&cmd)) {
        return 1;
    }
    if (!WString_create(&prog)) {
        WString_free(&cmd);
        return 1;
    }
    unsigned long exit_code = 1;

    unsigned opts = SUBPROCESS_STDIN_INHERIT | SUBPROCESS_STDOUT_INHERIT;
    while (get_arg_len(str, &i, &len, &quoted, ARG_OPTION_STD | ARG_OPTION_TERMINAL_OPERANDS)) {
        if (!WString_reserve(&cmd,  cmd.length + 3 + len)) {
            goto end;
        }
        if (cmd.length > 0) {
            WString_append(&cmd, ' ');
        }
        if (quoted) {
            WString_append(&cmd, '"');
        }
        get_arg(str, &pos, cmd.buffer + cmd.length, ARG_OPTION_STD | ARG_OPTION_TERMINAL_OPERANDS);
        if (!quoted && (cmd.buffer[cmd.length] == L'&' || cmd.buffer[cmd.length] == L'|')) {
            if (cmd.buffer[cmd.length] == L'|') {
                _wprintf_e(L"[%s]: Cannot handle '|'\n", argv[0]);
                goto end;
            }
            const bool short_and = cmd.buffer[cmd.length + 1] == L'&';
            cmd.buffer[cmd.length] = L'\0';
            if (cmd.length > 0) {
                WString_pop(&cmd, 1);
            }
            const wchar_t* program = find_program(cmd.buffer, &prog);
            unsigned long code;
            if (program == NULL || 
                !subprocess_run_program(program, cmd.buffer, NULL, INFINITE, &code, opts)) {
                _wprintf_e(L"[%s]: Failed to run '%s'\n", argv[0], cmd.buffer);
                goto end;
            }
            FlushFileBuffers(GetStdHandle(STD_OUTPUT_HANDLE));
            FlushFileBuffers(GetStdHandle(STD_ERROR_HANDLE));
            if (short_and && code != 0) {
                exit_code = code;
                goto end;
            }
            WString_clear(&cmd);
        } else {
            cmd.length += len;
            if (quoted) {
                WString_append(&cmd, '"');
            }
        }
    }
    const wchar_t* program = find_program(cmd.buffer, &prog);
    if (program == NULL ||
        !subprocess_run_program(program, cmd.buffer, NULL, INFINITE, &exit_code, opts)) {
        _wprintf_e(L"[%s]: Failed to run '%s'\n", argv[0], cmd.buffer);
        exit_code = 1;
    }
end:
    WString_free(&cmd);
    WString_free(&prog);
    return exit_code;
}


int main() {
    if (!WString_create(&pathext)) {
        ExitProcess(1);
    }
    if (!WString_create(&pathbuf)) {
        WString_free(&pathext);
        ExitProcess(1);
    }
    if (!WString_create(&workdir)) {
        WString_free(&pathext);
        WString_free(&pathbuf);
        ExitProcess(1);
    }

    int status = 1;
    if (!get_envvar(L"PATHEXT", 0, &pathext) || pathext.length == 0) {
        WString_clear(&pathext);
        if (!WString_extend(&pathext, L".EXE;.BAT")) {
            goto end;
        }
    }
    if (!get_envvar(L"PATH", 0, &pathbuf)) {
        goto end;
    }
    if (!get_workdir(&workdir)) {
        goto end;
    }
    
    const wchar_t* args = GetCommandLineW();
    int argc;
    wchar_t** argv = parse_command_line(args, &argc);

    FlagValue command = {FLAG_STRING};
    FlagInfo flags[] = {
        {'c', NULL, &command}
    };
    ErrorInfo err;
    if (!find_flags(argv, &argc, flags, 1, &err)) {
        wchar_t* msg = format_error(&err, flags, 1);
        if (msg != NULL) {
            _wprintf_e(L"%s\n", msg);
        }
        goto end;
    }
    if (flags[0].count == 0) {
        status = 0;
        goto end;
    }
    status = run(argv, command.str);

    FlushFileBuffers(GetStdHandle(STD_OUTPUT_HANDLE));
    FlushFileBuffers(GetStdHandle(STD_ERROR_HANDLE));
end:
    WString_free(&pathbuf);
    WString_free(&pathext);
    WString_free(&workdir);
    Mem_free(argv);
    ExitProcess(status);
}
