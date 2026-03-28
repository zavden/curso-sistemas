#include <stdio.h>
#include <string.h>

enum ValueType {
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_BOOL,
};

struct TaggedValue {
    enum ValueType type;
    union {
        int i;
        float f;
        char s[32];
        _Bool b;
    } data;
};

void print_value(const struct TaggedValue *v) {
    switch (v->type) {
        case VAL_INT:
            printf("int: %d\n", v->data.i);
            break;
        case VAL_FLOAT:
            printf("float: %.2f\n", v->data.f);
            break;
        case VAL_STRING:
            printf("string: \"%s\"\n", v->data.s);
            break;
        case VAL_BOOL:
            printf("bool: %s\n", v->data.b ? "true" : "false");
            break;
    }
}

struct TaggedValue make_int(int n) {
    struct TaggedValue v = { .type = VAL_INT, .data.i = n };
    return v;
}

struct TaggedValue make_float(float f) {
    struct TaggedValue v = { .type = VAL_FLOAT, .data.f = f };
    return v;
}

struct TaggedValue make_string(const char *s) {
    struct TaggedValue v = { .type = VAL_STRING };
    strncpy(v.data.s, s, sizeof(v.data.s) - 1);
    v.data.s[sizeof(v.data.s) - 1] = '\0';
    return v;
}

struct TaggedValue make_bool(_Bool b) {
    struct TaggedValue v = { .type = VAL_BOOL, .data.b = b };
    return v;
}

int main(void) {
    struct TaggedValue values[] = {
        make_int(42),
        make_float(3.14f),
        make_string("hello unions"),
        make_bool(1),
        make_int(-7),
    };

    int count = (int)(sizeof(values) / sizeof(values[0]));

    printf("=== tagged union: safe dispatch ===\n");
    for (int i = 0; i < count; i++) {
        printf("[%d] ", i);
        print_value(&values[i]);
    }

    printf("\n=== sizeof analysis ===\n");
    printf("sizeof(enum ValueType)    = %zu\n", sizeof(enum ValueType));
    printf("sizeof(union data)        = %zu\n", sizeof(values[0].data));
    printf("sizeof(struct TaggedValue) = %zu\n", sizeof(struct TaggedValue));

    return 0;
}
