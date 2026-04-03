# Interfaces Pequeñas — Principio de Go, Aceptar Interfaces Devolver Structs

## 1. Introduccion

Go tiene una filosofia radical sobre interfaces que lo diferencia de Java, C# y la mayoria de lenguajes OOP: **las interfaces deben ser pequeñas**. Mientras que en Java es comun ver interfaces con 10, 20 o 50 metodos, en Go la norma es 1-3 metodos. Las interfaces mas poderosas de Go tienen **un solo metodo**: `io.Reader`, `io.Writer`, `io.Closer`, `fmt.Stringer`, `error`, `sort.Interface` (3 metodos, y muchos la consideran "grande").

Esta filosofia se complementa con una regla de diseño igualmente importante: **"Accept interfaces, return structs"** (acepta interfaces, devuelve structs). Tus funciones deben pedir lo minimo que necesitan (una interfaz pequeña) y retornar lo maximo que ofrecen (un tipo concreto). Esto maximiza la flexibilidad para los callers y minimiza el acoplamiento.

En SysAdmin/DevOps, estas reglas producen componentes altamente reutilizables: un deployer que acepta `CommandRunner` (1 metodo) funciona con SSH, local exec, Docker exec, o mocks. Un monitor que acepta `HealthChecker` (1 metodo) funciona con TCP, HTTP, gRPC, o ping. Interfaces pequeñas = mas implementaciones posibles = mas composicion = menos codigo.

```
┌─────────────────────────────────────────────────────────────────────────┐
│          INTERFACES PEQUEÑAS — La Filosofia de Go                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Java/C# "enterprise":                                                  │
│  ┌──────────────────────────────────────┐                               │
│  │ interface ServiceManager {           │                               │
│  │   void start()                       │                               │
│  │   void stop()                        │                               │
│  │   void restart()                     │                               │
│  │   Status getStatus()                 │                               │
│  │   Config getConfig()                 │                               │
│  │   void setConfig(Config c)           │                               │
│  │   List<Log> getLogs()                │                               │
│  │   void scale(int replicas)           │                               │
│  │   Health checkHealth()               │                               │
│  │   Metrics getMetrics()               │                               │
│  │   void deploy(Artifact a)            │                               │
│  │   void rollback(String version)      │                               │
│  │   // ... 20 metodos mas              │                               │
│  │ }                                    │                               │
│  └──────────────────────────────────────┘                               │
│  → ¿Quien implementa 20 metodos para un mock?                          │
│  → ¿Quien necesita TODOS los metodos en cada lugar?                    │
│                                                                         │
│  Go idiomatico:                                                         │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐      │
│  │ Starter     │ │ Stopper     │ │ HealthCheck │ │ Deployer    │      │
│  │ Start()err  │ │ Stop()err   │ │ Check()err  │ │ Deploy()err │      │
│  └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘      │
│  → Cada funcion pide SOLO lo que necesita                               │
│  → Mocks triviales (1 metodo cada uno)                                  │
│  → Un tipo puede satisfacer varias simultaneamente                      │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. El Principio: Interfaces Pequeñas en la Stdlib

La stdlib de Go es el mejor ejemplo de esta filosofia. Veamos el tamaño de sus interfaces mas usadas:

```
┌──────────────────────────────────────────────────────────────────────┐
│  INTERFACES DE LA STDLIB POR TAMAÑO                                  │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  1 metodo (la mayoria):                                              │
│  ├─ io.Reader       → Read([]byte)(int,error)                       │
│  ├─ io.Writer       → Write([]byte)(int,error)                      │
│  ├─ io.Closer       → Close() error                                 │
│  ├─ io.Seeker       → Seek(int64,int)(int64,error)                  │
│  ├─ io.ReaderFrom   → ReadFrom(Reader)(int64,error)                 │
│  ├─ io.WriterTo     → WriteTo(Writer)(int64,error)                  │
│  ├─ io.ByteReader   → ReadByte()(byte,error)                        │
│  ├─ io.ByteWriter   → WriteByte(byte) error                         │
│  ├─ io.RuneReader   → ReadRune()(rune,int,error)                    │
│  ├─ io.StringWriter → WriteString(string)(int,error)                │
│  ├─ fmt.Stringer    → String() string                               │
│  ├─ fmt.GoStringer  → GoString() string                             │
│  ├─ error           → Error() string                                │
│  ├─ json.Marshaler  → MarshalJSON()([]byte,error)                   │
│  ├─ json.Unmarshaler→ UnmarshalJSON([]byte) error                   │
│  ├─ http.Handler    → ServeHTTP(ResponseWriter,*Request)            │
│  ├─ http.RoundTripper→RoundTrip(*Request)(*Response,error)          │
│  ├─ encoding.TextMarshaler→MarshalText()([]byte,error)              │
│  ├─ encoding.BinaryMarshaler→MarshalBinary()([]byte,error)          │
│  ├─ hash.Hash       → (embeds io.Writer) + Sum,Reset,Size,BlockSize│
│  └─ image.Image     → ColorModel,Bounds,At                         │
│                                                                      │
│  2 metodos:                                                          │
│  ├─ io.ReadWriter   → Reader + Writer (composicion)                 │
│  ├─ io.ReadCloser   → Reader + Closer                               │
│  ├─ io.WriteCloser  → Writer + Closer                               │
│  └─ fs.File         → Stat + Read + Close                           │
│                                                                      │
│  3 metodos:                                                          │
│  ├─ sort.Interface  → Len + Less + Swap                             │
│  ├─ io.ReadWriteCloser → Reader + Writer + Closer                   │
│  └─ http.ResponseWriter → Header + Write + WriteHeader              │
│                                                                      │
│  4+ metodos (rarisimo):                                              │
│  ├─ http.Flusher    → 1 metodo, pero a menudo combinado             │
│  └─ context.Context → 4 metodos (Deadline,Done,Err,Value)           │
│                                                                      │
│  Observacion: la mayoria tiene 1 metodo.                             │
│  Las de 2-3 son composiciones de interfaces de 1 metodo.            │
│  Las de 4+ son excepciones contadas.                                │
│                                                                      │
│  Rob Pike: "The bigger the interface, the weaker the abstraction."  │
└──────────────────────────────────────────────────────────────────────┘
```

### Por que interfaces pequeñas son mejores

```
┌──────────────────────────────────────────────────────────────────────┐
│  INTERFACES PEQUEÑAS vs GRANDES                                      │
├────────────────────────┬─────────────────────────────────────────────┤
│ Propiedad              │ Pequeña (1-2 metodos) │ Grande (10+ metodos)│
├────────────────────────┼───────────────────────┼─────────────────────┤
│ Tipos que la satisfacen│ Muchos                │ Pocos (quiza solo 1)│
│ Facilidad de mockear   │ Trivial               │ Tedioso             │
│ Composibilidad         │ Alta (embedding)      │ Baja                │
│ Reutilizacion          │ Maxima                │ Acoplada al dominio │
│ Comprensibilidad       │ Inmediata             │ Requiere estudio    │
│ Satisfaccion accidental│ Comun (retroactiva)   │ Imposible           │
│ Nombre significativo   │ Facil (-er suffix)    │ Dificil             │
│ Documentacion          │ 1-2 lineas            │ Pagina entera       │
└────────────────────────┴───────────────────────┴─────────────────────┘
```

---

## 3. La Regla "Accept Interfaces, Return Structs"

Esta es una de las reglas de diseño mas citadas de Go. Significa:

```
┌──────────────────────────────────────────────────────────────────────┐
│  "ACCEPT INTERFACES, RETURN STRUCTS"                                 │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  PARAMETROS: pide interfaces (lo minimo que necesitas)               │
│  ├─ ✓ func Process(r io.Reader) error                               │
│  ├─ ✓ func Deploy(runner CommandRunner) error                       │
│  ├─ ✗ func Process(f *os.File) error  ← demasiado especifico       │
│  └─ ✗ func Deploy(c *ssh.Client) error ← acoplado                  │
│                                                                      │
│  RETORNOS: devuelve el tipo concreto (lo maximo que ofreces)        │
│  ├─ ✓ func NewServer(addr string) *Server                          │
│  ├─ ✓ func NewBuffer() *bytes.Buffer                                │
│  ├─ ✗ func NewServer(addr string) Listener ← oculta capacidades    │
│  └─ ✗ func NewBuffer() io.ReadWriter ← el caller pierde metodos    │
│                                                                      │
│  ¿POR QUE?                                                          │
│  ├─ Parametros con interfaz: el caller tiene libertad para pasar    │
│  │   cualquier tipo que satisfaga (File, Buffer, Mock, Conn...)     │
│  ├─ Retorno con struct: el caller tiene acceso a TODOS los metodos  │
│  │   del tipo concreto, no solo a los de la interfaz                │
│  ├─ El caller decide si quiere guardarlo como interfaz:             │
│  │   var w io.Writer = NewBuffer()  ← el caller elige              │
│  └─ El caller puede usar metodos extra:                             │
│       buf := NewBuffer()                                            │
│       buf.Reset()  ← disponible porque retornamos *Buffer          │
└──────────────────────────────────────────────────────────────────────┘
```

### Ejemplo concreto

```go
// ✗ MALO: parametro demasiado especifico
func CountLines(f *os.File) (int, error) {
    scanner := bufio.NewScanner(f)
    count := 0
    for scanner.Scan() {
        count++
    }
    return count, scanner.Err()
}
// Solo funciona con archivos. ¿Y si quiero contar lineas de un string?
// ¿De un HTTP response? ¿De un pipe? No puedo.

// ✓ BIEN: acepta io.Reader
func CountLines(r io.Reader) (int, error) {
    scanner := bufio.NewScanner(r)
    count := 0
    for scanner.Scan() {
        count++
    }
    return count, scanner.Err()
}
// Ahora funciona con CUALQUIER cosa:
// CountLines(os.Stdin)
// CountLines(file)
// CountLines(strings.NewReader("line1\nline2\n"))
// CountLines(resp.Body)
// CountLines(gzipReader)
// CountLines(&networkBuffer)
```

```go
// ✗ MALO: retorna interfaz innecesariamente
func NewLogger() io.Writer {
    return &FileLogger{path: "/var/log/app.log"}
}
// El caller no puede acceder a FileLogger.Rotate(), FileLogger.SetLevel(), etc.

// ✓ BIEN: retorna el tipo concreto
func NewLogger() *FileLogger {
    return &FileLogger{path: "/var/log/app.log"}
}
// El caller tiene acceso completo:
// log := NewLogger()
// log.SetLevel("DEBUG")   ← disponible
// log.Rotate()             ← disponible
// var w io.Writer = log    ← si quiere interfaz, el CALLER decide
```

### Excepciones a la regla

```go
// Excepcion 1: Retornar error (una interfaz) — siempre correcto
func Open(path string) (*File, error) // ✓ error es interfaz, pero universal

// Excepcion 2: Factory que retorna diferentes tipos concretos
func NewStore(backend string) (Store, error) {
    switch backend {
    case "postgres":
        return &PostgresStore{}, nil
    case "redis":
        return &RedisStore{}, nil
    default:
        return nil, fmt.Errorf("unknown backend: %s", backend)
    }
}
// Aqui NECESITAS retornar interfaz porque el tipo concreto varia.
// El caller no puede saber en compilacion que tipo recibira.

// Excepcion 3: Ocultar internals del paquete (encapsulacion)
func NewClient(opts ...Option) Client {
    // Client es interfaz — el tipo concreto es unexported
    return &client{/* ... */}
}
// Usado cuando el tipo concreto es un detalle de implementacion
// que no quieres exponer (caso raro pero valido).
```

---

## 4. Diseñar Interfaces Pequeñas — Proceso

### Paso 1: Empieza sin interfaz

```go
// Primero escribe el codigo con tipos concretos:
func BackupDatabase(db *PostgresDB, dest *os.File) error {
    data, err := db.Dump()
    if err != nil {
        return err
    }
    _, err = dest.Write(data)
    return err
}
```

### Paso 2: Identifica que metodos REALMENTE usas

```go
// De PostgresDB usamos solo: Dump() ([]byte, error)
// De *os.File usamos solo: Write([]byte) (int, error)
```

### Paso 3: Extrae interfaces minimas

```go
// Interfaz para la fuente de datos
type Dumper interface {
    Dump() ([]byte, error)
}

// Para el destino ya existe: io.Writer

func BackupDatabase(src Dumper, dst io.Writer) error {
    data, err := src.Dump()
    if err != nil {
        return err
    }
    _, err = dst.Write(data)
    return err
}

// Ahora funciona con:
// - PostgresDB, MySQL, Redis (cualquiera con Dump)
// - os.File, bytes.Buffer, S3Writer, net.Conn (cualquier Writer)
// - MockDumper en tests
```

### Paso 4: Define la interfaz donde la usas

```go
// La interfaz va en el PAQUETE QUE LA CONSUME:
package backup

// Dumper es definida aqui, no en el paquete de postgres
type Dumper interface {
    Dump() ([]byte, error)
}

func Run(src Dumper, dst io.Writer) error {
    // ...
}

// package postgres — no sabe que backup.Dumper existe
package postgres

type DB struct { /* ... */ }
func (db *DB) Dump() ([]byte, error) { /* ... */ }
// *postgres.DB satisface backup.Dumper automaticamente
```

---

## 5. Convencion de Nombrado: El Sufijo -er

Go tiene una convencion fuerte para interfaces de 1 metodo: **nombre = metodo + "er"**:

```
┌──────────────────────────────────────────────────────────────────────┐
│  CONVENCION DE NOMBRADO PARA INTERFACES                              │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Metodo          → Interfaz       │ Ejemplo                          │
│  ─────────────────────────────────┼───────────────────────────────── │
│  Read()          → Reader         │ io.Reader                        │
│  Write()         → Writer         │ io.Writer                        │
│  Close()         → Closer         │ io.Closer                        │
│  String()        → Stringer       │ fmt.Stringer                     │
│  Scan()          → Scanner        │ fmt.Scanner                      │
│  Seek()          → Seeker         │ io.Seeker                        │
│  Serve()         → Server         │ (conceptual)                     │
│  Handle()        → Handler        │ http.Handler                     │
│  Marshal()       → Marshaler      │ json.Marshaler                   │
│  Unmarshal()     → Unmarshaler    │ json.Unmarshaler                 │
│  Flush()         → Flusher        │ http.Flusher                     │
│  Hijack()        → Hijacker       │ http.Hijacker                    │
│  Format()        → Formatter      │ fmt.Formatter                    │
│  Sort()          → Sorter         │ (conceptual)                     │
│                                                                      │
│  Para 2+ metodos: usar nombre descriptivo                            │
│  ├─ ReadWriter (no ReadWriteer)                                     │
│  ├─ ReadCloser                                                       │
│  └─ ResponseWriter                                                   │
│                                                                      │
│  Para tu dominio, seguir el patron:                                  │
│  ├─ Check() → Checker                                               │
│  ├─ Deploy() → Deployer                                             │
│  ├─ Execute() → Executor                                            │
│  ├─ Provision() → Provisioner                                       │
│  ├─ Notify() → Notifier                                             │
│  ├─ Balance() → Balancer                                            │
│  └─ Resolve() → Resolver                                            │
└──────────────────────────────────────────────────────────────────────┘
```

```go
// SysAdmin interfaces con convencion -er:

type Checker interface {
    Check() error
}

type Deployer interface {
    Deploy(image string, replicas int) error
}

type Executor interface {
    Execute(cmd string, args ...string) ([]byte, error)
}

type Provisioner interface {
    Provision(spec ServerSpec) (*Server, error)
}

type Notifier interface {
    Notify(msg string, severity Severity) error
}

type Resolver interface {
    Resolve(hostname string) ([]string, error)
}

type Rotater interface {
    Rotate() error
}

// Combinaciones descriptivas:
type DeployNotifier interface {
    Deployer
    Notifier
}
```

---

## 6. El Principio de Segregacion de Interfaces (ISP) en Go

ISP dice: "ningun cliente deberia ser forzado a depender de metodos que no usa." En Go esto es natural gracias a interfaces implicitas y pequeñas:

### Anti-patron: interfaz monolitica

```go
// ✗ ANTI-PATRON: interfaz monolitica "god interface"
type InfraManager interface {
    // Servers
    CreateServer(name, image string) error
    DeleteServer(name string) error
    ListServers() ([]Server, error)
    GetServer(name string) (*Server, error)

    // Networks
    CreateNetwork(cidr string) error
    DeleteNetwork(name string) error

    // Storage
    CreateVolume(name string, sizeGB int) error
    AttachVolume(volume, server string) error

    // Deploy
    Deploy(service, image string) error
    Rollback(service, version string) error
    Scale(service string, replicas int) error

    // Monitoring
    GetMetrics(service string) (Metrics, error)
    CheckHealth(service string) error
}

// Problemas:
// 1. Un mock necesita 14 metodos — nadie lo hara
// 2. Una funcion que solo necesita CheckHealth depende de 14 metodos
// 3. Agregar un metodo rompe TODAS las implementaciones
// 4. Solo 1-2 proveedores podran implementar todo
```

### Go idiomatico: interfaces segregadas

```go
// ✓ GO IDIOMATICO: interfaces pequeñas y especificas

type ServerCreator interface {
    CreateServer(name, image string) error
}

type ServerLister interface {
    ListServers() ([]Server, error)
}

type ServerGetter interface {
    GetServer(name string) (*Server, error)
}

type ServerDeleter interface {
    DeleteServer(name string) error
}

type Deployer interface {
    Deploy(service, image string) error
}

type Scaler interface {
    Scale(service string, replicas int) error
}

type HealthChecker interface {
    CheckHealth(service string) error
}

// Cada funcion pide SOLO lo que necesita:

func ProvisionFleet(creator ServerCreator, specs []ServerSpec) error {
    for _, spec := range specs {
        if err := creator.CreateServer(spec.Name, spec.Image); err != nil {
            return fmt.Errorf("provision %s: %w", spec.Name, err)
        }
    }
    return nil
}

func HealthReport(checker HealthChecker, services []string) map[string]error {
    results := make(map[string]error)
    for _, svc := range services {
        results[svc] = checker.CheckHealth(svc)
    }
    return results
}

func RollingDeploy(deployer Deployer, scaler Scaler, service, image string) error {
    if err := scaler.Scale(service, 0); err != nil {
        return err
    }
    if err := deployer.Deploy(service, image); err != nil {
        return err
    }
    return scaler.Scale(service, 3)
}

// Un tipo concreto puede implementar MUCHAS de estas:
type AWSManager struct { /* ... */ }

func (a *AWSManager) CreateServer(name, image string) error { /* ... */ return nil }
func (a *AWSManager) ListServers() ([]Server, error)       { /* ... */ return nil, nil }
func (a *AWSManager) GetServer(name string) (*Server, error){ /* ... */ return nil, nil }
func (a *AWSManager) DeleteServer(name string) error        { /* ... */ return nil }
func (a *AWSManager) Deploy(service, image string) error    { /* ... */ return nil }
func (a *AWSManager) Scale(service string, replicas int) error { /* ... */ return nil }
func (a *AWSManager) CheckHealth(service string) error      { /* ... */ return nil }

// *AWSManager satisface TODAS las interfaces individuales
// Cada funcion solo ve los metodos que necesita
```

### Componer interfaces cuando necesitas multiples capacidades

```go
// Si UNA funcion necesita deploy + scale, compone:
type DeployScaler interface {
    Deployer
    Scaler
}

func RollingDeploy(ds DeployScaler, service, image string) error {
    // Pide exactamente lo que necesita: Deploy + Scale
    // No 14 metodos, sino 2
    return nil
}

// Patron: componer ad-hoc solo cuando lo necesitas
// No crear interfaces compuestas "por si acaso"
```

---

## 7. Patron: Un Metodo, Maxima Reutilizacion

### io.Reader en accion

```go
// Una funcion que acepta io.Reader funciona con CUALQUIER fuente de bytes:

func SHA256Sum(r io.Reader) (string, error) {
    h := sha256.New()
    if _, err := io.Copy(h, r); err != nil {
        return "", err
    }
    return fmt.Sprintf("%x", h.Sum(nil)), nil
}

// Funciona con todo:
hash1, _ := SHA256Sum(file)                           // archivo
hash2, _ := SHA256Sum(strings.NewReader("data"))      // string
hash3, _ := SHA256Sum(resp.Body)                      // HTTP response
hash4, _ := SHA256Sum(&buf)                           // buffer
hash5, _ := SHA256Sum(gzReader)                       // stream comprimido
hash6, _ := SHA256Sum(io.LimitReader(conn, 1<<20))    // red, max 1MB
```

### Interfaz de 1 metodo para SysAdmin

```go
// HealthChecker — 1 metodo, composicion infinita
type HealthChecker interface {
    Check() error
}

// Implementaciones:
type TCPChecker struct {
    Addr    string
    Timeout time.Duration
}

func (t *TCPChecker) Check() error {
    conn, err := net.DialTimeout("tcp", t.Addr, t.Timeout)
    if err != nil {
        return err
    }
    return conn.Close()
}

type HTTPChecker struct {
    URL     string
    Timeout time.Duration
}

func (h *HTTPChecker) Check() error {
    client := &http.Client{Timeout: h.Timeout}
    resp, err := client.Get(h.URL)
    if err != nil {
        return err
    }
    defer resp.Body.Close()
    if resp.StatusCode >= 400 {
        return fmt.Errorf("unhealthy: status %d", resp.StatusCode)
    }
    return nil
}

type DiskChecker struct {
    Path      string
    MinFreeGB float64
}

func (d *DiskChecker) Check() error {
    // Verificar espacio libre en disco...
    return nil
}

type CommandChecker struct {
    Cmd  string
    Args []string
}

func (c *CommandChecker) Check() error {
    return exec.Command(c.Cmd, c.Args...).Run()
}

// Composicion: verificar multiples cosas
type MultiChecker struct {
    Name     string
    Checkers []HealthChecker
}

func (m *MultiChecker) Check() error {
    var errs []string
    for _, c := range m.Checkers {
        if err := c.Check(); err != nil {
            errs = append(errs, err.Error())
        }
    }
    if len(errs) > 0 {
        return fmt.Errorf("%s: %d checks failed: %s",
            m.Name, len(errs), strings.Join(errs, "; "))
    }
    return nil
}

// MultiChecker tambien satisface HealthChecker → composicion recursiva!
// Puedes anidar MultiCheckers dentro de MultiCheckers.

// Uso:
func main() {
    webCheck := &MultiChecker{
        Name: "web-service",
        Checkers: []HealthChecker{
            &TCPChecker{Addr: "10.0.1.10:443", Timeout: 5 * time.Second},
            &HTTPChecker{URL: "https://web-01/healthz", Timeout: 3 * time.Second},
            &DiskChecker{Path: "/var/log", MinFreeGB: 1.0},
        },
    }

    if err := webCheck.Check(); err != nil {
        fmt.Println("UNHEALTHY:", err)
    } else {
        fmt.Println("HEALTHY")
    }
}
```

### Adapter: convertir funciones a interfaces

```go
// Patron del stdlib: http.HandlerFunc convierte func → Handler
// Mismo patron para cualquier interfaz de 1 metodo:

// Interface
type HealthChecker interface {
    Check() error
}

// Adapter type — convierte func() error → HealthChecker
type CheckerFunc func() error

func (f CheckerFunc) Check() error {
    return f()
}

// Ahora puedes usar funciones anonimas como HealthCheckers:
func main() {
    pingCheck := CheckerFunc(func() error {
        return exec.Command("ping", "-c", "1", "-W", "2", "8.8.8.8").Run()
    })

    dnsCheck := CheckerFunc(func() error {
        _, err := net.LookupHost("google.com")
        return err
    })

    multi := &MultiChecker{
        Name: "connectivity",
        Checkers: []HealthChecker{pingCheck, dnsCheck},
    }

    multi.Check()
}
```

---

## 8. Patron: Interfaces en Constructores y Dependency Injection

### Constructor con interfaces — testabilidad automatica

```go
// Servicio de deployment — depende de interfaces, no de tipos concretos
type DeployService struct {
    runner  CommandRunner  // 1 metodo
    logger  Logger         // 1-2 metodos
    notifier Notifier      // 1 metodo
}

type CommandRunner interface {
    Run(cmd string, args ...string) ([]byte, error)
}

type Logger interface {
    Info(msg string)
    Error(msg string, err error)
}

type Notifier interface {
    Notify(channel, message string) error
}

// Constructor acepta interfaces
func NewDeployService(runner CommandRunner, logger Logger, notifier Notifier) *DeployService {
    return &DeployService{
        runner:   runner,
        logger:   logger,
        notifier: notifier,
    }
}

// Retorna struct (el caller tiene acceso completo)
func (d *DeployService) Deploy(service, image string) error {
    d.logger.Info(fmt.Sprintf("deploying %s with image %s", service, image))

    out, err := d.runner.Run("docker", "pull", image)
    if err != nil {
        d.logger.Error("pull failed", err)
        d.notifier.Notify("deploys", fmt.Sprintf("FAILED: %s pull %s: %v", service, image, err))
        return fmt.Errorf("pull %s: %w", image, err)
    }
    d.logger.Info(fmt.Sprintf("pulled: %s", string(out)))

    out, err = d.runner.Run("docker", "run", "-d", "--name", service, image)
    if err != nil {
        d.logger.Error("run failed", err)
        return fmt.Errorf("run %s: %w", service, err)
    }

    containerID := strings.TrimSpace(string(out))
    d.notifier.Notify("deploys", fmt.Sprintf("OK: %s deployed as %s", service, containerID))
    return nil
}
```

### Testing con mocks triviales

```go
// Mocks para testing — TRIVIALES porque las interfaces son pequeñas:

type MockRunner struct {
    Outputs map[string][]byte
    Errors  map[string]error
    Calls   []string
}

func (m *MockRunner) Run(cmd string, args ...string) ([]byte, error) {
    key := cmd + " " + strings.Join(args, " ")
    m.Calls = append(m.Calls, key)
    return m.Outputs[key], m.Errors[key]
}

type MockLogger struct {
    Infos  []string
    Errors []string
}

func (m *MockLogger) Info(msg string)           { m.Infos = append(m.Infos, msg) }
func (m *MockLogger) Error(msg string, err error) { m.Errors = append(m.Errors, msg) }

type MockNotifier struct {
    Messages []string
    Err      error
}

func (m *MockNotifier) Notify(channel, message string) error {
    m.Messages = append(m.Messages, channel+": "+message)
    return m.Err
}

// Test:
func TestDeploySuccess(t *testing.T) {
    runner := &MockRunner{
        Outputs: map[string][]byte{
            "docker pull nginx:latest":                       []byte("pulled"),
            "docker run -d --name web nginx:latest":          []byte("abc123\n"),
        },
    }
    logger := &MockLogger{}
    notifier := &MockNotifier{}

    svc := NewDeployService(runner, logger, notifier)
    err := svc.Deploy("web", "nginx:latest")

    if err != nil {
        t.Fatalf("unexpected error: %v", err)
    }
    if len(runner.Calls) != 2 {
        t.Fatalf("expected 2 commands, got %d", len(runner.Calls))
    }
    if len(notifier.Messages) != 1 {
        t.Fatalf("expected 1 notification, got %d", len(notifier.Messages))
    }
}

// Comparar con una interfaz de 20 metodos:
// ¿Cuantos metodos tendrias que implementar en cada mock?
// Con interfaces pequeñas: 1-2 metodos por mock.
```

---

## 9. Patron: Interfaces Opcionales (Feature Detection)

A veces un tipo tiene capacidades extra que no todos comparten. En lugar de una interfaz grande, detectas capacidades con type assertion:

```go
// Interfaz base: todos deben tener esto
type Storage interface {
    Read(key string) ([]byte, error)
    Write(key string, data []byte) error
}

// Interfaces opcionales: no todos la tienen
type Lister interface {
    List(prefix string) ([]string, error)
}

type Deleter interface {
    Delete(key string) error
}

type Watcher interface {
    Watch(key string) <-chan []byte
}

// Funcion que usa la base, pero aprovecha extras si existen:
func SyncConfig(src, dst Storage) error {
    // ¿Src puede listar? Mejor — sincronizamos todo
    if lister, ok := src.(Lister); ok {
        keys, err := lister.List("")
        if err != nil {
            return err
        }
        for _, key := range keys {
            data, err := src.Read(key)
            if err != nil {
                return err
            }
            if err := dst.Write(key, data); err != nil {
                return err
            }
        }
        return nil
    }

    // Sin List — solo podemos sincronizar keys conocidas
    for _, key := range []string{"config.yaml", "secrets.env", "hosts.txt"} {
        data, err := src.Read(key)
        if err != nil {
            continue // key might not exist
        }
        dst.Write(key, data)
    }
    return nil
}

// Stdlib usa este patron extensivamente:
// io.Copy verifica si src implementa WriterTo
// io.Copy verifica si dst implementa ReaderFrom
// net/http verifica si ResponseWriter implementa Flusher
// encoding/json verifica si el tipo implementa Marshaler
```

---

## 10. Anti-Patrones: Lo que NO Hacer

### Anti-patron 1: Interfaz prematura

```go
// ✗ Definir interfaz antes de tener 2+ implementaciones
package mydb

type Database interface {     // ← Solo hay UNA implementacion
    Query(sql string) error
    Close() error
}

type PostgresDB struct{}
func (p *PostgresDB) Query(sql string) error { return nil }
func (p *PostgresDB) Close() error { return nil }

// ✓ MEJOR: exporta el struct. Crea interfaz cuando la necesites
// (cuando tengas 2+ implementaciones o necesites mocking)
package mydb

type PostgresDB struct{}
func (p *PostgresDB) Query(sql string) error { return nil }
func (p *PostgresDB) Close() error { return nil }

// El CONSUMIDOR crea su propia interfaz si necesita:
// package handler
// type Querier interface {
//     Query(sql string) error
// }
```

### Anti-patron 2: Interfaz en el productor con una sola implementacion

```go
// ✗ Definir interfaz + struct + constructor en el mismo paquete
// cuando la interfaz no agrega valor
package storage

type Store interface {        // ← innecesaria
    Get(key string) (string, error)
    Set(key, value string) error
}

type RedisStore struct{}
func (r *RedisStore) Get(key string) (string, error) { return "", nil }
func (r *RedisStore) Set(key, value string) error { return nil }

func New() Store {    // ← retorna interfaz innecesariamente
    return &RedisStore{}
}

// ✓ MEJOR:
package storage

type RedisStore struct{}
func (r *RedisStore) Get(key string) (string, error) { return "", nil }
func (r *RedisStore) Set(key, value string) error { return nil }

func New() *RedisStore {    // ← retorna el tipo concreto
    return &RedisStore{}
}
```

### Anti-patron 3: Interfaces con getters y setters

```go
// ✗ Interfaz con getters/setters — estilo Java, no Go
type Config interface {
    GetHost() string
    SetHost(host string)
    GetPort() int
    SetPort(port int)
    GetTimeout() time.Duration
    SetTimeout(d time.Duration)
}

// ✓ En Go: si necesitas leer config, pasa el struct
type Config struct {
    Host    string
    Port    int
    Timeout time.Duration
}

// Si necesitas abstraer el acceso:
type ConfigReader interface {
    Config() Config  // retorna el struct completo
}
```

### Anti-patron 4: La interfaz util (utility interface)

```go
// ✗ Interfaz que agrupa metodos que no tienen relacion
type Utils interface {
    Hash(data []byte) string
    Compress(data []byte) ([]byte, error)
    ValidateEmail(email string) bool
    FormatBytes(n int64) string
}

// ✓ Estas son funciones, no interfaces. Usar funciones directamente:
func Hash(data []byte) string { /* ... */ }
func Compress(data []byte) ([]byte, error) { /* ... */ }
func ValidateEmail(email string) bool { /* ... */ }
func FormatBytes(n int64) string { /* ... */ }
```

---

## 11. Interfaces Pequeñas y Testing

Las interfaces pequeñas hacen testing **trivial**. Comparacion:

```go
// ============ INTERFAZ GRANDE → testing doloroso ============

type BigServiceManager interface {
    Deploy(service, image string) error
    Scale(service string, n int) error
    GetLogs(service string) ([]string, error)
    Restart(service string) error
    CheckHealth(service string) error
    GetMetrics(service string) (Metrics, error)
    ListServices() ([]Service, error)
    DeleteService(name string) error
}

// Mock necesita 8 metodos — incluso si el test solo necesita Deploy:
type MockBigManager struct {
    DeployErr error
    // ... 7 campos mas que no usas
}
func (m *MockBigManager) Deploy(s, i string) error   { return m.DeployErr }
func (m *MockBigManager) Scale(s string, n int) error { return nil }  // stub
func (m *MockBigManager) GetLogs(s string) ([]string, error) { return nil, nil }
func (m *MockBigManager) Restart(s string) error     { return nil }
func (m *MockBigManager) CheckHealth(s string) error  { return nil }
func (m *MockBigManager) GetMetrics(s string) (Metrics, error) { return Metrics{}, nil }
func (m *MockBigManager) ListServices() ([]Service, error) { return nil, nil }
func (m *MockBigManager) DeleteService(s string) error { return nil }
// 8 metodos stub para testear 1.

// ============ INTERFACES PEQUEÑAS → testing trivial ============

type Deployer interface {
    Deploy(service, image string) error
}

// Mock: 1 metodo, 2 lineas
type MockDeployer struct {
    Err error
}
func (m *MockDeployer) Deploy(s, i string) error { return m.Err }

// Test claro y directo:
func TestDeploy(t *testing.T) {
    mock := &MockDeployer{Err: nil}
    err := RunDeploy(mock, "web", "nginx:latest")
    if err != nil {
        t.Fatal(err)
    }
}
```

---

## 12. Comparacion con C y Rust

```
┌──────────────────────────────────────────────────────────────────────────┐
│  INTERFACES PEQUEÑAS — Go vs C vs Rust                                   │
├──────────────┬──────────────────────┬──────────────────┬─────────────────┤
│ Aspecto      │ Go                   │ C                │ Rust            │
├──────────────┼──────────────────────┼──────────────────┼─────────────────┤
│ Abstracion   │ Interfaces de 1-3    │ Function ptrs    │ Traits de 1-N   │
│ minima       │ metodos, implicitas  │ individuales     │ metodos, explic.│
├──────────────┼──────────────────────┼──────────────────┼─────────────────┤
│ Equivalente  │ io.Reader (1 metodo) │ ssize_t (*read)  │ trait Read {    │
│ a Reader     │                      │ (void*,char*,n)  │   fn read(...)  │
│              │                      │                  │ }               │
├──────────────┼──────────────────────┼──────────────────┼─────────────────┤
│ Composicion  │ Embedding:           │ Structs con      │ Supertraits:    │
│              │ ReadWriter = R + W   │ multiples fptrs  │ trait A: B + C  │
├──────────────┼──────────────────────┼──────────────────┼─────────────────┤
│ Segregacion  │ Natural (implicita)  │ Manual (1 fptr   │ Posible pero    │
│              │ define interfaz      │ por capacidad)   │ requiere impl   │
│              │ donde la consumes    │                  │ explicito       │
├──────────────┼──────────────────────┼──────────────────┼─────────────────┤
│ Mocking      │ Trivial (struct con  │ Function ptr a   │ Traits + mock   │
│              │ 1 metodo)            │ funcion fake     │ crate (mockall) │
├──────────────┼──────────────────────┼──────────────────┼─────────────────┤
│ Feature      │ Type assertion       │ Check si fptr    │ Trait bounds    │
│ detection    │ if v, ok :=          │ != NULL          │ (static) o      │
│              │ x.(Flusher)          │                  │ Any downcast    │
├──────────────┼──────────────────────┼──────────────────┼─────────────────┤
│ Adapter func │ type F func()        │ Wrapper func +   │ impl Trait for  │
│ → interface  │ func (f F) Do() {f()}│ void* context    │ closure type    │
├──────────────┼──────────────────────┼──────────────────┼─────────────────┤
│ Retornar     │ "Return structs"     │ Retornar struct  │ impl Trait (1   │
│              │ (tipo concreto)      │ siempre (no hay  │ tipo) o Box<dyn>│
│              │                      │ interfaces)      │ (dynamic)       │
└──────────────┴──────────────────────┴──────────────────┴─────────────────┘
```

---

## 13. Ejercicios de Prediccion

### Ejercicio 1: ¿Cual es mejor diseño?

```go
// Opcion A:
func Backup(db *PostgresDB, dest *os.File) error

// Opcion B:
func Backup(db *PostgresDB, dest io.Writer) error

// Opcion C:
func Backup(src Dumper, dest io.Writer) error
```

<details>
<summary>Respuesta</summary>

**Opcion C** es la mejor. Cada parametro acepta una interfaz minima:
- `src Dumper` (1 metodo: `Dump() ([]byte, error)`) — funciona con Postgres, MySQL, Redis, mock
- `dest io.Writer` (1 metodo: `Write([]byte)(int,error)`) — funciona con archivo, buffer, S3, red, mock

La opcion A es la peor (acoplada a 2 tipos concretos). La B mejora el destino pero no la fuente.

</details>

### Ejercicio 2: ¿Interfaz necesaria?

```go
package cache

type Cache interface {
    Get(key string) ([]byte, bool)
    Set(key string, val []byte, ttl time.Duration)
    Delete(key string)
}

type MemoryCache struct {
    items map[string]cacheEntry
    mu    sync.RWMutex
}

// MemoryCache es la UNICA implementacion de Cache.
```

<details>
<summary>Respuesta</summary>

**La interfaz Cache es prematura.** Solo hay una implementacion. En Go idiomatico:
1. Exporta `MemoryCache` directamente con sus metodos
2. No definas la interfaz `Cache` en este paquete
3. Si un consumidor necesita mockear, definira **su propia** interfaz (quiza solo con Get) en su paquete

Cuando agregues una segunda implementacion (RedisCache), entonces puede tener sentido una interfaz, pero definida en el **consumidor**, no en el paquete cache.

</details>

### Ejercicio 3: ¿Compila?

```go
type Pinger interface {
    Ping() error
}

func PingAll(pingers []Pinger) {
    for _, p := range pingers {
        p.Ping()
    }
}

type Server struct{ Addr string }
func (s *Server) Ping() error { return nil }

func main() {
    servers := []*Server{
        {Addr: "10.0.1.1"},
        {Addr: "10.0.1.2"},
    }
    PingAll(servers)
}
```

<details>
<summary>Respuesta</summary>

**No compila**: `cannot use servers (variable of type []*Server) as []Pinger in argument to PingAll`.

Aunque `*Server` satisface `Pinger`, `[]*Server` NO es `[]Pinger`. Go no tiene covarianza de slices. Necesitas convertir explicitamente:

```go
pingers := make([]Pinger, len(servers))
for i, s := range servers {
    pingers[i] = s
}
PingAll(pingers)
```

Esto es una consecuencia de la representacion en memoria: `[]Pinger` tiene 16 bytes por elemento (interfaz = tab+data), pero `[]*Server` tiene 8 bytes por elemento (solo un puntero).

</details>

### Ejercicio 4: ¿Es correcto este retorno?

```go
type Logger interface {
    Log(msg string)
}

func NewLogger(path string) Logger {
    if path == "" {
        return &ConsoleLogger{}
    }
    return &FileLogger{Path: path}
}
```

<details>
<summary>Respuesta</summary>

**Si, es correcto** — esta es una de las excepciones validas a "return structs". Cuando un factory/constructor retorna **diferentes tipos concretos** segun la configuracion, necesita retornar una interfaz. El caller no puede saber en compilacion si recibira `*ConsoleLogger` o `*FileLogger`.

Otra alternativa seria usar un solo tipo concreto que maneje ambos casos internamente, pero eso no siempre es practico.

</details>

### Ejercicio 5: ¿Cuantos metodos debe tener el mock?

```go
type ServiceManager interface {
    Start(name string) error
    Stop(name string) error
    Restart(name string) error
    Status(name string) (string, error)
    Logs(name string, lines int) ([]string, error)
}

func RestartIfUnhealthy(mgr ServiceManager, name string) error {
    status, err := mgr.Status(name)
    if err != nil {
        return err
    }
    if status != "running" {
        return mgr.Restart(name)
    }
    return nil
}
```

<details>
<summary>Respuesta</summary>

El mock necesita implementar **5 metodos** (todos los de ServiceManager), aunque `RestartIfUnhealthy` solo usa 2 (Status y Restart). Los otros 3 necesitan stubs vacios.

**Mejor diseño**: la funcion deberia aceptar una interfaz mas pequeña:

```go
type StatusRestarter interface {
    Status(name string) (string, error)
    Restart(name string) error
}

func RestartIfUnhealthy(mgr StatusRestarter, name string) error {
    // mismo codigo
}
```

Ahora el mock solo necesita 2 metodos. O mejor aun, 2 interfaces de 1 metodo:

```go
type StatusChecker interface { Status(name string) (string, error) }
type Restarter interface { Restart(name string) error }

type StatusRestarter interface {
    StatusChecker
    Restarter
}
```

</details>

---

## 14. Programa Completo: Modular Infrastructure Monitor con Interfaces Pequeñas

```go
// modular_monitor.go
// Demuestra el principio de interfaces pequeñas y "accept interfaces,
// return structs" para construir un monitor de infraestructura modular
// donde cada componente depende de interfaces minimas.
package main

import (
    "fmt"
    "math/rand"
    "sort"
    "strings"
    "sync"
    "time"
)

// =====================================================================
// INTERFACES PEQUEÑAS — cada una con 1-2 metodos
// =====================================================================

// Checker — 1 metodo: verificar salud
type Checker interface {
    Check() error
}

// Namer — 1 metodo: identificarse
type Namer interface {
    Name() string
}

// NamedChecker — composicion de 2 interfaces pequeñas
type NamedChecker interface {
    Checker
    Namer
}

// Notifier — 1 metodo: enviar notificacion
type Notifier interface {
    Notify(subject, body string) error
}

// MetricRecorder — 1 metodo: registrar metrica
type MetricRecorder interface {
    Record(name string, value float64, tags map[string]string)
}

// Logger — 2 metodos: info y error
type Logger interface {
    Info(msg string)
    Error(msg string)
}

// StatusStore — 2 metodos: guardar y obtener estado
type StatusStore interface {
    SetStatus(name string, status CheckResult)
    GetStatus(name string) (CheckResult, bool)
}

// Formatter — 1 metodo: formatear resultado como string
type Formatter interface {
    Format(results []CheckResult) string
}

// =====================================================================
// TIPOS DE DATO
// =====================================================================

type CheckResult struct {
    Name      string
    Healthy   bool
    Message   string
    Duration  time.Duration
    Timestamp time.Time
}

func (r CheckResult) String() string {
    status := "✓ OK"
    if !r.Healthy {
        status = "✗ FAIL"
    }
    return fmt.Sprintf("%-6s %-20s %8s  %s",
        status, r.Name, r.Duration.Round(time.Millisecond), r.Message)
}

// =====================================================================
// IMPLEMENTACIONES DE Checker (1 metodo)
// Cada una es un struct independiente que satisface Checker + Namer
// =====================================================================

// TCPCheck verifica conectividad TCP
type TCPCheck struct {
    Host    string
    Port    int
    Timeout time.Duration
}

func (t *TCPCheck) Check() error {
    // Simulado — en produccion: net.DialTimeout
    time.Sleep(time.Duration(rand.Intn(50)) * time.Millisecond)
    if rand.Intn(10) < 2 { // 20% de fallo simulado
        return fmt.Errorf("connection refused: %s:%d", t.Host, t.Port)
    }
    return nil
}

func (t *TCPCheck) Name() string {
    return fmt.Sprintf("tcp://%s:%d", t.Host, t.Port)
}

var _ NamedChecker = (*TCPCheck)(nil)

// DiskCheck verifica espacio en disco
type DiskCheck struct {
    Path      string
    MinFreeGB float64
}

func (d *DiskCheck) Check() error {
    time.Sleep(time.Duration(rand.Intn(20)) * time.Millisecond)
    freeGB := 5.0 + rand.Float64()*50 // simulado
    if freeGB < d.MinFreeGB {
        return fmt.Errorf("only %.1f GB free on %s (min: %.1f GB)",
            freeGB, d.Path, d.MinFreeGB)
    }
    return nil
}

func (d *DiskCheck) Name() string {
    return fmt.Sprintf("disk:%s", d.Path)
}

var _ NamedChecker = (*DiskCheck)(nil)

// ProcessCheck verifica que un proceso esta corriendo
type ProcessCheck struct {
    ProcessName string
}

func (p *ProcessCheck) Check() error {
    time.Sleep(time.Duration(rand.Intn(10)) * time.Millisecond)
    if rand.Intn(20) < 1 { // 5% de fallo
        return fmt.Errorf("process %s not found", p.ProcessName)
    }
    return nil
}

func (p *ProcessCheck) Name() string {
    return fmt.Sprintf("proc:%s", p.ProcessName)
}

var _ NamedChecker = (*ProcessCheck)(nil)

// CheckerFunc — adapter: convierte func() error en Checker
type CheckerFunc struct {
    name string
    fn   func() error
}

func NewCheckerFunc(name string, fn func() error) *CheckerFunc {
    return &CheckerFunc{name: name, fn: fn}
}

func (c *CheckerFunc) Check() error { return c.fn() }
func (c *CheckerFunc) Name() string { return c.name }

var _ NamedChecker = (*CheckerFunc)(nil)

// =====================================================================
// IMPLEMENTACIONES DE Notifier (1 metodo)
// =====================================================================

type ConsoleNotifier struct {
    Prefix string
}

func (c *ConsoleNotifier) Notify(subject, body string) error {
    fmt.Printf("  [NOTIFY:%s] %s — %s\n", c.Prefix, subject, body)
    return nil
}

var _ Notifier = (*ConsoleNotifier)(nil)

type SlackNotifier struct {
    Channel string
    Token   string
}

func (s *SlackNotifier) Notify(subject, body string) error {
    fmt.Printf("  [SLACK:#%s] %s — %s\n", s.Channel, subject, body)
    // En produccion: POST a Slack webhook
    return nil
}

var _ Notifier = (*SlackNotifier)(nil)

// MultiNotifier — compone multiples Notifiers
type MultiNotifier struct {
    notifiers []Notifier
}

func NewMultiNotifier(notifiers ...Notifier) *MultiNotifier {
    return &MultiNotifier{notifiers: notifiers}
}

// Satisface Notifier — composicion recursiva
func (m *MultiNotifier) Notify(subject, body string) error {
    var errs []string
    for _, n := range m.notifiers {
        if err := n.Notify(subject, body); err != nil {
            errs = append(errs, err.Error())
        }
    }
    if len(errs) > 0 {
        return fmt.Errorf("notification errors: %s", strings.Join(errs, "; "))
    }
    return nil
}

var _ Notifier = (*MultiNotifier)(nil)

// =====================================================================
// IMPLEMENTACIONES DE Logger (2 metodos)
// =====================================================================

type ConsoleLogger struct {
    mu sync.Mutex
}

func (c *ConsoleLogger) Info(msg string) {
    c.mu.Lock()
    defer c.mu.Unlock()
    fmt.Printf("  [INFO]  %s\n", msg)
}

func (c *ConsoleLogger) Error(msg string) {
    c.mu.Lock()
    defer c.mu.Unlock()
    fmt.Printf("  [ERROR] %s\n", msg)
}

var _ Logger = (*ConsoleLogger)(nil)

// =====================================================================
// IMPLEMENTACIONES DE MetricRecorder (1 metodo)
// =====================================================================

type InMemoryMetrics struct {
    mu      sync.Mutex
    metrics map[string][]float64
}

func NewInMemoryMetrics() *InMemoryMetrics {
    return &InMemoryMetrics{metrics: make(map[string][]float64)}
}

func (m *InMemoryMetrics) Record(name string, value float64, tags map[string]string) {
    m.mu.Lock()
    defer m.mu.Unlock()
    key := name
    if tags != nil {
        parts := make([]string, 0, len(tags))
        for k, v := range tags {
            parts = append(parts, k+"="+v)
        }
        sort.Strings(parts)
        key += "{" + strings.Join(parts, ",") + "}"
    }
    m.metrics[key] = append(m.metrics[key], value)
}

func (m *InMemoryMetrics) Summary() string {
    m.mu.Lock()
    defer m.mu.Unlock()
    var lines []string
    keys := make([]string, 0, len(m.metrics))
    for k := range m.metrics {
        keys = append(keys, k)
    }
    sort.Strings(keys)
    for _, k := range keys {
        vals := m.metrics[k]
        var sum float64
        for _, v := range vals {
            sum += v
        }
        lines = append(lines, fmt.Sprintf("  %-50s samples=%-3d avg=%.2f", k, len(vals), sum/float64(len(vals))))
    }
    return strings.Join(lines, "\n")
}

var _ MetricRecorder = (*InMemoryMetrics)(nil)

// =====================================================================
// IMPLEMENTACIONES DE StatusStore (2 metodos)
// =====================================================================

type InMemoryStatusStore struct {
    mu      sync.RWMutex
    statuses map[string]CheckResult
}

func NewInMemoryStatusStore() *InMemoryStatusStore {
    return &InMemoryStatusStore{statuses: make(map[string]CheckResult)}
}

func (s *InMemoryStatusStore) SetStatus(name string, status CheckResult) {
    s.mu.Lock()
    defer s.mu.Unlock()
    s.statuses[name] = status
}

func (s *InMemoryStatusStore) GetStatus(name string) (CheckResult, bool) {
    s.mu.RLock()
    defer s.mu.RUnlock()
    r, ok := s.statuses[name]
    return r, ok
}

func (s *InMemoryStatusStore) All() []CheckResult {
    s.mu.RLock()
    defer s.mu.RUnlock()
    results := make([]CheckResult, 0, len(s.statuses))
    for _, r := range s.statuses {
        results = append(results, r)
    }
    sort.Slice(results, func(i, j int) bool {
        return results[i].Name < results[j].Name
    })
    return results
}

var _ StatusStore = (*InMemoryStatusStore)(nil)

// =====================================================================
// IMPLEMENTACIONES DE Formatter (1 metodo)
// =====================================================================

type TableFormatter struct{}

func (f TableFormatter) Format(results []CheckResult) string {
    var b strings.Builder
    fmt.Fprintf(&b, "  %-6s %-20s %8s  %s\n", "STATUS", "CHECK", "DURATION", "MESSAGE")
    fmt.Fprintf(&b, "  %s\n", strings.Repeat("─", 60))
    for _, r := range results {
        fmt.Fprintf(&b, "  %s\n", r.String())
    }

    healthy := 0
    for _, r := range results {
        if r.Healthy {
            healthy++
        }
    }
    fmt.Fprintf(&b, "  %s\n", strings.Repeat("─", 60))
    fmt.Fprintf(&b, "  Total: %d checks, %d healthy, %d failed\n",
        len(results), healthy, len(results)-healthy)
    return b.String()
}

var _ Formatter = TableFormatter{}

type CompactFormatter struct{}

func (f CompactFormatter) Format(results []CheckResult) string {
    var parts []string
    for _, r := range results {
        sym := "+"
        if !r.Healthy {
            sym = "-"
        }
        parts = append(parts, fmt.Sprintf("[%s%s]", sym, r.Name))
    }
    return "  " + strings.Join(parts, " ")
}

var _ Formatter = CompactFormatter{}

// =====================================================================
// MONITOR — orquesta todo usando SOLO interfaces pequeñas
// Acepta interfaces, retorna struct
// =====================================================================

type Monitor struct {
    checks   []NamedChecker
    notifier Notifier
    metrics  MetricRecorder
    logger   Logger
    store    StatusStore
    formatter Formatter
}

// Constructor: acepta interfaces, retorna *Monitor (struct)
func NewMonitor(
    notifier Notifier,
    metrics MetricRecorder,
    logger Logger,
    store StatusStore,
    formatter Formatter,
) *Monitor {
    return &Monitor{
        notifier:  notifier,
        metrics:   metrics,
        logger:    logger,
        store:     store,
        formatter: formatter,
    }
}

func (m *Monitor) AddCheck(check NamedChecker) {
    m.checks = append(m.checks, check)
}

func (m *Monitor) RunAll() []CheckResult {
    results := make([]CheckResult, 0, len(m.checks))

    for _, check := range m.checks {
        start := time.Now()
        err := check.Check()
        duration := time.Since(start)

        result := CheckResult{
            Name:      check.Name(),
            Healthy:   err == nil,
            Duration:  duration,
            Timestamp: time.Now(),
        }

        if err != nil {
            result.Message = err.Error()
            m.logger.Error(fmt.Sprintf("%s FAILED: %v", check.Name(), err))

            // Notify on failure
            m.notifier.Notify(
                fmt.Sprintf("Health Check Failed: %s", check.Name()),
                err.Error(),
            )

            // Record failure metric
            m.metrics.Record("check.failed", 1, map[string]string{
                "check": check.Name(),
            })
        } else {
            result.Message = "ok"
            m.logger.Info(fmt.Sprintf("%s OK (%s)", check.Name(), duration.Round(time.Millisecond)))
        }

        // Record duration metric
        m.metrics.Record("check.duration_ms", float64(duration.Milliseconds()), map[string]string{
            "check": check.Name(),
        })

        // Store status
        m.store.SetStatus(check.Name(), result)

        results = append(results, result)
    }

    return results
}

func (m *Monitor) Report(results []CheckResult) string {
    return m.formatter.Format(results)
}

// =====================================================================
// MAIN
// =====================================================================

func main() {
    fmt.Println("╔══════════════════════════════════════════════════════════╗")
    fmt.Println("║   Modular Infrastructure Monitor — Small Interfaces     ║")
    fmt.Println("╚══════════════════════════════════════════════════════════╝")

    // --- 1. Create components (all satisfy small interfaces) ---

    fmt.Println("\n── 1. Building Components ──")

    // Notifier: compose multiple small notifiers
    notifier := NewMultiNotifier(
        &ConsoleNotifier{Prefix: "alert"},
        &SlackNotifier{Channel: "ops-alerts", Token: "xoxb-***"},
    )

    metrics := NewInMemoryMetrics()
    logger := &ConsoleLogger{}
    store := NewInMemoryStatusStore()
    tableFormatter := TableFormatter{}

    // --- 2. Create monitor (accepts interfaces, returns struct) ---

    fmt.Println("\n── 2. Creating Monitor ──")

    monitor := NewMonitor(notifier, metrics, logger, store, tableFormatter)

    // --- 3. Add checks (each satisfies NamedChecker = Checker + Namer) ---

    fmt.Println("\n── 3. Adding Checks ──")

    monitor.AddCheck(&TCPCheck{Host: "db-01.internal", Port: 5432, Timeout: 5 * time.Second})
    monitor.AddCheck(&TCPCheck{Host: "redis-01.internal", Port: 6379, Timeout: 3 * time.Second})
    monitor.AddCheck(&TCPCheck{Host: "web-01.internal", Port: 443, Timeout: 5 * time.Second})
    monitor.AddCheck(&DiskCheck{Path: "/var/log", MinFreeGB: 2.0})
    monitor.AddCheck(&DiskCheck{Path: "/data", MinFreeGB: 10.0})
    monitor.AddCheck(&ProcessCheck{ProcessName: "nginx"})
    monitor.AddCheck(&ProcessCheck{ProcessName: "postgres"})

    // CheckerFunc adapter — function as interface
    monitor.AddCheck(NewCheckerFunc("dns:google.com", func() error {
        time.Sleep(time.Duration(rand.Intn(30)) * time.Millisecond)
        return nil // simulated DNS resolution
    }))

    monitor.AddCheck(NewCheckerFunc("ntp:time.sync", func() error {
        time.Sleep(time.Duration(rand.Intn(15)) * time.Millisecond)
        drift := rand.Float64() * 2.0 // seconds
        if drift > 1.5 {
            return fmt.Errorf("NTP drift too high: %.2fs", drift)
        }
        return nil
    }))

    fmt.Printf("  Registered %d checks\n", len(monitor.checks))

    // --- 4. Run checks ---

    fmt.Println("\n── 4. Running Health Checks ──")

    results := monitor.RunAll()

    // --- 5. Display results (Formatter interface) ---

    fmt.Println("\n── 5. Results (TableFormatter) ──")
    fmt.Println(monitor.Report(results))

    // --- 6. Swap formatter (CompactFormatter) ---

    fmt.Println("── 6. Same Results (CompactFormatter) ──")
    compact := CompactFormatter{}
    fmt.Println(compact.Format(results))

    // --- 7. Show stored statuses ---

    fmt.Println("\n── 7. Stored Statuses (StatusStore) ──")

    allStatuses := store.All()
    for _, s := range allStatuses {
        status := "HEALTHY"
        if !s.Healthy {
            status = "FAILED "
        }
        fmt.Printf("  %-7s %-30s at %s\n",
            status, s.Name, s.Timestamp.Format("15:04:05.000"))
    }

    // --- 8. Show metrics ---

    fmt.Println("\n── 8. Metrics (MetricRecorder) ──")
    fmt.Println(metrics.Summary())

    // --- 9. Show interface composition ---

    fmt.Println("\n── 9. Interface Analysis ──")
    fmt.Println("  Interfaces used (all 1-2 methods):")
    fmt.Println("    Checker         — 1 method:  Check() error")
    fmt.Println("    Namer           — 1 method:  Name() string")
    fmt.Println("    NamedChecker    — composed:  Checker + Namer")
    fmt.Println("    Notifier        — 1 method:  Notify(subject, body) error")
    fmt.Println("    MetricRecorder  — 1 method:  Record(name, value, tags)")
    fmt.Println("    Logger          — 2 methods: Info(msg), Error(msg)")
    fmt.Println("    StatusStore     — 2 methods: SetStatus(), GetStatus()")
    fmt.Println("    Formatter       — 1 method:  Format(results) string")
    fmt.Println()
    fmt.Println("  Implementations per interface:")
    fmt.Println("    Checker:  TCPCheck, DiskCheck, ProcessCheck, CheckerFunc")
    fmt.Println("    Notifier: ConsoleNotifier, SlackNotifier, MultiNotifier")
    fmt.Println("    Logger:   ConsoleLogger (could add FileLogger, SyslogLogger)")
    fmt.Println("    Formatter: TableFormatter, CompactFormatter")
    fmt.Println()
    fmt.Println("  Key principle: Monitor.NewMonitor() accepts interfaces,")
    fmt.Println("  returns *Monitor (struct). The caller has full access.")
    fmt.Println("  Each mock needs only 1-2 methods — testing is trivial.")
}
```

**Output esperado (parcial):**
```
╔══════════════════════════════════════════════════════════╗
║   Modular Infrastructure Monitor — Small Interfaces     ║
╚══════════════════════════════════════════════════════════╝

── 4. Running Health Checks ──
  [INFO]  tcp://db-01.internal:5432 OK (23ms)
  [ERROR] tcp://redis-01.internal:6379 FAILED: connection refused: redis-01.internal:6379
  [NOTIFY:alert] Health Check Failed: tcp://redis-01.internal:6379 — connection refused...
  [SLACK:#ops-alerts] Health Check Failed: tcp://redis-01.internal:6379 — connection refused...
  ...

── 5. Results (TableFormatter) ──
  STATUS CHECK                DURATION  MESSAGE
  ────────────────────────────────────────────────────────────
  ✓ OK   tcp://db-01:5432         23ms  ok
  ✗ FAIL tcp://redis-01:6379      12ms  connection refused...
  ✓ OK   tcp://web-01:443         45ms  ok
  ✓ OK   disk:/var/log             8ms  ok
  ✓ OK   disk:/data               15ms  ok
  ✓ OK   proc:nginx                3ms  ok
  ✓ OK   proc:postgres             7ms  ok
  ✓ OK   dns:google.com           18ms  ok
  ✗ FAIL ntp:time.sync             9ms  NTP drift too high: 1.72s
  ────────────────────────────────────────────────────────────
  Total: 9 checks, 7 healthy, 2 failed

── 6. Same Results (CompactFormatter) ──
  [+tcp://db-01:5432] [-tcp://redis-01:6379] [+tcp://web-01:443] ...
```

---

## 15. Resumen

```
┌──────────────────────────────────────────────────────────────────────────┐
│  INTERFACES PEQUEÑAS — Resumen                                           │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Principio 1: Interfaces pequeñas (1-3 metodos)                          │
│  ├─ "The bigger the interface, the weaker the abstraction" — Rob Pike    │
│  ├─ Mas tipos las satisfacen → mas reutilizacion                         │
│  ├─ Mocks triviales → testing facil                                      │
│  ├─ Composicion por embedding → interfaces mayores cuando necesites      │
│  └─ Nombrar con sufijo -er: Reader, Writer, Checker, Deployer           │
│                                                                          │
│  Principio 2: Accept interfaces, return structs                          │
│  ├─ Parametros: interfaz minima (lo que NECESITAS)                       │
│  ├─ Retornos: tipo concreto (lo que OFRECES)                             │
│  ├─ El CALLER decide si quiere guardar como interfaz                     │
│  ├─ Excepcion: factory con multiples tipos concretos → retorna interfaz  │
│  └─ Excepcion: error siempre es interfaz y esta bien                     │
│                                                                          │
│  Principio 3: Define donde consumes, no donde produces                   │
│  ├─ La interfaz va en el paquete del CONSUMIDOR                          │
│  ├─ El productor exporta el struct                                       │
│  └─ Satisfaccion implicita conecta todo                                  │
│                                                                          │
│  Anti-patrones:                                                          │
│  ├─ Interfaz prematura (1 sola implementacion)                           │
│  ├─ Interfaz monolitica (10+ metodos, estilo Java)                       │
│  ├─ Interfaz en productor con implementacion unica                       │
│  ├─ Getters/setters como interfaz                                        │
│  └─ Interfaz "utility" que agrupa funciones sin relacion                 │
│                                                                          │
│  Proceso de diseño:                                                      │
│  1. Escribe con tipos concretos                                          │
│  2. Identifica que metodos REALMENTE usas                                │
│  3. Extrae interfaz minima en el paquete consumidor                      │
│  4. Componer interfaces solo cuando una funcion necesita varias          │
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: T02 - Composicion de interfaces — io.ReadWriter, io.ReadCloser, embedding de interfaces
