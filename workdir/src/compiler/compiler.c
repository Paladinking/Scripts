#include <args.h>
#include "parser.h"
#include "format.h"
#include "type_checker.h"
#include "quads.h"
#include "code_generation.h"
#include "log.h"
#include <glob.h>
#include <printf.h>

void dump_errors(Parser* parser) {
    String s;
    if (String_create(&s)) {
        fmt_errors(parser, &s);
        _printf_e("%s", s.buffer);
        parser->first_error = NULL;
        parser->last_error = NULL;
        String_free(&s);
    }
}

int compiler(char** argv, int argc) {
    Log_Init();

    Parser parser;
    if (!Parser_create(&parser)) {
        _printf_e("Failed creating parser\n");
        return 1;
    }

    String s;
    if (!read_text_file(&s, L"src\\compiler\\program.txt")) {
        _printf_e("Failed to read program.txt\n");
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
        String s;
        if (String_create(&s)) {
            fmt_functiondef(parser.function_table.data[i], &parser, &s);
            //outputUtf8(s.buffer, s.length);
            String_free(&s);
        }
    }

    TypeChecker_run(&parser);
    if (parser.first_error != NULL) {
        dump_errors(&parser);
        Log_Shutdown();
        return 1;
    }

    for (int i = 0; i < parser.function_table.size; ++i) {
        String s;
        if (String_create(&s)) {
            fmt_functiondef(parser.function_table.data[i], &parser, &s);
            //outputUtf8(s.buffer, s.length);
            String_free(&s);
        }
    }

    Quads q;
    q.quads_count = 0;
    Arena* a;
    Quad_GenerateQuads(&parser, &q, &parser.arena);

    dump_errors(&parser);

    String out;
    if (String_create(&out)) {
        fmt_quads(&q, &out);
        //outputUtf8(out.buffer, out.length);
        String_free(&out);
    }

    Generate_code(&q, &parser.function_table, &parser.name_table, &parser.arena);

    Log_Shutdown();

    return 0;
}

#ifdef WIN32
int main() {
    // Obtain utf8 command line
    wchar_t* cmd = GetCommandLineW();
    String argbuf;
    String_create(&argbuf);
    if (!String_from_utf16_str(&argbuf, cmd)) {
        return 1;
    }
    int argc;
    char** argv = parse_command_line(argbuf.buffer, &argc);
    String_free(&argbuf);
    int status = compiler(argv, argc);
    Mem_free(argv);

    ExitProcess(status);
}

#else
int main(char** argv, int argc) {
    return compiler(argv, argc);
}
#endif
