#include <stdio.h>
static int twice(int n) { return n * 2; }
static int square(int n) { return n * n; }
int main(void) {
    printf("%d\n", twice(3));
    printf("%d\n", square(4));
    return 0;
}
