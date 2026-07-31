int check8(int a, int b, int c, int d, int e, int f, int g, int h) {
    int r = 0;
    if (a == 1) r = r + 1;
    if (b == 2) r = r + 2;
    if (c == 3) r = r + 4;
    if (d == 4) r = r + 8;
    if (e == 5) r = r + 16;
    if (f == 6) r = r + 32;
    if (g == 7) r = r + 64;
    if (h == 8) r = r + 128;
    return r;
}

int main() {
    int a = 1;
    int b = 2;
    int c = 3;
    int d = 4;
    int e = 5;
    int f = 6;
    int g = 7;
    int h = 8;
    return check8(a, b, c, d, e, f, g, h);
}
