#define UNICODE
#ifdef __GNUC__
#define main entry_main
#endif
#include <windows.h>
#include <tlhelp32.h>
#include "path_utils.h"
#include "string_conv.h"

DWORD GetParentProcessId() {
    // Get the current process ID
    DWORD currentProcessId = GetCurrentProcessId();

    // Create a snapshot of the system processes
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    // Iterate through the processes to find the parent process
    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (pe32.th32ProcessID == currentProcessId) {
                // Found the current process in the snapshot, return its parent process ID
                CloseHandle(hSnapshot);
                return pe32.th32ParentProcessID;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    // Process not found or unable to retrieve information
    CloseHandle(hSnapshot);
    return 0;
}

LPSTR read_paths_file(LPWSTR binpath, DWORD* count) {
	HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
	HANDLE heap = GetProcessHeap();

	WCHAR dir[256];
	WCHAR drive[10];
	DWORD status = _wsplitpath_s(binpath, drive, 10, dir, 256, NULL, 0, NULL, 0);
	if (status != 0) {
		return NULL;
	}
	WCHAR path_file[256];
	status = _wmakepath_s(path_file, 256, drive, dir, L"paths", L".txt");
	if (status != 0) {
		return NULL;
	}
	HANDLE hPaths = CreateFile(path_file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hPaths == INVALID_HANDLE_VALUE) {
		return NULL;
	}
	LARGE_INTEGER size;
	if (!GetFileSizeEx(hPaths, &size)) {
		CloseHandle(hPaths);
		return NULL;
	}
	LPSTR buffer = HeapAlloc(heap, 0, size.QuadPart + 1);
	if (buffer == NULL) {
		CloseHandle(hPaths);
		return NULL;
	}
	DWORD read;
	if (!ReadFile(hPaths, buffer, size.QuadPart, &read, NULL) || read != size.QuadPart) {
		CloseHandle(hPaths);
		HeapFree(heap, 0, buffer);
		return NULL;
	}
	CloseHandle(hPaths);
	buffer[size.QuadPart] = '\0';
	*count = 1;
	LPSTR line_end = buffer - 1;
	while (1) {
		line_end = strchr(line_end + 1, '\n');
		if (line_end == NULL) {
			return buffer;
		}
		// Check for windows newline
		if (line_end > buffer && *(line_end - 1) == '\r') {
			*(line_end - 1) = '\0';
		} else {
			*line_end = '\0';
		}
		*count += 1;
	}

	return buffer;
}

PathBuffer path_buffer = {NULL, 0, 0};
WideBuffer add_buffer = {NULL, 0, 0};

OpStatus add_paths(LPSTR paths, DWORD count, LPSTR name) {
	HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
	HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
	LPSTR line = paths;
	DWORD name_len = strlen(name);
	OpStatus res = OP_NO_CHANGE;
	for (unsigned i = 0; i < count; ++i) {
		if (*line == '\n') {
			++line;
		}
		LPSTR line_name = NULL;
		if ((line_name = strchr(line, '|')) != NULL && _stricmp(line_name + 1, name) == 0) {
			DWORD len = line_name - line;
			if (line[0] == '"' && line[len - 1] == '"') {
				++line;
				len -= 2;
			}
			NarrowStringToWide(line, len, &add_buffer);
			OpStatus status = path_add(add_buffer.ptr, &path_buffer, TRUE, FALSE);
			if (status > OP_NO_CHANGE) {
				WriteFile(out, "Failed adding ", 14, NULL, NULL);
				WriteFile(out, line, len, NULL, NULL);
				WriteFile(out, " to Path\n", 9, NULL, NULL);
				return status;
			} else if (status != OP_NO_CHANGE) {
				WriteFile(out, "Added ", 6, NULL, NULL);
				WriteFile(out, line, len, NULL, NULL);
				WriteFile(out, " to Path\n", 9, NULL, NULL);
				res = status;
			}
			line = line_name + name_len + 2;
		} else if ((line_name = strchr(line, '<')) != NULL && _stricmp(line_name + 1, name) == 0) {
			*line_name = '\0';
			OpStatus status = add_paths(paths, count, line);
			if (status > OP_NO_CHANGE) {
				return status;
			} else if (status != OP_NO_CHANGE) {
				res = status;
			}
			*line_name = '<';
			line = line_name + name_len + 2;
		} else {
			line = line + strlen(line) + 1;
		}
	}
	return OP_SUCCESS;
}

int main() {
	HANDLE heap = GetProcessHeap();
	LPWSTR args = GetCommandLine();
	int argc;
	int status = 0;
	LPWSTR* argv = CommandLineToArgvW(args, &argc);
	if (argc < 2) {
		status = 1;
		goto end;
	}
	HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
	HANDLE err = GetStdHandle(STD_ERROR_HANDLE);

	DWORD lines;
	LPSTR paths_data = read_paths_file(argv[0], &lines);
	if (paths_data == NULL) {
		WriteConsole(err, L"Failed reading file\n", 20, NULL, NULL);
		status = 2;
		goto end;
	}
	
	if (!get_path_envvar(2000, 4, &path_buffer)) {
		WriteConsole(err, L"Could not get Path\n", 19, NULL, NULL);
		status = 3;
		HeapFree(heap, 0, path_buffer.ptr);
		goto end;
	}
	DWORD arg_capacity = 20;
	LPWSTR arg_buffer = HeapAlloc(heap, 0, arg_capacity);

	BOOL changed = FALSE;

	StringBuffer buffer = {NULL, 0, 0};
	for (DWORD i = 1; i < argc; ++i) {
		DWORD len = wcslen(argv[i]);
		if (!WideStringToNarrow(argv[i], len, &buffer)) {
			// WRITE ERROR
			continue;
		}
		OpStatus res = add_paths(paths_data, lines, buffer.ptr);
		if (res == OP_SUCCESS) {
			changed = TRUE;
		} else if (res != OP_NO_CHANGE) {
			status = res;
			HeapFree(heap, 0, add_buffer.ptr);
			HeapFree(heap, 0, buffer.ptr);
			HeapFree(heap, 0, paths_data);
			HeapFree(heap, 0, path_buffer.ptr);
			goto end;
		}
		if (add_paths(paths_data, lines, buffer.ptr) == OP_SUCCESS) {
			changed = TRUE;
		}
	}
	HeapFree(heap, 0, add_buffer.ptr);
	HeapFree(heap, 0, buffer.ptr);
	HeapFree(heap, 0, paths_data);
	if (!changed) {
		status = 1;
		HeapFree(heap, 0, path_buffer.ptr);
		goto end;
	}
	DWORD id = GetParentProcessId();
	if (id == 0) {
		WriteConsole(err, L"Could not get parent process\n", 29, NULL, NULL);
		status = 4;
		HeapFree(heap, 0, path_buffer.ptr);
		goto end;
	}
	if (SetProcessEnvironmentVariable(L"PATH", path_buffer.ptr, id) != 0) {
		WriteConsole(err, L"Failed setting Path\n", 20, NULL, NULL);
		status = 5;
	}
	HeapFree(heap, 0, path_buffer.ptr);
end:
	FlushFileBuffers(out);
	FlushFileBuffers(err);
	HeapFree(heap, 0, expanded);
	LocalFree(argv);
	ExitProcess(status);
	return status;
}