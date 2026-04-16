# T02 — Cuando usar cada mecanismo

---

## 1. De la teoria a la decision

T01 explico **que** genera cada mecanismo y sus trade-offs. Este tema
responde la pregunta practica: **dado mi problema, cual elijo?**

La decision depende de cuatro preguntas:

```
  1. ¿Quien define los tipos? (yo, el usuario de mi API, plugins)
  2. ¿Necesito mezclar tipos en una coleccion?
  3. ¿Es un hot path?
  4. ¿Estoy en C o en Rust?
```

---

## 2. Las cinco reglas

### Regla 1: Si los tipos son fijos y conocidos → enum

```
  ¿Puedo enumerar TODAS las variantes ahora mismo?
  ¿Es improbable que se agreguen nuevas?

  SI a ambas → enum
```

Ejemplos claros:

| Dominio | Enum natural | Por que |
|---------|-------------|---------|
| Tokens de un parser | `Number`, `Plus`, `String`, ... | Definidos por la gramatica |
| Estados de conexion | `Idle`, `Connecting`, `Connected` | Protocolo fijo |
| Tipos JSON | `Null`, `Bool`, `Number`, `String`, `Array`, `Object` | Estandar RFC |
| Errores de una API | `NotFound`, `Unauthorized`, `Timeout` | Definidos por ti |
| Direcciones | `North`, `South`, `East`, `West` | Realidad fisica |

Anti-ejemplo: entidades de un juego (los game designers agregan
entidades constantemente → no es enum).

### Regla 2: Si el usuario de tu API define los tipos → generics o dyn

```
  ¿Tu API es una libreria que otros consumiran con SUS propios tipos?

  SI → generics (preferido) o dyn Trait
```

El usuario no puede agregar variantes a tu enum. Necesita un trait
que pueda implementar para sus tipos.

```rust
// Libreria de serialization: el usuario define sus tipos
// Generics: el usuario implementa Serialize para su struct
fn to_json<T: Serialize>(value: &T) -> String { ... }

// Libreria de logging: el usuario define su logger
// dyn Trait: el logger se almacena como campo
struct App {
    logger: Box<dyn Logger>,
}
```

### Regla 3: Si necesitas coleccion heterogenea → dyn o enum

```
  ¿Un Vec/array donde cada elemento puede ser un tipo distinto?

  SI → ¿Los tipos son fijos?
       SI → Vec<Enum>           (sin allocation)
       NO → Vec<Box<dyn Trait>> (con allocation)
```

Generics no permite heterogeneidad: `Vec<T>` exige que todos los `T`
sean el mismo tipo.

```rust
// Generics: TODOS iguales
let circles: Vec<Circle> = vec![c1, c2, c3];  // OK

// Enum: tipos mezclados, sin heap
let shapes: Vec<Shape> = vec![
    Shape::Circle(5.0),
    Shape::Rect(3.0, 4.0),
];

// dyn: tipos mezclados, extensible
let shapes: Vec<Box<dyn Draw>> = vec![
    Box::new(Circle(5.0)),
    Box::new(custom_shape),  // tipo del usuario
];
```

### Regla 4: Si almacenas el tipo en un struct → dyn o enum

```
  ¿El tipo va como campo de un struct?

  SI → ¿Generics haria el struct generico (App<T>)?
       SI y es molesto → Box<dyn Trait>
       SI y es aceptable → generics (zero-cost)
```

El problema de generics en campos: el parametro generico "infecta" todo
el codigo que usa el struct:

```rust
// Generics: el tipo del logger infecta App, Server, Config...
struct App<L: Logger> {
    logger: L,  // zero-cost, pero...
}
struct Server<L: Logger> {
    app: App<L>,  // L se propaga
}
// Cada funcion que use Server necesita <L: Logger>

// dyn: el tipo del logger se borra, App es un solo tipo
struct App {
    logger: Box<dyn Logger>,  // un pequeno costo, mucha simpleza
}
struct Server {
    app: App,  // sin generics, limpio
}
```

Regla practica: si el generico se propaga a mas de 2-3 niveles de
structs, considera `dyn`.

### Regla 5: Si es hot path y el tipo se conoce → generics

```
  ¿El codigo se ejecuta millones de veces?
  ¿El tipo concreto se conoce en compile time?

  SI a ambas → generics (permite inlining, vectorizacion)
```

Si el tipo NO se conoce en compile time (viene de config, de un
plugin), generics no aplica — dyn es la unica opcion, y el costo es
aceptable.

---

## 3. Decision en C

C no tiene la misma gama de opciones, pero los principios son iguales:

### 3.1 Tabla de equivalencias

| Situacion | Rust | C |
|-----------|------|---|
| Tipos fijos conocidos | enum | tagged union + switch |
| Tipo generico, una operacion | `impl Fn(T)` | function pointer |
| Tipo generico, N operaciones | `dyn Trait` | vtable manual |
| Funcion generica por tipo | generics | macro generadora |
| Contenedor generico | `Vec<T>` | `void *` + `size_t` |
| Callback con contexto | closure | fn ptr + `void *userdata` |

### 3.2 Reglas para C

```
  ¿Necesito genericidad?
  │
  ├─ NO → struct concreto, funciones directas
  │       (lo mas simple, lo mas rapido)
  │
  └─ SI → ¿Para cuantas operaciones?
          │
          ├─ UNA → function pointer
          │        (qsort, callbacks, signal handlers)
          │
          └─ VARIAS → ¿El conjunto de tipos es fijo?
                      │
                      ├─ SI → tagged union + switch
                      │       (parsers, protocolos, state machines)
                      │
                      └─ NO → vtable manual
                              (plugins, drivers, extensibilidad)

  ¿Necesito contenedor generico?
  │
  ├─ Performance critico → macro generadora (tipo embebido)
  │
  └─ Flexibilidad → void* + size_t + function pointers
```

---

## 4. Escenarios concretos

### 4.1 Contenedor (Vec, HashMap, Stack)

```
  C:    void* + size_t elem_size + callbacks
  Rust: generics (Vec<T>, HashMap<K, V>)

  Por que: el contenedor debe funcionar con CUALQUIER tipo que el
  usuario defina. Generics da zero-cost + type safety. void* es la
  unica opcion en C.
```

### 4.2 Parser / compilador

```
  C:    tagged union (AST nodes) + switch
  Rust: enum (AST nodes) + match

  Por que: los nodos del AST estan definidos por la gramatica — son
  fijos. Se agregan operaciones frecuentemente (eval, typecheck,
  codegen, optimize). Enum es ideal: agregar operacion = nuevo match.
```

### 4.3 Sistema de plugins

```
  C:    vtable manual (struct de function pointers)
  Rust: dyn Trait

  Por que: los plugins son tipos que el autor de la libreria NO conoce.
  Solo dyn Trait (o vtable en C) permite que terceros implementen
  nuevos tipos sin recompilar la libreria.
```

### 4.4 Algoritmo de ordenamiento

```
  C:    void* + comparador fn ptr (qsort)
        O: macro generadora por tipo
  Rust: generics (sort::<T: Ord>)
        O: sort_by con closure

  Por que: el algoritmo es el mismo para todos los tipos. Solo cambia
  la comparacion. Generics genera una version optimizada por tipo.
  void* es la alternativa en C con costo de indirection.
```

### 4.5 Middleware / pipeline

```
  C:    array de fn ptrs + void* context cada uno
  Rust: Vec<Box<dyn Fn>> o Vec<Box<dyn Middleware>>

  Por que: el pipeline se configura en runtime (el usuario elige que
  middleware aplica). Los middleware son tipos heterogeneos (logging,
  auth, rate-limit). dyn Trait es natural.
```

### 4.6 Errores de una aplicacion

```
  C:    enum de codigos + (opcionalmente) union con datos extra
  Rust: enum con datos (thiserror, anyhow)

  Por que: los errores de TU aplicacion son fijos y conocidos.
  No hay extensibilidad externa. Enum da exhaustividad.
```

### 4.7 Base de datos abstracta

```
  C:    vtable manual (struct con open, query, close fn ptrs)
  Rust: dyn Trait con Box (o generico si se sabe en compile time)

  Por que: puedes tener PostgreSQL, SQLite, mock. El tipo se elige
  en runtime (config file). Almacenas en un campo de struct.
  Generics infectaria todo el codigo con <DB: Database>.
```

### 4.8 Operaciones matematicas (Vec2, Matrix)

```
  C:    struct concreto, funciones directas (vec2_add, mat4_mul)
  Rust: struct concreto con impl + operator overloading

  Por que: NO necesitas polimorfismo. Un Vec2 siempre es un Vec2.
  Genericidad aqui solo agrega complejidad. Hot path — acceso directo
  a campos, inlining, SIMD.
```

---

## 5. Anti-patrones: elecciones incorrectas

### 5.1 dyn Trait para tipos fijos

```rust
// MAL: solo hay 3 errores, nunca cambiaran
fn handle_error(e: &dyn AppError) { ... }

// BIEN: enum
enum AppError { Io(io::Error), Parse(String), NotFound }
fn handle_error(e: &AppError) { ... }
```

Pierdes exhaustividad y agregas allocation innecesaria.

### 5.2 Enum para tipos extensibles

```rust
// MAL: los usuarios quieren agregar sus propios formatos
enum OutputFormat {
    Json, Xml, Csv, Yaml, Toml,
    // usuario: "necesito MessagePack" → no puede sin tu codigo
}

// BIEN: trait
trait OutputFormat {
    fn serialize(&self, data: &Data) -> Vec<u8>;
}
// Ahora el usuario implementa MessagePack sin tocarte
```

### 5.3 Generics que infectan todo

```rust
// MAL: L se propaga a 10 niveles
struct Config<L: Logger> { logger: L, /* ... */ }
struct Database<L: Logger> { config: Config<L>, /* ... */ }
struct Server<L: Logger> { db: Database<L>, /* ... */ }
struct App<L: Logger> { server: Server<L>, /* ... */ }

// BIEN: dyn en el punto de almacenamiento
struct Config { logger: Box<dyn Logger>, /* ... */ }
struct Database { config: Config }
struct Server { db: Database }
struct App { server: Server }
```

### 5.4 void* cuando no hay genericidad

```c
// MAL: usar void* para un solo tipo
void process_data(void *data) {
    int *values = (int *)data;
    // siempre es int — por que void*?
}

// BIEN: tipo concreto
void process_data(int *values) {
    // sin casts, sin riesgo
}
```

### 5.5 Vtable manual con un solo tipo

```c
// MAL: vtable con una sola implementacion
typedef struct {
    void (*process)(void *self);
    void (*destroy)(void *self);
} ProcessorVtable;
// Solo existe FileProcessor — nunca hubo otro tipo

// BIEN: funciones directas
void file_processor_process(FileProcessor *fp);
void file_processor_destroy(FileProcessor *fp);
```

No agregues abstraccion sin necesidad real. YAGNI (You Ain't Gonna
Need It).

---

## 6. Migracion entre mecanismos

A veces empiezas con un mecanismo y necesitas cambiar. Estas son las
rutas comunes:

### 6.1 Enum → dyn Trait

Cuando el enum crece demasiado o necesitas extensibilidad:

```rust
// Antes: enum con 20 variantes y creciendo
enum Shape {
    Circle(f64), Rect(f64, f64), /* ...18 mas */
}

// Despues: trait
trait Shape {
    fn area(&self) -> f64;
    fn draw(&self);
}

// Cada variante se convierte en un struct + impl
struct Circle { radius: f64 }
impl Shape for Circle { /* ... */ }
```

Costo: pierdes exhaustividad. Cada `match` se convierte en un metodo
del trait.

### 6.2 dyn Trait → enum

Cuando descubres que los tipos son fijos y quieres exhaustividad:

```rust
// Antes: dyn con 3 tipos que nunca cambian
let loggers: Vec<Box<dyn Logger>> = vec![...];

// Despues: enum
enum Logger { Stdout, File(PathBuf), Syslog }
let loggers: Vec<Logger> = vec![...];
```

Ganancia: stack allocation, exhaustividad, sin Box.

### 6.3 dyn Trait → generics

Cuando descubres que el tipo se conoce en compile time:

```rust
// Antes: dyn innecesario
fn process(db: &dyn Database) { ... }

// Despues: generics (si el tipo es conocido por el llamador)
fn process<D: Database>(db: &D) { ... }
```

Ganancia: inlining, zero-cost. Solo posible si no necesitas almacenar
el tipo como campo o mezclar tipos.

### 6.4 void* → vtable (C)

Cuando pasas de un function pointer suelto a multiples operaciones:

```c
// Antes: callback suelto
typedef void (*ProcessFn)(void *data);

// Despues: vtable (multiples operaciones relacionadas)
typedef struct {
    void (*process)(void *self);
    void (*validate)(void *self);
    void (*cleanup)(void *self);
} ProcessorVtable;
```

### 6.5 Tabla de migraciones

| De | A | Cuando | Costo |
|----|---|--------|-------|
| Enum → dyn | Tipos crecen, necesitas extensibilidad | Pierdes exhaustividad |
| dyn → Enum | Tipos se estabilizaron, quieres match | Pierdes extensibilidad |
| dyn → Generics | Tipo conocido en compile time | Pierdes heterogeneidad |
| Generics → dyn | Generico infecta todo, necesitas almacenar | Pierdes inlining |
| fn ptr → Vtable (C) | De 1 a N operaciones | Mas complejo |
| void* → Tagged union (C) | Tipos se fijan | Menos flexible |

---

## 7. Decision por tipo de proyecto

### 7.1 Libreria publica (crate publicado)

```
  API publica → generics donde sea posible
  Razon: da maxima flexibilidad al usuario
  El usuario monomorphiza en su binario

  Almacenamiento interno → dyn Trait
  Razon: evita infectar la API con generics internos

  Tipos del dominio → enum
  Razon: errores, configuraciones, opciones
```

Ejemplo real: `serde`
- `Serialize` y `Deserialize` son traits (generics para el usuario)
- Internamente, serde usa `dyn` para la maquinaria de serializacion
- `serde_json::Value` es un enum (JSON tiene tipos fijos)

### 7.2 Aplicacion (binario)

```
  Logica de negocio → enum + structs concretos
  Razon: los tipos de TU dominio son conocidos

  Integraciones externas → dyn Trait
  Razon: base de datos, logger, cache pueden cambiar

  Utilidades internas → generics
  Razon: funciones helper que operan sobre tus tipos
```

### 7.3 Sistema embedded (C)

```
  Todo → structs concretos y funciones directas
  Razon: cada byte y ciclo cuenta

  Excepciones:
  - HAL (Hardware Abstraction Layer) → vtable
    (para soportar multiples plataformas)
  - Protocolos → tagged union
    (mensajes con tipo + datos)
```

### 7.4 Kernel / driver (C)

```
  Subsistemas (filesystem, network, device) → vtable
  Razon: extensibilidad (nuevos drivers, nuevos fs)

  Estructuras internas → structs concretos
  Razon: rendimiento critico, sin abstraccion innecesaria
```

Linux kernel usa vtables extensivamente:

```c
// Linux: struct file_operations (vtable de filesystem)
struct file_operations {
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
    int (*open)(struct inode *, struct file *);
    int (*release)(struct inode *, struct file *);
    // ... 20+ operaciones
};
```

---

## 8. Checklist de decision

Para cada punto de polimorfismo en tu codigo, responde en orden:

```
  □ ¿Necesito polimorfismo en absoluto?
    NO → struct concreto, funcion directa. Fin.

  □ ¿Cuantos tipos concretos existen?
    1 solo → no hay polimorfismo. Funcion directa.
    2-10 fijos → enum (probablemente)
    Desconocido/creciente → trait (dyn o generics)

  □ ¿Quien define los tipos?
    Yo → enum o generics
    El usuario de mi API → trait (generics o dyn)
    Plugins/runtime → dyn Trait (unica opcion)

  □ ¿Necesito almacenar en struct?
    SI y el generico se propaga mucho → Box<dyn Trait>
    SI y es un solo nivel → generics o dyn
    NO → generics (preferido)

  □ ¿Necesito coleccion heterogenea?
    SI con tipos fijos → Vec<Enum>
    SI con tipos abiertos → Vec<Box<dyn Trait>>
    NO → Vec<T> con generics

  □ ¿Es hot path (>100K iteraciones)?
    SI y tipo conocido → generics (inlining)
    SI y tipo desconocido → dyn (inevitable)
    NO → lo que sea mas legible
```

---

## Errores comunes

### Error 1 — Abstraccion prematura

```rust
// Dia 1: "quizas algun dia necesite otro tipo de cache"
trait Cache { fn get(&self, key: &str) -> Option<String>; }
struct RedisCache;
impl Cache for RedisCache { ... }

// 2 anos despues: solo existe RedisCache.
// El trait no aporta nada — solo agrega indirection.
```

No crees un trait hasta que tengas **dos o mas implementaciones reales**
(o certeza de que las habra). Puedes extraer un trait despues — el
refactoring es mecanico.

### Error 2 — No migrar cuando toca

```rust
// Empezo como enum con 3 variantes. Ahora tiene 47.
// Cada nuevo feature requiere tocar 30 match blocks.
// Debio migrar a dyn Trait hace 40 variantes.
```

Reacciona cuando sientes el dolor, no cuando es insostenible.

### Error 3 — Mezclar sin razon

```rust
// Generics Y dyn en la misma funcion sin necesidad
fn process<T: Debug>(items: &[Box<dyn Processor>], logger: &T) { ... }

// Si logger siempre es StdoutLogger:
fn process(items: &[Box<dyn Processor>], logger: &StdoutLogger) { ... }

// Si items siempre son del mismo tipo:
fn process<P: Processor, L: Logger>(items: &[P], logger: &L) { ... }
```

Usa un solo mecanismo cuando es suficiente. Mezcla solo cuando cada
uno resuelve un problema distinto.

---

## Ejercicios

### Ejercicio 1 — Clasificar escenarios

Para cada escenario, elige el mecanismo y justifica en una linea:

1. Tipos de mensaje en un protocolo de red (Request, Response, Ping,
   Error) — 4 tipos fijos
2. Estrategia de compresion para un archivador (gzip, lz4, zstd, y
   el usuario puede agregar las suyas)
3. Un `min` generico que funcione con cualquier tipo comparable
4. Los handlers de un web framework (cada endpoint es un handler
   distinto con logica propia)
5. Los colores de un semaforo

<details><summary>Respuesta</summary>

| Escenario | Mecanismo | Justificacion |
|-----------|-----------|--------------|
| 1. Mensajes de protocolo | Enum | Tipos fijos por la especificacion, exhaustividad critica |
| 2. Compresion extensible | dyn Trait | El usuario debe poder agregar algoritmos propios |
| 3. min generico | Generics (`T: Ord`) | Funcion utilitaria, un tipo a la vez, zero-cost |
| 4. Web handlers | dyn Trait o closures | Tipos heterogeneos, configurados en runtime |
| 5. Colores semaforo | Enum | 3 valores fijos por la realidad fisica |

</details>

---

### Ejercicio 2 — Identificar anti-patrones

Cada diseno tiene un anti-patron. Identificalo y propone la alternativa:

```rust
// A
trait Direction { fn dx(&self) -> i32; fn dy(&self) -> i32; }
struct North; struct South; struct East; struct West;
impl Direction for North { fn dx(&self) -> i32 { 0 } fn dy(&self) -> i32 { -1 } }
// ... 3 impls mas
fn move_player(d: &dyn Direction) { ... }

// B
enum DatabaseDriver {
    Postgres, MySQL, SQLite, /* usuarios piden MongoDB, Redis, ... */
}

// C
struct App<L: Logger, C: Cache, D: Database, M: Mailer> {
    logger: L, cache: C, db: D, mailer: M,
}
```

<details><summary>Respuesta</summary>

**A: dyn Trait para 4 valores fijos → deberia ser enum.**

Solo hay 4 direcciones, nunca habra una 5ta. Un enum da match
exhaustivo y stack allocation sin Box:

```rust
enum Direction { North, South, East, West }
impl Direction {
    fn dx(&self) -> i32 { match self { East => 1, West => -1, _ => 0 } }
    fn dy(&self) -> i32 { match self { South => 1, North => -1, _ => 0 } }
}
```

**B: Enum para tipos extensibles → deberia ser trait.**

Los usuarios necesitan agregar drivers. Con enum, deben modificar tu
codigo. Con trait, implementan en su propio crate:

```rust
trait DatabaseDriver {
    fn connect(&self, url: &str) -> Result<Connection, Error>;
    fn query(&self, sql: &str) -> Result<Rows, Error>;
}
```

**C: Generics que infecta con 4 parametros → deberia usar dyn.**

`App<L, C, D, M>` tiene 4 parametros genericos. Cada funcion que
reciba App necesita `<L: Logger, C: Cache, D: Database, M: Mailer>`.
Es inmanejable:

```rust
struct App {
    logger: Box<dyn Logger>,
    cache: Box<dyn Cache>,
    db: Box<dyn Database>,
    mailer: Box<dyn Mailer>,
}
```

Menos eficiente (4 indirecciones), pero dramaticamente mas simple.
Para un web server, el costo de las indirecciones es irrelevante
comparado con IO de red.

</details>

---

### Ejercicio 3 — Disenar desde cero

Estas disenando un sistema de notificaciones. Requisitos:

- Enviar por email, SMS, Slack, y webhook
- Los usuarios de tu libreria pueden agregar canales propios
- Una notificacion puede enviarse por multiples canales
- Los canales se configuran en runtime (archivo de config)

Decide el mecanismo para cada parte y escribe los tipos (solo
signatures, sin implementacion).

<details><summary>Respuesta</summary>

```rust
// Canal de notificacion: dyn Trait
// - extensible (usuarios agregan canales)
// - configurado en runtime
// - almacenado como campo (Box)
trait NotificationChannel {
    fn send(&self, to: &str, message: &str) -> Result<(), SendError>;
    fn name(&self) -> &str;
}

// Errores: enum
// - tipos fijos, exhaustividad util
enum SendError {
    ConnectionFailed(String),
    InvalidRecipient(String),
    RateLimited { retry_after_secs: u64 },
    Other(String),
}

// Prioridad: enum
// - valores fijos, pequeno
enum Priority { Low, Normal, High, Critical }

// Notificacion: struct concreto
// - no hay polimorfismo, es un dato
struct Notification {
    to: String,
    message: String,
    priority: Priority,
}

// Dispatcher: almacena canales como dyn
struct Dispatcher {
    channels: Vec<Box<dyn NotificationChannel>>,
}

impl Dispatcher {
    fn new() -> Self {
        Dispatcher { channels: vec![] }
    }

    fn add_channel(&mut self, ch: impl NotificationChannel + 'static) {
        self.channels.push(Box::new(ch));
    }

    fn send(&self, notif: &Notification) -> Vec<Result<(), SendError>> {
        self.channels.iter()
            .map(|ch| ch.send(&notif.to, &notif.message))
            .collect()
    }
}
```

Resumen de decisiones:

| Parte | Mecanismo | Razon |
|-------|-----------|-------|
| `NotificationChannel` | dyn Trait | Extensible, runtime, campo |
| `SendError` | Enum | Tipos fijos, exhaustividad |
| `Priority` | Enum | Valores fijos |
| `Notification` | Struct concreto | No necesita polimorfismo |
| `Dispatcher.channels` | `Vec<Box<dyn>>` | Heterogeneo, runtime |

</details>

---

### Ejercicio 4 — El mismo problema en C

Traduce el sistema de notificaciones del ejercicio 3 a C, eligiendo
el mecanismo C equivalente para cada parte.

<details><summary>Respuesta</summary>

```c
// Canal: vtable manual (equivalente a dyn Trait)
typedef struct {
    int  (*send)(void *self, const char *to, const char *message);
    const char *(*name)(const void *self);
    void (*destroy)(void *self);
} ChannelVtable;

typedef struct {
    const ChannelVtable *vt;
} Channel;

// Errores: enum (equivalente al enum Rust)
typedef enum {
    SEND_OK = 0,
    SEND_ERR_CONNECTION,
    SEND_ERR_INVALID_RECIPIENT,
    SEND_ERR_RATE_LIMITED,
    SEND_ERR_OTHER,
} SendError;

// Prioridad: enum (igual)
typedef enum {
    PRIORITY_LOW,
    PRIORITY_NORMAL,
    PRIORITY_HIGH,
    PRIORITY_CRITICAL,
} Priority;

// Notificacion: struct concreto (igual)
typedef struct {
    char to[256];
    char message[1024];
    Priority priority;
} Notification;

// Dispatcher: array de punteros a Channel (equivalente a Vec<Box<dyn>>)
typedef struct {
    Channel **channels;    // array de punteros
    int count;
    int capacity;
} Dispatcher;

Dispatcher *dispatcher_create(void);
void dispatcher_add_channel(Dispatcher *d, Channel *ch);  // takes ownership
void dispatcher_send(Dispatcher *d, const Notification *notif);
void dispatcher_destroy(Dispatcher *d);

// Implementacion de un canal concreto (ej. email)
typedef struct {
    Channel base;
    char smtp_host[128];
    int smtp_port;
} EmailChannel;

EmailChannel *email_channel_create(const char *host, int port);
```

Las decisiones son identicas: vtable para extensibilidad, enum para
tipos fijos, struct concreto para datos. La diferencia: todo es
manual (casts, ownership, cleanup), mientras Rust lo automatiza.

</details>

---

### Ejercicio 5 — Migracion

Tienes este enum que empezo con 3 variantes y ahora tiene 12:

```rust
enum Transform {
    Scale(f64),
    Rotate(f64),
    Translate(f64, f64),
    Skew(f64, f64),
    Flip(Axis),
    Mirror(Axis),
    Crop { x: u32, y: u32, w: u32, h: u32 },
    Blur(f64),
    Sharpen(f64),
    Brightness(f64),
    Contrast(f64),
    Custom(Box<dyn Fn(&mut Image)>),
}
```

Hay 8 funciones con `match` sobre Transform, y cada nueva variante
requiere actualizar las 8.

Ademas, la variante `Custom` ya rompe la clausura del enum.

Propone una migracion a trait. Que se gana y que se pierde?

<details><summary>Respuesta</summary>

```rust
trait Transform {
    fn apply(&self, image: &mut Image);
    fn name(&self) -> &str;

    // Metodos por defecto opcionales:
    fn is_reversible(&self) -> bool { false }
    fn inverse(&self) -> Option<Box<dyn Transform>> { None }
}

// Cada variante se convierte en un struct:
struct Scale(f64);
impl Transform for Scale {
    fn apply(&self, image: &mut Image) { /* ... */ }
    fn name(&self) -> &str { "Scale" }
    fn is_reversible(&self) -> bool { true }
    fn inverse(&self) -> Option<Box<dyn Transform>> {
        Some(Box::new(Scale(1.0 / self.0)))
    }
}

struct Rotate(f64);
impl Transform for Rotate {
    fn apply(&self, image: &mut Image) { /* ... */ }
    fn name(&self) -> &str { "Rotate" }
}

// Custom ya no es una variante especial — es un struct normal:
struct CustomTransform {
    name: String,
    func: Box<dyn Fn(&mut Image)>,
}
impl Transform for CustomTransform {
    fn apply(&self, image: &mut Image) { (self.func)(image) }
    fn name(&self) -> &str { &self.name }
}

// Pipeline:
struct Pipeline {
    transforms: Vec<Box<dyn Transform>>,
}
```

**Se gana:**
- Agregar transformaciones sin tocar codigo existente
- `Custom` ya no es un caso especial
- Cada transformacion encapsula su propia logica (testeable)
- Usuarios pueden crear transformaciones propias

**Se pierde:**
- Exhaustividad — si agregas `is_reversible` al trait, no hay error
  de compilacion para las implementaciones que olviden verificar
- Stack allocation — ahora necesitas Box para cada transformacion
- Match centralizado — las 8 funciones con match se convierten en
  8 metodos del trait; si alguna hacia logica cruzada entre variantes,
  hay que reestructurar

</details>

---

### Ejercicio 6 — Code review

Revisa cada fragmento y da tu veredicto: correcto, aceptable, o
cambiar. Si cambiar, propone la alternativa.

```rust
// A: una libreria de geometria
pub fn area<T: Shape>(s: &T) -> f64 { s.area() }

// B: un web framework
pub struct Router {
    routes: Vec<(String, Box<dyn Fn(&Request) -> Response>)>,
}

// C: un parser de CSV
enum CsvValue { Int(i64), Float(f64), Text(String), Empty }

// D: un ORM
pub fn find_by_id<T: Model>(id: u64) -> Option<T> { todo!() }

// E: un game engine
struct World {
    entities: Vec<Box<dyn Entity>>,
}
```

<details><summary>Respuesta</summary>

| | Veredicto | Comentario |
|---|-----------|-----------|
| A | Correcto | Libreria publica + generics = maxima flexibilidad |
| B | Correcto | Routes son heterogeneas, configuradas en runtime, closures como handlers es idiomatico |
| C | Correcto | Valores CSV son un conjunto fijo (int, float, text, empty), exhaustividad util |
| D | Correcto | ORM con generics: el usuario define sus modelos, la funcion retorna el tipo concreto |
| E | Aceptable pero revisar | dyn Entity funciona pero tiene overhead por Box. Un game engine de alto rendimiento usaria ECS (Entity Component System) con datos en arrays contiguos, no objetos polimorficos individuales. Para un juego simple o prototipo, dyn Entity esta bien |

</details>

---

### Ejercicio 7 — Disenar el equivalente C

Para cada API de Rust, escribe el equivalente C y marca que se pierde:

```rust
// A
fn sort<T: Ord>(slice: &mut [T])

// B
struct EventEmitter {
    listeners: Vec<Box<dyn Fn(&Event)>>,
}

// C
enum Token { Num(f64), Op(char), Paren(bool) }
```

<details><summary>Respuesta</summary>

```c
// A: sort generico
void sort(void *base, size_t count, size_t elem_size,
          int (*cmp)(const void *, const void *));
```

Se pierde: type safety (void* acepta cualquier tipo), el tamano se
pasa manualmente, el comparador puede tener la firma incorrecta.

```c
// B: event emitter
typedef void (*Listener)(const Event *event, void *ctx);
typedef struct {
    Listener *listeners;
    void    **contexts;
    int       count, capacity;
} EventEmitter;

void emitter_add(EventEmitter *e, Listener fn, void *ctx);
void emitter_emit(EventEmitter *e, const Event *event);
```

Se pierde: las capturas se pasan manualmente como `void *ctx`,
ownership del contexto no esta claro (quien hace free?), no hay
destructor automatico.

```c
// C: tokens
typedef enum { TOK_NUM, TOK_OP, TOK_PAREN } TokenTag;
typedef struct {
    TokenTag tag;
    union {
        double num;
        char op;
        int is_open;  // bool como int
    };
} Token;
```

Se pierde: puedes leer `t.num` cuando `t.tag == TOK_OP` (UB silencioso),
el switch no verifica exhaustividad por defecto, `bool` no es un tipo
nativo en C89.

</details>

---

### Ejercicio 8 — Escenario ambiguo

Un sistema de pagos tiene 5 procesadores: Stripe, PayPal, Square,
BankTransfer, CashOnDelivery. El equipo debate:

- Dev A: "Enum — son solo 5 y no cambiaran"
- Dev B: "Trait — el proximo quarter quieren Crypto y BNPL"
- Dev C: "Generics — rendimiento es critico"

Quien tiene razon? Propone tu diseno.

<details><summary>Respuesta</summary>

**Dev B tiene razon.** Razones:

1. "No cambiaran" es casi siempre falso para procesadores de pago. El
   negocio agrega metodos de pago frecuentemente.

2. Rendimiento: un pago tarda 200-2000ms (red + procesamiento externo).
   El dispatch dinamico es ~5ns. La diferencia es irrelevante — Dev C
   optimiza lo que no importa.

3. Diferentes procesadores pueden necesitar diferentes datos (API keys,
   webhooks, configuraciones). Un trait permite que cada uno tenga su
   propio struct con sus datos.

Diseno:

```rust
trait PaymentProcessor {
    fn charge(&self, amount: Money, card: &Card) -> Result<Receipt, PayError>;
    fn refund(&self, receipt: &Receipt) -> Result<(), PayError>;
    fn name(&self) -> &str;
}

// Errores: enum (tipos fijos de error)
enum PayError {
    Declined(String),
    NetworkError(String),
    InvalidCard,
    InsufficientFunds,
}

// Almacenar en la aplicacion
struct PaymentService {
    processor: Box<dyn PaymentProcessor>,
}
```

Si Dev A tuviera razon (realmente nunca cambian), un enum seria
aceptable. Pero en software de pagos, la experiencia dice que siempre
hay un procesador nuevo.

</details>

---

### Ejercicio 9 — Encontrar la abstraccion innecesaria

Cada fragmento tiene una abstraccion que no aporta. Simplifica:

```rust
// A
trait Adder { fn add(&self, a: i32, b: i32) -> i32; }
struct SimpleAdder;
impl Adder for SimpleAdder { fn add(&self, a: i32, b: i32) -> i32 { a + b } }
fn sum(adder: &dyn Adder, x: i32, y: i32) -> i32 { adder.add(x, y) }

// B
enum Config { Production(ProdConfig) }
fn get_config() -> Config { Config::Production(ProdConfig::load()) }

// C
fn process<T: Into<String>>(name: T) {
    let name: String = name.into();
    println!("Hello, {}", name);
}
```

<details><summary>Respuesta</summary>

**A: Trait con una sola implementacion → funcion directa.**

```rust
fn sum(a: i32, b: i32) -> i32 { a + b }
```

El trait `Adder` no aporta nada: solo hay una implementacion, y la
operacion es trivial. Si algun dia necesitas multiples estrategias de
suma (improbable), puedes crear el trait entonces.

**B: Enum con una sola variante → struct directo.**

```rust
fn get_config() -> ProdConfig { ProdConfig::load() }
```

Un enum con una variante es un wrapper sin sentido. El match siempre
tiene un solo brazo. Si necesitas Dev y Prod, agrega la variante
cuando exista.

**C: Generico innecesario → tipo concreto.**

```rust
fn process(name: &str) {
    println!("Hello, {}", name);
}
```

`Into<String>` acepta `&str` y `String`, pero la funcion solo usa el
valor para imprimirlo — no necesita ownership. `&str` es suficiente
y evita la conversion + allocation de `.into()`.

</details>

---

### Ejercicio 10 — Reflexion: la regla de oro

Responde sin ver la respuesta:

1. Cual es la abstraccion correcta cuando tienes UN solo tipo concreto?

2. Resume en una frase cuando usar cada mecanismo.

3. Que es mas caro: elegir la abstraccion incorrecta, o cambiar de
   abstraccion despues?

<details><summary>Respuesta</summary>

**1. Un solo tipo concreto:**

**Ninguna abstraccion.** Usa el tipo directamente. No crees un trait,
no crees un enum de una variante, no uses generics. La mejor
abstraccion es la que no existe.

Agrega la abstraccion cuando tengas **dos implementaciones reales** o
**certeza concreta** (no hipotetica) de que las habra.

**2. Una frase por mecanismo:**

- **Enum**: "se exactamente cuales son las opciones y quiero que el
  compilador verifique que manejo todas"
- **dyn Trait**: "no se cuantos tipos habra, otros pueden agregar los
  suyos, y el tipo se decide en runtime"
- **Generics**: "funciona con cualquier tipo que cumpla las reglas, y
  quiero que el compilador optimice como si fuera codigo especifico"
- **Nada**: "solo hay un tipo — usalo directamente"

**3. Que es mas caro:**

**Elegir la abstraccion incorrecta es mas caro** en el corto plazo
(complejidad innecesaria, bugs, confusion del equipo). Pero **cambiar
de abstraccion despues es mecanico y bajo riesgo** (enum→trait,
trait→enum, concreto→trait son refactorings bien definidos).

Por eso la recomendacion es: empieza con lo mas simple (concreto o
enum), y migra cuando la necesidad sea real. "Make it work, make it
right, make it fast" — no "make it abstract from day one".

La excepcion: APIs publicas de librerias. Ahi, el costo de cambiar
despues es alto (breaking change para los usuarios). Invierte mas
tiempo en el diseno inicial.

</details>
