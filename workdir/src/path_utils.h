#pragma once
#include <windows.h>
#include "string_conv.h"
#ifndef UNICODE
#error Unicode required
#endif

typedef WideBuffer PathBuffer;

BOOL get_path_envvar(DWORD capacity, DWORD hint, PathBuffer* res);

typedef enum _PathStatus {
	PATH_VALID, PATH_NEEDS_QUOTES, PATH_INVALID
} PathStatus;

/* 
 Makes sure a path is (probably) a valid Windows path.
 Replaces L'/' with L'\'.
 Does not check weird cases like CON.
*/
PathStatus validate(LPWSTR path);

typedef enum _OpStatus {
	OP_SUCCESS, OP_NO_CHANGE, OP_INVALID_PATH, OP_MISSSING_ARG, OP_OUT_OF_MEMORY
} OpStatus;

OpStatus expand_path(LPWSTR path, LPWSTR* dest);

OpStatus path_add(LPWSTR arg, PathBuffer* path, BOOL before, BOOL expand);

OpStatus path_remove(LPWSTR arg, PathBuffer* path, BOOL expand);

LPWSTR contains(LPWSTR path, LPCWSTR dir);

extern LPWSTR expanded;

int SetProcessEnvironmentVariable(LPCWSTR var, LPCWSTR val, DWORD processId);
