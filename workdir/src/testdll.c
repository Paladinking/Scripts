#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <windows.h>
#include <Psapi.h>
#include "printf.h"
#include "dynamic_string.h"
#include "glob.h"
#include <limits.h>

// TODO:
// Commands and arguments from file
// Do not double complete when commands and files have same name

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
    WString cmd_base;
    WalkCtx dir_ctx;
    WString dir;
    BOOL has_quotes;

    const WString* commands;
    unsigned command_count;
    unsigned command_ix;
} SearchContext;

wchar_t* find_parent_dir(wchar_t* str, unsigned len) {
    unsigned ix = len;
    while (ix > 0) {
        --ix;
        if (str[ix] == L'/' || str[ix] == L'\\') {
            return str + ix;
        }
    }
    return NULL;
}

bool char_needs_quotes(wchar_t c) {
    return c == L' ' || c == L'+' || c == L'=' || c == L'(' || c == L')' || 
            c == L'{' || c == '}' || c == L'%' || c == L'[' || c == L']';
}

bool str_needs_quotes(const WString* str) {
    for (unsigned i = 0; i < str->length; ++i) {
        wchar_t c = str->buffer[i];
        if (char_needs_quotes(c)) {
            return true;
        }
    }
    return false;
}

bool filter(const WString* str, void* ctx) {
    WString* base = &((SearchContext*)ctx)->cmd_base;
    if (base->length > str->length) {
        return false;
    }
    for (unsigned ix = 0; ix < base->length; ++ix) {
        if (towlower(str->buffer[ix]) != towlower(base->buffer[ix])) {
            return false;
        }
    }
    return true;
}

BOOL get_autocomplete(WString* in, DWORD ix, WString* out, SearchContext* context, BOOL changed) {
    WString_clear(out);
    if (changed) {
        WalkDir_abort(&context->dir_ctx);
        WString_clear(&context->cmd_base);
        WString_clear(&context->dir);
        wchar_t* sep = find_parent_dir(in->buffer + ix, in->length - ix);
        context->command_ix = 0;
        if (sep == NULL) {
            context->has_quotes = FALSE;
            WString_append_count(&context->cmd_base, in->buffer + ix, in->length - ix);
            WalkDir_begin(&context->dir_ctx, L".");
        } else {
            context->has_quotes = in->buffer[ix] == L'"';
            if (context->has_quotes) {
                ++ix;
            }
            unsigned offset = sep - (in->buffer + ix);
            unsigned remaining = in->length - ix - offset;
            if (context->has_quotes && in->buffer[ix + offset - 1] == L'"') {
                --offset;
            } else if (context->has_quotes && sep[remaining - 1] == L'"') {
                --remaining;
            }
            WString_append_count(&context->dir, in->buffer + ix, offset);
            WString_append(&context->dir, *sep);
            WString_append_count(&context->cmd_base, sep + 1, remaining - 1);
            WalkDir_begin(&context->dir_ctx, context->dir.buffer);
        }
    }

    const WString* line = NULL;
    Path* path = NULL;
    bool commands = context->dir.length == 0;

    while (commands && context->command_ix < context->command_count) {
        ++context->command_ix;
        if (filter(&context->commands[context->command_ix - 1], context)) {
            line = &context->commands[context->command_ix - 1];
            break;
        }
    }
    while (!line) {
        if (WalkDir_next(&context->dir_ctx, &path) <= 0) {
            const wchar_t* dir = commands ? L"." : context->dir.buffer;
            WalkDir_begin(&context->dir_ctx, dir);
            context->command_ix = 0;
            while (commands && context->command_ix < context->command_count) {
                ++context->command_ix;
                if (filter(&context->commands[context->command_ix - 1], context)) {
                    line = &context->commands[context->command_ix - 1];
                    break;
                }
            }
            while (!line) {
                if (WalkDir_next(&context->dir_ctx, &path) <= 0) {
                    return FALSE;
                }
                if (filter(&path->path, context)) {
                    line = &path->path;
                    break;
                }
            }
            break;
        }
        if (filter(&path->path, context)) {
            line = &path->path;
            break;
        }
    }
    BOOL quote = context->has_quotes;
    if (!quote && (str_needs_quotes(&context->dir) || str_needs_quotes(line))) {
        quote = TRUE;
    }

    if (quote) {
        WString_append(out, L'"');
    }
    WString_append_count(out, context->dir.buffer, context->dir.length);
    WString_append_count(out, line->buffer, line->length);
    if (quote) {
        WString_append(out, L'"');
    }
    return TRUE;
}

typedef BOOL(WINAPI* ReadConsoleW_t)(HANDLE, LPVOID, DWORD, LPDWORD, PCONSOLE_READCONSOLE_CONTROL);

ReadConsoleW_t ReadConsoleW_Old;

WString commands[1] = {L"git", 0, 3};

BOOL WINAPI ReadConsoleW_Hook(HANDLE hConsoleInput, LPVOID lpBuffer, DWORD nNumberOfCharsToRead, LPDWORD lpNumberOfCharsRead, 
        PCONSOLE_READCONSOLE_CONTROL pInputControl
    ) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    WString auto_complete;
    if (!WString_create(&auto_complete)) {
        return FALSE;
    }

    WString in;
    in.capacity = nNumberOfCharsToRead;
    in.buffer = lpBuffer;
    in.length = 0;

    SearchContext context;
    context.dir_ctx.handle = INVALID_HANDLE_VALUE;
    WString_create(&context.cmd_base);
    WString_create(&context.dir);
    context.command_count = 1;
    context.commands = commands;

    BOOL has_complete = FALSE;
read:
    if (!ReadConsoleW_Old(hConsoleInput, lpBuffer, nNumberOfCharsToRead, lpNumberOfCharsRead, pInputControl)) {
        goto fail;
    }
    DWORD events;
    GetNumberOfConsoleInputEvents(hConsoleInput, &events);
    in.length = *lpNumberOfCharsRead;

    for (DWORD ix = 0; ix < in.length; ++ix) {
        if (in.buffer[ix] == '\r') {
            WString_free(&context.dir);
            WString_free(&context.cmd_base);
            WString_free(&auto_complete);
            return TRUE;
        }
    }
    if (in.length == 0 || in.buffer[in.length - 1] != L'\t') {
        // A CTR-C event can trigger ReadConsoleW_Old to return without '\t'
        goto fail;
    }
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(out, &info)) {
        goto fail;
    }
    --in.length;
    COORD pos = info.dwCursorPosition;
    int ix = 0;
    unsigned quote = 0;
    for (; ix < in.length; ++ix) {
        wchar_t c = in.buffer[ix];
        if (c == L'"') {
            if (quote) {
                quote = 0;
            } else {
                quote = 1;
            }
        }
    }
    while (1) {
        if (ix == 0) {
            break;
        }
        wchar_t c = in.buffer[ix - 1];
        if (quote == 0 && (char_needs_quotes(c) || c == L'&' || c == L'|' || c == L'<' || c == L'>')) {
            break;
        }
        --ix;
        if (pos.X == 0) {
            pos.X = info.dwSize.X - 1;
            pos.Y -= 1;
        } else {
            pos.X -= 1;
        }
        if (in.buffer[ix] == L'"') {
            if (quote) {
                break;
            } else {
                quote = 1;
            }
        }
    }

    unsigned wordlen = in.length - ix;
    BOOL changed = FALSE;
    if (!has_complete || wordlen != auto_complete.length ||
        memcmp(in.buffer + ix, auto_complete.buffer, wordlen * sizeof(wchar_t)) != 0) {
        changed = TRUE;
    }

    has_complete = get_autocomplete(&in, ix, &auto_complete, &context, changed);

    if (has_complete) {
        in.length = ix;
        if (in.length + auto_complete.length >= in.capacity) {
            goto fail;
        }
        WString_append_count(&in, auto_complete.buffer, auto_complete.length);
    }

    DWORD written;
    FillConsoleOutputCharacterW(out, L' ', wordlen, pos, &written);

    if (!SetConsoleCursorPosition(out, pos)) {
        goto fail;
    }

    WriteConsoleW(out, in.buffer + ix, in.length - ix, &written, NULL);


    pInputControl->nInitialChars = in.length;

    goto read;
fail:
    WString_free(&context.dir);
    WString_free(&context.cmd_base);
    WString_free(&auto_complete);
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
