#include <stdio.h>
#include <stdlib.h>
struct Op {
    const char *name;
    int (*fn)(int, int);
};
static int add_op(int a, int b) { return a + b; }
static int sub_op(int a, int b) { return a - b; }
static int mul_op(int a, int b) { return a * b; }
static int div_op(int a, int b) {
    if (b == 0) {
        printf("error: division by zero\n");
        exit(3);
    }
    return a / b;
}
static _Bool is_digit(char c) {
    return c >= '0' && c <= '9';
}
static int tokenize(const char *src, int *kinds, int *vals) {
    int count = 0;
    int i = 0;
    while (src[i] != '\0') {
        char c = src[i];
        switch (c) {
            case ' ': {
                i = i + 1;
                break;
            }
            case '+': {
                kinds[count] = 1;
                vals[count] = 0;
                count = count + 1;
                i = i + 1;
                break;
            }
            case '-': {
                kinds[count] = 1;
                vals[count] = 1;
                count = count + 1;
                i = i + 1;
                break;
            }
            case '*': {
                kinds[count] = 1;
                vals[count] = 2;
                count = count + 1;
                i = i + 1;
                break;
            }
            case '/': {
                kinds[count] = 1;
                vals[count] = 3;
                count = count + 1;
                i = i + 1;
                break;
            }
            default: {
                if (is_digit(c)) {
                    int n = 0;
                    while (is_digit(src[i])) {
                        n = n * 10 + (int)src[i] - (int)'0';
                        i = i + 1;
                    }
                    kinds[count] = 0;
                    vals[count] = n;
                    count = count + 1;
                } else {
                    printf("error: bad character\n");
                    exit(2);
                }
                break;
            }
        }
    }
    return count;
}
static int evaluate(const char *src, struct Op *ops) {
    int kinds[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int vals[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int stack[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int ntok = tokenize(src, &kinds[0], &vals[0]);
    int sp = 0;
    int t = 0;
    while (t < ntok) {
        if (kinds[t] == 0) {
            stack[sp] = vals[t];
            sp = sp + 1;
        } else {
            if (sp < 2) {
                printf("error: stack underflow\n");
                exit(4);
            }
            int b = stack[sp - 1];
            int a = stack[sp - 2];
            stack[sp - 2] = ops[vals[t]].fn(a, b);
            sp = sp - 1;
        }
        t = t + 1;
    }
    return stack[0];
}
static void main_program(void) {
    struct Op ops[4] = {
        {"add", add_op},
        {"sub", sub_op},
        {"mul", mul_op},
        {"div", div_op},
    };
    const char *exprs[5] = {
        "3 4 + 2 *",
        "5 1 2 + 4 * + 3 -",
        "9 3 / 7 -",
        "6 2 / 1 +",
        "8 0 /",
    };
    int i = 0;
    while (i < 5) {
        printf("%d\n", evaluate(exprs[i], &ops[0]));
        i = i + 1;
    }
}
int main(void) {
    main_program();
    return 0;
}
