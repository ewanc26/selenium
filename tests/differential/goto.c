#include <stdio.h>
int main(void) {
    int n = 0;
    goto skip;
    printf("%d\n", 99);
skip:;
    {
        n = n + 1;
    }
top:;
    {
        n = n + 1;
    }
    if (n < 5) {
        goto top;
    }
    printf("%d\n", n);
    return 0;
}
