# Recorrido e impresión

## El patrón fundamental

Toda operación sobre una lista enlazada que necesite ver todos los elementos
sigue el mismo patrón:

```c
Node *current = head;
while (current != NULL) {
    /* hacer algo con current->data */
    current = current->next;
}
```

Este patrón es el equivalente del `for (int i = 0; i < n; i++)` de los arrays.
La diferencia: en un array avanzamos un índice entero; en una lista avanzamos
un puntero.

| Array | Lista enlazada |
|-------|----------------|
| `for (int i = 0; i < n; i++)` | `while (current != NULL)` |
| `arr[i]` | `current->data` |
| `i++` | `current = current->next` |
| Fin: `i == n` | Fin: `current == NULL` |

---

## Recorrido con for

El mismo recorrido puede escribirse como `for`:

```c
for (Node *cur = head; cur != NULL; cur = cur->next) {
    printf("%d\n", cur->data);
}
```

Es más compacto y deja claro la inicialización, condición y avance en una sola
línea.  Ambas formas son equivalentes — elegir según preferencia.

---

## Operaciones basadas en recorrido

### Contar elementos — $O(n)$

```c
int list_length(const Node *head) {
    int count = 0;
    for (const Node *cur = head; cur != NULL; cur = cur->next) {
        count++;
    }
    return count;
}
```

### Sumar todos los elementos — $O(n)$

```c
int list_sum(const Node *head) {
    int sum = 0;
    for (const Node *cur = head; cur != NULL; cur = cur->next) {
        sum += cur->data;
    }
    return sum;
}
```

### Encontrar el máximo — $O(n)$

```c
#include <limits.h>

bool list_max(const Node *head, int *out) {
    if (head == NULL) return false;

    int max = head->data;
    for (const Node *cur = head->next; cur != NULL; cur = cur->next) {
        if (cur->data > max) max = cur->data;
    }
    *out = max;
    return true;
}
```

### Verificar si contiene un valor — $O(n)$

```c
bool list_contains(const Node *head, int value) {
    for (const Node *cur = head; cur != NULL; cur = cur->next) {
        if (cur->data == value) return true;
    }
    return false;
}
```

### Contar ocurrencias — $O(n)$

```c
int list_count(const Node *head, int value) {
    int count = 0;
    for (const Node *cur = head; cur != NULL; cur = cur->next) {
        if (cur->data == value) count++;
    }
    return count;
}
```

---

## Impresión

### Formato básico

```c
void list_print(const Node *head) {
    printf("[");
    for (const Node *cur = head; cur != NULL; cur = cur->next) {
        printf("%d", cur->data);
        if (cur->next != NULL) printf(", ");
    }
    printf("]\n");
}
```

### Con flechas (debug)

```c
void list_print_arrows(const Node *head) {
    for (const Node *cur = head; cur != NULL; cur = cur->next) {
        printf("[%d]", cur->data);
        if (cur->next != NULL) printf(" -> ");
    }
    if (head == NULL) printf("(empty)");
    printf("\n");
}
/* [10] -> [20] -> [30] */
```

### Con direcciones de memoria (debug profundo)

```c
void list_print_debug(const Node *head) {
    int i = 0;
    for (const Node *cur = head; cur != NULL; cur = cur->next) {
        printf("  [%d] addr=%p  data=%d  next=%p\n",
               i++, (void *)cur, cur->data, (void *)cur->next);
    }
    if (head == NULL) printf("  (empty list)\n");
}
```

Salida ejemplo:

```
  [0] addr=0x55a1b2c040  data=10  next=0x55a1b2c060
  [1] addr=0x55a1b2c060  data=20  next=0x55a1b2c080
  [2] addr=0x55a1b2c080  data=30  next=(nil)
```

Esta función es invaluable para depurar bugs de punteros — permite ver
exactamente qué nodo apunta a dónde.

---

## Puntero a función: generalizar el recorrido

Las funciones de arriba (sumar, contar, máximo, imprimir) tienen la misma
estructura — solo cambia lo que se hace con cada elemento.  En C, podemos
abstraer el recorrido usando un **puntero a función**:

### foreach: aplicar una función a cada nodo

```c
void list_foreach(const Node *head, void (*fn)(int)) {
    for (const Node *cur = head; cur != NULL; cur = cur->next) {
        fn(cur->data);
    }
}
```

`void (*fn)(int)` es un puntero a una función que recibe `int` y retorna
`void`.

```c
/* Funciones que se pasan como argumento */
void print_item(int x) {
    printf("%d ", x);
}

void print_squared(int x) {
    printf("%d ", x * x);
}

int main(void) {
    /* ... crear lista [10, 20, 30] ... */

    printf("Items:   ");
    list_foreach(head, print_item);       // 10 20 30
    printf("\n");

    printf("Squared: ");
    list_foreach(head, print_squared);    // 100 400 900
    printf("\n");

    return 0;
}
```

### foreach con contexto (void *)

A veces la función necesita acceso a un estado acumulado (como una suma).
Se pasa un puntero `void *context`:

```c
void list_foreach_ctx(const Node *head,
                      void (*fn)(int, void *),
                      void *context) {
    for (const Node *cur = head; cur != NULL; cur = cur->next) {
        fn(cur->data, context);
    }
}

/* Acumular suma */
void accumulate_sum(int x, void *ctx) {
    int *sum = (int *)ctx;
    *sum += x;
}

/* Encontrar máximo */
void track_max(int x, void *ctx) {
    int *max = (int *)ctx;
    if (x > *max) *max = x;
}

int main(void) {
    /* ... lista [10, 20, 30] ... */

    int sum = 0;
    list_foreach_ctx(head, accumulate_sum, &sum);
    printf("Sum: %d\n", sum);   // 60

    int max = INT_MIN;
    list_foreach_ctx(head, track_max, &max);
    printf("Max: %d\n", max);   // 30

    return 0;
}
```

El patrón `void *context` es la forma genérica en C de pasar estado a
callbacks.  Es el precursor del **closure** en lenguajes modernos.

### map: crear una nueva lista transformada

```c
Node *list_map(const Node *head, int (*fn)(int)) {
    Node *new_head = NULL;
    Node *new_tail = NULL;

    for (const Node *cur = head; cur != NULL; cur = cur->next) {
        Node *node = create_node(fn(cur->data));
        if (new_head == NULL) {
            new_head = node;
        } else {
            new_tail->next = node;
        }
        new_tail = node;
    }
    return new_head;
}

int double_value(int x) { return x * 2; }
int negate(int x) { return -x; }

int main(void) {
    /* lista = [10, 20, 30] */
    Node *doubled = list_map(head, double_value);
    list_print(doubled);    // [20, 40, 60]

    Node *negated = list_map(head, negate);
    list_print(negated);    // [-10, -20, -30]

    list_free(&doubled);
    list_free(&negated);
    return 0;
}
```

### filter: crear lista con elementos que cumplen un predicado

```c
Node *list_filter(const Node *head, bool (*pred)(int)) {
    Node *new_head = NULL;
    Node *new_tail = NULL;

    for (const Node *cur = head; cur != NULL; cur = cur->next) {
        if (pred(cur->data)) {
            Node *node = create_node(cur->data);
            if (new_head == NULL) {
                new_head = node;
            } else {
                new_tail->next = node;
            }
            new_tail = node;
        }
    }
    return new_head;
}

bool is_even(int x) { return x % 2 == 0; }
bool is_positive(int x) { return x > 0; }

int main(void) {
    /* lista = [1, 2, 3, 4, 5, 6] */
    Node *evens = list_filter(head, is_even);
    list_print(evens);    // [2, 4, 6]

    list_free(&evens);
    return 0;
}
```

### reduce (fold): acumular un resultado

```c
int list_reduce(const Node *head, int initial, int (*fn)(int, int)) {
    int acc = initial;
    for (const Node *cur = head; cur != NULL; cur = cur->next) {
        acc = fn(acc, cur->data);
    }
    return acc;
}

int add(int a, int b) { return a + b; }
int mul(int a, int b) { return a * b; }
int max(int a, int b) { return a > b ? a : b; }

int main(void) {
    /* lista = [1, 2, 3, 4, 5] */
    printf("sum:     %d\n", list_reduce(head, 0, add));     // 15
    printf("product: %d\n", list_reduce(head, 1, mul));     // 120
    printf("max:     %d\n", list_reduce(head, INT_MIN, max)); // 5
    return 0;
}
```

---

## Recorrido recursivo

La lista enlazada es una estructura recursiva: es `NULL` (caso base) o un nodo
seguido de otra lista (caso recursivo).  Esto permite recorridos recursivos
naturales:

### Imprimir recursivamente

```c
void list_print_rec(const Node *head) {
    if (head == NULL) return;
    printf("%d ", head->data);
    list_print_rec(head->next);
}
```

### Imprimir en reversa (sin invertir la lista)

```c
void list_print_reverse(const Node *head) {
    if (head == NULL) return;
    list_print_reverse(head->next);   /* primero recurrir al resto */
    printf("%d ", head->data);         /* luego imprimir este nodo */
}
```

El truco: imprimir **después** de la llamada recursiva.  La pila de llamadas
almacena los nodos en orden; al retornar, los imprime de atrás hacia adelante.

```
list_print_reverse([10] → [20] → [30]):
  list_print_reverse([20] → [30]):
    list_print_reverse([30]):
      list_print_reverse(NULL):  ← return
      printf("30 ")             ← fase de vuelta
    printf("20 ")
  printf("10 ")

Salida: 30 20 10
```

Profundidad de pila: $O(n)$ — no usar para listas largas.

### Suma recursiva

```c
int list_sum_rec(const Node *head) {
    if (head == NULL) return 0;
    return head->data + list_sum_rec(head->next);
}
```

### Longitud recursiva

```c
int list_length_rec(const Node *head) {
    if (head == NULL) return 0;
    return 1 + list_length_rec(head->next);
}
```

---

## Equivalentes en Rust

### Recorrido con referencia

```rust
fn print_list<T: std::fmt::Display>(head: &Option<Box<Node<T>>>) {
    let mut current = head;
    while let Some(node) = current {
        print!("{} ", node.data);
        current = &node.next;
    }
    println!();
}
```

### foreach con closure

En Rust, los closures reemplazan los punteros a función + void *context:

```rust
impl<T> Node<T> {
    fn foreach<F: Fn(&T)>(&self, f: F) {
        f(&self.data);
        if let Some(next) = &self.next {
            next.foreach(f);
        }
    }
}

fn main() {
    // ... crear lista ...
    let mut sum = 0;
    list.as_ref().unwrap().foreach(|x| sum += x);
    println!("Sum: {sum}");
}
```

### Comparación puntero a función vs closure

| Aspecto | C (puntero a función) | Rust (closure) |
|---------|----------------------|----------------|
| Sintaxis | `void (*fn)(int, void *)` | `F: Fn(&T)` |
| Estado capturado | Manual con `void *context` | Automático (captura del entorno) |
| Type safety | Ninguna (`void *` pierde el tipo) | Completa (genéricos + traits) |
| Inline por el compilador | Rara vez | Casi siempre (monomorphization) |
| Ejemplo | `fn(cur->data, ctx)` | `f(&node.data)` |

El closure de Rust captura variables del entorno automáticamente — no
necesita el truco de `void *context`.  Además, el compilador conoce el tipo
concreto del closure, lo que permite inlining agresivo.

---

## Iterator trait en Rust

Para que una lista sea iterable con `for val in &list`, implementamos el
trait `Iterator`:

```rust
struct Node<T> {
    data: T,
    next: Option<Box<Node<T>>>,
}

struct List<T> {
    head: Option<Box<Node<T>>>,
}

struct ListIter<'a, T> {
    current: Option<&'a Node<T>>,
}

impl<T> List<T> {
    fn iter(&self) -> ListIter<'_, T> {
        ListIter {
            current: self.head.as_deref(),
        }
    }
}

impl<'a, T> Iterator for ListIter<'a, T> {
    type Item = &'a T;

    fn next(&mut self) -> Option<Self::Item> {
        self.current.map(|node| {
            self.current = node.next.as_deref();
            &node.data
        })
    }
}
```

Ahora funciona todo el ecosistema de iteradores:

```rust
fn main() {
    let list = /* ... */;

    // for loop
    for val in list.iter() {
        print!("{val} ");
    }

    // sum, count, max
    let sum: i32 = list.iter().sum();
    let count = list.iter().count();
    let max = list.iter().max();

    // map + collect
    let doubled: Vec<i32> = list.iter().map(|x| x * 2).collect();

    // filter + collect
    let evens: Vec<&i32> = list.iter().filter(|x| *x % 2 == 0).collect();

    // fold (reduce)
    let product: i32 = list.iter().fold(1, |acc, x| acc * x);
}
```

`as_deref()` convierte `Option<&Box<Node<T>>>` en `Option<&Node<T>>` —
desreferencia a través del `Box` para obtener una referencia al nodo.

---

## Ejercicios

### Ejercicio 1 — Funciones de recorrido

Implementa `list_length`, `list_sum`, `list_max` y `list_contains` usando el
patrón de recorrido con `for`.  Prueba con la lista `[3, 7, 1, 9, 4]`.

<details>
<summary>Resultados esperados</summary>

```
length:   5
sum:      24
max:      9
contains(7): true
contains(5): false
```
</details>

### Ejercicio 2 — print_debug

Usa `list_print_debug` (con direcciones) para visualizar una lista de 4
elementos.  Verifica que las direcciones `next` coinciden con las direcciones
de los nodos siguientes.

<details>
<summary>Qué observar</summary>

El `next` de cada nodo debe coincidir con el `addr` del siguiente nodo.  El
`next` del último nodo es `(nil)` / `0x0`.  Las direcciones probablemente no
son consecutivas — `malloc` las coloca donde hay espacio.
</details>

### Ejercicio 3 — foreach y callbacks

Escribe 3 funciones de callback e invócalas con `list_foreach`:
1. Imprimir cada valor multiplicado por 10.
2. Imprimir "par" o "impar" para cada valor.
3. Contar cuántos son mayores que un umbral (usa `list_foreach_ctx`).

<details>
<summary>Pista para el umbral</summary>

```c
typedef struct { int threshold; int count; } ThresholdCtx;

void count_above(int x, void *ctx) {
    ThresholdCtx *tc = (ThresholdCtx *)ctx;
    if (x > tc->threshold) tc->count++;
}

ThresholdCtx tc = { .threshold = 5, .count = 0 };
list_foreach_ctx(head, count_above, &tc);
printf("Above %d: %d\n", tc.threshold, tc.count);
```
</details>

### Ejercicio 4 — map y filter

Usando las funciones `list_map` y `list_filter` del tópico:
1. Crea una lista `[1, 2, 3, 4, 5, 6, 7, 8]`.
2. Filtra los pares.
3. Mapea el resultado multiplicando por 3.
4. Imprime el resultado final.

<details>
<summary>Resultado esperado</summary>

Filtrar pares: `[2, 4, 6, 8]`.  Multiplicar por 3: `[6, 12, 18, 24]`.
Recuerda liberar las listas intermedias para evitar memory leaks.
</details>

### Ejercicio 5 — reduce

Implementa usando `list_reduce`:
1. Suma de todos los elementos.
2. Producto de todos los elementos.
3. Máximo.
4. Concatenar dígitos: `[1, 2, 3] → 123` (hint: `fn(acc, x) = acc * 10 + x`).

<details>
<summary>Resultado para [1, 2, 3, 4, 5]</summary>

```
sum:     15
product: 120
max:     5
concat:  12345
```
</details>

### Ejercicio 6 — Imprimir en reversa

Implementa `list_print_reverse` recursivo.  Prueba con `[10, 20, 30, 40]`.
¿Cuál es la profundidad máxima de pila?  ¿Para qué largo de lista empieza
a ser un problema?

<details>
<summary>Respuestas</summary>

Salida: `40 30 20 10`.  Profundidad: $n$ (un marco por nodo).  Con stack
de 8 MB y ~64 bytes por marco: ~125,000 nodos antes de overflow.  Alternativa
sin recursión: invertir la lista, imprimir, invertir de nuevo ($O(n)$ tiempo,
$O(1)$ espacio extra).
</details>

### Ejercicio 7 — any y all con predicados

Implementa:
- `list_any(head, pred)` — retorna `true` si algún elemento cumple `pred`.
- `list_all(head, pred)` — retorna `true` si todos cumplen `pred`.

```c
bool list_any(const Node *head, bool (*pred)(int));
bool list_all(const Node *head, bool (*pred)(int));
```

<details>
<summary>Implementación</summary>

```c
bool list_any(const Node *head, bool (*pred)(int)) {
    for (const Node *cur = head; cur != NULL; cur = cur->next) {
        if (pred(cur->data)) return true;
    }
    return false;
}

bool list_all(const Node *head, bool (*pred)(int)) {
    for (const Node *cur = head; cur != NULL; cur = cur->next) {
        if (!pred(cur->data)) return false;
    }
    return true;
}
```

`any` retorna `true` al primer match (cortocircuito).  `all` retorna `false`
al primer fallo.  Para lista vacía: `any → false`, `all → true` (convención
estándar).
</details>

### Ejercicio 8 — Iterator en Rust

Implementa el trait `Iterator` para una lista enlazada en Rust (usa la
estructura del tópico).  Luego usa `.sum()`, `.max()`, `.filter()`, `.map()`
y `.fold()` con la lista `[1, 2, 3, 4, 5]`.

<details>
<summary>Pruebas</summary>

```rust
let list = List::from_slice(&[1, 2, 3, 4, 5]);

assert_eq!(list.iter().sum::<i32>(), 15);
assert_eq!(list.iter().copied().max(), Some(5));
assert_eq!(list.iter().filter(|&&x| x % 2 == 0).count(), 2);

let doubled: Vec<i32> = list.iter().map(|x| x * 2).collect();
assert_eq!(doubled, vec![2, 4, 6, 8, 10]);
```
</details>

### Ejercicio 9 — Comparar listas

Escribe `bool list_equal(const Node *a, const Node *b)` que retorne `true` si
ambas listas tienen los mismos datos en el mismo orden.

<details>
<summary>Implementación</summary>

```c
bool list_equal(const Node *a, const Node *b) {
    while (a != NULL && b != NULL) {
        if (a->data != b->data) return false;
        a = a->next;
        b = b->next;
    }
    return a == NULL && b == NULL;  /* ambas deben terminar al mismo tiempo */
}
```

Si una termina antes que la otra (distinto largo), retorna `false`.
</details>

### Ejercicio 10 — Pipeline completo

Implementa un pipeline que procese una lista de notas de estudiantes (enteros
0-100):

1. `filter`: notas ≥ 60 (aprobados).
2. `map`: convertir a escala 1-5 (`nota / 20`).
3. `reduce`: promedio (suma / count).

Implementa en C con las funciones del tópico y en Rust con iteradores.

<details>
<summary>Ejemplo</summary>

Notas: `[85, 42, 73, 91, 55, 68, 30, 77]`.
Aprobados (≥60): `[85, 73, 91, 68, 77]`.
Escala 1-5: `[4, 3, 4, 3, 3]`.
Promedio: `17 / 5 = 3.4`.

En Rust:
```rust
let avg: f64 = list.iter()
    .filter(|&&x| x >= 60)
    .map(|&x| x / 20)
    .fold((0, 0), |(sum, count), x| (sum + x, count + 1));
// avg = sum as f64 / count as f64
```
</details>
