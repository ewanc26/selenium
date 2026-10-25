#include <stdio.h>
int main(void) {
    int a = 4;
    int b = 9;
    int *p = &a;
    printf("%d\n", *p);
    p = &b;
    printf("%d\n", *p);
    *p = *p + 1;
    printf("%d\n", b);
    int *const q = &a;
    printf("%d\n", *q);
    return 0;
}
