#ifndef PARSER_H_00
#define PARSER_H_00
#include "tables.h"
#include "dynamic_string.h"

bool Parser_create(Parser* parser);

void scan_program(Parser* parser, String* indata);

void parse_program(Parser* parser, String* indata);

#endif
