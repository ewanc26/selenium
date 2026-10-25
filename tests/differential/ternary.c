#include <stdio.h>
int main(void) {
    int a = 7;
    int b = 3;
    printf("%d\n", (a > b) ? a : b);
    printf("%d\n", (a < b) ? 1 : 0);
    int m = (a > 5) ? ((b > 2) ? 10 : 20) : 30;
    printf("%d\n", m);
    printf("%s\n", (a > b) ? "true" : "false");
    return 0;
}
