# C09 — Búsqueda

**Objetivo**: Dominar algoritmos de búsqueda en secuencias (lineal, binaria,
interpolación) y en árboles (BST, balanceados, B-trees, tries). Incluye
búsqueda en las bibliotecas estándar de C y Rust.

**Prerequisitos**: C06 (árboles — BST y balanceados ya implementados),
C08 S01 (fundamentos — búsqueda requiere arrays ordenados).

---

## S01 — Búsqueda en secuencias

| Tópico | Contenido |
|--------|-----------|
| **T01 — Búsqueda secuencial** | O(n), optimizaciones (sentinel, move-to-front, transposition). |
| **T02 — Búsqueda binaria** | O(log n), requisito de array ordenado, iterativa y recursiva. `lower_bound`, `upper_bound`. Bug clásico: overflow en `(lo + hi) / 2`. ▸ **Demostración formal**: prueba de correctitud por invariante de bucle (si el elemento existe, está en arr[lo..hi] al inicio de cada iteración) y terminación (hi − lo decrece estrictamente en cada paso). |
| **T03 — Búsqueda por interpolación** | Estimar posición por valor. O(log log n) para uniformes, O(n) peor caso. |
| **T04 — Búsqueda en Rust** | `binary_search` (retorna `Result<usize, usize>`), `contains`, `Iterator::find`, `Iterator::position`. |

**Ejercicios**: Búsqueda binaria sin bugs en C. `lower_bound` y `upper_bound`.
Comparar secuencial vs binaria vs interpolación para 10⁶ elementos.

---

## S02 — Búsqueda en árboles

| Tópico | Contenido |
|--------|-----------|
| **T01 — Búsqueda en BST** | O(log n) promedio, O(n) peor. Referencia a C06 S03. |
| **T02 — Búsqueda en balanceados** | O(log n) garantizado (AVL, RB). Referencia a C06 S04. |
| **T03 — B-trees** | Nodos con múltiples claves e hijos, optimizado para disco (I/O), inserción con split, eliminación con merge/redistribución. |
| **T04 — B+ trees** | Datos solo en hojas, hojas enlazadas para recorrido secuencial. Uso en bases de datos y filesystems. |
| **T05 — Tries revisitado** | Búsqueda O(m) independiente de n, prefix matching, autocompletado. Patricia tries (compresión de caminos). |

**Ejercicios**: B-tree de orden 3 en C (inserción y búsqueda). Patricia trie para
diccionario. Comparar BST vs AVL vs B-tree vs trie para buscar 10⁴ palabras.

---

## Demostraciones formales pendientes (README_EXTRA.md)

| Ubicación | Contenido |
|-----------|-----------|
| `S01_Busqueda_en_secuencias/T02_Busqueda_binaria/README_EXTRA.md` | Prueba de correctitud por invariante de bucle (si el elemento existe, está en arr[lo..hi]) y terminación (hi − lo decrece estrictamente en cada paso). |
