# T03 — Análisis amortizado

---

## 1. El problema: operaciones con costo variable

Algunas operaciones son baratas la mayoría de las veces pero ocasionalmente
caras. El peor caso por operación individual no refleja el rendimiento
real:

```
Vector dinámico — push:

  push 1:  copia 0 elementos, inserta    → costo 1
  push 2:  copia 1 elemento, inserta     → costo 2   (realloc: cap 1→2)
  push 3:  copia 2 elementos, inserta    → costo 3   (realloc: cap 2→4)
  push 4:  inserta                       → costo 1   (hay espacio)
  push 5:  copia 4 elementos, inserta    → costo 5   (realloc: cap 4→8)
  push 6:  inserta                       → costo 1
  push 7:  inserta                       → costo 1
  push 8:  inserta                       → costo 1
  push 9:  copia 8 elementos, inserta    → costo 9   (realloc: cap 8→16)
  push 10: inserta                       → costo 1
  ...

  Peor caso individual: O(n) — cuando hay realloc
  Pero la MAYORÍA son O(1)
```

Si decimos "push es $O(n)$" estamos siendo demasiado pesimistas. Si decimos
"push es $O(1)$" estamos ignorando los reallocs. El análisis amortizado
resuelve esto: calcula el **costo promedio por operación sobre una secuencia**,
no sobre una sola operación.

---

## 2. ¿Qué es el análisis amortizado?

El costo amortizado de una operación es el costo **promedio por operación
cuando se ejecuta una secuencia** de $n$ operaciones:

```
Costo amortizado = Costo total de n operaciones / n
```

No es un promedio probabilístico (como el caso promedio) — no hay
suposiciones sobre la distribución de las entradas. Es un cálculo
exacto sobre la **peor secuencia posible**.

```
                    Análisis          Análisis         Análisis
                    peor caso         promedio         amortizado
──────────────────────────────────────────────────────────────────
Mide:               una operación     una operación    n operaciones
                    en el peor input  con input random  en secuencia
Suposiciones:       ninguna           distribución      ninguna
Garantía:           por operación     esperada          por secuencia
push en Vec:        O(n)              O(1)              O(1)
```

El análisis amortizado da una **garantía real**: $n$ pushes en un vector
dinámico cuestan como máximo $O(n)$ en total, garantizado, sin importar
la entrada. Por lo tanto cada push cuesta $O(1)$ amortizado.

---

## 3. Ejemplo canónico: vector dinámico con duplicación

### Estrategia de crecimiento

Cuando el vector está lleno, se duplica la capacidad:

```c
void vec_push(Vec *v, int elem) {
    if (v->size == v->capacity) {
        v->capacity *= 2;
        v->data = realloc(v->data, v->capacity * sizeof(int));
    }
    v->data[v->size] = elem;
    v->size++;
}
```

### Análisis directo: contar operaciones

Para $n$ pushes empezando con capacidad 1:

```
Push #   Capacidad antes   ¿Realloc?   Costo (copias + 1 insert)
  1          1               Sí          1 + 1 = 2    (cap: 1→2)
  2          2               Sí          2 + 1 = 3    (cap: 2→4)
  3          4               No          1
  4          4               Sí          4 + 1 = 5    (cap: 4→8)
  5          8               No          1
  6          8               No          1
  7          8               No          1
  8          8               Sí          8 + 1 = 9    (cap: 8→16)
  9-16       16              No          1 cada uno (×8)
 17          16              Sí          16 + 1 = 17  (cap: 16→32)
 ...
```

Los reallocs ocurren en los pushes 1, 2, 4, 8, 16, 32, ..., $2^k$.

### Costo total de n pushes

```
Costo total = n inserciones + copias de reallocs
            = n + (1 + 2 + 4 + 8 + ... + 2^⌊log₂(n)⌋)
            = n + (2^(⌊log₂(n)⌋+1) - 1)
            < n + 2n
            = 3n

Costo total < 3n  →  Costo amortizado = 3n/n = 3 = O(1)
```

La serie geométrica $1 + 2 + 4 + \ldots + n = 2n - 1 < 2n$. Sumando las $n$
inserciones individuales: total $< 3n$.

```
Costo
  │
  │  17
  │  ┃
  │  ┃
  │  ┃
  │  ┃
  │  ┃        9
  │  ┃        ┃
  │  ┃        ┃
  │  ┃    5   ┃
  │  ┃    ┃   ┃
  │  ┃ 3  ┃   ┃
  │  ┃ ┃  ┃   ┃
  │ 2┃ ┃  ┃   ┃
  │ ┃┃ ┃1 ┃1 1┃1 1 1 1 1 1 1 1 1 1 1
  ├─┸┸─┸┸─┸┸──┸┸─┸─┸─┸─┸──────────────
  1  2  3  4  5  6  7  8  9 ...      16  17
                                      Push #

  Picos cada 2^k, pero los valles (costo 1) compensan.
  Promedio por operación = 3 → O(1) amortizado.
```

---

## 4. ¿Por qué duplicar y no incrementar en 1?

Si en vez de duplicar, crecemos de 1 en 1:

```
Push #   Capacidad antes   Costo
  1          1              1 + 1 = 2
  2          2              2 + 1 = 3
  3          3              3 + 1 = 4
  4          4              4 + 1 = 5
  ...
  n          n              n + 1

Costo total = 2 + 3 + 4 + ... + (n+1) = n(n+3)/2 - 1 ≈ n²/2

Costo amortizado = n²/(2n) = n/2 = O(n)  ← LINEAL, no constante
```

Y si crecemos sumando una constante fija (ej: +100):

```
Reallocs cada 100 pushes: n/100 reallocs
Cada realloc copia ~k·100 elementos (k-ésimo realloc)

Costo total = n + 100 + 200 + 300 + ... + n
            ≈ n + n²/200
            = O(n²)

Costo amortizado = O(n)  ← sigue siendo lineal
```

Solo la **duplicación** (o multiplicación por cualquier factor $> 1$) da
$O(1)$ amortizado:

| Estrategia | Costo total de $n$ pushes | Amortizado |
|------------|------------------------|------------|
| +1 cada vez | $O(n^2)$ | $O(n)$ |
| +100 cada vez | $O(n^2)$ | $O(n)$ |
| $\times 2$ cada vez | $O(n)$ | **$O(1)$** |
| $\times 1.5$ cada vez | $O(n)$ | **$O(1)$** |
| $\times 4$ cada vez | $O(n)$ | **$O(1)$** |

Factor $\times 1.5$ (usado por MSVC `std::vector`) usa menos memoria que $\times 2$ pero
tiene más reallocs. Factor $\times 2$ (usado por GCC `std::vector`, Rust `Vec`)
desperdicia más memoria pero hace menos reallocs. Ambos son $O(1)$ amortizado.

---

## 5. Método del banquero (Banker's Method)

Técnica formal de análisis amortizado. La idea: cada operación "paga"
más de lo que cuesta, y el exceso se guarda como **crédito** para pagar
las operaciones caras futuras.

### Analogía

```
Restaurante con propinas:
  - Comidas baratas ($5): dejas $3 de propina → pagas $8
  - Comidas caras ($50): usas las propinas acumuladas → pagas $0

Si por cada 10 comidas baratas hay 1 cara:
  10 × $8 + 0 = $80 para 11 comidas
  Costo amortizado: $80/11 ≈ $7.27 por comida
  (no los $50 del peor caso individual)
```

### Aplicación al vector dinámico

Asigna un costo amortizado de **3** a cada push:

```
Cada push cuesta realmente 1 (insertar el elemento).
Se cobra 3 → sobran 2 monedas de crédito.

Las 2 monedas se "depositan" en el elemento insertado.

Cuando se necesita un realloc (duplicar de cap k a 2k):
  → hay que copiar k elementos
  → los k/2 elementos insertados DESDE el último realloc
    tienen 2 monedas cada uno → k/2 × 2 = k monedas
  → k monedas pagan la copia de k elementos ✓

  ¿Por qué k/2? Porque entre el realloc anterior (cap k/2 → k)
  y este (cap k → 2k), se insertaron exactamente k/2 elementos
  (los que llenaron la segunda mitad).

  Pero necesitamos k monedas y solo tenemos k/2 × 2 = k. Exacto. ✓
```

Visualización:

```
Capacidad = 8, size = 8 (lleno, próximo push dispara realloc)

Posiciones:  [0] [1] [2] [3] [4] [5] [6] [7]
Monedas:      0   0   0   0   💰  💰  💰  💰
                              └── 2 monedas cada uno ──┘
                              (insertados desde el último realloc)

Total monedas: 4 × 2 = 8
Costo de copiar 8 elementos: 8
→ Las monedas cubren exactamente el costo del realloc ✓
```

### El invariante

Después de cada push, cada elemento que fue insertado después del último
realloc tiene al menos 2 monedas de crédito. Cuando el siguiente realloc
ocurre, hay suficientes monedas para pagarlo.

Por lo tanto: el costo amortizado de 3 por push es suficiente. $O(1)$ amortizado. $\blacksquare$

---

## 6. Método del potencial (Potential Method)

Más formal que el método del banquero. Define una **función de potencial**
$\Phi$ que mide "cuánta energía acumulada" tiene la estructura:

```
Costo amortizado = costo real + ΔΦ
                 = costo real + (Φ_después - Φ_antes)
```

Si $\Phi$ sube (se acumula potencial), el costo amortizado es mayor que el real.
Si $\Phi$ baja (se gasta potencial), el costo amortizado es menor que el real.

### Para el vector dinámico

Función de potencial: **$\Phi = 2 \cdot \text{size} - \text{capacity}$**

($\Phi$ mide "cuánto falta para que el vector necesite un realloc". Cuando
$\text{size} = \text{capacity}$, $\Phi = \text{capacity}$, lo que compensa el costo del realloc.)

**Caso 1: push sin realloc** ($\text{size} < \text{capacity}$)

```
Costo real = 1
Φ_antes = 2·size - cap
Φ_después = 2·(size+1) - cap = Φ_antes + 2

Costo amortizado = 1 + 2 = 3
```

**Caso 2: push con realloc** ($\text{size} = \text{capacity} = k$)

```
Costo real = k + 1   (copiar k elementos + insertar 1)
Φ_antes = 2k - k = k
Φ_después = 2(k+1) - 2k = 2   (nueva cap = 2k, nuevo size = k+1)

ΔΦ = 2 - k

Costo amortizado = (k + 1) + (2 - k) = 3
```

En ambos casos, el costo amortizado es exactamente **3**. $\blacksquare$

### Requisito clave

La función de potencial debe cumplir:
- $\Phi \geq 0$ siempre (no se puede "deber" energía)
- $\Phi_0 = 0$ (empieza sin potencial acumulado)

Para nuestro $\Phi = 2 \cdot \text{size} - \text{capacity}$:
- Siempre $\geq 0$ cuando $\text{capacity} \leq 2 \cdot \text{size}$ (verdadero por la estrategia de duplicación)
- $\Phi_0 = 2 \cdot 0 - 0 = 0$ ✓

---

## 7. Amortizado ≠ Promedio

Es crucial no confundir:

```
Caso promedio:
  - Asume distribución probabilística de inputs
  - "En promedio sobre inputs aleatorios, la operación cuesta X"
  - Puede haber inputs donde SIEMPRE es caro

Amortizado:
  - NO asume nada sobre los inputs
  - "Sobre CUALQUIER secuencia de n operaciones, el total es ≤ n·X"
  - Garantía determinista sobre la secuencia completa
  - Operaciones individuales pueden ser caras, pero el promedio no
```

Ejemplo: la tabla hash tiene caso **promedio** $O(1)$ (depende de la
distribución de claves). El vector dinámico tiene push **amortizado**
$O(1)$ (garantizado para cualquier secuencia de pushes).

```
Hash table lookup:
  - Input adversarial: TODAS las operaciones son O(n)
  - "O(1) promedio" NO es una garantía para entradas malas

Vec push:
  - Cualquier secuencia de n pushes: total ≤ 3n
  - Esto es una GARANTÍA — no depende de los datos
  - Operaciones individuales pueden ser O(n), pero se compensan
```

---

## 8. Otros ejemplos de análisis amortizado

### 8.1 Contador binario

Un contador binario de $k$ bits incrementa de 0 a $n$:

```
Valor    Binario    Bits que cambian
  0      0000       —
  1      0001       1
  2      0010       2
  3      0011       1
  4      0100       3
  5      0101       1
  6      0110       2
  7      0111       1
  8      1000       4
  9      1001       1
 10      1010       2
```

```
Peor caso individual: O(k) bits cambian (ej: 0111→1000)
```

Análisis amortizado:
- Bit 0 cambia en cada incremento: $n$ veces
- Bit 1 cambia cada 2 incrementos: $n/2$ veces
- Bit 2 cambia cada 4 incrementos: $n/4$ veces
- Bit $j$ cambia cada $2^j$ incrementos: $n/2^j$ veces

```
Total de cambios = n + n/2 + n/4 + ... = n · (1 + 1/2 + 1/4 + ...) < 2n

Costo amortizado = 2n/n = 2 = O(1) por incremento
```

### 8.2 Stack con multipop

Un stack que tiene `push`, `pop` y `multipop(k)` (hace pop de $k$ elementos):

```c
void multipop(Stack *s, int k) {
    while (!stack_empty(s) && k > 0) {
        stack_pop(s);
        k--;
    }
}
```

```
Peor caso individual de multipop: O(n) — vaciar todo el stack
```

Análisis amortizado (método del banquero):
- Cada push deposita 1 moneda en el elemento
- Cada pop (individual o dentro de multipop) gasta 1 moneda
- Un elemento solo puede hacer pop una vez
- Si haces $n$ operaciones en total (mix de push, pop, multipop):
  → como máximo $n$ pushes → como máximo $n$ monedas depositadas
  → como máximo $n$ pops en total (no puedes popear más de lo que pusheaste)

```
Costo total ≤ 2n   (n pushes × 2 monedas cada uno)
Costo amortizado = O(1) por operación
```

### 8.3 Tabla hash con rehash

Cuando el load factor supera un umbral, se hace rehash (nueva tabla,
reinsertar todos los elementos):

```
Peor caso individual: O(n) — reinsertar n elementos
Amortizado: O(1) — el rehash se "paga" con las inserciones baratas
  (mismo patrón que el vector dinámico)
```

---

## 9. Implicaciones para diseño de TADs

### En C: realloc con duplicación

```c
typedef struct {
    int    *data;
    size_t  size;
    size_t  capacity;
} Vec;

bool vec_push(Vec *v, int elem) {
    if (v->size == v->capacity) {
        size_t new_cap = v->capacity == 0 ? 4 : v->capacity * 2;
        int *new_data = realloc(v->data, new_cap * sizeof(int));
        if (!new_data) return false;
        v->data = new_data;
        v->capacity = new_cap;
    }
    v->data[v->size++] = elem;
    return true;
}
```

### En Rust: Vec<T> usa exactamente esta estrategia

```rust
let mut v = Vec::new();     // capacity = 0
v.push(1);                  // capacity → 4 (alocación inicial)
v.push(2);                  // capacity = 4, sin realloc
v.push(3);                  // sin realloc
v.push(4);                  // sin realloc
v.push(5);                  // capacity → 8 (duplicación)
```

Rust expone la capacidad para optimizar:

```rust
// Pre-alocar si sabes cuántos elementos habrá:
let mut v = Vec::with_capacity(1000);  // 0 reallocs para 1000 pushes
for i in 0..1000 {
    v.push(i);   // O(1) real, no solo amortizado — sin reallocs
}
```

`with_capacity` convierte el $O(1)$ amortizado en $O(1)$ real eliminando
todos los reallocs. Esto importa en hot paths.

### Cuándo importa el amortizado vs real

```
O(1) amortizado está bien para:
  - La mayoría de aplicaciones
  - Procesamiento batch
  - Throughput (rendimiento total)

O(1) real (worst case) es necesario para:
  - Sistemas de tiempo real (robótica, audio, trading)
  - Latencia garantizada (el pico de realloc puede causar jank)
  - Garbage-collected systems (el GC ya causa picos)

Soluciones para necesitar O(1) real:
  - Pre-alocar con capacidad conocida
  - Usar estructuras con O(1) real (linked list para push)
  - Incremental realloc (copiar de a poco, no todo de golpe)
```

---

## 10. Shrink: la operación inversa

Si el vector crece con duplicación, ¿cuándo reducir la capacidad?

### Estrategia ingenua: shrink a la mitad cuando size = capacity/2

```
Problema — thrashing:

  Capacidad = 8, size = 4
  push  → size = 5 (OK)
  pop   → size = 4, capacity/2 = 4 → SHRINK a cap=4
  push  → size = 5 → cap llena → GROW a cap=8
  pop   → size = 4 → SHRINK a cap=4
  push  → GROW a cap=8
  ...

  Cada push/pop cuesta O(n) → amortizado O(n). Desastre.
```

### Estrategia correcta: shrink cuando size = capacity/4

```
Shrink a capacity/2 cuando size cae a capacity/4:

  Hay un "buffer zone" entre capacity/4 y capacity/2 que
  absorbe oscilaciones push/pop sin disparar reallocs.

  Capacidad = 16, shrink cuando size = 4 (a cap = 8)
  → 4 pops antes del siguiente shrink
  → 4 pushes antes del siguiente grow (desde cap 8)
  → No hay thrashing
```

Con esta estrategia, tanto push como pop son $O(1)$ amortizado.

---

## Ejercicios

### Ejercicio 1 — Contar el costo total

Calcula el costo total de 16 pushes en un vector con capacidad inicial 1
y estrategia de duplicación. Cuenta cada inserción como costo 1 y cada
copia en realloc como costo 1.

**Predicción**: ¿Cuántos reallocs hay? ¿Cuál es el costo total exacto?
¿El amortizado es menor a 3?

<details><summary>Respuesta</summary>

```
Push  Cap antes → después   Copias   Total (copias + 1)
 1     1 → 2                 1        2
 2     2 → 4                 2        3
 3     4                     0        1
 4     4 → 8                 4        5
 5     8                     0        1
 6     8                     0        1
 7     8                     0        1
 8     8 → 16                8        9
 9     16                    0        1
10     16                    0        1
11     16                    0        1
12     16                    0        1
13     16                    0        1
14     16                    0        1
15     16                    0        1
16     16                    0        1
─────────────────────────────────────────
                            15       31
```

- **Reallocs**: 4 (en pushes 1, 2, 4, 8)
- **Copias totales**: $1 + 2 + 4 + 8 = 15$
- **Costo total**: 16 inserciones + 15 copias = **31**
- **Amortizado**: $31/16 =$ **1.9375** → menor que 3

El amortizado teórico de 3 es una cota superior. El costo real es menor
porque la demostración usa desigualdades holgadas. En la práctica, el
costo amortizado real tiende a $\approx 2$ para secuencias largas.

</details>

---

### Ejercicio 2 — Crecimiento ×3 vs ×2

Repite el análisis del vector dinámico pero con factor de crecimiento $\times 3$.
Para 27 pushes con capacidad inicial 1, calcula:
1. El costo total
2. El costo amortizado
3. La memoria desperdiciada máxima

**Predicción**: ¿El amortizado será menor o mayor que con factor $\times 2$?
¿Y el desperdicio de memoria?

<details><summary>Respuesta</summary>

```
Reallocs en pushes 1, 3, 10 (capacidad: 1→3→9→27):

Push  Cap antes → después   Copias
 1     1 → 3                 1
 2     3                     0
 3     3 → 9                 3
 4-9   9                     0  (×6)
10     9 → 27                9
11-27  27                    0  (×17)
```

- Copias: $1 + 3 + 9 = 13$
- Costo total: $27 + 13 =$ **40**
- Amortizado: $40/27 =$ **1.48**

Para factor $\times 2$ y 27 pushes:
- Reallocs en 1,2,4,8,16: copias $= 1+2+4+8+16 = 31$
- Costo total: $27 + 31 = 58$
- Amortizado: $58/27 = 2.15$

**Factor $\times 3$ tiene menor amortizado** (1.48 vs 2.15) — menos reallocs,
menos copias totales.

**Pero desperdicia más memoria**:
- $\times 3$: capacidad 27 con 27 elementos → 0% desperdicio (caso exacto)
  pero en el peor caso: 10 elementos en capacidad 27 → 63% desperdiciado
- $\times 2$: peor caso desperdicio $\approx 50\%$ ($n+1$ elementos en capacidad $2n$)
- $\times 3$: peor caso desperdicio $\approx 67\%$ ($n+1$ elementos en capacidad $3n$)

Tradeoff: factor mayor → menos reallocs pero más memoria desperdiciada.
En la práctica, $\times 2$ y $\times 1.5$ son los más usados.

</details>

---

### Ejercicio 3 — Método del banquero para multipop

Un stack soporta `push` ($O(1)$) y `multipop(k)` (pop de $k$ elementos, $O(k)$).

Demuestra con el método del banquero que cualquier secuencia de $n$
operaciones (mezcla de push y multipop) tiene costo total $O(n)$.

**Predicción**: ¿Cuántas monedas debe depositar cada push? ¿Pueden las
monedas totales gastadas superar las depositadas?

<details><summary>Respuesta</summary>

**Asignación de costos**:
- Push: costo amortizado = **2** (1 real + 1 moneda depositada en el elemento)
- Multipop($k$): costo amortizado = **0** (usa las $k$ monedas de los $k$ elementos)

**Argumento**:
- Cada push deposita 1 moneda en el elemento insertado
- Cada pop (individual o como parte de multipop) gasta 1 moneda del elemento
- Un elemento solo puede ser popeado una vez → cada moneda se gasta como máximo una vez
- En $n$ operaciones, hay como máximo $n$ pushes → como máximo $n$ monedas depositadas
- Como máximo $n$ monedas gastadas → como máximo $n$ pops en total

```
Costo total ≤ n × 2 (pushes) + 0 (multipops pagados con monedas)
           = 2n
           = O(n)

Costo amortizado = 2n/n = 2 = O(1) por operación  ✓
```

Las monedas gastadas **nunca** pueden superar las depositadas — es imposible
popear un elemento que no fue pusheado. Esto es el invariante fundamental:
el saldo de monedas nunca es negativo.

</details>

---

### Ejercicio 4 — Incremento de capacidad fijo

Demuestra que si el vector crece sumando una constante $c$ (ej: +100) en
vez de multiplicar, el costo amortizado de push es $O(n)$, no $O(1)$.

Calcula el costo total para $n$ pushes con capacidad inicial $c$ y crecimiento $+c$.

**Predicción**: ¿Cuántos reallocs hay? ¿Cuál es la suma de copias?

<details><summary>Respuesta</summary>

Reallocs ocurren en los pushes $c+1$, $2c+1$, $3c+1$, ..., $kc+1$ donde $k = \lfloor (n-1)/c \rfloor$.

```
Realloc #1 (push c+1):    copia c elementos,     cap c → 2c
Realloc #2 (push 2c+1):   copia 2c elementos,    cap 2c → 3c
Realloc #3 (push 3c+1):   copia 3c elementos,    cap 3c → 4c
...
Realloc #k (push kc+1):   copia kc elementos

Total copias = c + 2c + 3c + ... + kc
             = c · (1 + 2 + ... + k)
             = c · k(k+1)/2

Donde k ≈ n/c:
Total copias ≈ c · (n/c)(n/c + 1)/2 ≈ n²/(2c)

Costo total = n + n²/(2c) = O(n²)
Costo amortizado = O(n²)/n = O(n)
```

Para $c = 100$, $n = 10000$:
- Reallocs: 99
- Copias: $100 + 200 + \ldots + 9900 = 100 \cdot (99 \cdot 100/2) = 495{,}000$
- Con duplicación: copias $\approx 2 \cdot 10000 = 20{,}000$

El crecimiento fijo es **$\approx 25$ veces más costoso** que la duplicación
para $n = 10000$. La diferencia crece con $n$.

</details>

---

### Ejercicio 5 — Potencial del contador binario

Aplica el método del potencial al contador binario.

Función de potencial: $\Phi$ = número de bits en 1.

Para un incremento que cambia $b$ bits de 1 a 0 y 1 bit de 0 a 1:
- Costo real = $b + 1$
- $\Delta\Phi$ = ?
- Costo amortizado = ?

**Predicción**: ¿El costo amortizado sale constante?

<details><summary>Respuesta</summary>

Un incremento cambia los bits así:
- Los $b$ bits de 1 más bajos pasan a 0 (el "carry" se propaga)
- El siguiente bit de 0 pasa a 1

Ejemplo: `0111` → `1000` ($b = 3$ bits de 1→0, 1 bit de 0→1)

```
Costo real = b + 1    (cambiar b+1 bits)

Φ_antes = k           (k bits en 1)
Φ_después = k - b + 1  (se apagaron b, se prendió 1)

ΔΦ = (k - b + 1) - k = 1 - b

Costo amortizado = costo real + ΔΦ
                 = (b + 1) + (1 - b)
                 = 2
                 = O(1)  ✓
```

El potencial sube en $1 - b$, que es negativo cuando $b > 1$ (muchos bits
cambian). Esto compensa el alto costo real. El resultado: exactamente 2
de costo amortizado, sin importar cuántos bits cambien.

Verificación de $\Phi \geq 0$: $\Phi$ = número de bits en 1, que siempre es $\geq 0$. ✓

</details>

---

### Ejercicio 6 — Vec::with_capacity en Rust

Ejecuta este programa y compara los tiempos:

```rust
use std::time::Instant;

fn with_reallocs(n: usize) {
    let mut v = Vec::new();
    for i in 0..n {
        v.push(i);
    }
}

fn without_reallocs(n: usize) {
    let mut v = Vec::with_capacity(n);
    for i in 0..n {
        v.push(i);
    }
}

fn main() {
    let n = 10_000_000;

    let start = Instant::now();
    with_reallocs(n);
    let t1 = start.elapsed();

    let start = Instant::now();
    without_reallocs(n);
    let t2 = start.elapsed();

    println!("Sin pre-alocar: {:?}", t1);
    println!("Con pre-alocar: {:?}", t2);
    println!("Ratio: {:.2}x", t1.as_secs_f64() / t2.as_secs_f64());
}
```

**Predicción**: ¿Cuántas veces más rápido será `with_capacity`?
¿Será una diferencia grande o pequeña?

<details><summary>Respuesta</summary>

Resultados típicos (release mode, `cargo run --release`):

```
Sin pre-alocar: ~50ms
Con pre-alocar: ~30ms
Ratio: ~1.5-2x
```

La diferencia es **modesta** ($\approx 1.5$-$2\times$), no dramática. ¿Por qué?

1. Los reallocs son pocos: para $10^7$ pushes, solo $\approx 23$ reallocs ($\log_2(10^7)$)
2. Cada realloc es `memcpy`, que es muy eficiente (optimizado en hardware)
3. El costo dominante son los $10^7$ writes, no los 23 reallocs
4. `realloc` puede extender in-place (si hay espacio contiguo libre)

Sin embargo, `with_capacity` tiene ventajas no visibles en este benchmark:
- **Sin fragmentación**: una sola alocación grande vs muchas pequeñas
- **Determinismo**: sin picos de latencia por realloc
- **Menos presión al allocator**: menos llamadas a malloc/realloc

En sistemas de tiempo real, la diferencia no es velocidad total sino
**predecibilidad**: `with_capacity` garantiza $O(1)$ real por push,
no solo amortizado.

Compilar en release es crucial para este benchmark:
```bash
cargo run --release
```

En debug, Rust agrega checks de bounds y overflow que dominan el tiempo.

</details>

---

### Ejercicio 7 — Thrashing al shrink

Implementa un vector en C con `push` (duplica si lleno) y `pop` (reduce
a la mitad si size = capacity/2). Ejecuta esta secuencia:

```c
for (int i = 0; i < 1000; i++) vec_push(&v, i);
for (int rep = 0; rep < 10000; rep++) {
    vec_pop(&v);
    vec_push(&v, 0);
}
```

**Predicción**: ¿Cuántos reallocs ocurren en los 10,000 pop+push?
¿Cómo se resuelve?

<details><summary>Respuesta</summary>

Después de 1000 pushes: $\text{size} = 1000$, $\text{capacity} = 1024$.

```
pop: size = 999. ¿999 == 1024/2 = 512? No → sin shrink
push: size = 1000. ¿1000 == 1024? No → sin realloc
```

En este caso no hay thrashing porque $999 > 512$. Pero si el tamaño
oscilara alrededor de 512:

```
Supongamos size = capacity/2 = 512, capacity = 1024:

pop:  size = 511, 511 < 512 → SHRINK a cap 512. Copia 511 elementos.
push: size = 512, 512 == 512 → GROW a cap 1024. Copia 512 elementos.
pop:  size = 511, 511 < 512 → SHRINK. Copia 511.
push: size = 512 → GROW. Copia 512.
...

10,000 × ~512 copias = ~5,120,000 copias. Catastrófico.
```

**Solución**: shrink cuando size = capacity/**4** (no capacity/2):

```
pop:  size = 255, cap = 1024. 255 < 256 → SHRINK a cap 512
push: size = 256, cap = 512. 256 < 512 → sin realloc
push: size = 257, cap = 512. → sin realloc
... (256 pushes antes del próximo grow)

→ No hay thrashing. Buffer zone entre cap/4 y cap/2 absorbe oscilaciones.
```

Con la estrategia cap/4, push y pop son **ambos $O(1)$ amortizado**.

</details>

---

### Ejercicio 8 — Amortizado en la tabla hash

Una tabla hash con chaining duplica la tabla (rehash) cuando el load
factor supera 0.75. Si hay $n = 1000$ inserciones:

1. ¿Cuántos rehashes ocurren?
2. ¿Cuál es el costo total de todos los rehashes?
3. ¿Cuál es el costo amortizado por inserción?

Asume capacidad inicial = 4.

**Predicción**: ¿El patrón es similar al del vector dinámico?

<details><summary>Respuesta</summary>

Rehash ocurre cuando size > capacity × 0.75:

```
Cap    Rehash cuando size >    Rehash en insert #
  4         3                       4    (cap → 8)
  8         6                       7    (cap → 16)
 16        12                      13    (cap → 32)
 32        24                      25    (cap → 64)
 64        48                      49    (cap → 128)
128        96                      97    (cap → 256)
256       192                     193    (cap → 512)
512       384                     385    (cap → 1024)
1024      768                     769    (cap → 2048)
```

Pero $769 < 1000$, así que necesitamos el rehash 9. No llega al siguiente
($1536 > 1000$).

**Rehashes: 9** (en inserts 4, 7, 13, 25, 49, 97, 193, 385, 769)

**Costo de rehashes** (reinsertar todos los elementos):

```
Copias: 3 + 6 + 12 + 24 + 48 + 96 + 192 + 384 + 768
      = 3 · (1 + 2 + 4 + 8 + 16 + 32 + 64 + 128 + 256)
      = 3 · (2⁹ - 1)
      = 3 · 511
      = 1533
```

**Costo total**: 1000 inserciones + 1533 copias = **2533**

**Amortizado**: $2533/1000 \approx$ **2.53** = $O(1)$ ✓

Sí, el patrón es idéntico al del vector dinámico: las duplicaciones
geométricas hacen que la suma de copias sea lineal en $n$, dando $O(1)$
amortizado. La única diferencia es el factor de carga (0.75 vs 1.0).

</details>

---

### Ejercicio 9 — ¿Es O(1) amortizado suficiente?

Para cada escenario, indica si $O(1)$ amortizado es aceptable o si
necesitas $O(1)$ de peor caso. Justifica:

```
a) Servidor web agregando logs a un buffer
b) Controlador de un marcapasos procesando señales cardíacas
c) Videojuego agregando partículas a un sistema de partículas
d) Base de datos insertando registros bajo carga alta
e) Compilador construyendo un AST (abstract syntax tree)
```

**Predicción**: ¿Cuántos de los cinco necesitan $O(1)$ real?

<details><summary>Respuesta</summary>

| Escenario | ¿$O(1)$ amortizado ok? | Razón |
|-----------|---------------------|-------|
| a) Logs en buffer | **Sí** | Los picos de latencia en logging son tolerables |
| b) Marcapasos | **No** | Un pico de latencia puede causar un latido perdido — consecuencia fatal. Necesita $O(1)$ worst-case |
| c) Partículas en videojuego | **Depende** | Un pico de 1ms causa "frame stutter" visible. Pre-alocar con capacidad conocida, o usar pool allocator |
| d) Base de datos | **Depende** | Bajo carga alta, un rehash de tabla hash puede causar timeouts. Las DBs usan "incremental rehash" (copiar de a poco) |
| e) Compilador AST | **Sí** | El throughput total importa, no la latencia de una inserción. Compilar tarda segundos/minutos de todos modos |

**Solo 1 necesita $O(1)$ real estrictamente** (marcapasos). 2 más se
benefician de $O(1)$ real pero pueden mitigarlo con pre-alocación.

Soluciones cuando se necesita $O(1)$ real:
- **Pre-alocar**: `Vec::with_capacity(max_esperado)` → sin reallocs
- **Pool allocator**: bloques pre-alocados de tamaño fijo → sin malloc
- **Incremental rehash**: mover $k$ elementos por operación (Redis hace esto)
- **Estructura diferente**: linked list tiene push $O(1)$ real (sin realloc)

</details>

---

### Ejercicio 10 — Implementar y medir

Implementa un vector dinámico en C con tres estrategias de crecimiento:
1. +1 cada vez
2. +100 cada vez
3. $\times 2$ cada vez

Para cada una, mide el tiempo de $10^5$ pushes y cuenta los reallocs.

**Predicción**: ¿Cuántos reallocs tiene cada estrategia? ¿Cuántas veces
más lenta es la estrategia +1 comparada con $\times 2$?

<details><summary>Respuesta</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct { int *data; size_t size, cap; int reallocs; } Vec;

void vec_init(Vec *v) { v->data = NULL; v->size = v->cap = v->reallocs = 0; }

void vec_push_plus1(Vec *v, int elem) {
    if (v->size == v->cap) {
        v->cap += 1;
        v->data = realloc(v->data, v->cap * sizeof(int));
        v->reallocs++;
    }
    v->data[v->size++] = elem;
}

void vec_push_plus100(Vec *v, int elem) {
    if (v->size == v->cap) {
        v->cap += 100;
        v->data = realloc(v->data, v->cap * sizeof(int));
        v->reallocs++;
    }
    v->data[v->size++] = elem;
}

void vec_push_times2(Vec *v, int elem) {
    if (v->size == v->cap) {
        v->cap = v->cap == 0 ? 4 : v->cap * 2;
        v->data = realloc(v->data, v->cap * sizeof(int));
        v->reallocs++;
    }
    v->data[v->size++] = elem;
}

int main(void) {
    int n = 100000;
    typedef void (*push_fn)(Vec *, int);
    push_fn fns[] = {vec_push_plus1, vec_push_plus100, vec_push_times2};
    const char *names[] = {"+1", "+100", "x2"};

    for (int f = 0; f < 3; f++) {
        Vec v;
        vec_init(&v);
        clock_t start = clock();
        for (int i = 0; i < n; i++) fns[f](&v, i);
        double ms = 1000.0 * (clock() - start) / CLOCKS_PER_SEC;
        printf("%-6s  reallocs: %6d  time: %8.2f ms\n",
               names[f], v.reallocs, ms);
        free(v.data);
    }
    return 0;
}
```

Resultados esperados:

```
+1      reallocs: 100000  time: ~2000-5000 ms
+100    reallocs:   1000  time:    ~20-50  ms
x2      reallocs:     17  time:     ~1-3   ms
```

| Estrategia | Reallocs | Razón |
|-----------|----------|-------|
| +1 | 100,000 | Uno por push |
| +100 | 1,000 | $n/100$ |
| $\times 2$ | 17 | $\log_2(n)$ |

La estrategia +1 es **$\approx 1000$-$2000\times$ más lenta** que $\times 2$. Cada realloc
copia todos los datos, y con +1 hay un realloc por push → $O(n^2)$ total.

Compilar sin optimización para que los tiempos reflejen el trabajo real:
```bash
gcc -O0 -std=c11 -o vec_bench vec_bench.c && ./vec_bench
```

</details>

Limpieza:

```bash
rm -f vec_bench
```
