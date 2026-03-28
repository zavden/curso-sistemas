# C07 — Montículos y colas de prioridad (Heaps)

**Objetivo**: Dominar el montículo binario como estructura basada en array,
implementar una cola de prioridad eficiente, y aplicar heaps a heapsort y
como preview de algoritmos de grafos.

**Prerequisitos**: C06 S01 (árboles binarios — un heap es un árbol binario
completo), C04 S02 (colas de prioridad — implementación ingenua como contraste).

---

## S01 — Montículo binario

| Tópico | Contenido |
|--------|-----------|
| **T01 — Definición** | Árbol binario completo con propiedad de heap (min-heap / max-heap), representación con array implícito, relación padre-hijo con índices. |
| **T02 — Inserción (sift-up)** | Agregar al final, subir mientras mayor/menor que padre. O(log n). |
| **T03 — Extracción (sift-down)** | Reemplazar raíz con último, bajar. O(log n). |
| **T04 — Construcción (heapify)** | Bottom-up O(n) vs top-down O(n log n). Por qué bottom-up es lineal. ▸ **Demostración formal**: prueba de que build-heap bottom-up es O(n) sumando el trabajo por nivel: Σ_{h=0}^{⌊log n⌋} ⌈n/2^{h+1}⌉·O(h) = O(n·Σ h/2ʰ) = O(n), con convergencia de la serie geométrica derivada. |
| **T05 — Implementación en C** | Array dinámico + `heap_push`, `heap_pop`, `heap_peek`. Genérico con comparador `int (*cmp)(const void*, const void*)`. |
| **T06 — Implementación en Rust** | `BinaryHeap<T>` de stdlib (max-heap), `Reverse<T>` para min-heap, implementación manual. |

**Ejercicios**: Min-heap y max-heap en C. Cola de prioridad con heap. Top-k elementos
de un array de n en O(n log k). Comparar cola de prioridad: lista ordenada vs heap
para 10⁵ operaciones.

---

## S02 — Aplicaciones del heap

| Tópico | Contenido |
|--------|-----------|
| **T01 — Heapsort** | Max-heap in-place, extraer máximo repetidamente. O(n log n), in-place, no estable. ▸ **Demostración formal**: prueba de correctitud por invariante de bucle (al inicio de cada iteración, arr[0..i] es un max-heap y arr[i..n] contiene los n−i mayores elementos ordenados). |
| **T02 — Cola de prioridad como TAD** | Interfaz (insert, extract_min, decrease_key), aplicaciones (scheduling, Dijkstra, Huffman). |
| **T03 — Heap en grafos (preview)** | Por qué Dijkstra necesita decrease_key, por qué Prim necesita extract_min. Se profundiza en C11. |

**Ejercicios**: Heapsort in-place en C. Scheduler con cola de prioridad (procesos
con prioridad y tiempo). Codificación de Huffman para compresión de texto.

---

## Demostraciones formales pendientes (README_EXTRA.md)

| Ubicación | Contenido |
|-----------|-----------|
| `S01_Monticulo_binario/T04_Construccion_heapify/README_EXTRA.md` | Prueba de que build-heap bottom-up es O(n) sumando trabajo por nivel: Σ ⌈n/2^{h+1}⌉·O(h) = O(n·Σ h/2ʰ) = O(n), con convergencia de la serie geométrica derivada. |
| `S02_Aplicaciones_del_heap/T01_Heapsort/README_EXTRA.md` | Prueba de correctitud por invariante de bucle (al inicio de cada iteración, arr[0..i] es un max-heap y arr[i..n] contiene los n−i mayores elementos ordenados). |
