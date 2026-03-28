# T03 — RAII en C

## Qué es RAII

RAII (Resource Acquisition Is Initialization) es un patrón donde
la adquisición de un recurso está ligada a la creación de un
objeto, y la liberación ocurre **automáticamente** al destruirse:

```c
// En C++/Rust, RAII es nativo:
//
// C++:
// {
//     std::unique_ptr<int> p = std::make_unique<int>(42);
//     // ... usar *p ...
// }  // p se destruye automáticamente → free automático
//
// Rust:
// {
//     let v = vec![1, 2, 3];
//     // ... usar v ...
// }  // v se destruye automáticamente → free automático

// C NO tiene RAII nativo.
// No hay destructores automáticos.
// Toda liberación es manual.
// Pero hay patrones que lo simulan parcialmente.
```

## Patrón 1: cleanup con goto

El patrón goto cleanup es la forma estándar de manejar
limpieza en C. Es usado extensamente en el kernel de Linux:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int process_file(const char *input, const char *output) {
    int result = -1;
    FILE *fin = NULL;
    FILE *fout = NULL;
    char *buffer = NULL;

    fin = fopen(input, "r");
    if (!fin) {
        perror("fopen input");
        goto cleanup;
    }

    fout = fopen(output, "w");
    if (!fout) {
        perror("fopen output");
        goto cleanup;
    }

    buffer = malloc(4096);
    if (!buffer) {
        perror("malloc");
        goto cleanup;
    }

    // Procesar:
    while (fgets(buffer, 4096, fin)) {
        // ... transformar buffer ...
        fputs(buffer, fout);
    }

    if (ferror(fin)) {
        perror("read error");
        goto cleanup;
    }

    result = 0;    // éxito

cleanup:
    // Los recursos se liberan en orden INVERSO:
    free(buffer);         // free(NULL) es seguro
    if (fout) fclose(fout);
    if (fin) fclose(fin);
    return result;
}
```

```c
// Reglas del patrón goto cleanup:
//
// 1. Declarar todas las variables al inicio, inicializadas a NULL/0.
// 2. Cada adquisición verifica y salta a cleanup si falla.
// 3. El label cleanup: libera todo en orden inverso.
// 4. Las funciones de liberación deben tolerar NULL.
//    (free(NULL) es seguro, fclose(NULL) NO es seguro → verificar)
// 5. Una variable de resultado indica éxito/fallo.
```

### Múltiples labels (kernel de Linux)

```c
// Para cleanup parcial, usar múltiples labels:

int init_subsystem(void) {
    int *buf1 = malloc(1000);
    if (!buf1) goto fail;

    int *buf2 = malloc(2000);
    if (!buf2) goto fail_buf1;

    int *buf3 = malloc(3000);
    if (!buf3) goto fail_buf2;

    // ... todo OK ...
    return 0;

fail_buf2:
    free(buf2);
fail_buf1:
    free(buf1);
fail:
    return -1;
}

// Los labels se leen de abajo hacia arriba:
// fail_buf2 libera buf2, luego cae a fail_buf1
// fail_buf1 libera buf1, luego cae a fail
// fail retorna -1
//
// Este patrón es MUY común en el kernel de Linux.
// Más de 300,000 gotos en el código fuente del kernel.
```

## Patrón 2: __attribute__((cleanup))

GCC y Clang ofrecen un atributo que llama a una función
cuando una variable sale del scope:

```c
#include <stdio.h>
#include <stdlib.h>

// Función de limpieza — recibe un PUNTERO a la variable:
void cleanup_free(void *p) {
    free(*(void **)p);
}

void cleanup_fclose(FILE **fp) {
    if (*fp) fclose(*fp);
}

int main(void) {
    // __attribute__((cleanup(func))) llama a func(&var) al salir del scope:

    __attribute__((cleanup(cleanup_free)))
    int *data = malloc(100 * sizeof(int));

    __attribute__((cleanup(cleanup_fclose)))
    FILE *f = fopen("test.txt", "r");

    if (!data || !f) return -1;

    // ... usar data y f ...

    return 0;
    // Al salir del scope:
    // cleanup_fclose(&f) se llama automáticamente
    // cleanup_free(&data) se llama automáticamente
    // Orden: inverso al de declaración (como destructores en C++)
}
```

```c
// Macros para hacerlo más legible:
#define AUTO_FREE __attribute__((cleanup(cleanup_free)))
#define AUTO_FCLOSE __attribute__((cleanup(cleanup_fclose)))

void example(void) {
    AUTO_FREE int *data = malloc(100 * sizeof(int));
    AUTO_FCLOSE FILE *f = fopen("test.txt", "r");

    if (!data || !f) return;    // cleanup automático al retornar

    // ... procesar ...

    // cleanup automático al final del scope
}
```

```c
// Macros genéricos con typeof (GCC):
#define CLEANUP(func) __attribute__((cleanup(func)))

// Para mutex:
void cleanup_mutex(pthread_mutex_t **m) {
    pthread_mutex_unlock(*m);
}

void thread_safe_function(void) {
    CLEANUP(cleanup_mutex) pthread_mutex_t *lock = &global_mutex;
    pthread_mutex_lock(lock);

    // ... sección crítica ...

    // unlock automático al salir, incluso con return temprano
}
```

### Limitaciones de cleanup

```c
// 1. Es una extensión GCC/Clang — no es estándar C.
//    No funciona con MSVC.

// 2. No se puede usar con todos los tipos de recursos.
//    La función de cleanup recibe un puntero a la variable,
//    no al recurso directamente.

// 3. El orden de cleanup depende del scope, no del programador.

// 4. No se puede "cancelar" el cleanup (por ejemplo, si
//    quieres transferir ownership).

// 5. Solo funciona con variables locales, no con struct members.
```

## Patrón 3: scope guards con macros

```c
// Un patrón que simula defer (Go) o scope guards (C++/Zig):

#include <stdio.h>
#include <stdlib.h>

// Implementación simplificada de defer con cleanup:
#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)

typedef void (*defer_fn)(void *);
struct defer_data {
    defer_fn fn;
    void *arg;
};

static inline void defer_exec(struct defer_data *d) {
    if (d->fn) d->fn(d->arg);
}

#define defer(func, ptr) \
    __attribute__((cleanup(defer_exec))) \
    struct defer_data CONCAT(_defer_, __LINE__) = { \
        .fn = (defer_fn)(func), .arg = (ptr) \
    }

// Uso:
void example(void) {
    int *data = malloc(100 * sizeof(int));
    defer(free, data);    // free(data) al salir del scope

    FILE *f = fopen("test.txt", "r");
    defer(fclose, f);     // fclose(f) al salir del scope

    if (!data || !f) return;    // cleanup automático

    // ... procesar ...
    // cleanup automático al final
}
```

## Comparación: C vs C++ vs Rust

```c
// === C — cleanup manual ===
int process(void) {
    int result = -1;
    FILE *f = NULL;
    char *buf = NULL;

    f = fopen("data.txt", "r");
    if (!f) goto cleanup;

    buf = malloc(4096);
    if (!buf) goto cleanup;

    // ... procesar ...
    result = 0;

cleanup:
    free(buf);
    if (f) fclose(f);
    return result;
}
// PRO: explícito, sin magia
// CON: verbose, propenso a errores si se olvida el goto
```

```c
// === C++ — RAII con smart pointers ===
// int process() {
//     auto f = std::unique_ptr<FILE, decltype(&fclose)>(
//         fopen("data.txt", "r"), &fclose);
//     if (!f) return -1;
//
//     auto buf = std::make_unique<char[]>(4096);
//     // ... procesar ...
//     return 0;
// }
// // f y buf se liberan automáticamente al salir del scope
// PRO: automático, exception-safe
// CON: overhead de abstracción, sintaxis compleja
```

```c
// === Rust — ownership + Drop trait ===
// fn process() -> Result<(), io::Error> {
//     let f = File::open("data.txt")?;   // ? retorna error temprano
//     let mut buf = vec![0u8; 4096];
//     // ... procesar ...
//     Ok(())
// }
// // f y buf se liberan automáticamente (Drop)
// // El compilador GARANTIZA que no hay leaks ni use-after-free
// PRO: automático, seguro, zero-cost
// CON: curva de aprendizaje del borrow checker
```

## Recomendación para el curso

```c
// En C, usar el patrón goto cleanup como estándar:
// - Es portable (C estándar)
// - Es el patrón del kernel de Linux
// - Es explícito y predecible
// - Funciona en todos los compiladores

// Usar __attribute__((cleanup)) cuando:
// - Solo se compila con GCC/Clang
// - Se quiere reducir boilerplate en código con muchos recursos
// - Se tiene claro que no se necesita portabilidad a MSVC

// El patrón general siempre es:
// 1. Adquirir recurso
// 2. Verificar éxito
// 3. Usar recurso
// 4. Liberar recurso (en orden inverso)
// goto cleanup automatiza el paso 4 para todos los paths de error.
```

---

## Ejercicios

### Ejercicio 1 — goto cleanup

```c
// Implementar una función que:
// 1. Abra un archivo de entrada
// 2. Abra un archivo de salida
// 3. Aloque un buffer de 4096 bytes
// 4. Copie el contenido de entrada a salida
// Usar el patrón goto cleanup con un solo label.
// Si cualquier paso falla, todos los recursos se liberan.
```

### Ejercicio 2 — __attribute__((cleanup))

```c
// Reescribir el ejercicio 1 usando __attribute__((cleanup)).
// Crear macros AUTO_FREE y AUTO_FCLOSE.
// Verificar que los return tempranos liberan todo.
// Compilar con gcc y clang.
```

### Ejercicio 3 — Comparar approaches

```c
// Implementar una función que abra 3 archivos, aloque 2 buffers,
// y procese datos, de tres formas:
// 1. Con if-else anidados (sin goto)
// 2. Con goto cleanup
// 3. Con __attribute__((cleanup))
// ¿Cuál es más legible? ¿Cuál es más segura?
// Verificar las 3 con valgrind.
```
