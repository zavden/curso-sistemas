# Árboles de expresión

## Concepto

Un **árbol de expresión** es un árbol binario que representa una expresión
aritmética (o lógica). La estructura codifica tanto los operandos como los
operadores y, crucialmente, el **orden de evaluación** — eliminando toda
ambigüedad sin necesidad de paréntesis ni reglas de precedencia.

Reglas de construcción:

1. Las **hojas** son operandos (números o variables).
2. Los **nodos internos** son operadores (`+`, `-`, `*`, `/`, etc.).
3. Los hijos izquierdo y derecho de un operador son sus dos operandos.

Ejemplo: la expresión $(3 + 4) \times (5 - 2)$:

```
        *
       / \
      +   -
     / \ / \
    3  4 5  2
```

La estructura del árbol captura que la suma y la resta se evalúan **antes** de la
multiplicación, sin necesidad de paréntesis.

---

## Relación con las notaciones

Las tres notaciones clásicas para expresiones corresponden exactamente a los tres
recorridos DFS del árbol:

| Notación | Recorrido | Ejemplo |
|----------|-----------|---------|
| **Infija** | Inorden | `3 + 4 * 5 - 2` (con ambigüedad) |
| **Prefija** (polaca) | Preorden | `* + 3 4 - 5 2` |
| **Posfija** (polaca inversa) | Postorden | `3 4 + 5 2 - *` |

La notación **infija** es la que usamos habitualmente, pero es la única que
necesita paréntesis para desambiguar. Las notaciones prefija y posfija son
**libres de paréntesis** porque el orden de los operandos es inequívoco.

Esta correspondencia no es coincidencia: un árbol de expresión **es** la
estructura de datos que resuelve la ambigüedad de la notación infija. El
compilador parsea la expresión infija, construye el árbol, y luego puede
generar código en cualquier orden.

---

## Estructura del nodo

Un nodo de árbol de expresión puede ser un operador o un operando. Hay varias
formas de modelar esto:

### En C: unión etiquetada

```c
typedef enum { NODE_NUM, NODE_OP } NodeType;

typedef struct ExprNode {
    NodeType type;
    union {
        double value;    // si type == NODE_NUM
        char operator;   // si type == NODE_OP
    };
    struct ExprNode *left;
    struct ExprNode *right;
} ExprNode;

ExprNode *make_num(double value) {
    ExprNode *node = malloc(sizeof(ExprNode));
    node->type = NODE_NUM;
    node->value = value;
    node->left = node->right = NULL;
    return node;
}

ExprNode *make_op(char op, ExprNode *left, ExprNode *right) {
    ExprNode *node = malloc(sizeof(ExprNode));
    node->type = NODE_OP;
    node->operator = op;
    node->left = left;
    node->right = right;
    return node;
}
```

El `union` permite compartir memoria entre `value` y `operator`, ya que un nodo
nunca es ambos a la vez. El campo `type` indica cuál está activo.

### En Rust: enum

```rust
enum Expr {
    Num(f64),
    Op {
        op: char,
        left: Box<Expr>,
        right: Box<Expr>,
    },
}
```

El `enum` de Rust es una **unión etiquetada segura** — el compilador garantiza
que solo accedes al campo activo vía pattern matching. No necesitamos nodos con
punteros `left`/`right` separados porque la recursión está en las variantes del
enum.

---

## Evaluación recursiva

Evaluar un árbol de expresión es elegantemente simple:

1. Si el nodo es un número, devolver su valor.
2. Si el nodo es un operador, evaluar recursivamente los hijos izquierdo y
   derecho, y aplicar el operador.

```
eval(*) = eval(+) * eval(-)
        = (eval(3) + eval(4)) * (eval(5) - eval(2))
        = (3 + 4) * (5 - 2)
        = 7 * 3
        = 21
```

### En C

```c
double eval(ExprNode *node) {
    if (node->type == NODE_NUM) {
        return node->value;
    }

    double left = eval(node->left);
    double right = eval(node->right);

    switch (node->operator) {
        case '+': return left + right;
        case '-': return left - right;
        case '*': return left * right;
        case '/':
            if (right == 0.0) {
                fprintf(stderr, "Error: division by zero\n");
                exit(1);
            }
            return left / right;
        default:
            fprintf(stderr, "Error: unknown operator '%c'\n",
                    node->operator);
            exit(1);
    }
}
```

### En Rust

```rust
impl Expr {
    fn eval(&self) -> f64 {
        match self {
            Expr::Num(v) => *v,
            Expr::Op { op, left, right } => {
                let l = left.eval();
                let r = right.eval();
                match op {
                    '+' => l + r,
                    '-' => l - r,
                    '*' => l * r,
                    '/' => {
                        assert!(r != 0.0, "division by zero");
                        l / r
                    }
                    _ => panic!("unknown operator '{}'", op),
                }
            }
        }
    }
}
```

La recursión sigue exactamente el **recorrido postorden**: evaluar hijo izquierdo,
evaluar hijo derecho, aplicar operador. Esto no es coincidencia — la evaluación
de una expresión procesa primero los subresultados y luego combina, que es la
esencia del postorden.

**Complejidad**: $O(n)$ tiempo, $O(h)$ espacio en la pila de llamadas, donde $h$
es la altura del árbol.

---

## Imprimir con paréntesis (notación infija)

Un recorrido inorden directo produce la expresión sin paréntesis, lo que puede
ser ambiguo. Para reconstruir la notación infija correcta, se añaden paréntesis
alrededor de cada subexpresión:

### Versión completa (siempre paréntesis)

```c
void infix_full(ExprNode *node) {
    if (node->type == NODE_NUM) {
        printf("%.0f", node->value);
        return;
    }
    printf("(");
    infix_full(node->left);
    printf(" %c ", node->operator);
    infix_full(node->right);
    printf(")");
}
```

Para $(3 + 4) \times (5 - 2)$ produce: `((3 + 4) * (5 - 2))`.

Los paréntesis exteriores son redundantes, pero la estructura es correcta. Se
puede evitar el par más externo no imprimiendo paréntesis para la raíz.

### Versión mínima (solo paréntesis necesarios)

Los paréntesis solo son necesarios cuando un operador de menor precedencia es
hijo de uno de mayor precedencia:

```c
int precedence(char op) {
    switch (op) {
        case '+': case '-': return 1;
        case '*': case '/': return 2;
        default: return 0;
    }
}

void infix_minimal(ExprNode *node, int parent_prec, int is_right) {
    if (node->type == NODE_NUM) {
        printf("%.0f", node->value);
        return;
    }

    int prec = precedence(node->operator);
    // necesita parentesis si:
    // - menor precedencia que el padre, o
    // - misma precedencia y es hijo derecho (asociatividad izquierda)
    int need_parens = (prec < parent_prec) ||
                      (prec == parent_prec && is_right);

    if (need_parens) printf("(");
    infix_minimal(node->left, prec, 0);
    printf(" %c ", node->operator);
    infix_minimal(node->right, prec, 1);
    if (need_parens) printf(")");
}
```

Para $3 + 4 \times 5$: produce `3 + 4 * 5` (sin paréntesis, precedencia natural).
Para $(3 + 4) \times 5$: produce `(3 + 4) * 5` (paréntesis necesarios).

### Imprimir prefija y posfija

Estas notaciones se obtienen directamente de preorden y postorden:

```c
void prefix(ExprNode *node) {
    if (node->type == NODE_NUM) {
        printf("%.0f ", node->value);
        return;
    }
    printf("%c ", node->operator);
    prefix(node->left);
    prefix(node->right);
}

void postfix(ExprNode *node) {
    if (node->type == NODE_NUM) {
        printf("%.0f ", node->value);
        return;
    }
    postfix(node->left);
    postfix(node->right);
    printf("%c ", node->operator);
}
```

### En Rust

```rust
impl Expr {
    fn to_infix(&self) -> String {
        match self {
            Expr::Num(v) => format!("{v}"),
            Expr::Op { op, left, right } => {
                format!("({} {} {})", left.to_infix(), op, right.to_infix())
            }
        }
    }

    fn to_prefix(&self) -> String {
        match self {
            Expr::Num(v) => format!("{v}"),
            Expr::Op { op, left, right } => {
                format!("{} {} {}", op, left.to_prefix(), right.to_prefix())
            }
        }
    }

    fn to_postfix(&self) -> String {
        match self {
            Expr::Num(v) => format!("{v}"),
            Expr::Op { op, left, right } => {
                format!("{} {} {}", left.to_postfix(), right.to_postfix(), op)
            }
        }
    }
}
```

---

## Construir desde expresión posfija

La construcción desde posfija es la más sencilla porque se procesa linealmente
con una **pila**, sin necesidad de manejar precedencia ni paréntesis.

### Algoritmo

1. Recorrer los tokens de izquierda a derecha.
2. Si es un **número**: crear un nodo hoja y apilar.
3. Si es un **operador**: desapilar dos nodos (el primero es el hijo derecho,
   el segundo el izquierdo), crear un nodo operador con ellos, y apilar el
   resultado.
4. Al final, la pila contiene exactamente un nodo: la raíz.

### Traza

Posfija: `3 4 + 5 2 - *`

| Token | Acción | Pila |
|-------|--------|------|
| `3` | Push Num(3) | `[3]` |
| `4` | Push Num(4) | `[3, 4]` |
| `+` | Pop 4, 3 → Op(+, 3, 4) → push | `[+]` |
| `5` | Push Num(5) | `[+, 5]` |
| `2` | Push Num(2) | `[+, 5, 2]` |
| `-` | Pop 2, 5 → Op(-, 5, 2) → push | `[+, -]` |
| `*` | Pop -, + → Op(*, +, -) → push | `[*]` |

Resultado:

```
        *
       / \
      +   -
     / \ / \
    3  4 5  2
```

### En C

```c
ExprNode *build_from_postfix(const char *tokens[], int count) {
    ExprNode *stack[100];
    int top = -1;

    for (int i = 0; i < count; i++) {
        const char *token = tokens[i];

        if (strlen(token) == 1 && strchr("+-*/", token[0])) {
            // operador
            ExprNode *right = stack[top--];
            ExprNode *left  = stack[top--];
            stack[++top] = make_op(token[0], left, right);
        } else {
            // numero
            stack[++top] = make_num(atof(token));
        }
    }

    return stack[0];  // raiz
}
```

### En Rust

```rust
fn build_from_postfix(tokens: &[&str]) -> Expr {
    let mut stack: Vec<Expr> = Vec::new();

    for token in tokens {
        match *token {
            "+" | "-" | "*" | "/" => {
                let right = stack.pop().expect("missing operand");
                let left  = stack.pop().expect("missing operand");
                stack.push(Expr::Op {
                    op: token.chars().next().unwrap(),
                    left: Box::new(left),
                    right: Box::new(right),
                });
            }
            _ => {
                let val: f64 = token.parse().expect("invalid number");
                stack.push(Expr::Num(val));
            }
        }
    }

    stack.pop().expect("empty expression")
}
```

**Complejidad**: $O(n)$ tiempo y espacio, donde $n$ es el número de tokens.

---

## Construir desde expresión infija

Construir desde infija es más complejo porque debemos manejar **precedencia** y
**paréntesis**. El algoritmo clásico es el **Shunting-yard** de Dijkstra (1961),
que primero convierte infija a posfija y luego construye el árbol.

Sin embargo, podemos construir el árbol **directamente** desde infija usando una
variante del Shunting-yard con dos pilas: una de operadores y una de operandos
(nodos del árbol).

### Algoritmo (dos pilas)

Usamos:
- `operands`: pila de nodos del árbol.
- `operators`: pila de operadores pendientes.

Función auxiliar `apply_top`: desapilar un operador y dos operandos, crear el
nodo, y apilar el resultado en `operands`.

Recorrer tokens:

1. **Número**: crear nodo hoja, apilar en `operands`.
2. **`(`**: apilar en `operators`.
3. **`)`**: aplicar operadores hasta encontrar `(`, descartar el `(`.
4. **Operador**: mientras el tope de `operators` tenga precedencia $\geq$ que el
   operador actual (y no sea `(`), aplicar. Luego apilar el operador actual.
5. Al final: aplicar todos los operadores restantes.

### Traza

Infija: `( 3 + 4 ) * ( 5 - 2 )`

| Token | Acción | Operands | Operators |
|-------|--------|----------|-----------|
| `(` | Push op | — | `(` |
| `3` | Push node | `3` | `(` |
| `+` | Push op | `3` | `( +` |
| `4` | Push node | `3, 4` | `( +` |
| `)` | Apply + → `+(3,4)`, pop `(` | `+` | — |
| `*` | Push op | `+` | `*` |
| `(` | Push op | `+` | `* (` |
| `5` | Push node | `+, 5` | `* (` |
| `-` | Push op | `+, 5` | `* ( -` |
| `2` | Push node | `+, 5, 2` | `* ( -` |
| `)` | Apply - → `-(5,2)`, pop `(` | `+, -` | `*` |
| fin | Apply * → `*(+,-)` | `*` | — |

Resultado: el mismo árbol `*(+(3,4), -(5,2))`.

### En C

```c
int prec(char op) {
    if (op == '+' || op == '-') return 1;
    if (op == '*' || op == '/') return 2;
    return 0;
}

void apply_top(ExprNode **operands, int *otop, char *operators, int *ptop) {
    char op = operators[(*ptop)--];
    ExprNode *right = operands[(*otop)--];
    ExprNode *left  = operands[(*otop)--];
    operands[++(*otop)] = make_op(op, left, right);
}

ExprNode *build_from_infix(const char *tokens[], int count) {
    ExprNode *operands[100];
    char operators[100];
    int otop = -1, ptop = -1;

    for (int i = 0; i < count; i++) {
        const char *t = tokens[i];

        if (strcmp(t, "(") == 0) {
            operators[++ptop] = '(';
        } else if (strcmp(t, ")") == 0) {
            while (operators[ptop] != '(') {
                apply_top(operands, &otop, operators, &ptop);
            }
            ptop--;  // descartar '('
        } else if (strlen(t) == 1 && strchr("+-*/", t[0])) {
            while (ptop >= 0 && operators[ptop] != '(' &&
                   prec(operators[ptop]) >= prec(t[0])) {
                apply_top(operands, &otop, operators, &ptop);
            }
            operators[++ptop] = t[0];
        } else {
            operands[++otop] = make_num(atof(t));
        }
    }

    while (ptop >= 0) {
        apply_top(operands, &otop, operators, &ptop);
    }

    return operands[0];
}
```

### En Rust

```rust
fn build_from_infix(tokens: &[&str]) -> Expr {
    let mut operands: Vec<Expr> = Vec::new();
    let mut operators: Vec<char> = Vec::new();

    let prec = |op: char| -> u8 {
        match op {
            '+' | '-' => 1,
            '*' | '/' => 2,
            _ => 0,
        }
    };

    let apply = |operands: &mut Vec<Expr>, op: char| {
        let right = operands.pop().unwrap();
        let left  = operands.pop().unwrap();
        operands.push(Expr::Op {
            op,
            left: Box::new(left),
            right: Box::new(right),
        });
    };

    for token in tokens {
        match *token {
            "(" => operators.push('('),
            ")" => {
                while *operators.last().unwrap() != '(' {
                    let op = operators.pop().unwrap();
                    apply(&mut operands, op);
                }
                operators.pop(); // discard '('
            }
            "+" | "-" | "*" | "/" => {
                let op = token.chars().next().unwrap();
                while operators.last().map_or(false, |&top|
                    top != '(' && prec(top) >= prec(op))
                {
                    let top = operators.pop().unwrap();
                    apply(&mut operands, top);
                }
                operators.push(op);
            }
            _ => {
                let val: f64 = token.parse().unwrap();
                operands.push(Expr::Num(val));
            }
        }
    }

    while let Some(op) = operators.pop() {
        apply(&mut operands, op);
    }

    operands.pop().unwrap()
}
```

---

## Construir desde expresión prefija

Análogo a posfija pero recorriendo de **derecha a izquierda**, o usando recursión
directa:

### Algoritmo recursivo

1. Leer el siguiente token.
2. Si es número, devolver nodo hoja.
3. Si es operador, construir recursivamente el hijo izquierdo y luego el derecho.

```c
ExprNode *build_from_prefix(const char *tokens[], int *pos) {
    const char *t = tokens[*pos];
    (*pos)++;

    if (strlen(t) == 1 && strchr("+-*/", t[0])) {
        ExprNode *left  = build_from_prefix(tokens, pos);
        ExprNode *right = build_from_prefix(tokens, pos);
        return make_op(t[0], left, right);
    }
    return make_num(atof(t));
}
```

Traza con `* + 3 4 - 5 2`:

| Paso | Token | Acción |
|------|-------|--------|
| 1 | `*` | Operador → construir izq y der |
| 2 | `+` | Operador → construir izq y der |
| 3 | `3` | Número → hoja |
| 4 | `4` | Número → hoja → retornar `+(3,4)` |
| 5 | `-` | Operador → construir izq y der |
| 6 | `5` | Número → hoja |
| 7 | `2` | Número → hoja → retornar `-(5,2)` |
| 8 | — | Retornar `*(+(3,4), -(5,2))` |

```rust
fn build_from_prefix(tokens: &[&str], pos: &mut usize) -> Expr {
    let token = tokens[*pos];
    *pos += 1;

    match token {
        "+" | "-" | "*" | "/" => {
            let left  = build_from_prefix(tokens, pos);
            let right = build_from_prefix(tokens, pos);
            Expr::Op {
                op: token.chars().next().unwrap(),
                left: Box::new(left),
                right: Box::new(right),
            }
        }
        _ => Expr::Num(token.parse().unwrap()),
    }
}
```

---

## Resumen de las tres construcciones

| Desde | Algoritmo | Estructura auxiliar | Complejidad |
|-------|-----------|-------------------|-------------|
| Posfija | Lineal izq→der | 1 pila (nodos) | $O(n)$ |
| Prefija | Recursivo (o lineal der→izq) | Pila implícita (recursión) | $O(n)$ |
| Infija | Shunting-yard (dos pilas) | 2 pilas (nodos + operadores) | $O(n)$ |

La construcción desde **posfija** es la más limpia en implementación. Por eso,
muchos parsers primero convierten infija → posfija (Shunting-yard clásico) y
luego construyen el árbol desde la posfija.

---

## Transformaciones sobre el árbol

Una vez construido, el árbol permite operaciones más allá de la evaluación:

### Derivación simbólica

Se pueden implementar reglas de derivación como transformación del árbol. Por
ejemplo, la regla del producto $\frac{d}{dx}[f \cdot g] = f' \cdot g + f \cdot g'$:

```rust
fn derivative(expr: &Expr, var: char) -> Expr {
    match expr {
        Expr::Num(_) => Expr::Num(0.0),
        Expr::Op { op: '*', left, right } => {
            // f'*g + f*g'
            Expr::Op {
                op: '+',
                left: Box::new(Expr::Op {
                    op: '*',
                    left: Box::new(derivative(left, var)),
                    right: right.clone(),
                }),
                right: Box::new(Expr::Op {
                    op: '*',
                    left: left.clone(),
                    right: Box::new(derivative(right, var)),
                }),
            }
        }
        // ... otras reglas
        _ => todo!(),
    }
}
```

### Simplificación

Reglas como $x + 0 = x$, $x \times 1 = x$, $0 \times x = 0$:

```c
ExprNode *simplify(ExprNode *node) {
    if (node->type == NODE_NUM) return node;

    node->left  = simplify(node->left);
    node->right = simplify(node->right);

    // constante + constante -> constante
    if (node->left->type == NODE_NUM && node->right->type == NODE_NUM) {
        double result = eval(node);
        free(node->left);
        free(node->right);
        node->type = NODE_NUM;
        node->value = result;
        node->left = node->right = NULL;
        return node;
    }

    // x + 0 = x
    if (node->operator == '+' &&
        node->right->type == NODE_NUM && node->right->value == 0) {
        ExprNode *left = node->left;
        free(node->right);
        free(node);
        return left;
    }

    // x * 1 = x
    if (node->operator == '*' &&
        node->right->type == NODE_NUM && node->right->value == 1) {
        ExprNode *left = node->left;
        free(node->right);
        free(node);
        return left;
    }

    // x * 0 = 0
    if (node->operator == '*' &&
        node->right->type == NODE_NUM && node->right->value == 0) {
        expr_free(node->left);
        free(node->right);
        node->type = NODE_NUM;
        node->value = 0;
        node->left = node->right = NULL;
        return node;
    }

    return node;
}
```

---

## Operadores unarios y extensiones

Los árboles de expresión básicos modelan operadores **binarios**. Para operadores
unarios (como negación `-x` o funciones `sin(x)`, `sqrt(x)`), hay dos enfoques:

### Enfoque 1: hijo único

El operador unario tiene solo un hijo (el izquierdo por convención):

```c
ExprNode *make_unary(char op, ExprNode *operand) {
    ExprNode *node = malloc(sizeof(ExprNode));
    node->type = NODE_OP;
    node->operator = op;
    node->left = operand;
    node->right = NULL;  // sin hijo derecho
    return node;
}
```

### Enfoque 2: enum extendido en Rust

```rust
enum Expr {
    Num(f64),
    BinOp {
        op: char,
        left: Box<Expr>,
        right: Box<Expr>,
    },
    UnaryOp {
        op: String,   // "neg", "sin", "sqrt", etc.
        operand: Box<Expr>,
    },
}
```

Este enfoque es más limpio porque hace explícita la distinción entre operadores
unarios y binarios en el sistema de tipos.

---

## Programa completo en C

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { NODE_NUM, NODE_OP } NodeType;

typedef struct ExprNode {
    NodeType type;
    union {
        double value;
        char operator;
    };
    struct ExprNode *left;
    struct ExprNode *right;
} ExprNode;

ExprNode *make_num(double value) {
    ExprNode *n = malloc(sizeof(ExprNode));
    n->type = NODE_NUM;
    n->value = value;
    n->left = n->right = NULL;
    return n;
}

ExprNode *make_op(char op, ExprNode *left, ExprNode *right) {
    ExprNode *n = malloc(sizeof(ExprNode));
    n->type = NODE_OP;
    n->operator = op;
    n->left = left;
    n->right = right;
    return n;
}

void expr_free(ExprNode *node) {
    if (node == NULL) return;
    expr_free(node->left);
    expr_free(node->right);
    free(node);
}

// ======== Evaluacion ========

double eval(ExprNode *node) {
    if (node->type == NODE_NUM) return node->value;
    double l = eval(node->left);
    double r = eval(node->right);
    switch (node->operator) {
        case '+': return l + r;
        case '-': return l - r;
        case '*': return l * r;
        case '/': return l / r;
        default:  return 0;
    }
}

// ======== Impresion ========

void print_infix(ExprNode *node) {
    if (node->type == NODE_NUM) {
        printf("%.0f", node->value);
        return;
    }
    printf("(");
    print_infix(node->left);
    printf(" %c ", node->operator);
    print_infix(node->right);
    printf(")");
}

void print_prefix(ExprNode *node) {
    if (node->type == NODE_NUM) {
        printf("%.0f ", node->value);
        return;
    }
    printf("%c ", node->operator);
    print_prefix(node->left);
    print_prefix(node->right);
}

void print_postfix(ExprNode *node) {
    if (node->type == NODE_NUM) {
        printf("%.0f ", node->value);
        return;
    }
    print_postfix(node->left);
    print_postfix(node->right);
    printf("%c ", node->operator);
}

void print_tree(ExprNode *node, const char *prefix, int is_last) {
    if (node == NULL) return;
    printf("%s%s", prefix, is_last ? "└── " : "├── ");
    if (node->type == NODE_NUM) printf("%.0f\n", node->value);
    else printf("%c\n", node->operator);

    char new_prefix[256];
    snprintf(new_prefix, sizeof(new_prefix), "%s%s",
             prefix, is_last ? "    " : "│   ");
    print_tree(node->left, new_prefix, node->right == NULL);
    print_tree(node->right, new_prefix, 1);
}

// ======== Construccion desde posfija ========

ExprNode *build_postfix(const char *tokens[], int count) {
    ExprNode *stack[100];
    int top = -1;
    for (int i = 0; i < count; i++) {
        const char *t = tokens[i];
        if (strlen(t) == 1 && strchr("+-*/", t[0])) {
            ExprNode *r = stack[top--];
            ExprNode *l = stack[top--];
            stack[++top] = make_op(t[0], l, r);
        } else {
            stack[++top] = make_num(atof(t));
        }
    }
    return stack[0];
}

// ======== Construccion desde infija ========

int prec(char op) {
    if (op == '+' || op == '-') return 1;
    if (op == '*' || op == '/') return 2;
    return 0;
}

void apply_top(ExprNode **operands, int *otop, char *operators, int *ptop) {
    char op = operators[(*ptop)--];
    ExprNode *r = operands[(*otop)--];
    ExprNode *l = operands[(*otop)--];
    operands[++(*otop)] = make_op(op, l, r);
}

ExprNode *build_infix(const char *tokens[], int count) {
    ExprNode *operands[100];
    char operators[100];
    int otop = -1, ptop = -1;

    for (int i = 0; i < count; i++) {
        const char *t = tokens[i];
        if (strcmp(t, "(") == 0) {
            operators[++ptop] = '(';
        } else if (strcmp(t, ")") == 0) {
            while (operators[ptop] != '(')
                apply_top(operands, &otop, operators, &ptop);
            ptop--;
        } else if (strlen(t) == 1 && strchr("+-*/", t[0])) {
            while (ptop >= 0 && operators[ptop] != '(' &&
                   prec(operators[ptop]) >= prec(t[0]))
                apply_top(operands, &otop, operators, &ptop);
            operators[++ptop] = t[0];
        } else {
            operands[++otop] = make_num(atof(t));
        }
    }
    while (ptop >= 0)
        apply_top(operands, &otop, operators, &ptop);

    return operands[0];
}

// ======== Construccion desde prefija ========

ExprNode *build_prefix(const char *tokens[], int *pos) {
    const char *t = tokens[*pos];
    (*pos)++;
    if (strlen(t) == 1 && strchr("+-*/", t[0])) {
        ExprNode *l = build_prefix(tokens, pos);
        ExprNode *r = build_prefix(tokens, pos);
        return make_op(t[0], l, r);
    }
    return make_num(atof(t));
}

// ======== main ========

int main(void) {
    // 1. Construir (3 + 4) * (5 - 2) directamente
    printf("=== Construccion directa ===\n\n");
    ExprNode *tree1 = make_op('*',
        make_op('+', make_num(3), make_num(4)),
        make_op('-', make_num(5), make_num(2))
    );

    printf("Arbol:\n");
    print_tree(tree1, "", 1);

    printf("\nInfija:   "); print_infix(tree1);
    printf("\nPrefija:  "); print_prefix(tree1);
    printf("\nPosfija:  "); print_postfix(tree1);
    printf("\nResultado: %.0f\n", eval(tree1));
    expr_free(tree1);

    // 2. Construir desde posfija: 3 4 + 5 2 - *
    printf("\n=== Desde posfija: 3 4 + 5 2 - * ===\n\n");
    const char *postfix_tokens[] = {"3","4","+","5","2","-","*"};
    ExprNode *tree2 = build_postfix(postfix_tokens, 7);

    printf("Arbol:\n");
    print_tree(tree2, "", 1);
    printf("\nInfija:    "); print_infix(tree2);
    printf("\nResultado: %.0f\n", eval(tree2));
    expr_free(tree2);

    // 3. Construir desde infija: ( 3 + 4 ) * ( 5 - 2 )
    printf("\n=== Desde infija: ( 3 + 4 ) * ( 5 - 2 ) ===\n\n");
    const char *infix_tokens[] = {"(","3","+","4",")","*","(","5","-","2",")"};
    ExprNode *tree3 = build_infix(infix_tokens, 11);

    printf("Arbol:\n");
    print_tree(tree3, "", 1);
    printf("\nInfija:    "); print_infix(tree3);
    printf("\nResultado: %.0f\n", eval(tree3));
    expr_free(tree3);

    // 4. Construir desde prefija: * + 3 4 - 5 2
    printf("\n=== Desde prefija: * + 3 4 - 5 2 ===\n\n");
    const char *prefix_tokens[] = {"*","+","3","4","-","5","2"};
    int pos = 0;
    ExprNode *tree4 = build_prefix(prefix_tokens, &pos);

    printf("Arbol:\n");
    print_tree(tree4, "", 1);
    printf("\nInfija:    "); print_infix(tree4);
    printf("\nResultado: %.0f\n", eval(tree4));
    expr_free(tree4);

    // 5. Expresion mas compleja: 2 * 3 + 4 * 5
    printf("\n=== Desde infija: 2 * 3 + 4 * 5 ===\n\n");
    const char *complex[] = {"2","*","3","+","4","*","5"};
    ExprNode *tree5 = build_infix(complex, 7);

    printf("Arbol:\n");
    print_tree(tree5, "", 1);
    printf("\nInfija:    "); print_infix(tree5);
    printf("\nResultado: %.0f\n", eval(tree5));
    expr_free(tree5);

    return 0;
}
```

### Salida esperada

```
=== Construccion directa ===

Arbol:
└── *
    ├── +
    │   ├── 3
    │   └── 4
    └── -
        ├── 5
        └── 2

Infija:   ((3 + 4) * (5 - 2))
Prefija:  * + 3 4 - 5 2
Posfija:  3 4 + 5 2 - *
Resultado: 21

=== Desde posfija: 3 4 + 5 2 - * ===

Arbol:
└── *
    ├── +
    │   ├── 3
    │   └── 4
    └── -
        ├── 5
        └── 2

Infija:    ((3 + 4) * (5 - 2))
Resultado: 21

=== Desde infija: ( 3 + 4 ) * ( 5 - 2 ) ===

Arbol:
└── *
    ├── +
    │   ├── 3
    │   └── 4
    └── -
        ├── 5
        └── 2

Infija:    ((3 + 4) * (5 - 2))
Resultado: 21

=== Desde prefija: * + 3 4 - 5 2 ===

Arbol:
└── *
    ├── +
    │   ├── 3
    │   └── 4
    └── -
        ├── 5
        └── 2

Infija:    ((3 + 4) * (5 - 2))
Resultado: 21

=== Desde infija: 2 * 3 + 4 * 5 ===

Arbol:
└── +
    ├── *
    │   ├── 2
    │   └── 3
    └── *
        ├── 4
        └── 5

Infija:    ((2 * 3) + (4 * 5))
Resultado: 26
```

---

## Programa completo en Rust

```rust
#[derive(Clone)]
enum Expr {
    Num(f64),
    Op {
        op: char,
        left: Box<Expr>,
        right: Box<Expr>,
    },
}

impl Expr {
    fn eval(&self) -> f64 {
        match self {
            Expr::Num(v) => *v,
            Expr::Op { op, left, right } => {
                let l = left.eval();
                let r = right.eval();
                match op {
                    '+' => l + r,
                    '-' => l - r,
                    '*' => l * r,
                    '/' => l / r,
                    _ => panic!("unknown op"),
                }
            }
        }
    }

    fn to_infix(&self) -> String {
        match self {
            Expr::Num(v) => format!("{v}"),
            Expr::Op { op, left, right } =>
                format!("({} {op} {})", left.to_infix(), right.to_infix()),
        }
    }

    fn to_prefix(&self) -> String {
        match self {
            Expr::Num(v) => format!("{v}"),
            Expr::Op { op, left, right } =>
                format!("{op} {} {}", left.to_prefix(), right.to_prefix()),
        }
    }

    fn to_postfix(&self) -> String {
        match self {
            Expr::Num(v) => format!("{v}"),
            Expr::Op { op, left, right } =>
                format!("{} {} {op}", left.to_postfix(), right.to_postfix()),
        }
    }

    fn print_tree(&self, prefix: &str, is_last: bool) {
        print!("{}{}", prefix, if is_last { "└── " } else { "├── " });
        match self {
            Expr::Num(v) => println!("{v}"),
            Expr::Op { op, left, right } => {
                println!("{op}");
                let new_prefix = format!("{}{}", prefix,
                    if is_last { "    " } else { "│   " });
                left.print_tree(&new_prefix, false);
                right.print_tree(&new_prefix, true);
            }
        }
    }
}

fn build_from_postfix(tokens: &[&str]) -> Expr {
    let mut stack: Vec<Expr> = Vec::new();
    for token in tokens {
        match *token {
            "+" | "-" | "*" | "/" => {
                let right = stack.pop().unwrap();
                let left  = stack.pop().unwrap();
                stack.push(Expr::Op {
                    op: token.chars().next().unwrap(),
                    left: Box::new(left),
                    right: Box::new(right),
                });
            }
            _ => stack.push(Expr::Num(token.parse().unwrap())),
        }
    }
    stack.pop().unwrap()
}

fn build_from_prefix(tokens: &[&str], pos: &mut usize) -> Expr {
    let token = tokens[*pos];
    *pos += 1;
    match token {
        "+" | "-" | "*" | "/" => {
            let left  = build_from_prefix(tokens, pos);
            let right = build_from_prefix(tokens, pos);
            Expr::Op {
                op: token.chars().next().unwrap(),
                left: Box::new(left),
                right: Box::new(right),
            }
        }
        _ => Expr::Num(token.parse().unwrap()),
    }
}

fn build_from_infix(tokens: &[&str]) -> Expr {
    let mut operands: Vec<Expr> = Vec::new();
    let mut operators: Vec<char> = Vec::new();

    let prec = |op: char| -> u8 {
        match op {
            '+' | '-' => 1,
            '*' | '/' => 2,
            _ => 0,
        }
    };

    let apply = |operands: &mut Vec<Expr>, op: char| {
        let right = operands.pop().unwrap();
        let left  = operands.pop().unwrap();
        operands.push(Expr::Op {
            op,
            left: Box::new(left),
            right: Box::new(right),
        });
    };

    for token in tokens {
        match *token {
            "(" => operators.push('('),
            ")" => {
                while *operators.last().unwrap() != '(' {
                    let op = operators.pop().unwrap();
                    apply(&mut operands, op);
                }
                operators.pop();
            }
            "+" | "-" | "*" | "/" => {
                let op = token.chars().next().unwrap();
                while operators.last().map_or(false, |&top|
                    top != '(' && prec(top) >= prec(op))
                {
                    let top = operators.pop().unwrap();
                    apply(&mut operands, top);
                }
                operators.push(op);
            }
            _ => operands.push(Expr::Num(token.parse().unwrap())),
        }
    }

    while let Some(op) = operators.pop() {
        apply(&mut operands, op);
    }

    operands.pop().unwrap()
}

fn main() {
    // 1. Construccion directa
    let tree = Expr::Op {
        op: '*',
        left: Box::new(Expr::Op {
            op: '+',
            left: Box::new(Expr::Num(3.0)),
            right: Box::new(Expr::Num(4.0)),
        }),
        right: Box::new(Expr::Op {
            op: '-',
            left: Box::new(Expr::Num(5.0)),
            right: Box::new(Expr::Num(2.0)),
        }),
    };

    println!("=== Construccion directa ===\n");
    tree.print_tree("", true);
    println!("\nInfija:    {}", tree.to_infix());
    println!("Prefija:   {}", tree.to_prefix());
    println!("Posfija:   {}", tree.to_postfix());
    println!("Resultado: {}\n", tree.eval());

    // 2. Desde posfija
    println!("=== Desde posfija: 3 4 + 5 2 - * ===\n");
    let tree2 = build_from_postfix(&["3","4","+","5","2","-","*"]);
    tree2.print_tree("", true);
    println!("\nInfija:    {}", tree2.to_infix());
    println!("Resultado: {}\n", tree2.eval());

    // 3. Desde infija
    println!("=== Desde infija: ( 3 + 4 ) * ( 5 - 2 ) ===\n");
    let tree3 = build_from_infix(
        &["(","3","+","4",")","*","(","5","-","2",")"]);
    tree3.print_tree("", true);
    println!("\nInfija:    {}", tree3.to_infix());
    println!("Resultado: {}\n", tree3.eval());

    // 4. Desde prefija
    println!("=== Desde prefija: * + 3 4 - 5 2 ===\n");
    let mut pos = 0;
    let tree4 = build_from_prefix(
        &["*","+","3","4","-","5","2"], &mut pos);
    tree4.print_tree("", true);
    println!("\nInfija:    {}", tree4.to_infix());
    println!("Resultado: {}\n", tree4.eval());

    // 5. Sin parentesis: 2 * 3 + 4 * 5
    println!("=== Desde infija: 2 * 3 + 4 * 5 ===\n");
    let tree5 = build_from_infix(&["2","*","3","+","4","*","5"]);
    tree5.print_tree("", true);
    println!("\nInfija:    {}", tree5.to_infix());
    println!("Resultado: {}", tree5.eval());
}
```

### Salida

```
=== Construccion directa ===

└── *
    ├── +
    │   ├── 3
    │   └── 4
    └── -
        ├── 5
        └── 2

Infija:    (3 + 4) * (5 - 2)
Prefija:   * + 3 4 - 5 2
Posfija:   3 4 + 5 2 - *
Resultado: 21

=== Desde posfija: 3 4 + 5 2 - * ===

└── *
    ├── +
    │   ├── 3
    │   └── 4
    └── -
        ├── 5
        └── 2

Infija:    (3 + 4) * (5 - 2)
Resultado: 21

=== Desde infija: ( 3 + 4 ) * ( 5 - 2 ) ===

└── *
    ├── +
    │   ├── 3
    │   └── 4
    └── -
        ├── 5
        └── 2

Infija:    (3 + 4) * (5 - 2)
Resultado: 21

=== Desde prefija: * + 3 4 - 5 2 ===

└── *
    ├── +
    │   ├── 3
    │   └── 4
    └── -
        ├── 5
        └── 2

Infija:    (3 + 4) * (5 - 2)
Resultado: 21

=== Desde infija: 2 * 3 + 4 * 5 ===

└── +
    ├── *
    │   ├── 2
    │   └── 3
    └── *
        ├── 4
        └── 5

Infija:    (2 * 3) + (4 * 5)
Resultado: 26
```

---

## Ejercicios

### Ejercicio 1 — Trazar evaluación

Dado el árbol de la expresión $8 / (2 + 2) - 1$:

```
        -
       / \
      /   1
     / \
    8   +
       / \
      2   2
```

Traza la evaluación recursiva paso a paso: ¿qué llamadas a `eval` se hacen y
en qué orden? ¿Cuál es el resultado?

<details>
<summary>Verificar</summary>

Evaluación (postorden):

```
eval(-) requiere eval(/) y eval(1)
  eval(/) requiere eval(8) y eval(+)
    eval(8) = 8
    eval(+) requiere eval(2) y eval(2)
      eval(2) = 2
      eval(2) = 2
    eval(+) = 2 + 2 = 4
  eval(/) = 8 / 4 = 2
  eval(1) = 1
eval(-) = 2 - 1 = 1
```

Resultado: **1**.

Orden de llamadas: 8, 2, 2, +, /, 1, - (postorden).
</details>

### Ejercicio 2 — Construir desde posfija

Construye paso a paso (con tabla de pila) el árbol de expresión para la posfija:
`5 1 2 + 4 * + 3 -`

Dibuja el árbol resultante y verifica evaluando.

<details>
<summary>Verificar</summary>

| Token | Pila (topes a la derecha) |
|-------|--------------------------|
| `5` | `[5]` |
| `1` | `[5, 1]` |
| `2` | `[5, 1, 2]` |
| `+` | `[5, +(1,2)]` |
| `4` | `[5, +(1,2), 4]` |
| `*` | `[5, *(+(1,2), 4)]` |
| `+` | `[+(5, *(+(1,2), 4))]` |
| `3` | `[+(5, *(+(1,2), 4)), 3]` |
| `-` | `[-(+(5, *(+(1,2), 4)), 3)]` |

Árbol:

```
            -
           / \
          +   3
         / \
        5   *
           / \
          +   4
         / \
        1   2
```

Evaluación: $(1+2) = 3$, $3 \times 4 = 12$, $5 + 12 = 17$, $17 - 3 = 14$.

Infija: `((5 + ((1 + 2) * 4)) - 3)` = **14**.
</details>

### Ejercicio 3 — Construir desde infija sin paréntesis

Construye el árbol para la expresión infija `3 + 4 * 2 - 1` (sin paréntesis,
respetando precedencia). Traza el algoritmo de dos pilas paso a paso.

<details>
<summary>Verificar</summary>

| Token | Operands | Operators | Acción |
|-------|----------|-----------|--------|
| `3` | `[3]` | `[]` | Push num |
| `+` | `[3]` | `[+]` | Push op |
| `4` | `[3, 4]` | `[+]` | Push num |
| `*` | `[3, 4]` | `[+ *]` | `*` > `+`, push op |
| `2` | `[3, 4, 2]` | `[+ *]` | Push num |
| `-` | `[3, 4, 2]` | `[+ *]` | `-` ≤ `*`, apply `*` → `[3, *(4,2)]`, `[+]` |
| | | | `-` ≤ `+`, apply `+` → `[+(3,*(4,2))]`, `[]` |
| | | | push `-` → `[+(3,*(4,2))]`, `[-]` |
| `1` | `[+(3,*(4,2)), 1]` | `[-]` | Push num |
| fin | | | Apply `-` → `[-(+(3,*(4,2)), 1)]` |

Árbol:

```
        -
       / \
      +   1
     / \
    3   *
       / \
      4   2
```

Evaluación: $4 \times 2 = 8$, $3 + 8 = 11$, $11 - 1 = 10$.

Infija: `((3 + (4 * 2)) - 1)` = **10**.
</details>

### Ejercicio 4 — Las tres notaciones

Dado el árbol:

```
        +
       / \
      *   -
     / \ / \
    2  3 8  /
            / \
           6   2
```

Escribe las tres notaciones (infija con paréntesis, prefija, posfija) y evalúa.

<details>
<summary>Verificar</summary>

**Infija**: `((2 * 3) + (8 - (6 / 2)))`

**Prefija**: `+ * 2 3 - 8 / 6 2`

**Posfija**: `2 3 * 8 6 2 / - +`

**Evaluación**:
- $2 \times 3 = 6$
- $6 / 2 = 3$
- $8 - 3 = 5$
- $6 + 5 = 11$

Resultado: **11**.
</details>

### Ejercicio 5 — Paréntesis mínimos

Implementa la función `infix_minimal` que solo añada paréntesis cuando sean
necesarios por precedencia. Verifica con estos casos:

- `3 + 4 * 5` → sin paréntesis
- `(3 + 4) * 5` → un par
- `3 * 4 + 5 * 6` → sin paréntesis

<details>
<summary>Verificar</summary>

```c
int precedence(char op) {
    if (op == '+' || op == '-') return 1;
    if (op == '*' || op == '/') return 2;
    return 0;
}

void infix_minimal(ExprNode *node, int parent_prec, int is_right) {
    if (node->type == NODE_NUM) {
        printf("%.0f", node->value);
        return;
    }

    int prec = precedence(node->operator);
    int need = (prec < parent_prec) ||
               (prec == parent_prec && is_right);

    if (need) printf("(");
    infix_minimal(node->left, prec, 0);
    printf(" %c ", node->operator);
    infix_minimal(node->right, prec, 1);
    if (need) printf(")");
}
// llamar: infix_minimal(root, 0, 0);
```

Verificación:

- `3 + 4 * 5`: `*` tiene prec 2 > 1 de `+` (padre), no necesita paréntesis → `3 + 4 * 5` ✓
- `(3 + 4) * 5`: `+` tiene prec 1 < 2 de `*` (padre), necesita → `(3 + 4) * 5` ✓
- `3 * 4 + 5 * 6`: ambos `*` tienen prec 2 > 1 de `+`, no necesitan → `3 * 4 + 5 * 6` ✓

Caso sutil: `8 - 3 - 2`. El `- 2` es hijo derecho de `- 3`. Misma precedencia + hijo
derecho → necesita paréntesis: `8 - 3 - 2` (pero por asociatividad izquierda, el
árbol sería `(8-3)-2`, y con `is_right` el 2 no obtiene paréntesis extra porque la
resta es naturalmente izquierda). El flag `is_right` maneja correctamente
`8 - (3 - 2)` vs `(8 - 3) - 2`.
</details>

### Ejercicio 6 — Variables

Extiende el árbol de expresión para soportar variables. Añade una variante
`Var(String)` (o `NODE_VAR` en C). Implementa `eval` que reciba un mapa de
valores y una función `substitute` que reemplace variables por valores.

<details>
<summary>Verificar</summary>

En Rust:

```rust
use std::collections::HashMap;

enum Expr {
    Num(f64),
    Var(String),
    Op { op: char, left: Box<Expr>, right: Box<Expr> },
}

impl Expr {
    fn eval(&self, vars: &HashMap<String, f64>) -> f64 {
        match self {
            Expr::Num(v) => *v,
            Expr::Var(name) => *vars.get(name)
                .unwrap_or_else(|| panic!("undefined variable: {}", name)),
            Expr::Op { op, left, right } => {
                let l = left.eval(vars);
                let r = right.eval(vars);
                match op {
                    '+' => l + r, '-' => l - r,
                    '*' => l * r, '/' => l / r,
                    _ => panic!("unknown op"),
                }
            }
        }
    }

    fn substitute(&self, vars: &HashMap<String, f64>) -> Expr {
        match self {
            Expr::Num(v) => Expr::Num(*v),
            Expr::Var(name) => match vars.get(name) {
                Some(v) => Expr::Num(*v),
                None => Expr::Var(name.clone()),
            },
            Expr::Op { op, left, right } => Expr::Op {
                op: *op,
                left: Box::new(left.substitute(vars)),
                right: Box::new(right.substitute(vars)),
            },
        }
    }
}
```

Ejemplo: `x + 2 * y` con `{x: 3, y: 5}` → `3 + 2 * 5` → **13**.
</details>

### Ejercicio 7 — Contar operaciones

Implementa `count_ops(ExprNode *root)` que cuente el número de operaciones (nodos
internos) en el árbol. ¿Cuántas operaciones tiene $(a + b) \times (c - d) / (e + f)$?

<details>
<summary>Verificar</summary>

```c
int count_ops(ExprNode *node) {
    if (node == NULL || node->type == NODE_NUM) return 0;
    return 1 + count_ops(node->left) + count_ops(node->right);
}
```

Árbol de $(a + b) \times (c - d) / (e + f)$:

```
        /
       / \
      *   +
     / \ / \
    +  - e  f
   /\ /\
  a b c d
```

Operadores: `/`, `*`, `+` (izq), `-`, `+` (der) = **5 operaciones**.

Operandos (hojas): a, b, c, d, e, f = **6**.

Propiedad general: en un árbol de expresión binario, el número de hojas =
número de nodos internos + 1, es decir, $\text{operandos} = \text{operadores} + 1$.
</details>

### Ejercicio 8 — Igualdad estructural

Implementa una función que determine si dos árboles de expresión son
**estructuralmente iguales** (misma forma, mismos valores). ¿Son iguales
`(3 + 4) * 5` y `(4 + 3) * 5`?

<details>
<summary>Verificar</summary>

```c
int expr_equal(ExprNode *a, ExprNode *b) {
    if (a == NULL && b == NULL) return 1;
    if (a == NULL || b == NULL) return 0;
    if (a->type != b->type) return 0;

    if (a->type == NODE_NUM)
        return a->value == b->value;

    return a->operator == b->operator &&
           expr_equal(a->left, b->left) &&
           expr_equal(a->right, b->right);
}
```

`(3 + 4) * 5` vs `(4 + 3) * 5`: **NO** son estructuralmente iguales.
Aunque $3 + 4 = 4 + 3$ (conmutatividad), los árboles tienen estructura
diferente (3 y 4 están intercambiados).

Para igualdad semántica (considerando conmutatividad), habría que verificar
también con los hijos intercambiados para operadores conmutativos:

```c
if ((a->operator == '+' || a->operator == '*') &&
    expr_equal(a->left, b->right) && expr_equal(a->right, b->left))
    return 1;
```
</details>

### Ejercicio 9 — Profundidad de evaluación

La **profundidad de evaluación** determina cuántas operaciones se pueden
paralelizar. Si evaluamos ambos hijos de un operador en paralelo, el tiempo
es la profundidad del árbol (no el número de nodos). Calcula la profundidad
mínima y máxima para evaluar `1 + 2 + 3 + 4 + 5 + 6 + 7 + 8` construyendo
el árbol de dos formas distintas.

<details>
<summary>Verificar</summary>

**Forma 1**: asociatividad izquierda (como parsea el Shunting-yard):

```
              +
             / \
            +   8
           / \
          +   7
         / \
        +   6
       ...
```

Profundidad = **7** (cadena lineal). Evaluación secuencial.

**Forma 2**: árbol balanceado (tipo divide-y-vencerás):

```
            +
          /   \
        +       +
       / \     / \
      +   +   +   +
     /\ /\  /\  /\
    1 2 3 4 5 6 7 8
```

Profundidad = **3** = $\log_2 8$. Con paralelismo, las 4 sumas del nivel inferior
se ejecutan simultáneamente, luego las 2 del medio, luego la final. Total: 3 pasos.

$\Rightarrow$ El mismo cálculo tarda 7 pasos secuenciales o 3 en paralelo. Esta
es la ventaja de los **árboles de expresión balanceados** en compiladores
optimizantes y procesamiento de señales (reducción paralela).
</details>

### Ejercicio 10 — Construir desde string

Implementa una función que tome una expresión como string (e.g. `"(3+4)*5"`)
y construya el árbol, manejando tokenización de números multi-dígito. Es decir,
un mini-parser completo.

<details>
<summary>Verificar</summary>

Pasos: 1) tokenizar el string, 2) aplicar Shunting-yard.

```rust
fn tokenize(input: &str) -> Vec<String> {
    let mut tokens = Vec::new();
    let mut chars = input.chars().peekable();

    while let Some(&ch) = chars.peek() {
        match ch {
            ' ' => { chars.next(); }
            '(' | ')' | '+' | '-' | '*' | '/' => {
                tokens.push(ch.to_string());
                chars.next();
            }
            '0'..='9' | '.' => {
                let mut num = String::new();
                while let Some(&c) = chars.peek() {
                    if c.is_ascii_digit() || c == '.' {
                        num.push(c);
                        chars.next();
                    } else {
                        break;
                    }
                }
                tokens.push(num);
            }
            _ => panic!("unexpected char: {ch}"),
        }
    }
    tokens
}

fn parse(input: &str) -> Expr {
    let tokens = tokenize(input);
    let refs: Vec<&str> = tokens.iter().map(|s| s.as_str()).collect();
    build_from_infix(&refs)
}

// Uso:
let tree = parse("(3+4)*5");
assert_eq!(tree.eval(), 35.0);

let tree2 = parse("100 / (25 - 5)");
assert_eq!(tree2.eval(), 5.0);
```

En C, la tokenización es similar pero con `char *` y manejo manual de buffer.
La clave es acumular dígitos consecutivos en un buffer antes de llamar a `atof`.
</details>
