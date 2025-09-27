#ifndef JSON_C_H
#define JSON_C_H

#include "linkedhashmap.h"
#include "dynamic_string.h"

struct JsonObject;
struct JsonList;
struct JsonType;

typedef struct JsonObject JsonObject;
typedef struct JsonList JsonList;
typedef struct JsonType JsonType;

struct JsonObject {
    LinkedHashMap data;
};

struct JsonList {
    JsonType* data;
    unsigned size;
    unsigned capacity;
};

struct JsonType {
    enum { JSON_OBJECT, JSON_LIST, JSON_INTEGER, JSON_DOUBLE, JSON_BOOL, JSON_STRING, JSON_NULL } type;
    union {
        JsonObject object;
        JsonList list;
        int64_t integer;
        double dbl;
        bool b;
        String string;
    };
};


bool JsonObject_create(JsonObject* obj);

void JsonObject_free(JsonObject* obj);

bool JsonObject_insert(JsonObject* obj, const char* key, JsonType val);

bool JsonObject_insert_obj(JsonObject* obj, const char* key, JsonObject val);
bool JsonObject_insert_list(JsonObject* obj, const char* key, JsonList val);
bool JsonObject_insert_int(JsonObject* obj, const char* key, int64_t val);
bool JsonObject_insert_double(JsonObject* obj, const char* key, double val);
bool JsonObject_insert_bool(JsonObject* obj, const char* key, bool val);
bool JsonObject_insert_string(JsonObject* obj, const char* key, String val);
bool JsonObject_insert_null(JsonObject* obj, const char* key);

bool JsonObject_has_key(JsonObject* obj, const char* key);

JsonType* JsonObject_get(JsonObject* obj, const char* key);

JsonObject* JsonObject_get_obj(JsonObject* obj, const char* key);
JsonList* JsonObject_get_list(JsonObject* obj, const char* key);
int64_t* JsonObject_get_int(JsonObject* obj, const char* key);
double* JsonObject_get_double(JsonObject* obj, const char* key);
bool* JsonObject_get_bool(JsonObject* obj, const char* key);
String* JsonObject_get_string(JsonObject* obj, const char* key);
bool JsonObject_get_null(JsonObject* obj, const char* key);

bool JsonObject_remove(JsonObject* obj, const char* key);

bool JsonList_create(JsonList* list);

bool JsonList_append(JsonList* list, JsonType val);

bool JsonList_append_obj(JsonList* list, JsonObject val);
bool JsonList_append_list(JsonList* list, JsonList val);
bool JsonList_append_int(JsonList* list, int64_t val);
bool JsonList_append_double(JsonList* list, double val);
bool JsonList_append_bool(JsonList* list, bool val);
bool JsonList_append_string(JsonList* list, String val);
bool JsonList_append_null(JsonList* list);

bool JsonList_insert(JsonList* list, unsigned ix, JsonType val);

bool JsonList_insert_obj(JsonList* list, unsigned ix, JsonObject val);
bool JsonList_insert_list(JsonList* list, unsigned ix, JsonList val);
bool JsonList_insert_int(JsonList* list, unsigned ix, int64_t val);
bool JsonList_insert_double(JsonList* list, unsigned ix, double val);
bool JsonList_insert_bool(JsonList* list, unsigned ix, bool val);
bool JsonList_insert_string(JsonList* list, unsigned ix, String val);
bool JsonList_insert_null(JsonList* list, unsigned ix);

bool JsonList_set(JsonList* list, unsigned ix, JsonType val);

bool JsonList_set_obj(JsonList* list, unsigned ix, JsonObject val);
bool JsonList_set_list(JsonList* list, unsigned ix, JsonList val);
bool JsonList_set_int(JsonList* list, unsigned ix, int64_t val);
bool JsonList_set_double(JsonList* list, unsigned ix, double val);
bool JsonList_set_bool(JsonList* list, unsigned ix, bool val);
bool JsonList_set_string(JsonList* list, unsigned ix, String val);
bool JsonList_set_null(JsonList* list, unsigned ix);

JsonType* JsonList_get(JsonList* list, unsigned ix);

JsonObject* JsonList_get_obj(JsonList* list, unsigned ix);
JsonList* JsonList_get_list(JsonList* list, unsigned ix);
int64_t* JsonList_get_int(JsonList* list, unsigned ix);
double* JsonList_get_double(JsonList* list, unsigned ix);
bool* JsonList_get_bool(JsonList* list, unsigned ix);
String* JsonList_get_string(JsonList* list, unsigned ix);
bool JsonList_get_null(JsonList* list, unsigned ix);

bool JsonList_pop(JsonList* list);
bool JsonList_remove(JsonList* list, unsigned ix);

void JsonList_free(JsonList* list);

void JsonType_free(JsonType* val);

bool json_parse_object(const char* str, JsonObject* obj, String_noinit* errormsg);

bool json_parse_type(const char* str, JsonType* val, String_noinit* errormsg);

bool json_object_to_string(const JsonObject* obj, String* res);

bool json_type_to_string(const JsonType* v, String* res);

#endif
