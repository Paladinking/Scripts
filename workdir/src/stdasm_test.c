#include "test.h"

#ifdef __GNUC__
#define main entry_main
#endif

const char* TEST_NAMES[3] = {
	"strcmp test", "memcopy test", "strlen test"
};
const size_t TEST_NAME_SIZE[3] = {
	11, 12, 11
};

int main() {
	const unsigned handle = GetStdHandle(0xfffffff5);
	
	if (strcmp("Hello world", "Hello world") != 0) {
		FAIL_TEST(0, 1, "Not equal\n");
	}
	if (strcmp("", "") != 0) {
		FAIL_TEST(0, 2, "Not equal\n");
	}
	if (strcmp("This is my test", "This is not my test") >= 0) {
		FAIL_TEST(0, 3, "Not less\n");
	}
	if (strcmp("This is my test", "This is a test") <= 0) {
		FAIL_TEST(0, 4, "Not greater\n");
	}
	if (strcmp("Word", "Word 2") >= 0) {
		FAIL_TEST(0, 5, "Not less\n");
	}
	if (strcmp("Word", "") <= 0) {
		FAIL_TEST(0, 6, "Not greater\n");
	}
	SUCCES_TEST(0);

	char buffer[10];
	memcopy("abcdefghi", buffer, 10);
	if (strcmp("abcdefghi", buffer) != 0) {
		FAIL_TEST(1, 1, "Not equal\n");
	}
	memcopy("12345", buffer, 5);
	for (int i = 0; i < 10; ++i) {
		if (buffer[i] != "12345fghi"[i]) {
			FAIL_TEST(1, 2, "Not equal\n");
		}
	}
	char buffer2[5000];
	for (int i = 0; i < 5; ++i) {
		char buf[1000];
		for (int j = 0; j < 1000; ++j) {
			buf[j] = i;
		}
		memcopy(buf, buffer2 + 1000 * i, 1000);
	}
	for (int i = 0; i < 5000; ++i) {
		if (buffer2[i] != i / 1000) {
			FAIL_TEST(1, 3, "Not equal\n");
		}
	}
	memcopy("Hello world", buffer2, 0);
	for (int i = 0; i < 5000; ++i) {
		if (buffer2[i] != i / 1000) {
			FAIL_TEST(1, 4, "Not equal\n");
		}
	}
	SUCCES_TEST(1);
	
	if (strlen("") != 0) {
		FAIL_TEST(2, 1, "Not correct length\n");
	}
	if (strlen("Hello world\n\n\r\t") != 15) {
		FAIL_TEST(2, 2, "Not correct length\n");
	}
	for (int i = 0; i < 5000; ++i) {
		buffer2[i] = 1;
	}
	buffer2[4999] = 0;
	if (strlen(buffer2) != 4999) {
		FAIL_TEST(2, 3, "Not correct length\n");
	}
	
	SUCCES_TEST(2);
	return 0;
}