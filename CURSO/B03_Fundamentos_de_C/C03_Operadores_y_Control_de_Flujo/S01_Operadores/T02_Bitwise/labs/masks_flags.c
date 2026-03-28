#include <stdio.h>
#include <stdint.h>

#define FLAG_READ    (1U << 0)
#define FLAG_WRITE   (1U << 1)
#define FLAG_EXEC    (1U << 2)
#define FLAG_HIDDEN  (1U << 3)

void print_bits_8(uint8_t value) {
    for (int i = 7; i >= 0; i--) {
        printf("%d", (value >> i) & 1);
        if (i == 4) printf(" ");
    }
}

void print_flags(unsigned int flags) {
    printf("  flags = 0x%02X = ", flags);
    print_bits_8((uint8_t)flags);
    printf("  [%c%c%c%c]\n",
           (flags & FLAG_HIDDEN) ? 'H' : '-',
           (flags & FLAG_EXEC)   ? 'X' : '-',
           (flags & FLAG_WRITE)  ? 'W' : '-',
           (flags & FLAG_READ)   ? 'R' : '-');
}

int main(void) {
    unsigned int flags = 0;

    printf("=== SET bits (|=) ===\n");
    printf("Initial:\n");
    print_flags(flags);

    printf("\nSet READ:\n");
    flags |= FLAG_READ;
    print_flags(flags);

    printf("\nSet WRITE:\n");
    flags |= FLAG_WRITE;
    print_flags(flags);

    printf("\nSet EXEC:\n");
    flags |= FLAG_EXEC;
    print_flags(flags);

    printf("\n=== CHECK bits (&) ===\n");
    printf("Has READ?   %s\n", (flags & FLAG_READ)   ? "yes" : "no");
    printf("Has WRITE?  %s\n", (flags & FLAG_WRITE)  ? "yes" : "no");
    printf("Has HIDDEN? %s\n", (flags & FLAG_HIDDEN) ? "yes" : "no");

    printf("\n=== CLEAR bits (&= ~) ===\n");
    printf("Clear WRITE:\n");
    flags &= ~FLAG_WRITE;
    print_flags(flags);
    printf("Has WRITE?  %s\n", (flags & FLAG_WRITE) ? "yes" : "no");

    printf("\n=== TOGGLE bits (^=) ===\n");
    printf("Toggle HIDDEN (was off -> on):\n");
    flags ^= FLAG_HIDDEN;
    print_flags(flags);

    printf("\nToggle HIDDEN (was on -> off):\n");
    flags ^= FLAG_HIDDEN;
    print_flags(flags);

    printf("\n=== Combining multiple flags ===\n");
    flags = FLAG_READ | FLAG_WRITE | FLAG_EXEC;
    printf("Set READ | WRITE | EXEC:\n");
    print_flags(flags);

    printf("\nCheck if has BOTH read AND write:\n");
    unsigned int rw = FLAG_READ | FLAG_WRITE;
    if ((flags & rw) == rw) {
        printf("  -> yes, has both READ and WRITE\n");
    }

    printf("\nCheck if has AT LEAST ONE of read or hidden:\n");
    if (flags & (FLAG_READ | FLAG_HIDDEN)) {
        printf("  -> yes, has at least one\n");
    }

    return 0;
}
