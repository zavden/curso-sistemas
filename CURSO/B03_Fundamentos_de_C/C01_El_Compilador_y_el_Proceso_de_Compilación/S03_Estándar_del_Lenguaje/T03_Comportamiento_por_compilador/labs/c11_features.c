#include <stdio.h>
#include <stdnoreturn.h>
#include <stdlib.h>

/* C11 feature 1: _Static_assert */
_Static_assert(sizeof(int) >= 4, "int must be at least 4 bytes");
_Static_assert(sizeof(char) == 1, "char must be exactly 1 byte");

/* C11 feature 2: _Noreturn (declared via stdnoreturn.h) */
noreturn void fatal_error(const char *msg) {
    fprintf(stderr, "FATAL: %s\n", msg);
    exit(1);
}

/* C11 feature 3: _Generic */
#define type_name(x) _Generic((x), \
    int:       "int",              \
    float:     "float",            \
    double:    "double",           \
    char *:    "char *",           \
    default:   "unknown"           \
)

int main(void) {
    /* C11 feature 4: anonymous struct/union */
    struct Vector {
        union {
            struct { float x, y, z; };
            float components[3];
        };
    };

    struct Vector v = { .x = 1.0f, .y = 2.0f, .z = 3.0f };

    printf("=== C11 features ===\n");

    /* _Generic in action */
    int i = 42;
    float f = 3.14f;
    double d = 2.718;
    printf("type of i: %s\n", type_name(i));
    printf("type of f: %s\n", type_name(f));
    printf("type of d: %s\n", type_name(d));

    /* _Alignof */
    printf("_Alignof(int)    = %zu\n", _Alignof(int));
    printf("_Alignof(double) = %zu\n", _Alignof(double));
    printf("_Alignof(char)   = %zu\n", _Alignof(char));

    /* anonymous struct access */
    printf("v.x = %.1f, v.y = %.1f, v.z = %.1f\n", v.x, v.y, v.z);
    printf("v.components[0] = %.1f\n", v.components[0]);

    printf("__STDC_VERSION__ = %ldL\n", __STDC_VERSION__);

    return 0;
}
