int helper(void);
int main() {
    int x = 3;
    for (;;) {
        x = x + 1;
        if (x > 5) break;
    }
    return x;
}
