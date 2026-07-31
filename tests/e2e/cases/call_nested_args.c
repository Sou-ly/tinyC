int dbl(int x) {
    return x + x;
}

int add3(int a, int b, int c) {
    return a + b + c;
}

int main() {
    return add3(dbl(1), dbl(2), dbl(3));
}
