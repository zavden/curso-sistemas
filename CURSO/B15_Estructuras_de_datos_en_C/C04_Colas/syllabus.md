# C04 — Colas (Queue)

**Objetivo**: Dominar la cola como estructura FIFO, implementarla con array
circular y con lista enlazada, y explorar variantes (deque, cola de prioridad).
Aplicar colas a simulación de sistemas.

**Prerequisitos**: C01 (TADs), C02 (pilas — patrón similar).

---

## S01 — La cola y variantes

| Tópico | Contenido |
|--------|-----------|
| **T01 — La cola como TAD** | FIFO, operaciones (`enqueue`, `dequeue`, `front`, `is_empty`, `size`), condiciones excepcionales. |
| **T02 — Array circular en C** | Índices `front` y `rear`, wrap-around con módulo, distinción vacía/llena (contador o slot vacío). |
| **T03 — Lista enlazada en C** | Punteros `front` y `rear`, enqueue al final, dequeue del inicio. Cola con nodo sentinela. |
| **T04 — Implementación en Rust** | `VecDeque<T>` de la stdlib (buffer circular), implementación manual con `Vec<T>` y con lista. |

**Ejercicios**: Cola circular con array en C. Comparar rendimiento array vs lista
para 10⁶ operaciones. Cola en Rust con y sin `VecDeque`.

---

## S02 — Colas especiales

| Tópico | Contenido |
|--------|-----------|
| **T01 — Cola de prioridad** | Concepto, implementación ingenua con array (O(n) inserción o extracción). Preview: implementación eficiente con heap (C07). |
| **T02 — Deque** | Operaciones en ambos extremos, implementación con array circular y lista doblemente enlazada. `VecDeque<T>` en Rust. |
| **T03 — Cola de prioridad con lista** | Inserción ordenada O(n), extracción O(1). Implementación en C. |
| **T04 — Simulación con colas** | Sistema de atención (cajeros, servidores), llegadas aleatorias, métricas (tiempo de espera, utilización). |

**Ejercicios**: Simulación de cola de supermercado (múltiples cajeros) en C.
Deque genérico en Rust. Comparar O de cola de prioridad con array vs lista.
