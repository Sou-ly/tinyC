int sum7(int n, int b, int c, int d, int e, int f, int g) {
    if (n == 0) return b + c + d + e + f + g;
    return sum7(n - 1, b, c, d, e, f, g);
}

int main() {
    return sum7(3, 1, 2, 4, 8, 16, 32);
}
