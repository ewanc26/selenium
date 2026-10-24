#include <stdio.h>
#include "ffi_helper.c"
int scale(int n);
int main(void) {
    printf("%d\n", scale(7));
    return 0;
}
