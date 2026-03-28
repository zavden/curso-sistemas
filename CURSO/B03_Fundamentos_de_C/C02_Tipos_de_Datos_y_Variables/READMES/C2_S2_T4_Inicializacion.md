# T04 — Inicialización

> **Sin erratas detectadas** en el material fuente.

---

## 1. Las reglas dependen del storage class

El valor inicial de una variable en C **no** es siempre cero. Depende de su lifetime:

| Storage class | Valor por defecto | Razón |
|---|---|---|
| `static` lifetime (globales, `static`) | **Cero** (int→0, float→0.0, puntero→NULL) | Viven en BSS; el kernel llena BSS con ceros al crear el proceso. Costo: cero |
| `automatic` lifetime (locales sin `static`) | **Indeterminado** (basura) | Viven en el stack, que se reutiliza. C prioriza velocidad → no inicializa |
| `malloc` | **Indeterminado** | El heap no se limpia |
| `calloc` | **Cero** (a nivel de bytes) | `calloc` llena con ceros explícitamente |

```c
int global;                    // 0 (static lifetime → BSS)
static int file_static;       // 0
void foo(void) {
    static int local_static;  // 0
}

void bar(void) {
    int x;                     // indeterminado (basura)
    int arr[100];              // 100 valores de basura
    int *p;                    // dirección de basura (NO es NULL)
    // Leer estas variables sin asignarles valor es UB
}
```

**Punto clave:** que una variable local muestre `0` en una ejecución es coincidencia (el stack estaba limpio en ese momento). El compilador **no garantiza** ningún valor. En programas más complejos aparecerá basura real.

El compilador advierte con `-Wuninitialized` (incluido en `-Wall`) cuando detecta lectura de locales no inicializadas. No advierte sobre globales ni `static` porque sabe que esas siempre son cero.

---

## 2. Inicialización explícita

### Tipos escalares

```c
int x = 42;
double d = 3.14;
char c = 'A';
int *p = NULL;
_Bool b = 1;              // o: bool b = true; (con stdbool.h)
```

### Arrays

```c
int arr[5] = {10, 20, 30, 40, 50};    // todos los elementos

int arr2[5] = {10, 20};               // parcial: {10, 20, 0, 0, 0}
// Regla: si hay inicializador, los elementos no mencionados → 0

int zeros[100] = {0};                 // todo a cero

int arr3[] = {10, 20, 30};            // tamaño deducido: 3 elementos
// sizeof(arr3) == 12  (3 × 4 bytes)

char str[] = "hello";
// Equivale a: char str[] = {'h', 'e', 'l', 'l', 'o', '\0'};
// sizeof(str) == 6  (incluye el '\0')

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

struct Point p1 = {10, 20, 30};   // posicional
struct Point p2 = {10};           // parcial: {10, 0, 0}
struct Point p3 = {0};            // todo a cero: {0, 0, 0}
```

**Regla general:** si hay un inicializador `= { ... }`, todo lo que no se especifique se llena con ceros. Esto aplica a arrays, structs, y arrays de structs.

---

## 3. Designated initializers (C99)

Permiten inicializar campos específicos **por nombre** (structs) o **por índice** (arrays).

### En structs

```c
// Inicializar por nombre — el orden no importa:
struct Point p = { .y = 20, .x = 10, .z = 30 };

// Los campos no mencionados → 0:
struct Point origin = { .z = 5 };   // {0, 0, 5}
```

Son especialmente útiles con structs grandes donde la inicialización posicional sería ilegible:

```c
struct Config {
    int port;
    int max_connections;
    int timeout_ms;
    int buffer_size;
    int debug_level;
};

struct Config cfg = {
    .port = 8080,
    .max_connections = 100,
    .timeout_ms = 5000,
    // buffer_size = 0 (implícito)
    // debug_level = 0 (implícito)
};

// Sin designated initializers:
// struct Config cfg = {8080, 100, 5000, 0, 0};
// ¿Qué es 100? ¿Qué es 5000? Ilegible.
```

### En arrays

```c
int arr[10] = { [3] = 42, [7] = 99 };
// arr = {0, 0, 0, 42, 0, 0, 0, 99, 0, 0}

// Combinado con posicional:
int arr2[5] = { [2] = 10, 20, 30 };
// arr2 = {0, 0, 10, 20, 30}
// Después de [2]=10, los siguientes son posicionales (índices 3, 4)
```

Patrón especialmente útil: **indexar arrays con `enum`**:

```c
enum Color { RED, GREEN, BLUE, COLOR_COUNT };
const char *color_name[COLOR_COUNT] = {
    [RED]   = "red",
    [GREEN] = "green",
    [BLUE]  = "blue",
};
```

Esto es auto-documentante: al leer `[RED] = "red"` es inmediatamente claro qué índice corresponde a qué valor. Si alguien reordena el `enum`, el array sigue siendo correcto.

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
```

> La sintaxis `.addr.city = "Madrid"` (dot-notation anidada) es una **extensión GNU**, no estándar C11. Usar la forma con llaves anidadas para portabilidad.

---

## 4. Compound literals (C99)

Un compound literal crea un **valor temporal sin nombre** con la sintaxis `(tipo){ inicializador }`:

```c
struct Point p = (struct Point){ .x = 10, .y = 20 };

// Pasar structs a funciones sin variable intermedia:
void draw(struct Point p);
draw((struct Point){ .x = 5, .y = 10 });

// Arrays temporales:
int *ptr = (int[]){10, 20, 30};
// ptr apunta a un array temporal de 3 ints
```

### Lifetime de compound literals

El lifetime depende del scope donde se crean:

```c
// Scope de archivo → static lifetime (vive toda la ejecución):
struct Point *global_p = &(struct Point){ .x = 0, .y = 0 };

// Scope de bloque → automatic lifetime (se destruye al salir del bloque):
void foo(void) {
    struct Point *p = &(struct Point){ .x = 1, .y = 2 };
    // NO retornar p — el compound literal se destruye al salir de foo
}
```

### Compound literal ≠ cast

```c
int x = (int)3.14;                              // Cast: convierte valor existente
struct Point p = (struct Point){ .x = 1, .y = 2 }; // Compound literal: CREA valor nuevo
```

La diferencia visual es la lista de inicialización `{ ... }` después del paréntesis. Un compound literal siempre tiene llaves.

### Compound literal como retorno

```c
struct Point midpoint(struct Point a, struct Point b) {
    return (struct Point){
        .x = (a.x + b.x) / 2,
        .y = (a.y + b.y) / 2
    };
}
```

Aquí el compound literal se copia al valor de retorno. Es seguro porque se copia antes de que termine el scope de la función.

---

## 5. Patrón `{0}` e inicialización parcial

### El patrón `{0}` — "todo a cero"

```c
int arr[100] = {0};           // todo a cero
char str[256] = {0};          // string vacío (todo NUL)
struct Point p = {0};         // todos los campos a 0
struct Config cfg = {0};      // todo a cero
```

Funciona porque: el `0` inicializa el primer elemento, y la regla de inicialización parcial llena el resto con ceros.

> **C23:** `int arr[100] = {};` (lista vacía) será válido. En C11/C17, se requiere al menos un elemento: `{0}`.

### Inicialización parcial

```c
int partial[8] = {10, 20, 30};
// partial = {10, 20, 30, 0, 0, 0, 0, 0}
// Solo 3 valores explícitos; los 5 restantes → 0

struct Sensor s = { .id = 7, .temperature = 22.5 };
// s.humidity = 0.0  (implícito)
// s.active   = 0    (implícito)
// s.name     = ""   (implícito — todo NUL)
```

### Arrays de structs

```c
struct Sensor sensors[3] = {
    { .id = 1, .temperature = 20.0, .name = "sala" },
    { .id = 2, .name = "cocina" },
    // El tercer elemento: todos los campos en 0
};
// sensors[2].id == 0, sensors[2].temperature == 0.0, etc.
```

El array tiene inicializador, así que el struct `sensors[2]` (no mencionado) se llena completamente con ceros.

### `memset` vs `{0}`

```c
// Para variables ya declaradas, memset puede poner a cero:
memset(arr, 0, sizeof(arr));
memset(&cfg, 0, sizeof(cfg));

// CUIDADO: memset opera sobre BYTES, no sobre tipos.
// Para int/char: todos los bits en 0 = valor 0 → OK
// Para IEEE 754 float/double: todos los bits en 0 = 0.0 → OK
// Para punteros: NULL suele ser todos los bits en 0 → OK en la práctica
// Pero el estándar no garantiza que NULL sea all-bits-zero.
```

Preferir `{0}` en la declaración cuando sea posible. `memset` es para cuando la variable ya existe y necesita reinicializarse.

---

## 6. Inicializadores constantes y definiciones tentativas

### Inicializadores de variables con static lifetime

Las variables con static lifetime (globales, `static`) requieren que su inicializador sea una **expresión constante**:

```c
int global = 42;              // OK: 42 es constante
int global2 = 40 + 2;         // OK: expresión constante
// int global3 = rand();      // ERROR: rand() no es constante
// static int s2 = global;    // ERROR: global no es constant expression

// Las variables locales (auto) aceptan cualquier expresión:
void foo(void) {
    int x = rand();            // OK
    int y = global + x;        // OK
}
```

Esto es porque las variables con static lifetime se inicializan **antes de que `main()` empiece**. El compilador necesita saber el valor en tiempo de compilación. Las variables locales se inicializan en runtime, cuando se ejecuta la función.

### Definiciones tentativas

```c
int x;            // definición tentativa (se inicializa a 0 si no hay otra)
int x = 42;       // definición con inicializador

// Si ambas aparecen en el mismo archivo, x = 42.
// La definición con inicializador "gana".

// Múltiples definiciones tentativas en el MISMO archivo:
int x;
int x;            // OK en C (permitido por el estándar, §6.9.2)
int x = 42;       // definición final

// Múltiples definiciones con inicializador en DIFERENTES archivos:
// file_a.c: int x = 10;
// file_b.c: int x = 20;
// ERROR de enlace: multiple definition of 'x'
```

---

## 7. Patrones y buenas prácticas

### Inicializar siempre

```c
int x = 0;           // siempre, incluso si vas a asignar después
int *p = NULL;        // nunca dejar un puntero sin inicializar
```

### Inicializar cerca del uso

```c
// Malo:
int result;
// ... 50 líneas de código ...
result = compute();   // fácil olvidar que no está inicializado

// Bueno:
// ... 50 líneas de código ...
int result = compute();   // declarar donde se usa (C99+)
```

### Structs con defaults via función

```c
struct Config default_config(void) {
    return (struct Config){
        .port = 8080,
        .max_connections = 100,
        .timeout_ms = 5000,
        .buffer_size = 4096,
    };
}

struct Config cfg = default_config();
cfg.port = 9090;   // sobreescribir solo lo necesario
```

### La diferencia crítica: locales con y sin `{0}`

```c
int arr1[100];          // Global: todo 0 (BSS)
int arr2[100] = {0};    // Global: todo 0 (explícito)

void foo(void) {
    int arr3[100];          // Local: BASURA (sin inicializador)
    int arr4[100] = {0};    // Local: todo 0 (zero-initialized)
}
// arr1 y arr2 dan el mismo resultado, pero arr2 explicita la intención
// arr3 y arr4 son MUY DIFERENTES: basura vs ceros
```

---

## 8. Tabla resumen

| Tipo de variable | Init por defecto | Requiere init constante | Segmento |
|---|---|---|---|
| Global | 0 | Sí | BSS (sin init) / Data (con init) |
| `static` (local o file) | 0 | Sí | BSS / Data |
| Local (`auto`) | Indeterminado | No | Stack |
| `malloc` | Indeterminado | N/A | Heap |
| `calloc` | 0 (bytes) | N/A | Heap |

---

## Ejercicios

### Ejercicio 1 — Predecir valores por defecto

```c
#include <stdio.h>

int a;
static float b;
int c[3];

int main(void) {
    int d;
    static char e;
    int f[4] = {1, 2};

    printf("a = %d\n", a);
    printf("b = %f\n", b);
    printf("c = %d %d %d\n", c[0], c[1], c[2]);
    // printf("d = %d\n", d);  // ¿Es seguro descomentar?
    printf("e = %d\n", e);
    printf("f = %d %d %d %d\n", f[0], f[1], f[2], f[3]);

    return 0;
}
```

**¿Qué imprime cada línea? ¿Qué pasa si descomentas la línea de `d`?**

<details>
<summary>Predicción</summary>

```
a = 0              // global → static lifetime → BSS → 0
b = 0.000000       // static → BSS → 0.0
c = 0 0 0          // global array → BSS → todos 0
e = 0              // static local → BSS → 0
f = 1 2 0 0        // inicialización parcial: {1, 2, 0, 0}
```

Descomentar `d`: es **UB** (comportamiento indefinido). `d` es local sin inicializar — su valor es indeterminado. GCC advertirá con `-Wuninitialized`. En la práctica puede mostrar 0 o basura, pero no es seguro.

</details>

---

### Ejercicio 2 — Designated initializers en struct

```c
#include <stdio.h>

struct Server {
    char host[32];
    int port;
    int max_clients;
    int timeout;
    int ssl_enabled;
};

int main(void) {
    struct Server srv = {
        .port = 443,
        .ssl_enabled = 1,
        .host = "api.example.com",
    };

    printf("host:        %s\n", srv.host);
    printf("port:        %d\n", srv.port);
    printf("max_clients: %d\n", srv.max_clients);
    printf("timeout:     %d\n", srv.timeout);
    printf("ssl_enabled: %d\n", srv.ssl_enabled);

    return 0;
}
```

**¿Qué valores tendrán `max_clients` y `timeout`?**

<details>
<summary>Predicción</summary>

```
host:        api.example.com
port:        443
max_clients: 0        // no mencionado en designated init → 0
timeout:     0        // no mencionado → 0
ssl_enabled: 1
```

Los campos no mencionados en un designated initializer se inicializan a cero. El orden de los designadores (`.port` antes que `.host`) no importa — cada campo se asigna por nombre.

</details>

---

### Ejercicio 3 — Designated initializers en arrays

```c
#include <stdio.h>

int main(void) {
    int table[8] = { [1] = 10, [4] = 40, 50, [7] = 70 };

    printf("table: ");
    for (int i = 0; i < 8; i++) {
        printf("%d ", table[i]);
    }
    printf("\n");

    return 0;
}
```

**¿Cuáles son los 8 valores del array? Atención al `50` después de `[4] = 40`.**

<details>
<summary>Predicción</summary>

```
table: 0 10 0 0 40 50 0 70
```

- `[0]` = 0 (no mencionado)
- `[1]` = 10 (designado)
- `[2]` = 0 (no mencionado)
- `[3]` = 0 (no mencionado)
- `[4]` = 40 (designado)
- `[5]` = 50 (posicional — sigue después de `[4]`)
- `[6]` = 0 (no mencionado)
- `[7]` = 70 (designado)

Después de un designador `[4] = 40`, los valores posicionales que siguen continúan desde el índice 5 en adelante.

</details>

---

### Ejercicio 4 — Compound literals como argumentos

```c
#include <stdio.h>

struct Rect {
    int x, y, w, h;
};

void print_rect(struct Rect r) {
    printf("Rect(%d, %d, %d, %d)\n", r.x, r.y, r.w, r.h);
}

int main(void) {
    print_rect((struct Rect){ .x = 0, .y = 0, .w = 100, .h = 50 });
    print_rect((struct Rect){ .w = 200, .h = 100 });

    return 0;
}
```

**¿Qué imprime la segunda llamada? ¿Qué valores tienen `.x` y `.y`?**

<details>
<summary>Predicción</summary>

```
Rect(0, 0, 100, 50)
Rect(0, 0, 200, 100)
```

La segunda llamada: `.x` y `.y` no se mencionan en el compound literal → se inicializan a 0. Un compound literal sigue las mismas reglas que un inicializador normal: los campos omitidos son cero.

</details>

---

### Ejercicio 5 — `{0}` con distintos tipos

```c
#include <stdio.h>

struct Mixed {
    int id;
    double value;
    char label[10];
    int *ptr;
};

int main(void) {
    struct Mixed m = {0};

    printf("id:    %d\n", m.id);
    printf("value: %f\n", m.value);
    printf("label: \"%s\"\n", m.label);
    printf("ptr:   %p\n", (void *)m.ptr);

    int matrix[3][3] = {0};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            printf("%d ", matrix[i][j]);
        }
        printf("\n");
    }

    return 0;
}
```

**¿Qué valor tiene cada campo del struct? ¿Y la matriz?**

<details>
<summary>Predicción</summary>

```
id:    0
value: 0.000000
label: ""             (todo NUL)
ptr:   (nil)          (NULL)
0 0 0
0 0 0
0 0 0
```

`{0}` inicializa el primer campo (`id`) a 0, y la regla de inicialización parcial llena el resto con ceros. Cada tipo recibe su "cero" apropiado: `int` → 0, `double` → 0.0, `char[]` → todos NUL, puntero → NULL. La matriz 2D también queda toda en ceros — `{0}` funciona con arrays multidimensionales.

</details>

---

### Ejercicio 6 — Inicialización parcial de array de structs

```c
#include <stdio.h>

struct Student {
    char name[20];
    int grade;
    int passed;
};

int main(void) {
    struct Student class[4] = {
        { .name = "Ana", .grade = 85, .passed = 1 },
        { .name = "Bob", .grade = 60 },
    };

    for (int i = 0; i < 4; i++) {
        printf("[%d] name=\"%s\" grade=%d passed=%d\n",
               i, class[i].name, class[i].grade, class[i].passed);
    }

    return 0;
}
```

**¿Qué valores tienen `class[1].passed`, `class[2]` y `class[3]`?**

<details>
<summary>Predicción</summary>

```
[0] name="Ana" grade=85 passed=1
[1] name="Bob" grade=60 passed=0
[2] name="" grade=0 passed=0
[3] name="" grade=0 passed=0
```

- `class[1].passed`: no mencionado en su designated init → 0
- `class[2]` y `class[3]`: no tienen inicializador explícito, pero como el array tiene inicializador parcial, todos los campos no mencionados → 0. `name` queda como string vacío (todo NUL), `grade` y `passed` en 0.

</details>

---

### Ejercicio 7 — Error: inicializador no constante en static

```c
#include <stdio.h>
#include <stdlib.h>

int global_x = 10;
// static int global_y = global_x;         // Línea A
// static int global_z = rand();           // Línea B

int main(void) {
    int local_a = global_x;                // Línea C
    int local_b = rand();                  // Línea D
    // static int local_c = global_x;      // Línea E

    printf("local_a = %d\n", local_a);
    printf("local_b = %d\n", local_b);

    return 0;
}
```

**¿Cuáles líneas (A-E) darían error de compilación si se descomentan? ¿Por qué?**

<details>
<summary>Predicción</summary>

- **Línea A:** Error. `global_y` tiene static lifetime → requiere constant expression. `global_x` es una variable, no una constante.
- **Línea B:** Error. `rand()` no es una constant expression — es una llamada a función evaluada en runtime.
- **Línea C:** OK. `local_a` es auto → acepta cualquier expresión.
- **Línea D:** OK. `local_b` es auto → `rand()` es válido.
- **Línea E:** Error. `local_c` es `static` → tiene static lifetime → requiere constant expression. Que sea local no cambia nada: `static` implica inicialización antes de `main()`.

La regla: **static lifetime = inicializador constante**. No importa si la variable es global o local con `static`.

</details>

---

### Ejercicio 8 — Compound literal: lifetime y punteros

```c
#include <stdio.h>

struct Point {
    int x;
    int y;
};

struct Point *bad_function(void) {
    struct Point *p = &(struct Point){ .x = 99, .y = 88 };
    return p;   // ¿Es seguro?
}

struct Point good_function(void) {
    return (struct Point){ .x = 99, .y = 88 };   // ¿Es seguro?
}

int main(void) {
    // struct Point *ptr = bad_function();
    // printf("ptr: (%d, %d)\n", ptr->x, ptr->y);

    struct Point p = good_function();
    printf("p: (%d, %d)\n", p.x, p.y);

    return 0;
}
```

**¿Por qué `bad_function` es peligrosa y `good_function` es segura? ¿Qué pasaría al descomentar las líneas de `main`?**

<details>
<summary>Predicción</summary>

`bad_function` retorna un puntero a un compound literal con automatic lifetime. El compound literal se destruye al salir de la función → el puntero queda **dangling**. Desreferenciarlo es UB. Es exactamente el mismo problema que retornar `&local_var`.

`good_function` retorna el compound literal **por valor** (copia). La copia se hace antes de que el compound literal se destruya → es seguro. Imprime:

```
p: (99, 88)
```

Si se descomentan las líneas de `bad_function`: comportamiento indefinido. Puede funcionar "por suerte" o dar basura, dependiendo de si el stack se ha reutilizado.

</details>

---

### Ejercicio 9 — Definiciones tentativas

```c
#include <stdio.h>

int x;
int x;
int x = 42;

int y;
int y;

int main(void) {
    printf("x = %d\n", x);
    printf("y = %d\n", y);

    return 0;
}
```

**¿Compila? ¿Qué valores tienen `x` e `y`?**

<details>
<summary>Predicción</summary>

Sí compila. Salida:

```
x = 42
y = 0
```

- `x`: tiene dos definiciones tentativas (`int x;`) y una definición con inicializador (`int x = 42;`). La definición con inicializador gana → `x = 42`.
- `y`: tiene dos definiciones tentativas y ninguna definición con inicializador. Las tentativas se colapsan en una sola definición con valor 0 (C11 §6.9.2). → `y = 0`.

Múltiples definiciones tentativas en el mismo archivo son válidas en C. Pero si hubiera dos definiciones **con inicializador** (`int x = 42; int x = 99;`), sería un error de compilación: redefinición.

</details>

---

### Ejercicio 10 — Mezcla de patrones

```c
#include <stdio.h>
#include <string.h>

struct Config {
    char name[32];
    int values[4];
    struct {
        int enabled;
        int priority;
    } feature;
};

int main(void) {
    // Patrón A: designated + parcial
    struct Config a = {
        .name = "alpha",
        .values = { [0] = 1, [3] = 4 },
        .feature = { .priority = 5 },
    };

    // Patrón B: {0} completo
    struct Config b = {0};
    strcpy(b.name, "beta");
    b.values[0] = 10;

    printf("a.name = %s\n", a.name);
    printf("a.values = %d %d %d %d\n",
           a.values[0], a.values[1], a.values[2], a.values[3]);
    printf("a.feature.enabled = %d\n", a.feature.enabled);
    printf("a.feature.priority = %d\n", a.feature.priority);

    printf("\nb.name = %s\n", b.name);
    printf("b.values = %d %d %d %d\n",
           b.values[0], b.values[1], b.values[2], b.values[3]);
    printf("b.feature.enabled = %d\n", b.feature.enabled);

    return 0;
}
```

**¿Qué valores tienen `a.values[1]`, `a.values[2]`, `a.feature.enabled`, y todos los campos de `b` antes de las asignaciones?**

<details>
<summary>Predicción</summary>

```
a.name = alpha
a.values = 1 0 0 4
a.feature.enabled = 0
a.feature.priority = 5

b.name = beta
b.values = 10 0 0 0
b.feature.enabled = 0
```

Para `a`:
- `a.values[1]` y `a.values[2]`: no mencionados en `{ [0] = 1, [3] = 4 }` → 0
- `a.feature.enabled`: no mencionado en `{ .priority = 5 }` → 0

Para `b`:
- `{0}` inicializa **todo** a cero. Luego `strcpy` y la asignación modifican solo `name` y `values[0]`. El resto permanece en 0.

Los dos patrones (designated init vs `{0}` + asignación posterior) son igualmente válidos. El patrón A es más conciso cuando conoces los valores en la declaración. El patrón B es útil cuando los valores se calculan o se leen en runtime.

</details>
