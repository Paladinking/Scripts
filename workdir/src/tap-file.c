#include "args.h"
#include "printf.h"
#include "mem.h"
#include "glob.h"

#define OPTION_INVALID 0
#define OPTION_VALID 1
#define OPTION_HELP 2
#define OPTION_ACCESSTIME 4
#define OPTION_NOCREATE 8
#define OPTION_MODTIME 16
#define OPTION_NODEREF 32

#define OPTION_IF(opt, cond, flag)                                             \
    if (cond) {                                                                \
        opt |= flag;                                                           \
    }

const wchar_t *HELP_MESSAGE =
    L"Usage: %s [OPTION]... FILE...\n"
    L"Update the access time and modification time of each FILE to current time\n"
    L"Creates an empty file if it does not already exists,\n"
    L"unless -c or -h is used)\n\n"
    L"-a                           update only access time\n"
    L"-c, --no-create              do not create any files\n"
    L"-h, --no-dereference         modify symlinks, not the file they point to\n"
    L"    --help                   display this message and exit\n"
    L"-m                           update only last modification time\n"
    L"-r, --reference=FILE         use FILE's times instead of current time\n";


uint32_t parse_options(int* argc, wchar_t** argv, FILETIME* accesstime, FILETIME* updatetime) {
    if (*argc == 0 || argv == NULL) {
        return OPTION_INVALID;
    }

    FlagValue ref_val = {FLAG_REQUIRED_VALUE | FLAG_STRING};

    FlagInfo flags[] = {
        {L'a', NULL, NULL},
        {L'c', L"no-create", NULL},
        {L'h', L"no-dereference", NULL},
        {L'm', NULL, NULL},
        {L'r', L"reference", &ref_val},
        {L'\0', L"help", NULL}
    };
    const uint32_t flag_count = sizeof(flags) / sizeof(FlagInfo);
    ErrorInfo err;
    if (!find_flags(argv, argc, flags, flag_count, &err)) {
        wchar_t* msg = format_error(&err, flags, flag_count);
        if (msg != NULL) {
            _wprintf_e(L"%s\n", msg);
            _wprintf_e(L"Run '%s --help' for more information\n", argv[0]);
            Mem_free(msg);
        }
        return OPTION_INVALID;
    }

    if (flags[5].count > 0) {
        _wprintf(HELP_MESSAGE, argv[0]);
        return OPTION_HELP;
    }

    uint32_t opts = OPTION_VALID;

    if (flags[0].count > 0) {
        if (flags[3].count > 0) {
            opts |= OPTION_ACCESSTIME | OPTION_MODTIME;
        } else {
            opts |= OPTION_ACCESSTIME;
        }
    } else if (flags[3].count > 0) {
        opts |= OPTION_MODTIME;
    } else {
        opts |= OPTION_ACCESSTIME | OPTION_MODTIME;
    }
    OPTION_IF(opts, flags[1].count > 0, OPTION_NOCREATE);
    OPTION_IF(opts, flags[2].count > 0, OPTION_NODEREF);


    if (flags[4].count > 0) {
        WString s;
        if (!WString_create(&s)) {
            _wprintf_e(L"Failed to get reference time from '%s'\n", ref_val.str);
            return OPTION_INVALID;
        }
        DWORD att;
        if (!to_windows_path(ref_val.str, &s) ||
           (att = GetFileAttributesW(s.buffer)) == INVALID_FILE_ATTRIBUTES) {
            WString_free(&s);
            _wprintf_e(L"Failed to get reference time from '%s'\n", ref_val.str);
            return OPTION_INVALID;
        }
        HANDLE f;
        if (att & FILE_ATTRIBUTE_DIRECTORY) {
             f = CreateFileW(s.buffer, GENERIC_READ,
                             FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        } else {
             f = CreateFileW(s.buffer, GENERIC_READ,
                             FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        }
        WString_free(&s);
        if (f != INVALID_HANDLE_VALUE) {
            if (!GetFileTime(f, NULL, accesstime, updatetime)) {
                CloseHandle(f);
                _wprintf_e(L"Failed to get reference time from '%s'\n", ref_val.str);
                return OPTION_INVALID;
            }
            CloseHandle(f);
        } else {
            _wprintf_e(L"Failed to get reference time from '%s'\n", ref_val.str);
            return OPTION_INVALID;
        }
    } else {
        GetSystemTimePreciseAsFileTime(accesstime);
        *updatetime = *accesstime;
    }

    return opts;
}

void file_error(DWORD e, const wchar_t* arg) {
    if (e == ERROR_FILE_NOT_FOUND || e == ERROR_BAD_ARGUMENTS ||
        e == ERROR_PATH_NOT_FOUND) {
        _wprintf_e(L"Cannot set times of '%s': No such file or directory\n",
            arg);
    } else if (e == ERROR_ACCESS_DENIED) {
        _wprintf_e(
            L"Cannot set times of '%s': Permission denied\n", arg);
    } else {
        wchar_t buf[256];
        if (FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM |
                           FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, e, 0, buf, 256, NULL)) {
            _wprintf_e(L"Cannot set times of '%s': %s", arg, buf);
        } else {
            _wprintf_e(L"Cannot set times of '%s'\n", arg);
        }
    }
}

int update_files(int argc, wchar_t** argv, uint32_t opts, FILETIME* accesstime, FILETIME* modtime) {
    WString buf;
    if (!WString_create(&buf)) {
        _wprintf_e(L"Out of memory\n");
        return 2;
    }
    if (!(opts & OPTION_ACCESSTIME)) {
        accesstime = NULL;
    }
    if (!(opts & OPTION_MODTIME)) {
        modtime = NULL;
    }
    int status = 0;
    for (int i = 1; i < argc; ++i) {
        WString_clear(&buf);
        DWORD att = INVALID_FILE_ATTRIBUTES;
        if (!to_windows_path(argv[i], &buf)) {
            if (GetLastError() == ERROR_OUTOFMEMORY) {
                _wprintf_e(L"Out of memory\n");
                WString_free(&buf);
                return 2;
            }
        } else {
            att = GetFileAttributesW(buf.buffer);
        }
        if (att == INVALID_FILE_ATTRIBUTES) {
            if (opts & OPTION_NOCREATE) {
                continue;
            }
            if (opts & OPTION_NODEREF) {
                file_error(GetLastError(), argv[i]);
                status = 1;
                continue;
            }
            HANDLE f = CreateFileW(buf.buffer, GENERIC_WRITE, 0, NULL, 
                    CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
            if (f == INVALID_HANDLE_VALUE) {
                file_error(GetLastError(), argv[i]);
                status = 1;
                continue;
            }
            if (!SetFileTime(f, NULL, accesstime, modtime)) {
                file_error(GetLastError(), argv[i]);
                status = 1;
            }
            CloseHandle(f);
            continue;
        }
        DWORD flags = 0;
        if (opts & OPTION_NODEREF) {
            flags |= FILE_FLAG_OPEN_REPARSE_POINT;
        }
        if (att & FILE_ATTRIBUTE_DIRECTORY) {
            flags |= FILE_FLAG_BACKUP_SEMANTICS;
        }
        if (flags == 0) {
            flags = FILE_ATTRIBUTE_NORMAL;
        }
        HANDLE f = CreateFileW(buf.buffer, FILE_WRITE_ATTRIBUTES, 0, NULL,
                               OPEN_EXISTING, flags, NULL);
        if (f == INVALID_HANDLE_VALUE) {
            file_error(GetLastError(), argv[i]);
            status = 1;
            continue;
        }
        if (!SetFileTime(f, NULL, accesstime, modtime)) {
            file_error(GetLastError(), argv[i]);
            status = 1;
        }
        CloseHandle(f);
    }
    return status;
}

int main() {
    wchar_t* args = GetCommandLineW();
    int argc;
    wchar_t** argv = parse_command_line(args, &argc);

    FILETIME accesstime, modtime;

    uint32_t opts = parse_options(&argc, argv, &accesstime, &modtime);
    if (opts == OPTION_INVALID) {
        Mem_free(argv);
        ExitProcess(1);
    }
    if (opts == OPTION_HELP) {
        Mem_free(argv);
        ExitProcess(0);
    }
    int status = update_files(argc, argv, opts, &accesstime, &modtime);
    Mem_free(argv);
    ExitProcess(status);
}
