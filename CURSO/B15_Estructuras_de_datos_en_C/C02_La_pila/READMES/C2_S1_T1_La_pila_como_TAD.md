# T01 — La pila como TAD

---

## 1. Qué es una pila

Una pila (stack) es una colección ordenada de elementos donde la inserción y
la eliminación siempre ocurren en el **mismo extremo**, llamado el **tope**
(top). El principio que la gobierna es **LIFO**: Last In, First Out — el
último elemento en entrar es el primero en salir.

```
Analogía física:

  Una pila de platos:
  - Pones un plato nuevo ARRIBA     (push)
  - Sacas el plato de ARRIBA        (pop)
  - No puedes sacar el de abajo sin quitar los de arriba

         ┌─────────┐
         │ plato 4 │  ← top (el último que pusiste)
         ├─────────┤
         │ plato 3 │
         ├─────────┤
         │ plato 2 │
         ├─────────┤
         │ plato 1 │  ← el primero que pusiste (el último en salir)
         └─────────┘
```

### LIFO vs FIFO

```
LIFO (pila):   el último en entrar es el primero en salir
  Push: A, B, C
  Pop:  C, B, A   ← orden inverso

FIFO (cola):   el primero en entrar es el primero en salir
  Enqueue: A, B, C
  Dequeue: A, B, C   ← mismo orden
```

La pila **invierte** el orden. Esta propiedad es fundamental y es la razón
por la que aparece en tantos algoritmos (recursión, evaluación de expresiones,
backtracking, undo).

---

## 2. Operaciones primitivas

La pila como TAD define un conjunto mínimo de operaciones. Todo lo demás
se construye sobre estas:

### Operaciones esenciales

| Operación | Descripción | Complejidad |
|-----------|------------|-------------|
| `push(x)` | Inserta `x` en el tope | $O(1)$ |
| `pop()` | Remueve y retorna el elemento del tope | $O(1)$ |
| `peek()` / `top()` | Retorna el elemento del tope **sin removerlo** | $O(1)$ |
| `is_empty()` | Retorna `true` si la pila no tiene elementos | $O(1)$ |
| `size()` / `len()` | Retorna la cantidad de elementos | $O(1)$ |

Todas las operaciones son $O(1)$. Esto es una propiedad fundamental de la pila
— no importa cuántos elementos tenga, cada operación tarda lo mismo.

### Visualización de push y pop

```
Estado inicial (vacía):
  top → (nada)
  [ ]

push(10):
  top → 10
  [ 10 ]

push(20):
  top → 20
  [ 10 | 20 ]

push(30):
  top → 30
  [ 10 | 20 | 30 ]

pop() → retorna 30:
  top → 20
  [ 10 | 20 ]

peek() → retorna 20 (sin remover):
  top → 20
  [ 10 | 20 ]

pop() → retorna 20:
  top → 10
  [ 10 ]

pop() → retorna 10:
  top → (nada)
  [ ]

pop() → ERROR: pila vacía
```

---

## 3. El contrato del TAD

El TAD Pila define un contrato — reglas que toda implementación debe cumplir,
sin importar si usa un array, una lista enlazada, o cualquier otra estructura
interna.

### Axiomas

Dado un stack `s` y un elemento `x`:

```
1. pop(push(s, x)) = (s, x)
   → Hacer push y luego pop retorna la pila al estado original
     y devuelve el elemento que se insertó.

2. peek(push(s, x)) = x
   → El elemento en el tope es siempre el último que se insertó.

3. is_empty(new()) = true
   → Una pila recién creada está vacía.

4. is_empty(push(s, x)) = false
   → Una pila después de un push nunca está vacía.

5. size(new()) = 0
   → Una pila recién creada tiene tamaño 0.

6. size(push(s, x)) = size(s) + 1
   → Cada push incrementa el tamaño en 1.

7. size(pop(s)) = size(s) - 1   (si s no está vacía)
   → Cada pop decrementa el tamaño en 1.
```

Estos axiomas definen la **semántica** de la pila. Si una implementación viola
cualquiera de estos, está rota — sin importar que compile y parezca funcionar.

### Precondiciones

| Operación | Precondición | Qué pasa si se viola |
|-----------|-------------|---------------------|
| `pop()` | La pila no está vacía | Comportamiento indefinido o error |
| `peek()` | La pila no está vacía | Comportamiento indefinido o error |
| `push(x)` | (Ninguna en TAD puro, pero capacidad en implementaciones con array) | Stack overflow |

### Postcondiciones

| Operación | Postcondición |
|-----------|--------------|
| `push(x)` | `size` aumentó en 1, `peek()` retorna `x` |
| `pop()` | `size` disminuyó en 1, retorna el ex-tope |
| `peek()` | `size` no cambió, la pila no se modificó |

---

## 4. Condiciones excepcionales

### Stack underflow — pop en pila vacía

Qué hacer cuando el usuario llama `pop()` o `peek()` en una pila vacía es una
decisión de diseño:

| Estrategia | C | Rust |
|-----------|---|------|
| Retornar valor centinela | Retornar `-1` o `INT_MIN` | No idiomático |
| Retornar indicador de error | Retornar `bool` + out param | `Option<T>` → `None` |
| Abortar el programa | `assert(!is_empty)` o `exit(1)` | `panic!()` |
| Comportamiento indefinido | No verificar, crashear | No aplica (no idiomático) |

**En C**, la estrategia más común es verificar con assert o retornar un código
de error. El problema del valor centinela (retornar `-1`) es que `-1` podría
ser un elemento válido del stack.

**En Rust**, `Option<T>` es la solución natural: `pop()` retorna `Some(x)` si
hay elementos, `None` si está vacía. No hay ambigüedad.

```c
// C: pop con assert
int stack_pop(Stack *s) {
    assert(s->top > 0 && "stack underflow");
    return s->data[--s->top];
}

// C: pop con retorno de error
bool stack_pop(Stack *s, int *out) {
    if (s->top == 0) return false;
    *out = s->data[--s->top];
    return true;
}
```

```rust
// Rust: pop con Option
fn pop(&mut self) -> Option<T> {
    self.data.pop()  // retorna None si está vacío
}
```

### Stack overflow — push en pila llena

Solo aplica a implementaciones con array de tamaño fijo:

```c
// C: push con verificación de capacidad
bool stack_push(Stack *s, int elem) {
    if (s->top >= s->capacity) return false;  // llena
    s->data[s->top++] = elem;
    return true;
}
```

Alternativas:
- **Array dinámico**: redimensionar (`realloc`) cuando se llena — elimina el
  problema a cambio de $O(1)$ amortizado en vez de $O(1)$ estricto
- **Lista enlazada**: no hay límite fijo — solo la memoria disponible

En Rust con `Vec<T>`, el overflow no existe: `Vec` crece automáticamente.

---

## 5. La interfaz del TAD en C

### Header (stack.h)

```c
#ifndef STACK_H
#define STACK_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Stack Stack;

// Crear y destruir
Stack *stack_create(size_t capacity);
void   stack_destroy(Stack *s);

// Operaciones primitivas
bool   stack_push(Stack *s, int elem);
bool   stack_pop(Stack *s, int *out);
bool   stack_peek(const Stack *s, int *out);
bool   stack_is_empty(const Stack *s);
size_t stack_size(const Stack *s);

#endif
```

Notas:
- `Stack` es un tipo opaco (`typedef struct Stack Stack`) — el usuario no
  conoce la implementación interna
- `stack_pop` y `stack_peek` retornan `bool` para indicar éxito/fracaso,
  y el valor se escribe en `*out`
- `const Stack *s` en `peek`, `is_empty`, `size` — estas operaciones no
  modifican la pila
- `stack_create` / `stack_destroy` — patrón constructor/destructor para
  gestionar memoria

### Uso

```c
Stack *s = stack_create(100);

stack_push(s, 10);
stack_push(s, 20);
stack_push(s, 30);

int val;
stack_peek(s, &val);     // val = 30
stack_pop(s, &val);      // val = 30
stack_pop(s, &val);      // val = 20

printf("size: %zu\n", stack_size(s));   // 1
printf("empty: %d\n", stack_is_empty(s)); // 0

stack_destroy(s);
```

---

## 6. La interfaz del TAD en Rust

### Trait como contrato

```rust
pub trait StackTrait<T> {
    fn new() -> Self;
    fn push(&mut self, elem: T);
    fn pop(&mut self) -> Option<T>;
    fn peek(&self) -> Option<&T>;
    fn is_empty(&self) -> bool;
    fn len(&self) -> usize;
}
```

Diferencias con la interfaz C:
- **Genérico**: `<T>` en vez de `int` — funciona con cualquier tipo
- **Option**: `pop` y `peek` retornan `Option`, no hay error silencioso
- **`&self`/`&mut self`**: el compilador sabe qué operaciones mutan la pila
- **Sin destroy**: `Drop` se implementa aparte (o es automático con `Vec`)

### Uso

```rust
let mut s = VecStack::new();

s.push(10);
s.push(20);
s.push(30);

assert_eq!(s.peek(), Some(&30));
assert_eq!(s.pop(), Some(30));
assert_eq!(s.pop(), Some(20));
assert_eq!(s.len(), 1);
assert!(!s.is_empty());
```

---

## 7. Invariantes de la pila

Independientemente de la implementación, una pila correcta mantiene estos
invariantes:

```
1. Orden LIFO: el último push es el próximo pop
   → Si push(A), push(B), push(C), entonces pop = C, pop = B, pop = A

2. Consistencia de tamaño:
   → size = número de pushes - número de pops (nunca negativo)
   → is_empty ⟺ size == 0

3. Peek no modifica:
   → peek no cambia size ni el estado interno
   → Dos peek consecutivos retornan lo mismo (si no hay push/pop entre ellos)

4. Pop retorna y remueve:
   → Después de pop, size disminuye en 1
   → El elemento retornado ya no está en la pila

5. Push siempre agrega al tope:
   → Después de push(x), peek = x
   → Los elementos previos no se modifican
```

Estos invariantes sirven como **tests de regresión**: si alguno falla después
de un cambio, la implementación está rota.

---

## 8. Comparación de implementaciones posibles

Antes de ver las implementaciones detalladas (T02, T03, T04), es útil conocer
las opciones y sus tradeoffs:

| Implementación | Push | Pop | Memoria | Capacidad |
|---------------|------|-----|---------|-----------|
| Array estático | $O(1)$ | $O(1)$ | Fija, preasignada | Limitada |
| Array dinámico | $O(1)$ amortizado | $O(1)$ | Crece según necesidad | Ilimitada (hasta RAM) |
| Lista enlazada | $O(1)$ | $O(1)$ | Un malloc por push | Ilimitada (hasta RAM) |

### Array estático

```
Ventajas:
  - O(1) estricto (sin reallocs)
  - Mejor cache locality
  - Sin overhead de punteros

Desventajas:
  - Capacidad fija — stack overflow si se excede
  - Desperdicio si se sobredimensiona
```

### Array dinámico (Vec)

```
Ventajas:
  - Sin límite fijo
  - O(1) amortizado (realloc duplica)
  - Buena cache locality

Desventajas:
  - Ocasionalmente O(n) por realloc
  - Puede desperdiciar ~50% de capacidad
```

### Lista enlazada

```
Ventajas:
  - O(1) estricto
  - Sin desperdicio (cada nodo = un elemento)
  - Sin reallocs

Desventajas:
  - Un malloc/free por push/pop (lento)
  - Overhead de puntero por nodo (8 bytes extra en 64-bit)
  - Mala cache locality (nodos dispersos en memoria)
```

### Recomendación general

| Situación | Implementación |
|-----------|---------------|
| Tamaño máximo conocido y pequeño | Array estático |
| Tamaño desconocido, rendimiento importa | Array dinámico (Vec) |
| O(1) estricto requerido (tiempo real) | Lista enlazada |
| Aprendizaje de punteros | Lista enlazada |

En la práctica, **el array dinámico (Vec) es la opción por defecto**. La lista
enlazada se estudia por su valor pedagógico y porque es la base para
estructuras más complejas.

---

## 9. Dónde aparece la pila

### En el lenguaje y el sistema

| Uso | Cómo funciona |
|-----|-------------|
| Call stack | Cada llamada a función push un frame; return lo poppea |
| Recursión | Cada nivel recursivo es un push en el call stack |
| Variables locales | Viven en el frame del stack |
| `alloca()` | Asigna memoria en el stack (se libera al retornar) |

### En algoritmos

| Aplicación | Por qué usa pila |
|-----------|-----------------|
| Paréntesis balanceados | Push al abrir, pop al cerrar — si matchean, están balanceados |
| Evaluación de expresiones (RPN) | Los operandos se push, los operadores pop 2 y push el resultado |
| Conversión infija → posfija | La pila maneja precedencia y asociatividad |
| DFS (búsqueda en profundidad) | La pila mantiene el camino actual (explícita o vía recursión) |
| Undo/Redo | Cada acción se push al undo stack; undo la poppea y la push al redo stack |
| Historial de navegación | Cada página se push; back poppea y push al forward stack |
| Backtracking | Se prueban opciones con push; si fallan, pop para retroceder |

Todas estas aplicaciones explotan la propiedad LIFO: necesitan acceder al
**último** elemento agregado, procesar los elementos en orden **inverso**,
o mantener un **historial** que se deshace de lo más reciente primero.

---

## 10. Stack vs heap: no confundir

El término "stack" aparece en dos contextos distintos:

```
1. Stack como ESTRUCTURA DE DATOS (este capítulo):
   → Colección LIFO con push/pop
   → Puede almacenar cualquier tipo de dato
   → Se implementa en el heap o donde sea

2. Stack como REGIÓN DE MEMORIA (del sistema operativo):
   → El "call stack" donde se almacenan frames de función
   → Variables locales, dirección de retorno, argumentos
   → Tamaño limitado (~1-8 MB típico)
   → Stack overflow = exceder este límite
```

Una pila (estructura de datos) implementada con `malloc` vive en el **heap**
(región de memoria). El call stack del programa vive en el **stack** (región
de memoria). Los nombres coinciden porque el call stack **es** una pila — pero
son conceptos diferentes.

```c
// La variable s está en el stack (región de memoria)
// El array s.data apunta al heap (malloc)
Stack *s = stack_create(100);

// push agrega al stack (estructura de datos)
// que internamente escribe en el heap (región de memoria)
stack_push(s, 42);
```

---

## Ejercicios

### Ejercicio 1 — Simular push y pop

Dada esta secuencia de operaciones, dibuja el estado de la pila después de
cada una:

```
push(5)
push(3)
push(8)
pop()
push(1)
peek()
pop()
pop()
push(7)
pop()
pop()
```

**Prediccion**: Después de la secuencia completa, la pila está vacía?
Cuántos elementos tiene?

<details><summary>Respuesta</summary>

```
Operación       Estado           Retorna    Size
─────────       ──────           ───────    ────
(inicio)        [ ]              —          0
push(5)         [ 5 ]            —          1
push(3)         [ 5, 3 ]         —          2
push(8)         [ 5, 3, 8 ]      —          3
pop()           [ 5, 3 ]         8          2
push(1)         [ 5, 3, 1 ]      —          3
peek()          [ 5, 3, 1 ]      1          3
pop()           [ 5, 3 ]         1          2
pop()           [ 5 ]            3          1
push(7)         [ 5, 7 ]         —          2
pop()           [ 5 ]            7          1
pop()           [ ]              5          0
```

Sí, la pila **está vacía** al final. 6 pushes y 5 pops + la última pop = 6
pops. Como pushes == pops, el resultado es vacío.

Regla rápida: `size final = pushes - pops = 6 - 6 = 0`.

</details>

---

### Ejercicio 2 — Verificar axiomas

Escribe un test (en pseudocódigo o en C/Rust) que verifique cada uno de los
7 axiomas de la sección 3. Para cada axioma, crea una aserción que falle si
el axioma se viola:

```
Axioma 1: pop(push(s, x)) == (s, x)
Axioma 2: peek(push(s, x)) == x
... etc.
```

**Prediccion**: Si una implementación pasa los 7 axiomas pero falla con
2 pushes seguidos, qué axioma está mal?

<details><summary>Respuesta</summary>

```rust
fn verify_axioms() {
    // Axioma 1: pop(push(s, x)) = (s, x)
    let mut s = VecStack::new();
    s.push(42);
    assert_eq!(s.pop(), Some(42));
    assert!(s.is_empty());  // s vuelve al estado original (vacío)

    // Axioma 2: peek(push(s, x)) = x
    let mut s = VecStack::new();
    s.push(99);
    assert_eq!(s.peek(), Some(&99));

    // Axioma 3: is_empty(new()) = true
    let s: VecStack<i32> = VecStack::new();
    assert!(s.is_empty());

    // Axioma 4: is_empty(push(s, x)) = false
    let mut s = VecStack::new();
    s.push(1);
    assert!(!s.is_empty());

    // Axioma 5: size(new()) = 0
    let s: VecStack<i32> = VecStack::new();
    assert_eq!(s.len(), 0);

    // Axioma 6: size(push(s, x)) = size(s) + 1
    let mut s = VecStack::new();
    assert_eq!(s.len(), 0);
    s.push(1);
    assert_eq!(s.len(), 1);
    s.push(2);
    assert_eq!(s.len(), 2);

    // Axioma 7: size(pop(s)) = size(s) - 1
    let mut s = VecStack::new();
    s.push(1);
    s.push(2);
    assert_eq!(s.len(), 2);
    s.pop();
    assert_eq!(s.len(), 1);
    s.pop();
    assert_eq!(s.len(), 0);

    println!("All 7 axioms verified!");
}
```

Si falla con 2 pushes seguidos, el axioma más probable es el **1** o el **6**:
- Axioma 1 con estado no vacío: `push(B)` sobreescribe `A` en vez de apilar
  → `pop` retorna `B` pero el siguiente `pop` no retorna `A`
- Axioma 6: el tamaño no incrementa correctamente

Un test extendido del axioma 1 sería:

```rust
// Axioma 1 — versión con pila no vacía
let mut s = VecStack::new();
s.push(1);
s.push(2);       // s = [1, 2]
s.push(3);       // s = [1, 2, 3]
assert_eq!(s.pop(), Some(3));  // s vuelve a [1, 2]
assert_eq!(s.pop(), Some(2));  // s vuelve a [1]
assert_eq!(s.pop(), Some(1));  // s vuelve a []
```

</details>

---

### Ejercicio 3 — Invertir un string

Usa una pila para invertir un string. El algoritmo:
1. Push cada carácter del string
2. Pop todos los caracteres para construir el string invertido

Implementa en Rust:

```rust
fn reverse_string(s: &str) -> String {
    // Tu código aquí
}

fn main() {
    assert_eq!(reverse_string("hello"), "olleh");
    assert_eq!(reverse_string("abc"), "cba");
    assert_eq!(reverse_string(""), "");
    assert_eq!(reverse_string("a"), "a");
    println!("All tests passed!");
}
```

**Prediccion**: Por qué funciona? Qué propiedad de la pila causa la inversión?

<details><summary>Respuesta</summary>

```rust
fn reverse_string(s: &str) -> String {
    let mut stack: Vec<char> = Vec::new();

    // Push cada carácter
    for ch in s.chars() {
        stack.push(ch);
    }

    // Pop todos → orden inverso
    let mut result = String::new();
    while let Some(ch) = stack.pop() {
        result.push(ch);
    }

    result
}

fn main() {
    assert_eq!(reverse_string("hello"), "olleh");
    assert_eq!(reverse_string("abc"), "cba");
    assert_eq!(reverse_string(""), "");
    assert_eq!(reverse_string("a"), "a");
    println!("All tests passed!");
}
```

Funciona por la propiedad **LIFO**: el último carácter en entrar ('o' de
"hello") es el primero en salir. Esto invierte naturalmente el orden.

```
Push: h → e → l → l → o
Stack: [h, e, l, l, o]

Pop: o → l → l → e → h
Resultado: "olleh"
```

Nota: en la práctica, invertir un string en Rust se haría con
`s.chars().rev().collect::<String>()`. Pero el ejercicio demuestra el
principio de inversión por pila.

Nota sobre UTF-8: `.chars()` itera sobre code points Unicode, no bytes.
Para strings con emojis o caracteres multibyte, esto maneja correctamente
la inversión a nivel de caracteres (aunque invertir grapheme clusters
requeriría la crate `unicode-segmentation`).

</details>

---

### Ejercicio 4 — Decimal a binario

Usa una pila para convertir un entero positivo de decimal a binario. El
algoritmo:
1. Mientras n > 0: push(n % 2), n = n / 2
2. Pop todos los dígitos para construir el número binario

```rust
fn to_binary(n: u32) -> String {
    // Tu código aquí
}

fn main() {
    assert_eq!(to_binary(0), "0");
    assert_eq!(to_binary(1), "1");
    assert_eq!(to_binary(5), "101");
    assert_eq!(to_binary(10), "1010");
    assert_eq!(to_binary(255), "11111111");
    println!("All tests passed!");
}
```

**Prediccion**: Los dígitos salen de la pila en el orden correcto (MSB primero)?

<details><summary>Respuesta</summary>

```rust
fn to_binary(n: u32) -> String {
    if n == 0 {
        return String::from("0");
    }

    let mut stack: Vec<u32> = Vec::new();
    let mut num = n;

    // Generar dígitos (LSB primero)
    while num > 0 {
        stack.push(num % 2);
        num /= 2;
    }

    // Pop → MSB primero (la pila invierte el orden)
    let mut result = String::new();
    while let Some(bit) = stack.pop() {
        result.push(if bit == 1 { '1' } else { '0' });
    }

    result
}

fn main() {
    assert_eq!(to_binary(0), "0");
    assert_eq!(to_binary(1), "1");
    assert_eq!(to_binary(5), "101");
    assert_eq!(to_binary(10), "1010");
    assert_eq!(to_binary(255), "11111111");
    println!("All tests passed!");
}
```

Sí, los dígitos salen en el orden correcto. La razón:

```
n = 10 (decimal)

Paso  n    n%2  n/2    Push
1     10   0    5      stack: [0]
2     5    1    2      stack: [0, 1]
3     2    0    1      stack: [0, 1, 0]
4     1    1    0      stack: [0, 1, 0, 1]

n%2 genera los dígitos LSB-first (bit menos significativo primero).
La pila los invierte al hacer pop → MSB-first:
Pop: 1, 0, 1, 0 → "1010" ✓
```

El truco: la división genera dígitos en orden inverso al que necesitamos
para escribir el número. La pila corrige esto automáticamente.

Esto funciona para cualquier base. Para octal: `n % 8` y `n / 8`.
Para hexadecimal: `n % 16` y `n / 16` (con conversión 10→A, 11→B, etc.).

</details>

---

### Ejercicio 5 — Detectar palíndromo

Usa una pila para determinar si un string (ignorando mayúsculas y espacios)
es un palíndromo:

```rust
fn is_palindrome(s: &str) -> bool {
    // Tu código aquí
}

fn main() {
    assert!(is_palindrome("racecar"));
    assert!(is_palindrome("A man a plan a canal Panama"));
    assert!(is_palindrome(""));
    assert!(is_palindrome("a"));
    assert!(!is_palindrome("hello"));
    assert!(!is_palindrome("ab"));
    println!("All tests passed!");
}
```

**Prediccion**: Necesitas pushear todos los caracteres o solo la mitad?

<details><summary>Respuesta</summary>

```rust
fn is_palindrome(s: &str) -> bool {
    let cleaned: Vec<char> = s.chars()
        .filter(|c| c.is_alphabetic())
        .map(|c| c.to_lowercase().next().unwrap())
        .collect();

    let mut stack: Vec<char> = Vec::new();

    // Push todos los caracteres limpiados
    for &ch in &cleaned {
        stack.push(ch);
    }

    // Comparar: pop da el orden inverso
    for &ch in &cleaned {
        if stack.pop() != Some(ch) {
            return false;
        }
    }

    true
}

fn main() {
    assert!(is_palindrome("racecar"));
    assert!(is_palindrome("A man a plan a canal Panama"));
    assert!(is_palindrome(""));
    assert!(is_palindrome("a"));
    assert!(!is_palindrome("hello"));
    assert!(!is_palindrome("ab"));
    println!("All tests passed!");
}
```

Puedes pushear **solo la primera mitad** para optimizar:

```rust
fn is_palindrome_half(s: &str) -> bool {
    let cleaned: Vec<char> = s.chars()
        .filter(|c| c.is_alphabetic())
        .map(|c| c.to_lowercase().next().unwrap())
        .collect();

    let n = cleaned.len();
    let mut stack: Vec<char> = Vec::new();

    // Push solo la primera mitad
    for i in 0..n / 2 {
        stack.push(cleaned[i]);
    }

    // Comparar la segunda mitad con lo que sale de la pila
    let start = (n + 1) / 2;  // saltar el carácter central si n es impar
    for i in start..n {
        if stack.pop() != Some(cleaned[i]) {
            return false;
        }
    }

    true
}
```

La versión con mitad es $O(n)$ en tiempo y $O(n/2)$ en espacio extra.
La versión con todos es $O(n)$ en tiempo y $O(n)$ en espacio extra.

En la práctica, la forma idiomática sin pila sería:

```rust
fn is_palindrome_simple(s: &str) -> bool {
    let cleaned: String = s.chars()
        .filter(|c| c.is_alphabetic())
        .flat_map(|c| c.to_lowercase())
        .collect();
    cleaned == cleaned.chars().rev().collect::<String>()
}
```

Pero el ejercicio demuestra el uso de la pila como herramienta de inversión.

</details>

---

### Ejercicio 6 — Pila con mínimo O(1)

Diseña (a nivel de TAD, sin implementar) una pila que soporte, además de las
operaciones estándar, una operación `min()` que retorne el elemento mínimo en
$O(1)$.

Pista: usa una pila auxiliar.

```
Operaciones:
  push(x)   → O(1)
  pop()     → O(1)
  peek()    → O(1)
  min()     → O(1)   ← nueva
```

**Prediccion**: Si push(5), push(3), push(7), push(1), pop(), cuál es el
min después del pop?

<details><summary>Respuesta</summary>

La idea: mantener una **segunda pila** que rastrea el mínimo actual:

```
Pila principal     Pila de mínimos
                   (guarda el min actual)
push(5):
  [5]              [5]          ← 5 es el min

push(3):
  [5, 3]           [5, 3]      ← 3 < 5, nuevo min

push(7):
  [5, 3, 7]        [5, 3, 3]   ← 7 > 3, min sigue siendo 3

push(1):
  [5, 3, 7, 1]     [5, 3, 3, 1] ← 1 < 3, nuevo min

pop() → 1:
  [5, 3, 7]        [5, 3, 3]   ← pop de ambas pilas

min() → 3 ← tope de la pila de mínimos
```

Después del pop, el min es **3** (no 1, porque 1 fue removido).

Diseño:

```rust
pub struct MinStack<T: Ord> {
    data: Vec<T>,
    mins: Vec<T>,   // mins.peek() = mínimo actual
}

impl<T: Ord + Clone> MinStack<T> {
    pub fn push(&mut self, x: T) {
        let new_min = match self.mins.last() {
            None => x.clone(),
            Some(current_min) => {
                if x <= *current_min { x.clone() } else { current_min.clone() }
            }
        };
        self.data.push(x);
        self.mins.push(new_min);
    }

    pub fn pop(&mut self) -> Option<T> {
        self.mins.pop();      // mantener sincronizadas
        self.data.pop()
    }

    pub fn min(&self) -> Option<&T> {
        self.mins.last()
    }
}
```

Complejidad:
- Espacio extra: $O(n)$ — una pila adicional del mismo tamaño
- Todas las operaciones: $O(1)$

Optimización posible: solo push a la pila de mínimos cuando el nuevo valor
es $\leq$ el mínimo actual, y solo pop cuando el valor que sale es igual al
mínimo actual. Esto reduce el espacio de la pila de mínimos en el caso
promedio.

</details>

---

### Ejercicio 7 — Orden de pop

Dada la secuencia de push: 1, 2, 3, 4, 5 (en ese orden), determina si las
siguientes secuencias de pop son válidas:

```
a) 5, 4, 3, 2, 1
b) 1, 2, 3, 4, 5
c) 3, 2, 5, 4, 1
d) 3, 4, 1, 2, 5
e) 1, 5, 2, 4, 3
```

Las operaciones push y pop se pueden intercalar. Simula para verificar.

**Prediccion**: Es posible obtener la secuencia (b) — el mismo orden de entrada?

<details><summary>Respuesta</summary>

Para cada secuencia, simulamos push/pop intercalados:

**a) 5, 4, 3, 2, 1** — **VÁLIDA**

```
Push 1,2,3,4,5 → stack [1,2,3,4,5]
Pop 5,4,3,2,1 → orden inverso completo ✓
```

**b) 1, 2, 3, 4, 5** — **VÁLIDA**

```
Push 1, Pop 1       stack: []
Push 2, Pop 2       stack: []
Push 3, Pop 3       stack: []
Push 4, Pop 4       stack: []
Push 5, Pop 5       stack: []
```

Sí, es posible — push y pop inmediato para cada elemento.

**c) 3, 2, 5, 4, 1** — **VÁLIDA**

```
Push 1, Push 2, Push 3    stack: [1,2,3]
Pop 3                     stack: [1,2]
Pop 2                     stack: [1]
Push 4, Push 5            stack: [1,4,5]
Pop 5                     stack: [1,4]
Pop 4                     stack: [1]
Pop 1                     stack: []  ✓
```

**d) 3, 4, 1, 2, 5** — **INVÁLIDA**

```
Push 1, Push 2, Push 3    stack: [1,2,3]
Pop 3                     stack: [1,2]
Push 4, Pop 4             stack: [1,2]
Pop → necesito 1, pero el tope es 2  ✗
```

No se puede obtener 1 sin primero sacar 2.

**e) 1, 5, 2, 4, 3** — **INVÁLIDA**

```
Push 1, Pop 1             stack: []
Push 2,3,4,5, Pop 5       stack: [2,3,4]
Pop → necesito 2, pero el tope es 4  ✗
```

No se puede obtener 2 sin sacar 4 y 3 primero.

Regla para verificar: una secuencia de pop es válida si y solo si no requiere
acceder a un elemento que está **debajo** de otro sin sacar el de arriba
primero. Si en algún punto necesitas un elemento que no está en el tope,
y hay elementos encima que aún no has "consumido" en tu secuencia de salida,
la secuencia es inválida.

</details>

---

### Ejercicio 8 — Diseño genérico en C

Diseña la interfaz de un stack genérico en C que pueda almacenar cualquier
tipo de dato. ¿Qué necesitas usar en vez de `int`?

```c
// ¿Cómo cambiarías el header para que funcione con
// int, float, structs, strings, etc.?
```

**Prediccion**: Necesitas `void *`? Qué problema introduce respecto a la
versión con `int`?

<details><summary>Respuesta</summary>

```c
#ifndef GENERIC_STACK_H
#define GENERIC_STACK_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Stack Stack;

// El usuario provee el tamaño del elemento
Stack *stack_create(size_t capacity, size_t elem_size);
void   stack_destroy(Stack *s);

// Push: copia elem_size bytes desde *elem al stack
bool   stack_push(Stack *s, const void *elem);

// Pop: copia elem_size bytes del tope a *out
bool   stack_pop(Stack *s, void *out);

// Peek: retorna puntero al tope (sin copiar)
const void *stack_peek(const Stack *s);

bool   stack_is_empty(const Stack *s);
size_t stack_size(const Stack *s);

#endif
```

Uso:

```c
// Stack de int
Stack *si = stack_create(100, sizeof(int));
int x = 42;
stack_push(si, &x);
int y;
stack_pop(si, &y);  // y = 42
stack_destroy(si);

// Stack de double
Stack *sd = stack_create(100, sizeof(double));
double d = 3.14;
stack_push(sd, &d);

// Stack de structs
typedef struct { int x, y; } Point;
Stack *sp = stack_create(50, sizeof(Point));
Point p = {10, 20};
stack_push(sp, &p);
```

Problemas con `void *`:

1. **Sin type safety**: puedes hacer push de un `int` y pop como `double` —
   compila sin errores, falla silenciosamente en runtime.

2. **Copias de datos**: `stack_push` copia `elem_size` bytes con `memcpy`.
   Para tipos pequeños (int, double) es eficiente. Para structs grandes,
   puede ser lento.

3. **Ownership ambiguo**: si push de un `char *` (string), el stack copia el
   puntero, no el string. El usuario debe gestionar el lifetime del string.

4. **Sin funciones específicas**: no puedes imprimir el stack sin saber el
   tipo de los elementos.

En Rust, los genéricos (`Stack<T>`) resuelven todos estos problemas: el
compilador genera código especializado para cada tipo, con verificación de
tipos en compile time y sin overhead de `void *`.

</details>

---

### Ejercicio 9 — Stack overflow del call stack

Escribe una función recursiva que cause stack overflow (del call stack, no de
la estructura de datos). Luego, calcula cuántas llamadas recursivas caben
antes del overflow:

```c
void recurse(int depth) {
    int local_array[256];  // 1 KB de variables locales
    local_array[0] = depth;
    printf("depth: %d\n", depth);
    recurse(depth + 1);
}
```

**Prediccion**: Si el call stack tiene 8 MB y cada frame ocupa ~1 KB, a qué
profundidad ocurre el overflow?

<details><summary>Respuesta</summary>

```c
#include <stdio.h>

void recurse(int depth) {
    int local_array[256];  // 256 × 4 = 1024 bytes ≈ 1 KB
    local_array[0] = depth;
    if (depth % 1000 == 0)
        printf("depth: %d\n", depth);
    recurse(depth + 1);
}

int main(void) {
    recurse(0);
    return 0;
}
```

```bash
gcc -o overflow overflow.c && ./overflow
# depth: 0
# depth: 1000
# depth: 2000
# ...
# depth: 7000
# Segmentation fault (stack overflow)
```

Cálculo:
```
Stack size:    8 MB = 8 × 1024 × 1024 = 8,388,608 bytes
Frame size:    ~1 KB (1024 bytes de local_array + ~100 bytes de overhead)
                ≈ 1,124 bytes por frame

Profundidad máxima ≈ 8,388,608 / 1,124 ≈ 7,463 llamadas
```

El resultado real varía porque:
- El overhead del frame incluye: dirección de retorno, saved registers,
  alignment padding
- El stack no empieza completamente vacío (main, libc init)
- El compilador puede optimizar (tail call optimization elimina frames)

Relación con la estructura de datos Stack:
- El call stack **es** una pila — cada llamada es un push, cada retorno es un pop
- Funciones recursivas profundas equivalen a muchos pushes sin pops
- Por eso la recursión profunda puede reemplazarse con un loop + stack explícito
  (la estructura de datos en el heap, que puede crecer mucho más)

```rust
// En vez de recursión profunda:
fn process_recursive(node: *const Node) {
    if node.is_null() { return; }
    unsafe {
        process((*node).data);
        process_recursive((*node).next);  // puede overflow si lista muy larga
    }
}

// Usar stack explícito:
fn process_iterative(head: *const Node) {
    let mut stack = Vec::new();
    stack.push(head);
    while let Some(node) = stack.pop() {
        if node.is_null() { continue; }
        unsafe {
            process((*node).data);
            stack.push((*node).next);
        }
    }
}
```

</details>

Limpieza:

```bash
rm -f overflow
```

---

### Ejercicio 10 — Comparación C vs Rust

Compara las interfaces de stack en C y Rust respondiendo estas preguntas:

```
a) ¿Cómo maneja cada lenguaje el caso de pop en pila vacía?
b) ¿Cómo logra cada lenguaje genericidad (funcionar con cualquier tipo)?
c) ¿Quién es responsable de liberar la memoria?
d) ¿Qué pasa si olvidas llamar destroy/drop?
e) ¿Cuál tiene mejor ergonomía para el usuario del stack?
```

**Prediccion**: Hay algún aspecto donde C sea claramente superior a Rust para
implementar un stack?

<details><summary>Respuesta</summary>

| Aspecto | C | Rust |
|---------|---|------|
| **a) Pop vacía** | Undefined behavior, assert, o retorno de error (`bool` + out param). El programador elige y documenta. | `Option<T>` — `None` si vacía. El compilador fuerza al usuario a manejar el caso. |
| **b) Genericidad** | `void *` + `elem_size` — sin type safety, `memcpy` para copiar, casts manuales. | `<T>` — verificación de tipos en compile time, monomorphización (código especializado por tipo), sin overhead. |
| **c) Liberación** | El usuario debe llamar `stack_destroy()`. Si tiene punteros a datos dinámicos, el usuario debe liberar esos datos antes o después. | `Drop` automático al salir del scope. Si `T` implementa `Drop`, se llama automáticamente para cada elemento. |
| **d) Olvidar destroy** | Memory leak silencioso. Valgrind lo detecta pero el compilador no. | Si usas `Box`/`Vec`: imposible olvidar (Drop automático). Si usas raw pointers: leak, pero Miri lo detecta. |
| **e) Ergonomía** | Verboso: `stack_push(s, &val)`, `stack_pop(s, &out)`, casts de `void *`. | Natural: `s.push(val)`, `s.pop()`, pattern matching para Option. |

**Aspectos donde C es superior:**

1. **Transparencia**: en C ves exactamente qué memoria se usa y cuándo se
   libera. No hay magia del compilador. Esto es una ventaja pedagógica.

2. **Control fino**: puedes elegir exactamente qué allocator usar, cómo
   alinear los datos, y el layout en memoria.

3. **Simplicidad de implementación**: un stack en C son ~30 líneas de código.
   En Rust safe, es similar. Pero en Rust unsafe (con raw pointers), hay
   más ceremonia (`Box::into_raw`, `NonNull`, `// SAFETY:`, `Drop`).

4. **Sin overhead conceptual**: no hay ownership, borrows, lifetimes, traits.
   Un principiante puede entender un stack en C más rápido.

Pero para uso en producción, Rust es superior en casi todos los aspectos
porque los errores de memoria (el problema principal de C) se eliminan o
detectan antes de llegar a producción.

</details>
