#define UNICODE
#include <windows.h>
#include <tlhelp32.h>
#include "printf.h"
#include "args.h"

/* CREATE_PROG asm in FASM
format binary
use64           ; 0                   8
; rcx: ptr to => (CreateFileMappingA, Null-terminated name string)
func:
    sub rsp, 48
    push rbx
    mov rbx, rcx
	
    xor rdx, rdx                    ; lpFileMappingAttributes = Null
    lea rcx, [rdx - 1]              ; hFile = INVALID_HANDLE_VALUE (-1)
    mov r8, 0x04                    ; flProtect = PAGE_READWRITE
    xor r9, r9                      ; dwMaximumSizeHigh = 0
    mov QWORD [rsp + 32], 512 + 8   ; dwMaximumSizeLow = 512 + 8
    lea rax, [rbx + 8]
    mov QWORD [rsp + 40], rax       ; lpName = name
    call QWORD [rbx]                ; CreateFileMappingA(...)

    pop rbx
    add rsp, 48
    ret
*/

const unsigned char CREATE_PROG[] = {72, 131, 236, 48, 83, 72, 137, 203, 72, 49, 210, 72, 141, 74, 255, 73, 199, 192, 4, 0, 0, 0, 77, 49, 201, 72, 199, 68, 36, 32, 8, 2, 0, 0, 72, 141, 67, 8, 72, 137, 68, 36, 40, 255, 19, 91, 72, 131, 196, 48, 195};

typedef struct _CreateStruct {
    FARPROC create;
    char name[100];
} CreateStruct;

/* STORE_PROG asm in FASM
format binary
use64	        ; 0                 8              16           24               32                      40            44
; rcx: ptr to => (OpenFileMappingA, MapViewOfFile, CloseHandle, UnmapViewOfFile, GetEnvironmentStringsW, DWORD offset, Null-terminated name string)
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
    mov QWORD [rsp + 56] , rax
	
    call QWORD [rbx + 32]     ; GetEnvironmentStringsW(...)
    mov rcx, QWORD [rsp + 56]
    mov edx, DWORD [rbx + 40]
    mov QWORD [rcx + rdx], rax

    call QWORD [rbx + 24]     ; UnmapViewOfFile(...)

    mov rcx, QWORD [rsp + 48] ; hObject = OpenFileMappingA(...)
    call QWORD [rbx + 16]     ; CloseHandle(...)

    pop rbx
    add rsp, 64
    ret
*/
const unsigned char STORE_PROG[] = {72, 131, 236, 64, 83, 72, 137, 203, 72, 199, 193, 31, 0, 15, 0, 72, 49, 210, 76, 141, 67, 44, 255, 19, 72, 137, 68, 36, 48, 72, 137, 193, 72, 199, 194, 31, 0, 15, 0, 77, 49, 192, 77, 49, 201, 76, 137, 68, 36, 32, 255, 83, 8, 72, 137, 68, 36, 56, 255, 83, 32, 72, 139, 76, 36, 56, 139, 83, 40, 72, 137, 4, 17, 255, 83, 24, 72, 139, 76, 36, 48, 255, 83, 16, 91, 72, 131, 196, 64, 195};

/* LOAD_PROG / FREE_PROG asm in FASM
format binary
use64            ; 0                 8              16           24               32                      40            44
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
    mov rcx, QWORD [rax + rdx]; NewEnvironment = MapViewOfFile(...)[offset]
    call QWORD [rbx + 32]     ; SetEnvironmentStringsW(...) / FreeEnvironmentStringsW(...)

    mov rcx, QWORD [rsp + 56] ; lpBaseAddress = MapViewOfFile(...)
    call QWORD [rbx + 24]     ; UnmapViewOfFile(...)

    mov rcx, QWORD [rsp + 48] ; hObject = OpenFileMappingA(...)
    call QWORD [rbx + 16]     ; CloseHandle(...)

    pop rbx
    add rsp, 64
    ret
*/
const unsigned char LOAD_PROG[] = {72, 131, 236, 64, 83, 72, 137, 203, 72, 199, 193, 31, 0, 15, 0, 72, 49, 210, 76, 141, 67, 44, 255, 19, 72, 137, 68, 36, 48, 72, 137, 193, 72, 199, 194, 31, 0, 15, 0, 77, 49, 192, 77, 49, 201, 76, 137, 68, 36, 32, 255, 83, 8, 72, 137, 68, 36, 56, 139, 83, 40, 72, 139, 12, 16, 255, 83, 32, 72, 139, 76, 36, 56, 255, 83, 24, 72, 139, 76, 36, 48, 255, 83, 16, 91, 72, 131, 196, 64, 195} ;
// Due to FreeEnvironmentStringsW and SetEnvironmentStringsW having the same signature, FREE_PROG and LOAD_PROG are identical
#define FREE_PROG LOAD_PROG

typedef struct _EnvStruct {
    FARPROC open;
    FARPROC map;
    FARPROC close;
    FARPROC unmap;
    FARPROC envfunc;
    DWORD offset;
    char name[100];
} EnvStruct;

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

BOOL call_prog(HANDLE hProcess, const unsigned char* prog, size_t prog_size, const void* data, size_t data_size) {
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
    VirtualFreeEx(hProcess, mem, 0, MEM_RELEASE);
    CloseHandle(hThread);
    return TRUE;
}

BOOL create_mapping_file(char* name, HANDLE hProcess) {
    HMODULE kernel = LoadLibraryA("Kernel32.dll");
    CreateStruct d;
    d.create = GetProcAddress(kernel, "CreateFileMappingA");
    memcpy(d.name, name, 100);
    BOOL res = call_prog(hProcess, CREATE_PROG, sizeof(CREATE_PROG), &d, sizeof(CreateStruct));
    FreeLibrary(kernel);
    return res;
}

BOOL store_environment(char* name, HANDLE hProcess, DWORD offset) {
    HMODULE kernel = LoadLibraryA("Kernel32.dll");
    EnvStruct s;
    s.open = GetProcAddress(kernel, "OpenFileMappingA");
    s.map = GetProcAddress(kernel, "MapViewOfFile");
    s.close = GetProcAddress(kernel, "CloseHandle");
    s.unmap = GetProcAddress(kernel, "UnmapViewOfFile");
    s.envfunc = GetProcAddress(kernel, "GetEnvironmentStringsW");
    s.offset = offset;
    memcpy(s.name, name, 100);
    BOOL res = call_prog(hProcess, STORE_PROG, sizeof(STORE_PROG), &s, sizeof(EnvStruct));
    FreeLibrary(kernel);
    return res;
}

BOOL load_environment(char* name, HANDLE hProcess, DWORD offset) {
    HMODULE kernel = LoadLibraryA("Kernel32.dll");
    EnvStruct s;
    s.open = GetProcAddress(kernel, "OpenFileMappingA");
    s.map = GetProcAddress(kernel, "MapViewOfFile");
    s.close = GetProcAddress(kernel, "CloseHandle");
    s.unmap = GetProcAddress(kernel, "UnmapViewOfFile");
    s.envfunc = GetProcAddress(kernel, "SetEnvironmentStringsW");
    s.offset = offset;
    memcpy(s.name, name, 100);
    BOOL res = call_prog(hProcess, LOAD_PROG, sizeof(LOAD_PROG), &s, sizeof(EnvStruct));
    FreeLibrary(kernel);
    return res;
}

BOOL free_environment(char* name, HANDLE hProcess, DWORD offset) {
    HMODULE kernel = LoadLibraryA("Kernel32.dll");
    EnvStruct s;
    s.open = GetProcAddress(kernel, "OpenFileMappingA");
    s.map = GetProcAddress(kernel, "MapViewOfFile");
    s.close = GetProcAddress(kernel, "CloseHandle");
    s.unmap = GetProcAddress(kernel, "UnmapViewOfFile");
    s.envfunc = GetProcAddress(kernel, "FreeEnvironmentStringsW");
    s.offset = offset;
    memcpy(s.name, name, 100);
    BOOL res = call_prog(hProcess, FREE_PROG, sizeof(FREE_PROG), &s, sizeof(EnvStruct));
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
	int status = 0;
	LPWSTR* argv = parse_command_line(args, &argc);

    HANDLE hProcess = NULL;
    HANDLE mapping = NULL;
    unsigned char* data = NULL;

	if (argc < 2) {
		_printf_h(err, "Missing argument\n");
        _printf_h(err, "usage: envir [--help] <command>\n");
        status = 1;
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
        status = 2;
        goto end;
    }
    
    DWORD parent = GetParentProcessId();
    if (parent == 0) {
        _printf_h(err, "Could not get parent process\n");
        status = 3;
        goto end;
    }

    hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, parent);
    if (hProcess == NULL) {
        _printf_h(err, "Could not get parent process\n");
        status = 3;
        goto end;
    }
    
    char file_name[100];
    _snprintf_s(file_name, 100, 100, "Env-Stack-File-%d", parent);
    if (action == STACK_ID) {
        _printf("%s\n", file_name);
        goto end;
    }
    
    mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, file_name);
    BOOL created = FALSE;
    if (mapping == NULL) {
        if (!create_mapping_file(file_name, hProcess)) {
            _printf_h(err, "Could not create environment stack\n");
            status = 4;
            goto end;
        }
        mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, file_name);
        if (mapping == NULL) {
            _printf_h(err, "Could access environment stack\n");
            status = 5;
            goto end;
        }
        created = TRUE;
    }
    data = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (data == NULL) {
        _printf_h(err, "Could not access environment stack\n");
        status = 5;
        goto end;
    }
    if (created) {
        memset(data, 0, 8); // Not realy needed since page-file mappings are zero-initialized, but to be safe.
    }
    size_t env_stack_length;
    memcpy(&env_stack_length, data, 8);
    if (env_stack_length > 64) {
        _printf_h(err, "Corrupt environment stack\n");
        status = 7;
        goto end;
    }

    if (action == STACK_SAVE || action == STACK_PUSH) {
        if (action == STACK_PUSH || env_stack_length == 0) {
            if (env_stack_length == 64) {
                _printf_h(err, "Stack is full\n");
                status = 1;
                goto end;
            }
            env_stack_length += 1;
        }     
        if (!store_environment(file_name, hProcess, 8 * env_stack_length)) {
            _printf_h(err, "Failed storing environment\n");
            status = 6;
            goto end;
        }
    } else if (action == STACK_LOAD || action == STACK_POP) {
        if (env_stack_length == 0) {
            _wprintf_h(err, L"Stack is empty\n");
            status = 1;
            goto end;
        }
        if (!load_environment(file_name, hProcess, 8 * env_stack_length)) {
            _printf_h(err, "Failed loading environment\n");
            status = 6;
            goto end;
        }
        if (action == STACK_POP) {
            if (!free_environment(file_name, hProcess, 8 * env_stack_length)) {
                _printf_h(err, "Failed freeing environment\n");
                status = 6;
                goto end;
            }
            env_stack_length -= 1;
        }
    } else if (action == STACK_CLEAR) {
        while (env_stack_length > 0) {
            if (!free_environment(file_name, hProcess, 8 * env_stack_length)) {
                _printf_h(err, "Failed freeing environment\n");
                status = 6;
                goto end;
            }
            env_stack_length -= 1;
        }
    } else if (action == STACK_SIZE) {
        _printf("Stack size: %llu\n", env_stack_length);
    }

    memcpy(data, &env_stack_length, 8);
end:
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
