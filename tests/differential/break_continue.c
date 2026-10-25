#include <stdio.h>
int main(void) {
    int total = 0;
    for (int k = 0; k < 10; k++) {
        if (k == 3) {
            continue;
        }
        if (k == 6) {
            break;
        }
        total = total + k;
    }
    printf("%d\n", total);
    int n = 0;
    while (n < 100) {
        n = n + 1;
        if (n == 4) {
            break;
        }
    }
    printf("%d\n", n);
    return 0;
}
