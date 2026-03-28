# Lab — Puntero void*

## Objetivo

Experimentar con el puntero generico `void*`: asignar direcciones de cualquier
tipo, entender por que no se puede desreferenciar directamente, escribir
funciones genericas con `memcpy`, y usar funciones de la biblioteca estandar
que dependen de `void*`. Al finalizar, sabras cuando y como usar `void*` de
forma segura.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `void_basic.c` | Asignacion de distintos tipos a void* y conversion implicita |
| `void_deref.c` | Desreferencia con cast, cast incorrecto, sizeof |
| `swap_generic.c` | Funcion swap generica con void* y memcpy |
| `void_stdlib.c` | malloc, qsort con comparadores, bsearch |

---

## Parte 1 — void* generico

**Objetivo**: Verificar que `void*` puede almacenar la direccion de cualquier
tipo y que la conversion es implicita en ambas direcciones.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat void_basic.c
```

Observa:

- Se declara un `void *vp` y se le asignan direcciones de `int`, `double` y
  `char` sin ningun cast
- Se hace un roundtrip: `int* -> void* -> int*`, todo implicito

Antes de compilar y ejecutar, predice:

- Las tres asignaciones a `vp` compilaran sin warnings?
- Despues del roundtrip `int* -> void* -> int*`, el puntero recuperado
  apuntara a la misma direccion que el original?

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic void_basic.c -o void_basic
./void_basic
```

Salida esperada:

```
vp points to int:    address = 0x<addr>
vp points to double: address = 0x<addr>
vp points to char:   address = 0x<addr>

Roundtrip test:
  original ip  = 0x<addr>, *ip  = 42
  recovered ip2 = 0x<addr>, *ip2 = 42
  same address? yes
```

Las direcciones exactas varian, pero lo importante es:

- Las tres asignaciones funcionaron sin cast y sin warnings
- El roundtrip preservo la direccion y el valor
- `void*` no modifica ni pierde informacion — solo almacena la direccion

### Paso 1.3 — Observar el tamano de void*

```bash
cat >> void_basic.c << 'EOF'

/* appended for size test */
EOF
```

No, mejor ejecuta directamente:

```bash
printf '#include <stdio.h>\nint main(void) {\n    printf("sizeof(void*)  = %%zu\\n", sizeof(void *));\n    printf("sizeof(int*)   = %%zu\\n", sizeof(int *));\n    printf("sizeof(double*)= %%zu\\n", sizeof(double *));\n    return 0;\n}\n' > size_test.c
gcc -std=c11 -Wall -Wextra -Wpedantic size_test.c -o size_test
./size_test
```

Salida esperada:

```
sizeof(void*)  = 8
sizeof(int*)   = 8
sizeof(double*)= 8
```

Todos los punteros tienen el mismo tamano (8 bytes en x86-64) porque un
puntero es solo una direccion de memoria. El tipo del puntero no afecta su
tamano — afecta como se interpreta el dato al que apunta.

### Limpieza de la parte 1

```bash
rm -f void_basic size_test.c size_test
```

---

## Parte 2 — No se puede desreferenciar void*

**Objetivo**: Entender por que `void*` no se puede desreferenciar, practicar
el cast correcto, y ver que ocurre con un cast incorrecto.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat void_deref.c
```

Observa:

- Para acceder al valor, primero se convierte `void*` a un tipo concreto
  (`int*`, `double*`)
- Se muestra un cast incorrecto: interpretar un `int` como `double`
- Se imprimen los tamanos de los tipos

Antes de compilar, predice:

- Que valor imprimira `x as double` cuando `x` es un `int` de 4 bytes pero
  se lee como `double` de 8 bytes?
- Cuantos bytes leera `*(double *)vp` si `vp` apunta a un `int`?

### Paso 2.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic void_deref.c -o void_deref
./void_deref
```

Salida esperada:

```
Via int*:    42
Via inline cast: 42
Via double*: 3.140000
Via inline cast: 3.140000

Dangerous wrong cast:
  x as int:    42
  x as double: <valor basura>  (garbage — UB)

sizeof through typed pointers:
  sizeof(int):    4 bytes
  sizeof(double): 8 bytes
  sizeof(char):   1 bytes
```

El cast incorrecto produce basura porque `*(double *)vp` lee 8 bytes a partir
de una variable `int` que solo ocupa 4. Los 4 bytes extra son basura del
stack. Ademas, esto es comportamiento indefinido (UB) por violacion de
strict aliasing.

### Paso 2.3 — Intentar desreferenciar void* directamente

Crea un programa que intente desreferenciar `void*`:

```bash
cat > deref_error.c << 'EOF'
#include <stdio.h>

int main(void) {
    int x = 42;
    void *vp = &x;
    printf("%d\n", *vp);    /* attempt to dereference void* */
    return 0;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic deref_error.c -o deref_error
```

Salida esperada (error de compilacion):

```
deref_error.c: ... error: dereferencing 'void *' pointer
```

El compilador rechaza `*vp` porque `void*` no tiene tipo asociado — no sabe
cuantos bytes leer ni como interpretarlos.

### Paso 2.4 — Intentar aritmetica con void*

```bash
cat > arith_error.c << 'EOF'
#include <stdio.h>

int main(void) {
    int arr[] = {10, 20, 30};
    void *vp = arr;
    void *vp2 = vp + 1;   /* arithmetic on void* */
    printf("%p %p\n", vp, vp2);
    return 0;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic arith_error.c -o arith_error
```

Salida esperada (warning o error):

```
arith_error.c: ... warning: pointer of type 'void *' used in arithmetic
```

Con `-Wpedantic`, GCC advierte que la aritmetica con `void*` no es estandar.
GCC la acepta como extension (sumando 1 byte), pero no es portable ni
correcto. Para avanzar en un array, siempre convertir a un tipo concreto
primero.

### Limpieza de la parte 2

```bash
rm -f void_deref deref_error.c deref_error arith_error.c arith_error
```

---

## Parte 3 — Funcion generica con void*

**Objetivo**: Escribir y probar una funcion `swap_generic` que intercambia
dos valores de cualquier tipo usando `void*` y `memcpy`.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat swap_generic.c
```

Observa la funcion `swap_generic`:

- Recibe `void *a`, `void *b` y `size_t size`
- Usa un VLA `unsigned char tmp[size]` como buffer temporal
- `memcpy` copia byte a byte — funciona con cualquier tipo

Antes de compilar, predice:

- La misma funcion `swap_generic` funcionara para ints, doubles y structs
  sin modificacion?
- Por que `swap_generic` necesita el parametro `size`? Que pasaria si no
  lo tuviera?

### Paso 3.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic swap_generic.c -o swap_generic
./swap_generic
```

Salida esperada:

```
Before swap: x=10, y=20
After swap:  x=20, y=10

Before swap: a=1.1, b=2.2
After swap:  a=2.2, b=1.1

Before swap: p1={1,2}, p2={3,4}
After swap:  p1={3,4}, p2={1,2}
```

Una sola funcion intercambio ints (4 bytes), doubles (8 bytes) y structs
(8 bytes). El parametro `size` le dice a `memcpy` cuantos bytes copiar. Sin
`size`, la funcion no sabria cuantos bytes mover — `void*` no tiene esa
informacion.

### Paso 3.3 — Que pasa con un size incorrecto

```bash
cat > swap_wrong_size.c << 'EOF'
#include <stdio.h>
#include <string.h>

void swap_generic(void *a, void *b, size_t size) {
    unsigned char tmp[size];
    memcpy(tmp, a, size);
    memcpy(a, b, size);
    memcpy(b, tmp, size);
}

int main(void) {
    double a = 1.1, b = 2.2;
    printf("Before: a=%.1f, b=%.1f\n", a, b);

    /* BUG: using sizeof(int) instead of sizeof(double) */
    swap_generic(&a, &b, sizeof(int));

    printf("After:  a=%.1f, b=%.1f  (corrupted!)\n", a, b);
    return 0;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic swap_wrong_size.c -o swap_wrong_size
./swap_wrong_size
```

Salida esperada:

```
Before: a=1.1, b=2.2
After:  a=<corrupted>, b=<corrupted>  (corrupted!)
```

Al pasar `sizeof(int)` (4 bytes) para un `double` (8 bytes), `memcpy` solo
copia la mitad de los bytes. El resultado es corrupcion de datos. Este es el
precio de la genericidad con `void*`: el compilador no puede verificar que
el `size` sea correcto. El programador es responsable.

### Limpieza de la parte 3

```bash
rm -f swap_generic swap_wrong_size.c swap_wrong_size
```

---

## Parte 4 — void* en la practica

**Objetivo**: Ver como `malloc`, `qsort` y `bsearch` de la biblioteca
estandar usan `void*` para ser genericas.

### Paso 4.1 — Examinar el codigo fuente

```bash
cat void_stdlib.c
```

Observa:

- `malloc` retorna `void*` — se asigna a `int*` sin cast (conversion
  implicita)
- `qsort` recibe `void *base` y un comparador `int (*)(const void *, const void *)`
- Cada comparador castea `const void*` al tipo correcto antes de comparar
- `bsearch` tiene la misma firma generica

Antes de compilar, predice:

- `qsort` ordena el array in-place o retorna una copia?
- Si se pasa `cmp_by_grade` a `qsort`, en que orden quedaran los
  estudiantes?
- `bsearch` encontrara el valor 7 en el array `{1, 2, 5, 8, 9}`?

### Paso 4.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic void_stdlib.c -o void_stdlib
./void_stdlib
```

Salida esperada:

```
=== malloc returns void* ===
Dynamic array:  [5, 2, 8, 1, 9]

=== qsort with ints ===
Before:  [5, 2, 8, 1, 9]
After:   [1, 2, 5, 8, 9]

=== qsort with doubles ===
Before:  [3.1, 1.4, 2.7, 0.5, 4.2]
After:   [0.5, 1.4, 2.7, 3.1, 4.2]

=== qsort with structs (by grade) ===
Before:
  Charlie: 85
  Alice: 92
  Bob: 78
  Diana: 95
After (by grade):
  Bob: 78
  Charlie: 85
  Alice: 92
  Diana: 95

=== qsort with structs (by name) ===
After (by name):
  Alice: 92
  Bob: 78
  Charlie: 85
  Diana: 95

=== bsearch ===
Array:  [1, 2, 5, 8, 9]
Search for 5: found at index 2
Search for 7: not found
```

Puntos clave:

- `malloc` retorno `void*` y se asigno a `int*` sin cast — en C la
  conversion es implicita
- `qsort` ordeno in-place (modifica el array original) usando tres tipos
  distintos (`int`, `double`, `struct Student`) con la misma funcion
- El mismo array de structs se ordeno de dos formas diferentes
  simplemente cambiando el comparador
- `bsearch` encontro el 5 (indice 2) pero no el 7

### Paso 4.3 — El patron del comparador

Examina la firma de un comparador:

```c
int cmp_int(const void *a, const void *b);
```

El patron es siempre el mismo:

1. Recibir `const void*` (la firma que `qsort`/`bsearch` esperan)
2. Castear al tipo real: `int ia = *(const int *)a`
3. Comparar y retornar: negativo si a < b, 0 si iguales, positivo si a > b

Nota que `cmp_by_grade` usa cast implicito (`const struct Student *sa = a`),
mientras que `cmp_int` usa cast explicito (`*(const int *)a`). Ambos son
validos en C — el cast implicito desde `void*` es siempre legal.

### Paso 4.4 — Comparador incorrecto

```bash
cat > cmp_wrong.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>

/* Common mistake: using subtraction for comparison */
int cmp_bad(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
    /* BUG: overflows if values differ by more than INT_MAX */
}

/* Correct: comparison without overflow */
int cmp_good(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}

void print_array(const int *arr, size_t n) {
    printf("[");
    for (size_t i = 0; i < n; i++) {
        printf("%d%s", arr[i], (i < n - 1) ? ", " : "");
    }
    printf("]\n");
}

int main(void) {
    int safe[] = {5, 2, 8, 1, 9};
    qsort(safe, 5, sizeof(int), cmp_bad);
    printf("Small values (cmp_bad works by luck): ");
    print_array(safe, 5);

    /* With extreme values, subtraction overflows */
    int extreme[] = {2000000000, -2000000000, 0};
    printf("\nExtreme values with cmp_bad: ");
    qsort(extreme, 3, sizeof(int), cmp_bad);
    print_array(extreme, 3);

    int extreme2[] = {2000000000, -2000000000, 0};
    printf("Extreme values with cmp_good: ");
    qsort(extreme2, 3, sizeof(int), cmp_good);
    print_array(extreme2, 3);

    return 0;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic cmp_wrong.c -o cmp_wrong
./cmp_wrong
```

Salida esperada:

```
Small values (cmp_bad works by luck): [1, 2, 5, 8, 9]

Extreme values with cmp_bad: [<posiblemente mal ordenados>]
Extreme values with cmp_good: [-2000000000, 0, 2000000000]
```

El patron `a - b` funciona con valores pequenos pero causa overflow con
valores extremos. `2000000000 - (-2000000000) = 4000000000`, que excede
`INT_MAX` y produce un resultado negativo. El patron
`(a > b) - (a < b)` nunca desborda.

### Limpieza de la parte 4

```bash
rm -f void_stdlib cmp_wrong.c cmp_wrong
```

---

## Limpieza final

```bash
rm -f void_basic void_deref swap_generic void_stdlib
rm -f size_test.c size_test deref_error.c deref_error
rm -f arith_error.c arith_error swap_wrong_size.c swap_wrong_size
rm -f cmp_wrong.c cmp_wrong
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  swap_generic.c  void_basic.c  void_deref.c  void_stdlib.c
```

---

## Conceptos reforzados

1. `void*` es un puntero generico que almacena direcciones de cualquier tipo.
   La conversion entre `void*` y cualquier otro puntero es implicita en C
   (no necesita cast).

2. `void*` no se puede desreferenciar porque el compilador no sabe cuantos
   bytes leer ni como interpretarlos. Siempre se debe convertir a un tipo
   concreto antes de acceder al dato.

3. La aritmetica con `void*` (`vp + 1`) no esta definida por el estandar.
   GCC la acepta como extension (sumando 1 byte), pero no es portable.

4. Para escribir funciones genericas con `void*`, se necesita el parametro
   `size_t size` porque `void*` no contiene informacion sobre el tamano del
   tipo. `memcpy` usa ese `size` para copiar byte a byte.

5. Un cast incorrecto (interpretar un `int` como `double`) produce
   comportamiento indefinido por violacion de strict aliasing. El compilador
   no puede detectar este error — el programador es responsable.

6. `malloc` retorna `void*` y en C no se debe castear el resultado
   (`int *p = malloc(...)` es correcto). El cast es innecesario y puede
   ocultar el olvido de `#include <stdlib.h>`.

7. `qsort` y `bsearch` usan `void*` para operar con cualquier tipo. El
   programador provee un comparador que convierte `const void*` al tipo
   real. El patron `(a > b) - (a < b)` es seguro; el patron `a - b`
   causa overflow con valores extremos.
