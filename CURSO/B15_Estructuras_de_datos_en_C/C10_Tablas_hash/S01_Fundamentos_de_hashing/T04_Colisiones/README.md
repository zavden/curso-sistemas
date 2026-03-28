# Colisiones

## Objetivo

Comprender por qué las colisiones son **inevitables** en una tabla
hash, clasificar las estrategias de resolución, y preparar el
terreno para las implementaciones detalladas de S02:

- **Principio del palomar**: por qué siempre habrá colisiones.
- **Birthday paradox**: por qué ocurren mucho antes de lo esperado.
- **Taxonomía de estrategias**: chaining vs open addressing, con
  sus variantes.
- **Impacto en complejidad**: de $O(1)$ a $O(n)$ según la
  estrategia y el factor de carga.
- Referencia: S02 implementa cada estrategia en detalle.

---

## Por qué las colisiones son inevitables

### Principio del palomar (pigeonhole principle)

Si hay $n$ palomas y $m$ casilleros con $n > m$, al menos un
casillero contiene más de una paloma.

Aplicado a hashing: si el espacio de claves posibles $|U|$ es
mayor que el número de buckets $m$ (y lo es — $|U|$ suele ser
infinito o astronómicamente grande), entonces existen claves
distintas $k_1 \neq k_2$ tales que $h(k_1) = h(k_2)$.

```
Espacio de claves (strings): infinito
Buckets: m = 1000

No importa lo buena que sea h():
  existen k1, k2 con h(k1) = h(k2)
```

Incluso si $n < m$ (menos elementos que buckets), las colisiones
ocurren en la práctica por el birthday paradox.

### Birthday paradox

¿Con cuántas personas en una sala hay >50% de probabilidad de que
dos compartan cumpleaños?

Respuesta: **23** (de 365 días posibles).

La probabilidad de que **no** haya colisión con $n$ elementos
en $m$ buckets (asumiendo hash uniforme):

$$P(\text{sin colisión}) = \prod_{i=0}^{n-1} \frac{m - i}{m} = \frac{m!}{(m-n)! \cdot m^n}$$

$$P(\text{colisión}) = 1 - P(\text{sin colisión})$$

Aproximación: la primera colisión se espera con
$n \approx \sqrt{\frac{\pi \cdot m}{2}} \approx 1.25 \sqrt{m}$.

### Tabla: primera colisión esperada

| $m$ (buckets) | Primera colisión ($\approx 1.25\sqrt{m}$) | $\alpha$ en ese punto |
|---------:|---------:|---------:|
| 100 | ~13 | 0.13 |
| 1,000 | ~40 | 0.04 |
| 10,000 | ~125 | 0.0125 |
| 100,000 | ~395 | 0.004 |
| 1,000,000 | ~1,250 | 0.00125 |
| $2^{32}$ | ~82,138 | 0.00002 |

Con $m = 10000$ buckets, la primera colisión ocurre con solo
~125 elementos ($\alpha = 0.0125$). No hay forma de evitarlas —
solo de **manejarlas**.

---

## Ejemplo concreto de colisión

```c
unsigned int h(const char *s, int m) {
    unsigned int hash = 5381;
    while (*s) hash = hash * 33 + *s++;
    return hash % m;
}
```

Con $m = 11$:

```
h("hello") = 7
h("world") = 3
h("foo")   = 7    ← colision con "hello"!
h("bar")   = 7    ← otra colision!
h("baz")   = 10
```

Tres claves (`"hello"`, `"foo"`, `"bar"`) compiten por el
bucket 7. ¿Qué hacemos?

---

## Taxonomía de estrategias

Hay dos familias principales de resolución de colisiones:

```
Resolucion de colisiones
├── Encadenamiento separado (chaining)
│   ├── Lista enlazada (clasico)
│   ├── Array dinamico por bucket
│   └── Arbol por bucket (Java 8+ TreeMap si chain > 8)
│
└── Direccionamiento abierto (open addressing)
    ├── Sonda lineal (linear probing)
    ├── Sonda cuadratica (quadratic probing)
    ├── Doble hashing (double hashing)
    ├── Robin Hood hashing
    ├── Cuckoo hashing
    └── Swiss Table (hashbrown / Rust HashMap)
```

### Chaining (encadenamiento separado)

Cada bucket contiene una **estructura auxiliar** (típicamente una
lista enlazada) que almacena todos los elementos que hashean a ese
bucket:

```
tabla[7]: "hello" → "foo" → "bar" → NULL
tabla[3]: "world" → NULL
tabla[10]: "baz" → NULL
(demas buckets vacios)
```

**Ventajas**:
- Simple de implementar.
- $\alpha$ puede superar 1 (más elementos que buckets).
- La eliminación es trivial (borrar de la lista).
- El peor caso es una lista larga, no un bloqueo total.

**Desventajas**:
- Memoria extra para punteros de lista (8 bytes por nodo en
  64-bit).
- Mala localidad de caché: los nodos de la lista están dispersos
  en el heap.
- Overhead de `malloc`/`free` por inserción/eliminación.

### Open addressing (direccionamiento abierto)

Todos los elementos se almacenan **dentro del array** de la tabla.
Si el bucket calculado está ocupado, se busca el siguiente
disponible según una **secuencia de sondeo**:

```
tabla: [_, _, _, "world", _, _, _, "hello", "foo", "bar", "baz"]
        0  1  2    3     4  5  6    7       8      9      10

"hello" → h=7, bucket 7 libre → ok
"foo"   → h=7, bucket 7 ocupado → probar 8 → libre → ok
"bar"   → h=7, buckets 7,8 ocupados → probar 9 → libre → ok
```

**Ventajas**:
- Sin memoria extra (todo en el array).
- Excelente localidad de caché (accesos secuenciales al array).
- Sin overhead de allocation.

**Desventajas**:
- $\alpha$ no puede alcanzar 1 (tabla llena = no insertar).
- Eliminación compleja (tombstones).
- Clustering: buckets ocupados tienden a agruparse, degradando
  búsquedas.
- Rendimiento se degrada exponencialmente cerca de $\alpha = 1$.

---

## Las variantes de open addressing

### Sonda lineal

Secuencia de sondeo: $h(k), h(k)+1, h(k)+2, \ldots$ (mod $m$).

```
h(k) = 7, m = 11:
  probar 7, 8, 9, 10, 0, 1, 2, 3, 4, 5, 6
```

Simple y excelente localidad de caché (accesos contiguos). Pero
sufre **clustering primario**: las cadenas de buckets ocupados
crecen y se fusionan, formando clusters cada vez más grandes que
degradan las búsquedas.

```
Clustering primario:

  [_, X, X, X, X, _, _, X, X, X, _, _]
      |-cluster A-|     |-cluster B-|

Si insertar en 5: el cluster A crece a [1..5]
Si insertar en 6: los clusters A y B se FUSIONAN: [1..9]
  → un cluster de 9 buckets, busqueda promedio ~5 sondeos
```

Detallado en **S02/T02**.

### Sonda cuadrática

Secuencia: $h(k), h(k)+1^2, h(k)+2^2, h(k)+3^2, \ldots$

```
h(k) = 7, m = 16:
  probar 7, 8, 11, 0, 7 (cicla!)
  mejor: 7, 8(+1), 11(+3), 0(+5), 7(+7)... depende de la formula
```

Elimina el clustering primario pero introduce **clustering
secundario**: claves que hashean al mismo bucket siguen la misma
secuencia de sondeo. Requiere que $m$ sea primo o potencia de 2
para garantizar que se visiten todos los buckets.

Detallado en **S02/T03**.

### Doble hashing

Secuencia: $h_1(k), h_1(k)+h_2(k), h_1(k)+2 \cdot h_2(k), \ldots$

Usa una **segunda función hash** $h_2(k)$ para determinar el paso.
Elimina tanto el clustering primario como el secundario (claves
con el mismo $h_1$ pero diferente $h_2$ siguen secuencias
diferentes).

Requisito: $h_2(k)$ debe ser coprimo con $m$ para garantizar
que se visiten todos los buckets. Si $m$ es primo, basta que
$h_2(k) \neq 0$.

Detallado en **S02/T03**.

---

## Comparación de estrategias

### Número de sondeos/comparaciones esperados

Para búsqueda **fallida** con hash uniforme:

| $\alpha$ | Chaining | Lineal | Cuadrática/Doble |
|---------:|:--------:|:------:|:-------:|
| 0.50 | 0.50 | 2.50 | 2.00 |
| 0.70 | 0.70 | 6.06 | 3.33 |
| 0.80 | 0.80 | 13.0 | 5.00 |
| 0.90 | 0.90 | 50.5 | 10.0 |

Chaining siempre gana en **número de comparaciones** porque su
costo es simplemente $\alpha$, independiente de clustering. Pero
open addressing puede ganar en **tiempo real** por mejor localidad
de caché (menos cache misses × más sondeos puede ser menor que
más cache misses × menos comparaciones).

### Decisión práctica

| Factor | Favorece chaining | Favorece open addressing |
|--------|:-:|:-:|
| Simplicidad de implementación | ✓ | |
| Eliminación frecuente | ✓ | |
| $\alpha > 0.75$ | ✓ | |
| Localidad de caché | | ✓ |
| Memoria mínima | | ✓ |
| Elementos grandes (> 64 bytes) | ✓ | |
| Elementos pequeños (< 16 bytes) | | ✓ |

---

## Colisiones patológicas: ataques HashDoS

### El problema

Si un atacante conoce la función hash, puede fabricar claves que
**todas colisionen** en el mismo bucket. Con $n$ claves en un solo
bucket, cada búsqueda cuesta $O(n)$.

```
Atacante envía n requests con claves k1, k2, ..., kn
tales que h(k1) = h(k2) = ... = h(kn)

tabla[42]: k1 → k2 → k3 → ... → kn  (lista de longitud n)
(todos los demas buckets vacios)

Cada busqueda: O(n) en vez de O(1)
Con n = 100,000: la "O(1) hash table" se convierte en O(100000)
```

### Ejemplo real: ataque a servidores web (2011)

En 2011, Alexander Klink y Julian Wälde demostraron ataques
HashDoS contra múltiples lenguajes (PHP, Java, Python, Ruby,
ASP.NET). Un único POST con ~100 KB de datos podía generar
~100,000 colisiones y consumir minutos de CPU del servidor.

### Defensas

1. **Hash aleatorizado**: usar una semilla aleatoria por instancia
   (SipHash en Rust/Python, seed en Java desde 8).
2. **Limitar tamaño de POST/input**: no aceptar inputs
   arbitrariamente grandes.
3. **Convertir a árbol**: Java 8+ convierte listas en buckets a
   árboles balanceados cuando la longitud supera 8 — peor caso
   $O(\log n)$ en vez de $O(n)$.

La defensa (1) es la más importante y es el motivo por el que Rust
usa SipHash por defecto en `HashMap`. Tema cubierto en detalle en
**S03/T04**.

---

## Colisiones vs hash perfecto

En el extremo opuesto: si se conocen todas las claves de antemano
(conjunto estático), es posible construir una función hash que
**no produzca colisiones** — un **hash perfecto**.

```
Claves conocidas: {"lunes", "martes", ..., "domingo"}
Hash perfecto: h("lunes")=0, h("martes")=1, ..., h("domingo")=6
Sin colisiones → busqueda exactamente O(1) sin probing
```

Los hashes perfectos se cubren en **S03/T04**. Son útiles para
keywords de compiladores, conjuntos de comandos fijos, y tablas
de lookup estáticas.

---

## Programa completo en C

```c
// collisions.c — collision visualization and analysis
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define ALPHA_SIZE 26

// --- hash function ---

unsigned int djb2(const char *s) {
    unsigned int h = 5381;
    while (*s) h = h * 33 + (unsigned char)*s++;
    return h;
}

// --- demo helpers ---

void generate_word(char *buf, int len, unsigned int seed) {
    for (int i = 0; i < len; i++) {
        seed = seed * 1103515245 + 12345;
        buf[i] = 'a' + (seed >> 16) % 26;
    }
    buf[len] = '\0';
}

// --- main ---

int main(void) {
    srand(42);

    // demo 1: birthday paradox simulation
    printf("=== Demo 1: birthday paradox — first collision ===\n");
    int trials = 1000;

    int test_sizes[] = {100, 1000, 10000, 100000};
    for (int t = 0; t < 4; t++) {
        int m = test_sizes[t];
        long long total_first = 0;

        for (int trial = 0; trial < trials; trial++) {
            int *seen = calloc(m, sizeof(int));
            int first_collision = -1;
            unsigned int seed = trial * 7919 + 1;

            for (int i = 1; i <= m; i++) {
                seed = seed * 2654435769U + 1;
                int bucket = seed % m;
                if (seen[bucket]) {
                    first_collision = i;
                    break;
                }
                seen[bucket] = 1;
            }

            total_first += first_collision;
            free(seen);
        }

        double avg = (double)total_first / trials;
        double expected = 1.25 * sqrt((double)m);
        printf("  m=%6d: avg first collision at n=%.1f  (theory ~%.1f)\n",
               m, avg, expected);
    }
    printf("\n");

    // demo 2: collision probability
    printf("=== Demo 2: collision probability vs n ===\n");
    int m = 365;  // birthday problem
    printf("  m = %d (birthday problem)\n", m);
    printf("  %4s  %10s  %10s\n", "n", "P(collision)", "P(theory)");

    for (int n = 5; n <= 60; n += 5) {
        // simulate
        int collisions = 0;
        int sim_trials = 10000;
        for (int trial = 0; trial < sim_trials; trial++) {
            int *buckets = calloc(m, sizeof(int));
            int collision = 0;
            unsigned int seed = trial * 31 + n;
            for (int i = 0; i < n; i++) {
                seed = seed * 2654435769U + 1;
                int b = seed % m;
                if (buckets[b]) { collision = 1; break; }
                buckets[b] = 1;
            }
            if (collision) collisions++;
            free(buckets);
        }

        // theory
        double p_no = 1.0;
        for (int i = 0; i < n; i++)
            p_no *= (double)(m - i) / m;
        double p_theory = 1.0 - p_no;

        printf("  %4d  %10.4f  %10.4f\n",
               n, (double)collisions / sim_trials, p_theory);
    }
    printf("\n");

    // demo 3: visualize collisions with different m
    printf("=== Demo 3: collision distribution ===\n");
    int n_words = 50;
    char words[50][16];
    for (int i = 0; i < n_words; i++)
        generate_word(words[i], 6 + i % 5, i * 997 + 42);

    int sizes[] = {10, 20, 50, 100};
    for (int s = 0; s < 4; s++) {
        int sz = sizes[s];
        int *counts = calloc(sz, sizeof(int));

        for (int i = 0; i < n_words; i++)
            counts[djb2(words[i]) % sz]++;

        int max_c = 0, collisions = 0, empty = 0;
        for (int i = 0; i < sz; i++) {
            if (counts[i] > max_c) max_c = counts[i];
            if (counts[i] > 1) collisions += counts[i] - 1;
            if (counts[i] == 0) empty++;
        }

        printf("  m=%3d, n=%d: collisions=%d, max_chain=%d, empty=%d/%d\n",
               sz, n_words, collisions, max_c, empty, sz);

        // show histogram for small m
        if (sz <= 20) {
            for (int i = 0; i < sz; i++) {
                printf("    [%2d]: ", i);
                for (int j = 0; j < counts[i]; j++) printf("#");
                if (counts[i] == 0) printf("-");
                printf("\n");
            }
        }
        free(counts);
        printf("\n");
    }

    // demo 4: worst case — all same hash
    printf("=== Demo 4: worst case — forced collisions ===\n");
    printf("  If all n keys hash to same bucket:\n");
    for (int n = 10; n <= 10000; n *= 10) {
        printf("    n=%5d: search = O(%d) — like a linked list!\n", n, n);
    }
    printf("  This is why hash function quality matters.\n\n");

    // demo 5: chaining vs open addressing sketch
    printf("=== Demo 5: strategy comparison sketch ===\n");
    int sketch_m = 11;
    int keys[] = {10, 22, 31, 4, 15, 28, 17, 88, 59};
    int n_keys = 9;

    // chaining
    printf("  Chaining (m=%d):\n", sketch_m);
    {
        int chain_counts[11] = {0};
        for (int i = 0; i < n_keys; i++) {
            int h = keys[i] % sketch_m;
            chain_counts[h]++;
        }
        for (int i = 0; i < sketch_m; i++) {
            printf("    [%2d]: ", i);
            int printed = 0;
            for (int j = 0; j < n_keys; j++) {
                if (keys[j] % sketch_m == i) {
                    if (printed) printf(" -> ");
                    printf("%d", keys[j]);
                    printed++;
                }
            }
            if (!printed) printf("(empty)");
            printf("\n");
        }
    }

    // linear probing
    printf("\n  Linear probing (m=%d):\n", sketch_m);
    {
        int table[11];
        int occ[11] = {0};
        for (int i = 0; i < n_keys; i++) {
            int h = keys[i] % sketch_m;
            int probes = 0;
            while (occ[h]) {
                h = (h + 1) % sketch_m;
                probes++;
            }
            table[h] = keys[i];
            occ[h] = 1;
            if (probes > 0)
                printf("    insert %2d: h=%d, %d probe(s) -> slot %d\n",
                       keys[i], keys[i] % sketch_m, probes, h);
            else
                printf("    insert %2d: h=%d -> slot %d\n",
                       keys[i], h, h);
        }
        printf("    Final table: [");
        for (int i = 0; i < sketch_m; i++) {
            if (occ[i]) printf("%d", table[i]);
            else printf("_");
            if (i < sketch_m - 1) printf(", ");
        }
        printf("]\n");
    }
    printf("\n");

    // demo 6: collision count growth
    printf("=== Demo 6: collisions grow with n ===\n");
    printf("  %8s  %6s  %10s  %8s\n", "n", "m", "collisions", "alpha");
    int fixed_m = 1000;

    for (int n = 100; n <= 5000; n += 100) {
        int *counts = calloc(fixed_m, sizeof(int));
        unsigned int seed = 12345;
        for (int i = 0; i < n; i++) {
            seed = seed * 2654435769U + 1;
            counts[seed % fixed_m]++;
        }
        int col = 0;
        for (int i = 0; i < fixed_m; i++)
            if (counts[i] > 1) col += counts[i] - 1;
        free(counts);

        if (n % 500 == 0 || n == 100)
            printf("  %8d  %6d  %10d  %8.2f\n",
                   n, fixed_m, col, (double)n / fixed_m);
    }

    return 0;
}
```

Compilar y ejecutar:

```bash
gcc -std=c11 -Wall -Wextra -O2 -o collisions collisions.c -lm
./collisions
```

---

## Programa completo en Rust

```rust
// collisions.rs — collision analysis, birthday paradox, strategy comparison
use std::collections::HashMap;

fn djb2(s: &str) -> u32 {
    let mut h: u32 = 5381;
    for b in s.bytes() {
        h = h.wrapping_mul(33).wrapping_add(b as u32);
    }
    h
}

fn main() {
    // demo 1: birthday paradox simulation
    println!("=== Demo 1: birthday paradox — first collision ===");
    let trials = 1000;

    for &m in &[100u64, 1000, 10000, 100000] {
        let mut total: u64 = 0;
        for trial in 0..trials {
            let mut seen = vec![false; m as usize];
            let mut seed: u64 = trial * 7919 + 1;
            for i in 1..=m {
                seed = seed.wrapping_mul(2654435769).wrapping_add(1);
                let bucket = (seed % m) as usize;
                if seen[bucket] {
                    total += i;
                    break;
                }
                seen[bucket] = true;
            }
        }
        let avg = total as f64 / trials as f64;
        let expected = 1.25 * (m as f64).sqrt();
        println!("  m={:>6}: avg first collision at n={:.1}  (theory ~{:.1})",
                 m, avg, expected);
    }
    println!();

    // demo 2: collision probability (birthday problem)
    println!("=== Demo 2: collision probability (m=365) ===");
    let m = 365.0f64;
    println!("  {:>4}  {:>12}", "n", "P(collision)");
    for n in (5..=60).step_by(5) {
        let mut p_no = 1.0;
        for i in 0..n {
            p_no *= (m - i as f64) / m;
        }
        println!("  {:>4}  {:>12.4}", n, 1.0 - p_no);
    }
    println!("  50% at n=23, 99% at n~57\n");

    // demo 3: collision distribution
    println!("=== Demo 3: collision count for n=50 words ===");
    let words: Vec<String> = (0..50).map(|i| {
        let mut w = String::new();
        let mut seed = i * 997u64 + 42;
        for _ in 0..(6 + i % 5) {
            seed = seed.wrapping_mul(6364136223846793005).wrapping_add(1);
            w.push((b'a' + (seed >> 16) as u8 % 26) as char);
        }
        w
    }).collect();

    for &table_size in &[10usize, 20, 50, 100] {
        let mut counts = vec![0u32; table_size];
        for w in &words {
            counts[djb2(w) as usize % table_size] += 1;
        }
        let max_c = *counts.iter().max().unwrap();
        let collisions: u32 = counts.iter().filter(|&&c| c > 1).map(|c| c - 1).sum();
        let empty = counts.iter().filter(|&&c| c == 0).count();

        println!("  m={:>3}: collisions={}, max_chain={}, empty={}/{}",
                 table_size, collisions, max_c, empty, table_size);

        if table_size <= 20 {
            for (i, &c) in counts.iter().enumerate() {
                let bar: String = (0..c).map(|_| '#').collect();
                println!("    [{:>2}]: {}", i, if c == 0 { "-".to_string() } else { bar });
            }
        }
        println!();
    }

    // demo 4: chaining vs linear probing side by side
    println!("=== Demo 4: chaining vs linear probing ===");
    let m = 11usize;
    let keys = [10, 22, 31, 4, 15, 28, 17, 88, 59];

    // chaining
    println!("  Chaining (m={}):", m);
    let mut chain: Vec<Vec<i32>> = vec![vec![]; m];
    for &k in &keys {
        chain[k as usize % m].push(k);
    }
    for (i, bucket) in chain.iter().enumerate() {
        if bucket.is_empty() {
            println!("    [{:>2}]: (empty)", i);
        } else {
            let s: Vec<String> = bucket.iter().map(|k| k.to_string()).collect();
            println!("    [{:>2}]: {}", i, s.join(" -> "));
        }
    }

    // linear probing
    println!("\n  Linear probing (m={}):", m);
    let mut table: Vec<Option<i32>> = vec![None; m];
    for &k in &keys {
        let mut h = k as usize % m;
        let mut probes = 0;
        while table[h].is_some() {
            h = (h + 1) % m;
            probes += 1;
        }
        table[h] = Some(k);
        if probes > 0 {
            println!("    insert {:>2}: h={}, {} probe(s) -> slot {}",
                     k, k as usize % m, probes, h);
        } else {
            println!("    insert {:>2}: h={} -> slot {}", k, h, h);
        }
    }
    print!("    Final: [");
    for (i, slot) in table.iter().enumerate() {
        match slot {
            Some(k) => print!("{}", k),
            None => print!("_"),
        }
        if i < m - 1 { print!(", "); }
    }
    println!("]\n");

    // demo 5: HashMap handles collisions automatically
    println!("=== Demo 5: Rust HashMap — collisions are transparent ===");
    let mut map: HashMap<String, i32> = HashMap::new();
    for (i, w) in words.iter().enumerate() {
        map.insert(w.clone(), i as i32);
    }

    println!("  Inserted {} words into HashMap", map.len());
    println!("  capacity: {}", map.capacity());
    println!("  alpha: {:.3}", map.len() as f64 / map.capacity() as f64);

    // all searches work in O(1) regardless of collisions
    let mut found = 0;
    for w in &words {
        if map.contains_key(w) { found += 1; }
    }
    println!("  All {} words found correctly", found);
    println!("  (HashMap uses Swiss Table — handles collisions internally)\n");

    // demo 6: collision count grows with n
    println!("=== Demo 6: collisions grow with n ===");
    println!("  {:>6}  {:>6}  {:>10}  {:>6}", "n", "m", "collisions", "alpha");
    let fixed_m = 1000usize;
    for n in (500..=5000).step_by(500) {
        let mut counts = vec![0u32; fixed_m];
        let mut seed: u64 = 12345;
        for _ in 0..n {
            seed = seed.wrapping_mul(2654435769).wrapping_add(1);
            counts[(seed as usize) % fixed_m] += 1;
        }
        let col: u32 = counts.iter().filter(|&&c| c > 1).map(|c| c - 1).sum();
        println!("  {:>6}  {:>6}  {:>10}  {:>6.2}", n, fixed_m, col,
                 n as f64 / fixed_m as f64);
    }
    println!();

    // demo 7: clustering visualization (linear probing)
    println!("=== Demo 7: clustering in linear probing ===");
    let m_vis = 30;
    let mut slots: Vec<bool> = vec![false; m_vis];
    let mut seed: u64 = 42;

    for step in 1..=20 {
        seed = seed.wrapping_mul(6364136223846793005).wrapping_add(1);
        let mut h = (seed as usize) % m_vis;
        while slots[h] {
            h = (h + 1) % m_vis;
        }
        slots[h] = true;

        if step % 5 == 0 || step <= 3 {
            let vis: String = slots.iter()
                .map(|&b| if b { '#' } else { '.' })
                .collect();
            // count clusters
            let mut clusters = 0;
            let mut in_cluster = false;
            for &b in &slots {
                if b && !in_cluster {
                    clusters += 1;
                    in_cluster = true;
                } else if !b {
                    in_cluster = false;
                }
            }
            let max_cluster = {
                let mut max = 0;
                let mut cur = 0;
                for &b in &slots {
                    if b { cur += 1; if cur > max { max = cur; } }
                    else { cur = 0; }
                }
                max
            };
            println!("  n={:>2}: {} (clusters={}, max_cluster={})",
                     step, vis, clusters, max_cluster);
        }
    }
    println!("  Clusters grow and merge as alpha increases");
}
```

Compilar y ejecutar:

```bash
rustc collisions.rs && ./collisions
```

---

## Errores frecuentes

1. **Creer que una "buena" función hash evita colisiones**: ninguna
   función hash elimina colisiones. La calidad de la función
   determina la **distribución** (uniformidad), no la ausencia de
   colisiones.
2. **Ignorar el birthday paradox**: con $m = 10000$ buckets, la
   primera colisión ocurre con ~125 elementos, no con ~5000.
   Subestimar las colisiones lleva a elegir $m$ demasiado pequeño.
3. **No manejar colisiones**: almacenar solo un elemento por bucket
   sin chaining ni probing sobreescribe silenciosamente el elemento
   anterior.
4. **Mezclar estrategias**: intentar usar chaining en algunos
   buckets y probing en otros. Elegir una estrategia y aplicarla
   uniformemente.
5. **Confundir colisión de hash con colisión de bucket**: dos claves
   pueden tener el mismo hash (colisión de hash) pero caer en
   buckets diferentes si $h(k_1) \bmod m \neq h(k_2) \bmod m$.
   Lo que importa es la colisión de bucket.

---

## Ejercicios

### Ejercicio 1 — Birthday paradox experimental

Para $m = 1000$, simula 1000 experimentos: inserta claves
aleatorias hasta la primera colisión. Reporta el promedio y la
desviación estándar de $n$ en la primera colisión. Compara con
la fórmula teórica $1.25\sqrt{m} \approx 39.5$.

<details><summary>Predice antes de ejecutar</summary>

¿El promedio experimental estará dentro del 10% del teórico?
¿La desviación estándar será grande o pequeña?

</details>

### Ejercicio 2 — Colisiones con hash mala vs buena

Inserta 100 strings (anagramas de "abcde" + permutaciones) en una
tabla de $m = 53$ usando `hash_sum` y `hash_djb2`. Cuenta las
colisiones de cada una.

<details><summary>Predice antes de ejecutar</summary>

¿Cuántas colisiones produce `hash_sum` para 100 anagramas?
Pista: todos los anagramas producen el mismo hash con `hash_sum`.
¿Y con `hash_djb2`?

</details>

### Ejercicio 3 — Visualizar clusters

Implementa sonda lineal con $m = 40$ y visualiza la tabla
(# = ocupado, . = vacío) después de insertar 10, 20 y 30 claves.
Identifica y cuenta los clusters.

<details><summary>Predice antes de ejecutar</summary>

¿Con 30 claves ($\alpha = 0.75$), cuántos clusters esperarías?
¿Cuál será el cluster más largo?

</details>

### Ejercicio 4 — Colisiones por crecimiento

Para una tabla de chaining con $m = 100$ (fijo, sin resize),
inserta de 1 a 1000 claves y registra el número total de
colisiones en cada punto. Grafica colisiones vs $n$.

<details><summary>Predice antes de ejecutar</summary>

¿La curva es lineal, cuadrática o exponencial? ¿En qué punto
las colisiones superan el 50% de las inserciones?

</details>

### Ejercicio 5 — Chaining vs linear probing: mismas claves

Inserta las claves $\{10, 22, 31, 4, 15, 28, 17, 88, 59\}$ en
tablas de $m = 11$ con chaining y con sonda lineal. Dibuja ambas
tablas y cuenta los sondeos/comparaciones para buscar cada clave.

<details><summary>Predice antes de ejecutar</summary>

¿Cuántos sondeos necesita la sonda lineal para buscar la última
clave insertada? ¿Y chaining?

</details>

### Ejercicio 6 — Probabilidad de colisión

Calcula la probabilidad de al menos una colisión con $n = 10$
claves en tablas de $m = 50, 100, 500, 1000$. Usa la fórmula
exacta y verifica con simulación.

<details><summary>Predice antes de ejecutar</summary>

¿Con $m = 50$ y $n = 10$, la probabilidad es ~20%, ~60% o ~90%?

</details>

### Ejercicio 7 — Max chain length

Para $m = 1000$ y $\alpha = 1.0$ (1000 claves aleatorias con
chaining), mide la longitud máxima de cadena en 100 experimentos.
Reporta promedio y máximo del máximo.

<details><summary>Predice antes de ejecutar</summary>

La teoría dice que la cadena más larga es
$\Theta(\log n / \log \log n) \approx 4.3$ para $n = 1000$.
¿Tu medición lo confirma?

</details>

### Ejercicio 8 — Colisiones de 32 bits

Genera $2^{16} = 65536$ strings aleatorios y calcula su hash
de 32 bits (djb2). ¿Cuántas colisiones hay? Compara con el
birthday paradox teórico para $m = 2^{32}$.

<details><summary>Predice antes de ejecutar</summary>

$1.25\sqrt{2^{32}} \approx 81920$, así que con 65536 claves
deberías estar cerca del umbral. ¿La probabilidad de colisión
será ~30%, ~50% o ~90%?

</details>

### Ejercicio 9 — Estrategia híbrida

Implementa una tabla hash donde cada bucket usa un **array de
capacidad fija** (e.g., 4 slots inline) antes de recurrir a una
lista enlazada para overflow. Compara el rendimiento con chaining
puro para $\alpha = 0.5, 1.0, 2.0$.

<details><summary>Predice antes de ejecutar</summary>

¿A qué $\alpha$ la estrategia híbrida deja de ser ventajosa?
¿El ahorro en cache misses compensa el desperdicio de slots vacíos?

</details>

### Ejercicio 10 — HashDoS simulado

Genera $n$ claves que todas hasheen al mismo bucket (por fuerza
bruta: generar claves hasta encontrar una con el hash deseado).
Mide el tiempo de $n$ búsquedas para $n = 100, 1000, 10000$ y
verifica que crece como $O(n^2)$ (total).

<details><summary>Predice antes de ejecutar</summary>

¿Cuántas claves aleatorias necesitarás probar para encontrar
100 que colisionen en el mismo bucket de $m = 1000$? Pista:
en promedio, 1 de cada $m$ claves cae en el bucket objetivo.

</details>
