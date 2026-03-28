# Lab — sizeof y alineacion

## Objetivo

Observar el tamano en bytes de cada tipo primitivo, entender como el compilador
alinea datos en memoria, visualizar el padding en structs con `offsetof`, y
experimentar con `__attribute__((packed))`. Al finalizar, sabras predecir el
`sizeof` de un struct y optimizar su layout.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `sizeof_basics.c` | sizeof y alignof de tipos primitivos y arrays |
| `padding_inspect.c` | offsetof para visualizar padding en structs |
| `reorder_fields.c` | Comparacion de struct con campos reordenados |
| `packed_struct.c` | Structs packed vs normales, peligros de desalineacion |

---

## Parte 1 — sizeof de tipos primitivos y arrays

**Objetivo**: Conocer el tamano y la alineacion de cada tipo primitivo, y
entender sizeof con arrays.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat sizeof_basics.c
```

Observa que el programa imprime `sizeof` y `alignof` para cada tipo primitivo,
y luego muestra sizeof de arrays y el patron de conteo de elementos.

### Paso 1.2 — Predecir tamanos

Antes de compilar, predice:

- sizeof(char) es siempre 1 por definicion del estandar. Pero sizeof(int)?
- sizeof(long) y sizeof(long long) son iguales o diferentes en tu sistema?
- sizeof(char*) y sizeof(int*): son diferentes o iguales?
- Un `int arr[10]` ocupa sizeof(int) * 10 bytes. Cuanto es eso?
- `char str[] = "hello"` tiene 5 caracteres. sizeof(str) sera 5 o 6?
- alignof(double) sera igual a sizeof(double)?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic sizeof_basics.c -o sizeof_basics
./sizeof_basics
```

Salida esperada:

```
=== sizeof de tipos primitivos ===
char:        1 bytes
short:       2 bytes
int:         4 bytes
long:        ~8 bytes
long long:   8 bytes
float:       4 bytes
double:      8 bytes
long double: ~16 bytes
char*:       ~8 bytes
int*:        ~8 bytes
void*:       ~8 bytes

=== sizeof de arrays ===
int arr[10]:    40 bytes (10 * 4)
double darr[5]: 40 bytes (5 * 8)
char str[]:     6 bytes (incluye '\0')

=== Conteo de elementos ===
arr elements:  10
darr elements: 5

=== alignof de tipos ===
alignof(char):        1
alignof(short):       2
alignof(int):         4
alignof(long):        ~8
alignof(long long):   8
alignof(float):       4
alignof(double):      8
alignof(long double): ~16
```

Verifica tus predicciones:

- Todos los punteros tienen el mismo tamano (8 en sistemas de 64 bits)
- `sizeof("hello")` es 6 porque incluye el `'\0'` terminador
- `alignof` de cada tipo coincide con su `sizeof` (en la mayoria de plataformas)

### Limpieza de la parte 1

```bash
rm -f sizeof_basics
```

---

## Parte 2 — Padding y alineacion en structs

**Objetivo**: Usar `offsetof` para visualizar exactamente donde el compilador
inserta bytes de padding.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat padding_inspect.c
```

Observa las tres estructuras:

- `struct Bad`: char, int, char (orden que genera mucho padding)
- `struct Good`: int, char, char (mismos campos, menos padding)
- `struct Mixed`: cinco campos de distintos tamanos

### Paso 2.2 — Predecir el layout de struct Bad

`struct Bad` tiene: `char a` (1 byte) + `int b` (4 bytes) + `char c` (1 byte).
La suma de los campos es 6 bytes.

Antes de compilar, predice:

- `int b` necesita alineacion a 4 bytes. Si `a` ocupa el byte 0, en que
  offset quedara `b`?
- Cuantos bytes de padding habra entre `a` y `b`?
- El struct completo necesita que su tamano sea multiplo de la alineacion
  maxima de sus campos (4 bytes). Cuanto padding habra al final despues de `c`?
- Cual sera sizeof(struct Bad)?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic padding_inspect.c -o padding_inspect
./padding_inspect
```

Salida esperada:

```
=== struct Bad (char, int, char) ===
sizeof:  12 bytes
alignof: 4
  a: offset=0  size=1
  b: offset=4  size=4
  c: offset=8  size=1

=== struct Good (int, char, char) ===
sizeof:  8 bytes
alignof: 4
  b: offset=0  size=4
  a: offset=4  size=1
  c: offset=5  size=1

=== struct Mixed (char, double, char, int, short) ===
sizeof:  32 bytes
alignof: 8
  a: offset= 0  size=1
  b: offset= 8  size=8
  c: offset=16  size=1
  d: offset=20  size=4
  e: offset=24  size=2

=== Padding desperdiciado ===
Bad:   sizeof=12, suma de campos=6, padding=6 bytes
Mixed: sizeof=32, suma de campos=16, padding=16 bytes
```

### Paso 2.4 — Analizar el layout

Dibuja el mapa de memoria de `struct Bad`:

```
Offset:  0     1  2  3    4  5  6  7    8     9  10 11
        [a]  [pad pad pad][b  b  b  b]  [c]  [pad pad pad]
```

- Bytes 1-3: padding para alinear `b` a multiplo de 4
- Bytes 9-11: padding final para que sizeof sea multiplo de 4

Compara con `struct Good` (mismos campos, distinto orden):

```
Offset:  0  1  2  3    4     5     6  7
        [b  b  b  b]  [a]   [c]  [pad pad]
```

Solo 2 bytes de padding. Mismos datos, 4 bytes menos.

Para `struct Mixed`, observa como el `double b` (alineacion 8) fuerza 7 bytes
de padding despues de `char a`. La mitad del struct es padding.

### Limpieza de la parte 2

```bash
rm -f padding_inspect
```

---

## Parte 3 — Reordenar campos para minimizar padding

**Objetivo**: Aplicar la regla de "mayor a menor" para reducir el sizeof de un
struct sin perder campos.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat reorder_fields.c
```

Observa las dos versiones del struct:

- `struct Wasteful`: campos en orden arbitrario (char, double, char, int, char, short)
- `struct Optimized`: mismos campos ordenados de mayor a menor (double, int, short, char, char, char)

### Paso 3.2 — Predecir los tamanos

La suma real de todos los campos es: 8 + 4 + 2 + 1 + 1 + 1 = 17 bytes.

Antes de compilar, predice:

- sizeof(struct Wasteful): el primer `char` necesitara padding antes del
  `double`. Cuanto padding total habra?
- sizeof(struct Optimized): al ordenar de mayor a menor, cada campo se alinea
  naturalmente despues del anterior. Cuanto padding habra?
- En un array de 1000 structs, cuantos bytes se ahorran con la version
  optimizada?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic reorder_fields.c -o reorder_fields
./reorder_fields
```

Salida esperada:

```
=== struct Wasteful (orden arbitrario) ===
sizeof:  32 bytes
alignof: 8
  name:   offset= 0  size=1
  salary: offset= 8  size=8
  grade:  offset=16  size=1
  id:     offset=20  size=4
  dept:   offset=24  size=1
  level:  offset=26  size=2

=== struct Optimized (mayor a menor) ===
sizeof:  24 bytes
alignof: 8
  salary: offset= 0  size=8
  id:     offset= 8  size=4
  level:  offset=12  size=2
  name:   offset=14  size=1
  grade:  offset=15  size=1
  dept:   offset=16  size=1

=== Comparacion ===
Suma real de campos: 17 bytes
Wasteful:  32 bytes (padding: 15)
Optimized: 24 bytes (padding: 7)
Ahorro: 8 bytes por struct

=== En un array de 1000 elementos ===
Wasteful[1000]:  32000 bytes
Optimized[1000]: 24000 bytes
Ahorro total:    8000 bytes
```

### Paso 3.4 — Analizar la mejora

Observa los offsets de `struct Wasteful`:

- `name` (char) en offset 0, seguido de 7 bytes de padding antes de `salary`
- `grade` (char) en offset 16, seguido de 3 bytes de padding antes de `id`
- `dept` (char) en offset 24, seguido de 1 byte de padding antes de `level`
- 4 bytes de padding final para redondear a multiplo de 8

En `struct Optimized`, los campos encajan mucho mejor:

- `salary` (double, 8 bytes) en offset 0
- `id` (int, 4 bytes) en offset 8 (alineado naturalmente)
- `level` (short, 2 bytes) en offset 12 (alineado naturalmente)
- `name`, `grade`, `dept` (chars) consecutivos sin padding entre ellos
- Solo 7 bytes de padding al final para redondear a multiplo de 8

La regla "mayor a menor" redujo el struct de 32 a 24 bytes. En un array de
1000 elementos, eso son 8000 bytes menos (25% de ahorro).

### Limpieza de la parte 3

```bash
rm -f reorder_fields
```

---

## Parte 4 — __attribute__((packed)) y sus consecuencias

**Objetivo**: Eliminar todo el padding con `__attribute__((packed))` y entender
por que no siempre es buena idea.

### Paso 4.1 — Examinar el codigo fuente

```bash
cat packed_struct.c
```

Observa:

- `struct Normal` y `struct Packed` tienen los mismos campos
- `struct Packed` usa `__attribute__((packed))`
- `struct Header` y `struct PackedHeader` simulan un header de protocolo binario
- Al final, se muestra la direccion de un campo desalineado

### Paso 4.2 — Predecir el efecto de packed

Antes de compilar, predice:

- sizeof(struct Normal) ya lo vimos en la parte 2: es 12 bytes.
  sizeof(struct Packed) sin ningun padding sera...?
- En struct Packed, el campo `b` (int) empezara en offset 1 (justo despues de
  `a`). Esa direccion sera multiplo de 4?
- struct Header tiene 5 campos que suman 17 bytes.
  sizeof(struct Header) con padding normal sera...?
  sizeof(struct PackedHeader) sera exactamente 17?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic packed_struct.c -o packed_struct
./packed_struct
```

Salida esperada:

```
=== Normal vs Packed ===
struct Normal: 12 bytes
  a: offset=0
  b: offset=4
  c: offset=8

struct Packed: 6 bytes
  a: offset=0
  b: offset=1
  c: offset=5

=== Caso practico: Header de protocolo ===
struct Header (normal):  32 bytes
  magic:     offset= 0  size=2
  version:   offset= 4  size=4
  flags:     offset= 8  size=2
  timestamp: offset=16  size=8
  type:      offset=24  size=1

struct PackedHeader:     17 bytes
  magic:     offset= 0  size=2
  version:   offset= 2  size=4
  flags:     offset= 6  size=2
  timestamp: offset= 8  size=8
  type:      offset=16  size=1

Suma real de campos: 17 bytes
Header normal:       32 bytes (padding: 15)
PackedHeader:        17 bytes (padding: 0)

=== Peligro: puntero a campo desalineado ===
p.b = 42 (acceso directo: OK)
Direccion de p.a:   0x<addr>
Direccion de p.b:   0x<addr>
Direccion de p.c:   0x<addr>
p.b esta en direccion multiplo de 4? No
```

### Paso 4.4 — Analizar las consecuencias

Observa los puntos clave:

1. **struct Packed es exactamente 6 bytes** (1 + 4 + 1, sin padding). El campo
   `b` esta en offset 1, que NO es multiplo de 4. En x86-64 esto funciona pero
   puede ser mas lento. En ARM, podria causar un SIGBUS (crash).

2. **PackedHeader es exactamente 17 bytes** (la suma exacta de los campos).
   Esto es necesario para protocolos de red o formatos de archivo binario donde
   cada byte debe estar en la posicion exacta.

3. **Nunca tomar la direccion de un campo packed como puntero de su tipo**.
   Si haces `int *ptr = &p.b` y el campo esta desalineado, usarlo puede causar
   comportamiento indefinido en arquitecturas estrictas.

### Paso 4.5 — Cuando usar packed

Usar `__attribute__((packed))` SOLO cuando:

- Necesitas que el layout binario coincida exactamente con un protocolo de red
- Necesitas leer/escribir un formato de archivo binario campo por campo
- Trabajas con registros de hardware mapeados en memoria

Para structs de uso general, es mejor **reordenar campos** (parte 3) en lugar
de usar packed.

### Limpieza de la parte 4

```bash
rm -f packed_struct
```

---

## Limpieza final

```bash
rm -f sizeof_basics padding_inspect reorder_fields packed_struct
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  padding_inspect.c  packed_struct.c  reorder_fields.c  sizeof_basics.c
```

---

## Conceptos reforzados

1. `sizeof` retorna el tamano en bytes de un tipo o expresion. Para arrays,
   retorna el tamano total (no el numero de elementos). Para obtener el conteo
   se usa `sizeof(arr) / sizeof(arr[0])`.

2. `alignof` (C11) retorna el requisito de alineacion de un tipo. La CPU
   accede a la memoria de forma mas eficiente cuando los datos estan en
   direcciones que son multiplo de su alineacion.

3. El compilador inserta bytes de **padding** entre campos de un struct para
   cumplir con los requisitos de alineacion. Tambien agrega padding al final
   para que `sizeof` sea multiplo de la alineacion maxima del struct.

4. `offsetof(tipo, campo)` revela la posicion exacta de cada campo, haciendo
   visible el padding que el compilador inserta.

5. Reordenar los campos de un struct de **mayor a menor** tamano minimiza el
   padding. Con los mismos datos, un struct puede reducirse significativamente
   (de 32 a 24 bytes en el ejemplo del lab).

6. `__attribute__((packed))` elimina todo el padding, forzando los campos a
   posiciones consecutivas. Esto puede causar accesos desalineados que son mas
   lentos en x86 y pueden crashear en ARM. Solo usarlo para protocolos de red,
   formatos binarios, o hardware mapeado en memoria.

7. La diferencia de padding se amplifica en arrays: un struct con 8 bytes extra
   de padding desperdicia 8 KB en un array de 1000 elementos.
