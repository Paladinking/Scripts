#define UNICODE
#include <windows.h>
#include "json.h"

bool JsonObject_create(JsonObject* obj) {
    return LinkedHashMap_Create(&obj->data);
}

void JsonObject_free(JsonObject* obj) {
    LinkedHashMap* map = &obj->data;
    for (uint32_t i = 0; i < map->bucket_count; ++i) {
        for (uint32_t j = 0; j < map->buckets[i].size; ++j) {
            JsonType* type = map->buckets[i].data[j].value;
            JsonType_free(type);
            Mem_free(type);
        } 
    }
    LinkedHashMap_Free(&obj->data);
}

bool JsonObject_insert(JsonObject* obj, const char* str, JsonType type) {
    LinkedHashElement* elem = LinkedHashMap_Get(&obj->data, str);
    if (elem == NULL) {
        return false;
    }
    if (elem->value != NULL) {
        JsonType_free(elem->value);
    } else {
        elem->value = Mem_alloc(sizeof(JsonType));
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
    return LinkedHashMap_Find(&obj->data, key) != NULL;
}

JsonType* JsonObject_get(JsonObject* obj, const char* key) {
    return LinkedHashMap_Value(&obj->data, key);
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
    if (!LinkedHashMap_RemoveGet(&obj->data, key, &val)) {
        return false;
    }
    JsonType_free(val);
    Mem_free(val);
    return true;
}

bool JsonList_create(JsonList* list) {
    list->data = Mem_alloc(4 * sizeof(JsonType));
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
    Mem_free(list->data);
}


bool JsonList_append(JsonList* list, JsonType val) {
    if (list->size == list->capacity) {
        unsigned cap = list->capacity == 0 ? 4 : list->capacity * 2;
        JsonType* data = Mem_realloc(list->data, sizeof(JsonType) * cap);
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
        JsonType* data = Mem_realloc(list->data, sizeof(JsonType) * cap);
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

typedef struct ParseCtx {
    const char* root;
    unsigned row;
    unsigned col;
    String errormsg;
} JsonParseCtx;

void find_position(JsonParseCtx* ctx, const char* pos) {
    ctx->row = 1;
    ctx->col = 1;
    const char* s = ctx->root;
    char last = '\0';
    while (s < pos) {
        if (*s == '\r' || (*s == '\n' && last != '\r')) {
            ctx->col = 1;
            ++ctx->row;
        } else {
            ++ctx->col;
        }
        last = *s;
        ++s;
    }
}

bool format_int(String* dst, int64_t i) {
    if (i == 0) {
        return String_append(dst, '0');
    }
    uint64_t n;
    if (i < 0) {
        if (i == INT64_MIN) {
            n = ((uint64_t)INT64_MAX) + 1;
        } else {
            n = -i;
        }
        if (!String_append(dst, '-')) {
            return false;
        }
    }  else {
        n = i;
    }
    unsigned ix = 0;
    char buf[20];
    while (n > 0) {
        buf[ix] = (n % 10) + '0';
        ++ix;
        n /= 10;
    }
    while (ix > 0) {
        if (!String_append(dst, buf[ix - 1])) {
            return false;
        }
        --ix;
    }
    return true;
}

bool unexpected_eof(const char* s, JsonParseCtx* ctx) {
    find_position(ctx, s);
    if (ctx->errormsg.buffer == NULL) {
        String_create(&ctx->errormsg);
    }
    String_clear(&ctx->errormsg);
    String_extend(&ctx->errormsg, "Error: Unexpeced EOF at: row ");
    format_int(&ctx->errormsg, ctx->row);
    String_extend(&ctx->errormsg, ", col ");
    format_int(&ctx->errormsg, ctx->col);
    return false;
}

bool expected_char(const char* s, JsonParseCtx* ctx, char expected, char found) {
    find_position(ctx, s);
    if (ctx->errormsg.buffer == NULL) {
        String_create(&ctx->errormsg);
    }
    String_clear(&ctx->errormsg);
    String_extend(&ctx->errormsg, "Error: Expected character '");
    String_append(&ctx->errormsg, expected);
    String_extend(&ctx->errormsg, "' at row ");
    format_int(&ctx->errormsg, ctx->row);
    String_extend(&ctx->errormsg, ", col ");
    format_int(&ctx->errormsg, ctx->col);
    return false;
}

bool unexpected_char(const char* s, JsonParseCtx* ctx, char found) {
    find_position(ctx, s);
    if (ctx->errormsg.buffer == NULL) {
        String_create(&ctx->errormsg);
    }
    String_clear(&ctx->errormsg);
    String_extend(&ctx->errormsg, "Error: Invalid character '");
    String_append(&ctx->errormsg, found);
    String_extend(&ctx->errormsg, "' at row ");
    format_int(&ctx->errormsg, ctx->row);
    String_extend(&ctx->errormsg, ", col ");
    format_int(&ctx->errormsg, ctx->col);
    return false;
}

bool invalid_literal(const char* s, JsonParseCtx* ctx, const char* type) {
    find_position(ctx, s);
    if (ctx->errormsg.buffer == NULL) {
        String_create(&ctx->errormsg);
    }
    String_clear(&ctx->errormsg);
    String_extend(&ctx->errormsg, "Error: Invalid ");
    String_extend(&ctx->errormsg, type);
    String_extend(&ctx->errormsg, " at row ");
    format_int(&ctx->errormsg, ctx->row);
    String_extend(&ctx->errormsg, ", col ");
    format_int(&ctx->errormsg, ctx->col);
    return false;
}

// Skip until next non-space character
bool skip_spaces(const char** str, JsonParseCtx* ctx) {
    const char* s = *str;
    if (*s == '\0') {
        return unexpected_eof(s, ctx);
    }
    while (1) {
        ++s;
        if (*s == '\t' || *s == '\n' || *s == '\r' || *s == ' ') {
            continue;
        }
        *str = s;
        if (*s == '\0') {
            return unexpected_eof(s, ctx);
        }
        return true;
    }
}

bool JsonType_parse(const char** str, JsonType* res, JsonParseCtx* ctx);
bool JsonString_parse(const char** str, String* res, JsonParseCtx* ctx);
bool JsonObject_parse(const char** str, JsonObject* res, JsonParseCtx* ctx);
bool JsonList_parse(const char** str, JsonList* res, JsonParseCtx* ctx);
bool JsonBool_parse(const char** str, bool* res, JsonParseCtx* ctx);
bool JsonNumber_parse(const char** str, int64_t* i, double* d, bool* is_int, JsonParseCtx* ctx);
bool JsonDouble_parse(const char** str, double* d, JsonParseCtx* ctx);
bool JsonNull_parse(const char** str, JsonParseCtx* ctx);


bool JsonType_parse(const char** str, JsonType* res, JsonParseCtx* ctx) {
    const char* s = *str;
    switch (*s) {
        case '{': {
            JsonObject obj;
            if (!JsonObject_parse(&s, &obj, ctx)) {
                return false;
            }
            res->type = JSON_OBJECT;
            res->object = obj;
            break;
        }
        case '[': {
            JsonList list;
            if (!JsonList_parse(&s, &list, ctx)) {
                return false;
            }
            res->type = JSON_LIST;
            res->list = list;
            break;
        }
        case '"': {
            String string;
            if (!JsonString_parse(&s, &string, ctx)) {
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
            if (!JsonNumber_parse(&s, &i, &d, &is_int, ctx)) {
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
            if (!JsonBool_parse(&s, &b, ctx)) {
                return false;
            }
            res->type = JSON_BOOL;
            res->b = b;
            break;
        }
        case 'n': {
            if (!JsonNull_parse(&s, ctx)) {
                return false;
            }
            res->type = JSON_NULL;
            break;
        }
        case '\0':
            return unexpected_eof(s, ctx);
        default:
            return unexpected_char(s, ctx, *s);
    }
    *str = s;
    return true;
}

bool JsonString_parse(const char** str, String* string, JsonParseCtx* ctx) {
    const char* s = *str;
    if (*s != '"') {
        return false;
    }
    if (!String_create(string)) {
        return false;
    }
    ++s;
    while (1) {
        char c = *s;
        switch (c) {
            case '\0':
                String_free(string);
                return unexpected_eof(s, ctx);
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
                        return unexpected_char(s, ctx, *s);
                }
                break;
            default:
                String_append(string, c);
                break;
        }
        ++s;
    }
}

bool JsonObject_parse(const char** str, JsonObject* obj, JsonParseCtx* ctx) {
    const char* s = *str;
    if (*s != '{') {
        return expected_char(s, ctx, '{', *s);
    }
    if (!JsonObject_create(obj)) {
        return false;
    }
    if (!skip_spaces(&s, ctx)) {
        goto end;
    }
    if (*s == '}') {
        *str = s;
        return true;
    }

    while (1) {
        String label;
        if (!JsonString_parse(&s, &label, ctx)) {
            goto end;
        }
        if (!skip_spaces(&s, ctx)) {
            String_free(&label);
            goto end;
        }
        if (*s != ':') {
            String_free(&label);
            expected_char(s, ctx, ':', *s);
            goto end;
        }
        JsonType type;
        if (!skip_spaces(&s, ctx) || !JsonType_parse(&s, &type, ctx)) {
            String_free(&label);
            goto end;
        }
        bool res = JsonObject_insert(obj, label.buffer, type);
        String_free(&label);
        if (!res) {
            JsonType_free(&type);
            goto end;
        }
        if (!skip_spaces(&s, ctx)) {
            goto end;
        }
        if (*s == '}') {
            *str = s;
            return true;
        }
        if (*s != ',') {
            expected_char(s, ctx, ',', *s);
            goto end;
        }
        if (!skip_spaces(&s, ctx)) {
            goto end;
        }
    }
end:
    JsonObject_free(obj);
    return false;
}

bool JsonList_parse(const char** str, JsonList* list, JsonParseCtx* ctx) {
    const char* s = *str;
    if (*s != '[') {
        return expected_char(s, ctx, '[', *s);
    }
    if (!JsonList_create(list)) {
        return false;
    }
    if (!skip_spaces(&s, ctx)) {
        goto end;
    }
    if (*s == ']') {
        *str = s;
        return true;
    }
    while (1) {
        JsonType type;
        if (!JsonType_parse(&s, &type, ctx)) {
            goto end;
        }
        if (!JsonList_append(list, type)) {
            JsonType_free(&type);
            goto end;
        }
        if (!skip_spaces(&s, ctx)) {
            goto end;
        }
        if (*s == ']') {
            *str = s;
            return true;
        }
        if (*s != ',') {
            expected_char(s, ctx, ',', *s);
            goto end;
        }
        if (!skip_spaces(&s, ctx)) {
            goto end;
        }
    }
end:
    JsonList_free(list);
    return false;
}


bool JsonNumber_parse(const char** str, int64_t* i, double* d, bool* is_int, JsonParseCtx* ctx) {
    const char* s = *str;
    uint64_t n;
    bool negative = false;
    *is_int = true;
    if (*s == '-')  {
        ++s;
        negative = true;
    }
    if (*s < '0' || *s > '9') {
        return invalid_literal(*str, ctx, "number");
    }
    n = (*s - '0');
    *d = (*s - '0');
    ++s;
    if (*(s - 1) != '0') {
        while (*s >= '0' && *s <= '9') {
            n *= 10;
            *d *= 10;
            n += *s - '0';
            *d += *s - '0';
            ++s;
        }
    } else if (*s >= '0' && *s <= '9') {
        return invalid_literal(*str, ctx, "number");
    }
    if (*s == '.') {
        *is_int = false;
        ++s;
        if (*s < '0' || *s > '9') {
            return invalid_literal(*str, ctx, "number");
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
            return invalid_literal(*str, ctx, "number");
        }
        while (*s >= '0' && *s <= '9') {
            exp = exp * 10;
            exp += *s - '0';
            ++s;
        }
        double factor = 1.0;
        while (exp > 15) {
            exp -= 15;
            // 10^15 is the largest power of 10 that can be fully represented by a double
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

bool JsonBool_parse(const char** str, bool* b, JsonParseCtx* ctx) {
    const char* s = *str;
    if (strncmp(s, "true", 4) == 0) {
        *b = true;
        *str = s + 3;
        return true;
    }
    if (strncmp(s, "false", 5) == 0) {
        *b = false;
        *str = s + 4;
        return true;
    }
    return invalid_literal(*str, ctx, "keyword");
}

bool JsonNull_parse(const char** str, JsonParseCtx* ctx) {
    if (strncmp(*str, "null", 4) == 0) {
        *str = *str + 3;
        return true;
    }
    return invalid_literal(*str, ctx, "keyword");
}

bool json_parse_object(const char* str, JsonObject* obj, String_noinit* errormsg) {
    JsonParseCtx ctx;
    ctx.errormsg.buffer = NULL;
    ctx.root = str;
    if ((*str != '{' && !skip_spaces(&str, &ctx)) || !JsonObject_parse(&str, obj, &ctx)) {
        if (errormsg == NULL) {
            String_free(&ctx.errormsg);
        } else {
            *errormsg = ctx.errormsg;
        }
        return false;
    }
    return true;
}

bool json_parse_type(const char* str, JsonType* type, String_noinit* errormsg) {
    JsonParseCtx ctx;
    ctx.errormsg.buffer = NULL;
    ctx.root = str;
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
        ++str;
    }
    if (!JsonType_parse(&str, type, &ctx)) {
        if (errormsg == NULL) {
            String_free(&ctx.errormsg);
        } else {
            *errormsg = ctx.errormsg;
        }
        return false;
    }
    return true;
}


bool double_to_string(double d, String* dest) {
    if (d == 0.0) {
        return String_append_count(dest, "0.0", 3);
    }
    bool negative = d < 0;
    if (d < 0) {
        d = -d;
    }
    int64_t exponent = 0;
    if (d < 0.000001) {
        while (d < 1.0) {
            d *= 10.0;
            --exponent;
        }
    } else if (d > 1000000) {
        while (d > 10.0) {
            d /= 10.0;
            ++exponent;
        }
    }
    uint64_t whole_part = (uint64_t)d;
    d = d - whole_part;
    uint64_t leading_zeroes = 0;
    uint64_t frac_part = 0;
    while (d > 0.000000000001 && d < 0.1) {
        ++leading_zeroes;
        d *= 10;
    }
    if (d >= 0.1) {
        for (uint64_t j = 0; j < 12; ++j) {
            d = d * 10;
            uint64_t i = (uint64_t) d;
            frac_part = frac_part * 10 + i;
            d = d - i;
        }
        if (d > 0.5) {
            frac_part += 1;
        }
        while (frac_part % 10 == 0) {
            frac_part = frac_part / 10;
        }
    }
    if (negative) {
        if (!String_append(dest, '-')) {
            return false;
        }
    }
    if (!String_format_append(dest, "%llu.", whole_part)) {
        return false;
    }
    for (uint64_t i = 0; i < leading_zeroes; ++i) {
        if (!String_append(dest, '0')) {
            return false;
        }
    }
    if (!String_format_append(dest, "%llu", frac_part)) {
        return false;
    }
    if (exponent != 0) {
        return String_format_append(dest, "e%lld", exponent);
    }
    return true;
}

bool write_json_string(const char* in, uint64_t len, String* out) {
    if (!String_append(out, '"')) {
        return false;
    }

    for (uint32_t i = 0; i < len;) {
        uint8_t c = (uint8_t)in[i];
        if (c >= 0x20 && c < 0x7F) {
            if (c == '\\' || c == '"') {
                if (!String_append(out, '\\')) {
                    return false;
                }
            }
            if (!String_append(out, (char)c)) {
                return false;
            }
        } else if (c == '\r') {
            if (!String_append_count(out, "\\r", 2)) {
                return false;
            }
        } else if (c == '\n') {
            if (!String_append_count(out, "\\n", 2)) {
                return false;
            }
        } else if (c == '\t') {
            if (!String_append_count(out, "\\t", 2)) {
                return false;
            }
        } else if (c == '\b') {
            if (!String_append_count(out, "\\b", 2)) {
                return false;
            }
        } else if (c == '\f') {
            if (!String_append_count(out, "\\f", 2)) {
                return false;
            }
        } else {
            if (!String_append(out, (char)c)) {
                return false;
            }
        }
        ++i;
    }

    return String_append(out, '"');
}

bool json_object_to_string(const JsonObject* obj, String* res) {
    JsonType t;
    t.type = JSON_OBJECT;
    t.object = *obj;
    return json_type_to_string(&t, res);
}

bool json_type_to_string(const JsonType* v, String* res) {
    char fmtbuf[64];
    switch (v->type) {
    case JSON_OBJECT:
        if (!String_append(res, '{')) {
            return false;
        }
        {
            LinkedHashMapIterator it;
            LinkedHashMapIter_Begin(&it, (LinkedHashMap*)&v->object.data);
            LinkedHashElement* el;
            uint32_t count = 0;
            while ((el = LinkedHashMapIter_Next(&it)) != NULL) {
                if (count > 0) {
                    if (!String_append(res, ',')) {
                        return false;
                    }
                }
                ++count;
                if (!write_json_string(el->key, strlen(el->key), res)) {
                    return false;
                }
                if (!String_append(res, ':')) {
                    return false;
                }
                if (!json_type_to_string(el->value, res)) {
                    return false;
                }
            }
        }
        if (!String_append(res, '}')) {
            return false;
        }
        return true;
    case JSON_LIST:
        if (!String_append(res, '[')) {
            return false;
        }
        for (uint32_t i = 0; i< v->list.size; ++i) {
            if (i > 0) {
                if (!String_append(res, ',')) {
                    return false;
                }
            }
            if (!json_type_to_string(&v->list.data[i], res)) {
                return false;
            }
        }
        if (!String_append(res, ']')) {
            return false;
        }
        return true;
    case JSON_INTEGER:
        return String_format_append(res, "%llu", v->integer);
    case JSON_DOUBLE:
        return double_to_string(v->dbl, res);
    case JSON_BOOL:
        if (v->b) {
            return String_append_count(res, "true", 4);
        } else {
            return String_append_count(res, "false", 5);
        }
    case JSON_STRING:
        return write_json_string(v->string.buffer, v->string.length, res);
    case JSON_NULL:
        return String_append_count(res, "null", 4);
    }
}

#ifdef JSON_TESTS
#include <stdio.h>
#include <assert.h>

double rel_diff(double a, double b) {
    double c = a < 0 ? -a : a;
    double d = b < 0 ? -b : b;
    d = c > d ? c : d;
    if (d == 0.0) {
        return 0.0;
    }
    double diff = a - b;
    diff = diff < 0 ? -diff : diff;
    return diff / d;
}

#define TEST_INT(str, res) do { \
    const char* s = str;        \
    JsonParseCtx ctx;           \
    ctx.errormsg.buffer = NULL; \
    ctx.root = s;               \
    int64_t i;                  \
    double d;                   \
    bool is_int;                \
    assert(JsonNumber_parse(&s, &i, &d, &is_int, &ctx)); \
    assert(is_int);             \
    printf("%lld, %lld\n", i, (int64_t)res); \
    assert(i == res);           \
} while (0)

#define TEST_DOUBLE(str, res) do { \
    const char* s = str;        \
    JsonParseCtx ctx;           \
    ctx.errormsg.buffer = NULL; \
    ctx.root = s;               \
    int64_t i;                  \
    double d;                   \
    bool is_int;                \
    assert(JsonNumber_parse(&s, &i, &d, &is_int, &ctx)); \
    assert(!is_int);             \
    printf("%g, %g, %g\n", d, res, rel_diff(d, res)); \
    if (rel_diff(d, res) > 1e-10) {              \
        assert(d == res);        \
    }                            \
} while (0)

int main() {
    TEST_INT("123", 123);
    TEST_INT("-12345678", -12345678);
    TEST_DOUBLE("123.456", 123.456);
    TEST_DOUBLE("-0.999999", -0.999999);
    TEST_DOUBLE("1e-20", 1e-20);
    TEST_DOUBLE("0.00214125E70", 0.00214125E70);
    TEST_DOUBLE("-0.00214125E+70", -0.00214125E70);
    TEST_DOUBLE("123456789101112131415.0", 123456789101112131415.0);
    TEST_INT("-9223372036854775807", )

    const char* str = "{\"Hello\": [0, -0.0, -1123, true, null, {\"This\": \"Is the fin\\\"al countdown\", \"Is a test\": -2.345e24}]}";
    JsonObject obj;
    String errormsg;
    if (!json_parse_object(str, &obj, &errormsg)) {
        printf("%s\n", errormsg.buffer);
        assert(false);
    }
    JsonList* list = JsonObject_get_list(&obj, "Hello");
    assert(list != NULL);
    assert(list->size == 6);
    assert(JsonList_get_int(list, 0) != NULL && *JsonList_get_int(list, 0) == 0);
    assert(JsonList_get_double(list, 1) != NULL && *JsonList_get_double(list, 1) == -0.0);
    assert(JsonList_get_int(list, 2) != NULL && *JsonList_get_int(list, 2) == -1123);
    assert(JsonList_get_bool(list, 3) != NULL && *JsonList_get_bool(list, 3) == true);
    assert(JsonList_get_null(list, 4));
    JsonObject* o = JsonList_get_obj(list, 5);
    assert(o != NULL);
    assert(JsonObject_get_string(o, "This") != NULL && strcmp(JsonObject_get_string(o, "This")->buffer, "Is the fin\"al countdown") == 0);
    assert(JsonObject_get_double(o, "Is a test") != NULL && rel_diff(*JsonObject_get_double(o, "Is a test"), -2.345e24) < 1e-10);

    printf("Success!\n");
}
#endif
