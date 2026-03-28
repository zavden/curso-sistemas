# Listas no homogéneas

## El problema

Todas las listas vistas hasta ahora son **homogéneas**: cada nodo almacena el
mismo tipo de dato (`int`, `float`, `T`).  Pero hay escenarios donde una lista
necesita contener datos de **tipos distintos** en la misma cadena:

```
[42] → ["hello"] → [3.14] → [true] → NULL
 int     string     float     bool
```

Ejemplos prácticos:
- Un parser que produce tokens de distintos tipos (número, string, operador).
- Un sistema de eventos donde cada evento tiene payload diferente.
- Un intérprete de lenguaje dinámico donde los valores son heterogéneos.
- Serialización/deserialización de mensajes con campos variables.

Ni C ni Rust permiten esto directamente con tipado estático.  Ambos lenguajes
ofrecen mecanismos distintos para resolverlo.

---

## Estrategia en C: void* + tag

### Diseño

Cada nodo almacena un `void *` (puntero genérico) al dato y un **tag** que
indica el tipo real:

```c
typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_BOOL,
} DataType;

typedef struct Node {
    DataType type;
    void *data;
    struct Node *next;
} Node;
```

El tag es un entero que el programador consulta antes de hacer cast:

```c
void print_node(const Node *node) {
    switch (node->type) {
        case TYPE_INT:
            printf("%d", *(int *)node->data);
            break;
        case TYPE_FLOAT:
            printf("%.2f", *(float *)node->data);
            break;
        case TYPE_STRING:
            printf("\"%s\"", (char *)node->data);
            break;
        case TYPE_BOOL:
            printf("%s", *(int *)node->data ? "true" : "false");
            break;
    }
}
```

### Crear nodos

Cada tipo requiere su propia función de creación (o una genérica con
`memcpy`):

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Node *create_int_node(int value) {
    Node *node = malloc(sizeof(Node));
    node->type = TYPE_INT;
    node->data = malloc(sizeof(int));
    *(int *)node->data = value;
    node->next = NULL;
    return node;
}

Node *create_float_node(float value) {
    Node *node = malloc(sizeof(Node));
    node->type = TYPE_FLOAT;
    node->data = malloc(sizeof(float));
    *(float *)node->data = value;
    node->next = NULL;
    return node;
}

Node *create_string_node(const char *value) {
    Node *node = malloc(sizeof(Node));
    node->type = TYPE_STRING;
    node->data = strdup(value);   // malloc + strcpy
    node->next = NULL;
    return node;
}

Node *create_bool_node(int value) {
    Node *node = malloc(sizeof(Node));
    node->type = TYPE_BOOL;
    node->data = malloc(sizeof(int));
    *(int *)node->data = value;
    node->next = NULL;
    return node;
}
```

Cada nodo requiere **dos allocations**: una para el `Node` y otra para el
dato apuntado por `void *`.

### Recorrido e impresión

```c
void print_list(const Node *head) {
    printf("[");
    const Node *cur = head;
    while (cur) {
        print_node(cur);
        if (cur->next) printf(", ");
        cur = cur->next;
    }
    printf("]\n");
}
```

### Liberación

Cada tipo puede requerir liberación diferente:

```c
void free_node(Node *node) {
    free(node->data);   // liberar el dato (todos usan malloc)
    free(node);         // liberar el nodo
}

void free_list(Node *head) {
    while (head) {
        Node *next = head->next;
        free_node(head);
        head = next;
    }
}
```

### Programa completo en C

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { TYPE_INT, TYPE_FLOAT, TYPE_STRING, TYPE_BOOL } DataType;

typedef struct Node {
    DataType type;
    void *data;
    struct Node *next;
} Node;

Node *create_int_node(int v) {
    Node *n = malloc(sizeof(Node));
    n->type = TYPE_INT;
    n->data = malloc(sizeof(int));
    *(int *)n->data = v;
    n->next = NULL;
    return n;
}

Node *create_float_node(float v) {
    Node *n = malloc(sizeof(Node));
    n->type = TYPE_FLOAT;
    n->data = malloc(sizeof(float));
    *(float *)n->data = v;
    n->next = NULL;
    return n;
}

Node *create_string_node(const char *v) {
    Node *n = malloc(sizeof(Node));
    n->type = TYPE_STRING;
    n->data = strdup(v);
    n->next = NULL;
    return n;
}

void push_front(Node **head, Node *node) {
    node->next = *head;
    *head = node;
}

void print_node(const Node *n) {
    switch (n->type) {
        case TYPE_INT:    printf("%d", *(int *)n->data); break;
        case TYPE_FLOAT:  printf("%.2f", *(float *)n->data); break;
        case TYPE_STRING: printf("\"%s\"", (char *)n->data); break;
        case TYPE_BOOL:   printf("%s", *(int *)n->data ? "true" : "false"); break;
    }
}

void print_list(const Node *head) {
    printf("[");
    while (head) {
        print_node(head);
        if (head->next) printf(", ");
        head = head->next;
    }
    printf("]\n");
}

void free_list(Node *head) {
    while (head) {
        Node *next = head->next;
        free(head->data);
        free(head);
        head = next;
    }
}

int main(void) {
    Node *list = NULL;
    push_front(&list, create_string_node("world"));
    push_front(&list, create_float_node(3.14));
    push_front(&list, create_int_node(42));

    print_list(list);   // [42, 3.14, "world"]
    free_list(list);
    return 0;
}
```

### Problemas de void*

| Problema | Consecuencia |
|----------|-------------|
| Cast incorrecto | `*(float *)node->data` cuando es `int` → UB |
| Tag desactualizado | Cambiar el dato sin cambiar el tag → cast incorrecto |
| Doble allocation | 2× `malloc` por nodo (nodo + dato) |
| Sin verificación | El compilador no verifica que el cast coincide con el tag |
| Extensibilidad | Añadir un tipo requiere modificar el `switch` en todas las funciones |

---

## Estrategia en C: tagged union

Una alternativa más eficiente: embeber el dato directamente en el nodo con
una `union`:

```c
typedef enum { TYPE_INT, TYPE_FLOAT, TYPE_STRING } DataType;

typedef union {
    int i;
    float f;
    char *s;      // strings todavía necesitan heap
} DataValue;

typedef struct Node {
    DataType type;
    DataValue value;
    struct Node *next;
} Node;
```

La `union` ocupa el tamaño del miembro más grande.  En este caso, `char *`
(8 bytes en 64-bit) domina:

```
sizeof(DataValue) = max(sizeof(int), sizeof(float), sizeof(char *)) = 8
sizeof(Node) = type(4) + padding(4) + value(8) + next(8) = 24 bytes
```

**Una sola allocation** por nodo — sin `void *` ni `malloc` extra para el dato
(excepto strings que viven en el heap).

### Uso

```c
Node *create_int_node(int v) {
    Node *n = malloc(sizeof(Node));
    n->type = TYPE_INT;
    n->value.i = v;         // acceso directo, sin cast
    n->next = NULL;
    return n;
}

Node *create_float_node(float v) {
    Node *n = malloc(sizeof(Node));
    n->type = TYPE_FLOAT;
    n->value.f = v;
    n->next = NULL;
    return n;
}

Node *create_string_node(const char *v) {
    Node *n = malloc(sizeof(Node));
    n->type = TYPE_STRING;
    n->value.s = strdup(v);
    n->next = NULL;
    return n;
}

void print_node(const Node *n) {
    switch (n->type) {
        case TYPE_INT:    printf("%d", n->value.i); break;
        case TYPE_FLOAT:  printf("%.2f", n->value.f); break;
        case TYPE_STRING: printf("\"%s\"", n->value.s); break;
    }
}

void free_node(Node *n) {
    if (n->type == TYPE_STRING) free(n->value.s);
    free(n);
}
```

### void* vs tagged union

| Aspecto | `void *` | Tagged union |
|---------|---------|-------------|
| Allocations por nodo | 2 (nodo + dato) | 1 (nodo contiene dato) |
| Acceso al dato | Cast: `*(int *)node->data` | Directo: `node->value.i` |
| Seguridad | Ninguna (cast erróneo = UB) | Ninguna (acceder miembro incorrecto = UB) |
| Memoria | Más (puntero + heap extra) | Menos (dato inline) |
| Flexibilidad | Ilimitada (cualquier tipo) | Limitada a miembros del union |
| Tipos grandes | Solo puntero en el nodo | El union crece al tamaño del mayor |

La tagged union es preferible cuando los tipos son pocos y conocidos.
`void *` es preferible cuando los tipos son arbitrarios o determinados por
el usuario.

---

## Estrategia en Rust: enum con variantes

El `enum` de Rust es una **tagged union** verificada por el compilador:

```rust
enum Value {
    Int(i32),
    Float(f64),
    Text(String),
    Bool(bool),
}
```

El compilador garantiza que solo accedes al dato correcto:

```rust
fn print_value(val: &Value) {
    match val {
        Value::Int(n)   => print!("{n}"),
        Value::Float(f) => print!("{f:.2}"),
        Value::Text(s)  => print!("\"{s}\""),
        Value::Bool(b)  => print!("{b}"),
    }
}
```

### El match es exhaustivo

```rust
fn broken(val: &Value) {
    match val {
        Value::Int(n) => println!("{n}"),
        Value::Float(f) => println!("{f}"),
        // ERROR de compilación: non-exhaustive patterns
        // Value::Text y Value::Bool no cubiertos
    }
}
```

En C, olvidar un caso en el `switch` compila sin error.  En Rust, el
compilador rechaza el programa.

### Lista no homogénea con enum

```rust
struct Node {
    data: Value,
    next: Option<Box<Node>>,
}

struct HeteroList {
    head: Option<Box<Node>>,
}

impl HeteroList {
    fn new() -> Self {
        HeteroList { head: None }
    }

    fn push_front(&mut self, value: Value) {
        self.head = Some(Box::new(Node {
            data: value,
            next: self.head.take(),
        }));
    }

    fn print(&self) {
        print!("[");
        let mut cur = &self.head;
        let mut first = true;
        while let Some(node) = cur {
            if !first { print!(", "); }
            print_value(&node.data);
            cur = &node.next;
            first = false;
        }
        println!("]");
    }
}

fn main() {
    let mut list = HeteroList::new();
    list.push_front(Value::Bool(true));
    list.push_front(Value::Text("hello".to_string()));
    list.push_front(Value::Float(3.14));
    list.push_front(Value::Int(42));

    list.print();   // [42, 3.14, "hello", true]
}
```

### Métodos en el enum

```rust
impl Value {
    fn type_name(&self) -> &str {
        match self {
            Value::Int(_)   => "int",
            Value::Float(_) => "float",
            Value::Text(_)  => "string",
            Value::Bool(_)  => "bool",
        }
    }

    fn as_int(&self) -> Option<i32> {
        match self {
            Value::Int(n) => Some(*n),
            _ => None,
        }
    }

    fn is_numeric(&self) -> bool {
        matches!(self, Value::Int(_) | Value::Float(_))
    }
}
```

### Memoria del enum

El tamaño de un enum Rust es el de la variante más grande + el discriminante
(tag):

```rust
use std::mem::size_of;

// size_of::<Value>()
// Variantes:     Int(i32)=4, Float(f64)=8, Text(String)=24, Bool(bool)=1
// Mayor:         String = 24 bytes
// Discriminante: 8 bytes (alineación a 8)
// Total:         32 bytes
```

Todas las variantes ocupan 32 bytes, incluso `Bool(bool)` que solo necesita
1 byte de dato.  El espacio extra se desperdicia (padding).  Esto es el
mismo trade-off que la `union` de C.

---

## Estrategia en Rust: Box<dyn Any>

Para tipos verdaderamente arbitrarios (no conocidos en compilación):

```rust
use std::any::Any;

struct DynNode {
    data: Box<dyn Any>,
    next: Option<Box<DynNode>>,
}

struct DynList {
    head: Option<Box<DynNode>>,
}

impl DynList {
    fn new() -> Self {
        DynList { head: None }
    }

    fn push_front<T: Any>(&mut self, value: T) {
        self.head = Some(Box::new(DynNode {
            data: Box::new(value),
            next: self.head.take(),
        }));
    }
}
```

`Box<dyn Any>` es un trait object — un puntero al dato + un puntero a la
vtable del trait `Any`.  El tamaño es 16 bytes (2 punteros), independiente
del tipo real.

### Downcast

Para recuperar el tipo original:

```rust
fn process(node: &DynNode) {
    if let Some(n) = node.data.downcast_ref::<i32>() {
        println!("int: {n}");
    } else if let Some(f) = node.data.downcast_ref::<f64>() {
        println!("float: {f}");
    } else if let Some(s) = node.data.downcast_ref::<String>() {
        println!("string: {s}");
    } else {
        println!("unknown type");
    }
}
```

`downcast_ref::<T>()` verifica en runtime si el tipo real es `T`.  Retorna
`Option<&T>`.

### Limitaciones de Any

- `downcast` es verificación **runtime** — si olvidas un tipo, compila pero
  no lo procesa.
- No es exhaustivo — a diferencia de `match` en un enum, no hay error de
  compilación si no cubres todos los tipos.
- Requiere `'static` — el tipo no puede contener referencias con lifetime.
- No hay acceso a los datos sin saber el tipo — no puedes "imprimir" un
  `dyn Any` genéricamente (necesitas implementar un trait propio).

---

## Comparación de estrategias

| Aspecto | C `void*` | C tagged union | Rust `enum` | Rust `Box<dyn Any>` |
|---------|----------|---------------|-------------|-------------------|
| Tipos conocidos en compilación | No requiere | Sí | Sí | No requiere |
| Verificación de tipo | Manual (tag + cast) | Manual (tag + miembro) | Compilador (match exhaustivo) | Runtime (downcast) |
| Error por tipo incorrecto | UB silencioso | UB silencioso | No compila | None (falla limpia) |
| Memoria por dato | heap extra | inline (union) | inline (enum) | heap (Box) |
| Extensibilidad | Ilimitada | Modificar union + switch | Modificar enum + match | Ilimitada |
| Allocations por nodo | 2 | 1 | 1 | 2 (nodo + dato) |
| Añadir tipo nuevo | No requiere recompilación | Recompilar | Recompilar (error si falta match) | No requiere recompilación |

### Recomendación

```
¿Conoces todos los tipos posibles en compilación?
├── Sí → Rust: enum (verificación exhaustiva)
│        C: tagged union (eficiente)
└── No → Rust: Box<dyn Any> o trait object propio
         C: void* (máxima flexibilidad)
```

---

## Trait objects como alternativa a Any

Un patrón más idiomático en Rust: definir un trait propio en vez de usar `Any`:

```rust
use std::fmt;

trait Printable: fmt::Display {
    fn type_name(&self) -> &str;
    fn clone_box(&self) -> Box<dyn Printable>;
}

impl Printable for i32 {
    fn type_name(&self) -> &str { "int" }
    fn clone_box(&self) -> Box<dyn Printable> { Box::new(*self) }
}

impl Printable for f64 {
    fn type_name(&self) -> &str { "float" }
    fn clone_box(&self) -> Box<dyn Printable> { Box::new(*self) }
}

impl Printable for String {
    fn type_name(&self) -> &str { "string" }
    fn clone_box(&self) -> Box<dyn Printable> { Box::new(self.clone()) }
}

struct TraitNode {
    data: Box<dyn Printable>,
    next: Option<Box<TraitNode>>,
}

struct TraitList {
    head: Option<Box<TraitNode>>,
}

impl TraitList {
    fn new() -> Self {
        TraitList { head: None }
    }

    fn push_front(&mut self, value: Box<dyn Printable>) {
        self.head = Some(Box::new(TraitNode {
            data: value,
            next: self.head.take(),
        }));
    }

    fn print(&self) {
        let mut cur = &self.head;
        while let Some(node) = cur {
            println!("{} ({})", node.data, node.data.type_name());
            cur = &node.next;
        }
    }
}

fn main() {
    let mut list = TraitList::new();
    list.push_front(Box::new("hello".to_string()));
    list.push_front(Box::new(3.14_f64));
    list.push_front(Box::new(42_i32));

    list.print();
    // 42 (int)
    // 3.14 (float)
    // hello (string)
}
```

Ventaja sobre `Any`: podemos llamar métodos del trait (`Display`,
`type_name`) **sin** downcast.  Desventaja: hay que implementar el trait
para cada tipo.

---

## Programa completo: C vs Rust

### C (tagged union)

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { VAL_INT, VAL_FLOAT, VAL_STR } ValType;

typedef struct Node {
    ValType type;
    union {
        int i;
        float f;
        char *s;
    } val;
    struct Node *next;
} Node;

Node *int_node(int v) {
    Node *n = malloc(sizeof(Node));
    n->type = VAL_INT; n->val.i = v; n->next = NULL;
    return n;
}

Node *float_node(float v) {
    Node *n = malloc(sizeof(Node));
    n->type = VAL_FLOAT; n->val.f = v; n->next = NULL;
    return n;
}

Node *str_node(const char *v) {
    Node *n = malloc(sizeof(Node));
    n->type = VAL_STR; n->val.s = strdup(v); n->next = NULL;
    return n;
}

void push(Node **head, Node *node) {
    node->next = *head;
    *head = node;
}

void print_list(const Node *n) {
    printf("[");
    while (n) {
        switch (n->type) {
            case VAL_INT:   printf("%d", n->val.i); break;
            case VAL_FLOAT: printf("%.2f", n->val.f); break;
            case VAL_STR:   printf("\"%s\"", n->val.s); break;
        }
        if (n->next) printf(", ");
        n = n->next;
    }
    printf("]\n");
}

void free_list(Node *n) {
    while (n) {
        Node *next = n->next;
        if (n->type == VAL_STR) free(n->val.s);
        free(n);
        n = next;
    }
}

int main(void) {
    Node *list = NULL;
    push(&list, str_node("world"));
    push(&list, float_node(3.14));
    push(&list, int_node(42));

    print_list(list);    // [42, 3.14, "world"]
    free_list(list);
    return 0;
}
```

### Rust (enum)

```rust
use std::fmt;

enum Value {
    Int(i32),
    Float(f64),
    Text(String),
}

impl fmt::Display for Value {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Value::Int(n)   => write!(f, "{n}"),
            Value::Float(v) => write!(f, "{v:.2}"),
            Value::Text(s)  => write!(f, "\"{s}\""),
        }
    }
}

struct Node {
    data: Value,
    next: Option<Box<Node>>,
}

struct HeteroList {
    head: Option<Box<Node>>,
}

impl HeteroList {
    fn new() -> Self { HeteroList { head: None } }

    fn push_front(&mut self, value: Value) {
        self.head = Some(Box::new(Node {
            data: value,
            next: self.head.take(),
        }));
    }
}

impl fmt::Display for HeteroList {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "[")?;
        let mut cur = &self.head;
        let mut first = true;
        while let Some(node) = cur {
            if !first { write!(f, ", ")?; }
            write!(f, "{}", node.data)?;
            cur = &node.next;
            first = false;
        }
        write!(f, "]")
    }
}

fn main() {
    let mut list = HeteroList::new();
    list.push_front(Value::Text("world".to_string()));
    list.push_front(Value::Float(3.14));
    list.push_front(Value::Int(42));

    println!("{list}");   // [42, 3.14, "world"]
}
```

---

## Ejercicios

### Ejercicio 1 — Tagged union en C

Implementa una lista no homogénea en C con tagged union que soporte `int`,
`double`, y `char *`.  Crea la lista `[42, 3.14, "hello"]` e imprímela.

<details>
<summary>Punto clave</summary>

La `union` contiene los 3 tipos.  `double` (8 bytes) es el mayor, así que
la union ocupa 8 bytes.  El `switch` en `print` debe cubrir los 3 tipos.
Liberar strings con `free`, pero no ints ni doubles.
</details>

### Ejercicio 2 — void* vs tagged union: memoria

Calcula la memoria por nodo para ambas estrategias, con datos `int` en
plataforma de 64 bits.

<details>
<summary>Cálculo</summary>

```
void*:
  Node: type(4) + pad(4) + data_ptr(8) + next(8) = 24 bytes
  + malloc para int: 4 bytes (+ overhead de malloc ~16 bytes)
  Total efectivo: ~40+ bytes

Tagged union:
  Node: type(4) + pad(4) + union(8) + next(8) = 24 bytes
  Total: 24 bytes (sin allocation extra)
```

La tagged union ahorra la segunda allocation y su overhead de `malloc`.
</details>

### Ejercicio 3 — Enum exhaustivo

Añade una variante `Value::List(Vec<Value>)` al enum de Rust (un valor que
es una lista de valores — estructura recursiva).  Implementa `Display` para
ella.  ¿El compilador te obliga a manejar el nuevo caso?

<details>
<summary>Respuesta</summary>

Sí — al añadir `List(Vec<Value>)`, todo `match` existente que no incluya
esta variante causa error de compilación:

```
error[E0004]: non-exhaustive patterns: `Value::List(_)` not covered
```

Esto es la ventaja fundamental del enum sobre `void*` y `dyn Any`: el
compilador detecta los casos que olvidaste.

```rust
Value::List(items) => {
    write!(f, "[")?;
    for (i, item) in items.iter().enumerate() {
        if i > 0 { write!(f, ", ")?; }
        write!(f, "{item}")?;
    }
    write!(f, "]")
}
```
</details>

### Ejercicio 4 — Box<dyn Any>: downcast

Crea una lista con `Box<dyn Any>` que contenga un `i32`, un `f64`, un
`String`, y un `Vec<i32>`.  Recorre la lista e intenta downcast a cada
tipo.

<details>
<summary>Patrón de downcast</summary>

```rust
use std::any::Any;

fn describe(data: &dyn Any) {
    if let Some(n) = data.downcast_ref::<i32>() {
        println!("i32: {n}");
    } else if let Some(f) = data.downcast_ref::<f64>() {
        println!("f64: {f}");
    } else if let Some(s) = data.downcast_ref::<String>() {
        println!("String: {s}");
    } else if let Some(v) = data.downcast_ref::<Vec<i32>>() {
        println!("Vec<i32>: {v:?}");
    } else {
        println!("unknown: {:?}", data.type_id());
    }
}
```

Si alguien inserta un tipo no contemplado (ej. `bool`), cae en el `else` —
sin error de compilación.  Esto es menos seguro que el enum.
</details>

### Ejercicio 5 — Trait object propio

Define un trait `Describable` con métodos `describe(&self) -> String` y
`type_name(&self) -> &str`.  Impleméntalo para `i32`, `f64` y `String`.
Crea una lista de `Box<dyn Describable>`.

<details>
<summary>Ventaja</summary>

```rust
trait Describable {
    fn describe(&self) -> String;
    fn type_name(&self) -> &str;
}

impl Describable for i32 {
    fn describe(&self) -> String { format!("{self}") }
    fn type_name(&self) -> &str { "int" }
}
```

Con un trait propio, puedes llamar `node.data.describe()` sin downcast —
el dispatch dinámico (vtable) resuelve la llamada.  Es más limpio que
cascadas de `downcast_ref`.
</details>

### Ejercicio 6 — Error de tipo en C vs Rust

Escribe código C que acceda al miembro incorrecto de la union (ej. leer
`val.f` cuando el tipo es `VAL_INT`).  ¿Qué pasa?  Luego intenta lo
equivalente en Rust con un enum.  ¿Compila?

<details>
<summary>Comparación</summary>

C — compila y ejecuta (UB):
```c
Node *n = int_node(42);
printf("%f\n", n->val.f);   // lee los mismos bytes como float → basura
```

Rust — no compila:
```rust
let val = Value::Int(42);
// No hay forma de acceder al i32 como f64
// match es la ÚNICA forma de extraer el dato
```

En Rust, el dato de un enum solo es accesible vía `match` o `if let`, que
siempre verifican la variante.  No existe equivalente de leer el miembro
incorrecto de una union.
</details>

### Ejercicio 7 — Suma de numéricos

Escribe una función que recorra una lista no homogénea y sume solo los
valores numéricos (int y float), ignorando strings y bools.  Implementa
en C (tagged union) y en Rust (enum).

<details>
<summary>Rust</summary>

```rust
fn sum_numeric(list: &HeteroList) -> f64 {
    let mut sum = 0.0;
    let mut cur = &list.head;
    while let Some(node) = cur {
        match &node.data {
            Value::Int(n) => sum += *n as f64,
            Value::Float(f) => sum += f,
            _ => {}   // ignorar no-numéricos
        }
        cur = &node.next;
    }
    sum
}
```

El `_` catch-all es seguro aquí porque deliberadamente ignoramos las demás
variantes.  Si se añade una nueva variante numérica, el `_` la ignorará
silenciosamente — si esto es un riesgo, mejor listar todas las variantes
explícitamente.
</details>

### Ejercicio 8 — sizeof del enum

Predice el `size_of` de estos enums y verifica:

```rust
enum A { X(u8) }
enum B { X(u8), Y(u64) }
enum C { X(u8), Y(u64), Z(String) }
```

<details>
<summary>Tamaños</summary>

```
A: discriminante(0, optimizado) + u8 = 1 byte
   (con un solo variant, no necesita tag)

B: discriminante(8, alineación a u64) + max(1, 8) = 16 bytes

C: discriminante(8) + max(1, 8, 24) = 32 bytes
   String es (ptr + len + cap) = 24 bytes
```

El enum siempre paga el tamaño de la variante más grande.  Si una variante
tiene un `Vec<u8>` (24 bytes) y otra tiene un `bool` (1 byte), el bool
desperdicia 23 bytes.
</details>

### Ejercicio 9 — Filtrar por tipo

Implementa `filter_ints` que recorra la lista no homogénea y retorne un
`Vec<i32>` con solo los valores enteros.  Implementa en C y en Rust.

<details>
<summary>Rust</summary>

```rust
fn filter_ints(list: &HeteroList) -> Vec<i32> {
    let mut result = Vec::new();
    let mut cur = &list.head;
    while let Some(node) = cur {
        if let Value::Int(n) = &node.data {
            result.push(*n);
        }
        cur = &node.next;
    }
    result
}
```

En C:
```c
int *filter_ints(const Node *head, int *out_count) {
    // Primer paso: contar
    int count = 0;
    for (const Node *n = head; n; n = n->next)
        if (n->type == VAL_INT) count++;

    int *result = malloc(count * sizeof(int));
    int idx = 0;
    for (const Node *n = head; n; n = n->next)
        if (n->type == VAL_INT) result[idx++] = n->val.i;

    *out_count = count;
    return result;
}
```
</details>

### Ejercicio 10 — Cuándo usar cada estrategia

Para cada escenario, elige la mejor estrategia y justifica:
1. Intérprete de un lenguaje con 5 tipos primitivos.
2. Sistema de plugins donde cada plugin define sus propios tipos.
3. Parser JSON (número, string, bool, null, array, object).
4. Lista de configuración leída de un archivo.

<details>
<summary>Respuestas</summary>

1. **Intérprete con 5 tipos → enum** (Rust) / **tagged union** (C).  Los
   tipos son fijos y conocidos.  El match exhaustivo previene olvidar un caso.

2. **Sistema de plugins → `Box<dyn Trait>`** (Rust) / **`void *`** (C).
   Los tipos no se conocen en compilación — los plugins definen los suyos.
   Un trait propio es mejor que `Any` porque define la interfaz.

3. **Parser JSON → enum**.  JSON tiene exactamente 6 tipos, recursivos
   (array contiene values, object contiene values).  El enum Rust con
   `Value::Array(Vec<Value>)` y `Value::Object(HashMap<String, Value>)` es
   el diseño perfecto (así funciona `serde_json::Value`).

4. **Lista de config → enum** si los tipos son fijos (string, int, bool),
   **`Box<dyn Any>`** si las configs pueden ser de tipo arbitrario.
</details>
