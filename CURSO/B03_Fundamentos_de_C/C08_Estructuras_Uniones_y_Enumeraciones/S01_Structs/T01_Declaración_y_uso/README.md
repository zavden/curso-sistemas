# T01 — Declaración y uso

## Qué es un struct

Un struct agrupa variables de diferentes tipos bajo un solo nombre.
Es el mecanismo de C para crear tipos de datos compuestos:

```c
#include <stdio.h>

// Declaración del struct:
struct Point {
    int x;
    int y;
};

int main(void) {
    // Declarar una variable de tipo struct Point:
    struct Point p;
    p.x = 10;
    p.y = 20;

    printf("(%d, %d)\n", p.x, p.y);    // (10, 20)

    return 0;
}
```

```c
// El tag "Point" crea un nuevo tipo: struct Point.
// En C, el keyword "struct" es necesario al usar el tipo:
struct Point p;        // OK
// Point p;            // ERROR en C (OK en C++)

// Los miembros pueden ser de cualquier tipo:
struct Person {
    char name[50];
    int age;
    double height;
    char email[100];
};
```

## Inicialización

```c
// Inicialización en orden de declaración:
struct Point p = {10, 20};

// Designated initializers (C99) — por nombre:
struct Point q = { .x = 10, .y = 20 };

// Los miembros no mencionados se inicializan a 0:
struct Person alice = { .name = "Alice", .age = 30 };
// alice.height = 0.0
// alice.email = "" (todo zeros)

// El orden de los designated initializers no importa:
struct Point r = { .y = 20, .x = 10 };    // OK

// Inicialización a cero:
struct Person empty = {0};    // todo a cero
```

```c
// Designated initializers son preferidos porque:
// 1. Son legibles — se ve qué campo es cada valor
// 2. Son robustos — si se reordena el struct, no se rompe
// 3. Son seguros — campos omitidos se inicializan a 0

// MAL — depende del orden:
struct Person p = {"Bob", 25, 1.80, "bob@mail.com"};
// Si alguien agrega un campo antes de "age", se rompe.

// BIEN — por nombre:
struct Person p = {
    .name = "Bob",
    .age = 25,
    .height = 1.80,
    .email = "bob@mail.com",
};
// El trailing comma después del último campo es válido en C99+.
```

## Acceso con . y ->

```c
struct Point p = { .x = 10, .y = 20 };

// Acceso directo con . (dot operator):
printf("x = %d\n", p.x);
p.y = 30;

// Acceso a través de puntero con -> (arrow operator):
struct Point *pp = &p;
printf("x = %d\n", pp->x);    // equivale a (*pp).x
pp->y = 40;                    // equivale a (*pp).y = 40

// -> es azúcar sintáctico para (*ptr).member:
(*pp).x;     // verboso
pp->x;       // limpio — preferido
```

```c
// ¿Cuándo usar . vs ->?
// . para variables directas:      struct Point p;    p.x
// -> para punteros:                struct Point *p;   p->x

// Error común:
struct Point p;
// p->x = 10;    // ERROR: p no es un puntero

struct Point *pp;
// pp.x = 10;    // ERROR: pp es un puntero, usar ->
```

## Copiar structs

```c
// A diferencia de arrays, los structs se pueden copiar con =:
struct Point a = {10, 20};
struct Point b = a;           // copia todos los miembros
printf("b = (%d, %d)\n", b.x, b.y);   // (10, 20)

b.x = 100;
printf("a = (%d, %d)\n", a.x, a.y);   // (10, 20) — a no cambió

// La copia es SUPERFICIAL (shallow copy):
struct Shallow {
    int value;
    char *name;       // puntero — se copia la DIRECCIÓN
};

struct Shallow s1 = { .value = 42, .name = "hello" };
struct Shallow s2 = s1;
// s2.name apunta al MISMO string que s1.name.
// Si s1.name fue alocado con malloc:
//   free(s1.name) → s2.name es dangling pointer.
```

## Comparar structs

```c
// NO se pueden comparar structs con ==:
struct Point a = {10, 20};
struct Point b = {10, 20};
// if (a == b) {}    // ERROR: invalid operands to binary ==

// Opciones:
// 1. Comparar campo por campo:
if (a.x == b.x && a.y == b.y) {
    printf("equal\n");
}

// 2. Función de comparación:
int point_eq(struct Point a, struct Point b) {
    return a.x == b.x && a.y == b.y;
}

// 3. memcmp — CUIDADO:
if (memcmp(&a, &b, sizeof(struct Point)) == 0) {
    // Funciona SOLO si no hay padding entre campos.
    // Si hay padding, los bytes de padding pueden tener
    // valores diferentes → falso negativo.
}
```

## Structs y funciones

```c
// Pasar struct por valor (copia):
void print_point(struct Point p) {
    printf("(%d, %d)\n", p.x, p.y);
}

// Pasar struct por puntero (eficiente, permite modificar):
void move_point(struct Point *p, int dx, int dy) {
    p->x += dx;
    p->y += dy;
}

// Pasar por puntero const (eficiente, solo lectura):
double distance(const struct Point *a, const struct Point *b) {
    int dx = a->x - b->x;
    int dy = a->y - b->y;
    return sqrt(dx * dx + dy * dy);
}
```

```c
// Retornar struct por valor:
struct Point make_point(int x, int y) {
    return (struct Point){ .x = x, .y = y };
}
// El compilador optimiza la copia (NRVO).
// Para structs pequeños (< ~32 bytes) es eficiente.

// Retornar por puntero — para structs grandes o alocados:
struct Config *config_create(void) {
    struct Config *c = calloc(1, sizeof(struct Config));
    if (!c) return NULL;
    // ... inicializar ...
    return c;
}
```

## Compound literals (C99)

```c
// Un compound literal crea un struct temporal sin nombrar:
struct Point p = (struct Point){ .x = 10, .y = 20 };

// Útil para pasar structs directamente a funciones:
print_point((struct Point){ .x = 5, .y = 15 });

// Y para inicializar punteros a structs temporales:
struct Point *pp = &(struct Point){ .x = 1, .y = 2 };
// El compound literal tiene lifetime del scope donde se crea.

// Resetear un struct a cero:
p = (struct Point){0};
```

## Structs anónimos (C11)

```c
// Un struct puede contener structs anónimos:
struct Vector3 {
    union {
        struct { float x, y, z; };       // anónimo
        float components[3];
    };
};

struct Vector3 v = { .x = 1.0f, .y = 2.0f, .z = 3.0f };
printf("x = %f\n", v.x);              // acceso directo
printf("y = %f\n", v.components[1]);   // acceso por array

// Los miembros del struct anónimo se acceden como si
// fueran miembros del struct externo.
```

## Declaración anticipada (forward declaration)

```c
// Se puede declarar un struct sin definirlo (incomplete type):
struct Node;    // forward declaration

// Útil para punteros a structs mutuamente referenciados:
struct A {
    struct B *b;    // OK — solo necesita saber que struct B existe
};

struct B {
    struct A *a;    // OK — struct A ya está definido
};

// No se puede tener una variable de tipo incompleto:
// struct Node n;     // ERROR: incomplete type
struct Node *np;      // OK — el puntero tiene tamaño fijo (8 bytes)
```

## Flexible array member (C99)

```c
// El último miembro de un struct puede ser un array sin tamaño:
struct Message {
    size_t len;
    char data[];    // flexible array member — sin tamaño
};

// sizeof(struct Message) NO incluye data[].
// Se aloca con tamaño extra:
struct Message *msg = malloc(sizeof(struct Message) + 100);
msg->len = 100;
memcpy(msg->data, "hello", 6);
// msg->data tiene 100 bytes disponibles.

// Solo puede ser el ÚLTIMO miembro.
// Solo funciona con malloc (no se puede declarar en el stack).
// No se puede copiar con = (solo copia los campos fijos).
free(msg);
```

---

## Ejercicios

### Ejercicio 1 — Struct básico

```c
// Crear struct Rectangle { double width, height; }.
// Implementar:
// - double rect_area(const struct Rectangle *r)
// - double rect_perimeter(const struct Rectangle *r)
// - struct Rectangle rect_create(double w, double h)
// Probar con varios rectángulos.
```

### Ejercicio 2 — Array de structs

```c
// Crear struct Student { char name[50]; int grades[5]; double avg; }.
// Crear un array de 5 estudiantes con designated initializers.
// Calcular el promedio de cada estudiante.
// Ordenar por promedio con qsort.
// Imprimir la lista ordenada.
```

### Ejercicio 3 — Flexible array member

```c
// Crear struct Packet { uint16_t type; uint16_t length; char payload[]; }.
// Implementar:
// - struct Packet *packet_create(uint16_t type, const void *data, uint16_t len)
// - void packet_print(const struct Packet *p)
// - void packet_free(struct Packet *p)
// Crear 3 packets con diferentes datos. Verificar con valgrind.
```
