#include <stdbool.h>
#include <stdio.h>

void selenium_print_int(int value) { printf("%d\n", value); }
void selenium_print_float(double value) { printf("%.15g\n", value); }
void selenium_print_bool(_Bool value) { puts(value ? "true" : "false"); }
void selenium_print_char(char value) { putchar((unsigned char)value); putchar('\n'); }
void selenium_print_string(const char *value) { puts(value ? value : "(null)"); }

int selenium_read_int(void) {
    int value = 0;
    if (scanf("%d", &value) != 1) return 0;
    return value;
}
double selenium_read_float(void) {
    double value = 0.0;
    if (scanf("%lf", &value) != 1) return 0.0;
    return value;
}
_Bool selenium_read_bool(void) {
    int value = 0;
    if (scanf("%d", &value) != 1) return false;
    return value != 0;
}
char selenium_read_char(void) {
    char value = '\0';
    if (scanf(" %c", &value) != 1) return '\0';
    return value;
}
