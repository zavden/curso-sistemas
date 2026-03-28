# T03 — typedef

## Qué es typedef

`typedef` crea un **alias** para un tipo existente.
No crea un tipo nuevo — solo un nombre alternativo:

```c
// Sin typedef:
unsigned long long int counter = 0;

// Con typedef:
typedef unsigned long long int u64;
u64 counter = 0;

// typedef no crea un tipo nuevo — u64 ES unsigned long long.
// Son completamente intercambiables.
```

```c
// Sintaxis: typedef tipo_existente nuevo_nombre;

typedef int Length;
typedef double Temperature;
typedef char *String;

Length width = 100;
Temperature temp = 36.5;
String name = "Alice";
// Son exactamente int, double y char*.
```

## typedef con structs

```c
// Sin typedef — hay que escribir "struct" cada vez:
struct Point {
    int x, y;
};
struct Point p = {10, 20};    // "struct" obligatorio

// Con typedef — se puede omitir "struct":
typedef struct Point {
    int x, y;
} Point;
Point p = {10, 20};          // sin "struct"

// O en dos líneas:
struct Point { int x, y; };
typedef struct Point Point;
```

```c
// Patrón común: typedef en la misma declaración:
typedef struct {
    int x, y;
} Point;
// El struct no tiene tag — solo se accede como "Point".
// No se puede hacer forward declaration sin tag.

// Con tag Y typedef (recomendado):
typedef struct Point {
    int x, y;
} Point;
// Se puede usar como "struct Point" o "Point".
// Se puede hacer forward declaration: struct Point;
```

## Convenciones

```c
// Convención 1: kernel de Linux — NO usa typedef para structs:
struct point { int x, y; };
struct point p;    // siempre "struct point"
// Razón de Linus: "typedef hides the fact that it's a struct.
// If you need to know it's a struct (because you pass it by
// pointer, allocate it, etc.), the syntax should remind you."

// Convención 2: Windows/POSIX — USA typedef:
typedef struct { int x, y; } POINT;
POINT p;

// Convención 3: sufijo _t (POSIX reserva nombres *_t):
typedef unsigned int uint32_t;    // stdint.h
typedef long ssize_t;             // POSIX
typedef unsigned long size_t;     // stdlib.h
// NOTA: POSIX reserva nombres que terminan en _t.
// No crear tipos con _t en código de usuario para evitar colisiones.

// En este curso: se usan ambas formas según contexto.
```

## typedef para punteros a función

```c
// Sin typedef — ilegible:
int (*get_operation(char op))(int, int);
// "get_operation es una función que toma char y retorna
//  un puntero a función (int,int)→int"

// Con typedef — legible:
typedef int (*BinaryOp)(int, int);
BinaryOp get_operation(char op);
// "get_operation retorna un BinaryOp"

// Callbacks:
typedef void (*EventHandler)(int event, void *data);

void register_handler(int event_type, EventHandler handler, void *data);

// Arrays de punteros a función:
typedef void (*Command)(void);
Command commands[10];
```

## Opaque types (tipos opacos)

El patrón de tipos opacos usa typedef + forward declaration
para **ocultar la implementación** de un tipo:

```c
// --- database.h (interfaz pública) ---
// El usuario ve esto:

typedef struct Database Database;    // forward declaration con typedef

Database *db_open(const char *path);
void db_close(Database *db);
int db_query(Database *db, const char *sql);

// El usuario NO sabe qué hay dentro de Database.
// Solo puede usar punteros a Database y las funciones públicas.
```

```c
// --- database.c (implementación privada) ---
#include "database.h"

// Aquí se define el struct completo:
struct Database {
    int fd;
    char path[256];
    void *cache;
    size_t cache_size;
    // ... detalles internos ...
};

Database *db_open(const char *path) {
    Database *db = calloc(1, sizeof(Database));
    if (!db) return NULL;
    // ... abrir archivo, inicializar cache ...
    return db;
}

void db_close(Database *db) {
    if (!db) return;
    // ... cerrar archivo, liberar cache ...
    free(db);
}
```

```c
// Ventajas de opaque types:
//
// 1. Encapsulación:
//    El usuario no puede acceder a los campos internos.
//    db->fd = 42;    // ERROR: incomplete type
//
// 2. Estabilidad de la API:
//    Se puede cambiar la implementación sin recompilar
//    el código del usuario. Solo database.c se recompila.
//
// 3. Compilación más rápida:
//    El header no incluye los tipos internos.
//    Cambiar la implementación no invalida los .o del usuario.
//
// Ejemplos reales: FILE* (stdio.h), DIR* (dirent.h),
// pthread_mutex_t (a veces implementado como opaque).
```

## typedef para arrays

```c
// typedef puede crear alias de tipos array:
typedef int Vector3[3];
typedef char Name[50];

Vector3 position = {1, 2, 3};
Name player_name = "Alice";

// CUIDADO: el typedef no cambia el decay de arrays.
// Cuando se pasa a una función, sigue siendo un puntero:
void foo(Vector3 v) {
    // v es int*, no int[3]
    sizeof(v);    // sizeof(int*) = 8, NO sizeof(int[3]) = 12
}

// Por esta razón, typedef de arrays puede ser confuso.
// Generalmente es mejor ser explícito.
```

## typedef vs #define

```c
// typedef es procesado por el compilador — entiende tipos:
typedef char *String;
String a, b;    // a y b son char*

// #define es sustitución de texto — NO entiende tipos:
#define String char *
String a, b;    // se expande a: char *a, b;
                // a es char*, b es char (¡diferente!)

// Otro ejemplo:
typedef int *IntPtr;
const IntPtr p;     // p es int *const (puntero const a int)

#define IntPtr int *
const IntPtr p;     // se expande a: const int *p (puntero a const int)
// ¡Significados diferentes!

// CONCLUSIÓN: usar typedef, no #define, para alias de tipos.
```

## Forward declarations con typedef

```c
// Forward declaration permite usar un tipo antes de definirlo:

// Sin typedef:
struct Node;    // forward declaration
struct Node {
    int value;
    struct Node *next;
};

// Con typedef:
typedef struct Node Node;    // forward declaration con typedef
struct Node {
    int value;
    Node *next;              // usar el typedef
};

// Structs mutuamente referenciados:
typedef struct A A;
typedef struct B B;

struct A {
    B *b;
};

struct B {
    A *a;
};
```

```c
// NO se puede hacer forward declaration de un struct sin tag:
typedef struct { int x; } Point;    // anónimo
// struct ???;    // ¿qué tag usar? No hay.

// Por eso la forma recomendada es con tag:
typedef struct Point Point;    // forward declaration
struct Point { int x, y; };   // definición
```

---

## Ejercicios

### Ejercicio 1 — Opaque type

```c
// Crear un módulo "stack" con tipo opaco:
// - stack.h: typedef struct Stack Stack;
//   Stack *stack_create(int capacity);
//   void stack_push(Stack *s, int value);
//   int stack_pop(Stack *s);
//   int stack_peek(const Stack *s);
//   _Bool stack_empty(const Stack *s);
//   void stack_destroy(Stack *s);
// - stack.c: struct Stack { int *data; int top; int cap; };
// Implementar y probar. El main no debe poder acceder a s->data.
```

### Ejercicio 2 — typedef para callbacks

```c
// Definir typedef para:
// - Comparator: función (const void*, const void*) → int
// - Predicate: función (const void*) → _Bool
// - Transform: función (int) → int
// Implementar filter(array, n, Predicate) que cuente matches.
// Implementar map(array, n, Transform) que aplique la función.
// Probar con funciones concretas (is_even, double_it, etc.)
```

### Ejercicio 3 — typedef convenciones

```c
// Refactorizar este código para usar typedef apropiadamente:
struct linked_list_node {
    int value;
    struct linked_list_node *next;
};

int (*compare_function)(const void *, const void *);

void sort(struct linked_list_node **head,
          int (*cmp)(const void *, const void *));

struct linked_list_node *find(struct linked_list_node *head, int val);
```
