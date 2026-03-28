#ifndef MATHUTIL_H
#define MATHUTIL_H

/* Returns the factorial of n (n must be >= 0 and <= 20). */
long long factorial(int n);

/* Returns the n-th Fibonacci number (n >= 0). */
long long fibonacci(int n);

/* Returns 1 if n is prime, 0 otherwise. */
int is_prime(int n);

/* Returns the library version string. */
const char *mathutil_version(void);

#endif /* MATHUTIL_H */
