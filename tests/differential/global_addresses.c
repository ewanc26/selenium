#include <stdio.h>
struct Node {
    int value;
    struct Node *next;
};
struct Op {
    const char *name;
    int (*fn)(int, int);
};
static int add_op(int a, int b) { return a + b; }
static int mul_op(int a, int b) { return a * b; }
struct Node tail = {3, nullptr};
struct Node mid = {2, &tail};
struct Node head = {1, &mid};
struct Node *first = &head;
struct Node pool[3] = {{10, &pool[1]}, {20, &pool[2]}, {30, nullptr}};
struct Node *const pool_start = &pool[0];
struct Op ops[2] = {{"add", add_op}, {"mul", mul_op}};
int counter = 7;
int *counter_ptr = &counter;
static int apply(struct Op *table, int idx, int a, int b) {
    return table[idx].fn(a, b);
}
static void main_program(void) {
    struct Node *cur = first;
    while (cur != nullptr) {
        printf("%d\n", cur->value);
        cur = cur->next;
    }
    struct Node *p = pool_start;
    printf("%d\n", p->next->value);
    printf("%d\n", p->next->next->value);
    printf("%d\n", apply(&ops[0], 0, 3, 4));
    printf("%d\n", apply(&ops[0], 1, 3, 4));
    *counter_ptr = *counter_ptr + 1;
    printf("%d\n", counter);
    printf("%s\n", ops[1].name);
}
int main(void) {
    main_program();
    return 0;
}
