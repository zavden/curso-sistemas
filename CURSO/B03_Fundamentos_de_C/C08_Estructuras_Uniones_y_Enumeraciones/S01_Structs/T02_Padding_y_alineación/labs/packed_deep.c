#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdalign.h>

/*
 * Explores __attribute__((packed)) in depth:
 * - Packed vs normal layout
 * - Packing individual fields
 * - Simulating a network protocol header with static_assert
 * - Serialization to a byte buffer
 */

/* ---------- Normal vs Packed ---------- */

struct SensorNormal {
    uint8_t  id;
    uint32_t timestamp;
    uint16_t value;
    uint8_t  status;
};

struct __attribute__((packed)) SensorPacked {
    uint8_t  id;
    uint32_t timestamp;
    uint16_t value;
    uint8_t  status;
};

/* ---------- Network protocol header ---------- */

struct __attribute__((packed)) NetHeader {
    uint8_t  version;
    uint8_t  type;
    uint16_t length;
    uint32_t sequence;
    uint32_t checksum;
};

_Static_assert(sizeof(struct NetHeader) == 12,
               "NetHeader must be exactly 12 bytes");
_Static_assert(offsetof(struct NetHeader, version)  == 0,  "version at 0");
_Static_assert(offsetof(struct NetHeader, type)     == 1,  "type at 1");
_Static_assert(offsetof(struct NetHeader, length)   == 2,  "length at 2");
_Static_assert(offsetof(struct NetHeader, sequence) == 4,  "sequence at 4");
_Static_assert(offsetof(struct NetHeader, checksum) == 8,  "checksum at 8");

/* ---------- Selective packing (GCC extension) ---------- */

struct Selective {
    char   tag;
    int    value __attribute__((packed));
    double score;
};

static void print_layout_comparison(void) {
    printf("=== Normal vs Packed: struct Sensor ===\n\n");

    printf("SensorNormal: %zu bytes  (alignof %zu)\n",
           sizeof(struct SensorNormal), alignof(struct SensorNormal));
    printf("  id:        offset=%zu  size=%zu\n",
           offsetof(struct SensorNormal, id), sizeof(uint8_t));
    printf("  timestamp: offset=%zu  size=%zu\n",
           offsetof(struct SensorNormal, timestamp), sizeof(uint32_t));
    printf("  value:     offset=%zu  size=%zu\n",
           offsetof(struct SensorNormal, value), sizeof(uint16_t));
    printf("  status:    offset=%zu  size=%zu\n",
           offsetof(struct SensorNormal, status), sizeof(uint8_t));

    printf("\nSensorPacked: %zu bytes  (alignof %zu)\n",
           sizeof(struct SensorPacked), alignof(struct SensorPacked));
    printf("  id:        offset=%zu  size=%zu\n",
           offsetof(struct SensorPacked, id), sizeof(uint8_t));
    printf("  timestamp: offset=%zu  size=%zu\n",
           offsetof(struct SensorPacked, timestamp), sizeof(uint32_t));
    printf("  value:     offset=%zu  size=%zu\n",
           offsetof(struct SensorPacked, value), sizeof(uint16_t));
    printf("  status:    offset=%zu  size=%zu\n",
           offsetof(struct SensorPacked, status), sizeof(uint8_t));

    size_t sum = sizeof(uint8_t) + sizeof(uint32_t) +
                 sizeof(uint16_t) + sizeof(uint8_t);
    printf("\nSuma real de campos: %zu bytes\n", sum);
    printf("Normal desperdicia: %zu bytes de padding\n",
           sizeof(struct SensorNormal) - sum);
    printf("Packed desperdicia: %zu bytes de padding\n\n",
           sizeof(struct SensorPacked) - sum);
}

static void print_protocol_header(void) {
    printf("=== Protocolo de red: layout exacto con packed ===\n\n");

    printf("NetHeader: %zu bytes\n", sizeof(struct NetHeader));
    printf("  version:  offset=%zu  size=%zu\n",
           offsetof(struct NetHeader, version), sizeof(uint8_t));
    printf("  type:     offset=%zu  size=%zu\n",
           offsetof(struct NetHeader, type), sizeof(uint8_t));
    printf("  length:   offset=%zu  size=%zu\n",
           offsetof(struct NetHeader, length), sizeof(uint16_t));
    printf("  sequence: offset=%zu  size=%zu\n",
           offsetof(struct NetHeader, sequence), sizeof(uint32_t));
    printf("  checksum: offset=%zu  size=%zu\n",
           offsetof(struct NetHeader, checksum), sizeof(uint32_t));

    /* Serialize to a byte buffer */
    struct NetHeader hdr = {
        .version  = 1,
        .type     = 0x02,
        .length   = 64,
        .sequence = 1001,
        .checksum = 0xDEADBEEF
    };

    uint8_t buffer[sizeof(struct NetHeader)];
    memcpy(buffer, &hdr, sizeof(hdr));

    printf("\nSerializacion con memcpy (byte por byte):\n");
    for (size_t i = 0; i < sizeof(buffer); i++) {
        printf("  byte[%2zu] = 0x%02X\n", i, buffer[i]);
    }

    /* Deserialize back */
    struct NetHeader hdr2;
    memcpy(&hdr2, buffer, sizeof(hdr2));
    printf("\nDeserializacion:\n");
    printf("  version=%u  type=0x%02X  length=%u  seq=%u  checksum=0x%X\n",
           hdr2.version, hdr2.type, hdr2.length,
           hdr2.sequence, hdr2.checksum);
    printf("\n");
}

static void print_selective_packing(void) {
    printf("=== Packing selectivo: solo un campo ===\n\n");
    printf("struct Selective { char tag; int value(packed); double score; }\n");
    printf("sizeof:  %zu bytes  (alignof %zu)\n",
           sizeof(struct Selective), alignof(struct Selective));
    printf("  tag:   offset=%zu  size=%zu\n",
           offsetof(struct Selective, tag), sizeof(char));
    printf("  value: offset=%zu  size=%zu  (packed: sin padding previo)\n",
           offsetof(struct Selective, value), sizeof(int));
    printf("  score: offset=%zu  size=%zu\n",
           offsetof(struct Selective, score), sizeof(double));

    printf("\nSin packing selectivo, value estaria en offset 4.\n");
    printf("Con packing selectivo, value esta en offset %zu.\n",
           offsetof(struct Selective, value));
}

int main(void) {
    print_layout_comparison();
    print_protocol_header();
    print_selective_packing();

    return 0;
}
