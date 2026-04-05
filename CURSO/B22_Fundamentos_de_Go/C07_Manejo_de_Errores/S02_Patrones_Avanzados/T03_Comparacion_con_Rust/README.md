# Comparacion con Rust — error vs Result<T,E>, Wrapping vs From Trait, Ausencia de ? Operator

## 1. Introduccion

Go y Rust resolvieron el mismo problema — **manejar errores sin excepciones** — pero llegaron a soluciones radicalmente diferentes. Go eligio simplicidad y convencion: un interfaz `error` de un solo metodo, el patron `if err != nil`, y herramientas runtime (`errors.Is`, `errors.As`). Rust eligio seguridad y expresividad: un tipo algebraico `Result<T,E>`, el operador `?` para propagacion automatica, y verificacion exhaustiva en compile-time con `match`.

Estas diferencias no son accidentales — reflejan las filosofias fundamentales de cada lenguaje:

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    FILOSOFIAS DE ERROR HANDLING                           │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  GO:                                  RUST:                              │
│  "Errors are values"                  "Make illegal states              │
│  — Rob Pike                            unrepresentable"                  │
│                                       — Yaron Minsky                     │
│                                                                          │
│  ├─ Simple is better than clever     ├─ Correct is better than simple   │
│  ├─ Un solo mecanismo (interface)    ├─ Type system enforces handling   │
│  ├─ Convencion sobre compilador      ├─ Compilador sobre convencion     │
│  ├─ Runtime checks (errors.Is/As)    ├─ Compile-time checks (match)     │
│  ├─ "Debes" verificar err            ├─ "Debes" verificar Result        │
│  │   (pero puedes ignorarlo)         │   (sino no compila)              │
│  ├─ Propagacion manual               ├─ Propagacion automatica (?)      │
│  └─ Wrapping con fmt.Errorf(%w)      └─ Conversion con From trait       │
│                                                                          │
│  RESULTADO:                           RESULTADO:                         │
│  Mas boilerplate, mas legible,        Menos boilerplate, mas seguro,    │
│  mas facil de aprender                mas dificil de aprender           │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

Este topico es el cierre del capitulo de error handling. Despues de dominar el sistema de errores de Go en los topicos anteriores (error como valor, `if err != nil`, wrapping, sentinels, errgroup, APIs), ahora lo ponemos en perspectiva con Rust para entender **por que** Go funciona como funciona, **que** se pierde y **que** se gana, y **como** un programador que conoce ambos lenguajes puede transferir patrones entre ellos.

Para SysAdmin/DevOps, esta comparacion es directamente util si:
- Evaluas herramientas escritas en Go vs Rust (Kubernetes vs Firecracker, Docker vs containerd)
- Lees codigo fuente de proyectos en ambos lenguajes para debugging
- Escribes CLIs o servicios y eliges entre Go y Rust
- Contribuyes a proyectos open source que usan uno u otro

---

## 2. El Tipo de Error: Interface vs Enum

### 2.1 Go: error como Interface

```go
// La interfaz error es trivial — un solo metodo:
type error interface {
    Error() string
}

// CUALQUIER tipo que tenga Error() string ES un error.
// No necesitas declararlo, no necesitas "implementar" nada explicitamente.
// Es implicito — el compilador lo verifica en cada punto de uso.

// Ejemplos de tipos que son error:

// 1. El mas basico: errors.New retorna *errorString
err := errors.New("connection refused")

// 2. fmt.Errorf retorna *fmt.wrapError (con Unwrap si usas %w)
err := fmt.Errorf("connect to %s: %w", host, ErrRefused)

// 3. Custom struct con campos
type ConnError struct {
    Host    string
    Port    int
    Cause   error
}
func (e *ConnError) Error() string {
    return fmt.Sprintf("connect %s:%d: %v", e.Host, e.Port, e.Cause)
}
func (e *ConnError) Unwrap() error { return e.Cause }

// 4. String type como const error
type Error string
func (e Error) Error() string { return string(e) }
const ErrTimeout Error = "timeout"
```

**Propiedades del sistema Go:**
- **Abierto**: cualquiera puede crear un nuevo tipo de error sin modificar nada existente
- **Runtime-checked**: `errors.Is` y `errors.As` son verificaciones en tiempo de ejecucion
- **No exhaustivo**: el compilador no verifica que manejes todos los errores posibles
- **Nullable**: `error` puede ser `nil` (lo que indica exito)

### 2.2 Rust: Result<T,E> como Enum

```rust
// Result es un enum con dos variantes — definido en la stdlib:
enum Result<T, E> {
    Ok(T),   // exito: contiene el valor
    Err(E),  // error: contiene el error
}

// Option es su companero — para "valor o nada" (equivalente a nil en Go):
enum Option<T> {
    Some(T), // hay un valor
    None,    // no hay valor (≈ nil)
}

// Un error custom en Rust es un enum:
#[derive(Debug)]
enum ConnError {
    Refused { host: String, port: u16 },
    Timeout { host: String, duration: Duration },
    DnsFailure { domain: String },
    Tls(native_tls::Error),
}

// Implementar Display (equivalente a Error() string en Go):
impl fmt::Display for ConnError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ConnError::Refused { host, port } =>
                write!(f, "connection refused to {}:{}", host, port),
            ConnError::Timeout { host, duration } =>
                write!(f, "connection to {} timed out after {:?}", host, duration),
            ConnError::DnsFailure { domain } =>
                write!(f, "DNS lookup failed for {}", domain),
            ConnError::Tls(e) =>
                write!(f, "TLS error: {}", e),
        }
    }
}

// Implementar std::error::Error (equivalente a la interfaz error):
impl std::error::Error for ConnError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            ConnError::Tls(e) => Some(e), // tiene causa (≈ Unwrap en Go)
            _ => None,
        }
    }
}
```

**Propiedades del sistema Rust:**
- **Cerrado**: un enum define TODAS las variantes posibles (no puedes agregar variantes desde fuera)
- **Compile-time checked**: `match` OBLIGA a manejar todas las variantes (exhaustivo)
- **No nullable**: `Result` siempre es `Ok` o `Err`, nunca "nada"
- **Tipo obligatorio**: si una funcion retorna `Result`, el caller DEBE manejarlo (sino no compila)

### 2.3 Comparacion Visual

```
┌──────────────────────────────────────────────────────────────────────────┐
│               TIPO DE ERROR: Go interface vs Rust enum                   │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  GO:                                                                     │
│  ┌──────────────────┐                                                    │
│  │  interface error  │   Cualquier tipo con Error() string              │
│  │  {               │                                                    │
│  │   Error() string  │   Abierto: nuevos tipos en cualquier paquete     │
│  │  }               │   Identidad: errors.Is/As en runtime             │
│  └──────────────────┘   Nullable: nil = exito                           │
│         ↑  ↑  ↑         No exhaustivo: switch sin default compila       │
│         │  │  │                                                          │
│    errorString  PathError  ConnError  (infinitos tipos posibles)         │
│                                                                          │
│                                                                          │
│  RUST:                                                                   │
│  ┌──────────────────┐                                                    │
│  │  enum ConnError   │   Variantes fijas en la definicion               │
│  │  {               │                                                    │
│  │   Refused{...}    │   Cerrado: solo estas variantes existen          │
│  │   Timeout{...}    │   Identidad: match en compile-time               │
│  │   DnsFailure{...} │   No nullable: siempre Ok o Err                  │
│  │   Tls(Error)      │   Exhaustivo: match sin variante = no compila    │
│  │  }               │                                                    │
│  └──────────────────┘                                                    │
│                                                                          │
│  CONSECUENCIA PRACTICA:                                                  │
│  Go: if errors.Is(err, ErrNotFound)  ← verificas lo que QUIERES        │
│       lo demas pasa al default       ← puede haber errores no manejados │
│                                                                          │
│  Rust: match err {                   ← verificas TODOS                  │
│            Refused{..} => ...                                            │
│            Timeout{..} => ...                                            │
│            DnsFailure{..} => ...                                         │
│            Tls(e) => ...                                                 │
│        }                             ← si olvidas uno, no compila       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Propagacion de Errores: if err != nil vs ?

### 3.1 Go: Propagacion Manual

Cada error se propaga manualmente con `if err != nil { return ..., err }`:

```go
func deployService(ctx context.Context, name string, image string) (*Deployment, error) {
    // Paso 1: validar imagen
    if err := validateImage(image); err != nil {
        return nil, fmt.Errorf("validate image: %w", err)
    }

    // Paso 2: pull de la imagen
    digest, err := pullImage(ctx, image)
    if err != nil {
        return nil, fmt.Errorf("pull image %s: %w", image, err)
    }

    // Paso 3: crear pod
    pod, err := createPod(ctx, name, digest)
    if err != nil {
        return nil, fmt.Errorf("create pod %s: %w", name, err)
    }

    // Paso 4: esperar ready
    if err := waitReady(ctx, pod.ID, 60*time.Second); err != nil {
        return nil, fmt.Errorf("wait ready %s: %w", pod.ID, err)
    }

    // Paso 5: registrar en service discovery
    endpoint, err := register(ctx, name, pod.IP, pod.Port)
    if err != nil {
        return nil, fmt.Errorf("register %s: %w", name, err)
    }

    return &Deployment{
        Name:     name,
        Pod:      pod,
        Endpoint: endpoint,
    }, nil
}
// Total: 5 operaciones, 5 bloques if err != nil, 10 lineas de boilerplate
```

### 3.2 Rust: Operador ? (Propagacion Automatica)

```rust
fn deploy_service(ctx: &Context, name: &str, image: &str) -> Result<Deployment, DeployError> {
    // Paso 1-5: ? propaga automaticamente si hay error
    let _ = validate_image(image)?;
    let digest = pull_image(ctx, image)?;
    let pod = create_pod(ctx, name, &digest)?;
    wait_ready(ctx, &pod.id, Duration::from_secs(60))?;
    let endpoint = register(ctx, name, &pod.ip, pod.port)?;

    Ok(Deployment {
        name: name.to_string(),
        pod,
        endpoint,
    })
}
// Total: 5 operaciones, 0 bloques if/match, 0 lineas de boilerplate
```

### 3.3 Como Funciona ? Internamente

```rust
// El operador ? es azucar sintactica para:
let digest = pull_image(ctx, image)?;

// Se expande a:
let digest = match pull_image(ctx, image) {
    Ok(val) => val,
    Err(e) => return Err(e.into()), // .into() convierte el tipo de error
};

// La clave es .into() — usa el trait From para CONVERTIR tipos de error.
// Si pull_image retorna Result<String, IoError>
// y deploy_service retorna Result<Deployment, DeployError>
// entonces necesitas: impl From<IoError> for DeployError

// Esto es AUTOMATICO con thiserror:
#[derive(Error, Debug)]
enum DeployError {
    #[error("validate image: {0}")]
    Validation(#[from] ValidationError),  // genera impl From<ValidationError>

    #[error("pull image: {0}")]
    Pull(#[from] PullError),              // genera impl From<PullError>

    #[error("create pod: {0}")]
    Pod(#[from] PodError),                // genera impl From<PodError>

    #[error("register: {0}")]
    Register(#[from] RegisterError),      // genera impl From<RegisterError>

    #[error("timeout: {0}")]
    Timeout(#[from] TimeoutError),
}

// Ahora ? sabe como convertir cada sub-error a DeployError automaticamente
```

### 3.4 El Equivalente Go del Operador ?

Go considero y **rechazo** un operador similar. La propuesta `try()` de 2019 fue retirada:

```go
// PROPUESTA RECHAZADA (2019):
func deployService(ctx context.Context, name, image string) (*Deployment, error) {
    try(validateImage(image))
    digest := try(pullImage(ctx, image))
    pod := try(createPod(ctx, name, digest))
    try(waitReady(ctx, pod.ID, 60*time.Second))
    endpoint := try(register(ctx, name, pod.IP, pod.Port))

    return &Deployment{Name: name, Pod: pod, Endpoint: endpoint}, nil
}

// RAZONES DEL RECHAZO:
// 1. "Oculta el flujo de control" — no es obvio que try() puede return
// 2. "Desalienta anadir contexto" — con try() no wrappeas el error
//    (en Rust, ? si puede transformar via From trait)
// 3. "La comunidad no la quiso" — encuesta con fuerte oposicion
// 4. "if err != nil es parte de la identidad de Go"
```

### 3.5 Comparacion Visual de Propagacion

```
┌──────────────────────────────────────────────────────────────────────────┐
│              PROPAGACION: Go manual vs Rust ?                            │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Go — 5 operaciones:                 Rust — 5 operaciones:              │
│  ────────────────────                ─────────────────────               │
│                                                                          │
│  err := validateImage(image)         validate_image(image)?;            │
│  if err != nil {                                                         │
│      return nil, fmt.Errorf(...)                                         │
│  }                                                                       │
│                                                                          │
│  digest, err := pullImage(ctx, img)  let d = pull_image(ctx, img)?;     │
│  if err != nil {                                                         │
│      return nil, fmt.Errorf(...)                                         │
│  }                                                                       │
│                                                                          │
│  pod, err := createPod(ctx, n, d)    let pod = create_pod(ctx, n, &d)?; │
│  if err != nil {                                                         │
│      return nil, fmt.Errorf(...)                                         │
│  }                                                                       │
│                                                                          │
│  err = waitReady(ctx, pod.ID, 60s)   wait_ready(ctx, &pod.id, 60s)?;   │
│  if err != nil {                                                         │
│      return nil, fmt.Errorf(...)                                         │
│  }                                                                       │
│                                                                          │
│  ep, err := register(ctx, n, ip, p)  let ep = register(ctx,n,&ip,p)?;  │
│  if err != nil {                                                         │
│      return nil, fmt.Errorf(...)                                         │
│  }                                                                       │
│                                                                          │
│  LINEAS DE LOGICA:      5            LINEAS DE LOGICA:    5             │
│  LINEAS DE ERROR:      15            LINEAS DE ERROR:     0             │
│  TOTAL:                20            TOTAL:               5             │
│  RATIO ERROR/LOGICA:  3:1            RATIO ERROR/LOGICA: 0:1            │
│                                                                          │
│  PERO: Go tiene contexto en cada     PERO: Rust pierde contexto         │
│  propagacion (fmt.Errorf)            sin .map_err() o #[from]           │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Wrapping: fmt.Errorf(%w) vs From Trait

### 4.1 Go: Wrapping con %w

Go wrappea errores creando una **cadena lineal** (o arbol desde Go 1.20+):

```go
// Wrapping basico — encadena errores con contexto textual
func readConfig(path string) (*Config, error) {
    data, err := os.ReadFile(path)
    if err != nil {
        return nil, fmt.Errorf("read config %s: %w", path, err)
        // Crea: &fmt.wrapError{msg: "read config /etc/app.yaml: ...", err: originalErr}
    }

    var cfg Config
    if err := json.Unmarshal(data, &cfg); err != nil {
        return nil, fmt.Errorf("parse config %s: %w", path, err)
    }

    return &cfg, nil
}

// El caller desenvuelve la cadena:
cfg, err := readConfig("/etc/app.yaml")
if err != nil {
    if errors.Is(err, os.ErrNotExist) {
        // readConfig wrappeo os.ErrNotExist → errors.Is recorre la cadena
        log.Println("config file not found, using defaults")
        return defaultConfig(), nil
    }
    return nil, err
}

// La cadena de wrapping se ve asi:
// "read config /etc/app.yaml: open /etc/app.yaml: no such file or directory"
//  └── wrapError ───────────────└── *os.PathError ────────└── os.ErrNotExist

// Inspeccion:
var pathErr *os.PathError
if errors.As(err, &pathErr) {
    fmt.Println("Operation:", pathErr.Op)   // "open"
    fmt.Println("Path:", pathErr.Path)       // "/etc/app.yaml"
}
```

**Caracteristicas del wrapping en Go:**
- **Textual**: cada capa anade un string de contexto
- **Runtime**: `errors.Is` y `errors.As` recorren la cadena en runtime
- **Manual**: decides en cada punto QUE wrappear y CON QUE contexto
- **Lineal**: hasta Go 1.13. Arbol desde Go 1.20+ con multiples `%w`

### 4.2 Rust: Conversion con From Trait

Rust no "wrappea" en el sentido de Go. En su lugar, **convierte** un tipo de error en otro usando el trait `From`:

```rust
use std::fs;
use std::io;

// Sin librerias externas — implementacion manual de From:
#[derive(Debug)]
enum ConfigError {
    Io { path: String, source: io::Error },
    Parse { path: String, source: serde_json::Error },
}

impl fmt::Display for ConfigError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ConfigError::Io { path, source } =>
                write!(f, "read config {}: {}", path, source),
            ConfigError::Parse { path, source } =>
                write!(f, "parse config {}: {}", path, source),
        }
    }
}

impl std::error::Error for ConfigError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            ConfigError::Io { source, .. } => Some(source),
            ConfigError::Parse { source, .. } => Some(source),
        }
    }
}

fn read_config(path: &str) -> Result<Config, ConfigError> {
    let data = fs::read_to_string(path)
        .map_err(|e| ConfigError::Io {
            path: path.to_string(),
            source: e,
        })?;

    let cfg: Config = serde_json::from_str(&data)
        .map_err(|e| ConfigError::Parse {
            path: path.to_string(),
            source: e,
        })?;

    Ok(cfg)
}

// Con thiserror (la forma idiomatica):
#[derive(Error, Debug)]
enum ConfigError {
    #[error("read config {path}")]
    Io {
        path: String,
        #[source]
        source: io::Error,
    },

    #[error("parse config {path}")]
    Parse {
        path: String,
        #[source]
        source: serde_json::Error,
    },
}

// El caller usa match (exhaustivo):
match read_config("/etc/app.yaml") {
    Ok(cfg) => use_config(cfg),
    Err(ConfigError::Io { path, source }) => {
        if source.kind() == io::ErrorKind::NotFound {
            println!("Config {} not found, using defaults", path);
            use_config(Config::default())
        } else {
            return Err(ConfigError::Io { path, source }.into());
        }
    }
    Err(ConfigError::Parse { path, source }) => {
        eprintln!("Malformed config {}: {}", path, source);
        return Err(ConfigError::Parse { path, source }.into());
    }
}
```

### 4.3 From Trait — El Mecanismo de Conversion

```rust
// From es un trait estandar de Rust para conversiones:
trait From<T> {
    fn from(value: T) -> Self;
}

// Cuando implementas From<io::Error> for AppError,
// el operador ? puede convertir automaticamente:
impl From<io::Error> for AppError {
    fn from(err: io::Error) -> AppError {
        AppError::Io(err) // el error de io se convierte en variante Io
    }
}

// Ahora ? funciona:
fn read_file() -> Result<String, AppError> {
    let data = fs::read_to_string("file.txt")?;
    //                                         ↑
    // fs::read_to_string retorna Result<String, io::Error>
    // ? ve que necesita Result<String, AppError>
    // ? llama AppError::from(io::Error) automaticamente
    Ok(data)
}

// thiserror genera los impl From automaticamente con #[from]:
#[derive(Error, Debug)]
enum AppError {
    #[error("io error")]
    Io(#[from] io::Error),        // genera impl From<io::Error> for AppError

    #[error("json error")]
    Json(#[from] serde_json::Error), // genera impl From<serde_json::Error>
}
```

### 4.4 Comparacion de Wrapping

```
┌──────────────────────────────────────────────────────────────────────────┐
│            WRAPPING: Go %w vs Rust From trait                            │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  CONCEPTO         GO                      RUST                           │
│  ──────────       ─────────────           ─────────────                  │
│  Mecanismo        fmt.Errorf("%w", err)   impl From<E> / #[from]        │
│  Estructura       Cadena lineal (lista)   Enum (arbol tipado)           │
│  Contexto         String ("read config:") Variante con campos           │
│  Acceso a causa   errors.Is / errors.As   source() / downcast_ref()     │
│  Automatizable    No (manual cada vez)    Si (? + From)                  │
│  Perdida info     No (cadena preserva)    Posible (From puede descartar)│
│  Verificacion     Runtime                 Compile-time (match)          │
│  Flexibilidad     Alta (cualquier texto)  Media (variantes fijas)       │
│                                                                          │
│  GO:                                                                     │
│  "read config: open /etc/app.yaml: no such file or directory"           │
│   wrapError ────→ PathError ────→ ErrNotExist                           │
│   (lista enlazada de errores con Unwrap)                                │
│                                                                          │
│  RUST:                                                                   │
│  ConfigError::Io { path: "/etc/app.yaml", source: io::Error }           │
│   (enum con datos tipados + source encadenado)                          │
│                                                                          │
│  TRADEOFF:                                                               │
│  Go: Mas facil wrappear. Mas facil perder info (texto opaco).           │
│  Rust: Mas dificil wrappear (tipos). Imposible perder variantes.        │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Obligatoriedad: Puedes Ignorar vs El Compilador Te Obliga

### 5.1 Go: Errores Ignorables

```go
// En Go, PUEDES ignorar un error. El compilador no te detiene.

// 1. Ignorar silenciosamente (antipatron pero compila):
data, _ := os.ReadFile("config.yaml") // _ descarta el error
json.Unmarshal(data, &cfg)            // descarta el error completamente

// 2. Ignorar el retorno completo:
os.Remove("tempfile") // retorna error, pero no lo capturamos
// El compilador no emite warning ni error

// 3. Solo verificar un error de multiples:
file, err := os.Open("data.txt")
if err != nil {
    return err
}
// ...usar file...
file.Close() // ← retorna error, ignorado por convencion
// (en este caso es aceptable si ya leiste todo)

// HERRAMIENTAS que detectan errores ignorados:
// - errcheck: go install github.com/kisielk/errcheck@latest
//   errcheck ./...
// - golangci-lint con errcheck habilitado
// - go vet (no detecta todos los casos)
```

### 5.2 Rust: El Compilador Obliga

```rust
// En Rust, ignorar un Result produce un WARNING del compilador:
fn main() {
    std::fs::read_to_string("config.yaml");
    // WARNING: unused `Result` that must be used
    // this `Result` may be an `Err` variant, which should be handled

    // Para ignorar EXPLICITAMENTE, debes usar:
    let _ = std::fs::read_to_string("config.yaml");
    //  ↑ let _ descarta el valor — EXPLICITO, no accidental

    // O con drop():
    drop(std::fs::read_to_string("config.yaml"));

    // Pero si intentas USAR el valor sin manejar el error:
    let data = std::fs::read_to_string("config.yaml");
    // data es Result<String, io::Error>, NO String
    // No puedes usarlo como String directamente:
    // println!("{}", data); // ERROR: Result no implementa Display asi
    // data.len();           // ERROR: Result no tiene .len()

    // DEBES extraer el valor:
    let data = std::fs::read_to_string("config.yaml").unwrap();
    //                                                 ^^^^^^^^
    // .unwrap() hace panic si es Err — explicito que estas eligiendo crash

    // O manejar el error:
    let data = match std::fs::read_to_string("config.yaml") {
        Ok(content) => content,
        Err(e) => {
            eprintln!("Error: {}", e);
            return;
        }
    };
}

// El compilador OBLIGA a:
// 1. Manejar el Result (match, if let, ?)
// 2. O descartarlo EXPLICITAMENTE (let _ =)
// 3. O crash explicitamente (.unwrap(), .expect("msg"))
// No puedes ignorarlo accidentalmente
```

### 5.3 Comparacion de Obligatoriedad

```
┌──────────────────────────────────────────────────────────────────────────┐
│                OBLIGATORIEDAD DE MANEJO DE ERRORES                       │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ESCENARIO                    GO                  RUST                   │
│  ───────────                  ────                ──────                  │
│  Ignorar error               Compila (silencio)  Warning (compilador)   │
│  Ignorar valor + error       Compila              Warning               │
│  Usar valor sin check        Compila (nil/zero)  No compila (Result)    │
│  Olvidar una variante        Compila (default)   No compila (match)     │
│  Error nuevo en dependencia  Sin detectar         Compile error (match) │
│                                                                          │
│  GO — El peligro silencioso:                                            │
│  user, _ := getUser(id)      // compila, user es nil                    │
│  fmt.Println(user.Name)      // PANIC: nil pointer dereference          │
│  // El compilador no te aviso                                            │
│                                                                          │
│  RUST — El compilador te obliga:                                        │
│  let user = get_user(id);    // user es Result<User, Error>             │
│  println!("{}", user.name);  // ERROR: Result no tiene campo .name     │
│  // Debes: let user = get_user(id)?; o match                           │
│                                                                          │
│  ¿QUE ES MEJOR?                                                        │
│  Go: mas rapido escribir, mas facil olvidar → bugs en runtime           │
│  Rust: mas lento escribir, imposible olvidar → bugs en compile-time    │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 6. Exhaustividad: Switch vs Match

### 6.1 Go: Switch No Exhaustivo

```go
type ErrorKind int

const (
    ErrNotFound ErrorKind = iota
    ErrForbidden
    ErrConflict
    ErrTimeout
    ErrInternal
)

func handleError(kind ErrorKind) int {
    switch kind {
    case ErrNotFound:
        return 404
    case ErrForbidden:
        return 403
    case ErrConflict:
        return 409
    // ⚠️ FALTA ErrTimeout y ErrInternal — COMPILA SIN ERROR
    // Si llega ErrTimeout, el switch cae al final y retorna 0
    default:
        return 500 // default catch-all — necesario en Go
    }
}

// Si alguien agrega ErrRateLimited al enum:
// const ErrRateLimited ErrorKind = 5
// NINGUN switch existente dara error de compilacion
// Todos caeran al default silenciosamente
// → Bug que solo se descubre en produccion
```

### 6.2 Rust: Match Exhaustivo

```rust
enum ErrorKind {
    NotFound,
    Forbidden,
    Conflict,
    Timeout,
    Internal,
}

fn handle_error(kind: ErrorKind) -> u16 {
    match kind {
        ErrorKind::NotFound => 404,
        ErrorKind::Forbidden => 403,
        ErrorKind::Conflict => 409,
        // ⛔ COMPILE ERROR: non-exhaustive patterns
        //    `ErrorKind::Timeout` and `ErrorKind::Internal` not covered
    }
}

// ✅ Correcto — todas las variantes cubiertas:
fn handle_error(kind: ErrorKind) -> u16 {
    match kind {
        ErrorKind::NotFound => 404,
        ErrorKind::Forbidden => 403,
        ErrorKind::Conflict => 409,
        ErrorKind::Timeout => 504,
        ErrorKind::Internal => 500,
    }
}

// Si alguien agrega RateLimited al enum:
// enum ErrorKind { ..., RateLimited }
// TODOS los match existentes daran error de compilacion:
// "non-exhaustive patterns: `ErrorKind::RateLimited` not covered"
// → Bug detectado en compile-time, ANTES de llegar a produccion
```

### 6.3 Go: Simular Exhaustividad

Go no tiene exhaustividad built-in, pero hay herramientas:

```go
// exhaustive linter: https://github.com/nishanths/exhaustive
// go install github.com/nishanths/exhaustive/cmd/exhaustive@latest

//go:generate exhaustive -default-signifies-exhaustive ./...

// Con golangci-lint:
// .golangci.yml:
// linters:
//   enable:
//     - exhaustive
// linters-settings:
//   exhaustive:
//     default-signifies-exhaustive: true

// Ejemplo que el linter detectaria:
type Status int
const (
    Active Status = iota
    Inactive
    Suspended
)

func statusString(s Status) string {
    switch s {
    case Active:
        return "active"
    case Inactive:
        return "inactive"
    // exhaustive linter: "missing case Suspended"
    }
    return "unknown"
}
```

---

## 7. Multiples Valores de Retorno vs Result<T,E>

### 7.1 Go: (T, error) como Convencion

```go
// Go retorna multiples valores — el error es el ULTIMO:
func findUser(id string) (*User, error) {
    // ...
}

// Problema 1: puedes usar el valor incluso con error
user, err := findUser("123")
// ⚠️ Nada te impide usar user ANTES de verificar err:
fmt.Println(user.Name) // si err != nil, user puede ser nil → PANIC

// Problema 2: la convencion no es enforced
// Nada impide retornar (valor, error) ambos != nil:
func weird() (int, error) {
    return 42, errors.New("oops")
    // ¿El caller debe usar 42 o no? No hay regla del compilador
}

// Problema 3: olvidas retornar el zero value
func findUser(id string) (*User, error) {
    user, err := db.Get(id)
    if err != nil {
        return user, err // ← user puede tener datos parciales
        // Deberia ser: return nil, err
    }
    return user, nil
}
```

### 7.2 Rust: Result<T,E> como Tipo

```rust
// Result es un tipo algebraico — es Ok(T) O Err(E), nunca ambos:
fn find_user(id: &str) -> Result<User, FindError> {
    // ...
}

// No puedes acceder al User sin extraerlo del Result:
let result = find_user("123");
// result.name;  // ⛔ ERROR: Result no tiene campo .name

// Debes extraer el valor:
let user = find_user("123")?;  // propaga si error
user.name; // ✅ ahora si — user es User, no Result<User>

// Es IMPOSIBLE retornar valor y error simultaneamente:
fn find_user(id: &str) -> Result<User, FindError> {
    Ok(user)          // O esto
    // O
    Err(FindError::NotFound)  // O esto
    // Nunca ambos
}

// Option<T> para "puede que no haya valor" (distinto de error):
fn find_user_optional(id: &str) -> Result<Option<User>, DbError> {
    let user = db.get(id)?;  // propagar error de DB
    Ok(user)                  // Option<User>: Some(user) o None
}

// El caller:
match find_user_optional("123")? {
    Some(user) => println!("Found: {}", user.name),
    None => println!("User not found"),  // no es error, solo ausencia
}
```

### 7.3 Comparacion Visual

```
┌──────────────────────────────────────────────────────────────────────────┐
│         RETORNO DE ERRORES: Go (T, error) vs Rust Result<T,E>           │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  GO:  func f() (User, error)          RUST: fn f() -> Result<User, E>  │
│                                                                          │
│  Valor y error son INDEPENDIENTES:     Valor y error son EXCLUYENTES:  │
│  ┌──────┬───────┐                      ┌──────────────────┐            │
│  │ User │ error │  ← dos campos        │ Ok(User) | Err(E)│ ← uno     │
│  └──────┴───────┘                      └──────────────────┘            │
│                                                                          │
│  Posible:                              Posible:                          │
│  (user, nil)   ← exito                 Ok(user)    ← exito             │
│  (nil, err)    ← error                 Err(e)      ← error             │
│  (user, err)   ← ¿¿ambos?? ⚠️         (imposible ambos)               │
│  (nil, nil)    ← ¿¿nada?? ⚠️          (imposible ninguno)              │
│                                                                          │
│  Acceso al valor:                      Acceso al valor:                 │
│  user, err := f()                      let user = f()?;                 │
│  user.Name ← COMPILA aunque err!=nil  user.name ← solo si f() fue Ok  │
│             (nil pointer → PANIC)                  (sino ? ya retorno) │
│                                                                          │
│  ¿Ausencia sin error?                  ¿Ausencia sin error?            │
│  Go no tiene patron estandar:          Result<Option<T>, E>:            │
│  - Retornar (nil, nil) → ambiguo       Ok(Some(user)) = encontrado     │
│  - Retornar sentinel ErrNotFound       Ok(None) = no encontrado        │
│  - sql.ErrNoRows (controversia)        Err(e) = error de DB            │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 8. Librerias de Error: Standard vs Ecosystem

### 8.1 Go: Stdlib Minimalista + x/ Packages

```go
// STDLIB (errors package):
errors.New("message")                    // crear error basico
fmt.Errorf("context: %w", err)           // wrap con contexto
errors.Is(err, target)                   // comparar por identidad
errors.As(err, &target)                  // extraer tipo
errors.Unwrap(err)                       // desenvolver una capa
errors.Join(err1, err2)                  // multi-error (Go 1.20+)

// Eso es TODO. No hay mas en la stdlib para errores.
// Todo lo demas es convencion y patrones.

// ECOSYSTEM:
// - pkg/errors (historico, pre-Go 1.13): Wrap, Cause, WithStack
//   import "github.com/pkg/errors"
//   DEPRECADO — usar stdlib desde Go 1.13+

// - hashicorp/go-multierror: multi-error pre-Go 1.20
//   import "github.com/hashicorp/go-multierror"
//   Menos necesario desde errors.Join (Go 1.20+)

// - cockroachdb/errors: errores con stack traces, encode/decode,
//   migraciones, errores de red
//   import "github.com/cockroachdb/errors"

// - uber-go/multierr: otra implementacion de multi-error
//   import "go.uber.org/multierr"
```

### 8.2 Rust: thiserror + anyhow (El Duo Estandar de Facto)

```rust
// ─── thiserror: para LIBRERIAS (errores tipados) ───

// Genera automaticamente: Display, Error, From
use thiserror::Error;

#[derive(Error, Debug)]
enum DataError {
    #[error("file not found: {path}")]
    NotFound {
        path: String,
        #[source]
        source: io::Error,
    },

    #[error("parse error at line {line}")]
    Parse {
        line: usize,
        #[source]
        source: serde_json::Error,
    },

    #[error("permission denied: {0}")]
    Forbidden(String),

    #[error(transparent)]  // delegado al error interno
    Other(#[from] anyhow::Error),
}

// thiserror genera:
// - impl Display for DataError (con el #[error("...")])
// - impl Error for DataError (con source() de #[source])
// - impl From<io::Error> for DataError (si usas #[from])
// Total: ~15 lineas vs ~60 lineas manual


// ─── anyhow: para APLICACIONES (errores opacos) ───

// anyhow::Error es como error de Go: opaco, con contexto, encadenable
use anyhow::{Context, Result, bail, ensure};

fn deploy(name: &str) -> Result<()> {  // Result<()> = anyhow::Result<()>
    let config = read_config(name)
        .context("loading deploy config")?;  // ← agrega contexto (como %w)

    let image = pull_image(&config.image)
        .with_context(|| format!("pulling image {}", config.image))?;

    ensure!(image.size < 1_000_000_000, "image too large: {} bytes", image.size);
    //      ↑ como assert pero retorna Err en lugar de panic

    if config.replicas == 0 {
        bail!("replicas must be > 0");  // ← return Err(anyhow!("..."))
    }

    Ok(())
}

// anyhow::Error:
// - Tiene backtrace automatico (si RUST_BACKTRACE=1)
// - Tiene chain de contexto (como errors.Is con cadena de wrapping)
// - Es opaco: el caller no puede match en variantes especificas
//   (por eso es para aplicaciones, no librerias)
// - Puede downcast a tipos concretos:
//   if let Some(io_err) = err.downcast_ref::<io::Error>() { ... }
//   (equivalente a errors.As en Go)
```

### 8.3 Cuando Usar Cada Uno

```
┌──────────────────────────────────────────────────────────────────────────┐
│              LIBRERIAS DE ERROR: Go vs Rust                              │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  GO:                                RUST:                                │
│  ─────                              ──────                               │
│  stdlib errors                      thiserror + anyhow                   │
│  (minimalista, suficiente)          (ecosystem, de facto estandar)       │
│                                                                          │
│  LIBRERIA (API publica):            LIBRERIA (API publica):             │
│  - Custom types + sentinels        - thiserror: enum con derive         │
│  - errors.Is / errors.As           - match exhaustivo                   │
│  - Todo manual                     - #[from] para ? automatico          │
│                                                                          │
│  APLICACION (main/handlers):        APLICACION (main/handlers):         │
│  - fmt.Errorf("%w", err)           - anyhow: contexto opaco            │
│  - Wrapping con contexto           - .context("msg") / bail!()         │
│  - Log en el punto de fallo        - Backtrace automatico              │
│                                                                          │
│  EQUIVALENCIAS:                                                         │
│  errors.New("msg")          ←→     anyhow!("msg")                      │
│  fmt.Errorf("ctx: %w", e)  ←→     e.context("ctx")                    │
│  errors.Is(e, target)       ←→     e.downcast_ref::<T>()               │
│  errors.As(e, &target)      ←→     e.downcast_ref::<T>()               │
│  errors.Join(e1, e2)        ←→     (no hay equivalente directo)        │
│  sentinel var                ←→     enum variant                        │
│  custom struct               ←→     enum variant con campos            │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 9. Panic/Recover vs panic!/catch_unwind

### 9.1 Go: panic + recover

```go
// panic es para situaciones IRRECUPERABLES:
func mustParseURL(raw string) *url.URL {
    u, err := url.Parse(raw)
    if err != nil {
        panic(fmt.Sprintf("invalid URL %q: %v", raw, err))
        // panic desenrolla el stack, ejecuta defers, y termina el programa
    }
    return u
}

// recover puede atrapar un panic DENTRO de un defer:
func safeCall(f func()) (err error) {
    defer func() {
        if r := recover(); r != nil {
            err = fmt.Errorf("panic recovered: %v", r)
        }
    }()
    f()
    return nil
}

// En Go, panic/recover se usa MUY POCO:
// - Inicializacion (must functions): panic si la configuracion es invalida
// - Bugs de programacion: index out of range, nil pointer
// - net/http: el servidor recover panics en handlers para no morir
// - NUNCA como control de flujo (eso es errors)
```

### 9.2 Rust: panic! + catch_unwind

```rust
// panic! es para situaciones IRRECUPERABLES:
fn must_parse_url(raw: &str) -> Url {
    Url::parse(raw).expect(&format!("invalid URL: {}", raw))
    // .expect() hace panic con el mensaje dado si es Err
    // .unwrap() hace panic sin mensaje custom
}

// catch_unwind puede atrapar un panic (similar a recover):
use std::panic;

fn safe_call(f: impl FnOnce() + panic::UnwindSafe) -> Result<(), String> {
    match panic::catch_unwind(f) {
        Ok(()) => Ok(()),
        Err(e) => {
            let msg = if let Some(s) = e.downcast_ref::<&str>() {
                s.to_string()
            } else if let Some(s) = e.downcast_ref::<String>() {
                s.clone()
            } else {
                "unknown panic".to_string()
            };
            Err(msg)
        }
    }
}

// PERO: en Rust, catch_unwind es MUCHO menos comun que recover en Go
// porque:
// 1. Result<T,E> maneja todos los errores recuperables
// 2. panic es solo para bugs (no para errores esperados)
// 3. #[cfg(panic = "abort")] puede desactivar unwind → catch_unwind no funciona
// 4. UnwindSafe trait restringe que se puede capturar
```

### 9.3 Comparacion

```
┌──────────────────────────────────────────────────────────────────────────┐
│             PANIC/RECOVER vs panic!/catch_unwind                         │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│                          GO                    RUST                      │
│  Mecanismo               panic + recover       panic! + catch_unwind    │
│  Tipo de panic value     any (interface{})     Box<dyn Any>             │
│  Captura                 recover() en defer    catch_unwind(closure)    │
│  Frecuencia de uso       Moderada (servers)    Rara (casi nunca)        │
│  HTTP server recovery    Comun (middleware)     Rara (tower::catch_panic)│
│  Puede desactivarse      No                    Si (panic="abort")       │
│  Stack unwind            Siempre               Configurable             │
│  Convencion              "Dont panic"           "Only bugs should panic"│
│                                                                          │
│  USO LEGITIMO:                                                           │
│  Go: must functions, server recovery, test assertions                   │
│  Rust: .unwrap() en prototipos, .expect() con mensajes claros           │
│                                                                          │
│  REGLA DE ORO (AMBOS):                                                   │
│  Si el caller puede manejar la situacion → error/Result                 │
│  Si el programa NO puede continuar → panic                              │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 10. Patron Completo Side-by-Side: Deploy Service

Un ejemplo realista completo en ambos lenguajes para ver todas las diferencias juntas:

### 10.1 Go — Deploy Service Completo

```go
package main

import (
    "context"
    "errors"
    "fmt"
    "math/rand"
    "strings"
    "time"
)

// ─── Errores de dominio ─────────────────────────────────────────

var (
    ErrNotFound     = errors.New("not found")
    ErrUnauthorized = errors.New("unauthorized")
    ErrConflict     = errors.New("conflict")
    ErrTimeout      = errors.New("timeout")
    ErrValidation   = errors.New("validation")
)

type DeployError struct {
    Op       string // "pull_image", "create_pod", etc.
    Resource string // nombre del recurso
    Kind     error  // sentinel
    Err      error  // causa original
}

func (e *DeployError) Error() string {
    return fmt.Sprintf("%s %s: %v", e.Op, e.Resource, e.Err)
}

func (e *DeployError) Unwrap() error { return e.Err }

func (e *DeployError) Is(target error) bool {
    return errors.Is(e.Kind, target)
}

// Retriable interface — errores que se pueden reintentar
type Retriable interface {
    IsRetriable() bool
}

func (e *DeployError) IsRetriable() bool {
    return errors.Is(e.Kind, ErrTimeout)
}

// ─── Servicio de deploy ─────────────────────────────────────────

type Image struct {
    Registry string
    Name     string
    Tag      string
    Digest   string
}

type Pod struct {
    ID     string
    Name   string
    IP     string
    Port   int
    Status string
}

type Deployment struct {
    ID       string
    Name     string
    Image    Image
    Pod      Pod
    Endpoint string
}

func validateImage(image string) (Image, error) {
    parts := strings.SplitN(image, "/", 2)
    if len(parts) != 2 {
        return Image{}, &DeployError{
            Op:       "validate",
            Resource: image,
            Kind:     ErrValidation,
            Err:      fmt.Errorf("invalid image format, expected registry/name:tag"),
        }
    }
    nameTag := strings.SplitN(parts[1], ":", 2)
    if len(nameTag) != 2 {
        return Image{}, &DeployError{
            Op:       "validate",
            Resource: image,
            Kind:     ErrValidation,
            Err:      fmt.Errorf("missing tag, expected registry/name:tag"),
        }
    }
    return Image{
        Registry: parts[0],
        Name:     nameTag[0],
        Tag:      nameTag[1],
    }, nil
}

func pullImage(ctx context.Context, img Image) (Image, error) {
    select {
    case <-ctx.Done():
        return Image{}, &DeployError{
            Op:       "pull_image",
            Resource: fmt.Sprintf("%s/%s:%s", img.Registry, img.Name, img.Tag),
            Kind:     ErrTimeout,
            Err:      fmt.Errorf("pull cancelled: %w", ctx.Err()),
        }
    case <-time.After(time.Duration(rand.Intn(100)) * time.Millisecond):
    }

    if strings.Contains(img.Name, "private") {
        return Image{}, &DeployError{
            Op:       "pull_image",
            Resource: img.Name,
            Kind:     ErrUnauthorized,
            Err:      fmt.Errorf("authentication required for private registry"),
        }
    }

    img.Digest = fmt.Sprintf("sha256:%x", rand.Int63())
    return img, nil
}

func createPod(ctx context.Context, name string, img Image) (Pod, error) {
    select {
    case <-ctx.Done():
        return Pod{}, &DeployError{
            Op: "create_pod", Resource: name, Kind: ErrTimeout,
            Err: fmt.Errorf("create cancelled: %w", ctx.Err()),
        }
    case <-time.After(time.Duration(rand.Intn(50)) * time.Millisecond):
    }

    if strings.Contains(name, "conflict") {
        return Pod{}, &DeployError{
            Op: "create_pod", Resource: name, Kind: ErrConflict,
            Err: fmt.Errorf("pod '%s' already exists in namespace", name),
        }
    }

    return Pod{
        ID:     fmt.Sprintf("pod-%x", rand.Int31()),
        Name:   name,
        IP:     fmt.Sprintf("10.0.%d.%d", rand.Intn(255), rand.Intn(255)),
        Port:   8080,
        Status: "running",
    }, nil
}

func registerService(ctx context.Context, name string, pod Pod) (string, error) {
    select {
    case <-ctx.Done():
        return "", &DeployError{
            Op: "register", Resource: name, Kind: ErrTimeout,
            Err: fmt.Errorf("registration cancelled: %w", ctx.Err()),
        }
    case <-time.After(time.Duration(rand.Intn(30)) * time.Millisecond):
    }

    return fmt.Sprintf("http://%s.svc.cluster.local:%d", name, pod.Port), nil
}

// Deploy — funcion principal con error handling Go idiomatico
func Deploy(ctx context.Context, name, imageRef string) (*Deployment, error) {
    // Paso 1: validar
    img, err := validateImage(imageRef)
    if err != nil {
        return nil, fmt.Errorf("deploy %s: %w", name, err)
    }

    // Paso 2: pull
    img, err = pullImage(ctx, img)
    if err != nil {
        return nil, fmt.Errorf("deploy %s: %w", name, err)
    }

    // Paso 3: crear pod
    pod, err := createPod(ctx, name, img)
    if err != nil {
        return nil, fmt.Errorf("deploy %s: %w", name, err)
    }

    // Paso 4: registrar
    endpoint, err := registerService(ctx, name, pod)
    if err != nil {
        // Cleanup: si el registro falla, intentar destruir el pod
        // (Go no tiene RAII, asi que cleanup es manual)
        fmt.Printf("WARN: cleaning up pod %s after register failure\n", pod.ID)
        return nil, fmt.Errorf("deploy %s: %w", name, err)
    }

    return &Deployment{
        ID:       fmt.Sprintf("deploy-%x", rand.Int31()),
        Name:     name,
        Image:    img,
        Pod:      pod,
        Endpoint: endpoint,
    }, nil
}

// DeployWithRetry — reintentar si el error es retriable
func DeployWithRetry(ctx context.Context, name, image string, maxRetries int) (*Deployment, error) {
    var lastErr error

    for attempt := 0; attempt <= maxRetries; attempt++ {
        if attempt > 0 {
            backoff := time.Duration(1<<uint(attempt-1)) * 200 * time.Millisecond
            fmt.Printf("  Retry %d/%d after %v...\n", attempt, maxRetries, backoff)
            select {
            case <-ctx.Done():
                return nil, fmt.Errorf("retry cancelled: %w", ctx.Err())
            case <-time.After(backoff):
            }
        }

        dep, err := Deploy(ctx, name, image)
        if err == nil {
            return dep, nil
        }

        lastErr = err

        // Verificar si el error es retriable
        var retriable Retriable
        if errors.As(err, &retriable) && retriable.IsRetriable() {
            continue // reintentar
        }

        // No es retriable — fallar inmediatamente
        return nil, err
    }

    return nil, fmt.Errorf("all %d retries exhausted: %w", maxRetries, lastErr)
}

func main() {
    ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
    defer cancel()

    deployments := []struct {
        name  string
        image string
    }{
        {"api-gateway", "docker.io/myapp:v1.2.3"},
        {"user-service", "docker.io/users:v2.0.0"},
        {"bad-format", "no-tag-here"},
        {"auth-service", "docker.io/private-auth:v1.0"},
        {"conflict-pod", "docker.io/web:v1.0.0"},
    }

    fmt.Println("=== Go Deploy Service ===\n")

    for _, d := range deployments {
        fmt.Printf("Deploying %s (%s)...\n", d.name, d.image)

        dep, err := Deploy(ctx, d.name, d.image)
        if err != nil {
            // Clasificar el error
            switch {
            case errors.Is(err, ErrValidation):
                fmt.Printf("  VALIDATION ERROR: %v\n", err)
            case errors.Is(err, ErrUnauthorized):
                fmt.Printf("  AUTH ERROR: %v\n", err)
            case errors.Is(err, ErrConflict):
                fmt.Printf("  CONFLICT: %v\n", err)
            case errors.Is(err, ErrTimeout):
                fmt.Printf("  TIMEOUT: %v\n", err)
            default:
                fmt.Printf("  ERROR: %v\n", err)
            }

            // Extraer DeployError si existe
            var de *DeployError
            if errors.As(err, &de) {
                fmt.Printf("  Details: op=%s resource=%s\n", de.Op, de.Resource)
            }
        } else {
            fmt.Printf("  OK: id=%s endpoint=%s\n", dep.ID, dep.Endpoint)
        }
        fmt.Println()
    }
}
```

### 10.2 Rust — Deploy Service Completo (Equivalente)

```rust
use std::fmt;
use std::time::Duration;
use rand::Rng;
use thiserror::Error;
use tokio::time::sleep;

// ─── Errores de dominio ─────────────────────────────────────────

#[derive(Error, Debug)]
enum DeployError {
    #[error("validate {resource}: {message}")]
    Validation {
        resource: String,
        message: String,
    },

    #[error("pull_image {resource}: authentication required")]
    Unauthorized {
        resource: String,
    },

    #[error("create_pod {resource}: already exists")]
    Conflict {
        resource: String,
    },

    #[error("{op} {resource}: operation timed out")]
    Timeout {
        op: String,
        resource: String,
        #[source]
        source: tokio::time::error::Elapsed,
    },

    #[error("{op} {resource}: cancelled")]
    Cancelled {
        op: String,
        resource: String,
    },
}

impl DeployError {
    fn is_retriable(&self) -> bool {
        matches!(self, DeployError::Timeout { .. })
    }

    fn op(&self) -> &str {
        match self {
            DeployError::Validation { .. } => "validate",
            DeployError::Unauthorized { .. } => "pull_image",
            DeployError::Conflict { .. } => "create_pod",
            DeployError::Timeout { op, .. } => op,
            DeployError::Cancelled { op, .. } => op,
        }
    }

    fn resource(&self) -> &str {
        match self {
            DeployError::Validation { resource, .. } => resource,
            DeployError::Unauthorized { resource, .. } => resource,
            DeployError::Conflict { resource, .. } => resource,
            DeployError::Timeout { resource, .. } => resource,
            DeployError::Cancelled { resource, .. } => resource,
        }
    }
}

// ─── Modelos ────────────────────────────────────────────────────

#[derive(Debug, Clone)]
struct Image {
    registry: String,
    name: String,
    tag: String,
    digest: String,
}

impl fmt::Display for Image {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}/{}:{}", self.registry, self.name, self.tag)
    }
}

#[derive(Debug)]
struct Pod {
    id: String,
    name: String,
    ip: String,
    port: u16,
}

#[derive(Debug)]
struct Deployment {
    id: String,
    name: String,
    image: Image,
    pod: Pod,
    endpoint: String,
}

// ─── Funciones de deploy ────────────────────────────────────────

fn validate_image(image: &str) -> Result<Image, DeployError> {
    let parts: Vec<&str> = image.splitn(2, '/').collect();
    if parts.len() != 2 {
        return Err(DeployError::Validation {
            resource: image.to_string(),
            message: "expected registry/name:tag".to_string(),
        });
    }
    let name_tag: Vec<&str> = parts[1].splitn(2, ':').collect();
    if name_tag.len() != 2 {
        return Err(DeployError::Validation {
            resource: image.to_string(),
            message: "missing tag".to_string(),
        });
    }
    Ok(Image {
        registry: parts[0].to_string(),
        name: name_tag[0].to_string(),
        tag: name_tag[1].to_string(),
        digest: String::new(),
    })
}

async fn pull_image(img: Image) -> Result<Image, DeployError> {
    let mut rng = rand::thread_rng();
    sleep(Duration::from_millis(rng.gen_range(10..100))).await;

    if img.name.contains("private") {
        return Err(DeployError::Unauthorized {
            resource: img.name.clone(),
        });
    }

    Ok(Image {
        digest: format!("sha256:{:x}", rng.gen::<u64>()),
        ..img
    })
}

async fn create_pod(name: &str, _img: &Image) -> Result<Pod, DeployError> {
    let mut rng = rand::thread_rng();
    sleep(Duration::from_millis(rng.gen_range(10..50))).await;

    if name.contains("conflict") {
        return Err(DeployError::Conflict {
            resource: name.to_string(),
        });
    }

    Ok(Pod {
        id: format!("pod-{:x}", rng.gen::<u32>()),
        name: name.to_string(),
        ip: format!("10.0.{}.{}", rng.gen_range(0..255), rng.gen_range(0..255)),
        port: 8080,
    })
}

async fn register_service(name: &str, pod: &Pod) -> Result<String, DeployError> {
    let mut rng = rand::thread_rng();
    sleep(Duration::from_millis(rng.gen_range(10..30))).await;
    Ok(format!("http://{}.svc.cluster.local:{}", name, pod.port))
}

// Deploy — funcion principal con ? operator
async fn deploy(name: &str, image_ref: &str) -> Result<Deployment, DeployError> {
    let img = validate_image(image_ref)?;    // ? propaga DeployError
    let img = pull_image(img).await?;         // ? propaga DeployError
    let pod = create_pod(name, &img).await?;  // ? propaga DeployError
    let endpoint = register_service(name, &pod).await?;  // ? propaga

    // Si register falla, pod se dropea automaticamente (RAII)
    // En Go necesitarias cleanup manual

    let mut rng = rand::thread_rng();
    Ok(Deployment {
        id: format!("deploy-{:x}", rng.gen::<u32>()),
        name: name.to_string(),
        image: img,
        pod,
        endpoint,
    })
}

// Deploy con reintentos
async fn deploy_with_retry(
    name: &str,
    image: &str,
    max_retries: u32,
) -> Result<Deployment, DeployError> {
    let mut last_err = None;

    for attempt in 0..=max_retries {
        if attempt > 0 {
            let backoff = Duration::from_millis(200 * 2u64.pow(attempt - 1));
            println!("  Retry {}/{} after {:?}...", attempt, max_retries, backoff);
            sleep(backoff).await;
        }

        match deploy(name, image).await {
            Ok(dep) => return Ok(dep),
            Err(e) => {
                if !e.is_retriable() {
                    return Err(e);  // no retriable → fallo inmediato
                }
                last_err = Some(e);
            }
        }
    }

    Err(last_err.unwrap()) // seguro: el loop siempre asigna last_err
}

#[tokio::main]
async fn main() {
    let deployments = vec![
        ("api-gateway", "docker.io/myapp:v1.2.3"),
        ("user-service", "docker.io/users:v2.0.0"),
        ("bad-format", "no-tag-here"),
        ("auth-service", "docker.io/private-auth:v1.0"),
        ("conflict-pod", "docker.io/web:v1.0.0"),
    ];

    println!("=== Rust Deploy Service ===\n");

    for (name, image) in &deployments {
        println!("Deploying {} ({})...", name, image);

        match deploy(name, image).await {
            Ok(dep) => {
                println!("  OK: id={} endpoint={}", dep.id, dep.endpoint);
            }
            // match exhaustivo — TODAS las variantes cubiertas:
            Err(DeployError::Validation { resource, message }) => {
                println!("  VALIDATION ERROR: {}: {}", resource, message);
            }
            Err(DeployError::Unauthorized { resource }) => {
                println!("  AUTH ERROR: {} needs credentials", resource);
            }
            Err(DeployError::Conflict { resource }) => {
                println!("  CONFLICT: {} already exists", resource);
            }
            Err(DeployError::Timeout { op, resource, .. }) => {
                println!("  TIMEOUT: {} {}", op, resource);
            }
            Err(DeployError::Cancelled { op, resource }) => {
                println!("  CANCELLED: {} {}", op, resource);
            }
            // Si agregas una nueva variante a DeployError,
            // este match DEJA DE COMPILAR hasta que la manejes
        }
        println!();
    }
}
```

### 10.3 Analisis Line-by-Line

```
┌──────────────────────────────────────────────────────────────────────────┐
│         SIDE-BY-SIDE: Deploy Function                                    │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  GO:                                 RUST:                               │
│  ──────────────────────              ──────────────────────              │
│  func Deploy(ctx, name, img)         async fn deploy(name, img)         │
│    (*Deployment, error) {              -> Result<Deployment, DeployError>│
│                                                                          │
│    img, err := validateImage(img)      let img = validate_image(img)?;  │
│    if err != nil {                     // ? = si Err, return Err        │
│      return nil, fmt.Errorf(...)       // From trait convierte el tipo  │
│    }                                                                     │
│                                                                          │
│    img, err = pullImage(ctx, img)      let img = pull_image(img).await?;│
│    if err != nil {                                                       │
│      return nil, fmt.Errorf(...)                                         │
│    }                                                                     │
│                                                                          │
│    pod, err := createPod(ctx, n, img)  let pod = create_pod(n,&i).await?│
│    if err != nil {                                                       │
│      return nil, fmt.Errorf(...)                                         │
│    }                                                                     │
│                                                                          │
│    ep, err := registerService(ctx,..)  let ep = register(n,&p).await?;  │
│    if err != nil {                                                       │
│      // cleanup manual                 // RAII: pod se dropea auto      │
│      return nil, fmt.Errorf(...)                                         │
│    }                                                                     │
│                                                                          │
│    return &Deployment{...}, nil        Ok(Deployment{...})              │
│  }                                   }                                   │
│                                                                          │
│  LINEAS: ~25                         LINEAS: ~8                          │
│  ERROR HANDLING: 12 lineas           ERROR HANDLING: 0 lineas (en ?)    │
│  CONTEXTO: en cada fmt.Errorf        CONTEXTO: en el tipo DeployError   │
│  CLEANUP: manual (defer o inline)    CLEANUP: RAII (Drop trait)         │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 11. Cleanup y RAII: defer vs Drop

### 11.1 Go: defer para Cleanup

```go
func processFile(path string) error {
    file, err := os.Open(path)
    if err != nil {
        return err
    }
    defer file.Close() // se ejecuta cuando processFile retorna

    // Si aqui hay un error, defer Close() se ejecuta igualmente
    data, err := io.ReadAll(file)
    if err != nil {
        return err // file.Close() se ejecuta via defer
    }

    // Problema: si file.Close() falla, el error se pierde
    // Solucion: named return + defer
    return processData(data)
}

// Cleanup con error capture:
func processFileSafe(path string) (retErr error) {
    file, err := os.Open(path)
    if err != nil {
        return err
    }
    defer func() {
        if cerr := file.Close(); cerr != nil && retErr == nil {
            retErr = cerr // capturar error de Close solo si no habia error previo
        }
    }()

    data, err := io.ReadAll(file)
    if err != nil {
        return err
    }
    return processData(data)
}

// Multiples recursos — cada defer en orden LIFO:
func deploy(ctx context.Context, name string) error {
    pod, err := createPod(ctx, name)
    if err != nil {
        return err
    }
    defer func() {
        if retErr != nil {
            destroyPod(pod) // cleanup si algo falla despues
        }
    }()

    svc, err := createService(ctx, name, pod)
    if err != nil {
        return err // → destroyPod via defer
    }
    defer func() {
        if retErr != nil {
            deleteService(svc) // cleanup service tambien
        }
    }()

    return registerDNS(ctx, name, svc) // si falla → cleanup ambos
}
```

### 11.2 Rust: Drop Trait (RAII)

```rust
// En Rust, el cleanup es AUTOMATICO via Drop trait:
fn process_file(path: &str) -> Result<(), io::Error> {
    let file = File::open(path)?; // file implementa Drop
    let data = std::io::read_to_string(file)?;
    // Si read_to_string falla, file se dropea automaticamente
    // Drop::drop cierra el file descriptor
    process_data(&data)?;
    Ok(())
    // file se dropea aqui tambien (fin del scope)
}

// Rust GARANTIZA que Drop se ejecuta cuando el valor sale del scope
// No necesitas defer — es automatico y deterministico

// Para cleanup condicional (como el deploy example):
struct PodGuard {
    pod: Pod,
    committed: bool,
}

impl PodGuard {
    fn new(pod: Pod) -> Self {
        PodGuard { pod, committed: false }
    }

    fn commit(mut self) -> Pod {
        self.committed = true;
        self.pod
    }
}

impl Drop for PodGuard {
    fn drop(&mut self) {
        if !self.committed {
            // Cleanup: destruir pod si no se hizo commit
            eprintln!("Cleaning up pod {}", self.pod.id);
            // destroy_pod(&self.pod);
        }
    }
}

async fn deploy(name: &str) -> Result<Deployment, DeployError> {
    let pod = create_pod(name).await?;
    let guard = PodGuard::new(pod); // guard destruye pod si deploy falla

    let svc = create_service(name, &guard.pod).await?;
    // Si create_service falla → guard se dropea → pod destruido

    let endpoint = register_dns(name, &svc).await?;
    // Si register_dns falla → guard se dropea → pod destruido

    let pod = guard.commit(); // exito → no destruir pod
    Ok(Deployment { pod, endpoint, .. })
}
```

### 11.3 Comparacion de Cleanup

```
┌──────────────────────────────────────────────────────────────────────────┐
│                 CLEANUP: Go defer vs Rust Drop (RAII)                    │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│                      GO (defer)              RUST (Drop)                │
│  Mecanismo           defer func() {...}      impl Drop for T            │
│  Cuando se ejecuta   Al retornar la funcion  Al salir del scope         │
│  Orden               LIFO (ultimo primero)   LIFO (ultimo primero)      │
│  Obligatorio         No (puedes olvidarlo)   Si (automatico)            │
│  Error de cleanup    Necesita named return   Drop no retorna error      │
│  Cleanup condicional defer con flag/check    Guard pattern + commit     │
│  Recursos multiples  Multiples defer         Multiples variables        │
│  Scope               Funcion completa        Bloque (mas granular)     │
│                                                                          │
│  RIESGO EN GO:                                                           │
│  file, _ := os.Open(path)   // olvida defer Close → leak               │
│                                                                          │
│  IMPOSIBLE EN RUST:                                                      │
│  let file = File::open(path)?; // Drop se llama SIEMPRE                │
│  // No puedes olvidar el cleanup — el compilador lo garantiza           │
│                                                                          │
│  LIMITACION DE RUST:                                                     │
│  Drop::drop no puede retornar error. Si file.close() falla:            │
│  - Go: defer captura el error de Close                                  │
│  - Rust: Drop ignora silenciosamente (o panic en debug)                 │
│  Solucion Rust: cerrar explicitamente ANTES del Drop                    │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 12. Tabla Exhaustiva de Equivalencias

```
┌──────────────────────────────────────────────────────────────────────────────────────┐
│                   GO vs RUST — ERROR HANDLING COMPLETO                                │
├────────────────────────┬──────────────────────────┬──────────────────────────────────┤
│ CONCEPTO               │ GO                       │ RUST                             │
├────────────────────────┼──────────────────────────┼──────────────────────────────────┤
│ Tipo de error          │ interface error           │ trait std::error::Error          │
│ Retorno                │ (T, error)                │ Result<T, E>                     │
│ Exito                  │ (value, nil)              │ Ok(value)                        │
│ Error                  │ (zero, err)               │ Err(e)                           │
│ Ausencia sin error     │ (nil, nil) o sentinel     │ Result<Option<T>, E>             │
│ Crear error basico     │ errors.New("msg")         │ anyhow!("msg")                   │
│ Crear error con fmt    │ fmt.Errorf("ctx: %v",e)   │ anyhow!("ctx: {}", e)            │
│ Wrap con causa         │ fmt.Errorf("c: %w", e)    │ .context("c")  (anyhow)          │
│ Error custom           │ struct + Error() string   │ enum + #[derive(Error)]          │
│ Sentinel error         │ var Err = errors.New(..)  │ enum variant (sin datos)         │
│ Error con campos       │ struct con campos         │ enum variant con campos          │
│ Verificar tipo         │ errors.As(err, &target)   │ err.downcast_ref::<T>()          │
│ Verificar identidad    │ errors.Is(err, sentinel)  │ match (compile-time)             │
│ Propagar               │ return ..., err           │ ?                                │
│ Propagar con contexto  │ return .., fmt.Errorf(%w) │ .context("msg")?                 │
│ Conversion automatica  │ No hay                    │ From trait + ?                   │
│ Multi-error            │ errors.Join (Go 1.20+)    │ Vec<Error> (manual)              │
│ Exhaustividad          │ No (linter opcional)      │ Si (match obligatorio)           │
│ Ignorar error          │ _ (compila silencioso)    │ let _ = (warning)                │
│ Panic                  │ panic("msg")              │ panic!("msg")                    │
│ Catch panic            │ recover() en defer        │ catch_unwind() (raro)            │
│ Crash explicito        │ log.Fatal / panic         │ .unwrap() / .expect("msg")       │
│ Cleanup                │ defer                     │ Drop trait (RAII)                │
│ Backtrace              │ debug.Stack() (manual)    │ RUST_BACKTRACE=1 (automatico)    │
│ Lib errores (lib)      │ stdlib (minimalista)      │ thiserror                        │
│ Lib errores (app)      │ stdlib + fmt.Errorf       │ anyhow                           │
│ HTTP error type        │ Custom APIError           │ IntoResponse + enum              │
│ Errgroup               │ errgroup.Group            │ JoinSet / try_join_all           │
│ Retry                  │ Manual loop               │ Manual loop / retry crate        │
│ Logging de errores     │ slog / log (manual)       │ tracing (automatic spans)        │
├────────────────────────┼──────────────────────────┼──────────────────────────────────┤
│ FILOSOFIA              │ Simple, explicito, manual │ Seguro, ergonomico, automatico   │
│ CURVA APRENDIZAJE      │ Baja                      │ Alta                             │
│ BOILERPLATE            │ Alto (if err != nil)      │ Bajo (?)                         │
│ SEGURIDAD              │ Runtime checks            │ Compile-time guarantees          │
│ FLEXIBILIDAD           │ Alta (interface abierta)  │ Media (enum cerrado)             │
│ PERFORMANCE            │ Interface dispatch        │ Zero-cost abstractions           │
└────────────────────────┴──────────────────────────┴──────────────────────────────────┘
```

---

## 13. Cuando Go es Mejor, Cuando Rust es Mejor

```
┌──────────────────────────────────────────────────────────────────────────┐
│                   TRADEOFFS REALES                                       │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  GO ES MEJOR CUANDO:                                                    │
│  ├─ Equipos grandes con niveles mixtos de experiencia                   │
│  │   → if err != nil es obvio para cualquiera                           │
│  ├─ Prototipado rapido / MVPs                                           │
│  │   → Menos tipos que definir, mas rapido de escribir                  │
│  ├─ El error handling es simple (propagar o log)                        │
│  │   → No necesitas la maquinaria de tipos de Rust                      │
│  ├─ APIs con errores que cambian frecuentemente                         │
│  │   → Interface abierta = no romper callers al agregar errores         │
│  ├─ Debugging rapido                                                    │
│  │   → El error message tiene todo el contexto (cadena de wrapping)     │
│  └─ Microservicios con logica simple                                    │
│      → Kubernetes, Docker — escritos en Go por esta razon               │
│                                                                          │
│  RUST ES MEJOR CUANDO:                                                  │
│  ├─ Correctness es critica (fintech, crypto, medical)                   │
│  │   → El compilador verifica que todos los errores se manejan          │
│  ├─ Librerias publicas con API estable                                  │
│  │   → enum cerrado fuerza a los callers a manejar nuevos errores       │
│  ├─ Muchas variantes de error con datos diferentes                      │
│  │   → enum con match es mas ergonomico que errors.As + type switch     │
│  ├─ Performance critica (sin GC, zero-cost abstractions)                │
│  │   → Error handling no tiene overhead en runtime                      │
│  ├─ El codigo debe ser mantenido a largo plazo                          │
│  │   → Agregar variante = compilador te guia a todos los match          │
│  └─ Sistemas de baja latencia / embedded                               │
│      → No hay GC pauses, no hay interface dispatch overhead             │
│                                                                          │
│  AMBOS SON IGUALES CUANDO:                                               │
│  ├─ CLIs simples                                                        │
│  ├─ Scripting de infraestructura                                        │
│  └─ Herramientas internas one-off                                       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 14. Transferir Patrones entre Go y Rust

### 14.1 Patron Go → Rust: Wrapping con Contexto

```go
// GO: wrapping en cada capa
func deploy(name string) error {
    if err := validate(name); err != nil {
        return fmt.Errorf("deploy %s: validate: %w", name, err)
    }
    // ...
}
```

```rust
// RUST equivalente: .context() de anyhow
fn deploy(name: &str) -> anyhow::Result<()> {
    validate(name)
        .context(format!("deploy {}: validate", name))?;
    // ...
    Ok(())
}

// O con thiserror para errores tipados:
fn deploy(name: &str) -> Result<(), DeployError> {
    validate(name)
        .map_err(|e| DeployError::Validation {
            step: "validate",
            name: name.to_string(),
            source: e,
        })?;
    Ok(())
}
```

### 14.2 Patron Rust → Go: Error Enum como Interface

```rust
// RUST: match exhaustivo en variantes
match deploy(name).await {
    Ok(dep) => use(dep),
    Err(DeployError::Validation { .. }) => /* handle */,
    Err(DeployError::Timeout { .. }) => /* retry */,
    Err(DeployError::Conflict { .. }) => /* skip */,
    // compilador obliga a cubrir todos
}
```

```go
// GO equivalente mas cercano: type switch
switch err := deploy(name); {
case err == nil:
    use(dep)
case errors.Is(err, ErrValidation):
    // handle
case errors.Is(err, ErrTimeout):
    // retry
case errors.Is(err, ErrConflict):
    // skip
default:
    // GO REQUIERE default — no hay exhaustividad
    return fmt.Errorf("unexpected: %w", err)
}
```

### 14.3 Patron Rust → Go: Option para nil Ambiguo

```rust
// RUST: distingue "no encontrado" de "error"
fn find_user(id: &str) -> Result<Option<User>, DbError> {
    // Ok(Some(user)) = encontrado
    // Ok(None) = no encontrado (no es error)
    // Err(e) = error de DB
}
```

```go
// GO: no tiene Option<T>, pero puedes emular:

// Opcion 1: retornar (nil, nil) — ambiguo
func FindUser(id string) (*User, error) {
    // user, nil → encontrado
    // nil, nil → no encontrado (no es error)
    // nil, err → error
}

// Opcion 2: sentinel error (comun en Go)
var ErrUserNotFound = errors.New("user not found")
func FindUser(id string) (*User, error) {
    // user, nil → encontrado
    // nil, ErrUserNotFound → no encontrado
    // nil, err → error de DB
}

// Opcion 3: bool explicito (como map lookup)
func FindUser(id string) (user *User, found bool, err error) {
    // user, true, nil → encontrado
    // nil, false, nil → no encontrado
    // nil, false, err → error
}

// La opcion 2 es la mas idiomatica en Go
```

### 14.4 Patron Go → Rust: errgroup → JoinSet

```go
// GO: errgroup para tareas paralelas con error
g, ctx := errgroup.WithContext(ctx)
for _, s := range servers {
    s := s
    g.Go(func() error { return check(ctx, s) })
}
err := g.Wait()
```

```rust
// RUST: JoinSet equivalente
let mut set = JoinSet::new();
for s in &servers {
    let s = s.clone();
    set.spawn(async move { check(&s).await });
}
while let Some(result) = set.join_next().await {
    result??;  // propagar JoinError y luego el error de check
}

// O con futures::try_join_all (mas conciso):
let futures: Vec<_> = servers.iter()
    .map(|s| check(s))
    .collect();
try_join_all(futures).await?;
```

---

## 15. Ejercicios

### Ejercicio 1: Traductor Go→Rust

Toma el siguiente programa Go y traducelo a Rust usando thiserror:

```go
var (
    ErrNotFound    = errors.New("not found")
    ErrForbidden   = errors.New("forbidden")
    ErrRateLimit   = errors.New("rate limited")
)

type ServiceError struct {
    Op       string
    Resource string
    Kind     error
    Err      error
}

func (e *ServiceError) Error() string { ... }
func (e *ServiceError) Unwrap() error { return e.Err }
func (e *ServiceError) Is(target error) bool { return errors.Is(e.Kind, target) }

func GetResource(ctx context.Context, id string) (*Resource, error) {
    token, err := authenticate(ctx)
    if err != nil { return nil, fmt.Errorf("get %s: auth: %w", id, err) }

    if !token.HasPermission("read") {
        return nil, &ServiceError{Op: "get", Resource: id, Kind: ErrForbidden, Err: ...}
    }

    resource, err := fetchFromDB(ctx, id)
    if err != nil {
        if errors.Is(err, sql.ErrNoRows) {
            return nil, &ServiceError{Op: "get", Resource: id, Kind: ErrNotFound, Err: err}
        }
        return nil, fmt.Errorf("get %s: db: %w", id, err)
    }
    return resource, nil
}
```

```
Requisitos de la traduccion:
- Usar #[derive(Error)] de thiserror
- Todos los errores como variantes de un enum ServiceError
- Verificar que match cubre TODAS las variantes
- Usar ? para propagacion
- Documentar las diferencias de comportamiento entre las dos versiones
```

### Ejercicio 2: Error Budget Calculator (Ambos Lenguajes)

Implementa el mismo programa en Go y Rust. El programa calcula error budgets de SLOs:

```
Requisitos funcionales:
- Leer un archivo YAML con definicion de SLOs:
  services:
    - name: api
      slo: 99.9
      window_days: 30
    - name: db
      slo: 99.99
      window_days: 7
- Leer un archivo CSV con incidentes:
  service,start,end,impact
  api,2026-03-01T10:00:00Z,2026-03-01T10:45:00Z,full
  db,2026-03-28T14:00:00Z,2026-03-28T14:05:00Z,partial
- Calcular: budget total, budget consumido, budget restante, % usado
- Errores posibles: file not found, YAML parse, CSV parse, date parse,
  service not defined, overlapping incidents, budget exceeded

Requisitos de error handling:
- Go: usar todos los patrones del capitulo (sentinels, custom types,
  wrapping, errors.Is/As, errgroup para leer archivos en paralelo)
- Rust: usar thiserror, ? operator, match exhaustivo
- Ambos: el programa debe compilar y producir la misma salida
- Documentar: ¿que errores atrapo Rust en compile-time que Go no?
```

### Ejercicio 3: Middleware HTTP (Ambos Lenguajes)

Implementa el mismo middleware de error handling HTTP en Go y Rust (Axum):

```
Requisitos:
- Handler que retorna error (Go: AppHandler, Rust: Result<impl IntoResponse, AppError>)
- Middleware de recovery (panic → 500)
- Error tipado con: code, message, details, status
- Validacion que retorna TODOS los errores de campo
- Mapeo dominio→HTTP (sentinel/variant → status code)
- Tests que verifican el formato JSON de cada tipo de error
- Comparar: lineas de codigo, tipos definidos, tests necesarios
  ¿Donde requiere Rust menos tests que Go (por garantias de compilador)?
```

---

## Resumen

```
┌──────────────────────────────────────────────────────────────────────────┐
│          ERROR HANDLING: GO vs RUST — RESUMEN COMPLETO                   │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  TIPO DE ERROR:                                                          │
│  Go: interface error { Error() string }  — abierto, runtime             │
│  Rust: enum + Error trait                — cerrado, compile-time         │
│                                                                          │
│  PROPAGACION:                                                            │
│  Go: if err != nil { return ..., fmt.Errorf("%w", err) }                │
│  Rust: result?  (+ From trait para conversion automatica)               │
│  Ratio: Go 3:1 (error:logica), Rust 0:1                                │
│                                                                          │
│  WRAPPING:                                                               │
│  Go: fmt.Errorf("%w") → cadena de texto con Unwrap                     │
│  Rust: enum variant + #[source] / .context()                            │
│                                                                          │
│  OBLIGATORIEDAD:                                                         │
│  Go: puedes ignorar error (compila sin warning)                         │
│  Rust: debes manejar Result (warning/error si no lo haces)              │
│                                                                          │
│  EXHAUSTIVIDAD:                                                          │
│  Go: switch sin default compila (linter opcional)                       │
│  Rust: match OBLIGA a cubrir todas las variantes (compilador)           │
│                                                                          │
│  CLEANUP:                                                                │
│  Go: defer (manual, puedes olvidarlo)                                   │
│  Rust: Drop/RAII (automatico, imposible olvidarlo)                      │
│                                                                          │
│  LIBRERIAS:                                                              │
│  Go: stdlib (errors, fmt) — minimalista, suficiente                     │
│  Rust: thiserror (librerias) + anyhow (aplicaciones)                    │
│                                                                          │
│  CUANDO ELEGIR:                                                          │
│  Go: equipos grandes, prototipado, microservicios, ops tooling         │
│  Rust: correctness critica, librerias, performance, long-term maint     │
│                                                                          │
│  PATRONES TRANSFERIBLES:                                                 │
│  Go→Rust: wrapping con contexto (.context), errgroup (JoinSet)          │
│  Rust→Go: exhaustividad (linter), Option (sentinel), From (mapError)   │
│                                                                          │
│  ═══════════════════════════════════════════════════════════════════     │
│  Este topico CIERRA el Capitulo 7 (Manejo de Errores).                  │
│  Los 7 topicos cubren: error como valor, if err != nil, wrapping,       │
│  sentinels/tipos, errgroup, APIs, y esta comparacion con Rust.          │
│  ═══════════════════════════════════════════════════════════════════     │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: C08 - Concurrencia, S01 - Goroutines, T01 - Goroutines — go keyword, modelo M:N (goroutines sobre OS threads), scheduler de Go
