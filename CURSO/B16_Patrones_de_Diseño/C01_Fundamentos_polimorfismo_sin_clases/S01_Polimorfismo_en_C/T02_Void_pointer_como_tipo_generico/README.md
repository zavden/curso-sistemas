# T02 — void* como tipo generico

---

## 1. El problema: datos genericos sin templates

En C++ tienes templates, en Rust tienes generics + traits. Ambos permiten
escribir codigo que funciona con cualquier tipo, y el compilador genera
una version especializada para cada tipo concreto (monomorphization).

C no tiene nada de esto. Pero el problema sigue existiendo: necesitas
escribir funciones y estructuras de datos que funcionen con **cualquier
tipo** sin reescribir una version para `int`, otra para `double`, otra
para `Student`, etc.

```
El problema:

  stack_push_int(s, 42);         // una funcion para int
  stack_push_double(s, 3.14);    // otra para double
  stack_push_student(s, &bob);   // otra para Student
  // ... y asi para cada tipo nuevo

La solucion en C:

  stack_push(s, &value);         // UNA funcion para todo
  // void* borra el tipo → la funcion no sabe (ni necesita saber)
  // que tipo es value
```

La herramienta de C para esto es `void *`: un puntero que puede apuntar
a **cualquier tipo** de dato. Es la base de toda genericidad en C.

---

## 2. Que es void*

### 2.1 Definicion

`void *` es un puntero sin tipo asociado. El estandar de C garantiza que:

- Un `void *` puede almacenar la direccion de **cualquier** objeto.
- La conversion de `T *` a `void *` y de vuelta a `T *` preserva la
  direccion original.
- No se puede desreferenciar directamente — primero hay que castear.

```c
int x = 42;
void *ptr = &x;        // int* → void*  (conversion implicita)

// No puedes hacer esto:
// int y = *ptr;        // ERROR: no se puede desreferenciar void*

// Debes castear primero:
int y = *(int *)ptr;    // void* → int* → desreferenciar → 42
```

### 2.2 Conversiones implicitas vs explicitas

En C, las conversiones entre `void *` y otros tipos de puntero son
**implicitas** — no necesitas cast:

```c
int x = 42;
void *vp = &x;           // OK: int* → void* implicito
int *ip = vp;             // OK: void* → int* implicito (en C)

// En C++, la segunda linea requiere cast explicito:
// int *ip = (int *)vp;   // requerido en C++, opcional en C
```

Esto es una diferencia importante entre C y C++. En C, `void *` es mas
permisivo, lo que facilita el codigo generico pero tambien facilita
errores.

### 2.3 void* en memoria

Un `void *` es solo una direccion — ocupa lo mismo que cualquier otro
puntero (tipicamente 8 bytes en x86-64):

```
         void *ptr = &x;

  ptr:   ┌──────────────────┐
         │  0x7ffd4a3b8c20  │  ← solo almacena la direccion
         └──────────────────┘
                  │
                  ▼
  x:     ┌──────────────────┐
         │   42 (int)       │  ← el dato real, con su tipo
         └──────────────────┘

  El void* NO sabe que x es int.
  NO sabe que ocupa 4 bytes.
  NO sabe como interpretar los bits.
  Solo sabe la direccion.
```

Esto se llama **type erasure**: el tipo del dato se borra al pasar por
`void *`. Es responsabilidad del programador recordar (o documentar)
que tipo hay detras.

---

## 3. Casting: de void* a tipo concreto

### 3.1 El cast basico

El patron fundamental: recibir `void *`, castear al tipo que **sabes**
que es, y usar.

```c
void print_int(const void *data) {
    const int *p = (const int *)data;
    printf("%d\n", *p);
}

void print_double(const void *data) {
    const double *p = (const double *)data;
    printf("%.2f\n", *p);
}

int x = 42;
double y = 3.14;

print_int(&x);       // 42
print_double(&y);     // 3.14
```

La clave: el que invoca la funcion **sabe** que tipo paso. El que la
recibe **asume** que el cast es correcto. Si alguien pasa `&y` a
`print_int`, no hay error de compilacion — solo basura en runtime.

### 3.2 const correctness con void*

`const void *` y `void *` no son intercambiables:

```c
void modify(void *data);
void inspect(const void *data);

const int cx = 10;

// inspect(&cx);   // OK: const int* → const void* (preserva const)
// modify(&cx);    // WARNING: descarta const qualifier

int mx = 20;
// inspect(&mx);   // OK: int* → const void* (agrega const, siempre valido)
// modify(&mx);    // OK: int* → void*
```

Regla: usa `const void *` cuando la funcion no modifica el dato.
Igual que en codigo tipado, pero mas importante aqui porque el cast
puede esconder errores.

### 3.3 Tabla de conversiones

| De | A | En C | En C++ |
|----|---|------|--------|
| `int *` | `void *` | implicito | implicito |
| `void *` | `int *` | implicito | requiere cast |
| `const int *` | `const void *` | implicito | implicito |
| `const void *` | `const int *` | implicito | requiere cast |
| `int *` | `const void *` | implicito | implicito |
| `const int *` | `void *` | warning (descarta const) | error |
| `void *` | `int` (no puntero) | error/warning | error |

---

## 4. Funciones genericas de la stdlib

La biblioteca estandar de C usa `void *` extensivamente. Estas son las
funciones que ya conoces de B03, ahora vistas desde la perspectiva de
genericidad.

### 4.1 memcpy — copia generica de bytes

```c
void *memcpy(void *dest, const void *src, size_t n);
```

`memcpy` no sabe que copia — solo mueve `n` bytes de `src` a `dest`.
Es la funcion generica mas fundamental:

```c
int a = 42;
int b;
memcpy(&b, &a, sizeof(int));   // copia 4 bytes: b == 42

Student s1 = {"Ana", 20};
Student s2;
memcpy(&s2, &s1, sizeof(Student));  // copia todo el struct
```

### 4.2 qsort — ordenamiento generico

Ya visto en T01, pero ahora observa la firma completa:

```c
void qsort(void *base,          // array de CUALQUIER tipo
            size_t nmemb,        // cuantos elementos
            size_t size,         // tamano de CADA elemento
            int (*compar)(const void *, const void *));
```

`qsort` necesita tres piezas de informacion para trabajar con `void *`:
1. **Direccion base** (`void *base`)
2. **Tamano del elemento** (`size_t size`) — sin esto, no puede
   calcular la direccion del elemento `i`
3. **Funcion de comparacion** (`compar`) — sin esto, no puede ordenar

```
  Direccion del elemento i:

  base + i * size      ← aritmetica que qsort hace internamente
                         (cast a char* para que funcione)

  ┌──────┬──────┬──────┬──────┬──────┐
  │ [0]  │ [1]  │ [2]  │ [3]  │ [4]  │  array de void*
  └──────┴──────┴──────┴──────┴──────┘
  ← size →                              cada elemento ocupa 'size' bytes
```

### 4.3 bsearch — busqueda binaria generica

```c
void *bsearch(const void *key,
              const void *base,
              size_t nmemb,
              size_t size,
              int (*compar)(const void *, const void *));
```

Mismo patron: `void *` + `size_t size` + comparador. Retorna `void *`
(puntero al elemento encontrado) o `NULL`.

### 4.4 El patron size_t size

Observa el patron comun: toda funcion generica que opera sobre `void *`
necesita recibir el **tamano del elemento** como parametro separado.

```
  Con tipos:    sort(int *arr, int n);        // el tipo lleva el tamano
  Con void*:    sort(void *arr, int n, size_t size);  // tamano explicito
```

Esto es lo que Rust generics resuelve en compile time: el compilador
genera `sort::<int>` y ya sabe que `sizeof::<int>() == 4`. Con `void *`,
esa informacion se pasa en runtime.

---

## 5. Construir funciones genericas con void*

### 5.1 Swap generico

El ejemplo canonico de funcion generica en C:

```c
void swap(void *a, void *b, size_t size) {
    unsigned char tmp[size];  // VLA (C99) como buffer temporal
    memcpy(tmp, a, size);
    memcpy(a, b, size);
    memcpy(b, tmp, size);
}

// Uso:
int x = 10, y = 20;
swap(&x, &y, sizeof(int));       // x == 20, y == 10

double d1 = 1.1, d2 = 2.2;
swap(&d1, &d2, sizeof(double));   // d1 == 2.2, d2 == 1.1

Student s1 = {"Ana", 20}, s2 = {"Bob", 22};
swap(&s1, &s2, sizeof(Student));  // s1 == {"Bob",22}, s2 == {"Ana",20}
```

Una funcion, tres tipos distintos. Sin `void *`, necesitarias tres
funciones (`swap_int`, `swap_double`, `swap_student`).

> **Nota**: el VLA `unsigned char tmp[size]` usa stack. Para structs
> grandes, se podria usar `malloc` + `free`. La implementacion de
> `qsort` de glibc usa esta optimizacion.

### 5.2 Array dinamico generico

Un contenedor que almacena elementos de cualquier tipo:

```c
typedef struct {
    void  *data;       // buffer de bytes
    size_t elem_size;  // tamano de cada elemento
    size_t count;      // elementos actuales
    size_t capacity;   // capacidad total
} GenArray;

GenArray *genarray_create(size_t elem_size, size_t initial_cap) {
    GenArray *arr = malloc(sizeof(GenArray));
    arr->data = malloc(elem_size * initial_cap);
    arr->elem_size = elem_size;
    arr->count = 0;
    arr->capacity = initial_cap;
    return arr;
}

void genarray_push(GenArray *arr, const void *elem) {
    if (arr->count == arr->capacity) {
        arr->capacity *= 2;
        arr->data = realloc(arr->data, arr->elem_size * arr->capacity);
    }
    // Calcular destino: base + count * elem_size
    void *dest = (char *)arr->data + arr->count * arr->elem_size;
    memcpy(dest, elem, arr->elem_size);
    arr->count++;
}

void *genarray_get(const GenArray *arr, size_t index) {
    return (char *)arr->data + index * arr->elem_size;
}
```

Observa:
- `(char *)arr->data + index * arr->elem_size` — aritmetica manual
  porque `void *` no soporta aritmetica (no sabe el tamano del
  elemento).
- `memcpy` copia bytes sin importar el tipo.
- El usuario castea el resultado de `genarray_get`:

```c
GenArray *ints = genarray_create(sizeof(int), 16);

int val = 42;
genarray_push(ints, &val);

int *retrieved = (int *)genarray_get(ints, 0);
printf("%d\n", *retrieved);  // 42
```

### 5.3 Diagrama de un GenArray de ints

```
  GenArray         data buffer (heap)
  ┌───────────┐    ┌─────┬─────┬─────┬─────┬─────┬─────┐
  │ data ───────→  │  42 │  17 │   8 │  ?? │  ?? │  ?? │
  │ elem_size: 4│   └─────┴─────┴─────┴─────┴─────┴─────┘
  │ count: 3    │    [0]    [1]    [2]   [3]   [4]   [5]
  │ capacity: 6 │
  └───────────┘    offset de [i] = i * elem_size

  genarray_get(arr, 1) → (char*)data + 1*4 → &data[4] → 17
```

El mismo GenArray funciona con `double`, con `Student`, con cualquier
tipo — siempre y cuando el `elem_size` sea correcto.

---

## 6. Ownership y lifetime con void*

El problema mas peligroso de `void *` no es el tipo — es el **ownership**.
Cuando pasas un `void *`, la pregunta critica es: **quien es responsable
de liberar la memoria?**

### 6.1 Tres convenciones de ownership

```
  Convencion     Quien libera         Analogo Rust
  ──────────     ──────────────       ────────────
  Borrow         El llamador          &T / &mut T
  Copy           Cada uno el suyo     Clone + owned
  Take           El receptor          Box<T> (move)
```

#### Borrow (prestamo)

La funcion recibe un puntero pero **no toma posesion**. El llamador
sigue siendo responsable de la memoria.

```c
// Esta funcion solo LEE el dato — no lo libera ni lo almacena
void print_value(const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;
    for (size_t i = 0; i < size; i++)
        printf("%02x ", bytes[i]);
    printf("\n");
}

int x = 42;
print_value(&x, sizeof(x));  // x sigue vivo despues de la llamada
```

#### Copy

La funcion hace una copia del dato. Ambos (llamador y funcion) tienen
su propia copia.

```c
// genarray_push copia el dato internamente con memcpy
void genarray_push(GenArray *arr, const void *elem) {
    // ...
    memcpy(dest, elem, arr->elem_size);  // copia
}

int x = 42;
genarray_push(arr, &x);
// x sigue vivo Y el array tiene su propia copia
```

#### Take (transferencia)

La funcion toma posesion del dato. El llamador ya **no debe** usar
ni liberar esa memoria.

```c
// Esta funcion toma posesion del nodo y lo inserta en la lista
void list_push(List *list, void *node) {
    // node ahora pertenece a la lista
    // el llamador NO debe hacer free(node) despues de esto
}

Node *n = malloc(sizeof(Node));
list_push(mylist, n);
// n ya NO te pertenece — no hagas free(n)
```

### 6.2 Documentar ownership en la API

Sin un sistema de tipos que lo enforce (como Rust), la unica defensa
es la **documentacion**:

```c
/**
 * genarray_push - Agrega un elemento al array.
 * @arr: El array generico.
 * @elem: Puntero al elemento a copiar. El array hace una copia interna
 *        con memcpy — el llamador conserva ownership del original.
 *
 * Convencion: COPY (el array copia, el llamador retiene el original)
 */
void genarray_push(GenArray *arr, const void *elem);

/**
 * genarray_destroy - Libera el array y su buffer interno.
 * @arr: El array a destruir.
 *
 * NOTA: Si los elementos contienen punteros a heap (e.g., char*),
 * esos punteros NO se liberan. El llamador debe iterar y limpiar
 * antes de llamar a genarray_destroy.
 */
void genarray_destroy(GenArray *arr);
```

### 6.3 El problema del contenido profundo

Cuando un `GenArray` almacena structs que contienen punteros, la copia
con `memcpy` es **shallow** (solo copia los bytes del struct, no lo que
apuntan los punteros internos):

```
  Original:                    Copia (memcpy):

  Student s                    Student copy
  ┌─────────────┐              ┌─────────────┐
  │ name ─────────────┐        │ name ─────────────┐
  │ age: 20     │     │        │ age: 20     │     │
  └─────────────┘     │        └─────────────┘     │
                      ▼                             │
                 ┌──────────┐                       │
                 │  "Ana"   │ ◀─────────────────────┘
                 └──────────┘
                 ¡AMBOS apuntan al mismo string!
```

Si liberas el original, la copia tiene un **dangling pointer**. Esta es
la razon por la que Rust distingue `Copy` (bitwise copy segura) de
`Clone` (copia profunda).

---

## 7. Limitaciones de void*

### 7.1 No hay type safety

El compilador no puede verificar que el cast sea correcto:

```c
double pi = 3.14159;
void *ptr = &pi;

// El compilador no se queja:
int *wrong = (int *)ptr;
printf("%d\n", *wrong);    // basura: interpreta los bits de double como int
```

En Rust, esto es imposible:

```rust
let pi: f64 = 3.14159;
let ptr: *const f64 = &pi;

// Error de compilacion:
// let wrong: *const i32 = ptr;  // types differ
```

### 7.2 No se puede hacer aritmetica de punteros

```c
void *ptr = some_array;
// ptr + 1;         // ERROR: void* no tiene tamano, no se puede sumar
// ptr[3];          // ERROR: misma razon

// Solucion: castear a char* (1 byte) y sumar manualmente
void *elem = (char *)ptr + index * elem_size;
```

Esto es por que toda funcion generica necesita recibir `size_t size`:
sin el tamano del elemento, no puedes navegar el array.

### 7.3 No se conoce el tamano del dato

```c
void mystery(const void *data) {
    // Cuantos bytes tiene data? No hay forma de saberlo.
    // sizeof(*data) no compila
    // No hay funcion que lo revele
    // El tamano se perdio con el tipo
}
```

Compara con Rust:

```rust
fn mystery<T>(data: &T) {
    let size = std::mem::size_of::<T>();  // el compilador lo sabe
}
```

### 7.4 No se puede detectar el tipo en runtime

C no tiene RTTI (Runtime Type Information). Una vez que el tipo se borra
con `void *`, no hay forma de recuperarlo. Ni `typeof` (extension GCC)
funciona sobre `void *` — retorna `void`.

La alternativa es un **tagged union** o pasar un enum de tipo
manualmente (seccion 8).

---

## 8. Alignment

### 8.1 Que es alignment

Cada tipo tiene un **alignment** (alineacion): la direccion de memoria
donde puede residir debe ser multiplo de su alineacion.

```
  Tipo        Tamano    Alignment (tipico x86-64)
  ──────      ──────    ─────────────────────────
  char          1          1
  short         2          2
  int           4          4
  long          8          8
  double        8          8
  void*         8          8
```

Una variable `int` debe estar en una direccion multiplo de 4.
Una variable `double` debe estar en una direccion multiplo de 8.

### 8.2 Alignment y void*

Cuando usas `void *` para almacenar datos genericos, el alignment se
vuelve tu responsabilidad:

```c
// malloc SIEMPRE retorna memoria alineada al maximo alignment
// (tipicamente 16 bytes en x86-64). Esto es seguro:
void *buf = malloc(100);
double *d = (double *)buf;   // OK: malloc garantiza alignment

// Pero esto puede fallar:
char raw[100];
double *bad = (double *)(raw + 3);  // PELIGRO: raw+3 puede no estar
                                     // alineado a 8 bytes
```

### 8.3 _Alignof (C11)

C11 introduce `_Alignof` (y el macro `alignof` en `<stdalign.h>`):

```c
#include <stdalign.h>

printf("alignof(int)    = %zu\n", alignof(int));     // 4
printf("alignof(double) = %zu\n", alignof(double));  // 8

// Verificar alignment manualmente:
void *ptr = some_allocation;
if ((uintptr_t)ptr % alignof(double) != 0) {
    // ptr no esta alineado para double — peligro
}
```

En la practica, `malloc` ya maneja el alignment. El problema surge con
buffers custom, arenas, o pools de memoria — temas de C05 y C09.

---

## 9. void* vs alternativas en C

### 9.1 Tagged union

En vez de borrar el tipo, lo acompanas con una etiqueta:

```c
typedef enum { TYPE_INT, TYPE_DOUBLE, TYPE_STRING } ValueType;

typedef struct {
    ValueType type;
    union {
        int    as_int;
        double as_double;
        char  *as_string;
    };
} Value;

void print_value(const Value *v) {
    switch (v->type) {
        case TYPE_INT:    printf("%d\n", v->as_int); break;
        case TYPE_DOUBLE: printf("%.2f\n", v->as_double); break;
        case TYPE_STRING: printf("%s\n", v->as_string); break;
    }
}
```

| Aspecto | void* | Tagged union |
|---------|-------|-------------|
| Type safety | Ninguna — cast manual | Parcial — el switch verifica |
| Tipos soportados | Cualquiera | Solo los definidos en el enum |
| Tamano | Solo un puntero (8 bytes) | Tamano del mayor miembro + tag |
| Extensibilidad | Abierta | Cerrada (recompilar para agregar) |
| Equivalente Rust | `Box<dyn Any>` | `enum Value { Int(i32), ... }` |

La tagged union es el equivalente directo de los enums de Rust.

### 9.2 Macros generadoras

Otra alternativa: usar macros del preprocesador para generar codigo
tipado:

```c
#define DECLARE_STACK(T, Name)                    \
    typedef struct {                              \
        T *data;                                  \
        size_t count, capacity;                   \
    } Name;                                       \
    void Name##_push(Name *s, T val) {            \
        if (s->count == s->capacity) { /* ... */ }\
        s->data[s->count++] = val;                \
    }                                             \
    T Name##_pop(Name *s) {                       \
        return s->data[--s->count];               \
    }

DECLARE_STACK(int, IntStack)
DECLARE_STACK(double, DoubleStack)
```

| Aspecto | void* | Macros |
|---------|-------|--------|
| Type safety | Ninguna | Completa (tipo embebido en la struct) |
| Code bloat | Una sola implementacion | Una copia por tipo (como monomorphization) |
| Debugging | Legible | Macros expandidas son ilegibles |
| Error messages | Claros | Crpticos (linea del macro, no del uso) |
| Equivalente Rust | `dyn Trait` | Generics (monomorphization) |

### 9.3 _Generic (C11)

C11 permite dispatch por tipo en compile time:

```c
#define print_val(x) _Generic((x),    \
    int:    print_int,                 \
    double: print_double,              \
    char *: print_string               \
)(x)

print_val(42);       // llama print_int(42)
print_val(3.14);     // llama print_double(3.14)
print_val("hello");  // llama print_string("hello")
```

Util para APIs amigables, pero no reemplaza `void *` para contenedores
o callbacks — `_Generic` solo funciona en compile time con tipos
conocidos.

---

## 10. C void* vs Rust generics

| Aspecto | C (`void *`) | Rust (generics) | Rust (`dyn Trait`) |
|---------|-------------|-----------------|-------------------|
| Resolucion | Runtime | Compile time | Runtime |
| Type safety | Ninguna | Completa | Completa (via trait) |
| Tamano del tipo | Se pasa como `size_t` | Compilador lo sabe | Compilador lo sabe |
| Codigo generado | Una copia | Una por tipo (monomorphization) | Una copia + vtable |
| Performance | Indirecciones, memcpy | Zero-cost | Un nivel de indirection |
| Errores de tipo | Runtime (crash, UB) | Compile time | Compile time |
| ABI estable | Si | No (name mangling) | No |

El flujo conceptual:

```
  C void*:   Programador castea → el codigo confia → UB si esta mal
  Rust gen:  Compilador verifica → monomorphiza → zero-cost en runtime
  Rust dyn:  Compilador verifica → vtable → un indirection en runtime
```

`void *` es la version "manual y peligrosa" de lo que Rust automatiza.
Entender `void *` a fondo te permite apreciar exactamente que problema
resuelven los generics de Rust — y por que Rust insiste tanto en que
el compilador verifique los tipos.

---

## Errores comunes

### Error 1 — Castear a tipo incorrecto

```c
float f = 3.14f;
void *ptr = &f;

int *wrong = (int *)ptr;
printf("%d\n", *wrong);
// Imprime 1078523331 (los bits de 3.14f interpretados como int)
// No hay crash — solo datos corruptos silenciosamente
```

Este es el error mas comun y mas peligroso. Compila limpio, no hay
segfault, pero el resultado es basura. Usa `-Wall -Wextra` y considera
wrappings con asserts en debug:

```c
#ifdef DEBUG
    assert(arr->type_tag == TYPE_FLOAT && "tipo incorrecto en cast");
#endif
```

### Error 2 — Olvidar sizeof

```c
GenArray *arr = genarray_create(sizeof(int), 16);
double d = 3.14;
genarray_push(arr, &d);  // BUG: copia solo 4 bytes de un double de 8
```

El array fue creado con `elem_size = sizeof(int) = 4`. Si haces push
de un `double` (8 bytes), solo se copian 4 bytes. No hay error — solo
datos truncados.

### Error 3 — Aritmetica sobre void*

```c
void *ptr = array;
void *next = ptr + 1;  // WARNING/ERROR: void* no tiene tamano

// Correcto:
char *byte_ptr = (char *)ptr;
void *next = byte_ptr + elem_size;
```

GCC permite `void * + n` como extension (trata `void` como size 1),
pero esto no es estandar y otros compiladores lo rechazan.

### Error 4 — Use-after-free via void*

```c
void *stolen;

{
    int local = 42;
    stolen = &local;
}
// 'local' ya no existe — 'stolen' es un dangling pointer
printf("%d\n", *(int *)stolen);  // UB: stack ya fue reutilizado
```

`void *` no sabe de lifetimes. En Rust, el borrow checker impide esto
en compile time.

---

## Ejercicios

### Ejercicio 1 — Casting basico

Sin compilar, predice la salida de cada fragmento:

```c
// A
int x = 0x41424344;
char *cp = (char *)&x;
printf("%c\n", cp[0]);

// B
double d = 0.0;
void *vp = &d;
int *ip = (int *)vp;
printf("%d\n", *ip);

// C
int arr[] = {10, 20, 30};
void *vp = arr;
int second = *(int *)((char *)vp + sizeof(int));
printf("%d\n", second);
```

**Prediccion**: Para A, necesitas saber el endianness de tu maquina.
Para B, piensa en como se representan los bits de `0.0` en double.

<details><summary>Respuesta</summary>

**A**: En x86-64 (little-endian), el byte menos significativo de
`0x41424344` es `0x44`, que es el caracter `'D'`. Salida: `D`.

Si fuera big-endian, seria `0x41` = `'A'`.

**B**: `0.0` como `double` IEEE-754 tiene todos los bits en 0. Leido
como `int`, son 4 bytes de ceros. Salida: `0`.

Pero atencion: esto funciona "por casualidad" con 0.0. Con `1.0`, los
bits son `0x3FF0000000000000` y como `int` (primeros 4 bytes en
little-endian) seria `0x00000000` = `0`.

**C**: `(char *)vp + sizeof(int)` avanza exactamente 4 bytes (un `int`).
Castear a `int *` y desreferenciar da `arr[1]`. Salida: `20`.

Este es exactamente el patron que `qsort` usa internamente para indexar
elementos de un array generico.

</details>

---

### Ejercicio 2 — Swap generico

Implementa `generic_swap` usando solo `memcpy` y un buffer temporal:

```c
void generic_swap(void *a, void *b, size_t size);
```

Pruebalo con `int`, `double`, y un struct `Point { int x, y; }`.

**Prediccion**: Si usas un VLA como buffer temporal (`char tmp[size]`),
que pasa si `size` es muy grande (ej. un struct de 1 MB)?

<details><summary>Respuesta</summary>

```c
#include <string.h>
#include <stdlib.h>

void generic_swap(void *a, void *b, size_t size) {
    unsigned char tmp[size];  // VLA en stack
    memcpy(tmp, a, size);
    memcpy(a, b, size);
    memcpy(b, tmp, size);
}

// Uso:
int x = 10, y = 20;
generic_swap(&x, &y, sizeof(int));
// x == 20, y == 10

double d1 = 1.1, d2 = 2.2;
generic_swap(&d1, &d2, sizeof(double));
// d1 == 2.2, d2 == 1.1

typedef struct { int x, y; } Point;
Point p1 = {1, 2}, p2 = {3, 4};
generic_swap(&p1, &p2, sizeof(Point));
// p1 == {3, 4}, p2 == {1, 2}
```

Si `size` es 1 MB, el VLA usa 1 MB de stack. El stack tipico es 8 MB
en Linux — un `size` grande puede causar stack overflow. La alternativa
robusta:

```c
void generic_swap_safe(void *a, void *b, size_t size) {
    if (size <= 256) {
        unsigned char tmp[size];
        memcpy(tmp, a, size);
        memcpy(a, b, size);
        memcpy(b, tmp, size);
    } else {
        void *tmp = malloc(size);
        memcpy(tmp, a, size);
        memcpy(a, b, size);
        memcpy(b, tmp, size);
        free(tmp);
    }
}
```

</details>

---

### Ejercicio 3 — Contenedor generico: stack

Implementa un stack generico usando `void *`:

```c
typedef struct {
    void  *data;
    size_t elem_size;
    size_t count;
    size_t capacity;
} GenStack;

GenStack *genstack_create(size_t elem_size, size_t capacity);
void      genstack_push(GenStack *s, const void *elem);
bool      genstack_pop(GenStack *s, void *out);  // copia al out
void     *genstack_peek(const GenStack *s);       // retorna puntero al top
void      genstack_destroy(GenStack *s);
```

**Prediccion**: `genstack_pop` copia el elemento a `out` en vez de
retornar `void *`. Por que esta decision de diseno? Que problemas
tendria retornar un `void *` directamente?

<details><summary>Respuesta</summary>

```c
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    void  *data;
    size_t elem_size;
    size_t count;
    size_t capacity;
} GenStack;

GenStack *genstack_create(size_t elem_size, size_t capacity) {
    GenStack *s = malloc(sizeof(GenStack));
    s->data = malloc(elem_size * capacity);
    s->elem_size = elem_size;
    s->count = 0;
    s->capacity = capacity;
    return s;
}

void genstack_push(GenStack *s, const void *elem) {
    if (s->count == s->capacity) {
        s->capacity *= 2;
        s->data = realloc(s->data, s->elem_size * s->capacity);
    }
    void *dest = (char *)s->data + s->count * s->elem_size;
    memcpy(dest, elem, s->elem_size);
    s->count++;
}

bool genstack_pop(GenStack *s, void *out) {
    if (s->count == 0) return false;
    s->count--;
    void *src = (char *)s->data + s->count * s->elem_size;
    memcpy(out, src, s->elem_size);
    return true;
}

void *genstack_peek(const GenStack *s) {
    if (s->count == 0) return NULL;
    return (char *)s->data + (s->count - 1) * s->elem_size;
}

void genstack_destroy(GenStack *s) {
    free(s->data);
    free(s);
}

// Uso:
GenStack *ints = genstack_create(sizeof(int), 8);

int v1 = 10, v2 = 20, v3 = 30;
genstack_push(ints, &v1);
genstack_push(ints, &v2);
genstack_push(ints, &v3);

int top;
genstack_pop(ints, &top);  // top == 30
genstack_pop(ints, &top);  // top == 20

genstack_destroy(ints);
```

`genstack_pop` copia a `out` en vez de retornar `void *` porque:

1. **El puntero interno se invalida.** Despues del pop, el proximo push
   sobrescribira esa posicion. Si retornaras un `void *` al interior
   del buffer, se convierte en dangling despues del siguiente push.

2. **Semantica de "ya no pertenece al stack".** Pop significa "quitar
   del stack" — copiar a `out` hace explicita la transferencia.

3. **Consistencia con `realloc`.** Si el stack se realloca, todos los
   punteros internos se invalidan. Copiar a `out` protege contra esto.

En Rust, `Vec::pop` retorna `Option<T>` — mueve el valor fuera del Vec.
El borrow checker impide usar punteros al interior del Vec despues de
una mutacion.

</details>

---

### Ejercicio 4 — for_each generico (no solo ints)

En T01 implementaste `for_each` para arrays de `int`. Ahora generaliza
para arrays de **cualquier tipo**:

```c
typedef void (*GenForEachFn)(const void *elem, void *userdata);

void gen_for_each(const void *array, size_t count, size_t elem_size,
                  GenForEachFn fn, void *userdata);
```

Usalo para: (a) sumar un array de `double`, (b) imprimir un array de
`Student`.

**Prediccion**: Cuantos parametros extra tiene esta version vs la de T01?
Cada parametro extra es informacion que `void *` borro y hay que reponer
manualmente.

<details><summary>Respuesta</summary>

```c
typedef void (*GenForEachFn)(const void *elem, void *userdata);

void gen_for_each(const void *array, size_t count, size_t elem_size,
                  GenForEachFn fn, void *userdata) {
    const char *ptr = (const char *)array;
    for (size_t i = 0; i < count; i++) {
        fn(ptr + i * elem_size, userdata);
    }
}

// (a) Sumar doubles
void sum_double(const void *elem, void *userdata) {
    double value = *(const double *)elem;
    double *sum = (double *)userdata;
    *sum += value;
}

double values[] = {1.1, 2.2, 3.3, 4.4};
double total = 0;
gen_for_each(values, 4, sizeof(double), sum_double, &total);
// total == 11.0

// (b) Imprimir students
typedef struct { char name[32]; int age; } Student;

void print_student(const void *elem, void *userdata) {
    (void)userdata;
    const Student *s = (const Student *)elem;
    printf("%s (age %d)\n", s->name, s->age);
}

Student students[] = {{"Ana", 20}, {"Bob", 22}};
gen_for_each(students, 2, sizeof(Student), print_student, NULL);
```

La version de T01 tenia 4 parametros: `(array, count, fn, userdata)`.
Esta version tiene 5: se agrega `elem_size`. Ese parametro extra
compensa la perdida de informacion de tipo por usar `void *`.

En Rust, la version es simplemente:

```rust
for student in students.iter() {
    println!("{} (age {})", student.name, student.age);
}
```

Sin `elem_size`, sin casts, sin `userdata`. El iterador sabe el tipo.

</details>

---

### Ejercicio 5 — Comparar con Rust: Vec vs GenArray

Dado el `GenArray` de la seccion 5.2, escribe el equivalente en Rust
usando `Vec<T>` y compara linea por linea.

**Prediccion**: Cuantas oportunidades de error tiene la version C que
la version Rust elimina por completo?

<details><summary>Respuesta</summary>

```
  C (GenArray)                         Rust (Vec<T>)
  ─────────────                        ────────────────
  GenArray *arr;                       let mut arr: Vec<i32>;
  arr = genarray_create(               arr = Vec::with_capacity(16);
      sizeof(int), 16);
                                       // No necesita elem_size
  int val = 42;
  genarray_push(arr, &val);            arr.push(42);
                                       // No necesita &, no copia bytes

  int *p = (int *)genarray_get(        let p: &i32 = &arr[0];
      arr, 0);                         // Sin cast, tipo verificado

  genarray_destroy(arr);               // drop automatico al salir de scope
```

Oportunidades de error eliminadas por Rust:

| Error posible en C | Rust |
|---------------------|------|
| `sizeof` incorrecto en `create` | Compilador sabe `size_of::<T>()` |
| Cast a tipo equivocado en `get` | Tipo inferido, verificado |
| Push de tipo equivocado | Error de compilacion |
| Olvidar llamar `destroy` | Drop automatico |
| Indice fuera de rango (UB) | Panic con mensaje claro |
| Use-after-free | Borrow checker lo impide |
| Double free | Ownership unico |
| Shallow copy de datos con punteros | Clone explcito requerido |

Son 8 categorias de error que la version C permite y Rust impide.
Este es el costo real de la genericidad via `void *`.

</details>

---

### Ejercicio 6 — Tagged union vs void*

Reimplementa el GenStack del ejercicio 3 usando una tagged union en vez
de `void *`. Solo necesitas soportar `int`, `double`, y `const char *`.

```c
typedef enum { VAL_INT, VAL_DOUBLE, VAL_STRING } ValType;

typedef struct {
    ValType type;
    union { int i; double d; const char *s; };
} Value;
```

**Prediccion**: Que ganas y que pierdes respecto a la version void*?
Podrias agregar un tipo nuevo sin recompilar?

<details><summary>Respuesta</summary>

```c
typedef enum { VAL_INT, VAL_DOUBLE, VAL_STRING } ValType;

typedef struct {
    ValType type;
    union { int i; double d; const char *s; };
} Value;

typedef struct {
    Value *data;
    size_t count;
    size_t capacity;
} ValueStack;

ValueStack *valuestack_create(size_t capacity) {
    ValueStack *s = malloc(sizeof(ValueStack));
    s->data = malloc(sizeof(Value) * capacity);
    s->count = 0;
    s->capacity = capacity;
    return s;
}

void valuestack_push(ValueStack *s, Value v) {
    if (s->count == s->capacity) {
        s->capacity *= 2;
        s->data = realloc(s->data, sizeof(Value) * s->capacity);
    }
    s->data[s->count++] = v;
}

Value valuestack_pop(ValueStack *s) {
    return s->data[--s->count];
}

void value_print(Value v) {
    switch (v.type) {
        case VAL_INT:    printf("int: %d\n", v.i); break;
        case VAL_DOUBLE: printf("double: %.2f\n", v.d); break;
        case VAL_STRING: printf("string: %s\n", v.s); break;
    }
}

// Uso:
ValueStack *vs = valuestack_create(8);
valuestack_push(vs, (Value){.type = VAL_INT, .i = 42});
valuestack_push(vs, (Value){.type = VAL_DOUBLE, .d = 3.14});
valuestack_push(vs, (Value){.type = VAL_STRING, .s = "hello"});

Value top = valuestack_pop(vs);
value_print(top);  // string: hello
```

**Ganancias vs void*:**
- Puedes inspeccionar el tipo en runtime (`v.type`)
- No necesitas `elem_size` ni `memcpy`
- El switch advierte si olvidas un caso (con `-Wswitch`)
- No hay casts a tipo incorrecto

**Perdidas vs void*:**
- Solo soporta los tipos del enum — no es extensible sin recompilar
- El tamano de `Value` es el del mayor miembro + tag (desperdicia
  espacio si la mayoria son `int` pero el union reserva 8 bytes
  para `double`)
- Agregar un tipo requiere modificar el enum, el union, y todos los
  switch

**Equivalente Rust**: `enum Value { Int(i32), Double(f64), Str(String) }`
con match exhaustivo. Rust combina ambas ventajas: type safety de la
tagged union + extensibilidad via traits.

</details>

---

### Ejercicio 7 — Alignment: detectar problemas

Sin compilar, analiza si cada fragmento tiene problemas de alignment:

```c
// A
char buffer[32];
int *ip = (int *)(buffer + 0);
*ip = 42;

// B
char buffer[32];
int *ip = (int *)(buffer + 1);
*ip = 42;

// C
void *mem = malloc(64);
double *dp = (double *)((char *)mem + 16);
*dp = 3.14;

// D
typedef struct __attribute__((packed)) {
    char c;
    int i;
} Packed;
int *ip = &((Packed *)some_ptr)->i;
*ip = 42;
```

**Prediccion**: Cuales causan undefined behavior por misalignment?
En que arquitecturas se manifesta como crash?

<details><summary>Respuesta</summary>

**A: Probablemente OK.** `buffer + 0` es la direccion de inicio del
array en stack. Los arrays locales suelen estar alineados al menos a
4 bytes en x86-64, pero el estandar no lo garantiza. Formalmente es
UB si la direccion no es multiplo de `alignof(int)`.

**B: UB — misalignment.** `buffer + 1` es casi seguro una direccion
impar, que no es multiplo de `alignof(int) = 4`. En x86-64, funciona
(la CPU tolera accesos desalineados con penalizacion de rendimiento).
En ARM estricto o SPARC, puede causar SIGBUS (crash).

**C: OK.** `malloc` retorna memoria alineada al maximo alignment
fundamental (tipicamente 16 bytes). `16` es multiplo de
`alignof(double) = 8`. Seguro.

**D: UB — misalignment con packed struct.** En un struct `packed`, `i`
puede estar en offset 1 (justo despues de `c`). Tomar un puntero a
`i` y desreferenciarlo viola el alignment de `int`. El compilador
puede generar instrucciones que asumen alignment — crash en ARM,
corrupcion silenciosa posible en x86.

En la practica, x86-64 tolera casi todo, pero el compilador puede
optimizar asumiendo alignment correcto (strict aliasing + alignment),
lo que puede romper el codigo incluso en x86.

</details>

---

### Ejercicio 8 — Documentar ownership

Para cada funcion, escribe un comentario de una linea indicando la
convencion de ownership (`BORROW`, `COPY`, o `TAKE`):

```c
void logger_write(Logger *log, const char *message);

void list_append(List *list, void *node);

char *string_duplicate(const char *src);

void array_sort(void *base, size_t n, size_t size,
                int (*cmp)(const void *, const void *));

void hashmap_insert(HashMap *map, const char *key, void *value);
```

**Prediccion**: Cual es la mas ambigua? Donde un error de ownership
causaria un bug mas dificil de diagnosticar?

<details><summary>Respuesta</summary>

```c
// BORROW: log lee message pero no lo almacena ni libera
void logger_write(Logger *log, const char *message);

// TAKE: la lista toma ownership de node — el llamador NO debe liberar
void list_append(List *list, void *node);

// COPY: retorna una nueva string (malloc) — el llamador debe free()
char *string_duplicate(const char *src);

// BORROW: sort reordena in-place, no toma ownership de los elementos
void array_sort(void *base, size_t n, size_t size,
                int (*cmp)(const void *, const void *));

// AMBIGUA: depende de la implementacion
// Opcion A — COPY: el hashmap copia key y value internamente
// Opcion B — TAKE: el hashmap toma ownership de value (no de key)
// Opcion C — BORROW: el hashmap almacena el puntero sin copiar
void hashmap_insert(HashMap *map, const char *key, void *value);
```

`hashmap_insert` es la mas ambigua. Las tres interpretaciones son
validas segun la implementacion:

- Si COPY: puedes free `value` despues — el hashmap tiene su copia.
- Si TAKE: no hagas free — el hashmap lo libera cuando se destruya.
- Si BORROW: `value` debe vivir tanto como el hashmap — dangling si
  liberas antes.

Un error de ownership en el hashmap es el mas dificil de diagnosticar
porque:
1. El bug no se manifiesta en el insert, sino mucho despues (en la
   lectura o destruccion del hashmap).
2. Double free o use-after-free puede no crashear inmediatamente.
3. Valgrind reporta el free, no el insert incorrecto.

En Rust, el sistema de tipos hace explicita la semantica: `&str` vs
`String` para la key, `T` (move) vs `&T` (borrow) para el value.

</details>

---

### Ejercicio 9 — Encontrar bugs

Cada fragmento tiene un bug relacionado con `void *`. Identificalo:

```c
// Bug A
void *create_int(int val) {
    int x = val;
    return &x;
}
int *p = (int *)create_int(42);
printf("%d\n", *p);

// Bug B
GenArray *arr = genarray_create(sizeof(int), 16);
double d = 3.14;
genarray_push(arr, &d);
int *val = (int *)genarray_get(arr, 0);
printf("%d\n", *val);

// Bug C
void print_all(void *arr, int n) {
    for (int i = 0; i < n; i++) {
        printf("%d\n", *(int *)(arr + i * sizeof(int)));
    }
}
```

**Prediccion**: Cual es crash, cual es datos corruptos, y cual es
error de compilacion?

<details><summary>Respuesta</summary>

**Bug A: Retornar puntero a variable local.**

`x` vive en el stack de `create_int`. Cuando la funcion retorna, el
stack frame se destruye — `p` apunta a memoria invalida. Puede
imprimir 42 "por suerte" o basura.
**Resultado**: datos corruptos o crash (UB — depende del estado del
stack).
**Fix**: usar `malloc`:

```c
void *create_int(int val) {
    int *x = malloc(sizeof(int));
    *x = val;
    return x;  // el llamador debe hacer free
}
```

**Bug B: Tipo incorrecto para el GenArray.**

El array fue creado con `elem_size = sizeof(int) = 4`. Se hace push de
un `double` (8 bytes). `memcpy` solo copia 4 de los 8 bytes del double.
Luego se lee como `int`, obteniendo basura.
**Resultado**: datos corruptos silenciosamente.
**Fix**: crear con `sizeof(double)` o no mezclar tipos.

**Bug C: Aritmetica sobre `void *`.**

`arr + i * sizeof(int)` hace aritmetica sobre `void *`. En C estandar,
esto es un error de compilacion (o warning). GCC lo permite como
extension (tratando `void` como size 1), y en ese caso funciona
accidentalmente — pero no es portable.
**Resultado**: error de compilacion en compiladores estrictos, funciona
en GCC con warning.
**Fix**: `(char *)arr + i * sizeof(int)` o `((int *)arr)[i]`.

</details>

---

### Ejercicio 10 — Reflexion: type erasure

Responde sin ver la respuesta:

1. `void *` borra el tipo en runtime. Que tres piezas de informacion
   se pierden al hacer la conversion `T * → void *`?

2. `qsort` requiere `size_t size` como parametro. Si C tuviera
   generics, seguiria necesitando ese parametro? Y si tuviera solo
   `_Generic`?

3. Rust tiene `Box<dyn Any>` que permite type erasure voluntario
   (similar a `void *`). Por que la mayoria del codigo Rust no lo usa?

**Prediccion**: Piensa las respuestas antes de abrir.

<details><summary>Respuesta</summary>

**1. Tres piezas de informacion que se pierden:**

1. **El tipo concreto**: no sabes si es `int`, `double`, o `Student`.
2. **El tamano**: no sabes cuantos bytes ocupa el dato.
3. **Las operaciones validas**: no sabes si se puede sumar, comparar,
   imprimir, o liberar.

Todo esto se debe reponer manualmente: el tipo via casts correctos,
el tamano via `size_t size`, y las operaciones via function pointers
(callbacks, comparadores). El conjunto de function pointers que repone
las operaciones es exactamente una **vtable** — tema T03.

**2. Generics vs _Generic:**

Con **generics** (como Rust o C++ templates): no se necesita `size_t
size`. El compilador genera una version de `qsort` para cada tipo, y
cada version conoce el tamano en compile time:

```rust
fn qsort<T: Ord>(arr: &mut [T]) {
    // size_of::<T>() es conocido en compilacion
}
```

Con **`_Generic`** (C11): sigue necesitando `size_t size`. `_Generic`
selecciona una funcion por tipo en compile time, pero no genera codigo
generico. Tendrias que escribir una version para cada tipo o seguir
usando `void *` internamente.

**3. Por que Rust evita `dyn Any`:**

`Box<dyn Any>` es el equivalente de `void *` en Rust: borra el tipo y
permite downcast en runtime. La mayoria del codigo Rust no lo usa
porque:

- **Generics son zero-cost**: monomorphization genera codigo
  especializado sin indirecciones.
- **Enums son type-safe**: para un conjunto conocido de variantes, un
  enum con match exhaustivo es superior.
- **`dyn Trait` es mas especifico**: en vez de borrar todo el tipo,
  solo expones las operaciones del trait. Es type erasure **parcial**
  (conserva las operaciones) vs total (`void *` / `dyn Any`).

`void *` en C es type erasure total por necesidad — el lenguaje no
ofrece alternativas. Rust ofrece alternativas (generics, enums, traits)
que preservan mas informacion, y reserva `dyn Any` para los pocos
casos donde realmente necesitas borrar todo.

</details>
