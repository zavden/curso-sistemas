# T01 — Declaración y uso de structs

## Erratas detectadas en el material original

| Ubicación | Error | Corrección |
|-----------|-------|------------|
| `labs/functions_arrays.c` línea 51 y `labs/README.md` línea 305 | Dice `"Triangle has %zu vertices:"` con 4 vértices | 4 vértices forman un cuadrilátero, no un triángulo. Debería decir `"Quadrilateral"` o usar solo 3 vértices para un triángulo |

---

## 1. Qué es un struct

Un **struct** agrupa variables de distintos tipos bajo un solo nombre. Es el mecanismo de C para crear **tipos de datos compuestos** (composite types):

```c
#include <stdio.h>

// Declaración: define un NUEVO tipo llamado "struct Point"
struct Point {
    int x;
    int y;
};

int main(void) {
    struct Point p;    // variable de tipo struct Point
    p.x = 10;
    p.y = 20;
    printf("(%d, %d)\n", p.x, p.y);    // (10, 20)
    return 0;
}
```

El **tag** `Point` crea el tipo `struct Point`. En C, la palabra `struct` es **obligatoria** al usar el tipo:

```c
struct Point p;      // OK en C
// Point p;          // ERROR en C (esto es válido en C++, no en C)
```

Los miembros pueden ser de **cualquier tipo** — enteros, doubles, arrays, punteros, otros structs:

```c
struct Person {
    char name[50];     // array de char
    int age;           // int
    double height;     // double
    char email[100];   // otro array
};
```

**Memoria**: los miembros se almacenan en memoria en el **orden de declaración**, con posible **padding** entre ellos para alineación (tema del siguiente tópico T02).

---

## 2. Inicialización: positional vs designated

### Inicialización positional

Asigna valores en el **orden de declaración** de los campos:

```c
struct Point p1 = {10, 20};    // x=10, y=20
```

**Problema**: si alguien reordena los campos del struct, los valores se asignan mal sin que el compilador avise.

### Designated initializers (C99)

Asignan valores **por nombre de campo** con la sintaxis `.campo = valor`:

```c
struct Point p2 = { .x = 10, .y = 20 };

// El orden no importa:
struct Point p3 = { .y = 60, .x = 50 };    // OK: x=50, y=60
```

### Campos omitidos → cero

Los campos **no mencionados** en un designated initializer se inicializan a **cero** automáticamente:

```c
struct Person alice = { .name = "Alice", .age = 30 };
// alice.height == 0.0  (double → 0.0)
// alice.email  == ""   (todos los bytes a cero)
```

### Inicialización completa a cero

```c
struct Person empty = {0};    // TODOS los campos a cero
```

### Por qué preferir designated initializers

```c
// MAL — depende del orden, frágil ante cambios:
struct Person p = {"Bob", 25, 1.80, "bob@mail.com"};

// BIEN — legible, robusto, auto-documenta:
struct Person p = {
    .name   = "Bob",
    .age    = 25,
    .height = 1.80,
    .email  = "bob@mail.com",
};
// La trailing comma después del último campo es válida en C99+.
```

### Variable sin inicializar

Una variable local `struct Point z;` sin inicializar tiene valores **indeterminados** (basura), igual que `int n;` sin inicializar. Siempre inicializar con `{0}` o designated initializers.

---

## 3. Acceso con `.` (punto) y `->` (flecha)

### Operador punto `.`

Accede a miembros de un struct cuando tienes la **variable directa**:

```c
struct Point p = { .x = 10, .y = 20 };
printf("x = %d\n", p.x);    // lectura
p.y = 30;                    // escritura
```

### Operador flecha `->`

Accede a miembros cuando tienes un **puntero** al struct:

```c
struct Point *pp = &p;
printf("x = %d\n", pp->x);    // lectura a través del puntero
pp->y = 40;                    // escritura a través del puntero
```

### `->` es azúcar sintáctico

`pp->x` es exactamente equivalente a `(*pp).x`, pero más legible:

```c
(*pp).x;     // verboso — requiere paréntesis por precedencia
pp->x;       // limpio — preferido universalmente
```

Los paréntesis en `(*pp).x` son necesarios porque `.` tiene mayor precedencia que `*`. Sin ellos, `*pp.x` se interpreta como `*(pp.x)` — error de compilación.

### Regla simple

| Tienes | Usas | Ejemplo |
|--------|------|---------|
| Variable directa `struct Point p` | `.` | `p.x` |
| Puntero `struct Point *p` | `->` | `p->x` |

```c
// Errores comunes:
struct Point p;
// p->x = 10;     // ERROR: p no es un puntero

struct Point *pp;
// pp.x = 10;     // ERROR: pp es un puntero, usar pp->x
```

---

## 4. Copia de structs y shallow copy

### Copia con `=`

A diferencia de los arrays, los structs **sí se pueden copiar** con asignación `=`:

```c
struct Point a = {10, 20};
struct Point b = a;           // copia TODOS los miembros
b.x = 999;
printf("a.x = %d\n", a.x);   // 10 — a NO cambió
printf("b.x = %d\n", b.x);   // 999
```

La copia crea una **copia independiente** de cada campo — modificar `b` no afecta a `a`.

### Shallow copy (copia superficial)

Cuando un campo es un **puntero**, `=` copia la **dirección**, no el contenido apuntado:

```c
struct WithPointer {
    int id;
    char *name;    // puntero — peligro de shallow copy
};

struct WithPointer s1 = { .id = 1, .name = malloc(32) };
strcpy(s1.name, "Alice");

struct WithPointer s2 = s1;    // copia el puntero, NO el string
// s1.name y s2.name apuntan a la MISMA memoria

free(s1.name);
// s2.name es ahora un dangling pointer — usar s2.name es UB
```

**Regla**: si un struct contiene punteros a memoria alocada, necesitas una **deep copy** (copiar también el contenido apuntado):

```c
struct WithPointer deep_copy(const struct WithPointer *src) {
    struct WithPointer copy = *src;
    copy.name = strdup(src->name);    // nueva copia del string
    return copy;
}
```

---

## 5. Comparación de structs

C **no** define `==` para structs:

```c
struct Point a = {10, 20};
struct Point b = {10, 20};
// if (a == b) {}    // ERROR: invalid operands to binary ==
```

### Alternativas

**1. Campo a campo** — directo para structs pequeños:

```c
if (a.x == b.x && a.y == b.y) {
    printf("equal\n");
}
```

**2. Función de comparación** — reutilizable y limpia:

```c
int point_eq(struct Point a, struct Point b) {
    return a.x == b.x && a.y == b.y;
}
```

**3. `memcmp`** — con precaución:

```c
if (memcmp(&a, &b, sizeof(struct Point)) == 0) {
    printf("equal\n");
}
```

**Peligro de `memcmp`**: si el struct tiene **padding** (bytes de relleno entre campos), esos bytes pueden tener valores diferentes aunque los campos sean iguales → **falso negativo**. Solo es seguro cuando puedes garantizar que no hay padding (struct con campos del mismo tipo y tamaño, o inicializado con `{0}` o `memset`).

### Para strings dentro del struct

Si el struct tiene `char name[50]`, usa `strcmp` para comparar ese campo, no `==`:

```c
int person_eq(struct Person a, struct Person b) {
    return strcmp(a.name, b.name) == 0 && a.age == b.age;
}
```

---

## 6. Structs y funciones: paso por valor vs puntero

### Paso por valor (copia)

La función recibe una **copia completa** del struct. No puede modificar el original:

```c
void print_point(struct Point p) {
    printf("(%d, %d)\n", p.x, p.y);
    // p es una copia local — modificarla no afecta al original
}
```

### Paso por puntero (eficiente, permite modificar)

La función recibe la **dirección** del struct. Puede modificar el original:

```c
void scale_rect(struct Rectangle *r, double factor) {
    r->width  *= factor;
    r->height *= factor;
}

struct Rectangle box = { .width = 5.0, .height = 3.0 };
scale_rect(&box, 2.0);
// box.width == 10.0, box.height == 6.0
```

### Paso por puntero `const` (eficiente, solo lectura)

Combina la eficiencia del puntero con la seguridad de no modificar:

```c
double rect_area(const struct Rectangle *r) {
    return r->width * r->height;
    // r->width = 0;    // ERROR: assignment of member in read-only object
}
```

### Cuándo usar cada uno

| Tamaño del struct | Solo lectura | Necesita modificar |
|-------------------|-------------|-------------------|
| Pequeño (≤ 16 bytes) | Por valor `struct T p` | Por puntero `struct T *p` |
| Grande (> 16 bytes) | Por puntero const `const struct T *p` | Por puntero `struct T *p` |

El umbral de ~16 bytes es una guía práctica: un struct `Point` con 2 ints (8 bytes) se pasa eficientemente por valor. Un struct `Person` con 160+ bytes debe pasarse por puntero.

---

## 7. Retorno de structs y compound literals (C99)

### Retornar por valor

Las funciones pueden retornar structs completos por valor:

```c
struct Point make_point(int x, int y) {
    return (struct Point){ .x = x, .y = y };
}

struct Point p = make_point(10, 20);
```

El compilador optimiza la copia con **RVO** (Return Value Optimization) o **NRVO** (Named RVO cuando se retorna una variable local nombrada). Para structs pequeños esto es eficiente — no hay copia real en la práctica.

### Retornar por puntero

Para structs grandes o alocados dinámicamente:

```c
struct Config *config_create(void) {
    struct Config *c = calloc(1, sizeof(struct Config));
    if (!c) return NULL;
    // ... inicializar ...
    return c;    // el caller debe hacer free()
}
```

### Compound literals (C99)

Un **compound literal** crea un struct temporal sin nombrar:

```c
// Crear e inicializar inline:
struct Point p = (struct Point){ .x = 10, .y = 20 };

// Pasar directamente a una función:
print_point((struct Point){ .x = 99, .y = -1 });

// Tomar la dirección de un struct temporal:
struct Point *pp = &(struct Point){ .x = 1, .y = 2 };
// El compound literal vive hasta el final del scope donde se creó.

// Resetear un struct a cero:
p = (struct Point){0};
```

El **lifetime** de un compound literal depende del contexto:
- Dentro de una función: dura hasta el final del **bloque** donde se creó
- Fuera de funciones (file scope): tiene **static storage duration**

---

## 8. Structs anónimos (C11) y forward declarations

### Structs anónimos (C11)

Un struct puede contener **structs o unions anónimos** — sin tag ni nombre de campo. Sus miembros se acceden como si fueran del struct externo:

```c
struct Vector3 {
    union {
        struct { float x, y, z; };     // struct anónimo dentro de union
        float components[3];           // mismo espacio de memoria
    };
};

struct Vector3 v = { .x = 1.0f, .y = 2.0f, .z = 3.0f };
printf("x = %f\n", v.x);              // acceso directo (sin .miembro_intermedio)
printf("y = %f\n", v.components[1]);   // acceso por array — misma memoria
```

Esto es útil para ofrecer **múltiples interfaces** al mismo dato: acceso por nombre (`.x`, `.y`, `.z`) o por índice (`.components[i]`).

### Forward declaration (declaración anticipada)

Se puede declarar un struct sin definirlo, creando un **incomplete type**:

```c
struct Node;    // forward declaration — solo dice que "struct Node existe"
```

Esto permite definir structs con **referencias mutuas**:

```c
struct A {
    struct B *b;    // OK — solo necesita saber que struct B existe
};

struct B {
    struct A *a;    // OK — struct A ya está definido
};
```

Restricciones del incomplete type:
- **Sí** se puede declarar un puntero: `struct Node *np;` — un puntero siempre es 8 bytes
- **No** se puede declarar una variable: `struct Node n;` — error, tamaño desconocido
- **No** se puede acceder a miembros: `np->data` — error hasta que el struct se defina

---

## 9. Flexible array member (C99)

El **último** miembro de un struct puede ser un array **sin tamaño fijo**:

```c
struct Message {
    size_t len;
    char data[];    // flexible array member — tamaño variable
};
```

### Características

- `sizeof(struct Message)` **no incluye** `data[]` — solo cuenta los campos fijos
- Se aloca con espacio extra para el array:

```c
size_t payload = 100;
struct Message *msg = malloc(sizeof(struct Message) + payload);
if (!msg) return NULL;
msg->len = payload;
memcpy(msg->data, "hello", 6);
// msg->data tiene 100 bytes disponibles
```

### Restricciones

- Solo puede ser el **último** miembro del struct
- Solo funciona con **malloc** — no se puede declarar en el stack:
  ```c
  // struct Message m;           // ERROR: incomplete type
  // struct Message m = {0};     // ERROR
  ```
- **No se puede copiar** con `=` — solo se copian los campos fijos, no el array flexible:
  ```c
  struct Message *m2 = malloc(sizeof(struct Message) + msg->len);
  // Hay que usar memcpy para copiar todo:
  memcpy(m2, msg, sizeof(struct Message) + msg->len);
  ```

### Uso típico

Protocolos de red, buffers de tamaño variable, mensajes con payload:

```c
struct Packet {
    uint16_t type;
    uint16_t length;
    char payload[];    // datos de longitud variable
};
```

La ventaja sobre un `char *payload` es que los datos están **contiguos** en memoria con el header — una sola alocación y un solo `free`.

---

## 10. Conexión con Rust

### Structs en Rust

Rust tiene structs similares a C, pero con diferencias importantes:

```rust
struct Point {
    x: i32,
    y: i32,
}

let p = Point { x: 10, y: 20 };    // siempre "designated" — no hay positional
println!("({}, {})", p.x, p.y);
```

### Diferencias clave

| Concepto | C | Rust |
|----------|---|------|
| Declaración de tipo | `struct Point p;` | `let p: Point` (sin `struct` keyword) |
| Inicialización | Positional o designated | Solo por nombre (siempre explícito) |
| Mutabilidad | Mutable por defecto | Inmutable por defecto (`let mut p` para modificar) |
| Copia | Siempre shallow copy con `=` | Solo si implementa `Copy` trait; sino se **mueve** |
| Comparación | No hay `==` | `#[derive(PartialEq)]` genera `==` automáticamente |
| Padding | Definido por ABI pero reordenable por compilador | Compilador reordena campos libremente para optimizar |
| Flexible array | `data[]` en último miembro | No existe — se usa `Vec<u8>` |
| Forward declaration | `struct Node;` | No necesario — el compilador resuelve dependencias |

### Move vs Copy en Rust

En C, `struct B = A;` siempre copia los bytes. En Rust, sin `Copy`, es un **move** — `A` deja de ser válido:

```rust
let a = String::from("hello");
let b = a;     // move — a ya no es válido
// println!("{}", a);    // ERROR: value used after move
```

Esto previene los problemas de shallow copy de C (dangling pointers, double free) a nivel de compilación.

### Derive macros

Rust puede generar automáticamente funcionalidad que en C se escribe a mano:

```rust
#[derive(Debug, Clone, PartialEq)]
struct Point {
    x: i32,
    y: i32,
}

let a = Point { x: 10, y: 20 };
let b = a.clone();           // deep copy explícita
println!("{:?}", a);         // Debug print
assert_eq!(a, b);            // PartialEq permite ==
```

---

## Ejercicios

### Ejercicio 1 — Declaración y acceso básico

```c
// Declara un struct Color con campos:
//   uint8_t r, g, b, a;
//
// En main:
// 1. Crea 'red' con designated initializers: r=255, g=0, b=0, a=255
// 2. Crea 'transparent' inicializado a cero con {0}
// 3. Imprime cada campo de ambos con formato "r=%u g=%u b=%u a=%u"
// 4. Imprime sizeof(struct Color)
//
// Compila: gcc -std=c11 -Wall -Wextra -Wpedantic color.c -o color
```

<details><summary>Predicción</summary>

- `red`: `r=255 g=0 b=0 a=255`
- `transparent`: `r=0 g=0 b=0 a=0` (todo a cero con `{0}`)
- `sizeof(struct Color)` = **4 bytes** (4 × `uint8_t` de 1 byte cada uno, sin padding entre campos del mismo tamaño y alineación de 1)
</details>

### Ejercicio 2 — Positional vs designated

```c
// Declara:
//   struct Config {
//       int width;
//       int height;
//       int fps;
//       int fullscreen;
//   };
//
// 1. Crea 'c1' con inicialización positional: {1920, 1080, 60, 1}
// 2. Crea 'c2' con designated: solo .width=1280, .height=720
// 3. Imprime ambos mostrando los 4 campos
// 4. ¿Qué valores tienen c2.fps y c2.fullscreen?
//
// Compila y ejecuta. Luego agrega un campo 'int vsync;' ENTRE
// height y fps en el struct. Sin cambiar las inicializaciones,
// ¿qué pasa con c1 y c2? Compila con -Wall para ver.
```

<details><summary>Predicción</summary>

- `c2.fps` y `c2.fullscreen` serán **0** (campos omitidos → cero)
- Al agregar `vsync` entre `height` y `fps`:
  - `c1` positional: se rompe silenciosamente — `60` se asigna a `vsync`, `1` a `fps`, y `fullscreen` queda como 0. El compilador no avisa (es sintácticamente válido)
  - `c2` designated: funciona correctamente — `.width` y `.height` se asignan por nombre, y `vsync`, `fps`, `fullscreen` quedan a 0
- Esto demuestra por qué los designated initializers son más robustos
</details>

### Ejercicio 3 — Operador punto vs flecha

```c
// Declara struct Rectangle { double width, height; };
//
// Implementa:
//   void rect_print(const struct Rectangle *r);
//       → imprime "Rectangle(W x H)"
//   void rect_scale(struct Rectangle *r, double factor);
//       → multiplica width y height por factor
//   double rect_area(const struct Rectangle *r);
//       → retorna width * height
//
// En main:
// 1. Crea 'box' con width=5, height=3
// 2. Imprime con rect_print, imprime área con rect_area
// 3. Escala por 2.0 con rect_scale
// 4. Imprime de nuevo y muestra la nueva área
// 5. Intenta acceder con box->width (sin &) — ¿qué error da?
```

<details><summary>Predicción</summary>

- Área inicial: `5.0 × 3.0 = 15.0`
- Después de escalar ×2: `width=10.0, height=6.0`, área = `60.0`
- `box->width` da error de compilación: `box` no es un puntero, se necesita `box.width` o `(&box)->width`
- Error exacto: algo como `error: invalid type argument of '->'`
</details>

### Ejercicio 4 — Copia y shallow copy

```c
// Declara:
//   struct Student {
//       char name[50];
//       int grade;
//   };
//   struct StudentPtr {
//       char *name;    // puntero — peligro
//       int grade;
//   };
//
// 1. Crea 'a' de tipo Student con name="Alice", grade=90
// 2. Copia: Student b = a;
// 3. Modifica b.name[0] = 'X' y b.grade = 50
// 4. Imprime a y b — ¿cambió a.name? ¿cambió a.grade?
//
// 5. Crea 's1' de tipo StudentPtr con name=strdup("Bob"), grade=85
// 6. Copia: StudentPtr s2 = s1;
// 7. Modifica s2.name[0] = 'Z'
// 8. Imprime s1.name y s2.name — ¿cambió s1.name?
// 9. ¿Qué pasa si haces free(s1.name) y luego printf s2.name?
//
// Compila con -fsanitize=address para detectar UAF.
```

<details><summary>Predicción</summary>

- `Student` (array): `b.name[0] = 'X'` **no** afecta a `a.name` porque `char name[50]` se copió completo (50 bytes copiados). `a.name` sigue siendo "Alice"
- `StudentPtr` (puntero): `s2.name[0] = 'Z'` **sí** afecta a `s1.name` porque ambos apuntan a la **misma** memoria. `s1.name` mostrará "Zob"
- Después de `free(s1.name)`: `s2.name` es un dangling pointer → ASan reportará **use-after-free**
- Diferencia clave: `char name[50]` (array incrustado) se copia por valor; `char *name` (puntero) solo copia la dirección
</details>

### Ejercicio 5 — Comparación de structs

```c
// Declara struct Vec2 { float x, y; };
//
// Implementa:
//   int vec2_eq_fields(struct Vec2 a, struct Vec2 b);
//       → compara campo a campo
//   int vec2_eq_memcmp(const struct Vec2 *a, const struct Vec2 *b);
//       → compara con memcmp
//
// En main:
// 1. Crea a = {1.0f, 2.0f} y b = {1.0f, 2.0f}
// 2. Compara con ambas funciones, imprime resultados
// 3. Ahora crea:
//      struct Vec2 c = {0};
//      c.x = 1.0f;
//      c.y = 2.0f;
// 4. Compara a con c usando ambos métodos
// 5. ¿Dan el mismo resultado? ¿Por qué?
//
// Nota: struct Vec2 tiene 2 floats de 4 bytes = 8 bytes total.
// ¿Hay padding? Comprueba con sizeof.
```

<details><summary>Predicción</summary>

- `sizeof(struct Vec2)` = **8 bytes** (2 × 4 bytes, sin padding — `float` tiene alineación de 4 y están contiguos)
- `vec2_eq_fields(a, b)` → **iguales** (1.0f == 1.0f, 2.0f == 2.0f)
- `vec2_eq_memcmp(&a, &b)` → **iguales** (mismos bits, sin padding)
- `vec2_eq_fields(a, c)` → **iguales** (mismos valores)
- `vec2_eq_memcmp(&a, &c)` → **iguales** (sin padding en este struct, y `{0}` inicializó todo a cero antes de asignar)
- En este caso ambos métodos coinciden porque no hay padding. Con un struct que tenga padding (ej. `{char, int}`), `memcmp` podría fallar
</details>

### Ejercicio 6 — Paso por valor vs puntero

```c
// Declara struct Matrix2x2 { double m[2][2]; };
// (sizeof = 32 bytes — 4 doubles de 8 bytes)
//
// Implementa DOS versiones de una función que suma dos matrices:
//   struct Matrix2x2 mat_add_val(struct Matrix2x2 a, struct Matrix2x2 b);
//       → recibe por valor, retorna por valor
//   void mat_add_ptr(const struct Matrix2x2 *a, const struct Matrix2x2 *b,
//                    struct Matrix2x2 *result);
//       → recibe por puntero const, escribe resultado en puntero
//
// Implementa:
//   void mat_print(const struct Matrix2x2 *m);
//
// En main:
// 1. Crea A = {{1,2},{3,4}} y B = {{5,6},{7,8}}
// 2. Suma con mat_add_val, imprime resultado
// 3. Suma con mat_add_ptr, imprime resultado
// 4. Verifica que ambos resultados son iguales
```

<details><summary>Predicción</summary>

- Resultado de A + B: `{{6,8},{10,12}}`
- Ambas versiones producen el mismo resultado
- Diferencia de rendimiento: `mat_add_val` copia 32 bytes por cada parámetro (64 bytes de copia al entrar + 32 al salir), `mat_add_ptr` solo pasa 3 punteros de 8 bytes (24 bytes)
- Para structs de 32 bytes, la versión por puntero es más eficiente
</details>

### Ejercicio 7 — Compound literals

```c
// Declara struct Point { int x, y; };
//
// Implementa:
//   void print_point(struct Point p);
//   double distance(const struct Point *a, const struct Point *b);
//       → retorna sqrt((ax-bx)² + (ay-by)²) — usar #include <math.h>
//
// En main, usa SOLO compound literals (no declares variables con nombre):
// 1. Pasa (struct Point){.x=3, .y=0} y (struct Point){.x=0, .y=4}
//    directamente a print_point
// 2. Calcula la distancia entre esos dos puntos usando compound literals
//    como argumentos de distance — necesitas &(struct Point){...}
// 3. Imprime la distancia (debería ser 5.0 — triángulo 3-4-5)
// 4. Resetea un struct Point p a cero usando p = (struct Point){0};
//
// Compila: gcc -std=c11 -Wall -Wextra -Wpedantic cl.c -o cl -lm
```

<details><summary>Predicción</summary>

- `print_point` mostrará `(3, 0)` y `(0, 4)`
- Distancia = `sqrt(9 + 16)` = `sqrt(25)` = **5.0**
- Los compound literals son temporales válidos dentro del scope — tomar su dirección con `&` es legal
- El flag `-lm` es necesario para `sqrt` (enlazar libmath)
</details>

### Ejercicio 8 — Array de structs con qsort

```c
// Declara:
//   struct Student {
//       char name[50];
//       double avg;
//   };
//
// 1. Crea un array de 5 estudiantes con designated initializers:
//    {"Carlos", 8.5}, {"Ana", 9.2}, {"Luis", 7.8}, {"María", 9.5}, {"Pedro", 8.0}
// 2. Imprime la lista original
// 3. Implementa int cmp_by_avg(const void *a, const void *b)
//    que ordene de MAYOR a MENOR promedio
// 4. Ordena con qsort(students, 5, sizeof(students[0]), cmp_by_avg)
// 5. Imprime la lista ordenada
//
// Pista para cmp_by_avg:
//   const struct Student *sa = (const struct Student *)a;
//   Retorna negativo si sa->avg > sb->avg (para orden descendente)
```

<details><summary>Predicción</summary>

- Orden original: Carlos(8.5), Ana(9.2), Luis(7.8), María(9.5), Pedro(8.0)
- Orden descendente por promedio: María(9.5), Ana(9.2), Carlos(8.5), Pedro(8.0), Luis(7.8)
- En `cmp_by_avg`: para orden descendente, si `sa->avg > sb->avg` retornar -1 (negativo). Cuidado: no hacer `return sb->avg - sa->avg` directamente con doubles (truncamiento al convertir a int). Usar comparaciones explícitas:
  ```c
  if (sa->avg > sb->avg) return -1;
  if (sa->avg < sb->avg) return  1;
  return 0;
  ```
</details>

### Ejercicio 9 — Forward declaration y referencias mutuas

```c
// Modela un sistema donde:
//   struct Department tiene un puntero a su jefe (struct Employee *)
//   struct Employee tiene un puntero a su departamento (struct Department *)
//
// 1. Usa forward declaration para resolver la dependencia circular
// 2. Define ambos structs:
//      struct Department { char name[50]; struct Employee *head; };
//      struct Employee { char name[50]; int id; struct Department *dept; };
// 3. En main, crea en el stack:
//      struct Department eng = { .name = "Engineering" };
//      struct Employee alice = { .name = "Alice", .id = 1 };
// 4. Enlaza: eng.head = &alice; alice.dept = &eng;
// 5. Imprime: "Alice (id=1) heads Engineering"
//    usando solo alice: alice.dept->name para el departamento
//    y eng.head->name para el empleado
//
// ¿Qué error obtienes si eliminas la forward declaration?
```

<details><summary>Predicción</summary>

- Sin forward declaration, al definir `struct Department` el compilador no conoce `struct Employee` → error: `incomplete type` al intentar usar `struct Employee *head` (aunque en realidad, en C, puedes declarar un puntero a un tipo que aún no está definido — el compilador lo acepta como incomplete type implícito). El error real ocurriría si intentas **acceder** a miembros del tipo incompleto antes de su definición
- La navegación cruzada funciona: `alice.dept->name` da `"Engineering"` y `eng.head->name` da `"Alice"` — ambos siguen los punteros a la otra estructura
- Salida: `"Alice (id=1) heads Engineering"`
- Nota: todo está en el stack — no hay `malloc` ni `free` necesarios
</details>

### Ejercicio 10 — Flexible array member

```c
// Declara:
//   struct LogEntry {
//       time_t timestamp;
//       int level;           // 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR
//       char message[];      // flexible array member
//   };
//
// Implementa:
//   struct LogEntry *log_create(int level, const char *msg);
//       → malloc(sizeof(struct LogEntry) + strlen(msg) + 1)
//       → copia msg con strcpy, pone timestamp con time(NULL)
//   void log_print(const struct LogEntry *e);
//       → imprime "[LEVEL] mensaje (timestamp)"
//   void log_free(struct LogEntry *e);
//       → free(e) — un solo free porque todo es contiguo
//
// En main:
// 1. Crea 3 entradas: DEBUG "starting up", INFO "listening on port 8080",
//    ERROR "connection refused"
// 2. Imprime las 3 con log_print
// 3. Imprime sizeof(struct LogEntry) — ¿incluye message[]?
// 4. Libera las 3 entradas
// 5. Verifica con Valgrind: valgrind --leak-check=full ./log
```

<details><summary>Predicción</summary>

- `sizeof(struct LogEntry)` **no** incluye `message[]` — solo cuenta `timestamp` (8 bytes en 64-bit) + `level` (4 bytes) + posible padding = probablemente **16 bytes** (8 + 4 + 4 padding para alineación a 8)
- Cada `log_create` hace **un solo `malloc`** que incluye el header + el string
- Cada `log_free` hace **un solo `free`** — no hay miembros alocados por separado
- Valgrind debe reportar: `All heap blocks were freed -- no leaks are possible`
- Ventaja vs `char *message`: con un puntero serían 2 alocaciones (struct + string) y 2 frees; con flexible array member es 1 de cada uno, y los datos están contiguos en memoria (mejor cache locality)
</details>
