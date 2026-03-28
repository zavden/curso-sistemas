#include <stdio.h>
#include <stdint.h>

void print_bytes(const char *label, void *ptr, size_t size) {
    uint8_t *bytes = (uint8_t *)ptr;
    printf("%-20s  Valor: ", label);

    if (size == 2) {
        printf("0x%04X", *(uint16_t *)ptr);
    } else if (size == 4) {
        printf("0x%08X", *(uint32_t *)ptr);
    }

    printf("  Bytes en memoria: ");
    for (size_t i = 0; i < size; i++) {
        printf("%02X ", bytes[i]);
    }
    printf("\n");
}

int main(void) {
    printf("=== Orden de bytes en memoria ===\n\n");

    uint16_t val16 = 0xABCD;
    uint32_t val32 = 0x01020304;
    uint32_t val32b = 0xDEADBEEF;
    uint32_t val32c = 0x12345678;

    printf("--- uint16_t (2 bytes) ---\n");
    print_bytes("0xABCD", &val16, sizeof(val16));

    printf("\n--- uint32_t (4 bytes) ---\n");
    print_bytes("0x01020304", &val32, sizeof(val32));
    print_bytes("0xDEADBEEF", &val32b, sizeof(val32b));
    print_bytes("0x12345678", &val32c, sizeof(val32c));

    printf("\n--- Analisis detallado de 0x01020304 ---\n");
    uint8_t *bytes = (uint8_t *)&val32;
    printf("Direccion base: %p\n", (void *)&val32);
    for (int i = 0; i < 4; i++) {
        printf("  Offset +%d: 0x%02X", i, bytes[i]);
        if (bytes[i] == 0x04) {
            printf("  (byte menos significativo)");
        } else if (bytes[i] == 0x01) {
            printf("  (byte mas significativo)");
        }
        printf("\n");
    }

    return 0;
}
