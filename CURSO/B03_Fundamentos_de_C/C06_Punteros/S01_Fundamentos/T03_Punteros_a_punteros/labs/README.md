# Lab -- Punteros a punteros

## Objetivo

Practicar la doble indireccion con `int **`, entender como se usa para
modificar punteros del caller desde una funcion, explorar `char **argv` como
array de strings, y construir un jagged array con `malloc`. Al finalizar,
sabras leer y escribir codigo que usa punteros a punteros con confianza.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `double_indirection.c` | Variables `int *p` e `int **pp`, direcciones y sizeof |
| `alloc_wrong.c` | Comparacion de alocar con `int *` (falla) vs `int **` (correcto) |
| `argv_explore.c` | Recorrer `argv` por indice y por puntero, acceso a caracteres |
| `jagged_array.c` | Array 2D dinamico con filas de diferente tamano |

---

## Parte 1 -- Doble indireccion

**Objetivo**: Entender la relacion entre `int *p` e `int **pp`, verificar que
`**pp` llega al mismo valor que `*p` y `x`, y observar las direcciones en
memoria.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat double_indirection.c
```

Observa:

- `p` almacena la direccion de `x`
- `pp` almacena la direccion de `p`
- Se imprimen valores y direcciones para verificar la cadena de indireccion

### Paso 1.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic double_indirection.c -o double_indirection
```

Antes de ejecutar, predice:

- `*p`, `**pp` y `x` mostraran el mismo valor?
- `p`, `*pp` y `&x` mostraran la misma direccion?
- `pp` y `&p` mostraran la misma direccion?
- Que tamano tendra `sizeof(pp)` comparado con `sizeof(p)`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.3 -- Verificar la prediccion

```bash
./double_indirection
```

Salida esperada:

```
Valor de x:    42
Valor de *p:   42
Valor de **pp: 42

--- Direcciones ---
Direccion de x  (&x):  <addr_x>
Valor de p      (p):   <addr_x>
Valor de *pp    (*pp): <addr_x>

Direccion de p  (&p):  <addr_p>
Valor de pp     (pp):  <addr_p>

--- Modificar x a traves de pp ---
x despues de **pp = 100: 100

--- sizeof ---
sizeof(x)  = 4 (int)
sizeof(p)  = 8 (int *)
sizeof(pp) = 8 (int **)
```

Puntos clave:

- Las tres expresiones `x`, `*p` y `**pp` dan 42 -- llegan al mismo dato
  por caminos distintos
- `p` y `*pp` son la misma direccion (la de `x`), porque `*pp` sigue el
  puntero `pp` para llegar a `p`, cuyo valor es `&x`
- `pp` y `&p` son la misma direccion (la de `p`)
- Todos los punteros miden 8 bytes en un sistema de 64 bits,
  independientemente del nivel de indireccion
- `**pp = 100` modifica `x` a traves de dos niveles de indireccion

### Limpieza de la parte 1

```bash
rm -f double_indirection
```

---

## Parte 2 -- Modificar un puntero desde funcion

**Objetivo**: Entender por que se necesita `int **` para que una funcion
modifique un puntero del caller. Comparar la version incorrecta (que recibe
`int *`) con la correcta (que recibe `int **`).

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat alloc_wrong.c
```

Observa las dos funciones:

- `alloc_wrong(int *p)` -- recibe una **copia** del puntero
- `alloc_correct(int **pp)` -- recibe la **direccion** del puntero

### Paso 2.2 -- Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic alloc_wrong.c -o alloc_wrong
```

Antes de ejecutar, predice:

- Despues de `alloc_wrong(data)`, `data` seguira siendo NULL?
- Despues de `alloc_correct(&data)`, `data` tendra una direccion valida?
- Que pasa con la memoria que aloca `alloc_wrong`? Se puede liberar?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.3 -- Ejecutar y verificar

```bash
./alloc_wrong
```

Salida esperada:

```
--- alloc_wrong ---
data antes: (nil)
data despues: (nil)
data sigue siendo NULL: si

--- alloc_correct ---
data antes: (nil)
data despues: <addr>
*data = 42
```

Analisis:

- `alloc_wrong` recibe una copia de `data` (que es NULL). Modifica la copia
  local, pero el `data` original en `main` no cambia. Ademas, la memoria
  alocada con `malloc` se pierde -- es un memory leak
- `alloc_correct` recibe `&data` (la direccion del puntero `data`). Con
  `*pp = malloc(...)` modifica el puntero original. Despues de la llamada,
  `data` apunta a memoria valida con el valor 42

Regla: para que una funcion modifique un `int` del caller, necesita un `int *`.
Para que modifique un `int *` del caller, necesita un `int **`. El patron es
siempre "un nivel mas de indireccion".

### Limpieza de la parte 2

```bash
rm -f alloc_wrong
```

---

## Parte 3 -- argv

**Objetivo**: Explorar `char **argv` como ejemplo real de puntero a puntero.
Recorrer los argumentos de la linea de comandos por indice y por puntero,
verificar el terminador NULL, y acceder caracter por caracter.

### Paso 3.1 -- Examinar el codigo fuente

```bash
cat argv_explore.c
```

Observa:

- `argv` es un `char **` -- un puntero a un array de punteros a strings
- Se recorre con indice (`argv[i]`) y con puntero (`char **ptr`)
- `argv[argc]` siempre es NULL (garantizado por el estandar)
- `argv[0][i]` accede a caracteres individuales del nombre del programa

### Paso 3.2 -- Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic argv_explore.c -o argv_explore
```

### Paso 3.3 -- Ejecutar sin argumentos

```bash
./argv_explore
```

Antes de ver la salida, predice:

- Cual sera el valor de `argc`?
- Que contendra `argv[0]`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.4 -- Verificar

Salida esperada:

```
argc = 1

--- Recorrido con indice ---
argv[0] = "./argv_explore"  (en <addr>)

argv[argc] = (nil) (debe ser NULL)

--- Recorrido con char **ptr ---
*ptr = "./argv_explore"

--- Caracteres de argv[0] ---
argv[0][0] = '.'
argv[0][1] = '/'
argv[0][2] = 'a'
...
```

Incluso sin argumentos adicionales, `argc` es 1 porque `argv[0]` siempre
contiene el nombre del programa.

### Paso 3.5 -- Ejecutar con argumentos

```bash
./argv_explore hello world 42
```

Salida esperada:

```
argc = 4

--- Recorrido con indice ---
argv[0] = "./argv_explore"  (en <addr>)
argv[1] = "hello"  (en <addr>)
argv[2] = "world"  (en <addr>)
argv[3] = "42"  (en <addr>)

argv[argc] = (nil) (debe ser NULL)

--- Recorrido con char **ptr ---
*ptr = "./argv_explore"
*ptr = "hello"
*ptr = "world"
*ptr = "42"

--- Caracteres de argv[0] ---
argv[0][0] = '.'
argv[0][1] = '/'
...
```

Observa:

- `argc` cuenta el nombre del programa mas los argumentos
- Las direcciones de cada string son diferentes -- cada argumento esta en su
  propia zona de memoria
- El recorrido con `char **ptr` funciona porque `argv` es un array de punteros
  terminado en NULL, igual que un array de strings
- `argv[0][i]` demuestra que `argv` es realmente un "puntero a puntero a
  char": `argv` apunta a un array de `char *`, y cada `char *` apunta a un
  array de `char`

### Paso 3.6 -- Ejecutar con argumentos que contienen espacios

```bash
./argv_explore "hello world" 42
```

Predice: cuantos argumentos detectara `argc`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.7 -- Verificar

Salida esperada:

```
argc = 3

--- Recorrido con indice ---
argv[0] = "./argv_explore"  (en <addr>)
argv[1] = "hello world"  (en <addr>)
argv[2] = "42"  (en <addr>)
...
```

Las comillas del shell agrupan "hello world" como un solo argumento. El
programa recibe 3 argumentos, no 4.

### Limpieza de la parte 3

```bash
rm -f argv_explore
```

---

## Parte 4 -- Array dinamico 2D (jagged array)

**Objetivo**: Construir un array 2D donde cada fila tiene un tamano diferente
usando `int **` y `malloc`. Entender la estructura de memoria y la liberacion
correcta.

### Paso 4.1 -- Examinar el codigo fuente

```bash
cat jagged_array.c
```

Observa la estructura en dos niveles:

- `int **matrix` -- un puntero a un array de punteros
- `matrix[i]` -- cada elemento es un `int *` que apunta a una fila
- Cada fila se aloca con `malloc(row_sizes[i] * sizeof(int))`
- Las filas tienen tamanos 2, 5, 3, 1 -- por eso se llama "jagged array"

### Paso 4.2 -- Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic jagged_array.c -o jagged_array
```

Antes de ejecutar, predice:

- Cuantos `malloc` se ejecutaran en total?
- Las direcciones de las filas seran contiguas o estaran dispersas?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.3 -- Ejecutar y verificar

```bash
./jagged_array
```

Salida esperada:

```
--- Jagged array ---
fila 0 (2 elementos):  10  11
fila 1 (5 elementos):  20  21  22  23  24
fila 2 (3 elementos):  30  31  32
fila 3 (1 elementos):  40

--- Direcciones ---
matrix     = <addr> (el int**)
matrix[0]  = <addr> (puntero a fila 0)
matrix[1]  = <addr> (puntero a fila 1)
matrix[2]  = <addr> (puntero a fila 2)
matrix[3]  = <addr> (puntero a fila 3)

Memoria liberada correctamente.
```

Analisis:

- Se ejecutan 5 `malloc` en total: 1 para el array de punteros (`int **`) y
  4 para las filas individuales
- Las direcciones de las filas no tienen por que ser contiguas -- cada `malloc`
  puede devolver una direccion diferente. Esto contrasta con un array 2D
  estatico (`int m[4][5]`), donde todo el bloque es contiguo
- La notacion `matrix[i][j]` funciona igual que con un array estatico, pero
  la mecanica es distinta: `matrix[i]` sigue un puntero para encontrar la
  fila, y luego `[j]` accede al elemento dentro de esa fila

### Paso 4.4 -- Modelo mental de la memoria

Dibuja mentalmente la estructura:

```
matrix (int **)
┌──────────┐
│ ptr fila0│ --> [10][11]
├──────────┤
│ ptr fila1│ --> [20][21][22][23][24]
├──────────┤
│ ptr fila2│ --> [30][31][32]
├──────────┤
│ ptr fila3│ --> [40]
└──────────┘
```

Cada flecha es un puntero que sigue `matrix[i]`. Sin `int **`, no se podria
tener filas de tamano diferente con `malloc`.

### Paso 4.5 -- Orden de liberacion

Observa en el codigo que la liberacion se hace en orden inverso a la
alocacion:

1. Primero se liberan las filas (`free(matrix[i])`)
2. Despues se libera el array de punteros (`free(matrix)`)

Predice: que pasaria si se hace `free(matrix)` primero y luego
`free(matrix[i])`?

Respuesta: seria **undefined behavior**. Despues de `free(matrix)`, los
punteros `matrix[0]`, `matrix[1]`, etc. ya no son accesibles -- se perderian
las direcciones de las filas y no se podrian liberar (memory leak). Siempre
liberar desde lo mas interno hacia lo mas externo.

### Limpieza de la parte 4

```bash
rm -f jagged_array
```

---

## Limpieza final

```bash
rm -f double_indirection alloc_wrong argv_explore jagged_array
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  alloc_wrong.c  argv_explore.c  double_indirection.c  jagged_array.c
```

---

## Conceptos reforzados

1. `int **pp` almacena la direccion de un `int *`. Desreferenciar una vez
   (`*pp`) da el puntero intermedio; desreferenciar dos veces (`**pp`) da el
   dato final. Todos los punteros miden 8 bytes en 64 bits,
   independientemente del nivel de indireccion.

2. Para que una funcion modifique un puntero del caller, debe recibir un
   puntero a ese puntero (`int **`). Pasar solo `int *` crea una copia local
   -- el caller no ve el cambio, y la memoria alocada se pierde (memory leak).

3. `char **argv` es un array de punteros a strings. `argv[i]` accede al
   i-esimo argumento, `argv[i][j]` accede al j-esimo caracter de ese
   argumento. `argv[argc]` siempre es NULL.

4. Un jagged array (`int **` con `malloc`) permite filas de diferente tamano.
   Requiere una alocacion para el array de punteros y una alocacion por fila.
   La liberacion debe hacerse en orden inverso: primero las filas, despues el
   array de punteros.

5. La notacion `matrix[i][j]` funciona tanto con arrays estaticos 2D como con
   `int **` dinamico, pero la mecanica es diferente: con `int **`, cada
   `matrix[i]` sigue un puntero para encontrar la fila en memoria, mientras
   que con un array estatico el compilador calcula un offset dentro de un
   bloque contiguo.
