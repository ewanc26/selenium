#include <stdio.h>
struct Point {
    int x;
    int y;
};
int main(void) {
    struct Point p = {3, 4};
    p.x = p.x + 10;
    printf("%d\n", p.x);
    printf("%d\n", p.y);
    struct Point *pp = &p;
    pp->y = 20;
    printf("%d\n", p.y);
    return 0;
}
