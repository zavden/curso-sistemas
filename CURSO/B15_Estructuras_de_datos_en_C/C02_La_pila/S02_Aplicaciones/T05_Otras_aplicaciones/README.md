# T05 — Otras aplicaciones de la pila

## Índice

1. [Navegación back/forward](#1-navegación-backforward)
2. [Undo/Redo](#2-undoredo)
3. [Palíndromos](#3-palíndromos)
4. [Inversión de secuencias](#4-inversión-de-secuencias)
5. [Conversión de bases numéricas](#5-conversión-de-bases-numéricas)
6. [Implementaciones en C y Rust](#6-implementaciones-en-c-y-rust)
7. [Patrón general: cuándo usar una pila](#7-patrón-general-cuándo-usar-una-pila)
8. [Ejercicios](#8-ejercicios)

---

## 1. Navegación back/forward

### El problema

Los navegadores web mantienen un historial de páginas visitadas. El botón
**Back** regresa a la página anterior; el botón **Forward** avanza a la
página que se dejó al retroceder. Si se navega a una página nueva después
de retroceder, el historial Forward se descarta.

### Solución con dos pilas

Se usan dos pilas: `back_stack` y `forward_stack`, más una variable
`current` que indica la página activa:

```
Estado inicial:
  current = A
  back:    ∅
  forward: ∅

Navegar a B:
  push A a back
  current = B
  vaciar forward          ← se descarta historial Forward
  back:    A
  forward: ∅

Navegar a C:
  push B a back
  current = C
  vaciar forward
  back:    A  B
  forward: ∅

Back:
  push C a forward
  current = pop(back) = B
  back:    A
  forward: C

Back:
  push B a forward
  current = pop(back) = A
  back:    ∅
  forward: C  B

Forward:
  push A a back
  current = pop(forward) = B
  back:    A
  forward: C

Navegar a D (después de retroceder):
  push B a back
  current = D
  vaciar forward          ← C se descarta
  back:    A  B
  forward: ∅
```

### Reglas formales

| Acción | Precondición | Efecto |
|--------|:------------:|--------|
| `navigate(url)` | — | push `current` a back, `current = url`, vaciar forward |
| `back()` | back no vacía | push `current` a forward, `current = pop(back)` |
| `forward()` | forward no vacía | push `current` a back, `current = pop(forward)` |

### Diagrama de las operaciones

```
   ┌─────────┐                        ┌─────────┐
   │  BACK   │  ◄──── back() ──────   │ FORWARD │
   │  stack   │                        │  stack   │
   │         │  ───── forward() ───►  │         │
   └─────────┘                        └─────────┘
                    ┌─────────┐
                    │ CURRENT │
                    └─────────┘
                         ▲
                         │
                    navigate(url)
                    (vacía forward)
```

### Por qué la navegación nueva descarta Forward

Si el usuario está en la página B, retrocede a A, y luego navega a D, el
historial forward (C) deja de tener sentido: no hay una línea temporal
"hacia adelante" desde D que pase por C. El usuario bifurcó el historial.
Descartar Forward evita confusión.

---

## 2. Undo/Redo

### El problema

Los editores de texto permiten deshacer (undo) y rehacer (redo) acciones.
El patrón es idéntico al de navegación back/forward, pero en lugar de URLs,
las pilas almacenan **acciones** (o sus inversas).

### Dos enfoques

#### Enfoque 1: almacenar estados completos

Cada acción guarda una copia del estado completo del documento:

```
Pila undo: [estado_0, estado_1, estado_2]
Pila redo: []
```

Undo = restaurar el estado anterior. Simple pero costoso en memoria si el
documento es grande.

#### Enfoque 2: almacenar comandos (patrón Command)

Cada acción se registra como un **comando invertible**: un objeto que sabe
cómo ejecutarse y cómo deshacerse:

```
Comando: Insert { position: 5, text: "hello" }
Inverso: Delete { position: 5, length: 5 }
```

| Comando | Inverso |
|---------|---------|
| Insert(pos, text) | Delete(pos, len) |
| Delete(pos, text) | Insert(pos, text) |
| Replace(pos, old, new) | Replace(pos, new, old) |

El enfoque por comandos usa mucha menos memoria porque solo almacena los
**deltas** (cambios), no el estado completo.

### Estructura

```
Pila undo: [cmd1, cmd2, cmd3]    (comandos ejecutados)
Pila redo: []                     (comandos deshechos)
current state = resultado de aplicar cmd1, cmd2, cmd3

Undo:
  cmd = pop(undo)
  aplicar inverso(cmd) al estado
  push cmd a redo

Redo:
  cmd = pop(redo)
  aplicar cmd al estado
  push cmd a undo

Nueva acción:
  ejecutar cmd
  push cmd a undo
  vaciar redo                     (igual que navigate)
```

### Similitud con navegación

| Aspecto | Navegación | Undo/Redo |
|---------|:----------:|:---------:|
| Pila principal | back (URLs) | undo (comandos) |
| Pila secundaria | forward (URLs) | redo (comandos) |
| Acción nueva | navigate(url) | execute(cmd) |
| Descartar pila 2 | Sí, al navegar | Sí, al ejecutar nuevo |
| Retroceder | back() | undo() |
| Avanzar | forward() | redo() |

El patrón es **exactamente el mismo**. La única diferencia es qué se
almacena en las pilas.

---

## 3. Palíndromos

### El problema

Un **palíndromo** es una secuencia que se lee igual de izquierda a derecha
que de derecha a izquierda. Ejemplos: `"aba"`, `"racecar"`, `"12321"`.

### Verificación con pila

La pila invierte naturalmente el orden de los elementos (LIFO). Para
verificar un palíndromo:

1. Push de todos los caracteres a la pila.
2. Pop uno por uno y comparar con la cadena original de izquierda a derecha.
3. Si todas las comparaciones coinciden, es palíndromo.

```
Cadena: "abcba"

Push todo:  a b c b a  → pila: a b c b a (top)

Pop y comparar:
  pop 'a' == cadena[0] 'a' ✓
  pop 'b' == cadena[1] 'b' ✓
  pop 'c' == cadena[2] 'c' ✓
  pop 'b' == cadena[3] 'b' ✓
  pop 'a' == cadena[4] 'a' ✓
  → palíndromo ✓
```

### Optimización: solo la mitad

Solo se necesita push de la primera mitad y comparar con la segunda mitad:

```
Cadena: "abcba" (longitud 5)

Push primera mitad (posiciones 0..1): a b
Saltar el centro (posición 2): c
Comparar segunda mitad (posiciones 3..4):
  pop 'b' == cadena[3] 'b' ✓
  pop 'a' == cadena[4] 'a' ✓
  → palíndromo ✓
```

Esto reduce el uso de pila a la mitad. Para longitud impar, el carácter
central se salta (no afecta al resultado).

### ¿Se necesita una pila?

Estrictamente, no. Comparar `s[i] == s[n-1-i]` para $i = 0, \ldots, n/2-1$
es más eficiente ($O(1)$ espacio). Pero el método con pila es relevante en
dos contextos:

1. **Flujo de datos sin acceso aleatorio**: si los caracteres llegan uno por
   uno (por ejemplo, de un stream) y no se puede indexar arbitrariamente, la
   pila permite almacenar la primera mitad para compararla con la segunda.
2. **Didáctico**: demuestra la propiedad de inversión de la pila.

---

## 4. Inversión de secuencias

### Principio

Push de $n$ elementos y luego pop de $n$ elementos produce la secuencia
en orden inverso. Esta propiedad fundamental de la pila tiene aplicaciones
directas.

### Invertir una cadena

```c
void reverse_string(char *str) {
    int len = strlen(str);
    CharStack *s = charstack_create(len);

    for (int i = 0; i < len; i++) {
        charstack_push(s, str[i]);
    }
    for (int i = 0; i < len; i++) {
        charstack_pop(s, &str[i]);
    }

    charstack_destroy(s);
}
```

### Invertir una lista enlazada

Invertir una lista enlazada con una pila es una alternativa al método
in-place (que manipula punteros):

```c
/* push all nodes to stack, then pop to build reversed list */
Node *reverse_with_stack(Node *head) {
    /* Phase 1: push all values */
    IntStack *s = intstack_create(1000);
    Node *curr = head;
    while (curr) {
        intstack_push(s, curr->data);
        curr = curr->next;
    }

    /* Phase 2: pop values back into the list */
    curr = head;
    while (curr) {
        intstack_pop(s, &curr->data);
        curr = curr->next;
    }

    intstack_destroy(s);
    return head;
}
```

Este método copia los **valores** (no los nodos), por lo que usa $O(n)$
espacio extra. El método in-place con punteros usa $O(1)$ extra pero es más
propenso a errores.

---

## 5. Conversión de bases numéricas

### El algoritmo

Para convertir un número entero de base 10 a base $b$, se divide
repetidamente entre $b$ y los **restos** son los dígitos de derecha a
izquierda. La pila invierte el orden para obtener el resultado de izquierda
a derecha:

```
Convertir 42 a binario (base 2):

42 / 2 = 21, resto 0  → push 0
21 / 2 = 10, resto 1  → push 1
10 / 2 = 5,  resto 0  → push 0
 5 / 2 = 2,  resto 1  → push 1
 2 / 2 = 1,  resto 0  → push 0
 1 / 2 = 0,  resto 1  → push 1

Pop todo: 1 0 1 0 1 0 → "101010"
```

Verificación: $1 \times 32 + 0 \times 16 + 1 \times 8 + 0 \times 4 + 1 \times 2 + 0 \times 1 = 42$.

### Generalización

El mismo algoritmo funciona para cualquier base. Para bases mayores que 10,
los dígitos mayores que 9 se representan con letras:

```
Convertir 255 a hexadecimal (base 16):

255 / 16 = 15, resto 15 (F)  → push 'F'
 15 / 16 = 0,  resto 15 (F)  → push 'F'

Pop todo: F F → "FF"
```

### Pseudocódigo

```
FUNCIÓN to_base(n, base):
    SI n = 0: RETORNAR "0"
    digits = "0123456789ABCDEF"
    stack ← pila vacía

    MIENTRAS n > 0:
        push(stack, digits[n % base])
        n ← n / base      (división entera)

    resultado ← ""
    MIENTRAS NOT is_empty(stack):
        resultado ← resultado + pop(stack)

    RETORNAR resultado
```

---

## 6. Implementaciones en C y Rust

### Navegación back/forward en C

```c
/* browser.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_URL 256
#define MAX_HISTORY 100

typedef struct {
    char data[MAX_HISTORY][MAX_URL];
    int  top;
} UrlStack;

void urlstack_init(UrlStack *s) { s->top = 0; }

int urlstack_is_empty(const UrlStack *s) { return s->top == 0; }

void urlstack_push(UrlStack *s, const char *url) {
    if (s->top < MAX_HISTORY) {
        strncpy(s->data[s->top], url, MAX_URL - 1);
        s->data[s->top][MAX_URL - 1] = '\0';
        s->top++;
    }
}

const char *urlstack_pop(UrlStack *s) {
    if (s->top == 0) return NULL;
    return s->data[--s->top];
}

void urlstack_clear(UrlStack *s) { s->top = 0; }

/* --- Browser ---------------------------------------------- */

typedef struct {
    char     current[MAX_URL];
    UrlStack back;
    UrlStack forward;
} Browser;

void browser_init(Browser *b, const char *home) {
    strncpy(b->current, home, MAX_URL - 1);
    b->current[MAX_URL - 1] = '\0';
    urlstack_init(&b->back);
    urlstack_init(&b->forward);
}

void browser_navigate(Browser *b, const char *url) {
    urlstack_push(&b->back, b->current);
    strncpy(b->current, url, MAX_URL - 1);
    urlstack_clear(&b->forward);
}

int browser_back(Browser *b) {
    if (urlstack_is_empty(&b->back)) return 0;
    urlstack_push(&b->forward, b->current);
    strncpy(b->current, urlstack_pop(&b->back), MAX_URL - 1);
    return 1;
}

int browser_forward(Browser *b) {
    if (urlstack_is_empty(&b->forward)) return 0;
    urlstack_push(&b->back, b->current);
    strncpy(b->current, urlstack_pop(&b->forward), MAX_URL - 1);
    return 1;
}

void browser_print(const Browser *b) {
    printf("  Current: %s\n", b->current);
}

int main(void) {
    Browser b;
    browser_init(&b, "home.html");

    printf("Start:\n");
    browser_print(&b);

    browser_navigate(&b, "page_a.html");
    printf("Navigate to A:\n");
    browser_print(&b);

    browser_navigate(&b, "page_b.html");
    printf("Navigate to B:\n");
    browser_print(&b);

    browser_navigate(&b, "page_c.html");
    printf("Navigate to C:\n");
    browser_print(&b);

    browser_back(&b);
    printf("Back:\n");
    browser_print(&b);

    browser_back(&b);
    printf("Back:\n");
    browser_print(&b);

    browser_forward(&b);
    printf("Forward:\n");
    browser_print(&b);

    browser_navigate(&b, "page_d.html");
    printf("Navigate to D (forward cleared):\n");
    browser_print(&b);

    /* forward should fail now */
    if (!browser_forward(&b)) {
        printf("Forward: nothing to go forward to\n");
    }

    return 0;
}
```

Salida esperada:

```
Start:
  Current: home.html
Navigate to A:
  Current: page_a.html
Navigate to B:
  Current: page_b.html
Navigate to C:
  Current: page_c.html
Back:
  Current: page_b.html
Back:
  Current: page_a.html
Forward:
  Current: page_b.html
Navigate to D (forward cleared):
  Current: page_d.html
Forward: nothing to go forward to
```

### Navegación back/forward en Rust

```rust
// browser.rs

struct Browser {
    current: String,
    back:    Vec<String>,
    forward: Vec<String>,
}

impl Browser {
    fn new(home: &str) -> Self {
        Browser {
            current: home.to_string(),
            back:    Vec::new(),
            forward: Vec::new(),
        }
    }

    fn navigate(&mut self, url: &str) {
        self.back.push(std::mem::take(&mut self.current));
        self.current = url.to_string();
        self.forward.clear();
    }

    fn back(&mut self) -> bool {
        match self.back.pop() {
            None => false,
            Some(prev) => {
                self.forward.push(std::mem::take(&mut self.current));
                self.current = prev;
                true
            }
        }
    }

    fn forward(&mut self) -> bool {
        match self.forward.pop() {
            None => false,
            Some(next) => {
                self.back.push(std::mem::take(&mut self.current));
                self.current = next;
                true
            }
        }
    }
}

fn main() {
    let mut b = Browser::new("home.html");
    println!("Start: {}", b.current);

    b.navigate("page_a.html");
    println!("Navigate to A: {}", b.current);

    b.navigate("page_b.html");
    println!("Navigate to B: {}", b.current);

    b.navigate("page_c.html");
    println!("Navigate to C: {}", b.current);

    b.back();
    println!("Back: {}", b.current);

    b.back();
    println!("Back: {}", b.current);

    b.forward();
    println!("Forward: {}", b.current);

    b.navigate("page_d.html");
    println!("Navigate to D: {}", b.current);

    if !b.forward() {
        println!("Forward: nothing to go forward to");
    }
}
```

### `std::mem::take` en Rust

La línea `std::mem::take(&mut self.current)` reemplaza `self.current` con
un `String` vacío y retorna el valor anterior. Esto evita clonar: el
`String` se **mueve** a la pila sin copiar datos del heap.

Sin `take`, habría que hacer `self.current.clone()` (copia del buffer) y
luego asignar el nuevo valor. Con `take`, el costo es $O(1)$ en lugar de
$O(n)$ donde $n$ es la longitud del URL.

### Comparación C vs Rust

| Aspecto | C | Rust |
|---------|---|------|
| Almacenamiento | Arrays fijos `char[256]` | `Vec<String>` dinámico |
| Copia de URL | `strncpy` (copia bytes) | `take` (mueve ownership, $O(1)$) |
| Límite de historial | `MAX_HISTORY` fijo | Sin límite (crece con heap) |
| Limpieza | No necesaria (todo en stack) | Automática (Drop) |
| Buffer overflow | Posible si URL > 255 | Imposible (String crece) |

---

## 7. Patrón general: cuándo usar una pila

Después de ver múltiples aplicaciones a lo largo de S02, emerge un patrón:

### Señales de que el problema necesita una pila

1. **Anidamiento**: el problema tiene estructuras que se abren y cierran
   (paréntesis, etiquetas, llamadas a función).
2. **Inversión**: necesitas procesar elementos en orden inverso al que
   llegaron.
3. **Lo último primero (LIFO)**: el elemento más reciente es el que debe
   procesarse primero (undo, back).
4. **Recursión simulada**: se necesita rastrear "dónde estaba" al volver
   de un sub-problema.
5. **Evaluación diferida**: un operador debe esperar hasta que sus operandos
   estén completos (Shunting-Yard).

### Mapa de aplicaciones

| Aplicación | Señal LIFO | Qué se apila |
|------------|:----------:|--------------|
| Paréntesis balanceados | Anidamiento | Delimitadores de apertura |
| Evaluación RPN | Evaluación diferida | Operandos intermedios |
| Shunting-Yard | Evaluación diferida | Operadores en espera |
| Navegación back/forward | Lo último primero | URLs visitadas |
| Undo/Redo | Lo último primero | Comandos ejecutados |
| Palíndromos | Inversión | Caracteres |
| Conversión de base | Inversión | Dígitos (orden inverso) |
| Call stack | Recursión | Frames de retorno |
| DFS (recorrido en profundidad) | Recursión | Nodos por visitar |

### Cuándo NO usar una pila

- **Orden FIFO** (primero en llegar, primero en salir): usar una **cola**.
- **Acceso por clave**: usar un **mapa/tabla hash**.
- **Acceso por posición**: usar un **array**.
- **Prioridad**: usar una **cola de prioridad/heap**.

---

## 8. Ejercicios

---

### Ejercicio 1 — Traza de navegación

Dada la secuencia de acciones, escribe el estado de `current`, `back` y
`forward` después de cada una:

```
1. browser_init("home")
2. navigate("A")
3. navigate("B")
4. navigate("C")
5. back()
6. back()
7. navigate("D")
8. back()
9. forward()
```

<details>
<summary>¿Qué contiene la pila forward después del paso 7?</summary>

```
Paso    Acción           current    back (base→top)    forward (base→top)
──────────────────────────────────────────────────────────────────────────
1       init("home")     home       ∅                  ∅
2       navigate("A")    A          home               ∅
3       navigate("B")    B          home A              ∅
4       navigate("C")    C          home A B            ∅
5       back()           B          home A              C
6       back()           A          home                C B
7       navigate("D")    D          home A              ∅  ← vaciada
8       back()           A          home                D
9       forward()        D          home A              ∅
```

Después del paso 7, la pila forward está **vacía**. Al navegar a D desde A,
se descartó el historial forward (C y B). Esto demuestra que la bifurcación
del historial elimina la línea temporal anterior.

</details>

---

### Ejercicio 2 — Undo/Redo con comandos

Diseña un editor de texto mínimo que soporte:
- `insert(pos, text)`: inserta texto en la posición dada
- `delete(pos, len)`: elimina `len` caracteres desde la posición
- `undo()`: deshace la última acción
- `redo()`: rehace la última acción deshecha

Aplica esta secuencia al texto vacío `""` y muestra el estado después de
cada paso:

```
1. insert(0, "hello")      → ?
2. insert(5, " world")     → ?
3. undo()                   → ?
4. undo()                   → ?
5. redo()                   → ?
6. insert(5, "!")           → ?
7. redo()                   → ?
```

<details>
<summary>¿El paso 7 tiene efecto? ¿Por qué?</summary>

```
Paso    Acción                 Texto           Undo stack        Redo stack
──────────────────────────────────────────────────────────────────────────────
1       insert(0, "hello")     "hello"         [ins(0,"hello")]  ∅
2       insert(5, " world")   "hello world"   [ins(0,"hello"),   ∅
                                                ins(5," world")]
3       undo()                 "hello"         [ins(0,"hello")]  [ins(5," world")]
4       undo()                 ""              ∅                 [ins(5," world"),
                                                                  ins(0,"hello")]
5       redo()                 "hello"         [ins(0,"hello")]  [ins(5," world")]
6       insert(5, "!")         "hello!"        [ins(0,"hello"),   ∅  ← vaciada
                                                ins(5,"!")]
7       redo()                 "hello!"        sin cambio         ∅ (vacía)
```

El paso 7 **no tiene efecto**: la pila redo está vacía porque el paso 6
(nueva acción) la vació. Esto es análogo a navegar a una nueva página
después de retroceder — el historial forward se descarta.

</details>

---

### Ejercicio 3 — Palíndromo con pila

Implementa en C una función `is_palindrome(const char *s)` que use una pila
de caracteres. Ignora espacios y no distingue mayúsculas de minúsculas.
Prueba con: `"racecar"`, `"A man a plan a canal Panama"`, `"hello"`.

<details>
<summary>¿Cómo manejas los espacios y mayúsculas?</summary>

Normalizar antes de comparar:

```c
#include <ctype.h>

int is_palindrome(const char *s) {
    /* Phase 1: build cleaned version (lowercase, no spaces) */
    int len = strlen(s);
    char *clean = malloc(len + 1);
    int clean_len = 0;
    for (int i = 0; i < len; i++) {
        if (!isspace(s[i])) {
            clean[clean_len++] = tolower(s[i]);
        }
    }
    clean[clean_len] = '\0';

    /* Phase 2: push first half */
    CharStack *stack = charstack_create(clean_len);
    int half = clean_len / 2;
    for (int i = 0; i < half; i++) {
        charstack_push(stack, clean[i]);
    }

    /* Phase 3: compare second half */
    int start = (clean_len % 2 == 0) ? half : half + 1;
    int result = 1;
    for (int i = start; i < clean_len; i++) {
        char c;
        charstack_pop(stack, &c);
        if (c != clean[i]) {
            result = 0;
            break;
        }
    }

    charstack_destroy(stack);
    free(clean);
    return result;
}
```

Para `"A man a plan a canal Panama"`:
- Limpio: `"amanaplanacanalpanama"` (21 caracteres)
- Push primera mitad (0..9): `"amanaplana"`
- Saltar centro (posición 10): `"c"`
- Comparar segunda mitad (11..20): pop y comparar cada uno
- Resultado: palíndromo ✓

</details>

---

### Ejercicio 4 — Conversión de base en Rust

Implementa `to_base(n: u64, base: u64) -> String` en Rust usando una pila
(`Vec<char>`). Prueba:

```
to_base(42, 2)   → "101010"
to_base(255, 16) → "FF"
to_base(100, 8)  → "144"
to_base(0, 2)    → "0"
```

<details>
<summary>¿Cómo manejas el caso $n = 0$ y los dígitos mayores que 9?</summary>

```rust
fn to_base(mut n: u64, base: u64) -> String {
    if n == 0 {
        return "0".to_string();
    }

    let digits: Vec<char> = "0123456789ABCDEF".chars().collect();
    let mut stack: Vec<char> = Vec::new();

    while n > 0 {
        stack.push(digits[(n % base) as usize]);
        n /= base;
    }

    // pop all (reverse) to build result
    stack.iter().rev().collect()
}
```

El caso $n = 0$ se maneja por separado porque el bucle `while n > 0` no
entraría y produciría una cadena vacía.

Los dígitos mayores que 9 se mapean con el array `digits`: índice 10 → `'A'`,
11 → `'B'`, etc. El `as usize` convierte el resto a índice del array.

`stack.iter().rev().collect()` itera la pila en orden inverso (equivalente
a pop repetido) y construye un `String`. Esto es más idiomático en Rust
que hacer pop en un bucle.

</details>

---

### Ejercicio 5 — Inversión de palabras

Escribe una función en C que invierta el **orden de las palabras** en una
frase (no los caracteres). Ejemplo: `"hello world foo"` → `"foo world hello"`.
Usa una pila de strings.

<details>
<summary>¿Qué estructura de pila necesitas para almacenar palabras?</summary>

Una pila de punteros a `char *`, donde cada puntero apunta al inicio de
una palabra en el buffer:

```c
void reverse_words(char *str) {
    /* Use strtok to split and a stack of char* to reverse */
    char *words[100];
    int count = 0;

    char *copy = strdup(str);
    char *token = strtok(copy, " ");
    while (token != NULL) {
        words[count++] = token;  /* push */
        token = strtok(NULL, " ");
    }

    /* Rebuild string by popping (reverse order) */
    str[0] = '\0';
    for (int i = count - 1; i >= 0; i--) {  /* pop */
        if (i < count - 1) strcat(str, " ");
        strcat(str, words[i]);
    }

    free(copy);
}
```

El array `words[]` actúa como pila (push con `count++`, pop recorriendo
de `count-1` a 0). Se necesita `strdup` porque `strtok` modifica la cadena,
y `strcat` reconstruye el resultado en el buffer original.

Nota: esta implementación asume que `str` tiene suficiente espacio y que
hay como máximo 100 palabras. Una versión robusta usaría memoria dinámica.

</details>

---

### Ejercicio 6 — Browser con límite de historial

Modifica la implementación del navegador en C para que las pilas back y
forward tengan un **límite máximo** de 10 entradas. Si se excede el límite
al hacer push, se descarta la entrada más antigua (la del fondo de la pila).

<details>
<summary>¿Cómo descartas el fondo de una pila (que es un array)?</summary>

Hay dos enfoques:

**Enfoque 1: Shift** — Mover todos los elementos una posición hacia abajo y
push en el tope:

```c
void urlstack_push_limited(UrlStack *s, const char *url) {
    if (s->top >= MAX_HISTORY) {
        /* shift everything down, discarding bottom */
        memmove(&s->data[0], &s->data[1],
                (MAX_HISTORY - 1) * MAX_URL);
        s->top = MAX_HISTORY - 1;
    }
    strncpy(s->data[s->top], url, MAX_URL - 1);
    s->data[s->top][MAX_URL - 1] = '\0';
    s->top++;
}
```

`memmove` es $O(n)$ pero para $n = 10$ es despreciable.

**Enfoque 2: Buffer circular** — Usar un índice `base` que avanza cuando
se desborda:

```c
typedef struct {
    char data[MAX_HISTORY][MAX_URL];
    int  base;   /* index of bottom element */
    int  count;  /* number of elements */
} CircularUrlStack;
```

El buffer circular es $O(1)$ para todas las operaciones pero más complejo
de implementar. Para un límite de 10, el shift es perfectamente adecuado.

</details>

---

### Ejercicio 7 — Undo/Redo genérico en Rust

Implementa una estructura `UndoStack<T>` genérica que almacene estados
de tipo `T` (enfoque 1: estados completos). Requiere `T: Clone`.

```rust
struct UndoStack<T> { ... }
impl<T: Clone> UndoStack<T> {
    fn new(initial: T) -> Self;
    fn apply(&mut self, new_state: T);
    fn undo(&mut self) -> Option<&T>;     // returns current after undo
    fn redo(&mut self) -> Option<&T>;     // returns current after redo
    fn current(&self) -> &T;
}
```

<details>
<summary>¿Cómo implementas `undo` sin clonar el estado actual?</summary>

```rust
struct UndoStack<T> {
    current: T,
    undo_stack: Vec<T>,
    redo_stack: Vec<T>,
}

impl<T: Clone> UndoStack<T> {
    fn new(initial: T) -> Self {
        UndoStack {
            current: initial,
            undo_stack: Vec::new(),
            redo_stack: Vec::new(),
        }
    }

    fn apply(&mut self, new_state: T) {
        let old = std::mem::replace(&mut self.current, new_state);
        self.undo_stack.push(old);
        self.redo_stack.clear();
    }

    fn undo(&mut self) -> Option<&T> {
        let prev = self.undo_stack.pop()?;
        let current = std::mem::replace(&mut self.current, prev);
        self.redo_stack.push(current);
        Some(&self.current)
    }

    fn redo(&mut self) -> Option<&T> {
        let next = self.redo_stack.pop()?;
        let current = std::mem::replace(&mut self.current, next);
        self.undo_stack.push(current);
        Some(&self.current)
    }

    fn current(&self) -> &T {
        &self.current
    }
}
```

`std::mem::replace` intercambia `self.current` con el nuevo valor y retorna
el anterior, **sin clonar**. Solo se mueven los valores. Para tipos como
`String` o `Vec`, esto es $O(1)$ (move de 3 punteros internos, no copia
del contenido).

Uso:

```rust
let mut editor = UndoStack::new(String::new());
editor.apply("hello".to_string());
editor.apply("hello world".to_string());
assert_eq!(editor.current(), "hello world");
editor.undo();
assert_eq!(editor.current(), "hello");
editor.redo();
assert_eq!(editor.current(), "hello world");
```

</details>

---

### Ejercicio 8 — Verificar secuencia de push/pop

Dados dos arrays — uno con el orden de push y otro con el supuesto orden
de pop — determina si la secuencia de pop es posible.

Ejemplo: push `[1, 2, 3, 4, 5]`, pop `[4, 5, 3, 2, 1]` → **sí** (push
1,2,3,4, pop 4, push 5, pop 5,3,2,1).

Ejemplo: push `[1, 2, 3, 4, 5]`, pop `[4, 3, 5, 1, 2]` → **no** (cuando
se necesita pop 1, el 2 está encima).

<details>
<summary>¿Cuál es el algoritmo para verificar esto?</summary>

Simular las operaciones con una pila real:

```rust
fn is_valid_pop_sequence(push_order: &[i32], pop_order: &[i32]) -> bool {
    let mut stack: Vec<i32> = Vec::new();
    let mut push_idx = 0;

    for &target in pop_order {
        // push elements until we find the target
        while stack.last() != Some(&target) {
            if push_idx >= push_order.len() {
                return false;  // nothing left to push
            }
            stack.push(push_order[push_idx]);
            push_idx += 1;
        }
        // target is on top, pop it
        stack.pop();
    }

    stack.is_empty()
}
```

Para `push=[1,2,3,4,5]`, `pop=[4,5,3,2,1]`:
- target=4: push 1,2,3,4 → pop 4 ✓
- target=5: push 5 → pop 5 ✓
- target=3: ya en top → pop 3 ✓
- target=2: ya en top → pop 2 ✓
- target=1: ya en top → pop 1 ✓
- Pila vacía → válido ✓

Para `push=[1,2,3,4,5]`, `pop=[4,3,5,1,2]`:
- target=4: push 1,2,3,4 → pop 4 ✓
- target=3: ya en top → pop 3 ✓
- target=5: push 5 → pop 5 ✓
- target=1: top es 2 ≠ 1, nada más que push → no se puede ✗

La complejidad es $O(n)$: cada elemento se empuja y saca a lo sumo una vez.

</details>

---

### Ejercicio 9 — DFS con pila explícita

El recorrido en profundidad (DFS) de un grafo se implementa naturalmente
con recursión. Reimpleméntalo con una **pila explícita** en Rust para un
grafo representado como lista de adyacencia:

```rust
type Graph = Vec<Vec<usize>>;  // graph[node] = [neighbors]
fn dfs(graph: &Graph, start: usize) -> Vec<usize>;
```

<details>
<summary>¿El orden de visita es idéntico al de la versión recursiva?</summary>

```rust
fn dfs(graph: &Graph, start: usize) -> Vec<usize> {
    let mut visited = vec![false; graph.len()];
    let mut order = Vec::new();
    let mut stack = vec![start];

    while let Some(node) = stack.pop() {
        if visited[node] { continue; }
        visited[node] = true;
        order.push(node);

        // push neighbors in reverse to visit in original order
        for &neighbor in graph[node].iter().rev() {
            if !visited[neighbor] {
                stack.push(neighbor);
            }
        }
    }

    order
}
```

El orden **no es idéntico** al de la versión recursiva a menos que se
empujen los vecinos en orden inverso (`.rev()`). Esto se debe a que la
pila invierte el orden: si se empuja `[A, B, C]`, se saca `C` primero.
Con `.rev()` se empuja `[C, B, A]` y se saca `A` primero, igualando al
recursivo.

La pila explícita puede consumir más memoria que la recursión cuando
nodos se empujan múltiples veces (antes de ser visitados). Un nodo puede
estar en la pila varias veces, a diferencia de la recursión donde la
marca `visited` se pone antes de la llamada recursiva. Para grafos densos,
esto puede ser significativo.

</details>

---

### Ejercicio 10 — Mapa de aplicaciones

Completa la siguiente tabla identificando qué estructura de datos (pila,
cola, o ninguna) es más adecuada para cada problema:

```
a)  Cola de impresión (primer documento enviado = primer documento impreso)
b)  Ctrl+Z en un editor
c)  Verificar que <html>...</html> está bien anidado
d)  BFS (recorrido en anchura) de un grafo
e)  Convertir 42 a binario
f)  Atender clientes en orden de llegada
g)  Deshacer la última jugada en ajedrez
h)  Evaluar "3 4 + 2 *"
```

<details>
<summary>¿Cuáles son pila, cuáles cola, y cuáles podrían ser ambas?</summary>

```
a)  Cola de impresión               → COLA (FIFO: primero enviado, primero impreso)
b)  Ctrl+Z en un editor             → PILA (LIFO: última acción, primera en deshacerse)
c)  Verificar anidamiento HTML      → PILA (anidamiento = LIFO)
d)  BFS de un grafo                 → COLA (explora por niveles, FIFO)
e)  Convertir 42 a binario          → PILA (inversión de orden de dígitos)
f)  Atender clientes en orden       → COLA (FIFO: orden de llegada)
g)  Deshacer jugada de ajedrez      → PILA (LIFO: última jugada)
h)  Evaluar "3 4 + 2 *"            → PILA (evaluación RPN)
```

Ninguno requiere ambas. La distinción clave es:
- ¿El **primero** en llegar es el primero en procesarse? → COLA
- ¿El **último** en llegar es el primero en procesarse? → PILA

BFS y DFS son el ejemplo canónico de la diferencia: mismo algoritmo,
distinta estructura de datos, resultados completamente diferentes.

</details>
