#include "commands.h"
#include "util.h"
#include "screen.h"

static int stack[16];
static int sp = 0;

void push(int val) {
    if (sp < 16) stack[sp++] = val;
}

int pop() {
    if (sp > 0) return stack[--sp];
    return 0;
}

void cmd_calc(char* args, int* row) {
    if (args == 0 || args[0] == '\0') {
        print_at_color("Usage: calc <num1> <num2> <op> (e.g. 2 2 +)", *row, 0, 0x0E);
        (*row)++;
        return;
    }

    sp = 0;
    char* token = args;
    char buf[32];

    while (*token != '\0') {
        if (*token == ' ') {
            token++;
            continue;
        }

        int ti = 0;
        while (*token != ' ' && *token != '\0') {
            buf[ti++] = *token++;
        }
        buf[ti] = '\0';

        if (buf[0] >= '0' && buf[0] <= '9') {
            push(atoi(buf));
        } else {
            int b = pop();
            int a = pop();
            if (buf[0] == '+') push(a + b);
            else if (buf[0] == '-') push(a - b);
            else if (buf[0] == '*') push(a * b);
            else if (buf[0] == '/') {
                if (b != 0) push(a / b);
                else {
                    print_at_color("Error: Div by zero!", *row, 0, 0x0C);
                    (*row)++;
                    return;
                }
            }
        }
    }

    int result = pop();
    char res_str[16];
    itoa(result, res_str);

    print_at("Result: ", *row, 0);
    print_at_color(res_str, *row, 8, 0x0A);
    (*row)++;
}

REGISTER_COMMAND("calc", cmd_calc, 1);