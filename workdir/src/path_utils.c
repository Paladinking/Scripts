#define UNICODE
#include "path_utils.h"

WCHAR to_upper(const WCHAR c) {
	if (c >= L'a' && c <= L'z') {
		return c + L'A' - L'a';
	}
	return c;
}

LPWSTR contains(LPWSTR path, LPCWSTR dir) {
	WCHAR dir_endc = L'\0';
	if (dir[0] == L'"') {
		dir_endc = L'"';
		++dir;
	}
	
	while (1) {
		if (path[0] == L'\0') {
			return NULL;
		}
		LPWSTR pos = path;
		WCHAR endc = L';';
		if (path[0] == L'"') {
			endc = L'"';
			++path;
		}
		for(unsigned i = 0; TRUE; ++path,++i) {
			if (dir[i] == L'\0' || dir[i] == dir_endc) {
				if (path[0] == L'\0' || path[0] == endc) {
					return pos;
				}
				if (path[0] == L'\\' && (path[1] == L'\0' || path[1] == endc)) {
					return pos;
				}
				break;
			}
			if (path[0] == L'\0' || path[0] == endc) {
				if (dir[i] == L'\0' || dir[i] == dir_endc) {
					return pos;
				}
				if (dir[i] == L'\\' && (dir[i + 1] == L'\0' || dir[i + 1] == dir_endc)) {
					return pos;
				}
				break;
			}
			if (to_upper(path[0]) != to_upper(dir[i])) {
				break;
			}	
		}
		while (path[0] != L';') {
			if (path[0] == L'\0') {
				return NULL;
			}
			++path;
		}
		++path;
	}
}

PathStatus validate(LPWSTR path) {
	int index = 0;
	BOOL needs_quotes = FALSE;
	BOOL has_quotes = FALSE;
	while (path[index] != L'\0') {
		if (path[index] == L';') {
			needs_quotes = TRUE;
		}
		if (path[index] < L' ') {
			return PATH_INVALID;
		}
		for (int j = 0; j < 5; ++j) {
			if (path[index] == L"?|<>*"[j]) {
				return PATH_INVALID;
			}
		}
		if (path[index] == L'/') {
			path[index] = L'\\';
		}
		
		if (path[index] == L'\\' && (path[index + 1] == L'\\' || path[index + 1] == L'/')) {
			return PATH_INVALID;
		}
		if (path[index] == L':' && ((has_quotes && index != 2) || (!has_quotes && index != 1))) {
			return PATH_INVALID;
		} else if (path[index] == L'"') {
			if (index != 0 && path[index + 1] != L'\0') {
				return PATH_INVALID;
			}
			if (index == 0) {
				has_quotes = TRUE;
			} else if (!has_quotes) {
				return PATH_INVALID;
			}
		}
		++index;
	}
	if (has_quotes && path[index - 1] != L'"') {
		return PATH_INVALID;
	}
	if (index >= 2 && path[1] == L':') {
		if (!((path[0] >= L'a' && path[0] <= L'z') || (path[0] >= L'A' && path[0] <= L'Z'))) {
			return PATH_INVALID;
		}
		if (path[2] != L'\\') {
			return PATH_INVALID;
		}
	}
	return needs_quotes && !has_quotes ? PATH_NEEDS_QUOTES : PATH_VALID;
}

OpStatus path_remove(LPWSTR arg, LPWSTR* path_buffer, DWORD* path_size, BOOL expand) {
    LPWSTR path;
    if (expand) {
    	OpStatus status = expand_path(arg, &path);
    	if (status != OP_SUCCES) {
    		return status;
    	}
    } else {
    	path = arg;
    }
    
    DWORD status = validate(path);
    if (status == PATH_INVALID) {
    	return OP_INVALID_PATH;
    }
    LPWSTR pos = contains(*path_buffer, path);
    if (pos == NULL) {
    	return OP_NO_CHANGE;
    }
    DWORD index = (DWORD)(pos - *path_buffer);
    WCHAR endc = pos[0] == L'"' ? L'"' : L';';
    DWORD len = 0;
    while (pos[len] != endc && pos[len] != L'\0') {
    	++len;
    }
    while (pos[len] != L';' && pos[len] != L'\0') {
    	++len;
    }
    if (pos[len] == L';') {
    	++len;
    }
	DWORD size = *path_size - index - len + 1;
	memmove((*path_buffer) + index, *path_buffer + index + len, size * sizeof(WCHAR));
    *path_size -= len;
	return OP_SUCCES;
}

OpStatus path_add(LPWSTR arg, LPWSTR* path_buffer, DWORD* path_size, DWORD* path_capacaty, BOOL before, BOOL expand) {
	LPWSTR path;
	if (expand) {
		OpStatus status = expand_path(arg, &path);
		if (status != OP_SUCCES) {
			return status;
		}
	} else {
		path = arg;
	}
	DWORD status = validate(path);
	if (status == PATH_INVALID) {
		return OP_INVALID_PATH;
	}
	if (contains(*path_buffer, path)) {
		return OP_NO_CHANGE;
	}

	HANDLE heap = GetProcessHeap();
	DWORD size = 0;
	while (path[size]) {
		++size;
	}
	DWORD extra_size = status == PATH_NEEDS_QUOTES ? 3 : 1;

	int offset;
	if (before) {
		if (*path_size + size + extra_size + 1 > *path_capacaty) {
			*path_capacaty = *path_size + size + extra_size + 1;
			LPWSTR new_buffer = HeapAlloc(heap, 0, *path_capacaty * sizeof(WCHAR));
			if (new_buffer == NULL) {
				return OP_OUT_OF_MEMORY;
			}
			memcpy(new_buffer + size + extra_size, *path_buffer, *path_size * sizeof(WCHAR));
			HeapFree(heap, 0, *path_buffer);
			*path_buffer = new_buffer;
		} else {
			memmove((*path_buffer) + size + extra_size, *path_buffer, *path_size * sizeof(WCHAR));
		}
		(*path_buffer)[size + extra_size - 1] = L';';
		offset = 0;
	} else {
		if ((*path_buffer)[*path_size - 1] != L';') {
			++extra_size;
		}
		if (*path_size + size + extra_size + 1 > *path_capacaty) {
			*path_capacaty = *path_size + size + extra_size + 1;
			LPWSTR new_buffer = HeapAlloc(heap, 0, *path_capacaty * sizeof(WCHAR));
			if (new_buffer == NULL) {
				return OP_OUT_OF_MEMORY;
			}
			memcpy(new_buffer, *path_buffer, *path_size * sizeof(WCHAR));
			HeapFree(heap, 0, *path_buffer);
			*path_buffer = new_buffer;
		}
		offset = *path_size;
		if ((*path_buffer)[*path_size - 1] != L';') {
			(*path_buffer)[*path_size] = L';';
			++offset;
		}
		(*path_buffer)[*path_size + size + extra_size - 1] = L';';
		(*path_buffer)[*path_size + size + extra_size] = L'\0';
	}
	if (status == PATH_NEEDS_QUOTES) {
		(*path_buffer)[offset] = L'"';
		++offset;
		(*path_buffer)[offset + size] = L'"';
	}
	memcpy((*path_buffer) + offset, path, size * sizeof(WCHAR));
	*path_size = *path_size + size + extra_size;
	return OP_SUCCES;
}

LPWSTR expanded = NULL;
DWORD expanded_capacity = 0;

OpStatus expand_path(LPWSTR path, LPWSTR* dest) {
	HANDLE heap = GetProcessHeap();
	if (path[0] == L'"') {
		++path;
		DWORD index = 1;
		while (path[index] != L'"' && path[index] != L'\0') {
			++index;
		}
		path[index] = L'\0';
	}
	DWORD expanded_size = GetFullPathName(path, expanded_capacity, expanded, NULL);
	if (expanded_size == 0) {
		return OP_INVALID_PATH;
	}
	if (expanded_size > expanded_capacity) {
		HeapFree(heap, 0, expanded);
		expanded = HeapAlloc(heap, 0, (expanded_size + 1) * sizeof(WCHAR));
		if (expanded == NULL) {
			return OP_OUT_OF_MEMORY;
		}
		expanded_capacity = expanded_size + 1;
		DWORD res = GetFullPathName(path, expanded_capacity, expanded, NULL);
		if (!res) {
			return OP_INVALID_PATH;
		}
	}
	*dest = expanded;
	return OP_SUCCES;
}