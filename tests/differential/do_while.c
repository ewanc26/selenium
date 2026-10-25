#include <stdio.h>
int main(void) {
    int n = 0;
    do {
        n = n + 3;
    } while (n < 10);
    printf("%d\n", n);
    int k = 100;
    do {
        k = k + 1;
    } while (k < 0);
    printf("%d\n", k);
    return 0;
}
