#define UNICODE
#ifdef __GNUC__
#define main entry_main
#endif
#include <windows.h>

WCHAR to_upper(const WCHAR c) {
	if (c >= L'a' && c <= L'z') {
		return c + L'A' - L'a';
	}
	return c;
}

BOOL equals(LPCWSTR a, LPCWSTR b) {
	int index = 0;
	while (TRUE) {
		if (a[index] != b[index]) {
			return FALSE;
		}
		if (a[index] == L'\0' || b[index] == L'\0') {
			return TRUE;
		}
		++index;
	}
}

DWORD find_flag(LPWSTR* argv, int* argc, LPCWSTR flag, LPCWSTR long_flag) {
	DWORD count = 0;
	for (int i = 1; i < *argc; ++i) {
		if (equals(argv[i], flag) || equals(argv[i], long_flag)) {
			count += 1;
			for (int j = i + 1; j < *argc; ++j) {
				argv[j - 1] = argv[j];
			}
			--(*argc);
			--i;
		}
	}
	return count;
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

typedef enum _PathStatus {
	PATH_VALID, PATH_NEEDS_QUOTES, PATH_INVALID
} PathStatus;

PathStatus validate(LPWSTR path) {
	int index = 0;
	BOOL needs_quotes = FALSE;
	BOOL has_quotes = FALSE;
	while (path[index] != L'\0') {
		if (path[index] == L';') {
			needs_quotes = TRUE;
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

int WriteFileWide(HANDLE out, LPCWSTR ptr, int size) {
	HANDLE heap = GetProcessHeap();
	UINT code_point = GetConsoleOutputCP();
	DWORD out_req_size = WideCharToMultiByte(
		code_point, 0, ptr, size, NULL, 0, NULL, NULL
	);

	LPSTR print_buffer = HeapAlloc(heap, 0, out_req_size);

	out_req_size = WideCharToMultiByte(
		code_point, 0, ptr, size, print_buffer, out_req_size, NULL, NULL
	);
	if (out_req_size == 0) {
		HeapFree(heap, 0, print_buffer);
		return 0;
	}
	int status = WriteFile(out, print_buffer, out_req_size, NULL, NULL);
	HeapFree(heap, 0, print_buffer);
	return status;
}


typedef enum _OpStatus {
	OP_SUCCES, OP_NO_CHANGE, OP_INVALID_PATH, OP_MISSSING_ARG, OP_OUT_OF_MEMORY
} OpStatus;

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

OpStatus path_add(
	int argc, 
	LPWSTR* argv,
	LPWSTR* path_buffer,
	DWORD* path_size, 
	DWORD path_capacaty,
	BOOL before,
	BOOL expand
) {

	if (argc < 2) {
		return OP_MISSSING_ARG;
	}
	HANDLE heap = GetProcessHeap();
	BOOL added_any = FALSE;

	for (int arg = 1; arg < argc; ++arg) {
		LPWSTR path;
		if (expand) {
			OpStatus status = expand_path(argv[arg], &path);
			if (status != OP_SUCCES) {
				return status;
			}
		} else {
			path = argv[arg];
		}
		
		DWORD status = validate(path);
		if (status == PATH_INVALID) {
			return OP_INVALID_PATH;
		}
		if (contains(*path_buffer, path)) {
			continue;
		}
		added_any = TRUE;
		DWORD size = 0;
		while (path[size]) {
			++size;
		}
		DWORD extra_size = status == PATH_NEEDS_QUOTES ? 2 : 0;

		int offset;
		if (before) {
			++extra_size;
			if (*path_size + size + extra_size + 1 > path_capacaty) {
				path_capacaty = *path_size + size + extra_size + 1;
				LPWSTR new_buffer = HeapAlloc(heap, 0, path_capacaty * sizeof(WCHAR));
				if (new_buffer == NULL) {
					return OP_OUT_OF_MEMORY;
				}
				for (unsigned i = 0; i <= *path_size; ++i) {
					new_buffer[i + size + extra_size] = (*path_buffer)[i];
				}
				HeapFree(heap, 0, *path_buffer);
				*path_buffer = new_buffer;
			} else {
				for (int i = *path_size; i >= 0; --i) {
					(*path_buffer)[i + size + extra_size] = (*path_buffer)[i];
				}
			}
			(*path_buffer)[size + extra_size - 1] = L';';
			offset = 0;
		} else {
			if ((*path_buffer)[*path_size - 1] != L';') {
				++extra_size;
			}
			if (*path_size + size + extra_size + 1 > path_capacaty) {
				path_capacaty = *path_size + size + extra_size + 1;
				LPWSTR new_buffer = HeapAlloc(heap, 0, path_capacaty * sizeof(WCHAR));
				if (new_buffer == NULL) {
					return OP_OUT_OF_MEMORY;
				}
				for (unsigned i = 0; i < *path_size; ++i) {
					new_buffer[i] = (*path_buffer)[i];
				}
				HeapFree(heap, 0, *path_buffer);
				*path_buffer = new_buffer;
			}
			offset = *path_size;
			if ((*path_buffer)[*path_size - 1] != L';') {
				(*path_buffer)[*path_size] = L';';
				++offset;
			}
			(*path_buffer)[*path_size + size + extra_size] = L'\0';
		}
		if (status == PATH_NEEDS_QUOTES) {
			(*path_buffer)[offset] = L'"';
			++offset;
		}
		for (unsigned i = 0; i < size; ++i) {
			(*path_buffer)[offset + i] = path[i];
		}
		if (status == PATH_NEEDS_QUOTES) {
			(*path_buffer)[offset + size] = L'"';
		}
		*path_size = *path_size + size + extra_size;
	}
	if (!added_any) {
		return OP_NO_CHANGE;
	}
	return OP_SUCCES;
}


OpStatus path_remove(
	int argc, 
	LPWSTR* argv,
	LPWSTR* path_buffer,
	DWORD* path_size, 
	DWORD path_capacaty,
	BOOL before,
	BOOL expand
) {
	if (argc < 2) {
		return OP_MISSSING_ARG;
	}
	if (before) {
		return OP_INVALID_PATH;
	}
	BOOL removed_any = FALSE;
	for (int arg = 1; arg < argc; ++arg) {
		LPWSTR path;
		if (expand) {
			OpStatus status = expand_path(argv[arg], &path);
			if (status != OP_SUCCES) {
				return status;
			}
		} else {
			path = argv[arg];
		}

		DWORD status = validate(path);
		if (status == PATH_INVALID) {
			return OP_INVALID_PATH;
		}
		LPWSTR pos = contains(*path_buffer, path);
		if (pos == NULL) {
			continue;
		}
		removed_any = TRUE;
		DWORD index = (DWORD)(pos - *path_buffer);
		WCHAR endc = pos[0] == L'"' ? L'"' : L';';
		DWORD len = 0;
		while (pos[len] != endc && pos[len] != L'\0') {
			++len;
		}
		while (pos[len] != L';' && pos[len] != L'\0') {
			++len;
		}
		if (pos[len] == ';') {
			++len;
		}
		for (unsigned i = index; i + len <= *path_size; ++i) {
			(*path_buffer)[i] = (*path_buffer)[i + len];
		}
		*path_size -= len;
	}
	if (!removed_any) {
		return OP_NO_CHANGE;
	}
	return OP_SUCCES;
}

typedef enum _OperationType {
	OPERATION_ADD, OPERATION_REMOVE, OPERATION_MOVE
} OperationType;

LPWSTR path_buffer = NULL;
DWORD path_size = 0;
DWORD path_capacaty = 0;

int pathc(int argc, LPWSTR* argv, HANDLE out, HANDLE err) {
	if (argc < 2) {
		WriteFile(err, "Missing operation type\n", 23, NULL, NULL);
		return 2;
	}

	OperationType operation;
	if (equals(argv[1], L"a") || equals(argv[1], L"add")) {
		operation = OPERATION_ADD;
	} else if (equals(argv[1], L"r") || equals(argv[1], L"remove")) {
		operation = OPERATION_REMOVE;
	} else if (equals(argv[1], L"m") || equals(argv[1], L"move")) {
		operation = OPERATION_MOVE;
	}else {
		WriteFile(err, "Invalid operation type\n", 23, NULL, NULL);
		return 2;
	}
	BOOL expand = !find_flag(argv, &argc, L"-n", L"--no-expand");
	BOOL before = find_flag(argv, &argc, L"-b", L"--before");
	
	HANDLE heap = GetProcessHeap();

	path_capacaty = 2000;
	path_buffer = HeapAlloc(heap, 0, path_capacaty * sizeof(WCHAR));
	path_size = GetEnvironmentVariable(L"Path", path_buffer, path_capacaty);

	if (path_size == 0) {
		int err_code = GetLastError();
		if (err_code == ERROR_ENVVAR_NOT_FOUND) {
			path_buffer[0] = L'\0';
		} else {
			WriteFile(err, "Could not get Path\n", 19, NULL, NULL);
			return err_code;
		}
	}
	if (path_size > path_capacaty) {
		HeapFree(heap, 0, path_buffer);
		path_capacaty = path_size + 20 * (argc - 1);
		path_buffer = HeapAlloc(heap, 0, path_capacaty * sizeof(WCHAR));
		path_size = GetEnvironmentVariable(L"Path", path_buffer, path_capacaty);
		if (path_size == 0) {
			int err_code = GetLastError();
			WriteFile(err, "Could not get Path\n", 19, NULL, NULL);
			return err_code;
		}
	}
	OpStatus status;

	if (operation == OPERATION_ADD) {
		status = path_add(
			argc - 1, argv + 1, &path_buffer, &path_size, path_capacaty, before, expand
		);
	} else if (operation == OPERATION_REMOVE){
		status = path_remove(
			argc - 1, argv + 1, &path_buffer, &path_size, path_capacaty, before, expand
		);
	} else {
		status = path_remove(
			argc - 1, argv + 1, &path_buffer, &path_size, path_capacaty, FALSE, expand
		);
		if (status <= OP_NO_CHANGE) {
			status = path_add(
				argc - 1, argv + 1, &path_buffer, &path_size, path_capacaty, before, expand
			);
		}
	}
	if (status == OP_MISSSING_ARG) {
		WriteFile(err, "Missing argument\n", 17, NULL, NULL);
		return 2;
	}
	if (status == OP_INVALID_PATH) {
		WriteFile(err, "Invalid argument\n", 17, NULL, NULL);
		return 2;
	}
	if (status == OP_OUT_OF_MEMORY) {
		WriteFile(err, "Out of memory\n", 14, NULL, NULL);
		return 3;
	}
	WriteFileWide(out, path_buffer, path_size);
	WriteFile(out, "\n", 1, NULL, NULL);
	if (status == OP_NO_CHANGE) {
		return 1;
	}
	return 0;
}

int main() {
	HANDLE heap = GetProcessHeap();
	LPWSTR args = GetCommandLine();
	int argc;
	LPWSTR* argv = CommandLineToArgvW(args, &argc);
	HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
	HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
	int status = pathc(argc, argv, out, err);
	FlushFileBuffers(out);
	FlushFileBuffers(err);
	HeapFree(heap, 0, expanded);
	HeapFree(heap, 0, path_buffer);
	LocalFree(argv);
	return status;
}