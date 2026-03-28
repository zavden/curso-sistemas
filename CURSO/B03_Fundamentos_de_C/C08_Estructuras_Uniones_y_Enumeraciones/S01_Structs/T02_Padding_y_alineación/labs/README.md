# Lab -- Padding y alineacion (avanzado)

## Objetivo

Profundizar en las reglas de alineacion que determinan el layout de structs en
memoria: visualizar el padding byte por byte, entender las tres reglas
(alineacion de campo, alineacion de struct, tail padding), usar
`__attribute__((packed))` con protocolos de red reales, y controlar la
alineacion con `_Alignas` y cache line alignment. Al finalizar, sabras predecir
el layout exacto de cualquier struct y elegir cuando usar packed, cuando
reordenar campos, y cuando forzar sobre-alineacion.

Nota: el lab de C02/S01/T03 cubre sizeof basico, offsetof introductorio y
packed elemental. Este lab profundiza en las reglas detalladas, mapeo byte a
byte, `_Static_assert` para validar layouts, packing selectivo, `_Alignas`,
y alineacion a cache line.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `padding_map.c` | Mapa byte por byte de structs Bad vs Good con memset |
| `layout_rules.c` | Las tres reglas de alineacion con structs de ejemplo |
| `packed_deep.c` | packed con protocolo de red, static_assert, packing selectivo |
| `alignas_demo.c` | _Alignas, _Alignof, cache line alignment, false sharing |

---

## Parte 1 -- Visualizar padding con offsetof (Bad vs Good)

**Objetivo**: Ver byte por byte donde el compilador inserta padding, y como
reordenar campos elimina el desperdicio.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat padding_map.c
```

Observa las dos estructuras:

- `struct Bad`: char, double, char, int, short (orden arbitrario)
- `struct Good`: double, int, short, char, char (ordenado de mayor a menor)

El programa usa `memset` con `0xAA` para llenar la memoria antes de asignar
los campos. Los bytes que conservan `0xAA` son padding; los que cambian son
datos reales.

### Paso 1.2 -- Predecir el layout de struct Bad

Los campos de `struct Bad` suman 16 bytes (1 + 8 + 1 + 4 + 2).

Antes de compilar, predice:

- `char a` esta en offset 0. `double b` necesita alineacion a 8. En que
  offset empezara `b`? Cuantos bytes de padding habra entre `a` y `b`?
- Despues de `b` (offset 8, 8 bytes), `char c` puede ir en cualquier offset.
  Donde quedara `c`?
- `int d` necesita alineacion a 4. Si `c` esta en offset 16, donde empezara `d`?
- `short e` necesita alineacion a 2. Donde empezara `e`?
- El struct completo tiene alineacion 8 (por el double). sizeof debe ser
  multiplo de 8. Cuanto tail padding habra?
- Cual sera sizeof(struct Bad)?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic padding_map.c -o padding_map
./padding_map
```

Salida esperada (el mapa hexadecimal puede variar en los valores exactos):

```
=== Mapa de memoria byte por byte ===

struct Bad (32 bytes):
  Offset:   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 ...
  Hex:     58 AA AA AA AA AA AA AA 00 00 00 00 00 00 00 00 ...
  Tipo:     D  .  .  .  .  .  .  .  D  D  D  D  D  D  D  D ...
  (D = dato, . = padding)

Detalle de struct Bad:
  a:  offset= 0  size=1
  b:  offset= 8  size=8
  c:  offset=16  size=1
  d:  offset=20  size=4
  e:  offset=24  size=2
  Suma campos: 16  padding total: 16
```

Observa: **16 bytes de datos reales, 16 bytes de padding**. La mitad del struct
es desperdicio.

### Paso 1.4 -- Analizar struct Good

Mira la seccion de `struct Good` en la salida:

```
struct Good (16 bytes):
  Offset:   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
  Hex:     00 00 00 00 00 00 00 00 01 00 00 00 02 00 58 59
  Tipo:     D  D  D  D  D  D  D  D  D  D  D  D  D  D  D  D
  (D = dato, . = padding)
```

Cero padding. Los 16 bytes son datos reales. Mismos campos, la mitad de tamano.

### Paso 1.5 -- Impacto en arrays

Observa la comparacion final:

```
En un array de 100000 structs:
  Bad[100000]:  3200000 bytes (~3125 KB)
  Good[100000]: 1600000 bytes (~1562 KB)
  Ahorro: 1600000 bytes (~1562 KB)
```

En una tabla de 100000 registros, la diferencia es 1.5 MB. En un sistema
embebido con 8 MB de RAM, eso puede ser la diferencia entre funcionar o no.

### Limpieza de la parte 1

```bash
rm -f padding_map
```

---

## Parte 2 -- Reglas de alineacion: campo, struct, tail padding

**Objetivo**: Entender las tres reglas que el compilador aplica para determinar
offsets y sizeof.

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat layout_rules.c
```

Observa las tres secciones:

- `struct RuleDemo1`: mezcla de char, short, char, int (demuestra regla 1)
- `struct RuleDemo2`: char, double, int (demuestra regla 2)
- `struct TailPad` y `struct NoTailPad` (demuestra regla 3)

### Paso 2.2 -- Predecir con la regla 1

`struct RuleDemo1 { char a; short b; char c; int d; }`

Aplica la regla: cada campo se alinea a un multiplo de su tamano natural.

- `a` (char, align 1): offset 0.
- `b` (short, align 2): el siguiente offset disponible es 1, pero 1 no es
  par. Necesita offset 2. Padding: 1 byte.
- `c` (char, align 1): despues de `b` (offset 2 + 2 = 4). Offset 4.
- `d` (int, align 4): despues de `c` (offset 4 + 1 = 5). 5 no es multiplo
  de 4. Siguiente multiplo: 8. Padding: 3 bytes.

sizeof sera multiplo de 4 (mayor alineacion). Offset 8 + 4 = 12. 12 es
multiplo de 4. sizeof = 12.

Verifica tu prediccion antes de continuar.

### Paso 2.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic layout_rules.c -o layout_rules
./layout_rules
```

Salida esperada (seccion de regla 1):

```
=== Regla 1: cada campo alineado a su tamano ===

struct RuleDemo1 { char a; short b; char c; int d; }
sizeof:  12 bytes
alignof: 4

  a: offset=0  size=1  (align 1 -- cualquier offset)
  b: offset=2  size=2  (align 2 -- necesita offset par)
  c: offset=4  size=1  (align 1)
  d: offset=8  size=4  (align 4 -- necesita multiplo de 4)
```

Nota como `b` salto de offset 1 a offset 2 (1 byte de padding), y `d` salto
de offset 5 a offset 8 (3 bytes de padding).

### Paso 2.4 -- Analizar la regla 2

Observa la seccion de `struct RuleDemo2`:

```
=== Regla 2: alineacion del struct = mayor miembro ===

struct RuleDemo2 { char a; double b; int c; }
sizeof:  24 bytes
alignof: 8  (double es el miembro con mayor alineacion)
```

El `double b` (alineacion 8) fuerza 7 bytes de padding entre `a` y `b`.
La alineacion del struct completo es 8, no 4, porque el miembro mas exigente
determina la alineacion del todo.

### Paso 2.5 -- Analizar la regla 3 (tail padding)

Observa la seccion de tail padding:

```
struct TailPad { double d; int i; char c; }
sizeof:  16 bytes  (alineacion 8, necesita multiplo de 8)
  d: offset=0   size=8
  i: offset=8   size=4
  c: offset=12  size=1
Suma de campos: 13  --  tail padding: 3 bytes
```

Los campos terminan en el byte 12 (offset 12 + 1 = 13 bytes). Pero sizeof
debe ser multiplo de 8. El siguiente multiplo de 8 despues de 13 es 16. El
compilador agrega 3 bytes de tail padding.

Ahora mira la explicacion de por que:

```
Sin tail padding, arr[1].d empezaria en offset 13,
que NO es multiplo de 8 -- violaria la alineacion del double.
```

El tail padding existe para que los arrays funcionen correctamente. Si `arr[0]`
tuviera sizeof 13, `arr[1]` empezaria en offset 13, y su campo `double d`
(que necesita alineacion 8) estaria desalineado.

### Limpieza de la parte 2

```bash
rm -f layout_rules
```

---

## Parte 3 -- packed: protocolo de red y packing selectivo

**Objetivo**: Usar `__attribute__((packed))` para definir un header de
protocolo de red con layout exacto, validar el layout con `_Static_assert`,
serializar/deserializar con `memcpy`, y aplicar packing selectivo.

### Paso 3.1 -- Examinar el codigo fuente

```bash
cat packed_deep.c
```

Observa:

- `struct SensorNormal` y `struct SensorPacked`: mismos campos, diferente
  layout
- `struct NetHeader`: header de protocolo packed con `_Static_assert` que
  valida los offsets
- `struct Selective`: un solo campo packed, el resto con alineacion normal

### Paso 3.2 -- Predecir el efecto

`struct SensorPacked { uint8_t id; uint32_t timestamp; uint16_t value; uint8_t status; }`

La suma de campos es 1 + 4 + 2 + 1 = 8 bytes.

Antes de compilar, predice:

- sizeof(struct SensorNormal) con padding normal: `id` en 0, `timestamp`
  necesita alineacion 4 asi que offset 4, `value` en offset 8, `status`
  en offset 10. sizeof multiplo de 4 = ?
- sizeof(struct SensorPacked): sin padding = ?
- En struct SensorPacked, `timestamp` empieza en offset 1 (justo despues de
  `id`). 1 es multiplo de 4? Que consecuencia tiene eso?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic packed_deep.c -o packed_deep
./packed_deep
```

Salida esperada:

```
=== Normal vs Packed: struct Sensor ===

SensorNormal: 12 bytes  (alignof 4)
  id:        offset=0  size=1
  timestamp: offset=4  size=4
  value:     offset=8  size=2
  status:    offset=10  size=1

SensorPacked: 8 bytes  (alignof 1)
  id:        offset=0  size=1
  timestamp: offset=1  size=4
  value:     offset=5  size=2
  status:    offset=7  size=1

Suma real de campos: 8 bytes
Normal desperdicia: 4 bytes de padding
Packed desperdicia: 0 bytes de padding
```

Nota que `alignof(SensorPacked)` es 1 -- el struct packed pierde su requisito
de alineacion natural. En x86-64 funciona, pero en ARM estricto un acceso
desalineado al `uint32_t timestamp` en offset 1 puede causar SIGBUS.

### Paso 3.4 -- Validacion con static_assert

Observa la seccion del protocolo de red:

```
=== Protocolo de red: layout exacto con packed ===

NetHeader: 12 bytes
  version:  offset=0  size=1
  type:     offset=1  size=1
  length:   offset=2  size=2
  sequence: offset=4  size=4
  checksum: offset=8  size=4
```

En el codigo fuente, cada offset esta validado con `_Static_assert`. Si
alguien cambia el orden de los campos o agrega uno nuevo, la compilacion
falla con un mensaje claro. Esto es una practica esencial en protocolos de
red donde cada byte debe estar en la posicion exacta.

### Paso 3.5 -- Serializacion con memcpy

Observa la serializacion byte por byte:

```
Serializacion con memcpy (byte por byte):
  byte[ 0] = 0x01
  byte[ 1] = 0x02
  byte[ 2] = 0x40
  byte[ 3] = 0x00
  ...
```

Con un struct packed, `memcpy` copia el layout exacto a un buffer de bytes.
Esto es lo que se envia por la red o se escribe en un archivo binario.

Nota: `length = 64` se ve como `0x40 0x00` (little-endian). En protocolos
de red reales, hay que convertir a network byte order con `htons`/`htonl`.
Este lab se enfoca en el layout del struct, no en byte order.

### Paso 3.6 -- Packing selectivo

Observa la ultima seccion:

```
=== Packing selectivo: solo un campo ===

struct Selective { char tag; int value(packed); double score; }
sizeof:  16 bytes  (alignof 8)
  tag:   offset=0  size=1
  value: offset=1  size=4  (packed: sin padding previo)
  score: offset=8  size=8
```

Solo `value` esta packed (sin los 3 bytes de padding entre `tag` y `value`).
El `double score` mantiene su alineacion natural a 8. Esta tecnica permite
ahorrar bytes en campos especificos sin comprometer la alineacion de los
demas.

### Paso 3.7 -- Probar que static_assert funciona

Modifica temporalmente el struct `NetHeader` en el codigo fuente para
romper el layout. Por ejemplo, cambia `uint16_t length` a `uint32_t length`:

```bash
cp packed_deep.c packed_deep_broken.c
```

Edita `packed_deep_broken.c` y cambia la linea `uint16_t length;` a
`uint32_t length;`. Luego intenta compilar:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic packed_deep_broken.c -o packed_deep_broken
```

Salida esperada (error):

```
packed_deep_broken.c: ... error: static assertion failed: "NetHeader must be exactly 12 bytes"
packed_deep_broken.c: ... error: static assertion failed: "sequence at 4"
```

El `_Static_assert` detecto que el layout cambio y rechazo la compilacion.
Sin el assert, el programa compilaria pero produciria datos corruptos al
comunicarse con otros sistemas que esperan el formato original.

### Limpieza de la parte 3

```bash
rm -f packed_deep packed_deep_broken.c
```

---

## Parte 4 -- _Alignas, _Alignof, y alineacion a cache line

**Objetivo**: Usar `_Alignas`/`alignas` para forzar alineacion mayor que la
natural, alinear structs a cache line (64 bytes), y entender la prevencion
de false sharing.

### Paso 4.1 -- Examinar el codigo fuente

```bash
cat alignas_demo.c
```

Observa:

- `struct BasicAligned`: usa `alignas(16)` en un campo `int[4]`
- `struct CacheAligned`: usa `__attribute__((aligned(64)))` para alinear a
  cache line
- `struct Counter`: un int que ocupa un cache line completo (prevencion de
  false sharing)
- `struct MemberAligned`: `alignas(16)` aplicado a un miembro individual

### Paso 4.2 -- Predecir el efecto de alignas

Antes de compilar, predice:

- `alignof(max_align_t)`: esta es la alineacion maxima que `malloc` garantiza.
  En x86-64 con glibc, es tipicamente 16 bytes. Sera mayor o menor?
- `struct BasicAligned { alignas(16) int data[4]; }`: `int[4]` son 16 bytes.
  Con alignas(16), sizeof cambiara o sera igual? Y alignof?
- `struct CacheAligned (aligned(64))`: sizeof sera 64 bytes (un cache line
  completo). La direccion de una instancia sera multiplo de 64?
- `struct Counter counters[2]`: si cada Counter ocupa 64 bytes, la distancia
  entre `counters[0]` y `counters[1]` sera exactamente 64?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic alignas_demo.c -o alignas_demo
./alignas_demo
```

Salida esperada:

```
=== _Alignof / alignof ===

alignof(char):        1
alignof(short):       2
alignof(int):         4
alignof(double):      8
alignof(long double): 16
alignof(void *):      8
alignof(max_align_t): 16
```

`max_align_t` es el tipo con la alineacion mas grande del sistema. Todo lo que
`malloc` retorna esta alineado al menos a `alignof(max_align_t)`. Valores
mayores (como 32 o 64) requieren `aligned_alloc` o `posix_memalign`.

### Paso 4.4 -- Alineacion a cache line

Observa la seccion de cache line:

```
=== Alineacion a cache line (64 bytes) ===

struct CacheAligned (aligned(64)):
sizeof:  64 bytes
alignof: 64
Direccion de obj: 0x<addr>
Multiplo de 64?   Si
```

La direccion de `obj` es siempre multiplo de 64. Esto garantiza que el struct
completo vive dentro de un solo cache line (64 bytes en la mayoria de CPUs
x86-64).

### Paso 4.5 -- Prevencion de false sharing

Observa la seccion de false sharing:

```
=== Prevencion de false sharing ===

sizeof(Counter): 64 bytes (ocupa un cache line completo)
counters[0] en: 0x<addr0>
counters[1] en: 0x<addr1>
Distancia: 64 bytes
Cada counter en cache line separado? Si
```

En un programa multithreaded, si dos hilos modifican variables que estan en
el mismo cache line, la CPU invalida el cache del otro core en cada escritura
(false sharing). Esto degrada el rendimiento drasticamente.

Al alinear cada `Counter` a 64 bytes, garantizamos que `counters[0]` y
`counters[1]` viven en cache lines diferentes. El hilo 0 puede modificar
`counters[0].value` sin invalidar el cache del hilo 1.

### Paso 4.6 -- alignas en un miembro

Observa la seccion de MemberAligned:

```
=== alignas en un miembro ===

struct MemberAligned { char tag; alignas(16) int vector[4]; char flag; }
sizeof:  48 bytes
alignof: 16
  tag:    offset=0   size=1
  vector: offset=16  size=16  (forzado a multiplo de 16)
  flag:   offset=32  size=1
```

`alignas(16)` en `vector` fuerza 15 bytes de padding entre `tag` (offset 0)
y `vector` (offset 16). El alignof del struct sube a 16 (el mayor de todos
sus miembros), y sizeof sube a 48 (multiplo de 16).

Esto es util para datos que necesitan alineacion SIMD (SSE requiere 16 bytes,
AVX requiere 32 bytes).

### Paso 4.7 -- alignas en variables locales

Observa la ultima seccion:

```
=== Variable local con alignas ===

aligned_buffer (alignas(32)):
Direccion: 0x<addr>
Multiplo de 32? Si
```

`alignas` tambien funciona con variables locales (stack). El compilador
ajusta el stack pointer para cumplir con la alineacion solicitada.

### Limpieza de la parte 4

```bash
rm -f alignas_demo
```

---

## Limpieza final

```bash
rm -f padding_map layout_rules packed_deep alignas_demo packed_deep_broken.c
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  alignas_demo.c  layout_rules.c  packed_deep.c  padding_map.c
```

---

## Conceptos reforzados

1. El compilador inserta padding para cumplir la **regla 1** (cada campo se
   alinea a un multiplo de su tamano natural). Un `char` seguido de un
   `double` genera 7 bytes de padding entre ambos.

2. La **regla 2** (alineacion del struct = mayor miembro) determina el
   requisito de alineacion del struct completo. Si el struct contiene un
   `double`, todo el struct se alinea a 8.

3. El **tail padding** (regla 3) garantiza que `sizeof` sea multiplo de la
   alineacion del struct. Sin tail padding, los arrays de structs violarian
   la alineacion de los campos internos.

4. Un mapa byte a byte con `memset(0xAA)` revela exactamente que bytes son
   datos y cuales son padding. En el peor caso, el 50% de un struct puede
   ser desperdicio.

5. `__attribute__((packed))` elimina todo el padding, pero genera accesos
   desalineados que son lentos en x86-64 y pueden causar SIGBUS en ARM.
   Usarlo solo para protocolos de red, formatos binarios, o hardware.

6. `_Static_assert` con `sizeof` y `offsetof` valida el layout en tiempo de
   compilacion. Si alguien modifica el struct, la compilacion falla con un
   mensaje claro en lugar de producir datos corruptos silenciosamente.

7. `_Alignas`/`alignas` fuerza alineacion **mayor** que la natural. Alinear
   a 64 bytes (cache line) previene false sharing en programas multithreaded
   al garantizar que datos independientes vivan en cache lines separados.

8. El packing selectivo (`int value __attribute__((packed))`) permite
   comprimir un campo individual sin afectar la alineacion de los demas
   miembros del struct.
