/*
 * build_info.c — Incrustar informacion de build en el binario.
 *
 * Demuestra __DATE__, __TIME__, __STDC_VERSION__, __GNUC__,
 * __SIZEOF_POINTER__, __BYTE_ORDER__ y deteccion de plataforma.
 */

#include <stdio.h>

#define BUILD_TIMESTAMP "Built on " __DATE__ " at " __TIME__

static const char *get_std_name(void) {
#if __STDC_VERSION__ >= 202311L
    return "C23";
#elif __STDC_VERSION__ >= 201710L
    return "C17";
#elif __STDC_VERSION__ >= 201112L
    return "C11";
#elif __STDC_VERSION__ >= 199901L
    return "C99";
#else
    return "C89/C90";
#endif
}

static void print_build_info(void) {
    printf("=== Build Information ===\n\n");

    /* Compiler */
#if defined(__clang__)
    printf("Compiler    : Clang %d.%d.%d\n",
           __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
    printf("Compiler    : GCC %d.%d.%d\n",
           __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
    printf("Compiler    : unknown\n");
#endif

    /* Standard */
    printf("C Standard  : %s (__STDC_VERSION__ = %ld)\n",
           get_std_name(), __STDC_VERSION__);
    printf("__STDC__    : %d\n", __STDC__);

    /* Platform */
#if defined(__linux__)
    printf("Platform    : Linux\n");
#elif defined(__APPLE__)
    printf("Platform    : macOS\n");
#elif defined(_WIN32)
    printf("Platform    : Windows\n");
#else
    printf("Platform    : unknown\n");
#endif

    /* Architecture */
#if defined(__x86_64__)
    printf("Architecture: x86-64\n");
#elif defined(__aarch64__)
    printf("Architecture: ARM64\n");
#elif defined(__i386__)
    printf("Architecture: x86 (32-bit)\n");
#else
    printf("Architecture: unknown\n");
#endif

    /* Pointer and type sizes (GCC/Clang extension) */
#ifdef __SIZEOF_POINTER__
    printf("Pointer size: %d bytes (%d-bit)\n",
           __SIZEOF_POINTER__, __SIZEOF_POINTER__ * 8);
#endif
#ifdef __SIZEOF_INT__
    printf("int size    : %d bytes\n", __SIZEOF_INT__);
#endif
#ifdef __SIZEOF_LONG__
    printf("long size   : %d bytes\n", __SIZEOF_LONG__);
#endif

    /* Endianness (GCC/Clang extension) */
#if defined(__BYTE_ORDER__)
  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    printf("Byte order  : little-endian\n");
  #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    printf("Byte order  : big-endian\n");
  #endif
#endif

    /* Build type */
#ifdef NDEBUG
    printf("Build type  : release (NDEBUG defined)\n");
#else
    printf("Build type  : debug (NDEBUG not defined)\n");
#endif

    /* Timestamp */
    printf("\n%s\n", BUILD_TIMESTAMP);
}

int main(void) {
    print_build_info();
    return 0;
}
