#include <stdio.h>
#include <stddef.h>
#include <stdalign.h>

/*
 * Demonstrates the three alignment rules in detail:
 *   Rule 1: Each field aligns to a multiple of its natural size
 *   Rule 2: Struct alignment = largest member alignment
 *   Rule 3: sizeof(struct) is a multiple of struct alignment (tail padding)
 */

struct RuleDemo1 {
    char   a;      /* 1 byte  */
    short  b;      /* 2 bytes */
    char   c;      /* 1 byte  */
    int    d;      /* 4 bytes */
};

struct RuleDemo2 {
    char   a;      /* 1 byte  */
    double b;      /* 8 bytes */
    int    c;      /* 4 bytes */
};

struct TailPad {
    double d;      /* 8 bytes */
    int    i;      /* 4 bytes */
    char   c;      /* 1 byte  */
};

struct NoTailPad {
    double d;      /* 8 bytes */
    int    i;      /* 4 bytes */
    int    j;      /* 4 bytes */
};

static void print_rule1(void) {
    printf("=== Regla 1: cada campo alineado a su tamano ===\n\n");
    printf("struct RuleDemo1 { char a; short b; char c; int d; }\n");
    printf("sizeof:  %zu bytes\n", sizeof(struct RuleDemo1));
    printf("alignof: %zu\n\n", alignof(struct RuleDemo1));
    printf("  a: offset=%zu  size=%zu  (align 1 -- cualquier offset)\n",
           offsetof(struct RuleDemo1, a), sizeof(char));
    printf("  b: offset=%zu  size=%zu  (align 2 -- necesita offset par)\n",
           offsetof(struct RuleDemo1, b), sizeof(short));
    printf("  c: offset=%zu  size=%zu  (align 1)\n",
           offsetof(struct RuleDemo1, c), sizeof(char));
    printf("  d: offset=%zu  size=%zu  (align 4 -- necesita multiplo de 4)\n",
           offsetof(struct RuleDemo1, d), sizeof(int));
    printf("\n");
}

static void print_rule2(void) {
    printf("=== Regla 2: alineacion del struct = mayor miembro ===\n\n");
    printf("struct RuleDemo2 { char a; double b; int c; }\n");
    printf("sizeof:  %zu bytes\n", sizeof(struct RuleDemo2));
    printf("alignof: %zu  (double es el miembro con mayor alineacion)\n\n",
           alignof(struct RuleDemo2));
    printf("  a: offset=%zu   size=%zu\n",
           offsetof(struct RuleDemo2, a), sizeof(char));
    printf("  b: offset=%zu   size=%zu  (7 bytes de padding antes)\n",
           offsetof(struct RuleDemo2, b), sizeof(double));
    printf("  c: offset=%zu  size=%zu\n",
           offsetof(struct RuleDemo2, c), sizeof(int));
    printf("\n");
}

static void print_rule3(void) {
    printf("=== Regla 3: sizeof es multiplo de la alineacion (tail padding) ===\n\n");

    printf("struct TailPad { double d; int i; char c; }\n");
    printf("sizeof:  %zu bytes  (alineacion %zu, necesita multiplo de %zu)\n",
           sizeof(struct TailPad), alignof(struct TailPad),
           alignof(struct TailPad));
    printf("  d: offset=%zu   size=%zu\n",
           offsetof(struct TailPad, d), sizeof(double));
    printf("  i: offset=%zu   size=%zu\n",
           offsetof(struct TailPad, i), sizeof(int));
    printf("  c: offset=%zu  size=%zu\n",
           offsetof(struct TailPad, c), sizeof(char));
    size_t sum1 = sizeof(double) + sizeof(int) + sizeof(char);
    printf("Suma de campos: %zu  --  tail padding: %zu bytes\n\n",
           sum1, sizeof(struct TailPad) - sum1);

    printf("struct NoTailPad { double d; int i; int j; }\n");
    printf("sizeof:  %zu bytes  (alineacion %zu)\n",
           sizeof(struct NoTailPad), alignof(struct NoTailPad));
    printf("  d: offset=%zu   size=%zu\n",
           offsetof(struct NoTailPad, d), sizeof(double));
    printf("  i: offset=%zu   size=%zu\n",
           offsetof(struct NoTailPad, i), sizeof(int));
    printf("  j: offset=%zu  size=%zu\n",
           offsetof(struct NoTailPad, j), sizeof(int));
    size_t sum2 = sizeof(double) + sizeof(int) + sizeof(int);
    printf("Suma de campos: %zu  --  tail padding: %zu bytes\n\n",
           sum2, sizeof(struct NoTailPad) - sum2);

    printf("=== Por que importa el tail padding: arrays ===\n\n");
    struct TailPad arr[3];
    printf("TailPad arr[3]:\n");
    for (int k = 0; k < 3; k++) {
        printf("  arr[%d] empieza en offset %zu (multiplo de %zu? %s)\n",
               k,
               (size_t)((char *)&arr[k] - (char *)&arr[0]),
               alignof(struct TailPad),
               ((size_t)((char *)&arr[k] - (char *)&arr[0])
                % alignof(struct TailPad) == 0) ? "Si" : "No");
    }
    printf("\nSin tail padding, arr[1].d empezaria en offset 13,\n");
    printf("que NO es multiplo de 8 -- violaria la alineacion del double.\n");
}

int main(void) {
    print_rule1();
    print_rule2();
    print_rule3();

    return 0;
}
