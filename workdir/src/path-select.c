#define UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <Shlwapi.h>
#include <tlhelp32.h>
#include "args.h"
#include "printf.h"
#include "path_utils.h"

DWORD GetParentProcessId() {
    // Get the current process ID
    DWORD currentProcessId = GetCurrentProcessId();

    // Create a snapshot of the system processes
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    // Iterate through the processes to find the parent process
    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (pe32.th32ProcessID == currentProcessId) {
                // Found the current process in the snapshot, return its parent
                // process ID
                CloseHandle(hSnapshot);
                return pe32.th32ParentProcessID;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    // Process not found or unable to retrieve information
    CloseHandle(hSnapshot);
    return 0;
}

wchar_t** split_path(WString* path, unsigned* count) {
    HANDLE heap = GetProcessHeap();
    *count = 0;
    unsigned cap = 16;
    wchar_t** out = HeapAlloc(heap, 0, cap * sizeof(wchar_t*));
    *count = 0;

    unsigned i = 0;
    while (path->buffer[i] != L'\0') {
        WCHAR endc = L';';
        unsigned begin = i;
        if (path->buffer[i] == L'"') {
            endc = L'"';
            ++i;
        }
        while (1) {
            if (path->buffer[i] == L'\0' || path->buffer[i] == endc) {
                break;
            }
            ++i;
        }
        while (path->buffer[i] != L'\0' && path->buffer[i] != L';') {
            ++i;
        }
        unsigned end = i;
        if (*count == cap) {
            cap = cap * 2;
            out = HeapReAlloc(heap, 0, out, cap * sizeof(wchar_t*));
        }
        out[*count] = path->buffer + begin;
        *count += 1;
        if (path->buffer[i] == '\0') {
            path->buffer[end] = '\0';
            break;
        } else {
            path->buffer[end] = '\0';
            ++i;
        }
    }

    return out;
}

typedef struct _WideBuffer {
	LPWSTR ptr;
	DWORD size;
	DWORD capacity;
} WideBuffer;

// 0 = false, 1 = true, < 0 = error
int has_file(const wchar_t* file, const wchar_t* dir, unsigned file_len, WideBuffer* buf) {
    unsigned dir_len = wcslen(dir);
    if (file_len + dir_len + 2 > buf->capacity) {
        buf->capacity = file_len + dir_len + 2;
        HeapFree(GetProcessHeap(), 0, buf->ptr);
        buf->ptr = HeapAlloc(GetProcessHeap(), 0, buf->capacity * sizeof(wchar_t));
    }
    if (dir[0] == L'"') {
        ++dir;
        --dir_len;
        while (dir[dir_len] != L'"') {
            --dir_len;
        }
    }
    memcpy(buf->ptr, dir, dir_len * sizeof(wchar_t));
    buf->size = file_len + dir_len + 1;
    buf->ptr[dir_len] = L'\\';
    
    memcpy(buf->ptr + dir_len + 1, file, (file_len + 1) * sizeof(wchar_t));

    WIN32_FIND_DATAW data;
    HANDLE h = FindFirstFileW(buf->ptr, &data);
    if (h == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND) {
            return 0;
        }
        return -1;
    }
    FindClose(h);
    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        return 0;
    }
    // FindFirstFileW matches also on short filenames
    // PathMatchSpecW is needed to confirm true match.
    if (PathMatchSpecW(data.cFileName, file)) {
        return 1;
    }
    return 0;
}

typedef struct PathInfo {
    unsigned ix;
    BOOL has_file;
} PathInfo;

BOOL scan_path(wchar_t* arg, unsigned arg_len, wchar_t** path, unsigned count,
               BOOL* path_data, unsigned* match_count) {

    WideBuffer work;
    work.ptr = NULL;
    work.size = 0;
    work.capacity = 0;
    *match_count = 0;

    for (unsigned i = 0; i < count; ++i) {
        int res = has_file(arg, path[i], arg_len, &work);
        if (res < 0) {
            HeapFree(GetProcessHeap(), 0, work.ptr);
            return FALSE;
        }
        if (res) {
            path_data[i] = TRUE;
            *match_count = *match_count + 1;
        } else {
            path_data[i] = FALSE;
        }
    }

    HeapFree(GetProcessHeap(), 0, work.ptr);
    return TRUE;
}

void select_path(wchar_t** paths, BOOL* has_file, unsigned count, unsigned ix) {
    for (unsigned i = 0; i < count; ++i) {
        if (has_file[i]) {
            unsigned n = 0;
            for (unsigned j = 0; j < count; ++j) {
                if (has_file[j]) {
                    if (n == ix) {
                        wchar_t* temp = paths[i];
                        paths[i] = paths[j];
                        paths[j] = temp;
                        has_file[i] = FALSE;
                        break;
                    }
                    ++n;
                }
            }
            break;
        }
    }
}

void merge_path(WString* dest, wchar_t** path_parts, unsigned count, unsigned path_size) {
    WString_create_capacity(dest, path_size + 1);

    for (unsigned ix = 0; ix < count; ++ix) {
        WString_extend(dest, path_parts[ix]);
        WString_append(dest, L';');
    }
}

const char* help_message =
    "usage: path-select [-h | --help] [-e | --exact] <file>\n\n"
    "Reorders entries in PATH where <file> can be found.\n"
    "The following flags can be used:\n\n"
    "  -h, --help          Displays this help message\n"
    "  -e, --exact         When <file> has no file extension, find files named <file> that also have no extension.\n";

int main() {
    HANDLE heap = GetProcessHeap();
    HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    wchar_t* args = GetCommandLine();
    int argc;
    int status = 0;
    wchar_t **argv = parse_command_line(args, &argc);
    if (find_flag(argv, &argc, L"-h", L"--help") > 0) {
        _wprintf(L"%S", help_message);
        goto end;
    }
    if (argc < 2) {
        _wprintf_h(err, L"Missing argument\n");
        status = 1;
        goto end;
    }
    BOOL exact = find_flag(argv, &argc, L"-e", L"--exact") > 0;
    if (argc > 2) {
        _wprintf_h(err, L"Unexpecet argument '%s'\n", argv[2]);
        status = 2;
        goto end;
    }
    unsigned arg_len = wcslen(argv[1]);
    if (arg_len > 256) {
        _wprintf_h("Invalid argument '%s' (too long)\n", argv[1]);
        status = 3;
        goto end;
    }

    wchar_t arg[260];
    memcpy(arg, argv[1], (arg_len + 1) * sizeof(wchar_t));
    if (!exact) {
        if (wcsrchr(arg, L'.') == NULL) {
            arg[arg_len] = L'.';
            arg[arg_len + 1] = '*';
            arg[arg_len + 2] = '\0'; 
            arg_len += 2;
        }
    }

    WString buf;
    buf.buffer = NULL;
    buf.length = 0;
    buf.capacity = 0;
    if (get_envvar(L"PATH", 2048, &buf)) {
        unsigned count;
        wchar_t** path_parts = split_path(&buf, &count);
        
        BOOL* path_data = HeapAlloc(GetProcessHeap(), 0, count * sizeof(BOOL));
        unsigned match_count = 0;
        if (scan_path(arg, arg_len, path_parts, count, path_data, &match_count)) {
            if (match_count == 0) {
                _wprintf(L"%s not found in Path\n", argv[1]);
                status = 1;
            } else if (match_count == 1) {
                _wprintf(L"Only one match found, no changes can be made\n");
            } else {
                HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
                BOOL skip = FALSE;
                DWORD read;
                do {
                    unsigned n = 0;
                    for (unsigned i = 0; i < count; ++i) {
                        if (path_data[i]) {
                            ++n;
                            _wprintf(L"%d: %s\n", n, path_parts[i]);
                        }
                    }

                    _wprintf(L"Pick path to come first (1-%d): ", match_count);
                    char numbuf[256];
                    read_input:
                    if (!ReadFile(in, numbuf, 255, &read, NULL)) {
                        _wprintf_h(err, L"I/O Error\n");
                        break;
                    }
                    if (skip) {
                        if (read != 0 && numbuf[read - 1] == '\n') {
                            skip = FALSE;
                        }
                        goto read_input;
                    }
                    if (read == 0 || numbuf[read - 1] != '\n') {
                        _wprintf(L"Invalid value, pick (1-%d): ", match_count);
                        skip = read > 0;
                        goto read_input;
                    }
                    char* newline = numbuf + read - 1;
                    if (read > 1 && numbuf[read - 2] == '\r') {
                        newline = numbuf + read - 2;
                    }
                    *newline = '\0';
                    char* end;
                    long val = strtol(numbuf, &end, 10) - 1;
                    if (end != newline || val < 0 || val >= match_count) {
                        _wprintf(L"Invalid value, pick (1-%d): ", match_count);
                        goto read_input;
                    }
                    select_path(path_parts, path_data, count, val);
                    --match_count;
                } while (match_count > 1);
                WString dest;
                merge_path(&dest, path_parts, count, buf.length);
                DWORD id = GetParentProcessId();
                if (id == 0) {
                    status = 5;
                    _wprintf_h(err, L"Failed getting parent process id\n");
                } else if (SetProcessEnvironmentVariable(L"PATH", dest.buffer, id)) {
                    status = 6;
                    _wprintf_h(err, L"Failed setting PATH variable\n");
                }

                WString_free(&dest);
            }
        } else {
            status = 4;
            _wprintf_h(err, L"Error: %d", GetLastError());
        }

        HeapFree(GetProcessHeap(), 0, path_data);
        HeapFree(GetProcessHeap(), 0, path_parts);
        WString_free(&buf);
    } else {
        _wprintf_h(err, L"Failed getting PATH\n");
        status = 3;
    }
end:
    HeapFree(GetProcessHeap(), 0, argv);
    return status;
}
