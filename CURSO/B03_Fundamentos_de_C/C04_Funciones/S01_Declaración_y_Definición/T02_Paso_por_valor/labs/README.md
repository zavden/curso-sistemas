# Lab -- Paso por valor

## Objetivo

Comprobar experimentalmente que C pasa todos los argumentos por valor (copia),
y practicar las tecnicas para simular paso por referencia usando punteros. Al
finalizar, sabras predecir cuando una funcion puede o no modificar una variable
del caller, y elegir entre pasar structs por valor o por puntero.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `pass_by_value.c` | Funcion que intenta modificar un argumento int |
| `swap.c` | Swap incorrecto (por valor) y correcto (por puntero) |
| `struct_pass.c` | Struct pequeno y grande pasados por valor y por puntero |
| `array_decay.c` | sizeof de array en main vs dentro de funcion, modificacion |

---

## Parte 1 -- Paso por valor con escalares

**Objetivo**: Verificar que la funcion recibe una copia del argumento y que
modificar la copia no afecta al original.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat pass_by_value.c
```

Observa que `try_to_modify()` recibe un `int x` y le asigna `999`. La pregunta
clave: cuando la funcion retorne, el `a` de `main` valdra 42 o 999?

Predice antes de compilar:

- Que valor imprimira `a` despues de la llamada?
- La linea "Inside (after)" mostrara 999?

### Paso 1.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic pass_by_value.c -o pass_by_value
./pass_by_value
```

Salida esperada:

```
Before call: a = 42
  Inside (before):  x = 42
  Inside (after):   x = 999
After call:  a = 42
```

La funcion modifico su copia `x` a 999, pero `a` en `main` sigue siendo 42.
Esto es paso por valor: la funcion trabajo con una copia independiente.

### Paso 1.3 -- Verificar con direcciones de memoria

Vamos a confirmar que `x` y `a` viven en direcciones distintas. Modifica
temporalmente el programa:

```bash
cat > pass_by_value_addr.c << 'EOF'
#include <stdio.h>

void try_to_modify(int x) {
    printf("  Address of x: %p\n", (void *)&x);
    x = 999;
}

int main(void) {
    int a = 42;
    printf("Address of a: %p\n", (void *)&a);
    try_to_modify(a);
    printf("a = %d (unchanged)\n", a);
    return 0;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic pass_by_value_addr.c -o pass_by_value_addr
./pass_by_value_addr
```

Salida esperada:

```
Address of a: 0x7fff...
  Address of x: 0x7fff...
a = 42 (unchanged)
```

Las dos direcciones son distintas. `a` y `x` son variables independientes en el
stack. La copia se creo al entrar a la funcion y se destruyo al salir.

### Limpieza de la parte 1

```bash
rm -f pass_by_value pass_by_value_addr pass_by_value_addr.c
```

---

## Parte 2 -- Simular paso por referencia (swap)

**Objetivo**: Demostrar que para modificar la variable del caller, se necesita
pasar un puntero. El ejemplo clasico es swap.

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat swap.c
```

Observa las dos funciones:

- `swap_wrong(int a, int b)` -- recibe copias, intercambia las copias
- `swap_correct(int *a, int *b)` -- recibe punteros, intercambia los datos apuntados

Antes de ejecutar, predice:

- Despues de `swap_wrong`, los valores de `x` y `y` cambiaran?
- Despues de `swap_correct`, los valores de `x` y `y` cambiaran?

### Paso 2.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic swap.c -o swap
./swap
```

Salida esperada:

```
Original:          x = 5, y = 10
  Inside swap_wrong: a = 10, b = 5
After swap_wrong:  x = 5, y = 10
After swap_correct: x = 10, y = 5
```

`swap_wrong` intercambio sus copias locales (la linea "Inside" lo muestra),
pero `x` y `y` no cambiaron. `swap_correct` recibio las direcciones de `x` y
`y`, y a traves de `*a` y `*b` modifico los datos originales.

### Paso 2.3 -- Entender el mecanismo

Nota importante: incluso con `swap_correct`, el puntero se pasa **por valor**.
Lo que se copia es la direccion, no el dato. La funcion recibe una copia de la
direccion, pero esa copia apunta al mismo lugar en memoria que el original.

Piensa en ello asi:

- `swap_wrong(x, y)` -- copia el 5 y el 10, trabaja con las copias
- `swap_correct(&x, &y)` -- copia las direcciones de x e y, accede a los originales a traves de ellas

### Limpieza de la parte 2

```bash
rm -f swap
```

---

## Parte 3 -- Structs por valor vs por puntero

**Objetivo**: Comprobar que los structs se copian completos al pasar por valor,
y que pasar por puntero evita la copia. Medir el costo con sizeof.

### Paso 3.1 -- Examinar el codigo fuente

```bash
cat struct_pass.c
```

El programa define dos structs:

- `SmallPoint` -- 2 ints (8 bytes)
- `LargeBuffer` -- char[4096] + int (~4100 bytes)

Y cuatro funciones que los reciben por valor o por puntero.

Antes de ejecutar, predice:

- Despues de `modify_point_value`, los campos de `pt` cambiaran?
- Despues de `modify_point_ptr`, los campos de `pt` cambiaran?
- Dentro de `fill_buffer_value`, que reportara `sizeof(buf)`?
- Dentro de `fill_buffer_ptr`, que reportara `sizeof(buf)`?

### Paso 3.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic struct_pass.c -o struct_pass
./struct_pass
```

Salida esperada:

```
=== SmallPoint (sizeof = 8 bytes) ===
Before: pt = (10, 20)
  Inside (value): p = (999, 999)
After modify_point_value: pt = (10, 20)
  Inside (ptr):   p = (999, 999)
After modify_point_ptr:   pt = (999, 999)

=== LargeBuffer (sizeof = ~4100 bytes) ===
  sizeof(buf) inside (value): ~4100 bytes
  sizeof(buf) inside (ptr):   8 bytes (pointer size)
  sizeof(*buf) inside (ptr):  ~4100 bytes (struct size)
  buf->length = 4096
```

Observa:

- `modify_point_value` recibio una copia del struct, lo modifico internamente,
  pero `pt` en main no cambio
- `modify_point_ptr` recibio un puntero al struct y lo modifico a traves del
  puntero; `pt` en main si cambio
- `fill_buffer_value` copio los ~4100 bytes del struct al stack de la funcion
- `fill_buffer_ptr` solo copio 8 bytes (el puntero); accede a los datos
  originales sin copiarlos

### Paso 3.3 -- Visualizar el costo de la copia

Para un struct de ~4100 bytes, cada llamada por valor copia esos ~4100 bytes al
stack. En un loop de 1 millon de iteraciones, eso son ~4 GB de copias
innecesarias.

La regla practica:

- Structs pequenos (hasta ~16-32 bytes): por valor esta bien
- Structs grandes: pasar por puntero (`const` si no se modifica)

### Limpieza de la parte 3

```bash
rm -f struct_pass
```

---

## Parte 4 -- Arrays como argumentos

**Objetivo**: Comprobar que los arrays decaen a punteros al pasarlos a
funciones, que sizeof cambia, y que la funcion modifica el array original.

### Paso 4.1 -- Examinar el codigo fuente

```bash
cat array_decay.c
```

Observa:

- `print_sizeof` recibe `int arr[]` y usa `sizeof(arr)` dentro de la funcion
- `double_elements` recibe `int *arr` y multiplica cada elemento por 2
- `print_array` recibe `const int *arr` -- el `const` impide modificar

Antes de ejecutar, predice:

- En `main`, `sizeof(data)` para un array de 5 ints sera?
- Dentro de `print_sizeof`, `sizeof(arr)` sera el mismo valor?
- Despues de `double_elements`, el array en `main` estara modificado?

### Paso 4.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic array_decay.c -o array_decay
```

Observa que GCC produce un warning:

```
array_decay.c: In function 'print_sizeof':
array_decay.c:4:73: warning: 'sizeof' on array function parameter 'arr'
will return size of 'int *' [-Wsizeof-array-argument]
```

Este warning es exactamente el punto del ejercicio. GCC te advierte que
`sizeof(arr)` dentro de la funcion devuelve el tamano del **puntero**, no del
array. El compilador sabe que `int arr[]` en un parametro es realmente
`int *arr`.

Ahora ejecuta:

```bash
./array_decay
```

Salida esperada:

```
sizeof(data) in main: 20 (full array size)
  sizeof(arr) inside function: 8 (pointer size)
  Elements received: 5

Before double_elements: {1, 2, 3, 4, 5}
After double_elements : {2, 4, 6, 8, 10}
```

Observa:

- En `main`, `sizeof(data)` = 20 bytes (5 ints * 4 bytes)
- Dentro de la funcion, `sizeof(arr)` = 8 bytes (tamano del puntero en x86-64)
- La funcion **no sabe** el tamano del array -- por eso siempre se pasa `n`
  como parametro separado
- `double_elements` modifico el array original en `main`, porque `arr` es un
  puntero a los mismos datos

### Paso 4.3 -- Contraste con structs

Este comportamiento es lo opuesto a los structs de la parte 3:

| Tipo | Se pasa por | La funcion modifica el original? |
|------|-------------|----------------------------------|
| `int` | valor (copia) | No |
| `struct` | valor (copia completa) | No |
| `int[]` (array) | puntero (decae automaticamente) | Si |
| `int *` (puntero) | valor (copia de la direccion) | Si (el dato apuntado) |

Los arrays son la excepcion: C no copia arrays al pasarlos a funciones. Decaen
a un puntero al primer elemento. Por eso la funcion puede modificar el array
original.

### Limpieza de la parte 4

```bash
rm -f array_decay
```

---

## Limpieza final

```bash
rm -f pass_by_value pass_by_value_addr pass_by_value_addr.c
rm -f swap struct_pass array_decay
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  array_decay.c  pass_by_value.c  struct_pass.c  swap.c
```

---

## Conceptos reforzados

1. C pasa **todos** los argumentos por valor -- la funcion siempre recibe una
   copia. Esto aplica a ints, doubles, structs y punteros.

2. Para modificar una variable del caller, se pasa un **puntero** a esa
   variable. El puntero se pasa por valor (se copia la direccion), pero a
   traves de `*p` se accede al dato original.

3. `swap_wrong(int a, int b)` intercambia copias locales y no tiene efecto.
   `swap_correct(int *a, int *b)` intercambia los datos originales a traves
   de punteros.

4. Los structs se copian **completos** al pasar por valor. Para un struct
   grande (~4100 bytes), cada llamada copia todo ese bloque al stack. Pasar
   por puntero solo copia 8 bytes (la direccion).

5. Los arrays **no se copian** al pasar a funciones -- decaen a un puntero al
   primer elemento. Esto significa que `sizeof(arr)` dentro de la funcion
   devuelve el tamano del puntero (8 bytes), no del array. Por eso se pasa el
   tamano como parametro separado.

6. Dentro de una funcion, `int arr[]`, `int arr[10]` y `int *arr` son
   **equivalentes** -- las tres declaran un puntero. El `[10]` se ignora.

7. Para evitar que una funcion modifique datos a traves de un puntero, se usa
   `const`: `void foo(const int *arr)` impide escribir en `arr[i]`.
