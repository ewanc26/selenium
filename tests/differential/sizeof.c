#include <stdio.h>
struct Pair {
    int a;
    int b;
};
int main(void) {
    int n = (int)sizeof(int);
    printf("%d\n", n);
    printf("%d\n", (int)sizeof(char));
    int arr[5] = {1, 2, 3, 4, 5};
    printf("%d\n", (int)sizeof(arr));
    printf("%d\n", (int)sizeof(struct Pair));
    return 0;
}
