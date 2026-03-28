# T02 — Enums

## Declaración de enums

Un enum define un conjunto de constantes enteras con nombre:

```c
#include <stdio.h>

enum Color {
    RED,       // 0
    GREEN,     // 1
    BLUE,      // 2
};

int main(void) {
    enum Color c = RED;

    printf("RED = %d\n", RED);       // 0
    printf("GREEN = %d\n", GREEN);   // 1
    printf("BLUE = %d\n", BLUE);     // 2

    return 0;
}
```

```c
// Los valores se asignan automáticamente desde 0:
enum Direction { NORTH, SOUTH, EAST, WEST };
// NORTH=0, SOUTH=1, EAST=2, WEST=3

// Se pueden asignar valores explícitos:
enum HttpStatus {
    HTTP_OK         = 200,
    HTTP_NOT_FOUND  = 404,
    HTTP_ERROR      = 500,
};

// Mezclar auto y explícito:
enum Mixed {
    A,          // 0
    B = 10,     // 10
    C,          // 11 (sigue desde el anterior)
    D,          // 12
    E = 100,    // 100
    F,          // 101
};
```

## Enums y tipos en C

```c
// En C, un enum es esencialmente un int.
// No hay type safety — cualquier int se puede usar como enum:

enum Color { RED, GREEN, BLUE };

enum Color c = RED;     // OK
c = 42;                  // OK en C — sin warning
c = -1;                  // OK en C — sin warning

// El compilador no verifica que el valor sea un miembro válido.
// Esto es diferente de C++ (que da warning) y Rust (que da error).

// El tipo subyacente es implementation-defined (generalmente int):
printf("sizeof(enum Color) = %zu\n", sizeof(enum Color));    // 4
```

```c
// Conversión implícita enum ↔ int:
enum Color c = GREEN;
int n = c;          // OK — enum → int implícito
c = n;              // OK en C — int → enum implícito

// Por eso los enums de C son "weak enums" — son ints disfrazados.
// Se pueden sumar, restar, usar en bitwise:
int sum = RED + GREEN + BLUE;    // 3 — compila sin problema
```

## Buenas prácticas

### Prefijo para evitar colisiones

```c
// Los valores de enum están en el scope global.
// Sin prefijo, colisionan fácilmente:

// MAL — colisión probable:
enum Color { RED, GREEN, BLUE };
enum TrafficLight { RED, YELLOW, GREEN };    // ERROR: RED y GREEN redefinidos

// BIEN — prefijo por tipo:
enum Color { COLOR_RED, COLOR_GREEN, COLOR_BLUE };
enum TrafficLight { LIGHT_RED, LIGHT_YELLOW, LIGHT_GREEN };

// Convención común: TIPO_VALOR
// COLOR_RED, HTTP_OK, LOG_ERROR, STATE_IDLE
```

### Valor COUNT para iterar

```c
// Agregar un _COUNT al final para saber cuántos valores hay:
enum Color {
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE,
    COLOR_COUNT,    // = 3 (número de colores)
};

// Iterar:
for (int i = 0; i < COLOR_COUNT; i++) {
    printf("color %d\n", i);
}

// Verificar arrays de lookup:
static const char *color_names[COLOR_COUNT] = {
    "red", "green", "blue",
};

static_assert(sizeof(color_names) / sizeof(color_names[0]) == COLOR_COUNT,
              "color_names out of sync with Color enum");

// Si se agrega un nuevo color antes de COUNT,
// COLOR_COUNT se actualiza automáticamente.
```

### Enum para flags (bitmask)

```c
// Cada valor es una potencia de 2 → se pueden combinar con |:
enum Permission {
    PERM_NONE    = 0,
    PERM_READ    = 1 << 0,    // 1
    PERM_WRITE   = 1 << 1,    // 2
    PERM_EXECUTE = 1 << 2,    // 4
    PERM_ALL     = PERM_READ | PERM_WRITE | PERM_EXECUTE,    // 7
};

unsigned int perms = PERM_READ | PERM_WRITE;

// Testear:
if (perms & PERM_READ)  printf("can read\n");
if (perms & PERM_EXECUTE) printf("can execute\n");

// Agregar/quitar:
perms |= PERM_EXECUTE;      // agregar
perms &= ~PERM_WRITE;       // quitar
```

## -Wswitch para exhaustividad

```c
// Con -Wall, GCC/Clang avisan si un switch no cubre todos los valores:

enum Color { COLOR_RED, COLOR_GREEN, COLOR_BLUE };

const char *color_name(enum Color c) {
    switch (c) {
        case COLOR_RED:   return "red";
        case COLOR_GREEN: return "green";
        // FALTA COLOR_BLUE
    }
    // warning: enumeration value 'COLOR_BLUE' not handled in switch
    return "unknown";
}

// Esto es MUY útil — cuando agregas un valor al enum,
// el compilador te dice todos los switch que necesitan actualizarse.

// NO usar default si quieres esta verificación:
switch (c) {
    case COLOR_RED:   return "red";
    case COLOR_GREEN: return "green";
    case COLOR_BLUE:  return "blue";
    // Sin default → si agregas COLOR_YELLOW, el compilador avisa
}
```

## Enum vs #define

```c
// #define:
#define COLOR_RED   0
#define COLOR_GREEN 1
#define COLOR_BLUE  2
// Son sustitución de texto — no tienen tipo.
// No aparecen en el debugger.
// Pueden colisionar con otros macros.

// enum:
enum Color { COLOR_RED, COLOR_GREEN, COLOR_BLUE };
// Son valores con tipo enum Color.
// Aparecen en el debugger con su nombre.
// El compilador puede verificar exhaustividad en switch.
// Respetan scope (dentro de functions si se declaran ahí).

// Preferir enum sobre #define para constantes relacionadas.
// Usar #define solo cuando necesitas:
// - Valores string (#define NAME "hello")
// - Compilación condicional (#ifdef)
// - Valores que no caben en int
```

## Enum en structs

```c
// Patrón común: estado como enum dentro de un struct:
enum ConnectionState {
    CONN_DISCONNECTED,
    CONN_CONNECTING,
    CONN_CONNECTED,
    CONN_ERROR,
};

struct Connection {
    int fd;
    enum ConnectionState state;
    char address[64];
};

void conn_print_status(const struct Connection *c) {
    static const char *state_names[] = {
        "disconnected", "connecting", "connected", "error",
    };
    printf("[%s] %s\n", c->address, state_names[c->state]);
}
```

## Conversión a string

```c
// Patrón: array de nombres indexado por el enum:
enum LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_LEVEL_COUNT,
};

static const char *log_level_names[LOG_LEVEL_COUNT] = {
    [LOG_DEBUG] = "DEBUG",
    [LOG_INFO]  = "INFO",
    [LOG_WARN]  = "WARN",
    [LOG_ERROR] = "ERROR",
};

const char *log_level_str(enum LogLevel level) {
    if (level >= 0 && level < LOG_LEVEL_COUNT) {
        return log_level_names[level];
    }
    return "UNKNOWN";
}

// Patrón X-macro (avanzado — para evitar desincronización):
#define LOG_LEVELS(X) \
    X(LOG_DEBUG, "DEBUG") \
    X(LOG_INFO,  "INFO")  \
    X(LOG_WARN,  "WARN")  \
    X(LOG_ERROR, "ERROR")

// Generar enum:
enum LogLevel {
    #define X(name, str) name,
    LOG_LEVELS(X)
    #undef X
    LOG_LEVEL_COUNT,
};

// Generar nombres:
static const char *log_level_names[] = {
    #define X(name, str) [name] = str,
    LOG_LEVELS(X)
    #undef X
};
```

---

## Ejercicios

### Ejercicio 1 — Máquina de estados

```c
// Definir enum State { IDLE, RUNNING, PAUSED, STOPPED }.
// Implementar State transition(State current, const char *event):
// - IDLE + "start" → RUNNING
// - RUNNING + "pause" → PAUSED
// - RUNNING + "stop" → STOPPED
// - PAUSED + "resume" → RUNNING
// - PAUSED + "stop" → STOPPED
// Usar switch con -Wswitch. Imprimir las transiciones.
```

### Ejercicio 2 — Enum flags

```c
// Definir enum FileMode con flags:
// MODE_READ, MODE_WRITE, MODE_APPEND, MODE_CREATE, MODE_TRUNCATE
// Implementar:
// - const char *fopen_mode(unsigned int flags) — convierte a string de fopen
//   Ej: READ → "r", READ|WRITE → "r+", WRITE|CREATE|TRUNCATE → "w"
// Probar con varias combinaciones.
```

### Ejercicio 3 — Enum to string

```c
// Crear enum Weekday con los 7 días.
// Implementar:
// - const char *weekday_name(enum Weekday d)
// - enum Weekday weekday_from_string(const char *s) — retorna -1 si no válido
// - _Bool weekday_is_weekend(enum Weekday d)
// Usar designated initializers para el array de nombres.
// Compilar con -Wswitch y verificar exhaustividad.
```
