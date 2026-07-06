int main() {
    int sum = 0;
    for (int i = 0; i < 4; i = i + 1) {
        switch (i) {
            case 2: break;
            default: sum = sum + i;
        }
        sum = sum + 1;
    }
    return sum;
}
