# T01 — Function pointers como estrategia

---

## 1. El problema: polimorfismo sin clases

En C++ o Java, cuando quieres que una misma operacion se comporte de
distintas formas, usas herencia y metodos virtuales:

```
clase base → metodo virtual → la subclase decide que pasa
```

C no tiene clases, ni herencia, ni metodos virtuales. Pero el
**problema** sigue existiendo: necesitas que una funcion haga cosas
distintas segun el contexto, sin reescribirla para cada caso.

La solucion en C es directa: **un puntero a funcion es una variable
que almacena la direccion de una funcion**. Si puedes almacenar la
funcion en una variable, puedes pasarla como argumento, guardarla en
un struct, meterla en un array — y con eso construyes polimorfismo.

```
Sin function pointers:              Con function pointers:

if (tipo == CIRCULO)                shape->area(shape)
    area = pi * r * r;              // la funcion correcta se
else if (tipo == RECT)              // invoca automaticamente
    area = w * h;                   // segun lo que shape apunte
else if (tipo == TRIANG)
    area = b * h / 2;
// cada tipo nuevo = otro else if
```

La cadena de `if/else` crece con cada tipo nuevo. El function pointer
lo resuelve: cada tipo "trae" su propia funcion, y el codigo generico
la invoca sin saber cual es.

---

## 2. Sintaxis del function pointer

### 2.1 Declaracion basica

La sintaxis de un function pointer en C refleja exactamente la firma
de la funcion a la que apunta:

```
tipo_retorno (*nombre)(parametros)
```

El parentesis alrededor de `*nombre` es obligatorio — sin el, el
compilador lee "funcion que retorna un puntero", no "puntero a funcion".

```c
// Funcion normal
int sumar(int a, int b) {
    return a + b;
}

// Puntero a funcion que apunta a sumar
int (*operacion)(int, int) = sumar;

// Invocacion a traves del puntero
int resultado = operacion(3, 4);  // resultado == 7
```

### 2.2 Lectura de la declaracion

Para leer la declaracion, sigue la espiral desde el nombre:

```
int (*operacion)(int, int)
      ^^^^^^^^^^              → "operacion" es...
     *                        → un puntero a...
                 (int, int)   → una funcion que recibe dos int...
int                           → y retorna int.
```

### 2.3 Asignacion

Un function pointer se asigna con el nombre de la funcion — el operador
`&` es opcional:

```c
int sumar(int a, int b);

// Estas dos lineas son equivalentes:
int (*op)(int, int) = sumar;
int (*op)(int, int) = &sumar;
```

De igual forma, al invocar, el operador `*` es opcional:

```c
// Estas dos lineas son equivalentes:
int r = op(3, 4);
int r = (*op)(3, 4);
```

La forma sin `*` y sin `&` es la convencion moderna dominante.

---

## 3. typedef — la clave de la legibilidad

Sin typedef, la sintaxis se vuelve ilegible rapidamente:

```c
// Sin typedef: funcion que recibe un function pointer y retorna otro
int (*(*horrible)(void (*)(int)))(double, double);
// ¿Que es esto? Nadie lo sabe sin detenerse 5 minutos.
```

Con typedef, defines un **nombre de tipo** para la firma:

```c
// Con typedef:
typedef int (*BinaryOp)(int, int);

// Ahora se usa como cualquier tipo:
BinaryOp sumar_ptr = sumar;
BinaryOp restar_ptr = restar;

// En parametros de funcion:
int aplicar(BinaryOp op, int a, int b) {
    return op(a, b);
}
```

### Convencion de nombres

La convencion mas comun: el typedef describe el **rol**, no la firma.

```c
// Bueno — describe que hace
typedef int  (*Comparator)(const void *, const void *);
typedef void (*EventHandler)(int event_type, void *data);
typedef double (*MathFunc)(double);

// Malo — describe la firma (no aporta nada sobre el typedef)
typedef int (*IntIntToInt)(int, int);
```

### typedef con structs

Esto es el uso mas importante para B16: almacenar function pointers
dentro de structs. Es la base del despacho dinamico en C.

```c
typedef double (*AreaFn)(const void *self);
typedef void   (*DrawFn)(const void *self);

typedef struct {
    AreaFn area;
    DrawFn draw;
} ShapeOps;
```

Esta struct `ShapeOps` es, en esencia, una **vtable manual**. El tema
T03 lo desarrolla a fondo; aqui lo mencionamos para mostrar por que
typedef es indispensable.

---

## 4. Pasar function pointers como argumento (Strategy)

El patron Strategy dice: "define una familia de algoritmos, encapsula
cada uno, y hazlos intercambiables". En C, el mecanismo es pasar un
function pointer como argumento.

### 4.1 El ejemplo clasico: qsort

`qsort` de la biblioteca estandar es el ejemplo mas puro de Strategy
en C:

```c
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));
//          ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
//          La estrategia de comparacion se inyecta aqui
```

`qsort` no sabe que esta ordenando — solo sabe invocar `compar`
para decidir el orden. El **llamador** decide la estrategia:

```c
// Estrategia 1: ordenar enteros ascendente
int cmp_int_asc(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

// Estrategia 2: ordenar enteros descendente
int cmp_int_desc(const void *a, const void *b) {
    return *(const int *)b - *(const int *)a;
}

// Estrategia 3: ordenar strings por longitud
int cmp_strlen(const void *a, const void *b) {
    const char *sa = *(const char **)a;
    const char *sb = *(const char **)b;
    return (int)strlen(sa) - (int)strlen(sb);
}

int main(void) {
    int nums[] = {5, 2, 8, 1, 9};

    qsort(nums, 5, sizeof(int), cmp_int_asc);   // {1, 2, 5, 8, 9}
    qsort(nums, 5, sizeof(int), cmp_int_desc);  // {9, 8, 5, 2, 1}
}
```

El mismo algoritmo (`qsort`) con tres comportamientos distintos, sin
modificar una sola linea de `qsort`. Eso es Strategy.

### 4.2 Strategy personalizado

Implementemos un ejemplo mas cercano a un patron real:

```c
typedef double (*PricingStrategy)(double base_price, int quantity);

// Estrategia 1: sin descuento
double no_discount(double price, int qty) {
    return price * qty;
}

// Estrategia 2: 10% de descuento si qty > 10
double bulk_discount(double price, int qty) {
    double total = price * qty;
    return qty > 10 ? total * 0.9 : total;
}

// Estrategia 3: precio fijo por paquete
double flat_rate(double price, int qty) {
    (void)price;
    (void)qty;
    return 99.99;
}

// Funcion generica que usa la estrategia
double calculate_price(PricingStrategy strategy,
                       double price, int qty) {
    return strategy(price, qty);
}

// Uso:
double total = calculate_price(bulk_discount, 15.0, 20);
```

El patron: la funcion `calculate_price` no conoce los detalles del
calculo — solo invoca la estrategia que le inyectaron.

### 4.3 Diagrama del flujo Strategy

```
  Llamador                    Funcion generica          Estrategia
  ────────                    ────────────────          ──────────
     │                              │                       │
     │  calculate_price(            │                       │
     │    bulk_discount,  ─────────▶│                       │
     │    15.0, 20)                 │                       │
     │                              │  strategy(15.0, 20)   │
     │                              │──────────────────────▶│
     │                              │                       │
     │                              │◀─────── 270.0 ───────│
     │◀──────── 270.0 ─────────────│                       │
     │                              │                       │
```

La funcion generica no tiene dependencia directa con ninguna
estrategia concreta. Solo depende de la **firma** del function pointer.

---

## 5. Arrays de function pointers

Un array de function pointers es una **tabla de despacho** (dispatch
table): dado un indice, ejecuta la funcion correspondiente.

### 5.1 Calculadora con dispatch table

```c
typedef double (*MathOp)(double, double);

double add(double a, double b) { return a + b; }
double sub(double a, double b) { return a - b; }
double mul(double a, double b) { return a * b; }
double divide(double a, double b) { return a / b; }

// Tabla de despacho
enum { OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_COUNT };

MathOp dispatch[] = {
    [OP_ADD] = add,
    [OP_SUB] = sub,
    [OP_MUL] = mul,
    [OP_DIV] = divide,
};

// Uso: seleccionar operacion por indice
double result = dispatch[OP_MUL](3.0, 4.0);  // 12.0
```

### 5.2 Comparacion: if/else vs dispatch table

```c
// Con if/else — O(n) ramas, crece con cada operacion nueva
double calc_ifelse(int op, double a, double b) {
    if      (op == OP_ADD) return a + b;
    else if (op == OP_SUB) return a - b;
    else if (op == OP_MUL) return a * b;
    else if (op == OP_DIV) return a / b;
    return 0;
}

// Con dispatch table — O(1), agregar operacion = agregar a la tabla
double calc_dispatch(int op, double a, double b) {
    return dispatch[op](a, b);
}
```

| Aspecto | if/else | Dispatch table |
|---------|---------|----------------|
| Complejidad de seleccion | O(n) ramas | O(1) indexacion |
| Agregar operacion | Modificar la funcion | Agregar al array |
| Verificacion en compile | Completa | El indice puede ser invalido |
| Legibilidad con 5 ops | Aceptable | Mejor |
| Legibilidad con 50 ops | Inmanejable | Clara |

### 5.3 Maquina de estados con dispatch table

Uso real frecuente: cada estado de una FSM tiene su propia funcion.

```c
typedef void (*StateHandler)(int event);

void state_idle(int event);
void state_connecting(int event);
void state_connected(int event);
void state_error(int event);

enum State { IDLE, CONNECTING, CONNECTED, ERROR, STATE_COUNT };

StateHandler handlers[STATE_COUNT] = {
    [IDLE]       = state_idle,
    [CONNECTING] = state_connecting,
    [CONNECTED]  = state_connected,
    [ERROR]      = state_error,
};

// El loop principal es generico:
enum State current = IDLE;

void process_event(int event) {
    handlers[current](event);
    // Cada handler puede cambiar 'current' al siguiente estado
}
```

Este patron se ve en kernels, protocolos de red, parsers, y cualquier
sistema con multiples estados y transiciones.

---

## 6. Function pointers en structs (preludio a vtables)

Cuando almacenas function pointers dentro de un struct, estas creando
la base del despacho dinamico — el mecanismo que en C++ se implementa
con metodos virtuales y en Rust con `dyn Trait`.

```c
typedef struct Logger {
    void (*log)(struct Logger *self, const char *msg);
    void (*close)(struct Logger *self);
} Logger;

// Implementacion 1: log a consola
void console_log(Logger *self, const char *msg) {
    (void)self;
    printf("[CONSOLE] %s\n", msg);
}
void console_close(Logger *self) { (void)self; }

// Implementacion 2: log a archivo
typedef struct {
    Logger base;     // "herencia" manual
    FILE *file;
} FileLogger;

void file_log(Logger *self, const char *msg) {
    FileLogger *fl = (FileLogger *)self;
    fprintf(fl->file, "[FILE] %s\n", msg);
}
void file_close(Logger *self) {
    FileLogger *fl = (FileLogger *)self;
    fclose(fl->file);
}

// Uso polimorfico:
void write_logs(Logger *logger) {
    logger->log(logger, "Sistema iniciado");
    logger->log(logger, "Procesando datos");
    logger->close(logger);
}
```

La funcion `write_logs` trabaja con **cualquier** Logger sin saber
si es consola, archivo, red, o syslog. Esto es exactamente lo que
`dyn Trait` hace en Rust — pero aqui lo construyes a mano.

> **Nota**: Este patron se desarrolla completamente en T03 (Vtable manual)
> y T04 (Opaque pointer). Aqui solo se introduce como motivacion de
> por que los function pointers importan para patrones de diseno.

---

## 7. Errores comunes

### 7.1 Invocar un function pointer NULL

```c
typedef void (*Callback)(int);

Callback on_complete = NULL;
on_complete(42);  // SEGFAULT — undefined behavior
```

Siempre verificar antes de invocar:

```c
if (on_complete) {
    on_complete(42);
}
```

### 7.2 Firma incompatible (sin typedef)

```c
int sumar(int a, int b);

// Error silencioso: la firma no coincide
void (*ptr)(float, float) = (void (*)(float, float))sumar;
ptr(1.0f, 2.0f);  // Comportamiento indefinido: los tipos no coinciden
```

El cast fuerza la compilacion pero los parametros se pasan con el
tipo equivocado. Usar typedef y dejar que el compilador detecte el
error:

```c
typedef int (*BinaryIntOp)(int, int);
BinaryIntOp op = sumar;  // OK, firma coincide
```

### 7.3 Olvidar pasar `self`

En el patron de struct con function pointers, la funcion necesita
acceder a los datos del struct. Si no pasas `self`, la funcion
no tiene contexto:

```c
// MAL: la funcion no sabe sobre que instancia opera
typedef void (*DrawFn)(void);

// BIEN: self da acceso a los datos de la instancia
typedef void (*DrawFn)(const void *self);
```

Este es el equivalente manual del `this` de C++ o del `&self` de Rust.

---

## 8. Function pointers vs alternativas en C

| Mecanismo | Cuando usarlo |
|-----------|--------------|
| Function pointer | Comportamiento decidido en **runtime** |
| Macro / inline | Comportamiento decidido en **compile time** |
| Switch/if-else | Pocas opciones fijas, no extensible |
| Dispatch table | Muchas opciones, seleccion por indice |

En Rust, los function pointers (`fn(i32) -> i32`) existen pero son
menos comunes porque el lenguaje ofrece abstracciones superiores:

| C | Rust equivalente |
|---|-------------------|
| `typedef int (*Cmp)(const void*, const void*)` | `Fn(&T, &T) -> Ordering` (closure trait) |
| Struct con function pointers | `dyn Trait` (trait object) |
| Array de function pointers | `match` o tabla de closures |
| Paso de callback | Closure como argumento generico |

La seccion S02 de este capitulo cubre los mecanismos de Rust en detalle.

---

## Ejercicios

### Ejercicio 1 — Leer declaraciones de function pointers

Sin compilar, indica que describe cada declaracion:

```c
a) void (*f)(int);
b) int (*g)(const char *, ...);
c) double (*h[4])(double);
d) void (*(*k)(int))(double);
```

**Prediccion**: Para cada una, di en palabras: "[nombre] es un [puntero a /
array de] funcion(es) que recibe(n) [...] y retorna(n) [...]".

<details><summary>Respuesta</summary>

| Decl | Lectura |
|------|---------|
| a) | `f` es un puntero a funcion que recibe `int` y retorna `void` |
| b) | `g` es un puntero a funcion variadic que recibe `const char *` (y mas argumentos) y retorna `int` — esta es la firma de `printf` |
| c) | `h` es un array de 4 punteros a funcion, cada uno recibe `double` y retorna `double` — esto es una dispatch table |
| d) | `k` es un puntero a funcion que recibe `int` y retorna un puntero a funcion que recibe `double` y retorna `void` — una "factory" de callbacks |

Para (d), con typedef se leeria:

```c
typedef void (*Action)(double);
Action (*k)(int);  // k recibe int, retorna un Action
```

</details>

---

### Ejercicio 2 — Implementar Strategy: filtro generico

Implementa una funcion `filter` que recibe un array de enteros, su
tamano, un function pointer predicado, y retorna (via parametro de
salida) un nuevo array con solo los elementos que cumplen el predicado.

```c
typedef bool (*Predicate)(int);

int filter(const int *src, int n, Predicate pred,
           int *dest);  // retorna cantidad de elementos filtrados
```

Implementa al menos dos predicados: `is_even` e `is_positive`.

**Prediccion**: Antes de implementar, piensa: el array `dest` debe
tener memoria preasignada — cual es su tamano maximo?

<details><summary>Respuesta</summary>

```c
#include <stdbool.h>

typedef bool (*Predicate)(int);

bool is_even(int x)     { return x % 2 == 0; }
bool is_positive(int x) { return x > 0; }

// dest debe tener espacio para al menos n elementos (peor caso: todos pasan)
int filter(const int *src, int n, Predicate pred, int *dest) {
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (pred(src[i])) {
            dest[count++] = src[i];
        }
    }
    return count;
}

// Uso:
int data[] = {-3, 4, -1, 8, 0, 5, -2};
int result[7];

int m = filter(data, 7, is_even, result);
// result: {4, 8, 0, -2}, m == 4

int p = filter(data, 7, is_positive, result);
// result: {4, 8, 5}, p == 3
```

El tamano maximo de `dest` es `n` — en el peor caso, todos los
elementos cumplen el predicado. Esto es analogo a `Iterator::filter`
en Rust, pero sin closures ni iteradores lazy.

</details>

---

### Ejercicio 3 — Dispatch table: operaciones sobre strings

Crea una dispatch table para operaciones sobre strings usando este enum:

```c
enum StringOp { STR_UPPER, STR_LOWER, STR_REVERSE, STR_OP_COUNT };
```

Cada funcion recibe un `char *` y lo modifica in-place. Luego escribe
una funcion `apply_op(enum StringOp op, char *s)` que use la tabla.

**Prediccion**: Que pasa si alguien pasa un valor de `op` fuera del
rango `[0, STR_OP_COUNT)`? Como lo manejas?

<details><summary>Respuesta</summary>

```c
#include <ctype.h>
#include <string.h>

typedef void (*StringTransform)(char *s);

void str_upper(char *s) {
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}

void str_lower(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

void str_reverse(char *s) {
    size_t len = strlen(s);
    for (size_t i = 0; i < len / 2; i++) {
        char tmp = s[i];
        s[i] = s[len - 1 - i];
        s[len - 1 - i] = tmp;
    }
}

StringTransform string_ops[STR_OP_COUNT] = {
    [STR_UPPER]   = str_upper,
    [STR_LOWER]   = str_lower,
    [STR_REVERSE] = str_reverse,
};

void apply_op(enum StringOp op, char *s) {
    if (op >= 0 && op < STR_OP_COUNT && string_ops[op]) {
        string_ops[op](s);
    }
    // Si op esta fuera de rango, no hace nada (fail-safe)
}

// Uso:
char buf[] = "Hello World";
apply_op(STR_UPPER, buf);    // "HELLO WORLD"
apply_op(STR_REVERSE, buf);  // "DLROW OLLEH"
```

La validacion `op >= 0 && op < STR_OP_COUNT` protege contra indices
invalidos. Tambien verificamos que la entrada en la tabla no sea NULL
por si se agregan enums sin implementacion.

</details>

---

### Ejercicio 4 — Comparadores para qsort

Dado este struct:

```c
typedef struct {
    char name[64];
    int age;
    double gpa;
} Student;
```

Escribe tres comparadores para `qsort`:
1. Ordenar por `age` ascendente
2. Ordenar por `gpa` descendente
3. Ordenar por `name` alfabeticamente

**Prediccion**: `qsort` pasa `const void *` — cuantos casts necesitas
por comparador? Podrias equivocarte en el cast y que compile sin error?

<details><summary>Respuesta</summary>

```c
#include <string.h>

int cmp_by_age_asc(const void *a, const void *b) {
    const Student *sa = (const Student *)a;
    const Student *sb = (const Student *)b;
    return sa->age - sb->age;
}

int cmp_by_gpa_desc(const void *a, const void *b) {
    const Student *sa = (const Student *)a;
    const Student *sb = (const Student *)b;
    // Para double, no usar resta directa (perdida de precision)
    if (sa->gpa < sb->gpa) return  1;   // invertido: mayor primero
    if (sa->gpa > sb->gpa) return -1;
    return 0;
}

int cmp_by_name(const void *a, const void *b) {
    const Student *sa = (const Student *)a;
    const Student *sb = (const Student *)b;
    return strcmp(sa->name, sb->name);
}

// Uso:
Student students[100];
// ... llenar ...
qsort(students, 100, sizeof(Student), cmp_by_age_asc);
qsort(students, 100, sizeof(Student), cmp_by_gpa_desc);
qsort(students, 100, sizeof(Student), cmp_by_name);
```

Cada comparador necesita exactamente 2 casts (uno por argumento). Y si,
podrias castear a un tipo equivocado (`const int *` en vez de
`const Student *`) y compilaria sin error — `const void *` acepta
cualquier cast. Este es uno de los costos de la genericidad via `void *`
que Rust elimina completamente con generics + traits.

**Nota sobre `cmp_by_age_asc`**: la resta `sa->age - sb->age` puede
producir overflow si los valores son extremos (`INT_MAX - INT_MIN`).
Para comparadores robustos, usar la forma con condicionales como en
`cmp_by_gpa_desc`.

</details>

---

### Ejercicio 5 — Callback con contexto (void *userdata)

Muchas APIs en C usan el patron "callback + userdata": la funcion de
callback recibe un `void *` extra que el llamador usa para pasar
contexto arbitrario.

Implementa un `for_each` generico:

```c
typedef void (*ForEachFn)(int element, void *userdata);

void for_each(const int *arr, int n, ForEachFn fn, void *userdata);
```

Usalo para: (a) sumar todos los elementos, (b) encontrar el maximo.

**Prediccion**: Por que se necesita `void *userdata` en vez de simplemente
usar una variable global?

<details><summary>Respuesta</summary>

```c
typedef void (*ForEachFn)(int element, void *userdata);

void for_each(const int *arr, int n, ForEachFn fn, void *userdata) {
    for (int i = 0; i < n; i++) {
        fn(arr[i], userdata);
    }
}

// (a) Sumar todos los elementos
void sum_callback(int elem, void *userdata) {
    int *sum = (int *)userdata;
    *sum += elem;
}

// (b) Encontrar el maximo
void max_callback(int elem, void *userdata) {
    int *max = (int *)userdata;
    if (elem > *max) *max = elem;
}

// Uso:
int data[] = {3, 7, 1, 9, 2};

int total = 0;
for_each(data, 5, sum_callback, &total);
// total == 22

int max = data[0];
for_each(data, 5, max_callback, &max);
// max == 9
```

`void *userdata` se necesita porque sin el, el callback no tiene donde
acumular estado. Una variable global funcionaria para un solo uso, pero
falla si tienes dos hilos llamando `for_each` simultaneamente, o si
quieres dos acumuladores distintos en el mismo programa.

El patron `callback + userdata` es el equivalente manual de un **closure
en Rust**: la closure captura variables del entorno; en C, las pasas
manualmente via `void *`.

| C | Rust |
|---|------|
| `void (*fn)(int, void *), void *userdata` | `impl FnMut(i32)` |
| El contexto se pasa explicitamente | La closure captura automaticamente |
| Sin type safety (void * cast) | Type safe (el compilador verifica) |

</details>

---

### Ejercicio 6 — Construir un mini-framework de tests

Usa function pointers para crear un test runner minimalista:

```c
typedef bool (*TestFn)(void);

typedef struct {
    const char *name;
    TestFn fn;
} TestCase;
```

Implementa `run_tests(TestCase *tests, int n)` que ejecute cada test,
imprima PASS/FAIL, y retorne el numero de tests que fallaron.

**Prediccion**: Si un test causa segfault, que pasa con los demas
tests? Hay forma de protegerse en C?

<details><summary>Respuesta</summary>

```c
#include <stdio.h>
#include <stdbool.h>

typedef bool (*TestFn)(void);

typedef struct {
    const char *name;
    TestFn fn;
} TestCase;

int run_tests(TestCase *tests, int n) {
    int failed = 0;
    for (int i = 0; i < n; i++) {
        bool ok = tests[i].fn();
        printf("[%s] %s\n", ok ? "PASS" : "FAIL", tests[i].name);
        if (!ok) failed++;
    }
    printf("\nResultado: %d/%d pasaron\n", n - failed, n);
    return failed;
}

// Tests de ejemplo
bool test_sum(void) {
    return (2 + 2) == 4;
}

bool test_overflow(void) {
    // Verificar que el compilador no optimiza esto
    int x = __INT_MAX__;
    return (x + 1) < 0;  // true en complemento a dos
}

bool test_should_fail(void) {
    return 1 == 2;  // falla a proposito
}

int main(void) {
    TestCase suite[] = {
        {"suma basica",        test_sum},
        {"overflow detectado", test_overflow},
        {"fallo intencional",  test_should_fail},
    };
    int n = sizeof(suite) / sizeof(suite[0]);
    return run_tests(suite, n);
}
```

Salida:

```
[PASS] suma basica
[PASS] overflow detectado
[FAIL] fallo intencional

Resultado: 2/3 pasaron
```

Si un test causa segfault, el proceso entero muere — los demas tests
no se ejecutan. En C, la proteccion posible es `fork()` + ejecutar
cada test en un proceso hijo, o usar `setjmp/longjmp` para recovery
parcial. Los frameworks reales (Unity, CMocka) implementan estas
protecciones. En Rust, `cargo test` ejecuta cada test en un hilo
separado y captura panics.

</details>

---

### Ejercicio 7 — De if/else a dispatch table

Refactoriza esta funcion que usa una cadena de if/else a una dispatch
table:

```c
void handle_command(const char *cmd, int value) {
    if (strcmp(cmd, "print") == 0)
        printf("%d\n", value);
    else if (strcmp(cmd, "double") == 0)
        printf("%d\n", value * 2);
    else if (strcmp(cmd, "negate") == 0)
        printf("%d\n", -value);
    else if (strcmp(cmd, "square") == 0)
        printf("%d\n", value * value);
    else
        printf("Unknown command: %s\n", cmd);
}
```

**Prediccion**: Un array indexado por enum no funciona aqui (los
comandos son strings). Que estructura necesitas para mapear
string → function pointer?

<details><summary>Respuesta</summary>

```c
typedef void (*CommandHandler)(int value);

void cmd_print(int v)  { printf("%d\n", v); }
void cmd_double(int v) { printf("%d\n", v * 2); }
void cmd_negate(int v) { printf("%d\n", -v); }
void cmd_square(int v) { printf("%d\n", v * v); }

typedef struct {
    const char *name;
    CommandHandler handler;
} Command;

Command commands[] = {
    {"print",  cmd_print},
    {"double", cmd_double},
    {"negate", cmd_negate},
    {"square", cmd_square},
};
int num_commands = sizeof(commands) / sizeof(commands[0]);

void handle_command(const char *cmd, int value) {
    for (int i = 0; i < num_commands; i++) {
        if (strcmp(commands[i].name, cmd) == 0) {
            commands[i].handler(value);
            return;
        }
    }
    printf("Unknown command: %s\n", cmd);
}
```

Como los comandos son strings (no indices numericos), necesitamos un
array de pares `{nombre, handler}` y busqueda lineal. Para muchos
comandos, se podria usar una tabla hash, pero la busqueda lineal es
suficiente para ~10-20 comandos.

Ventajas de la refactorizacion:
- Agregar un comando nuevo = agregar una linea al array + la funcion
- Cada comando esta aislado en su propia funcion (testeable)
- La logica de dispatch (`handle_command`) nunca cambia

</details>

---

### Ejercicio 8 — Composicion de funciones

Implementa una funcion `compose` que recibe dos function pointers
`f` y `g` (ambos `int → int`) y un valor, y retorna `f(g(x))`.

Luego, implementa `pipe` que recibe un **array** de function pointers
y aplica todas en secuencia: `fn[n-1](...fn[1](fn[0](x)))`.

```c
typedef int (*IntTransform)(int);

int compose(IntTransform f, IntTransform g, int x);
int pipe(IntTransform *fns, int n, int x);
```

**Prediccion**: Podrias crear una funcion que retorne una "composicion
permanente" (sin recibir `x`)? Por que si o por que no en C?

<details><summary>Respuesta</summary>

```c
typedef int (*IntTransform)(int);

int compose(IntTransform f, IntTransform g, int x) {
    return f(g(x));
}

int pipe(IntTransform *fns, int n, int x) {
    int result = x;
    for (int i = 0; i < n; i++) {
        result = fns[i](result);
    }
    return result;
}

// Transformaciones:
int double_it(int x)  { return x * 2; }
int add_one(int x)    { return x + 1; }
int square(int x)     { return x * x; }
int negate(int x)     { return -x; }

// Uso:
int r = compose(add_one, double_it, 5);
// double_it(5) = 10, add_one(10) = 11 → r = 11

IntTransform pipeline[] = {double_it, add_one, square, negate};
int s = pipe(pipeline, 4, 3);
// 3 → 6 → 7 → 49 → -49 → s = -49
```

**Sobre composicion permanente**: En C puro, **no** se puede crear una
funcion `compose_permanent(f, g)` que retorne un nuevo function pointer
que internamente llame `f(g(x))`, porque no hay closures — no hay
forma de "capturar" `f` y `g` dentro de una funcion generada
dinamicamente.

En Rust, esto es trivial:

```rust
fn compose(f: impl Fn(i32) -> i32, g: impl Fn(i32) -> i32)
    -> impl Fn(i32) -> i32
{
    move |x| f(g(x))  // closure que captura f y g
}

let double_then_inc = compose(|x| x + 1, |x| x * 2);
let result = double_then_inc(5);  // 11
```

Esta es una de las limitaciones fundamentales de C que Rust resuelve
con closures. En C, la alternativa seria generar codigo en runtime
(JIT) o usar una struct que almacene `f`, `g`, y una funcion
`apply(struct, x)` — que es exactamente lo que la closure de Rust
hace internamente.

</details>

---

### Ejercicio 9 — Encontrar el bug

Cada fragmento tiene un bug relacionado con function pointers.
Identificalo sin compilar:

```c
// Bug A
typedef int (*Comparator)(int, int);
int max(int a, int b) { return a > b ? a : b; }
Comparator cmp = max;
printf("%d\n", cmp(3));  // ???

// Bug B
typedef void (*Callback)(void);
void greet(void) { printf("hello\n"); }
Callback *cb = greet;
cb();  // ???

// Bug C
typedef int (*Op)(int, int);
Op ops[3];
ops[0] = NULL;
printf("%d\n", ops[0](1, 2));  // ???
```

**Prediccion**: Para cada bug, predice si es error de compilacion,
warning, o crash en runtime.

<details><summary>Respuesta</summary>

**Bug A**: `cmp(3)` — faltan argumentos.

`Comparator` espera dos `int`, pero se invoca con uno solo. En C,
esto puede compilar con un warning (los function pointers no siempre
se validan estrictamente dependiendo del compilador y flags). En
runtime, el segundo argumento sera basura del stack.
**Resultado**: warning o compilacion exitosa, comportamiento indefinido
en runtime.

**Bug B**: `Callback *cb` — sobra un `*`.

`Callback` ya es un tipo puntero (`typedef void (*Callback)(void)`).
`Callback *cb` es un puntero a puntero a funcion. La asignacion
`cb = greet` es un type mismatch, y `cb()` intenta usar la direccion
de greet como si fuera un puntero a puntero.
**Resultado**: warning de tipo incompatible. Crash probable.
**Fix**: `Callback cb = greet;`

**Bug C**: Invocar un function pointer NULL.

`ops[0]` es NULL y `ops[0](1, 2)` lo invoca. Esto es dereferenciar
NULL — segfault inmediato en la mayoria de sistemas.
**Resultado**: crash en runtime (SIGSEGV).
**Fix**: `if (ops[0]) printf("%d\n", ops[0](1, 2));`

</details>

---

### Ejercicio 10 — Reflexion: function pointers como base de patrones

Responde sin ver la respuesta:

1. En el patron Strategy de GoF, hay tres roles: Context, Strategy
   (interfaz), y ConcreteStrategy. En el ejemplo de `qsort`, identifica
   quien cumple cada rol.

2. Por que un function pointer es "mas primitivo" que una vtable?
   Que le falta para ser una vtable completa?

3. Si C tuviera closures (como Rust), seguirias necesitando function
   pointers para el patron Strategy? Cuando si y cuando no?

**Prediccion**: Piensa las respuestas antes de abrir.

<details><summary>Respuesta</summary>

**1. Strategy en qsort:**

| Rol GoF | En qsort |
|---------|----------|
| Context | `qsort` — el algoritmo que usa la estrategia |
| Strategy (interfaz) | `int (*compar)(const void *, const void *)` — la firma del callback |
| ConcreteStrategy | `cmp_int_asc`, `cmp_by_name`, etc. — cada funcion concreta |

`qsort` no conoce las implementaciones concretas. Solo depende de la
"interfaz" (la firma del function pointer). Cambiar la estrategia es
cambiar el argumento, no el codigo de `qsort`.

**2. Function pointer vs vtable:**

Un function pointer es **una sola funcion**. Una vtable es una
**coleccion de function pointers** que representan todas las
operaciones de un tipo.

Lo que le falta al function pointer para ser vtable:
- Agrupar multiples operaciones (area, perimeter, draw, destroy...)
- Asociar la tabla con datos de instancia (el `self`)
- Garantizar que todas las operaciones estan implementadas

La vtable (T03) resuelve esto: es un struct de function pointers
asociado a un struct de datos.

**3. Closures vs function pointers:**

Con closures, muchos usos de function pointers se simplifican:
- No necesitas `void *userdata` — la closure captura el contexto
- No necesitas funciones con nombre para cada estrategia trivial
- Puedes crear estrategias inline

Pero seguirias necesitando function pointers (o su equivalente) para:
- APIs estables (ABI de C, FFI) — closures no tienen ABI estable
- Dispatch tables con indices — `fn` pointers son mas ligeros que closures
- Almacenamiento de larga duracion sin allocator — un `fn` pointer es
  solo un `usize`, una closure puede necesitar heap

En Rust, `fn(i32) -> i32` (function pointer) coexiste con
`impl Fn(i32) -> i32` (closure) exactamente por estas razones.

</details>
