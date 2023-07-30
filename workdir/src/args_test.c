
extern unsigned parse_args(char** buffer, int len);

int my_main() {
	char* buffer[10];
	unsigned handle = GetStdHandle(0xfffffff5);
	int count = parse_args(buffer, 10);

	
	for (int i = 0; i < count; ++i) {
		int len = strlen(buffer[i]);
		WriteConsoleA(handle, buffer[i], len, 0, 0);
		WriteConsoleA(handle, "\n", 1, 0, 0);
	}
	
	return count;
}
/*
int main(int argc, char** argv) {
	unsigned handle = GetStdHandle(0xfffffff5);
	for (int i = 0; i < argc; ++i) {
		int len = strlen(argv[i]);
		WriteConsoleA(handle, argv[i], len, 0, 0);
		WriteConsoleA(handle, "\n", 1, 0, 0);
	}
}*/