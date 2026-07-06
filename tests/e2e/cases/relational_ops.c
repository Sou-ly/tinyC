int main() {
    int t = 0;
    if (3 < 4)  t = t + 1;
    if (4 < 3)  t = t + 2;
    if (5 > 2)  t = t + 4;
    if (2 >= 2) t = t + 8;
    if (2 <= 1) t = t + 16;
    return t;
}
