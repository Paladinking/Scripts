#pragma once
#include <stdbool.h>
#include <wchar.h>

typedef struct _DynamicString {
	char* buffer;
	unsigned capacity;
	unsigned length;
} String;


bool String_append(String* s, const char c);

bool String_extend(String* s, const char* c_str);

bool String_insert(String* s, unsigned ix, const char c);

void String_pop(String* s, unsigned count);

void String_remove(String* s, unsigned ix, unsigned count);

void String_clear(String* s);

bool String_create(String* s);

void String_free(String* s);

bool String_copy(String* dest, String* source);

typedef struct _DynamicWString {
	wchar_t* buffer;
	unsigned capacity;
	unsigned length;
} WString;

bool WString_append(WString* s, const wchar_t c);

bool WString_extend(WString*s, const wchar_t* c_str);

bool WString_append_count(WString* s, const wchar_t* buf, unsigned count);

bool WString_insert(WString* s, unsigned ix, const wchar_t c);

void WString_pop(WString* s, unsigned count);

void WString_remove(WString* s, unsigned ix, unsigned count);

void WString_clear(WString* s);

bool WString_create(WString* s);

void WString_free(WString* s);

bool WString_copy(WString* dest, WString* source);

bool WString_reserve(WString* s, size_t count);


