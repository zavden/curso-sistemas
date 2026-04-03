# Composicion de Interfaces — io.ReadWriter, io.ReadCloser, Embedding de Interfaces

## 1. Introduccion

Go no tiene herencia, pero tiene **composicion** — y esto aplica tanto a structs como a interfaces. Una interfaz puede **embeber** otras interfaces, heredando todos sus metodos sin copiarlos. Esto es el mecanismo fundamental para construir interfaces mas complejas a partir de interfaces simples.

La stdlib de Go usa este patron extensivamente: `io.ReadWriter` es simplemente `io.Reader` + `io.Writer` embebidas. `io.ReadCloser` es `io.Reader` + `io.Closer`. No se redefinen los metodos — se **componen**. Esto crea un arbol de interfaces donde cada nodo es la union de sus hijos, y un tipo concreto que satisface la interfaz compuesta automaticamente satisface todas las interfaces embebidas.

Esta composicion es ortogonal a la composicion de structs (embedding de T01 anterior). Aqui estamos embebiendo **interfaces dentro de interfaces** — definiendo contratos mas amplios a partir de contratos mas pequeños. El resultado es un sistema de tipos increiblemente flexible: puedes pedir exactamente la combinacion de capacidades que necesitas, ni mas ni menos.

En SysAdmin/DevOps, la composicion de interfaces permite modelar recursos con capacidades incrementales: un recurso que puede leerse y cerrarse (ReadCloser), un servicio que puede desplegarse y escalarse (DeployScaler), un backend que puede leer, escribir y listar (ReadWriteLister). Cada funcion pide la combinacion exacta que necesita.

```
┌─────────────────────────────────────────────────────────────────────────┐
│           COMPOSICION DE INTERFACES — Embedding                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Interfaces atomicas (1 metodo cada una):                               │
│                                                                         │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐  ┌────────────┐       │
│  │  Reader     │  │  Writer    │  │  Closer    │  │  Seeker    │       │
│  │  Read()     │  │  Write()   │  │  Close()   │  │  Seek()    │       │
│  └─────┬──────┘  └─────┬──────┘  └─────┬──────┘  └─────┬──────┘       │
│        │               │               │               │               │
│  Interfaces compuestas (embedding):                                     │
│        │               │               │               │               │
│        ├───────┬───────┤               │               │               │
│        │       │       │               │               │               │
│        ▼       │       ▼               │               │               │
│  ┌──────────┐  │  ┌──────────┐         │               │               │
│  │ReadWriter│  │  │WriteCloser│        │               │               │
│  │ R + W    │  │  │ W + C     │        │               │               │
│  └────┬─────┘  │  └──────────┘         │               │               │
│       │        │                       │               │               │
│       │        ├───────────────────────┤               │               │
│       │        ▼                       │               │               │
│       │  ┌──────────┐                  │               │               │
│       │  │ReadCloser│                  │               │               │
│       │  │ R + C    │                  │               │               │
│       │  └──────────┘                  │               │               │
│       │        │                       │               │               │
│       ├────────┼───────────────────────┤               │               │
│       ▼        ▼                       ▼               │               │
│  ┌───────────────────┐                                 │               │
│  │ ReadWriteCloser   │                                 │               │
│  │ R + W + C         │                                 │               │
│  └───────────────────┘                                 │               │
│                                                                         │
│  *os.File satisface TODAS estas interfaces                              │
│  Un tipo compuesto satisface todas las interfaces componentes           │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Sintaxis: Embedding de Interfaces

### La forma basica

```go
// Interfaces atomicas
type Reader interface {
    Read(p []byte) (n int, err error)
}

type Writer interface {
    Write(p []byte) (n int, err error)
}

type Closer interface {
    Close() error
}

// Interfaces compuestas — embedding sin repetir metodos
type ReadWriter interface {
    Reader  // embebe todos los metodos de Reader
    Writer  // embebe todos los metodos de Writer
}
// ReadWriter tiene: Read() + Write()

type ReadCloser interface {
    Reader
    Closer
}
// ReadCloser tiene: Read() + Close()

type WriteCloser interface {
    Writer
    Closer
}
// WriteCloser tiene: Write() + Close()

type ReadWriteCloser interface {
    Reader
    Writer
    Closer
}
// ReadWriteCloser tiene: Read() + Write() + Close()
```

### Equivalencia: embedding vs declaracion explicita

```go
// Estas dos declaraciones son 100% EQUIVALENTES:

// Version 1: con embedding
type ReadWriter interface {
    Reader
    Writer
}

// Version 2: sin embedding (metodos explicitos)
type ReadWriter interface {
    Read(p []byte) (n int, err error)
    Write(p []byte) (n int, err error)
}

// Ambas definen la misma interfaz: un tipo necesita Read() Y Write()
// para satisfacerla.

// Embedding es preferido porque:
// 1. No repite firmas de metodos (DRY)
// 2. Documenta que las capacidades vienen de Reader y Writer
// 3. Si Reader cambia (hipotetico), ReadWriter cambia automaticamente
// 4. Permite verificacion de satisfaccion parcial
```

---

## 3. Composicion en la Stdlib — El Arbol de io

El paquete `io` define la composicion mas importante de Go:

```go
// Atomicas (1 metodo)
type Reader    interface { Read(p []byte) (n int, err error) }
type Writer    interface { Write(p []byte) (n int, err error) }
type Closer    interface { Close() error }
type Seeker    interface { Seek(offset int64, whence int) (int64, error) }
type ReaderAt  interface { ReadAt(p []byte, off int64) (n int, err error) }
type WriterAt  interface { WriteAt(p []byte, off int64) (n int, err error) }

// Compuestas de 2
type ReadWriter   interface { Reader; Writer }
type ReadCloser   interface { Reader; Closer }
type WriteCloser  interface { Writer; Closer }
type ReadSeeker   interface { Reader; Seeker }
type WriteSeeker  interface { Writer; Seeker }

// Compuestas de 3
type ReadWriteCloser interface { Reader; Writer; Closer }
type ReadWriteSeeker interface { Reader; Writer; Seeker }
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│  SATISFACCION DEL ARBOL DE INTERFACES io.*                               │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Tipo concreto     │ R │ W │ C │ S │ RW │ RC │ WC │ RS │ RWC │ RWS     │
│  ──────────────────┼───┼───┼───┼───┼────┼────┼────┼────┼─────┼───      │
│  *os.File          │ ✓ │ ✓ │ ✓ │ ✓ │ ✓  │ ✓  │ ✓  │ ✓  │ ✓   │ ✓      │
│  *bytes.Buffer     │ ✓ │ ✓ │   │   │ ✓  │    │    │    │     │         │
│  *strings.Reader   │ ✓ │   │   │ ✓ │    │    │    │ ✓  │     │         │
│  net.Conn          │ ✓ │ ✓ │ ✓ │   │ ✓  │ ✓  │ ✓  │    │ ✓   │         │
│  *gzip.Reader      │ ✓ │   │ ✓ │   │    │ ✓  │    │    │     │         │
│  *gzip.Writer      │   │ ✓ │ ✓ │   │    │    │ ✓  │    │     │         │
│  *bufio.ReadWriter │ ✓ │ ✓ │   │   │ ✓  │    │    │    │     │         │
│  http.Response.Body│ ✓ │   │ ✓ │   │    │ ✓  │    │    │     │         │
│  *tls.Conn         │ ✓ │ ✓ │ ✓ │   │ ✓  │ ✓  │ ✓  │    │ ✓   │         │
│                                                                          │
│  Un tipo que satisface una interfaz compuesta automaticamente            │
│  satisface todas sus componentes.                                        │
│  *os.File satisface TODAS las 10 interfaces de la tabla.                 │
└──────────────────────────────────────────────────────────────────────────┘
```

### Usando interfaces compuestas

```go
package main

import (
    "bytes"
    "compress/gzip"
    "fmt"
    "io"
    "os"
    "strings"
)

// Funcion que necesita leer Y cerrar
func ProcessAndClose(rc io.ReadCloser) error {
    defer rc.Close()
    data, err := io.ReadAll(rc)
    if err != nil {
        return err
    }
    fmt.Printf("  Read %d bytes\n", len(data))
    return nil
}

// Funcion que necesita leer Y escribir
func CopyWithTransform(rw io.ReadWriter) error {
    data, err := io.ReadAll(rw)
    if err != nil {
        return err
    }
    upper := strings.ToUpper(string(data))
    _, err = rw.Write([]byte(upper))
    return err
}

func main() {
    // 1. ProcessAndClose con archivo (ReadCloser)
    file, _ := os.Open("/etc/hostname")
    ProcessAndClose(file) // *os.File satisface ReadCloser

    // 2. ProcessAndClose con gzip reader (ReadCloser)
    var buf bytes.Buffer
    gz := gzip.NewWriter(&buf)
    gz.Write([]byte("compressed data"))
    gz.Close()

    gzReader, _ := gzip.NewReader(&buf)
    ProcessAndClose(gzReader) // *gzip.Reader satisface ReadCloser

    // 3. ProcessAndClose con NopCloser (convierte Reader → ReadCloser)
    strReader := strings.NewReader("no close needed")
    rc := io.NopCloser(strReader) // agrega Close() que no hace nada
    ProcessAndClose(rc)
}
```

---

## 4. Crear Tus Propias Interfaces Compuestas

### Composicion ad-hoc en tu dominio

```go
// Interfaces atomicas de dominio
type Deployer interface {
    Deploy(service, image string) error
}

type Scaler interface {
    Scale(service string, replicas int) error
}

type Stopper interface {
    Stop(service string) error
}

type HealthChecker interface {
    Check(service string) error
}

type LogViewer interface {
    Logs(service string, lines int) ([]string, error)
}

// Composiciones para diferentes contextos:

// Rolling update necesita deploy + scale
type DeployScaler interface {
    Deployer
    Scaler
}

// Blue-green deploy necesita deploy + stop + health check
type BlueGreenDeployer interface {
    Deployer
    Stopper
    HealthChecker
}

// Debugging necesita health + logs
type Debugger interface {
    HealthChecker
    LogViewer
}

// Lifecycle completo
type ServiceManager interface {
    Deployer
    Scaler
    Stopper
    HealthChecker
    LogViewer
}
```

### Cada funcion pide exactamente lo que necesita

```go
// Rolling update: solo necesita Deploy + Scale
func RollingUpdate(ds DeployScaler, service, image string, replicas int) error {
    // Scale down
    if err := ds.Scale(service, 0); err != nil {
        return fmt.Errorf("scale down: %w", err)
    }
    // Deploy new version
    if err := ds.Deploy(service, image); err != nil {
        return fmt.Errorf("deploy: %w", err)
    }
    // Scale up
    return ds.Scale(service, replicas)
}

// Blue-green: necesita Deploy + Stop + Check
func BlueGreenDeploy(bgd BlueGreenDeployer, service, image string) error {
    // Deploy new version alongside old
    newService := service + "-green"
    if err := bgd.Deploy(newService, image); err != nil {
        return fmt.Errorf("deploy green: %w", err)
    }
    // Health check new version
    if err := bgd.Check(newService); err != nil {
        bgd.Stop(newService) // rollback
        return fmt.Errorf("green unhealthy: %w", err)
    }
    // Stop old version
    return bgd.Stop(service)
}

// Debugging: solo necesita Check + Logs
func DiagnoseService(d Debugger, service string) {
    if err := d.Check(service); err != nil {
        fmt.Printf("UNHEALTHY: %v\n", err)
        logs, _ := d.Logs(service, 50)
        for _, line := range logs {
            fmt.Println("  ", line)
        }
    }
}

// Un solo tipo concreto satisface TODAS las combinaciones:
type K8sManager struct {
    namespace string
    // ...
}

func (k *K8sManager) Deploy(service, image string) error { return nil }
func (k *K8sManager) Scale(service string, n int) error  { return nil }
func (k *K8sManager) Stop(service string) error          { return nil }
func (k *K8sManager) Check(service string) error         { return nil }
func (k *K8sManager) Logs(service string, n int) ([]string, error) { return nil, nil }

// K8sManager satisface:
var _ Deployer = (*K8sManager)(nil)
var _ Scaler = (*K8sManager)(nil)
var _ Stopper = (*K8sManager)(nil)
var _ HealthChecker = (*K8sManager)(nil)
var _ LogViewer = (*K8sManager)(nil)
var _ DeployScaler = (*K8sManager)(nil)
var _ BlueGreenDeployer = (*K8sManager)(nil)
var _ Debugger = (*K8sManager)(nil)
var _ ServiceManager = (*K8sManager)(nil)
// ¡9 interfaces con 5 metodos!
```

---

## 5. Embedding de Interfaces en Structs (Decorator Pattern)

No confundir con embedding de interfaces en interfaces. Aqui embebemos una **interfaz dentro de un struct** — esto permite construir wrappers/decorators:

```go
// Embeber interfaz en struct: el struct "hereda" los metodos de la interfaz
// pero puede overridear los que quiera.

// Ejemplo: logging wrapper para io.ReadCloser
type LoggingReader struct {
    io.ReadCloser  // embebe la interfaz, no un tipo concreto
    label string
    total int64
}

func NewLoggingReader(rc io.ReadCloser, label string) *LoggingReader {
    return &LoggingReader{ReadCloser: rc, label: label}
}

// Override Read — agrega logging
func (lr *LoggingReader) Read(p []byte) (int, error) {
    n, err := lr.ReadCloser.Read(p) // delegar al reader embebido
    lr.total += int64(n)
    if err != nil {
        fmt.Printf("[%s] read finished: %d total bytes, err=%v\n", lr.label, lr.total, err)
    }
    return n, err
}

// Close se delega automaticamente al ReadCloser embebido
// (no necesitamos overridear)

// Uso:
func main() {
    file, _ := os.Open("/etc/hosts")
    logged := NewLoggingReader(file, "hosts")
    // logged satisface io.ReadCloser

    data, _ := io.ReadAll(logged) // usa Read con logging
    logged.Close()                // delegado al file original
    fmt.Printf("Read %d bytes\n", len(data))
}
```

### Patron: Middleware como wrapper de interfaz

```go
// http.ResponseWriter wrapper que captura status code

type StatusRecorder struct {
    http.ResponseWriter  // embebe la interfaz
    StatusCode int
    Written    int64
}

func NewStatusRecorder(w http.ResponseWriter) *StatusRecorder {
    return &StatusRecorder{ResponseWriter: w, StatusCode: 200}
}

// Override WriteHeader para capturar el status code
func (sr *StatusRecorder) WriteHeader(code int) {
    sr.StatusCode = code
    sr.ResponseWriter.WriteHeader(code) // delegar
}

// Override Write para contar bytes
func (sr *StatusRecorder) Write(b []byte) (int, error) {
    n, err := sr.ResponseWriter.Write(b) // delegar
    sr.Written += int64(n)
    return n, err
}

// Header() se delega automaticamente (embebido)

func LoggingMiddleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        rec := NewStatusRecorder(w)
        start := time.Now()

        next.ServeHTTP(rec, r) // usa el recorder en vez del writer original

        fmt.Printf("%s %s → %d (%d bytes) in %s\n",
            r.Method, r.URL.Path, rec.StatusCode, rec.Written, time.Since(start))
    })
}
```

### Diferencia critica: embeber interfaz vs embeber struct

```go
// Embeber INTERFAZ en struct:
type Wrapper struct {
    io.Reader  // cualquier Reader puede ir aqui
}
// → Flexible: puedes pasar cualquier Reader
// → Wrapper.Reader puede ser nil si no se inicializa → panic!

// Embeber STRUCT en struct:
type Wrapper struct {
    *bytes.Buffer  // solo *bytes.Buffer puede ir aqui
}
// → Rigido: siempre es *bytes.Buffer
// → Si nil, auto-inicializado a zero value (pero Buffer nil = panic en Read)

// Regla: embeber interfaz cuando necesitas flexibilidad (decorators)
//        embeber struct cuando sabes el tipo concreto (extension)
```

---

## 6. Composicion vs Herencia — Por que Go Elige Composicion

```
┌──────────────────────────────────────────────────────────────────────────┐
│  HERENCIA (Java/C++)              vs    COMPOSICION (Go)                 │
├───────────────────────────────────┬──────────────────────────────────────┤
│                                   │                                      │
│  class Animal {                   │  type Mover interface {              │
│    void move() { ... }            │      Move() error                    │
│    void eat() { ... }             │  }                                   │
│  }                                │  type Eater interface {              │
│                                   │      Eat(food string) error          │
│  class Dog extends Animal {       │  }                                   │
│    void bark() { ... }            │  type Barker interface {             │
│    // hereda move() y eat()       │      Bark() string                   │
│  }                                │  }                                   │
│                                   │                                      │
│  class Cat extends Animal {       │  // Composicion: combinar capacidades│
│    void purr() { ... }            │  type Dog struct { ... }             │
│  }                                │  func (d Dog) Move() error { ... }   │
│                                   │  func (d Dog) Eat(food string) error │
│  // Problema: diamante            │  func (d Dog) Bark() string { ... }  │
│  class FlyingFish extends         │                                      │
│    Fish, Bird { ??? }             │  // No hay jerarquia → no hay diamante│
│                                   │  // Dog satisface Mover, Eater,      │
│  // Problema: fragil              │  //   y Barker independientemente    │
│  // Cambiar Animal rompe          │                                      │
│  //   Dog y Cat                   │  // Interfaz compuesta si la necesitas│
│                                   │  type Animal interface {             │
│                                   │      Mover                           │
│                                   │      Eater                           │
│                                   │  }                                   │
└───────────────────────────────────┴──────────────────────────────────────┘
```

### Ventajas de composicion de interfaces

```go
// 1. Sin problema del diamante
type Swimmer interface { Swim() error }
type Flyer interface { Fly() error }
type Walker interface { Walk() error }

// Composicion libre sin conflictos:
type Amphibian interface {
    Swimmer
    Walker
}

type FlyingFish interface {
    Swimmer
    Flyer
    // Sin diamante — solo se unen metodos, no implementaciones
}

// 2. Composicion incremental
type Duck interface {
    Swimmer
    Flyer
    Walker
    // Duck tiene 3 metodos — ninguno hereda implementacion
    // Un tipo satisface Duck teniendo Swim(), Fly(), Walk()
}

// 3. Descomposicion retroactiva
// Tengo un tipo con 10 metodos. Puedo crear interfaces
// pequeñas que extraen subconjuntos — sin modificar el tipo.
```

---

## 7. Patrones Avanzados de Composicion

### Patron 1: Interface upgrading (ampliar capacidades)

```go
// Base: todos los backends deben leer
type Reader interface {
    Read(key string) ([]byte, error)
}

// Extension: algunos backends tambien pueden escribir
type ReadWriter interface {
    Reader
    Write(key string, data []byte) error
}

// Extension: algunos backends tambien pueden listar
type ReadWriteLister interface {
    ReadWriter
    List(prefix string) ([]string, error)
}

// Extension: algunos backends tienen watch
type WatchableStore interface {
    ReadWriteLister
    Watch(prefix string) (<-chan Event, error)
}

// Cada nivel agrega una capacidad.
// Un backend avanzado (etcd) satisface WatchableStore.
// Un backend simple (archivos) solo satisface ReadWriter.
// Las funciones piden el nivel que necesitan.

func Backup(src Reader, dst io.Writer) error {
    // Solo necesita leer — funciona con cualquier backend
    data, err := src.Read("config")
    if err != nil {
        return err
    }
    _, err = dst.Write(data)
    return err
}

func Sync(src Reader, dst ReadWriter, keys []string) error {
    // Necesita leer + escribir
    for _, key := range keys {
        data, err := src.Read(key)
        if err != nil {
            continue
        }
        if err := dst.Write(key, data); err != nil {
            return err
        }
    }
    return nil
}

func FullSync(src ReadWriteLister, dst ReadWriter) error {
    // Necesita listar + leer + escribir
    keys, err := src.List("")
    if err != nil {
        return err
    }
    return Sync(src, dst, keys) // ReadWriteLister satisface Reader
}
```

### Patron 2: Interfaz privada con metodo publico

```go
// A veces quieres que una interfaz solo sea implementable
// dentro de tu paquete (sealed interface):

package auth

// Interfaz exportada — usable por cualquiera
type Authenticator interface {
    Authenticate(token string) (User, error)
    sealed() // metodo unexported → solo tipos de este paquete pueden implementar
}

// Como sealed() es unexported, ningun paquete externo puede implementar
// Authenticator — porque no pueden tener un metodo llamado sealed() en
// el paquete auth.

type OAuthAuth struct{}
func (o OAuthAuth) Authenticate(token string) (User, error) { return User{}, nil }
func (o OAuthAuth) sealed() {} // implementa sealed

type JWTAuth struct{}
func (j JWTAuth) Authenticate(token string) (User, error) { return User{}, nil }
func (j JWTAuth) sealed() {} // implementa sealed

// Fuera de package auth:
// type MyAuth struct{}
// func (m MyAuth) Authenticate(token string) (User, error) { ... }
// func (m MyAuth) sealed() {} // FUNCIONA pero sealed() esta en package "main",
//                              // no en package "auth" — no satisface auth.Authenticator
```

### Patron 3: Composicion para testing selectivo

```go
// En tests, a veces necesitas mockear SOLO algunas partes
// de una interfaz compuesta:

type Store interface {
    Reader
    Writer
    Closer
}

// Mock que solo implementa lo que el test necesita:
type ReadOnlyMock struct {
    data map[string][]byte
}

func (m *ReadOnlyMock) Read(key string) ([]byte, error) {
    d, ok := m.data[key]
    if !ok {
        return nil, fmt.Errorf("not found: %s", key)
    }
    return d, nil
}

// ReadOnlyMock NO satisface Store (le faltan Write y Close)
// Pero SI satisface Reader

// Para funciones que solo necesitan Reader, funciona perfecto:
func GetConfig(r Reader) (Config, error) {
    data, err := r.Read("config.yaml")
    // ...
}

// Test:
func TestGetConfig(t *testing.T) {
    mock := &ReadOnlyMock{
        data: map[string][]byte{
            "config.yaml": []byte("port: 8080"),
        },
    }
    cfg, err := GetConfig(mock) // solo necesita Reader
    // ...
}
```

---

## 8. Metodos Duplicados en Composicion — Reglas de Resolucion

### Metodos identicos: OK

```go
type A interface {
    Foo() string
}

type B interface {
    Foo() string  // mismo nombre, misma firma
}

type AB interface {
    A
    B
}
// AB tiene UN solo metodo Foo() string
// No hay conflicto porque la firma es identica
// Un tipo solo necesita implementar Foo() una vez

type MyType struct{}
func (m MyType) Foo() string { return "hello" }

var _ AB = MyType{} // OK — Foo() satisface tanto A.Foo como B.Foo
```

### Metodos con firmas diferentes: ERROR de compilacion

```go
type A interface {
    Foo() string
}

type B interface {
    Foo() int  // mismo nombre, DIFERENTE firma
}

// ESTO NO COMPILA:
// type AB interface {
//     A
//     B
// }
// ERROR: duplicate method Foo

// Go no tiene mecanismo de desambiguacion (no hay "virtual inheritance"
// ni "method resolution order"). Si hay conflicto, no compila.
```

### En la practica: raro pero posible

```go
// Esto puede pasar cuando compones interfaces de paquetes diferentes:
// package a: type X interface { Process() error }
// package b: type Y interface { Process() error }

// Componer a.X + b.Y esta bien — Process() error es identico

// Pero si:
// package a: type X interface { Process() error }
// package b: type Y interface { Process() ([]byte, error) }
// Componer falla — firmas diferentes para Process
```

---

## 9. Composicion con Metodos Adicionales

Una interfaz compuesta puede agregar metodos propios ademas de los embebidos:

```go
// sort.Interface combina embedding conceptual con metodos propios
// (aunque en la stdlib no usa embedding, podria):

type Sizer interface {
    Len() int
}

type Comparator interface {
    Less(i, j int) bool
}

type Swapper interface {
    Swap(i, j int)
}

// Composicion + metodo extra
type SortableWithName interface {
    Sizer
    Comparator
    Swapper
    Name() string  // metodo adicional propio
}

// Un tipo necesita Len(), Less(), Swap() Y Name() para satisfacerla
```

```go
// Patron SysAdmin: interfaz compuesta con metodo de ciclo de vida

type Configurable interface {
    Configure(opts map[string]string) error
}

type Runnable interface {
    Run() error
}

type Service interface {
    Configurable
    Runnable
    io.Closer
    Name() string    // metodo propio — identificacion
    Status() string  // metodo propio — estado actual
}
// Un Service debe: Configure, Run, Close, Name, Status
// 5 metodos de 3 fuentes + 2 propios
```

---

## 10. io.NopCloser y Adapters de Composicion

A veces tienes un tipo que satisface una interfaz parcialmente y necesitas "completar" la interfaz que falta:

### io.NopCloser — agregar Close() a un Reader

```go
// Problema: strings.Reader satisface io.Reader pero NO io.ReadCloser
// Solucion: io.NopCloser agrega un Close() que no hace nada

r := strings.NewReader("hello")
// r solo es Reader

rc := io.NopCloser(r)
// rc es ReadCloser — Close() no hace nada

// Util cuando una funcion requiere ReadCloser pero tu fuente
// no tiene recursos que cerrar
func ProcessBody(body io.ReadCloser) {
    defer body.Close()
    data, _ := io.ReadAll(body)
    fmt.Println(string(data))
}

ProcessBody(io.NopCloser(strings.NewReader("mock body")))
```

### Crear tus propios adapters

```go
// Adapter: agregar Close() a cualquier Writer
type NopWriteCloser struct {
    io.Writer
}

func (NopWriteCloser) Close() error { return nil }

func NewNopWriteCloser(w io.Writer) io.WriteCloser {
    return NopWriteCloser{Writer: w}
}

// Uso:
var buf bytes.Buffer
wc := NewNopWriteCloser(&buf) // buf como WriteCloser
wc.Write([]byte("hello"))
wc.Close() // no-op
```

```go
// Adapter: agregar nombre a cualquier Checker
type NamedCheckerAdapter struct {
    Checker
    name string
}

func WithName(c Checker, name string) NamedChecker {
    return &NamedCheckerAdapter{Checker: c, name: name}
}

func (a *NamedCheckerAdapter) Name() string { return a.name }
// Check() se delega automaticamente al Checker embebido

// Uso:
func main() {
    check := CheckerFunc(func() error {
        return exec.Command("ping", "-c", "1", "8.8.8.8").Run()
    })

    // CheckerFunc satisface Checker pero no NamedChecker
    // WithName le agrega el metodo Name()
    named := WithName(check, "ping:8.8.8.8")
    // named satisface NamedChecker
}
```

---

## 11. Satisfaccion Parcial y Conversion Ascendente

Un tipo que satisface una interfaz compuesta **automaticamente satisface todas sus partes**. La conversion es implicita y en la direccion "ancha → estrecha":

```go
// *os.File satisface ReadWriteCloser (tiene Read, Write, Close)

file, _ := os.Open("/etc/hosts")

// Conversion "descendente": de tipo concreto a interfaces parciales
var rw io.ReadWriter = file       // ✓ File tiene Read + Write
var rc io.ReadCloser = file       // ✓ File tiene Read + Close
var r io.Reader = file            // ✓ File tiene Read
var c io.Closer = file            // ✓ File tiene Close

// Conversion entre interfaces: de compuesta a componente
var rwc io.ReadWriteCloser = file
var r2 io.Reader = rwc            // ✓ ReadWriteCloser contiene Reader
var rc2 io.ReadCloser = rwc       // ✓ ReadWriteCloser contiene Reader + Closer

// La OTRA direccion (estrecha → ancha) NO compila:
// var rwc2 io.ReadWriteCloser = r  // ✗ Reader no tiene Write ni Close
// Necesitarias type assertion:
// if rwc3, ok := r.(io.ReadWriteCloser); ok { ... }
```

```go
// Patron SysAdmin: pasar una interfaz grande donde se pide una pequeña

type FullManager struct {
    // implementa Deploy, Scale, Stop, Check, Logs
}

func (f *FullManager) Deploy(s, i string) error { return nil }
func (f *FullManager) Scale(s string, n int) error { return nil }
func (f *FullManager) Stop(s string) error { return nil }
func (f *FullManager) Check(s string) error { return nil }
func (f *FullManager) Logs(s string, n int) ([]string, error) { return nil, nil }

func main() {
    mgr := &FullManager{}

    // Pasar a funciones que piden interfaces parciales:
    RollingUpdate(mgr, "web", "nginx:latest", 3)  // necesita Deployer + Scaler
    DiagnoseService(mgr, "web")                     // necesita HealthChecker + LogViewer
    Backup(mgr, os.Stdout)                         // necesita Reader

    // El mismo objeto, diferentes "vistas" segun la funcion
}
```

---

## 12. Comparacion con C y Rust

```
┌──────────────────────────────────────────────────────────────────────────┐
│  COMPOSICION DE INTERFACES — Go vs C vs Rust                             │
├──────────────┬──────────────────────┬──────────────────┬─────────────────┤
│ Aspecto      │ Go                   │ C                │ Rust            │
├──────────────┼──────────────────────┼──────────────────┼─────────────────┤
│ Componer     │ Embedding:           │ Structs con      │ Supertraits:    │
│ interfaces   │ type RW interface {  │ multiples fptrs  │ trait RW:       │
│              │   Reader; Writer     │ (manual)         │   Read + Write  │
│              │ }                    │                  │                 │
├──────────────┼──────────────────────┼──────────────────┼─────────────────┤
│ Satisfaccion │ Implicita — si tiene │ N/A (no hay      │ Explicita —     │
│ parcial      │ Read(), satisface    │ interfaces)      │ impl Read for T │
│              │ Reader aunque sea RW │                  │ separado de     │
│              │                      │                  │ impl Write for T│
├──────────────┼──────────────────────┼──────────────────┼─────────────────┤
│ Decorator    │ Embed interface en   │ Wrapper func +   │ Newtype wrapper │
│ pattern      │ struct + override    │ next fptr        │ impl Trait for  │
│              │ metodos              │                  │ Wrapper<T>      │
├──────────────┼──────────────────────┼──────────────────┼─────────────────┤
│ Sealed       │ Metodo unexported    │ N/A              │ pub trait in    │
│ interface    │ sealed()             │                  │ private module  │
├──────────────┼──────────────────────┼──────────────────┼─────────────────┤
│ Conflicto    │ Error de compilacion │ N/A              │ UFCS:           │
│ de nombres   │ si firmas difieren   │                  │ <T as Trait>::m │
│              │ OK si son identicas  │                  │ desambigua      │
├──────────────┼──────────────────────┼──────────────────┼─────────────────┤
│ NopCloser    │ io.NopCloser(r)      │ Wrapper manual   │ Blanket impl o │
│ adapter      │                      │                  │ newtype         │
├──────────────┼──────────────────────┼──────────────────┼─────────────────┤
│ Conversion   │ RWC → R implicita    │ N/A              │ &dyn RW → no   │
│ descendente  │ (assignment directo) │                  │ automatico a    │
│              │                      │                  │ &dyn Read       │
└──────────────┴──────────────────────┴──────────────────┴─────────────────┘
```

### Ejemplo: ReadWriter en los tres lenguajes

**C:**
```c
// C: composicion manual con structs de function pointers
typedef struct {
    ssize_t (*read)(void* self, char* buf, size_t n);
    ssize_t (*write)(void* self, const char* buf, size_t n);
    void* ctx;
} ReadWriter;

// Crear desde un fd:
ssize_t fd_read(void* self, char* buf, size_t n) {
    return read(*(int*)self, buf, n);
}
ssize_t fd_write(void* self, const char* buf, size_t n) {
    return write(*(int*)self, buf, n);
}

ReadWriter make_fd_rw(int* fd) {
    return (ReadWriter){.read = fd_read, .write = fd_write, .ctx = fd};
}
// 100% manual, sin verificacion de tipos
```

**Rust:**
```rust
use std::io::{Read, Write};

// Read y Write son traits separados
// Rust los compone con "trait bounds":

fn copy_data(src: &mut dyn Read, dst: &mut dyn Write) -> std::io::Result<u64> {
    std::io::copy(src, dst)
}

// O con supertrait custom:
trait ReadWrite: Read + Write {}
// Blanket impl:
impl<T: Read + Write> ReadWrite for T {}

// Requiere impl EXPLICITO para cada trait:
// impl Read for MyType { fn read(...) -> ... { ... } }
// impl Write for MyType { fn write(...) -> ... { ... } }
```

**Go:**
```go
import "io"

// ReadWriter = Reader + Writer (embedding)
// Ya definido en io — solo usarlo:

func CopyData(src io.Reader, dst io.Writer) (int64, error) {
    return io.Copy(dst, src)
}

// Cualquier tipo con Read() + Write() satisface io.ReadWriter
// automaticamente — sin declarar nada
```

---

## 13. Ejercicios de Prediccion

### Ejercicio 1: ¿Compila?

```go
type Pinger interface {
    Ping() error
}

type Closer interface {
    Close() error
}

type PingCloser interface {
    Pinger
    Closer
}

type Server struct{}
func (s *Server) Ping() error  { return nil }
func (s *Server) Close() error { return nil }

func main() {
    var pc PingCloser = &Server{}

    var p Pinger = pc  // linea A
    var c Closer = pc  // linea B

    var pc2 PingCloser = p // linea C

    _ = p; _ = c; _ = pc2
}
```

<details>
<summary>Respuesta</summary>

Lineas A y B **compilan** — conversion descendente de PingCloser a Pinger/Closer es implicita.

Linea C **no compila**: `cannot use p (variable of type Pinger) as PingCloser`. Pinger solo garantiza `Ping()`, no `Close()`. Necesitarias type assertion: `pc2 = p.(PingCloser)` (puede panic si p no satisface PingCloser).

</details>

### Ejercicio 2: ¿Cuantas interfaces satisface?

```go
type A interface { M1() }
type B interface { M2() }
type C interface { M3() }
type AB interface { A; B }
type BC interface { B; C }
type ABC interface { A; B; C }

type X struct{}
func (X) M1() {}
func (X) M2() {}
func (X) M3() {}
```

<details>
<summary>Respuesta</summary>

X satisface **7 interfaces**:
1. A (tiene M1)
2. B (tiene M2)
3. C (tiene M3)
4. AB (tiene M1 + M2)
5. BC (tiene M2 + M3)
6. ABC (tiene M1 + M2 + M3)
7. `any` (todo satisface any)

Ademas, satisface cualquier otra interfaz que sea subconjunto de {M1, M2, M3}.

</details>

### Ejercicio 3: ¿Que pasa?

```go
type Reader interface {
    Read(p []byte) (int, error)
}

type LoggingReader struct {
    Reader  // embedding de interfaz
}

func main() {
    lr := LoggingReader{} // sin inicializar Reader
    buf := make([]byte, 10)
    lr.Read(buf) // ¿que ocurre?
}
```

<details>
<summary>Respuesta</summary>

**PANIC en runtime**: `nil pointer dereference`. LoggingReader embebe la interfaz Reader, pero no se inicializo (es nil). Al llamar `lr.Read()`, Go intenta llamar el metodo Read en el Reader embebido, que es nil.

Fix: `lr := LoggingReader{Reader: strings.NewReader("hello")}`

Regla: cuando embebes una interfaz en un struct, SIEMPRE inicializala. Un campo de interfaz nil produce panic al llamar cualquier metodo.

</details>

### Ejercicio 4: ¿Compila?

```go
type A interface {
    Process() error
}

type B interface {
    Process() ([]byte, error) // misma firma? diferente retorno
}

type AB interface {
    A
    B
}
```

<details>
<summary>Respuesta</summary>

**No compila**: `duplicate method Process`. A.Process retorna `error`, B.Process retorna `([]byte, error)` — firmas diferentes. Go no permite metodos con el mismo nombre y firmas diferentes en una interfaz compuesta. No hay mecanismo de desambiguacion.

Si las firmas fueran identicas (ambas `Process() error`), compilaria sin problemas — Go las unifica en un solo metodo.

</details>

### Ejercicio 5: ¿Funciona el decorator?

```go
type Writer interface {
    Write(p []byte) (int, error)
}

type CountingWriter struct {
    Writer
    BytesWritten int64
}

func (cw *CountingWriter) Write(p []byte) (int, error) {
    n, err := cw.Writer.Write(p)
    cw.BytesWritten += int64(n)
    return n, err
}

func main() {
    var buf bytes.Buffer
    cw := &CountingWriter{Writer: &buf}
    fmt.Fprintln(cw, "hello, world")
    fmt.Println("bytes written:", cw.BytesWritten)
    fmt.Println("buffer content:", buf.String())
}
```

<details>
<summary>Respuesta</summary>

**Funciona correctamente**. Output:
```
bytes written: 13
buffer content: hello, world
```

CountingWriter embebe la interfaz Writer (inicializada con &buf). Su metodo Write overridea el embebido, cuenta bytes, y delega al Writer subyacente. `fmt.Fprintln(cw, ...)` llama a `cw.Write()` que es el override. Los 13 bytes son "hello, world\n".

</details>

---

## 14. Programa Completo: Composable Infrastructure Pipeline con Interfaces Compuestas

```go
// composable_pipeline.go
// Demuestra composicion de interfaces para construir un pipeline
// de operaciones de infraestructura donde cada etapa requiere
// diferentes combinaciones de capacidades.
package main

import (
    "fmt"
    "math/rand"
    "sort"
    "strings"
    "time"
)

// =====================================================================
// INTERFACES ATOMICAS — 1 metodo cada una
// =====================================================================

type Discoverer interface {
    Discover() ([]string, error) // descubrir recursos
}

type Inspector interface {
    Inspect(name string) (ResourceInfo, error) // inspeccionar un recurso
}

type Executor interface {
    Execute(name string, cmd Command) (Result, error) // ejecutar comando
}

type Reporter interface {
    Report(results []StageResult) string // formatear reporte
}

type Notifier interface {
    Notify(summary string) error // enviar notificacion
}

type Logger interface {
    Log(level, msg string)
}

// =====================================================================
// INTERFACES COMPUESTAS — combinaciones para diferentes etapas
// =====================================================================

// DiscoverInspector: descubrir + inspeccionar (inventory stage)
type DiscoverInspector interface {
    Discoverer
    Inspector
}

// InspectExecutor: inspeccionar + ejecutar (remediation stage)
type InspectExecutor interface {
    Inspector
    Executor
}

// FullOperator: todas las operaciones sobre recursos
type FullOperator interface {
    Discoverer
    Inspector
    Executor
}

// ReportNotifier: reportar + notificar (output stage)
type ReportNotifier interface {
    Reporter
    Notifier
}

// =====================================================================
// TIPOS DE DATO
// =====================================================================

type ResourceInfo struct {
    Name       string
    Type       string
    Status     string
    CPU        float64
    MemoryMB   int
    DiskUsePct float64
    Uptime     time.Duration
    Tags       map[string]string
}

func (r ResourceInfo) String() string {
    return fmt.Sprintf("%-16s %-8s %-10s CPU:%.0f%% Mem:%dMB Disk:%.0f%%",
        r.Name, r.Type, r.Status, r.CPU, r.MemoryMB, r.DiskUsePct)
}

type Command struct {
    Action string
    Args   map[string]string
}

func (c Command) String() string {
    var args []string
    for k, v := range c.Args {
        args = append(args, k+"="+v)
    }
    sort.Strings(args)
    return fmt.Sprintf("%s(%s)", c.Action, strings.Join(args, ", "))
}

type Result struct {
    Success bool
    Message string
}

type StageResult struct {
    Stage    string
    Resource string
    Status   string
    Message  string
    Duration time.Duration
}

// =====================================================================
// IMPLEMENTACION: SimulatedInfra — satisface TODAS las interfaces atomicas
// =====================================================================

type SimulatedInfra struct {
    resources map[string]ResourceInfo
}

func NewSimulatedInfra() *SimulatedInfra {
    return &SimulatedInfra{
        resources: map[string]ResourceInfo{
            "web-01":    {Name: "web-01", Type: "server", Status: "running", CPU: 82, MemoryMB: 3200, DiskUsePct: 71, Uptime: 72 * time.Hour, Tags: map[string]string{"role": "frontend", "env": "prod"}},
            "web-02":    {Name: "web-02", Type: "server", Status: "running", CPU: 15, MemoryMB: 1200, DiskUsePct: 45, Uptime: 72 * time.Hour, Tags: map[string]string{"role": "frontend", "env": "prod"}},
            "api-01":    {Name: "api-01", Type: "server", Status: "running", CPU: 45, MemoryMB: 2800, DiskUsePct: 88, Uptime: 24 * time.Hour, Tags: map[string]string{"role": "backend", "env": "prod"}},
            "db-01":     {Name: "db-01", Type: "server", Status: "running", CPU: 92, MemoryMB: 7500, DiskUsePct: 65, Uptime: 168 * time.Hour, Tags: map[string]string{"role": "database", "env": "prod"}},
            "worker-01": {Name: "worker-01", Type: "server", Status: "stopped", CPU: 0, MemoryMB: 0, DiskUsePct: 30, Uptime: 0, Tags: map[string]string{"role": "worker", "env": "prod"}},
            "cache-01":  {Name: "cache-01", Type: "server", Status: "running", CPU: 35, MemoryMB: 4096, DiskUsePct: 12, Uptime: 48 * time.Hour, Tags: map[string]string{"role": "cache", "env": "prod"}},
        },
    }
}

// Discover satisface Discoverer
func (s *SimulatedInfra) Discover() ([]string, error) {
    names := make([]string, 0, len(s.resources))
    for name := range s.resources {
        names = append(names, name)
    }
    sort.Strings(names)
    return names, nil
}

// Inspect satisface Inspector
func (s *SimulatedInfra) Inspect(name string) (ResourceInfo, error) {
    info, ok := s.resources[name]
    if !ok {
        return ResourceInfo{}, fmt.Errorf("resource %q not found", name)
    }
    return info, nil
}

// Execute satisface Executor
func (s *SimulatedInfra) Execute(name string, cmd Command) (Result, error) {
    info, ok := s.resources[name]
    if !ok {
        return Result{}, fmt.Errorf("resource %q not found", name)
    }

    time.Sleep(time.Duration(rand.Intn(50)) * time.Millisecond)

    switch cmd.Action {
    case "restart":
        info.Status = "running"
        info.CPU = 5 + rand.Float64()*20
        info.Uptime = 0
        s.resources[name] = info
        return Result{Success: true, Message: fmt.Sprintf("%s restarted", name)}, nil

    case "cleanup":
        info.DiskUsePct = info.DiskUsePct * 0.5
        s.resources[name] = info
        return Result{Success: true, Message: fmt.Sprintf("%s disk cleaned: %.0f%% → %.0f%%",
            name, info.DiskUsePct*2, info.DiskUsePct)}, nil

    case "scale-down":
        info.CPU = info.CPU * 0.7
        info.MemoryMB = int(float64(info.MemoryMB) * 0.7)
        s.resources[name] = info
        return Result{Success: true, Message: fmt.Sprintf("%s scaled down", name)}, nil

    default:
        return Result{Success: false, Message: "unknown action: " + cmd.Action}, nil
    }
}

// Compile-time assertions
var _ Discoverer = (*SimulatedInfra)(nil)
var _ Inspector = (*SimulatedInfra)(nil)
var _ Executor = (*SimulatedInfra)(nil)
var _ DiscoverInspector = (*SimulatedInfra)(nil)
var _ InspectExecutor = (*SimulatedInfra)(nil)
var _ FullOperator = (*SimulatedInfra)(nil)

// =====================================================================
// IMPLEMENTACION: TableReporter satisface Reporter
// =====================================================================

type TableReporter struct{}

func (TableReporter) Report(results []StageResult) string {
    var b strings.Builder
    fmt.Fprintf(&b, "  %-14s %-16s %-10s %-8s %s\n",
        "STAGE", "RESOURCE", "STATUS", "TIME", "MESSAGE")
    fmt.Fprintf(&b, "  %s\n", strings.Repeat("─", 70))
    for _, r := range results {
        fmt.Fprintf(&b, "  %-14s %-16s %-10s %6s  %s\n",
            r.Stage, r.Resource, r.Status,
            r.Duration.Round(time.Millisecond), r.Message)
    }
    return b.String()
}

var _ Reporter = TableReporter{}

// =====================================================================
// IMPLEMENTACION: ConsoleNotifier satisface Notifier
// =====================================================================

type ConsoleNotifier struct {
    Channel string
}

func (c *ConsoleNotifier) Notify(summary string) error {
    fmt.Printf("  [NOTIFY → #%s]\n%s\n", c.Channel, summary)
    return nil
}

var _ Notifier = (*ConsoleNotifier)(nil)

// =====================================================================
// IMPLEMENTACION: ConsoleLogger satisface Logger
// =====================================================================

type ConsoleLogger struct{}

func (ConsoleLogger) Log(level, msg string) {
    fmt.Printf("  [%-5s] %s\n", level, msg)
}

var _ Logger = ConsoleLogger{}

// =====================================================================
// PIPELINE STAGES — cada una pide la interfaz compuesta MINIMA
// =====================================================================

// Stage 1: Inventory — necesita Discoverer + Inspector
func StageInventory(di DiscoverInspector, log Logger) ([]ResourceInfo, []StageResult) {
    log.Log("INFO", "Starting inventory stage")
    start := time.Now()

    names, err := di.Discover()
    if err != nil {
        log.Log("ERROR", "Discovery failed: "+err.Error())
        return nil, nil
    }

    var resources []ResourceInfo
    var results []StageResult

    for _, name := range names {
        info, err := di.Inspect(name)
        if err != nil {
            results = append(results, StageResult{
                Stage: "inventory", Resource: name, Status: "error",
                Message: err.Error(), Duration: time.Since(start),
            })
            continue
        }
        resources = append(resources, info)
        results = append(results, StageResult{
            Stage: "inventory", Resource: name, Status: "ok",
            Message: info.String(), Duration: time.Since(start),
        })
    }

    log.Log("INFO", fmt.Sprintf("Inventory complete: %d resources found", len(resources)))
    return resources, results
}

// Stage 2: Analysis — solo necesita la data, no interfaces
func StageAnalysis(resources []ResourceInfo, log Logger) ([]Command, []StageResult) {
    log.Log("INFO", "Starting analysis stage")
    var commands []Command
    var results []StageResult

    for _, r := range resources {
        // High CPU
        if r.CPU > 80 {
            cmd := Command{Action: "scale-down", Args: map[string]string{
                "target": r.Name, "reason": fmt.Sprintf("CPU=%.0f%%", r.CPU)}}
            commands = append(commands, cmd)
            results = append(results, StageResult{
                Stage: "analysis", Resource: r.Name, Status: "action",
                Message: fmt.Sprintf("HIGH CPU (%.0f%%) → %s", r.CPU, cmd),
            })
        }
        // High disk
        if r.DiskUsePct > 85 {
            cmd := Command{Action: "cleanup", Args: map[string]string{
                "target": r.Name, "reason": fmt.Sprintf("Disk=%.0f%%", r.DiskUsePct)}}
            commands = append(commands, cmd)
            results = append(results, StageResult{
                Stage: "analysis", Resource: r.Name, Status: "action",
                Message: fmt.Sprintf("HIGH DISK (%.0f%%) → %s", r.DiskUsePct, cmd),
            })
        }
        // Stopped
        if r.Status == "stopped" {
            cmd := Command{Action: "restart", Args: map[string]string{
                "target": r.Name, "reason": "stopped"}}
            commands = append(commands, cmd)
            results = append(results, StageResult{
                Stage: "analysis", Resource: r.Name, Status: "action",
                Message: "STOPPED → " + cmd.String(),
            })
        }
    }

    if len(commands) == 0 {
        log.Log("INFO", "Analysis complete: no actions needed")
    } else {
        log.Log("WARN", fmt.Sprintf("Analysis complete: %d actions planned", len(commands)))
    }
    return commands, results
}

// Stage 3: Remediation — necesita Inspector + Executor
func StageRemediation(ie InspectExecutor, commands []Command, log Logger) []StageResult {
    log.Log("INFO", "Starting remediation stage")
    var results []StageResult

    for _, cmd := range commands {
        target := cmd.Args["target"]
        start := time.Now()

        // Pre-check
        info, err := ie.Inspect(target)
        if err != nil {
            results = append(results, StageResult{
                Stage: "remediate", Resource: target, Status: "error",
                Message: "pre-check failed: " + err.Error(), Duration: time.Since(start),
            })
            continue
        }

        log.Log("INFO", fmt.Sprintf("Executing %s on %s (current: %s)",
            cmd.Action, target, info.Status))

        // Execute
        result, err := ie.Execute(target, cmd)
        duration := time.Since(start)

        if err != nil {
            results = append(results, StageResult{
                Stage: "remediate", Resource: target, Status: "error",
                Message: err.Error(), Duration: duration,
            })
            log.Log("ERROR", fmt.Sprintf("  %s FAILED: %v", target, err))
        } else if result.Success {
            results = append(results, StageResult{
                Stage: "remediate", Resource: target, Status: "success",
                Message: result.Message, Duration: duration,
            })
            log.Log("INFO", fmt.Sprintf("  %s OK: %s", target, result.Message))
        } else {
            results = append(results, StageResult{
                Stage: "remediate", Resource: target, Status: "failed",
                Message: result.Message, Duration: duration,
            })
            log.Log("WARN", fmt.Sprintf("  %s FAILED: %s", target, result.Message))
        }
    }

    return results
}

// Stage 4: Output — necesita Reporter + Notifier
func StageOutput(rn ReportNotifier, allResults []StageResult, log Logger) {
    log.Log("INFO", "Generating report and notifications")

    report := rn.Report(allResults)
    fmt.Println(report)

    // Summary for notification
    var actions, successes, failures int
    for _, r := range allResults {
        if r.Stage == "remediate" {
            actions++
            if r.Status == "success" {
                successes++
            } else {
                failures++
            }
        }
    }

    summary := fmt.Sprintf("Pipeline complete: %d resources, %d actions, %d success, %d failed",
        countByStage(allResults, "inventory"), actions, successes, failures)

    rn.Notify(summary)
}

func countByStage(results []StageResult, stage string) int {
    count := 0
    for _, r := range results {
        if r.Stage == stage {
            count++
        }
    }
    return count
}

// =====================================================================
// COMBINER: ReportNotifierCombo — componer dos tipos en una interfaz
// =====================================================================

// A veces tienes dos tipos separados que juntos satisfacen una interfaz compuesta.
// Un struct que los embebe crea la interfaz compuesta:

type ReportNotifierCombo struct {
    Reporter
    Notifier
}

var _ ReportNotifier = (*ReportNotifierCombo)(nil)

// =====================================================================
// MAIN
// =====================================================================

func main() {
    fmt.Println("╔══════════════════════════════════════════════════════════╗")
    fmt.Println("║   Composable Infrastructure Pipeline                    ║")
    fmt.Println("╚══════════════════════════════════════════════════════════╝")

    // Create components
    infra := NewSimulatedInfra()
    logger := ConsoleLogger{}
    reporter := TableReporter{}
    notifier := &ConsoleNotifier{Channel: "ops-alerts"}

    // Compose Reporter + Notifier into ReportNotifier
    output := &ReportNotifierCombo{
        Reporter: reporter,
        Notifier: notifier,
    }

    var allResults []StageResult

    // Stage 1: Inventory (needs DiscoverInspector)
    fmt.Println("\n── Stage 1: Inventory (DiscoverInspector) ──")
    resources, invResults := StageInventory(infra, logger)
    allResults = append(allResults, invResults...)

    fmt.Println("\n  Resources found:")
    for _, r := range resources {
        fmt.Printf("    %s\n", r)
    }

    // Stage 2: Analysis (pure function — no interfaces needed)
    fmt.Println("\n── Stage 2: Analysis (pure data) ──")
    commands, anaResults := StageAnalysis(resources, logger)
    allResults = append(allResults, anaResults...)

    fmt.Println("\n  Planned actions:")
    for _, cmd := range commands {
        fmt.Printf("    %s on %s\n", cmd.Action, cmd.Args["target"])
    }

    // Stage 3: Remediation (needs InspectExecutor)
    fmt.Println("\n── Stage 3: Remediation (InspectExecutor) ──")
    remResults := StageRemediation(infra, commands, logger)
    allResults = append(allResults, remResults...)

    // Stage 4: Output (needs ReportNotifier — composed interface)
    fmt.Println("\n── Stage 4: Report & Notify (ReportNotifier) ──")
    StageOutput(output, allResults, logger)

    // Show interface composition
    fmt.Println("\n── Interface Composition Map ──")
    fmt.Println("  Atomicas:")
    fmt.Println("    Discoverer    (1 method) → Discover()")
    fmt.Println("    Inspector     (1 method) → Inspect()")
    fmt.Println("    Executor      (1 method) → Execute()")
    fmt.Println("    Reporter      (1 method) → Report()")
    fmt.Println("    Notifier      (1 method) → Notify()")
    fmt.Println("    Logger        (1 method) → Log()")
    fmt.Println("  Compuestas:")
    fmt.Println("    DiscoverInspector = Discoverer + Inspector")
    fmt.Println("    InspectExecutor   = Inspector + Executor")
    fmt.Println("    FullOperator      = Discoverer + Inspector + Executor")
    fmt.Println("    ReportNotifier    = Reporter + Notifier")
    fmt.Println("  SimulatedInfra satisface: 6 interfaces (3 atomicas + 3 compuestas)")
    fmt.Println("  Cada stage pide SOLO la combinacion que necesita")
}
```

**Output esperado (parcial):**
```
╔══════════════════════════════════════════════════════════╗
║   Composable Infrastructure Pipeline                    ║
╚══════════════════════════════════════════════════════════╝

── Stage 1: Inventory (DiscoverInspector) ──
  [INFO ] Starting inventory stage
  [INFO ] Inventory complete: 6 resources found

  Resources found:
    api-01           server   running    CPU:45% Mem:2800MB Disk:88%
    cache-01         server   running    CPU:35% Mem:4096MB Disk:12%
    db-01            server   running    CPU:92% Mem:7500MB Disk:65%
    web-01           server   running    CPU:82% Mem:3200MB Disk:71%
    web-02           server   running    CPU:15% Mem:1200MB Disk:45%
    worker-01        server   stopped    CPU:0% Mem:0MB Disk:30%

── Stage 2: Analysis (pure data) ──
  [WARN ] Analysis complete: 4 actions planned

  Planned actions:
    cleanup on api-01
    scale-down on db-01
    scale-down on web-01
    restart on worker-01

── Stage 3: Remediation (InspectExecutor) ──
  [INFO ] Starting remediation stage
  [INFO ] Executing cleanup on api-01 (current: running)
  [INFO ]   api-01 OK: api-01 disk cleaned: 88% → 44%
  ...

── Stage 4: Report & Notify (ReportNotifier) ──
  STAGE          RESOURCE         STATUS     TIME    MESSAGE
  ──────────────────────────────────────────────────────────────────────
  inventory      api-01           ok           0s    api-01 ...
  ...
  remediate      api-01           success     23ms   api-01 disk cleaned
  remediate      db-01            success     45ms   db-01 scaled down
  ...

  [NOTIFY → #ops-alerts]
  Pipeline complete: 6 resources, 4 actions, 4 success, 0 failed
```

---

## 15. Resumen

```
┌──────────────────────────────────────────────────────────────────────────┐
│  COMPOSICION DE INTERFACES — Resumen                                     │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Embedding de interfaces en interfaces:                                  │
│  ├─ type RW interface { Reader; Writer } — union de metodos             │
│  ├─ Equivalente a listar los metodos, pero DRY y autodocumentado        │
│  ├─ Metodos identicos: OK (se unifican)                                 │
│  ├─ Metodos con firmas diferentes: ERROR de compilacion                 │
│  └─ Puede agregar metodos propios ademas de los embebidos               │
│                                                                          │
│  Embedding de interfaz en struct (decorators):                           │
│  ├─ struct { io.Reader } — el struct "hereda" Read()                    │
│  ├─ Override: definir Read() en el struct reemplaza el embebido         │
│  ├─ Delegacion: self.Reader.Read() llama al embebido                    │
│  ├─ SIEMPRE inicializar — interfaz nil embebida = panic                 │
│  └─ Patron: LoggingReader, StatusRecorder, CountingWriter               │
│                                                                          │
│  Satisfaccion descendente:                                               │
│  ├─ RWC → RW → R implicita (assignment directo)                        │
│  ├─ R → RW NO compila (falta Write)                                    │
│  └─ Un tipo que satisface la compuesta satisface todas las partes       │
│                                                                          │
│  Patrones:                                                               │
│  ├─ Interface upgrading: Reader → ReadWriter → ReadWriteLister          │
│  ├─ Composicion ad-hoc: combinar solo cuando una funcion lo necesita     │
│  ├─ Adapter: NopCloser, WithName — completar interfaces parciales       │
│  ├─ Combiner struct: embed 2 interfaces → satisfacer la compuesta       │
│  └─ Sealed interface: metodo unexported impide impl externa             │
│                                                                          │
│  stdlib:                                                                 │
│  ├─ io: Reader, Writer, Closer → ReadWriter, ReadCloser, ReadWriteCloser│
│  ├─ *os.File satisface TODAS las combinaciones de io                     │
│  └─ io.NopCloser: Reader → ReadCloser (adapter estandar)                │
│                                                                          │
│  vs C: composicion manual con structs de function pointers               │
│  vs Rust: supertraits (trait A: B + C) + impl explicito por trait       │
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: T03 - nil interface vs interface con valor nil — la trampa clasica de Go, por que interface(nil) ≠ (*T)(nil)
