# T04 — Puntero `void*`

> **Fuente**: `README.md`, `LABS.md`, `labs/README.md`, `void_basic.c`, `void_deref.c`, `swap_generic.c`, `void_stdlib.c`
>
> Sin erratas detectadas en el material fuente.

---

## 1. Qué es `void*` — el puntero genérico

`void *` es un puntero que almacena una dirección de memoria **sin tipo asociado**. No sabe a qué tipo de dato apunta — solo guarda la dirección:

```c
int x = 42;
double d = 3.14;
char c = 'A';

void *vp;
vp = &x;     // OK — int* → void*
vp = &d;     // OK — double* → void*
vp = &c;     // OK — char* → void*
```

Todos los punteros en x86-64 ocupan 8 bytes, independientemente del tipo al que apunten. El tipo no afecta el tamaño del puntero — afecta **cómo se interpreta** el dato al desreferenciar:

| Puntero | `sizeof` | Bytes leídos al desreferenciar | Interpretación |
|---------|----------|-------------------------------|----------------|
| `int *` | 8 | 4 | Entero complemento a 2 |
| `double *` | 8 | 8 | IEEE 754 |
| `char *` | 8 | 1 | Carácter ASCII/UTF-8 |
| `void *` | 8 | **No se puede** | **No tiene tipo** |

La diferencia clave: un `int *` dice "aquí hay 4 bytes que son un entero". Un `void *` dice "aquí hay algo, pero no sé qué".

---

## 2. Conversiones implícitas — C vs C++ y `malloc`

En C, la conversión entre `void *` y cualquier otro tipo de puntero es **implícita** en ambas direcciones:

```c
int *ip = &x;
void *vp = ip;      // int* → void* — implícito
int *ip2 = vp;      // void* → int* — implícito en C
```

No se necesita cast. Esto es **diferente en C++**, donde `void* → tipo*` requiere cast explícito.

Esta diferencia explica por qué **no se castea `malloc` en C**:

```c
// Correcto en C — conversión implícita void* → int*:
int *arr = malloc(10 * sizeof(int));

// Innecesario (y contraproducente):
int *arr = (int *)malloc(10 * sizeof(int));
```

El cast es contraproducente porque:

1. Si olvidas `#include <stdlib.h>`, en compiladores pre-C99 el compilador asume que `malloc` retorna `int`. Sin cast, obtienes un error de tipos que te avisa del olvido. Con cast, el error se oculta y el programa compila con un bug silencioso.
2. En C99+ la declaración implícita de funciones es un error, así que este problema desaparece. Pero el cast sigue siendo ruido innecesario.

**Regla**: en C, nunca castear el retorno de `malloc`. Si necesitas que tu código compile como C++, estás escribiendo C++ — usa `new` o `static_cast`.

---

## 3. Restricciones: no desreferenciar, no aritmética

### No se puede desreferenciar

```c
void *vp = &some_int;

// *vp;      // ERROR de compilación
// vp[0];    // ERROR — mismo problema
```

El compilador rechaza `*vp` porque no sabe cuántos bytes leer ni cómo interpretarlos. Para acceder al dato, hay que convertir a un tipo concreto:

```c
// Opción 1: variable intermedia
int *ip = vp;
printf("%d\n", *ip);

// Opción 2: cast inline
printf("%d\n", *(int *)vp);
```

### No se puede hacer aritmética

```c
void *vp = arr;
// vp + 1;   // NO ESTÁNDAR
```

La aritmética de punteros necesita saber el tamaño del tipo para calcular el stride. `void` no tiene tamaño.

**GCC lo acepta como extensión** (tratando `sizeof(void)` como 1, es decir `vp + 1` avanza 1 byte), pero esto:
- No es estándar C
- Genera warnings con `-Wpedantic`
- No es portable a otros compiladores

Para avanzar en un array a través de `void *`, convertir primero:

```c
// Para avanzar al siguiente int:
int *ip = (int *)vp + 1;

// Para avanzar n bytes (acceso byte a byte):
unsigned char *bp = (unsigned char *)vp + n;
```

---

## 4. Funciones genéricas: `swap_generic` con `memcpy`

El uso principal de `void *` es escribir funciones que operen con **cualquier tipo**. El precio: hay que pasar el tamaño como parámetro, porque `void *` no contiene esa información.

```c
void swap_generic(void *a, void *b, size_t size) {
    unsigned char tmp[size];    // VLA como buffer temporal
    memcpy(tmp, a, size);       // copia byte a byte — funciona con cualquier tipo
    memcpy(a, b, size);
    memcpy(b, tmp, size);
}
```

La misma función funciona con `int`, `double`, `struct` — cualquier tipo:

```c
int x = 10, y = 20;
swap_generic(&x, &y, sizeof(int));       // x=20, y=10

double a = 1.1, b = 2.2;
swap_generic(&a, &b, sizeof(double));    // a=2.2, b=1.1

struct Point p1 = {1, 2}, p2 = {3, 4};
swap_generic(&p1, &p2, sizeof(struct Point));  // p1={3,4}, p2={1,2}
```

### Peligro: `size` incorrecto

```c
double a = 1.1, b = 2.2;
swap_generic(&a, &b, sizeof(int));   // BUG: copia 4 de 8 bytes
// Resultado: datos corrompidos
```

Con `sizeof(int)` (4 bytes) para un `double` (8 bytes), `memcpy` copia solo la mitad. El compilador **no puede detectar** este error — el programador es responsable de pasar el tamaño correcto. Este es el precio de la genericidad con `void *`.

---

## 5. `qsort` — genericidad en la biblioteca estándar

`qsort` es el ejemplo canónico de función genérica con `void *`:

```c
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));
```

Los cuatro parámetros corresponden a lo que `void *` no sabe:
- `base`: dirección del array (como `void *`)
- `nmemb`: cuántos elementos
- `size`: cuántos bytes ocupa cada elemento
- `compar`: cómo comparar dos elementos

### Patrón del comparador

Todos los comparadores siguen el mismo patrón:

```c
int cmp_int(const void *a, const void *b) {
    int ia = *(const int *)a;    // 1. cast al tipo real
    int ib = *(const int *)b;
    return (ia > ib) - (ia < ib); // 2. retornar: <0, 0, >0
}
```

El patrón `(a > b) - (a < b)` retorna -1, 0 o 1 **sin riesgo de overflow**. La alternativa `a - b` parece más simple, pero causa overflow con valores extremos:

```c
// PELIGROSO — overflow con valores grandes:
int cmp_bad(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
    // 2000000000 - (-2000000000) = 4000000000 → overflow → UB
}
```

### Comparadores para structs

```c
struct Student {
    char name[50];
    int grade;
};

int cmp_by_grade(const void *a, const void *b) {
    const struct Student *sa = a;   // cast implícito — válido en C
    const struct Student *sb = b;
    return (sa->grade > sb->grade) - (sa->grade < sb->grade);
}

int cmp_by_name(const void *a, const void *b) {
    const struct Student *sa = a;
    const struct Student *sb = b;
    return strcmp(sa->name, sb->name);   // strcmp ya retorna el formato correcto
}
```

El poder de este diseño: el mismo array se ordena de formas diferentes simplemente cambiando el comparador:

```c
qsort(students, n, sizeof(struct Student), cmp_by_grade);  // por nota
qsort(students, n, sizeof(struct Student), cmp_by_name);   // por nombre
```

`qsort` ordena **in-place** — modifica el array original, no crea una copia.

---

## 6. `bsearch` — búsqueda binaria genérica

`bsearch` usa la misma firma genérica que `qsort`:

```c
void *bsearch(const void *key, const void *base,
              size_t nmemb, size_t size,
              int (*compar)(const void *, const void *));
```

Requisito: el array **debe estar ordenado** (con el mismo criterio que el comparador).

```c
int sorted[] = {1, 2, 5, 8, 9};
int key = 5;
int *found = bsearch(&key, sorted, 5, sizeof(int), cmp_int);

if (found) {
    printf("Found %d at index %td\n", *found, found - sorted);
} else {
    printf("Not found\n");
}
```

Nota clave: `bsearch` retorna `void *` — un puntero al elemento encontrado (dentro del array original) o `NULL` si no existe. Se usa el mismo comparador que para `qsort`, haciendo las dos funciones intercambiables.

---

## 7. Patrón "user data" para callbacks

Otro uso fundamental de `void *`: pasar **datos de contexto** a funciones callback:

```c
typedef void (*Callback)(int event, void *user_data);

void register_callback(Callback cb, void *user_data) {
    // Cuando ocurre el evento:
    cb(42, user_data);
}
```

El callback recibe sus datos a través del `void *` y los convierte al tipo correcto:

```c
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
}
```

Este patrón aparece en todo C: `pthread_create`, señales, event loops, GUI toolkits. El `void *user_data` permite que cada callback tenga su propio estado sin variables globales.

---

## 8. Strict aliasing y `void*`

La **strict aliasing rule** dice que acceder a un objeto a través de un puntero de tipo incompatible es **comportamiento indefinido**:

```c
int x = 42;
float *fp = (float *)&x;
float f = *fp;              // UB — strict aliasing violation
```

Excepciones — siempre se puede acceder como:
1. El tipo real del objeto (o su versión `signed`/`unsigned`)
2. `char *` o `unsigned char *` (acceso byte a byte — siempre legal)

`void *` **no viola aliasing por sí mismo** porque no se desreferencia. La violación ocurre al convertir a un tipo **incorrecto** y desreferenciar:

```c
int x = 42;
void *vp = &x;

int *good = vp;
*good;              // OK — x es int, accedemos como int

float *bad = vp;
*bad;               // UB — x es int, no float
```

### `char *` como acceso universal a bytes

`char *` y `unsigned char *` pueden leer bytes de **cualquier** objeto:

```c
int x = 0x04030201;
unsigned char *bytes = (unsigned char *)&x;
for (int i = 0; i < (int)sizeof(x); i++) {
    printf("byte[%d] = 0x%02x\n", i, bytes[i]);
}
// Legal y portátil (excepto por endianness)
```

Por eso `memcpy` y `memset` funcionan con `void *` internamente — usan `char *` para acceder byte a byte, lo cual es siempre legal.

---

## 9. Type safety: cast correcto vs incorrecto

`void *` no tiene **type safety** — el compilador no puede verificar que el cast sea correcto:

```c
// SEGURO — conversión al tipo original:
int x = 42;
void *vp = &x;
int *ip = vp;
printf("%d\n", *ip);    // 42 — correcto

// PELIGROSO — conversión a tipo incorrecto:
double *dp = vp;
printf("%f\n", *dp);    // UB — lee 8 bytes de un int de 4
```

El cast incorrecto produce basura (o peor) porque:
1. `*(double *)vp` lee 8 bytes, pero `x` solo ocupa 4. Los 4 bytes extra son basura del stack.
2. Incluso si leyera solo 4 bytes, la representación binaria de un `int` no es un `double` válido.
3. Es UB por strict aliasing — el compilador puede optimizar asumiendo que no ocurre.

**No hay mecanismo en C para verificar qué tipo almacena un `void *`** en tiempo de ejecución. El programador es responsable de recordar el tipo. Este es el trade-off fundamental: `void *` da genericidad a costa de type safety.

---

## 10. Comparación: `void*` vs `_Generic` vs generics de Rust

### C: `void *` — polimorfismo en runtime

```c
void *container[10];
container[0] = &some_int;
container[1] = &some_string;    // compila — sin verificación
// El programador debe saber qué hay en cada posición
```

Características: flexible, peligroso, cero type safety, overhead mínimo (solo indirección).

### C11: `_Generic` — dispatch por tipo en compilación

```c
#define print_val(x) _Generic((x), \
    int:    printf("%d\n", x),     \
    double: printf("%f\n", x),     \
    char *: printf("%s\n", x)      \
)
```

`_Generic` selecciona una expresión según el tipo de la expresión de control, **en tiempo de compilación**. No es genericidad completa — es un switch de tipos estático, útil para sobrecargar macros.

### Rust: generics con monomorphization

```rust
fn swap<T>(a: &mut T, b: &mut T) {
    std::mem::swap(a, b);
}
```

El compilador genera una versión especializada para cada tipo usado (`swap::<i32>`, `swap::<f64>`, etc.). Type-safe en compilación, zero-cost en runtime (sin indirección, sin `void *`, sin `size` manual). Si el tipo es incorrecto, el compilador rechaza el código — imposible tener bugs de cast.

| Mecanismo | Type safety | Runtime cost | Flexibilidad |
|-----------|-------------|--------------|--------------|
| `void *` | Ninguna | Mínimo (indirección) | Máxima — cualquier tipo |
| `_Generic` | Compilación | Zero-cost | Limitada — tipos listados |
| Rust generics | Compilación | Zero-cost | Alta — traits como bounds |

---

## Ejercicios

### Ejercicio 1 — Roundtrip de tipos

```c
// Declara variables int, double, char, y un array int[3].
// Asigna cada dirección a un void* e imprímela con %p.
// Haz roundtrip void* → tipo_original* y verifica que el valor se preserva.
// ¿El roundtrip funciona para el array? ¿Qué tipo usas para recuperarlo?
```

<details><summary>Predicción</summary>

Todas las asignaciones a `void *` compilarán sin warnings ni casts. Cada roundtrip preservará la dirección y el valor. Para el array, `void *vp = arr` funciona (el array decae a `int *`), y se recupera con `int *recovered = vp`. El puntero recuperado apunta al primer elemento.

</details>

### Ejercicio 2 — `sizeof` de punteros

```c
// Imprime sizeof(void*), sizeof(int*), sizeof(double*), sizeof(char*),
// sizeof(struct Point*) donde Point tiene dos ints.
// Imprime también sizeof(void**) y sizeof(int***).
// ¿Alguno es diferente de 8?
```

<details><summary>Predicción</summary>

Todos imprimen 8 en x86-64. Un puntero es una dirección de memoria (64 bits = 8 bytes) independientemente de a qué apunte — el tipo afecta la interpretación, no el tamaño del puntero.

</details>

### Ejercicio 3 — Desreferenciar void* (errores)

```c
// Crea un programa que:
// a) Intente compilar *vp donde vp es void* — observa el error
// b) Intente compilar vp[0] — observa el error
// c) Intente compilar vp + 1 con -Wpedantic — observa el warning
// d) Corrígelos: usa (int*)vp para desreferenciar, (int*)vp + 1 para aritmética
```

<details><summary>Predicción</summary>

(a) y (b) producen `error: dereferencing 'void *' pointer`. (c) produce `warning: pointer of type 'void *' used in arithmetic` con `-Wpedantic` (GCC lo acepta como extensión, sumando 1 byte). (d) compila limpiamente — el cast proporciona la información de tipo que el compilador necesita.

</details>

### Ejercicio 4 — `swap_generic` con tamaño correcto e incorrecto

```c
// Usa swap_generic para intercambiar:
// a) Dos long long (sizeof = 8)
// b) Dos char (sizeof = 1)
// c) Dos doubles pasando sizeof(int) — ¿qué ocurre?
// d) Dos ints pasando sizeof(double) — ¿qué ocurre?
// Imprime antes y después de cada swap.
```

<details><summary>Predicción</summary>

(a) y (b) funcionan correctamente con `sizeof(long long)` y `sizeof(char)`. (c) Con `sizeof(int)` = 4 para un `double` de 8 bytes, `memcpy` copia solo la mitad — los doubles quedan corrompidos. (d) Con `sizeof(double)` = 8 para un `int` de 4 bytes, `memcpy` lee/escribe 4 bytes extra **fuera** de las variables — corrupción de stack, comportamiento indefinido.

</details>

### Ejercicio 5 — Comparador para `qsort`

```c
// Define un struct Product { char name[32]; double price; int stock; }.
// Crea un array de 5 productos.
// Escribe tres comparadores: cmp_by_price, cmp_by_stock, cmp_by_name.
// Ordena e imprime el array con cada comparador.
// Usa el patrón (a > b) - (a < b), NO la resta a - b.
```

<details><summary>Predicción</summary>

Los tres comparadores siguen el mismo patrón: cast `const void *` → `const struct Product *`, acceder al campo relevante, retornar con el patrón seguro. `cmp_by_name` puede delegar a `strcmp` directamente. El mismo array se reordena de tres formas distintas. `qsort` modifica in-place.

</details>

### Ejercicio 6 — `bsearch` con structs

```c
// Ordena el array de Product por precio con qsort.
// Usa bsearch para buscar un producto por precio exacto.
// Busca un precio que existe y uno que no.
// Pregunta: ¿puedes usar bsearch para buscar por nombre si el array
// está ordenado por precio? ¿Por qué?
```

<details><summary>Predicción</summary>

`bsearch` con el precio existente retorna un `void *` no-NULL que se puede castear a `struct Product *`. Con un precio inexistente retorna `NULL`. **No** se puede buscar por nombre si el array está ordenado por precio — `bsearch` requiere que el array esté ordenado con el **mismo** criterio que el comparador usado para la búsqueda. Si se ordena por precio pero se busca por nombre, la búsqueda binaria salta a posiciones incorrectas.

</details>

### Ejercicio 7 — Callback con user data

```c
// Implementa un sistema simple de eventos:
// - typedef void (*EventHandler)(int event_id, void *ctx);
// - void dispatch(EventHandler handler, int event_id, void *ctx);
// Crea dos structs diferentes como contexto (Logger con prefix y Counter con count).
// Crea un handler para cada uno que castee el void* al tipo correcto.
// Despacha varios eventos y verifica que cada handler modifica su propio contexto.
```

<details><summary>Predicción</summary>

`dispatch` simplemente llama `handler(event_id, ctx)`. Cada handler castea `void *ctx` a su struct: `struct Logger *log = ctx` y `struct Counter *ctr = ctx`. El cast implícito funciona sin warnings en C. Cada handler modifica su propia copia del estado (incrementa counter, imprime con prefix). No hay interferencia entre los dos handlers porque cada uno tiene su propio `void *ctx`.

</details>

### Ejercicio 8 — `linear_search` genérica

```c
// Implementa:
// void *linear_search(const void *key, const void *base,
//                     size_t nmemb, size_t size,
//                     int (*cmp)(const void *, const void *));
// Recorre el array elemento a elemento, comparando con cmp.
// Retorna puntero al primer match o NULL.
// Pista: para avanzar al siguiente elemento, usa (const char *)base + i * size.
// Prueba con un array de ints y un array de strings (char*).
```

<details><summary>Predicción</summary>

El truco clave: como `void *` no permite aritmética, hay que castear a `const char *` para avanzar `i * size` bytes. La función itera `i` de 0 a `nmemb-1`, calcula la dirección del elemento `i` como `(const char *)base + i * size`, y compara con `cmp(key, elem_addr)`. Si `cmp` retorna 0, retorna el puntero al elemento (casteando away el `const` del retorno, igual que `bsearch`). Si no encuentra nada, retorna `NULL`.

</details>

### Ejercicio 9 — `print_array` genérica

```c
// Implementa:
// void print_array(const void *base, size_t nmemb, size_t size,
//                  void (*print_elem)(const void *));
// Crea funciones print_int, print_double, print_string que impriman un elemento.
// Imprime arrays de cada tipo con la misma función print_array.
// Nota: print_string recibe const void* que apunta a un char*
//       (el array es char*[], cada elemento es un char*).
```

<details><summary>Predicción</summary>

`print_array` itera con `(const char *)base + i * size` para obtener la dirección de cada elemento, y llama `print_elem(elem_addr)`. `print_int` castea `const void *` → `const int *` y desreferencia. `print_double` → `const double *`. Para strings: el array es `char *arr[]`, cada elemento es un `char *`. `print_string` recibe un puntero **al** puntero: `const void *` apunta a un `char *`, así que se castea a `const char * const *` y se desreferencia una vez para obtener el string: `printf("%s", *(const char * const *)elem)`.

</details>

### Ejercicio 10 — Cast incorrecto y strict aliasing

```c
// a) Almacena un int x = 0x41424344 y léelo como char* byte a byte.
//    ¿Qué letras imprime en little-endian? ¿Es legal por aliasing?
// b) Almacena un float f = 3.14f y léelo como int* con *(int *)&f.
//    ¿Es UB? ¿Qué alternativa legal existe para inspeccionar los bits?
// c) Implementa la alternativa legal del punto (b) usando memcpy a un int
//    e imprime el resultado en hexadecimal. Compara con el valor UB.
```

<details><summary>Predicción</summary>

(a) `0x41424344` en little-endian se almacena como bytes `44 43 42 41`, que en ASCII son `D C B A`. Imprime "DCBA". Es **legal** — `char *` puede acceder a cualquier objeto (excepción de aliasing). (b) `*(int *)&f` es **UB** — viola strict aliasing (acceder a un `float` como `int`). Puede funcionar en práctica pero el compilador puede optimizarlo incorrectamente con `-O2`. (c) La alternativa legal: `int bits; memcpy(&bits, &f, sizeof(bits));` — `memcpy` no viola aliasing porque opera byte a byte. El valor hexadecimal de 3.14f en IEEE 754 es `0x4048F5C3`. Ambos métodos probablemente den el mismo resultado, pero solo el de `memcpy` es portátil y legal.

</details>
