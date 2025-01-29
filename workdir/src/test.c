#ifndef UNICODE
#define UNICODE
#endif

#include "args.h"
#include "glob.h"
#include "json.h"
#include "match_node.h"
#include "printf.h"
#include "subprocess.h"
#include "path_utils.h"

bool add_node(NodeBuilder *builder, const char *key, HashMap *extr_map,
              MatchNode *node, WString *workbuf) {
    if (strcmp(key, "&FILE") == 0) {
        NodeBuilder_add_files(builder, true, node);
    } else if (strcmp(key, "&FILELIKE") == 0) {
        NodeBuilder_add_files(builder, false, node);
    } else if (strcmp(key, "&DEFAULT") == 0) {
        NodeBuilder_add_any(builder, node);
    } else if (key[0] == '&') {
        DynamicMatch *dyn = HashMap_Value(extr_map, key);
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

bool append_file(const char *str, const wchar_t *filename) {
    HANDLE file = CreateFileW(filename, FILE_APPEND_DATA, 0, NULL, OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    unsigned len = strlen(str);
    DWORD w;
    unsigned written = 0;
    while (written < len) {
        if (!WriteFile(file, str + written, len - written, &w, NULL)) {
            CloseHandle(file);
            return false;
        }
        written += w;
    }
    CloseHandle(file);
    return true;
}

// replace all instances of $
void substitute_commands(wchar_t *str, DWORD *len, DWORD capacity) {
    wchar_t *pos = str;
    if (*len == 0) {
        return;
    }
    String outbuf;
    String_create(&outbuf);
    WString wbuf;
    WString_create(&wbuf);

    wchar_t last = str[*len - 1];
    wchar_t end_par = 0;
    if (last == L')' || last == L'}' || last == L']') {
        end_par = last;
    }
    str[*len - 1] = L'\0';
    while (pos < str + *len) {
        wchar_t *dlr = wcschr(pos, L'$');
        if (dlr == NULL) {
            break;
        }
        pos = dlr + 1;
        wchar_t par = *pos;
        if (par == L'(') {
            par = L')';
        } else if (par == L'{') {
            par = L'}';
        } else if (par == L'[') {
            par = L']';
        } else {
            continue;
        }
        wchar_t *close = wcschr(pos, par);
        bool at_end = false;
        if (close == NULL) {
            if (end_par == par) {
                at_end = true;
                close = str + *len - 1;
            } else {
                break;
            }
        }
        DWORD cmd_len = close - pos + 2;
        *close = L'\0';
        String_clear(&outbuf);
        WString_clear(&wbuf);
        unsigned long errorcode;
        if (!subprocess_run(pos + 1, &outbuf, 5000, &errorcode, SUBPROCESS_STDIN_DEVNULL)) {
            String_clear(&outbuf);
        }
        if (outbuf.length > 0 &&
            !WString_from_utf8_bytes(&wbuf, outbuf.buffer, outbuf.length)) {
            _wprintf(L"Failed decoding to utf-16\n");
            WString_clear(&wbuf);
        }
        if ((*len + wbuf.length - cmd_len < capacity)) {
            WString_replaceall(&wbuf, L'\n', ' ');
            WString_replaceall(&wbuf, L'\r', ' ');
            WString_replaceall(&wbuf, L'\t', ' ');
            DWORD start = (pos - 1) - str;
            DWORD old_end = 1 + close - str;
            DWORD rem = *len - old_end;
            DWORD new_end = start + wbuf.length;
            memmove(str + new_end, str + old_end, rem * sizeof(wchar_t));
            memcpy(str + start, wbuf.buffer, wbuf.length * sizeof(wchar_t));
            *len = *len - cmd_len + wbuf.length;
            if (at_end) {
                last = str[*len - 1];
                break;
            }
            close = str + new_end - 1;
        } else {
            if (at_end) {
                break;
            }
            *close = par;
        }
        pos = close + 1;
    }
    str[*len - 1] = last;
    String_free(&outbuf);
    WString_free(&wbuf);
}


WString pathext, pathbuf, progbuf, workdir;

const wchar_t* find_program(const wchar_t* cmd, WString* dest) {
    unsigned ix = 0;
    bool in_quote = false;
    WString prog;
    WString_clear(dest);
    WString_clear(&progbuf);
    while (1) {
        if (cmd[ix] == L'\0' || (cmd[ix] == L' ' && !in_quote)) {
            break;
        }
        if (cmd[ix] == L'"') {
            in_quote = !in_quote;
        } else {
            WString_append(&progbuf, cmd[ix]);
        }
        ++ix;
    }

    if (!get_envvar(L"PATHEXT", 0, &pathext) || pathext.length == 0) {
        WString_clear(&pathext);
        WString_extend(&pathext, L".EXE;.BAT");
    }
    wchar_t *ext = pathext.buffer;
    DWORD count = 0;
    do {
        wchar_t* sep = wcschr(ext, L';');
        if (sep == NULL) {
            sep = pathext.buffer + pathext.length;
        }
        *sep = L'\0';
        do {
            if (!WString_reserve(dest, count)) {
                return NULL;
            }
            count = SearchPathW(workdir.buffer,
                                progbuf.buffer, ext, dest->capacity,
                                dest->buffer, NULL);
        } while (count > dest->capacity);
        dest->length = count;
        dest->buffer[dest->length] = L'\0';
        if (count > 0) {
            return dest->buffer;
        }
        *sep = L';';
        ext = sep + 1;
    } while (ext < pathext.buffer + pathext.length);

    if (!get_envvar(L"PATH", 0, &pathbuf)) {
        return NULL;
    }
    ext = pathext.buffer;
    count = 0;
    do {
        wchar_t* sep = wcschr(ext, L';');
        if (sep == NULL) {
            sep = pathext.buffer + pathext.length;
        }
        *sep = L'\0';
        do {
            if (!WString_reserve(dest, count)) {
                return NULL;
            }
            count = SearchPathW(pathbuf.buffer,
                                progbuf.buffer, ext, dest->capacity,
                                dest->buffer, NULL);
        } while (count > dest->capacity);
        dest->length = count;
        dest->buffer[dest->length] = L'\0';
        if (count > 0) {
            return dest->buffer;
        }
        *sep = L';';
        ext = sep + 1;
    } while (ext < pathext.buffer + pathext.length);
    return NULL;
}

int main() {
    WString_create(&pathbuf);
    WString_create(&pathext);
    WString_create(&progbuf);
    WString_create(&workdir);

    get_workdir(&workdir);

    wchar_t cmd_[] = L"\"echo\" this is a testt";



    WString dest;
    WString_create(&dest);
    const wchar_t* prog = find_program(cmd_, &dest);

    String outbuf;
    String_create(&outbuf);
    unsigned long errorcode;
    if (!subprocess_run_program(prog, cmd_, &outbuf, 5000, &errorcode, SUBPROCESS_STDIN_DEVNULL)) {
        _wprintf(L"error\n");
        return 1;
    }
    _wprintf(L"%S\n", outbuf.buffer);

    return 0;

    wchar_t cmd[1024] = L"This is a test ${poetry -v env info --path} is life $[echo hi]11111";
    DWORD len = 64;
    _wprintf(L"%*.*s\n", len, len, cmd);
    substitute_commands(cmd, &len, 1024);
    _wprintf(L"%*.*s, %c\n", len, len, cmd, cmd[64]);
    return 0;
    MatchNode_init();

    wchar_t json_buf[1025];
    if (!find_file_relative(json_buf, 1024, L"autocmp.json", true)) {
        _printf("Could not find autocmp.json\n");
        return 1;
    }
    _wprintf(L"Opening %s\n", json_buf);

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
    JsonObject *root_obj = JsonObject_get_obj(&obj, "root");
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
        const char *key;
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
            NodeBuilder *node = &stack[stack_ix].node;
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
        const char *key = stack[stack_ix].key;
        MatchNode *node = NodeBuilder_finalize(&stack[stack_ix].node);
        --stack_ix;
        add_node(&stack[stack_ix].node, key, &extr_map, node, &workbuf);
    }
    WString_free(&workbuf);

    MatchNode *root = NodeBuilder_finalize(&stack[0].node);
    MatchNode_set_root(root);
    HashMap_Free(&extr_map);
    JsonObject_free(&obj);

    MatchNode_free();

    return 0;
}
