#include "args.h"
#include "glob.h"
#include "json.h"
#include "match_node.h"
#include "printf.h"


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

int main() {
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

    return 0;
}
