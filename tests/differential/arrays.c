#include <stdbool.h>
#include <stdio.h>
int main(void) {
    int xs[4] = {3, 1, 4, 1};
    xs[2] = 9;
    int total = 0;
    int i = 0;
    while (i < 4) {
        total = total + xs[i];
        i = i + 1;
    }
    printf("%d\n", total);
    printf("%d\n", xs[2]);
    const bool flags[2] = {true, false};
    printf("%s\n", flags[1] ? "true" : "false");
    return 0;
}
