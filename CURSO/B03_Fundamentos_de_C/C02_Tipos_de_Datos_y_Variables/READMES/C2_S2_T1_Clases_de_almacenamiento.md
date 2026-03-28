# T01 — Clases de almacenamiento

> Sin erratas detectadas en el material original.

---

## 1. Qué es una storage class

La storage class de una variable determina tres cosas: **dónde** se almacena en memoria, **cuánto tiempo** vive (lifetime), y **quién puede verla** (linkage).

```c
// C tiene 5 storage class specifiers:
auto           // variable local (default, casi nunca se escribe)
static         // persiste entre llamadas / limita visibilidad
extern         // declaración de variable definida en otro archivo
register       // sugerencia para poner en registro (obsoleto)
_Thread_local  // una copia por thread (C11)
```

Solo se puede aplicar **un** storage class specifier a una variable (excepto `_Thread_local`, que se combina con `static` o `extern`).

---

## 2. `auto` — Variables locales (stack)

`auto` es la storage class por defecto de las variables locales. **Nunca se escribe** porque es redundante:

```c
void foo(void) {
    auto int x = 42;    // auto es el default — no se escribe
    int x = 42;          // exactamente lo mismo
}
```

Características de `auto`:
- Se almacena en el **stack**
- Se crea al entrar al bloque, se destruye al salir
- **NO se inicializa** automáticamente (valor indeterminado)
- Solo visible dentro del bloque donde fue declarada

```c
void example(void) {
    int x = 10;          // se crea aquí (stack)
    if (x > 5) {
        int y = 20;      // se crea aquí (stack)
        // x y y son visibles
    }
    // y ya no existe (fue destruida al salir del if)
}
// x se destruye aquí
```

Nota sobre C23: `auto` cambia de significado — deduce el tipo (como en C++). `auto x = 42;` infiere `int`. En C11, `auto` es solo un storage class specifier inútil.

---

## 3. `static` — Dos significados diferentes

### 3a. `static` en variable local → persistencia

```c
void counter(void) {
    static int count = 0;   // se inicializa UNA sola vez
    count++;
    printf("count = %d\n", count);
}

int main(void) {
    counter();    // count = 1
    counter();    // count = 2
    counter();    // count = 3
    return 0;
}
```

Sin `static`, `count` se reinicializaría a 0 en cada llamada (siempre imprimiría 1).

Propiedades de `static` local:
- Se almacena en el segmento **Data** (si inicializada ≠ 0) o **BSS** (si = 0 o sin inicializador)
- Vive durante **toda la ejecución** del programa
- Se inicializa **una vez** (antes de `main`)
- Si no se inicializa explícitamente, se **garantiza en 0** (no basura)
- El **scope no cambia** — sigue siendo local a la función

Usos comunes:

```c
// 1. Generador de IDs:
int get_id(void) {
    static int next_id = 0;
    return next_id++;
}

// 2. Buffer que debe persistir (CUIDADO: no thread-safe):
const char *error_message(int code) {
    static char buffer[256];
    snprintf(buffer, sizeof(buffer), "Error %d", code);
    return buffer;    // válido porque buffer es static
}

// 3. Inicialización costosa una sola vez:
const int *get_lookup_table(void) {
    static int table[256];
    static int initialized = 0;
    if (!initialized) {
        for (int i = 0; i < 256; i++)
            table[i] = compute(i);
        initialized = 1;
    }
    return table;
}
```

### 3b. `static` en scope de archivo → linkage interno

```c
// math_utils.c

static int helper(int x) {      // INVISIBLE fuera de este archivo
    return x * 2;
}

int public_add(int a, int b) {   // visible desde otros archivos
    return helper(a) + helper(b);
}

static int internal_counter = 0; // INVISIBLE fuera de este archivo
```

```c
// main.c
extern int public_add(int a, int b);  // OK
extern int helper(int x);             // ERROR de enlace — helper es static
```

`static` a nivel de archivo:
- Limita el linkage a **interno** (solo este `.c`)
- Evita conflictos de nombres entre archivos
- Permite al compilador optimizar más agresivamente (puede inline, eliminar, etc.)
- Es la forma de C de tener **encapsulación**

---

## 4. `extern` — Declaración sin definición

`extern` dice "esta variable existe, pero está definida en otro archivo":

```c
// config.c — DEFINICIÓN (reserva memoria):
int debug_level = 0;
int max_connections = 100;

// main.c — DECLARACIÓN (no reserva memoria):
extern int debug_level;
extern int max_connections;
```

### Declaración vs definición

```c
int x = 42;           // DEFINICIÓN: reserva memoria, asigna valor
int x;                // DEFINICIÓN TENTATIVA: reserva memoria, init a 0
extern int x;         // DECLARACIÓN: no reserva memoria, solo anuncia

// Puede haber múltiples declaraciones, pero solo UNA definición.
// Dos definiciones → error de enlace: "multiple definition of x"
// Ninguna definición → error de enlace: "undefined reference to x"
```

### El patrón header/source

```c
// config.h — declaraciones:
#ifndef CONFIG_H
#define CONFIG_H
extern int debug_level;
extern int max_connections;
void config_init(void);
#endif

// config.c — definiciones:
#include "config.h"
int debug_level = 0;
int max_connections = 100;
void config_init(void) { /* ... */ }

// main.c — uso:
#include "config.h"    // obtiene las declaraciones extern
int main(void) {
    config_init();
    printf("debug: %d\n", debug_level);  // accede a la variable de config.c
    return 0;
}
```

Regla:
- En el `.h`: `extern int variable;` (declaración)
- En el `.c`: `int variable = valor;` (definición)
- Sin `extern` en el `.h` → `int variable;` sería definición tentativa → si dos `.c` incluyen el `.h` → "multiple definition"

### `extern` con funciones

```c
// Las funciones son extern por defecto:
int add(int a, int b);           // extern implícito
extern int add(int a, int b);   // redundante

// Por eso los prototipos en headers no necesitan extern.
// Solo se usa extern explícito para variables globales.
```

---

## 5. `register` — Sugerencia obsoleta

```c
register int i;    // "sugiere" poner i en un registro del CPU

// En la práctica:
// - Los compiladores modernos IGNORAN register completamente
// - El compilador decide mejor que el programador
// - Única restricción real: no puedes tomar la dirección (&i → error)
// - Deprecated en C23
// - No usar en código moderno
```

Se puede verificar empíricamente: el assembly generado con y sin `register` es idéntico. El compilador asigna registros basándose en su propio análisis, no en las sugerencias del programador.

---

## 6. `_Thread_local` (C11)

Crea una **copia independiente** de la variable para cada thread:

```c
#include <threads.h>

_Thread_local int counter = 0;   // cada thread tiene su propia copia

int worker(void *arg) {
    int id = *(int *)arg;
    for (int i = 0; i < 5; i++)
        counter++;               // modifica SU copia
    printf("Thread %d: counter = %d\n", id, counter);
    return 0;
}
// Thread 1: counter = 5
// Thread 2: counter = 5
// Cada thread incrementó su propia copia de 0 a 5
```

Propiedades:
- Cada thread tiene su propia instancia
- Se inicializa con el valor original para cada thread nuevo
- No necesita mutex para acceder (es privada del thread)
- Se almacena en **TLS** (Thread-Local Storage)
- Se combina con `static` o `extern`:
  - `static _Thread_local int x;` → thread-local + linkage interno
  - `extern _Thread_local int x;` → thread-local + linkage externo
- GCC también soporta `__thread` como extensión equivalente

---

## 7. Mapa de memoria

```
Segmento                Qué vive ahí
────────────────────    ──────────────────────────────────
Stack                   Variables locales (auto)
                        Parámetros de función
                        Dirección de retorno

Data (.data)            Variables globales inicializadas (≠ 0)
                        Variables static inicializadas (≠ 0)

BSS (.bss)              Variables globales sin inicializar / = 0
                        Variables static sin inicializar / = 0
                        (el SO las llena con ceros al cargar)

Heap                    Memoria de malloc/calloc/realloc

Text (.text)            Instrucciones del programa
Read-only (.rodata)     String literals, constantes

TLS                     Variables _Thread_local
```

```c
int global_init = 42;          // .data (inicializado ≠ 0)
int global_no_init;            // .bss (se inicializa a 0)
static int file_scope = 10;   // .data

void foo(void) {
    int local = 5;             // Stack
    static int persist = 0;    // .bss (inicializado a 0)
    int *heap = malloc(100);   // puntero en Stack, 100 bytes en Heap
    const char *s = "hello";   // puntero en Stack, "hello" en .rodata
}
```

### Verificar con herramientas

```bash
# nm — tabla de símbolos:
nm file.o
# D/d = .data (inicializado)    B/b = .bss (sin inicializar)
# T/t = .text (código)          U = undefined (pendiente de enlace)
# Mayúscula = linkage externo   Minúscula = linkage interno (static)

# objdump — secciones:
objdump -h file.o | grep -E "\.text|\.data|\.bss|\.rodata"

# size — resumen de tamaños:
size file.o
#    text    data     bss     dec     hex filename
```

Nota: las variables locales (`auto`) **no aparecen** en `nm` porque viven en el stack y solo existen en runtime.

---

## 8. Tabla resumen

| Specifier | Almacenamiento | Lifetime | Scope | Init default | Notas |
|-----------|---------------|----------|-------|-------------|-------|
| `auto` (default) | Stack | Bloque | Bloque | Indeterminado | Nunca se escribe |
| `static` (local) | Data/BSS | Programa | Bloque | 0 | Persiste entre llamadas |
| `static` (archivo) | Data/BSS | Programa | Archivo | 0 | Linkage interno |
| `extern` | Data/BSS | Programa | Multi-archivo | 0 | Solo declaración |
| `register` | Registro/Stack | Bloque | Bloque | Indeterminado | Obsoleto |
| `_Thread_local` | TLS | Thread | Depende | 0 | C11 |

---

## Ejercicios

### Ejercicio 1 — auto vs static: predicción

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex1.c -o ex1
#include <stdio.h>

void counter_auto(void) {
    int count = 0;
    count++;
    printf("  auto:   count = %d\n", count);
}

void counter_static(void) {
    static int count = 0;
    count++;
    printf("  static: count = %d\n", count);
}

int main(void) {
    for (int i = 1; i <= 3; i++) {
        printf("Call %d:\n", i);
        counter_auto();
        counter_static();
    }
    return 0;
}
```

**Predicción:** ¿Qué valores imprimirá cada función en las 3 llamadas?

<details><summary>Respuesta</summary>

```
Call 1:
  auto:   count = 1
  static: count = 1
Call 2:
  auto:   count = 1
  static: count = 2
Call 3:
  auto:   count = 1
  static: count = 3
```

`counter_auto()` siempre imprime 1: la variable `count` se crea en el stack a 0, se incrementa a 1, y se destruye al salir. `counter_static()` acumula: `count` vive en .data/.bss, se inicializa una vez, y conserva su valor entre llamadas.

</details>

---

### Ejercicio 2 — Inicialización por defecto

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex2.c -o ex2
#include <stdio.h>

static int s_int;
static double s_double;
static char s_char;
static int *s_ptr;

int main(void) {
    printf("static int:    %d\n", s_int);
    printf("static double: %f\n", s_double);
    printf("static char:   %d\n", s_char);
    printf("static ptr:    %p\n", (void *)s_ptr);

    int local_int;
    printf("\nlocal int (sin init): probablemente basura\n");
    // Leer local_int sin inicializar es comportamiento indeterminado.
    // No lo hacemos — solo demostramos que static sí se garantiza en 0.
    return 0;
}
```

**Predicción:** ¿Qué valores tendrán las variables `static` no inicializadas?

<details><summary>Respuesta</summary>

```
static int:    0
static double: 0.000000
static char:   0
static ptr:    (nil)
```

Todas las variables con storage duration estática (globales y `static`) se inicializan a **cero** si no tienen inicializador explícito (C11 §6.7.9p10). Enteros a 0, flotantes a 0.0, punteros a NULL. Las variables `auto` (locales) **no** tienen esta garantía — su valor es indeterminado.

</details>

---

### Ejercicio 3 — Generador de IDs con static

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex3.c -o ex3
#include <stdio.h>

int next_id(void) {
    static int id = 0;
    return ++id;
}

int main(void) {
    for (int i = 0; i < 5; i++) {
        printf("ID: %d\n", next_id());
    }
    printf("Next: %d\n", next_id());
    return 0;
}
```

**Predicción:** ¿Qué IDs se generarán? ¿El `= 0` se ejecuta en cada llamada?

<details><summary>Respuesta</summary>

```
ID: 1
ID: 2
ID: 3
ID: 4
ID: 5
Next: 6
```

El `= 0` **no** se ejecuta en cada llamada — solo define el valor inicial (una vez, antes de `main`). Cada llamada a `next_id()` incrementa el mismo `id` que persiste en .bss. Esto funciona como un contador global pero con el scope limitado a la función.

</details>

---

### Ejercicio 4 — extern entre archivos

```c
// Archivo 1: config.c
// Compila: gcc -std=c11 -c config.c -o config.o
#include <stdio.h>

int verbose = 0;
int max_retries = 3;

void config_print(void) {
    printf("verbose=%d, max_retries=%d\n", verbose, max_retries);
}
```

```c
// Archivo 2: main.c
// Compila: gcc -std=c11 -c main.c -o main.o
// Enlaza: gcc config.o main.o -o program
#include <stdio.h>

extern int verbose;
extern int max_retries;
extern void config_print(void);

int main(void) {
    config_print();
    verbose = 1;
    max_retries = 5;
    config_print();
    return 0;
}
```

**Predicción:** ¿`config_print()` verá los cambios hechos en `main.c`? ¿Por qué?

<details><summary>Respuesta</summary>

```
verbose=0, max_retries=3
verbose=1, max_retries=5
```

Sí, `config_print()` ve los cambios porque `extern` no crea una copia — ambos archivos acceden a la **misma posición de memoria**. `verbose` y `max_retries` están definidas una sola vez en `config.c`, y `main.c` solo declara que existen. El enlazador resuelve las referencias a la misma dirección.

</details>

---

### Ejercicio 5 — Símbolos con nm

```c
// Compila como objeto: gcc -std=c11 -Wall -Wextra -Wpedantic -c ex5.c -o ex5.o
// Luego: nm ex5.o
#include <stdio.h>

int global_a = 10;
int global_b;
static int file_private = 20;
static int file_zero;

void public_func(void) {
    static int local_static = 30;
    int local_auto = 40;
    printf("%d %d %d %d %d %d\n",
           global_a, global_b, file_private,
           file_zero, local_static, local_auto);
}
```

**Predicción:** Para cada variable, indica: (a) qué letra tendrá en `nm` (D, d, B, b, o no aparece), y (b) por qué.

<details><summary>Respuesta</summary>

```
nm ex5.o:
... D global_a          ← .data, linkage externo (mayúscula)
... B global_b          ← .bss, linkage externo (no inicializado)
... d file_private      ← .data, linkage interno (static → minúscula)
... b file_zero         ← .bss, linkage interno (static + no init)
... d local_static.0    ← .data, linkage interno (static local)
... T public_func       ← .text, linkage externo
    U printf            ← undefined (se resuelve al enlazar)
```

`local_auto` **no aparece** en `nm` — vive en el stack y solo existe en runtime. La diferencia mayúscula/minúscula indica linkage: `D` (externo, visible fuera) vs `d` (interno, static). `global_b` va a BSS porque no tiene inicializador.

</details>

---

### Ejercicio 6 — static file scope: encapsulación

```c
// Archivo 1: helpers.c
// Compila: gcc -std=c11 -c helpers.c -o helpers.o
static int square(int x) {
    return x * x;
}

int compute(int x) {
    return square(x) + 1;
}
```

```c
// Archivo 2: main.c
// Compila: gcc -std=c11 -c main.c -o main.o
// Enlaza: gcc helpers.o main.o -o program
#include <stdio.h>

extern int compute(int x);
// extern int square(int x);  // ← ¿Qué pasa si descomentas esto?

int main(void) {
    printf("compute(5) = %d\n", compute(5));
    // printf("square(5) = %d\n", square(5));  // ← ¿Y esto?
    return 0;
}
```

**Predicción:** ¿Qué ocurre si descomentas la declaración y llamada a `square`?

<details><summary>Respuesta</summary>

```
compute(5) = 26
```

Si descomentas ambas líneas e intentas compilar/enlazar, obtendrás:
```
undefined reference to `square'
```

`square` es `static` en `helpers.c`, lo que le da **linkage interno** — solo es visible dentro de `helpers.c`. El enlazador no puede encontrarla desde `main.o`. Esto es encapsulación: `compute` es la API pública, `square` es un detalle de implementación privado.

</details>

---

### Ejercicio 7 — Errores de enlace

```c
// Solo main.c (sin config.c):
// Compila: gcc -std=c11 main_alone.c -o fail
#include <stdio.h>

extern int missing_var;
extern void missing_func(void);

int main(void) {
    printf("%d\n", missing_var);
    missing_func();
    return 0;
}
```

**Predicción:** ¿Compilará? ¿Enlazará? ¿Qué errores producirá?

<details><summary>Respuesta</summary>

**Compilará** sin errores — `extern` solo dice "esto existe en algún lado", y el compilador confía en esa promesa. Pero **no enlazará**:

```
undefined reference to `missing_var'
undefined reference to `missing_func'
```

La compilación (`gcc -c`) genera el `.o` sin problemas. El error aparece en la fase de enlace, cuando el enlazador busca las definiciones y no las encuentra en ningún `.o` ni biblioteca.

</details>

---

### Ejercicio 8 — Direcciones en runtime

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex8.c -o ex8
#include <stdio.h>
#include <stdlib.h>

int global = 42;
static int file_static = 100;

int main(void) {
    int stack_var = 7;
    static int local_static = 99;
    int *heap_var = malloc(sizeof(int));

    printf("global:       %p\n", (void *)&global);
    printf("file_static:  %p\n", (void *)&file_static);
    printf("local_static: %p\n", (void *)&local_static);
    printf("stack_var:    %p\n", (void *)&stack_var);
    printf("heap_var:     %p\n", (void *)heap_var);

    free(heap_var);
    return 0;
}
```

**Predicción:** ¿Las variables globales/static tendrán direcciones similares entre sí? ¿Y la variable de stack? ¿Y la de heap?

<details><summary>Respuesta</summary>

```
global:       0x40XXXX          ← segmento data (bajo)
file_static:  0x40XXXX          ← segmento data (cerca de global)
local_static: 0x40XXXX          ← segmento data (cerca de las otras)
stack_var:    0x7fXXXXXXXXXX    ← stack (alto)
heap_var:     0x55XXXX o similar ← heap (entre data y stack)
```

Las tres variables con storage estática (`global`, `file_static`, `local_static`) tienen direcciones en el rango bajo (`0x40XXXX`), agrupadas en .data. La variable de stack está en el rango alto (`0x7fXX...`). La de heap está en un rango intermedio. Esto refleja el layout de memoria del proceso en Linux: text/data abajo, heap creciendo hacia arriba, stack creciendo hacia abajo.

</details>

---

### Ejercicio 9 — register: verificar que se ignora

```c
// Compila DOS versiones del assembly:
//   gcc -std=c11 -S -O0 ex9.c -o ex9_O0.s
//   gcc -std=c11 -S -O2 ex9.c -o ex9_O2.s
// Luego compara: diff ex9_reg.s ex9_noreg.s (renombrando)

#include <stdio.h>

int sum_register(const int *arr, int n) {
    register int total = 0;
    for (register int i = 0; i < n; i++)
        total += arr[i];
    return total;
}

int sum_normal(const int *arr, int n) {
    int total = 0;
    for (int i = 0; i < n; i++)
        total += arr[i];
    return total;
}

int main(void) {
    int data[] = {10, 20, 30, 40, 50};
    printf("register: %d\n", sum_register(data, 5));
    printf("normal:   %d\n", sum_normal(data, 5));
    return 0;
}
```

**Predicción:** ¿El assembly de ambas funciones será idéntico? ¿`register` hace alguna diferencia?

<details><summary>Respuesta</summary>

```
register: 150
normal:   150
```

El assembly es **idéntico** (o prácticamente idéntico) tanto con `-O0` como con `-O2`. GCC ignora `register` por completo — la única diferencia es que `register` prohíbe tomar la dirección con `&`. Con `-O2`, ambas funciones se optimizan agresivamente (posiblemente inlineadas), pero las optimizaciones las decide el compilador, no el keyword.

</details>

---

### Ejercicio 10 — Static local: no thread-safe

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex10.c -o ex10
#include <stdio.h>

const char *format_number(int n) {
    static char buffer[64];
    snprintf(buffer, sizeof(buffer), "[%d]", n);
    return buffer;
}

int main(void) {
    const char *a = format_number(42);
    printf("a = %s\n", a);

    const char *b = format_number(99);
    printf("b = %s\n", b);
    printf("a = %s\n", a);  // ← ¿Qué imprime AHORA?

    printf("Same address? %s\n", a == b ? "YES" : "NO");
    return 0;
}
```

**Predicción:** Después de la segunda llamada, ¿qué contiene `a`? ¿`a` y `b` apuntan a la misma dirección?

<details><summary>Respuesta</summary>

```
a = [42]
b = [99]
a = [99]
Same address? YES
```

`a` y `b` apuntan al **mismo buffer static**. La segunda llamada a `format_number(99)` sobrescribió el contenido de `buffer` con `[99]`. Como `a` apunta al mismo buffer, ahora `a` también muestra `[99]`. Este es el peligro clásico de retornar punteros a buffers `static`: la siguiente llamada sobrescribe el resultado anterior. En código multihilo, el problema es aún peor (race condition).

</details>
