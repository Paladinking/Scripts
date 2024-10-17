#pragma once
#include <stdbool.h>
#include <wchar.h>

typedef struct String {
	char* buffer;
	unsigned capacity;
	unsigned length;
} String;

typedef String String_noinit;

bool String_append(String* s, const char c);

bool String_extend(String* s, const char* c_str);

bool String_insert(String* s, unsigned ix, const char c);

void String_pop(String* s, unsigned count);

void String_remove(String* s, unsigned ix, unsigned count);

void String_clear(String* s);

bool String_create(String* s);

void String_free(String* s);

bool String_copy(String* dest, String* source);

typedef struct WString {
	wchar_t* buffer;
	unsigned capacity;
	unsigned length;
} WString;

typedef WString WString_noinit;

// Append character `c` to string
bool WString_append(WString* s, const wchar_t c);

// Append null-terminated string `c_str` to string
bool WString_extend(WString*s, const wchar_t* c_str);

// Append `count` characters from `buf` to string
bool WString_append_count(WString* s, const wchar_t* buf, unsigned count);

// Insert `c` at offset `ix` into string
bool WString_insert(WString* s, unsigned ix, const wchar_t c);

// Insert `count` characters from `buf` at `ix`
bool WString_insert_count(WString* s, unsigned ix, const wchar_t* buf, unsigned count);

// Remove the `count` last elements from string
void WString_pop(WString* s, unsigned count);

// Remove `count` elements starting at `ix` from string
void WString_remove(WString* s, unsigned ix, unsigned count);

// Clear string. Leaves capacity unchanged
void WString_clear(WString* s);

// Create a new string
bool WString_create(WString_noinit* s);

// Create a new string with capacity >= `cap`
bool WString_create_capacity(WString_noinit* s, size_t cap);

// Free a string
void WString_free(WString* s);

// Copy `source` into `dest`. `dest` should be unitialized
bool WString_copy(WString_noinit* dest, WString* source);

// Increase capacity to allow `count` elements
bool WString_reserve(WString* s, size_t count);

bool WString_from_utf8_bytes(WString* dest, const char* s, size_t count);

bool WString_from_utf8_str(WString* dest, const char* s);
