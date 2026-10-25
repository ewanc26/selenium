#include <stdio.h>
int main(void) {
    double x = 1.5;
    double y = 2.25;
    printf("%g\n", x * y);
    const float s = 0.5;
    printf("%g\n", s + 1.0);
    const long double big = 3.0;
    printf("%Lg\n", big / 2.0);
    return 0;
}
