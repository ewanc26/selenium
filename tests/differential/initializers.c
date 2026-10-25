#include <stdio.h>
struct Point {
    int x;
    int y;
};
int main(void) {
    const struct Point pts[2] = {{1, 2}, {3, 4}};
    printf("%d\n", pts[0].x);
    printf("%d\n", pts[1].y);
    int zeros[3] = {0, 0, 0};
    printf("%d\n", zeros[2]);
    return 0;
}
