# Lab — Clases de almacenamiento

## Objetivo

Observar el comportamiento de las cuatro storage class specifiers de C (`auto`,
`static`, `extern`, `register`) y verificar en que segmento de memoria reside
cada tipo de variable usando herramientas de inspeccion binaria. Al finalizar,
sabras predecir el lifetime, scope e inicializacion de una variable segun su
storage class.

## Prerequisitos

- GCC instalado
- Herramientas: `nm`, `objdump`, `size`
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `auto_vs_static.c` | Comparacion entre variable auto y static local |
| `static_init.c` | Valores por defecto de variables static no inicializadas |
| `config.h` | Header con declaraciones extern |
| `config.c` | Definiciones de variables globales |
| `main_extern.c` | Programa que usa variables externas via config.h |
| `register_demo.c` | Funciones con y sin register para comparar assembly |
| `storage_map.c` | Variables en distintos segmentos: global, static, local |

---

## Parte 1 — auto vs static

**Objetivo**: Demostrar que una variable `auto` (local normal) se reinicializa en
cada llamada, mientras que una variable `static` local persiste entre llamadas.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat auto_vs_static.c
```

Observa las dos funciones:

- `counter_auto()` declara `int count = 0` (auto por defecto)
- `counter_static()` declara `static int count = 0`

Ambas incrementan `count` y lo imprimen. `main()` llama a cada una 3 veces.

Antes de compilar, predice:

- Que valores imprimira `counter_auto()` en las 3 llamadas?
- Que valores imprimira `counter_static()` en las 3 llamadas?
- Habra alguna diferencia entre ellas?

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic auto_vs_static.c -o auto_vs_static
./auto_vs_static
```

Salida esperada:

```
Call 1:
  auto:   count = 1
  static: count = 1
Call 2:
  auto:   count = 1
  static: count = 2
Call 3:
  auto:   count = 1
  static: count = 3
```

`counter_auto()` siempre imprime 1: la variable `count` se crea en el stack al
entrar a la funcion, se inicializa a 0, se incrementa a 1, y se destruye al
salir. En la siguiente llamada, el proceso se repite desde cero.

`counter_static()` imprime 1, 2, 3: la variable `static count` vive en el
segmento de datos, se inicializa una sola vez (antes de `main`), y conserva su
valor entre llamadas. El `= 0` no se ejecuta en cada llamada — solo define el
valor inicial la primera vez.

### Paso 1.3 — Valores por defecto de static

Las variables `static` que no se inicializan explicitamente se garantizan en 0
(a diferencia de las variables `auto`, que tienen valor indeterminado).

```bash
cat static_init.c
```

Predice: que valor tendra cada variable static no inicializada?

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic static_init.c -o static_init
./static_init
```

Salida esperada:

```
=== Default values of static variables ===
static int:    0
static double: 0.000000
static char:   0 (ASCII)
static ptr:    (nil)
```

Todas se inicializaron a cero. Esto es parte del estandar C: las variables con
storage duration estatica (static y global) se inicializan a cero si no tienen
inicializador explicito. Las variables `auto` no tienen esta garantia.

### Limpieza de la parte 1

```bash
rm -f auto_vs_static static_init
```

---

## Parte 2 — extern (compartir variables entre archivos)

**Objetivo**: Usar `extern` para compartir variables entre archivos de
compilacion separados. Entender la diferencia entre declaracion y definicion,
y provocar errores de enlace.

### Paso 2.1 — Examinar la estructura de archivos

```bash
cat config.h
```

El header declara dos variables con `extern` y una funcion. Estas son
**declaraciones** — no reservan memoria, solo anuncian que las variables
existen en algun otro archivo.

```bash
cat config.c
```

Aqui estan las **definiciones**: `int verbose = 0` y `int max_retries = 3`
reservan memoria y asignan valores iniciales. Este archivo tambien incluye
`config.h` para verificar que las declaraciones coinciden con las definiciones.

```bash
cat main_extern.c
```

`main_extern.c` incluye `config.h` y accede a las variables como si fueran
locales. El enlazador resolvera las referencias a las definiciones en `config.c`.

### Paso 2.2 — Compilar cada archivo por separado

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -c config.c -o config.o
gcc -std=c11 -Wall -Wextra -Wpedantic -c main_extern.c -o main_extern.o
```

Antes de examinar los simbolos, predice:

- En `config.o`, `verbose` y `max_retries` estaran como definidos (D/B) o
  undefined (U)?
- En `main_extern.o`, esas mismas variables estaran como D/B o U?

### Paso 2.3 — Verificar simbolos con nm

```bash
nm config.o
```

Salida esperada (el orden puede variar):

```
0000000000000000 T config_print
0000000000000000 D max_retries
                 U printf
0000000000000000 B verbose
```

- `D max_retries` — definida en .data (inicializada a 3)
- `B verbose` — definida en .bss (inicializada a 0, que equivale a sin
  inicializar)
- `T config_print` — funcion definida aqui

```bash
nm main_extern.o
```

Salida esperada:

```
                 U config_print
0000000000000000 T main
                 U max_retries
                 U puts
                 U verbose
```

Todo `U` (undefined) excepto `main`. Las variables `verbose` y `max_retries`
se necesitan, pero no estan definidas aqui — el enlazador las buscara en otros
archivos objeto.

### Paso 2.4 — Enlazar y ejecutar

```bash
gcc config.o main_extern.o -o extern_demo
./extern_demo
```

Salida esperada:

```
=== Before modification ===
verbose     = 0
max_retries = 3

=== After modification ===
verbose     = 1
max_retries = 5
```

`main_extern.c` modifico las variables y `config_print()` (que vive en
`config.c`) ve los cambios. Ambos archivos acceden a la misma posicion de
memoria — `extern` hace que compartan la variable, no que cada uno tenga
una copia.

### Paso 2.5 — Error: undefined reference (sin definicion)

Que pasa si enlazamos solo `main_extern.o` sin `config.o`?

```bash
gcc main_extern.o -o extern_broken
```

Salida esperada (error):

```
undefined reference to `config_print'
undefined reference to `verbose'
undefined reference to `max_retries'
```

El enlazador encontro simbolos `U` en `main_extern.o` pero no tiene donde
resolverlos. Sin `config.o`, nadie define esas variables ni la funcion.

### Paso 2.6 — Error: multiple definition (definicion duplicada)

Crea un archivo que tambien defina `verbose`:

```bash
cat > duplicate_config.c << 'EOF'
int verbose = 99;
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic -c duplicate_config.c -o duplicate_config.o
gcc config.o duplicate_config.o main_extern.o -o extern_dup
```

Salida esperada (error):

```
multiple definition of `verbose'
```

Dos archivos objeto definen `verbose` — el enlazador no sabe cual usar. Solo
puede haber una definicion de cada variable global en todo el programa.

### Limpieza de la parte 2

```bash
rm -f config.o main_extern.o extern_demo duplicate_config.c duplicate_config.o
```

---

## Parte 3 — register (hint al compilador)

**Objetivo**: Verificar que `register` es solo una sugerencia que los
compiladores modernos ignoran, comparando el assembly generado con y sin
optimizacion.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat register_demo.c
```

Dos funciones identicas en logica: `sum_with_register()` usa `register` en
las variables del loop, `sum_without_register()` no. Ambas suman un arreglo.

### Paso 3.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic register_demo.c -o register_demo
./register_demo
```

Salida esperada:

```
With register:    150
Without register: 150
```

El resultado es identico. La pregunta es: el assembly generado es diferente?

### Paso 3.3 — Comparar assembly sin optimizacion

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -S -O0 register_demo.c -o register_O0.s
wc -l register_O0.s
```

Salida esperada:

```
~126 register_O0.s
```

Busca las dos funciones en el assembly:

```bash
grep -A1 "sum_with_register\|sum_without_register" register_O0.s | head -10
```

Predice: con `-O0` (sin optimizacion), habra alguna diferencia entre las dos
funciones en el assembly? El compilador honrara el `register`?

### Paso 3.4 — Verificar

```bash
diff <(sed -n '/sum_with_register/,/\.size.*sum_with_register/p' register_O0.s) \
     <(sed -n '/sum_without_register/,/\.size.*sum_without_register/p' register_O0.s | \
       sed 's/without_register/with_register/g')
```

Si la salida esta vacia (o casi), las funciones generan assembly identico incluso
sin optimizacion. Los compiladores modernos (GCC, Clang) ignoran `register` por
completo — el keyword solo prohibe tomar la direccion de la variable con `&`.

### Paso 3.5 — Comparar con optimizacion

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -S -O2 register_demo.c -o register_O2.s
wc -l register_O2.s
```

Salida esperada:

```
~89 register_O2.s
```

El assembly es mas corto con `-O2`. Pero la reduccion no tiene nada que ver con
`register` — es el optimizador general de GCC el que elimina instrucciones
redundantes, usa registros de forma eficiente, y puede incluso inline las
funciones.

```bash
grep "call" register_O0.s | wc -l
grep "call" register_O2.s | wc -l
```

Con `-O0` habra ~4 llamadas (`call`). Con `-O2` habra ~2 — el compilador
posiblemente inlineo las funciones de suma. El uso de registros lo decide
el compilador, no el programador.

### Paso 3.6 — register prohibe &

El unico efecto real de `register` es prohibir tomar la direccion de la variable:

```bash
cat > register_address.c << 'EOF'
#include <stdio.h>

int main(void) {
    register int x = 42;
    printf("address: %p\n", (void *)&x);
    return 0;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic register_address.c -o register_address
```

Salida esperada (error):

```
error: address of register variable 'x' requested
```

Esta es la unica restriccion practica de `register` en C. En C++ moderno
`register` esta deprecated, y en C23 tambien.

### Limpieza de la parte 3

```bash
rm -f register_demo register_O0.s register_O2.s register_address.c
```

---

## Parte 4 — Donde vive cada variable

**Objetivo**: Usar `nm`, `objdump` y `size` para verificar en que segmento de
memoria (.data, .bss, stack) reside cada tipo de variable.

### Paso 4.1 — Examinar el codigo fuente

```bash
cat storage_map.c
```

El programa declara variables en distintas categorias:

| Variable | Tipo | Inicializada |
|----------|------|--------------|
| `global_init` | Global | Si (42) |
| `global_no_init` | Global | No |
| `file_static_init` | static a nivel de archivo | Si (100) |
| `file_static_no_init` | static a nivel de archivo | No |
| `local_static` | static dentro de funcion | Si (99) |
| `local_var` | Local (auto) | Si (7) |

Predice: cuales apareceran en .data, cuales en .bss, y cuales no apareceran
en ninguna seccion (porque viven en el stack)?

### Paso 4.2 — Compilar el archivo objeto

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -c storage_map.c -o storage_map.o
```

### Paso 4.3 — Tabla de simbolos con nm

```bash
nm storage_map.o
```

Salida esperada (las direcciones pueden variar):

```
                 ... d file_static_init
                 ... b file_static_no_init
                 ... D global_init
                 ... B global_no_init
                 ... d local_static.0
                 ... T main
                 U printf
                 U puts
                 ... T show_addresses
```

Interpretacion de las letras:

| Letra | Significado | Segmento |
|-------|-------------|----------|
| `D` | Data, global visible | .data (inicializado) |
| `d` | Data, local/static | .data (inicializado) |
| `B` | BSS, global visible | .bss (sin inicializar) |
| `b` | BSS, local/static | .bss (sin inicializar) |
| `T` | Text (codigo) | .text |
| `U` | Undefined | (se resuelve al enlazar) |

La diferencia entre mayuscula y minuscula indica el linkage:

- Mayuscula (`D`, `B`, `T`) = linkage externo (visible desde otros archivos)
- Minuscula (`d`, `b`) = linkage interno (static, invisible fuera del archivo)

Observa que `local_var` no aparece. Las variables locales (auto) viven en el
stack y no existen en la tabla de simbolos — solo existen en runtime, no en
el archivo objeto.

### Paso 4.4 — Secciones con objdump

```bash
objdump -h storage_map.o | grep -E "Idx|\.text|\.data|\.bss|\.rodata"
```

Salida esperada (los tamanos pueden variar):

```
Idx Name          Size      ...
  0 .text         ~000000e2 ...
  1 .data         ~0000000c ...
  2 .bss          ~00000008 ...
  3 .rodata       ~0000017f ...
```

- `.text` (~226 bytes) — codigo ejecutable de `main()` y `show_addresses()`
- `.data` (~12 bytes) — 3 variables inicializadas: `global_init` (4),
  `file_static_init` (4), `local_static` (4) = 12 bytes
- `.bss` (~8 bytes) — 2 variables sin inicializar: `global_no_init` (4),
  `file_static_no_init` (4) = 8 bytes
- `.rodata` — las cadenas de formato de `printf`

### Paso 4.5 — Tamanos con size

```bash
size storage_map.o
```

Salida esperada:

```
   text    data     bss     dec     hex filename
   ~745      12       8    ~765     ... storage_map.o
```

`text` incluye el codigo y .rodata. `data` son 12 bytes (las 3 variables
inicializadas). `bss` son 8 bytes (las 2 variables sin inicializar). El BSS
no ocupa espacio en el archivo — el sistema operativo lo llena con ceros al
cargar el programa.

### Paso 4.6 — Compilar y verificar direcciones en runtime

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic storage_map.c -o storage_map
./storage_map
```

Salida esperada (las direcciones exactas varian):

```
=== Variable addresses ===

-- Global (initialized) --
  global_init:          0x40XXXX

-- Global (uninitialized) --
  global_no_init:       0x40XXXX

-- File static (initialized) --
  file_static_init:     0x40XXXX

-- File static (uninitialized) --
  file_static_no_init:  0x40XXXX

-- Local static (initialized) --
  local_static:         0x40XXXX

-- Local (stack) --
  local_var:            0x7fXXXXXXXXXX
```

Observa la diferencia clave: las variables globales y static tienen direcciones
en el rango `0x40XXXX` (segmentos .data/.bss, parte baja del espacio de
direcciones). La variable local tiene una direccion en `0x7fXXXXXXXXXX` (stack,
parte alta del espacio de direcciones). Esto confirma que viven en regiones de
memoria completamente distintas.

### Paso 4.7 — Verificar con nm en el ejecutable

```bash
nm storage_map | grep -E "global_init|global_no_init|file_static|local_static"
```

Ahora las direcciones son reales (no relativas a 0). Compara:

- Las variables `D`/`d` (data) tienen direcciones cercanas entre si
- Las variables `B`/`b` (bss) tienen direcciones cercanas entre si
- Las variables `d`/`b` (minuscula = static) no son visibles si usas `nm` sin
  flags especiales en el ejecutable final (pueden ser eliminadas por el linker)

### Limpieza de la parte 4

```bash
rm -f storage_map.o storage_map
```

---

## Limpieza final

```bash
rm -f *.o *.s auto_vs_static static_init extern_demo register_demo storage_map
rm -f duplicate_config.c duplicate_config.o register_address.c
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  auto_vs_static.c  config.c  config.h  main_extern.c  register_demo.c  static_init.c  storage_map.c
```

---

## Conceptos reforzados

1. Una variable `auto` (local) se crea en el stack al entrar al bloque y se
   destruye al salir. Si no se inicializa, su valor es indeterminado. El keyword
   `auto` nunca se escribe porque es el default.

2. Una variable `static` local persiste entre llamadas a la funcion. Se almacena
   en el segmento .data (si fue inicializada) o .bss (si no), y se inicializa
   una sola vez antes de `main`. Su scope sigue siendo local a la funcion.

3. Las variables `static` no inicializadas se garantizan en 0, a diferencia de
   las variables `auto` cuyo valor es indeterminado.

4. `extern` declara una variable sin definirla — no reserva memoria. La
   definicion debe existir en exactamente un archivo. Sin la definicion, el
   enlazador da "undefined reference". Con dos definiciones, da "multiple
   definition".

5. El patron header/source para variables globales es: `extern int var;` en el
   .h (declaracion) e `int var = valor;` en el .c (definicion). Otros archivos
   incluyen el .h.

6. `register` es ignorado por los compiladores modernos. El compilador decide
   mejor que el programador que variables poner en registros. La unica
   restriccion real es que no se puede tomar la direccion (`&`) de una variable
   `register`.

7. `nm` muestra donde vive cada variable: `D`/`d` = .data (inicializada),
   `B`/`b` = .bss (sin inicializar), `T` = .text (codigo). Mayuscula =
   linkage externo, minuscula = linkage interno (static).

8. Las variables locales (auto) no aparecen en `nm` porque viven en el stack
   y solo existen en runtime — no tienen entrada en la tabla de simbolos del
   archivo objeto.
