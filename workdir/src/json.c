#define UNICODE
#include <windows.h>
#include "json.h"

bool JsonObject_create(JsonObject* obj) {
    return HashMap_Create(&obj->data);
}

void JsonObject_free(JsonObject* obj) {
    HANDLE heap = GetProcessHeap();
    HashMap* map = &obj->data;
    for (uint32_t i = 0; i < map->bucket_count; ++i) {
        for (uint32_t j = 0; j < map->buckets[i].size; ++j) {
            JsonType* type = map->buckets[i].data[j].value;
            JsonType_free(type);
            HeapFree(heap, 0, type);
        } 
    }
    HashMap_Free(&obj->data);
}

bool JsonObject_insert(JsonObject* obj, const char* str, JsonType type) {
    HashElement* elem = HashMap_Get(&obj->data, str);
    if (elem == NULL) {
        return false;
    }
    if (elem->value != NULL) {
        JsonType_free(elem->value);
    } else {
        elem->value = HeapAlloc(GetProcessHeap(), 0, sizeof(JsonType));
        if (elem->value == NULL) {
            return false;
        }
    }
    memcpy(elem->value, &type, sizeof(type));
    return true;
}

bool JsonObject_insert_obj(JsonObject* obj, const char* key, JsonObject val) {
    JsonType type;
    type.type = JSON_OBJECT;
    type.object = val;
    return JsonObject_insert(obj, key, type);
}

bool JsonObject_insert_list(JsonObject* obj, const char* key, JsonList val) {
    JsonType type;
    type.type = JSON_LIST;
    type.list = val;
    return JsonObject_insert(obj, key, type);
}

bool JsonObject_insert_int(JsonObject* obj, const char* key, int64_t val) {
    JsonType type;
    type.type = JSON_INTEGER;
    type.integer = val;
    return JsonObject_insert(obj, key, type);
}

bool JsonObject_insert_double(JsonObject* obj, const char* key, double val) {
    JsonType type;
    type.type = JSON_DOUBLE;
    type.dbl = val;
    return JsonObject_insert(obj, key, type);
}

bool JsonObject_insert_bool(JsonObject* obj, const char* key, bool val) {
    JsonType type;
    type.type = JSON_BOOL;
    type.b = val;
    return JsonObject_insert(obj, key, type);
}

bool JsonObject_insert_string(JsonObject* obj, const char* key, String val) {
    JsonType type;
    type.type = JSON_STRING;
    type.string = val;
    return JsonObject_insert(obj, key, type);
}

bool JsonObject_insert_null(JsonObject* obj, const char* key) {
    JsonType type;
    type.type = JSON_NULL;
    return JsonObject_insert(obj, key, type);
}

bool JsonObject_has_key(JsonObject* obj, const char* key) {
    return HashMap_Find(&obj->data, key) != NULL;
}

JsonType* JsonObject_get(JsonObject* obj, const char* key) {
    return HashMap_Value(&obj->data, key);
}

JsonObject* JsonObject_get_obj(JsonObject* obj, const char* key) {
    JsonType* type = JsonObject_get(obj, key);
    if (type == NULL || type->type != JSON_OBJECT) {
        return NULL;
    }
    return &type->object;
}

JsonList* JsonObject_get_list(JsonObject* obj, const char* key) {
    JsonType* type = JsonObject_get(obj, key);
    if (type == NULL || type->type != JSON_LIST) {
        return NULL;
    }
    return &type->list;
}

int64_t* JsonObject_get_int(JsonObject* obj, const char* key) {
    JsonType* type = JsonObject_get(obj, key);
    if (type == NULL || type->type != JSON_INTEGER) {
        return NULL;
    }
    return &type->integer;
}

double* JsonObject_get_double(JsonObject* obj, const char* key) {
    JsonType* type = JsonObject_get(obj, key);
    if (type == NULL || type->type != JSON_DOUBLE) {
        return NULL;
    }
    return &type->dbl;
}

bool* JsonObject_get_bool(JsonObject* obj, const char* key) {
    JsonType* type = JsonObject_get(obj, key);
    if (type == NULL || type->type != JSON_BOOL) {
        return NULL;
    }
    return &type->b;
}

String* JsonObject_get_string(JsonObject* obj, const char* key) {
    JsonType* type = JsonObject_get(obj, key);
    if (type == NULL || type->type != JSON_STRING) {
        return NULL;
    }
    return &type->string;
}

bool JsonObject_get_null(JsonObject* obj, const char* key) {
    JsonType* type = JsonObject_get(obj, key);
    if (type == NULL || type->type != JSON_NULL) {
        return false;
    }
    return true;
}

bool JsonObject_remove(JsonObject* obj, const char* key) {
    void* val;
    if (!HashMap_RemoveGet(&obj->data, key, &val)) {
        return false;
    }
    JsonType_free(val);
    HeapFree(GetProcessHeap(), 0, val);
    return true;
}

bool JsonList_create(JsonList* list) {
    list->data = HeapAlloc(GetProcessHeap(), 0, 4 * sizeof(JsonType));
    if (list->data == NULL) {
        list->capacity = 0;
        list->size = 0;
        return false;
    }
    list->capacity = 4;
    list->size = 0;
    return true;
}

void JsonList_free(JsonList* list) {
    for (unsigned ix = 0; ix < list->size; ++ix) {
        JsonType_free(list->data + ix);
    }
    HeapFree(GetProcessHeap(), 0, list->data);
}


bool JsonList_append(JsonList* list, JsonType val) {
    if (list->size == list->capacity) {
        unsigned cap = list->capacity == 0 ? 4 : list->capacity * 2;
        JsonType* data = HeapReAlloc(GetProcessHeap(), 0, list->data, 
                                     sizeof(JsonType) * cap);
        if (data == NULL) {
            return false;
        }
        list->data = data;
        list->capacity = cap;
    }
    memcpy(list->data + list->size, &val, sizeof(JsonType));
    ++(list->size);
    return true;
}

bool JsonList_append_obj(JsonList* list, JsonObject val) {
    JsonType type;
    type.type = JSON_OBJECT;
    type.object = val;
    return JsonList_append(list, type);
}

bool JsonList_append_list(JsonList* list, JsonList val) {
    JsonType type;
    type.type = JSON_LIST;
    type.list = val;
    return JsonList_append(list, type);
}

bool JsonList_append_int(JsonList* list, int64_t val) {
    JsonType type;
    type.type = JSON_INTEGER;
    type.integer = val;
    return JsonList_append(list, type);
}

bool JsonList_append_double(JsonList* list, double val) {
    JsonType type;
    type.type = JSON_DOUBLE;
    type.dbl = val;
    return JsonList_append(list, type);
}

bool JsonList_append_bool(JsonList* list, bool val) {
    JsonType type;
    type.type = JSON_BOOL;
    type.b = val;
    return JsonList_append(list, type);
}

bool JsonList_append_string(JsonList* list, String val) {
    JsonType type;
    type.type = JSON_STRING;
    type.string = val;
    return JsonList_append(list, type);
}

bool JsonList_append_null(JsonList* list) {
    JsonType type;
    type.type = JSON_NULL;
    return JsonList_append(list, type);
}

bool JsonList_insert(JsonList* list, unsigned ix, JsonType val) {
    if (ix > list->size) {
        ix = list->size;
    }
    if (list->size == list->capacity) {
        unsigned cap = list->capacity == 0 ? 4 : list->capacity * 2;
        JsonType* data = HeapReAlloc(GetProcessHeap(), 0, list->data, 
                                     sizeof(JsonType) * cap);
        if (data == NULL) {
            return false;
        }
        list->data = data;
        list->capacity = cap;
    }
    // ix = 0, len = 5, 
    memmove(list->data + ix + 1, list->data + ix, 
            (list->size - ix) * sizeof(JsonType));
    memcpy(list->data + ix, &val, sizeof(JsonType));
    ++(list->size);
    return true;
}

bool JsonList_insert_obj(JsonList* list, unsigned ix, JsonObject val) {
    JsonType type;
    type.type = JSON_OBJECT;
    type.object = val;
    return JsonList_insert(list, ix, type);
}

bool JsonList_insert_list(JsonList* list, unsigned ix, JsonList val) {
    JsonType type;
    type.type = JSON_LIST;
    type.list = val;
    return JsonList_insert(list, ix, type);
}

bool JsonList_insert_int(JsonList* list, unsigned ix, int64_t val) {
    JsonType type;
    type.type = JSON_INTEGER;
    type.integer = val;
    return JsonList_insert(list, ix, type);
}

bool JsonList_insert_double(JsonList* list, unsigned ix, double val) {
    JsonType type;
    type.type = JSON_DOUBLE;
    type.dbl = val;
    return JsonList_insert(list, ix, type);
}

bool JsonList_insert_bool(JsonList* list, unsigned ix, bool val) {
    JsonType type;
    type.type = JSON_BOOL;
    type.b = val;
    return JsonList_insert(list, ix, type);
}

bool JsonList_insert_string(JsonList* list, unsigned ix, String val) {
    JsonType type;
    type.type = JSON_STRING;
    type.string = val;
    return JsonList_insert(list, ix, type);
}

bool JsonList_insert_null(JsonList* list, unsigned ix) {
    JsonType type;
    type.type = JSON_NULL;
    return JsonList_insert(list, ix, type);
}

bool JsonList_set(JsonList* list, unsigned ix, JsonType val) {
    if (ix >= list->size) {
        return false;
    }
    JsonType_free(list->data + ix);
    memcpy(list->data + ix, &val, sizeof(JsonType));
    return true;
}

bool JsonList_set_obj(JsonList* list, unsigned ix, JsonObject val) {
    JsonType type;
    type.type = JSON_OBJECT;
    type.object = val;
    return JsonList_set(list, ix, type);
}

bool JsonList_set_list(JsonList* list, unsigned ix, JsonList val) {
    JsonType type;
    type.type = JSON_LIST;
    type.list = val;
    return JsonList_set(list, ix, type);
}

bool JsonList_set_int(JsonList* list, unsigned ix, int64_t val) {
    JsonType type;
    type.type = JSON_INTEGER;
    type.integer = val;
    return JsonList_set(list, ix, type);
}

bool JsonList_set_double(JsonList* list, unsigned ix, double val) {
    JsonType type;
    type.type = JSON_DOUBLE;
    type.dbl = val;
    return JsonList_set(list, ix, type);
}

bool JsonList_set_bool(JsonList* list, unsigned ix, bool val) {
    JsonType type;
    type.type = JSON_BOOL;
    type.b = val;
    return JsonList_set(list, ix, type);
}

bool JsonList_set_string(JsonList* list, unsigned ix, String val) {
    JsonType type;
    type.type = JSON_STRING;
    type.string = val;
    return JsonList_set(list, ix, type);
}

bool JsonList_set_null(JsonList* list, unsigned ix) {
    JsonType type;
    type.type = JSON_NULL;
    return JsonList_set(list, ix, type);
}

JsonType* JsonList_get(JsonList* list, unsigned ix) {
    if (ix >= list->size) {
        return NULL;
    }
    return list->data + ix;
}

JsonObject* JsonList_get_obj(JsonList* list, unsigned ix) {
    JsonType* type = JsonList_get(list, ix);
    if (type == NULL || type->type != JSON_OBJECT) {
        return NULL;
    }
    return &(type->object);
}

JsonList* JsonList_get_list(JsonList* list, unsigned ix) {
    JsonType* type = JsonList_get(list, ix);
    if (type == NULL || type->type != JSON_LIST) {
        return NULL;
    }
    return &(type->list);
}

int64_t* JsonList_get_int(JsonList* list, unsigned ix) {
    JsonType* type = JsonList_get(list, ix);
    if (type == NULL || type->type != JSON_INTEGER) {
        return NULL;
    }
    return &(type->integer);
}

double* JsonList_get_double(JsonList* list, unsigned ix) {
    JsonType* type = JsonList_get(list, ix);
    if (type == NULL || type->type != JSON_DOUBLE) {
        return NULL;
    }
    return &(type->dbl);
}

bool* JsonList_get_bool(JsonList* list, unsigned ix) {
    JsonType* type = JsonList_get(list, ix);
    if (type == NULL || type->type != JSON_BOOL) {
        return NULL;
    }
    return &(type->b);
}

String* JsonList_get_string(JsonList* list, unsigned ix) {
    JsonType* type = JsonList_get(list, ix);
    if (type == NULL || type->type != JSON_STRING) {
        return NULL;
    }
    return &(type->string);
}

bool JsonList_get_null(JsonList* list, unsigned ix) {
    JsonType* type = JsonList_get(list, ix);
    if (type == NULL || type->type != JSON_NULL) {
        return false;
    }
    return true;
}

bool JsonList_pop(JsonList* list) {
    if (list->size == 0) {
        return false;
    }
    JsonType_free(list->data + list->size - 1);
    --(list->size);
    return true;
}

bool JsonList_remove(JsonList* list, unsigned ix) {
    if (ix >= list->size) {
        return false;
    }
    JsonType_free(list->data + ix);
    memmove(list->data + ix, list->data + ix + 1, 
            (list->size - ix - 1) * sizeof(JsonType));
    return true;
}

void JsonType_free(JsonType* val) {
    switch (val->type) {
        case JSON_OBJECT:
            JsonObject_free(&val->object);
            break;
        case JSON_LIST:
            JsonList_free(&val->list);
            break;
        case JSON_STRING:
            String_free(&val->string);
        default:
            break;
    }
    val->type = JSON_NULL;
}


// Skip until next non-space character
bool skip_spaces(const char** str) {
    const char* s = *str;
    if (*s == '\0') {
        return false;
    }
    while (1) {
        ++s;
        if (*s == '\t' || *s == '\n' || *s == '\r' || *s == ' ') {
            continue;
        }
        *str = s;
        if (*s == '\0') {
            return false;
        }
        return true;
    }
}

bool JsonType_parse(const char** str, JsonType* res);
bool JsonString_parse(const char** str, String* res);
bool JsonObject_parse(const char** str, JsonObject* res);
bool JsonList_parse(const char** str, JsonList* res);
bool JsonBool_parse(const char** str, bool* res);
bool JsonNumber_parse(const char** str, int64_t* i, double* d, bool* is_int);
bool JsonDouble_parse(const char** str, double* d);
bool JsonNull_parse(const char** str);


bool JsonType_parse(const char** str, JsonType* res) {
    const char* s = *str;
    switch (*s) {
        case '{': {
            JsonObject obj;
            if (!JsonObject_parse(&s, &obj)) {
                return false;
            }
            res->type = JSON_OBJECT;
            res->object = obj;
            break;
        }
        case '[': {
            JsonList list;
            if (!JsonList_parse(&s, &list)) {
                return false;
            }
            res->type = JSON_LIST;
            res->list = list;
            break;
        }
        case '"': {
            String string;
            if (!JsonString_parse(&s, &string)) {
                return false;
            }
            res->type = JSON_STRING;
            res->string = string;
            break;
        }
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '-': {
            int64_t i;
            double d;
            bool is_int;
            if (!JsonNumber_parse(&s, &i, &d, &is_int)) {
                return false;
            }
            if (is_int) {
                res->type = JSON_INTEGER;
                res->integer = i;
            } else {
                res->type = JSON_DOUBLE;
                res->dbl = d;
            }
            break;
        }
        case 't':
        case 'f': {
            bool b;
            if (!JsonBool_parse(&s, &b)) {
                return false;
            }
            res->type = JSON_BOOL;
            res->b = b;
            break;
        }
        case 'n': {
            if (!JsonNull_parse(&s)) {
                return false;
            }
            res->type = JSON_NULL;
            break;
        }
        default:
            return false;
    }
    *str = s;
    return true;
}

bool JsonString_parse(const char** str, String* string) {
    const char* s = *str;
    if (*s != '"') {
        return false;
    }
    if (!String_create(string)) {
        return false;
    }
    while (1) {
        char c = *s;
        switch (c) {
            case '\0':
                String_free(string);
                return false;
            case '"':
                *str = s;
                return true;
            case '\\':
                ++s;
                switch (*s) {
                    case 't':
                        String_append(string, '\t');
                        break;
                    case 'n':
                        String_append(string, '\n');
                        break;
                    case 'r':
                        String_append(string, '\r');
                        break;
                    case 'b':
                        String_append(string, '\b');
                        break;
                    case 'f':
                        String_append(string, '\f');
                        break;
                    case '\\':
                    case '/':
                    case '"':
                        String_append(string, *s);
                        break;
                    default:
                        String_free(string);
                        return false;
                }
                break;
            default:
                String_append(string, c);
                break;
        }
        ++s;
    }
}

bool JsonObject_parse(const char** str, JsonObject* obj) {
    const char* s = *str;
    if (*s != '{') {
        return false;
    }
    if (!JsonObject_create(obj)) {
        return false;
    }
    if (!skip_spaces(&s)) {
        goto end;
    }
    if (*s == '}') {
        *str = s;
        return true;
    }

    while (1) {
        String label;
        if (!JsonString_parse(&s, &label)) {
            goto end;
        }
        if (!skip_spaces(&s) || *s != ':') {
            String_free(&label);
            goto end;
        }
        if (!skip_spaces(&s)) {
            String_free(&label);
            goto end;
        }
        JsonType type;
        if (!JsonType_parse(&s, &type)) {
            String_free(&label);
            goto end;
        }
        bool res = JsonObject_insert(obj, label.buffer, type);
        String_free(&label);
        if (!res) {
            JsonType_free(&type);
            goto end;
        }
        if (!skip_spaces(&s)) {
            goto end;
        }
        if (*s == '}') {
            *str = s;
            return true;
        }
        if (*s != ',') {
            goto end;
        }
        if (!skip_spaces(&s)) {
            goto end;
        }
    }
end:
    JsonObject_free(obj);
    return false;
}

bool JsonList_parse(const char** str, JsonList* list) {
    const char* s = *str;
    if (*s != '[') {
        return false;
    }
    if (!JsonList_create(list)) {
        return false;
    }
    if (!skip_spaces(&s)) {
        goto end;
    }
    if (*s == ']') {
        *str = s;
        return true;
    }
    while (1) {
        JsonType type;
        if (!JsonType_parse(&s, &type)) {
            goto end;
        }
        if (!JsonList_append(list, type)) {
            JsonType_free(&type);
            goto end;
        }
        if (!skip_spaces(&s)) {
            goto end;
        }
        if (*s == ']') {
            *str = s;
            return true;
        }
        if (*s != ',') {
            goto end;
        }
        if (!skip_spaces(&s)) {
            goto end;
        }
    }
end:
    JsonList_free(list);
    return false;
}

bool JsonNumber_parse(const char** str, int64_t* i, double* d, bool* is_int) {
    const char* s = *str;
    bool negative = false;
    *is_int = true;
    if (*s == '-')  {
        ++s;
        negative = true;
    }
    if (*s < '0' && *s > '9') {
        return false;
    }
    *i = (*s - '0');
    ++s;
    if (*(s - 1) != '0') {
        while (*s >= '0' && *s <= '9') {
            *i *= 10;
            *i += *s - '0';
            ++s;
        }
    } else if (*s >= '0' && *s <= '9') {
        return false;
    }
    if (*s == '.') {
        *is_int = false;
        *d = *i;
        ++s;
        if (*s < '0' || *s > '9') {
            return false;
        }
        double factor = 0.1;
        while (*s >= '0' && *s <= '9') {
            *d += (*s - '0') * factor;
            factor *= 0.1;
            ++s;
        }
    }
    if (*s == 'e' || *s == 'E') {
        if (*is_int) {
            *is_int = false;
            *d = *i;
        }
        ++s;
        int64_t exp = 0;
        bool neg_exp = false;
        if (*s == '-') {
            neg_exp = true;
            ++s;
        } else if (*s == '+') {
            ++s;
        }
        if (*s < '0' || *s > '9') {
            return false;
        }
        while (*s >= '0' && *s <= '9') {
            exp = exp * 10;
            exp += *s - '0';
            ++s;
        }
        double factor = 1.0;
        while (exp > 15) {
            exp -= 15;
            factor *= 1000000000000000;
        }
        int64_t exps[15] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 
                            1000000000, 10000000000, 100000000000, 1000000000000, 10000000000000,
                            100000000000000};
        factor *= exps[exp];
        if (neg_exp) {
            *d /= factor;
        } else {
            *d *= factor;
        }
    }
    *str = s - 1;
    if (negative) {
        if (*is_int) {
            *i = - *i;
        } else {
            *d = - *d;
        }
    }
    return true;
}


int main() {

}
