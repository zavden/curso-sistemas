# T01 — Estatico vs dinamico

---

## 1. Los dos modelos de dispatch

A lo largo de S01 y S02 viste multiples mecanismos de polimorfismo.
Todos se reducen a una decision fundamental: **cuando** se elige la
funcion concreta a ejecutar.

```
  Dispatch estatico (compile time):
  El compilador sabe EXACTAMENTE que funcion llamar.
  Genera una llamada directa: call circle_area

  Dispatch dinamico (runtime):
  El compilador NO sabe que funcion llamar.
  Genera una indirection: load fn_ptr from vtable; call fn_ptr
```

Esta seccion analiza las consecuencias de cada modelo: rendimiento,
tamano de binario, flexibilidad, y cuando elegir uno sobre otro.

---

## 2. Catalogo de mecanismos

Todos los mecanismos que viste, clasificados:

### 2.1 Dispatch estatico

| Mecanismo | Lenguaje | Como funciona |
|-----------|---------|---------------|
| Llamada directa | C y Rust | `circle_area(&c)` — compilador sabe la funcion |
| Macro generadora | C | `DECLARE_STACK(int)` — genera codigo por tipo |
| `_Generic` (C11) | C | Selecciona funcion por tipo en compile time |
| Generics (`T: Trait`) | Rust | Monomorphization — copia por tipo |
| `impl Trait` (argumento) | Rust | Azucar para generics |
| `impl Trait` (retorno) | Rust | Opaque type, tipo concreto unico |
| Enum + match | Rust | Ramas, no indirection (ver seccion 4) |
| Inline function | C y Rust | Elimina la llamada completamente |

### 2.2 Dispatch dinamico

| Mecanismo | Lenguaje | Como funciona |
|-----------|---------|---------------|
| Function pointer | C | `fp(args)` — indirection via puntero |
| Vtable manual | C | `obj->vt->method(obj)` — doble indirection |
| C++ virtual | C++ | Automatico, vtable interna (vptr) |
| `dyn Trait` | Rust | Fat pointer, vtable externa |
| `Box<dyn Fn>` | Rust | Closure con dispatch dinamico |

### 2.3 Caso especial: enum

El enum de Rust merece mencion aparte. No es dispatch clasico:

```rust
match shape {
    Shape::Circle { r } => pi * r * r,
    Shape::Rect { w, h } => w * h,
}
```

El compilador genera **ramas** (branch), no indirecciones. Es mas
parecido a un `switch` optimizado que a una vtable:

```
  Vtable:  load vt → load fn → call fn    (2 indirections)
  Match:   compare tag → branch            (1 branch)
```

Un branch es generalmente mas rapido que dos loads de memoria, pero
el compilador puede optimizar ambos segun el contexto.

---

## 3. Que genera el compilador

### 3.1 Dispatch estatico: generics

```rust
fn area<T: Shape>(s: &T) -> f64 {
    s.area()
}

area(&circle);  // T = Circle
area(&rect);    // T = Rect
```

El compilador genera:

```
  ; area::<Circle>
  area_circle:
      movsd  xmm0, [rdi + 0]      ; cargar radius
      mulsd  xmm0, xmm0           ; radius * radius
      mulsd  xmm0, [PI]           ; * PI
      ret

  ; area::<Rect>
  area_rect:
      movsd  xmm0, [rdi + 0]      ; cargar width
      mulsd  xmm0, [rdi + 8]      ; * height
      ret
```

Dos funciones independientes. Llamada directa. El optimizador puede
inlinear, eliminar la llamada completamente, vectorizar, etc.

### 3.2 Dispatch dinamico: dyn Trait

```rust
fn area(s: &dyn Shape) -> f64 {
    s.area()
}
```

El compilador genera:

```
  ; area (una sola funcion)
  area_dyn:
      mov    rax, [rsi + offset]   ; cargar fn ptr de vtable
      mov    rdi, rdi              ; pasar data_ptr como &self
      call   rax                   ; llamada indirecta
      ret
```

Una sola funcion. Dos loads de memoria. Una llamada indirecta.

### 3.3 Diferencia en instrucciones

```
  Estatico:                    Dinamico:

  call circle_area             mov rax, [vtable + offset]
                               call rax

  1 instruccion                3 instrucciones
  Sin memory loads             2 memory loads
  Branch predictor: trivial    Branch predictor: dificil
  Inlineable: SI               Inlineable: NO
```

---

## 4. Rendimiento: donde importa y donde no

### 4.1 El costo real del dispatch dinamico

Dos fuentes de costo:

**1. Indirecciones de memoria (latencia)**

Cada load de memoria que no esta en cache L1 puede costar ~4ns (L2)
o ~10ns (L3). Con vtable:

```
  Load 1: vtable_ptr = obj->vt        → puede estar en cache
  Load 2: fn_ptr = vtable_ptr->area   → puede estar en cache
  Call:    fn_ptr(obj)                 → salto indirecto
```

Si la vtable y la funcion estan en cache (comun en hot loops), el costo
es ~1-2ns por llamada. Si no estan en cache (codigo con muchos tipos
distintos), puede ser ~10-20ns.

**2. Branch prediction (prediccion de salto)**

El procesador intenta predecir a donde saltara el `call rax`. Con
dispatch dinamico:

```
  Si siempre se llama al mismo tipo → predictor acierta (~0 costo)
  Si se alternan 2-3 tipos          → predictor puede acertar (~1-2ns)
  Si hay muchos tipos aleatorios    → predictor falla (~15-20ns)
```

### 4.2 Cuando NO importa

Para la **mayoria** del codigo, la diferencia es imperceptible:

```
  Operacion                  Tiempo tipico
  ─────────                  ─────────────
  Dispatch dinamico          ~2-5 ns
  Llamada a funcion          ~1-2 ns
  Allocation (malloc)        ~50-100 ns
  Acceso a disco             ~10,000 ns
  Peticion HTTP              ~10,000,000 ns

  Si tu funcion hace IO, database, o networking:
  el dispatch dinamico es <0.01% del tiempo total
```

### 4.3 Cuando SI importa

```
  Hot loop con millones de iteraciones:

  for item in million_items {
      item.process();  // esta linea se ejecuta 1,000,000 veces
  }

  Estatico: ~1ns × 1M = 1ms
  Dinamico: ~5ns × 1M = 5ms
  Diferencia: 4ms — puede importar en game loops, audio, HFT
```

Tambien importa cuando el dispatch impide **inlining**:

```rust
// Estatico: el compilador puede inlinear area()
// y luego optimizar el loop completo (vectorizar, etc.)
fn sum_areas<T: Shape>(shapes: &[T]) -> f64 {
    shapes.iter().map(|s| s.area()).sum()
    // El compilador ve TODO el codigo, optimiza como un solo bloque
}

// Dinamico: el compilador no puede ver dentro de area()
// No puede inlinear, no puede vectorizar
fn sum_areas(shapes: &[Box<dyn Shape>]) -> f64 {
    shapes.iter().map(|s| s.area()).sum()
    // Cada s.area() es una barrera de optimizacion
}
```

La perdida de inlining es frecuentemente mas costosa que el dispatch
en si.

---

## 5. Tamano de binario: monomorphization vs erasure

### 5.1 El costo de monomorphization

```rust
fn process<T: Shape>(s: &T) { /* 100 lineas de logica */ }

// Usado con 20 tipos → 20 copias de 100 lineas = 2000 lineas en binario
```

```
  Tipos usados    Copias generadas    Tamano extra (aprox)
  ────────────    ────────────────    ───────────────────
  1               1                   baseline
  5               5                   ~5x
  20              20                  ~20x
  100             100                 ~100x
```

### 5.2 El costo de type erasure

```rust
fn process(s: &dyn Shape) { /* 100 lineas de logica */ }

// Usado con 20 tipos → 1 copia + 20 vtables pequenas
// Vtable = ~5 punteros × 8 bytes = 40 bytes por tipo
// 20 vtables = 800 bytes
```

### 5.3 Tabla comparativa

| Factor | Monomorphization | Type erasure |
|--------|-----------------|-------------|
| Codigo de funciones | N copias (una por tipo) | 1 copia |
| Vtables | 0 | N vtables (pequenas) |
| Crecimiento con tipos | Lineal (significativo) | Lineal (minimo) |
| Inlining | Si | No |
| Dead code elimination | Por copia | Global |

### 5.4 Cuando importa el tamano

- **Embedded/WASM**: cada KB cuenta. Prefiere `dyn Trait` o enum.
- **Aplicaciones desktop**: rara vez importa. Usa lo que sea mas claro.
- **Librerias genericas** (como `serde`): pueden causar bloat
  significativo si monomorphizan funciones grandes.

---

## 6. Monomorphization en C

C no tiene monomorphization nativa, pero las macros generadoras la
simulan:

```c
// Macro que genera una funcion tipada
#define DEFINE_SORT(T, name)                          \
    void name(T *arr, size_t n,                       \
              int (*cmp)(const T *, const T *)) {     \
        /* ... implementacion de sort ... */          \
    }

DEFINE_SORT(int, sort_int)
DEFINE_SORT(double, sort_double)
DEFINE_SORT(Student, sort_student)
```

Esto es lo que hace Rust con generics, pero:
- En C, el programador escribe la macro y la invoca manualmente
- En Rust, el compilador genera las copias automaticamente
- En C, los errores de la macro son crpticos
- En Rust, los errores mencionan el trait faltante

El `void *` + `size_t size` de `qsort` es type erasure en C:
una sola copia de la funcion, dispatch dinamico via function pointer
comparador.

```
  C monomorphization (macro):    Rust monomorphization (generics):
  ──────────────────────────     ────────────────────────────────
  Manual (programador invoca)    Automatica (compilador genera)
  Sin type safety en la macro    Type safety via trait bounds
  Error: linea del macro         Error: "T doesn't implement Ord"

  C type erasure (void*):        Rust type erasure (dyn Trait):
  ────────────────────────       ──────────────────────────────
  Sin safety (void* casts)       Type safe (trait verificado)
  sizeof manual                  sizeof automatico (en vtable)
  Ownership manual               Ownership via Drop
```

---

## 7. Enum: el tercer camino

El enum no es ni estatico ni dinamico en el sentido clasico:

```rust
fn area(shape: &Shape) -> f64 {
    match shape {
        Shape::Circle { radius } => PI * radius.powi(2),
        Shape::Rect { width, height } => width * height,
    }
}
```

### 7.1 Que genera el compilador

```
  ; Pseudocode assembly
  area_enum:
      mov  eax, [rdi]            ; cargar tag (discriminante)
      cmp  eax, 0                ; ¿es Circle?
      je   .circle
      cmp  eax, 1                ; ¿es Rect?
      je   .rect
      ; ...
  .circle:
      ; codigo de circle inlined aqui
      ret
  .rect:
      ; codigo de rect inlined aqui
      ret
```

Todo el codigo esta **inlined** en una sola funcion. No hay
indirecciones de memoria (sin vtable), solo comparaciones de tag y
branches.

### 7.2 Comparacion de los tres

```
  Generics (estatico):
  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
  │ area::<C>() │    │ area::<R>() │    │ area::<T>() │
  │ (inlined)   │    │ (inlined)   │    │ (inlined)   │
  └─────────────┘    └─────────────┘    └─────────────┘
  N funciones. Zero-cost. Binario grande.

  dyn Trait (dinamico):
  ┌─────────────┐    ┌────────┐
  │ area_dyn()  │──→ │ vtable │ ──→ circle_area() o rect_area()
  └─────────────┘    └────────┘
  1 funcion. Indirection. Binario compacto.

  Enum (match):
  ┌──────────────────────────────────┐
  │ area_enum() {                    │
  │   tag==0? → circle code inline  │
  │   tag==1? → rect code inline    │
  │ }                                │
  └──────────────────────────────────┘
  1 funcion con branches. Todo inline. Binario medio.
```

### 7.3 Tabla comparativa

| Aspecto | Generics | dyn Trait | Enum |
|---------|----------|-----------|------|
| Indirecciones | 0 | 2 (vtable) | 0 |
| Branches | 0 | 0 | N (uno por variante) |
| Inlining | Si | No | Si (dentro del match) |
| Funciones generadas | N (por tipo) | 1 | 1 |
| Extensible | Si (nuevos tipos) | Si (nuevos tipos) | No (recompilar) |
| Cache | Cada tipo tiene su ruta | Vtable puede estar fria | Todo en una funcion |

---

## 8. Devirtualization

Los compiladores modernos (LLVM, GCC) pueden convertir dispatch
dinamico en estatico cuando detectan que solo hay un tipo posible:

```rust
fn process(s: &dyn Shape) {
    println!("{}", s.area());
}

// Si el compilador puede probar que s siempre es Circle:
let c = Circle { radius: 5.0 };
process(&c);
// LLVM puede devirtualizar: reemplazar call [vtable] por call circle_area
```

Esto se llama **devirtualization**. Funciona cuando:
- El tipo concreto es visible en el call site
- El compilador tiene suficiente informacion (LTO ayuda)
- No hay demasiados call sites con tipos distintos

En la practica, devirtualization es impredecible. No cuentes con ella
para performance — si necesitas garantia de dispatch estatico, usa
generics.

---

## 9. Criterios de decision

### 9.1 Arbol de decision completo

```
  ¿Cuantos tipos concretos existen?

  ├─ Conocidos y finitos (ej. 3-10)
  │   └─ ¿Cambian frecuentemente?
  │       ├─ NO → Enum
  │       │       Maximo rendimiento + exhaustividad
  │       └─ SI → dyn Trait o generics
  │
  ├─ Desconocidos / extensibles (plugins, user types)
  │   └─ dyn Trait
  │       Unica opcion para extensibilidad runtime
  │
  └─ Un tipo a la vez (algoritmos, contenedores)
      └─ Generics (T: Trait)
          Zero-cost + type safety
```

### 9.2 Reglas practicas

| Situacion | Recomendacion | Razon |
|-----------|--------------|-------|
| Funcion de libreria publica | Generics | Maximo flexibility para el usuario |
| Campo de struct | `Box<dyn Trait>` o enum | Generics en struct complica la API |
| Coleccion heterogenea | `Vec<Box<dyn Trait>>` o `Vec<Enum>` | Generics no permite mezclar |
| Hot loop (>1M iter) | Generics o enum | Evitar indirecciones |
| Codigo de aplicacion normal | Lo que sea mas legible | El dispatch rara vez es bottleneck |
| Embedded / WASM | `dyn Trait` o enum | Controlar tamano de binario |
| AST / parser | Enum | Variantes fijas, muchas operaciones |
| Plugin system | `dyn Trait` | Tipos no conocidos en compile time |

### 9.3 En C

En C no hay eleccion tan explicita, pero los mismos trade-offs aplican:

| Situacion | Recomendacion C | Equivalente Rust |
|-----------|----------------|-----------------|
| API generica de libreria | `void *` + `size_t` + fn ptr | `dyn Trait` |
| Tipos fijos conocidos | `enum` + `union` + `switch` | Enum |
| Rendimiento critico | Macros generadoras por tipo | Generics |
| Estructura extensible | Vtable manual (S01/T03) | `dyn Trait` |

---

## 10. Resumen visual

```
                    ┌──────────────────────────────────────┐
                    │         Polimorfismo                  │
                    └──────────┬───────────────────────────┘
                               │
              ┌────────────────┼────────────────┐
              ▼                ▼                ▼
        Estatico          Dinamico          Cerrado
     (compile time)      (runtime)       (enum/match)
              │                │                │
    ┌─────────┴──────┐   ┌────┴─────┐    ┌─────┴─────┐
    │ C: macros      │   │ C: void* │    │ C: tagged  │
    │ Rust: generics │   │ C: vtable│    │    union   │
    │ Rust: impl Tr  │   │ Rust: dyn│    │ Rust: enum │
    └────────────────┘   └──────────┘    └───────────┘
    │                    │                │
    Pro: zero-cost       Pro: extensible  Pro: exhaustivo
    Pro: inlining        Pro: compacto    Pro: stack
    Con: code bloat      Con: indirection Con: cerrado
    Con: no heterogeneo  Con: no inline   Con: recompilar
```

---

## Errores comunes

### Error 1 — Optimizar prematuramente

```rust
// "dyn Trait es lento, mejor uso generics para todo"

// Si la funcion se llama 10 veces por request HTTP:
fn handle(db: &dyn Database, req: &Request) -> Response { ... }
// El dispatch dinamico es ~5ns. El request HTTP es ~5ms.
// Optimizar el dispatch aqui es absurdo.
```

Mide antes de optimizar. El dispatch rara vez es el bottleneck.

### Error 2 — Ignorar el code bloat

```rust
// Funcion generica gigante usada con 50 tipos
fn mega_process<T: Serialize + Deserialize + Debug + Clone>(
    data: &[T],
    config: &Config,
) -> Result<Vec<T>, Error> {
    // 500 lineas de logica
}
```

50 tipos × 500 lineas = 25000 lineas en el binario. Considera el
patron outline (T02/S02, ejercicio 10): extraer la logica pesada a
una funcion `dyn` interna.

### Error 3 — Enum cuando deberia ser dyn

```rust
// Agregar un tipo requiere modificar el enum y TODOS los match
enum Shape {
    Circle(f64),
    Rect(f64, f64),
    Triangle(f64, f64, f64),
    Ellipse(f64, f64),
    Polygon(Vec<(f64, f64)>),
    // ... 20 variantes mas y creciendo
}
```

Si las variantes crecen frecuentemente, `dyn Trait` es mejor. El enum
es para conjuntos estables.

### Error 4 — dyn cuando deberia ser enum

```rust
// Solo hay 3 errores posibles y nunca cambiaran
trait AppError { fn message(&self) -> &str; }
struct IoError;
struct ParseError;
struct NotFound;

// Overkill: Vec<Box<dyn AppError>>
// Mejor: enum AppError { Io, Parse, NotFound }
```

---

## Ejercicios

### Ejercicio 1 — Clasificar mecanismos

Para cada fragmento, indica si es dispatch estatico, dinamico, o
cerrado (enum):

```rust
// A
fn f<T: Display>(x: &T) { println!("{}", x); }

// B
fn f(x: &dyn Display) { println!("{}", x); }

// C
match color {
    Color::Red => "#ff0000",
    Color::Blue => "#0000ff",
}

// D
let fp: fn(i32) -> i32 = some_function;
fp(42);

// E
fn f(x: &impl Iterator<Item = i32>) { ... }
```

<details><summary>Respuesta</summary>

| Fragmento | Tipo | Razon |
|-----------|------|-------|
| A | Estatico | Generics → monomorphization |
| B | Dinamico | `dyn Display` → vtable |
| C | Cerrado (enum) | Match sobre variantes fijas |
| D | Dinamico | Function pointer → indirection |
| E | Estatico | `impl Trait` en argumento = generics |

</details>

---

### Ejercicio 2 — Predecir codigo generado

Dado este codigo, cuantas funciones genera el compilador y como
despachan?

```rust
trait Greet {
    fn greet(&self) -> String;
}

struct English;
struct Spanish;

impl Greet for English {
    fn greet(&self) -> String { "Hello".into() }
}
impl Greet for Spanish {
    fn greet(&self) -> String { "Hola".into() }
}

fn static_greet<T: Greet>(g: &T) -> String { g.greet() }
fn dynamic_greet(g: &dyn Greet) -> String { g.greet() }

fn main() {
    let e = English;
    let s = Spanish;

    static_greet(&e);
    static_greet(&s);
    dynamic_greet(&e);
    dynamic_greet(&s);
}
```

<details><summary>Respuesta</summary>

Funciones generadas:

```
  static_greet::<English>   → call English::greet (directo)
  static_greet::<Spanish>   → call Spanish::greet (directo)
  dynamic_greet             → load vtable → call [fn_ptr] (indirecto)

  English::greet            → retorna "Hello"
  Spanish::greet            → retorna "Hola"
```

Total: **5 funciones** (2 copias de static_greet + 1 dynamic_greet +
2 implementaciones de greet).

Las dos llamadas a `dynamic_greet` usan la **misma** funcion. Las dos
llamadas a `static_greet` usan funciones **distintas** (una por tipo).

Vtables generadas: 2 (una para English, una para Spanish), usadas solo
por `dynamic_greet`.

</details>

---

### Ejercicio 3 — Medir impacto

Sin ejecutar, ordena estas operaciones de mas rapida a mas lenta:

```rust
// A: dispatch estatico (generico)
fn sum_static<T: Shape>(shapes: &[T]) -> f64 {
    shapes.iter().map(|s| s.area()).sum()
}

// B: dispatch dinamico
fn sum_dyn(shapes: &[Box<dyn Shape>]) -> f64 {
    shapes.iter().map(|s| s.area()).sum()
}

// C: enum match
fn sum_enum(shapes: &[ShapeEnum]) -> f64 {
    shapes.iter().map(|s| s.area()).sum()
}
```

Asume 1 millon de elementos, un solo tipo (Circle).

**Prediccion**: Si todos son Circle, el branch predictor acierta
siempre en C. Cambia el orden?

<details><summary>Respuesta</summary>

Orden de mas rapido a mas lento (un solo tipo, 1M elementos):

**1. A (estatico)** — El compilador genera `sum_static::<Circle>`.
La llamada a `area()` se inlinea como `PI * r * r`. El loop completo
puede vectorizarse (SIMD). Cero indirecciones, cero branches.

**2. C (enum)** — El match tiene un branch por el tag, pero con un solo
tipo el branch predictor acierta el 100%. El codigo de area esta
inlined dentro del match. Casi tan rapido como A, con un branch
predecido (costo ~0) por iteracion.

**3. B (dinamico)** — Cada `s.area()` requiere dos loads de memoria
(fat pointer → vtable → fn ptr). Aunque el branch predictor puede
predecir el indirect call (un solo tipo), la funcion no se puede
inlinear y el loop no se puede vectorizar.

Con tipos mezclados aleatoriamente:
- A no aplica (requiere un solo tipo)
- C empeora un poco (branch predictor falla con muchos tipos)
- B empeora significativamente (indirect call prediction falla)

En la practica, la diferencia entre A y B suele ser 2-5x para
operaciones triviales (como `area`). Para operaciones pesadas
(>100ns por llamada), la diferencia se diluye (<1%).

</details>

---

### Ejercicio 4 — Tamano de binario

Tienes una funcion generica de 200 lineas usada con 30 tipos.
Calcula (aproximadamente):

1. Lineas de codigo generadas con generics
2. Lineas de codigo generadas con dyn Trait
3. Tamano extra de vtables con dyn Trait

**Prediccion**: A partir de cuantos tipos conviene considerar dyn
por tamano de binario?

<details><summary>Respuesta</summary>

1. **Generics**: 200 lineas × 30 tipos = **6000 lineas** de codigo
   generado (menos si el compilador deduplica versiones identicas).

2. **dyn Trait**: **200 lineas** (una sola copia) + N funciones
   concretas (ej. 30 implementaciones de `area`, `draw`, etc.).

3. **Vtables**: Si el trait tiene 5 metodos, cada vtable = 5 punteros
   + drop + size + align = 8 punteros × 8 bytes = 64 bytes.
   30 tipos × 64 bytes = **1920 bytes** (~2 KB).

Comparacion:
- Generics: ~6000 lineas de assembly (~30-60 KB segun optimizacion)
- dyn: ~200 lineas + ~2 KB vtables (~2-4 KB total)

A partir de ~5-10 tipos, el ahorro de tamano de `dyn` se vuelve
significativo si la funcion generica es grande. Para funciones pequenas
(< 20 lineas), el bloat de generics es minimo y no vale la pena
cambiar a `dyn`.

Regla practica:
- Funcion grande (>100 lineas) con muchos tipos → considerar `dyn`
  o el patron outline
- Funcion pequena (<20 lineas) → generics siempre

</details>

---

### Ejercicio 5 — Traducir entre modelos

Dada esta funcion con dyn Trait:

```rust
fn process(items: &[Box<dyn Processor>]) {
    for item in items {
        item.validate();
        item.execute();
        item.cleanup();
    }
}
```

Reescribela con: (A) generics, (B) enum. Para cada version, indica
que se gana y que se pierde.

<details><summary>Respuesta</summary>

**A: Generics**

```rust
fn process<T: Processor>(items: &[T]) {
    for item in items {
        item.validate();
        item.execute();
        item.cleanup();
    }
}
```

Gana: zero-cost (inlining), vectorizacion posible.
Pierde: todos los items deben ser del mismo tipo. No puedes mezclar
ProcessorA y ProcessorB en un solo slice.

**B: Enum**

```rust
enum ProcessorKind {
    TypeA(ProcessorA),
    TypeB(ProcessorB),
}

impl ProcessorKind {
    fn validate(&self) { match self { /* ... */ } }
    fn execute(&self) { match self { /* ... */ } }
    fn cleanup(&self) { match self { /* ... */ } }
}

fn process(items: &[ProcessorKind]) {
    for item in items {
        item.validate();
        item.execute();
        item.cleanup();
    }
}
```

Gana: stack allocation (sin Box), exhaustividad, inlining dentro del
match, heterogeneo sin indirection.
Pierde: cerrado (agregar tipo = recompilar todo), cada metodo nuevo
requiere N branches, tamano de ProcessorKind = mayor variante.

**Original (dyn Trait):**

Gana: extensible (cualquiera puede implementar Processor), heterogeneo.
Pierde: heap allocation (Box), indirecciones por cada llamada,
no inlining.

</details>

---

### Ejercicio 6 — Equivalentes C para cada modelo

Para cada modelo Rust, escribe el equivalente en C:

```rust
// A: generics
fn max<T: Ord>(a: T, b: T) -> T { if a >= b { a } else { b } }

// B: dyn Trait
fn draw(s: &dyn Shape) { s.draw(); }

// C: enum
fn eval(e: &Expr) -> f64 { match e { ... } }
```

<details><summary>Respuesta</summary>

```c
// A: equivalente a generics → macro generadora
#define DEFINE_MAX(T, name)       \
    T name(T a, T b) {            \
        return a >= b ? a : b;    \
    }
DEFINE_MAX(int, max_int)
DEFINE_MAX(double, max_double)
// O: void* generico (type erasure)
// void *max(void *a, void *b, int (*cmp)(const void*, const void*))

// B: equivalente a dyn Trait → vtable manual
typedef struct { void (*draw)(const void *self); } ShapeVtable;
typedef struct { const ShapeVtable *vt; } Shape;

void draw(const Shape *s) {
    s->vt->draw(s);
}

// C: equivalente a enum → tagged union + switch
typedef enum { EXPR_NUM, EXPR_ADD, EXPR_MUL } ExprTag;
typedef struct Expr Expr;
struct Expr {
    ExprTag tag;
    union {
        double num;
        struct { Expr *left; Expr *right; } binop;
    };
};

double eval(const Expr *e) {
    switch (e->tag) {
        case EXPR_NUM: return e->num;
        case EXPR_ADD: return eval(e->binop.left) + eval(e->binop.right);
        case EXPR_MUL: return eval(e->binop.left) * eval(e->binop.right);
    }
    return 0;
}
```

Los mecanismos son identicos. La diferencia esta en quien hace el
trabajo: en C, el programador. En Rust, el compilador. Los trade-offs
de rendimiento y tamano son los mismos.

</details>

---

### Ejercicio 7 — Devirtualization

Indica si el compilador puede devirtualizar cada caso:

```rust
// A
let c = Circle { radius: 5.0 };
let s: &dyn Shape = &c;
s.area();

// B
fn process(s: &dyn Shape) { s.area(); }
process(&Circle { radius: 5.0 });

// C
let shapes: Vec<Box<dyn Shape>> = get_shapes_from_config();
for s in &shapes { s.area(); }

// D
fn process(s: &dyn Shape) { s.area(); }
// llamado desde 50 lugares con tipos distintos
```

<details><summary>Respuesta</summary>

| Caso | Devirtualizable | Razon |
|------|----------------|-------|
| A | Si (muy probable) | El tipo concreto es visible: `c` es `Circle` |
| B | Posible (con inlining) | Si el compilador inlinea `process`, ve que el argumento es Circle |
| C | No | Los tipos vienen de runtime (config), el compilador no puede saber |
| D | No | Multiples call sites con tipos distintos, el compilador no puede especializar |

LLVM devirtualiza A en casi todos los casos. B depende del nivel de
optimizacion y de si `process` se inlinea. C y D son inherentemente
dinamicos — ningun compilador puede resolverlos en compile time.

Moraleja: si el tipo es conocido localmente, `dyn Trait` puede ser tan
rapido como generics gracias a devirtualization. Pero no cuentes con
ello — si el rendimiento importa, usa generics explicitamente.

</details>

---

### Ejercicio 8 — Disenar una API

Estas disenando una libreria de logging con estas operaciones:
`log`, `warn`, `error`, `set_level`, `flush`.

Los usuarios pueden crear loggers custom (archivo, red, syslog).

Decide: generics, dyn Trait, o enum? Justifica.

**Prediccion**: Es logging un hot path?

<details><summary>Respuesta</summary>

**Recomendacion: `dyn Trait`.**

Razones:

1. **Los usuarios crean tipos custom** → necesita ser extensible →
   descarta enum.

2. **El logger se almacena como campo** en structs de la aplicacion
   (ej. `struct App { logger: ??? }`). Con generics, el tipo del
   logger "infecta" todo: `App<FileLogger>` ≠ `App<SyslogLogger>`.
   Con `dyn`: `App { logger: Box<dyn Logger> }` — un solo tipo.

3. **Logging NO es hot path.** Cada llamada a log hace IO (disco, red).
   El IO es ~10,000x mas lento que el dispatch dinamico. Optimizar el
   dispatch aqui seria absurdo.

4. **Se configura en runtime.** El usuario puede elegir el logger segun
   config file o env vars. Esto requiere dispatch dinamico.

```rust
trait Logger {
    fn log(&self, level: Level, msg: &str);
    fn flush(&self);
}

struct App {
    logger: Box<dyn Logger>,
    // ... otros campos
}
```

Si fuera un sistema de logging de ultra-alto rendimiento (ej. tracing
en kernel), podrias usar generics para evitar indirecciones. Pero para
el 99% de aplicaciones, `dyn Trait` es la eleccion correcta.

</details>

---

### Ejercicio 9 — Encontrar el trade-off incorrecto

Cada diseno tiene un trade-off inadecuado. Identificalo:

```rust
// A: parser de JSON con dyn Trait
trait JsonValue { fn to_string(&self) -> String; }
struct JsonNull;
struct JsonBool(bool);
struct JsonNumber(f64);
// ... cada tipo implementa JsonValue

fn parse(input: &str) -> Box<dyn JsonValue> { todo!() }

// B: game engine con enum para entidades
enum Entity {
    Player { x: f64, y: f64, health: i32 },
    Enemy { x: f64, y: f64, ai: String },
    Bullet { x: f64, y: f64, dx: f64, dy: f64 },
    // ... 50 tipos de entidades
}

// C: embedded (4KB RAM) con generics
fn process<T: Sensor>(sensor: &T) {
    // 500 lineas de procesamiento
}
// Usado con 20 tipos de sensores
```

<details><summary>Respuesta</summary>

**A: JSON con dyn Trait — deberia ser enum.**

Los tipos JSON son fijos (null, bool, number, string, array, object) y
nunca cambian. Un enum da exhaustividad, stack allocation, y mejor
rendimiento. `dyn Trait` aqui agrega allocations innecesarias y pierde
el match exhaustivo. `serde_json::Value` usa enum, no dyn.

**B: Game entities como enum — deberia ser dyn Trait (o ECS).**

50 tipos de entidades que probablemente crecen (nuevos enemigos, items,
efectos). Cada nuevo tipo requiere actualizar todos los match. Un
sistema Entity-Component-System (ECS) o al menos `dyn Entity` seria
mas mantenible. Ademas, el tamano del enum es el de la variante mas
grande — si una variante tiene mucho dato, todas las demas desperdician
espacio.

**C: Embedded con generics pesados — deberia ser dyn Trait.**

500 lineas × 20 tipos = 10000 lineas de codigo generado. En 4KB de
RAM, el binario probablemente no cabe. `dyn Trait` generaria 500 lineas
+ 20 vtables pequenas — mucho mas compacto.

</details>

---

### Ejercicio 10 — Reflexion: el triangulo de trade-offs

Responde sin ver la respuesta:

1. Resume el trade-off fundamental en una frase: que ganas y que
   pierdes al elegir estatico vs dinamico?

2. El enum es un tercer camino. En que escenario es estrictamente
   superior a ambos (generics y dyn Trait)?

3. Si tuvieras que elegir UN SOLO mecanismo para todo un proyecto
   de 50K lineas, cual elegirias y por que?

<details><summary>Respuesta</summary>

**1. Trade-off en una frase:**

Estatico: **pagas en tamano de binario, ganas en velocidad y
optimizaciones del compilador.**
Dinamico: **pagas en indirecciones y perdida de inlining, ganas en
flexibilidad y compacidad.**

O mas conciso: **informacion en compile time vs flexibilidad en
runtime.**

**2. Cuando enum es superior a ambos:**

Cuando el conjunto de variantes es **finito, estable, y cerrado**, y
necesitas **garantia de exhaustividad**.

Ejemplo: tokens de un parser, estados de una state machine, tipos de
error de una API. En estos casos:
- Generics no da exhaustividad (no verifica que manejaste todos los
  tipos)
- dyn Trait no da exhaustividad (cualquiera puede agregar un tipo)
- Enum da exhaustividad verificada por el compilador + stack allocation
  + inlining dentro del match

**3. Un solo mecanismo para 50K lineas:**

**dyn Trait.** Razones:

- Funciona para todos los casos (hot path, cold path, extensible,
  almacenable)
- El costo de rendimiento es aceptable para el 95% del codigo
- No causa code bloat
- Permite heterogeneidad
- Los pocos hot paths se pueden optimizar con generics localmente

Generics tendria problemas de code bloat y compile time. Enum seria
imposible para codigo extensible. `dyn Trait` es el "default seguro"
— no es el optimo para ningun caso, pero es aceptable para todos.

En la practica, Rust te permite mezclar los tres. El arte esta en
elegir el correcto para cada situacion, no en usar uno para todo.

</details>
