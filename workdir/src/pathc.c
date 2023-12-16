#define UNICODE
#ifdef __GNUC__
#define main entry_main
#endif
#include <windows.h>
#include <tlhelp32.h>
#include "path_utils.h"


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


OpStatus paths_add(
	int argc, 
	LPWSTR* argv,
	PathBuffer* path_buffer,
	BOOL before,
	BOOL expand
) {

	if (argc < 2) {
		return OP_MISSSING_ARG;
	}
	OpStatus status = OP_NO_CHANGE;

	for (int arg = 1; arg < argc; ++arg) {
		OpStatus res = path_add(argv[arg], path_buffer, before, expand);
		if (res == OP_SUCCESS) {
			status = OP_SUCCESS;
		} else if (res != OP_NO_CHANGE) {
			return res;
		}
	}
	return status;
}

OpStatus paths_remove(
	int argc, 
	LPWSTR* argv,
	PathBuffer* path_buffer,
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
		OpStatus status = path_remove(argv[arg], path_buffer, expand);
		if (status == OP_SUCCESS) {
			removed_any = TRUE;
		} else if (status != OP_NO_CHANGE) {
			return status;
		}
	}
	if (!removed_any) {
		return OP_NO_CHANGE;
	}
	return OP_SUCCESS;
}

typedef enum _OperationType {
	OPERATION_ADD, OPERATION_REMOVE, OPERATION_MOVE
} OperationType;

PathBuffer path_buffer = {NULL, 0, 0};

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
	BOOL update = find_flag(argv, &argc, L"-u", L"--update");

	HANDLE heap = GetProcessHeap();

	DWORD capacity = 2000;
	if (!get_path_envvar(capacity, argc - 1, &path_buffer)) {
		WriteFile(err, "Could not get Path\n", 19, NULL, NULL);
		return 6;
	}

	OpStatus status;

	if (operation == OPERATION_ADD) {
		status = paths_add(
			argc - 1, argv + 1, &path_buffer, before, expand
		);
	} else if (operation == OPERATION_REMOVE){
		status = paths_remove(
			argc - 1, argv + 1, &path_buffer, before, expand
		);
	} else {
		status = paths_remove(
			argc - 1, argv + 1, &path_buffer, FALSE, expand
		);
		if (status <= OP_NO_CHANGE) {
			status = paths_add(
				argc - 1, argv + 1, &path_buffer, before, expand
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
	if (update) {
		if (status == OP_NO_CHANGE) {
			return 1;
		}
		DWORD id = GetParentProcessId();
		if (id == 0) {
			WriteFile(err, "Could not get parent\n", 22, NULL, NULL);
			return 4;
		}
		int res = SetProcessEnvironmentVariable(L"PATH", path_buffer.ptr, id);
		if (res != 0) {
			WriteFile(err, "Failed setting PATH\n", 20, NULL, NULL);
			return 5;
		}
	} else {
		WriteFileWide(out, path_buffer.ptr, path_buffer.size);
		WriteFile(out, "\n", 1, NULL, NULL);
	}
	
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
	HeapFree(heap, 0, path_buffer.ptr);
	LocalFree(argv);
	ExitProcess(status);
	return status;
}