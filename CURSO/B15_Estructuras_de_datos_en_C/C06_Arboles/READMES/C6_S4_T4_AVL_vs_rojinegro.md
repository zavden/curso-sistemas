# AVL vs rojinegro

## Dos soluciones al mismo problema

Tanto AVL como rojinegro (RB) resuelven la degeneración del BST
garantizando $O(\log n)$ para búsqueda, inserción y eliminación.  Pero
lo hacen con invariantes distintos que producen trade-offs diferentes:

| Aspecto | AVL | Rojinegro |
|---------|-----|-----------|
| Invariante | $\|h_{izq} - h_{der}\| \leq 1$ | 5 propiedades de color |
| Dato extra por nodo | Altura (4 bytes) | Color (1 bit) |
| Altura máxima | $1.44 \log_2(n+2)$ | $2 \log_2(n+1)$ |
| Balance | Más estricto | Más relajado |

La diferencia fundamental: AVL es **más estricto** en su balance, lo
que produce árboles más bajos pero requiere más trabajo para mantener.

---

## Altura comparada

Para el mismo número de nodos, el AVL siempre tiene altura menor o
igual que el RB:

| $n$ | Altura mínima | AVL máx | RB máx | Ratio RB/AVL |
|-----|--------------|---------|--------|-------------|
| 7 | 2 | 3 | 5 | 1.67 |
| 15 | 3 | 4 | 7 | 1.75 |
| 100 | 6 | 9 | 13 | 1.44 |
| 1000 | 9 | 14 | 19 | 1.36 |
| $10^6$ | 19 | 28 | 39 | 1.39 |
| $10^9$ | 29 | 43 | 59 | 1.37 |

El RB tree puede ser hasta ~40% más alto que el AVL.  Esto impacta
directamente el número de comparaciones en una búsqueda.

---

## Búsquedas: AVL gana

Como el AVL es más bajo, las búsquedas visitan menos nodos:

```
Buscar en AVL (h ≈ 1.44 log n):

         10
        /  \
       5    15
      / \   / \
     3   7 12  20

  Buscar 12: 10 → 15 → 12.  3 comparaciones.


Buscar en RB (h ≈ 2 log n, peor caso):

           10
          /  \
         5    15
        /    / \
       3   12   20
      /       \
     2        13
    /
   1

  Buscar 12: 10 → 15 → 12.  3 comparaciones (mismo camino).
  Buscar 1: 10 → 5 → 3 → 2 → 1.  5 comparaciones.
  En AVL serían ≤ 4.
```

Para aplicaciones **dominadas por búsquedas** (muchas lecturas, pocas
escrituras), AVL es mejor.

---

## Rotaciones por operación

Aquí es donde el RB tree tiene ventaja:

| Operación | AVL rotaciones | RB rotaciones |
|-----------|---------------|---------------|
| Inserción | ≤ 2 | ≤ 2 |
| Eliminación | Hasta $O(\log n)$ | **≤ 3** |

### Por qué la eliminación AVL puede requerir $O(\log n)$ rotaciones

En un AVL, una eliminación puede reducir la altura de un subárbol,
causando un desbalance en el padre.  La rotación en el padre puede
a su vez reducir la altura de ese subárbol, causando desbalance en
el abuelo.  Esta cascada puede propagarse hasta la raíz.

```
AVL — eliminación en cascada:

Eliminar un nodo en la rama izquierda profunda:
  → desbalance en nodo A → rotación → A baja de altura
  → desbalance en nodo B (padre de A) → rotación → B baja
  → desbalance en nodo C → rotación → ...
  → hasta la raíz: O(log n) rotaciones
```

### Por qué la eliminación RB tiene ≤ 3 rotaciones

En un RB tree, las correcciones post-eliminación se resuelven con
recoloreo (que se propaga pero es $O(1)$ por nivel) y a lo sumo 3
rotaciones.  Una vez que se hacen las rotaciones, la propagación
termina.

Esto es significativo en sistemas donde las rotaciones son costosas
(ej. cada rotación invalida caché, o requiere actualizar metadatos
como tamaños de subárboles).

---

## Recoloreo vs rotaciones

El RB tree compensa su menor número de rotaciones con más **recoloreo**:

| Corrección | AVL | RB |
|-----------|-----|-----|
| Actualizar altura | $O(\log n)$ nodos | — |
| Recolorear | — | Hasta $O(\log n)$ nodos |
| Rotaciones (inserción) | ≤ 2 | ≤ 2 |
| Rotaciones (eliminación) | Hasta $O(\log n)$ | ≤ 3 |
| **Total por operación** | $O(\log n)$ | $O(\log n)$ |

La complejidad total es la misma — $O(\log n)$.  La diferencia está en
qué tipo de trabajo se hace.  Las rotaciones mueven punteros (3
reasignaciones); el recoloreo cambia un bit.  Actualizar la altura es
una suma y un máximo.  En la práctica, todos son $O(1)$ por nodo.

---

## Espacio por nodo

| | AVL | RB | RB optimizado (kernel) |
|--|-----|-----|----------------------|
| Dato extra | `int height` (4 bytes) | `Color color` (1 byte min, 4 con padding) | 0 bytes (bit en puntero) |
| Puntero parent | No necesario | Necesario (8 bytes) | Incluido (color en su bit) |
| Total overhead | 4 bytes | 9-12 bytes | 0 bytes |

Si se implementa AVL sin `parent` (recursivo), el overhead es solo 4
bytes.  Si se implementa RB con `parent` + color, son 9-12 bytes.

La versión del kernel Linux tiene 0 bytes de overhead: el color se
almacena en el bit bajo del puntero `parent`, que ya existe.

Para LLRB (sin `parent`): 4 bytes (`int is_red`), igual que AVL.

---

## Complejidad de implementación

| Métrica | AVL | RB clásico | LLRB |
|---------|-----|-----------|------|
| Líneas de inserción | ~30 | ~80 | ~25 |
| Líneas de eliminación | ~35 | ~120 | ~40 |
| Casos en inserción | 4 (LL, RR, LR, RL) | 3 + simétricos = 6 | 3 |
| Casos en eliminación | 4 + simétricos | 6 + simétricos = 12 | 4 |
| Dificultad conceptual | Media | Alta | Media |

El RB clásico es notoriamente difícil de implementar correctamente.
La eliminación tiene tantos subcasos que la mayoría de los textos la
omiten o la presentan incompleta.

LLRB reduce la complejidad al nivel de AVL, sacrificando un poco de
rendimiento.

---

## Rendimiento en la práctica

Las diferencias teóricas se traducen así en benchmarks reales:

### Escenario 1: muchas búsquedas, pocas modificaciones

Ejemplo: diccionario, índice de solo lectura.

**AVL gana**: su menor altura (~30% menos) produce menos cache misses
por búsqueda.  Las pocas modificaciones hacen irrelevante el costo de
las rotaciones.

### Escenario 2: muchas inserciones y eliminaciones

Ejemplo: scheduler de procesos, gestión de conexiones.

**RB gana**: las eliminaciones con ≤ 3 rotaciones son más predecibles.
En un scheduler de tiempo real, un pico de $O(\log n)$ rotaciones en
AVL puede causar latencia inaceptable.

### Escenario 3: inserciones en orden

Ejemplo: insertar datos cronológicos.

**Ambos similares**: ambos rebalancean en $O(\log n)$ por operación.
AVL produce un árbol ligeramente más bajo.  La diferencia es marginal.

### Escenario 4: memoria limitada

**RB (kernel) gana**: 0 bytes extra con el truco del bit en el puntero.
AVL necesita 4 bytes por nodo.  Para $10^6$ nodos: 4 MB de diferencia.

---

## Tabla comparativa completa

| Criterio | AVL | Rojinegro | Ganador |
|----------|-----|-----------|---------|
| Altura máxima | $1.44 \log n$ | $2 \log n$ | AVL |
| Búsqueda (peor caso) | Más rápida | Más lenta | AVL |
| Rotaciones en inserción | ≤ 2 | ≤ 2 | Empate |
| Rotaciones en eliminación | $O(\log n)$ | ≤ 3 | **RB** |
| Espacio extra | 4 bytes/nodo | 1 bit/nodo (optimizado) | **RB** |
| Facilidad de implementación | Media | Difícil | AVL |
| Uso en bases de datos | Común | Raro | AVL |
| Uso en sistemas operativos | Raro | Dominante | **RB** |
| Uso en bibliotecas estándar | Raro | Dominante | **RB** |
| Balance | Más estricto | Más relajado | Depende |
| Peor caso predecible | Menos (rotaciones variables) | Más (rotaciones acotadas) | **RB** |

---

## Dónde se usa cada uno

### AVL en la práctica

| Sistema | Uso |
|---------|-----|
| Bases de datos (algunos) | Índices donde la búsqueda domina |
| PostgreSQL | Variante de B-tree (no AVL puro, pero principio similar) |
| Sistemas embebidos | Cuando la implementación simple importa |
| Competencias de programación | Fácil de implementar bajo presión |
| Enseñanza | Primer árbol balanceado que se aprende |

### Rojinegro en la práctica

| Sistema | Uso |
|---------|-----|
| C++ STL (`std::map/set`) | Contenedores ordenados |
| Java (`TreeMap/TreeSet`) | Contenedores ordenados |
| Linux kernel | Scheduler, VMAs, epoll, networking |
| .NET (`SortedDictionary`) | Contenedores ordenados |
| Nginx | Timer management |

La mayoría de las bibliotecas estándar eligen RB porque es un buen
compromiso: rendimiento consistente en todos los escenarios, y las
eliminaciones constantes son importantes para APIs genéricas donde no
se sabe el patrón de uso.

---

## Alternativas: B-trees

Ni AVL ni RB son la mejor opción para todos los casos.  Los **B-trees**
dominan en:

- **Bases de datos**: los nodos grandes (cientos de claves) minimizan
  accesos a disco.
- **Rust stdlib**: `BTreeMap`/`BTreeSet` usan B-tree, no RB ni AVL.
- **Sistemas de archivos**: ext4, NTFS, Btrfs usan variantes de B-tree.

Los B-trees almacenan múltiples claves por nodo, lo que mejora la
localidad de caché (un solo nodo cabe en una línea de caché).  Para
datos en memoria, los B-trees modernos suelen superar tanto a AVL como
a RB en rendimiento real, aunque la complejidad asintótica es la misma.

```
B-tree nodo (orden 4):
┌────┬────┬────┐
│ 10 │ 20 │ 30 │  ← 3 claves en un nodo
└─┬──┴─┬──┴─┬──┘
  │    │    │
 <10  10-20 20-30  >30

vs BST/AVL/RB:
    20
   / \
  10  30     ← 3 nodos separados en memoria
```

---

## Guía de decisión

```
¿Necesitas un árbol ordenado?
├── No → HashMap/HashSet (O(1) amortizado)
└── Sí
    ├── ¿En disco o con I/O? → B-tree / B+tree
    └── ¿En memoria?
        ├── ¿Producción (Rust)? → BTreeMap/BTreeSet (stdlib)
        ├── ¿Producción (C++/Java)? → std::map / TreeMap (RB interno)
        ├── ¿Producción (C)? → Implementar RB o usar librería
        ├── ¿Dominan las búsquedas? → AVL
        ├── ¿Dominan las eliminaciones? → RB
        ├── ¿Kernel / tiempo real? → RB
        └── ¿Aprendizaje / entrevista? → AVL (más simple)
```

---

## Ejercicios

### Ejercicio 1 — Elegir estructura

Para cada escenario, ¿cuál elegirías: AVL, RB, B-tree, o hash table?

a) Caché LRU con acceso por clave.
b) Índice de base de datos en disco.
c) Scheduler de procesos.
d) Autocompletado (búsqueda por prefijo).
e) Contar frecuencia de palabras.

<details>
<summary>Respuestas</summary>

a) **Hash table** (HashMap): acceso O(1) por clave.  El orden no importa
   (LRU se maneja con lista doblemente enlazada separada).

b) **B-tree/B+tree**: minimiza accesos a disco.  Cada nodo cabe en un
   bloque de disco.

c) **RB tree**: inserciones y eliminaciones frecuentes de procesos.
   El kernel Linux usa RB para su CFS.

d) **Trie** (o B-tree con prefijos): búsqueda por prefijo es O(k) donde
   k es la longitud del prefijo.  Un BST/AVL/RB también funciona con
   `range()` pero es menos eficiente para este patrón.

e) **Hash table** (HashMap): insertar/buscar en O(1).  No necesitamos
   orden.  Si necesitamos las palabras ordenadas al final:
   `HashMap` → recolectar → sort, o usar `BTreeMap` directamente.
</details>

### Ejercicio 2 — Comparar alturas

Un AVL y un RB tree contienen los mismos $n = 10^6$ nodos.  ¿Cuántas
comparaciones más hace una búsqueda en el peor caso del RB vs el AVL?

<details>
<summary>Cálculo</summary>

AVL peor caso: $h_{AVL} \leq 1.44 \log_2(10^6 + 2) \approx 1.44 \times 20 = 28.8 \approx 29$.

RB peor caso: $h_{RB} \leq 2 \log_2(10^6 + 1) \approx 2 \times 20 = 40$.

Diferencia: $40 - 29 = 11$ comparaciones más en el peor caso.

En porcentaje: el RB hace ~38% más comparaciones en el peor caso.

En la práctica, la diferencia es menor porque los árboles rara vez
alcanzan su peor caso teórico.
</details>

### Ejercicio 3 — Rotaciones en eliminación

Eliminar 1000 nodos de un AVL de $10^6$ nodos.  ¿Cuántas rotaciones
puede requerir en el peor caso?  ¿Y en un RB?

<details>
<summary>Cálculo</summary>

**AVL**: cada eliminación puede causar hasta $O(\log n) \approx 29$
rotaciones.  Total peor caso: $1000 \times 29 = 29000$ rotaciones.

**RB**: cada eliminación causa ≤ 3 rotaciones.  Total peor caso:
$1000 \times 3 = 3000$ rotaciones.

Ratio: el AVL puede hacer ~10× más rotaciones que el RB para las
mismas eliminaciones.

Pero cada rotación es $O(1)$ (3 reasignaciones de punteros), así que
la diferencia absoluta es ~26000 operaciones de puntero — microsegundos
en hardware moderno.  La diferencia importa más por efectos de caché
que por operaciones brutas.
</details>

### Ejercicio 4 — Espacio total

Calcula el espacio total para $10^6$ nodos `int` en: BST simple, AVL,
RB con parent, RB kernel (bit en puntero).

<details>
<summary>Cálculos</summary>

Asumiendo 64 bits, `int` = 4 bytes, puntero = 8 bytes:

**BST simple**: `data(4) + left(8) + right(8) + padding(4) = 24 bytes`.
Total: $24 \times 10^6 = 24$ MB.

**AVL**: `data(4) + height(4) + left(8) + right(8) = 24 bytes`
(el padding se absorbe).
Total: $24 \times 10^6 = 24$ MB.

**RB con parent**: `data(4) + color(1) + padding(3) + left(8) + right(8) + parent(8) = 32 bytes`.
Total: $32 \times 10^6 = 32$ MB.

**RB kernel** (color en bit del parent):
`data(4) + padding(4) + left(8) + right(8) + parent_color(8) = 32 bytes`.
Total: $32 \times 10^6 = 32$ MB.

El "0 bytes extra" del kernel se refiere al color, pero el puntero
parent sigue ocupando 8 bytes.  Sin parent (como AVL o LLRB), serían
24 bytes.  Con parent son 32 bytes independientemente de dónde se guarde
el color.

Resumen: AVL sin parent (24B) < RB con parent (32B).  La ventaja de
espacio del RB kernel es vs un RB que usara un campo separado para el
color (33 bytes con padding vs 32).
</details>

### Ejercicio 5 — ¿Cuándo AVL > RB en eliminación?

¿Existe un escenario donde la eliminación AVL es más rápida que la RB,
a pesar de las potenciales $O(\log n)$ rotaciones?

<details>
<summary>Análisis</summary>

Sí, cuando la eliminación **no causa cascada**.  En muchos casos
prácticos, la eliminación AVL solo requiere 0-1 rotaciones (la cascada
es el peor caso, no el caso común).

Además, la eliminación RB tiene más **casos lógicos** (6 subcasos +
simétricos), lo que implica más branches en el código.  En CPU modernas,
los branch mispredictions pueden ser más costosos que las rotaciones
extra del AVL.

En benchmarks reales, la diferencia de eliminación entre AVL y RB suele
ser < 10%, mucho menor que la diferencia teórica sugiere.  La ventaja
del RB es más relevante en contextos de **worst-case garantizado** (ej.
sistemas de tiempo real) que en rendimiento promedio.
</details>

### Ejercicio 6 — ¿Por qué Rust usa B-tree?

La stdlib de Rust eligió `BTreeMap` en vez de `TreeMap` (RB) o
`AVLMap`.  ¿Por qué?

<details>
<summary>Razones</summary>

1. **Cache locality**: un nodo B-tree almacena múltiples claves
   (típicamente 5-11) en memoria contigua.  Un recorrido toca menos
   líneas de caché que un RB/AVL donde cada nodo es una asignación
   separada en el heap.

2. **Menos asignaciones**: un B-tree con $n$ claves tiene ~$n/B$ nodos
   (donde $B$ es el orden), vs $n$ nodos en RB/AVL.  Menos `malloc`/
   `Box::new`, menos fragmentación.

3. **Rendimiento real**: en benchmarks con datos en memoria, B-trees
   modernos superan a RB trees en la mayoría de operaciones gracias a
   la localidad de caché, a pesar de tener la misma complejidad
   asintótica.

4. **API natural**: `BTreeMap` implementa `range()` eficientemente,
   que es una operación común en Rust.

5. **No necesita `parent`**: la implementación usa descenso recursivo
   sin puntero parent, que encaja mejor con el modelo de ownership de
   Rust.
</details>

### Ejercicio 7 — Inserción idéntica, resultados diferentes

Inserta `[4, 2, 6, 1, 3, 5, 7]` en un AVL y en un RB (LLRB).  ¿Los
árboles resultantes son idénticos?

<details>
<summary>Análisis</summary>

Este orden de inserción produce un árbol perfecto, así que no se
necesitan rotaciones en ninguno de los dos.

**AVL**:
```
         4
        / \
       2    6
      / \  / \
     1  3 5   7
```
Todos los balances son 0.

**LLRB**:
```
         4B
        / \
       2B   6B
      / \  / \
     1B 3B 5B  7B
```
Todos negros (flip_colors se aplica en cada nivel completo).

La **estructura** es idéntica.  Pero los **metadatos** difieren:
AVL almacena alturas (todos 0 para hojas, 1 para nivel 1, 2 para raíz),
LLRB almacena colores (todos negros aquí).

Para órdenes de inserción que causan rotaciones, los árboles
resultantes pueden diferir en estructura.
</details>

### Ejercicio 8 — Implementar búsqueda genérica

Escribe una función de búsqueda que funcione tanto para AVL como para
RB, usando solo la propiedad BST (sin depender de altura ni color).

<details>
<summary>Implementación</summary>

La búsqueda no usa ni la altura ni el color — es idéntica en BST,
AVL y RB:

```c
// Funciona para TreeNode, AVLNode, o RBNode
// (siempre que tengan data, left, right)
void *bst_search(void *root, int key,
                 int (*get_data)(void *),
                 void *(*get_left)(void *),
                 void *(*get_right)(void *)) {
    while (root) {
        int data = get_data(root);
        if (key == data) return root;
        root = (key < data) ? get_left(root) : get_right(root);
    }
    return NULL;
}
```

En Rust, esto es trivial con traits o simplemente con el mismo código:
el patrón `match key.cmp(&node.data)` es idéntico para AVL, RB, y BST.

Moraleja: la búsqueda es la misma en todos los BSTs.  La diferencia
está solo en inserción y eliminación (que mantienen el balance).
</details>

### Ejercicio 9 — Trade-off en un sistema real

Diseñas un servidor web que mantiene las conexiones activas en un
árbol ordenado por timeout.  Las operaciones son: insertar conexión
nueva, eliminar conexión expirada, buscar la conexión con menor
timeout (para el timer).  ¿AVL o RB?

<details>
<summary>Análisis</summary>

Operaciones dominantes:
- **Insertar**: cada nueva conexión.  Frecuente.
- **Eliminar**: cada conexión que expira o se cierra.  Frecuente.
- **Min**: encontrar la próxima expiración.  Frecuente.
- **Buscar por clave**: menos frecuente (solo al recibir datos).

Eliminaciones frecuentes → **RB tree** preferido (≤ 3 rotaciones vs
$O(\log n)$ en AVL).

`Min` es $O(\log n)$ en ambos.  Se puede optimizar manteniendo un
puntero al mínimo actualizado en $O(1)$.

Nginx usa exactamente este patrón con un RB tree para su timer
management.
</details>

### Ejercicio 10 — Resumir en una frase

Completa la frase: "Elige AVL cuando ___, elige RB cuando ___."

<details>
<summary>Respuesta</summary>

"Elige AVL cuando **las búsquedas dominan y la implementación simple
importa**; elige RB cuando **las eliminaciones son frecuentes y
necesitas rendimiento predecible en el peor caso**."

Versión más concisa: "AVL para leer más, RB para escribir más."

Y en la práctica: si estás usando una biblioteca estándar (C++, Java,
Rust), la elección ya está hecha por ti — y suele ser la correcta para
uso general.
</details>
