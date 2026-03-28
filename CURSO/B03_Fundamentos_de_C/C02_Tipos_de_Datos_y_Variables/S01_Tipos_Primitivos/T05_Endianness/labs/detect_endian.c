#include <stdio.h>
#include <stdint.h>

int is_little_endian_cast(void) {
    uint16_t x = 1;
    uint8_t *byte = (uint8_t *)&x;
    return byte[0] == 1;
}

union EndianCheck {
    uint32_t value;
    uint8_t  bytes[4];
};

int is_little_endian_union(void) {
    union EndianCheck check = { .value = 1 };
    return check.bytes[0] == 1;
}

int main(void) {
    printf("=== Deteccion de endianness ===\n\n");

    printf("Metodo 1 (cast a uint8_t*):\n");
    if (is_little_endian_cast()) {
        printf("  Resultado: Little-endian\n");
    } else {
        printf("  Resultado: Big-endian\n");
    }

    printf("\nMetodo 2 (union):\n");
    if (is_little_endian_union()) {
        printf("  Resultado: Little-endian\n");
    } else {
        printf("  Resultado: Big-endian\n");
    }

    printf("\n=== Verificacion con uint16_t ===\n");
    uint16_t test = 0x0001;
    uint8_t *bytes = (uint8_t *)&test;
    printf("Valor: 0x%04X\n", test);
    printf("Byte en direccion baja [0]: 0x%02X\n", bytes[0]);
    printf("Byte en direccion alta [1]: 0x%02X\n", bytes[1]);

    if (bytes[0] == 0x01) {
        printf("El byte menos significativo (01) esta primero -> Little-endian\n");
    } else {
        printf("El byte mas significativo (00) esta primero -> Big-endian\n");
    }

    return 0;
}
