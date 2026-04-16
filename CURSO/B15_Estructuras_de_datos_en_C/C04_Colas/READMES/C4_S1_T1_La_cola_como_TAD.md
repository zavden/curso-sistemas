# La cola como TAD

## FIFO: First In, First Out

Una **cola** (*queue*) es una colección lineal donde los elementos se insertan
por un extremo (el **final**, *rear*) y se eliminan por el otro (el **frente**,
*front*).  El primer elemento en entrar es el primero en salir: **FIFO**.

Contraste con la pila (LIFO):

| Aspecto | Pila (stack) | Cola (queue) |
|---------|-------------|--------------|
| Disciplina | LIFO | FIFO |
| Inserción | `push` — en el tope | `enqueue` — en el final |
| Eliminación | `pop` — del tope | `dequeue` — del frente |
| Acceso | Solo al tope | Solo al frente |
| Analogía | Pila de platos | Fila de personas |

### Analogía cotidiana

Una fila en un supermercado: el primero en llegar es el primero en ser atendido.
Los nuevos clientes se incorporan al final de la fila.  Nadie se salta la cola
(en una cola justa).

```
  frente                              final
    │                                   │
    ▼                                   ▼
┌───┬───┬───┬───┬───┐
│ A │ B │ C │ D │ E │  ──►  A sale primero (dequeue)
└───┴───┴───┴───┴───┘       E entró último (enqueue)
```

---

## Definición formal del TAD Cola

### Operaciones esenciales

| Operación | Firma (abstracta) | Descripción | Complejidad esperada |
|-----------|-------------------|-------------|---------------------|
| `enqueue(q, x)` | Cola × T → Cola | Inserta `x` al final | $O(1)$ |
| `dequeue(q)` | Cola → (T, Cola) | Elimina y retorna el elemento del frente | $O(1)$ |
| `front(q)` | Cola → T | Retorna el elemento del frente sin eliminarlo | $O(1)$ |
| `is_empty(q)` | Cola → Bool | Verdadero si la cola no tiene elementos | $O(1)$ |
| `size(q)` | Cola → Nat | Número de elementos | $O(1)$ |

Todas las operaciones deben ser $O(1)$.  Si `enqueue` o `dequeue` fueran
$O(n)$, la cola no estaría bien implementada.

### Operaciones auxiliares comunes

| Operación | Descripción |
|-----------|-------------|
| `create()` | Crear cola vacía |
| `destroy(q)` | Liberar memoria (C) |
| `is_full(q)` | Para implementaciones con capacidad fija |
| `clear(q)` | Vaciar la cola |

### Axiomas (comportamiento esperado)

Usando notación algebraica:

```
dequeue(enqueue(create(), x)) = (x, create())
front(enqueue(create(), x)) = x
is_empty(create()) = true
is_empty(enqueue(q, x)) = false
size(create()) = 0
size(enqueue(q, x)) = size(q) + 1
```

El axioma clave es el **orden FIFO**: si se encolan $a$ y luego $b$, el
`dequeue` retorna $a$ antes que $b$:

```
sea q = enqueue(enqueue(create(), a), b)
dequeue(q) = (a, enqueue(create(), b))
```

---

## Condiciones excepcionales

### Dequeue / front en cola vacía

Intentar extraer u observar el frente de una cola vacía es un error.  Las
estrategias son las mismas que vimos en la pila (C02):

| Estrategia | Mecanismo | Lenguaje típico |
|-----------|-----------|-----------------|
| **Precondición** | El llamador verifica `!is_empty()` antes de llamar | Documentación |
| **Valor centinela** | Retornar un valor especial (`-1`, `NULL`) | C (común pero frágil) |
| **Retorno de error** | Retornar un código de error por parámetro o struct | C (robusto) |
| **Option/Result** | `Option<T>` o `Result<T, E>` | Rust |
| **Abort** | `assert(!is_empty)` — terminar el programa | C (debug) |

En C, la convención más robusta es retornar un `bool` que indica éxito y
escribir el valor en un puntero de salida:

```c
bool queue_dequeue(Queue *q, int *out);   /* true si éxito */
```

En Rust, `Option<T>` es lo idiomático:

```rust
fn dequeue(&mut self) -> Option<T>;
```

### Enqueue en cola llena (capacidad fija)

Solo aplica a implementaciones con array de tamaño fijo.  Opciones:

| Estrategia | Comportamiento |
|-----------|---------------|
| Rechazar | Retornar error / `false` |
| Redimensionar | Asignar array más grande (amortizado $O(1)$) |
| Sobreescribir | Descartar el elemento más viejo (cola circular con overwrite — útil en buffers de log) |

---

## La cola como modelo de sistemas reales

La cola FIFO modela cualquier sistema con **llegadas** y **servicio** en orden:

| Sistema | Elemento | Enqueue | Dequeue |
|---------|----------|---------|---------|
| Impresiora | Documentos | Enviar a imprimir | Imprimir siguiente |
| SO: procesos | PCB (Process Control Block) | Proceso llega a ready | Planificador elige siguiente |
| Red: paquetes | Paquetes IP | Paquete llega al router | Router envía paquete |
| Web: requests | Peticiones HTTP | Llega petición | Worker la procesa |
| Supermercado | Clientes | Cliente llega a la fila | Cajero atiende |

La **teoría de colas** (*queueing theory*) es una rama de las matemáticas que
estudia estos sistemas.  Conceptos clave:

- **Tasa de llegada** ($\lambda$): elementos por unidad de tiempo.
- **Tasa de servicio** ($\mu$): elementos procesados por unidad de tiempo.
- **Utilización** ($\rho = \lambda / \mu$): si $\rho \geq 1$, la cola crece
  indefinidamente.
- **Tiempo de espera**: cuánto tiempo pasa un elemento en la cola antes de ser
  atendido.

Veremos simulación de estos sistemas en S02/T04.

---

## Implementaciones: vista general

Antes de profundizar en cada implementación (T02-T04), veamos el mapa:

| Implementación | Inserción | Eliminación | Espacio | Notas |
|---------------|-----------|-------------|---------|-------|
| **Array lineal** (naive) | $O(1)$ al final | $O(n)$ desplazar todo | Fijo | No usar — dequeue es $O(n)$ |
| **Array circular** | $O(1)$ | $O(1)$ | Fijo (o redimensionable) | La implementación estándar |
| **Lista enlazada** | $O(1)$ al rear | $O(1)$ del front | Dinámico | Overhead por nodo |
| **Dos pilas** | $O(1)$ amortizado | $O(1)$ amortizado | Dinámico | Truco elegante |

### Por qué el array lineal no funciona

Con un array lineal, `enqueue` añade al final ($O(1)$) y `dequeue` elimina
del inicio.  Pero eliminar del inicio requiere desplazar todos los elementos
una posición a la izquierda — $O(n)$:

```
Antes:     [A, B, C, D, _, _]    front=0, rear=3
dequeue:   [ , B, C, D, _, _]    ← vacío al inicio
Desplazar: [B, C, D, _, _, _]    front=0, rear=2   ← O(n)
```

Alternativa: no desplazar, solo avanzar `front`:

```
Antes:     [A, B, C, D, _, _]    front=0, rear=3
dequeue:   [_, B, C, D, _, _]    front=1, rear=3   ← O(1)
```

Pero ahora el espacio al inicio se desperdicia.  Después de muchas operaciones,
`front` y `rear` avanzan hasta el final del array, agotando el espacio aunque
hay huecos al principio.  La solución: **array circular** (T02).

### El truco de las dos pilas

Una cola puede implementarse con dos pilas (llamadas *inbox* y *outbox*):

- `enqueue`: push en *inbox*.
- `dequeue`: si *outbox* está vacía, volcar *inbox* completo en *outbox*
  (pop de inbox, push en outbox — esto invierte el orden).  Luego pop de
  *outbox*.

```
enqueue(A):  inbox=[A]    outbox=[]
enqueue(B):  inbox=[B,A]  outbox=[]
enqueue(C):  inbox=[C,B,A] outbox=[]

dequeue():   volcar inbox → outbox
             inbox=[]     outbox=[A,B,C]
             pop outbox → A ✓ (FIFO)

dequeue():   outbox no vacío, no volcar
             pop outbox → B ✓

enqueue(D):  inbox=[D]    outbox=[C]

dequeue():   outbox no vacío
             pop outbox → C ✓

dequeue():   outbox vacío, volcar inbox
             inbox=[]     outbox=[D]
             pop outbox → D ✓
```

Cada elemento se mueve exactamente 2 veces (una push a inbox, una push a
outbox), así que el costo amortizado por operación es $O(1)$.  Este truco es
importante en programación funcional, donde las pilas (listas) son la estructura
natural y las colas no.

### Implementación de dos pilas en C

```c
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#define CAPACITY 100

typedef struct {
    int data[CAPACITY];
    int top;
} Stack;

void stack_init(Stack *s) { s->top = -1; }
bool stack_empty(const Stack *s) { return s->top == -1; }
void stack_push(Stack *s, int val) { s->data[++s->top] = val; }
int  stack_pop(Stack *s) { return s->data[s->top--]; }

typedef struct {
    Stack inbox;
    Stack outbox;
} QueueTwoStacks;

void queue_init(QueueTwoStacks *q) {
    stack_init(&q->inbox);
    stack_init(&q->outbox);
}

void queue_enqueue(QueueTwoStacks *q, int val) {
    stack_push(&q->inbox, val);
}

bool queue_dequeue(QueueTwoStacks *q, int *out) {
    if (stack_empty(&q->outbox)) {
        if (stack_empty(&q->inbox)) return false;  /* cola vacía */
        /* Volcar inbox → outbox */
        while (!stack_empty(&q->inbox)) {
            stack_push(&q->outbox, stack_pop(&q->inbox));
        }
    }
    *out = stack_pop(&q->outbox);
    return true;
}

int main(void) {
    QueueTwoStacks q;
    queue_init(&q);

    queue_enqueue(&q, 10);
    queue_enqueue(&q, 20);
    queue_enqueue(&q, 30);

    int val;
    while (queue_dequeue(&q, &val)) {
        printf("%d ", val);   /* 10 20 30 — FIFO */
    }
    printf("\n");
    return 0;
}
```

### Implementación de dos pilas en Rust

```rust
struct QueueTwoStacks<T> {
    inbox: Vec<T>,
    outbox: Vec<T>,
}

impl<T> QueueTwoStacks<T> {
    fn new() -> Self {
        QueueTwoStacks { inbox: Vec::new(), outbox: Vec::new() }
    }

    fn enqueue(&mut self, val: T) {
        self.inbox.push(val);
    }

    fn dequeue(&mut self) -> Option<T> {
        if self.outbox.is_empty() {
            while let Some(val) = self.inbox.pop() {
                self.outbox.push(val);
            }
        }
        self.outbox.pop()
    }

    fn is_empty(&self) -> bool {
        self.inbox.is_empty() && self.outbox.is_empty()
    }

    fn size(&self) -> usize {
        self.inbox.len() + self.outbox.len()
    }
}

fn main() {
    let mut q = QueueTwoStacks::new();
    q.enqueue(10);
    q.enqueue(20);
    q.enqueue(30);

    while let Some(val) = q.dequeue() {
        print!("{val} ");   // 10 20 30
    }
    println!();
}
```

---

## BFS: la aplicación fundamental

La **búsqueda en anchura** (*Breadth-First Search*, BFS) es el algoritmo más
importante que usa colas.  Mientras que el backtracking (C03/T04) usa la pila
(DFS — profundidad primero), BFS explora todos los vecinos de un nodo antes de
pasar al siguiente nivel:

```
         1
        / \
       2   3
      / \   \
     4   5   6

DFS (pila): 1, 2, 4, 5, 3, 6     ← profundidad primero
BFS (cola): 1, 2, 3, 4, 5, 6     ← anchura primero (nivel por nivel)
```

### Pseudocódigo de BFS

```
funcion bfs(inicio):
    cola = crear_cola()
    visitado = conjunto vacío

    enqueue(cola, inicio)
    marcar inicio como visitado

    mientras cola no esté vacía:
        nodo = dequeue(cola)
        procesar(nodo)

        para cada vecino de nodo:
            si vecino no está visitado:
                marcar vecino como visitado
                enqueue(cola, vecino)
```

### BFS encuentra el camino más corto

En un grafo no ponderado, BFS garantiza encontrar el camino más corto (en
número de aristas) desde el inicio hasta cualquier nodo alcanzable.  Esto se
debe a que BFS explora por **niveles**: primero todos los nodos a distancia 1,
luego a distancia 2, etc.

Comparación con DFS en laberintos (referencia a C03/T04):

| Propiedad | DFS (pila / recursión) | BFS (cola) |
|-----------|----------------------|------------|
| Encuentra *un* camino | Sí | Sí |
| Camino más corto | No garantizado | **Sí** |
| Espacio | $O(d)$ — profundidad | $O(w)$ — ancho máximo |
| Estructura de datos | Pila (o recursión) | Cola |

### BFS en laberinto: C

```c
#include <stdio.h>
#include <stdbool.h>

#define ROWS 6
#define COLS 8
#define MAX_QUEUE (ROWS * COLS)

typedef struct { int r, c; } Pos;

static char maze[ROWS][COLS + 1] = {
    "########",
    "#S.#...#",
    "#.##.#.#",
    "#....#.#",
    "#.##...E",
    "########",
};

static const int dr[] = {-1, 1, 0, 0};
static const int dc[] = {0, 0, -1, 1};

void bfs(int sr, int sc) {
    bool visited[ROWS][COLS] = {{false}};
    Pos parent[ROWS][COLS];     /* para reconstruir el camino */
    Pos queue[MAX_QUEUE];
    int head = 0, tail = 0;

    visited[sr][sc] = true;
    queue[tail++] = (Pos){sr, sc};

    bool found = false;
    Pos end;

    while (head < tail && !found) {
        Pos cur = queue[head++];

        for (int d = 0; d < 4; d++) {
            int nr = cur.r + dr[d];
            int nc = cur.c + dc[d];

            if (nr < 0 || nr >= ROWS || nc < 0 || nc >= COLS) continue;
            if (maze[nr][nc] == '#' || visited[nr][nc]) continue;

            visited[nr][nc] = true;
            parent[nr][nc] = cur;
            queue[tail++] = (Pos){nr, nc};

            if (maze[nr][nc] == 'E') {
                found = true;
                end = (Pos){nr, nc};
                break;
            }
        }
    }

    if (found) {
        /* Reconstruir camino desde end hasta start */
        Pos p = end;
        while (!(p.r == sr && p.c == sc)) {
            if (maze[p.r][p.c] != 'E') maze[p.r][p.c] = '*';
            p = parent[p.r][p.c];
        }
    }
}

int main(void) {
    bfs(1, 1);
    for (int r = 0; r < ROWS; r++) printf("%s\n", maze[r]);
    return 0;
}
```

### BFS en laberinto: Rust

```rust
use std::collections::VecDeque;

const ROWS: usize = 6;
const COLS: usize = 8;

fn bfs(maze: &mut Vec<Vec<u8>>, sr: usize, sc: usize) {
    let mut visited = vec![vec![false; COLS]; ROWS];
    let mut parent = vec![vec![(0usize, 0usize); COLS]; ROWS];
    let mut queue = VecDeque::new();

    visited[sr][sc] = true;
    queue.push_back((sr, sc));

    let directions: [(isize, isize); 4] = [(-1,0),(1,0),(0,-1),(0,1)];
    let mut end = None;

    'outer: while let Some((r, c)) = queue.pop_front() {
        for (dr, dc) in &directions {
            let nr = r as isize + dr;
            let nc = c as isize + dc;
            if nr < 0 || nr >= ROWS as isize || nc < 0 || nc >= COLS as isize {
                continue;
            }
            let (nr, nc) = (nr as usize, nc as usize);
            if maze[nr][nc] == b'#' || visited[nr][nc] { continue; }

            visited[nr][nc] = true;
            parent[nr][nc] = (r, c);
            queue.push_back((nr, nc));

            if maze[nr][nc] == b'E' {
                end = Some((nr, nc));
                break 'outer;
            }
        }
    }

    if let Some((mut r, mut c)) = end {
        while !(r == sr && c == sc) {
            if maze[r][c] != b'E' { maze[r][c] = b'*'; }
            let (pr, pc) = parent[r][c];
            r = pr;
            c = pc;
        }
    }
}

fn main() {
    let grid: [&[u8; 8]; 6] = [
        b"########", b"#S.#...#", b"#.##.#.#",
        b"#....#.#", b"#.##...E", b"########",
    ];
    let mut maze: Vec<Vec<u8>> = grid.iter().map(|row| row.to_vec()).collect();
    bfs(&mut maze, 1, 1);
    for row in &maze {
        println!("{}", std::str::from_utf8(row).unwrap());
    }
}
```

BFS encuentra el camino más corto que DFS (C03/T04) no garantizaba.

---

## Comparación con la pila como TAD

La cola y la pila comparten la misma estructura: una colección lineal con
acceso restringido.  La diferencia es **dónde** se inserta y elimina:

| Propiedad | Pila | Cola |
|-----------|------|------|
| Disciplina | LIFO | FIFO |
| Inserción | Un extremo (tope) | Un extremo (final) |
| Eliminación | Mismo extremo (tope) | **Otro** extremo (frente) |
| Operaciones | push / pop / top | enqueue / dequeue / front |
| Algoritmo asociado | DFS, backtracking | BFS, simulación |
| Analogía | Pila de platos, ctrl+Z | Fila de espera, impresora |

La cola necesita acceso a **ambos extremos**: insertar en uno, eliminar del
otro.  Esto hace que la implementación sea un poco más compleja que la pila
(que solo necesita un extremo).

---

## Ejercicios

### Ejercicio 1 — Traza FIFO

Dada la siguiente secuencia de operaciones, muestra el estado de la cola
después de cada una:

```
enqueue(5), enqueue(3), enqueue(8), dequeue(), enqueue(1),
dequeue(), dequeue(), enqueue(7), dequeue(), dequeue()
```

<details>
<summary>Predice cada estado</summary>

```
enqueue(5):  [5]           front=5
enqueue(3):  [5, 3]        front=5
enqueue(8):  [5, 3, 8]     front=5
dequeue():   [3, 8]        → 5
enqueue(1):  [3, 8, 1]     front=3
dequeue():   [8, 1]        → 3
dequeue():   [1]           → 8
enqueue(7):  [1, 7]        front=1
dequeue():   [7]           → 1
dequeue():   []            → 7  (cola vacía)
```

Orden de salida: 5, 3, 8, 1, 7 — exactamente el orden de entrada.
</details>

### Ejercicio 2 — Cola vs pila: misma secuencia

Aplica la misma secuencia del ejercicio 1 pero usando una **pila** en lugar de
cola.  Compara los órdenes de salida.

<details>
<summary>Predice las diferencias</summary>

Con pila (LIFO), los `pop` retornan en orden inverso al de inserción.  Los
primeros tres push dan [5,3,8], pop retorna 8 (no 5).  La secuencia de salida
será distinta: 8, 1, 3, 7, 5 (según el estado de la pila en cada momento).
</details>

### Ejercicio 3 — Dos pilas: traza completa

Traza la implementación de cola con dos pilas para la secuencia:

```
enqueue(A), enqueue(B), dequeue(), enqueue(C), enqueue(D),
dequeue(), dequeue(), dequeue()
```

Muestra el estado de *inbox* y *outbox* después de cada operación, incluyendo
cuándo ocurre el volcado.

<details>
<summary>Predice cuántas veces se vuelca</summary>

Se vuelca 2 veces.  Primer dequeue: inbox=[B,A] → outbox=[A,B], pop A.  Segundo
dequeue: outbox=[B], pop B (sin volcar).  Tercer dequeue: outbox vacío, volcar
inbox=[D,C] → outbox=[C,D], pop C.  Cuarto dequeue: outbox=[D], pop D.
Salida: A, B, C, D — FIFO correcto.
</details>

### Ejercicio 4 — Dequeue en cola vacía

Escribe un programa en C que intente hacer `dequeue` en una cola vacía.
Implementa las tres estrategias de manejo (centinela `-1`, `bool` con puntero
de salida, `assert`).  ¿Cuál preferirías y por qué?

<details>
<summary>Comparación</summary>

- Centinela `-1`: falla si `-1` es un valor válido.
- `bool` + puntero: robusto, no ambiguo, pero verboso.
- `assert`: termina el programa — útil en desarrollo, no en producción.

La mejor opción general en C es `bool` con puntero de salida.  En Rust,
`Option<T>` combina las ventajas de todas: no ambiguo, no termina el programa,
y el compilador obliga a manejar el caso `None`.
</details>

### Ejercicio 5 — Identificar la estructura

Para cada escenario, indica si el modelo es una cola (FIFO), una pila (LIFO),
u otra estructura:

1. Bandeja de entrada de correo electrónico (leer primero el más reciente).
2. Sala de emergencias (atender por gravedad, no por orden de llegada).
3. Buffer de teclado (las teclas se procesan en orden).
4. Función "deshacer" de un editor.
5. Cola de impresión.
6. Navegación "atrás" en el navegador.

<details>
<summary>Respuestas</summary>

1. **Pila** (LIFO) — el más reciente se ve primero.
2. **Cola de prioridad** — ni FIFO ni LIFO, se atiende por prioridad.
3. **Cola** (FIFO) — las teclas se procesan en orden.
4. **Pila** (LIFO) — la última acción se deshace primero.
5. **Cola** (FIFO) — el primer documento enviado se imprime primero.
6. **Pila** (LIFO) — la última página visitada es la primera al retroceder.
</details>

### Ejercicio 6 — Costo amortizado de dos pilas

Demuestra que la cola con dos pilas tiene costo amortizado $O(1)$ por
operación.  Pista: usa el **método del banquero** — cada elemento "paga" 2
monedas al entrar (una por el push a inbox, una por el futuro push a outbox).

<details>
<summary>Argumento</summary>

Cada elemento pasa por exactamente 4 operaciones: push a inbox (1), pop de
inbox (1), push a outbox (1), pop de outbox (1).  Total: 4 operaciones por
elemento, distribuidas entre un enqueue y un dequeue.  Costo amortizado: 2 por
enqueue + 2 por dequeue = $O(1)$ cada uno.  El volcado puede ser $O(k)$ para
$k$ elementos, pero esos $k$ elementos habían acumulado crédito suficiente.
</details>

### Ejercicio 7 — BFS nivel por nivel

Modifica el BFS del tópico para que imprima los nodos **nivel por nivel**
(separados por líneas).  Pista: procesa todos los nodos del nivel actual antes
de pasar al siguiente (usa `size()` de la cola al inicio de cada nivel).

<details>
<summary>Patrón del bucle</summary>

```c
while (!queue_empty(&q)) {
    int level_size = queue_size(&q);
    for (int i = 0; i < level_size; i++) {
        Pos cur = queue_dequeue(&q);
        /* procesar cur */
        /* enqueue vecinos */
    }
    printf("--- fin nivel ---\n");
}
```

Este patrón es fundamental en BFS de árboles/grafos para distinguir niveles.
</details>

### Ejercicio 8 — Cola con array: el problema del desperdicio

Implementa una cola con array lineal (sin circularidad) donde `dequeue` solo
avanza `front` sin desplazar.  Muestra qué ocurre después de 1000 enqueues
seguidos de 1000 dequeues seguidos de 1 enqueue más.  ¿Cuánto espacio se
desperdicia?

<details>
<summary>Predice el resultado</summary>

Después de 1000 enqueues y 1000 dequeues: front=1000, rear=999.  Las 1000
posiciones iniciales están vacías pero inutilizables.  El siguiente enqueue
necesita posición 1000 — si el array era de tamaño 1000, se reporta "cola
llena" aunque está vacía.  Este es exactamente el problema que el array circular
(T02) resuelve.
</details>

### Ejercicio 9 — Invertir una cola

Escribe una función que invierta el contenido de una cola usando:
(a) una pila auxiliar, (b) recursión (sin pila explícita).
Implementa ambas versiones en C o Rust.

<details>
<summary>Pista para la versión recursiva</summary>

```
invertir(cola):
    si cola vacía: return
    x = dequeue(cola)
    invertir(cola)       // invertir el resto
    enqueue(cola, x)     // poner x al final
```

Cada elemento se saca, se invierte el resto recursivamente, y se reinserta al
final.  La pila de llamadas actúa como pila auxiliar implícita.  Complejidad:
$O(n^2)$ por los enqueues sucesivos al final (la versión con pila explícita es
$O(n)$).
</details>

### Ejercicio 10 — Hot potato (Josephus con cola)

El juego "patata caliente": $n$ personas en círculo pasan la patata.  Cada $k$
pases, la persona con la patata es eliminada.  El último sobreviviente gana.

Implementa con una cola: en cada ronda, hacer `dequeue` y `enqueue` $k-1$
veces (rotar), luego `dequeue` sin reenqueue (eliminar).  Repetir hasta que
quede 1.

<details>
<summary>Ejemplo: n=7, k=3</summary>

Inicio: [1, 2, 3, 4, 5, 6, 7].  Rotar 2: [3, 4, 5, 6, 7, 1, 2].  Eliminar 3.
Cola: [4, 5, 6, 7, 1, 2].  Rotar 2: [6, 7, 1, 2, 4, 5].  Eliminar 6.
Cola: [7, 1, 2, 4, 5].  Continuar...

Este es el **problema de Josephus**.  La cola hace que la implementación sea
natural — el `dequeue`/`enqueue` simula la rotación del círculo.  La fórmula
cerrada es $J(n) = 2L + 1$ donde $n = 2^m + L$, pero implementarlo con cola
es más general (funciona para cualquier $k$).
</details>
