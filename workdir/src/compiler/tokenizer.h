#ifndef COMPILER_TOKENIZER_H_00
#define COMPILER_TOKENIZER_H_00

#include "utils.h"
#include <dynamic_string.h>

// Set parser input to indata
void parser_set_input(Parser* parser, String* indata);

// Skip all space characters at current pos
void parser_skip_spaces(Parser* parser);

// Read a keyword / identifier
const uint8_t* parser_read_name(Parser* parser, uint32_t* len);

// Read a string literal
uint8_t* parser_read_string(Parser* parser, uint64_t* len);

// Read a number literar, integer or real
bool parser_read_number(Parser* parser, uint64_t* i, double* d, bool* is_int);

void parser_row_col(const Parser* parser, uint64_t pos, uint64_t* row, uint64_t* col);

void parser_skip_statement(Parser* parser);

static inline bool is_identifier(uint8_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
             c == '_' || (c >= '0' && c <= '9');
}

static inline bool is_identifier_start(uint8_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c == '_');
}


#endif
