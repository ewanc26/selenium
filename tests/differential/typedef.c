#include <stdio.h>
int main(void) {
    const int n = 7;
    int m = n + 1;
    int *p = &m;
    *p = 9;
    printf("%d\n", m);
    return 0;
}
