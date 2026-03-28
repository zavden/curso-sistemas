# B15 — Estructuras de Datos en C y Rust

## Análisis de dependencias

### Prerequisitos: B01–B05

| Bloque | Aporta a B15 |
|--------|-------------|
| B01 (Docker) | Entorno de laboratorio con gcc, valgrind, gdb, cargo |
| B02 (Linux) | Shell, filesystem, procesos básicos |
| B03 (C) | Punteros, malloc/free/realloc, structs, punteros a función, void*, macros, bibliotecas |
| B04 (Build) | Make para gestionar proyectos multi-archivo |
| B05 (Rust) | Ownership, generics, traits, Box/Rc/RefCell, Vec, HashMap, iteradores, closures |

### ¿Se necesita B06 o B07?

**No.** Los bloques 6 (Sistemas en C) y 7 (Sistemas en Rust) cubren syscalls,
procesos, señales, IPC, hilos, sockets, async y FFI — ninguno es prerequisito
para estructuras de datos. Los dos temas limítrofes se resuelven así:

| Tema | Dónde está | Solución en B15 |
|------|-----------|-----------------|
| Valgrind/GDB básico | B03 C01 S02 | Suficiente. C01 de B15 añade patrones específicos para DS |
| unsafe Rust (raw pointers) | B07 C06 | Autocontenido en C01 S04 de B15 (mini-módulo de 4 tópicos) |
| Estructuras concurrentes | Apéndice del roadmap | Se mantiene como apéndice opcional; requiere B06 C05 / B07 C01 |

**Conclusión**: B01–B05 es suficiente. Se añade un mini-módulo de `unsafe` Rust
dentro de C01 para cubrir el único gap real.

---

## Filosofía C vs Rust en este bloque

```
C:    Aprendes cómo funciona la máquina.
      Punteros, malloc, free — ves cada byte, cada indirección.
      El error te enseña: dangling pointer, double free, buffer overflow.
      Ideal para entender la estructura de datos "desde abajo".

Rust: Aprendes cómo codificar la estructura sin errores.
      Ownership, generics, traits — el compilador rechaza lo incorrecto.
      El tipo te enseña: Option<Box<Node<T>>>, &T vs &mut T.
      Ideal para implementar estructuras que otros van a usar.

Cada estructura se implementa primero en C (para entender el mecanismo)
y luego en Rust (para entender la abstracción segura).
Se discute explícitamente cuándo conviene cada lenguaje.
```

---

## Estructura del bloque

```
C01  Fundamentos transversales
C02  La pila
C03  Recursión
C04  Colas
C05  Listas enlazadas
C06  Árboles
C07  Montículos y colas de prioridad
C08  Ordenamiento
C09  Búsqueda
C10  Tablas hash (dispersión)
C11  Grafos
C12  Administración de almacenamiento
```

---

## C01 — Fundamentos transversales

### S01 — Tipos abstractos de datos (TAD)

- **T01 — Definición y contrato de un TAD**: qué es un TAD, separación de interfaz e implementación, invariantes, precondiciones, postcondiciones. Ejemplo: TAD "Conjunto" definido por operaciones (insertar, eliminar, pertenece) sin fijar la representación.
- **T02 — TADs en C**: headers `.h` como interfaz pública, archivos `.c` como implementación privada, structs opacos (`struct Set;` en el header, definición completa solo en el `.c`), punteros opacos como encapsulación. Patrón `set_create()` / `set_destroy()`.
- **T03 — TADs en Rust**: traits como contrato, structs con campos privados, `impl` blocks, visibilidad `pub`/`pub(crate)`. Ejemplo: `trait Set<T> { fn insert(&mut self, val: T); fn contains(&self, val: &T) -> bool; }`.
- **T04 — Genericidad**: `void *` + callbacks en C vs genéricos `<T>` + trait bounds en Rust. Tradeoffs: type-safety, ergonomía, rendimiento. `qsort` en C como ejemplo de genericidad con `void *`.

**Ejercicios**: Diseñar el TAD "Bolsa" (multiset) — definir operaciones, implementar interfaz opaca en C y trait en Rust. Comparar ergonomía.

### S02 — Complejidad algorítmica

- **T01 — Notación O, Ω, Θ**: definición formal, interpretación intuitiva, ejemplos con loops anidados, logaritmos. Reglas de simplificación (constantes, término dominante).
- **T02 — Análisis de mejor, peor y caso promedio**: por qué el peor caso importa para garantías, por qué el caso promedio importa para rendimiento real. Ejemplo: búsqueda lineal (mejor O(1), peor O(n), promedio O(n/2) = O(n)).
- **T03 — Análisis amortizado**: concepto (costo promedio por operación en una secuencia), método del banquero, método del potencial. Ejemplo canónico: `push` en vector dinámico con duplicación de capacidad → O(1) amortizado.
- **T04 — Complejidad espacial**: memoria extra vs memoria total, espacio in-place (O(1) extra), espacio O(n), relación tiempo-espacio (tradeoffs).
- **T05 — Clases de complejidad comunes**: O(1), O(log n), O(n), O(n log n), O(n²), O(2ⁿ), O(n!) — tabla con ejemplos de algoritmos de cada clase, gráfica mental de crecimiento.

**Ejercicios**: Analizar complejidad de fragmentos de código. Determinar O de loops anidados con incrementos no unitarios. Comparar experimentalmente tiempos de ejecución de O(n) vs O(n²) para n = 10³, 10⁴, 10⁵, 10⁶.

### S03 — Herramientas para estructuras dinámicas

- **T01 — Valgrind para estructuras de datos**: patrones de leak en listas/árboles (liberar nodos), detección de use-after-free en destrucción de estructuras enlazadas, `--track-origins=yes` para variables no inicializadas.
- **T02 — GDB para punteros y estructuras**: inspeccionar campos de nodos con `p node->next->data`, watchpoints en campos de struct, `display` para seguir un puntero, recorrer listas con scripts GDB.
- **T03 — AddressSanitizer y Miri**: `-fsanitize=address` en C para detección en tiempo real, `cargo miri test` en Rust para detectar UB en código unsafe. Cuándo usar cada herramienta.
- **T04 — Testing de estructuras**: tests unitarios para operaciones del TAD, tests de estrés (miles de inserciones/eliminaciones aleatorias), verificación de invariantes internos, property-based testing en Rust (proptest/quickcheck).

**Ejercicios**: Introducir bugs deliberados (double free, use-after-free, leak) en una lista enlazada C y detectarlos con Valgrind. Hacer lo mismo en Rust unsafe y detectar con Miri.

### S04 — unsafe Rust para estructuras de datos

> Mini-módulo autocontenido. No requiere B07.

- **T01 — Raw pointers**: `*const T` y `*mut T`, creación segura (`as *const T`), desreferencia unsafe, `std::ptr::null/null_mut`, aritmética de punteros con `.add()/.offset()`. Diferencias con referencias: no borrow checker, pueden ser null, pueden aliasear.
- **T02 — Bloques unsafe y safety invariants**: qué garantiza el programador (no el compilador), documentar invariantes con `// SAFETY:` comments, minimizar el scope de `unsafe {}`, exponer API safe sobre implementación unsafe.
- **T03 — NonNull\<T\> y Box::into_raw/from_raw**: `NonNull<T>` como puntero no-nulo (invariante de tipo), convertir entre `Box<T>` y raw pointers para ownership manual, patrón para listas enlazadas con raw pointers.
- **T04 — Cuándo usar unsafe vs abstracciones safe**: árbol de decisión — ¿se puede hacer con `Box<Option<Node<T>>>`? → safe. ¿Se necesitan punteros al padre, nodos compartidos? → unsafe con raw pointers. ¿Se necesita aliasing mutable (grafo)? → `Rc<RefCell<T>>` o unsafe. Benchmark: overhead de `Rc<RefCell>` vs raw pointers.

**Ejercicios**: Implementar un stack simple con raw pointers y `unsafe`. Comparar con la versión safe usando `Vec<T>`. Verificar con `cargo miri test`.

---

## C02 — La pila (Stack)

### S01 — Concepto y operaciones

- **T01 — La pila como TAD**: LIFO (Last In, First Out), operaciones primitivas (`push`, `pop`, `peek`/`top`, `is_empty`, `size`), condiciones excepcionales (pila vacía, pila llena en implementación con array).
- **T02 — Implementación con array en C**: array estático + índice `top`, crecimiento (stack overflow), verificación de límites. Struct `Stack` con `data[]`, `top`, `capacity`.
- **T03 — Implementación con lista enlazada en C**: nodo con `data` + `next`, push = insertar al inicio, pop = eliminar del inicio. Ventaja: sin límite fijo. Desventaja: overhead por nodo (puntero extra), cache-unfriendly.
- **T04 — Implementación en Rust**: con `Vec<T>` (push/pop son O(1) amortizado, idéntico a array dinámico), con lista enlazada genérica (`Option<Box<Node<T>>>`). Comparación de ergonomía y seguridad vs C.

**Ejercicios**: Implementar stack genérico con `void *` en C y con `<T>` en Rust. Test de push 1000 elementos y pop todos. Verificar con Valgrind (C) y tests (Rust).

### S02 — Aplicaciones de la pila

- **T01 — Verificación de paréntesis balanceados**: algoritmo con pila, extensión a múltiples tipos de delimitadores (`()`, `[]`, `{}`). Implementación en C y Rust.
- **T02 — Notación posfija (RPN)**: qué es, por qué es útil (no necesita precedencia ni paréntesis), evaluación con pila.
- **T03 — Conversión infija a posfija**: algoritmo de Shunting-Yard (Dijkstra), manejo de precedencia y asociatividad, manejo de paréntesis.
- **T04 — Evaluación de expresiones posfijas con pila**: implementación completa en C (operandos numéricos) y Rust (con enums para tokens).
- **T05 — Otras aplicaciones**: historial de navegación (back/forward con dos pilas), undo/redo, verificación de palíndromos.

**Ejercicios**: Calculadora RPN completa en C y Rust. Verificador de sintaxis de delimitadores para archivos de código fuente. Convertir expresiones infijas a posfijas con soporte de `+`, `-`, `*`, `/`, `^`, `(`, `)`.

---

## C03 — Recursión

### S01 — Fundamentos

- **T01 — Definiciones recursivas**: caso base y caso recursivo, inducción, factorial, Fibonacci. Cómo la máquina ejecuta recursión (call stack, stack frames).
- **T02 — Pila de llamadas y memoria**: visualización del call stack con GDB (`backtrace`), stack overflow por recursión excesiva, `ulimit -s` para ver/ajustar tamaño de stack. Relación con la pila como estructura de datos.
- **T03 — Recursión vs iteración**: toda recursión se puede convertir a iteración con pila explícita. Cuándo preferir recursión (código más claro: árboles, divide-and-conquer) vs iteración (rendimiento, evitar stack overflow).
- **T04 — Recursión de cola (tail recursion)**: definición, optimización del compilador (`-O2` en gcc, TCO), `loop` en Rust como reemplazo idiomático. Por qué C y Rust no garantizan TCO (a diferencia de Scheme).

**Ejercicios**: Implementar factorial y Fibonacci recursivo e iterativo. Medir stack depth con GDB para fibonacci(40). Convertir recursión a iteración con pila explícita.

### S02 — Recursión aplicada

- **T01 — Búsqueda binaria recursiva**: implementación, comparación con versión iterativa, análisis O(log n).
- **T02 — Torres de Hanoi**: solución recursiva clásica, conteo de movimientos (2ⁿ - 1), implementación en C y Rust.
- **T03 — Recursión mutua**: funciones que se llaman entre sí, ejemplo `is_even`/`is_odd`, forward declarations en C, necesidad de prototipos.
- **T04 — Backtracking**: resolver problemas por prueba y retroceso (N-reinas, laberintos), la recursión como mecanismo natural para explorar espacios de soluciones.
- **T05 — Divide y vencerás (preview)**: concepto general, cómo merge sort y quicksort lo usan (se profundiza en C08).

**Ejercicios**: N-reinas para N=4,8 en C y Rust. Resolver un Sudoku por backtracking. Implementar merge sort recursivo (preview del capítulo de ordenamiento).

---

## C04 — Colas (Queue)

### S01 — La cola y sus variantes

- **T01 — La cola como TAD**: FIFO (First In, First Out), operaciones (`enqueue`, `dequeue`, `front`, `is_empty`, `size`), condiciones excepcionales.
- **T02 — Implementación con array circular en C**: índices `front` y `rear`, lógica de wrap-around con módulo, distinción entre cola vacía y llena (con contador o slot vacío).
- **T03 — Implementación con lista enlazada en C**: punteros `front` y `rear`, enqueue al final, dequeue del inicio. Cola con nodo sentinela (dummy head).
- **T04 — Implementación en Rust**: `VecDeque<T>` de la biblioteca estándar (buffer circular), implementación manual con `Vec<T>` y con lista enlazada.

**Ejercicios**: Implementar cola circular con array en C. Comparar rendimiento array vs lista para 10⁶ operaciones. Implementar cola en Rust con y sin VecDeque.

### S02 — Colas especiales

- **T01 — Cola de prioridad**: concepto, implementación ingenua con array ordenado/desordenado (O(n) inserción o extracción). Preview: implementación eficiente con heap (C07).
- **T02 — Deque (cola de doble extremo)**: operaciones en ambos extremos, implementación con array circular y con lista doblemente enlazada. `VecDeque<T>` en Rust.
- **T03 — Cola de prioridad con lista enlazada**: inserción ordenada O(n), extracción O(1). Implementación en C.
- **T04 — Simulación con colas**: modelar un sistema de atención (cajeros, servidores), llegadas con distribución aleatoria, métricas (tiempo de espera promedio, utilización).

**Ejercicios**: Simulación de cola de supermercado (múltiples cajeros) en C. Implementar deque genérico en Rust. Comparar experimentalmente O de cola de prioridad con array vs lista.

---

## C05 — Listas enlazadas

### S01 — Lista simplemente enlazada

- **T01 — Nodos y punteros**: struct `Node` con `data` y `next`, lista como puntero al primer nodo, lista vacía = `NULL`. Visualización de la estructura en memoria.
- **T02 — Operaciones fundamentales en C**: insertar al inicio, insertar al final, insertar en posición, buscar, eliminar por valor, eliminar por posición. Uso de puntero a puntero (`Node **`) para modificar el head.
- **T03 — Recorrido e impresión**: iterar con `while (current != NULL)`, contar elementos, buscar elemento, aplicar función a cada nodo (callback con puntero a función).
- **T04 — Implementación en Rust (safe)**: `Option<Box<Node<T>>>`, por qué las listas enlazadas son incómodas en Rust (ownership lineal vs grafos), `enum List { Nil, Cons(T, Box<List>) }` estilo funcional.
- **T05 — Implementación en Rust (unsafe)**: raw pointers `*mut Node<T>`, por qué a veces es necesario (rendimiento, flexibilidad). Comparación de ergonomía y seguridad.

**Ejercicios**: Lista enlazada completa en C con 8+ operaciones. Lista funcional en Rust con enum + pattern matching. Benchmark: inserción al inicio ×10⁶ en lista C, lista Rust safe, lista Rust unsafe, Vec::insert(0).

### S02 — Lista doblemente enlazada

- **T01 — Nodos con doble enlace**: `prev` y `next`, ventajas (recorrido bidireccional, eliminación O(1) con puntero al nodo), desventajas (doble puntero por nodo, complejidad de inserción/eliminación).
- **T02 — Operaciones en C**: insertar, eliminar, recorrer en ambas direcciones. Nodo centinela (dummy head y dummy tail) para simplificar casos borde.
- **T03 — Implementación en Rust**: por qué las listas dobles son difíciles en safe Rust (un nodo tiene dos owners), soluciones: `Rc<RefCell<Node<T>>>` (overhead de runtime), raw pointers con unsafe, o usar `LinkedList<T>` de la stdlib.
- **T04 — C vs Rust para listas dobles**: C con raw pointers es natural y eficiente; Rust safe requiere contorsiones. Este es el caso clásico donde C es más ergonómico. Cuándo aceptar unsafe en Rust.

**Ejercicios**: Lista doble con sentinelas en C. Implementar `Rc<RefCell>` versión en Rust y medir overhead vs raw pointers. Implementar un editor de texto simple (buffer de líneas como lista doble).

### S03 — Listas circulares y variantes

- **T01 — Lista circular simple**: el último nodo apunta al primero, recorrido con condición `do...while`, ventajas para rotación y scheduling.
- **T02 — El problema de Josephus**: formulación, solución con lista circular, implementación en C y Rust.
- **T03 — Lista circular doble**: combina las ventajas de circular y doble. Inserción/eliminación. Patrón del kernel Linux (`list_head` intrusivo).
- **T04 — Listas no homogéneas en C**: `void *` para datos genéricos, tagged unions para múltiples tipos en la misma lista. En Rust: `enum` con variantes o `Box<dyn Any>`.
- **T05 — Comparación de todas las variantes**: tabla de complejidades (insertar inicio/final/medio, eliminar, buscar, espacio) para simple, doble, circular, array.

**Ejercicios**: Problema de Josephus en C y Rust. Implementar `list_head` intrusivo simplificado en C. Comparativa de rendimiento: array vs lista simple vs lista doble para inserción/eliminación en el medio.

---

## C06 — Árboles

### S01 — Árboles binarios

- **T01 — Definición y terminología**: raíz, nodos, hojas, padre, hijo, hermano, subárbol, profundidad, altura, nivel. Propiedades: un árbol binario de altura h tiene máximo 2^(h+1) - 1 nodos.
- **T02 — Representación con punteros en C**: struct `TreeNode` con `data`, `left`, `right`. Creación de nodos, árbol vacío = `NULL`.
- **T03 — Representación con array implícito**: hijo izquierdo en `2i+1`, derecho en `2i+2`, padre en `(i-1)/2`. Eficiente para árboles completos (heaps), desperdicio de espacio para árboles desbalanceados.
- **T04 — Representación en Rust**: `Option<Box<TreeNode<T>>>` para hijos, `enum Tree<T> { Leaf, Node(T, Box<Tree<T>>, Box<Tree<T>>) }`. Comparación de ambos estilos.

**Ejercicios**: Construir un árbol binario manualmente en C y Rust. Contar nodos, hojas, calcular altura. Visualizar el árbol en consola (indentado o con arte ASCII).

### S02 — Recorridos

- **T01 — Recorridos recursivos**: inorden (izquierda-raíz-derecha), preorden (raíz-izquierda-derecha), postorden (izquierda-derecha-raíz). Implementación en C y Rust.
- **T02 — Recorrido por niveles (BFS)**: usando cola, nivel por nivel de izquierda a derecha. Implementación en C (con la cola del C04) y Rust (con `VecDeque`).
- **T03 — Recorridos iterativos**: inorden iterativo con pila explícita (Morris traversal como bonus), preorden iterativo. Por qué el iterativo evita stack overflow en árboles muy profundos.
- **T04 — Aplicaciones de recorridos**: serialización/deserialización de árboles, copia de árboles, comparación de igualdad, liberación de memoria (postorden).

**Ejercicios**: Implementar los 4 recorridos en C y Rust. Serializar un árbol a string con preorden y reconstruirlo. Verificar que postorden libera toda la memoria (Valgrind).

### S03 — Árbol binario de búsqueda (ABB/BST)

- **T01 — Definición y propiedad de orden**: para cada nodo, todos los valores del subárbol izquierdo son menores y todos los del derecho son mayores. Búsqueda O(log n) en caso balanceado, O(n) en caso degenerado.
- **T02 — Inserción**: buscar posición correcta, insertar como hoja. Implementación recursiva e iterativa en C.
- **T03 — Búsqueda**: recursiva e iterativa, mínimo, máximo, sucesor, predecesor.
- **T04 — Eliminación**: tres casos (hoja, un hijo, dos hijos). Reemplazo con sucesor inorden o predecesor inorden. Implementación en C.
- **T05 — BST en Rust**: implementación con `Option<Box<Node<T>>>` donde `T: Ord`, aprovechando `match` para los tres casos de eliminación.
- **T06 — Degeneración y motivación para balanceo**: insertar secuencia ordenada → lista enlazada, O(n) en todas las operaciones. Necesidad de árboles balanceados.

**Ejercicios**: BST completo en C (insertar, buscar, eliminar, mín, máx, inorden). BST genérico en Rust con `T: Ord`. Insertar 1..n en orden y verificar que la altura es n (degenerado). Insertar en orden aleatorio y verificar que la altura es ~log₂(n).

### S04 — Árboles balanceados

- **T01 — Rotaciones**: rotación simple (izquierda, derecha), rotación doble (izquierda-derecha, derecha-izquierda). Visualización paso a paso. Implementación en C.
- **T02 — Árboles AVL**: factor de balance (altura izq - altura der ∈ {-1, 0, 1}), inserción con rebalanceo, cuándo aplicar cada tipo de rotación. Complejidad O(log n) garantizada.
- **T03 — Árboles rojinegros**: 5 propiedades, coloreo, inserción con recoloreo y rotaciones. Por qué los usan los sistemas operativos (Linux kernel, Java TreeMap). Implementación simplificada.
- **T04 — AVL vs rojinegro**: AVL → búsquedas más rápidas (más balanceado), RB → inserciones/eliminaciones más rápidas (menos rotaciones). Tabla comparativa.
- **T05 — Implementación en Rust**: AVL tree genérico. Uso de `BTreeMap<K,V>` de la stdlib (B-tree, no AVL ni RB) como alternativa práctica.

**Ejercicios**: Implementar AVL con inserción y rotaciones en C. Verificar invariante de balance después de cada inserción. Comparar tiempos de búsqueda: BST degenerado vs AVL vs `BTreeMap` de Rust para n = 10⁵.

### S05 — Árboles generales y aplicaciones

- **T01 — Árboles generales (n-arios)**: representación hijo-izquierdo/hermano-derecho (transformación a árbol binario), representación con array de hijos.
- **T02 — Árboles de expresión**: construir árbol a partir de expresión infija/posfija, evaluar recursivamente, imprimir en notación infija con paréntesis.
- **T03 — Árboles de juego y minimax**: representación de estados de juego como árbol, algoritmo minimax, poda alfa-beta. Ejemplo: tres en raya (tic-tac-toe).
- **T04 — Tries (árboles digitales)**: estructura para búsqueda de strings por prefijo, inserción, búsqueda, autocompletado. Implementación en C y Rust.

**Ejercicios**: Construir y evaluar un árbol de expresión para `(3 + 4) * (5 - 2)`. Implementar minimax para tres en raya. Implementar un trie para autocompletado de palabras.

---

## C07 — Montículos y colas de prioridad (Heaps)

### S01 — El montículo binario

- **T01 — Definición**: árbol binario completo con propiedad de heap (min-heap o max-heap), representación con array implícito. Relación padre-hijo con índices.
- **T02 — Inserción (sift-up)**: agregar al final, subir mientras sea mayor/menor que el padre. O(log n).
- **T03 — Extracción del mínimo/máximo (sift-down)**: reemplazar raíz con último elemento, bajar. O(log n).
- **T04 — Construcción de heap (heapify)**: bottom-up en O(n) vs top-down en O(n log n). Por qué bottom-up es lineal.
- **T05 — Implementación en C**: array dinámico + funciones `heap_push`, `heap_pop`, `heap_peek`. Heap genérico con comparador (`int (*cmp)(const void*, const void*)`).
- **T06 — Implementación en Rust**: `BinaryHeap<T>` de la stdlib (max-heap), `std::cmp::Reverse<T>` para min-heap, implementación manual.

**Ejercicios**: Implementar min-heap y max-heap en C. Implementar cola de prioridad con heap. Encontrar los k elementos más grandes de un array de n elementos con heap en O(n log k). Comparar rendimiento: cola de prioridad con lista ordenada vs heap para 10⁵ operaciones.

### S02 — Aplicaciones del heap

- **T01 — Heapsort**: construir max-heap in-place, extraer máximo repetidamente. O(n log n) garantizado, in-place, no estable. Implementación en C.
- **T02 — Cola de prioridad como TAD**: interfaz (insert, extract_min, decrease_key), aplicaciones (scheduling, Dijkstra, Huffman).
- **T03 — Heap en algoritmos de grafos (preview)**: por qué Dijkstra necesita decrease_key, por qué Prim necesita extract_min. Se profundiza en C11.

**Ejercicios**: Heapsort in-place en C. Simular un scheduler con cola de prioridad (procesos con prioridad y tiempo de ejecución). Codificación de Huffman para compresión de texto (usa cola de prioridad de árboles).

---

## C08 — Ordenamiento

### S01 — Fundamentos de ordenamiento

- **T01 — Terminología**: estabilidad (preservar orden relativo de iguales), in-place (O(1) espacio extra), adaptativo (más rápido si casi-ordenado), interno vs externo.
- **T02 — Límite inferior para comparaciones**: Ω(n log n) para ordenamientos basados en comparación. Demostración por árbol de decisión.
- **T03 — Cómo medir rendimiento**: comparaciones, movimientos/swaps, espacio auxiliar, cache-friendliness. Herramientas: `clock()` en C, `std::time::Instant` en Rust.

### S02 — Ordenamientos cuadráticos O(n²)

- **T01 — Bubble sort**: algoritmo, optimización con bandera de "ya ordenado", análisis. Implementación en C.
- **T02 — Selection sort**: encontrar mínimo, intercambiar con posición actual. Siempre O(n²) comparaciones, O(n) swaps. Implementación en C.
- **T03 — Insertion sort**: insertar cada elemento en su posición correcta en la parte ya ordenada. Adaptativo: O(n) si casi-ordenado. Implementación en C y Rust.
- **T04 — Cuándo usar O(n²)**: arrays pequeños (n < ~40-60), insertion sort como base para Timsort/introsort, estabilidad de insertion sort.

**Ejercicios**: Implementar los tres en C con contadores de comparaciones y swaps. Verificar experimentalmente que insertion sort es O(n) para arrays casi-ordenados. Comparar tiempos para n = 10², 10³, 10⁴.

### S03 — Ordenamientos eficientes O(n log n)

- **T01 — Merge sort**: divide-and-conquer, merge de dos mitades ordenadas, O(n log n) siempre, estable, pero O(n) espacio extra. Implementación recursiva en C y Rust.
- **T02 — Quicksort**: partición con pivote, recursión en las dos mitades. O(n log n) promedio, O(n²) peor caso. Elección de pivote: primero, último, mediana de tres, aleatorio. Implementación en C.
- **T03 — Quicksort optimizaciones**: cutoff a insertion sort para subarrays pequeños, partición de tres vías (Dutch National Flag) para elementos duplicados, quicksort aleatorizado.
- **T04 — Heapsort (referencia a C07)**: O(n log n) garantizado, in-place, no estable. Cuándo elegir heapsort vs quicksort vs merge sort.
- **T05 — Shell sort**: generalización de insertion sort con gaps decrecientes, secuencias de gaps (Shell, Knuth, Sedgewick), complejidad depende de la secuencia de gaps.
- **T06 — Comparación de algoritmos O(n log n)**: tabla con estabilidad, espacio, peor caso, rendimiento práctico, cache-friendliness.

**Ejercicios**: Merge sort en C (recursivo) y Rust. Quicksort con 3 estrategias de pivote, comparar rendimiento. Comparar merge sort vs quicksort vs heapsort para n = 10⁵ con datos aleatorios, ordenados, y casi-ordenados.

### S04 — Ordenamientos lineales y funciones de biblioteca

- **T01 — Counting sort**: cuando los valores están en un rango conocido [0, k]. O(n + k). Estable.
- **T02 — Radix sort**: ordenar por dígitos (LSD o MSD), usa counting sort como subrutina. O(d·(n + k)) donde d = dígitos.
- **T03 — `qsort` en C**: firma, comparador, casting de `void *`, ejemplos con diferentes tipos. Por qué se llama "qsort" pero no garantiza quicksort.
- **T04 — Ordenamiento en Rust**: `slice::sort` (estable, Timsort adaptativo), `slice::sort_unstable` (no estable, pattern-defeating quicksort, más rápido), `sort_by`, `sort_by_key`. Closures como comparadores.
- **T05 — C vs Rust para ordenamiento**: `qsort` usa `void *` y callbacks (indirección, no inline), Rust usa generics y closures (monomorphization, inline). Benchmark: `qsort` vs `.sort()` para 10⁶ elementos.

**Ejercicios**: Radix sort para enteros en C. Comparar `qsort` (C) vs `.sort_unstable()` (Rust) en rendimiento puro. Ordenar structs por múltiples campos (primario y secundario).

---

## C09 — Búsqueda

### S01 — Búsqueda en secuencias

- **T01 — Búsqueda secuencial**: O(n), optimizaciones (sentinel, move-to-front, transposition). Implementación en C.
- **T02 — Búsqueda binaria**: O(log n), requisito de array ordenado, implementación iterativa y recursiva. Variantes: `lower_bound`, `upper_bound`. Errores comunes (overflow en `(lo + hi) / 2`).
- **T03 — Búsqueda por interpolación**: estimar posición basándose en el valor buscado (como buscar en un diccionario). O(log log n) para distribuciones uniformes, O(n) peor caso.
- **T04 — Búsqueda en Rust**: `slice::binary_search`, `slice::contains`, `Iterator::find`, `Iterator::position`. Retorno de `Result<usize, usize>` en binary_search.

**Ejercicios**: Búsqueda binaria iterativa en C sin bugs (atención a overflow). Implementar `lower_bound` y `upper_bound` en C. Comparar experimentalmente búsqueda secuencial vs binaria vs interpolación para arrays de 10⁶ elementos.

### S02 — Búsqueda en árboles (referencia a C06)

- **T01 — Búsqueda en BST**: O(log n) promedio, O(n) peor caso. Ya implementado en C06 S03.
- **T02 — Búsqueda en árboles balanceados**: O(log n) garantizado (AVL, RB). Ya implementado en C06 S04.
- **T03 — Árboles de búsqueda multidireccional y B-trees**: nodos con múltiples claves y múltiples hijos, utilidad para almacenamiento en disco (I/O optimizado), inserción con split, eliminación con merge/redistribución.
- **T04 — B+ trees**: datos solo en hojas, hojas enlazadas para recorrido secuencial, uso en bases de datos y filesystems.
- **T05 — Tries revisitado**: búsqueda O(m) donde m = longitud de la clave (independiente de n), prefix matching, autocompletado. Patricia tries (compresión de caminos).

**Ejercicios**: Implementar B-tree de orden 3 en C (inserción y búsqueda). Implementar Patricia trie para un diccionario de palabras. Comparar tiempos: BST vs AVL vs B-tree vs trie para buscar 10⁴ palabras.

---

## C10 — Tablas hash (Dispersión)

### S01 — Fundamentos de hashing

- **T01 — Concepto**: mapear claves a índices, búsqueda O(1) promedio. Función hash, tabla, colisiones.
- **T02 — Funciones hash**: división (`key % m`), multiplicación (método de Knuth), para strings (djb2, FNV-1a, polynomial rolling hash). Propiedades deseables: distribución uniforme, determinismo, eficiencia.
- **T03 — Factor de carga**: α = n/m, relación con rendimiento, cuándo redimensionar (típicamente α > 0.7 para open addressing, α > 1.0 para chaining).
- **T04 — Colisiones**: por qué son inevitables (principio del palomar), estrategias de resolución.

**Ejercicios**: Implementar función hash djb2 para strings. Visualizar distribución de hashes para un diccionario de 10⁴ palabras. Experimentar con diferentes tamaños de tabla y verificar uniformidad.

### S02 — Resolución de colisiones

- **T01 — Encadenamiento separado (chaining)**: cada bucket es una lista enlazada. Inserción O(1), búsqueda O(1 + α) promedio. Implementación en C.
- **T02 — Direccionamiento abierto — sonda lineal**: buscar siguiente slot libre. Clustering primario, rendimiento degrada rápido con α alto.
- **T03 — Direccionamiento abierto — sonda cuadrática y doble hashing**: reducir clustering, cálculo de secuencia de sonda, doble hashing con h₂(k) coprimo con m.
- **T04 — Eliminación en open addressing**: por qué no se puede simplemente marcar como vacío (rompe la cadena de sondeo), tombstones (marcadores "deleted"), rehashing periódico.
- **T05 — Redimensionamiento (rehashing)**: cuándo crecer/shrink, factor de crecimiento (×2), rehash de todos los elementos, costo amortizado O(1).

**Ejercicios**: Implementar hash table con chaining en C (genérica con `void *`). Implementar hash table con sonda lineal. Comparar rendimiento chaining vs lineal vs cuadrática para α = 0.5, 0.7, 0.9, 0.95.

### S03 — Hash tables en la práctica

- **T01 — `HashMap<K,V>` en Rust**: implementación interna (SwissTable/hashbrown), `Hash` trait, `Eq` trait, entry API. Por qué K necesita `Hash + Eq`.
- **T02 — `HashSet<K>` en Rust**: como `HashMap<K, ()>`, operaciones de conjuntos (unión, intersección, diferencia).
- **T03 — Hash tables en C — patrones**: GLib `GHashTable`, uthash (macros para hash table intrusiva), implementación propia vs usar librería.
- **T04 — Hashing perfecto y hashing universal**: hash perfecto (sin colisiones para conjunto fijo de claves), familias universales de funciones hash, hash aleatorizado para resistir ataques DoS.
- **T05 — Cuándo NO usar hash tables**: datos ordenados (usar BST/B-tree), pocos elementos (usar array), claves no hashables, necesidad de recorrido ordenado.

**Ejercicios**: Implementar un diccionario inglés-español con hash table en C. Implementar lo mismo con `HashMap` en Rust. Comparar tiempos de inserción y búsqueda. Implementar un contador de frecuencia de palabras en un archivo de texto.

---

## C11 — Grafos

### S01 — Definición y representación

- **T01 — Terminología**: vértices (nodos), aristas (arcos), grafo dirigido vs no dirigido, grafo ponderado, grado (in-degree, out-degree), camino, ciclo, componente conexo, grafo completo, grafo bipartito.
- **T02 — Matriz de adyacencia**: array 2D, O(1) para verificar arista, O(V²) espacio. Implementación en C. Ventajas para grafos densos.
- **T03 — Lista de adyacencia**: array de listas enlazadas (o arrays dinámicos), O(V + E) espacio. Implementación en C. Ventajas para grafos dispersos.
- **T04 — Representación en Rust**: `Vec<Vec<usize>>` para lista de adyacencia simple, `HashMap<usize, Vec<(usize, Weight)>>` para ponderado, crate `petgraph` como referencia.
- **T05 — Comparación de representaciones**: tabla de complejidades (espacio, verificar arista, listar vecinos, agregar arista), cuándo usar cada una.

**Ejercicios**: Implementar grafo con matriz y con lista de adyacencia en C. Leer un grafo desde archivo (formato de lista de aristas). Implementar en Rust con `Vec<Vec<usize>>`.

### S02 — Recorridos

- **T01 — BFS (Breadth-First Search)**: recorrido por niveles usando cola, árbol BFS, cálculo de distancias (camino más corto en grafos no ponderados). Implementación en C y Rust.
- **T02 — DFS (Depth-First Search)**: recorrido en profundidad usando pila (o recursión), tiempos de descubrimiento y finalización, clasificación de aristas (tree, back, forward, cross).
- **T03 — Aplicaciones de DFS**: detección de ciclos, componentes conexos, ordenamiento topológico (con DFS).
- **T04 — Componentes fuertemente conexos**: algoritmo de Kosaraju o Tarjan. Conceptualización en grafo dirigido.

**Ejercicios**: BFS y DFS en C y Rust para un grafo de 20+ nodos. Detectar ciclos en un grafo dirigido. Encontrar componentes conexos de un grafo no dirigido.

### S03 — Caminos más cortos

- **T01 — Dijkstra**: camino más corto desde una fuente, con cola de prioridad (heap). O((V + E) log V). Restricción: pesos no negativos. Implementación en C usando el heap de C07.
- **T02 — Bellman-Ford**: permite pesos negativos, detecta ciclos negativos. O(V · E). Implementación en C.
- **T03 — Floyd-Warshall**: todos los pares de caminos más cortos. O(V³). Programación dinámica. Implementación en C.
- **T04 — Comparación**: cuándo usar cada algoritmo (tabla con restricciones y complejidades).

**Ejercicios**: Dijkstra para un mapa de ciudades con distancias en C. Bellman-Ford con detección de ciclo negativo. Floyd-Warshall para grafo pequeño (verificar a mano). Implementar Dijkstra en Rust con `BinaryHeap`.

### S04 — Árboles generadores y ordenamiento topológico

- **T01 — Árbol generador mínimo (MST)**: definición, aplicaciones (diseño de redes).
- **T02 — Algoritmo de Kruskal**: ordenar aristas por peso, agregar si no forma ciclo. Union-Find (Disjoint Set Union) como estructura auxiliar. Implementación en C.
- **T03 — Algoritmo de Prim**: crecer el MST desde un vértice, usar cola de prioridad. Implementación en C.
- **T04 — Ordenamiento topológico**: DAGs (grafos acíclicos dirigidos), algoritmo de Kahn (BFS con grado de entrada), DFS con post-order. Aplicaciones: dependencias de tareas, orden de compilación.

**Ejercicios**: MST de Kruskal con Union-Find en C. MST de Prim con heap en C. Ordenamiento topológico de un DAG de dependencias de paquetes. Implementar Union-Find en Rust.

### S05 — Flujo en redes (opcional/avanzado)

- **T01 — Redes de flujo**: fuente, sumidero, capacidad, flujo. Definición de flujo máximo.
- **T02 — Ford-Fulkerson**: caminos aumentantes, BFS (Edmonds-Karp) para encontrar caminos. O(V · E²).
- **T03 — Aplicaciones**: matching bipartito, corte mínimo, asignación de recursos.

**Ejercicios**: Implementar Ford-Fulkerson con BFS en C. Resolver un problema de matching bipartito.

---

## C12 — Administración de almacenamiento

### S01 — Gestión de memoria dinámica

- **T01 — El heap del proceso**: cómo `malloc` gestiona memoria (brk/sbrk, mmap para bloques grandes), free list, metadata por bloque.
- **T02 — Estrategias de asignación**: first fit, best fit, worst fit, next fit. Tradeoffs entre fragmentación y velocidad.
- **T03 — Fragmentación**: externa (huecos entre bloques) vs interna (desperdicio dentro de bloques). Compactación.
- **T04 — Buddy system**: dividir y fusionar bloques de potencias de 2. Implementación simplificada en C.

**Ejercicios**: Implementar un allocator simple con first-fit y free-list en C. Simular fragmentación con secuencias de malloc/free de tamaños variados. Implementar buddy allocator simplificado.

### S02 — Arena allocators y pool allocators

- **T01 — Arena allocator**: asignar linealmente, liberar todo de golpe. Ideal para fases de compilación, parsing, frame de juego. Implementación en C.
- **T02 — Pool allocator**: bloques de tamaño fijo pre-asignados, free-list para reutilización. Ideal para nodos de listas/árboles. Implementación en C.
- **T03 — Allocators en Rust**: el trait `Allocator` (nightly), crates como `bumpalo` (arena), `typed-arena`. `Box::new_in` con allocator custom.

**Ejercicios**: Implementar arena allocator en C. Implementar pool allocator para nodos de árbol binario. Comparar rendimiento: malloc/free individual vs pool para 10⁶ nodos.

### S03 — Garbage collection (conceptual)

- **T01 — Conteo de referencias**: concepto, ciclos como problema. `Rc<T>` y `Weak<T>` en Rust como ejemplo. Implementación manual simplificada en C.
- **T02 — Mark-and-sweep**: fase de marcado (BFS/DFS desde raíces), fase de barrido (liberar no marcados). Concepto, no implementación completa.
- **T03 — Generacional y otros**: concepto de generaciones (jóvenes mueren pronto), stop-the-world vs concurrente. Referencia a GC de Java, Go, Python.
- **T04 — Rust como "GC en compilación"**: ownership = tracking estático de lifetime, Drop = liberación determinista, sin overhead en runtime. Por qué Rust no necesita GC.

**Ejercicios**: Implementar conteo de referencias con detección de ciclos (mark-and-sweep simplificado) en C. Crear un grafo con ciclo usando `Rc` + `Weak` en Rust y verificar que no hay leak.

### S04 — Errores comunes y herramientas

- **T01 — Memory leaks en estructuras de datos**: patrones de leak en listas (olvidar liberar nodos al destruir), árboles (olvidar postorden), grafos (ciclos de ownership).
- **T02 — Use-after-free**: acceder a un nodo después de liberarlo, dangling pointers en iteradores. Prevención en C (set to NULL after free) y en Rust (borrow checker).
- **T03 — Double free**: liberar el mismo nodo dos veces (corrupción del heap). Detección con Valgrind/ASan.
- **T04 — Herramienta integrada**: Valgrind memcheck, ASan, Miri — flujo de trabajo para depurar estructuras de datos. Checklist antes de hacer commit.

**Ejercicios**: Introducir deliberadamente cada tipo de error (leak, use-after-free, double free) en un BST en C. Detectar cada uno con Valgrind. Intentar lo mismo en Rust safe (¿compila?) y unsafe (detectar con Miri).

---

## Apéndice A — Cuándo usar C vs Rust para estructuras de datos

```
┌─────────────────────────────────────┬───────────────┬───────────────┐
│ Escenario                           │ Mejor en C    │ Mejor en Rust │
├─────────────────────────────────────┼───────────────┼───────────────┤
│ Aprender cómo funciona la memoria   │ ✓             │               │
│ Código de producción con seguridad  │               │ ✓             │
│ Embedded / kernel / legacy          │ ✓             │               │
│ Listas doblemente enlazadas         │ ✓ (natural)   │ ~ (incómodo)  │
│ Grafos con punteros cruzados        │ ✓             │ ~ (Rc o unsafe)│
│ Estructuras genéricas reutilizables │               │ ✓ (generics)  │
│ Estructuras concurrentes            │               │ ✓ (Send/Sync) │
│ Interop con sistema existente en C  │ ✓             │               │
│ Allocators custom                   │ ✓ (control)   │ ✓ (trait)     │
│ Rendimiento puro (sorted arrays)    │ empate        │ empate        │
│ Hash tables                         │ ~ (void*)     │ ✓ (HashMap)   │
│ Árboles tipo-seguro con generics    │               │ ✓             │
│ Debugging de corrupción de memoria  │ ✓ (Valgrind)  │ ✓ (Miri)     │
└─────────────────────────────────────┴───────────────┴───────────────┘

Regla general:
  Usa C para aprender los mecanismos internos.
  Usa Rust cuando escribas código que otros van a mantener.
  Usa la stdlib de Rust (Vec, HashMap, BTreeMap, BinaryHeap) cuando
  la estructura estándar sea suficiente — no reimplementes sin razón.
```

## Apéndice B — Temas avanzados opcionales

> Estos temas requieren prerrequisitos adicionales más allá de B01–B05.

### Estructuras de datos concurrentes

> Prerrequisito: B06 C05 (pthreads) o B07 C01 (Rust threads)

- Colas lock-free (CAS, compare-and-swap)
- Concurrent hash maps (segmentación, striped locking)
- Skip lists
- `crossbeam` crate en Rust (colas lock-free, epoch-based reclamation)

### Estructuras de datos persistentes

- Immutable data structures (path copying)
- Persistent trees, persistent stacks
- Referencia a lenguajes funcionales (Clojure, Haskell)

### Estructuras de datos probabilísticas

- Bloom filters
- Count-Min Sketch
- HyperLogLog
- Skip lists como alternativa probabilística a árboles balanceados

### Estructuras para almacenamiento externo

- B-trees y B+ trees para disco (ya introducidos en C09 S02)
- LSM trees (Log-Structured Merge)
- External merge sort (ya introducido en C08)
- Consideraciones de I/O: block size, cache, locality

---

## Resumen de ejercicios por capítulo

| Capítulo | Ejercicios C | Ejercicios Rust | Ejercicios mixtos |
|----------|-------------|----------------|-------------------|
| C01 Fundamentos | 4 | 3 | 3 |
| C02 Pilas | 4 | 3 | 3 |
| C03 Recursión | 5 | 3 | 2 |
| C04 Colas | 4 | 3 | 3 |
| C05 Listas | 6 | 4 | 3 |
| C06 Árboles | 6 | 4 | 4 |
| C07 Heaps | 4 | 2 | 3 |
| C08 Ordenamiento | 5 | 3 | 3 |
| C09 Búsqueda | 5 | 3 | 3 |
| C10 Hash tables | 4 | 3 | 3 |
| C11 Grafos | 6 | 3 | 3 |
| C12 Almacenamiento | 5 | 3 | 2 |
| **Total** | **~58** | **~37** | **~35** |

Cada ejercicio de la tabla se corresponde con los listados al final de cada sección.
Los ejercicios "mixtos" son comparaciones C vs Rust (benchmark, ergonomía, seguridad).

---

## Demostraciones formales — README_EXTRA.md pendientes

Archivos con pruebas matemáticas rigurosas que complementan los README.md existentes.
Marcar con `[x]` conforme se cree cada uno.

### C01 — Fundamentos transversales (2)

- [x] `C01_Fundamentos_transversales/S02_Complejidad_algoritmica/T01_Notacion_O_Omega_Theta/README_EXTRA.md` — Probar con la definición (∃c, n₀) que 3n² + 5n ∈ O(n²) y que n² ∉ O(n); probar Θ como intersección de O y Ω.
- [x] `C01_Fundamentos_transversales/S02_Complejidad_algoritmica/T03_Analisis_amortizado/README_EXTRA.md` — Prueba por método del potencial (Φ = 2n − capacidad) de que la inserción en vector dinámico con duplicación es O(1) amortizado.

### C03 — Recursión (1)

- [x] `C03_Recursion/S01_Fundamentos/T01_Definiciones_recursivas/README_EXTRA.md` — Prueba por inducción fuerte de la correctitud de factorial recursivo y derivación de la fórmula cerrada de Fibonacci vía ecuación característica.

### C06 — Árboles (3)

- [x] `C06_Arboles/S03_Arbol_binario_de_busqueda/T02_Insercion/README_EXTRA.md` — Prueba por invariante de bucle de que la inserción preserva la propiedad BST (∀ nodo: izq < nodo < der).
- [x] `C06_Arboles/S04_Arboles_balanceados/T02_Arboles_AVL/README_EXTRA.md` — Probar que h ≤ 1.44·log₂(n+2) usando árboles de Fibonacci (N(h) ≥ φʰ).
- [x] `C06_Arboles/S04_Arboles_balanceados/T03_Arboles_rojinegros/README_EXTRA.md` — Probar que h ≤ 2·log₂(n+1) usando el argumento de black-height por inducción.

### C07 — Montículos (2)

- [x] `C07_Monticulos_y_colas_de_prioridad/S01_Monticulo_binario/T04_Construccion_heapify/README_EXTRA.md` — Prueba de que build-heap bottom-up es O(n): Σ ⌈n/2^{h+1}⌉·O(h) = O(n·Σ h/2ʰ) = O(n).
- [x] `C07_Monticulos_y_colas_de_prioridad/S02_Aplicaciones_del_heap/T01_Heapsort/README_EXTRA.md` — Prueba de correctitud por invariante de bucle del heapsort.

### C08 — Ordenamiento (4)

- [x] `C08_Ordenamiento/S01_Fundamentos/T02_Limite_inferior/README_EXTRA.md` — Prueba por árbol de decisión: altura ≥ log₂(n!) = Ω(n log n) con aproximación de Stirling.
- [x] `C08_Ordenamiento/S03_Ordenamientos_eficientes/T01_Merge_sort/README_EXTRA.md` — Derivar T(n) = 2T(n/2) + Θ(n) = Θ(n log n) por Teorema Maestro y árbol de recursión.
- [x] `C08_Ordenamiento/S03_Ordenamientos_eficientes/T02_Quicksort/README_EXTRA.md` — Caso promedio: E[comparaciones] = 2n·Hₙ ≈ 1.39·n·log₂ n.
- [x] `C08_Ordenamiento/S03_Ordenamientos_eficientes/T03_Quicksort_optimizaciones/README_EXTRA.md` — Quicksort aleatorizado: complejidad esperada O(n log n) independiente de la entrada.

### C09 — Búsqueda (1)

- [x] `C09_Busqueda/S01_Busqueda_en_secuencias/T02_Busqueda_binaria/README_EXTRA.md` — Correctitud por invariante de bucle y terminación (hi − lo decrece estrictamente).

### C10 — Tablas hash (4)

- [x] `C10_Tablas_hash/S01_Fundamentos_de_hashing/T03_Factor_de_carga/README_EXTRA.md` — E[longitud de cadena] = α por linealidad de la esperanza.
- [x] `C10_Tablas_hash/S02_Resolucion_de_colisiones/T01_Encadenamiento_separado/README_EXTRA.md` — Búsqueda fallida Θ(1 + α), exitosa Θ(1 + α/2).
- [x] `C10_Tablas_hash/S02_Resolucion_de_colisiones/T02_Sonda_lineal/README_EXTRA.md` — Resultado de Knuth: exitosa ½(1 + 1/(1−α)), fallida ½(1 + 1/(1−α)²).
- [x] `C10_Tablas_hash/S02_Resolucion_de_colisiones/T05_Redimensionamiento/README_EXTRA.md` — Inserción con rehash automático es O(1) amortizado por método agregado.

### C11 — Grafos (4)

- [x] `C11_Grafos/S03_Caminos_mas_cortos/T01_Dijkstra/README_EXTRA.md` — Correctitud por contradicción: al extraer u del heap, dist[u] es mínima.
- [x] `C11_Grafos/S03_Caminos_mas_cortos/T02_Bellman_Ford/README_EXTRA.md` — Prueba por inducción: después de i iteraciones, dist[v] óptima con ≤ i aristas.
- [x] `C11_Grafos/S04_Arboles_generadores_y_topologico/T02_Kruskal/README_EXTRA.md` — Propiedad de corte → Kruskal produce un MST.
- [x] `C11_Grafos/S04_Arboles_generadores_y_topologico/T03_Prim/README_EXTRA.md` — Propiedad de corte → Prim produce un MST.
