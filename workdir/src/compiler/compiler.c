#include "parser.h"
#include "format.h"
#include "type_checker.h"
#include "quads.h"
#include "code_generation.h"
#include "log.h"
#include <glob.h>
#include <printf.h>

void dump_errors(Parser* parser) {
    WString s;
    WString_create(&s);
    fmt_errors(parser, &s);
    _wprintf_e(L"%s", s.buffer);
    parser->first_error = NULL;
    parser->last_error = NULL;
    WString_free(&s);
}

int main() {

    Log_Init();

    Parser parser;
    if (!Parser_create(&parser)) {
        _wprintf_e(L"Failed creating parser\n");
        return 1;
    }

    String s;
    if (!read_text_file(&s, L"src\\compiler\\program.txt")) {
        _wprintf_e(L"Failed to read program.txt\n");
        return 1;
    }

    parser.indata = s.buffer;
    parser.input_size = s.length;

    parse_program(&parser);
    if (parser.first_error != NULL) {
        dump_errors(&parser);
        Log_Shutdown();
        return 1;
    }


    for (int i = 0; i < parser.function_table.size; ++i) {
        WString s;
        if (WString_create(&s)) {
            fmt_functiondef(parser.function_table.data[i], &parser, &s);
            outputw(s.buffer, s.length);
            WString_free(&s);
        }
    }

    TypeChecker_run(&parser);
    if (parser.first_error != NULL) {
        dump_errors(&parser);
        Log_Shutdown();
        return 1;
    }

    for (int i = 0; i < parser.function_table.size; ++i) {
        WString s;
        if (WString_create(&s)) {
            fmt_functiondef(parser.function_table.data[i], &parser, &s);
            outputw(s.buffer, s.length);
            WString_free(&s);
        }
    }

    Quads q;
    q.quads_count = 0;
    Arena* a;
    Quad_GenerateQuads(&parser, &q, &parser.arena);

    dump_errors(&parser);

    WString out;
    if (WString_create(&out)) {
        fmt_quads(&q, &out);
        outputw(out.buffer, out.length);
        WString_free(&out);
    }

    Generate_code(&q, &parser.function_table, &parser.name_table, &parser.arena);

    Log_Shutdown();
}
