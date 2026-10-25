#include <stdbool.h>
#include <stdio.h>
static int fact(int n);
static int fib(int n);
static bool is_even(int n);
static bool is_odd(int n);
static int fact(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * fact(n - 1);
}
static int fib(int n) {
    if (n < 2) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}
static bool is_even(int n) {
    if (n == 0) {
        return true;
    }
    return is_odd(n - 1);
}
static bool is_odd(int n) {
    if (n == 0) {
        return false;
    }
    return is_even(n - 1);
}
int main(void) {
    printf("%d\n", fact(5));
    printf("%d\n", fib(10));
    printf("%s\n", is_even(10) ? "true" : "false");
    printf("%s\n", is_odd(7) ? "true" : "false");
    return 0;
}
