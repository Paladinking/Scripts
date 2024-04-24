#define UNICODE
#include "cli.h"
#include "printf.h"

int main() {
    _wprintf(L"Hello \xf6 world");    

    DynamicWString data[100];
    for (int i = 0; i < 100; ++i) {
        DynamicWStringCreate(&data[i]);
    }
    for (int i = 0; i < 10; ++i) {
        const wchar_t * alph = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        DynamicWStringExtend(&data[10 * i + 0], L"This is a test 0 ");
        DynamicWStringExtend(&data[10 * i + 0], alph);
        DynamicWStringExtend(&data[10 * i + 0], alph);
        //DynamicWStringExtend(&data[10 * i + 1], L"This is a test 1");
        DynamicWStringExtend(&data[10 * i + 2], L"This is a test 2");
        //DynamicWStringExtend(&data[10 * i + 3], L"This is a test 3");
        DynamicWStringExtend(&data[10 * i + 4], L"This is a \xd808\xdc00 test 4");
        DynamicWStringExtend(&data[10 * i + 4], alph);
        DynamicWStringExtend(&data[10 * i + 4], alph);
        DynamicWStringExtend(&data[10 * i + 4], alph);
        DynamicWStringExtend(&data[10 * i + 5], L"This is a test 5");
        DynamicWStringExtend(&data[10 * i + 6], L"This is \xac01 a test 6");
        DynamicWStringExtend(&data[10 * i + 7], L"This is a test 7");
        DynamicWStringExtend(&data[10 * i + 8], L"This is a test \x01ac 8");
        DynamicWStringExtend(&data[10 * i + 9], L"This is \xf6 a test 9");
        DynamicWStringExtend(&data[10 * i + 9], alph);
    }

    CliList* list = CliList_create(L"Hello", data, 100, CLI_EDIT | CLI_MOVE | CLI_INSERT | CLI_DELETE);
    CliList_run(list);
    CliList_free(list);

    _printf("\n\rDone\n");
    WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), "\xf6", 1, NULL, NULL);
    _printf("\nHello \xf6 world\n");

    return 0;
}
