#include <stdio.h>
#include <stdint.h>

#define PERM_R_USR  (1U << 8)
#define PERM_W_USR  (1U << 7)
#define PERM_X_USR  (1U << 6)
#define PERM_R_GRP  (1U << 5)
#define PERM_W_GRP  (1U << 4)
#define PERM_X_GRP  (1U << 3)
#define PERM_R_OTH  (1U << 2)
#define PERM_W_OTH  (1U << 1)
#define PERM_X_OTH  (1U << 0)

void print_perm_bits(unsigned int perms) {
    printf("  0%03o = ", perms);
    for (int i = 8; i >= 0; i--) {
        printf("%d", (perms >> i) & 1);
        if (i % 3 == 0 && i > 0) printf(" ");
    }
    printf("  ");
    printf("%c%c%c",
           (perms & PERM_R_USR) ? 'r' : '-',
           (perms & PERM_W_USR) ? 'w' : '-',
           (perms & PERM_X_USR) ? 'x' : '-');
    printf("%c%c%c",
           (perms & PERM_R_GRP) ? 'r' : '-',
           (perms & PERM_W_GRP) ? 'w' : '-',
           (perms & PERM_X_GRP) ? 'x' : '-');
    printf("%c%c%c",
           (perms & PERM_R_OTH) ? 'r' : '-',
           (perms & PERM_W_OTH) ? 'w' : '-',
           (perms & PERM_X_OTH) ? 'x' : '-');
    printf("\n");
}

int main(void) {
    printf("=== Building permissions 0755 (rwxr-xr-x) ===\n\n");
    unsigned int perms = 0;

    printf("Start empty:\n");
    print_perm_bits(perms);

    printf("\nSet owner: rwx\n");
    perms |= PERM_R_USR | PERM_W_USR | PERM_X_USR;
    print_perm_bits(perms);

    printf("\nSet group: r-x\n");
    perms |= PERM_R_GRP | PERM_X_GRP;
    print_perm_bits(perms);

    printf("\nSet other: r-x\n");
    perms |= PERM_R_OTH | PERM_X_OTH;
    print_perm_bits(perms);

    printf("\n=== Testing individual permissions ===\n");
    printf("Owner can read?    %s\n", (perms & PERM_R_USR) ? "yes" : "no");
    printf("Owner can write?   %s\n", (perms & PERM_W_USR) ? "yes" : "no");
    printf("Group can write?   %s\n", (perms & PERM_W_GRP) ? "yes" : "no");
    printf("Other can execute? %s\n", (perms & PERM_X_OTH) ? "yes" : "no");

    printf("\n=== Removing owner write (0755 -> 0555) ===\n");
    perms &= ~PERM_W_USR;
    print_perm_bits(perms);

    printf("\n=== Adding group write (0555 -> 0575) ===\n");
    perms |= PERM_W_GRP;
    print_perm_bits(perms);

    printf("\n=== Common permission patterns ===\n");
    unsigned int p644 = PERM_R_USR | PERM_W_USR | PERM_R_GRP | PERM_R_OTH;
    unsigned int p755 = PERM_R_USR | PERM_W_USR | PERM_X_USR
                      | PERM_R_GRP | PERM_X_GRP
                      | PERM_R_OTH | PERM_X_OTH;
    unsigned int p700 = PERM_R_USR | PERM_W_USR | PERM_X_USR;

    printf("0644: ");
    print_perm_bits(p644);
    printf("0755: ");
    print_perm_bits(p755);
    printf("0700: ");
    print_perm_bits(p700);

    return 0;
}
