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
        print_line_scroll("Usage: calc <num1> <num2> <op>", 0, row, 0x0E);
        print_line_scroll("Example: calc 10 5 -", 0, row, 0x07);
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
            if (ti < 31) buf[ti++] = *token++;
            else token++;
        }
        buf[ti] = '\0';
        if ((buf[0] >= '0' && buf[0] <= '9') || (buf[0] == '-' && buf[1] >= '0' && buf[1] <= '9')) {
            push(atoi(buf));
        } else {
            if (sp < 2) {
                print_line_scroll("Error: Not enough operands!", 0, row, 0x0C);
                return;
            }

            int b = pop();
            int a = pop();
            
            if (buf[0] == '+') push(a + b);
            else if (buf[0] == '-') push(a - b);
            else if (buf[0] == '*') push(a * b);
            else if (buf[0] == '/') {
                if (b != 0) push(a / b);
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

    if (sp != 1) {
        print_line_scroll("Error: Invalid expression!", 0, row, 0x0C);
    } else {
        int result = pop();
        char res_str[32];
        char num[16];
        itoa(result, num);
        strcpy(res_str, "Result: ");
        strcat(res_str, num);
        print_line_scroll(res_str, 0, row, 0x0A);
    }
}

REGISTER_COMMAND("calc", cmd_calc, 1);