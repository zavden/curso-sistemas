# C03 — Recursión

**Objetivo**: Comprender la recursión como mecanismo fundamental, su relación con
la pila de llamadas, cuándo preferir recursión vs iteración, y aplicarla a
problemas clásicos (backtracking, divide y vencerás).

**Prerequisitos**: C02 (pilas — la recursión usa el stack implícitamente).

---

## S01 — Fundamentos

| Tópico | Contenido |
|--------|-----------|
| **T01 — Definiciones recursivas** | Caso base y caso recursivo, inducción, factorial, Fibonacci. Cómo la máquina ejecuta recursión (call stack, stack frames). ▸ **Demostración formal**: prueba por inducción fuerte de la correctitud de factorial recursivo (n! = n·(n−1)!) y derivación de la fórmula cerrada de Fibonacci vía ecuación característica. |
| **T02 — Pila de llamadas** | Visualización con GDB (`backtrace`), stack overflow, `ulimit -s`. Relación con la pila como estructura de datos. |
| **T03 — Recursión vs iteración** | Toda recursión → iteración con pila explícita. Cuándo preferir recursión (árboles, divide-and-conquer) vs iteración (rendimiento, stack overflow). |
| **T04 — Recursión de cola** | Definición, optimización del compilador (`-O2` en gcc), `loop` en Rust como reemplazo idiomático. C y Rust no garantizan TCO. |

**Ejercicios**: Factorial y Fibonacci recursivo/iterativo. Medir stack depth con
GDB para fibonacci(40). Convertir recursión a iteración con pila explícita.

---

## S02 — Recursión aplicada

| Tópico | Contenido |
|--------|-----------|
| **T01 — Búsqueda binaria recursiva** | Implementación, comparación con iterativa, O(log n). |
| **T02 — Torres de Hanoi** | Solución recursiva clásica, conteo de movimientos (2ⁿ − 1). Implementación en C y Rust. |
| **T03 — Recursión mutua** | Funciones que se llaman entre sí, `is_even`/`is_odd`, forward declarations en C. |
| **T04 — Backtracking** | Prueba y retroceso (N-reinas, laberintos), recursión como exploración de espacios de soluciones. |
| **T05 — Divide y vencerás** | Concepto general, preview de merge sort y quicksort (se profundiza en C08). |

**Ejercicios**: N-reinas para N=4,8 en C y Rust. Sudoku por backtracking.
Merge sort recursivo (preview de C08).

---

## Demostraciones formales pendientes (README_EXTRA.md)

| Ubicación | Contenido |
|-----------|-----------|
| `S01_Fundamentos/T01_Definiciones_recursivas/README_EXTRA.md` | Prueba por inducción fuerte de la correctitud de factorial recursivo (n! = n·(n−1)!) y derivación de la fórmula cerrada de Fibonacci vía ecuación característica. |
