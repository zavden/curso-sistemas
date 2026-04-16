# Iteración sobre composites

## 1. El problema: recorrer sin recursión explícita

En T01 y T02 todas las operaciones sobre composites usaban **recursión directa**:
cada nodo se llamaba a sí mismo sobre sus hijos. Esto funciona, pero tiene
limitaciones:

```
Recursión directa:                 Iteración:
fn size(&self) -> usize {          for node in tree.iter() {
    match self {                       // procesar node
        Leaf(v) => *v,             }
        Branch(ch) => ch.iter()    // El USUARIO decide qué hacer
            .map(|c| c.size())     // No está embebido en el árbol
            .sum(),
    }
}
// La operación está DENTRO        // La operación está FUERA
// del árbol                       // del árbol
```

**¿Por qué separar recorrido de operación?**
1. **Reutilización**: un solo iterador sirve para size, count, find, filter...
2. **Composición**: encadenar `.filter().map().take()` sobre el árbol
3. **Control**: el consumidor decide cuándo parar (lazy evaluation)
4. **Desacoplamiento**: el árbol no conoce las operaciones que se aplican sobre él

---

## 2. DFS vs BFS: dos órdenes fundamentales

```
Árbol de ejemplo:
         A
        / \
       B   C
      / \   \
     D   E   F

DFS pre-order:  A, B, D, E, C, F   (raíz primero, luego hijos izq→der)
DFS post-order: D, E, B, F, C, A   (hijos primero, luego raíz)
DFS in-order:   D, B, E, A, C, F   (izq, raíz, der — solo árboles binarios)
BFS:            A, B, C, D, E, F   (nivel por nivel)
```

### Cuándo usar cada uno

| Orden | Caso de uso | Ejemplo |
|-------|------------|---------|
| DFS pre-order | Serializar, copiar, imprimir con indentación | `tree` command |
| DFS post-order | Calcular tamaño, destruir, evaluar expresiones | `rm -rf`, `du` |
| BFS | Buscar el más cercano, niveles, shortest path | `find -maxdepth` |

---

## 3. DFS iterativo en C

La recursión usa el call stack implícitamente. Para convertirla en iteración,
necesitamos un **stack explícito**:

### 3.1 Stack de punteros

```c
typedef struct {
    const FsNode **items;
    size_t         count;
    size_t         capacity;
} NodeStack;

void stack_push(NodeStack *s, const FsNode *node) {
    if (s->count == s->capacity) {
        s->capacity = s->capacity == 0 ? 16 : s->capacity * 2;
        s->items = realloc(s->items, s->capacity * sizeof(FsNode *));
    }
    s->items[s->count++] = node;
}

const FsNode *stack_pop(NodeStack *s) {
    return s->count > 0 ? s->items[--s->count] : NULL;
}

void stack_free(NodeStack *s) {
    free(s->items);
    s->items = NULL;
    s->count = s->capacity = 0;
}
```

### 3.2 DFS pre-order iterativo

```c
void dfs_preorder(const FsNode *root) {
    NodeStack stack = {0};
    stack_push(&stack, root);

    while (stack.count > 0) {
        const FsNode *node = stack_pop(&stack);

        // PROCESAR nodo (pre-order: antes de los hijos)
        printf("%s\n", node->name);

        // Si es directorio, push hijos en orden INVERSO
        // (para que el primer hijo sea el próximo en salir)
        if (node->ops == &DIR_OPS) {
            const Directory *d = (const Directory *)node;
            for (size_t i = d->count; i > 0; i--) {
                stack_push(&stack, d->children[i - 1]);
            }
        }
    }

    stack_free(&stack);
}
```

```
Ejecución paso a paso:

Stack: [A]           → pop A, print "A", push C,B
Stack: [B, C]        → pop B, print "B", push E,D
Stack: [D, E, C]     → pop D, print "D" (hoja)
Stack: [E, C]        → pop E, print "E" (hoja)
Stack: [C]           → pop C, print "C", push F
Stack: [F]           → pop F, print "F" (hoja)
Stack: []            → fin

Salida: A, B, D, E, C, F  ✓ (pre-order)
```

**¿Por qué push en orden inverso?** El stack es LIFO. Si pusheas B antes que C,
C sale primero. Invirtiendo el orden, B sale primero (izquierda antes que derecha).

### 3.3 Iterador como struct (estilo C)

Encapsular el estado de la iteración permite recorrer bajo demanda:

```c
typedef struct {
    NodeStack stack;
} DfsIter;

void dfs_iter_init(DfsIter *it, const FsNode *root) {
    it->stack = (NodeStack){0};
    if (root) stack_push(&it->stack, root);
}

const FsNode *dfs_iter_next(DfsIter *it) {
    if (it->stack.count == 0) return NULL;

    const FsNode *node = stack_pop(&it->stack);

    if (node->ops == &DIR_OPS) {
        const Directory *d = (const Directory *)node;
        for (size_t i = d->count; i > 0; i--) {
            stack_push(&it->stack, d->children[i - 1]);
        }
    }

    return node;
}

void dfs_iter_free(DfsIter *it) {
    stack_free(&it->stack);
}
```

Uso:

```c
DfsIter it;
dfs_iter_init(&it, (FsNode *)root);

const FsNode *node;
while ((node = dfs_iter_next(&it)) != NULL) {
    if (node->ops == &FILE_OPS) {
        const File *f = (const File *)node;
        if (f->file_size > 1000) {
            printf("Large: %s (%zu bytes)\n", node->name, f->file_size);
        }
    }
}

dfs_iter_free(&it);
```

El iterador es **lazy**: solo avanza cuando llamas `next`. Puedes parar en
cualquier momento sin recorrer todo el árbol.

---

## 4. BFS iterativo en C

BFS usa una **cola** (queue) en lugar de un stack:

```c
typedef struct {
    const FsNode **items;
    size_t         head;     // índice del próximo a sacar
    size_t         tail;     // índice donde insertar
    size_t         capacity;
} NodeQueue;

void queue_init(NodeQueue *q, size_t capacity) {
    q->items    = malloc(capacity * sizeof(FsNode *));
    q->head     = 0;
    q->tail     = 0;
    q->capacity = capacity;
}

void queue_push(NodeQueue *q, const FsNode *node) {
    // Simplificado: sin resize circular
    q->items[q->tail++] = node;
}

const FsNode *queue_pop(NodeQueue *q) {
    return q->head < q->tail ? q->items[q->head++] : NULL;
}

bool queue_empty(const NodeQueue *q) {
    return q->head == q->tail;
}

void queue_free(NodeQueue *q) {
    free(q->items);
}
```

```c
void bfs(const FsNode *root) {
    NodeQueue queue;
    queue_init(&queue, 256);
    queue_push(&queue, root);

    while (!queue_empty(&queue)) {
        const FsNode *node = queue_pop(&queue);
        printf("%s\n", node->name);

        if (node->ops == &DIR_OPS) {
            const Directory *d = (const Directory *)node;
            for (size_t i = 0; i < d->count; i++) {
                queue_push(&queue, d->children[i]);  // orden natural
            }
        }
    }

    queue_free(&queue);
}
```

```
Ejecución BFS:

Queue: [A]           → pop A, print "A", enqueue B,C
Queue: [B, C]        → pop B, print "B", enqueue D,E
Queue: [C, D, E]     → pop C, print "C", enqueue F
Queue: [D, E, F]     → pop D, print "D"
Queue: [E, F]        → pop E, print "E"
Queue: [F]           → pop F, print "F"

Salida: A, B, C, D, E, F  ✓ (nivel por nivel)
```

**Diferencia clave**: en BFS los hijos se encolan en orden natural (no inverso),
porque FIFO preserva el orden de inserción.

---

## 5. DFS iterativo en Rust

### 5.1 Iterador con stack explícito

```rust
struct DfsIter<'a> {
    stack: Vec<&'a FsNode>,
}

impl<'a> DfsIter<'a> {
    fn new(root: &'a FsNode) -> Self {
        DfsIter { stack: vec![root] }
    }
}

impl<'a> Iterator for DfsIter<'a> {
    type Item = &'a FsNode;

    fn next(&mut self) -> Option<Self::Item> {
        let node = self.stack.pop()?;

        if let FsNode::Dir { children, .. } = node {
            // Push en orden inverso para que el primer hijo salga primero
            for child in children.iter().rev() {
                self.stack.push(child);
            }
        }

        Some(node)
    }
}
```

Al implementar `Iterator`, obtenemos **todo el ecosistema de iteradores** gratis:

```rust
impl FsNode {
    fn iter(&self) -> DfsIter<'_> {
        DfsIter::new(self)
    }
}

// Ahora se puede usar con toda la API de Iterator:
let total_size: usize = root.iter()
    .filter_map(|node| match node {
        FsNode::File { size, .. } => Some(size),
        _ => None,
    })
    .sum();

let large_files: Vec<&str> = root.iter()
    .filter_map(|node| match node {
        FsNode::File { name, size } if *size > 1000 => Some(name.as_str()),
        _ => None,
    })
    .collect();

let first_big = root.iter()
    .find(|node| node.size() > 5000);  // para al encontrar el primero
```

**Comparación con C**:

```
C:                                  Rust:
DfsIter it;                         for node in root.iter() {
dfs_iter_init(&it, root);               // ...
const FsNode *node;                 }
while ((node = dfs_iter_next(&it))) {
    // ...                          // O con adaptadores:
}                                   root.iter()
dfs_iter_free(&it);                     .filter(...)
                                        .map(...)
// Manual, no componible                .collect()
```

### 5.2 BFS en Rust

```rust
use std::collections::VecDeque;

struct BfsIter<'a> {
    queue: VecDeque<&'a FsNode>,
}

impl<'a> BfsIter<'a> {
    fn new(root: &'a FsNode) -> Self {
        let mut queue = VecDeque::new();
        queue.push_back(root);
        BfsIter { queue }
    }
}

impl<'a> Iterator for BfsIter<'a> {
    type Item = &'a FsNode;

    fn next(&mut self) -> Option<Self::Item> {
        let node = self.queue.pop_front()?;

        if let FsNode::Dir { children, .. } = node {
            for child in children {
                self.queue.push_back(child);
            }
        }

        Some(node)
    }
}

impl FsNode {
    fn bfs_iter(&self) -> BfsIter<'_> {
        BfsIter::new(self)
    }
}
```

La **única diferencia** entre DFS y BFS es la estructura auxiliar:

| | DFS | BFS |
|---|-----|-----|
| Estructura | `Vec` (stack) | `VecDeque` (queue) |
| Insertar | `push` (final) | `push_back` (final) |
| Extraer | `pop` (final = LIFO) | `pop_front` (inicio = FIFO) |
| Hijos | Orden inverso | Orden natural |

---

## 6. DFS post-order iterativo

Post-order es más complejo porque necesitas visitar un nodo **después** de
todos sus hijos. Hay dos estrategias:

### 6.1 Estrategia de dos stacks

```rust
fn post_order<'a>(root: &'a FsNode) -> Vec<&'a FsNode> {
    let mut stack1 = vec![root];
    let mut stack2: Vec<&FsNode> = Vec::new();

    while let Some(node) = stack1.pop() {
        stack2.push(node);
        if let FsNode::Dir { children, .. } = node {
            for child in children {
                stack1.push(child);  // orden natural (no inverso)
            }
        }
    }

    stack2.reverse();  // invertir da post-order
    stack2
}
```

```
Paso 1 (llenar stack2 en pre-order inverso):
stack1: [A] → pop A → stack2: [A], push B,C
stack1: [B,C] → pop C → stack2: [A,C], push F
stack1: [B,F] → pop F → stack2: [A,C,F]
stack1: [B] → pop B → stack2: [A,C,F,B], push D,E
stack1: [D,E] → pop E → stack2: [A,C,F,B,E]
stack1: [D] → pop D → stack2: [A,C,F,B,E,D]

Paso 2 (reverse):
stack2 reversed: [D, E, B, F, C, A]  ✓ post-order
```

### 6.2 Post-order como iterador con estado

```rust
enum VisitState<'a> {
    Enter(&'a FsNode),    // primera vez que vemos el nodo
    Exit(&'a FsNode),     // ya visitamos todos sus hijos
}

struct PostOrderIter<'a> {
    stack: Vec<VisitState<'a>>,
}

impl<'a> PostOrderIter<'a> {
    fn new(root: &'a FsNode) -> Self {
        PostOrderIter {
            stack: vec![VisitState::Enter(root)],
        }
    }
}

impl<'a> Iterator for PostOrderIter<'a> {
    type Item = &'a FsNode;

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            match self.stack.pop()? {
                VisitState::Enter(node) => {
                    // Programar la visita de salida DESPUÉS de los hijos
                    self.stack.push(VisitState::Exit(node));
                    if let FsNode::Dir { children, .. } = node {
                        for child in children.iter().rev() {
                            self.stack.push(VisitState::Enter(child));
                        }
                    }
                }
                VisitState::Exit(node) => {
                    return Some(node);  // emitir en post-order
                }
            }
        }
    }
}
```

```
Stack: [Enter(A)]
  pop Enter(A) → push Exit(A), Enter(C), Enter(B)
Stack: [Exit(A), Enter(C), Enter(B)]
  pop Enter(B) → push Exit(B), Enter(E), Enter(D)
Stack: [Exit(A), Enter(C), Exit(B), Enter(E), Enter(D)]
  pop Enter(D) → push Exit(D) (hoja, no hijos)
  pop Exit(D) → emit D ✓
  pop Enter(E) → push Exit(E)
  pop Exit(E) → emit E ✓
  pop Exit(B) → emit B ✓
  pop Enter(C) → push Exit(C), Enter(F)
  pop Enter(F) → push Exit(F)
  pop Exit(F) → emit F ✓
  pop Exit(C) → emit C ✓
  pop Exit(A) → emit A ✓

Salida: D, E, B, F, C, A  ✓ post-order
```

Esta técnica de Enter/Exit es generalizable: si emites tanto en Enter como en
Exit, obtienes un recorrido **euler tour** del árbol (útil para serialización
con profundidad).

---

## 7. Iterador con profundidad

Muchas operaciones necesitan saber a qué profundidad está cada nodo:

```rust
struct DepthDfsIter<'a> {
    stack: Vec<(&'a FsNode, usize)>,  // (nodo, profundidad)
}

impl<'a> DepthDfsIter<'a> {
    fn new(root: &'a FsNode) -> Self {
        DepthDfsIter { stack: vec![(root, 0)] }
    }
}

impl<'a> Iterator for DepthDfsIter<'a> {
    type Item = (&'a FsNode, usize);  // (nodo, profundidad)

    fn next(&mut self) -> Option<Self::Item> {
        let (node, depth) = self.stack.pop()?;

        if let FsNode::Dir { children, .. } = node {
            for child in children.iter().rev() {
                self.stack.push((child, depth + 1));
            }
        }

        Some((node, depth))
    }
}

impl FsNode {
    fn iter_with_depth(&self) -> DepthDfsIter<'_> {
        DepthDfsIter::new(self)
    }
}
```

Uso: imprimir el árbol con indentación:

```rust
for (node, depth) in root.iter_with_depth() {
    let indent = "  ".repeat(depth);
    match node {
        FsNode::File { name, size } => {
            println!("{indent}{name} ({size} bytes)");
        }
        FsNode::Dir { name, .. } => {
            println!("{indent}{name}/");
        }
    }
}
```

Limitar profundidad (`find -maxdepth`):

```rust
let shallow: Vec<_> = root.iter_with_depth()
    .filter(|(_, depth)| *depth <= 2)
    .map(|(node, _)| node)
    .collect();
```

---

## 8. Aplanamiento lazy (flatten)

"Aplanar" un árbol es convertirlo en una secuencia lineal. Con iteradores
lazy, el aplanamiento ocurre bajo demanda:

### 8.1 Flatten que solo emite hojas

```rust
struct LeafIter<'a> {
    stack: Vec<&'a FsNode>,
}

impl<'a> LeafIter<'a> {
    fn new(root: &'a FsNode) -> Self {
        LeafIter { stack: vec![root] }
    }
}

impl<'a> Iterator for LeafIter<'a> {
    type Item = &'a FsNode;

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            let node = self.stack.pop()?;
            match node {
                FsNode::File { .. } => return Some(node),  // emitir hoja
                FsNode::Dir { children, .. } => {
                    for child in children.iter().rev() {
                        self.stack.push(child);
                    }
                    // NO emitir directorios, seguir buscando
                }
            }
        }
    }
}

impl FsNode {
    fn files(&self) -> LeafIter<'_> {
        LeafIter::new(self)
    }
}
```

```rust
// Todos los archivos del árbol, ignorando la estructura de directorios
for file in root.files() {
    println!("{}", file.name());
}

// Total de bytes en archivos
let total: usize = root.files().map(|f| f.size()).sum();

// Los 3 archivos más grandes
let mut files: Vec<_> = root.files().collect();
files.sort_by(|a, b| b.size().cmp(&a.size()));
let top3 = &files[..3.min(files.len())];
```

### 8.2 Flatten con ruta completa

```rust
struct PathIter<'a> {
    stack: Vec<(&'a FsNode, String)>,  // (nodo, ruta acumulada)
}

impl<'a> PathIter<'a> {
    fn new(root: &'a FsNode) -> Self {
        let name = root.name().to_string();
        PathIter { stack: vec![(root, name)] }
    }
}

impl<'a> Iterator for PathIter<'a> {
    type Item = (&'a FsNode, String);

    fn next(&mut self) -> Option<Self::Item> {
        let (node, path) = self.stack.pop()?;

        if let FsNode::Dir { children, .. } = node {
            for child in children.iter().rev() {
                let child_path = format!("{}/{}", path, child.name());
                self.stack.push((child, child_path));
            }
        }

        Some((node, path))
    }
}
```

```rust
// Equivalente a: find project/ -type f -size +1000c
for (node, path) in PathIter::new(&root) {
    if let FsNode::File { size, .. } = node {
        if *size > 1000 {
            println!("{path} ({size} bytes)");
        }
    }
}
// project/main.c (2048 bytes)
// project/src/parser.c (4096 bytes)
// project/src/lexer.c (3072 bytes)
```

---

## 9. Iterador genérico sobre expresiones (Box)

Los composites con `Box` (árboles binarios) necesitan un enfoque ligeramente
diferente porque los hijos no están en un `Vec`:

```rust
#[derive(Debug)]
enum Expr {
    Num(f64),
    Add(Box<Expr>, Box<Expr>),
    Mul(Box<Expr>, Box<Expr>),
}

struct ExprIter<'a> {
    stack: Vec<&'a Expr>,
}

impl<'a> ExprIter<'a> {
    fn new(root: &'a Expr) -> Self {
        ExprIter { stack: vec![root] }
    }
}

impl<'a> Iterator for ExprIter<'a> {
    type Item = &'a Expr;

    fn next(&mut self) -> Option<Self::Item> {
        let node = self.stack.pop()?;

        match node {
            Expr::Num(_) => {}
            Expr::Add(l, r) | Expr::Mul(l, r) => {
                self.stack.push(r);  // derecho primero (sale después)
                self.stack.push(l);  // izquierdo segundo (sale primero)
            }
        }

        Some(node)
    }
}

impl Expr {
    fn iter(&self) -> ExprIter<'_> {
        ExprIter::new(self)
    }
}
```

```rust
let expr = Expr::Add(
    Box::new(Expr::Mul(
        Box::new(Expr::Num(2.0)),
        Box::new(Expr::Num(3.0)),
    )),
    Box::new(Expr::Num(4.0)),
);

// Contar nodos
let count = expr.iter().count();  // 5: Add, Mul, 2, 3, 4

// Contar solo hojas
let leaves = expr.iter()
    .filter(|e| matches!(e, Expr::Num(_)))
    .count();  // 3

// Sumar todos los literales
let sum: f64 = expr.iter()
    .filter_map(|e| match e {
        Expr::Num(v) => Some(v),
        _ => None,
    })
    .sum();  // 9.0
```

---

## 10. Equivalente en C: callback visitor

En C no hay closures ni adaptadores de iterador. El patrón equivalente es
un **visitor con callback**:

```c
// Callback: recibe el nodo y datos del usuario
typedef bool (*VisitFn)(const FsNode *node, int depth, void *user_data);
// Retorna true para continuar, false para parar

void dfs_visit(const FsNode *root, VisitFn visitor, void *user_data) {
    typedef struct { const FsNode *node; int depth; } Entry;

    Entry *stack = malloc(256 * sizeof(Entry));
    size_t top = 0;

    stack[top++] = (Entry){ root, 0 };

    while (top > 0) {
        Entry entry = stack[--top];

        if (!visitor(entry.node, entry.depth, user_data)) {
            break;  // visitor pidió parar
        }

        if (entry.node->ops == &DIR_OPS) {
            const Directory *d = (const Directory *)entry.node;
            for (size_t i = d->count; i > 0; i--) {
                stack[top++] = (Entry){ d->children[i - 1], entry.depth + 1 };
            }
        }
    }

    free(stack);
}
```

Uso:

```c
// Encontrar archivos grandes
typedef struct {
    size_t threshold;
    size_t found;
} FindCtx;

bool find_large_visitor(const FsNode *node, int depth, void *user_data) {
    (void)depth;
    FindCtx *ctx = user_data;
    if (node->ops == &FILE_OPS) {
        const File *f = (const File *)node;
        if (f->file_size >= ctx->threshold) {
            printf("Found: %s (%zu bytes)\n", node->name, f->file_size);
            ctx->found++;
        }
    }
    return true;  // continuar
}

FindCtx ctx = { .threshold = 1000, .found = 0 };
dfs_visit((FsNode *)root, find_large_visitor, &ctx);
printf("Total found: %zu\n", ctx.found);
```

### Comparación: callback (C) vs Iterator (Rust)

```
C con callback:                     Rust con Iterator:

FindCtx ctx = { 1000, 0 };         let found: Vec<_> = root.iter()
dfs_visit(root, find_large, &ctx);      .filter(|n| n.size() >= 1000)
printf("%zu\n", ctx.found);             .collect();
                                    println!("{}", found.len());

// Definir struct + función           // Inline, componible
// void* para pasar datos            // Closures capturan contexto
// Manual early-exit con bool        // .take(n), .find(), etc.
```

---

## 11. BFS por niveles (level-order con separación)

A veces necesitas saber dónde termina un nivel y empieza el siguiente:

```rust
struct LevelIter<'a> {
    current_level: VecDeque<&'a FsNode>,
    next_level: VecDeque<&'a FsNode>,
    level: usize,
}

impl<'a> LevelIter<'a> {
    fn new(root: &'a FsNode) -> Self {
        let mut current = VecDeque::new();
        current.push_back(root);
        LevelIter {
            current_level: current,
            next_level: VecDeque::new(),
            level: 0,
        }
    }

    /// Itera retornando (nodo, nivel)
    fn next_with_level(&mut self) -> Option<(&'a FsNode, usize)> {
        // Si el nivel actual está vacío, rotar al siguiente
        if self.current_level.is_empty() {
            if self.next_level.is_empty() {
                return None;
            }
            std::mem::swap(&mut self.current_level, &mut self.next_level);
            self.level += 1;
        }

        let node = self.current_level.pop_front()?;

        if let FsNode::Dir { children, .. } = node {
            for child in children {
                self.next_level.push_back(child);
            }
        }

        Some((node, self.level))
    }
}
```

```rust
let mut iter = LevelIter::new(&root);
let mut current_level = 0;

while let Some((node, level)) = iter.next_with_level() {
    if level != current_level {
        println!("--- Level {} ---", level);
        current_level = level;
    }
    println!("  {}", node.name());
}
```

Salida:
```
  project
--- Level 1 ---
  main.c
  Makefile
  src
  include
--- Level 2 ---
  parser.c
  lexer.c
  parser.h
```

---

## 12. Recursión vs iteración: trade-offs

| Aspecto | Recursión | Iteración con stack |
|---------|-----------|-------------------|
| Legibilidad | Más clara, directa | Más código, explícita |
| Stack overflow | Posible en árboles profundos | Solo limitado por heap |
| Control de flujo | Difícil parar a mitad | `break` o retornar None |
| Composición | Una operación por función | `.filter().map().take()` |
| Performance | Overhead de llamadas a función | Overhead de push/pop en Vec |
| Estado entre nodos | Parámetros + retorno | Campos del iterador |
| Post-order | Natural (base case) | Requiere Enter/Exit trick |

**Regla práctica**:
- Árboles poco profundos (< 100 niveles) + operación simple → recursión
- Árboles potencialmente profundos o necesidad de composición → iterador
- Necesitas parar temprano (find, take) → iterador
- Necesitas post-order → recursión es más simple

**Stack overflow en la práctica**: el stack por defecto es ~8MB en Linux.
Cada frame de recursión usa ~64-256 bytes. Un árbol de 50,000 niveles de
profundidad puede causar overflow con recursión pero no con iteración
(el Vec crece en el heap).

---

## 13. Errores comunes

| Error | Problema | Solución |
|-------|----------|----------|
| Push hijos sin invertir (DFS) | Recorre derecha antes que izquierda | `children.iter().rev()` |
| Usar stack para BFS | DFS en lugar de BFS | `VecDeque` con `pop_front` |
| Usar queue para DFS | BFS en lugar de DFS | `Vec` con `pop` |
| No implementar `Iterator` trait | No se puede usar `.filter()`, `.map()`, etc. | `impl Iterator for ...` |
| Post-order con stack simple | Emite en pre-order | Usar Enter/Exit o dos stacks |
| Lifetime incorrecto en iterador | `&FsNode` no vive suficiente | `struct Iter<'a>` con lifetime del árbol |
| Mutar árbol durante iteración | Borrow checker lo impide (correctamente) | Recolectar primero, mutar después |
| Stack explícito sin capacidad inicial | Muchas reallocaciones al inicio | `Vec::with_capacity(estimated)` |
| BFS queue sin límite | Memoria O(ancho) del nivel más amplio | Monitorear o limitar |
| Olvidar caso de árbol vacío | Panic en `pop` | El `?` de `self.stack.pop()?` lo maneja |

---

## 14. Ejercicios

### Ejercicio 1 — DFS pre-order manual

**Predicción**: dado este árbol, ¿en qué orden emite el `DfsIter`?

```rust
let tree = FsNode::Dir {
    name: "root".into(),
    children: vec![
        FsNode::File { name: "x".into(), size: 10 },
        FsNode::Dir {
            name: "a".into(),
            children: vec![
                FsNode::File { name: "y".into(), size: 20 },
            ],
        },
        FsNode::File { name: "z".into(), size: 30 },
    ],
};

let names: Vec<_> = tree.iter().map(|n| n.name()).collect();
```

<details>
<summary>Respuesta</summary>

`names = ["root", "x", "a", "y", "z"]`

```
Stack trace:
[root]          → pop root, push z, a, x (inverso)
[x, a, z]       → pop x (hoja)
[a, z]          → pop a (dir), push y
[y, z]          → pop y (hoja)
[z]             → pop z (hoja)
[]              → fin
```

Pre-order DFS: visita raíz, luego recursivamente de izquierda a derecha.
Los hijos se pushean en orden inverso para que el primero salga primero del stack.
</details>

---

### Ejercicio 2 — BFS vs DFS

**Predicción**: ¿cuál es la diferencia de orden entre `iter()` (DFS) y
`bfs_iter()` para el mismo árbol del Ejercicio 1?

<details>
<summary>Respuesta</summary>

```
DFS: root, x, a, y, z
BFS: root, x, a, z, y
```

```
         root (nivel 0)
        / | \
       x  a  z   (nivel 1)
          |
          y       (nivel 2)
```

- **DFS**: al llegar a `a`, desciende inmediatamente a `y` antes de visitar `z`
- **BFS**: procesa todo el nivel 1 (x, a, z) antes de bajar al nivel 2 (y)

La diferencia clave es que DFS va "profundo primero" y BFS va "ancho primero".
`z` está al mismo nivel que `a`, pero DFS lo visita después de `y` (hijo de `a`).
</details>

---

### Ejercicio 3 — Post-order con Enter/Exit

**Predicción**: traza el stack del `PostOrderIter` para este árbol:

```rust
let tree = FsNode::Dir {
    name: "d".into(),
    children: vec![
        FsNode::File { name: "a".into(), size: 1 },
        FsNode::File { name: "b".into(), size: 2 },
    ],
};
```

¿En qué orden emite los nodos?

<details>
<summary>Respuesta</summary>

Emite: `a`, `b`, `d`

```
Stack: [Enter(d)]
  pop Enter(d) → push Exit(d), Enter(b), Enter(a)
Stack: [Exit(d), Enter(b), Enter(a)]
  pop Enter(a) → push Exit(a) (hoja, no hijos)
Stack: [Exit(d), Enter(b), Exit(a)]
  pop Exit(a) → emit "a" ✓
Stack: [Exit(d), Enter(b)]
  pop Enter(b) → push Exit(b)
Stack: [Exit(d), Exit(b)]
  pop Exit(b) → emit "b" ✓
Stack: [Exit(d)]
  pop Exit(d) → emit "d" ✓
```

Post-order: los hijos se emiten antes que el padre. Es el orden correcto
para operaciones como `size` (necesitas los tamaños de los hijos antes de
sumar) o `destroy` (destruir hijos antes que el contenedor).
</details>

---

### Ejercicio 4 — Early exit con Iterator

**Predicción**: ¿cuántos nodos visita `find` en este árbol?

```rust
let tree = FsNode::Dir {
    name: "root".into(),
    children: vec![
        FsNode::Dir {
            name: "a".into(),
            children: vec![
                FsNode::File { name: "target".into(), size: 42 },
                FsNode::File { name: "other".into(), size: 99 },
            ],
        },
        FsNode::Dir {
            name: "b".into(),
            children: vec![
                FsNode::File { name: "skip".into(), size: 1 },
            ],
        },
    ],
};

let found = tree.iter().find(|n| n.name() == "target");
```

<details>
<summary>Respuesta</summary>

Visita **3 nodos**: `root`, `a`, `target`.

```
Stack: [root]       → pop root, push b, a
Stack: [a, b]       → pop a, push other, target
Stack: [target, other, b]  → pop target → name == "target" → STOP
```

`find` detiene la iteración en cuanto el predicado retorna true.
Los nodos `other`, `b`, y `skip` nunca se visitan.

Con recursión directa, el control de flujo temprano es más difícil:
necesitarías propagar un `Option` o `Result` hacia arriba en cada nivel.
Con el iterador, `find` simplemente deja de llamar `next`.
</details>

---

### Ejercicio 5 — LeafIter

**Predicción**: ¿qué produce `root.files().map(|f| f.name()).collect::<Vec<_>>()`
para el árbol del Ejercicio 1?

<details>
<summary>Respuesta</summary>

`["x", "y", "z"]`

`LeafIter` solo emite nodos `File`, saltando directorios:

```
Stack: [root]    → root es Dir, push z, a, x. NO emitir.
Stack: [x, a, z] → x es File → emit "x"
Stack: [a, z]    → a es Dir, push y. NO emitir.
Stack: [y, z]    → y es File → emit "y"
Stack: [z]       → z es File → emit "z"
```

Los directorios `root` y `a` se usan solo para descubrir sus hijos,
pero nunca se retornan al consumidor. Es un aplanamiento: la estructura
jerárquica desaparece y solo quedan las hojas en orden DFS.
</details>

---

### Ejercicio 6 — Iterador con profundidad

**Predicción**: ¿qué produce `iter_with_depth` filtrado a `depth <= 1` para
el árbol del Ejercicio 1?

```rust
let shallow: Vec<_> = tree.iter_with_depth()
    .filter(|(_, depth)| *depth <= 1)
    .map(|(node, _)| node.name())
    .collect();
```

<details>
<summary>Respuesta</summary>

`["root", "x", "a", "z"]`

```
         root (depth=0) ✓
        / | \
       x  a  z          (depth=1) ✓ ✓ ✓
          |
          y              (depth=2) ✗ filtrado
```

El iterador emite todos los nodos con su profundidad, pero el `filter`
descarta los que tienen `depth > 1`. El nodo `y` (depth=2) no pasa el filtro.

Nota: `a` sí aparece (es un directorio a depth=1), pero sus hijos no.
Esto es equivalente a `find . -maxdepth 1`.

Importante: el iterador **sí visita** `y` internamente (lo saca del stack),
pero el `filter` lo descarta. Si el árbol es enorme y quieres **no
descender**, necesitarías un iterador que no pushee hijos más allá de la
profundidad máxima.
</details>

---

### Ejercicio 7 — Callback visitor en C

**Predicción**: ¿qué imprime este visitor?

```c
bool count_visitor(const FsNode *node, int depth, void *user_data) {
    int *counts = user_data;  // array: counts[depth]
    counts[depth]++;
    return true;
}

int counts[10] = {0};
dfs_visit((FsNode *)root, count_visitor, counts);
// root tiene: root/{x, a/{y}, z}
for (int i = 0; i < 3; i++) {
    printf("Level %d: %d nodes\n", i, counts[i]);
}
```

<details>
<summary>Respuesta</summary>

```
Level 0: 1 nodes
Level 1: 3 nodes
Level 2: 1 nodes
```

```
root         (depth 0) → counts[0]++ = 1
├── x        (depth 1) → counts[1]++ = 1
├── a/       (depth 1) → counts[1]++ = 2
│   └── y   (depth 2) → counts[2]++ = 1
└── z        (depth 1) → counts[1]++ = 3
```

El `void *user_data` es un `int[10]`, y el visitor incrementa la posición
correspondiente a la profundidad. Es el equivalente en C de:

```rust
let mut counts = [0; 10];
for (_, depth) in root.iter_with_depth() {
    counts[depth] += 1;
}
```
</details>

---

### Ejercicio 8 — DFS sin invertir hijos

**Predicción**: si NO invertimos los hijos al pushear, ¿qué orden produce?

```rust
// Versión incorrecta:
fn next(&mut self) -> Option<Self::Item> {
    let node = self.stack.pop()?;
    if let FsNode::Dir { children, .. } = node {
        for child in children {      // sin .rev()
            self.stack.push(child);
        }
    }
    Some(node)
}
```

Para el árbol `root/{x, a/{y}, z}`:

<details>
<summary>Respuesta</summary>

Produce: `root`, `z`, `a`, `y`, `x`

```
Stack: [root]       → pop root, push x, a, z (orden natural)
Stack: [x, a, z]    → pop z (último = primero en salir del stack)
Stack: [x, a]       → pop a, push y
Stack: [x, y]       → pop y
Stack: [x]          → pop x

Orden: root, z, a, y, x  (derecha antes que izquierda)
```

Sin `.rev()`, el último hijo se pushea último y sale primero (LIFO).
El recorrido es DFS pero de **derecha a izquierda** en lugar de izquierda
a derecha. Es técnicamente un DFS pre-order válido, pero con el orden de
hermanos invertido.

Para BFS esto no pasa: FIFO preserva el orden de inserción naturalmente.
</details>

---

### Ejercicio 9 — Composición de iteradores

**Predicción**: ¿qué retorna este código?

```rust
let tree = FsNode::Dir {
    name: "root".into(),
    children: vec![
        FsNode::File { name: "a".into(), size: 500 },
        FsNode::File { name: "b".into(), size: 1500 },
        FsNode::Dir {
            name: "sub".into(),
            children: vec![
                FsNode::File { name: "c".into(), size: 2000 },
                FsNode::File { name: "d".into(), size: 100 },
                FsNode::File { name: "e".into(), size: 3000 },
            ],
        },
    ],
};

let result: Vec<&str> = tree.files()
    .filter(|f| f.size() >= 1000)
    .take(2)
    .map(|f| f.name())
    .collect();
```

<details>
<summary>Respuesta</summary>

`result = ["b", "c"]`

Paso a paso (lazy, cada nodo se procesa uno por uno):

1. `files()` emite `a` → `filter(size >= 1000)` → 500 < 1000, descartado
2. `files()` emite `b` → `filter` → 1500 >= 1000 ✓ → `take(2)` → 1/2 → `map` → "b"
3. `files()` emite `c` → `filter` → 2000 >= 1000 ✓ → `take(2)` → 2/2 → `map` → "c"
4. `take(2)` ya tiene 2 → **STOP**

Los nodos `d` y `e` nunca se visitan. Toda la cadena es lazy: cada
adaptador pide al anterior "un elemento más" solo cuando lo necesita.
`take(2)` deja de pedir después de recibir 2 elementos, y la iteración
termina sin recorrer el árbol completo.
</details>

---

### Ejercicio 10 — Iterador de expresiones

**Predicción**: ¿cuántos nodos visita `iter().count()` para esta expresión?

```rust
let expr = Expr::Add(
    Box::new(Expr::Mul(
        Box::new(Expr::Num(1.0)),
        Box::new(Expr::Add(
            Box::new(Expr::Num(2.0)),
            Box::new(Expr::Num(3.0)),
        )),
    )),
    Box::new(Expr::Num(4.0)),
);
```

<details>
<summary>Respuesta</summary>

`count() = 7`

```
        Add
       /   \
     Mul    Num(4)
    /   \
 Num(1)  Add
        /   \
     Num(2)  Num(3)
```

Nodos: Add, Mul, Num(1), Add, Num(2), Num(3), Num(4) = 7

Traza del iterador:
```
Stack: [Add₁]                          → pop Add₁, push Num(4), Mul
Stack: [Mul, Num(4)]                   → pop Mul, push Add₂, Num(1)
Stack: [Num(1), Add₂, Num(4)]         → pop Num(1)
Stack: [Add₂, Num(4)]                 → pop Add₂, push Num(3), Num(2)
Stack: [Num(2), Num(3), Num(4)]       → pop Num(2)
Stack: [Num(3), Num(4)]               → pop Num(3)
Stack: [Num(4)]                        → pop Num(4)
Stack: []                              → fin
```

El iterador visita todos los nodos en pre-order sin recursión, usando
el stack explícito. `count()` consume todo el iterador, contando cada
elemento que `next()` retorna.
</details>