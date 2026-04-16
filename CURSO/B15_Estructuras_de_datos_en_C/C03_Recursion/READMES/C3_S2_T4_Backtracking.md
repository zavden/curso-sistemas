# Backtracking

## La idea central

Muchos problemas piden encontrar una configuración que satisfaga un conjunto de
restricciones: colocar reinas sin que se ataquen, encontrar un camino en un
laberinto, llenar un Sudoku.  Todos comparten una estructura:

1. **Elegir** — tomar una decisión parcial (colocar una reina, avanzar en una
   dirección).
2. **Explorar** — recurrir para resolver el subproblema restante con esa
   decisión ya tomada.
3. **Retroceder** (*backtrack*) — si la exploración falla, **deshacer** la
   decisión y probar la siguiente alternativa.

Este esquema se llama **backtracking** (prueba y retroceso).  La recursión lo
implementa naturalmente: cada nivel del árbol de llamadas representa una
decisión, y el retorno automático de la función deshace esa decisión sin código
adicional.

---

## Espacio de soluciones como árbol

Cada problema de backtracking puede visualizarse como un **árbol de decisiones**
(o árbol de búsqueda):

```
                         (vacío)
                       /    |    \
                    d₁     d₂    d₃       ← decisión 1
                   / \     / \    / \
                 d₁  d₂  d₁ d₂  d₁ d₂    ← decisión 2
                ...  ... ... ... ... ...
```

- **Raíz**: estado inicial (ninguna decisión tomada).
- **Nodos internos**: estados parciales.
- **Hojas**: estados finales — pueden ser **soluciones** (satisfacen todas las
  restricciones) o **callejones sin salida** (violan alguna restricción).
- **Aristas**: decisiones posibles desde un estado.

El backtracking recorre este árbol en **profundidad** (DFS).  La recursión
desciende por una rama; al fallar, retrocede y prueba la siguiente.  La clave
de la eficiencia es la **poda**: no descender por ramas que ya se sabe que
violarán una restricción.

---

## Esquema general

### Pseudocódigo

```
funcion backtrack(estado):
    si es_solucion(estado):
        reportar(estado)
        return verdadero          // o seguir buscando

    para cada decision en candidatos(estado):
        si es_valida(decision, estado):    // PODA
            aplicar(decision, estado)
            si backtrack(estado):
                return verdadero
            deshacer(decision, estado)     // BACKTRACK

    return falso                           // callejón sin salida
```

### Los cuatro componentes

| Componente | Pregunta | Ejemplo (N-reinas) |
|-----------|----------|---------------------|
| `es_solucion` | ¿Está completa la configuración? | ¿Se colocaron las N reinas? |
| `candidatos` | ¿Qué decisiones puedo tomar? | Columnas 0..N-1 para la fila actual |
| `es_valida` | ¿Esta decisión viola alguna restricción? | ¿Alguna reina existente ataca esta posición? |
| `deshacer` | ¿Cómo revierto la decisión? | Quitar la reina de la posición |

La **poda** (`es_valida`) es lo que distingue al backtracking de la fuerza
bruta exhaustiva.  Si podamos pronto, evitamos explorar subárboles enteros.

---

## El retroceso automático de la recursión

¿Por qué la recursión es ideal para backtracking?  Porque el **estado local**
de cada marco de pila se preserva y restaura automáticamente:

```
backtrack(fila=0):
    colocar reina en (0, col=2)
    backtrack(fila=1):
        colocar reina en (1, col=0)
        backtrack(fila=2):
            ninguna columna es válida → return false
        deshacer (1, 0)              ← automático al retornar
        colocar reina en (1, col=3)
        backtrack(fila=2):
            ...
```

Cada llamada recursiva «recuerda» su estado.  Al retornar, el marco anterior
sigue intacto con la decisión que tomó.  Esto es exactamente el mismo mecanismo
de la pila de llamadas que estudiamos en T02.

Si usáramos iteración, tendríamos que guardar y restaurar el estado
manualmente con una pila explícita — posible, pero mucho más propenso a
errores (visto en C03/S01/T03).

---

## Problema 1: N-reinas

### Enunciado

Colocar $N$ reinas en un tablero de $N \times N$ de modo que ninguna reina
ataque a otra.  Una reina ataca en fila, columna y ambas diagonales.

### Estrategia

Trabajamos **fila por fila**: en la fila $r$ probamos cada columna $c \in
[0, N)$.  Como colocamos exactamente una reina por fila, los conflictos de fila
quedan eliminados.  Solo verificamos:

- **Columna**: ninguna reina previa en columna $c$.
- **Diagonal principal** ($\nwarrow \searrow$): celdas con igual $r - c$.
- **Diagonal secundaria** ($\nearrow \swarrow$): celdas con igual $r + c$.

### Representación

Un array `cols[r] = c` indica que la reina de la fila $r$ está en la columna
$c$.  Para verificar conflictos rápidamente, usamos tres arrays booleanos:

- `col_used[c]` — columna $c$ ocupada.
- `diag1[r - c + N - 1]` — diagonal principal (el offset `+N-1` evita índices
  negativos).
- `diag2[r + c]` — diagonal secundaria.

### Implementación en C

```c
#include <stdio.h>
#include <stdbool.h>

#define MAX_N 20

static int cols[MAX_N];
static bool col_used[MAX_N];
static bool diag1[2 * MAX_N];    /* r - c + N - 1 */
static bool diag2[2 * MAX_N];    /* r + c          */
static int solution_count;

void print_board(int n) {
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            printf("%c ", cols[r] == c ? 'Q' : '.');
        }
        printf("\n");
    }
    printf("\n");
}

void solve(int row, int n) {
    if (row == n) {
        solution_count++;
        if (n <= 8) print_board(n);   /* solo imprimir tableros pequeños */
        return;
    }

    for (int c = 0; c < n; c++) {
        if (col_used[c] || diag1[row - c + n - 1] || diag2[row + c]) {
            continue;   /* PODA: posición atacada */
        }

        /* Aplicar decisión */
        cols[row] = c;
        col_used[c] = true;
        diag1[row - c + n - 1] = true;
        diag2[row + c] = true;

        solve(row + 1, n);

        /* Deshacer decisión (BACKTRACK) */
        col_used[c] = false;
        diag1[row - c + n - 1] = false;
        diag2[row + c] = false;
    }
}

int main(void) {
    int n = 8;
    solution_count = 0;
    solve(0, n);
    printf("N=%d: %d soluciones\n", n, solution_count);
    return 0;
}
```

### Implementación en Rust

```rust
struct Board {
    n: usize,
    cols: Vec<usize>,
    col_used: Vec<bool>,
    diag1: Vec<bool>,          // r - c + n - 1
    diag2: Vec<bool>,          // r + c
}

impl Board {
    fn new(n: usize) -> Self {
        Board {
            n,
            cols: vec![0; n],
            col_used: vec![false; n],
            diag1: vec![false; 2 * n],
            diag2: vec![false; 2 * n],
        }
    }

    fn print(&self) {
        for r in 0..self.n {
            for c in 0..self.n {
                print!("{} ", if self.cols[r] == c { 'Q' } else { '.' });
            }
            println!();
        }
        println!();
    }

    fn solve(&mut self, row: usize) -> u32 {
        if row == self.n {
            if self.n <= 8 { self.print(); }
            return 1;
        }

        let mut count = 0;
        for c in 0..self.n {
            let d1 = row + self.n - 1 - c;   // evitar underflow de usize
            let d2 = row + c;

            if self.col_used[c] || self.diag1[d1] || self.diag2[d2] {
                continue;   // PODA
            }

            // Aplicar
            self.cols[row] = c;
            self.col_used[c] = true;
            self.diag1[d1] = true;
            self.diag2[d2] = true;

            count += self.solve(row + 1);

            // Deshacer
            self.col_used[c] = false;
            self.diag1[d1] = false;
            self.diag2[d2] = false;
        }
        count
    }
}

fn main() {
    let n = 8;
    let mut board = Board::new(n);
    let count = board.solve(0);
    println!("N={n}: {count} soluciones");
}
```

### Traza para N = 4

$N = 4$ tiene 2 soluciones.  Traza abreviada del árbol de búsqueda:

```
fila 0, col 0:  Q . . .     col_used=[0], diag1=[3], diag2=[0]
  fila 1, col 0: atacada (col)
  fila 1, col 1: atacada (diag2: 0+0=0, 1+1=2? No. diag1: 1-1+3=3? Sí)
  fila 1, col 2: válida
    Q . . .
    . . Q .
    fila 2, col 0: atacada (diag2: 0+0=0, ya marcado)
    fila 2, col 1: atacada (diag1: 2-1+3=4, libre. diag2: 2+1=3, libre.
                    col_used[1]=false... verificar diag: 2-1=1, 1-2=-1 → |diff|=1
                    → atacada por reina en (1,2) vía diagonal)
    fila 2, col 2: atacada (col)
    fila 2, col 3: atacada (diag: |2-1|=1, |3-2|=1 → sí, atacada)
    → retrocede de fila 2 sin solución
  deshacer (1, 2)
  fila 1, col 3: válida
    Q . . .
    . . . Q
    fila 2, col 0: atacada (diag: |2-0|=2, |0-0|=0, diferente → libre?
                    col_used[0]=true → atacada por col)
    fila 2, col 1: válida
      Q . . .
      . . . Q
      . Q . .
      fila 3, col 0: atacada (diag: |3-0|=3, |0-1|=1, no; pero diag2: 3+0=3,
                      1+3=4, 2+1=3 → atacada)
      fila 3, col 1: atacada (col)
      fila 3, col 2: atacada (diag: |3-1|=2, |2-3|=1 → no por esa.
                      Verificar (0,0): |3-0|=3,|2-0|=2 → no.
                      (1,3): |3-1|=2,|2-3|=1 → no. OK en diags.
                      col_used[2]=false → ¡VÁLIDA!)

      ¡SOLUCIÓN 1!
      Q . . .          fila 0, col 0
      . . . Q          fila 1, col 3
      . Q . .          fila 2, col 1
      . . Q .          fila 3, col 2
```

(La segunda solución se encuentra al retroceder completamente a fila 0, col 1.)

### Número de soluciones

| $N$ | Soluciones | Nodos explorados (aprox.) |
|-----|-----------|---------------------------|
| 1 | 1 | 1 |
| 2 | 0 | 2 |
| 3 | 0 | 5 |
| 4 | 2 | 16 |
| 5 | 10 | 53 |
| 8 | 92 | 1,965 |
| 10 | 724 | ~20,000 |
| 12 | 14,200 | ~400,000 |

Sin poda (fuerza bruta): se explorarían $N^N$ combinaciones ($8^8 \approx 16$
millones para $N=8$).  Con poda: solo ~2000 nodos.  La poda reduce el espacio
de búsqueda en varios órdenes de magnitud.

### Complejidad

No existe una fórmula cerrada para el número de nodos explorados.  El peor caso
sigue siendo exponencial — el backtracking no convierte un problema $NP$ en
polinómico — pero la poda hace que el caso práctico sea mucho más rápido que la
fuerza bruta.

Profundidad de la recursión: exactamente $N$ (una llamada por fila).  El stack
no es un problema incluso para $N$ grande.

---

## Problema 2: Laberintos

### Enunciado

Dado un laberinto representado como una grilla de celdas libres (`.`) y muros
(`#`), encontrar un camino desde la entrada $(r_s, c_s)$ hasta la salida
$(r_e, c_e)$.

### Estrategia

Desde la posición actual, probar cada dirección (arriba, abajo, izquierda,
derecha).  Marcar la celda como visitada para no volver a ella (evitar ciclos).
Si ninguna dirección lleva a la salida, desmarcar y retroceder.

### Implementación en C

```c
#include <stdio.h>
#include <stdbool.h>

#define ROWS 6
#define COLS 8

static char maze[ROWS][COLS + 1] = {
    "########",
    "#S.#...#",
    "#.##.#.#",
    "#....#.#",
    "#.##...E",
    "########",
};

static bool visited[ROWS][COLS];

/* Direcciones: arriba, abajo, izquierda, derecha */
static const int dr[] = {-1, 1, 0, 0};
static const int dc[] = {0, 0, -1, 1};

bool solve_maze(int r, int c) {
    /* Fuera de límites o muro */
    if (r < 0 || r >= ROWS || c < 0 || c >= COLS) return false;
    if (maze[r][c] == '#') return false;
    if (visited[r][c]) return false;

    /* ¿Llegamos a la salida? */
    if (maze[r][c] == 'E') return true;

    /* Marcar como visitado */
    visited[r][c] = true;

    /* Probar cada dirección */
    for (int d = 0; d < 4; d++) {
        if (solve_maze(r + dr[d], c + dc[d])) {
            maze[r][c] = '*';   /* marcar camino en la solución */
            return true;
        }
    }

    /* Ninguna dirección funcionó → deshacer */
    visited[r][c] = false;    /* BACKTRACK */
    return false;
}

int main(void) {
    solve_maze(1, 1);   /* posición de 'S' */
    maze[1][1] = 'S';   /* restaurar 'S' (fue sobreescrito por '*') */

    for (int r = 0; r < ROWS; r++) {
        printf("%s\n", maze[r]);
    }
    return 0;
}
```

Salida esperada (una de las posibles soluciones):

```
########
#S.#...#
#*##.#.#
#*...#.#
#*##***E
########
```

### Implementación en Rust

```rust
const ROWS: usize = 6;
const COLS: usize = 8;

fn solve_maze(maze: &mut Vec<Vec<u8>>, visited: &mut Vec<Vec<bool>>,
              r: isize, c: isize) -> bool {
    if r < 0 || r >= ROWS as isize || c < 0 || c >= COLS as isize {
        return false;
    }
    let (ru, cu) = (r as usize, c as usize);

    if maze[ru][cu] == b'#' || visited[ru][cu] {
        return false;
    }
    if maze[ru][cu] == b'E' {
        return true;
    }

    visited[ru][cu] = true;

    let directions: [(isize, isize); 4] = [(-1,0), (1,0), (0,-1), (0,1)];
    for (dr, dc) in &directions {
        if solve_maze(maze, visited, r + dr, c + dc) {
            maze[ru][cu] = b'*';
            return true;
        }
    }

    visited[ru][cu] = false;   // BACKTRACK
    false
}

fn main() {
    let grid = [
        b"########",
        b"#S.#...#",
        b"#.##.#.#",
        b"#....#.#",
        b"#.##...E",
        b"########",
    ];
    let mut maze: Vec<Vec<u8>> = grid.iter().map(|row| row.to_vec()).collect();
    let mut visited = vec![vec![false; COLS]; ROWS];

    if solve_maze(&mut maze, &mut visited, 1, 1) {
        maze[1][1] = b'S';
        for row in &maze {
            println!("{}", std::str::from_utf8(row).unwrap());
        }
    } else {
        println!("No hay solución");
    }
}
```

### El laberinto como árbol

Cada celda libre es un nodo.  Desde cada nodo se pueden explorar hasta 4
vecinos.  El array `visited` evita ciclos, convirtiendo el grafo en un árbol de
búsqueda:

```
(1,1) S
  ↓
(2,1)
  ↓
(3,1)
  → (3,2) → (3,3) → (3,4) → …
  ↓
(4,1)
  → …
```

La profundidad máxima de la recursión es el número de celdas libres (en el peor
caso, un camino que serpentea por todo el laberinto).  Para una grilla de
$R \times C$: profundidad máxima $R \cdot C$, lo cual es manejable.

---

## Poda: la clave de la eficiencia

### Sin poda (fuerza bruta)

Generar todas las configuraciones posibles y verificar al final:

- N-reinas sin poda: $N^N$ configuraciones (para $N=8$: 16,777,216).
- Laberinto sin poda: explorar todas las permutaciones de celdas.

### Con poda (backtracking)

Verificar restricciones **tan pronto como sea posible**, antes de explorar el
subárbol:

- N-reinas con poda: ~2000 nodos para $N=8$ (reducción $\times 8000$).
- Laberinto con poda: `visited` y muros eliminan ramas inmediatamente.

### Tipos de poda

| Tipo | Descripción | Ejemplo |
|------|-------------|---------|
| **Factibilidad** | La decisión actual ya viola una restricción | Reina en columna ocupada |
| **Anticipación** (*look-ahead*) | Se puede prever que ninguna extensión funcionará | Si quedan menos columnas libres que filas por llenar |
| **Simetría** | Eliminar configuraciones equivalentes por rotación/reflexión | Solo explorar la primera mitad de columnas en fila 0 |
| **Acotamiento** (*bound*) | En problemas de optimización, podar si el mejor caso del subárbol no mejora la solución actual | Branch and bound (no es backtracking puro) |

---

## Encontrar una vs todas las soluciones

El esquema base puede adaptarse:

### Una solución (terminar al encontrar la primera)

```c
bool solve(int row, int n) {
    if (row == n) return true;    /* encontrada */
    for (int c = 0; c < n; c++) {
        if (!valid(row, c)) continue;
        apply(row, c);
        if (solve(row + 1, n)) return true;   /* propagar éxito */
        undo(row, c);
    }
    return false;   /* no hay solución en esta rama */
}
```

### Todas las soluciones (explorar todo el árbol)

```c
void solve_all(int row, int n) {
    if (row == n) {
        count++;
        print_solution();
        return;       /* NO return true — seguir buscando */
    }
    for (int c = 0; c < n; c++) {
        if (!valid(row, c)) continue;
        apply(row, c);
        solve_all(row + 1, n);    /* no comprobar retorno */
        undo(row, c);
    }
}
```

La diferencia es mínima: solo cambia si el `return` propaga inmediatamente o
permite continuar.

---

## Backtracking y la pila de llamadas

Revisemos la conexión con temas anteriores de C03:

| Concepto (tópico previo) | Rol en backtracking |
|--------------------------|---------------------|
| **Pila de llamadas** (T02) | Cada nivel de decisión = un marco de pila |
| **Recursión vs iteración** (T03) | Backtracking iterativo requiere pila explícita con operaciones push/pop del estado completo |
| **Recursión de cola** (T04) | El backtracking **no** es recursión de cola — hay trabajo después de la llamada recursiva (el deshacer) |
| **Torres de Hanoi** (T02 S02) | Hanoi es recursión sin backtracking (no hay decisión ni fallo posible) |

El backtracking es fundamentalmente **no** optimizable con TCO: después de cada
llamada recursiva, la función necesita comprobar si tuvo éxito y, si no,
deshacer la decisión.  Esa operación post-llamada impide que sea una llamada de
cola.

---

## Complejidad del backtracking

### Tiempo

El peor caso depende del problema.  En general, si hay $b$ opciones en cada
nivel y la profundidad máxima es $d$:

- Sin poda: $O(b^d)$ — exponencial.
- Con poda: mejor en la práctica, pero el peor caso sigue siendo exponencial.

El backtracking no resuelve la exponencialidad inherente del problema (muchos
problemas de backtracking son $NP$-completos), pero la poda puede reducir
enormemente la constante.

### Espacio

La profundidad de la pila es $O(d)$.  Si el estado en cada nivel ocupa $O(s)$
bytes, el espacio total es $O(d \cdot s)$.

| Problema | $b$ (opciones) | $d$ (profundidad) | Espacio |
|----------|----------------|---------------------|---------|
| N-reinas | $N$ | $N$ | $O(N)$ |
| Laberinto $R \times C$ | 4 | $R \cdot C$ | $O(R \cdot C)$ |
| Sudoku | 9 | 81 (celdas vacías) | $O(81)$ — constante |

---

## Patrón en Rust: sin efectos laterales

En C, el backtracking típicamente modifica un estado global (o un struct) y lo
deshace manualmente.  En Rust se pueden explorar dos enfoques:

### Enfoque 1: Mutación con deshacer (igual que C)

```rust
fn solve(&mut self, row: usize) -> u32 {
    // ... aplicar ...
    let count = self.solve(row + 1);
    // ... deshacer ...
    count
}
```

Es eficiente pero requiere disciplina para deshacer correctamente.

### Enfoque 2: Copiar el estado (sin deshacer)

```rust
fn solve(state: &State, row: usize) -> u32 {
    // ...
    let mut new_state = state.clone();
    new_state.apply(row, c);
    count += solve(&new_state, row + 1);
    // No hay deshacer: new_state se destruye al salir del scope
    // ...
}
```

Más seguro (imposible olvidar el deshacer), pero el `clone()` en cada nivel
puede ser costoso.  Funciona bien cuando el estado es pequeño (ej. un array de
8 enteros para N-reinas) pero no para un laberinto grande.

### Enfoque 3: Acumulador inmutable (idiomático para recolectar soluciones)

```rust
fn solve(queens: &[usize], n: usize, solutions: &mut Vec<Vec<usize>>) {
    if queens.len() == n {
        solutions.push(queens.to_vec());
        return;
    }
    let row = queens.len();
    for c in 0..n {
        if is_valid(queens, row, c) {
            let mut next = queens.to_vec();
            next.push(c);
            solve(&next, n, solutions);
        }
    }
}
```

Aquí `queens` crece en cada nivel pero nunca se modifica — se crea una nueva
copia con el elemento añadido.  No hay operación de deshacer.

---

## Problema 3: Sudoku (preview)

El Sudoku es el ejemplo canónico de backtracking con restricciones múltiples.
Lo mencionamos como preview — los principios son los mismos:

1. **Elegir**: la siguiente celda vacía.
2. **Candidatos**: dígitos 1-9.
3. **Poda**: el dígito no puede estar ya en la fila, columna o bloque 3×3.
4. **Deshacer**: borrar el dígito y probar el siguiente.

```c
bool solve_sudoku(int board[9][9]) {
    /* Encontrar celda vacía */
    int r, c;
    if (!find_empty(board, &r, &c)) return true;  /* completo */

    for (int digit = 1; digit <= 9; digit++) {
        if (is_valid_placement(board, r, c, digit)) {
            board[r][c] = digit;
            if (solve_sudoku(board)) return true;
            board[r][c] = 0;    /* BACKTRACK */
        }
    }
    return false;
}
```

Profundidad máxima: 81 (celdas en el tablero).  En la práctica, un Sudoku
estándar se resuelve en milisegundos con backtracking y poda básica.

---

## Errores comunes

### 1. Olvidar deshacer

```c
/* MAL */
apply(row, c);
solve(row + 1, n);
/* falta undo(row, c) — el estado queda corrompido */
```

Consecuencia: las ramas posteriores ven un estado incorrecto.  El programa
puede dar soluciones erróneas o no encontrar ninguna.

### 2. No comprobar el retorno (cuando solo se busca una solución)

```c
/* MAL */
apply(row, c);
solve(row + 1, n);   /* ignoramos si encontró solución */
undo(row, c);
```

El programa seguirá buscando incluso después de encontrar una solución, y el
deshacer borrará la solución encontrada.

### 3. Olvidar marcar visitado (laberintos)

Sin `visited[r][c] = true`, la búsqueda entra en un ciclo infinito entre dos
celdas adyacentes hasta desbordar la pila.

### 4. Deshacer en exceso

```c
/* MAL — deshace incluso cuando tuvo éxito */
apply(row, c);
if (solve(row + 1, n)) {
    undo(row, c);    /* ¡NO! La solución necesita esta decisión */
    return true;
}
undo(row, c);
```

### 5. Poda insuficiente

Comprobar restricciones solo al final (cuando `row == n`) en lugar de en cada
paso convierte el backtracking en fuerza bruta.

---

## Ejercicios

### Ejercicio 1 — N-reinas para N = 4

Traza manualmente el algoritmo de N-reinas para $N = 4$.  Muestra el árbol de
búsqueda completo: qué columna se prueba en cada fila, cuándo se poda, y
cuándo se retrocede.  Dibuja las 2 soluciones.

<details>
<summary>Predice las posiciones de las soluciones</summary>

Solución 1: `cols = [1, 3, 0, 2]` — reinas en (0,1), (1,3), (2,0), (3,2).
Solución 2: `cols = [2, 0, 3, 1]` — reinas en (0,2), (1,0), (2,3), (3,1).
Son reflejos una de la otra.  El árbol explora 16 nodos (de los $4^4 = 256$
posibles sin poda).
</details>

### Ejercicio 2 — Laberinto propio

Diseña un laberinto de al menos 8×8 con un camino único de S a E.  Implementa
la solución con backtracking en C o Rust y verifica que encuentra el camino
correcto.  Luego modifica el laberinto para que no tenga solución y verifica que
el programa reporta "sin solución".

<details>
<summary>Pista para laberinto sin solución</summary>

Rodea la E con muros por todos los lados, o crea una pared continua que divida
el laberinto en dos zonas desconectadas.  El backtracking explorará todas las
celdas accesibles y retornará `false`.
</details>

### Ejercicio 3 — Contar nodos explorados

Modifica la implementación de N-reinas (C o Rust) para contar el número total
de nodos visitados (cada vez que se entra en `solve`, incrementar un contador).
Compara el conteo para $N = 4, 6, 8, 10, 12$ contra $N^N$ (fuerza bruta sin
poda).

<details>
<summary>Valores aproximados</summary>

| $N$ | Nodos explorados | $N^N$ | Reducción |
|-----|-----------------|-------|-----------|
| 4 | 17 | 256 | $\times 15$ |
| 8 | ~2,000 | 16M | $\times 8,000$ |
| 12 | ~400,000 | 8.9×$10^{12}$ | $\times 22M$ |

La poda es cada vez más efectiva a medida que $N$ crece.
</details>

### Ejercicio 4 — N-reinas: una sola solución

Modifica el programa para que se detenga después de encontrar la primera
solución.  Compara el número de nodos explorados con la versión que busca todas.
Para $N = 8$, ¿cuántos nodos explora antes de la primera solución?

<details>
<summary>Predice la diferencia</summary>

La primera solución para $N = 8$ se encuentra mucho antes de explorar todo el
árbol — típicamente en unas ~100 llamadas.  La versión "todas" explora ~2000.
La diferencia es más dramática para $N$ grande: la primera solución de $N = 12$
se encuentra rápido, pero buscar las 14,200 soluciones toma mucho más.
</details>

### Ejercicio 5 — Sudoku solver

Implementa un solver de Sudoku por backtracking en C o Rust.  Usa el tablero
de ejemplo:

```
5 3 0 | 0 7 0 | 0 0 0
6 0 0 | 1 9 5 | 0 0 0
0 9 8 | 0 0 0 | 0 6 0
------+-------+------
8 0 0 | 0 6 0 | 0 0 3
4 0 0 | 8 0 3 | 0 0 1
7 0 0 | 0 2 0 | 0 0 6
------+-------+------
0 6 0 | 0 0 0 | 2 8 0
0 0 0 | 4 1 9 | 0 0 5
0 0 0 | 0 8 0 | 0 7 9
```

(Los 0 son celdas vacías.)

<details>
<summary>Pista de implementación</summary>

`find_empty` recorre el tablero buscando el primer 0.  `is_valid_placement`
verifica fila, columna y bloque 3×3 (bloque: `(r/3)*3` a `(r/3)*3+2`, ídem
para columna).  Con 51 celdas vacías y poda efectiva, se resuelve en <1ms.
</details>

### Ejercicio 6 — Generar permutaciones

Usa backtracking para generar todas las permutaciones de los números $1..N$.
La decisión en cada nivel es qué número colocar; la poda es que no esté ya
usado.  Implementa en C y Rust.  Verifica que se generan $N!$ permutaciones.

<details>
<summary>Estructura del código</summary>

```c
void permutations(int arr[], bool used[], int pos, int n) {
    if (pos == n) { print(arr, n); return; }
    for (int i = 1; i <= n; i++) {
        if (used[i]) continue;
        arr[pos] = i;
        used[i] = true;
        permutations(arr, used, pos + 1, n);
        used[i] = false;
    }
}
```

Para $N = 4$: 24 permutaciones.  La estructura es idéntica a N-reinas pero sin
la restricción de diagonales.
</details>

### Ejercicio 7 — Subconjuntos que suman S

Dado un array de enteros positivos y un objetivo $S$, encontrar todos los
subconjuntos cuya suma sea exactamente $S$.  La decisión en cada nivel: incluir
o no incluir el elemento actual.  Poda: si la suma parcial ya excede $S$, no
seguir.

<details>
<summary>Predice el árbol de decisiones</summary>

Para `arr = [2, 3, 5, 7]` y $S = 10$: el árbol es binario (incluir/excluir),
profundidad 4.  Sin poda: $2^4 = 16$ hojas.  Con poda: se eliminan ramas donde
la suma parcial > 10.  Soluciones: `{3, 7}` y `{2, 3, 5}`.
</details>

### Ejercicio 8 — Camino más corto (laberinto)

Modifica el solver de laberintos para encontrar el **camino más corto** (no
solo cualquier camino).  Pista: en lugar de retornar al encontrar el primer
camino, explora todas las ramas y guarda la longitud mínima.

<details>
<summary>Consideración importante</summary>

El backtracking con "mejor solución" es mucho más lento que BFS (búsqueda en
anchura), que encuentra el camino más corto directamente.  Este ejercicio
muestra una limitación del backtracking/DFS: es natural para encontrar
*cualquier* solución o *todas*, pero ineficiente para encontrar la *óptima* en
grafos no ponderados.  BFS se verá en el capítulo de colas (C04).
</details>

### Ejercicio 9 — Backtracking con pila explícita

Convierte el solver de N-reinas de recursivo a iterativo usando una pila
explícita.  El estado en cada nivel de la pila debe incluir la fila actual y la
siguiente columna a probar.  Verifica que produce las mismas soluciones.

<details>
<summary>Estructura sugerida</summary>

```c
typedef struct {
    int row;
    int next_col;   /* la siguiente columna a intentar */
} Frame;
```

El bucle: pop un frame, probar columnas desde `next_col`.  Si encuentra una
válida, push un frame actualizado (con `next_col+1` para el retry) y push un
nuevo frame para `row+1`.  Si no queda columna válida, deshacer la decisión del
nivel actual.  Es más complejo que la versión recursiva — ilustra por qué el
backtracking se prefiere recursivo.
</details>

### Ejercicio 10 — Knight's tour

El **recorrido del caballo**: mover un caballo de ajedrez por un tablero
$N \times N$ visitando cada casilla exactamente una vez.  Implementa con
backtracking.  El caballo tiene hasta 8 movimientos posibles desde cada casilla.

<details>
<summary>Pistas de implementación</summary>

Movimientos del caballo: `{(-2,-1),(-2,1),(-1,-2),(-1,2),(1,-2),(1,2),(2,-1),(2,1)}`.
Tablero: array `board[N][N]` inicializado en -1, donde `board[r][c] = k`
significa que la casilla fue visitada en el paso $k$.

Para $N = 5$: existe solución.  Para $N = 8$: existe pero puede tardar sin
heurística.  **Heurística de Warnsdorff**: en cada paso, elegir la casilla con
menos movimientos futuros (reduce enormemente el backtracking).  Sin esta
heurística, $N = 8$ puede tardar minutos; con ella, se resuelve al instante.
</details>
