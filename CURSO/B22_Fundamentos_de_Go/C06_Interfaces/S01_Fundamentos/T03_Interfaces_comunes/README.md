# Interfaces Comunes — io.Reader, io.Writer, fmt.Stringer, error, sort.Interface

## 1. Introduccion

Go no tiene una jerarquia de clases ni herencia. En su lugar, tiene un ecosistema de **interfaces pequeñas y ubicuas** definidas en la biblioteca estandar que actuan como puntos de integracion universal. Si tu tipo satisface `io.Reader`, automaticamente funciona con `io.Copy`, `bufio.Scanner`, `json.Decoder`, `gzip.NewReader`, `http.Response.Body`, `crypto/tls`, y cientos de funciones mas — sin importar que tu tipo sea un archivo, un socket, un buffer en memoria, o un descompresor.

Estas interfaces son el "pegamento" de Go. Entenderlas es entender como diseñar software en Go — son el vocabulario compartido entre todos los paquetes del ecosistema. Cada una sigue el principio de interfaces pequeñas: **un metodo, un contrato claro, composicion infinita**.

En SysAdmin/DevOps, estas interfaces aparecen constantemente: leer archivos de configuracion (`io.Reader`), escribir logs (`io.Writer`), formatear objetos para debugging (`fmt.Stringer`), propagar errores con contexto (`error`), ordenar inventarios (`sort.Interface`), y cerrar recursos (`io.Closer`). Dominar estas 6-7 interfaces es dominar el 80% de la interoperabilidad en Go.

```
┌─────────────────────────────────────────────────────────────────────────┐
│              INTERFACES COMUNES DE LA STDLIB                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Interface        │ Package │ Metodo(s)              │ Proposito         │
│  ─────────────────┼─────────┼────────────────────────┼─────────────────  │
│  io.Reader        │ io      │ Read([]byte)(int,error)│ Leer bytes        │
│  io.Writer        │ io      │ Write([]byte)(int,err) │ Escribir bytes    │
│  io.Closer        │ io      │ Close() error          │ Liberar recurso   │
│  io.ReadWriter    │ io      │ Reader + Writer        │ Leer y escribir   │
│  io.ReadCloser    │ io      │ Reader + Closer        │ Leer y cerrar     │
│  io.WriteCloser   │ io      │ Writer + Closer        │ Escribir y cerrar │
│  io.ReadWriteCloser│io      │ Reader+Writer+Closer   │ Todo              │
│  io.ReaderFrom    │ io      │ ReadFrom(Reader)(int64)│ Optimizar copia   │
│  io.WriterTo      │ io      │ WriteTo(Writer)(int64) │ Optimizar copia   │
│  io.Seeker        │ io      │ Seek(int64,int)(int64) │ Posicion random   │
│  fmt.Stringer     │ fmt     │ String() string        │ Texto legible     │
│  fmt.GoStringer   │ fmt     │ GoString() string      │ Repr Go (%#v)     │
│  error            │ builtin │ Error() string         │ Errores           │
│  sort.Interface   │ sort    │ Len,Less,Swap          │ Ordenamiento      │
│  encoding.TextMarshaler│encoding│MarshalText()([]byte)│ Texto serial.   │
│  json.Marshaler   │ json    │ MarshalJSON()([]byte)  │ JSON custom       │
│  io.StringWriter  │ io      │ WriteString(string)    │ Escribir strings  │
│  net.Error        │ net     │ Error()+Timeout()+Temp │ Errores de red    │
│  http.Handler     │ net/http│ ServeHTTP(w,r)         │ HTTP handler      │
│  http.ResponseWriter│net/http│Write+WriteHeader+Header│HTTP response    │
│  context.Context  │ context │ Deadline,Done,Err,Value│ Cancelacion       │
│                                                                         │
│  Regla: si la stdlib tiene una interfaz para lo que haces, USALA.       │
│  No inventes la tuya propia.                                            │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. io.Reader — La Interfaz mas Importante de Go

### Definicion

```go
// package io

type Reader interface {
    Read(p []byte) (n int, err error)
}
```

Un solo metodo. El contrato:
- `p` es un buffer **proporcionado por el caller** — Reader escribe datos DENTRO de `p`
- Retorna `n` = numero de bytes escritos en `p` (0 ≤ n ≤ len(p))
- Retorna `err` = `nil` si hay mas datos, `io.EOF` si se acabo, o un error real
- **Puede retornar n > 0 Y err != nil al mismo tiempo** (datos parciales + error)
- El caller es responsable de llamar Read en un loop hasta `io.EOF`

```
┌──────────────────────────────────────────────────────────────────────┐
│  CONTRATO DE io.Reader                                               │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Caller proporciona buffer:    p := make([]byte, 4096)               │
│  Reader llena el buffer:       n, err := reader.Read(p)              │
│  Datos leidos:                 p[:n] ← estos son los datos           │
│                                                                      │
│  Combinaciones validas:                                              │
│  ┌──────────┬──────────┬──────────────────────────────────────────┐  │
│  │ n        │ err      │ Significado                              │  │
│  ├──────────┼──────────┼──────────────────────────────────────────┤  │
│  │ > 0      │ nil      │ Datos leidos, probablemente hay mas     │  │
│  │ > 0      │ io.EOF   │ Ultimos datos + fin del stream          │  │
│  │ > 0      │ error    │ Datos parciales + error                 │  │
│  │ 0        │ io.EOF   │ Fin del stream, sin mas datos           │  │
│  │ 0        │ error    │ Error, sin datos                        │  │
│  │ 0        │ nil      │ NO deberia pasar (pero manejar)         │  │
│  └──────────┴──────────┴──────────────────────────────────────────┘  │
│                                                                      │
│  REGLA: siempre procesar p[:n] ANTES de verificar err                │
└──────────────────────────────────────────────────────────────────────┘
```

### Tipos que satisfacen io.Reader

```go
// Todo esto satisface io.Reader:
import (
    "bytes"
    "compress/gzip"
    "crypto/rand"
    "io"
    "net"
    "net/http"
    "os"
    "strings"
)

// Archivos
var _ io.Reader = (*os.File)(nil)

// Buffers en memoria
var _ io.Reader = (*bytes.Buffer)(nil)
var _ io.Reader = (*bytes.Reader)(nil)
var _ io.Reader = (*strings.Reader)(nil)

// Red
var _ io.Reader = (net.Conn)(nil)        // TCP/UDP connections
// http.Response.Body es io.ReadCloser

// Compresion
var _ io.Reader = (*gzip.Reader)(nil)    // descomprime al leer

// Crypto
var _ io.Reader = (crypto_rand.Reader)   // /dev/urandom

// Pipe
// pr, pw := io.Pipe()  // pr es *io.PipeReader → Reader
```

### Leer correctamente de un Reader

```go
package main

import (
    "fmt"
    "io"
    "strings"
)

func main() {
    r := strings.NewReader("Hello, SysAdmin world! This is a longer message for demo purposes.")

    // ✗ INCORRECTO: un solo Read no garantiza todos los datos
    // buf := make([]byte, 1024)
    // n, _ := r.Read(buf)
    // // Si el reader es un socket, n podria ser 5 la primera vez

    // ✓ CORRECTO: loop hasta EOF
    buf := make([]byte, 16) // buffer pequeño para demostrar el loop
    for {
        n, err := r.Read(buf)
        if n > 0 {
            // PRIMERO procesar datos
            fmt.Printf("  read %d bytes: %q\n", n, string(buf[:n]))
        }
        if err == io.EOF {
            break // fin del stream
        }
        if err != nil {
            fmt.Println("error:", err)
            break
        }
    }

    // ✓ MAS SIMPLE: usar io.ReadAll (lee todo en memoria)
    r2 := strings.NewReader("small config file content")
    data, err := io.ReadAll(r2)
    if err != nil {
        fmt.Println("error:", err)
        return
    }
    fmt.Printf("\n  ReadAll: %q (%d bytes)\n", string(data), len(data))
}
```

### Composicion de Readers — el poder real

```go
// io.Reader se compone como bloques LEGO:

import (
    "compress/gzip"
    "crypto/sha256"
    "io"
    "os"
)

// Leer un archivo gzipped, calcular SHA256, y copiar a stdout:
func ProcessGzippedFile(path string) error {
    // Capa 1: archivo en disco
    file, err := os.Open(path)
    if err != nil {
        return err
    }
    defer file.Close()

    // Capa 2: descompresor gzip (envuelve al Reader anterior)
    gz, err := gzip.NewReader(file)
    if err != nil {
        return err
    }
    defer gz.Close()

    // Capa 3: TeeReader — bifurca: lo que se lee va tambien al hasher
    hasher := sha256.New()
    tee := io.TeeReader(gz, hasher)

    // Capa 4: LimitReader — leer maximo 1MB
    limited := io.LimitReader(tee, 1024*1024)

    // Copiar todo a stdout
    n, err := io.Copy(os.Stdout, limited)
    if err != nil {
        return err
    }

    fmt.Fprintf(os.Stderr, "\n--- %d bytes, SHA256: %x ---\n", n, hasher.Sum(nil))
    return nil
}

// Pipeline: file → gzip.Reader → TeeReader → LimitReader → stdout
//                                     └→ sha256.Hash
//
// Cada capa es un io.Reader que envuelve otro io.Reader.
// NINGUNA capa sabe que hay debajo — total desacoplamiento.
```

### Readers utiles de io

```go
// io.LimitReader — limitar bytes leidos (proteccion contra input gigante)
limited := io.LimitReader(r, 1024*1024) // max 1MB

// io.TeeReader — bifurcar: leer de r, copiar a w simultaneamente
tee := io.TeeReader(r, logWriter) // lo leido tambien se escribe en logWriter

// io.MultiReader — concatenar multiples readers en secuencia
combined := io.MultiReader(header, body, footer) // lee header, luego body, luego footer

// io.NopCloser — convertir Reader en ReadCloser (Close no hace nada)
rc := io.NopCloser(strings.NewReader("data"))
defer rc.Close() // no-op, pero satisface ReadCloser

// io.SectionReader — leer una porcion de un ReaderAt
section := io.NewSectionReader(file, 100, 200) // 200 bytes desde offset 100
```

---

## 3. io.Writer — La Contraparte de Reader

### Definicion

```go
type Writer interface {
    Write(p []byte) (n int, err error)
}
```

Contrato:
- `p` contiene los datos a escribir
- Retorna `n` = bytes escritos (0 ≤ n ≤ len(p))
- Si `n < len(p)`, err **debe** ser no-nil (escritura parcial = error)
- Writer **no debe** modificar `p` ni retenerlo despues de retornar

### Tipos que satisfacen io.Writer

```go
var _ io.Writer = (*os.File)(nil)         // archivos, stdout, stderr
var _ io.Writer = (*bytes.Buffer)(nil)    // buffer en memoria
var _ io.Writer = (*strings.Builder)(nil) // construir strings eficientemente
var _ io.Writer = (net.Conn)(nil)         // conexiones de red
var _ io.Writer = (*gzip.Writer)(nil)     // compresion
var _ io.Writer = (*bufio.Writer)(nil)    // buffered writing
var _ io.Writer = (http.ResponseWriter)(nil) // HTTP responses
```

### Escribir en un Writer

```go
package main

import (
    "bytes"
    "fmt"
    "io"
    "os"
)

func main() {
    // ---- Escribir a stdout ----
    os.Stdout.Write([]byte("direct write to stdout\n"))

    // ---- Escribir a un buffer ----
    var buf bytes.Buffer
    buf.Write([]byte("hello "))
    buf.WriteString("world\n") // bytes.Buffer tiene WriteString extra
    fmt.Println("buffer:", buf.String())

    // ---- fmt.Fprintf: escribir formateado a cualquier Writer ----
    fmt.Fprintf(os.Stderr, "error count: %d\n", 42)
    fmt.Fprintf(&buf, "formatted: %s=%d\n", "port", 8080)
    fmt.Println("buffer after Fprintf:", buf.String())

    // ---- io.WriteString: escribir string a cualquier Writer ----
    io.WriteString(os.Stdout, "via io.WriteString\n")

    // ---- MultiWriter: escribir a multiples destinos ----
    multi := io.MultiWriter(os.Stdout, &buf)
    fmt.Fprintln(multi, "this goes to BOTH stdout AND buffer")
    fmt.Println("buffer now:", buf.String())
}
```

### Patron SysAdmin: log a archivo + stdout

```go
func SetupDualLogging(logPath string) (io.Writer, func(), error) {
    file, err := os.OpenFile(logPath, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0644)
    if err != nil {
        return nil, nil, err
    }

    // MultiWriter: cada Write va a AMBOS destinos
    dual := io.MultiWriter(os.Stdout, file)

    cleanup := func() {
        file.Close()
    }

    return dual, cleanup, nil
}

// Uso:
// logger, cleanup, err := SetupDualLogging("/var/log/myapp.log")
// defer cleanup()
// fmt.Fprintln(logger, "this appears in terminal AND log file")
```

### Writers utiles

```go
// io.Discard — /dev/null, descarta todo lo escrito
n, _ := io.Copy(io.Discard, r) // contar bytes sin almacenar

// io.MultiWriter — tee: escribir a multiples destinos
both := io.MultiWriter(file, os.Stdout)

// io.Pipe — pipe sincrono entre goroutines
pr, pw := io.Pipe()
go func() {
    fmt.Fprintln(pw, "from goroutine")
    pw.Close()
}()
data, _ := io.ReadAll(pr)
```

---

## 4. io.Closer y las Interfaces Compuestas de io

### io.Closer

```go
type Closer interface {
    Close() error
}

// Patron fundamental: defer Close()
func ReadConfig(path string) ([]byte, error) {
    f, err := os.Open(path)
    if err != nil {
        return nil, err
    }
    defer f.Close() // SIEMPRE cerrar recursos

    return io.ReadAll(f)
}
```

### Interfaces compuestas del paquete io

Go define combinaciones utiles usando embedding de interfaces:

```go
// Composiciones en el paquete io:
type ReadWriter interface {
    Reader
    Writer
}

type ReadCloser interface {
    Reader
    Closer
}

type WriteCloser interface {
    Writer
    Closer
}

type ReadWriteCloser interface {
    Reader
    Writer
    Closer
}

type ReadSeeker interface {
    Reader
    Seeker
}

type ReadWriteSeeker interface {
    Reader
    Writer
    Seeker
}
```

```
┌──────────────────────────────────────────────────────────────────────┐
│  COMPOSICION DE INTERFACES io.*                                      │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│                        Reader                                        │
│                       /  |   \                                       │
│                      /   |    \                                      │
│             ReadWriter  ReadCloser  ReadSeeker                       │
│                 |    \   |                                           │
│                 |     ReadWriteCloser                                │
│                 |                                                    │
│                Writer ── WriteCloser                                 │
│                                                                      │
│  *os.File satisface TODAS estas interfaces                           │
│  (tiene Read, Write, Close, Seek)                                   │
│                                                                      │
│  net.Conn satisface ReadWriteCloser                                  │
│  (tiene Read, Write, Close — no Seek)                               │
│                                                                      │
│  *bytes.Buffer satisface ReadWriter                                  │
│  (tiene Read, Write — no Close ni Seek)                             │
└──────────────────────────────────────────────────────────────────────┘
```

### io.Copy — la funcion puente entre Reader y Writer

```go
// func Copy(dst Writer, src Reader) (written int64, err error)

// Copiar archivo
func CopyFile(src, dst string) error {
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

    _, err = io.Copy(out, in)
    return err
}

// Descargar URL a archivo
func DownloadFile(url, path string) error {
    resp, err := http.Get(url)
    if err != nil {
        return err
    }
    defer resp.Body.Close() // Body es io.ReadCloser

    out, err := os.Create(path)
    if err != nil {
        return err
    }
    defer out.Close()

    _, err = io.Copy(out, resp.Body) // Writer ← Reader
    return err
}
```

---

## 5. fmt.Stringer — Representacion Textual Legible

### Definicion

```go
// package fmt

type Stringer interface {
    String() string
}
```

Si tu tipo implementa `String() string`, las funciones `fmt.Print*` lo usan automaticamente cuando formatean con `%v`, `%s`, o `Println`:

```go
type Server struct {
    Hostname string
    IP       string
    Port     int
    Status   string
}

func (s Server) String() string {
    return fmt.Sprintf("%s (%s:%d) [%s]", s.Hostname, s.IP, s.Port, s.Status)
}

func main() {
    srv := Server{
        Hostname: "web-01",
        IP:       "10.0.1.10",
        Port:     443,
        Status:   "healthy",
    }

    fmt.Println(srv)             // web-01 (10.0.1.10:443) [healthy]
    fmt.Printf("Server: %s\n", srv)  // Server: web-01 (10.0.1.10:443) [healthy]
    fmt.Printf("Verb v: %v\n", srv)  // Verb v: web-01 (10.0.1.10:443) [healthy]

    // Sin String(): {web-01 10.0.1.10 443 healthy}  ← no muy legible
}
```

### fmt.GoStringer — representacion para debugging (%#v)

```go
type GoStringer interface {
    GoString() string
}

func (s Server) GoString() string {
    return fmt.Sprintf("Server{Hostname:%q, IP:%q, Port:%d, Status:%q}",
        s.Hostname, s.IP, s.Port, s.Status)
}

func main() {
    srv := Server{Hostname: "web-01", IP: "10.0.1.10", Port: 443, Status: "healthy"}
    fmt.Printf("%#v\n", srv)
    // Server{Hostname:"web-01", IP:"10.0.1.10", Port:443, Status:"healthy"}
}
```

### Patron SysAdmin: tipos con Stringer

```go
// IP address
type IPv4 [4]byte

func (ip IPv4) String() string {
    return fmt.Sprintf("%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3])
}

// CIDR network
type CIDR struct {
    Network IPv4
    Mask    int
}

func (c CIDR) String() string {
    return fmt.Sprintf("%s/%d", c.Network, c.Mask)
}

// Duration wrapper (human-readable)
type Uptime struct {
    Duration time.Duration
}

func (u Uptime) String() string {
    d := u.Duration
    days := int(d.Hours()) / 24
    hours := int(d.Hours()) % 24
    mins := int(d.Minutes()) % 60

    if days > 0 {
        return fmt.Sprintf("%dd %dh %dm", days, hours, mins)
    }
    if hours > 0 {
        return fmt.Sprintf("%dh %dm", hours, mins)
    }
    return fmt.Sprintf("%dm", mins)
}

// Bytes (human-readable sizes)
type ByteSize int64

func (b ByteSize) String() string {
    const (
        KB = 1024
        MB = KB * 1024
        GB = MB * 1024
        TB = GB * 1024
    )
    switch {
    case b >= TB:
        return fmt.Sprintf("%.1f TB", float64(b)/float64(TB))
    case b >= GB:
        return fmt.Sprintf("%.1f GB", float64(b)/float64(GB))
    case b >= MB:
        return fmt.Sprintf("%.1f MB", float64(b)/float64(MB))
    case b >= KB:
        return fmt.Sprintf("%.1f KB", float64(b)/float64(KB))
    default:
        return fmt.Sprintf("%d B", b)
    }
}

// Uso:
func main() {
    ip := IPv4{10, 0, 1, 42}
    net := CIDR{Network: IPv4{10, 0, 1, 0}, Mask: 24}
    uptime := Uptime{Duration: 72*time.Hour + 15*time.Minute}
    disk := ByteSize(4831838208)

    fmt.Println(ip)      // 10.0.1.42
    fmt.Println(net)     // 10.0.1.0/24
    fmt.Println(uptime)  // 3d 0h 15m
    fmt.Println(disk)    // 4.5 GB

    // Funcionan en format strings automaticamente:
    fmt.Printf("Host %s on %s, up %s, disk free %s\n", ip, net, uptime, disk)
    // Host 10.0.1.42 on 10.0.1.0/24, up 3d 0h 15m, disk free 4.5 GB
}
```

### Trampa: String() con pointer receiver

```go
type Counter struct {
    Name  string
    Value int
}

// Pointer receiver — solo *Counter tiene String()
func (c *Counter) String() string {
    return fmt.Sprintf("%s=%d", c.Name, c.Value)
}

func main() {
    c1 := &Counter{Name: "requests", Value: 42}
    fmt.Println(c1) // requests=42 ← funciona, *Counter tiene String()

    c2 := Counter{Name: "errors", Value: 5}
    fmt.Println(c2) // {errors 5} ← NO usa String()!
    // Counter (valor) no tiene String() en su method set
    // fmt.Println recibe una COPIA, no un puntero

    fmt.Println(&c2) // errors=5 ← con & funciona
}
```

---

## 6. error — La Interfaz mas Simple y Ubicua

### Definicion

```go
// package builtin — definida especialmente, sin necesidad de importar

type error interface {
    Error() string
}
```

`error` es la unica interfaz **predeclarada** en Go (no necesitas importar nada). Cualquier tipo con `Error() string` es un error.

### Crear errores simples

```go
import (
    "errors"
    "fmt"
)

// errors.New — error con mensaje fijo
var ErrNotFound = errors.New("not found")
var ErrTimeout = errors.New("operation timed out")

// fmt.Errorf — error con mensaje formateado
func OpenConfig(path string) error {
    return fmt.Errorf("failed to open config: %s", path)
}

// fmt.Errorf con %w — error wrapping (Go 1.13+)
func ReadConfig(path string) error {
    _, err := os.Open(path)
    if err != nil {
        return fmt.Errorf("read config %s: %w", path, err)
    }
    return nil
}
```

### Errores custom con campos

```go
// Cualquier struct con Error() string es un error
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

// Usar como error normal:
func Deploy(service, host string) error {
    // ... deploy logic ...
    return &DeployError{
        Service:  service,
        Host:     host,
        ExitCode: 137,
        Stderr:   "OOMKilled",
    }
}

// Extraer el error tipado con errors.As:
func HandleDeployment() {
    err := Deploy("web", "prod-01")
    if err != nil {
        var deployErr *DeployError
        if errors.As(err, &deployErr) {
            if deployErr.ExitCode == 137 {
                fmt.Println("OOM! Increase memory limit")
            }
            fmt.Printf("Failed service: %s on %s\n", deployErr.Service, deployErr.Host)
        }
    }
}
```

### Sentinel errors y errors.Is

```go
// Sentinel errors — errores predefinidos como variables de paquete
var (
    ErrNotFound    = errors.New("resource not found")
    ErrUnauthorized = errors.New("unauthorized")
    ErrConflict    = errors.New("resource conflict")
)

func GetServer(name string) (*Server, error) {
    // ...
    return nil, fmt.Errorf("server %s: %w", name, ErrNotFound)
}

// Verificar con errors.Is (recorre la cadena de wrapping):
func main() {
    _, err := GetServer("missing-host")
    if errors.Is(err, ErrNotFound) {
        fmt.Println("Server not found — provision it?")
    }
    // errors.Is funciona incluso con wrapping:
    // err = "server missing-host: resource not found"
    // errors.Is(err, ErrNotFound) = true (gracias a %w)
}
```

### Interfaces de error en la stdlib

```go
// net.Error — errores de red con mas info
type Error interface {
    error
    Timeout() bool
    Temporary() bool // deprecated en Go 1.18+
}

// os.PathError — errores de filesystem
type PathError struct {
    Op   string // "open", "read", "write"
    Path string
    Err  error
}
func (e *PathError) Error() string {
    return e.Op + " " + e.Path + ": " + e.Err.Error()
}
func (e *PathError) Unwrap() error { return e.Err }

// Uso:
func main() {
    _, err := os.Open("/nonexistent")
    if err != nil {
        var pathErr *os.PathError
        if errors.As(err, &pathErr) {
            fmt.Println("Op:", pathErr.Op)     // open
            fmt.Println("Path:", pathErr.Path) // /nonexistent
            fmt.Println("Err:", pathErr.Err)   // no such file or directory
        }
    }
}
```

---

## 7. sort.Interface — Ordenamiento Generico

### Definicion

```go
// package sort

type Interface interface {
    Len() int
    Less(i, j int) bool
    Swap(i, j int)
}
```

Tres metodos — `sort.Sort()` ordena cualquier coleccion que los implemente:

```go
type ServerList []Server

func (s ServerList) Len() int           { return len(s) }
func (s ServerList) Less(i, j int) bool { return s[i].CPU < s[j].CPU }
func (s ServerList) Swap(i, j int)      { s[i], s[j] = s[j], s[i] }

func main() {
    servers := ServerList{
        {Name: "web-01", CPU: 85.2},
        {Name: "web-02", CPU: 12.1},
        {Name: "web-03", CPU: 67.8},
        {Name: "web-04", CPU: 45.3},
    }

    sort.Sort(servers)

    for _, s := range servers {
        fmt.Printf("  %-10s CPU: %.1f%%\n", s.Name, s.CPU)
    }
    // web-02 CPU: 12.1%
    // web-04 CPU: 45.3%
    // web-03 CPU: 67.8%
    // web-01 CPU: 85.2%

    // Invertir: sort.Reverse wrapper
    sort.Sort(sort.Reverse(servers))
    // web-01 CPU: 85.2%  (ahora descendente)
}
```

### sort.Slice vs sort.Interface

En Go moderno, `sort.Slice` (Go 1.8+) y `slices.SortFunc` (Go 1.21+) son mas simples para casos ad-hoc:

```go
// sort.Slice — no necesita tipo custom
servers := []Server{
    {Name: "web-01", CPU: 85.2},
    {Name: "web-02", CPU: 12.1},
}

sort.Slice(servers, func(i, j int) bool {
    return servers[i].CPU < servers[j].CPU
})

// slices.SortFunc (Go 1.21+) — aun mas limpio
slices.SortFunc(servers, func(a, b Server) int {
    return cmp.Compare(a.CPU, b.CPU)
})
```

```
┌──────────────────────────────────────────────────────────────────────┐
│  sort.Interface vs sort.Slice vs slices.SortFunc                     │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  sort.Interface:                                                     │
│  ├─ 3 metodos (Len, Less, Swap)                                    │
│  ├─ Necesita tipo custom                                            │
│  ├─ Reutilizable (el tipo encapsula la logica)                      │
│  ├─ sort.Reverse funciona                                           │
│  └─ sort.IsSorted verifica                                          │
│                                                                      │
│  sort.Slice:                                                         │
│  ├─ 1 closure (Less)                                                │
│  ├─ Ad-hoc, no necesita tipo custom                                 │
│  ├─ La closure captura el slice — cuidado con concurrencia          │
│  └─ sort.SliceStable para sort estable                              │
│                                                                      │
│  slices.SortFunc (Go 1.21+):                                        │
│  ├─ 1 funcion comparadora (retorna int: -1, 0, 1)                  │
│  ├─ Generico — type-safe                                            │
│  ├─ No captura slice (recibe elementos directamente)                │
│  └─ Preferido en codigo nuevo                                       │
│                                                                      │
│  Usar sort.Interface cuando:                                         │
│  ├─ El tipo se ordena frecuentemente                                │
│  ├─ La logica de ordenamiento es compleja o tiene variantes         │
│  └─ Necesitas sort.Reverse o sort.IsSorted                          │
│                                                                      │
│  Usar slices.SortFunc para todo lo demas (Go 1.21+)                 │
└──────────────────────────────────────────────────────────────────────┘
```

### Patron SysAdmin: ordenamiento multi-criterio

```go
type Process struct {
    PID    int
    Name   string
    CPU    float64
    Memory int64 // bytes
}

// sort.Interface con ordenamiento por multiples campos
type ByResourceUsage []Process

func (p ByResourceUsage) Len() int      { return len(p) }
func (p ByResourceUsage) Swap(i, j int) { p[i], p[j] = p[j], p[i] }
func (p ByResourceUsage) Less(i, j int) bool {
    // Primero por CPU descendente
    if p[i].CPU != p[j].CPU {
        return p[i].CPU > p[j].CPU
    }
    // Si CPU igual, por memoria descendente
    if p[i].Memory != p[j].Memory {
        return p[i].Memory > p[j].Memory
    }
    // Si ambos iguales, por nombre ascendente
    return p[i].Name < p[j].Name
}

// Equivalente con slices.SortFunc:
// slices.SortFunc(procs, func(a, b Process) int {
//     if c := cmp.Compare(b.CPU, a.CPU); c != 0 { return c }
//     if c := cmp.Compare(b.Memory, a.Memory); c != 0 { return c }
//     return cmp.Compare(a.Name, b.Name)
// })
```

---

## 8. io.ReaderFrom y io.WriterTo — Optimizacion Transparente

Estas interfaces opcionales permiten que `io.Copy` optimice transferencias:

```go
// Si el Writer implementa ReaderFrom, io.Copy llama a ReadFrom
type ReaderFrom interface {
    ReadFrom(r Reader) (n int64, err error)
}

// Si el Reader implementa WriterTo, io.Copy llama a WriteTo
type WriterTo interface {
    WriteTo(w Writer) (n int64, err error)
}
```

```go
// *os.File implementa ReadFrom — puede usar sendfile() del kernel
// para copiar datos sin pasar por userspace:

// io.Copy(dstFile, srcFile)
// Internamente:
// 1. ¿dst implementa ReaderFrom? SI (*os.File tiene ReadFrom)
// 2. Llama dstFile.ReadFrom(srcFile)
// 3. ReadFrom usa syscall sendfile() — zero-copy kernel transfer

// *bytes.Buffer implementa ReadFrom — lee todo de una vez
// buf.ReadFrom(r) es equivalente a io.ReadAll pero reutilizando el buffer

// *bytes.Buffer implementa WriterTo — escribe todo de una vez
// buf.WriteTo(w) escribe todo el contenido del buffer
```

### Implementar WriterTo para optimizar

```go
// Tipo custom que puede escribirse eficientemente
type LogBatch struct {
    entries []string
}

// WriterTo — io.Copy usara esto automaticamente
func (lb *LogBatch) WriteTo(w io.Writer) (int64, error) {
    var total int64
    for _, entry := range lb.entries {
        n, err := io.WriteString(w, entry+"\n")
        total += int64(n)
        if err != nil {
            return total, err
        }
    }
    return total, nil
}

// Tambien satisface io.Reader para compatibilidad:
func (lb *LogBatch) Read(p []byte) (int, error) {
    // implementacion basica...
    return 0, io.EOF
}

// io.Copy detecta WriterTo y lo usa:
// io.Copy(file, logBatch) → llama logBatch.WriteTo(file)
```

---

## 9. encoding.TextMarshaler y encoding.TextUnmarshaler

```go
// package encoding

type TextMarshaler interface {
    MarshalText() (text []byte, err error)
}

type TextUnmarshaler interface {
    UnmarshalText(text []byte) error
}
```

Estas interfaces controlan como un tipo se serializa/deserializa como texto en JSON (como key de map), XML, CSV, etc.:

```go
type Severity int

const (
    SeverityLow Severity = iota
    SeverityMedium
    SeverityHigh
    SeverityCritical
)

// TextMarshaler — controla como aparece en JSON, YAML, etc.
func (s Severity) MarshalText() ([]byte, error) {
    switch s {
    case SeverityLow:
        return []byte("low"), nil
    case SeverityMedium:
        return []byte("medium"), nil
    case SeverityHigh:
        return []byte("high"), nil
    case SeverityCritical:
        return []byte("critical"), nil
    default:
        return nil, fmt.Errorf("unknown severity: %d", s)
    }
}

// TextUnmarshaler — parsea de texto
func (s *Severity) UnmarshalText(text []byte) error {
    switch string(text) {
    case "low":
        *s = SeverityLow
    case "medium":
        *s = SeverityMedium
    case "high":
        *s = SeverityHigh
    case "critical":
        *s = SeverityCritical
    default:
        return fmt.Errorf("unknown severity: %q", text)
    }
    return nil
}

type Alert struct {
    Name     string   `json:"name"`
    Severity Severity `json:"severity"` // serializa como "high", no como 2
}

func main() {
    a := Alert{Name: "cpu_high", Severity: SeverityHigh}
    data, _ := json.Marshal(a)
    fmt.Println(string(data))
    // {"name":"cpu_high","severity":"high"}

    var a2 Alert
    json.Unmarshal([]byte(`{"name":"disk_full","severity":"critical"}`), &a2)
    fmt.Println(a2.Severity) // 3 (SeverityCritical)
}
```

---

## 10. http.Handler — La Interfaz del Servidor Web

```go
// package net/http

type Handler interface {
    ServeHTTP(ResponseWriter, *Request)
}

// HandlerFunc — adapter que convierte una funcion en Handler
type HandlerFunc func(ResponseWriter, *Request)

func (f HandlerFunc) ServeHTTP(w ResponseWriter, r *Request) {
    f(w, r)
}
```

```go
// Implementar Handler directamente (para handlers con estado):

type HealthHandler struct {
    checker HealthChecker
    mu      sync.RWMutex
    status  string
}

func (h *HealthHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
    h.mu.RLock()
    status := h.status
    h.mu.RUnlock()

    if status == "healthy" {
        w.WriteHeader(http.StatusOK)
    } else {
        w.WriteHeader(http.StatusServiceUnavailable)
    }
    fmt.Fprintf(w, `{"status":%q}`, status)
}

// Middleware como wrapper de Handler:
type LoggingMiddleware struct {
    next   http.Handler
    logger *slog.Logger
}

func (lm *LoggingMiddleware) ServeHTTP(w http.ResponseWriter, r *http.Request) {
    start := time.Now()
    lm.next.ServeHTTP(w, r) // delegar al handler envuelto
    lm.logger.Info("request",
        "method", r.Method,
        "path", r.URL.Path,
        "duration", time.Since(start),
    )
}

func WithLogging(next http.Handler, logger *slog.Logger) http.Handler {
    return &LoggingMiddleware{next: next, logger: logger}
}
```

---

## 11. context.Context — Cancelacion y Deadlines

```go
// package context

type Context interface {
    Deadline() (deadline time.Time, ok bool)
    Done() <-chan struct{}
    Err() error
    Value(key any) any
}
```

Context no es una interfaz que normalmente implementes — usas las funciones del paquete:

```go
// Cancelacion manual
ctx, cancel := context.WithCancel(context.Background())
defer cancel()

// Timeout
ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
defer cancel()

// Deadline absoluto
ctx, cancel := context.WithDeadline(context.Background(), time.Now().Add(5*time.Minute))
defer cancel()

// Valor (metadata)
ctx = context.WithValue(ctx, "request-id", "abc123")
```

### Patron SysAdmin: operacion con timeout

```go
func PingHost(ctx context.Context, host string) error {
    // Respetar cancelacion/timeout del contexto
    select {
    case <-ctx.Done():
        return fmt.Errorf("ping %s cancelled: %w", host, ctx.Err())
    default:
    }

    cmd := exec.CommandContext(ctx, "ping", "-c", "1", "-W", "2", host)
    output, err := cmd.CombinedOutput()
    if err != nil {
        return fmt.Errorf("ping %s failed: %s: %w", host, string(output), err)
    }
    return nil
}

func PingFleet(hosts []string, timeout time.Duration) map[string]error {
    ctx, cancel := context.WithTimeout(context.Background(), timeout)
    defer cancel()

    results := make(map[string]error)
    for _, host := range hosts {
        results[host] = PingHost(ctx, host)
    }
    return results
}
```

---

## 12. Tabla Resumen de Interfaces Comunes

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  INTERFACES COMUNES — Referencia rapida                                     │
├──────────────────┬──────────────────────────────────┬───────────────────────┤
│ Interface        │ Metodo(s)                        │ Cuando implementar    │
├──────────────────┼──────────────────────────────────┼───────────────────────┤
│ io.Reader        │ Read([]byte)(int,error)           │ Tu tipo produce bytes │
│ io.Writer        │ Write([]byte)(int,error)          │ Tu tipo consume bytes │
│ io.Closer        │ Close() error                    │ Tu tipo tiene recurso │
│                  │                                  │ que liberar           │
│ io.ReadWriter    │ Read + Write                     │ Leer y escribir       │
│ io.ReadCloser    │ Read + Close                     │ Leer + cleanup        │
│ io.Seeker        │ Seek(int64,int)(int64,error)     │ Acceso posicional     │
│ io.ReaderFrom    │ ReadFrom(Reader)(int64,error)    │ Optimizar io.Copy dst │
│ io.WriterTo      │ WriteTo(Writer)(int64,error)     │ Optimizar io.Copy src │
├──────────────────┼──────────────────────────────────┼───────────────────────┤
│ fmt.Stringer     │ String() string                  │ Representacion humana │
│ fmt.GoStringer   │ GoString() string                │ Repr para debug %#v   │
├──────────────────┼──────────────────────────────────┼───────────────────────┤
│ error            │ Error() string                   │ Tu tipo es un error   │
│                  │ + Unwrap() error (opcional)       │ Cadena de errores     │
├──────────────────┼──────────────────────────────────┼───────────────────────┤
│ sort.Interface   │ Len() int                        │ Coleccion ordenable   │
│                  │ Less(i,j int) bool               │ (preferir slices.     │
│                  │ Swap(i,j int)                    │  SortFunc en 1.21+)   │
├──────────────────┼──────────────────────────────────┼───────────────────────┤
│ encoding.        │ MarshalText()([]byte,error)      │ Serializacion texto   │
│  TextMarshaler   │                                  │ (JSON keys, CSV, etc) │
│ json.Marshaler   │ MarshalJSON()([]byte,error)      │ JSON custom           │
├──────────────────┼──────────────────────────────────┼───────────────────────┤
│ http.Handler     │ ServeHTTP(w,*Request)            │ HTTP endpoint         │
│ context.Context  │ Deadline,Done,Err,Value          │ Raro implementar      │
│                  │                                  │ (usar context.With*)  │
└──────────────────┴──────────────────────────────────┴───────────────────────┘
```

---

## 13. Comparacion con C y Rust

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  INTERFACES COMUNES — Go vs C vs Rust                                        │
├──────────────┬──────────────────────┬───────────────────┬────────────────────┤
│ Concepto     │ Go                   │ C                 │ Rust               │
├──────────────┼──────────────────────┼───────────────────┼────────────────────┤
│ Leer bytes   │ io.Reader            │ read(fd,buf,n)    │ std::io::Read      │
│              │ .Read([]byte)        │ o fread()         │ .read(&mut [u8])   │
├──────────────┼──────────────────────┼───────────────────┼────────────────────┤
│ Escribir     │ io.Writer            │ write(fd,buf,n)   │ std::io::Write     │
│ bytes        │ .Write([]byte)       │ o fwrite()        │ .write(&[u8])      │
├──────────────┼──────────────────────┼───────────────────┼────────────────────┤
│ Cerrar       │ io.Closer            │ close(fd) o       │ Drop trait (auto)  │
│ recurso      │ .Close()             │ fclose()          │ o manual close()   │
├──────────────┼──────────────────────┼───────────────────┼────────────────────┤
│ Repr texto   │ fmt.Stringer         │ (no existe)       │ std::fmt::Display  │
│              │ .String()            │ printf manual     │ impl Display       │
├──────────────┼──────────────────────┼───────────────────┼────────────────────┤
│ Debug repr   │ fmt.GoStringer       │ (no existe)       │ std::fmt::Debug    │
│              │ .GoString()          │                   │ #[derive(Debug)]   │
├──────────────┼──────────────────────┼───────────────────┼────────────────────┤
│ Errores      │ error                │ int errno         │ std::error::Error  │
│              │ .Error() string      │ (codigo numerico) │ impl Error + Display│
├──────────────┼──────────────────────┼───────────────────┼────────────────────┤
│ Ordenar      │ sort.Interface       │ qsort(void*,      │ Ord trait          │
│              │ Len+Less+Swap        │  comparator fn)   │ .cmp(&self,&other) │
├──────────────┼──────────────────────┼───────────────────┼────────────────────┤
│ Composicion  │ Embedding de         │ N/A               │ Supertraits        │
│              │ interfaces           │                   │ trait A: B + C {}  │
├──────────────┼──────────────────────┼───────────────────┼────────────────────┤
│ Satisfaccion │ Implicita            │ Manual (casts)    │ Explicita (impl)   │
│              │ (duck typing)        │                   │ + derive macros    │
├──────────────┼──────────────────────┼───────────────────┼────────────────────┤
│ HTTP handler │ http.Handler         │ (no estandar)     │ (no estandar,      │
│              │ .ServeHTTP(w,r)      │ libmicrohttpd etc │  axum/actix etc)   │
└──────────────┴──────────────────────┴───────────────────┴────────────────────┘
```

### Ejemplo: io.Reader en los tres lenguajes

**C:**
```c
// No hay interfaz — solo la syscall
#include <unistd.h>

char buf[4096];
ssize_t n = read(fd, buf, sizeof(buf));
if (n < 0) {
    perror("read");
}
// Para abstraer: typedef ssize_t (*read_fn)(void* ctx, char* buf, size_t n);
// Cada "tipo" necesita su propia funcion + void* context
```

**Rust:**
```rust
use std::io::Read;

// Read es un trait con metodo read(&mut self, buf: &mut [u8]) -> Result<usize>
fn process(reader: &mut dyn Read) -> std::io::Result<Vec<u8>> {
    let mut buf = Vec::new();
    reader.read_to_end(&mut buf)?;  // metodo provided del trait
    Ok(buf)
}

// File, TcpStream, Cursor<Vec<u8>> — todos implementan Read
// Pero deben declarar: impl Read for MyType { ... }
```

**Go:**
```go
import "io"

func Process(r io.Reader) ([]byte, error) {
    return io.ReadAll(r)
}

// os.File, net.Conn, bytes.Buffer, strings.Reader, gzip.Reader...
// Todos satisfacen io.Reader automaticamente — sin declarar nada
```

---

## 14. Ejercicios de Prediccion

### Ejercicio 1: ¿Compila?

```go
type MyBuffer struct {
    data []byte
}

func (b *MyBuffer) Write(p []byte) (int, error) {
    b.data = append(b.data, p...)
    return len(p), nil
}

func main() {
    var w io.Writer = MyBuffer{}
    w.Write([]byte("hello"))
}
```

<details>
<summary>Respuesta</summary>

**No compila**: `MyBuffer does not implement io.Writer (method Write has pointer receiver)`. El metodo Write tiene pointer receiver (`*MyBuffer`), pero estamos asignando un valor `MyBuffer{}`. Solo `*MyBuffer` satisface `io.Writer`.

Fix: `var w io.Writer = &MyBuffer{}`

</details>

### Ejercicio 2: ¿Que imprime?

```go
type Host struct {
    Name string
    IP   string
}

func (h Host) String() string {
    return fmt.Sprintf("%s(%s)", h.Name, h.IP)
}

func main() {
    h := Host{Name: "web-01", IP: "10.0.1.1"}
    fmt.Println(h)
    fmt.Printf("%v\n", h)
    fmt.Printf("%+v\n", h)
    fmt.Printf("%#v\n", h)
}
```

<details>
<summary>Respuesta</summary>

```
web-01(10.0.1.1)
web-01(10.0.1.1)
web-01(10.0.1.1)
main.Host{Name:"web-01", IP:"10.0.1.1"}
```

- `Println` y `%v` usan `String()` si existe → `web-01(10.0.1.1)`
- `%+v` **tambien** usa `String()` si existe (no muestra campos como con structs sin Stringer)
- `%#v` usa `GoString()` si existe; como no existe, muestra la representacion Go default del struct

**Nota**: `%+v` con Stringer definido NO muestra `{Name:web-01 IP:10.0.1.1}` — siempre prefiere String(). Si quieres ver los campos individuales con Stringer definido, debes usar `%#v` o acceder a los campos manualmente.

</details>

### Ejercicio 3: ¿Cuantos bytes se copian?

```go
func main() {
    r := strings.NewReader("Hello, World!")
    limited := io.LimitReader(r, 5)
    n, _ := io.Copy(os.Stdout, limited)
    fmt.Fprintf(os.Stderr, "\ncopied: %d\n", n)
}
```

<details>
<summary>Respuesta</summary>

Se copian **5 bytes**. La salida a stdout es `Hello` y la salida a stderr es `copied: 5`.

`io.LimitReader(r, 5)` crea un Reader que retorna `io.EOF` despues de leer 5 bytes, sin importar cuantos tenga el reader subyacente. "Hello, World!" tiene 13 bytes, pero solo se leen 5.

</details>

### Ejercicio 4: ¿Error o nil?

```go
func ReadConfig(path string) error {
    _, err := os.Open(path)
    if err != nil {
        return fmt.Errorf("config: %w", err)
    }
    return nil
}

func main() {
    err := ReadConfig("/nonexistent/config.yaml")
    fmt.Println(errors.Is(err, os.ErrNotExist))
    fmt.Println(errors.Is(err, fs.ErrNotExist))

    var pathErr *os.PathError
    fmt.Println(errors.As(err, &pathErr))
}
```

<details>
<summary>Respuesta</summary>

```
true
true
true
```

- `os.ErrNotExist` y `fs.ErrNotExist` son el **mismo** error (os.ErrNotExist = fs.ErrNotExist desde Go 1.16)
- `errors.Is` recorre la cadena de wrapping: `fmt.Errorf("config: %w", err)` envuelve un `*os.PathError` que a su vez envuelve `syscall.ENOENT` que es equivalente a `os.ErrNotExist`
- `errors.As` tambien recorre la cadena y encuentra el `*os.PathError` envuelto

</details>

### Ejercicio 5: ¿A donde va el output?

```go
func main() {
    var buf bytes.Buffer
    multi := io.MultiWriter(os.Stdout, &buf)

    fmt.Fprintln(multi, "hello")
    fmt.Fprintln(multi, "world")

    fmt.Fprintf(os.Stderr, "buffer has: %q\n", buf.String())
}
```

<details>
<summary>Respuesta</summary>

**stdout:**
```
hello
world
```

**stderr:**
```
buffer has: "hello\nworld\n"
```

`io.MultiWriter` duplica cada Write a AMBOS destinos. Los dos `Fprintln` escriben a stdout (visible en terminal) y al buffer (acumulado). Luego el contenido del buffer se imprime a stderr.

</details>

---

## 15. Programa Completo: Infrastructure Log Pipeline con Interfaces Comunes

```go
// log_pipeline.go
// Demuestra el uso real de io.Reader, io.Writer, fmt.Stringer,
// error, sort.Interface y otras interfaces comunes para construir
// un pipeline de procesamiento de logs de infraestructura.
//
// Lee logs de multiples fuentes (Reader), los parsea, filtra, ordena
// (sort.Interface), formatea (Stringer), y escribe a multiples
// destinos (Writer), con manejo de errores (error) robusto.
package main

import (
    "bufio"
    "bytes"
    "cmp"
    "encoding/json"
    "errors"
    "fmt"
    "io"
    "slices"
    "sort"
    "strings"
    "time"
)

// =====================================================================
// TIPOS BASE — usan fmt.Stringer y encoding interfaces
// =====================================================================

// Severity con Stringer y TextMarshaler
type Severity int

const (
    DEBUG Severity = iota
    INFO
    WARN
    ERROR
    FATAL
)

func (s Severity) String() string {
    names := [...]string{"DEBUG", "INFO", "WARN", "ERROR", "FATAL"}
    if int(s) < len(names) {
        return names[s]
    }
    return fmt.Sprintf("UNKNOWN(%d)", s)
}

func (s Severity) MarshalText() ([]byte, error) {
    return []byte(s.String()), nil
}

func (s *Severity) UnmarshalText(text []byte) error {
    switch strings.ToUpper(string(text)) {
    case "DEBUG":
        *s = DEBUG
    case "INFO":
        *s = INFO
    case "WARN", "WARNING":
        *s = WARN
    case "ERROR", "ERR":
        *s = ERROR
    case "FATAL", "CRIT", "CRITICAL":
        *s = FATAL
    default:
        return fmt.Errorf("unknown severity: %q", text)
    }
    return nil
}

// LogEntry — una linea de log parseada
type LogEntry struct {
    Timestamp time.Time
    Severity  Severity
    Host      string
    Service   string
    Message   string
    Fields    map[string]string
}

// fmt.Stringer — representacion legible
func (e LogEntry) String() string {
    ts := e.Timestamp.Format("2006-01-02 15:04:05")
    fields := ""
    if len(e.Fields) > 0 {
        var parts []string
        for k, v := range e.Fields {
            parts = append(parts, fmt.Sprintf("%s=%s", k, v))
        }
        sort.Strings(parts)
        fields = " " + strings.Join(parts, " ")
    }
    return fmt.Sprintf("[%s] %-5s %-12s %-10s %s%s",
        ts, e.Severity, e.Host, e.Service, e.Message, fields)
}

// =====================================================================
// CUSTOM ERRORS — satisfacen error interface
// =====================================================================

type ParseError struct {
    Line   int
    Input  string
    Reason string
}

func (e *ParseError) Error() string {
    return fmt.Sprintf("parse error at line %d: %s (input: %q)", e.Line, e.Reason, e.Input)
}

type FilterError struct {
    Filter string
    Err    error
}

func (e *FilterError) Error() string {
    return fmt.Sprintf("filter %q error: %v", e.Filter, e.Err)
}

func (e *FilterError) Unwrap() error {
    return e.Err
}

// Sentinel errors
var (
    ErrEmptyLine    = errors.New("empty line")
    ErrInvalidFormat = errors.New("invalid log format")
)

// =====================================================================
// LOG SOURCE — implementa io.Reader
// =====================================================================

// SimulatedLogSource genera logs como un io.Reader
type SimulatedLogSource struct {
    host    string
    entries []string
    reader  *strings.Reader
}

func NewSimulatedLogSource(host string, entries []string) *SimulatedLogSource {
    combined := strings.Join(entries, "\n") + "\n"
    return &SimulatedLogSource{
        host:    host,
        entries: entries,
        reader:  strings.NewReader(combined),
    }
}

// Read satisface io.Reader
func (s *SimulatedLogSource) Read(p []byte) (int, error) {
    return s.reader.Read(p)
}

// String satisface fmt.Stringer
func (s *SimulatedLogSource) String() string {
    return fmt.Sprintf("LogSource{host=%s, entries=%d}", s.host, len(s.entries))
}

// =====================================================================
// LOG PARSER — lee de io.Reader, produce LogEntries
// =====================================================================

type LogParser struct {
    defaultHost string
}

func (lp *LogParser) Parse(r io.Reader) ([]LogEntry, []error) {
    var entries []LogEntry
    var errs []error

    scanner := bufio.NewScanner(r) // bufio.Scanner usa io.Reader
    lineNum := 0

    for scanner.Scan() {
        lineNum++
        line := strings.TrimSpace(scanner.Text())

        if line == "" {
            continue
        }

        entry, err := lp.parseLine(line, lineNum)
        if err != nil {
            errs = append(errs, err)
            continue
        }
        entries = append(entries, entry)
    }

    if err := scanner.Err(); err != nil {
        errs = append(errs, fmt.Errorf("scanner error: %w", err))
    }

    return entries, errs
}

func (lp *LogParser) parseLine(line string, lineNum int) (LogEntry, error) {
    // Formato esperado: TIMESTAMP|SEVERITY|HOST|SERVICE|MESSAGE[|KEY=VAL,...]
    parts := strings.SplitN(line, "|", 6)
    if len(parts) < 5 {
        return LogEntry{}, &ParseError{
            Line:   lineNum,
            Input:  line,
            Reason: fmt.Sprintf("expected 5+ fields, got %d", len(parts)),
        }
    }

    ts, err := time.Parse("2006-01-02T15:04:05", parts[0])
    if err != nil {
        return LogEntry{}, &ParseError{
            Line:   lineNum,
            Input:  parts[0],
            Reason: "invalid timestamp",
        }
    }

    var sev Severity
    if err := sev.UnmarshalText([]byte(parts[1])); err != nil {
        return LogEntry{}, &ParseError{
            Line:   lineNum,
            Input:  parts[1],
            Reason: "invalid severity",
        }
    }

    host := parts[2]
    if host == "" {
        host = lp.defaultHost
    }

    entry := LogEntry{
        Timestamp: ts,
        Severity:  sev,
        Host:      host,
        Service:   parts[3],
        Message:   parts[4],
        Fields:    make(map[string]string),
    }

    // Parse optional fields
    if len(parts) > 5 && parts[5] != "" {
        for _, kv := range strings.Split(parts[5], ",") {
            if k, v, ok := strings.Cut(kv, "="); ok {
                entry.Fields[k] = v
            }
        }
    }

    return entry, nil
}

// =====================================================================
// LOG FILTER — filtra entries
// =====================================================================

type LogFilter func(LogEntry) bool

func FilterBySeverity(minSev Severity) LogFilter {
    return func(e LogEntry) bool {
        return e.Severity >= minSev
    }
}

func FilterByHost(host string) LogFilter {
    return func(e LogEntry) bool {
        return e.Host == host
    }
}

func FilterByService(service string) LogFilter {
    return func(e LogEntry) bool {
        return e.Service == service
    }
}

func FilterByMessage(substr string) LogFilter {
    return func(e LogEntry) bool {
        return strings.Contains(e.Message, substr)
    }
}

func ApplyFilters(entries []LogEntry, filters ...LogFilter) []LogEntry {
    result := make([]LogEntry, 0, len(entries))
    for _, e := range entries {
        pass := true
        for _, f := range filters {
            if !f(e) {
                pass = false
                break
            }
        }
        if pass {
            result = append(result, e)
        }
    }
    return result
}

// =====================================================================
// LOG SORTER — usa sort.Interface
// =====================================================================

type ByTimestamp []LogEntry

func (b ByTimestamp) Len() int           { return len(b) }
func (b ByTimestamp) Less(i, j int) bool { return b[i].Timestamp.Before(b[j].Timestamp) }
func (b ByTimestamp) Swap(i, j int)      { b[i], b[j] = b[j], b[i] }

type BySeverityDesc []LogEntry

func (b BySeverityDesc) Len() int           { return len(b) }
func (b BySeverityDesc) Less(i, j int) bool { return b[i].Severity > b[j].Severity }
func (b BySeverityDesc) Swap(i, j int)      { b[i], b[j] = b[j], b[i] }

// =====================================================================
// LOG FORMATTERS — escriben a io.Writer
// =====================================================================

// TextFormatter escribe logs como texto legible
type TextFormatter struct{}

func (f TextFormatter) Format(w io.Writer, entries []LogEntry) (int64, error) {
    var total int64
    for _, e := range entries {
        n, err := fmt.Fprintln(w, e.String()) // usa Stringer
        total += int64(n)
        if err != nil {
            return total, err
        }
    }
    return total, nil
}

// JSONFormatter escribe logs como JSON lines
type JSONFormatter struct {
    Pretty bool
}

func (f JSONFormatter) Format(w io.Writer, entries []LogEntry) (int64, error) {
    var total int64
    enc := json.NewEncoder(w)
    if f.Pretty {
        enc.SetIndent("", "  ")
    }
    for _, e := range entries {
        // Struct anonimo para JSON con campos custom
        jsonEntry := struct {
            Timestamp string            `json:"timestamp"`
            Severity  Severity          `json:"severity"`
            Host      string            `json:"host"`
            Service   string            `json:"service"`
            Message   string            `json:"message"`
            Fields    map[string]string `json:"fields,omitempty"`
        }{
            Timestamp: e.Timestamp.Format(time.RFC3339),
            Severity:  e.Severity,
            Host:      e.Host,
            Service:   e.Service,
            Message:   e.Message,
            Fields:    e.Fields,
        }
        if err := enc.Encode(jsonEntry); err != nil {
            return total, err
        }
    }
    return total, nil
}

// CSVFormatter escribe logs como CSV
type CSVFormatter struct{}

func (f CSVFormatter) Format(w io.Writer, entries []LogEntry) (int64, error) {
    var total int64
    // Header
    n, err := fmt.Fprintln(w, "timestamp,severity,host,service,message")
    total += int64(n)
    if err != nil {
        return total, err
    }
    for _, e := range entries {
        n, err := fmt.Fprintf(w, "%s,%s,%s,%s,%q\n",
            e.Timestamp.Format(time.RFC3339),
            e.Severity,
            e.Host,
            e.Service,
            e.Message,
        )
        total += int64(n)
        if err != nil {
            return total, err
        }
    }
    return total, nil
}

// =====================================================================
// LOG STATS — estadisticas
// =====================================================================

type LogStats struct {
    Total       int
    BySeverity  map[Severity]int
    ByHost      map[string]int
    ByService   map[string]int
    TimeRange   [2]time.Time // earliest, latest
    ParseErrors int
}

func (s LogStats) String() string {
    var b strings.Builder
    fmt.Fprintf(&b, "Total entries: %d (parse errors: %d)\n", s.Total, s.ParseErrors)

    fmt.Fprintf(&b, "By severity:\n")
    for sev := FATAL; sev >= DEBUG; sev-- {
        if count, ok := s.BySeverity[sev]; ok && count > 0 {
            bar := strings.Repeat("█", count)
            fmt.Fprintf(&b, "  %-7s %3d %s\n", sev, count, bar)
        }
    }

    fmt.Fprintf(&b, "By host:\n")
    hosts := make([]string, 0, len(s.ByHost))
    for h := range s.ByHost {
        hosts = append(hosts, h)
    }
    sort.Strings(hosts)
    for _, h := range hosts {
        fmt.Fprintf(&b, "  %-14s %d\n", h, s.ByHost[h])
    }

    fmt.Fprintf(&b, "By service:\n")
    svcs := make([]string, 0, len(s.ByService))
    for s := range s.ByService {
        svcs = append(svcs, s)
    }
    sort.Strings(svcs)
    for _, svc := range svcs {
        fmt.Fprintf(&b, "  %-14s %d\n", svc, s.ByService[svc])
    }

    if !s.TimeRange[0].IsZero() {
        duration := s.TimeRange[1].Sub(s.TimeRange[0])
        fmt.Fprintf(&b, "Time range: %s to %s (%s)\n",
            s.TimeRange[0].Format("15:04:05"),
            s.TimeRange[1].Format("15:04:05"),
            duration)
    }

    return b.String()
}

func ComputeStats(entries []LogEntry, parseErrors int) LogStats {
    stats := LogStats{
        Total:       len(entries),
        BySeverity:  make(map[Severity]int),
        ByHost:      make(map[string]int),
        ByService:   make(map[string]int),
        ParseErrors: parseErrors,
    }

    for _, e := range entries {
        stats.BySeverity[e.Severity]++
        stats.ByHost[e.Host]++
        stats.ByService[e.Service]++

        if stats.TimeRange[0].IsZero() || e.Timestamp.Before(stats.TimeRange[0]) {
            stats.TimeRange[0] = e.Timestamp
        }
        if e.Timestamp.After(stats.TimeRange[1]) {
            stats.TimeRange[1] = e.Timestamp
        }
    }

    return stats
}

// =====================================================================
// MAIN — pipeline completo
// =====================================================================

func main() {
    fmt.Println("╔══════════════════════════════════════════════════════╗")
    fmt.Println("║   Infrastructure Log Pipeline — Common Interfaces   ║")
    fmt.Println("╚══════════════════════════════════════════════════════╝")

    // --- 1. Create log sources (io.Reader) ---

    fmt.Println("\n── 1. Log Sources (io.Reader) ──")

    webLogs := NewSimulatedLogSource("web-01", []string{
        "2026-04-03T10:00:01|INFO|web-01|nginx|request handled|path=/api/health,status=200",
        "2026-04-03T10:00:02|INFO|web-01|nginx|request handled|path=/api/users,status=200",
        "2026-04-03T10:00:03|WARN|web-01|nginx|slow request|path=/api/report,duration_ms=3200",
        "2026-04-03T10:00:05|ERROR|web-01|nginx|upstream timeout|path=/api/export,upstream=backend-01",
        "2026-04-03T10:00:06|INFO|web-01|nginx|request handled|path=/api/health,status=200",
        "2026-04-03T10:00:08|ERROR|web-01|app|panic recovered|handler=exportHandler",
        "2026-04-03T10:00:10|INFO|web-01|app|reconnected to database|attempt=3",
        "MALFORMED LINE WITHOUT PIPES",
    })

    dbLogs := NewSimulatedLogSource("db-01", []string{
        "2026-04-03T10:00:00|INFO|db-01|postgres|checkpoint starting|type=time",
        "2026-04-03T10:00:02|INFO|db-01|postgres|checkpoint complete|duration_ms=850",
        "2026-04-03T10:00:04|WARN|db-01|postgres|connection pool near limit|active=48,max=50",
        "2026-04-03T10:00:07|ERROR|db-01|postgres|deadlock detected|table=inventory,pid=12345",
        "2026-04-03T10:00:09|FATAL|db-01|postgres|out of shared memory|requested=256MB",
        "2026-04-03T10:00:11|INFO|db-01|pgbouncer|stats|active=25,waiting=3",
    })

    workerLogs := NewSimulatedLogSource("worker-01", []string{
        "2026-04-03T10:00:01|INFO|worker-01|cron|job started|job=backup-daily",
        "2026-04-03T10:00:03|INFO|worker-01|cron|job progress|job=backup-daily,pct=45",
        "2026-04-03T10:00:05|WARN|worker-01|cron|job slow|job=backup-daily,elapsed=120s",
        "2026-04-03T10:00:08|ERROR|worker-01|cron|job failed|job=backup-daily,exit=1",
        "2026-04-03T10:00:09|INFO|worker-01|cron|job started|job=cleanup-tmp",
        "2026-04-03T10:00:10|INFO|worker-01|cron|job completed|job=cleanup-tmp,duration=2s",
    })

    // Stringer para los sources
    fmt.Printf("  Source 1: %s\n", webLogs)
    fmt.Printf("  Source 2: %s\n", dbLogs)
    fmt.Printf("  Source 3: %s\n", workerLogs)

    // --- 2. Combine sources using io.MultiReader ---

    fmt.Println("\n── 2. Combining Sources (io.MultiReader) ──")

    // io.MultiReader concatena multiples Readers en secuencia
    combined := io.MultiReader(webLogs, dbLogs, workerLogs)

    // TeeReader para contar bytes leidos
    var rawBuf bytes.Buffer
    tee := io.TeeReader(combined, &rawBuf)

    fmt.Println("  Combined 3 sources into single io.Reader")

    // --- 3. Parse logs ---

    fmt.Println("\n── 3. Parsing Logs (bufio.Scanner over io.Reader) ──")

    parser := &LogParser{defaultHost: "unknown"}
    entries, parseErrors := parser.Parse(tee)

    fmt.Printf("  Parsed %d entries, %d errors\n", len(entries), len(parseErrors))
    fmt.Printf("  Raw bytes read: %d\n", rawBuf.Len())

    // Show parse errors (error interface)
    for _, err := range parseErrors {
        var pe *ParseError
        if errors.As(err, &pe) {
            fmt.Printf("  Parse error (line %d): %s\n", pe.Line, pe.Reason)
        } else {
            fmt.Printf("  Error: %v\n", err)
        }
    }

    // --- 4. Sort logs (sort.Interface) ---

    fmt.Println("\n── 4. Sorting Logs (sort.Interface) ──")

    // sort.Interface: ByTimestamp
    sort.Sort(ByTimestamp(entries))
    fmt.Println("  Sorted by timestamp (sort.Interface)")

    fmt.Println("\n  First 5 entries (chronological):")
    for i, e := range entries[:min(5, len(entries))] {
        fmt.Printf("  %d. %s\n", i+1, e) // usa Stringer
    }

    // --- 5. Filter logs ---

    fmt.Println("\n── 5. Filtering Logs ──")

    // Filtro 1: solo warnings y errores
    warningsAndErrors := ApplyFilters(entries, FilterBySeverity(WARN))
    fmt.Printf("\n  Severity >= WARN: %d entries\n", len(warningsAndErrors))
    for _, e := range warningsAndErrors {
        fmt.Printf("    %s\n", e)
    }

    // Filtro 2: errores de db-01
    dbErrors := ApplyFilters(entries,
        FilterByHost("db-01"),
        FilterBySeverity(ERROR),
    )
    fmt.Printf("\n  db-01 errors: %d entries\n", len(dbErrors))
    for _, e := range dbErrors {
        fmt.Printf("    %s\n", e)
    }

    // Filtro 3: busqueda por mensaje
    timeoutLogs := ApplyFilters(entries, FilterByMessage("timeout"))
    fmt.Printf("\n  Contains 'timeout': %d entries\n", len(timeoutLogs))
    for _, e := range timeoutLogs {
        fmt.Printf("    %s\n", e)
    }

    // --- 6. Sort by severity (slices.SortFunc — modern Go) ---

    fmt.Println("\n── 6. Sort by Severity (slices.SortFunc) ──")

    critical := ApplyFilters(entries, FilterBySeverity(WARN))
    slices.SortFunc(critical, func(a, b LogEntry) int {
        if c := cmp.Compare(b.Severity, a.Severity); c != 0 {
            return c // severity descending
        }
        return a.Timestamp.Compare(b.Timestamp) // then time ascending
    })

    fmt.Println("  Critical entries (severity desc, then time asc):")
    for _, e := range critical {
        fmt.Printf("    %s\n", e)
    }

    // --- 7. Format and write to different Writers ---

    fmt.Println("\n── 7. Output Formatting (io.Writer) ──")

    // Text format to buffer
    var textBuf bytes.Buffer
    textFmt := TextFormatter{}
    textBytes, _ := textFmt.Format(&textBuf, warningsAndErrors)
    fmt.Printf("\n  Text format (%d bytes):\n", textBytes)
    // Show first 3 lines
    lines := strings.Split(textBuf.String(), "\n")
    for i, line := range lines[:min(3, len(lines))] {
        if line != "" {
            fmt.Printf("    %d: %s\n", i+1, line)
        }
    }
    if len(lines) > 3 {
        fmt.Printf("    ... (%d more lines)\n", len(lines)-3)
    }

    // JSON format to buffer
    var jsonBuf bytes.Buffer
    jsonFmt := JSONFormatter{Pretty: false}
    jsonBytes, _ := jsonFmt.Format(&jsonBuf, warningsAndErrors[:min(2, len(warningsAndErrors))])
    fmt.Printf("\n  JSON Lines format (%d bytes, first 2 entries):\n", jsonBytes)
    for _, line := range strings.Split(strings.TrimSpace(jsonBuf.String()), "\n") {
        fmt.Printf("    %s\n", line)
    }

    // CSV format to buffer
    var csvBuf bytes.Buffer
    csvFmt := CSVFormatter{}
    csvBytes, _ := csvFmt.Format(&csvBuf, entries[:min(3, len(entries))])
    fmt.Printf("\n  CSV format (%d bytes, first 3 entries):\n", csvBytes)
    for _, line := range strings.Split(strings.TrimSpace(csvBuf.String()), "\n") {
        fmt.Printf("    %s\n", line)
    }

    // --- 8. MultiWriter — write to multiple destinations ---

    fmt.Println("\n── 8. MultiWriter (dual output) ──")

    var alertBuf bytes.Buffer
    var auditBuf bytes.Buffer
    dual := io.MultiWriter(&alertBuf, &auditBuf)

    fatalEntries := ApplyFilters(entries, FilterBySeverity(FATAL))
    textFmt.Format(dual, fatalEntries)

    fmt.Printf("  FATAL alerts written to 2 destinations:\n")
    fmt.Printf("    Alert buffer: %d bytes\n", alertBuf.Len())
    fmt.Printf("    Audit buffer: %d bytes\n", auditBuf.Len())
    fmt.Printf("    Content: %s", alertBuf.String())

    // --- 9. Statistics (Stringer) ---

    fmt.Println("\n── 9. Statistics (fmt.Stringer) ──")

    stats := ComputeStats(entries, len(parseErrors))
    fmt.Println(stats) // usa stats.String() automaticamente

    // --- 10. io.Copy demo — pipe filtered logs ---

    fmt.Println("── 10. io.Copy Pipeline ──")

    // Crear reader desde formatted text
    errEntries := ApplyFilters(entries, FilterBySeverity(ERROR))
    var errBuf bytes.Buffer
    textFmt.Format(&errBuf, errEntries)

    // LimitReader: max 200 bytes de output
    limited := io.LimitReader(&errBuf, 200)

    // Copy to a new buffer (simulating file or network)
    var outputBuf bytes.Buffer
    n, err := io.Copy(&outputBuf, limited)
    fmt.Printf("  Copied %d bytes (limited to 200) of error logs\n", n)
    if err != nil {
        fmt.Printf("  Copy error: %v\n", err)
    }
    fmt.Printf("  Preview: %s...\n", strings.TrimSpace(outputBuf.String()))

    // --- Summary ---

    fmt.Println("\n── Interfaces Used ──")
    fmt.Println("  io.Reader     → SimulatedLogSource, io.MultiReader, io.TeeReader, io.LimitReader")
    fmt.Println("  io.Writer     → bytes.Buffer, io.MultiWriter, io.Discard")
    fmt.Println("  fmt.Stringer  → Severity, LogEntry, SimulatedLogSource, LogStats")
    fmt.Println("  error         → ParseError, FilterError, ErrEmptyLine, errors.Is/As")
    fmt.Println("  sort.Interface→ ByTimestamp, BySeverityDesc")
    fmt.Println("  TextMarshaler → Severity (JSON serialization)")
    fmt.Println("  io.Copy       → Pipeline from Reader to Writer")
}

func min(a, b int) int {
    if a < b {
        return a
    }
    return b
}
```

**Output esperado (parcial):**
```
╔══════════════════════════════════════════════════════╗
║   Infrastructure Log Pipeline — Common Interfaces   ║
╚══════════════════════════════════════════════════════╝

── 1. Log Sources (io.Reader) ──
  Source 1: LogSource{host=web-01, entries=8}
  Source 2: LogSource{host=db-01, entries=6}
  Source 3: LogSource{host=worker-01, entries=6}

── 3. Parsing Logs (bufio.Scanner over io.Reader) ──
  Parsed 19 entries, 1 errors
  Raw bytes read: 1247
  Parse error (line 8): expected 5+ fields, got 1

── 5. Filtering Logs ──
  Severity >= WARN: 7 entries
    [2026-04-03 10:00:03] WARN  web-01       nginx      slow request duration_ms=3200 path=/api/report
    [2026-04-03 10:00:04] WARN  db-01        postgres   connection pool near limit active=48 max=50
    ...

── 9. Statistics (fmt.Stringer) ──
Total entries: 19 (parse errors: 1)
By severity:
  FATAL     1 █
  ERROR     4 ████
  WARN      3 ███
  INFO     11 ███████████
By host:
  db-01          6
  web-01         7
  worker-01      6
Time range: 10:00:00 to 10:00:11 (11s)
```

---

## 16. Resumen

```
┌──────────────────────────────────────────────────────────────────────────┐
│  INTERFACES COMUNES DE GO — Resumen                                      │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  io.Reader  — Read([]byte)(int,error)                                    │
│  ├─ La abstraccion MAS importante de Go                                  │
│  ├─ Procesar p[:n] ANTES de verificar err                                │
│  ├─ Composicion: LimitReader, TeeReader, MultiReader, Pipe              │
│  └─ 50+ tipos la satisfacen (File, Conn, Buffer, gzip, http.Body...)   │
│                                                                          │
│  io.Writer  — Write([]byte)(int,error)                                   │
│  ├─ Si n < len(p), err debe ser no-nil                                   │
│  ├─ MultiWriter, Discard, Pipe                                          │
│  └─ fmt.Fprintf escribe formateado a cualquier Writer                    │
│                                                                          │
│  io.Closer  — Close() error                                              │
│  ├─ defer Close() siempre                                                │
│  ├─ Se combina: ReadCloser, WriteCloser, ReadWriteCloser                │
│  └─ io.NopCloser wrappea Reader → ReadCloser                            │
│                                                                          │
│  fmt.Stringer — String() string                                          │
│  ├─ fmt.Println, %v, %s lo usan automaticamente                         │
│  ├─ Cuidado con pointer receiver: valor no usa String()                  │
│  └─ Definir para TODOS tus tipos de dominio (IPs, sizes, durations)     │
│                                                                          │
│  error — Error() string                                                  │
│  ├─ Predeclarada (no necesita import)                                    │
│  ├─ errors.New, fmt.Errorf(%w), custom structs                           │
│  ├─ errors.Is (sentinel), errors.As (tipo custom)                        │
│  └─ Unwrap() error para cadenas de errores                               │
│                                                                          │
│  sort.Interface — Len() + Less(i,j) + Swap(i,j)                         │
│  ├─ Para tipos con ordenamiento reutilizable                             │
│  ├─ sort.Reverse invierte el orden                                       │
│  └─ Preferir slices.SortFunc en Go 1.21+ para casos ad-hoc              │
│                                                                          │
│  Regla de oro: si la stdlib tiene una interfaz, USALA.                   │
│  No reinventes Reader, Writer, Error, Stringer.                          │
│  Tu codigo se integrara con todo el ecosistema automaticamente.          │
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: S02/T01 - Interfaces pequeñas — principio de Go, aceptar interfaces devolver structs
