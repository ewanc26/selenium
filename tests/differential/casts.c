#include <stdio.h>
#include <stdbool.h>
int main(void) {
    int a = 7;
    int b = 2;
    printf("%g\n", ((double)(a)) / ((double)(b)));
    printf("%d\n", ((int)(3.9)));
    printf("%c\n", ((char)(66)));
    printf("%d\n", ((int)('A')) + 1);
    printf("%s\n", ((_Bool)(5)) ? "true" : "false");
    return 0;
}
