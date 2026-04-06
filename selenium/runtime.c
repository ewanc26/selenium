#include <stdbool.h>
#include <stdio.h>

void selenium_print_int(int value) { printf("%d\n", value); }
void selenium_print_float(double value) { printf("%g\n", value); }
void selenium_print_bool(_Bool value) { printf("%s\n", value ? "true" : "false"); }
void selenium_print_char(char value) { printf("%c\n", value); }
void selenium_print_string(const char *value) { printf("%s\n", value); }

int selenium_read_int() { int x; scanf("%d", &x); return x; }
double selenium_read_float() { double x; scanf("%lf", &x); return x; }
_Bool selenium_read_bool() { int x; scanf("%d", &x); return x; }
char selenium_read_char() { char x; scanf(" %c", &x); return x; }
