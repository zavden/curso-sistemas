# Redimensionamiento de tablas hash

## Objetivo

Estudiar en profundidad el mecanismo de **crecimiento y contracción**
de tablas hash — la pieza que convierte una estructura de tamaño
fijo en una estructura dinámica con $O(1)$ amortizado:

- **Cuándo crecer**: umbrales de $\alpha$ para chaining y open
  addressing; por qué no esperar demasiado.
- **Cuándo encoger**: umbrales de $\alpha$ mínimo, histéresis para
  evitar thrashing.
- **Cómo redimensionar**: rehash total, nueva capacidad, por qué
  todos los elementos deben reinsertarse.
- **Factor de crecimiento**: $\times 2$ vs $\times 1.5$ vs primos;
  implicaciones en memoria y rendimiento.
- **Análisis amortizado**: demostración formal de $O(1)$ amortizado
  por inserción usando el método del banquero y el método agregado.
- **Rehash incremental**: la alternativa de Redis para evitar
  pausas largas.
- Referencia: T03 (factor de carga) introdujo umbrales y
  mencionó el concepto. T04 cubrió rehash de limpieza (sin cambio
  de capacidad). Aquí analizamos el redimensionamiento completo.

---

## Por qué redimensionar

Una tabla hash de tamaño fijo $m$ solo funciona bien mientras
$\alpha = n/m$ se mantenga acotado. Sin redimensionamiento:

- **Chaining**: $\alpha$ crece sin límite. Con $n = 10m$, cada
  bucket tiene ~10 elementos → búsqueda $O(\alpha) = O(10)$.
  Funcional pero lento.
- **Open addressing**: cuando $\alpha \to 1$, las cadenas de
  sondeo crecen drásticamente. Con $\alpha = 0.95$, la fórmula
  de Knuth da ~200 sondeos para búsqueda fallida. Con $\alpha = 1$
  la tabla está llena y no se puede insertar nada.

```
Rendimiento de open addressing (sonda lineal, Knuth miss):

  α     sondeos_miss
  0.50     2.50
  0.70     5.56
  0.80    12.50
  0.90    50.50
  0.95   200.50
  0.99  5000.50

La degradación no es gradual — es catastrófica.
```

El redimensionamiento mantiene $\alpha$ en un rango saludable
aumentando $m$ cuando $n$ crece (o reduciéndolo cuando $n$ baja).

---

## Cuándo crecer (grow)

### Umbral de carga máxima

Se define un umbral $\alpha_{\max}$. Cuando una inserción haría
que $\alpha > \alpha_{\max}$, se duplica la tabla **antes** de
insertar:

```c
bool ht_insert(HashTable *t, int key, int value) {
    // check BEFORE insert
    if ((double)(t->count + 1) / t->capacity > LOAD_MAX) {
        ht_resize(t, t->capacity * 2);
    }
    // ... proceed with insert ...
}
```

Umbrales típicos en implementaciones reales:

| Implementación | Tipo | $\alpha_{\max}$ | Nuevo $\alpha$ tras grow |
|----------------|------|-----------------|--------------------------|
| Java HashMap | chaining | 0.75 | 0.375 |
| Python dict | open (custom) | 0.667 | 0.333 |
| Rust HashMap | open (Swiss Table) | 0.875 | ~0.44 |
| Go map | chaining (8/bucket) | 6.5 (por bucket) | ~3.25 |
| C++ unordered_map | chaining | 1.0 | 0.5 |
| Redis dict | chaining | 1.0 | 0.5 |

### ¿Por qué verificar antes de insertar?

Si verificamos **después**, la tabla temporalmente tiene
$\alpha > \alpha_{\max}$. En open addressing esto puede significar
cadenas de sondeo muy largas o incluso tabla llena. En chaining
es menos grave pero inconsistente con la garantía de rendimiento.

La convención es: **crecer antes de que $\alpha$ supere el umbral**.

---

## Cuándo encoger (shrink)

### Umbral de carga mínima

Si se eliminan muchos elementos, la tabla queda
sobredimensionada — desperdicia memoria sin beneficio. Se define
un umbral $\alpha_{\min}$:

```c
bool ht_delete(HashTable *t, int key) {
    // ... perform delete ...
    if (t->capacity > INITIAL_CAPACITY &&
        (double)t->count / t->capacity < LOAD_MIN) {
        ht_resize(t, t->capacity / 2);
    }
    return true;
}
```

Umbrales típicos:

| Implementación | $\alpha_{\min}$ para shrink | Notas |
|----------------|----------------------------|-------|
| Java HashMap | No encoge | Nunca reduce capacidad |
| Python dict | No encoge | Solo reduce en rehash |
| Rust HashMap | No encoge automáticamente | `shrink_to_fit()` manual |
| Redis dict | 0.1 | Encoge si $\alpha < 0.1$ |
| Go map | No encoge | Nunca reduce buckets |

La mayoría de implementaciones **no encogen automáticamente**.
¿Por qué?

1. **Thrashing**: si $\alpha$ oscila alrededor del umbral, la
   tabla crece y encoge repetidamente. Cada resize cuesta $O(n)$.
2. **Patrón de uso**: en muchas aplicaciones, los elementos
   eliminados se reemplazan pronto por otros nuevos.
3. **Memoria vs CPU**: desperdiciar memoria (array grande con
   pocos elementos) es preferible a desperdiciar CPU (rehashes
   innecesarios).

### Histéresis

Para evitar thrashing, los umbrales de grow y shrink deben tener
un **gap** suficiente. Si se crece al duplicar cuando
$\alpha > 0.75$, el nuevo $\alpha$ es $\approx 0.375$. Si se
encoge al dividir por 2 cuando $\alpha < 0.25$, el nuevo $\alpha$
es $\approx 0.5$. Este gap (0.25 a 0.75) evita oscilaciones:

```
                ← shrink (÷2)         grow (×2) →
  ──────|───────────────────────────|──────────
       0.25                       0.75
        ↓                          ↓
     α=0.50                     α=0.375

  Zona estable: 0.25 ≤ α ≤ 0.75
  No hay resize en esta zona → sin thrashing
```

La regla general: $\alpha_{\min} \leq \alpha_{\max} / 2$ cuando
el factor de crecimiento es $\times 2$. Esto garantiza que después
de un grow, $\alpha$ está lejos de $\alpha_{\min}$, y después de
un shrink, $\alpha$ está lejos de $\alpha_{\max}$.

---

## Cómo redimensionar: rehash total

### Por qué reinsertar todo

No se puede simplemente copiar los elementos a una tabla más
grande. La posición de cada elemento depende de $h(k) \bmod m$, y
con un nuevo $m' \neq m$, los índices cambian:

```
Tabla original m=8, h(k) = k % 8:
  slot: 0  1  2  3  4  5  6  7
        _  _  _  3  _  5  _  _

Tabla nueva m=16, h(k) = k % 16:
  ¿Copiar 3 al slot 3? h(3) % 16 = 3 → correcto (casualidad)
  ¿Copiar 5 al slot 5? h(5) % 16 = 5 → correcto (casualidad)

  Pero: clave 11, h(11)%8 = 3 → slot 3 en tabla vieja
        h(11)%16 = 11 → slot 11 en tabla nueva
  ¡NO es el mismo slot!
```

Cada elemento debe ser **reinsertado** en la nueva tabla usando
la función hash con el nuevo módulo. Esto rompe y reconstruye
todas las cadenas de sondeo.

### Implementación

```c
#define LOAD_MAX 0.75
#define LOAD_MIN 0.25
#define INITIAL_CAPACITY 8

typedef struct {
    int *keys;
    int *values;
    bool *occupied;
    int capacity;
    int count;
} HashTable;

void ht_resize(HashTable *t, int new_cap) {
    int old_cap = t->capacity;
    int *old_keys = t->keys;
    int *old_vals = t->values;
    bool *old_occ = t->occupied;

    t->keys = calloc(new_cap, sizeof(int));
    t->values = calloc(new_cap, sizeof(int));
    t->occupied = calloc(new_cap, sizeof(bool));
    t->capacity = new_cap;
    t->count = 0;

    for (int i = 0; i < old_cap; i++) {
        if (old_occ[i]) {
            ht_insert_no_resize(t, old_keys[i], old_vals[i]);
        }
    }

    free(old_keys);
    free(old_vals);
    free(old_occ);
}
```

Nótese `ht_insert_no_resize`: durante el rehash, las inserciones
**no deben disparar otro resize**. Se usa una versión interna que
omite la verificación de $\alpha$.

### Elección de nueva capacidad

| Estrategia | Nuevo $m$ | Ventaja | Desventaja |
|------------|-----------|---------|------------|
| Potencia de 2 | $m' = 2m$ | `% m` es bitwise AND; simple | $m$ no primo (peor distribución para hash por división) |
| Primo | $m' = \text{next\_prime}(2m)$ | Mejor distribución | Búsqueda de primo costosa; no se puede usar AND |
| Potencia de 2 + hash bueno | $m' = 2m$ con multiplicación | Lo mejor de ambos | Requiere hash multiplicativo (no solo `% m`) |

En la práctica, **potencia de 2 + hash de buena calidad** es lo
estándar. Si $h(k)$ ya distribuye bien (FNV-1a, SipHash, xxHash),
tomar los bits bajos con AND es suficiente:

```c
// fast modulo for power-of-2 capacity
int index = hash(key) & (capacity - 1);  // equivalent to % capacity
```

Python usa potencias de 2 con perturbación del hash. Rust
(hashbrown) usa potencias de 2 con SipHash. Java usa potencias de
2 desde Java 8 (antes usaba primos).

### Pico de memoria durante resize

Durante el rehash, ambas tablas (vieja y nueva) coexisten en
memoria:

```
Memoria durante resize de m a 2m:

  Tabla vieja: m slots   ─┐
  Tabla nueva: 2m slots  ─┤── total: 3m slots temporalmente
                           │
  Después: tabla vieja liberada → solo 2m slots

  Pico: 3× la memoria original
```

Con factor $\times 1.5$: pico = $m + 1.5m = 2.5m$ — un 17% menos.

Para tablas muy grandes (millones de entradas), este pico puede
ser problemático. Soluciones:
- **Rehash incremental** (Redis): migrar elementos gradualmente.
- **Pre-reservar** con `reserve()` o `with_capacity()` si se
  conoce el tamaño final.

---

## Factor de crecimiento: ×2 vs ×1.5 vs primos

### Factor ×2 (el estándar)

```
m:  8 → 16 → 32 → 64 → 128 → 256 → 512 → 1024
n max (α=0.75):
    6 → 12 → 24 → 48 →  96 → 192 → 384 →  768
```

Propiedades:
- **Número de resizes** para llegar de $m_0$ a $m$:
  $\log_2(m/m_0)$.
- Potencia de 2 permite `& (m-1)` en vez de `% m`.
- Cada resize duplica la capacidad → se amortiza sobre $m/2$
  inserciones.

### Factor ×1.5

Usado por algunos allocators (`realloc` en ciertas libc) y por
Facebook's F14 hash map:

```
m:  8 → 12 → 18 → 27 → 40 → 60 → 90 → 135 → 202
```

Ventaja: menor pico de memoria ($2.5m$ vs $3m$). Desventaja: más
resizes ($\log_{1.5}(m/m_0)$ vs $\log_2(m/m_0)$) y pierde la
optimización de potencia de 2.

### Secuencia de primos

Usada por C++ `libstdc++ unordered_map` (GCC):

```
m:  7 → 17 → 37 → 79 → 167 → 337 → 677 → 1361
```

Cada primo es aproximadamente el doble del anterior. La búsqueda
del siguiente primo se hace en una tabla precalculada.

### Comparación

| Factor | Resizes para $n = 10^6$ (desde $m_0=8$) | Módulo | Pico memoria |
|--------|----------------------------------------|--------|--------------|
| $\times 2$ | $\lceil\log_2(10^6/6)\rceil = 18$ | `& (m-1)` | $3m$ |
| $\times 1.5$ | $\lceil\log_{1.5}(10^6/6)\rceil = 30$ | `% m` | $2.5m$ |
| Primos (~×2) | ~18 | `% m` | $3m$ |

---

## Análisis amortizado: demostración de O(1)

### Método agregado

Partimos de una tabla vacía de capacidad $m_0$ con umbral
$\alpha_{\max}$, e insertamos $n$ elementos. Los resizes ocurren
cuando $n$ supera $\alpha_{\max} \cdot m_0, \alpha_{\max} \cdot 2m_0,
\alpha_{\max} \cdot 4m_0, \ldots$

Costo de cada inserción: $O(1)$ para la inserción misma.

Costo de los resizes: en el $k$-ésimo resize, la tabla tiene
$\approx \alpha_{\max} \cdot 2^k m_0$ elementos que se reinsertan,
cada uno a costo $O(1)$. El costo total de **todos** los resizes
hasta llegar a $n$ elementos es:

$$\sum_{k=0}^{\log_2(n / (\alpha_{\max} m_0))} \alpha_{\max} \cdot 2^k m_0 = \alpha_{\max} m_0 \cdot \frac{2^{K+1} - 1}{2 - 1} < 2n$$

donde $K$ es el último resize. La suma geométrica está dominada
por el último término, que es $\approx n$. Así:

$$\text{Costo total} = O(n) + O(2n) = O(n)$$

$$\text{Costo amortizado por inserción} = \frac{O(n)}{n} = O(1)$$

### Método del banquero (análisis con créditos)

Cada inserción paga **3 unidades** de costo:
- 1 unidad para la inserción misma.
- 2 unidades se depositan como **crédito** en la tabla.

Cuando ocurre un resize de $m$ a $2m$, hay $\alpha_{\max} \cdot m$
elementos que reinsertar. Desde el último resize (o desde el
inicio), se han insertado $\alpha_{\max} \cdot m / 2$ elementos
nuevos (la tabla pasó de $\alpha_{\max}/2$ a $\alpha_{\max}$), y
cada uno dejó 2 créditos. Total de créditos disponibles:

$$\text{créditos} = 2 \cdot \frac{\alpha_{\max} \cdot m}{2} = \alpha_{\max} \cdot m$$

Esto es exactamente el número de elementos a reinsertar. Los
créditos cubren el costo del resize. Como cada inserción paga $O(3)
= O(1)$ unidades, el costo amortizado es $O(1)$.

### Visualización

```
Inserciones con grow ×2, α_max = 0.75, m₀ = 4:

Insert #  n  m   α      Costo   Créditos
   1      1  4   0.25   1+2=3   2
   2      2  4   0.50   1+2=3   4
   3      3  4   0.75   1+2=3   6
   4      ─── RESIZE: m=4→8, reinsertar 3 ───
          4  8   0.50   3+1+2   6-3+2=5
   5      5  8   0.625  1+2=3   7
   6      6  8   0.75   1+2=3   9
   7      ─── RESIZE: m=8→16, reinsertar 6 ───
          7  16  0.44   6+1+2   9-6+2=5
   8-12   ...                   acumulando
  12      ─── RESIZE: m=16→32 ───
          ...

  Créditos ≥ 0 siempre → sistema solvent → O(1) amortizado ✓
```

### Cuidado: amortizado ≠ constante

El costo **individual** de una inserción que dispara resize es
$O(n)$, no $O(1)$. Lo que es $O(1)$ es el promedio sobre $n$
operaciones. En sistemas de tiempo real (embedded, trading),
un pico de $O(n)$ puede ser inaceptable. Alternativas:
- Rehash incremental.
- Pre-reservar capacidad.
- Usar estructuras de tamaño fijo.

---

## Rehash incremental

### El problema de Redis

Redis es single-threaded. Un rehash de una tabla con 10 millones
de claves bloquearía el servidor durante ~100ms — inaceptable para
un sistema que promete latencias de microsegundos.

### La solución: dos tablas

Redis mantiene **dos tablas** durante el rehash:
- `ht[0]`: tabla vieja (se va vaciando).
- `ht[1]`: tabla nueva (se va llenando).

Cada operación (insert, search, delete) migra **1 bucket** de
`ht[0]` a `ht[1]` además de realizar la operación solicitada.
Las búsquedas consultan **ambas tablas**:

```
Estado durante rehash incremental:

  ht[0]:  [a, _, c, d, _, _, _, _]  ← vaciando
                  rehash_idx = 2    ← próximo bucket a migrar
  ht[1]:  [_, b, _, _, _, _, _, _, _, _, _, _, _, _, _, _]
           ↑
           b migrado de ht[0]

  Insert(k): insertar en ht[1], migrar bucket rehash_idx de ht[0]
  Search(k): buscar en ht[0] primero, luego en ht[1]
  Migrate:   mover todos los elementos de ht[0][rehash_idx] a ht[1]
             rehash_idx++
```

### Implementación simplificada

```c
typedef struct {
    // two tables
    int *keys[2];
    int *values[2];
    bool *occupied[2];
    int capacity[2];
    int count[2];

    bool rehashing;    // is incremental rehash in progress?
    int rehash_idx;    // next bucket to migrate from ht[0]
} IncrementalTable;

void incr_migrate_one_bucket(IncrementalTable *t) {
    if (!t->rehashing) return;

    // skip empty buckets
    while (t->rehash_idx < t->capacity[0] &&
           !t->occupied[0][t->rehash_idx]) {
        t->rehash_idx++;
    }

    if (t->rehash_idx >= t->capacity[0]) {
        // migration complete
        free(t->keys[0]);
        free(t->values[0]);
        free(t->occupied[0]);

        t->keys[0] = t->keys[1];
        t->values[0] = t->values[1];
        t->occupied[0] = t->occupied[1];
        t->capacity[0] = t->capacity[1];
        t->count[0] = t->count[1];

        t->keys[1] = NULL;
        t->values[1] = NULL;
        t->occupied[1] = NULL;
        t->capacity[1] = 0;
        t->count[1] = 0;

        t->rehashing = false;
        return;
    }

    // migrate this bucket (for open addressing: just this slot)
    int key = t->keys[0][t->rehash_idx];
    int val = t->values[0][t->rehash_idx];
    t->occupied[0][t->rehash_idx] = false;
    t->count[0]--;

    // insert into ht[1]
    int idx = ((key % t->capacity[1]) + t->capacity[1]) % t->capacity[1];
    while (t->occupied[1][idx]) {
        idx = (idx + 1) % t->capacity[1];
    }
    t->keys[1][idx] = key;
    t->values[1][idx] = val;
    t->occupied[1][idx] = true;
    t->count[1]++;

    t->rehash_idx++;
}
```

### Costo por operación durante rehash

Cada operación normal gasta $O(1)$ extra para migrar un bucket.
Para chaining, un bucket puede tener varios elementos
($\alpha_{\max}$ en promedio); para open addressing, es un solo
slot. El costo total del rehash se distribuye sobre $m$ operaciones
(una por bucket), manteniendo $O(1)$ por operación incluso en el
peor caso individual.

### Trade-offs del rehash incremental

| Aspecto | Rehash total | Rehash incremental |
|---------|-------------|-------------------|
| Latencia pico | $O(n)$ | $O(1)$ por operación |
| Throughput total | Menor overhead | Mayor overhead (buscar en 2 tablas) |
| Memoria pico | $3m$ temporal | $3m$ prolongado durante migración |
| Complejidad | Simple | Mucho más compleja |
| Uso típico | General | Sistemas de baja latencia (Redis) |

---

## Shrink con cuidado

### Capacidad mínima

Nunca encoger por debajo de una capacidad mínima (e.g., 8 o 16
slots). Esto evita:
- Tablas de 1-2 slots con colisiones constantes.
- Resizes frecuentes para tablas pequeñas (el overhead de
  `malloc`/`free` domina).

```c
void ht_maybe_shrink(HashTable *t) {
    if (t->capacity <= INITIAL_CAPACITY) return;
    if ((double)t->count / t->capacity < LOAD_MIN) {
        int new_cap = t->capacity / 2;
        if (new_cap < INITIAL_CAPACITY) {
            new_cap = INITIAL_CAPACITY;
        }
        ht_resize(t, new_cap);
    }
}
```

### Shrink en Rust

Rust `HashMap` no encoge automáticamente pero ofrece:

```rust
let mut map = HashMap::new();
// ... insert many, then remove many ...
map.shrink_to_fit();       // reduce to minimum needed
map.shrink_to(100);        // reduce to hold at least 100 elements
```

`shrink_to_fit` ajusta la capacidad a la potencia de 2 mínima que
satisface $n / m \leq \alpha_{\max}$. Esto puede liberar
cantidades significativas de memoria:

```
Ejemplo: HashMap con 100 elementos, capacity = 16384
  (insertamos 10000, eliminamos 9900)

  shrink_to_fit():
    100 / 0.875 ≈ 115 → next power of 2 = 128
    Memoria: 16384 × entry_size → 128 × entry_size
    Ahorro: ~99% de la memoria
```

---

## Programa completo en C

```c
// resizing.c
// Dynamic resizing of hash tables: grow, shrink, amortized analysis,
// incremental rehash
// Compile: gcc -O2 -o resizing resizing.c -lm

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <time.h>

// ============================================================
// Dynamic hash table with grow and shrink (linear probing)
// ============================================================

#define LOAD_MAX 0.75
#define LOAD_MIN 0.25
#define INITIAL_CAP 8

typedef struct {
    int *keys;
    int *values;
    bool *occupied;
    int capacity;
    int count;
    int resize_count;  // track number of resizes
    long rehash_ops;   // track total rehash insertions
} DynTable;

DynTable *dt_create(int capacity) {
    DynTable *t = malloc(sizeof(DynTable));
    t->keys = calloc(capacity, sizeof(int));
    t->values = calloc(capacity, sizeof(int));
    t->occupied = calloc(capacity, sizeof(bool));
    t->capacity = capacity;
    t->count = 0;
    t->resize_count = 0;
    t->rehash_ops = 0;
    return t;
}

void dt_free(DynTable *t) {
    free(t->keys);
    free(t->values);
    free(t->occupied);
    free(t);
}

// internal insert (no resize check)
static bool dt_insert_internal(DynTable *t, int key, int value) {
    int idx = ((key % t->capacity) + t->capacity) % t->capacity;
    for (int i = 0; i < t->capacity; i++) {
        if (!t->occupied[idx]) {
            t->keys[idx] = key;
            t->values[idx] = value;
            t->occupied[idx] = true;
            t->count++;
            return true;
        }
        if (t->keys[idx] == key) {
            t->values[idx] = value;
            return false;
        }
        idx = (idx + 1) % t->capacity;
    }
    return false;
}

void dt_resize(DynTable *t, int new_cap) {
    int old_cap = t->capacity;
    int *old_keys = t->keys;
    int *old_vals = t->values;
    bool *old_occ = t->occupied;

    t->keys = calloc(new_cap, sizeof(int));
    t->values = calloc(new_cap, sizeof(int));
    t->occupied = calloc(new_cap, sizeof(bool));
    t->capacity = new_cap;
    int old_count = t->count;
    t->count = 0;

    for (int i = 0; i < old_cap; i++) {
        if (old_occ[i]) {
            dt_insert_internal(t, old_keys[i], old_vals[i]);
        }
    }

    t->resize_count++;
    t->rehash_ops += old_count;

    free(old_keys);
    free(old_vals);
    free(old_occ);
}

bool dt_insert(DynTable *t, int key, int value) {
    if ((double)(t->count + 1) / t->capacity > LOAD_MAX) {
        dt_resize(t, t->capacity * 2);
    }
    return dt_insert_internal(t, key, value);
}

int dt_search(DynTable *t, int key) {
    int idx = ((key % t->capacity) + t->capacity) % t->capacity;
    for (int i = 0; i < t->capacity; i++) {
        if (!t->occupied[idx]) return -1;
        if (t->keys[idx] == key) return t->values[idx];
        idx = (idx + 1) % t->capacity;
    }
    return -1;
}

bool dt_delete(DynTable *t, int key) {
    int idx = ((key % t->capacity) + t->capacity) % t->capacity;

    bool found = false;
    for (int i = 0; i < t->capacity; i++) {
        if (!t->occupied[idx]) return false;
        if (t->keys[idx] == key) { found = true; break; }
        idx = (idx + 1) % t->capacity;
    }
    if (!found) return false;

    // backward shift delete
    int gap = idx;
    int j = (gap + 1) % t->capacity;
    while (t->occupied[j]) {
        int h = ((t->keys[j] % t->capacity) + t->capacity) % t->capacity;
        bool shift;
        if (gap < j) {
            shift = (h <= gap) || (h > j);
        } else {
            shift = (h <= gap) && (h > j);
        }
        if (shift) {
            t->keys[gap] = t->keys[j];
            t->values[gap] = t->values[j];
            gap = j;
        }
        j = (j + 1) % t->capacity;
    }
    t->occupied[gap] = false;
    t->count--;

    // shrink check
    if (t->capacity > INITIAL_CAP &&
        (double)t->count / t->capacity < LOAD_MIN) {
        int new_cap = t->capacity / 2;
        if (new_cap < INITIAL_CAP) new_cap = INITIAL_CAP;
        dt_resize(t, new_cap);
    }

    return true;
}

// ============================================================
// Incremental rehash table
// ============================================================

typedef struct {
    int *keys[2];
    int *values[2];
    bool *occupied[2];
    int capacity[2];
    int count[2];
    bool rehashing;
    int rehash_idx;
    int migrations;  // count total migrations
} IncrTable;

IncrTable *it_create(int capacity) {
    IncrTable *t = calloc(1, sizeof(IncrTable));
    t->keys[0] = calloc(capacity, sizeof(int));
    t->values[0] = calloc(capacity, sizeof(int));
    t->occupied[0] = calloc(capacity, sizeof(bool));
    t->capacity[0] = capacity;
    return t;
}

void it_free(IncrTable *t) {
    free(t->keys[0]); free(t->values[0]); free(t->occupied[0]);
    if (t->rehashing) {
        free(t->keys[1]); free(t->values[1]); free(t->occupied[1]);
    }
    free(t);
}

static void it_insert_to(int *keys, int *values, bool *occupied,
                          int capacity, int *count, int key, int value) {
    int idx = ((key % capacity) + capacity) % capacity;
    while (occupied[idx]) {
        if (keys[idx] == key) { values[idx] = value; return; }
        idx = (idx + 1) % capacity;
    }
    keys[idx] = key;
    values[idx] = value;
    occupied[idx] = true;
    (*count)++;
}

static void it_migrate_step(IncrTable *t) {
    if (!t->rehashing) return;

    // migrate one occupied slot
    while (t->rehash_idx < t->capacity[0] &&
           !t->occupied[0][t->rehash_idx]) {
        t->rehash_idx++;
    }

    if (t->rehash_idx >= t->capacity[0]) {
        // done
        free(t->keys[0]);
        free(t->values[0]);
        free(t->occupied[0]);

        t->keys[0] = t->keys[1];
        t->values[0] = t->values[1];
        t->occupied[0] = t->occupied[1];
        t->capacity[0] = t->capacity[1];
        t->count[0] = t->count[1];

        t->keys[1] = NULL;
        t->values[1] = NULL;
        t->occupied[1] = NULL;
        t->capacity[1] = 0;
        t->count[1] = 0;
        t->rehashing = false;
        return;
    }

    int key = t->keys[0][t->rehash_idx];
    int val = t->values[0][t->rehash_idx];
    t->occupied[0][t->rehash_idx] = false;
    t->count[0]--;

    it_insert_to(t->keys[1], t->values[1], t->occupied[1],
                 t->capacity[1], &t->count[1], key, val);
    t->rehash_idx++;
    t->migrations++;
}

static void it_start_rehash(IncrTable *t, int new_cap) {
    t->keys[1] = calloc(new_cap, sizeof(int));
    t->values[1] = calloc(new_cap, sizeof(int));
    t->occupied[1] = calloc(new_cap, sizeof(bool));
    t->capacity[1] = new_cap;
    t->count[1] = 0;
    t->rehashing = true;
    t->rehash_idx = 0;
}

bool it_insert(IncrTable *t, int key, int value) {
    if (t->rehashing) {
        it_migrate_step(t);
    }

    int total = t->count[0] + t->count[1];
    int cap = t->rehashing ? t->capacity[1] : t->capacity[0];

    if (!t->rehashing && (double)(total + 1) / cap > LOAD_MAX) {
        it_start_rehash(t, t->capacity[0] * 2);
        it_migrate_step(t);
    }

    // insert into active table (ht[1] during rehash, ht[0] otherwise)
    int tbl = t->rehashing ? 1 : 0;
    it_insert_to(t->keys[tbl], t->values[tbl], t->occupied[tbl],
                 t->capacity[tbl], &t->count[tbl], key, value);
    return true;
}

int it_search(IncrTable *t, int key) {
    if (t->rehashing) {
        it_migrate_step(t);
    }

    // search ht[0]
    int idx = ((key % t->capacity[0]) + t->capacity[0]) % t->capacity[0];
    for (int i = 0; i < t->capacity[0]; i++) {
        if (!t->occupied[0][idx]) break;
        if (t->keys[0][idx] == key) return t->values[0][idx];
        idx = (idx + 1) % t->capacity[0];
    }

    // search ht[1] if rehashing
    if (t->rehashing) {
        idx = ((key % t->capacity[1]) + t->capacity[1]) % t->capacity[1];
        for (int i = 0; i < t->capacity[1]; i++) {
            if (!t->occupied[1][idx]) break;
            if (t->keys[1][idx] == key) return t->values[1][idx];
            idx = (idx + 1) % t->capacity[1];
        }
    }

    return -1;
}

// ============================================================
// Demo functions
// ============================================================

void demo1_grow_trace(void) {
    printf("=== Demo 1: grow trace ===\n");
    DynTable *t = dt_create(INITIAL_CAP);

    printf("  m₀=%d, α_max=%.2f\n\n", INITIAL_CAP, LOAD_MAX);
    printf("  %-8s %-5s %-6s %-6s %-8s\n",
           "insert", "n", "m", "alpha", "resize?");

    for (int i = 1; i <= 50; i++) {
        int old_cap = t->capacity;
        dt_insert(t, i * 7, i);
        bool resized = (t->capacity != old_cap);
        if (i <= 20 || resized) {
            printf("  %-8d %-5d %-6d %-6.3f %s\n",
                   i, t->count, t->capacity,
                   (double)t->count / t->capacity,
                   resized ? "YES" : "");
        }
    }

    printf("\n  Total resizes: %d\n", t->resize_count);
    printf("  Total rehash insertions: %ld\n", t->rehash_ops);
    printf("  Amortized cost per insert: %.2f\n\n",
           (50.0 + t->rehash_ops) / 50.0);

    dt_free(t);
}

void demo2_shrink_trace(void) {
    printf("=== Demo 2: shrink trace ===\n");
    DynTable *t = dt_create(INITIAL_CAP);

    // insert 100 elements
    for (int i = 0; i < 100; i++) {
        dt_insert(t, i, i);
    }
    printf("  After 100 inserts: n=%d, m=%d, α=%.3f\n",
           t->count, t->capacity,
           (double)t->count / t->capacity);

    int old_resizes = t->resize_count;

    // delete 90 elements
    printf("\n  Deleting 90 elements...\n");
    for (int i = 0; i < 90; i++) {
        int old_cap = t->capacity;
        dt_delete(t, i);
        if (t->capacity != old_cap) {
            printf("    n=%d → shrink m=%d→%d (α=%.3f)\n",
                   t->count, old_cap, t->capacity,
                   (double)t->count / t->capacity);
        }
    }

    printf("\n  After 90 deletes: n=%d, m=%d, α=%.3f\n",
           t->count, t->capacity,
           (double)t->count / t->capacity);
    printf("  Shrink resizes: %d\n\n", t->resize_count - old_resizes);

    // verify remaining elements
    int found = 0;
    for (int i = 90; i < 100; i++) {
        if (dt_search(t, i) == i) found++;
    }
    printf("  Remaining elements verified: %d/10\n\n", found);

    dt_free(t);
}

void demo3_amortized_cost(void) {
    printf("=== Demo 3: amortized cost analysis ===\n");
    printf("  %-10s %-10s %-12s %-10s\n",
           "n", "resizes", "rehash_ops", "amortized");

    int sizes[] = {100, 1000, 10000, 100000, 0};

    for (int s = 0; sizes[s] != 0; s++) {
        int n = sizes[s];
        DynTable *t = dt_create(INITIAL_CAP);

        for (int i = 0; i < n; i++) {
            dt_insert(t, i * 13 + 7, i);
        }

        double amortized = ((double)n + t->rehash_ops) / n;
        printf("  %-10d %-10d %-12ld %-10.3f\n",
               n, t->resize_count, t->rehash_ops, amortized);

        dt_free(t);
    }
    printf("\n  Amortized cost converges to ~2.33 (1 + 1/0.75)\n");
    printf("  This is O(1) regardless of n.\n\n");
}

void demo4_growth_factor_comparison(void) {
    printf("=== Demo 4: growth factor ×2 vs ×1.5 ===\n");
    int n = 100000;

    // factor ×2
    DynTable *t2 = dt_create(INITIAL_CAP);
    for (int i = 0; i < n; i++) {
        dt_insert(t2, i, i);
    }

    // factor ×1.5 (manual simulation)
    int cap_15 = INITIAL_CAP;
    int count_15 = 0;
    int resizes_15 = 0;
    long rehash_15 = 0;
    int peak_15 = 0;

    for (int i = 0; i < n; i++) {
        count_15++;
        if ((double)count_15 / cap_15 > LOAD_MAX) {
            int new_cap = (int)(cap_15 * 1.5);
            if (new_cap == cap_15) new_cap++;
            int peak = cap_15 + new_cap;
            if (peak > peak_15) peak_15 = peak;
            rehash_15 += count_15;
            cap_15 = new_cap;
            resizes_15++;
        }
    }

    printf("  n = %d\n\n", n);
    printf("  %-12s %-8s %-10s %-12s %-12s\n",
           "Factor", "m_final", "resizes", "rehash_ops", "peak_mem");
    printf("  %-12s %-8d %-10d %-12ld %-12d\n",
           "×2", t2->capacity, t2->resize_count, t2->rehash_ops,
           t2->capacity + t2->capacity / 2);  // peak = old + new = m/2 + m
    printf("  %-12s %-8d %-10d %-12ld %-12d\n\n",
           "×1.5", cap_15, resizes_15, rehash_15, peak_15);

    dt_free(t2);
}

void demo5_incremental_rehash(void) {
    printf("=== Demo 5: incremental rehash ===\n");
    IncrTable *t = it_create(INITIAL_CAP);

    printf("  Inserting 50 elements with incremental rehash:\n");
    for (int i = 1; i <= 50; i++) {
        it_insert(t, i * 7, i);
        if (i <= 10 || i % 10 == 0) {
            printf("    insert #%d: ht[0] cap=%d n=%d",
                   i, t->capacity[0], t->count[0]);
            if (t->rehashing) {
                printf("  |  ht[1] cap=%d n=%d (migrating...)",
                       t->capacity[1], t->count[1]);
            }
            printf("\n");
        }
    }

    printf("  Total migrations: %d\n", t->migrations);

    // verify all elements
    int found = 0;
    for (int i = 1; i <= 50; i++) {
        if (it_search(t, i * 7) == i) found++;
    }
    printf("  All elements found: %d/50\n\n", found);

    it_free(t);
}

void demo6_thrashing_demo(void) {
    printf("=== Demo 6: thrashing without hysteresis ===\n");

    // simulate without hysteresis: grow at 0.5, shrink at 0.25
    // after grow: α ≈ 0.25 → immediately triggers shrink!
    int m = 16;
    int n = 8;  // α = 0.5 → triggers grow

    printf("  Bad thresholds: grow at α>0.5, shrink at α<0.25\n\n");

    int resizes = 0;
    for (int op = 0; op < 10; op++) {
        if (op % 2 == 0) {
            // insert
            n++;
            double alpha = (double)n / m;
            printf("  insert: n=%d, m=%d, α=%.3f", n, m, alpha);
            if (alpha > 0.5) {
                m *= 2;
                resizes++;
                printf(" → GROW to m=%d (α=%.3f)", m, (double)n / m);
            }
            printf("\n");
        } else {
            // delete
            n--;
            double alpha = (double)n / m;
            printf("  delete: n=%d, m=%d, α=%.3f", n, m, alpha);
            if (alpha < 0.25) {
                m /= 2;
                resizes++;
                printf(" → SHRINK to m=%d (α=%.3f)", m, (double)n / m);
            }
            printf("\n");
        }
    }
    printf("\n  Total resizes: %d (thrashing!)\n", resizes);

    printf("\n  Good thresholds: grow at α>0.75, shrink at α<0.25\n");
    printf("  After grow: α ≈ 0.375 — far from shrink threshold\n");
    printf("  After shrink: α ≈ 0.50 — far from grow threshold\n");
    printf("  → Stable zone [0.25, 0.75] prevents thrashing\n\n");
}

int main(void) {
    demo1_grow_trace();
    demo2_shrink_trace();
    demo3_amortized_cost();
    demo4_growth_factor_comparison();
    demo5_incremental_rehash();
    demo6_thrashing_demo();
    return 0;
}
```

---

## Programa completo en Rust

```rust
// resizing.rs
// Dynamic resizing of hash tables: grow, shrink, amortized analysis
// Run: rustc -O resizing.rs && ./resizing

use std::collections::HashMap;

// ============================================================
// Dynamic hash table with grow and shrink
// ============================================================

const LOAD_MAX: f64 = 0.75;
const LOAD_MIN: f64 = 0.25;
const INITIAL_CAP: usize = 8;

struct DynTable {
    keys: Vec<i64>,
    values: Vec<i64>,
    occupied: Vec<bool>,
    capacity: usize,
    count: usize,
    resize_count: usize,
    rehash_ops: usize,
}

impl DynTable {
    fn new(capacity: usize) -> Self {
        Self {
            keys: vec![0; capacity],
            values: vec![0; capacity],
            occupied: vec![false; capacity],
            capacity,
            count: 0,
            resize_count: 0,
            rehash_ops: 0,
        }
    }

    fn hash(&self, key: i64) -> usize {
        (key.rem_euclid(self.capacity as i64)) as usize
    }

    fn insert_internal(&mut self, key: i64, value: i64) -> bool {
        let mut idx = self.hash(key);
        for _ in 0..self.capacity {
            if !self.occupied[idx] {
                self.keys[idx] = key;
                self.values[idx] = value;
                self.occupied[idx] = true;
                self.count += 1;
                return true;
            }
            if self.keys[idx] == key {
                self.values[idx] = value;
                return false;
            }
            idx = (idx + 1) % self.capacity;
        }
        false
    }

    fn resize(&mut self, new_cap: usize) {
        let old_keys = std::mem::replace(&mut self.keys, vec![0; new_cap]);
        let old_vals = std::mem::replace(&mut self.values, vec![0; new_cap]);
        let old_occ = std::mem::replace(&mut self.occupied, vec![false; new_cap]);
        let old_count = self.count;

        self.capacity = new_cap;
        self.count = 0;

        for i in 0..old_keys.len() {
            if old_occ[i] {
                self.insert_internal(old_keys[i], old_vals[i]);
            }
        }

        self.resize_count += 1;
        self.rehash_ops += old_count;
    }

    fn insert(&mut self, key: i64, value: i64) -> bool {
        if (self.count + 1) as f64 / self.capacity as f64 > LOAD_MAX {
            let new_cap = self.capacity * 2;
            self.resize(new_cap);
        }
        self.insert_internal(key, value)
    }

    fn search(&self, key: i64) -> Option<i64> {
        let mut idx = self.hash(key);
        for _ in 0..self.capacity {
            if !self.occupied[idx] { return None; }
            if self.keys[idx] == key { return Some(self.values[idx]); }
            idx = (idx + 1) % self.capacity;
        }
        None
    }

    fn delete(&mut self, key: i64) -> bool {
        let mut idx = self.hash(key);
        let mut found = false;
        for _ in 0..self.capacity {
            if !self.occupied[idx] { return false; }
            if self.keys[idx] == key { found = true; break; }
            idx = (idx + 1) % self.capacity;
        }
        if !found { return false; }

        // backward shift
        let mut gap = idx;
        let mut j = (gap + 1) % self.capacity;
        while self.occupied[j] {
            let h = (self.keys[j].rem_euclid(self.capacity as i64)) as usize;
            let shift = if gap < j {
                h <= gap || h > j
            } else {
                h <= gap && h > j
            };
            if shift {
                self.keys[gap] = self.keys[j];
                self.values[gap] = self.values[j];
                gap = j;
            }
            j = (j + 1) % self.capacity;
        }
        self.occupied[gap] = false;
        self.count -= 1;

        // shrink check
        if self.capacity > INITIAL_CAP
            && (self.count as f64 / self.capacity as f64) < LOAD_MIN
        {
            let new_cap = std::cmp::max(self.capacity / 2, INITIAL_CAP);
            self.resize(new_cap);
        }
        true
    }
}

// ============================================================
// Demos
// ============================================================

fn demo1_grow_trace() {
    println!("=== Demo 1: grow trace ===");
    let mut t = DynTable::new(INITIAL_CAP);

    println!("  m₀={}, α_max={:.2}\n", INITIAL_CAP, LOAD_MAX);
    println!("  {:>8} {:>5} {:>6} {:>6} {:>8}",
             "insert", "n", "m", "alpha", "resize?");

    for i in 1..=50i64 {
        let old_cap = t.capacity;
        t.insert(i * 7, i);
        let resized = t.capacity != old_cap;
        if i <= 20 || resized {
            println!("  {:>8} {:>5} {:>6} {:>6.3} {}",
                     i, t.count, t.capacity,
                     t.count as f64 / t.capacity as f64,
                     if resized { "YES" } else { "" });
        }
    }

    println!("\n  Total resizes: {}", t.resize_count);
    println!("  Total rehash ops: {}", t.rehash_ops);
    println!("  Amortized cost: {:.2}\n",
             (50.0 + t.rehash_ops as f64) / 50.0);
}

fn demo2_shrink_trace() {
    println!("=== Demo 2: shrink trace ===");
    let mut t = DynTable::new(INITIAL_CAP);

    for i in 0..100i64 {
        t.insert(i, i);
    }
    println!("  After 100 inserts: n={}, m={}, α={:.3}",
             t.count, t.capacity, t.count as f64 / t.capacity as f64);

    let resizes_before = t.resize_count;
    println!("\n  Deleting 90 elements...");

    for i in 0..90i64 {
        let old_cap = t.capacity;
        t.delete(i);
        if t.capacity != old_cap {
            println!("    n={} → shrink m={}→{} (α={:.3})",
                     t.count, old_cap, t.capacity,
                     t.count as f64 / t.capacity as f64);
        }
    }

    println!("\n  After 90 deletes: n={}, m={}, α={:.3}",
             t.count, t.capacity, t.count as f64 / t.capacity as f64);
    println!("  Shrink resizes: {}", t.resize_count - resizes_before);

    let found = (90..100i64).filter(|i| t.search(*i) == Some(*i)).count();
    println!("  Remaining verified: {}/10\n", found);
}

fn demo3_amortized_cost() {
    println!("=== Demo 3: amortized cost analysis ===");
    println!("  {:>10} {:>10} {:>12} {:>10}",
             "n", "resizes", "rehash_ops", "amortized");

    for &n in &[100, 1_000, 10_000, 100_000] {
        let mut t = DynTable::new(INITIAL_CAP);
        for i in 0..n as i64 {
            t.insert(i * 13 + 7, i);
        }
        let amortized = (n as f64 + t.rehash_ops as f64) / n as f64;
        println!("  {:>10} {:>10} {:>12} {:>10.3}",
                 n, t.resize_count, t.rehash_ops, amortized);
    }
    println!("\n  Converges to ~2.33 → O(1) amortized\n");
}

fn demo4_rust_hashmap_capacity() {
    println!("=== Demo 4: Rust HashMap capacity behavior ===");

    let mut map: HashMap<i32, i32> = HashMap::new();
    println!("  Empty:     len={}, cap={}", map.len(), map.capacity());

    for i in 0..100 {
        map.insert(i, i);
        if map.len() == 1 || map.len() == 4 || map.len() == 8
           || map.len() == 16 || map.len() == 32 || map.len() == 64
           || map.len() == 100 {
            println!("  len={:>4}: cap={}", map.len(), map.capacity());
        }
    }

    // with_capacity
    let map2: HashMap<i32, i32> = HashMap::with_capacity(100);
    println!("\n  with_capacity(100): len={}, cap={}",
             map2.len(), map2.capacity());

    // shrink_to_fit
    for i in 0..80 {
        map.remove(&i);
    }
    println!("\n  After removing 80 of 100:");
    println!("  Before shrink: len={}, cap={}", map.len(), map.capacity());
    map.shrink_to_fit();
    println!("  After shrink:  len={}, cap={}", map.len(), map.capacity());
    println!();
}

fn demo5_growth_factor_comparison() {
    println!("=== Demo 5: growth factor ×2 vs ×1.5 ===");
    let n = 100_000;

    // ×2
    let mut t2 = DynTable::new(INITIAL_CAP);
    for i in 0..n as i64 {
        t2.insert(i, i);
    }

    // ×1.5 simulation
    let mut cap: usize = INITIAL_CAP;
    let mut count: usize = 0;
    let mut resizes = 0usize;
    let mut rehash_ops = 0usize;

    for _ in 0..n {
        count += 1;
        if count as f64 / cap as f64 > LOAD_MAX {
            let new_cap = (cap as f64 * 1.5) as usize;
            rehash_ops += count;
            cap = new_cap;
            resizes += 1;
        }
    }

    println!("  n = {}\n", n);
    println!("  {:>8} {:>10} {:>10} {:>12}",
             "Factor", "m_final", "resizes", "rehash_ops");
    println!("  {:>8} {:>10} {:>10} {:>12}",
             "×2", t2.capacity, t2.resize_count, t2.rehash_ops);
    println!("  {:>8} {:>10} {:>10} {:>12}\n",
             "×1.5", cap, resizes, rehash_ops);
}

fn demo6_pre_allocate() {
    println!("=== Demo 6: pre-allocating vs dynamic growth ===");
    let n = 50_000;

    // dynamic
    let mut dynamic = DynTable::new(INITIAL_CAP);
    for i in 0..n as i64 {
        dynamic.insert(i, i);
    }

    // pre-allocated (capacity that avoids any resize)
    let pre_cap = ((n as f64 / LOAD_MAX).ceil() as usize).next_power_of_two();
    let mut prealloc = DynTable::new(pre_cap);
    for i in 0..n as i64 {
        prealloc.insert(i, i);
    }

    println!("  n = {}\n", n);
    println!("  {:>14} {:>10} {:>10} {:>12}",
             "Strategy", "m_final", "resizes", "rehash_ops");
    println!("  {:>14} {:>10} {:>10} {:>12}",
             "dynamic", dynamic.capacity, dynamic.resize_count, dynamic.rehash_ops);
    println!("  {:>14} {:>10} {:>10} {:>12}",
             "pre-alloc", prealloc.capacity, prealloc.resize_count, prealloc.rehash_ops);
    println!("\n  Pre-allocating eliminates ALL resizes and rehash cost.");

    // Rust equivalent
    println!("  Rust: HashMap::with_capacity({}) → cap={}\n",
             n, {
                let m: HashMap<i32,i32> = HashMap::with_capacity(n);
                m.capacity()
             });
}

fn main() {
    demo1_grow_trace();
    demo2_shrink_trace();
    demo3_amortized_cost();
    demo4_rust_hashmap_capacity();
    demo5_growth_factor_comparison();
    demo6_pre_allocate();
}
```

---

## Ejercicios

### Ejercicio 1 — Traza de crecimiento

Tabla con $m_0 = 4$, sonda lineal, $\alpha_{\max} = 0.75$,
factor $\times 2$. Inserta las claves 10, 22, 35, 5, 17, 8, 44,
13, 29, 41 en ese orden. Para cada inserción, indica $n$, $m$,
$\alpha$, y si ocurre resize.

<details><summary>¿En qué inserciones ocurre resize?</summary>

$m_0 = 4$, umbral = $0.75 \times 4 = 3$ elementos.

- Insert 10: $n=1$, $m=4$, $\alpha=0.25$.
- Insert 22: $n=2$, $m=4$, $\alpha=0.50$.
- Insert 35: $n=3$, $m=4$, $\alpha=0.75$.
- Insert 5: $(3+1)/4 = 1.0 > 0.75$ → **RESIZE** $m=4 \to 8$.
  Reinsertar 10, 22, 35 en tabla de 8. Luego insertar 5.
  $n=4$, $m=8$, $\alpha=0.50$.
- Insert 17, 8: sin resize. $n=6$, $m=8$, $\alpha=0.75$.
- Insert 44: $(6+1)/8 = 0.875 > 0.75$ → **RESIZE** $m=8 \to 16$.
  Reinsertar 6 elementos. $n=7$, $m=16$, $\alpha=0.4375$.
- Insert 13, 29, 41: sin resize. $n=10$, $m=16$, $\alpha=0.625$.

Total: **2 resizes**, 9 reinserciones (3 + 6).

</details>

<details><summary>¿Cuál es el costo amortizado?</summary>

Costo total: 10 inserciones directas + 9 reinserciones = 19
operaciones de inserción.

Amortizado: $19/10 = 1.9$ operaciones por inserción.

Esto es $O(1)$ — el costo del resize se amortiza.

</details>

---

### Ejercicio 2 — Traza de contracción

Continuando del ejercicio anterior ($n=10$, $m=16$),
$\alpha_{\min} = 0.25$. Elimina las claves 10, 22, 35, 5, 17, 8,
44 en ese orden. ¿Cuándo ocurre shrink?

<details><summary>¿En qué eliminación ocurre shrink?</summary>

Umbral de shrink: $0.25 \times 16 = 4$ elementos.

- Delete 10: $n=9$, $\alpha=0.5625$.
- Delete 22: $n=8$, $\alpha=0.50$.
- Delete 35: $n=7$, $\alpha=0.4375$.
- Delete 5: $n=6$, $\alpha=0.375$.
- Delete 17: $n=5$, $\alpha=0.3125$.
- Delete 8: $n=4$, $\alpha=0.25$. No shrink (no es $< 0.25$).
- Delete 44: $n=3$, $\alpha=0.1875 < 0.25$ → **SHRINK** $m=16 \to 8$.
  Reinsertar 3 elementos. $n=3$, $m=8$, $\alpha=0.375$.

Un shrink, 3 reinserciones.

</details>

<details><summary>¿Hay riesgo de thrashing aquí?</summary>

No: después del shrink, $\alpha = 0.375$. Para que ocurra un grow,
se necesita $(n+1)/8 > 0.75$, es decir $n > 5$. Habría que
insertar 3 elementos más. Y para que ocurra otro shrink,
$n/8 < 0.25$, es decir $n < 2$. El gap de histéresis (0.25 a 0.75)
protege contra oscilaciones.

</details>

---

### Ejercicio 3 — Demostración del método del banquero

Usando créditos de 3 unidades por inserción (1 para insertar, 2
guardados), demuestra que los créditos cubren **exactamente** el
costo del resize para $m_0 = 4$, $\alpha_{\max} = 0.75$, factor
$\times 2$.

<details><summary>¿Cuántos créditos acumulados antes del primer resize?</summary>

Se insertan 3 elementos antes del resize (umbral = $0.75 \times 4 = 3$).
Al intentar insertar el 4to, se dispara resize.

Créditos acumulados: $3 \times 2 = 6$.

Costo del resize: reinsertar 3 elementos = 3 unidades.

Créditos restantes: $6 - 3 = 3$. Sobrante para amortizar.

</details>

<details><summary>¿Y antes del segundo resize (m=8→16)?</summary>

Desde el primer resize hasta el segundo, se insertaron los
elementos 4, 5, 6 (3 inserciones nuevas, ya que $0.75 \times 8 = 6$
es el umbral). Al insertar el 7mo, se dispara resize.

Créditos nuevos desde último resize: $3 \times 2 = 6$.
Créditos sobrantes del resize anterior: 3.
Total disponible: $6 + 3 = 9$.

Costo del resize: reinsertar 6 elementos = 6 unidades.

Restantes: $9 - 6 = 3$. El sistema siempre tiene créditos
positivos — la invariante se mantiene.

</details>

---

### Ejercicio 4 — Capacidad óptima inicial

Vas a insertar exactamente 1000 elementos en un `HashMap` de Rust
($\alpha_{\max} = 0.875$). ¿Qué argumento pasarías a
`HashMap::with_capacity()` para evitar **todo** resize?

<details><summary>¿Qué valor usar?</summary>

`HashMap::with_capacity(1000)` — Rust calcula internamente la
capacidad necesaria. Internamente:

$m = \lceil 1000 / 0.875 \rceil = \lceil 1142.86 \rceil = 1143$

Siguiente potencia de 2: $m = 2048$.

Pero `with_capacity(n)` ya hace este cálculo. No necesitas pasar
1143 — pasar 1000 es suficiente.

</details>

<details><summary>¿Cuántos resizes ahorra?</summary>

Sin pre-alocación (desde $m_0 = 0$, Rust comienza sin
asignar memoria hasta el primer insert):

Resizes: $\lceil \log_2(2048/\text{initial}) \rceil$ — depende del
tamaño inicial, pero típicamente ~10-11 resizes.

Rehash total: $1 + 2 + 4 + \ldots + 1024 \approx 2047$
reinserciones.

Con `with_capacity(1000)`: 0 resizes, 0 reinserciones. Ahorro de
~2000 operaciones de inserción.

</details>

---

### Ejercicio 5 — Factor ×1.5 vs ×2

Calcula cuántos resizes y reinserciones totales ocurren al insertar
$n = 10000$ elementos con $m_0 = 8$ y $\alpha_{\max} = 0.75$ para
factores $\times 2$ y $\times 1.5$.

<details><summary>¿Cuántos resizes con cada factor?</summary>

**Factor ×2**: necesitamos $m \geq 10000/0.75 = 13334$. La
potencia de 2: $m = 16384$. Resizes:
$\log_2(16384/8) = \log_2(2048) = 11$ resizes.

**Factor ×1.5**: necesitamos $m \geq 13334$.
$8 \times 1.5^k \geq 13334$ → $k = \lceil \log_{1.5}(1667) \rceil
= \lceil 18.3 \rceil = 19$ resizes.

Factor ×1.5 requiere 19 vs 11 resizes — casi el doble.

</details>

<details><summary>¿Reinserciones totales?</summary>

**Factor ×2**: $\sum 6 + 12 + 24 + \ldots + 6144 \approx 12282$.

**Factor ×1.5**: la serie crece más lento pero tiene más términos.
$\sum 6 + 9 + 13 + 20 + \ldots \approx 19800$.

Factor ×1.5 tiene ~60% más reinserciones en total, pero usa ~25%
menos memoria en el pico.

</details>

---

### Ejercicio 6 — Rehash incremental: búsqueda en dos tablas

En un sistema con rehash incremental, la tabla tiene:
- `ht[0]`: capacidad 8, elementos {3, 11, 19} (slots 3, 4, 5).
- `ht[1]`: capacidad 16, elemento {27} (migrado, slot 11).
- `rehash_idx = 4` (buckets 0-3 ya procesados).

¿Cómo se resuelve la búsqueda de 11? ¿Y de 27?

<details><summary>Búsqueda de 11</summary>

1. Buscar en `ht[0]`: $h(11) \bmod 8 = 3$. Slot 3 → 3 ≠ 11.
   Slot 4 → 11 ✓. **Encontrado en ht[0]**.

11 aún no fue migrado (`rehash_idx = 4` significa que solo los
buckets 0, 1, 2, 3 fueron procesados; pero la clave 11 está en
slot 4 que no ha sido procesado aún — wait, `rehash_idx = 4`
significa que el **próximo** a migrar es el bucket 4, así que los
buckets 0-3 ya se procesaron). Slot 4 tiene 11 y no ha sido
migrado todavía.

</details>

<details><summary>Búsqueda de 27</summary>

1. Buscar en `ht[0]`: $h(27) \bmod 8 = 3$. Slot 3 → 3 ≠ 27.
   Slot 4 → 11 ≠ 27. Slot 5 → 19 ≠ 27. Slot 6 → EMPTY → no está
   en `ht[0]`.
2. Buscar en `ht[1]`: $h(27) \bmod 16 = 11$. Slot 11 → 27 ✓.
   **Encontrado en ht[1]**.

La búsqueda requirió consultar **ambas tablas** — costo extra
durante el periodo de migración.

</details>

---

### Ejercicio 7 — Pico de memoria

Una tabla hash almacena claves de 8 bytes y valores de 8 bytes con
1 byte de estado. Tiene $n = 500000$ elementos y $m = 2^{20}$
(~1M slots). Calcula la memoria total durante un resize $\times 2$.

<details><summary>¿Memoria por slot y totales?</summary>

Memoria por slot (SoA): $8 + 8 + 1 = 17$ bytes.

Tabla vieja: $2^{20} \times 17 = 17825792$ bytes $\approx 17$ MB.

Tabla nueva: $2^{21} \times 17 = 35651584$ bytes $\approx 34$ MB.

Pico total: $17 + 34 = 51$ MB (durante el resize).

Después: solo tabla nueva = 34 MB.

</details>

<details><summary>¿Cómo se reduce con factor ×1.5?</summary>

Tabla nueva: $\lfloor 2^{20} \times 1.5 \rfloor = 1572864$ slots.
$1572864 \times 17 = 26738688$ bytes $\approx 25.5$ MB.

Pico: $17 + 25.5 = 42.5$ MB — ahorro de ~17% respecto a ×2.

Con rehash incremental: el pico es el mismo (ambas tablas
coexisten), pero dura más tiempo.

</details>

---

### Ejercicio 8 — with_capacity en Rust

Experimenta con `HashMap::with_capacity()` para diferentes valores.
Observa `capacity()` para 10, 100, 1000, 10000. ¿Los valores
reportados son potencias de 2?

<details><summary>¿Qué valores reporta capacity()?</summary>

```rust
HashMap::<i32, i32>::with_capacity(10).capacity()    // ~14
HashMap::<i32, i32>::with_capacity(100).capacity()    // ~114
HashMap::<i32, i32>::with_capacity(1000).capacity()   // ~1142
HashMap::<i32, i32>::with_capacity(10000).capacity()  // ~11428
```

Los valores **no** son potencias de 2 como tales — Rust
(`hashbrown`) reporta el número de **elementos** que puede
almacenar antes de resize, no la capacidad interna del array.
Internamente, el array es potencia de 2, pero `capacity()` retorna
$\lfloor m \times \alpha_{\max} \rfloor$ donde $m$ es la potencia
de 2 interna y $\alpha_{\max} = 7/8 = 0.875$.

Para `with_capacity(100)`: $m_{\text{interno}} = 128$,
$capacity = \lfloor 128 \times 0.875 \rfloor = 112$.

</details>

<details><summary>¿Cuánto se puede insertar antes del primer resize?</summary>

Exactamente `capacity()` elementos. Si haces
`with_capacity(100)` y obtienes `cap=112`, puedes insertar 112
sin resize. El insert número 113 dispara un resize a
$m_{\text{interno}} = 256$.

</details>

---

### Ejercicio 9 — Rehash incremental: duración de la migración

Con rehash incremental, si la tabla tiene $m = 1024$ buckets y
cada operación migra 1 bucket, ¿cuántas operaciones se necesitan
para completar la migración? ¿Qué pasa si las operaciones son
poco frecuentes?

<details><summary>¿Cuántas operaciones?</summary>

En el peor caso, 1024 operaciones — una por bucket, incluyendo
buckets vacíos (que se saltan rápido pero aún requieren verificar).
Con $\alpha = 0.75$, ~256 buckets están vacíos y se procesan casi
instantáneamente, pero el conteo de operaciones sigue siendo 1024.

En la práctica, Redis migra **10 buckets** por operación para
acelerar la migración cuando la carga de operaciones es alta.

</details>

<details><summary>¿Qué pasa con baja frecuencia de operaciones?</summary>

Si las operaciones son infrecuentes, la migración se prolonga y
ambas tablas coexisten en memoria por mucho tiempo. Redis resuelve
esto con un **cron job** que migra 1ms de buckets cada 100ms
(~1% del tiempo), garantizando progreso incluso sin operaciones
del cliente.

Sin esta medida, una tabla inactiva podría mantener ambas tablas
indefinidamente — desperdicio de memoria.

</details>

---

### Ejercicio 10 — Diseño de política completa

Diseña una tabla hash con la siguiente política de
redimensionamiento:
- Grow $\times 2$ cuando $\alpha > 0.75$.
- Shrink $\times 0.5$ cuando $\alpha < 0.2$ (con capacidad
  mínima 16).
- Rehash de limpieza de tombstones cuando $d > 0.3 \cdot m$.
- Pre-alocación con `create(initial_capacity)`.

Implementa las 4 reglas y prueba con el patrón: insertar 1000,
eliminar 800, insertar 500, eliminar 600.

<details><summary>¿Cuántos resizes y rehashes ocurren?</summary>

Fase 1 (insertar 1000): ~7 grow resizes ($8 \to 16 \to \ldots \to
2048$). Capacidad final ~2048.

Fase 2 (eliminar 800): $n = 200$, $\alpha = 200/2048 \approx 0.10
< 0.2$ → shrink a 1024. $\alpha = 200/1024 \approx 0.20$ — justo
en el umbral. Si usamos tombstones: $d = 800$, $0.3 \times 2048 =
614$ → rehash de limpieza primero (que elimina tombstones),
luego shrink si $\alpha < 0.2$.

Fase 3 (insertar 500): $n = 700$, $\alpha \approx 0.68$ → sin
resize.

Fase 4 (eliminar 600): $n = 100$ → probablemente 1-2 shrinks.

Total estimado: ~7 grows + 1-2 rehashes + 2-3 shrinks ≈ 10-12
resizes.

</details>

<details><summary>¿Qué memoria máxima se usa?</summary>

Pico de memoria durante grow: al pasar de $m=1024$ a $m=2048$,
temporalmente $3072$ slots × tamaño de entry.

Con backward shift delete (sin tombstones): se elimina la
necesidad de rehash de limpieza y se reducen los resizes a solo
grow + shrink.

La elección entre tombstones + rehash vs backward shift depende
del patrón de uso. Para este patrón (muchas eliminaciones
masivas), backward shift es claramente superior.

</details>
