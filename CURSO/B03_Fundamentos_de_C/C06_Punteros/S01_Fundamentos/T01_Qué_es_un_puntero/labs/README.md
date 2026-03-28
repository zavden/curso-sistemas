# Lab — Que es un puntero

## Objetivo

Experimentar con los operadores `&` y `*`, observar direcciones de memoria,
modificar datos a traves de punteros, verificar que todos los punteros tienen
el mismo tamano, y usar punteros como parametros de funciones. Al finalizar,
tendras una comprension solida del modelo mental de punteros en C.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `address.c` | Programa para explorar direcciones y desreferencia |
| `modify.c` | Modificacion de datos y swap con punteros |
| `sizes.c` | sizeof de punteros y demo de NULL |
| `functions.c` | Paso por puntero y output parameters |

---

## Parte 1 — Direccion y desreferencia

**Objetivo**: Entender los operadores `&` (direccion) y `*` (desreferencia),
imprimir direcciones con `%p`, y verificar el modelo mental de punteros.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat address.c
```

Observa:

- `&x` obtiene la direccion de `x`
- `int *p = &x` almacena esa direccion en el puntero `p`
- `*p` accede al valor en la direccion almacenada
- `&p` obtiene la direccion del propio puntero

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic address.c -o address
```

Antes de ejecutar, predice:

- `Value of p` y `Address of x` mostraran la misma direccion o diferente?
- `Value of *p` mostrara una direccion o el valor `42`?
- `Address of p` sera igual o diferente a `Address of x`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.3 — Verificar la prediccion

```bash
./address
```

Salida esperada (las direcciones varian entre ejecuciones):

```
Value of x:       42
Address of x:     0x7fff...
Value of p:       0x7fff...
Value of *p:      42
Address of p:     0x7fff...
```

Verifica:

- `Value of p` y `Address of x` son **la misma direccion** — `p` almacena la
  direccion de `x`
- `Value of *p` es `42` — desreferenciar `p` devuelve el dato en esa direccion
- `Address of p` es **diferente** — `p` es una variable propia con su propia
  direccion en memoria

### Paso 1.4 — Experimentar con la inversion de & y *

Abre el archivo y agrega estas lineas antes del `return 0`:

```bash
cat >> address.c << 'EOF'

// Add before return 0 — demonstrating & and * as inverses:
// printf("*(&x) = %d\n", *(&x));
// printf("&(*p) = %p\n", (void *)&(*p));
EOF
```

Esto solo agrega comentarios al final del archivo. Lo importante es el concepto:
`*(&x)` toma la direccion de `x` y la desreferencia — devuelve `x`. `&(*p)`
desreferencia `p` y toma la direccion — devuelve `p`. Son operaciones inversas.

### Limpieza de la parte 1

```bash
rm -f address
```

---

## Parte 2 — Modificar datos a traves de punteros

**Objetivo**: Comprobar que escribir a traves de `*p` modifica la variable
original, e implementar swap con punteros.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat modify.c
```

Observa dos cosas:

- En `main`: `*p = 99` modifica `x` a traves del puntero
- La funcion `swap` recibe dos punteros y usa una variable temporal

### Paso 2.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic modify.c -o modify
```

Antes de ejecutar, predice:

- Despues de `*p = 99`, que valor tendra `x`?
- Despues de `swap(&x, &y)`, `x` sera 99 o 20? Y `y`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.3 — Verificar la prediccion

```bash
./modify
```

Salida esperada:

```
Before: x = 10
After *p = 99: x = 99

Before swap: x = 99, y = 20
After swap:  x = 20, y = 99
```

Analisis:

- `*p = 99` modifico `x` directamente — el puntero `p` apunta a la misma
  memoria que `x`
- `swap` funciona porque recibe las **direcciones** de `x` e `y`. Si recibiera
  los valores (sin punteros), el intercambio ocurriria en copias locales y `x`
  e `y` no cambiarian

### Paso 2.4 — Comprender por que swap sin punteros no funciona

Piensa en esto: si `swap` se declarara como `void swap(int a, int b)` (sin
punteros), cada parametro seria una **copia** del valor original. Modificar `a`
y `b` dentro de la funcion no afectaria a las variables en `main`. Este es el
problema fundamental que los punteros resuelven: C pasa todo **por valor**, y
pasar la direccion permite simular paso por referencia.

### Limpieza de la parte 2

```bash
rm -f modify
```

---

## Parte 3 — sizeof de punteros y NULL

**Objetivo**: Verificar que todos los punteros tienen el mismo tamano
independientemente del tipo apuntado, y entender NULL como valor seguro de
inicializacion.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat sizes.c
```

Observa que el programa imprime `sizeof` de varios tipos de puntero y luego
compara con `sizeof` de los tipos base. Tambien demuestra el comportamiento de
NULL.

### Paso 3.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic sizes.c -o sizes
```

Antes de ejecutar, predice:

- `sizeof(int *)` y `sizeof(char *)` seran iguales o diferentes?
- `sizeof(int *)` sera igual a `sizeof(int)`?
- Un puntero NULL se evaluara como true o false en un `if`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.3 — Verificar la prediccion

```bash
./sizes
```

Salida esperada:

```
sizeof(int *):    8 bytes
sizeof(double *): 8 bytes
sizeof(char *):   8 bytes
sizeof(void *):   8 bytes

sizeof(int):      4 bytes
sizeof(double):   8 bytes
sizeof(char):     1 bytes

--- NULL demo ---
p is NULL — does not point to anything
!p is true — NULL is falsy
p is not NULL, *p = 42
```

Analisis:

- **Todos los punteros son 8 bytes** en un sistema de 64 bits. El tipo
  apuntado (`int`, `double`, `char`) no afecta el tamano del puntero. Un
  puntero almacena una direccion, y en 64 bits todas las direcciones son de 8
  bytes
- Los tipos base tienen tamanos diferentes (`int` = 4, `double` = 8, `char` =
  1). El tipo del puntero determina **cuantos bytes lee/escribe al
  desreferenciar**, no cuanto ocupa el puntero mismo
- NULL es falsy — `if (p)` es equivalente a `if (p != NULL)`. Siempre
  verificar antes de desreferenciar

### Paso 3.4 — La importancia de inicializar punteros

Piensa en esto: un puntero sin inicializar (`int *p;`) contiene un valor
indeterminado — podria apuntar a cualquier direccion. Desreferenciar un puntero
no inicializado es comportamiento indefinido (UB). Por eso la regla es:

- Si tienes una direccion valida, usa esa: `int *p = &x;`
- Si no, inicializa con NULL: `int *p = NULL;`
- Antes de desreferenciar, verifica: `if (p != NULL)`

### Limpieza de la parte 3

```bash
rm -f sizes
```

---

## Parte 4 — Punteros y funciones

**Objetivo**: Usar punteros como parametros de funciones para modificar
variables del caller y retornar multiples valores a traves de output
parameters.

### Paso 4.1 — Examinar el codigo fuente

```bash
cat functions.c
```

Observa dos funciones:

- `increment(int *p)` — recibe un puntero y modifica el valor apuntado
- `find_extremes(const int *arr, int n, int *min, int *max)` — recibe un array
  como `const` (solo lectura) y retorna dos valores a traves de punteros `min`
  y `max`

Nota el uso de `const int *arr`: esto le dice al compilador que la funcion
**no modificara** los datos del array. Es una buena practica para parametros
de solo lectura.

### Paso 4.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic functions.c -o functions
```

Antes de ejecutar, predice:

- Despues de llamar `increment(&x)` tres veces (empezando en 10), que valor
  tendra `x`?
- Para el array `{5, 2, 8, 1, 9, 3}`, cual sera el min y cual el max?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.3 — Verificar la prediccion

```bash
./functions
```

Salida esperada:

```
Before increment: x = 10
After increment:  x = 11
After 2 more:     x = 13

Array: 5 2 8 1 9 3
Min: 1
Max: 9
```

Analisis:

- `increment` recibe `&x` (la direccion de `x`) y usa `(*p)++` para
  incrementar el valor. Cada llamada modifica `x` directamente
- `find_extremes` es un patron comun en C: la funcion **retorna multiples
  valores** a traves de punteros. En lugar de retornar solo un valor con
  `return`, escribe resultados en `*min` y `*max`. El caller pasa `&min` y
  `&max` para recibir los resultados

### Paso 4.4 — El patron de output parameters

El patron que usa `find_extremes` es fundamental en C:

1. El caller declara variables para recibir resultados: `int min, max;`
2. El caller pasa las direcciones: `find_extremes(data, n, &min, &max);`
3. La funcion escribe los resultados: `*min = valor; *max = valor;`

Este patron se usa en toda la biblioteca estandar de C. Por ejemplo, `scanf`
usa `scanf("%d", &x)` — pasa la direccion de `x` para que `scanf` escriba el
valor leido.

### Limpieza de la parte 4

```bash
rm -f functions
```

---

## Limpieza final

```bash
rm -f address modify sizes functions
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  address.c  functions.c  modify.c  sizes.c
```

---

## Conceptos reforzados

1. Un puntero es una variable que almacena una **direccion de memoria**. Se
   declara con `tipo *nombre` y su valor se imprime con `%p`.

2. `&x` obtiene la direccion de `x`. `*p` accede al valor en la direccion
   almacenada en `p`. Son operaciones **inversas**: `*(&x)` devuelve `x` y
   `&(*p)` devuelve `p`.

3. Escribir a traves de `*p = valor` modifica la variable original, no una
   copia. Esto es lo que permite que funciones como `swap` funcionen: reciben
   **direcciones**, no valores.

4. En un sistema de 64 bits, **todos los punteros ocupan 8 bytes**
   independientemente del tipo apuntado. El tipo del puntero determina cuantos
   bytes lee/escribe al desreferenciar, no el tamano del puntero mismo.

5. Un puntero no inicializado contiene un valor indeterminado.
   **Siempre** inicializar con una direccion valida o con NULL, y verificar
   con `if (p != NULL)` antes de desreferenciar.

6. C pasa todo **por valor**. Para que una funcion modifique una variable
   del caller, se pasa la direccion (`&x`) y la funcion recibe un puntero
   (`int *p`). Este mecanismo tambien permite retornar multiples valores a
   traves de output parameters.
