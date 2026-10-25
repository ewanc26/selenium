#include <stdio.h>
struct Rec {
    const char *name;
    int score;
    struct Rec *next;
};
static void sort_by_score(struct Rec *xs, int n) {
    int i = 1;
    while (i < n) {
        struct Rec key = xs[i];
        int j = i - 1;
        while (j >= 0 && xs[j].score < key.score) {
            xs[j + 1] = xs[j];
            j = j - 1;
        }
        xs[j + 1] = key;
        i = i + 1;
    }
}
static void link(struct Rec *xs, int n) {
    int i = 0;
    while (i < n - 1) {
        xs[i].next = &xs[i + 1];
        i = i + 1;
    }
    xs[n - 1].next = nullptr;
}
static int walk(struct Rec *head) {
    int total = 0;
    int rank = 1;
    struct Rec *cur = head;
    while (cur != nullptr) {
        total = total + rank * cur->score;
        printf("%s\n", cur->name);
        cur = cur->next;
        rank = rank + 1;
    }
    return total;
}
static void main_program(void) {
    struct Rec table[5] = {
        {"ada", 72, nullptr},
        {"brin", 91, nullptr},
        {"cole", 58, nullptr},
        {"dara", 91, nullptr},
        {"eli", 66, nullptr},
    };
    sort_by_score(&table[0], 5);
    link(&table[0], 5);
    printf("%d\n", walk(&table[0]));
    int i = 0;
    int best = table[0].score;
    while (i < 5) {
        if (table[i].score == best) {
            printf("%s\n", table[i].name);
        }
        i = i + 1;
    }
}
int main(void) {
    main_program();
    return 0;
}
