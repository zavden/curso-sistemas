# Lab — restrict

## Objetivo

Entender por que existe `restrict`, ver su efecto en el assembly generado,
observar comportamiento indefinido al violar la promesa, y comprender la
diferencia entre `memcpy` y `memmove`. Al finalizar, sabras cuando usar
`restrict`, cuando no, y por que `memcpy` es mas rapido que `memmove`.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `aliasing.c` | Dos funciones de suma de arrays: con y sin restrict |
| `aliasing_problem.c` | Demuestra por que el aliasing impide optimizaciones |
| `restrict_violation.c` | Viola restrict intencionalmente para observar UB |
| `memcpy_vs_memmove.c` | Compara el comportamiento con memoria solapada |

---

## Parte 1 — El problema del aliasing

**Objetivo**: Entender por que el compilador no puede optimizar cuando dos
punteros podrian apuntar a la misma memoria.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat aliasing_problem.c
```

Observa las dos funciones `scale_add` y `scale_add_restrict`. Ambas hacen
exactamente lo mismo: `dst[i] += src[i] * factor`. La unica diferencia es
que la segunda usa `restrict`.

Observa tambien la funcion `main`: llama a `scale_add` dos veces. La
primera vez con arrays separados (`a` y `b`). La segunda vez con **el mismo
array** como `dst` y `src` (`self` y `self`).

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic aliasing_problem.c -o aliasing_problem
./aliasing_problem
```

Salida esperada:

```
Non-aliased result: 21.0 42.0 63.0 84.0
Self-aliased result: 2.0 4.0 6.0 8.0
```

Analisis:

- El primer resultado: `a[i] += b[i] * 2.0`. El array `a` empieza en
  {1,2,3,4} y se le suma {10,20,30,40}*2 = {20,40,60,80}. Resultado:
  {21,42,63,84}.
- El segundo resultado: `self[i] += self[i] * 1.0`. Como `dst` y `src` son
  el mismo array, `self[i] += self[i]` equivale a `self[i] *= 2`. Resultado:
  {2,4,6,8}.

Este segundo caso es **aliasing**: `dst` y `src` apuntan a la misma memoria.
Cada escritura en `dst[i]` cambia `src[i]`. El compilador **debe** contemplar
esta posibilidad si no tiene `restrict`.

### Paso 1.3 — Ver el assembly sin restrict

```bash
gcc -std=c11 -O2 -S aliasing_problem.c -o aliasing_problem.s
```

Busca la funcion `scale_add` (sin restrict) en el assembly:

```bash
sed -n '/^scale_add:/,/ret$/p' aliasing_problem.s
```

Antes de mirar, predice:

- El compilador puede usar instrucciones SIMD (xmm/ymm) para procesar
  varios doubles a la vez?
- O debe cargar y almacenar un double por iteracion?

### Paso 1.4 — Verificar la prediccion

Cuenta las instrucciones SIMD en cada funcion:

```bash
echo "=== scale_add (sin restrict) ==="
sed -n '/^scale_add:/,/\.cfi_endproc/p' aliasing_problem.s | grep -cE "xmm|ymm"

echo "=== scale_add_restrict (con restrict) ==="
sed -n '/^scale_add_restrict:/,/\.cfi_endproc/p' aliasing_problem.s | grep -cE "xmm|ymm"
```

Incluso sin restrict, el compilador usa registros XMM para operaciones con
`double` (no puede evitarlo en x86-64). Pero la diferencia esta en **como**
los usa. Examina el loop de cada funcion:

```bash
echo "=== Loop de scale_add (sin restrict) ==="
sed -n '/^scale_add:/,/\.cfi_endproc/p' aliasing_problem.s | grep -E "movsd|addsd|mulsd|movapd|addpd|mulpd|vfmadd"

echo ""
echo "=== Loop de scale_add_restrict (con restrict) ==="
sed -n '/^scale_add_restrict:/,/\.cfi_endproc/p' aliasing_problem.s | grep -E "movsd|addsd|mulsd|movapd|addpd|mulpd|vfmadd|movupd"
```

Sin restrict veras instrucciones escalares (`movsd`, `addsd`, `mulsd`) que
procesan **un double** a la vez. Con restrict, el compilador puede usar
instrucciones empaquetadas (`movapd`/`movupd`, `addpd`, `mulpd` o `vfmadd`)
que procesan **dos doubles** simultaneamente. Esta es la vectorizacion que
`restrict` habilita.

### Limpieza de la parte 1

```bash
rm -f aliasing_problem aliasing_problem.s
```

---

## Parte 2 — restrict en accion: assembly lado a lado

**Objetivo**: Comparar el assembly de funciones identicas que solo difieren en
`restrict`, esta vez con un loop de enteros donde la vectorizacion es mas
visible.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat aliasing.c
```

Dos funciones: `sum_no_restrict` y `sum_restrict`. Ambas suman dos arrays
de `int` y guardan el resultado en un tercero. Los arrays tienen 8 elementos.

### Paso 2.2 — Generar assembly con -O2

```bash
gcc -std=c11 -O2 -S aliasing.c -o aliasing_O2.s
```

### Paso 2.3 — Comparar los loops

Antes de mirar, predice:

- La funcion sin restrict usara instrucciones como `paddd` (suma de 4 ints
  empaquetados) o `addl` (suma de 1 int)?
- La funcion con restrict usara `paddd` o `addl`?

### Paso 2.4 — Verificar la prediccion

```bash
echo "=== sum_no_restrict ==="
sed -n '/^sum_no_restrict:/,/\.cfi_endproc/p' aliasing_O2.s

echo ""
echo "=== sum_restrict ==="
sed -n '/^sum_restrict:/,/\.cfi_endproc/p' aliasing_O2.s
```

Busca estas instrucciones clave:

| Instruccion | Significado |
|-------------|-------------|
| `movl` + `addl` | Cargar y sumar UN int (escalar) |
| `movdqu` | Cargar 4 ints de memoria (no alineados) |
| `paddd` | Sumar 4 ints empaquetados en registros XMM |
| `movups` | Almacenar 4 ints de un registro XMM a memoria |

En `sum_no_restrict` veras un loop escalar: `movl`, `addl`, `movl`, avanzar
4 bytes, repetir. El compilador no puede vectorizar porque no sabe si `out`
se solapa con `a` o `b`.

En `sum_restrict` veras un loop vectorizado: `movdqu`, `paddd`, `movups`,
avanzar 16 bytes (4 ints). El compilador sabe que los tres punteros apuntan
a memoria independiente.

### Paso 2.5 — Confirmar con un conteo

```bash
echo "=== Instrucciones escalares en sum_no_restrict ==="
sed -n '/^sum_no_restrict:/,/\.cfi_endproc/p' aliasing_O2.s | grep -cE "addl|movl"

echo "=== Instrucciones vectoriales en sum_restrict ==="
sed -n '/^sum_restrict:/,/\.cfi_endproc/p' aliasing_O2.s | grep -cE "paddd|movdqu|movups"
```

La funcion sin restrict tiene cero instrucciones vectoriales para el loop
principal. La funcion con restrict las tiene.

### Paso 2.6 — Compilar y verificar que ambas dan el mismo resultado

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic aliasing.c -o aliasing
./aliasing
```

Salida esperada:

```
sum_no_restrict: 11 22 33 44 55 66 77 88
sum_restrict:    11 22 33 44 55 66 77 88
```

El resultado es identico. La diferencia esta **solo en el rendimiento**: la
version con restrict procesa 4 ints por iteracion en lugar de 1.

### Limpieza de la parte 2

```bash
rm -f aliasing aliasing_O2.s
```

---

## Parte 3 — Violar restrict: comportamiento indefinido observable

**Objetivo**: Ver que pasa cuando se viola la promesa de `restrict`. El
compilador genera codigo basado en esa promesa; si es falsa, los resultados
cambian segun el nivel de optimizacion.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat restrict_violation.c
```

Observa:

- `update(int *restrict a, int *restrict b)`: escribe 10 en `*a`, luego 20
  en `*b`, luego imprime `*a`.
- `copy_restrict`: copia un array con punteros restrict.
- En `main`, ambas funciones se llaman violando restrict: pasando punteros
  que se solapan.

### Paso 3.2 — Compilar sin optimizacion

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic restrict_violation.c -o violation_O0
```

Observa el warning del compilador:

```
warning: passing argument 1 to 'restrict'-qualified parameter aliases with argument 2 [-Wrestrict]
```

GCC detecta la violacion en `update(&x, &x)` y avisa. Pero compila sin error;
`restrict` es una promesa, no una regla que el compilador obligue.

```bash
./violation_O0
```

Salida esperada:

```
=== Test 1: update with aliased pointers ===
*a = 20 (expect 10 if no alias, 20 if aliased)
Final x = 20

=== Test 2: copy with non-overlapping arrays ===
dst: 1 2 3 4 5

=== Test 3: copy with overlapping regions ===
data: 1 2 1 2 1 2 1 8
```

Sin optimizacion, el compilador ejecuta las instrucciones en orden literal.
En Test 1: `*a = 10`, `*b = 20` (que sobrescribe `*a` porque son lo mismo),
luego imprime `*a` que ahora es 20.

Antes de compilar con -O2, predice:

- En Test 1, el compilador con restrict puede asumir que `*a` sigue siendo
  10 despues de `*b = 20`. Que imprimira?
- En Test 3, si el compilador vectoriza la copia, los datos solapados se
  copiaran correctamente?

### Paso 3.3 — Compilar con optimizacion

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 restrict_violation.c -o violation_O2
./violation_O2
```

Salida esperada (puede variar entre versiones de GCC):

```
=== Test 1: update with aliased pointers ===
*a = 10 (expect 10 if no alias, 20 if aliased)
Final x = 20

=== Test 2: copy with non-overlapping arrays ===
dst: 1 2 3 4 5

=== Test 3: copy with overlapping regions ===
data: 1 2 1 2 3 4 3 8
```

### Paso 3.4 — Analizar las diferencias

Compara los resultados de -O0 y -O2:

| Test | -O0 | -O2 | Explicacion |
|------|-----|-----|-------------|
| Test 1: *a | 20 | 10 | Con -O2, el compilador cacheo *a=10 en un registro. La escritura *b=20 "no puede" afectar *a (por restrict). Imprime el valor cacheado: 10 |
| Test 1: x | 20 | 20 | El valor real en memoria es 20 (ambas escrituras ocurren), pero la lectura para printf uso el cache |
| Test 3: data | 1 2 1 2 1 2 1 8 | 1 2 1 2 3 4 3 8 | Con -O2, el loop fue vectorizado y copio bloques. La copia solapada produjo un resultado diferente |

Esto es **comportamiento indefinido** en accion. El programa no tiene un
resultado correcto ni incorrecto: al violar `restrict`, cualquier resultado
es valido segun el estandar.

### Paso 3.5 — Ver el reordenamiento en el assembly

```bash
gcc -std=c11 -O2 -S restrict_violation.c -o violation_O2.s
sed -n '/^update:/,/\.cfi_endproc/p' violation_O2.s
```

Busca el orden de las instrucciones `movl` (las escrituras de 10 y 20). Con
restrict, el compilador puede mover la escritura de `*a = 10` despues de
`*b = 20`, o puede pasar directamente el valor 10 al `printf` sin releer
`*a` de memoria.

### Limpieza de la parte 3

```bash
rm -f violation_O0 violation_O2 violation_O2.s
```

---

## Parte 4 — restrict en la stdlib: memcpy vs memmove

**Objetivo**: Entender por que existen dos funciones de copia en la biblioteca
estandar y como `restrict` en la firma de `memcpy` permite que sea mas rapida.

### Paso 4.1 — Ver las firmas oficiales

```bash
man memcpy 2>/dev/null | head -20
```

```bash
man memmove 2>/dev/null | head -20
```

Si `man` no esta disponible, las firmas son:

```c
void *memcpy(void *restrict dst, const void *restrict src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
```

La diferencia clave: `memcpy` tiene `restrict` en ambos punteros. `memmove`
no. Eso significa:

- `memcpy` **promete** que `dst` y `src` no se solapan. Puede copiar en
  cualquier orden (adelante, atras, en bloques de 64 bytes, con SIMD).
- `memmove` **no promete** nada. Debe verificar si hay solapamiento y copiar
  en la direccion correcta para no corromper los datos.

### Paso 4.2 — Examinar el codigo fuente

```bash
cat memcpy_vs_memmove.c
```

El programa hace tres pruebas:

1. Copia sin solapamiento: `memcpy` y `memmove` dan el mismo resultado.
2. Copia solapada hacia adelante: `data[0..4] -> data[2..6]`.
3. Copia solapada hacia atras: `data[2..6] -> data[0..4]`.

### Paso 4.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic memcpy_vs_memmove.c -o memcpy_vs_memmove
./memcpy_vs_memmove
```

Antes de mirar el resultado, predice:

- En la copia sin solapamiento, los resultados seran iguales o diferentes?
- En la copia solapada hacia adelante, `memmove` dara el resultado correcto.
  Y `memcpy`?
- En la copia solapada hacia atras, habra diferencia?

### Paso 4.4 — Analizar la salida

Salida esperada (el resultado de memcpy con solapamiento puede variar):

```
=== Non-overlapping copy ===
memcpy dst: 1 2 3 4 5
memmove dst: 1 2 3 4 5
Both produce the same result with non-overlapping memory.

=== Overlapping copy (forward) ===
Before: data: 1 2 3 4 5 6 7 8
Copying data[0..4] -> data[2..6]
memcpy result : ...
memmove result: 1 2 1 2 3 4 5 8

memmove always gives the correct result: {1,2, 1,2,3,4,5, 8}
memcpy may or may not be correct -- it depends on the implementation.

=== Overlapping copy (backward) ===
Before: data: 1 2 3 4 5 6 7 8
Copying data[2..6] -> data[0..4]
memcpy result : ...
memmove result: 3 4 5 6 7 6 7 8

Both should give: {3,4,5,6,7, 6,7,8}
```

Puntos clave:

- **Sin solapamiento**: ambas dan el mismo resultado. No hay diferencia
  funcional.
- **Con solapamiento**: `memmove` siempre da el resultado correcto. `memcpy`
  puede dar el resultado correcto o no, dependiendo de la implementacion
  interna de la libc. En glibc moderna, `memcpy` suele usar la misma
  implementacion que `memmove` para bloques pequenos, pero esto no esta
  garantizado.
- En tu sistema, `memcpy` con solapamiento puede dar el mismo resultado que
  `memmove`. Eso no significa que sea seguro: en otra arquitectura, otra
  version de libc, u otro tamano de datos, puede fallar.

### Paso 4.5 — Por que memcpy es mas rapida

La razon por la que `memcpy` puede ser mas rapida que `memmove`:

- `memcpy` no necesita comprobar si `dst` y `src` se solapan.
- Puede copiar directamente con instrucciones SIMD anchas (AVX-512: 64
  bytes por instruccion) sin preocuparse por la direccion.
- `memmove` debe comparar `dst` y `src`, y si se solapan, copiar en la
  direccion correcta (de atras hacia adelante si `dst > src`).

En la practica, la diferencia es pequena para copias pequenas. Para copias
grandes (megabytes), `memcpy` puede ser mediblemente mas rapida.

La regla es simple:

- Si sabes que no se solapan: usa `memcpy` (mas rapida).
- Si podrian solaparse: usa `memmove` (segura).

### Paso 4.6 — Otras funciones de la stdlib con restrict

`restrict` aparece en muchas funciones estandar:

```bash
grep -r "restrict" /usr/include/string.h 2>/dev/null | head -10
```

Funciones como `strcpy`, `strcat`, `sprintf`, `printf`, `scanf` y `fopen`
usan `restrict` en sus parametros. Todas asumen que los punteros no se solapan.
Pasar punteros solapados a cualquiera de estas funciones es UB.

### Limpieza de la parte 4

```bash
rm -f memcpy_vs_memmove
```

---

## Limpieza final

```bash
rm -f aliasing aliasing_problem aliasing_O2.s aliasing_problem.s
rm -f violation_O0 violation_O2 violation_O2.s
rm -f memcpy_vs_memmove
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  aliasing.c  aliasing_problem.c  memcpy_vs_memmove.c  restrict_violation.c
```

---

## Conceptos reforzados

1. El **problema de aliasing** impide que el compilador optimice: si dos
   punteros podrian apuntar a la misma memoria, debe releer de memoria en cada
   iteracion en lugar de cachear valores en registros.

2. `restrict` es una **promesa del programador** al compilador: este puntero
   es la unica forma de acceder a estos datos. No es una regla que el
   compilador verifique; es responsabilidad del programador cumplirla.

3. Con `restrict`, el compilador puede **vectorizar** loops: usar
   instrucciones SIMD (`paddd`, `movdqu`, `movups`) que procesan 4 ints
   simultaneamente en lugar de 1. Esto puede significar 2-4x mas rendimiento.

4. Violar `restrict` es **comportamiento indefinido**. El programa puede
   producir resultados diferentes con -O0 y -O2 porque el compilador
   reordena instrucciones y cachea valores basandose en la promesa de
   no-aliasing.

5. `memcpy` tiene `restrict` en su firma y por eso puede ser mas rapida que
   `memmove`: no necesita verificar solapamiento ni copiar en una direccion
   especifica. `memmove` no tiene `restrict` y siempre maneja correctamente
   la memoria solapada.

6. Muchas funciones de la biblioteca estandar (`strcpy`, `printf`, `scanf`,
   `fopen`) usan `restrict`. Pasar punteros solapados a estas funciones es
   comportamiento indefinido aunque compile sin errores.
