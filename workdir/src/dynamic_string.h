#pragma once
#include <stdbool.h>

typedef struct _DynamicString {
	char* buffer;
	unsigned capacity;
	unsigned length;
} DynamicString;


bool DynamicStringAppend(DynamicString* s, const char c);

bool DynamicStringExtend(DynamicString*s, const char* c_str);

void DynamicStringClear(DynamicString* s);

bool DynamicStringCreate(DynamicString* s);

void DynamicStringFree(DynamicString* s);
