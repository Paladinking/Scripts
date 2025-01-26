#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <windows.h>
#include <Psapi.h>
#include "printf.h"
#include "dynamic_string.h"
#include "match_node.h"
#include "mem.h"
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

bool char_needs_quotes(wchar_t c) {
    return c == L' ' || c == L'+' || c == L'=' || c == L'(' || c == L')' || 
            c == L'{' || c == '}' || c == L'%' || c == L'[' || c == L']';
}

bool str_needs_quotes(const wchar_t* str) {
    for (unsigned i = 0; i < str[i] != L'\0'; ++i) {
        wchar_t c = str[i];
        if (char_needs_quotes(c)) {
            return true;
        }
    }
    return false;
}


typedef struct SearchContext {
    NodeIterator it;
    bool active_it;
} SearchContext;

bool get_autocomplete(MatchNode* node, SearchContext* it, WString* out, WString* rem, bool changed) {
    if (changed && it->active_it) {
        NodeIterator_stop(&it->it);
        it->active_it = false;
    }
    WString_clear(out);

    const wchar_t* buf;
    if (it->active_it) {
        buf = NodeIterator_next(&it->it);
        if (buf == NULL) {
            NodeIterator_restart(&it->it);
            buf = NodeIterator_next(&it->it);
            if (buf == NULL) {
                return false;
            }
        } 
    } else {
        NodeIterator_begin(&it->it, node, rem->buffer);
        it->active_it = true;
        buf = NodeIterator_next(&it->it);
        if (buf == NULL) {
            return false;
        }
    }

    if (str_needs_quotes(buf)) {
        WString_append(out, L'"');
        WString_extend(out, buf);
        WString_append(out, L'"');
    } else {
        WString_extend(out, buf);
    }
    return true;
}

WString workdir;

typedef BOOL(WINAPI* ReadConsoleW_t)(HANDLE, LPVOID, DWORD, LPDWORD, PCONSOLE_READCONSOLE_CONTROL);

ReadConsoleW_t ReadConsoleW_Old;

__declspec(dllexport) BOOL WINAPI ReadConsoleW_Hook(HANDLE hConsoleInput, LPVOID lpBuffer, DWORD nNumberOfCharsToRead, LPDWORD lpNumberOfCharsRead, 
        PCONSOLE_READCONSOLE_CONTROL pInputControl
    ) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    WString auto_complete;
    MatchNode* last_node = NULL;
    WString rem;
    if (!WString_create(&auto_complete)) {
        return FALSE;
    }
    if (!WString_create(&rem)) {
        WString_free(&auto_complete);
        return FALSE;
    }

    WString in;
    in.capacity = nNumberOfCharsToRead;
    in.buffer = lpBuffer;
    in.length = 0;

    SearchContext context;
    context.active_it = false;

    get_workdir(&rem);
    bool chdir = !WString_equals(&rem, &workdir);
    if (chdir) {
        WString_clear(&workdir);
        WString_append_count(&workdir, rem.buffer, rem.length);
    }
    DynamicMatch_invalidate_many(chdir);
read:
    if (!ReadConsoleW_Old(hConsoleInput, lpBuffer, nNumberOfCharsToRead, lpNumberOfCharsRead, pInputControl)) {
        goto fail;
    }
    in.length = *lpNumberOfCharsRead;

    for (DWORD ix = 0; ix < in.length; ++ix) {
        if (in.buffer[ix] == '\r') {
            WString_free(&auto_complete);
            WString_free(&rem);
            if (context.active_it) {
                NodeIterator_stop(&context.it);
            }
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
    // Remove '\t' and append '\0'
    WString_pop(&in, 1);

    size_t offset;
    MatchNode* final = find_final(in.buffer, &offset, &rem);

    COORD pos = info.dwCursorPosition;

    unsigned wordlen = in.length - offset;
    bool changed = false;
    if (last_node != final || wordlen != auto_complete.length ||
        memcmp(in.buffer + offset, auto_complete.buffer, wordlen * sizeof(wchar_t)) != 0) {
        changed = true;
    }

    if (get_autocomplete(final, &context, &auto_complete, &rem, changed)) {
        last_node = final;
        size_t ix = in.length;
        in.length = offset;
        if (in.length + auto_complete.length >= in.capacity) {
            goto fail;
        }
        WString_append_count(&in, auto_complete.buffer, auto_complete.length);

        DWORD written;
        if (auto_complete.length < wordlen) {
            while (ix > offset + auto_complete.length) {
                if (pos.X == 0) {
                    pos.X = info.dwSize.X - 1;
                    pos.Y -= 1;
                } else {
                    pos.X -= 1;
                }
                --ix;
            }
            FillConsoleOutputCharacterW(out, L' ', wordlen - auto_complete.length, pos, &written);
        }

        while (ix > offset) {
            if (pos.X == 0) {
                pos.X = info.dwSize.X - 1;
                pos.Y -= 1;
            } else {
                pos.X -= 1;
            }
            --ix;
        }

        CONSOLE_CURSOR_INFO i;
        i.dwSize = 100;
        i.bVisible = FALSE;
        SetConsoleCursorInfo(out, &i);

        if (!SetConsoleCursorPosition(out, pos)) {
            goto fail;
        }

        WriteConsoleW(out, in.buffer + ix, in.length - ix, &written, NULL);

        i.bVisible = TRUE;
        SetConsoleCursorInfo(out, &i);
    } else {
        last_node = NULL;
    }

    pInputControl->nInitialChars = in.length;

    goto read;
fail:
    WString_free(&rem);
    if (context.active_it) {
        NodeIterator_stop(&context.it);
    }

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
    unsigned char* save_addr = Mem_alloc(prelude_len + JMPABS_SIZE);
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

    MatchNode_init();
    NodeBuilder b;
    NodeBuilder_create(&b);
    NodeBuilder_add_fixed(&b, L"Hello world", 11, NULL);
    NodeBuilder_add_files(&b, false, NULL);
    MatchNode* git_add = NodeBuilder_finalize(&b);

    NodeBuilder_create(&b);
    MatchNode* git_status = NodeBuilder_finalize(&b);

    NodeBuilder_create(&b);
    NodeBuilder_add_files(&b, true, NULL);
    DynamicMatch* dyn = DynamicMatch_create(L"git for-each-ref --format=%(refname:short) refs/heads", INVALID_CHDIR, '\n');
    NodeBuilder_add_dynamic(&b, dyn, NULL);
    MatchNode* git_show = NodeBuilder_finalize(&b);

    NodeBuilder_create(&b);
    NodeBuilder_add_fixed(&b, L"add", wcslen(L"add"), git_add);
    NodeBuilder_add_fixed(&b, L"status", wcslen(L"status"), git_status);
    NodeBuilder_add_fixed(&b, L"show", wcslen(L"show"), git_show);
    MatchNode* git = NodeBuilder_finalize(&b);

    NodeBuilder_create(&b);
    NodeBuilder_add_files(&b, true, NULL);
    MatchNode* grep = NodeBuilder_finalize(&b);

    NodeBuilder_create(&b);
    NodeBuilder_add_files(&b, true, NULL);
    MatchNode* file = NodeBuilder_finalize(&b);
    
    NodeBuilder_create(&b);
    NodeBuilder_add_fixed(&b, L"grep", wcslen(L"grep"), grep);
    NodeBuilder_add_fixed(&b, L"git", wcslen(L"git"), git);
    NodeBuilder_add_files(&b, true, NULL);
    NodeBuilder_add_any(&b, file);

    MatchNode_set_root(NodeBuilder_finalize(&b));

    WString_create(&workdir);

    return 0;
}



BOOL DLLMain() {



    return TRUE;
}
