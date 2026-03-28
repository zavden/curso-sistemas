#include <stdio.h>

/* --- Enfoque 1: bit fields --- */
struct BFPerms {
    unsigned int read    : 1;
    unsigned int write   : 1;
    unsigned int execute : 1;
};

/* --- Enfoque 2: bitwise con mascaras --- */
#define PERM_READ  (1u << 0)
#define PERM_WRITE (1u << 1)
#define PERM_EXEC  (1u << 2)

static void print_bf_perms(struct BFPerms p) {
    printf("  %c%c%c\n",
           p.read    ? 'r' : '-',
           p.write   ? 'w' : '-',
           p.execute ? 'x' : '-');
}

static void print_bw_perms(unsigned int p) {
    printf("  %c%c%c\n",
           (p & PERM_READ)  ? 'r' : '-',
           (p & PERM_WRITE) ? 'w' : '-',
           (p & PERM_EXEC)  ? 'x' : '-');
}

int main(void) {
    /* --- Bit fields --- */
    printf("=== Bit fields ===\n");

    struct BFPerms bf = { .read = 1, .write = 1, .execute = 0 };
    printf("Inicial (rw-):\n");
    print_bf_perms(bf);

    bf.execute = 1;
    printf("Agregar execute (rwx):\n");
    print_bf_perms(bf);

    bf.write = 0;
    printf("Quitar write (r-x):\n");
    print_bf_perms(bf);

    printf("Verificar read: %s\n\n", bf.read ? "si" : "no");

    /* --- Bitwise --- */
    printf("=== Bitwise con mascaras ===\n");

    unsigned int bw = PERM_READ | PERM_WRITE;
    printf("Inicial (rw-):\n");
    print_bw_perms(bw);

    bw |= PERM_EXEC;
    printf("Agregar execute (rwx):\n");
    print_bw_perms(bw);

    bw &= ~PERM_WRITE;
    printf("Quitar write (r-x):\n");
    print_bw_perms(bw);

    printf("Verificar read: %s\n\n", (bw & PERM_READ) ? "si" : "no");

    /* --- Comparacion de tamano --- */
    printf("=== Tamano ===\n");
    printf("sizeof(struct BFPerms) = %zu\n", sizeof(struct BFPerms));
    printf("sizeof(unsigned int)   = %zu\n", sizeof(unsigned int));

    return 0;
}
