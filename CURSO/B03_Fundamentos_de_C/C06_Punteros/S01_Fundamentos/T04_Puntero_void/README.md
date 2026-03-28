# T04 — Puntero void*

## Qué es void*

`void *` es un puntero **genérico** que puede apuntar a cualquier
tipo de dato. No tiene tipo asociado — solo almacena una dirección:

```c
#include <stdio.h>

int main(void) {
    int x = 42;
    double d = 3.14;
    char c = 'A';

    // void* puede apuntar a cualquier tipo:
    void *p;
    p = &x;     // OK — int* → void*
    p = &d;     // OK — double* → void*
    p = &c;     // OK — char* → void*

    // En C, la conversión entre void* y cualquier puntero
    // es implícita (no necesita cast):
    int *ip = &x;
    void *vp = ip;     // int* → void* — implícito
    int *ip2 = vp;     // void* → int* — implícito en C

    // En C++, void* → tipo* requiere cast explícito.
    // Por eso en C NO se castea malloc:
    int *arr = malloc(10 * sizeof(int));    // correcto en C
    // int *arr = (int *)malloc(10 * sizeof(int));  // innecesario en C

    return 0;
}
```

## No se puede desreferenciar void*

```c
void *p = &some_int;

// *p;      // ERROR: void* no tiene tipo — ¿cuántos bytes leer?
// p[0];    // ERROR: mismo problema
// p + 1;   // NO ESTÁNDAR — GCC lo permite como extensión (suma 1 byte)

// Para acceder al dato, convertir a un tipo concreto:
int *ip = p;
printf("%d\n", *ip);

// O con cast inline:
printf("%d\n", *(int *)p);
```

```c
// ¿Por qué no se puede desreferenciar?
// El compilador no sabe cuántos bytes leer ni cómo interpretarlos.
// Un int* sabe que debe leer 4 bytes como entero.
// Un double* sabe que debe leer 8 bytes como IEEE 754.
// Un void* no sabe nada — solo tiene la dirección.
```

## void* para funciones genéricas

El uso principal de `void*` es escribir funciones que operen
con **cualquier tipo**:

```c
#include <stdio.h>
#include <string.h>

// Intercambiar dos valores de cualquier tipo:
void swap_generic(void *a, void *b, size_t size) {
    unsigned char tmp[size];    // VLA como buffer temporal
    memcpy(tmp, a, size);
    memcpy(a, b, size);
    memcpy(b, tmp, size);
}

int main(void) {
    // Funciona con ints:
    int x = 10, y = 20;
    swap_generic(&x, &y, sizeof(int));
    printf("x=%d y=%d\n", x, y);    // x=20 y=10

    // Funciona con doubles:
    double a = 1.1, b = 2.2;
    swap_generic(&a, &b, sizeof(double));
    printf("a=%f b=%f\n", a, b);    // a=2.2 b=1.1

    // Funciona con structs:
    struct Point { int x, y; };
    struct Point p1 = {1, 2}, p2 = {3, 4};
    swap_generic(&p1, &p2, sizeof(struct Point));
    printf("p1={%d,%d}\n", p1.x, p1.y);   // p1={3,4}

    return 0;
}
```

### qsort — El ejemplo canónico

```c
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// qsort usa void* para ser genérica:
// void qsort(void *base, size_t nmemb, size_t size,
//            int (*compar)(const void *, const void *));

// Comparar ints:
int cmp_int(const void *a, const void *b) {
    int ia = *(const int *)a;    // cast void* → int*
    int ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}

// Comparar doubles:
int cmp_double(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da > db) - (da < db);
}

// Comparar structs por un campo:
struct Student {
    char name[50];
    int grade;
};

int cmp_by_grade(const void *a, const void *b) {
    const struct Student *sa = a;    // cast implícito en C
    const struct Student *sb = b;
    return (sa->grade > sb->grade) - (sa->grade < sb->grade);
}

int cmp_by_name(const void *a, const void *b) {
    const struct Student *sa = a;
    const struct Student *sb = b;
    return strcmp(sa->name, sb->name);
}

int main(void) {
    int nums[] = {5, 2, 8, 1, 9};
    qsort(nums, 5, sizeof(int), cmp_int);
    // nums = {1, 2, 5, 8, 9}

    struct Student class[] = {
        {"Charlie", 85}, {"Alice", 92}, {"Bob", 78}
    };
    qsort(class, 3, sizeof(struct Student), cmp_by_name);
    // Ordenado por nombre: Alice, Bob, Charlie

    return 0;
}
```

### bsearch — Búsqueda binaria genérica

```c
#include <stdlib.h>

// bsearch busca en un array ordenado:
// void *bsearch(const void *key, const void *base,
//               size_t nmemb, size_t size,
//               int (*compar)(const void *, const void *));

int nums[] = {1, 2, 5, 8, 9};    // debe estar ordenado
int key = 5;
int *found = bsearch(&key, nums, 5, sizeof(int), cmp_int);
if (found) {
    printf("Found %d at index %td\n", *found, found - nums);
} else {
    printf("Not found\n");
}
```

## void* como "user data" (contexto)

```c
// Patrón: pasar datos extra a un callback usando void*:

typedef void (*Callback)(int event, void *user_data);

void register_callback(Callback cb, void *user_data) {
    // Cuando ocurre el evento:
    cb(42, user_data);
}

// El callback recibe sus datos a través del void*:
struct MyData {
    const char *name;
    int count;
};

void my_handler(int event, void *user_data) {
    struct MyData *data = user_data;    // cast implícito
    data->count++;
    printf("[%s] event %d (count=%d)\n", data->name, event, data->count);
}

int main(void) {
    struct MyData ctx = { .name = "handler1", .count = 0 };
    register_callback(my_handler, &ctx);
    // [handler1] event 42 (count=1)
    return 0;
}
```

## Reglas de aliasing y void*

```c
// Strict aliasing rule: acceder a un objeto a través de un
// puntero de tipo incompatible es UB:

int x = 42;
float *fp = (float *)&x;
float f = *fp;              // UB — strict aliasing violation

// EXCEPCIONES — siempre se puede acceder como:
// 1. El tipo real del objeto
// 2. Un tipo compatible (signed/unsigned del mismo tipo)
// 3. char* o unsigned char* (acceso byte a byte)
// 4. void* solo almacena la dirección — no accede al dato

// void* en sí NO viola aliasing porque no se desreferencia.
// La violación ocurre al convertir a un tipo incorrecto
// y desreferenciar:

void *vp = &x;
float *bad = vp;
*bad;                       // UB — x es int, no float

int *good = vp;
*good;                      // OK — x es int
```

```c
// char* y unsigned char* son especiales — SIEMPRE se pueden
// usar para leer bytes de cualquier objeto:

int x = 0x04030201;
unsigned char *bytes = (unsigned char *)&x;
for (int i = 0; i < (int)sizeof(x); i++) {
    printf("byte[%d] = 0x%02x\n", i, bytes[i]);
}
// Es legal y portátil (excepto por endianness).

// memcpy y memset funcionan con void* internamente
// usando char* para acceder byte a byte — siempre legal.
```

## Cast seguro vs peligroso

```c
// SEGURO — conversión al tipo original:
int x = 42;
void *vp = &x;
int *ip = vp;          // OK — el tipo original era int
printf("%d\n", *ip);   // 42

// PELIGROSO — conversión a tipo incorrecto:
void *vp2 = &x;
double *dp = vp2;      // compila pero...
printf("%f\n", *dp);   // UB — lee 8 bytes de un int de 4

// void* no tiene type safety — el programador es responsable
// de recordar qué tipo almacena. Un error de tipo no se
// detecta en compilación ni en runtime.
```

## Comparación con generics en otros lenguajes

```c
// C: void* — sin type safety:
void *container[10];
container[0] = &some_int;
container[1] = &some_string;    // compila — sin verificación
// El programador debe saber qué hay en cada posición.

// C11: _Generic — dispatch por tipo en compilación:
#define print_val(x) _Generic((x), \
    int:    printf("%d\n", x),     \
    double: printf("%f\n", x),     \
    char *: printf("%s\n", x)      \
)

// Rust: generics con monomorphization — type-safe, zero cost:
// fn swap<T>(a: &mut T, b: &mut T) { ... }
// El compilador genera una versión para cada tipo usado.
```

---

## Ejercicios

### Ejercicio 1 — Generic max

```c
// Implementar void *generic_max(void *a, void *b,
//                                int (*cmp)(const void *, const void *))
// que retorne un puntero al mayor de los dos valores.
// Probar con ints, doubles y strings (usando strcmp).
```

### Ejercicio 2 — Generic array search

```c
// Implementar void *linear_search(const void *key, const void *base,
//                                  size_t nmemb, size_t size,
//                                  int (*cmp)(const void *, const void *))
// Similar a bsearch pero búsqueda lineal (no requiere array ordenado).
// Probar con un array de structs.
```

### Ejercicio 3 — Generic print array

```c
// Implementar void print_array(const void *base, size_t nmemb,
//                               size_t size,
//                               void (*print_elem)(const void *))
// que imprima cada elemento usando la función print_elem.
// Crear print_int, print_double, print_string.
// Probar con arrays de cada tipo.
```
