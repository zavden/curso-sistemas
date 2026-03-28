#include <stdio.h>
#include <stddef.h>

union Value {
    int i;
    float f;
    char c;
    double d;
};

struct ValueStruct {
    int i;
    float f;
    char c;
    double d;
};

int main(void) {
    printf("=== sizeof each member ===\n");
    printf("sizeof(int)    = %zu\n", sizeof(int));
    printf("sizeof(float)  = %zu\n", sizeof(float));
    printf("sizeof(char)   = %zu\n", sizeof(char));
    printf("sizeof(double) = %zu\n", sizeof(double));

    printf("\n=== sizeof union vs struct ===\n");
    printf("sizeof(union Value)       = %zu\n", sizeof(union Value));
    printf("sizeof(struct ValueStruct) = %zu\n", sizeof(struct ValueStruct));

    printf("\n=== offsets inside the union ===\n");
    printf("offset of i = %zu\n", offsetof(union Value, i));
    printf("offset of f = %zu\n", offsetof(union Value, f));
    printf("offset of c = %zu\n", offsetof(union Value, c));
    printf("offset of d = %zu\n", offsetof(union Value, d));

    printf("\n=== writing to one member overwrites others ===\n");
    union Value v;
    v.i = 42;
    printf("wrote v.i = 42\n");
    printf("  v.i = %d\n", v.i);

    v.f = 3.14f;
    printf("wrote v.f = 3.14\n");
    printf("  v.f = %f\n", v.f);
    printf("  v.i = %d  (garbage -- overwritten by v.f)\n", v.i);

    return 0;
}
