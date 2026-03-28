# T02 — Enums

> **Fuentes**: `README.md`, `LABS.md`, `labs/README.md`, `labs/enum_basics.c`,
> `labs/enum_switch.c`, `labs/enum_count.c`, `labs/enum_limits.c`

## Erratas detectadas

Sin erratas detectadas en el material fuente.

---

## Tabla de contenidos

1. [Declaración y valores](#1--declaración-y-valores)
2. [sizeof y tipo subyacente](#2--sizeof-y-tipo-subyacente)
3. [Enums como constantes en switch](#3--enums-como-constantes-en-switch)
4. [Exhaustividad con -Wswitch](#4--exhaustividad-con--wswitch)
5. [Patrón COUNT](#5--patrón-count)
6. [Prefijos y namespace global](#6--prefijos-y-namespace-global)
7. [Enum como flags (bitmask)](#7--enum-como-flags-bitmask)
8. [Enum vs #define](#8--enum-vs-define)
9. [Enum en structs y conversión a string](#9--enum-en-structs-y-conversión-a-string)
10. [Comparación con Rust](#10--comparación-con-rust)

---

## 1 — Declaración y valores

Un `enum` define un conjunto de **constantes enteras con nombre**. Reemplazan
números mágicos con identificadores legibles:

```c
enum Direction { NORTH, SOUTH, EAST, WEST };
// NORTH=0, SOUTH=1, EAST=2, WEST=3
```

### Valores automáticos

Sin asignación explícita, el primer miembro vale **0** y cada siguiente
incrementa en 1:

```c
enum Color { RED, GREEN, BLUE };
// RED=0, GREEN=1, BLUE=2
```

### Valores explícitos

Se puede asignar cualquier valor entero (incluso negativo o repetido):

```c
enum HttpStatus {
    HTTP_OK         = 200,
    HTTP_NOT_FOUND  = 404,
    HTTP_ERROR      = 500,
};
```

### Valores mixtos

Después de un valor explícito, los automáticos **continúan desde ahí**:

```c
enum Mixed {
    A,          // 0
    B = 10,     // 10
    C,          // 11 (10 + 1)
    D,          // 12 (11 + 1)
    E = 100,    // 100
    F,          // 101 (100 + 1)
};
```

Esta regla es la clave: el compilador siempre asigna `anterior + 1` al
siguiente miembro que no tiene valor explícito.

### Valores duplicados

Los valores duplicados son legales — dos nombres pueden tener el mismo valor:

```c
enum Alias {
    ENABLED  = 1,
    ON       = 1,   // alias de ENABLED
    DISABLED = 0,
    OFF      = 0,   // alias de DISABLED
};
```

Esto es útil para crear alias o sinónimos, pero puede causar confusión si se
usa sin intención.

---

## 2 — sizeof y tipo subyacente

En C, un enum es **esencialmente un `int`**. El estándar dice que el tipo
subyacente es *implementation-defined*, pero en la práctica casi todos los
compiladores usan `int`:

```c
enum Color { RED, GREEN, BLUE };

printf("sizeof(enum Color) = %zu\n", sizeof(enum Color));  // 4
printf("sizeof(int)        = %zu\n", sizeof(int));          // 4
```

### Consecuencias de ser int

```c
enum Color c = RED;

// Conversión implícita enum → int:
int n = c;          // OK — sin cast necesario

// Conversión implícita int → enum:
c = 42;             // OK en C — compila sin error ni warning
c = -1;             // OK en C — compila sin error

// Aritmética directa:
int sum = RED + GREEN + BLUE;   // 0 + 1 + 2 = 3
```

Los enums de C son **"weak enums"**: ints disfrazados con nombres simbólicos.
El compilador no verifica que el valor asignado sea un miembro válido del enum.
Esto contrasta con C++ (`enum class`) y Rust (`enum`), que son type-safe.

---

## 3 — Enums como constantes en switch

El uso más natural de un enum es en un `switch`:

```c
enum Color { COLOR_RED, COLOR_GREEN, COLOR_BLUE };

const char *color_describe(enum Color c) {
    switch (c) {
        case COLOR_RED:   return "warm";
        case COLOR_GREEN: return "cool";
        case COLOR_BLUE:  return "cool";
    }
    return "unknown";
}
```

### Array indexado por enum

Un array con designated initializers (C99) indexado por los valores del enum
es un patrón potente:

```c
static const char *color_names[] = {
    [COLOR_RED]   = "red",
    [COLOR_GREEN] = "green",
    [COLOR_BLUE]  = "blue",
};

printf("%s\n", color_names[COLOR_GREEN]);   // "green"
```

**Ventaja**: los designated initializers `[COLOR_RED] = "red"` garantizan que
cada posición corresponda al valor correcto del enum, independientemente del
orden en que se listen. Si reorganizas los miembros del enum, el array sigue
correcto.

---

## 4 — Exhaustividad con -Wswitch

Con `-Wall` (que incluye `-Wswitch`), GCC y Clang **avisan** si un `switch`
sobre un enum no cubre todos los miembros:

```c
enum Color { COLOR_RED, COLOR_GREEN, COLOR_BLUE };

const char *color_name(enum Color c) {
    switch (c) {
        case COLOR_RED:   return "red";
        case COLOR_GREEN: return "green";
        // FALTA COLOR_BLUE
    }
    return "unknown";
}
// warning: enumeration value 'COLOR_BLUE' not handled in switch [-Wswitch]
```

### La trampa del default

Si agregas `default:`, el compilador ya no avisa de miembros faltantes:

```c
switch (c) {
    case COLOR_RED:   return "red";
    case COLOR_GREEN: return "green";
    default:          return "other";
    // Si agregas COLOR_YELLOW al enum, NO recibes warning
}
```

**Regla práctica**: en switches sobre enums, **omitir `default`** y manejar
el caso residual con un `return` después del switch. Así el compilador te
dice exactamente qué valores no cubriste cuando el enum crece.

Esto es una ventaja enorme sobre `#define`: cuando agregas un valor al enum,
el compilador te marca **todos** los switches que necesitan actualizarse.

---

## 5 — Patrón COUNT

Agregar un miembro `_COUNT` al final del enum proporciona automáticamente la
**cantidad de valores reales**:

```c
enum LogLevel {
    LOG_DEBUG,          // 0
    LOG_INFO,           // 1
    LOG_WARN,           // 2
    LOG_ERROR,          // 3
    LOG_LEVEL_COUNT,    // 4 — no es un nivel real, es el conteo
};
```

### Tres usos del COUNT

**1. Dimensionar arrays:**

```c
static const char *level_names[LOG_LEVEL_COUNT] = {
    [LOG_DEBUG] = "DEBUG",
    [LOG_INFO]  = "INFO",
    [LOG_WARN]  = "WARN",
    [LOG_ERROR] = "ERROR",
};
```

**2. Iterar sobre todos los valores:**

```c
for (int i = 0; i < LOG_LEVEL_COUNT; i++) {
    printf("[%d] %s\n", i, level_names[i]);
}
```

**3. Validar rangos (conversión segura):**

```c
const char *log_level_str(enum LogLevel level) {
    if (level >= 0 && level < LOG_LEVEL_COUNT) {
        return level_names[level];
    }
    return "UNKNOWN";
}

log_level_str(LOG_WARN);   // "WARN"
log_level_str(99);         // "UNKNOWN"
```

### Auto-sincronización

Si agregas `LOG_FATAL` **antes** de `LOG_LEVEL_COUNT`:

```c
enum LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL,          // nuevo
    LOG_LEVEL_COUNT,    // ahora vale 5 automáticamente
};
```

Los arrays dimensionados con `[LOG_LEVEL_COUNT]` se redimensionan en
compilación. Si olvidas agregar la entrada correspondiente en el array, un
`static_assert` puede detectarlo:

```c
static_assert(sizeof(level_names) / sizeof(level_names[0]) == LOG_LEVEL_COUNT,
              "level_names out of sync with LogLevel enum");
```

### Limitación del patrón COUNT

Solo funciona cuando los valores son **consecutivos desde 0**. Si el enum
tiene valores explícitos no consecutivos (`HTTP_OK = 200, HTTP_NOT_FOUND = 404`),
el COUNT no refleja la cantidad real de miembros.

---

## 6 — Prefijos y namespace global

Los valores de un enum viven en el **scope global** (a diferencia de C++ `enum class`):

```c
// ERROR — colisión de nombres:
enum Color        { RED, GREEN, BLUE };
enum TrafficLight { RED, YELLOW, GREEN };   // error: RED y GREEN redefinidos
```

### Convención de prefijos

La solución estándar en C es usar **prefijos por tipo**:

```c
enum Color        { COLOR_RED,  COLOR_GREEN,  COLOR_BLUE };
enum TrafficLight { LIGHT_RED,  LIGHT_YELLOW, LIGHT_GREEN };
```

Convención: `TIPO_VALOR` — `COLOR_RED`, `HTTP_OK`, `LOG_ERROR`, `STATE_IDLE`.

### Mezcla entre enums

Al ser ints, un valor de un enum puede asignarse a otro:

```c
enum Color color = COLOR_RED;
enum Fruit fruit = FRUIT_RED;

color = FRUIT_GREEN;    // compila en C (warning con -Wenum-conversion)
```

Con `-Wextra` (GCC 10+) obtienes:

```
warning: implicit conversion from 'enum Fruit' to 'enum Color' [-Wenum-conversion]
```

Pero sigue siendo **solo un warning**, no un error. El binario se genera.

---

## 7 — Enum como flags (bitmask)

Cuando cada valor del enum es una **potencia de 2**, se pueden combinar con
operaciones bitwise para crear conjuntos de flags:

```c
enum Permission {
    PERM_NONE    = 0,
    PERM_READ    = 1 << 0,    // 1  (0b001)
    PERM_WRITE   = 1 << 1,    // 2  (0b010)
    PERM_EXECUTE = 1 << 2,    // 4  (0b100)
    PERM_ALL     = PERM_READ | PERM_WRITE | PERM_EXECUTE,  // 7 (0b111)
};
```

### Operaciones con flags

```c
unsigned int perms = PERM_READ | PERM_WRITE;   // 0b011

// Testear si un flag está presente:
if (perms & PERM_READ)    printf("can read\n");     // sí
if (perms & PERM_EXECUTE) printf("can execute\n");  // no

// Agregar un flag:
perms |= PERM_EXECUTE;     // 0b111

// Quitar un flag:
perms &= ~PERM_WRITE;      // 0b101

// Verificar si tiene todos los flags:
if ((perms & PERM_ALL) == PERM_ALL) printf("full access\n");

// Toggle (invertir) un flag:
perms ^= PERM_READ;        // si estaba activo, se desactiva y viceversa
```

### Nota sobre el tipo

La variable que almacena flags combinados se declara como `unsigned int`,
**no** como `enum Permission`, porque un valor combinado (`PERM_READ | PERM_WRITE = 3`)
no es un miembro del enum. Usar `unsigned int` es semánticamente más correcto.

---

## 8 — Enum vs #define

| Aspecto | `#define` | `enum` |
|---------|-----------|--------|
| Tipo | Sin tipo (sustitución textual) | `int` con nombre de tipo |
| Debugger | No visible (desaparece en preproceso) | Visible con nombre simbólico |
| Scope | Global (macro) | Respeta scope si se declara en función |
| Exhaustividad | No verificable en switch | `-Wswitch` detecta casos faltantes |
| Valores | Cualquier tipo/expresión | Solo `int` |
| Agrupación | Sin agrupación lógica | Miembros agrupados bajo un tipo |

### Cuándo usar cada uno

**Preferir `enum`** para constantes enteras relacionadas (estados, niveles,
colores, tipos). Ofrecen mejor verificación y depuración.

**Usar `#define`** cuando necesitas:
- Constantes de tipo string: `#define APP_NAME "myapp"`
- Compilación condicional: `#ifdef DEBUG`
- Valores que no caben en `int`: `#define BIG_VAL 0xFFFFFFFFFFLL`
- Expresiones macro con parámetros: `#define MAX(a,b) ((a)>(b)?(a):(b))`

---

## 9 — Enum en structs y conversión a string

### Enum como campo de estado

Patrón muy común: un struct que tiene un enum como campo de estado o tipo:

```c
enum ConnectionState {
    CONN_DISCONNECTED,
    CONN_CONNECTING,
    CONN_CONNECTED,
    CONN_ERROR,
    CONN_STATE_COUNT,
};

struct Connection {
    int fd;
    enum ConnectionState state;
    char address[64];
};
```

### Conversión a string con array

```c
static const char *state_names[CONN_STATE_COUNT] = {
    [CONN_DISCONNECTED] = "disconnected",
    [CONN_CONNECTING]   = "connecting",
    [CONN_CONNECTED]    = "connected",
    [CONN_ERROR]        = "error",
};

void conn_print(const struct Connection *c) {
    printf("[%s] %s\n", c->address, state_names[c->state]);
}
```

### X-macro para sincronización automática

El problema: enum y array de nombres son dos listas que deben mantenerse en
sincronía. Si agregas un miembro al enum pero olvidas la string, tienes un
bug silencioso (`NULL` en el array).

La X-macro resuelve esto con **una sola fuente de verdad**:

```c
// Definir la tabla una sola vez:
#define LOG_LEVELS(X) \
    X(LOG_DEBUG, "DEBUG") \
    X(LOG_INFO,  "INFO")  \
    X(LOG_WARN,  "WARN")  \
    X(LOG_ERROR, "ERROR")

// Generar el enum:
enum LogLevel {
    #define X(name, str) name,
    LOG_LEVELS(X)
    #undef X
    LOG_LEVEL_COUNT,
};

// Generar el array de nombres:
static const char *log_level_names[] = {
    #define X(name, str) [name] = str,
    LOG_LEVELS(X)
    #undef X
};
```

Ahora agregar un nivel requiere **una sola línea** en la macro `LOG_LEVELS`.
El enum y el array se generan automáticamente. Es más complejo de leer pero
imposible de desincronizar.

---

## 10 — Comparación con Rust

| Aspecto | C `enum` | Rust `enum` |
|---------|----------|-------------|
| Type safety | Ninguna — es un `int` | Total — tipo propio, no convertible |
| Asignar int | `c = 42;` — sin error | Error de compilación |
| Datos asociados | No (solo constante) | Sí — cada variante puede tener datos |
| Pattern matching | `switch` sin verificación obligatoria | `match` exhaustivo obligatorio |
| Namespace | Global — requiere prefijos | Propio — `Color::Red`, sin colisión |
| Conversión a string | Manual (array/X-macro) | `#[derive(Debug)]` o `Display` |

### Ejemplo comparativo

```c
// C — tagged union manual (union + enum):
enum ShapeKind { SHAPE_CIRCLE, SHAPE_RECT };
struct Shape {
    enum ShapeKind kind;
    union {
        double radius;
        struct { double w, h; };
    };
};
// Nada impide acceder .radius cuando kind == SHAPE_RECT
```

```rust
// Rust — enum con datos (verified by compiler):
enum Shape {
    Circle { radius: f64 },
    Rect { w: f64, h: f64 },
}
// match shape {
//     Shape::Circle { radius } => ...,
//     Shape::Rect { w, h } => ...,
// }
// Acceder la variante incorrecta es imposible
```

El `enum` de Rust unifica lo que en C requiere `enum` + `union` + `struct` +
disciplina manual. El compilador de Rust garantiza que no accedas a la variante
incorrecta, mientras que en C esto queda como responsabilidad del programador.

### Conversión explícita en Rust

```rust
#[repr(u8)]
enum Color { Red = 0, Green = 1, Blue = 2 }

let n: u8 = Color::Green as u8;    // enum → int requiere cast explícito
// let c: Color = 1;               // int → enum: ERROR de compilación
```

---

## Ejercicios

### Ejercicio 1 — Declaración y valores mixtos

```c
// Declarar un enum Priority con:
//   PRIORITY_LOW (automático desde 0),
//   PRIORITY_MEDIUM = 5,
//   PRIORITY_HIGH (automático),
//   PRIORITY_CRITICAL = 100,
//   PRIORITY_COUNT (automático)
//
// Imprimir cada valor y sizeof(enum Priority).
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex01.c -o ex01
```

<details><summary>Predicción</summary>

```
PRIORITY_LOW      = 0
PRIORITY_MEDIUM   = 5
PRIORITY_HIGH     = 6
PRIORITY_CRITICAL = 100
PRIORITY_COUNT    = 101
sizeof(enum Priority) = 4
```

`PRIORITY_HIGH` vale 6 (5 + 1). `PRIORITY_COUNT` vale 101 (100 + 1).
Nota: aquí el patrón COUNT **no funciona** como conteo real — hay solo 4
prioridades pero COUNT vale 101. El patrón COUNT solo es útil con valores
consecutivos desde 0.

</details>

---

### Ejercicio 2 — Switch exhaustivo

```c
// Declarar:
//   enum Season { SEASON_SPRING, SEASON_SUMMER, SEASON_AUTUMN, SEASON_WINTER };
//
// Implementar:
//   const char *season_description(enum Season s)
// que retorne una descripción para cada estación (ej: "flowers bloom").
//
// Compilar con -Wall. NO usar default en el switch.
// Luego agregar SEASON_MONSOON al enum (sin actualizar el switch)
// y recompilar para ver el warning de -Wswitch.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex02.c -o ex02
```

<details><summary>Predicción</summary>

Con las 4 estaciones cubiertas: compila sin warnings y muestra la descripción
de cada estación.

Al agregar `SEASON_MONSOON` sin actualizar el switch:

```
warning: enumeration value 'SEASON_MONSOON' not handled in switch [-Wswitch]
```

El programa compila (es un warning, no un error) pero la función retorna
lo que haya después del switch para `SEASON_MONSOON`. Sin `default`, la
ejecución cae al `return` posterior.

</details>

---

### Ejercicio 3 — Patrón COUNT con array de nombres

```c
// Declarar:
//   enum Planet {
//       PLANET_MERCURY, PLANET_VENUS, PLANET_EARTH, PLANET_MARS,
//       PLANET_JUPITER, PLANET_SATURN, PLANET_URANUS, PLANET_NEPTUNE,
//       PLANET_COUNT,
//   };
//
// Crear un array planet_names[PLANET_COUNT] con designated initializers.
// Implementar:
//   const char *planet_str(enum Planet p)  — retorna nombre o "UNKNOWN"
//
// Iterar con for e imprimir todos los planetas con su índice.
// Probar planet_str(99) para verificar la validación de rango.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex03.c -o ex03
```

<details><summary>Predicción</summary>

```
[0] Mercury
[1] Venus
[2] Earth
[3] Mars
[4] Jupiter
[5] Saturn
[6] Uranus
[7] Neptune
PLANET_COUNT = 8

planet_str(PLANET_EARTH) = Earth
planet_str(99)           = UNKNOWN
```

`PLANET_COUNT` vale 8 automáticamente. La función `planet_str` valida
`p >= 0 && p < PLANET_COUNT` antes de indexar, evitando acceso fuera de rango.

</details>

---

### Ejercicio 4 — Flags bitmask

```c
// Declarar:
//   enum TextStyle {
//       STYLE_NONE      = 0,
//       STYLE_BOLD      = 1 << 0,   // 1
//       STYLE_ITALIC    = 1 << 1,   // 2
//       STYLE_UNDERLINE = 1 << 2,   // 4
//       STYLE_STRIKE    = 1 << 3,   // 8
//   };
//
// Implementar:
//   void print_styles(unsigned int styles)
// que imprima los nombres de todos los flags activos.
//
// Probar con:
//   unsigned int s = STYLE_BOLD | STYLE_UNDERLINE;
//   print_styles(s);       // → "BOLD UNDERLINE"
//   s |= STYLE_ITALIC;     // agregar
//   s &= ~STYLE_BOLD;      // quitar
//   print_styles(s);       // → "ITALIC UNDERLINE"
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex04.c -o ex04
```

<details><summary>Predicción</summary>

```
Styles: BOLD UNDERLINE
Styles: ITALIC UNDERLINE
```

`STYLE_BOLD | STYLE_UNDERLINE` = 1 | 4 = 5 (0b0101). Después de
`|= STYLE_ITALIC` = 5 | 2 = 7 (0b0111). Después de `&= ~STYLE_BOLD`
= 7 & ~1 = 7 & 0xFFFFFFFE = 6 (0b0110). La función testea cada flag con
`& STYLE_X` e imprime los activos.

</details>

---

### Ejercicio 5 — Enum to string y string to enum

```c
// Declarar:
//   enum Weekday {
//       WEEKDAY_MON, WEEKDAY_TUE, WEEKDAY_WED, WEEKDAY_THU,
//       WEEKDAY_FRI, WEEKDAY_SAT, WEEKDAY_SUN, WEEKDAY_COUNT,
//   };
//
// Implementar:
//   const char *weekday_name(enum Weekday d)      — enum → string
//   enum Weekday weekday_from_str(const char *s)   — string → enum (-1 si inválido)
//   _Bool weekday_is_weekend(enum Weekday d)        — SAT o SUN → true
//
// Usar strcmp en weekday_from_str para comparar strings.
// Probar con varias conversiones ida y vuelta.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex05.c -o ex05
```

<details><summary>Predicción</summary>

```
weekday_name(WEEKDAY_WED) = Wednesday
weekday_from_str("Friday") = 4 (WEEKDAY_FRI)
weekday_from_str("Holiday") = -1 (invalid)
weekday_is_weekend(WEEKDAY_SAT) = true
weekday_is_weekend(WEEKDAY_MON) = false
```

`weekday_from_str` recorre el array de nombres comparando con `strcmp`. Si
ninguno coincide, retorna `-1` (que como `enum Weekday` sería un valor
inválido — pero en C compila sin problema porque enums son ints).

</details>

---

### Ejercicio 6 — Enum vs #define

```c
// Definir las mismas constantes de dos formas:
//
// Con #define:
//   #define DIR_NORTH 0
//   #define DIR_SOUTH 1
//   #define DIR_EAST  2
//   #define DIR_WEST  3
//
// Con enum:
//   enum Direction { DIR2_NORTH, DIR2_SOUTH, DIR2_EAST, DIR2_WEST };
//
// Implementar un switch para cada grupo.
// En el switch de #define, quitar un case y compilar → ¿warning?
// En el switch de enum, quitar un case y compilar → ¿warning?
//
// Imprimir sizeof para ambos.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex06.c -o ex06
```

<details><summary>Predicción</summary>

El switch sobre variables `int` con `#define` **no genera** warning al
omitir un caso, porque el compilador no sabe que la variable solo tiene
4 valores posibles.

El switch sobre `enum Direction` **sí genera** warning:

```
warning: enumeration value 'DIR2_WEST' not handled in switch [-Wswitch]
```

`sizeof(enum Direction)` = 4 (= `sizeof(int)`). Para las constantes
`#define`, no tienen sizeof propio — son literales enteros.

</details>

---

### Ejercicio 7 — Máquina de estados

```c
// Declarar:
//   enum State { STATE_IDLE, STATE_RUNNING, STATE_PAUSED, STATE_STOPPED };
//
// Implementar:
//   enum State transition(enum State current, const char *event)
// con las reglas:
//   IDLE     + "start"  → RUNNING
//   RUNNING  + "pause"  → PAUSED
//   RUNNING  + "stop"   → STOPPED
//   PAUSED   + "resume" → RUNNING
//   PAUSED   + "stop"   → STOPPED
//   Cualquier otra combinación → retorna current (sin cambio)
//
// Implementar state_name() usando array con designated initializers.
// Probar la secuencia: IDLE → start → pause → resume → stop
// Imprimir cada transición.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex07.c -o ex07
```

<details><summary>Predicción</summary>

```
[IDLE] + "start"  → RUNNING
[RUNNING] + "pause"  → PAUSED
[PAUSED] + "resume" → RUNNING
[RUNNING] + "stop"   → STOPPED
[STOPPED] + "start"  → STOPPED  (no hay transición definida)
```

La función usa un switch sobre `current` y dentro de cada case compara
`event` con `strcmp`. `STOPPED` es un estado terminal — ningún evento
lo saca de ahí (retorna `current`).

</details>

---

### Ejercicio 8 — Enum en struct

```c
// Declarar:
//   enum TaskStatus { TASK_TODO, TASK_IN_PROGRESS, TASK_DONE, TASK_STATUS_COUNT };
//
//   struct Task {
//       int id;
//       char title[64];
//       enum TaskStatus status;
//   };
//
// Implementar:
//   void task_print(const struct Task *t)        — imprime "[STATUS] #id: title"
//   void task_advance(struct Task *t)             — avanza al siguiente status
//   void tasks_by_status(const struct Task *tasks, int n, enum TaskStatus s)
//       — imprime solo los tasks con el status dado
//
// Crear un array de 5 tasks con estados variados. Avanzar algunos y filtrar.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex08.c -o ex08
```

<details><summary>Predicción</summary>

```
All tasks:
[TODO]        #1: Design UI
[IN_PROGRESS] #2: Write API
[DONE]        #3: Setup CI
[TODO]        #4: Write tests
[IN_PROGRESS] #5: Deploy staging

After advancing #1 and #4:
[IN_PROGRESS] #1: Design UI
[IN_PROGRESS] #4: Write tests

Tasks with status IN_PROGRESS:
  #1: Design UI
  #2: Write API
  #4: Write tests
  #5: Deploy staging
```

`task_advance` incrementa `t->status` si no ha llegado a `TASK_DONE`.
Se puede implementar como `if (t->status < TASK_DONE) t->status++`.
El filtro `tasks_by_status` recorre el array comparando `tasks[i].status == s`.

</details>

---

### Ejercicio 9 — X-macro

```c
// Usar la técnica X-macro para definir un enum y su array de strings
// desde una sola fuente:
//
// #define HTTP_CODES(X) \
//     X(HTTP_OK,            200, "OK")            \
//     X(HTTP_CREATED,       201, "Created")        \
//     X(HTTP_BAD_REQUEST,   400, "Bad Request")    \
//     X(HTTP_NOT_FOUND,     404, "Not Found")      \
//     X(HTTP_SERVER_ERROR,  500, "Server Error")
//
// Generar:
//   1. El enum con los valores explícitos
//   2. Un array de strings con los nombres
//   3. Una función http_status_str(int code) que retorne el string
//
// Probar con varios códigos, incluyendo uno no definido (302).
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex09.c -o ex09
```

<details><summary>Predicción</summary>

```
200 → OK
201 → Created
400 → Bad Request
404 → Not Found
500 → Server Error
302 → UNKNOWN
```

El X-macro con 3 parámetros (name, value, string) genera:
- Enum: `HTTP_OK = 200, HTTP_CREATED = 201, ...`
- Array: requiere un enfoque diferente porque los valores no son consecutivos.
  Se necesita un array de structs `{code, string}` o un switch en lugar de
  array indexado. La función `http_status_str` recorre el array o usa switch
  para encontrar el código correspondiente.

</details>

---

### Ejercicio 10 — Limitaciones: type safety

```c
// Declarar:
//   enum Color { COLOR_RED, COLOR_GREEN, COLOR_BLUE };
//   enum Fruit { FRUIT_APPLE, FRUIT_BANANA, FRUIT_CHERRY };
//
// Demostrar las 4 limitaciones:
//   1. Asignar int arbitrario: enum Color c = 42;
//   2. Aritmética: int x = COLOR_RED + COLOR_BLUE;
//   3. Mezcla entre enums: enum Color c2 = FRUIT_BANANA;
//   4. Namespace: intentar usar el mismo nombre en dos enums (comentado, explicar)
//
// Para cada caso, predecir si compila, si da warning, y qué valor tiene.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex10.c -o ex10
```

<details><summary>Predicción</summary>

```
1. c = 42     → c = 42  (compila OK, sin warning)
2. RED + BLUE → x = 2   (compila OK, sin warning)
3. c2 = FRUIT_BANANA → c2 = 1 (compila con warning: -Wenum-conversion)
4. Namespace: si dos enums tienen un miembro con el mismo nombre, es error
   de compilación (redefinición)
```

Los casos 1 y 2 no generan warnings porque para C un enum es un int.
El caso 3 genera warning con `-Wenum-conversion` (incluido en `-Wextra`
en GCC moderno) porque mezcla dos tipos enum distintos, pero compila.
El caso 4 es el único que es un error real de compilación.

</details>
