# Lab -- Inicializacion

## Objetivo

Verificar experimentalmente como C inicializa (o no) las variables segun su
storage class, practicar designated initializers y compound literals, y dominar
el patron `{0}` para zero-initialization. Al finalizar, sabras predecir el
valor inicial de cualquier variable en C sin ejecutar el programa.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `default_values.c` | Demuestra valores por defecto segun storage class |
| `designated_init.c` | Designated initializers en structs y arrays |
| `compound_literals.c` | Compound literals como argumentos y con punteros |
| `partial_init.c` | Inicializacion parcial y zero-initialization con `{0}` |

---

## Parte 1 -- Valores por defecto segun storage class

**Objetivo**: Comprobar que las variables con static lifetime se inicializan a 0,
y que las variables locales contienen basura.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat default_values.c
```

Observa la estructura del programa:

- Variables globales y `static` (static lifetime)
- Variables locales sin inicializar (automatic lifetime)
- El programa imprime ambos grupos

### Paso 1.2 -- Predecir antes de compilar

Antes de compilar, responde mentalmente:

- Las variables globales (`global_int`, `global_array`) tendran valor 0 o basura?
- La variable `static_local` dentro de `main` tendra valor 0 o basura?
- Las variables locales (`local_int`, `local_double`) tendran valor 0 o basura?
- El compilador producira warnings? Para cuales variables?

### Paso 1.3 -- Compilar con warnings habilitados

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic default_values.c -o default_values
```

Salida esperada (warnings):

```
default_values.c: warning: 'local_int' is used uninitialized [-Wuninitialized]
default_values.c: warning: 'local_double' is used uninitialized [-Wuninitialized]
default_values.c: warning: 'local_ptr' is used uninitialized [-Wuninitialized]
```

El compilador advierte sobre las variables locales no inicializadas. No advierte
sobre las globales ni las `static` porque sabe que esas siempre son 0.

### Paso 1.4 -- Ejecutar el programa

```bash
./default_values
```

Salida esperada:

```
=== Variables con static lifetime (deben ser 0) ===
global_int:    0
static_double: 0.000000
static_local:  0
static_ptr:    (nil)
global_array:  0 0 0 0 0

=== Variables locales (valor indeterminado) ===
NOTA: estos valores son basura del stack.
Cada ejecucion puede mostrar valores distintos.
local_int:     ~0
local_double:  ~0.000000
local_ptr:     ~(nil)
local_array:   ~0 ~0 ~0 ~0 ~0
```

Las variables con static lifetime son siempre 0 (garantizado por el estandar).
Las variables locales pueden mostrar 0 o basura -- depende del estado del stack.
En muchos sistemas modernos el stack parece limpio en un programa simple, pero
eso no es garantia. En programas mas complejos, las locales tendran basura real.

El punto clave: el compilador **no garantiza** ningun valor para las locales.
Que aparezcan como 0 en esta ejecucion es coincidencia, no regla.

### Paso 1.5 -- Verificar la razon: segmentos de memoria

```bash
size default_values
```

Salida esperada (los numeros varian):

```
   text    data     bss     dec     hex filename
  ~2000    ~600     ~40   ~2600     ... default_values
```

Observa el campo `bss`. Las variables globales sin inicializador (`global_int`,
`global_array`, `static_double`) viven en BSS. El sistema operativo llena BSS
con ceros al cargar el programa. Las locales viven en el stack, que no se limpia.

### Limpieza de la parte 1

```bash
rm -f default_values
```

---

## Parte 2 -- Designated initializers (C99)

**Objetivo**: Usar designated initializers para inicializar structs por nombre
de campo y arrays por indice, verificando que los elementos no mencionados
quedan en 0.

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat designated_init.c
```

Observa cuatro patrones distintos:

1. Struct con campos designados (y campos omitidos)
2. Array con indices designados (sparse array)
3. Array indexado por `enum`
4. Mezcla de posicional y designado en un array

### Paso 2.2 -- Predecir antes de compilar

Antes de compilar, responde mentalmente:

- En el struct `Config`, que valor tendran `buffer_size` y `debug_level`?
- En `sparse[10] = { [2] = 42, [7] = 99 }`, que valor tendra `sparse[0]`?
- En `mixed[6] = { [2] = 10, 20, 30 }`, a que indice ira el valor `20`?

### Paso 2.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic designated_init.c -o designated_init
./designated_init
```

Salida esperada:

```
=== Designated initializers: structs ===
port:            8080
max_connections: 100
timeout_ms:      5000
buffer_size:     0 (implicito)
debug_level:     0 (implicito)

=== Designated initializers: arrays ===
sparse: 0 0 42 0 0 0 0 99 0 0

=== Array indexado por enum ===
color_name[0] = "red"
color_name[1] = "green"
color_name[2] = "blue"

=== Posicional despues de designado ===
mixed: 0 0 10 20 30 0
Despues de [2]=10, los valores 20 y 30 van a indices 3 y 4
```

Puntos clave:

- Los campos/indices no mencionados se inicializan a 0 (regla del estandar)
- Despues de un designador `[2] = 10`, los valores posicionales continuan desde
  el indice 3 en adelante
- Indexar arrays con `enum` hace el codigo mucho mas legible que usar numeros

### Paso 2.4 -- Compilar sin warnings (verificacion)

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic designated_init.c -o designated_init 2>&1
echo "Exit code: $?"
```

Salida esperada:

```
Exit code: 0
```

El compilador no produce ningun warning. Designated initializers son estandar
C99/C11 y perfectamente validos.

### Limpieza de la parte 2

```bash
rm -f designated_init
```

---

## Parte 3 -- Compound literals (C99)

**Objetivo**: Usar compound literals para crear valores temporales de structs
y arrays, pasarlos a funciones sin variables intermedias, y entender su lifetime.

### Paso 3.1 -- Examinar el codigo fuente

```bash
cat compound_literals.c
```

Observa los usos:

1. `draw_line()` recibe compound literals como argumentos
2. `midpoint()` retorna un compound literal
3. `sum_array()` recibe un array compound literal
4. Un compound literal asignado a un puntero

### Paso 3.2 -- Predecir antes de compilar

Antes de compilar, responde mentalmente:

- Cual es el resultado de `midpoint({0,0}, {10,20})`?
- Cual es el resultado de `sum_array({10,20,30,40})`?
- La sintaxis `(struct Point){ .x = 5, .y = 10 }` crea una variable con nombre?

### Paso 3.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic compound_literals.c -o compound_literals
./compound_literals
```

Salida esperada:

```
=== Compound literals como argumentos ===
Linea de (0, 0) a (10, 20)
Linea de (5, 5) a (15, 25)

=== Compound literal como retorno ===
Midpoint: (5, 10)

=== Compound literal con arrays ===
sum_array({10,20,30,40}) = 100

=== Compound literal y punteros ===
ptr[0]=100  ptr[1]=200  ptr[2]=300
```

Puntos clave:

- Un compound literal crea un valor temporal sin nombre
- Dentro de una funcion, su lifetime es el del bloque (como una variable local)
- Son ideales para pasar structs a funciones sin crear variables intermedias
- `(int[]){10, 20, 30}` crea un array temporal al que se puede apuntar con un
  puntero, pero ese array se destruye al salir del bloque

### Paso 3.4 -- Distinguir compound literal de cast

Un compound literal no es un cast. Compara:

```bash
cat << 'EOF'
// Cast: convierte un valor existente
int x = (int)3.14;

// Compound literal: CREA un valor nuevo
struct Point p = (struct Point){ .x = 1, .y = 2 };

// La diferencia es la lista de inicializacion { ... } despues del parentesis.
EOF
```

No es necesario compilar esto. Es una distincion conceptual importante: la
sintaxis `(tipo){ ... }` con llaves es compound literal, no cast.

### Limpieza de la parte 3

```bash
rm -f compound_literals
```

---

## Parte 4 -- Inicializacion parcial y zero-initialization con {0}

**Objetivo**: Dominar la inicializacion parcial (donde lo no especificado se
llena con ceros) y el patron `{0}` como forma idiomatica de inicializar todo
a cero.

### Paso 4.1 -- Examinar el codigo fuente

```bash
cat partial_init.c
```

Observa cinco patrones:

1. Array con menos elementos que su tamano
2. Array y buffer de chars con `{0}`
3. Struct con `{0}` (todos los campos a cero)
4. Struct con inicializacion parcial (designated + zero-fill)
5. Array de structs con inicializacion parcial

### Paso 4.2 -- Predecir antes de compilar

Antes de compilar, responde mentalmente:

- En `int partial[8] = {10, 20, 30}`, que valor tendra `partial[5]`?
- En `struct Sensor s_partial = { .id = 7, .temperature = 22.5 }`, que valor
  tendra `s_partial.humidity`?
- En un array de 3 structs donde solo se inicializan 2, que valores tendra
  el tercer struct?

### Paso 4.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic partial_init.c -o partial_init
./partial_init
```

Salida esperada:

```
=== Inicializacion parcial de arrays ===
partial: 10 20 30 0 0 0 0 0
Solo los 3 primeros tienen valor, el resto es 0

=== Zero-initialization con {0} ===
zeros: 0 0 0 0 0 0 0 0 0 0
str_buf como string: "" (vacio, todo NUL)
str_buf[0] = 0, str_buf[19] = 0

=== Zero-initialization de structs con {0} ===
s_zero.id:          0
s_zero.temperature: 0.000000
s_zero.humidity:    0.000000
s_zero.active:      0
s_zero.name:        ""

=== Inicializacion parcial de structs ===
s_partial.id:          7
s_partial.temperature: 22.500000
s_partial.humidity:    0.000000 (implicito)
s_partial.active:      0 (implicito)
s_partial.name:        "" (implicito)

=== Array de structs con {0} ===
sensors[0]: id=1 temp=20.0 humid=0.0 active=0 name="sala"
sensors[1]: id=2 temp=0.0 humid=0.0 active=0 name="cocina"
sensors[2]: id=0 temp=0.0 humid=0.0 active=0 name=""
```

Puntos clave:

- La regla es simple: si hay inicializador (`= {...}`), todo lo no mencionado
  se llena con ceros
- `{0}` es el patron idiomatico para "todo a cero" -- funciona con arrays,
  structs, y arrays de structs
- En el array de structs, `sensors[2]` no tiene inicializador explicito pero
  tiene todos sus campos en 0 porque el array tiene inicializador parcial

### Paso 4.4 -- La diferencia entre {0} y sin inicializador

Este paso es conceptual. Reflexiona sobre la diferencia:

```bash
cat << 'EOF'
int arr1[100];        // Global: todo 0 (BSS)
int arr2[100] = {0};  // Global: todo 0 (explicitamente)

void foo(void) {
    int arr3[100];        // Local: BASURA (sin inicializador)
    int arr4[100] = {0};  // Local: todo 0 (zero-initialized)
}

// arr1 y arr2 producen el mismo resultado, pero arr2 es mas claro.
// arr3 y arr4 son MUY DIFERENTES: arr3 es basura, arr4 es todo ceros.
// Para locales, {0} es esencial cuando necesitas ceros.
EOF
```

La regla practica: para variables locales, siempre inicializar. Usar `{0}` cuando
necesites todo a cero. No confiar en que "ya sera cero" -- eso solo es verdad
para variables con static lifetime.

### Limpieza de la parte 4

```bash
rm -f partial_init
```

---

## Limpieza final

```bash
rm -f default_values designated_init compound_literals partial_init
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  compound_literals.c  default_values.c  designated_init.c  partial_init.c
```

---

## Conceptos reforzados

1. Las variables con **static lifetime** (globales, `static`) se inicializan a
   cero automaticamente porque viven en BSS, que el sistema operativo llena con
   ceros al cargar el programa.

2. Las variables **locales** (automatic lifetime) no se inicializan -- contienen
   basura del stack. Leerlas sin asignarles valor es comportamiento indefinido,
   y el compilador advierte con `-Wuninitialized`.

3. Los **designated initializers** (C99) permiten inicializar campos de structs
   por nombre (`.campo = valor`) e indices de arrays por posicion
   (`[indice] = valor`). Los campos o indices no mencionados se llenan con ceros.

4. Los **compound literals** (C99) crean valores temporales sin nombre con la
   sintaxis `(tipo){ ... }`. Son utiles para pasar structs o arrays a funciones
   sin crear variables intermedias. Su lifetime es el del bloque donde se crean.

5. La **inicializacion parcial** llena con ceros todo lo no especificado. El
   patron `{0}` es la forma idiomatica de inicializar cualquier tipo agregado
   (array, struct) completamente a cero.

6. La diferencia critica esta en las locales: `int arr[100];` es basura, pero
   `int arr[100] = {0};` es todo ceros. Para globales y `static`, ambas formas
   dan ceros, pero `{0}` explicita la intencion.
