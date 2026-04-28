#include "commands.h"
#include "util.h"
#include "screen.h"

static float f_stack[16];
static int f_sp = 0;

void f_push(float val) {
    if (f_sp < 16) f_stack[f_sp++] = val;
}

float f_pop() {
    if (f_sp > 0) return f_stack[--f_sp];
    return 0.0f;
}

void cmd_calc(char* args, int* row) {
    if (args == 0 || args[0] == '\0') {
        print_line_scroll("Usage: calc <num1> <num2> <op>", 0, row, 0x0E);
        print_line_scroll("Example: calc 5 2 /", 0, row, 0x07);
        return;
    }

    f_sp = 0;
    char* token = args;
    char buf[32];

    while (*token != '\0') {
        if (*token == ' ') {
            token++;
            continue;
        }

        int ti = 0;
        while (*token != ' ' && *token != '\0') {
            if (ti < 31) buf[ti++] = *token++;
            else token++;
        }
        buf[ti] = '\0';
        if ((buf[0] >= '0' && buf[0] <= '9') || (buf[0] == '-' && buf[1] >= '0')) {
            f_push(atof(buf));
        } else {
            if (f_sp < 2) {
                print_line_scroll("Error: Not enough operands!", 0, row, 0x0C);
                return;
            }
            float b = f_pop();
            float a = f_pop();
            if (buf[0] == '+') f_push(a + b);
            else if (buf[0] == '-') f_push(a - b);
            else if (buf[0] == '*') f_push(a * b);
            else if (buf[0] == '/') {
                if (b != 0.0f) f_push(a / b);
                else {
                    print_line_scroll("Error: Div by zero!", 0, row, 0x0C);
                    return;
                }
            } else {
                print_line_scroll("Error: Unknown operator!", 0, row, 0x0C);
                return;
            }
        }
    }
    if (f_sp != 1) {
        print_line_scroll("Error: Invalid expression!", 0, row, 0x0C);
    } else {
        float result = f_pop();
        char res_str[64];
        char num[32];
        ftoa(result, num, 3);
        strcpy(res_str, "Result: ");
        strcat(res_str, num);
        print_line_scroll(res_str, 0, row, 0x0A);
    }
}

REGISTER_COMMAND("calc", cmd_calc, 1);