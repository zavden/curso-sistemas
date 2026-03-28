/* dlopen_basic.c -- load libm at runtime, call cos() via dlsym */
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    /* 1. Open libm */
    void *handle = dlopen("libm.so.6", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return 1;
    }
    printf("dlopen(\"libm.so.6\") succeeded\n");

    /* 2. Clear previous errors, then look up cos */
    dlerror();
    double (*cosine)(double);
    /*
     * dlsym returns void*, but we need a function pointer.
     * ISO C forbids casting void* to a function pointer directly.
     * The POSIX-recommended workaround uses memcpy to copy the
     * pointer value without an illegal cast.
     */
    void *sym = dlsym(handle, "cos");
    memcpy(&cosine, &sym, sizeof(cosine));
    char *err = dlerror();
    if (err) {
        fprintf(stderr, "dlsym: %s\n", err);
        dlclose(handle);
        return 1;
    }
    printf("dlsym(\"cos\") succeeded\n");

    /* 3. Use the function */
    printf("cos(0.0) = %f\n", cosine(0.0));
    printf("cos(3.14159) = %f\n", cosine(3.14159));

    /* 4. Close */
    dlclose(handle);
    printf("dlclose succeeded\n");

    return 0;
}
