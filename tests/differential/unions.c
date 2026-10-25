#include <stdio.h>
union Cell {
    int count;
    char tag;
};
int main(void) {
    union Cell c = {41};
    c.count = c.count + 1;
    printf("%d\n", c.count);
    return 0;
}
