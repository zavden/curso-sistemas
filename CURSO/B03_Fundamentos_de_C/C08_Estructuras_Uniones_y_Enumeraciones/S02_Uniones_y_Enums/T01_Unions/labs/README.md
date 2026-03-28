# Lab -- Unions

## Objetivo

Explorar como las unions comparten memoria, implementar el patron tagged union
para despacho seguro, usar type punning para inspeccionar la representacion
binaria de datos, y comparar el uso de memoria entre union y struct. Al
finalizar, sabras cuando y por que usar una union en lugar de un struct.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `shared_memory.c` | Union basica: offsets, sizeof, sobrescritura de miembros |
| `tagged_union.c` | Tagged union con enum + switch para despacho seguro |
| `type_punning.c` | Reinterpretar bits de float, deteccion de endianness |
| `union_vs_struct.c` | Comparacion de tamano entre union y struct |

---

## Parte 1 -- Memoria compartida

**Objetivo**: Verificar que todos los miembros de una union empiezan en el
mismo offset y que el sizeof de la union es el de su miembro mas grande.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat shared_memory.c
```

Observa la union `Value` con cuatro miembros (`int`, `float`, `char`, `double`)
y el struct `ValueStruct` con los mismos miembros. El programa imprime sizeof
y offsets de cada uno.

### Paso 1.2 -- Predecir los tamanos

Antes de compilar, responde mentalmente:

- `sizeof(int)` en un sistema de 64 bits tipicamente es 4 u 8?
- `sizeof(double)` es mayor que `sizeof(float)`. Cuanto mide cada uno?
- Si la union toma el tamano de su miembro mas grande, cual sera `sizeof(union Value)`?
- Y `sizeof(struct ValueStruct)` sera mayor o menor que la union? Por cuanto?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic shared_memory.c -o shared_memory
./shared_memory
```

Salida esperada:

```
=== sizeof each member ===
sizeof(int)    = 4
sizeof(float)  = 4
sizeof(char)   = 1
sizeof(double) = 8

=== sizeof union vs struct ===
sizeof(union Value)       = 8
sizeof(struct ValueStruct) = 24

=== offsets inside the union ===
offset of i = 0
offset of f = 0
offset of c = 0
offset of d = 0

=== writing to one member overwrites others ===
wrote v.i = 42
  v.i = 42
wrote v.f = 3.14
  v.f = 3.140000
  v.i = 1078523331  (garbage -- overwritten by v.f)
```

Verifica tus predicciones:

- La union mide 8 bytes (el tamano de `double`, su miembro mas grande).
- El struct mide 24 bytes (todos los miembros con su propio espacio + padding).
- Todos los offsets de la union son 0 -- todos los miembros empiezan en la
  misma direccion de memoria.
- Escribir `v.f = 3.14f` sobrescribio los bytes de `v.i`. El valor 1078523331
  es la representacion IEEE 754 de 3.14f interpretada como entero.

### Limpieza de la parte 1

```bash
rm -f shared_memory
```

---

## Parte 2 -- Tagged union

**Objetivo**: Implementar el patron tagged union donde un enum indica que
miembro de la union es valido, y usar switch para despacho seguro.

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat tagged_union.c
```

Observa:

- El enum `ValueType` con cuatro variantes: `VAL_INT`, `VAL_FLOAT`,
  `VAL_STRING`, `VAL_BOOL`.
- El struct `TaggedValue` que combina el enum (tag) con la union (datos).
- La funcion `print_value()` que usa switch sobre el tag para acceder al
  miembro correcto.
- Las funciones constructoras (`make_int`, `make_float`, etc.) que garantizan
  que el tag y el dato son coherentes.

### Paso 2.2 -- Predecir el sizeof

Antes de compilar, responde mentalmente:

- La union dentro de `TaggedValue` tiene miembros: `int` (4), `float` (4),
  `char[32]` (32), `_Bool` (1). Cual domina el tamano?
- El struct tiene el enum (tag) + la union. Cuanto medira el struct completo
  considerando alignment?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic tagged_union.c -o tagged_union
./tagged_union
```

Salida esperada:

```
=== tagged union: safe dispatch ===
[0] int: 42
[1] float: 3.14
[2] string: "hello unions"
[3] bool: true
[4] int: -7

=== sizeof analysis ===
sizeof(enum ValueType)    = 4
sizeof(union data)        = 32
sizeof(struct TaggedValue) = 36
```

Verifica tus predicciones:

- La union mide 32 bytes porque `char s[32]` es el miembro mas grande.
- El struct mide 36 bytes: 4 (enum) + 32 (union) = 36. En este caso no hay
  padding extra porque el enum ya tiene 4 bytes de alignment.
- El switch en `print_value()` despacha correctamente segun el tag. Este es el
  patron estandar para trabajar con unions de forma segura en C.

### Paso 2.4 -- Reflexion: que pasa sin el tag?

Sin el enum tag, no hay forma de saber que miembro de la union fue el ultimo
escrito. Leer el miembro incorrecto produce basura. El tag es la unica
proteccion que el programador tiene en C -- el compilador no verifica nada.

En Rust, los enums con datos son tagged unions con verificacion del compilador:
el `match` obliga a cubrir todas las variantes.

### Limpieza de la parte 2

```bash
rm -f tagged_union
```

---

## Parte 3 -- Type punning

**Objetivo**: Usar unions para reinterpretar la representacion binaria de un
float y para detectar el endianness de la plataforma.

### Paso 3.1 -- Examinar el codigo fuente

```bash
cat type_punning.c
```

Observa:

- `union FloatBits`: permite leer los 4 bytes de un `float` como `uint32_t`.
- `union IntBytes`: permite acceder a los bytes individuales de un `uint32_t`.
- El programa descompone un float en signo, exponente y mantisa (IEEE 754).
- La deteccion de endianness se basa en el orden de los bytes en memoria.

### Paso 3.2 -- Predecir el endianness

Antes de compilar, responde mentalmente:

- Si `ib.value = 0x04030201`, el byte menos significativo es `0x01`. En
  little-endian, ese byte va en la direccion mas baja (bytes[0]). En
  big-endian, va en la mas alta (bytes[3]).
- Tu maquina x86-64 es little-endian o big-endian?
- Para el float `3.14f`, el bit de signo sera 0 o 1?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic type_punning.c -o type_punning
./type_punning
```

Salida esperada (en x86-64, little-endian):

```
=== float bit representation ===
float 3.14 as hex: 0x4048F5C3
  sign     = 0
  exponent = 128 (biased, subtract 127 -> 1)
  mantissa = 0x48F5C3

=== endianness check ===
value = 0x04030201
byte[0] = 0x01
byte[1] = 0x02
byte[2] = 0x03
byte[3] = 0x04
result: little-endian (least significant byte first)

=== negative float ===
float -3.14 as hex: 0xC048F5C3
  sign bit = 1 (1 = negative)

=== positive vs negative comparison ===
+3.14f -> 0x4048F5C3
-3.14f -> 0xC048F5C3
XOR    -> 0x80000000 (only sign bit differs)
```

Verifica tus predicciones:

- x86-64 es little-endian: `bytes[0]` contiene `0x01` (byte menos significativo).
- El signo de `3.14f` es 0 (positivo). El de `-3.14f` es 1.
- El XOR entre positivo y negativo es `0x80000000` -- solo difiere el bit 31
  (el bit de signo en IEEE 754).

### Paso 3.4 -- Sobre la legalidad del type punning

En C99+, leer un miembro de la union diferente al ultimo escrito esta definido
como reinterpretacion de los bytes. Este es el mecanismo idiomatico para type
punning en C.

En C++, este mismo uso es undefined behavior. La alternativa portátil (valida
en C y C++) es `memcpy`:

```c
uint32_t bits;
memcpy(&bits, &some_float, sizeof(bits));
```

### Limpieza de la parte 3

```bash
rm -f type_punning
```

---

## Parte 4 -- Union vs struct

**Objetivo**: Comparar el uso de memoria entre un struct que reserva espacio
para todos los campos y una union que comparte la memoria, y entender cuando
conviene cada enfoque.

### Paso 4.1 -- Examinar el codigo fuente

```bash
cat union_vs_struct.c
```

Observa las dos versiones del mismo tipo `Event`:

- `EventStruct`: tiene todos los campos siempre (click x/y, key, resize w/h).
  Solo uno se usa a la vez, pero todos ocupan espacio.
- `EventUnion`: usa una tagged union. Solo el campo relevante ocupa espacio,
  los demas comparten la misma memoria.

### Paso 4.2 -- Predecir el ahorro

Antes de compilar, responde mentalmente:

- `EventStruct` tiene: `int type` + `int x, y` + `char key` + `int width, height`.
  Cuantos bytes ocupa (considerando alignment)?
- `EventUnion` tiene: `int type` + union del mayor miembro. El miembro mas
  grande de la union tiene dos `int` (click o resize). Cuantos bytes ocupa?
- Con 1000 eventos, cuantos bytes se ahorran?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic union_vs_struct.c -o union_vs_struct
./union_vs_struct
```

Salida esperada (los tamanos pueden variar por padding):

```
=== memory comparison ===
sizeof(struct EventStruct) = 24 bytes
sizeof(struct EventUnion)  = 12 bytes
savings per event: 12 bytes

with 1000 events:
  struct: 24000 bytes
  union:  12000 bytes
  saved:  12000 bytes

=== tagged union in action ===
event[0]:
  click at (100, 200)
event[1]:
  key pressed: 'q'
event[2]:
  resize to 1920x1080
```

Verifica tus predicciones:

- El struct ocupa 24 bytes (con padding para alignment).
- La union ocupa 12 bytes: 4 (tag) + 8 (la union del miembro mas grande).
- Con 1000 eventos se ahorran 12000 bytes. En estructuras con variantes mas
  complejas, el ahorro puede ser mucho mayor.

### Paso 4.4 -- Cuando usar cada uno

Regla practica:

- **Struct**: cuando necesitas todos los campos al mismo tiempo. Cada miembro
  tiene su propio espacio.
- **Union**: cuando solo un campo es valido a la vez. Los miembros comparten
  espacio. Siempre combinala con un tag (tagged union) para saber cual es el
  campo activo.

El coste de la union es la responsabilidad del programador: en C no hay
verificacion automatica de que el tag y el dato sean coherentes. Un bug en el
tag produce lecturas de basura silenciosas.

### Limpieza de la parte 4

```bash
rm -f union_vs_struct
```

---

## Limpieza final

```bash
rm -f shared_memory tagged_union type_punning union_vs_struct
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  shared_memory.c  tagged_union.c  type_punning.c  union_vs_struct.c
```

---

## Conceptos reforzados

1. Todos los miembros de una union comparten la misma direccion de memoria
   (offset 0). El sizeof de la union es el de su miembro mas grande.

2. Escribir en un miembro de la union sobrescribe a todos los demas. Solo el
   ultimo miembro escrito tiene un valor valido.

3. El patron tagged union (struct con enum tag + union) es la forma estandar de
   usar unions de forma segura en C. El switch sobre el tag garantiza que se
   accede al miembro correcto.

4. Type punning con unions es legal en C99+ y permite reinterpretar los bytes
   de un tipo como otro. Es la forma idiomatica de inspeccionar la
   representacion binaria de floats o detectar endianness.

5. Las unions ahorran memoria significativa cuando solo un campo es valido a la
   vez. Con arrays grandes, la diferencia entre struct y union puede ser de
   50% o mas.

6. En C, la seguridad de una tagged union depende enteramente del programador.
   En Rust, los enums con datos proporcionan la misma funcionalidad con
   verificacion del compilador.
