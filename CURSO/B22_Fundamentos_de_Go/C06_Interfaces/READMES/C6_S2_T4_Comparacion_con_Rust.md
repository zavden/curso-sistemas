# Comparacion con Rust — Interfaces vs Traits, Satisfaccion Implicita vs Explicita

## 1. Introduccion

Go y Rust resuelven el mismo problema fundamental — **polimorfismo sin herencia** — con filosofias diametralmente opuestas. Go usa **interfaces implicitas** con **dynamic dispatch** exclusivo. Rust usa **traits explicitos** con **static dispatch por defecto** y dynamic dispatch opcional via `dyn`. Ambos rechazan la herencia clasica de C++/Java, pero llegan a soluciones radicalmente diferentes.

Esta comparacion no es sobre "cual es mejor" — es sobre entender las **tradeoffs** de cada decision de diseno. Go optimiza para **simplicidad y velocidad de desarrollo**: satisfaccion implicita, compilacion rapida, reglas simples. Rust optimiza para **seguridad y rendimiento**: el compilador verifica mas, el programador declara mas, el binario es mas eficiente.

Para un SysAdmin/DevOps, entender ambos modelos es critico: Go domina en tooling de infraestructura (Docker, Kubernetes, Terraform, Prometheus) y Rust avanza en sistemas de bajo nivel (Firecracker, Bottlerocket, herramientas de red). Saber cuando cada modelo brilla te permite elegir la herramienta correcta y entender por que el codigo de cada ecosistema "se ve" tan diferente.

C entra como referencia historica — el abuelo comun que no tiene ni interfaces ni traits, y donde todo se resuelve con function pointers y `void*`. Ver como Go y Rust mejoran sobre C ilumina las decisiones de ambos.

```
┌──────────────────────────────────────────────────────────────────────────┐
│           POLIMORFISMO SIN HERENCIA — Tres Enfoques                      │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  C (1972)           Go (2009)            Rust (2015)                     │
│  ──────────         ────────             ──────────                      │
│  void*              interface{}          dyn Any (raro)                  │
│  function ptrs      interface I { M() }  trait T { fn m(); }            │
│  manual dispatch    implicit satisfaction explicit impl T for S         │
│  no type safety     runtime type info    compile-time monomorphization  │
│  zero overhead      fat pointer (16B)    zero-cost abstractions         │
│                                                                          │
│  "Trust the         "Simple is           "If it compiles,               │
│   programmer"        better"              it works"                      │
│                                                                          │
│  Compilacion:       Compilacion:          Compilacion:                   │
│  No verifica nada   Verifica que el tipo  Verifica impl, lifetimes,     │
│  sobre polimorfismo tiene los metodos     ownership, Send/Sync...       │
│                                                                          │
│  Runtime:           Runtime:              Runtime:                       │
│  Crash si esta mal  Panic en type assert  No panic (salvo unwrap)       │
│                     si tipo no coincide   Errores manejados con Result  │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Satisfaccion de Interfaces: Implicita vs Explicita

Esta es la **diferencia mas fundamental** entre Go y Rust. Determina como los tipos adquieren capacidades polimorfica y tiene consecuencias profundas en el diseno de codigo.

### Go — Satisfaccion implicita (duck typing estructural)

```go
// Go: defines la interfaz
type Writer interface {
    Write(p []byte) (n int, err error)
}

// Go: defines el tipo — NO mencionas Writer en ningun lado
type FileLogger struct {
    Path string
}

// Go: implementas el metodo con la firma exacta
func (f *FileLogger) Write(p []byte) (n int, err error) {
    // ... escribir a archivo ...
    return len(p), nil
}

// Go: FileLogger satisface Writer automaticamente
// No hay "implements", no hay "impl Writer for FileLogger"
// El compilador verifica que el method set coincida — y ya

func logTo(w Writer, msg string) {
    w.Write([]byte(msg))
}

func main() {
    logger := &FileLogger{Path: "/var/log/app.log"}
    logTo(logger, "hello") // compila porque *FileLogger tiene Write()
}
```

**Consecuencias de la satisfaccion implicita:**

| Aspecto | Implicacion |
|---------|-------------|
| Desacoplamiento | El tipo no necesita importar el paquete de la interfaz |
| Retrocompatibilidad | Puedes definir interfaces que tipos existentes ya satisfacen |
| Descubribilidad | Dificil saber que interfaces satisface un tipo sin herramientas |
| Errores | Si un metodo tiene firma ligeramente diferente, simplemente no satisface — sin mensaje explicito |
| Intencion | No queda claro si la satisfaccion fue intencional o coincidencia |

### Rust — Satisfaccion explicita (impl blocks)

```rust
// Rust: defines el trait
trait Writer {
    fn write(&mut self, buf: &[u8]) -> Result<usize, std::io::Error>;
}

// Rust: defines el tipo
struct FileLogger {
    path: String,
}

// Rust: EXPLICITAMENTE implementas el trait PARA el tipo
impl Writer for FileLogger {
    fn write(&mut self, buf: &[u8]) -> Result<usize, std::io::Error> {
        // ... escribir a archivo ...
        Ok(buf.len())
    }
}

// Si no escribes "impl Writer for FileLogger", FileLogger NO satisface Writer
// Aunque tenga un metodo write() con la misma firma — NO cuenta

// Rust: usas trait bounds para polimorfismo
fn log_to(w: &mut impl Writer, msg: &str) {
    w.write(msg.as_bytes()).unwrap();
}

fn main() {
    let mut logger = FileLogger { path: "/var/log/app.log".into() };
    log_to(&mut logger, "hello");
}
```

**Consecuencias de la satisfaccion explicita:**

| Aspecto | Implicacion |
|---------|-------------|
| Intencion clara | `impl Writer for FileLogger` declara la intencion explicitamente |
| Descubribilidad | `rustdoc` lista todos los traits que un tipo implementa |
| Errores claros | Si falta un metodo del trait, el compilador dice exactamente cual |
| Metodos default | Los traits pueden tener implementaciones default (Go no puede) |
| Acoplamiento | El tipo O su crate debe importar el trait para hacer `impl` |
| Orphan rule | No puedes implementar traits externos para tipos externos |

### Comparacion directa

```go
// Go: ¿FileLogger satisface Writer?
// Respuesta: depende — compilalo y ve si compila.
// O usa: var _ Writer = (*FileLogger)(nil) // compile-time assertion

// Herramienta: `go doc` no lista interfaces satisfechas.
// Herramienta: `guru implements` o gopls lo puede deducir.
```

```rust
// Rust: ¿FileLogger satisface Writer?
// Respuesta: busca "impl Writer for FileLogger" en el codigo.
// `cargo doc` lista TODOS los traits implementados para cada tipo.
// El compilador te dice exactamente que falta si no compila.
```

### El caso revelador: satisfaccion accidental

```go
// Go: puedes satisfacer una interfaz sin querer
type Dog struct{}
func (d Dog) String() string { return "Woof" }

// Dog ahora satisface fmt.Stringer — ¿fue intencional?
// El compilador no distingue intencion de coincidencia.

// Esto puede ser UTIL (retrocompatibilidad) o PELIGROSO
// (un tipo pasa donde no deberia porque coinciden los metodos)
```

```rust
// Rust: imposible satisfacer un trait sin querer
struct Dog;

impl Dog {
    // Este metodo NO hace que Dog implemente Display
    fn to_string(&self) -> String {
        "Woof".into()
    }
}

// Para implementar Display, DEBES escribir:
use std::fmt;
impl fmt::Display for Dog {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Woof")
    }
}
// Intencion explicita — nunca accidental
```

---

## 3. Method Sets vs impl Blocks

Go y Rust organizan los metodos de un tipo de formas fundamentalmente diferentes.

### Go — Method sets ligados a receivers

```go
type Server struct {
    Host string
    Port int
}

// Metodos con value receiver — disponibles para Server y *Server
func (s Server) Address() string {
    return fmt.Sprintf("%s:%d", s.Host, s.Port)
}

// Metodos con pointer receiver — solo disponibles para *Server
func (s *Server) SetPort(p int) {
    s.Port = p
}

// Method set de Server:  { Address() }
// Method set de *Server: { Address(), SetPort() }

// Interfaz que requiere ambos metodos:
type Configurable interface {
    Address() string
    SetPort(int)
}

var _ Configurable = Server{}   // ERROR: Server no tiene SetPort
var _ Configurable = &Server{}  // OK: *Server tiene ambos
```

**Reglas del method set en Go:**

| Tipo | Method set incluye |
|------|--------------------|
| `T` (valor) | Metodos con receiver `T` |
| `*T` (puntero) | Metodos con receiver `T` **y** `*T` |
| Interface `I` | Todos los metodos declarados en `I` |

### Rust — impl blocks separados por contexto

```rust
struct Server {
    host: String,
    port: u16,
}

// impl block "inherente" — metodos propios del tipo
impl Server {
    // Constructor (convencion, no magia del lenguaje)
    fn new(host: &str, port: u16) -> Self {
        Server { host: host.into(), port }
    }

    // &self = referencia inmutable (como value receiver en Go)
    fn address(&self) -> String {
        format!("{}:{}", self.host, self.port)
    }

    // &mut self = referencia mutable (como pointer receiver en Go)
    fn set_port(&mut self, port: u16) {
        self.port = port;
    }

    // self = toma ownership (sin equivalente en Go)
    fn into_address(self) -> String {
        format!("{}:{}", self.host, self.port)
        // self se consume — no se puede usar despues
    }
}

// impl block para un trait — separado del inherente
trait Configurable {
    fn address(&self) -> String;
    fn set_port(&mut self, port: u16);
}

impl Configurable for Server {
    fn address(&self) -> String {
        format!("{}:{}", self.host, self.port)
    }

    fn set_port(&mut self, port: u16) {
        self.port = port;
    }
}
```

### Diferencias clave

```
┌─────────────────────────────────────────────────────────────────────────┐
│  METHOD SETS (Go) vs IMPL BLOCKS (Rust)                                 │
├──────────────────┬──────────────────────┬───────────────────────────────┤
│  Aspecto         │  Go                  │  Rust                         │
├──────────────────┼──────────────────────┼───────────────────────────────┤
│  Organizacion    │  Metodos dispersos   │  Agrupados en impl blocks    │
│                  │  en el mismo paquete │  (uno inherente + N traits)   │
├──────────────────┼──────────────────────┼───────────────────────────────┤
│  Mutabilidad     │  Value receiver (T)  │  &self (inmutable)            │
│                  │  Pointer receiver    │  &mut self (mutable)          │
│                  │  (*T)                │  self (consume ownership)     │
├──────────────────┼──────────────────────┼───────────────────────────────┤
│  Constructores   │  func NewServer()    │  impl Server { fn new() }    │
│                  │  (convencion, no     │  (convencion, no keyword)     │
│                  │  keyword especial)   │                               │
├──────────────────┼──────────────────────┼───────────────────────────────┤
│  Ubicacion       │  Mismo paquete que   │  Mismo crate que el tipo     │
│                  │  el tipo             │  O mismo crate que el trait   │
├──────────────────┼──────────────────────┼───────────────────────────────┤
│  Visibilidad     │  Mayuscula/minuscula │  pub/pub(crate)/private      │
│  de metodos      │  (exportado o no)    │  (granular por modulo)        │
├──────────────────┼──────────────────────┼───────────────────────────────┤
│  Multiples impl  │  No aplica —         │  Si: impl inherente +        │
│  blocks          │  metodos sueltos     │  impl TraitA for T +          │
│                  │                      │  impl TraitB for T            │
└──────────────────┴──────────────────────┴───────────────────────────────┘
```

### Rust: self, &self, &mut self — el trio que Go no tiene

```rust
trait Resource {
    // &self — lee pero no modifica (como value receiver en Go)
    fn status(&self) -> String;

    // &mut self — puede modificar (como pointer receiver en Go)
    fn restart(&mut self) -> Result<(), String>;

    // self — CONSUME el recurso (Go no tiene equivalente)
    // Despues de llamar shutdown(), el recurso ya no existe
    fn shutdown(self) -> Result<(), String>;
}

struct Container {
    id: String,
    running: bool,
}

impl Resource for Container {
    fn status(&self) -> String {
        if self.running { "running".into() } else { "stopped".into() }
    }

    fn restart(&mut self) -> Result<(), String> {
        self.running = true;
        Ok(())
    }

    fn shutdown(self) -> Result<(), String> {
        // self se consume — el Container deja de existir
        println!("Container {} destroyed", self.id);
        Ok(())
        // drop(self) implicito aqui
    }
}

fn main() {
    let mut c = Container { id: "abc123".into(), running: true };
    println!("{}", c.status());   // &self — c sigue existiendo
    c.restart().unwrap();          // &mut self — c sigue existiendo
    c.shutdown().unwrap();         // self — c se CONSUME
    // println!("{}", c.status()); // ERROR de compilacion: c fue movido
}
```

Go no puede expresar "este metodo destruye el recurso" en el sistema de tipos. En Go, despues de llamar `Shutdown()`, el programador debe recordar no usar el objeto — el compilador no lo impide.

---

## 4. Dynamic Dispatch vs Static Dispatch

Esta es posiblemente la diferencia de **rendimiento** mas importante entre Go y Rust.

### Go — Siempre dynamic dispatch

```go
type Hasher interface {
    Hash(data []byte) []byte
}

type SHA256Hasher struct{}
func (h SHA256Hasher) Hash(data []byte) []byte {
    sum := sha256.Sum256(data)
    return sum[:]
}

type MD5Hasher struct{}
func (h MD5Hasher) Hash(data []byte) []byte {
    sum := md5.Sum(data)
    return sum[:]
}

// Cuando llamas hashData, Go usa dynamic dispatch:
// 1. Lee el campo "tab" del interface value (fat pointer)
// 2. Busca el puntero a la funcion Hash en la itable
// 3. Llama la funcion a traves del puntero
// Esto ocurre en CADA llamada — overhead real pero pequeño
func hashData(h Hasher, data []byte) []byte {
    return h.Hash(data)
    // Compilado: CALL [h.itab.fun[0]](h.data, data)
    // No puede hacer inline — no sabe que funcion va a llamar
}
```

**Costo del dynamic dispatch en Go:**
- Indirection: 1-2 instrucciones extra por llamada (load itable, indirect call)
- No inlining: el compilador no puede optimizar a traves de la llamada
- Cache miss potencial: la itable y el codigo del metodo pueden no estar en L1

### Rust — Static dispatch por defecto, dynamic opcional

```rust
trait Hasher {
    fn hash(&self, data: &[u8]) -> Vec<u8>;
}

struct Sha256Hasher;
impl Hasher for Sha256Hasher {
    fn hash(&self, data: &[u8]) -> Vec<u8> {
        // ... sha256 ...
        vec![]
    }
}

struct Md5Hasher;
impl Hasher for Md5Hasher {
    fn hash(&self, data: &[u8]) -> Vec<u8> {
        // ... md5 ...
        vec![]
    }
}

// STATIC DISPATCH — impl Hasher (syntactic sugar para generico)
// El compilador genera UNA version de esta funcion POR CADA tipo concreto
fn hash_data_static(h: &impl Hasher, data: &[u8]) -> Vec<u8> {
    h.hash(data)
    // Compilado: CALL sha256_hasher_hash(h, data)  ← llamada directa
    // O:         CALL md5_hasher_hash(h, data)     ← otra version
    // PUEDE hacer inline — sabe exactamente que funcion es
}

// Equivalente explicito con genericos:
fn hash_data_generic<H: Hasher>(h: &H, data: &[u8]) -> Vec<u8> {
    h.hash(data) // monomorphized — una copia por tipo
}

// DYNAMIC DISPATCH — dyn Hasher (equivalente a Go)
// Una sola version de la funcion, dispatch via vtable
fn hash_data_dynamic(h: &dyn Hasher, data: &[u8]) -> Vec<u8> {
    h.hash(data)
    // Compilado: CALL [h.vtable.hash](h.data, data) ← como Go
    // NO puede hacer inline
}
```

### Monomorphization explicada

```
Rust: fn process<T: Hasher>(h: &T) { h.hash(...) }

Cuando el compilador ve:
  process(&Sha256Hasher)  →  genera: fn process_SHA256(h: &Sha256Hasher)
  process(&Md5Hasher)     →  genera: fn process_MD5(h: &Md5Hasher)

Cada version tiene la llamada DIRECTA al metodo concreto.
El compilador puede INLINE la llamada completa.

Tradeoff: mas codigo generado (binario mas grande) pero mas rapido.

Go: func process(h Hasher) { h.Hash(...) }

El compilador genera UNA version.
En runtime, lee la itable del fat pointer para encontrar Hash().
No puede inlinear.

Tradeoff: menos codigo generado pero overhead por indirection.
```

### Tabla comparativa de dispatch

```
┌──────────────────────────────────────────────────────────────────────────┐
│  DISPATCH — Go vs Rust                                                   │
├──────────────────┬───────────────────────┬───────────────────────────────┤
│  Aspecto         │  Go                   │  Rust                         │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│  Default         │  Dynamic (siempre)    │  Static (monomorphization)    │
│  Dynamic         │  Unica opcion         │  Opt-in: &dyn Trait           │
│  Static          │  No disponible        │  Default: impl Trait / <T: T> │
│  Inlining        │  No (a traves de      │  Si (static dispatch)         │
│                  │  interfaces)          │  No (dyn dispatch)            │
│  Binario         │  Mas compacto         │  Mas grande (copias por tipo) │
│  Compilacion     │  Rapida               │  Mas lenta (genera codigo)    │
│  Overhead        │  ~2ns por llamada     │  0 (static) / ~2ns (dyn)      │
│  Flexibilidad    │  Siempre heterogeneo  │  Heterogeneo solo con dyn     │
│  Colecciones     │  []Interface — facil  │  Vec<Box<dyn Trait>> — verbose│
└──────────────────┴───────────────────────┴───────────────────────────────┘
```

### Colecciones heterogeneas — la ventaja de Go

```go
// Go: natural y limpio
type HealthChecker interface {
    Check() error
}

// Slice de diferentes tipos concretos — funciona directamente
checkers := []HealthChecker{
    &DBChecker{host: "db"},
    &RedisChecker{host: "cache"},
    &DNSChecker{domain: "api.example.com"},
}

for _, c := range checkers {
    if err := c.Check(); err != nil {
        log.Println(err)
    }
}
```

```rust
// Rust: requiere Box<dyn Trait> para colecciones heterogeneas
trait HealthChecker {
    fn check(&self) -> Result<(), String>;
}

// Cada elemento necesita Box (heap allocation) + dyn (dynamic dispatch)
let checkers: Vec<Box<dyn HealthChecker>> = vec![
    Box::new(DBChecker { host: "db".into() }),
    Box::new(RedisChecker { host: "cache".into() }),
    Box::new(DNSChecker { domain: "api.example.com".into() }),
];

for c in &checkers {
    if let Err(e) = c.check() {
        eprintln!("{}", e);
    }
}

// Alternativa con enum (static dispatch, sin allocation):
enum Checker {
    DB(DBChecker),
    Redis(RedisChecker),
    DNS(DNSChecker),
}

impl Checker {
    fn check(&self) -> Result<(), String> {
        match self {
            Checker::DB(c) => c.check(),
            Checker::Redis(c) => c.check(),
            Checker::DNS(c) => c.check(),
        }
    }
}
// Mas eficiente pero menos extensible — cerrado (closed set)
```

---

## 5. Fat Pointers: (type, value) vs (data, vtable)

Ambos lenguajes usan punteros "gordos" de dos palabras para el dynamic dispatch, pero con organizacion diferente.

### Go — Interface value: (itable, data)

```
┌────────────────────────────────────────────────────────┐
│  Go interface value (16 bytes en 64-bit)               │
│                                                        │
│  ┌──────────┬──────────┐                               │
│  │   tab    │   data   │                               │
│  │  8 bytes │  8 bytes │                               │
│  └────┬─────┴────┬─────┘                               │
│       │          │                                      │
│       ▼          ▼                                      │
│  ┌─────────┐  ┌──────────┐                             │
│  │  itab   │  │  valor   │                             │
│  ├─────────┤  │ concreto │                             │
│  │ inter   │  │ (o copy  │                             │
│  │ _type   │  │  si cabe │                             │
│  │ hash    │  │  en 8B)  │                             │
│  │ fun[0]  │──→ metodo 0                               │
│  │ fun[1]  │──→ metodo 1                               │
│  │ ...     │                                           │
│  └─────────┘                                           │
│                                                        │
│  La itab contiene:                                     │
│  - inter: puntero al tipo de la interfaz               │
│  - _type: puntero al tipo concreto                     │
│  - hash: para type switches rapidos                    │
│  - fun[]: array de punteros a funciones (metodos)      │
│                                                        │
│  Las itabs se CACHEAN — Go mantiene un mapa global     │
│  de (interfaceType, concreteType) → *itab              │
│  Primera asignacion: lookup + posible creacion          │
│  Siguientes: cache hit directo                         │
└────────────────────────────────────────────────────────┘
```

### Rust — Trait object: (data, vtable)

```
┌────────────────────────────────────────────────────────┐
│  Rust &dyn Trait (16 bytes en 64-bit)                  │
│                                                        │
│  ┌──────────┬──────────┐                               │
│  │   data   │  vtable  │                               │
│  │  8 bytes │  8 bytes │                               │
│  └────┬─────┴────┬─────┘                               │
│       │          │                                      │
│       ▼          ▼                                      │
│  ┌──────────┐  ┌──────────────┐                        │
│  │  valor   │  │   vtable     │                        │
│  │ concreto │  ├──────────────┤                        │
│  │          │  │ drop_fn      │ → destructor           │
│  │          │  │ size         │ → sizeof(T)            │
│  │          │  │ align        │ → alignof(T)           │
│  │          │  │ method_0     │ → fn check(&self)      │
│  │          │  │ method_1     │ → fn name(&self)       │
│  │          │  │ ...          │                        │
│  │          │  └──────────────┘                        │
│  └──────────┘                                          │
│                                                        │
│  La vtable contiene:                                   │
│  - Funciones de destruccion (drop)                     │
│  - Size y alignment del tipo concreto                  │
│  - Punteros a las implementaciones de cada metodo      │
│                                                        │
│  Las vtables son ESTATICAS — generadas en compilacion  │
│  No hay cache en runtime — solo un puntero constante   │
│  Cada (Trait, ConcreteType) tiene su propia vtable     │
└────────────────────────────────────────────────────────┘
```

### Diferencias clave en fat pointers

| Aspecto | Go (iface) | Rust (&dyn Trait) |
|---------|-----------|-------------------|
| Generacion | Runtime (cache global) | Compile-time (static) |
| Contiene info de tipo | Si (para type assertions) | Solo metodos + drop + size |
| type switch/assertion | Si — core feature | No — usa `Any` trait si necesitas downcast |
| Overhead de creacion | Primer uso: cache miss. Siguiente: rapido | Zero — vtable es constante en .rodata |
| Destruccion | GC se encarga | vtable tiene `drop_fn` |
| Almacenamiento de valor | Puede optimizar: valores <= 1 word se almacenan inline en `data` | Siempre puntero a heap (Box) o stack (&) |

### C — sin fat pointers (manual)

```c
// C: si quieres polimorfismo, construyes tu propio fat pointer
struct Checker {
    void* data;                           // "valor concreto"
    const char* (*check)(void* self);     // "vtable manual"
    const char* (*name)(void* self);
};

// "Implementacion" para un tipo concreto
const char* db_check(void* self) {
    struct DBChecker* db = (struct DBChecker*)self;  // cast manual, unsafe
    return db->healthy ? "ok" : "fail";
}

// Crear el "interface value" manualmente
struct Checker make_db_checker(struct DBChecker* db) {
    return (struct Checker){
        .data = db,
        .check = db_check,
        .name = db_name,
    };
}

// Uso — identico concepto, 100% manual
void run_check(struct Checker c) {
    printf("%s: %s\n", c.name(c.data), c.check(c.data));
}
```

---

## 6. Composicion: Embedding vs Supertraits

### Go — Embedding de interfaces

```go
// Go: composicion por embedding — union de method sets
type Reader interface {
    Read(p []byte) (n int, err error)
}

type Writer interface {
    Write(p []byte) (n int, err error)
}

// ReadWriter embebe Reader y Writer — hereda sus metodos
type ReadWriter interface {
    Reader
    Writer
}

// Un tipo que tenga Read() y Write() satisface las tres interfaces
// La composicion es al nivel de la INTERFAZ, no del tipo concreto
```

### Rust — Supertraits (herencia de traits)

```rust
// Rust: supertraits — un trait REQUIERE que otro ya este implementado
trait Reader {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize, std::io::Error>;
}

trait Writer {
    fn write(&mut self, buf: &[u8]) -> Result<usize, std::io::Error>;
}

// ReadWriter requiere que el tipo implemente Reader Y Writer
trait ReadWriter: Reader + Writer {
    // Puede agregar metodos adicionales, o estar vacio
}

// IMPORTANTE: implementar ReadWriter para un tipo requiere
// implementar TAMBIEN Reader y Writer por separado
struct File { /* ... */ }

impl Reader for File {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize, std::io::Error> {
        Ok(0)
    }
}

impl Writer for File {
    fn write(&mut self, buf: &[u8]) -> Result<usize, std::io::Error> {
        Ok(buf.len())
    }
}

// Solo ahora puedes implementar ReadWriter (que puede estar vacio)
impl ReadWriter for File {}

// Versus Go donde *File satisface ReadWriter automaticamente
// si tiene Read() y Write() — sin impl explicito
```

### Diferencia critica en composicion

```
┌──────────────────────────────────────────────────────────────────────┐
│  COMPOSICION: Go embedding vs Rust supertraits                       │
├────────────────┬────────────────────────┬────────────────────────────┤
│  Aspecto       │  Go                    │  Rust                      │
├────────────────┼────────────────────────┼────────────────────────────┤
│  Sintaxis      │  type RW interface {   │  trait RW: R + W {}        │
│                │      Reader            │                            │
│                │      Writer            │                            │
│                │  }                     │                            │
├────────────────┼────────────────────────┼────────────────────────────┤
│  Satisfaccion  │  Automatica si tipo    │  Requiere impl explicito   │
│                │  tiene los metodos     │  de R, W, Y RW             │
├────────────────┼────────────────────────┼────────────────────────────┤
│  Metodos       │  Solo union de metodos │  Puede agregar metodos     │
│  adicionales   │  embebidos + propios   │  y metodos con default     │
├────────────────┼────────────────────────┼────────────────────────────┤
│  Conversion    │  RW → R implicita      │  &dyn RW → &dyn R NO      │
│  descendente   │  (assignment directo)  │  (requiere supertrait      │
│                │                        │  upcasting, estabilizado   │
│                │                        │  en Rust 1.76+)            │
├────────────────┼────────────────────────┼────────────────────────────┤
│  Default       │  No existen            │  Metodos con body default: │
│  methods       │                        │  trait R { fn read_all()   │
│                │                        │  { default impl } }        │
└────────────────┴────────────────────────┴────────────────────────────┘
```

### Default methods — la ventaja de Rust

```rust
// Rust: un trait puede tener metodos con implementacion default
trait HealthChecker {
    // Metodo requerido — el implementador DEBE proveerlo
    fn check(&self) -> Result<(), String>;

    // Metodo requerido
    fn name(&self) -> &str;

    // Metodo con default — el implementador puede override o heredar
    fn check_with_retry(&self, retries: usize) -> Result<(), String> {
        for i in 0..retries {
            match self.check() {
                Ok(()) => return Ok(()),
                Err(e) if i == retries - 1 => return Err(e),
                Err(_) => continue,
            }
        }
        Err("max retries exhausted".into())
    }

    // Otro default — compone sobre check()
    fn is_healthy(&self) -> bool {
        self.check().is_ok()
    }
}

// Implementador solo necesita proveer check() y name()
// check_with_retry() y is_healthy() se "heredan" gratis
struct DBChecker { host: String }
impl HealthChecker for DBChecker {
    fn check(&self) -> Result<(), String> { Ok(()) }
    fn name(&self) -> &str { &self.host }
    // check_with_retry e is_healthy se heredan del default
}
```

```go
// Go: no tiene default methods — debes usar embedding de structs
// o funciones libres que acepten la interfaz

type HealthChecker interface {
    Check() error
    Name() string
}

// "Default method" en Go = funcion libre que acepta la interfaz
func CheckWithRetry(c HealthChecker, retries int) error {
    var lastErr error
    for i := 0; i < retries; i++ {
        lastErr = c.Check()
        if lastErr == nil {
            return nil
        }
    }
    return lastErr
}

// O usar un struct base embedido (simulacion de herencia):
type BaseChecker struct{}
func (b BaseChecker) CheckWithRetry(c HealthChecker, retries int) error {
    // ... misma logica ...
    return nil
}
```

---

## 7. Generics y Constraints: Go vs Rust

Desde Go 1.18, Go tiene genericos con **type constraints** que son interfaces. Rust tuvo genericos con **trait bounds** desde el dia uno (1.0). La interaccion entre generics y el sistema de interfaces/traits es reveladora.

### Go — Type constraints son interfaces

```go
// Go 1.18+: los constraints de genericos SON interfaces
// Esto unifica dos conceptos en uno

// Constraint basico: cualquier tipo que tenga String()
type Stringer interface {
    String() string
}

func PrintAll[T Stringer](items []T) {
    for _, item := range items {
        fmt.Println(item.String())
    }
}

// Constraint con type sets (Go exclusivo):
type Number interface {
    ~int | ~int32 | ~int64 | ~float32 | ~float64
}

func Sum[T Number](nums []T) T {
    var total T
    for _, n := range nums {
        total += n
    }
    return total
}

// comparable — constraint builtin para tipos que soportan == y !=
func Contains[T comparable](slice []T, target T) bool {
    for _, v := range slice {
        if v == target {
            return true
        }
    }
    return false
}
```

### Rust — Trait bounds

```rust
// Rust: los constraints de genericos son trait bounds
// Concepto separado de trait objects (dyn)

// Trait bound basico
fn print_all<T: std::fmt::Display>(items: &[T]) {
    for item in items {
        println!("{}", item);
    }
}

// Multiples bounds con +
fn process<T: Clone + std::fmt::Debug + PartialEq>(item: &T) {
    let copy = item.clone();
    println!("{:?}", copy);
}

// Where clause para bounds complejos
fn complex_op<T, U>(t: &T, u: &U) -> String
where
    T: std::fmt::Display + Clone,
    U: std::fmt::Debug + Into<String>,
{
    format!("{} / {:?}", t, u)
}

// Associated types — un trait puede tener tipos asociados
trait Iterator {
    type Item; // tipo asociado — cada implementador elige el tipo
    fn next(&mut self) -> Option<Self::Item>;
}

// Equivalente en Go (mas limitado):
// type Iterator[T any] interface {
//     Next() (T, bool)
// }
// Go necesita parametro de tipo explicito; Rust lo infiere del impl
```

### Diferencias en genericos

```
┌──────────────────────────────────────────────────────────────────────────┐
│  GENERICS — Go vs Rust                                                   │
├──────────────────┬───────────────────────┬───────────────────────────────┤
│  Aspecto         │  Go                   │  Rust                         │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│  Constraints     │  Interfaces (con      │  Trait bounds                 │
│                  │  type sets opcionales)│                               │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│  Implementacion  │  Hibrido: GCShape     │  Monomorphization pura        │
│                  │  stenciling           │  (una copia por tipo)         │
│                  │  (diccionarios)       │                               │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│  Type sets       │  ~int | ~string       │  No existe — usa macros       │
│  (union types)   │  (exclusivo de Go)    │  o traits separados           │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│  Associated      │  No existen           │  type Item; en traits         │
│  types           │  (usa parametros)     │  (reduce parametros)          │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│  Const generics  │  No                   │  Si: [T; N] donde N: usize   │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│  Specialization  │  No                   │  Nightly (inestable)          │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│  Compile time    │  Rapido (menos codigo │  Lento (genera mucho codigo)  │
│                  │  generado)            │                               │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│  Madurez         │  Go 1.18 (2022)       │  Rust 1.0 (2015) — 7 años    │
│                  │  Todavia evolucionando│  antes, muy maduro            │
└──────────────────┴───────────────────────┴───────────────────────────────┘
```

---

## 8. Error Handling: error Interface vs Result<T, E>

Go y Rust manejan errores de formas profundamente diferentes, y la raiz esta en como cada uno usa su sistema de polimorfismo.

### Go — error es una interfaz

```go
// error es una interfaz con un solo metodo:
type error interface {
    Error() string
}

// Cualquier tipo que tenga Error() string es un error
// Esto es la satisfaccion implicita en accion

type DNSError struct {
    Domain string
    Code   int
}

func (e *DNSError) Error() string {
    return fmt.Sprintf("DNS lookup failed for %s (code %d)", e.Domain, e.Code)
}

// Funciones devuelven (result, error) — por convencion, NO por el sistema de tipos
func resolve(domain string) (string, error) {
    if domain == "" {
        return "", &DNSError{Domain: domain, Code: 3} // NXDOMAIN
    }
    return "192.168.1.1", nil
}

// El caller DEBERIA verificar err, pero el compilador NO obliga
func main() {
    ip, err := resolve("example.com")
    // Si ignoras err, compila perfecto — pero es un bug silencioso
    _ = err // explicitamente ignorar para que `go vet` no se queje
    fmt.Println(ip)

    // Patron idiomatico:
    ip, err = resolve("")
    if err != nil {
        var dnsErr *DNSError
        if errors.As(err, &dnsErr) {
            fmt.Printf("DNS error: domain=%s, code=%d\n", dnsErr.Domain, dnsErr.Code)
        }
        return
    }
}
```

### Rust — Result<T, E> es un tipo algebraico

```rust
// Result es un enum (tipo algebraico) — no es un trait
enum Result<T, E> {
    Ok(T),
    Err(E),
}

// Error es un trait (similar a la interfaz error de Go)
trait Error: std::fmt::Display + std::fmt::Debug {
    fn source(&self) -> Option<&(dyn Error + 'static)> { None }
}

#[derive(Debug)]
struct DNSError {
    domain: String,
    code: u32,
}

impl std::fmt::Display for DNSError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "DNS lookup failed for {} (code {})", self.domain, self.code)
    }
}

impl std::error::Error for DNSError {}

fn resolve(domain: &str) -> Result<String, DNSError> {
    if domain.is_empty() {
        return Err(DNSError { domain: domain.into(), code: 3 });
    }
    Ok("192.168.1.1".into())
}

fn main() {
    // El compilador OBLIGA a manejar el Result
    // Esto NO compila:
    // let ip = resolve("example.com"); // ip es Result, no String
    // println!("{}", ip);  // ERROR: Result no implementa Display directamente

    // DEBES hacer match, if let, o usar ?
    match resolve("example.com") {
        Ok(ip) => println!("{}", ip),
        Err(e) => eprintln!("Error: {}", e),
    }

    // Operador ? — propaga errores automaticamente
    // (solo funciona dentro de funciones que devuelven Result)
}
```

### El operador ? de Rust — lo que Go no tiene

```rust
// Rust: ? propaga errores limpiamente
fn setup_infrastructure() -> Result<(), Box<dyn std::error::Error>> {
    let db = connect_database()?;     // si Err, return Err inmediatamente
    let cache = connect_cache()?;      // idem
    let dns = resolve_domain()?;       // idem
    configure_services(&db, &cache, &dns)?;
    Ok(())
}

// Equivalente en Go — mucho mas verboso
func setupInfrastructure() error {
    db, err := connectDatabase()
    if err != nil {
        return fmt.Errorf("database: %w", err)
    }
    cache, err := connectCache()
    if err != nil {
        return fmt.Errorf("cache: %w", err)
    }
    dns, err := resolveDomain()
    if err != nil {
        return fmt.Errorf("dns: %w", err)
    }
    if err := configureServices(db, cache, dns); err != nil {
        return fmt.Errorf("configure: %w", err)
    }
    return nil
}
```

### Comparacion de error handling

```
┌──────────────────────────────────────────────────────────────────────────┐
│  ERROR HANDLING — Go vs Rust                                             │
├──────────────────┬───────────────────────┬───────────────────────────────┤
│  Aspecto         │  Go                   │  Rust                         │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│  Tipo            │  error (interfaz)     │  Result<T, E> (enum)          │
│  Verificacion    │  Convencion           │  Compilador obliga            │
│  Ignorar error   │  Compila (bug)        │  Warning: unused Result       │
│  Propagacion     │  if err != nil {      │  ? operator (1 caracter)      │
│                  │    return err         │                               │
│                  │  }                    │                               │
│  Wrapping        │  fmt.Errorf("%w", e)  │  .map_err() / From trait      │
│  Inspeccion      │  errors.Is/errors.As  │  match / if let / downcast    │
│  nil pitfall     │  Si (T03 anterior)    │  No — Option/Result           │
│  Panic           │  panic() — no captura │  panic!() — catch_unwind      │
│                  │  limpia en general    │  (no recomendado)             │
│  Filosofia       │  "Errors are values"  │  "Make illegal states         │
│                  │                       │   unrepresentable"            │
└──────────────────┴───────────────────────┴───────────────────────────────┘
```

---

## 9. Marker Traits y Capacidades del Sistema de Tipos

Rust tiene traits que **no tienen metodos** pero marcan propiedades del tipo. Go no tiene equivalente.

### Rust — Marker traits

```rust
// Send: el tipo puede enviarse entre threads de forma segura
// Sync: el tipo puede compartirse entre threads (&T es Send)
// Copy: el tipo se copia en lugar de moverse (bitwise copy)
// Sized: el tipo tiene tamaño conocido en compilacion
// Unpin: el tipo puede moverse despues de ser pinned

// Estos traits son "auto-traits" — el compilador los implementa
// automaticamente si todos los campos del struct tambien los cumplen

struct SafeCounter {
    count: std::sync::atomic::AtomicUsize,
}
// SafeCounter: Send + Sync automaticamente (AtomicUsize es Send+Sync)

struct UnsafeCounter {
    count: std::cell::Cell<usize>, // Cell no es Sync
}
// UnsafeCounter: Send pero NO Sync — el compilador lo sabe

// Esto IMPIDE en compilacion:
// use std::thread;
// let counter = UnsafeCounter { count: Cell::new(0) };
// let r = &counter;
// thread::spawn(move || {
//     r.count.set(1); // ERROR: Cell<usize> cannot be shared between threads
// });

// Go no tiene equivalente — data races son bugs de runtime
// detectados con: go run -race
```

### Go — no tiene marker traits ni auto-traits

```go
// Go: no hay forma de marcar un tipo como "safe to share between goroutines"
// El programador debe saberlo y usar sync.Mutex o channels

type Counter struct {
    mu    sync.Mutex
    count int
}

// ¿Es Counter seguro para concurrencia? Solo si usas mu correctamente.
// El compilador no verifica esto.
// La unica herramienta es: go run -race (detector de data races en runtime)
```

### Tabla de marker traits sin equivalente en Go

| Rust Marker Trait | Proposito | Equivalente en Go |
|-------------------|-----------|-------------------|
| `Send` | Puede enviarse a otro thread | No existe — el programador decide |
| `Sync` | `&T` puede compartirse entre threads | No existe — usa sync.Mutex |
| `Copy` | Semantica de copia (bitwise) | Todos los tipos de Go se copian por valor por defecto |
| `Clone` | Clone explícito (puede ser deep copy) | No existe — copia manual o `json.Marshal/Unmarshal` hack |
| `Sized` | Tamaño conocido en compilacion | Todos los tipos en Go tienen tamaño conocido |
| `Unpin` | Puede moverse en memoria | No aplica — Go tiene GC con movimiento |
| `Drop` (no marker, pero auto) | Destructor deterministico | `defer` / `Close()` por convencion (no automatico) |

---

## 10. Testing y Mocking

El modelo de interfaces afecta directamente como se escribe codigo testeable.

### Go — Mocking es trivial gracias a interfaces implicitas

```go
// Produccion: cliente HTTP real
type HTTPClient interface {
    Do(req *http.Request) (*http.Response, error)
}

// La funcion bajo test acepta la interfaz
func fetchStatus(client HTTPClient, url string) (int, error) {
    req, err := http.NewRequest("GET", url, nil)
    if err != nil {
        return 0, err
    }
    resp, err := client.Do(req)
    if err != nil {
        return 0, err
    }
    defer resp.Body.Close()
    return resp.StatusCode, nil
}

// Test: mock inline — NO necesita framework
type mockClient struct {
    statusCode int
    err        error
}

func (m *mockClient) Do(req *http.Request) (*http.Response, error) {
    if m.err != nil {
        return nil, m.err
    }
    return &http.Response{
        StatusCode: m.statusCode,
        Body:       io.NopCloser(strings.NewReader("")),
    }, nil
}

func TestFetchStatus(t *testing.T) {
    client := &mockClient{statusCode: 200}
    code, err := fetchStatus(client, "http://example.com")
    if err != nil {
        t.Fatal(err)
    }
    if code != 200 {
        t.Errorf("expected 200, got %d", code)
    }
}

// Nota: *http.Client satisface HTTPClient sin saberlo
// Esto es la magia de la satisfaccion implicita
```

### Rust — Mocking requiere mas ceremonia

```rust
// Produccion: trait explicito
trait HttpClient {
    fn do_request(&self, url: &str) -> Result<u16, String>;
}

// Implementacion real
struct RealClient;
impl HttpClient for RealClient {
    fn do_request(&self, url: &str) -> Result<u16, String> {
        // ... HTTP real ...
        Ok(200)
    }
}

// Funcion bajo test — usa genericos para static dispatch (mas eficiente)
fn fetch_status<C: HttpClient>(client: &C, url: &str) -> Result<u16, String> {
    client.do_request(url)
}

// Test: mock manual
struct MockClient {
    status_code: u16,
}

impl HttpClient for MockClient {
    fn do_request(&self, _url: &str) -> Result<u16, String> {
        Ok(self.status_code)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_fetch_status() {
        let client = MockClient { status_code: 200 };
        let code = fetch_status(&client, "http://example.com").unwrap();
        assert_eq!(code, 200);
    }
}

// Con mockall (crate popular):
// #[cfg(test)]
// use mockall::{automock, predicate::*};
//
// #[automock]  // genera MockHttpClient automaticamente
// trait HttpClient {
//     fn do_request(&self, url: &str) -> Result<u16, String>;
// }
```

### Diferencias en testing

```
┌──────────────────────────────────────────────────────────────────────────┐
│  TESTING — Go vs Rust                                                    │
├──────────────────┬───────────────────────┬───────────────────────────────┤
│  Aspecto         │  Go                   │  Rust                         │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│  Mock inline     │  Trivial — struct con │  Posible pero requiere        │
│                  │  metodos que coinciden│  impl TraitName for MockType  │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│  Mock framework  │  Raro — la mayoria    │  mockall, mockers — mas       │
│                  │  prefiere mocks       │  comunes porque la ceremonia  │
│                  │  manuales             │  manual es mayor              │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│  Interfaces para │  Definidas en el      │  Definidas donde se declaran │
│  test            │  consumidor (patron   │  — el "consumidor" las        │
│                  │  idiomatico de Go)    │  importa                      │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│  Overhead        │  Zero — interfaces    │  Zero si static dispatch      │
│                  │  son gratis en Go     │  Overhead si dyn dispatch     │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│  httptest        │  httptest.NewServer   │  No hay equivalente stdlib    │
│                  │  (stdlib incluido)    │  wiremock, mockito crates     │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│  Race detector   │  go test -race        │  cargo miri (experimental)    │
│                  │  (runtime, maduro)    │  Pero la mayoria de data      │
│                  │                       │  races son imposibles         │
└──────────────────┴───────────────────────┴───────────────────────────────┘
```

---

## 11. Orphan Rule y Coherencia — el Precio de Rust

Rust tiene una regla que Go evita completamente: la **orphan rule**. Esta regla previene conflictos pero limita la flexibilidad.

### Rust — Orphan rule

```rust
// La regla: puedes implementar un trait T para un tipo S SOLO si:
// - T esta definido en tu crate, O
// - S esta definido en tu crate
// (al menos uno de los dos debe ser tuyo)

// Ejemplo: quieres que Vec<String> implemente tu trait
trait Deployable {
    fn deploy(&self) -> Result<(), String>;
}

// ESTO FALLA:
// impl Deployable for Vec<String> { ... }
// ERROR: ni Deployable (tuyo) ni Vec (stdlib) estan ambos en tu crate
// Espera, Deployable SI es tuyo — esto SI compila.

// Lo que NO puedes hacer:
// impl std::fmt::Display for Vec<String> { ... }
// ERROR: ni Display (stdlib) ni Vec<String> (stdlib) son tuyos
// No puedes implementar traits de otros para tipos de otros

// Solucion: newtype pattern
struct ServiceList(Vec<String>);

impl std::fmt::Display for ServiceList {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "[{}]", self.0.join(", "))
    }
}
```

### Go — Sin orphan rule

```go
// Go: no hay orphan rule — satisfaccion implicita = no hay conflicto posible

// Puedes definir una interfaz y CUALQUIER tipo la satisface
// sin que el tipo sepa que la interfaz existe

// Ejemplo: interfaz en tu paquete
type Deployable interface {
    Deploy() error
}

// Un tipo de otro paquete puede satisfacerla sin saberlo
// Si http.Server tuviera un metodo Deploy() error,
// seria Deployable sin ninguna declaracion

// Ventaja: maxima flexibilidad
// Desventaja: no hay registro central de "quien implementa que"
// Herramientas como gopls/guru deducen las relaciones
```

### Coherencia en Rust — beneficio de la orphan rule

```rust
// La orphan rule garantiza COHERENCIA: para cualquier par (Trait, Type),
// hay COMO MAXIMO una implementacion en todo el programa.

// Esto permite al compilador:
// 1. Resolver metodos sin ambiguedad
// 2. Generar vtables correctas
// 3. Asegurar que dos crates no provean impls conflictivas

// Go no necesita coherencia porque:
// 1. La satisfaccion es implicita — no hay "implementaciones" en conflicto
// 2. El method set de un tipo es fijo (definido en su paquete)
// 3. No puedes agregar metodos a tipos de otro paquete
```

---

## 12. Tabla Exhaustiva de Equivalencias

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  EQUIVALENCIAS COMPLETAS: Go ↔ Rust ↔ C                                        │
├──────────────────────┬────────────────────────┬──────────────────────────────────┤
│  Concepto            │  Go                    │  Rust                            │
├──────────────────────┼────────────────────────┼──────────────────────────────────┤
│  Polimorfismo        │  interface I { M() }   │  trait T { fn m(&self); }        │
│  Satisfaccion        │  Implicita (duck)      │  Explicita (impl T for S)        │
│  Dynamic dispatch    │  interface value       │  &dyn Trait / Box<dyn Trait>     │
│  Static dispatch     │  No existe             │  impl Trait / <T: Trait> (default)│
│  Fat pointer         │  (itab, data) 16B      │  (data, vtable) 16B              │
│  Composicion         │  Embedding interfaces  │  Supertraits (T: A + B)          │
│  Default methods     │  No existe             │  fn m(&self) { default }          │
│  Generics constraint │  interface (type set)  │  trait bound (<T: Trait>)         │
│  Associated types    │  No existe             │  type Item; en trait              │
│  Type assertion      │  val.(ConcreteType)    │  Any::downcast_ref (raro)         │
│  Type switch         │  switch v := i.(type)  │  enum + match (diferente)         │
│  Tipo universal      │  any / interface{}     │  dyn Any (raro)                   │
│  Error type          │  error (interface)     │  dyn Error / impl Error           │
│  Error propagation   │  if err != nil { }     │  ? operator                       │
│  Null/nil safety     │  nil (T03 — trampas)   │  Option<T> (compile-time)         │
│  Thread safety       │  -race flag (runtime)  │  Send/Sync (compile-time)         │
│  Destructor          │  defer + Close()       │  Drop trait (automatico)           │
│  Conversion          │  i.(T) / i.(type)      │  From/Into traits                  │
│  Iteracion           │  range (builtin)       │  Iterator trait                    │
│  Comparacion         │  == (comparable types) │  PartialEq/Eq traits               │
│  Ordenacion          │  sort.Interface        │  PartialOrd/Ord traits             │
│  Stringification     │  fmt.Stringer          │  Display trait                     │
│  Debug print         │  fmt.GoStringer        │  Debug trait (derivable)           │
│  Copia               │  Siempre valor         │  Clone/Copy traits                 │
│  Serialization       │  encoding/json (tags)  │  serde (derive macros)             │
│  Marker types        │  No existe             │  Send, Sync, Copy, Sized, Unpin    │
│  Sealed interfaces   │  Metodo unexported     │  Sealed trait pattern (mod privado)│
│  Orphan rule         │  No existe             │  Si — coherencia garantizada        │
├──────────────────────┼────────────────────────┼──────────────────────────────────┤
│  C equivalente       │  void* + func ptrs     │  —                                │
│  C type safety       │  Ninguna               │  —                                │
│  C dispatch          │  Manual (switch/fptr)  │  —                                │
│  C error handling    │  int return + errno    │  —                                │
└──────────────────────┴────────────────────────┴──────────────────────────────────┘
```

---

## 13. Programa Completo: El Mismo Sistema en Go y Rust

Implementamos el mismo sistema — **Infrastructure Service Monitor** — en ambos lenguajes para comparar linea por linea.

### Version Go

```go
// ============================================================================
// INFRASTRUCTURE SERVICE MONITOR — Version Go
//
// Demuestra:
// - Interfaces implicitas
// - Dynamic dispatch (unica opcion)
// - Composicion por embedding
// - Error handling con error interface
// - Colecciones heterogeneas naturales
// - Mocking trivial
//
// go run main.go
// ============================================================================

package main

import (
    "fmt"
    "strings"
    "time"
)

// ─── Interfaces (contratos) ───

// ServiceChecker — interfaz atomica (1 metodo util + 1 de metadata)
type ServiceChecker interface {
    Check() error
    Name() string
}

// Notifier — interfaz atomica
type Notifier interface {
    Notify(service, message string) error
}

// MonitorReporter — composicion por embedding
type MonitorReporter interface {
    ServiceChecker
    Report() string
}

// ─── Tipos de error ───

type ServiceError struct {
    Service string
    Detail  string
    At      time.Time
}

func (e *ServiceError) Error() string {
    return fmt.Sprintf("[%s] %s: %s", e.At.Format("15:04:05"), e.Service, e.Detail)
}

// ─── Implementaciones concretas ───

// DatabaseService
type DatabaseService struct {
    Host    string
    Port    int
    Healthy bool
    Latency time.Duration
}

func (d *DatabaseService) Name() string {
    return fmt.Sprintf("postgres(%s:%d)", d.Host, d.Port)
}

func (d *DatabaseService) Check() error {
    if !d.Healthy {
        return &ServiceError{Service: d.Name(), Detail: "connection refused", At: time.Now()}
    }
    if d.Latency > 100*time.Millisecond {
        return &ServiceError{Service: d.Name(), Detail: fmt.Sprintf("slow: %v", d.Latency), At: time.Now()}
    }
    return nil // nil puro — correcto
}

func (d *DatabaseService) Report() string {
    status := "UP"
    if err := d.Check(); err != nil {
        status = "DOWN"
    }
    return fmt.Sprintf("%-30s [%s] latency=%v", d.Name(), status, d.Latency)
}

// CacheService
type CacheService struct {
    Host    string
    Port    int
    Healthy bool
}

func (c *CacheService) Name() string {
    return fmt.Sprintf("redis(%s:%d)", c.Host, c.Port)
}

func (c *CacheService) Check() error {
    if !c.Healthy {
        return &ServiceError{Service: c.Name(), Detail: "timeout", At: time.Now()}
    }
    return nil
}

func (c *CacheService) Report() string {
    status := "UP"
    if err := c.Check(); err != nil {
        status = "DOWN"
    }
    return fmt.Sprintf("%-30s [%s]", c.Name(), status)
}

// ConsoleNotifier
type ConsoleNotifier struct {
    Prefix string
}

func (n *ConsoleNotifier) Notify(service, message string) error {
    fmt.Printf("  [NOTIFY/%s] %s: %s\n", n.Prefix, service, message)
    return nil
}

// ─── Monitor ───

type Monitor struct {
    services  []MonitorReporter
    notifiers []Notifier
}

func (m *Monitor) RunChecks() {
    fmt.Println("=== Go: Infrastructure Monitor ===")
    fmt.Println()

    for _, svc := range m.services {
        err := svc.Check()
        fmt.Printf("  %s\n", svc.Report())

        if err != nil {
            for _, n := range m.notifiers {
                n.Notify(svc.Name(), err.Error())
            }
        }
    }
}

// ─── main ───

func main() {
    monitor := &Monitor{
        // Coleccion heterogenea — natural en Go
        services: []MonitorReporter{
            &DatabaseService{Host: "db-primary", Port: 5432, Healthy: true, Latency: 5 * time.Millisecond},
            &DatabaseService{Host: "db-replica", Port: 5432, Healthy: false, Latency: 0},
            &CacheService{Host: "cache-01", Port: 6379, Healthy: true},
            &CacheService{Host: "cache-02", Port: 6379, Healthy: false},
        },
        notifiers: []Notifier{
            &ConsoleNotifier{Prefix: "SLACK"},
            &ConsoleNotifier{Prefix: "PAGER"},
        },
    }

    monitor.RunChecks()

    fmt.Println()
    fmt.Println("=== Go: Interfaces implicitas, dynamic dispatch, nil puro ===")
    fmt.Println()

    // Verificar que nil funciona correctamente
    for _, svc := range monitor.services {
        err := svc.Check()
        fmt.Printf("  %-30s Check() == nil: %v\n", svc.Name(), err == nil)
    }
}
```

### Version Rust equivalente

```rust
// ============================================================================
// INFRASTRUCTURE SERVICE MONITOR — Version Rust
//
// Demuestra:
// - Traits explicitos (impl Trait for Type)
// - Static dispatch por defecto (genericos)
// - Dynamic dispatch explícito (dyn Trait)
// - Supertraits para composicion
// - Error handling con Result<T, E>
// - Colecciones heterogeneas con Box<dyn Trait>
//
// cargo run
// ============================================================================

use std::fmt;
use std::time::{Duration, Instant};

// ─── Traits (contratos) ───

trait ServiceChecker {
    fn check(&self) -> Result<(), ServiceError>;
    fn name(&self) -> &str;
}

trait Notifier {
    fn notify(&self, service: &str, message: &str) -> Result<(), String>;
}

// Supertrait: MonitorReporter REQUIERE ServiceChecker
trait MonitorReporter: ServiceChecker {
    fn report(&self) -> String;
}

// ─── Tipos de error ───

#[derive(Debug)]
struct ServiceError {
    service: String,
    detail: String,
}

impl fmt::Display for ServiceError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}: {}", self.service, self.detail)
    }
}

impl std::error::Error for ServiceError {}

// ─── Implementaciones concretas ───

struct DatabaseService {
    host: String,
    port: u16,
    healthy: bool,
    latency: Duration,
}

// impl inherente — metodos propios (constructores, helpers)
impl DatabaseService {
    fn new(host: &str, port: u16, healthy: bool, latency: Duration) -> Self {
        DatabaseService {
            host: host.into(),
            port,
            healthy,
            latency,
        }
    }

    fn full_name(&self) -> String {
        format!("postgres({}:{})", self.host, self.port)
    }
}

// impl del trait — separado del inherente
impl ServiceChecker for DatabaseService {
    fn check(&self) -> Result<(), ServiceError> {
        if !self.healthy {
            return Err(ServiceError {
                service: self.full_name(),
                detail: "connection refused".into(),
            });
        }
        if self.latency > Duration::from_millis(100) {
            return Err(ServiceError {
                service: self.full_name(),
                detail: format!("slow: {:?}", self.latency),
            });
        }
        Ok(()) // No hay nil — Ok() es exito explicito
    }

    fn name(&self) -> &str {
        &self.host
    }
}

impl MonitorReporter for DatabaseService {
    fn report(&self) -> String {
        let status = if self.check().is_ok() { "UP" } else { "DOWN" };
        format!("{:<30} [{}] latency={:?}", self.full_name(), status, self.latency)
    }
}

struct CacheService {
    host: String,
    port: u16,
    healthy: bool,
}

impl CacheService {
    fn new(host: &str, port: u16, healthy: bool) -> Self {
        CacheService { host: host.into(), port, healthy }
    }

    fn full_name(&self) -> String {
        format!("redis({}:{})", self.host, self.port)
    }
}

impl ServiceChecker for CacheService {
    fn check(&self) -> Result<(), ServiceError> {
        if !self.healthy {
            return Err(ServiceError {
                service: self.full_name(),
                detail: "timeout".into(),
            });
        }
        Ok(())
    }

    fn name(&self) -> &str {
        &self.host
    }
}

impl MonitorReporter for CacheService {
    fn report(&self) -> String {
        let status = if self.check().is_ok() { "UP" } else { "DOWN" };
        format!("{:<30} [{}]", self.full_name(), status)
    }
}

struct ConsoleNotifier {
    prefix: String,
}

impl Notifier for ConsoleNotifier {
    fn notify(&self, service: &str, message: &str) -> Result<(), String> {
        println!("  [NOTIFY/{}] {}: {}", self.prefix, service, message);
        Ok(())
    }
}

// ─── Monitor ───

struct Monitor {
    // Colecciones heterogeneas requieren Box<dyn Trait>
    services: Vec<Box<dyn MonitorReporter>>,
    notifiers: Vec<Box<dyn Notifier>>,
}

impl Monitor {
    fn run_checks(&self) {
        println!("=== Rust: Infrastructure Monitor ===");
        println!();

        for svc in &self.services {
            let result = svc.check(); // Result, no error nullable
            println!("  {}", svc.report());

            if let Err(e) = result {
                for n in &self.notifiers {
                    let _ = n.notify(svc.name(), &e.to_string());
                }
            }
        }
    }
}

// ─── main ───

fn main() {
    let monitor = Monitor {
        services: vec![
            Box::new(DatabaseService::new("db-primary", 5432, true, Duration::from_millis(5))),
            Box::new(DatabaseService::new("db-replica", 5432, false, Duration::from_millis(0))),
            Box::new(CacheService::new("cache-01", 6379, true)),
            Box::new(CacheService::new("cache-02", 6379, false)),
        ],
        notifiers: vec![
            Box::new(ConsoleNotifier { prefix: "SLACK".into() }),
            Box::new(ConsoleNotifier { prefix: "PAGER".into() }),
        ],
    };

    monitor.run_checks();

    println!();
    println!("=== Rust: Traits explicitos, static+dyn dispatch, Result<T,E> ===");
    println!();

    // No hay "nil check" — Result es is_ok() o is_err()
    for svc in &monitor.services {
        let ok = svc.check().is_ok();
        println!("  {:<30} check().is_ok(): {}", svc.name(), ok);
    }
}
```

### Analisis comparativo del programa

| Aspecto | Go | Rust |
|---------|-----|------|
| Lineas de codigo | ~130 | ~180 (~40% mas) |
| Boilerplate | Bajo — solo metodos | Alto — impl blocks por trait |
| Coleccion heterogenea | `[]MonitorReporter` | `Vec<Box<dyn MonitorReporter>>` |
| Error handling | `if err != nil` (7x) | `if let Err(e)` / `?` (mas expresivo) |
| nil safety | Responsabilidad del programador | Garantizada por el compilador |
| Thread safety | `go run -race` (runtime) | `Send/Sync` (compile-time) |
| Performance | Dynamic dispatch siempre | Podria usar genericos para static |

---

## 14. Ejercicios

### Ejercicio 1: Traducir interfaz Go a trait Rust

```go
// Traduce esta interfaz Go y su implementacion a Rust.
// Incluye: trait, struct, impl inherente, impl Trait for Struct.

type Deployer interface {
    Deploy(image string, replicas int) error
    Rollback() error
    Status() string
}

type KubernetesDeployer struct {
    Namespace string
    Current   string
}

func (k *KubernetesDeployer) Deploy(image string, replicas int) error {
    k.Current = image
    fmt.Printf("Deploying %s (%d replicas) to %s\n", image, replicas, k.Namespace)
    return nil
}

func (k *KubernetesDeployer) Rollback() error {
    fmt.Printf("Rolling back in %s\n", k.Namespace)
    k.Current = ""
    return nil
}

func (k *KubernetesDeployer) Status() string {
    if k.Current == "" {
        return "idle"
    }
    return fmt.Sprintf("running: %s", k.Current)
}
```

<details>
<summary>Solucion</summary>

```rust
trait Deployer {
    fn deploy(&mut self, image: &str, replicas: u32) -> Result<(), String>;
    fn rollback(&mut self) -> Result<(), String>;
    fn status(&self) -> String;
}

struct KubernetesDeployer {
    namespace: String,
    current: Option<String>, // Option en lugar de string vacio
}

impl KubernetesDeployer {
    fn new(namespace: &str) -> Self {
        KubernetesDeployer {
            namespace: namespace.into(),
            current: None,
        }
    }
}

impl Deployer for KubernetesDeployer {
    fn deploy(&mut self, image: &str, replicas: u32) -> Result<(), String> {
        self.current = Some(image.into());
        println!("Deploying {} ({} replicas) to {}", image, replicas, self.namespace);
        Ok(())
    }

    fn rollback(&mut self) -> Result<(), String> {
        println!("Rolling back in {}", self.namespace);
        self.current = None;
        Ok(())
    }

    fn status(&self) -> String {
        match &self.current {
            None => "idle".into(),
            Some(img) => format!("running: {}", img),
        }
    }
}
```

Diferencias clave:
- `&mut self` en deploy/rollback (Go usaba pointer receiver implicitamente)
- `Option<String>` en lugar de string vacio (Rust hace explicito "hay/no hay valor")
- `Result<(), String>` en lugar de `error` (Rust obliga a manejar)
- `impl Deployer for KubernetesDeployer` — declaracion explicita de intencion
</details>

### Ejercicio 2: Comparar testing approaches

```go
// Dado este codigo Go con interfaz para testing:
type Store interface {
    Get(key string) (string, error)
    Set(key, value string) error
}

func ProcessConfig(store Store, key string) (string, error) {
    val, err := store.Get(key)
    if err != nil {
        return "", fmt.Errorf("get %s: %w", key, err)
    }
    processed := strings.ToUpper(val)
    if err := store.Set(key+"_processed", processed); err != nil {
        return "", fmt.Errorf("set %s: %w", key, err)
    }
    return processed, nil
}

// 1. Escribe el test con mock en Go
// 2. Traduce todo a Rust (trait + implementacion + test)
// 3. Identifica al menos 3 diferencias en la experiencia de testing
```

<details>
<summary>Solucion</summary>

**Go test:**
```go
type mockStore struct {
    data map[string]string
    err  error
}

func (m *mockStore) Get(key string) (string, error) {
    if m.err != nil {
        return "", m.err
    }
    v, ok := m.data[key]
    if !ok {
        return "", fmt.Errorf("not found: %s", key)
    }
    return v, nil
}

func (m *mockStore) Set(key, value string) error {
    if m.err != nil {
        return m.err
    }
    m.data[key] = value
    return nil
}

func TestProcessConfig(t *testing.T) {
    store := &mockStore{data: map[string]string{"db_host": "localhost"}}
    result, err := ProcessConfig(store, "db_host")
    if err != nil {
        t.Fatal(err)
    }
    if result != "LOCALHOST" {
        t.Errorf("expected LOCALHOST, got %s", result)
    }
    if store.data["db_host_processed"] != "LOCALHOST" {
        t.Error("processed value not stored")
    }
}
```

**Rust equivalente:**
```rust
trait Store {
    fn get(&self, key: &str) -> Result<String, String>;
    fn set(&mut self, key: &str, value: &str) -> Result<(), String>;
}

fn process_config(store: &mut impl Store, key: &str) -> Result<String, String> {
    let val = store.get(key).map_err(|e| format!("get {}: {}", key, e))?;
    let processed = val.to_uppercase();
    store.set(&format!("{}_processed", key), &processed)
         .map_err(|e| format!("set {}: {}", key, e))?;
    Ok(processed)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashMap;

    struct MockStore {
        data: HashMap<String, String>,
    }

    impl Store for MockStore {
        fn get(&self, key: &str) -> Result<String, String> {
            self.data.get(key).cloned().ok_or(format!("not found: {}", key))
        }
        fn set(&mut self, key: &str, value: &str) -> Result<(), String> {
            self.data.insert(key.into(), value.into());
            Ok(())
        }
    }

    #[test]
    fn test_process_config() {
        let mut store = MockStore {
            data: HashMap::from([("db_host".into(), "localhost".into())]),
        };
        let result = process_config(&mut store, "db_host").unwrap();
        assert_eq!(result, "LOCALHOST");
        assert_eq!(store.data.get("db_host_processed").unwrap(), "LOCALHOST");
    }
}
```

**3 diferencias:**
1. En Rust necesitas `impl Store for MockStore` explicito; en Go basta con que MockStore tenga los metodos
2. Rust usa `&mut impl Store` (static dispatch en test, zero overhead); Go siempre dynamic dispatch
3. Rust test maneja errores con `.unwrap()` (panic claro); Go usa `t.Fatal(err)` (reporta y para)
</details>

### Ejercicio 3: Analizar tradeoffs de diseno

```
Escenario: estas disenando un sistema de plugins para un monitoring tool.
Los plugins pueden ser: checkers, notifiers, formatters, y transformers.

Responde para AMBOS lenguajes:

1. ¿Usarias interfaces/traits pequenas (1 metodo) o grandes (4+ metodos)?
   Justifica.

2. ¿Como manejarias un plugin que necesita inicializacion y cleanup?
   (En Go: Close()? defer? En Rust: Drop trait? RAII?)

3. ¿Como harias el registro de plugins en runtime?
   (En Go: map[string]PluginFactory. En Rust: inventory crate? enum?)

4. ¿Como testear un pipeline de plugins?
   (Mock individual? Integration test?)
```

<details>
<summary>Solucion</summary>

**1. Interfaces/traits pequenas (1-2 metodos) en AMBOS lenguajes:**
- Go: principio fundamental — "the bigger the interface, the weaker the abstraction"
- Rust: traits pequenos permiten usar genericos (static dispatch) eficientemente y componer con `+`
- Ambos: `Checker` (1 metodo), `Notifier` (1 metodo), `Formatter` (1 metodo) separados
- Si un plugin hace todo: satisface/implementa todos los traits, no necesitas uno grande

**2. Inicializacion y cleanup:**
- Go: interfaz `io.Closer` — el caller llama `defer plugin.Close()`. No es automatico — si olvidas defer, resource leak.
- Rust: `Drop` trait — cleanup automatico cuando el valor sale del scope. RAII garantiza que no hay leak. El compilador se encarga.

**3. Registro en runtime:**
- Go: `map[string]func() Plugin` — funcion factory por nombre. Simple, directo, dynamic dispatch natural. Ejemplo: `plugins["slack"] = func() Notifier { return &SlackNotifier{} }`
- Rust: Para plugins como tipos cerrados, un `enum Plugin { Checker(Box<dyn Checker>), ... }`. Para plugins abiertos (loadable), `libloading` crate con `Box<dyn Trait>`. `inventory` crate para registro declarativo.

**4. Testing:**
- Go: mock individual de cada interfaz (trivial). Pipeline test con mocks inyectados. httptest para notifiers HTTP.
- Rust: mock individual con `impl Trait for MockType`. Pipeline test con genericos (static dispatch en test, sin overhead). `mockall` para generar mocks automaticamente si hay muchos traits.
</details>

---

## 15. Resumen Visual

```
┌──────────────────────────────────────────────────────────────────────────┐
│  INTERFACES (Go) vs TRAITS (Rust) — Resumen Completo                     │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  SATISFACCION:                                                           │
│  ├─ Go: implicita — tipo tiene metodos → satisface interfaz             │
│  ├─ Rust: explicita — impl Trait for Type { ... }                       │
│  ├─ Go: descubribilidad dificil (gopls/guru)                            │
│  ├─ Rust: descubribilidad trivial (cargo doc, IDE)                      │
│  └─ Go: satisfaccion accidental posible; Rust: imposible                │
│                                                                          │
│  DISPATCH:                                                               │
│  ├─ Go: siempre dynamic (fat pointer, no inlining)                      │
│  ├─ Rust: static por defecto (monomorphization, inlining)               │
│  ├─ Rust: dynamic opt-in con &dyn Trait / Box<dyn Trait>                │
│  └─ Go: binario mas compacto; Rust: binario mas grande, mas rapido      │
│                                                                          │
│  METHOD SETS vs IMPL BLOCKS:                                             │
│  ├─ Go: metodos sueltos con value/pointer receiver                      │
│  ├─ Rust: agrupados en impl blocks (inherente + por trait)              │
│  ├─ Rust: self/&self/&mut self (ownership/borrow semantics)             │
│  └─ Go: no puede expresar "este metodo consume el recurso"             │
│                                                                          │
│  COMPOSICION:                                                            │
│  ├─ Go: embedding de interfaces (union de method sets)                  │
│  ├─ Rust: supertraits (trait A: B + C)                                  │
│  ├─ Rust: default methods (Go no tiene)                                 │
│  └─ Go: satisfaccion de compuesta automatica; Rust: requiere impl       │
│                                                                          │
│  GENERICOS:                                                              │
│  ├─ Go: constraints = interfaces (con type sets)                        │
│  ├─ Rust: bounds = traits (+ associated types, const generics)          │
│  ├─ Go: compilacion rapida, menos codigo generado                       │
│  └─ Rust: compilacion lenta, zero-cost abstractions                     │
│                                                                          │
│  ERRORES:                                                                │
│  ├─ Go: error interface + if err != nil (no obligatorio)                │
│  ├─ Rust: Result<T,E> + ? operator (obligatorio manejar)               │
│  └─ Go: nil interface trap; Rust: impossible by design                  │
│                                                                          │
│  SAFETY:                                                                 │
│  ├─ Go: race detector (-race) en runtime                                │
│  ├─ Rust: Send/Sync en compile-time                                     │
│  ├─ Go: nil panics posibles                                             │
│  └─ Rust: Option<T> elimina null por diseno                             │
│                                                                          │
│  TESTING:                                                                │
│  ├─ Go: mock trivial (satisfaccion implicita)                           │
│  ├─ Rust: mock requiere impl explicito (o mockall crate)                │
│  └─ Ambos: interfaces/traits pequenas facilitan testing                 │
│                                                                          │
│  ORPHAN RULE:                                                            │
│  ├─ Go: no existe — maxima flexibilidad                                 │
│  ├─ Rust: si — coherencia garantizada, menos flexibilidad               │
│  └─ Rust: newtype pattern como workaround                               │
│                                                                          │
│  FILOSOFIA:                                                              │
│  ├─ Go: "Simple is better" — menos features, mas rapido de aprender    │
│  ├─ Rust: "If it compiles, it works" — mas features, mas seguro        │
│  ├─ C:   "Trust the programmer" — zero features, maximo control         │
│  ├─ Go en ecosistema: Docker, K8s, Terraform, Prometheus                │
│  └─ Rust en ecosistema: Firecracker, ripgrep, fd, Bottlerocket          │
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: C07 - Manejo de Errores, S01 - El Patron de Go, T01 - Error como valor — el tipo error (interfaz), errors.New, fmt.Errorf, por que Go no tiene excepciones
