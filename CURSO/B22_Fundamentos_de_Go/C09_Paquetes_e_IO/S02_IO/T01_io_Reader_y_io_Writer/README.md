# io.Reader y io.Writer — la abstraccion central, composicion, io.Copy, io.TeeReader

## 1. Introduccion

`io.Reader` e `io.Writer` son las dos interfaces mas importantes de Go. No son solo "interfaces de I/O" — son el **patron de diseño fundamental** que conecta todo el ecosistema: archivos, red, compresion, cifrado, HTTP, JSON, buffers, hashes, y cualquier flujo de datos. Entender estas interfaces a fondo es entender como Go piensa sobre los datos.

La genialidad esta en la simplicidad: un solo metodo cada una. Eso las hace implementables por cualquier tipo y componibles entre si — puedes apilar un gzip.Reader sobre un bufio.Reader sobre un os.File, y cada capa solo sabe que lee bytes de un Reader.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                    io.Reader / io.Writer — EL CORAZON DE GO                     │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  INTERFACES (un solo metodo cada una)                                           │
│                                                                                  │
│  type Reader interface {                                                         │
│      Read(p []byte) (n int, err error)                                          │
│  }                                                                               │
│                                                                                  │
│  type Writer interface {                                                         │
│      Write(p []byte) (n int, err error)                                         │
│  }                                                                               │
│                                                                                  │
│  FILOSOFIA                                                                       │
│  ├─ Unix: "everything is a file"                                                │
│  ├─ Go:   "everything is a Reader or Writer"                                    │
│  │                                                                               │
│  ├─ Un archivo en disco         → Reader + Writer                               │
│  ├─ Una conexion TCP            → Reader + Writer                               │
│  ├─ Un HTTP response body       → Reader (ReadCloser)                           │
│  ├─ Un HTTP response writer     → Writer                                        │
│  ├─ Un buffer en memoria        → Reader + Writer                               │
│  ├─ Un string                   → Reader                                        │
│  ├─ Un compresor gzip           → Writer (escribe datos comprimidos)            │
│  ├─ Un descompresor gzip        → Reader (lee datos descomprimidos)             │
│  ├─ Un hasher SHA256            → Writer (alimenta el hash)                     │
│  ├─ Un cifrador AES             → Writer                                        │
│  ├─ Un encoder JSON             → Writer                                        │
│  ├─ Un decoder JSON             → Reader                                        │
│  └─ /dev/null                   → Writer (io.Discard)                           │
│                                                                                  │
│  COMPOSICION (el superpoder)                                                    │
│  File → BufReader → GzipReader → TeeReader(log) → JsonDecoder → struct        │
│  Cada capa solo sabe: "leo bytes de un Reader"                                  │
│  No les importa si los bytes vienen de disco, red, o memoria.                  │
│                                                                                  │
│  ANALOGIA UNIX                                                                  │
│  cat file.gz | gunzip | tee /dev/stderr | jq '.data'                           │
│  ≈ io.Reader pipeline en Go                                                     │
│                                                                                  │
│  ANALOGIA CON C y RUST                                                          │
│  C:    FILE*, read(fd, buf, n), write(fd, buf, n)                              │
│  Rust: std::io::Read, std::io::Write (traits, muy similar a Go)               │
│  Go:   io.Reader, io.Writer (interfaces, inspiraron a Rust)                    │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. io.Reader en profundidad

### 2.1 La interface

```go
type Reader interface {
    Read(p []byte) (n int, err error)
}
```

Un solo metodo con un contrato preciso. Parece simple, pero las **reglas del contrato** son sutiles y criticas:

### 2.2 Contrato de Read

```
CONTRATO DE io.Reader.Read(p []byte) (n int, err error)

1. Read lee HASTA len(p) bytes en p.
   Puede leer MENOS de len(p) — esto es normal, no un error.
   
2. Retorna el numero de bytes leidos (n) y un error (err).
   n puede ser 0 a len(p).

3. Al final del stream, Read retorna n >= 0, err = io.EOF.
   IMPORTANTE: puede retornar n > 0 Y err = io.EOF en la misma llamada.
   Esto significa: "aqui tienes los ultimos bytes, y ya no hay mas."

4. Si Read retorna n > 0, el caller debe procesar esos n bytes
   ANTES de mirar el error. Ignorar n cuando err != nil pierde datos.

5. Read NO debe retornar n = 0, err = nil (sin bytes y sin error).
   Eso seria un "Read vacio" que no avanza — confuso para el caller.
   Excepcion: cuando len(p) == 0.

6. El caller NO debe asumir que un solo Read retorna todos los datos.
   Debe llamar Read en un loop hasta recibir io.EOF o un error.

7. Read NO debe retener p despues de retornar.
   El caller puede reusar o modificar p despues de cada Read.
```

### 2.3 Patron correcto de lectura

```go
// PATRON: leer de un Reader correctamente
func readAll(r io.Reader) ([]byte, error) {
    var result []byte
    buf := make([]byte, 4096) // Buffer de lectura
    
    for {
        n, err := r.Read(buf)
        
        // PRIMERO: procesar los bytes leidos (incluso si hay error)
        if n > 0 {
            result = append(result, buf[:n]...)
        }
        
        // DESPUES: verificar el error
        if err == io.EOF {
            break // Fin del stream — terminamos
        }
        if err != nil {
            return nil, err // Error real
        }
    }
    
    return result, nil
}

// En la practica, usa io.ReadAll (que hace exactamente esto):
data, err := io.ReadAll(r)
```

### 2.4 Error comun: ignorar n cuando hay error

```go
// MAL — pierde los ultimos bytes
func readBad(r io.Reader) ([]byte, error) {
    var result []byte
    buf := make([]byte, 4096)
    
    for {
        n, err := r.Read(buf)
        if err != nil {       // ← Verifica error PRIMERO
            if err == io.EOF {
                break         // ← PIERDE buf[:n] si n > 0 !
            }
            return nil, err
        }
        result = append(result, buf[:n]...)
    }
    
    return result, nil
}

// BIEN — procesa n ANTES de verificar error
func readGood(r io.Reader) ([]byte, error) {
    var result []byte
    buf := make([]byte, 4096)
    
    for {
        n, err := r.Read(buf)
        if n > 0 {                          // ← Procesa bytes PRIMERO
            result = append(result, buf[:n]...)
        }
        if err == io.EOF {
            break
        }
        if err != nil {
            return nil, err
        }
    }
    
    return result, nil
}
```

### 2.5 Implementar io.Reader

```go
// Un Reader que genera ceros infinitamente (como /dev/zero)
type ZeroReader struct{}

func (z ZeroReader) Read(p []byte) (int, error) {
    for i := range p {
        p[i] = 0
    }
    return len(p), nil // Nunca retorna EOF — infinito
}

// Un Reader que genera una secuencia de bytes repetidos
type RepeatReader struct {
    data []byte
    pos  int
}

func NewRepeatReader(data []byte) *RepeatReader {
    return &RepeatReader{data: data}
}

func (r *RepeatReader) Read(p []byte) (int, error) {
    if len(r.data) == 0 {
        return 0, io.EOF
    }
    n := 0
    for n < len(p) {
        copied := copy(p[n:], r.data[r.pos:])
        n += copied
        r.pos += copied
        if r.pos >= len(r.data) {
            r.pos = 0 // Wrap around — repetir
        }
    }
    return n, nil
}

// Un Reader que cuenta bytes leidos
type CountingReader struct {
    reader    io.Reader
    BytesRead int64
}

func NewCountingReader(r io.Reader) *CountingReader {
    return &CountingReader{reader: r}
}

func (cr *CountingReader) Read(p []byte) (int, error) {
    n, err := cr.reader.Read(p)
    cr.BytesRead += int64(n)
    return n, err
}

// Uso:
func main() {
    f, _ := os.Open("largefile.dat")
    defer f.Close()
    
    cr := NewCountingReader(f)
    io.Copy(io.Discard, cr) // Lee todo
    fmt.Printf("Total bytes: %d\n", cr.BytesRead)
}
```

### 2.6 Readers de la stdlib

```go
// --- strings.NewReader: leer de un string ---
r := strings.NewReader("hello world")
buf := make([]byte, 5)
n, _ := r.Read(buf) // n=5, buf="hello"
n, _ = r.Read(buf)  // n=6, buf="world" (sobreescribe parcialmente)
// strings.Reader tambien implementa: io.ReaderAt, io.Seeker, io.WriterTo

// --- bytes.NewReader: leer de un []byte ---
r2 := bytes.NewReader([]byte{0x48, 0x65, 0x6c, 0x6c, 0x6f})
// Mismas interfaces que strings.Reader

// --- bytes.Buffer: Reader + Writer ---
var buf2 bytes.Buffer
buf2.WriteString("datos ")
buf2.WriteString("concatenados")
data, _ := io.ReadAll(&buf2) // "datos concatenados"
// Buffer es un Reader que se consume: despues de leer, los bytes desaparecen

// --- os.File: leer archivos ---
f, _ := os.Open("data.txt") // Implementa io.Reader (+ io.Writer, io.Closer, etc.)
defer f.Close()
data, _ = io.ReadAll(f)

// --- os.Stdin: leer de la terminal/pipe ---
// os.Stdin es un *os.File — implementa io.Reader
data, _ = io.ReadAll(os.Stdin) // Lee hasta EOF (Ctrl+D en terminal)

// --- http.Response.Body: leer body HTTP ---
resp, _ := http.Get("https://example.com")
defer resp.Body.Close() // Body es io.ReadCloser
data, _ = io.ReadAll(resp.Body)

// --- net.Conn: leer de una conexion TCP ---
conn, _ := net.Dial("tcp", "example.com:80")
defer conn.Close()
// conn implementa io.Reader — puedes hacer io.ReadAll(conn)
```

---

## 3. io.Writer en profundidad

### 3.1 La interface

```go
type Writer interface {
    Write(p []byte) (n int, err error)
}
```

### 3.2 Contrato de Write

```
CONTRATO DE io.Writer.Write(p []byte) (n int, err error)

1. Write escribe len(p) bytes de p al stream.
   Retorna el numero de bytes escritos (n) y un error.

2. n debe ser 0 <= n <= len(p).
   Si n < len(p), err DEBE ser != nil.
   Esto es diferente de Read: una escritura parcial SIEMPRE es un error.

3. Write NO debe modificar el slice p, ni siquiera temporalmente.

4. Write NO debe retener p despues de retornar.

DIFERENCIA CLAVE con Read:
  Read:  n < len(p) puede ser normal (leer parcial es OK)
  Write: n < len(p) SIEMPRE es error (escribir parcial = algo fallo)
```

### 3.3 Implementar io.Writer

```go
// Un Writer que cuenta bytes escritos
type CountingWriter struct {
    writer       io.Writer
    BytesWritten int64
}

func NewCountingWriter(w io.Writer) *CountingWriter {
    return &CountingWriter{writer: w}
}

func (cw *CountingWriter) Write(p []byte) (int, error) {
    n, err := cw.writer.Write(p)
    cw.BytesWritten += int64(n)
    return n, err
}

// Un Writer que convierte todo a mayusculas
type UpperWriter struct {
    writer io.Writer
}

func (uw *UpperWriter) Write(p []byte) (int, error) {
    upper := bytes.ToUpper(p)
    return uw.writer.Write(upper)
}

// Un Writer que agrega prefijo a cada linea
type PrefixWriter struct {
    writer    io.Writer
    prefix    string
    needsPrefix bool
}

func NewPrefixWriter(w io.Writer, prefix string) *PrefixWriter {
    return &PrefixWriter{writer: w, prefix: prefix, needsPrefix: true}
}

func (pw *PrefixWriter) Write(p []byte) (int, error) {
    total := 0
    for len(p) > 0 {
        if pw.needsPrefix {
            _, err := io.WriteString(pw.writer, pw.prefix)
            if err != nil {
                return total, err
            }
            pw.needsPrefix = false
        }
        
        idx := bytes.IndexByte(p, '\n')
        if idx < 0 {
            n, err := pw.writer.Write(p)
            total += n
            return total, err
        }
        
        n, err := pw.writer.Write(p[:idx+1])
        total += n
        if err != nil {
            return total, err
        }
        p = p[idx+1:]
        pw.needsPrefix = true
    }
    return total, nil
}

// Uso:
func main() {
    pw := NewPrefixWriter(os.Stdout, "[LOG] ")
    fmt.Fprintln(pw, "primera linea")
    fmt.Fprintln(pw, "segunda linea")
    // Output:
    // [LOG] primera linea
    // [LOG] segunda linea
}
```

### 3.4 Writers de la stdlib

```go
// --- os.File ---
f, _ := os.Create("output.txt")
defer f.Close()
f.Write([]byte("datos binarios"))
f.WriteString("datos string") // Shortcut
io.WriteString(f, "tambien funciona") // Funcion de io

// --- os.Stdout, os.Stderr ---
fmt.Fprintln(os.Stdout, "a stdout")
fmt.Fprintln(os.Stderr, "a stderr")

// --- bytes.Buffer ---
var buf bytes.Buffer
buf.WriteString("hello ")
buf.Write([]byte("world"))
fmt.Println(buf.String()) // "hello world"

// --- strings.Builder (solo escritura, mas eficiente que Buffer para strings) ---
var sb strings.Builder
sb.WriteString("hello ")
sb.WriteString("world")
fmt.Println(sb.String()) // "hello world"

// --- http.ResponseWriter ---
func handler(w http.ResponseWriter, r *http.Request) {
    w.Header().Set("Content-Type", "text/plain")
    w.Write([]byte("response body"))
    // w implementa io.Writer
}

// --- hash.Hash (SHA256, MD5, etc.) ---
h := sha256.New()
h.Write([]byte("datos para hashear"))
// h implementa io.Writer — puedes usar io.Copy para alimentar el hash
io.Copy(h, file) // Hashear un archivo entero
digest := h.Sum(nil)

// --- net.Conn ---
conn, _ := net.Dial("tcp", "example.com:80")
conn.Write([]byte("GET / HTTP/1.0\r\n\r\n"))

// --- gzip.Writer ---
gzw := gzip.NewWriter(file)
gzw.Write([]byte("datos a comprimir"))
gzw.Close() // IMPORTANTE: Close() flushea y escribe el footer gzip

// --- io.Discard (como /dev/null) ---
io.Copy(io.Discard, resp.Body) // Consumir body sin guardarlo
```

---

## 4. Interfaces compuestas

Go combina Reader y Writer con otras interfaces para expresar capacidades adicionales:

```go
// --- Interfaces compuestas de io ---

// ReadCloser: Reader que debe cerrarse (ej: HTTP body, archivo abierto para lectura)
type ReadCloser interface {
    Reader
    Closer
}

// WriteCloser: Writer que debe cerrarse (ej: gzip.Writer, archivo)
type WriteCloser interface {
    Writer
    Closer
}

// ReadWriter: puede leer y escribir (ej: net.Conn, os.File con O_RDWR)
type ReadWriter interface {
    Reader
    Writer
}

// ReadWriteCloser: lee, escribe, y cierra (ej: net.Conn, os.File)
type ReadWriteCloser interface {
    Reader
    Writer
    Closer
}

// ReadSeeker: puede leer y mover la posicion (ej: os.File, strings.Reader)
type ReadSeeker interface {
    Reader
    Seeker
}

// WriteSeeker: puede escribir y mover la posicion
type WriteSeeker interface {
    Writer
    Seeker
}

// ReadWriteSeeker: todo (ej: os.File abierto con O_RDWR)
type ReadWriteSeeker interface {
    Reader
    Writer
    Seeker
}
```

### 4.1 Closer

```go
type Closer interface {
    Close() error
}

// REGLA: si un tipo implementa Closer, DEBES llamar Close().
// Patrones:

// 1. defer Close() para lectura (ignorar error de Close es aceptable)
f, err := os.Open("data.txt")
if err != nil {
    return err
}
defer f.Close()

// 2. defer Close() para escritura (el error de Close importa!)
f, err := os.Create("output.txt")
if err != nil {
    return err
}
defer func() {
    if cerr := f.Close(); cerr != nil && err == nil {
        err = cerr // Capturar error de Close
    }
}()
// Esto importa porque el OS puede bufferear writes.
// Close() flushea el buffer — si el disco esta lleno, Close falla.
// Si ignoras el error de Close, podrías perder datos.

// 3. NopCloser: convertir Reader a ReadCloser (Close no hace nada)
rc := io.NopCloser(strings.NewReader("datos"))
// Util cuando una API requiere ReadCloser pero tu Reader no necesita cerrarse
```

### 4.2 Seeker

```go
type Seeker interface {
    Seek(offset int64, whence int) (int64, error)
    // whence: io.SeekStart (0), io.SeekCurrent (1), io.SeekEnd (2)
}

f, _ := os.Open("data.bin")
defer f.Close()

// Ir al byte 100 desde el inicio
pos, _ := f.Seek(100, io.SeekStart) // pos = 100

// Avanzar 50 bytes desde la posicion actual
pos, _ = f.Seek(50, io.SeekCurrent) // pos = 150

// Ir a 10 bytes antes del final
pos, _ = f.Seek(-10, io.SeekEnd) // pos = fileSize - 10

// Obtener posicion actual sin mover
pos, _ = f.Seek(0, io.SeekCurrent) // pos = posicion actual

// Rebobinar al inicio
f.Seek(0, io.SeekStart)
```

### 4.3 ReaderAt y WriterAt

```go
// ReaderAt: leer en una posicion especifica sin mover el cursor
type ReaderAt interface {
    ReadAt(p []byte, off int64) (n int, err error)
}

// WriterAt: escribir en una posicion especifica sin mover el cursor
type WriterAt interface {
    WriteAt(p []byte, off int64) (n int, err error)
}

// Ambas son seguras para uso concurrente — multiples goroutines
// pueden hacer ReadAt/WriteAt simultaneamente porque cada llamada
// especifica su propia posicion (no depende del cursor compartido).

f, _ := os.Open("data.bin")
defer f.Close()

// Leer 10 bytes en la posicion 1000 (sin mover el cursor)
buf := make([]byte, 10)
n, err := f.ReadAt(buf, 1000)
// Otra goroutine puede hacer ReadAt simultaneamente sin conflicto
```

### 4.4 WriterTo y ReaderFrom (optimizacion)

```go
// WriterTo: un Reader que sabe escribirse a un Writer eficientemente
type WriterTo interface {
    WriteTo(w Writer) (n int64, err error)
}

// ReaderFrom: un Writer que sabe leer de un Reader eficientemente
type ReaderFrom interface {
    ReadFrom(r Reader) (n int64, err error)
}

// io.Copy VERIFICA si src implementa WriterTo o dst implementa ReaderFrom.
// Si alguno lo hace, usa la implementacion optimizada en vez del loop buffer.

// Ejemplo: os.File implementa ReaderFrom.
// En Linux, puede usar sendfile(2) para copiar directamente en el kernel
// sin copiar datos a userspace — zero-copy.

src, _ := os.Open("input.dat")
dst, _ := os.Create("output.dat")
io.Copy(dst, src) // Internamente usa sendfile() en Linux
// Cero copias a userspace — los datos van directo de un fd a otro en el kernel
src.Close()
dst.Close()
```

---

## 5. io.Copy — la funcion central

### 5.1 Basico

`io.Copy` es la navaja suiza del I/O en Go. Copia bytes de un Reader a un Writer:

```go
func Copy(dst Writer, src Reader) (written int64, err error)
```

```go
// Copiar archivo
src, _ := os.Open("input.txt")
dst, _ := os.Create("output.txt")
written, err := io.Copy(dst, src)
fmt.Printf("Copiados %d bytes\n", written)
src.Close()
dst.Close()

// Descargar archivo de HTTP
resp, _ := http.Get("https://example.com/file.zip")
f, _ := os.Create("file.zip")
written, _ = io.Copy(f, resp.Body)
resp.Body.Close()
f.Close()

// Enviar archivo como HTTP response
func serveFile(w http.ResponseWriter, r *http.Request) {
    f, err := os.Open("report.pdf")
    if err != nil {
        http.Error(w, "not found", 404)
        return
    }
    defer f.Close()
    
    w.Header().Set("Content-Type", "application/pdf")
    io.Copy(w, f) // Copia el archivo al response
}

// Hashear un archivo
f, _ := os.Open("data.bin")
defer f.Close()
h := sha256.New()
io.Copy(h, f)          // h implementa Writer — io.Copy alimenta el hash
digest := h.Sum(nil)    // Obtener el hash final
fmt.Printf("%x\n", digest)

// Descartar datos (consumir un Reader sin guardar)
io.Copy(io.Discard, resp.Body) // Como > /dev/null
```

### 5.2 Internals de io.Copy

```go
// io.Copy internamente hace esto (simplificado):
func Copy(dst Writer, src Reader) (written int64, err error) {
    // Optimizacion 1: si src implementa WriterTo, usarlo
    if wt, ok := src.(WriterTo); ok {
        return wt.WriteTo(dst)
    }
    
    // Optimizacion 2: si dst implementa ReaderFrom, usarlo
    if rf, ok := dst.(ReaderFrom); ok {
        return rf.ReadFrom(src)
    }
    
    // Fallback: loop con buffer de 32KB
    buf := make([]byte, 32*1024) // 32KB
    for {
        nr, er := src.Read(buf)
        if nr > 0 {
            nw, ew := dst.Write(buf[:nr])
            if nw < 0 || nr < nw {
                nw = 0
                if ew == nil {
                    ew = errors.New("invalid write result")
                }
            }
            written += int64(nw)
            if ew != nil {
                err = ew
                break
            }
            if nr != nw {
                err = ErrShortWrite
                break
            }
        }
        if er != nil {
            if er != EOF {
                err = er
            }
            break
        }
    }
    return written, err
}
```

```
OPTIMIZACIONES DE io.Copy

1. WriterTo → src.WriteTo(dst)
   strings.Reader, bytes.Buffer, bytes.Reader implementan WriterTo.
   Evitan el buffer intermedio.

2. ReaderFrom → dst.ReadFrom(src)
   os.File implementa ReaderFrom.
   En Linux usa sendfile(2) — zero-copy kernel a kernel.
   En otros OS usa splice(2) o fallback a buffer.

3. Buffer de 32KB
   Si ninguna optimizacion aplica, usa un buffer de 32KB.
   Bueno para la mayoria de casos — suficientemente grande para
   I/O eficiente, suficientemente pequeno para no desperdiciar memoria.
```

### 5.3 io.CopyN y io.CopyBuffer

```go
// io.CopyN: copiar exactamente n bytes
written, err := io.CopyN(dst, src, 1024) // Copiar maximo 1024 bytes
// Si src tiene menos de 1024 bytes, err = io.EOF

// io.CopyBuffer: como Copy pero con buffer proporcionado por el caller
buf := make([]byte, 64*1024) // Buffer de 64KB (en vez de 32KB default)
written, err := io.CopyBuffer(dst, src, buf)
// Util para controlar el tamaño del buffer o reusar buffers
// Si buf es nil, se comporta igual que io.Copy
```

---

## 6. Funciones utilitarias de io

### 6.1 io.ReadAll

```go
// Leer TODO de un Reader en un []byte
// CUIDADO: carga todo en memoria — no usar con streams enormes
data, err := io.ReadAll(r)

// Equivalente a:
func ReadAll(r Reader) ([]byte, error) {
    return io.ReadAll(r) // Antes era ioutil.ReadAll
}

// Usos comunes:
body, _ := io.ReadAll(resp.Body) // HTTP response body
content, _ := io.ReadAll(f)       // Archivo completo

// CUANDO NO USAR:
// - Archivos de varios GB → usa io.Copy o bufio.Scanner
// - Streaming data → usa io.Copy o procesar en chunks
// - HTTP bodies potencialmente enormes → limitar primero
limited := io.LimitReader(resp.Body, 10*1024*1024) // Max 10MB
data, err := io.ReadAll(limited)
```

### 6.2 io.ReadFull

```go
// Leer EXACTAMENTE len(buf) bytes — error si no hay suficientes
buf := make([]byte, 100)
n, err := io.ReadFull(r, buf)
// n < 100 → err = io.ErrUnexpectedEOF
// n == 100 → err = nil
// r vacio → err = io.EOF

// Usos: leer headers de formato binario con tamaño fijo
header := make([]byte, 16) // Header siempre es 16 bytes
_, err := io.ReadFull(conn, header)
if err != nil {
    return fmt.Errorf("incomplete header: %w", err)
}
```

### 6.3 io.ReadAtLeast

```go
// Leer AL MENOS min bytes
buf := make([]byte, 1024)
n, err := io.ReadAtLeast(r, buf, 100)
// Lee hasta len(buf) bytes, pero falla si no hay al menos 100
// Si n < 100 → err = io.ErrUnexpectedEOF
```

### 6.4 io.WriteString

```go
// Escribir un string a un Writer
// Si el Writer implementa io.StringWriter, lo usa (evita conversion []byte)
io.WriteString(w, "hello world")

// Internamente:
func WriteString(w Writer, s string) (n int, err error) {
    if sw, ok := w.(StringWriter); ok {
        return sw.WriteString(s) // Evita []byte(s) allocation
    }
    return w.Write([]byte(s))
}
// os.File, bytes.Buffer, strings.Builder implementan StringWriter
```

---

## 7. Wrappers de Reader y Writer

### 7.1 io.LimitReader — limitar lectura

```go
// Limitar cuantos bytes se leen de un Reader
// Devuelve un Reader que retorna EOF despues de n bytes
limited := io.LimitReader(r, 1024) // Maximo 1024 bytes

// Uso critico: proteger contra DoS en HTTP servers
func handler(w http.ResponseWriter, r *http.Request) {
    // Limitar body a 1MB para prevenir que alguien envie un body de 10GB
    body := io.LimitReader(r.Body, 1024*1024) // 1MB max
    data, err := io.ReadAll(body)
    if err != nil {
        http.Error(w, "read error", 500)
        return
    }
    // Procesar data...
    
    // http.MaxBytesReader es aun mejor para HTTP (retorna error 413):
    r.Body = http.MaxBytesReader(w, r.Body, 1024*1024)
}

// LimitReader NO retorna error cuando se alcanza el limite — solo EOF.
// Si necesitas detectar que se excedio el limite:
limited := io.LimitReader(r, maxSize+1) // Leer 1 byte extra
data, _ := io.ReadAll(limited)
if int64(len(data)) > maxSize {
    return fmt.Errorf("data exceeds %d bytes", maxSize)
}
```

### 7.2 io.TeeReader — leer y duplicar

```go
// TeeReader: lee de r y simultaneamente escribe a w
// Como el comando tee de Unix: cat file | tee copy.txt
func TeeReader(r Reader, w Writer) Reader

// Uso: logging de datos mientras se procesan
func processWithLogging(r io.Reader) error {
    // Todo lo que leemos se copia automaticamente a stderr
    tee := io.TeeReader(r, os.Stderr)
    
    var data MyStruct
    return json.NewDecoder(tee).Decode(&data)
    // El JSON se imprime en stderr mientras se decodifica
}

// Uso: calcular hash mientras se copia
func copyWithHash(dst io.Writer, src io.Reader) ([]byte, error) {
    h := sha256.New()
    tee := io.TeeReader(src, h)       // Alimentar el hash mientras leemos
    
    _, err := io.Copy(dst, tee)       // Copiar al destino
    if err != nil {
        return nil, err
    }
    
    return h.Sum(nil), nil            // Hash de todo lo copiado
}

// Uso: guardar body HTTP y procesarlo
func handler(w http.ResponseWriter, r *http.Request) {
    // Guardar una copia del body para logging
    var bodyLog bytes.Buffer
    tee := io.TeeReader(r.Body, &bodyLog)
    
    var req CreateRequest
    if err := json.NewDecoder(tee).Decode(&req); err != nil {
        log.Printf("invalid body: %s", bodyLog.String())
        http.Error(w, "bad request", 400)
        return
    }
    
    log.Printf("received: %s", bodyLog.String())
    // Procesar req...
}
```

### 7.3 io.MultiReader — concatenar readers

```go
// MultiReader: multiples Readers en secuencia, como si fueran uno
// Como cat file1 file2 file3

header := strings.NewReader("--- BEGIN ---\n")
body, _ := os.Open("data.txt")
footer := strings.NewReader("--- END ---\n")

combined := io.MultiReader(header, body, footer)
io.Copy(os.Stdout, combined)
// Output:
// --- BEGIN ---
// (contenido de data.txt)
// --- END ---
body.Close()

// Uso: construir un HTTP request body de multiples partes
func buildBody(preamble string, data []byte, postamble string) io.Reader {
    return io.MultiReader(
        strings.NewReader(preamble),
        bytes.NewReader(data),
        strings.NewReader(postamble),
    )
}
```

### 7.4 io.MultiWriter — escribir a multiples destinos

```go
// MultiWriter: escribe a TODOS los writers simultaneamente
// Como tee pero con multiples destinos

logFile, _ := os.Create("app.log")
defer logFile.Close()

// Escribir a stdout Y al archivo simultaneamente
multi := io.MultiWriter(os.Stdout, logFile)
fmt.Fprintln(multi, "este mensaje va a ambos destinos")

// Uso avanzado: log + hash + archivo
h := sha256.New()
outFile, _ := os.Create("output.dat")
defer outFile.Close()

multi = io.MultiWriter(outFile, h, os.Stdout)
io.Copy(multi, inputReader)
// Los datos se escriben a: archivo, hash, y stdout simultaneamente
// Una sola pasada por los datos — muy eficiente

digest := h.Sum(nil)
fmt.Printf("\nSHA256: %x\n", digest)
```

### 7.5 io.Pipe — Reader/Writer conectados

```go
// Pipe: crea un par Reader/Writer sincronizados
// Lo que escribes al Writer, lo lees del Reader
// Util para conectar codigo que produce datos con codigo que los consume
// cuando uno espera Writer y el otro espera Reader

pr, pw := io.Pipe()

// Goroutine productora: escribe al pipe
go func() {
    defer pw.Close() // IMPORTANTE: cerrar el writer cuando termines
    
    encoder := json.NewEncoder(pw)
    for _, item := range items {
        if err := encoder.Encode(item); err != nil {
            pw.CloseWithError(err) // Propaga el error al reader
            return
        }
    }
}()

// Goroutine consumidora: lee del pipe
decoder := json.NewDecoder(pr)
for decoder.More() {
    var item Item
    if err := decoder.Decode(&item); err != nil {
        log.Fatal(err)
    }
    process(item)
}
pr.Close()

// Uso: generar datos on-the-fly para HTTP request
func uploadData(items []Item) error {
    pr, pw := io.Pipe()
    
    // Producir JSON en background
    go func() {
        defer pw.Close()
        json.NewEncoder(pw).Encode(items)
    }()
    
    // El body del request lee del pipe — streaming, sin buffer completo
    resp, err := http.Post("https://api.example.com/data", "application/json", pr)
    if err != nil {
        return err
    }
    defer resp.Body.Close()
    return nil
}
```

```
io.Pipe — CARACTERISTICAS

1. SINCRONIZADO: Write bloquea hasta que Read consume los datos
   No hay buffer interno — es rendez-vous.

2. SEGURO para goroutines: tipicamente una goroutine escribe y otra lee.

3. CLOSE:
   pw.Close()             → el Reader ve io.EOF
   pw.CloseWithError(err) → el Reader ve ese error
   pr.Close()             → el Writer ve io.ErrClosedPipe
   pr.CloseWithError(err) → el Writer ve ese error

4. SIN BUFFER: si necesitas buffer, wrappea con bufio:
   bw := bufio.NewWriter(pw)  // Buffer de escritura
   br := bufio.NewReader(pr)  // Buffer de lectura
```

---

## 8. bufio — I/O buffered

`bufio` envuelve Readers y Writers con buffers para mejorar el rendimiento y agregar funcionalidad como lectura por lineas:

### 8.1 bufio.Reader

```go
import "bufio"

// Crear un buffered reader (default: 4KB buffer)
br := bufio.NewReader(file)

// Con buffer custom
br = bufio.NewReaderSize(file, 64*1024) // 64KB buffer

// Leer una linea completa (incluyendo \n)
line, err := br.ReadString('\n')
// line = "contenido de la linea\n"
// Al final del archivo sin \n final: err = io.EOF

// Leer bytes hasta un delimitador
data, err := br.ReadBytes(':')
// data = []byte("key:")

// Peek: ver los proximos N bytes SIN avanzar la posicion
next5, err := br.Peek(5)
// next5 contiene los proximos 5 bytes, pero el cursor no avanza
// El proximo Read leerá esos mismos bytes

// ReadByte: leer un solo byte
b, err := br.ReadByte()

// UnreadByte: "devolver" el ultimo byte leido
br.UnreadByte() // Deshace el ultimo ReadByte

// ReadRune: leer un rune (caracter Unicode, puede ser multi-byte)
r, size, err := br.ReadRune()

// Buffered: cuantos bytes hay en el buffer actualmente
fmt.Println("bytes en buffer:", br.Buffered())

// Reset: reusar el bufio.Reader con un nuevo source
br.Reset(otherFile) // Evita nueva allocacion
```

### 8.2 bufio.Scanner — leer por lineas (lo mas comun)

```go
// Scanner: la forma idiomatica de leer lineas en Go
scanner := bufio.NewScanner(file)

for scanner.Scan() {
    line := scanner.Text() // String SIN \n al final
    fmt.Println(line)
}

if err := scanner.Err(); err != nil {
    log.Fatal(err)
}

// Leer desde stdin (linea por linea):
scanner = bufio.NewScanner(os.Stdin)
fmt.Println("Escribe lineas (Ctrl+D para terminar):")
for scanner.Scan() {
    fmt.Printf("Leido: %q\n", scanner.Text())
}

// Leer por palabras en vez de lineas:
scanner = bufio.NewScanner(file)
scanner.Split(bufio.ScanWords)
for scanner.Scan() {
    word := scanner.Text()
    fmt.Println("Palabra:", word)
}

// Split functions disponibles:
// bufio.ScanLines  → por lineas (default)
// bufio.ScanWords  → por palabras (separadas por whitespace)
// bufio.ScanRunes  → por runes (caracteres Unicode)
// bufio.ScanBytes  → por bytes individuales

// Custom split function:
scanner.Split(func(data []byte, atEOF bool) (advance int, token []byte, err error) {
    // Separar por doble newline (parrafos)
    if i := bytes.Index(data, []byte("\n\n")); i >= 0 {
        return i + 2, data[:i], nil
    }
    if atEOF && len(data) > 0 {
        return len(data), data, nil
    }
    return 0, nil, nil // Necesita mas datos
})

// Aumentar el tamaño maximo de linea (default: 64KB)
scanner.Buffer(make([]byte, 1024*1024), 1024*1024) // Buffer de 1MB
// Sin esto, lineas mayores a 64KB causan error: "bufio.Scanner: token too long"
```

### 8.3 bufio.Writer

```go
// Buffered writer: acumula writes y los flushea en lotes
bw := bufio.NewWriter(file)

bw.WriteString("linea 1\n")
bw.WriteString("linea 2\n")
bw.WriteString("linea 3\n")
// NADA se ha escrito al archivo todavia — todo esta en el buffer

bw.Flush() // AHORA se escribe todo al archivo
// IMPORTANTE: siempre hacer Flush() o Close() al final
// Si no haces Flush, los datos se PIERDEN

// Patron correcto con defer:
bw = bufio.NewWriter(file)
defer bw.Flush() // Asegurar flush antes de cerrar el file

for _, item := range items {
    fmt.Fprintf(bw, "%s: %d\n", item.Name, item.Value)
}
// Flush se ejecuta al salir de la funcion

// Con buffer custom:
bw = bufio.NewWriterSize(file, 256*1024) // 256KB buffer

// Disponible: cuantos bytes caben antes del proximo flush
fmt.Println("espacio disponible:", bw.Available())

// Buffered: cuantos bytes hay acumulados sin flushear
fmt.Println("bytes pendientes:", bw.Buffered())

// Reset: reusar con otro writer
bw.Reset(otherFile)
```

### 8.4 Por que usar bufio?

```
POR QUE BUFIO — RENDIMIENTO

Sin buffer (os.File directamente):
  Cada Write() = 1 syscall al kernel
  10,000 WriteString("linea\n") = 10,000 syscalls
  Syscalls son caros (~1-10 microsegundos cada uno)

Con bufio.Writer (4KB buffer default):
  Los writes se acumulan en el buffer (en userspace)
  Solo cuando el buffer se llena, se hace UN syscall
  10,000 WriteString("linea\n") ≈ 15 syscalls (60KB / 4KB)
  
  Mejora tipica: 10x-100x mas rapido para writes pequenos

Analogia:
  Sin bufio = ir al supermercado a comprar 1 item a la vez
  Con bufio = hacer una lista y comprar todo de una vez

CUANDO NO NECESITAS BUFIO:
  - io.ReadAll() (ya lee todo de una vez)
  - io.Copy() (ya usa buffer interno de 32KB)
  - Writes grandes (un Write de 1MB no necesita buffer extra)
  - os.ReadFile() / os.WriteFile() (ya buffered internamente)
```

---

## 9. Patron de composicion (pipeline)

El verdadero poder de Reader/Writer es la composicion. Cada wrapper implementa la misma interface, asi que puedes apilarlos:

### 9.1 Pipeline de lectura

```go
// Leer un archivo gzip con JSON, calcular hash, y loggear
func processGzipJSON(filename string) ([]Record, []byte, error) {
    // Capa 1: archivo
    f, err := os.Open(filename)
    if err != nil {
        return nil, nil, err
    }
    defer f.Close()
    
    // Capa 2: contar bytes leidos
    counter := NewCountingReader(f)
    
    // Capa 3: descomprimir gzip
    gz, err := gzip.NewReader(counter)
    if err != nil {
        return nil, nil, err
    }
    defer gz.Close()
    
    // Capa 4: calcular hash del contenido descomprimido
    h := sha256.New()
    tee := io.TeeReader(gz, h)
    
    // Capa 5: buffered reader para rendimiento
    br := bufio.NewReader(tee)
    
    // Capa 6: decodificar JSON
    var records []Record
    if err := json.NewDecoder(br).Decode(&records); err != nil {
        return nil, nil, err
    }
    
    fmt.Printf("Bytes del disco: %d\n", counter.BytesRead)
    fmt.Printf("Records: %d\n", len(records))
    
    return records, h.Sum(nil), nil
}

// Pipeline visual:
//
// Disco → File → CountingReader → GzipReader → TeeReader → BufReader → JsonDecoder
//                                                   ↓
//                                              SHA256 Hash
//
// Cada capa solo sabe: "leo de un io.Reader"
// No les importa que hay debajo.
```

### 9.2 Pipeline de escritura

```go
// Escribir JSON comprimido con gzip a archivo, con hash y progreso
func writeCompressedJSON(filename string, data any) ([]byte, error) {
    // Capa 1: archivo
    f, err := os.Create(filename)
    if err != nil {
        return nil, err
    }
    defer f.Close()
    
    // Capa 2: contar bytes escritos al disco (comprimidos)
    counter := NewCountingWriter(f)
    
    // Capa 3: calcular hash del archivo final (comprimido)
    h := sha256.New()
    multi := io.MultiWriter(counter, h)
    
    // Capa 4: comprimir gzip
    gz, err := gzip.NewWriterLevel(multi, gzip.BestCompression)
    if err != nil {
        return nil, err
    }
    
    // Capa 5: buffered writer para rendimiento
    bw := bufio.NewWriter(gz)
    
    // Capa 6: encodificar JSON
    encoder := json.NewEncoder(bw)
    encoder.SetIndent("", "  ")
    if err := encoder.Encode(data); err != nil {
        return nil, err
    }
    
    // FLUSH en orden inverso
    if err := bw.Flush(); err != nil {
        return nil, err
    }
    if err := gz.Close(); err != nil { // Close flushea Y escribe footer gzip
        return nil, err
    }
    
    fmt.Printf("Bytes escritos al disco: %d\n", counter.BytesWritten)
    
    return h.Sum(nil), nil
}

// Pipeline visual:
//
// JsonEncoder → BufWriter → GzipWriter → MultiWriter → File
//                                              ↓
//                                         SHA256 Hash
//                                              ↓
//                                        CountingWriter
```

### 9.3 Pipeline bidireccional con Pipe

```go
// Producir datos en una goroutine, consumir en otra
// Sin cargar todo en memoria
func streamProcess() error {
    pr, pw := io.Pipe()
    
    errCh := make(chan error, 1)
    
    // Productor: genera CSV
    go func() {
        defer pw.Close()
        bw := bufio.NewWriter(pw)
        defer bw.Flush()
        
        writer := csv.NewWriter(bw)
        for i := 0; i < 1_000_000; i++ {
            writer.Write([]string{
                strconv.Itoa(i),
                fmt.Sprintf("item_%d", i),
                strconv.FormatFloat(rand.Float64()*100, 'f', 2, 64),
            })
        }
        writer.Flush()
        errCh <- writer.Error()
    }()
    
    // Consumidor: comprimir y guardar
    gz, _ := gzip.NewWriterLevel(os.Stdout, gzip.BestSpeed)
    _, err := io.Copy(gz, pr)
    gz.Close()
    pr.Close()
    
    if prodErr := <-errCh; prodErr != nil {
        return prodErr
    }
    return err
}
```

---

## 10. Patrones avanzados

### 10.1 Reader wrapper generico

```go
// Patron: middleware de Reader (similar a HTTP middleware)
type ReaderFunc func(io.Reader) io.Reader

// Aplicar una cadena de transformaciones
func Chain(r io.Reader, transforms ...ReaderFunc) io.Reader {
    for _, t := range transforms {
        r = t(r)
    }
    return r
}

// Ejemplo:
func withLimit(n int64) ReaderFunc {
    return func(r io.Reader) io.Reader {
        return io.LimitReader(r, n)
    }
}

func withCounting(counter *int64) ReaderFunc {
    return func(r io.Reader) io.Reader {
        return &countingReader{reader: r, count: counter}
    }
}

func withDecompress() ReaderFunc {
    return func(r io.Reader) io.Reader {
        gz, err := gzip.NewReader(r)
        if err != nil {
            return &errorReader{err: err}
        }
        return gz
    }
}

// Uso:
var bytesRead int64
r := Chain(file,
    withLimit(10*1024*1024),  // Max 10MB
    withCounting(&bytesRead),
    withDecompress(),
)
data, err := io.ReadAll(r)
```

### 10.2 Progress reader

```go
// Reader que reporta progreso de lectura
type ProgressReader struct {
    reader   io.Reader
    total    int64
    current  int64
    callback func(current, total int64)
}

func NewProgressReader(r io.Reader, total int64, cb func(int64, int64)) *ProgressReader {
    return &ProgressReader{
        reader:   r,
        total:    total,
        callback: cb,
    }
}

func (pr *ProgressReader) Read(p []byte) (int, error) {
    n, err := pr.reader.Read(p)
    pr.current += int64(n)
    if pr.callback != nil {
        pr.callback(pr.current, pr.total)
    }
    return n, err
}

// Uso: descarga con progreso
func downloadWithProgress(url, filename string) error {
    resp, err := http.Get(url)
    if err != nil {
        return err
    }
    defer resp.Body.Close()
    
    f, err := os.Create(filename)
    if err != nil {
        return err
    }
    defer f.Close()
    
    progress := NewProgressReader(resp.Body, resp.ContentLength,
        func(current, total int64) {
            if total > 0 {
                pct := float64(current) / float64(total) * 100
                fmt.Printf("\r%.1f%% (%d / %d bytes)", pct, current, total)
            }
        },
    )
    
    _, err = io.Copy(f, progress)
    fmt.Println() // Newline despues del progreso
    return err
}
```

### 10.3 Adapter: convertir entre Reader y channel

```go
// Channel → Reader
type ChanReader struct {
    ch     <-chan []byte
    buf    []byte
    closed bool
}

func NewChanReader(ch <-chan []byte) *ChanReader {
    return &ChanReader{ch: ch}
}

func (cr *ChanReader) Read(p []byte) (int, error) {
    if cr.closed {
        return 0, io.EOF
    }
    
    // Si hay datos en el buffer, servirlos primero
    if len(cr.buf) > 0 {
        n := copy(p, cr.buf)
        cr.buf = cr.buf[n:]
        return n, nil
    }
    
    // Leer del channel
    data, ok := <-cr.ch
    if !ok {
        cr.closed = true
        return 0, io.EOF
    }
    
    n := copy(p, data)
    if n < len(data) {
        cr.buf = data[n:] // Guardar el sobrante
    }
    return n, nil
}

// Reader → Channel
func ReaderToChan(r io.Reader, bufSize int) <-chan []byte {
    ch := make(chan []byte)
    go func() {
        defer close(ch)
        buf := make([]byte, bufSize)
        for {
            n, err := r.Read(buf)
            if n > 0 {
                data := make([]byte, n)
                copy(data, buf[:n])
                ch <- data
            }
            if err != nil {
                return
            }
        }
    }()
    return ch
}
```

---

## 11. Comparacion con C y Rust

### 11.1 C: read()/write() y FILE*

```c
// C: I/O de bajo nivel con file descriptors
#include <unistd.h>
#include <fcntl.h>

int fd = open("data.txt", O_RDONLY);
char buf[4096];
ssize_t n;

// Loop de lectura (similar al patron de Go)
while ((n = read(fd, buf, sizeof(buf))) > 0) {
    // Procesar buf[:n]
    write(STDOUT_FILENO, buf, n); // Escribir a stdout
}
if (n < 0) {
    perror("read error");
}
close(fd);

// C: I/O buffered con FILE*
#include <stdio.h>

FILE *f = fopen("data.txt", "r");
char line[1024];

// Leer por lineas (como bufio.Scanner)
while (fgets(line, sizeof(line), f) != NULL) {
    printf("%s", line); // line incluye \n
}
fclose(f);

// fprintf ≈ fmt.Fprintf
fprintf(stderr, "error: %s\n", message);
```

### 11.2 Rust: Read y Write traits

```rust
// Rust: traits Read y Write (muy similar a Go)
use std::io::{self, Read, Write, BufRead, BufReader, BufWriter};
use std::fs::File;

fn main() -> io::Result<()> {
    // Leer archivo
    let f = File::open("data.txt")?;
    let mut reader = BufReader::new(f);
    
    // Leer por lineas (como bufio.Scanner en Go)
    for line in reader.lines() {
        let line = line?;
        println!("{}", line);
    }
    
    // Read trait (como io.Reader):
    // fn read(&mut self, buf: &mut [u8]) -> io::Result<usize>
    
    // Leer todo (como io.ReadAll):
    let mut content = String::new();
    File::open("data.txt")?.read_to_string(&mut content)?;
    
    // Escribir (como io.Writer):
    let mut f = File::create("output.txt")?;
    f.write_all(b"hello world")?; // write_all = escribir TODO o error
    // Nota: write() puede ser parcial (como Go Write)
    // write_all() garantiza escritura completa (como io.Copy)
    
    // BufWriter (como bufio.Writer):
    let f = File::create("output.txt")?;
    let mut bw = BufWriter::new(f);
    writeln!(bw, "linea {}", 1)?; // writeln! ≈ fmt.Fprintln
    bw.flush()?;
    
    // Composicion (como Go):
    // File → BufReader → GzDecoder → serde_json::from_reader
    
    Ok(())
}
```

### 11.3 Tabla comparativa

```
┌─────────────────────────┬──────────────────────────┬────────────────────────┬──────────────────────────┐
│ Concepto                 │ Go                       │ C                      │ Rust                     │
├─────────────────────────┼──────────────────────────┼────────────────────────┼──────────────────────────┤
│ Interface/trait           │ io.Reader (1 metodo)     │ No (convencion)        │ std::io::Read (trait)    │
│ Lectura basica           │ r.Read(buf)              │ read(fd, buf, n)       │ r.read(&mut buf)         │
│ Escritura basica         │ w.Write(buf)             │ write(fd, buf, n)      │ w.write(buf)             │
│ Escribir todo            │ — (Write ya lo requiere) │ — (loop manual)        │ w.write_all(buf)         │
│ Leer todo                │ io.ReadAll(r)            │ Loop manual            │ r.read_to_end(&mut vec)  │
│ Copiar stream            │ io.Copy(dst, src)        │ Loop manual + sendfile │ io::copy(&mut r, &mut w) │
│ Tee (duplicar)           │ io.TeeReader(r, w)       │ tee(2) + manual        │ No std (manual o crate)  │
│ Multi-write              │ io.MultiWriter(w1, w2)   │ Loop manual            │ No std (manual)          │
│ Multi-read               │ io.MultiReader(r1, r2)   │ No (manual)            │ r1.chain(r2)             │
│ Limitar lectura          │ io.LimitReader(r, n)     │ Manual                 │ r.take(n)                │
│ Buffered read            │ bufio.NewReader(r)       │ setvbuf / fgets        │ BufReader::new(r)        │
│ Buffered write           │ bufio.NewWriter(w)       │ setvbuf / fprintf      │ BufWriter::new(w)        │
│ Leer lineas              │ bufio.Scanner            │ fgets / getline        │ BufRead::lines()         │
│ Pipe                     │ io.Pipe()                │ pipe(2)                │ No std (channel manual)  │
│ Devolver sin leer        │ bufio.UnreadByte         │ ungetc                 │ No (usar BufReader)      │
│ Seek                     │ io.Seeker                │ lseek(fd, off, whence) │ io::Seek trait            │
│ Close                    │ io.Closer                │ close(fd) / fclose(f)  │ Drop trait (automatico)  │
│ Discard                  │ io.Discard               │ /dev/null              │ io::sink()               │
│ NopClose                 │ io.NopCloser             │ N/A                    │ No std                   │
│ Read posicion fija       │ io.ReaderAt              │ pread(fd, buf, n, off) │ No std (manual seek)     │
│ Zero-copy                │ io.Copy auto-optimiza    │ sendfile(2)            │ No en std                │
│ Composicion              │ Si (wrapping interfaces) │ Manual                 │ Si (wrapping traits)     │
│ Cierre automatico        │ No (defer manual)        │ No (manual)            │ Si (Drop trait)          │
│ Error en lectura parcial │ n > 0, err != nil        │ -1 + errno             │ Err(e)                   │
│ Error short write        │ io.ErrShortWrite         │ check return value     │ write vs write_all       │
└─────────────────────────┴──────────────────────────┴────────────────────────┴──────────────────────────┘
```

```
DIFERENCIA CLAVE: COMPOSICION

Go y Rust comparten la misma filosofia de composicion de I/O.
La diferencia principal:

Go: io.Reader es una INTERFACE (dynamic dispatch via vtable)
    → Puede combinar cualquier Reader con cualquier wrapper en runtime
    → Overhead de indirección (pequeño)

Rust: Read es un TRAIT (puede ser static dispatch con generics)
    → El compilador puede eliminar la indirección (monomorphization)
    → Zero-cost abstraction
    → Pero tipos concretos en firmas hacen el codigo mas verboso

C: No tiene interfaces ni traits
    → Composicion se hace con function pointers (manual, fragil)
    → O macros (no type-safe)
    → FILE* tiene algo de buffering pero no composicion

Go inspiro a Rust: los traits Read/Write de Rust se diseñaron
mirando io.Reader/io.Writer de Go como modelo.
```

---

## 12. Programa completo: File Processing Pipeline

Este programa demuestra un pipeline de I/O completo: lee un archivo de log, lo filtra, calcula estadisticas, genera un reporte comprimido, y reporta progreso — todo con composicion de Reader/Writer:

```go
package main

import (
    "bufio"
    "compress/gzip"
    "crypto/sha256"
    "encoding/json"
    "fmt"
    "io"
    "os"
    "strconv"
    "strings"
    "time"
)

// --- CountingReader/Writer ---

type CountingReader struct {
    r     io.Reader
    Count int64
}

func (cr *CountingReader) Read(p []byte) (int, error) {
    n, err := cr.r.Read(p)
    cr.Count += int64(n)
    return n, err
}

type CountingWriter struct {
    w     io.Writer
    Count int64
}

func (cw *CountingWriter) Write(p []byte) (int, error) {
    n, err := cw.w.Write(p)
    cw.Count += int64(n)
    return n, err
}

// --- Log Entry ---

type LogEntry struct {
    Timestamp string `json:"timestamp"`
    Level     string `json:"level"`
    Message   string `json:"message"`
    Source    string  `json:"source"`
}

type Report struct {
    GeneratedAt    string            `json:"generated_at"`
    InputFile      string            `json:"input_file"`
    InputBytes     int64             `json:"input_bytes"`
    OutputBytes    int64             `json:"output_bytes"`
    CompressionPct float64           `json:"compression_pct"`
    SHA256         string            `json:"sha256"`
    TotalLines     int               `json:"total_lines"`
    ErrorCount     int               `json:"error_count"`
    WarnCount      int               `json:"warn_count"`
    InfoCount      int               `json:"info_count"`
    Errors         []LogEntry        `json:"errors"`
    TopSources     map[string]int    `json:"top_sources"`
}

func main() {
    if len(os.Args) < 2 {
        fmt.Fprintln(os.Stderr, "usage: logpipeline <logfile>")
        fmt.Fprintln(os.Stderr, "  Reads a log file, filters errors, generates compressed JSON report")
        fmt.Fprintln(os.Stderr, "  Log format: TIMESTAMP LEVEL [SOURCE] message")
        os.Exit(1)
    }
    
    inputFile := os.Args[1]
    outputFile := inputFile + ".report.json.gz"
    
    // --- PIPELINE DE LECTURA ---
    // File → CountingReader → BufScanner
    
    f, err := os.Open(inputFile)
    if err != nil {
        fmt.Fprintf(os.Stderr, "open: %v\n", err)
        os.Exit(1)
    }
    defer f.Close()
    
    inputCounter := &CountingReader{r: f}
    scanner := bufio.NewScanner(inputCounter)
    scanner.Buffer(make([]byte, 256*1024), 256*1024) // Lineas hasta 256KB
    
    // --- PROCESAR LINEAS ---
    
    report := Report{
        InputFile:  inputFile,
        TopSources: make(map[string]int),
    }
    
    for scanner.Scan() {
        line := scanner.Text()
        report.TotalLines++
        
        entry := parseLine(line)
        report.TopSources[entry.Source]++
        
        switch entry.Level {
        case "ERROR":
            report.ErrorCount++
            if len(report.Errors) < 100 { // Max 100 errores en el reporte
                report.Errors = append(report.Errors, entry)
            }
        case "WARN":
            report.WarnCount++
        case "INFO":
            report.InfoCount++
        }
    }
    
    if err := scanner.Err(); err != nil {
        fmt.Fprintf(os.Stderr, "scan: %v\n", err)
        os.Exit(1)
    }
    
    report.InputBytes = inputCounter.Count
    report.GeneratedAt = time.Now().Format(time.RFC3339)
    
    // --- PIPELINE DE ESCRITURA ---
    // JsonEncoder → BufWriter → GzipWriter → MultiWriter(file, hash) → CountingWriter → File
    
    outFile, err := os.Create(outputFile)
    if err != nil {
        fmt.Fprintf(os.Stderr, "create: %v\n", err)
        os.Exit(1)
    }
    defer outFile.Close()
    
    outputCounter := &CountingWriter{w: outFile}
    
    hash := sha256.New()
    multi := io.MultiWriter(outputCounter, hash)
    
    gz, err := gzip.NewWriterLevel(multi, gzip.BestCompression)
    if err != nil {
        fmt.Fprintf(os.Stderr, "gzip: %v\n", err)
        os.Exit(1)
    }
    
    bw := bufio.NewWriter(gz)
    
    encoder := json.NewEncoder(bw)
    encoder.SetIndent("", "  ")
    if err := encoder.Encode(report); err != nil {
        fmt.Fprintf(os.Stderr, "encode: %v\n", err)
        os.Exit(1)
    }
    
    // Flush y close en orden inverso
    if err := bw.Flush(); err != nil {
        fmt.Fprintf(os.Stderr, "flush: %v\n", err)
        os.Exit(1)
    }
    if err := gz.Close(); err != nil {
        fmt.Fprintf(os.Stderr, "gzip close: %v\n", err)
        os.Exit(1)
    }
    
    // Actualizar report con datos finales
    report.OutputBytes = outputCounter.Count
    if report.InputBytes > 0 {
        report.CompressionPct = (1 - float64(report.OutputBytes)/float64(report.InputBytes)) * 100
    }
    report.SHA256 = fmt.Sprintf("%x", hash.Sum(nil))
    
    // --- REPORTE A STDOUT ---
    // Usando io.MultiWriter, strings.Builder, y fmt.Fprintf
    
    var summary strings.Builder
    
    fmt.Fprintf(&summary, "=== Log Pipeline Report ===\n\n")
    fmt.Fprintf(&summary, "Input:       %s (%s bytes)\n", inputFile, formatBytes(report.InputBytes))
    fmt.Fprintf(&summary, "Output:      %s (%s bytes)\n", outputFile, formatBytes(report.OutputBytes))
    fmt.Fprintf(&summary, "Compression: %.1f%%\n", report.CompressionPct)
    fmt.Fprintf(&summary, "SHA256:      %s\n\n", report.SHA256)
    fmt.Fprintf(&summary, "Lines:       %d\n", report.TotalLines)
    fmt.Fprintf(&summary, "Errors:      %d\n", report.ErrorCount)
    fmt.Fprintf(&summary, "Warnings:    %d\n", report.WarnCount)
    fmt.Fprintf(&summary, "Info:        %d\n\n", report.InfoCount)
    
    if len(report.TopSources) > 0 {
        fmt.Fprintf(&summary, "Top sources:\n")
        for source, count := range report.TopSources {
            fmt.Fprintf(&summary, "  %-20s %d\n", source, count)
        }
    }
    
    if report.ErrorCount > 0 {
        fmt.Fprintf(&summary, "\nFirst errors:\n")
        limit := 5
        if len(report.Errors) < limit {
            limit = len(report.Errors)
        }
        for _, e := range report.Errors[:limit] {
            fmt.Fprintf(&summary, "  [%s] %s: %s\n", e.Timestamp, e.Source, e.Message)
        }
    }
    
    // Escribir summary a stdout
    io.WriteString(os.Stdout, summary.String())
}

func parseLine(line string) LogEntry {
    entry := LogEntry{Level: "INFO", Source: "unknown"}
    
    // Formato esperado: "2024-01-15T10:30:00Z ERROR [auth] message here"
    parts := strings.SplitN(line, " ", 4)
    if len(parts) >= 1 {
        entry.Timestamp = parts[0]
    }
    if len(parts) >= 2 {
        entry.Level = strings.ToUpper(parts[1])
    }
    if len(parts) >= 3 {
        entry.Source = strings.Trim(parts[2], "[]")
    }
    if len(parts) >= 4 {
        entry.Message = parts[3]
    }
    
    return entry
}

func formatBytes(b int64) string {
    const unit = 1024
    if b < unit {
        return strconv.FormatInt(b, 10)
    }
    div, exp := int64(unit), 0
    for n := b / unit; n >= unit; n /= unit {
        div *= unit
        exp++
    }
    return fmt.Sprintf("%.1f %ciB", float64(b)/float64(div), "KMGTPE"[exp])
}
```

```
PIPELINE VISUAL DEL PROGRAMA:

LECTURA:
  Archivo → CountingReader → bufio.Scanner
                                 ↓
                           parseLine()
                                 ↓
                          Report struct

ESCRITURA:
  Report → json.Encoder → bufio.Writer → gzip.Writer → io.MultiWriter
                                                            ├→ CountingWriter → File
                                                            └→ SHA256 Hash

REPORTE:
  strings.Builder → fmt.Fprintf → io.WriteString → os.Stdout

CONCEPTOS DEMOSTRADOS:
  ✓ io.Reader (os.File, CountingReader)
  ✓ io.Writer (os.File, CountingWriter, sha256.Hash)
  ✓ io.MultiWriter (archivo + hash)
  ✓ bufio.Scanner (lectura por lineas)
  ✓ bufio.Writer (escritura buffered)
  ✓ gzip.Writer (compresion)
  ✓ json.Encoder (serializar a Writer)
  ✓ strings.Builder (como Writer para strings)
  ✓ io.WriteString (escribir string a Writer)
  ✓ Composicion: 6 capas de Writers apiladas
  ✓ Flush/Close en orden inverso
```

---

## 13. Ejercicios

### Ejercicio 1: Implementar io.Reader
Implementa un `FibonacciReader` que genera la secuencia de Fibonacci como texto (un numero por linea). Debe implementar `io.Reader`. Verificalo con:
```go
r := NewFibonacciReader(20) // 20 numeros
io.Copy(os.Stdout, r)
// Output: 0\n1\n1\n2\n3\n5\n8\n13\n21\n34\n...
```
Luego composicion con `io.LimitReader(r, 100)` para limitar a 100 bytes.

### Ejercicio 2: Pipeline de transformacion
Crea un programa que:
- Lee un archivo CSV desde stdin o argumento
- Filtra lineas donde la columna 3 sea mayor que un umbral (argumento)
- Escribe el resultado a stdout como JSON
- Simultaneamente calcula el SHA256 del output (usando `io.TeeReader` o `io.MultiWriter`)
- Imprime el hash al final en stderr
Todo sin cargar el archivo completo en memoria (streaming).

### Ejercicio 3: ProgressWriter
Implementa un `ProgressWriter` que envuelve un `io.Writer` y:
- Reporta progreso cada N bytes escritos (configurable)
- Muestra velocidad de escritura (bytes/segundo)
- Muestra tiempo transcurrido
Usalo con `io.Copy` para copiar un archivo grande mostrando progreso.

### Ejercicio 4: Composicion maxima
Escribe un programa que lea un archivo, y construya un pipeline con TODAS estas capas:
1. `os.File` (fuente)
2. `CountingReader` (contar bytes del disco)
3. `io.LimitReader` (maximo 10MB)
4. `gzip.NewReader` (descomprimir)
5. `io.TeeReader` + `sha256.New()` (hash del contenido descomprimido)
6. `bufio.Scanner` (leer por lineas)

Y una pipeline de escritura con:
1. `json.Encoder`
2. `bufio.Writer`
3. `gzip.Writer`
4. `io.MultiWriter` (archivo + `CountingWriter`)

Imprime al final: bytes leidos del disco, bytes descomprimidos (hash), lineas procesadas, bytes escritos al disco.

---

> **Siguiente**: C09/S02 - I/O, T02 - Archivos — os.Open, os.Create, os.ReadFile, os.WriteFile, bufio.Scanner, bufio.Writer
