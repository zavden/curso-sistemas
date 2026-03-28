# Lab — Calificadores

## Objetivo

Experimentar con `const`, `volatile` y `restrict` para entender como cada
calificador afecta al comportamiento del compilador. Al finalizar, sabras
interpretar errores de `const`, leer la diferencia en assembly que produce
`volatile`, y reconocer la vectorizacion que habilita `restrict`.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `const_basics.c` | Las cuatro combinaciones de const con punteros |
| `const_errors.c` | Errores de compilacion al violar const |
| `const_function.c` | Funcion con parametro const (const correctness) |
| `volatile_demo.c` | Lectura doble con y sin volatile |
| `volatile_loop.c` | Loop con y sin volatile (inspeccion de assembly) |
| `restrict_demo.c` | Suma de arrays con y sin restrict |

---

## Parte 1 — const

**Objetivo**: Entender las cuatro combinaciones de `const` con punteros,
provocar errores de compilacion, y practicar const correctness en funciones.

### Paso 1.1 — Verificar el entorno

```bash
gcc --version
```

Confirma que GCC esta instalado.

### Paso 1.2 — Las cuatro combinaciones de const

```bash
cat const_basics.c
```

Observa las cuatro combinaciones:

| Declaracion | Dato | Puntero |
|-------------|------|---------|
| `const int *p` | No modificable | Reasignable |
| `int *const p` | Modificable | No reasignable |
| `const int *const p` | No modificable | No reasignable |
| `int *p` | Modificable | Reasignable |

Antes de compilar, predice para cada seccion del programa:

- `ptr_to_const`: se puede reasignar a `&mutable_var`?
- `const_ptr`: se puede escribir `*const_ptr = 99`?
- `both_const`: se puede hacer algo con el?

### Paso 1.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic const_basics.c -o const_basics
./const_basics
```

Salida esperada:

```
max = 100
mutable_var = 50

--- Pointer to const ---
*ptr_to_const = 100
After reassign, *ptr_to_const = 50

--- Const pointer ---
After *const_ptr = 99, mutable_var = 99

--- Both const ---
*both_const = 100
```

Verifica tus predicciones:

- `ptr_to_const` se reasigno sin problema (el puntero no es const)
- `*const_ptr = 99` funciono (el dato no es const, solo el puntero)
- `both_const` solo se pudo leer

### Paso 1.4 — Provocar errores de const

```bash
cat const_errors.c
```

El archivo tiene cuatro lineas comentadas. Cada una viola `const` de una forma
diferente. Primero, compila tal como esta:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic const_errors.c -o const_errors
./const_errors
```

Salida esperada:

```
*p3 = 10 (both const — cannot modify data or pointer)
After p1 = &y: *p1 = 30
After *p2 = 30: y = 30
```

### Paso 1.5 — Descomentar el primer error

Abre `const_errors.c` y descomenta la linea del ERROR 1:

```c
*p1 = 30;
```

Antes de compilar, predice: que mensaje de error daraGCC? Recuerda que `p1`
es `const int *p1` (puntero a const).

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic const_errors.c -o const_errors
```

Salida esperada (error):

```
error: assignment of read-only location '*p1'
```

GCC impide modificar el dato a traves de un puntero a const. Vuelve a
comentar la linea antes de continuar.

### Paso 1.6 — Descomentar el segundo error

Descomenta la linea del ERROR 2:

```c
p2 = &x;
```

Predice: `p2` es `int *const p2`. Que parte es const — el dato o el puntero?

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic const_errors.c -o const_errors
```

Salida esperada (error):

```
error: assignment of read-only variable 'p2'
```

GCC impide reasignar un puntero const. Vuelve a comentar la linea.

### Paso 1.7 — Probar los otros dos errores

Descomenta ERROR 3 (`*p3 = 30;`) y compila. Observa el error.
Luego descomenta ERROR 4 (`p3 = &y;`) y compila. Observa el error.

Ambos fallan porque `p3` es `const int *const p3` — ni el dato ni el puntero
se pueden modificar. Asegurate de volver a comentar todas las lineas de error
antes de continuar.

### Paso 1.8 — const correctness en funciones

```bash
cat const_function.c
```

Observa que `find_max` recibe `const int *arr`. Esto comunica al compilador y
al lector que la funcion no modificara el array.

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic const_function.c -o const_function
./const_function
```

Salida esperada:

```
Array: 3 7 2 9 5 1 8
Max value: 9
Array after find_max: 3 7 2 9 5 1 8
```

El array no fue modificado — const lo garantizo en compilacion.

### Paso 1.9 — Intentar violar const en la funcion

Abre `const_function.c` y descomenta la linea dentro de `find_max`:

```c
arr[0] = 999;
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic const_function.c -o const_function
```

Salida esperada (error):

```
error: assignment of read-only location '*arr'
```

El compilador detecto el intento de modificar datos a traves de un puntero a
const. Vuelve a comentar la linea.

### Limpieza de la parte 1

```bash
rm -f const_basics const_errors const_function
```

---

## Parte 2 — volatile

**Objetivo**: Observar en el assembly como `volatile` impide que el compilador
optimice lecturas de memoria.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat volatile_demo.c
```

Observa las dos funciones:

- `read_normal()`: lee `normal_var` dos veces y suma
- `read_volatile()`: lee `volatile_var` dos veces y suma

Ambas hacen lo mismo logicamente. La diferencia esta en como el compilador
las optimiza.

### Paso 2.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic volatile_demo.c -o volatile_demo
./volatile_demo
```

Salida esperada:

```
read_normal()   = 200
read_volatile() = 200
```

El resultado es identico. La diferencia no esta en el resultado — esta en el
codigo maquina generado.

### Paso 2.3 — Generar assembly con optimizacion

```bash
gcc -std=c11 -S -O2 volatile_demo.c -o volatile_demo.s
```

Antes de examinar el assembly, predice:

- En `read_normal`, el compilador leera `normal_var` de memoria una vez o dos?
- En `read_volatile`, el compilador leera `volatile_var` de memoria una vez o dos?

### Paso 2.4 — Inspeccionar read_normal

```bash
sed -n '/^read_normal:/,/ret/p' volatile_demo.s
```

Salida esperada (aproximada, puede variar entre versiones de GCC):

```
read_normal:
	movl	normal_var(%rip), %eax
	addl	%eax, %eax
	ret
```

El compilador leyo `normal_var` **una sola vez** y duplico el valor con
`addl %eax, %eax`. Optimizo dos lecturas a una porque "sabe" que nadie
cambio la variable entre las dos lecturas.

### Paso 2.5 — Inspeccionar read_volatile

```bash
sed -n '/^read_volatile:/,/ret/p' volatile_demo.s
```

Salida esperada (aproximada):

```
read_volatile:
	movl	volatile_var(%rip), %eax
	movl	volatile_var(%rip), %edx
	addl	%edx, %eax
	ret
```

Con `volatile`, el compilador leyo la variable **dos veces** de memoria
(`movl` aparece dos veces). No puede optimizar porque `volatile` le dice que
el valor podria haber cambiado entre las dos lecturas.

### Paso 2.6 — volatile en un loop

```bash
cat volatile_loop.c
```

Este archivo tiene dos funciones con loops `while(flag)`:

- `loop_normal`: usa `int normal_flag`
- `loop_volatile`: usa `volatile int volatile_flag`

Antes de ver el assembly, predice: con `-O2`, el compilador convertira
`loop_normal` en un loop infinito que nunca re-lee la variable?

```bash
gcc -std=c11 -S -O2 volatile_loop.c -o volatile_loop.s
```

### Paso 2.7 — Inspeccionar loop_normal

```bash
sed -n '/^loop_normal:/,/\.size.*loop_normal/p' volatile_loop.s
```

Salida esperada (aproximada):

```
loop_normal:
	movl	normal_flag(%rip), %eax
	testl	%eax, %eax
	jne	.L3
	ret
.L3:
	jmp	.L3
```

El compilador leyo `normal_flag` **una sola vez** antes del loop. Si el flag
es 1, entra en `.L3: jmp .L3` — un loop infinito que **nunca re-lee** la
variable. Aunque otro thread o una interrupcion cambie `normal_flag` a 0, este
loop nunca se enterara.

### Paso 2.8 — Inspeccionar loop_volatile

```bash
sed -n '/^loop_volatile:/,/\.size.*loop_volatile/p' volatile_loop.s
```

Salida esperada (aproximada):

```
loop_volatile:
.L6:
	movl	volatile_flag(%rip), %eax
	testl	%eax, %eax
	jne	.L6
	ret
```

Con `volatile`, el compilador re-lee `volatile_flag` de memoria **en cada
iteracion** del loop (`movl` esta dentro del loop `.L6`). Si algo externo
cambia el flag a 0, el loop lo detectara y terminara.

Esta es la razon principal de `volatile` en sistemas embebidos: sin el, el
compilador asume que "nadie mas" puede tocar la variable y optimiza las
lecturas.

### Limpieza de la parte 2

```bash
rm -f volatile_demo volatile_demo.s volatile_loop.s
```

---

## Parte 3 — restrict

**Objetivo**: Ver como `restrict` permite al compilador vectorizar operaciones
con instrucciones SIMD.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat restrict_demo.c
```

Observa las dos funciones:

- `add_arrays`: suma `src[i]` a `dst[i]`, sin restrict
- `add_arrays_restrict`: lo mismo, con `restrict` en ambos punteros

La logica es identica. La diferencia es la promesa al compilador: con
`restrict`, el programador garantiza que `dst` y `src` no apuntan a memoria
solapada.

### Paso 3.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic restrict_demo.c -o restrict_demo
./restrict_demo
```

Salida esperada:

```
Result after add_arrays: 11 22 33 44 55
Result after add_arrays_restrict: 11 22 33 44 55
```

Mismo resultado. La diferencia esta en el assembly.

### Paso 3.3 — Generar assembly con optimizacion

```bash
gcc -std=c11 -S -O2 restrict_demo.c -o restrict_demo.s
```

Antes de inspeccionar, predice:

- La version sin restrict usara instrucciones SIMD (como `movdqu`, `paddd`)?
- La version con restrict usara instrucciones SIMD?
- Cual funcion tendra mas lineas de assembly?

### Paso 3.4 — Inspeccionar add_arrays (sin restrict)

```bash
sed -n '/^add_arrays:/,/ret/p' restrict_demo.s | head -20
```

Salida esperada (aproximada):

```
add_arrays:
	testq	%rdx, %rdx
	je	.L1
	xorl	%eax, %eax
.L3:
	movl	(%rsi,%rax,4), %ecx
	addl	%ecx, (%rdi,%rax,4)
	addq	$1, %rax
	cmpq	%rax, %rdx
	jne	.L3
.L1:
	ret
```

Un loop simple elemento por elemento. El compilador no puede vectorizar porque
`dst` y `src` podrian solaparse. Si `dst[0]` y `src[3]` fueran la misma
direccion, procesar 4 elementos a la vez daria un resultado incorrecto.

### Paso 3.5 — Inspeccionar add_arrays_restrict (con restrict)

```bash
sed -n '/^add_arrays_restrict:/,/^\.L/p' restrict_demo.s | head -25
```

Busca instrucciones como `movdqu`, `paddd`, `movups` en la salida. Estas son
instrucciones SIMD (SSE/AVX) que procesan 4 enteros de 32 bits a la vez.

Salida esperada (aproximada, las instrucciones exactas dependen de la version
de GCC y la arquitectura):

```
add_arrays_restrict:
	...
	movdqu	(%rdi,%rax), %xmm0
	movdqu	(%rsi,%rax), %xmm1
	paddd	%xmm1, %xmm0
	movups	%xmm0, (%rdi,%rax)
	...
```

Observa:

- `movdqu`: carga 128 bits (4 ints) de memoria a un registro SIMD
- `paddd`: suma 4 pares de enteros de 32 bits en paralelo
- `movups`: almacena el resultado de vuelta en memoria

Con `restrict`, el compilador sabe que `dst` y `src` no se solapan. Es seguro
leer 4 elementos de `src` y escribir 4 en `dst` simultaneamente. Esto es
**vectorizacion**: una instruccion SIMD hace el trabajo de 4 operaciones
escalares.

### Paso 3.6 — Contar lineas de assembly

```bash
echo "=== add_arrays (sin restrict) ==="
sed -n '/^add_arrays:/,/\.size.*add_arrays[^_]/p' restrict_demo.s | wc -l

echo "=== add_arrays_restrict (con restrict) ==="
sed -n '/^add_arrays_restrict:/,/\.size.*add_arrays_restrict/p' restrict_demo.s | wc -l
```

La version con `restrict` tiene mas lineas de assembly. Esto parece
contradictorio, pero la razon es que el compilador genero un loop vectorizado
(SIMD, procesando 4 elementos a la vez) y un loop escalar de respaldo para los
elementos restantes que no completan un grupo de 4. Mas codigo, pero mas
rapido en ejecucion.

### Paso 3.7 — Por que restrict importa

Sin `restrict`, el compilador debe asumir que `dst` y `src` podrian solaparse
(aliasing). Considerar este caso:

```c
int arr[5] = {1, 2, 3, 4, 5};
add_arrays(arr + 1, arr, 4);  /* dst y src se solapan */
```

Aqui `dst[0]` es `arr[1]` y `src[1]` tambien es `arr[1]`. Si el compilador
procesara 4 elementos a la vez, leeria `src[1]` antes de que `dst[0]` lo
modificara — resultado incorrecto. Por eso, sin `restrict`, va elemento por
elemento.

Con `restrict`, el programador promete que esto no ocurrira. Si la promesa
es falsa, el comportamiento es indefinido.

Esto es exactamente la diferencia entre `memcpy` (usa `restrict`, no permite
solapamiento) y `memmove` (no usa `restrict`, maneja solapamiento).

### Limpieza de la parte 3

```bash
rm -f restrict_demo restrict_demo.s
```

---

## Limpieza final

```bash
rm -f const_basics const_errors const_function
rm -f volatile_demo volatile_demo.s volatile_loop.s
rm -f restrict_demo restrict_demo.s
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  const_basics.c  const_errors.c  const_function.c  restrict_demo.c  volatile_demo.c  volatile_loop.c
```

---

## Conceptos reforzados

1. `const` con punteros tiene cuatro combinaciones: la posicion de `const`
   respecto al `*` determina si lo inmutable es el dato, el puntero, ambos,
   o ninguno. La regla es leer de derecha a izquierda.

2. `const` en parametros de funcion es una herramienta de comunicacion: le dice
   al compilador y al lector que la funcion no modificara los datos. El
   compilador rechaza cualquier intento de escritura a traves de un puntero a
   const.

3. `volatile` impide que el compilador optimice lecturas de memoria. Sin
   `volatile`, el compilador puede leer una variable una sola vez y reutilizar
   el valor; con `volatile`, debe leer de memoria en cada acceso.

4. Sin `volatile`, un loop `while(flag)` puede convertirse en un loop infinito
   `jmp .L3` que nunca re-lee `flag` de memoria, incluso si algo externo lo
   modifica. Esto es critico en sistemas embebidos con interrupciones.

5. `restrict` es una promesa de que el puntero es el unico acceso a esa
   memoria. Esto permite al compilador vectorizar con instrucciones SIMD
   (`movdqu`, `paddd`), procesando multiples elementos en paralelo en lugar de
   ir uno por uno.

6. La diferencia entre `memcpy` (tiene `restrict`) y `memmove` (no tiene
   `restrict`) se explica por aliasing: `memcpy` puede vectorizar porque el
   programador garantiza que no hay solapamiento; `memmove` debe ser
   conservador porque los buffers pueden solaparse.
