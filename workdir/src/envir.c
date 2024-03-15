#define UNICODE
#include <windows.h>
#include <tlhelp32.h>
#include "printf.h"
#include "args.h"
#include "path_utils.h"

/* CREATE_PROG asm in FASM
format binary
use64           ; 0                   8,            16             24,          32
; rcx: ptr to => (CreateFileMappingA, CreateMutexA, MappingHandle, MutexHandle, Null-terminated name string)
func:
    sub rsp, 64
    push rbx
    mov rbx, rcx

    xor rdx, rdx                    ; lpFileMappingAttributes = NULL
    lea rcx, [rdx - 1]              ; hFile = INVALID_HANDLE_VALUE (-1)
    mov r8, 0x4000004               ; flProtect = PAGE_READWRITE | SEC_RESERVE
    xor r9, r9                      ; dwMaximumSizeHigh = 0
    mov QWORD [rsp + 32], 16777216  ; dwMaximumSizeLow = 16 MB
    lea rax, [rbx + 32]
    mov QWORD [rsp + 40], rax       ; lpName = name
    call QWORD [rbx]                ; CreateFileMappingA(...)
	mov QWORD [rbx + 16], rax

	mov DWORD [rbx + 42], "Lock"
	xor rcx, rcx					; lpMutexAttributes = NULL
	xor rdx, rdx					; bInitialOwner = FALSE
	lea r8, [rbx + 32]				; lpName = Name
	call QWORD [rbx + 8]			; CreateMutexA(...)
	mov QWORD [rbx + 24], rax

    pop rbx
    add rsp, 64
    ret
*/

const unsigned char CREATE_PROG[] = {72, 131, 236, 64, 83, 72, 137, 203, 72, 49, 210, 72, 141, 74, 255, 73, 199, 192, 4, 0, 0, 4, 77, 49, 201, 72, 199, 68, 36, 32, 0, 0, 0, 1, 72, 141, 67, 32, 72, 137, 68, 36, 40, 255, 19, 72, 137, 67, 16, 199, 67, 42, 76, 111, 99, 107, 72, 49, 201, 72, 49, 210, 76, 141, 67, 32, 255, 83, 8, 72, 137, 67, 24, 91, 72, 131, 196, 64, 195};

typedef struct _CreateStruct {
    FARPROC create;
    FARPROC mutex;
    HANDLE hFile;
    HANDLE hMutex;
    char name[100];
} CreateStruct;

/* LOAD_PROG asm in FASM
format binary
use64            ; 0                 8              16          24               32                      40            44
; rcx: ptr to => (OpenFileMappingA, MapViewOfFile, CloseHandle, UnmapViewOfFile, SetEnvironmentStringsW, DWORD offset, Null-terminated name string)
func:
    sub rsp, 64
    push rbx
    mov rbx, rcx
    
    mov rcx, 0xf001f          ; dwDesiredAccess = FILE_MAP_ALL_ACCESS
    xor rdx, rdx              ; bInheritHandle = FALSE
    lea r8, [rbx + 44]        ; lpName = name
    call QWORD [rbx]          ; OpenFileMappingA(...)
    mov QWORD [rsp + 48], rax

    mov rcx, rax              ; hFileMappingObject = OpenFileMappingA(...)
    mov rdx, 0xf001f          ; dwDesiredAccess = FILE_MAP_ALL_ACCESS
    xor r8, r8                ; dwFileOffsetHigh = 0
    xor r9, r9                ; dwFileOffsetLow = 0
    mov QWORD [rsp + 32], r8  ; dwNumberOfBytesToMap = 0
    call QWORD [rbx + 8]      ; MapViewOfFile(...)
    mov QWORD [rsp + 56], rax

    mov edx, DWORD [rbx + 40]
    lea rcx, QWORD [rax + rdx]; NewEnvironment = MapViewOfFile(...) + offset
    call QWORD [rbx + 32]     ; SetEnvironmentStringsW(...)

    mov rcx, QWORD [rsp + 56] ; lpBaseAddress = MapViewOfFile(...)
    call QWORD [rbx + 24]     ; UnmapViewOfFile(...)

    mov rcx, QWORD [rsp + 48] ; hObject = OpenFileMappingA(...)
    call QWORD [rbx + 16]     ; CloseHandle(...)

    pop rbx
    add rsp, 64
    ret
*/
const unsigned char LOAD_PROG[] = {72, 131, 236, 64, 83, 72, 137, 203, 72, 199, 193, 31, 0, 15, 0, 72, 49, 210, 76, 141, 67, 44, 255, 19, 72, 137, 68, 36, 48, 72, 137, 193, 72, 199, 194, 31, 0, 15, 0, 77, 49, 192, 77, 49, 201, 76, 137, 68, 36, 32, 255, 83, 8, 72, 137, 68, 36, 56, 139, 83, 40, 72, 141, 12, 16, 255, 83, 32, 72, 139, 76, 36, 56, 255, 83, 24, 72, 139, 76, 36, 48, 255, 83, 16, 91, 72, 131, 196, 64, 195} ;

typedef struct _LoadStruct {
    FARPROC open;
    FARPROC map;
    FARPROC close;
    FARPROC unmap;
    FARPROC envfunc;
    DWORD offset;
    char name[100];
} LoadStruct;
/* FREE_PROG asm in FASM
format binary
use64	        ; 0            8              16
; rcx: ptr to => (CloseHandle, MappingHandle, MutexHandle)
func:
	sub rsp, 64
	push rbx
	mov rbx, rcx
	
	mov rcx, QWORD [rbx + 8]  ; hObject = MappingHandle
	call QWORD [rbx]		  ; CloseHandle(...)

	mov rcx, QWORD [rbx + 16] ; hObject = MutexHandle
	call QWORD [rbx]		  ; CloseHandle(...)

	pop rbx
	add rsp, 64
	ret
*/
const unsigned char FREE_PROG[] = {72, 131, 236, 64, 83, 72, 137, 203, 72, 139, 75, 8, 255, 19, 72, 139, 75, 16, 255, 19, 91, 72, 131, 196, 64, 195};

typedef struct _FreeStruct {
    FARPROC close;
    HANDLE hFile;
    HANDLE hMutex;
} FreeStruct;

#define MAX_CAPACITY 16777216 // 16 MB
                     
typedef struct _StackHeader {
    HANDLE file_handle; // Handle created by original process.
    HANDLE mutex_handle; // Handle created by original process.
    size_t stack_size; // Number of elements in the environment stack.
    size_t stack_usage; // Number of bytes of the mapping used.
    size_t stack_capacity; // Number of bytes of the mapping commited.
    void* reserved; // Potential future use.
    size_t stack[64]; // Offsets to environment strings.
} StackHeader;

int __CRTDECL _snwprintf_s(wchar_t* const, size_t, size_t,wchar_t const*, ...);
int __CRTDECL _snprintf_s(char* const, size_t, size_t, char const*, ...);



//header + 10 kb (should be enough to fit one environment strings instance).
#define INITIAL_COMMIT_SIZE (sizeof(StackHeader) + 10240)

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
                // Found the current process in the snapshot, return its parent process ID
                CloseHandle(hSnapshot);
                return pe32.th32ParentProcessID;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    // Process not found or unable to retrieve information
    CloseHandle(hSnapshot);
    return 0;
}

BOOL call_prog(HANDLE hProcess, const unsigned char* prog, size_t prog_size, void* data, size_t data_size) {
    unsigned char* mem = VirtualAllocEx(hProcess, NULL, prog_size + data_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (mem == NULL) {
        return FALSE;
    }
    WriteProcessMemory(hProcess, mem, prog, prog_size, NULL);
    WriteProcessMemory(hProcess, mem + prog_size, data, data_size, NULL);
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)mem, mem + prog_size, 0, NULL);
    if (hThread == NULL) {
        VirtualFreeEx(hProcess, mem, 0, MEM_RELEASE);
        return FALSE;
    }
    WaitForSingleObject(hThread, INFINITE);
    ReadProcessMemory(hProcess, mem + prog_size, data, data_size, NULL);
    VirtualFreeEx(hProcess, mem, 0, MEM_RELEASE);
    CloseHandle(hThread);
    return TRUE;
}

BOOL create_mapping_file(char* name, HANDLE hProcess, HANDLE* hFile, HANDLE* hMutex) {
    HMODULE kernel = LoadLibraryA("Kernel32.dll");
    CreateStruct d;
    d.create = GetProcAddress(kernel, "CreateFileMappingA");
    d.mutex = GetProcAddress(kernel, "CreateMutexA");
    memcpy(d.name, name, 100);
    BOOL res = call_prog(hProcess, CREATE_PROG, sizeof(CREATE_PROG), &d, sizeof(CreateStruct));
    FreeLibrary(kernel);
    *hFile = d.hFile;
    *hMutex = d.hMutex;
    return res;
}

BOOL load_environment(char* name, DWORD offset, HANDLE hProcess) {
    HMODULE kernel = LoadLibraryA("Kernel32.dll");
    LoadStruct s;
    s.open = GetProcAddress(kernel, "OpenFileMappingA");
    s.map = GetProcAddress(kernel, "MapViewOfFile");
    s.close = GetProcAddress(kernel, "CloseHandle");
    s.unmap = GetProcAddress(kernel, "UnmapViewOfFile");
    s.envfunc = GetProcAddress(kernel, "SetEnvironmentStringsW");
    s.offset = offset;
    memcpy(s.name, name, 100);
    BOOL res = call_prog(hProcess, LOAD_PROG, sizeof(LOAD_PROG), &s, sizeof(LoadStruct));
    FreeLibrary(kernel);
    return res;
}

BOOL free_environment(HANDLE hFile, HANDLE hMutex, HANDLE hProcess) {
    HMODULE kernel = LoadLibraryA("Kernel32.dll");
    FreeStruct f;
    f.close = GetProcAddress(kernel, "CloseHandle");
    f.hFile = hFile;
    f.hMutex = hMutex;
    BOOL res = call_prog(hProcess, FREE_PROG, sizeof(FREE_PROG), &f, sizeof(FreeStruct));
    FreeLibrary(kernel);
    return res;
}

wchar_t* GetSizedEnvStrings(size_t *size) {
    wchar_t* env = GetEnvironmentStringsW();
    size_t len = 2;
    while (env[len - 2] != L'\0' || env[len - 1] != L'\0') {
        ++len;
    }
    *size = len * sizeof(wchar_t);
    return env;
}

// Format of envheap file:
// Array of entries:
//  Size of entry (including name and size): 8 bytes
//  Null-terminated name string
//  The environment strings, of given size

DWORD GetEnvFile(wchar_t* buf, size_t len) {
    wchar_t bin_path[256];
    DWORD mod_res = GetModuleFileName(NULL, bin_path, 256);
    if (mod_res >= 256 || mod_res == 0) {
        return GetLastError();
    }
    wchar_t dir[256];
    wchar_t drive[10];
    _wsplitpath_s(bin_path, drive, 10, dir, 256, NULL, 0, NULL, 0);
    DWORD status;
    if ((status = _wmakepath_s(buf, len, drive, dir, L"envheap", NULL)) != 0) {
        return status;
    }

    return ERROR_SUCCESS;
}

DWORD FindEnvFileEntry(HANDLE file, const wchar_t* entry, size_t *len, wchar_t** data) {
    HANDLE heap = GetProcessHeap();
    size_t entry_len = wcslen(entry); 
    if (entry_len > 200) {
        return ERROR_INVALID_PARAMETER;
    }
    LARGE_INTEGER to_move, file_pointer;
    to_move.QuadPart = 0;
    if (!SetFilePointerEx(file, to_move, &file_pointer, FILE_BEGIN)) {
        return GetLastError();
    }
    DWORD read;
    size_t size;
    while (1) {
        if (!ReadFile(file, &size, sizeof(size_t), &read, NULL)) {
            return GetLastError();
        }
        if (read < sizeof(size_t)) {
            return ERROR_HANDLE_EOF; // End of file
        }
        for (size_t i = 0; i <= entry_len; ++i) {
            wchar_t c;
            if (!ReadFile(file, &c, sizeof(wchar_t), &read, NULL)) {
                return GetLastError();
            }
            if (read < sizeof(wchar_t)) {
                return ERROR_HANDLE_EOF;
            }
            if (towlower(c) != towlower(entry[i])) {
                break;
            }
            if (i == entry_len) {
                if (len != NULL) {
                    *len = size;
                } else if (data != NULL) {
                    *data = NULL;
                }
                if (len == NULL || data == NULL) {
                    if (!SetFilePointerEx(file, file_pointer, NULL, FILE_BEGIN)) {
                        return GetLastError();
                    }
                    return ERROR_SUCCESS;
                }
                size = size - sizeof(wchar_t) * (entry_len + 1) - sizeof(size_t);
                size_t total_read = 0;

                char* buf = HeapAlloc(GetProcessHeap(), 0, size);
                if (buf == NULL) {
                    return ERROR_OUTOFMEMORY;
                }
                while (total_read < size) {
                    if (!ReadFile(file, buf + total_read, size - total_read, &read, NULL)) {
                        HeapFree(GetProcessHeap(), 0, buf);
                        return GetLastError();
                    }
                    if (read == 0) {
                        HeapFree(GetProcessHeap(), 0, buf);
                        // Cannot return EOF, since that indicates entry not found.
                        return ERROR_BAD_LENGTH;
                    }
                    total_read += read;
                }
                if (!SetFilePointerEx(file, file_pointer, NULL, FILE_BEGIN)) {
                    HeapFree(GetProcessHeap(), 0, buf);
                    return GetLastError();
                }
                *data = (wchar_t*) buf;
                return ERROR_SUCCESS;
            }
        }
        to_move.QuadPart = file_pointer.QuadPart + size;
        if (!SetFilePointerEx(file, to_move, &file_pointer, FILE_BEGIN)) {
            return GetLastError();
        }
    }
}

DWORD ListEnvFile(const wchar_t* file_name) {
    HANDLE file = CreateFile(file_name,
                             GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == NULL) {
        return GetLastError();
    }
    BOOL allocated = FALSE;
    wchar_t buffer[100];
    wchar_t *buf = buffer;
    size_t capacity = 100;

    DWORD res = ERROR_SUCCESS;
    OVERLAPPED o;
    memset(&o, 0, sizeof(OVERLAPPED));
    if (!LockFileEx(file, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &o)) {
        CloseHandle(file);
        return GetLastError();
    }

    LARGE_INTEGER offset, fp;
    fp.QuadPart = 0;

    _printf("Saved environment entries:\n");
    DWORD count;
    while(1) {
        size_t entry_size;
        size_t name_size = 0;
        if (!ReadFile(file, &entry_size, sizeof(size_t), &count, NULL)) {
           res = GetLastError();
           goto end;
        }
        if (count != sizeof(size_t)) {
            break;
        }
        wchar_t c;
        do {
            if (!ReadFile(file, &c, sizeof(wchar_t), &count, NULL)) {
                res = GetLastError();
                goto end;
            }
            if (count != sizeof(wchar_t)) {
                res = ERROR_BAD_LENGTH;
                goto end;
            }
            if (name_size == capacity) {
                wchar_t* new_buf = HeapAlloc(GetProcessHeap(), 0, 2 * capacity);
                if (new_buf == NULL) {
                    res = ERROR_OUTOFMEMORY;
                    goto end;
                }
                memcpy(new_buf, buf, name_size);
                if (allocated) {
                    HeapFree(GetProcessHeap(), 0, buf);
                } else {
                    allocated = TRUE;
                }
                capacity = capacity * 2;
                buf = new_buf;
            }
            buf[name_size] = c;
            ++name_size;
        } while (c != L'\0');

        _printf(" %S\n", buf);
        offset.QuadPart = fp.QuadPart + entry_size;
        if (!SetFilePointerEx(file, offset, &fp, FILE_BEGIN)) {
            res = GetLastError();
            goto end;
        }
    }
end:
    if (allocated) {
        HeapFree(GetProcessHeap(), 0, buf);
    }
    UnlockFile(file, 0, 0, 1, 0);
    CloseHandle(file);
    return res;
}

DWORD WriteEnvFile(const wchar_t* file_name, const wchar_t* entry_name, BOOL remove, BOOL add) {
    HANDLE file = CreateFile(file_name,
                             GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == NULL) {
        return GetLastError();
    }
    OVERLAPPED o;
    memset(&o, 0, sizeof(OVERLAPPED));
    if (!LockFileEx(file, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &o)) {
        CloseHandle(file);
        return GetLastError();
    }

    size_t entry_size;
    LARGE_INTEGER offset, file_end;
    offset.QuadPart = 0;
    DWORD res;
    if (!SetFilePointerEx(file, offset, &file_end, FILE_END)) {
        res = GetLastError();
        goto end;
    }

    res = FindEnvFileEntry(file, entry_name, &entry_size, NULL);
    if (res == ERROR_SUCCESS) {
        if (!remove) {
            _printf_h(GetStdHandle(STD_ERROR_HANDLE), "Entry already exists\n");
            res = ERROR_SUCCESS;
            goto end;
        }
        remove = FALSE;
        char buf[1024];
        DWORD count = 0;
        while (1) {
            offset.QuadPart = entry_size;
            LARGE_INTEGER fp;
            if (!SetFilePointerEx(file, offset, &fp, FILE_CURRENT)) {
                res = GetLastError();
                goto end;
            }
            if (fp.QuadPart == file_end.QuadPart) {
                break;
            }
            if (fp.QuadPart > file_end.QuadPart) {
                res = ERROR_BAD_LENGTH;
                goto end;
            }
            if (!ReadFile(file, buf, 1024, &count, NULL)) {
                res = GetLastError();
                goto end;
            }
            offset.QuadPart = -entry_size - count;
            LARGE_INTEGER wo;
            if (!SetFilePointerEx(file, offset, &wo, FILE_CURRENT)) {
                res = GetLastError();
                goto end;
            }
            DWORD to_write = count;
            DWORD written = 0;
            while (written < to_write) {
                if (!WriteFile(file, buf + written, to_write - written, &count, NULL)) {
                    res = GetLastError();
                    goto end;
                }
                written += count;
            }
        
        }
        offset.QuadPart = -((long long)entry_size);
        if (!SetFilePointerEx(file, offset, &file_end, FILE_CURRENT) || !SetEndOfFile(file)) {
            res = GetLastError();
            goto end;
        }
    } else if (res != ERROR_HANDLE_EOF) {
        goto end;
    }

    if (!add) {
        if (remove) {
            _printf_h(GetStdHandle(STD_ERROR_HANDLE), "Entry '%S' not found\n", entry_name);
        }
        res = ERROR_SUCCESS;
        goto end; 
    }
    size_t len;
    wchar_t* env = GetSizedEnvStrings(&len);

    size_t name_len = (wcslen(entry_name) + 1) * sizeof(wchar_t);

    offset.QuadPart = 0;
    if (!SetFilePointerEx(file, offset, NULL, FILE_END)) {
        res = GetLastError();
        FreeEnvironmentStringsW(env);
        goto end;
    }

    DWORD written = 0;
    size_t total_len = len + name_len + sizeof(size_t);
    if (!WriteFile(file, &total_len, sizeof(size_t), &written, NULL) || written != sizeof(size_t)) {
        res = GetLastError();
        FreeEnvironmentStringsW(env);
        goto end;
    }
    written = 0;
    while (written < name_len) {
        DWORD w;
        if (!WriteFile(file, ((char*)entry_name) + written, name_len - written, &w, NULL)) {
            res = GetLastError();
            FreeEnvironmentStringsW(env);
            goto end;
        }
        written += w;
    }
    written = 0;
    while (written < len) {
        DWORD w;
        if (!WriteFile(file, ((char*)env) + written, len - written, &w, NULL)) {
            res = GetLastError();
            FreeEnvironmentStringsW(env);
            goto end;

        }
        written += w;
    }
    res = ERROR_SUCCESS;
    FreeEnvironmentStringsW(env);
end: 
    UnlockFile(file, 0, 0, 1, 0);
    CloseHandle(file);
    return res;
}


DWORD ReadEnvFile(const wchar_t* file_name, const wchar_t* entry_name, HANDLE process) {
    HANDLE file = CreateFile(file_name,
                             GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == NULL) {
        return GetLastError();
    }
    OVERLAPPED o;
    memset(&o, 0, sizeof(OVERLAPPED));
    if (!LockFileEx(file, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &o)) {
        CloseHandle(file);
        return GetLastError();
    }
    wchar_t* data;
    size_t len;
    DWORD res = FindEnvFileEntry(file, entry_name, &len, &data);
    UnlockFile(file, 0, 0, 1, 0);
    CloseHandle(file);
    if (res != ERROR_SUCCESS) {
        return res;
    }
    len = len - (wcslen(entry_name) + 1) * sizeof(wchar_t) - sizeof(size_t);
    unsigned char* mem = VirtualAllocEx(process, NULL, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (mem == NULL) {
        res = ERROR_OUTOFMEMORY;
        goto end;
    }

    if (!WriteProcessMemory(process, mem, data, len, NULL)) {
        res = GetLastError();
        VirtualFreeEx(process, mem, 0, MEM_RELEASE);
        goto end;
    }
    HMODULE kernel = LoadLibraryA("Kernel32.dll");
    FARPROC f = GetProcAddress(kernel, "SetEnvironmentStringsW");
    HANDLE t = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)f, mem, 0, NULL);
    if (t == NULL) {
        res = GetLastError();
        VirtualFreeEx(process, mem, 0, MEM_RELEASE);
        goto end;
    }
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
    VirtualFreeEx(process, mem, 0, MEM_RELEASE);
end:
    HeapFree(GetProcessHeap(), 0, data);
    return  res;
}


typedef enum _Action {
    STACK_PUSH, STACK_POP, STACK_SAVE, STACK_LOAD, STACK_CLEAR, STACK_SIZE, STACK_ID,
    STACK_FILE_LIST, STACK_FILE_SAVE, STACK_FILE_LOAD, STACK_FILE_REMOVE
} Action;

const char* help_message  = "usage: envir [--help] <commnand>\n\n"
                            "Saves or loads the environment from/to a stack local to the current terminal.\n\n"
                            "The following commands are available:\n\n"
                            "  push           Push the current environment onto the environment stack\n"
                            "  pop            Pop the top of the environment stack into the current environment\n"
                            "  save           Overwrite the top of the environment stack with the current environment,\n"
                            "                  does the same as push if the environment stack is empty\n"
                            "  load, l        Load the top of the environment stack into the current environment\n"
                            "                  without removing it\n"
                            "  clear, c       Remove all entries from the stack\n"
                            "  size           Print the size of the stack\n"
                            "  id, i          Print the id of the stack\n\n"
                            "The size of the stack is limited to 64 entries, after that push will fail.\n"
                            "The id in generated based on the process id of the parent process.\n";

int main() {
    LPWSTR args = GetCommandLine();
    HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
	int argc;
	int status = ERROR_SUCCESS;
	LPWSTR* argv = parse_command_line(args, &argc);

    HANDLE hProcess = NULL;
    HANDLE mapping = NULL;
    HANDLE mutex = NULL;
    DWORD wait_res = WAIT_FAILED;
    unsigned char* data = NULL;

    if (argc < 2) {
    	_printf_h(err, "Missing argument\n");
        _printf_h(err, "usage: envir [--help] <command>\n");
        status = ERROR_INVALID_PARAMETER;
        goto end;
    }
    if (find_flag(argv, &argc, L"-h", L"--help")) {
        _printf(help_message);
        goto end;
    }
    
    Action action;
    if (wcscmp(argv[1], L"push") == 0) {
        action = STACK_PUSH;
    } else if (wcscmp(argv[1], L"pop") == 0) {
        action = STACK_POP;
    } else if (wcscmp(argv[1], L"save") == 0) {
        action = argc >= 3 ? STACK_FILE_SAVE : STACK_SAVE;
    } else if (wcscmp(argv[1], L"load") == 0 || wcscmp(argv[1], L"l") == 0) {
        action = argc >= 3 ? STACK_FILE_LOAD : STACK_LOAD;
    } else if (wcscmp(argv[1], L"clear") == 0 || wcscmp(argv[1], L"c") == 0) {
        action = STACK_CLEAR;
    } else if (wcscmp(argv[1], L"size") == 0) {
        action = STACK_SIZE;
    } else if (wcscmp(argv[1], L"id") == 0 || wcscmp(argv[1], L"i") == 0) {
        action = STACK_ID;  
    } else if (wcscmp(argv[1], L"fs") == 0 ) {
        action = STACK_FILE_SAVE;
    } else if (wcscmp(argv[1], L"fl") == 0) {
        action = STACK_FILE_LOAD;
    } else if (wcscmp(argv[1], L"remove") == 0 || wcscmp(argv[1], L"r") == 0) {
        action = STACK_FILE_REMOVE;
    } else if (wcscmp(argv[1], L"list") == 0) {
        action = STACK_FILE_LIST;
    } else {
        _printf_h(err, "Invalid operation\n");
        status = ERROR_INVALID_OPERATION;
        goto end;
    }

    BOOL flag = FALSE;
    if (action == STACK_FILE_SAVE) {
        flag = find_flag(argv, &argc, L"-f", L"--force") > 0;
    }
    if (action >= STACK_FILE_SAVE && argc < 3) {
        _printf_h(err, "Missing argument, %d, %d\n", action, STACK_FILE_SAVE);
        status = ERROR_INVALID_PARAMETER;
        goto end;
    } else if ((action < STACK_FILE_SAVE && argc > 2) || argc > 3) {
        _printf_h(err, "Unexpected argument\n");
        status = ERROR_INVALID_PARAMETER;
        goto end;
    }

    DWORD parent = GetParentProcessId();
    if (parent == 0) {
        _printf_h(err, "Could not get parent process\n");
        status = ERROR_INVALID_STATE;
        goto end;
    }

    hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, parent);
    if (hProcess == NULL) {
        status = GetLastError();
        _printf_h(err, "Could not get parent process\n");
        goto end;
    }
    char file_name[100];
    char mutex_name[100];
    _snprintf_s(file_name, 100, 100, "Env-Stack-File-%d", parent);
    _snprintf_s(mutex_name, 100, 100, "Env-Stack-Lock-%d", parent);
    if (action == STACK_ID) {
        _printf("%s\n", file_name);
    }
    
    mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, file_name);
    BOOL created = FALSE;
    HANDLE owner_file;
    HANDLE owner_mutex;
    if (mapping == NULL) {
        if (!create_mapping_file(file_name, hProcess, &owner_file, &owner_mutex)) {
            status = GetLastError();
            _printf_h(err, "Could not create environment stack\n");
            goto end;
        }
        mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, file_name);
        if (mapping == NULL) {
            status = GetLastError();
            _printf_h(err, "Could access environment stack\n");
            goto end;
        }
        created = TRUE;
    }
    data = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (data == NULL) {
        status = GetLastError();
        _printf_h(err, "Could not access environment stack\n");
        goto end;
    }
    
    mutex = OpenMutexA(SYNCHRONIZE, FALSE, mutex_name);
    if (mutex == NULL) {
        status = GetLastError();
        _printf_h(err, "Could not access environment mutex\n");
        goto end;
    }
    wait_res = WaitForSingleObject(mutex, INFINITE);
    if (wait_res != WAIT_OBJECT_0) {
        if (wait_res == WAIT_ABANDONED) {
            // Mark stack as corrupted. This might be saved later.
            ((StackHeader*)data)->stack_size = 0xffffffffffffffff;
        } else {
            status = GetLastError();
            _printf_h(err, "Could not lock environment stack\n");
            goto end;
        }
    }

    if (created) {
        if (!VirtualAlloc(data, INITIAL_COMMIT_SIZE, MEM_COMMIT, PAGE_READWRITE)) {
            status = GetLastError();
            _printf_h(err, "Out of memory\n");
            goto end;
        }
        StackHeader* header = (StackHeader*)data;
        header->file_handle = owner_file;
        header->mutex_handle = owner_mutex;
        header->stack_size = 0;
        header->stack_usage = sizeof(StackHeader);
        header->stack_capacity = INITIAL_COMMIT_SIZE;
    }
    
    char stack_name[100];
    BOOL rename_file = TRUE;
    DWORD env_res = GetEnvironmentVariableA("ENV_STACK_FILE_NAME", stack_name, 100);
    if (env_res > 0 && env_res < 100) {
        if (strcmp(file_name, stack_name) != 0) {
            char lock_name[100];
            memcpy(lock_name, stack_name, 100);
            memcpy(lock_name + 10, "Lock", 4);
            HANDLE old_mutex = OpenMutexA(SYNCHRONIZE, FALSE, lock_name);
            if (old_mutex != NULL) {
                // NOTE: mutex is not released if WAIT_ABANDONED is returned.
                // This will cause it to be abandoned again, making sure no other process uses it.
                DWORD old_wait_res = WaitForSingleObject(old_mutex, 2000); // Max 2 seconds
                if (old_wait_res == WAIT_OBJECT_0) {
                    HANDLE old_mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, stack_name);
                    if (old_mapping != NULL) {
                        unsigned char* old_data = MapViewOfFile(old_mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
                        if (old_data != NULL) {
                            // To avoid aliasing issues, data is not cast to StackHeader*...
                            StackHeader* old_header = (StackHeader*) old_data;
                            if (old_header->stack_size <= 64) {
                                size_t usage = old_header->stack_usage;
                                if (usage < INITIAL_COMMIT_SIZE) {
                                    usage = INITIAL_COMMIT_SIZE;
                                }
                                if (VirtualAlloc(data, usage, MEM_COMMIT, PAGE_READWRITE)) {
                                    size_t offset = 2 * sizeof(HANDLE);
                                    memcpy(data + offset, old_data + offset, old_header->stack_usage - offset);
                                    memcpy(data + offsetof(StackHeader, stack_capacity), &usage, sizeof(size_t));
                                }
                            }
                            UnmapViewOfFile(old_data);
                        }
                        CloseHandle(old_mapping);
                    }
                    ReleaseMutex(old_mutex);
                }
                CloseHandle(old_mutex);
            }
        } else {
            rename_file = FALSE;
        }
    }

    StackHeader* header = (StackHeader*)data;
    size_t env_stack_length = header->stack_size;

    if (env_stack_length > 64 && action != STACK_CLEAR) {
        status = ERROR_INVALID_STATE;
        _printf_h(err, "Corrupt environment stack\n");
        goto end;
    }
    
    if (action >= STACK_FILE_LIST) {
        // The environment file uses file locking. Release the mutex
        // so that it does not get abandoned in case IO hangs and user presses Ctrl-C
        ReleaseMutex(mutex);
        CloseHandle(mutex);
        mutex = NULL;

        wchar_t env_file[256];
        if ((status = GetEnvFile(env_file, 256)) != 0) {
            _printf_h(err, "Failed finding environment file\n");
            goto end;
        }
        if (action == STACK_FILE_SAVE || action == STACK_FILE_REMOVE) {
            if ((status = WriteEnvFile(env_file, argv[2], action == STACK_FILE_REMOVE || flag, action == STACK_FILE_SAVE)) != 0) {
                _printf_h(err, "Failed writing to environment file\n");
                goto end;
            }
        } else if (action == STACK_FILE_LOAD) {
            status = ReadEnvFile(env_file, argv[2], hProcess);
            if (status == ERROR_HANDLE_EOF) {
                _printf_h(err, "Entry '%S' not found\n", argv[2]);
                goto end;
            } else if (status != 0) {
                _printf_h(err, "Failed loading environment\n");
                goto end;
            }
        } else {
            status = ListEnvFile(env_file);
        }
    } else if (action == STACK_SAVE || action == STACK_PUSH) {
        if (action == STACK_PUSH || env_stack_length == 0) {
            if (env_stack_length == 64) {
                _printf_h(err, "Stack is full\n");
                status = ERROR_INVALID_FUNCTION;
                goto end;
            }
            header->stack[env_stack_length] = header->stack_usage;
            env_stack_length += 1;
        }
        size_t len;
        wchar_t* env = GetSizedEnvStrings(&len);
        size_t stack_usage = header->stack[env_stack_length - 1];
        if (header->stack_capacity - stack_usage < len) {
            size_t to_alloc = len < 4096 ? 4096 : len; // VirtualAlloc commits whole pages anyway.
            if (header->stack_capacity + to_alloc > MAX_CAPACITY) {
                _printf_h(err, "Memory usage exceded capacity\n");
                status = ERROR_NOT_ENOUGH_MEMORY;
                goto end;
            }
            if (!VirtualAlloc(data + header->stack_capacity, to_alloc, MEM_COMMIT, PAGE_READWRITE)) {
                status = GetLastError();
                _printf_h(err, "Out of memory\n");
                goto end;
            }
            header->stack_capacity += to_alloc;
        }
        memcpy(data + header->stack[env_stack_length - 1], env, len);
        header->stack_usage = stack_usage + len;
        FreeEnvironmentStringsW(env);
    } else if (action == STACK_LOAD || action == STACK_POP) {
        if (env_stack_length == 0) {
            _printf_h(err, "Stack is empty\n");
            status = ERROR_INVALID_FUNCTION;
            goto end;
        }
        if (!load_environment(file_name, header->stack[env_stack_length - 1], hProcess)) {
            status = GetLastError();
            _printf_h(err, "Failed loading environment\n");
            header->stack_size = 0xffffffffffffffff;
            goto end;
        }
        if (action == STACK_POP) {
            env_stack_length -= 1;
            header->stack_usage = header->stack[env_stack_length];
        }
    } else if (action == STACK_CLEAR) {
        if (!free_environment(header->file_handle, header->mutex_handle, hProcess)) {
            status = GetLastError();
            _printf_h(err, "Failed clearing environment\n");
            header->stack_size = 0xffffffffffffffff;
            goto end;
        }
        env_stack_length = 0;
        header->stack_usage = sizeof(StackHeader);
    } else if (action == STACK_SIZE) {
        _printf("Stack size: %llu\n", env_stack_length);
    } else if (action == STACK_ID) {
        _printf("Stack size: %llu\n", header->stack_size);
        _printf("Stack usage: %llu\n", header->stack_usage);
        _printf("Stack capacity: %llu\n", header->stack_capacity);
    }

    header->stack_size = env_stack_length;
    if (action == STACK_CLEAR || action == STACK_LOAD || action == STACK_POP || rename_file) {
        wchar_t wide_name[100];
        _snwprintf_s(wide_name, 100, 100, L"Env-Stack-File-%d", parent);
        SetProcessEnvironmentVariable(L"ENV_STACK_FILE_NAME", wide_name, parent);
    }
end:
    if (mutex != NULL) {
        if (wait_res == WAIT_OBJECT_0 || wait_res == WAIT_ABANDONED) {
            ReleaseMutex(mutex);
        }
        CloseHandle(mutex);
    }

    if (data != NULL) {
        UnmapViewOfFile(data);
    }
    if (mapping != NULL) {
        CloseHandle(mapping);
    }
    if (hProcess != NULL) {
        CloseHandle(hProcess);
    }
    HeapFree(GetProcessHeap(), 0, argv);
    return status;
}

