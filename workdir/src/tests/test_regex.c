#include "regex.h"
#include "printf.h"
#include "dynamic_string.h"


#define ASSERT_TRUE(b, ...) if (!(b)) {              \
    _wprintf(L"Test failed at %S:%u, ", __FILE__, __LINE__); \
    _wprintf(__VA_ARGS__); _wprintf(L"\n");          \
    ExitProcess(1);                                  \
}

#define COMPILE_REGEX(dest, regex) do {dest = Regex_compile(regex); \
    ASSERT_TRUE(dest, L"Failed compiling regex '" L## regex L"'") \
    ASSERT_TRUE(dest->dfa != NULL, L"Dfa failed to compile for '" L##regex L"'"); \
} while (0)

#define REGEX_FULLMATCH(reg, s) \
    ASSERT_TRUE(Regex_fullmatch(reg, s, strlen(s)) == REGEX_MATCH, \
                L"Expected fullmatch '" L##s L"'")

#define REGEX_NOFULLMATCH(reg, s) \
    ASSERT_TRUE(Regex_fullmatch(reg, s, strlen(s)) == REGEX_NO_MATCH, \
                L"Expected no fullmatch '" L##s L"'")

#define REGEX_ALLMATCH_BEGIN(reg, s) { RegexAllCtx ctx; const char* m; \
    uint64_t len; Regex_allmatch_init(reg, s, strlen(s), &ctx)

#define REGEX_ALLMATCH_MATCH(offset, l)                            \
    ASSERT_TRUE(Regex_allmatch(&ctx, &m, &len) == REGEX_MATCH,     \
                L"Expected match at %d, length %d", offset, l);    \
    ASSERT_TRUE(m - ctx.str == offset && l == len,                 \
                L"Wrong match, expected offset %d, length %d"      \
                L", got offset %d, length %d", offset, l, m - ctx.str, len);

#define START_PERF(loop_count) { LARGE_INTEGER freq, start, end; \
    QueryPerformanceFrequency(&freq); \
    QueryPerformanceCounter(&start);    \
    for (uint64_t loop_count_i = 0; loop_count_i < (loop_count); ++loop_count_i) {

#define END_PERF(msg) } \
    QueryPerformanceCounter(&end); \
    _wprintf(L##msg L": %llu ms\n", (end.QuadPart - start.QuadPart) * 1000 / freq.QuadPart); }

#define REGEX_ALLMATCH_END()                                      \
    ASSERT_TRUE(Regex_allmatch(&ctx, &m, &len) == REGEX_NO_MATCH, \
                L"Expected no match, matched offset %d, length %d", m - ctx.str, len); }

const char* TEST_LINES[] = {
    "Usage: path-add.exe [NAME]...",
    "Goes through paths.txt and adds all matching lines.",
    "Lines in paths.txt can have 3 different formats:",
    " 1. {PATH}|{NAME}",
    "  If {NAME} matches an input argument, {PATH} will be added to PATH environment variable.",
    "  Note that | is a literal and should be present in the file.",
    " 2. {CHILD}<{NAME}",
    "  If {NAME} matches an input argument, add {CHILD} next in argument list.",
    "  This can (but does not have to) cause infinite recursion for bad input, causing the program to run out of memory.",
    " 3. {FILE}>{NAME}",
    "  If {NAME} matches an input argument, parse {FILE} for environment variables.",
    "All other lines are ignored.",
    "Lines in an environment variable file given by format 3 in paths.txt can have 5 different formats:",
    " 1. !!{VAR}",
    "  Stop parsing file if {VAR} is 100 defined.",
    " 2. **{TEXT}",
    "  {TEXT} is printed to the console.",
    " 3. {PATH}|{VAR}",
    "  {PATH} will be prepended to the environment variable {VAR}, separated by semi-colons.",
    " 4. {VALUE}>{VAR}",
    "  The environment variable {VAR} will be set to {VALUE}.",
    "  Just >{VAR} is allowed and will 99 delete the environment variable {VAR}.",
    " 5. {TO_SAVE}<{VAR}",
    "  The environment variable {VAR} will be set to the value in {TO_SAVE}.",
    "  This is usefull for saving and restoring environment variables.",
    "All other lines are again ignored."
};
const size_t TEST_LINES_VAR = 10;
const size_t TEST_LINE_NUM = 13;
const size_t TEST_LINE_COUNT = sizeof(TEST_LINES) / sizeof(const char*);

int main() {
    Regex* reg;
    COMPILE_REGEX(reg, "Hello world");
    REGEX_FULLMATCH(reg, "Hello world");
    REGEX_NOFULLMATCH(reg, "Hello world ");
    REGEX_NOFULLMATCH(reg, "Hello world\n");
    REGEX_NOFULLMATCH(reg, "hello world");
    REGEX_NOFULLMATCH(reg, "Hello wo");
    REGEX_NOFULLMATCH(reg, "");
    REGEX_ALLMATCH_BEGIN(reg, "Hello world 123\n Hello worldHello world HelHello world");
        REGEX_ALLMATCH_MATCH(0, 11);
        REGEX_ALLMATCH_MATCH(17, 11);
        REGEX_ALLMATCH_MATCH(28, 11);
        REGEX_ALLMATCH_MATCH(43, 11);
    REGEX_ALLMATCH_END();
    Regex_free(reg);

    COMPILE_REGEX(reg, "");
    REGEX_FULLMATCH(reg, "");
    REGEX_NOFULLMATCH(reg, "1");
    REGEX_NOFULLMATCH(reg, "12334657812345346576875463524132456754321456764354");
    REGEX_ALLMATCH_BEGIN(reg, "Hello world");
        for (int i = 0; i <= 11; ++i) {
            REGEX_ALLMATCH_MATCH(i, 0);
        }
    REGEX_ALLMATCH_END();
    Regex_free(reg);

    COMPILE_REGEX(reg, "a*");
    REGEX_FULLMATCH(reg, "aaaaaaaaaaaaaa");
    REGEX_FULLMATCH(reg, "");
    REGEX_FULLMATCH(reg, "a");
    REGEX_FULLMATCH(reg, "aaaa");
    REGEX_NOFULLMATCH(reg, "123jfosaij");
    REGEX_NOFULLMATCH(reg, "aaaaab");
    REGEX_ALLMATCH_BEGIN(reg, "Helloaa.aaa.a");
        REGEX_ALLMATCH_MATCH(0, 0);
        REGEX_ALLMATCH_MATCH(1, 0);
        REGEX_ALLMATCH_MATCH(2, 0);
        REGEX_ALLMATCH_MATCH(3, 0);
        REGEX_ALLMATCH_MATCH(4, 0);
        REGEX_ALLMATCH_MATCH(5, 2);
        REGEX_ALLMATCH_MATCH(7, 0);
        REGEX_ALLMATCH_MATCH(8, 3);
        REGEX_ALLMATCH_MATCH(11, 0);
        REGEX_ALLMATCH_MATCH(12, 1);
        REGEX_ALLMATCH_MATCH(13, 0);
    REGEX_ALLMATCH_END();
    Regex_free(reg);

    COMPILE_REGEX(reg, "a..b...c");
    REGEX_FULLMATCH(reg, "a12b345c");
    REGEX_FULLMATCH(reg, "a..b...c");
    REGEX_FULLMATCH(reg, "a\t\tb\n\t\nc");
    REGEX_NOFULLMATCH(reg, "a123b123c");
    REGEX_FULLMATCH(reg, "a\xf0\x9f\x92\xb2MbMMMc");
    REGEX_ALLMATCH_BEGIN(reg, "Hello worlda\xc3\xb6MbbbbcMMMMMMMMMMM");
        REGEX_ALLMATCH_MATCH(11, 9);
    REGEX_ALLMATCH_END();
    Regex_free(reg);

    COMPILE_REGEX(reg, "a\xe2\x9e\x95*\xc3\xb6");
    REGEX_FULLMATCH(reg, "a\xc3\xb6");
    REGEX_FULLMATCH(reg, "a\xe2\x9e\x95\xc3\xb6");
    REGEX_FULLMATCH(reg, "a\xe2\x9e\x95\xe2\x9e\x95\xc3\xb6");
    REGEX_NOFULLMATCH(reg, "aa\xe2\x9e\x95\xc3\xb6");

    REGEX_ALLMATCH_BEGIN(reg, "a\xe2\x9e\x95\xe2\x9e\x95Hella\xc3\xb6Worldaba\xe2\x9e\x95\xc3\xb6""a\x9e\x95\xe2");
        REGEX_ALLMATCH_MATCH(11, 3);
        REGEX_ALLMATCH_MATCH(21, 6);
    REGEX_ALLMATCH_END();
    Regex_free(reg);

    COMPILE_REGEX(reg, "..*");
    REGEX_FULLMATCH(reg, "asfjoijioj2oi3j4ias90if9a0eit2jjji\xe2\x9e\x95\xe2\x9e\x95");
    REGEX_FULLMATCH(reg, "a");
    REGEX_NOFULLMATCH(reg, "");
    REGEX_ALLMATCH_BEGIN(reg, "123oaksfkoaskdpaksdkaskdpok");
        REGEX_ALLMATCH_MATCH(0, 27);
    REGEX_ALLMATCH_END();
    Regex_free(reg);

    COMPILE_REGEX(reg, "\xf0\x9f\x92\xb2\xf0\x9f\x92\xb2\xf0\x9f\x92\xb2*");
    REGEX_FULLMATCH(reg, "\xf0\x9f\x92\xb2\xf0\x9f\x92\xb2\xf0\x9f\x92\xb2");

    REGEX_ALLMATCH_BEGIN(reg, "\xc0\xbbHello \xf0\x9f\x92\xb2\r\nHello world\r\n\xf0\x9f\x92\xb2\xf0\x9f\x92\xb2\xf0\x9f\x92\xb2Hello\r\n");
        REGEX_ALLMATCH_MATCH(27, 12);
    REGEX_ALLMATCH_END();

    Regex_free(reg);
    
    _wprintf(L"All tests successfull\n");

    COMPILE_REGEX(reg, "VAR");
    START_PERF(1000000);
        RegexAllCtx ctx;
        const char* s;
        uint64_t len;
        uint64_t total_matches = 0;
        for (uint64_t i = 0; i < TEST_LINE_COUNT; ++i) {
            Regex_allmatch_init(reg, TEST_LINES[i], strlen(TEST_LINES[i]), &ctx);
            while (Regex_allmatch(&ctx, &s, &len)) {
                total_matches += len;
            }
        }
        ASSERT_TRUE(total_matches == TEST_LINES_VAR * 3,
                    L"Expected %d matches, got %d",
                    total_matches, TEST_LINES_VAR * 3);
    END_PERF("VAR time");
    Regex_free(reg);

    COMPILE_REGEX(reg, "[0-9]\\{1,\\}");
    START_PERF(1000000);
        RegexAllCtx ctx;
        const char* s;
        uint64_t len;
        uint64_t total_matches = 0;
        for (uint64_t i = 0; i < TEST_LINE_COUNT; ++i) {
            Regex_allmatch_init(reg, TEST_LINES[i], strlen(TEST_LINES[i]), &ctx);
            while (Regex_allmatch(&ctx, &s, &len)) {
                total_matches += 1;
            }
        }
        ASSERT_TRUE(total_matches == TEST_LINE_NUM,
                    L"Expected %d matches, got %d",
                    total_matches, TEST_LINE_NUM);

    END_PERF("NUM time");
    Regex_free(reg);

    ExitProcess(0);
}
