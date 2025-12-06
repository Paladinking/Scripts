#ifndef PARSER_H_00
#define PARSER_H_00
#include "tables.h"
#include "dynamic_string.h"

void Parser_create(Parser* parser);

void scan_program(Parser* parser, String* indata, const char* filename);

void parse_program(Parser* parser, String* indata, const char* filename, bool is_import);

#endif
