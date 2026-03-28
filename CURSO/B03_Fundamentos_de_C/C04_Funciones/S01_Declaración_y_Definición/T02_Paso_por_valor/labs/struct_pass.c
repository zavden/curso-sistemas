#include <stdio.h>
#include <string.h>

struct SmallPoint {
    int x;
    int y;
};

struct LargeBuffer {
    char data[4096];
    int length;
};

void modify_point_value(struct SmallPoint p) {
    p.x = 999;
    p.y = 999;
    printf("  Inside (value): p = (%d, %d)\n", p.x, p.y);
}

void modify_point_ptr(struct SmallPoint *p) {
    p->x = 999;
    p->y = 999;
    printf("  Inside (ptr):   p = (%d, %d)\n", p->x, p->y);
}

void fill_buffer_value(struct LargeBuffer buf) {
    (void)buf;  /* suppress unused warning */
    printf("  sizeof(buf) inside (value): %zu bytes\n", sizeof(buf));
}

void fill_buffer_ptr(const struct LargeBuffer *buf) {
    printf("  sizeof(buf) inside (ptr):   %zu bytes (pointer size)\n",
           sizeof(buf));
    printf("  sizeof(*buf) inside (ptr):  %zu bytes (struct size)\n",
           sizeof(*buf));
    printf("  buf->length = %d\n", buf->length);
}

int main(void) {
    /* --- SmallPoint: value vs pointer --- */
    struct SmallPoint pt = {10, 20};

    printf("=== SmallPoint (sizeof = %zu bytes) ===\n", sizeof(pt));
    printf("Before: pt = (%d, %d)\n", pt.x, pt.y);

    modify_point_value(pt);
    printf("After modify_point_value: pt = (%d, %d)\n", pt.x, pt.y);

    modify_point_ptr(&pt);
    printf("After modify_point_ptr:   pt = (%d, %d)\n", pt.x, pt.y);

    /* --- LargeBuffer: cost of copying --- */
    struct LargeBuffer big;
    memset(big.data, 'A', sizeof(big.data));
    big.length = 4096;

    printf("\n=== LargeBuffer (sizeof = %zu bytes) ===\n", sizeof(big));

    fill_buffer_value(big);
    fill_buffer_ptr(&big);

    return 0;
}
