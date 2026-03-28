# Lab — Declaracion y uso de structs

## Objetivo

Declarar structs, inicializarlos con designated initializers, acceder a
miembros con `.` y `->`, copiarlos, y usarlos como parametros, retorno de
funciones y elementos de arrays. Al finalizar, sabras elegir entre paso por
valor y paso por puntero, y entenderas por que los structs no se pueden
comparar con `==`.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `basics.c` | Declaracion, inicializacion positional y designated, acceso con punto |
| `pointers.c` | Punteros a struct, operador flecha, paso a funciones |
| `copy_compare.c` | Copia con =, comparacion campo a campo, shallow copy |
| `functions_arrays.c` | Struct como retorno, compound literals, arrays de structs |

---

## Parte 1 — Declaracion, inicializacion y acceso con punto

**Objetivo**: Declarar structs, inicializarlos de distintas formas, y acceder a
sus miembros con el operador punto.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat basics.c
```

Observa:

- `struct Point` tiene dos campos `int`
- `struct Person` tiene campos de distintos tipos (char array, int, double)
- Se usan tres formas de inicializacion: positional, designated, y a cero

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic basics.c -o basics
./basics
```

Salida esperada:

```
p1 = (10, 20)
p2 = (30, 40)
p3 = (50, 60)

alice.name   = "Alice"
alice.age    = 30
alice.height = 0.0 (zero -- not set)
alice.email  = "" (empty -- not set)

empty.name   = ""
empty.age    = 0

p1 modified = (100, 200)
```

### Paso 1.3 — Analisis

Verifica estos puntos en la salida:

1. `p3` tiene `x = 50, y = 60` a pesar de que en el codigo `.y` aparece antes
   que `.x`. Los designated initializers permiten cualquier orden.

2. `alice.height` es `0.0` y `alice.email` es `""`. Los campos omitidos en un
   designated initializer se inicializan a cero automaticamente.

3. `empty` tiene todos los campos a cero gracias a `{0}`.

4. Despues de `p1.x = 100; p1.y = 200;`, los valores cambiaron. El operador
   punto permite tanto lectura como escritura.

### Paso 1.4 — Pregunta de prediccion

Antes de continuar, piensa:

- Si declaras `struct Point z;` sin inicializar, que valores tendran `z.x` y
  `z.y`?
- Es lo mismo que declarar `int n;` sin inicializar?

La respuesta se revela en la teoria: variables locales sin inicializar tienen
valores indeterminados. Usar `{0}` o designated initializers evita este
problema.

### Limpieza de la parte 1

```bash
rm -f basics
```

---

## Parte 2 — Punteros a struct y operador flecha

**Objetivo**: Acceder a miembros de un struct a traves de un puntero usando
`->`, y pasar structs a funciones por puntero.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat pointers.c
```

Observa la diferencia entre las funciones:

- `print_rect` recibe `const struct Rectangle *r` -- solo lectura
- `scale_rect` recibe `struct Rectangle *r` -- permite modificar
- `rect_area` recibe `const struct Rectangle *r` -- solo lectura

### Paso 2.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic pointers.c -o pointers
./pointers
```

Salida esperada:

```
width  = 5.0
height = 3.0
width  = 5.0 (using (*p).width)

Rectangle(5.0 x 3.0)
area = 15.0

Scaling by 2x...
Rectangle(10.0 x 6.0)
area = 60.0

box.width  = 10.0 (modified through pointer)
box.height = 6.0 (modified through pointer)
```

### Paso 2.3 — Analisis

Verifica estos puntos:

1. `p->width` y `(*p).width` producen el mismo resultado. El operador `->` es
   azucar sintactico para `(*ptr).member`.

2. Despues de `scale_rect(&box, 2.0)`, los valores de `box` cambiaron (5.0 a
   10.0, 3.0 a 6.0). La funcion recibio un puntero y modifico el struct
   original.

3. `print_rect` y `rect_area` usan `const` -- el compilador impediria
   modificar los campos dentro de esas funciones.

### Paso 2.4 — Pregunta de prediccion

Antes de continuar, piensa:

- Si `scale_rect` recibiera `struct Rectangle r` (por valor, sin puntero),
  el struct original `box` cambiaria despues de la llamada?
- Por que las funciones que solo leen usan `const struct Rectangle *r` en
  lugar de `struct Rectangle r` (por valor)?

La respuesta: por valor se copia todo el struct, asi que el original no cambia.
Se usa `const *` por eficiencia -- pasar un puntero (8 bytes) es mas barato que
copiar un struct completo, y `const` garantiza que no se modifica.

### Limpieza de la parte 2

```bash
rm -f pointers
```

---

## Parte 3 — Copia de structs y comparacion

**Objetivo**: Copiar structs con `=`, entender por que `==` no funciona, y ver
los peligros de la copia superficial (shallow copy) con punteros.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat copy_compare.c
```

Observa:

- `struct Point` solo tiene datos simples (int)
- `struct WithPointer` tiene un campo `const char *label` -- un puntero
- La funcion `point_eq` compara campo a campo

### Paso 3.2 — Pregunta de prediccion

Antes de compilar, piensa:

- Despues de `struct Point b = a;` y luego `b.x = 999`, que valor tendra
  `a.x`?
- `s1.label` y `s2.label` (despues de `s2 = s1`) apuntaran a la misma
  direccion de memoria o a direcciones diferentes?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic copy_compare.c -o copy_compare
./copy_compare
```

Salida esperada:

```
a = (10, 20)
b = (10, 20)  (copied from a)

After b.x = 999:
a = (10, 20)  (unchanged)
b = (999, 20)  (modified)

c and d are equal (field-by-field)
c and d are equal (point_eq function)
c and e are equal (memcmp)

--- Shallow copy ---
s1.label = "alpha" at <address>
s2.label = "alpha" at <address>
Same address? YES -- shallow copy
```

### Paso 3.4 — Analisis

Verifica estos puntos:

1. **Copia con `=`**: `b = a` copia todos los campos. Modificar `b` no afecta
   a `a` -- son copias independientes (para campos que no son punteros).

2. **`==` no funciona**: la linea `if (c == d)` esta comentada en el codigo
   porque no compilaria. C no define el operador `==` para structs.

3. **Alternativas de comparacion**: campo a campo (`c.x == d.x && ...`),
   funcion dedicada (`point_eq`), o `memcmp` (con la advertencia de padding).

4. **Shallow copy**: `s2 = s1` copio el puntero `label`, no el string.
   Ambos apuntan a la misma direccion. Si `s1.label` fuera memoria alocada
   con `malloc` y se hiciera `free(s1.label)`, entonces `s2.label` seria un
   dangling pointer.

### Limpieza de la parte 3

```bash
rm -f copy_compare
```

---

## Parte 4 — Struct como retorno de funcion y arrays de structs

**Objetivo**: Retornar structs por valor desde funciones, usar compound
literals, y trabajar con arrays de structs.

### Paso 4.1 — Examinar el codigo fuente

```bash
cat functions_arrays.c
```

Observa:

- `make_point` retorna un struct por valor usando compound literal
- `midpoint` retorna un struct por valor usando variable nombrada
- `vertices[]` es un array de structs inicializado con designated initializers

### Paso 4.2 — Pregunta de prediccion

Antes de compilar, piensa:

- Cual sera el midpoint entre (10, 20) y (30, 40)?
- Cuantos elementos tendra el array `vertices`? (`sizeof` calcula esto
  automaticamente)

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic functions_arrays.c -o functions_arrays
./functions_arrays
```

Salida esperada:

```
p = (10, 20)
q = (30, 40)
midpoint(p, q) = (20, 30)
literal = (99, -1)

--- Array of structs ---
Triangle has 4 vertices:
  vertices[0] = (0, 0)
  vertices[1] = (10, 0)
  vertices[2] = (10, 10)
  vertices[3] = (0, 10)

After vertices[2] = make_point(15, 15):
  vertices[0] = (0, 0)
  vertices[1] = (10, 0)
  vertices[2] = (15, 15)
  vertices[3] = (0, 10)

Iterate with pointer:
  (0, 0)
  (10, 0)
  (15, 15)
  (0, 10)
```

### Paso 4.4 — Analisis

Verifica estos puntos:

1. **Retorno por valor**: `make_point(10, 20)` retorna un `struct Point`
   completo. El compilador optimiza la copia (NRVO). Para structs pequenos
   (como `Point` con 8 bytes) esto es eficiente.

2. **Compound literal**: `(struct Point){ .x = 99, .y = -1 }` crea un struct
   temporal sin nombrar, directamente como argumento de `print_point`.

3. **Array de structs**: `vertices[]` se declara e inicializa como cualquier
   array. Cada elemento se accede con `vertices[i].x`.

4. **Reemplazo de elemento**: `vertices[2] = make_point(15, 15)` reemplaza un
   elemento completo del array con asignacion `=`.

5. **Iteracion con puntero**: `ptr->x` accede a los campos mientras se
   recorre el array con aritmetica de punteros. El puntero avanza
   `sizeof(struct Point)` bytes por cada incremento.

### Limpieza de la parte 4

```bash
rm -f functions_arrays
```

---

## Limpieza final

```bash
rm -f basics pointers copy_compare functions_arrays
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  basics.c  copy_compare.c  functions_arrays.c  pointers.c
```

---

## Conceptos reforzados

1. Los designated initializers (`.campo = valor`) son preferibles a la
   inicializacion positional porque no dependen del orden de los campos y
   los campos omitidos se inicializan automaticamente a cero.

2. El operador punto (`.`) accede a miembros de un struct directamente. El
   operador flecha (`->`) accede a miembros a traves de un puntero y es
   equivalente a `(*ptr).campo`.

3. Pasar structs por puntero (`struct T *p`) es mas eficiente que por valor
   porque solo se copia la direccion (8 bytes), no el struct completo. Usar
   `const` cuando la funcion no necesita modificar el struct.

4. La asignacion `=` entre structs copia todos los campos, pero es una copia
   superficial (shallow copy): si un campo es un puntero, se copia la
   direccion, no el contenido apuntado.

5. Los structs no se pueden comparar con `==`. Se deben comparar campo a campo,
   con una funcion dedicada, o con `memcmp` (con precaucion por el padding).

6. Las funciones pueden retornar structs por valor. Para structs pequenos es
   eficiente gracias a las optimizaciones del compilador (NRVO). Los compound
   literals permiten crear structs temporales inline.

7. Los arrays de structs se declaran e inicializan como cualquier array. Se
   accede a los campos con `array[i].campo` o con `ptr->campo` al iterar con
   punteros.
