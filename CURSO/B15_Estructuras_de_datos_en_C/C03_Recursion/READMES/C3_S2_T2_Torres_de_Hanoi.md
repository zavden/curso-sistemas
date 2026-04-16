# T02 — Torres de Hanoi

## Índice

1. [El problema](#1-el-problema)
2. [Solución recursiva](#2-solución-recursiva)
3. [Traza para 3 discos](#3-traza-para-3-discos)
4. [Conteo de movimientos](#4-conteo-de-movimientos)
5. [Implementación en C](#5-implementación-en-c)
6. [Implementación en Rust](#6-implementación-en-rust)
7. [Visualización del árbol de llamadas](#7-visualización-del-árbol-de-llamadas)
8. [Versión iterativa](#8-versión-iterativa)
9. [Análisis de complejidad](#9-análisis-de-complejidad)
10. [Ejercicios](#10-ejercicios)

---

## 1. El problema

Tres postes (A, B, C) y $n$ discos de tamaños diferentes, inicialmente
apilados en el poste A de mayor (abajo) a menor (arriba).

**Objetivo**: mover todos los discos de A a C.

**Reglas**:
1. Solo se puede mover **un disco a la vez**.
2. Solo se puede tomar el disco del **tope** de un poste.
3. Un disco **nunca** puede colocarse sobre uno más pequeño.

### Estado inicial y final para $n = 3$

```
Estado inicial:              Estado final:

    |          |      |          |      |          |
   [1]         |      |          |      |         [1]
  [_2_]        |      |          |      |        [_2_]
 [__3__]       |      |          |      |       [__3__]
────A──── ────B──── ────C────  ────A──── ────B──── ────C────
```

### Origen

El problema fue inventado por el matemático Édouard Lucas en 1883. Lo
presentó con una leyenda: monjes en un templo mueven 64 discos de oro
siguiendo estas reglas. Cuando terminen, el mundo acabará. Con
$2^{64} - 1$ movimientos y uno por segundo, tardarían ~585 mil millones
de años.

---

## 2. Solución recursiva

La solución es sorprendentemente simple si se piensa recursivamente:

> Para mover $n$ discos de A a C usando B como auxiliar:
> 1. Mover los $n-1$ discos superiores de A a B (usando C como auxiliar).
> 2. Mover el disco $n$ (el más grande) de A a C.
> 3. Mover los $n-1$ discos de B a C (usando A como auxiliar).

### Por qué funciona

- **Paso 1**: los $n-1$ discos superiores deben salir del camino para
  poder mover el disco grande. Se mueven a B recursivamente.
- **Paso 2**: el disco grande ahora está solo en A y se mueve directamente
  a C. Este movimiento es legal porque C está vacío (o tiene discos más
  grandes debajo).
- **Paso 3**: los $n-1$ discos en B se mueven a C recursivamente, encima
  del disco grande.

### Pseudocódigo

```
FUNCIÓN hanoi(n, origen, destino, auxiliar):
    SI n == 0:
        RETORNAR              ← caso base: nada que mover
    hanoi(n-1, origen, auxiliar, destino)     ← paso 1
    mover disco n de origen a destino         ← paso 2
    hanoi(n-1, auxiliar, destino, origen)     ← paso 3
```

El caso base es $n = 0$: no hay discos que mover. Cada llamada recursiva
reduce $n$ en 1, garantizando terminación.

---

## 3. Traza para 3 discos

Postes: A (origen), B (auxiliar), C (destino).

```
hanoi(3, A, C, B)
├── hanoi(2, A, B, C)                    ← mover 2 discos A→B
│   ├── hanoi(1, A, C, B)               ← mover 1 disco A→C
│   │   ├── hanoi(0, A, B, C)           ← base
│   │   ├── Move disk 1: A → C          ← movimiento 1
│   │   └── hanoi(0, B, C, A)           ← base
│   ├── Move disk 2: A → B              ← movimiento 2
│   └── hanoi(1, C, B, A)               ← mover 1 disco C→B
│       ├── hanoi(0, C, A, B)           ← base
│       ├── Move disk 1: C → B          ← movimiento 3
│       └── hanoi(0, A, B, C)           ← base
├── Move disk 3: A → C                  ← movimiento 4
└── hanoi(2, B, C, A)                    ← mover 2 discos B→C
    ├── hanoi(1, B, A, C)               ← mover 1 disco B→A
    │   ├── hanoi(0, B, C, A)           ← base
    │   ├── Move disk 1: B → A          ← movimiento 5
    │   └── hanoi(0, C, A, B)           ← base
    ├── Move disk 2: B → C              ← movimiento 6
    └── hanoi(1, A, C, B)               ← mover 1 disco A→C
        ├── hanoi(0, A, B, C)           ← base
        ├── Move disk 1: A → C          ← movimiento 7
        └── hanoi(0, B, C, A)           ← base
```

### Secuencia de movimientos

| Paso | Disco | Desde | Hacia | Estado A | Estado B | Estado C |
|:----:|:-----:|:-----:|:-----:|:--------:|:--------:|:--------:|
| — | — | — | — | 3,2,1 | — | — |
| 1 | 1 | A | C | 3,2 | — | 1 |
| 2 | 2 | A | B | 3 | 2 | 1 |
| 3 | 1 | C | B | 3 | 2,1 | — |
| 4 | 3 | A | C | — | 2,1 | 3 |
| 5 | 1 | B | A | 1 | 2 | 3 |
| 6 | 2 | B | C | 1 | — | 3,2 |
| 7 | 1 | A | C | — | — | 3,2,1 |

**7 movimientos** = $2^3 - 1$.

---

## 4. Conteo de movimientos

### Recurrencia

Sea $T(n)$ el número de movimientos para $n$ discos:

$$T(n) = \begin{cases} 0 & \text{si } n = 0 \\ 2 \cdot T(n-1) + 1 & \text{si } n > 0 \end{cases}$$

El $2 \cdot T(n-1)$ viene de los pasos 1 y 3 (cada uno mueve $n-1$
discos). El $+1$ es el paso 2 (mover el disco grande).

### Solución cerrada

Expandiendo la recurrencia:

$$T(n) = 2 \cdot T(n-1) + 1$$
$$= 2(2 \cdot T(n-2) + 1) + 1 = 4 \cdot T(n-2) + 2 + 1$$
$$= 4(2 \cdot T(n-3) + 1) + 3 = 8 \cdot T(n-3) + 4 + 2 + 1$$
$$= 2^k \cdot T(n-k) + \sum_{i=0}^{k-1} 2^i$$

Con $k = n$ y $T(0) = 0$:

$$T(n) = \sum_{i=0}^{n-1} 2^i = 2^n - 1$$

### Verificación

| $n$ | $2^n - 1$ | Verificación |
|:---:|:---------:|:------------:|
| 0 | 0 | Nada que mover |
| 1 | 1 | Un movimiento directo |
| 2 | 3 | 1 + 1 + 1 |
| 3 | 7 | 3 + 1 + 3 |
| 4 | 15 | 7 + 1 + 7 |
| 5 | 31 | 15 + 1 + 15 |

### Demostración de optimalidad

$2^n - 1$ es no solo la cantidad que este algoritmo produce, sino el
**mínimo posible**. Demostración por inducción:

- **Base**: para $n = 1$, se necesita al menos 1 movimiento. $2^1 - 1 = 1$. ✓
- **Paso**: supongamos que $n-1$ discos requieren al menos $2^{n-1} - 1$
  movimientos. Para $n$ discos:
  - El disco más grande debe moverse al menos 1 vez.
  - Antes de moverlo, los $n-1$ superiores deben estar fuera (al menos
    $2^{n-1} - 1$ movimientos).
  - Después de moverlo, los $n-1$ deben volver encima (al menos
    $2^{n-1} - 1$ movimientos).
  - Total: al menos $2 \cdot (2^{n-1} - 1) + 1 = 2^n - 1$.

---

## 5. Implementación en C

```c
/* hanoi.c */
#include <stdio.h>

void hanoi(int n, char from, char to, char aux) {
    if (n == 0) return;

    hanoi(n - 1, from, aux, to);
    printf("Move disk %d: %c -> %c\n", n, from, to);
    hanoi(n - 1, aux, to, from);
}

int main(void) {
    int n = 4;
    printf("Tower of Hanoi with %d disks:\n\n", n);
    hanoi(n, 'A', 'C', 'B');
    printf("\nTotal moves: %d\n", (1 << n) - 1);
    return 0;
}
```

Salida para $n = 4$ (15 movimientos):

```
Tower of Hanoi with 4 disks:

Move disk 1: A -> B
Move disk 2: A -> C
Move disk 1: B -> C
Move disk 3: A -> B
Move disk 1: C -> A
Move disk 2: C -> B
Move disk 1: A -> B
Move disk 4: A -> C
Move disk 1: B -> C
Move disk 2: B -> A
Move disk 1: C -> A
Move disk 3: B -> C
Move disk 1: A -> B
Move disk 2: A -> C
Move disk 1: B -> C

Total moves: 15
```

### Nota sobre `1 << n`

`1 << n` es $2^n$ calculado con desplazamiento de bits. `(1 << n) - 1`
es $2^n - 1$. Para `int` de 32 bits, funciona hasta $n = 30$ ($2^{31} - 1$).
Para $n$ mayores, usar `1UL << n` (`unsigned long`).

### Compilación

```bash
gcc -std=c11 -Wall -Wextra -o hanoi hanoi.c
./hanoi
```

---

## 6. Implementación en Rust

```rust
// hanoi.rs

fn hanoi(n: u32, from: char, to: char, aux: char) {
    if n == 0 { return; }

    hanoi(n - 1, from, aux, to);
    println!("Move disk {}: {} -> {}", n, from, to);
    hanoi(n - 1, aux, to, from);
}

fn main() {
    let n = 4;
    println!("Tower of Hanoi with {} disks:\n", n);
    hanoi(n, 'A', 'C', 'B');
    println!("\nTotal moves: {}", (1u64 << n) - 1);
}
```

### Versión con conteo de movimientos

```rust
fn hanoi_count(n: u32, from: char, to: char, aux: char, moves: &mut u64) {
    if n == 0 { return; }

    hanoi_count(n - 1, from, aux, to, moves);
    *moves += 1;
    println!("Move {}: disk {} {} -> {}", moves, n, from, to);
    hanoi_count(n - 1, aux, to, from, moves);
}

fn main() {
    let mut moves = 0u64;
    hanoi_count(4, 'A', 'C', 'B', &mut moves);
    println!("\nTotal: {} moves", moves);
}
```

### Versión que retorna la lista de movimientos

```rust
fn hanoi_moves(n: u32, from: char, to: char, aux: char) -> Vec<(u32, char, char)> {
    if n == 0 { return Vec::new(); }

    let mut moves = hanoi_moves(n - 1, from, aux, to);
    moves.push((n, from, to));
    moves.extend(hanoi_moves(n - 1, aux, to, from));
    moves
}

fn main() {
    let moves = hanoi_moves(3, 'A', 'C', 'B');
    for (i, (disk, from, to)) in moves.iter().enumerate() {
        println!("{}. disk {} : {} -> {}", i + 1, disk, from, to);
    }
}
```

Esta versión recolecta los movimientos en un `Vec` en lugar de
imprimirlos. Útil para testing o para alimentar una visualización. El
costo es $O(2^n)$ de memoria para almacenar todos los movimientos.

---

## 7. Visualización del árbol de llamadas

Para $n = 3$, el árbol tiene la siguiente estructura:

```
                        hanoi(3, A, C, B)
                       /        |        \
              hanoi(2,A,B,C)  disk 3    hanoi(2,B,C,A)
              /     |     \   A→C       /     |     \
        h(1,A,C,B) d2   h(1,C,B,A)  h(1,B,A,C) d2   h(1,A,C,B)
        /  |  \   A→B   /  |  \    /  |  \   B→C   /  |  \
      h0  d1  h0       h0  d1  h0  h0  d1  h0      h0  d1  h0
          A→C               C→B        B→A              A→C
```

### Propiedades del árbol

- **Profundidad**: $n$ (para $n$ discos).
- **Nodos totales**: $2^{n+1} - 1$ (cada nodo es una llamada a `hanoi`).
- **Hojas**: $2^n$ (llamadas a `hanoi(0, ...)` que retornan inmediatamente).
- **Nodos internos**: $2^n - 1$ (cada uno genera un movimiento de disco).
- **Movimientos**: $2^n - 1$ = número de nodos internos.

El árbol es un **árbol binario completo** de profundidad $n$: cada nodo
interno tiene exactamente 2 hijos (las dos llamadas recursivas), y el
movimiento del disco se ejecuta entre ellas.

---

## 8. Versión iterativa

La versión iterativa existe pero es menos intuitiva. Se basa en un patrón
observado en la secuencia de movimientos:

### Patrón del disco más pequeño

El disco 1 (el más pequeño) se mueve en **todos los pasos impares**
(1, 3, 5, 7, ...) y siempre en la misma dirección cíclica:

- Si $n$ es **impar**: disco 1 se mueve $A \to C \to B \to A \to \ldots$
- Si $n$ es **par**: disco 1 se mueve $A \to B \to C \to A \to \ldots$

En los pasos pares, el único movimiento legal (que no involucre al disco
1) determina qué hacer.

### Implementación iterativa en C

```c
/* hanoi_iter.c */
#include <stdio.h>

typedef struct {
    int stack[64];
    int top;
    char name;
} Peg;

void peg_init(Peg *p, char name) {
    p->top = 0;
    p->name = name;
}

void peg_push(Peg *p, int disk) {
    p->stack[p->top++] = disk;
}

int peg_pop(Peg *p) {
    return p->stack[--p->top];
}

int peg_peek(const Peg *p) {
    return p->top == 0 ? 999 : p->stack[p->top - 1];
}

void move_disk(Peg *from, Peg *to) {
    int disk = peg_pop(from);
    printf("Move disk %d: %c -> %c\n", disk, from->name, to->name);
    peg_push(to, disk);
}

/* Move one disk between two pegs (the legal move) */
void move_between(Peg *a, Peg *b) {
    if (peg_peek(a) < peg_peek(b)) {
        move_disk(a, b);
    } else {
        move_disk(b, a);
    }
}

void hanoi_iterative(int n) {
    Peg A, B, C;
    peg_init(&A, 'A');
    peg_init(&B, 'B');
    peg_init(&C, 'C');

    /* push disks n, n-1, ..., 1 onto A */
    for (int i = n; i >= 1; i--) {
        peg_push(&A, i);
    }

    int total_moves = (1 << n) - 1;

    /* For odd n: cycle is A→C→B; for even n: cycle is A→B→C */
    Peg *first  = &A;
    Peg *second = (n % 2 == 1) ? &C : &B;
    Peg *third  = (n % 2 == 1) ? &B : &C;

    for (int i = 1; i <= total_moves; i++) {
        if (i % 3 == 1) {
            move_between(first, second);
        } else if (i % 3 == 2) {
            move_between(first, third);
        } else {
            move_between(second, third);
        }
    }
}

int main(void) {
    printf("Iterative Tower of Hanoi with 3 disks:\n\n");
    hanoi_iterative(3);
    return 0;
}
```

### Comparación recursiva vs iterativa

| Aspecto | Recursiva | Iterativa |
|---------|:---------:|:---------:|
| Líneas de lógica | 5 | ~30 |
| Claridad | Directa | Requiere conocer el patrón |
| Estado explícito | No necesita | Tres pilas explícitas |
| Stack del sistema | $O(n)$ frames | $O(1)$ frames |
| Profundidad máxima | $n \leq \sim 100,000$ | Sin límite |

La recursiva es elegante y directa. La iterativa es útil para
animaciones (se puede pausar entre movimientos, acceder al estado de
los postes en cualquier momento) o para $n$ muy grandes donde el stack
del sistema sería insuficiente. En la práctica, $n > 30$ ya produce
$2^{30} \approx 10^9$ movimientos — nadie ejecutaría esto.

---

## 9. Análisis de complejidad

### Tiempo

$$T(n) = 2^n - 1 = O(2^n)$$

El crecimiento es **exponencial**. Esto no es ineficiencia del algoritmo —
es inherente al problema. Se demostró en la sección 4 que $2^n - 1$ es el
mínimo posible. No existe algoritmo más rápido.

| $n$ | Movimientos | Tiempo (1 mov/s) |
|:---:|:-----------:|:----------------:|
| 5 | 31 | 31 s |
| 10 | 1,023 | 17 min |
| 20 | 1,048,575 | 12 días |
| 30 | $\sim 10^9$ | 34 años |
| 64 | $\sim 1.8 \times 10^{19}$ | 585 mil millones de años |

### Espacio

| Versión | Stack frames | Pilas de disco | Total |
|---------|:------------:|:--------------:|:-----:|
| Recursiva (print) | $O(n)$ | Ninguna | $O(n)$ |
| Recursiva (collect) | $O(n)$ stack + $O(2^n)$ Vec | Ninguna | $O(2^n)$ |
| Iterativa | $O(1)$ | $O(n)$ | $O(n)$ |

La recursiva sin recolección usa $O(n)$ espacio — un frame por nivel de
recursión, con profundidad máxima $n$.

### Recurrencia del número de llamadas

El número total de llamadas a `hanoi` (incluyendo los casos base):

$$C(n) = 2 \cdot C(n-1) + 1 \quad \text{con } C(0) = 1$$

Solución: $C(n) = 2^{n+1} - 1$. Para $n = 3$: 15 llamadas (7 internas +
8 bases). Para $n = 20$: ~2 millones de llamadas — instantáneo para un
CPU moderno, pero los movimientos son $\sim 10^6$.

---

## 10. Ejercicios

---

### Ejercicio 1 — Traza para 2 discos

Realiza la traza completa de `hanoi(2, 'A', 'C', 'B')` mostrando cada
llamada, su profundidad, y los movimientos generados.

<details>
<summary>¿Cuántos movimientos y cuántas llamadas totales?</summary>

```
hanoi(2, A, C, B)
├── hanoi(1, A, B, C)
│   ├── hanoi(0, A, C, B)     ← base, return
│   ├── Move disk 1: A → B    ← movimiento 1
│   └── hanoi(0, C, B, A)     ← base, return
├── Move disk 2: A → C        ← movimiento 2
└── hanoi(1, B, C, A)
    ├── hanoi(0, B, A, C)     ← base, return
    ├── Move disk 1: B → C    ← movimiento 3
    └── hanoi(0, A, C, B)     ← base, return
```

**3 movimientos** ($2^2 - 1 = 3$).
**7 llamadas totales** ($2^3 - 1 = 7$): 3 internas + 4 bases.

Estado paso a paso:
- Inicio: A=[2,1], B=[], C=[]
- Paso 1 (disco 1: A→B): A=[2], B=[1], C=[]
- Paso 2 (disco 2: A→C): A=[], B=[1], C=[2]
- Paso 3 (disco 1: B→C): A=[], B=[], C=[2,1] ✓

</details>

---

### Ejercicio 2 — Predecir movimientos

Sin ejecutar el algoritmo, calcula el número de movimientos para
$n = 1, 2, 3, 4, 5, 6$. Verifica que cada uno cumple $T(n) = 2T(n-1)+1$.

<details>
<summary>¿Cuántos movimientos para n=6?</summary>

| $n$ | $T(n) = 2T(n-1) + 1$ | $2^n - 1$ |
|:---:|:---------------------:|:---------:|
| 0 | 0 | 0 |
| 1 | $2(0)+1 = 1$ | 1 |
| 2 | $2(1)+1 = 3$ | 3 |
| 3 | $2(3)+1 = 7$ | 7 |
| 4 | $2(7)+1 = 15$ | 15 |
| 5 | $2(15)+1 = 31$ | 31 |
| 6 | $2(31)+1 = $ **63** | 63 |

$T(6) = 63 = 2^6 - 1$. La recurrencia y la fórmula cerrada coinciden
en todos los casos, como se demostró por inducción en la sección 4.

</details>

---

### Ejercicio 3 — Cuatro postes (Frame-Stewart)

Si se añade un cuarto poste (D), el problema se puede resolver en menos
movimientos. Para $n = 4$ con 3 postes se necesitan 15 movimientos.
Investiga: ¿cuántos se necesitan con 4 postes?

<details>
<summary>¿Cuál es la estrategia con 4 postes?</summary>

El algoritmo de **Frame-Stewart** (1941):

1. Elegir un $k$ ($1 \leq k < n$).
2. Mover los $k$ discos superiores a un poste auxiliar usando 4 postes.
3. Mover los $n - k$ discos restantes al destino usando **3 postes**
   (el cuarto está ocupado).
4. Mover los $k$ discos del auxiliar al destino usando 4 postes.

Para $n = 4$, la solución óptima con 4 postes usa **9 movimientos**
(vs 15 con 3 postes). La secuencia con $k = 2$:

1. Mover 2 discos superiores de A a B (usando 4 postes): 3 movimientos.
2. Mover 2 discos restantes de A a C (usando 3 postes): 3 movimientos.
3. Mover 2 discos de B a C (usando 4 postes): 3 movimientos.

Total: $3 + 3 + 3 = 9$.

La fórmula general no tiene forma cerrada conocida. Los valores óptimos
para algunos $n$:

| $n$ | 3 postes | 4 postes |
|:---:|:--------:|:--------:|
| 4 | 15 | 9 |
| 8 | 255 | 33 |
| 15 | 32,767 | 129 |
| 64 | $\sim 1.8 \times 10^{19}$ | ~18,000 |

El cuarto poste reduce dramáticamente el número de movimientos. La
conjetura de Frame-Stewart (que este algoritmo es óptimo para 4 postes)
fue probada para $n \leq 30$ en 2014 pero sigue sin demostrarse en
general.

</details>

---

### Ejercicio 4 — Contar llamadas

Implementa una versión de `hanoi` en Rust que retorne el número total de
**llamadas a la función** (incluyendo las de caso base). Verifica que
es $2^{n+1} - 1$.

<details>
<summary>¿Cómo cuentas las llamadas sin variable global?</summary>

Retornar el conteo como valor:

```rust
fn hanoi_count_calls(n: u32) -> u64 {
    if n == 0 { return 1; }       // this call counts
    let left = hanoi_count_calls(n - 1);
    // +1 for this call (the move)
    let right = hanoi_count_calls(n - 1);
    left + 1 + right
}

fn main() {
    for n in 0..=10 {
        let calls = hanoi_count_calls(n);
        let expected = (1u64 << (n + 1)) - 1;
        println!("n={:>2}: calls={}, 2^(n+1)-1={}, match={}",
                 n, calls, expected, calls == expected);
    }
}
```

Salida:

```
n= 0: calls=1, 2^(n+1)-1=1, match=true
n= 1: calls=3, 2^(n+1)-1=3, match=true
n= 2: calls=7, 2^(n+1)-1=7, match=true
n= 3: calls=15, 2^(n+1)-1=15, match=true
...
n=10: calls=2047, 2^(n+1)-1=2047, match=true
```

Cada llamada retorna la cantidad de llamadas en su sub-árbol (incluida
ella misma). La recurrencia $C(n) = 2C(n-1) + 1$ con $C(0) = 1$ da
$C(n) = 2^{n+1} - 1$.

</details>

---

### Ejercicio 5 — Visualización ASCII

Implementa en C una función que muestre el estado de los tres postes
visualmente después de cada movimiento:

```
    |          |          |
   [1]         |          |
  [_2_]        |          |
 [__3__]       |          |
───A─── ───B─── ───C───
```

<details>
<summary>¿Cómo representas los postes para poder dibujarlos?</summary>

Usar arrays como pilas para cada poste:

```c
#include <stdio.h>

#define MAX_DISKS 10

typedef struct {
    int disks[MAX_DISKS];
    int top;
    char name;
} Peg;

void peg_init(Peg *p, char name, int n) {
    p->name = name;
    p->top = 0;
    /* push disks n..1 if this is the source peg */
}

void print_state(Peg pegs[3], int n) {
    for (int row = n - 1; row >= 0; row--) {
        for (int p = 0; p < 3; p++) {
            if (row < pegs[p].top) {
                int disk = pegs[p].disks[row];
                /* print centered disk of width 2*disk-1 */
                int padding = n - disk;
                for (int i = 0; i < padding; i++) printf(" ");
                for (int i = 0; i < 2 * disk - 1; i++) printf("=");
                for (int i = 0; i < padding; i++) printf(" ");
            } else {
                /* print empty pole */
                for (int i = 0; i < n - 1; i++) printf(" ");
                printf("|");
                for (int i = 0; i < n - 1; i++) printf(" ");
            }
            printf("  ");  /* spacing between pegs */
        }
        printf("\n");
    }
    /* base line */
    for (int p = 0; p < 3; p++) {
        for (int i = 0; i < 2 * n - 1; i++) printf("-");
        printf("  ");
    }
    printf("\n\n");
}
```

Los discos se representan con `=` repetidos ($2 \times \text{disk} - 1$
caracteres) centrados en un campo de ancho $2n - 1$. Los postes vacíos
muestran `|`. Después de cada `move_disk`, llamar a `print_state` para
ver la animación paso a paso.

</details>

---

### Ejercicio 6 — Versión con 4 postes

Implementa en Rust el algoritmo de Frame-Stewart para 4 postes. Compara
el número de movimientos con la versión de 3 postes para $n = 1$ a $15$.

<details>
<summary>¿Cómo eliges el valor óptimo de $k$?</summary>

Probar todos los valores de $k$ y elegir el que minimiza:

```rust
fn hanoi4(n: u32, pegs: [char; 4]) -> Vec<(u32, char, char)> {
    if n == 0 { return Vec::new(); }
    if n == 1 {
        return vec![(1, pegs[0], pegs[3])];
    }

    // Try all k values, pick the one with fewest moves
    let mut best: Option<Vec<(u32, char, char)>> = None;

    for k in 1..n {
        let mut moves = Vec::new();

        // Move top k disks to peg[1] using 4 pegs
        moves.extend(hanoi4(k, [pegs[0], pegs[2], pegs[3], pegs[1]]));

        // Move remaining n-k disks to peg[3] using 3 pegs
        moves.extend(hanoi3(n - k, pegs[0], pegs[3], pegs[2]));

        // Move k disks from peg[1] to peg[3] using 4 pegs
        moves.extend(hanoi4(k, [pegs[1], pegs[0], pegs[2], pegs[3]]));

        if best.is_none() || moves.len() < best.as_ref().unwrap().len() {
            best = Some(moves);
        }
    }

    best.unwrap()
}

fn hanoi3(n: u32, from: char, to: char, aux: char) -> Vec<(u32, char, char)> {
    if n == 0 { return Vec::new(); }
    let mut moves = hanoi3(n - 1, from, aux, to);
    moves.push((n, from, to));
    moves.extend(hanoi3(n - 1, aux, to, from));
    moves
}
```

La elección óptima de $k$ depende de $n$. La secuencia de $k$ óptimos
sigue un patrón relacionado con los números triangulares, pero no hay
fórmula cerrada simple. La fuerza bruta sobre $k$ es viable porque
$k < n$ y $n$ es típicamente pequeño.

</details>

---

### Ejercicio 7 — Verificar la solución

Implementa en Rust un verificador que, dada una secuencia de movimientos
`Vec<(u32, char, char)>`, determine si resuelve el problema de Hanoi
correctamente: todos los discos terminan en C, ningún movimiento pone un
disco grande sobre uno pequeño.

<details>
<summary>¿Qué condiciones debes verificar en cada movimiento?</summary>

```rust
fn verify_hanoi(n: u32, moves: &[(u32, char, char)]) -> Result<(), String> {
    use std::collections::HashMap;

    let mut pegs: HashMap<char, Vec<u32>> = HashMap::new();
    // Initial state: all disks on 'A', largest at bottom
    pegs.insert('A', (1..=n).rev().collect());
    pegs.insert('B', Vec::new());
    pegs.insert('C', Vec::new());

    for (i, &(disk, from, to)) in moves.iter().enumerate() {
        let source = pegs.get_mut(&from)
            .ok_or(format!("Move {}: invalid peg '{}'", i+1, from))?;

        // Check disk is on top
        match source.last() {
            None => return Err(format!("Move {}: peg '{}' is empty", i+1, from)),
            Some(&top) if top != disk => {
                return Err(format!(
                    "Move {}: disk {} is not on top of '{}'", i+1, disk, from
                ));
            }
            _ => {}
        }
        source.pop();

        let dest = pegs.get_mut(&to).unwrap();
        // Check not placing on smaller disk
        if let Some(&top) = dest.last() {
            if disk > top {
                return Err(format!(
                    "Move {}: disk {} onto smaller disk {} on '{}'",
                    i+1, disk, top, to
                ));
            }
        }
        dest.push(disk);
    }

    // Check final state
    let c = pegs.get(&'C').unwrap();
    if c.len() as u32 != n {
        return Err(format!("Not all disks on C: {:?}", c));
    }

    Ok(())
}
```

Condiciones verificadas:
1. El poste origen no está vacío.
2. El disco especificado está en el tope del poste origen.
3. El disco no se coloca sobre uno más pequeño.
4. Al final, todos los discos están en C, de mayor a menor.

</details>

---

### Ejercicio 8 — Patrones en los movimientos

Observa la secuencia de discos movidos para $n = 4$:
`1, 2, 1, 3, 1, 2, 1, 4, 1, 2, 1, 3, 1, 2, 1`.

¿Qué patrón ves? Relacióna con la representación binaria del número de
movimiento.

<details>
<summary>¿Qué disco se mueve en el paso $i$?</summary>

El disco movido en el paso $i$ (base 1) es **la posición del bit menos
significativo** de $i$ (contando desde 1).

Formalmente: en el paso $i$, se mueve el disco $k$ donde $k$ es el
exponente de la mayor potencia de 2 que divide a $i$, más 1.

| Paso $i$ | Binario | Trailing zeros | Disco |
|:--------:|:-------:|:--------------:|:-----:|
| 1 | 0001 | 0 | 1 |
| 2 | 0010 | 1 | 2 |
| 3 | 0011 | 0 | 1 |
| 4 | 0100 | 2 | 3 |
| 5 | 0101 | 0 | 1 |
| 6 | 0110 | 1 | 2 |
| 7 | 0111 | 0 | 1 |
| 8 | 1000 | 3 | 4 |

Disco en paso $i$ = trailing zeros de $i$ + 1 = `i.trailing_zeros() + 1`.

El disco 1 se mueve en todos los pasos impares ($i$ par es divisible por
al menos $2^1$). El disco $n$ se mueve una sola vez (paso $2^{n-1}$, el
punto medio).

Este patrón permite implementar Hanoi sin recursión ni pilas, usando solo
aritmética de bits.

</details>

---

### Ejercicio 9 — Profundidad del stack

Para $n = 20$, calcula:
a) El número de movimientos.
b) El número de llamadas totales a `hanoi`.
c) La profundidad máxima del stack (frames simultáneos).

<details>
<summary>¿La profundidad del stack es proporcional a $n$ o a $2^n$?</summary>

```
a) Movimientos: 2^20 - 1 = 1,048,575 (≈ 1 millón)
b) Llamadas totales: 2^21 - 1 = 2,097,151 (≈ 2 millones)
c) Profundidad máxima del stack: 20 frames + main = 21
```

La profundidad es **$O(n)$**, no $O(2^n)$. Aunque se hacen 2 millones de
llamadas, la pila nunca tiene más de 20+1 frames simultáneos. Esto es
porque cada llamada retorna antes de que la segunda llamada del mismo
nivel ocurra — el árbol se recorre en profundidad, no en anchura.

Con ~32 bytes por frame y profundidad 21: ~672 bytes de stack. Incluso
para $n = 100,000$ (con $2^{100000}$ movimientos imposibles de ejecutar),
la profundidad del stack sería solo 100,000 frames = ~3 MB. El stack
nunca es el cuello de botella; el tiempo exponencial lo es.

</details>

---

### Ejercicio 10 — Hanoi con restricción

Variante: solo se permiten movimientos entre postes **adyacentes**
(A↔B y B↔C, pero NO A↔C directamente). ¿Cuántos movimientos se
necesitan para $n$ discos?

<details>
<summary>¿Cuál es la recurrencia y la solución?</summary>

Sin movimiento directo A→C, mover un disco de A a C requiere 2 pasos:
A→B, luego B→C.

Recurrencia para $n$ discos de A a C:

1. Mover $n-1$ discos de A a C (recursivamente): $T(n-1)$ movimientos.
2. Mover disco $n$ de A a B: 1 movimiento.
3. Mover $n-1$ discos de C a A (para liberar C): $T(n-1)$ movimientos.
4. Mover disco $n$ de B a C: 1 movimiento.
5. Mover $n-1$ discos de A a C: $T(n-1)$ movimientos.

$$T(n) = 3 \cdot T(n-1) + 2$$

Con $T(0) = 0$:

$$T(n) = 3^n - 1$$

| $n$ | Normal ($2^n-1$) | Adyacente ($3^n-1$) |
|:---:|:----------------:|:-------------------:|
| 1 | 1 | 2 |
| 2 | 3 | 8 |
| 3 | 7 | 26 |
| 4 | 15 | 80 |
| 5 | 31 | 242 |

La restricción de adyacencia hace que el problema crezca como $O(3^n)$ en
lugar de $O(2^n)$ — significativamente más lento. Es un ejemplo de cómo
cambiar las restricciones del problema cambia la complejidad fundamental.

</details>
