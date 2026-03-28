# T01 — Clases de almacenamiento

## Qué es una storage class

La storage class de una variable determina **dónde** se almacena
en memoria, **cuánto tiempo** vive y **quién puede verla**:

```c
// C tiene 5 storage class specifiers:
auto           // variable local (default, casi nunca se escribe)
static         // persiste entre llamadas / limita visibilidad
extern         // declaración de variable definida en otro archivo
register       // sugerencia para poner en registro (obsoleto)
_Thread_local  // una copia por thread (C11)
```

## auto — Variables locales

`auto` es la storage class por defecto de las variables locales.
**Nunca se escribe** porque es redundante:

```c
void foo(void) {
    auto int x = 42;    // auto es el default — no se escribe
    int x = 42;          // exactamente lo mismo
}
```

```c
// Características de auto:
// - Se almacena en el STACK
// - Se crea al entrar al bloque donde fue declarada
// - Se destruye al salir del bloque
// - NO se inicializa automáticamente (valor indeterminado)
// - Solo visible dentro del bloque

void example(void) {
    int x = 10;          // se crea aquí

    if (x > 5) {
        int y = 20;      // se crea aquí
        // x y y son visibles
    }
    // y ya no existe (fue destruida al salir del if)
    // x sigue viva
}
// x se destruye aquí
```

```c
// NOTA sobre C23: auto cambia de significado.
// En C23, auto deduce el tipo (como en C++):
// auto x = 42;    // x es int (C23)
// En C11, auto es solo un storage class specifier (nunca se usa).
```

## static — Dos significados

`static` tiene dos significados completamente diferentes según
el contexto:

### 1. static en variable local — persistencia

```c
#include <stdio.h>

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

```c
// Sin static:
void counter_bad(void) {
    int count = 0;        // se reinicializa CADA llamada
    count++;
    printf("count = %d\n", count);
}
// Siempre imprime 1

// Con static:
// - Se almacena en el segmento DATA (no en el stack)
// - Vive durante toda la ejecución del programa
// - Se inicializa UNA vez (antes de main)
// - Si no se inicializa, se inicializa a 0 (no a basura)
// - Solo es visible dentro de la función (scope no cambia)
```

```c
// Usos comunes de static local:
// 1. Contadores:
int get_id(void) {
    static int next_id = 0;
    return next_id++;
}

// 2. Buffers que deben persistir:
const char *error_message(int code) {
    static char buffer[256];
    snprintf(buffer, sizeof(buffer), "Error %d", code);
    return buffer;    // válido porque buffer es static
    // CUIDADO: no es thread-safe, y la siguiente llamada
    // sobreescribe el contenido
}

// 3. Inicialización costosa que solo se hace una vez:
const int *get_lookup_table(void) {
    static int table[256];
    static int initialized = 0;
    if (!initialized) {
        for (int i = 0; i < 256; i++) {
            table[i] = compute(i);  // solo la primera vez
        }
        initialized = 1;
    }
    return table;
}
```

### 2. static en scope de archivo — linkage interno

```c
// math_utils.c

// static en una función o variable GLOBAL la hace invisible
// fuera de este archivo:

static int helper(int x) {      // solo visible en math_utils.c
    return x * 2;
}

int public_add(int a, int b) {   // visible en otros archivos
    return helper(a) + helper(b);
}

static int internal_counter = 0; // solo visible en math_utils.c
```

```c
// main.c
extern int public_add(int a, int b);  // OK — puede verla
extern int helper(int x);             // ERROR de enlace — helper es static

// static en scope de archivo = "privado a este archivo"
// Es la forma de C de tener encapsulación.
```

```c
// static a nivel de archivo:
// - Limita el linkage a "interno" (solo este .c)
// - Evita conflictos de nombres entre archivos
// - Permite que el compilador optimice más agresivamente
//   (sabe que nadie más la usa → puede inline, eliminar, etc.)
```

## extern — Declaración sin definición

`extern` dice "esta variable existe, pero está definida en
otro archivo":

```c
// config.c — DEFINICIÓN:
int debug_level = 0;              // aquí se reserva la memoria
int max_connections = 100;

// main.c — DECLARACIÓN:
extern int debug_level;            // no reserva memoria
extern int max_connections;        // solo dice "existe en algún lado"

void foo(void) {
    if (debug_level > 0) {
        printf("debug mode\n");
    }
}
```

```c
// La diferencia entre declaración y definición:

int x = 42;           // DEFINICIÓN: reserva memoria, asigna valor
int x;                // DEFINICIÓN tentativa: reserva memoria, se inicializa a 0
extern int x;         // DECLARACIÓN: no reserva memoria, solo anuncia

// Puede haber múltiples declaraciones pero solo UNA definición.
// Si hay más de una definición → error de enlace (multiple definition).
```

### El patrón header/source

```c
// config.h — declaraciones (lo que otros archivos ven):
#ifndef CONFIG_H
#define CONFIG_H

extern int debug_level;
extern int max_connections;

void config_init(void);

#endif

// config.c — definiciones:
#include "config.h"

int debug_level = 0;           // definición
int max_connections = 100;     // definición

void config_init(void) {
    // leer configuración...
}

// main.c — uso:
#include "config.h"

int main(void) {
    config_init();
    printf("debug: %d\n", debug_level);
    return 0;
}
```

```c
// Regla:
// - En el .h: extern int variable;  (declaración)
// - En el .c: int variable = valor; (definición)
// - Otros .c incluyen el .h y pueden usar la variable

// Sin extern en el .h:
// int variable;  ← esto es una definición tentativa
// Si dos .c incluyen este .h → multiple definition error
```

### extern con funciones

```c
// Las funciones son extern por defecto.
// Estos dos son equivalentes:
int add(int a, int b);           // extern implícito
extern int add(int a, int b);   // extern explícito (redundante)

// Por eso los prototipos en headers no llevan extern:
// Solo se usa extern explícito para variables globales.
```

## register — Sugerencia al compilador (obsoleto)

```c
register int i;    // "sugiere" al compilador poner i en un registro

// En la práctica:
// - Los compiladores modernos ignoran register
// - El compilador decide mejor que el programador qué poner en registros
// - La única restricción real: no puedes tomar la dirección (&i es error)
// - C23 deprecated register como storage class

// No usar register en código moderno.
```

## _Thread_local (C11)

`_Thread_local` crea una copia independiente de la variable
para cada thread:

```c
#include <threads.h>
#include <stdio.h>

_Thread_local int counter = 0;   // cada thread tiene su propia copia

int worker(void *arg) {
    int id = *(int *)arg;
    for (int i = 0; i < 5; i++) {
        counter++;               // modifica SU copia, no la de otros threads
    }
    printf("Thread %d: counter = %d\n", id, counter);
    return 0;
}

int main(void) {
    thrd_t t1, t2;
    int id1 = 1, id2 = 2;

    thrd_create(&t1, worker, &id1);
    thrd_create(&t2, worker, &id2);
    thrd_join(t1, NULL);
    thrd_join(t2, NULL);

    // Thread 1: counter = 5
    // Thread 2: counter = 5
    // Cada uno tiene su propia copia

    return 0;
}
```

```c
// _Thread_local:
// - Cada thread tiene su propia instancia de la variable
// - Se inicializa de nuevo para cada thread que se crea
// - No necesita mutex para acceder (es privada del thread)
// - Se almacena en TLS (Thread-Local Storage)

// Se puede combinar con static o extern:
static _Thread_local int private_counter = 0;  // thread-local + interno
extern _Thread_local int shared_counter;       // thread-local + externo

// GCC también soporta __thread (extensión):
__thread int counter = 0;    // equivalente a _Thread_local
```

## Dónde vive cada tipo de variable

```
Segmento de memoria     Qué vive ahí
─────────────────────   ──────────────────────────────────
Stack                   Variables locales (auto)
                        Parámetros de función
                        Dirección de retorno

Data (inicializado)     Variables globales inicializadas
                        Variables static inicializadas

BSS (sin inicializar)   Variables globales sin inicializar
                        Variables static sin inicializar
                        (inicializadas a 0 por el sistema)

Heap                    Memoria de malloc/calloc/realloc

Text (código)           Instrucciones del programa
                        Constantes string literals

TLS                     Variables _Thread_local
```

```c
int global_init = 42;          // Data
int global_no_init;            // BSS (se inicializa a 0)
static int file_scope = 10;   // Data

void foo(void) {
    int local = 5;             // Stack
    static int persist = 0;    // Data (o BSS si fuera = 0)
    int *heap = malloc(100);   // el puntero en Stack,
                               // los 100 bytes en Heap
    const char *s = "hello";   // puntero en Stack,
                               // "hello" en Text/rodata
}
```

## Tabla resumen

| Specifier | Almacenamiento | Lifetime | Scope | Init default |
|---|---|---|---|---|
| auto (default local) | Stack | Bloque | Bloque | Indeterminado |
| static (local) | Data/BSS | Programa | Bloque | 0 |
| static (file scope) | Data/BSS | Programa | Archivo | 0 |
| extern | Data/BSS | Programa | Múltiples archivos | 0 |
| register | Registro/Stack | Bloque | Bloque | Indeterminado |
| _Thread_local | TLS | Thread | Depende | 0 |

---

## Ejercicios

### Ejercicio 1 — static local

```c
// Implementar una función next_id() que retorne un ID
// diferente cada vez que se llama (1, 2, 3, ...).
// Usar static local.
// Llamar 5 veces y verificar los valores.
```

### Ejercicio 2 — extern

```c
// Crear dos archivos:
// - config.c: define una variable global "verbose" (int, default 0)
// - main.c: declara verbose con extern, la modifica y la imprime
// Compilar: gcc -std=c11 config.c main.c -o program
```

### Ejercicio 3 — static file scope

```c
// Crear helpers.c con dos funciones:
// - static int internal_helper(int x) — solo visible internamente
// - int public_compute(int x) — usa internal_helper, visible fuera
// Verificar que main.c NO puede llamar a internal_helper directamente.
```
