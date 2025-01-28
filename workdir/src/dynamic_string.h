#pragma once
#include <stdbool.h>
#include <wchar.h>
#include "mem.h"

typedef unsigned string_size_t;

typedef struct String {
	char* buffer;
	string_size_t capacity;
	string_size_t length;
} String;

typedef String String_noinit;

// Append character `c` to string
bool String_append(String* s, const char c);

// Append null-terminated string `c_str` to string
bool String_extend(String*s, const char* c_str);

// Append `count` characters from `buf` to string
bool String_append_count(String* s, const char* buf, string_size_t count);

// Insert `c` at offset `ix` into string
bool String_insert(String* s, string_size_t ix, const char c);

// Insert `count` characters from `buf` at `ix`
bool String_insert_count(String* s, string_size_t ix, const char* buf, string_size_t count);

// Remove the `count` last elements from string
void String_pop(String* s, string_size_t count);

// Remove `count` elements starting at `ix` from string
void String_remove(String* s, string_size_t ix, string_size_t count);

// Clear string. Leaves capacity unchanged
void String_clear(String* s);

// Create a new string 
bool String_create(String_noinit* s);

// Create a new string with capacity >= `cap`
bool String_create_capacity(String_noinit* s, string_size_t cap);

void String_replaceall(String* s, char from, char to);

string_size_t String_count(const String* s, char c);

bool String_startswith(const String* s, const char* str);

bool String_equals(const String* s, const String* str);

// Free a string
void String_free(String* s);

// Copy `source` into `dest`. `dest` should be unitialized
bool String_copy(String_noinit* dest, String* source);

// Increase capacity to allow `count` elements
bool String_reserve(String* s, size_t count);

bool String_from_utf16_bytes(String* dest, const wchar_t* s, size_t count);

bool String_from_utf16_str(String* dest, const wchar_t* s);

typedef struct WString {
	wchar_t* buffer;
	string_size_t capacity;
	string_size_t length;
} WString;

typedef WString WString_noinit;

// Append character `c` to string
bool WString_append(WString* s, const wchar_t c);

// Append null-terminated string `c_str` to string
bool WString_extend(WString*s, const wchar_t* c_str);

// Append `count` characters from `buf` to string
bool WString_append_count(WString* s, const wchar_t* buf, string_size_t count);

// Insert `c` at offset `ix` into string
bool WString_insert(WString* s, string_size_t ix, const wchar_t c);

// Insert `count` characters from `buf` at `ix`
bool WString_insert_count(WString* s, string_size_t ix, const wchar_t* buf, string_size_t count);

// Remove the `count` last elements from string
void WString_pop(WString* s, string_size_t count);

// Remove `count` elements starting at `ix` from string
void WString_remove(WString* s, string_size_t ix, string_size_t count);

// Clear string. Leaves capacity unchanged
void WString_clear(WString* s);

// Create a new string 
bool WString_create(WString_noinit* s);

// Create a new string with capacity >= `cap`
bool WString_create_capacity(WString_noinit* s, string_size_t cap);

void WString_replaceall(WString* s, wchar_t from, wchar_t to);

string_size_t WString_count(const WString* s, wchar_t c);

bool WString_startswith(const WString* s, const wchar_t* str);

bool WString_equals(const WString* s, const WString* str);

// Free a string
void WString_free(WString* s);

// Copy `source` into `dest`. `dest` should be unitialized
bool WString_copy(WString_noinit* dest, WString* source);

// Increase capacity to allow `count` elements
bool WString_reserve(WString* s, size_t count);

bool WString_from_con_bytes(WString* dest, const char* s, size_t count, UINT code_point);

bool WString_from_con_str(WString* dest, const char* s, UINT code_point);

bool WString_from_utf8_bytes(WString* dest, const char* s, size_t count);

bool WString_from_utf8_str(WString* dest, const char* s);
