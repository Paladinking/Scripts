#include "args.h"
#include "mem.h"
#include "glob.h"
#include "printf.h"

#define OPTION_INVALID 0
#define OPTION_VALID 1
#define OPTION_ABSOLUTE 2
#define OPTION_HELP 8192

#define OPTION_IF(opt, cond, flag)                                             \
    if (cond) {                                                                \
        opt |= flag;                                                           \
    }

const wchar_t *HELP_MESSAGE =
    L"Usage: %s [OPTION]... DIR\n"
    L"Recursivly list files in a directory\n\n"
    L"-a, --absolute               list absolute file paths\n"
    L"-c, --count=COUNT            list at most COUNT files\n"
    L"    --file-only              only match files\n"
    L"    --folder-only            only match folders\n"
    L"-h, --help                   display this message and exit\n"
    L"    --max-depth=DEPTH        specify max depth to search\n"
    L"-n, --name=NAME              specify name glob pattern to match\n"
    L"-s, --substring              specify that -n and -w match if pattern is \n"
    L"                             a substring of the filename, not a glob match.\n"
    L"-w, --whole-name             specify glob pattern for path to match\n"
    L"-q, --quit                   quit after first match, same as --count=1\n";


typedef struct Filter {
    uint64_t max_depth;
    wchar_t* name_pattern;
    wchar_t* path_pattern;
    bool file;
    bool folder;
    bool substring;
    uint64_t max_count;
} Filter;


uint32_t parse_options(int* argc, wchar_t** argv, Filter* filter) {
    FlagValue max_depth_val = {FLAG_REQUIRED_VALUE | FLAG_UINT};
    FlagValue name_pattern = {FLAG_REQUIRED_VALUE | FLAG_STRING};
    FlagValue wholename_pattern = {FLAG_REQUIRED_VALUE | FLAG_STRING};
    FlagValue count = {FLAG_REQUIRED_VALUE | FLAG_UINT};
    FlagInfo flags[] = {
        {L'a', L"absolute", NULL},
        {L'h', L"help", NULL},
        {L'\0', L"max-depth", &max_depth_val},
        {L'n', L"name", &name_pattern},
        {L'\0', L"file-only", NULL},
        {L'\0', L"folder-only", NULL},
        {L'c', L"count", &count},
        {L'q', L"quit", NULL},
        {L'w', L"whole-name", &wholename_pattern},
        {L's', L"substring", NULL}
    };
    const uint32_t flag_count = sizeof(flags) / sizeof(FlagInfo);
    ErrorInfo err;
    if (!find_flags(argv, argc, flags, flag_count, &err)) {
        wchar_t *err_msg = format_error(&err, flags, flag_count);
        if (err_msg) {
            _wprintf_e(L"%s\n", err_msg);
            Mem_free(err_msg);
        }
        if (argc > 0) {
            _wprintf_e(L"Run '%s --help' for more information\n", argv[0]);
        }
        return OPTION_INVALID;
    }
    if (flags[1].count > 0) {
        _wprintf(HELP_MESSAGE, argv[0]);
        return OPTION_HELP;
    }
    if (flags[2].count > 0) {
        filter->max_depth = max_depth_val.uint;
    } else {
        filter->max_depth = 0xFFFFFFFFFFFFFFFF;
    }
    if (flags[3].count > 0) {
        filter->name_pattern = name_pattern.str;
    } else {
        filter->name_pattern = NULL;
    }
    filter->file = flags[4].count > 0;
    filter->folder = flags[5].count > 0;
    if (flags[6].ord >= flags[7].ord) {
        if (flags[6].count > 0) {
            filter->max_count = count.uint;
        }  else {
            filter->max_count = 0xFFFFFFFFFFFFFFFF;
        }
    } else {
        filter->max_count = 1;
    }
    if (flags[8].count > 0) {
        filter->path_pattern = wholename_pattern.str;
    } else {
        filter->path_pattern = NULL;
    }
    filter->substring = flags[9].count > 0;

    uint32_t opts = OPTION_VALID;
    OPTION_IF(opts, flags[0].count > 0, OPTION_ABSOLUTE);

    return opts;
}

bool contains_substr(const wchar_t* s, const wchar_t* substr) {
    for (uint64_t ix = 0; s[ix] != L'\0'; ++ix) {
        for (uint64_t j = 0;; ++j) {
            if (substr[j] == L'\0') {
                return true;
            }
            if (s[ix + j] == L'\0') {
                return false;
            }
            if (towlower(s[ix + j]) != towlower(substr[j])) {
                break;
            }
        }
    }
    return false;
}

bool matches_filter(Path* path, Filter* filter) {
    uint64_t filename_offset = path->path.length - path->name_len;
    wchar_t* name = path->path.buffer + filename_offset;
    if (filter->file && path->is_dir) {
        return false;
    }
    if (filter->folder && !path->is_dir) {
        return false;
    }
    bool matches_name = true;
    bool matches_path = true;
    if (filter->name_pattern != NULL) {
        if (filter->substring) {
            matches_name = contains_substr(name, filter->name_pattern);
        } else {
            matches_name = matches_glob(filter->name_pattern, name);
        }
    }
    if (filter->path_pattern != NULL) {
        if (filter->substring) {
            matches_path = contains_substr(path->path.buffer,
                                           filter->path_pattern);
        } else {
            matches_path = matches_glob(filter->path_pattern,
                                        path->path.buffer);
        }
    }
    return matches_name && matches_path;
}

int find(Path* dir, Filter* filter) {
    uint32_t stack_size = 0;
    uint32_t stack_cap = 16;
    if (filter->max_count == 0) {
        return 0;
    }

    typedef struct Node {
        WalkCtx ctx;
    } Node;

    Node* stack = Mem_alloc(stack_cap * sizeof(Node));
    if (stack == NULL) {
        return 1;
    }
    WalkDir_begin(&stack[0].ctx, dir->path.buffer, true);
    stack_size = 1;

    uint64_t matches = 0;
    if (matches_filter(dir, filter)) {
        _wprintf(L"%s\n", dir->path.buffer);
        ++matches;
    }
    if (filter->max_depth == 0 || filter->max_count == matches) {
        WalkDir_abort(&stack[0].ctx);
        --stack_size;
    }

    int status = 0;
    while (stack_size > 0) {
        Node* elem = &stack[stack_size - 1];
        Path* path;
        if (WalkDir_next(&elem->ctx, &path)) {
            if (matches_filter(path, filter)) {
                _wprintf(L"%s\n", path->path.buffer);
                ++matches;
                if (filter->max_count == matches) {
                    break;
                }
            }
            if (path->is_dir && stack_size < filter->max_depth) {
                if (stack_size == stack_cap) {
                    Node* new_stack = Mem_realloc(stack, 
                            stack_cap * 2 * sizeof(Node));
                    if (new_stack == NULL) {
                        status = 1;
                        break;
                    }
                    stack_cap *= 2;
                    stack = new_stack;
                }
                WalkDir_begin(&stack[stack_size].ctx, path->path.buffer, true);
                ++stack_size;
            }
        } else {
            --stack_size;
        }
    }
    for (uint32_t ix = 0; ix < stack_size; ++ix) {
        WalkDir_abort(&stack[ix].ctx);
    }

    Mem_free(stack);
    return status;
}


int find_file(int argc, wchar_t** argv) {
    Filter filter;
    uint32_t opts = parse_options(&argc, argv, &filter);
    if (opts == OPTION_HELP) {
        return 0;
    } else if (!(opts & OPTION_VALID)) {
        return 1;
    }

    Path path;
    WString_create(&path.path);
    if (argc >= 2) {
        if (!WString_extend(&path.path, argv[1])) {
            WString_free(&path.path);
            return 1;
        }
    } else {
        if (!WString_append(&path.path, L'.')) {
            WString_free(&path.path);
            return 1;
        }

    }
    if (opts & OPTION_ABSOLUTE) {
        if (!make_absolute(path.path.buffer, &path.path)) {
            WString_free(&path.path);
            return 1;
        }
    }

    path.is_dir = is_directory(path.path.buffer);
    path.is_link = false;
    wchar_t* sep = wcsrchr(path.path.buffer, L'\\');
    if (sep != NULL) {
        uint64_t dir_size = sep - path.path.buffer;
        path.name_len = path.path.length - dir_size - 1;
    } else {
        path.name_len = path.path.length;
    }

    int status = find(&path, &filter);
    WString_free(&path.path);
    return status;
}

int main() {
    wchar_t* args = GetCommandLineW();
    int argc;
    wchar_t** argv = parse_command_line(args, &argc);
    int status = 1;
    if (argv != NULL) {
        status = find_file(argc, argv);
        Mem_free(argv);
    }

    ExitProcess(status);
}
