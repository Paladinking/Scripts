#include "test.h"

const char* TEST_NAMES[3] = {
	"parse_args1 test",
	"parse_args2 test",
	"parse_args3 test",
};
const size_t TEST_NAME_SIZE[3] = {
	16, 16, 16
};


const char* test_arg0(char** buffer, const char* name) {
	char buf[4096];
	char* file;
	unsigned res = GetFullPathNameA(buffer[0], 4096, buf, &file);
	if (res >= 4096) {
		return "Too long path\n";
	} else if (res == 0) {
		return "Wrong exe name, could not parse\n";
	}
	if (strcmp(file, name) != 0) {
		return "Wrong exe name\n";
	}

	return 0;
}

// Called with no extra arguments
// Executable args_test1.exe
int parse_args_1() {
	char* buffer[10];
	int count = parse_args(buffer, 10);
	unsigned handle = GetStdHandle(0xfffffff5);
	if (count != 1) {
		FAIL_TEST(0, 1, "Wrong number of args\n");
	}
	const char* res = test_arg0(buffer, "args_test1.exe");
	if (res != 0) {
		FAIL_TEST_S(0, 2, res, strlen(res)); 
	}
	SUCCES_TEST(0);
	return 0;
}

// Called with "Hello world" 1 2 3456789     nice   "run""that"
// Executable args_test2.exe
int parse_args_2() {
	char* buffer[10];
	int count = parse_args(buffer, 10);
	unsigned handle = GetStdHandle(0xfffffff5);
	if (count != 7) {
		FAIL_TEST(1, 1, "Wrong number of args\n");
	}
	const char* res = test_arg0(buffer, "args_test2.exe");
	if (res != 0) {
		FAIL_TEST_S(1, 2, res, strlen(res)); 
	}
	if (strcmp(buffer[1], "Hello world") != 0) {
		FAIL_TEST(1, 3, "Wrong first argument\n");
	}
	if (strcmp(buffer[2], "1") != 0) {
		FAIL_TEST(1, 4, "Wrong second argument\n");
	}
	if (strcmp(buffer[3], "2") != 0) {
		FAIL_TEST(1, 5, "Wrong third argument\n");
	}
	if (strcmp(buffer[4], "3456789") != 0) {
		FAIL_TEST(1, 6, "Wrong fourth argument\n");
	}
	if (strcmp(buffer[5], "nice") != 0) {
		FAIL_TEST(1, 7, "Wrong fifth argument\n");
	}
	if (strcmp(buffer[6], "runthat") != 0) {
		FAIL_TEST(1, 8, "Wrong sixth argument\n");
	}
	SUCCES_TEST(1);
	return 0;
}

// Called with abc "efgh ijkl" mnop qrstuv name
// Executable args_test3.exe
int parse_args_3() {
	char* buffer[4];
	int count = parse_args(buffer, 4);
	unsigned handle = GetStdHandle(0xfffffff5);
	if (count != 4) {
		FAIL_TEST(2, 1, "Wrong number of args\n");
	}
	const char* res = test_arg0(buffer, "args_test3.exe");
	if (res != 0) {
		FAIL_TEST_S(2, 2, res, strlen(res)); 
	}
	if (strcmp(buffer[1], "abc") != 0) {
		FAIL_TEST(2, 3, "Wrong first argument\n");
	}
	if (strcmp(buffer[2], "efgh ijkl") != 0) {
		FAIL_TEST(2, 4, "Wrong second argument\n");
	}
	if (strcmp(buffer[3], "mnop") != 0) {
		FAIL_TEST(2, 5, "Wrong third argument\n");
	}
	SUCCES_TEST(2);
	return 0;
}