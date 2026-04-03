# Comparacion con Rust — defer vs Drop, panic vs panic!, recover vs catch_unwind

## 1. Introduccion

Go y Rust resuelven los mismos problemas fundamentales — cleanup de recursos, manejo de errores fatales y contencion de fallos — pero con filosofias radicalmente opuestas. Entender las diferencias no solo te ayuda a escribir mejor Go y mejor Rust, sino que revela **por que** cada lenguaje tomo las decisiones que tomo.

```
┌──────────────────────────────────────────────────────────────┐
│  FILOSOFIAS OPUESTAS                                         │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Go:   "Haz que sea simple y explicito"                      │
│  ├─ defer: el programador decide que limpiar y cuando        │
│  ├─ panic: crash explicito, recover para contener            │
│  ├─ error: valor de retorno, if err != nil                   │
│  └─ GC: la memoria no es tu problema                         │
│                                                              │
│  Rust: "Haz que sea imposible equivocarse"                   │
│  ├─ Drop: limpieza automatica al salir del scope             │
│  ├─ panic!: crash, catch_unwind para FFI boundaries          │
│  ├─ Result: tipo que OBLIGA a manejar el error               │
│  └─ Ownership: la memoria se gestiona en compile time        │
│                                                              │
│  C:    "Confio en que sabes lo que haces"                    │
│  ├─ Manual: free(), fclose(), cada path de error             │
│  ├─ abort/assert: crash sin cleanup                          │
│  ├─ errno/return codes: facil de ignorar                     │
│  └─ Manual: malloc/free, buffer overflows posibles           │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

Este topico es una comparacion profunda y practica. Cada seccion muestra el mismo problema resuelto en Go, Rust y C, analizando trade-offs.

---

## 2. defer vs Drop — Cleanup de recursos

### El problema fundamental

Cuando abres un recurso (archivo, conexion, lock), necesitas cerrarlo. El desafio es garantizar que se cierre **en todos los paths de ejecucion** — incluyendo errores, returns tempranos y panics.

### Go: defer — Explicito y por funcion

```go
func processFile(path string) error {
    f, err := os.Open(path)
    if err != nil {
        return err
    }
    defer f.Close()  // el programador DECIDE poner defer

    data, err := io.ReadAll(f)
    if err != nil {
        return err  // f.Close() se ejecuta via defer
    }

    return process(data)  // f.Close() se ejecuta via defer
}
```

**Caracteristicas:**
- El programador escribe `defer` explicitamente
- Se ejecuta al salir de la **funcion**, no del bloque
- Los argumentos se evaluan al registrar, no al ejecutar
- Puede modificar named return values
- Orden LIFO
- Se ejecuta durante panic (pero no en `os.Exit`)

### Rust: Drop — Automatico y por scope

```rust
use std::fs::File;
use std::io::Read;

fn process_file(path: &str) -> Result<(), Box<dyn std::error::Error>> {
    let mut f = File::open(path)?;  // no necesitas defer ni close
    // File implementa Drop, se cierra automaticamente

    let mut data = Vec::new();
    f.read_to_end(&mut data)?;  // si falla, f se cierra (Drop)

    process(&data)?;

    Ok(())
    // f se cierra AQUI al salir del scope
    // No escribiste Close() en ningun lugar
}
```

**Caracteristicas:**
- **Automatico** — no puedes olvidar hacer cleanup
- Se ejecuta al salir del **scope** (bloque `{}`), no de la funcion
- El compilador inserta las llamadas a Drop
- No puedes controlar cuando se ejecuta (sin `drop()` explicito)
- Drop no puede retornar errores
- Orden: inverso al de declaracion (LIFO)
- Se ejecuta durante panic (en modo unwind)

### C: Manual — La responsabilidad total

```c
int process_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char *data = malloc(4096);
    if (!data) {
        fclose(f);      // ← tienes que recordar cerrar f
        return -1;
    }

    size_t n = fread(data, 1, 4096, f);
    if (ferror(f)) {
        free(data);     // ← tienes que recordar liberar data
        fclose(f);      // ← Y cerrar f
        return -1;
    }

    int result = process(data, n);

    free(data);         // cleanup manual
    fclose(f);          // cleanup manual
    return result;
}
// Cada nuevo return necesita TODOS los cleanups anteriores
// Es la fuente #1 de resource leaks en C
```

### Comparacion lado a lado: Multiples recursos

```go
// Go — defer en secuencia, LIFO automatico
func setup() error {
    db, err := openDB()
    if err != nil { return err }
    defer db.Close()              // cierra 3ro

    cache, err := openCache()
    if err != nil { return err }  // db.Close() via defer
    defer cache.Close()           // cierra 2do

    mq, err := openMQ()
    if err != nil { return err }  // cache.Close() + db.Close() via defer
    defer mq.Close()              // cierra 1ro

    return doWork(db, cache, mq)
    // Al retornar: mq.Close(), cache.Close(), db.Close() (LIFO)
}
```

```rust
// Rust — Drop automatico al salir del scope
fn setup() -> Result<(), Box<dyn Error>> {
    let db = open_db()?;           // si falla, nada que limpiar
    let cache = open_cache()?;     // si falla, db.drop() automatico
    let mq = open_mq()?;          // si falla, cache.drop() + db.drop()

    do_work(&db, &cache, &mq)?;

    Ok(())
    // Al salir: mq.drop(), cache.drop(), db.drop() (LIFO automatico)
    // No escribiste NINGUN cleanup — el compilador lo hizo todo
}
```

```c
// C — cleanup manual con goto (patron comun)
int setup(void) {
    DB *db = open_db();
    if (!db) return -1;

    Cache *cache = open_cache();
    if (!cache) goto close_db;

    MQ *mq = open_mq();
    if (!mq) goto close_cache;

    int result = do_work(db, cache, mq);

    close_mq(mq);
close_cache:
    close_cache(cache);
close_db:
    close_db(db);
    return result;
}
```

---

## 3. La diferencia de scope — Funcion vs Bloque

Esta es la diferencia mas importante entre `defer` y `Drop`:

```go
// Go — defer es por FUNCION
func goScope() {
    fmt.Println("1: start")
    {
        f, _ := os.Open("file.txt")
        defer f.Close()
        fmt.Println("2: inside block, file open")
    }
    fmt.Println("3: outside block, file STILL OPEN")  // ← f sigue abierto
    // f.Close() se ejecuta AQUI, al salir de goScope()
}
// Esto puede ser un problema si el bloque esta en un loop
```

```rust
// Rust — Drop es por SCOPE (bloque)
fn rust_scope() {
    println!("1: start");
    {
        let f = File::open("file.txt").unwrap();
        println!("2: inside block, file open");
    }  // ← f.drop() se ejecuta AQUI
    println!("3: outside block, file CLOSED");  // f ya no existe
    // El compilador ni siquiera te deja usar f aqui
}
```

### Implicacion practica: Loops

```go
// Go — PROBLEMA: defer se acumula en loops
func goLoop() error {
    for _, path := range paths {
        f, err := os.Open(path)
        if err != nil { return err }
        defer f.Close()  // ← se acumula: 1000 archivos = 1000 abiertos
        process(f)
    }
    return nil  // AQUI se cierran todos
}

// Go — SOLUCION: extraer a funcion para crear scope de defer
func goLoop() error {
    for _, path := range paths {
        if err := processOne(path); err != nil {
            return err
        }
    }
    return nil
}
func processOne(path string) error {
    f, err := os.Open(path)
    if err != nil { return err }
    defer f.Close()  // se ejecuta al salir de processOne
    return process(f)
}
```

```rust
// Rust — NO HAY PROBLEMA: Drop se ejecuta al final de cada iteracion
fn rust_loop() -> Result<(), Box<dyn Error>> {
    for path in &paths {
        let f = File::open(path)?;
        process(&f)?;
    }  // f.drop() se ejecuta al final de CADA iteracion
    // Solo un archivo abierto a la vez — automaticamente
    Ok(())
}
```

```c
// C — cleanup manual en cada iteracion
void c_loop(const char **paths, int n) {
    for (int i = 0; i < n; i++) {
        FILE *f = fopen(paths[i], "r");
        if (!f) continue;
        process(f);
        fclose(f);  // manual en cada iteracion
    }
}
```

### Que pasa si quieres Drop manual en Rust?

```rust
// Puedes forzar Drop con drop() o std::mem::drop()
fn manual_drop() {
    let f = File::open("big_file.txt").unwrap();
    let data = read_all(&f);
    drop(f);  // cierra AHORA, no al final del scope
    // f ya no es valida — el compilador no te deja usarla
    process_data(&data);  // data sigue valida
}
// En Go, el equivalente seria Close() manual en lugar de defer
```

---

## 4. defer vs Drop — Manejo de errores en cleanup

### Go: defer puede inspeccionar y modificar errores

```go
// Go puede capturar el error de Close() via named returns
func writeFile(path string, data []byte) (err error) {
    f, err := os.Create(path)
    if err != nil {
        return err
    }

    defer func() {
        closeErr := f.Close()
        if err == nil {
            err = closeErr  // modifica el return value
        }
    }()

    _, err = f.Write(data)
    return err
}
// El defer lee 'err' despues del return y decide si el error de Close
// es relevante. Esto es IMPOSIBLE con Drop en Rust.
```

### Rust: Drop no puede retornar errores

```rust
// Drop trait:
impl Drop for MyFile {
    fn drop(&mut self) {
        // self.close() puede fallar, pero Drop retorna ()
        // No hay forma de reportar el error
        let _ = self.inner.close();  // error silenciado
    }
}

// Para manejar el error de close, Rust te obliga a hacerlo explicito:
fn write_file(path: &str, data: &[u8]) -> Result<(), io::Error> {
    let mut f = File::create(path)?;
    f.write_all(data)?;
    f.flush()?;        // flush explicito
    f.sync_all()?;     // sync explicito
    // Drop se ejecuta al salir, pero si flush/sync fallaron,
    // ya retornaste el error

    // Para cerrar con manejo de error:
    drop(f);           // cierra, pero no puedes capturar el error
    // Alternativa: usar into_raw_fd() para tomar ownership del fd
    Ok(())
}
```

### C: Error de cleanup es tu responsabilidad total

```c
int write_file(const char *path, const char *data, size_t len) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;

    if (fwrite(data, 1, len, f) != len) {
        fclose(f);  // tambien podria fallar, pero lo ignoramos
        return -1;
    }

    if (fflush(f) != 0) {
        fclose(f);
        return -1;
    }

    // fclose puede fallar si hay datos sin escribir
    if (fclose(f) != 0) {
        return -1;
    }

    return 0;
}
```

### Resumen: Errores en cleanup

```
┌──────────────────────────────────────────────────────────────┐
│  Error de Close/Drop/cleanup:                                │
│                                                              │
│  Go:   defer puede inspeccionar err Y el error de Close      │
│        → puede combinar ambos errores                        │
│        → es flexible pero requiere discipline                │
│                                                              │
│  Rust: Drop() no puede retornar error                        │
│        → debes hacer flush/sync/close explicitamente          │
│        → Drop es "best effort" cleanup                       │
│        → mas seguro (no puedes ignorar errors de write)      │
│                                                              │
│  C:    Totalmente manual                                     │
│        → cada fclose necesita check                          │
│        → facil de olvidar                                    │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 5. defer vs Drop — Evaluacion de argumentos

### Go: Argumentos evaluados al registrar

```go
func goEval() {
    x := "hello"
    defer fmt.Println(x)  // captura "hello" AHORA

    x = "world"
    // Imprime: "hello" (el valor al momento del defer)
}
```

Esta semantica no tiene equivalente directo en Rust porque Drop no toma argumentos — opera sobre `&mut self`.

### Rust: Drop opera sobre el valor actual del objeto

```rust
struct Logger {
    name: String,
}

impl Drop for Logger {
    fn drop(&mut self) {
        println!("dropping: {}", self.name);  // siempre el valor actual
    }
}

fn rust_eval() {
    let mut logger = Logger { name: "hello".into() };
    logger.name = "world".into();
    // Drop imprime: "dropping: world" — el valor ACTUAL
}
// No hay forma de "capturar" el valor al momento de la creacion
// como lo hace defer con argumentos directos
```

### Go: Closure vs argumento directo

```go
func goBothWays() {
    name := "hello"

    // Argumento directo: captura el valor
    defer fmt.Println("arg:", name)    // imprime "hello"

    // Closure: captura la referencia (como Drop en Rust)
    defer func() {
        fmt.Println("closure:", name)  // imprime "world"
    }()

    name = "world"
}
// Go te da ambas opciones en una sola feature
// Rust solo tiene la semantica "valor actual" (Drop)
```

---

## 6. panic vs panic! — Errores irrecuperables

### Filosofia

```
┌──────────────────────────────────────────────────────────────┐
│  Go panic:                                                   │
│  ├─ Diseñado para ser RECUPERABLE (recover)                  │
│  ├─ Siempre hace unwinding del stack                         │
│  ├─ Ejecuta todos los defers                                 │
│  ├─ La stdlib lo usa internamente (encoding/json)            │
│  └─ Patron: panic interno + recover en boundary              │
│                                                              │
│  Rust panic!:                                                │
│  ├─ Diseñado para ser generalmente IRRECUPERABLE             │
│  ├─ Dos modos: unwind (default) o abort (configurable)       │
│  ├─ En modo abort: termina sin ejecutar Drop                 │
│  ├─ catch_unwind existe pero es "de ultimo recurso"          │
│  └─ Patron: panic! = bug, Result = error esperado            │
│                                                              │
│  C:                                                          │
│  ├─ abort(): termina sin cleanup                             │
│  ├─ assert(): abort con mensaje en debug                     │
│  ├─ exit(): termina sin stack cleanup                        │
│  └─ longjmp: "recover" manual, peligroso                     │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Comparacion de sintaxis

```go
// Go — panic acepta any
panic("message")
panic(fmt.Errorf("detail: %w", err))
panic(42)  // cualquier tipo, pero usa string o error
```

```rust
// Rust — panic! es un macro con format string
panic!("message");
panic!("detail: {}", err);
panic!("value: {value}");  // Rust 1.58+ — named params

// Macros relacionados:
unreachable!("should never reach here");  // panic con mensaje especial
todo!("not implemented yet");              // panic para codigo WIP
unimplemented!("feature X");              // panic para features no implementadas
```

```c
// C — no tiene panic, pero tiene:
assert(condition);                  // abort si es false (solo en debug)
fprintf(stderr, "fatal: ...\n");
abort();                            // terminacion inmediata
exit(1);                            // terminacion con exit handlers (atexit)
```

### Panics implicitos del runtime

```go
// Go — panics del runtime
var s []int
_ = s[5]              // panic: index out of range
var p *int
_ = *p                // panic: nil pointer dereference
var m map[string]int
m["k"] = 1            // panic: assignment to entry in nil map
```

```rust
// Rust — panics del runtime
let v = vec![1, 2, 3];
let _ = v[5];         // panic: index out of bounds
// Alternativa segura: v.get(5) retorna Option<&T>

// Rust PREVIENE muchos panics en COMPILE TIME:
let p: &i32;          // ERROR de compilacion: use of uninitialized
// No existe nil pointer dereference en safe Rust
// No existen null references en safe Rust

// unwrap() en None/Err
let x: Option<i32> = None;
x.unwrap();           // panic: called `Option::unwrap()` on a `None` value

let r: Result<i32, &str> = Err("fail");
r.unwrap();           // panic: called `Result::unwrap()` on an `Err` value
```

```c
// C — no tiene panics, tiene comportamiento indefinido
int arr[3] = {1, 2, 3};
printf("%d\n", arr[5]);    // UB: podria imprimir basura, segfault, o nada

int *p = NULL;
printf("%d\n", *p);        // UB: segfault en la mayoria de plataformas

// C no tiene null safety ni bounds checking
// El programa puede corromperse silenciosamente
```

### Tabla de panics implicitos

```
┌──────────────────────────────────────────────────────────────┐
│  Situacion              │ Go          │ Rust         │ C     │
│─────────────────────────┼─────────────┼──────────────┼───────│
│  Index out of range     │ panic       │ panic!       │ UB    │
│  Null/nil dereference   │ panic       │ IMPOSIBLE*   │ UB    │
│  Nil map write          │ panic       │ IMPOSIBLE**  │ N/A   │
│  Integer overflow       │ Wraps***    │ panic (debug)│ UB    │
│  Division by zero (int) │ panic       │ panic!       │ UB    │
│  Division by zero (flt) │ +Inf        │ NaN/Inf      │ UB    │
│  Buffer overflow        │ panic       │ IMPOSIBLE*   │ UB    │
│  Use after free         │ GC previene │ IMPOSIBLE*   │ UB    │
│  Data race              │ Fatal****   │ IMPOSIBLE*   │ UB    │
│  Double close channel   │ panic       │ IMPOSIBLE*   │ N/A   │
│─────────────────────────┼─────────────┼──────────────┼───────│
│  * en safe Rust (compilador previene)                        │
│  ** HashMap se inicializa con HashMap::new(), no puede ser nil│
│  *** Go wraps silenciosamente; usa math/bits para detectar   │
│  **** concurrent map access es fatal, no recuperable         │
└──────────────────────────────────────────────────────────────┘
```

---

## 7. recover vs catch_unwind — Contencion de fallos

### Go: recover — Integrado en el flujo normal

```go
// recover es un builtin, parte del lenguaje
// Funciona SOLO dentro de un defer
// Es comun y aceptado en servidores

func safeHandler(w http.ResponseWriter, r *http.Request) {
    defer func() {
        if r := recover(); r != nil {
            log.Printf("panic in handler: %v", r)
            http.Error(w, "Internal Server Error", 500)
        }
    }()
    // ... handler logic que podria hacer panic ...
}
```

### Rust: catch_unwind — De ultimo recurso

```rust
use std::panic;

// catch_unwind es una funcion de std::panic
// Es explicitamente documentado como "no es try/catch"
// Solo se usa en boundaries (FFI, thread pools)

fn safe_handler() -> Result<String, String> {
    let result = panic::catch_unwind(|| {
        // ... logica que podria hacer panic! ...
        "success".to_string()
    });

    match result {
        Ok(value) => Ok(value),
        Err(e) => {
            // e es Box<dyn Any + Send>
            let msg = if let Some(s) = e.downcast_ref::<&str>() {
                s.to_string()
            } else if let Some(s) = e.downcast_ref::<String>() {
                s.clone()
            } else {
                "unknown panic".to_string()
            };
            Err(format!("panic caught: {}", msg))
        }
    }
}
```

### Diferencias clave

```
┌──────────────────────────────────────────────────────────────┐
│  Go recover()              │  Rust catch_unwind()            │
│────────────────────────────┼─────────────────────────────────│
│  Builtin del lenguaje      │  Funcion de la stdlib           │
│  Solo en defer             │  Toma un closure FnUnwindSafe   │
│  Retorna interface{}       │  Retorna Result<T, Box<Any>>    │
│  Comun en servidores       │  Raro (solo FFI/thread pools)   │
│  Siempre funciona          │  No funciona con panic="abort"  │
│  Scope: goroutine actual   │  Scope: closure + thread actual │
│  El programa continua      │  El programa continua (si unwind)│
│  Patron aceptado           │  Considerado code smell         │
│  15+ usos en net/http      │  ~5 usos en la stdlib           │
│────────────────────────────┼─────────────────────────────────│
│  recover = "me preparo     │  catch_unwind = "esto no deberia│
│  para manejar fallos"      │  pasar, pero si pasa, no muero" │
└──────────────────────────────────────────────────────────────┘
```

### UnwindSafe en Rust — Un concepto que Go no tiene

```rust
use std::panic::UnwindSafe;

// Rust requiere que el closure en catch_unwind sea UnwindSafe
// Esto previene observar estado corrupto despues de un panic

// ✗ No compila sin AssertUnwindSafe
fn bad_example() {
    let mut data = vec![1, 2, 3];
    let result = panic::catch_unwind(|| {
        data.push(4);  // ERROR: &mut Vec no es UnwindSafe
        data.push(5);  // si panic! ocurre aqui, data esta a medias
    });
    // data podria tener 4 elementos o 5 — estado inconsistente
}

// ✓ AssertUnwindSafe: "se que lo que hago es seguro"
fn ok_example() {
    let mut data = vec![1, 2, 3];
    let result = panic::catch_unwind(panic::AssertUnwindSafe(|| {
        data.push(4);
        data.push(5);
    }));
    // Funciona, pero el programador es responsable de verificar el estado
}
```

```go
// Go no tiene este concepto — recover siempre funciona
// El programador es responsable de verificar el estado
func goExample() {
    data := []int{1, 2, 3}
    defer func() {
        if r := recover(); r != nil {
            // data podria estar en estado inconsistente
            // Go confia en que el programador lo sabe
            log.Println("recovered, data might be inconsistent:", data)
        }
    }()
    data = append(data, 4)
    panic("oops")
    data = append(data, 5)  // nunca se ejecuta
}
// Despues del recover, data = [1, 2, 3, 4] — parcialmente actualizado
```

---

## 8. Error handling — error vs Result<T, E>

Aunque no es directamente sobre defer/panic/recover, el manejo de errores es inseparable de esta discusion:

### Go: error como valor de retorno

```go
// Go: error es un valor que PUEDES ignorar (el compilador no te obliga)
func readFile(path string) ([]byte, error) {
    return os.ReadFile(path)
}

// Uso correcto:
data, err := readFile("config.yaml")
if err != nil {
    return fmt.Errorf("read config: %w", err)
}

// Uso incorrecto (compila sin warning):
data, _ := readFile("config.yaml")  // error ignorado

// Peor aun (compila sin warning):
data, err := readFile("config.yaml")
// ... 50 lineas despues, err nunca se chequeo ...
```

### Rust: Result como tipo obligatorio

```rust
// Rust: Result OBLIGA a manejar el error (o el programa no compila)
fn read_file(path: &str) -> Result<Vec<u8>, io::Error> {
    std::fs::read(path)
}

// Uso con ? (propagacion automatica):
fn process() -> Result<(), Box<dyn Error>> {
    let data = read_file("config.yaml")?;  // error se propaga
    // si read_file falla, process retorna Err inmediatamente
    // no puedes olvidar manejar el error
    Ok(())
}

// Uso con match (manejo explicito):
fn process() {
    match read_file("config.yaml") {
        Ok(data) => { /* usar data */ }
        Err(e) => { eprintln!("error: {}", e); }
    }
}

// Ignorar explicitamente (posible pero obvio):
let _ = read_file("config.yaml");  // ignorado a proposito
// El compilador daria warning sin el "let _"

// ¿Que pasa si NO manejas el Result? WARNING del compilador:
read_file("config.yaml");
// warning: unused `Result` that must be used
```

### C: Return codes y errno

```c
// C: el error es un int que es MUY facil de ignorar
FILE *f = fopen("config.yaml", "r");
// fopen retorna NULL en error, pero nada te obliga a chequearlo
// si no chequeas y usas f:
char buf[100];
fread(buf, 1, 100, f);  // segfault si f es NULL

// Patron correcto:
FILE *f = fopen("config.yaml", "r");
if (!f) {
    perror("fopen");
    return -1;
}
```

### El operador ? de Rust vs if err != nil de Go

```go
// Go — if err != nil se repite constantemente
func deployService(ctx context.Context, name string) error {
    config, err := loadConfig(name)
    if err != nil {
        return fmt.Errorf("load config: %w", err)
    }

    artifact, err := buildArtifact(config)
    if err != nil {
        return fmt.Errorf("build: %w", err)
    }

    err = uploadArtifact(artifact)
    if err != nil {
        return fmt.Errorf("upload: %w", err)
    }

    err = activateService(ctx, name, artifact)
    if err != nil {
        return fmt.Errorf("activate: %w", err)
    }

    return nil
}
// 4 operaciones = 4 bloques de error handling
// Es verbose pero cada error tiene contexto
```

```rust
// Rust — ? propaga el error automaticamente
fn deploy_service(ctx: &Context, name: &str) -> Result<(), DeployError> {
    let config = load_config(name)?;
    let artifact = build_artifact(&config)?;
    upload_artifact(&artifact)?;
    activate_service(ctx, name, &artifact)?;
    Ok(())
}
// 4 operaciones = 4 lineas
// Menos verbose pero menos contexto por defecto
// Para agregar contexto: usa .map_err() o crates como anyhow/thiserror

// Con contexto (equivalente al Go):
fn deploy_service(ctx: &Context, name: &str) -> Result<(), DeployError> {
    let config = load_config(name)
        .map_err(|e| DeployError::Config(e))?;
    let artifact = build_artifact(&config)
        .map_err(|e| DeployError::Build(e))?;
    upload_artifact(&artifact)
        .map_err(|e| DeployError::Upload(e))?;
    activate_service(ctx, name, &artifact)
        .map_err(|e| DeployError::Activate(e))?;
    Ok(())
}

// Con anyhow (crate popular):
use anyhow::{Context, Result};

fn deploy_service(ctx: &Context, name: &str) -> Result<()> {
    let config = load_config(name).context("load config")?;
    let artifact = build_artifact(&config).context("build")?;
    upload_artifact(&artifact).context("upload")?;
    activate_service(ctx, name, &artifact).context("activate")?;
    Ok(())
}
```

### El debate: verbosidad vs seguridad

```
┌──────────────────────────────────────────────────────────────┐
│  Go "if err != nil":                                         │
│  ├─ Pro: explicito, cada error tiene contexto custom         │
│  ├─ Pro: el flow es lineal, facil de debuggear               │
│  ├─ Con: verbose (~40% del codigo es error handling)         │
│  ├─ Con: facil olvidar un check (el compilador no obliga)    │
│  └─ Con: no hay ? operator (propuesta rechazada multiples    │
│          veces por el equipo de Go)                           │
│                                                              │
│  Rust "Result + ?":                                          │
│  ├─ Pro: IMPOSIBLE ignorar un error sin ser explicito        │
│  ├─ Pro: ? hace el codigo compacto                           │
│  ├─ Pro: el sistema de tipos previene bugs de error handling │
│  ├─ Con: ? sin contexto pierde informacion                   │
│  ├─ Con: requires learning From trait, thiserror, anyhow     │
│  └─ Con: error types can get complex (enum variants)         │
│                                                              │
│  C return codes:                                             │
│  ├─ Pro: simple y rapido                                     │
│  ├─ Con: MUY facil ignorar                                   │
│  ├─ Con: errno es global (no thread-safe en implementaciones │
│          viejas)                                              │
│  └─ Con: no hay wrapping ni contexto nativo                  │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 9. Unwinding del stack — Go vs Rust

### Go: Siempre unwind

```go
// Go SIEMPRE hace unwinding durante panic
// No hay forma de configurar "abort on panic"
// (excepto GOTRACEBACK=crash para coredumps)

func main() {
    defer fmt.Println("main defer")  // SIEMPRE se ejecuta

    func() {
        defer fmt.Println("inner defer")  // SIEMPRE se ejecuta
        panic("boom")
    }()
}
// Output (antes del stack trace):
// inner defer
// main defer
```

### Rust: Configurable — unwind o abort

```rust
// Rust tiene DOS modos de panic:

// ── Modo 1: unwind (default en debug) ────────────────
// Similar a Go: ejecuta Drop, permite catch_unwind
fn main() {
    let _guard = Guard;  // impl Drop
    panic!("boom");
    // Guard::drop() SE EJECUTA
}

// ── Modo 2: abort (comun en release/embedded) ────────
// Cargo.toml:
// [profile.release]
// panic = "abort"
//
// Ventajas de abort:
// - Binario mas pequeño (no incluye unwinding tables)
// - Mas rapido (no hay overhead de unwinding)
// - Mas predecible (termina inmediatamente)
//
// Desventajas:
// - Drop NO se ejecuta — resource leaks posibles
// - catch_unwind no funciona
// - No hay recovery posible

// Esto es util para:
// - Embedded systems (sin heap, sin unwinding)
// - Microservicios que se reinician via orchestrator (k8s)
// - Cualquier contexto donde "crash and restart" es la estrategia
```

### C: abort es lo unico que hay

```c
// C no tiene unwinding nativo
// abort() termina inmediatamente sin ejecutar atexit handlers
// exit() ejecuta atexit handlers pero no hay "unwinding" de stack

#include <stdlib.h>

void cleanup(void) {
    printf("atexit cleanup\n");
}

int main() {
    atexit(cleanup);

    abort();   // cleanup NO se ejecuta
    // vs
    exit(0);   // cleanup SI se ejecuta
}
```

### Implicacion para SysAdmin: Containers y Kubernetes

```
┌──────────────────────────────────────────────────────────────┐
│  En contexto de containers/k8s:                              │
│                                                              │
│  Go: panic + recover en el servidor                          │
│  ├─ La request falla, el servidor sigue                      │
│  ├─ El Pod NO se reinicia por un panic recuperado            │
│  └─ Ideal para servidores de larga vida                      │
│                                                              │
│  Rust (panic=unwind): similar a Go                           │
│  ├─ catch_unwind en el thread pool                           │
│  ├─ La request falla, el servidor sigue                      │
│  └─ Pero es menos idiomatico que en Go                       │
│                                                              │
│  Rust (panic=abort): crash and restart                       │
│  ├─ El Pod crashea con exit code != 0                        │
│  ├─ Kubernetes reinicia el Pod                               │
│  ├─ Mas simple pero mas latencia en el restart               │
│  └─ Ideal para workers stateless con restartPolicy: Always   │
│                                                              │
│  C: crash and restart (similar a Rust abort)                 │
│  ├─ segfault → SIGSEGV → container exit code 139             │
│  ├─ Kubernetes reinicia                                      │
│  └─ Comun en servicios legacy                                │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 10. Patrones equivalentes — Tabla de traduccion

### cleanup basico

```go
// Go
f, err := os.Open(path)
if err != nil { return err }
defer f.Close()
```

```rust
// Rust
let f = File::open(path)?;
// Drop automatico, no escribes nada
```

```c
// C
FILE *f = fopen(path, "r");
if (!f) return -1;
// ... usar f ...
fclose(f);  // manual al final
```

### Lock/Unlock

```go
// Go
mu.Lock()
defer mu.Unlock()
// ... seccion critica ...
```

```rust
// Rust — MutexGuard implementa Drop
let guard = mutex.lock().unwrap();
// ... seccion critica con *guard ...
// guard.drop() al salir del scope = unlock automatico

// Nota: en Rust no puedes olvidar hacer unlock
// ni puedes hacer unlock sin haber hecho lock
// El sistema de tipos lo garantiza
```

```c
// C
pthread_mutex_lock(&mutex);
// ... seccion critica ...
pthread_mutex_unlock(&mutex);  // facil de olvidar
```

### Timer/Medicion

```go
// Go
func timed(name string) func() {
    start := time.Now()
    return func() {
        log.Printf("[%s] %s", name, time.Since(start))
    }
}
defer timed("deploy")()
```

```rust
// Rust — struct con Drop
struct Timer {
    name: String,
    start: std::time::Instant,
}

impl Timer {
    fn new(name: &str) -> Self {
        Timer {
            name: name.to_string(),
            start: std::time::Instant::now(),
        }
    }
}

impl Drop for Timer {
    fn drop(&mut self) {
        println!("[{}] {:?}", self.name, self.start.elapsed());
    }
}

let _timer = Timer::new("deploy");
// se imprime al salir del scope
```

### Must pattern (panic en inicializacion)

```go
// Go
var pattern = regexp.MustCompile(`\d+`)
```

```rust
// Rust — unwrap() en lazy_static o once_cell
use once_cell::sync::Lazy;
use regex::Regex;

static PATTERN: Lazy<Regex> = Lazy::new(|| {
    Regex::new(r"\d+").unwrap()  // panic! si el regex es invalido
});

// Rust 1.80+: std::sync::LazyLock (sin crate externo)
use std::sync::LazyLock;
static PATTERN: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r"\d+").unwrap()
});
```

```c
// C — no hay equivalente limpio
// Compilar el regex en runtime con regcomp()
regex_t pattern;
if (regcomp(&pattern, "\\d+", REG_EXTENDED) != 0) {
    abort();  // no puede continuar
}
// No hay forma de hacerlo en compile time sin macros complejos
```

### Transaction/Rollback

```go
// Go
tx, err := db.Begin()
if err != nil { return err }
defer tx.Rollback()  // safe: no-op after Commit

// ... operaciones ...

return tx.Commit()
```

```rust
// Rust — patron con Guard/Drop
struct TxGuard<'a> {
    tx: Option<Transaction<'a>>,
    committed: bool,
}

impl<'a> TxGuard<'a> {
    fn new(tx: Transaction<'a>) -> Self {
        TxGuard { tx: Some(tx), committed: false }
    }

    fn commit(mut self) -> Result<(), Error> {
        self.committed = true;
        self.tx.take().unwrap().commit()
    }
}

impl<'a> Drop for TxGuard<'a> {
    fn drop(&mut self) {
        if !self.committed {
            if let Some(tx) = self.tx.take() {
                let _ = tx.rollback();  // best effort
            }
        }
    }
}

// Uso:
let tx = db.begin()?;
let guard = TxGuard::new(tx);
// ... operaciones con guard.tx ...
guard.commit()?;  // si no se llama, Drop hace rollback
```

---

## 11. Ventajas unicas de cada enfoque

### Cosas que Go puede hacer y Rust no

```go
// 1. Modificar return values desde defer
func enrichError() (err error) {
    defer func() {
        if err != nil {
            err = fmt.Errorf("enriched: %w", err)
        }
    }()
    return doWork()
}
// Rust Drop no puede modificar el return value de la funcion

// 2. Logica condicional en defer basada en el resultado
func conditionalCleanup() (err error) {
    tmpFile := createTempFile()
    defer func() {
        if err != nil {
            os.Remove(tmpFile)  // solo limpia si hubo error
        }
        // Si todo salio bien, el tmpFile se mantiene
    }()
    return processWithTempFile(tmpFile)
}
// En Rust, necesitas un Guard struct custom con un flag

// 3. Capturar el valor de un argumento en un momento especifico
func snapshot() {
    state := getState()
    defer reportState(state)  // captura el estado ANTES de modificarlo
    modifyState()
}
// En Rust, necesitas clonar el estado manualmente

// 4. recover() como control flow en boundaries
func safeCall(fn func()) (err error) {
    defer func() {
        if r := recover(); r != nil {
            err = fmt.Errorf("panic: %v", r)
        }
    }()
    fn()
    return nil
}
// catch_unwind en Rust es mas limitado (UnwindSafe requirement)
```

### Cosas que Rust puede hacer y Go no

```rust
// 1. Cleanup automatico por scope (no solo por funcion)
fn rust_scoped() {
    {
        let conn = TcpStream::connect("localhost:80").unwrap();
        // ... usar conn ...
    }  // conn se cierra AQUI
    // En Go, defer Close() esperaria hasta el final de la funcion

    // Esto es especialmente importante en loops:
    for path in &paths {
        let f = File::open(path).unwrap();
        // f se cierra al final de CADA iteracion — automaticamente
    }

    // 2. IMPOSIBLE olvidar hacer cleanup
    // El compilador inserta Drop automaticamente
    // No puedes tener un resource leak por olvidar defer

    // 3. Ownership previene uso despues de liberacion
    let f = File::open("file.txt").unwrap();
    drop(f);
    // f.read(...)  // ERROR de compilacion: use of moved value

    // En Go:
    // f, _ := os.Open("file.txt")
    // f.Close()
    // f.Read(...)  // compila! runtime error o comportamiento indefinido

    // 4. Drop order es determinista y verificable
    // El compilador garantiza LIFO basado en el orden de declaracion
    // No hay "olvide poner defer en el orden correcto"

    // 5. Zero-cost abstractions — Drop no tiene overhead
    // defer tiene ~0-6ns; Drop tiene 0ns (compilado inline)
}
```

### Cosas que C puede hacer y Go/Rust no

```c
// 1. Control total sobre el lifetime
// No hay GC (Go) ni borrow checker (Rust) que limite
char *buf = malloc(1024);
// buf puede vivir EXACTAMENTE cuanto quieras
// Puedes pasarlo entre funciones, threads, procesos
// (con todos los riesgos que eso implica)

// 2. longjmp entre stacks (peligroso pero posible)
// Go y Rust no permiten saltar entre funciones de esta manera

// 3. No-overhead para cleanup condicional
// Solo llamas free/close cuando lo necesitas
// No hay registro de defer ni destructor
```

---

## 12. Patrones de error handling: Comparacion detallada

### Wrapping de errores

```go
// Go — fmt.Errorf con %w
func goWrap() error {
    data, err := os.ReadFile("config.yaml")
    if err != nil {
        return fmt.Errorf("load config: %w", err)
    }

    var cfg Config
    if err := json.Unmarshal(data, &cfg); err != nil {
        return fmt.Errorf("parse config: %w", err)
    }

    return nil
}

// Inspeccionar cadena de errores:
if errors.Is(err, os.ErrNotExist) { /* ... */ }
var pathErr *os.PathError
if errors.As(err, &pathErr) { /* ... */ }
```

```rust
// Rust — From trait o thiserror crate
use thiserror::Error;

#[derive(Error, Debug)]
enum ConfigError {
    #[error("load config: {0}")]
    Io(#[from] io::Error),

    #[error("parse config: {0}")]
    Parse(#[from] serde_json::Error),
}

fn rust_wrap() -> Result<Config, ConfigError> {
    let data = std::fs::read("config.yaml")?;  // From<io::Error> automatico
    let cfg: Config = serde_json::from_slice(&data)?;  // From<serde_json::Error>
    Ok(cfg)
}

// Inspeccionar:
match err {
    ConfigError::Io(io_err) => { /* ... */ }
    ConfigError::Parse(parse_err) => { /* ... */ }
}

// O con anyhow para prototyping rapido:
use anyhow::{Context, Result};
fn rust_wrap() -> Result<Config> {
    let data = std::fs::read("config.yaml")
        .context("load config")?;
    let cfg: Config = serde_json::from_slice(&data)
        .context("parse config")?;
    Ok(cfg)
}
```

```c
// C — no hay wrapping nativo
// Patron comun: errno + log manual
int c_wrap(void) {
    FILE *f = fopen("config.yaml", "r");
    if (!f) {
        fprintf(stderr, "load config: %s\n", strerror(errno));
        return -1;
    }
    // ... parsear manualmente ...
    return 0;
}
```

---

## 13. Cuando elegir Go vs Rust para SysAdmin/DevOps

```
┌──────────────────────────────────────────────────────────────┐
│  ELIGE GO CUANDO:                                            │
│  ├─ Necesitas prototipado rapido de herramientas CLI         │
│  ├─ El equipo tiene experiencia mixta (Go es facil de leer)  │
│  ├─ Necesitas cross-compilation trivial (GOOS/GOARCH)        │
│  ├─ El servicio es I/O bound (HTTP server, API gateway)      │
│  ├─ Quieres un solo binario estatico para deploy facil       │
│  ├─ La stdlib es suficiente (net/http, encoding/json, os)    │
│  └─ El tiempo de compilacion importa (Go: segundos)          │
│                                                              │
│  ELIGE RUST CUANDO:                                          │
│  ├─ Necesitas rendimiento maximo (CPU bound, parsing, proxy) │
│  ├─ La memoria es critica (embedded, edge computing)         │
│  ├─ La seguridad de tipos es prioridad (fintech, healthcare) │
│  ├─ Necesitas garantias de ausencia de data races            │
│  ├─ El proyecto es de larga vida (el compilador previene     │
│  │   bugs que aparecerian meses despues en Go)               │
│  ├─ Necesitas WASM (WebAssembly) — Rust tiene mejor soporte │
│  └─ Quieres zero-cost abstractions                           │
│                                                              │
│  HERRAMIENTAS DEVOPS POPULARES:                              │
│                                                              │
│  Go:  Docker, Kubernetes, Terraform, Prometheus, Grafana,    │
│       CockroachDB, Consul, Vault, Traefik, Caddy, Hugo      │
│                                                              │
│  Rust: ripgrep, fd, bat, exa/eza, delta, zoxide, starship,  │
│        bottom, Firecracker, Vector, tikv, nushell            │
│                                                              │
│  Patron: Go domina en ORQUESTACION (servidores, APIs)        │
│          Rust domina en HERRAMIENTAS CLI y BAJO NIVEL         │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 14. Ejemplo comparativo completo: HTTP health checker

### Version Go

```go
package main

import (
    "context"
    "encoding/json"
    "fmt"
    "log"
    "net/http"
    "os"
    "sync"
    "time"
)

type CheckResult struct {
    URL      string        `json:"url"`
    Status   int           `json:"status"`
    Healthy  bool          `json:"healthy"`
    Duration time.Duration `json:"duration_ms"`
    Error    string        `json:"error,omitempty"`
}

func checkURL(ctx context.Context, client *http.Client, url string) CheckResult {
    start := time.Now()
    result := CheckResult{URL: url}

    // defer para siempre registrar la duracion
    defer func() {
        result.Duration = time.Since(start)
    }()

    // recover para manejar panics inesperados
    defer func() {
        if r := recover(); r != nil {
            result.Error = fmt.Sprintf("panic: %v", r)
            result.Healthy = false
        }
    }()

    req, err := http.NewRequestWithContext(ctx, "GET", url, nil)
    if err != nil {
        result.Error = err.Error()
        return result
    }

    resp, err := client.Do(req)
    if err != nil {
        result.Error = err.Error()
        return result
    }
    defer resp.Body.Close()

    result.Status = resp.StatusCode
    result.Healthy = resp.StatusCode >= 200 && resp.StatusCode < 400

    return result
}

func main() {
    urls := []string{
        "https://httpbin.org/status/200",
        "https://httpbin.org/status/500",
        "https://httpbin.org/delay/10",
        "https://invalid.example.com",
    }

    client := &http.Client{Timeout: 5 * time.Second}
    ctx := context.Background()

    var wg sync.WaitGroup
    results := make([]CheckResult, len(urls))

    for i, url := range urls {
        wg.Add(1)
        go func(idx int, u string) {
            defer wg.Done()
            results[idx] = checkURL(ctx, client, u)
        }(i, url)
    }
    wg.Wait()

    enc := json.NewEncoder(os.Stdout)
    enc.SetIndent("", "  ")
    enc.Encode(results)
}
```

### Version Rust equivalente

```rust
use reqwest::Client;
use serde::Serialize;
use std::time::{Duration, Instant};
use tokio;

#[derive(Serialize)]
struct CheckResult {
    url: String,
    status: Option<u16>,
    healthy: bool,
    duration_ms: u128,
    error: Option<String>,
}

async fn check_url(client: &Client, url: &str) -> CheckResult {
    let start = Instant::now();  // no hay defer — se usa al final

    let mut result = CheckResult {
        url: url.to_string(),
        status: None,
        healthy: false,
        duration_ms: 0,
        error: None,
    };

    // Rust no necesita recover — no hay panics inesperados en safe code
    // El error handling es via Result
    match client.get(url).send().await {
        Ok(resp) => {
            let status = resp.status().as_u16();
            result.status = Some(status);
            result.healthy = status >= 200 && status < 400;
            // resp.Body se cierra automaticamente via Drop
        }
        Err(e) => {
            result.error = Some(e.to_string());
        }
    }

    result.duration_ms = start.elapsed().as_millis();
    // No hay defer — simplemente asignamos al final
    // En Rust, esto es mas explicito pero igualmente seguro

    result
}

#[tokio::main]
async fn main() {
    let urls = vec![
        "https://httpbin.org/status/200",
        "https://httpbin.org/status/500",
        "https://httpbin.org/delay/10",
        "https://invalid.example.com",
    ];

    let client = Client::builder()
        .timeout(Duration::from_secs(5))
        .build()
        .unwrap();

    // Equivalente de WaitGroup: tokio::join! o futures::join_all
    let tasks: Vec<_> = urls.iter()
        .map(|url| {
            let client = client.clone();
            let url = url.to_string();
            tokio::spawn(async move {
                check_url(&client, &url).await
            })
        })
        .collect();

    let mut results = Vec::new();
    for task in tasks {
        results.push(task.await.unwrap());
        // unwrap aqui porque JoinError = panic en la task
        // En Go, esto seria capturado por el recover
    }

    println!("{}", serde_json::to_string_pretty(&results).unwrap());
}

// Dependencias (Cargo.toml):
// [dependencies]
// reqwest = { version = "0.12", features = ["json"] }
// tokio = { version = "1", features = ["full"] }
// serde = { version = "1", features = ["derive"] }
// serde_json = "1"
```

### Observaciones de la comparacion

```
┌──────────────────────────────────────────────────────────────┐
│  Go:                                                         │
│  ├─ defer resp.Body.Close() — explicito                      │
│  ├─ defer + recover para panics — boundary de seguridad      │
│  ├─ WaitGroup para concurrencia — familiar de threads        │
│  ├─ Sin dependencias externas — stdlib suficiente            │
│  ├─ goroutines son baratas (~8KB cada una)                   │
│  └─ Compila en ~1 segundo                                    │
│                                                              │
│  Rust:                                                       │
│  ├─ Drop cierra resp automaticamente — imposible olvidar     │
│  ├─ No necesita recover — Result maneja todo                 │
│  ├─ tokio::spawn para concurrencia — async/await             │
│  ├─ 4 dependencias externas (reqwest, tokio, serde, serde_json) │
│  ├─ Cada task es un future liviano                           │
│  └─ Compila en ~30-60 segundos (primera vez)                 │
│                                                              │
│  Lineas de codigo: Go ~65, Rust ~75                          │
│  Pero Rust tiene mas garantias en compile time               │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 15. Tabla resumen final

```
┌────────────────────┬─────────────────────┬──────────────────────┬──────────────────┐
│ Concepto           │ Go                  │ Rust                 │ C                │
├────────────────────┼─────────────────────┼──────────────────────┼──────────────────┤
│ Cleanup            │ defer f.Close()     │ Drop (automatico)    │ fclose(f) manual │
│ Scope de cleanup   │ Funcion             │ Bloque { }           │ Manual           │
│ Olvido de cleanup  │ Posible (sin defer) │ Imposible (Drop)     │ Comun            │
│ Orden de cleanup   │ LIFO (explicito)    │ LIFO (automatico)    │ Manual           │
│ Error en cleanup   │ defer + named return│ Drop no puede err    │ Manual           │
│ Crash explicito    │ panic("msg")        │ panic!("msg")        │ abort()          │
│ Crash implicito    │ Runtime panics      │ unwrap(), index OOB  │ UB / segfault    │
│ Capturar crash     │ recover() en defer  │ catch_unwind()       │ longjmp (raro)   │
│ Error esperado     │ return error        │ Result<T, E>         │ Return code      │
│ Ignorar error      │ Facil (_, err)      │ Dificil (warning)    │ Muy facil        │
│ Propagar error     │ if err != nil       │ ? operator           │ Manual           │
│ Envolver error     │ fmt.Errorf + %w     │ From / thiserror     │ No nativo        │
│ Null safety        │ nil existe (panic)  │ Option<T> (no null)  │ NULL (UB)        │
│ Data race          │ Fatal (runtime)     │ Imposible (compile)  │ UB               │
│ Memoria            │ GC                  │ Ownership (compile)  │ Manual           │
│ Compilacion        │ Segundos            │ Minutos (1ra vez)    │ Segundos         │
│ Binario            │ ~10MB (estatico)    │ ~2-5MB (estatico)    │ ~50KB-1MB        │
│ Paradigma cleanup  │ "Se explicito"      │ "Se automatico"      │ "Se responsable" │
└────────────────────┴─────────────────────┴──────────────────────┴──────────────────┘
```

---

## 16. Errores comunes al migrar entre Go y Rust

| # | Error | Contexto | Solucion |
|---|---|---|---|
| 1 | Esperar scope de bloque de defer | Viniendo de Rust a Go | defer es por funcion; extraer a funcion para scope |
| 2 | Olvidar defer (viniendo de Rust) | Rust no necesita "defer", Go si | Siempre `if err != nil { return } ; defer Close()` |
| 3 | Usar panic como Result | Viniendo de lenguajes con excepciones | panic = bug, error = resultado esperado |
| 4 | Usar unwrap() en produccion | Viniendo de Go a Rust | unwrap() = panic!; usar ? o match |
| 5 | Ignorar error de Close en Go | Viniendo de Rust donde Drop no falla | Para escritura, capturar error de Close |
| 6 | Esperar catch_unwind como recover | Viniendo de Go a Rust | catch_unwind es limitado (UnwindSafe, abort mode) |
| 7 | No poner recover en goroutines | Rust threads propagan panic al join | Cada goroutine necesita su propio recover |
| 8 | Asumir que Drop siempre corre | En Rust con panic=abort | Drop NO corre en abort mode ni con mem::forget |
| 9 | No wrappear errores en Go | Rust From/? hace wrapping automatico | Siempre `fmt.Errorf("contexto: %w", err)` |
| 10 | Overuse de recover | Go permite recover facil | recover solo en boundaries (server, worker pool) |

---

## 17. Ejercicios

### Ejercicio 1: Traduccion Go → Rust mental
```go
func processFile(path string) (err error) {
    f, err := os.Open(path)
    if err != nil {
        return fmt.Errorf("open: %w", err)
    }
    defer f.Close()

    data, err := io.ReadAll(f)
    if err != nil {
        return fmt.Errorf("read: %w", err)
    }

    if len(data) == 0 {
        return errors.New("file is empty")
    }

    return process(data)
}
```
**Tarea**: Reescribe esta funcion mentalmente en Rust. ¿Donde usarias `?`? ¿Necesitas defer/Drop? ¿Como manejas el error de archivo vacio?

### Ejercicio 2: Traduccion Rust → Go mental
```rust
fn read_config(path: &str) -> Result<Config, ConfigError> {
    let data = std::fs::read(path)?;
    let config: Config = toml::from_slice(&data)?;

    if config.port == 0 {
        return Err(ConfigError::Validation("port cannot be 0".into()));
    }

    Ok(config)
}
```
**Tarea**: Reescribe en Go. ¿Cuantos `if err != nil` necesitas? ¿Necesitas defer?

### Ejercicio 3: Prediccion de scope
```go
// Go version
func goVersion() {
    for i := 0; i < 3; i++ {
        f, _ := os.Open(fmt.Sprintf("file%d.txt", i))
        defer f.Close()
    }
    fmt.Println("all files still open here")
}
```
**Prediccion**: ¿Cuantos archivos estan abiertos cuando se imprime el mensaje? ¿Cuando se cierran? ¿Como lo reescribirias en Rust y que cambiaria?

### Ejercicio 4: Panic boundaries comparados
Dado un servidor HTTP que procesa requests que pueden hacer panic:
1. Escribe la solucion en Go usando defer+recover
2. Escribe la solucion conceptual en Rust usando catch_unwind
3. Explica por que en Rust probablemente NO usarias catch_unwind y que harias en su lugar

### Ejercicio 5: Resource cleanup chain
Implementa en Go una funcion que abra 3 recursos en secuencia (DB, cache, message queue). Si cualquiera falla, cierra los anteriores. Luego describe como el mismo codigo se veria en Rust (solo Drop, sin defer explicito).

### Ejercicio 6: Error wrapping comparado
Dado este flujo: leer archivo → parsear JSON → validar schema → insertar en DB
1. Implementa en Go con `fmt.Errorf("contexto: %w", err)`
2. Describe como se veria en Rust con `?` y `thiserror`
3. ¿Cual te da mejor stack trace? ¿Cual es mas facil de inspeccionar?

### Ejercicio 7: "Olvide el cleanup" — Analisis de bugs
Para cada caso, analiza que pasa en Go, Rust y C:
1. Abrir un archivo y olvidar cerrarlo en una funcion que se llama 10,000 veces
2. Hacer Lock() y olvidar Unlock() en un servidor HTTP concurrente
3. Abrir una conexion TCP y olvidar cerrarla en un loop

### Ejercicio 8: Criterio de eleccion
Para cada herramienta SysAdmin, argumenta si usarias Go o Rust y por que:
1. Un CLI que hace healthchecks a 100 servidores
2. Un proxy HTTP de alto rendimiento
3. Un agente de monitoring que corre en miles de nodos
4. Un tool de backup que comprime archivos grandes
5. Un orchestrator de deploys

---

## 18. Resumen

```
┌──────────────────────────────────────────────────────────────┐
│           GO vs RUST vs C — RESUMEN DE CLEANUP Y PANICS      │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  DEFER vs DROP vs MANUAL                                     │
│  ├─ Go defer: explicito, por funcion, flexible               │
│  │   Pros: puede modificar returns, logica condicional       │
│  │   Cons: se puede olvidar, scope de funcion (no bloque)    │
│  ├─ Rust Drop: automatico, por scope, determinista           │
│  │   Pros: imposible olvidar, scope preciso                  │
│  │   Cons: no puede retornar error, no modifica returns      │
│  └─ C manual: total control, total responsabilidad           │
│      Pros: sin overhead, control total                       │
│      Cons: fuente #1 de resource leaks                       │
│                                                              │
│  PANIC vs PANIC! vs ABORT                                    │
│  ├─ Go panic: siempre recuperable, recover en defer          │
│  │   Patron: panic boundaries en servers y workers           │
│  ├─ Rust panic!: configurable (unwind/abort)                 │
│  │   Patron: catch_unwind solo en FFI boundaries             │
│  └─ C abort: terminacion inmediata, sin cleanup              │
│      Patron: crash and restart via systemd/k8s               │
│                                                              │
│  ERROR vs RESULT vs RETURN CODE                              │
│  ├─ Go error: valor retornado, if err != nil                 │
│  │   Facil de ignorar, verbose pero explicito                │
│  ├─ Rust Result: tipo obligatorio, ? operator                │
│  │   Imposible ignorar, compacto con contexto                │
│  └─ C return code: int + errno, muy facil de ignorar         │
│                                                              │
│  DECISION PARA SYSADMIN/DEVOPS                               │
│  ├─ Go: orquestacion, APIs, servers, herramientas de equipo  │
│  ├─ Rust: CLIs de rendimiento, proxies, bajo nivel           │
│  └─ C: legacy, kernel modules, embedded, rendimiento puro    │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: C04/S01/T01 - Declaracion y multiples retornos — func, retornos nombrados, naked return (evitar), blank identifier _
