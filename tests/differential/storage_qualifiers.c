#include <stdio.h>
static int counter = 0;
volatile int ticks = 5;
static int bump(void) {
    static int n = 0;
    n = n + 1;
    counter = counter + n;
    return counter;
}
int main(void) {
    printf("%d\n", bump());
    printf("%d\n", bump());
    printf("%d\n", ticks);
    return 0;
}
