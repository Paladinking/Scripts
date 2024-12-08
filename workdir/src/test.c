#include "glob.h"
#include "json.h"
#include "printf.h"
#include "hashmap.h"
#include "whashmap.h"

enum Invalidation {
    ALWAYS, NEVER, CHDIR
};

#define EXTRA_ANY_ITEM 1
#define EXTRA_EXISTING_FILES 2
#define EXTRA_ANY_FILES 4
#define EXTRA_DEFAULT 8

WHashMap nodes;

typedef struct DynamicMatches {
    // First is length of first string, followed by first string
    // After that, length of second string etc...
    const wchar_t* matches; // NULL when invalidated
    unsigned match_count;
    enum Invalidation invalidation;
    wchar_t separator;
} DynamicMatches;

typedef struct StaticMatches {
    const wchar_t* matches;
    unsigned match_count;
} StaticMatches;

typedef struct CmpNode {
    StaticMatches fixed;
    DynamicMatches* dynamic;
    unsigned dynamic_count;
    unsigned extra;
    // If extra has EXTRA_ANY_ITEM, contains one child that
    // all matches lead to. All other flags are ignored.
    // Otherwise first has dynamic_count children for dynamic matches.
    // If extra has EXTRA_ANY_FILES or EXTRA_EXISTING_FILES next child
    // is anything matched by files.
    // If extra has EXTRA_DEFAULT, next child is default child.
    struct CmpNode* children;
    wchar_t hash_postfix[5]; // Prefix used in global hashmap for finding next node
} CmpNode;

bool is_filelike(const wchar_t* str, unsigned len) {
    if (len == 0) {
        return false;
    }
    unsigned ix = 0;
    for (; ix < len; ++ix) {
        if (str[ix] == L':') {
            if (ix != 1) {
                return false;
            }
            if (!(str[ix - 1] >= 'a' && str[ix - 1] <= 'z') &&
                !(str[ix - 1] >= 'A' && str[ix - 1] <= 'Z')) {
                return false;
            }
        } else if (str[ix] == L'|' || str[ix] == L'>' || str[ix] == L'<' || str[ix] == L'*' || str[ix] == L'?') {
            return false;
        }
    }
    return true;
}

// str should have 6 byte space for postfix + null-terminator
// quotes should be gone from str
CmpNode* find_next_node(CmpNode* current, wchar_t* str, unsigned len) {
    if (current->extra & EXTRA_ANY_ITEM) {
        return current->children;
    }
    str[len + 0] = current->hash_postfix[0];
    str[len + 1] = current->hash_postfix[1];
    str[len + 2] = current->hash_postfix[2];
    str[len + 3] = current->hash_postfix[3];
    str[len + 4] = current->hash_postfix[4];
    str[len + 5] = L'\0';
    // This finds all static and dynamic matches
    CmpNode* match = WHashMap_Value(&nodes, str);
    str[len] = L'\0';
    if (match != NULL) {
        return match;
    }

    unsigned child_offset = current->dynamic_count;
    if (current->extra & EXTRA_ANY_FILES) {
        if (is_filelike(str, len)) {
            return current->children + child_offset;
        }
        ++child_offset;
    } else if (current->extra & EXTRA_EXISTING_FILES) {
        if (is_file(str)) {
            return current->children + child_offset;
        }
        ++child_offset;
    }
    if (current->extra & EXTRA_DEFAULT) {
        return current->children + child_offset;
    }
    return current;
}


int main() {
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
