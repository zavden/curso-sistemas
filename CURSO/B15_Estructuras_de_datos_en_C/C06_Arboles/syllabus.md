# C06 — Árboles

**Objetivo**: Dominar árboles binarios (representación, recorridos), árboles de
búsqueda (BST), árboles balanceados (AVL, rojinegros), y árboles generales
(n-arios, tries, expresión, juego). Implementar en C y Rust.

**Prerequisitos**: C03 (recursión — los recorridos y operaciones son recursivos),
C05 (listas — los nodos de árbol son similares a nodos de lista).

---

## S01 — Árboles binarios

| Tópico | Contenido |
|--------|-----------|
| **T01 — Definición y terminología** | Raíz, nodos, hojas, padre, hijo, hermano, subárbol, profundidad, altura, nivel. Propiedades: máximo 2^(h+1) − 1 nodos. |
| **T02 — Representación con punteros C** | Struct `TreeNode` con `data`, `left`, `right`. Creación, árbol vacío = `NULL`. |
| **T03 — Representación con array** | Hijo izquierdo `2i+1`, derecho `2i+2`, padre `(i−1)/2`. Eficiente para completos (heaps), desperdicio para desbalanceados. |
| **T04 — Representación en Rust** | `Option<Box<TreeNode<T>>>`, `enum Tree<T> { Leaf, Node(...) }`. Comparación de estilos. |

**Ejercicios**: Construir árbol manualmente en C y Rust. Contar nodos, hojas,
calcular altura. Visualizar en consola.

---

## S02 — Recorridos

| Tópico | Contenido |
|--------|-----------|
| **T01 — Recorridos recursivos** | Inorden, preorden, postorden. Implementación en C y Rust. |
| **T02 — Recorrido por niveles (BFS)** | Usando cola, nivel por nivel. Con cola de C04 (C) y `VecDeque` (Rust). |
| **T03 — Recorridos iterativos** | Inorden iterativo con pila explícita. Morris traversal como bonus. |
| **T04 — Aplicaciones de recorridos** | Serialización/deserialización, copia, igualdad, liberación (postorden). |

**Ejercicios**: Los 4 recorridos en C y Rust. Serializar árbol con preorden y
reconstruir. Verificar que postorden libera toda la memoria (Valgrind).

---

## S03 — Árbol binario de búsqueda (BST)

| Tópico | Contenido |
|--------|-----------|
| **T01 — Definición y propiedad** | Para cada nodo: izquierda < nodo < derecha. O(log n) balanceado, O(n) degenerado. |
| **T02 — Inserción** | Buscar posición, insertar como hoja. Recursiva e iterativa en C. ▸ **Demostración formal**: prueba por invariante de bucle de que la inserción preserva la propiedad BST (∀ nodo: izq < nodo < der). |
| **T03 — Búsqueda** | Recursiva e iterativa, mínimo, máximo, sucesor, predecesor. |
| **T04 — Eliminación** | Tres casos (hoja, un hijo, dos hijos). Reemplazo con sucesor/predecesor inorden. |
| **T05 — BST en Rust** | `Option<Box<Node<T>>>` con `T: Ord`, `match` para los tres casos de eliminación. |
| **T06 — Degeneración y balanceo** | Insertar secuencia ordenada → lista. Motivación para árboles balanceados. |

**Ejercicios**: BST completo en C y Rust. Insertar 1..n (degenerado) vs aleatorio.
Verificar alturas.

---

## S04 — Árboles balanceados

| Tópico | Contenido |
|--------|-----------|
| **T01 — Rotaciones** | Simple (izquierda, derecha), doble (izquierda-derecha, derecha-izquierda). Visualización paso a paso. |
| **T02 — Árboles AVL** | Factor de balance ∈ {−1, 0, 1}, inserción con rebalanceo, O(log n) garantizado. ▸ **Demostración formal**: probar que la altura de un AVL con n nodos satisface h ≤ 1.44·log₂(n+2) usando árboles de Fibonacci (N(h) = N(h−1) + N(h−2) + 1 ≥ φʰ). |
| **T03 — Árboles rojinegros** | 5 propiedades, coloreo, inserción con recoloreo + rotaciones. Uso en kernel Linux, Java TreeMap. ▸ **Demostración formal**: probar que h ≤ 2·log₂(n+1) usando el argumento de black-height (bh ≥ h/2 y subárbol con bh tiene ≥ 2^bh − 1 nodos internos, por inducción). |
| **T04 — AVL vs rojinegro** | AVL: búsquedas más rápidas. RB: inserciones más rápidas. Tabla comparativa. |
| **T05 — Implementación en Rust** | AVL genérico. `BTreeMap<K,V>` de stdlib (B-tree) como alternativa práctica. |

**Ejercicios**: AVL con inserción + rotaciones en C. Verificar invariante de balance.
Comparar BST degenerado vs AVL vs `BTreeMap` para n = 10⁵.

---

## S05 — Árboles generales y aplicaciones

| Tópico | Contenido |
|--------|-----------|
| **T01 — Árboles n-arios** | Representación hijo-izquierdo/hermano-derecho, array de hijos. |
| **T02 — Árboles de expresión** | Construir desde infija/posfija, evaluar recursivamente, imprimir con paréntesis. |
| **T03 — Árboles de juego minimax** | Estados de juego como árbol, minimax, poda alfa-beta. Tres en raya. |
| **T04 — Tries** | Búsqueda de strings por prefijo, inserción, autocompletado. Implementación en C y Rust. |

**Ejercicios**: Árbol de expresión para `(3+4)*(5−2)`. Minimax para tres en raya.
Trie para autocompletado de palabras.

---

## Demostraciones formales pendientes (README_EXTRA.md)

| Ubicación | Contenido |
|-----------|-----------|
| `S03_Arbol_binario_de_busqueda/T02_Insercion/README_EXTRA.md` | Prueba por invariante de bucle de que la inserción preserva la propiedad BST (∀ nodo: izq < nodo < der). |
| `S04_Arboles_balanceados/T02_Arboles_AVL/README_EXTRA.md` | Probar que h ≤ 1.44·log₂(n+2) usando árboles de Fibonacci (N(h) = N(h−1) + N(h−2) + 1 ≥ φʰ). |
| `S04_Arboles_balanceados/T03_Arboles_rojinegros/README_EXTRA.md` | Probar que h ≤ 2·log₂(n+1) usando el argumento de black-height (bh ≥ h/2, subárbol con bh tiene ≥ 2^bh − 1 nodos internos, por inducción). |
