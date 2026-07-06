int main() {
    int x = 1;
    int y = 0;
    switch (x) {
        case 1: y = y + 1;
        case 2: y = y + 2;
        case 3: y = y + 4;
    }
    return y;
}
