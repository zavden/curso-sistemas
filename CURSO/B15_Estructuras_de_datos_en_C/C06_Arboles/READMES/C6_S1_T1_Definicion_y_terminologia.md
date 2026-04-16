# Definición y terminología

## Qué es un árbol

Un **árbol** es una estructura de datos jerárquica formada por nodos conectados
por aristas, donde:

1. Existe un nodo distinguido llamado **raíz** (root).
2. Cada nodo (excepto la raíz) tiene exactamente **un padre**.
3. No hay ciclos — existe un único camino entre la raíz y cualquier nodo.

```
         A           ← raíz
        / \
       B   C
      / \   \
     D   E   F
    /
   G
```

Un árbol con $n$ nodos tiene exactamente $n - 1$ aristas.

### Árbol vs lista vs grafo

| Propiedad | Lista | Árbol | Grafo |
|-----------|:-----:|:-----:|:-----:|
| Un padre por nodo | Sí (un predecesor) | Sí | No (múltiples) |
| Ciclos | No | No | Posible |
| Múltiples hijos | No (solo next) | Sí | Sí |
| Camino único entre nodos | Sí (trivial) | Sí | No necesariamente |
| Raíz distinguida | head | Sí | No necesariamente |

Una lista es un caso degenerado de árbol donde cada nodo tiene a lo sumo
un hijo.  Un árbol es un caso especial de grafo sin ciclos y conexo.

---

## Árbol binario

Un **árbol binario** es un árbol donde cada nodo tiene a lo sumo **dos hijos**:
izquierdo (left) y derecho (right).

```
       10
      /  \
     5    15
    / \     \
   3   7    20
```

La distinción izquierdo/derecho es significativa — estos dos árboles son
**diferentes**:

```
  A           A
 /             \
B               B

(B es hijo izquierdo)    (B es hijo derecho)
```

---

## Terminología

### Relaciones entre nodos

| Término | Definición | Ejemplo (árbol de arriba) |
|---------|-----------|--------------------------|
| **Raíz** (root) | Nodo sin padre | 10 |
| **Padre** (parent) | Nodo del que desciende otro | 5 es padre de 3 y 7 |
| **Hijo** (child) | Nodo que desciende de otro | 3 y 7 son hijos de 5 |
| **Hermano** (sibling) | Nodos con el mismo padre | 3 y 7 son hermanos |
| **Hoja** (leaf) | Nodo sin hijos | 3, 7, 20 |
| **Nodo interno** | Nodo con al menos un hijo | 10, 5, 15 |
| **Ancestro** | Nodo en el camino hacia la raíz | 10 y 5 son ancestros de 3 |
| **Descendiente** | Nodo en algún subárbol | 3, 7 son descendientes de 5 |
| **Subárbol** (subtree) | Árbol formado por un nodo y todos sus descendientes | Subárbol de 5: {5, 3, 7} |

### Métricas

| Término | Definición | Ejemplo |
|---------|-----------|---------|
| **Profundidad** (depth) | Distancia (en aristas) desde la raíz hasta el nodo | depth(10)=0, depth(5)=1, depth(3)=2 |
| **Altura** (height) | Distancia desde el nodo hasta su hoja más lejana | height(3)=0, height(5)=1, height(10)=2 |
| **Nivel** (level) | Conjunto de nodos a la misma profundidad | Nivel 0: {10}, Nivel 1: {5, 15}, Nivel 2: {3, 7, 20} |
| **Altura del árbol** | Altura de la raíz (= profundidad máxima) | 2 |

```
Profundidad:   0         1        2
              10
             /  \
            5    15
           / \     \
          3   7    20

Nivel 0: {10}
Nivel 1: {5, 15}
Nivel 2: {3, 7, 20}

Altura del árbol: 2
```

La profundidad crece **hacia abajo** (desde la raíz).  La altura crece **hacia
arriba** (desde las hojas).  Para la raíz: profundidad = 0, altura = altura
del árbol.

### Convención: altura del árbol vacío

Dos convenciones comunes:

| Árbol | Convención 1 | Convención 2 |
|-------|:---:|:---:|
| Vacío (NULL) | $-1$ | $0$ |
| Un solo nodo | $0$ | $1$ |
| Raíz con un hijo | $1$ | $2$ |

Este curso usa la convención 1: **altura del árbol vacío = $-1$**, altura de
un nodo hoja = 0.  Esto hace que la fórmula de nodos máximos sea más limpia.

---

## Tipos de árboles binarios

### Árbol completo (full binary tree)

Cada nodo tiene **0 o 2 hijos** — nunca 1:

```
       1
      / \
     2   3
    / \
   4   5

Full: cada nodo tiene 0 o 2 hijos ✓
```

### Árbol perfecto (perfect binary tree)

Todos los niveles están completamente llenos:

```
        1
       / \
      2   3
     / \ / \
    4  5 6  7

Perfecto: todos los niveles llenos ✓
Altura 2, 7 nodos = 2³ − 1
```

### Árbol completo (complete binary tree)

Todos los niveles están llenos excepto posiblemente el último, que se llena
de izquierda a derecha:

```
        1
       / \
      2   3
     / \ /
    4  5 6

Completo: último nivel llena de izquierda a derecha ✓
(el hueco está a la derecha, no en medio)
```

```
        1
       / \
      2   3
     /   / \
    4   6   7

NO completo: hueco en hijo derecho de 2, pero 3 tiene dos hijos ✗
```

Los heaps (montículos) usan árboles completos — se verán en C07.

### Árbol degenerado

Cada nodo tiene a lo sumo un hijo — esencialmente una lista:

```
  1
   \
    2
     \
      3
       \
        4

Degenerado: cada nodo tiene 0 o 1 hijo
Altura = n − 1 (máxima posible)
```

---

## Propiedades matemáticas

### Nodos máximos por nivel

El nivel $k$ tiene a lo sumo $2^k$ nodos:

| Nivel | Máximo de nodos |
|:-----:|:---------------:|
| 0 | $2^0 = 1$ |
| 1 | $2^1 = 2$ |
| 2 | $2^2 = 4$ |
| 3 | $2^3 = 8$ |
| $k$ | $2^k$ |

### Nodos máximos en un árbol de altura $h$

$$n_{\max} = 2^{h+1} - 1$$

Un árbol perfecto de altura $h$ tiene exactamente $2^{h+1} - 1$ nodos.

| Altura | Nodos máximos |
|:------:|:-------------:|
| 0 | 1 |
| 1 | 3 |
| 2 | 7 |
| 3 | 15 |
| 10 | 2047 |
| 20 | 2,097,151 |
| 30 | ~$10^9$ |

Con solo 30 niveles, un árbol binario perfecto puede contener más de mil
millones de nodos.

### Altura mínima para $n$ nodos

$$h_{\min} = \lfloor \log_2 n \rfloor$$

Un árbol balanceado con $n$ nodos tiene altura $O(\log n)$.  Esto es lo que
hace a los BST balanceados eficientes: buscar entre un millón de nodos
requiere a lo sumo ~20 comparaciones.

### Altura máxima para $n$ nodos

$$h_{\max} = n - 1$$

El caso degenerado (lista).  Un BST degenerado con $n$ nodos tiene búsqueda
$O(n)$ — tan malo como una lista.

### Número de hojas

En un árbol binario completo con $n$ nodos internos:

$$\text{hojas} = n + 1$$

Los nodos internos tienen exactamente $n - 1$ aristas (una por hijo), y
las hojas son los nodos sin hijos.  En un árbol perfecto de altura $h$:
hojas $= 2^h$, internos $= 2^h - 1$.

---

## Recursión y árboles

Los árboles binarios son intrínsecamente recursivos: un árbol es un nodo con
dos **subárboles** (que a su vez son árboles o vacíos).

```
Definición recursiva:
  Un árbol binario es:
    - Vacío (NULL), o
    - Un nodo con un dato, un subárbol izquierdo, y un subárbol derecho
```

Esta definición se mapea directamente a código:

```c
// C: NULL o un nodo con dos subárboles
typedef struct TreeNode {
    int data;
    struct TreeNode *left;
    struct TreeNode *right;
} TreeNode;
```

```rust
// Rust: None o un nodo con dos subárboles
type Tree<T> = Option<Box<TreeNode<T>>>;

struct TreeNode<T> {
    data: T,
    left: Tree<T>,
    right: Tree<T>,
}
```

Las operaciones sobre árboles siguen naturalmente el patrón recursivo:

```c
// Contar nodos — recursivo
int count(TreeNode *root) {
    if (!root) return 0;
    return 1 + count(root->left) + count(root->right);
}

// Altura — recursivo
int height(TreeNode *root) {
    if (!root) return -1;
    int lh = height(root->left);
    int rh = height(root->right);
    return 1 + (lh > rh ? lh : rh);
}

// Contar hojas — recursivo
int count_leaves(TreeNode *root) {
    if (!root) return 0;
    if (!root->left && !root->right) return 1;
    return count_leaves(root->left) + count_leaves(root->right);
}
```

```rust
fn count<T>(node: &Tree<T>) -> usize {
    match node {
        None => 0,
        Some(n) => 1 + count(&n.left) + count(&n.right),
    }
}

fn height<T>(node: &Tree<T>) -> i32 {
    match node {
        None => -1,
        Some(n) => 1 + height(&n.left).max(height(&n.right)),
    }
}

fn count_leaves<T>(node: &Tree<T>) -> usize {
    match node {
        None => 0,
        Some(n) if n.left.is_none() && n.right.is_none() => 1,
        Some(n) => count_leaves(&n.left) + count_leaves(&n.right),
    }
}
```

Cada función sigue el mismo patrón:
1. **Caso base**: árbol vacío → retornar valor base.
2. **Caso recursivo**: procesar el nodo actual + recurrir en left y right.

---

## Visualización en consola

Una función útil para debugging — imprimir el árbol de forma legible:

```c
void print_tree(TreeNode *root, int depth) {
    if (!root) return;
    print_tree(root->right, depth + 1);   // derecho primero (arriba)
    for (int i = 0; i < depth; i++) printf("    ");
    printf("%d\n", root->data);
    print_tree(root->left, depth + 1);    // izquierdo después (abajo)
}
```

Para el árbol `{10, 5, 15, 3, 7, 20}`:

```
        20
    15
10
        7
    5
        3
```

Rotado 90° a la derecha: la raíz está a la izquierda, el subárbol derecho
arriba, el izquierdo abajo.

```rust
fn print_tree<T: std::fmt::Display>(node: &Tree<T>, depth: usize) {
    if let Some(n) = node {
        print_tree(&n.right, depth + 1);
        println!("{}{}", "    ".repeat(depth), n.data);
        print_tree(&n.left, depth + 1);
    }
}
```

---

## Por qué estudiar árboles

| Aplicación | Tipo de árbol | Beneficio |
|-----------|--------------|-----------|
| Búsqueda eficiente | BST, AVL, rojinegro | $O(\log n)$ |
| Colas de prioridad | Heap (árbol completo) | Insert/extract $O(\log n)$ |
| Sistemas de archivos | Árbol general | Jerarquía natural |
| Compiladores | Árbol de sintaxis (AST) | Representar expresiones |
| Bases de datos | B-tree | Búsqueda en disco $O(\log n)$ |
| Compresión | Huffman | Codificación óptima |
| IA / juegos | Minimax | Evaluar decisiones |
| Autocompletado | Trie | Búsqueda por prefijo |
| DOM (HTML) | Árbol general | Estructura de documentos |

Los árboles son la estructura más versátil en ciencias de la computación.
Aparecen en prácticamente toda área de la disciplina.

---

## Ejercicios

### Ejercicio 1 — Identificar términos

Dado el árbol:

```
         A
        / \
       B   C
      / \   \
     D   E   F
    /       / \
   G       H   I
```

Identifica: raíz, hojas, nodos internos, padre de E, hijos de C, hermanos
de D, ancestros de G, profundidad de H, altura de B, altura del árbol.

<details>
<summary>Respuestas</summary>

- Raíz: A
- Hojas: G, E, H, I
- Nodos internos: A, B, C, D, F
- Padre de E: B
- Hijos de C: F
- Hermanos de D: E (ambos hijos de B)
- Ancestros de G: D, B, A
- Profundidad de H: 3 (A→C→F→H)
- Altura de B: 2 (B→D→G)
- Altura del árbol: 3 (A→C→F→H o A→B→D→G)
</details>

### Ejercicio 2 — Nodos máximos

¿Cuántos nodos tiene un árbol binario perfecto de altura 5?  ¿Y de altura 10?

<details>
<summary>Cálculo</summary>

$$n = 2^{h+1} - 1$$

Altura 5: $2^6 - 1 = 63$ nodos.
Altura 10: $2^{11} - 1 = 2047$ nodos.

Un árbol perfecto de altura 20 tendría $2^{21} - 1 = 2,097,151$ nodos —
más de 2 millones, con solo 21 niveles.
</details>

### Ejercicio 3 — Altura mínima

¿Cuál es la altura mínima de un árbol binario con 100 nodos?  ¿Y con
1,000,000?

<details>
<summary>Cálculo</summary>

$$h_{\min} = \lfloor \log_2 n \rfloor$$

100 nodos: $\lfloor \log_2 100 \rfloor = \lfloor 6.64 \rfloor = 6$.
1,000,000 nodos: $\lfloor \log_2 10^6 \rfloor = \lfloor 19.93 \rfloor = 19$.

Con 1 millón de nodos, un árbol balanceado tiene solo 20 niveles.  Buscar
un elemento requiere a lo sumo 20 comparaciones — esto es la potencia de
$O(\log n)$.
</details>

### Ejercicio 4 — Clasificar árboles

Clasifica cada árbol como completo/full/perfecto/degenerado (puede ser más
de uno):

```
Árbol 1:       Árbol 2:       Árbol 3:       Árbol 4:
    1              1              1              1
   / \            /              / \            / \
  2   3          2              2   3          2   3
 / \            /              / \   \        / \ / \
4   5          3              4   5   6      4  5 6  7
              /
             4
```

<details>
<summary>Clasificación</summary>

1. Full ✓ (cada nodo tiene 0 o 2 hijos), completo ✓ (último nivel llena de
   izquierda a derecha). No perfecto (nivel 2 no está lleno).

2. Degenerado ✓ (cada nodo tiene 0 o 1 hijo). No completo (los hijos no
   llenan de izquierda a derecha uniformemente — 2 tiene solo hijo izquierdo,
   lo cual sí es left-filled, pero 3 tiene solo hijo izquierdo también; es
   completo si y solo si el último nivel llena de izquierda a derecha).
   En realidad: 1→2→3→4 todos por la izquierda, sí es completo.

3. Completo ✓ (último nivel llena de izquierda a derecha: 4,5,6). No full
   (nodo 3 tiene solo un hijo).

4. Full ✓, completo ✓, perfecto ✓ (todos los niveles llenos).
</details>

### Ejercicio 5 — Recursión: count y height

Implementa `count` y `height` en C.  Traza manualmente ambas funciones
sobre:

```
    10
   /  \
  5    15
 /
3
```

<details>
<summary>Traza de height</summary>

```
height(10)
  height(5)
    height(3)
      height(NULL) = -1
      height(NULL) = -1
      return 1 + max(-1, -1) = 0
    height(NULL) = -1
    return 1 + max(0, -1) = 1
  height(15)
    height(NULL) = -1
    height(NULL) = -1
    return 1 + max(-1, -1) = 0
  return 1 + max(1, 0) = 2

count(10) = 1 + count(5) + count(15) = 1 + (1 + 1 + 0) + (1 + 0 + 0) = 4
```
</details>

### Ejercicio 6 — Contar hojas

Implementa `count_leaves` en C y en Rust.  ¿Cuántas hojas tiene un árbol
perfecto de altura $h$?

<details>
<summary>Respuesta</summary>

Un árbol perfecto de altura $h$ tiene $2^h$ hojas.

Altura 0: 1 hoja (solo raíz).
Altura 3: 8 hojas.
Altura 10: 1024 hojas.

Exactamente la mitad (redondeando arriba) de los nodos de un árbol perfecto
son hojas: $\frac{2^{h+1}-1+1}{2} = 2^h$.
</details>

### Ejercicio 7 — Relación hojas y nodos internos

Para un árbol binario **full** (cada nodo tiene 0 o 2 hijos), demuestra
que hojas $=$ internos $+ 1$.  Verifica con un ejemplo de 7 nodos.

<details>
<summary>Demostración y ejemplo</summary>

Cada nodo interno contribuye 2 hijos.  El total de nodos es
$n = \text{internos} + \text{hojas}$.  El total de aristas es
$n - 1 = 2 \times \text{internos}$ (cada interno tiene 2 hijos).

$\text{internos} + \text{hojas} - 1 = 2 \times \text{internos}$
$\text{hojas} = \text{internos} + 1$

Ejemplo con 7 nodos:
```
       1
      / \
     2   3
    / \ / \
   4  5 6  7
```
Internos: 1, 2, 3 → 3.  Hojas: 4, 5, 6, 7 → 4.  $4 = 3 + 1$ ✓.
</details>

### Ejercicio 8 — Visualizar en consola

Implementa `print_tree` en C (la versión rotada 90°).  Construye manualmente
el árbol `{10, 5, 15, 3, 7, 12, 20}` y muestra la salida.

<details>
<summary>Salida</summary>

```
        20
    15
        12
10
        7
    5
        3
```

La raíz (10) está en la columna 0.  Cada nivel de profundidad añade 4
espacios de indentación.  El subárbol derecho se imprime arriba, el
izquierdo abajo — si giras la cabeza 90° a la izquierda, ves el árbol
en su orientación natural.
</details>

### Ejercicio 9 — Árbol degenerado

Construye un árbol binario insertando los valores 1, 2, 3, 4, 5 cada uno
como hijo derecho del anterior.  ¿Cuál es la altura?  ¿Cuántas comparaciones
necesitas para buscar el valor 5?

<details>
<summary>Respuesta</summary>

```
1
 \
  2
   \
    3
     \
      4
       \
        5
```

Altura: 4 ($n - 1$).  Comparaciones para buscar 5: 5 (recorrer toda la
cadena).  Es una lista enlazada disfrazada de árbol — $O(n)$ para todo.

Este es el caso que motiva los árboles balanceados (S04): garantizar
altura $O(\log n)$ independientemente del orden de inserción.
</details>

### Ejercicio 10 — Árboles posibles

¿Cuántos árboles binarios **estructuralmente distintos** existen con 3
nodos?  Dibújalos todos.

<details>
<summary>Respuesta</summary>

5 árboles (números de Catalan: $C_3 = 5$):

```
  1       1       1       1         1
 /       /         \       \       / \
2       2           2       2     2   3
 \     /           /         \
  3   3           3           3
```

La fórmula general es el número de Catalan:
$$C_n = \frac{1}{n+1}\binom{2n}{n}$$

$C_1 = 1$, $C_2 = 2$, $C_3 = 5$, $C_4 = 14$, $C_5 = 42$.
</details>
