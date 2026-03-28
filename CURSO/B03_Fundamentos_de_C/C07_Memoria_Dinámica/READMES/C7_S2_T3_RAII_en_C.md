# T03 — RAII en C

## Erratas detectadas en el material base

| Ubicación | Error | Corrección |
|-----------|-------|------------|
| `README.md` línea 308 | Dice que C++ RAII con smart pointers tiene "overhead de abstracción" | `std::unique_ptr` tiene **cero overhead** en runtime — es una zero-cost abstraction. El compilador optimiza la abstracción completamente. Solo `shared_ptr` tiene overhead real (reference counting atómico). La complejidad sintáctica sí es válida como desventaja |

---

## 1. Qué es RAII

RAII (Resource Acquisition Is Initialization) es un patrón donde la adquisición de un recurso está ligada a la creación de un objeto, y la liberación ocurre **automáticamente** cuando el objeto se destruye (sale del scope):

```c
// C++ — RAII nativo:
// {
//     std::unique_ptr<int> p = std::make_unique<int>(42);
//     // ... usar *p ...
// }  // destructor de unique_ptr → free automático. Zero overhead.

// Rust — RAII nativo (Drop trait):
// {
//     let v = vec![1, 2, 3];
//     // ... usar v ...
// }  // drop(v) automático → free interno

// C — NO tiene RAII nativo.
// No hay destructores automáticos.
// Toda liberación es manual.
// Pero hay patrones que lo simulan parcialmente.
```

El problema central en C: si una función adquiere N recursos (archivos, buffers, locks) y puede fallar en cualquier punto, cada path de error debe liberar los recursos ya adquiridos. Sin RAII, el programador debe manejar esto manualmente.

---

## 2. Patrón goto cleanup: un solo label

El patrón estándar en C. Usado extensamente en el **kernel de Linux** (más de 300,000 gotos en el código fuente):

```c
int process_file(const char *input, const char *output) {
    int result = -1;
    FILE *fin = NULL;        // Inicializar TODO a NULL
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

    // --- trabajo real (indentación plana) ---
    while (fgets(buffer, 4096, fin)) {
        fputs(buffer, fout);
    }

    if (ferror(fin)) {
        perror("read error");
        goto cleanup;
    }

    result = 0;  // Éxito

cleanup:
    // Liberar en orden INVERSO al de adquisición:
    free(buffer);           // free(NULL) es seguro (no-op)
    if (fout) fclose(fout); // fclose(NULL) NO es seguro → verificar
    if (fin) fclose(fin);
    return result;
}
```

### Las cinco reglas del goto cleanup

1. **Declarar todas las variables al inicio**, inicializadas a NULL/0
2. **Cada adquisición verifica** y salta a `cleanup` si falla
3. **El label `cleanup:` libera todo** en orden inverso
4. **Las funciones de liberación deben tolerar NULL**: `free(NULL)` es seguro, `fclose(NULL)` no lo es → verificar con `if`
5. **Una variable `result`** indica éxito (0) o fallo (-1)

---

## 3. Patrón goto cleanup: múltiples labels (kernel Linux)

Para cleanup parcial, el kernel usa labels **escalonados** — cada label libera solo lo adquirido hasta ese punto, luego cae al siguiente:

```c
int init_subsystem(void) {
    int *buf1 = malloc(1000);
    if (!buf1) goto fail;

    int *buf2 = malloc(2000);
    if (!buf2) goto fail_buf1;

    int *buf3 = malloc(3000);
    if (!buf3) goto fail_buf2;

    // ... todo OK ...
    return 0;

// Los labels se leen de abajo hacia arriba (fall-through):
fail_buf2:
    free(buf2);         // Libera buf2
fail_buf1:
    free(buf1);         // Libera buf1
fail:
    return -1;
}
// Si buf3 falla: free(buf2), free(buf1), return -1
// Si buf2 falla: free(buf1), return -1
// Si buf1 falla: return -1
```

### Un solo label vs múltiples labels

| Aspecto | Un solo label | Múltiples labels |
|---------|--------------|-----------------|
| Simplicidad | Más simple | Más labels que mantener |
| Liberación innecesaria | Llama `free(NULL)` para lo no alocado | Solo libera lo necesario |
| Uso típico | Funciones con pocos recursos | Kernel Linux, funciones con muchos recursos |
| Riesgo | Bajo | Medio (labels desordenados = bug) |

Para la mayoría del código, un solo label es suficiente. `free(NULL)` no tiene costo real.

---

## 4. __attribute__((cleanup)) de GCC/Clang

GCC y Clang ofrecen un atributo que llama a una función **automáticamente** cuando una variable sale del scope:

```c
// La función de cleanup recibe un PUNTERO A LA VARIABLE (no el valor):
void cleanup_free(void *p) {
    free(*(void **)p);    // p es &variable, *p es el puntero alocado
}

void cleanup_fclose(FILE **fp) {
    if (*fp) fclose(*fp);
}

int process(void) {
    __attribute__((cleanup(cleanup_free)))
    char *buffer = malloc(4096);

    __attribute__((cleanup(cleanup_fclose)))
    FILE *f = fopen("data.txt", "r");

    if (!buffer || !f) return -1;
    // cleanup_fclose(&f) y cleanup_free(&buffer)
    // se llaman automáticamente al hacer return

    // ... procesar ...

    return 0;
    // cleanup automático aquí también
    // Orden: inverso al de declaración (como destructores en C++)
}
```

### Comportamiento clave

El atributo genera código de cleanup en **cada punto de salida** de la función (cada `return`, final del bloque). No es magia en runtime — es código insertado por el compilador. Solo se ejecuta el cleanup de variables que **ya fueron declaradas** en ese punto del código:

```c
int example(void) {
    __attribute__((cleanup(cleanup_fclose)))
    FILE *f = fopen("a.txt", "r");
    if (!f) return -1;
    // Solo cleanup_fclose(&f) se ejecuta aquí
    // (buffer aún no fue declarado)

    __attribute__((cleanup(cleanup_free)))
    char *buffer = malloc(1024);
    if (!buffer) return -1;
    // Aquí se ejecutan AMBOS cleanups
    // (en orden inverso: cleanup_free, luego cleanup_fclose)

    return 0;
    // Ambos cleanups aquí también
}
```

---

## 5. Macros AUTO_FREE / AUTO_FCLOSE

Las macros mejoran la legibilidad:

```c
#define AUTO_FREE   __attribute__((cleanup(cleanup_free)))
#define AUTO_FCLOSE __attribute__((cleanup(cleanup_fclose)))

int copy_file(const char *input, const char *output) {
    AUTO_FCLOSE FILE *fin = fopen(input, "r");
    if (!fin) return -1;    // cleanup automático

    AUTO_FCLOSE FILE *fout = fopen(output, "w");
    if (!fout) return -1;   // cleanup automático de fin y fout

    AUTO_FREE char *buf = malloc(4096);
    if (!buf) return -1;    // cleanup automático de fin, fout, buf

    // --- trabajo real ---
    while (fgets(buf, 4096, fin)) {
        fputs(buf, fout);
    }

    return ferror(fin) ? -1 : 0;
    // cleanup automático de todo al retornar
}
```

Comparado con goto cleanup: no hay label, no hay bloque `cleanup:`, los `return` tempranos son seguros sin código adicional. El código es más conciso y con menos oportunidades de error.

---

## 6. Scope guards con macros (defer)

Un patrón que simula `defer` de Go o scope guards de Zig, usando `__attribute__((cleanup))` internamente:

```c
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

    if (!data || !f) return;  // cleanup automático

    // ... procesar ...
    // cleanup automático al final
}
```

### Caveat

El cast `(defer_fn)(func)` convierte el puntero de función a un tipo genérico `void (*)(void *)`. Esto es técnicamente UB en C estricto (llamar a una función por puntero incompatible). En la práctica funciona en todas las plataformas principales por compatibilidad de calling conventions, pero no es portable al 100%.

---

## 7. Comparación de los tres estilos

### if-else anidados (sin goto)

```c
int nested_style(const char *p1, const char *p2) {
    int result = -1;
    FILE *f1 = fopen(p1, "r");
    if (f1) {
        FILE *f2 = fopen(p2, "r");
        if (f2) {
            char *buf = malloc(4096);
            if (buf) {
                        // Trabajo real a 4 niveles de indentación
                        result = 0;
                        free(buf);
            }
            fclose(f2);
        }
        fclose(f1);
    }
    return result;
}
```

### goto cleanup

```c
int goto_style(const char *p1, const char *p2) {
    int result = -1;
    FILE *f1 = NULL, *f2 = NULL;
    char *buf = NULL;

    f1 = fopen(p1, "r"); if (!f1) goto cleanup;
    f2 = fopen(p2, "r"); if (!f2) goto cleanup;
    buf = malloc(4096);   if (!buf) goto cleanup;

    // Trabajo real a 1 nivel de indentación
    result = 0;

cleanup:
    free(buf);
    if (f2) fclose(f2);
    if (f1) fclose(f1);
    return result;
}
```

### __attribute__((cleanup))

```c
int attr_style(const char *p1, const char *p2) {
    AUTO_FCLOSE FILE *f1 = fopen(p1, "r"); if (!f1) return -1;
    AUTO_FCLOSE FILE *f2 = fopen(p2, "r"); if (!f2) return -1;
    AUTO_FREE char *buf = malloc(4096);     if (!buf) return -1;

    // Trabajo real a 1 nivel de indentación
    return 0;
    // Cleanup automático
}
```

### Resumen comparativo

| Aspecto | Nested if-else | goto cleanup | cleanup attribute |
|---------|---------------|--------------|-------------------|
| Indentación | Crece con cada recurso | Plana (1 nivel) | Plana (1 nivel) |
| Escalabilidad | Mala (10 recursos = 11 niveles) | Buena | Buena |
| Portabilidad | C estándar | C estándar | Solo GCC/Clang |
| Riesgo de leak | Bajo (estructura fuerza cleanup) | Medio (olvidar goto) | Bajo (automático) |
| Legibilidad | Mala con muchos recursos | Buena | Muy buena |

---

## 8. Limitaciones y cuándo usar cada patrón

### Limitaciones de __attribute__((cleanup))

```c
// 1. NO es C estándar — solo GCC/Clang (no MSVC)

// 2. No se puede cancelar el cleanup (transferir ownership):
AUTO_FREE char *data = malloc(100);
// Si quieres retornar `data` al caller (transferir ownership),
// el cleanup automático lo liberaría al salir del scope.
// Solución: asignar NULL antes de retornar:
char *result = data;
data = NULL;   // "Desactivar" el cleanup
return result;

// 3. Solo funciona con variables locales, no con struct members

// 4. El cleanup de un mutex puede llamar unlock sobre
//    un mutex que nunca fue locked (si el lock falla
//    entre la declaración y el lock)
```

### Recomendación

```
¿Necesitas portabilidad a MSVC?
  └─ SÍ → goto cleanup
  └─ NO → ¿Muchos recursos con returns tempranos?
           └─ SÍ → __attribute__((cleanup))
           └─ NO → goto cleanup (más explícito)
```

El kernel de Linux usa goto cleanup porque necesita compilar con cualquier toolchain. Proyectos GCC-only (como systemd) usan `__attribute__((cleanup))` extensivamente.

---

## 9. Liberación en orden inverso

La regla fundamental: **el último recurso adquirido es el primero en liberarse**. Esto es importante cuando un recurso depende de otro:

```c
// Ejemplo: buffer que se usa para escribir en un archivo
FILE *f = fopen("output.txt", "w");    // Adquirir primero
char *buf = malloc(4096);              // Adquirir segundo

// ... escribir buf → f ...

// Liberar en orden INVERSO:
free(buf);        // Primero el buffer (ya no lo necesitamos)
fclose(f);        // Luego el archivo (el buffer ya no escribe en él)

// Si se invirtiera el orden:
// fclose(f);     // ← El archivo se cierra
// fputs(buf, f); // ← Escribir en archivo cerrado = UB
// free(buf);
```

### Analogía con una pila

Los recursos forman una **pila** (LIFO):
```
Adquisición:          Liberación:
push f                pop buf    ← buf es el top
push buf              pop f      ← f estaba debajo

┌──────┐              ┌──────┐
│ buf  │ ← top        │      │
├──────┤              ├──────┤
│  f   │              │      │
└──────┘              └──────┘
```

Tanto goto cleanup como `__attribute__((cleanup))` respetan este orden automáticamente — el cleanup se ejecuta en orden inverso al de declaración/adquisición.

---

## 10. Comparación con C++ y Rust

### C: cleanup manual

```c
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
// PRO: explícito, portable, sin magia
// CON: verbose, propenso a errores
```

### C++: RAII con smart pointers

```cpp
// int process() {
//     auto f = std::unique_ptr<FILE, decltype(&fclose)>(
//         fopen("data.txt", "r"), &fclose);
//     if (!f) return -1;
//
//     auto buf = std::make_unique<char[]>(4096);
//     // ... procesar ...
//     return 0;
// }
// f y buf se liberan automáticamente al salir del scope
// PRO: automático, exception-safe, zero overhead (unique_ptr)
// CON: sintaxis verbose para FILE* con custom deleter
```

### Rust: ownership + Drop

```rust
// fn process() -> Result<(), io::Error> {
//     let f = File::open("data.txt")?;    // ? retorna si error
//     let mut buf = vec![0u8; 4096];
//     // ... procesar ...
//     Ok(())
// }
// f y buf se dropean automáticamente al salir del scope
// El compilador GARANTIZA: no hay leaks, no hay use-after-free
// PRO: automático, seguro, zero-cost, verificado en compilación
// CON: curva de aprendizaje del borrow checker
```

### Tabla resumen

| Característica | C (goto) | C (cleanup attr) | C++ (RAII) | Rust (Drop) |
|---------------|----------|------------------|------------|-------------|
| Cleanup automático | No | Sí (scope) | Sí (scope) | Sí (scope) |
| Portable | Sí | GCC/Clang | Sí (C++ std) | Sí (Rust std) |
| Verificación compilación | No | No | Parcial | Total |
| Overhead runtime | 0 | 0 | 0 (unique_ptr) | 0 |
| Exception-safe | N/A (no exceptions) | N/A | Sí | Sí (panic) |
| Transferir ownership | Manual | Asignar NULL | `std::move` | Move semántico |

---

## Ejercicios

### Ejercicio 1 — goto cleanup básico

```c
#include <stdio.h>
#include <stdlib.h>

int process(void) {
    int result = -1;
    int *a = NULL;
    int *b = NULL;

    a = malloc(10 * sizeof(int));
    if (!a) goto cleanup;

    b = malloc(20 * sizeof(int));
    if (!b) goto cleanup;

    a[0] = 42;
    b[0] = 99;
    printf("a[0]=%d b[0]=%d\n", a[0], b[0]);
    result = 0;

cleanup:
    free(b);
    free(a);
    return result;
}

int main(void) {
    int r = process();
    printf("result: %d\n", r);
    return 0;
}
```

Ejecuta con `valgrind --leak-check=full`.

**Predicción**: ¿Cuántos allocs y frees reportará Valgrind? ¿Habrá leaks?

<details><summary>Respuesta</summary>

2 allocs (malloc de `a` y `b`) + 2 frees (en cleanup) = 0 leaks. Valgrind dirá "All heap blocks were freed -- no leaks are possible". Incluso si `malloc(20 * sizeof(int))` fallara, `goto cleanup` ejecutaría `free(b)` (que es `free(NULL)` = no-op) y `free(a)` (válido). No hay path que produzca leak.

</details>

### Ejercicio 2 — goto cleanup con fallo simulado

```c
#include <stdio.h>
#include <stdlib.h>

int process(int fail_at) {
    int result = -1;
    int *a = NULL;
    int *b = NULL;
    int *c = NULL;

    a = malloc(100);
    if (!a || fail_at == 1) { printf("fail at a\n"); goto cleanup; }

    b = malloc(200);
    if (!b || fail_at == 2) { printf("fail at b\n"); goto cleanup; }

    c = malloc(300);
    if (!c || fail_at == 3) { printf("fail at c\n"); goto cleanup; }

    printf("all OK: a=%p b=%p c=%p\n", (void*)a, (void*)b, (void*)c);
    result = 0;

cleanup:
    printf("cleanup: freeing c=%p b=%p a=%p\n", (void*)c, (void*)b, (void*)a);
    free(c);
    free(b);
    free(a);
    return result;
}

int main(void) {
    for (int i = 0; i <= 3; i++) {
        printf("--- fail_at=%d ---\n", i);
        process(i);
        printf("\n");
    }
    return 0;
}
```

**Predicción**: Cuando `fail_at=2`, ¿cuáles de a, b, c serán NULL en el cleanup? ¿Se ejecuta `free(NULL)` para alguno?

<details><summary>Respuesta</summary>

Con `fail_at=2`: `a` fue alocado (no NULL), `b` fue alocado (no NULL) pero el `fail_at == 2` fuerza el goto, `c` nunca se alocó (sigue NULL). En cleanup: `free(c)` = `free(NULL)` (no-op), `free(b)` libera 200 bytes, `free(a)` libera 100 bytes. Valgrind: 0 leaks en todos los casos. El patrón funciona para cualquier punto de fallo.

</details>

### Ejercicio 3 — __attribute__((cleanup)) básico

```c
#include <stdio.h>
#include <stdlib.h>

void cleanup_free(void *p) {
    printf("  [cleanup] freeing %p\n", *(void **)p);
    free(*(void **)p);
}

#define AUTO_FREE __attribute__((cleanup(cleanup_free)))

void do_work(int early_return) {
    AUTO_FREE int *data = malloc(40);
    if (!data) return;
    data[0] = 42;
    printf("data[0] = %d\n", data[0]);

    if (early_return) {
        printf("returning early\n");
        return;
    }

    printf("normal end\n");
}

int main(void) {
    printf("--- early return ---\n");
    do_work(1);
    printf("\n--- normal end ---\n");
    do_work(0);
    return 0;
}
```

Compila con `gcc -std=c11 -Wall -Wextra`.

**Predicción**: ¿En ambos casos (`early_return=1` y `early_return=0`) se imprime `[cleanup]`?

<details><summary>Respuesta</summary>

Sí. `__attribute__((cleanup))` se ejecuta en **todo** punto de salida del scope — tanto en `return` tempranos como en el final normal de la función. Ambos casos imprimirán `[cleanup] freeing <addr>`. Eso es lo que lo hace similar a RAII: no importa cómo se sale, el cleanup ocurre.

</details>

### Ejercicio 4 — Diferencia entre goto y cleanup attr en error temprano

```c
#include <stdio.h>
#include <stdlib.h>

void cleanup_free(void *p) {
    printf("[auto] free(%p)\n", *(void **)p);
    free(*(void **)p);
}
#define AUTO_FREE __attribute__((cleanup(cleanup_free)))

void attr_style(void) {
    AUTO_FREE int *a = malloc(100);
    if (!a) return;  // Punto A

    AUTO_FREE int *b = malloc(200);
    if (!b) return;  // Punto B

    printf("both OK\n");
}

int main(void) {
    printf("--- simulating b fails ---\n");
    attr_style();
    return 0;
}
```

Imagina que `malloc(200)` retorna NULL (simulemos mentalmente).

**Predicción**: En el punto B (`return` porque `b` es NULL), ¿cuántos mensajes `[auto]` se imprimirán? ¿Se ejecuta cleanup de `a`?

<details><summary>Respuesta</summary>

**2 mensajes** `[auto]`. Ambas variables `a` y `b` ya fueron declaradas con `AUTO_FREE` antes del `return` en punto B. El cleanup de `b` se ejecuta primero (orden inverso), imprimiendo `[auto] free((nil))` (ya que `b` es NULL, `free(NULL)` es no-op). Luego cleanup de `a`, imprimiendo `[auto] free(<addr>)`. Esto es diferente del goto cleanup con label único, donde el cleanup siempre se ejecuta para TODAS las variables — aquí también, pero solo para las ya declaradas.

</details>

### Ejercicio 5 — Cancelar cleanup (transferir ownership)

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cleanup_free(void *p) {
    void *ptr = *(void **)p;
    if (ptr) printf("[cleanup] freeing %p\n", ptr);
    free(ptr);
}
#define AUTO_FREE __attribute__((cleanup(cleanup_free)))

char *create_string(const char *text) {
    AUTO_FREE char *buf = malloc(strlen(text) + 1);
    if (!buf) return NULL;
    strcpy(buf, text);

    // Queremos retornar buf al caller (transferir ownership)
    // Pero AUTO_FREE lo liberaría al hacer return...
    char *result = buf;
    buf = NULL;    // "Desactivar" el cleanup
    return result;
}

int main(void) {
    char *s = create_string("hello");
    if (s) {
        printf("Got: %s\n", s);
        free(s);
    }
    return 0;
}
```

**Predicción**: ¿Se ejecuta el `[cleanup]` al retornar de `create_string`? ¿`s` será válido o ya liberado?

<details><summary>Respuesta</summary>

El cleanup se ejecuta pero **no libera nada**: `buf` fue puesto a NULL antes del return, así que `cleanup_free` recibe un puntero a NULL y `free(NULL)` es no-op. No se imprime `[cleanup] freeing ...` (el if protege el printf). `s` contiene la dirección original, sigue válido, y el caller hace `free(s)`. Este patrón de "asignar NULL para desactivar cleanup" es la forma estándar de transferir ownership con `__attribute__((cleanup))`.

</details>

### Ejercicio 6 — Orden de cleanup

```c
#include <stdio.h>
#include <stdlib.h>

void cleanup_int(int **p) {
    printf("[cleanup] int at %p (val=%d)\n", (void *)*p, **p);
    free(*p);
}

#define AUTO_INT __attribute__((cleanup(cleanup_int)))

int main(void) {
    AUTO_INT int *a = malloc(sizeof(int)); *a = 1;
    AUTO_INT int *b = malloc(sizeof(int)); *b = 2;
    AUTO_INT int *c = malloc(sizeof(int)); *c = 3;

    printf("Created: a=%d b=%d c=%d\n", *a, *b, *c);
    return 0;
}
```

**Predicción**: ¿En qué orden se ejecutan los cleanups? ¿a→b→c o c→b→a?

<details><summary>Respuesta</summary>

**c→b→a** (orden inverso al de declaración). Como destructores en C++, los cleanups se ejecutan en orden LIFO. La salida será:
```
Created: a=1 b=2 c=3
[cleanup] int at <addr> (val=3)
[cleanup] int at <addr> (val=2)
[cleanup] int at <addr> (val=1)
```

</details>

### Ejercicio 7 — Comparar indentación: 4 recursos

```c
// Sin compilar — análisis de legibilidad.

// Estilo nested con 4 recursos:
int nested(void) {
    FILE *f1 = fopen("a", "r");
    if (f1) {
        FILE *f2 = fopen("b", "r");
        if (f2) {
            char *buf1 = malloc(100);
            if (buf1) {
                char *buf2 = malloc(200);
                if (buf2) {
                            // Trabajo real a 5 niveles
                            free(buf2);
                }
                free(buf1);
            }
            fclose(f2);
        }
        fclose(f1);
    }
    return 0;
}
```

**Predicción**: ¿A cuántos niveles de indentación está el "trabajo real"? Si tuvieras 8 recursos, ¿cuántos niveles serían? ¿Cuántos niveles tendría goto cleanup para 8 recursos?

<details><summary>Respuesta</summary>

Con 4 recursos: **5 niveles** (función + 4 ifs). Con 8 recursos: **9 niveles** de indentación — el código sería ilegible en una pantalla normal (80 columnas). Con goto cleanup: siempre **1 nivel** (dentro de la función), sin importar cuántos recursos. Esta es la razón principal por la que el kernel de Linux usa goto cleanup: la indentación plana escala a cualquier número de recursos.

</details>

### Ejercicio 8 — goto cleanup con FILE y malloc

```c
#include <stdio.h>
#include <stdlib.h>

int count_lines(const char *path) {
    int result = -1;
    FILE *f = NULL;
    char *line = NULL;

    f = fopen(path, "r");
    if (!f) goto cleanup;

    line = malloc(1024);
    if (!line) goto cleanup;

    int count = 0;
    while (fgets(line, 1024, f)) {
        count++;
    }

    if (ferror(f)) goto cleanup;

    result = count;

cleanup:
    free(line);
    if (f) fclose(f);
    return result;
}

int main(void) {
    // Crear archivo de test
    FILE *tmp = fopen("/tmp/raii_test.txt", "w");
    if (tmp) {
        fputs("line 1\nline 2\nline 3\n", tmp);
        fclose(tmp);
    }

    int n = count_lines("/tmp/raii_test.txt");
    printf("Lines: %d\n", n);

    n = count_lines("/tmp/nonexistent.txt");
    printf("Lines (bad path): %d\n", n);

    remove("/tmp/raii_test.txt");
    return 0;
}
```

**Predicción**: ¿Qué imprime para cada llamada? ¿Valgrind reporta leaks?

<details><summary>Respuesta</summary>

- `count_lines("/tmp/raii_test.txt")` → `Lines: 3` (éxito, result=3)
- `count_lines("/tmp/nonexistent.txt")` → `Lines: -1` (fopen falla, goto cleanup, result=-1)

Valgrind: 0 leaks. En el caso exitoso: 1 malloc + 1 fopen, 1 free + 1 fclose. En el caso fallido: 1 malloc (line) + fopen falla, goto cleanup, free(line) + if(f=NULL) skip fclose. Todo limpio.

</details>

### Ejercicio 9 — Múltiples labels del kernel

```c
#include <stdio.h>
#include <stdlib.h>

int init(int fail_step) {
    int *a = malloc(100);
    if (!a || fail_step == 1) goto fail;
    printf("a OK\n");

    int *b = malloc(200);
    if (!b || fail_step == 2) goto fail_a;
    printf("b OK\n");

    int *c = malloc(300);
    if (!c || fail_step == 3) goto fail_b;
    printf("c OK\n");

    printf("All initialized\n");
    free(c); free(b); free(a);
    return 0;

fail_b:
    printf("cleanup: free(b)\n");
    free(b);
fail_a:
    printf("cleanup: free(a)\n");
    free(a);
fail:
    printf("init failed at step %d\n", fail_step);
    return -1;
}

int main(void) {
    for (int i = 0; i <= 3; i++) {
        printf("--- step %d ---\n", i);
        init(i);
        printf("\n");
    }
    return 0;
}
```

**Predicción**: Con `fail_step=2`, ¿qué labels se ejecutan? ¿Se ejecuta `free(c)`?

<details><summary>Respuesta</summary>

Con `fail_step=2`: `a` se aloca OK, `b` se aloca pero `fail_step==2` fuerza `goto fail_a`. Se ejecutan los labels `fail_a` → `fail` (fall-through): `free(a)`, "init failed at step 2". `free(b)` NO se ejecuta (el goto salta a `fail_a`, no a `fail_b`). Pero espera — `b` fue alocado antes del goto. Hay un **leak de b**. Este es un bug sutil: si `fail_step==2` fuerza el goto después de que `b` ya fue alocado exitosamente, `b` no se libera porque el goto salta a `fail_a` en vez de `fail_b`. Corrección: cambiar a `goto fail_b` cuando falla en el paso 2.

</details>

### Ejercicio 10 — RAII simulado: resource guard

```c
#include <stdio.h>
#include <stdlib.h>

// "Guard" que protege un recurso
struct Guard {
    void *resource;
    void (*cleanup)(void *);
};

struct Guard guard_new(void *resource, void (*cleanup)(void *)) {
    return (struct Guard){ .resource = resource, .cleanup = cleanup };
}

void guard_cleanup(struct Guard *g) {
    if (g->cleanup && g->resource) {
        g->cleanup(g->resource);
    }
}

void fclose_wrapper(void *p) { fclose(p); }

#define GUARD __attribute__((cleanup(guard_cleanup)))

int process(const char *path) {
    GUARD struct Guard file_guard = guard_new(fopen(path, "r"), fclose_wrapper);
    if (!file_guard.resource) return -1;

    GUARD struct Guard buf_guard = guard_new(malloc(1024), free);
    if (!buf_guard.resource) return -1;

    FILE *f = file_guard.resource;
    char *buf = buf_guard.resource;

    if (fgets(buf, 1024, f)) {
        printf("Read: %s", buf);
    }

    return 0;
}

int main(void) {
    process("/etc/hostname");
    return 0;
}
```

**Predicción**: ¿Este patrón es más seguro que los macros AUTO_FREE individuales? ¿Qué ventaja tiene sobre goto cleanup?

<details><summary>Respuesta</summary>

Es **igual de seguro** que AUTO_FREE — ambos usan `__attribute__((cleanup))` internamente. La ventaja del Guard es que encapsula recurso + función de cleanup en una sola estructura, permitiendo usar cualquier tipo de recurso sin definir una macro específica por tipo. La desventaja: más verboso, requiere wrappers como `fclose_wrapper` porque `fclose` retorna `int`, no `void`. Sobre goto cleanup: la ventaja es no necesitar el label `cleanup:`, pero la desventaja es la misma que todo `__attribute__((cleanup))`: no portable a MSVC.

</details>
