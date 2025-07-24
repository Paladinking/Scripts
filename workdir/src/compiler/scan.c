#include "scan.h"
#include "mem.h"

struct StackEntry {
    int32_t state;
    uint64_t start;
    uint64_t end;
    union {
        int64_t m0;
        StrWithLength m1;
        int64_t m2;
        FunctionDef* m3;
        int64_t m8;
        int64_t m14;
        uint64_t m15;
        int64_t m16;
        uint64_t m18;
    };
};

struct Stack {
    struct StackEntry* b;
    uint32_t size;
    uint32_t cap;
};

const static uint8_t reduce_table[220] = {
    -1, -1, -1, 1, 1, 1, -1, -1, -1, -1, -1,
    17, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 12, 5, -1, -1, -1, -1, -1, 5, 11, 12,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, 7, 7, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    19, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

const static Token state0_expected[] = {{ TOKEN_LITERAL, .literal = "fn" }, { TOKEN_END}};
const static Token state1_expected[] = {{ TOKEN_LITERAL, .literal = "fn" }};
const static Token state2_expected[] = {{ TOKEN_IDENTIFIER}};
const static Token state3_expected[] = {{ TOKEN_LITERAL, .literal = "(" }};
const static Token state4_expected[] = {{ TOKEN_IDENTIFIER}, { TOKEN_LITERAL, .literal = ")" }};
const static Token state5_expected[] = {{ TOKEN_LITERAL, .literal = ")" }};
const static Token state6_expected[] = {{ TOKEN_LITERAL, .literal = "->" }, { TOKEN_STATEMENTS}};
const static Token state7_expected[] = {{ TOKEN_STATEMENTS}};
const static Token state8_expected[] = {{ TOKEN_LITERAL, .literal = "fn" }, { TOKEN_END}};
const static Token state9_expected[] = {{ TOKEN_IDENTIFIER}};
const static Token state10_expected[] = {{ TOKEN_STATEMENTS}};
const static Token state11_expected[] = {{ TOKEN_LITERAL, .literal = ")" }, { TOKEN_LITERAL, .literal = "," }};
const static Token state12_expected[] = {{ TOKEN_LITERAL, .literal = "," }, { TOKEN_LITERAL, .literal = ")" }};
const static Token state13_expected[] = {{ TOKEN_IDENTIFIER}};
const static Token state14_expected[] = {{ TOKEN_IDENTIFIER}};
const static Token state15_expected[] = {{ TOKEN_LITERAL, .literal = ")" }, { TOKEN_LITERAL, .literal = "," }};
const static Token state16_expected[] = {{ TOKEN_LITERAL, .literal = ")" }, { TOKEN_LITERAL, .literal = "," }};
const static Token state17_expected[] = {{ TOKEN_LITERAL, .literal = "fn" }, { TOKEN_END}};
const static Token state18_expected[] = {{ TOKEN_LITERAL, .literal = "fn" }};
const static Token state19_expected[] = {{ TOKEN_LITERAL, .literal = "fn" }, { TOKEN_END}};
static const Token* expected[] = {state0_expected, state1_expected, state2_expected, state3_expected, state4_expected, state5_expected, state6_expected, state7_expected, state8_expected, state9_expected, state10_expected, state11_expected, state12_expected, state13_expected, state14_expected, state15_expected, state16_expected, state17_expected, state18_expected, state19_expected};
static const uint32_t expected_count[] = {2, 1, 1, 1, 2, 1, 2, 1, 2, 1, 1, 2, 2, 1, 1, 2, 2, 2, 1, 2};

static SyntaxError get_syntax_error(int32_t state, Token t, uint64_t start, uint64_t end) {
    SyntaxError e = {t, start, end, NULL, 0};
    if (state < 0 || state > 20) {
        return e;
    }
    e.expected = expected[state];
    e.expected_count = expected_count[state];
    return e;
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

bool parse(void* ctx) {
    struct Stack stack;
    stack.b = Mem_alloc(16 * sizeof(struct StackEntry));
    stack.size = 1;
    stack.b[0].state = 0;
    stack.b[0].start = 0;
    stack.b[0].end = 0;
    stack.cap = 16;
    struct StackEntry n = {0, 0, 0};
    while (1) {
loop:
        uint64_t start, end;
        Token t = peek_token(ctx, &start, &end);
        uint64_t len = end - start;
        switch (n.state) {
        case 0: 
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 2 && t.literal[0] == 'f' && t.literal[1] == 'n') {
                    n.start = start;
                    n.end = start;
                    n.state = reduce_table[n.state * 11 + 3];
                    push_state(&stack, n);
                } else {
                    goto failure;
                }
                break;
            case TOKEN_END:
                n.start = start;
                n.end = start;
                goto accept;
            default:
                goto failure;
            }
            break;
        case 1: 
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 2 && t.literal[0] == 'f' && t.literal[1] == 'n') {
                    n.state = 2;
                    consume_token(ctx, &n.start, &n.end);
                    push_state(&stack, n);
                } else {
                    goto failure;
                }
                break;
            default:
                goto failure;
            }
            break;
        case 2: 
            switch (t.id) {
            case TOKEN_IDENTIFIER:
                n.m1 = t.identifier;
                n.state = 3;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            default:
                goto failure;
            }
            break;
        case 3: 
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 1 && t.literal[0] == '(') {
                    n.state = 4;
                    consume_token(ctx, &n.start, &n.end);
                    push_state(&stack, n);
                } else {
                    goto failure;
                }
                break;
            default:
                goto failure;
            }
            break;
        case 4: 
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 1 && t.literal[0] == ')') {
                    n.start = start;
                    n.end = start;
                    n.m15 = 0;
                    n.state = reduce_table[n.state * 11 + 2];
                    push_state(&stack, n);
                } else {
                    goto failure;
                }
                break;
            case TOKEN_IDENTIFIER:
                n.m1 = t.identifier;
                n.state = 14;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            default:
                goto failure;
            }
            break;
        case 5: 
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 1 && t.literal[0] == ')') {
                    n.state = 6;
                    consume_token(ctx, &n.start, &n.end);
                    push_state(&stack, n);
                } else {
                    goto failure;
                }
                break;
            default:
                goto failure;
            }
            break;
        case 6: 
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 2 && t.literal[0] == '-' && t.literal[1] == '>') {
                    n.state = 9;
                    consume_token(ctx, &n.start, &n.end);
                    push_state(&stack, n);
                } else {
                    goto failure;
                }
                break;
            case TOKEN_STATEMENTS:
                n.start = start;
                n.end = start;
                n.state = reduce_table[n.state * 11 + 6];
                push_state(&stack, n);
                break;
            default:
                goto failure;
            }
            break;
        case 7: 
            switch (t.id) {
            case TOKEN_STATEMENTS:
                n.m2 = t.statements;
                n.state = 8;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            default:
                goto failure;
            }
            break;
        case 8: 
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 2 && t.literal[0] == 'f' && t.literal[1] == 'n') {
                    n.start = stack.b[stack.size - 7].start;
                    n.end = stack.b[stack.size - 1].end;
                    n.m3 = OnScanFunction(ctx, n.start, n.end, stack.b[stack.size - 6].m1, stack.b[stack.size - 4].m15, stack.b[stack.size - 2].m14, stack.b[stack.size - 1].m2);
                    stack.size -= 7;
                    n.state = stack.b[stack.size - 1].state;
                    n.state = reduce_table[n.state * 11 + 0];
                    stack.b[stack.size] = n;
                    ++stack.size;
                } else {
                    goto failure;
                }
                break;
            case TOKEN_END:
                n.start = stack.b[stack.size - 7].start;
                n.end = stack.b[stack.size - 1].end;
                n.m3 = OnScanFunction(ctx, n.start, n.end, stack.b[stack.size - 6].m1, stack.b[stack.size - 4].m15, stack.b[stack.size - 2].m14, stack.b[stack.size - 1].m2);
                stack.size -= 7;
                n.state = stack.b[stack.size - 1].state;
                n.state = reduce_table[n.state * 11 + 0];
                stack.b[stack.size] = n;
                ++stack.size;
                break;
            default:
                goto failure;
            }
            break;
        case 9: 
            switch (t.id) {
            case TOKEN_IDENTIFIER:
                n.m1 = t.identifier;
                n.state = 10;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            default:
                goto failure;
            }
            break;
        case 10: 
            switch (t.id) {
            case TOKEN_STATEMENTS:
                n.start = stack.b[stack.size - 2].start;
                n.end = stack.b[stack.size - 1].end;
                stack.size -= 2;
                n.state = stack.b[stack.size - 1].state;
                n.state = reduce_table[n.state * 11 + 7];
                stack.b[stack.size] = n;
                ++stack.size;
                break;
            default:
                goto failure;
            }
            break;
        case 11: 
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 1 && t.literal[0] == ')') {
                    n.start = stack.b[stack.size - 1].start;
                    n.end = stack.b[stack.size - 1].end;
                    n.m18 = 1;
                    stack.size -= 1;
                    n.state = stack.b[stack.size - 1].state;
                    n.state = reduce_table[n.state * 11 + 1];
                    stack.b[stack.size] = n;
                    ++stack.size;
                } else if (len == 1 && t.literal[0] == ',') {
                    n.start = stack.b[stack.size - 1].start;
                    n.end = stack.b[stack.size - 1].end;
                    n.m18 = 1;
                    stack.size -= 1;
                    n.state = stack.b[stack.size - 1].state;
                    n.state = reduce_table[n.state * 11 + 1];
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
        case 12: 
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 1 && t.literal[0] == ',') {
                    n.state = 13;
                    consume_token(ctx, &n.start, &n.end);
                    push_state(&stack, n);
                } else if (len == 1 && t.literal[0] == ')') {
                    n.start = stack.b[stack.size - 1].start;
                    n.end = stack.b[stack.size - 1].end;
                    n.m15 = stack.b[stack.size - 1].m18;
                    stack.size -= 1;
                    n.state = stack.b[stack.size - 1].state;
                    n.state = reduce_table[n.state * 11 + 8];
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
        case 13: 
            switch (t.id) {
            case TOKEN_IDENTIFIER:
                n.m1 = t.identifier;
                n.state = 14;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            default:
                goto failure;
            }
            break;
        case 14: 
            switch (t.id) {
            case TOKEN_IDENTIFIER:
                n.m1 = t.identifier;
                n.state = 15;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            default:
                goto failure;
            }
            break;
        case 15: 
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 1 && t.literal[0] == ')') {
                    n.start = stack.b[stack.size - 2].start;
                    n.end = stack.b[stack.size - 1].end;
                    stack.size -= 2;
                    n.state = stack.b[stack.size - 1].state;
                    n.state = reduce_table[n.state * 11 + 9];
                    stack.b[stack.size] = n;
                    ++stack.size;
                } else if (len == 1 && t.literal[0] == ',') {
                    n.start = stack.b[stack.size - 2].start;
                    n.end = stack.b[stack.size - 1].end;
                    stack.size -= 2;
                    n.state = stack.b[stack.size - 1].state;
                    n.state = reduce_table[n.state * 11 + 9];
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
        case 16: 
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 1 && t.literal[0] == ')') {
                    n.start = stack.b[stack.size - 3].start;
                    n.end = stack.b[stack.size - 1].end;
                    n.m18 = stack.b[stack.size - 3].m18 + 1;
                    stack.size -= 3;
                    n.state = stack.b[stack.size - 1].state;
                    n.state = reduce_table[n.state * 11 + 10];
                    stack.b[stack.size] = n;
                    ++stack.size;
                } else if (len == 1 && t.literal[0] == ',') {
                    n.start = stack.b[stack.size - 3].start;
                    n.end = stack.b[stack.size - 1].end;
                    n.m18 = stack.b[stack.size - 3].m18 + 1;
                    stack.size -= 3;
                    n.state = stack.b[stack.size - 1].state;
                    n.state = reduce_table[n.state * 11 + 10];
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
        case 17: 
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 2 && t.literal[0] == 'f' && t.literal[1] == 'n') {
                    n.start = stack.b[stack.size - 2].start;
                    n.end = stack.b[stack.size - 1].end;
                    n.m8 = OnScanProgram(ctx, n.start, n.end, stack.b[stack.size - 2].m8, stack.b[stack.size - 1].m3);
                    stack.size -= 2;
                    n.state = stack.b[stack.size - 1].state;
                    n.state = reduce_table[n.state * 11 + 4];
                    stack.b[stack.size] = n;
                    ++stack.size;
                } else {
                    goto failure;
                }
                break;
            case TOKEN_END:
                n.start = stack.b[stack.size - 2].start;
                n.end = stack.b[stack.size - 1].end;
                n.m8 = OnScanProgram(ctx, n.start, n.end, stack.b[stack.size - 2].m8, stack.b[stack.size - 1].m3);
                stack.size -= 2;
                n.state = stack.b[stack.size - 1].state;
                goto accept;
            default:
                goto failure;
            }
            break;
        case 18: 
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 2 && t.literal[0] == 'f' && t.literal[1] == 'n') {
                    n.state = 2;
                    consume_token(ctx, &n.start, &n.end);
                    push_state(&stack, n);
                } else {
                    goto failure;
                }
                break;
            default:
                goto failure;
            }
            break;
        case 19: 
            switch (t.id) {
            case TOKEN_LITERAL:
                if (len == 2 && t.literal[0] == 'f' && t.literal[1] == 'n') {
                    n.start = stack.b[stack.size - 3].start;
                    n.end = stack.b[stack.size - 1].end;
                    stack.size -= 3;
                    n.state = stack.b[stack.size - 1].state;
                    n.state = reduce_table[n.state * 11 + 5];
                    stack.b[stack.size] = n;
                    ++stack.size;
                } else {
                    goto failure;
                }
                break;
            case TOKEN_END:
                n.start = stack.b[stack.size - 3].start;
                n.end = stack.b[stack.size - 1].end;
                stack.size -= 3;
                n.state = stack.b[stack.size - 1].state;
                goto accept;
            default:
                goto failure;
            }
            break;
        default:
            goto failure;
        }
        continue;
    failure:
        parser_error(ctx, get_syntax_error(n.state, t, start, end));
        while(1) {
            uint32_t i = stack.size;
            while (i > 0) {
                --i;
                switch(stack.b[i].state) {
                case 1:
                    if ((t.id == TOKEN_LITERAL && len == 2 && t.literal[0] == 'f' && t.literal[1] == 'n')) {
                        stack.size = i + 1;
                        n.state = 18;
                        push_state(&stack, n);
                        goto loop;
                    }
                default:
                    break;
                }
            }
            if (t.id != TOKEN_END) {
                consume_token(ctx, &n.start, &n.end);
                t = peek_token(ctx, &start, &end);
                len = end - start;
                continue;
            }
            Mem_free(stack.b);
            return false;
        }
    }
accept:
    Mem_free(stack.b);
    return true;
}
