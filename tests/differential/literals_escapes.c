#include <stdio.h>
int main(void) {
    printf("%s\n", "tab:\there");
    printf("%s\n", "quote:\"q\"");
    printf("%s\n", "back\\slash");
    printf("%s\n", "hex:\x41");
    printf("%s\n", "oct:\101");
    return 0;
}
