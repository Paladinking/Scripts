#define UNICODE
#ifdef __GNUC__
#define main entry_main
#endif
#include "args.h"
#include "path_utils.h"
#include "printf.h"
#include "string_conv.h"
#include <wchar.h>
#include <tlhelp32.h>
#include <windows.h>

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

LPSTR read_paths_file(LPCWSTR name, LPCWSTR ext, LPCWSTR binpath,
                      DWORD *count) {
    HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    HANDLE heap = GetProcessHeap();

    WCHAR dir[256];
    WCHAR drive[10];
    DWORD status =
        _wsplitpath_s(binpath, drive, 10, dir, 256, NULL, 0, NULL, 0);
    if (status != 0) {
        return NULL;
    }
    WCHAR path_file[256];
    status = _wmakepath_s(path_file, 256, drive, dir, name, ext);
    if (status != 0) {
        return NULL;
    }

    HANDLE hPaths = CreateFile(path_file, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, 0, NULL);
    if (hPaths == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    LARGE_INTEGER size;
    if (!GetFileSizeEx(hPaths, &size)) {
        CloseHandle(hPaths);
        return NULL;
    }
    LPSTR buffer = HeapAlloc(heap, 0, size.QuadPart + 1);
    if (buffer == NULL) {
        CloseHandle(hPaths);
        return NULL;
    }
    DWORD read;
    if (!ReadFile(hPaths, buffer, size.QuadPart, &read, NULL) ||
        read != size.QuadPart) {
        CloseHandle(hPaths);
        HeapFree(heap, 0, buffer);
        return NULL;
    }
    CloseHandle(hPaths);
    buffer[size.QuadPart] = '\0';
    *count = 1;
    LPSTR line_end = buffer - 1;
    while (1) {
        line_end = strchr(line_end + 1, '\n');
        if (line_end == NULL) {
            return buffer;
        }
        // Check for windows newline
        if (line_end > buffer && *(line_end - 1) == '\r') {
            *(line_end - 1) = '\0';
        } else {
            *line_end = '\0';
        }
        *count += 1;
    }

    return buffer;
}

WideBuffer add_buffer = {NULL, 0, 0};

EnvBuffer environment = {NULL, 0, 0};
WCHAR gBinpath[260];

OpStatus parse_env_file(LPSTR file, DWORD count, LPSTR file_name,
                        DWORD file_name_len) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    HANDLE heap = GetProcessHeap();
    LPSTR line = file;
    OpStatus res = OP_NO_CHANGE;
    for (unsigned i = 0; i < count; ++i) {
        if (*line == '\n') {
            ++line;
        }
        LPSTR line_name = NULL;
        if (line[0] == '!' && line[1] == '!') {
            DWORD len = strlen(line + 2);
            NarrowStringToWide(line + 2, len, &add_buffer);
            WideBuffer *var = read_envvar(add_buffer.ptr, &environment);
            if (var != NULL && var->size > 0) {
                return res;
            }
        }
        if (line[0] == '*' && line[1] == '*') {
            line = line + 2;
            DWORD len = strlen(line);
            _printf_h(out, "%.*s\n", len, line);
            line = line + len + 1;
            continue;
        }
        if ((line_name = strchr(line, '|')) != NULL ||
            (line_name = strchr(line, '>')) != NULL ||
            (line_name = strchr(line, '<')) != NULL) {
            DWORD len = line_name - line;
            DWORD name_len = strlen(line_name + 1);
            NarrowStringToWide(line_name + 1, name_len, &add_buffer);
            WideBuffer *var = read_envvar(add_buffer.ptr, &environment);
            if (var == NULL) {
                _wprintf_h(err, L"Invalid environment variable %.*s\n",
                           add_buffer.size, add_buffer.ptr);
            } else if (*line_name == '|') {
                NarrowStringToWide(line, len, &add_buffer);
                OpStatus status = path_add(add_buffer.ptr, var, TRUE, FALSE);
                if (status > OP_NO_CHANGE) {
                    _wprintf_h(err, L"Error while parsing file %.*S: %S\n",
                               file_name_len, file_name, line);
                    if (status != OP_INVALID_PATH) {
                        return status;
                    }
                } else if (status == OP_SUCCESS) {
                    res = OP_SUCCESS;
                }
            } else if (*line_name == '>') {
                NarrowStringToWide(line, len, &add_buffer);
                WideBuffer temp = add_buffer;
                add_buffer = *var;
                *var = temp;
                res = OP_SUCCESS;
            } else {
                NarrowStringToWide(line, len, &add_buffer);
                WideBuffer *to_save = read_envvar(add_buffer.ptr, &environment);
                if (to_save == NULL) {
                    _wprintf_h(err, L"Invalid environment variable %.*s\n",
                               add_buffer.size, add_buffer.ptr);
                } else {
                    if (var->capacity < to_save->size + 1) {
                        LPWSTR buf = HeapAlloc(
                            heap, 0, (to_save->size + 1) * sizeof(WCHAR));
                        if (buf == NULL) {
                            return OP_OUT_OF_MEMORY;
                        }
                        var->capacity = to_save->size + 1;
                        var->size = to_save->size;
                        HeapFree(heap, 0, var->ptr);
                        var->ptr = buf;
                    }
                    memcpy(var->ptr, to_save->ptr,
                           (to_save->size + 1) * sizeof(WCHAR));
                    res = OP_SUCCESS;
                }
            }
            line = line + len + name_len + 2;
        } else {
            line = line + strlen(line) + 1;
        }
    }
    return res;
}

OpStatus add_paths(LPSTR paths, DWORD count, LPSTR name, LPCWSTR binpath) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    HANDLE heap = GetProcessHeap();
    LPSTR line = paths;
    DWORD name_len = strlen(name);
    OpStatus res = OP_NO_CHANGE;
    for (unsigned i = 0; i < count; ++i) {
        if (*line == '\n') {
            ++line;
        }
        LPSTR line_name = NULL;
        if ((line_name = strchr(line, '|')) != NULL &&
            _stricmp(line_name + 1, name) == 0) {
            DWORD len = line_name - line;
            if (line[0] == '"' && line[len - 1] == '"') {
                ++line;
                len -= 2;
            }
            NarrowStringToWide(line, len, &add_buffer);
            OpStatus status =
                path_add(add_buffer.ptr, &environment.vars[0].val, TRUE, FALSE);
            if (status > OP_NO_CHANGE) {
                _printf_h(err, "Failed adding %.*s to Path\n", len, line);
                if (status != OP_INVALID_PATH) {
                    return status;
                }
            } else if (status == OP_SUCCESS) {
                _printf_h(out, "Added %.*s to Path\n", len, line);
                res = OP_SUCCESS;
            }
            line = line_name + name_len + 2;
        } else if ((line_name = strchr(line, '<')) != NULL &&
                   _stricmp(line_name + 1, name) == 0) {
            *line_name = '\0';
            OpStatus status = add_paths(paths, count, line, binpath);
            *line_name = '<';
            if (status == OP_SUCCESS) {
                res = OP_SUCCESS;
            } else if (status > OP_INVALID_PATH) {
                return status;
            }
            line = line_name + name_len + 2;
        } else if ((line_name = strchr(line, '>')) != NULL &&
                   _stricmp(line_name + 1, name) == 0) {
            DWORD len = line_name - line;
            NarrowStringToWide(line, len, &add_buffer);
            LPWSTR ext = add_buffer.ptr + add_buffer.size;
            while (ext > add_buffer.ptr && *ext != L'.') {
                --ext;
            }
            if (ext == add_buffer.ptr) {
                ext = NULL;
            } else {
                *ext = L'\0';
                ext += 1;
            }
            DWORD lines;
            LPSTR file = read_paths_file(add_buffer.ptr, ext, gBinpath, &lines);
            if (ext != NULL) {
                *(ext - 1) = L'.';
            }
            if (file == NULL) {
                _wprintf_h(err, L"Could not open %.*s\n", add_buffer.size,
                           add_buffer.ptr);
            } else {
                OpStatus status = parse_env_file(file, lines, line, len);
                if (status == OP_SUCCESS) {
                    res = OP_SUCCESS;
                } else if (status > OP_INVALID_PATH) {
                    return status;
                }
                HeapFree(heap, 0, file);
            }
            line = line_name + name_len + 2;
        } else {
            line = line + strlen(line) + 1;
        }
    }
    return res;
}
const char* help_message =
    "usage: path-add [NAME]...\n"
    "Goes through paths.txt and adds all matching lines.\n"
    "The paths.txt file should be located next to the path-add binary.\n"
    "\n"
    "Lines in paths.txt can have 3 different formats:\n"
    " 1. {PATH}|{NAME}\n"
    "  If {NAME} matches an input argument, {PATH} will be added to PATH environment variable.\n"
    "  Note that | is a literal and should be present in the file.\n"
    " 2. {CHILD}<{NAME}\n"
    "  If {NAME} matches an input argument, add {CHILD} next in argument list.\n"
    "  This can (but does not have to) cause infinite recursion for bad input, causing the program to run out of memory.\n"
    " 3. {FILE}>{NAME}\n"
    "  If {NAME} matches an input argument, parse {FILE} for environment variables.\n"
    "\n"
    "All other lines are ignored.\n"
    "\n"
    "Lines in an environment variable file given by format 3 in paths.txt can have 5 different formats:\n"
    " 1. !!{VAR}\n"
    "  Stop parsing file if {VAR} is defined.\n"
    " 2. **{TEXT}\n"
    "  {TEXT} is printed to the console.\n"
    " 3. {PATH}|{VAR}\n"
    "  {PATH} will be prepended to the environment variable {VAR}, separated by semi-colons.\n"
    " 4. {VALUE}>{VAR}\n"
    "  The environment variable {VAR} will be set to {VALUE}.\n"
    "  Just >{VAR} is allowed and will delete the environment variable {VAR}.\n"
    " 5. {TO_SAVE}<{VAR}\n"
    "  The environment variable {VAR} will be set to the value in {TO_SAVE}.\n"
    "  This is usefull for saving and restoring environment variables.\n"
    "\n"
    "All other lines are again ignored.\n";

int main() {
    HANDLE heap = GetProcessHeap();
    LPWSTR args = GetCommandLine();
    int argc;
    int status = 0;
    LPWSTR *argv = parse_command_line(args, &argc);
    if (argc < 2) {
        status = 1;
        goto end;
    }

    if (find_flag(argv, &argc, L"--help", L"-h")) {
        _printf(help_message);
        goto end;
    }

    DWORD mod_res = GetModuleFileName(NULL, gBinpath, 260);
    if (mod_res == 260 || mod_res == 0) {
        status = 8;
        goto end;
    }
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    environment.capacity = 4;
    environment.size = 0;
    environment.vars = HeapAlloc(heap, 0, 4 * sizeof(EnvVar));
    if (environment.vars == NULL) {
        status = 6;
        goto end;
    }

    DWORD lines;
    LPSTR paths_data = read_paths_file(L"Paths", L"txt", gBinpath, &lines);
    if (paths_data == NULL) {
        WriteConsole(err, L"Failed reading file\n", 20, NULL, NULL);
        status = 2;
        goto end;
    }

    if (read_envvar(L"Path", &environment) == NULL) {
        WriteConsole(err, L"Could not get Path\n", 19, NULL, NULL);
        status = 3;
        goto end;
    }
    DWORD arg_capacity = 20;
    LPWSTR arg_buffer = HeapAlloc(heap, 0, arg_capacity);

    BOOL changed = FALSE;

    StringBuffer buffer = {NULL, 0, 0};
    for (DWORD i = 1; i < argc; ++i) {
        DWORD len = wcslen(argv[i]);
        if (!WideStringToNarrow(argv[i], len, &buffer)) {
            continue;
        }
        OpStatus res = add_paths(paths_data, lines, buffer.ptr, argv[0]);
        if (res == OP_SUCCESS) {
            changed = TRUE;
        } else if (res != OP_NO_CHANGE) {
            status = res;

            HeapFree(heap, 0, buffer.ptr);
            HeapFree(heap, 0, paths_data);
            goto end;
        }
    }
    HeapFree(heap, 0, buffer.ptr);
    HeapFree(heap, 0, paths_data);
    if (!changed) {
        status = 1;
        goto end;
    }
    DWORD id = GetParentProcessId();
    if (id == 0) {
        WriteConsole(err, L"Could not get parent process\n", 29, NULL, NULL);
        status = 4;
        goto end;
    }

    for (DWORD i = 0; i < environment.size; ++i) {
        LPCWSTR name = environment.vars[i].name;
        LPCWSTR val = environment.vars[i].val.ptr;
        if (SetEnvironmentVariable(name, val)) {
            if (SetProcessEnvironmentVariable(name, val, id) == 0) {
                continue;
            }
        }
        _wprintf_h(err, L"Could not set %s to %s\n", name, val);
    }
end:
    free_env(&environment);
    HeapFree(heap, 0, add_buffer.ptr);
    FlushFileBuffers(out);
    FlushFileBuffers(err);
    HeapFree(heap, 0, expanded);
    HeapFree(heap, 0, argv);
    ExitProcess(status);
    return status;
}
