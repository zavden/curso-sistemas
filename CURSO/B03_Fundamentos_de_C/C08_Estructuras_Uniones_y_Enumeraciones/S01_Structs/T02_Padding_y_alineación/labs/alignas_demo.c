#include <stdio.h>
#include <stddef.h>
#include <stdalign.h>
#include <stdint.h>

/*
 * Demonstrates C11 _Alignas / alignas and _Alignof / alignof:
 * - Increasing alignment beyond the natural minimum
 * - Cache line alignment (64 bytes)
 * - Effect on sizeof and arrays
 * - Comparing with __attribute__((aligned))
 */

/* ---------- Basic alignas ---------- */

struct BasicAligned {
    alignas(16) int data[4];
};

/* ---------- Cache line alignment ---------- */

#define CACHE_LINE 64

struct __attribute__((aligned(CACHE_LINE))) CacheAligned {
    int counter;
    char padding_visible[CACHE_LINE - sizeof(int)];
};

/* Simulating a multi-threaded scenario: two counters that
   should NOT share a cache line (false sharing prevention) */
struct __attribute__((aligned(CACHE_LINE))) Counter {
    int value;
};

/* ---------- alignas on struct members ---------- */

struct MemberAligned {
    char  tag;
    alignas(16) int  vector[4];
    char  flag;
};

/* ---------- Over-aligned variable ---------- */

int main(void) {
    printf("=== _Alignof / alignof ===\n\n");
    printf("alignof(char):        %zu\n", alignof(char));
    printf("alignof(short):       %zu\n", alignof(short));
    printf("alignof(int):         %zu\n", alignof(int));
    printf("alignof(double):      %zu\n", alignof(double));
    printf("alignof(long double): %zu\n", alignof(long double));
    printf("alignof(void *):      %zu\n", alignof(void *));
    printf("alignof(max_align_t): %zu\n", alignof(max_align_t));

    printf("\n=== alignas en un struct ===\n\n");
    printf("struct BasicAligned { alignas(16) int data[4]; }\n");
    printf("sizeof:  %zu bytes\n", sizeof(struct BasicAligned));
    printf("alignof: %zu  (forzado a 16, no 4)\n",
           alignof(struct BasicAligned));

    printf("\n=== Alineacion a cache line (64 bytes) ===\n\n");
    printf("struct CacheAligned (aligned(64)):\n");
    printf("sizeof:  %zu bytes\n", sizeof(struct CacheAligned));
    printf("alignof: %zu\n", alignof(struct CacheAligned));

    struct CacheAligned obj;
    printf("Direccion de obj: %p\n", (void *)&obj);
    printf("Multiplo de 64?   %s\n",
           ((uintptr_t)&obj % CACHE_LINE == 0) ? "Si" : "No");

    printf("\n=== Prevencion de false sharing ===\n\n");
    struct Counter counters[2];
    printf("sizeof(Counter): %zu bytes (ocupa un cache line completo)\n",
           sizeof(struct Counter));
    printf("counters[0] en: %p\n", (void *)&counters[0]);
    printf("counters[1] en: %p\n", (void *)&counters[1]);
    ptrdiff_t dist = (char *)&counters[1] - (char *)&counters[0];
    printf("Distancia: %td bytes\n", dist);
    printf("Cada counter en cache line separado? %s\n",
           (dist >= CACHE_LINE) ? "Si" : "No");

    printf("\n=== alignas en un miembro ===\n\n");
    printf("struct MemberAligned { char tag; alignas(16) int vector[4]; "
           "char flag; }\n");
    printf("sizeof:  %zu bytes\n", sizeof(struct MemberAligned));
    printf("alignof: %zu\n", alignof(struct MemberAligned));
    printf("  tag:    offset=%zu   size=%zu\n",
           offsetof(struct MemberAligned, tag), sizeof(char));
    printf("  vector: offset=%zu  size=%zu  (forzado a multiplo de 16)\n",
           offsetof(struct MemberAligned, vector), sizeof(int[4]));
    printf("  flag:   offset=%zu  size=%zu\n",
           offsetof(struct MemberAligned, flag), sizeof(char));

    printf("\n=== Variable local con alignas ===\n\n");
    alignas(32) char aligned_buffer[128];
    printf("aligned_buffer (alignas(32)):\n");
    printf("Direccion: %p\n", (void *)aligned_buffer);
    printf("Multiplo de 32? %s\n",
           ((uintptr_t)aligned_buffer % 32 == 0) ? "Si" : "No");

    return 0;
}
