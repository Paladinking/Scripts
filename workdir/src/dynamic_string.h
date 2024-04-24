#pragma once
#include <stdbool.h>
#include <wchar.h>

typedef struct _DynamicString {
	char* buffer;
	unsigned capacity;
	unsigned length;
} DynamicString;


bool DynamicStringAppend(DynamicString* s, const char c);

bool DynamicStringExtend(DynamicString* s, const char* c_str);

bool DynamicStringInsert(DynamicString* s, unsigned ix, const char c);

void DynamicStringPop(DynamicString* s, unsigned count);

void DynamicStringRemove(DynamicString* s, unsigned ix, unsigned count);

void DynamicStringClear(DynamicString* s);

bool DynamicStringCreate(DynamicString* s);

void DynamicStringFree(DynamicString* s);

bool DynamicStringCopy(DynamicString* dest, DynamicString* source);

typedef struct _DynamicWString {
	wchar_t* buffer;
	unsigned capacity;
	unsigned length;
} DynamicWString;

bool DynamicWStringAppend(DynamicWString* s, const wchar_t c);

bool DynamicWStringExtend(DynamicWString*s, const wchar_t* c_str);

bool DynamicWStringAppendCount(DynamicWString* s, const wchar_t* buf, unsigned count);

bool DynamicWStringInsert(DynamicWString* s, unsigned ix, const wchar_t c);

void DynamicWStringPop(DynamicWString* s, unsigned count);

void DynamicWStringRemove(DynamicWString* s, unsigned ix, unsigned count);

void DynamicWStringClear(DynamicWString* s);

bool DynamicWStringCreate(DynamicWString* s);

void DynamicWStringFree(DynamicWString* s);

bool DynamicWStringCopy(DynamicWString* dest, DynamicWString* source);

bool DynamicWStringReserve(DynamicWString* s, size_t count);


