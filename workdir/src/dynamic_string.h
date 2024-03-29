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


typedef struct _DynamicWString {
	wchar_t* buffer;
	unsigned capacity;
	unsigned length;
} DynamicWString;

bool DynamicWStringAppend(DynamicWString* s, const wchar_t c);

bool DynamicWStringExtend(DynamicWString*s, const wchar_t* c_str);

bool DynamicWStringInsert(DynamicWString* s, unsigned ix, const char c);

void DynamicWStringPop(DynamicWString* s, unsigned count);

void DynamicWStringRemove(DynamicWString* s, unsigned ix, unsigned count);

void DynamicWStringClear(DynamicWString* s);

bool DynamicWStringCreate(DynamicWString* s);

void DynamicWStringFree(DynamicWString* s);

