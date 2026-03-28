# Lab -- Inline

## Objetivo

Observar el efecto de `inline` en el assembly generado por GCC, practicar la
definicion de funciones `static inline` en headers para uso en multiples
translation units, y verificar que el compilador con `-O2` decide
automaticamente cuando hacer inline. Al finalizar, sabras cuando usar `inline`,
por que `static inline` en headers es la forma correcta, y que el compilador
suele tomar mejores decisiones de inlining que el programador.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `square_regular.c` | Funcion `square()` normal, sin inline |
| `square_inline.c` | Funcion `square()` declarada como `static inline` |
| `math_inline.h` | Header con `min`, `max`, `clamp` como `static inline` |
| `caller_a.c` | Translation unit que usa funciones de `math_inline.h` |
| `caller_b.c` | Otro translation unit que usa funciones de `math_inline.h` |
| `main_multi.c` | Programa principal que enlaza `caller_a` y `caller_b` |
| `auto_inline.c` | Funcion pequena y funcion grande para probar decisiones del compilador |

---

## Parte 1 -- Inline basico: assembly con y sin inline

**Objetivo**: Comparar el assembly generado para una funcion normal vs una
`static inline`, y observar como cambia el comportamiento con optimizacion.

### Paso 1.1 -- Verificar el entorno

```bash
gcc --version
```

Confirma que GCC esta instalado. Anota la version.

### Paso 1.2 -- Examinar los archivos fuente

```bash
cat square_regular.c
cat square_inline.c
```

Observa la unica diferencia entre los dos archivos: `square_inline.c` declara
la funcion como `static inline`. El resto es identico.

### Paso 1.3 -- Compilar ambos y verificar que funcionan

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic square_regular.c -o square_regular
gcc -std=c11 -Wall -Wextra -Wpedantic square_inline.c -o square_inline
./square_regular
./square_inline
```

Salida esperada (ambos):

```
square(5) = 25
```

### Paso 1.4 -- Generar assembly sin optimizacion (-O0)

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -S -O0 square_regular.c -o square_regular_O0.s
gcc -std=c11 -Wall -Wextra -Wpedantic -S -O0 square_inline.c -o square_inline_O0.s
```

Antes de examinar, predice:

- Con `-O0` (sin optimizacion), el compilador respetara el `inline` y
  expandira el cuerpo de `square()` en `main`? O generara un `call square`
  igual que la version sin inline?

### Paso 1.5 -- Verificar la prediccion

```bash
grep "call" square_regular_O0.s
grep "call" square_inline_O0.s
```

Salida esperada (ambos):

```
	call	square
	call	printf
```

Ambos generan `call square`. Con `-O0`, el compilador **ignora** la sugerencia
`inline` y genera una llamada normal. Recuerda: `inline` es una sugerencia, no
una orden.

### Paso 1.6 -- Generar assembly con optimizacion (-O2)

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -S -O2 square_regular.c -o square_regular_O2.s
gcc -std=c11 -Wall -Wextra -Wpedantic -S -O2 square_inline.c -o square_inline_O2.s
```

Antes de examinar, predice:

- Con `-O2`, seguira apareciendo `call square`?
- Habra diferencia entre la version regular y la inline?

### Paso 1.7 -- Verificar la prediccion

```bash
echo "=== regular -O2: calls ==="
grep "call" square_regular_O2.s

echo "=== inline -O2: calls ==="
grep "call" square_inline_O2.s
```

Salida esperada (ambos):

```
	call	printf
```

Con `-O2`, **ninguno** tiene `call square`. El compilador inlineo la funcion en
ambos casos porque es suficientemente pequena. La diferencia esta en otra parte:

```bash
echo "=== regular -O2: square label ==="
grep "square" square_regular_O2.s

echo "=== inline -O2: square label ==="
grep "square" square_inline_O2.s
```

En la version regular, `square` sigue presente como simbolo global
(`.globl square`, etiqueta `square:`). El compilador la inlineo en `main`, pero
mantiene una copia por si otro archivo la necesita.

En la version `static inline`, el simbolo `square` desaparece completamente del
assembly (solo queda en la cadena `"square(%d) = %d\n"`). Como es `static`, no
tiene enlace externo, y como se inlineo en todas las llamadas, el compilador
elimina la funcion por completo.

### Paso 1.8 -- Comparar tamano del assembly

```bash
wc -l square_regular_O2.s square_inline_O2.s
```

Salida esperada:

```
 ~40 square_regular_O2.s
 ~28 square_inline_O2.s
 ~68 total
```

La version `static inline` genera menos assembly porque no conserva una copia
separada de la funcion.

### Limpieza de la parte 1

```bash
rm -f square_regular square_inline
rm -f square_regular_O0.s square_inline_O0.s
rm -f square_regular_O2.s square_inline_O2.s
```

---

## Parte 2 -- Inline en headers: multiples translation units

**Objetivo**: Definir funciones `static inline` en un header e incluirlo desde
multiples archivos `.c` sin errores de enlace.

### Paso 2.1 -- Examinar los archivos fuente

```bash
cat math_inline.h
```

Observa: tres funciones definidas como `static inline` dentro del header, con
include guard (`#ifndef`). La definicion completa (cuerpo) esta en el header,
no solo el prototipo.

```bash
cat caller_a.c
cat caller_b.c
cat main_multi.c
```

Ambos `caller_a.c` y `caller_b.c` incluyen `math_inline.h` y usan sus
funciones. `main_multi.c` declara los prototipos de `test_from_a()` y
`test_from_b()` y las llama desde `main()`.

Antes de compilar, predice:

- Si dos archivos `.c` incluyen el mismo header con funciones `static inline`,
  habra error de "multiple definition" al enlazar?
- Las funciones `min`, `max`, `clamp` apareceran en la tabla de simbolos de
  los archivos `.o`?

### Paso 2.2 -- Compilar y enlazar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic caller_a.c caller_b.c main_multi.c -o multi_inline
./multi_inline
```

Salida esperada:

```
=== Inline functions from header ===
[caller_a] clamp(15, 0, 10) = 10
[caller_a] min(3, 7) = 3
[caller_b] clamp(-5, 0, 10) = 0
[caller_b] max(3, 7) = 7
```

No hay error de enlace. `static` hace que cada translation unit tenga su propia
copia local de las funciones, sin conflicto de nombres.

### Paso 2.3 -- Examinar simbolos sin optimizacion

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -c -O0 caller_a.c -o caller_a.o
gcc -std=c11 -Wall -Wextra -Wpedantic -c -O0 caller_b.c -o caller_b.o
```

```bash
echo "=== caller_a.o ==="
nm caller_a.o

echo "=== caller_b.o ==="
nm caller_b.o
```

Salida esperada:

```
=== caller_a.o ===
... t clamp
... t min
                 U printf
... T test_from_a

=== caller_b.o ===
... t clamp
... t max
                 U printf
... T test_from_b
```

Observa:

- `clamp`, `min`, `max` aparecen con `t` minuscula (simbolo local, por el
  `static`). No son `T` (global) -- no causan conflicto al enlazar.
- Cada `.o` tiene su propia copia de las funciones que usa.
- Con `-O0`, el compilador no inlineo -- genero las funciones como codigo
  normal pero local.

### Paso 2.4 -- Examinar simbolos con optimizacion

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -c -O2 caller_a.c -o caller_a.o
gcc -std=c11 -Wall -Wextra -Wpedantic -c -O2 caller_b.c -o caller_b.o
```

```bash
echo "=== caller_a.o (-O2) ==="
nm caller_a.o

echo "=== caller_b.o (-O2) ==="
nm caller_b.o
```

Salida esperada:

```
=== caller_a.o (-O2) ===
                 U printf
0000000000000000 T test_from_a

=== caller_b.o (-O2) ===
                 U printf
0000000000000000 T test_from_b
```

Con `-O2`, `clamp`, `min` y `max` desaparecen por completo. El compilador
inlineo sus cuerpos dentro de `test_from_a()` y `test_from_b()`, y como son
`static` sin otras llamadas, elimino las copias sobrantes. Este es el
comportamiento ideal de `static inline`.

### Limpieza de la parte 2

```bash
rm -f caller_a.o caller_b.o multi_inline
```

---

## Parte 3 -- El compilador decide: inlining automatico con -O2

**Objetivo**: Verificar que el compilador con `-O2` inlinea funciones pequenas
automaticamente (sin `inline` explicito) y no inlinea funciones grandes.

### Paso 3.1 -- Examinar el codigo fuente

```bash
cat auto_inline.c
```

Observa:

- `tiny_add()` -- funcion de una linea, sin marcar como `inline`
- `medium_work()` -- funcion con dos loops y condicionales, sin marcar como
  `inline`
- Ninguna tiene `inline` explicito

### Paso 3.2 -- Compilar y verificar que funciona

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic auto_inline.c -o auto_inline
./auto_inline
```

Salida esperada:

```
tiny_add(10, 20) = 30
medium_work(30) = 269
```

### Paso 3.3 -- Generar assembly sin optimizacion

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -S -O0 auto_inline.c -o auto_inline_O0.s
```

```bash
grep "call" auto_inline_O0.s
```

Salida esperada:

```
	call	tiny_add
	call	medium_work
	call	printf
	call	printf
```

Sin optimizacion, ambas funciones se llaman con `call`. Esto es lo esperable.

### Paso 3.4 -- Generar assembly con optimizacion

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -S -O2 auto_inline.c -o auto_inline_O2.s
```

Antes de examinar, predice:

- `tiny_add()` (una sola linea: `return a + b`) sera inlineada por el
  compilador aunque no tenga la palabra `inline`?
- `medium_work()` (dos loops con condicionales) sera inlineada?

### Paso 3.5 -- Verificar la prediccion

```bash
grep "call" auto_inline_O2.s
```

Salida esperada:

```
	call	medium_work
	call	printf
	call	printf
```

`tiny_add` desaparecio de las llamadas -- el compilador la inlineo
automaticamente porque es lo suficientemente pequena. `medium_work` sigue como
`call` porque su cuerpo es grande y el costo de expandirla superaria el
beneficio.

### Paso 3.6 -- Confirmar con las etiquetas de funcion

```bash
echo "=== -O0: etiquetas de funcion ==="
grep -E "^tiny_add:|^medium_work:" auto_inline_O0.s

echo "=== -O2: etiquetas de funcion ==="
grep -E "^tiny_add:|^medium_work:" auto_inline_O2.s
```

Salida esperada:

```
=== -O0: etiquetas de funcion ===
tiny_add:
medium_work:

=== -O2: etiquetas de funcion ===
tiny_add:
medium_work:
```

Aunque `tiny_add` fue inlineada en `main`, su etiqueta sigue en el assembly.
A diferencia de `static inline` (parte 1), aqui la funcion no es `static`, asi
que el compilador conserva una copia por si otro archivo la necesita al enlazar.

### Paso 3.7 -- Comparar tamano del assembly

```bash
wc -l auto_inline_O0.s auto_inline_O2.s
```

Salida esperada:

```
 ~134 auto_inline_O0.s
 ~103 auto_inline_O2.s
 ~237 total
```

Con `-O2`, el assembly se reduce. El compilador elimino overhead de la llamada
a `tiny_add` y optimizo el codigo en general.

### Limpieza de la parte 3

```bash
rm -f auto_inline auto_inline_O0.s auto_inline_O2.s
```

---

## Limpieza final

```bash
rm -f *.s *.o square_regular square_inline multi_inline auto_inline
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  auto_inline.c  caller_a.c  caller_b.c  main_multi.c  math_inline.h  square_inline.c  square_regular.c
```

---

## Conceptos reforzados

1. `inline` es una **sugerencia** al compilador, no una orden. Con `-O0`, el
   compilador la ignora y genera `call` normalmente.

2. `static inline` en un header es la forma idiomatica en C. `static` evita
   conflictos de enlace entre translation units, e `inline` sugiere expansion
   en linea.

3. Con `-O2`, las funciones `static inline` que se inlinean desaparecen
   completamente del assembly -- no queda ni la etiqueta ni el simbolo en la
   tabla.

4. El compilador con `-O2` inlinea funciones pequenas **automaticamente**, sin
   necesidad de que el programador escriba `inline`. El criterio es el tamano y
   la frecuencia de llamada.

5. Funciones grandes no se inlinean ni con `-O2` -- el costo de duplicar el
   codigo supera el ahorro de eliminar la llamada.

6. Sin `static`, una funcion inlineada por el compilador conserva su etiqueta
   en el assembly (por si otro translation unit la necesita). Con `static
   inline`, el compilador puede eliminarla por completo si no queda ninguna
   llamada sin inlinear.
