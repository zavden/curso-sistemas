#include <stdio.h>

int main(void) {
    /* Extension 1: statement expression (GNU extension) */
    int max = ({ int a = 5, b = 3; a > b ? a : b; });

    /* Extension 2: nested function (GNU extension, GCC only) */
    void greet(void) {
        printf("hello from nested function\n");
    }
    greet();

    /* Extension 3: binary literal (GNU extension, standard in C23) */
    int mask = 0b11110000;

    /* Extension 4: zero-length array (GNU extension) */
    struct Packet {
        int length;
        char data[0];
    };

    /* Extension 5: case range (GNU extension) */
    char c = 'f';
    switch (c) {
        case 'a' ... 'z':
            printf("lowercase letter\n");
            break;
        default:
            printf("other character\n");
            break;
    }

    printf("max  = %d\n", max);
    printf("mask = 0x%X\n", mask);
    printf("sizeof(struct Packet) = %zu\n", sizeof(struct Packet));

    return 0;
}
