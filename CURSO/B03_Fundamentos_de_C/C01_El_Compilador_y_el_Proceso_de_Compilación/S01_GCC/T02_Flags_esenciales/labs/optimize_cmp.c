#include <stdio.h>

long sum_to_n(long n) {
    long total = 0;
    for (long i = 1; i <= n; i++) {
        total += i;
    }
    return total;
}

int main(void) {
    long n = 1000000;
    long result = sum_to_n(n);
    printf("sum(1..%ld) = %ld\n", n, result);
    return 0;
}
