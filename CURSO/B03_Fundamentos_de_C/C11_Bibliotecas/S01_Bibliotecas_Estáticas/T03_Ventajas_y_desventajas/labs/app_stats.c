#include <stdio.h>
#include <stdlib.h>
#include "mathutil.h"

int main(void)
{
    printf("mathutil version: %s\n", mathutil_version());
    printf("\nPrime numbers up to 50:\n");

    int count = 0;
    for (int i = 2; i <= 50; i++) {
        if (is_prime(i)) {
            printf("  %d", i);
            count++;
        }
    }
    printf("\n  (%d primes found)\n", count);

    printf("\nFirst 15 Fibonacci numbers:\n ");
    for (int i = 0; i < 15; i++) {
        printf(" %lld", fibonacci(i));
    }
    printf("\n");

    return 0;
}
