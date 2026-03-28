# T03 — Macros peligrosas

## Evaluación múltiple

El problema más común de las macros: los argumentos se
**evalúan cada vez que aparecen** en la expansión:

```c
#define MAX(a, b) ((a) > (b) ? (a) : (b))

int x = 5, y = 3;
int m = MAX(x++, y++);
// Se expande a: ((x++) > (y++) ? (x++) : (y++))
// x++ se evalúa DOS veces si x > y:
//   Evaluación 1: (5) > (3) → true, x=6, y=4
//   Evaluación 2: (x++) → 6, x=7
// Resultado: m=6, x=7, y=4
// ¡x se incrementó DOS veces!

// Con una función no hay problema:
static inline int max(int a, int b) { return a > b ? a : b; }
int m2 = max(x++, y++);
// x++ se evalúa UNA sola vez antes de llamar a max.
```

```c
// Otro ejemplo: función con side effects:
#define SQUARE(x) ((x) * (x))

int a = SQUARE(expensive_function());
// Se expande a: ((expensive_function()) * (expensive_function()))
// La función se llama DOS veces.
// Si retorna valores diferentes cada vez → resultado incorrecto.

// Solución con GCC extension — statement expression:
#define SQUARE(x) ({ \
    typeof(x) _x = (x); \
    _x * _x; \
})
// x se evalúa UNA sola vez y se guarda en _x.
// No es estándar C pero funciona en GCC y Clang.
```

## Side effects en argumentos

```c
// REGLA: nunca pasar expresiones con side effects a macros.
// Side effects: ++, --, función que modifica estado, I/O

// MAL:
#define ABS(x) ((x) >= 0 ? (x) : -(x))
int a = ABS(read_sensor());    // lee el sensor 2 veces
int b = ABS(i++);              // i se incrementa 2 veces

// BIEN: guardar en variable primero:
int val = read_sensor();
int a = ABS(val);

int temp = i;
int b = ABS(temp);
i++;

// O mejor: usar una función inline:
static inline int abs_val(int x) { return x >= 0 ? x : -x; }
```

## El patrón do { ... } while (0)

Las macros que contienen múltiples sentencias deben envolverse
en `do { ... } while (0)`:

```c
// MAL — sin do-while:
#define LOG(msg) printf("[LOG] "); printf("%s\n", msg);

if (error)
    LOG("something failed");
else
    do_something();

// Se expande a:
// if (error)
//     printf("[LOG] ");
// printf("%s\n", msg);    ← FUERA del if — siempre se ejecuta
// else                    ← error: else sin if
//     do_something();
```

```c
// MAL — con llaves solas:
#define LOG(msg) { printf("[LOG] "); printf("%s\n", msg); }

if (error)
    LOG("failed");    // → { printf...; printf...; };
else                  // error: ; antes de else
    do_something();

// El ; después de LOG("failed") cierra el if.
// El else no tiene un if asociado.
```

```c
// BIEN — do { ... } while (0):
#define LOG(msg) do { \
    printf("[LOG] "); \
    printf("%s\n", msg); \
} while (0)

if (error)
    LOG("failed");    // → do { printf...; printf...; } while (0);
else                  // OK — el ; es parte del do-while
    do_something();

// do { ... } while (0) es una SENTENCIA que:
// 1. Se puede usar donde va una sentencia
// 2. Requiere ; al final (como cualquier sentencia)
// 3. Se ejecuta exactamente una vez
// 4. No interfiere con if/else
```

```c
// Todos estos macros tipo "statement" deben usar do-while(0):

#define SAFE_FREE(p) do { free(p); (p) = NULL; } while (0)

#define SWAP(a, b) do { \
    typeof(a) _tmp = (a); \
    (a) = (b); \
    (b) = _tmp; \
} while (0)

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Check failed: %s at %s:%d\n", \
                #cond, __FILE__, __LINE__); \
        return -1; \
    } \
} while (0)
```

## Variables temporales en macros

```c
// Si un macro usa variables temporales, pueden colisionar
// con variables del caller:

#define SWAP(a, b) do { \
    int tmp = (a); \
    (a) = (b); \
    (b) = tmp; \
} while (0)

int tmp = 10, x = 20;
SWAP(tmp, x);
// Se expande a:
// do {
//     int tmp = (tmp);    ← tmp se refiere a sí mismo, no al tmp externo
//     (tmp) = (x);
//     (x) = tmp;
// } while (0)
// Resultado: incorrecto — el tmp interno shadowing el externo.
```

```c
// Soluciones:

// 1. Prefijo con underscore (convención):
#define SWAP(a, b) do { \
    typeof(a) _swap_tmp = (a); \
    (a) = (b); \
    (b) = _swap_tmp; \
} while (0)

// 2. Usar __LINE__ para nombre único:
#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)
#define UNIQUE_NAME(prefix) CONCAT(prefix, __LINE__)

// 3. GCC statement expression (evita el problema):
#define SWAP(a, b) ({ \
    typeof(a) _t = (a); \
    (a) = (b); \
    (b) = _t; \
})
```

## Macros con tipo incorrecto

```c
// Las macros no verifican tipos:

#define MAX(a, b) ((a) > (b) ? (a) : (b))

// Funciona con ints, floats, punteros... sin verificación:
MAX(42, "hello");    // compara int con char* → compila (¡pero es basura!)

// Solución: _Generic (C11) para dispatch por tipo:
#define MAX(a, b) _Generic((a), \
    int:    max_int,   \
    double: max_double \
)(a, b)

// O simplemente usar funciones inline con tipo explícito:
static inline int max_int(int a, int b) { return a > b ? a : b; }
```

## Macros que rompen el flujo

```c
// MAL — return dentro de un macro:
#define CHECK_NULL(p) if (!(p)) return NULL

void *process(void *data) {
    CHECK_NULL(data);    // return oculto — difícil de rastrear
    // ...
}

// El return es invisible para quien lee el código.
// Si el macro se usa en una función que retorna int:
int foo(void *p) {
    CHECK_NULL(p);    // return NULL en función que retorna int → warning
    return 0;
}

// Mejor: ser explícito con el return:
#define CHECK_NULL(p) do { \
    if (!(p)) { \
        fprintf(stderr, "%s:%d: null pointer\n", __FILE__, __LINE__); \
        return NULL; \
    } \
} while (0)
// Al menos el stderr hace visible que algo pasa.
// O mejor: no usar macros para control de flujo.
```

## Resumen de reglas

```c
// 1. Envolver cada parámetro en paréntesis:
#define AREA(w, h) ((w) * (h))

// 2. Envolver la expresión completa en paréntesis:
#define DOUBLE(x) ((x) * 2)

// 3. Usar do { ... } while (0) para macros con sentencias:
#define LOG(msg) do { printf("%s\n", msg); } while (0)

// 4. No pasar expresiones con side effects:
// MAX(i++, j++)  — MAL
// max(i++, j++)  — BIEN (si max es función)

// 5. Preferir funciones inline sobre macros:
// static inline int max(int a, int b) { return a > b ? a : b; }
// Es type-safe, se evalúa una vez, aparece en debugger.

// 6. Usar macros SOLO cuando inline no puede:
// - # y ## (stringify, token pasting)
// - __FILE__, __LINE__ del caller
// - Compilación condicional (#ifdef)
// - Código genérico por tipo (_Generic)
```

---

## Ejercicios

### Ejercicio 1 — Demostrar evaluación múltiple

```c
// Crear un macro MIN(a, b) sin protección contra evaluación múltiple.
// Llamar con MIN(i++, j++) y mostrar que los valores son incorrectos.
// Luego crear una versión con GCC statement expression que funcione.
// Comparar con una función inline.
```

### Ejercicio 2 — do-while(0)

```c
// Crear un macro ASSERT(cond, msg) que:
// - Verifique cond
// - Si falla: imprima archivo, línea, condición y msg a stderr
// - Llame a abort()
// Usar do { } while (0).
// Probar dentro de if/else para verificar que no rompe el flujo.
```

### Ejercicio 3 — Encontrar bugs

```c
// Estos macros tienen bugs. Identificar y corregir cada uno:
#define DOUBLE(x)  x + x
#define IS_ODD(n)  n % 2 == 1
#define PRINT(x)   printf("%d\n", x)   // sin ; ni do-while
#define ALLOC(type) (type *)malloc(sizeof(type))
```
