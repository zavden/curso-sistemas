#include "mathutil.h"

static const char *VERSION = "1.0.0";

long long factorial(int n)
{
    if (n < 0 || n > 20) {
        return -1;
    }
    long long result = 1;
    for (int i = 2; i <= n; i++) {
        result *= i;
    }
    return result;
}

long long fibonacci(int n)
{
    if (n < 0) {
        return -1;
    }
    if (n <= 1) {
        return n;
    }
    long long a = 0, b = 1;
    for (int i = 2; i <= n; i++) {
        long long tmp = a + b;
        a = b;
        b = tmp;
    }
    return b;
}

int is_prime(int n)
{
    if (n < 2) {
        return 0;
    }
    for (int i = 2; i * i <= n; i++) {
        if (n % i == 0) {
            return 0;
        }
    }
    return 1;
}

const char *mathutil_version(void)
{
    return VERSION;
}
