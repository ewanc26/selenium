#include <stdio.h>
static void swap(int *a, int i, int j) {
    int t = a[i];
    a[i] = a[j];
    a[j] = t;
}
static int count_derangements(int *a, int k, int n) {
    if (k == n) {
        int fixed = 0;
        int i = 0;
        while (i < n) {
            if (a[i] == i + 1) {
                fixed = fixed + 1;
            }
            i = i + 1;
        }
        if (fixed == 0) {
            return 1;
        }
        return 0;
    }
    int total = 0;
    int j = k;
    while (j < n) {
        swap(a, k, j);
        total = total + count_derangements(a, k + 1, n);
        swap(a, k, j);
        j = j + 1;
    }
    return total;
}
static int length_of(const char *s) {
    int n = 0;
    while (s[n] != '\0') {
        n = n + 1;
    }
    return n;
}
static int lcs(const char *a, const char *b) {
    int n = length_of(a);
    int m = length_of(b);
    int dp[8][8] = {
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
    };
    int i = 1;
    while (i <= n) {
        int j = 1;
        while (j <= m) {
            if (a[i - 1] == b[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1] + 1;
            } else {
                if (dp[i - 1][j] >= dp[i][j - 1]) {
                    dp[i][j] = dp[i - 1][j];
                } else {
                    dp[i][j] = dp[i][j - 1];
                }
            }
            j = j + 1;
        }
        i = i + 1;
    }
    return dp[n][m];
}
static int paths(int r, int c, int *memo) {
    if (r == 0 || c == 0) {
        return 1;
    }
    if (memo[r * 5 + c] != 0) {
        return memo[r * 5 + c];
    }
    int v = paths(r - 1, c, memo) + paths(r, c - 1, memo);
    memo[r * 5 + c] = v;
    return v;
}
static void main_program(void) {
    int a[5] = {1, 2, 3, 4, 5};
    int n = 1;
    while (n <= 5) {
        printf("%d\n", count_derangements(&a[0], 0, n));
        n = n + 1;
    }
    printf("%d\n", lcs("lunar", "solar"));
    printf("%d\n", lcs("kitten", "sitting"));
    printf("%d\n", lcs("", "moon"));
    int memo[25] = {
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
    };
    printf("%d\n", paths(4, 4, &memo[0]));
    printf("%d\n", paths(4, 4, &memo[0]));
    printf("%d\n", memo[4 * 5 + 4]);
}
int main(void) {
    main_program();
    return 0;
}
