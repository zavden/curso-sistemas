# C02 — La pila (Stack)

**Objetivo**: Dominar la pila como estructura de datos LIFO, implementarla con
array y con lista enlazada en C, implementarla en Rust, y aplicarla a problemas
clásicos (evaluación de expresiones, verificación de paréntesis).

**Prerequisitos**: C01 (TADs, complejidad).

---

## S01 — Concepto y operaciones

| Tópico | Contenido |
|--------|-----------|
| **T01 — La pila como TAD** | LIFO, operaciones primitivas (`push`, `pop`, `peek`/`top`, `is_empty`, `size`), condiciones excepcionales (pila vacía, pila llena). |
| **T02 — Implementación con array en C** | Array estático + índice `top`, crecimiento (stack overflow), verificación de límites. Struct `Stack` con `data[]`, `top`, `capacity`. |
| **T03 — Implementación con lista en C** | Nodo con `data` + `next`, push = insertar al inicio, pop = eliminar del inicio. Sin límite fijo, overhead por nodo, cache-unfriendly. |
| **T04 — Implementación en Rust** | Con `Vec<T>` (push/pop O(1) amortizado), con lista genérica `Option<Box<Node<T>>>`. Comparación de ergonomía y seguridad vs C. |

**Ejercicios**: Stack genérico con `void *` en C y con `<T>` en Rust. Test push/pop
de 1000 elementos. Verificar con Valgrind (C) y tests (Rust).

---

## S02 — Aplicaciones de la pila

| Tópico | Contenido |
|--------|-----------|
| **T01 — Paréntesis balanceados** | Algoritmo con pila, extensión a `()`, `[]`, `{}`. Implementación en C y Rust. |
| **T02 — Notación posfija (RPN)** | Qué es, por qué no necesita precedencia ni paréntesis, evaluación con pila. |
| **T03 — Conversión infija a posfija** | Algoritmo de Shunting-Yard (Dijkstra), precedencia, asociatividad, paréntesis. |
| **T04 — Evaluación de expresiones** | Evaluación de expresiones posfijas con pila. Implementación en C y Rust (enums para tokens). |
| **T05 — Otras aplicaciones** | Historial de navegación (back/forward con dos pilas), undo/redo, palíndromos. |

**Ejercicios**: Calculadora RPN completa en C y Rust. Verificador de delimitadores
para archivos de código fuente. Conversor infija → posfija con `+`, `-`, `*`, `/`,
`^`, `(`, `)`.
