#include <stdio.h>
int main(void) {
    int grid[2][3] = {{1, 2, 3}, {4, 5, 6}};
    grid[1][2] = 60;
    printf("%d\n", grid[1][2]);
    printf("%d\n", grid[0][1]);
    printf("%d\n", (int)sizeof(grid));
    return 0;
}
