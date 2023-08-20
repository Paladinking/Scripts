#include <windows.h>


BOOL StringEq(const char* a, const char* b) {
	while (1) {
		if (*a == '\0') {
			return *b == '\0';
		}
		char c1 = *a;
		char c2 = *b;
		if (c1 >= 'a' && c1 <= 'z') {
			c1 = c1 - 'a' + 'A';
		}
		if (c2 >= 'a' && c2 <= 'z') {
			c2 = c2 - 'a' + 'A';
		}
		if (c1 != c2) {
			return FALSE;
		}
		++a;
		++b;
	}
}

int main() {
	char* argv[4];
	int argc = parse_args(argv, 4);
	HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
	HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
	if (argc < 2) {
		WriteFile(err, "No root key provided\n", 21, NULL, NULL);
		return -1;
	}
	
	HKEY key;
	if (StringEq("HKEY_CLASSES_ROOT", argv[1]) || StringEq("HKCR", argv[1])) {
		key = HKEY_CLASSES_ROOT;
	} else if (StringEq("HKEY_CURRENT_CONFIG", argv[1]) || StringEq("HKCC", argv[1])) {
		key = HKEY_CURRENT_CONFIG;
	} else if (StringEq("HKEY_CURRENT_USER", argv[1]) || StringEq("HKCU", argv[1])) {
		key = HKEY_CURRENT_USER;
	} else if (StringEq("HKEY_LOCAL_MACHINE", argv[1]) || StringEq("HKLM", argv[1])) {
		key = HKEY_LOCAL_MACHINE;
	} else if (StringEq("HKEY_USERS", argv[1]) || StringEq("HKU", argv[1])) {
		key = HKEY_USERS;
	}  else {
		WriteFile(err, "Invalid root key provided\n", 26, NULL, NULL);
		return -2;
	}
	if (argc < 3) {
		WriteFile(err, "No subkey provided\n", 19, NULL, NULL);
		return -3;
	}
	LPCSTR name = NULL;
	if (argc > 3) {
		name = argv[3];
	}
	
	
	HANDLE heap = GetProcessHeap();
	DWORD size = 4096;
	char* buffer = HeapAlloc(heap, 0, size);
	LSTATUS status = RegGetValue(
		key,
		argv[2],
		name, RRF_RT_ANY, NULL, buffer, &size 
	);
	if (status != ERROR_SUCCESS) {
		WriteFile(err, "Could not read key\n", 19, NULL, NULL);
		HeapFree(heap, 0, buffer);
		return status;
	}
	size = 0;
	while (buffer[size++]) {}

	WriteFile(out, buffer, size, NULL, NULL);
	WriteFile(out, "\n", 1, NULL, NULL);
	HeapFree(heap, 0, buffer);

	return 0;
}
