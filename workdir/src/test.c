#include "glob.h"
#include "json.h"
#include "printf.h"
#include "match_node.h"
#include "args.h"

int main() {
    MatchNode_init();

    String res;
    String_create(&res);
    unsigned long exit_code;
    _wprintf(L"%S", res.buffer);

    String json_str;
    if (!read_text_file(&json_str, L"autocmp.json")) {
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

    JsonObject* extr = JsonObject_get_obj(&obj, "extern");
    HashMap extr_map;
    HashMap_Create(&extr_map);

    if (extr != NULL) {
        HashMapIterator it;
        HashMapIter_Begin(&it, &extr->data);
        HashElement* elem;
        while ((elem = HashMapIter_Next(&it)) != NULL) {
            JsonType* type = elem->value;
            if (type->type != JSON_OBJECT) {
                continue;
            }
            JsonObject* dyn_obj = &type->object;

            String* cmd = JsonObject_get_string(dyn_obj, "cmd");
            String* sep = JsonObject_get_string(dyn_obj, "separator");
            String* inval = JsonObject_get_string(dyn_obj, "invalidation");
            if (cmd == NULL || sep == NULL || inval == NULL || cmd->length == 0 ||
                inval->length == 0) {
                _wprintf(L"Invalid extern spec for %S\n", elem->key);
                continue;
            }
            WString wcmd;
            WString_create(&wcmd);
            // use buffer for separator as well
            if (!WString_from_utf8_bytes(&wcmd, sep->buffer, sep->length) || wcmd.length != 1) {
                _wprintf(L"Invalid separator for %S\n", elem->key);
                WString_free(&wcmd);
                continue;
            } 
            wchar_t sep_char = wcmd.buffer[0];
            if (!WString_from_utf8_bytes(&wcmd, cmd->buffer, cmd->length)) {
                _wprintf(L"Invalid command from %S\n", elem->key);
                WString_free(&wcmd);
            }
            enum Invalidation invalidation = INVALID_NEVER;
            if (inval->buffer[0] == 'c' || inval->buffer[0] == 'C') {
                invalidation = INVALID_CHDIR;
            } else if (inval->buffer[0] == 'a' || inval->buffer[0] == 'A') {
                invalidation = INVALID_ALWAYS;
            }
            DynamicMatch* match = DynamicMatch_create(wcmd.buffer, invalidation, sep_char);
            HashMap_Insert(&extr_map, elem->key, match);

            _wprintf(L"%S cmd: \"%s\", sep: 0x%x, invalid: %d\n", 
                     elem->key, wcmd.buffer, sep_char, invalidation);
            WString_free(&wcmd);
        }

    }

    return 0;
}
