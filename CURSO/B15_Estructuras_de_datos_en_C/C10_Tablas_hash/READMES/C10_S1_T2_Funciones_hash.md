# Funciones hash

## Objetivo

Estudiar las funciones hash más utilizadas en la práctica, sus
propiedades formales, y cómo elegir o implementar una según el
tipo de clave:

- **Método de la división** (`key % m`) y elección de $m$.
- **Método de la multiplicación** (Knuth): $\lfloor m \cdot \text{frac}(k \cdot A) \rfloor$.
- **Hash para strings**: djb2, FNV-1a, hash polinomial.
- **Propiedades formales**: uniformidad, determinismo, eficiencia,
  avalancha.
- Cómo Rust implementa hashing (`Hash` trait, SipHash, `DefaultHasher`).
- Referencia: T01 introdujo el concepto general y la función
  polinomial básica. Aquí se profundiza.

---

## Propiedades de una buena función hash

### 1. Determinismo

$$k_1 = k_2 \implies h(k_1) = h(k_2)$$

Si la misma clave produce hashes diferentes en distintas llamadas,
la tabla se corrompe (se inserta en un bucket y se busca en otro).

Esto parece trivial, pero hay trampas:
- En Python, `hash("hello")` cambia entre ejecuciones (hash
  randomization para seguridad). La tabla funciona dentro de una
  ejecución, pero no se puede persistir el hash.
- En Rust, `DefaultHasher` usa SipHash con semillas aleatorias por
  instancia de `RandomState`. Dos `HashMap` distintos pueden dar
  hashes diferentes para la misma clave.

### 2. Uniformidad

Idealmente, para claves aleatorias uniformes:

$$P[h(k) = i] = \frac{1}{m} \quad \forall i \in [0, m)$$

En la práctica, las claves **no** son aleatorias (nombres,
IDs secuenciales, URLs con patrones). Una buena función hash
convierte patrones en las claves en distribuciones uniformes
en los buckets.

### 3. Eficiencia

El hash se calcula en **cada** operación (inserción, búsqueda,
eliminación). Si $h(k)$ es lento, domina el costo total. Para
claves de tamaño fijo (int, puntero): $O(1)$. Para claves de
longitud $m$ (strings): $O(m)$ es óptimo (hay que leer toda la
clave).

### 4. Efecto avalancha

Un cambio de **un bit** en la clave debería cambiar
aproximadamente **la mitad** de los bits del hash. Sin esto,
claves similares caen en buckets cercanos, creando clusters.

```
Buena avalancha:
  h("abc") = 0x7C3E9A1F
  h("abd") = 0xE1A54B82  (completamente diferente)

Mala avalancha:
  h("abc") = 0x00000150
  h("abd") = 0x00000151  (casi igual — clustering)
```

---

## Método de la división

$$h(k) = k \bmod m$$

El más simple. Ya introducido en T01, aquí profundizamos en la
**elección de** $m$.

### Valores de $m$ problemáticos

**$m$ potencia de 2** ($m = 2^p$):

$k \bmod 2^p$ es equivalente a tomar los $p$ bits menos
significativos de $k$. Los bits superiores se ignoran
completamente:

```
k = 0b 1010 1100 0011 0110
                  ^^^^^^^^
m = 256 (2^8):    h = 0x36 (solo ultimos 8 bits)
```

Si las claves tienen patrones en los bits bajos (e.g., direcciones
de memoria alineadas a 4/8 bytes, donde los bits 0-2 son siempre
0), la distribución es terrible.

**$m$ potencia de 10** ($m = 100, 1000, ...$):

Similar problema: solo importan los últimos dígitos decimales.
Claves como años (2020, 2021, ...) o IDs secuenciales (1000, 2000,
...) colisionan masivamente.

**$m$ con factores comunes con las claves**:

Si las claves son múltiplos de $d$ y $\gcd(d, m) > 1$, solo una
fracción de los buckets se usa.

```
Claves: {0, 6, 12, 18, 24, 30}  (multiplos de 6)
m = 12:  h = {0, 6, 0, 6, 0, 6}   (solo 2 buckets!)
m = 13:  h = {0, 6, 12, 5, 11, 4} (6 buckets, excelente)
```

### Valores de $m$ recomendados

**$m$ primo**, especialmente lejos de potencias de 2:

| Rango | Buenos primos |
|-------|--------------|
| ~100 | 97, 101, 103 |
| ~1000 | 997, 1009, 1013 |
| ~10000 | 9973, 10007, 10009 |
| ~100000 | 99991, 100003 |

¿Por qué primos? Si $m$ es primo, $k \bmod m$ distribuye las
claves uniformemente incluso si las claves tienen estructura
(múltiplos de algún número $d$, siempre que $d \neq m$).

### Implementación en C

```c
unsigned int hash_div(int key, int m) {
    // manejar negativos: en C, (-7) % 10 puede ser -7
    return ((key % m) + m) % m;
}
```

Alternativa más eficiente para claves `unsigned`:

```c
unsigned int hash_div_unsigned(unsigned int key, unsigned int m) {
    return key % m;
}
```

---

## Método de la multiplicación (Knuth)

$$h(k) = \lfloor m \cdot \text{frac}(k \cdot A) \rfloor$$

donde $\text{frac}(x) = x - \lfloor x \rfloor$ es la parte
fraccionaria, y $A$ es una constante en $(0, 1)$.

### Pasos

1. Multiplicar $k$ por $A$: resultado es un real con parte entera
   y fraccionaria.
2. Tomar solo la **parte fraccionaria** (entre 0 y 1).
3. Multiplicar por $m$ para obtener un índice en $[0, m)$.
4. Tomar la parte entera del resultado.

### Ejemplo paso a paso

Con $A = 0.6180339887$ (proporción áurea $\phi - 1$), $m = 16$,
$k = 123456$:

```
Paso 1: k * A = 123456 * 0.6180339887 = 76300.0041...
Paso 2: frac(76300.0041) = 0.0041...
Paso 3: m * 0.0041... = 16 * 0.0041... = 0.0662...
Paso 4: floor(0.0662) = 0

h(123456) = 0
```

Con $k = 123457$:

```
Paso 1: k * A = 123457 * 0.6180339887 = 76300.6221...
Paso 2: frac(76300.6221) = 0.6221...
Paso 3: m * 0.6221... = 16 * 0.6221... = 9.954...
Paso 4: floor(9.954) = 9

h(123457) = 9
```

Claves consecutivas (123456, 123457) caen en buckets muy
diferentes (0 y 9) — buena distribución.

### ¿Por qué la proporción áurea?

Knuth demostró que $A = \frac{\sqrt{5} - 1}{2} \approx 0.6180339887$
produce la distribución más uniforme. La razón es que $\phi$ es el
"número más irracional" — su expansión en fracciones continuas
$[0; 1, 1, 1, \ldots]$ converge lo más lentamente posible, lo que
evita clustering.

Cualquier irracional funciona razonablemente bien, pero $\phi - 1$
es óptimo.

### Ventaja sobre la división

El valor de $m$ **no importa** — funciona bien con cualquier $m$,
incluyendo potencias de 2. Esto es una ventaja significativa porque
$m = 2^p$ permite reemplazar la multiplicación por $m$ con un shift.

### Implementación con aritmética entera

Para evitar punto flotante, se usa la representación en punto fijo
con un entero de 32 o 64 bits:

```c
unsigned int hash_mult(unsigned int key, unsigned int m) {
    // A ≈ 0.6180339887, representado como 2654435769 / 2^32
    // 2654435769 = floor(2^32 * (sqrt(5) - 1) / 2)
    unsigned int hash = key * 2654435769U;
    // los bits altos de hash contienen la parte fraccionaria escalada
    return hash >> (32 - __builtin_ctz(m));
    // solo funciona si m es potencia de 2
}

// version general (m no necesita ser potencia de 2)
unsigned int hash_mult_general(unsigned int key, unsigned int m) {
    unsigned long long hash = (unsigned long long)key * 2654435769ULL;
    return (unsigned int)((hash >> 32) * m >> 32);
}
```

La constante `2654435769` es $\lfloor 2^{32} \cdot (\sqrt{5}-1)/2 \rfloor$.

### Tabla comparativa: división vs multiplicación

| Aspecto | División | Multiplicación |
|---------|:--------:|:--------------:|
| Fórmula | $k \bmod m$ | $\lfloor m \cdot \text{frac}(kA) \rfloor$ |
| Sensible a $m$ | sí ($m$ primo recomendado) | no (cualquier $m$) |
| Velocidad (HW) | `div`/`mod` (~20-40 ciclos) | `mul` + shift (~3-5 ciclos) |
| Distribución | buena si $m$ primo | buena siempre |
| Simplicidad | muy alta | media |
| Uso práctico | enseñanza, tablas pequeñas | implementaciones reales |

---

## Hash para strings

Las claves tipo string requieren combinar todos los caracteres en
un único valor hash. Hay varias funciones clásicas.

### Hash polinomial (revisión de T01)

$$h(s) = \left(\sum_{i=0}^{n-1} s[i] \cdot b^{n-1-i}\right) \bmod m$$

Usando la **regla de Horner** para evaluación eficiente:

$$h = (\cdots((s[0] \cdot b + s[1]) \cdot b + s[2]) \cdot b + \cdots + s[n-1]) \bmod m$$

```c
unsigned int hash_poly(const char *s, int m) {
    unsigned int h = 0;
    for (int i = 0; s[i]; i++)
        h = h * 31 + (unsigned char)s[i];
    return h % m;
}
```

Bases comunes: 31 (Java `String.hashCode()`), 37, 65599.
El overflow de `unsigned int` actúa como módulo $2^{32}$ implícito,
seguido del módulo $m$ final.

### djb2 (Bernstein)

```c
unsigned int hash_djb2(const char *s) {
    unsigned int h = 5381;
    int c;
    while ((c = *s++))
        h = h * 33 + c;    // equivalente a ((h << 5) + h) + c
    return h;
}
```

- Creada por Daniel J. Bernstein.
- Semilla: 5381 (primo con buenas propiedades empíricas).
- Multiplicador: 33 ($2^5 + 1$, optimizable a shift + add).
- Simple, rápida, buena distribución para strings cortos.
- **No es criptográfica** — no usar para seguridad.

¿Por qué 5381? No hay prueba formal; Bernstein lo encontró
empíricamente como un valor que produce buena distribución para
diccionarios de palabras inglesas.

### FNV-1a (Fowler-Noll-Vo)

```c
unsigned int hash_fnv1a(const char *s) {
    unsigned int h = 2166136261U;   // FNV offset basis (32-bit)
    while (*s) {
        h ^= (unsigned char)*s++;   // XOR byte
        h *= 16777619U;             // FNV prime (32-bit)
    }
    return h;
}
```

Existe también en versión 64-bit:

```c
unsigned long long hash_fnv1a_64(const char *s) {
    unsigned long long h = 14695981039346656037ULL;
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 1099511628211ULL;
    }
    return h;
}
```

Características:
- **XOR antes de multiplicar** (FNV-1a) produce mejor avalancha
  que multiplicar antes de XOR (FNV-1).
- Constantes elegidas para maximizar dispersión en 32 y 64 bits.
- Buen balance velocidad/distribución.
- Usada en muchos sistemas (DNS, compiladores, detectores de
  duplicados).

### Comparación de hash functions para strings

| Función | Operación por byte | Calidad | Velocidad | Notas |
|---------|:--------:|:-------:|:---------:|-------|
| Suma | `h += c` | mala (anagramas) | muy rápida | nunca usar |
| Polinomial (31) | `h = h*31 + c` | buena | rápida | Java String |
| djb2 | `h = h*33 + c` | buena | rápida | semilla 5381 |
| FNV-1a | `h ^= c; h *= prime` | muy buena | rápida | buena avalancha |
| SipHash | compresión por bloque | excelente | media | segura contra DoS |

### Traza comparativa

Hash de `"hello"` (caracteres: 104, 101, 108, 108, 111):

```
Polinomial (base=31):
  h = 0
  h = 0 * 31 + 104 = 104
  h = 104 * 31 + 101 = 3325
  h = 3325 * 31 + 108 = 103183
  h = 103183 * 31 + 108 = 3198781
  h = 3198781 * 31 + 111 = 99162322
  Resultado: 99162322

djb2:
  h = 5381
  h = 5381 * 33 + 104 = 177777
  h = 177777 * 33 + 101 = 5866742
  h = 5866742 * 33 + 108 = 193602594
  h = 193602594 * 33 + 108 = 2093572702 (overflow u32: wraps)
  h = wrapped * 33 + 111 = ...
  Resultado: 261238937 (after overflow)

FNV-1a (32-bit):
  h = 2166136261
  h = (2166136261 ^ 104) * 16777619 = ...
  (cada paso: XOR con byte, luego multiplicar por primo)
  Resultado: 1335831723 (approximately)
```

---

## Hash para datos binarios arbitrarios

Para estructuras, buffers, o claves multi-campo, se puede
generalizar FNV-1a para procesar bytes arbitrarios:

```c
unsigned int hash_bytes(const void *data, size_t len) {
    const unsigned char *p = data;
    unsigned int h = 2166136261U;
    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 16777619U;
    }
    return h;
}
```

Uso con structs:

```c
typedef struct {
    int x, y;
} Point;

unsigned int hash_point(Point p) {
    return hash_bytes(&p, sizeof(p));
}
```

**Cuidado**: `sizeof(struct)` puede incluir padding bytes que no
están inicializados. Para structs con padding, hashear campo por
campo:

```c
unsigned int hash_point_safe(Point p) {
    unsigned int h = 2166136261U;
    // hash x
    const unsigned char *px = (const unsigned char *)&p.x;
    for (size_t i = 0; i < sizeof(p.x); i++) {
        h ^= px[i];
        h *= 16777619U;
    }
    // hash y
    const unsigned char *py = (const unsigned char *)&p.y;
    for (size_t i = 0; i < sizeof(p.y); i++) {
        h ^= py[i];
        h *= 16777619U;
    }
    return h;
}
```

---

## Hashing en Rust

### El trait `Hash`

En Rust, cualquier tipo que implemente `Hash` puede usarse como
clave de `HashMap`:

```rust
use std::hash::{Hash, Hasher};

struct Point {
    x: i32,
    y: i32,
}

impl Hash for Point {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.x.hash(state);
        self.y.hash(state);
    }
}
```

El trait `Hasher` es la interfaz del algoritmo de hash. El tipo
llama `state.write_*()` con sus datos, y el hasher produce el
resultado final.

### `#[derive(Hash)]`

Para la mayoría de los casos, `derive` genera la implementación
automáticamente:

```rust
#[derive(Hash, PartialEq, Eq)]
struct Point {
    x: i32,
    y: i32,
}
```

El derive hashea cada campo en orden de declaración, lo cual es
correcto si todos los campos contribuyen a la igualdad.

### Regla: `Hash` debe ser consistente con `Eq`

$$a == b \implies \text{hash}(a) == \text{hash}(b)$$

Si dos valores son iguales según `Eq`, **deben** tener el mismo
hash. Lo contrario no es necesario (colisiones permitidas). Si
esta regla se viola, la tabla se corrompe:

```rust
// MAL: inconsistencia Hash/Eq
impl Hash for BadType {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.id.hash(state);
    }
}
impl PartialEq for BadType {
    fn eq(&self, other: &Self) -> bool {
        self.name == other.name  // Eq usa name, Hash usa id!
    }
}
```

### `DefaultHasher` y SipHash

El `HashMap` de Rust usa `RandomState` como builder de hashers, que
crea instancias de `SipHash-1-3` (desde Rust 1.36; antes era
SipHash-2-4). Características:

- **Semilla aleatoria** por instancia de `RandomState`: protege
  contra ataques HashDoS (ver C10/S03/T04).
- **SipHash**: función hash criptográficamente robusta diseñada
  para hash tables. Más lenta que djb2/FNV-1a (~2-3x), pero
  resistente a ataques de colisión.
- No es configurable sin cambiar el hasher.

### Hashers alternativos

Para rendimiento cuando la seguridad no importa:

```rust
use std::collections::HashMap;
use std::hash::BuildHasherDefault;

// FxHash: usado internamente por el compilador de Rust
// (crate: rustc-hash)
// AHash: usado por hashbrown (el backend de HashMap)
// (crate: ahash)
```

| Hasher | Velocidad | Seguridad | Uso |
|--------|:---------:|:---------:|-----|
| SipHash-1-3 | media | alta (DoS-resistant) | default en HashMap |
| FxHash | muy rápida | ninguna | compilador Rust, benchmarks |
| AHash | rápida | media (keyed) | hashbrown default |

### Tipos que no implementan `Hash`

`f32` y `f64` **no** implementan `Hash` porque `NaN != NaN` viola
la regla de consistencia con `Eq` (que tampoco implementan; solo
implementan `PartialEq`).

Para usar floats como claves, se necesita un wrapper:

```rust
use std::hash::{Hash, Hasher};

#[derive(PartialEq, Eq)]
struct OrderedFloat(u64);  // bits de f64

impl OrderedFloat {
    fn new(f: f64) -> Self {
        OrderedFloat(f.to_bits())
    }
}

impl Hash for OrderedFloat {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.0.hash(state);
    }
}
```

O usar el crate `ordered-float`.

---

## Programa completo en C

```c
// hash_functions.c — comparison of hash functions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// --- hash functions for integers ---

unsigned int hash_div(unsigned int key, unsigned int m) {
    return key % m;
}

unsigned int hash_mult(unsigned int key, unsigned int m) {
    // Knuth multiplicative: A = (sqrt(5)-1)/2 ≈ 0.618...
    // represented as 2654435769 / 2^32
    unsigned long long h = (unsigned long long)key * 2654435769ULL;
    return (unsigned int)((h * m) >> 32);
}

// --- hash functions for strings ---

unsigned int hash_sum(const char *s) {
    unsigned int h = 0;
    for (int i = 0; s[i]; i++)
        h += (unsigned char)s[i];
    return h;
}

unsigned int hash_poly31(const char *s) {
    unsigned int h = 0;
    for (int i = 0; s[i]; i++)
        h = h * 31 + (unsigned char)s[i];
    return h;
}

unsigned int hash_djb2(const char *s) {
    unsigned int h = 5381;
    int c;
    while ((c = *s++))
        h = h * 33 + c;
    return h;
}

unsigned int hash_fnv1a(const char *s) {
    unsigned int h = 2166136261U;
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 16777619U;
    }
    return h;
}

// --- analysis helpers ---

// chi-squared test for uniformity
double chi_squared(int *counts, int m, int n) {
    double expected = (double)n / m;
    double chi2 = 0;
    for (int i = 0; i < m; i++) {
        double diff = counts[i] - expected;
        chi2 += diff * diff / expected;
    }
    return chi2;
}

void analyze_distribution(const char *name, unsigned int (*hash_fn)(const char *),
                          const char **words, int nwords, int m) {
    int *counts = calloc(m, sizeof(int));
    for (int i = 0; i < nwords; i++)
        counts[hash_fn(words[i]) % m]++;

    int max_count = 0, empty = 0;
    for (int i = 0; i < m; i++) {
        if (counts[i] > max_count) max_count = counts[i];
        if (counts[i] == 0) empty++;
    }

    double chi2 = chi_squared(counts, m, nwords);

    printf("  %-12s: max_bucket=%d, empty=%d/%d, chi2=%.1f\n",
           name, max_count, empty, m, chi2);

    free(counts);
}

// --- main ---

int main(void) {
    // demo 1: integer hash comparison
    printf("=== Demo 1: integer hash (division vs multiplication) ===\n");
    int m = 16;
    printf("m = %d\n", m);
    printf("%8s  div  mult\n", "key");
    for (int k = 0; k < 20; k++) {
        printf("%8d  %3u   %3u\n", k,
               hash_div(k, m), hash_mult(k, m));
    }
    printf("\n");

    // demo 2: division with bad m
    printf("=== Demo 2: division — m matters ===\n");
    int keys[] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 72};
    int sizes[] = {8, 10, 11};
    for (int s = 0; s < 3; s++) {
        printf("m = %d, keys = multiples of 8:\n  h = [", sizes[s]);
        for (int i = 0; i < 10; i++) {
            if (i > 0) printf(", ");
            printf("%u", hash_div(keys[i], sizes[s]));
        }
        int distinct = 0;
        int seen[64] = {0};
        for (int i = 0; i < 10; i++) {
            unsigned int h = hash_div(keys[i], sizes[s]);
            if (!seen[h]) { seen[h] = 1; distinct++; }
        }
        printf("]\n  distinct buckets: %d/10\n\n", distinct);
    }

    // demo 3: multiplication with golden ratio
    printf("=== Demo 3: Knuth multiplicative — sequential keys ===\n");
    printf("m = 16, sequential keys:\n");
    printf("%8s  hash\n", "key");
    for (int k = 100; k < 120; k++) {
        printf("%8d  %3u\n", k, hash_mult(k, 16));
    }
    printf("Note: sequential keys spread well (golden ratio property)\n\n");

    // demo 4: string hash comparison
    printf("=== Demo 4: string hash — anagram test ===\n");
    const char *anagrams[] = {"abc", "bca", "cab", "bac", "acb", "cba"};
    printf("%-6s  %10s  %10s  %10s  %10s\n",
           "word", "sum", "poly31", "djb2", "fnv1a");
    for (int i = 0; i < 6; i++) {
        printf("%-6s  %10u  %10u  %10u  %10u\n", anagrams[i],
               hash_sum(anagrams[i]),
               hash_poly31(anagrams[i]),
               hash_djb2(anagrams[i]),
               hash_fnv1a(anagrams[i]));
    }
    printf("Sum produces identical hashes for all anagrams!\n\n");

    // demo 5: trace of djb2
    printf("=== Demo 5: djb2 trace for \"hello\" ===\n");
    {
        unsigned int h = 5381;
        const char *s = "hello";
        printf("  seed = %u\n", h);
        for (int i = 0; s[i]; i++) {
            unsigned int old = h;
            h = h * 33 + (unsigned char)s[i];
            printf("  h = %u * 33 + '%c'(%d) = %u\n",
                   old, s[i], s[i], h);
        }
        printf("  final hash = %u\n\n", h);
    }

    // demo 6: trace of FNV-1a
    printf("=== Demo 6: FNV-1a trace for \"hello\" ===\n");
    {
        unsigned int h = 2166136261U;
        const char *s = "hello";
        printf("  offset basis = %u\n", h);
        for (int i = 0; s[i]; i++) {
            unsigned int xored = h ^ (unsigned char)s[i];
            h = xored * 16777619U;
            printf("  h = (%u ^ '%c') * 16777619 = %u\n",
                   h / 16777619U * 16777619U != h ? xored : xored,
                   s[i], h);
        }
        printf("  final hash = %u\n\n", h);
    }

    // demo 7: distribution analysis
    printf("=== Demo 7: distribution analysis ===\n");
    // generate test words
    #define NWORDS 1000
    #define MAXLEN 12
    char *test_words[NWORDS];
    srand(42);
    for (int i = 0; i < NWORDS; i++) {
        int len = 4 + rand() % (MAXLEN - 4);
        test_words[i] = malloc(len + 1);
        for (int j = 0; j < len; j++)
            test_words[i][j] = 'a' + rand() % 26;
        test_words[i][len] = '\0';
    }

    int table_size = 127;  // prime
    printf("1000 random words, m = %d (prime):\n", table_size);
    analyze_distribution("sum", hash_sum,
                         (const char **)test_words, NWORDS, table_size);
    analyze_distribution("poly31", hash_poly31,
                         (const char **)test_words, NWORDS, table_size);
    analyze_distribution("djb2", hash_djb2,
                         (const char **)test_words, NWORDS, table_size);
    analyze_distribution("fnv1a", hash_fnv1a,
                         (const char **)test_words, NWORDS, table_size);

    printf("\nm = %d (power of 2):\n", 128);
    analyze_distribution("sum", hash_sum,
                         (const char **)test_words, NWORDS, 128);
    analyze_distribution("poly31", hash_poly31,
                         (const char **)test_words, NWORDS, 128);
    analyze_distribution("djb2", hash_djb2,
                         (const char **)test_words, NWORDS, 128);
    analyze_distribution("fnv1a", hash_fnv1a,
                         (const char **)test_words, NWORDS, 128);
    printf("(lower chi2 = more uniform; ideal chi2 ≈ m-1 = %d)\n",
           table_size - 1);

    for (int i = 0; i < NWORDS; i++)
        free(test_words[i]);

    // demo 8: avalanche test
    printf("\n=== Demo 8: avalanche (1-bit change) ===\n");
    const char *base = "hello";
    char modified[6];
    strcpy(modified, base);

    printf("%-8s  %10s  %10s  %10s  %10s\n",
           "word", "poly31", "djb2", "fnv1a", "bits_diff");
    printf("%-8s  %10u  %10u  %10u  (baseline)\n",
           base, hash_poly31(base), hash_djb2(base), hash_fnv1a(base));

    for (int i = 0; i < 5; i++) {
        strcpy(modified, base);
        modified[i] ^= 1;  // flip 1 bit of character i
        unsigned int h1 = hash_fnv1a(base);
        unsigned int h2 = hash_fnv1a(modified);
        int bits = __builtin_popcount(h1 ^ h2);
        printf("%-8s  %10u  %10u  %10u  %d/32 bits\n",
               modified, hash_poly31(modified), hash_djb2(modified),
               hash_fnv1a(modified), bits);
    }
    printf("Good avalanche: ~16/32 bits differ per 1-bit change\n");

    return 0;
}
```

Compilar y ejecutar:

```bash
gcc -std=c11 -Wall -Wextra -O2 -o hash_functions hash_functions.c -lm
./hash_functions
```

---

## Programa completo en Rust

```rust
// hash_functions.rs — hash function properties and Rust hashing system
use std::collections::HashMap;
use std::hash::{Hash, Hasher, DefaultHasher, BuildHasher};

// --- custom hash functions ---

fn hash_sum(s: &str) -> u32 {
    s.bytes().map(|b| b as u32).sum()
}

fn hash_poly31(s: &str) -> u32 {
    let mut h: u32 = 0;
    for b in s.bytes() {
        h = h.wrapping_mul(31).wrapping_add(b as u32);
    }
    h
}

fn hash_djb2(s: &str) -> u32 {
    let mut h: u32 = 5381;
    for b in s.bytes() {
        h = h.wrapping_mul(33).wrapping_add(b as u32);
    }
    h
}

fn hash_fnv1a(s: &str) -> u32 {
    let mut h: u32 = 2166136261;
    for b in s.bytes() {
        h ^= b as u32;
        h = h.wrapping_mul(16777619);
    }
    h
}

fn compute_default_hash<T: Hash>(val: &T) -> u64 {
    let mut hasher = DefaultHasher::new();
    val.hash(&mut hasher);
    hasher.finish()
}

fn main() {
    // demo 1: anagram test
    println!("=== Demo 1: anagram test ===");
    let anagrams = ["abc", "bca", "cab", "bac", "acb", "cba"];

    println!("{:<6} {:>10} {:>10} {:>10} {:>10}",
             "word", "sum", "poly31", "djb2", "fnv1a");
    for w in &anagrams {
        println!("{:<6} {:>10} {:>10} {:>10} {:>10}",
                 w, hash_sum(w), hash_poly31(w), hash_djb2(w), hash_fnv1a(w));
    }
    println!("Sum: all identical (anagram-blind)\n");

    // demo 2: djb2 trace
    println!("=== Demo 2: djb2 trace for \"hello\" ===");
    {
        let mut h: u32 = 5381;
        println!("  seed = {}", h);
        for b in "hello".bytes() {
            let old = h;
            h = h.wrapping_mul(33).wrapping_add(b as u32);
            println!("  h = {} * 33 + '{}'({}) = {}", old, b as char, b, h);
        }
        println!("  final = {}\n", h);
    }

    // demo 3: Rust's DefaultHasher (SipHash)
    println!("=== Demo 3: Rust DefaultHasher (SipHash) ===");
    for w in &["hello", "hellp", "Hello", "abc", "bca"] {
        println!("  DefaultHasher(\"{}\") = {}", w, compute_default_hash(w));
    }
    println!("Note: SipHash is keyed — different program runs may give different results\n");

    // demo 4: Hash trait for custom types
    println!("=== Demo 4: Hash trait ===");

    // derive version
    #[derive(Hash, PartialEq, Eq, Debug)]
    struct PointDerived {
        x: i32,
        y: i32,
    }

    // manual version
    #[derive(PartialEq, Eq, Debug)]
    struct PointManual {
        x: i32,
        y: i32,
    }

    impl Hash for PointManual {
        fn hash<H: Hasher>(&self, state: &mut H) {
            self.x.hash(state);
            self.y.hash(state);
        }
    }

    let pd = PointDerived { x: 3, y: 7 };
    let pm = PointManual { x: 3, y: 7 };
    println!("  Derived hash({:?}) = {}", pd, compute_default_hash(&pd));
    println!("  Manual  hash({:?}) = {}", pm, compute_default_hash(&pm));
    println!("  (should be identical)\n");

    // demo 5: using custom type as HashMap key
    println!("=== Demo 5: custom type as HashMap key ===");
    let mut grid: HashMap<PointDerived, &str> = HashMap::new();
    grid.insert(PointDerived { x: 0, y: 0 }, "origin");
    grid.insert(PointDerived { x: 1, y: 0 }, "east");
    grid.insert(PointDerived { x: 0, y: 1 }, "north");

    println!("  grid[0,0] = {:?}", grid.get(&PointDerived { x: 0, y: 0 }));
    println!("  grid[1,0] = {:?}", grid.get(&PointDerived { x: 1, y: 0 }));
    println!("  grid[2,2] = {:?}", grid.get(&PointDerived { x: 2, y: 2 }));
    println!();

    // demo 6: distribution analysis
    println!("=== Demo 6: distribution analysis ===");
    let m = 127usize;

    // generate random words
    let words: Vec<String> = (0..1000).map(|i| {
        let mut w = String::new();
        let mut x = i * 2654435769u64;
        for _ in 0..(4 + (x % 8)) {
            x = x.wrapping_mul(6364136223846793005).wrapping_add(1);
            w.push((b'a' + (x % 26) as u8) as char);
        }
        w
    }).collect();

    // test each function
    for (name, hash_fn) in &[
        ("sum", hash_sum as fn(&str) -> u32),
        ("poly31", hash_poly31),
        ("djb2", hash_djb2),
        ("fnv1a", hash_fnv1a),
    ] {
        let mut buckets = vec![0u32; m];
        for w in &words {
            buckets[hash_fn(w) as usize % m] += 1;
        }
        let max_b = *buckets.iter().max().unwrap();
        let empty = buckets.iter().filter(|&&c| c == 0).count();
        let chi2: f64 = {
            let expected = words.len() as f64 / m as f64;
            buckets.iter().map(|&c| {
                let d = c as f64 - expected;
                d * d / expected
            }).sum()
        };
        println!("  {:<8}: max_bucket={}, empty={}/{}, chi2={:.1}",
                 name, max_b, empty, m, chi2);
    }
    println!("  (ideal chi2 ≈ {})\n", m - 1);

    // demo 7: avalanche test
    println!("=== Demo 7: avalanche (fnv1a) ===");
    let base = "hello";
    let base_hash = hash_fnv1a(base);
    println!("  base: \"{}\" -> {}", base, base_hash);

    for i in 0..5 {
        let mut modified: Vec<u8> = base.bytes().collect();
        modified[i] ^= 1;  // flip 1 bit
        let mod_str = String::from_utf8_lossy(&modified);
        let mod_hash = hash_fnv1a(&mod_str);
        let bits_diff = (base_hash ^ mod_hash).count_ones();
        println!("  flip bit at [{}]: \"{}\" -> {}, bits changed: {}/32",
                 i, mod_str, mod_hash, bits_diff);
    }
    println!("  Good avalanche: ~16/32 bits change per 1-bit flip\n");

    // demo 8: HashMap randomization
    println!("=== Demo 8: HashMap randomization ===");
    let map1: HashMap<&str, i32> = [("a", 1), ("b", 2), ("c", 3)].into();
    let map2: HashMap<&str, i32> = [("a", 1), ("b", 2), ("c", 3)].into();

    let hasher1 = map1.hasher();
    let hasher2 = map2.hasher();

    let mut h1 = hasher1.build_hasher();
    let mut h2 = hasher2.build_hasher();
    "test".hash(&mut h1);
    "test".hash(&mut h2);

    println!("  map1 hasher(\"test\") = {}", h1.finish());
    println!("  map2 hasher(\"test\") = {}", h2.finish());
    println!("  (may differ — each HashMap has its own random seed)");
    println!("  This prevents HashDoS attacks");
}
```

Compilar y ejecutar:

```bash
rustc hash_functions.rs && ./hash_functions
```

---

## Errores frecuentes

1. **Usar hash sum para strings**: `h += c` produce colisiones
   para todos los anagramas. Siempre usar hash polinomial, djb2 o
   FNV-1a.
2. **Ignorar overflow**: en C, `unsigned int` overflow es
   comportamiento definido (wrap around), pero `signed int`
   overflow es **undefined behavior**. Siempre usar `unsigned` para
   hashing.
3. **Hashear structs con padding**: `hash_bytes(&s, sizeof(s))`
   incluye bytes de padding no inicializados. El hash será
   no-determinista. Hashear campo por campo.
4. **`Hash` inconsistente con `Eq` en Rust**: si `a == b` pero
   `hash(a) != hash(b)`, el `HashMap` se corrompe silenciosamente
   (inserta duplicados, no encuentra claves existentes).
5. **Asumir que el hash es único**: una función hash de 32 bits
   solo produce $2^{32} \approx 4.3 \times 10^9$ valores. Con
   ~77000 claves, la probabilidad de al menos una colisión de hash
   (birthday paradox) supera 50%.

---

## Ejercicios

### Ejercicio 1 — Multiplicación de Knuth a mano

Calcula $h(k) = \lfloor 16 \cdot \text{frac}(k \cdot A) \rfloor$
con $A = 0.618$ para $k = 0, 1, 2, \ldots, 9$. Verifica que
claves consecutivas se dispersan bien.

<details><summary>Predice antes de ejecutar</summary>

¿Hay algún par de claves consecutivas que caigan en el mismo
bucket? ¿Cuántos de los 16 buckets se usan con solo 10 claves?

</details>

### Ejercicio 2 — djb2 vs FNV-1a para palabras similares

Calcula el hash djb2 y FNV-1a de `{"test0", "test1", "test2",
..., "test9"}`. ¿Cuántos buckets distintos (mod 16) usa cada
función?

<details><summary>Predice antes de ejecutar</summary>

Estas claves difieren solo en el último carácter. ¿Cuál función
distribuye mejor? ¿El efecto avalancha es visible?

</details>

### Ejercicio 3 — Chi-cuadrado

Genera 10000 strings aleatorios de longitud 8 (a-z) e insértalos
en tablas de $m = 97$ (primo) y $m = 100$ (no primo) usando
`hash_fnv1a`. Calcula el estadístico $\chi^2$ para cada una.

<details><summary>Predice antes de ejecutar</summary>

¿Cuál $m$ tendrá mejor $\chi^2$? ¿La diferencia será grande o
pequeña para FNV-1a (que ya tiene buena avalancha)?

</details>

### Ejercicio 4 — Hash para enteros negativos

Implementa `hash_int_safe(int key, int m)` que funcione
correctamente para claves negativas en C. Prueba con
$\{-10, -1, 0, 1, 10\}$ y $m = 7$.

<details><summary>Predice antes de ejecutar</summary>

¿Qué retorna `(-10) % 7` en C? ¿Y `(-1) % 7`? ¿Cómo lo
corrige la fórmula `((key % m) + m) % m`?

</details>

### Ejercicio 5 — Hash polinomial: efecto de la base

Prueba la función hash polinomial con bases $b = 2, 31, 256$
para 1000 palabras en una tabla de $m = 127$. Compara la
distribución (max bucket, $\chi^2$).

<details><summary>Predice antes de ejecutar</summary>

¿Por qué $b = 256$ es problemática? Pista: equivale a
interpretar la cadena como un entero big-endian, y mod 127
pierde información de los bytes altos.

</details>

### Ejercicio 6 — Implementar `Hash` para un struct en Rust

Crea un struct `Student { id: u32, name: String, gpa: f64 }`.
Implementa `Hash` y `Eq` manualmente, donde la igualdad y el hash
se basan solo en `id` (ignoran `name` y `gpa`). Verifica que dos
estudiantes con mismo `id` pero diferente `name` se tratan como
iguales en un `HashMap`.

<details><summary>Predice antes de ejecutar</summary>

Si insertas dos estudiantes con `id = 1` pero nombres diferentes,
¿cuántas entradas hay en el HashMap? ¿Cuál nombre se conserva?

</details>

### Ejercicio 7 — Visualizar distribución

Escribe un programa que muestre la distribución de una función
hash como histograma en la terminal. Para $m = 20$ y 200 palabras,
imprime una línea por bucket con `#` proporcional al conteo.

<details><summary>Predice antes de ejecutar</summary>

Con una buena función hash, ¿cuántos `#` esperarías por bucket?
¿Cuánta variación es normal?

</details>

### Ejercicio 8 — FNV-1a 64-bit

Implementa FNV-1a de 64 bits (offset basis = 14695981039346656037,
prime = 1099511628211). Compara la distribución con la versión de
32 bits para 10000 palabras en tabla de $m = 997$.

<details><summary>Predice antes de ejecutar</summary>

¿Habrá diferencia significativa en la distribución? ¿Cuándo
importa usar 64 bits en vez de 32?

</details>

### Ejercicio 9 — Hash de tuplas

Implementa en C una función `hash_pair(int a, int b, int m)` que
hashee un par de enteros. Prueba dos estrategias: (a) `hash(a) ^
hash(b)`, (b) `hash(a) * 31 + hash(b)`. Inserta los pares
$(i, j)$ para $0 \leq i, j < 10$ y compara distribución.

<details><summary>Predice antes de ejecutar</summary>

¿La estrategia XOR tiene un problema con pares como $(3, 5)$ y
$(5, 3)$? ¿Y con $(4, 4)$?

</details>

### Ejercicio 10 — Benchmark: hash function speed

Mide el tiempo de calcular djb2 y FNV-1a para 1 millón de strings
de longitud 10, 100 y 1000. Reporta bytes/segundo para cada
combinación.

<details><summary>Predice antes de ejecutar</summary>

¿Cuál será más rápida? ¿El throughput dependerá de la longitud
del string? ¿Cuántos GB/s esperarías en hardware moderno?

</details>
