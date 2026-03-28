# T03 — typedef

> **Fuentes**: `README.md`, `LABS.md`, `labs/README.md`, `labs/basic_typedef.c`,
> `labs/define_vs_typedef.c`, `labs/func_pointers.c`, `labs/stack.h`,
> `labs/stack.c`, `labs/stack_main.c`

## Erratas detectadas

Sin erratas detectadas en el material fuente.

---

## Tabla de contenidos

1. [Qué es typedef](#1--qué-es-typedef)
2. [typedef con structs](#2--typedef-con-structs)
3. [typedef con enums](#3--typedef-con-enums)
4. [Convenciones de nombres](#4--convenciones-de-nombres)
5. [typedef para punteros a función](#5--typedef-para-punteros-a-función)
6. [Opaque types (tipos opacos)](#6--opaque-types-tipos-opacos)
7. [typedef para arrays](#7--typedef-para-arrays)
8. [typedef vs #define](#8--typedef-vs-define)
9. [Forward declarations con typedef](#9--forward-declarations-con-typedef)
10. [Comparación con Rust](#10--comparación-con-rust)

---

## 1 — Qué es typedef

`typedef` crea un **alias** (nombre alternativo) para un tipo existente. **No
crea un tipo nuevo** — el alias y el tipo original son completamente
intercambiables:

```c
// Sin typedef:
unsigned long long int counter = 0;

// Con typedef:
typedef unsigned long long int u64;
u64 counter = 0;

// u64 ES unsigned long long — mismo tipo, mismo sizeof, mismo comportamiento.
```

### Sintaxis

```c
// typedef tipo_existente nuevo_nombre;

typedef int Length;
typedef double Temperature;
typedef char *String;

Length width = 100;        // es int
Temperature temp = 36.5;  // es double
String name = "Alice";    // es char*
```

### Lo que typedef NO hace

- **No crea un tipo distinto**: `Length` y `int` son el mismo tipo. Se pueden
  mezclar sin warning ni cast.
- **No añade type safety**: una función que espera `Length` acepta cualquier `int`.
- **No cambia el sizeof**: `sizeof(u64) == sizeof(unsigned long long)` siempre.

Esto es una diferencia fundamental con Rust, donde `type Celsius = f64` crea
un alias débil pero `struct Celsius(f64)` crea un tipo nuevo (newtype pattern).

---

## 2 — typedef con structs

En C, declarar una variable de struct requiere escribir `struct` cada vez:

```c
struct Point { int x, y; };
struct Point p = {10, 20};   // "struct" obligatorio
```

Con `typedef` se elimina esa repetición:

```c
typedef struct Point {
    int x, y;
} Point;

Point p = {10, 20};          // sin "struct"
struct Point p2 = {30, 40};  // también funciona (tiene tag)
```

### Tres variantes

**1. Con tag Y typedef (recomendada):**

```c
typedef struct Point {
    int x, y;
} Point;
// Se puede usar como "Point" o "struct Point"
// Se puede hacer forward declaration: struct Point;
```

**2. Sin tag (struct anónimo):**

```c
typedef struct {
    float r, g, b;
} Color;
// Solo se puede usar como "Color"
// struct Color c; → ERROR (no hay tag)
// NO se puede hacer forward declaration
```

**3. Typedef separado:**

```c
struct Point { int x, y; };
typedef struct Point Point;   // en línea aparte
```

La variante 1 es la más flexible: permite ambas formas de acceso y soporta
forward declaration. La variante 2 es más concisa pero limita la
reutilización.

---

## 3 — typedef con enums

El mismo patrón aplica a enums:

```c
// Sin typedef:
enum Direction { NORTH, SOUTH, EAST, WEST };
enum Direction dir = NORTH;   // "enum" obligatorio

// Con typedef:
typedef enum {
    NORTH, SOUTH, EAST, WEST
} Direction;
Direction dir = NORTH;        // sin "enum"
```

En la práctica, `typedef` con enums es menos crítico que con structs porque
los valores del enum (`NORTH`, `SOUTH`) están en scope global de todos modos
— lo que se simplifica es solo la declaración de variables.

---

## 4 — Convenciones de nombres

### Kernel de Linux — sin typedef para structs

```c
struct point { int x, y; };
struct point p;   // siempre "struct point"
```

La filosofía de Linus Torvalds: `typedef` oculta que es un struct. Si
necesitas saber que es un struct (para pasarlo por puntero, hacer `malloc`,
etc.), la sintaxis debe recordártelo. El kernel de Linux reserva `typedef`
solo para tipos opacos y punteros a función.

### Windows/POSIX — con typedef

```c
typedef struct { int x, y; } POINT;
POINT p;
```

Windows usa mayúsculas para los typedef (`DWORD`, `HANDLE`, `LPSTR`). POSIX
usa el sufijo `_t` para tipos del sistema.

### El sufijo _t

```c
typedef unsigned int uint32_t;    // <stdint.h>
typedef long ssize_t;             // POSIX
typedef unsigned long size_t;     // <stdlib.h>
```

**POSIX reserva nombres que terminan en `_t`**. No crear tipos con `_t` en
código de usuario para evitar colisiones con futuras versiones del estándar.

### Recomendación para este curso

Se usan ambas formas según contexto. Para código de aplicación, typedef con
structs es conveniente. Para código que necesita claridad sobre la naturaleza
del tipo, se usa `struct Nombre` explícito.

---

## 5 — typedef para punteros a función

Este es el uso más valioso de `typedef` — transforma declaraciones crípticas
en código legible:

### Sin typedef (ilegible)

```c
// "get_operation es una función que toma char y retorna
//  un puntero a función que toma (int, int) y retorna int"
int (*get_operation(char op))(int, int);

// Array de punteros a función:
int (*ops[])(int, int) = {add, subtract, multiply};
```

### Con typedef (legible)

```c
typedef int (*BinaryOp)(int, int);

BinaryOp get_operation(char op);   // retorna un BinaryOp
BinaryOp ops[] = {add, subtract, multiply};   // array de BinaryOp
```

### Dispatch table

```c
typedef int (*BinaryOp)(int, int);

static int add(int a, int b)      { return a + b; }
static int subtract(int a, int b) { return a - b; }
static int multiply(int a, int b) { return a * b; }

BinaryOp get_operation(char op) {
    switch (op) {
        case '+': return add;
        case '-': return subtract;
        case '*': return multiply;
        default:  return NULL;
    }
}

// Uso:
BinaryOp fn = get_operation('+');
int result = fn(15, 4);   // 19
```

### Callbacks

```c
typedef void (*EventHandler)(int event, void *data);

void register_handler(int event_type, EventHandler handler, void *data);
// Sin typedef: void register_handler(int, void (*)(int, void*), void*);
```

### Arrays de punteros a función

```c
typedef void (*Command)(void);

Command menu_commands[10];
menu_commands[0] = cmd_new;
menu_commands[1] = cmd_open;
// ...
menu_commands[selection]();   // despachar por índice
```

---

## 6 — Opaque types (tipos opacos)

El patrón más poderoso de typedef en C: **ocultar la implementación** de un
tipo para lograr encapsulación.

### La interfaz pública (header)

```c
// --- stack.h ---
#ifndef STACK_H
#define STACK_H

#include <stdbool.h>

typedef struct Stack Stack;   // forward declaration con typedef

Stack *stack_create(int capacity);
void   stack_destroy(Stack *s);
bool   stack_push(Stack *s, int value);
bool   stack_pop(Stack *s, int *out);
bool   stack_peek(const Stack *s, int *out);
bool   stack_empty(const Stack *s);
int    stack_size(const Stack *s);

#endif
```

El usuario que incluye `stack.h` sabe que existe `Stack` pero **no conoce su
contenido**. Solo puede trabajar con punteros a `Stack` y llamar las funciones
públicas.

### La implementación privada (.c)

```c
// --- stack.c ---
#include "stack.h"
#include <stdlib.h>

struct Stack {
    int *data;
    int  top;
    int  capacity;
};

Stack *stack_create(int capacity) {
    Stack *s = malloc(sizeof(Stack));
    if (!s) return NULL;
    s->data = malloc(sizeof(int) * (size_t)capacity);
    if (!s->data) { free(s); return NULL; }
    s->top = -1;
    s->capacity = capacity;
    return s;
}

void stack_destroy(Stack *s) {
    if (!s) return;
    free(s->data);
    free(s);
}
// ... demás funciones ...
```

### Encapsulación verificada por el compilador

Si el usuario intenta acceder a un campo interno:

```c
// main.c
#include "stack.h"

Stack *s = stack_create(5);
s->data = NULL;   // ERROR de compilación
```

```
error: dereferencing pointer to incomplete type 'Stack' {aka 'struct Stack'}
```

`Stack` es un **incomplete type** fuera de `stack.c`. El compilador impide
cualquier acceso a campos internos.

### Ventajas de los opaque types

1. **Encapsulación**: el usuario no puede leer ni modificar campos internos.
2. **Estabilidad de API**: cambiar la estructura interna solo requiere
   recompilar `stack.c` — el código del usuario no se ve afectado.
3. **Compilación más rápida**: el header no incluye detalles internos, así
   que cambiar la implementación no invalida los `.o` del usuario.

### Ejemplos reales

- `FILE *` (`<stdio.h>`) — no sabes qué campos tiene `FILE`
- `DIR *` (`<dirent.h>`) — la estructura interna es del sistema operativo
- `sqlite3 *` — la biblioteca SQLite expone un tipo opaco

### Compilación multi-archivo

```bash
# Compilar cada .c por separado:
gcc -c stack.c -o stack.o
gcc -c stack_main.c -o stack_main.o

# Enlazar:
gcc stack.o stack_main.o -o stack_main

# O todo junto:
gcc stack.c stack_main.c -o stack_main
```

Con `nm stack.o` se ven los símbolos públicos (funciones). Los campos
internos (`data`, `top`, `capacity`) no aparecen como símbolos — son detalles
de implementación dentro de las funciones compiladas.

---

## 7 — typedef para arrays

`typedef` puede crear alias para tipos array:

```c
typedef int Vector3[3];
typedef char Name[50];

Vector3 position = {1, 2, 3};
Name player_name = "Alice";
```

### La trampa del array decay

Cuando un array typedef se pasa a una función, sigue aplicando el **decay a
puntero**:

```c
void foo(Vector3 v) {
    // v es int*, NO int[3]
    printf("sizeof(v) = %zu\n", sizeof(v));   // 8 (sizeof(int*))
    // No 12 (sizeof(int[3]))
}
```

El typedef **no cambia la semántica** del lenguaje. Los arrays siguen
decayendo a punteros al pasarse a funciones, sin importar cómo los nombres.
Por esta razón, `typedef` de arrays puede ser **engañoso** — sugiere que se
pasa el array completo cuando en realidad se pasa un puntero.

**Recomendación**: usar typedef de arrays con precaución. Es más claro ser
explícito con `int v[3]` o pasar puntero + tamaño.

---

## 8 — typedef vs #define

La diferencia es crítica y frecuentemente preguntada en entrevistas:

### El problema con #define

```c
typedef char *String_t;    // compilador entiende tipos
#define  String_d char *   // sustitución de texto

String_t t_a, t_b;   // ambas son char*
String_d d_a, d_b;   // se expande a: char *d_a, d_b;
                      // d_a es char*, d_b es char (¡diferente!)
```

Con `typedef`, el compilador sabe que `String_t` es un tipo completo (`char *`)
y aplica `*` a ambas variables. Con `#define`, el preprocesador hace
sustitución textual ciega: `char *d_a, d_b` — el `*` solo se adjunta a `d_a`.

### const también cambia significado

```c
typedef int *IntPtr;
const IntPtr p;      // p es int *const  (puntero constante a int)

#define IntPtr int *
const IntPtr p;      // se expande a: const int *p  (puntero a int constante)
// ¡Significados diferentes!
```

Con `typedef`, `const` se aplica al alias completo (`int *`), haciendo el
puntero constante. Con `#define`, `const` se aplica a `int`, haciendo el
contenido apuntado constante.

### Resumen

| Aspecto | `typedef` | `#define` |
|---------|-----------|-----------|
| Procesado por | Compilador | Preprocesador |
| Entiende tipos | Sí | No (texto) |
| Múltiples variables | Todas del mismo tipo | Solo la primera tiene `*` |
| `const` | Aplica al tipo completo | Aplica al tipo base |
| Scope | Respeta scope C | Global hasta `#undef` |
| Debugger | Visible | Invisible (desaparece) |

**Conclusión**: usar `typedef`, no `#define`, para alias de tipos. Siempre.

---

## 9 — Forward declarations con typedef

`typedef` + forward declaration permite usar un tipo antes de definirlo:

### Struct auto-referencial

```c
// Forward declaration con typedef:
typedef struct Node Node;

struct Node {
    int value;
    Node *next;    // usar el alias dentro del struct
};
```

Sin typedef, se necesita `struct Node *next` (con la palabra `struct`). Con
typedef, se puede escribir `Node *next` directamente.

### Structs mutuamente referenciados

```c
typedef struct A A;
typedef struct B B;

struct A {
    B *b;    // A conoce B
};

struct B {
    A *a;    // B conoce A
};
```

Sin los typedefs previos, se necesitarían forward declarations con `struct`:

```c
struct A;
struct B;
struct A { struct B *b; };
struct B { struct A *a; };
```

### Forward declaration requiere tag

```c
// CON tag: forward declaration posible
typedef struct Point Point;    // OK
struct Point { int x, y; };

// SIN tag: forward declaration IMPOSIBLE
typedef struct { int x; } Point;
// ¿struct ???;  — no hay tag para referenciar
```

Esta es la razón principal para preferir la variante con tag
(`typedef struct Point { ... } Point`) sobre la variante anónima
(`typedef struct { ... } Point`).

---

## 10 — Comparación con Rust

| Aspecto | C `typedef` | Rust `type` / newtype |
|---------|-------------|----------------------|
| Alias simple | `typedef int Length;` | `type Length = i32;` |
| ¿Tipo nuevo? | No — mismo tipo | `type`: no. `struct Length(i32)`: sí |
| Type safety | Ninguna | Newtype impide mezcla |
| Opaque type | `typedef struct X X;` (manual) | Campos privados por defecto |
| Function pointer | `typedef int (*F)(int);` | `type F = fn(i32) -> i32;` |
| Necesidad | Alta (struct/enum repetitivos) | Baja (no hay `struct` keyword obligatorio) |

### Alias en Rust

```rust
type Celsius = f64;
let temp: Celsius = 36.5;
let x: f64 = temp;   // OK — mismo tipo, se mezclan sin problema
```

### Newtype pattern (tipo nuevo real)

```rust
struct Celsius(f64);
struct Fahrenheit(f64);

let c = Celsius(36.5);
let f = Fahrenheit(98.6);
// let x: Celsius = f;  // ERROR — tipos incompatibles
```

Rust logra con el newtype pattern lo que C **no puede** con `typedef`: crear
tipos que el compilador distingue y no permite mezclar.

### Encapsulación en Rust

```rust
// Los campos de un struct son privados por defecto en Rust
pub struct Stack {
    data: Vec<i32>,   // privado — equivalente al opaque type
    top: usize,       // privado
}

impl Stack {
    pub fn new(capacity: usize) -> Self { /* ... */ }
    pub fn push(&mut self, value: i32) { /* ... */ }
    pub fn pop(&mut self) -> Option<i32> { /* ... */ }
}
```

En Rust no se necesita el patrón header/implementation para lograr
encapsulación — los campos son privados por defecto. En C, se requiere la
arquitectura de opaque type (typedef + forward declaration + compilación
separada).

### Function pointers en Rust

```rust
type BinaryOp = fn(i32, i32) -> i32;

fn add(a: i32, b: i32) -> i32 { a + b }

let op: BinaryOp = add;
let result = op(15, 4);   // 19
```

La sintaxis de Rust es más limpia que la de C, pero `typedef` en C cumple la
misma función de dar un nombre legible al tipo de puntero a función.

---

## Ejercicios

### Ejercicio 1 — Alias básicos

```c
// Crear los siguientes typedef:
//   typedef unsigned char      Byte;
//   typedef unsigned short     u16;
//   typedef unsigned int       u32;
//   typedef unsigned long long u64;
//
// Declarar una variable de cada tipo, asignar su valor máximo, e imprimir:
//   - El valor
//   - sizeof del alias
//   - sizeof del tipo original
//
// Verificar que sizeof(alias) == sizeof(tipo_original) para cada uno.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex01.c -o ex01
```

<details><summary>Predicción</summary>

```
Byte: 255           sizeof(Byte)=1  sizeof(unsigned char)=1
u16:  65535          sizeof(u16)=2   sizeof(unsigned short)=2
u32:  4294967295     sizeof(u32)=4   sizeof(unsigned int)=4
u64:  18446744073709551615  sizeof(u64)=8   sizeof(unsigned long long)=8
```

Todos los sizeof coinciden porque `typedef` no crea un tipo nuevo — es un
alias textual que el compilador resuelve al tipo original. Los valores
máximos son 2^N - 1 para cada tipo de N bits.

</details>

---

### Ejercicio 2 — typedef con struct (tag vs anónimo)

```c
// Declarar dos structs:
//
// 1. Con tag Y typedef:
//    typedef struct Vec2 { float x, y; } Vec2;
//
// 2. Sin tag (anónimo):
//    typedef struct { float r, g, b, a; } Color;
//
// En main:
//   - Declarar Vec2 v1 = {1.0, 2.0};
//   - Declarar struct Vec2 v2 = {3.0, 4.0};  (funciona porque tiene tag)
//   - Declarar Color c = {1.0, 0.0, 0.0, 1.0};
//   - Intentar struct Color c2 = {...};  (comentado — explicar por qué falla)
//
// Imprimir todos los valores.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex02.c -o ex02
```

<details><summary>Predicción</summary>

```
v1 (Vec2):        (1.0, 2.0)
v2 (struct Vec2): (3.0, 4.0)
c  (Color):       (1.0, 0.0, 0.0, 1.0)
```

`struct Vec2 v2` funciona porque `Vec2` tiene tag. `struct Color c2`
no compilaría porque `Color` es un struct anónimo — el tag no existe, así
que `struct Color` no es un tipo válido. Solo se puede usar como `Color`.

</details>

---

### Ejercicio 3 — typedef vs #define: la trampa

```c
// Definir:
//   typedef int *IntPtr_t;
//   #define  IntPtr_d int *
//
// En main:
//   int a = 10, b = 20;
//
//   IntPtr_t t1, t2;    // ¿qué tipo tienen t1 y t2?
//   t1 = &a; t2 = &b;
//
//   IntPtr_d d1, d2;    // ¿qué tipo tienen d1 y d2?
//   d1 = &a; d2 = ???;  // ¿qué se puede asignar a d2?
//
// Imprimir sizeof de cada variable.
// Explicar con un comentario la diferencia.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex03.c -o ex03
```

<details><summary>Predicción</summary>

```
sizeof(t1) = 8 (int*)
sizeof(t2) = 8 (int*)
sizeof(d1) = 8 (int*)
sizeof(d2) = 4 (int!)
```

`IntPtr_t t1, t2;` → ambas son `int*` (typedef entiende tipos).
`IntPtr_d d1, d2;` → se expande a `int *d1, d2;` → `d1` es `int*` pero
`d2` es `int`. A `d2` se le asigna un entero, no un puntero. Esta es la
diferencia fundamental: `typedef` opera a nivel de tipos, `#define` a nivel
de texto.

</details>

---

### Ejercicio 4 — Dispatch table con typedef

```c
// Definir:
//   typedef double (*MathFunc)(double);
//
// Crear un array de structs que asocie nombre con función:
//   struct MathEntry {
//       const char *name;
//       MathFunc func;
//   };
//
// Poblar con: {"sqrt", sqrt}, {"sin", sin}, {"cos", cos},
//             {"log", log}, {"exp", exp}
//
// Implementar:
//   MathFunc find_func(const struct MathEntry *table, int n, const char *name)
// que busque por nombre y retorne la función (o NULL si no existe).
//
// Probar con: find_func(table, 5, "sqrt") aplicado a 144.0
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex04.c -o ex04 -lm
```

<details><summary>Predicción</summary>

```
sqrt(144.0) = 12.000000
sin(1.5708) = 1.000000
cos(0.0)    = 1.000000
log(2.7183) = 1.000000
exp(1.0)    = 2.718282
unknown("nope") = (not found)
```

`MathFunc` simplifica enormemente la declaración. Sin typedef, la firma
de `find_func` sería: `double (*find_func(...))(double)` — ilegible. La
tabla de dispatch asocia strings con punteros a función, permitiendo
buscar funciones por nombre en tiempo de ejecución.

</details>

---

### Ejercicio 5 — Callback con typedef

```c
// Definir:
//   typedef bool (*Predicate)(int);
//   typedef int (*Transform)(int);
//
// Implementar:
//   int count_if(const int *arr, int n, Predicate pred)
//       — cuenta cuántos elementos cumplen pred
//   void map_inplace(int *arr, int n, Transform fn)
//       — aplica fn a cada elemento in-place
//
// Funciones concretas:
//   bool is_even(int x) { return x % 2 == 0; }
//   bool is_positive(int x) { return x > 0; }
//   int double_it(int x) { return x * 2; }
//   int negate(int x) { return -x; }
//
// Probar con int arr[] = {-3, 4, 7, -1, 8, 0, 2}.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex05.c -o ex05
```

<details><summary>Predicción</summary>

```
Array: -3 4 7 -1 8 0 2
count_if(is_even):     3  (4, 8, 0)
count_if(is_positive): 4  (4, 7, 8, 2)

After map_inplace(double_it):
Array: -6 8 14 -2 16 0 4

After map_inplace(negate):
Array: 6 -8 -14 2 -16 0 -4
```

`count_if` recorre el array llamando `pred(arr[i])` y contando los `true`.
`map_inplace` modifica cada elemento con `arr[i] = fn(arr[i])`. Los typedef
hacen que las firmas sean claras: `Predicate` y `Transform` en vez de
`bool (*)(int)` y `int (*)(int)`.

</details>

---

### Ejercicio 6 — Opaque type: contador

```c
// Crear un módulo "counter" con tipo opaco:
//
// counter.h:
//   typedef struct Counter Counter;
//   Counter *counter_create(int initial);
//   void counter_increment(Counter *c);
//   void counter_decrement(Counter *c);
//   void counter_reset(Counter *c);
//   int counter_get(const Counter *c);
//   void counter_destroy(Counter *c);
//
// counter.c:
//   struct Counter { int value; int min_reached; int max_reached; };
//   - increment/decrement actualizan min_reached y max_reached
//   - get retorna value
//
// counter_main.c:
//   Crear contador, incrementar 5 veces, decrementar 3 veces,
//   imprimir valor actual.
//   Intentar c->value = 0; (comentado — explicar error).
//
// Compilar: gcc -std=c11 -Wall counter.c counter_main.c -o counter_main
```

<details><summary>Predicción</summary>

```
Initial: 0
After 5 increments: 5
After 3 decrements: 2
After reset: 0
```

`c->value = 0;` en `counter_main.c` daría:
`error: dereferencing pointer to incomplete type 'Counter'`
porque `Counter` es un tipo incompleto fuera de `counter.c`. Solo las
funciones definidas en `counter.c` (que tiene la definición completa del
struct) pueden acceder a los campos.

</details>

---

### Ejercicio 7 — typedef para array: la trampa del decay

```c
// Definir:
//   typedef int Row[4];
//   typedef Row Matrix[3];   // Matrix es int[3][4]
//
// En main:
//   Matrix m = {
//       {1, 2, 3, 4},
//       {5, 6, 7, 8},
//       {9, 10, 11, 12},
//   };
//
// Implementar:
//   void print_row(Row r)         — imprimir una fila
//   void print_matrix(Matrix m)   — imprimir toda la matriz
//
// Dentro de print_row, imprimir sizeof(r) y comparar con sizeof(Row).
// Dentro de print_matrix, imprimir sizeof(m) y comparar con sizeof(Matrix).
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex07.c -o ex07
```

<details><summary>Predicción</summary>

```
sizeof(Row)    in main   = 16  (4 × 4 bytes)
sizeof(r)      in func   = 8   (int* — decay!)

sizeof(Matrix) in main   = 48  (3 × 4 × 4 bytes)
sizeof(m)      in func   = 8   (int(*)[4] — decay!)

Row 0: 1 2 3 4
Row 1: 5 6 7 8
Row 2: 9 10 11 12
```

Dentro de las funciones, `Row r` decae a `int *r` (8 bytes) y `Matrix m`
decae a `int (*m)[4]` (puntero a array de 4 ints, 8 bytes). El typedef NO
impide el decay — solo cambia el nombre, no la semántica. En `main`, donde
la variable es un array real, sizeof da el tamaño completo.

</details>

---

### Ejercicio 8 — Forward declaration con typedef (mutua referencia)

```c
// Crear dos structs mutuamente referenciados:
//
//   typedef struct Teacher Teacher;
//   typedef struct Student Student;
//
//   struct Teacher {
//       char name[50];
//       Student **students;   // array dinámico de punteros
//       int num_students;
//   };
//
//   struct Student {
//       char name[50];
//       Teacher *advisor;     // puntero al asesor
//   };
//
// Implementar:
//   Teacher *teacher_create(const char *name)
//   Student *student_create(const char *name, Teacher *advisor)
//   void teacher_add_student(Teacher *t, Student *s)
//   void teacher_print(const Teacher *t)  — imprime nombre y lista de estudiantes
//
// Probar: crear un teacher, 3 students, asociarlos, imprimir.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex08.c -o ex08
```

<details><summary>Predicción</summary>

```
Teacher: Dr. Smith
  Students:
    - Alice (advisor: Dr. Smith)
    - Bob (advisor: Dr. Smith)
    - Carol (advisor: Dr. Smith)
```

Los forward declarations con typedef permiten que `Teacher` referencie
`Student *` y viceversa. Sin ellos, se necesitaría `struct Teacher` y
`struct Student` en todas partes. La referencia circular funciona porque
ambos usan **punteros** — el compilador solo necesita saber que el tipo
existe, no su tamaño completo.

</details>

---

### Ejercicio 9 — Refactorizar con typedef

```c
// Dado este código sin typedef, refactorizarlo para usar typedef
// donde mejore la legibilidad:
//
// struct linked_list_node {
//     int value;
//     struct linked_list_node *next;
// };
//
// void sort_list(struct linked_list_node **head,
//                int (*compare)(const void *, const void *));
//
// struct linked_list_node *find_node(struct linked_list_node *head, int val);
//
// void apply_to_all(struct linked_list_node *head,
//                   void (*action)(int *value));
//
// Implementar las funciones (sort_list puede ser bubble sort simple).
// Probar con una lista de 5 nodos.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex09.c -o ex09
```

<details><summary>Predicción</summary>

Refactorización:

```c
typedef struct Node Node;
typedef int (*Comparator)(const void *, const void *);
typedef void (*Action)(int *value);

struct Node {
    int value;
    Node *next;
};

void sort_list(Node **head, Comparator cmp);
Node *find_node(Node *head, int val);
void apply_to_all(Node *head, Action action);
```

La legibilidad mejora drásticamente: `Comparator` y `Action` son claros,
`Node` reemplaza `struct linked_list_node`. Las firmas pasan de 2 líneas
a 1 línea cada una.

</details>

---

### Ejercicio 10 — Opaque type con callback typedef

```c
// Combinar opaque type + typedef de función:
//
// timer.h:
//   typedef struct Timer Timer;
//   typedef void (*TimerCallback)(int tick, void *user_data);
//
//   Timer *timer_create(int interval_ms, TimerCallback cb, void *data);
//   void timer_simulate(Timer *t, int num_ticks);
//   void timer_destroy(Timer *t);
//
// timer.c:
//   struct Timer {
//       int interval_ms;
//       TimerCallback callback;
//       void *user_data;
//   };
//   - timer_simulate: loop de num_ticks, llamando cb(tick, data) cada vez
//
// timer_main.c:
//   - Callback que imprime "Tick N: message" donde message viene de user_data
//   - Crear timer con interval 100ms, simular 5 ticks
//
// Compilar: gcc -std=c11 -Wall timer.c timer_main.c -o timer_main
```

<details><summary>Predicción</summary>

```
Timer created (interval: 100ms)
Tick 0: Hello from timer
Tick 1: Hello from timer
Tick 2: Hello from timer
Tick 3: Hello from timer
Tick 4: Hello from timer
Timer destroyed.
```

Este ejercicio combina los dos usos más potentes de typedef:
- **Opaque type**: `Timer` es incompleto fuera de `timer.c`, forzando el uso
  de la API pública.
- **Function pointer typedef**: `TimerCallback` da un nombre legible al tipo
  de callback. Sin typedef sería `void (*)(int, void*)` en cada firma.

El patrón `void *user_data` es el mecanismo estándar en C para pasar
contexto a callbacks — equivalente a closures en Rust (que capturan el
entorno automáticamente).

</details>
