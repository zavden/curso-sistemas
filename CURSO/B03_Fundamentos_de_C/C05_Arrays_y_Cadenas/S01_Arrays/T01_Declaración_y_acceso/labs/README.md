# Lab — Declaracion y acceso a arrays

## Objetivo

Declarar arrays, explorar su tamano en memoria con sizeof, practicar distintas
formas de inicializacion, acceder a elementos (incluyendo fuera de limites),
usar VLAs y observar como un array pierde su informacion de tamano al pasarse
a una funcion. Al finalizar, sabras usar el patron ARRAY_SIZE y entenderas por
que sizeof no funciona con punteros.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `sizes.c` | Declaracion de arrays y sizeof |
| `init.c` | Inicializacion completa, parcial, designated initializers |
| `access.c` | Acceso, modificacion e iteracion |
| `oob.c` | Acceso fuera de limites (UB) |
| `vla.c` | Variable-Length Arrays (C99) |
| `decay.c` | Decay de array a puntero en funciones |

---

## Parte 1 — Declaracion y sizeof

**Objetivo**: Declarar arrays de distintos tipos y entender la relacion entre
sizeof del array, sizeof del elemento y cantidad de elementos.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat sizes.c
```

Observa que se declaran tres arrays de tipos distintos: `int`, `double` y
`char`. El programa imprime sizeof de cada tipo base, sizeof de cada array, y
el conteo de elementos calculado con `sizeof(arr) / sizeof(arr[0])`.

Antes de compilar y ejecutar, predice:

- Si `sizeof(int) = 4`, cuanto dara `sizeof(nums)` para `int nums[5]`?
- Si `sizeof(double) = 8`, cuanto dara `sizeof(temps)` para `double temps[3]`?
- `sizeof(char)` siempre es 1. Cuanto dara `sizeof(name)` para `char name[20]`?

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic sizes.c -o sizes
./sizes
```

Salida esperada:

```
sizeof(int)    = 4
sizeof(double) = 8
sizeof(char)   = 1

sizeof(nums)   = 20  (5 ints)
sizeof(temps)  = 24  (3 doubles)
sizeof(name)   = 20  (20 chars)

count nums  = 5
count temps = 3
count name  = 20
```

Analisis:

- `sizeof(nums) = 20` porque `5 * sizeof(int) = 5 * 4 = 20`
- `sizeof(temps) = 24` porque `3 * sizeof(double) = 3 * 8 = 24`
- El conteo de elementos es siempre `sizeof(arr) / sizeof(arr[0])`. Este es
  el patron estandar para obtener la longitud de un array en C.

### Limpieza de la parte 1

```bash
rm -f sizes
```

---

## Parte 2 — Inicializacion

**Objetivo**: Practicar las distintas formas de inicializar arrays y entender
que pasa con los elementos no inicializados explicitamente.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat init.c
```

Observa los distintos modos de inicializacion:

- `full[5] = {10, 20, 30, 40, 50}` — todos los elementos explicitamente
- `partial[5] = {10, 20}` — solo los primeros dos
- `zeros[5] = {0}` — todos a cero
- `auto_size[] = {1, 2, 3, 4}` — el compilador determina el tamano
- `desig[10] = {[0]=100, [5]=500, [9]=900}` — designated initializers (C99)

Antes de ejecutar, predice:

- Que valores tendra `partial` en las posiciones 2, 3 y 4?
- Cuantos elementos tendra `auto_size`?
- Que valor tendra `desig[3]`?

### Paso 2.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic init.c -o init
./init
```

Salida esperada:

```
full = {10, 20, 30, 40, 50}
partial = {10, 20, 0, 0, 0}
zeros = {0, 0, 0, 0, 0}

auto_size count = 4
auto_size = {1, 2, 3, 4}

desig = {100, 0, 0, 0, 0, 500, 0, 0, 0, 900}
```

Analisis:

- En inicializacion parcial, los elementos no especificados se ponen a **0**.
  Esta es una garantia del estandar C, no un accidente.
- `{0}` inicializa el primer elemento a 0, y el resto tambien a 0 por la
  regla anterior. Es el idioma estandar para "poner todo a cero".
- Con `[]` sin tamano, el compilador cuenta los inicializadores.
- Los designated initializers permiten establecer posiciones especificas.
  Los huecos se rellenan con 0.

### Limpieza de la parte 2

```bash
rm -f init
```

---

## Parte 3 — Acceso, out-of-bounds e iteracion

**Objetivo**: Acceder y modificar elementos, observar que pasa con accesos
fuera de limites, e iterar sobre un array.

### Paso 3.1 — Acceso e iteracion normal

```bash
cat access.c
```

El programa lee elementos, modifica uno y recorre el array con un `for`.

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic access.c -o access
./access
```

Salida esperada:

```
data[0] = 10
data[4] = 50

After data[2] = 99:
  data[0] = 10
  data[1] = 20
  data[2] = 99
  data[3] = 40
  data[4] = 50
```

La iteracion usa `ARRAY_SIZE(data)` para calcular el limite. Observa que el
patron `sizeof(data)/sizeof(data[0])` funciona porque `data` es un array
real (no un puntero).

### Paso 3.2 — Acceso fuera de limites

```bash
cat oob.c
```

El programa accede a `arr[5]` y `arr[10]` en un array de 5 elementos.

Antes de compilar, predice:

- El compilador dara un error? Un warning?
- Que valor imprimira `arr[5]`?

### Paso 3.3 — Compilar sin sanitizers

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic oob.c -o oob
./oob
```

Salida esperada:

```
arr[0] = 10
arr[4] = 50
arr[5] = <valor impredecible>  (UB: past the end)
arr[10] = <valor impredecible>  (UB: far past the end)
```

El programa **compila sin errores**. Los valores de `arr[5]` y `arr[10]` son
basura — cualquier dato que este en esa posicion de memoria. C no verifica
limites. Esto es **Undefined Behavior (UB)**: el programa puede imprimir
cualquier numero, crashear, o parecer que funciona correctamente.

### Paso 3.4 — Compilar con address sanitizer

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -fsanitize=address oob.c -o oob_asan
./oob_asan
```

Salida esperada (el programa abortara):

```
arr[0] = 10
arr[4] = 50
=================================================================
==<pid>==ERROR: AddressSanitizer: stack-buffer-overflow on address ...
```

Con `-fsanitize=address`, el compilador agrega verificaciones en runtime.
Detecta el acceso fuera de limites y aborta el programa con un reporte
detallado. Esta herramienta es invaluable para depurar bugs de memoria.

### Limpieza de la parte 3

```bash
rm -f access oob oob_asan
```

---

## Parte 4 — Arrays y funciones: decay a puntero, VLAs

**Objetivo**: Entender que un array "decae" a puntero cuando se pasa a una
funcion, lo cual hace que sizeof pierda la informacion de tamano. Explorar
los VLAs como alternativa.

### Paso 4.1 — El problema: sizeof dentro de una funcion

```bash
cat decay.c
```

Observa:

- `main()` declara `data[5]` y usa `sizeof(data)` — funciona correctamente
- `print_info()` recibe `int arr[]` y usa `sizeof(arr)` — da el tamano de
  un puntero, no del array

Antes de ejecutar, predice:

- Cuanto dara `sizeof(arr)` dentro de `print_info` en un sistema de 64 bits?
- Cuanto dara `sizeof(arr) / sizeof(arr[0])` con esos valores?

### Paso 4.2 — Compilar y observar los warnings

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic decay.c -o decay
```

Salida esperada (warnings):

```
decay.c: In function 'print_info':
decay.c:8:18: warning: 'sizeof' on array function parameter 'arr'
              will return size of 'int *' [-Wsizeof-array-argument]
```

El compilador **advierte** que `sizeof(arr)` no hace lo que parece. Este
warning existe porque es un error muy comun. La declaracion `int arr[]` en
un parametro de funcion es identica a `int *arr` — el array decae a puntero.

### Paso 4.3 — Ejecutar y ver el efecto

```bash
./decay
```

Salida esperada:

```
sizeof(data) in main = 20 (real array size)
ARRAY_SIZE(data) = 5 (correct count)

Calling print_info(data, 5):
  sizeof(arr) inside function = 8 (size of a pointer)
  ARRAY_SIZE would give: 2 (WRONG!)
  Correct iteration using len parameter:
    arr[0] = 10
    arr[1] = 20
    arr[2] = 30
    arr[3] = 40
    arr[4] = 50
```

Analisis:

- En `main`, `sizeof(data)` da 20 (5 ints * 4 bytes). Correcto.
- En `print_info`, `sizeof(arr)` da 8 (tamano de un puntero en x86-64).
  **ARRAY_SIZE daria 8/4 = 2**, que es completamente incorrecto.
- La solucion es pasar el tamano como parametro separado (`int len`). Este
  es el patron estandar en C: `funcion(array, tamano)`.

### Paso 4.4 — VLAs: sizeof en runtime

```bash
cat vla.c
```

Observa que `fill_and_print` declara `int data[n]` con `n` como parametro.
El tamano del array se determina en runtime.

Antes de ejecutar, predice:

- Cuanto dara `sizeof(data)` cuando `n = 3`?
- Y cuando `n = 8`?

### Paso 4.5 — Compilar y ejecutar el VLA

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic vla.c -o vla
./vla
```

Salida esperada:

```
n = 3, sizeof(data) = 12
  data = {0, 10, 20}
n = 5, sizeof(data) = 20
  data = {0, 10, 20, 30, 40}
n = 8, sizeof(data) = 32
  data = {0, 10, 20, 30, 40, 50, 60, 70}
```

Analisis:

- `sizeof(data)` cambia con cada llamada: 12, 20, 32. Con un array de tamano
  fijo, sizeof se evalua en compilacion. Con un VLA, se evalua en **runtime**.
- Los VLAs se alocan en el stack. No hay verificacion de tamano: si `n` es
  muy grande, se produce stack overflow sin ningun mensaje de error.
- C11 hizo los VLAs opcionales (`__STDC_NO_VLA__`). El kernel de Linux los
  prohibe con `-Wvla`. Para tamanos variables, la alternativa segura es
  `malloc`.

### Limpieza de la parte 4

```bash
rm -f decay vla
```

---

## Limpieza final

```bash
rm -f sizes init access oob oob_asan decay vla
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  access.c  decay.c  init.c  oob.c  sizes.c  vla.c
```

---

## Conceptos reforzados

1. `sizeof(array)` devuelve el tamano total en bytes del array. Dividido por
   `sizeof(array[0])` da el numero de elementos. Este es el patron ARRAY_SIZE.

2. La inicializacion parcial (`int arr[5] = {1, 2}`) pone los elementos no
   especificados a **0**. Esto es una garantia del estandar, no del compilador.

3. Los designated initializers (`[indice] = valor`) permiten inicializar
   posiciones especificas. Los huecos se rellenan con 0.

4. C **no verifica limites** al acceder a arrays. Un acceso fuera de limites
   compila sin errores y produce Undefined Behavior. El sanitizer
   `-fsanitize=address` detecta estos errores en runtime.

5. Cuando un array se pasa a una funcion, **decae a puntero**. `sizeof` ya
   no devuelve el tamano del array sino el tamano de un puntero (8 bytes en
   x86-64). La solucion es pasar el tamano como parametro adicional.

6. Los VLAs (C99) permiten arrays de tamano variable en el stack, pero son
   peligrosos (stack overflow sin control) y opcionales en C11. La alternativa
   segura es `malloc`.
