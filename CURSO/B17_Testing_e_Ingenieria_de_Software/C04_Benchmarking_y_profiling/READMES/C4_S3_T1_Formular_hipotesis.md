# T01 — Formular hipótesis

> **Bloque 17 · Capítulo 4 · Sección 3 · Tópico 1**
> Sin hipótesis, profiling es turismo. Con hipótesis, es ingeniería. El método científico aplicado al rendimiento.

---

## Índice

1. [La diferencia entre medir y experimentar](#1-la-diferencia-entre-medir-y-experimentar)
2. [Qué es una hipótesis de rendimiento](#2-qué-es-una-hipótesis-de-rendimiento)
3. [Anatomía de una hipótesis bien formulada](#3-anatomía-de-una-hipótesis-bien-formulada)
4. [El ciclo hipótesis → experimento → decisión](#4-el-ciclo-hipótesis--experimento--decisión)
5. [Hipótesis vaga vs hipótesis falsable](#5-hipótesis-vaga-vs-hipótesis-falsable)
6. [Predicciones cuantitativas: el criterio de éxito](#6-predicciones-cuantitativas-el-criterio-de-éxito)
7. [Base de la hipótesis: modelos de coste](#7-base-de-la-hipótesis-modelos-de-coste)
8. [Mental models de hardware para hipotetizar](#8-mental-models-de-hardware-para-hipotetizar)
9. [Napkin math: estimar antes de medir](#9-napkin-math-estimar-antes-de-medir)
10. [Números que todo ingeniero debe saber de memoria](#10-números-que-todo-ingeniero-debe-saber-de-memoria)
11. [Ejemplo 1: SoA vs AoS](#11-ejemplo-1-soa-vs-aos)
12. [Ejemplo 2: Vec vs LinkedList](#12-ejemplo-2-vec-vs-linkedlist)
13. [Ejemplo 3: HashMap vs BTreeMap](#13-ejemplo-3-hashmap-vs-btreemap)
14. [Ejemplo 4: malloc vs stack vs arena](#14-ejemplo-4-malloc-vs-stack-vs-arena)
15. [Ejemplo 5: branchy vs branchless](#15-ejemplo-5-branchy-vs-branchless)
16. [Ejemplo 6: SIMD vs escalar](#16-ejemplo-6-simd-vs-escalar)
17. [Diseño experimental: variables independientes y controladas](#17-diseño-experimental-variables-independientes-y-controladas)
18. [Tamaño de muestra: cuántas iteraciones](#18-tamaño-de-muestra-cuántas-iteraciones)
19. [Criterio de significancia estadística](#19-criterio-de-significancia-estadística)
20. [Qué hacer cuando la hipótesis es refutada](#20-qué-hacer-cuando-la-hipótesis-es-refutada)
21. [Confirmation bias y cómo evitarlo](#21-confirmation-bias-y-cómo-evitarlo)
22. [Documentar hipótesis: el lab notebook](#22-documentar-hipótesis-el-lab-notebook)
23. [El peligro de la optimización especulativa](#23-el-peligro-de-la-optimización-especulativa)
24. [Plantilla de hipótesis estándar](#24-plantilla-de-hipótesis-estándar)
25. [Anti-patrones comunes al hipotetizar](#25-anti-patrones-comunes-al-hipotetizar)
26. [Programa de práctica: hypothesis_lab.c](#26-programa-de-práctica-hypothesis_labc)
27. [Ejercicios](#27-ejercicios)
28. [Checklist del método](#28-checklist-del-método)
29. [Navegación](#29-navegación)

---

## 1. La diferencia entre medir y experimentar

Medir y experimentar **no son lo mismo**.

**Medir** es preguntar "¿cuánto tarda esto?". Es descriptivo. Abres perf, corres
el programa, ves que `process_batch` ocupa el 40% del tiempo. Has medido.

**Experimentar** es preguntar "¿tardará menos si hago X?". Es predictivo y causal.
Antes de medir, ya has formulado una predicción concreta. Mides para **confirmar
o refutar** esa predicción.

Por qué importa:

1. **Sin hipótesis, no sabes qué buscar.** Abres perf y ves 200 funciones. ¿Cuál
   es "lenta"? Depende de la expectativa. Sin expectativa, todo parece normal.
2. **Sin hipótesis, no sabes cuándo parar.** Optimizas algo, ves mejora de 10%.
   ¿Es suficiente? ¿Es lo máximo posible? Si predijiste 30%, sabes que queda
   trabajo. Si predijiste 5%, está hecho.
3. **Sin hipótesis, puedes optimizar lo incorrecto.** Pasas días mejorando una
   función que no era el cuello de botella porque "parecía lenta". Con hipótesis,
   verificas primero que la función **es** el cuello de botella.
4. **Sin hipótesis, no aprendes.** Una medición aislada te da un número. Una
   hipótesis confirmada o refutada te enseña cómo funciona el sistema. A largo
   plazo esto es mucho más valioso.

**Regla**: antes de profilear, escribe en una nota lo que **esperas** encontrar.
Después profilea. La diferencia entre lo esperado y lo observado es donde vive
el aprendizaje.

---

## 2. Qué es una hipótesis de rendimiento

Una **hipótesis de rendimiento** es una afirmación **falsable** sobre el
comportamiento de un programa que incluye:

1. **Una causa** (el cambio que propones).
2. **Un efecto** (la mejora esperada).
3. **Una magnitud** (cuánto).
4. **Un mecanismo** (por qué, basado en un modelo mental).
5. **Condiciones** (en qué contexto vale).

Ejemplos:

**MALA** (vaga, no falsable):
> "Cambiar Vec por SmallVec mejorará el rendimiento."

**MEJOR** (causa + efecto + magnitud):
> "Cambiar `Vec<u8>` por `SmallVec<[u8; 32]>` reducirá el tiempo de
> `build_message` en ≥20% porque eliminará la asignación en heap en el 95% de
> los casos (los que caben en 32 bytes)."

**MEJOR AÚN** (añadiendo condiciones y mecanismo):
> "Para mensajes ≤32 bytes (95% del tráfico actual según logs), `SmallVec<[u8; 32]>`
> reducirá el tiempo de `build_message` en ≥20% en p50 y ≥40% en p99, porque
> elimina una llamada a malloc (~40 ns) que actualmente domina. Para mensajes
> >32 bytes, el rendimiento será equivalente (±5%). Esto lo confirmaré midiendo
> `build_message` con 1000 muestras de tamaños [16, 32, 64, 128] bytes."

La última versión es **accionable**: puedes diseñar un experimento que la
confirme o refute inequívocamente.

---

## 3. Anatomía de una hipótesis bien formulada

Componentes obligatorios:

```
┌──────────────────────────────────────────────────────────┐
│                     HIPÓTESIS                            │
├──────────────────────────────────────────────────────────┤
│  CAMBIO:     qué vas a modificar                        │
│  EFECTO:     qué métrica mejorará                        │
│  MAGNITUD:   cuánto, en número concreto                 │
│  DIRECCIÓN:  mejorar vs empeorar (y en qué)             │
│  MECANISMO:  por qué, basado en modelo mental           │
│  CONDICIÓN:  en qué inputs/contexto vale                │
│  MEDICIÓN:   cómo vas a confirmar/refutar               │
│  UMBRAL:     qué resultado confirma vs refuta           │
└──────────────────────────────────────────────────────────┘
```

Ejemplo completo para optimizar una búsqueda:

- **CAMBIO**: Sustituir `linear_search` O(n) por `HashMap::get` O(1) amortizado.
- **EFECTO**: Reducir tiempo de `find_by_id`.
- **MAGNITUD**: Al menos 10× más rápido para N ≥ 1000 elementos.
- **DIRECCIÓN**: Mejorar p50, p95 y p99 simultáneamente.
- **MECANISMO**: Linear search hace N/2 comparaciones promedio (O(N)). HashMap
  hace 1-2 comparaciones y un hash (~15 ns). Para N=1000, 500 comparaciones vs 1
  es ~500× menos trabajo. Pero el hash cuesta ~15 ns y las comparaciones son ~1
  ns cada una, así que la ventaja real es ~30×, no 500×.
- **CONDICIÓN**: Para N=1000 elementos, keys uniformes aleatorias, 50%
  lookups hit.
- **MEDICIÓN**: Criterion con grupos `linear/1000`, `hashmap/1000`, 100
  samples, warmup 3s, 5s por sample.
- **UMBRAL**: Confirmo si `hashmap/1000` < `linear/1000` / 10. Refuto si
  la ventaja es <5× (fuera de mi modelo de coste). Investigo más si está entre
  5× y 10×.

Con esta estructura, el experimento ya está diseñado. Solo queda ejecutarlo.

---

## 4. El ciclo hipótesis → experimento → decisión

```
                        ┌─────────────────┐
                        │  1. OBSERVAR    │
                        │  (profilear,    │
                        │   ver hotspot)  │
                        └────────┬────────┘
                                 │
                                 ▼
                        ┌─────────────────┐
                        │  2. HIPOTETIZAR │
                        │  (causa+efecto+ │
                        │   magnitud)     │
                        └────────┬────────┘
                                 │
                                 ▼
                        ┌─────────────────┐
                        │  3. DISEÑAR     │
                        │  EXPERIMENTO    │
                        └────────┬────────┘
                                 │
                                 ▼
                        ┌─────────────────┐
                        │  4. MEDIR       │
                        │  baseline +     │
                        │  propuesta      │
                        └────────┬────────┘
                                 │
                                 ▼
                        ┌─────────────────┐
                        │  5. COMPARAR    │
                        │  vs predicción  │
                        └────────┬────────┘
                                 │
                           ┌─────┴─────┐
                           ▼           ▼
                      CONFIRMADO    REFUTADO
                           │           │
                           ▼           ▼
                        DEPLOY     APRENDER
                                   (¿qué falló
                                    en mi modelo?)
                                       │
                                       └──→ nueva hipótesis
```

Cada ciclo produce uno de tres resultados:

1. **Confirmado**: mejora esperada, deploy.
2. **Refutado por debajo**: la mejora fue menor o nula. Aprendizaje: tu modelo
   mental estaba incompleto. ¿Por qué? ¿Cache effect? ¿Compiler optimization?
   ¿Overhead no considerado?
3. **Refutado por arriba**: la mejora fue mayor que lo esperado. Aprendizaje:
   igual de importante. Hay un factor que no habías considerado que amplifica la
   ventaja. Entenderlo te ayuda en futuros cambios.

Los tres son útiles. El resultado negativo (refutado por debajo) **es el más
valioso pedagógicamente** porque te obliga a corregir tu modelo mental.

---

## 5. Hipótesis vaga vs hipótesis falsable

Karl Popper: una hipótesis científica debe ser **falsable**. Tienes que poder
imaginar un resultado experimental que la invalide.

### Vaga (no falsable)

> "SIMD será más rápido que escalar."

No es falsable porque **siempre** puedes encontrar un caso donde sea verdad.
¿Cuánto? ¿En qué input? ¿Comparado con qué versión escalar?

### Falsable

> "AVX2 `_mm256_add_ps` será al menos 4× más rápido que un bucle escalar para
> sumar dos arrays de 1 millón de floats alineados a 32 bytes, compilado con
> `-O2 -mavx2`, en una CPU Intel Skylake o posterior."

Es falsable porque si mides 3.5×, se refuta. Y cuando se refuta, aprendes: quizá
el compilador ya auto-vectorizaba con `-O2`, quizá había dependencias de datos,
quizá el prefetcher saturó el bus de memoria y SIMD no ayuda en memory-bound.

### Test de falsabilidad

Pregúntate: "¿qué número concreto haría que mi hipótesis sea falsa?". Si no
puedes responder, tu hipótesis es vaga.

Ejemplos:

| Hipótesis                             | ¿Qué la refutaría?                        |
|---------------------------------------|-------------------------------------------|
| "Rust será más rápido"                | (vacío — no falsable)                     |
| "Rust será ≥10% más rápido en Fibonacci(40)" | 9% o menos                        |
| "HashMap O(1) es mejor que linear"    | (vacío — depende de todo)                |
| "HashMap es ≥5× más rápido para N≥1000 con hit rate 50%" | 4× o menos |

La segunda columna es la que convierte la afirmación en algo útil.

---

## 6. Predicciones cuantitativas: el criterio de éxito

Las predicciones deben ser **numéricas** siempre que sea posible:

- ✓ "Reducción de ≥30% en p99 latency"
- ✓ "Throughput ≥ 10K req/s"
- ✓ "Peak heap < 500 MB"
- ✗ "Más rápido"
- ✗ "Mejor"
- ✗ "Significativo"

Por qué importa:

1. **Define éxito y fracaso**. Si predices 30% y obtienes 28%, **es fracaso**
   (aunque sea mejora). Eso te hace pensar: ¿por qué no llegué? La respuesta te
   enseña algo.
2. **Permite priorizar**. Tres optimizaciones candidatas con mejoras esperadas
   de 5%, 15% y 40% se priorizan solas.
3. **Evita "mejoró" sin contexto**. Un programa que era 100 ms y ahora es 99 ms
   "mejoró". ¿Es eso relevante? Depende del objetivo.

### Rangos de predicción

Es aceptable (y a menudo más honesto) predecir un **rango**:

> "Entre 20% y 40% de reducción en p50."

Esto refleja incertidumbre genuina. Si obtienes 30%, dentro del rango,
**confirmado**. Si obtienes 10%, fuera del rango, **refutado** → investiga.

### Umbrales absolutos vs relativos

**Relativo** ("X% más rápido") es mejor para comparar versiones.
**Absoluto** ("≤ 50 ms p99") es mejor para SLO/SLA.

Usa el que corresponda al objetivo:
- Optimización interna: relativo.
- Contrato con usuario: absoluto.

---

## 7. Base de la hipótesis: modelos de coste

Las hipótesis sin base son **adivinanzas**. Las hipótesis bien formuladas se
sustentan en un **modelo de coste** — una abstracción del sistema que predice
el comportamiento.

Modelos típicos:

### Modelo asintótico (Big-O)

O(1), O(log n), O(n), O(n log n), O(n²)...

Útil para decidir entre algoritmos cuando N es grande. Insuficiente para
pequeñas N (las constantes dominan).

Ejemplo: para N=10, linear search (O(N)) puede ser más rápido que HashMap (O(1)
con overhead) porque 10 comparaciones simples son más baratas que un hash.

### Modelo de cache

Memoria no es uniforme. Acceder L1 es ~1 ns, L2 ~3 ns, L3 ~12 ns, DRAM ~100 ns.
Un algoritmo con peor Big-O puede ser más rápido si tiene mejor locality.

Predicción basada en cache: "reordenar el loop para acceso secuencial reducirá
los D1 misses de ~25% a <1%, y el tiempo en ~3×".

### Modelo de instrucciones

Contar aproximadamente cuántas instrucciones ejecuta cada variante. Callgrind
(T03) da números exactos.

Predicción: "la versión branchless ejecuta 20% menos instrucciones por iteración".

### Modelo de pipeline

Las CPUs modernas son superescalares: 3-6 instrucciones por ciclo en el mejor
caso. Dependencias serializan. SIMD procesa N valores en una instrucción.

Predicción: "SIMD 4-wide reducirá el tiempo en ~3.5× (no 4× por overhead de
load/store)".

### Modelo de allocator

`malloc` tarda ~30-50 ns, `free` ~20-30 ns. Stack allocation es ~0 ns (solo
ajuste de puntero). Arena allocation ~5 ns.

Predicción: "eliminar la malloc temporal del loop caliente (100K iteraciones)
ahorrará ~4 ms (100K × 40 ns)".

### Modelo de branch predictor

Branches predictibles ~0 ciclos de overhead. Mispredict ~15-20 ciclos.

Predicción: "Sortear los datos antes del loop reducirá el mispredict rate del
50% al ~0%, lo que ahorra ~0.7 × 15 = ~10 ns por iteración".

Los modelos son **aproximaciones**. Lo importante es tener uno. Sin modelo, no
hay razón para esperar una magnitud específica, y la hipótesis se degrada a
adivinanza.

---

## 8. Mental models de hardware para hipotetizar

Modelos mentales fundamentales que todo ingeniero de sistemas debería tener:

### 8.1. Jerarquía de memoria

```
 ┌──────────────┐
 │ Registros    │  ~0.3 ns  ~100 bytes
 ├──────────────┤
 │ L1 cache     │  ~1 ns    ~32-64 KB
 ├──────────────┤
 │ L2 cache     │  ~3-4 ns  ~256KB-1MB
 ├──────────────┤
 │ L3 cache     │  ~12 ns   ~10-40 MB
 ├──────────────┤
 │ DRAM         │  ~100 ns  GBs
 ├──────────────┤
 │ SSD          │  ~100 µs  TBs
 ├──────────────┤
 │ HDD          │  ~10 ms   TBs
 ├──────────────┤
 │ Network RTT  │  ~1 ms (local), ~100 ms (remote)
 └──────────────┘
```

Ratio clave: **DRAM es ~100× más lenta que L1**. Un algoritmo memory-bound está
limitado por el bus, no por la CPU.

### 8.2. Cache line y locality

Cada acceso a memoria trae **64 bytes contiguos** a la cache (una línea). Si lees
secuencialmente, el hardware prefetcher trae las siguientes antes de que las
necesites → "gratis". Si saltas aleatoriamente, cada acceso es miss.

**Regla**: organiza los datos como los vas a recorrer. Si recorres en columnas,
guarda en columnas. Si agrupas por tipo, guarda por tipo (SoA).

### 8.3. Branch predictor

La CPU adivina el resultado de cada branch antes de evaluarla. Si acierta, sin
overhead. Si falla, descarta el trabajo especulativo y pierde ~15-20 ciclos.

**Regla**: branches predictibles son gratis. Branches aleatorias son caras.
Si un branch falla el 50% del tiempo, cada iteración paga ~7-10 ns extra.

### 8.4. Pipeline y dependencias

Las CPUs ejecutan varias instrucciones en paralelo **si no hay dependencias**.
Una cadena de `a = a + x; a = a * y; a = a / z;` se ejecuta secuencialmente
porque cada una depende de la anterior. Una cadena de sumas en variables
distintas se paraleliza.

**Regla**: si haces accumulación, usa múltiples acumuladores para permitir
paralelismo. Esto es el "loop unrolling con acumuladores distintos".

### 8.5. SIMD

Una instrucción SIMD de 256 bits (AVX) opera sobre 8 floats o 4 doubles a la
vez. 4-8× ganancia teórica si el compilador o tú la usan. Real: 2-4× por
overhead.

**Regla**: el compilador con `-O3` auto-vectoriza loops simples sobre arrays
contiguos sin dependencias. Estructuras complejas requieren intrinsics manuales.

### 8.6. TLB

La traducción de virtual a físico se cachea en el TLB. Con páginas 4KB, 64
entries de L1 TLB cubren 256 KB. Accesos que saltan entre muchas páginas causan
TLB misses (~30-100 ns cada uno).

**Regla**: huge pages (2 MB) multiplican la cobertura del TLB por 512.

### 8.7. Allocator cost

`malloc` no es gratis. Un `malloc(16)` en glibc cuesta ~40 ns. `free` cuesta
~25 ns. Una allocation total ~65 ns. En un loop de 1 M iteraciones: ~65 ms solo
en allocator.

**Regla**: la asignación más rápida es la que no existe. Segundo: stack. Tercero:
arena/pool.

Estos 7 modelos cubren el 95% de las hipótesis de rendimiento que formularás.

---

## 9. Napkin math: estimar antes de medir

Antes de ejecutar un benchmark, **calcula a mano** qué esperas. Esto se llama
*napkin math* (matemática de servilleta) — estimaciones rápidas para validar
hipótesis.

### Ejemplo 1

> "¿Cuánto tarda sumar 1 millón de ints en una array?"

Napkin math:
- 1 M ints × 4 bytes = 4 MB total.
- 4 MB / 64 byte cache line = 62 500 cache lines.
- Datos no caben en L1 (32 KB) ni L2 (1 MB). Cabe parcialmente en L3.
- Acceso secuencial → el prefetcher ayuda → la latencia DRAM se oculta.
- Bandwidth típica L3→CPU: ~20 GB/s. Bandwidth DRAM: ~20 GB/s (en un canal).
- 4 MB / 20 GB/s = 200 µs.
- CPU puede sumar 8 ints/ciclo (SIMD) con IPC ~2 → 16 sumas/ciclo teóricas.
- 1 M sumas / 16 = 62 500 ciclos / 3 GHz ≈ 20 µs CPU.
- **El programa será memory-bound: ~200 µs esperado**.

Medida real en un laptop moderno: ~180-250 µs. **Coincide** → tu modelo mental
es correcto.

Si midieras 2000 µs → tu modelo falla. ¿Por qué? Quizá el compilador no
vectorizó. Quizá había false sharing. Investiga.

### Ejemplo 2

> "¿Cuánto ahorras eliminando 1 malloc por request en un servidor HTTP a 10K req/s?"

Napkin math:
- `malloc` ≈ 40 ns, `free` ≈ 25 ns. Total 65 ns por request eliminada.
- 10 000 req/s × 65 ns = 650 µs/s = 0.065% de CPU.
- Prácticamente **nada**, en CPU.
- Pero cada malloc incrementa la presión sobre el allocator global (lock
  contention con múltiples threads). En servidores multithread, el efecto real
  puede ser 10-20× mayor (~1% CPU) por contención en el allocator.
- **Estimación**: 0.1% - 2% de mejora de CPU global. No es dramático, pero
  acumulado sobre 100 decisiones similares, puede ser 10-20%.

Esta estimación **decide si vale la pena** el cambio. Si esperas 0.1%, quizá no.
Si esperas 20%, sí.

### Ejemplo 3

> "¿Cuánto tarda una búsqueda binaria en un sorted array de 1 millón de ints?"

- log2(1M) = 20 comparaciones.
- Cada comparación accede a una posición distinta → miss L1, probable hit L2/L3
  al final.
- 20 accesos × ~10 ns promedio = 200 ns.
- Medición real: 100-300 ns. Coincide.

Si nunca hicieras este cálculo, "rápido" es todo lo que sabes. Con el cálculo,
sabes que 200 ns es el óptimo y cualquier cosa ≥1 µs es sospechosa.

---

## 10. Números que todo ingeniero debe saber de memoria

Basado en el famoso post "Latency Numbers Every Programmer Should Know" de Jeff
Dean, actualizado a hardware moderno (~2024):

| Operación                                   | Latencia típica   | Notas                         |
|---------------------------------------------|-------------------|-------------------------------|
| L1 cache hit                                | ~1 ns            | 1 ciclo en CPU 3GHz          |
| Branch misprediction                        | ~5 ns            | 15-20 ciclos                 |
| L2 cache hit                                | ~4 ns            |                               |
| L3 cache hit                                | ~12 ns           | Cache compartido             |
| Mutex lock/unlock (uncontended)             | ~25 ns           | Futex fast path              |
| malloc/free small                           | ~40 ns + ~25 ns  | glibc                        |
| DRAM access                                 | ~100 ns          |                               |
| Context switch                              | ~1-3 µs          | Userspace ↔ kernel          |
| Mutex lock contended                        | ~1-5 µs          |                               |
| Read 1 MB sequential from memory            | ~50 µs           | Limitado por bandwidth       |
| Disk seek (HDD)                             | ~10 ms           | Cada vez menos relevante     |
| SSD random 4K read                          | ~150 µs          |                               |
| SSD sequential 1 MB read                    | ~500 µs          |                               |
| Network 1KB in same datacenter              | ~500 µs          |                               |
| Network 1 KB cross-continent RTT            | ~100-150 ms      |                               |
| Syscall (getpid, minimal)                   | ~50-200 ns       | vDSO más rápido              |
| syscall con trabajo real                    | ~1-10 µs         |                               |
| Process fork + exec                         | ~1-5 ms          |                               |
| Thread create                               | ~10-50 µs        |                               |
| ~100M simples ALU ops in 1 segundo (1 core) | ~1 s             | Con IPC 3                    |
| ~10 GB bandwidth memoria en 1 segundo       | ~1 s             | Canal único                  |

Memoriza estos o ten una referencia a mano. Son la base de cualquier napkin math.

### Derivadas útiles

Con estos números puedes responder rápidamente:

- "¿Cuánto tarda un programa que accede 10 M veces a una array en memoria que
  no cabe en cache?"
  → ~10 M × ~100 ns = ~1 segundo.
- "¿Cuánto tarda hacer 1 M llamadas a `getpid` via syscall?"
  → ~1 M × ~100 ns = ~0.1 s (con vDSO).
- "¿Cuánto tarda un algoritmo que hace 10 M mallocs?"
  → ~10 M × ~65 ns = ~650 ms (sin contar trabajo útil).

Cada vez que mides y el resultado no coincide con tu napkin math, **investiga
por qué**. Ahí está el aprendizaje.

---

## 11. Ejemplo 1: SoA vs AoS

Caso clásico del modelo de cache.

### Contexto

Tienes una partícula física con posición, velocidad, masa:

```c
/* AoS — Array of Structures */
typedef struct {
    float x, y, z;        /* 12 bytes */
    float vx, vy, vz;     /* 12 bytes */
    float mass;           /*  4 bytes */
    float _pad;           /*  4 bytes */
} Particle;               /* 32 bytes total, 2 por cache line */

Particle particles[1000000];
```

```c
/* SoA — Structure of Arrays */
typedef struct {
    float *x, *y, *z;
    float *vx, *vy, *vz;
    float *mass;
} ParticleSystem;

/* Todos los x contiguos, todos los y contiguos, etc. */
```

### Hipótesis

Operación: calcular el módulo de velocidad para todas las partículas.

```c
for (int i = 0; i < N; i++) {
    speed[i] = sqrt(vx[i]*vx[i] + vy[i]*vy[i] + vz[i]*vz[i]);
}
```

- **CAMBIO**: AoS → SoA.
- **EFECTO**: reducir tiempo del loop.
- **MAGNITUD**: ≥2×, posiblemente 3×.
- **MECANISMO**: En AoS cada iteración lee 32 bytes pero solo usa 12 (vx,vy,vz)
  → 62% de la cache line desperdiciada. SoA usa el 100% porque solo carga
  `vx`, `vy`, `vz` arrays.
- **CONDICIÓN**: N=1M, datos no caben en L2. `-O2` para ambos.
- **MEDICIÓN**: Criterion benchmark con samples 100, warmup 3s.
- **UMBRAL**: Confirmo si SoA < AoS × 0.5. Investigo si la ratio está entre 0.5
  y 0.8.

### Napkin math

- **AoS**: 1M × 32 bytes = 32 MB total. L3 es ~25 MB → memory-bound. BW ~20
  GB/s → 32 MB / 20 GB/s = 1.6 ms.
- **SoA (solo vx,vy,vz)**: 1M × 12 bytes = 12 MB total. Cabe en L3. BW ~50 GB/s
  (L3) → 12 MB / 50 GB/s = 0.24 ms. Pero mass, x,y,z también viven en memoria,
  los frees no ocurren, así que realmente es bandwidth DRAM: ~12 MB / 20 GB/s
  = 0.6 ms.
- **Ratio esperado**: 1.6 / 0.6 ≈ 2.7×.

### Resultado esperado

Si mides 2.5-3×, confirmado. Si mides <1.5×, algo no encaja: ¿SIMD activado en
uno pero no en otro? ¿Alineación?

### Cómo haría el experimento

```c
/* bench_soa_vs_aos.c */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define N 1000000

typedef struct { float x, y, z, vx, vy, vz, mass, _pad; } Particle;

static void bench_aos(Particle *p, float *speed, int n) {
    for (int i = 0; i < n; i++) {
        speed[i] = sqrtf(p[i].vx*p[i].vx + p[i].vy*p[i].vy + p[i].vz*p[i].vz);
    }
}

static void bench_soa(float *vx, float *vy, float *vz, float *speed, int n) {
    for (int i = 0; i < n; i++) {
        speed[i] = sqrtf(vx[i]*vx[i] + vy[i]*vy[i] + vz[i]*vz[i]);
    }
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(void) {
    Particle *p = aligned_alloc(64, sizeof(Particle) * N);
    float *vx = aligned_alloc(64, sizeof(float) * N);
    float *vy = aligned_alloc(64, sizeof(float) * N);
    float *vz = aligned_alloc(64, sizeof(float) * N);
    float *speed = aligned_alloc(64, sizeof(float) * N);
    for (int i = 0; i < N; i++) {
        p[i].vx = vx[i] = (float)rand()/RAND_MAX;
        p[i].vy = vy[i] = (float)rand()/RAND_MAX;
        p[i].vz = vz[i] = (float)rand()/RAND_MAX;
    }

    /* warmup */
    bench_aos(p, speed, N);
    bench_soa(vx, vy, vz, speed, N);

    double t0 = now_sec();
    for (int r = 0; r < 100; r++) bench_aos(p, speed, N);
    double t_aos = now_sec() - t0;

    t0 = now_sec();
    for (int r = 0; r < 100; r++) bench_soa(vx, vy, vz, speed, N);
    double t_soa = now_sec() - t0;

    printf("AoS: %.3f ms/iter\n", t_aos * 1000.0 / 100);
    printf("SoA: %.3f ms/iter\n", t_soa * 1000.0 / 100);
    printf("Speedup: %.2fx\n", t_aos / t_soa);
    return 0;
}
```

Compilar con `-O2 -march=native` y ejecutar.

---

## 12. Ejemplo 2: Vec vs LinkedList

### Hipótesis

- **CAMBIO**: Reemplazar `std::list` (linked list) por `std::vector` para una
  lista de 100 K enteros.
- **EFECTO**: Reducir tiempo de iteración (suma de todos los elementos).
- **MAGNITUD**: ≥20×.
- **MECANISMO**: `std::list` tiene un nodo por elemento. En C++/glibc cada nodo
  es ~32 bytes (int + 2 pointers + overhead allocator). Nodos están dispersos
  en el heap → cada iteración causa ~1 cache miss. `std::vector` es contiguo →
  prefetcher funciona → misses cerca de 0.
- **CONDICIÓN**: 100 K elementos insertados en orden. Suma de todos.
- **MEDICIÓN**: 1000 iteraciones, criterion.
- **UMBRAL**: Confirmo si vector ≤ list / 20. Si <20×, investigar allocator
  pooling que podría haber agrupado nodos.

### Napkin math

- **Vector**: 100K × 4 bytes = 400 KB. Cabe en L2 (1MB). Acceso secuencial,
  BW L2 ~50 GB/s → 400 KB / 50 GB/s = 8 µs.
- **List**: 100K × 32 bytes = 3.2 MB (nodos) + overhead heap. No cabe en L2.
  Nodos dispersos → cada acceso es probable DRAM miss ~100 ns. 100K × 100 ns
  = 10 ms = 10 000 µs.
- **Ratio**: ~1250×? Demasiado optimista. En realidad:
  - Hardware prefetcher no ayuda con acceso indirecto.
  - Pero los nodos, si fueron creados en orden, pueden estar parcialmente
    contiguos por suerte del allocator.
  - TLB hits reducen overhead.
- **Ratio realista**: 30-50×. El prefetcher no funciona, DRAM pero algunos hits
  L3/L2 por allocator locality.

### Ejemplo de hipótesis refutada

Si mides 5×, hipótesis **refutada**. Posible causa: el allocator de tu libstdc++
usó un bloque contiguo para los primeros N nodos (lo habitual en glibc para
tamaños frecuentes). De hecho, puedes confirmarlo profileando con DHAT: los
nodos están cerca en memoria. Aprendizaje: la teoría dice "nodos dispersos"
pero la práctica depende del allocator.

Siguiente iteración: insertar nodos con otras allocations intercaladas para
forzar dispersión. Vuelves a medir, obtienes 40×, confirmado.

---

## 13. Ejemplo 3: HashMap vs BTreeMap

### Hipótesis

- **CAMBIO**: Usar `HashMap` en lugar de `BTreeMap` para lookups.
- **EFECTO**: Reducir tiempo de `get(key)`.
- **MAGNITUD**: 2-4× para N=10K, uniformly distributed keys.
- **MECANISMO**: BTreeMap es O(log N) con log2(10K) ≈ 13 comparaciones. HashMap
  es O(1) con 1 hash + 1 comparación. Por iteración: ~13 × 2 ns = 26 ns vs
  ~15 ns (hash) + 1 ns = 16 ns. Ratio teórico: 1.6×. Pero BTreeMap sufre cache
  misses en los nodos del árbol, que están dispersos por el allocator.
- **CONDICIÓN**: 10 K String keys, workload 50% hits 50% miss.
- **UMBRAL**: Confirmo si HashMap ≤ BTreeMap / 2.

### Cuándo la hipótesis se refuta

Si mides 1.2×, refutada. Posibles causas:
1. BTreeMap en Rust es cache-friendly: cada nodo contiene varios elementos
   (B-tree con fan-out 11 por default), así que solo son ~4 niveles en lugar de
   13 comparaciones saltadas.
2. El hash de String cuesta más de 15 ns para strings largos.
3. HashMap tiene overhead por colisiones y resize.

Este tipo de sorpresas son **valiosas**. Te fuerzan a refinar tu modelo mental:
"BTreeMap no es lo mismo que un binary tree clásico, fan-out alto cambia todo".

---

## 14. Ejemplo 4: malloc vs stack vs arena

### Hipótesis

- **CAMBIO**: En una función que hace `malloc(256)` al inicio y `free(buf)` al
  final, cambiar a `char buf[256]` en el stack.
- **EFECTO**: Reducir tiempo de la función.
- **MAGNITUD**: ~65 ns menos por llamada.
- **MECANISMO**: malloc+free ~65 ns. Stack allocation ~0 ns (ajuste de
  `rsp`, que es una instrucción).
- **CONDICIÓN**: Tamaño fijo 256 bytes conocido en compile time. No cruza thread
  ni sobrevive a la función.
- **UMBRAL**: Confirmo si la función es ≥50 ns más rápida.

### Cuándo importa

Si la función se llama 1 M veces en un segundo, ahorrar 65 ns = 65 ms = 6.5%
CPU. Importante.

Si se llama 10 veces por segundo, ahorras 650 ns = negligible.

**Napkin math primero**: sin volumen, no hay beneficio.

---

## 15. Ejemplo 5: branchy vs branchless

### Hipótesis

- **CAMBIO**: En un loop que cuenta elementos mayores que un threshold:
  ```c
  for (int i = 0; i < n; i++) if (data[i] > t) count++;
  ```
  cambiar a:
  ```c
  for (int i = 0; i < n; i++) count += (data[i] > t);
  ```
- **EFECTO**: Para datos aleatorios, reducir tiempo. Para datos ordenados,
  mantener o ligeramente empeorar.
- **MAGNITUD**: 2-3× para datos aleatorios, -10% a +10% para datos ordenados.
- **MECANISMO**: Con datos aleatorios, el branch acierta ~50% → mispredict rate
  50% → penalty ~10 ns por iteración perdida. Branchless elimina la rama →
  predictible siempre. Con datos ordenados, el predictor acierta casi el 100%
  → sin overhead, y añadir la suma puede ser marginalmente más caro.
- **CONDICIÓN**: Array de 1 M ints, threshold = median.
- **UMBRAL**: Confirmo si random improves ≥1.5× y sorted cambia <15%.

### Detalles técnicos

El compilador con `-O2`/`-O3` a veces ya convierte el branch en CMOV
(branchless) por su cuenta. Puedes verificarlo con `perf annotate` o
`objdump -d`: busca `cmov` en la función. Si ya hay `cmov`, el cambio manual no
hará nada. Si hay `jg`/`jl`, el cambio manual puede ayudar.

Lección: **lee el ensamblador antes de optimizar**. Puede que el compilador ya
hizo tu "optimización" por ti.

---

## 16. Ejemplo 6: SIMD vs escalar

### Hipótesis

- **CAMBIO**: Sumar dos arrays de 1M floats. Escalar → AVX2 (8 floats/instrucción).
- **EFECTO**: Reducir tiempo del loop.
- **MAGNITUD**: ~4×. No 8× por overhead de memoria y constantes.
- **MECANISMO**: AVX2 `_mm256_add_ps` hace 8 sumas en ~1 ciclo. Escalar hace 1
  suma en 1 ciclo. Teórico 8×. Pero en memory-bound (que es el caso para arrays
  grandes), el bottleneck es el bandwidth, no la ALU → la ganancia se limita a
  ~2-4×.
- **CONDICIÓN**: 1M floats, arrays alineados a 32 bytes, CPU con AVX2,
  compilado con `-O2 -mavx2`.
- **UMBRAL**: Confirmo si SIMD ≥ 3× más rápido.

### Napkin math

- **Datos**: 1M × 4 bytes × 2 arrays = 8 MB, no cabe en L2.
- **Bandwidth**: ~20 GB/s efectivo DRAM. 8 MB / 20 GB/s = 400 µs.
- **Escalar**: ALU no es bottleneck (3 GHz, 1 suma/ciclo = 3G sumas/s, 1 M sumas
  = 0.33 µs). El bottleneck es memoria → ~400 µs.
- **SIMD**: Mismo bottleneck de memoria → ~400 µs también.
- **Ratio esperado**: ~1×. **La SIMD no ayuda en memory-bound.**

Este es un resultado contraintuitivo pero muy importante. Para beneficiarse de
SIMD, el código debe ser **compute-bound**, no memory-bound. Ejemplo típico:
procesar imágenes con filtros donde cada pixel requiere muchas operaciones.

### Hipótesis refinada

Para un caso compute-bound (por ejemplo, aplicar una función trigonométrica a
cada elemento):

- **MAGNITUD**: ~4× para datos en L2, ~2× para datos en DRAM.

Tu modelo mental debe distinguir memory-bound vs compute-bound antes de predecir
el impacto de SIMD. Si no lo haces, predecirás mal.

---

## 17. Diseño experimental: variables independientes y controladas

Un experimento bien diseñado tiene **una** variable que cambias (la
**independiente**) y muchas que mantienes constantes (las **controladas**). Si
cambias dos cosas a la vez, no sabes a cuál atribuir el resultado.

### Variables que debes mantener constantes

1. **Hardware**: misma CPU, mismo governor (`performance`), misma temperatura.
2. **OS**: misma versión, misma carga del sistema.
3. **Compilador**: misma versión, mismos flags, misma libstdc++/glibc.
4. **Optimizaciones**: `-O2` o `-O3` para ambas versiones, no una en `-O0` y
   otra en `-O3`.
5. **Input**: exactamente el mismo input.
6. **Warmup**: mismo tiempo de warmup para ambas.
7. **Número de muestras**: mismo.
8. **Orden de ejecución**: idealmente aleatorizado para evitar efectos
   sistemáticos.
9. **Aislamiento**: mismo nivel (taskset, nice, sin otros procesos).

### La variable independiente

Solo **una** cosa cambia entre runs:

- Hipótesis válida: "Vec vs List, ambas con 100K ints, mismo compilador, misma
  CPU, mismo input."
- Hipótesis INVÁLIDA: "Rust Vec vs C++ list, compiladores distintos, flags
  distintos, libstdc++ distinto." Aquí cambian tantas cosas que el resultado
  no dice nada.

### Control group vs experimental group

- **Control group** (baseline): la versión actual.
- **Experimental group** (tratamiento): la versión modificada.

Mide ambos en las mismas condiciones. Idealmente intercalados: A, B, A, B, A, B,
... para evitar que temperature drift o cache state afecten sistemáticamente a
uno.

Criterion hace esto automáticamente.

---

## 18. Tamaño de muestra: cuántas iteraciones

¿Cuántas muestras necesitas para tener confianza?

Depende de la **varianza** del benchmark:

### Varianza baja (CV < 2%)

30-100 muestras son suficientes. Criterion default (100 samples) basta.

### Varianza media (CV 2-10%)

200-500 muestras. Aumenta `sample_size` en criterion.

### Varianza alta (CV > 10%)

1000+ muestras, y probablemente **hay un problema**: el sistema tiene ruido
(otros procesos, thermal throttling, GC pauses). Soluciones:

- Aislamiento con `taskset` a un core dedicado.
- `cpupower frequency-set -g performance`.
- Desactivar SMT/hyperthreading.
- Parar servicios innecesarios.
- Criar `isolcpus` en grub.

Si la varianza sigue alta después de todo eso, el benchmark **no es estable**.
Los hallazgos son poco confiables.

### Teoría: intervalo de confianza y error estándar

El error estándar de la media se reduce con √N. Para reducir el error a la
mitad, necesitas 4× más muestras. Ley de rendimientos decrecientes.

Regla práctica: busca que tu intervalo de confianza 95% sea **más estrecho** que
la diferencia que intentas detectar. Si tu hipótesis es "≥20% de mejora" y tu
IC95 es ±15%, no puedes distinguir 15% (no confirma) de 25% (confirma). Necesitas
más muestras o menor varianza.

---

## 19. Criterio de significancia estadística

Si el baseline da 100 ± 5 ms y la propuesta da 95 ± 5 ms, ¿es una mejora real o
ruido?

### Test estadístico

Un **t-test** de dos muestras compara si las medias son estadísticamente
distintas. Criterion lo hace automáticamente: reporta "Performance has improved
(p < 0.05)" o "No change".

`p < 0.05`: hay <5% de probabilidad de que la diferencia observada sea pura
casualidad. Por convención, se considera "significativo".

`p < 0.01`: más estricto. <1% probabilidad de casualidad.

### Pero significancia ≠ magnitud

Una mejora de 0.1% puede ser **estadísticamente significativa** con suficientes
muestras (si la varianza es minúscula), pero **prácticamente irrelevante**.

Al revés: una mejora de 50% puede no ser estadísticamente significativa si solo
tienes 3 muestras con varianza alta, aunque sea una mejora real.

Regla: mira **ambos**:
- **Magnitud** del efecto (effect size): ¿es >20%? ¿Importa?
- **Significancia** (p-value): ¿no es ruido?

Tu hipótesis debería incluir ambos: "≥20% de mejora con p < 0.05".

---

## 20. Qué hacer cuando la hipótesis es refutada

Si tu hipótesis era "20% mejora" y el resultado es "3% mejora":

### Paso 1: verificar que no es error

- ¿Misma versión compilada?
- ¿Mismos flags?
- ¿Input correcto?
- ¿El cambio realmente se aplicó? (a veces `cargo bench` usa código cached).

### Paso 2: profilear el "antes" y el "después"

`perf record` ambas versiones. Compara los flamegraphs.

Preguntas:
- ¿Sigue siendo la misma función el hotspot?
- ¿Hay una nueva función que apareció?
- ¿Cambió la proporción entre cache hits/misses?

### Paso 3: refinar el modelo mental

Pregúntate: "¿qué factor no consideré?". Ejemplos típicos:

- **Auto-vectorización**: el compilador ya hacía lo que pensabas era "manual".
- **Cache effect**: tu propuesta tiene peor locality.
- **Branch predictor**: tu propuesta introduce ramas nuevas.
- **Allocator**: tu propuesta hace más mallocs escondidos.
- **Overhead de abstracción**: las abstracciones no siempre se inlinean.
- **Ley de Amdahl**: la función que optimizaste no era el cuello de botella.

### Paso 4: formular nueva hipótesis

Basado en el nuevo aprendizaje, escribe una hipótesis **distinta**. Puede ser:

- "La mejora es pequeña porque el compilador ya vectorizaba. Propuesta: usar
  intrinsics manuales para forzar vector width mayor."
- "La función hot no es la que pensé. Propuesta: optimizar la que realmente
  domina."

### Paso 5: documentar

Escribe en tu lab notebook (§22):
- Hipótesis original.
- Predicción cuantitativa.
- Resultado.
- Causa de la diferencia.
- Nueva hipótesis (si aplica).

Esta documentación es lo que convierte fracaso en aprendizaje. Sin ella, en 6
meses repetirás el mismo error.

---

## 21. Confirmation bias y cómo evitarlo

**Confirmation bias**: tendemos a ver lo que esperamos ver. Si predijiste una
mejora, miras los números y te enfocas en los que la confirman, ignorando los
que la contradicen.

Manifestaciones:

1. **Cherry-picking de runs**: "Los runs 3 y 7 muestran la mejora. Los otros
   son ruido." **No**. Usa todos los runs.
2. **Cambiar la hipótesis post-hoc**: "Ah, en realidad esperaba 5%, no 20%." No,
   la hipótesis original era 20%. Escríbela antes.
3. **Benchmark favorable**: diseñar el benchmark para que muestre tu hipótesis.
   Por ejemplo, inputs artificialmente grandes donde tu optimización brilla.
4. **Ignorar empeoras**: tu cambio mejora el p50 pero empeora el p99. Enfocas
   en el p50 y olvidas el p99.

Técnicas para mitigarlo:

### 21.1. Escribe la hipótesis antes de medir

En un fichero timestamp-eado o commit. Nada de "yo esperaba eso". La hipótesis
escrita es inmutable.

### 21.2. Pre-registro

Práctica de ciencias duras: registra tu experimento (hipótesis, diseño, análisis
plan) **antes** de hacerlo. Cuando publicas, incluyes ambos.

### 21.3. Intercambiar baseline y propuesta

Mide A y B. Luego mide B y A (intercalado). Si los resultados cambian según el
orden, hay un bias temporal.

### 21.4. Alguien más revisa

Un colega lee tu hipótesis y tus resultados sin saber cuál es tu expectativa.
Pregúntale: "¿Es esto una mejora?" Si discrepa, es bias tuyo.

### 21.5. Pruebas ciegas

Nombra las versiones como "version_a" y "version_b" sin indicar cuál es el
baseline. Mide, luego descubre cuál era cuál. Difícil en solitario pero útil.

---

## 22. Documentar hipótesis: el lab notebook

Un **lab notebook** (cuaderno de laboratorio) es el registro secuencial de tus
experimentos. Cada entrada incluye:

```markdown
# 2026-04-05 - Optimización de `parse_header` en el parser HTTP

## Contexto
- Perf muestra que `parse_header` ocupa 18% del tiempo en el benchmark
  `http_bench_1k_requests`.
- El hotspot dentro de `parse_header` es la llamada a `String::from(&slice)`.

## Hipótesis
- **Cambio**: Reemplazar `String::from(&slice)` por `&str` (zero-copy slice).
- **Efecto**: Reducir tiempo de `parse_header` en ≥30%.
- **Magnitud**: 30-50%.
- **Mecanismo**: `String::from` hace `malloc + memcpy`. Para headers típicos
  (~50 bytes), es ~40 ns malloc + ~5 ns memcpy = ~45 ns. Con 20 headers por
  request, 900 ns/request solo en allocator.
- **Condición**: benchmark con 1000 requests, 20 headers cada uno, sizes
  aleatorios entre 20 y 80 bytes.
- **Medición**: criterion, 100 samples, warmup 3s, baseline guardado.
- **Umbral**: Confirmo si mejora ≥30%. Refuto si ≤20%. Entre 20-30% investigo.

## Predicción numérica
- Baseline: ~450 µs/request (medido).
- Esperado: ~300-320 µs/request.
- Total ahorro esperado: ~130-150 µs/request.

## Diseño experimental
- Rama `baseline` = master actual.
- Rama `experiment` = cambio aplicado.
- Mismo hardware (laptop A, governor performance, taskset 3).
- Criterion benchmark `http_bench`.
- 3 runs consecutivos de cada versión, intercalados.

## Resultado
- Baseline: 452 ± 8 µs/request.
- Experiment: 341 ± 7 µs/request.
- Delta: -24.6% ± 2.5%.
- p-value: <0.001 (altamente significativo).

## Análisis
- La mejora es real pero MENOR que la predicción (30-50%).
- Profilé con flamegraph: el nuevo bottleneck es `parse_content_length`, que
  hace `.parse::<usize>()` y es ahora el 15% del tiempo (antes era 8%).
- La reducción de allocations es correcta (heaptrack confirma -60% allocations
  en este path), pero el ahorro por request es menor porque:
  1. No todas las `String::from` eran del mismo path caliente.
  2. Algunas convertían a `String` para guardarlo, no puedo hacer zero-copy.

## Conclusión
- Hipótesis **parcialmente confirmada**: mejora real (24.6%) pero por debajo
  del rango predicho (30-50%).
- Nueva hipótesis: optimizar `parse_content_length` con un parser custom
  podría dar otro 5-10%.
- Merge el cambio actual, abrir nuevo ticket para la siguiente hipótesis.

## Artefactos
- commits: abc123 (baseline), def456 (experiment)
- perf.data: ~/perf/2026-04-05-http-bench/
- criterion reports: target/criterion/http_bench/
```

Esta estructura es la norma en investigación. En ingeniería es menos común pero
igual de valiosa. Un año después, puedes volver y entender exactamente qué
probaste, por qué, y qué aprendiste.

Formato ligero: un archivo markdown por experimento, en un directorio
`bench_notes/` del repo. O una wiki del proyecto. O un tag/label en tu issue
tracker.

---

## 23. El peligro de la optimización especulativa

**Optimización especulativa** = optimizar sin evidencia de que el código es un
cuello de botella.

Ejemplos reales que deberías evitar:

- "Voy a usar `SmallVec` en todas partes porque en general es más rápido."
- "Voy a evitar `Box` en esta estructura porque las indirections son caras."
- "Voy a escribir este loop en unsafe para saltar los bounds checks."

Estos cambios pueden ser neutros, negativos, o no medibles, y **aumentan la
complejidad del código**. Un principio de oro:

> **Si no puedes formular una hipótesis cuantitativa, no tienes justificación
> para optimizar.**

La optimización especulativa es uno de los antipatrones más costosos en
ingeniería: añades complejidad (mantenimiento, bugs, revisión), pierdes tiempo,
y no necesariamente ganas rendimiento. Y si ganas, nunca sabrás cuánto porque
no mediste antes.

### Antídoto

1. Profilea primero. Identifica el hotspot real.
2. Formula una hipótesis cuantitativa (con mecanismo y magnitud).
3. Mide.
4. Decide.

Si alguno de estos pasos te parece "demasiado trabajo", la optimización no vale
la pena. Esa es la señal de que estás especulando.

### Excepción: sabiduría de principios

Hay casos donde usar el "mejor" por default es razonable **sin** medición:

- Usar `Vec` en lugar de `LinkedList` por default (95% de los casos `Vec` es
  mejor).
- Usar `HashMap` en lugar de `Vec` para lookups de >100 elementos.
- Pasar `&str` por default en funciones, no `String` (evita allocations).
- Compilar con `-O2` en producción (ningún caso razonable lo justifica en `-O0`).

Estos son **defaults sensatos**, no optimizaciones. Si todo tu código usa
`Vec<T>` por default y luego hay un caso específico donde `LinkedList` es mejor
(rara), lo justificas con hipótesis y medición. Pero el default es `Vec`.

---

## 24. Plantilla de hipótesis estándar

Usa esta plantilla cada vez. Copia-pega y rellena:

```markdown
## Hipótesis: <título corto>

**Fecha**: YYYY-MM-DD
**Autor**: tu nombre
**Contexto**: <cómo llegaste a esta hipótesis; qué vista la motiva>

### Cambio propuesto
- <descripción concreta de qué modificas en el código>

### Predicción
- **Métrica**: <tiempo/throughput/memoria/otro>
- **Baseline actual**: <número con unidades>
- **Magnitud esperada**: <número, rango, dirección>
- **Dirección**: mejorar/empeorar
- **Qué percentiles**: p50, p95, p99

### Mecanismo
- <por qué esperas ese efecto; modelo de coste subyacente>

### Condiciones
- **Input**: <cuál>
- **Hardware**: <cuál>
- **Compilador/flags**: <cuáles>
- **Carga del sistema**: <cuál>

### Plan de medición
- **Herramienta**: <criterion/bench.h/iai/perf stat>
- **Muestras**: <N>
- **Warmup**: <tiempo>
- **Repeticiones**: <runs intercalados>

### Umbral de decisión
- **Confirmo si**: <condición numérica>
- **Refuto si**: <condición numérica>
- **Zona gris**: <qué investigo si cae en medio>

### Resultado esperado post-experimento
- Si confirmo: <acción>
- Si refuto: <qué buscar; nueva hipótesis>
```

Al principio parece burocrático. Después de unas pocas iteraciones, escribirlo
te tarda 5 minutos y te ahorra horas de optimización perdida.

---

## 25. Anti-patrones comunes al hipotetizar

### Anti-patrón 1: hipótesis post-hoc

Primero mides, luego escribes "la hipótesis". Hacer trampa al método. Si ves el
resultado primero, tu "hipótesis" será siempre correcta.

### Anti-patrón 2: hipótesis sin magnitud

"Será más rápido". Inútil: cualquier mejora confirma. Cualquier empeora refuta
débilmente. Ningún aprendizaje.

### Anti-patrón 3: hipótesis sin mecanismo

"Será más rápido porque probé." ¿Por qué? Sin mecanismo no puedes refinar si la
hipótesis falla.

### Anti-patrón 4: hipótesis sobre "casos raros"

Optimizas un camino que se ejecuta 0.1% del tiempo. Incluso 10× mejor ahí es
0.09% de mejora global. Amdahl te machaca.

### Anti-patrón 5: optimizar micro-benchmarks fuera de contexto

El micro-benchmark muestra 10× mejora. Pero en la aplicación real, esa función
es 2% del total → mejora global 1.8%. Los micro-benchmarks deben ser relevantes
al hot path real.

### Anti-patrón 6: no validar en producción

El benchmark local muestra 30% mejora. En producción (hardware distinto, carga
distinta, input distinto) la mejora es 2%. Siempre valida en el contexto real
cuando sea posible.

### Anti-patrón 7: optimizar lo difícil de medir

"No sé cuánto mejoró porque el benchmark es variable, pero creo que ayuda."
Si no puedes medir, no tienes hipótesis. Primero haz un benchmark estable,
después mides.

### Anti-patrón 8: escalar hallazgos

"En N=1000 mejoró 3×, así que en N=1M también mejorará 3×." Los algoritmos
cambian de comportamiento con escala (cache effects, paging, etc.). Mide en el
N relevante.

### Anti-patrón 9: obviar la varianza

"Corre en 120 ms." ¿Media? ¿p95? ¿p99? ¿CV? Un número solo es insuficiente para
comparar.

### Anti-patrón 10: ignorar trade-offs no medidos

"Mi cambio mejora latency 20%." ¿Pero qué pasó con throughput? ¿Memory? ¿Complejidad
del código? ¿Tiempo de compilación? Las optimizaciones tienen precios ocultos.
Hazlos explícitos.

---

## 26. Programa de práctica: hypothesis_lab.c

Programa que contiene **pares de funciones** para que formules y pruebes
hipótesis.

```c
/* hypothesis_lab.c — laboratorio para practicar formular hipótesis
 *
 * Compilar:
 *   gcc -O2 -g -march=native -o hypothesis_lab hypothesis_lab.c -lm
 *
 * Uso:
 *   1. ELIGE un par de funciones (ej. sum_aos vs sum_soa).
 *   2. ESCRIBE tu hipótesis en un archivo hypothesis_XX.md
 *      - cambio, efecto, magnitud, mecanismo, condición, umbral
 *   3. EJECUTA:
 *      ./hypothesis_lab <nombre_funcion>
 *   4. COMPARA la medición con tu predicción.
 *   5. DOCUMENTA en el lab notebook qué aprendiste.
 *
 * Pares disponibles:
 *   sum_aos        vs  sum_soa
 *   bsearch_std    vs  bsearch_branchless
 *   count_branchy  vs  count_branchless  (con data random)
 *   count_branchy  vs  count_branchless  (con data sorted)
 *   malloc_loop    vs  stack_loop
 *   malloc_loop    vs  arena_loop
 *   strlen_naive   vs  strlen_libc
 *   memcpy_naive   vs  memcpy_libc
 *   matmul_naive   vs  matmul_tiled
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>

#define N 1000000

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

#define BENCH(name, code) do {                                    \
    /* warmup */                                                  \
    for (int w = 0; w < 3; w++) { code; }                         \
    double t0 = now_sec();                                        \
    for (int r = 0; r < 10; r++) { code; }                        \
    double dt = (now_sec() - t0) / 10.0;                          \
    printf("%-24s  %.3f ms\n", name, dt * 1000.0);                \
} while (0)

/* ---- AoS vs SoA ---- */
typedef struct { float x, y, z, vx, vy, vz, mass, _pad; } Particle;

static void sum_aos(Particle *p, float *out, int n) {
    for (int i = 0; i < n; i++)
        out[i] = sqrtf(p[i].vx*p[i].vx + p[i].vy*p[i].vy + p[i].vz*p[i].vz);
}

static void sum_soa(float *vx, float *vy, float *vz, float *out, int n) {
    for (int i = 0; i < n; i++)
        out[i] = sqrtf(vx[i]*vx[i] + vy[i]*vy[i] + vz[i]*vz[i]);
}

/* ---- binary search: std vs branchless ---- */
static int bsearch_std(const int *arr, int n, int key) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (arr[mid] == key) return mid;
        if (arr[mid] < key) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

static int bsearch_branchless(const int *arr, int n, int key) {
    int lo = 0, len = n;
    while (len > 1) {
        int half = len / 2;
        lo += (arr[lo + half - 1] < key) * half;
        len -= half;
    }
    return (arr[lo] == key) ? lo : -1;
}

/* ---- count: branchy vs branchless ---- */
static long count_branchy(const int *data, int n, int t) {
    long c = 0;
    for (int i = 0; i < n; i++) if (data[i] > t) c++;
    return c;
}

static long count_branchless(const int *data, int n, int t) {
    long c = 0;
    for (int i = 0; i < n; i++) c += (data[i] > t);
    return c;
}

/* ---- malloc vs stack vs arena ---- */
static void malloc_loop(int n) {
    for (int i = 0; i < n; i++) {
        char *b = malloc(256);
        snprintf(b, 256, "item_%d", i);
        volatile size_t len = strlen(b);
        (void)len;
        free(b);
    }
}

static void stack_loop(int n) {
    for (int i = 0; i < n; i++) {
        char b[256];
        snprintf(b, 256, "item_%d", i);
        volatile size_t len = strlen(b);
        (void)len;
    }
}

typedef struct {
    char *buffer;
    size_t pos;
    size_t cap;
} Arena;

static void *arena_alloc(Arena *a, size_t sz) {
    if (a->pos + sz > a->cap) {
        a->pos = 0;  /* reset: estilo "linear allocator" */
    }
    void *p = a->buffer + a->pos;
    a->pos += sz;
    return p;
}

static void arena_loop(int n) {
    Arena a = { .buffer = malloc(4096), .pos = 0, .cap = 4096 };
    for (int i = 0; i < n; i++) {
        char *b = arena_alloc(&a, 256);
        snprintf(b, 256, "item_%d", i);
        volatile size_t len = strlen(b);
        (void)len;
    }
    free(a.buffer);
}

/* ---- strlen: naive vs libc ---- */
static size_t strlen_naive(const char *s) {
    size_t n = 0;
    while (*s++) n++;
    return n;
}

/* ---- memcpy: naive vs libc ---- */
static void memcpy_naive(void *dst, const void *src, size_t n) {
    char *d = dst; const char *s = src;
    while (n--) *d++ = *s++;
}

/* ---- matmul: naive vs tiled ---- */
#define M 256
static void matmul_naive(const float *A, const float *B, float *C) {
    for (int i = 0; i < M; i++)
        for (int j = 0; j < M; j++) {
            float s = 0.0f;
            for (int k = 0; k < M; k++) s += A[i*M+k] * B[k*M+j];
            C[i*M+j] = s;
        }
}

static void matmul_tiled(const float *A, const float *B, float *C) {
    const int T = 32;
    memset(C, 0, sizeof(float) * M * M);
    for (int ii = 0; ii < M; ii += T)
        for (int jj = 0; jj < M; jj += T)
            for (int kk = 0; kk < M; kk += T)
                for (int i = ii; i < ii+T; i++)
                    for (int j = jj; j < jj+T; j++) {
                        float s = C[i*M+j];
                        for (int k = kk; k < kk+T; k++) s += A[i*M+k] * B[k*M+j];
                        C[i*M+j] = s;
                    }
}

/* ---- main: setup data y dispatch ---- */
int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <test_name>\n", argv[0]);
        printf("Tests: aos_soa, bsearch, count_random, count_sorted,\n");
        printf("       alloc_compare, strlen_compare, memcpy_compare,\n");
        printf("       matmul_compare\n");
        return 1;
    }

    const char *test = argv[1];
    srand(42);

    if (strcmp(test, "aos_soa") == 0) {
        Particle *p = aligned_alloc(64, sizeof(Particle) * N);
        float *vx = aligned_alloc(64, sizeof(float) * N);
        float *vy = aligned_alloc(64, sizeof(float) * N);
        float *vz = aligned_alloc(64, sizeof(float) * N);
        float *out = aligned_alloc(64, sizeof(float) * N);
        for (int i = 0; i < N; i++) {
            p[i].vx = vx[i] = (float)rand()/RAND_MAX;
            p[i].vy = vy[i] = (float)rand()/RAND_MAX;
            p[i].vz = vz[i] = (float)rand()/RAND_MAX;
        }
        BENCH("sum_aos", sum_aos(p, out, N));
        BENCH("sum_soa", sum_soa(vx, vy, vz, out, N));
        free(p); free(vx); free(vy); free(vz); free(out);
    }
    else if (strcmp(test, "bsearch") == 0) {
        int *arr = malloc(sizeof(int) * N);
        for (int i = 0; i < N; i++) arr[i] = i * 2;
        int keys[1000];
        for (int i = 0; i < 1000; i++) keys[i] = (rand() % N) * 2;

        volatile long sink = 0;
        BENCH("bsearch_std",
              for (int i = 0; i < 1000; i++) sink += bsearch_std(arr, N, keys[i]));
        BENCH("bsearch_branchless",
              for (int i = 0; i < 1000; i++) sink += bsearch_branchless(arr, N, keys[i]));
        (void)sink;
        free(arr);
    }
    else if (strcmp(test, "count_random") == 0) {
        int *data = malloc(sizeof(int) * N);
        for (int i = 0; i < N; i++) data[i] = rand();
        volatile long sink = 0;
        BENCH("count_branchy(random)", sink += count_branchy(data, N, RAND_MAX/2));
        BENCH("count_branchless(random)", sink += count_branchless(data, N, RAND_MAX/2));
        (void)sink;
        free(data);
    }
    else if (strcmp(test, "count_sorted") == 0) {
        int *data = malloc(sizeof(int) * N);
        for (int i = 0; i < N; i++) data[i] = i;
        volatile long sink = 0;
        BENCH("count_branchy(sorted)", sink += count_branchy(data, N, N/2));
        BENCH("count_branchless(sorted)", sink += count_branchless(data, N, N/2));
        (void)sink;
        free(data);
    }
    else if (strcmp(test, "alloc_compare") == 0) {
        BENCH("malloc_loop", malloc_loop(10000));
        BENCH("stack_loop", stack_loop(10000));
        BENCH("arena_loop", arena_loop(10000));
    }
    else if (strcmp(test, "strlen_compare") == 0) {
        char *big = malloc(1 << 20);
        memset(big, 'a', (1 << 20) - 1);
        big[(1 << 20) - 1] = 0;
        volatile size_t sink = 0;
        BENCH("strlen_naive", sink += strlen_naive(big));
        BENCH("strlen_libc", sink += strlen(big));
        (void)sink;
        free(big);
    }
    else if (strcmp(test, "memcpy_compare") == 0) {
        char *src = malloc(1 << 20);
        char *dst = malloc(1 << 20);
        memset(src, 'x', 1 << 20);
        BENCH("memcpy_naive", memcpy_naive(dst, src, 1 << 20));
        BENCH("memcpy_libc", memcpy(dst, src, 1 << 20));
        free(src); free(dst);
    }
    else if (strcmp(test, "matmul_compare") == 0) {
        float *A = aligned_alloc(64, sizeof(float) * M * M);
        float *B = aligned_alloc(64, sizeof(float) * M * M);
        float *C = aligned_alloc(64, sizeof(float) * M * M);
        for (int i = 0; i < M * M; i++) { A[i] = (float)i; B[i] = (float)i; }
        BENCH("matmul_naive", matmul_naive(A, B, C));
        BENCH("matmul_tiled", matmul_tiled(A, B, C));
        free(A); free(B); free(C);
    }
    else {
        fprintf(stderr, "Unknown test: %s\n", test);
        return 1;
    }

    return 0;
}
```

### Cómo usar el programa

Para cada test:

1. **Antes de ejecutar**: en un papel o archivo, escribe tu hipótesis con
   magnitud predicha.
2. **Ejecuta**: `./hypothesis_lab <test>`.
3. **Compara**: tu predicción vs el resultado real.
4. **Documenta** qué aprendiste (incluso si acertaste).

Ejemplo de sesión:

```bash
# Test 1: AoS vs SoA
# Mi predicción: SoA será ~2.5× más rápido porque usa 3/8 cache lines en AoS.
./hypothesis_lab aos_soa
# sum_aos                   12.345 ms
# sum_soa                    4.567 ms
# Ratio: 12.345 / 4.567 = 2.70×
# ✓ Confirmado (dentro del rango esperado)

# Test 2: count con datos random (branch predictor)
# Mi predicción: branchless ~2× más rápido por evitar mispredicts al 50%.
./hypothesis_lab count_random
# count_branchy(random)      3.210 ms
# count_branchless(random)   1.234 ms
# Ratio: 2.60×
# ✓ Confirmado (incluso ligeramente mejor)

# Test 3: count con datos sorted (branch predictor acierta)
# Mi predicción: sin diferencia significativa (±10%).
./hypothesis_lab count_sorted
# count_branchy(sorted)      0.987 ms
# count_branchless(sorted)   1.012 ms
# Ratio: 1.03× (branchless es marginalmente más lento)
# ✓ Confirmado. El compilador no puede optimizar tan bien el branchless.
```

---

## 27. Ejercicios

### Ejercicio 1 — Formular hipótesis a ciegas

Sin ejecutar el programa, formula una hipótesis cuantitativa para cada test:

1. `aos_soa`
2. `bsearch`
3. `count_random`
4. `count_sorted`
5. `alloc_compare`
6. `strlen_compare`
7. `memcpy_compare`
8. `matmul_compare`

Para cada una, escribe:
- Magnitud esperada (con rango).
- Mecanismo.
- Por qué crees que ese rango es correcto.

### Ejercicio 2 — Ejecutar y comparar

Ahora ejecuta cada test y anota el resultado. Para cada uno:
- ¿Confirmó tu hipótesis?
- Si no, ¿por cuánto se desvió?
- ¿Qué factor no habías considerado?

### Ejercicio 3 — Refinar un modelo mental

Elige el test donde más te equivocaste. Investiga:
- `perf stat ./hypothesis_lab <test>`: mira IPC, cache misses, branch misses.
- Compara qué métrica hw cambió entre versiones.
- Formula una segunda hipótesis ("en realidad la diferencia es porque...") y
  verifica leyendo el ensamblador con `objdump -d`.

### Ejercicio 4 — Hipótesis sobre tu proyecto

Elige un proyecto real (en Rust o C) en el que estés trabajando. Haz un
profiling inicial con `perf` o flamegraph. Identifica un hotspot. Formula **una**
hipótesis con la plantilla del §24. Ejecuta el experimento. Documenta.

### Ejercicio 5 — Napkin math de rendimiento

Sin mirar código real, estima:

1. ¿Cuánto tarda leer 10 GB secuenciales desde SSD?
2. ¿Cuánto tarda construir un `HashMap<String, u64>` de 1 M entradas, asumiendo
   strings de 20 chars?
3. ¿Cuánto tarda un programa que llama 10 M veces a `malloc(1024)` + `free`?
4. ¿Cuánto tarda sumar en paralelo 1 G floats con 8 threads, asumiendo escalado
   ideal?
5. ¿Cuántas requests HTTP/s puede servir un servidor si cada una hace 3 mallocs
   y 1 lookup en un HashMap de 10K elementos?

Ahora verifica cada una escribiendo y midiendo.

### Ejercicio 6 — Replicar un antipatrón

Elige uno de los antipatrones del §25. Deliberadamente cae en él al analizar
`hypothesis_lab`:
- Cherry-pick un run favorable.
- O escribe la "hipótesis" después de ver los resultados.
- O ignora la varianza.

Reconoce por qué cayó en ese antipatrón y cómo lo evitarías en un proyecto real.

### Ejercicio 7 — Lab notebook completo

Escribe una entrada completa de lab notebook (§22) para uno de los tests. Incluye
todas las secciones. El objetivo no es el contenido científico sino la disciplina
de documentar.

---

## 28. Checklist del método

Antes de optimizar algo:

- [ ] Profilé y confirmé que esta función es un hotspot (>5% del tiempo total).
- [ ] Formulé una hipótesis **escrita** con cambio, efecto, magnitud, mecanismo,
      condición, medición y umbral.
- [ ] La magnitud predicha es cuantitativa y falsable.
- [ ] El mecanismo se basa en un modelo de coste concreto (cache/branch/
      allocator/pipeline/...).
- [ ] Hice napkin math para estimar el efecto antes de medir.
- [ ] Diseñé el experimento con **una sola** variable independiente.
- [ ] Controlé las variables de entorno (CPU governor, taskset, load).
- [ ] Número de muestras suficiente para la varianza observada.
- [ ] Tengo criterios claros de confirmación y refutación antes de ver resultados.

Después de medir:

- [ ] Comparé media/mediana y dispersión, no solo un número.
- [ ] Verifiqué significancia estadística (p < 0.05 si aplica).
- [ ] Si confirmado: documenté y mergeé.
- [ ] Si refutado: profilé las dos versiones para entender el porqué; formulé
      una nueva hipótesis refinada.
- [ ] Si en zona gris: reduje varianza o aumenté muestras.
- [ ] Documenté en lab notebook (hipótesis, resultado, aprendizaje).

Antes de cada ciclo:

- [ ] Pregunta: ¿sigue el hotspot identificado siendo el mismo después de mi
      cambio? Si no, el próximo hotspot es otro.

---

## 29. Navegación

| ← Anterior | ↑ Sección | Siguiente → |
|------------|-----------|-------------|
| [T04 — Profiling de memoria](../../S02_Profiling/T04_Profiling_de_memoria/README.md) | [S03 — Benchmark guiado](../) | [T02 — Benchmark comparativo](../T02_Benchmark_comparativo/README.md) |

**Capítulo 4 — Sección 3: Benchmarking Guiado por Hipótesis — Tópico 1 de 3**

Has aprendido la disciplina que convierte el profiling en ingeniería: formular
hipótesis falsables, cuantitativas, basadas en modelos mentales de hardware, y
diseñar experimentos que las confirmen o refuten. En el siguiente tópico (T02
Benchmark comparativo) aplicarás esta disciplina a un caso concreto y extenso:
comparar el mismo algoritmo en C vs Rust, AoS vs SoA, Vec vs LinkedList, y
presentar los resultados con rigor. No solo "el número", sino el contexto,
la interpretación y las conclusiones.
