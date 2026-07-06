int main() {
    int sum = 0;
    for (int i = 0; i < 5; i = i + 1) {
        switch (i) {
            case 2: continue;
            default: sum = sum + i;
        }
        sum = sum + 10;
    }
    return sum;
}
