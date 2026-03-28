#include <stdio.h>
#include <stdlib.h>

/* --- Multi-line macro with do-while(0) --- */
#define SWAP(a, b) do {  \
    __typeof__(a) _tmp = (a); \
    (a) = (b);            \
    (b) = _tmp;            \
} while (0)

#define LOG(msg) do {                                       \
    printf("[%s:%d] %s\n", __FILE__, __LINE__, (msg));      \
} while (0)

/* --- Conditional with -D flag --- */
#ifndef BUILD_MODE
#define BUILD_MODE "default"
#endif

#ifndef MAX_ITEMS
#define MAX_ITEMS 10
#endif

int main(void)
{
    printf("=== Multi-line macro: SWAP ===\n");
    int x = 10, y = 20;
    printf("Before: x=%d, y=%d\n", x, y);
    SWAP(x, y);
    printf("After:  x=%d, y=%d\n", x, y);

    double p = 1.5, q = 9.9;
    printf("\nBefore: p=%.1f, q=%.1f\n", p, q);
    SWAP(p, q);
    printf("After:  p=%.1f, q=%.1f\n", p, q);

    printf("\n=== Multi-line macro: LOG ===\n");
    LOG("program started");
    LOG("about to finish");

    printf("\n=== -D command-line defines ===\n");
    printf("BUILD_MODE = %s\n", BUILD_MODE);
    printf("MAX_ITEMS  = %d\n", MAX_ITEMS);

    return 0;
}
