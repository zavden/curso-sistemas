#include <stdio.h>

int main(void) {
    printf("=== Integer division (truncation toward zero) ===\n\n");

    printf(" 17 / 5  = %d\n",  17 / 5);
    printf("-17 / 5  = %d\n", -17 / 5);
    printf(" 17 / -5 = %d\n",  17 / -5);
    printf("-17 / -5 = %d\n", -17 / -5);

    printf("\n=== Modulo (sign follows dividend, C99+) ===\n\n");

    printf(" 17 %%  5 = %d\n",  17 %  5);
    printf("-17 %%  5 = %d\n", -17 %  5);
    printf(" 17 %% -5 = %d\n",  17 % -5);
    printf("-17 %% -5 = %d\n", -17 % -5);

    printf("\n=== Invariant: (a/b)*b + a%%b == a ===\n\n");

    int a = -17, b = 5;
    int quotient = a / b;
    int remainder = a % b;
    int reconstructed = quotient * b + remainder;

    printf("a = %d, b = %d\n", a, b);
    printf("a/b = %d, a%%b = %d\n", quotient, remainder);
    printf("(a/b)*b + a%%b = %d * %d + %d = %d\n",
           quotient, b, remainder, reconstructed);
    printf("Equals a? %s\n", reconstructed == a ? "YES" : "NO");

    printf("\n=== Practical: seconds to h/m/s ===\n\n");

    int total_seconds = 3661;
    int hours   = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;

    printf("%d seconds = %dh %dm %ds\n",
           total_seconds, hours, minutes, seconds);

    printf("\n=== Float vs integer division ===\n\n");

    printf("17 / 5     = %d   (integer)\n", 17 / 5);
    printf("17.0 / 5   = %f   (float)\n", 17.0 / 5);
    printf("(double)17 / 5 = %f   (cast)\n", (double)17 / 5);

    return 0;
}
