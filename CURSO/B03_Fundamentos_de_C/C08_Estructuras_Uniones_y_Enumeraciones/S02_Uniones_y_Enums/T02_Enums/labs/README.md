# Lab — Enums

## Objetivo

Declarar enums con valores automaticos y explicitos, usarlos como constantes
en switch y arrays, aplicar el patron COUNT para dimensionar arrays, y
verificar las limitaciones de type safety que tienen los enums en C. Al
finalizar, sabras cuando y como usar enums en lugar de `#define` o numeros
magicos.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `enum_basics.c` | Declaracion con valores automaticos, explicitos y mixtos |
| `enum_switch.c` | Enum en switch y array de nombres con designated initializers |
| `enum_count.c` | Patron COUNT para dimensionar arrays y conversion segura |
| `enum_limits.c` | Limitaciones: enums son int, no type-safe, namespace global |

---

## Parte 1 — Declaracion de enums

**Objetivo**: Declarar enums con valores automaticos, explicitos y mixtos, y
verificar su sizeof.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat enum_basics.c
```

Observa tres enums distintos:

- `Direction` — valores automaticos desde 0
- `HttpStatus` — valores explicitos (200, 404, 500)
- `Mixed` — mezcla de automaticos y explicitos

### Paso 1.2 — Predecir los valores

Antes de compilar, responde mentalmente:

- En `Mixed`, si `B = 10`, cuanto vale `C`?
- Y `D`?
- Si `E = 100`, cuanto vale `F`?
- `sizeof(enum Direction)` sera igual a `sizeof(int)` o diferente?

### Paso 1.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic enum_basics.c -o enum_basics
./enum_basics
```

Salida esperada:

```
--- Valores automaticos ---
NORTH = 0
SOUTH = 1
EAST  = 2
WEST  = 3

--- Valores explicitos ---
HTTP_OK        = 200
HTTP_NOT_FOUND = 404
HTTP_ERROR     = 500

--- Valores mixtos ---
A = 0
B = 10
C = 11
D = 12
E = 100
F = 101

--- sizeof ---
sizeof(enum Direction)  = 4
sizeof(enum HttpStatus) = 4
sizeof(int)             = 4
```

### Paso 1.4 — Analisis

Observa:

- Los valores automaticos empiezan en 0 y se incrementan en 1
- Despues de un valor explicito, los automaticos continuan desde ese valor:
  `B = 10` implica `C = 11`, `D = 12`
- `sizeof(enum)` es igual a `sizeof(int)` — en C, un enum es esencialmente
  un int con nombres

### Limpieza de la parte 1

```bash
rm -f enum_basics
```

---

## Parte 2 — Enums como constantes

**Objetivo**: Usar enums en switch y como indice de arrays con designated
initializers.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat enum_switch.c
```

Observa dos patrones:

- Un array `color_names[]` indexado por los valores del enum, usando
  designated initializers (`[COLOR_RED] = "red"`)
- Una funcion `color_describe()` con switch sobre el enum

### Paso 2.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic enum_switch.c -o enum_switch
./enum_switch
```

Salida esperada:

```
--- Array indexado por enum ---
COLOR_RED   (0) -> red
COLOR_GREEN (1) -> green
COLOR_BLUE  (2) -> blue

--- Switch sobre enum ---
red is warm
green is cool
blue is cool
```

### Paso 2.3 — Verificar exhaustividad con -Wswitch

El switch en `color_describe()` cubre los tres casos del enum. El compilador
no emite warning.

Ahora imagina que agregas `COLOR_YELLOW` al enum pero no al switch. Con
`-Wall` (que incluye `-Wswitch`), GCC avisaria:

```
warning: enumeration value 'COLOR_YELLOW' not handled in switch
```

Esto es una ventaja sobre `#define`: el compilador puede verificar que el
switch cubra todos los valores. Nota que esto solo funciona si **no** usas
`default` en el switch.

### Paso 2.4 — Designated initializers

El array `color_names[]` usa la sintaxis `[COLOR_RED] = "red"`. Esto
garantiza que cada posicion del array corresponde exactamente al valor del
enum, sin depender del orden. Si reorganizas los miembros del enum, el array
sigue correcto.

### Limpieza de la parte 2

```bash
rm -f enum_switch
```

---

## Parte 3 — Patron COUNT

**Objetivo**: Usar un valor `_COUNT` al final del enum para dimensionar
arrays y validar conversiones.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat enum_count.c
```

Observa:

- `LOG_LEVEL_COUNT` es el ultimo valor del enum — automaticamente vale 4
  (la cantidad de niveles reales)
- Los arrays `level_names[]` y `level_colors[]` se declaran con tamano
  `[LOG_LEVEL_COUNT]`
- `log_level_str()` verifica que el valor este dentro del rango antes de
  acceder al array

### Paso 3.2 — Predecir

Antes de compilar, responde mentalmente:

- Si hay 4 niveles (DEBUG, INFO, WARN, ERROR), cuanto vale `LOG_LEVEL_COUNT`?
- Que retorna `log_level_str(99)`?
- El check de `sizeof(level_names) / sizeof(level_names[0]) == LOG_LEVEL_COUNT`
  dara match o no?

### Paso 3.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic enum_count.c -o enum_count
./enum_count
```

Salida esperada:

```
LOG_LEVEL_COUNT = 4

--- Iterar con COUNT ---
  [0] DEBUG (color: gray)
  [1] INFO  (color: green)
  [2] WARN  (color: yellow)
  [3] ERROR (color: red)

--- Conversion segura ---
log_level_str(LOG_WARN) = WARN
log_level_str(99)       = UNKNOWN

--- Array size check ---
sizeof(level_names) / sizeof(level_names[0]) = 4
LOG_LEVEL_COUNT = 4
Match: yes
```

### Paso 3.4 — Analisis

El patron COUNT ofrece tres ventajas:

1. **Dimensiona arrays automaticamente** — si agregas `LOG_FATAL` antes de
   `LOG_LEVEL_COUNT`, el count se incrementa y los arrays se redimensionan
   en compilacion (aunque necesitaras agregar las entradas correspondientes)
2. **Permite iterar** — `for (int i = 0; i < LOG_LEVEL_COUNT; i++)`
   recorre todos los valores sin hardcodear el limite
3. **Valida conversiones** — puedes verificar `level >= 0 && level < COUNT`
   antes de indexar un array, evitando accesos fuera de rango

### Limpieza de la parte 3

```bash
rm -f enum_count
```

---

## Parte 4 — Limitaciones

**Objetivo**: Verificar que los enums de C no ofrecen type safety y que sus
valores viven en el namespace global.

### Paso 4.1 — Examinar el codigo fuente

```bash
cat enum_limits.c
```

Observa:

- Se asigna `42` y `-1` a una variable `enum Color` — valores que no son
  miembros del enum
- Se suman valores de enum como si fueran enteros
- Se asigna un valor de `enum Fruit` a una variable `enum Color`

### Paso 4.2 — Predecir

Antes de compilar, responde mentalmente:

- Compilara sin errores? Con warnings?
- `c = 42` generara algun warning?
- `color = FRUIT_GREEN` generara algun warning?

### Paso 4.3 — Compilar y observar los warnings

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic enum_limits.c -o enum_limits 2>&1
```

Salida esperada:

```
enum_limits.c: In function 'main':
enum_limits.c:30:11: warning: implicit conversion from 'enum Fruit' to 'enum Color' [-Wenum-conversion]
   30 |     color = FRUIT_GREEN;
      |           ^
```

Nota que:

- `c = 42` y `c = -1` **no** generan warning — para C, un enum es un int
  y cualquier valor entero es valido
- `color = FRUIT_GREEN` genera warning con `-Wextra` porque mezcla dos
  tipos enum distintos, pero **compila** sin error

### Paso 4.4 — Ejecutar

```bash
./enum_limits
```

Salida esperada:

```
--- Enum es int ---
enum Color c = COLOR_RED;
int n = c;          -> n = 0
c = 42;             -> c = 42 (compila sin error)
c = -1;             -> c = -1 (compila sin error)

--- Aritmetica con enums ---
COLOR_RED + COLOR_GREEN + COLOR_BLUE = 3

--- Sin type safety ---
COLOR_RED = 0, FRUIT_RED = 0
Son distintos tipos pero ambos valen 0
color = FRUIT_GREEN; -> color = 1 (compila sin error en C)

--- Namespace global ---
COLOR_RED y FRUIT_RED coexisten porque tienen prefijos distintos.
Sin prefijo, dos enums con el mismo nombre causarian error.
```

### Paso 4.5 — Analisis

Las limitaciones de los enums en C:

1. **Son int** — `sizeof(enum X) == sizeof(int)`. No son un tipo distinto
   a nivel de binario
2. **No son type-safe** — puedes asignar cualquier entero sin error. El
   compilador no verifica que el valor sea un miembro valido del enum
3. **Se pueden mezclar** — un valor de `enum Fruit` se puede asignar a
   `enum Color`. Con `-Wextra` da warning, pero compila
4. **Namespace global** — los valores del enum (`COLOR_RED`, `FRUIT_RED`)
   viven en el scope global. Por eso se usan prefijos (`COLOR_`, `FRUIT_`)
   para evitar colisiones. Dos enums no pueden tener un miembro con el
   mismo nombre

Estas limitaciones son las que motivan los enum classes de C++ y los
enums de Rust, que son type-safe por diseno.

### Limpieza de la parte 4

```bash
rm -f enum_limits
```

---

## Limpieza final

```bash
rm -f enum_basics enum_switch enum_count enum_limits
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  enum_basics.c  enum_count.c  enum_limits.c  enum_switch.c
```

---

## Conceptos reforzados

1. Los valores de un enum se asignan automaticamente desde 0, incrementando
   en 1. Despues de un valor explicito, los automaticos continuan desde ahi.

2. `sizeof(enum X)` es igual a `sizeof(int)` en la mayoria de implementaciones
   — un enum es un int con nombres simbolicos.

3. Los designated initializers (`[COLOR_RED] = "red"`) garantizan que un array
   este sincronizado con el enum, independientemente del orden de los miembros.

4. Un switch sin `default` sobre un enum permite que `-Wswitch` avise si falta
   cubrir algun valor — esto es una ventaja sobre `#define`.

5. El patron `_COUNT` al final del enum provee automaticamente la cantidad de
   valores, permitiendo dimensionar arrays, iterar, y validar rangos sin
   hardcodear numeros.

6. Los enums de C no son type-safe: se puede asignar cualquier entero a una
   variable enum sin error, y se pueden mezclar enums de tipos distintos.

7. Los valores de un enum viven en el namespace global. Usar prefijos
   (`COLOR_`, `LOG_`) es la convencion para evitar colisiones entre enums.
