#include <stdio.h>
int main(void) {
    signed char a = 100;
    unsigned char b = 200;
    short c = 30000;
    unsigned short d = 60000;
    unsigned int e = 4000000000;
    long f = 1234567890123;
    unsigned long g = 9000000000000000000;
    printf("%d\n", (int)(a));
    printf("%d\n", (int)(b));
    printf("%d\n", (int)(c));
    printf("%d\n", (int)(d));
    printf("%u\n", e);
    printf("%ld\n", f);
    printf("%lu\n", g);
    return 0;
}
