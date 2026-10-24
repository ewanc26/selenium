#include <stdbool.h>
#include <stdio.h>
static const char *b(bool v) { return v ? "true" : "false"; }
int main(void) {
    printf("%s\n", b(true && false));
    printf("%s\n", b(true || false));
    printf("%s\n", b(!true));
    printf("%s\n", b(3 < 4));
    printf("%s\n", b(4 == 4));
    printf("%s\n", b(4 != 4));
    return 0;
}
