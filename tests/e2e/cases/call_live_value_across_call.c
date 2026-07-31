int side(int x) {
    return x * 2;
}

int main() {
    int a = 10;
    return (a + 1) + side(3) + (a + 2);
}
