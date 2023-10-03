#define UNICODE
#ifdef __GNUC__
#define main entry_main
#endif
#include <windows.h>
#include "dynamic_string.h"

LPSTR buffer = NULL;

typedef enum _ParseStatus {
	PARSE_SUCCES, PARSE_GET_ERROR, PARSE_OUT_OF_MEMORY,
	PARSE_INVALID_FILE
} ParseStatus;

typedef struct map_entry {
	LPSTR key;
	LPSTR value;
	DWORD value_len;
} MapEntry;


int compare(MapEntry a, MapEntry b) {
	int index = 0;
	while (TRUE) {
		int diff = b.key[index] - a.key[index];
		if (diff != 0) {
			return diff;
		}
		if (a.key[index] == '\0' || b.key[index] == '\0') {
			return 0;
		}
		++index;
	}
}

void sort(MapEntry* map, const unsigned start, const unsigned len) {
	HANDLE heap = GetProcessHeap();
	if (len == 1) {
		return;
	}
	if (len == 2) {
		if (compare(map[start], map[start + len - 1]) <= -1) {
			MapEntry temp = map[start];
			map[start] = map[start + len - 1];
			map[start + len  -1] = temp;
		}
		return;
	}
	MapEntry* parts = HeapAlloc(heap, 0, len * sizeof(MapEntry));
	memcpy(parts, map + start, len * sizeof(MapEntry));

	size_t start_1 = 0;
	size_t start_2 = len / 2;
	const size_t len_1 = len / 2;
	const size_t len_2 = (len + 1) / 2;
	sort(parts, start_1, len_1); 
	sort(parts, start_2, len_2);
	for (size_t i = start; i < start + len; ++i) {
		if (start_1 == len_1) {
			map[i] = parts[start_2];
			++start_2;
		} else if (start_2 - len_1 == len_2) {
			map[i] = parts[start_1];
			++start_1;
		} else if (compare(parts[start_1], parts[start_2]) >= 1) {
			map[i] = parts[start_1];
			++start_1;
		} else {
			map[i] = parts[start_2];
			++start_2;	
		}
	}
	HeapFree(heap, 0, parts);
}

MapEntry* binsearch(MapEntry* map, const size_t len, LPSTR key) {
	MapEntry entry;
	entry.key = key;
	size_t lower = 0;
	size_t upper = len -1;
	while (lower != upper) {
		const size_t middle = (lower + upper) / 2;
		const int cmp = compare(entry, map[middle]);
		if (cmp == 0) {
			return map + middle;
		}
		if (cmp > 0) {
			upper = middle;
		} else {
			lower = middle + 1;
		}
	}
	if (compare(entry, map[lower]) != 0) {
		return NULL;
	}
	return map + lower;
}

ParseStatus parse_translation(HANDLE file, MapEntry** map, unsigned* len) {
	HANDLE heap = GetProcessHeap();
	LARGE_INTEGER size;
	if (!GetFileSizeEx(file, &size)) {
		CloseHandle(file);
		return PARSE_GET_ERROR;
	}
	buffer = HeapAlloc(heap, 0, size.QuadPart + 1);
	if (buffer == NULL) {
		CloseHandle(file);
		return PARSE_OUT_OF_MEMORY;
	}
	DWORD read = 0;
	BOOL succes = ReadFile(file, buffer, size.QuadPart, &read, NULL);
	if (!succes || read < size.QuadPart) {
		CloseHandle(file);
		return PARSE_GET_ERROR;
	}
	CloseHandle(file);
	DWORD length = 1;
	for (LONGLONG i = 0; i < size.QuadPart; ++i) {
		if (buffer[i] == '\n') {
			++length;
		}
	}

	*map = HeapAlloc(heap, 0, length * sizeof(MapEntry));
	if (*map == NULL) {
		return PARSE_OUT_OF_MEMORY;
	}
	*len = 0;
	BOOL value = FALSE;
	LONGLONG line_start = 0;
	for (LONGLONG i = 0; i < size.QuadPart; ++i) {
		if (buffer[i] == '\n' || buffer[i] == '\r') {
			if (!value) {
				HeapFree(heap, 0, *map);
				return PARSE_INVALID_FILE;
			}
			buffer[i] = '\0';
			value = FALSE;
			(*map)[*len].value = buffer + line_start;
			(*map)[*len].value_len = i - line_start;
			*len += 1;
			line_start = i + 1;
			while (buffer[line_start] == '\n' || buffer[line_start] == '\r') {
				++line_start;
				++i;
			}
		}
		if (buffer[i] == '=') {
			if (value) {
				HeapFree(heap, 0, *map);
				return PARSE_INVALID_FILE;
			}
			buffer[i] = '\0';
			value = TRUE;
			(*map)[*len].key = buffer + line_start;
			line_start = i + 1;
		}
	}
	if (value) {
		(*map)[*len].value = buffer + line_start;
		buffer[size.QuadPart] = '\0';
		(*map)[*len].value_len = size.QuadPart - line_start;
		*len += 1;
	}
	sort(*map, 0, *len);
	return PARSE_SUCCES;
}

typedef enum _ReplaceStatus {
	REPLACE_SUCCES, REPLACE_OUT_OF_MEMORY, REPLACE_GET_ERROR,
	REPLACE_INVALID
} ReplaceStatus;

ReplaceStatus replace(MapEntry* map, unsigned map_len, HANDLE in, HANDLE out) {
	DynamicString key_buffer;
	if (!DynamicStringCreate(&key_buffer)) {
		return REPLACE_OUT_OF_MEMORY;
	}

	CHAR read_buffer[4096];
	DWORD open_template = 0;
	DWORD closed_template = 0;

	while (TRUE) {
		DWORD read = 0;
		BOOL succes = ReadFile(in, read_buffer, 4096, &read, NULL);
		if (!succes) {
			return REPLACE_GET_ERROR;
		}
		if (read == 0) {
			break;
		}
		for (unsigned i = 0; i < read; ++i) {
			if (read_buffer[i] == '{') {
				open_template += 1;
				if (open_template == 3) {
					DynamicStringFree(&key_buffer);
					return REPLACE_INVALID;
				}
			} else if (read_buffer[i] == '}' && open_template == 2) {
				closed_template += 1;
				if (closed_template < 2) {
					continue;
				}
				if (key_buffer.length == 0) {
					WriteFile(out, "{{}}", 4, NULL, NULL);
				} else {
					MapEntry* entry = binsearch(map, map_len, key_buffer.buffer);
					if (entry == NULL) {
						WriteFile(out, "{{", 2, NULL, NULL);
						WriteFile(out, key_buffer.buffer, key_buffer.length, NULL, NULL);
						WriteFile(out, "}}", 2, NULL, NULL);
					} else {
						WriteFile(out, entry->value, entry->value_len, NULL, NULL);
					}
				}
				DynamicStringClear(&key_buffer);
				open_template = 0;
				closed_template = 0;
			}
			else if (open_template == 2) {
				if (closed_template == 1) {
					WriteFile(out, "{{", 2, NULL, NULL);
					WriteFile(out, key_buffer.buffer, key_buffer.length, NULL, NULL);
					WriteFile(out, "}", 1, NULL, NULL);
					WriteFile(out, &read_buffer[i], 1, NULL, NULL);
					open_template = 0;
					closed_template = 0;
					DynamicStringClear(&key_buffer);
				} else if (!DynamicStringAppend(&key_buffer, read_buffer[i])) {
					DynamicStringFree(&key_buffer);
					return REPLACE_OUT_OF_MEMORY;
				}
			} else {
				if (open_template == 1) {
					WriteFile(out, "{", 1, NULL, NULL);
					open_template = 0;
				}
				WriteFile(out, &read_buffer[i], 1, NULL, NULL);
			}
		}
	}
	DynamicStringFree(&key_buffer);
	if (open_template == 2) {
		return REPLACE_INVALID;
	}
	return REPLACE_SUCCES;
}


int main() {
	HANDLE heap = GetProcessHeap();
	LPWSTR args = GetCommandLine();
	int status = 0;
	int argc;
	HANDLE template_file = INVALID_HANDLE_VALUE;
	LPWSTR* argv = CommandLineToArgvW(args, &argc);
	HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
	HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
	if (argc < 4) {
		WriteFile(err, "Missing argument\n", 17, NULL, NULL);
		status = 1;
		goto end;
	}
	template_file = CreateFile(argv[1], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (template_file == INVALID_HANDLE_VALUE) {
		WriteFile(err, "Could not open template file\n", 29, NULL, NULL);
		status = GetLastError();
		goto end;
	}
	HANDLE translation_file = CreateFile(argv[2], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (translation_file == INVALID_HANDLE_VALUE) {
		WriteFile(err, "Could not open translation file\n", 32, NULL, NULL);
		status = GetLastError();
		goto end;
	}
	MapEntry* keys_values = NULL;
	unsigned len = 0;
	ParseStatus s = parse_translation(translation_file, &keys_values, &len);
	if (s == PARSE_SUCCES) {
		HANDLE out_file = CreateFile(argv[3], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
		if (out_file == INVALID_HANDLE_VALUE) {
			WriteFile(err, "Could not open output file\n", 27, NULL, NULL);
			status = GetLastError();
			HeapFree(heap, 0, keys_values);
			goto end;
		}
		ReplaceStatus s = replace(keys_values, len, template_file, out_file);
		if (s == REPLACE_GET_ERROR) {
			WriteFile(err, "Could not replace\n", 18, NULL, NULL);
			status  = GetLastError();
		} else if (s == REPLACE_OUT_OF_MEMORY) {
			status = 3;
		} else if (s == REPLACE_INVALID) {
			WriteFile(err, "Invalid template file\n", 22, NULL, NULL);
		}
		CloseHandle(out_file);
		HeapFree(heap, 0, keys_values);
	} else if (s == PARSE_GET_ERROR) {
		status = GetLastError();
	} else if (s == PARSE_INVALID_FILE) {
		WriteFile(err, "Invalid translation file\n", 25, NULL, NULL);
		status = 2;
	} else {
		status = 3;
		goto end;
	}
	
end:
	if (buffer != NULL) {
		HeapFree(heap, 0, buffer);
	}
	if (template_file != INVALID_HANDLE_VALUE) {
		CloseHandle(template_file);
	}
	FlushFileBuffers(out);
	FlushFileBuffers(err);
	return status;
}