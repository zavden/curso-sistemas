# T02 — Scope y lifetime

## Scope (visibilidad)

El scope de una variable determina **desde dónde** se puede
acceder a ella por su nombre. C tiene cuatro niveles de scope:

```c
// 1. File scope — declarada fuera de cualquier función:
int global = 42;              // visible en todo el archivo
                              // (y en otros, si no es static)

// 2. Block scope — declarada dentro de { }:
void foo(void) {
    int local = 10;           // visible solo dentro de foo

    if (local > 5) {
        int inner = 20;       // visible solo dentro del if
    }
    // inner ya no es visible aquí
}

// 3. Function scope — solo aplica a labels (goto):
void bar(void) {
    goto end;                 // los labels son visibles en toda la función
    // ...
    end:
    return;
}

// 4. Function prototype scope — parámetros en prototipos:
void process(int n, int arr[n]);   // n visible solo en la declaración
```

### Block scope en detalle

```c
void example(void) {
    int x = 10;

    {   // bloque anónimo — crea un nuevo scope
        int y = 20;
        printf("%d %d\n", x, y);   // OK: x visible, y visible
    }
    // printf("%d\n", y);          // ERROR: y fuera de scope

    for (int i = 0; i < 5; i++) {
        int temp = i * 2;          // temp solo existe en el loop
    }
    // printf("%d\n", i);          // ERROR: i fuera de scope
    // printf("%d\n", temp);       // ERROR: temp fuera de scope
}
```

### Shadowing — misma nombre, diferente variable

```c
int x = 100;                      // file scope: x = 100

void foo(void) {
    int x = 200;                  // block scope: oculta la global
    printf("%d\n", x);            // 200 (la local)

    {
        int x = 300;              // otro block scope: oculta la anterior
        printf("%d\n", x);        // 300 (la más interna)
    }

    printf("%d\n", x);            // 200 (la local de foo)
}

// x global sigue siendo 100
```

```bash
# -Wshadow advierte cuando una variable oculta a otra:
gcc -Wall -Wextra -Wshadow main.c -o main
# warning: declaration of 'x' shadows a global declaration
```

```c
// Shadowing puede ser fuente de bugs:
int count = 0;

void process(void) {
    for (int count = 0; count < 10; count++) {
        // Este count es diferente de la global
        // Si querías modificar la global, no lo estás haciendo
    }
    printf("count = %d\n", count);  // 0 — la global no cambió
}
```

## Lifetime (tiempo de vida)

El lifetime de una variable es el período durante el cual la
memoria asignada a esa variable es válida:

```c
// Hay tres lifetimes posibles:

// 1. Automatic (stack) — vive mientras el bloque está activo:
void foo(void) {
    int x = 42;               // nace aquí
    // ...
}                              // muere aquí

// 2. Static — vive durante toda la ejecución:
static int count = 0;         // nace antes de main, muere al final
int global = 10;              // nace antes de main, muere al final

// 3. Allocated (heap) — vive entre malloc y free:
int *p = malloc(sizeof(int)); // nace aquí
// ...
free(p);                       // muere aquí
```

### Scope ≠ Lifetime

```c
// Scope y lifetime son conceptos DIFERENTES:

static int count = 0;
// Scope: block scope (solo visible en la función)
// Lifetime: static (vive toda la ejecución)

int *create(void) {
    int *p = malloc(sizeof(int));
    *p = 42;
    return p;
    // Scope de p: se acaba aquí
    // Lifetime de *p: sigue viva (fue malloc'd)
    // Lifetime de p: se acaba aquí (la variable p se destruye)
    // Pero el VALOR que retornamos (la dirección) sigue siendo válido
}

int *dangerous(void) {
    int x = 42;
    return &x;
    // Scope de x: se acaba aquí
    // Lifetime de x: se acaba aquí (es automatic)
    // PELIGRO: retornamos una dirección a memoria que ya no es válida
    // → dangling pointer → UB
}
```

## Linkage (enlace)

El linkage determina si un nombre es compartido entre archivos:

```c
// 1. External linkage — visible en todos los archivos:
int global_var = 42;              // external linkage (default)
void global_func(void) { }       // external linkage (default)

// 2. Internal linkage — solo visible en este archivo:
static int file_var = 42;        // internal linkage
static void file_func(void) { }  // internal linkage

// 3. No linkage — variables locales:
void foo(void) {
    int local = 42;               // no linkage
    static int persist = 0;       // no linkage (pero lifetime static)
}
```

### Cómo interactúan scope y linkage

```c
// file_a.c
int shared = 10;              // external linkage, file scope
static int private = 20;      // internal linkage, file scope

void public_func(void) {      // external linkage
    int local = 30;            // no linkage, block scope
    static int persist = 40;   // no linkage, block scope, static lifetime
}

static void helper(void) { }  // internal linkage

// file_b.c
extern int shared;             // OK — shared tiene external linkage
// extern int private;         // ERROR de enlace — private es internal
// extern void helper(void);   // ERROR de enlace — helper es internal
```

```c
// Visibilidad completa:

// File scope + external linkage = "variable global pública"
int public_var = 42;

// File scope + internal linkage = "variable global privada"
static int private_var = 42;

// Block scope + no linkage + automatic lifetime = "variable local"
void f(void) { int x = 42; }

// Block scope + no linkage + static lifetime = "variable local persistente"
void g(void) { static int x = 42; }
```

## Parámetros de función

```c
// Los parámetros de función tienen:
// - Block scope (del cuerpo de la función)
// - Automatic lifetime (se destruyen al retornar)
// - No linkage

void process(int x, int *ptr) {
    // x y ptr son locales a esta función
    // x es una COPIA del argumento
    // ptr es una COPIA de la dirección
    x = 100;      // no afecta al llamador
    *ptr = 100;   // SÍ afecta al dato apuntado (es la misma dirección)
}
```

## Orden de inicialización

```c
// Variables con static lifetime se inicializan ANTES de main:
int a = 10;
static int b = 20;
// a y b ya tienen sus valores cuando main empieza

// El ORDEN de inicialización entre archivos NO está definido:
// file_a.c:  int x = 10;
// file_b.c:  int y = 20;
// ¿Se inicializa x antes que y? No está definido.

// Dentro de un archivo, el orden sigue la declaración:
int first = 1;     // se inicializa primero
int second = 2;    // se inicializa segundo

// Las variables static locales se inicializan la primera
// vez que se ejecuta la función:
void foo(void) {
    static int x = expensive_compute();
    // expensive_compute() se llama UNA vez, la primera vez
    // que se ejecuta foo(). En llamadas posteriores, x
    // ya tiene su valor.
}
```

## Errores comunes

### Retornar puntero a variable local

```c
// INCORRECTO:
int *bad_function(void) {
    int x = 42;
    return &x;           // x se destruye al retornar
}                        // → dangling pointer

// CORRECTO — opciones:
// 1. malloc:
int *good_malloc(void) {
    int *p = malloc(sizeof(int));
    *p = 42;
    return p;            // el caller debe hacer free
}

// 2. static:
int *good_static(void) {
    static int x = 42;
    return &x;           // OK: x vive toda la ejecución
    // CUIDADO: no es thread-safe, y siempre es la misma dirección
}

// 3. Pasar buffer del caller:
void good_output(int *out) {
    *out = 42;           // escribe en memoria del caller
}
```

### Asumir que locales se inicializan a 0

```c
// INCORRECTO:
void foo(void) {
    int x;             // NO es 0 — es indeterminado
    int arr[100];      // NO está a ceros — es basura
    int *p;            // NO es NULL — es basura

    if (x > 0) { }     // UB: leer x sin inicializar
}

// Las variables STATIC sí se inicializan a 0:
void bar(void) {
    static int x;      // SÍ es 0
    static int arr[100]; // SÍ está a ceros
    static int *p;     // SÍ es NULL
}
```

## Tabla resumen

| Declaración | Scope | Linkage | Lifetime | Init default |
|---|---|---|---|---|
| `int x;` (en función) | Block | Ninguno | Automatic | Indeterminado |
| `static int x;` (en función) | Block | Ninguno | Static | 0 |
| `int x;` (fuera de función) | File | External | Static | 0 |
| `static int x;` (fuera de función) | File | Internal | Static | 0 |
| `extern int x;` | File | External | Static | (declaración) |

---

## Ejercicios

### Ejercicio 1 — Scope

```c
// ¿Qué imprime este programa? Predecir antes de ejecutar.

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

### Ejercicio 2 — Lifetime

```c
// ¿Cuál de estos retornos es seguro? ¿Por qué?

int *func_a(void) { int x = 42; return &x; }
int *func_b(void) { static int x = 42; return &x; }
int *func_c(void) { int *p = malloc(sizeof(int)); *p = 42; return p; }
```

### Ejercicio 3 — Linkage

```c
// Crear tres archivos:
// - counter.c: variable static int count, funciones increment() y get_count()
// - logger.c: variable static int count (diferente), funciones log_msg() y get_log_count()
// - main.c: usa ambos
// Verificar que los dos count son independientes (internal linkage).
```
