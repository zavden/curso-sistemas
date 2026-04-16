# Problema de Josephus

## Formulación

$n$ personas están en un círculo, numeradas de $1$ a $n$.  Empezando por la
persona 1, se cuenta hasta $k$.  La persona en la posición $k$ es eliminada.
El conteo reinicia desde la siguiente persona.  El proceso se repite hasta que
queda una sola persona — el **sobreviviente**.

```
n=5, k=3:

Ronda 1:  1  2 [3]  4  5     → eliminar 3, contar desde 4
Ronda 2:  1  2  ·   4  5
          4  5  1  [2]        → eliminar 2, contar desde 4 (saltando el hueco de 3)

Pero con lista circular no hay "huecos" — los eliminados desaparecen:

Ronda 1:  (1)→2→3→4→5→(1)    contar 3 desde 1: 1,2,[3]  → eliminar 3
Ronda 2:  (4)→5→1→2→(4)      contar 3 desde 4: 4,5,[1]  → eliminar 1
Ronda 3:  (2)→4→5→(2)        contar 3 desde 2: 2,4,[5]  → eliminar 5
Ronda 4:  (2)→4→(2)          contar 3 desde 2: 2,4,[2]  → eliminar 2

Sobreviviente: 4
```

El problema lleva el nombre de Flavio Josefo, historiador romano-judío del
siglo I, que según su relato sobrevivió a un pacto suicida colectivo durante
el sitio de Yotapata eligiendo la posición correcta en el círculo.

---

## Por qué lista circular

La lista circular es la estructura natural para este problema:

1. Las personas forman un **anillo** — la lista circular lo representa
   directamente.
2. Eliminar una persona es **eliminar un nodo** — $O(1)$ si tenemos el
   puntero al nodo anterior.
3. El conteo continúa cíclicamente — no hay extremos, no hay `NULL`,
   no hay reset.

Alternativas y sus problemas:

| Estructura | Problema |
|-----------|----------|
| Array | Eliminar requiere desplazar o marcar como "muerto" + saltar |
| Lista simple | Al llegar al final hay que volver al inicio manualmente |
| Matemáticas puras | Fórmula recursiva existe pero no muestra el orden de eliminación |

---

## Implementación en C

### Con lista circular

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int id;
    struct Node *next;
} Node;

// Construir círculo de n personas: 1, 2, ..., n
Node *build_circle(int n) {
    Node *head = malloc(sizeof(Node));
    head->id = 1;
    Node *prev = head;

    for (int i = 2; i <= n; i++) {
        Node *node = malloc(sizeof(Node));
        node->id = i;
        prev->next = node;
        prev = node;
    }
    prev->next = head;   // cerrar el círculo
    return head;
}

int josephus(int n, int k) {
    Node *circle = build_circle(n);

    // Posicionar prev en el nodo ANTERIOR al head
    // (necesario para eliminar: prev->next = target->next)
    Node *prev = circle;
    while (prev->next != circle) {
        prev = prev->next;
    }
    // prev es el último nodo (n), prev->next es el primero (1)

    Node *cur = circle;

    printf("Order of elimination: ");
    while (cur->next != cur) {   // mientras quede más de 1
        // Avanzar k-1 pasos (cur queda en el nodo a eliminar)
        for (int i = 1; i < k; i++) {
            prev = cur;
            cur = cur->next;
        }

        // Eliminar cur
        printf("%d ", cur->id);
        prev->next = cur->next;
        Node *to_free = cur;
        cur = cur->next;        // siguiente persona después del eliminado
        free(to_free);
    }

    int survivor = cur->id;
    printf("\nSurvivor: %d\n", survivor);
    free(cur);
    return survivor;
}

int main(void) {
    josephus(5, 3);    // Elimina: 3 1 5 2, Sobreviviente: 4
    josephus(7, 2);    // Elimina: 2 4 6 1 5 3, Sobreviviente: 7
    josephus(10, 3);   // Sobreviviente: 4
    return 0;
}
```

### Traza detallada: n=5, k=3

```
Círculo inicial: 1→2→3→4→5→(1)
prev = 5 (último), cur = 1 (primero)

Ronda 1: contar 3 desde 1
  i=1: prev=1, cur=2
  i=2: prev=2, cur=3
  Eliminar 3: prev(2)->next = cur->next(4), free(3), cur=4
  Círculo: 1→2→4→5→(1)

Ronda 2: contar 3 desde 4
  i=1: prev=4, cur=5
  i=2: prev=5, cur=1
  Eliminar 1: prev(5)->next = cur->next(2), free(1), cur=2
  Círculo: 2→4→5→(2)

Ronda 3: contar 3 desde 2
  i=1: prev=2, cur=4
  i=2: prev=4, cur=5
  Eliminar 5: prev(4)->next = cur->next(2), free(5), cur=2
  Círculo: 2→4→(2)

Ronda 4: contar 3 desde 2
  i=1: prev=2, cur=4
  i=2: prev=4, cur=2
  Eliminar 2: prev(4)->next = cur->next(4), free(2), cur=4
  Círculo: 4→(4)   (auto-referencia)

cur->next == cur → salir del while
Sobreviviente: 4
```

### Complejidad

- Construir el círculo: $O(n)$.
- Cada eliminación: $O(k)$ pasos para contar.
- Total de eliminaciones: $n - 1$.
- **Total: $O(nk)$**.

Para $k$ constante, es $O(n)$.  Para $k = n$, es $O(n^2)$.

---

## Implementación en Rust

```rust
use std::ptr;

struct Node {
    id: usize,
    next: *mut Node,
}

fn build_circle(n: usize) -> *mut Node {
    let head = Box::into_raw(Box::new(Node {
        id: 1,
        next: ptr::null_mut(),
    }));

    let mut prev = head;
    for i in 2..=n {
        let node = Box::into_raw(Box::new(Node {
            id: i,
            next: ptr::null_mut(),
        }));
        unsafe { (*prev).next = node; }
        prev = node;
    }
    unsafe { (*prev).next = head; }   // cerrar círculo
    head
}

fn josephus(n: usize, k: usize) -> usize {
    let circle = build_circle(n);

    unsafe {
        // Encontrar el nodo anterior al head
        let mut prev = circle;
        while (*prev).next != circle {
            prev = (*prev).next;
        }

        let mut cur = circle;

        print!("Elimination order (n={n}, k={k}): ");
        while (*cur).next != cur {
            // Avanzar k-1 pasos
            for _ in 1..k {
                prev = cur;
                cur = (*cur).next;
            }

            // Eliminar cur
            print!("{} ", (*cur).id);
            (*prev).next = (*cur).next;
            let next = (*cur).next;
            let _ = Box::from_raw(cur);
            cur = next;
        }

        let survivor = (*cur).id;
        println!("\nSurvivor: {survivor}");
        let _ = Box::from_raw(cur);   // liberar el último
        survivor
    }
}

fn main() {
    assert_eq!(josephus(5, 3), 4);
    assert_eq!(josephus(7, 2), 7);
    assert_eq!(josephus(10, 3), 4);
}
```

---

## Solución matemática

Existe una fórmula recursiva que calcula la posición del sobreviviente sin
simular la eliminación:

$$J(1, k) = 0$$

$$J(n, k) = (J(n-1, k) + k) \bmod n$$

El resultado usa indexación desde 0.  Para obtener la persona (indexada
desde 1): $J(n,k) + 1$.

### Derivación intuitiva

Después de eliminar a la persona en posición $k - 1$ (0-indexed), quedan
$n - 1$ personas.  El subproblema es Josephus con $n - 1$ personas, pero
el conteo **reinicia** desde la posición $k$ (la persona después del
eliminado).

La posición del sobreviviente en el subproblema de $n-1$ es $J(n-1, k)$.  Para
traducirla al problema original, desplazamos $k$ posiciones y tomamos módulo
$n$: $(J(n-1, k) + k) \bmod n$.

### Implementación recursiva

```c
int josephus_math(int n, int k) {
    if (n == 1) return 0;
    return (josephus_math(n - 1, k) + k) % n;
}

// Uso: persona = josephus_math(n, k) + 1
```

### Implementación iterativa — $O(n)$

```c
int josephus_iter(int n, int k) {
    int pos = 0;   // J(1, k) = 0
    for (int i = 2; i <= n; i++) {
        pos = (pos + k) % i;
    }
    return pos + 1;   // convertir a 1-indexed
}
```

$O(n)$ tiempo, $O(1)$ espacio — muy superior a la simulación $O(nk)$ cuando
solo necesitamos la posición final.

### Verificación

| $n$ | $k$ | Simulación | Fórmula $(J(n,k) + 1)$ |
|-----|-----|-----------|----------------------|
| 5 | 3 | 4 | $(0+3)\%2=1, (1+3)\%3=1, (1+3)\%4=0, (0+3)\%5=3 → 3+1=4$ ✓ |
| 7 | 2 | 7 | Iterando: $0,1,0,1,0,3,6 → 6+1=7$ ✓ |
| 10 | 3 | 4 | Iterando: resultado $3 → 3+1=4$ ✓ |

### En Rust

```rust
fn josephus_math(n: usize, k: usize) -> usize {
    let mut pos = 0;
    for i in 2..=n {
        pos = (pos + k) % i;
    }
    pos + 1
}
```

---

## Caso especial: k = 2

Cuando $k = 2$, existe una fórmula cerrada basada en la representación
binaria de $n$:

$$J(n, 2) = 2L + 1$$

donde $n = 2^m + L$ con $0 \leq L < 2^m$.

Equivalentemente: tomar la representación binaria de $n$, mover el bit más
significativo al final.

```
n = 5  = 101₂  →  011₂ = 3  →  persona 3
n = 7  = 111₂  →  111₂ = 7  →  persona 7
n = 10 = 1010₂ →  0101₂ = 5 →  persona 5
n = 12 = 1100₂ →  1001₂ = 9 →  persona 9
```

```c
int josephus_k2(int n) {
    // Encontrar la potencia de 2 más alta <= n
    int p = 1;
    while (p * 2 <= n) p *= 2;
    // n = p + L, resultado = 2*L + 1
    return 2 * (n - p) + 1;
}
```

```rust
fn josephus_k2(n: usize) -> usize {
    let p = n.next_power_of_two() >> 1;  // mayor potencia de 2 <= n
    2 * (n - p) + 1
}
```

---

## Variantes del problema

### Contar en sentido contrario

Alternar el sentido del conteo en cada ronda.  La lista circular simple
no permite retroceder — se necesita lista **circular doble** (tema de T03).

### Empezar desde una posición arbitraria

```c
int josephus_start(int n, int k, int start) {
    // Avanzar start-1 posiciones antes de empezar
    Node *circle = build_circle(n);
    Node *prev = circle;
    while (prev->next != circle) prev = prev->next;

    Node *cur = circle;
    for (int i = 1; i < start; i++) {
        prev = cur;
        cur = cur->next;
    }
    // Continuar con la simulación normal desde cur
    // ...
}
```

### Cada persona tiene un paso diferente

En vez de $k$ fijo, la persona eliminada determina el paso siguiente (ej.
su número es el nuevo $k$).  No tiene fórmula cerrada — solo simulación.

### Encontrar la posición segura

El problema original de Josefo: dado $n$ y $k$, ¿en qué posición sentarse
para sobrevivir?  Es exactamente lo que calcula la fórmula.

---

## Simulación vs fórmula

| Aspecto | Simulación (lista circular) | Fórmula iterativa |
|---------|---------------------------|-------------------|
| Tiempo | $O(nk)$ | $O(n)$ |
| Espacio | $O(n)$ (nodos) | $O(1)$ |
| Orden de eliminación | Sí — se puede imprimir | No — solo posición final |
| Variantes (paso variable, etc.) | Soporta todas | Solo $k$ fijo |
| Valor pedagógico | Alto (listas circulares) | Alto (recurrencia + módulo) |

La simulación es preferible cuando necesitas el **orden de eliminación**.
La fórmula es preferible cuando solo necesitas el **sobreviviente** y $n$ es
grande.

---

## Programa completo

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int id;
    struct Node *next;
} Node;

Node *build_circle(int n) {
    Node *head = malloc(sizeof(Node));
    head->id = 1;
    Node *prev = head;
    for (int i = 2; i <= n; i++) {
        Node *node = malloc(sizeof(Node));
        node->id = i;
        prev->next = node;
        prev = node;
    }
    prev->next = head;
    return head;
}

int josephus_simulate(int n, int k) {
    Node *circle = build_circle(n);
    Node *prev = circle;
    while (prev->next != circle) prev = prev->next;
    Node *cur = circle;

    printf("  Elimination: ");
    while (cur->next != cur) {
        for (int i = 1; i < k; i++) {
            prev = cur;
            cur = cur->next;
        }
        printf("%d ", cur->id);
        prev->next = cur->next;
        Node *tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    int survivor = cur->id;
    printf("→ Survivor: %d\n", survivor);
    free(cur);
    return survivor;
}

int josephus_formula(int n, int k) {
    int pos = 0;
    for (int i = 2; i <= n; i++) {
        pos = (pos + k) % i;
    }
    return pos + 1;
}

int josephus_k2(int n) {
    int p = 1;
    while (p * 2 <= n) p *= 2;
    return 2 * (n - p) + 1;
}

int main(void) {
    int tests[][2] = {{5,3}, {7,2}, {10,3}, {6,4}, {41,3}};
    int num_tests = sizeof(tests) / sizeof(tests[0]);

    for (int t = 0; t < num_tests; t++) {
        int n = tests[t][0], k = tests[t][1];
        printf("J(%d, %d):\n", n, k);

        int sim = josephus_simulate(n, k);
        int formula = josephus_formula(n, k);
        printf("  Formula: %d  %s\n\n",
               formula, sim == formula ? "✓" : "MISMATCH");
    }

    // Caso especial k=2
    printf("k=2 formula:\n");
    for (int n = 1; n <= 16; n++) {
        printf("  J(%2d, 2) = %2d (formula: %2d)\n",
               n, josephus_formula(n, 2), josephus_k2(n));
    }

    return 0;
}
```

Salida (parcial):

```
J(5, 3):
  Elimination: 3 1 5 2 → Survivor: 4
  Formula: 4  ✓

J(7, 2):
  Elimination: 2 4 6 1 5 3 → Survivor: 7
  Formula: 7  ✓

J(10, 3):
  Elimination: 3 6 9 2 7 1 8 5 10 → Survivor: 4
  Formula: 4  ✓
```

---

## Ejercicios

### Ejercicio 1 — Traza manual

Resuelve Josephus con $n=6$, $k=2$ a mano.  Dibuja el círculo y marca
cada eliminación.  ¿Quién sobrevive?

<details>
<summary>Traza</summary>

```
Inicio: 1→2→3→4→5→6→(1)

Ronda 1: contar 2 desde 1: 1,[2] → eliminar 2
  1→3→4→5→6→(1)

Ronda 2: contar 2 desde 3: 3,[4] → eliminar 4
  1→3→5→6→(1)

Ronda 3: contar 2 desde 5: 5,[6] → eliminar 6
  1→3→5→(1)

Ronda 4: contar 2 desde 1: 1,[3] → eliminar 3
  1→5→(1)

Ronda 5: contar 2 desde 5: 5,[1] → eliminar 1
  5→(5)

Sobreviviente: 5

Verificar: josephus_formula(6, 2) = 0,1,0,3,2,4 → 4+1 = 5 ✓
```
</details>

### Ejercicio 2 — Fórmula iterativa

Implementa `josephus_formula(n, k)` en Rust.  Verifica con la simulación
para $n = 1..20$ y $k = 3$.

<details>
<summary>Implementación</summary>

```rust
fn josephus_formula(n: usize, k: usize) -> usize {
    let mut pos = 0;
    for i in 2..=n {
        pos = (pos + k) % i;
    }
    pos + 1
}

fn main() {
    for n in 1..=20 {
        let f = josephus_formula(n, 3);
        let s = josephus_simulate(n, 3);   // de la implementación anterior
        assert_eq!(f, s, "Mismatch at n={n}");
        print!("J({n:2},3) = {f:2}  ");
        if n % 5 == 0 { println!(); }
    }
}
```
</details>

### Ejercicio 3 — Caso k=2: fórmula binaria

Implementa `josephus_k2` usando manipulación de bits.  Verifica con la
fórmula general para $n = 1..32$.

<details>
<summary>Implementación con bits</summary>

```rust
fn josephus_k2(n: usize) -> usize {
    // Rotar bit más significativo al final
    let bits = usize::BITS - n.leading_zeros();  // número de bits
    let msb = 1 << (bits - 1);                   // bit más significativo
    let l = n - msb;                              // n sin el MSB
    2 * l + 1
}

// Alternativa con rotación de bits:
fn josephus_k2_rotate(n: usize) -> usize {
    let bits = usize::BITS - n.leading_zeros();
    ((n << 1) | 1) & ((1 << bits) - 1)
    // shift left (añadir 0), poner bit bajo en 1, mask a bits correctos
}
```

Ambas son $O(1)$ — sin loop.
</details>

### Ejercicio 4 — Orden de eliminación

Modifica la simulación para que retorne un `Vec<usize>` con el orden de
eliminación (no solo el sobreviviente).  Para $n=7$, $k=3$, ¿cuál es el
orden?

<details>
<summary>Resultado</summary>

```
n=7, k=3: eliminación [3, 6, 2, 7, 5, 1] → sobreviviente: 4
```

```rust
fn josephus_order(n: usize, k: usize) -> (Vec<usize>, usize) {
    let circle = build_circle(n);
    let mut order = Vec::with_capacity(n - 1);
    // ... simulación guardando cada eliminado en order ...
    (order, survivor)
}
```

La fórmula NO puede producir este orden — solo la simulación.
</details>

### Ejercicio 5 — Simulación con array

Implementa Josephus sin lista circular, usando un array de booleanos
`alive[n]` y saltando los muertos.  ¿Cuál es la complejidad?

<details>
<summary>Análisis</summary>

```c
int josephus_array(int n, int k) {
    int *alive = calloc(n, sizeof(int));
    for (int i = 0; i < n; i++) alive[i] = 1;

    int pos = 0, remaining = n;
    while (remaining > 1) {
        int count = 0;
        while (count < k) {
            if (alive[pos]) count++;
            if (count < k) pos = (pos + 1) % n;
        }
        alive[pos] = 0;
        remaining--;
        pos = (pos + 1) % n;
    }
    // encontrar el vivo
    for (int i = 0; i < n; i++)
        if (alive[i]) { free(alive); return i + 1; }
}
```

Complejidad: peor caso $O(n^2 k)$ — cada conteo de $k$ puede saltar
muchos muertos.  La lista circular es superior: $O(nk)$ porque los
eliminados desaparecen del recorrido.
</details>

### Ejercicio 6 — n grande

Calcula $J(1000000, 3)$ con la fórmula iterativa.  ¿Cuánto tarda?
¿Sería viable con la simulación de lista circular?

<details>
<summary>Análisis</summary>

Fórmula iterativa: $10^6$ iteraciones de suma y módulo — **microsegundos**.

Simulación: $10^6 \times 3 = 3 \times 10^6$ pasos de puntero + $10^6$
`malloc`/`free` — **decenas de milisegundos**, pero factible.

Simulación con array de booleanos: hacia el final, contar $k$ vivos
requiere saltar muchos muertos — puede alcanzar **segundos** para $n=10^6$.

$J(1000000, 3) = 481823$ (verificable con la fórmula).
</details>

### Ejercicio 7 — Variante: empezar desde posición s

Modifica la simulación para empezar a contar desde la persona $s$ en vez
de la persona 1.  Calcula $J(7, 3, \text{start}=4)$.

<details>
<summary>Implementación</summary>

```c
int josephus_start(int n, int k, int start) {
    Node *circle = build_circle(n);
    Node *prev = circle;
    while (prev->next != circle) prev = prev->next;
    Node *cur = circle;

    // Avanzar start-1 posiciones
    for (int i = 1; i < start; i++) {
        prev = cur;
        cur = cur->next;
    }

    // Simulación normal desde cur
    while (cur->next != cur) {
        for (int i = 1; i < k; i++) {
            prev = cur;
            cur = cur->next;
        }
        prev->next = cur->next;
        Node *tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    int survivor = cur->id;
    free(cur);
    return survivor;
}
```

Alternativamente, la fórmula ajustada:
$J(n, k, s) = ((J(n, k) - 1 + s - 1) \bmod n) + 1$
</details>

### Ejercicio 8 — Implementación completa en Rust

Implementa tanto la simulación (con lista circular y raw pointers) como la
fórmula iterativa en Rust.  Verifica que coinciden para $n = 1..100$,
$k = 2..5$.

<details>
<summary>Verificación</summary>

```rust
fn main() {
    for k in 2..=5 {
        for n in 1..=100 {
            let sim = josephus_simulate(n, k);
            let formula = josephus_formula(n, k);
            assert_eq!(sim, formula, "Fail n={n}, k={k}");
        }
        println!("k={k}: all n=1..100 passed");
    }
}
```

400 pruebas — todas deben pasar si ambas implementaciones son correctas.
</details>

### Ejercicio 9 — Tabla de sobrevivientes

Genera una tabla de $J(n, k)$ para $n = 1..12$ y $k = 2, 3, 4$.
¿Observas algún patrón para $k = 2$?

<details>
<summary>Tabla y patrón</summary>

```
n    k=2   k=3   k=4
1      1     1     1
2      1     2     2
3      3     2     2
4      1     1     2
5      3     4     1
6      5     1     5
7      7     4     2
8      1     7     6
9      3     1     3
10     5     4     8
11     7     7     5
12     9     10    10
```

Patrón para $k = 2$: los sobrevivientes son siempre impares ($1, 1, 3, 1, 3,
5, 7, 1, 3, 5, 7, 9$).  Se resetean a 1 cada vez que $n$ es potencia de 2
y crecen de 2 en 2 ($2L + 1$).
</details>

### Ejercicio 10 — Inverso: encontrar la posición

Dado que quieres ser el sobreviviente en un grupo de $n = 13$ personas con
$k = 3$, ¿en qué posición debes sentarte?  Verifica con simulación.

<details>
<summary>Solución</summary>

```c
int position = josephus_formula(13, 3);
// pos = 0
// i=2: (0+3)%2 = 1
// i=3: (1+3)%3 = 1
// ...iterando...
// i=13: resultado final
```

$J(13, 3) = 13$ — debes sentarte en la posición 13 (la última).

Verificar: simular la eliminación y confirmar que 13 es el último en pie.

Este es el problema original de Josefo: conocer $n$ y $k$ te permite
calcular tu posición segura en $O(n)$ tiempo con la fórmula.
</details>
