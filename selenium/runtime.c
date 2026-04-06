#include <stdbool.h>
#include <stdio.h>

void selenium_print_int(int value) { printf("%d\n", value); }
void selenium_print_float(double value) { printf("%g\n", value); }
void selenium_print_bool(_Bool value) { printf("%s\n", value ? "true" : "false"); }
void selenium_print_char(char value) { printf("%c\n", value); }
void selenium_print_string(const char *value) { printf("%s\n", value); }
