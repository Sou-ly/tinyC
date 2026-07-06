int main() {
    int a = 2;
    int b = 1;
    int r = 0;
    switch (a) {
        case 1: r = 10; break;
        case 2:
            switch (b) {
                case 1: r = 21; break;
                default: r = 29;
            }
            break;
        default: r = 99;
    }
    return r;
}
