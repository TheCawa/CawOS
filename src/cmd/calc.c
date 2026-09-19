#include "commands.h"
#include "libc/util.h"
#include "drivers/screen.h"

static float last_result = 0.0f;
static int calc_error = 0;
static char calc_err_msg[48];

static void calc_fail(const char* msg) {
    if (!calc_error) {
        calc_error = 1;
        safe_strcpy(calc_err_msg, msg, sizeof(calc_err_msg));
    }
}

typedef enum { T_NUM, T_OP, T_IDENT, T_END } tok_type_t;
typedef struct {
    tok_type_t type;
    float num;
    char op[3];
    char ident[16];
} token_t;

static const char* calc_pos;
static token_t cur_tok;

static token_t next_token(void) {
    token_t t;
    memset(&t, 0, sizeof(t));
    while (*calc_pos == ' ') calc_pos++;
    if (*calc_pos == '\0') { t.type = T_END; return t; }

    char c = *calc_pos;

    if (c >= '0' && c <= '9') {
        if (c == '0' && (calc_pos[1] == 'x' || calc_pos[1] == 'X')) {
            uint32_t v = 0;
            int digits = 0;
            calc_pos += 2;
            while (1) {
                char h = *calc_pos;
                int d;
                if (h >= '0' && h <= '9') d = h - '0';
                else if (h >= 'a' && h <= 'f') d = h - 'a' + 10;
                else if (h >= 'A' && h <= 'F') d = h - 'A' + 10;
                else break;
                v = v * 16 + (uint32_t)d;
                digits++;
                calc_pos++;
            }
            if (digits == 0) calc_fail("Bad hex number");
            t.type = T_NUM;
            t.num = (float)(int32_t)v;
            return t;
        }
        if (c == '0' && (calc_pos[1] == 'b' || calc_pos[1] == 'B')) {
            uint32_t v = 0;
            int digits = 0;
            calc_pos += 2;
            while (*calc_pos == '0' || *calc_pos == '1') {
                v = v * 2 + (uint32_t)(*calc_pos - '0');
                digits++;
                calc_pos++;
            }
            if (digits == 0) calc_fail("Bad binary number");
            t.type = T_NUM;
            t.num = (float)(int32_t)v;
            return t;
        }
        char buf[32];
        int i = 0;
        while ((*calc_pos >= '0' && *calc_pos <= '9') || *calc_pos == '.') {
            if (i < 31) buf[i++] = *calc_pos;
            calc_pos++;
        }
        buf[i] = '\0';
        t.type = T_NUM;
        t.num = atof(buf);
        return t;
    }

    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        int i = 0;
        while ((calc_pos[i] >= 'a' && calc_pos[i] <= 'z') ||
               (calc_pos[i] >= 'A' && calc_pos[i] <= 'Z') ||
               (calc_pos[i] >= '0' && calc_pos[i] <= '9')) {
            if (i < 15) t.ident[i] = calc_pos[i];
            i++;
        }
        calc_pos += i;
        t.type = T_IDENT;
        return t;
    }

    if (c == '<' && calc_pos[1] == '<') { t.type = T_OP; strcpy(t.op, "<<"); calc_pos += 2; return t; }
    if (c == '>' && calc_pos[1] == '>') { t.type = T_OP; strcpy(t.op, ">>"); calc_pos += 2; return t; }
    if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
        c == '&' || c == '|' || c == '^' || c == '~' || c == '(' || c == ')') {
        t.type = T_OP;
        t.op[0] = c;
        calc_pos++;
        return t;
    }

    calc_fail("Unexpected symbol");
    t.type = T_END;
    return t;
}

static void advance(void) { cur_tok = next_token(); }

static float parse_bitor(void);

static float parse_primary(void) {
    if (calc_error) return 0;
    if (cur_tok.type == T_NUM) {
        float v = cur_tok.num;
        advance();
        return v;
    }
    if (cur_tok.type == T_OP && strcmp(cur_tok.op, "(") == 0) {
        advance();
        float v = parse_bitor();
        if (calc_error) return 0;
        if (!(cur_tok.type == T_OP && strcmp(cur_tok.op, ")") == 0)) {
            calc_fail("Expected ')'");
            return 0;
        }
        advance();
        return v;
    }
    if (cur_tok.type == T_IDENT) {
        char name[16];
        strcpy(name, cur_tok.ident);
        advance();
        if (strcasecmp(name, "ans") == 0) return last_result;
        if (strcasecmp(name, "sqrt") != 0 && strcasecmp(name, "abs") != 0) {
            calc_fail("Unknown name");
            return 0;
        }
        if (!(cur_tok.type == T_OP && strcmp(cur_tok.op, "(") == 0)) {
            calc_fail("Expected '(' after function");
            return 0;
        }
        advance();
        float arg = parse_bitor();
        if (calc_error) return 0;
        if (!(cur_tok.type == T_OP && strcmp(cur_tok.op, ")") == 0)) {
            calc_fail("Expected ')'");
            return 0;
        }
        advance();
        if (strcasecmp(name, "abs") == 0) return (arg < 0) ? -arg : arg;
        if (arg < 0) { calc_fail("sqrt of negative"); return 0; }
        if (arg == 0) return 0;
        float x = arg;
        for (int i = 0; i < 40; i++) x = 0.5f * (x + arg / x);  /* Ньютон, libm нет */
        return x;
    }
    calc_fail("Unexpected token");
    return 0;
}

static float parse_unary(void) {
    if (calc_error) return 0;
    if (cur_tok.type == T_OP && strcmp(cur_tok.op, "-") == 0) {
        advance();
        return -parse_unary();
    }
    if (cur_tok.type == T_OP && strcmp(cur_tok.op, "+") == 0) {
        advance();
        return parse_unary();
    }
    if (cur_tok.type == T_OP && strcmp(cur_tok.op, "~") == 0) {
        advance();
        return (float)(~(int32_t)parse_unary());
    }
    return parse_primary();
}

static float parse_muldiv(void) {
    float v = parse_unary();
    while (!calc_error && cur_tok.type == T_OP &&
           (strcmp(cur_tok.op, "*") == 0 || strcmp(cur_tok.op, "/") == 0 ||
            strcmp(cur_tok.op, "%") == 0)) {
        char op0 = cur_tok.op[0];
        advance();
        float r = parse_unary();
        if (op0 == '*') v = v * r;
        else if (op0 == '/') {
            if (r == 0.0f) { calc_fail("Div by zero!"); return 0; }
            v = v / r;
        } else {
            int32_t ri = (int32_t)r;
            if (ri == 0) { calc_fail("Mod by zero!"); return 0; }
            v = (float)((int32_t)v % ri);
        }
    }
    return v;
}

static float parse_addsub(void) {
    float v = parse_muldiv();
    while (!calc_error && cur_tok.type == T_OP &&
           (strcmp(cur_tok.op, "+") == 0 || strcmp(cur_tok.op, "-") == 0)) {
        char op0 = cur_tok.op[0];
        advance();
        float r = parse_muldiv();
        v = (op0 == '+') ? v + r : v - r;
    }
    return v;
}

static float parse_shift(void) {
    float v = parse_addsub();
    while (!calc_error && cur_tok.type == T_OP &&
           (strcmp(cur_tok.op, "<<") == 0 || strcmp(cur_tok.op, ">>") == 0)) {
        char op0 = cur_tok.op[0];
        advance();
        float r = parse_addsub();
        int32_t rv = (int32_t)r;
        if (rv < 0 || rv > 31) { calc_fail("Bad shift amount"); return 0; }
        v = (op0 == '<') ? (float)((int32_t)v << rv) : (float)((int32_t)v >> rv);
    }
    return v;
}

static float parse_bitand(void) {
    float v = parse_shift();
    while (!calc_error && cur_tok.type == T_OP && strcmp(cur_tok.op, "&") == 0) {
        advance();
        float r = parse_shift();
        v = (float)((int32_t)v & (int32_t)r);
    }
    return v;
}

static float parse_bitxor(void) {
    float v = parse_bitand();
    while (!calc_error && cur_tok.type == T_OP && strcmp(cur_tok.op, "^") == 0) {
        advance();
        float r = parse_bitand();
        v = (float)((int32_t)v ^ (int32_t)r);
    }
    return v;
}

static float parse_bitor(void) {
    float v = parse_bitxor();
    while (!calc_error && cur_tok.type == T_OP && strcmp(cur_tok.op, "|") == 0) {
        advance();
        float r = parse_bitxor();
        v = (float)((int32_t)v | (int32_t)r);
    }
    return v;
}

void cmd_calc(char* args, int* row) {
    if (args == 0 || args[0] == '\0') {
        print_line_scroll("Usage: calc <expression>", 0, row, 0x0E);
        print_line_scroll("Example: calc 2 + 3 * 4 | calc (2+3)*4 | calc 0xFF & 0x0F", 0, row, 0x07);
        print_line_scroll("Ops: + - * / % & | ^ ~ << >> | Funcs: sqrt() abs() | ans", 0, row, 0x07);
        return;
    }

    calc_error = 0;
    calc_err_msg[0] = '\0';
    calc_pos = args;
    advance();

    float result = parse_bitor();

    if (!calc_error && cur_tok.type != T_END) {
        calc_fail("Unexpected token");
    }

    if (calc_error) {
        char line[64];
        memset(line, 0, 64);
        strcpy(line, "Error: ");
        strcat(line, calc_err_msg);
        print_line_scroll(line, 0, row, 0x0C);
        return;
    }

    last_result = result;

    char res_str[80];
    char num[32];
    memset(res_str, 0, 80);
    strcpy(res_str, "Result: ");
    int32_t iv = (int32_t)result;
    if ((float)iv == result) {
        itoa(iv, num);
        strcat(res_str, num);
        strcat(res_str, " (0x");
        char hx[12];
        itoa_hex((uint32_t)iv, hx);
        strcat(res_str, hx);
        strcat(res_str, ")");
    } else {
        ftoa(result, num, 6);
        strcat(res_str, num);
    }
    print_line_scroll(res_str, 0, row, 0x0A);
}
REGISTER_COMMAND("calc", cmd_calc, 1);