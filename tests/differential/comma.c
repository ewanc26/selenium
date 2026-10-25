#include <stdio.h>
int main(void) {
    int i = 0;
    int j = 10;
    int v = (++i, ++j, i + j);
    printf("%d\n", v);
    printf("%d\n", i);
    return 0;
}
