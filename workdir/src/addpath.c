#include <windows.h>


CHAR to_upper(CHAR c) {
	if (c >= 'a' && c <= 'z') {
		return c + 'A' - 'a';
	}
	return c;
}

BOOL contains(LPCSTR path, LPCSTR dir) {
	CHAR dir_endc = '\0';
	if (dir[0] == '"') {
		dir_endc = '"';
		++dir;
	}
	
	while (1) {
		if (path[0] == '\0') {
			return FALSE;
		}
		CHAR endc = ';';
		if (path[0] == '"') {
			endc = '"';
			++path;
		}
		for(unsigned i = 0;TRUE;++path,++i) {
			if (dir[i] == '\0' || dir[i] == dir_endc) {
				if (path[0] == '\0' || path[0] == endc) {
					return TRUE;
				}
				if (path[0] == '\\' && (path[1] == '\0' || path[1] == endc)) {
					return TRUE;
				}
				return FALSE;
			}
			if (path[0] =='\0' || path[0] == endc) {
				if (dir[i] == '\0' || dir[i] == dir_endc) {
					return TRUE;
				}
				if (dir[i] == '\\' && (dir[i + 1] == '\0' || dir[i + 1] == dir_endc)) {
					return TRUE;
				}
				break;
			}
			if (to_upper(path[0]) != to_upper(dir[i])) {
				break;
			}	
		}
		while (path[0] != ';') {
			if (path[0] == '\0') {
				return FALSE;
			}
			++path;
		}
		++path;
	}
}

DWORD PATH_VALID = 0;
DWORD PATH_NEEDS_QUOTES = 1;
DWORD PATH_INVALID = 2;
DWORD validate(LPCSTR path) {
	int index = 0;
	BOOL needs_quotes = FALSE;
	BOOL has_quotes = FALSE;
	while (path[index] != '\0') {
		if (path[index] == ';') {
			needs_quotes = TRUE;
		}
		for (int j = 0; j < 6; ++j) {
			if (path[index] == "/?|<>*"[j]) {
				return PATH_INVALID;
			}
		}
		if (path[index] == ':' && ((has_quotes && index != 2) || (!has_quotes && index != 1))) {
			return PATH_INVALID;
		} else if (path[index] == '"') {
			if (index != 0 && path[index + 1] != '\0') {
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
	if (has_quotes && path[index - 1] != '"') {
		return PATH_INVALID;
	}
	if (index >= 2 && path[1] == ':') {
		if (!((path[0] >= 'a' && path[0] <= 'z') || (path[0] >= 'A' && path[0] <= 'Z'))) {
			return PATH_INVALID;
		}
		if (path[2] != '\\') {
			return PATH_INVALID;
		}
	}
	return needs_quotes && !has_quotes ? PATH_NEEDS_QUOTES : PATH_VALID;
}

int main() {
	char* argv[4096];
	int argc = parse_args(argv, 4096);

	HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
	HANDLE err = GetStdHandle(STD_ERROR_HANDLE);

	BOOL before = FALSE;
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '/' && (argv[i][1] == 'b') || argv[i][1] == 'B') {
			before = TRUE;
			for (int j = i + 1; j < argc; ++j) {
				argv[j - 1] = argv[j];
			}
			--argc;
			--i;
		}
	}

	if (argc < 2) {
		WriteFile(err, "Missing argument\n", 17, NULL, NULL);
		return 2;
	}
	
	HANDLE heap = GetProcessHeap();

	int err_code = 0;

	DWORD path_capacaty = 2000;
	LPSTR path_buffer = HeapAlloc(heap, 0, path_capacaty);
	DWORD req_size = GetEnvironmentVariable("Path", path_buffer, path_capacaty);
	if (req_size == 0) {
		err_code = GetLastError();
		if (err_code == ERROR_ENVVAR_NOT_FOUND) {
			path_buffer[0] = '\0';
			err_code = 0;
		} else {
			WriteFile(err, "Corrupt PATH\n", 13, NULL, NULL);
			goto end;
		}
	}
	if (req_size > path_capacaty) {
		HeapFree(heap, 0, path_buffer);
		path_capacaty = req_size + 20 * (argc - 1);
		path_buffer = HeapAlloc(heap, 0, path_capacaty);
		req_size = GetEnvironmentVariable("Path", path_buffer, path_capacaty);
		if (req_size == 0) {
			err_code = GetLastError();
			WriteFile(err, "Corrupt PATH\n", 13, NULL, NULL);
			goto end;
		}
	}
	BOOL added_any = FALSE;
	for (int arg = 1; arg < argc; ++arg) {
		DWORD status = validate(argv[arg]);
		if (status == PATH_INVALID) {
			err_code = 3;
			WriteFile(err, "Invalid argument\n", 17, NULL, NULL);
			goto end;
		}
		if (contains(path_buffer, argv[arg])) {
			continue;
		}
		added_any = TRUE;
		DWORD size = 0;
		while (argv[arg][size]) {
			++size;
		}
		DWORD extra_size = status == PATH_NEEDS_QUOTES ? 2 : 0;
		int offset;
		if (before) {
			++extra_size;
			if (req_size + size + extra_size + 1 > path_capacaty) {
				WriteFile(out, "REALLOC\n", 8, NULL, NULL);
				path_capacaty = req_size + size + extra_size + 1;
				LPSTR new_buffer = HeapAlloc(heap, 0, path_capacaty);
				for (int i = 0; i <= req_size; ++i) {
					new_buffer[i + size + extra_size] = path_buffer[i];
				}
				HeapFree(heap, 0, path_buffer);
				path_buffer = new_buffer;
			} else {
				for (int i = req_size; i >= 0; --i) {
					path_buffer[i + size + extra_size] = path_buffer[i];
				}
			}
			path_buffer[size + extra_size - 1] = ';';
			offset = 0;
		} else {
			if (path_buffer[req_size - 1] != ';') {
				++extra_size;
			}
			if (req_size + size + extra_size + 1 > path_capacaty) {
				WriteFile(out, "REALLOC\n", 8, NULL, NULL);
				path_capacaty = req_size + size + extra_size + 1;
				LPSTR new_buffer = HeapAlloc(heap, 0, path_capacaty);
				for (int i = 0; i < req_size; ++i) {
					new_buffer[i] = path_buffer[i];
				}
				HeapFree(heap, 0, path_buffer);
				path_buffer = new_buffer;
			}
			offset = req_size;
			if (path_buffer[req_size - 1 != ';']) {
				path_buffer[req_size] = ';';
				++offset;
			}
			path_buffer[req_size + size + extra_size] = '\0';
		}
		if (status == PATH_NEEDS_QUOTES) {
			path_buffer[offset] = '"';
			++offset;
		}
		for (int i = 0; i < size; ++i) {
			path_buffer[offset + i] = argv[arg][i];
		}
		if (status == PATH_NEEDS_QUOTES) {
			path_buffer[offset + size] = '"';
		}
		req_size = req_size + size + extra_size;
	}
	if (!added_any) {
		err_code = 1;
	}
	WriteFile(out, path_buffer, req_size, NULL, NULL);
	WriteFile(out, "\n", 1, NULL, NULL);
end:
	FlushFileBuffers(out);
	FlushFileBuffers(err);
	HeapFree(heap, 0, path_buffer);
	return err_code;
}