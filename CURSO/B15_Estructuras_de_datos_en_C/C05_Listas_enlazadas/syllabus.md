# C05 — Listas enlazadas

**Objetivo**: Dominar las listas enlazadas en todas sus variantes (simple, doble,
circular), implementarlas en C con raw pointers y en Rust (safe y unsafe),
y entender por qué las listas dobles son el caso clásico donde C es más
natural que Rust.

**Prerequisitos**: C01 (TADs, unsafe Rust), C02–C04 (pilas y colas como
contexto de uso).

---

## S01 — Lista simplemente enlazada

| Tópico | Contenido |
|--------|-----------|
| **T01 — Nodos y punteros** | Struct `Node` con `data` y `next`, lista vacía = `NULL`, visualización en memoria. |
| **T02 — Operaciones en C** | Insertar inicio/final/posición, buscar, eliminar por valor/posición. Puntero a puntero (`Node **`) para modificar head. |
| **T03 — Recorrido e impresión** | Iterar con `while (current != NULL)`, contar, buscar, callback con puntero a función. |
| **T04 — Implementación Rust safe** | `Option<Box<Node<T>>>`, `enum List { Nil, Cons(T, Box<List>) }` estilo funcional. Por qué las listas son incómodas en Rust (ownership lineal). |
| **T05 — Implementación Rust unsafe** | Raw pointers `*mut Node<T>`, cuándo es necesario (rendimiento, flexibilidad). |

**Ejercicios**: Lista completa en C (8+ operaciones). Lista funcional en Rust.
Benchmark: inserción al inicio ×10⁶ en lista C, Rust safe, Rust unsafe, `Vec::insert(0)`.

---

## S02 — Lista doblemente enlazada

| Tópico | Contenido |
|--------|-----------|
| **T01 — Nodos con doble enlace** | `prev` y `next`, recorrido bidireccional, eliminación O(1) con puntero al nodo. |
| **T02 — Operaciones en C** | Insertar, eliminar, recorrer en ambas direcciones. Nodo sentinela (dummy head/tail). |
| **T03 — Implementación en Rust** | `Rc<RefCell<Node<T>>>` (overhead runtime), raw pointers unsafe, `LinkedList<T>` de stdlib. |
| **T04 — C vs Rust** | C con raw pointers es natural y eficiente; Rust safe requiere contorsiones. Caso clásico donde C es más ergonómico. |

**Ejercicios**: Lista doble con sentinelas en C. `Rc<RefCell>` en Rust + overhead
vs raw pointers. Editor de texto simple (buffer de líneas como lista doble).

---

## S03 — Listas circulares y variantes

| Tópico | Contenido |
|--------|-----------|
| **T01 — Lista circular simple** | Último nodo apunta al primero, recorrido con `do...while`, ventajas para rotación y scheduling. |
| **T02 — Problema de Josephus** | Formulación, solución con lista circular. Implementación en C y Rust. |
| **T03 — Lista circular doble** | Combina circular y doble. Patrón del kernel Linux (`list_head` intrusivo). |
| **T04 — Listas no homogéneas** | `void *` + tagged unions en C. `enum` con variantes o `Box<dyn Any>` en Rust. |
| **T05 — Comparación de variantes** | Tabla: complejidades de insertar/eliminar/buscar para simple, doble, circular, array. |

**Ejercicios**: Josephus en C y Rust. `list_head` intrusivo simplificado en C.
Comparativa de rendimiento: array vs simple vs doble para inserción/eliminación en medio.
