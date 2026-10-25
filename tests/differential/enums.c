#include <stdio.h>
enum Color { Red, Green = 5, Blue };
int main(void) {
    const enum Color c = Green;
    printf("%d\n", c);
    enum Color d = Blue;
    printf("%d\n", d);
    printf("%d\n", Blue - Red);
    return 0;
}
