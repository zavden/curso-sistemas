# T04 — Inicialización

## Las reglas dependen del storage class

El valor inicial de una variable en C **no** es siempre cero.
Depende de dónde vive la variable:

```c
// Variables con STATIC lifetime (globales, static):
// → Se inicializan a CERO automáticamente

int global;                    // 0
static int file_static;       // 0
void foo(void) {
    static int local_static;  // 0
}
// Esto incluye: int → 0, float → 0.0, puntero → NULL

// Variables con AUTOMATIC lifetime (locales sin static):
// → NO se inicializan. Valor INDETERMINADO (basura).

void bar(void) {
    int x;                     // indeterminado (basura)
    int arr[100];              // 100 valores de basura
    int *p;                    // dirección de basura (NO es NULL)

    // Leer estas variables sin asignarles valor es UB.
}
```

### Por qué esta diferencia

```c
// Las variables static están en los segmentos DATA/BSS.
// El sistema operativo llena BSS con ceros al cargar el programa.
// Costo: cero (el kernel ya lo hace al crear el proceso).

// Las variables locales están en el STACK.
// El stack se reutiliza constantemente (llamadas a función).
// Inicializar todo a cero tendría un costo en runtime.
// C prioriza velocidad → no inicializa locales.
```

## Inicialización explícita

### Tipos escalares

```c
int x = 42;
double d = 3.14;
char c = 'A';
int *p = NULL;
_Bool b = 1;           // o: bool b = true; (con stdbool.h)
```

### Arrays

```c
// Array con todos los elementos:
int arr[5] = {10, 20, 30, 40, 50};

// Menos elementos que el tamaño — el resto se inicializa a 0:
int arr2[5] = {10, 20};
// arr2 = {10, 20, 0, 0, 0}

// Truco para inicializar todo a 0:
int zeros[100] = {0};
// El primer elemento es 0, el resto también (regla del estándar)

// Array sin tamaño — el compilador lo deduce:
int arr3[] = {10, 20, 30};
// sizeof(arr3) == 12 (3 elementos × 4 bytes)

// Strings:
char str[] = "hello";
// Equivalente a: char str[] = {'h', 'e', 'l', 'l', 'o', '\0'};
// sizeof(str) == 6 (incluye el '\0')

char str2[10] = "hi";
// str2 = {'h', 'i', '\0', 0, 0, 0, 0, 0, 0, 0}
```

### Structs

```c
struct Point {
    int x;
    int y;
    int z;
};

// Inicialización posicional:
struct Point p1 = {10, 20, 30};

// Inicialización parcial — el resto es 0:
struct Point p2 = {10};
// p2 = {10, 0, 0}

// Inicializar todo a 0:
struct Point p3 = {0};
// p3 = {0, 0, 0}
```

## Designated initializers (C99)

Permiten inicializar campos específicos por nombre (structs)
o por índice (arrays):

### En structs

```c
struct Point {
    int x;
    int y;
    int z;
};

// Inicializar por nombre — el orden no importa:
struct Point p = { .y = 20, .x = 10, .z = 30 };

// Los campos no mencionados se inicializan a 0:
struct Point origin = { .z = 5 };
// origin = {0, 0, 5}

// Mezclar posicional y designado (no recomendado):
struct Point p2 = { .x = 10, 20, 30 };
// x = 10, y = 20, z = 30 (los no-designados siguen en orden)
```

```c
// Muy útil con structs grandes:
struct Config {
    int port;
    int max_connections;
    int timeout_ms;
    int buffer_size;
    int debug_level;
    int log_to_file;
};

struct Config cfg = {
    .port = 8080,
    .max_connections = 100,
    .timeout_ms = 5000,
    .buffer_size = 4096,
    // debug_level = 0 (implícito)
    // log_to_file = 0 (implícito)
};

// Sin designated initializers tendrías que recordar el orden
// y poner todos los valores:
// struct Config cfg = {8080, 100, 5000, 4096, 0, 0};
// ¿Qué es 100? ¿Qué es 5000? Ilegible.
```

### En arrays

```c
// Inicializar por índice:
int arr[10] = { [3] = 42, [7] = 99 };
// arr = {0, 0, 0, 42, 0, 0, 0, 99, 0, 0}

// Combinado con posicional:
int arr2[5] = { [2] = 10, 20, 30 };
// arr2 = {0, 0, 10, 20, 30}
// Después de [2]=10, los siguientes son posicionales (índices 3, 4)

// Útil para tablas de lookup:
const char *day_name[] = {
    [0] = "Sunday",
    [1] = "Monday",
    [2] = "Tuesday",
    [3] = "Wednesday",
    [4] = "Thursday",
    [5] = "Friday",
    [6] = "Saturday",
};

// Útil para mapear valores de enum:
enum Color { RED, GREEN, BLUE, COLOR_COUNT };
const char *color_name[COLOR_COUNT] = {
    [RED]   = "red",
    [GREEN] = "green",
    [BLUE]  = "blue",
};
```

### En structs anidados

```c
struct Address {
    char city[50];
    int zip;
};

struct Person {
    char name[50];
    int age;
    struct Address addr;
};

struct Person p = {
    .name = "Alice",
    .age = 30,
    .addr = {
        .city = "Madrid",
        .zip = 28001,
    },
};

// O con dot-notation anidada (extensión GNU, no estándar C11):
// struct Person p = {
//     .name = "Alice",
//     .addr.city = "Madrid",  // extensión GNU
// };
```

## Compound literals (C99)

Un compound literal crea un valor temporal sin nombre:

```c
struct Point {
    int x;
    int y;
};

// Crear un struct temporal:
struct Point p = (struct Point){ .x = 10, .y = 20 };

// Útil para pasar structs a funciones sin variable intermedia:
void draw(struct Point p);
draw((struct Point){ .x = 5, .y = 10 });

// Arrays temporales:
int *ptr = (int[]){10, 20, 30};
// ptr apunta a un array temporal de 3 ints
// El lifetime es el del bloque (como una variable local)
```

```c
// Compound literals en scope de archivo tienen static lifetime:
struct Point *global_p = &(struct Point){ .x = 0, .y = 0 };
// El compound literal vive toda la ejecución

// Compound literals en scope de bloque tienen automatic lifetime:
void foo(void) {
    struct Point *p = &(struct Point){ .x = 1, .y = 2 };
    // El compound literal se destruye al salir de foo
    // → no retornar p
}
```

## Initializer comunes para "todo a cero"

```c
// Scalares:
int x = 0;
double d = 0.0;
char *p = NULL;

// Arrays:
int arr[100] = {0};           // todo a cero
char str[256] = {0};          // string vacío (todo NUL)
// NOTA: int arr[100] = {}; es válido en C23, no en C11

// Structs:
struct Point p = {0};         // todos los campos a 0
struct Config cfg = {0};      // todo a cero

// Con memset (para arrays/structs ya declarados):
memset(arr, 0, sizeof(arr));
memset(&cfg, 0, sizeof(cfg));
// Funciona para ceros, pero no para otros valores.
// memset opera sobre bytes, no sobre tipos.

// CUIDADO con memset y floats/punteros:
// memset a 0 pone todos los bits en 0.
// Para IEEE 754, 0.0 tiene todos los bits en 0 → OK.
// Para punteros, NULL suele ser todos los bits en 0 → OK en la práctica.
// Pero el estándar no lo garantiza.
```

## Variables globales vs inicialización

```c
// Definición tentativa:
int x;            // definición tentativa (se inicializa a 0)
int x = 42;       // definición con inicializador

// Si ambas aparecen en el mismo archivo, x = 42.
// La definición con inicializador "gana".

// Múltiples definiciones tentativas en el MISMO archivo:
int x;
int x;            // OK en C (extensión común, aceptada por el estándar)
int x = 42;       // definición final

// Múltiples definiciones en DIFERENTES archivos:
// file_a.c: int x = 10;
// file_b.c: int x = 20;
// ERROR de enlace: multiple definition of 'x'
```

## Inicializadores constantes

```c
// Las variables con static lifetime requieren que su
// inicializador sea una CONSTANT EXPRESSION:

int global = 42;                // OK: 42 es constante
int global2 = 40 + 2;           // OK: expresión constante
// int global3 = rand();        // ERROR: rand() no es constante

static int s = 10;              // OK: constante
// static int s2 = global;      // ERROR: global no es constant expression

// Las variables locales (auto) pueden inicializarse con
// cualquier expresión:
void foo(void) {
    int x = rand();              // OK: locales aceptan cualquier expresión
    int y = global + x;          // OK
}
```

## Patrones de inicialización

```c
// 1. Inicializar siempre:
int x = 0;                      // siempre, incluso si vas a asignar después
int *p = NULL;

// 2. Inicializar cerca del uso:
// No:
int result;
// ... 50 líneas de código ...
result = compute();              // fácil olvidar que no está inicializado

// Sí:
// ... 50 líneas de código ...
int result = compute();          // declarar donde se usa

// 3. Structs con defaults:
struct Config default_config(void) {
    return (struct Config){
        .port = 8080,
        .max_connections = 100,
        .timeout_ms = 5000,
        .buffer_size = 4096,
    };
}

struct Config cfg = default_config();
cfg.port = 9090;                 // sobreescribir solo lo necesario
```

## Tabla resumen

| Tipo de variable | Init por defecto | Requiere init constante |
|---|---|---|
| Global | 0 | Sí |
| static (local o file) | 0 | Sí |
| Local (auto) | Indeterminado | No |
| malloc | Indeterminado | N/A |
| calloc | 0 (bytes) | N/A |

---

## Ejercicios

### Ejercicio 1 — Valores por defecto

```c
// ¿Qué imprime este programa? Predecir antes de ejecutar.
// Compilar con -Wall -Wextra.

#include <stdio.h>

int global_int;
static double static_double;

int main(void) {
    int local_int;
    static int static_local;
    int arr[3];
    int zeros[3] = {0};

    printf("global_int:    %d\n", global_int);
    printf("static_double: %f\n", static_double);
    // printf("local_int:     %d\n", local_int);  // ¿Es seguro?
    printf("static_local:  %d\n", static_local);
    printf("zeros:         %d %d %d\n", zeros[0], zeros[1], zeros[2]);
    return 0;
}
```

### Ejercicio 2 — Designated initializers

```c
// Usar designated initializers para inicializar:
// 1. Un struct con al menos 5 campos, inicializando solo 3
// 2. Un array de 10 ints con valores solo en índices 0, 5, 9
// 3. Un array de strings indexado por un enum
```

### Ejercicio 3 — Compound literals

```c
// Escribir una función draw_line(struct Point from, struct Point to).
// Llamarla usando compound literals (sin crear variables intermedias).
// Verificar que funciona.
```
