int check6(int a, int b, int c, int d, int e, int f) {
    int r = 0;
    if (a == 1) r = r + 1;
    if (b == 2) r = r + 2;
    if (c == 3) r = r + 4;
    if (d == 4) r = r + 8;
    if (e == 5) r = r + 16;
    if (f == 6) r = r + 32;
    return r;
}

int main() {
    return check6(1, 2, 3, 4, 5, 6);
}
