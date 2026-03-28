# T03 — Calificadores

> Sin erratas detectadas en el material original.

---

## 1. Qué son los type qualifiers

Los calificadores modifican las propiedades de un tipo sin cambiar su representación en memoria:

```c
const       // el valor no se puede modificar (por esta vía)
volatile    // el valor puede cambiar sin que el código lo toque
restrict    // este puntero es el único acceso a esa memoria (C99)
```

---

## 2. `const` — No modificable

`const` indica que el valor no se puede modificar a través de esa variable:

```c
const int MAX = 100;
MAX = 200;              // ERROR: assignment of read-only variable

const double PI = 3.14159265358979;
```

### `const` NO es una constante de compilación en C

```c
// En C (a diferencia de C++), const NO significa "constante en compilación":
const int size = 10;
int arr[size];           // Crea un VLA (variable-length array), NO un array estático
                         // VLAs son opcionales en C11, pero GCC/Clang los soportan

// Para constantes de compilación reales, usar:
#define SIZE 10
int arr1[SIZE];          // OK — macro reemplazada por el preprocesador

enum { MAX = 100 };
int arr2[MAX];           // OK — enum es constant expression

// C23 introduce constexpr para esto
```

---

## 3. `const` con punteros — Cuatro combinaciones

La posición de `const` respecto al `*` cambia el significado:

```c
// 1. Puntero a const — no puedes modificar el dato:
const int *p1 = &x;       // equivalente: int const *p1 = &x;
*p1 = 42;                  // ERROR: el dato es const
p1 = &y;                   // OK: el puntero no es const

// 2. Puntero const — no puedes cambiar a dónde apunta:
int *const p2 = &x;
*p2 = 42;                  // OK: el dato no es const
p2 = &y;                   // ERROR: el puntero es const

// 3. Ambos const:
const int *const p3 = &x;
*p3 = 42;                  // ERROR
p3 = &y;                   // ERROR

// 4. Ninguno — todo mutable (default):
int *p4 = &x;
*p4 = 42;                  // OK
p4 = &y;                   // OK
```

### Regla de lectura: derecha a izquierda

```c
const int *p;
// p es un puntero (*) a int que es const
// → dato const, puntero mutable

int *const p;
// p es const, es un puntero (*) a int
// → puntero const, dato mutable

const int *const p;
// p es const, es un puntero (*) a int que es const
// → ambos const
```

### Tabla de combinaciones

| Declaración | Dato | Puntero |
|-------------|------|---------|
| `const int *p` | No modificable | Reasignable |
| `int *const p` | Modificable | No reasignable |
| `const int *const p` | No modificable | No reasignable |
| `int *p` | Modificable | Reasignable |

---

## 4. `const` en funciones (const correctness)

`const` en parámetros comunica intención y da garantías al compilador:

```c
// "No voy a modificar el string":
size_t my_strlen(const char *s) {
    size_t len = 0;
    while (s[len] != '\0') len++;
    return len;
    // s[0] = 'x';   ← ERROR de compilación si lo intentas
}

// "No voy a modificar el array":
int sum(const int *arr, size_t n) {
    int total = 0;
    for (size_t i = 0; i < n; i++)
        total += arr[i];
    return total;
}

// El caller puede pasar un array mutable sin problema:
int data[] = {1, 2, 3};
int s = sum(data, 3);       // OK: int* → const int* es seguro (agregando const)
```

Conversiones de const:
- `int*` → `const int*` : **OK** (agregar const es seguro)
- `const int*` → `int*` : **WARNING** — quitar const es peligroso, UB si el dato era realmente `const`

---

## 5. `const`: cast away y UB

```c
// const es una PROMESA, no una protección hardware.
// Se puede violar con un cast — pero puede ser UB:

// CASO 1: dato declarado const → UB
const int x = 42;
int *p = (int *)&x;          // cast away const
*p = 100;                     // UB: x fue declarado const
// El compilador puede haber puesto x en .rodata (read-only)

// CASO 2: dato original NO era const → técnicamente OK (pero mal estilo)
int y = 42;
const int *cp = &y;           // promesa de no modificar via cp
int *mp = (int *)cp;          // violar la promesa
*mp = 100;                    // Definido: y no era const
```

---

## 6. `volatile` — Puede cambiar externamente

`volatile` le dice al compilador que el valor puede cambiar por razones externas al código visible:

```c
volatile int *hardware_reg = (volatile int *)0x40021000;

// Sin volatile, el compilador puede optimizar:
int a = *hardware_reg;    // leer registro
int b = *hardware_reg;    // leer de nuevo
// → Optimizado a: int b = a;  (INCORRECTO si el hardware cambió el valor)

// Con volatile: el compilador lee de memoria CADA vez.
```

### Assembly: la diferencia real

Con `-O2`, el compilador transforma las funciones de forma muy diferente:

```c
int normal_var = 100;
volatile int volatile_var = 100;

int read_normal(void) {
    int a = normal_var;
    int b = normal_var;
    return a + b;
}
// Assembly O2: UNA lectura, luego addl %eax, %eax (duplica)

int read_volatile(void) {
    int a = volatile_var;
    int b = volatile_var;
    return a + b;
}
// Assembly O2: DOS lecturas (movl aparece dos veces), luego addl
```

### `volatile` y loops

```c
int normal_flag = 1;
volatile int volatile_flag = 1;

void loop_normal(void) {
    while (normal_flag) { }
}
// Assembly O2: lee flag UNA vez. Si es 1 → jmp .L3 (loop infinito)
// NUNCA re-lee la variable. Si algo externo la cambia, no se entera.

void loop_volatile(void) {
    while (volatile_flag) { }
}
// Assembly O2: re-lee la variable EN CADA ITERACIÓN (movl dentro del loop)
// Si algo externo la cambia a 0, el loop termina.
```

### Cuándo usar volatile

```c
// 1. Registros de hardware mapeados en memoria:
volatile uint32_t *gpio_data = (volatile uint32_t *)0x40021014;

// 2. Variables modificadas por interrupciones (embedded):
volatile int interrupt_flag = 0;
void ISR_handler(void) { interrupt_flag = 1; }
while (!interrupt_flag) { /* esperar */ }  // requiere volatile

// 3. MMIO (memory-mapped I/O)
```

---

## 7. `volatile` NO es para threads

```c
// INCORRECTO para threads:
volatile int shared = 0;

// Thread 1:
shared = 42;

// Thread 2:
while (shared == 0) { }    // volatile asegura re-lectura, PERO:

// volatile NO garantiza:
// - Atomicidad (la escritura podría ser parcial en tipos grandes)
// - Orden de memoria (otro thread podría ver un estado intermedio)
// - Visibilidad entre cores (coherencia de caches)

// CORRECTO para threads:
_Atomic int shared = 0;    // C11 atomics
// O usar mutex (mtx_lock/mtx_unlock)
```

### `volatile` + `const`

```c
// Combinación útil para registros de hardware de solo lectura:
const volatile int *status_reg = (const volatile int *)0x40021000;

// const: el código no puede escribir en este registro
// volatile: el hardware puede cambiar el valor entre lecturas
int val = *status_reg;     // OK: leer (forzado a ir a memoria)
// *status_reg = 42;       // ERROR: es const
```

---

## 8. `restrict` (C99) — Promesa de no aliasing

`restrict` promete al compilador que el puntero es el **único** acceso a esa memoria, habilitando optimizaciones que de otra forma no serían seguras:

```c
// Sin restrict — el compilador debe ser conservador:
void add_arrays(int *dst, const int *src, size_t n) {
    for (size_t i = 0; i < n; i++)
        dst[i] += src[i];
}
// Assembly O2: loop escalar, un elemento a la vez
// ¿Por qué? dst y src podrían solaparse (aliasing)

// Con restrict — el compilador puede vectorizar:
void add_arrays_fast(int *restrict dst, const int *restrict src, size_t n) {
    for (size_t i = 0; i < n; i++)
        dst[i] += src[i];
}
// Assembly O2: instrucciones SIMD (movdqu, paddd, movups)
// Procesa 4 enteros a la vez. Más código pero mucho más rápido.
```

### Por qué el aliasing impide vectorización

```c
int arr[5] = {1, 2, 3, 4, 5};
add_arrays(arr + 1, arr, 4);  // dst y src se solapan
// dst[0] = arr[1], src[1] = arr[1] — misma dirección
// Si el compilador procesara 4 a la vez, leería src[1] ANTES de que
// dst[0] lo modificara → resultado incorrecto
// Por eso, sin restrict, va elemento por elemento
```

### `restrict` en la biblioteca estándar

```c
// memcpy usa restrict: dst y src NO deben solaparse
void *memcpy(void *restrict dst, const void *restrict src, size_t n);

// memmove NO usa restrict: maneja solapamiento correctamente
void *memmove(void *dst, const void *src, size_t n);

// sprintf usa restrict:
int sprintf(char *restrict str, const char *restrict fmt, ...);
```

`restrict` solo aplica a punteros. Es relevante para funciones que operan sobre arrays grandes, copia de memoria, y código de alto rendimiento. Para código general, rara vez es necesario, pero entender su significado importa porque aparece en la biblioteca estándar.

---

## Tabla resumen

| Calificador | Significado | Uso principal |
|-------------|-------------|---------------|
| `const` | No modificable por este código | Parámetros, constantes |
| `volatile` | Puede cambiar externamente | Hardware, interrupciones |
| `restrict` | Único puntero a esta memoria | Vectorización, arrays grandes |

---

## Ejercicios

### Ejercicio 1 — const con punteros: ¿qué compila?

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex1.c -o ex1
#include <stdio.h>

int main(void) {
    const int x = 10;
    int y = 20;

    const int *p1 = &x;
    int *const p2 = &y;
    const int *const p3 = &x;

    // ¿Cuáles de estas líneas dan error?
    // *p1 = 30;       // Línea A
    p1 = &y;           // Línea B
    *p2 = 30;          // Línea C
    // p2 = &x;        // Línea D
    // *p3 = 30;       // Línea E
    // p3 = &y;        // Línea F

    printf("*p1=%d, *p2=%d, *p3=%d\n", *p1, *p2, *p3);
    return 0;
}
```

**Predicción:** De las 6 líneas (A-F), ¿cuáles dan error y cuáles compilan? ¿Qué imprime?

<details><summary>Respuesta</summary>

- A: **ERROR** — `p1` es `const int *` (dato const), no se puede escribir `*p1`
- B: **OK** — `p1` es puntero mutable, se puede reasignar
- C: **OK** — `p2` es `int *const` (puntero const, dato mutable), se puede escribir `*p2`
- D: **ERROR** — `p2` es puntero const, no se puede reasignar
- E: **ERROR** — `p3` es `const int *const` (ambos const), no se puede escribir
- F: **ERROR** — `p3` es puntero const, no se puede reasignar

```
*p1=20, *p2=30, *p3=10
```

`p1` fue reasignado a `&y` (20), `*p2` cambió `y` a 30, `p3` sigue apuntando a `x` (10).

</details>

---

### Ejercicio 2 — const correctness en función

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex2.c -o ex2
#include <stdio.h>
#include <stddef.h>

int find_max(const int *arr, size_t n) {
    int max = arr[0];
    for (size_t i = 1; i < n; i++) {
        if (arr[i] > max) max = arr[i];
    }
    // arr[0] = 999;    // ← ¿Qué pasa si descomentas?
    return max;
}

int main(void) {
    int data[] = {3, 7, 2, 9, 5, 1, 8};
    size_t n = sizeof(data) / sizeof(data[0]);
    printf("Max: %d\n", find_max(data, n));
    printf("data[0] still = %d\n", data[0]);
    return 0;
}
```

**Predicción:** ¿Qué imprime? ¿Qué error da si descomentas `arr[0] = 999`?

<details><summary>Respuesta</summary>

```
Max: 9
data[0] still = 3
```

Si descomentas `arr[0] = 999`:
```
error: assignment of read-only location '*arr'
```

`const int *arr` garantiza que `find_max` no puede modificar los datos a través de `arr`. El caller pasa `int[]` (mutable) a `const int *` — esta conversión es siempre segura (agregar const).

</details>

---

### Ejercicio 3 — Quitar const: warning y UB

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex3.c -o ex3
#include <stdio.h>

int main(void) {
    int mutable = 42;
    const int immutable = 42;

    // Caso 1: quitar const de dato originalmente mutable
    const int *cp1 = &mutable;
    int *mp1 = (int *)cp1;
    *mp1 = 100;
    printf("mutable = %d\n", mutable);

    // Caso 2: quitar const de dato originalmente const
    const int *cp2 = &immutable;
    int *mp2 = (int *)cp2;
    // *mp2 = 100;    // ← ¿UB?
    printf("immutable = %d\n", immutable);

    return 0;
}
```

**Predicción:** ¿El caso 1 es UB? ¿El caso 2 (si se descomenta) es UB? ¿Por qué la diferencia?

<details><summary>Respuesta</summary>

```
mutable = 100
immutable = 42
```

- Caso 1: **No es UB** — `mutable` fue declarada sin `const`, así que modificarla a través de un puntero es válido (aunque es mal estilo quitar const).
- Caso 2 (si se descomenta): **UB** — `immutable` fue declarada `const`. El compilador puede haberla colocado en `.rodata` (memoria de solo lectura) o asumido que su valor nunca cambia para optimizaciones. Escribir en ella a través de un cast viola esa garantía.

La diferencia: lo que importa es la declaración **original** del dato, no el tipo del puntero que usas para acceder a él.

</details>

---

### Ejercicio 4 — volatile: assembly de lectura doble

```c
// Compila DOS versiones:
//   gcc -std=c11 -S -O2 ex4.c -o ex4.s
// Luego busca read_normal y read_volatile en ex4.s
#include <stdio.h>

int normal_var = 100;
volatile int volatile_var = 100;

int read_normal(void) {
    int a = normal_var;
    int b = normal_var;
    return a + b;
}

int read_volatile(void) {
    int a = volatile_var;
    int b = volatile_var;
    return a + b;
}

int main(void) {
    printf("normal:   %d\n", read_normal());
    printf("volatile: %d\n", read_volatile());
    return 0;
}
```

**Predicción:** Con `-O2`, ¿cuántas instrucciones `movl` tendrá cada función? ¿Ambas dan el mismo resultado?

<details><summary>Respuesta</summary>

```
normal:   200
volatile: 200
```

Ambas dan 200. Pero el assembly es diferente:

- `read_normal`: **1 `movl`** + `addl %eax, %eax`. El compilador lee `normal_var` una vez y duplica el valor.
- `read_volatile`: **2 `movl`**. El compilador lee `volatile_var` de memoria dos veces porque `volatile` prohíbe reutilizar el valor anterior.

En este caso el resultado es igual, pero si un hardware/interrupción cambiara `volatile_var` entre las dos lecturas, `a` y `b` tendrían valores diferentes.

</details>

---

### Ejercicio 5 — volatile: loop optimizado

```c
// Compila: gcc -std=c11 -S -O2 ex5.c -o ex5.s
// NO ejecutar — contiene loops potencialmente infinitos
#include <stdio.h>

int flag_normal = 1;
volatile int flag_volatile = 1;

void wait_normal(void) {
    while (flag_normal) { }    // ← ¿Qué genera con -O2?
}

void wait_volatile(void) {
    while (flag_volatile) { }  // ← ¿Y esta?
}

int main(void) {
    printf("Inspect assembly only. Do not run.\n");
    return 0;
}
```

**Predicción:** Con `-O2`, ¿`wait_normal` se convierte en loop infinito que nunca re-lee `flag`?

<details><summary>Respuesta</summary>

`wait_normal` con `-O2`:
```asm
wait_normal:
    movl    flag_normal(%rip), %eax   # lee UNA vez
    testl   %eax, %eax
    jne     .L3                        # si ≠ 0, saltar a loop
    ret
.L3:
    jmp     .L3                        # loop infinito: nunca re-lee
```

`wait_volatile` con `-O2`:
```asm
wait_volatile:
.L6:
    movl    flag_volatile(%rip), %eax  # re-lee CADA iteración
    testl   %eax, %eax
    jne     .L6
    ret
```

Sin `volatile`, el compilador razona: "nadie en mi código modifica `flag_normal` dentro del loop, así que no puede cambiar → no necesito re-leerla". Esto es correcto para código single-threaded normal, pero incorrecto si algo externo (interrupción, hardware, signal handler) puede modificar la variable.

</details>

---

### Ejercicio 6 — restrict: comparar assembly

```c
// Compila: gcc -std=c11 -S -O2 ex6.c -o ex6.s
// Buscar instrucciones SIMD (movdqu, paddd, movups) en add_restrict
#include <stddef.h>

void add_no_restrict(int *dst, const int *src, size_t n) {
    for (size_t i = 0; i < n; i++)
        dst[i] += src[i];
}

void add_restrict(int *restrict dst, const int *restrict src, size_t n) {
    for (size_t i = 0; i < n; i++)
        dst[i] += src[i];
}
```

**Predicción:** ¿Cuál versión usará instrucciones SIMD (procesando 4 ints a la vez)?

<details><summary>Respuesta</summary>

- `add_no_restrict`: loop escalar, un elemento a la vez (`movl`, `addl` individualmente). El compilador no puede vectorizar porque `dst` y `src` podrían solaparse.
- `add_restrict`: instrucciones SIMD como `movdqu` (cargar 128 bits = 4 ints), `paddd` (sumar 4 ints en paralelo), `movups` (almacenar resultado). Más líneas de assembly (incluye loop de respaldo para elementos sobrantes), pero mucho más rápido.

`restrict` habilita la vectorización al garantizar que `dst` y `src` no apuntan a memoria solapada.

</details>

---

### Ejercicio 7 — memcpy vs memmove

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex7.c -o ex7
#include <stdio.h>
#include <string.h>

int main(void) {
    char str1[] = "ABCDEFGHIJ";
    char str2[] = "ABCDEFGHIJ";

    // Caso 1: regiones NO solapadas — ambas funcionan
    char dst[11];
    memcpy(dst, str1, 11);
    printf("memcpy non-overlap: %s\n", dst);

    // Caso 2: regiones solapadas — solo memmove es seguro
    memmove(str1 + 2, str1, 8);   // solapamiento: OK con memmove
    printf("memmove overlap:    %s\n", str1);

    // memcpy con solapamiento es UB (el resultado puede ser correcto
    // o incorrecto dependiendo de la implementación):
    memcpy(str2 + 2, str2, 8);    // UB: viola restrict
    printf("memcpy overlap:     %s\n", str2);

    return 0;
}
```

**Predicción:** ¿Qué produce `memmove` con solapamiento? ¿Y `memcpy`?

<details><summary>Respuesta</summary>

```
memcpy non-overlap: ABCDEFGHIJ
memmove overlap:    ABABCDEFGH
memcpy overlap:     ABABCDEFGH   ← puede variar (UB)
```

`memmove(str1+2, str1, 8)` copia "ABCDEFGH" a la posición str1+2 correctamente, produciendo "ABABCDEFGH". `memmove` maneja el solapamiento copiando en la dirección correcta (desde el final si es necesario).

`memcpy` con solapamiento es **UB** porque viola la promesa `restrict`. En la práctica, muchas implementaciones de `memcpy` dan el resultado "correcto" porque internamente copian de forma similar a `memmove`, pero el estándar no lo garantiza.

</details>

---

### Ejercicio 8 — const volatile: registro de solo lectura

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex8.c -o ex8
#include <stdio.h>

int main(void) {
    int underlying = 42;
    const volatile int *status = (const volatile int *)&underlying;

    printf("status = %d\n", *status);

    // *status = 99;    // ← ¿Compila?

    // Simular cambio externo (en hardware real, el registro cambiaría solo):
    underlying = 99;
    printf("status = %d\n", *status);

    return 0;
}
```

**Predicción:** ¿La línea comentada compila? ¿Qué significado tiene `const volatile` junto?

<details><summary>Respuesta</summary>

```
status = 42
status = 99
```

`*status = 99` **no compila**: `const` prohíbe escribir a través de este puntero. `const volatile` significa:
- `const`: **nuestro código** no puede escribir
- `volatile`: **algo externo** puede cambiar el valor (hardware, DMA, otro core)

El compilador debe leer de memoria cada vez (volatile), pero no permite que el código escriba (const). Es el patrón ideal para registros de hardware de solo lectura (status registers).

</details>

---

### Ejercicio 9 — const con arrays

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex9.c -o ex9
#include <stdio.h>

void print_array(const int *arr, int n) {
    for (int i = 0; i < n; i++)
        printf("%d ", arr[i]);
    printf("\n");
}

void zero_array(int *arr, int n) {
    for (int i = 0; i < n; i++)
        arr[i] = 0;
}

int main(void) {
    int data[] = {10, 20, 30};

    print_array(data, 3);     // ← ¿Compila?
    zero_array(data, 3);      // ← ¿Compila?
    print_array(data, 3);

    const int cdata[] = {40, 50, 60};
    print_array(cdata, 3);    // ← ¿Compila?
    // zero_array(cdata, 3);  // ← ¿Compila?

    return 0;
}
```

**Predicción:** ¿Cuáles de las 4 llamadas compilan? ¿Qué imprime?

<details><summary>Respuesta</summary>

```
10 20 30
0 0 0
40 50 60
```

- `print_array(data, 3)`: **compila** — `int*` → `const int*` (agregar const es seguro)
- `zero_array(data, 3)`: **compila** — `int*` → `int*` (mismo tipo)
- `print_array(cdata, 3)`: **compila** — `const int*` → `const int*` (mismo tipo)
- `zero_array(cdata, 3)`: **no compila** — `const int*` → `int*` (quitar const → warning/error). `zero_array` podría modificar el array, pero `cdata` es `const`.

</details>

---

### Ejercicio 10 — restrict: violación

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic -O2 ex10.c -o ex10
#include <stdio.h>
#include <stddef.h>

void copy_restrict(int *restrict dst, const int *restrict src, size_t n) {
    for (size_t i = 0; i < n; i++)
        dst[i] = src[i];
}

int main(void) {
    int arr[] = {1, 2, 3, 4, 5};

    // Caso 1: sin solapamiento — correcto
    int dst[5];
    copy_restrict(dst, arr, 5);
    printf("No overlap: ");
    for (int i = 0; i < 5; i++) printf("%d ", dst[i]);
    printf("\n");

    // Caso 2: con solapamiento — viola restrict (UB)
    copy_restrict(arr + 1, arr, 4);  // dst y src se solapan
    printf("Overlap (UB): ");
    for (int i = 0; i < 5; i++) printf("%d ", arr[i]);
    printf("\n");

    return 0;
}
```

**Predicción:** ¿El caso 2 dará el resultado "esperado" {1,1,2,3,4}? ¿Es confiable?

<details><summary>Respuesta</summary>

```
No overlap: 1 2 3 4 5
Overlap (UB): 1 1 2 3 4     ← resultado PUEDE variar
```

El caso 1 funciona correctamente. El caso 2 es **UB** porque `dst` (arr+1) y `src` (arr) se solapan, violando la promesa `restrict`. El resultado `1 1 2 3 4` es lo que se obtendría con copia secuencial, pero con `-O2` y vectorización SIMD, el compilador podría leer 4 elementos de `src` antes de escribir en `dst`, produciendo `1 1 2 3 4` o `1 1 1 1 1` o cualquier otro resultado. No es confiable — para copias con solapamiento, usar `memmove`.

</details>
