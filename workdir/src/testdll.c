#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <windows.h>
#include <Psapi.h>
#include "printf.h"
#include "dynamic_string.h"
#include "path_utils.h"
#include <limits.h>


static int refcount = 0;

#define IS_LEGACY_PREFIX(b) (b == 0x66 || b == 0x67 || \
        b == 0x2E || b == 0x3E || b == 0x26 || \
        b == 0x64 || b == 0x65 || b == 0x36 || \
        b == 0xF0 || b == 0xF3 || b == 0xF2)

#define IS_REX(b) ((b & 0xf0) == 0x40)

// determines the length of an instruction starting at data
// Only works for push register, push immediate, sub rsp with immediate, xor register, register
// Returns -1 on failure
// The length returned might be longer then len, if the given bytes are enough
// to be sure of the length, independent of the following bytes.
// TODO: support mov
int instruction_length(unsigned char* data, unsigned len) {
    if (len == 0) {
        return -1;
    }

    unsigned ix = 0;
    unsigned lp_count = 0;
    BOOL operand_size_override = FALSE;
    BOOL address_size_override = FALSE;
    while (lp_count < 4 && ix < len) {
        if (IS_LEGACY_PREFIX(data[ix])) {
            if (data[ix] == 0x66) {
                operand_size_override = TRUE;
            } else if (data[ix] == 0x67) {
                address_size_override = TRUE;
            }
            ++lp_count;
            ++ix;
        } else {
            break;
        }
    }
    if (ix == len) {
        return -1;
    }

    unsigned rex = 0;
    if (IS_REX(data[ix])) {
        ++ix;
        rex = 1;
        if (ix == len) {
            return -1;
        }
    }

    unsigned char opcode = data[ix];

    // push register
    if ((opcode & 0xf0) == 0x50) {
        return rex + lp_count + 1;
    }
    // push immediate
    if (opcode == 0x68) {
        if (operand_size_override) {
            return rex + lp_count + 3;
        } else {
            return rex + lp_count + 5;
        }
    }

    ++ix;
    if (ix == len) {
        return -1;
    }
    unsigned char modrm = data[ix];

    // sub 64-bit register
    if (opcode == 0x81 || opcode == 0x83) {
        BOOL byte = opcode == 0x83;
        if ((modrm & 0b11111000) != 0b11101000) {
            return -1;
        }
        return rex + lp_count + (byte ? 3 : 6);
    }

    // xor
    if (opcode == 0x33) {
        if ((modrm & 0b11000000) != 0b11000000) {
            return -1;
        }
        return rex + lp_count + 2;
    }

    return -1;
}

typedef struct SearchContext {
    WideBuffer lookup_exe;

} SearchContext;


BOOL make_absolute(DynamicWString* path) {
    wchar_t buf[MAX_PATH];
    DWORD res = GetFullPathNameW(path->buffer, MAX_PATH, buf, NULL);
    if (res == 0) {
        return FALSE;
    } else if (res > MAX_PATH) {
        // TODO: HeapAlloc
        return FALSE;
    } else {
        DynamicWStringClear(path);
        DynamicWStringAppendCount(path, buf, res);
        return TRUE;
    }
}

BOOL get_autocomplete(DynamicWString* in, DWORD ix, DynamicWString* out, SearchContext* context) {
    DynamicWStringClear(out);

    HANDLE read, write;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&read, &write, &sa, 0)) {
        return FALSE;
    }

    if (!SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(read);
        CloseHandle(write);
        return FALSE;
    }

    WideBuffer buf;
    buf.capacity = 48;
    buf.ptr = NULL;
    buf.size = 0;
    if (!get_envvar(L""))

    wchar_t cmd[] = L"python.exe test.py";

    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.hStdOutput = write;
    si.dwFlags = STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));

    if (!CreateProcessW(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(read);
        CloseHandle(write);
        return FALSE;
    }

    CloseHandle(pi.hThread);
    CloseHandle(write);

    wchar_t buf[128];
    DWORD read_count;
    while (ReadFile(read, buf, 256, &read_count, NULL)) {
        if ((read_count % sizeof(wchar_t) != 0)) {
            DWORD r;
            char last_byte = buf[read_count / sizeof(wchar_t)];
            char b;
            if (!ReadFile(read, &b, 1, &r, NULL) || r == 0) {
                --read_count;
            } else {
                buf[read_count / sizeof(wchar_t)] = ((wchar_t)last_byte) | (((wchar_t) b) << 8);
                ++read_count;
            }
        }
        DynamicWStringAppendCount(out, buf, read_count / sizeof(wchar_t));
    }

    CloseHandle(read);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit;
    if (!GetExitCodeProcess(pi.hProcess, &exit)) {
        CloseHandle(pi.hProcess);
        return FALSE;
    }

    CloseHandle(pi.hProcess);
    if (exit != 0) {
        return FALSE;
    }

    return TRUE;
}

typedef BOOL(WINAPI* ReadConsoleW_t)(HANDLE, LPVOID, DWORD, LPDWORD, PCONSOLE_READCONSOLE_CONTROL);

ReadConsoleW_t ReadConsoleW_Old;

BOOL WINAPI ReadConsoleW_Hook(HANDLE hConsoleInput, LPVOID lpBuffer, DWORD nNumberOfCharsToRead, LPDWORD lpNumberOfCharsRead, 
        PCONSOLE_READCONSOLE_CONTROL pInputControl
    ) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DynamicWString auto_complete;
    if (!DynamicWStringCreate(&auto_complete)) {
        return FALSE;
    }

    DynamicWString in;
    in.capacity = nNumberOfCharsToRead;
    in.buffer = lpBuffer;

    SearchContext context;
    context.lookup_exe.size = 0;
    context.lookup_exe.capacity = 48;
    context.lookup_exe.ptr = NULL;
read:
    if (!ReadConsoleW_Old(hConsoleInput, lpBuffer, nNumberOfCharsToRead, lpNumberOfCharsRead, pInputControl)) {
        goto fail;
    }
    in.length = *lpNumberOfCharsRead;

    for (DWORD ix = 0; ix < in.length; ++ix) {
        if (in.buffer[ix] == '\r') {
            DynamicWStringFree(&auto_complete);
            HeapFree(GetProcessHeap(), 0, context.lookup_exe.ptr);
            return TRUE;
        }
    }
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(out, &info)) {
        goto fail;
    }
    --in.length;
    COORD pos = info.dwCursorPosition;
    int ix = in.length;
    while (1) {
        if (ix == 0 || in.buffer[ix - 1] == L' ') {
            break;
        }
        --ix;
        if (pos.X == 0) {
            pos.X = info.dwSize.X - 1;
            pos.Y -= 1;
        } else {
            pos.X -= 1;
        }
    }

    DWORD written;
    FillConsoleOutputCharacterW(out, L' ', in.length - ix, pos, &written);

    if (!SetConsoleCursorPosition(out, pos)) {
        goto fail;
    }

    if (get_autocomplete(&in, ix, &auto_complete, &context)) {
        in.length = ix;
        if (in.length + auto_complete.length >= in.capacity) {
            return FALSE;
        }
        DynamicWStringAppendCount(&in, auto_complete.buffer, auto_complete.length);
    }
    WriteConsoleW(out, in.buffer + ix, in.length - ix, &written, NULL);


    pInputControl->nInitialChars = in.length;

    goto read;
fail:
    DynamicWStringFree(&auto_complete);
    HeapFree(GetProcessHeap(), 0, context.lookup_exe.ptr);
    return FALSE;
}

#define JMPREL_SIZE 5
#define JMPABS_SIZE (6 + 8)
#define MAX_PRELUDE_SIZE 20

__declspec(dllexport) DWORD entry() {
    ++refcount;
    if (refcount > 1) {
        _printf("Already active\n");
        return 1;
    }

    HANDLE mod = GetModuleHandle(L"KernelBase.dll");

    unsigned char* src_addr = (unsigned char*)GetProcAddress(mod, "ReadConsoleW");
    unsigned char* hook_addr = (unsigned char*)ReadConsoleW_Hook;

    // Find a 32-bit relative offset from start of function and the hook function.
    long long full_delta = hook_addr - (src_addr + JMPREL_SIZE);
    if (full_delta > INT_MAX || full_delta < INT_MIN) {
        _printf("Failed creating hook: Dll mapped too far away in memory\n");
        return -1;
    }
    int delta = full_delta;

    // Find how many whole instructions need to be moved in order to fit a jump to the hook.
    unsigned prelude_len = 0;
    while (prelude_len < JMPREL_SIZE) {
        int len = instruction_length(src_addr + prelude_len, MAX_PRELUDE_SIZE - prelude_len);
        if (len == -1) {
            break;
        }
        prelude_len += len;
    }
    if (prelude_len < JMPREL_SIZE) {
        _printf("Failed creating hook: Bad function prelude\n");
        return -1;
    }

    // Copy the begining of the function to separate place in memory,
    // and write a jump back to the rest of function at the end.
    unsigned char* save_addr = HeapAlloc(GetProcessHeap(), 0, prelude_len + JMPABS_SIZE);
    memcpy(save_addr, src_addr, prelude_len);
    long long old_addr = (long long)(src_addr + prelude_len);
    save_addr[prelude_len] = 0xff;
    save_addr[prelude_len + 1] = 0x25;
    save_addr[prelude_len + 2] = 0;
    save_addr[prelude_len + 3] = 0;
    save_addr[prelude_len + 4] = 0;
    save_addr[prelude_len + 5] = 0;
    memcpy(save_addr + prelude_len + 6, &old_addr, sizeof(old_addr));
    DWORD old_protect;
    // Idealy this should be PAGE_EXECUTE_READ, but since HeapAlloc is used this
    // might make memory that expects to be writable readonly.
    // Using VirtualAlloc solves this, but crashes when memory is read from other thread...
    if (!VirtualProtect(save_addr, prelude_len + JMPABS_SIZE, PAGE_EXECUTE_READWRITE, &old_protect)) {
        _printf("Failed creating hook: Could not make page executable\n");
        return -1;
    }
    ReadConsoleW_Old = (ReadConsoleW_t)save_addr;
    FlushInstructionCache(GetCurrentProcess(), save_addr, prelude_len + JMPABS_SIZE);

    // Make the original function memory writable.
    if (!VirtualProtect(src_addr, prelude_len, PAGE_EXECUTE_READWRITE, &old_protect)) {
        _printf("Failed creating hook: Could not make page writable\n");
        return -1;
    }
    // Write relative jump to the hook at begining of original function.
    src_addr[0] = 0xe9;
    memcpy(src_addr + 1, &delta, sizeof(delta));

    // Make page no longer writable, not very important so no error check.
    VirtualProtect(src_addr, prelude_len, old_protect, &old_protect);

    FlushInstructionCache(GetCurrentProcess(), src_addr, 1 + sizeof(delta));

    return 0;
}



BOOL DLLMain() {



    return TRUE;
}
