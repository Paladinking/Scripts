#include "autocmp-lang.h"
#include "mem.h"

struct StackEntry {
    int32_t state;
    uint64_t start;
    uint64_t end;
    union {
        char* m1;
        DynamicMatch* m7;
        Command* m10;
        ParenList* m12;
        int32_t m13;
        Line m16;
    };
};

struct Stack {
    struct StackEntry* b;
    uint32_t size;
    uint32_t cap;
};

const static uint8_t reduce_table[406] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 1, 1, 1, 1,
    18, 255, 21, 26, 25, 21, 255, 18, 26, 26, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 14, 255, 255, 255, 255, 14, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 21, 27, 255, 21, 255, 255, 27, 27, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 28, 255, 255, 28, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
};

const static Token state0_expected[] = {{ TOKEN_STRING }, { TOKEN_NEWLINE }, { TOKEN_LITERAL_OPENPAREN, "(" }, { TOKEN_LITERAL_PIPE, "|" }, { TOKEN_END }};
const static Token state1_expected[] = {{ TOKEN_LITERAL_PIPE, "|" }, { TOKEN_STRING }, { TOKEN_NEWLINE }, { TOKEN_LITERAL_OPENPAREN, "(" }};
const static Token state2_expected[] = {{ TOKEN_STRING }, { TOKEN_NEWLINE }, { TOKEN_LITERAL_OPENPAREN, "(" }, { TOKEN_LITERAL_PIPE, "|" }};
const static Token state3_expected[] = {{ TOKEN_LITERAL_LT, "<" }, { TOKEN_NEWLINE }, { TOKEN_LITERAL_GT, ">" }};
const static Token state4_expected[] = {{ TOKEN_STRING }};
const static Token state5_expected[] = {{ TOKEN_LITERAL_LT, "<" }};
const static Token state6_expected[] = {{ TOKEN_STRING }};
const static Token state7_expected[] = {{ TOKEN_LITERAL_PIPE, "|" }};
const static Token state8_expected[] = {{ TOKEN_STRING }};
const static Token state9_expected[] = {{ TOKEN_NEWLINE }};
const static Token state10_expected[] = {{ TOKEN_STRING }, { TOKEN_NEWLINE }, { TOKEN_LITERAL_OPENPAREN, "(" }, { TOKEN_LITERAL_PIPE, "|" }, { TOKEN_END }};
const static Token state11_expected[] = {{ TOKEN_STRING }, { TOKEN_NEWLINE }, { TOKEN_LITERAL_OPENPAREN, "(" }, { TOKEN_LITERAL_PIPE, "|" }, { TOKEN_END }};
const static Token state12_expected[] = {{ TOKEN_STRING }};
const static Token state13_expected[] = {{ TOKEN_LITERAL_CLOSEPAREN, ")" }, { TOKEN_LITERAL_PIPE, "|" }};
const static Token state14_expected[] = {{ TOKEN_LITERAL_CLOSEPAREN, ")" }, { TOKEN_LITERAL_PIPE, "|" }};
const static Token state15_expected[] = {{ TOKEN_NEWLINE }, { TOKEN_LITERAL_GT, ">" }};
const static Token state16_expected[] = {{ TOKEN_STRING }};
const static Token state17_expected[] = {{ TOKEN_LITERAL_CLOSEPAREN, ")" }, { TOKEN_LITERAL_PIPE, "|" }};
const static Token state18_expected[] = {{ TOKEN_STRING }, { TOKEN_NEWLINE }, { TOKEN_LITERAL_OPENPAREN, "(" }, { TOKEN_LITERAL_PIPE, "|" }};
const static Token state19_expected[] = {{ TOKEN_NEWLINE }, { TOKEN_LITERAL_GT, ">" }};
const static Token state20_expected[] = {{ TOKEN_STRING }, { TOKEN_NEWLINE }, { TOKEN_LITERAL_OPENPAREN, "(" }, { TOKEN_LITERAL_PIPE, "|" }};
const static Token state21_expected[] = {{ TOKEN_NEWLINE }, { TOKEN_LITERAL_GT, ">" }};
const static Token state22_expected[] = {{ TOKEN_STRING }, { TOKEN_NEWLINE }, { TOKEN_LITERAL_OPENPAREN, "(" }, { TOKEN_LITERAL_PIPE, "|" }, { TOKEN_END }};
const static Token state23_expected[] = {{ TOKEN_STRING }, { TOKEN_LITERAL_OPENPAREN, "(" }};
const static Token state24_expected[] = {{ TOKEN_STRING }, { TOKEN_NEWLINE }, { TOKEN_LITERAL_OPENPAREN, "(" }, { TOKEN_LITERAL_PIPE, "|" }, { TOKEN_END }};
const static Token state25_expected[] = {{ TOKEN_STRING }, { TOKEN_NEWLINE }, { TOKEN_LITERAL_OPENPAREN, "(" }, { TOKEN_LITERAL_PIPE, "|" }, { TOKEN_END }};
const static Token state26_expected[] = {{ TOKEN_STRING }, { TOKEN_NEWLINE }, { TOKEN_LITERAL_OPENPAREN, "(" }, { TOKEN_LITERAL_PIPE, "|" }, { TOKEN_END }};
const static Token state27_expected[] = {{ TOKEN_STRING }, { TOKEN_NEWLINE }, { TOKEN_LITERAL_OPENPAREN, "(" }, { TOKEN_LITERAL_PIPE, "|" }, { TOKEN_END }};
const static Token state28_expected[] = {{ TOKEN_NEWLINE }};
static const Token* expected[] = {state0_expected, state1_expected, state2_expected, state3_expected, state4_expected, state5_expected, state6_expected, state7_expected, state8_expected, state9_expected, state10_expected, state11_expected, state12_expected, state13_expected, state14_expected, state15_expected, state16_expected, state17_expected, state18_expected, state19_expected, state20_expected, state21_expected, state22_expected, state23_expected, state24_expected, state25_expected, state26_expected, state27_expected, state28_expected};
static const uint32_t expected_count[] = {5, 4, 4, 3, 1, 1, 1, 1, 1, 1, 5, 5, 1, 2, 2, 2, 1, 2, 4, 2, 4, 2, 5, 2, 5, 5, 5, 5, 1};

static SyntaxError get_syntax_error(int32_t state, Token t, uint64_t start, uint64_t end) {
    SyntaxError e = {t, start, end, NULL, 0};
    if (state < 0 || state >= 29) {
        return e;
    }
    e.expected = expected[state];
    e.expected_count = expected_count[state];
    return e;
}

Token literal_token(const char* s, uint64_t len, uint64_t* ix) {
    Token t;
    t.literal = s;
    if (*ix >= len) {
        t.id = TOKEN_UNKOWN;
        return t;
    }
    char c = s[(*ix)++];
    switch (c) {
    case '(':
        t.id = TOKEN_LITERAL_OPENPAREN;
        return t;
    case ')':
        t.id = TOKEN_LITERAL_CLOSEPAREN;
        return t;
    case '|':
        t.id = TOKEN_LITERAL_PIPE;
        return t;
    case '>':
        t.id = TOKEN_LITERAL_GT;
        return t;
    case '<':
        t.id = TOKEN_LITERAL_LT;
        return t;
    default:
        t.id = TOKEN_UNKOWN;
        return t;
    }
}
static void push_state(struct Stack* stack, struct StackEntry n) {
    if (stack->size == stack->cap) {
        struct StackEntry* p = (struct StackEntry*)Mem_realloc(stack->b,
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
    stack.b = (struct StackEntry*)Mem_alloc(16 * sizeof(struct StackEntry));
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
            case TOKEN_STRING:
            case TOKEN_NEWLINE:
            case TOKEN_LITERAL_OPENPAREN:
            case TOKEN_LITERAL_PIPE:
                n.start = start;
                n.end = start;
                n.state = reduce_table[n.state * 14 + 10];
                push_state(&stack, n);
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
            case TOKEN_LITERAL_PIPE:
                n.state = 2;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            case TOKEN_STRING:
                n.m1 = t.string;
                n.state = 3;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            case TOKEN_NEWLINE:
                n.state = 11;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            case TOKEN_LITERAL_OPENPAREN:
                n.state = 12;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            default:
                goto failure;
            }
            break;
        case 2: 
            switch (t.id) {
            case TOKEN_STRING:
            case TOKEN_NEWLINE:
            case TOKEN_LITERAL_OPENPAREN:
            case TOKEN_LITERAL_PIPE:
                n.start = stack.b[stack.size - 1].start;
                n.end = stack.b[stack.size - 1].end;
                n.m13 = 1;
                stack.size -= 1;
                n.state = stack.b[stack.size - 1].state;
                n.state = reduce_table[n.state * 14 + 0];
                stack.b[stack.size] = n;
                ++stack.size;
                break;
            default:
                goto failure;
            }
            break;
        case 3: 
            switch (t.id) {
            case TOKEN_LITERAL_LT:
                n.state = 4;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            case TOKEN_NEWLINE:
            case TOKEN_LITERAL_GT:
                n.start = stack.b[stack.size - 1].start;
                n.end = stack.b[stack.size - 1].end;
                n.m10 = OnCommand(ctx, n.start, n.end, stack.b[stack.size - 1].m1);
                stack.size -= 1;
                n.state = stack.b[stack.size - 1].state;
                n.state = reduce_table[n.state * 14 + 2];
                stack.b[stack.size] = n;
                ++stack.size;
                break;
            default:
                goto failure;
            }
            break;
        case 4: 
            switch (t.id) {
            case TOKEN_STRING:
                n.m1 = t.string;
                n.state = 5;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            default:
                goto failure;
            }
            break;
        case 5: 
            switch (t.id) {
            case TOKEN_LITERAL_LT:
                n.state = 6;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            default:
                goto failure;
            }
            break;
        case 6: 
            switch (t.id) {
            case TOKEN_STRING:
                n.m1 = t.string;
                n.state = 7;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            default:
                goto failure;
            }
            break;
        case 7: 
            switch (t.id) {
            case TOKEN_LITERAL_PIPE:
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
            case TOKEN_STRING:
                n.m1 = t.string;
                n.state = 9;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            default:
                goto failure;
            }
            break;
        case 9: 
            switch (t.id) {
            case TOKEN_NEWLINE:
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
            case TOKEN_STRING:
            case TOKEN_NEWLINE:
            case TOKEN_LITERAL_OPENPAREN:
            case TOKEN_LITERAL_PIPE:
            case TOKEN_END:
                n.start = stack.b[stack.size - 8].start;
                n.end = stack.b[stack.size - 1].end;
                n.m7 = OnDynamic(ctx, n.start, n.end, stack.b[stack.size - 8].m1, stack.b[stack.size - 6].m1, stack.b[stack.size - 4].m1, stack.b[stack.size - 2].m1);
                stack.size -= 8;
                n.state = stack.b[stack.size - 1].state;
                n.state = reduce_table[n.state * 14 + 4];
                stack.b[stack.size] = n;
                ++stack.size;
                break;
            default:
                goto failure;
            }
            break;
        case 11: 
            switch (t.id) {
            case TOKEN_STRING:
            case TOKEN_NEWLINE:
            case TOKEN_LITERAL_OPENPAREN:
            case TOKEN_LITERAL_PIPE:
            case TOKEN_END:
                n.start = stack.b[stack.size - 1].start;
                n.end = stack.b[stack.size - 1].end;
                n.m16 = (Line){.command = NULL, .loop = NULL};
                stack.size -= 1;
                n.state = stack.b[stack.size - 1].state;
                n.state = reduce_table[n.state * 14 + 3];
                stack.b[stack.size] = n;
                ++stack.size;
                break;
            default:
                goto failure;
            }
            break;
        case 12: 
            switch (t.id) {
            case TOKEN_STRING:
                n.m1 = t.string;
                n.state = 13;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            default:
                goto failure;
            }
            break;
        case 13: 
            switch (t.id) {
            case TOKEN_LITERAL_CLOSEPAREN:
            case TOKEN_LITERAL_PIPE:
                n.start = stack.b[stack.size - 1].start;
                n.end = stack.b[stack.size - 1].end;
                n.m12 = OnParenList(ctx, n.start, n.end, stack.b[stack.size - 1].m1);
                stack.size -= 1;
                n.state = stack.b[stack.size - 1].state;
                n.state = reduce_table[n.state * 14 + 1];
                stack.b[stack.size] = n;
                ++stack.size;
                break;
            default:
                goto failure;
            }
            break;
        case 14: 
            switch (t.id) {
            case TOKEN_LITERAL_CLOSEPAREN:
                n.state = 15;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            case TOKEN_LITERAL_PIPE:
                n.state = 16;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            default:
                goto failure;
            }
            break;
        case 15: 
            switch (t.id) {
            case TOKEN_NEWLINE:
            case TOKEN_LITERAL_GT:
                n.start = stack.b[stack.size - 3].start;
                n.end = stack.b[stack.size - 1].end;
                n.m10 = OnListCommand(ctx, n.start, n.end, stack.b[stack.size - 2].m12);
                stack.size -= 3;
                n.state = stack.b[stack.size - 1].state;
                n.state = reduce_table[n.state * 14 + 5];
                stack.b[stack.size] = n;
                ++stack.size;
                break;
            default:
                goto failure;
            }
            break;
        case 16: 
            switch (t.id) {
            case TOKEN_STRING:
                n.m1 = t.string;
                n.state = 17;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            default:
                goto failure;
            }
            break;
        case 17: 
            switch (t.id) {
            case TOKEN_LITERAL_CLOSEPAREN:
            case TOKEN_LITERAL_PIPE:
                n.start = stack.b[stack.size - 3].start;
                n.end = stack.b[stack.size - 1].end;
                n.m12 = OnAddParenList(ctx, n.start, n.end, stack.b[stack.size - 3].m12, stack.b[stack.size - 1].m1);
                stack.size -= 3;
                n.state = stack.b[stack.size - 1].state;
                n.state = reduce_table[n.state * 14 + 6];
                stack.b[stack.size] = n;
                ++stack.size;
                break;
            default:
                goto failure;
            }
            break;
        case 18: 
            switch (t.id) {
            case TOKEN_STRING:
                n.m1 = t.string;
                n.state = 19;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            case TOKEN_NEWLINE:
                n.state = 11;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            case TOKEN_LITERAL_OPENPAREN:
                n.state = 12;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            case TOKEN_LITERAL_PIPE:
                n.state = 20;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            default:
                goto failure;
            }
            break;
        case 19: 
            switch (t.id) {
            case TOKEN_NEWLINE:
            case TOKEN_LITERAL_GT:
                n.start = stack.b[stack.size - 1].start;
                n.end = stack.b[stack.size - 1].end;
                n.m10 = OnCommand(ctx, n.start, n.end, stack.b[stack.size - 1].m1);
                stack.size -= 1;
                n.state = stack.b[stack.size - 1].state;
                n.state = reduce_table[n.state * 14 + 2];
                stack.b[stack.size] = n;
                ++stack.size;
                break;
            default:
                goto failure;
            }
            break;
        case 20: 
            switch (t.id) {
            case TOKEN_STRING:
            case TOKEN_NEWLINE:
            case TOKEN_LITERAL_OPENPAREN:
            case TOKEN_LITERAL_PIPE:
                n.start = stack.b[stack.size - 2].start;
                n.end = stack.b[stack.size - 1].end;
                n.m13 = stack.b[stack.size - 2].m13 + 1;
                stack.size -= 2;
                n.state = stack.b[stack.size - 1].state;
                n.state = reduce_table[n.state * 14 + 7];
                stack.b[stack.size] = n;
                ++stack.size;
                break;
            default:
                goto failure;
            }
            break;
        case 21: 
            switch (t.id) {
            case TOKEN_NEWLINE:
                n.state = 22;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            case TOKEN_LITERAL_GT:
                n.state = 23;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            default:
                goto failure;
            }
            break;
        case 22: 
            switch (t.id) {
            case TOKEN_STRING:
            case TOKEN_NEWLINE:
            case TOKEN_LITERAL_OPENPAREN:
            case TOKEN_LITERAL_PIPE:
            case TOKEN_END:
                n.start = stack.b[stack.size - 2].start;
                n.end = stack.b[stack.size - 1].end;
                n.m16 = OnLine(ctx, n.start, n.end, stack.b[stack.size - 2].m10);
                stack.size -= 2;
                n.state = stack.b[stack.size - 1].state;
                n.state = reduce_table[n.state * 14 + 8];
                stack.b[stack.size] = n;
                ++stack.size;
                break;
            default:
                goto failure;
            }
            break;
        case 23: 
            switch (t.id) {
            case TOKEN_STRING:
                n.m1 = t.string;
                n.state = 19;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            case TOKEN_LITERAL_OPENPAREN:
                n.state = 12;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
            default:
                goto failure;
            }
            break;
        case 24: 
            switch (t.id) {
            case TOKEN_STRING:
            case TOKEN_NEWLINE:
            case TOKEN_LITERAL_OPENPAREN:
            case TOKEN_LITERAL_PIPE:
            case TOKEN_END:
                n.start = stack.b[stack.size - 4].start;
                n.end = stack.b[stack.size - 1].end;
                n.m16 = OnLineWithLoop(ctx, n.start, n.end, stack.b[stack.size - 4].m10, stack.b[stack.size - 2].m10);
                stack.size -= 4;
                n.state = stack.b[stack.size - 1].state;
                n.state = reduce_table[n.state * 14 + 9];
                stack.b[stack.size] = n;
                ++stack.size;
                break;
            default:
                goto failure;
            }
            break;
        case 25: 
            switch (t.id) {
            case TOKEN_STRING:
            case TOKEN_NEWLINE:
            case TOKEN_LITERAL_OPENPAREN:
            case TOKEN_LITERAL_PIPE:
                n.start = stack.b[stack.size - 2].start;
                n.end = stack.b[stack.size - 1].end;
                OnProgramDynamic(ctx, n.start, n.end, stack.b[stack.size - 1].m7);
                stack.size -= 2;
                n.state = stack.b[stack.size - 1].state;
                n.state = reduce_table[n.state * 14 + 13];
                stack.b[stack.size] = n;
                ++stack.size;
                break;
            case TOKEN_END:
                n.start = stack.b[stack.size - 2].start;
                n.end = stack.b[stack.size - 1].end;
                OnProgramDynamic(ctx, n.start, n.end, stack.b[stack.size - 1].m7);
                stack.size -= 2;
                n.state = stack.b[stack.size - 1].state;
                goto accept;
            default:
                goto failure;
            }
            break;
        case 26: 
            switch (t.id) {
            case TOKEN_STRING:
            case TOKEN_NEWLINE:
            case TOKEN_LITERAL_OPENPAREN:
            case TOKEN_LITERAL_PIPE:
                n.start = stack.b[stack.size - 2].start;
                n.end = stack.b[stack.size - 1].end;
                OnProgramLine(ctx, n.start, n.end, stack.b[stack.size - 1].m16);
                stack.size -= 2;
                n.state = stack.b[stack.size - 1].state;
                n.state = reduce_table[n.state * 14 + 11];
                stack.b[stack.size] = n;
                ++stack.size;
                break;
            case TOKEN_END:
                n.start = stack.b[stack.size - 2].start;
                n.end = stack.b[stack.size - 1].end;
                OnProgramLine(ctx, n.start, n.end, stack.b[stack.size - 1].m16);
                stack.size -= 2;
                n.state = stack.b[stack.size - 1].state;
                goto accept;
            default:
                goto failure;
            }
            break;
        case 27: 
            switch (t.id) {
            case TOKEN_STRING:
            case TOKEN_NEWLINE:
            case TOKEN_LITERAL_OPENPAREN:
            case TOKEN_LITERAL_PIPE:
                n.start = stack.b[stack.size - 3].start;
                n.end = stack.b[stack.size - 1].end;
                OnProgramLineWithPrefix(ctx, n.start, n.end, stack.b[stack.size - 2].m13, stack.b[stack.size - 1].m16);
                stack.size -= 3;
                n.state = stack.b[stack.size - 1].state;
                n.state = reduce_table[n.state * 14 + 12];
                stack.b[stack.size] = n;
                ++stack.size;
                break;
            case TOKEN_END:
                n.start = stack.b[stack.size - 3].start;
                n.end = stack.b[stack.size - 1].end;
                OnProgramLineWithPrefix(ctx, n.start, n.end, stack.b[stack.size - 2].m13, stack.b[stack.size - 1].m16);
                stack.size -= 3;
                n.state = stack.b[stack.size - 1].state;
                goto accept;
            default:
                goto failure;
            }
            break;
        case 28: 
            switch (t.id) {
            case TOKEN_NEWLINE:
                n.state = 24;
                consume_token(ctx, &n.start, &n.end);
                push_state(&stack, n);
                break;
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
