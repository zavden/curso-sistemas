#include <stdio.h>
#include <stddef.h>
#include <stdalign.h>
#include <string.h>

/*
 * Visualizes the exact memory layout of structs by printing a byte-by-byte
 * map showing which bytes belong to fields and which are padding.
 *
 * Compares Bad (arbitrary order) vs Good (sorted largest-to-smallest).
 */

struct Bad {
    char   a;       /* 1 byte  */
    double b;       /* 8 bytes */
    char   c;       /* 1 byte  */
    int    d;       /* 4 bytes */
    short  e;       /* 2 bytes */
};

struct Good {
    double b;       /* 8 bytes */
    int    d;       /* 4 bytes */
    short  e;       /* 2 bytes */
    char   a;       /* 1 byte  */
    char   c;       /* 1 byte  */
};

/* Helper: fill struct memory with a known pattern, then set fields
   to recognizable values. Padding bytes will retain the fill pattern. */
static void show_memory_map(const void *ptr, size_t size,
                            const char *name) {
    const unsigned char *bytes = (const unsigned char *)ptr;
    printf("%s (%zu bytes):\n", name, size);
    printf("  Offset: ");
    for (size_t i = 0; i < size; i++) {
        printf("%3zu", i);
    }
    printf("\n  Hex:    ");
    for (size_t i = 0; i < size; i++) {
        printf(" %02X", bytes[i]);
    }
    printf("\n  Tipo:   ");
    for (size_t i = 0; i < size; i++) {
        if (bytes[i] == 0xAA) {
            printf("  .");   /* padding (still has fill pattern) */
        } else {
            printf("  D");   /* data (field overwrote the fill) */
        }
    }
    printf("\n  (D = dato, . = padding)\n\n");
}

int main(void) {
    printf("=== Mapa de memoria byte por byte ===\n\n");

    /* Fill with 0xAA, then set fields. Padding bytes keep 0xAA. */
    struct Bad bad;
    memset(&bad, 0xAA, sizeof(bad));
    bad.a = 'X';           /* 0x58 */
    bad.b = 0.0;           /* 8 bytes of 0x00 */
    bad.c = 'Y';           /* 0x59 */
    bad.d = 1;             /* 0x01 0x00 0x00 0x00 in little-endian */
    bad.e = 2;             /* 0x02 0x00 in little-endian */

    show_memory_map(&bad, sizeof(bad), "struct Bad");

    printf("Detalle de struct Bad:\n");
    printf("  a:  offset=%2zu  size=%zu\n", offsetof(struct Bad, a),
           sizeof(bad.a));
    printf("  b:  offset=%2zu  size=%zu\n", offsetof(struct Bad, b),
           sizeof(bad.b));
    printf("  c:  offset=%2zu  size=%zu\n", offsetof(struct Bad, c),
           sizeof(bad.c));
    printf("  d:  offset=%2zu  size=%zu\n", offsetof(struct Bad, d),
           sizeof(bad.d));
    printf("  e:  offset=%2zu  size=%zu\n", offsetof(struct Bad, e),
           sizeof(bad.e));

    size_t sum_bad = sizeof(char) + sizeof(double) + sizeof(char)
                     + sizeof(int) + sizeof(short);
    printf("  Suma campos: %zu  padding total: %zu\n\n",
           sum_bad, sizeof(struct Bad) - sum_bad);

    /* Same with struct Good */
    struct Good good;
    memset(&good, 0xAA, sizeof(good));
    good.b = 0.0;
    good.d = 1;
    good.e = 2;
    good.a = 'X';
    good.c = 'Y';

    show_memory_map(&good, sizeof(good), "struct Good");

    printf("Detalle de struct Good:\n");
    printf("  b:  offset=%2zu  size=%zu\n", offsetof(struct Good, b),
           sizeof(good.b));
    printf("  d:  offset=%2zu  size=%zu\n", offsetof(struct Good, d),
           sizeof(good.d));
    printf("  e:  offset=%2zu  size=%zu\n", offsetof(struct Good, e),
           sizeof(good.e));
    printf("  a:  offset=%2zu  size=%zu\n", offsetof(struct Good, a),
           sizeof(good.a));
    printf("  c:  offset=%2zu  size=%zu\n", offsetof(struct Good, c),
           sizeof(good.c));

    size_t sum_good = sizeof(double) + sizeof(int) + sizeof(short)
                      + sizeof(char) + sizeof(char);
    printf("  Suma campos: %zu  padding total: %zu\n\n",
           sum_good, sizeof(struct Good) - sum_good);

    printf("=== Comparacion ===\n\n");
    printf("Mismos 5 campos (%zu bytes de datos reales)\n", sum_bad);
    printf("Bad:  %zu bytes (%zu bytes de padding, %.0f%% desperdicio)\n",
           sizeof(struct Bad), sizeof(struct Bad) - sum_bad,
           100.0 * (double)(sizeof(struct Bad) - sum_bad)
           / (double)sizeof(struct Bad));
    printf("Good: %zu bytes (%zu bytes de padding, %.0f%% desperdicio)\n",
           sizeof(struct Good), sizeof(struct Good) - sum_good,
           100.0 * (double)(sizeof(struct Good) - sum_good)
           / (double)sizeof(struct Good));

    printf("\nEn un array de 100000 structs:\n");
    printf("  Bad[100000]:  %zu bytes (%zu KB)\n",
           100000 * sizeof(struct Bad),
           100000 * sizeof(struct Bad) / 1024);
    printf("  Good[100000]: %zu bytes (%zu KB)\n",
           100000 * sizeof(struct Good),
           100000 * sizeof(struct Good) / 1024);
    printf("  Ahorro: %zu bytes (%zu KB)\n",
           100000 * (sizeof(struct Bad) - sizeof(struct Good)),
           100000 * (sizeof(struct Bad) - sizeof(struct Good)) / 1024);

    return 0;
}
