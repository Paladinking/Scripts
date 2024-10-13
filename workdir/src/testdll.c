#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <windows.h>
#include <Psapi.h>
#include "printf.h"
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


BOOL get_autocomplete(wchar_t* in_buf, wchar_t* out_buf, DWORD ix, DWORD len, DWORD* out_len) {
    memcpy(out_buf, L"Hello_world", 11 * sizeof(wchar_t));
    *out_len = 11;
    return TRUE;
}

typedef BOOL(WINAPI* ReadConsoleW_t)(HANDLE, LPVOID, DWORD, LPDWORD, PCONSOLE_READCONSOLE_CONTROL);

ReadConsoleW_t ReadConsoleW_Old;

BOOL WINAPI ReadConsoleW_Hook(HANDLE hConsoleInput, LPVOID lpBuffer, DWORD nNumberOfCharsToRead, LPDWORD lpNumberOfCharsRead, 
        PCONSOLE_READCONSOLE_CONTROL pInputControl
    ) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    wchar_t *autcomplete_buf = HeapAlloc(GetProcessHeap(), 0, nNumberOfCharsToRead * sizeof(wchar_t));

read:
    if (!ReadConsoleW_Old(hConsoleInput, lpBuffer, nNumberOfCharsToRead, lpNumberOfCharsRead, pInputControl)) {
        return FALSE;
    }
    DWORD len = *lpNumberOfCharsRead;
    wchar_t* data = lpBuffer;

    for (DWORD ix = 0; ix < len; ++ix) {
        if (data[ix] == '\r') {
            return TRUE;
        }
    }
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(out, &info)) {
        return FALSE;
    }
    COORD pos = info.dwCursorPosition;
    int ix = len - 1;
    while (1) {
        if (ix == 0 || data[ix - 1] == L' ') {
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
    FillConsoleOutputCharacterW(out, L' ', len - ix, pos, &written);

    if (!SetConsoleCursorPosition(out, pos)) {
        return FALSE;
    }

    get_autocomplete(data, data + ix, ix, len, &written);
    WriteConsoleW(out, data + ix, written, &written, NULL);

    pInputControl->nInitialChars = ix + 11;

    goto read;
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
