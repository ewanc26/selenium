#include <stdio.h>
int main(void) {
    const char a = 'A';
    const char nl = '\n';
    const char q = '\'';
    printf("%c\n", a);
    printf("%c\n", q);
    printf("%d\n", a + 1);
    printf("%c\n", nl);
    return 0;
}
