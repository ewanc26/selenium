#include <stdio.h>
static int label(int x) {
    int r = 0;
    switch (x) {
    case 1:
        r = 10;
        break;
    case 2:
        r = 20;
        break;
    default:
        r = 99;
        break;
    }
    return r;
}
int main(void) {
    printf("%d\n", label(1));
    printf("%d\n", label(2));
    printf("%d\n", label(7));
    return 0;
}
