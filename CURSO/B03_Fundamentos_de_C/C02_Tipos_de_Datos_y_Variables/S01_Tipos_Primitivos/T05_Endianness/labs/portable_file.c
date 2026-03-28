#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

/* Write a uint32_t in big-endian (network byte order) */
void write_uint32_be(FILE *f, uint32_t val) {
    uint8_t bytes[4];
    bytes[0] = (val >> 24) & 0xFF;
    bytes[1] = (val >> 16) & 0xFF;
    bytes[2] = (val >> 8)  & 0xFF;
    bytes[3] =  val        & 0xFF;
    fwrite(bytes, 4, 1, f);
}

/* Read a uint32_t stored in big-endian */
uint32_t read_uint32_be(FILE *f) {
    uint8_t bytes[4];
    fread(bytes, 4, 1, f);
    return ((uint32_t)bytes[0] << 24) |
           ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8)  |
           ((uint32_t)bytes[3]);
}

/* Write a uint16_t in big-endian */
void write_uint16_be(FILE *f, uint16_t val) {
    uint8_t bytes[2];
    bytes[0] = (val >> 8) & 0xFF;
    bytes[1] =  val       & 0xFF;
    fwrite(bytes, 2, 1, f);
}

/* Read a uint16_t stored in big-endian */
uint16_t read_uint16_be(FILE *f) {
    uint8_t bytes[2];
    fread(bytes, 2, 1, f);
    return ((uint16_t)bytes[0] << 8) |
           ((uint16_t)bytes[1]);
}

int main(void) {
    const char *filename = "data.bin";

    /* --- Write --- */
    printf("=== Escritura portable (big-endian) ===\n\n");

    uint32_t values32[] = { 0x01020304, 0xDEADBEEF, 0x00000001 };
    uint16_t values16[] = { 0xABCD, 0x1F90 };
    int n32 = 3;
    int n16 = 2;

    FILE *f = fopen(filename, "wb");
    if (!f) {
        perror("fopen write");
        return 1;
    }

    for (int i = 0; i < n32; i++) {
        write_uint32_be(f, values32[i]);
        printf("Escrito uint32: 0x%08X\n", values32[i]);
    }
    for (int i = 0; i < n16; i++) {
        write_uint16_be(f, values16[i]);
        printf("Escrito uint16: 0x%04X\n", values16[i]);
    }
    fclose(f);

    printf("\nArchivo '%s' creado.\n", filename);

    /* --- Read back --- */
    printf("\n=== Lectura portable ===\n\n");

    f = fopen(filename, "rb");
    if (!f) {
        perror("fopen read");
        return 1;
    }

    printf("Leidos uint32:\n");
    for (int i = 0; i < n32; i++) {
        uint32_t val = read_uint32_be(f);
        printf("  [%d] 0x%08X  %s\n", i, val,
               (val == values32[i]) ? "(correcto)" : "(ERROR)");
    }
    printf("Leidos uint16:\n");
    for (int i = 0; i < n16; i++) {
        uint16_t val = read_uint16_be(f);
        printf("  [%d] 0x%04X  %s\n", i, val,
               (val == values16[i]) ? "(correcto)" : "(ERROR)");
    }
    fclose(f);

    return 0;
}
