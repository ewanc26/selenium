#include <stdio.h>
int main(void) {
    int x = 10;
    x += 5;
    x -= 3;
    x *= 2;
    x /= 4;
    x %= 4;
    x <<= 3;
    x >>= 1;
    x &= 12;
    x |= 3;
    x ^= 5;
    printf("%d\n", x);
    return 0;
}
