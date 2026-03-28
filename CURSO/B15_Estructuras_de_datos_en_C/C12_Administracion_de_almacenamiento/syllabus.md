# C12 — Administración de almacenamiento

**Objetivo**: Comprender cómo funciona la gestión de memoria dinámica a bajo nivel
(allocators, fragmentación, buddy system), implementar allocators custom (arena, pool),
entender garbage collection conceptualmente, y dominar la depuración de errores
de memoria en estructuras de datos.

**Prerequisitos**: B03 C07 (malloc/free/realloc), C01 S03 (Valgrind, GDB, Miri),
C05 (listas — usadas internamente en allocators), C06 (árboles — mark-and-sweep
recorre grafos de objetos).

---

## S01 — Gestión de memoria dinámica

| Tópico | Contenido |
|--------|-----------|
| **T01 — El heap del proceso** | Cómo `malloc` gestiona memoria (brk/sbrk, mmap para bloques grandes), free list, metadata por bloque. |
| **T02 — Estrategias de asignación** | First fit, best fit, worst fit, next fit. Tradeoffs fragmentación vs velocidad. |
| **T03 — Fragmentación** | Externa (huecos entre bloques) vs interna (desperdicio dentro). Compactación. |
| **T04 — Buddy system** | Dividir y fusionar bloques de potencias de 2. Implementación simplificada en C. |

**Ejercicios**: Allocator con first-fit y free-list en C. Simular fragmentación con
malloc/free de tamaños variados. Buddy allocator simplificado.

---

## S02 — Arena y pool allocators

| Tópico | Contenido |
|--------|-----------|
| **T01 — Arena allocator** | Asignar linealmente, liberar todo de golpe. Ideal para parsing, frames de juego. Implementación en C. |
| **T02 — Pool allocator** | Bloques de tamaño fijo pre-asignados, free-list para reutilización. Ideal para nodos de lista/árbol. Implementación en C. |
| **T03 — Allocators en Rust** | Trait `Allocator` (nightly), `bumpalo` (arena), `typed-arena`. `Box::new_in` con allocator custom. |

**Ejercicios**: Arena allocator en C. Pool allocator para nodos de BST. Comparar
malloc/free individual vs pool para 10⁶ nodos.

---

## S03 — Garbage collection (conceptual)

| Tópico | Contenido |
|--------|-----------|
| **T01 — Conteo de referencias** | Concepto, ciclos como problema. `Rc<T>` + `Weak<T>` en Rust. Implementación manual simplificada en C. |
| **T02 — Mark-and-sweep** | Marcado (BFS/DFS desde raíces), barrido (liberar no marcados). Concepto, no implementación completa. |
| **T03 — Generacional** | Generaciones (jóvenes mueren pronto), stop-the-world vs concurrente. Referencia a Java, Go, Python. |
| **T04 — Rust como GC en compilación** | Ownership = tracking estático, Drop = liberación determinista, sin overhead runtime. |

**Ejercicios**: Conteo de referencias con detección de ciclos en C. Grafo con ciclo
usando `Rc` + `Weak` en Rust (sin leak).

---

## S04 — Errores y herramientas

| Tópico | Contenido |
|--------|-----------|
| **T01 — Memory leaks** | Patrones en listas (olvidar nodos al destruir), árboles (olvidar postorden), grafos (ciclos). |
| **T02 — Use-after-free** | Acceder a nodo liberado, dangling pointers. Prevención en C (NULL after free) y Rust (borrow checker). |
| **T03 — Double free** | Liberar mismo nodo dos veces (corrupción). Detección con Valgrind/ASan. |
| **T04 — Herramienta integrada** | Valgrind memcheck, ASan, Miri — flujo de trabajo. Checklist antes de commit. |

**Ejercicios**: Introducir leak, use-after-free, double free en BST en C. Detectar
con Valgrind. Intentar en Rust safe (¿compila?) y unsafe (detectar con Miri).
