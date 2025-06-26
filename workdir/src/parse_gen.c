#include <stdint.h>
#include "mem.h"
#include "dynamic_string.h"
#include "printf.h"

enum TokenType {
    TOKEN_LITERAL = 0, TOKEN_END = 1, TOKEN_IDENTIFIER = 2, TOKEN_STATEMENTS = 3
};
typedef struct Token {
    enum TokenType id;
    union {
        const char* literal;
        int64_t identifier;
        int64_t statements;
    };
} Token;

struct StackEntry {
    int32_t state;
    union {
        int64_t m0;
        int64_t m1;
        int64_t m3;
        int64_t m4;
        int64_t m11;
        int64_t m13;
        int64_t m14;
        int64_t m16;
    };
};

typedef void Program;

struct Stack {
    struct StackEntry* b;
    uint32_t size;
    uint32_t cap;
};

Program* Prog2(void* ctx, uint64_t start, uint64_t end) {
    return NULL;
}

Program* Prog(void* ctx, uint64_t start, uint64_t end, Program* p, int64_t i) {
    return NULL;
}

const static int32_t reduce_table[180] = {
    1, -1, 1, -1, -1, -1, -1, -1, -1, -1,
    -1, 2, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, 6, -1, -1, 12, 6, 17, 12,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, 8, 8, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, 14, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

struct Tokenizer {
    String s;
    uint64_t start;
    uint64_t end;
    Token last_token;
};

Token peek_token(void* ctx, uint64_t* start, uint64_t* end) {
    struct Tokenizer* t = ctx;
    *start = t->start;
    *end = t->end;

    return t->last_token;
}

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool is_identifier_start(char c) {
    return c == '_' || (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z');
}

bool is_identifier_char(char c) {
    return is_identifier_start(c) || (c >= '0' && c <= '9');
}

void _consume_token(void* ctx) {
    struct Tokenizer* t = ctx;
    t->start = t->end;
    if (t->start >= t->s.length) {
        t->last_token.id = TOKEN_END;
        t->end = t->start;
        return;
    }
    while (is_space(t->s.buffer[t->start])) {
        t->start += 1;
        if (t->start >= t->s.length) {
            t->last_token.id = TOKEN_END;
            t->end = t->start;
            return;
        }
    }
    uint64_t ix = t->start;
    if (t->s.buffer[ix] == '{') {
        ++ix;
        uint32_t count = 1;
        while (1) {
            if (ix >= t->s.length) {
                // ERROR
                t->start = ix;
                t->end = ix;
                t->last_token.id = TOKEN_END;
                return;
            }
            char c = t->s.buffer[ix];
            ++ix;
            if (c == '}') {
                --count;
                if (count == 0) {
                    t->end = ix;
                    t->last_token.id = TOKEN_STATEMENTS;
                    return;
                }
            } else if (c == '{') {
                ++count;
            } else if (c == '"' || c == '\'') {
                while (1) {
                    if (ix >= t->s.length) {
                        // ERROR
                        t->start = ix;
                        t->end = ix;
                        t->last_token.id = TOKEN_END;
                        return;
                    }
                    if (t->s.buffer[ix] == c) {
                        ++ix;
                        break;
                    } else if (t->s.buffer[ix] == '\\') {
                        ++ix;
                    }
                }
            }
        }
    } else if (t->s.buffer[ix] == '-') {
        ++ix;
        if (ix < t->s.length && t->s.buffer[ix] == '>') {
            ++ix;
        }
        t->end = ix;
        t->last_token.id = TOKEN_LITERAL;
        t->last_token.literal = t->s.buffer + t->start;
    } else if (is_identifier_start(t->s.buffer[ix])) {
        ++ix;
        while (ix < t->s.length && is_identifier_char(t->s.buffer[ix])) {
            ++ix;
        }
        t->end = ix;
        if (t->end - t->start == 2 && t->s.buffer[t->start] == 'f' &&
            t->s.buffer[t->start + 1] == 'n') {
            t->last_token.id = TOKEN_LITERAL;
            t->last_token.literal = t->s.buffer + t->start;
        } else {
            t->last_token.id = TOKEN_IDENTIFIER;
        }
    } else {
        ++ix;
        t->end = ix;
        t->last_token.id = TOKEN_LITERAL;
        t->last_token.literal = t->s.buffer + t->start;
    }
}

void consume_token(void* ctx) {
    _consume_token(ctx);
    struct Tokenizer* t = ctx;
    _printf("Token %ld\n", t->last_token.id);
}

static void push_state(struct Stack* stack, struct StackEntry n) {
    if (stack->size == stack->cap) {
        struct StackEntry* p = Mem_realloc(stack->b,
                           2 * stack->cap * sizeof(struct StackEntry));
        if (p == NULL) {
            // TODO
        }
        stack->b = p;
        stack->cap *= 2;
    }
    stack->b[stack->size] = n;
    stack->size += 1;
}

void parse(void* ctx) {
    int32_t state = 0;
    struct Stack stack;
    stack.b = Mem_alloc(16 * sizeof(struct StackEntry));
    stack.size = 1;
    stack.b[0].state = 0;
    stack.cap = 16;
    while (1) {
        struct StackEntry n;
        uint64_t start, end;
        Token t = peek_token(ctx, &start, &end);
        uint64_t len = end - start;
        switch (state) {
        case 0: {
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 2 && t.literal[0] == 'f' && t.literal[1] == 'n') {
                    n.state = state = reduce_table[state * 10 + 0];
                    push_state(&stack, n);
                } else {
                    goto failure;
                }
                break;
            case TOKEN_END:
                n.state = state = reduce_table[state * 10 + 0];
                push_state(&stack, n);
                break;
            default:
                goto failure;
            }
            break;
        }
        case 1: {
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 2 && t.literal[0] == 'f' && t.literal[1] == 'n') {
                    n.state = state = 3;
                    push_state(&stack, n);
                    consume_token(ctx);
                } else {
                    goto failure;
                }
                break;
            default:
                goto failure;
            }
            break;
        }
        case 2: {
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 2 && t.literal[0] == 'f' && t.literal[1] == 'n') {
                    stack.size -= 2;
                    state = stack.b[stack.size - 1].state;
                    n.state = state = reduce_table[state * 10 + 2];
                    stack.b[stack.size] = n;
                    ++stack.size;
                } else {
                    goto failure;
                }
                break;
            case TOKEN_END:
                stack.size -= 2;
                state = stack.b[stack.size - 1].state;
                n.state = state = reduce_table[state * 10 + 2];
                stack.b[stack.size] = n;
                ++stack.size;
                break;
            default:
                goto failure;
            }
            break;
        }
        case 3: {
            switch (t.id) {
            case TOKEN_IDENTIFIER:
                n.state = state = 4;
                n.m0 = t.identifier;
                push_state(&stack, n);
                consume_token(ctx);
                break;
            default:
                goto failure;
            }
            break;
        }
        case 4: {
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 1 && t.literal[0] == '(') {
                    n.state = state = 5;
                    push_state(&stack, n);
                    consume_token(ctx);
                } else {
                    goto failure;
                }
                break;
            default:
                goto failure;
            }
            break;
        }
        case 5: {
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 1 && t.literal[0] == ')') {
                    n.state = state = reduce_table[state * 10 + 3];
                    push_state(&stack, n);
                } else {
                    goto failure;
                }
                break;
            case TOKEN_IDENTIFIER:
                n.state = state = 15;
                n.m0 = t.identifier;
                push_state(&stack, n);
                consume_token(ctx);
                break;
            default:
                goto failure;
            }
            break;
        }
        case 6: {
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 1 && t.literal[0] == ')') {
                    n.state = state = 7;
                    push_state(&stack, n);
                    consume_token(ctx);
                } else {
                    goto failure;
                }
                break;
            default:
                goto failure;
            }
            break;
        }
        case 7: {
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 2 && t.literal[0] == '-' && t.literal[1] == '>') {
                    n.state = state = 10;
                    push_state(&stack, n);
                    consume_token(ctx);
                } else {
                    goto failure;
                }
                break;
            case TOKEN_STATEMENTS:
                n.state = state = reduce_table[state * 10 + 4];
                push_state(&stack, n);
                break;
            default:
                goto failure;
            }
            break;
        }
        case 8: {
            switch (t.id) {
            case TOKEN_STATEMENTS:
                n.state = state = 9;
                n.m1 = t.statements;
                push_state(&stack, n);
                consume_token(ctx);
                break;
            default:
                goto failure;
            }
            break;
        }
        case 9: {
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 2 && t.literal[0] == 'f' && t.literal[1] == 'n') {
                    stack.size -= 7;
                    state = stack.b[stack.size - 1].state;
                    n.state = state = reduce_table[state * 10 + 1];
                    stack.b[stack.size] = n;
                    ++stack.size;
                } else {
                    goto failure;
                }
                break;
            case TOKEN_END:
                stack.size -= 7;
                state = stack.b[stack.size - 1].state;
                n.state = state = reduce_table[state * 10 + 1];
                stack.b[stack.size] = n;
                ++stack.size;
                break;
            default:
                goto failure;
            }
            break;
        }
        case 10: {
            switch (t.id) {
            case TOKEN_IDENTIFIER:
                n.state = state = 11;
                n.m0 = t.identifier;
                push_state(&stack, n);
                consume_token(ctx);
                break;
            default:
                goto failure;
            }
            break;
        }
        case 11: {
            switch (t.id) {
            case TOKEN_STATEMENTS:
                stack.size -= 2;
                state = stack.b[stack.size - 1].state;
                n.state = state = reduce_table[state * 10 + 5];
                stack.b[stack.size] = n;
                ++stack.size;
                break;
            default:
                goto failure;
            }
            break;
        }
        case 12: {
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 1 && t.literal[0] == ',') {
                    n.state = state = 13;
                    push_state(&stack, n);
                    consume_token(ctx);
                } else if (len == 1 && t.literal[0] == ')') {
                    stack.size -= 1;
                    state = stack.b[stack.size - 1].state;
                    n.state = state = reduce_table[state * 10 + 7];
                    stack.b[stack.size] = n;
                    ++stack.size;
                } else {
                    goto failure;
                }
                break;
            default:
                goto failure;
            }
            break;
        }
        case 13: {
            switch (t.id) {
            case TOKEN_IDENTIFIER:
                n.state = state = 15;
                n.m0 = t.identifier;
                push_state(&stack, n);
                consume_token(ctx);
                break;
            default:
                goto failure;
            }
            break;
        }
        case 14: {
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 1 && t.literal[0] == ')') {
                    stack.size -= 3;
                    state = stack.b[stack.size - 1].state;
                    n.state = state = reduce_table[state * 10 + 9];
                    stack.b[stack.size] = n;
                    ++stack.size;
                } else if (len == 1 && t.literal[0] == ',') {
                    stack.size -= 3;
                    state = stack.b[stack.size - 1].state;
                    n.state = state = reduce_table[state * 10 + 9];
                    stack.b[stack.size] = n;
                    ++stack.size;
                } else {
                    goto failure;
                }
                break;
            default:
                goto failure;
            }
            break;
        }
        case 15: {
            switch (t.id) {
            case TOKEN_IDENTIFIER:
                n.state = state = 16;
                n.m0 = t.identifier;
                push_state(&stack, n);
                consume_token(ctx);
                break;
            default:
                goto failure;
            }
            break;
        }
        case 16: {
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 1 && t.literal[0] == ')') {
                    stack.size -= 2;
                    state = stack.b[stack.size - 1].state;
                    n.state = state = reduce_table[state * 10 + 8];
                    stack.b[stack.size] = n;
                    ++stack.size;
                } else if (len == 1 && t.literal[0] == ',') {
                    stack.size -= 2;
                    state = stack.b[stack.size - 1].state;
                    n.state = state = reduce_table[state * 10 + 8];
                    stack.b[stack.size] = n;
                    ++stack.size;
                } else {
                    goto failure;
                }
                break;
            default:
                goto failure;
            }
            break;
        }
        case 17: {
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 1 && t.literal[0] == ')') {
                    stack.size -= 1;
                    state = stack.b[stack.size - 1].state;
                    n.state = state = reduce_table[state * 10 + 6];
                    stack.b[stack.size] = n;
                    ++stack.size;
                } else if (len == 1 && t.literal[0] == ',') {
                    stack.size -= 1;
                    state = stack.b[stack.size - 1].state;
                    n.state = state = reduce_table[state * 10 + 6];
                    stack.b[stack.size] = n;
                    ++stack.size;
                } else {
                    goto failure;
                }
                break;
            default:
                goto failure;
            }
            break;
        }
        default:
            goto failure;
        }
    }
failure:
    __debugbreak();
}

#include "glob.h"

int main() {
    String s;
    read_text_file(&s, L"src\\compiler\\program.txt");

    struct Tokenizer t;
    t.s = s;
    t.start = 0;
    t.end = 0;
    consume_token(&t);

    parse(&t);
}
