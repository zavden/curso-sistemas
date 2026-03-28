#include <stdio.h>
#include <stdint.h>
#include <arpa/inet.h>

void print_bytes_raw(void *ptr, size_t size) {
    uint8_t *bytes = (uint8_t *)ptr;
    for (size_t i = 0; i < size; i++) {
        printf("%02X ", bytes[i]);
    }
}

int main(void) {
    printf("=== Funciones de conversion host <-> network ===\n\n");

    /* --- htons / ntohs (16 bits) --- */
    printf("--- htons / ntohs (16 bits) ---\n\n");

    uint16_t port = 8080;
    uint16_t net_port = htons(port);
    uint16_t back_port = ntohs(net_port);

    printf("Puerto original:  %u (0x%04X)\n", port, port);
    printf("  Bytes host:     ");
    print_bytes_raw(&port, sizeof(port));
    printf("\n");

    printf("Despues de htons: %u (0x%04X)\n", net_port, net_port);
    printf("  Bytes network:  ");
    print_bytes_raw(&net_port, sizeof(net_port));
    printf("\n");

    printf("Despues de ntohs: %u (0x%04X)\n", back_port, back_port);
    printf("  Bytes host:     ");
    print_bytes_raw(&back_port, sizeof(back_port));
    printf("\n");

    /* --- htonl / ntohl (32 bits) --- */
    printf("\n--- htonl / ntohl (32 bits) ---\n\n");

    uint32_t addr = 0xC0A80001;  /* 192.168.0.1 */
    uint32_t net_addr = htonl(addr);
    uint32_t back_addr = ntohl(net_addr);

    printf("Direccion original:  0x%08X (192.168.0.1)\n", addr);
    printf("  Bytes host:        ");
    print_bytes_raw(&addr, sizeof(addr));
    printf("\n");

    printf("Despues de htonl:    0x%08X\n", net_addr);
    printf("  Bytes network:     ");
    print_bytes_raw(&net_addr, sizeof(net_addr));
    printf("\n");

    printf("Despues de ntohl:    0x%08X\n", back_addr);
    printf("  Bytes host:        ");
    print_bytes_raw(&back_addr, sizeof(back_addr));
    printf("\n");

    /* --- Verificar ida y vuelta --- */
    printf("\n--- Verificacion ida y vuelta ---\n\n");

    uint32_t test = 0xDEADBEEF;
    printf("Original:              0x%08X\n", test);
    printf("ntohl(htonl(x)):       0x%08X\n", ntohl(htonl(test)));
    printf("Coinciden: %s\n", (ntohl(htonl(test)) == test) ? "SI" : "NO");

    return 0;
}
