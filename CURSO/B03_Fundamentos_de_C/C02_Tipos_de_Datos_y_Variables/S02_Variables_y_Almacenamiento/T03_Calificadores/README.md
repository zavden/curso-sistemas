# T03 — Calificadores

## Qué son los type qualifiers

Los calificadores modifican las propiedades de un tipo sin
cambiar su representación en memoria. C tiene tres:

```c
const       // el valor no se puede modificar (por esta vía)
volatile    // el valor puede cambiar sin que el código lo toque
restrict   // este puntero es el único acceso a esa memoria (C99)
```

## const — No modificable

`const` indica que el valor no se puede modificar a través de
esa variable. Es una promesa del programador al compilador:

```c
const int MAX = 100;
MAX = 200;              // ERROR de compilación: assignment of read-only variable

const double PI = 3.14159265358979;
```

### const NO es una constante de compilación

```c
// En C, const NO significa "constante en compilación":
const int size = 10;
int arr[size];           // ERROR en C11 estricto (no es constant expression)
                         // Funciona en GCC/Clang pero no es estándar

// Para constantes de compilación, usar:
#define SIZE 10
int arr[SIZE];           // OK — el preprocesador reemplaza

enum { MAX = 100 };
int arr2[MAX];           // OK — enum es constant expression

// C23 introduce constexpr para esto (ver T04 de S03).
```

### const con punteros — Cuatro combinaciones

```c
// La posición de const respecto al * cambia el significado:

// 1. Puntero a const — no puedes modificar el dato:
const int *p1 = &x;       // o: int const *p1 = &x;
*p1 = 42;                  // ERROR: el dato es const
p1 = &y;                   // OK: el puntero no es const

// 2. Puntero const — no puedes cambiar a dónde apunta:
int *const p2 = &x;
*p2 = 42;                  // OK: el dato no es const
p2 = &y;                   // ERROR: el puntero es const

// 3. Ambos const — ni el dato ni el puntero se pueden modificar:
const int *const p3 = &x;
*p3 = 42;                  // ERROR
p3 = &y;                   // ERROR

// 4. Ninguno — todo mutable (el default):
int *p4 = &x;
*p4 = 42;                  // OK
p4 = &y;                   // OK
```

### Regla de lectura: derecha a izquierda

```c
// Leer la declaración de derecha a izquierda:

const int *p;
// p es un puntero (*) a int que es const
// → el int es const, el puntero no

int *const p;
// p es const, es un puntero (*) a int
// → el puntero es const, el int no

const int *const p;
// p es const, es un puntero (*) a int que es const
// → ambos son const
```

### const en parámetros de función

```c
// const en parámetros comunica intención:

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
    for (size_t i = 0; i < n; i++) {
        total += arr[i];
    }
    return total;
}

// El caller puede pasar un array mutable sin problema:
int data[] = {1, 2, 3};
int s = sum(data, 3);       // OK: int* → const int* es seguro
```

```c
// Conversión de const:

// int* → const int* : OK (agregando const — seguro)
int x = 42;
const int *cp = &x;         // OK

// const int* → int* : WARNING (quitando const — peligroso)
const int y = 42;
int *mp = &y;               // WARNING: discards 'const'
*mp = 100;                  // UB si y era realmente const
```

### const no impide la modificación

```c
// const es una PROMESA, no una protección hardware.
// Se puede violar con un cast (pero es UB si el dato era realmente const):

const int x = 42;
int *p = (int *)&x;          // cast away const
*p = 100;                     // UB: x fue declarado const
// El compilador puede haber puesto x en .rodata

// Pero si el dato original NO era const:
int y = 42;
const int *cp = &y;           // promesa: no modificar via cp
int *mp = (int *)cp;          // violar la promesa
*mp = 100;                    // OK técnicamente: y no era const
// Pero es mal estilo — no hacer esto
```

## volatile — Puede cambiar sin que lo toques

`volatile` le dice al compilador que el valor puede cambiar
por razones externas al código:

```c
volatile int *hardware_reg = (volatile int *)0x40021000;

// Sin volatile, el compilador podría optimizar esto:
int value = *hardware_reg;    // leer el registro
// ... hacer algo ...
int value2 = *hardware_reg;   // leer de nuevo

// El compilador piensa: "ya leí hardware_reg y nadie lo cambió
// en mi código, puedo reutilizar el primer valor"
// → Optimiza a: int value2 = value;  (INCORRECTO)

// Con volatile: el compilador lee de memoria CADA vez.
// No reutiliza valores anteriores, no reordena accesos.
```

### Cuándo usar volatile

```c
// 1. Registros de hardware mapeados en memoria:
volatile uint32_t *gpio_data = (volatile uint32_t *)0x40021014;
*gpio_data = 0xFF;          // escribir al hardware
uint32_t status = *gpio_data; // leer del hardware

// 2. Variables modificadas por interrupciones (embedded):
volatile int interrupt_flag = 0;

// En el handler de interrupción:
void ISR_handler(void) {
    interrupt_flag = 1;    // modificada fuera del flujo normal
}

// En el loop principal:
while (!interrupt_flag) {
    // Sin volatile, el compilador podría optimizar a:
    // while (1) { } — porque "interrupt_flag nunca cambia"
}

// 3. Variables compartidas entre threads (PERO):
// volatile NO es suficiente para threads.
// No garantiza atomicidad ni ordering.
// Usar _Atomic o mutexes para threads.
```

### volatile NO es para threads

```c
// INCORRECTO para threads:
volatile int shared = 0;

// Thread 1:
shared = 42;

// Thread 2:
while (shared == 0) { }   // esperar
printf("%d\n", shared);

// volatile asegura que se lee de memoria, pero NO garantiza:
// - Atomicidad (la escritura podría ser parcial)
// - Orden de memoria (otro thread podría ver un estado intermedio)
// - Visibilidad entre cores (caches de CPU)

// CORRECTO para threads:
_Atomic int shared = 0;    // o usar mutex
```

### volatile con const

```c
// volatile y const pueden combinarse:
const volatile int *status_reg = (const volatile int *)0x40021000;

// const: el código no puede escribir en este registro
// volatile: el hardware puede cambiar el valor
// Cada lectura va a memoria, pero no puedes escribir

int val = *status_reg;     // OK: leer
// *status_reg = 42;       // ERROR: es const
```

## restrict (C99) — Promesa de no aliasing

`restrict` es una promesa al compilador de que el puntero es
el **único** acceso a esa memoria. Permite optimizaciones que
de otra forma no serían seguras:

```c
// Sin restrict:
void add_arrays(int *dst, const int *src, size_t n) {
    for (size_t i = 0; i < n; i++) {
        dst[i] += src[i];
    }
}
// El compilador NO puede vectorizar (SIMD) fácilmente.
// ¿Por qué? dst y src podrían apuntar a la misma memoria.
// Si dst[0] y src[3] son la misma dirección, modificar
// dst[0] cambiaría src[3] — el compilador debe ser conservador.

// Con restrict:
void add_arrays_fast(int *restrict dst, const int *restrict src, size_t n) {
    for (size_t i = 0; i < n; i++) {
        dst[i] += src[i];
    }
}
// El programador PROMETE que dst y src no se solapan.
// El compilador puede vectorizar, reordenar, etc.
// Si la promesa es falsa → UB.
```

### Dónde aparece restrict

```c
// La biblioteca estándar usa restrict:
void *memcpy(void *restrict dst, const void *restrict src, size_t n);
// dst y src NO deben solaparse — si se solapan, usar memmove()

void *memmove(void *dst, const void *src, size_t n);
// memmove SÍ maneja solapamiento — por eso no tiene restrict

int sprintf(char *restrict str, const char *restrict fmt, ...);
// str y fmt no deben solaparse
```

```c
// restrict solo aplica a punteros:
int *restrict p;           // OK
// restrict int x;         // ERROR: no es puntero

// restrict es una promesa sobre la memoria apuntada:
void foo(int *restrict a, int *restrict b) {
    // a y b apuntan a regiones que NO se solapan
    *a = 10;
    *b = 20;
    // El compilador puede reordenar estas escrituras
    // porque sabe que no interfieren
}
```

### restrict en la práctica

```c
// restrict es relevante para:
// 1. Funciones que operan sobre arrays grandes (SIMD, vectorización)
// 2. Funciones de copia de memoria
// 3. Código de alto rendimiento (procesamiento de señales, gráficos)

// Para código general, restrict rara vez es necesario.
// Pero entender su significado es importante porque aparece
// en la biblioteca estándar (memcpy, sprintf, etc.).
```

## Combinaciones

```c
// Los calificadores se pueden combinar:
const volatile int *status;           // solo lectura, puede cambiar (hardware)
int *restrict const p;                // puntero const, acceso exclusivo
const int *restrict src;              // datos const, acceso exclusivo
```

## Tabla resumen

| Calificador | Significado | Uso principal |
|---|---|---|
| const | No modificable por este código | Parámetros de función, constantes |
| volatile | Puede cambiar externamente | Hardware, interrupciones |
| restrict | Único puntero a esta memoria | Optimización, arrays grandes |

| Combinación puntero | Dato | Puntero |
|---|---|---|
| `const int *p` | No modificable | Reasignable |
| `int *const p` | Modificable | No reasignable |
| `const int *const p` | No modificable | No reasignable |

---

## Ejercicios

### Ejercicio 1 — const con punteros

```c
// ¿Cuáles de estas líneas dan error de compilación?

const int x = 10;
int y = 20;
const int *p1 = &x;
int *const p2 = &y;
const int *const p3 = &x;

*p1 = 30;       // ¿?
p1 = &y;        // ¿?
*p2 = 30;       // ¿?
p2 = &x;        // ¿?
*p3 = 30;       // ¿?
p3 = &y;        // ¿?
```

### Ejercicio 2 — const en funciones

```c
// Escribir una función find_max que reciba un array const
// y retorne el valor máximo.
// Verificar que la función no puede modificar el array.
// Firma: int find_max(const int *arr, size_t n);
```

### Ejercicio 3 — restrict

```c
// Escribir dos versiones de copy_array:
// 1. Sin restrict
// 2. Con restrict
// Compilar con -O2 -S y comparar el assembly generado.
// ¿Hay diferencia en las instrucciones?

void copy_v1(int *dst, const int *src, size_t n);
void copy_v2(int *restrict dst, const int *restrict src, size_t n);
```
