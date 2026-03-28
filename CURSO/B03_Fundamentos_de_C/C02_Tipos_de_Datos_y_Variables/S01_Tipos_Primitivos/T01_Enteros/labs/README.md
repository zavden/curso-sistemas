# Lab -- Enteros

## Objetivo

Explorar los tipos enteros de C en tu plataforma: verificar sus tamanos con
`sizeof`, consultar sus rangos con `limits.h`, usar los tipos de ancho fijo de
`stdint.h`, e imprimir valores con los format specifiers portables de
`inttypes.h`. Al finalizar, sabras elegir el tipo entero correcto para cada
situacion y como imprimir sus valores de forma portable.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `sizes.c` | `sizeof` de todos los tipos enteros basicos |
| `limits.c` | Rangos de cada tipo con macros de `limits.h` |
| `fixed_width.c` | Tamanos de tipos de `stdint.h` (exact, least, fast, especiales) |
| `format_specifiers.c` | Macros `PRId32`, `PRIx64`, etc. de `inttypes.h` |

---

## Parte 1 -- Tamanos de tipos enteros

**Objetivo**: Verificar cuantos bytes ocupa cada tipo entero en tu plataforma
y determinar si `char` es signed o unsigned.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat sizes.c
```

Observa:

- Usa `sizeof` para obtener el tamano en bytes de cada tipo
- Usa `%zu` como format specifier (es el correcto para `size_t`, el tipo que
  retorna `sizeof`)
- Compara `CHAR_MIN` contra 0 para determinar si `char` es signed o unsigned

### Paso 1.2 -- Predecir los tamanos

Antes de compilar, responde mentalmente:

- Cuantos bytes tiene un `int` en tu sistema?
- `long` tiene 4 u 8 bytes en Linux de 64 bits?
- `signed int` y `unsigned int` tienen el mismo tamano?
- `char` es signed o unsigned en x86 con GCC?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic sizes.c -o sizes
./sizes
```

Salida esperada:

```
=== sizeof of integer types ===

char:      1 byte(s)
short:     2 bytes
int:       4 bytes
long:      ~8 bytes
long long: 8 bytes

=== signed vs unsigned (same sizes) ===

unsigned char:      1 byte(s)
unsigned short:     2 bytes
unsigned int:       4 bytes
unsigned long:      ~8 bytes
unsigned long long: 8 bytes

=== is plain char signed or unsigned? ===

char is signed (CHAR_MIN = -128)
```

Analisis:

- `sizeof(char)` siempre es 1, por definicion del estandar
- `signed` y `unsigned` no cambian el tamano, solo la interpretacion de los bits
- `long` es 8 bytes en Linux 64-bit (modelo LP64), pero seria 4 en Windows
  64-bit (modelo LLP64) -- por eso no se debe asumir su tamano
- En x86 con GCC, `char` es signed (-128 a 127). En ARM podria ser unsigned

### Paso 1.4 -- Verificar la jerarquia del estandar

El estandar garantiza:

```
sizeof(char) <= sizeof(short) <= sizeof(int) <= sizeof(long) <= sizeof(long long)
```

Verifica con la salida anterior que tu sistema cumple esta jerarquia.

### Limpieza de la parte 1

```bash
rm -f sizes
```

---

## Parte 2 -- Rangos y limites con limits.h

**Objetivo**: Consultar los rangos minimo y maximo de cada tipo entero usando
las macros de `limits.h`.

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat limits.c
```

Observa los format specifiers usados para cada tipo:

- `%d` para `char`, `short`, `int` (se promueven a `int` en printf)
- `%ld` para `long`
- `%lld` para `long long`
- `%u`, `%lu`, `%llu` para las versiones unsigned

### Paso 2.2 -- Predecir los rangos

Antes de compilar, responde mentalmente:

- Para un tipo de N bits con signo, el rango es -2^(N-1) a 2^(N-1)-1.
  Cual es el rango de un `int` de 32 bits?
- Cual es el maximo de `unsigned int` de 32 bits (2^32 - 1)?
- `long` y `long long` tendran los mismos rangos en Linux 64-bit?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic limits.c -o limits_prog
./limits_prog
```

Salida esperada:

```
=== signed type ranges ===

char:      -128 to 127
short:     -32768 to 32767
int:       -2147483648 to 2147483647
long:      -9223372036854775808 to 9223372036854775807
long long: -9223372036854775808 to 9223372036854775807

=== unsigned type ranges ===

unsigned char:      0 to 255
unsigned short:     0 to 65535
unsigned int:       0 to 4294967295
unsigned long:      0 to 18446744073709551615
unsigned long long: 0 to 18446744073709551615
```

Analisis:

- El rango signed no es simetrico: hay un negativo mas que positivos
  (por ejemplo, `INT_MIN` es -2147483648 pero `INT_MAX` es 2147483647)
- Esto se debe a la representacion en complemento a dos
- `long` y `long long` tienen los mismos rangos en Linux 64-bit porque ambos
  son de 8 bytes. En otras plataformas pueden diferir
- `UINT_MAX` es exactamente `2 * INT_MAX + 1` (4294967295 = 2 * 2147483647 + 1)

### Paso 2.4 -- Observacion sobre los nombres de las macros

Nota los nombres irregulares en `limits.h`:

| Tipo | Macro MIN | Macro MAX |
|------|-----------|-----------|
| `char` | `CHAR_MIN` | `CHAR_MAX` |
| `short` | `SHRT_MIN` | `SHRT_MAX` |
| `int` | `INT_MIN` | `INT_MAX` |
| `long` | `LONG_MIN` | `LONG_MAX` |
| `long long` | `LLONG_MIN` | `LLONG_MAX` |
| `unsigned char` | (es 0) | `UCHAR_MAX` |
| `unsigned short` | (es 0) | `USHRT_MAX` |
| `unsigned int` | (es 0) | `UINT_MAX` |
| `unsigned long` | (es 0) | `ULONG_MAX` |
| `unsigned long long` | (es 0) | `ULLONG_MAX` |

No hay macros `_MIN` para tipos unsigned porque siempre es 0.

### Limpieza de la parte 2

```bash
rm -f limits_prog
```

---

## Parte 3 -- Tipos de ancho fijo con stdint.h

**Objetivo**: Verificar los tamanos de los tipos de `stdint.h` y entender la
diferencia entre exact-width, least-width y fast types.

### Paso 3.1 -- Examinar el codigo fuente

```bash
cat fixed_width.c
```

Observa que el programa incluye `stdint.h` (tipos) y `stddef.h` (para
`ptrdiff_t`). Mide cuatro categorias:

- **Exact-width**: `int8_t`, `int16_t`, `int32_t`, `int64_t` -- tamano exacto
  garantizado
- **Least-width**: `int_least8_t`, etc. -- al menos N bits, puede ser mas
- **Fast**: `int_fast8_t`, etc. -- el tipo mas eficiente con al menos N bits
- **Especiales**: `intmax_t`, `intptr_t`, `size_t`, `ptrdiff_t`

### Paso 3.2 -- Predecir los tamanos

Antes de compilar, responde mentalmente:

- Los exact-width types tendran exactamente los bytes esperados (1, 2, 4, 8)?
- Los least-width types tendran los mismos tamanos que los exact-width?
- `int_fast16_t` tendra 2 bytes (lo minimo) u 8 bytes (lo mas rapido en
  x86-64)?
- `intptr_t` tendra 4 u 8 bytes en un sistema de 64 bits?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic fixed_width.c -o fixed_width
./fixed_width
```

Salida esperada:

```
=== exact-width types (stdint.h) ===

int8_t:   1 byte(s)
int16_t:  2 bytes
int32_t:  4 bytes
int64_t:  8 bytes

uint8_t:  1 byte(s)
uint16_t: 2 bytes
uint32_t: 4 bytes
uint64_t: 8 bytes

=== least-width types ===

int_least8_t:  1 byte(s)
int_least16_t: 2 bytes
int_least32_t: 4 bytes
int_least64_t: 8 bytes

=== fast types ===

int_fast8_t:  1 byte(s)
int_fast16_t: ~8 bytes
int_fast32_t: ~8 bytes
int_fast64_t: 8 bytes

=== special types ===

intmax_t:  8 bytes
intptr_t:  8 bytes
size_t:    8 bytes
ptrdiff_t: 8 bytes
```

Analisis:

- Los exact-width types tienen exactamente el tamano indicado por su nombre
- Los least-width types coinciden en esta plataforma, pero en arquitecturas
  exoticas podrian ser mas grandes
- Los fast types son la sorpresa: `int_fast16_t` es 8 bytes, no 2. La
  implementacion decide que operar con 64 bits es mas rapido que con 16 en
  x86-64, aunque desperdicia memoria
- `intptr_t` es 8 bytes porque los punteros son de 64 bits en esta plataforma.
  En un sistema de 32 bits seria 4
- `size_t` y `ptrdiff_t` tambien son 8 bytes en 64-bit

### Paso 3.4 -- Cuando usar cada tipo

Referencia rapida:

| Situacion | Tipo correcto |
|-----------|---------------|
| Variable de loop, uso general | `int` |
| Tamano de array, indice de array | `size_t` |
| Puerto TCP, pixel RGB | `uint16_t`, `uint8_t` |
| Timestamp en milisegundos | `int64_t` |
| Direccion IP v4 | `uint32_t` |
| Almacenar un puntero como entero | `uintptr_t` |
| Contador que puede ser muy grande | `uint64_t` |

### Limpieza de la parte 3

```bash
rm -f fixed_width
```

---

## Parte 4 -- Format specifiers portables con inttypes.h

**Objetivo**: Usar las macros de `inttypes.h` para imprimir tipos de `stdint.h`
de forma portable, en decimal y hexadecimal.

### Paso 4.1 -- Examinar el codigo fuente

```bash
cat format_specifiers.c
```

Observa:

- Incluye `inttypes.h` (que a su vez incluye `stdint.h`)
- Usa macros como `PRId32`, `PRIu64`, `PRIx32`, `PRIX64` dentro del format
  string de `printf`
- La sintaxis es `"%" PRId32` -- la macro se concatena con el string literal

### Paso 4.2 -- Por que no usar %d o %ld directamente?

Antes de compilar, reflexiona:

- `int32_t` podria ser `int` en una plataforma y `long` en otra. Si usas `%d`
  y resulta ser `long`, tendras comportamiento indefinido
- `PRId32` se expande a `"d"` o `"ld"` segun la plataforma, garantizando que
  el format specifier siempre es correcto
- Para `int64_t`, usar `%lld` funciona en Linux pero no necesariamente en
  Windows donde podria necesitar `%I64d`. `PRId64` resuelve esto

### Paso 4.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic format_specifiers.c -o format_specifiers
./format_specifiers
```

Salida esperada:

```
=== format specifiers for stdint.h types ===

int32_t  max: 2147483647
uint32_t max: 4294967295
int64_t  max: 9223372036854775807
uint64_t max: 18446744073709551615

=== hexadecimal format ===

uint32_t hex (lowercase): deadbeef
uint32_t hex (uppercase): DEADBEEF
uint64_t hex (lowercase): cafebabedeadc0de
uint64_t hex (uppercase): CAFEBABEDEADC0DE

=== limits from stdint.h ===

INT8_MIN:   -128
INT8_MAX:   127
UINT8_MAX:  255
INT16_MIN:  -32768
INT16_MAX:  32767
UINT16_MAX: 65535
INT32_MIN:  -2147483648
INT32_MAX:  2147483647
UINT32_MAX: 4294967295
INT64_MIN:  -9223372036854775808
INT64_MAX:  9223372036854775807
UINT64_MAX: 18446744073709551615
```

Analisis:

- Los valores coinciden con los de la parte 2 (limits.h), pero ahora usamos
  las macros de `stdint.h` (`INT32_MAX` vs `INT_MAX`) y los format specifiers
  de `inttypes.h`
- El formato hexadecimal con `PRIx` produce minusculas, `PRIX` produce
  mayusculas
- Nota como `0xDEADBEEF` se imprime correctamente con `PRIx32` y
  `0xCAFEBABEDEADC0DE` con `PRIx64` -- los specifiers manejan el ancho
  correcto

### Paso 4.4 -- Referencia de macros de inttypes.h

| Macro | Formato | Ejemplo para `int32_t val = 42` |
|-------|---------|------|
| `PRId32` | Decimal con signo | `"%" PRId32` imprime `42` |
| `PRIu32` | Decimal sin signo | `"%" PRIu32` imprime `42` |
| `PRIx32` | Hexadecimal minuscula | `"%" PRIx32` imprime `2a` |
| `PRIX32` | Hexadecimal mayuscula | `"%" PRIX32` imprime `2A` |
| `PRIo32` | Octal | `"%" PRIo32` imprime `52` |

Las mismas macros existen para 8, 16, 32 y 64 bits. Para scanf se usan
`SCNd32`, `SCNu64`, etc.

### Limpieza de la parte 4

```bash
rm -f format_specifiers
```

---

## Limpieza final

```bash
rm -f sizes limits_prog fixed_width format_specifiers
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  fixed_width.c  format_specifiers.c  limits.c  sizes.c
```

---

## Conceptos reforzados

1. `sizeof(char)` siempre es 1 por definicion. Los demas tamanos dependen de
   la plataforma, pero el estandar garantiza la jerarquia
   `char <= short <= int <= long <= long long`.

2. `long` es 8 bytes en Linux 64-bit (modelo LP64) pero 4 en Windows 64-bit
   (modelo LLP64). Nunca asumir su tamano en codigo portable.

3. `signed` y `unsigned` no cambian el tamano de un tipo, solo la
   interpretacion de los bits. El bit mas significativo pasa de ser bit de
   signo a bit de valor.

4. El `char` sin calificador puede ser signed o unsigned segun la plataforma.
   En x86 con GCC es signed. Si el comportamiento importa, usar
   `signed char` o `unsigned char` explicitamente.

5. Los rangos de tipos signed no son simetricos: hay un negativo mas que
   positivos (por ejemplo, -2147483648 a 2147483647 para `int` de 32 bits)
   debido a la representacion en complemento a dos.

6. Los tipos de `stdint.h` (`int32_t`, `uint64_t`, etc.) garantizan un tamano
   exacto. Usarlos cuando el tamano importa (protocolos de red, formatos de
   archivo, hardware).

7. Los fast types (`int_fast16_t`) pueden ser mas grandes que lo pedido. En
   x86-64, `int_fast16_t` es de 8 bytes porque operar con 64 bits es mas
   rapido que con 16.

8. Las macros de `inttypes.h` (`PRId32`, `PRIu64`, `PRIx32`) son la forma
   portable de imprimir tipos de `stdint.h`. Usar `%d` directamente con
   `int32_t` puede ser incorrecto en plataformas donde `int32_t` no es `int`.
