#include "dynamic_string.h"
#include <windows.h>


bool DynamicStringAppend(DynamicString* s, const char c) {
	if (s->capacity == s->length + 1) {
		HANDLE heap = GetProcessHeap();
		unsigned capacity = s->capacity * 2;
		char* buffer = HeapReAlloc(heap, 0, s->buffer, capacity);
		if (buffer == NULL) {
			return false;
		}
		s->buffer = buffer;
		s->capacity = capacity;
	}
	s->buffer[s->length] = c;
	s->length += 1;
	s->buffer[s->length] = '\0';
	return true;
}

bool DynamicStringExtend(DynamicString*s, const char* c_str) {
	unsigned index = 0;
	while (c_str[index] != '\0') {
		if (!DynamicStringAppend(s, c_str[index])) {
			return false;
		}
	}
	return true;
}

void DynamicStringClear(DynamicString* s) {
	s->length = 0;
	s->buffer[0] = '\0';
}

bool DynamicStringCreate(DynamicString* s) {
	HANDLE heap = GetProcessHeap();
	s->length = 0;
	s->buffer = HeapAlloc(heap, 0, 4);
	if (s->buffer == NULL) {
		s->capacity = 0;
		return false;
	}
	s->capacity = 4;
	s->buffer[0] = '\0';
	return true;
}

void DynamicStringFree(DynamicString* s) {
	HANDLE heap = GetProcessHeap();
	HeapFree(heap, 0, s->buffer);
	s->capacity = 0;
	s->length = 0;
	s->buffer = NULL;
}