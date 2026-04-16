# Interfaces Implicitas — Satisfaccion sin Declaracion Explicita, Duck Typing Estatico

## 1. Introduccion

Las interfaces en Go son el mecanismo central de abstraccion y polimorfismo. Pero a diferencia de Java, C# o Rust, Go implementa **satisfaccion implicita**: un tipo satisface una interfaz simplemente teniendo los metodos correctos, sin declarar `implements`, `extends`, ni `impl Trait for T`. No se necesita ninguna relacion explicita entre el tipo y la interfaz.

Esta decision de diseno tiene consecuencias profundas: permite que paquetes independientes (que nunca se han visto) interoperen, elimina la necesidad de planificar jerarquias de tipos por adelantado, y fomenta interfaces pequeñas que son faciles de satisfacer accidentalmente. Go logra **duck typing** ("si camina como pato y grazna como pato, es un pato") pero con verificacion en **tiempo de compilacion**, no en runtime como Python o Ruby.

En SysAdmin/DevOps, las interfaces aparecen en: abstraer backends de storage (local, S3, GCS), intercambiar implementaciones de logging (slog, zap, zerolog), definir contratos para health checks, plugins de monitoring, adapters de configuracion (archivos, env vars, vault), y cualquier componente que necesite ser testeable mediante mocks.

```
┌─────────────────────────────────────────────────────────────────────┐
│                 INTERFACES EN GO — SATISFACCION IMPLICITA           │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Java/C#:       class Foo implements Bar { ... }                    │
│  Rust:          impl Bar for Foo { ... }                            │
│  Go:            // no se declara nada — si tiene los metodos, OK    │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │  type Writer interface {                                     │   │
│  │      Write(p []byte) (n int, err error)                      │   │
│  │  }                                                           │   │
│  └──────────────────────────────────────────────────────────────┘   │
│                        ▲                                            │
│                        │ satisface (implicito)                      │
│       ┌────────────────┼─────────────────────┐                      │
│       │                │                     │                      │
│  os.File         bytes.Buffer        net.Conn                       │
│  (tiene Write)   (tiene Write)       (tiene Write)                  │
│                                                                     │
│  Ninguno declara "implements io.Writer"                             │
│  Todos tienen el metodo correcto → satisfacen automaticamente       │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 2. Que es una Interfaz en Go

Una interfaz es un **tipo** que define un **conjunto de metodos** (method set). Cualquier tipo concreto cuyo method set incluya todos los metodos de la interfaz, la satisface automaticamente.

```go
// Declaracion de interfaz: solo firmas de metodos
type Stringer interface {
    String() string
}

// Un tipo concreto — NO declara "implements Stringer"
type IPAddress struct {
    Octets [4]byte
}

// Al tener este metodo, IPAddress satisface fmt.Stringer automaticamente
func (ip IPAddress) String() string {
    return fmt.Sprintf("%d.%d.%d.%d", ip.Octets[0], ip.Octets[1], ip.Octets[2], ip.Octets[3])
}
```

### Reglas fundamentales

```
┌─────────────────────────────────────────────────────────────────────┐
│  REGLAS DE SATISFACCION                                             │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  1. Solo metodos — no campos, no constructores, no constantes       │
│  2. Solo firmas — nombre + parametros + retornos deben coincidir    │
│  3. Todos los metodos — faltar uno = no satisface                   │
│  4. Sin keyword — no hay "implements", "satisfies", etc.            │
│  5. Verificacion en compilacion — no en runtime                     │
│  6. Un tipo puede satisfacer multiples interfaces simultaneamente   │
│  7. Una interfaz puede ser satisfecha por infinitos tipos           │
│  8. El paquete de la interfaz y el del tipo NO necesitan conocerse  │
│                                                                     │
│  Consecuencia: puedes definir una interfaz HOY que tipos escritos   │
│  AYER ya satisfacen, sin modificar esos tipos                       │
└─────────────────────────────────────────────────────────────────────┘
```

```go
package main

import "fmt"

// Mi interfaz personalizada
type Describable interface {
    Describe() string
}

// Tipo 1: servidor
type Server struct {
    Hostname string
    IP       string
    Port     int
}

func (s Server) Describe() string {
    return fmt.Sprintf("server %s (%s:%d)", s.Hostname, s.IP, s.Port)
}

// Tipo 2: servicio
type Service struct {
    Name    string
    Version string
    Status  string
}

func (svc Service) Describe() string {
    return fmt.Sprintf("service %s v%s [%s]", svc.Name, svc.Version, svc.Status)
}

// Tipo 3: network
type Network struct {
    CIDR string
    VLAN int
}

func (n Network) Describe() string {
    return fmt.Sprintf("network %s (VLAN %d)", n.CIDR, n.VLAN)
}

// Funcion polimorfica: acepta CUALQUIER Describable
func PrintInventory(items []Describable) {
    for i, item := range items {
        fmt.Printf("  [%d] %s\n", i+1, item.Describe())
    }
}

func main() {
    // Tres tipos diferentes, una interfaz comun
    inventory := []Describable{
        Server{Hostname: "web-01", IP: "10.0.1.10", Port: 443},
        Service{Name: "nginx", Version: "1.25.3", Status: "running"},
        Network{CIDR: "10.0.1.0/24", VLAN: 100},
        Server{Hostname: "db-01", IP: "10.0.2.20", Port: 5432},
    }
    PrintInventory(inventory)
}
```

---

## 3. Satisfaccion Implicita en Detalle

### El compilador verifica en el punto de uso

Go no verifica si un tipo satisface una interfaz en la declaracion del tipo — lo verifica **donde se usa el tipo como la interfaz** (asignacion, argumento de funcion, retorno, etc.):

```go
type Reader interface {
    Read(p []byte) (n int, err error)
}

type MyFile struct {
    path string
}

// Sin el metodo Read, MyFile NO satisface Reader
// Pero no hay error AQUI — Go no se queja en la declaracion

// El error aparece AQUI, al intentar USAR MyFile como Reader:
// var r Reader = MyFile{}  // ERROR: MyFile does not implement Reader
//                          //        (missing method Read)

// Agregar el metodo:
func (f MyFile) Read(p []byte) (int, error) {
    // implementacion...
    return 0, nil
}

// Ahora SI compila:
var r Reader = MyFile{path: "/etc/hosts"}  // OK
```

### Verificacion en compilacion vs runtime

```go
// COMPILACION: Go verifica que el tipo concreto tiene los metodos
var w io.Writer = os.Stdout        // OK: *os.File tiene Write()
var w2 io.Writer = "hello"         // ERROR: string no tiene Write()

// La verificacion es ESTATICA — errores antes de ejecutar
// A diferencia de Python:
//   def process(writer):
//       writer.write(data)  # falla en runtime si writer no tiene write()
```

### Compile-time assertion: verificar satisfaccion sin usar

A veces quieres asegurarte de que un tipo satisface una interfaz sin usarlo en ese punto. El patron idiomatico:

```go
// Verificacion en compilacion: Server debe satisfacer Describable
var _ Describable = Server{}     // value receiver → usa valor
var _ Describable = (*Server)(nil) // pointer receiver → usa puntero

// Si Server NO satisface Describable, este archivo no compila.
// El _ descarta el valor — solo queremos la verificacion del compilador.
```

Este patron es **muy comun** en codigo Go de produccion. Lo ves en la stdlib y en cualquier proyecto serio:

```go
// De net/http (verificacion real del stdlib):
// var _ Handler = HandlerFunc(nil)

// Patron SysAdmin: verificar que tu plugin satisface la interfaz del framework
type HealthChecker interface {
    Check() error
    Name() string
}

type PostgresChecker struct {
    DSN string
}

func (p *PostgresChecker) Check() error { /* ... */ return nil }
func (p *PostgresChecker) Name() string { return "postgres" }

// Compile-time assertion — falla si olvidamos un metodo
var _ HealthChecker = (*PostgresChecker)(nil)
```

---

## 4. Duck Typing Estatico

### Que significa "duck typing"

```
┌─────────────────────────────────────────────────────────────────────┐
│  "If it walks like a duck and quacks like a duck, it's a duck."    │
│                                                                     │
│  No me importa QUE TIPO eres.                                      │
│  Me importa QUE PUEDES HACER (que metodos tienes).                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Python/Ruby:   duck typing DINAMICO (runtime)                      │
│  │  → Si falla, panic/exception en runtime                          │
│  │  → No hay forma de saber antes de ejecutar                       │
│                                                                     │
│  Go:            duck typing ESTATICO (compilacion)                  │
│  │  → Si falla, error de compilacion                                │
│  │  → Imposible que falle en runtime*                               │
│  │  → (*con excepciones: type assertions, ver T02)                  │
│                                                                     │
│  Java/C#:       tipado NOMINAL (implements)                         │
│  │  → Declaracion explicita requerida                               │
│  │  → Tipo y interfaz deben conocerse mutuamente                    │
│                                                                     │
│  Rust:          tipado NOMINAL con generics (impl Trait for T)      │
│  │  → Declaracion explicita requerida                               │
│  │  → Pero bounds checkeados en compilacion                         │
└─────────────────────────────────────────────────────────────────────┘
```

### La ventaja del duck typing estatico

La ventaja clave es **desacoplamiento retroactivo**: puedes definir una interfaz que tipos existentes (incluso de la stdlib u otros paquetes) ya satisfacen, sin modificarlos:

```go
package mypackage

// Defino MI interfaz en MI paquete
type ConfigSource interface {
    ReadFile(name string) ([]byte, error)
}

// os.DirFS(dir) retorna un fs.FS, y cuando haces os.ReadFile(path)
// internamente usa os.File — pero eso no importa.
// Lo que importa es que puedo crear un mock:

type MockFS struct {
    Files map[string][]byte
}

func (m MockFS) ReadFile(name string) ([]byte, error) {
    data, ok := m.Files[name]
    if !ok {
        return nil, fmt.Errorf("file not found: %s", name)
    }
    return data, nil
}

// En produccion: paso el sistema de archivos real
// En tests: paso MockFS
// Ambos satisfacen ConfigSource sin declarar nada
```

### Desacoplamiento entre paquetes

```
                    package A                  package B
                    (define interfaz)          (define tipo)
                    
                    type Closer interface {    type DBConn struct { ... }
                        Close() error          func (d *DBConn) Close() error { ... }
                    }
                    
                    // A no importa B
                    // B no importa A
                    // Pero *DBConn satisface A.Closer
                    
                    package C
                    (usa ambos)
                    
                    import "A"
                    import "B"
                    
                    func Cleanup(c A.Closer) {
                        c.Close()
                    }
                    
                    func main() {
                        db := &B.DBConn{}
                        Cleanup(db)  // OK — Go verifica aqui
                    }
```

Esto es **imposible** en Java/C# sin que B declare `implements A.Closer`. En Go, es natural y comun.

---

## 5. Anatomia de un Valor de Interfaz (Interface Value)

Un valor de interfaz en Go es internamente un par `(tipo, valor)` — dos palabras de maquina (16 bytes en 64-bit):

```
┌────────────────────────────────────────────────────────────────────┐
│  INTERFACE VALUE — estructura interna                              │
├────────────────────────────────────────────────────────────────────┤
│                                                                    │
│  var w io.Writer = os.Stdout                                       │
│                                                                    │
│  ┌───────────────────────────┐                                     │
│  │ Interface value (16 bytes)│                                     │
│  ├───────────────────────────┤                                     │
│  │ tab  *itab ──────────────────→ ┌─────────────────────┐          │
│  │                           │    │ type: *os.File       │          │
│  │                           │    │ inter: io.Writer     │          │
│  │                           │    │ fun[0]: (*File).Write│          │
│  │                           │    └─────────────────────┘          │
│  ├───────────────────────────┤                                     │
│  │ data unsafe.Pointer ─────────→ ┌─────────────────────┐          │
│  │                           │    │ *os.File{fd: 1, ... }│         │
│  │                           │    └─────────────────────┘          │
│  └───────────────────────────┘                                     │
│                                                                    │
│  tab  = puntero a la "interface table":                            │
│         contiene el tipo concreto y los punteros a los metodos     │
│  data = puntero al valor concreto (o copia si es pequeño)          │
│                                                                    │
│  Interface nil:                                                    │
│  ┌───────────────────────────┐                                     │
│  │ tab  = nil                │  → tanto tipo como valor son nil    │
│  │ data = nil                │  → == nil es true                   │
│  └───────────────────────────┘                                     │
│                                                                    │
│  Interface con tipo pero valor nil (la trampa clasica):            │
│  ┌───────────────────────────┐                                     │
│  │ tab  = *MyType ≠ nil      │  → tiene tipo asignado             │
│  │ data = nil                │  → valor es nil                     │
│  └───────────────────────────┘  → == nil es FALSE!                 │
│                                    (se cubre en S02/T03)           │
└────────────────────────────────────────────────────────────────────┘
```

```go
package main

import (
    "fmt"
    "io"
    "os"
    "bytes"
)

func inspectInterface(name string, w io.Writer) {
    if w == nil {
        fmt.Printf("%-12s → nil interface\n", name)
        return
    }
    fmt.Printf("%-12s → type=%T, value=%v\n", name, w, w)
}

func main() {
    var w1 io.Writer                           // nil interface
    var w2 io.Writer = os.Stdout               // (*os.File, &{stdout})
    var w3 io.Writer = new(bytes.Buffer)       // (*bytes.Buffer, &{})
    var w4 io.Writer = (*os.File)(nil)         // (*os.File, nil) — NO es nil!

    inspectInterface("w1 (nil)", w1)
    inspectInterface("w2 (File)", w2)
    inspectInterface("w3 (Buffer)", w3)
    inspectInterface("w4 (trap!)", w4)

    fmt.Println("\nw1 == nil:", w1 == nil) // true
    fmt.Println("w4 == nil:", w4 == nil)   // false! (tiene tipo asignado)
}
```

**Output:**
```
w1 (nil)     → nil interface
w2 (File)    → type=*os.File, value=&{0xc00005e060}
w3 (Buffer)  → type=*bytes.Buffer, value=
w4 (trap!)   → type=*os.File, value=<nil>

w1 == nil: true
w4 == nil: false
```

---

## 6. Method Sets y Satisfaccion: Value vs Pointer Receiver

Una de las reglas mas importantes y confusas de Go: el **method set** de un tipo determina que interfaces satisface, y difiere entre `T` y `*T`.

```
┌────────────────────────────────────────────────────────────────────┐
│  METHOD SETS — Quien satisface que                                 │
├────────────────────────────────────────────────────────────────────┤
│                                                                    │
│  Tipo    │ Method set incluye                                      │
│  ────────┼─────────────────────────────────────────────────────    │
│  T       │ Solo metodos con value receiver: func (t T) M()        │
│  *T      │ AMBOS: func (t T) M() + func (t *T) M()               │
│                                                                    │
│  ¿Por que? Si tienes un valor T, Go no siempre puede              │
│  obtener su direccion (por ejemplo, valores en maps,              │
│  retornos de funciones). Pero si tienes *T, siempre               │
│  puedes obtener T con *ptr.                                       │
│                                                                    │
│  Regla practica:                                                   │
│  ├─ Si TODOS tus metodos son value receiver → T y *T satisfacen   │
│  ├─ Si ALGUNO es pointer receiver → solo *T satisface              │
│  └─ En SysAdmin: casi siempre usas pointer receiver (mutable)     │
│     → casi siempre pasas *T a interfaces                           │
└────────────────────────────────────────────────────────────────────┘
```

```go
package main

import "fmt"

type Logger interface {
    Log(msg string)
    SetLevel(level string)
}

type FileLogger struct {
    Path  string
    Level string
}

// Value receiver — disponible para FileLogger y *FileLogger
func (f FileLogger) Log(msg string) {
    fmt.Printf("[%s] %s → %s\n", f.Level, msg, f.Path)
}

// Pointer receiver — SOLO disponible para *FileLogger
func (f *FileLogger) SetLevel(level string) {
    f.Level = level
}

func main() {
    // *FileLogger satisface Logger ✓ (tiene ambos metodos)
    var log1 Logger = &FileLogger{Path: "/var/log/app.log", Level: "INFO"}
    log1.Log("server started")
    log1.SetLevel("DEBUG")
    log1.Log("debug message")

    // FileLogger NO satisface Logger ✗
    // var log2 Logger = FileLogger{Path: "/var/log/app.log", Level: "INFO"}
    // ERROR: FileLogger does not implement Logger
    //        (method SetLevel has pointer receiver)

    // Pero FileLogger SI funciona cuando llamas metodos directamente:
    fl := FileLogger{Path: "/var/log/app.log", Level: "INFO"}
    fl.SetLevel("WARN") // Go automaticamente toma &fl → OK
    fl.Log("direct call works")
    // La restriccion solo aplica para SATISFACCION DE INTERFACES
}
```

### Tabla de satisfaccion

```go
type OnlyValue interface {
    Get() string          // value receiver suficiente
}

type NeedsPointer interface {
    Get() string          // value receiver
    Set(s string)         // pointer receiver
}

type Config struct {
    Data string
}

func (c Config) Get() string     { return c.Data }
func (c *Config) Set(s string)   { c.Data = s }

// ┌──────────────┬──────────────────┬──────────────────┐
// │              │ OnlyValue        │ NeedsPointer     │
// ├──────────────┼──────────────────┼──────────────────┤
// │ Config       │ ✓ satisface      │ ✗ no satisface   │
// │ *Config      │ ✓ satisface      │ ✓ satisface      │
// └──────────────┴──────────────────┴──────────────────┘

var _ OnlyValue = Config{}           // ✓ OK
var _ OnlyValue = (*Config)(nil)     // ✓ OK
// var _ NeedsPointer = Config{}     // ✗ ERROR
var _ NeedsPointer = (*Config)(nil)  // ✓ OK
```

---

## 7. Un Tipo, Multiples Interfaces

Un tipo puede satisfacer tantas interfaces como quiera — siempre que tenga todos los metodos de cada una. No hay conflicto ni ambiguedad:

```go
package main

import (
    "encoding/json"
    "fmt"
)

// Interfaz 1: para logging
type Stringer interface {
    String() string
}

// Interfaz 2: para serializacion
type JSONMarshaler interface {
    MarshalJSON() ([]byte, error)
}

// Interfaz 3: para validacion
type Validator interface {
    Validate() error
}

// Interfaz 4: para lifecycle
type Closer interface {
    Close() error
}

// Un solo tipo satisface las 4 interfaces simultaneamente
type DatabaseConn struct {
    Host     string
    Port     int
    Database string
    pool     int // conexiones activas
}

func (d DatabaseConn) String() string {
    return fmt.Sprintf("%s:%d/%s", d.Host, d.Port, d.Database)
}

func (d DatabaseConn) MarshalJSON() ([]byte, error) {
    // Redact pool internals
    return json.Marshal(struct {
        Host     string `json:"host"`
        Port     int    `json:"port"`
        Database string `json:"database"`
    }{d.Host, d.Port, d.Database})
}

func (d DatabaseConn) Validate() error {
    if d.Host == "" {
        return fmt.Errorf("host is required")
    }
    if d.Port < 1 || d.Port > 65535 {
        return fmt.Errorf("invalid port: %d", d.Port)
    }
    return nil
}

func (d *DatabaseConn) Close() error {
    fmt.Printf("closing %d connections to %s\n", d.pool, d.Host)
    d.pool = 0
    return nil
}

// Cada funcion solo ve los metodos de SU interfaz
func LogItem(s Stringer) {
    fmt.Println("LOG:", s.String())
}

func SerializeItem(m JSONMarshaler) {
    data, _ := m.MarshalJSON()
    fmt.Println("JSON:", string(data))
}

func ValidateItem(v Validator) error {
    return v.Validate()
}

func CleanupItem(c Closer) {
    c.Close()
}

func main() {
    db := &DatabaseConn{
        Host:     "db-primary.internal",
        Port:     5432,
        Database: "inventory",
        pool:     25,
    }

    // El mismo objeto, vista diferente segun la interfaz
    LogItem(db)            // ve solo String()
    SerializeItem(db)      // ve solo MarshalJSON()
    if err := ValidateItem(db); err != nil {
        fmt.Println("INVALID:", err)
    }
    CleanupItem(db)        // ve solo Close()
}
```

---

## 8. Interfaces y el Principio de Definirlas Donde se Consumen

Go tiene una filosofia opuesta a Java: **define la interfaz donde la usas (consumidor), no donde la implementas (productor)**.

```
┌────────────────────────────────────────────────────────────────────┐
│  JAVA style:                                                       │
│  ┌──────────┐     ┌──────────┐     ┌──────────┐                   │
│  │ package A │     │ package B │     │ package C │                  │
│  │           │     │           │     │           │                  │
│  │ interface │◄────│ class Foo │     │ class Bar │                  │
│  │ Service   │     │ implements│     │ implements│                  │
│  │           │     │ Service   │     │ Service   │                  │
│  └──────────┘     └──────────┘     └──────────┘                   │
│  A define la interfaz, B y C la importan para declarar implements  │
│  → B y C DEPENDEN de A                                             │
│                                                                    │
│  GO style:                                                         │
│  ┌──────────┐     ┌──────────┐     ┌──────────┐                   │
│  │ package A │     │ package B │     │ package C │                  │
│  │           │     │           │     │           │                  │
│  │ type Foo  │     │ interface │     │ interface │                  │
│  │  .Do()    │     │ Doer {    │     │ Doer {    │                  │
│  │           │     │   Do()   │     │   Do()   │                  │
│  │           │     │ }         │     │ }         │                  │
│  └──────────┘     └──────────┘     └──────────┘                   │
│  A no sabe que interfaces existen.                                 │
│  B y C definen SUS PROPIAS interfaces con los metodos que ELLOS    │
│  necesitan. A satisface ambas sin importar ninguna.                │
│  → A NO depende de B ni C                                          │
│  → B y C NO dependen entre si                                      │
│  → Dependencias invertidas naturalmente                            │
└────────────────────────────────────────────────────────────────────┘
```

```go
// package deployer — define su propia interfaz con lo que necesita
package deployer

// Solo necesita Exec — no importa de donde venga
type CommandRunner interface {
    Exec(cmd string, args ...string) ([]byte, error)
}

func Deploy(runner CommandRunner, image string) error {
    out, err := runner.Exec("docker", "pull", image)
    if err != nil {
        return fmt.Errorf("pull failed: %w", err)
    }
    fmt.Println(string(out))

    out, err = runner.Exec("docker", "run", "-d", image)
    if err != nil {
        return fmt.Errorf("run failed: %w", err)
    }
    fmt.Printf("container: %s\n", string(out))
    return nil
}

// package ssh — no conoce package deployer
package ssh

type Client struct {
    Host string
    Port int
}

func (c *Client) Exec(cmd string, args ...string) ([]byte, error) {
    // Ejecuta via SSH...
    return nil, nil
}
// *ssh.Client satisface deployer.CommandRunner sin saberlo

// package local — tampoco conoce package deployer
package local

type Shell struct{}

func (s Shell) Exec(cmd string, args ...string) ([]byte, error) {
    return exec.Command(cmd, args...).CombinedOutput()
}
// local.Shell satisface deployer.CommandRunner sin saberlo
```

### Regla practica para SysAdmin

```
┌───────────────────────────────────────────────────────┐
│  DONDE DEFINIR INTERFACES                             │
├───────────────────────────────────────────────────────┤
│                                                       │
│  ✓ En el paquete que CONSUME la interfaz              │
│    (deployer define CommandRunner)                    │
│                                                       │
│  ✗ En el paquete que IMPLEMENTA el tipo concreto      │
│    (ssh NO deberia definir CommandRunner)             │
│                                                       │
│  ✗ En un paquete "interfaces" central                 │
│    (anti-pattern Java)                                │
│                                                       │
│  Excepcion: interfaces de la stdlib que son           │
│  universales (io.Reader, io.Writer, fmt.Stringer,     │
│  error) — estas SI se definen en el productor         │
│  porque son tan generales que todos las usan          │
└───────────────────────────────────────────────────────┘
```

---

## 9. Satisfaccion Retroactiva — Interfaces para Tipos Existentes

La satisfaccion implicita permite crear interfaces que tipos **ya existentes** satisfacen — sin modificar esos tipos. Esto es imposible en Java/Rust.

```go
package main

import (
    "fmt"
    "os"
    "bytes"
    "strings"
    "net/http"
)

// Defino UNA interfaz que multiples tipos de la stdlib YA satisfacen
type ByteReader interface {
    Read(p []byte) (n int, err error)
}

// Todos estos tipos ya existian antes de mi interfaz:
func demo() {
    var readers []ByteReader

    // os.File — satisface ByteReader (tiene Read)
    f, _ := os.Open("/etc/hostname")
    readers = append(readers, f)

    // bytes.Buffer — satisface ByteReader (tiene Read)
    buf := bytes.NewBufferString("hello from buffer")
    readers = append(readers, buf)

    // strings.Reader — satisface ByteReader (tiene Read)
    sr := strings.NewReader("hello from strings")
    readers = append(readers, sr)

    // http.Response.Body — satisface ByteReader (tiene Read)
    resp, _ := http.Get("https://example.com")
    if resp != nil {
        readers = append(readers, resp.Body)
    }

    // Proceso uniforme — no importa el origen
    for _, r := range readers {
        buf := make([]byte, 64)
        n, _ := r.Read(buf)
        fmt.Printf("  [%T] %q\n", r, string(buf[:n]))
    }
}
```

### Patron SysAdmin: wrapper retroactivo

```go
// Quiero auditar todas las operaciones de escritura sin modificar
// los tipos existentes. Creo una interfaz que os.File ya satisface:

type AuditableWriter interface {
    Write(p []byte) (n int, err error)
    Name() string  // os.File tiene Name()
}

// os.File satisface AuditableWriter sin modificacion!
// Tiene Write() y Name()

type AuditLogger struct {
    target AuditableWriter
}

func (a *AuditLogger) Write(p []byte) (int, error) {
    n, err := a.target.Write(p)
    fmt.Printf("[AUDIT] wrote %d bytes to %s\n", n, a.target.Name())
    return n, err
}

func main() {
    f, _ := os.Create("/tmp/audit-test.txt")
    defer f.Close()

    audited := &AuditLogger{target: f} // os.File satisface AuditableWriter
    fmt.Fprintln(audited, "hello, audited world")
}
```

---

## 10. Comparacion con C y Rust

```
┌────────────────────────────────────────────────────────────────────────┐
│  POLIMORFISMO — Go vs C vs Rust                                        │
├──────────────┬──────────────────┬─────────────────┬────────────────────┤
│ Aspecto      │ Go               │ C               │ Rust               │
├──────────────┼──────────────────┼─────────────────┼────────────────────┤
│ Mecanismo    │ Interfaces       │ Function ptrs   │ Traits             │
│              │ (method sets)    │ (void*, vtable  │ (impl blocks)      │
│              │                  │  manual)        │                    │
├──────────────┼──────────────────┼─────────────────┼────────────────────┤
│ Declaracion  │ Implicita        │ Manual (cast    │ Explicita          │
│ de implem.   │ (sin keyword)    │  a void*)       │ (impl Trait for T) │
├──────────────┼──────────────────┼─────────────────┼────────────────────┤
│ Verificacion │ Compilacion      │ Ninguna (void*) │ Compilacion        │
│              │ (en punto de uso)│                 │ (en impl + uso)    │
├──────────────┼──────────────────┼─────────────────┼────────────────────┤
│ Dispatch     │ Dynamic (vtable) │ Dynamic (fptr)  │ Static (generics)  │
│              │ en runtime       │ en runtime      │ o dynamic (dyn)    │
├──────────────┼──────────────────┼─────────────────┼────────────────────┤
│ Retroactiva  │ SI — cualquier   │ N/A — no hay    │ Solo con newtype   │
│              │ tipo existente   │ sistema formal  │ o blanket impl     │
├──────────────┼──────────────────┼─────────────────┼────────────────────┤
│ Coste        │ 16 bytes + 1     │ sizeof(void*)   │ 0 (static) o      │
│ runtime      │ indirection      │ + 1 indir.      │ 16 bytes (dyn)     │
├──────────────┼──────────────────┼─────────────────┼────────────────────┤
│ Seguridad    │ Type-safe        │ Unsafe (void*)  │ Type-safe + borrow │
│              │                  │                 │ checker             │
└──────────────┴──────────────────┴─────────────────┴────────────────────┘
```

### Ejemplo comparativo: "loggable" en los tres lenguajes

**C — manual, unsafe:**
```c
// C: polimorfismo con punteros a funcion
typedef struct {
    const char* (*describe)(void* self);
    void (*close)(void* self);
} Loggable;

typedef struct {
    char hostname[64];
    int port;
} Server;

const char* server_describe(void* self) {
    Server* s = (Server*)self;  // cast manual, sin verificacion
    static char buf[128];
    snprintf(buf, sizeof(buf), "%s:%d", s->hostname, s->port);
    return buf;
}

void server_close(void* self) { /* cleanup */ }

// Crear "interfaz" manualmente:
Loggable server_loggable = {
    .describe = server_describe,
    .close = server_close,
};

void log_item(Loggable* l, void* obj) {
    printf("LOG: %s\n", l->describe(obj));
}
```

**Rust — explicita, type-safe:**
```rust
// Rust: trait con impl explicito
trait Loggable {
    fn describe(&self) -> String;
    fn close(&mut self);
}

struct Server {
    hostname: String,
    port: u16,
}

// DEBE declarar explicitamente: impl Trait for Type
impl Loggable for Server {
    fn describe(&self) -> String {
        format!("{}:{}", self.hostname, self.port)
    }
    fn close(&mut self) { /* cleanup */ }
}

// Static dispatch (sin coste runtime, monomorphizado):
fn log_item(item: &impl Loggable) {
    println!("LOG: {}", item.describe());
}

// Dynamic dispatch (equivalente a Go):
fn log_item_dyn(item: &dyn Loggable) {
    println!("LOG: {}", item.describe());
}
```

**Go — implicita, type-safe:**
```go
// Go: interfaz con satisfaccion implicita
type Loggable interface {
    Describe() string
    Close() error
}

type Server struct {
    Hostname string
    Port     int
}

// NO declara "implements Loggable" — simplemente tiene los metodos
func (s Server) Describe() string {
    return fmt.Sprintf("%s:%d", s.Hostname, s.Port)
}

func (s *Server) Close() error { /* cleanup */ return nil }

// Siempre dynamic dispatch (no hay generics para esto pre-1.18)
func LogItem(item Loggable) {
    fmt.Println("LOG:", item.Describe())
}
```

---

## 11. Patrones Idiomaticos con Interfaces Implicitas

### Patron 1: Interface minima para testing

```go
// En produccion tu funcion necesita *http.Client, que tiene 20+ metodos.
// Para testing, define una interfaz con SOLO lo que usas:

// Produccion: http.Client.Do() hace requests
// Test: mock solo necesita Do()

type HTTPDoer interface {
    Do(req *http.Request) (*http.Response, error)
}

// http.Client ya satisface HTTPDoer (tiene Do)!
// Tu mock tambien:

type MockHTTPClient struct {
    Response *http.Response
    Err      error
}

func (m *MockHTTPClient) Do(req *http.Request) (*http.Response, error) {
    return m.Response, m.Err
}

// Funcion que acepta la interfaz minima
func FetchServiceStatus(client HTTPDoer, url string) (int, error) {
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
```

### Patron 2: Adapter con interfaz retroactiva

```go
// Quiero unificar diferentes fuentes de configuracion
// bajo una interfaz comun:

type ConfigLoader interface {
    Load(key string) (string, error)
}

// Adapter para os.Getenv — convierte una funcion en interfaz
type EnvLoader struct{}

func (e EnvLoader) Load(key string) (string, error) {
    val := os.Getenv(key)
    if val == "" {
        return "", fmt.Errorf("env var %s not set", key)
    }
    return val, nil
}

// Adapter para map (configuracion en memoria)
type MapLoader map[string]string

func (m MapLoader) Load(key string) (string, error) {
    val, ok := m[key]
    if !ok {
        return "", fmt.Errorf("key %s not found", key)
    }
    return val, nil
}

// Adapter para archivos (un archivo = un valor)
type FileLoader struct {
    Dir string
}

func (f FileLoader) Load(key string) (string, error) {
    data, err := os.ReadFile(filepath.Join(f.Dir, key))
    if err != nil {
        return "", err
    }
    return strings.TrimSpace(string(data)), nil
}

// Todas satisfacen ConfigLoader — la funcion no sabe de donde viene:
func GetConfig(loader ConfigLoader, keys []string) map[string]string {
    result := make(map[string]string, len(keys))
    for _, key := range keys {
        if val, err := loader.Load(key); err == nil {
            result[key] = val
        }
    }
    return result
}
```

### Patron 3: Strategy pattern sin "pattern"

```go
// En Go el strategy pattern es tan natural que no necesita nombre.
// Es simplemente una funcion que acepta una interfaz.

type BackupStrategy interface {
    Backup(source, dest string) error
    Name() string
}

type RsyncBackup struct {
    Flags []string // e.g., -avz --delete
}

func (r RsyncBackup) Backup(source, dest string) error {
    args := append(r.Flags, source, dest)
    cmd := exec.Command("rsync", args...)
    return cmd.Run()
}

func (r RsyncBackup) Name() string { return "rsync" }

type TarBackup struct {
    Compress bool
}

func (t TarBackup) Backup(source, dest string) error {
    flags := "-cf"
    if t.Compress {
        flags = "-czf"
    }
    return exec.Command("tar", flags, dest, source).Run()
}

func (t TarBackup) Name() string { return "tar" }

type ResticBackup struct {
    Repo     string
    Password string
}

func (r ResticBackup) Backup(source, dest string) error {
    cmd := exec.Command("restic", "-r", r.Repo, "backup", source)
    cmd.Env = append(os.Environ(), "RESTIC_PASSWORD="+r.Password)
    return cmd.Run()
}

func (r ResticBackup) Name() string { return "restic" }

// Uso: cambiar estrategia sin cambiar la logica
func RunBackup(strategy BackupStrategy, source, dest string) {
    fmt.Printf("Starting backup with %s: %s → %s\n", strategy.Name(), source, dest)
    if err := strategy.Backup(source, dest); err != nil {
        fmt.Printf("ERROR: %s backup failed: %v\n", strategy.Name(), err)
        return
    }
    fmt.Printf("OK: %s backup completed\n", strategy.Name())
}
```

### Patron 4: Optional interface (feature detection)

```go
// Algunos tipos pueden tener capacidades opcionales.
// Verificamos con type assertion en runtime:

type Printer interface {
    Print() string
}

// Interfaz opcional — no todos la implementan
type ColorPrinter interface {
    PrintColor() string
}

type PlainLog struct {
    Message string
}

func (p PlainLog) Print() string {
    return p.Message
}

type ColorLog struct {
    Message string
    Color   string
}

func (c ColorLog) Print() string {
    return c.Message
}

func (c ColorLog) PrintColor() string {
    return fmt.Sprintf("\033[%sm%s\033[0m", c.Color, c.Message)
}

func Output(p Printer, useColor bool) string {
    // Feature detection: ¿este Printer tambien sabe imprimir en color?
    if useColor {
        if cp, ok := p.(ColorPrinter); ok {
            return cp.PrintColor() // tiene la capacidad extra
        }
    }
    return p.Print() // fallback al metodo basico
}
```

Este patron se usa en la stdlib — por ejemplo, `io.WriterTo`:

```go
// io.Copy verifica si el Reader implementa WriterTo para optimizar:
// if wt, ok := src.(WriterTo); ok {
//     return wt.WriteTo(dst)
// }
```

---

## 12. Errores Comunes con Interfaces

### Error 1: Pointer receiver y satisfaccion

```go
type Shutdowner interface {
    Shutdown() error
}

type Service struct {
    Name string
    pid  int
}

// Pointer receiver
func (s *Service) Shutdown() error {
    fmt.Printf("Shutting down %s (pid %d)\n", s.Name, s.pid)
    return nil
}

func StopAll(services []Shutdowner) {
    for _, s := range services {
        s.Shutdown()
    }
}

func main() {
    // ✗ INCORRECTO — Service (valor) no satisface Shutdowner
    // services := []Shutdowner{
    //     Service{Name: "web", pid: 1234},  // ERROR
    // }

    // ✓ CORRECTO — *Service (puntero) satisface Shutdowner
    services := []Shutdowner{
        &Service{Name: "web", pid: 1234},     // &Service
        &Service{Name: "worker", pid: 5678},
    }
    StopAll(services)
}
```

### Error 2: Interfaz demasiado grande

```go
// ✗ ANTI-PATRON: interfaz gigante estilo Java
type InfrastructureManager interface {
    CreateServer(name string) error
    DeleteServer(name string) error
    ListServers() ([]Server, error)
    CreateNetwork(cidr string) error
    DeleteNetwork(name string) error
    ListNetworks() ([]Network, error)
    CreateVolume(size int) error
    DeleteVolume(name string) error
    DeployApp(name string, image string) error
    ScaleApp(name string, replicas int) error
    GetLogs(name string) ([]string, error)
    // ... 20 metodos mas
}
// ¿Quien va a implementar TODOS estos metodos? Solo tu proveedor de cloud.
// ¿Quien va a hacer un mock? Nadie — es demasiado trabajo.

// ✓ GO IDIOMATICO: interfaces pequeñas y especificas
type ServerCreator interface {
    CreateServer(name string) error
}

type ServerLister interface {
    ListServers() ([]Server, error)
}

type Deployer interface {
    DeployApp(name string, image string) error
}

// Cada funcion pide SOLO lo que necesita:
func ProvisionFleet(creator ServerCreator, count int) error { /* ... */ }
func InventoryReport(lister ServerLister) error { /* ... */ }
func RollOut(d Deployer, image string) error { /* ... */ }
```

### Error 3: Exportar tipo concreto + interfaz innecesaria

```go
// ✗ Si tu paquete exporta el struct Y la interfaz, ¿para que la interfaz?
package mydb

type Database interface {  // innecesaria si solo hay UNA implementacion
    Query(sql string) ([]Row, error)
    Close() error
}

type PostgresDB struct { /* ... */ }
func (p *PostgresDB) Query(sql string) ([]Row, error) { /* ... */ }
func (p *PostgresDB) Close() error { /* ... */ }

// ✓ MEJOR: exporta solo el struct. El CONSUMIDOR definira su propia interfaz
// si necesita mockear o abstraer.
package mydb

type PostgresDB struct { /* ... */ }
func (p *PostgresDB) Query(sql string) ([]Row, error) { /* ... */ }
func (p *PostgresDB) Close() error { /* ... */ }

// El consumidor (otro paquete) define lo que necesita:
package api

type Querier interface {
    Query(sql string) ([]mydb.Row, error)
}
// *mydb.PostgresDB satisface Querier automaticamente
```

### Error 4: Confundir satisfaccion con compatibilidad semantica

```go
// Dos tipos pueden satisfacer la misma interfaz pero tener
// semantica completamente diferente

type Sizer interface {
    Size() int64
}

// Para un archivo: Size() = bytes en disco
type File struct { /* ... */ }
func (f File) Size() int64 { return f.sizeBytes }

// Para una cola: Size() = numero de elementos
type Queue struct { /* ... */ }
func (q Queue) Size() int64 { return int64(len(q.items)) }

// Ambos satisfacen Sizer, pero Size() significa cosas diferentes.
// El compilador no puede verificar semantica — solo firmas.
// La documentacion de la interfaz debe ser clara sobre el contrato.
```

---

## 13. Interfaces en la Stdlib — Patrones del Mundo Real

### fmt.Stringer — la interfaz mas ubicua

```go
// Definida en fmt:
// type Stringer interface {
//     String() string
// }

// Si tu tipo tiene String(), fmt.Println lo usa automaticamente:

type CIDR struct {
    Network [4]byte
    Mask    int
}

func (c CIDR) String() string {
    return fmt.Sprintf("%d.%d.%d.%d/%d",
        c.Network[0], c.Network[1], c.Network[2], c.Network[3], c.Mask)
}

func main() {
    net := CIDR{Network: [4]byte{10, 0, 1, 0}, Mask: 24}
    fmt.Println(net)         // 10.0.1.0/24 — usa String() automaticamente
    fmt.Printf("Red: %s\n", net)  // tambien
    fmt.Printf("Verb v: %v\n", net) // tambien
}
```

### io.Reader / io.Writer — la abstraccion central

```go
// type Reader interface {
//     Read(p []byte) (n int, err error)
// }
// type Writer interface {
//     Write(p []byte) (n int, err error)
// }

// CUALQUIER cosa que lea bytes satisface Reader:
// os.File, net.Conn, http.Response.Body, bytes.Buffer,
// strings.Reader, compress/gzip.Reader, crypto/tls.Conn...

// CUALQUIER cosa que escriba bytes satisface Writer:
// os.File, net.Conn, http.ResponseWriter, bytes.Buffer,
// compress/gzip.Writer, bufio.Writer, os.Stdout...

// Por eso io.Copy funciona con CUALQUIER combinacion:
func BackupFile(src, dst string) error {
    in, err := os.Open(src)
    if err != nil {
        return err
    }
    defer in.Close()

    out, err := os.Create(dst)
    if err != nil {
        return err
    }
    defer out.Close()

    _, err = io.Copy(out, in)  // Writer ← Reader, no importa que tipos
    return err
}
```

### error — la interfaz mas simple

```go
// type error interface {
//     Error() string
// }

// CUALQUIER tipo con Error() string es un error en Go.
// Esto permite errores custom con campos extra:

type DeployError struct {
    Service  string
    Host     string
    ExitCode int
    Stderr   string
}

func (e *DeployError) Error() string {
    return fmt.Sprintf("deploy %s on %s failed (exit %d): %s",
        e.Service, e.Host, e.ExitCode, e.Stderr)
}

// Es un error normal:
func deploy(service, host string) error {
    // ...
    return &DeployError{
        Service:  service,
        Host:     host,
        ExitCode: 137,
        Stderr:   "OOMKilled",
    }
}
```

### sort.Interface — interfaz de 3 metodos

```go
// type Interface interface {
//     Len() int
//     Less(i, j int) bool
//     Swap(i, j int)
// }

type ServerList []Server

func (s ServerList) Len() int           { return len(s) }
func (s ServerList) Less(i, j int) bool { return s[i].CPU < s[j].CPU }
func (s ServerList) Swap(i, j int)      { s[i], s[j] = s[j], s[i] }

// Ahora sort.Sort funciona:
func main() {
    servers := ServerList{
        {Name: "web-01", CPU: 85.2},
        {Name: "web-02", CPU: 12.1},
        {Name: "web-03", CPU: 67.8},
    }
    sort.Sort(servers)
    // Ordenados por CPU ascendente
}
// Nota: sort.Slice es mas simple para casos ad-hoc (ver C05/S01/T04)
```

---

## 14. Ejercicios de Prediccion

### Ejercicio 1: ¿Compila o no?

```go
type Pinger interface {
    Ping() error
}

type Host struct {
    IP string
}

func (h *Host) Ping() error {
    return nil
}

func PingAll(targets []Pinger) {
    for _, t := range targets {
        t.Ping()
    }
}

func main() {
    hosts := []Pinger{
        Host{IP: "10.0.1.1"},      // linea A
        &Host{IP: "10.0.1.2"},     // linea B
    }
    PingAll(hosts)
}
```

<details>
<summary>Respuesta</summary>

**No compila.** La linea A es un error: `Host does not implement Pinger (method Ping has pointer receiver)`. Solo `*Host` tiene el method set completo (incluye pointer receivers). La linea B es correcta. Solucion: usar `&Host{...}` en la linea A tambien.

</details>

### Ejercicio 2: ¿Que imprime?

```go
type Namer interface {
    Name() string
}

type Service struct {
    SvcName string
}

func (s Service) Name() string {
    return s.SvcName
}

type Container struct {
    CtrName string
}

func (c *Container) Name() string {
    return c.CtrName
}

func main() {
    var n1 Namer = Service{SvcName: "web"}
    var n2 Namer = &Service{SvcName: "api"}
    var n3 Namer = &Container{CtrName: "nginx"}

    fmt.Println(n1.Name())
    fmt.Println(n2.Name())
    fmt.Println(n3.Name())
    fmt.Printf("%T\n", n1)
    fmt.Printf("%T\n", n2)
    fmt.Printf("%T\n", n3)
}
```

<details>
<summary>Respuesta</summary>

```
web
api
nginx
main.Service
*main.Service
*main.Container
```

- `n1`: Service (valor) satisface Namer porque Name() es value receiver. Tipo dinamico: `main.Service`
- `n2`: *Service tambien satisface. Tipo dinamico: `*main.Service`
- `n3`: Solo *Container satisface (pointer receiver). Tipo dinamico: `*main.Container`
- `Container{}` sin `&` no compilaria

</details>

### Ejercicio 3: ¿Es nil?

```go
type Checker interface {
    Check() error
}

type DiskChecker struct {
    Path string
}

func (d *DiskChecker) Check() error {
    return nil
}

func GetChecker(enabled bool) Checker {
    if !enabled {
        var d *DiskChecker  // nil *DiskChecker
        return d            // ← ¿que retorna?
    }
    return &DiskChecker{Path: "/"}
}

func main() {
    c := GetChecker(false)
    if c == nil {
        fmt.Println("nil checker")
    } else {
        fmt.Println("checker exists:", c.Check())
    }
}
```

<details>
<summary>Respuesta</summary>

Imprime: `checker exists: <nil>`

`GetChecker(false)` retorna una interfaz con tipo `*DiskChecker` y valor `nil`. Como la interfaz tiene un tipo asignado (su campo `tab` no es nil), `c == nil` es **false**. El metodo `Check()` se ejecuta correctamente con nil receiver y retorna nil. Esta es la **clasica trampa de nil interface** que se cubre en profundidad en S02/T03.

**Fix correcto:**
```go
func GetChecker(enabled bool) Checker {
    if !enabled {
        return nil  // retorna nil INTERFACE, no nil *DiskChecker
    }
    return &DiskChecker{Path: "/"}
}
```

</details>

### Ejercicio 4: ¿Cuantas interfaces satisface?

```go
type A interface { M1() }
type B interface { M2() }
type C interface { M1(); M2() }
type D interface { M1(); M2(); M3() }

type X struct{}

func (X) M1() {}
func (X) M2() {}
```

<details>
<summary>Respuesta</summary>

`X` satisface **3 interfaces**: A, B y C. No satisface D porque le falta M3(). Ademas, satisface la interfaz vacia `any` (todo la satisface) y cualquier otra interfaz cuyos metodos sean un subconjunto de {M1, M2}.

</details>

### Ejercicio 5: ¿Compila?

```go
type Reader interface {
    Read(p []byte) (int, error)
}

type Writer interface {
    Write(p []byte) (int, error)
}

type ReadWriter interface {
    Reader
    Writer
}

type MyBuffer struct {
    data []byte
}

func (b *MyBuffer) Read(p []byte) (int, error) {
    n := copy(p, b.data)
    return n, nil
}

func (b *MyBuffer) Write(p []byte) (int, error) {
    b.data = append(b.data, p...)
    return len(p), nil
}

func Process(rw ReadWriter) {
    rw.Write([]byte("hello"))
    buf := make([]byte, 5)
    rw.Read(buf)
}

func main() {
    buf := &MyBuffer{}
    Process(buf)        // linea A
    Process(MyBuffer{}) // linea B
}
```

<details>
<summary>Respuesta</summary>

La linea A compila. La linea B **no compila**: `MyBuffer does not implement ReadWriter` porque tanto Read como Write tienen pointer receiver, asi que solo `*MyBuffer` satisface la interfaz. ReadWriter embebe Reader y Writer, y para satisfacerla hay que satisfacer AMBAS — se necesitan Read Y Write, ambos con pointer receiver → solo `*MyBuffer` tiene ambos en su method set.

</details>

---

## 15. Programa Completo: Infrastructure Service Mesh con Interfaces Implicitas

```go
// infrastructure_mesh.go
// Demuestra duck typing estatico con interfaces implicitas
// para construir un service mesh simplificado de infraestructura.
//
// Cada componente (backend, monitor, logger, health checker)
// satisface interfaces sin declararlas — la composicion se
// logra por comportamiento, no por jerarquia.
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
// INTERFACES — definidas donde se CONSUMEN, no donde se implementan
// =====================================================================

// ServiceDiscovery — lo que el mesh necesita para encontrar servicios
type ServiceDiscovery interface {
    Discover(serviceName string) ([]Endpoint, error)
    Register(ep Endpoint) error
    Deregister(id string) error
}

// HealthChecker — lo que el mesh necesita para verificar salud
type HealthChecker interface {
    Check(ep Endpoint) HealthStatus
    Name() string
}

// LoadBalancer — lo que el mesh necesita para elegir un endpoint
type LoadBalancer interface {
    Select(endpoints []Endpoint) (Endpoint, error)
    Name() string
}

// MetricsCollector — lo que el mesh necesita para reportar metricas
type MetricsCollector interface {
    Record(metric string, value float64, tags map[string]string)
    Summary() string
}

// Logger — lo que el mesh necesita para loguear
type MeshLogger interface {
    Info(msg string, fields map[string]string)
    Error(msg string, err error, fields map[string]string)
}

// =====================================================================
// TIPOS DE DATO
// =====================================================================

type Endpoint struct {
    ID      string
    Service string
    Host    string
    Port    int
    Tags    map[string]string
}

func (e Endpoint) Addr() string {
    return fmt.Sprintf("%s:%d", e.Host, e.Port)
}

func (e Endpoint) String() string {
    return fmt.Sprintf("[%s] %s @ %s", e.ID, e.Service, e.Addr())
}

type HealthStatus int

const (
    StatusHealthy   HealthStatus = iota
    StatusDegraded
    StatusUnhealthy
    StatusUnknown
)

func (s HealthStatus) String() string {
    switch s {
    case StatusHealthy:
        return "healthy"
    case StatusDegraded:
        return "degraded"
    case StatusUnhealthy:
        return "unhealthy"
    default:
        return "unknown"
    }
}

// =====================================================================
// IMPLEMENTACION 1: InMemoryRegistry
// Satisface ServiceDiscovery — sin declarar implements
// =====================================================================

type InMemoryRegistry struct {
    mu        sync.RWMutex
    endpoints map[string]Endpoint // id → endpoint
}

func NewInMemoryRegistry() *InMemoryRegistry {
    return &InMemoryRegistry{
        endpoints: make(map[string]Endpoint),
    }
}

func (r *InMemoryRegistry) Register(ep Endpoint) error {
    r.mu.Lock()
    defer r.mu.Unlock()

    if ep.ID == "" {
        return fmt.Errorf("endpoint ID required")
    }
    if _, exists := r.endpoints[ep.ID]; exists {
        return fmt.Errorf("endpoint %s already registered", ep.ID)
    }
    r.endpoints[ep.ID] = ep
    return nil
}

func (r *InMemoryRegistry) Deregister(id string) error {
    r.mu.Lock()
    defer r.mu.Unlock()

    if _, exists := r.endpoints[id]; !exists {
        return fmt.Errorf("endpoint %s not found", id)
    }
    delete(r.endpoints, id)
    return nil
}

func (r *InMemoryRegistry) Discover(serviceName string) ([]Endpoint, error) {
    r.mu.RLock()
    defer r.mu.RUnlock()

    var result []Endpoint
    for _, ep := range r.endpoints {
        if ep.Service == serviceName {
            result = append(result, ep)
        }
    }
    if len(result) == 0 {
        return nil, fmt.Errorf("no endpoints found for service %s", serviceName)
    }

    // Orden determinista por ID
    sort.Slice(result, func(i, j int) bool {
        return result[i].ID < result[j].ID
    })
    return result, nil
}

func (r *InMemoryRegistry) Count() int {
    r.mu.RLock()
    defer r.mu.RUnlock()
    return len(r.endpoints)
}

// Compile-time assertion
var _ ServiceDiscovery = (*InMemoryRegistry)(nil)

// =====================================================================
// IMPLEMENTACION 2: TCPHealthChecker
// Satisface HealthChecker — sin declarar implements
// =====================================================================

type TCPHealthChecker struct {
    Timeout time.Duration
}

func (t TCPHealthChecker) Check(ep Endpoint) HealthStatus {
    // Simular health check (en produccion: net.DialTimeout)
    // Usar hash del endpoint para resultado deterministico en demo
    hash := 0
    for _, c := range ep.ID {
        hash += int(c)
    }
    switch hash % 4 {
    case 0:
        return StatusHealthy
    case 1:
        return StatusHealthy
    case 2:
        return StatusDegraded
    default:
        return StatusUnhealthy
    }
}

func (t TCPHealthChecker) Name() string {
    return fmt.Sprintf("tcp-checker (timeout=%s)", t.Timeout)
}

// Compile-time assertion
var _ HealthChecker = TCPHealthChecker{}

// =====================================================================
// IMPLEMENTACION 3: HTTPHealthChecker
// TAMBIEN satisface HealthChecker — misma interfaz, diferente estrategia
// =====================================================================

type HTTPHealthChecker struct {
    Path    string // e.g., "/healthz"
    Timeout time.Duration
}

func (h HTTPHealthChecker) Check(ep Endpoint) HealthStatus {
    // Simular HTTP health check
    if ep.Port == 443 || ep.Port == 8443 {
        return StatusHealthy // HTTPS endpoints siempre sanos en demo
    }
    return StatusHealthy
}

func (h HTTPHealthChecker) Name() string {
    return fmt.Sprintf("http-checker (path=%s, timeout=%s)", h.Path, h.Timeout)
}

var _ HealthChecker = HTTPHealthChecker{}

// =====================================================================
// IMPLEMENTACION 4: RoundRobinLB
// Satisface LoadBalancer — sin declarar implements
// =====================================================================

type RoundRobinLB struct {
    mu      sync.Mutex
    counter uint64
}

func (r *RoundRobinLB) Select(endpoints []Endpoint) (Endpoint, error) {
    if len(endpoints) == 0 {
        return Endpoint{}, fmt.Errorf("no endpoints available")
    }
    r.mu.Lock()
    idx := r.counter % uint64(len(endpoints))
    r.counter++
    r.mu.Unlock()
    return endpoints[idx], nil
}

func (r *RoundRobinLB) Name() string {
    return "round-robin"
}

var _ LoadBalancer = (*RoundRobinLB)(nil)

// =====================================================================
// IMPLEMENTACION 5: WeightedRandomLB
// TAMBIEN satisface LoadBalancer
// =====================================================================

type WeightedRandomLB struct{}

func (w WeightedRandomLB) Select(endpoints []Endpoint) (Endpoint, error) {
    if len(endpoints) == 0 {
        return Endpoint{}, fmt.Errorf("no endpoints available")
    }
    // Simple random selection (en produccion: pesos basados en latencia)
    return endpoints[rand.Intn(len(endpoints))], nil
}

func (w WeightedRandomLB) Name() string {
    return "weighted-random"
}

var _ LoadBalancer = WeightedRandomLB{}

// =====================================================================
// IMPLEMENTACION 6: InMemoryMetrics
// Satisface MetricsCollector — sin declarar implements
// =====================================================================

type InMemoryMetrics struct {
    mu      sync.Mutex
    metrics map[string][]float64
    tags    map[string]map[string]string
}

func NewInMemoryMetrics() *InMemoryMetrics {
    return &InMemoryMetrics{
        metrics: make(map[string][]float64),
        tags:    make(map[string]map[string]string),
    }
}

func (m *InMemoryMetrics) Record(metric string, value float64, tags map[string]string) {
    m.mu.Lock()
    defer m.mu.Unlock()
    m.metrics[metric] = append(m.metrics[metric], value)
    if tags != nil {
        m.tags[metric] = tags
    }
}

func (m *InMemoryMetrics) Summary() string {
    m.mu.Lock()
    defer m.mu.Unlock()

    var lines []string
    // Orden determinista
    keys := make([]string, 0, len(m.metrics))
    for k := range m.metrics {
        keys = append(keys, k)
    }
    sort.Strings(keys)

    for _, name := range keys {
        values := m.metrics[name]
        var sum float64
        for _, v := range values {
            sum += v
        }
        avg := sum / float64(len(values))
        lines = append(lines, fmt.Sprintf("  %-30s count=%-4d avg=%.2f",
            name, len(values), avg))
    }
    return "Metrics Summary:\n" + strings.Join(lines, "\n")
}

var _ MetricsCollector = (*InMemoryMetrics)(nil)

// =====================================================================
// IMPLEMENTACION 7: ConsoleLogger
// Satisface MeshLogger — sin declarar implements
// =====================================================================

type ConsoleLogger struct {
    Prefix string
}

func (c ConsoleLogger) Info(msg string, fields map[string]string) {
    fmt.Printf("[INFO]  %-8s %s %s\n", c.Prefix, msg, formatFields(fields))
}

func (c ConsoleLogger) Error(msg string, err error, fields map[string]string) {
    fmt.Printf("[ERROR] %-8s %s error=%v %s\n", c.Prefix, msg, err, formatFields(fields))
}

func formatFields(fields map[string]string) string {
    if len(fields) == 0 {
        return ""
    }
    var parts []string
    keys := make([]string, 0, len(fields))
    for k := range fields {
        keys = append(keys, k)
    }
    sort.Strings(keys)
    for _, k := range keys {
        parts = append(parts, fmt.Sprintf("%s=%s", k, fields[k]))
    }
    return strings.Join(parts, " ")
}

var _ MeshLogger = ConsoleLogger{}

// =====================================================================
// SERVICE MESH — orquesta todo usando SOLO interfaces
// No conoce tipos concretos. Maximo desacoplamiento.
// =====================================================================

type ServiceMesh struct {
    discovery ServiceDiscovery
    checker   HealthChecker
    balancer  LoadBalancer
    metrics   MetricsCollector
    logger    MeshLogger
}

// Constructor acepta interfaces — cualquier implementacion funciona
func NewServiceMesh(
    discovery ServiceDiscovery,
    checker HealthChecker,
    balancer LoadBalancer,
    metrics MetricsCollector,
    logger MeshLogger,
) *ServiceMesh {
    return &ServiceMesh{
        discovery: discovery,
        checker:   checker,
        balancer:  balancer,
        metrics:   metrics,
        logger:    logger,
    }
}

// Route simula el routing de una request a un servicio
func (m *ServiceMesh) Route(serviceName string) (Endpoint, error) {
    m.logger.Info("routing request", map[string]string{
        "service": serviceName,
    })

    // 1. Discover endpoints
    endpoints, err := m.discovery.Discover(serviceName)
    if err != nil {
        m.logger.Error("discovery failed", err, map[string]string{
            "service": serviceName,
        })
        m.metrics.Record("discovery.errors", 1, map[string]string{
            "service": serviceName,
        })
        return Endpoint{}, err
    }
    m.metrics.Record("discovery.endpoints", float64(len(endpoints)), map[string]string{
        "service": serviceName,
    })

    // 2. Health check — filter unhealthy
    var healthy []Endpoint
    for _, ep := range endpoints {
        status := m.checker.Check(ep)
        m.metrics.Record("health.check", float64(status), map[string]string{
            "endpoint": ep.ID,
            "status":   status.String(),
        })
        if status <= StatusDegraded { // healthy or degraded
            healthy = append(healthy, ep)
        } else {
            m.logger.Info("excluding unhealthy endpoint", map[string]string{
                "endpoint": ep.ID,
                "status":   status.String(),
            })
        }
    }

    if len(healthy) == 0 {
        err := fmt.Errorf("no healthy endpoints for %s", serviceName)
        m.logger.Error("all endpoints unhealthy", err, nil)
        return Endpoint{}, err
    }

    // 3. Load balance — select one
    selected, err := m.balancer.Select(healthy)
    if err != nil {
        m.logger.Error("load balancer failed", err, nil)
        return Endpoint{}, err
    }

    m.logger.Info("routed request", map[string]string{
        "service":  serviceName,
        "endpoint": selected.ID,
        "host":     selected.Addr(),
        "balancer": m.balancer.Name(),
    })
    m.metrics.Record("routes.success", 1, map[string]string{
        "service": serviceName,
    })

    return selected, nil
}

// HealthReport genera un reporte de salud de todos los endpoints de un servicio
func (m *ServiceMesh) HealthReport(serviceName string) {
    endpoints, err := m.discovery.Discover(serviceName)
    if err != nil {
        m.logger.Error("cannot generate health report", err, nil)
        return
    }

    fmt.Printf("\n=== Health Report: %s (checker: %s) ===\n", serviceName, m.checker.Name())
    for _, ep := range endpoints {
        status := m.checker.Check(ep)
        marker := "  "
        if status == StatusUnhealthy {
            marker = "✗ "
        } else if status == StatusDegraded {
            marker = "~ "
        } else {
            marker = "✓ "
        }
        fmt.Printf("  %s%-20s %-22s %s\n", marker, ep.ID, ep.Addr(), status)
    }
}

// =====================================================================
// MAIN — ensamblar componentes intercambiables
// =====================================================================

func main() {
    fmt.Println("╔══════════════════════════════════════════════════════╗")
    fmt.Println("║   Infrastructure Service Mesh — Duck Typing Demo    ║")
    fmt.Println("╚══════════════════════════════════════════════════════╝")

    // --- 1. Crear componentes (todos satisfacen interfaces implicitamente) ---

    registry := NewInMemoryRegistry()
    tcpChecker := TCPHealthChecker{Timeout: 5 * time.Second}
    httpChecker := HTTPHealthChecker{Path: "/healthz", Timeout: 3 * time.Second}
    rrBalancer := &RoundRobinLB{}
    wrBalancer := WeightedRandomLB{}
    metrics := NewInMemoryMetrics()
    logger := ConsoleLogger{Prefix: "mesh"}

    // --- 2. Registrar servicios ---

    fmt.Println("\n── Registering Services ──")

    webEndpoints := []Endpoint{
        {ID: "web-01", Service: "web", Host: "10.0.1.10", Port: 8080,
            Tags: map[string]string{"region": "us-east", "version": "v2.1"}},
        {ID: "web-02", Service: "web", Host: "10.0.1.11", Port: 8080,
            Tags: map[string]string{"region": "us-east", "version": "v2.1"}},
        {ID: "web-03", Service: "web", Host: "10.0.2.10", Port: 8080,
            Tags: map[string]string{"region": "eu-west", "version": "v2.0"}},
        {ID: "web-04", Service: "web", Host: "10.0.2.11", Port: 8080,
            Tags: map[string]string{"region": "eu-west", "version": "v2.1"}},
    }

    apiEndpoints := []Endpoint{
        {ID: "api-01", Service: "api", Host: "10.0.3.10", Port: 443,
            Tags: map[string]string{"region": "us-east"}},
        {ID: "api-02", Service: "api", Host: "10.0.3.11", Port: 443,
            Tags: map[string]string{"region": "us-east"}},
        {ID: "api-03", Service: "api", Host: "10.0.4.10", Port: 8443,
            Tags: map[string]string{"region": "eu-west"}},
    }

    for _, ep := range append(webEndpoints, apiEndpoints...) {
        if err := registry.Register(ep); err != nil {
            fmt.Printf("  ERROR: %v\n", err)
        } else {
            fmt.Printf("  Registered: %s\n", ep)
        }
    }
    fmt.Printf("  Total endpoints: %d\n", registry.Count())

    // --- 3. Crear mesh con TCP checker + round-robin ---

    fmt.Println("\n── Mesh Configuration 1: TCP + Round-Robin ──")

    mesh1 := NewServiceMesh(registry, tcpChecker, rrBalancer, metrics, logger)

    mesh1.HealthReport("web")

    fmt.Println("\n  Routing 4 requests to 'web':")
    for i := 0; i < 4; i++ {
        ep, err := mesh1.Route("web")
        if err != nil {
            fmt.Printf("  Request %d: ERROR %v\n", i+1, err)
        } else {
            fmt.Printf("  Request %d → %s\n", i+1, ep)
        }
    }

    // --- 4. Crear mesh con HTTP checker + weighted-random ---
    //         (intercambiar componentes sin cambiar ServiceMesh!)

    fmt.Println("\n── Mesh Configuration 2: HTTP + Weighted-Random ──")

    mesh2 := NewServiceMesh(registry, httpChecker, wrBalancer, metrics, logger)

    mesh2.HealthReport("api")

    fmt.Println("\n  Routing 3 requests to 'api':")
    for i := 0; i < 3; i++ {
        ep, err := mesh2.Route("api")
        if err != nil {
            fmt.Printf("  Request %d: ERROR %v\n", i+1, err)
        } else {
            fmt.Printf("  Request %d → %s\n", i+1, ep)
        }
    }

    // --- 5. Deregister y re-route ---

    fmt.Println("\n── Deregistering web-02 ──")
    if err := registry.Deregister("web-02"); err != nil {
        fmt.Printf("  ERROR: %v\n", err)
    } else {
        fmt.Println("  Done. Re-routing:")
    }

    ep, err := mesh1.Route("web")
    if err != nil {
        fmt.Printf("  ERROR: %v\n", err)
    } else {
        fmt.Printf("  Routed to: %s\n", ep)
    }

    // --- 6. Error case: servicio no registrado ---

    fmt.Println("\n── Routing to unknown service ──")
    _, err = mesh1.Route("cache")
    if err != nil {
        fmt.Printf("  Expected error: %v\n", err)
    }

    // --- 7. Duplicate registration error ---

    fmt.Println("\n── Duplicate registration ──")
    err = registry.Register(Endpoint{ID: "web-01", Service: "web", Host: "10.0.1.10", Port: 8080})
    if err != nil {
        fmt.Printf("  Expected error: %v\n", err)
    }

    // --- 8. Metrics summary ---

    fmt.Println("\n── Metrics ──")
    fmt.Println(metrics.Summary())

    // --- 9. Demostrar intercambiabilidad ---

    fmt.Println("\n── Duck Typing Demo ──")
    fmt.Println("  Los siguientes tipos satisfacen interfaces SIN declarar 'implements':")
    fmt.Println()

    // Cada tipo satisface su interfaz implicitamente
    var disc ServiceDiscovery = registry
    var hc1 HealthChecker = tcpChecker
    var hc2 HealthChecker = httpChecker
    var lb1 LoadBalancer = rrBalancer
    var lb2 LoadBalancer = wrBalancer
    var mc MetricsCollector = metrics
    var ml MeshLogger = logger

    _ = disc
    _ = mc
    _ = ml

    fmt.Printf("  %-24s → satisface HealthChecker  (%s)\n",
        fmt.Sprintf("%T", tcpChecker), hc1.Name())
    fmt.Printf("  %-24s → satisface HealthChecker  (%s)\n",
        fmt.Sprintf("%T", httpChecker), hc2.Name())
    fmt.Printf("  %-24s → satisface LoadBalancer   (%s)\n",
        fmt.Sprintf("%T", rrBalancer), lb1.Name())
    fmt.Printf("  %-24s → satisface LoadBalancer   (%s)\n",
        fmt.Sprintf("%T", wrBalancer), lb2.Name())
    fmt.Printf("  %-24s → satisface ServiceDiscovery\n",
        fmt.Sprintf("%T", registry))
    fmt.Printf("  %-24s → satisface MetricsCollector\n",
        fmt.Sprintf("%T", metrics))
    fmt.Printf("  %-24s → satisface MeshLogger\n",
        fmt.Sprintf("%T", logger))

    fmt.Println("\n  Ninguno usa 'implements'. Go verifica en compilacion.")
    fmt.Println("  Puedes agregar nuevas implementaciones sin modificar ServiceMesh.")
}
```

**Output esperado (parcial):**
```
╔══════════════════════════════════════════════════════╗
║   Infrastructure Service Mesh — Duck Typing Demo    ║
╚══════════════════════════════════════════════════════╝

── Registering Services ──
  Registered: [web-01] web @ 10.0.1.10:8080
  Registered: [web-02] web @ 10.0.1.11:8080
  Registered: [web-03] web @ 10.0.2.10:8080
  Registered: [web-04] web @ 10.0.2.11:8080
  Registered: [api-01] api @ 10.0.3.10:443
  Registered: [api-02] api @ 10.0.3.11:443
  Registered: [api-03] api @ 10.0.4.10:8443
  Total endpoints: 7

── Mesh Configuration 1: TCP + Round-Robin ──

=== Health Report: web (checker: tcp-checker (timeout=5s)) ===
  ✓ web-01               10.0.1.10:8080         healthy
  ✓ web-02               10.0.1.11:8080         healthy
  ~ web-03               10.0.2.10:8080         degraded
  ✗ web-04               10.0.2.11:8080         unhealthy

  Routing 4 requests to 'web':
[INFO]  mesh     routing request service=web
[INFO]  mesh     routed request balancer=round-robin endpoint=web-01 host=10.0.1.10:8080 service=web
  Request 1 → [web-01] web @ 10.0.1.10:8080
  ...

── Duck Typing Demo ──
  Los siguientes tipos satisfacen interfaces SIN declarar 'implements':

  main.TCPHealthChecker    → satisface HealthChecker  (tcp-checker (timeout=5s))
  main.HTTPHealthChecker   → satisface HealthChecker  (http-checker (path=/healthz, timeout=3s))
  *main.RoundRobinLB       → satisface LoadBalancer   (round-robin)
  main.WeightedRandomLB    → satisface LoadBalancer   (weighted-random)
  *main.InMemoryRegistry   → satisface ServiceDiscovery
  *main.InMemoryMetrics    → satisface MetricsCollector
  main.ConsoleLogger       → satisface MeshLogger

  Ninguno usa 'implements'. Go verifica en compilacion.
  Puedes agregar nuevas implementaciones sin modificar ServiceMesh.
```

---

## 16. Resumen

```
┌────────────────────────────────────────────────────────────────────────┐
│  INTERFACES IMPLICITAS — Resumen                                       │
├────────────────────────────────────────────────────────────────────────┤
│                                                                        │
│  1. Satisfaccion sin declaracion: si tiene los metodos, satisface      │
│  2. Duck typing ESTATICO: verificado en compilacion, no runtime        │
│  3. Un tipo → multiples interfaces simultaneamente                     │
│  4. Interfaz value = par (tipo, valor) — 16 bytes                      │
│  5. Method sets: T solo value recv, *T ambos                           │
│  6. Definir interfaces en el CONSUMIDOR, no en el productor            │
│  7. Satisfaccion retroactiva: interfaces para tipos que ya existen     │
│  8. Interfaces pequeñas: 1-3 metodos es lo idiomatico                  │
│  9. Compile-time assertion: var _ Interface = (*Type)(nil)             │
│ 10. No exportar interfaz + struct — exporta struct, consumidor define  │
│                                                                        │
│  SysAdmin/DevOps:                                                      │
│  ├─ Abstraer backends: storage, logging, monitoring, health checks     │
│  ├─ Testing: interfaces minimas → mocks triviales                      │
│  ├─ Plugins: agregar implementaciones sin modificar el core            │
│  └─ Strategy: intercambiar comportamiento en runtime                   │
│                                                                        │
│  vs C: Go = type-safe, verificado en compilacion; C = void* manual     │
│  vs Rust: Go = implicito, solo dynamic dispatch; Rust = explicito,     │
│           static por defecto, dynamic con dyn, zero-cost abstractions  │
└────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: T02 - Interface vacia (any) — interface{} / any (Go 1.18+), type assertions, type switch
