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

typedef enum _Action {
    STACK_PUSH, STACK_POP, STACK_SAVE, STACK_LOAD, STACK_CLEAR, STACK_SIZE, STACK_ID
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
        action = STACK_SAVE;
    } else if (wcscmp(argv[1], L"load") == 0 || wcscmp(argv[1], L"l") == 0) {
        action = STACK_LOAD;
    } else if (wcscmp(argv[1], L"clear") == 0 || wcscmp(argv[1], L"c") == 0) {
        action = STACK_CLEAR;
    } else if (wcscmp(argv[1], L"size") == 0) {
        action = STACK_SIZE;
    } else if (wcscmp(argv[1], L"id") == 0 || wcscmp(argv[1], L"i") == 0) {
        action = STACK_ID;  
    } else {
        _printf_h(err, "Invalid operation\n");
        status = ERROR_INVALID_OPERATION;
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
                                    memcpy(data, old_data, old_header->stack_usage);
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

    if (env_stack_length > 64) {
        status = ERROR_INVALID_STATE;
        _printf_h(err, "Corrupt environment stack\n");
        goto end;
    }

    if (action == STACK_SAVE || action == STACK_PUSH) {
        if (action == STACK_PUSH || env_stack_length == 0) {
            if (env_stack_length == 64) {
                _printf_h(err, "Stack is full\n");
                status = ERROR_INVALID_FUNCTION;
                goto end;
            }
            header->stack[env_stack_length] = header->stack_usage;
            env_stack_length += 1;
        }
        wchar_t* env = GetEnvironmentStringsW();
        size_t len = 2;
        while (env[len - 2] != L'\0' || env[len - 1] != L'\0') {
            ++len;
        }
        len = len * sizeof(wchar_t);
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
