# Tries

## Concepto

Un **trie** (pronunciado "trai", del inglés *retrieval*) es un árbol de búsqueda
donde cada nodo representa un **carácter** de una cadena, y los caminos desde la
raíz hasta nodos marcados forman las **palabras** almacenadas. También se
conoce como **árbol de prefijos** (*prefix tree*) o **árbol digital**.

A diferencia de un BST, donde cada nodo almacena una clave completa y la búsqueda
compara claves enteras, en un trie la clave se **descompone** carácter por
carácter a lo largo del camino. Cada nivel del árbol corresponde a una posición
en la cadena.

### Ejemplo

Almacenando las palabras `car`, `cat`, `card`, `care`, `do`, `dog`:

```
         (raiz)
        /      \
       c        d
       |        |
       a        o *
      / \       |
     r * t *    g *
    / \
   d * e *
```

Los nodos marcados con `*` indican **fin de palabra**. Observaciones:

- `car` y `card` comparten el prefijo `car`; la `d` de `card` extiende el camino.
- `cat` diverge de `car` en el tercer carácter.
- `do` es prefijo de `dog`, ambos almacenados (nodo `o` marcado como fin de
  palabra).
- La raíz no contiene carácter — es el punto de partida para todas las palabras.

---

## Propiedades fundamentales

| Propiedad | Descripción |
|-----------|-------------|
| Búsqueda | $O(m)$ donde $m$ es la longitud de la cadena buscada |
| Inserción | $O(m)$ |
| Eliminación | $O(m)$ |
| Independencia de $n$ | El costo **no depende** del número de claves almacenadas |
| Prefijos compartidos | Las claves con prefijo común comparten nodos |
| Búsqueda por prefijo | Naturalmente eficiente — es la operación para la que fue diseñado |
| Orden lexicográfico | Un recorrido DFS produce las claves en orden alfabético |

La complejidad $O(m)$ para búsqueda es notable. En un BST balanceado con $n$
cadenas de longitud promedio $m$, la búsqueda es $O(m \log n)$ — cada comparación
de cadenas cuesta $O(m)$ y hay $O(\log n)$ comparaciones. En un trie, la búsqueda
es $O(m)$ independientemente de $n$. Para diccionarios grandes, esta diferencia
es significativa.

En una tabla hash la búsqueda también es $O(m)$ en promedio (calcular el hash
cuesta $O(m)$), pero la tabla hash no soporta eficientemente búsqueda por
prefijo ni iteración ordenada.

---

## Representación del nodo

### Opción 1: Array de hijos (alfabeto fijo)

Para un alfabeto de tamaño $|\Sigma|$ (e.g., 26 letras minúsculas), cada nodo
tiene un array de $|\Sigma|$ punteros:

```c
#define ALPHABET_SIZE 26

typedef struct TrieNode {
    struct TrieNode *children[ALPHABET_SIZE];
    int is_end_of_word;
} TrieNode;
```

El índice del array corresponde al carácter: `'a'` → 0, `'b'` → 1, …,
`'z'` → 25. La función de mapeo es `c - 'a'`.

**Ventaja**: acceso $O(1)$ al hijo correspondiente a cualquier carácter.
**Desventaja**: cada nodo ocupa $26 \times 8 = 208$ bytes (en 64-bit) más el
flag, independientemente de cuántos hijos tenga realmente. Si el alfabeto es
grande (Unicode: 143,859 caracteres) o el trie es disperso, el desperdicio es
enorme.

### Opción 2: HashMap de hijos

```rust
use std::collections::HashMap;

struct TrieNode {
    children: HashMap<char, Box<TrieNode>>,
    is_end_of_word: bool,
}
```

**Ventaja**: solo almacena los hijos que existen; funciona con cualquier
alfabeto.
**Desventaja**: overhead del hash map por nodo; peor localidad de caché.

### Opción 3: Lista enlazada de hijos (LCRS)

Usar la representación hijo-izquierdo/hermano-derecho del tópico T01. Cada nodo
tiene un puntero al primer hijo y al siguiente hermano. Buscar un carácter
requiere recorrer la lista de hermanos: $O(|\Sigma|)$ en el peor caso, pero
$O(k)$ donde $k$ es el número de hijos reales.

### Cuándo usar cada una

| Representación | Mejor para |
|---------------|------------|
| Array fijo | Alfabeto pequeño conocido (a-z), alto rendimiento |
| HashMap | Alfabeto grande o variable (Unicode), flexibilidad |
| LCRS | Memoria mínima, pocos hijos por nodo |

Para este tópico usamos **array fijo de 26** (letras minúsculas), que es la
implementación más común en entrevistas y aplicaciones prácticas.

---

## Operaciones básicas en C

### Crear nodo

```c
TrieNode *trie_create_node(void) {
    TrieNode *node = malloc(sizeof(TrieNode));
    node->is_end_of_word = 0;
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        node->children[i] = NULL;
    }
    return node;
}
```

Alternativa más eficiente con `calloc`:

```c
TrieNode *trie_create_node(void) {
    TrieNode *node = calloc(1, sizeof(TrieNode));
    // calloc inicializa todo a cero: punteros NULL, is_end = 0
    return node;
}
```

### Inserción

Recorrer la cadena carácter por carácter, creando nodos según sea necesario,
y marcar el último como fin de palabra.

```c
void trie_insert(TrieNode *root, const char *word) {
    TrieNode *current = root;
    for (int i = 0; word[i] != '\0'; i++) {
        int index = word[i] - 'a';
        if (current->children[index] == NULL) {
            current->children[index] = trie_create_node();
        }
        current = current->children[index];
    }
    current->is_end_of_word = 1;
}
```

Traza de insertar `cat` en un trie vacío:

| Paso | Carácter | Índice | Acción |
|------|----------|--------|--------|
| 1 | `c` | 2 | Crear nodo, avanzar |
| 2 | `a` | 0 | Crear nodo, avanzar |
| 3 | `t` | 19 | Crear nodo, avanzar |
| 4 | — | — | Marcar `is_end_of_word = 1` |

Insertar `car` después solo crea un nodo nuevo para `r` (reutiliza `c` → `a`):

| Paso | Carácter | Índice | Acción |
|------|----------|--------|--------|
| 1 | `c` | 2 | Ya existe, avanzar |
| 2 | `a` | 0 | Ya existe, avanzar |
| 3 | `r` | 17 | Crear nodo, avanzar |
| 4 | — | — | Marcar `is_end_of_word = 1` |

### Búsqueda

```c
int trie_search(TrieNode *root, const char *word) {
    TrieNode *current = root;
    for (int i = 0; word[i] != '\0'; i++) {
        int index = word[i] - 'a';
        if (current->children[index] == NULL) {
            return 0;  // caracter no encontrado
        }
        current = current->children[index];
    }
    return current->is_end_of_word;  // 1 si es palabra completa
}
```

Observación crucial: si `is_end_of_word` es 0, la cadena existe como **prefijo**
de alguna palabra, pero no fue insertada como palabra completa. Por ejemplo,
buscar `car` en un trie que solo contiene `card` llegaría al nodo de `r` pero
`is_end_of_word` sería 0.

### Búsqueda de prefijo

```c
int trie_starts_with(TrieNode *root, const char *prefix) {
    TrieNode *current = root;
    for (int i = 0; prefix[i] != '\0'; i++) {
        int index = prefix[i] - 'a';
        if (current->children[index] == NULL) {
            return 0;
        }
        current = current->children[index];
    }
    return 1;  // el prefijo existe (no necesita ser palabra completa)
}
```

La diferencia con `search` es que no verificamos `is_end_of_word`.

### Eliminación

Eliminar una palabra es más complejo. No basta con desmarcar `is_end_of_word` —
también debemos eliminar nodos que ya no son necesarios (que no son prefijo de
ninguna otra palabra ni fin de otra palabra).

```c
int has_children(TrieNode *node) {
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (node->children[i] != NULL) return 1;
    }
    return 0;
}

// retorna 1 si el nodo actual debe ser eliminado por el padre
int trie_delete_helper(TrieNode *node, const char *word, int depth) {
    if (node == NULL) return 0;

    if (word[depth] == '\0') {
        // llegamos al final de la palabra
        if (!node->is_end_of_word) return 0;  // palabra no existia
        node->is_end_of_word = 0;
        // eliminar si no tiene hijos
        return !has_children(node);
    }

    int index = word[depth] - 'a';
    if (trie_delete_helper(node->children[index], word, depth + 1)) {
        free(node->children[index]);
        node->children[index] = NULL;
        // eliminar este nodo si no es fin de otra palabra y no tiene hijos
        return !node->is_end_of_word && !has_children(node);
    }
    return 0;
}

void trie_delete(TrieNode *root, const char *word) {
    trie_delete_helper(root, word, 0);
}
```

Traza de eliminar `car` del trie con `car`, `card`, `cat`:

1. Recorrer c → a → r. Llegamos al nodo `r`.
2. Desmarcar `is_end_of_word` de `r`.
3. ¿Tiene hijos `r`? Sí (`d` para `card`). **No eliminar** `r`.
4. Retornar 0 hasta la raíz. No se elimina ningún nodo.

Traza de eliminar `card` del trie (suponiendo que `car` ya fue eliminado):

1. Recorrer c → a → r → d. Llegamos al nodo `d`.
2. Desmarcar `is_end_of_word` de `d`.
3. ¿Tiene hijos `d`? No. Retornar 1 → padre (`r`) libera `d`.
4. ¿`r` es fin de palabra? No (se eliminó `car`). ¿Tiene hijos? No. Retornar 1
   → padre (`a`) libera `r`.
5. ¿`a` es fin de palabra? No. ¿Tiene hijos? Sí (`t` para `cat`). Retornar 0.

Resultado: se eliminan los nodos `d` y `r`; `a` permanece por `cat`.

---

## Autocompletado

La aplicación estrella de los tries: dado un prefijo, encontrar todas las
palabras que lo extienden.

### Algoritmo

1. Navegar hasta el nodo correspondiente al último carácter del prefijo.
2. Desde ahí, hacer un DFS recolectando todas las palabras (nodos con
   `is_end_of_word`).

```c
void trie_autocomplete_helper(TrieNode *node, char *buffer, int depth) {
    if (node->is_end_of_word) {
        buffer[depth] = '\0';
        printf("  %s\n", buffer);
    }

    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (node->children[i] != NULL) {
            buffer[depth] = 'a' + i;
            trie_autocomplete_helper(node->children[i], buffer, depth + 1);
        }
    }
}

void trie_autocomplete(TrieNode *root, const char *prefix) {
    TrieNode *current = root;
    for (int i = 0; prefix[i] != '\0'; i++) {
        int index = prefix[i] - 'a';
        if (current->children[index] == NULL) {
            printf("No hay sugerencias para '%s'\n", prefix);
            return;
        }
        current = current->children[index];
    }

    char buffer[256];
    strcpy(buffer, prefix);
    int prefix_len = strlen(prefix);

    printf("Sugerencias para '%s':\n", prefix);
    trie_autocomplete_helper(current, buffer, prefix_len);
}
```

Para el trie con `car`, `card`, `care`, `cat`:

```
autocomplete("ca"):
  car
  card
  care
  cat

autocomplete("car"):
  car
  card
  care

autocomplete("card"):
  card

autocomplete("cu"):
  No hay sugerencias para 'cu'
```

Las sugerencias salen en **orden lexicográfico** automáticamente, porque el DFS
recorre los hijos en orden de índice (0='a', 1='b', …).

---

## Contar palabras y nodos

```c
int trie_count_words(TrieNode *root) {
    if (root == NULL) return 0;
    int count = root->is_end_of_word ? 1 : 0;
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        count += trie_count_words(root->children[i]);
    }
    return count;
}

int trie_count_nodes(TrieNode *root) {
    if (root == NULL) return 0;
    int count = 1;
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        count += trie_count_nodes(root->children[i]);
    }
    return count;
}
```

Para el trie con `car`, `card`, `care`, `cat`, `do`, `dog`:
- Palabras: 6
- Nodos: 11 (raíz + c, a, r, d, e, t, d, o, g más la raíz = 11 contando raíz)

### Palabra más larga

```c
int trie_longest_word(TrieNode *root, int depth) {
    if (root == NULL) return 0;
    int max_len = root->is_end_of_word ? depth : 0;
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        int len = trie_longest_word(root->children[i], depth + 1);
        if (len > max_len) max_len = len;
    }
    return max_len;
}
```

---

## Listar todas las palabras

```c
void trie_list_all(TrieNode *root, char *buffer, int depth) {
    if (root == NULL) return;
    if (root->is_end_of_word) {
        buffer[depth] = '\0';
        printf("%s\n", buffer);
    }
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (root->children[i] != NULL) {
            buffer[depth] = 'a' + i;
            trie_list_all(root->children[i], buffer, depth + 1);
        }
    }
}
```

Produce todas las palabras en **orden lexicográfico** — el trie actúa como un
diccionario ordenado sin necesidad de algoritmos de ordenamiento.

---

## Liberar memoria

```c
void trie_free(TrieNode *root) {
    if (root == NULL) return;
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        trie_free(root->children[i]);
    }
    free(root);
}
```

---

## Análisis de memoria

El uso de memoria de un trie depende fuertemente del contenido:

Para $n$ palabras de longitud promedio $m$ con alfabeto de tamaño $|\Sigma|$:

| Aspecto | Valor |
|---------|-------|
| Nodos (peor caso, sin prefijos compartidos) | $n \times m$ |
| Nodos (mejor caso, prefijos muy compartidos) | $\approx m$ |
| Memoria por nodo (array fijo) | $|\Sigma| \times$ sizeof(puntero) + flags |
| Memoria total (peor caso, array fijo, 26 letras) | $n \times m \times 216$ bytes |

Para un diccionario de 100,000 palabras inglesas (longitud promedio 8):

- Nodos estimados: ~400,000 (muchos prefijos compartidos)
- Memoria con array fijo: $400{,}000 \times 216 \approx 82$ MB
- Memoria con HashMap: mucho menor, pero con overhead por hash

Comparación con alternativas:

| Estructura | Búsqueda | Prefijo | Memoria |
|-----------|----------|---------|---------|
| Trie (array) | $O(m)$ | $O(m + k)$ | Alta |
| Trie (HashMap) | $O(m)$ | $O(m + k)$ | Media |
| BST de strings | $O(m \log n)$ | $O(m \log n + k)$ | Baja |
| Hash table | $O(m)$ avg | No eficiente | Baja |
| Sorted array + binary search | $O(m \log n)$ | $O(m \log n + k)$ | Mínima |

Donde $k$ es el número de resultados para la búsqueda por prefijo.

---

## Implementación en Rust

```rust
const ALPHABET_SIZE: usize = 26;

struct TrieNode {
    children: [Option<Box<TrieNode>>; ALPHABET_SIZE],
    is_end_of_word: bool,
}

impl TrieNode {
    fn new() -> Self {
        TrieNode {
            children: Default::default(),
            is_end_of_word: false,
        }
    }
}

struct Trie {
    root: TrieNode,
}

impl Trie {
    fn new() -> Self {
        Trie { root: TrieNode::new() }
    }

    fn insert(&mut self, word: &str) {
        let mut current = &mut self.root;
        for ch in word.chars() {
            let index = (ch as u8 - b'a') as usize;
            current = current.children[index]
                .get_or_insert_with(|| Box::new(TrieNode::new()));
        }
        current.is_end_of_word = true;
    }

    fn search(&self, word: &str) -> bool {
        self.find_node(word)
            .map_or(false, |node| node.is_end_of_word)
    }

    fn starts_with(&self, prefix: &str) -> bool {
        self.find_node(prefix).is_some()
    }

    fn find_node(&self, prefix: &str) -> Option<&TrieNode> {
        let mut current = &self.root;
        for ch in prefix.chars() {
            let index = (ch as u8 - b'a') as usize;
            match &current.children[index] {
                Some(node) => current = node,
                None => return None,
            }
        }
        Some(current)
    }

    fn autocomplete(&self, prefix: &str) -> Vec<String> {
        let mut results = Vec::new();
        if let Some(node) = self.find_node(prefix) {
            let mut buffer = prefix.to_string();
            Self::collect_words(node, &mut buffer, &mut results);
        }
        results
    }

    fn collect_words(node: &TrieNode, buffer: &mut String,
                     results: &mut Vec<String>) {
        if node.is_end_of_word {
            results.push(buffer.clone());
        }
        for i in 0..ALPHABET_SIZE {
            if let Some(ref child) = node.children[i] {
                buffer.push((b'a' + i as u8) as char);
                Self::collect_words(child, buffer, results);
                buffer.pop();
            }
        }
    }

    fn delete(&mut self, word: &str) -> bool {
        Self::delete_helper(&mut self.root, word, 0)
    }

    fn delete_helper(node: &mut TrieNode, word: &str, depth: usize) -> bool {
        let chars: Vec<char> = word.chars().collect();

        if depth == chars.len() {
            if !node.is_end_of_word { return false; }
            node.is_end_of_word = false;
            return true;
        }

        let index = (chars[depth] as u8 - b'a') as usize;
        if let Some(ref mut child) = node.children[index] {
            let deleted = Self::delete_helper(child, word, depth + 1);
            if deleted && !child.is_end_of_word && Self::is_empty(child) {
                node.children[index] = None;
            }
            deleted
        } else {
            false
        }
    }

    fn is_empty(node: &TrieNode) -> bool {
        node.children.iter().all(|c| c.is_none())
    }

    fn count_words(&self) -> usize {
        Self::count_words_helper(&self.root)
    }

    fn count_words_helper(node: &TrieNode) -> usize {
        let mut count = if node.is_end_of_word { 1 } else { 0 };
        for child in &node.children {
            if let Some(ref c) = child {
                count += Self::count_words_helper(c);
            }
        }
        count
    }

    fn list_all(&self) -> Vec<String> {
        let mut results = Vec::new();
        let mut buffer = String::new();
        Self::collect_words(&self.root, &mut buffer, &mut results);
        results
    }
}
```

### Notas sobre la implementación Rust

- `[Option<Box<TrieNode>>; ALPHABET_SIZE]` usa `Default::default()` que
  inicializa todos los elementos a `None`. Esto funciona porque `Option<Box<T>>`
  implementa `Default`.
- `get_or_insert_with` es la forma idiomática de "crear si no existe, luego
  devolver referencia mutable" — equivalente al patrón `if NULL then malloc`
  de C.
- `find_node` es un helper privado compartido entre `search`, `starts_with` y
  `autocomplete`, evitando duplicación.
- La eliminación limpia nodos huérfanos automáticamente: cuando `delete_helper`
  retorna `true`, el padre verifica si el hijo quedó vacío y sin marca, y lo
  elimina con `= None` (que ejecuta `Drop` automáticamente).

---

## Variante: HashMap de hijos

Para soportar cualquier carácter (mayúsculas, Unicode, dígitos):

```rust
use std::collections::HashMap;

struct FlexTrieNode {
    children: HashMap<char, Box<FlexTrieNode>>,
    is_end_of_word: bool,
}

impl FlexTrieNode {
    fn new() -> Self {
        FlexTrieNode {
            children: HashMap::new(),
            is_end_of_word: false,
        }
    }
}

struct FlexTrie {
    root: FlexTrieNode,
}

impl FlexTrie {
    fn new() -> Self {
        FlexTrie { root: FlexTrieNode::new() }
    }

    fn insert(&mut self, word: &str) {
        let mut current = &mut self.root;
        for ch in word.chars() {
            current = current.children
                .entry(ch)
                .or_insert_with(|| Box::new(FlexTrieNode::new()));
        }
        current.is_end_of_word = true;
    }

    fn search(&self, word: &str) -> bool {
        let mut current = &self.root;
        for ch in word.chars() {
            match current.children.get(&ch) {
                Some(node) => current = node,
                None => return false,
            }
        }
        current.is_end_of_word
    }

    fn autocomplete(&self, prefix: &str) -> Vec<String> {
        let mut current = &self.root;
        for ch in prefix.chars() {
            match current.children.get(&ch) {
                Some(node) => current = node,
                None => return Vec::new(),
            }
        }
        let mut results = Vec::new();
        let mut buffer = prefix.to_string();
        Self::collect(current, &mut buffer, &mut results);
        results
    }

    fn collect(node: &FlexTrieNode, buffer: &mut String,
               results: &mut Vec<String>) {
        if node.is_end_of_word {
            results.push(buffer.clone());
        }
        let mut keys: Vec<char> = node.children.keys().copied().collect();
        keys.sort();  // orden lexicografico
        for ch in keys {
            buffer.push(ch);
            Self::collect(&node.children[&ch], buffer, results);
            buffer.pop();
        }
    }
}
```

La principal diferencia es que `collect` debe **ordenar** las claves del HashMap
antes de iterar para garantizar orden lexicográfico. Con el array fijo esto era
automático.

---

## Aplicaciones reales

| Aplicación | Cómo usa el trie |
|------------|-----------------|
| **Autocompletado** (IDE, buscadores) | Prefijo → lista de sugerencias |
| **Corrector ortográfico** | Buscar palabra; si no existe, sugerir similares |
| **Enrutamiento IP** | Longest prefix match para tablas de enrutamiento |
| **T9 / teclado predictivo** | Secuencia de dígitos → palabras posibles |
| **Filtro de palabras** | Verificar si una cadena contiene palabras prohibidas |
| **Compresión (LZW)** | Diccionario de secuencias vistas |
| **Bioinformática** | Búsqueda de secuencias de ADN (alfabeto {A, C, G, T}) |

---

## Programa completo en C

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALPHABET_SIZE 26

typedef struct TrieNode {
    struct TrieNode *children[ALPHABET_SIZE];
    int is_end_of_word;
} TrieNode;

TrieNode *trie_create_node(void) {
    return calloc(1, sizeof(TrieNode));
}

void trie_insert(TrieNode *root, const char *word) {
    TrieNode *current = root;
    for (int i = 0; word[i]; i++) {
        int idx = word[i] - 'a';
        if (current->children[idx] == NULL) {
            current->children[idx] = trie_create_node();
        }
        current = current->children[idx];
    }
    current->is_end_of_word = 1;
}

int trie_search(TrieNode *root, const char *word) {
    TrieNode *current = root;
    for (int i = 0; word[i]; i++) {
        int idx = word[i] - 'a';
        if (current->children[idx] == NULL) return 0;
        current = current->children[idx];
    }
    return current->is_end_of_word;
}

int trie_starts_with(TrieNode *root, const char *prefix) {
    TrieNode *current = root;
    for (int i = 0; prefix[i]; i++) {
        int idx = prefix[i] - 'a';
        if (current->children[idx] == NULL) return 0;
        current = current->children[idx];
    }
    return 1;
}

int has_children(TrieNode *node) {
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (node->children[i]) return 1;
    }
    return 0;
}

int trie_delete_helper(TrieNode *node, const char *word, int depth) {
    if (node == NULL) return 0;
    if (word[depth] == '\0') {
        if (!node->is_end_of_word) return 0;
        node->is_end_of_word = 0;
        return !has_children(node);
    }
    int idx = word[depth] - 'a';
    if (trie_delete_helper(node->children[idx], word, depth + 1)) {
        free(node->children[idx]);
        node->children[idx] = NULL;
        return !node->is_end_of_word && !has_children(node);
    }
    return 0;
}

void trie_delete(TrieNode *root, const char *word) {
    trie_delete_helper(root, word, 0);
}

void autocomplete_helper(TrieNode *node, char *buffer, int depth) {
    if (node->is_end_of_word) {
        buffer[depth] = '\0';
        printf("  %s\n", buffer);
    }
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (node->children[i]) {
            buffer[depth] = 'a' + i;
            autocomplete_helper(node->children[i], buffer, depth + 1);
        }
    }
}

void trie_autocomplete(TrieNode *root, const char *prefix) {
    TrieNode *current = root;
    for (int i = 0; prefix[i]; i++) {
        int idx = prefix[i] - 'a';
        if (current->children[idx] == NULL) {
            printf("'%s': sin sugerencias\n", prefix);
            return;
        }
        current = current->children[idx];
    }
    char buffer[256];
    strcpy(buffer, prefix);
    printf("'%s':\n", prefix);
    autocomplete_helper(current, buffer, strlen(prefix));
}

int trie_count_words(TrieNode *root) {
    if (root == NULL) return 0;
    int count = root->is_end_of_word ? 1 : 0;
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        count += trie_count_words(root->children[i]);
    }
    return count;
}

int trie_count_nodes(TrieNode *root) {
    if (root == NULL) return 0;
    int count = 1;
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        count += trie_count_nodes(root->children[i]);
    }
    return count;
}

void trie_list_all(TrieNode *root, char *buffer, int depth) {
    if (root == NULL) return;
    if (root->is_end_of_word) {
        buffer[depth] = '\0';
        printf("  %s\n", buffer);
    }
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (root->children[i]) {
            buffer[depth] = 'a' + i;
            trie_list_all(root->children[i], buffer, depth + 1);
        }
    }
}

void trie_free(TrieNode *root) {
    if (root == NULL) return;
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        trie_free(root->children[i]);
    }
    free(root);
}

int main(void) {
    TrieNode *root = trie_create_node();

    // Insertar palabras
    const char *words[] = {
        "car", "card", "care", "careful", "cat",
        "do", "dog", "done", "door"
    };
    int n = sizeof(words) / sizeof(words[0]);

    printf("=== Insercion ===\n\n");
    for (int i = 0; i < n; i++) {
        trie_insert(root, words[i]);
        printf("Insertado: %s\n", words[i]);
    }

    printf("\nPalabras: %d\n", trie_count_words(root));
    printf("Nodos:    %d\n", trie_count_nodes(root));

    // Busquedas
    printf("\n=== Busqueda ===\n\n");
    const char *searches[] = {"car", "care", "cars", "do", "d", "cat", "cap"};
    int ns = sizeof(searches) / sizeof(searches[0]);
    for (int i = 0; i < ns; i++) {
        printf("search('%s'):      %s\n", searches[i],
               trie_search(root, searches[i]) ? "SI" : "NO");
    }

    printf("\n=== Prefijos ===\n\n");
    const char *prefixes[] = {"ca", "car", "do", "doo", "x", "c"};
    int np = sizeof(prefixes) / sizeof(prefixes[0]);
    for (int i = 0; i < np; i++) {
        printf("starts_with('%s'): %s\n", prefixes[i],
               trie_starts_with(root, prefixes[i]) ? "SI" : "NO");
    }

    // Autocompletado
    printf("\n=== Autocompletado ===\n\n");
    trie_autocomplete(root, "ca");
    trie_autocomplete(root, "car");
    trie_autocomplete(root, "do");
    trie_autocomplete(root, "z");

    // Listar todas
    printf("\n=== Todas las palabras (orden lexicografico) ===\n\n");
    char buffer[256];
    trie_list_all(root, buffer, 0);

    // Eliminacion
    printf("\n=== Eliminacion ===\n\n");
    printf("Eliminar 'car'...\n");
    trie_delete(root, "car");
    printf("search('car'):  %s\n", trie_search(root, "car") ? "SI" : "NO");
    printf("search('card'): %s\n", trie_search(root, "card") ? "SI" : "NO");
    printf("search('care'): %s\n", trie_search(root, "care") ? "SI" : "NO");

    printf("\nEliminar 'done'...\n");
    trie_delete(root, "done");
    printf("Autocompletado 'do':\n");
    trie_autocomplete(root, "do");

    printf("\nPalabras restantes: %d\n", trie_count_words(root));

    trie_free(root);
    return 0;
}
```

### Salida esperada

```
=== Insercion ===

Insertado: car
Insertado: card
Insertado: care
Insertado: careful
Insertado: cat
Insertado: do
Insertado: dog
Insertado: done
Insertado: door

Palabras: 9
Nodos:    18

=== Busqueda ===

search('car'):      SI
search('care'):     SI
search('cars'):     NO
search('do'):       SI
search('d'):        NO
search('cat'):      SI
search('cap'):      NO

=== Prefijos ===

starts_with('ca'):  SI
starts_with('car'): SI
starts_with('do'):  SI
starts_with('doo'): SI
starts_with('x'):   NO
starts_with('c'):   SI

=== Autocompletado ===

'ca':
  car
  card
  care
  careful
  cat
'car':
  car
  card
  care
  careful
'do':
  do
  dog
  done
  door
'z': sin sugerencias

=== Todas las palabras (orden lexicografico) ===

  car
  card
  care
  careful
  cat
  do
  dog
  done
  door

=== Eliminacion ===

Eliminar 'car'...
search('car'):  NO
search('card'): SI
search('care'): SI

Eliminar 'done'...
Autocompletado 'do':
'do':
  do
  dog
  door

Palabras restantes: 7
```

---

## Programa completo en Rust

```rust
const ALPHABET_SIZE: usize = 26;

struct TrieNode {
    children: [Option<Box<TrieNode>>; ALPHABET_SIZE],
    is_end_of_word: bool,
}

impl TrieNode {
    fn new() -> Self {
        TrieNode {
            children: Default::default(),
            is_end_of_word: false,
        }
    }
}

struct Trie {
    root: TrieNode,
}

impl Trie {
    fn new() -> Self {
        Trie { root: TrieNode::new() }
    }

    fn insert(&mut self, word: &str) {
        let mut current = &mut self.root;
        for ch in word.bytes() {
            let idx = (ch - b'a') as usize;
            current = current.children[idx]
                .get_or_insert_with(|| Box::new(TrieNode::new()));
        }
        current.is_end_of_word = true;
    }

    fn search(&self, word: &str) -> bool {
        self.find_node(word).map_or(false, |n| n.is_end_of_word)
    }

    fn starts_with(&self, prefix: &str) -> bool {
        self.find_node(prefix).is_some()
    }

    fn find_node(&self, s: &str) -> Option<&TrieNode> {
        let mut current = &self.root;
        for ch in s.bytes() {
            let idx = (ch - b'a') as usize;
            match &current.children[idx] {
                Some(node) => current = node,
                None => return None,
            }
        }
        Some(current)
    }

    fn autocomplete(&self, prefix: &str) -> Vec<String> {
        let mut results = Vec::new();
        if let Some(node) = self.find_node(prefix) {
            let mut buffer = prefix.to_string();
            Self::collect(node, &mut buffer, &mut results);
        }
        results
    }

    fn collect(node: &TrieNode, buffer: &mut String,
               results: &mut Vec<String>) {
        if node.is_end_of_word {
            results.push(buffer.clone());
        }
        for i in 0..ALPHABET_SIZE {
            if let Some(ref child) = node.children[i] {
                buffer.push((b'a' + i as u8) as char);
                Self::collect(child, buffer, results);
                buffer.pop();
            }
        }
    }

    fn delete(&mut self, word: &str) -> bool {
        let bytes: Vec<u8> = word.bytes().collect();
        Self::delete_helper(&mut self.root, &bytes, 0)
    }

    fn delete_helper(node: &mut TrieNode, word: &[u8], depth: usize) -> bool {
        if depth == word.len() {
            if !node.is_end_of_word { return false; }
            node.is_end_of_word = false;
            return true;
        }
        let idx = (word[depth] - b'a') as usize;
        let should_remove = if let Some(ref mut child) = node.children[idx] {
            let deleted = Self::delete_helper(child, word, depth + 1);
            deleted && !child.is_end_of_word && Self::is_empty(child)
        } else {
            return false;
        };
        if should_remove {
            node.children[idx] = None;
        }
        true
    }

    fn is_empty(node: &TrieNode) -> bool {
        node.children.iter().all(|c| c.is_none())
    }

    fn count_words(&self) -> usize {
        Self::count_helper(&self.root)
    }

    fn count_helper(node: &TrieNode) -> usize {
        let mut count = if node.is_end_of_word { 1 } else { 0 };
        for child in &node.children {
            if let Some(ref c) = child {
                count += Self::count_helper(c);
            }
        }
        count
    }

    fn list_all(&self) -> Vec<String> {
        let mut results = Vec::new();
        let mut buffer = String::new();
        Self::collect(&self.root, &mut buffer, &mut results);
        results
    }
}

fn main() {
    let mut trie = Trie::new();

    let words = ["car", "card", "care", "careful", "cat",
                 "do", "dog", "done", "door"];

    println!("=== Insercion ===\n");
    for w in &words {
        trie.insert(w);
        println!("Insertado: {w}");
    }
    println!("\nPalabras: {}", trie.count_words());

    // Busquedas
    println!("\n=== Busqueda ===\n");
    for w in &["car", "care", "cars", "do", "d", "cat", "cap"] {
        println!("search('{w}'):      {}", if trie.search(w) { "SI" } else { "NO" });
    }

    println!("\n=== Prefijos ===\n");
    for p in &["ca", "car", "do", "doo", "x", "c"] {
        println!("starts_with('{p}'): {}",
                 if trie.starts_with(p) { "SI" } else { "NO" });
    }

    // Autocompletado
    println!("\n=== Autocompletado ===\n");
    for prefix in &["ca", "car", "do", "z"] {
        let suggestions = trie.autocomplete(prefix);
        if suggestions.is_empty() {
            println!("'{prefix}': sin sugerencias");
        } else {
            println!("'{prefix}':");
            for s in &suggestions {
                println!("  {s}");
            }
        }
    }

    // Listar todas
    println!("\n=== Todas (orden lexicografico) ===\n");
    for w in trie.list_all() {
        println!("  {w}");
    }

    // Eliminacion
    println!("\n=== Eliminacion ===\n");
    println!("Eliminar 'car'...");
    trie.delete("car");
    println!("search('car'):  {}", if trie.search("car") { "SI" } else { "NO" });
    println!("search('card'): {}", if trie.search("card") { "SI" } else { "NO" });

    println!("\nEliminar 'done'...");
    trie.delete("done");
    println!("Autocompletado 'do':");
    for s in trie.autocomplete("do") {
        println!("  {s}");
    }

    println!("\nPalabras restantes: {}", trie.count_words());
}
```

### Salida

```
=== Insercion ===

Insertado: car
Insertado: card
Insertado: care
Insertado: careful
Insertado: cat
Insertado: do
Insertado: dog
Insertado: done
Insertado: door

Palabras: 9

=== Busqueda ===

search('car'):      SI
search('care'):     SI
search('cars'):     NO
search('do'):       SI
search('d'):        NO
search('cat'):      SI
search('cap'):      NO

=== Prefijos ===

starts_with('ca'):  SI
starts_with('car'): SI
starts_with('do'):  SI
starts_with('doo'): SI
starts_with('x'):   NO
starts_with('c'):   SI

=== Autocompletado ===

'ca':
  car
  card
  care
  careful
  cat
'car':
  car
  card
  care
  careful
'do':
  do
  dog
  done
  door
'z': sin sugerencias

=== Todas (orden lexicografico) ===

  car
  card
  care
  careful
  cat
  do
  dog
  done
  door

=== Eliminacion ===

Eliminar 'car'...
search('car'):  NO
search('card'): SI

Eliminar 'done'...
Autocompletado 'do':
  do
  dog
  door

Palabras restantes: 7
```

---

## Ejercicios

### Ejercicio 1 — Dibujar trie

Dibuja el trie resultante de insertar las palabras: `an`, `ant`, `any`,
`be`, `bee`, `been`, `bed`. Indica los nodos marcados como fin de palabra.

<details>
<summary>Verificar</summary>

```
         (raiz)
        /      \
       a        b
       |        |
       n *      e *
      / \      / \
     t * y *  e * d *
              |
              n *
```

Nodos marcados (*): `n` (an), `t` (ant), `y` (any), `e`-primer nivel (be),
`e`-segundo nivel (bee), `n` (been), `d` (bed).

Total: 7 palabras, 10 nodos (sin contar raíz) o 11 contando raíz.

Prefijos compartidos: `an` es prefijo de `ant` y `any`; `be` es prefijo de
`bee`, `been`, `bed`.
</details>

### Ejercicio 2 — search vs starts_with

Para el trie del Ejercicio 1, predice el resultado de cada operación:

- `search("an")`, `search("a")`, `search("bee")`, `search("bees")`
- `starts_with("an")`, `starts_with("a")`, `starts_with("bee")`,
  `starts_with("bees")`

<details>
<summary>Verificar</summary>

| Operación | Resultado | Razón |
|-----------|-----------|-------|
| `search("an")` | **SI** | `n` está marcado como fin |
| `search("a")` | **NO** | `a` existe pero no está marcado |
| `search("bee")` | **SI** | `e` (segunda) está marcado |
| `search("bees")` | **NO** | No hay hijo `s` después de `bee` |
| `starts_with("an")` | **SI** | El camino `a→n` existe |
| `starts_with("a")` | **SI** | El camino `a` existe |
| `starts_with("bee")` | **SI** | El camino `b→e→e` existe |
| `starts_with("bees")` | **NO** | No hay hijo `s` después de `bee` |

La diferencia: `search` verifica que el nodo final esté marcado;
`starts_with` solo verifica que el camino exista.

Caso clave: `search("a")` = NO pero `starts_with("a")` = SI. El carácter
`a` existe en el trie como parte de `an`, pero `a` sola no fue insertada.
</details>

### Ejercicio 3 — Traza de eliminación

En el trie del Ejercicio 1, traza paso a paso la eliminación de `been`.
¿Qué nodos se eliminan y cuáles permanecen? Después, traza la eliminación
de `bee`. ¿Cambia algo?

<details>
<summary>Verificar</summary>

**Eliminar `been`**:

1. Recorrer b → e → e → n. Llegamos al nodo `n`.
2. Desmarcar `is_end_of_word` de `n`.
3. ¿`n` tiene hijos? No. Retornar "eliminar" al padre.
4. Padre `e` (segunda): libera hijo `n`. ¿`e` es fin? Sí (`bee`). No eliminar.

Resultado: solo se elimina el nodo `n`. El nodo `e` (segunda) permanece
porque es fin de `bee`.

**Después, eliminar `bee`**:

1. Recorrer b → e → e. Llegamos al nodo `e` (segunda).
2. Desmarcar `is_end_of_word`.
3. ¿Tiene hijos? No (el `n` ya fue eliminado). Retornar "eliminar".
4. Padre `e` (primera): libera hijo `e`. ¿`e` es fin? Sí (`be`). No eliminar.

Resultado: se elimina el nodo `e` (segunda). El nodo `e` (primera) permanece
por `be` y `bed`.

Si hubiéramos eliminado `bee` primero (sin eliminar `been` antes), el nodo `e`
(segunda) NO se eliminaría porque todavía tendría hijo `n`.
</details>

### Ejercicio 4 — Contar prefijos

Implementa `count_words_with_prefix(root, prefix)` que cuente cuántas palabras
tienen un prefijo dado. Para el trie con `car`, `card`, `care`, `careful`, `cat`:
¿cuántas tienen prefijo `car`?

<details>
<summary>Verificar</summary>

```c
int trie_count_with_prefix(TrieNode *root, const char *prefix) {
    TrieNode *current = root;
    for (int i = 0; prefix[i]; i++) {
        int idx = prefix[i] - 'a';
        if (current->children[idx] == NULL) return 0;
        current = current->children[idx];
    }
    return trie_count_words(current);
}
```

En Rust:

```rust
fn count_with_prefix(&self, prefix: &str) -> usize {
    match self.find_node(prefix) {
        Some(node) => Self::count_helper(node),
        None => 0,
    }
}
```

Resultados:
- Prefijo `car`: car, card, care, careful → **4**
- Prefijo `ca`: car, card, care, careful, cat → **5**
- Prefijo `c`: los mismos 5 → **5**
- Prefijo `careful`: careful → **1**
- Prefijo `cup`: **0**
</details>

### Ejercicio 5 — Palabra más corta con prefijo único

Implementa una función que, dado un conjunto de palabras, devuelva el **prefijo
más corto** de cada una que la distingue de las demás. Por ejemplo, para
`car`, `cat`, `card`: los prefijos son `car` (no `ca` porque `cat` también
empieza con `ca`), `cat`, `card`.

<details>
<summary>Verificar</summary>

Estrategia: para cada palabra, insertar todas en un trie. Luego, para cada
palabra, recorrer el trie y detenerse cuando el conteo de palabras bajo ese
prefijo sea 1.

```c
void shortest_unique_prefix(TrieNode *root, const char *word, char *result) {
    TrieNode *current = root;
    for (int i = 0; word[i]; i++) {
        int idx = word[i] - 'a';
        current = current->children[idx];
        result[i] = word[i];
        result[i + 1] = '\0';
        if (trie_count_words(current) == 1) return;
    }
}
```

Pero `trie_count_words` en cada paso es costoso. Mejor: almacenar un contador
de paso en cada nodo (cuántas palabras pasan por él):

```c
typedef struct TrieNodeEx {
    struct TrieNodeEx *children[26];
    int is_end;
    int pass_count;  // cuantas palabras pasan por este nodo
} TrieNodeEx;
```

Incrementar `pass_count` durante la inserción. El prefijo único es el punto
donde `pass_count == 1`.

Para `car`, `cat`, `card`:
- `car` → prefijo único: `car` (en `c`: 3 pasan, en `ca`: 3, en `car`: 2
  porque `card` también pasa... necesitamos `card` → ¡`car` no basta!
  Corrección: `car` como prefijo cubre `car` y `card`. Si la pregunta es
  distinguir `car` de `card`, el prefijo es la palabra completa `car`).

Depende de la definición exacta. Si "prefijo que identifica unívocamente a la
palabra", para `car` vs `card`, `car` no es único porque es prefijo de ambas.
La respuesta completa para `car` es `car` misma (es fin de palabra y se
distingue de `card`).
</details>

### Ejercicio 6 — Autocompletado con límite

Modifica `autocomplete` para que devuelva como máximo $k$ sugerencias. Usa un
contador que se decrementa y detiene el DFS cuando llega a 0.

<details>
<summary>Verificar</summary>

```c
void autocomplete_k_helper(TrieNode *node, char *buffer, int depth,
                           int *remaining) {
    if (*remaining <= 0) return;

    if (node->is_end_of_word) {
        buffer[depth] = '\0';
        printf("  %s\n", buffer);
        (*remaining)--;
    }

    for (int i = 0; i < ALPHABET_SIZE && *remaining > 0; i++) {
        if (node->children[i]) {
            buffer[depth] = 'a' + i;
            autocomplete_k_helper(node->children[i], buffer,
                                  depth + 1, remaining);
        }
    }
}

void trie_autocomplete_k(TrieNode *root, const char *prefix, int k) {
    TrieNode *current = root;
    for (int i = 0; prefix[i]; i++) {
        int idx = prefix[i] - 'a';
        if (!current->children[idx]) {
            printf("Sin sugerencias\n");
            return;
        }
        current = current->children[idx];
    }
    char buffer[256];
    strcpy(buffer, prefix);
    int remaining = k;
    autocomplete_k_helper(current, buffer, strlen(prefix), &remaining);
}
```

En Rust:

```rust
fn autocomplete_k(&self, prefix: &str, k: usize) -> Vec<String> {
    let mut results = Vec::new();
    if let Some(node) = self.find_node(prefix) {
        let mut buffer = prefix.to_string();
        Self::collect_k(node, &mut buffer, &mut results, k);
    }
    results
}

fn collect_k(node: &TrieNode, buffer: &mut String,
             results: &mut Vec<String>, k: usize) {
    if results.len() >= k { return; }
    if node.is_end_of_word {
        results.push(buffer.clone());
    }
    for i in 0..ALPHABET_SIZE {
        if results.len() >= k { return; }
        if let Some(ref child) = node.children[i] {
            buffer.push((b'a' + i as u8) as char);
            Self::collect_k(child, buffer, results, k);
            buffer.pop();
        }
    }
}
```

`autocomplete_k("ca", 3)` → `["car", "card", "care"]` (las 3 primeras
lexicográficamente).
</details>

### Ejercicio 7 — Trie con frecuencias

Extiende el trie para almacenar la **frecuencia** de cada palabra (cuántas veces
fue insertada). Modifica `autocomplete` para devolver las sugerencias
ordenadas por frecuencia descendente.

<details>
<summary>Verificar</summary>

```c
typedef struct FreqTrieNode {
    struct FreqTrieNode *children[26];
    int frequency;  // 0 = no es palabra; >0 = frecuencia
} FreqTrieNode;

void freq_insert(FreqTrieNode *root, const char *word) {
    FreqTrieNode *current = root;
    for (int i = 0; word[i]; i++) {
        int idx = word[i] - 'a';
        if (!current->children[idx])
            current->children[idx] = calloc(1, sizeof(FreqTrieNode));
        current = current->children[idx];
    }
    current->frequency++;
}
```

Para ordenar por frecuencia en autocomplete: recolectar todas las sugerencias
con sus frecuencias y luego ordenar.

En Rust:

```rust
struct FreqTrie { root: FreqNode }
struct FreqNode {
    children: [Option<Box<FreqNode>>; 26],
    frequency: u32,
}

impl FreqTrie {
    fn autocomplete_sorted(&self, prefix: &str) -> Vec<(String, u32)> {
        let mut results = Vec::new();
        if let Some(node) = self.find_node(prefix) {
            let mut buf = prefix.to_string();
            Self::collect_freq(node, &mut buf, &mut results);
        }
        results.sort_by(|a, b| b.1.cmp(&a.1)); // frecuencia descendente
        results
    }
}
```

Ejemplo: insertar "car" 5 veces, "cat" 10 veces, "card" 2 veces.
`autocomplete("ca")` → `[("cat", 10), ("car", 5), ("card", 2)]`.
</details>

### Ejercicio 8 — Reemplazar diccionario

Dado un conjunto de raíces (roots) y una oración, reemplaza cada palabra de la
oración por su raíz más corta si existe. Ejemplo:
roots = `["cat", "bat", "rat"]`, oración = `"the cattle was rattled by the battery"`,
resultado = `"the cat was rat by the bat"`.

<details>
<summary>Verificar</summary>

```rust
fn replace_words(roots: &[&str], sentence: &str) -> String {
    let mut trie = Trie::new();
    for root in roots {
        trie.insert(root);
    }

    sentence.split_whitespace()
        .map(|word| {
            let mut current = &trie.root;
            for (i, ch) in word.bytes().enumerate() {
                let idx = (ch - b'a') as usize;
                match &current.children[idx] {
                    None => return word.to_string(),
                    Some(node) => {
                        current = node;
                        if current.is_end_of_word {
                            return word[..=i].to_string();
                        }
                    }
                }
            }
            word.to_string()
        })
        .collect::<Vec<_>>()
        .join(" ")
}
```

Traza para "cattle":
- c → a → t (es fin de "cat") → devolver "cat"

Traza para "rattled":
- r → a → t (es fin de "rat") → devolver "rat"

Traza para "the":
- t → h → no hay "th" como raíz → devolver "the"

Resultado: `"the cat was rat by the bat"` ✓

Este es el problema LeetCode 648: Replace Words.
</details>

### Ejercicio 9 — Contar nodos vs palabras

Un trie con las palabras `a`, `ab`, `abc`, `abcd`, `abcde` tiene 5 palabras
pero ¿cuántos nodos? ¿Qué pasa con `a`, `b`, `c`, `d`, `e` (5 palabras
completamente disjuntas)? Analiza la relación entre compartición de prefijos
y número de nodos.

<details>
<summary>Verificar</summary>

**Caso 1**: `a`, `ab`, `abc`, `abcd`, `abcde` (cadena de prefijos):

```
raiz → a* → b* → c* → d* → e*
```

Nodos: **5** (más la raíz = 6). Máxima compartición: cada palabra extiende
la anterior.

**Caso 2**: `a`, `b`, `c`, `d`, `e` (completamente disjuntas):

```
       raiz
     / | | | \
    a* b* c* d* e*
```

Nodos: **5** (más la raíz = 6). Sin compartición pero palabras de longitud 1.

**Caso 3**: `abcde`, `fghij`, `klmno`, `pqrst`, `uvwxy` (disjuntas largas):

Nodos: $5 \times 5 = 25$ (más la raíz = 26). Sin compartición, cada palabra
contribuye todos sus nodos.

**Relación**: si $L$ es la suma de longitudes de todas las palabras, el número
de nodos está entre $\max(|w_i|)$ (máxima compartición, una cadena de prefijos)
y $L$ (sin compartición). Para un diccionario real, la compartición es
sustancial: palabras como "compute", "computer", "computing", "computation"
comparten "comput" (6 de 7-11 caracteres).
</details>

### Ejercicio 10 — Trie de bits (binary trie)

Un **trie binario** almacena números usando su representación binaria (alfabeto
= {0, 1}). Implementa un trie binario que soporte `insert(num)` y
`max_xor(num)` — encontrar el número en el trie cuyo XOR con `num` sea máximo.

<details>
<summary>Verificar</summary>

Idea: para maximizar XOR, en cada bit queremos ir por el camino opuesto al bit
de `num`. Si `num` tiene bit 1, queremos ir por 0, y viceversa.

```rust
struct BitTrieNode {
    children: [Option<Box<BitTrieNode>>; 2],
}

impl BitTrieNode {
    fn new() -> Self {
        BitTrieNode { children: [None, None] }
    }
}

struct BitTrie {
    root: BitTrieNode,
}

impl BitTrie {
    fn new() -> Self {
        BitTrie { root: BitTrieNode::new() }
    }

    fn insert(&mut self, num: u32) {
        let mut current = &mut self.root;
        for i in (0..32).rev() {
            let bit = ((num >> i) & 1) as usize;
            current = current.children[bit]
                .get_or_insert_with(|| Box::new(BitTrieNode::new()));
        }
    }

    fn max_xor(&self, num: u32) -> u32 {
        let mut current = &self.root;
        let mut result = 0u32;

        for i in (0..32).rev() {
            let bit = ((num >> i) & 1) as usize;
            let preferred = 1 - bit;  // bit opuesto

            if current.children[preferred].is_some() {
                result |= 1 << i;
                current = current.children[preferred].as_ref().unwrap();
            } else {
                current = current.children[bit].as_ref().unwrap();
            }
        }
        result
    }
}

// Ejemplo:
// insert(3)  -> 011
// insert(10) -> 1010
// insert(5)  -> 0101
// max_xor(8) -> 8=1000, XOR con 3=011=1011=11,
//               XOR con 10=1010=0010=2,
//               XOR con 5=0101=1101=13
// Resultado: 13 (8 XOR 5)
```

Complejidad: $O(32)$ = $O(1)$ para enteros de 32 bits. El trie binario
convierte un problema que sería $O(n)$ por fuerza bruta en $O(\log(\text{max\_val}))$.

Este es el problema LeetCode 421: Maximum XOR of Two Numbers in an Array.
</details>
