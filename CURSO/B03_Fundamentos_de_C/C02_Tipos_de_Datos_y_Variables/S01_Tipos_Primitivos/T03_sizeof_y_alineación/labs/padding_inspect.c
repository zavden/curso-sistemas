#include <stdio.h>
#include <stddef.h>
#include <stdalign.h>

struct Bad {
    char  a;
    int   b;
    char  c;
};

struct Good {
    int   b;
    char  a;
    char  c;
};

struct Mixed {
    char   a;
    double b;
    char   c;
    int    d;
    short  e;
};

int main(void) {
    printf("=== struct Bad (char, int, char) ===\n");
    printf("sizeof:  %zu bytes\n", sizeof(struct Bad));
    printf("alignof: %zu\n", alignof(struct Bad));
    printf("  a: offset=%zu  size=%zu\n", offsetof(struct Bad, a), sizeof(char));
    printf("  b: offset=%zu  size=%zu\n", offsetof(struct Bad, b), sizeof(int));
    printf("  c: offset=%zu  size=%zu\n", offsetof(struct Bad, c), sizeof(char));

    printf("\n=== struct Good (int, char, char) ===\n");
    printf("sizeof:  %zu bytes\n", sizeof(struct Good));
    printf("alignof: %zu\n", alignof(struct Good));
    printf("  b: offset=%zu  size=%zu\n", offsetof(struct Good, b), sizeof(int));
    printf("  a: offset=%zu  size=%zu\n", offsetof(struct Good, a), sizeof(char));
    printf("  c: offset=%zu  size=%zu\n", offsetof(struct Good, c), sizeof(char));

    printf("\n=== struct Mixed (char, double, char, int, short) ===\n");
    printf("sizeof:  %zu bytes\n", sizeof(struct Mixed));
    printf("alignof: %zu\n", alignof(struct Mixed));
    printf("  a: offset=%2zu  size=%zu\n", offsetof(struct Mixed, a), sizeof(char));
    printf("  b: offset=%2zu  size=%zu\n", offsetof(struct Mixed, b), sizeof(double));
    printf("  c: offset=%2zu  size=%zu\n", offsetof(struct Mixed, c), sizeof(char));
    printf("  d: offset=%2zu  size=%zu\n", offsetof(struct Mixed, d), sizeof(int));
    printf("  e: offset=%2zu  size=%zu\n", offsetof(struct Mixed, e), sizeof(short));

    size_t sum_bad = sizeof(char) + sizeof(int) + sizeof(char);
    size_t sum_mixed = sizeof(char) + sizeof(double) + sizeof(char)
                     + sizeof(int) + sizeof(short);

    printf("\n=== Padding desperdiciado ===\n");
    printf("Bad:   sizeof=%zu, suma de campos=%zu, padding=%zu bytes\n",
           sizeof(struct Bad), sum_bad, sizeof(struct Bad) - sum_bad);
    printf("Mixed: sizeof=%zu, suma de campos=%zu, padding=%zu bytes\n",
           sizeof(struct Mixed), sum_mixed, sizeof(struct Mixed) - sum_mixed);

    return 0;
}
