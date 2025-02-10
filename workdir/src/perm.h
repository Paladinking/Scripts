#ifndef PERM_H
#define PERM_H
#include <stdint.h>
#include <stdbool.h>
#include "dynamic_string.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define OWNER_READ 1
#define OWNER_WRITE 2
#define OWNER_EXECUTE 4
#define OWNER_ALL (OWNER_READ | OWNER_WRITE | OWNER_EXECUTE)
#define OWNER_MASK(r, w, e) (((r) ? OWNER_READ : 0) | ((w) ? OWNER_WRITE : 0) | ((e) ? OWNER_EXECUTE : 0))
#define GROUP_READ 8
#define GROUP_WRITE 16
#define GROUP_EXECUTE 32
#define GROUP_ALL (GROUP_READ | GROUP_WRITE | GROUP_EXECUTE)
#define GROUP_MASK(r, w, e) (((r) ? GROUP_READ : 0) | ((w) ? GROUP_WRITE : 0) | ((e) ? GROUP_EXECUTE : 0))
#define USER_READ 64
#define USER_WRITE 128
#define USER_EXECUTE 256
#define USER_ALL (USER_READ | USER_WRITE | USER_EXECUTE)
#define USER_MASK(r, w, e) (((r) ? USER_READ : 0) | ((w) ? USER_WRITE : 0) | ((e) ? USER_EXECUTE : 0))


bool get_perms_h(HANDLE file, uint32_t* flags, WString* owner_name, WString* group_name);

bool get_perms_size_date(const wchar_t *path, bool is_dir, uint32_t *flags,
                         WString *owner_name, WString *group_name,
                         uint64_t *size, FILETIME *time, uint64_t* disk_size);

bool get_perms(const wchar_t *path, bool is_dir, uint32_t *flags,
               WString *owner, WString *group);

#endif
