# Lab — Arrays y punteros

## Objetivo

Explorar la relacion entre arrays y punteros en C: observar el decay
automatico, verificar las diferencias fundamentales entre un array y un puntero,
recorrer arrays con punteros, y entender la diferencia de tipos entre `arr` y
`&arr`. Al finalizar, sabras por que un array no es un puntero aunque se
comporte como uno en muchos contextos.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `decay.c` | Demuestra decay y equivalencia `arr[i]` / `*(arr+i)` |
| `differences.c` | Compara sizeof, direcciones y comportamiento en funciones |
| `iteration.c` | Cuatro formas de recorrer un array |
| `arr_vs_addr.c` | Puntero a int vs puntero a array completo |

---

## Parte 1 — Decay: de array a puntero

**Objetivo**: Verificar que el nombre de un array decae a un puntero al primer
elemento, y que `arr[i]` es exactamente `*(arr + i)`.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat decay.c
```

Observa:

- `p` se inicializa con `arr` (sin `&`)
- `q` se inicializa con `&arr[0]` (explicitamente)
- El programa compara ambos y luego muestra las direcciones de cada elemento

### Paso 1.2 — Compilar y ejecutar

Antes de ejecutar, predice:

- `p` y `q` tendran la misma direccion o diferente?
- `arr[2]`, `*(arr + 2)` y `*(p + 2)` daran el mismo valor?
- Cuantos bytes habra entre `arr + 0` y `arr + 1`? (pista: `sizeof(int)`)

Intenta responder antes de continuar al siguiente paso.

### Paso 1.3 — Verificar la prediccion

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic decay.c -o decay
./decay
```

Salida esperada (las direcciones varian):

```
arr          = 0x7fff...
&arr[0]      = 0x7fff...
p (= arr)    = 0x7fff...
q (= &arr[0])= 0x7fff...
p == q: true

--- arr[i] vs *(arr + i) ---
arr[0] = 10,  *(arr + 0) = 10,  *(p + 0) = 10
arr[1] = 20,  *(arr + 1) = 20,  *(p + 1) = 20
arr[2] = 30,  *(arr + 2) = 30,  *(p + 2) = 30
arr[3] = 40,  *(arr + 3) = 40,  *(p + 3) = 40
arr[4] = 50,  *(arr + 4) = 50,  *(p + 4) = 50

--- pointer arithmetic addresses ---
arr + 0 = 0x7fff...  (diff from arr: 0 bytes)
arr + 1 = 0x7fff...  (diff from arr: 4 bytes)
arr + 2 = 0x7fff...  (diff from arr: 8 bytes)
arr + 3 = 0x7fff...  (diff from arr: 12 bytes)
arr + 4 = 0x7fff...  (diff from arr: 16 bytes)
```

Analisis:

- `arr`, `&arr[0]`, `p` y `q` son la misma direccion. `int *p = arr` y
  `int *q = &arr[0]` son equivalentes porque `arr` decae a `&arr[0]`.
- Las tres formas de acceso (`arr[i]`, `*(arr + i)`, `*(p + i)`) dan el mismo
  valor. El compilador transforma `arr[i]` en `*(arr + i)` internamente.
- Cada elemento esta separado por 4 bytes (`sizeof(int)` en la mayoria de
  plataformas). La aritmetica de punteros avanza en unidades del tipo apuntado,
  no en bytes.

### Limpieza de la parte 1

```bash
rm -f decay
```

---

## Parte 2 — Diferencias: array no es puntero

**Objetivo**: Demostrar que a pesar del decay, un array y un puntero son cosas
diferentes. Las diferencias se manifiestan en `sizeof`, en el tipo de `&`, y en
el comportamiento dentro de funciones.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat differences.c
```

Observa:

- La funcion `show_sizeof` recibe `int param[]` y aplica `sizeof` al parametro
- En `main`, se comparan `sizeof(arr)` vs `sizeof(ptr)`
- Se muestran las direcciones de `arr`, `&arr` y `&arr[0]`
- Se comparan `arr + 1` vs `&arr + 1`

### Paso 2.2 — Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic differences.c -o differences
```

Observa el warning del compilador:

```
warning: 'sizeof' on array function parameter 'param' will return size of
         'int *' [-Wsizeof-array-argument]
```

Este warning es intencional. GCC te avisa que `sizeof(param)` dentro de la
funcion devolvera el tamano del puntero (8 bytes), no del array original. Es el
"sizeof trap" que la teoria menciona.

### Paso 2.3 — Ejecutar

Antes de ver la salida, predice:

- `sizeof(arr)` en main: cuanto dara? (5 ints de 4 bytes cada uno...)
- `sizeof(ptr)` en main: cuanto dara? (tamano de un puntero en 64-bit...)
- `sizeof(param)` dentro de la funcion: cuanto dara?
- `arr + 1` avanzara cuantos bytes? Y `&arr + 1`?

Intenta responder antes de continuar al siguiente paso.

### Paso 2.4 — Verificar la prediccion

```bash
./differences
```

Salida esperada (las direcciones varian):

```
=== sizeof ===
sizeof(arr) = 20  (total array size: 5 elements * 4 bytes)
sizeof(ptr) = 8  (pointer size)

=== sizeof inside function ===
Inside function:
  sizeof(param)    = 8  (pointer size)
  sizeof(param[0]) = 4  (element size)
  n (passed)       = 5

=== addresses: arr vs &arr ===
arr      = 0x7fff...  (decays to &arr[0], type: int *)
&arr     = 0x7fff...  (type: int (*)[5])
&arr[0]  = 0x7fff...  (type: int *)
Same numeric value: true

=== arr + 1 vs &arr + 1 ===
arr          = 0x7fff...
arr + 1      = 0x7fff...  (advanced 4 bytes = sizeof(int))
&arr + 1     = 0x7fff...  (advanced 20 bytes = sizeof(int[5]))
```

Analisis:

- `sizeof(arr)` devuelve 20 (el tamano total del array). `sizeof(ptr)` devuelve
  8 (tamano del puntero). Esta es la diferencia mas importante: `sizeof`
  distingue un array de un puntero.
- Dentro de la funcion, `param` ya no es un array, es un puntero. Por eso
  `sizeof(param)` da 8, no 20. Es la razon por la que siempre se pasa `n` como
  parametro separado.
- `arr`, `&arr` y `&arr[0]` tienen el mismo valor numerico, pero tipos
  distintos. `arr + 1` avanza 4 bytes (un `int`), mientras que `&arr + 1` avanza
  20 bytes (un array completo de 5 ints). El tipo determina el tamano del paso.

### Limpieza de la parte 2

```bash
rm -f differences
```

---

## Parte 3 — Iteracion con punteros

**Objetivo**: Comparar cuatro formas de recorrer un array y verificar que todas
producen el mismo resultado, observando las direcciones de memoria en cada paso.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat iteration.c
```

Observa las cuatro formas:

1. Indice clasico: `arr[i]`
2. Puntero que se incrementa: `p++`
3. Aritmetica: `*(arr + i)`
4. Sentinel (puntero end): `p != end`

### Paso 3.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic iteration.c -o iteration
./iteration
```

Salida esperada (las direcciones varian):

```
=== Method 1: index (arr[i]) ===
arr[0] = 5
arr[1] = 10
arr[2] = 15
arr[3] = 20
arr[4] = 25
arr[5] = 30

=== Method 2: pointer increment (p++) ===
*p = 5  (p = 0x7fff...)
*p = 10  (p = 0x7fff...)
*p = 15  (p = 0x7fff...)
*p = 20  (p = 0x7fff...)
*p = 25  (p = 0x7fff...)
*p = 30  (p = 0x7fff...)

=== Method 3: arithmetic (*(arr + i)) ===
*(arr + 0) = 5
*(arr + 1) = 10
*(arr + 2) = 15
*(arr + 3) = 20
*(arr + 4) = 25
*(arr + 5) = 30

=== Method 4: pointer with end sentinel ===
arr   = 0x7fff...
end   = 0x7fff...  (one past last element)
count = 6 elements
*p = 5
*p = 10
*p = 15
*p = 20
*p = 25
*p = 30
```

Analisis:

- Las cuatro formas producen exactamente los mismos valores. En el metodo 2,
  observa como la direccion del puntero avanza 4 bytes en cada iteracion (un
  `int`).
- El metodo 4 usa un puntero "one past the end" (`arr + n`). Este puntero es
  valido como direccion pero NO se puede desreferenciar. Es el mismo patron que
  usan los iteradores en C++.
- La condicion `p < arr + n` (metodo 2) y `p != end` (metodo 4) son equivalentes
  para recorridos lineales. La primera es mas defensiva ante errores.

### Paso 3.3 — Observar las direcciones

En la salida del metodo 2, verifica que cada direccion esta separada por 4
bytes de la anterior. Ejemplo:

```
0x7fff...eb0 -> 0x7fff...eb4 -> 0x7fff...eb8 -> ...
```

Cada incremento de `p++` avanza `sizeof(int)` bytes. El compilador sabe el
tipo apuntado y calcula el desplazamiento automaticamente.

### Limpieza de la parte 3

```bash
rm -f iteration
```

---

## Parte 4 — arr vs &arr: puntero a int vs puntero a array

**Objetivo**: Entender la diferencia de tipos entre `int *` (puntero a int) e
`int (*)[5]` (puntero a array de 5 ints), y como esto afecta la aritmetica de
punteros.

### Paso 4.1 — Examinar el codigo fuente

```bash
cat arr_vs_addr.c
```

Observa:

- `p_elem` es `int *` — apunta a un solo `int`
- `p_arr` es `int (*)[5]` — apunta a un array completo de 5 ints
- Ambos se inicializan con la misma direccion pero con tipos distintos

### Paso 4.2 — Compilar y ejecutar

Antes de ejecutar, predice:

- `p_elem + 1` avanzara cuantos bytes?
- `p_arr + 1` avanzara cuantos bytes?
- `*p_elem` dara que tipo de valor? Y `*p_arr`?

Intenta responder antes de continuar al siguiente paso.

### Paso 4.3 — Verificar la prediccion

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic arr_vs_addr.c -o arr_vs_addr
./arr_vs_addr
```

Salida esperada (las direcciones varian):

```
=== Types and values ===
p_elem (arr)   = 0x7fff...  (int *)
p_arr  (&arr)  = 0x7fff...  (int (*)[5])
Same address: true

=== Step size: +1 ===
p_elem + 1     = 0x7fff...  (advanced 4 bytes)
p_arr + 1      = 0x7fff...  (advanced 20 bytes)

=== Accessing through pointer-to-array ===
(*p_arr)[0] = 100
(*p_arr)[1] = 200
(*p_arr)[2] = 300
(*p_arr)[3] = 400
(*p_arr)[4] = 500

=== Dereferencing comparison ===
*p_elem        = 100  (first element)
*p_arr         = 0x7fff...  (the array itself, decays to &arr[0])
**p_arr        = 100  (first element via double deref)
```

Analisis:

- Ambos punteros contienen la misma direccion numerica, pero el tipo es
  diferente. `p_elem` es `int *` y `p_arr` es `int (*)[5]`.
- `p_elem + 1` avanza 4 bytes (un `int`). `p_arr + 1` avanza 20 bytes (un
  array entero de 5 ints). El tipo del puntero determina el tamano del paso.
- `*p_elem` desreferencia a un `int` (valor 100). `*p_arr` desreferencia a un
  `int[5]` (el array completo, que a su vez decae a `int *`). Por eso se
  necesita `**p_arr` para llegar al primer elemento, o `(*p_arr)[i]` para
  acceder por indice.
- La sintaxis `int (*p)[5]` (puntero a array) no debe confundirse con
  `int *p[5]` (array de 5 punteros). Los parentesis cambian completamente el
  significado.

### Limpieza de la parte 4

```bash
rm -f arr_vs_addr
```

---

## Limpieza final

```bash
rm -f decay differences iteration arr_vs_addr
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  arr_vs_addr.c  decay.c  differences.c  iteration.c
```

---

## Conceptos reforzados

1. El nombre de un array decae automaticamente a un puntero al primer elemento
   (`arr` se convierte en `&arr[0]`), pero esto no significa que un array sea un
   puntero.

2. `arr[i]` es exactamente `*(arr + i)` — el compilador transforma la notacion
   de corchetes en aritmetica de punteros. La suma es conmutativa, por eso
   `i[arr]` tambien compila (pero nunca se debe usar).

3. La aritmetica de punteros avanza en unidades del tipo apuntado, no en bytes.
   `int *p; p + 1` avanza `sizeof(int)` bytes (tipicamente 4).

4. `sizeof(arr)` devuelve el tamano total del array (en bytes), pero
   `sizeof(ptr)` devuelve el tamano del puntero (8 en 64-bit). Esta diferencia
   desaparece dentro de funciones: un parametro `int arr[]` es en realidad
   `int *`, y `sizeof` devolvera 8.

5. `arr` y `&arr` tienen el mismo valor numerico pero tipos distintos: `arr`
   decae a `int *` y `&arr` es `int (*)[5]`. La consecuencia practica es que
   `arr + 1` avanza `sizeof(int)` bytes, mientras que `&arr + 1` avanza
   `sizeof(int[5])` bytes (el array completo).

6. Un puntero "one past the end" (`arr + n`) es valido como direccion para
   comparaciones, pero desreferenciarlo es comportamiento indefinido. Es un
   patron comun para marcar el final de un recorrido.
