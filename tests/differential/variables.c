#include <stdio.h>
int main(void) {
    int x = 5;
    const int y = 10;
    x = x + y;
    printf("%d\n", x);
    printf("%d\n", y);
    return 0;
}
