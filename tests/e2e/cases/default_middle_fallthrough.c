int main() {
    int x = 40;
    int y = 0;
    switch (x) {
        case 1: y = 1; break;
        default: y = y + 3;
        case 2: y = y + 10;
    }
    return y;
}
