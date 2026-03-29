# T03 — Costo real del dispatch dinamico

---

## 1. Mas alla de "es lento"

T01 explico que el dispatch dinamico agrega indirecciones y dificulta
el inlining. T02 dio reglas para elegir mecanismo. Este tema va mas
profundo: **que ocurre exactamente en el hardware** cuando se ejecuta
un `call [vtable + offset]` y cuanto cuesta en nanosegundos reales.

El objetivo no es memorizar numeros, sino desarrollar intuicion:
cuando un cache miss importa, cuando el branch predictor salva la
situacion, y cuando nada de esto es relevante porque el bottleneck
esta en otro lugar.

```
  Preguntas que responde este tema:

  1. ¿Que es un cache miss y como afecta al dispatch dinamico?
  2. ¿Como predice el procesador los saltos indirectos?
  3. ¿Cuanto cuesta realmente en un benchmark controlado?
  4. ¿Cuando el dispatch NO es el bottleneck?
  5. ¿Como instrumentar y medir en tu propio codigo?
```

---

## 2. Modelo de memoria: la jerarquia de cache

Para entender el costo del dispatch dinamico, necesitas entender como
el procesador accede a la memoria.

### 2.1 La jerarquia

```
  CPU Core
  ┌────────────────────────────┐
  │  Registros     ~0.3 ns     │  ← 64-128 registros, KB
  │  ┌────────────────────┐    │
  │  │  Cache L1   ~1 ns  │    │  ← 32-64 KB (datos) + 32-64 KB (instrucciones)
  │  │  ┌──────────────┐  │    │
  │  │  │ Cache L2     │  │    │  ← 256 KB - 1 MB
  │  │  │  ~4-5 ns     │  │    │
  │  │  └──────────────┘  │    │
  │  └────────────────────┘    │
  └────────────────────────────┘
  ┌────────────────────────────┐
  │  Cache L3 (compartido)     │  ← 4-32 MB
  │  ~10-15 ns                 │
  └────────────────────────────┘
  ┌────────────────────────────┐
  │  RAM principal             │  ← 8-64 GB
  │  ~60-100 ns                │
  └────────────────────────────┘
```

### 2.2 Cache lines

La memoria no se lee byte a byte. Se lee en **cache lines** de
tipicamente 64 bytes:

```
  Pediste leer direccion 0x1000 (8 bytes)?
  El hardware carga 0x1000..0x103F (64 bytes) en cache L1.

  ┌────────────────────────────────────────┐
  │     Cache line de 64 bytes             │
  │  [dato pedido] [vecinos gratis]        │
  │  0x1000─0x1007  0x1008─0x103F          │
  └────────────────────────────────────────┘

  Si luego pides 0x1008, ya esta en cache → ~1 ns
  Si pides 0x5000, no esta en cache     → ~60-100 ns (RAM)
```

Esto tiene implicaciones directas para vtables y function pointers.

### 2.3 Localidad temporal y espacial

```
  Localidad temporal:
  Si accediste a un dato recientemente, probablemente lo haras de nuevo.
  Ejemplo: la vtable de un tipo usado en un loop → queda en cache.

  Localidad espacial:
  Si accediste a un dato, probablemente accederas a datos cercanos.
  Ejemplo: campos de un struct accedidos en secuencia → misma cache line.
```

---

## 3. Que accede a memoria el dispatch dinamico

### 3.1 Anatomia de una llamada virtual en C

```c
// shape->vt->area(shape)
```

```
  Paso 1: Cargar vt (puntero a vtable)
  ┌────────────────┐
  │ shape          │
  │ ├─ vt ─────────┼──→  Paso 2: Cargar fn ptr de vtable
  │ ├─ x           │     ┌─────────────┐
  │ └─ y           │     │ vtable      │
  └────────────────┘     │ ├─ area ────┼──→  Paso 3: Saltar a la funcion
                         │ ├─ draw     │     ┌─────────────────────┐
                         │ └─ destroy  │     │ circle_area:        │
                         └─────────────┘     │   movsd xmm0, [rdi] │
                                             │   mulsd ...          │
                                             └─────────────────────┘

  Accesos a memoria:
  1. shape->vt          → load del puntero a vtable
  2. vtable->area       → load del puntero a funcion
  3. Instrucciones de circle_area → fetch de instrucciones
  4. shape->radius (dentro de circle_area) → load del dato
```

Cada uno de esos loads puede ser un **cache hit** (~1 ns) o un
**cache miss** (~60-100 ns).

### 3.2 Anatomia de una llamada virtual en Rust (dyn Trait)

```rust
// s.area()  donde s: &dyn Shape (fat pointer)
```

```
  Fat pointer (16 bytes, en registro o stack):
  ┌──────────────┬──────────────┐
  │ data_ptr     │ vtable_ptr   │
  └──────┬───────┴──────┬───────┘
         │              │
         ▼              ▼
  ┌────────────┐  ┌─────────────┐
  │ Circle     │  │ vtable      │
  │ ├─ radius  │  │ ├─ drop     │
  └────────────┘  │ ├─ size     │
                  │ ├─ align    │
                  │ └─ area ────┼──→ circle_area()
                  └─────────────┘

  Accesos a memoria:
  1. vtable_ptr->area    → load del puntero a funcion
  2. Instrucciones       → fetch
  3. data_ptr->radius    → load del dato
```

Diferencia con C: el fat pointer ya tiene el vtable_ptr directamente
(no necesita load de shape->vt primero). Una indirection menos.

```
  C (vtable interna):          Rust (fat pointer):
  load shape->vt               (vtable_ptr ya en registro)
  load vt->area                load vtable_ptr->area
  call fn_ptr                  call fn_ptr

  3 pasos                      2 pasos
```

### 3.3 Mapa de accesos a memoria

```
  Tipo de acceso    Que se lee              Probabilidad de cache hit
  ─────────────     ──────────              ─────────────────────────
  vtable_ptr        Puntero a vtable        Alta (pocos tipos = pocas vtables)
  fn_ptr            Puntero a funcion       Alta (vtable pequena, cabe en 1 cache line)
  Instrucciones     Codigo de la funcion    Depende de cuantas funciones distintas
  Datos del objeto  Campos del struct       Depende del patron de acceso

  En un loop con 1 tipo: todo en cache → ~1-2 ns por dispatch
  En un loop con 100 tipos aleatorios: cache misses → ~20-50 ns
```

---

## 4. Branch prediction y saltos indirectos

### 4.1 Como funciona el predictor

El procesador no espera a saber a donde saltar. **Predice** el
destino y empieza a ejecutar instrucciones especulativamente:

```
  Salto directo (dispatch estatico):
  call circle_area     → destino siempre igual → prediccion trivial

  Salto indirecto (dispatch dinamico):
  call [rax]           → destino cambia segun el tipo
                       → el predictor debe adivinar
```

El **indirect branch predictor** mantiene una tabla (BTB — Branch
Target Buffer) que asocia la direccion de la instruccion `call`
con el ultimo destino observado:

```
  BTB (simplificado):
  ┌──────────────────┬────────────────────────┐
  │ Direccion call   │ Ultimo destino visto   │
  ├──────────────────┼────────────────────────┤
  │ 0x401234         │ circle_area   (0x405000) │
  │ 0x401300         │ rect_draw     (0x406000) │
  │ ...              │ ...                      │
  └──────────────────┴────────────────────────┘
```

### 4.2 Escenarios de prediccion

```
  Escenario 1: Monomorfico (un solo tipo)
  ──────────────────────────────────────
  shapes = [Circle, Circle, Circle, Circle, ...]

  El BTB ve que 0x401234 siempre salta a circle_area.
  Prediccion: 100% acierto
  Penalizacion: ~0 ns

  Escenario 2: Bimorfico (dos tipos)
  ───────────────────────────────────
  shapes = [Circle, Rect, Circle, Rect, ...]

  El BTB moderno puede rastrear 2-3 destinos por call site.
  Prediccion: 50-90% acierto (depende del patron)
  Penalizacion: ~2-5 ns promedio

  Escenario 3: Polimorfico (muchos tipos)
  ────────────────────────────────────────
  shapes = [Circle, Rect, Triangle, Ellipse, Polygon, ...]
             (aleatorio, 10+ tipos)

  El BTB no puede predecir. Mispredict en casi cada llamada.
  Prediccion: <10% acierto
  Penalizacion: ~15-25 ns por mispredict
```

### 4.3 Costo de un branch mispredict

Cuando el predictor falla, el procesador debe:

```
  1. Descartar el trabajo especulativo (pipeline flush)
     → ~10-20 ciclos desperdiciados
  2. Cargar las instrucciones correctas
     → posible cache miss adicional
  3. Re-ejecutar desde el punto correcto

  En un procesador moderno a 4 GHz:
  20 ciclos = 5 ns
  + posible miss de instrucciones = 5-15 ns adicionales

  Total: ~10-25 ns por mispredict
```

### 4.4 Comparacion con enum match

```
  Enum match (branch condicional):
  ─────────────────────────────
  cmp tag, 0        → ¿es Circle?
  je  .circle
  cmp tag, 1        → ¿es Rect?
  je  .rect

  El branch predictor de ramas condicionales es MAS PRECISO que el
  de saltos indirectos. Razon: tiene patrones binarios simples
  (taken/not taken) en lugar de N destinos posibles.

  Con patron predecible: ~0 ns
  Con patron aleatorio de 10 tipos: ~5-10 ns
  (menor que los ~15-25 ns del indirect call)
```

```
  Resumen: costo por llamada (tipico, x86-64 moderno)

                    1 tipo    3 tipos   10+ tipos
  ────────────      ───────   ────────  ──────────
  Estatico          ~0.5 ns   N/A       N/A
  Enum match        ~1 ns     ~2 ns     ~5-8 ns
  dyn Trait (Rust)  ~1-2 ns   ~3-5 ns   ~15-25 ns
  vtable (C)        ~2-3 ns   ~4-6 ns   ~15-25 ns
```

---

## 5. Cache misses en la practica

### 5.1 Patron favorable: array denso, pocos tipos

```rust
// Vec<ShapeEnum> — todo contiguo en memoria
let shapes: Vec<ShapeEnum> = vec![
    ShapeEnum::Circle(5.0),  // offset 0
    ShapeEnum::Circle(3.0),  // offset 16
    ShapeEnum::Rect(4.0, 5.0), // offset 32
    // ...
];
```

```
  Memoria:
  ┌────────┬────────┬────────┬────────┬───...
  │ Circ 0 │ Circ 1 │ Rect 0 │ Circ 2 │
  └────────┴────────┴────────┴────────┴───...
  ←──────── cache line (64 bytes) ────────→

  Varios shapes caben en una cache line.
  Iterar es rapido: prefetcher del hardware anticipa los accesos.
```

### 5.2 Patron desfavorable: Vec<Box<dyn Trait>>

```rust
// Vec<Box<dyn Shape>> — punteros a objetos dispersos en heap
let shapes: Vec<Box<dyn Shape>> = vec![
    Box::new(Circle(5.0)),     // heap alloc en 0x7f001000
    Box::new(Rect(4.0, 5.0)),  // heap alloc en 0x7f005000
    Box::new(Circle(3.0)),     // heap alloc en 0x7f009000
];
```

```
  El Vec contiene fat pointers (16 bytes cada uno):
  ┌──────────────────┬──────────────────┬──────────────────┐
  │ (0x7f001000, vt) │ (0x7f005000, vt) │ (0x7f009000, vt) │
  └────────┬─────────┴────────┬─────────┴────────┬─────────┘
           │                  │                  │
           ▼                  ▼                  ▼
     ┌──────────┐       ┌──────────┐       ┌──────────┐
     │ Circle   │       │ Rect     │       │ Circle   │
     │ (heap)   │       │ (heap)   │       │ (heap)   │
     └──────────┘       └──────────┘       └──────────┘
     0x7f001000         0x7f005000         0x7f009000

  Cada acceso al objeto = posible cache miss (direcciones dispersas)
  + acceso a vtable = otro posible miss
  + salto a funcion = otro posible miss de instrucciones
```

### 5.3 Impacto medido

```
  Benchmark: iterar 1M shapes, calcular area

  Vec<ShapeEnum>          → ~2 ms   (contiguo, branch predecible)
  Vec<Box<dyn Shape>>     → ~8 ms   (disperso, indirecciones)
  Vec<Circle> (generics)  → ~1 ms   (contiguo, inlined, SIMD posible)

  La diferencia principal NO es el dispatch en si.
  Es la LOCALIDAD DE MEMORIA:
  - Enum: datos contiguos en memoria (cache friendly)
  - Box<dyn>: datos dispersos en heap (cache hostile)
```

### 5.4 El verdadero enemigo: pointer chasing

El patron mas danino para el cache no es el dispatch dinamico en si,
sino el **pointer chasing** — seguir cadenas de punteros donde cada
uno puede causar un cache miss:

```
  Vec<Box<dyn Shape>> con Shape que contiene Box<String>:

  fat_ptr → Box → Shape { name: Box<String> → heap data }
         miss?       miss?                       miss?

  Potencialmente 3 cache misses por elemento.
  3 × 60ns × 1M = 180ms → ESTO es el bottleneck, no el dispatch.
```

---

## 6. Benchmark comparativo: C y Rust

### 6.1 Setup del benchmark

Comparamos cuatro estrategias procesando 1 millon de elementos.
Operacion: calcular el area (aritmetica trivial para aislar el
costo del dispatch).

### 6.2 Implementacion C

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define N 1000000
#define PI 3.14159265358979323846

/* ── Dispatch directo (estatico) ───────────────────────── */
typedef struct { double radius; } Circle;

double circle_area(const Circle *c) {
    return PI * c->radius * c->radius;
}

double bench_direct(const Circle *circles, size_t n) {
    double sum = 0;
    for (size_t i = 0; i < n; i++) {
        sum += circle_area(&circles[i]);
    }
    return sum;
}

/* ── Dispatch via vtable ───────────────────────────────── */
typedef struct ShapeVt ShapeVt;
typedef struct Shape Shape;

struct ShapeVt {
    double (*area)(const void *self);
    void   (*destroy)(void *self);
};

struct Shape {
    const ShapeVt *vt;
};

typedef struct { Shape base; double radius; } VCircle;
typedef struct { Shape base; double w, h; }   VRect;

double vcircle_area(const void *self) {
    const VCircle *c = self;
    return PI * c->radius * c->radius;
}

double vrect_area(const void *self) {
    const VRect *r = self;
    return r->w * r->h;
}

void noop_destroy(void *self) { (void)self; }

static const ShapeVt vcircle_vt = { vcircle_area, noop_destroy };
static const ShapeVt vrect_vt   = { vrect_area,   noop_destroy };

/* Benchmark: array de Shape* (dispersion en heap) */
double bench_vtable(Shape **shapes, size_t n) {
    double sum = 0;
    for (size_t i = 0; i < n; i++) {
        sum += shapes[i]->vt->area(shapes[i]);
    }
    return sum;
}

/* ── Dispatch via tagged union (enum en C) ─────────────── */
typedef enum { SHAPE_CIRCLE, SHAPE_RECT } ShapeTag;

typedef struct {
    ShapeTag tag;
    union {
        double radius;           /* SHAPE_CIRCLE */
        struct { double w, h; }; /* SHAPE_RECT   */
    };
} TaggedShape;

double bench_tagged(const TaggedShape *shapes, size_t n) {
    double sum = 0;
    for (size_t i = 0; i < n; i++) {
        switch (shapes[i].tag) {
        case SHAPE_CIRCLE:
            sum += PI * shapes[i].radius * shapes[i].radius;
            break;
        case SHAPE_RECT:
            sum += shapes[i].w * shapes[i].h;
            break;
        }
    }
    return sum;
}

/* ── Dispatch via function pointer (sin vtable) ────────── */
typedef struct {
    double (*area)(const void *self);
    double data[2];  /* inline storage */
} FpShape;

double bench_fp(const FpShape *shapes, size_t n) {
    double sum = 0;
    for (size_t i = 0; i < n; i++) {
        sum += shapes[i].area(&shapes[i].data);
    }
    return sum;
}

/* ── Utilidades ────────────────────────────────────────── */
double elapsed_ms(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000.0
         + (end.tv_nsec - start.tv_nsec) / 1e6;
}

int main(void) {
    struct timespec t0, t1;
    double sum;

    /* Preparar datos */
    Circle *circles = malloc(N * sizeof *circles);
    Shape **vshapes = malloc(N * sizeof *vshapes);
    TaggedShape *tagged = malloc(N * sizeof *tagged);
    FpShape *fps = malloc(N * sizeof *fps);

    for (size_t i = 0; i < N; i++) {
        double r = (double)(i % 100) + 1.0;
        circles[i] = (Circle){ .radius = r };

        VCircle *vc = malloc(sizeof *vc);
        *vc = (VCircle){ .base = { &vcircle_vt }, .radius = r };
        vshapes[i] = (Shape *)vc;

        tagged[i] = (TaggedShape){ .tag = SHAPE_CIRCLE, .radius = r };

        fps[i] = (FpShape){
            .area = vcircle_area,
            .data = { r, 0 }
        };
    }

    /* Benchmark: dispatch directo */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    sum = bench_direct(circles, N);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("Direct:  %8.3f ms  (sum=%.0f)\n", elapsed_ms(t0, t1), sum);

    /* Benchmark: vtable */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    sum = bench_vtable(vshapes, N);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("Vtable:  %8.3f ms  (sum=%.0f)\n", elapsed_ms(t0, t1), sum);

    /* Benchmark: tagged union */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    sum = bench_tagged(tagged, N);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("Tagged:  %8.3f ms  (sum=%.0f)\n", elapsed_ms(t0, t1), sum);

    /* Benchmark: function pointer inline */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    sum = bench_fp(fps, N);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("FnPtr:   %8.3f ms  (sum=%.0f)\n", elapsed_ms(t0, t1), sum);

    /* Cleanup */
    for (size_t i = 0; i < N; i++) free(vshapes[i]);
    free(circles);
    free(vshapes);
    free(tagged);
    free(fps);
    return 0;
}
```

### 6.3 Implementacion Rust

```rust
use std::time::Instant;

const N: usize = 1_000_000;

// ── Datos ──────────────────────────────────────────────
#[derive(Clone, Copy)]
struct Circle { radius: f64 }

#[derive(Clone, Copy)]
struct Rect { w: f64, h: f64 }

trait Shape {
    fn area(&self) -> f64;
}

impl Shape for Circle {
    fn area(&self) -> f64 {
        std::f64::consts::PI * self.radius * self.radius
    }
}

impl Shape for Rect {
    fn area(&self) -> f64 { self.w * self.h }
}

#[derive(Clone, Copy)]
enum ShapeEnum {
    Circle(f64),
    Rect(f64, f64),
}

impl ShapeEnum {
    fn area(&self) -> f64 {
        match self {
            ShapeEnum::Circle(r) =>
                std::f64::consts::PI * r * r,
            ShapeEnum::Rect(w, h) => w * h,
        }
    }
}

// ── Benchmarks ─────────────────────────────────────────
fn bench_generic(circles: &[Circle]) -> f64 {
    circles.iter().map(|c| c.area()).sum()
}

fn bench_dyn(shapes: &[Box<dyn Shape>]) -> f64 {
    shapes.iter().map(|s| s.area()).sum()
}

fn bench_enum(shapes: &[ShapeEnum]) -> f64 {
    shapes.iter().map(|s| s.area()).sum()
}

fn bench_dyn_ref(shapes: &[&dyn Shape]) -> f64 {
    shapes.iter().map(|s| s.area()).sum()
}

fn main() {
    let circles: Vec<Circle> = (0..N)
        .map(|i| Circle { radius: (i % 100 + 1) as f64 })
        .collect();

    let boxed: Vec<Box<dyn Shape>> = circles.iter()
        .map(|c| Box::new(*c) as Box<dyn Shape>)
        .collect();

    let enums: Vec<ShapeEnum> = circles.iter()
        .map(|c| ShapeEnum::Circle(c.radius))
        .collect();

    let refs: Vec<&dyn Shape> = circles.iter()
        .map(|c| c as &dyn Shape)
        .collect();

    // Warmup
    let _ = bench_generic(&circles);
    let _ = bench_dyn(&boxed);
    let _ = bench_enum(&enums);
    let _ = bench_dyn_ref(&refs);

    // Benchmark: generics (estatico)
    let t = Instant::now();
    let sum = bench_generic(&circles);
    let d1 = t.elapsed();
    println!("Generic: {:8.3} ms  (sum={sum:.0})", d1.as_secs_f64() * 1000.0);

    // Benchmark: dyn Trait (dinamico, disperso)
    let t = Instant::now();
    let sum = bench_dyn(&boxed);
    let d2 = t.elapsed();
    println!("Box<dyn>:{:8.3} ms  (sum={sum:.0})", d2.as_secs_f64() * 1000.0);

    // Benchmark: enum (cerrado)
    let t = Instant::now();
    let sum = bench_enum(&enums);
    let d3 = t.elapsed();
    println!("Enum:    {:8.3} ms  (sum={sum:.0})", d3.as_secs_f64() * 1000.0);

    // Benchmark: &dyn (dinamico, contiguo en circles[])
    let t = Instant::now();
    let sum = bench_dyn_ref(&refs);
    let d4 = t.elapsed();
    println!("&dyn:    {:8.3} ms  (sum={sum:.0})", d4.as_secs_f64() * 1000.0);
}
```

### 6.4 Resultados tipicos (x86-64, release, un solo tipo)

Los numeros exactos varian por hardware, pero las **proporciones**
son consistentes:

```
  Mecanismo          Tiempo     Relativo    Por que
  ─────────          ──────     ────────    ───────
  Generic (Rust)     ~1.0 ms    1.0x        Inlined, posible SIMD
  Direct (C)         ~1.0 ms    1.0x        Inlined por el compilador
  Enum (Rust)        ~1.5 ms    1.5x        Branch predecible, inlined
  Tagged union (C)   ~1.5 ms    1.5x        Switch, branch predecible
  &dyn (Rust)        ~2.5 ms    2.5x        Indirection, datos contiguos
  FnPtr inline (C)   ~3.0 ms    3.0x        Indirection, datos contiguos
  Box<dyn> (Rust)    ~6.0 ms    6.0x        Indirection + heap disperso
  Vtable heap (C)    ~7.0 ms    7.0x        Doble indirection + heap
```

Observaciones clave:

```
  1. Estatico vs dinamico: ~2.5x diferencia (cuando datos son contiguos)
  2. Contiguo vs disperso: ~2.5x diferencia (heap scatter es costoso)
  3. Enum esta entre ambos: branch predecible + datos contiguos

  El factor dominante NO es el dispatch.
  Es la LOCALIDAD DE MEMORIA (contiguo vs disperso).
```

### 6.5 Con tipos mezclados aleatoriamente

```
  Mecanismo               1 tipo    3 tipos    10 tipos
  ─────────               ──────    ───────    ────────
  Generic (Rust)          ~1.0 ms   N/A        N/A
  Enum (Rust)             ~1.5 ms   ~2.0 ms    ~3.0 ms
  &dyn (Rust)             ~2.5 ms   ~4.0 ms    ~8.0 ms
  Box<dyn> (Rust)         ~6.0 ms   ~9.0 ms    ~15.0 ms

  Con 10 tipos aleatorios:
  - Enum empeora ligeramente (branch mispredict ~5-8 ns)
  - dyn empeora mas (indirect branch mispredict ~15-25 ns)
  - Box<dyn> empeora mucho (mispredict + cache miss de datos)
```

---

## 7. Cuando el dispatch NO es el bottleneck

### 7.1 La regla del 99%

Para la mayoria del codigo, el dispatch dinamico es irrelevante
para el rendimiento:

```
  Costo de operaciones comunes:

  Operacion               Tiempo tipico   dispatch / operacion
  ─────────               ─────────────   ────────────────────
  dispatch dinamico       ~5 ns           1.0x
  Crear String            ~30 ns          0.17x
  Vec::push (realloc)     ~50 ns          0.10x
  malloc/free             ~80 ns          0.06x
  Mutex lock/unlock       ~25 ns          0.20x
  Leer 4KB de disco       ~10,000 ns      0.0005x
  Enviar paquete red      ~50,000 ns      0.0001x
  Query a base de datos   ~1,000,000 ns   0.000005x
  HTTP request            ~10,000,000 ns  0.0000005x
```

Si tu funcion virtual hace **cualquier** IO, el dispatch es ruido
estadistico.

### 7.2 Ejemplo: web server

```rust
trait Handler: Send + Sync {
    fn handle(&self, req: &Request) -> Response;
}

struct Router {
    routes: Vec<(String, Box<dyn Handler>)>,
}

impl Router {
    fn dispatch(&self, req: &Request) -> Response {
        for (path, handler) in &self.routes {
            if req.path.starts_with(path) {
                return handler.handle(req);  // dispatch dinamico ~5ns
            }
        }
        Response::not_found()
    }
}
```

```
  Tiempo de un request tipico:
  ┌─────────────────────────────────────────────────────┐
  │ TCP accept      │██                        │ 0.5 ms │
  │ Parse headers   │█                         │ 0.1 ms │
  │ Route dispatch  │                          │ 5 ns   │ ← AQUI
  │ DB query        │████████████████          │ 3.0 ms │
  │ Serialize JSON  │███                       │ 0.5 ms │
  │ TCP send        │██                        │ 0.5 ms │
  └─────────────────────────────────────────────────────┘
  Total: ~4.6 ms     dispatch: 0.0001% del total

  Optimizar el dispatch aqui es absurdo.
```

### 7.3 Ejemplo: donde SI importa

```rust
// Audio processing: 44100 samples/segundo × 2 canales
// = 88200 llamadas por segundo, deadline de ~11ms por buffer

trait AudioEffect: Send {
    fn process(&mut self, sample: f64) -> f64;
}

fn process_buffer(
    buffer: &mut [f64],
    effects: &mut [Box<dyn AudioEffect>],
) {
    for sample in buffer.iter_mut() {
        for effect in effects.iter_mut() {
            *sample = effect.process(*sample);
            // dispatch dinamico × 1024 samples × 8 effects
            // = 8192 llamadas por buffer
            // × ~5ns = ~41us por buffer → aceptable (41us << 11ms)
        }
    }
}
```

```
  Pero con 512 efectos en cadena (synthesizer):
  1024 × 512 × ~5ns = ~2.6ms por buffer
  Budget es 11ms → el dispatch consume 24% del budget
  AQUI si importa → considerar generics o enum
```

---

## 8. Como medir en tu propio codigo

### 8.1 Micro-benchmark en Rust con criterion

```rust
// Cargo.toml: [dev-dependencies] criterion = "0.5"

use criterion::{black_box, criterion_group, criterion_main, Criterion};

fn bench_dispatch(c: &mut Criterion) {
    let circles: Vec<Circle> = (0..1000)
        .map(|i| Circle { radius: i as f64 })
        .collect();

    let boxed: Vec<Box<dyn Shape>> = circles.iter()
        .map(|c| Box::new(c.clone()) as Box<dyn Shape>)
        .collect();

    let enums: Vec<ShapeEnum> = circles.iter()
        .map(|c| ShapeEnum::Circle(c.radius))
        .collect();

    c.bench_function("generic", |b| {
        b.iter(|| {
            circles.iter()
                .map(|c| black_box(c).area())
                .sum::<f64>()
        })
    });

    c.bench_function("box_dyn", |b| {
        b.iter(|| {
            boxed.iter()
                .map(|s| black_box(s).area())
                .sum::<f64>()
        })
    });

    c.bench_function("enum", |b| {
        b.iter(|| {
            enums.iter()
                .map(|s| black_box(s).area())
                .sum::<f64>()
        })
    });
}

criterion_group!(benches, bench_dispatch);
criterion_main!(benches);
```

### 8.2 Medir cache misses con perf (Linux)

```bash
# Compilar con optimizaciones
gcc -O2 -o bench bench.c -lm

# Contar cache misses
perf stat -e cache-references,cache-misses,branch-misses ./bench

# Resultado tipico:
#   1,234,567  cache-references
#     123,456  cache-misses     (10.0%)     ← alto = problema
#      12,345  branch-misses    (0.5%)      ← bajo = bien
```

```bash
# En Rust:
cargo build --release
perf stat -e cache-references,cache-misses,branch-misses \
    target/release/bench
```

### 8.3 Que buscar en perf stat

```
  Metrica             Valor sano    Valor problematico
  ───────             ──────────    ──────────────────
  cache-misses %      < 5%          > 15%
  branch-misses %     < 2%          > 10%
  IPC (inst/cycle)    > 2.0         < 1.0
  L1-dcache-misses    < 3%          > 10%

  Si cache-misses es alto:
  → El problema es localidad de memoria, no dispatch
  → Solucion: reestructurar datos (SOA, arena, inline storage)

  Si branch-misses es alto:
  → El problema es prediccion de saltos
  → Solucion: reducir variedad de tipos, usar generics/enum
```

### 8.4 Profiling detallado con perf record

```bash
# Registrar perfil detallado
perf record -g ./bench

# Ver donde se gasta el tiempo
perf report

# Anotar el codigo fuente con ciclos
perf annotate bench_vtable
```

```
  perf annotate muestra:
  ──────────────────────
       │ bench_vtable:
  0.5% │ mov  rax, [rdi + 8*rcx]    ← cargar shapes[i]
  3.2% │ mov  rdx, [rax]            ← cargar vt (posible miss)
  1.1% │ mov  rdx, [rdx]            ← cargar fn ptr
 15.7% │ call rdx                   ← indirect call (mispredict)
  0.3% │ addsd xmm0, xmm1          ← sumar al total
       │ inc  rcx
       │ cmp  rcx, rsi
       │ jne  loop

  15.7% en el call indirecto = branch mispredict es el costo dominante
  3.2% en cargar vt = algunos cache misses al acceder vtable
```

---

## 9. Tecnicas de optimizacion

### 9.1 Agrupar por tipo (sort by type)

```
  Antes (tipos mezclados):
  [C, R, C, T, R, C, T, R, C, C, T, R]
   Branch predictor falla constantemente

  Despues (agrupados por tipo):
  [C, C, C, C, C, R, R, R, R, T, T, T]
   Cada grupo: predictor acierta 100%
   Solo falla en las transiciones (3 veces, no N veces)
```

```rust
// Antes: tipos mezclados
shapes.iter().map(|s| s.area()).sum::<f64>()

// Despues: sort por tipo, luego iterar
shapes.sort_by_key(|s| std::mem::discriminant(s));
shapes.iter().map(|s| s.area()).sum::<f64>()
```

En C con vtable:

```c
// Ordenar por puntero a vtable (mismo tipo = misma vtable)
int cmp_by_vt(const void *a, const void *b) {
    const Shape *sa = *(const Shape **)a;
    const Shape *sb = *(const Shape **)b;
    return (sa->vt > sb->vt) - (sa->vt < sb->vt);
}
qsort(shapes, n, sizeof(Shape *), cmp_by_vt);
```

### 9.2 Inline storage (evitar heap scatter)

```
  Problema: Box<dyn Shape> dispersa los datos en heap.
  Solucion: almacenar los datos inline en un buffer fijo.
```

```rust
// SmallShape: inline storage para shapes pequenos
struct SmallShape {
    data: [u8; 32],  // espacio para cualquier shape
    area_fn: fn(&[u8]) -> f64,
}

impl SmallShape {
    fn new<T: Shape + Copy>(val: T) -> Self {
        assert!(std::mem::size_of::<T>() <= 32);
        let mut data = [0u8; 32];
        unsafe {
            std::ptr::write(data.as_mut_ptr() as *mut T, val);
        }
        SmallShape {
            data,
            area_fn: |bytes| unsafe {
                let t = &*(bytes.as_ptr() as *const T);
                t.area()
            },
        }
    }

    fn area(&self) -> f64 {
        (self.area_fn)(&self.data)
    }
}
```

```
  Vec<Box<dyn Shape>>:         Vec<SmallShape>:
  ┌─────┐                     ┌───────────────────────┐
  │ ptr ├──→ Circle (heap)    │ [data 32B] [fn_ptr]   │ ← contiguo
  │ ptr ├──→ Rect (heap)      │ [data 32B] [fn_ptr]   │ ← contiguo
  │ ptr ├──→ Circle (heap)    │ [data 32B] [fn_ptr]   │ ← contiguo
  └─────┘                     └───────────────────────┘
  Disperso: cache misses       Contiguo: cache friendly
```

En C, el equivalente es un tagged buffer o fat struct:

```c
typedef struct {
    double (*area)(const void *self);
    char data[32];
} InlineShape;
```

### 9.3 Arena allocator (mejorar localidad del heap)

```
  malloc normal:                Arena:
  ┌──────┐  ┌──────┐          ┌──────┬──────┬──────┬──────┐
  │ obj1 │  │ obj2 │          │ obj1 │ obj2 │ obj3 │ obj4 │
  └──────┘  └──────┘          └──────┴──────┴──────┴──────┘
  0x1000    0x5000             0x1000 0x1008 0x1010 0x1018
  (disperso)                   (contiguo)
```

```c
// Arena simple en C
typedef struct {
    char *buf;
    size_t used;
    size_t cap;
} Arena;

void *arena_alloc(Arena *a, size_t size, size_t align) {
    size_t padding = (align - (a->used % align)) % align;
    a->used += padding;
    void *ptr = a->buf + a->used;
    a->used += size;
    return ptr;
}

// Todos los shapes asignados en la arena quedan contiguos
Arena a = arena_new(N * sizeof(VCircle));
for (size_t i = 0; i < N; i++) {
    VCircle *c = arena_alloc(&a, sizeof *c, _Alignof(VCircle));
    *c = (VCircle){ .base = { &vcircle_vt }, .radius = r };
    shapes[i] = (Shape *)c;
}
```

### 9.4 Tabla de tecnicas

```
  Tecnica                Mejora                     Costo
  ───────                ──────                     ─────
  Sort by type           Branch prediction ↑↑       O(n log n) sort
  Inline storage         Localidad de datos ↑↑      Limite de tamano, unsafe
  Arena allocator        Localidad de datos ↑        Liberar todo-o-nada
  Enum en lugar de dyn   Elimina indirection        Cerrado, recompilar
  Generics               Elimina todo overhead      Sin heterogeneidad
  Devirtualization       Automatica (cuando aplica) Impredecible
  SOA (struct of arrays) Localidad por campo ↑↑↑    Complejidad de codigo
```

---

## 10. Resumen: modelo mental

```
  ┌─────────────────────────────────────────────────┐
  │         Costo del dispatch dinamico              │
  │                                                  │
  │  1. Indirection:  ~2-5 ns (si cache hit)        │
  │  2. Cache miss:   ~60-100 ns (si cache miss)    │
  │  3. Mispredict:   ~10-25 ns (si muchos tipos)   │
  │  4. No inlining:  variable (pierde SIMD, etc.)  │
  │                                                  │
  │  Factor dominante (tipico):                      │
  │  ┌──────────────────────────────────────────┐   │
  │  │  Localidad de memoria >> Branch predict  │   │
  │  │  >> Indirection pura                     │   │
  │  └──────────────────────────────────────────┘   │
  │                                                  │
  │  Reglas:                                         │
  │  • Si hace IO → dispatch es irrelevante          │
  │  • Si hot loop → medir con perf/criterion        │
  │  • Optimizar localidad ANTES que dispatch         │
  │  • Enum > &dyn > Box<dyn> en rendimiento         │
  │  • Sort by type es la optimizacion mas facil     │
  └─────────────────────────────────────────────────┘
```

---

## Errores comunes

### Error 1 — Optimizar dispatch sin medir

```rust
// "Box<dyn Trait> es lento, voy a reescribir todo con generics"

// Antes de reescribir, mide:
// perf stat -e cache-misses,branch-misses ./programa

// Si cache-misses es <5% y branch-misses es <2%:
// el dispatch NO es tu bottleneck. No reescribas.
```

Siempre mide antes de optimizar. La intuicion sobre rendimiento
es famosamente poco confiable.

### Error 2 — Culpar al dispatch cuando el problema es allocation

```rust
// "dyn Trait es lento" → en realidad Box::new es lento

// Esto NO mide dispatch, mide allocation:
for _ in 0..1_000_000 {
    let s: Box<dyn Shape> = Box::new(Circle { radius: 5.0 });
    total += s.area();
    // drop aqui → deallocation
}
// Cada iteracion: malloc + free + dispatch
// El dispatch es ~5ns, malloc+free es ~100ns → 95% del tiempo es allocation
```

Separa creation de uso al medir. Pre-crea los objetos, luego mide
el loop.

### Error 3 — Micro-benchmark sin black_box

```rust
// El compilador optimiza esto a una constante:
fn bench_bad() -> f64 {
    let c = Circle { radius: 5.0 };
    c.area()  // compilador calcula 78.54 en compile time
}

// Usa black_box para evitar que el compilador elimine el trabajo:
fn bench_good(b: &mut Bencher) {
    let c = Circle { radius: 5.0 };
    b.iter(|| black_box(&c).area())
}
```

### Error 4 — Ignorar el prefetcher

```
  "Los datos estan en un Vec, asi que son contiguos y rapidos"

  Vec<Circle>: SI — los Circle estan contiguos
  Vec<Box<dyn Shape>>: NO — el Vec contiene fat pointers contiguos,
                        pero los datos apuntan a heap disperso

  El prefetcher del hardware predice accesos secuenciales.
  Funciona con Vec<Circle> (stride fijo).
  NO funciona con Vec<Box<dyn Shape>> (stride aleatorio via punteros).
```

---

## Ejercicios

### Ejercicio 1 — Contar accesos a memoria

Para cada mecanismo, cuenta cuantos accesos a memoria (loads)
requiere una llamada virtual a `area()`:

```
A. Vtable manual en C:    shape->vt->area(shape)
B. Fat pointer en Rust:   s.area()  donde s: &dyn Shape
C. Function pointer en C: fp(shape)
D. Enum match en Rust:    match shape { ... }
```

**Prediccion**: Cual tiene mas cache misses potenciales?

<details><summary>Respuesta</summary>

| Mecanismo | Loads para hacer la llamada | Detalle |
|-----------|---------------------------|---------|
| A. Vtable C | 3 | load vt, load fn_ptr, load datos |
| B. Fat ptr Rust | 2 | load fn_ptr (vtable_ptr en registro), load datos |
| C. Fn ptr C | 2 | load fn_ptr, load datos |
| D. Enum match | 1 | load tag + datos (misma cache line) |

Cache misses potenciales:

- **A** tiene mas: tres direcciones potencialmente distintas
  (shape, vtable, funcion). Si los shapes estan dispersos en heap,
  cada uno puede causar un miss.
- **B** es mejor que A: el vtable_ptr ya esta en registro (parte
  del fat pointer), ahorra un load.
- **C** es similar a B si el fn_ptr esta inline con los datos.
- **D** tiene menos: tag y datos estan contiguos (mismo struct),
  el codigo del match esta inlined (sin salto a otra funcion).

</details>

---

### Ejercicio 2 — Predecir branch prediction

Para cada patron de tipos en un array de 1000 elementos, indica si
el branch predictor acertara (enum match) y si el indirect branch
predictor acertara (dyn Trait):

```
A. [Circle, Circle, Circle, ..., Circle]         (1 tipo)
B. [Circle, Rect, Circle, Rect, ..., Circle]     (alternante)
C. [C, C, C, C, R, R, R, R, C, C, C, C, ...]    (grupos de 4)
D. [C, R, T, E, P, C, T, R, E, P, ...]          (5 tipos aleatorio)
```

**Prediccion**: Cual patron se beneficia mas de sort-by-type?

<details><summary>Respuesta</summary>

| Patron | Enum match | dyn Trait (indirect) |
|--------|-----------|---------------------|
| A | 100% hit (siempre igual) | 100% hit (BTB memoriza 1 destino) |
| B | ~95% hit (patron regular detectable) | ~70-90% hit (BTB moderno puede rastrear 2) |
| C | ~95% hit (cambia cada 4, predecible) | ~90% hit (BTB se adapta en ~2 iteraciones) |
| D | ~50-70% hit (5 patrones, parcialmente predecible) | ~20-40% hit (BTB no puede rastrear 5 destinos) |

**D** se beneficia mas de sort-by-type. Al agrupar:
`[C,C,...,R,R,...,T,T,...,E,E,...,P,P,...]`
ambos predictores ven un solo tipo por grupo y aciertan ~100%,
con mispredicts solo en las 4 transiciones.

**A** no se beneficia nada (ya es perfecto).
**B** se beneficia moderadamente.
**C** se beneficia poco (ya tiene buena localidad temporal).

</details>

---

### Ejercicio 3 — Calcular si importa

Tu aplicacion procesa 10,000 requests HTTP por segundo. Cada request
pasa por un pipeline de 5 handlers con dispatch dinamico
(`Box<dyn Handler>`). Cada handler hace una query a base de datos
(~2ms promedio).

Calcula:

1. Tiempo total de dispatch por request
2. Tiempo total de IO por request
3. Porcentaje que representa el dispatch

**Prediccion**: Vale la pena optimizar el dispatch a generics?

<details><summary>Respuesta</summary>

1. **Dispatch**: 5 handlers × ~5 ns = **25 ns** por request

2. **IO**: 5 handlers × ~2 ms = **10 ms** por request

3. **Porcentaje**: 25 ns / 10,000,000 ns = **0.00025%**

No vale la pena optimizar el dispatch. Incluso si lo reduces a 0 ns,
el request sigue tardando 10 ms. El bottleneck es la base de datos.

Para mejorar rendimiento aqui:
- Cachear resultados de DB
- Paralelizar queries independientes
- Optimizar las queries SQL
- Usar connection pooling

Cambiar `Box<dyn Handler>` a generics agregaria complejidad al codigo
(el tipo del handler "infecta" todo el pipeline) por un ahorro de
25 ns en un request de 10 ms.

</details>

---

### Ejercicio 4 — Benchmark en C

Escribe un programa en C que mida la diferencia entre dispatch
directo y dispatch via vtable para 1 millon de iteraciones.

Requisitos:
- Usa `clock_gettime(CLOCK_MONOTONIC, ...)` para medir
- Imprime tiempo en milisegundos
- Usa `volatile` o una suma acumulada para evitar que el compilador
  elimine el loop

**Prediccion**: Cuantas veces mas lento sera vtable vs directo?

<details><summary>Respuesta</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define N 1000000
#define PI 3.14159265358979323846

typedef struct { double radius; } Circle;

double circle_area_direct(const Circle *c) {
    return PI * c->radius * c->radius;
}

typedef struct {
    double (*area)(const void *);
} Vtable;

typedef struct {
    const Vtable *vt;
    double radius;
} VCircle;

double vcircle_area(const void *self) {
    const VCircle *c = self;
    return PI * c->radius * c->radius;
}

static const Vtable vcircle_vt = { vcircle_area };

double elapsed_ms(struct timespec a, struct timespec b) {
    return (b.tv_sec - a.tv_sec) * 1000.0
         + (b.tv_nsec - a.tv_nsec) / 1e6;
}

int main(void) {
    Circle *circles = malloc(N * sizeof *circles);
    VCircle *vcircles = malloc(N * sizeof *vcircles);
    for (size_t i = 0; i < N; i++) {
        double r = (double)(i % 100) + 1.0;
        circles[i] = (Circle){ r };
        vcircles[i] = (VCircle){ &vcircle_vt, r };
    }

    struct timespec t0, t1;
    double sum;

    /* Directo */
    sum = 0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (size_t i = 0; i < N; i++)
        sum += circle_area_direct(&circles[i]);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("Direct: %.3f ms (sum=%.0f)\n", elapsed_ms(t0, t1), sum);

    /* Vtable (datos contiguos, sin heap scatter) */
    sum = 0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (size_t i = 0; i < N; i++)
        sum += vcircles[i].vt->area(&vcircles[i]);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("Vtable: %.3f ms (sum=%.0f)\n", elapsed_ms(t0, t1), sum);

    free(circles);
    free(vcircles);
    return 0;
}
```

Compilar y ejecutar:
```bash
gcc -O2 -o bench bench.c -lm
./bench
```

Resultado tipico:
```
Direct: 1.2 ms (sum=...)
Vtable: 2.8 ms (sum=...)
```

~2-3x mas lento. La diferencia viene de:
1. La indirection (load vt, load fn_ptr) evita el inlining
2. Sin inlining, el compilador no puede optimizar el loop
   (SIMD, strength reduction, etc.)

Nota: con datos contiguos (array de VCircle, no punteros a heap),
la diferencia se debe puramente al dispatch, no a cache misses.

</details>

---

### Ejercicio 5 — Interpretar perf stat

Dado este output de `perf stat`:

```
  Performance counter stats for './bench_vtable':

      5,234,567,890  cycles
      3,456,789,012  instructions          #  0.66 IPC
         98,765,432  cache-references
         23,456,789  cache-misses           # 23.75%
         12,345,678  branch-instructions
          2,345,678  branch-misses          # 19.00%

       1.312 seconds time elapsed
```

1. El IPC de 0.66 es bueno o malo? (Tipico optimo es >2.0)
2. Cache miss rate de 23.75% es alto o bajo?
3. Branch miss rate de 19% es alto o bajo?
4. Cual es el bottleneck probable?
5. Que optimizacion recomendarias primero?

<details><summary>Respuesta</summary>

1. **IPC 0.66 es malo.** Un IPC saludable es >2.0 en procesadores
   modernos (que pueden ejecutar 4-6 instrucciones por ciclo). 0.66
   indica que el procesador pasa la mayor parte del tiempo esperando
   (stalls), no ejecutando.

2. **Cache miss rate 23.75% es muy alto.** Un valor sano es <5%.
   Casi 1 de cada 4 accesos a cache falla → los datos estan muy
   dispersos en memoria.

3. **Branch miss rate 19% es muy alto.** Un valor sano es <2%.
   Indica muchos tipos distintos mezclados aleatoriamente, causando
   mispredictions en el indirect call.

4. **Bottleneck: cache misses.** El IPC bajo + cache miss alto
   indica que el procesador espera datos de RAM constantemente.
   El branch miss empeora la situacion (pipeline flush desperdicia
   trabajo especulativo).

5. **Primera optimizacion: mejorar localidad de memoria.**
   - Si usa punteros a heap → arena allocator o inline storage
   - Si mezcla tipos → sort by type (reduce branch misses tambien)
   - Si los objetos son grandes → separar hot fields (SOA)

   Sort by type atacaria ambos problemas: agrupa tipos similares
   (reduce branch misses) y mejora la localidad temporal de vtables
   (reduce cache misses de vtable). Es la optimizacion de mayor
   impacto con menor esfuerzo.

</details>

---

### Ejercicio 6 — Sort by type

Implementa sort-by-type para un `Vec<ShapeEnum>` en Rust y un
array de `Shape*` en C. Mide la diferencia con tipos mezclados
vs agrupados.

**Prediccion**: Para cuantos tipos el sort-by-type deja de dar
mejora significativa?

<details><summary>Respuesta</summary>

Rust:

```rust
use std::mem;

#[derive(Clone)]
enum ShapeEnum {
    Circle(f64),
    Rect(f64, f64),
    Triangle(f64, f64, f64),
}

fn main() {
    let mut shapes: Vec<ShapeEnum> = (0..1_000_000)
        .map(|i| match i % 3 {
            0 => ShapeEnum::Circle(i as f64),
            1 => ShapeEnum::Rect(i as f64, (i + 1) as f64),
            _ => ShapeEnum::Triangle(1.0, 2.0, 3.0),
        })
        .collect();

    // Mezcla aleatoria
    let sum_mixed: f64 = shapes.iter().map(|s| match s {
        ShapeEnum::Circle(r) => r * r,
        ShapeEnum::Rect(w, h) => w * h,
        ShapeEnum::Triangle(a, b, c) => a + b + c,
    }).sum();

    // Sort by discriminant
    shapes.sort_by_key(|s| mem::discriminant(s));

    let sum_sorted: f64 = shapes.iter().map(|s| match s {
        ShapeEnum::Circle(r) => r * r,
        ShapeEnum::Rect(w, h) => w * h,
        ShapeEnum::Triangle(a, b, c) => a + b + c,
    }).sum();

    println!("Mixed:  {sum_mixed:.0}");
    println!("Sorted: {sum_sorted:.0}");
}
```

C:

```c
int cmp_by_vt(const void *a, const void *b) {
    const Shape *sa = *(const Shape **)a;
    const Shape *sb = *(const Shape **)b;
    return (sa->vt > sb->vt) - (sa->vt < sb->vt);
}

// Antes de iterar:
qsort(shapes, n, sizeof(Shape *), cmp_by_vt);
```

Mejora tipica:

```
  2 tipos, alternados:  ~10-20% mejora (ya era parcialmente predecible)
  5 tipos, aleatorio:   ~30-50% mejora
  10+ tipos, aleatorio: ~50-70% mejora (branch misses se reducen drasticamente)
```

Sort-by-type siempre ayuda si hay >2 tipos mezclados. Con 1 tipo
no hay beneficio (ya es monomorfico). Con 2 tipos alternados el
beneficio es menor porque el predictor bimorfico moderno ya maneja
2 destinos razonablemente.

El costo del sort es O(n log n) — solo vale la pena si iteras el
array muchas veces despues del sort.

</details>

---

### Ejercicio 7 — Inline storage

Disenar en C un `InlineShape` que almacene cualquier shape en un
buffer fijo de 32 bytes, sin usar malloc:

```c
typedef struct {
    double (*area)(const void *self);
    char data[32];
} InlineShape;
```

Escribe funciones para crear InlineShape desde Circle y Rect, y
una funcion para iterar un array de InlineShape calculando areas.

**Prediccion**: Sera mas rapido o mas lento que `Shape**` con heap?

<details><summary>Respuesta</summary>

```c
#include <string.h>
#include <assert.h>
#include <math.h>

#define PI 3.14159265358979323846

typedef struct {
    double (*area)(const void *self);
    char data[32];
} InlineShape;

typedef struct { double radius; } ISCircle;
typedef struct { double w, h; } ISRect;

double is_circle_area(const void *self) {
    const ISCircle *c = self;
    return PI * c->radius * c->radius;
}

double is_rect_area(const void *self) {
    const ISRect *r = self;
    return r->w * r->h;
}

InlineShape inline_circle(double radius) {
    assert(sizeof(ISCircle) <= 32);
    InlineShape s = { .area = is_circle_area };
    ISCircle c = { radius };
    memcpy(s.data, &c, sizeof c);
    return s;
}

InlineShape inline_rect(double w, double h) {
    assert(sizeof(ISRect) <= 32);
    InlineShape s = { .area = is_rect_area };
    ISRect r = { w, h };
    memcpy(s.data, &r, sizeof r);
    return s;
}

double sum_areas(const InlineShape *shapes, size_t n) {
    double sum = 0;
    for (size_t i = 0; i < n; i++) {
        sum += shapes[i].area(shapes[i].data);
    }
    return sum;
}
```

Sera **mas rapido** que `Shape**` con heap por dos razones:

1. **Localidad**: los InlineShape estan contiguos en un array.
   Cada uno tiene 40 bytes (8 fn_ptr + 32 data). Varios caben en
   una cache line. Sin pointer chasing.

2. **Menos indirecciones**: solo 1 load (fn_ptr) en lugar de 2
   (shape_ptr → vt_ptr → fn_ptr).

Tipicamente ~2-3x mas rapido que `Shape**` con datos en heap.
Similar en velocidad a la version con vtable donde los datos ya
estan contiguos (VCircle en un array).

Limitacion: todos los datos deben caber en 32 bytes. Si un shape
necesita mas espacio, necesitas aumentar el buffer (desperdiciando
espacio para shapes pequenos) o caer a heap allocation.

</details>

---

### Ejercicio 8 — Diagnosticar un benchmark incorrecto

Este benchmark pretende comparar generics vs dyn Trait. Tiene tres
errores metodologicos. Identificalos:

```rust
fn bench() {
    let n = 100;

    // Generics
    let start = Instant::now();
    for _ in 0..n {
        let c = Circle { radius: 5.0 };
        let area = compute_generic(&c);
    }
    let t_generic = start.elapsed();

    // dyn Trait
    let start = Instant::now();
    for _ in 0..n {
        let c: Box<dyn Shape> = Box::new(Circle { radius: 5.0 });
        let area = compute_dyn(&*c);
    }
    let t_dyn = start.elapsed();

    println!("Generic: {:?}", t_generic);
    println!("Dyn:     {:?}", t_dyn);
}
```

<details><summary>Respuesta</summary>

**Error 1: n = 100 es demasiado pequeno.**

100 iteraciones toman nanosegundos. La resolucion del reloj
(`Instant::now()`) es ~20-50 ns. Con operaciones de ~2 ns, el
ruido del reloj domina la medicion. Necesitas al menos 100,000
iteraciones, o mejor aun, usar `criterion` que ajusta N
automaticamente.

**Error 2: El loop de dyn incluye allocation.**

```rust
let c: Box<dyn Shape> = Box::new(Circle { radius: 5.0 });
```

Cada iteracion hace `malloc` + `free`. El benchmark mide
allocation + dispatch, no solo dispatch. El benchmark de generics
crea en stack (sin allocation). Para comparar dispatch justo:
pre-crea los objetos fuera del loop.

**Error 3: El compilador puede eliminar el resultado.**

```rust
let area = compute_generic(&c);
// `area` no se usa → el compilador puede eliminar la llamada
```

Sin `black_box` o uso del resultado, el compilador puede optimizar
todo el loop a cero instrucciones. Usa:
```rust
std::hint::black_box(compute_generic(std::hint::black_box(&c)));
```

Benchmark correcto:

```rust
let circles: Vec<Circle> = (0..1_000_000)
    .map(|i| Circle { radius: i as f64 })
    .collect();
let boxed: Vec<Box<dyn Shape>> = circles.iter()
    .map(|c| Box::new(*c) as Box<dyn Shape>)
    .collect();

// Ahora medir solo el dispatch:
let start = Instant::now();
let sum: f64 = circles.iter()
    .map(|c| black_box(c).area())
    .sum();
black_box(sum);
let t_generic = start.elapsed();
```

</details>

---

### Ejercicio 9 — Disenar para rendimiento

Tienes un motor de particulas que procesa 100,000 particulas
60 veces por segundo. Cada particula tiene un `behavior` que
determina como se actualiza (gravedad, viento, rebote, etc.).
Hay 5 behaviors posibles.

Disena la estructura de datos optimizando para rendimiento.
Considera: enum vs dyn Trait, layout de memoria, sort by type.

**Prediccion**: Cual es el budget de tiempo por frame y por
particula?

<details><summary>Respuesta</summary>

**Budget**: 1/60 s = 16.67 ms por frame.
100,000 particulas en 16.67 ms = ~167 ns por particula.
Con 5 behaviors: ~33 ns por behavior call (promedio).

Esto esta en la zona donde el dispatch importa (~5-25 ns por call).

**Diseno recomendado: enum + sort by type.**

```rust
#[derive(Clone, Copy)]
enum Behavior {
    Gravity { strength: f32 },
    Wind { dx: f32, dy: f32 },
    Bounce { elasticity: f32 },
    Drag { coefficient: f32 },
    Attract { target_x: f32, target_y: f32 },
}

#[derive(Clone, Copy)]
struct Particle {
    x: f32,
    y: f32,
    vx: f32,
    vy: f32,
    behavior: Behavior,
}

fn update_all(particles: &mut [Particle], dt: f32) {
    // Sort by behavior type (una vez al inicio o cuando cambian)
    particles.sort_by_key(|p| std::mem::discriminant(&p.behavior));

    for p in particles.iter_mut() {
        match p.behavior {
            Behavior::Gravity { strength } => {
                p.vy += strength * dt;
            }
            Behavior::Wind { dx, dy } => {
                p.vx += dx * dt;
                p.vy += dy * dt;
            }
            // ... etc
        }
        p.x += p.vx * dt;
        p.y += p.vy * dt;
    }
}
```

Por que enum y no dyn Trait:
1. 5 behaviors fijos → enum es ideal (cerrado, conocido)
2. Particle con enum cabe en stack, contiguo en Vec
3. Match inlinea el codigo, permite SIMD
4. Sin heap allocation (Box<dyn> requiere una por particula)
5. Sort by type reduce branch misses a ~0

Un `Vec<Box<dyn Behavior>>` con 100K particulas significaria:
- 100K heap allocations
- Datos dispersos en heap → cache misses masivos
- Indirect calls sin inlining → sin SIMD

El enum es ~5-10x mas rapido aqui.

Para aun mas rendimiento (SOA — Struct of Arrays):

```rust
struct Particles {
    xs: Vec<f32>,
    ys: Vec<f32>,
    vxs: Vec<f32>,
    vys: Vec<f32>,
    behaviors: Vec<Behavior>,
}
```

SOA maximiza la localidad por campo: al actualizar posiciones,
solo xs e ys entran en cache (no se cargan behaviors). Pero la
complejidad de codigo aumenta.

</details>

---

### Ejercicio 10 — Reflexion: que aprendiste

Responde sin ver la respuesta:

1. Si te dicen "dispatch dinamico es lento", cual es tu respuesta
   calibrada?

2. En un programa nuevo sin restricciones de rendimiento, usarias
   dyn Trait libremente? Por que?

3. Nombra los tres factores de costo del dispatch dinamico,
   ordenados de mayor a menor impacto tipico.

4. Cual es la optimizacion mas facil para mejorar rendimiento de
   dispatch dinamico sin cambiar de mecanismo?

<details><summary>Respuesta</summary>

**1. Respuesta calibrada:**

"Depende del contexto. En la mayoria del codigo (~99%), el dispatch
dinamico de ~5 ns es imperceptible comparado con IO, allocation, y
logica de negocio. En hot loops con millones de iteraciones y
operaciones triviales (aritmetica), puede ser 2-5x mas lento que
generics. Pero antes de optimizar: mide. El bottleneck suele ser
la localidad de memoria, no el dispatch en si."

**2. Si, usaria dyn Trait libremente.**

En un programa sin restricciones de rendimiento, dyn Trait es la
opcion mas flexible: permite heterogeneidad, extensibilidad,
campos en structs sin generics, y codigo mas legible. El costo de
~5 ns por llamada es irrelevante en contexto de IO, UI, o logica
de negocio. Solo optimizaria a generics/enum si un profiler senala
el dispatch como bottleneck.

**3. Tres factores, mayor a menor impacto tipico:**

1. **Localidad de memoria** (cache misses) — domina cuando los
   datos estan dispersos en heap (Box<dyn>). Un solo cache miss
   cuesta 60-100 ns, mas que todo lo demas combinado.

2. **Branch prediction** (indirect call mispredict) — importa con
   muchos tipos mezclados aleatoriamente. ~15-25 ns por mispredict.

3. **Perdida de inlining** — el compilador no puede ver a traves
   del dispatch dinamico. Pierde SIMD, constant propagation,
   dead code elimination. Impacto variable pero puede ser 2-5x
   en loops numericos.

La indirection pura (load del vtable pointer) es el factor MENOS
importante (~1-2 ns si esta en cache).

**4. Sort by type.**

Agrupar los elementos por tipo antes de iterar. No cambia la
arquitectura (sigue siendo dyn Trait o enum), pero:
- Reduce branch misses (cada grupo = un solo tipo)
- Mejora localidad temporal de vtables y codigo
- Costo: O(n log n) sort, amortizado si iteras muchas veces

Es la optimizacion de mayor impacto con menor esfuerzo y cero
cambio de API.

</details>
