#include <stdio.h>
#include <stdlib.h>
#include "mathutil.h"

int main(int argc, char *argv[])
{
    int n = 10;
    if (argc > 1) {
        n = atoi(argv[1]);
    }

    printf("mathutil version: %s\n", mathutil_version());
    printf("factorial(%d)  = %lld\n", n, factorial(n));
    printf("fibonacci(%d)  = %lld\n", n, fibonacci(n));
    printf("is_prime(%d)   = %s\n", n, is_prime(n) ? "yes" : "no");

    return 0;
}
