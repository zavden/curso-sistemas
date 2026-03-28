#include <stdio.h>
#include <stddef.h>
#include <stdalign.h>

struct Normal {
    char  a;
    int   b;
    char  c;
};

struct __attribute__((packed)) Packed {
    char  a;
    int   b;
    char  c;
};

struct Header {
    char     magic[2];
    int      version;
    short    flags;
    double   timestamp;
    char     type;
};

struct __attribute__((packed)) PackedHeader {
    char     magic[2];
    int      version;
    short    flags;
    double   timestamp;
    char     type;
};

int main(void) {
    printf("=== Normal vs Packed ===\n");
    printf("struct Normal: %zu bytes\n", sizeof(struct Normal));
    printf("  a: offset=%zu\n", offsetof(struct Normal, a));
    printf("  b: offset=%zu\n", offsetof(struct Normal, b));
    printf("  c: offset=%zu\n", offsetof(struct Normal, c));

    printf("\nstruct Packed: %zu bytes\n", sizeof(struct Packed));
    printf("  a: offset=%zu\n", offsetof(struct Packed, a));
    printf("  b: offset=%zu\n", offsetof(struct Packed, b));
    printf("  c: offset=%zu\n", offsetof(struct Packed, c));

    printf("\n=== Caso practico: Header de protocolo ===\n");
    printf("struct Header (normal):  %zu bytes\n", sizeof(struct Header));
    printf("  magic:     offset=%2zu  size=%zu\n",
           offsetof(struct Header, magic), sizeof(char[2]));
    printf("  version:   offset=%2zu  size=%zu\n",
           offsetof(struct Header, version), sizeof(int));
    printf("  flags:     offset=%2zu  size=%zu\n",
           offsetof(struct Header, flags), sizeof(short));
    printf("  timestamp: offset=%2zu  size=%zu\n",
           offsetof(struct Header, timestamp), sizeof(double));
    printf("  type:      offset=%2zu  size=%zu\n",
           offsetof(struct Header, type), sizeof(char));

    printf("\nstruct PackedHeader:     %zu bytes\n", sizeof(struct PackedHeader));
    printf("  magic:     offset=%2zu  size=%zu\n",
           offsetof(struct PackedHeader, magic), sizeof(char[2]));
    printf("  version:   offset=%2zu  size=%zu\n",
           offsetof(struct PackedHeader, version), sizeof(int));
    printf("  flags:     offset=%2zu  size=%zu\n",
           offsetof(struct PackedHeader, flags), sizeof(short));
    printf("  timestamp: offset=%2zu  size=%zu\n",
           offsetof(struct PackedHeader, timestamp), sizeof(double));
    printf("  type:      offset=%2zu  size=%zu\n",
           offsetof(struct PackedHeader, type), sizeof(char));

    size_t sum = sizeof(char[2]) + sizeof(int) + sizeof(short)
               + sizeof(double) + sizeof(char);
    printf("\nSuma real de campos: %zu bytes\n", sum);
    printf("Header normal:       %zu bytes (padding: %zu)\n",
           sizeof(struct Header), sizeof(struct Header) - sum);
    printf("PackedHeader:        %zu bytes (padding: %zu)\n",
           sizeof(struct PackedHeader), sizeof(struct PackedHeader) - sum);

    printf("\n=== Peligro: puntero a campo desalineado ===\n");
    struct Packed p = { .a = 'X', .b = 42, .c = 'Y' };
    printf("p.b = %d (acceso directo: OK)\n", p.b);
    printf("Direccion de p.a:   %p\n", (void *)&p.a);
    printf("Direccion de p.b:   %p\n", (void *)&p.b);
    printf("Direccion de p.c:   %p\n", (void *)&p.c);
    printf("p.b esta en direccion multiplo de 4? %s\n",
           ((unsigned long)&p.b % 4 == 0) ? "Si" : "No");

    return 0;
}
