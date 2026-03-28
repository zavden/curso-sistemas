# Eliminación en open addressing

## Objetivo

Profundizar en el problema de la eliminación dentro de tablas hash
con **open addressing** — una operación que rompe invariantes si
se implementa de forma ingenua:

- Por qué poner **EMPTY** en el slot eliminado destruye cadenas de
  sondeo existentes.
- El mecanismo de **tombstones** (DELETED): solución estándar, con
  su impacto en rendimiento y acumulación progresiva.
- **Backward shift delete**: eliminación sin tombstones que mantiene
  la tabla limpia desplazando elementos hacia atrás.
- **Robin Hood hashing**: variante que facilita la eliminación y
  reduce la varianza del número de sondeos.
- **Rehash de limpieza**: cuándo y cómo eliminar tombstones
  acumulados reconstruyendo la tabla.
- Referencia: T02 introdujo tombstones en sonda lineal; T03 los
  usó en cuadrática y doble hashing. Aquí los analizamos en
  profundidad y estudiamos alternativas.

---

## El problema: por qué EMPTY destruye cadenas

En open addressing, la inserción coloca un elemento en el
**primer slot libre** según la secuencia de sondeo. La búsqueda
sigue esa misma secuencia hasta encontrar la clave o un slot
**EMPTY** (que indica que la clave no existe). Si eliminamos un
elemento colocando EMPTY, rompemos la cadena de sondeo de todos
los elementos que fueron insertados **después** y cuya secuencia
pasaba por ese slot:

```
Tabla m=8, h(k) = k % 8:

Insertar 3, 11, 19 (todos h=3):
  slot: 0  1  2  3   4   5  6  7
        _  _  _  3  11  19  _  _

Eliminar 11 poniendo EMPTY:
  slot: 0  1  2  3  _  19  6  7

Buscar 19: h(19)=3, slot 3 → 3≠19, slot 4 → EMPTY → "no existe"
  ¡FALSO! 19 sigue en slot 5, pero la búsqueda se detuvo en 4.
```

El contrato de open addressing es:

> Si una clave $k$ está en la tabla, todos los slots de su
> secuencia de sondeo entre $h(k)$ y la posición real de $k$
> están **ocupados o marcados como DELETED** — nunca EMPTY.

Romper este contrato produce **falsos negativos**: la búsqueda
reporta que una clave no existe cuando sí está en la tabla. Esto
es peor que un falso positivo — el usuario pierde datos
silenciosamente.

---

## Estrategia 1: tombstones (marcadores DELETED)

### Mecanismo

Se añade un **tercer estado** a cada slot:

| Estado | Significado | Búsqueda | Inserción |
|--------|-------------|----------|-----------|
| EMPTY | Nunca ocupado | Detener: clave no existe | Insertar aquí |
| OCCUPIED | Contiene una clave activa | Comparar clave, seguir si ≠ | Seguir sondeando |
| DELETED | Tenía un elemento, fue eliminado | **Continuar** sondeando | Insertar aquí (reutilizar) |

La eliminación simplemente cambia el estado del slot de OCCUPIED
a DELETED. Esto preserva la cadena de sondeo:

```
Eliminar 11 con tombstone:
  slot: 0  1  2  3  D  19  6  7     (D = DELETED)

Buscar 19: h(19)=3, slot 3 → 3≠19, slot 4 → DELETED → continuar,
           slot 5 → 19 ✓
```

### Implementación en C

```c
enum SlotState { EMPTY = 0, OCCUPIED = 1, DELETED = 2 };

typedef struct {
    int *keys;
    int *values;
    enum SlotState *state;
    int capacity;
    int count;       // solo OCCUPIED
    int tombstones;  // solo DELETED
} HashTable;

bool ht_delete(HashTable *t, int key) {
    int idx = (key % t->capacity + t->capacity) % t->capacity;

    for (int i = 0; i < t->capacity; i++) {
        if (t->state[idx] == EMPTY) {
            return false;  // key not found
        }
        if (t->state[idx] == OCCUPIED && t->keys[idx] == key) {
            t->state[idx] = DELETED;
            t->count--;
            t->tombstones++;
            return true;
        }
        idx = (idx + 1) % t->capacity;  // linear probing
    }
    return false;  // table full, key not found
}
```

### Optimización: reutilizar tombstones en inserción

Durante una inserción, si encontramos un tombstone antes de un
EMPTY, podemos **insertar ahí** siempre que sigamos sondeando
hasta EMPTY para verificar que la clave no existe ya:

```c
bool ht_insert(HashTable *t, int key, int value) {
    int idx = (key % t->capacity + t->capacity) % t->capacity;
    int first_deleted = -1;

    for (int i = 0; i < t->capacity; i++) {
        if (t->state[idx] == EMPTY) {
            // key doesn't exist; insert at first_deleted or here
            int target = (first_deleted >= 0) ? first_deleted : idx;
            t->keys[target] = key;
            t->values[target] = value;
            if (target == first_deleted) {
                t->tombstones--;
            }
            t->state[target] = OCCUPIED;
            t->count++;
            return true;
        }
        if (t->state[idx] == OCCUPIED && t->keys[idx] == key) {
            t->values[idx] = value;  // update
            return false;            // not a new insertion
        }
        if (t->state[idx] == DELETED && first_deleted < 0) {
            first_deleted = idx;
        }
        idx = (idx + 1) % t->capacity;
    }
    return false;  // table full
}
```

### Problema: acumulación de tombstones

Los tombstones nunca desaparecen solos. Después de muchos ciclos
de inserción/eliminación, la tabla puede tener:

```
Estado de tabla tras 1000 inserciones y 800 eliminaciones:
  200 OCCUPIED  +  800 DELETED  +  0 EMPTY

  Búsqueda: debe atravesar tombstones → cadenas largas
  Factor de carga real: α = 200/m (pocos elementos)
  Factor de carga efectivo: α_eff = (200+800)/m = 1.0 (tabla "llena")
```

El factor de carga **efectivo** (que determina el rendimiento
real) incluye tanto OCCUPIED como DELETED:

$$\alpha_{\text{eff}} = \frac{n + d}{m}$$

donde $n$ es el número de elementos y $d$ el número de tombstones.
Incluso con pocos elementos activos, los tombstones mantienen
cadenas de sondeo largas. El rendimiento se degrada como si la
tabla tuviera $n + d$ elementos.

### Cuándo limpiar tombstones

Hay dos estrategias principales:

1. **Umbral de tombstones**: limpiar cuando $d > \beta \cdot m$
   para algún umbral $\beta$ (típicamente $\beta \approx 0.25$).
2. **Proporción tombstones/ocupados**: limpiar cuando
   $d > \gamma \cdot n$ (por ejemplo, $\gamma = 2$ — más del
   doble de tombstones que elementos activos).

La limpieza consiste en un **rehash completo**: crear una nueva
tabla, reinsertar solo los OCCUPIED, descartar DELETED y EMPTY.
Costo $O(m)$ pero amortizado si se elige bien el umbral.

---

## Estrategia 2: backward shift delete

### La idea

En lugar de marcar DELETED, al eliminar una clave **desplazamos
hacia atrás** los elementos que están en posiciones desplazadas
(insertados más adelante en la cadena). Esto elimina la necesidad
de tombstones y mantiene la tabla limpia:

```
Antes de eliminar 11:
  slot: 0  1  2  3   4   5  6  7
        _  _  _  3  11  19  _  _

Eliminar 11: slot 4 queda vacío.
  ¿Hay alguien después que debería estar antes?
  Slot 5: 19, h(19)=3. Posición natural es 3, está en 5.
          ¿Está entre slot_vacío(4) y slot_actual(5)? Sí → mover.

  slot: 0  1  2  3  19   _  6  7
  (19 se movió de 5 a 4, acercándose a su posición natural)

  Slot 6: EMPTY → terminar.
```

### El invariante

Después de eliminar un slot $s$ y desplazarlo, para cada slot
posterior $j$ en la cadena debemos verificar si $j$ "pertenece"
antes de $s$. La condición de desplazamiento es:

> Un slot $j$ se desplaza al hueco $s$ si la posición natural
> de $j$ (su hash) estaría **entre** $s$ y $j$ en sentido
> circular, es decir, si el slot fue desplazado más allá de $s$
> durante su inserción.

Para sonda lineal, la condición (con aritmética circular) es:

```c
// Should slot j be shifted back to gap s?
// j's natural position is h = hash(keys[j]) % capacity
// In circular terms: j was displaced past s if
// gap < j: h <= gap || h > j      (no wrap-around)
// gap > j: h <= gap && h > j      (wrap-around)
bool should_shift(int gap, int j, int h, int capacity) {
    if (gap < j) {
        return (h <= gap) || (h > j);
    } else {
        // wrap-around case: gap > j
        return (h <= gap) && (h > j);
    }
}
```

### Implementación completa

```c
bool ht_delete_shift(HashTable *t, int key) {
    // 1. find the key
    int idx = (key % t->capacity + t->capacity) % t->capacity;

    for (int i = 0; i < t->capacity; i++) {
        if (t->state[idx] == EMPTY) {
            return false;  // not found
        }
        if (t->keys[idx] == key) {
            break;  // found at idx
        }
        idx = (idx + 1) % t->capacity;
    }

    if (t->keys[idx] != key) return false;

    // 2. backward shift
    int gap = idx;
    int j = (gap + 1) % t->capacity;

    while (t->state[j] == OCCUPIED) {
        int h = (t->keys[j] % t->capacity + t->capacity) % t->capacity;

        // check if j should be shifted to gap
        bool shift;
        if (gap < j) {
            shift = (h <= gap) || (h > j);
        } else {
            // wrap-around
            shift = (h <= gap) && (h > j);
        }

        if (shift) {
            t->keys[gap] = t->keys[j];
            t->values[gap] = t->values[j];
            gap = j;
        }

        j = (j + 1) % t->capacity;
    }

    // 3. clear the final gap
    t->state[gap] = EMPTY;
    t->count--;
    return true;
}
```

### Traza completa

```
Tabla m=8, h(k) = k % 8:
Insertados: 3(slot 3), 11(slot 4), 19(slot 5), 27(slot 6), 4(slot 7)
                                                 h=3       h=4

  slot: 0  1  2  3   4   5   6   7
        _  _  _  3  11  19  27   4

Eliminar 11 (slot 4):
  gap = 4
  j = 5: keys[5]=19, h(19)=3.
    gap(4) < j(5): h(3) <= gap(4)? Sí → shift.
    Mover 19 de slot 5 a slot 4.
    gap = 5.

  j = 6: keys[6]=27, h(27)=3.
    gap(5) < j(6): h(3) <= gap(5)? Sí → shift.
    Mover 27 de slot 6 a slot 5.
    gap = 6.

  j = 7: keys[7]=4, h(4)=4.
    gap(6) < j(7): h(4) <= gap(6)? Sí → shift.
    Mover 4 de slot 7 a slot 6.
    gap = 7.

  j = 0: state[0] == EMPTY → parar.
  Marcar slot 7 como EMPTY.

  Resultado:
  slot: 0  1  2  3  19  27   4   _
        _  _  _  3  19  27   4   _

  Verificar: buscar 19, h=3 → slot 3(3≠19), slot 4(19✓) → 2 sondeos ✓
             buscar 27, h=3 → slot 3, 4, 5(27✓) → 3 sondeos ✓
             buscar 4,  h=4 → slot 4(19≠4), 5(27≠4), 6(4✓) → 3 sondeos ✓
```

### Ventajas y desventajas

| Aspecto | Tombstone | Backward shift |
|---------|-----------|----------------|
| Complejidad de delete | $O(1)$ (solo marcar) | $O(L)$ donde $L$ = longitud del cluster |
| Complejidad de search | Degrada con tombstones | Siempre óptima |
| Complejidad de insert | Puede reutilizar tombstone | Sin cambio |
| Memoria extra | Campo state por slot | Campo state (solo EMPTY/OCCUPIED) |
| Limpieza periódica | Necesaria (rehash) | No necesaria |
| Implementación | Simple | Condición circular delicada |
| Sondeo compatible | Todas las variantes | **Solo sonda lineal** |

La restricción más importante: backward shift **solo funciona con
sonda lineal**. Con sonda cuadrática o doble hashing, los
elementos desplazados no están en posiciones consecutivas, y el
desplazamiento no es posible de forma directa.

---

## Estrategia 3: Robin Hood hashing

### La idea

**Robin Hood hashing** no es solo una estrategia de eliminación
sino una variante de inserción que **reduce la varianza** del
número de sondeos. La regla es: al insertar, si encontramos un
elemento cuya **distancia de sondeo** (displacement o probe
distance) es **menor** que la del elemento que estamos insertando,
los intercambiamos — le "robamos" al rico para darle al pobre:

```
Insertar clave K con displacement 3.
Encontramos slot ocupado por clave X con displacement 1.
  3 > 1 → swap: K toma el slot, X continúa buscando.
  X ahora tiene displacement 2 (sigue desde el siguiente slot).
```

La **distancia de sondeo** (displacement) de un elemento en un
slot $j$ con hash $h$ es:

$$d(j) = (j - h + m) \bmod m$$

Es decir, cuántos slots se desplazó desde su posición natural.

### Inserción Robin Hood

```c
void ht_insert_robin_hood(HashTable *t, int key, int value) {
    int idx = (key % t->capacity + t->capacity) % t->capacity;
    int displacement = 0;

    int cur_key = key;
    int cur_val = value;

    for (int i = 0; i < t->capacity; i++) {
        if (t->state[idx] == EMPTY || t->state[idx] == DELETED) {
            t->keys[idx] = cur_key;
            t->values[idx] = cur_val;
            t->state[idx] = OCCUPIED;
            t->count++;
            return;
        }

        // displacement of the existing element
        int existing_disp = (idx - ((t->keys[idx] % t->capacity
                            + t->capacity) % t->capacity)
                            + t->capacity) % t->capacity;

        if (existing_disp < displacement) {
            // steal from the rich: swap
            int tmp_k = t->keys[idx]; t->keys[idx] = cur_key; cur_key = tmp_k;
            int tmp_v = t->values[idx]; t->values[idx] = cur_val; cur_val = tmp_v;
            displacement = existing_disp;
        }

        displacement++;
        idx = (idx + 1) % t->capacity;
    }
}
```

### Eliminación Robin Hood: backward shift natural

Con Robin Hood hashing, el backward shift es más natural porque
conocemos el displacement de cada elemento. La eliminación es
esencialmente la misma que backward shift delete, pero la
condición de desplazamiento se simplifica:

```c
bool ht_delete_robin_hood(HashTable *t, int key) {
    int idx = (key % t->capacity + t->capacity) % t->capacity;

    // find the key
    for (int i = 0; i < t->capacity; i++) {
        if (t->state[idx] == EMPTY) return false;

        int disp = (idx - ((t->keys[idx] % t->capacity
                   + t->capacity) % t->capacity)
                   + t->capacity) % t->capacity;

        // Robin Hood guarantee: if our displacement > disp
        // of current slot, the key doesn't exist
        if (disp < i) return false;

        if (t->keys[idx] == key) break;
        idx = (idx + 1) % t->capacity;
    }

    if (t->keys[idx] != key) return false;

    // backward shift: move elements with displacement > 0
    int gap = idx;
    int next = (gap + 1) % t->capacity;

    while (t->state[next] == OCCUPIED) {
        int disp = (next - ((t->keys[next] % t->capacity
                   + t->capacity) % t->capacity)
                   + t->capacity) % t->capacity;

        if (disp == 0) break;  // element at natural position

        // shift back
        t->keys[gap] = t->keys[next];
        t->values[gap] = t->values[next];
        gap = next;
        next = (next + 1) % t->capacity;
    }

    t->state[gap] = EMPTY;
    t->count--;
    return true;
}
```

La mejora clave respecto a backward shift estándar: la condición
de parada es simplemente `displacement == 0`. No necesitamos la
comparación circular complicada.

Además, Robin Hood ofrece una optimización en la **búsqueda**: si
durante la búsqueda encontramos un slot cuyo displacement es menor
que el número de sondeos que llevamos, la clave **no puede estar**
más adelante (cualquier clave más adelante habría sido
intercambiada durante la inserción). Esto acota la búsqueda
fallida.

### Traza Robin Hood

```
Tabla m=8, h(k) = k % 8:
Insertar 3: h=3, slot 3 libre → slot 3. disp=0.
Insertar 11: h=3, slot 3 ocupado (3, disp=0).
  Mi disp=1 > disp_existente=0? No (1 > 0 sí → swap)
  Swap: 11 va a slot 3, 3 busca desde slot 4.
  Espera — recalculemos:
    slot 3: 3(disp=0), mi disp=0 → 0 < 0? No → seguir.
    slot 4: libre → insertar 11 en slot 4. disp=1.

Insertar 19: h=3.
  slot 3: 3(disp=0), mi disp=0 → iguales, seguir.
  slot 4: 11(disp=1), mi disp=1 → iguales, seguir.
  slot 5: libre → insertar 19 en slot 5. disp=2.

  slot: _  _  _  3  11  19  _  _
  disp: _  _  _  0   1   2  _  _

Insertar 4: h=4.
  slot 4: 11(disp=1), mi disp=0 → 0 < 1? No → seguir.
  slot 5: 19(disp=2), mi disp=1 → 1 < 2? Sí → swap!
  4 va a slot 5, 19 continúa con disp=1.
  slot 6: libre → insertar 19 en slot 6. disp=3.

  slot: _  _  _  3  11   4  19  _
  disp: _  _  _  0   1   1   3  _

Eliminar 11 (slot 4):
  gap = 4.
  next = 5: 4(disp=1), disp > 0 → shift.
    Mover 4 de slot 5 a slot 4. gap = 5.
  next = 6: 19(disp=3), disp > 0 → shift.
    Mover 19 de slot 6 a slot 5. gap = 6.
  next = 7: EMPTY → parar.
  Marcar slot 6 EMPTY.

  slot: _  _  _  3   4  19   _  _
  disp: _  _  _  0   0   2   _  _

Verificar: buscar 4, h=4, slot 4 → 4 ✓ (1 sondeo)
           buscar 19, h=3, slot 3(3), slot 4(4), slot 5(19) ✓ (3 sondeos)
```

---

## Comparación de estrategias de eliminación

| Criterio | Tombstone | Backward shift | Robin Hood + shift |
|----------|-----------|----------------|-------------------|
| Delete $O(?)$ | $O(1)$ | $O(L)$ promedio | $O(L)$ promedio |
| Search hit | Degrada con $d$ | Siempre óptima | Óptima + early exit |
| Search miss | Degrada mucho | Siempre óptima | Acotada por max disp |
| Sondeo compatible | Todos | Solo lineal | Solo lineal |
| Complejidad impl. | Baja | Media | Media-alta |
| Limpieza periódica | Sí (rehash) | No | No |
| Uso en la práctica | Muy común | Python `dict` | Rust `hashbrown` (variante) |

### Análisis de rendimiento con tombstones

Supongamos una tabla de $m = 1000$ slots con sonda lineal. Tras
$n = 200$ elementos activos y $d = 500$ tombstones:

- Factor de carga real: $\alpha = 200/1000 = 0.2$.
- Factor de carga efectivo: $\alpha_{\text{eff}} = 700/1000 = 0.7$.
- Sondeos esperados (miss), fórmula de Knuth:
  $\frac{1}{2}\left(1 + \frac{1}{(1-\alpha_{\text{eff}})^2}\right) = \frac{1}{2}(1 + \frac{1}{0.09}) \approx 6.06$.
- Sin tombstones (tras rehash):
  $\frac{1}{2}\left(1 + \frac{1}{(1-0.2)^2}\right) = \frac{1}{2}(1 + 1.5625) \approx 1.28$.

Los tombstones inflaron los sondeos de **1.28** a **6.06** — casi
$5\times$ más. Esto justifica el rehash periódico.

### Implementaciones reales

| Lenguaje / librería | Estrategia de eliminación |
|---------------------|--------------------------|
| Java `HashMap` | Chaining (no aplica open addressing) |
| Python `dict` | Open addressing + backward shift (desde CPython 3.6) |
| Rust `HashMap` (hashbrown) | Swiss Table: SIMD + metadatos, sin tombstones en búsqueda |
| Go `map` | Chaining con buckets de 8 + overflow |
| C++ `std::unordered_map` | Chaining (nodos enlazados) |
| Redis | Chaining + rehash incremental |
| khash (C) | Tombstones con bits de estado |

Python es notable por usar backward shift (con una variante de
sondeo personalizada) en lugar de tombstones, lo que mantiene el
rendimiento estable bajo carga de trabajo con muchas eliminaciones.

Rust `hashbrown` (Swiss Table) usa un byte de control por slot con
valores especiales: `0x80` = EMPTY, `0xFE` = DELETED, `0x00-0x7F` =
hash parcial (7 bits). La búsqueda SIMD examina 16 slots en
paralelo comparando estos bytes de control, lo que hace que los
tombstones sean mucho menos costosos que en sonda lineal
tradicional.

---

## Rehash de limpieza

Cuando los tombstones se acumulan más allá de un umbral, la
solución es **reconstruir la tabla**:

```c
void ht_rehash_cleanup(HashTable *t) {
    int old_cap = t->capacity;
    int *old_keys = t->keys;
    int *old_vals = t->values;
    enum SlotState *old_state = t->state;

    // allocate new arrays (same capacity)
    t->keys = calloc(old_cap, sizeof(int));
    t->values = calloc(old_cap, sizeof(int));
    t->state = calloc(old_cap, sizeof(enum SlotState));
    t->count = 0;
    t->tombstones = 0;

    // reinsert only OCCUPIED entries
    for (int i = 0; i < old_cap; i++) {
        if (old_state[i] == OCCUPIED) {
            ht_insert(t, old_keys[i], old_vals[i]);
        }
    }

    free(old_keys);
    free(old_vals);
    free(old_state);
}
```

Nótese que el rehash de limpieza **no cambia la capacidad** — solo
elimina tombstones. El redimensionamiento (crecer/shrink) se
tratará en T05.

### Costo amortizado

Si hacemos rehash cada $k$ eliminaciones, el costo amortizado de
cada eliminación es:

$$O(1) + \frac{O(m)}{k} = O\left(1 + \frac{m}{k}\right)$$

Si elegimos $k = \Theta(m)$ (por ejemplo, $k = m/4$), el costo
amortizado es $O(1)$. En la práctica, se suele verificar
el ratio $d/m$ o $d/n$ después de cada eliminación y hacer rehash
solo cuando se supera el umbral.

---

## Programa completo en C

```c
// deletion_strategies.c
// Deletion strategies in open addressing: tombstone vs backward shift
// vs Robin Hood hashing
// Compile: gcc -O2 -o deletion_strategies deletion_strategies.c

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

// ============================================================
// Tombstone-based hash table (linear probing)
// ============================================================

enum SlotState { EMPTY = 0, OCCUPIED = 1, DELETED = 2 };

typedef struct {
    int *keys;
    int *values;
    enum SlotState *state;
    int capacity;
    int count;
    int tombstones;
} TombstoneTable;

TombstoneTable *tt_create(int capacity) {
    TombstoneTable *t = malloc(sizeof(TombstoneTable));
    t->keys = calloc(capacity, sizeof(int));
    t->values = calloc(capacity, sizeof(int));
    t->state = calloc(capacity, sizeof(enum SlotState));
    t->capacity = capacity;
    t->count = 0;
    t->tombstones = 0;
    return t;
}

void tt_free(TombstoneTable *t) {
    free(t->keys);
    free(t->values);
    free(t->state);
    free(t);
}

bool tt_insert(TombstoneTable *t, int key, int value) {
    int idx = ((key % t->capacity) + t->capacity) % t->capacity;
    int first_deleted = -1;

    for (int i = 0; i < t->capacity; i++) {
        if (t->state[idx] == EMPTY) {
            int target = (first_deleted >= 0) ? first_deleted : idx;
            t->keys[target] = key;
            t->values[target] = value;
            if (first_deleted >= 0) {
                t->tombstones--;
            }
            t->state[target] = OCCUPIED;
            t->count++;
            return true;
        }
        if (t->state[idx] == OCCUPIED && t->keys[idx] == key) {
            t->values[idx] = value;
            return false;
        }
        if (t->state[idx] == DELETED && first_deleted < 0) {
            first_deleted = idx;
        }
        idx = (idx + 1) % t->capacity;
    }

    // table full but first_deleted may exist
    if (first_deleted >= 0) {
        t->keys[first_deleted] = key;
        t->values[first_deleted] = value;
        t->state[first_deleted] = OCCUPIED;
        t->tombstones--;
        t->count++;
        return true;
    }
    return false;
}

int tt_search(TombstoneTable *t, int key, int *probes) {
    int idx = ((key % t->capacity) + t->capacity) % t->capacity;
    int p = 0;

    for (int i = 0; i < t->capacity; i++) {
        p++;
        if (t->state[idx] == EMPTY) {
            if (probes) *probes = p;
            return -1;
        }
        if (t->state[idx] == OCCUPIED && t->keys[idx] == key) {
            if (probes) *probes = p;
            return t->values[idx];
        }
        idx = (idx + 1) % t->capacity;
    }
    if (probes) *probes = p;
    return -1;
}

bool tt_delete(TombstoneTable *t, int key) {
    int idx = ((key % t->capacity) + t->capacity) % t->capacity;

    for (int i = 0; i < t->capacity; i++) {
        if (t->state[idx] == EMPTY) return false;
        if (t->state[idx] == OCCUPIED && t->keys[idx] == key) {
            t->state[idx] = DELETED;
            t->count--;
            t->tombstones++;
            return true;
        }
        idx = (idx + 1) % t->capacity;
    }
    return false;
}

void tt_rehash_cleanup(TombstoneTable *t) {
    int old_cap = t->capacity;
    int *old_keys = t->keys;
    int *old_vals = t->values;
    enum SlotState *old_state = t->state;

    t->keys = calloc(old_cap, sizeof(int));
    t->values = calloc(old_cap, sizeof(int));
    t->state = calloc(old_cap, sizeof(enum SlotState));
    t->count = 0;
    t->tombstones = 0;

    for (int i = 0; i < old_cap; i++) {
        if (old_state[i] == OCCUPIED) {
            tt_insert(t, old_keys[i], old_vals[i]);
        }
    }

    free(old_keys);
    free(old_vals);
    free(old_state);
}

void tt_stats(TombstoneTable *t) {
    int empty = 0, occ = 0, del = 0;
    for (int i = 0; i < t->capacity; i++) {
        if (t->state[i] == EMPTY) empty++;
        else if (t->state[i] == OCCUPIED) occ++;
        else del++;
    }
    printf("  capacity=%d, occupied=%d, tombstones=%d, empty=%d\n",
           t->capacity, occ, del, empty);
    printf("  alpha_real=%.3f, alpha_eff=%.3f\n",
           (double)occ / t->capacity,
           (double)(occ + del) / t->capacity);
}

// ============================================================
// Backward-shift hash table (linear probing, no tombstones)
// ============================================================

typedef struct {
    int *keys;
    int *values;
    bool *occupied;
    int capacity;
    int count;
} ShiftTable;

ShiftTable *st_create(int capacity) {
    ShiftTable *t = malloc(sizeof(ShiftTable));
    t->keys = calloc(capacity, sizeof(int));
    t->values = calloc(capacity, sizeof(int));
    t->occupied = calloc(capacity, sizeof(bool));
    t->capacity = capacity;
    t->count = 0;
    return t;
}

void st_free(ShiftTable *t) {
    free(t->keys);
    free(t->values);
    free(t->occupied);
    free(t);
}

bool st_insert(ShiftTable *t, int key, int value) {
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

int st_search(ShiftTable *t, int key, int *probes) {
    int idx = ((key % t->capacity) + t->capacity) % t->capacity;
    int p = 0;

    for (int i = 0; i < t->capacity; i++) {
        p++;
        if (!t->occupied[idx]) {
            if (probes) *probes = p;
            return -1;
        }
        if (t->keys[idx] == key) {
            if (probes) *probes = p;
            return t->values[idx];
        }
        idx = (idx + 1) % t->capacity;
    }
    if (probes) *probes = p;
    return -1;
}

bool st_delete(ShiftTable *t, int key) {
    int idx = ((key % t->capacity) + t->capacity) % t->capacity;

    // find the key
    bool found = false;
    for (int i = 0; i < t->capacity; i++) {
        if (!t->occupied[idx]) return false;
        if (t->keys[idx] == key) { found = true; break; }
        idx = (idx + 1) % t->capacity;
    }
    if (!found) return false;

    // backward shift
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
    return true;
}

// ============================================================
// Robin Hood hash table (linear probing)
// ============================================================

typedef struct {
    int *keys;
    int *values;
    bool *occupied;
    int capacity;
    int count;
} RobinTable;

RobinTable *rh_create(int capacity) {
    RobinTable *t = malloc(sizeof(RobinTable));
    t->keys = calloc(capacity, sizeof(int));
    t->values = calloc(capacity, sizeof(int));
    t->occupied = calloc(capacity, sizeof(bool));
    t->capacity = capacity;
    t->count = 0;
    return t;
}

void rh_free(RobinTable *t) {
    free(t->keys);
    free(t->values);
    free(t->occupied);
    free(t);
}

static int rh_displacement(RobinTable *t, int slot) {
    int h = ((t->keys[slot] % t->capacity) + t->capacity) % t->capacity;
    return (slot - h + t->capacity) % t->capacity;
}

bool rh_insert(RobinTable *t, int key, int value) {
    int idx = ((key % t->capacity) + t->capacity) % t->capacity;
    int cur_key = key;
    int cur_val = value;
    int displacement = 0;
    bool is_new = true;

    for (int i = 0; i < t->capacity; i++) {
        if (!t->occupied[idx]) {
            t->keys[idx] = cur_key;
            t->values[idx] = cur_val;
            t->occupied[idx] = true;
            if (is_new) t->count++;
            return true;
        }

        if (t->keys[idx] == cur_key && is_new) {
            t->values[idx] = cur_val;
            return false;
        }

        int existing_disp = rh_displacement(t, idx);
        if (existing_disp < displacement) {
            // swap: steal from the rich
            int tmp_k = t->keys[idx]; t->keys[idx] = cur_key; cur_key = tmp_k;
            int tmp_v = t->values[idx]; t->values[idx] = cur_val; cur_val = tmp_v;
            displacement = existing_disp;
            is_new = true;  // displaced element needs a new slot
        }

        displacement++;
        idx = (idx + 1) % t->capacity;
    }
    return false;
}

int rh_search(RobinTable *t, int key, int *probes) {
    int idx = ((key % t->capacity) + t->capacity) % t->capacity;
    int p = 0;

    for (int i = 0; i < t->capacity; i++) {
        p++;
        if (!t->occupied[idx]) {
            if (probes) *probes = p;
            return -1;
        }

        if (t->keys[idx] == key) {
            if (probes) *probes = p;
            return t->values[idx];
        }

        // Robin Hood early exit
        int existing_disp = rh_displacement(t, idx);
        if (existing_disp < i) {
            // our key would have been placed here or earlier
            if (probes) *probes = p;
            return -1;
        }

        idx = (idx + 1) % t->capacity;
    }
    if (probes) *probes = p;
    return -1;
}

bool rh_delete(RobinTable *t, int key) {
    int idx = ((key % t->capacity) + t->capacity) % t->capacity;

    // find with Robin Hood early exit
    bool found = false;
    for (int i = 0; i < t->capacity; i++) {
        if (!t->occupied[idx]) return false;
        if (t->keys[idx] == key) { found = true; break; }
        int existing_disp = rh_displacement(t, idx);
        if (existing_disp < i) return false;
        idx = (idx + 1) % t->capacity;
    }
    if (!found) return false;

    // backward shift until displacement == 0 or empty
    int gap = idx;
    int next = (gap + 1) % t->capacity;

    while (t->occupied[next]) {
        int disp = rh_displacement(t, next);
        if (disp == 0) break;

        t->keys[gap] = t->keys[next];
        t->values[gap] = t->values[next];
        gap = next;
        next = (next + 1) % t->capacity;
    }

    t->occupied[gap] = false;
    t->count--;
    return true;
}

// ============================================================
// Demo functions
// ============================================================

void demo1_why_empty_breaks(void) {
    printf("=== Demo 1: why EMPTY breaks probe chains ===\n");
    // simulate the problem manually
    int table[8] = {0};
    int state[8] = {0};  // 0=EMPTY, 1=OCCUPIED

    // insert 3, 11, 19 (all h % 8 = 3)
    table[3] = 3;  state[3] = 1;
    table[4] = 11; state[4] = 1;
    table[5] = 19; state[5] = 1;

    printf("  After inserting 3, 11, 19 (all hash to slot 3):\n");
    printf("  ");
    for (int i = 0; i < 8; i++) {
        if (state[i]) printf("[%d]", table[i]);
        else printf("[_]");
    }
    printf("\n");

    // "delete" 11 by setting EMPTY
    state[4] = 0;
    printf("  After deleting 11 with EMPTY (WRONG):\n  ");
    for (int i = 0; i < 8; i++) {
        if (state[i]) printf("[%d]", table[i]);
        else printf("[_]");
    }
    printf("\n");

    // search for 19
    int idx = 19 % 8;  // 3
    printf("  Search for 19: h(19)=%d\n", idx);
    while (state[idx]) {
        printf("    slot %d: %d (not 19, continue)\n", idx, table[idx]);
        idx = (idx + 1) % 8;
    }
    printf("    slot %d: EMPTY -> \"not found\" (FALSE NEGATIVE!)\n", idx);
    printf("    But 19 IS in slot 5!\n\n");
}

void demo2_tombstone_basic(void) {
    printf("=== Demo 2: tombstone preserves probe chain ===\n");
    TombstoneTable *t = tt_create(16);

    tt_insert(t, 3, 30);
    tt_insert(t, 19, 190);   // h=3, goes to slot 4
    tt_insert(t, 35, 350);   // h=3, goes to slot 5

    printf("  Inserted 3, 19, 35 (all h %% 16 = 3):\n");
    for (int i = 0; i < 8; i++) {
        if (t->state[i] == OCCUPIED)
            printf("    slot %d: %d (OCCUPIED)\n", i, t->keys[i]);
        else if (t->state[i] == DELETED)
            printf("    slot %d: DELETED\n", i);
        else if (i <= 5)
            printf("    slot %d: EMPTY\n", i);
    }

    printf("  Delete 19:\n");
    tt_delete(t, 19);

    for (int i = 0; i < 8; i++) {
        if (t->state[i] == OCCUPIED)
            printf("    slot %d: %d (OCCUPIED)\n", i, t->keys[i]);
        else if (t->state[i] == DELETED)
            printf("    slot %d: DELETED\n", i);
        else if (i <= 5)
            printf("    slot %d: EMPTY\n", i);
    }

    int probes;
    int val = tt_search(t, 35, &probes);
    printf("  Search 35: value=%d, probes=%d (chain preserved!)\n\n",
           val, probes);

    tt_free(t);
}

void demo3_tombstone_accumulation(void) {
    printf("=== Demo 3: tombstone accumulation degrades performance ===\n");
    TombstoneTable *t = tt_create(1000);

    // insert 700 elements
    for (int i = 0; i < 700; i++) {
        tt_insert(t, i * 17 + 1, i);
    }

    // delete 500 of them
    for (int i = 0; i < 500; i++) {
        tt_delete(t, i * 17 + 1);
    }

    tt_stats(t);

    // measure average search probes for existing keys
    long total_probes = 0;
    int searches = 0;
    for (int i = 500; i < 700; i++) {
        int probes;
        tt_search(t, i * 17 + 1, &probes);
        total_probes += probes;
        searches++;
    }
    printf("  Avg probes (hit, with tombstones): %.2f\n",
           (double)total_probes / searches);

    // rehash cleanup
    tt_rehash_cleanup(t);
    printf("  After rehash cleanup:\n");
    tt_stats(t);

    total_probes = 0;
    for (int i = 500; i < 700; i++) {
        int probes;
        tt_search(t, i * 17 + 1, &probes);
        total_probes += probes;
    }
    printf("  Avg probes (hit, after cleanup): %.2f\n\n",
           (double)total_probes / searches);

    tt_free(t);
}

void demo4_backward_shift(void) {
    printf("=== Demo 4: backward shift delete (no tombstones) ===\n");
    ShiftTable *t = st_create(16);

    // insert elements that form a cluster
    st_insert(t, 3, 30);
    st_insert(t, 19, 190);  // h=3, slot 4
    st_insert(t, 35, 350);  // h=3, slot 5
    st_insert(t, 51, 510);  // h=3, slot 6
    st_insert(t, 4, 40);    // h=4, slot 7

    printf("  Before delete:\n");
    for (int i = 0; i < 10; i++) {
        if (t->occupied[i])
            printf("    slot %d: %d (h=%d)\n", i, t->keys[i],
                   ((t->keys[i] % 16) + 16) % 16);
        else
            printf("    slot %d: EMPTY\n", i);
    }

    printf("  Delete 19 (slot 4) with backward shift:\n");
    st_delete(t, 19);

    for (int i = 0; i < 10; i++) {
        if (t->occupied[i])
            printf("    slot %d: %d (h=%d)\n", i, t->keys[i],
                   ((t->keys[i] % 16) + 16) % 16);
        else
            printf("    slot %d: EMPTY\n", i);
    }

    // verify all elements still findable
    int probes;
    for (int key = 3; key <= 51; key += 16) {
        if (key == 19) continue;
        int val = st_search(t, key, &probes);
        printf("    search %d: value=%d, probes=%d\n", key, val, probes);
    }
    int val = st_search(t, 4, &probes);
    printf("    search 4: value=%d, probes=%d\n\n", val, probes);

    st_free(t);
}

void demo5_robin_hood(void) {
    printf("=== Demo 5: Robin Hood hashing ===\n");
    RobinTable *t = rh_create(16);

    rh_insert(t, 3, 30);
    rh_insert(t, 19, 190);
    rh_insert(t, 35, 350);
    rh_insert(t, 4, 40);

    printf("  After inserts (3, 19, 35, 4):\n");
    for (int i = 0; i < 10; i++) {
        if (t->occupied[i]) {
            int disp = rh_displacement(t, i);
            printf("    slot %d: %d (h=%d, disp=%d)\n",
                   i, t->keys[i],
                   ((t->keys[i] % 16) + 16) % 16, disp);
        } else {
            printf("    slot %d: EMPTY\n", i);
        }
    }

    printf("  Delete 19:\n");
    rh_delete(t, 19);

    for (int i = 0; i < 10; i++) {
        if (t->occupied[i]) {
            int disp = rh_displacement(t, i);
            printf("    slot %d: %d (h=%d, disp=%d)\n",
                   i, t->keys[i],
                   ((t->keys[i] % 16) + 16) % 16, disp);
        } else {
            printf("    slot %d: EMPTY\n", i);
        }
    }

    int probes;
    int val = rh_search(t, 35, &probes);
    printf("    search 35: value=%d, probes=%d\n", val, probes);
    val = rh_search(t, 4, &probes);
    printf("    search 4: value=%d, probes=%d\n\n", val, probes);

    rh_free(t);
}

void demo6_performance_comparison(void) {
    printf("=== Demo 6: performance comparison (insert/delete cycles) ===\n");
    int cap = 10007;  // prime
    int n_insert = 7000;
    int n_delete = 5000;

    TombstoneTable *tt = tt_create(cap);
    ShiftTable *st = st_create(cap);
    RobinTable *rh = rh_create(cap);

    srand(42);
    int *keys = malloc(n_insert * sizeof(int));
    for (int i = 0; i < n_insert; i++) {
        keys[i] = rand() % 100000;
    }

    // insert all
    for (int i = 0; i < n_insert; i++) {
        tt_insert(tt, keys[i], i);
        st_insert(st, keys[i], i);
        rh_insert(rh, keys[i], i);
    }

    // delete first n_delete
    for (int i = 0; i < n_delete; i++) {
        tt_delete(tt, keys[i]);
        st_delete(st, keys[i]);
        rh_delete(rh, keys[i]);
    }

    printf("  After %d inserts + %d deletes:\n", n_insert, n_delete);
    printf("  Tombstone table: ");
    tt_stats(tt);

    // measure search performance for remaining keys
    long tt_probes = 0, st_probes = 0, rh_probes = 0;
    int found = 0;
    for (int i = n_delete; i < n_insert; i++) {
        int p;
        tt_search(tt, keys[i], &p); tt_probes += p;
        st_search(st, keys[i], &p); st_probes += p;
        rh_search(rh, keys[i], &p); rh_probes += p;
        found++;
    }

    printf("  Avg probes for %d existing keys:\n", found);
    printf("    Tombstone:      %.2f\n", (double)tt_probes / found);
    printf("    Backward shift: %.2f\n", (double)st_probes / found);
    printf("    Robin Hood:     %.2f\n", (double)rh_probes / found);

    // measure miss performance
    long tt_miss = 0, st_miss = 0, rh_miss = 0;
    int miss_count = 2000;
    for (int i = 0; i < miss_count; i++) {
        int p;
        int fake_key = 200000 + i;
        tt_search(tt, fake_key, &p); tt_miss += p;
        st_search(st, fake_key, &p); st_miss += p;
        rh_search(rh, fake_key, &p); rh_miss += p;
    }

    printf("  Avg probes for %d missing keys:\n", miss_count);
    printf("    Tombstone:      %.2f\n", (double)tt_miss / miss_count);
    printf("    Backward shift: %.2f\n", (double)st_miss / miss_count);
    printf("    Robin Hood:     %.2f\n", (double)rh_miss / miss_count);

    // rehash cleanup for tombstone table
    tt_rehash_cleanup(tt);
    tt_probes = 0;
    for (int i = n_delete; i < n_insert; i++) {
        int p;
        tt_search(tt, keys[i], &p); tt_probes += p;
    }
    printf("  After rehash cleanup:\n");
    printf("    Tombstone:      %.2f (hit)\n",
           (double)tt_probes / found);

    free(keys);
    tt_free(tt);
    st_free(st);
    rh_free(rh);
    printf("\n");
}

int main(void) {
    demo1_why_empty_breaks();
    demo2_tombstone_basic();
    demo3_tombstone_accumulation();
    demo4_backward_shift();
    demo5_robin_hood();
    demo6_performance_comparison();
    return 0;
}
```

---

## Programa completo en Rust

```rust
// deletion_strategies.rs
// Deletion strategies in open addressing: tombstone vs backward shift
// vs Robin Hood hashing
// Run: rustc -O deletion_strategies.rs && ./deletion_strategies

use std::fmt;

// ============================================================
// Tombstone-based hash table
// ============================================================

#[derive(Clone, Copy, PartialEq)]
enum SlotState {
    Empty,
    Occupied,
    Deleted,
}

struct TombstoneTable {
    keys: Vec<i64>,
    values: Vec<i64>,
    state: Vec<SlotState>,
    capacity: usize,
    count: usize,
    tombstones: usize,
}

impl TombstoneTable {
    fn new(capacity: usize) -> Self {
        Self {
            keys: vec![0; capacity],
            values: vec![0; capacity],
            state: vec![SlotState::Empty; capacity],
            capacity,
            count: 0,
            tombstones: 0,
        }
    }

    fn hash(&self, key: i64) -> usize {
        (key.rem_euclid(self.capacity as i64)) as usize
    }

    fn insert(&mut self, key: i64, value: i64) -> bool {
        let mut idx = self.hash(key);
        let mut first_deleted: Option<usize> = None;

        for _ in 0..self.capacity {
            match self.state[idx] {
                SlotState::Empty => {
                    let target = first_deleted.unwrap_or(idx);
                    self.keys[target] = key;
                    self.values[target] = value;
                    if first_deleted.is_some() {
                        self.tombstones -= 1;
                    }
                    self.state[target] = SlotState::Occupied;
                    self.count += 1;
                    return true;
                }
                SlotState::Occupied if self.keys[idx] == key => {
                    self.values[idx] = value;
                    return false;
                }
                SlotState::Deleted if first_deleted.is_none() => {
                    first_deleted = Some(idx);
                }
                _ => {}
            }
            idx = (idx + 1) % self.capacity;
        }
        false
    }

    fn search(&self, key: i64) -> Option<(i64, usize)> {
        let mut idx = self.hash(key);
        let mut probes = 0;

        for _ in 0..self.capacity {
            probes += 1;
            match self.state[idx] {
                SlotState::Empty => return None,
                SlotState::Occupied if self.keys[idx] == key => {
                    return Some((self.values[idx], probes));
                }
                _ => {}
            }
            idx = (idx + 1) % self.capacity;
        }
        None
    }

    fn search_probes(&self, key: i64) -> usize {
        let mut idx = self.hash(key);
        let mut probes = 0;
        for _ in 0..self.capacity {
            probes += 1;
            match self.state[idx] {
                SlotState::Empty => return probes,
                SlotState::Occupied if self.keys[idx] == key => return probes,
                _ => {}
            }
            idx = (idx + 1) % self.capacity;
        }
        probes
    }

    fn delete(&mut self, key: i64) -> bool {
        let mut idx = self.hash(key);

        for _ in 0..self.capacity {
            match self.state[idx] {
                SlotState::Empty => return false,
                SlotState::Occupied if self.keys[idx] == key => {
                    self.state[idx] = SlotState::Deleted;
                    self.count -= 1;
                    self.tombstones += 1;
                    return true;
                }
                _ => {}
            }
            idx = (idx + 1) % self.capacity;
        }
        false
    }

    fn rehash_cleanup(&mut self) {
        let old_keys = self.keys.clone();
        let old_values = self.values.clone();
        let old_state = self.state.clone();

        self.keys = vec![0; self.capacity];
        self.values = vec![0; self.capacity];
        self.state = vec![SlotState::Empty; self.capacity];
        self.count = 0;
        self.tombstones = 0;

        for i in 0..self.capacity {
            if old_state[i] == SlotState::Occupied {
                self.insert(old_keys[i], old_values[i]);
            }
        }
    }
}

impl fmt::Display for TombstoneTable {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let occ = self.state.iter().filter(|s| **s == SlotState::Occupied).count();
        let del = self.state.iter().filter(|s| **s == SlotState::Deleted).count();
        let emp = self.state.iter().filter(|s| **s == SlotState::Empty).count();
        write!(f, "cap={}, occ={}, tomb={}, empty={}, α={:.3}, α_eff={:.3}",
               self.capacity, occ, del, emp,
               occ as f64 / self.capacity as f64,
               (occ + del) as f64 / self.capacity as f64)
    }
}

// ============================================================
// Backward-shift hash table
// ============================================================

struct ShiftTable {
    keys: Vec<i64>,
    values: Vec<i64>,
    occupied: Vec<bool>,
    capacity: usize,
    count: usize,
}

impl ShiftTable {
    fn new(capacity: usize) -> Self {
        Self {
            keys: vec![0; capacity],
            values: vec![0; capacity],
            occupied: vec![false; capacity],
            capacity,
            count: 0,
        }
    }

    fn hash(&self, key: i64) -> usize {
        (key.rem_euclid(self.capacity as i64)) as usize
    }

    fn insert(&mut self, key: i64, value: i64) -> bool {
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

    fn search_probes(&self, key: i64) -> usize {
        let mut idx = self.hash(key);
        let mut probes = 0;
        for _ in 0..self.capacity {
            probes += 1;
            if !self.occupied[idx] { return probes; }
            if self.keys[idx] == key { return probes; }
            idx = (idx + 1) % self.capacity;
        }
        probes
    }

    fn delete(&mut self, key: i64) -> bool {
        let mut idx = self.hash(key);

        // find key
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
            let h = self.hash(self.keys[j]);
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
        true
    }
}

// ============================================================
// Robin Hood hash table
// ============================================================

struct RobinTable {
    keys: Vec<i64>,
    values: Vec<i64>,
    occupied: Vec<bool>,
    capacity: usize,
    count: usize,
}

impl RobinTable {
    fn new(capacity: usize) -> Self {
        Self {
            keys: vec![0; capacity],
            values: vec![0; capacity],
            occupied: vec![false; capacity],
            capacity,
            count: 0,
        }
    }

    fn hash(&self, key: i64) -> usize {
        (key.rem_euclid(self.capacity as i64)) as usize
    }

    fn displacement(&self, slot: usize) -> usize {
        let h = self.hash(self.keys[slot]);
        (slot + self.capacity - h) % self.capacity
    }

    fn insert(&mut self, key: i64, value: i64) -> bool {
        let mut idx = self.hash(key);
        let mut cur_key = key;
        let mut cur_val = value;
        let mut disp = 0usize;
        let mut is_new = true;

        for _ in 0..self.capacity {
            if !self.occupied[idx] {
                self.keys[idx] = cur_key;
                self.values[idx] = cur_val;
                self.occupied[idx] = true;
                if is_new { self.count += 1; }
                return true;
            }

            if self.keys[idx] == cur_key && is_new {
                self.values[idx] = cur_val;
                return false;
            }

            let existing_disp = self.displacement(idx);
            if existing_disp < disp {
                std::mem::swap(&mut self.keys[idx], &mut cur_key);
                std::mem::swap(&mut self.values[idx], &mut cur_val);
                disp = existing_disp;
                is_new = true;
            }

            disp += 1;
            idx = (idx + 1) % self.capacity;
        }
        false
    }

    fn search_probes(&self, key: i64) -> usize {
        let mut idx = self.hash(key);
        let mut probes = 0;

        for i in 0..self.capacity {
            probes += 1;
            if !self.occupied[idx] { return probes; }
            if self.keys[idx] == key { return probes; }

            let existing_disp = self.displacement(idx);
            if existing_disp < i { return probes; }

            idx = (idx + 1) % self.capacity;
        }
        probes
    }

    fn delete(&mut self, key: i64) -> bool {
        let mut idx = self.hash(key);

        let mut found = false;
        for i in 0..self.capacity {
            if !self.occupied[idx] { return false; }
            if self.keys[idx] == key { found = true; break; }
            let existing_disp = self.displacement(idx);
            if existing_disp < i { return false; }
            idx = (idx + 1) % self.capacity;
        }
        if !found { return false; }

        // backward shift (Robin Hood version)
        let mut gap = idx;
        let mut next = (gap + 1) % self.capacity;

        while self.occupied[next] {
            let disp = self.displacement(next);
            if disp == 0 { break; }

            self.keys[gap] = self.keys[next];
            self.values[gap] = self.values[next];
            gap = next;
            next = (next + 1) % self.capacity;
        }

        self.occupied[gap] = false;
        self.count -= 1;
        true
    }
}

// ============================================================
// Demos
// ============================================================

fn demo1_why_empty_breaks() {
    println!("=== Demo 1: why EMPTY breaks probe chains ===");

    let mut table = [0i64; 8];
    let mut occupied = [false; 8];

    table[3] = 3; occupied[3] = true;
    table[4] = 11; occupied[4] = true;
    table[5] = 19; occupied[5] = true;

    print!("  After insert 3, 11, 19 (all hash to 3): ");
    for i in 0..8 {
        if occupied[i] { print!("[{}]", table[i]); }
        else { print!("[_]"); }
    }
    println!();

    // wrong: set EMPTY
    occupied[4] = false;
    print!("  Delete 11 with EMPTY (WRONG):            ");
    for i in 0..8 {
        if occupied[i] { print!("[{}]", table[i]); }
        else { print!("[_]"); }
    }
    println!();

    let mut idx = (19 % 8) as usize;
    print!("  Search 19: h={}...", idx);
    while occupied[idx] {
        print!(" slot {}({})", idx, table[idx]);
        idx = (idx + 1) % 8;
    }
    println!(" slot {}=EMPTY -> FALSE NEGATIVE!", idx);
    println!("  19 is still in slot 5!\n");
}

fn demo2_tombstone_basic() {
    println!("=== Demo 2: tombstone preserves chain ===");
    let mut t = TombstoneTable::new(16);

    t.insert(3, 30);
    t.insert(19, 190);
    t.insert(35, 350);

    println!("  Inserted 3, 19, 35 (all h%16=3)");
    t.delete(19);
    println!("  Deleted 19 (tombstone at slot 4)");

    if let Some((val, probes)) = t.search(35) {
        println!("  Search 35: value={}, probes={} (chain preserved!)\n",
                 val, probes);
    }
}

fn demo3_accumulation() {
    println!("=== Demo 3: tombstone accumulation ===");
    let mut t = TombstoneTable::new(1000);

    for i in 0..700i64 {
        t.insert(i * 17 + 1, i);
    }
    for i in 0..500i64 {
        t.delete(i * 17 + 1);
    }

    println!("  {}", t);

    let avg: f64 = (500..700i64)
        .map(|i| t.search_probes(i * 17 + 1) as f64)
        .sum::<f64>() / 200.0;
    println!("  Avg probes (hit, with tombstones): {:.2}", avg);

    t.rehash_cleanup();
    println!("  After cleanup: {}", t);

    let avg: f64 = (500..700i64)
        .map(|i| t.search_probes(i * 17 + 1) as f64)
        .sum::<f64>() / 200.0;
    println!("  Avg probes (hit, after cleanup):   {:.2}\n", avg);
}

fn demo4_backward_shift() {
    println!("=== Demo 4: backward shift delete ===");
    let mut t = ShiftTable::new(16);

    t.insert(3, 30);
    t.insert(19, 190);
    t.insert(35, 350);
    t.insert(51, 510);
    t.insert(4, 40);

    println!("  Before:");
    for i in 0..10 {
        if t.occupied[i] {
            println!("    slot {}: {} (h={})", i, t.keys[i], t.hash(t.keys[i]));
        }
    }

    t.delete(19);
    println!("  After deleting 19:");
    for i in 0..10 {
        if t.occupied[i] {
            println!("    slot {}: {} (h={})", i, t.keys[i], t.hash(t.keys[i]));
        } else if i <= 7 {
            println!("    slot {}: EMPTY", i);
        }
    }
    println!();
}

fn demo5_robin_hood() {
    println!("=== Demo 5: Robin Hood hashing ===");
    let mut t = RobinTable::new(16);

    t.insert(3, 30);
    t.insert(19, 190);
    t.insert(35, 350);
    t.insert(4, 40);

    println!("  After inserts:");
    for i in 0..10 {
        if t.occupied[i] {
            println!("    slot {}: {} (h={}, disp={})",
                     i, t.keys[i], t.hash(t.keys[i]), t.displacement(i));
        }
    }

    t.delete(19);
    println!("  After deleting 19:");
    for i in 0..10 {
        if t.occupied[i] {
            println!("    slot {}: {} (h={}, disp={})",
                     i, t.keys[i], t.hash(t.keys[i]), t.displacement(i));
        }
    }
    println!();
}

fn demo6_comparison() {
    println!("=== Demo 6: performance comparison ===");
    let cap = 10007;
    let n_insert = 7000;
    let n_delete = 5000;

    let mut tt = TombstoneTable::new(cap);
    let mut st = ShiftTable::new(cap);
    let mut rh = RobinTable::new(cap);

    // simple LCG for reproducibility
    let mut rng = 42u64;
    let mut keys = Vec::with_capacity(n_insert);
    for _ in 0..n_insert {
        rng = rng.wrapping_mul(6364136223846793005).wrapping_add(1);
        keys.push((rng >> 33) as i64);
    }

    for &k in &keys {
        tt.insert(k, k);
        st.insert(k, k);
        rh.insert(k, k);
    }

    for &k in &keys[..n_delete] {
        tt.delete(k);
        st.delete(k);
        rh.delete(k);
    }

    println!("  After {} inserts + {} deletes:", n_insert, n_delete);
    println!("  Tombstone: {}", tt);

    let remaining = &keys[n_delete..];
    let tt_avg: f64 = remaining.iter()
        .map(|k| tt.search_probes(*k) as f64).sum::<f64>() / remaining.len() as f64;
    let st_avg: f64 = remaining.iter()
        .map(|k| st.search_probes(*k) as f64).sum::<f64>() / remaining.len() as f64;
    let rh_avg: f64 = remaining.iter()
        .map(|k| rh.search_probes(*k) as f64).sum::<f64>() / remaining.len() as f64;

    println!("  Avg probes (hit):");
    println!("    Tombstone:      {:.2}", tt_avg);
    println!("    Backward shift: {:.2}", st_avg);
    println!("    Robin Hood:     {:.2}", rh_avg);

    // miss probes
    let tt_miss: f64 = (200000..202000i64)
        .map(|k| tt.search_probes(k) as f64).sum::<f64>() / 2000.0;
    let st_miss: f64 = (200000..202000i64)
        .map(|k| st.search_probes(k) as f64).sum::<f64>() / 2000.0;
    let rh_miss: f64 = (200000..202000i64)
        .map(|k| rh.search_probes(k) as f64).sum::<f64>() / 2000.0;

    println!("  Avg probes (miss):");
    println!("    Tombstone:      {:.2}", tt_miss);
    println!("    Backward shift: {:.2}", st_miss);
    println!("    Robin Hood:     {:.2}", rh_miss);

    tt.rehash_cleanup();
    let tt_clean: f64 = remaining.iter()
        .map(|k| tt.search_probes(*k) as f64).sum::<f64>() / remaining.len() as f64;
    println!("  After cleanup:");
    println!("    Tombstone:      {:.2} (hit)\n", tt_clean);
}

fn main() {
    demo1_why_empty_breaks();
    demo2_tombstone_basic();
    demo3_accumulation();
    demo4_backward_shift();
    demo5_robin_hood();
    demo6_comparison();
}
```

---

## Ejercicios

### Ejercicio 1 — Traza de eliminación EMPTY vs DELETED

En una tabla de $m = 8$ con sonda lineal y $h(k) = k \bmod 8$,
inserta las claves 5, 13, 21, 6, 14 en ese orden. Luego elimina
13. Muestra qué pasa al buscar 21 y 14 si usas EMPTY vs DELETED.

<details><summary>¿Qué resultado da la búsqueda de 21 con EMPTY en el slot de 13?</summary>

Después de insertar: slot 5=5, 6=13, 7=21, 0=6 (wraparound, h=6
ocupado), 1=14 (h=6 continuación).

Eliminar 13 con EMPTY: slot 6 = EMPTY.

Buscar 21: $h(21)=5$, slot 5 (5≠21), slot 6 EMPTY → "no existe".
**Falso negativo** — 21 está en slot 7.

Con DELETED: slot 6 DELETED → continuar, slot 7 (21✓). Correcto.

</details>

<details><summary>¿Y la búsqueda de 14?</summary>

Buscar 14: $h(14)=6$.

Con EMPTY: slot 6 EMPTY → "no existe". **Falso negativo** — 14
está en slot 1 (fue desplazado por la cadena 6, 13, 21, 6→0, 14→1).

Con DELETED: slot 6 DELETED → continuar, slot 7 (21≠14), slot 0
(6≠14), slot 1 (14✓). Correcto: 4 sondeos.

</details>

---

### Ejercicio 2 — Conteo de sondeos con tombstones

Tabla $m = 16$, sonda lineal, $h(k) = k \bmod 16$. Inserta las
claves 2, 18, 34, 50, 66. Elimina 18 y 50 con tombstones. ¿Cuántos
sondeos requiere buscar 66? ¿Cuántos tras un rehash de limpieza?

<details><summary>¿Cuántos sondeos con tombstones?</summary>

Posiciones iniciales: slot 2=2, 3=18, 4=34, 5=50, 6=66.

Tras eliminar 18 y 50: slot 2=2(OCC), 3=DEL, 4=34(OCC), 5=DEL,
6=66(OCC).

Buscar 66: $h(66)=2$. Slot 2 (2≠66), slot 3 (DEL, continuar),
slot 4 (34≠66), slot 5 (DEL, continuar), slot 6 (66✓) = **5
sondeos**.

</details>

<details><summary>¿Cuántos tras rehash?</summary>

Después del rehash, solo 2, 34, 66 están en la tabla.
2 → slot 2, 34 → slot 2 ocupado → slot 3, 66 → slot 2 ocupado →
slot 3 ocupado → slot 4.

Buscar 66: slot 2 (2≠66), slot 3 (34≠66), slot 4 (66✓) = **3
sondeos**.

Ahorro: de 5 a 3 sondeos (40% menos).

</details>

---

### Ejercicio 3 — Backward shift: traza con wraparound

Tabla $m = 8$, sonda lineal. Estado actual:

```
slot: 0   1  2  3  4  5  6   7
      9  17  _  _  _  5  13  8
h:    1   1  _  _  _  5   5  0
```

Elimina la clave 5 (slot 5) usando backward shift. Muestra el
estado paso a paso.

<details><summary>¿Qué elemento se desplaza primero?</summary>

Gap = 5. Siguiente: slot 6, key=13, h=5.
gap(5) < j(6): $h(5) \leq gap(5)$? Sí → shift.
Mover 13 de slot 6 a slot 5. Gap = 6.

</details>

<details><summary>¿Estado final?</summary>

Gap = 6. Siguiente: slot 7, key=8, h=0.
gap(6) < j(7): $h(0) \leq gap(6)$? Sí → shift.
Mover 8 de slot 7 a slot 6. Gap = 7.

Siguiente: slot 0, key=9, h=1.
gap(7) > j(0) [wraparound]: $h(1) \leq gap(7)$ AND $h(1) > j(0)$?
$1 \leq 7$ sí, $1 > 0$ sí → shift.
Mover 9 de slot 0 a slot 7. Gap = 0.

Siguiente: slot 1, key=17, h=1.
gap(0) < j(1): $h(1) \leq gap(0)$? $1 \leq 0$? No → no shift.

Marcar slot 0 EMPTY.

```
slot: _  17  _  _  _  13  8  9
h:    _   1  _  _  _   5  0  1
```

Todos los elementos se acercaron a su posición natural.

</details>

---

### Ejercicio 4 — Factor de carga efectivo

Una tabla de $m = 500$ con sonda lineal tiene $n = 100$ elementos
activos y $d = 250$ tombstones. Calcula:
a) $\alpha_{\text{real}}$ y $\alpha_{\text{eff}}$.
b) Sondeos esperados para búsqueda fallida (Knuth).
c) Los mismos valores después de un rehash de limpieza.

<details><summary>a) Factores de carga</summary>

$\alpha_{\text{real}} = 100/500 = 0.2$

$\alpha_{\text{eff}} = (100 + 250)/500 = 350/500 = 0.7$

</details>

<details><summary>b) Sondeos con tombstones</summary>

Fórmula de Knuth (miss): $\frac{1}{2}\left(1 + \frac{1}{(1-\alpha)^2}\right)$

Con $\alpha_{\text{eff}} = 0.7$:
$\frac{1}{2}\left(1 + \frac{1}{(0.3)^2}\right) = \frac{1}{2}(1 + 11.11) = 6.06$ sondeos.

</details>

<details><summary>c) Después del rehash</summary>

Con $\alpha = 0.2$ (sin tombstones):
$\frac{1}{2}\left(1 + \frac{1}{(0.8)^2}\right) = \frac{1}{2}(1 + 1.5625) = 1.28$ sondeos.

Mejora: de 6.06 a 1.28 — factor $4.7\times$.

</details>

---

### Ejercicio 5 — Implementar eliminación con backward shift para strings

Modifica la implementación de `ShiftTable` para que funcione con
claves `char *` en vez de `int`. Usa `djb2` como función hash y
`strcmp` para comparar. Inserta 10 palabras, elimina 3, y verifica
que las 7 restantes son encontrables.

<details><summary>¿Qué cambia en la condición de shift?</summary>

La lógica de backward shift es idéntica — solo cambia:
- `t->keys` de `int *` a `char **` (punteros a strings).
- Comparaciones `==` por `strcmp() == 0`.
- Hash `key % capacity` por `djb2(key) % capacity`.
- Liberar las strings al destruir la tabla (si son copias con
  `strdup`).

La condición circular `should_shift` no cambia porque opera sobre
índices, no sobre claves.

</details>

<details><summary>¿Hay problemas con la gestión de memoria?</summary>

Sí: al hacer backward shift, se copian **punteros**, no strings.
Si las claves son `strdup`, el shift copia el puntero de `keys[j]`
a `keys[gap]`. No hay leak porque no se duplica ni libera — solo
se mueve la referencia. Al final, `keys[gap]` (el último gap) se
pone a `NULL` y no se hace `free` porque el puntero ya se movió a
una posición anterior.

</details>

---

### Ejercicio 6 — Robin Hood: verificar propiedad de varianza

Inserta 1000 claves aleatorias en una tabla Robin Hood de
$m = 2003$ (primo) y calcula la **varianza** del displacement de
todos los elementos. Compara con la varianza de sonda lineal
estándar para las mismas claves.

<details><summary>¿Cuál tiene menor varianza?</summary>

Robin Hood **siempre** tiene menor varianza de displacement. La
regla de intercambiar con elementos de menor displacement redistribuye
los "costos" de las colisiones. Típicamente:
- Sonda lineal: varianza alta (algunos con disp=0, otros con disp>10).
- Robin Hood: varianza baja (la mayoría con disp cercano al promedio).

El displacement máximo también es menor en Robin Hood:
$O(\log \log n)$ esperado vs $O(\log n)$ para sonda lineal.

</details>

<details><summary>¿Cómo afecta esto a la búsqueda?</summary>

Menor varianza implica búsquedas más **predecibles**. Además, el
early exit de Robin Hood (detener cuando `displacement_existente <
mis_sondeos`) funciona mejor con menor varianza — corta antes las
búsquedas fallidas.

</details>

---

### Ejercicio 7 — Umbral óptimo de rehash

Experimenta con una tabla de $m = 1009$. Inserta y elimina claves
aleatoriamente (50% insert, 50% delete) durante 10000 operaciones.
Mide el promedio de sondeos por búsqueda al final para diferentes
umbrales de rehash: $d/m > 0.1, 0.25, 0.5$, y sin rehash.

<details><summary>¿Qué umbral da mejor rendimiento total?</summary>

Típicamente $d/m > 0.25$ ofrece el mejor balance:
- $0.1$: rehash demasiado frecuente, costo $O(m)$ se amortiza mal.
- $0.25$: buen equilibrio entre sondeos y costo de rehash.
- $0.5$: las cadenas ya se degradan significativamente.
- Sin rehash: peor rendimiento de búsqueda, pero menor costo total
  de rehash.

El óptimo depende del ratio insert/delete y del patrón de acceso.

</details>

<details><summary>¿Cuántos rehashes se disparan con cada umbral?</summary>

Con 10000 operaciones (50/50):
- $0.1$: ~20-30 rehashes (cada ~400 operaciones).
- $0.25$: ~8-12 rehashes.
- $0.5$: ~3-5 rehashes.
- Sin rehash: 0.

El costo total de rehash es $\text{num\_rehashes} \times O(m)$.
Con $m = 1009$ y umbral 0.25, eso es ~$10 \times 1009 \approx 10^4$
operaciones extra — menos del costo acumulado de sondeos largos.

</details>

---

### Ejercicio 8 — Comparación de memoria

Calcula el **overhead de memoria** por slot para cada estrategia:

| Estrategia | ¿Qué almacena por slot? |
|------------|------------------------|
| Tombstone (enum) | key + value + state (2 bits mínimo) |
| Backward shift (bool) | key + value + occupied (1 bit mínimo) |
| Robin Hood (bool) | key + value + occupied (1 bit mínimo) |
| Swiss Table (byte) | key + value + control_byte (1 byte) |

<details><summary>¿Cuántos bytes por slot con claves de 8 bytes y valores de 8 bytes?</summary>

- **Tombstone**: 8 + 8 + 1 (state como `char`) = 17 bytes. Con
  padding a 8 bytes: 24 bytes.
- **Backward shift**: 8 + 8 + 1 (bool) = 17 bytes → 24 con padding.
- **Robin Hood**: igual que backward shift: 24 bytes.
- **Swiss Table**: 1 byte de control separado + 8 + 8 = 17 bytes.
  Pero los control bytes se almacenan en un **array separado** de
  16 bytes (un grupo SIMD), así que no hay padding por slot: 16 +
  1 bytes efectivos = 17 bytes.

Swiss Table ahorra ~30% de memoria vs las implementaciones con
padding.

</details>

<details><summary>¿Cómo se puede reducir el padding?</summary>

Usando **arrays separados** (Structure of Arrays, SoA) en vez de
un struct por slot:

```c
// AoS (con padding): 24 bytes/slot
struct Slot { int64_t key; int64_t value; char state; };

// SoA (sin padding): 17 bytes/slot
int64_t keys[m];
int64_t values[m];
char state[m];
```

Esta es exactamente la estrategia que usan Swiss Table y nuestra
implementación. El acceso a `state[i]` es contiguo en memoria,
excelente para SIMD.

</details>

---

### Ejercicio 9 — Backward shift con sonda cuadrática

Intenta implementar backward shift delete para sonda cuadrática
($h(k) + i(i+1)/2$). ¿Por qué falla?

<details><summary>¿Cuál es el problema fundamental?</summary>

En sonda cuadrática, los slots de una cadena de sondeo **no son
consecutivos**. Después de eliminar un slot, los elementos
siguientes en la cadena no están en slots adyacentes sino en
posiciones dispersas: $h+1, h+3, h+6, h+10, \ldots$

Para hacer backward shift necesitaríamos:
1. Saber cuáles son **todos** los slots que forman parte de alguna
   cadena que pasa por el slot eliminado.
2. Recalcular la secuencia de sondeo de cada candidato.
3. Mover cada uno a su nueva posición correcta.

Esto es $O(m)$ en el peor caso (escanear toda la tabla) y
extremadamente complejo. No vale la pena.

</details>

<details><summary>¿Qué alternativas quedan para cuadrática y doble hashing?</summary>

1. **Tombstones**: la solución estándar, con rehash periódico.
2. **Cuckoo hashing**: no usa sondeo lineal/cuadrático, y la
   eliminación es $O(1)$ directa (sin tombstones ni shift).
3. **Rehash incremental**: en cada operación, re-ubicar 1-2
   tombstones encontrados durante el sondeo.

En la práctica, la mayoría de implementaciones con sondeo no-lineal
usan tombstones.

</details>

---

### Ejercicio 10 — Diseño de política de limpieza adaptativa

Diseña una política de rehash que se adapte al patrón de uso:
- Si el ratio de eliminaciones es alto (>30% de operaciones),
  usar un umbral de rehash bajo ($d/m > 0.15$).
- Si el ratio es bajo (<10%), usar un umbral alto ($d/m > 0.5$).
- Implementa un contador de operaciones de insert/delete para
  calcular el ratio dinámicamente.

<details><summary>¿Cómo implementar el contador de ratio?</summary>

```c
typedef struct {
    // ... campos de la tabla ...
    int op_inserts;    // counter since last rehash
    int op_deletes;    // counter since last rehash
} AdaptiveTable;

void check_rehash(AdaptiveTable *t) {
    int total_ops = t->op_inserts + t->op_deletes;
    if (total_ops < 100) return;  // not enough data

    double delete_ratio = (double)t->op_deletes / total_ops;
    double threshold;

    if (delete_ratio > 0.3)      threshold = 0.15;
    else if (delete_ratio > 0.1) threshold = 0.25;
    else                         threshold = 0.50;

    if ((double)t->tombstones / t->capacity > threshold) {
        rehash_cleanup(t);
        t->op_inserts = 0;
        t->op_deletes = 0;
    }
}
```

</details>

<details><summary>¿Qué patrón se beneficia más?</summary>

El patrón que más se beneficia es **delete-heavy con búsquedas
frecuentes**: muchas eliminaciones generan tombstones rápidamente,
y las búsquedas pagan el costo. La política adaptativa detecta
este patrón y limpia antes.

Un patrón **insert-heavy** con pocas eliminaciones acumula pocos
tombstones, así que el umbral alto evita rehashes innecesarios.

El caso intermedio (50/50 insert/delete) es donde la adaptación
más importa: sin ella, podrías limpiar demasiado pronto o
demasiado tarde según el umbral fijo elegido.

</details>
