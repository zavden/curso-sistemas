# T02 — Scope y lifetime

## Erratas detectadas en el material original

| Archivo | Línea(s) | Error | Corrección |
|---------|----------|-------|------------|
| `README.md` | 237–244 | Afirma que las variables `static` locales se inicializan "la primera vez que se ejecuta la función" y muestra `static int x = expensive_compute();`. En C, los inicializadores de variables con storage duration estática **deben ser constant expressions** (C11 §6.7.9p4). `expensive_compute()` no compila en C — ese comportamiento es de **C++**, no de C | En C, `static` locales se inicializan antes de `main` con valores constantes. La inicialización dinámica "on first call" es exclusiva de C++ |

---

## 1. Scope (visibilidad)

El scope determina **desde dónde** se puede acceder a una variable por su nombre. C tiene cuatro niveles:

### File scope

```c
int global = 42;              // visible en todo el archivo
                              // (y en otros, si no es static)
```

### Block scope

```c
void foo(void) {
    int local = 10;           // visible solo dentro de foo
    if (local > 5) {
        int inner = 20;       // visible solo dentro del if
    }
    // inner ya no es visible aquí — error de compilación
}
```

Cada `{ }` crea un nuevo scope. Las variables declaradas en un `for` también son block scope:

```c
for (int i = 0; i < 5; i++) {
    int temp = i * 2;
}
// i y temp ya no existen aquí
```

### Function scope

Solo aplica a **labels** (etiquetas de `goto`). Un label es visible en toda la función, sin importar en qué bloque se definió.

### Function prototype scope

Solo aplica a nombres de parámetros en prototipos:

```c
void process(int n, int arr[n]);   // n visible solo en esta declaración
```

---

## 2. Shadowing

Una variable en un bloque interno puede tener el mismo nombre que una en un bloque externo. La interna **oculta** (shadow) a la externa:

```c
int x = 100;                      // file scope

void foo(void) {
    int x = 200;                  // oculta la global
    printf("%d\n", x);            // 200

    {
        int x = 300;              // oculta la local de foo
        printf("%d\n", x);        // 300
    }

    printf("%d\n", x);            // 200 (la de foo)
}
// x global sigue siendo 100
```

Shadowing es fuente frecuente de bugs:

```c
int count = 0;                    // global

void process(void) {
    for (int count = 0; count < 10; count++) {
        // Este count es DIFERENTE de la global
    }
    printf("count = %d\n", count);  // 0 — la global no cambió
}
```

Detección:

```bash
# -Wshadow NO está incluido en -Wall ni -Wextra:
gcc -Wall -Wextra -Wshadow main.c -o main
# warning: declaration of 'x' shadows a global declaration
```

---

## 3. Lifetime (tiempo de vida)

El lifetime es el período durante el cual la memoria de una variable es válida:

```c
// 1. Automatic — vive mientras el bloque está activo:
void foo(void) {
    int x = 42;               // nace aquí (stack)
}                              // muere aquí

// 2. Static — vive toda la ejecución:
static int count = 0;         // nace antes de main, muere al final
int global = 10;              // nace antes de main, muere al final

// 3. Allocated — vive entre malloc y free:
int *p = malloc(sizeof(int)); // nace aquí (heap)
free(p);                       // muere aquí
```

---

## 4. Scope ≠ Lifetime

Esta es la distinción más importante del tópico:

```c
// static local: scope limitado, lifetime extendido
void counter(void) {
    static int count = 0;
    // Scope: block (solo visible aquí)
    // Lifetime: static (vive toda la ejecución)
    count++;
}

// malloc: scope del puntero termina, lifetime de la memoria continúa
int *create(void) {
    int *p = malloc(sizeof(int));
    *p = 42;
    return p;
    // Scope de p: termina aquí
    // Lifetime de *p: sigue viva (fue malloc'd)
}

// PELIGRO: scope Y lifetime terminan juntos en auto
int *dangerous(void) {
    int x = 42;
    return &x;
    // Scope de x: termina aquí
    // Lifetime de x: termina aquí (automatic)
    // → dangling pointer → UB
}
```

---

## 5. Linkage (enlace)

El linkage determina si un nombre es compartido entre archivos:

```c
// 1. External linkage — visible en todos los archivos:
int global_var = 42;              // external (default)
void global_func(void) { }       // external (default)

// 2. Internal linkage — solo visible en este archivo:
static int file_var = 42;        // internal
static void file_func(void) { }  // internal

// 3. No linkage — variables locales:
void foo(void) {
    int local = 42;               // no linkage
    static int persist = 0;       // no linkage (pero lifetime static)
}
```

### Combinaciones completas

| Declaración | Scope | Linkage | Lifetime |
|-------------|-------|---------|----------|
| `int x;` (fuera de función) | File | External | Static |
| `static int x;` (fuera de función) | File | Internal | Static |
| `int x;` (en función) | Block | Ninguno | Automatic |
| `static int x;` (en función) | Block | Ninguno | Static |
| `extern int x;` | File | External | Static (declaración) |

Verificación con `nm`:

```bash
nm file.o
# Mayúscula (T, D, B) = external linkage (visible desde otros archivos)
# Minúscula (t, d, b) = internal linkage (static)
# U = undefined (se resuelve al enlazar)
```

---

## 6. Parámetros de función

```c
void process(int x, int *ptr) {
    // x y ptr tienen:
    //   - Block scope (del cuerpo de la función)
    //   - Automatic lifetime (se destruyen al retornar)
    //   - No linkage

    x = 100;      // NO afecta al llamador (x es copia)
    *ptr = 100;   // SÍ afecta al dato apuntado (misma dirección)
}
```

Los parámetros son siempre **copias** del argumento. Para un puntero, se copia la dirección (no el dato apuntado), por lo que modificar `*ptr` sí afecta al llamador.

---

## 7. Orden de inicialización

```c
// Variables con static lifetime se inicializan ANTES de main:
int a = 10;
static int b = 20;
// Ambas ya tienen su valor cuando main empieza

// ORDEN entre archivos: NO definido por el estándar
// file_a.c: int x = 10;
// file_b.c: int y = 20;
// ¿x se inicializa antes que y? No está garantizado.

// Dentro de un archivo, el orden sigue la declaración:
int first = 1;     // se inicializa primero
int second = 2;    // se inicializa segundo
```

**Importante en C (diferencia con C++):** los inicializadores de variables con storage duration estática deben ser **constant expressions**. No se puede escribir `static int x = compute();` — eso no compila en C. Si se necesita inicialización dinámica, usar un patrón con flag:

```c
static int table[256];
static int initialized = 0;

void ensure_init(void) {
    if (!initialized) {
        for (int i = 0; i < 256; i++)
            table[i] = i * i;    // inicialización dinámica explícita
        initialized = 1;
    }
}
```

---

## 8. Errores comunes

### Retornar puntero a variable local (dangling pointer)

```c
// INCORRECTO:
int *bad_function(void) {
    int x = 42;
    return &x;           // x muere al retornar → dangling pointer → UB
}

// CORRECTO — tres alternativas:

// 1. malloc (caller hace free):
int *good_malloc(void) {
    int *p = malloc(sizeof(int));
    *p = 42;
    return p;
}

// 2. static (vive toda la ejecución, no thread-safe):
int *good_static(void) {
    static int x = 42;
    return &x;
}

// 3. Pasar buffer del caller:
void good_output(int *out) {
    *out = 42;
}
```

GCC detecta `return &local;` directamente con `-Wreturn-local-addr` (incluido en `-Wall`), pero si la dirección se toma indirectamente (`int *ptr = &local; return ptr;`), puede no detectarlo.

### Asumir que locales se inicializan a 0

```c
void foo(void) {
    int x;             // NO es 0 — valor indeterminado
    int arr[100];      // NO está a ceros
    int *p;            // NO es NULL

    if (x > 0) { }     // UB: leer variable indeterminada
}

// Solo las STATIC se inicializan a 0:
void bar(void) {
    static int x;      // SÍ es 0 (garantizado)
    static int *p;     // SÍ es NULL (garantizado)
}
```

---

## Ejercicios

### Ejercicio 1 — Shadowing y scope

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex1.c -o ex1
#include <stdio.h>

int x = 1;

void foo(void) {
    int x = 2;
    {
        int x = 3;
        printf("A: %d\n", x);
    }
    printf("B: %d\n", x);
}

int main(void) {
    printf("C: %d\n", x);
    foo();
    printf("D: %d\n", x);
    return 0;
}
```

**Predicción:** ¿Qué imprime cada línea? ¿En qué orden?

<details><summary>Respuesta</summary>

```
C: 1
A: 3
B: 2
D: 1
```

`C` imprime la global (1). Dentro de `foo`, el bloque más interno tiene `x=3` (A), al salir vuelve a ser visible la `x=2` de `foo` (B). `D` imprime la global de nuevo (1) — nunca fue modificada. Con `-Wshadow`, GCC advertiría de las dos declaraciones internas que ocultan a las externas.

</details>

---

### Ejercicio 2 — Block scope: error de compilación

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex2.c -o ex2
#include <stdio.h>

int main(void) {
    for (int i = 0; i < 3; i++) {
        int temp = i * 10;
        printf("i=%d, temp=%d\n", i, temp);
    }
    printf("After loop: i=%d\n", i);       // ← ¿Compila?
    return 0;
}
```

**Predicción:** ¿Compilará este programa? ¿Por qué?

<details><summary>Respuesta</summary>

**No compila.** Error:
```
error: 'i' undeclared (first use in this function)
```

`i` fue declarada en el `for`, así que su scope es el bloque del loop. Fuera del loop, `i` ya no existe. Para acceder al valor después del loop, declarar `i` antes del `for`:
```c
int i;
for (i = 0; i < 3; i++) { ... }
printf("After loop: i=%d\n", i);  // OK: i = 3
```

</details>

---

### Ejercicio 3 — Lifetime: dangling pointer

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex3.c -o ex3
#include <stdio.h>
#include <stdlib.h>

int *from_stack(void) {
    int x = 42;
    return &x;               // ← ¿Seguro?
}

int *from_heap(void) {
    int *p = malloc(sizeof(int));
    *p = 42;
    return p;                // ← ¿Seguro?
}

int *from_static(void) {
    static int x = 42;
    return &x;               // ← ¿Seguro?
}

int main(void) {
    int *a = from_heap();
    int *b = from_static();

    printf("heap:   %d\n", *a);
    printf("static: %d\n", *b);
    free(a);

    // int *c = from_stack();
    // printf("stack: %d\n", *c);  // ← ¿Qué pasaría?
    return 0;
}
```

**Predicción:** ¿Cuáles de las tres funciones son seguras? ¿Qué warning da GCC para `from_stack`?

<details><summary>Respuesta</summary>

```
heap:   42
static: 42
```

- `from_heap`: **segura** — la memoria vive en el heap hasta `free()`.
- `from_static`: **segura** — la variable `static` vive toda la ejecución.
- `from_stack`: **insegura** — `x` muere al retornar. GCC produce: `warning: function returns address of local variable [-Wreturn-local-addr]`. Desreferenciar el puntero retornado es UB.

El patrón `from_static` tiene la limitación de que siempre retorna la misma dirección (no es thread-safe y la siguiente llamada puede sobrescribir el valor).

</details>

---

### Ejercicio 4 — Linkage: internal vs external con nm

```c
// Archivo: visibility.c
// Compila como objeto: gcc -std=c11 -Wall -Wextra -Wpedantic -c visibility.c
// Luego: nm visibility.o
#include <stdio.h>

int public_var = 10;
static int private_var = 20;

void public_func(void) {
    printf("public: %d, %d\n", public_var, private_var);
}

static void private_func(void) {
    printf("private\n");
}

void call_private(void) {
    private_func();
}
```

**Predicción:** ¿Qué letras tendrá cada símbolo en `nm`? (D/d para datos, T/t para código)

<details><summary>Respuesta</summary>

```
nm visibility.o:
... T call_private       ← T: .text, external linkage
... t private_func       ← t: .text, internal linkage (static)
... d private_var        ← d: .data, internal linkage (static)
... T public_func        ← T: .text, external linkage
... D public_var         ← D: .data, external linkage
    U printf             ← U: undefined (se resuelve al enlazar)
```

Regla: **mayúscula = external** (visible desde otros archivos), **minúscula = internal** (static, invisible fuera). Si otro archivo declara `extern int private_var;` y se enlaza, producirá `undefined reference`.

</details>

---

### Ejercicio 5 — Dos módulos con `static int count`

```c
// counter.c
static int count = 0;
void counter_inc(void) { count++; }
int counter_get(void) { return count; }

// logger.c
#include <stdio.h>
static int count = 0;
void logger_log(const char *msg) { count++; printf("[%d] %s\n", count, msg); }
int logger_get(void) { return count; }

// main.c
// Compila: gcc -std=c11 counter.c logger.c main.c -o multi
#include <stdio.h>
extern void counter_inc(void);
extern int counter_get(void);
extern void logger_log(const char *msg);
extern int logger_get(void);

int main(void) {
    counter_inc();
    counter_inc();
    counter_inc();
    logger_log("hello");
    logger_log("world");
    printf("counter: %d, logger: %d\n", counter_get(), logger_get());
    return 0;
}
```

**Predicción:** ¿Los dos `count` interfieren entre sí? ¿Qué valores tendrán?

<details><summary>Respuesta</summary>

```
[1] hello
[2] world
counter: 3, logger: 2
```

Los dos `count` son **completamente independientes** gracias a `static` (internal linkage). El contador llega a 3 (tres incrementos), el logger llega a 2 (dos mensajes). Sin `static`, habría error de enlace: `multiple definition of 'count'`.

</details>

---

### Ejercicio 6 — Scope vs lifetime en static local

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex6.c -o ex6
#include <stdio.h>

void accumulate(int value) {
    static int total = 0;
    total += value;
    printf("total = %d\n", total);
}

int main(void) {
    accumulate(10);
    accumulate(20);
    accumulate(30);
    // printf("total = %d\n", total);  // ← ¿Compila?
    return 0;
}
```

**Predicción:** ¿Qué imprime cada llamada? ¿Qué pasa si descomentas la línea en main?

<details><summary>Respuesta</summary>

```
total = 10
total = 30
total = 60
```

`total` acumula porque es `static` (lifetime = toda la ejecución). Pero si descomentas la línea en `main`, **no compila**:
```
error: 'total' undeclared
```

`total` tiene block scope (solo visible dentro de `accumulate`), aunque su lifetime es estático. Scope y lifetime son propiedades independientes.

</details>

---

### Ejercicio 7 — Parámetros: copia vs puntero

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex7.c -o ex7
#include <stdio.h>

void by_value(int x) {
    x = 999;
}

void by_pointer(int *x) {
    *x = 999;
}

int main(void) {
    int a = 1, b = 1;

    by_value(a);
    by_pointer(&b);

    printf("a = %d\n", a);
    printf("b = %d\n", b);
    return 0;
}
```

**Predicción:** ¿Qué valores tendrán `a` y `b` después de las llamadas?

<details><summary>Respuesta</summary>

```
a = 1
b = 999
```

`by_value` recibe una **copia** de `a` — modificar `x` no afecta a `a`. `by_pointer` recibe una **copia de la dirección** de `b` — `*x = 999` escribe en la dirección de `b`, modificándolo. En C, todos los parámetros se pasan por valor; pasar punteros simula paso por referencia.

</details>

---

### Ejercicio 8 — Inicialización estática: constante obligatoria

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex8.c -o ex8
#include <stdio.h>

int compute(void) {
    return 42;
}

int main(void) {
    static int a = 10;           // ← ¿Compila?
    // static int b = compute(); // ← ¿Compila?
    static int c = 2 + 3;       // ← ¿Compila?

    printf("a = %d, c = %d\n", a, c);
    return 0;
}
```

**Predicción:** ¿Cuáles de las tres líneas compilan? ¿Por qué?

<details><summary>Respuesta</summary>

```
a = 10, c = 5
```

- `a = 10`: **compila** — `10` es una constant expression.
- `b = compute()`: **no compila** — `compute()` no es una constant expression. Error: `initializer element is not a compile-time constant`. En C, los inicializadores de variables `static` deben ser constantes conocidas en tiempo de compilación.
- `c = 2 + 3`: **compila** — `2 + 3` es una constant expression (el compilador la evalúa a `5` en tiempo de compilación).

Nota: en C++, `static int b = compute();` sí compila — la inicialización dinámica on-first-call es una feature de C++, no de C.

</details>

---

### Ejercicio 9 — Shadowing: el bug silencioso

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wshadow ex9.c -o ex9
#include <stdio.h>

int total = 0;

void add_to_total(int value) {
    int total = 0;      // ← ¿Bug o intencional?
    total += value;
    printf("local total: %d\n", total);
}

int main(void) {
    add_to_total(10);
    add_to_total(20);
    add_to_total(30);
    printf("global total: %d\n", total);
    return 0;
}
```

**Predicción:** ¿Qué imprime? ¿Cuántos warnings produce `-Wshadow`?

<details><summary>Respuesta</summary>

```
local total: 10
local total: 20
local total: 30
global total: 0
```

La variable local `total` oculta a la global. Cada llamada crea un `total` local a 0, le suma el valor, lo imprime, y lo destruye. La global **nunca se modifica** — siempre es 0. Con `-Wshadow`, GCC produce **1 warning**:
```
warning: declaration of 'total' shadows a global declaration
```

Sin `-Wshadow`, este bug pasa completamente desapercibido.

</details>

---

### Ejercicio 10 — Linkage error: multiple definition

```c
// Archivo 1: module_a.c
int shared = 10;
void func_a(void) { shared++; }

// Archivo 2: module_b.c
int shared = 20;
void func_b(void) { shared++; }

// Archivo 3: main.c
// Compila: gcc -std=c11 module_a.c module_b.c main.c -o fail
#include <stdio.h>
extern int shared;
extern void func_a(void);
extern void func_b(void);
int main(void) {
    func_a();
    func_b();
    printf("shared = %d\n", shared);
    return 0;
}
```

**Predicción:** ¿Compilará? ¿Enlazará? ¿Cómo se corrige?

<details><summary>Respuesta</summary>

Cada archivo **compila** individualmente sin errores. Pero al **enlazar**:
```
multiple definition of `shared'
```

Ambos `module_a.c` y `module_b.c` definen `shared` con external linkage. El enlazador no sabe cuál usar. Dos correcciones:

1. Hacer una de las dos `static` (si deben ser independientes):
```c
// module_a.c
static int shared = 10;    // internal linkage, no conflicto
```

2. Definir en un solo archivo y declarar con `extern` en el otro (si debe ser compartida):
```c
// module_a.c: int shared = 10;      (definición)
// module_b.c: extern int shared;     (declaración)
```

</details>
