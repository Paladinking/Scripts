#include "args.h"
#include "printf.h"
#include "mem.h"
#include "glob.h"

#define OPTION_INVALID 0
#define OPTION_VALID 1
#define OPTION_HELP 2
#define OPTION_RECURSE 4
#define OPTION_FORCE 8
#define OPTION_PROMT_ALWAYS 16
#define OPTION_PROMT_MANY 32
#define OPTION_DIR 64
#define OPTION_VERBOSE 128

#define OPTION_IF(opt, cond, flag)                                             \
    if (cond) {                                                                \
        opt |= flag;                                                           \
    }

const wchar_t *HELP_MESSAGE =
    L"Usage: %s [OPTION]... [FILE]...\n"
    L"Remove FILE(s)\n\n"
    L"-d, --dir                remove empty directories\n"
    L"-f, --force              ignore non-existant files and arguments, never prompt\n"
    L"-i                       prompt before every removal\n"
    L"-I,                      prompt once before removing more than 3 files, or\n"
    L"                         when removing recursively\n"
    L"    --interactive=[WHEN] interactive 'never', 'once' (-I) or 'always' (-i);\n"
    L"                         without WHEN prompt always\n"
    L"-r, --recursive          remove directories and their contents recursively\n"
    L"-v, --verbose            show output of what is being done\n";

uint32_t parse_options(int* argc, wchar_t** argv) {
    if (*argc == 0 || argv[0] == NULL) {
        return OPTION_INVALID;
    }
    
    const wchar_t* promt_never[] = {L"never"};
    const wchar_t* promt_once[] = {L"once"};
    const wchar_t* promt_always[] = {L"always"};

    EnumValue promt[] = {{promt_never, 1}, {promt_once, 1}, {promt_always, 1}};
    FlagValue interactive = {FLAG_ENUM | FLAG_OPTONAL_VALUE, promt, 3};

    FlagInfo flags[] = {
        {L'r', L"recursive", NULL},            // 0
        {L'f', L"force", NULL},                // 1
        {L'i', NULL, NULL},                    // 2
        {L'I', NULL, NULL},                    // 3
        {L'd', L"dir", NULL},                  // 4
        {L'v', L"verbose", NULL},              // 5
        {L'\0', L"interactive", &interactive}, // 6
        {L'h', L"help", NULL},                 // 7
    };

    uint32_t flag_count = sizeof(flags) / sizeof(FlagInfo);
    ErrorInfo err;
    if (!find_flags(argv, argc, flags, flag_count, &err)) {
        wchar_t* e = format_error(&err, flags, flag_count);
        if (e != NULL) {
            _wprintf_e(L"%s\n", e);
            _wprintf_e(L"Run '%s --help' for more information\n", argv[0]);
        }
        return OPTION_INVALID;
    }
    if (flags[7].count > 0) {
        _wprintf(HELP_MESSAGE, argv[0]);
        return OPTION_HELP;
    }

    uint32_t opts = OPTION_VALID;

    if (flags[2].ord > flags[3].ord && flags[2].ord > flags[6].ord) {
        opts |= OPTION_PROMT_ALWAYS;
    } else if (flags[3].ord > flags[2].ord && flags[3].ord > flags[6].ord) {
        opts |= OPTION_PROMT_MANY;
    } else if (flags[6].ord > flags[2].ord && flags[6].ord > flags[3].ord) {
        if (!interactive.has_value) {
            interactive.uint = 2;
        }
        OPTION_IF(opts, interactive.uint == 1, OPTION_PROMT_MANY);
        OPTION_IF(opts, interactive.uint == 2, OPTION_PROMT_ALWAYS);
    }
    OPTION_IF(opts, flags[0].count > 0, OPTION_RECURSE);
    OPTION_IF(opts, flags[1].count > 0, OPTION_FORCE);
    OPTION_IF(opts, flags[4].count > 0, OPTION_DIR);
    OPTION_IF(opts, flags[5].count > 0, OPTION_VERBOSE);

    return opts;
}

void file_error(DWORD e, const wchar_t* arg) {
    if (e == ERROR_FILE_NOT_FOUND || e == ERROR_BAD_ARGUMENTS ||
        e == ERROR_PATH_NOT_FOUND) {
        _wprintf_e(L"Cannot remove '%s': No such file or directory\n",
            arg);
    } else if (e == ERROR_ACCESS_DENIED) {
        _wprintf_e(
            L"Cannot remove '%s': Permission denied\n", arg);
    } else {
        wchar_t buf[256];
        if (FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM |
                           FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, e, 0, buf, 256, NULL)) {
            _wprintf_e(L"Cannot remove '%s': %s", arg, buf);
        } else {
            _wprintf_e(L"Cannot remove '%s'\n", arg);
        }
    }
}

bool is_empty(const wchar_t* path) {
    WalkCtx ctx;
    WalkDir_begin(&ctx, path, true);
    Path* p;
    if (WalkDir_next(&ctx, &p)) {
        WalkDir_abort(&ctx);
        return false;
    }
    DWORD err = GetLastError();
    if (err == ERROR_NO_MORE_FILES) {
        return true;
    }
    return false;
}

bool prompt_recurse(const wchar_t* arg, uint32_t opts) {
    if (opts & OPTION_FORCE) {
        return true;
    }
    if (!(opts & (OPTION_PROMT_MANY | OPTION_PROMT_ALWAYS))) {
        return true;
    }
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    if (GetFileType(in) != FILE_TYPE_CHAR) {
        return false;
    }
    wchar_t buf[1024];
    DWORD read;
    _wprintf(L"Remove directory '%s' recursively (y/N)?: ", arg);
    if (!ReadConsoleW(in, buf, 1024, &read, NULL)) {
        return false;
    }
    if (read > 0) {
        if (buf[0] == L'y' || buf[0] == L'Y') {
            return true;
        }
        return false;
    } else {
        return false;
    }
}

bool prompt_many(uint32_t count, uint32_t opts) {
    if (opts & OPTION_FORCE) {
        return true;
    }
    if (!(opts & OPTION_PROMT_MANY)) {
        return true;
    }
    if (count < 4) {
        return true;
    }
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    if (GetFileType(in) != FILE_TYPE_CHAR) {
        return false;
    }
    wchar_t buf[1024];
    DWORD read;
    _wprintf(L"Remove %d aguments (y/N)?: ", count);
    if (!ReadConsoleW(in, buf, 1024, &read, NULL)) {
        return false;
    }
    if (read > 0) {
        if (buf[0] == L'y' || buf[0] == L'Y') {
            return true;
        }
        return false;
    } else {
        return false;
    }

}

bool prompt(DWORD attrs, uint32_t opts, const wchar_t* arg) {
    if (opts & OPTION_FORCE) {
        return true;
    }
    if (!(opts & (OPTION_PROMT_ALWAYS))) {
        if (!(attrs & (FILE_ATTRIBUTE_READONLY))) {
            return true;
        }
    }

    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    if (GetFileType(in) != FILE_TYPE_CHAR) {
        return false;
    }
    const wchar_t* msg;
    if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) {
            msg = L"Remove directory symlink '%s' (y/N)?: ";
        } else {
            msg = L"Remove directory '%s' (y/N): ";
        }
    } else {
        if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) {
            msg = L"Remove symlink '%s' (y/N): ";
        } else{
            msg = L"Remove file '%s' (y/N): ";
        }
    }
    wchar_t buf[1024];
    DWORD read;
    _wprintf(msg, arg);
    if (!ReadConsoleW(in, buf, 1024, &read, NULL)) {
        return false;
    }
    if (read > 0) {
        if (buf[0] == L'y' || buf[0] == L'Y') {
            return true;
        }
        return false;
    } else {
        return false;
    }
}

bool remove_dir(const wchar_t* dir, const wchar_t* arg, uint32_t opts, DWORD attrs) {
    if (!prompt(attrs, opts, arg)) {
        return true;
    }
    if (!RemoveDirectoryW(dir)) {
        file_error(GetLastError(), arg);
        return false;
    }

    if (opts & OPTION_VERBOSE) {
        if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) {
            _wprintf(L"Removed symlink directory '%s'\n", arg);
        } else {
            _wprintf(L"Removed directory '%s'\n", arg);
        }
    }
    return true;
}

bool remove_file(const wchar_t* file, const wchar_t* arg, uint32_t opts, DWORD attrs) {
    if (!prompt(attrs, opts, arg)) {
        return true;
    }
    if (!DeleteFileW(file)) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND ||
            GetLastError() == ERROR_PATH_NOT_FOUND) {
            return true;
        }
        file_error(GetLastError(), arg);
        return false;
    }

    if (opts & OPTION_VERBOSE) {
        if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) {
            _wprintf(L"Removed symlink '%s'\n", arg);
        } else {
            _wprintf(L"Removed file '%s'\n", arg);
        }
    }
    return true;
}

int remove_files_recursive(const wchar_t* filename, const wchar_t* arg, DWORD attrs, uint32_t opts) {

    struct Node {
        WString dir;
        WalkCtx ctx;
        DWORD attrs;
    };

    struct Node* stack = Mem_alloc(4 * sizeof(struct Node));
    uint32_t stack_size = 1;
    uint32_t stack_cap = 4;
    if (stack == NULL) {
        return 2;
    }
    if (!WString_create(&stack[0].dir)) {
        Mem_free(stack);
        return 2;
    }
    if (!WString_extend(&stack[0].dir, filename)) {
        Mem_free(stack);
        WString_free(&stack[0].dir);
        return 2;
    }
    stack[0].attrs = attrs;
    WalkDir_begin(&stack[0].ctx, stack[0].dir.buffer, true);

    int status = 0;

    Path* path;
    while (stack_size > 0) {
        if (WalkDir_next(&stack[stack_size - 1].ctx, &path)) {
            if (path->is_dir) {
                if (path->is_link) {
                    if (!remove_dir(path->path.buffer, path->path.buffer, opts, 
                                    path->attrs)) {
                        status = 1;
                    }
                    continue;
                }
                if (!prompt_recurse(path->path.buffer, opts)) {
                    continue;
                }
                if (stack_size == stack_cap) {
                    struct Node* n = Mem_realloc(stack, stack_cap * 2 * sizeof(struct Node));
                    if (n == NULL) {
                        status = 2;
                        break;
                    }
                    stack = n;
                    stack_cap = stack_cap * 2;
                }
                if (!WString_create(&stack[stack_size].dir)) {
                    status = 2;
                    break;
                }
                if (!WString_extend(&stack[stack_size].dir, path->path.buffer)) {
                    WString_free(&stack[stack_size].dir);
                    status = 2;
                    break;
                }
                stack[stack_size].attrs = path->attrs;
                WalkDir_begin(&stack[stack_size].ctx, stack[stack_size].dir.buffer, true);
                ++stack_size;
            } else if (!remove_file(path->path.buffer, path->path.buffer, opts, path->attrs)){
                status = 1;
            }
        } else {
            if (GetLastError() == ERROR_NO_MORE_FILES || GetLastError() == ERROR_SUCCESS) {
                if (!remove_dir(stack[stack_size - 1].dir.buffer, 
                            stack[stack_size - 1].dir.buffer, 
                            opts, stack[stack_size - 1].attrs)) {
                    status = 1;
                }
            } else if (GetLastError() == ERROR_OUTOFMEMORY) { 
                status = 2;
                break;
            } else {
                file_error(GetLastError(), stack[stack_size - 1].dir.buffer);
                status = 1;
            }
            WString_free(&stack[stack_size - 1].dir);
            --stack_size;
        }
    }
    for (uint32_t ix = 0; ix < stack_size; ++ix) {
        WalkDir_abort(&stack[ix].ctx);
        WString_free(&stack[ix].dir);
    }

    Mem_free(stack);

    return status;
}


int remove_files(int argc, wchar_t** argv, uint32_t opts) {
    if (argc < 2) {
        _wprintf_e(L"Missing argument\n");
        _wprintf_e(L"Run '%s --help' for more information\n", argv[0]);
        return 1;
    }

    int status = 0;
    WString s;
    if (!WString_create(&s)) {
        _wprintf_e(L"Out of memory\n");
        return 2;
    }

    if (!prompt_many(argc - 1, opts)) {
        return 0;
    }

    for (int ix = 1; ix < argc; ++ix) {
        WString_clear(&s);
        if (!to_windows_path(argv[ix], &s)) {
            DWORD err = GetLastError();
            if (err == ERROR_OUTOFMEMORY) {
                _wprintf_e(L"Out of memory\n");
                WString_free(&s);
                return 2;
            }
            file_error(err, argv[ix]);
            status = 1;
            continue;
        }
        DWORD attr = GetFileAttributesW(s.buffer);
        if (attr == INVALID_FILE_ATTRIBUTES) {
            file_error(GetLastError(), argv[ix]);
            status = 1;
            continue;
        }
        if (attr & FILE_ATTRIBUTE_DIRECTORY) {
            if (opts & OPTION_RECURSE) {
                if (attr & FILE_ATTRIBUTE_REPARSE_POINT) {
                    if (!remove_dir(s.buffer, argv[ix], opts, attr)) {
                        status = 1;
                    }
                } else if (prompt_recurse(argv[ix], opts)){
                    int st = remove_files_recursive(s.buffer, argv[ix], attr, opts);
                    if (st > status) {
                        status = st;
                        if (st > 1) {
                            break;
                        }
                    }
                }
            } else if (opts & OPTION_DIR) {
                if (!(attr & FILE_ATTRIBUTE_REPARSE_POINT) && !is_empty(s.buffer)) {
                    _wprintf_e(L"Cannot remove '%s': Directory is not empty\n", argv[ix]);
                    status = 1;
                    continue;
                }
                if (!remove_dir(s.buffer, argv[ix], opts, attr)) {
                    status = 1;
                }
            } else {
                _wprintf_e(L"Cannot remove '%s': Is directory\n", argv[ix]);
                status = 1;
            }
            continue;
        }
        if (!remove_file(s.buffer, argv[ix], opts, attr)) {
            status = 1;
        }
    }

    return status;
}


int main() {
    wchar_t* args = GetCommandLineW();
    int argc;
    wchar_t** argv = glob_command_line(args, &argc);

    uint32_t opts = parse_options(&argc, argv);
    if (opts == OPTION_INVALID) {
        Mem_free(argv);
        ExitProcess(1);
    }
    if (opts == OPTION_HELP) {
        Mem_free(argv);
        ExitProcess(0);
    }

    int status = remove_files(argc, argv, opts);

    Mem_free(argv);
    ExitProcess(status);
}
