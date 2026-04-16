# T02 — Simple Factory en Rust

---

## 1. De C a Rust: que cambia

T01 implemento la factory en C: `shape_create()` retorna un `Shape*`
alocado con malloc y el caller debe llamar `shape_destroy()`. Los
riesgos son: NULL sin verificar, memory leaks, double-free, y cast
incorrecto.

Rust elimina todos esos riesgos a nivel de tipo:

```
  C factory:                        Rust factory:
  ─────────                         ─────────────
  Retorna Shape* (puede ser NULL)   Retorna Option<Box<dyn Shape>>
  Ownership: documentacion          Ownership: tipo (Box = owner)
  Destruccion: manual               Destruccion: automatica (Drop)
  Validacion: runtime               Validacion: compile + runtime
  Vtable: manual                    Vtable: automatica (dyn)
  Cast: unsafe, error silencioso    Sin cast (el trait system lo maneja)
```

Pero la **estructura** del patron es la misma: una funcion que
recibe un selector, crea el objeto correcto, y retorna una
interfaz comun ocultando el tipo concreto.

---

## 2. Tres formas de factory en Rust

En C, la factory siempre retorna un puntero a heap (`Shape*`).
En Rust, hay tres opciones segun lo que retornes:

```
  Forma 1: Box<dyn Trait>  → objeto en heap, tipo borrado (como C)
  Forma 2: Enum            → objeto en stack, tipo cerrado
  Forma 3: impl Trait      → objeto en stack, tipo concreto oculto

  ┌─────────────────┬───────────┬──────────┬──────────────┐
  │                 │ Box<dyn>  │ Enum     │ impl Trait   │
  ├─────────────────┼───────────┼──────────┼──────────────┤
  │ Allocation      │ Heap      │ Stack    │ Stack        │
  │ Heterogeneidad  │ Si        │ Si       │ No           │
  │ Extensible      │ Si        │ No       │ Si           │
  │ Rendimiento     │ Dinamico  │ Estatico │ Estatico     │
  │ Varios tipos    │ Si        │ Si       │ Un solo tipo │
  │ Analogia en C   │ Shape*    │ Tagged   │ Inline       │
  └─────────────────┴───────────┴──────────┴──────────────┘
```

Las tres se cubren en este tema.

---

## 3. Factory con Box<dyn Trait>

### 3.1 El trait (equivalente a ShapeVtable)

```rust
use std::f64::consts::PI;

trait Shape {
    fn type_name(&self) -> &str;
    fn area(&self) -> f64;
    fn perimeter(&self) -> f64;
}
```

### 3.2 Tipos concretos

```rust
struct Circle {
    radius: f64,
}

struct Rect {
    width: f64,
    height: f64,
}

struct Triangle {
    a: f64,
    b: f64,
    c: f64,
}

impl Shape for Circle {
    fn type_name(&self) -> &str { "Circle" }

    fn area(&self) -> f64 {
        PI * self.radius * self.radius
    }

    fn perimeter(&self) -> f64 {
        2.0 * PI * self.radius
    }
}

impl Shape for Rect {
    fn type_name(&self) -> &str { "Rect" }

    fn area(&self) -> f64 {
        self.width * self.height
    }

    fn perimeter(&self) -> f64 {
        2.0 * (self.width + self.height)
    }
}

impl Shape for Triangle {
    fn type_name(&self) -> &str { "Triangle" }

    fn area(&self) -> f64 {
        let s = (self.a + self.b + self.c) / 2.0;
        (s * (s - self.a) * (s - self.b) * (s - self.c)).sqrt()
    }

    fn perimeter(&self) -> f64 {
        self.a + self.b + self.c
    }
}
```

### 3.3 La factory

```rust
fn shape_create(kind: &str, params: &[f64]) -> Option<Box<dyn Shape>> {
    match kind {
        "circle" => {
            let radius = *params.first()?;
            Some(Box::new(Circle { radius }))
        }
        "rect" => {
            let width = *params.get(0)?;
            let height = *params.get(1)?;
            Some(Box::new(Rect { width, height }))
        }
        "triangle" => {
            let a = *params.get(0)?;
            let b = *params.get(1)?;
            let c = *params.get(2)?;
            Some(Box::new(Triangle { a, b, c }))
        }
        _ => None,
    }
}
```

### 3.4 El cliente

```rust
fn main() {
    let specs = [
        ("circle",   vec![5.0]),
        ("rect",     vec![3.0, 4.0]),
        ("triangle", vec![3.0, 4.0, 5.0]),
    ];

    let shapes: Vec<Box<dyn Shape>> = specs.iter()
        .filter_map(|(kind, params)| shape_create(kind, params))
        .collect();

    for s in &shapes {
        println!("{:<10} area={:.2}  perim={:.2}",
                 s.type_name(), s.area(), s.perimeter());
    }
    // shapes se destruye automaticamente aqui (Drop)
}
```

```
  Output:
  Circle     area=78.54  perim=31.42
  Rect       area=12.00  perim=14.00
  Triangle   area=6.00   perim=12.00
```

### 3.5 Comparacion directa con T01

```
  C (T01):                              Rust:
  ──────                                ─────
  Shape *s = shape_create(              let s = shape_create(
      SHAPE_CIRCLE,                         "circle",
      (double[]){5.0}, 1);                 &[5.0]);

  if (s == NULL) { /* error */ }        // s es Option, compilador fuerza check

  shape_area(s);                        s.area();        // sin unwrap si ya checked
  shape_destroy(s);                     // automatico al salir del scope

  // Olvidar destroy → leak             // Imposible olvidar: Drop es automatico
  // Double-free → UB                   // Imposible: ownership move
  // Usar despues de destroy → UB       // Imposible: borrow checker
```

---

## 4. Factory con enum

Para un conjunto cerrado de tipos, un enum es mas eficiente que
`Box<dyn Trait>` (sin heap, sin indirecciones).

### 4.1 Definir el enum

```rust
enum ShapeEnum {
    Circle { radius: f64 },
    Rect { width: f64, height: f64 },
    Triangle { a: f64, b: f64, c: f64 },
}

impl ShapeEnum {
    fn type_name(&self) -> &str {
        match self {
            ShapeEnum::Circle { .. }   => "Circle",
            ShapeEnum::Rect { .. }     => "Rect",
            ShapeEnum::Triangle { .. } => "Triangle",
        }
    }

    fn area(&self) -> f64 {
        match self {
            ShapeEnum::Circle { radius } =>
                PI * radius * radius,
            ShapeEnum::Rect { width, height } =>
                width * height,
            ShapeEnum::Triangle { a, b, c } => {
                let s = (a + b + c) / 2.0;
                (s * (s - a) * (s - b) * (s - c)).sqrt()
            }
        }
    }

    fn perimeter(&self) -> f64 {
        match self {
            ShapeEnum::Circle { radius } =>
                2.0 * PI * radius,
            ShapeEnum::Rect { width, height } =>
                2.0 * (width + height),
            ShapeEnum::Triangle { a, b, c } =>
                a + b + c,
        }
    }
}
```

### 4.2 La factory con enum

```rust
fn shape_create_enum(kind: &str, params: &[f64]) -> Option<ShapeEnum> {
    match kind {
        "circle" => {
            let radius = *params.first()?;
            Some(ShapeEnum::Circle { radius })
        }
        "rect" => {
            let width = *params.get(0)?;
            let height = *params.get(1)?;
            Some(ShapeEnum::Rect { width, height })
        }
        "triangle" => {
            let a = *params.get(0)?;
            let b = *params.get(1)?;
            let c = *params.get(2)?;
            Some(ShapeEnum::Triangle { a, b, c })
        }
        _ => None,
    }
}
```

### 4.3 Comparacion Box<dyn> vs Enum

```
  Box<dyn Shape>:                     ShapeEnum:
  ┌──────────┬──────────┐            ┌──────────────────────┐
  │ data_ptr │ vtable   │──→ heap    │ tag │ radius/w/h/abc │
  └──────────┴──────────┘            └──────────────────────┘
  16 bytes + heap alloc               32 bytes inline (stack)

  Box<dyn>:  extensible, heap alloc, dispatch dinamico
  Enum:      cerrado, sin alloc, match inlined, exhaustivo
```

```
  Cuando usar cada uno:

  Box<dyn Shape>:
  - Los tipos se agregan sin recompilar (plugins)
  - El trait es de otro crate
  - Necesitas heterogeneidad con extensibilidad

  ShapeEnum:
  - Tipos conocidos y fijos
  - Rendimiento critico (sin heap, sin indirection)
  - Quieres exhaustive matching
```

---

## 5. Factory con impl Trait

Cuando la factory retorna **un solo tipo** en cada rama (pero el
caller no necesita saber cual), puedes usar `impl Trait`:

```rust
// ATENCION: esto NO compila si las ramas retornan tipos distintos
fn shape_create_impl(kind: &str) -> impl Shape {
    Circle { radius: 5.0 }  // solo puede retornar UN tipo
}
```

```
  Error si intentas retornar distintos tipos:

  fn bad_factory(kind: &str) -> impl Shape {
      match kind {
          "circle" => Circle { radius: 5.0 },
          //         ^^^^^^^^^^^^^^^^^^^^^^^ tipo: Circle
          "rect" => Rect { width: 3.0, height: 4.0 },
          //        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ tipo: Rect
          //        ERROR: match arms have incompatible types
      }
  }
```

### 5.1 Donde si funciona impl Trait

```rust
// Factory parametrizada: cada llamada retorna un solo tipo
fn circle_factory(radius: f64) -> impl Shape {
    Circle { radius }
}

fn rect_factory(w: f64, h: f64) -> impl Shape {
    Rect { width: w, height: h }
}

// El caller no sabe que tipo es — solo sabe que implementa Shape
let s = circle_factory(5.0);
println!("area = {:.2}", s.area());
// s es Circle, pero el caller no lo sabe ni le importa
```

### 5.2 impl Trait + generics

```rust
// Factory generica: crea cualquier Shape con defaults
trait ShapeDefault: Shape {
    fn default_instance() -> Self;
}

impl ShapeDefault for Circle {
    fn default_instance() -> Self {
        Circle { radius: 1.0 }
    }
}

impl ShapeDefault for Rect {
    fn default_instance() -> Self {
        Rect { width: 1.0, height: 1.0 }
    }
}

fn create_default<T: ShapeDefault>() -> T {
    T::default_instance()
}

// Uso: el caller elige el tipo
let c: Circle = create_default();
let r: Rect = create_default();
```

```
  impl Trait como retorno:
  - La FUNCION elige que tipo retornar (pero siempre el mismo)
  - El caller no sabe el tipo

  Generics como retorno:
  - El CALLER elige que tipo quiere
  - La funcion es parametrica

  Analogia C:
  - impl Trait ≈ opaque pointer (FILE*)
  - Generics ≈ macro que se instancia para cada tipo
```

---

## 6. Factory con validacion y Result

En T01, la factory C retornaba NULL o un codigo de error. En Rust,
usamos `Result` para errores ricos:

### 6.1 Definir errores

```rust
#[derive(Debug)]
enum FactoryError {
    UnknownType(String),
    MissingParams { expected: usize, got: usize },
    InvalidParam(String),
}

impl std::fmt::Display for FactoryError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            FactoryError::UnknownType(t) =>
                write!(f, "unknown shape type: {t}"),
            FactoryError::MissingParams { expected, got } =>
                write!(f, "expected {expected} params, got {got}"),
            FactoryError::InvalidParam(msg) =>
                write!(f, "invalid parameter: {msg}"),
        }
    }
}

impl std::error::Error for FactoryError {}
```

### 6.2 Factory con Result

```rust
fn shape_create(
    kind: &str,
    params: &[f64],
) -> Result<Box<dyn Shape>, FactoryError> {
    match kind {
        "circle" => {
            if params.len() < 1 {
                return Err(FactoryError::MissingParams {
                    expected: 1, got: params.len(),
                });
            }
            let radius = params[0];
            if radius <= 0.0 {
                return Err(FactoryError::InvalidParam(
                    format!("radius must be > 0, got {radius}"),
                ));
            }
            Ok(Box::new(Circle { radius }))
        }
        "rect" => {
            if params.len() < 2 {
                return Err(FactoryError::MissingParams {
                    expected: 2, got: params.len(),
                });
            }
            let (width, height) = (params[0], params[1]);
            if width <= 0.0 || height <= 0.0 {
                return Err(FactoryError::InvalidParam(
                    "dimensions must be > 0".into(),
                ));
            }
            Ok(Box::new(Rect { width, height }))
        }
        "triangle" => {
            if params.len() < 3 {
                return Err(FactoryError::MissingParams {
                    expected: 3, got: params.len(),
                });
            }
            let (a, b, c) = (params[0], params[1], params[2]);
            if a + b <= c || a + c <= b || b + c <= a {
                return Err(FactoryError::InvalidParam(
                    "sides violate triangle inequality".into(),
                ));
            }
            Ok(Box::new(Triangle { a, b, c }))
        }
        other => Err(FactoryError::UnknownType(other.into())),
    }
}
```

### 6.3 El caller

```rust
fn main() {
    match shape_create("circle", &[5.0]) {
        Ok(s) => println!("{}: area={:.2}", s.type_name(), s.area()),
        Err(e) => eprintln!("Error: {e}"),
    }

    // Con ? en funciones que retornan Result
    // process_shapes()?;

    // Errores especificos:
    match shape_create("circle", &[-3.0]) {
        Err(FactoryError::InvalidParam(msg)) =>
            eprintln!("Parametro invalido: {msg}"),
        Err(e) => eprintln!("Otro error: {e}"),
        Ok(_) => unreachable!(),
    }

    // Multiples shapes, ignorando errores:
    let kinds = [("circle", vec![5.0]), ("hexagon", vec![3.0])];
    let shapes: Vec<Box<dyn Shape>> = kinds.iter()
        .filter_map(|(k, p)| shape_create(k, p).ok())
        .collect();
}
```

```
  C (T01):                              Rust:
  ──────                                ─────
  FactoryResult res =                   match shape_create("circle", &[5.0]) {
    shape_create(type, p, n, &out);         Ok(s) => /* usar s */
  switch (res) {                            Err(e) => /* manejar e */
    case FACTORY_OK: ...                }
    case FACTORY_INVALID_PARAMS: ...
  }

  Si olvidas checkear → usas Shape* NULL   Si olvidas → no compila (Result
  → crash o UB                              force unwrap o match)
```

---

## 7. Factory con enum selector (type-safe)

En lugar de strings, usar un enum como en la version C:

```rust
#[derive(Debug, Clone, Copy)]
enum ShapeKind {
    Circle,
    Rect,
    Triangle,
}

impl ShapeKind {
    /// Cuantos parametros necesita este tipo
    fn expected_params(self) -> usize {
        match self {
            ShapeKind::Circle   => 1,
            ShapeKind::Rect     => 2,
            ShapeKind::Triangle => 3,
        }
    }

    /// Convertir desde string (para configs)
    fn from_str(s: &str) -> Option<Self> {
        match s {
            "circle"   => Some(ShapeKind::Circle),
            "rect"     => Some(ShapeKind::Rect),
            "triangle" => Some(ShapeKind::Triangle),
            _ => None,
        }
    }
}

fn shape_create_typed(
    kind: ShapeKind,
    params: &[f64],
) -> Result<Box<dyn Shape>, FactoryError> {
    let expected = kind.expected_params();
    if params.len() < expected {
        return Err(FactoryError::MissingParams {
            expected,
            got: params.len(),
        });
    }

    match kind {
        ShapeKind::Circle => {
            Ok(Box::new(Circle { radius: params[0] }))
        }
        ShapeKind::Rect => {
            Ok(Box::new(Rect {
                width: params[0],
                height: params[1],
            }))
        }
        ShapeKind::Triangle => {
            Ok(Box::new(Triangle {
                a: params[0],
                b: params[1],
                c: params[2],
            }))
        }
    }
}
```

Si agregas `ShapeKind::Ellipse` y olvidas el match arm, el
compilador da error (exhaustive matching). Equivalente a
`-Wswitch` en C, pero aqui es **error**, no warning.

---

## 8. Factory config-driven

### 8.1 Parsear desde un archivo de texto

```rust
use std::io::{self, BufRead};
use std::fs::File;

fn load_shapes(path: &str) -> io::Result<Vec<Box<dyn Shape>>> {
    let file = File::open(path)?;
    let reader = io::BufReader::new(file);
    let mut shapes = Vec::new();

    for (line_num, line) in reader.lines().enumerate() {
        let line = line?;
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') {
            continue;  // skip comments and blanks
        }

        let parts: Vec<&str> = line.split_whitespace().collect();
        if parts.is_empty() { continue; }

        let kind = parts[0];
        let params: Vec<f64> = parts[1..].iter()
            .filter_map(|s| s.parse().ok())
            .collect();

        match shape_create(kind, &params) {
            Ok(s) => shapes.push(s),
            Err(e) => {
                eprintln!("line {}: {e}", line_num + 1);
            }
        }
    }

    Ok(shapes)
}

fn main() -> io::Result<()> {
    let shapes = load_shapes("shapes.conf")?;

    let total: f64 = shapes.iter()
        .map(|s| {
            println!("{:<10} area={:.2}", s.type_name(), s.area());
            s.area()
        })
        .sum();

    println!("Total area: {total:.2}");
    Ok(())
}
```

### 8.2 Parsear desde JSON con serde

```rust
use serde::Deserialize;

#[derive(Deserialize)]
struct ShapeSpec {
    #[serde(rename = "type")]
    kind: String,
    params: Vec<f64>,
}

fn load_shapes_json(json: &str) -> Vec<Box<dyn Shape>> {
    let specs: Vec<ShapeSpec> = serde_json::from_str(json)
        .expect("invalid JSON");

    specs.iter()
        .filter_map(|spec| shape_create(&spec.kind, &spec.params).ok())
        .collect()
}

// JSON input:
// [
//   { "type": "circle",   "params": [5.0] },
//   { "type": "rect",     "params": [3.0, 4.0] },
//   { "type": "triangle", "params": [3.0, 4.0, 5.0] }
// ]
```

### 8.3 Parsear con parametros nombrados (serde enum)

```rust
#[derive(Deserialize)]
#[serde(tag = "type")]
enum ShapeSpec {
    #[serde(rename = "circle")]
    Circle { radius: f64 },
    #[serde(rename = "rect")]
    Rect { width: f64, height: f64 },
    #[serde(rename = "triangle")]
    Triangle { a: f64, b: f64, c: f64 },
}

impl ShapeSpec {
    fn build(self) -> Box<dyn Shape> {
        match self {
            ShapeSpec::Circle { radius } =>
                Box::new(Circle { radius }),
            ShapeSpec::Rect { width, height } =>
                Box::new(Rect { width, height }),
            ShapeSpec::Triangle { a, b, c } =>
                Box::new(Triangle { a, b, c }),
        }
    }
}

// JSON input (con parametros nombrados):
// [
//   { "type": "circle",   "radius": 5.0 },
//   { "type": "rect",     "width": 3.0, "height": 4.0 },
//   { "type": "triangle", "a": 3.0, "b": 4.0, "c": 5.0 }
// ]
```

```
  Serde tag maneja automaticamente:
  1. Leer el campo "type" → decide que variante
  2. Deserializar los campos restantes → llenar la variante
  3. Error si falta un campo → deserializacion falla

  Es una factory AUTOMATICA derivada del tipo enum.
  Equivalente a escribir el match + validacion a mano, pero gratis.
```

---

## 9. Registry dinamico en Rust

En T01 implementamos un registry en C con array global. En Rust
usamos un HashMap con closures:

### 9.1 Registry con HashMap

```rust
use std::collections::HashMap;

type Constructor = Box<dyn Fn(&[f64]) -> Option<Box<dyn Shape>>>;

struct ShapeRegistry {
    constructors: HashMap<String, Constructor>,
}

impl ShapeRegistry {
    fn new() -> Self {
        ShapeRegistry {
            constructors: HashMap::new(),
        }
    }

    fn register<F>(&mut self, name: &str, ctor: F)
    where
        F: Fn(&[f64]) -> Option<Box<dyn Shape>> + 'static,
    {
        self.constructors.insert(name.to_string(), Box::new(ctor));
    }

    fn create(&self, name: &str, params: &[f64]) -> Option<Box<dyn Shape>> {
        let ctor = self.constructors.get(name)?;
        ctor(params)
    }

    fn list_types(&self) -> Vec<&str> {
        self.constructors.keys().map(|s| s.as_str()).collect()
    }
}
```

### 9.2 Registrar tipos

```rust
fn main() {
    let mut registry = ShapeRegistry::new();

    // Registrar tipos con closures
    registry.register("circle", |params| {
        let radius = *params.first()?;
        Some(Box::new(Circle { radius }))
    });

    registry.register("rect", |params| {
        let width = *params.get(0)?;
        let height = *params.get(1)?;
        Some(Box::new(Rect { width, height }))
    });

    // Crear via registry
    if let Some(s) = registry.create("circle", &[5.0]) {
        println!("{}: {:.2}", s.type_name(), s.area());
    }

    // Listar tipos
    println!("Types: {:?}", registry.list_types());
}
```

### 9.3 Registry con inventory crate (auto-registro)

```rust
// Con el crate `inventory`, los tipos se registran automaticamente
// sin necesidad de llamar register() en main.

// En cada modulo que define un tipo:
inventory::submit! {
    ShapeRegistration {
        name: "circle",
        ctor: |params: &[f64]| -> Option<Box<dyn Shape>> {
            Some(Box::new(Circle { radius: *params.first()? }))
        },
    }
}

// La factory consulta todos los registros automaticamente:
fn create_from_registry(name: &str, params: &[f64]) -> Option<Box<dyn Shape>> {
    for entry in inventory::iter::<ShapeRegistration> {
        if entry.name == name {
            return (entry.ctor)(params);
        }
    }
    None
}
```

```
  Comparacion de registries:

  C (T01):                          Rust:
  ──────                            ─────
  Array global estático             HashMap o inventory
  Limite fijo (MAX_TYPES)           Crece dinamicamente
  strcmp para buscar                 HashMap O(1) lookup
  Function pointer                  Closure (captura contexto)
  Thread safety: manual             HashMap: &self = safe, &mut = exclusive
  Plugins: dlopen + dlsym           Plugins: libloading crate
```

---

## 10. Factory como metodo asociado (new pattern)

El patron mas idiomatico en Rust: el constructor es un metodo
asociado del tipo, no una funcion separada.

### 10.1 Constructores por tipo

```rust
impl Circle {
    fn new(radius: f64) -> Self {
        Circle { radius }
    }
}

impl Rect {
    fn new(width: f64, height: f64) -> Self {
        Rect { width, height }
    }
}

impl Triangle {
    fn new(a: f64, b: f64, c: f64) -> Option<Self> {
        if a + b <= c || a + c <= b || b + c <= a {
            return None;
        }
        Some(Triangle { a, b, c })
    }
}
```

### 10.2 Constructor polimorfico via trait

```rust
trait ShapeFactory {
    fn create(params: &[f64]) -> Option<Box<dyn Shape>>
    where
        Self: Sized;
}

impl ShapeFactory for Circle {
    fn create(params: &[f64]) -> Option<Box<dyn Shape>> {
        let radius = *params.first()?;
        if radius <= 0.0 { return None; }
        Some(Box::new(Circle { radius }))
    }
}

impl ShapeFactory for Rect {
    fn create(params: &[f64]) -> Option<Box<dyn Shape>> {
        let w = *params.get(0)?;
        let h = *params.get(1)?;
        if w <= 0.0 || h <= 0.0 { return None; }
        Some(Box::new(Rect { width: w, height: h }))
    }
}

// Uso:
let c = Circle::create(&[5.0]).unwrap();
let r = Rect::create(&[3.0, 4.0]).unwrap();
```

### 10.3 Constructores alternativos (named constructors)

```rust
impl Circle {
    fn new(radius: f64) -> Self {
        Circle { radius }
    }

    fn unit() -> Self {
        Circle { radius: 1.0 }
    }

    fn from_area(area: f64) -> Self {
        Circle { radius: (area / PI).sqrt() }
    }

    fn from_circumference(circ: f64) -> Self {
        Circle { radius: circ / (2.0 * PI) }
    }
}

// Uso: claro y auto-documentado
let c1 = Circle::new(5.0);
let c2 = Circle::unit();
let c3 = Circle::from_area(78.54);
let c4 = Circle::from_circumference(31.42);
```

```
  Este patron (named constructors) es el equivalente idiomatico
  de la factory en Rust cuando el caller conoce el tipo.

  C:     circle_new(5.0)     vs  shape_create("circle", ...)
  Rust:  Circle::new(5.0)    vs  shape_create("circle", ...)

  Misma dicotomia que en T01: constructor directo vs factory.
  En Rust, los constructores directos son el camino idiomatico
  y la factory se reserva para creacion config-driven.
```

---

## 11. Factory + From / Into

Rust tiene un patron estandar para conversiones: los traits
`From` y `Into`. Se pueden usar como mini-factories:

```rust
impl From<f64> for Circle {
    fn from(radius: f64) -> Self {
        Circle { radius }
    }
}

impl From<(f64, f64)> for Rect {
    fn from((width, height): (f64, f64)) -> Self {
        Rect { width, height }
    }
}

impl From<(f64, f64, f64)> for Triangle {
    fn from((a, b, c): (f64, f64, f64)) -> Self {
        Triangle { a, b, c }
    }
}

// Uso:
let c = Circle::from(5.0);
let r: Rect = (3.0, 4.0).into();
let t: Triangle = (3.0, 4.0, 5.0).into();
```

### 11.1 From para conversion entre representaciones

```rust
// Convertir enum a trait object (y viceversa)
impl From<ShapeEnum> for Box<dyn Shape> {
    fn from(s: ShapeEnum) -> Self {
        match s {
            ShapeEnum::Circle { radius } =>
                Box::new(Circle { radius }),
            ShapeEnum::Rect { width, height } =>
                Box::new(Rect { width, height }),
            ShapeEnum::Triangle { a, b, c } =>
                Box::new(Triangle { a, b, c }),
        }
    }
}

// Uso:
let e = ShapeEnum::Circle { radius: 5.0 };
let boxed: Box<dyn Shape> = e.into();
```

```
  From/Into:
  - Es una factory con nombre estandar
  - El compilador puede usarla automaticamente en contextos de tipo
  - Implementar From<T> te da Into<T> gratis
  - Convencion: From no debe fallar. Si puede fallar, usar TryFrom.
```

### 11.2 TryFrom para factory falible

```rust
impl TryFrom<&[f64]> for Triangle {
    type Error = String;

    fn try_from(params: &[f64]) -> Result<Self, Self::Error> {
        if params.len() < 3 {
            return Err(format!(
                "need 3 sides, got {}", params.len()
            ));
        }
        let (a, b, c) = (params[0], params[1], params[2]);
        if a + b <= c || a + c <= b || b + c <= a {
            return Err("violates triangle inequality".into());
        }
        Ok(Triangle { a, b, c })
    }
}

// Uso:
let t = Triangle::try_from(&[3.0, 4.0, 5.0][..])?;
```

---

## 12. Comparacion completa: C vs Rust

```
  Aspecto               C (T01)                  Rust (T02)
  ───────               ───────                  ──────────
  Factory function      shape_create()           shape_create() → Option/Result
  Retorno               Shape* (nullable)        Box<dyn Shape> o enum
  Error handling         NULL o error code        Result<T, E>
  Parametros            double[], varargs        &[f64], struct, enum
  Selector              enum o string            enum, &str, serde tag
  Type safety           switch sin default       exhaustive match
  Ownership             Documentacion            Box = owner, & = borrow
  Destruccion           shape_destroy() manual   Drop automatico
  Registry              Array global + strcmp     HashMap + closures
  Config-driven         fscanf manual            serde (automatico)
  Named constructors    circle_new()             Circle::new()
  Conversion            No estandar              From/Into/TryFrom
  Plugins               dlopen/dlsym             libloading crate
  ABI stability         Si (C ABI)               No (Rust ABI inestable)
```

```
  Lo que Rust automatiza:
  1. Vtable (dyn Trait)          → eliminado el boilerplate de C01/S01
  2. Destruccion (Drop)          → imposible olvidar
  3. Null safety (Option)        → compilador fuerza el check
  4. Error richness (Result)     → tipos de error, no codigos
  5. Serializacion (serde)       → factory casi automatica
  6. Conversion (From/Into)      → patron estandar

  Lo que C mantiene como ventaja:
  1. ABI estable (plugins binarios entre compiladores)
  2. Menos abstraccion (mas facil de razonar sobre el bajo nivel)
  3. No necesita allocator para crear via stack (tagged union)
```

---

## Errores comunes

### Error 1 — Retornar tipos distintos con impl Trait

```rust
// NO COMPILA: impl Trait exige UN solo tipo concreto
fn bad_factory(kind: &str) -> impl Shape {
    match kind {
        "circle" => Circle { radius: 5.0 },
        _ => Rect { width: 1.0, height: 1.0 },
        //   ^^^^ expected Circle, found Rect
    }
}
```

Si necesitas retornar tipos distintos, usa `Box<dyn Shape>` o un
enum. `impl Trait` en retorno es para cuando la funcion siempre
retorna el mismo tipo.

### Error 2 — Olvidar que Box<dyn> requiere object safety

```rust
trait BadShape {
    fn new(r: f64) -> Self;  // NO es object-safe
    fn area(&self) -> f64;
}

// Esto no compila:
fn factory() -> Box<dyn BadShape> {
//                  ^^^^^^^^^^^ `BadShape` cannot be made into an object
}
```

`fn new(r: f64) -> Self` impide object safety porque `Self` tiene
tamano desconocido detras de `dyn`. Solucion: separar el trait de
construccion del trait de uso, o agregar `where Self: Sized`:

```rust
trait Shape {
    fn new(r: f64) -> Self where Self: Sized;  // excluido de dyn
    fn area(&self) -> f64;
}
```

### Error 3 — Comparar Box<dyn> con == sin PartialEq

```rust
let a = shape_create("circle", &[5.0]).unwrap();
let b = shape_create("circle", &[5.0]).unwrap();

// NO COMPILA: Box<dyn Shape> no implementa PartialEq
// if a == b { ... }

// Solucion: comparar por area u otro criterio
if (a.area() - b.area()).abs() < 1e-10 {
    println!("Same area");
}
```

`dyn Trait` no puede derivar `PartialEq` automaticamente porque
no sabe los tipos concretos. Si necesitas comparar, agrega un
metodo al trait (`fn eq_shape(&self, other: &dyn Shape) -> bool`).

### Error 4 — Usar unwrap en factory sin justificacion

```rust
// PELIGROSO: si la config tiene un tipo invalido → panic
let shapes: Vec<Box<dyn Shape>> = config.iter()
    .map(|(k, p)| shape_create(k, p).unwrap())  // panic si falla
    .collect();

// CORRECTO: filtrar errores
let shapes: Vec<Box<dyn Shape>> = config.iter()
    .filter_map(|(k, p)| shape_create(k, p).ok())
    .collect();

// O MEJOR: propagar errores
let shapes: Result<Vec<Box<dyn Shape>>, FactoryError> = config.iter()
    .map(|(k, p)| shape_create(k, p))
    .collect();  // para al primer error
```

---

## Ejercicios

### Ejercicio 1 — Factory basica

Implementa la factory `shape_create` que retorna `Box<dyn Shape>`
para circle, rect, y triangle. Testa con tres shapes y verifica
que `area()` retorna los valores correctos.

**Prediccion**: Cuantas lineas de boilerplate te ahorras comparado
con la version C de T01?

<details><summary>Respuesta</summary>

```rust
use std::f64::consts::PI;

trait Shape {
    fn type_name(&self) -> &str;
    fn area(&self) -> f64;
    fn perimeter(&self) -> f64;
}

struct Circle { radius: f64 }
struct Rect { width: f64, height: f64 }
struct Triangle { a: f64, b: f64, c: f64 }

impl Shape for Circle {
    fn type_name(&self) -> &str { "Circle" }
    fn area(&self) -> f64 { PI * self.radius * self.radius }
    fn perimeter(&self) -> f64 { 2.0 * PI * self.radius }
}

impl Shape for Rect {
    fn type_name(&self) -> &str { "Rect" }
    fn area(&self) -> f64 { self.width * self.height }
    fn perimeter(&self) -> f64 { 2.0 * (self.width + self.height) }
}

impl Shape for Triangle {
    fn type_name(&self) -> &str { "Triangle" }
    fn area(&self) -> f64 {
        let s = (self.a + self.b + self.c) / 2.0;
        (s * (s - self.a) * (s - self.b) * (s - self.c)).sqrt()
    }
    fn perimeter(&self) -> f64 { self.a + self.b + self.c }
}

fn shape_create(kind: &str, params: &[f64]) -> Option<Box<dyn Shape>> {
    match kind {
        "circle" => Some(Box::new(Circle { radius: *params.first()? })),
        "rect" => {
            Some(Box::new(Rect {
                width: *params.get(0)?,
                height: *params.get(1)?,
            }))
        }
        "triangle" => {
            Some(Box::new(Triangle {
                a: *params.get(0)?,
                b: *params.get(1)?,
                c: *params.get(2)?,
            }))
        }
        _ => None,
    }
}

fn main() {
    let shapes = [
        shape_create("circle",   &[5.0]),
        shape_create("rect",     &[3.0, 4.0]),
        shape_create("triangle", &[3.0, 4.0, 5.0]),
    ];

    for s in shapes.iter().flatten() {
        println!("{:<10} area={:.2}  perim={:.2}",
                 s.type_name(), s.area(), s.perimeter());
    }
}
```

Comparado con C (T01): no necesitas shape.h, vtable struct,
vtable static const, base field, cast a Shape*, destroy function,
ni free manual. Estimacion: ~60-80 lineas menos de boilerplate.

</details>

---

### Ejercicio 2 — Factory con Result

Agrega validacion a la factory: radius > 0, width/height > 0,
desigualdad triangular. Retorna `Result<Box<dyn Shape>, FactoryError>`
con errores descriptivos.

**Prediccion**: Que pasa si usas `?` en la factory — convierte
`Option` a `FactoryError` automaticamente?

<details><summary>Respuesta</summary>

```rust
#[derive(Debug)]
enum FactoryError {
    UnknownType(String),
    MissingParams { expected: usize, got: usize },
    InvalidParam(String),
}

impl std::fmt::Display for FactoryError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            FactoryError::UnknownType(t) =>
                write!(f, "unknown type: {t}"),
            FactoryError::MissingParams { expected, got } =>
                write!(f, "need {expected} params, got {got}"),
            FactoryError::InvalidParam(msg) =>
                write!(f, "invalid: {msg}"),
        }
    }
}

fn shape_create(
    kind: &str,
    params: &[f64],
) -> Result<Box<dyn Shape>, FactoryError> {
    match kind {
        "circle" => {
            if params.is_empty() {
                return Err(FactoryError::MissingParams {
                    expected: 1, got: 0,
                });
            }
            if params[0] <= 0.0 {
                return Err(FactoryError::InvalidParam(
                    format!("radius must be > 0, got {}", params[0]),
                ));
            }
            Ok(Box::new(Circle { radius: params[0] }))
        }
        "rect" => {
            if params.len() < 2 {
                return Err(FactoryError::MissingParams {
                    expected: 2, got: params.len(),
                });
            }
            if params[0] <= 0.0 || params[1] <= 0.0 {
                return Err(FactoryError::InvalidParam(
                    "dimensions must be > 0".into(),
                ));
            }
            Ok(Box::new(Rect {
                width: params[0], height: params[1],
            }))
        }
        "triangle" => {
            if params.len() < 3 {
                return Err(FactoryError::MissingParams {
                    expected: 3, got: params.len(),
                });
            }
            let (a, b, c) = (params[0], params[1], params[2]);
            if a + b <= c || a + c <= b || b + c <= a {
                return Err(FactoryError::InvalidParam(
                    "violates triangle inequality".into(),
                ));
            }
            Ok(Box::new(Triangle { a, b, c }))
        }
        other => Err(FactoryError::UnknownType(other.into())),
    }
}
```

Sobre `?` con Option: **no convierte automaticamente**. `?` en un
`Result`-returning fn necesita que el error implemente `From`. Para
convertir `Option` a `FactoryError`, necesitas `.ok_or()`:

```rust
let radius = params.first()
    .ok_or(FactoryError::MissingParams { expected: 1, got: 0 })?;
```

O implementar `From<NoneError>` (unstable). La forma explicita con
`ok_or` es la recomendada.

</details>

---

### Ejercicio 3 — Factory con enum

Implementa la misma factory pero retornando `ShapeEnum` en lugar
de `Box<dyn Shape>`. Implementa el trait `Shape` para `ShapeEnum`.

**Prediccion**: Cual es el `size_of::<ShapeEnum>()`?

<details><summary>Respuesta</summary>

```rust
#[derive(Debug, Clone, Copy)]
enum ShapeEnum {
    Circle { radius: f64 },
    Rect { width: f64, height: f64 },
    Triangle { a: f64, b: f64, c: f64 },
}

impl Shape for ShapeEnum {
    fn type_name(&self) -> &str {
        match self {
            ShapeEnum::Circle { .. }   => "Circle",
            ShapeEnum::Rect { .. }     => "Rect",
            ShapeEnum::Triangle { .. } => "Triangle",
        }
    }

    fn area(&self) -> f64 {
        match self {
            ShapeEnum::Circle { radius } =>
                PI * radius * radius,
            ShapeEnum::Rect { width, height } =>
                width * height,
            ShapeEnum::Triangle { a, b, c } => {
                let s = (a + b + c) / 2.0;
                (s * (s - a) * (s - b) * (s - c)).sqrt()
            }
        }
    }

    fn perimeter(&self) -> f64 {
        match self {
            ShapeEnum::Circle { radius } =>
                2.0 * PI * radius,
            ShapeEnum::Rect { width, height } =>
                2.0 * (width + height),
            ShapeEnum::Triangle { a, b, c } =>
                a + b + c,
        }
    }
}

fn shape_create_enum(kind: &str, params: &[f64]) -> Option<ShapeEnum> {
    match kind {
        "circle" =>
            Some(ShapeEnum::Circle { radius: *params.first()? }),
        "rect" =>
            Some(ShapeEnum::Rect {
                width: *params.get(0)?,
                height: *params.get(1)?,
            }),
        "triangle" =>
            Some(ShapeEnum::Triangle {
                a: *params.get(0)?,
                b: *params.get(1)?,
                c: *params.get(2)?,
            }),
        _ => None,
    }
}
```

`size_of::<ShapeEnum>()` = **32 bytes**: tag (8 bytes alineado) +
la variante mas grande (Triangle = 3 × f64 = 24 bytes).
Comparado con `Box<dyn Shape>` que son 16 bytes (fat pointer) +
heap allocation del struct concreto.

El enum es mas grande en stack pero no usa heap. Para un
`Vec<ShapeEnum>` vs `Vec<Box<dyn Shape>>`, el enum tiene mejor
localidad de cache (datos contiguos, sin indirecciones).

</details>

---

### Ejercicio 4 — Registry con closures

Implementa un `ShapeRegistry` que use `HashMap<String, Constructor>`
donde `Constructor` es un `Box<dyn Fn>`. Registra circle y rect, y
crea shapes via el registry.

**Prediccion**: Puede el closure capturar estado (ej. un radio
por defecto)?

<details><summary>Respuesta</summary>

```rust
use std::collections::HashMap;

type Constructor = Box<dyn Fn(&[f64]) -> Option<Box<dyn Shape>>>;

struct ShapeRegistry {
    ctors: HashMap<String, Constructor>,
}

impl ShapeRegistry {
    fn new() -> Self {
        ShapeRegistry { ctors: HashMap::new() }
    }

    fn register(&mut self, name: &str, ctor: Constructor) {
        self.ctors.insert(name.to_string(), ctor);
    }

    fn create(&self, name: &str, params: &[f64]) -> Option<Box<dyn Shape>> {
        self.ctors.get(name)?(params)
    }
}

fn main() {
    let mut reg = ShapeRegistry::new();

    reg.register("circle", Box::new(|params| {
        Some(Box::new(Circle { radius: *params.first()? }))
    }));

    reg.register("rect", Box::new(|params| {
        Some(Box::new(Rect {
            width: *params.get(0)?,
            height: *params.get(1)?,
        }))
    }));

    // Closure con estado capturado:
    let default_radius = 10.0;
    reg.register("default_circle", Box::new(move |_params| {
        Some(Box::new(Circle { radius: default_radius }))
    }));

    let s = reg.create("circle", &[5.0]).unwrap();
    println!("{}: {:.2}", s.type_name(), s.area());

    let d = reg.create("default_circle", &[]).unwrap();
    println!("{}: {:.2}", d.type_name(), d.area());
    // Circle: 314.16 (usa default_radius = 10.0)
}
```

Si, el closure puede capturar estado con `move`. Esto es mas
poderoso que los function pointers de C (que no pueden capturar
nada — necesitan un `void* ctx` separado). Ver C01/S02/T04.

</details>

---

### Ejercicio 5 — From/Into como factory

Implementa `From<f64>` para Circle, `From<(f64, f64)>` para Rect,
y `TryFrom<(f64, f64, f64)>` para Triangle (con validacion de
desigualdad triangular).

**Prediccion**: Por que Triangle usa TryFrom en vez de From?

<details><summary>Respuesta</summary>

```rust
impl From<f64> for Circle {
    fn from(radius: f64) -> Self {
        Circle { radius }
    }
}

impl From<(f64, f64)> for Rect {
    fn from((width, height): (f64, f64)) -> Self {
        Rect { width, height }
    }
}

impl TryFrom<(f64, f64, f64)> for Triangle {
    type Error = String;

    fn try_from((a, b, c): (f64, f64, f64)) -> Result<Self, String> {
        if a <= 0.0 || b <= 0.0 || c <= 0.0 {
            return Err("sides must be positive".into());
        }
        if a + b <= c || a + c <= b || b + c <= a {
            return Err("violates triangle inequality".into());
        }
        Ok(Triangle { a, b, c })
    }
}

fn main() {
    let c = Circle::from(5.0);
    let r: Rect = (3.0, 4.0).into();

    match Triangle::try_from((3.0, 4.0, 5.0)) {
        Ok(t) => println!("area = {:.2}", t.area()),
        Err(e) => eprintln!("Error: {e}"),
    }

    // Falla:
    match Triangle::try_from((1.0, 2.0, 100.0)) {
        Ok(_) => unreachable!(),
        Err(e) => println!("Expected error: {e}"),
    }
}
```

Triangle usa `TryFrom` porque no todas las tuplas `(f64, f64, f64)`
son triangulos validos. La convencion de Rust es:

- `From`: conversion que **siempre** funciona (no puede fallar)
- `TryFrom`: conversion que **puede** fallar (retorna Result)

Un `From<(f64,f64,f64)>` para Triangle tendria que hacer panic o
crear un triangulo invalido — ambos son inaceptables. `TryFrom`
fuerza al caller a manejar el error.

Circle y Rect usan `From` porque cualquier f64 es un radio valido
(en este contexto simplificado). En un sistema mas estricto, tambien
usarian `TryFrom` para rechazar valores negativos.

</details>

---

### Ejercicio 6 — Serde como factory automatica

Define un `ShapeSpec` con `#[serde(tag = "type")]` y un metodo
`build()` que convierta a `Box<dyn Shape>`. Parsea el siguiente
JSON:

```json
[
  { "type": "circle", "radius": 5.0 },
  { "type": "rect", "width": 3.0, "height": 4.0 }
]
```

**Prediccion**: Que error da serde si el JSON tiene
`"type": "hexagon"`?

<details><summary>Respuesta</summary>

```rust
use serde::Deserialize;

#[derive(Deserialize)]
#[serde(tag = "type")]
enum ShapeSpec {
    #[serde(rename = "circle")]
    Circle { radius: f64 },
    #[serde(rename = "rect")]
    Rect { width: f64, height: f64 },
    #[serde(rename = "triangle")]
    Triangle { a: f64, b: f64, c: f64 },
}

impl ShapeSpec {
    fn build(self) -> Box<dyn Shape> {
        match self {
            ShapeSpec::Circle { radius } =>
                Box::new(Circle { radius }),
            ShapeSpec::Rect { width, height } =>
                Box::new(Rect { width, height }),
            ShapeSpec::Triangle { a, b, c } =>
                Box::new(Triangle { a, b, c }),
        }
    }
}

fn main() {
    let json = r#"[
        { "type": "circle", "radius": 5.0 },
        { "type": "rect", "width": 3.0, "height": 4.0 }
    ]"#;

    let specs: Vec<ShapeSpec> = serde_json::from_str(json).unwrap();
    let shapes: Vec<Box<dyn Shape>> = specs.into_iter()
        .map(|s| s.build())
        .collect();

    for s in &shapes {
        println!("{}: area={:.2}", s.type_name(), s.area());
    }
}
```

Con `"type": "hexagon"`, serde retorna un error como:
```
Error: unknown variant `hexagon`, expected one of `circle`, `rect`, `triangle`
```

Serde genera automaticamente el mensaje de error con todas las
variantes validas. No necesitas escribir ni mantener esa lista —
se deriva del enum.

</details>

---

### Ejercicio 7 — Comparar las tres formas

Escribe tres versiones de una funcion que crea un shape y calcula
su area:

```rust
fn via_box(kind: &str) -> Option<f64>;   // Box<dyn Shape>
fn via_enum(kind: &str) -> Option<f64>;  // ShapeEnum
fn via_impl() -> f64;                    // impl Shape (solo circle)
```

**Prediccion**: Cual no necesita heap allocation?

<details><summary>Respuesta</summary>

```rust
fn via_box(kind: &str) -> Option<f64> {
    let shape: Box<dyn Shape> = match kind {
        "circle" => Box::new(Circle { radius: 5.0 }),
        "rect"   => Box::new(Rect { width: 3.0, height: 4.0 }),
        _ => return None,
    };
    Some(shape.area())
    // Box<dyn Shape> se destruye aqui (heap deallocation)
}

fn via_enum(kind: &str) -> Option<f64> {
    let shape: ShapeEnum = match kind {
        "circle" => ShapeEnum::Circle { radius: 5.0 },
        "rect"   => ShapeEnum::Rect { width: 3.0, height: 4.0 },
        _ => return None,
    };
    Some(shape.area())
    // ShapeEnum estaba en stack, nada que liberar
}

fn via_impl() -> f64 {
    fn make_circle() -> impl Shape {
        Circle { radius: 5.0 }
    }
    let shape = make_circle();
    shape.area()
    // Circle en stack, dispatch estatico (inlined)
}
```

**Enum** y **impl** no necesitan heap allocation.

- `Box<dyn Shape>`: heap alloc (Box::new) + dealloc (Drop). ~80 ns.
- `ShapeEnum`: todo en stack, 32 bytes. ~0 ns de alloc.
- `impl Shape`: todo en stack, `size_of::<Circle>()` bytes. ~0 ns,
  y ademas el compilador puede inlinear `area()`.

En un hot loop, la diferencia es significativa. Para una sola
llamada (config, setup), es irrelevante.

</details>

---

### Ejercicio 8 — Factory con Clone

El trait `Shape` no es `Clone` automaticamente (los trait objects
no son clonables). Implementa un patron para clonar `Box<dyn Shape>`:

```rust
fn clone_shape(s: &dyn Shape) -> Box<dyn Shape>;
```

**Prediccion**: Puedes agregar `Clone` como supertrait de `Shape`?

<details><summary>Respuesta</summary>

No puedes hacer `trait Shape: Clone` directamente porque `Clone`
requiere `Sized` (retorna `Self`), y `dyn Shape` no es `Sized`.

Solucion: un metodo `clone_box` dentro del trait:

```rust
trait Shape {
    fn type_name(&self) -> &str;
    fn area(&self) -> f64;
    fn perimeter(&self) -> f64;

    // Clonar como Box<dyn Shape>
    fn clone_box(&self) -> Box<dyn Shape>;
}

impl Shape for Circle {
    fn type_name(&self) -> &str { "Circle" }
    fn area(&self) -> f64 { PI * self.radius * self.radius }
    fn perimeter(&self) -> f64 { 2.0 * PI * self.radius }

    fn clone_box(&self) -> Box<dyn Shape> {
        Box::new(Circle { radius: self.radius })
    }
}

impl Shape for Rect {
    fn type_name(&self) -> &str { "Rect" }
    fn area(&self) -> f64 { self.width * self.height }
    fn perimeter(&self) -> f64 { 2.0 * (self.width + self.height) }

    fn clone_box(&self) -> Box<dyn Shape> {
        Box::new(Rect { width: self.width, height: self.height })
    }
}

// Implementar Clone para Box<dyn Shape>:
impl Clone for Box<dyn Shape> {
    fn clone(&self) -> Self {
        self.clone_box()
    }
}

// Uso:
let original: Box<dyn Shape> = Box::new(Circle { radius: 5.0 });
let copy: Box<dyn Shape> = original.clone();
```

Esto es el equivalente Rust del `clone` en el vtable que
implementamos en T01 ejercicio 8. La diferencia: en C asignabas
el function pointer manualmente; en Rust, `impl Clone for Box<dyn Shape>`
permite usar `.clone()` de forma natural.

</details>

---

### Ejercicio 9 — Named constructors

Implementa multiples constructores para `Rect`:

```rust
impl Rect {
    fn new(w: f64, h: f64) -> Self;
    fn square(side: f64) -> Self;
    fn from_area_ratio(area: f64, ratio: f64) -> Self;
    fn from_diagonal(d: f64, ratio: f64) -> Self;
}
```

**Prediccion**: Cual de estos no es posible con la factory
string-based?

<details><summary>Respuesta</summary>

```rust
impl Rect {
    fn new(width: f64, height: f64) -> Self {
        Rect { width, height }
    }

    fn square(side: f64) -> Self {
        Rect { width: side, height: side }
    }

    fn from_area_ratio(area: f64, ratio: f64) -> Self {
        // area = w * h, ratio = w / h
        // h = sqrt(area / ratio), w = area / h
        let height = (area / ratio).sqrt();
        let width = area / height;
        Rect { width, height }
    }

    fn from_diagonal(diagonal: f64, ratio: f64) -> Self {
        // d^2 = w^2 + h^2, w = ratio * h
        // d^2 = (ratio^2 + 1) * h^2
        let height = diagonal / (ratio * ratio + 1.0).sqrt();
        let width = ratio * height;
        Rect { width, height }
    }
}

fn main() {
    let r1 = Rect::new(3.0, 4.0);
    let r2 = Rect::square(5.0);
    let r3 = Rect::from_area_ratio(12.0, 0.75);
    let r4 = Rect::from_diagonal(5.0, 0.75);

    for r in [r1, r2, r3, r4] {
        println!("{:.2} × {:.2} area={:.2}",
                 r.width, r.height, r.area());
    }
}
```

Todos son posibles con la factory string-based, pero
`from_area_ratio` y `from_diagonal` serian confusos:

```rust
// Factory string-based:
shape_create("rect", &[12.0, 0.75])
// ¿Esto es new(12, 0.75), from_area_ratio(12, 0.75),
// o from_diagonal(12, 0.75)? Imposible saber.

// Necesitarias un selector adicional:
shape_create("rect_from_diagonal", &[5.0, 0.75])
// Feo y fragil.
```

Los named constructors son **auto-documentados**: el nombre dice
exactamente que parametros significan. Este es el motivo por el
que Rust los prefiere sobre factories string-based cuando el
caller conoce el tipo.

</details>

---

### Ejercicio 10 — Reflexion: factory en Rust vs C

Responde sin ver la respuesta:

1. En que escenarios usarias `Box<dyn Shape>` como retorno de
   factory vs `ShapeEnum`?

2. Que mecanismo de Rust reemplaza el "ownership por documentacion"
   de la factory en C?

3. Serde `#[serde(tag = "type")]` es esencialmente una factory
   automatica. Que sacrificas al usarlo vs escribir la factory
   a mano?

4. Si la factory esta en una libreria publica, deberia retornar
   `Box<dyn Trait>` o un enum? Por que?

<details><summary>Respuesta</summary>

**1. Box<dyn> vs enum:**

- `Box<dyn Shape>`: cuando los tipos son **extensibles** (el usuario
  de la libreria puede agregar tipos propios), o cuando los tipos
  estan en crates distintos, o cuando quieres un registry dinamico.

- `ShapeEnum`: cuando los tipos son **fijos** (cerrados), rendimiento
  importa (sin heap), y quieres exhaustive matching que avise al
  agregar variantes.

Regla: si puedes enumerar todos los tipos al escribir el enum, usa
enum. Si no, usa dyn.

**2. Ownership en Rust:**

- `Box<T>` = owner exclusivo. Solo uno puede tener el Box.
  Mover el Box transfiere ownership. Drop libera automaticamente.
- `&T` = borrow inmutable. No puede destruir ni mover.
- `&mut T` = borrow exclusivo mutable.

Esto reemplaza completamente la documentacion "caller must free"
de C. El tipo sistema **fuerza** las reglas de ownership. No puedes
compilar codigo que viole ownership.

**3. Serde factory automatica — que sacrificas:**

- **Validacion custom**: serde parsea y valida formato, pero no
  validacion de negocio (desigualdad triangular, rangos).
  Necesitas post-validacion despues de deserializar.
- **Transformaciones**: serde mapea 1:1 desde JSON al struct. Si
  quieres `from_area_ratio`, necesitas un paso extra.
- **Control de allocation**: serde crea los valores directamente.
  No puedes intercalar pooling, logging, o metricas.

En la practica, serde + un metodo `validate()` o `build()` es la
combinacion mas comun.

**4. Libreria publica — Box<dyn> o enum:**

Depende de si quieres que el usuario agregue tipos:

- **Enum**: si la libreria define un conjunto cerrado (ej. tipos
  de error, nodos de un AST). Agregar variantes es un breaking
  change. Puedes marcar con `#[non_exhaustive]` para reservarte
  el derecho.

- **Box<dyn Trait>**: si la libreria define una interfaz que el
  usuario extiende (ej. middleware, storage backends, codecs).
  Agregar metodos al trait es un breaking change (mitigable con
  default methods).

Regla: enum para datos, dyn Trait para comportamiento extensible.

</details>
