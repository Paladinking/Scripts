#include <args.h>
#include "parser.h"
#include "format.h"
#include "type_checker.h"
#include "quads.h"
#include "code_generation.h"
#include "log.h"
#include <path_utils.h>
#include <glob.h>
#include <printf.h>

// TODO: modules
// Syntax: import <identifier>
// Finds a file called <identifier>.txt and scans it
// It will not get compiled unless specifed on the command line
// Linker should be able to read and write COFF64 object files
// Support should exist so that higher-level tooling can compile whole project at once.
// Reading COFF objects already works?

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
    FlagValue outfile = { FLAG_STRING };
    FlagInfo flags[] = {
        {'s', "log-to-socket", NULL}, // 0
        {'\0', "show-ast"},           // 1
        {'\0', "show-prossesed-ast"}, // 2
        {'\0', "show-quads"},         // 3
        {'\0', "show-asm"},           // 4
        {'\0', "show-object"},        // 5
        {'\0', "show-all"},           // 6
        {'c', NULL},                  // 7
        {'o', NULL, &outfile},        // 8
    };
    const uint32_t flag_count = sizeof(flags) / sizeof(FlagInfo);
    ErrorInfo err;
    if (!find_flags(argv, &argc, flags, flag_count, &err)) {
        char* s = format_error(&err, flags, flag_count);
        if (s != NULL) {
            _printf_e("%s\n", s);
            Mem_free(s);
        }
        return 1;
    }
    Log_Init(flags[0].count > 0);

    bool show_ast = flags[1].count > 0 || flags[6].count > 0;
    bool show_processed_ast = flags[2].count > 0 || flags[6].count > 0;
    bool show_quads = flags[3].count > 0 || flags[6].count > 0;
    bool show_asm = flags[4].count > 0 || flags[6].count > 0;
    bool show_object = flags[5].count > 0 || flags[6].count > 0;
    bool compile_only = flags[7].count > 0;

    if (argc < 2) {
        LOG_USER_ERROR("Missing argument");
        return 1;
    }

    Parser parser;
    Parser_create(&parser);

    for (uint32_t ix = 1; ix < argc; ++ix) {
        if (!add_module(&parser, argv[ix], false)) {
            LOG_USER_ERROR("Failed to find '%s'\n", argv[ix]);
            return 1;
        }
    }


    for (uint32_t mod = 0; mod < parser.modules.count; ++mod) {
        String s;
        const char* filename = parser.modules.modules[mod].filename;
        if (!read_text_file(&s, filename)) {
            LOG_USER_ERROR("Failed to read '%s'\n", filename);
            return 1;
        }
        parser.modules.modules[mod].content = s;
        scan_program(&parser, &s, filename);

        if (parser.first_error != NULL) {
            dump_errors(&parser);
            return 1;
        }
    }
    if (parser.first_error == NULL) {
        for (uint32_t ix = 0; ix < parser.modules.count; ++ix) {
            Module* mod = &parser.modules.modules[ix];

            parse_program(&parser, &mod->content, mod->filename, mod->import);

            if (parser.first_error != NULL) {
                dump_errors(&parser);
                return 1;
            }
        }
    }


    for (int i = 0; i < parser.function_table.size; ++i) {
        String s;
        if (show_ast && String_create(&s)) {
            fmt_functiondef(parser.function_table.data[i], &parser, &s);
            outputUtf8(s.buffer, s.length);
            String_free(&s);
        }
    }

    TypeChecker_run(&parser);
    if (parser.first_error != NULL) {
        dump_errors(&parser);
        return 1;
    }

    for (int i = 0; i < parser.function_table.size; ++i) {
        String s;
        if (show_processed_ast && String_create(&s)) {
            fmt_functiondef(parser.function_table.data[i], &parser, &s);
            outputUtf8(s.buffer, s.length);
            String_free(&s);
        }
    }

    Quads q;
    q.quads_count = 0;
    Arena* a;
    Quad_GenerateQuads(&parser, &q, &parser.arena);

    dump_errors(&parser);

    String out;
    if (show_quads && String_create(&out)) {
        fmt_quads(&q, &out);
        outputUtf8(out.buffer, out.length);
        String_free(&out);
    }

    Object* object = Generate_code(&q, &parser.function_table,
                                    &parser.externs,
                                    &parser.name_table,
                                    parser.first_str, &parser.arena,
                                    show_asm);

    if (compile_only) {
        const char* outname = outfile.str;
        if (!outfile.has_value) {
            outname = "out.obj";
        }
        ByteBuffer b;
        Buffer_create(&b);
        Object_write(object, &b);

        if (write_file(outname, b.data, b.size)) {
            LOG_USER_WARNING("Created '%s'", outname);
            return 0;
        } else {
            LOG_USER_ERROR("Failed creating '%s'", outname);
            return 1;
        }
    }

    ObjectSet objects;
    ObjectSet_create(&objects);
    ObjectSet_add(&objects, object);

    String kernel32Path;
    String_create(&kernel32Path);

    String path;
    if (create_envvar("LIB", &path) && kernel32Path.buffer != NULL) {
        PathIterator it;
        PathIterator_begin(&it, &path);
        ochar_t* p;
        uint32_t size;
        while ((p = PathIterator_next(&it, &size)) != NULL) {
            String_append_count(&kernel32Path, p, size);
#ifdef _WIN32
            if (p[size - 1] != '\\' && p[size - 1] != '/') {
                String_append(&kernel32Path, '\\');
            }
#else
            if (p[size - 1] != '/') {
                String_append(&kernel32Path, '/');
            }
#endif
            String_extend(&kernel32Path, "kernel32.lib");
            if (is_file(kernel32Path.buffer)) {
                oprintf("Found kernel32.lib at %s\n", kernel32Path.buffer);
                break;
            }
            String_clear(&kernel32Path);
        }
        String_free(&path);
    }

    const char* linker_args[3];
    uint32_t arg_count = 0;
    if (kernel32Path.length > 0) {
        linker_args[0] = kernel32Path.buffer;
        ++arg_count;
    }
    //linker_args[arg_count++] = "stdlib.obj";

    Linker_run(&objects, linker_args, arg_count, show_object);
    String_free(&kernel32Path);

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
    Log_Shutdown();
    Mem_free(argv);

    ExitProcess(status);
}

#else
int main(char** argv, int argc) {
    Log_Init();
    int status = compiler(argv, argc);
    Log_Shutdown();
    return status;
}
#endif
