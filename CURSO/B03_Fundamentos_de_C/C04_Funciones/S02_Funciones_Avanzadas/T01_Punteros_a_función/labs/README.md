# Lab — Punteros a funcion

## Objetivo

Practicar la declaracion y uso de punteros a funcion: asignar funciones a
punteros, pasar funciones como argumento (callbacks), usar qsort con
comparadores custom, y construir dispatch tables. Al finalizar, sabras leer
y escribir la sintaxis de punteros a funcion, usar typedef para simplificarla,
y aplicar estos patrones en programas reales.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `basics.c` | Declaracion, asignacion y llamada a traves de puntero a funcion |
| `callback.c` | apply() y filter_count() con funciones como argumento |
| `sorting.c` | qsort con comparadores para ints y strings |
| `dispatch.c` | Dispatch table: calculadora con array de punteros a funcion |

---

## Parte 1 — Sintaxis basica de punteros a funcion

**Objetivo**: Declarar un puntero a funcion, asignarle funciones, llamar a
traves del puntero, y simplificar con typedef.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat basics.c
```

Observa la estructura del programa:

- Tres funciones con la misma firma: `int (int, int)`
- Un puntero `op` que se reasigna a cada una
- Dos formas equivalentes de asignar (`add` vs `&add`)
- Dos formas equivalentes de llamar (`fp()` vs `(*fp)()`)
- Un typedef `BinaryOp` que simplifica la declaracion

### Paso 1.2 — Predecir la salida

Antes de compilar, responde mentalmente:

- Si `op = add` y luego `op = sub`, que valor imprime `op(10, 3)` la segunda vez?
- `fp(5, 2)` y `(*fp)(5, 2)` dan el mismo resultado?
- Con `typedef int (*BinaryOp)(int, int)`, que tipo tiene `operation`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic basics.c -o basics
./basics
```

Salida esperada:

```
add(10, 3) = 13
sub(10, 3) = 7
mul(10, 3) = 30

With &:    fp(5, 2) = 7
With (*fp): (*fp)(5, 2) = 7

typedef: operation(4, 7) = 28
```

Verifica tus predicciones:

- El puntero `op` se reasigna sin problema: primero apunta a `add`, luego a
  `sub`, luego a `mul`. Cada vez que se llama `op(10, 3)`, ejecuta la funcion
  a la que apunta en ese momento.
- `fp(5, 2)` y `(*fp)(5, 2)` producen el mismo resultado (7). La desreferencia
  explicita `(*fp)` es equivalente a `fp` al llamar.
- Con typedef, `operation` tiene tipo `BinaryOp`, que es un alias para
  `int (*)(int, int)`. Mucho mas legible.

### Paso 1.4 — Error comun: olvidar los parentesis

Considera estas dos declaraciones:

```c
int (*fp)(int, int);     // puntero a funcion
int *fp(int, int);       // funcion que retorna int*
```

Los parentesis alrededor de `(*fp)` son obligatorios. Sin ellos, `fp` se
interpreta como una funcion, no como un puntero. La regla: si vas a declarar
un puntero a funcion, siempre encierra `(*nombre)` entre parentesis.

### Limpieza de la parte 1

```bash
rm -f basics
```

---

## Parte 2 — Callbacks: funciones como argumento

**Objetivo**: Pasar funciones como argumento a otras funciones. Implementar
`apply()` (transforma un array) y `filter_count()` (cuenta con predicado).

### Paso 2.1 — Examinar el codigo fuente

```bash
cat callback.c
```

Observa dos patrones de callback:

1. `apply(int *arr, int n, int (*transform)(int))` — recibe una funcion de
   transformacion y la aplica a cada elemento del array, modificandolo in-place.
2. `filter_count(const int *arr, int n, int (*predicate)(int))` — recibe una
   funcion predicado y cuenta cuantos elementos la satisfacen.

Ambas funciones son genericas: no saben que operacion van a aplicar. El caller
decide que funcion pasar.

### Paso 2.2 — Predecir la salida

Antes de compilar, responde mentalmente:

- Despues de `apply(data, 5, double_it)`, el array sera {1,2,3,4,5} o {2,4,6,8,10}?
- Despues de `apply(data, 5, negate)` (sobre el array ya duplicado), que valores tendra?
- En el array {-3, 0, 7, -1, 4, 12, -8, 2}, cuantos numeros pares hay? Y cuantos positivos?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic callback.c -o callback
./callback
```

Salida esperada:

```
=== apply() with different transforms ===
Original:  {1, 2, 3, 4, 5}
After double_it: {2, 4, 6, 8, 10}
After negate:    {-2, -4, -6, -8, -10}

=== filter_count() with predicates ===
Array: {-3, 0, 7, -1, 4, 12, -8, 2}
Even numbers:     5
Positive numbers: 4
```

Verifica tus predicciones:

- `double_it` multiplica cada elemento por 2: {1,2,3,4,5} se convierte en
  {2,4,6,8,10}.
- `negate` opera sobre el array ya modificado: {2,4,6,8,10} se convierte en
  {-2,-4,-6,-8,-10}. `apply()` modifica in-place, asi que las transformaciones
  se acumulan.
- Pares en {-3, 0, 7, -1, 4, 12, -8, 2}: 0, 4, 12, -8, 2 = 5. Nota que 0 es
  par (0 % 2 == 0).
- Positivos: 7, 4, 12, 2 = 4. Ni 0 ni los negativos cuentan.

### Paso 2.4 — Analizar el patron callback

El patron clave esta en la firma de `apply()`:

```c
void apply(int *arr, int n, int (*transform)(int))
```

El tercer parametro `transform` es un puntero a funcion. `apply()` no sabe
si va a duplicar, negar, o elevar al cuadrado: eso lo decide quien la llama.
Este patron se llama **callback** porque la funcion `apply()` "llama de vuelta"
a la funcion que le pasaron.

En la practica, los callbacks permiten escribir funciones genericas que operan
sobre datos sin conocer la logica especifica de la operacion.

### Limpieza de la parte 2

```bash
rm -f callback
```

---

## Parte 3 — qsort: callback en la biblioteca estandar

**Objetivo**: Usar `qsort()` de la biblioteca estandar con funciones
comparadoras custom para ordenar arrays de enteros y de strings.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat sorting.c
```

Observa la firma de qsort:

```c
void qsort(void *base, size_t nmemb, size_t size,
            int (*compar)(const void *, const void *));
```

Los cuatro parametros:

| Parametro | Que es |
|-----------|--------|
| `base` | Puntero al inicio del array |
| `nmemb` | Numero de elementos |
| `size` | Tamano de cada elemento en bytes |
| `compar` | Funcion comparadora (callback) |

La funcion comparadora debe retornar:

- Negativo si `a < b`
- Cero si `a == b`
- Positivo si `a > b`

Observa las cuatro funciones comparadoras: `compare_ascending`,
`compare_descending`, `compare_strings`, y `compare_by_length`. Cada una
recibe `const void *` y hace un cast al tipo real.

### Paso 3.2 — Predecir la salida

Antes de compilar, responde mentalmente:

- El array {42, 7, 13, 99, 1, 55, 23} ordenado ascendente es...?
- Si cambias la funcion comparadora a `compare_descending`, el mismo array queda...?
- Las palabras {"banana", "apple", "cherry", "date", "elderberry", "fig"} ordenadas
  por longitud: cual va primero, cual ultima?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic sorting.c -o sorting
./sorting
```

Salida esperada:

```
=== qsort with integers ===
Original:   {42, 7, 13, 99, 1, 55, 23}
Ascending:  {1, 7, 13, 23, 42, 55, 99}
Descending: {99, 55, 42, 23, 13, 7, 1}

=== qsort with strings ===
Original:        {"banana", "apple", "cherry", "date", "elderberry", "fig"}
Alphabetical:    {"apple", "banana", "cherry", "date", "elderberry", "fig"}
By length:       {"fig", "date", "apple", "banana", "cherry", "elderberry"}
```

Verifica tus predicciones:

- Ascendente: {1, 7, 13, 23, 42, 55, 99}. La funcion retorna positivo cuando
  `a > b`, asi que los menores van primero.
- Descendente: {99, 55, 42, 23, 13, 7, 1}. `compare_descending` invierte la
  logica: retorna positivo cuando `a < b`.
- Por longitud: "fig" (3), "date" (4), "apple" (5), "banana" (6), "cherry" (6),
  "elderberry" (10). Nota que "apple" y "banana" tienen diferente longitud (5 vs 6),
  pero "banana" y "cherry" tienen la misma (6) — qsort no garantiza el orden
  relativo de elementos iguales (no es estable).

### Paso 3.4 — Analizar el patron de cast en comparadores

El detalle mas importante de qsort es el cast dentro de la funcion comparadora:

```c
int compare_ascending(const void *a, const void *b) {
    int ia = *(const int *)a;   // cast void* -> int*, luego desreferenciar
    int ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}
```

`qsort()` es generica: no sabe el tipo de los elementos. Por eso pasa
`const void *`. El comparador sabe el tipo real y hace el cast. Este es el
precio de la genericidad en C sin templates ni generics.

La expresion `(ia > ib) - (ia < ib)` produce -1, 0, o 1 de forma segura.
Evita el patron `return a - b` que puede causar overflow con enteros grandes.

### Paso 3.5 — Diferencia entre comparar ints y strings

Para strings, el cast es doble:

```c
int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}
```

Porque el array es de `const char *` (punteros a char). Cada elemento del
array es un `char *`. `qsort` pasa la direccion de cada elemento, asi que
`a` es un `char **` (puntero a puntero a char). Se desreferencia una vez para
obtener el `char *` que `strcmp` necesita.

### Limpieza de la parte 3

```bash
rm -f sorting
```

---

## Parte 4 — Dispatch table: array de punteros a funcion

**Objetivo**: Construir una dispatch table (array de punteros a funcion) que
selecciona la operacion por indice, reemplazando un switch.

### Paso 4.1 — Examinar el codigo fuente

```bash
cat dispatch.c
```

Observa la estructura:

- Cuatro funciones con la misma firma: `int (int, int)`
- Un array de punteros a funcion: `Operation ops[]`
- Un array paralelo de nombres: `symbols[]`
- Un for loop que itera y llama `ops[i](a, b)`

### Paso 4.2 — Predecir la salida

Antes de compilar, responde mentalmente:

- Que imprime `ops[0](20, 6)`? Y `ops[3](20, 6)`?
- Si `choice = 2`, que operacion se selecciona?
- Que pasa con `ops[3](20, 0)` (division por cero)?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic dispatch.c -o dispatch
./dispatch
```

Salida esperada:

```
=== Dispatch table: calculator ===
Operands: 20 and 6

  20 + 6 = 26
  20 - 6 = 14
  20 * 6 = 120
  20 / 6 = 3

=== Select by index ===
Index 2 -> 20 * 6 = 120

=== Division by zero ===
  20 / 0 = 0 (protected)
```

Verifica tus predicciones:

- `ops[0]` es `add`, asi que `ops[0](20, 6)` = 26.
- `ops[3]` es `divi`, asi que `ops[3](20, 6)` = 3 (division entera).
- `choice = 2` selecciona `mul` (indice 2 en el array).
- `ops[3](20, 0)` retorna 0 gracias a la proteccion `b != 0 ? a / b : 0`
  dentro de `divi()`.

### Paso 4.4 — Dispatch table vs switch

La dispatch table reemplaza un switch:

```c
// Con switch:
switch (choice) {
    case 0: result = add(a, b); break;
    case 1: result = sub(a, b); break;
    case 2: result = mul(a, b); break;
    case 3: result = divi(a, b); break;
}

// Con dispatch table:
result = ops[choice](a, b);
```

Ventajas de la dispatch table:

- **O(1)**: acceso directo por indice, sin evaluar condiciones.
- **Extensible**: agregar una operacion es agregar un elemento al array.
- **Menos codigo**: una linea en lugar de multiples cases.

Desventaja: requiere que las selecciones sean indices consecutivos (0, 1, 2...).
Para claves arbitrarias (strings, valores no consecutivos), un switch o hash
table es mas apropiado.

### Paso 4.5 — Conexion con enum

En programas reales, los indices de la dispatch table se definen con un enum:

```c
enum Op { OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_COUNT };
Operation ops[OP_COUNT] = { add, sub, mul, divi };
```

`OP_COUNT` al final del enum vale 4 (la cantidad de operaciones) y sirve como
tamano del array. Esto asegura que el array siempre tiene el tamano correcto.

### Limpieza de la parte 4

```bash
rm -f dispatch
```

---

## Limpieza final

```bash
rm -f basics callback sorting dispatch
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  basics.c  callback.c  dispatch.c  sorting.c
```

---

## Conceptos reforzados

1. Un puntero a funcion se declara con `tipo_retorno (*nombre)(parametros)`. Los
   parentesis alrededor de `(*nombre)` son obligatorios; sin ellos se declara una
   funcion que retorna un puntero.

2. `typedef int (*BinaryOp)(int, int)` crea un alias de tipo que simplifica
   declaraciones de punteros a funcion, especialmente en parametros y arrays.

3. El nombre de una funcion ya es su direccion: `fp = add` y `fp = &add` son
   equivalentes. `fp(x)` y `(*fp)(x)` tambien son equivalentes al llamar.

4. Un callback es una funcion que recibe otra funcion como argumento. Permite
   escribir funciones genericas (como `apply()` o `filter_count()`) que no
   conocen la operacion concreta hasta runtime.

5. `qsort()` de stdlib es el ejemplo canonico de callback en C: ordena
   cualquier tipo de array porque el caller provee la funcion comparadora. El
   cast de `const void *` al tipo real es responsabilidad del comparador.

6. La expresion `(a > b) - (a < b)` produce -1, 0, o 1 de forma segura para
   comparadores. Evitar `return a - b` que puede causar overflow con enteros
   grandes.

7. Una dispatch table (array de punteros a funcion) selecciona comportamiento
   por indice en O(1), reemplazando un switch. Es extensible y concisa, pero
   requiere indices consecutivos.
