#include <stdio.h>
static int add(int a, int b) { return a + b; }
static int mul(int a, int b) { return a * b; }
static int twice(int n) { return n * 2; }
static int apply(int (*g)(int), int v) { return g(v); }
int main(void) {
    int (*const op)(int, int) = add;
    printf("%d\n", op(2, 3));
    int (*f)(int, int) = mul;
    printf("%d\n", f(4, 5));
    f = add;
    printf("%d\n", f(4, 5));
    printf("%d\n", apply(twice, 5));
    return 0;
}
