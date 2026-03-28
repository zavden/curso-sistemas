# Lab -- Fragmentacion de heap

## Objetivo

Observar como se fragmenta el heap tras ciclos de malloc/free, medir el
overhead interno del allocator, y aplicar estrategias para reducir la
fragmentacion. Al finalizar, sabras interpretar las direcciones que retorna
malloc, distinguir fragmentacion externa de interna, y elegir patrones de
alocacion que minimizan el desperdicio.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `frag_visual.c` | Aloca y libera bloques alternando para visualizar huecos |
| `frag_external.c` | Demuestra fragmentacion externa con huecos no contiguos |
| `frag_internal.c` | Mide el overhead interno con malloc_usable_size |
| `frag_mitigation.c` | Estrategias: tamanos uniformes, pool, batch, realloc |

---

## Parte 1 -- Visualizar fragmentacion

**Objetivo**: Alocar bloques, liberar algunos alternadamente, y observar como
las nuevas alocaciones reutilizan (o no) los huecos.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat frag_visual.c
```

Observa la estructura:

- Se alocan 10 bloques de 100 bytes
- Se liberan los de indice par (0, 2, 4, 6, 8)
- Se alocan 5 bloques nuevos del mismo tamano
- Se imprimen todas las direcciones para comparar

### Paso 1.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic frag_visual.c -o frag_visual
```

Antes de ejecutar, predice:

- Las 10 direcciones originales seran contiguas o separadas?
- Los 5 bloques nuevos (mismo tamano) reutilizaran las direcciones
  de los bloques liberados, o recibiran direcciones nuevas?

### Paso 1.3 -- Verificar la prediccion

```bash
./frag_visual
```

Salida esperada (las direcciones varian):

```
=== Allocating 10 blocks of 100 bytes ===

Block  0: 0x<addr_0>
Block  1: 0x<addr_1>
Block  2: 0x<addr_2>
...
Block  9: 0x<addr_9>

=== Freeing even-indexed blocks (0, 2, 4, ...) ===

Freeing block 0 (0x<addr_0>)
Freeing block 2 (0x<addr_2>)
...

=== Allocating 5 new blocks of 100 bytes ===

New block 0: 0x<addr_8>
New block 1: 0x<addr_6>
New block 2: 0x<addr_4>
New block 3: 0x<addr_2>
New block 4: 0x<addr_0>
```

Observa:

- Los 10 bloques originales tienen direcciones consecutivas (cada uno separado
  por ~112 bytes: 100 de datos + overhead del allocator).
- Los 5 bloques nuevos reutilizan **exactamente** las direcciones de los
  bloques liberados, pero en orden inverso (LIFO -- el allocator usa un
  fast bin tipo pila).
- Los bloques impares (1, 3, 5, 7, 9) siguen intercalados con los nuevos.
  El heap parece un tablero de ajedrez: bloques originales y nuevos alternando.

Esto es el patron clasico de fragmentacion: bloques intercalados. Si uno de
los nuevos fuera de tamano diferente, no cabria en los huecos de 100 bytes.

### Limpieza de la parte 1

```bash
rm -f frag_visual
```

---

## Parte 2 -- Fragmentacion externa

**Objetivo**: Crear huecos entre bloques alocados y demostrar que un bloque
grande no puede usar la memoria fragmentada.

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat frag_external.c
```

Observa las 3 fases:

1. Se alocan 20 bloques de 128 bytes
2. Se liberan los de indice impar (1, 3, 5, ...) -- 10 huecos de 128 bytes
3. Se intenta alocar un bloque de 1280 bytes (10 x 128 = total liberado)

### Paso 2.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic frag_external.c -o frag_external
```

Antes de ejecutar, predice:

- Hay 1280 bytes libres en total (10 huecos de 128). malloc(1280) va a
  encontrar un bloque contiguo de 1280 bytes en los huecos?
- Si malloc consigue la memoria, estara ubicada entre los bloques
  originales o en otra parte del heap?

### Paso 2.3 -- Verificar la prediccion

```bash
./frag_external
```

Salida esperada (las direcciones varian):

```
=== Phase 1: Allocate 20 blocks of 128 bytes ===

Block  0: 0x<addr>
Block  1: 0x<addr>
...

=== Phase 2: Free odd-indexed blocks (1, 3, 5, ...) ===
This creates gaps of 128 bytes between allocated blocks.

Freed 10 blocks = 1280 bytes total free
But each free gap is only 128 bytes.

=== Phase 3: Try to allocate one large block ===

Requesting 1280 bytes (total freed memory)...
Large block allocated at: 0x<addr>
NOTE: glibc malloc succeeded because it can extend the heap
or use mmap. In a constrained allocator, this could fail
due to external fragmentation.

Compare the large block address with original blocks:
  Large block:     0x<addr_high>
  First original:  0x<addr_low>
  Last original:   0x<addr_mid>

The large block is NOT in the freed gaps.
The allocator had to find/create space elsewhere.
The freed gaps remain as fragmentation.
```

Analisis:

- glibc malloc pudo satisfacer la peticion porque extendio el heap (con sbrk)
  o uso mmap. Pero la direccion del bloque grande esta **fuera** del rango de
  los bloques originales.
- Los 10 huecos de 128 bytes siguen ahi, inutilizados. Esa es fragmentacion
  externa: hay 1280 bytes libres en total, pero en 10 pedazos de 128 bytes,
  no en un bloque contiguo.
- En un allocator simple (sin sbrk/mmap), o en un sistema con memoria
  limitada, este malloc(1280) fallaria.

### Limpieza de la parte 2

```bash
rm -f frag_external
```

---

## Parte 3 -- malloc_usable_size y fragmentacion interna

**Objetivo**: Medir cuanta memoria usa realmente el allocator por cada bloque
y cuanto se desperdicia internamente.

### Paso 3.1 -- Examinar el codigo fuente

```bash
cat frag_internal.c
```

Observa:

- Se usa `malloc_usable_size()` (extension de glibc, declarada en `<malloc.h>`)
  para obtener el tamano real utilizable de cada bloque
- Se prueban distintos tamanos (1, 7, 16, 17, ..., 256 bytes)
- Se compara el costo de 1000 x malloc(1) vs 1 x malloc(1000)

### Paso 3.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic frag_internal.c -o frag_internal
```

Antes de ejecutar, predice:

- Si pides malloc(1), cuantos bytes crees que el allocator realmente te da?
  (Pista: glibc tiene un tamano minimo de bloque.)
- Si pides malloc(17), el allocator te da exactamente 17 bytes o los redondea?
  A que numero?
- Que porcentaje de desperdicio tiene malloc(1) vs malloc(256)?

### Paso 3.3 -- Verificar la prediccion

```bash
./frag_internal
```

Salida esperada (los valores pueden variar segun la version de glibc):

```
=== Internal fragmentation: requested vs usable size ===

Requested     Usable        Overhead      Waste %
----------    ----------    ----------    --------
1             24            23            95.8    %
7             24            17            70.8    %
16            24            8             33.3    %
17            24            7             29.2    %
24            24            0             0.0     %
32            40            8             20.0    %
33            40            7             17.5    %
48            56            8             14.3    %
64            72            8             11.1    %
100           104           4             3.8     %
128           136           8             5.9     %
256           264           8             3.0     %
```

Analisis:

- **malloc(1) desperdicia ~96%** del espacio. El allocator da un minimo de ~24
  bytes (en esta version de glibc) porque necesita espacio para metadata y
  alineacion.
- Los tamanos se redondean al siguiente multiplo de 8 (con un minimo de 24
  bytes usables en glibc moderno).
- A medida que el tamano pedido crece, el porcentaje de desperdicio baja. Con
  256 bytes, el overhead es solo ~3%.
- Ademas del espacio usable, cada bloque tiene un **header** de 8-16 bytes que
  no se cuenta en `malloc_usable_size` (es metadata interna del allocator).

### Paso 3.4 -- Comparar muchas alocaciones pequenas vs una grande

Observa la segunda parte de la salida:

```
=== Comparing total memory: many small vs one large ===

1000 x malloc(1):
  Requested total:  1000 bytes
  Usable total:     24000 bytes
  Overhead factor:  24.0x

1 x malloc(1000):
  Requested total:  1000 bytes
  Usable total:     ~1000 bytes
  Overhead factor:  ~1.0x
```

1000 alocaciones de 1 byte usan **24 veces** mas memoria que una sola
alocacion de 1000 bytes. Esto sin contar los headers internos del allocator
(otros ~16 bytes por bloque), que elevan el uso real aun mas.

Leccion: las alocaciones muy pequenas son extremadamente ineficientes.

### Limpieza de la parte 3

```bash
rm -f frag_internal
```

---

## Parte 4 -- Estrategias de mitigacion

**Objetivo**: Aplicar cuatro estrategias para reducir la fragmentacion y
observar su efecto.

### Paso 4.1 -- Examinar el codigo fuente

```bash
cat frag_mitigation.c
```

El programa demuestra 4 estrategias:

1. **Tamanos uniformes** -- todas las alocaciones del mismo tamano
2. **Pool allocator** -- un bloque grande subdividido en bloques fijos
3. **Batch allocation** -- una sola alocacion grande en vez de muchas pequenas
4. **realloc** -- crecer un buffer existente en lugar de alocar uno nuevo

### Paso 4.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic frag_mitigation.c -o frag_mitigation
./frag_mitigation
```

### Paso 4.3 -- Analizar la estrategia 1: tamanos uniformes

Observa la primera seccion de la salida:

```
=== Strategy 1: Uniform allocation sizes ===

Mixed sizes (bad for fragmentation):
  malloc( 16) -> 0x<addr> (usable: 24)
  malloc(128) -> 0x<addr> (usable: 136)
  malloc( 32) -> 0x<addr> (usable: 40)
  ...

Uniform sizes (good for fragmentation):
  malloc( 64) -> 0x<addr> (usable: 72)
  malloc( 64) -> 0x<addr> (usable: 72)
  malloc( 64) -> 0x<addr> (usable: 72)
  ...
```

Con tamanos mixtos, los huecos que deja un bloque de 128 bytes no pueden ser
reutilizados por un malloc(256). Con tamanos uniformes, cada hueco es
exactamente del tamano correcto para cualquier nueva alocacion.

### Paso 4.4 -- Analizar la estrategia 2: pool allocator

Observa la seccion del pool:

```
=== Strategy 2: Pool allocator ===

Allocating 8 blocks from pool:
  pool_alloc() -> 0x<addr>
  ...
  Pool map: [########........]
  Allocs: 8, Frees: 0

Freeing blocks 1, 3, 5 (creating gaps):
  Pool map: [#.#.#.##........]
  Allocs: 8, Frees: 3

Allocating 3 more blocks:
  pool_alloc() -> 0x<addr>    <- reutiliza slot 1
  pool_alloc() -> 0x<addr>    <- reutiliza slot 3
  pool_alloc() -> 0x<addr>    <- reutiliza slot 5
  Pool map: [########........]
  Allocs: 11, Frees: 3
```

El mapa `[#.#.#.##........]` muestra visualmente los huecos. Al alocar 3
bloques nuevos, el pool reutiliza exactamente los slots liberados. No hay
fragmentacion externa porque todos los bloques son del mismo tamano fijo.

### Paso 4.5 -- Analizar la estrategia 3: batch allocation

```
=== Strategy 3: One large allocation vs many small ===

Many small allocations:
  100 x malloc(sizeof(int)) = 100 x malloc(4)
  Total wasted in overhead: ~2000 bytes

One batch allocation:
  1 x malloc(400)
  Total wasted in overhead: ~8 bytes
  Saving: ~1992 bytes
```

100 alocaciones individuales de un `int` (4 bytes) desperdician ~2000 bytes
en overhead. Una sola alocacion de 400 bytes desperdicia solo ~8 bytes.
Si necesitas un arreglo de structs, aloca un arreglo completo con un solo
malloc.

### Paso 4.6 -- Analizar la estrategia 4: realloc

```
=== Strategy 4: realloc instead of malloc+copy+free ===

Growing a buffer with realloc:

  Initial: malloc(16) -> 0x<addr_a> (usable: 24)
  realloc( 32) -> 0x<addr_b> (usable: 40) [moved]
  realloc( 64) -> 0x<addr_b> (usable: 72) [in-place]
  realloc(128) -> 0x<addr_b> (usable: 136) [in-place]
  realloc(256) -> 0x<addr_b> (usable: 264) [in-place]
```

Observa:

- El primer realloc mueve el bloque (la direccion cambia). El bloque anterior
  queda libre pero el allocator lo gestiona.
- Los siguientes reallocs crecen **in-place** (la direccion no cambia). Esto
  es posible porque `malloc_usable_size` devuelve mas espacio del pedido, y
  realloc aprovecha ese espacio extra sin mover nada.
- Crecer in-place no genera fragmentacion. Es preferible a hacer
  malloc + memcpy + free manualmente.

### Limpieza de la parte 4

```bash
rm -f frag_mitigation
```

---

## Limpieza final

```bash
rm -f frag_visual frag_external frag_internal frag_mitigation
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  frag_external.c  frag_internal.c  frag_mitigation.c  frag_visual.c
```

---

## Conceptos reforzados

1. Despues de ciclos de malloc/free con liberacion alternada, el heap queda
   con bloques intercalados (patron de tablero de ajedrez). El allocator
   reutiliza los huecos en orden LIFO (fast bins), pero solo si el tamano
   nuevo coincide con el hueco.

2. La **fragmentacion externa** ocurre cuando la memoria libre total es
   suficiente pero esta distribuida en huecos no contiguos. Un bloque grande
   no puede usar 10 huecos de 128 bytes aunque sumen 1280 -- necesita 1280
   bytes contiguos.

3. La **fragmentacion interna** es el desperdicio dentro de cada bloque.
   malloc(1) usa ~24 bytes reales (minimo del allocator), desperdiciando el
   96%. A mayor tamano pedido, menor porcentaje de desperdicio.

4. 1000 alocaciones de 1 byte consumen ~24x mas memoria que una sola
   alocacion de 1000 bytes. Las alocaciones muy pequenas son extremadamente
   ineficientes.

5. **Tamanos uniformes** eliminan la fragmentacion externa: cada hueco
   liberado sirve para cualquier nueva alocacion del mismo tamano.

6. Un **pool allocator** subdivide un bloque grande en slots de tamano fijo.
   No hay fragmentacion externa y la alocacion/liberacion es O(n) en la
   version simple (O(1) con una free list).

7. **Batch allocation** (un solo malloc para un arreglo completo) reduce
   drasticamente el overhead por elemento comparado con muchos mallocs
   individuales.

8. **realloc** puede crecer un bloque in-place si el allocator tiene espacio
   contiguo disponible, evitando fragmentacion. Es preferible a
   malloc + memcpy + free manual.
