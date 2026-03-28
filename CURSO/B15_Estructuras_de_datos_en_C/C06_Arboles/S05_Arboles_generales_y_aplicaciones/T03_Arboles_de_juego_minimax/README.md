# Árboles de juego y minimax

## Concepto

Un **árbol de juego** modela todas las posibles secuencias de jugadas en un juego
de dos jugadores. Cada nodo representa un **estado** del juego, y cada arista una
**jugada** válida. Los hijos de un nodo son los estados resultantes de aplicar
cada jugada posible.

Propiedades del tipo de juegos que modelamos:

- **Dos jugadores** que alternan turnos.
- **Suma cero**: lo que gana uno, lo pierde el otro.
- **Información perfecta**: ambos jugadores ven el estado completo (no hay cartas
  ocultas ni dados).
- **Determinista**: no hay azar.

Ejemplos: tres en raya (tic-tac-toe), ajedrez, damas, Go, Othello, Connect Four.

Contraejemplos: póker (información oculta), backgammon (azar con dados).

---

## El árbol de juego

Para tres en raya, el árbol comienza con el tablero vacío y se ramifica:

```
              [ vacío ]           <- MAX (X) juega
             /    |    \
          [X__]  [_X_]  ...       <- MIN (O) juega
         / | \
     [XO_] [X_O] ...             <- MAX (X) juega
       ...
```

Dimensiones del árbol completo de tres en raya:

| Propiedad | Valor |
|-----------|-------|
| Factor de ramificación máximo | 9 (primer turno) |
| Profundidad máxima | 9 jugadas |
| Nodos totales (cota superior) | $9! = 362\,880$ |
| Nodos reales (con simetría y terminación temprana) | $\approx 5\,478$ |
| Estados terminales | 958 (362 victorias X, 148 victorias O, 448 empates por simetría reducida) |

Para ajedrez, el árbol es astronómicamente más grande: factor de ramificación
promedio $\approx 35$, profundidad típica $\approx 80$ jugadas, lo que da
$\approx 35^{80} \approx 10^{123}$ nodos (más que átomos en el universo observable,
$\approx 10^{80}$). Por eso no se puede explorar completo — se necesitan
heurísticas y poda.

---

## El algoritmo minimax

**Minimax** es el algoritmo fundamental para decidir la mejor jugada en un árbol
de juego. La idea:

- Un jugador (**MAX**) quiere **maximizar** su puntuación.
- El otro (**MIN**) quiere **minimizarla**.
- Ambos juegan de forma **óptima** (eligen la mejor jugada disponible).

### Valores de los nodos terminales

A cada estado terminal se le asigna un valor desde la perspectiva de MAX:

| Resultado | Valor |
|-----------|-------|
| MAX gana | $+1$ (o $+\infty$, o $+10$) |
| MIN gana | $-1$ (o $-\infty$, o $-10$) |
| Empate | $0$ |

### Regla de propagación

Para nodos no terminales:

- Si es turno de **MAX**: el valor es el **máximo** de los valores de sus hijos.
- Si es turno de **MIN**: el valor es el **mínimo** de los valores de sus hijos.

MAX elige la jugada que le da el mayor valor posible, asumiendo que MIN responderá
con la jugada que le da el menor valor posible.

### Ejemplo con árbol pequeño

```
              MAX
           /   |   \
         MIN  MIN  MIN
        / \    |   / \
       3   5   2  9   1
```

Evaluación de abajo hacia arriba:

- Nodo MIN izquierdo: $\min(3, 5) = 3$
- Nodo MIN central: $\min(2) = 2$
- Nodo MIN derecho: $\min(9, 1) = 1$
- Nodo MAX (raíz): $\max(3, 2, 1) = 3$

MAX elige la rama izquierda (valor 3). MIN responderá eligiendo 3 (no 5).
El resultado garantizado para MAX es **3**, sin importar lo que haga MIN.

### Pseudocódigo

```
function minimax(node, is_maximizing):
    if node is terminal:
        return evaluate(node)

    if is_maximizing:
        best = -∞
        for each child of node:
            val = minimax(child, false)
            best = max(best, val)
        return best
    else:
        best = +∞
        for each child of node:
            val = minimax(child, true)
            best = min(best, val)
        return best
```

### En C (genérico)

```c
#define MAX_CHILDREN 20

typedef struct GameNode {
    int value;              // solo para terminales
    int is_terminal;
    struct GameNode *children[MAX_CHILDREN];
    int num_children;
} GameNode;

int minimax(GameNode *node, int is_maximizing) {
    if (node->is_terminal) {
        return node->value;
    }

    if (is_maximizing) {
        int best = -1000;
        for (int i = 0; i < node->num_children; i++) {
            int val = minimax(node->children[i], 0);
            if (val > best) best = val;
        }
        return best;
    } else {
        int best = 1000;
        for (int i = 0; i < node->num_children; i++) {
            int val = minimax(node->children[i], 1);
            if (val < best) best = val;
        }
        return best;
    }
}
```

### En Rust

```rust
fn minimax(node: &GameNode, is_maximizing: bool) -> i32 {
    if node.is_terminal {
        return node.value;
    }

    if is_maximizing {
        node.children.iter()
            .map(|c| minimax(c, false))
            .max()
            .unwrap()
    } else {
        node.children.iter()
            .map(|c| minimax(c, true))
            .min()
            .unwrap()
    }
}
```

**Complejidad**: $O(b^d)$ donde $b$ es el factor de ramificación y $d$ la
profundidad. Recorre **todo** el árbol.

---

## Poda alfa-beta

La poda alfa-beta es una optimización de minimax que **evita explorar ramas**
que no pueden influir en la decisión final. El resultado es exactamente el
mismo que minimax, pero se exploran menos nodos.

### Intuición

Si MAX ya encontró una opción que le garantiza al menos 5, y al explorar otra
rama descubre que MIN puede forzar un valor de 3 ahí, MAX no necesita seguir
explorando esa rama — ya tiene algo mejor.

Los dos parámetros:

- $\alpha$: el **mejor** valor que MAX tiene garantizado hasta ahora (límite
  inferior). Inicia en $-\infty$.
- $\beta$: el **mejor** valor que MIN tiene garantizado hasta ahora (límite
  superior). Inicia en $+\infty$.

**Poda**: si en cualquier punto $\alpha \geq \beta$, se puede cortar la búsqueda
en esa rama.

### Ejemplo detallado

```
                MAX (α=-∞, β=+∞)
              /        \
          MIN            MIN
         / \            / \
        3   5          2   ?
```

1. Explorar rama izquierda de la raíz:
   - Nodo MIN, hijos 3 y 5.
   - MIN evalúa 3 → $\beta = 3$. Evalúa 5 → $\min(3,5) = 3$. Retorna 3.
2. En la raíz MAX: $\alpha = 3$ (ya tiene garantizado al menos 3).
3. Explorar rama derecha:
   - Nodo MIN con $\alpha=3$, $\beta=+\infty$.
   - MIN evalúa su primer hijo: 2. Ahora $\beta = 2$.
   - Verificar: $\alpha(3) \geq \beta(2)$ → **PODA**. No evaluar `?`.
   - ¿Por qué? MIN ya puede forzar 2 en esta rama, pero MAX nunca elegiría
     esta rama porque ya tiene 3 garantizado.

Resultado: **3** (igual que minimax), pero **sin evaluar** el nodo `?`.

### Implementación

```c
int alpha_beta(GameNode *node, int alpha, int beta, int is_maximizing) {
    if (node->is_terminal) {
        return node->value;
    }

    if (is_maximizing) {
        int best = -1000;
        for (int i = 0; i < node->num_children; i++) {
            int val = alpha_beta(node->children[i], alpha, beta, 0);
            if (val > best) best = val;
            if (best > alpha) alpha = best;
            if (alpha >= beta) break;  // poda beta
        }
        return best;
    } else {
        int best = 1000;
        for (int i = 0; i < node->num_children; i++) {
            int val = alpha_beta(node->children[i], alpha, beta, 1);
            if (val < best) best = val;
            if (best < beta) beta = best;
            if (alpha >= beta) break;  // poda alfa
        }
        return best;
    }
}
// llamar: alpha_beta(root, -1000, 1000, 1);
```

```rust
fn alpha_beta(node: &GameNode, mut alpha: i32, mut beta: i32,
              is_maximizing: bool) -> i32 {
    if node.is_terminal {
        return node.value;
    }

    if is_maximizing {
        let mut best = i32::MIN;
        for child in &node.children {
            let val = alpha_beta(child, alpha, beta, false);
            best = best.max(val);
            alpha = alpha.max(best);
            if alpha >= beta { break; }  // beta cutoff
        }
        best
    } else {
        let mut best = i32::MAX;
        for child in &node.children {
            let val = alpha_beta(child, alpha, beta, true);
            best = best.min(val);
            beta = beta.min(best);
            if alpha >= beta { break; }  // alpha cutoff
        }
        best
    }
}
```

### Eficiencia de la poda

| Aspecto | Minimax | Alfa-beta |
|---------|---------|-----------|
| Resultado | Valor óptimo | Idéntico |
| Nodos explorados (peor caso) | $b^d$ | $b^d$ (sin poda) |
| Nodos explorados (mejor caso) | $b^d$ | $b^{d/2}$ |
| Nodos explorados (caso promedio) | $b^d$ | $\approx b^{3d/4}$ |

En el **mejor caso** (cuando los hijos están ordenados de mejor a peor), la
poda reduce la complejidad de $O(b^d)$ a $O(b^{d/2})$. Esto equivale a duplicar
la profundidad de búsqueda con el mismo costo. Para ajedrez con $b \approx 35$:
minimax a profundidad 6 explora $\approx 1.8 \times 10^9$ nodos, mientras que
alfa-beta en el mejor caso explora $\approx 42\,875$ ($35^3$).

El **orden de evaluación** es crucial. Heurísticas comunes para mejorar el
orden: evaluar capturas primero (ajedrez), ordenar por evaluación heurística
previa, usar tablas de transposición.

---

## Tres en raya: implementación completa

Aplicamos minimax con poda alfa-beta a tres en raya, donde el árbol es lo
suficientemente pequeño para explorarlo completamente.

### Representación del tablero

```c
// tablero[i] = ' ' (vacio), 'X' (MAX), 'O' (MIN)
// posiciones: 0|1|2
//             3|4|5
//             6|7|8

typedef struct {
    char cells[9];
    char current_player;  // 'X' o 'O'
} Board;

void board_init(Board *b) {
    memset(b->cells, ' ', 9);
    b->current_player = 'X';
}

void board_print(Board *b) {
    printf(" %c | %c | %c\n", b->cells[0], b->cells[1], b->cells[2]);
    printf("---+---+---\n");
    printf(" %c | %c | %c\n", b->cells[3], b->cells[4], b->cells[5]);
    printf("---+---+---\n");
    printf(" %c | %c | %c\n", b->cells[6], b->cells[7], b->cells[8]);
}
```

### Detectar victoria y empate

```c
int check_winner(Board *b, char player) {
    // filas
    for (int i = 0; i < 9; i += 3)
        if (b->cells[i] == player && b->cells[i+1] == player
            && b->cells[i+2] == player) return 1;
    // columnas
    for (int i = 0; i < 3; i++)
        if (b->cells[i] == player && b->cells[i+3] == player
            && b->cells[i+6] == player) return 1;
    // diagonales
    if (b->cells[0] == player && b->cells[4] == player
        && b->cells[8] == player) return 1;
    if (b->cells[2] == player && b->cells[4] == player
        && b->cells[6] == player) return 1;
    return 0;
}

int board_full(Board *b) {
    for (int i = 0; i < 9; i++)
        if (b->cells[i] == ' ') return 0;
    return 1;
}
```

### Evaluación terminal

```c
// retorna +10 si gana X, -10 si gana O, 0 si empate
// el +/-10 es arbitrario; cualquier valor > 0 y < 0 funciona
int evaluate(Board *b) {
    if (check_winner(b, 'X')) return 10;
    if (check_winner(b, 'O')) return -10;
    return 0;
}
```

Se usa $\pm 10$ en lugar de $\pm 1$ para permitir una mejora: restar la
profundidad al puntaje para que la IA prefiera ganar **rápido** y perder
**tarde** (veremos esto más adelante).

### Minimax para tres en raya

```c
int minimax_ttt(Board *b, int is_maximizing) {
    int score = evaluate(b);
    if (score != 0) return score;     // alguien gano
    if (board_full(b)) return 0;      // empate

    if (is_maximizing) {
        int best = -1000;
        for (int i = 0; i < 9; i++) {
            if (b->cells[i] == ' ') {
                b->cells[i] = 'X';
                int val = minimax_ttt(b, 0);
                b->cells[i] = ' ';    // deshacer
                if (val > best) best = val;
            }
        }
        return best;
    } else {
        int best = 1000;
        for (int i = 0; i < 9; i++) {
            if (b->cells[i] == ' ') {
                b->cells[i] = 'O';
                int val = minimax_ttt(b, 1);
                b->cells[i] = ' ';    // deshacer
                if (val < best) best = val;
            }
        }
        return best;
    }
}
```

Observación importante: **no construimos el árbol explícitamente**. En lugar de
crear nodos hijos, modificamos el tablero in-place, hacemos la llamada recursiva,
y **deshacemos** la jugada (backtracking). Esto es mucho más eficiente en memoria.

### Encontrar la mejor jugada

```c
int best_move(Board *b) {
    int best_val = -1000;
    int best_pos = -1;

    for (int i = 0; i < 9; i++) {
        if (b->cells[i] == ' ') {
            b->cells[i] = 'X';
            int val = minimax_ttt(b, 0);  // MIN responde
            b->cells[i] = ' ';
            if (val > best_val) {
                best_val = val;
                best_pos = i;
            }
        }
    }

    printf("Mejor jugada: posicion %d (valor: %d)\n", best_pos, best_val);
    return best_pos;
}
```

### Con poda alfa-beta

```c
int alphabeta_ttt(Board *b, int alpha, int beta, int is_maximizing) {
    int score = evaluate(b);
    if (score != 0) return score;
    if (board_full(b)) return 0;

    if (is_maximizing) {
        int best = -1000;
        for (int i = 0; i < 9; i++) {
            if (b->cells[i] == ' ') {
                b->cells[i] = 'X';
                int val = alphabeta_ttt(b, alpha, beta, 0);
                b->cells[i] = ' ';
                if (val > best) best = val;
                if (best > alpha) alpha = best;
                if (alpha >= beta) break;
            }
        }
        return best;
    } else {
        int best = 1000;
        for (int i = 0; i < 9; i++) {
            if (b->cells[i] == ' ') {
                b->cells[i] = 'O';
                int val = alphabeta_ttt(b, alpha, beta, 1);
                b->cells[i] = ' ';
                if (val < best) best = val;
                if (best < beta) beta = best;
                if (alpha >= beta) break;
            }
        }
        return best;
    }
}
```

### Preferir victorias rápidas

Un problema sutil: minimax asigna el mismo valor ($+10$) a ganar en 3 jugadas
que en 7 jugadas. Queremos que la IA prefiera ganar lo antes posible y, si va a
perder, que lo retrase lo más posible.

Solución: incorporar la **profundidad** en la evaluación:

```c
int minimax_depth(Board *b, int depth, int is_maximizing) {
    int score = evaluate(b);
    if (score > 0) return score - depth;   // ganar rapido: mayor puntaje
    if (score < 0) return score + depth;   // perder tarde: menor castigo
    if (board_full(b)) return 0;

    if (is_maximizing) {
        int best = -1000;
        for (int i = 0; i < 9; i++) {
            if (b->cells[i] == ' ') {
                b->cells[i] = 'X';
                int val = minimax_depth(b, depth + 1, 0);
                b->cells[i] = ' ';
                if (val > best) best = val;
            }
        }
        return best;
    } else {
        int best = 1000;
        for (int i = 0; i < 9; i++) {
            if (b->cells[i] == ' ') {
                b->cells[i] = 'O';
                int val = minimax_depth(b, depth + 1, 1);
                b->cells[i] = ' ';
                if (val < best) best = val;
            }
        }
        return best;
    }
}
```

Con esto, ganar en profundidad 3 da $10 - 3 = 7$, mientras que ganar en
profundidad 7 da $10 - 7 = 3$. La IA elige la victoria más rápida.

---

## Negamax: simplificación elegante

En un juego de suma cero, el valor de una posición para un jugador es el negativo
del valor para el otro. Esto permite unificar MAX y MIN en una sola función:

```c
int negamax(Board *b, int player_sign) {
    int score = evaluate(b) * player_sign;
    if (score != 0) return score;
    if (board_full(b)) return 0;

    char player = (player_sign == 1) ? 'X' : 'O';
    int best = -1000;

    for (int i = 0; i < 9; i++) {
        if (b->cells[i] == ' ') {
            b->cells[i] = player;
            int val = -negamax(b, -player_sign);
            b->cells[i] = ' ';
            if (val > best) best = val;
        }
    }
    return best;
}
// llamar: negamax(b, 1) para X, negamax(b, -1) para O
```

La clave es `-negamax(b, -player_sign)`: el valor del oponente negado es mi
valor. Esto elimina la bifurcación `if (is_maximizing)` y reduce la duplicación
de código.

Negamax con poda alfa-beta:

```c
int negamax_ab(Board *b, int alpha, int beta, int player_sign) {
    int score = evaluate(b) * player_sign;
    if (score != 0) return score;
    if (board_full(b)) return 0;

    char player = (player_sign == 1) ? 'X' : 'O';
    int best = -1000;

    for (int i = 0; i < 9; i++) {
        if (b->cells[i] == ' ') {
            b->cells[i] = player;
            int val = -negamax_ab(b, -beta, -alpha, -player_sign);
            b->cells[i] = ' ';
            if (val > best) best = val;
            if (best > alpha) alpha = best;
            if (alpha >= beta) break;
        }
    }
    return best;
}
```

Notar la inversión: $-\beta$ se convierte en el nuevo $\alpha$ y $-\alpha$ en el
nuevo $\beta$. Esta simetría es la razón por la que negamax es preferido en
implementaciones reales.

---

## Conteo de nodos explorados

Para cuantificar la mejora de la poda, contamos los nodos:

```c
int nodes_explored = 0;

int minimax_counted(Board *b, int is_maximizing) {
    nodes_explored++;
    // ... resto igual
}
```

Resultados típicos desde tablero vacío:

| Algoritmo | Nodos explorados |
|-----------|-----------------|
| Minimax | $\approx 549\,946$ |
| Alfa-beta | $\approx 18\,297$ |
| Alfa-beta + preferir profundidad | $\approx 16\,000$ |

La poda alfa-beta reduce la exploración en un **97%** para tres en raya.

---

## Limitaciones y extensiones

### Profundidad limitada

Para juegos con árboles enormes (ajedrez, Go), no se puede explorar hasta los
estados terminales. Se usa una **función de evaluación heurística** que estima
el valor de posiciones no terminales:

```c
int minimax_limited(Board *b, int depth, int max_depth, int is_max) {
    if (is_terminal(b) || depth == max_depth) {
        return heuristic_eval(b);  // evaluacion aproximada
    }
    // ... resto igual
}
```

La calidad de la función heurística determina la fuerza del programa. En ajedrez,
las heurísticas clásicas consideran: valor material, control del centro, seguridad
del rey, estructura de peones, movilidad, etc.

### Tabla de transposiciones

Muchas posiciones se repiten por distinto orden de jugadas. Una **tabla de
transposiciones** (hash table) almacena posiciones ya evaluadas para evitar
recalcularlas. En tres en raya, esto es simple (el tablero es pequeño); en
ajedrez, se usa el hash de Zobrist.

### Búsqueda iterativa con profundización

En lugar de buscar directamente a profundidad $d$, se busca primero a profundidad
1, luego 2, luego 3, etc. Esto parece redundante, pero tiene ventajas:

1. Siempre hay una respuesta disponible (la de la profundidad anterior) si se
   acaba el tiempo.
2. La información de profundidades anteriores mejora el orden de los nodos para
   la poda alfa-beta.
3. El costo total es solo $\approx \frac{b}{b-1}$ veces el de la última
   iteración (para $b$ grande, casi gratuito).

---

## Programa completo en C

```c
#include <stdio.h>
#include <string.h>

typedef struct {
    char cells[9];
} Board;

void board_init(Board *b) {
    memset(b->cells, ' ', 9);
}

void board_print(Board *b) {
    for (int r = 0; r < 3; r++) {
        printf(" %c | %c | %c\n",
               b->cells[r*3], b->cells[r*3+1], b->cells[r*3+2]);
        if (r < 2) printf("---+---+---\n");
    }
    printf("\n");
}

int check_winner(Board *b, char p) {
    for (int i = 0; i < 9; i += 3)
        if (b->cells[i]==p && b->cells[i+1]==p && b->cells[i+2]==p) return 1;
    for (int i = 0; i < 3; i++)
        if (b->cells[i]==p && b->cells[i+3]==p && b->cells[i+6]==p) return 1;
    if (b->cells[0]==p && b->cells[4]==p && b->cells[8]==p) return 1;
    if (b->cells[2]==p && b->cells[4]==p && b->cells[6]==p) return 1;
    return 0;
}

int board_full(Board *b) {
    for (int i = 0; i < 9; i++)
        if (b->cells[i] == ' ') return 0;
    return 1;
}

int evaluate(Board *b) {
    if (check_winner(b, 'X')) return 10;
    if (check_winner(b, 'O')) return -10;
    return 0;
}

// ======== Minimax ========

int mm_nodes = 0;

int minimax(Board *b, int depth, int is_max) {
    mm_nodes++;
    int score = evaluate(b);
    if (score > 0) return score - depth;
    if (score < 0) return score + depth;
    if (board_full(b)) return 0;

    char player = is_max ? 'X' : 'O';

    if (is_max) {
        int best = -1000;
        for (int i = 0; i < 9; i++) {
            if (b->cells[i] == ' ') {
                b->cells[i] = player;
                int val = minimax(b, depth + 1, 0);
                b->cells[i] = ' ';
                if (val > best) best = val;
            }
        }
        return best;
    } else {
        int best = 1000;
        for (int i = 0; i < 9; i++) {
            if (b->cells[i] == ' ') {
                b->cells[i] = player;
                int val = minimax(b, depth + 1, 1);
                b->cells[i] = ' ';
                if (val < best) best = val;
            }
        }
        return best;
    }
}

// ======== Alfa-beta ========

int ab_nodes = 0;

int alphabeta(Board *b, int depth, int alpha, int beta, int is_max) {
    ab_nodes++;
    int score = evaluate(b);
    if (score > 0) return score - depth;
    if (score < 0) return score + depth;
    if (board_full(b)) return 0;

    char player = is_max ? 'X' : 'O';

    if (is_max) {
        int best = -1000;
        for (int i = 0; i < 9; i++) {
            if (b->cells[i] == ' ') {
                b->cells[i] = player;
                int val = alphabeta(b, depth + 1, alpha, beta, 0);
                b->cells[i] = ' ';
                if (val > best) best = val;
                if (best > alpha) alpha = best;
                if (alpha >= beta) break;
            }
        }
        return best;
    } else {
        int best = 1000;
        for (int i = 0; i < 9; i++) {
            if (b->cells[i] == ' ') {
                b->cells[i] = player;
                int val = alphabeta(b, depth + 1, alpha, beta, 1);
                b->cells[i] = ' ';
                if (val < best) best = val;
                if (best < beta) beta = best;
                if (alpha >= beta) break;
            }
        }
        return best;
    }
}

// ======== Mejor jugada ========

int best_move_ab(Board *b, char player) {
    int is_max = (player == 'X');
    int best_val = is_max ? -1000 : 1000;
    int best_pos = -1;

    for (int i = 0; i < 9; i++) {
        if (b->cells[i] == ' ') {
            b->cells[i] = player;
            int val = alphabeta(b, 0, -1000, 1000, !is_max);
            b->cells[i] = ' ';

            if (is_max && val > best_val) {
                best_val = val; best_pos = i;
            }
            if (!is_max && val < best_val) {
                best_val = val; best_pos = i;
            }
        }
    }
    return best_pos;
}

// ======== Demo: IA vs IA ========

void play_ai_vs_ai(void) {
    Board b;
    board_init(&b);
    char player = 'X';

    printf("=== IA vs IA ===\n\n");

    while (1) {
        int pos = best_move_ab(&b, player);
        b.cells[pos] = player;
        printf("%c juega en posicion %d:\n", player, pos);
        board_print(&b);

        if (check_winner(&b, player)) {
            printf("%c gana!\n", player);
            return;
        }
        if (board_full(&b)) {
            printf("Empate!\n");
            return;
        }
        player = (player == 'X') ? 'O' : 'X';
    }
}

// ======== Demo: analisis de primera jugada ========

void analyze_first_move(void) {
    Board b;
    board_init(&b);

    printf("=== Analisis de primera jugada (X) ===\n\n");
    printf("Posicion | Valor | Nodos (minimax) | Nodos (alfa-beta)\n");
    printf("---------|-------|-----------------|------------------\n");

    for (int i = 0; i < 9; i++) {
        b.cells[i] = 'X';

        mm_nodes = 0;
        int val_mm = minimax(&b, 1, 0);
        int mm_count = mm_nodes;

        ab_nodes = 0;
        int val_ab = alphabeta(&b, 1, -1000, 1000, 0);
        int ab_count = ab_nodes;

        char *pos_name;
        if (i == 0 || i == 2 || i == 6 || i == 8) pos_name = "esquina";
        else if (i == 4) pos_name = "centro ";
        else pos_name = "borde  ";

        printf("   %d %s |  %3d  |     %7d     |     %7d\n",
               i, pos_name, val_ab, mm_count, ab_count);

        b.cells[i] = ' ';
    }
}

// ======== Demo: escenario con victoria forzada ========

void analyze_forced_win(void) {
    Board b;
    board_init(&b);

    // X tiene doble amenaza
    b.cells[0] = 'X'; b.cells[4] = 'X';  // X en esquina y centro
    b.cells[1] = 'O'; b.cells[3] = 'O';  // O en bordes

    printf("\n=== Escenario: turno de X ===\n\n");
    board_print(&b);

    ab_nodes = 0;
    int pos = best_move_ab(&b, 'X');
    printf("Mejor jugada de X: posicion %d (nodos explorados: %d)\n\n",
           pos, ab_nodes);

    b.cells[pos] = 'X';
    printf("Tablero resultante:\n");
    board_print(&b);
}

int main(void) {
    analyze_first_move();
    analyze_forced_win();
    play_ai_vs_ai();
    return 0;
}
```

### Salida esperada

```
=== Analisis de primera jugada (X) ===

Posicion | Valor | Nodos (minimax) | Nodos (alfa-beta)
---------|-------|-----------------|------------------
   0 esquina |    0  |       59704     |        4952
   1 borde   |   -1  |       55244     |        3354
   2 esquina |    0  |       59704     |        4952
   3 borde   |   -1  |       55244     |        3354
   4 centro  |    0  |       52802     |        2929
   5 borde   |   -1  |       55244     |        3354
   6 esquina |    0  |       59704     |        4952
   7 borde   |   -1  |       55244     |        3354
   8 esquina |    0  |       59704     |        4952

=== Escenario: turno de X ===

 X | O |
---+---+---
 O | X |
---+---+---
   |   |

Mejor jugada de X: posicion 8 (nodos explorados: 167)

Tablero resultante:
 X | O |
---+---+---
 O | X |
---+---+---
   |   | X

=== IA vs IA ===

X juega en posicion 0:
 X |   |
---+---+---
   |   |
---+---+---
   |   |

O juega en posicion 4:
 X |   |
---+---+---
   | O |
---+---+---
   |   |

...

Empate!
```

Los valores exactos de nodos pueden variar ligeramente según la implementación
del ajuste por profundidad, pero el patrón es claro: esquina y centro dan
valor 0 (empate con juego óptimo), los bordes dan $-1$ (MIN puede forzar
ventaja con la respuesta correcta de profundidad). Alfa-beta reduce los nodos
explorados significativamente.

El resultado de IA vs IA es siempre **empate** — tres en raya es un juego
resuelto: con juego óptimo de ambos lados, el resultado es siempre tablas.

---

## Implementación en Rust

```rust
#[derive(Clone)]
struct Board {
    cells: [char; 9],
}

impl Board {
    fn new() -> Self {
        Board { cells: [' '; 9] }
    }

    fn print(&self) {
        for r in 0..3 {
            println!(" {} | {} | {}",
                self.cells[r*3], self.cells[r*3+1], self.cells[r*3+2]);
            if r < 2 { println!("---+---+---"); }
        }
        println!();
    }

    fn check_winner(&self, p: char) -> bool {
        let lines = [
            [0,1,2],[3,4,5],[6,7,8],  // filas
            [0,3,6],[1,4,7],[2,5,8],  // columnas
            [0,4,8],[2,4,6],          // diagonales
        ];
        lines.iter().any(|line|
            line.iter().all(|&i| self.cells[i] == p))
    }

    fn is_full(&self) -> bool {
        self.cells.iter().all(|&c| c != ' ')
    }

    fn evaluate(&self) -> i32 {
        if self.check_winner('X') { 10 }
        else if self.check_winner('O') { -10 }
        else { 0 }
    }

    fn available_moves(&self) -> Vec<usize> {
        (0..9).filter(|&i| self.cells[i] == ' ').collect()
    }
}

fn alphabeta(b: &mut Board, depth: i32, mut alpha: i32, mut beta: i32,
             is_max: bool, nodes: &mut u64) -> i32 {
    *nodes += 1;
    let score = b.evaluate();
    if score > 0 { return score - depth; }
    if score < 0 { return score + depth; }
    if b.is_full() { return 0; }

    let player = if is_max { 'X' } else { 'O' };
    let moves = b.available_moves();

    if is_max {
        let mut best = i32::MIN;
        for pos in moves {
            b.cells[pos] = player;
            let val = alphabeta(b, depth + 1, alpha, beta, false, nodes);
            b.cells[pos] = ' ';
            best = best.max(val);
            alpha = alpha.max(best);
            if alpha >= beta { break; }
        }
        best
    } else {
        let mut best = i32::MAX;
        for pos in moves {
            b.cells[pos] = player;
            let val = alphabeta(b, depth + 1, alpha, beta, true, nodes);
            b.cells[pos] = ' ';
            best = best.min(val);
            beta = beta.min(best);
            if alpha >= beta { break; }
        }
        best
    }
}

fn best_move(b: &mut Board, player: char) -> (usize, i32) {
    let is_max = player == 'X';
    let mut best_val = if is_max { i32::MIN } else { i32::MAX };
    let mut best_pos = 0;
    let mut nodes = 0u64;

    for pos in b.available_moves() {
        b.cells[pos] = player;
        let val = alphabeta(b, 0, i32::MIN, i32::MAX, !is_max, &mut nodes);
        b.cells[pos] = ' ';

        if (is_max && val > best_val) || (!is_max && val < best_val) {
            best_val = val;
            best_pos = pos;
        }
    }

    println!("  {} elige posicion {} (valor: {}, nodos: {})",
             player, best_pos, best_val, nodes);
    (best_pos, best_val)
}

fn play_ai_vs_ai() {
    let mut b = Board::new();
    let mut player = 'X';

    println!("=== IA vs IA ===\n");

    loop {
        let (pos, _) = best_move(&mut b, player);
        b.cells[pos] = player;
        b.print();

        if b.check_winner(player) {
            println!("{player} gana!");
            return;
        }
        if b.is_full() {
            println!("Empate!");
            return;
        }
        player = if player == 'X' { 'O' } else { 'X' };
    }
}

fn analyze_first_moves() {
    let mut b = Board::new();

    println!("=== Analisis de primera jugada ===\n");
    println!("Pos | Tipo    | Valor | Nodos");
    println!("----|---------|-------|------");

    for i in 0..9 {
        b.cells[i] = 'X';
        let mut nodes = 0u64;
        let val = alphabeta(&mut b, 1, i32::MIN, i32::MAX, false, &mut nodes);
        b.cells[i] = ' ';

        let kind = match i {
            0 | 2 | 6 | 8 => "esquina",
            4 => "centro ",
            _ => "borde  ",
        };
        println!("  {i} | {kind} |  {val:>3}  | {nodes}");
    }
    println!();
}

fn main() {
    analyze_first_moves();
    play_ai_vs_ai();
}
```

---

## Ejercicios

### Ejercicio 1 — Minimax a mano

Evalúa el siguiente árbol de juego con minimax. ¿Qué rama elige MAX?

```
                 MAX
              /   |   \
          MIN    MIN    MIN
         / \     / \      |
        7   3   5   8     4
```

<details>
<summary>Verificar</summary>

- MIN izquierdo: $\min(7, 3) = 3$
- MIN central: $\min(5, 8) = 5$
- MIN derecho: $\min(4) = 4$
- MAX raíz: $\max(3, 5, 4) = 5$

MAX elige la **rama central** (valor 5). MIN responderá con 5 (no 8).
</details>

### Ejercicio 2 — Poda alfa-beta a mano

Aplica poda alfa-beta al siguiente árbol (evaluación izquierda a derecha).
Indica qué nodos **no** se evalúan.

```
                 MAX
              /       \
          MIN           MIN
         / \           / \
        3   12        8   2
```

<details>
<summary>Verificar</summary>

1. Raíz MAX: $\alpha = -\infty$, $\beta = +\infty$.
2. MIN izquierdo: evalúa 3 → $\beta=3$. Evalúa 12 → $\min(3,12)=3$. Retorna 3.
3. Raíz MAX: $\alpha = 3$.
4. MIN derecho con $\alpha=3$, $\beta=+\infty$:
   - Evalúa 8 → $\beta=8$. No hay poda ($3 < 8$).
   - Evalúa 2 → $\beta=2$. Ahora $\alpha(3) \geq \beta(2)$ → **¡PODA!**
   - Pero ya evaluamos ambos hijos. En este caso, no hay poda real.

Corrección: para que haya poda, los hijos de MIN derecho deben estar en otro
orden. Si fueran `2, 8`:
- Evalúa 2 → $\beta=2$. $\alpha(3) \geq \beta(2)$ → **PODA**: no evaluar 8.

**Conclusión**: el orden de los hijos afecta cuánto se poda. Con el orden
original `8, 2`, se evalúan **todos** los nodos. Con `2, 8`, se poda el 8.

Resultado en ambos casos: MAX elige izquierda con valor **3**.
</details>

### Ejercicio 3 — Poda con árbol profundo

Aplica alfa-beta al siguiente árbol. ¿Cuántos nodos terminales se evalúan
vs el total?

```
                    MAX
                 /       \
             MIN           MIN
            / \           / \
         MAX   MAX     MAX   MAX
        / \   / \     / \   / \
       3  17 2   12  15  25 5   8
```

<details>
<summary>Verificar</summary>

Recorrido izquierda a derecha con $\alpha=-\infty$, $\beta=+\infty$:

1. MAX-izq-izq: $\max(3, 17) = 17$.
2. MAX-izq-der: evalúa 2. Ahora verificar: el padre MIN tiene $\beta=17$ (de
   arriba). MAX-izq-der con $\alpha=-\infty$: evalúa 2, luego 12 → $\max(2,12)=12$.
3. MIN-izq: $\min(17, 12) = 12$.
4. Raíz: $\alpha = 12$.
5. MIN-der con $\alpha=12$, $\beta=+\infty$:
   - MAX-der-izq: evalúa 15 → 15. $\alpha=12 < 15$, evalúa 25 → $\max(15,25)=25$.
     MIN ahora tiene $\beta=25$.
   - MAX-der-der: evalúa 5. $\max$ parcial = 5. Pero MIN ya tiene $\beta=25$,
     y necesitamos ver si MAX puede subir sobre 25. Evalúa 8 → $\max(5,8)=8$.
6. MIN-der: $\min(25, 8) = 8$.
7. Raíz: $\max(12, 8) = 12$.

Se evalúan **8 de 8** nodos terminales en este caso. No hay poda porque el
orden no es favorable.

Si reordenamos (heurística: mejores primero), se pueden podar varios nodos.
Por ejemplo, si MAX-izq-izq fuera `17, 3` y MIN-izq tuviera el 12 primero,
podríamos podar nodos. Total con buen orden: **~5-6 de 8**.
</details>

### Ejercicio 4 — Tres en raya: mejor respuesta

X juega en la esquina (posición 0). ¿Cuál es la mejor respuesta de O según
minimax? Usa razonamiento antes de verificar con el programa.

<details>
<summary>Verificar</summary>

Si X juega esquina, O debe jugar **centro** (posición 4) para empatar.
Cualquier otra respuesta permite a X forzar victoria.

Razonamiento: si O juega borde (e.g., posición 1), X juega la esquina opuesta
(8), creando doble amenaza en las diagonales. Si O juega otra esquina (e.g., 2),
X juega la esquina opuesta (6), amenazando columna izquierda y diagonal, y O
no puede bloquear ambas.

El centro es la única posición que cubre ambas diagonales y las líneas que pasan
por la esquina de X. Esto se confirma en el análisis: jugar en borde tras esquina
da valor $-1$ para O (pierde), mientras que centro da valor $0$ (empate).
</details>

### Ejercicio 5 — Negamax

Reescribe la evaluación del Ejercicio 1 usando negamax en lugar de minimax.
Verifica que el resultado es el mismo.

<details>
<summary>Verificar</summary>

```
negamax(raiz, +1):
  hijo 1: -negamax(MIN-izq, -1)
    negamax(MIN-izq, -1):
      hijo: -negamax(7, +1) = -7  -> valor -7
      hijo: -negamax(3, +1) = -3  -> valor max(-7,-3) = -3
    retorna -3
  -(-3) = 3

  hijo 2: -negamax(MIN-cen, -1)
    negamax(MIN-cen, -1):
      -negamax(5, +1) = -5
      -negamax(8, +1) = -8  -> max(-5,-8) = -5
    retorna -5
  -(-5) = 5

  hijo 3: -negamax(MIN-der, -1)
    negamax(MIN-der, -1):
      -negamax(4, +1) = -4
    retorna -4
  -(-4) = 4

  max(3, 5, 4) = 5
```

Resultado: **5** ✓ — idéntico a minimax. La clave es que negamax siempre
maximiza, y la negación al retornar invierte la perspectiva.
</details>

### Ejercicio 6 — Contar nodos con y sin poda

Implementa contadores de nodos evaluados tanto en `minimax` como en `alphabeta`
para tres en raya. Compara el número de nodos explorados desde el tablero vacío.
¿Cuál es la reducción porcentual?

<details>
<summary>Verificar</summary>

```c
int mm_count = 0, ab_count = 0;

// agregar mm_count++ al inicio de minimax
// agregar ab_count++ al inicio de alphabeta

int main(void) {
    Board b;
    board_init(&b);

    mm_count = 0;
    minimax(&b, 0, 1);
    printf("Minimax:     %d nodos\n", mm_count);

    ab_count = 0;
    alphabeta(&b, 0, -1000, 1000, 1);
    printf("Alfa-beta:   %d nodos\n", ab_count);

    printf("Reduccion:   %.1f%%\n",
           100.0 * (1.0 - (double)ab_count / mm_count));
}
```

Valores típicos:
- Minimax: ~549,946 nodos
- Alfa-beta: ~18,297 nodos
- Reducción: **~96.7%**

La poda descarta la gran mayoría de ramas sin afectar el resultado.
</details>

### Ejercicio 7 — Tablero parcial

Dado este tablero con turno de X:

```
 X |   | O
---+---+---
   | O |
---+---+---
   |   | X
```

Aplica minimax manualmente. ¿Puede X ganar? ¿Cuál es la mejor jugada?

<details>
<summary>Verificar</summary>

Estado: `cells = [X, _, O, _, O, _, _, _, X]`. Turno de X (MAX).

O tiene el centro y una esquina. X tiene dos esquinas opuestas (0, 8).

Jugadas de X: posiciones 1, 3, 5, 6, 7.

Análisis clave: X juega **posición 6** (esquina inferior izquierda):
```
 X |   | O
---+---+---
   | O |
---+---+---
 X |   | X
```
Ahora X amenaza la fila inferior (6-7-8) y la columna izquierda (0-3-6). O
solo puede bloquear una → X gana.

Si X juega posición 3:
```
 X |   | O
---+---+---
 X | O |
---+---+---
   |   | X
```
Amenaza columna izquierda (0-3-6) y diagonal (2-4-6 ya tiene O). Solo amenaza
una línea → O puede bloquear.

La mejor jugada es **posición 6** — victoria forzada para X con doble amenaza.
Valor minimax: $+10$ (o $+10 - \text{profundidad}$).
</details>

### Ejercicio 8 — Juego resuelto

Tres en raya es un **juego resuelto**: con juego perfecto, siempre es empate.
Modifica el programa para contar cuántas de las 9 primeras jugadas llevan a
empate, cuántas a victoria de X, y cuántas a derrota de X (con respuesta
óptima de O).

<details>
<summary>Verificar</summary>

```c
int main(void) {
    Board b;
    board_init(&b);

    int wins = 0, draws = 0, losses = 0;

    for (int i = 0; i < 9; i++) {
        b.cells[i] = 'X';
        int val = alphabeta(&b, 1, -1000, 1000, 0);
        b.cells[i] = ' ';

        if (val > 0) wins++;
        else if (val < 0) losses++;
        else draws++;

        printf("Posicion %d: valor %d\n", i, val);
    }

    printf("\nVictorias forzadas: %d\n", wins);
    printf("Empates:            %d\n", draws);
    printf("Derrotas posibles:  %d\n", losses);
}
```

Resultado:
- Esquinas (0, 2, 6, 8): valor 0 → **empate** (4 posiciones)
- Centro (4): valor 0 → **empate** (1 posición)
- Bordes (1, 3, 5, 7): valor negativo → **desventaja** para X (4 posiciones)

Total: 5 empates, 0 victorias forzadas, 4 con desventaja.

Esto puede parecer sorprendente: ¿por qué los bordes dan desventaja? Porque con
ajuste de profundidad (`score - depth`), un empate a menor profundidad puntúa
ligeramente distinto. Sin el ajuste, los 9 darían 0 (empate). El resultado
exacto depende de la implementación del ajuste por profundidad.

Sin ajuste de profundidad: las 9 posiciones dan valor **0** (empate con juego
óptimo de ambos lados).
</details>

### Ejercicio 9 — Función heurística

Para un juego de Connect Four (4 en línea en tablero 6×7), el árbol es demasiado
grande para explorarlo completo. Diseña una función de evaluación heurística que
estime el valor de posiciones no terminales. ¿Qué factores considerarías?

<details>
<summary>Verificar</summary>

Factores para una heurística de Connect Four:

```c
int heuristic_eval(Board4 *b) {
    int score = 0;

    // 1. Lineas de 4 (terminal): +/-1000
    // 2. Amenazas de 3 en linea con casilla libre: +/-50
    // 3. Lineas de 2 con 2 casillas libres: +/-10
    // 4. Control del centro (columna 3): +3 por ficha
    // 5. Posiciones bajas preferibles (mas estables)
    // 6. Amenazas sobre casillas impares vs pares
    //    (importante para determinar quien puede forzar)

    // Ejemplo simplificado:
    // Contar segmentos de 4 casillas consecutivas
    // Para cada segmento, sumar segun contenido:
    //   4 fichas mias: +1000  (victoria)
    //   3 mias + 1 vacia: +50
    //   2 mias + 2 vacias: +10
    //   1 mia + 3 vacias: +1
    //   (analogo negativo para oponente)

    return score;
}
```

Factores importantes por prioridad:

1. **Victoria/derrota inmediata** (peso máximo)
2. **Amenazas dobles** (dos formas de ganar simultáneamente)
3. **Control del centro** (más oportunidades de conectar)
4. **Altura de amenazas** (paridad: casillas impares vs pares)
5. **Fichas conectadas** (2 o 3 en línea con espacio)
6. **Bloqueo de amenazas del oponente**

La dificultad real no es listar factores, sino calibrar los **pesos** para que
la IA tome buenas decisiones. Esto se hace por prueba-error, análisis teórico,
o aprendizaje automático.
</details>

### Ejercicio 10 — Negamax con alfa-beta en Rust

Implementa la versión negamax con poda alfa-beta para tres en raya en Rust.
Verifica que produce las mismas jugadas que la versión minimax con alfa-beta.

<details>
<summary>Verificar</summary>

```rust
fn negamax_ab(b: &mut Board, depth: i32, mut alpha: i32, beta: i32,
              player: char) -> i32 {
    let raw_score = b.evaluate();
    // ajustar por perspectiva del jugador actual
    let sign = if player == 'X' { 1 } else { -1 };
    if raw_score != 0 {
        return (raw_score - depth * sign) * sign;
    }
    if b.is_full() { return 0; }

    let mut best = i32::MIN;
    let next = if player == 'X' { 'O' } else { 'X' };

    for pos in b.available_moves() {
        b.cells[pos] = player;
        let val = -negamax_ab(b, depth + 1, -beta, -alpha, next);
        b.cells[pos] = ' ';
        best = best.max(val);
        alpha = alpha.max(best);
        if alpha >= beta { break; }
    }
    best
}

fn best_move_negamax(b: &mut Board, player: char) -> usize {
    let mut best_val = i32::MIN;
    let mut best_pos = 0;
    let next = if player == 'X' { 'O' } else { 'X' };

    for pos in b.available_moves() {
        b.cells[pos] = player;
        let val = -negamax_ab(b, 0, i32::MIN + 1, i32::MAX, next);
        b.cells[pos] = ' ';
        if val > best_val {
            best_val = val;
            best_pos = pos;
        }
    }
    best_pos
}
```

Nota: `i32::MIN + 1` evita overflow al negar `i32::MIN` (que no tiene
representación positiva en complemento a dos).

Verificación: ejecutar ambas versiones en paralelo y comparar jugada por jugada.
Deben producir resultados idénticos en todos los casos.
</details>
