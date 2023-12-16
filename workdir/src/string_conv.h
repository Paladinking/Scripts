#pragma once
#include <windows.h>

typedef struct _WideBuffer {
	LPWSTR ptr;
	DWORD size;
	DWORD capacity;
} WideBuffer;

typedef struct _StringBuffer {
	LPSTR ptr;
	DWORD size;
	DWORD capacity;
} StringBuffer;

BOOL WideStringToNarrow(LPCWSTR string, DWORD size, StringBuffer* buffer);

BOOL NarrowStringToWide(LPCSTR string, DWORD size, WideBuffer* buffer);
