# T03 — Recursión vs iteración

## Índice

1. [El teorema fundamental](#1-el-teorema-fundamental)
2. [Conversión mecánica: recursión a iteración con pila](#2-conversión-mecánica-recursión-a-iteración-con-pila)
3. [Caso simple: recursión lineal sin pila](#3-caso-simple-recursión-lineal-sin-pila)
4. [Caso complejo: recursión de árbol con pila](#4-caso-complejo-recursión-de-árbol-con-pila)
5. [Cuándo preferir recursión](#5-cuándo-preferir-recursión)
6. [Cuándo preferir iteración](#6-cuándo-preferir-iteración)
7. [Ejemplos comparados en C y Rust](#7-ejemplos-comparados-en-c-y-rust)
8. [Tabla de decisión](#8-tabla-de-decisión)
9. [Ejercicios](#9-ejercicios)

---

## 1. El teorema fundamental

> **Toda función recursiva puede convertirse en una función iterativa
> equivalente usando una pila explícita.**

Esto no es una heurística — es un resultado formal. La razón es directa:
la recursión **ya usa una pila** (la pila de llamadas del sistema). Si
reemplazamos esa pila implícita por una estructura de datos explícita
(`Vec`, array), eliminamos las llamadas recursivas manteniendo el mismo
comportamiento.

La conversión inversa también es verdadera: toda iteración puede
expresarse como recursión (reemplazando el bucle por una llamada
recursiva). Pero esto rara vez es útil en la práctica.

### Implicación práctica

Si un algoritmo recursivo funciona correctamente pero:

- Produce stack overflow por profundidad excesiva, o
- Es lento por el overhead de frames de función, o
- Necesita ejecutarse en un entorno con stack limitado (threads, embebidos)

...entonces siempre existe una versión iterativa equivalente.

---

## 2. Conversión mecánica: recursión a iteración con pila

El método general tiene tres pasos:

### Paso 1: Identificar qué se apila

En cada llamada recursiva, identificar qué datos necesita el frame:
parámetros, variables locales, y qué acción queda pendiente después del
retorno.

### Paso 2: Crear la pila explícita

Definir una estructura que contenga esos datos y usar un `Vec` o array
como pila.

### Paso 3: Reemplazar llamadas por push/pop

- Donde había una llamada recursiva, hacer **push** del estado.
- El bucle principal hace **pop** y procesa.
- Donde la función recursiva retornaba un valor para combinar, almacenar
  el resultado parcial.

### Plantilla general

```
FUNCIÓN recursiva_original(params):
    SI caso_base(params):
        RETORNAR valor_base
    // trabajo antes de la llamada recursiva
    resultado_parcial = recursiva_original(sub_params)
    // trabajo después de la llamada recursiva
    RETORNAR combinar(resultado_parcial, ...)
```

Se convierte en:

```
FUNCIÓN iterativa(params):
    stack ← pila vacía
    push(stack, params)

    MIENTRAS stack no vacía:
        current ← pop(stack)
        SI caso_base(current):
            procesar valor_base
        SI NO:
            // push en orden inverso (lo que debe procesarse primero va último)
            push(stack, sub_params)
            // almacenar trabajo pendiente si es necesario
```

La complejidad del método varía enormemente según el tipo de recursión.
Veamos los casos de menor a mayor dificultad.

---

## 3. Caso simple: recursión lineal sin pila

Cuando la recursión es **lineal** (una sola llamada recursiva por
invocación), muchas veces se puede convertir a un bucle simple **sin pila
explícita**.

### Factorial

```c
/* Recursiva */
unsigned long factorial_rec(int n) {
    if (n <= 1) return 1;
    return n * factorial_rec(n - 1);
}

/* Iterativa — sin pila */
unsigned long factorial_iter(int n) {
    unsigned long result = 1;
    for (int i = 2; i <= n; i++) {
        result *= i;
    }
    return result;
}
```

La variable `result` acumula lo que la recursión combinaba en la fase de
vuelta. No se necesita pila porque solo hay **un camino lineal**: no hay
ramificaciones.

### Suma de array

```rust
// Recursiva
fn sum_rec(arr: &[i32]) -> i32 {
    if arr.is_empty() { return 0; }
    arr[0] + sum_rec(&arr[1..])
}

// Iterativa — sin pila
fn sum_iter(arr: &[i32]) -> i32 {
    let mut total = 0;
    for &x in arr {
        total += x;
    }
    total
}
```

### GCD (Euclides)

```c
/* Recursiva */
int gcd_rec(int a, int b) {
    if (b == 0) return a;
    return gcd_rec(b, a % b);
}

/* Iterativa — sin pila */
int gcd_iter(int a, int b) {
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}
```

### Patrón

La recursión lineal se convierte en iteración cuando:

- Solo hay **una llamada recursiva**.
- El resultado se puede acumular en una variable.
- Los parámetros de la siguiente llamada se pueden calcular a partir de
  los actuales.

La transformación es: el cuerpo de la función se convierte en el cuerpo
del bucle, el caso base se convierte en la condición de terminación, y la
reducción de parámetros se convierte en la actualización de variables.

---

## 4. Caso complejo: recursión de árbol con pila

Cuando hay **múltiples llamadas recursivas** por invocación (recursión de
árbol), la conversión a iteración **requiere pila explícita**.

### Recorrido de árbol binario: inorder

```c
/* Recursiva */
void inorder_rec(Node *node) {
    if (node == NULL) return;
    inorder_rec(node->left);
    printf("%d ", node->data);
    inorder_rec(node->right);
}
```

Hay dos llamadas recursivas con **trabajo entre ellas** (el `printf`).
La conversión necesita rastrear "¿ya procesé el hijo izquierdo?":

```c
/* Iterativa con pila explícita */
void inorder_iter(Node *root) {
    Node **stack = malloc(1000 * sizeof(Node *));
    int top = 0;
    Node *current = root;

    while (current != NULL || top > 0) {
        /* descender por la izquierda */
        while (current != NULL) {
            stack[top++] = current;   /* push */
            current = current->left;
        }
        /* procesar el nodo */
        current = stack[--top];       /* pop */
        printf("%d ", current->data);
        /* ir al hijo derecho */
        current = current->right;
    }

    free(stack);
}
```

El algoritmo simula manualmente lo que la pila de llamadas hacía:
descender por la izquierda (fase de ida), procesar, ir a la derecha.

### Fibonacci con pila

Fibonacci tiene dos llamadas recursivas y combina resultados sumando:

```rust
// Recursiva
fn fib_rec(n: u32) -> u64 {
    if n <= 1 { return n as u64; }
    fib_rec(n - 1) + fib_rec(n - 2)
}

// Iterativa con pila explícita (simula los frames)
fn fib_stack(n: u32) -> u64 {
    // Each stack entry: value to compute
    // Result stack: accumulate partial results
    let mut call_stack: Vec<u32> = vec![n];
    let mut result_stack: Vec<u64> = Vec::new();

    while let Some(current) = call_stack.pop() {
        if current <= 1 {
            result_stack.push(current as u64);
        } else {
            // sentinel: "add the next two results"
            call_stack.push(u32::MAX); // marker for "combine"
            call_stack.push(current - 2);
            call_stack.push(current - 1);
        }

        // process combine markers
        while call_stack.last() == Some(&u32::MAX)
              && result_stack.len() >= 2
        {
            call_stack.pop(); // remove marker
            let b = result_stack.pop().unwrap();
            let a = result_stack.pop().unwrap();
            result_stack.push(a + b);
        }
    }

    result_stack.pop().unwrap_or(0)
}
```

Esto es intencionalmente engorroso — demuestra que la conversión mecánica
de recursión de árbol es compleja y poco intuitiva. Para Fibonacci, la
versión iterativa con dos variables (T01) es muy superior. La pila explícita
se justifica en problemas donde la iteración simple no es posible (árboles,
grafos, backtracking).

### Cuándo la pila explícita vale la pena

La pila explícita se justifica cuando:

1. La recursión es de árbol (múltiples llamadas) **y**
2. No hay una reformulación iterativa obvia **y**
3. La profundidad puede causar stack overflow.

Ejemplos: recorrido de árbol, DFS en grafo, exploración de directorios.

---

## 5. Cuándo preferir recursión

### Árboles y estructuras recursivas

Los árboles son inherentemente recursivos: un nodo tiene hijos que son
sub-árboles. Recorrerlos recursivamente es natural y claro:

```rust
fn tree_height(node: &Option<Box<TreeNode>>) -> usize {
    match node {
        None => 0,
        Some(n) => {
            1 + std::cmp::max(
                tree_height(&n.left),
                tree_height(&n.right),
            )
        }
    }
}
```

La versión iterativa requiere una pila y lógica adicional para rastrear
el máximo parcial. La ganancia es mínima porque los árboles balanceados
tienen profundidad $O(\log n)$, muy lejos del límite del stack.

### Divide y vencerás

Algoritmos como merge sort, quicksort, y búsqueda binaria se expresan
naturalmente con recursión:

```
mergesort(arr):
    if len(arr) <= 1: return arr
    mid = len(arr) / 2
    left = mergesort(arr[..mid])
    right = mergesort(arr[mid..])
    return merge(left, right)
```

La recursión expresa directamente la estructura del algoritmo: dividir,
resolver recursivamente, combinar. La versión iterativa oscurece esta
estructura.

### Backtracking

En problemas como N-reinas, laberintos, o sudoku, la recursión explora
un **espacio de soluciones** como un árbol de decisiones:

```
resolver(estado):
    if es_solución(estado): return estado
    for opción in opciones_válidas(estado):
        aplicar(opción)
        resultado = resolver(nuevo_estado)
        if resultado ≠ NULL: return resultado
        deshacer(opción)              ← backtrack
    return NULL                       ← sin solución en esta rama
```

El backtrack (deshacer la opción) es automático con recursión porque las
variables locales se restauran al retornar. Con iteración, hay que
deshacer explícitamente, lo que es más propenso a errores.

### Código más corto y claro

Cuando la profundidad está acotada (por ejemplo, $O(\log n)$ en divide y
vencerás), la recursión es segura y produce código más legible.

---

## 6. Cuándo preferir iteración

### Recursión lineal simple

Si la recursión es lineal y se traduce directamente a un bucle, la
iteración es siempre preferible:

```c
/* Innecesariamente recursivo */
int sum(int n) {
    if (n == 0) return 0;
    return n + sum(n - 1);  /* stack overflow para n grande */
}

/* Iterativo — más rápido, sin límite de profundidad */
int sum(int n) {
    int total = 0;
    for (int i = 1; i <= n; i++) total += i;
    return total;
}
```

### Riesgo de stack overflow

Si la profundidad de recursión puede ser proporcional al tamaño de la
entrada ($O(n)$), la iteración evita el riesgo:

| Profundidad | Stack 8 MB (~100K frames) | Riesgo |
|:-----------:|:------------------------:|:------:|
| $O(\log n)$ | Segura para $n < 2^{100000}$ | Ninguno |
| $O(\sqrt{n})$ | Segura para $n < 10^{10}$ | Bajo |
| $O(n)$ | Overflow para $n > 100,000$ | **Alto** |
| $O(n^2)$ | Overflow para $n > 300$ | **Extremo** |

### Rendimiento

Cada llamada a función tiene overhead: guardar registros, crear frame,
saltar a la función, retornar. Para funciones triviales llamadas millones
de veces, este overhead puede ser significativo:

```c
/* Recursivo: overhead de llamada en cada iteración */
int count_rec(int n) {
    if (n == 0) return 0;
    return 1 + count_rec(n - 1);
}

/* Iterativo: solo un salto condicional */
int count_iter(int n) {
    return n;  /* trivial, pero el punto es el overhead */
}
```

En benchmarks típicos, una función recursiva lineal es 2-10× más lenta
que su equivalente iterativo debido al overhead de frames.

### Entornos con stack limitado

En sistemas embebidos, threads con stack pequeño, o WebAssembly, el stack
puede ser de solo unos KB. En estos entornos, la iteración es obligatoria
para cualquier operación que procese entradas de tamaño variable.

---

## 7. Ejemplos comparados en C y Rust

### Ejemplo 1: Búsqueda lineal

```c
/* Recursiva — innecesaria */
int search_rec(const int *arr, int len, int target) {
    if (len == 0) return -1;
    if (arr[0] == target) return 0;
    int result = search_rec(arr + 1, len - 1, target);
    return (result == -1) ? -1 : result + 1;
}

/* Iterativa — directa */
int search_iter(const int *arr, int len, int target) {
    for (int i = 0; i < len; i++) {
        if (arr[i] == target) return i;
    }
    return -1;
}
```

Veredicto: **iteración**. La recursión no aporta claridad y añade
$O(n)$ de profundidad de stack.

### Ejemplo 2: Recorrido preorder de árbol binario

```rust
// Recursiva — natural
fn preorder_rec(node: &Option<Box<Node>>, result: &mut Vec<i32>) {
    if let Some(n) = node {
        result.push(n.data);
        preorder_rec(&n.left, result);
        preorder_rec(&n.right, result);
    }
}

// Iterativa con pila — más código pero sin riesgo de stack overflow
fn preorder_iter(root: &Option<Box<Node>>) -> Vec<i32> {
    let mut result = Vec::new();
    let mut stack: Vec<&Box<Node>> = Vec::new();

    if let Some(ref root_node) = root {
        stack.push(root_node);
    }

    while let Some(node) = stack.pop() {
        result.push(node.data);
        // push right first so left is processed first (LIFO)
        if let Some(ref right) = node.right {
            stack.push(right);
        }
        if let Some(ref left) = node.left {
            stack.push(left);
        }
    }

    result
}
```

Veredicto: **recursión para árboles balanceados** (profundidad
$O(\log n)$, seguro). **Iteración para árboles potencialmente
desbalanceados** (lista degenerada con profundidad $O(n)$).

### Ejemplo 3: Invertir lista enlazada

```c
/* Recursiva — elegante pero O(n) stack */
Node *reverse_rec(Node *head) {
    if (head == NULL || head->next == NULL) return head;
    Node *rest = reverse_rec(head->next);
    head->next->next = head;
    head->next = NULL;
    return rest;
}

/* Iterativa — O(1) espacio extra */
Node *reverse_iter(Node *head) {
    Node *prev = NULL;
    Node *curr = head;
    while (curr != NULL) {
        Node *next = curr->next;
        curr->next = prev;
        prev = curr;
        curr = next;
    }
    return prev;
}
```

Veredicto: **iteración**. La versión recursiva es elegante pero usa
$O(n)$ stack para una operación que es fundamentalmente lineal. Para una
lista de 1 millón de nodos, la recursiva produce stack overflow.

---

## 8. Tabla de decisión

| Situación | Preferir | Razón |
|-----------|:--------:|-------|
| Recursión lineal simple (factorial, suma, GCD) | Iteración | Bucle directo, $O(1)$ espacio |
| Árbol balanceado (recorrido, altura) | Recursión | Profundidad $O(\log n)$, código claro |
| Árbol potencialmente desbalanceado | Iteración + pila | Evitar stack overflow $O(n)$ |
| Divide y vencerás (merge sort, quicksort) | Recursión | Expresa la estructura natural |
| Backtracking (N-reinas, sudoku) | Recursión | Backtrack automático con variables locales |
| Lista enlazada (inversión, búsqueda) | Iteración | Estructura lineal, no necesita pila |
| Fibonacci, números combinatorios | Iteración / DP | Recursión ingenua es exponencial |
| Entrada de tamaño arbitrario, $O(n)$ profundidad | Iteración + pila | Stack del sistema es limitado |
| Sistema embebido / stack limitado | Iteración | Stack de KB, no MB |
| Código que será leído por otros | Depende | La claridad pesa más que la micro-optimización |

### Regla general

> Usar recursión cuando expresa la estructura del problema con claridad
> **y** la profundidad está acotada logarítmicamente. En los demás casos,
> preferir iteración.

---

## 9. Ejercicios

---

### Ejercicio 1 — Conversión de factorial

Convierte `factorial` de recursión a iteración de **dos formas**:

a) Con un bucle simple (sin pila).
b) Con una pila explícita que simule los stack frames.

¿Cuál es más eficiente?

<details>
<summary>¿Qué almacena la pila explícita en la versión (b)?</summary>

**a) Bucle simple:**

```c
unsigned long factorial_loop(int n) {
    unsigned long result = 1;
    for (int i = 2; i <= n; i++) {
        result *= i;
    }
    return result;
}
```

**b) Pila explícita:**

```c
unsigned long factorial_stack(int n) {
    /* Phase 1: push all values */
    int *stack = malloc(n * sizeof(int));
    int top = 0;
    for (int i = n; i >= 2; i--) {
        stack[top++] = i;
    }

    /* Phase 2: pop and multiply */
    unsigned long result = 1;
    while (top > 0) {
        result *= stack[--top];
    }

    free(stack);
    return result;
}
```

La pila almacena los valores de `n` pendientes de multiplicar — lo mismo
que los stack frames de la recursión almacenaban como parámetro `n`.

La versión (a) es más eficiente: $O(1)$ espacio vs $O(n)$ para (b). La
pila explícita es una conversión fiel de la recursión, pero para recursión
lineal, el bucle simple siempre gana. La pila explícita se justifica solo
cuando no es posible simplificar (recursión de árbol).

</details>

---

### Ejercicio 2 — DFS recursivo vs iterativo

Dado un grafo como lista de adyacencia, implementa DFS en Rust de ambas
formas. Compara el código y la profundidad máxima que cada versión
soporta.

```rust
type Graph = Vec<Vec<usize>>;
```

<details>
<summary>¿Cuál soporta grafos más grandes sin stack overflow?</summary>

```rust
// Recursiva
fn dfs_rec(graph: &Graph, node: usize, visited: &mut Vec<bool>,
           order: &mut Vec<usize>) {
    if visited[node] { return; }
    visited[node] = true;
    order.push(node);
    for &neighbor in &graph[node] {
        dfs_rec(graph, neighbor, visited, order);
    }
}

// Iterativa
fn dfs_iter(graph: &Graph, start: usize) -> Vec<usize> {
    let mut visited = vec![false; graph.len()];
    let mut order = Vec::new();
    let mut stack = vec![start];

    while let Some(node) = stack.pop() {
        if visited[node] { continue; }
        visited[node] = true;
        order.push(node);
        for &neighbor in graph[node].iter().rev() {
            if !visited[neighbor] {
                stack.push(neighbor);
            }
        }
    }
    order
}
```

La versión recursiva falla con stack overflow para grafos que formen
cadenas largas (por ejemplo, un grafo lineal `0→1→2→...→100000`). La
versión iterativa usa un `Vec` en el heap que puede crecer hasta la
memoria disponible (GBs en lugar de MB).

Para un grafo con 1 millón de nodos en cadena:
- Recursiva: stack overflow (~100K niveles máximo)
- Iterativa: funciona sin problema

</details>

---

### Ejercicio 3 — Clasificar recursiones

Para cada función, clasifica como recursión lineal (convertible a bucle
sin pila), recursión de cola, o recursión de árbol:

```c
int f1(int n) { return n <= 0 ? 0 : n + f1(n-1); }
int f2(int n) { return n <= 1 ? n : f2(n-1) + f2(n-2); }
int f3(int n, int acc) { return n <= 0 ? acc : f3(n-1, acc+n); }
void f4(Node *n) { if(n) { f4(n->left); print(n); f4(n->right); } }
int f5(int n) { return n <= 0 ? 1 : 2 * f5(n-1); }
```

<details>
<summary>¿Cuáles se beneficiarían de pila explícita y cuáles de bucle simple?</summary>

```
f1: Recursión LINEAL. Una sola llamada, resultado = n + f1(n-1).
    Convertible a bucle simple: for(i=1..n) sum += i.

f2: Recursión de ÁRBOL. Dos llamadas: f2(n-1) + f2(n-2). (Fibonacci)
    Convertible a bucle simple con dos variables (caso especial).
    Pila explícita posible pero innecesaria para este caso.

f3: Recursión de COLA. La llamada recursiva es la última operación
    (no hay trabajo pendiente después). Convertible a bucle directo:
    while(n > 0) { acc += n; n--; }

f4: Recursión de ÁRBOL. Dos llamadas con trabajo entre ellas (inorder).
    Requiere pila explícita para la conversión iterativa.

f5: Recursión LINEAL. Una sola llamada, resultado = 2 * f5(n-1).
    Convertible a bucle: result = 1; for(i=0..n) result *= 2.
    (O simplemente: 1 << n)
```

Beneficio de pila explícita: **solo f4** (árbol con trabajo intermedio).
Las demás se convierten a bucles simples. f2 tiene estructura de árbol
pero se simplifica a dos variables por la propiedad aditiva de Fibonacci.

</details>

---

### Ejercicio 4 — Inorder iterativo

Implementa el recorrido inorder iterativo con pila en C (sección 4).
Prueba con el siguiente árbol:

```
        4
       / \
      2   6
     / \ / \
    1  3 5  7
```

El inorder debe producir: `1 2 3 4 5 6 7`.

<details>
<summary>¿Por qué se necesita el bucle interno `while (current != NULL)`?</summary>

El bucle interno desciende por la izquierda hasta `NULL`, apilando cada
nodo en el camino:

```c
// Build the tree
Node *root = make_node(4);
root->left = make_node(2);
root->right = make_node(6);
root->left->left = make_node(1);
root->left->right = make_node(3);
root->right->left = make_node(5);
root->right->right = make_node(7);

// Inorder iterative traversal
void inorder_iter(Node *root) {
    Node *stack[100];
    int top = 0;
    Node *current = root;

    while (current != NULL || top > 0) {
        while (current != NULL) {
            stack[top++] = current;
            current = current->left;
        }
        current = stack[--top];
        printf("%d ", current->data);
        current = current->right;
    }
}
```

Traza:

```
current=4: push 4, go left → current=2
current=2: push 2, go left → current=1
current=1: push 1, go left → current=NULL
  pop 1, print 1, go right → current=NULL
  pop 2, print 2, go right → current=3
current=3: push 3, go left → current=NULL
  pop 3, print 3, go right → current=NULL
  pop 4, print 4, go right → current=6
current=6: push 6, go left → current=5
current=5: push 5, go left → current=NULL
  pop 5, print 5, go right → current=NULL
  pop 6, print 6, go right → current=7
current=7: push 7, go left → current=NULL
  pop 7, print 7, go right → current=NULL
```

Output: `1 2 3 4 5 6 7` ✓

El bucle interno simula la fase de ida de la recursión: descender por la
izquierda. Cada pop simula el retorno de `inorder(node->left)` y el
procesamiento del nodo. Asignar `current = current->right` simula la
segunda llamada `inorder(node->right)`.

</details>

---

### Ejercicio 5 — Medir overhead

Implementa `sum(1..n)` de tres formas en C: recursiva, iterativa con
pila explícita, iterativa con bucle. Mide el tiempo para $n = 10,000,000$.

<details>
<summary>¿Cuál es el orden de velocidad?</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

long sum_recursive(int n) {
    if (n <= 0) return 0;
    return n + sum_recursive(n - 1);
}

long sum_stack(int n) {
    int *stack = malloc(n * sizeof(int));
    int top = 0;
    for (int i = n; i >= 1; i--) stack[top++] = i;
    long result = 0;
    while (top > 0) result += stack[--top];
    free(stack);
    return result;
}

long sum_loop(int n) {
    long result = 0;
    for (int i = 1; i <= n; i++) result += i;
    return result;
}
```

Resultados típicos para $n = 10,000,000$ (compilado con `-O0`):

| Versión | Tiempo | Razón |
|---------|:------:|-------|
| Bucle | ~30 ms | Solo aritmética, sin overhead |
| Pila explícita | ~80 ms | malloc + accesos a memoria del Vec |
| Recursiva | Stack overflow | $10^7$ frames > 8 MB de stack |

Con `ulimit -s unlimited` para permitir la recursiva:

| Versión | Tiempo |
|---------|:------:|
| Bucle | ~30 ms |
| Pila explícita | ~80 ms |
| Recursiva | ~200 ms |

Orden: **bucle >> pila explícita >> recursiva**. La recursiva es ~7× más
lenta que el bucle debido al overhead de crear y destruir $10^7$ stack
frames (guardar/restaurar registros, saltar a la función, retornar).

Con `-O2`, el compilador puede convertir la recursiva a un bucle,
igualando los tiempos. Pero esto no se puede asumir para recursiones más
complejas.

</details>

---

### Ejercicio 6 — Backtracking recursivo vs iterativo

Las N-reinas se resuelven naturalmente con recursión. Escribe el
pseudocódigo iterativo equivalente. ¿Qué información necesita la pila
explícita?

<details>
<summary>¿Qué hay que apilar además de la columna elegida?</summary>

Pseudocódigo recursivo:

```
solve(row, board):
    if row == N: print(board); return
    for col in 0..N:
        if is_safe(row, col, board):
            board[row] = col
            solve(row + 1, board)
            board[row] = -1     // backtrack (automático al retornar)
```

Pseudocódigo iterativo con pila:

```
solve_iter(N):
    stack = pila vacía
    push(stack, {row=0, col=0})

    while stack not empty:
        state = peek(stack)

        if state.row == N:
            print(board)
            pop(stack)
            board[stack.peek().row] = -1  // undo
            stack.peek().col += 1         // try next column
            continue

        found = false
        for c in state.col..N:
            if is_safe(state.row, c, board):
                board[state.row] = c
                state.col = c + 1    // remember where to resume
                push(stack, {row=state.row+1, col=0})
                found = true
                break

        if not found:
            pop(stack)
            if stack not empty:
                board[stack.peek().row] = -1  // undo
                // resume from next column
```

La pila necesita almacenar:
1. **`row`**: qué fila se está procesando.
2. **`col`**: la **siguiente columna a probar** — esto es crucial para
   reanudar el backtracking después de un retorno.

El backtracking explícito (`board[row] = -1`) reemplaza lo que la
recursión hacía automáticamente al retornar (restaurar variables locales).
El código es más largo y más propenso a errores — por eso la recursión
es preferible para backtracking cuando la profundidad es razonable ($N$
reinas → profundidad $N$).

</details>

---

### Ejercicio 7 — Recursión mutua a iteración

Convierte estas funciones mutuamente recursivas a una versión iterativa:

```c
int is_even(int n) {
    if (n == 0) return 1;
    return is_odd(n - 1);
}

int is_odd(int n) {
    if (n == 0) return 0;
    return is_even(n - 1);
}
```

<details>
<summary>¿Se necesita pila para esta conversión?</summary>

No. La recursión mutua aquí es lineal (cada función llama exactamente una
vez a la otra). Se puede simplificar observando que:

- `is_even(n)` → `is_odd(n-1)` → `is_even(n-2)` → ...
- Es decir, `is_even(n) = is_even(n-2)` con caso base `is_even(0) = true`

La versión iterativa:

```c
int is_even_iter(int n) {
    if (n < 0) n = -n;  // handle negative
    return (n % 2) == 0;
}
```

O si se quiere simular la alternancia:

```c
int is_even_iter(int n) {
    int result = 1;  // start as "even"
    for (int i = n; i > 0; i--) {
        result = !result;  // toggle even/odd
    }
    return result;
}
```

La recursión mutua se colapsa a una alternancia simple. No hay
bifurcación, así que no se necesita pila. La lección: antes de aplicar
la conversión mecánica, buscar simplificaciones matemáticas.

</details>

---

### Ejercicio 8 — Profundidad segura

Escribe una función en Rust que calcule la profundidad máxima de
recursión segura dado el tamaño del stack y un estimado de bytes por
frame. Usa esta función para decidir dinámicamente si usar recursión o
iteración con pila.

<details>
<summary>¿Cómo se obtiene el tamaño del stack en Rust?</summary>

Rust no expone el tamaño del stack directamente. Las opciones son:

```rust
fn safe_depth(stack_bytes: usize, frame_bytes: usize) -> usize {
    // 80% safety margin (other functions use stack too)
    (stack_bytes * 80 / 100) / frame_bytes
}

fn solve(data: &[i32]) {
    let stack_size: usize = 8 * 1024 * 1024; // assume 8 MB default
    let frame_size: usize = 128;              // estimate per frame
    let max_depth = safe_depth(stack_size, frame_size);

    if data.len() <= max_depth {
        // safe to use recursion
        solve_recursive(data);
    } else {
        // use iterative version with explicit stack
        solve_iterative(data);
    }
}
```

Para threads donde se controla el stack:

```rust
fn solve_in_thread(data: Vec<i32>) {
    let stack_size = 64 * 1024 * 1024; // 64 MB
    std::thread::Builder::new()
        .stack_size(stack_size)
        .spawn(move || {
            let max_depth = safe_depth(stack_size, 128);
            // now max_depth is ~400K, safe for most inputs
            solve_recursive(&data);
        })
        .unwrap()
        .join()
        .unwrap();
}
```

En la práctica, la decisión suele ser estática: "si la profundidad es
$O(\log n)$, usar recursión; si es $O(n)$, usar iteración". La decisión
dinámica es para bibliotecas que no controlan su contexto de ejecución.

</details>

---

### Ejercicio 9 — Preorder, inorder, postorder iterativos

Implementa los tres recorridos de árbol binario de forma iterativa con
pila en Rust. Preorder e inorder están en las secciones anteriores.
Postorder es el más complejo: necesitas procesar el nodo **después** de
ambos hijos.

<details>
<summary>¿Por qué postorder iterativo es más difícil que preorder?</summary>

En preorder, se procesa el nodo **antes** de apilar los hijos — simple.
En postorder, se procesa **después** de ambos hijos. El problema es
saber si ambos hijos ya fueron visitados.

Solución con dos pilas:

```rust
fn postorder_iter(root: &Option<Box<Node>>) -> Vec<i32> {
    let mut result = Vec::new();
    let mut stack1: Vec<&Box<Node>> = Vec::new();
    let mut stack2: Vec<&Box<Node>> = Vec::new();

    if let Some(ref root_node) = root {
        stack1.push(root_node);
    }

    // Phase 1: build reverse postorder in stack2
    while let Some(node) = stack1.pop() {
        stack2.push(node);
        if let Some(ref left) = node.left {
            stack1.push(left);
        }
        if let Some(ref right) = node.right {
            stack1.push(right);
        }
    }

    // Phase 2: pop stack2 for correct postorder
    while let Some(node) = stack2.pop() {
        result.push(node.data);
    }

    result
}
```

La técnica: stack1 produce un orden raíz-derecho-izquierdo (similar a
preorder pero con hijos invertidos). Stack2 invierte ese orden,
produciendo izquierdo-derecho-raíz = postorder.

Es más difícil que preorder porque preorder procesa en la fase de ida
(push = procesar), mientras que postorder procesa en la fase de vuelta
(necesita saber que ambos hijos terminaron). La solución con dos pilas
simula la inversión de la fase de vuelta.

</details>
