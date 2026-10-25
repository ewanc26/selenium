#include <stdbool.h>
#include <stdio.h>
struct Node {
    int value;
    struct Node *next;
};
static int add_one(int n) { return n + 1; }
static const char *b(bool v) { return v ? "true" : "false"; }
static bool find(struct Node *head, int target) {
    struct Node *cur = head;
    while (cur != nullptr) {
        if (cur->value == target) {
            return true;
        }
        cur = cur->next;
    }
    return false;
}
static void main_program(void) {
    struct Node *empty = nullptr;
    struct Node *const sealed = nullptr;
    struct Node tail = {3, nullptr};
    struct Node mid = {2, &tail};
    struct Node head = {1, &mid};
    struct Node *first = &head;
    printf("%s\n", b(empty == nullptr));
    printf("%s\n", b(sealed == empty));
    printf("%s\n", b(first != nullptr));
    printf("%s\n", b(find(first, 3)));
    printf("%s\n", b(find(empty, 3)));
    int (*op)(int) = nullptr;
    printf("%s\n", b(op == nullptr));
    op = add_one;
    printf("%s\n", b(op != nullptr));
    printf("%d\n", op(41));
    int total = 0;
    struct Node *cur = first;
    while (cur != nullptr) {
        total = total + cur->value;
        cur = cur->next;
    }
    printf("%d\n", total);
    struct Node *pick = first == nullptr ? nullptr : first;
    printf("%s\n", b(pick == nullptr));
    pick = nullptr;
    printf("%s\n", b(pick == nullptr));
}
int main(void) {
    main_program();
    return 0;
}
