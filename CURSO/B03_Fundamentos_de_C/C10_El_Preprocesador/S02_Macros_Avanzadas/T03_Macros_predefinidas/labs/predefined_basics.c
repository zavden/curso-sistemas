/*
 * predefined_basics.c — Macros predefinidas estandar del compilador C.
 *
 * Demuestra __FILE__, __LINE__, __func__, __DATE__, __TIME__,
 * __STDC__ y __STDC_VERSION__.
 */

#include <stdio.h>

static void show_func_name(void) {
    printf("  Inside function: %s\n", __func__);
    printf("  At line: %d\n", __LINE__);
}

int main(void) {
    printf("=== Standard Predefined Macros ===\n\n");

    printf("__FILE__         : %s\n", __FILE__);
    printf("__LINE__         : %d\n", __LINE__);
    printf("__DATE__         : %s\n", __DATE__);
    printf("__TIME__         : %s\n", __TIME__);
    printf("__STDC__         : %d\n", __STDC__);
    printf("__STDC_VERSION__ : %ld\n", __STDC_VERSION__);

    printf("\n=== __func__ in different functions ===\n\n");
    printf("  Inside function: %s\n", __func__);
    show_func_name();

    printf("\n=== __LINE__ expands where it appears ===\n\n");
    printf("  This is line %d\n", __LINE__);
    printf("  This is line %d\n", __LINE__);
    printf("  This is line %d\n", __LINE__);

    return 0;
}
