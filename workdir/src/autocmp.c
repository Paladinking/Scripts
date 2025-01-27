#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <windows.h>
#include <Psapi.h>
#include "printf.h"
#include "dynamic_string.h"
#include "json.h"
#include "match_node.h"
#include "mem.h"
#include <limits.h>

// TODO:
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
        WString_free(&auto_complete);
        WString_free(&rem);
        if (context.active_it) {
            NodeIterator_stop(&context.it);
        }
        return TRUE;
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


bool add_node(NodeBuilder* builder, const char* key, HashMap* extr_map, MatchNode* node, WString* workbuf) {
    if (strcmp(key, "&FILE") == 0) {
        NodeBuilder_add_files(builder, true, node);
    } else if (strcmp(key, "&FILELIKE") == 0) {
        NodeBuilder_add_files(builder, false, node);
    } else if (strcmp(key, "&DEFAULT") == 0) {
        NodeBuilder_add_any(builder, node);
    } else if (key[0] == '&') {
        DynamicMatch* dyn = HashMap_Value(extr_map, key);
        if (dyn != NULL) {
            NodeBuilder_add_dynamic(builder, dyn, node);
        } else {
            _wprintf(L"Invalid extern \"%S\"\n", key);
            return false;
        }
    } else {
        if (!WString_from_utf8_str(workbuf, key)) {
            _wprintf(L"Invalid static \"%S\"\n", key);
            return false;
        }
        NodeBuilder_add_fixed(builder, workbuf->buffer, workbuf->length, node);
    }
    return true;
}

bool load_json() {
    wchar_t json_buf[1025];
    if (!find_file_relative(json_buf, 1024, L"autocmp.json", true)) {
        _printf("Could not find autocmp.json\n");
        return 1;
    }
    _wprintf(L"Found json at %s\n", json_buf);

    String json_str;
    if (!read_text_file(&json_str, json_buf)) {
        _printf("Could not read autocmp.json\n");
        return 1;
    }

    JsonObject obj;
    String error_msg;
    if (!json_parse_object(json_str.buffer, &obj, &error_msg)) {
        _wprintf(L"Failed parsing json file: %S\n", error_msg.buffer);
        String_free(&json_str);
        String_free(&error_msg);
        return 1;
    }
    String_free(&json_str);

    JsonObject *extr = JsonObject_get_obj(&obj, "extern");
    JsonObject* root_obj = JsonObject_get_obj(&obj, "root");
    if (root_obj == NULL) {
        _wprintf(L"Missing root node\n");
        JsonObject_free(&obj);
        return 1;
    }

    HashMap extr_map;
    HashMap_Create(&extr_map);
    HashMapIterator it;
    HashElement *elem;

    if (extr != NULL) {
        HashMapIter_Begin(&it, &extr->data);
        while ((elem = HashMapIter_Next(&it)) != NULL) {
            JsonType *type = elem->value;
            if (type->type != JSON_OBJECT) {
                continue;
            }
            JsonObject *dyn_obj = &type->object;

            String *cmd = JsonObject_get_string(dyn_obj, "cmd");
            String *sep = JsonObject_get_string(dyn_obj, "separator");
            String *inval = JsonObject_get_string(dyn_obj, "invalidation");
            if (cmd == NULL || sep == NULL || inval == NULL ||
                cmd->length == 0 || inval->length == 0) {
                _wprintf(L"Invalid extern spec for %S\n", elem->key);
                continue;
            }
            WString wcmd;
            WString_create(&wcmd);
            // use buffer for separator as well
            if (!WString_from_utf8_bytes(&wcmd, sep->buffer, sep->length) ||
                wcmd.length != 1) {
                _wprintf(L"Invalid separator for %S\n", elem->key);
                WString_free(&wcmd);
                continue;
            }
            wchar_t sep_char = wcmd.buffer[0];
            if (!WString_from_utf8_bytes(&wcmd, cmd->buffer, cmd->length)) {
                _wprintf(L"Invalid command from %S\n", elem->key);
                WString_free(&wcmd);
                continue;
            }
            enum Invalidation invalidation = INVALID_NEVER;
            if (inval->buffer[0] == 'c' || inval->buffer[0] == 'C') {
                invalidation = INVALID_CHDIR;
            } else if (inval->buffer[0] == 'a' || inval->buffer[0] == 'A') {
                invalidation = INVALID_ALWAYS;
            }

            // wcmd.buffer is moved
            DynamicMatch *match =
                DynamicMatch_create(wcmd.buffer, invalidation, sep_char);
            HashMap_Insert(&extr_map, elem->key, match);
        }
    }

    struct {
        const char* key;
        HashMapIterator it;
        NodeBuilder node;
    } stack[32];
    unsigned stack_ix = 0;

    HashMapIter_Begin(&stack[0].it, &root_obj->data);
    NodeBuilder_create(&stack[0].node);
    WString workbuf;
    WString_create(&workbuf);
    while (1) {
        while ((elem = HashMapIter_Next(&stack[stack_ix].it)) != NULL) {
            JsonType *type = elem->value;
            if (elem->key[0] == '\0') {
                continue;
            }
            NodeBuilder* node = &stack[stack_ix].node;
            if (type->type == JSON_NULL) {
                add_node(node, elem->key, &extr_map, NULL, &workbuf);
                continue;
            }
            if (type->type != JSON_OBJECT) {
                continue;
            }
            if (stack_ix >= 31) {
                _wprintf(L"Too deep nesting in tree\n");
                add_node(node, elem->key, &extr_map, NULL, &workbuf);
                continue;
            }

            ++stack_ix;
            stack[stack_ix].key = elem->key;
            HashMapIter_Begin(&stack[stack_ix].it, &type->object.data);
            NodeBuilder_create(&stack[stack_ix].node);
        }
        if (stack_ix == 0) {
            break;
        }
        const char* key = stack[stack_ix].key;
        MatchNode* node = NodeBuilder_finalize(&stack[stack_ix].node);
        --stack_ix;
        add_node(&stack[stack_ix].node, key, &extr_map, node, &workbuf);
    }
    WString_free(&workbuf);

    MatchNode* root = NodeBuilder_finalize(&stack[0].node);
    MatchNode_set_root(root);
    HashMap_Free(&extr_map);
    JsonObject_free(&obj);

    return true;
}


__declspec(dllexport) DWORD entry() {
    if (refcount > 0) {
        _printf("Already active\n");
        return 1;
    }
    ++refcount;

    MatchNode_init();

    if (!load_json()) {
        NodeBuilder b;
        NodeBuilder_create(&b);
        MatchNode_set_root(NodeBuilder_finalize(&b));
    }

    WString_create(&workdir);

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
    return 0;
}


__declspec(dllexport) DWORD reload() {
    if (refcount == 0) {
        return entry();
    }

    MatchNode_reset();
    if (!load_json()) {
        NodeBuilder b;
        NodeBuilder_create(&b);
        MatchNode_set_root(NodeBuilder_finalize(&b));
        return 1;
    }
    return 0;
}


BOOL DLLMain() {
    return TRUE;
}
