#pragma once
#include <windows.h>
#include "dynamic_string.h"
#ifndef UNICODE
#error Unicode required
#endif

typedef WString PathBuffer;

typedef struct _EnvVar {
	LPCWSTR name;
	WString val;
} EnvVar;

typedef struct _EnvBuffer {
	EnvVar* vars;
	DWORD size;
	DWORD capacity;
} EnvBuffer;

/*
 Gets an environment variable <name> from the process environment block.
 Returns TRUE on success.
 If the function fails, res->buffer is not changed.
 If the function fails, res->capacity and res->length are undefined.
 The value res->capacity + hint is used as an initial capacity if res->buffer is NULL.
 The value in hint should indicate how much the buffer is expected to grow.
*/
BOOL get_envvar(LPCWSTR name, DWORD hint, WString* res);

/*
 Gets an environment variable <name> from <env>.
 If the variable is not present, it first added to env.
 If the function fails, NULL is returned.
*/
WString* read_envvar(LPCWSTR name, EnvBuffer* env);

void free_env(EnvBuffer* env);

typedef enum _PathStatus {
	PATH_VALID, PATH_NEEDS_QUOTES, PATH_INVALID
} PathStatus;

/* 
 Makes sure a path is (probably) a valid Windows path.
 Replaces L'/' with L'\'. Removes trailing L'\'.
 Does not check weird cases like CON.
*/
PathStatus validate(LPWSTR path);

typedef enum _OpStatus {
	OP_SUCCESS, OP_NO_CHANGE, OP_INVALID_PATH, OP_MISSSING_ARG, OP_OUT_OF_MEMORY
} OpStatus;

OpStatus expand_path(LPWSTR path, LPWSTR* dest, DWORD *dest_cap);

OpStatus path_add(LPWSTR arg, PathBuffer* path, BOOL before, BOOL expand);

OpStatus path_remove(LPWSTR arg, PathBuffer* path, BOOL expand);

OpStatus path_prune(PathBuffer* path);

LPWSTR contains(LPWSTR path, LPCWSTR dir);

extern LPWSTR expanded;

int SetProcessEnvironmentVariable(LPCWSTR var, LPCWSTR val, DWORD processId);
