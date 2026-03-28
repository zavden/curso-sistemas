#include <stdio.h>

int tiny_add(int a, int b) {
    return a + b;
}

int medium_work(int x) {
    int sum = 0;
    for (int i = 0; i < x; i++) {
        sum += i * i;
        if (sum > 1000) {
            sum = sum % 100;
        }
    }
    for (int j = 0; j < x; j++) {
        sum += j * 3;
        if (sum > 500) {
            sum = sum / 2;
        }
    }
    return sum;
}

int main(void) {
    int a = tiny_add(10, 20);
    int b = medium_work(a);
    printf("tiny_add(10, 20) = %d\n", a);
    printf("medium_work(%d) = %d\n", a, b);
    return 0;
}
