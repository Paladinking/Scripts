#include "glob.h"
#include "json.h"
#include "printf.h"
#include "hashmap.h"
#include "match_node.h"
#include "args.h"

int main() {
    MatchNode_init();

    NodeBuilder b;
    NodeBuilder_create(&b);
    NodeBuilder_add_fixed(&b, L"Hello world", 11, NULL);
    NodeBuilder_add_files(&b, false, NULL);
    MatchNode* git_add = NodeBuilder_finalize(&b);

    NodeBuilder_create(&b);
    MatchNode* git_status = NodeBuilder_finalize(&b);

    NodeBuilder_create(&b);
    NodeBuilder_add_fixed(&b, L"add", wcslen(L"add"), git_add);
    NodeBuilder_add_fixed(&b, L"status", wcslen(L"status"), git_status);
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

    size_t offset, len;
    const wchar_t* cmd = L"git add ";

    WString rem;
    WString_create(&rem);
    MatchNode* final = find_final(cmd, &offset, &rem);

    _wprintf(L"Final: %d, %d, %s\n", offset, len, rem.buffer);

    NodeIterator it;
    NodeIterator_begin(&it, final, rem.buffer);
    const wchar_t* s1;
    while ((s1 = NodeIterator_next(&it)) != NULL) {
        _wprintf(L"%s\n", s1);
    }

    return 0;

    String res;
    String_create(&res);
    unsigned long exit_code;
    _wprintf(L"%S", res.buffer);

    String json_str;
    if (!read_text_file(&json_str, L"autocmp.json")) {
        return 1;
    }


    JsonObject obj;
    if (!json_parse_object(json_str.buffer, &obj, NULL)) {
        _wprintf(L"Failed parsing json file\n");
        String_free(&json_str);
        return 1;
    }
    String_free(&json_str);

    JsonList* desc = JsonObject_get_list(&obj, "description");
    if (desc != NULL) {
        for (DWORD i = 0; i < desc->size; ++i) {
            String* line = JsonList_get_string(desc, i);
            if (line != NULL) {
                _wprintf(L"%S\n", line->buffer);
            }
        }
    }


    return 0;
    WString s;
    WString_create(&s);
    get_workdir(&s);
    HashMap m1;
    WHashMap m2;
    
    HashMap_Create(&m1);
    WHashMap_Create(&m2);

    WalkCtx ctk;
    WalkDir_begin(&ctk, s.buffer);
    Path* path;
    while (WalkDir_next(&ctk, &path) > 0) {
        String s;
        String_create(&s);
        String_from_utf16_str(&s, path->path.buffer);
        HashMap_Insert(&m1, s.buffer, (void*)path->is_dir);
        String_free(&s);
        WHashMap_Insert(&m2, path->path.buffer, (void*)path->is_dir);
        _wprintf(L"%s: %u\n", path->path.buffer, path->is_dir);
    }
    _wprintf(L"-----------------------------\n");

    WalkDir_begin(&ctk, s.buffer);
    while (WalkDir_next(&ctk, &path) > 0) {
        String s;
        String_create(&s);
        String_from_utf16_str(&s, path->path.buffer);
        bool dir = (bool)WHashMap_Value(&m2, path->path.buffer);
        bool dir2 = (bool)HashMap_Value(&m1, s.buffer);
        _wprintf(L"%s: %u %u %u\n", path->path.buffer, path->is_dir, dir, dir2);
        String_free(&s);
    }
    WHashMapFrozen m3;
    WHashMap_Freeze(&m2, &m3);
    WHashMap_Free(&m2);
    HashMapFrozen m4;
    HashMap_Freeze(&m1, &m4);
    HashMap_Free(&m1);

    _wprintf(L"-----------------------------\n");
    WalkDir_begin(&ctk, s.buffer);
    while (WalkDir_next(&ctk, &path) > 0) {
        String s;
        String_create(&s);
        String_from_utf16_str(&s, path->path.buffer);
        bool dir = (bool)WHashMap_Value(&m3.map, path->path.buffer);
        bool dir2 = (bool)HashMap_Value(&m4.map, s.buffer);
        _wprintf(L"%s: %u %u %u\n", path->path.buffer, path->is_dir, dir, dir2);
        String_free(&s);
    }
    WHashElement *e = WHashMap_Find(& m3.map, L"JSON.EXE");
    if (e == NULL) {
        _wprintf(L"null\n");
    } else {
        _wprintf(L"%s: %u\n", e->key, (bool)e->value);
    }

    WHashMap_FreeFrozen(&m3);
    HashMap_FreeFrozen(&m4);

    return 0;
}
