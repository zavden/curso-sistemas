/* dlerror_demo.c -- demonstrate error handling with dlopen/dlsym */
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    /* Error 1: file not found */
    printf("=== Test 1: library not found ===\n");
    void *h1 = dlopen("./libno_existe.so", RTLD_NOW);
    if (!h1) {
        printf("dlopen failed: %s\n", dlerror());
    }

    /* Error 2: symbol not found */
    printf("\n=== Test 2: symbol not found ===\n");
    void *h2 = dlopen("libm.so.6", RTLD_NOW);
    if (!h2) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return 1;
    }

    dlerror(); /* clear */
    void *sym = dlsym(h2, "funcion_inventada");
    char *err = dlerror();
    if (err) {
        printf("dlsym failed: %s\n", err);
    } else {
        printf("dlsym returned: %p\n", sym);
    }

    /* Success case for comparison */
    printf("\n=== Test 3: valid symbol ===\n");
    dlerror();
    double (*sine)(double);
    sym = dlsym(h2, "sin");
    memcpy(&sine, &sym, sizeof(sine));
    err = dlerror();
    if (err) {
        printf("dlsym failed: %s\n", err);
    } else {
        printf("dlsym(\"sin\") succeeded\n");
        printf("sin(1.5708) = %f\n", sine(1.5708));
    }

    dlclose(h2);
    printf("\nAll tests completed\n");

    return 0;
}
