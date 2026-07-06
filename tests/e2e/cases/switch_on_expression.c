int main() {
    int a = 2;
    int b = 3;
    int y = 0;
    switch (a + b) {
        case 4: y = 1; break;
        case 5: y = 2; break;
        default: y = 9;
    }
    return y;
}
