#define UNICODE
#include "path_utils.h"

BOOL get_envvar(LPCWSTR name, DWORD hint, WString *res) {
    BOOL free = FALSE;
    if (res->buffer == NULL) {
        free = TRUE;
        if (!WString_create_capacity(res, res->capacity + hint)) {
            return FALSE;
        }
    }

    DWORD size = GetEnvironmentVariableW(name, res->buffer, res->capacity);
    if (size == 0) {
        int err = GetLastError();
        if (err == ERROR_ENVVAR_NOT_FOUND || err == 0) {
            res->length = 0;
            res->buffer[0] = '\0';
            return TRUE;
        }
        if (free) {
            WString_free(res);
        }
        return FALSE;
    }

    if (size > res->capacity) {
        DWORD capacity = size + hint;
        WString temp;
        if (free) {
            WString_free(res);
        }
        if (!WString_create_capacity(&temp, capacity)) {
            return FALSE;
        }
        size = GetEnvironmentVariableW(name, temp.buffer, capacity);
        if (size == 0) {
            WString_free(&temp);
            return FALSE;
        }
        if (!free) {
            WString_free(res);
        }
        *res = temp;
    }
    res->length = size;

    return TRUE;
}

WString *read_envvar(LPCWSTR name, EnvBuffer *env) {
    for (DWORD i = 0; i < env->size; ++i) {
        if (_wcsicmp(name, env->vars[i].name) == 0) {
            return &(env->vars[i].val);
        }
    }
    HANDLE heap = GetProcessHeap();
    if (env->size == env->capacity) {
        EnvVar *buf = HeapAlloc(heap, 0, env->capacity * 2 * sizeof(EnvVar));
        if (buf == NULL) {
            return NULL;
        }
        memcpy(buf, env->vars, env->size * sizeof(EnvVar));
        env->vars = buf;
        env->capacity = env->capacity * 2;
    }

    env->vars[env->size].val.buffer = NULL;
    env->vars[env->size].val.capacity = 100;
    env->vars[env->size].val.length = 0;
    if (get_envvar(name, 0, &(env->vars[env->size].val))) {
        DWORD len = (wcslen(name) + 1) * sizeof(WCHAR);
        LPWSTR new_name = HeapAlloc(heap, 0, len);
        if (new_name == NULL) {
            return NULL;
        }
        memcpy(new_name, name, len);
        env->vars[env->size].name = new_name;
        env->size += 1;
        return &(env->vars[env->size - 1].val);
    }

    return NULL;
}

void free_env(EnvBuffer *env) {
    HANDLE heap = GetProcessHeap();
    for (DWORD i = 0; i < env->size; ++i) {
        WString_free(&env->vars[i].val);
        HeapFree(heap, 0, (LPWSTR)env->vars[i].name);
    }
    HeapFree(heap, 0, env->vars);
    env->capacity = 0;
    env->size = 0;
}

LPWSTR expanded = NULL;
DWORD expanded_capacity = 0;

LPWSTR contains(LPWSTR path, LPCWSTR dir) {
    WCHAR dir_endc = L'\0';
    if (dir[0] == L'"') {
        dir_endc = L'"';
        ++dir;
    }

    while (1) {
        if (path[0] == L'\0') {
            return NULL;
        }
        LPWSTR pos = path;
        WCHAR endc = L';';
        if (path[0] == L'"') {
            endc = L'"';
            ++path;
        }
        for (unsigned i = 0; TRUE; ++path, ++i) {
            if (dir[i] == L'\0' || dir[i] == dir_endc) {
                if (path[0] == L'\0' || path[0] == endc) {
                    return pos;
                }
                if (path[0] == L'\\' && (path[1] == L'\0' || path[1] == endc)) {
                    return pos;
                }
                break;
            }
            if (path[0] == L'\0' || path[0] == endc) {
                if (dir[i] == L'\0' || dir[i] == dir_endc) {
                    return pos;
                }
                if (dir[i] == L'\\' &&
                    (dir[i + 1] == L'\0' || dir[i + 1] == dir_endc)) {
                    return pos;
                }
                break;
            }
            if (towlower(path[0]) != towlower(dir[i])) {
                break;
            }
        }
        while (path[0] != L';') {
            if (path[0] == L'\0') {
                return NULL;
            }
            ++path;
        }
        ++path;
    }
}

PathStatus validate(LPWSTR path) {
    int index = 0;
    BOOL needs_quotes = FALSE;
    BOOL has_quotes = FALSE;
    while (path[index] != L'\0') {
        if (path[index] == L';') {
            needs_quotes = TRUE;
        }
        if (path[index] < L' ') {
            return PATH_INVALID;
        }
        for (int j = 0; j < 5; ++j) {
            if (path[index] == L"?|<>*"[j]) {
                return PATH_INVALID;
            }
        }
        if (path[index] == L'/') {
            path[index] = L'\\';
        }

        if (path[index] == L'\\' &&
            (path[index + 1] == L'\\' || path[index + 1] == L'/')) {
            return PATH_INVALID;
        }
        if (path[index] == L':' &&
            ((has_quotes && index != 2) || (!has_quotes && index != 1))) {
            return PATH_INVALID;
        } else if (path[index] == L'"') {
            if (index != 0 && path[index + 1] != L'\0') {
                return PATH_INVALID;
            }
            if (index == 0) {
                has_quotes = TRUE;
            } else if (!has_quotes) {
                return PATH_INVALID;
            }
        }
        ++index;
    }
    if (index == 0 || has_quotes && index <= 2) {
        return PATH_INVALID;
    }
    if (has_quotes && path[index - 1] != L'"') {
        return PATH_INVALID;
    }
    if (index >= 2 && path[1] == L':') {
        if (!((path[0] >= L'a' && path[0] <= L'z') ||
              (path[0] >= L'A' && path[0] <= L'Z'))) {
            return PATH_INVALID;
        }
        if (path[2] != L'\\') {
            return PATH_INVALID;
        }
    }
    if (path[index - 1] == L'\\') {
        path[index - 1] = L'\0';
    } else if (has_quotes && path[index - 2] == L'\\') {
        path[index - 2] = L'"';
        path[index - 1] = L'\0';
    }
    return needs_quotes && !has_quotes ? PATH_NEEDS_QUOTES : PATH_VALID;
}

OpStatus path_prune(PathBuffer *path) {
    PathBuffer res;
    WString_create_capacity(&res, path->capacity);

    LPWSTR work_buf = HeapAlloc(GetProcessHeap(), 0, 256 * sizeof(WCHAR));
    unsigned work_cap = 256;

    unsigned i = 0;

    OpStatus final_status = OP_NO_CHANGE;
    while (path->buffer[i] != L'\0') {
        WCHAR endc = L';';
        if (path->buffer[i] == L'"') {
            endc = L'"';
            ++i;
        }
        unsigned begin = i;
        while (1) {
            if (path->buffer[i] == L'\0' || path->buffer[i] == endc) {
                break;
            }
            ++i;
        }

        unsigned size = i - begin;
        if (size + 1 > work_cap) {
            HeapFree(GetProcessHeap(), 0, work_buf);
            work_buf =
                HeapAlloc(GetProcessHeap(), 0, (size + 1) * sizeof(WCHAR));
            work_cap = size + 1;
            if (work_buf == NULL) {
                WString_free(&res);
                return OP_OUT_OF_MEMORY;
            }
        }
        memcpy(work_buf, path->buffer + begin, size * sizeof(WCHAR));
        work_buf[size] = L'\0';
        PathStatus path_status = validate(work_buf);

        if (path_status != PATH_INVALID) {
            if (res.length == 0) {
                if (size + 2 > res.capacity) {
                    res.capacity = size + 2;
                    HeapFree(GetProcessHeap(), 0, res.buffer);
                    res.buffer = HeapAlloc(GetProcessHeap(), 0,
                                           res.capacity * sizeof(WCHAR));
                }
                res.length = size + 1;
                memcpy(res.buffer, work_buf, size * sizeof(WCHAR));
                res.buffer[size] = L';';
                res.buffer[size + 1] = '\0';
            } else {
                OpStatus status = path_add(work_buf, &res, FALSE, FALSE);
                if (status > OP_NO_CHANGE) {
                    HeapFree(GetProcessHeap(), 0, work_buf);
                    HeapFree(GetProcessHeap(), 0, res.buffer);
                    return status;
                }
                if (status == OP_SUCCESS) {
                    final_status = OP_SUCCESS;
                }
            }
        }

        while (path->buffer[i] != L';') {
            if (path->buffer[i] == L'\0') {
                break;
            }
            ++i;
        }
        ++i;
    }
    HeapFree(GetProcessHeap(), 0, work_buf);
    WString_free(path);
    path->buffer = res.buffer;
    path->length = res.length;
    path->capacity = res.capacity;

    return final_status;
}

OpStatus path_remove(LPWSTR arg, PathBuffer *path_buffer, BOOL expand) {
    LPWSTR path;
    if (expand) {
        OpStatus status = expand_path(arg, &expanded, &expanded_capacity);
        path = expanded;
        if (status != OP_SUCCESS) {
            return status;
        }
    } else {
        path = arg;
    }

    DWORD status = validate(path);
    if (status == PATH_INVALID) {
        return OP_INVALID_PATH;
    }
    LPWSTR pos = contains(path_buffer->buffer, path);
    if (pos == NULL) {
        return OP_NO_CHANGE;
    }
    DWORD index = (DWORD)(pos - path_buffer->buffer);
    WCHAR endc = pos[0] == L'"' ? L'"' : L';';
    DWORD len = 0;
    while (pos[len] != endc && pos[len] != L'\0') {
        ++len;
    }
    while (pos[len] != L';' && pos[len] != L'\0') {
        ++len;
    }
    if (pos[len] == L';') {
        ++len;
    }
    DWORD size = path_buffer->length - index - len + 1;
    memmove((path_buffer->buffer) + index, path_buffer->buffer + index + len,
            size * sizeof(WCHAR));
    path_buffer->length -= len;
    return OP_SUCCESS;
}

OpStatus path_add(LPWSTR arg, PathBuffer *path_buffer, BOOL before,
                  BOOL expand) {
    LPWSTR path;
    if (expand) {
        OpStatus status = expand_path(arg, &expanded, &expanded_capacity);
        path = expanded;
        if (status != OP_SUCCESS) {
            return status;
        }
    } else {
        path = arg;
    }
    DWORD status = validate(path);
    if (status == PATH_INVALID) {
        return OP_INVALID_PATH;
    }
    if (contains(path_buffer->buffer, path)) {
        return OP_NO_CHANGE;
    }

    DWORD size = 0;
    while (path[size]) {
        ++size;
    }
    DWORD extra_size = status == PATH_NEEDS_QUOTES ? 3 : 1;
    if (!WString_reserve(path_buffer, path_buffer->length + extra_size + size)) {
        return OP_OUT_OF_MEMORY;
    }
    if (before) {
        memmove(path_buffer->buffer + extra_size + size, path_buffer->buffer,
                (path_buffer->length + 1) * sizeof(wchar_t));
        if (status == PATH_NEEDS_QUOTES) {
            memcpy(path_buffer->buffer + 1, path, size * sizeof(wchar_t));
            path_buffer->buffer[0] = L'"';
            path_buffer->buffer[size + 1] = L'"';
            path_buffer->buffer[size + 2] = L';';
        } else {
            memcpy(path_buffer->buffer, path, size * sizeof(wchar_t));
            path_buffer->buffer[size] = L';';
        }
        path_buffer->length += size + extra_size;
    } else {
        if (path_buffer->length == 0 ||
            path_buffer->buffer[path_buffer->length - 1] != L';') {
            WString_append(path_buffer, L';');
        }
        if (status == PATH_NEEDS_QUOTES) {
            WString_append(path_buffer, L'"');
            WString_append_count(path_buffer, path, size);
            WString_append(path_buffer, L'"');
        } else {
            WString_append_count(path_buffer, path, size);
        }
        if (!WString_append(path_buffer, L';')) {
            return OP_OUT_OF_MEMORY;
        }
    }
    return OP_SUCCESS;
}

OpStatus expand_path(LPWSTR path, LPWSTR *dest, DWORD *dest_cap) {
    HANDLE heap = GetProcessHeap();
    if (path[0] == L'"') {
        ++path;
        DWORD index = 1;
        while (path[index] != L'"' && path[index] != L'\0') {
            ++index;
        }
        path[index] = L'\0';
    }
    DWORD expanded_size = GetFullPathNameW(path, *dest_cap, *dest, NULL);
    if (expanded_size == 0) {
        return OP_INVALID_PATH;
    }
    if (expanded_size > *dest_cap) {
        HeapFree(heap, 0, *dest);
        *dest = HeapAlloc(heap, 0, (expanded_size + 1) * sizeof(WCHAR));
        if (expanded == NULL) {
            *dest_cap = 0;
            return OP_OUT_OF_MEMORY;
        }
        *dest_cap = expanded_size + 1;
        DWORD res = GetFullPathNameW(path, *dest_cap, *dest, NULL);
        if (!res) {
            return OP_INVALID_PATH;
        }
    }
    return OP_SUCCESS;
}

/* Contains the following asm:
                sub rsp, 72
        mov rax, rcx
        lea rcx, [var]
        lea rdx, [val]
        call rax
        add rsp, 72
        ret
    var:
        db 0, 0, ... Space for 50 WCHARs in variable name
        val:
*/
const unsigned char INJECT_BASE[] = {
    72, 131, 236, 72, 72, 137, 200, 72, 141, 13,  14, 0,   0, 0, 72, 141,
    21, 107, 0,   0,  0,  255, 208, 72, 131, 196, 72, 195, 0, 0, 0,  0,
    0,  0,   0,   0,  0,  0,   0,   0,  0,   0,   0,  0,   0, 0, 0,  0,
    0,  0,   0,   0,  0,  0,   0,   0,  0,   0,   0,  0,   0, 0, 0,  0,
    0,  0,   0,   0,  0,  0,   0,   0,  0,   0,   0,  0,   0, 0, 0,  0,
    0,  0,   0,   0,  0,  0,   0,   0,  0,   0,   0,  0,   0, 0, 0,  0,
    0,  0,   0,   0,  0,  0,   0,   0,  0,   0,   0,  0,   0, 0, 0,  0,
    0,  0,   0,   0,  0,  0,   0,   0,  0,   0,   0,  0,   0, 0, 0,  0};
const unsigned VAR_START = 28;
const unsigned VAL_START = 128;

LPVOID get_setter_func(LPCWSTR var, LPCWSTR val, HANDLE process) {
    unsigned var_len = wcslen(var) * sizeof(WCHAR);
    unsigned val_len = (wcslen(val) + 1) * sizeof(WCHAR);
    if (var_len >= 100) {
        return NULL;
    }
    unsigned char *func =
        VirtualAllocEx(process, NULL, sizeof(INJECT_BASE) + val_len,
                       MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (func == NULL) {
        return NULL;
    }
    if (!WriteProcessMemory(process, func, INJECT_BASE, sizeof(INJECT_BASE),
                            NULL)) {
        VirtualFreeEx(process, func, 0, MEM_RELEASE);
        return NULL;
    }
    if (!WriteProcessMemory(process, func + VAR_START, var, var_len, NULL)) {
        VirtualFreeEx(process, func, 0, MEM_RELEASE);
        return NULL;
    }
    if (!WriteProcessMemory(process, func + VAL_START, val, val_len, NULL)) {
        VirtualFreeEx(process, func, 0, MEM_RELEASE);
        return NULL;
    }
    return func;
}

int SetProcessEnvironmentVariable(LPCWSTR var, LPCWSTR val, DWORD processId) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (hProcess == NULL) {
        return 1;
    }

    LPTHREAD_START_ROUTINE func = get_setter_func(var, val, hProcess);
    if (func == NULL) {
        CloseHandle(hProcess);
        return 2;
    }

    LPVOID loadLibraryAddr = (LPVOID)GetProcAddress(
        GetModuleHandle(L"kernel32.dll"), "SetEnvironmentVariableW");
    if (loadLibraryAddr == NULL) {
        VirtualFreeEx(hProcess, func, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 3;
    }

    HANDLE hThread =
        CreateRemoteThread(hProcess, NULL, 0, func, loadLibraryAddr, 0, NULL);
    if (hThread == NULL) {
        VirtualFreeEx(hProcess, func, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 4;
    }

    WaitForSingleObject(hThread, INFINITE);

    VirtualFreeEx(hProcess, func, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProcess);
    return 0;
}
