#pragma once
#include <windows.h>
#include "dynamic_string.h"
#include "ochar.h"

#ifdef NARROW_OCHAR
typedef String PathBuffer;
#else
typedef WString PathBuffer;
#endif

typedef struct _EnvVar {
    const ochar_t* name;
    OString val;
} EnvVar;

typedef struct _EnvBuffer {
    EnvVar* vars;
    uint32_t size;
    uint32_t capacity;
} EnvBuffer;

/*
 Gets an environment variable <name> from the process environment block.
 Returns TRUE on success.
 If the function fails, res->buffer is not changed.
 If the function fails, res->capacity and res->length are undefined.
 The value res->capacity + hint is used as an initial capacity if res->buffer is NULL.
 The value in hint should indicate how much the buffer is expected to grow.
*/
bool get_envvar(const ochar_t* name, uint32_t hint, OString* res);

/*
 * Gets environment variable <name>.
 * Returns TRUE on success.
 */
bool create_envvar(const ochar_t* name, OString_noinit* res);

/*
 Gets an environment variable <name> from <env>.
 If the variable is not present, it first added to env.
 If the function fails, NULL is returned.
*/
OString* read_envvar(const ochar_t* name, EnvBuffer* env);

void free_env(EnvBuffer* env);

typedef enum _PathStatus {
    PATH_VALID, PATH_NEEDS_QUOTES, PATH_INVALID
} PathStatus;

/* 
 Makes sure a path is (probably) a valid Windows path.
 Replaces L'/' with L'\'. Removes trailing L'\'.
 Does not check weird cases like CON.
*/
PathStatus validate(ochar_t* path);

typedef enum _OpStatus {
	OP_SUCCESS, OP_NO_CHANGE, OP_INVALID_PATH, OP_MISSSING_ARG, OP_OUT_OF_MEMORY
} OpStatus;


typedef struct PathIterator {
    PathBuffer* path;
    wchar_t* buf;
    uint32_t capacity;
    uint32_t ix;
} PathIterator;

bool PathIterator_begin(PathIterator* it, PathBuffer* path);

ochar_t* PathIterator_next(PathIterator* it, uint32_t* size);

void PathIterator_abort(PathIterator* it);

#ifndef NARROW_OCHAR
OpStatus expand_path(LPWSTR path, LPWSTR* dest, DWORD *dest_cap);

OpStatus path_add(LPWSTR arg, PathBuffer* path, BOOL before, BOOL expand);

OpStatus path_remove(LPWSTR arg, PathBuffer* path, BOOL expand);

OpStatus path_prune(PathBuffer* path);

LPWSTR contains(LPWSTR path, LPCWSTR dir);

extern LPWSTR expanded;

int SetProcessEnvironmentVariable(LPCWSTR var, LPCWSTR val, DWORD processId);
#endif
