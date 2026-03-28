# Tries revisitado: búsqueda por prefijo y compresión

## Objetivo

Revisitar el trie desde la perspectiva de **estructura de búsqueda**,
enfocándose en aspectos no cubiertos en C06/S05/T04 (donde se
implementaron las operaciones básicas):

- Búsqueda $O(m)$ **independiente de** $n$ — análisis detallado de
  por qué y cuándo esto supera a BST, hash y B-tree.
- **Prefix matching** como primitiva de búsqueda: autocompletado,
  filtrado, longest prefix match.
- **Patricia tries** (compresión de caminos): reducción drástica de
  nodos y memoria.
- **Radix trees** generalizados: compresión por bloques de bits.
- Comparación de rendimiento empírico con las estructuras de
  búsqueda vistas en C09.

Referencia: la implementación básica del trie (inserción, búsqueda,
eliminación, autocompletado) está en **C06/S05/T04**. Aquí no se
repiten esas operaciones; el foco es el análisis de búsqueda y las
variantes comprimidas.

---

## $O(m)$ vs $O(m \log n)$: cuándo importa

En un BST balanceado con $n$ cadenas de longitud promedio $m$,
buscar una cadena cuesta $O(m \log n)$: hay $O(\log n)$
comparaciones de cadenas, y cada comparación recorre hasta $m$
caracteres.

En un trie, buscar cuesta $O(m)$ — se recorre la cadena una sola
vez, carácter por carácter, independientemente de cuántas cadenas
haya almacenadas.

### Impacto numérico

| $n$ (cadenas) | $m$ (longitud) | BST $O(m \log n)$ | Trie $O(m)$ | Speedup |
|---------:|---------:|---------:|---------:|---------:|
| 1,000 | 10 | 100 ops | 10 ops | 10x |
| 100,000 | 10 | 170 ops | 10 ops | 17x |
| 10,000,000 | 10 | 230 ops | 10 ops | 23x |
| 1,000 | 100 | 1,000 ops | 100 ops | 10x |
| 10,000,000 | 100 | 2,300 ops | 100 ops | 23x |

Para diccionarios grandes ($n > 10^5$) con cadenas cortas
($m < 20$), la ventaja es significativa.

### Pero: la constante importa

El trie accede a un nodo por carácter. Con la representación de
array fijo (26 punteros por nodo), cada nodo ocupa 216 bytes.
Para una cadena de 10 caracteres: 10 accesos a memoria posiblemente
no contiguos = ~10 cache misses potenciales.

Un BST que almacena cadenas completas puede tener mejor localidad:
las cadenas cortas caben en el mismo nodo, y hay menos nodos
totales.

| Estructura | Accesos a nodo ($m=10$, $n=10^5$) | Cache misses estimados |
|------------|-------:|-------:|
| Trie (array) | 10 | ~8-10 |
| BST balanceado | ~17 | ~12-15 |
| Hash table | 1 (+ hash) | ~2-3 |
| B-tree ($t=50$) | ~3 | ~3 |

La hash table gana en búsqueda puntual, pero no soporta búsqueda
por prefijo. El B-tree gana si las claves son comparables como
bloques y no se necesita prefix matching.

### Cuándo el trie gana decisivamente

1. **Prefix matching**: ninguna otra estructura lo hace en $O(m)$.
2. **Diccionarios muy grandes** ($n > 10^6$): el $\log n$ del BST
   se vuelve significativo.
3. **Claves con muchos prefijos compartidos**: el trie comparte
   nodos, reduciendo memoria efectiva.
4. **Longest prefix match**: fundamental en networking (IP routing).

---

## Prefix matching como primitiva de búsqueda

La búsqueda por prefijo no es una funcionalidad auxiliar del trie
— es su **operación fundamental**, la que justifica su existencia
frente a hash tables y árboles balanceados.

### Operaciones de prefix matching

**1. Exists prefix** — ¿existe alguna palabra con este prefijo?
$O(m)$, no necesita DFS.

**2. Autocomplete** — listar todas las palabras con un prefijo.
$O(m + k)$ donde $k$ es el número de resultados. Cubierto en
C06/S05/T04.

**3. Count prefix** — ¿cuántas palabras tienen este prefijo?
$O(m)$ si cada nodo almacena un contador de palabras en su
subárbol.

**4. Longest prefix match (LPM)** — dada una cadena, encontrar
la palabra más larga del diccionario que es prefijo de esa cadena.

### Longest prefix match

Esta operación es crucial en:
- **IP routing**: la tabla de rutas contiene prefijos de red
  (`192.168.0.0/16`, `192.168.1.0/24`). Para un paquete con IP
  destino, se busca la ruta más específica (prefijo más largo).
- **URL routing** en servidores web.
- **Tokenización** en compiladores y NLP.

```
Diccionario: {"a", "ab", "abc", "b", "bc"}

longest_prefix_match("abcd") → "abc"
longest_prefix_match("abd")  → "ab"
longest_prefix_match("bcd")  → "bc"
longest_prefix_match("xyz")  → NULL (no hay prefijo)
```

Algoritmo:

```
LONGEST_PREFIX_MATCH(root, text):
    node = root
    last_match = -1        // longitud del ultimo prefijo encontrado

    for i = 0 to len(text) - 1:
        index = text[i] - 'a'
        if node.children[index] == NULL:
            break
        node = node.children[index]
        if node.is_end_of_word:
            last_match = i + 1

    return last_match      // -1 si no hay match
```

Costo: $O(m)$ donde $m$ es la longitud del texto, o menos si se
encuentra un carácter sin hijo antes de terminar.

### Count prefix con preprocesamiento

Agregar un campo `prefix_count` a cada nodo que almacene cuántas
palabras pasan por ese nodo:

```c
typedef struct TrieNode {
    struct TrieNode *children[26];
    int is_end_of_word;
    int prefix_count;      // palabras que tienen este prefijo
} TrieNode;
```

Al insertar, incrementar `prefix_count` en cada nodo del camino.
Luego, `count_with_prefix("ca")` es simplemente navegar hasta el
nodo de `a` (hijo de `c`) y retornar `prefix_count`. Costo:
$O(m)$ sin DFS.

---

## Patricia tries (compresión de caminos)

### El problema del trie estándar

Un trie estándar crea un nodo por carácter. Para la palabra
"international" (13 caracteres), si no comparte prefijo con ninguna
otra palabra, hay **13 nodos** con un solo hijo cada uno. Esto es
un desperdicio de memoria y cache.

### La idea: comprimir cadenas de nodos con un solo hijo

Un **Patricia trie** (Practical Algorithm To Retrieve Information
Coded In Alphanumeric — Morrison, 1968) comprime las cadenas de
nodos unarios en un solo nodo que almacena la subcadena completa:

```
Trie estandar para {"internet", "internal", "inter", "integer"}:

    i
    |
    n
    |
    t
    |
    e
   / \
  r   g
  |   |
  n   e
 / \  |
a   e  r *
|   |
l * t *
    (internet)

Patricia trie para las mismas palabras:

    "int"
    /   \
  "e"    "eger" *
  /  \
"rn"  — (impossible, let me redo)
```

Veámoslo con un ejemplo más claro. Trie estándar para
`{"test", "team", "toast"}`:

```
Trie estandar:
      t
     / \
    e    o
   / \    \
  s   a    a
  |   |    |
  t * m *  s
           |
           t *
```

Patricia trie (comprimir caminos lineales):

```
Patricia trie:
        "t"
       /    \
    "e"     "oast" *
   /    \
 "st" * "am" *
```

Los nodos `o → a → s → t` se comprimen en un solo nodo `"oast"`.
El nodo `s → t` se comprime en `"st"`. El nodo `a → m` se
comprime en `"am"`.

### Ahorro de nodos

| Ejemplo | Nodos en trie estándar | Nodos en Patricia | Reducción |
|---------|-------:|-------:|-------:|
| `{"test", "team", "toast"}` | 10 | 6 | 40% |
| `{"ab", "abcd", "abcde", "abcdf"}` | 7 | 4 | 43% |
| Diccionario inglés (~100K palabras) | ~400K | ~200K | ~50% |
| URLs típicas | ~80% nodos unarios | mucho menor | ~70% |

### Estructura del nodo Patricia

```c
typedef struct PatriciaNode {
    char *label;                     // subcadena comprimida
    struct PatriciaNode *children[26];
    int is_end_of_word;
} PatriciaNode;
```

Alternativamente, para evitar almacenar la subcadena directamente
(que requiere allocation por nodo), se puede almacenar un **índice
y longitud** dentro de una cadena original:

```c
typedef struct PatriciaNode {
    int start;       // inicio de la subcadena en la cadena original
    int len;         // longitud de la subcadena
    struct PatriciaNode *children[26];
    int is_end_of_word;
} PatriciaNode;
```

### Búsqueda en Patricia trie

La búsqueda compara la subcadena del nodo con el segmento
correspondiente de la clave buscada:

```
PATRICIA_SEARCH(node, key, pos):
    if node is NULL:
        return NOT_FOUND

    // comparar label del nodo con key[pos..pos+len]
    for i = 0 to node.label_len - 1:
        if pos + i >= len(key):
            return NOT_FOUND         // key es mas corta que el label
        if key[pos + i] != node.label[i]:
            return NOT_FOUND         // mismatch

    pos += node.label_len

    if pos == len(key):
        return node.is_end_of_word ? FOUND : NOT_FOUND

    index = key[pos] - 'a'
    return PATRICIA_SEARCH(node.children[index], key, pos)
```

**Traza**: buscar `"team"` en el Patricia trie de arriba:

```
Paso 1: nodo raiz, label = "t"
        key[0] = 't' == 't' ✓
        pos = 1

Paso 2: key[1] = 'e', ir a hijo 'e'
        nodo label = "e"
        key[1] = 'e' == 'e' ✓
        pos = 2

Paso 3: key[2] = 'a', ir a hijo 'a'
        nodo label = "am"
        key[2] = 'a' == 'a' ✓
        key[3] = 'm' == 'm' ✓
        pos = 4 == len("team")
        is_end_of_word = true → ENCONTRADO
```

**Traza**: buscar `"tear"`:

```
Paso 1: nodo "t", key[0]='t' ✓, pos=1
Paso 2: hijo 'e', nodo "e", key[1]='e' ✓, pos=2
Paso 3: hijo 'a', nodo "am"
        key[2]='a' == 'a' ✓
        key[3]='r' != 'm' ✗ → NO ENCONTRADO
```

### Inserción en Patricia trie

La inserción puede requerir **partir un nodo** cuando la nueva
clave diverge en medio de un label comprimido:

Insertar `"tea"` en el Patricia trie que contiene `"team"`:

```
Antes:
  ...→ "am" *     (nodo para "team")

Despues:
  ...→ "a" *      (nodo para "tea", partido)
        |
       "m" *      (nodo para "team")
```

El nodo `"am"` se parte en `"a"` + `"m"`. El nodo `"a"` se marca
como fin de palabra.

Algoritmo general:

```
PATRICIA_INSERT(node, key, pos):
    if node is NULL:
        crear nodo con label = key[pos..]
        marcar como fin de palabra
        return

    // comparar label con key[pos..]
    mismatch = primer indice donde label[i] != key[pos+i]

    if mismatch < label_len:
        // split: partir el nodo en el punto de divergencia
        nuevo_hijo = crear nodo con label = label[mismatch+1..]
        copiar hijos y is_end del nodo actual a nuevo_hijo

        nodo.label = label[0..mismatch]
        limpiar hijos del nodo
        nodo.children[label[mismatch]] = nuevo_hijo

        if pos + mismatch == len(key):
            nodo.is_end_of_word = true
        else:
            nodo.children[key[pos+mismatch]] = crear hoja(key[pos+mismatch..])

    else if pos + label_len == len(key):
        nodo.is_end_of_word = true

    else:
        // el label matchea completo, descender
        index = key[pos + label_len] - 'a'
        PATRICIA_INSERT(nodo.children[index], key, pos + label_len)
```

### Complejidad

| Operación | Trie estándar | Patricia trie |
|-----------|:-------:|:-------:|
| Búsqueda | $O(m)$ | $O(m)$ |
| Inserción | $O(m)$ | $O(m)$ |
| Eliminación | $O(m)$ | $O(m)$ (+ posible merge de nodos) |
| Memoria (nodos) | $O(n \cdot m)$ peor caso | $O(n)$ (a lo sumo $2n - 1$ nodos) |
| Prefix match | $O(m + k)$ | $O(m + k)$ |

El resultado clave: un Patricia trie con $n$ palabras tiene **a lo
sumo** $2n - 1$ nodos (cada palabra añade a lo sumo 2 nodos: un
split y una hoja). En un trie estándar, el peor caso es $n \cdot m$
nodos.

---

## Radix tree (trie de base $r$)

Un **radix tree** generaliza la compresión del Patricia trie. En
vez de branching por carácter individual, se agrupa la clave en
**bloques de** $b$ **bits** y se hace branching por bloque.

### Ejemplo: radix-4 tree (2 bits por nivel)

Para claves de 8 bits, un radix-4 tree tiene 4 niveles (8 bits /
2 bits por nivel) con 4 hijos por nodo ($2^2 = 4$):

```
Clave 10110100 (180 decimal):

Nivel 1: bits 7-6 = 10 → hijo 2
Nivel 2: bits 5-4 = 11 → hijo 3
Nivel 3: bits 3-2 = 01 → hijo 1
Nivel 4: bits 1-0 = 00 → hijo 0
```

### Radix trees en la práctica

| Aplicación | Tipo | Clave |
|------------|------|-------|
| Linux kernel (page cache) | Radix tree | dirección de página |
| IP routing (longest prefix) | Patricia trie | dirección IP (32/128 bits) |
| DNS resolvers | Trie comprimido | nombre de dominio invertido |
| Git object store | Radix-256 (por byte) | hash SHA |

### Linux kernel radix tree

El kernel de Linux usa un radix tree para mapear `page index →
struct page *` en el page cache. Cada nodo tiene
$2^6 = 64$ hijos (6 bits por nivel). Para un espacio de
direcciones de 64 bits, la altura máxima es $\lceil 64/6 \rceil
= 11$, pero en la práctica la mayoría de los accesos se resuelven
en 3-4 niveles porque las direcciones altas no se usan.

Desde Linux 4.20, la estructura se renombró a **XArray** (eXtensible
Array), pero la estructura subyacente sigue siendo un radix tree.

---

## Trie vs otras estructuras de búsqueda: resumen

| Criterio | Trie | Patricia | Hash | BST/AVL | B-tree |
|----------|:----:|:--------:|:----:|:-------:|:------:|
| Búsqueda exacta | $O(m)$ | $O(m)$ | $O(m)$ avg | $O(m \log n)$ | $O(m \log_t n)$ |
| Prefix search | $O(m+k)$ | $O(m+k)$ | $O(n \cdot m)$ | $O(m \log n + k)$ | $O(m \log_t n + k)$ |
| Longest prefix | $O(m)$ | $O(m)$ | no eficiente | no eficiente | no eficiente |
| Range (lex) | $O(m+k)$ | $O(m+k)$ | no soporta | $O(m \log n + k)$ | $O(m \log_t n + k)$ |
| Memoria | alta | media | baja | baja | baja |
| Cache locality | pobre | mejor | buena | pobre | buena |
| Inserción | $O(m)$ | $O(m)$ | $O(m)$ avg | $O(m \log n)$ | $O(m \log_t n)$ |
| Depende de $n$ | no | no | resize | sí | sí |

### Regla de decisión

- **¿Necesitas prefix search?** → trie o Patricia.
- **¿Necesitas longest prefix match?** → trie o Patricia.
- **¿Solo búsqueda exacta, datos en RAM?** → hash table.
- **¿Búsqueda exacta + rango, datos en disco?** → B-tree / B+ tree.
- **¿Búsqueda exacta + rango, datos en RAM?** → BST balanceado
  o BTreeMap.
- **¿Memoria es crítica?** → Patricia trie sobre trie estándar;
  hash table sobre ambos si no necesitas prefijos.

---

## Programa completo en C

```c
// trie_search.c — trie and patricia trie: search operations
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define ALPHA 26

// =============================================
// Part 1: Trie with prefix_count and LPM
// =============================================

typedef struct TrieNode {
    struct TrieNode *children[ALPHA];
    bool is_end;
    int prefix_count;  // words passing through this node
} TrieNode;

TrieNode *trie_create(void) {
    return calloc(1, sizeof(TrieNode));
}

void trie_insert(TrieNode *root, const char *word) {
    TrieNode *cur = root;
    for (int i = 0; word[i]; i++) {
        int idx = word[i] - 'a';
        if (!cur->children[idx])
            cur->children[idx] = trie_create();
        cur = cur->children[idx];
        cur->prefix_count++;
    }
    cur->is_end = true;
}

bool trie_search(TrieNode *root, const char *word) {
    TrieNode *cur = root;
    for (int i = 0; word[i]; i++) {
        int idx = word[i] - 'a';
        if (!cur->children[idx]) return false;
        cur = cur->children[idx];
    }
    return cur->is_end;
}

int trie_count_prefix(TrieNode *root, const char *prefix) {
    TrieNode *cur = root;
    for (int i = 0; prefix[i]; i++) {
        int idx = prefix[i] - 'a';
        if (!cur->children[idx]) return 0;
        cur = cur->children[idx];
    }
    return cur->prefix_count;
}

// longest prefix match: find longest word that is prefix of text
int trie_lpm(TrieNode *root, const char *text) {
    TrieNode *cur = root;
    int last_match = -1;
    for (int i = 0; text[i]; i++) {
        int idx = text[i] - 'a';
        if (!cur->children[idx]) break;
        cur = cur->children[idx];
        if (cur->is_end)
            last_match = i + 1;
    }
    return last_match;
}

// autocomplete with limit
void autocomplete_helper(TrieNode *node, char *buf, int depth, int *count, int limit) {
    if (*count >= limit) return;
    if (node->is_end) {
        buf[depth] = '\0';
        printf("  %s\n", buf);
        (*count)++;
    }
    for (int i = 0; i < ALPHA && *count < limit; i++) {
        if (node->children[i]) {
            buf[depth] = 'a' + i;
            autocomplete_helper(node->children[i], buf, depth + 1, count, limit);
        }
    }
}

void trie_autocomplete(TrieNode *root, const char *prefix, int limit) {
    TrieNode *cur = root;
    for (int i = 0; prefix[i]; i++) {
        int idx = prefix[i] - 'a';
        if (!cur->children[idx]) {
            printf("  (no suggestions)\n");
            return;
        }
        cur = cur->children[idx];
    }
    char buf[256];
    strcpy(buf, prefix);
    int count = 0;
    autocomplete_helper(cur, buf, (int)strlen(prefix), &count, limit);
}

void trie_free(TrieNode *root) {
    if (!root) return;
    for (int i = 0; i < ALPHA; i++)
        trie_free(root->children[i]);
    free(root);
}

int trie_count_nodes(TrieNode *root) {
    if (!root) return 0;
    int count = 1;
    for (int i = 0; i < ALPHA; i++)
        count += trie_count_nodes(root->children[i]);
    return count;
}

// =============================================
// Part 2: Patricia trie (compressed trie)
// =============================================

typedef struct PatNode {
    char *label;    // compressed edge label
    int label_len;
    struct PatNode *children[ALPHA];
    bool is_end;
} PatNode;

PatNode *pat_create(const char *label, int len) {
    PatNode *node = calloc(1, sizeof(PatNode));
    node->label = malloc(len + 1);
    memcpy(node->label, label, len);
    node->label[len] = '\0';
    node->label_len = len;
    return node;
}

void pat_insert(PatNode *root, const char *word) {
    PatNode *cur = root;
    int pos = 0;
    int wlen = (int)strlen(word);

    while (pos < wlen) {
        int idx = word[pos] - 'a';

        if (!cur->children[idx]) {
            // create new leaf with remaining string
            cur->children[idx] = pat_create(word + pos, wlen - pos);
            cur->children[idx]->is_end = true;
            return;
        }

        PatNode *child = cur->children[idx];

        // find mismatch between child label and word[pos..]
        int match_len = 0;
        while (match_len < child->label_len &&
               pos + match_len < wlen &&
               child->label[match_len] == word[pos + match_len]) {
            match_len++;
        }

        if (match_len == child->label_len) {
            // full label matched, descend
            pos += match_len;
            cur = child;
            if (pos == wlen) {
                cur->is_end = true;
                return;
            }
            continue;
        }

        // mismatch: split the child node
        PatNode *split = pat_create(child->label, match_len);

        // adjust child: remove matched prefix
        char *old_label = child->label;
        int old_len = child->label_len;
        child->label = malloc(old_len - match_len + 1);
        memcpy(child->label, old_label + match_len, old_len - match_len);
        child->label[old_len - match_len] = '\0';
        child->label_len = old_len - match_len;
        free(old_label);

        // split takes over child's position
        split->children[child->label[0] - 'a'] = child;
        cur->children[idx] = split;

        if (pos + match_len == wlen) {
            split->is_end = true;
        } else {
            // create new leaf for remaining
            PatNode *leaf = pat_create(word + pos + match_len,
                                       wlen - pos - match_len);
            leaf->is_end = true;
            split->children[word[pos + match_len] - 'a'] = leaf;
        }
        return;
    }

    cur->is_end = true;
}

bool pat_search(PatNode *root, const char *word) {
    PatNode *cur = root;
    int pos = 0;
    int wlen = (int)strlen(word);

    while (pos < wlen) {
        int idx = word[pos] - 'a';
        if (!cur->children[idx]) return false;

        PatNode *child = cur->children[idx];

        // compare label
        if (wlen - pos < child->label_len) return false;
        if (memcmp(child->label, word + pos, child->label_len) != 0)
            return false;

        pos += child->label_len;
        cur = child;
    }
    return cur->is_end;
}

int pat_count_nodes(PatNode *root) {
    if (!root) return 0;
    int count = 1;
    for (int i = 0; i < ALPHA; i++)
        count += pat_count_nodes(root->children[i]);
    return count;
}

void pat_print(PatNode *node, int depth) {
    if (!node) return;
    printf("%*s\"%s\"%s\n", depth * 2, "",
           node->label_len > 0 ? node->label : "(root)",
           node->is_end ? " *" : "");
    for (int i = 0; i < ALPHA; i++)
        pat_print(node->children[i], depth + 1);
}

void pat_free(PatNode *node) {
    if (!node) return;
    for (int i = 0; i < ALPHA; i++)
        pat_free(node->children[i]);
    free(node->label);
    free(node);
}

// =============================================
// Demos
// =============================================

int main(void) {
    // demo 1: prefix count
    printf("=== Demo 1: prefix count ===\n");
    TrieNode *trie = trie_create();
    const char *words[] = {
        "car", "card", "care", "careful", "cars",
        "cat", "category", "catalog",
        "do", "dog", "done", "door"
    };
    int nwords = sizeof(words) / sizeof(words[0]);

    for (int i = 0; i < nwords; i++)
        trie_insert(trie, words[i]);

    printf("Words with prefix 'car': %d\n", trie_count_prefix(trie, "car"));
    printf("Words with prefix 'cat': %d\n", trie_count_prefix(trie, "cat"));
    printf("Words with prefix 'ca':  %d\n", trie_count_prefix(trie, "ca"));
    printf("Words with prefix 'do':  %d\n", trie_count_prefix(trie, "do"));
    printf("Words with prefix 'z':   %d\n", trie_count_prefix(trie, "z"));
    printf("\n");

    // demo 2: longest prefix match
    printf("=== Demo 2: longest prefix match ===\n");
    TrieNode *routes = trie_create();
    // simulate IP-like prefix routing with words
    trie_insert(routes, "app");
    trie_insert(routes, "apple");
    trie_insert(routes, "application");
    trie_insert(routes, "api");
    trie_insert(routes, "apply");

    const char *queries[] = {"applesauce", "applying", "apartment",
                              "api", "appx", "application"};
    for (int i = 0; i < 6; i++) {
        int len = trie_lpm(routes, queries[i]);
        if (len > 0)
            printf("LPM(\"%s\") = \"%.*s\"\n", queries[i], len, queries[i]);
        else
            printf("LPM(\"%s\") = (none)\n", queries[i]);
    }
    printf("\n");

    // demo 3: autocomplete with limit
    printf("=== Demo 3: autocomplete (max 5) ===\n");
    printf("Autocomplete 'car':\n");
    trie_autocomplete(trie, "car", 5);
    printf("Autocomplete 'cat':\n");
    trie_autocomplete(trie, "cat", 5);
    printf("Autocomplete 'do':\n");
    trie_autocomplete(trie, "do", 5);
    printf("\n");

    // demo 4: patricia trie vs standard trie
    printf("=== Demo 4: Patricia trie ===\n");
    PatNode *patricia = pat_create("", 0);

    for (int i = 0; i < nwords; i++)
        pat_insert(patricia, words[i]);

    printf("Structure:\n");
    pat_print(patricia, 0);
    printf("\n");

    printf("Search results:\n");
    const char *search_words[] = {"car", "card", "care", "cart", "cat", "dog", "xyz"};
    for (int i = 0; i < 7; i++) {
        printf("  search(\"%s\"): %s\n", search_words[i],
               pat_search(patricia, search_words[i]) ? "FOUND" : "NOT FOUND");
    }
    printf("\n");

    // demo 5: node count comparison
    printf("=== Demo 5: node count comparison ===\n");
    printf("Standard trie nodes: %d\n", trie_count_nodes(trie));
    printf("Patricia trie nodes: %d\n", pat_count_nodes(patricia));
    printf("Words stored: %d\n", nwords);
    printf("Reduction: %.0f%%\n",
           100.0 * (1.0 - (double)pat_count_nodes(patricia) /
                           trie_count_nodes(trie)));
    printf("\n");

    // demo 6: memory comparison
    printf("=== Demo 6: memory estimate ===\n");
    int trie_nodes = trie_count_nodes(trie);
    int pat_nodes = pat_count_nodes(patricia);
    // trie: 26 pointers + bool + int = 26*8 + 1 + 4 = 213 bytes/node
    // patricia: 26 pointers + bool + pointer + int + int = ~230 bytes/node
    // but patricia has far fewer nodes
    printf("Trie:     %d nodes * ~216 bytes = ~%d bytes\n",
           trie_nodes, trie_nodes * 216);
    printf("Patricia: %d nodes * ~230 bytes = ~%d bytes\n",
           pat_nodes, pat_nodes * 230);
    printf("Patricia uses ~%.0f%% of trie memory\n",
           100.0 * (pat_nodes * 230.0) / (trie_nodes * 216.0));

    trie_free(trie);
    trie_free(routes);
    pat_free(patricia);

    return 0;
}
```

Compilar y ejecutar:

```bash
gcc -std=c11 -Wall -Wextra -o trie_search trie_search.c
./trie_search
```

---

## Programa completo en Rust

```rust
// trie_search.rs — trie search operations and comparison
use std::collections::HashMap;

// =============================================
// Part 1: Trie with prefix_count and LPM
// =============================================

struct TrieNode {
    children: [Option<Box<TrieNode>>; 26],
    is_end: bool,
    prefix_count: u32,
}

impl TrieNode {
    fn new() -> Self {
        TrieNode {
            children: Default::default(),
            is_end: false,
            prefix_count: 0,
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
        let mut node = &mut self.root;
        for b in word.bytes() {
            let idx = (b - b'a') as usize;
            node = node.children[idx].get_or_insert_with(|| Box::new(TrieNode::new()));
            node.prefix_count += 1;
        }
        node.is_end = true;
    }

    fn search(&self, word: &str) -> bool {
        let mut node = &self.root;
        for b in word.bytes() {
            let idx = (b - b'a') as usize;
            match &node.children[idx] {
                Some(child) => node = child,
                None => return false,
            }
        }
        node.is_end
    }

    fn count_prefix(&self, prefix: &str) -> u32 {
        let mut node = &self.root;
        for b in prefix.bytes() {
            let idx = (b - b'a') as usize;
            match &node.children[idx] {
                Some(child) => node = child,
                None => return 0,
            }
        }
        node.prefix_count
    }

    fn longest_prefix_match<'a>(&self, text: &'a str) -> Option<&'a str> {
        let mut node = &self.root;
        let mut last_match = None;
        for (i, b) in text.bytes().enumerate() {
            let idx = (b - b'a') as usize;
            match &node.children[idx] {
                Some(child) => {
                    node = child;
                    if node.is_end {
                        last_match = Some(&text[..=i]);
                    }
                }
                None => break,
            }
        }
        last_match
    }

    fn autocomplete(&self, prefix: &str, limit: usize) -> Vec<String> {
        let mut node = &self.root;
        for b in prefix.bytes() {
            let idx = (b - b'a') as usize;
            match &node.children[idx] {
                Some(child) => node = child,
                None => return vec![],
            }
        }
        let mut results = Vec::new();
        let mut buf = prefix.to_string();
        Self::collect(node, &mut buf, &mut results, limit);
        results
    }

    fn collect(node: &TrieNode, buf: &mut String, results: &mut Vec<String>, limit: usize) {
        if results.len() >= limit { return; }
        if node.is_end {
            results.push(buf.clone());
        }
        for i in 0..26 {
            if let Some(child) = &node.children[i] {
                buf.push((b'a' + i as u8) as char);
                Self::collect(child, buf, results, limit);
                buf.pop();
                if results.len() >= limit { return; }
            }
        }
    }

    fn count_nodes(&self) -> usize {
        Self::count_recursive(&self.root)
    }

    fn count_recursive(node: &TrieNode) -> usize {
        let mut count = 1;
        for child in &node.children {
            if let Some(c) = child {
                count += Self::count_recursive(c);
            }
        }
        count
    }
}

// =============================================
// Part 2: Patricia trie (compressed)
// =============================================

struct PatriciaNode {
    label: String,
    children: HashMap<u8, Box<PatriciaNode>>,
    is_end: bool,
}

impl PatriciaNode {
    fn new(label: &str) -> Self {
        PatriciaNode {
            label: label.to_string(),
            children: HashMap::new(),
            is_end: false,
        }
    }

    fn insert(node: &mut PatriciaNode, word: &str) {
        if word.is_empty() {
            node.is_end = true;
            return;
        }

        let first = word.as_bytes()[0];

        if let Some(child) = node.children.get_mut(&first) {
            // find common prefix length
            let common = child.label.bytes()
                .zip(word.bytes())
                .take_while(|(a, b)| a == b)
                .count();

            if common == child.label.len() {
                // full match of label, descend
                Self::insert(child, &word[common..]);
            } else {
                // split needed
                let mut split = PatriciaNode::new(&child.label[..common]);

                // adjust existing child
                let old_label = child.label[common..].to_string();
                child.label = old_label;

                // take child out, put under split
                let old_first = child.label.as_bytes()[0];
                let taken = node.children.remove(&first).unwrap();
                split.children.insert(old_first, taken);

                if common == word.len() {
                    split.is_end = true;
                } else {
                    let remaining = &word[common..];
                    let mut leaf = PatriciaNode::new(remaining);
                    leaf.is_end = true;
                    split.children.insert(remaining.as_bytes()[0], Box::new(leaf));
                }

                node.children.insert(first, Box::new(split));
            }
        } else {
            let mut leaf = PatriciaNode::new(word);
            leaf.is_end = true;
            node.children.insert(first, Box::new(leaf));
        }
    }

    fn search(node: &PatriciaNode, word: &str) -> bool {
        if word.is_empty() {
            return node.is_end;
        }

        let first = word.as_bytes()[0];
        match node.children.get(&first) {
            None => false,
            Some(child) => {
                if word.len() < child.label.len() { return false; }
                if !word.starts_with(&child.label) { return false; }
                Self::search(child, &word[child.label.len()..])
            }
        }
    }

    fn count_nodes(node: &PatriciaNode) -> usize {
        let mut count = 1;
        for child in node.children.values() {
            count += Self::count_nodes(child);
        }
        count
    }

    fn print(node: &PatriciaNode, depth: usize) {
        let indent = "  ".repeat(depth);
        let marker = if node.is_end { " *" } else { "" };
        if node.label.is_empty() {
            println!("{}(root){}", indent, marker);
        } else {
            println!("{}\"{}\"{}", indent, node.label, marker);
        }
        let mut keys: Vec<_> = node.children.keys().collect();
        keys.sort();
        for &k in &keys {
            Self::print(&node.children[k], depth + 1);
        }
    }
}

fn main() {
    let words = vec![
        "car", "card", "care", "careful", "cars",
        "cat", "category", "catalog",
        "do", "dog", "done", "door",
    ];

    // demo 1: prefix count
    println!("=== Demo 1: prefix count ===");
    let mut trie = Trie::new();
    for &w in &words {
        trie.insert(w);
    }

    for prefix in &["car", "cat", "ca", "do", "z"] {
        println!("Words with prefix '{}': {}", prefix, trie.count_prefix(prefix));
    }
    println!();

    // demo 2: longest prefix match
    println!("=== Demo 2: longest prefix match ===");
    let mut routes = Trie::new();
    for &w in &["app", "apple", "application", "api", "apply"] {
        routes.insert(w);
    }

    for query in &["applesauce", "applying", "apartment", "api", "appx", "application"] {
        match routes.longest_prefix_match(query) {
            Some(prefix) => println!("LPM(\"{}\") = \"{}\"", query, prefix),
            None => println!("LPM(\"{}\") = (none)", query),
        }
    }
    println!();

    // demo 3: autocomplete
    println!("=== Demo 3: autocomplete (max 5) ===");
    for prefix in &["car", "cat", "do"] {
        let suggestions = trie.autocomplete(prefix, 5);
        println!("Autocomplete '{}': {:?}", prefix, suggestions);
    }
    println!();

    // demo 4: patricia trie
    println!("=== Demo 4: Patricia trie ===");
    let mut patricia = PatriciaNode::new("");
    for &w in &words {
        PatriciaNode::insert(&mut patricia, w);
    }

    println!("Structure:");
    PatriciaNode::print(&patricia, 0);
    println!();

    let search_words = ["car", "card", "care", "cart", "cat", "dog", "xyz"];
    for &w in &search_words {
        println!("search(\"{}\"): {}", w,
                 if PatriciaNode::search(&patricia, w) { "FOUND" } else { "NOT FOUND" });
    }
    println!();

    // demo 5: node count comparison
    println!("=== Demo 5: node count comparison ===");
    let trie_nodes = trie.count_nodes();
    let pat_nodes = PatriciaNode::count_nodes(&patricia);
    println!("Standard trie: {} nodes", trie_nodes);
    println!("Patricia trie: {} nodes", pat_nodes);
    println!("Words stored:  {}", words.len());
    println!("Reduction: {:.0}%", 100.0 * (1.0 - pat_nodes as f64 / trie_nodes as f64));
    println!();

    // demo 6: trie vs HashMap for prefix operations
    println!("=== Demo 6: trie vs HashMap ===");
    let hash: HashMap<&str, bool> = words.iter().map(|&w| (w, true)).collect();

    // exact search: both O(m)
    println!("HashMap search(\"car\"): {}", hash.contains_key("car"));
    println!("Trie   search(\"car\"): {}", trie.search("car"));

    // prefix search: HashMap must scan all keys O(n*m)
    let prefix = "car";
    let hash_prefix: Vec<_> = hash.keys()
        .filter(|k| k.starts_with(prefix))
        .collect();
    let trie_prefix = trie.autocomplete(prefix, 100);
    println!("\nPrefix '{}' via HashMap (full scan): {:?}", prefix, hash_prefix);
    println!("Prefix '{}' via Trie (O(m+k)):       {:?}", prefix, trie_prefix);
    println!();

    // demo 7: practical pattern — building a search index
    println!("=== Demo 7: search index with ranked results ===");

    // insert words with frequency counts
    let mut freq_trie = Trie::new();
    let corpus = vec![
        "the", "the", "the", "there", "there", "their",
        "then", "them", "these", "they", "they",
        "this", "think", "thing",
    ];
    for &w in &corpus {
        freq_trie.insert(w);
    }

    // autocomplete "the" — shows all completions
    let suggestions = freq_trie.autocomplete("the", 10);
    println!("Autocomplete 'the': {:?}", suggestions);

    // count_prefix tells us how many words START with "the"
    println!("Words with prefix 'the': {}", freq_trie.count_prefix("the"));
    println!("Words with prefix 'thi': {}", freq_trie.count_prefix("thi"));
}
```

Compilar y ejecutar:

```bash
rustc trie_search.rs && ./trie_search
```

---

## Errores frecuentes

1. **Confundir split con inserción normal** en Patricia trie: si la
   nueva clave diverge en medio de un label comprimido, hay que
   partir el nodo. Si simplemente se crea un hijo nuevo sin partir,
   la búsqueda posterior falla.
2. **No distinguir fin de palabra de nodo intermedio**: en un trie
   estándar, llegar a un nodo no significa que la palabra exista —
   hay que verificar `is_end`. Lo mismo aplica en Patricia con
   labels compartidos.
3. **Olvidar actualizar `prefix_count` al eliminar**: si se usa
   `prefix_count` para count_prefix, al eliminar una palabra hay
   que decrementar el contador en cada nodo del camino.
4. **Confundir búsqueda exacta con prefix match**: `trie_search`
   verifica `is_end_of_word` al final; `trie_starts_with` no.
   Mezclarlos da resultados silenciosamente incorrectos.
5. **Asumir que Patricia siempre usa menos memoria**: cada nodo
   Patricia almacena un string (puntero + longitud + contenido),
   que ocupa más que un nodo de trie estándar. La ventaja es tener
   **menos nodos**, pero si las palabras comparten pocos prefijos,
   el ahorro puede ser marginal.

---

## Ejercicios

Todos los ejercicios se resuelven en C salvo que se indique lo
contrario.

### Ejercicio 1 — Prefix count en acción

Inserta las palabras `{"pre", "prefix", "predict", "prevent",
"preview", "price", "prime", "print", "pro", "program", "project",
"proof"}`. Sin ejecutar, responde: ¿cuántas palabras tienen el
prefijo `"pr"`? ¿Y `"pre"`? ¿Y `"pri"`? ¿Y `"pro"`?

<details><summary>Predice antes de ejecutar</summary>

¿La suma de `count("pre") + count("pri") + count("pro")` es igual
a `count("pr")`? ¿Por qué o por qué no?

</details>

### Ejercicio 2 — Longest prefix match para URLs

Construye un trie con las "rutas" (tratadas como caracteres a-z):
`{"api", "apiv", "app", "apple", "application"}`. Implementa
longest_prefix_match y prueba con: `"apiversion"`, `"applet"`,
`"application"`, `"apply"`, `"banana"`.

<details><summary>Predice antes de ejecutar</summary>

¿Cuál es el LPM de `"applet"`? ¿Es `"app"` o `"apple"`? Pista:
`"apple"` no es prefijo de `"applet"` — comparte 4 de 5 caracteres
pero no todos.

</details>

### Ejercicio 3 — Patricia trie: traza de construcción

Dibuja el Patricia trie paso a paso al insertar:
`{"romane", "romanus", "romulus", "rubens", "ruber", "rubicon",
"rubicundus"}`. Indica en qué inserciones ocurre un split de nodo.

<details><summary>Predice antes de ejecutar</summary>

¿Cuántos nodos tiene el Patricia trie final? ¿Cuántos tendría un
trie estándar? ¿Cuál es la reducción porcentual?

</details>

### Ejercicio 4 — Comparación de nodos: trie vs Patricia

Inserta un diccionario de 1000 palabras generadas aleatoriamente
(longitud 5-15, caracteres a-z) en un trie estándar y en un
Patricia trie. Reporta el número de nodos de cada uno.

<details><summary>Predice antes de ejecutar</summary>

Con 1000 palabras aleatorias de longitud promedio 10, ¿cuántos
nodos esperarías en cada estructura? Pista: con 26 caracteres,
las palabras aleatorias comparten pocos prefijos.

</details>

### Ejercicio 5 — Autocomplete con ranking

Modifica el trie para que cada palabra tenga una **frecuencia**
(int). Implementa `autocomplete_ranked(prefix, k)` que retorne las
$k$ palabras más frecuentes con ese prefijo. Pista: necesitas
recolectar todos los resultados y luego ordenar, o usar un heap.

<details><summary>Predice antes de ejecutar</summary>

¿Cuál es la complejidad de tu solución? ¿Es $O(m + n_p \log k)$
donde $n_p$ es el número de palabras con el prefijo, o peor?

</details>

### Ejercicio 6 — Eliminar y verificar prefix_count

Implementa `trie_delete` que al eliminar una palabra:
1. Desmarque `is_end`.
2. Decremente `prefix_count` en cada nodo del camino.
3. Libere nodos que ya no son necesarios.

Verifica que `count_prefix` retorne valores correctos después de
eliminar varias palabras.

<details><summary>Predice antes de ejecutar</summary>

Si insertas `{"car", "card", "care"}` y luego eliminas `"card"`,
¿cuánto vale `count_prefix("car")` después? ¿Y `count_prefix("ca")`?

</details>

### Ejercicio 7 — Patricia trie: eliminación

Implementa eliminación en un Patricia trie. Al eliminar, si un nodo
interno queda con un solo hijo y no es fin de palabra, fusiona el
nodo con su hijo (operación inversa al split).

<details><summary>Predice antes de ejecutar</summary>

Partiendo de `{"test", "team", "toast"}`, si eliminas `"toast"`,
¿se fusiona algún nodo? ¿Cuántos nodos quedan?

</details>

### Ejercicio 8 — Trie vs BTreeMap para prefix search (Rust)

Compara un `Trie` manual vs `BTreeMap<String, ()>` para prefix
search con 10000 palabras. Para BTreeMap, usa `range` para
encontrar todas las claves con un prefijo dado. Mide el tiempo de
1000 búsquedas de prefijo aleatorias de longitud 3.

<details><summary>Predice antes de ejecutar</summary>

¿Cuál será más rápido para prefix search de 3 caracteres? ¿Y si
el prefijo tiene longitud 10?

</details>

### Ejercicio 9 — Wildcard search

Implementa `trie_wildcard(root, pattern)` que busque palabras
donde `'.'` coincide con cualquier carácter. Por ejemplo,
`"c.r"` coincide con `"car"` y `"cur"`.

<details><summary>Predice antes de ejecutar</summary>

¿Cuál es la complejidad en el peor caso? ¿Qué pasa con el patrón
`"..."` (tres puntos)?

</details>

### Ejercicio 10 — Simulación de IP routing

Representa direcciones IP como strings de bits (`"11000000101010..."`).
Inserta las siguientes rutas en un trie:
- `"11000000"` → ruta A (192.0.0.0/8 simplificado)
- `"1100000010101000"` → ruta B (192.168.0.0/16 simplificado)
- `"110000001010100000000001"` → ruta C (192.168.1.0/24 simplificado)

Para cada "paquete" (string de 32 bits), usa longest_prefix_match
para determinar qué ruta usar.

<details><summary>Predice antes de ejecutar</summary>

Si el paquete tiene IP 192.168.1.42, ¿cuál ruta gana? ¿Y si es
192.168.2.1? ¿Qué ventaja tiene el trie sobre una tabla lineal
para esta operación?

</details>
