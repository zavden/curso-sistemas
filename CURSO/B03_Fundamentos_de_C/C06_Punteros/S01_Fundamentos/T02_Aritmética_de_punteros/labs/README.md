# Lab -- Aritmetica de punteros

## Objetivo

Comprobar experimentalmente que la aritmetica de punteros opera en unidades de
elementos (no bytes), que el "paso" depende del tipo apuntado, y que el
compilador rechaza operaciones que no tienen sentido con direcciones de memoria.
Al finalizar, sabras predecir cuantos bytes avanza `p + n` para cualquier tipo,
interpretar resultados de `ptrdiff_t`, y recorrer arrays con el patron
begin/end.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `stride.c` | Demuestra el stride de int*, char*, double* |
| `ptrdiff.c` | Demuestra resta de punteros y ptrdiff_t |
| `compare.c` | Demuestra comparacion y recorrido con centinela |
| `illegal.c` | Operaciones ilegales comentadas para provocar errores |

---

## Parte 1 -- Stride de punteros: p+n avanza sizeof(*p)*n bytes

**Objetivo**: Verificar que sumar un entero a un puntero avanza por elementos,
no por bytes, y que el tamano del paso depende del tipo apuntado.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat stride.c
```

Observa que el programa crea tres arrays de tipos distintos (`int`, `char`,
`double`) y para cada uno imprime la direccion del puntero base, la de `p + 1`
y la de `p + 2`, junto con la diferencia en bytes.

### Paso 1.2 -- Predecir antes de compilar

Antes de compilar, responde mentalmente:

- Si `sizeof(int) = 4`, cuantos bytes separan `pi` de `pi + 1`?
- Si `sizeof(char) = 1`, cuantos bytes separan `pc` de `pc + 1`?
- Si `sizeof(double) = 8`, cuantos bytes separan `pd` de `pd + 1`?
- Las direcciones hexadecimales de `pi` y `pi + 1`, en que digito difieren?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic stride.c -o stride
./stride
```

Salida esperada (las direcciones varian, los deltas no):

```
=== sizeof each type ===
sizeof(int)    = 4
sizeof(char)   = 1
sizeof(double) = 8

=== int* stride ===
pi     = 0x<addr>  -> *pi     = 10
pi + 1 = 0x<addr>  -> *(pi+1) = 20
pi + 2 = 0x<addr>  -> *(pi+2) = 30
Difference in bytes: 4

=== char* stride ===
pc     = 0x<addr>  -> *pc     = A
pc + 1 = 0x<addr>  -> *(pc+1) = B
pc + 2 = 0x<addr>  -> *(pc+2) = C
Difference in bytes: 1

=== double* stride ===
pd     = 0x<addr>  -> *pd     = 1.1
pd + 1 = 0x<addr>  -> *(pd+1) = 2.2
pd + 2 = 0x<addr>  -> *(pd+2) = 3.3
Difference in bytes: 8
```

### Paso 1.4 -- Analisis

Verifica tus predicciones:

- Para `int*`: la diferencia es **4 bytes** (`sizeof(int) = 4`). Si `pi` termina
  en `...70`, entonces `pi + 1` termina en `...74` y `pi + 2` en `...78`.
- Para `char*`: la diferencia es **1 byte**. Las direcciones avanzan de uno en
  uno.
- Para `double*`: la diferencia es **8 bytes**. Las direcciones saltan de 8 en 8.

La formula clave es: `p + n` equivale a avanzar `n * sizeof(*p)` bytes en la
direccion real. El compilador hace esta multiplicacion automaticamente -- el
programador piensa en elementos, no en bytes.

### Limpieza de la parte 1

```bash
rm -f stride
```

---

## Parte 2 -- Resta de punteros: ptrdiff_t y distancia en elementos

**Objetivo**: Verificar que restar dos punteros al mismo array produce el
numero de elementos entre ellos (no bytes), y que el resultado es un
`ptrdiff_t` con signo.

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat ptrdiff.c
```

Observa que el programa apunta `p` a `arr[1]` y `q` a `arr[6]`, luego calcula
`q - p` y `p - q`. Tambien muestra la diferencia en bytes para verificar la
formula.

### Paso 2.2 -- Predecir antes de compilar

Antes de compilar, responde mentalmente:

- `q` apunta a `arr[6]` y `p` a `arr[1]`. Cuanto vale `q - p`?
- Y `p - q`? (recuerda: `ptrdiff_t` tiene signo)
- Si la diferencia en elementos es 5, cuantos bytes los separan?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ptrdiff.c -o ptrdiff
./ptrdiff
```

Salida esperada (las direcciones varian):

```
=== Pointer subtraction ===
p points to arr[1] = 20  (address 0x<addr>)
q points to arr[6] = 70  (address 0x<addr>)

q - p = 5 elements
p - q = -5 elements

Address difference in bytes: 20
Elements = bytes / sizeof(int) = 20 / 4 = 5

=== Walking between p and q ===
p[0] = 20
p[1] = 30
p[2] = 40
p[3] = 50
p[4] = 60
```

### Paso 2.4 -- Analisis

Verifica tus predicciones:

- `q - p = 5` elementos (de `arr[1]` a `arr[6]`, hay 5 posiciones).
- `p - q = -5` elementos (la resta tiene signo, `ptrdiff_t` es signed).
- La diferencia en bytes es `5 * sizeof(int) = 5 * 4 = 20`.

La formula interna es: `(q - p) = (direccion_q - direccion_p) / sizeof(int)`.
El compilador divide automaticamente por el tamano del tipo. Esto es lo inverso
de la suma: si sumar un entero multiplica, restar dos punteros divide.

Observa tambien que `p[i]` funciona correctamente desde una posicion intermedia
del array -- `p[0]` es `arr[1]`, `p[1]` es `arr[2]`, etc. Esto es porque
`p[i]` es equivalente a `*(p + i)`.

### Limpieza de la parte 2

```bash
rm -f ptrdiff
```

---

## Parte 3 -- Comparacion de punteros y recorrido centinela

**Objetivo**: Verificar que los operadores de comparacion funcionan con punteros
al mismo array, y practicar el patron begin/end para recorrer arrays.

### Paso 3.1 -- Examinar el codigo fuente

```bash
cat compare.c
```

Observa que el programa compara `p` (apuntando a `arr[1]`) con `q` (apuntando a
`arr[3]`) usando todos los operadores de comparacion. Luego recorre el array
usando tres estilos: forward con `<`, reverse con `>=`, y equality con `!=`.

### Paso 3.2 -- Predecir antes de compilar

Antes de compilar, responde mentalmente:

- `p` apunta a `arr[1]` y `q` a `arr[3]`. Cual es "menor"?
- En el recorrido begin/end, `end` apunta a `arr[5]` (one-past-the-end). Es
  seguro desreferenciar `end`?
- El recorrido en reversa empieza en `end - 1`. Cual es el primer valor que
  imprime?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic compare.c -o compare
./compare
```

Salida esperada (las direcciones varian):

```
=== Pointer comparison operators ===
p -> arr[1] = 20  (address 0x<addr>)
q -> arr[3] = 40  (address 0x<addr>)

p < q  ? true
p > q  ? false
p == q ? false
p != q ? true
p <= q ? true
p >= q ? false

=== Sentinel traversal (begin/end) ===
begin = 0x<addr>
end   = 0x<addr>  (one-past-the-end, NOT dereferenceable)

Forward traversal:  10 20 30 40 50
Reverse traversal: 50 40 30 20 10

Equality sentinel:  10 20 30 40 50
```

### Paso 3.4 -- Analisis

Verifica tus predicciones:

- `p < q` es `true` porque `arr[1]` esta en una direccion menor que `arr[3]`.
  En un array, las posiciones mas bajas tienen direcciones mas bajas.
- `end` apunta una posicion despues del ultimo elemento. Es valido **calcular**
  esta direccion y **compararla**, pero **no desreferenciarla** (`*end` seria
  comportamiento indefinido).
- El recorrido en reversa empieza en `end - 1`, que es `arr[4] = 50`.

El patron begin/end es fundamental en C: permite recorrer un array sin necesitar
su tamano como parametro separado. Basta con pasar dos punteros. Este mismo
patron es la base de los iteradores en C++.

### Limpieza de la parte 3

```bash
rm -f compare
```

---

## Parte 4 -- Operaciones ilegales

**Objetivo**: Verificar que el compilador rechaza operaciones sin sentido
matematico sobre punteros, y aprender a interpretar los mensajes de error.

### Paso 4.1 -- Examinar el codigo fuente

```bash
cat illegal.c
```

Observa que el archivo tiene varias operaciones ilegales comentadas. La parte
que compila muestra las operaciones validas como referencia. Cada bloque
comentado tiene una operacion ilegal distinta.

### Paso 4.2 -- Compilar la version limpia

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic illegal.c -o illegal
./illegal
```

Salida esperada:

```
Valid: r1=0x<addr>  r2=0x<addr>  d=3  cmp=1

Uncomment one illegal block at a time and recompile.
Observe the error message from the compiler.
```

Las operaciones validas son:
- `p + 2` (puntero + entero -> puntero)
- `q - 1` (puntero - entero -> puntero)
- `q - p` (puntero - puntero -> ptrdiff_t)
- `p < q` (comparacion -> int)

### Paso 4.3 -- Provocar error: sumar dos punteros

Abre `illegal.c` con tu editor y descomenta la linea:

```c
int *bad1 = p + q;
```

Antes de recompilar, predice: que tipo de error dara el compilador? Sera un
warning o un error fatal?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.4 -- Compilar y observar el error

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic illegal.c -o illegal
```

Salida esperada (error):

```
illegal.c: error: invalid operands to binary + (have 'int *' and 'int *')
```

Es un **error fatal** (no un warning). No se genera ejecutable. Sumar dos
punteros no tiene sentido: si `p = 0x1000` y `q = 0x2000`, que significaria
`0x3000`? No es una direccion valida en relacion al array.

Vuelve a comentar la linea antes de continuar.

### Paso 4.5 -- Provocar error: multiplicar un puntero

Descomenta la linea:

```c
int *bad2 = p * 2;
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic illegal.c -o illegal
```

Salida esperada (error):

```
illegal.c: error: invalid operands to binary * (have 'int *' and 'int')
```

Mismo patron: multiplicar una direccion de memoria no tiene significado
geometrico ni logico. Solo `+` y `-` con enteros tienen sentido (avanzar y
retroceder en el array).

Vuelve a comentar la linea antes de continuar.

### Paso 4.6 -- Restaurar el archivo

Asegurate de que todas las lineas ilegales esten comentadas nuevamente.
Verifica que el archivo compila limpiamente:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic illegal.c -o illegal
echo $?
```

Si la salida es `0`, el archivo esta correcto.

### Paso 4.7 -- Resumen de operaciones

| Operacion | Resultado | Valida? |
|-----------|-----------|---------|
| `puntero + entero` | puntero | Si |
| `puntero - entero` | puntero | Si |
| `puntero - puntero` | ptrdiff_t | Si (mismo array) |
| `puntero < puntero` | int | Si (mismo array) |
| `puntero + puntero` | -- | No (error de compilacion) |
| `puntero * entero` | -- | No (error de compilacion) |
| `puntero / entero` | -- | No (error de compilacion) |
| `puntero % entero` | -- | No (error de compilacion) |
| `puntero & entero` | -- | No (error de compilacion) |

### Limpieza de la parte 4

```bash
rm -f illegal
```

---

## Limpieza final

```bash
rm -f stride ptrdiff compare illegal
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  compare.c  illegal.c  ptrdiff.c  stride.c
```

---

## Conceptos reforzados

1. `p + n` avanza `n * sizeof(*p)` bytes en la direccion real. Para `int*`
   avanza 4 bytes, para `char*` avanza 1 byte, para `double*` avanza 8 bytes.
   El compilador multiplica automaticamente por el tamano del tipo.

2. La resta de dos punteros al mismo array produce un `ptrdiff_t` (con signo)
   que indica la distancia en **elementos**, no en bytes. Internamente el
   compilador divide la diferencia de direcciones por `sizeof(*p)`.

3. Los operadores de comparacion (`<`, `>`, `==`, `!=`, `<=`, `>=`) funcionan
   con punteros al mismo array. Un puntero a un indice menor es "menor" que
   uno a un indice mayor, porque las direcciones de memoria crecen con el indice.

4. El patron begin/end permite recorrer un array con dos punteros sin necesitar
   el tamano como parametro separado. `end` apunta a one-past-the-end: es
   valido calcularlo y compararlo, pero **no desreferenciarlo**.

5. Solo cuatro operaciones son validas con punteros: sumar un entero, restar un
   entero, restar otro puntero (al mismo array), y comparar con otro puntero (al
   mismo array) o con NULL. Cualquier otra operacion aritmetica (`+`, `*`, `/`,
   `%`, `&` entre punteros o con tipos invalidos) es un error de compilacion.
