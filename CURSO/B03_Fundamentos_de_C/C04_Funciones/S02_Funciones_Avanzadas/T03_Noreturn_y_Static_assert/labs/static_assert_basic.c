#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <stdalign.h>
#include <stdint.h>

/* Platform assumptions */
static_assert(sizeof(char) == 1, "char must be 1 byte");
static_assert(CHAR_BIT == 8, "need 8-bit bytes");
static_assert(sizeof(int) >= 4, "need at least 32-bit ints");

/* Struct layout verification */
struct Packet {
    uint8_t  type;
    uint8_t  flags;
    uint16_t length;
    uint32_t payload;
};
static_assert(sizeof(struct Packet) == 8,
              "Packet must be exactly 8 bytes");

/* Enum-array sync verification */
enum Color { RED, GREEN, BLUE, COLOR_COUNT };
static const char *color_names[] = { "red", "green", "blue" };
static_assert(sizeof(color_names) / sizeof(color_names[0]) == COLOR_COUNT,
              "color_names out of sync with Color enum");

/* Alignment verification */
static_assert(alignof(double) >= 4, "doubles need at least 4-byte alignment");

/* Type relationship */
static_assert(sizeof(int) <= sizeof(long),
              "int must fit in long");

int main(void) {
    printf("All static assertions passed!\n");
    printf("sizeof(char)   = %zu\n", sizeof(char));
    printf("sizeof(int)    = %zu\n", sizeof(int));
    printf("sizeof(long)   = %zu\n", sizeof(long));
    printf("sizeof(Packet) = %zu\n", sizeof(struct Packet));
    printf("alignof(double)= %zu\n", alignof(double));
    printf("COLOR_COUNT    = %d\n", COLOR_COUNT);
    return 0;
}
