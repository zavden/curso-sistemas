# Comparación de variantes

## Las cuatro variantes de lista enlazada

A lo largo de C05 hemos estudiado cuatro variantes de lista enlazada:

| Variante | Punteros por nodo | Extremos |
|----------|:-:|----------|
| **Simple lineal** | `next` | head → ... → NULL |
| **Simple circular** | `next` | head → ... → head |
| **Doble lineal** | `prev`, `next` | NULL ← head ⟷ ... ⟷ tail → NULL |
| **Doble circular** | `prev`, `next` | head ⟷ ... ⟷ tail ⟷ head |

Y la estructura contigua de referencia:

| Estructura | Layout |
|-----------|--------|
| **Array dinámico** (Vec/VecDeque) | Bloque contiguo en heap |

---

## Tabla maestra de complejidades

Asumiendo puntero a `head` (y `tail` donde se indica).

| Operación | Simple lineal | Simple circular | Doble lineal | Doble circular | Array (Vec) | VecDeque |
|-----------|:---:|:---:|:---:|:---:|:---:|:---:|
| push_front | $O(1)$ | $O(1)$ | $O(1)$ | $O(1)$ | $O(n)$ | $O(1)$* |
| push_back | $O(1)$† | $O(1)$ | $O(1)$ | $O(1)$ | $O(1)$* | $O(1)$* |
| pop_front | $O(1)$ | $O(1)$ | $O(1)$ | $O(1)$ | $O(n)$ | $O(1)$* |
| pop_back | $O(n)$ | $O(n)$ | $O(1)$ | $O(1)$ | $O(1)$ | $O(1)$* |
| Acceso por índice | $O(n)$ | $O(n)$ | $O(n)$‡ | $O(n)$‡ | $O(1)$ | $O(1)$ |
| Buscar por valor | $O(n)$ | $O(n)$ | $O(n)$ | $O(n)$ | $O(n)$ | $O(n)$ |
| Insertar en medio (con puntero) | $O(n)$§ | $O(n)$§ | $O(1)$ | $O(1)$ | — | — |
| Insertar en medio (por posición) | $O(n)$ | $O(n)$ | $O(n)$‡ | $O(n)$‡ | $O(n)$ | $O(n)$ |
| Eliminar nodo (con puntero) | $O(n)$§ | $O(n)$§ | $O(1)$ | $O(1)$ | — | — |
| Eliminar por posición | $O(n)$ | $O(n)$ | $O(n)$‡ | $O(n)$‡ | $O(n)$ | $O(n)$ |
| Concatenar | $O(n)$ o $O(1)$† | $O(1)$ | $O(1)$ | $O(1)$ | $O(m)$ | $O(m)$ |
| Rotate left | $O(n)$ | $O(1)$ | $O(n)$ | $O(1)$ | $O(n)$ | $O(1)$¶ |
| Rotate right | $O(n)$ | $O(n)$ | $O(n)$ | $O(1)$ | $O(n)$ | $O(1)$¶ |
| Reverse | $O(n)$ | $O(n)$ | $O(n)$ | $O(n)$ | $O(n)$ | $O(n)$ |

\* Amortizado.  † Con tail pointer.  ‡ Desde el extremo más cercano: $\leq n/2$.
§ Necesita el nodo anterior, que requiere recorrer.  ¶ `VecDeque::rotate_left/right`.

---

## Memoria por nodo

Para un dato `int` (4 bytes) en plataforma de 64 bits:

| Variante | Campos | sizeof nodo | Overhead vs dato |
|----------|--------|:-----------:|:---:|
| Array | solo `int` | 4 bytes | 1× |
| Simple lineal | data + next | 16 bytes | 4× |
| Simple circular | data + next | 16 bytes | 4× |
| Doble lineal | data + prev + next | 24 bytes | 6× |
| Doble circular | data + prev + next | 24 bytes | 6× |

Para un dato grande (ej. struct de 200 bytes):

| Variante | sizeof nodo | Overhead vs dato |
|----------|:-----------:|:---:|
| Array | 200 bytes | 1× |
| Simple | 208 bytes | 1.04× |
| Doble | 216 bytes | 1.08× |

El overhead de punteros es insignificante para datos grandes.

### Memoria total de la estructura

| Variante | Struct de lista | Nodos × 1000 (int) | Total |
|----------|:-:|:-:|:-:|
| Simple (head) | 8 bytes | 16,000 | 16,008 |
| Simple (head+tail) | 16 bytes | 16,000 | 16,016 |
| Circular simple (tail) | 8 bytes | 16,000 | 16,008 |
| Doble (head+tail) | 16 bytes | 24,000 | 24,016 |
| Circular doble (head) | 8 bytes | 24,000 | 24,008 |
| Vec | 24 bytes (ptr+len+cap) | 4,000 | 4,024 |
| VecDeque | 32 bytes (ptr+len+cap+head) | 4,000* | 4,032 |

\* VecDeque puede tener slots vacíos (capacidad > longitud).

---

## Localidad de caché

La diferencia más importante en rendimiento real — más que la complejidad
asintótica para muchos casos prácticos.

### Array: acceso secuencial óptimo

```
Memoria:
  [10][20][30][40][50][60][70][80]...
  ←── línea de caché (64 bytes) ──→

Recorrer: CPU carga 64 bytes de golpe → 16 ints disponibles sin ir a RAM.
Prefetcher: detecta patrón secuencial → precarga la siguiente línea.
```

### Lista enlazada: acceso disperso

```
Memoria:
  0x1A00: [10|0x3F00]
  0x2B00: [algo no relacionado]
  0x3F00: [20|0x7100]
  0x4C00: [otro dato]
  0x7100: [30|0x1200]
  ...

Recorrer: cada next apunta a una dirección arbitraria → cache miss probable.
Prefetcher: no puede predecir la siguiente dirección → sin precarga.
```

### Impacto cuantitativo

Tiempos típicos de acceso a memoria (2024):

| Nivel | Latencia | Capacidad |
|-------|:--------:|:---------:|
| Registro | < 1 ns | bytes |
| L1 cache | ~1 ns | 32-64 KB |
| L2 cache | ~4 ns | 256 KB - 1 MB |
| L3 cache | ~10 ns | 4-32 MB |
| RAM | ~60-100 ns | GB |

Un recorrido de array tiene ~1 ns por acceso (L1 hit + prefetch).
Un recorrido de lista tiene ~10-100 ns por acceso (L2/L3 miss, RAM).

Para recorrer $10^6$ elementos:
- Array: ~1 ms
- Lista: ~10-100 ms

Factor 10-100× de diferencia con la misma complejidad asintótica $O(n)$.

---

## Cuándo usar cada variante

### Array dinámico (Vec / VecDeque)

**Usar cuando:**
- Acceso por índice frecuente.
- Recorrido secuencial (iteración, map, filter, reduce).
- Push/pop solo en los extremos.
- La mayoría de los casos — es la estructura por defecto.

**Evitar cuando:**
- Inserción/eliminación frecuente en medio (desplaza elementos).
- Push_front frecuente sin VecDeque.
- Necesitas que los punteros/referencias a elementos no se invaliden.

### Lista simple lineal

**Usar cuando:**
- Solo necesitas stack (push/pop al frente).
- Implementación educativa.
- Memoria fragmentada (cada nodo se alloca independientemente).

**Evitar cuando:**
- Necesitas acceso al final — pop_back es $O(n)$.
- Necesitas acceso por índice.

### Lista simple circular

**Usar cuando:**
- Recorrido cíclico natural (round-robin, buffers circulares).
- Rotación frecuente — $O(1)$.
- Solo un puntero en la estructura (tail da acceso a ambos extremos).
- Problema de Josephus y similares.

**Evitar cuando:**
- Necesitas recorrido inverso.
- La lógica de `do-while` y auto-referencia complica el código sin
  beneficio.

### Lista doble lineal

**Usar cuando:**
- Pop_back $O(1)$ necesario (deque).
- Eliminar nodos por puntero $O(1)$ (LRU cache, editores).
- Recorrido bidireccional.
- Insertar antes de un nodo dado.

**Evitar cuando:**
- El overhead de `prev` (50% más memoria por nodo) no se justifica.
- Solo usas push/pop al frente — la lista simple es suficiente.

### Lista doble circular

**Usar cuando:**
- Todas las ventajas de la doble lineal + rotación $O(1)$.
- Patrón del kernel Linux (`list_head` intrusivo).
- Sentinela único elimina todas las ramas.
- Rotación bidireccional $O(1)$.

**Evitar cuando:**
- La complejidad adicional del código no se justifica.
- Una doble lineal o un VecDeque cubren el caso.

---

## Diagrama de decisión

```
¿Necesitas una colección secuencial?
│
├── ¿Acceso por índice O(1)?
│   ├── Sí → Vec o VecDeque
│   └── No ↓
│
├── ¿Solo push/pop al frente (stack)?
│   └── Sí → Lista simple (o Vec)
│
├── ¿Push/pop en ambos extremos (deque)?
│   ├── ¿Rendimiento importa? → VecDeque
│   └── ¿Aprendizaje / flexibilidad? → Lista doble
│
├── ¿Eliminar nodos por puntero O(1)?
│   └── Sí → Lista doble (lineal o circular)
│
├── ¿Recorrido cíclico / rotación O(1)?
│   ├── ¿Solo forward? → Circular simple
│   └── ¿Bidireccional? → Circular doble
│
├── ¿Concatenar O(1)?
│   └── Sí → Lista enlazada (cualquier variante con tail)
│
└── ¿Ningún requisito especial?
    └── Vec o VecDeque (localidad de caché gana)
```

---

## Benchmark conceptual

Para $n = 10^6$ operaciones de cada tipo:

| Operación | Vec | VecDeque | Lista simple | Lista doble |
|-----------|:---:|:--------:|:---:|:---:|
| push_back ×n | ~5 ms | ~5 ms | ~80 ms* | ~80 ms* |
| push_front ×n | ~minutos | ~5 ms | ~80 ms* | ~80 ms* |
| Recorrer (sum) | ~1 ms | ~1 ms | ~50 ms | ~50 ms |
| Acceso aleatorio ×n | ~3 ms | ~3 ms | ~horas | ~horas |
| Insertar en medio ×n | ~minutos | ~minutos | $O(1)$† | $O(1)$† |

\* Dominado por `malloc` (una allocation por nodo) y cache misses.
† Con puntero al nodo; sin puntero, la búsqueda sigue siendo $O(n)$.

Los tiempos de lista enlazada son peores que array/VecDeque para casi todo
excepto inserción/eliminación con puntero al nodo.  La causa: localidad
de caché, no complejidad asintótica.

---

## Resumen: trade-offs fundamentales

| Trade-off | Favorece listas | Favorece arrays |
|-----------|----------------|----------------|
| Inserción/eliminación en medio | $O(1)$ con puntero | $O(n)$ desplazar |
| Concatenación | $O(1)$ | $O(n)$ copiar |
| Estabilidad de punteros | Nodos no se mueven | Realloc invalida todo |
| Localidad de caché | Mala (disperso) | Excelente (contiguo) |
| Overhead de memoria | 8-16 bytes/nodo + alloc | 0 (amortizado) |
| Acceso por índice | $O(n)$ | $O(1)$ |
| Allocation | Una por nodo (lento) | Una por resize (infrecuente) |
| Recorrido | Cache miss cada nodo | Prefetch automático |

La lección principal: la complejidad asintótica ($O$-notation) no cuenta
toda la historia.  En hardware moderno, las constantes de caché dominan
para la mayoría de cargas de trabajo.  Las listas enlazadas ganan en
nichos específicos donde sus ventajas asintóticas son cruciales (inserción
$O(1)$ con cursor, concatenación $O(1)$, estabilidad de punteros).

---

## C vs Rust: resumen de ergonomía

| Variante | Ergonomía en C | Ergonomía en Rust |
|----------|:-:|:-:|
| Simple lineal | Natural | Aceptable (Option<Box>) |
| Simple circular | Natural | Requiere unsafe |
| Doble lineal | Natural | Difícil (Rc<RefCell> o unsafe) |
| Doble circular | Natural | Difícil (unsafe preferible) |
| Intrusiva (list_head) | Natural (container_of) | Muy difícil (unsafe + offset_of) |

C gana en ergonomía para todas las variantes de lista enlazada.  Rust gana
en seguridad (bugs de memoria imposibles en safe, delimitados en unsafe) y
en integración con el ecosistema (genéricos, iteradores, traits, Drop).

La lista enlazada es el caso clásico donde el modelo de ownership de Rust
choca con la estructura de datos.  Para estructuras tipo árbol (un padre,
múltiples hijos), Rust es ergonómico.  Para grafos y listas con múltiples
referencias a un nodo, Rust requiere decisiones explícitas que C no exige.

---

## Ejercicios

### Ejercicio 1 — Completar la tabla

Sin mirar la tabla maestra, escribe de memoria la complejidad de `push_front`,
`push_back`, `pop_front`, `pop_back` para las 4 variantes de lista + Vec +
VecDeque.

<details>
<summary>Tabla</summary>

| | push_front | push_back | pop_front | pop_back |
|-|:---:|:---:|:---:|:---:|
| Simple lineal | $O(1)$ | $O(1)$† | $O(1)$ | $O(n)$ |
| Simple circular | $O(1)$ | $O(1)$ | $O(1)$ | $O(n)$ |
| Doble lineal | $O(1)$ | $O(1)$ | $O(1)$ | $O(1)$ |
| Doble circular | $O(1)$ | $O(1)$ | $O(1)$ | $O(1)$ |
| Vec | $O(n)$ | $O(1)$* | $O(n)$ | $O(1)$ |
| VecDeque | $O(1)$* | $O(1)$* | $O(1)$* | $O(1)$* |

† Con tail pointer.  * Amortizado.

La regla: las listas ganan en `push_front`/`pop_front` vs Vec.  La doble
gana en `pop_back` vs simple.  VecDeque empata en todo.
</details>

### Ejercicio 2 — Elegir estructura

Para cada escenario, elige la estructura óptima:
1. Cola de impresión (FIFO).
2. Historial de navegador (atrás/adelante).
3. Tabla de puntuaciones (acceso por ranking).
4. Playlist con modo repeat.

<details>
<summary>Respuestas</summary>

1. **Cola FIFO → VecDeque**.  Push_back + pop_front, ambos $O(1)$ amortizado.
   Lista simple con tail también funciona, pero VecDeque es más rápido.

2. **Historial → Lista doble o Vec**.  Atrás = prev, adelante = next.  En la
   práctica, Vec con índice de posición actual es más simple y rápido.

3. **Puntuaciones por ranking → Vec**.  Acceso por índice $O(1)$.  Insertar
   manteniendo orden es $O(n)$ pero infrecuente vs consultar.

4. **Playlist con repeat → Circular simple o doble**.  Recorrido infinito
   natural.  Si necesitas "canción anterior": circular doble.  Si solo
   "siguiente": circular simple.
</details>

### Ejercicio 3 — Memoria total

Calcula la memoria total para almacenar 10,000 structs de 64 bytes en:
1. Vec.
2. Lista simple.
3. Lista doble.

<details>
<summary>Cálculo</summary>

```
Dato: 64 bytes

1. Vec:
   10000 × 64 + 24 (ptr+len+cap) = 640,024 bytes
   Overhead: 0.004%

2. Lista simple:
   Nodo: 64 + 8 (next) = 72 bytes (sin padding asumiendo alineación a 8)
   10000 × 72 + 8 (head) = 720,008 bytes
   Overhead: 12.5%

3. Lista doble:
   Nodo: 64 + 8 (prev) + 8 (next) = 80 bytes
   10000 × 80 + 16 (head+tail) = 800,016 bytes
   Overhead: 25%
```

Para datos grandes (64 bytes), el overhead de punteros es tolerable (12-25%).
Para datos pequeños (4 bytes), el overhead domina (300-500%).
</details>

### Ejercicio 4 — Caché: predicción de rendimiento

Un programa recorre $10^6$ nodos sumando sus valores.  Predice el ratio
de rendimiento entre Vec y lista doble, considerando:
- L1 cache line: 64 bytes.
- L1 hit: 1 ns.
- RAM access: 80 ns.
- Dato: `int` (4 bytes).

<details>
<summary>Análisis</summary>

```
Vec:
  64 bytes / 4 bytes = 16 ints por cache line
  10^6 ints / 16 = 62,500 cache loads
  Con prefetch: ~1 ns por int (L1 hit tras primera carga)
  Total: ~1 ms

Lista doble:
  Cada nodo: 24 bytes (int + prev + next)
  ~2-3 nodos por cache line, pero nodos dispersos → 1 miss por nodo
  ~80 ns por nodo (RAM)
  Total: ~80 ms

Ratio: ~80× más lento con lista
```

En la práctica, L2/L3 amortiguan el impacto (ratio real ~10-30×), pero
la diferencia es significativa.
</details>

### Ejercicio 5 — Inserción en medio: ¿dónde ganan las listas?

Un programa mantiene una colección ordenada de $n = 10000$ elementos.
Cada operación es:
1. Buscar la posición correcta para el nuevo elemento.
2. Insertar.

Compara Vec (binary search + insert) vs lista doble (recorrer + insert).

<details>
<summary>Análisis</summary>

```
Vec:
  Buscar: binary search O(log n) = ~14 comparaciones ≈ ~20 ns
  Insertar: desplazar O(n/2) = ~5000 moves ≈ ~5 μs (memcpy optimizado)
  Total: ~5 μs

Lista doble:
  Buscar: recorrer O(n/2) = ~5000 nodos ≈ ~400 μs (cache misses)
  Insertar: redirigir punteros O(1) ≈ ~5 ns
  Total: ~400 μs
```

Vec gana ~80× a pesar de que su inserción es $O(n)$ y la de la lista es
$O(1)$.  La búsqueda domina, y binary search ($O(\log n)$) + caché
(secuencial) aplasta al recorrido lineal ($O(n)$) con cache misses.

Las listas ganan cuando ya tienes el puntero al nodo de inserción (ej.
cursor, LRU cache con hash map) y no necesitas buscar.
</details>

### Ejercicio 6 — Concatenación

Compara el costo de concatenar dos colecciones de $n = 10^5$ elementos:
1. `Vec::extend` (copiar).
2. `LinkedList::append` (reconectar punteros).

<details>
<summary>Resultado</summary>

```
Vec::extend:
  Copiar 10^5 elementos de 4 bytes = 400 KB memcpy
  Posible realloc si capacidad insuficiente
  Tiempo: ~50-100 μs

LinkedList::append:
  Reconectar 2 punteros (tail de a → head de b)
  Tiempo: ~5 ns

Ratio: ~10000-20000×
```

Esta es la ventaja más dramática de las listas enlazadas.  Si tu algoritmo
requiere concatenación frecuente de colecciones grandes, la lista enlazada
gana por órdenes de magnitud.
</details>

### Ejercicio 7 — Estabilidad de punteros

Explica qué significa "estabilidad de punteros" y da un ejemplo donde
una lista enlazada la necesita pero un Vec no puede garantizarla.

<details>
<summary>Explicación</summary>

Estabilidad de punteros: los elementos **no se mueven en memoria** cuando
la colección crece o se modifica.

```rust
// Vec: los punteros se INVALIDAN al hacer push
let mut v = vec![1, 2, 3];
let ptr = &v[0] as *const i32;
v.push(4);   // posible realloc → ptr ahora es dangling

// Lista: los nodos NUNCA se mueven
let mut list = LinkedList::new();
list.push_back(1);
// El nodo con 1 permanece en la misma dirección de memoria
// para siempre (hasta que se elimina)
```

Caso práctico: un hash map que almacena punteros a nodos de una lista
(LRU cache).  Si los nodos se movieran (como en Vec), todos los punteros
del hash map se invalidarían en cada modificación.
</details>

### Ejercicio 8 — Variante óptima por operación

Para cada operación, ¿cuál es la variante de lista (ignorando arrays) que
la realiza con menor complejidad?
1. pop_back
2. Rotación derecha
3. Eliminar nodo dado puntero
4. Concatenar

<details>
<summary>Respuestas</summary>

1. **pop_back $O(1)$** → Doble (lineal o circular).  La simple necesita
   el penúltimo ($O(n)$).

2. **rotate_right $O(1)$** → **Circular doble** (única variante).
   Circular simple: $O(n)$ (no hay prev).
   Doble lineal: necesita mover head y tail ($O(1)$ pero no es rotación
   natural).

3. **remove_node $O(1)$** → Doble (lineal o circular).  La simple necesita
   encontrar el anterior ($O(n)$).

4. **Concatenar $O(1)$** → Cualquier variante con puntero a tail.
   La circular es la más elegante (reconectar el ciclo).
</details>

### Ejercicio 9 — Diseño de LRU cache

Un LRU cache necesita:
- `get(key)`: $O(1)$.
- `put(key, value)`: $O(1)$.
- Evicción del least recently used: $O(1)$.

Diseña la estructura combinando un hash map con una de las variantes de
lista.  ¿Cuál variante y por qué?

<details>
<summary>Diseño</summary>

**Lista doble lineal** + `HashMap<Key, *mut DNode>`.

```
HashMap: key → puntero al nodo en la lista
Lista doble: MRU (head) ⟷ ... ⟷ LRU (tail)
```

Operaciones:
- `get(key)`: HashMap busca en $O(1)$ → obtiene puntero al nodo →
  `remove_node` $O(1)$ + `push_front` $O(1)$ (mover al frente).
- `put(key, value)`: `push_front` $O(1)$ + HashMap insert $O(1)$.
  Si lleno: `pop_back` $O(1)$ (evictar LRU) + HashMap remove $O(1)$.

¿Por qué doble y no simple?  `remove_node` requiere $O(1)$ → necesita
`prev`.  ¿Por qué no circular?  No hay beneficio — no necesitamos rotación
ni recorrido cíclico.
</details>

### Ejercicio 10 — Argumento para cada estructura

Para cada una de las 6 estructuras (4 listas + Vec + VecDeque), escribe
**un** escenario concreto donde esa estructura es la mejor opción y
ninguna de las otras la iguala.

<details>
<summary>Escenarios</summary>

1. **Vec**: ranking de jugadores consultado por posición —
   acceso por índice $O(1)$, ninguna lista puede.

2. **VecDeque**: cola de mensajes con push_back/pop_front de alta
   frecuencia — $O(1)$ amortizado con localidad de caché excelente.

3. **Simple lineal**: stack para evaluación de expresiones — solo
   push_front/pop_front, mínima memoria (un puntero por nodo), más
   simple que doble.

4. **Simple circular**: scheduler round-robin — recorrido cíclico
   infinito sin condición de reset, rotación $O(1)$.

5. **Doble lineal**: LRU cache — eliminar nodo por puntero $O(1)$ con
   `prev`, pop_back $O(1)$ para evicción, sin necesidad de rotación.

6. **Doble circular**: buffer de undo/redo con límite fijo —
   rotación bidireccional $O(1)$, eliminar el más antiguo (tail) $O(1)$
   cuando se alcanza el límite, sentinela único elimina todas las ramas.
</details>
