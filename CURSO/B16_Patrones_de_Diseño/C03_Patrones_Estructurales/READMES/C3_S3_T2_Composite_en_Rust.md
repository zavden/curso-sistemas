# Composite en Rust

## 1. Del void** al enum recursivo

En C construimos composites con vtables y `void** children` (T01). Rust ofrece
una alternativa más segura y expresiva: **enums recursivos con `Box`**. El
compilador garantiza en tiempo de compilación que:

- No hay ciclos accidentales (ownership lineal)
- No hay double free (un solo dueño)
- No hay use-after-free (borrow checker)
- Se manejan todos los variantes (exhaustive match)

```
C:                              Rust:
struct FsNode {                 enum FsNode {
    const FsOps *ops;  ← vtable    File { name: String, size: usize },
    char *name;                     Dir { name: String, children: Vec<FsNode> },
};                              }
// dispatch dinámico             // dispatch estático (match)
node->ops->size(node);          node.size()  // match interno
```

---

## 2. Enum recursivo: el composite natural de Rust

### 2.1 Filesystem virtual

```rust
#[derive(Debug)]
enum FsNode {
    File {
        name: String,
        size: usize,
    },
    Dir {
        name: String,
        children: Vec<FsNode>,  // Vec posee los hijos
    },
}
```

¿Por qué no necesitamos `Box` aquí? Porque `Vec<FsNode>` ya almacena los hijos
en el heap. El `Vec` tiene tamaño fijo en stack (puntero + len + capacity),
independientemente de cuántos hijos haya.

```
Stack:                     Heap:
Dir { name, children } ──→ [FsNode, FsNode, FsNode, ...]
     ↑ tamaño fijo          ↑ tamaño variable
```

### 2.2 Operaciones recursivas

```rust
impl FsNode {
    fn name(&self) -> &str {
        match self {
            FsNode::File { name, .. } => name,
            FsNode::Dir { name, .. } => name,
        }
    }

    fn size(&self) -> usize {
        match self {
            FsNode::File { size, .. } => *size,
            FsNode::Dir { children, .. } => {
                children.iter().map(|c| c.size()).sum()
            }
        }
    }

    fn node_count(&self) -> usize {
        match self {
            FsNode::File { .. } => 1,
            FsNode::Dir { children, .. } => {
                1 + children.iter().map(|c| c.node_count()).sum::<usize>()
            }
        }
    }

    fn max_depth(&self) -> usize {
        match self {
            FsNode::File { .. } => 0,
            FsNode::Dir { children, .. } => {
                children.iter()
                    .map(|c| c.max_depth())
                    .max()
                    .unwrap_or(0) + 1
            }
        }
    }
}
```

**Patrón idéntico a C** pero sin casts, sin vtable, sin recursión manual de
punteros. El `match` es exhaustivo: si añades una variante (ej: `Symlink`), el
compilador te obliga a manejarla en cada `match`.

### 2.3 Construcción y uso

```rust
fn main() {
    let root = FsNode::Dir {
        name: "project".into(),
        children: vec![
            FsNode::File { name: "main.c".into(), size: 2048 },
            FsNode::File { name: "Makefile".into(), size: 512 },
            FsNode::Dir {
                name: "src".into(),
                children: vec![
                    FsNode::File { name: "parser.c".into(), size: 4096 },
                    FsNode::File { name: "lexer.c".into(), size: 3072 },
                ],
            },
            FsNode::Dir {
                name: "include".into(),
                children: vec![
                    FsNode::File { name: "parser.h".into(), size: 256 },
                ],
            },
        ],
    };

    println!("Total size: {} bytes", root.size());
    println!("Node count: {}", root.node_count());
    println!("Max depth: {}", root.max_depth());
}
// Total size: 9984 bytes
// Node count: 7
// Max depth: 2
```

Sin `destroy` manual. Cuando `root` sale del scope, Rust destruye recursivamente
todo el árbol automáticamente (Drop de Vec llama Drop de cada elemento).

---

## 3. Cuándo SÍ necesitas Box

`Vec<FsNode>` resuelve el caso de "N hijos". Pero hay composites donde el
número de hijos es fijo (ej: un árbol binario):

```rust
// ¡ERROR! No compila:
enum Expr {
    Num(f64),
    Add(Expr, Expr),  // error[E0072]: recursive type has infinite size
}
```

```
¿Por qué? El compilador necesita saber el tamaño de Expr en stack:

size_of(Expr) = max(
    size_of(Num)  = size_of(f64)  = 8,
    size_of(Add)  = 2 * size_of(Expr)  = 2 * size_of(Expr) = ∞
)
```

`Box` rompe la recursión poniendo el hijo en el heap:

```rust
enum Expr {
    Num(f64),
    Add(Box<Expr>, Box<Expr>),  // Box tiene tamaño fijo (1 puntero)
}
```

```
size_of(Expr) = max(
    size_of(Num)  = 8,
    size_of(Add)  = 2 * size_of(Box<Expr>)  = 2 * 8 = 16
) + discriminante

Stack:              Heap:
Add(box1, box2) ──→ Expr::Num(3.0)
                ──→ Expr::Add(box3, box4) ──→ ...
```

---

## 4. Expresiones aritméticas con Box

### 4.1 Definición

```rust
#[derive(Debug)]
enum Expr {
    Num(f64),
    Add(Box<Expr>, Box<Expr>),
    Sub(Box<Expr>, Box<Expr>),
    Mul(Box<Expr>, Box<Expr>),
    Div(Box<Expr>, Box<Expr>),
}
```

### 4.2 Constructores ergonómicos

Escribir `Box::new(...)` en cada lugar es tedioso. Usa funciones helper:

```rust
impl Expr {
    fn num(v: f64) -> Self {
        Expr::Num(v)
    }

    fn add(l: Expr, r: Expr) -> Self {
        Expr::Add(Box::new(l), Box::new(r))
    }

    fn sub(l: Expr, r: Expr) -> Self {
        Expr::Sub(Box::new(l), Box::new(r))
    }

    fn mul(l: Expr, r: Expr) -> Self {
        Expr::Mul(Box::new(l), Box::new(r))
    }

    fn div(l: Expr, r: Expr) -> Self {
        Expr::Div(Box::new(l), Box::new(r))
    }
}
```

Ahora se construye naturalmente:

```rust
// (3 + 4) * (10 - 2)
let expr = Expr::mul(
    Expr::add(Expr::num(3.0), Expr::num(4.0)),
    Expr::sub(Expr::num(10.0), Expr::num(2.0)),
);
```

### 4.3 Evaluación

```rust
impl Expr {
    fn eval(&self) -> f64 {
        match self {
            Expr::Num(v) => *v,
            Expr::Add(l, r) => l.eval() + r.eval(),
            Expr::Sub(l, r) => l.eval() - r.eval(),
            Expr::Mul(l, r) => l.eval() * r.eval(),
            Expr::Div(l, r) => {
                let divisor = r.eval();
                if divisor == 0.0 { f64::NAN } else { l.eval() / divisor }
            }
        }
    }
}
```

### 4.4 Pretty-print

```rust
impl std::fmt::Display for Expr {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Expr::Num(v) => write!(f, "{v}"),
            Expr::Add(l, r) => write!(f, "({l} + {r})"),
            Expr::Sub(l, r) => write!(f, "({l} - {r})"),
            Expr::Mul(l, r) => write!(f, "({l} * {r})"),
            Expr::Div(l, r) => write!(f, "({l} / {r})"),
        }
    }
}

fn main() {
    let expr = Expr::mul(
        Expr::add(Expr::num(3.0), Expr::num(4.0)),
        Expr::sub(Expr::num(10.0), Expr::num(2.0)),
    );
    println!("{expr} = {}", expr.eval());
    // ((3 + 4) * (10 - 2)) = 56
}
```

### Comparación con C (T01)

```
C:                                  Rust:
bin_create('*',                     Expr::mul(
  bin_create('+',                     Expr::add(Expr::num(3.0), Expr::num(4.0)),
    num_create(3), num_create(4)),    Expr::sub(Expr::num(10.0), Expr::num(2.0)),
  bin_create('-',                   )
    num_create(10), num_create(2))
);
// 6 malloc, casts, vtables          // Box::new automáticos, sin casts
// destroy manual recursivo           // Drop automático
```

---

## 5. Vec<FsNode> vs Vec<Box<FsNode>>: cuándo usar cuál

```
Vec<FsNode>:                Vec<Box<FsNode>>:
┌──────────────────┐        ┌──────┬──────┬──────┐
│ FsNode │ FsNode  │        │ ptr  │ ptr  │ ptr  │
│ (inline, contig.)│        │  ↓   │  ↓   │  ↓   │
└──────────────────┘        │ Node │ Node │ Node │  (heap separado)
                            └──────┴──────┴──────┘
```

| Aspecto | `Vec<FsNode>` | `Vec<Box<FsNode>>` |
|---------|---------------|-------------------|
| Cache locality | Excelente (contiguo) | Pobre (indirección) |
| Mover elementos | Copia todo el FsNode | Copia solo el puntero |
| Realloc de Vec | Mueve todos los nodos | Mueve solo punteros |
| Tamaño si variantes muy dispares | Desperdicio (cada elem = variante mayor) | Eficiente (cada Box = 8 bytes) |
| Punteros estables | No (realloc invalida) | Sí (Box no se mueve) |

**Regla práctica**:
- Variantes de tamaño similar, iteración frecuente → `Vec<FsNode>`
- Variantes con tamaños muy diferentes, o necesitas punteros estables → `Vec<Box<FsNode>>`

Para nuestro filesystem simple, `Vec<FsNode>` es ideal.

---

## 6. Trait object como alternativa: dyn Component

Si necesitas **extensibilidad abierta** (nuevos tipos sin modificar el enum),
usa trait objects — el equivalente directo de las vtables de C:

```rust
trait Component {
    fn name(&self) -> &str;
    fn size(&self) -> usize;
    fn print(&self, depth: usize);
}

struct File {
    name: String,
    size: usize,
}

impl Component for File {
    fn name(&self) -> &str { &self.name }
    fn size(&self) -> usize { self.size }
    fn print(&self, depth: usize) {
        println!("{:indent$}[F] {} ({} bytes)", "", self.name, self.size,
                 indent = depth * 2);
    }
}

struct Dir {
    name: String,
    children: Vec<Box<dyn Component>>,
}

impl Component for Dir {
    fn name(&self) -> &str { &self.name }

    fn size(&self) -> usize {
        self.children.iter().map(|c| c.size()).sum()
    }

    fn print(&self, depth: usize) {
        println!("{:indent$}[D] {}/", "", self.name, indent = depth * 2);
        for child in &self.children {
            child.print(depth + 1);
        }
    }
}
```

```rust
fn main() {
    let mut root = Dir {
        name: "root".into(),
        children: Vec::new(),
    };
    root.children.push(Box::new(File { name: "a.txt".into(), size: 100 }));
    root.children.push(Box::new(File { name: "b.txt".into(), size: 200 }));

    let mut sub = Dir { name: "sub".into(), children: Vec::new() };
    sub.children.push(Box::new(File { name: "c.txt".into(), size: 50 }));
    root.children.push(Box::new(sub));

    root.print(0);
    println!("Total: {} bytes", root.size());
}
```

### Enum vs trait object: cuándo usar cuál

| Criterio | Enum | Trait object (dyn) |
|----------|------|--------------------|
| Tipos conocidos | Sí, cerrados | Abiertos, extensibles |
| Añadir tipo nuevo | Modificar enum + todos los match | Implementar trait |
| Añadir operación nueva | Un match nuevo (fácil) | Método nuevo en trait (rompe impl) |
| Pattern matching | Sí, exhaustivo | No |
| Performance | Sin indirección | Vtable dispatch |
| Datos por variante | Pueden diferir | Cada struct independiente |
| Uso idiomático Rust | ASTs, protocolos, datos | Plugins, GUI, extensibilidad |

**Es exactamente el mismo trade-off que vtable vs tagged union en C (T01 §7)**,
pero con garantías del compilador.

---

## 7. Métodos de mutación en el composite

### 7.1 Añadir y eliminar con enum

```rust
impl FsNode {
    /// Añade un hijo si self es Dir. Retorna Err si self es File.
    fn add_child(&mut self, child: FsNode) -> Result<(), FsNode> {
        match self {
            FsNode::Dir { children, .. } => {
                children.push(child);
                Ok(())
            }
            FsNode::File { .. } => Err(child),  // devuelve ownership
        }
    }

    /// Remueve un hijo por nombre. Retorna el hijo si existe.
    fn remove_child(&mut self, name: &str) -> Option<FsNode> {
        match self {
            FsNode::Dir { children, .. } => {
                let pos = children.iter().position(|c| c.name() == name)?;
                Some(children.remove(pos))  // Vec::remove mantiene orden
            }
            FsNode::File { .. } => None,
        }
    }
}
```

**Comparación con C**:
- En C: `dir_detach` retorna `FsNode *` (el caller debe liberarlo eventualmente)
- En Rust: `remove_child` retorna `Option<FsNode>` (ownership transferida, Drop automático si se ignora)

```rust
let mut root = FsNode::Dir {
    name: "root".into(),
    children: vec![
        FsNode::File { name: "temp.txt".into(), size: 100 },
        FsNode::File { name: "keep.txt".into(), size: 200 },
    ],
};

// Extraer temp.txt — ownership transferida
let removed = root.remove_child("temp.txt");
println!("{:?}", removed);  // Some(File { name: "temp.txt", size: 100 })
// removed sale del scope → Drop automático, sin leak

println!("Size after: {}", root.size());  // 200
```

### 7.2 Acceso mutable a un hijo

```rust
impl FsNode {
    fn get_child_mut(&mut self, name: &str) -> Option<&mut FsNode> {
        match self {
            FsNode::Dir { children, .. } => {
                children.iter_mut().find(|c| c.name() == name)
            }
            FsNode::File { .. } => None,
        }
    }
}

// Uso: añadir un archivo dentro de un subdirectorio
if let Some(sub) = root.get_child_mut("src") {
    sub.add_child(FsNode::File {
        name: "new_file.c".into(),
        size: 1024,
    }).ok();
}
```

El borrow checker garantiza que solo tienes **una** referencia mutable al
subárbol. No puedes tener dos `&mut` simultáneos a diferentes partes del árbol
a través del mismo `root`.

---

## 8. Búsqueda y recolección

### 8.1 Buscar por nombre (DFS)

```rust
impl FsNode {
    fn find(&self, target: &str) -> Option<&FsNode> {
        if self.name() == target {
            return Some(self);
        }
        match self {
            FsNode::Dir { children, .. } => {
                children.iter().find_map(|c| c.find(target))
            }
            FsNode::File { .. } => None,
        }
    }
}
```

Retorna `Option<&FsNode>`: una referencia prestada. No hay transferencia de
propiedad, no hay copia. El nodo encontrado sigue perteneciendo al árbol.

### 8.2 Recolectar con filtro (equivalente del acumulador de C)

```rust
impl FsNode {
    fn find_large(&self, threshold: usize) -> Vec<&FsNode> {
        let mut results = Vec::new();
        self.find_large_inner(threshold, &mut results);
        results
    }

    fn find_large_inner<'a>(&'a self, threshold: usize, out: &mut Vec<&'a FsNode>) {
        match self {
            FsNode::File { size, .. } => {
                if *size >= threshold {
                    out.push(self);
                }
            }
            FsNode::Dir { children, .. } => {
                for child in children {
                    child.find_large_inner(threshold, out);
                }
            }
        }
    }
}
```

Las lifetimes `'a` dicen: "los elementos del vector viven tanto como el árbol".
El compilador verifica que no destruyas el árbol mientras usas los resultados.

**Versión funcional** (sin acumulador explícito):

```rust
impl FsNode {
    fn collect_files(&self) -> Vec<&FsNode> {
        match self {
            FsNode::File { .. } => vec![self],
            FsNode::Dir { children, .. } => {
                children.iter().flat_map(|c| c.collect_files()).collect()
            }
        }
    }
}

// Uso con filter:
let large: Vec<_> = root.collect_files()
    .into_iter()
    .filter(|f| f.size() >= 1000)
    .collect();
```

La versión con acumulador (`find_large_inner`) es más eficiente: un solo `Vec`,
sin allocaciones intermedias. La versión funcional es más legible pero crea
vectores temporales en cada nivel.

---

## 9. Serialización y transformación del árbol

### 9.1 Mostrar como árbol (tree command)

```rust
impl FsNode {
    fn display_tree(&self, prefix: &str, is_last: bool) {
        let connector = if is_last { "└── " } else { "├── " };
        let extension = if is_last { "    " } else { "│   " };

        match self {
            FsNode::File { name, size, .. } => {
                println!("{prefix}{connector}{name} ({size} bytes)");
            }
            FsNode::Dir { name, children, .. } => {
                println!("{prefix}{connector}{name}/");
                let new_prefix = format!("{prefix}{extension}");
                for (i, child) in children.iter().enumerate() {
                    child.display_tree(&new_prefix, i == children.len() - 1);
                }
            }
        }
    }
}

// En la raíz:
println!("{}/", root.name());
if let FsNode::Dir { children, .. } = &root {
    for (i, child) in children.iter().enumerate() {
        child.display_tree("", i == children.len() - 1);
    }
}
```

Salida:
```
project/
├── main.c (2048 bytes)
├── Makefile (512 bytes)
├── src/
│   ├── parser.c (4096 bytes)
│   └── lexer.c (3072 bytes)
└── include/
    └── parser.h (256 bytes)
```

### 9.2 Map: transformar el árbol sin mutarlo

```rust
impl FsNode {
    /// Crea un nuevo árbol aplicando f a cada tamaño de archivo
    fn map_sizes<F: Fn(usize) -> usize>(&self, f: &F) -> FsNode {
        match self {
            FsNode::File { name, size } => FsNode::File {
                name: name.clone(),
                size: f(*size),
            },
            FsNode::Dir { name, children } => FsNode::Dir {
                name: name.clone(),
                children: children.iter().map(|c| c.map_sizes(f)).collect(),
            },
        }
    }
}

// Duplicar todos los tamaños
let doubled = root.map_sizes(&|s| s * 2);
println!("Original: {}", root.size());  // 9984
println!("Doubled: {}", doubled.size()); // 19968
```

Ownership claro: `root` sigue intacto, `doubled` es un árbol completamente nuevo.

---

## 10. Composite con datos compartidos: Rc y Arc

A veces quieres que un nodo aparezca en **múltiples lugares** (ej: hardlinks
en un filesystem). Ownership lineal no permite esto, pero `Rc` sí:

```rust
use std::rc::Rc;
use std::cell::RefCell;

#[derive(Debug)]
enum SharedNode {
    File {
        name: String,
        size: usize,
    },
    Dir {
        name: String,
        children: RefCell<Vec<Rc<SharedNode>>>,
    },
}

impl SharedNode {
    fn size(&self) -> usize {
        match self {
            SharedNode::File { size, .. } => *size,
            SharedNode::Dir { children, .. } => {
                children.borrow().iter().map(|c| c.size()).sum()
            }
        }
    }
}
```

```rust
fn main() {
    let shared_file = Rc::new(SharedNode::File {
        name: "shared.dat".into(),
        size: 1000,
    });

    let dir_a = Rc::new(SharedNode::Dir {
        name: "a".into(),
        children: RefCell::new(vec![Rc::clone(&shared_file)]),
    });
    let dir_b = Rc::new(SharedNode::Dir {
        name: "b".into(),
        children: RefCell::new(vec![Rc::clone(&shared_file)]),
    });

    println!("Rc count: {}", Rc::strong_count(&shared_file)); // 3

    // shared_file aparece en ambos directorios
    println!("a size: {}", dir_a.size()); // 1000
    println!("b size: {}", dir_b.size()); // 1000
}
```

```
dir_a ──→ children: [Rc ─┐]
                          ↓
                    shared.dat (refcount = 3)
                          ↑
dir_b ──→ children: [Rc ─┘]
```

**Peligro**: con `Rc` puedes crear ciclos (padre ↔ hijo) que causan memory leak.
Usa `Weak` para referencias al padre:

```rust
use std::rc::Weak;

enum NodeWithParent {
    File {
        name: String,
        size: usize,
        parent: Weak<NodeWithParent>,  // no incrementa refcount
    },
    Dir {
        name: String,
        children: RefCell<Vec<Rc<NodeWithParent>>>,
        parent: Weak<NodeWithParent>,
    },
}
```

`Weak::upgrade()` retorna `Option<Rc<T>>` — `None` si el padre ya fue destruido.

---

## 11. Comparación completa con C

| Aspecto | C (T01) | Rust enum | Rust dyn Trait |
|---------|---------|-----------|---------------|
| Polimorfismo | Vtable manual | match exhaustivo | Vtable automática |
| Hijos | `void**` o `FsNode**` | `Vec<FsNode>` | `Vec<Box<dyn T>>` |
| Extensibilidad de tipos | Crear nueva vtable | Añadir variante (recompilar) | Implementar trait |
| Extensibilidad de ops | Modificar vtable struct | Nuevo match (fácil) | Nuevo método (recompilar) |
| Destrucción | `destroy` recursivo manual | Drop automático | Drop automático |
| Ciclos | Posibles (crash) | Imposibles con ownership | Posibles con Rc (leak) |
| Cast entre tipos | Manual, inseguro | No necesario | Downcast con Any |
| Tamaño en memoria | Solo lo necesario + vtable ptr | Variante más grande + tag | Struct + Box + vtable |
| Propiedad de hijos | Convención (documentar) | Ownership o Rc explícito | Box = dueño, Rc = compartido |
| Detección de errores | Runtime (segfault) | Compilación (match, borrow) | Compilación |

---

## 12. Errores comunes

| Error | Problema | Solución |
|-------|----------|----------|
| `enum E { A(E) }` sin Box | Tipo de tamaño infinito | `A(Box<E>)` o `A(Vec<E>)` |
| Match no exhaustivo | Variante nueva no manejada | Compilador lo detecta (`_` oculta el error) |
| `&mut` a dos hijos simultáneos | Borrow checker rechaza | `split_at_mut` o indices |
| Rc sin Weak para padre | Ciclo de referencias, leak | `parent: Weak<Node>` |
| `collect_files` crea muchos Vec | Allocaciones O(depth) | Usar acumulador `&mut Vec` |
| Clonar árbol grande sin necesidad | Performance | Usar `&FsNode` (préstamo) |
| Wildcard `_` en match | Nuevo variante no detectado | Match explícito cada variante |
| `RefCell` borrow doble | panic en runtime | Minimizar scope de `borrow()` |
| `Rc::clone` confundido con deep clone | Comparten el mismo dato | `Rc::clone` = incrementar refcount |
| Box en Vec innecesariamente | Indirección extra sin beneficio | `Vec<Node>` si no necesitas punteros estables |

---

## 13. Ejercicios

### Ejercicio 1 — Enum básico

**Predicción**: ¿qué imprime este código?

```rust
enum Node {
    Leaf(i32),
    Branch(Vec<Node>),
}

fn sum(node: &Node) -> i32 {
    match node {
        Node::Leaf(v) => *v,
        Node::Branch(children) => children.iter().map(sum).sum(),
    }
}

fn main() {
    let tree = Node::Branch(vec![
        Node::Leaf(10),
        Node::Branch(vec![Node::Leaf(20), Node::Leaf(30)]),
        Node::Leaf(40),
    ]);
    println!("{}", sum(&tree));
}
```

<details>
<summary>Respuesta</summary>

Imprime `100`.

```
Branch
├── Leaf(10)
├── Branch
│   ├── Leaf(20)
│   └── Leaf(30)
└── Leaf(40)

sum(Branch) = sum(10) + sum(Branch[20,30]) + sum(40)
            = 10 + (20 + 30) + 40
            = 100
```

`children.iter().map(sum).sum()` aplica `sum` a cada hijo y acumula.
El `map(sum)` funciona porque `sum` tiene la firma `fn(&Node) -> i32`,
que coincide con lo que `iter()` produce (`&Node`).
</details>

---

### Ejercicio 2 — Box obligatorio

**Predicción**: ¿cuál compila y cuál no?

```rust
// A
enum ExprA {
    Num(f64),
    Add(ExprA, ExprA),
}

// B
enum ExprB {
    Num(f64),
    Add(Box<ExprB>, Box<ExprB>),
}

// C
enum ExprC {
    Num(f64),
    Add(Vec<ExprC>),
}
```

<details>
<summary>Respuesta</summary>

- **A**: No compila. `Add(ExprA, ExprA)` tiene tamaño infinito: para saber
  cuánto mide `ExprA`, necesitas saber cuánto mide `ExprA`. Error:
  `recursive type has infinite size`.

- **B**: Compila. `Box<ExprB>` tiene tamaño fijo (8 bytes en 64-bit),
  independientemente de qué contenga. Rompe la recursión de tamaño.

- **C**: Compila. `Vec<ExprC>` tiene tamaño fijo (24 bytes: ptr + len + cap).
  Los elementos se almacenan en el heap, igual que `Box`.

Regla: cualquier tipo que ponga la recursión en el heap (`Box`, `Vec`, `Rc`,
`Arc`) rompe el ciclo de tamaño. El problema es solo cuando intentas embeber
el tipo recursivo **directamente** en stack.
</details>

---

### Ejercicio 3 — Ownership en remove_child

**Predicción**: ¿hay memory leak en este código?

```rust
let mut dir = FsNode::Dir {
    name: "tmp".into(),
    children: vec![
        FsNode::File { name: "a.txt".into(), size: 100 },
        FsNode::File { name: "b.txt".into(), size: 200 },
    ],
};

let _ = dir.remove_child("a.txt");  // descarta el resultado
println!("Size: {}", dir.size());
```

<details>
<summary>Respuesta</summary>

**No hay memory leak**. Imprime `Size: 200`.

`remove_child("a.txt")` retorna `Some(FsNode::File { name: "a.txt", size: 100 })`.
Al asignar a `let _ =`, el valor retornado se destruye inmediatamente
(Drop se ejecuta al final de la sentencia).

En C, esto sería un leak: `dir_detach` retorna un puntero, y si no llamas
`destroy`, se pierde. En Rust, Drop se ejecuta automáticamente cuando el
valor sale del scope, incluso si lo ignoras con `let _ =`.

Nota: `let _x = ...` (con nombre) destruye al final del scope. `let _ = ...`
(sin nombre) destruye inmediatamente. Ambos evitan leaks.
</details>

---

### Ejercicio 4 — Trait object composite

**Predicción**: ¿compila este código?

```rust
trait Widget {
    fn draw(&self);
}

struct Button { label: String }
impl Widget for Button {
    fn draw(&self) { println!("Button: {}", self.label); }
}

struct Panel {
    children: Vec<Box<dyn Widget>>,
}
impl Widget for Panel {
    fn draw(&self) {
        println!("Panel:");
        for child in &self.children {
            child.draw();
        }
    }
}

fn main() {
    let ui: Box<dyn Widget> = Box::new(Panel {
        children: vec![
            Box::new(Button { label: "OK".into() }),
            Box::new(Panel {
                children: vec![
                    Box::new(Button { label: "Cancel".into() }),
                ],
            }),
        ],
    });
    ui.draw();
}
```

<details>
<summary>Respuesta</summary>

Sí compila. Imprime:

```
Panel:
Button: OK
Panel:
Button: Cancel
```

`Panel` implementa `Widget` y contiene `Vec<Box<dyn Widget>>`. Como `Panel`
implementa `Widget`, puede ser un hijo de otro `Panel` vía `Box<dyn Widget>`.

Esto es el patrón Composite con trait objects: `Panel` es el composite,
`Button` es la hoja, `Widget` es la interfaz común. Es el equivalente directo
de las vtables de C, pero el compilador genera las vtables automáticamente.
</details>

---

### Ejercicio 5 — flat_map vs acumulador

**Predicción**: ¿cuántas allocaciones de `Vec` se hacen en cada versión para
un árbol con 3 niveles de directorios y 2 archivos por directorio?

```rust
// Versión A: flat_map
fn collect_a(node: &FsNode) -> Vec<&FsNode> {
    match node {
        FsNode::File { .. } => vec![node],
        FsNode::Dir { children, .. } => {
            children.iter().flat_map(|c| collect_a(c)).collect()
        }
    }
}

// Versión B: acumulador
fn collect_b<'a>(node: &'a FsNode, out: &mut Vec<&'a FsNode>) {
    match node {
        FsNode::File { .. } => out.push(node),
        FsNode::Dir { children, .. } => {
            for child in children {
                collect_b(child, out);
            }
        }
    }
}
```

<details>
<summary>Respuesta</summary>

Árbol:
```
root/           (nivel 0)
├── f1
├── f2
├── sub1/       (nivel 1)
│   ├── f3
│   ├── f4
│   └── sub2/   (nivel 2)
│       ├── f5
│       └── f6
```

**Versión A (flat_map)**:
- `collect_a(f1)` → `vec![f1]` → 1 alloc
- `collect_a(f2)` → `vec![f2]` → 1 alloc
- `collect_a(f3)` → `vec![f3]` → 1 alloc
- `collect_a(f4)` → `vec![f4]` → 1 alloc
- `collect_a(f5)` → `vec![f5]` → 1 alloc
- `collect_a(f6)` → `vec![f6]` → 1 alloc
- `collect_a(sub2)` → `flat_map` + collect → 1 alloc
- `collect_a(sub1)` → `flat_map` + collect → 1 alloc
- `collect_a(root)` → `flat_map` + collect → 1 alloc
- **Total: 9 allocations** (6 hojas + 3 directorios)

**Versión B (acumulador)**:
- Se crea un solo `Vec` antes de llamar a `collect_b`
- Todos los `push` operan sobre el mismo Vec
- **Total: 1 allocation** (más posibles reallocaciones del Vec al crecer)

La versión B es mucho más eficiente para árboles grandes. Los Vec intermedios
de la versión A se crean, se copian al collect, y se destruyen inmediatamente.
</details>

---

### Ejercicio 6 — Map sobre el árbol

**Predicción**: ¿qué imprime este código?

```rust
let tree = FsNode::Dir {
    name: "root".into(),
    children: vec![
        FsNode::File { name: "a.txt".into(), size: 100 },
        FsNode::Dir {
            name: "sub".into(),
            children: vec![
                FsNode::File { name: "b.txt".into(), size: 200 },
            ],
        },
    ],
};

let transformed = tree.map_sizes(&|s| s + 50);
println!("Original: {}", tree.size());
println!("Transformed: {}", transformed.size());
```

<details>
<summary>Respuesta</summary>

```
Original: 300
Transformed: 400
```

`map_sizes` crea un árbol **nuevo** sin modificar el original.

- Original: a.txt(100) + b.txt(200) = 300
- Transformado: a.txt(100+50=150) + b.txt(200+50=250) = 400

Los directorios no tienen tamaño propio; solo suman sus hijos.
Cada archivo recibe +50, y hay 2 archivos, así que el total crece en 100.

`tree` sigue intacto porque `map_sizes` toma `&self` (referencia inmutable)
y clona los nombres para construir el nuevo árbol.
</details>

---

### Ejercicio 7 — Rc y conteo de referencias

**Predicción**: ¿cuál es el strong_count de `shared` en cada punto marcado?

```rust
use std::rc::Rc;

let shared = Rc::new(FsNode::File { name: "data.bin".into(), size: 5000 });
// Punto A
println!("A: {}", Rc::strong_count(&shared));

let clone1 = Rc::clone(&shared);
// Punto B
println!("B: {}", Rc::strong_count(&shared));

{
    let clone2 = Rc::clone(&shared);
    // Punto C
    println!("C: {}", Rc::strong_count(&shared));
}
// Punto D
println!("D: {}", Rc::strong_count(&shared));

drop(clone1);
// Punto E
println!("E: {}", Rc::strong_count(&shared));
```

<details>
<summary>Respuesta</summary>

```
A: 1
B: 2
C: 3
D: 2
E: 1
```

- **A**: Solo `shared` existe → count = 1
- **B**: `clone1` es un nuevo `Rc` al mismo dato → count = 2
- **C**: `clone2` dentro del bloque → count = 3
- **D**: `clone2` salió del scope → Drop decrementa → count = 2
- **E**: `drop(clone1)` explícito → count = 1

El dato (`File`) se destruye solo cuando el count llega a 0 (cuando `shared`
salga del scope). `Rc::clone` NO clona el dato: incrementa el contador.
</details>

---

### Ejercicio 8 — Wildcard peligroso

**Predicción**: ¿qué problema introduce el wildcard al añadir `Symlink`?

```rust
enum FsNode {
    File { name: String, size: usize },
    Dir { name: String, children: Vec<FsNode> },
    Symlink { name: String, target: String },  // NUEVO
}

impl FsNode {
    fn size(&self) -> usize {
        match self {
            FsNode::File { size, .. } => *size,
            FsNode::Dir { children, .. } => children.iter().map(|c| c.size()).sum(),
            _ => 0,  // "catch-all" para todo lo demás
        }
    }
}
```

<details>
<summary>Respuesta</summary>

El `_ => 0` compila sin warnings, pero **silencia** la nueva variante `Symlink`.
Si después añades `FsNode::Mount { ... }` o `FsNode::Pipe { ... }`, el `_`
los atrapa silenciosamente también, retornando 0 sin que te des cuenta.

Sin el wildcard:
```rust
fn size(&self) -> usize {
    match self {
        FsNode::File { size, .. } => *size,
        FsNode::Dir { children, .. } => children.iter().map(|c| c.size()).sum(),
        // error[E0004]: non-exhaustive patterns: `Symlink` not covered
    }
}
```

El compilador te **obliga** a decidir qué hacer con `Symlink`.

Solución correcta:
```rust
FsNode::Symlink { .. } => 0, // Explícitamente: symlinks no tienen tamaño propio
```

**Regla**: evita `_` en match sobre tus propios enums. Listá cada variante
explícitamente para que el compilador detecte adiciones futuras.
</details>

---

### Ejercicio 9 — Expresión con eval

**Predicción**: ¿qué retorna `expr.eval()`?

```rust
let expr = Expr::add(
    Expr::mul(Expr::num(2.0), Expr::num(5.0)),
    Expr::div(Expr::num(9.0), Expr::num(0.0)),
);
println!("{}", expr.eval());
```

<details>
<summary>Respuesta</summary>

Imprime `NaN`.

```
      [+]
     /   \
   [*]   [/]
  / \    / \
 2   5  9   0

eval(*) = 2.0 * 5.0 = 10.0
eval(/) = 9.0 / 0.0 → divisor es 0.0 → retorna f64::NAN
eval(+) = 10.0 + NaN = NaN
```

Cualquier operación aritmética con `NaN` produce `NaN`. Es "contagioso":
una vez que aparece en una rama del árbol, se propaga hasta la raíz.

En nuestra implementación, `Div` verifica `divisor == 0.0` y retorna `NAN`
explícitamente. Nota: en Rust, `f64::NAN != f64::NAN` (es `false`), así que
`expr.eval() == f64::NAN` siempre es false. Usa `f64::is_nan()` para verificar.
</details>

---

### Ejercicio 10 — Display tree

**Predicción**: ¿qué imprime `display_tree` para este árbol?

```rust
let tree = FsNode::Dir {
    name: "app".into(),
    children: vec![
        FsNode::File { name: "main.rs".into(), size: 500 },
        FsNode::Dir {
            name: "tests".into(),
            children: vec![
                FsNode::File { name: "test_a.rs".into(), size: 300 },
                FsNode::File { name: "test_b.rs".into(), size: 400 },
            ],
        },
        FsNode::File { name: "lib.rs".into(), size: 200 },
    ],
};
```

<details>
<summary>Respuesta</summary>

```
app/
├── main.rs (500 bytes)
├── tests/
│   ├── test_a.rs (300 bytes)
│   └── test_b.rs (400 bytes)
└── lib.rs (200 bytes)
```

La función `display_tree` usa dos variables según `is_last`:
- `connector`: `├── ` (no último) o `└── ` (último hijo)
- `extension`: `│   ` (no último) o `    ` (último hijo, ya no hay línea vertical)

Para `tests/` (no es último hijo de `app`):
- Conector: `├── `
- Sus hijos usan prefix `│   ` (porque la línea vertical continúa para `lib.rs`)
- `test_b.rs` es último hijo de `tests` → usa `└── `

Para `lib.rs` (último hijo de `app`):
- Conector: `└── `
- No hay más hermanos, así que no hay línea vertical debajo
</details>