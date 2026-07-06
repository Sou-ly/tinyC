int main() {
    int x = 1;
    int y = 0;
    switch (x) {
        case 1: y = y + 1; break;
        case 2: y = y + 2; break;
        default: y = 99;
    }
    return y;
}
