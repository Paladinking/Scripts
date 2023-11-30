#include <windows.h>
#ifndef UNICODE
#error Unicode required
#endif

WCHAR to_upper(const WCHAR c);

typedef enum _PathStatus {
	PATH_VALID, PATH_NEEDS_QUOTES, PATH_INVALID
} PathStatus;

/* 
 Makes sure a path is a valid Windows path.
 Replaces L'/' with L'\'
*/
PathStatus validate(LPWSTR path);

typedef enum _OpStatus {
	OP_SUCCES, OP_NO_CHANGE, OP_INVALID_PATH, OP_MISSSING_ARG, OP_OUT_OF_MEMORY
} OpStatus;

OpStatus expand_path(LPWSTR path, LPWSTR* dest);

OpStatus path_add(LPWSTR arg, LPWSTR* path_buffer, DWORD* path_size, DWORD* path_capacaty, BOOL before, BOOL expand);

OpStatus path_remove(LPWSTR arg, LPWSTR* path_buffer, DWORD* path_size, BOOL expand);

LPWSTR contains(LPWSTR path, LPCWSTR dir);

extern LPWSTR expanded;
