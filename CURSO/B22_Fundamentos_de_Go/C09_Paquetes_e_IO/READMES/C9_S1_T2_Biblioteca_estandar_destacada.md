# Biblioteca estandar destacada — os, io, fmt, strings, strconv, filepath, encoding/json, net/http, log/slog

## 1. Introduccion

La biblioteca estandar de Go es una de sus mayores fortalezas. Mientras que en muchos lenguajes necesitas docenas de dependencias externas para hacer cualquier cosa util (frameworks web, parsers JSON, loggers, HTTP clients), en Go la stdlib cubre el 80-90% de las necesidades de un proyecto tipico. Puedes construir un servidor HTTP con JSON API, logging estructurado, manejo de archivos, y procesamiento de strings **sin una sola dependencia externa**.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                    STDLIB DE GO — TOUR RAPIDO                                   │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  PAQUETES QUE CUBRE ESTE TOPICO                                                │
│                                                                                  │
│  I/O Y SISTEMA                                                                  │
│  ├─ os         → archivos, env vars, procesos, stdin/stdout/stderr             │
│  ├─ io         → interfaces Reader/Writer, utilidades de I/O                   │
│  └─ filepath   → manipulacion de rutas cross-platform                          │
│                                                                                  │
│  TEXTO                                                                           │
│  ├─ fmt        → formateo y scanning de texto                                  │
│  ├─ strings    → manipulacion de strings                                       │
│  └─ strconv    → conversion entre strings y tipos numericos                    │
│                                                                                  │
│  DATOS                                                                           │
│  └─ encoding/json → serializar/deserializar JSON                               │
│                                                                                  │
│  RED                                                                             │
│  └─ net/http   → cliente y servidor HTTP completo                              │
│                                                                                  │
│  OBSERVABILIDAD                                                                  │
│  └─ log/slog   → logging estructurado (Go 1.21+)                              │
│                                                                                  │
│  OTROS PAQUETES IMPORTANTES (no cubiertos aqui)                                │
│  ├─ bytes, bufio       → buffers y I/O buffered                                │
│  ├─ regexp             → expresiones regulares                                 │
│  ├─ sort, slices, maps → ordenar y manipular colecciones                       │
│  ├─ time               → fechas, duraciones, timers                            │
│  ├─ math, math/rand    → matematicas y numeros aleatorios                      │
│  ├─ crypto/*           → hash, cifrado, TLS                                   │
│  ├─ database/sql       → interfaz de bases de datos                            │
│  ├─ html/template      → templates HTML seguros                                │
│  ├─ testing            → framework de testing integrado                        │
│  ├─ sync, sync/atomic  → primitivas de concurrencia                            │
│  └─ context            → cancelacion y propagacion de valores                  │
│                                                                                  │
│  FILOSOFIA:                                                                      │
│  "A little copying is better than a little dependency"                          │
│  La stdlib es estable, bien documentada, y no introduce dependency hell.       │
│  Prefiere stdlib sobre crates externos a menos que necesites algo especifico.  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Package os — interfaz con el sistema operativo

### 2.1 Archivos: abrir, crear, leer, escribir

```go
package main

import (
    "fmt"
    "os"
)

func main() {
    // --- CREAR Y ESCRIBIR ---
    
    // os.Create: crea o trunca un archivo (permisos 0666 antes de umask)
    f, err := os.Create("output.txt")
    if err != nil {
        fmt.Fprintf(os.Stderr, "create: %v\n", err)
        os.Exit(1)
    }
    
    _, err = f.WriteString("linea 1\nlinea 2\nlinea 3\n")
    if err != nil {
        f.Close()
        fmt.Fprintf(os.Stderr, "write: %v\n", err)
        os.Exit(1)
    }
    f.Close() // Siempre cerrar despues de escribir
    
    // --- ABRIR Y LEER ---
    
    // os.Open: abre para SOLO LECTURA (O_RDONLY)
    f, err = os.Open("output.txt")
    if err != nil {
        fmt.Fprintf(os.Stderr, "open: %v\n", err)
        os.Exit(1)
    }
    defer f.Close()
    
    buf := make([]byte, 1024)
    n, err := f.Read(buf)
    if err != nil {
        fmt.Fprintf(os.Stderr, "read: %v\n", err)
        os.Exit(1)
    }
    fmt.Printf("Leidos %d bytes:\n%s\n", n, buf[:n])
    
    // --- ABRIR CON FLAGS ESPECIFICOS ---
    
    // os.OpenFile: control total sobre modo y permisos
    f2, err := os.OpenFile("log.txt",
        os.O_CREATE|os.O_WRONLY|os.O_APPEND, // Crear si no existe, solo escritura, append
        0o644,                                 // Permisos: rw-r--r--
    )
    if err != nil {
        fmt.Fprintf(os.Stderr, "openfile: %v\n", err)
        os.Exit(1)
    }
    defer f2.Close()
    
    f2.WriteString("nueva linea al final\n")
    
    // --- OPERACIONES RAPIDAS (leer/escribir de un golpe) ---
    
    // os.ReadFile: lee todo el archivo en memoria (Go 1.16+)
    data, err := os.ReadFile("output.txt")
    if err != nil {
        fmt.Fprintf(os.Stderr, "readfile: %v\n", err)
        os.Exit(1)
    }
    fmt.Printf("Contenido completo:\n%s\n", data)
    
    // os.WriteFile: escribe todo el archivo de un golpe (Go 1.16+)
    err = os.WriteFile("quick.txt", []byte("contenido rapido\n"), 0o644)
    if err != nil {
        fmt.Fprintf(os.Stderr, "writefile: %v\n", err)
        os.Exit(1)
    }
}
```

### 2.2 Flags de os.OpenFile

```
FLAGS DE APERTURA (combinables con |)

os.O_RDONLY    → solo lectura (default de os.Open)
os.O_WRONLY    → solo escritura
os.O_RDWR      → lectura y escritura
os.O_CREATE    → crear si no existe
os.O_TRUNC     → truncar a tamaño 0 si existe (default de os.Create)
os.O_APPEND    → escribir al final del archivo
os.O_EXCL      → error si el archivo ya existe (con O_CREATE)
os.O_SYNC      → I/O sincronico (flush despues de cada write)

COMBINACIONES COMUNES:

os.Open("f")                                    → O_RDONLY
os.Create("f")                                  → O_RDWR|O_CREATE|O_TRUNC, 0666
os.OpenFile("f", O_WRONLY|O_CREATE|O_APPEND, 0644) → log file (append)
os.OpenFile("f", O_WRONLY|O_CREATE|O_EXCL, 0600)   → crear NUEVO (falla si existe)
os.OpenFile("f", O_RDWR|O_CREATE, 0644)            → leer y escribir
```

### 2.3 Informacion de archivos y directorios

```go
// os.Stat: obtener informacion de un archivo
info, err := os.Stat("output.txt")
if err != nil {
    if os.IsNotExist(err) {
        fmt.Println("archivo no existe")
    } else {
        fmt.Println("error:", err)
    }
    return
}

fmt.Println("Nombre:", info.Name())      // "output.txt"
fmt.Println("Tamaño:", info.Size())      // en bytes
fmt.Println("Permisos:", info.Mode())    // ej: -rw-r--r--
fmt.Println("Modificado:", info.ModTime()) // time.Time
fmt.Println("Es directorio:", info.IsDir())

// os.Lstat: como Stat pero NO sigue symlinks
linkInfo, _ := os.Lstat("mylink")
fmt.Println("Es symlink:", linkInfo.Mode()&os.ModeSymlink != 0)
```

### 2.4 Operaciones en el filesystem

```go
// Crear directorio
os.Mkdir("mydir", 0o755)           // Un solo nivel
os.MkdirAll("a/b/c/d", 0o755)     // Crea toda la jerarquia

// Renombrar/mover
os.Rename("old.txt", "new.txt")
os.Rename("file.txt", "subdir/file.txt") // Mover

// Eliminar
os.Remove("file.txt")              // Un archivo o directorio VACIO
os.RemoveAll("directory/")         // Recursivo (como rm -rf)

// Symlinks
os.Symlink("target.txt", "link.txt")
target, _ := os.Readlink("link.txt") // → "target.txt"

// Directorio temporal
tmpDir, err := os.MkdirTemp("", "myapp-*")
// tmpDir = "/tmp/myapp-a1b2c3d4" (patron * reemplazado)
defer os.RemoveAll(tmpDir)

// Archivo temporal
tmpFile, err := os.CreateTemp("", "data-*.json")
// tmpFile.Name() = "/tmp/data-x7y8z9.json"
defer os.Remove(tmpFile.Name())
defer tmpFile.Close()

// Permisos
os.Chmod("file.txt", 0o755)
os.Chown("file.txt", uid, gid)

// Directorio actual
wd, _ := os.Getwd()
fmt.Println("CWD:", wd)

os.Chdir("/tmp") // Cambiar directorio actual (afecta todo el proceso)
```

### 2.5 Environment variables

```go
// Leer
home := os.Getenv("HOME")        // "" si no existe
port := os.Getenv("PORT")

// Leer con deteccion de existencia
val, exists := os.LookupEnv("API_KEY")
if !exists {
    fmt.Println("API_KEY no esta definida")
} else if val == "" {
    fmt.Println("API_KEY esta definida pero vacia")
}

// Escribir (afecta solo este proceso y sus hijos)
os.Setenv("MY_VAR", "value")
os.Unsetenv("OLD_VAR")

// Listar todas
for _, env := range os.Environ() {
    fmt.Println(env) // "KEY=VALUE"
}

// Patron comun: env var con default
func getEnvDefault(key, defaultVal string) string {
    if v := os.Getenv(key); v != "" {
        return v
    }
    return defaultVal
}

port = getEnvDefault("PORT", "8080")
```

### 2.6 Proceso y stdin/stdout/stderr

```go
// Argumentos de linea de comandos
fmt.Println("Programa:", os.Args[0])      // Nombre del ejecutable
fmt.Println("Args:", os.Args[1:])         // Argumentos del usuario

// Streams estandar (son *os.File — implementan io.Reader/io.Writer)
os.Stdin   // Lectura desde terminal o pipe
os.Stdout  // Salida estandar
os.Stderr  // Salida de errores

fmt.Fprintln(os.Stdout, "mensaje normal")
fmt.Fprintln(os.Stderr, "mensaje de error")

// Exit
os.Exit(0)  // Exito (defer NO se ejecuta!)
os.Exit(1)  // Error

// PID y hostname
fmt.Println("PID:", os.Getpid())
hostname, _ := os.Hostname()
fmt.Println("Host:", hostname)

// Ejecutar comandos del sistema → usa os/exec (no os directamente)
```

### 2.7 os.DirEntry y os.ReadDir

```go
// os.ReadDir: listar un directorio (Go 1.16+, reemplaza ioutil.ReadDir)
entries, err := os.ReadDir(".")
if err != nil {
    log.Fatal(err)
}

for _, entry := range entries {
    fmt.Printf("%-20s dir=%v\n", entry.Name(), entry.IsDir())
    
    // Si necesitas mas info:
    info, err := entry.Info() // Llama Stat solo cuando lo necesitas (lazy)
    if err == nil {
        fmt.Printf("  size=%d mode=%s\n", info.Size(), info.Mode())
    }
}
```

---

## 3. Package io — interfaces fundamentales de I/O

### 3.1 Las interfaces centrales

`io` define las interfaces mas importantes de Go. Casi todo el I/O en Go se construye sobre `io.Reader` e `io.Writer`:

```go
// io.Reader — la interfaz mas importante de Go
type Reader interface {
    Read(p []byte) (n int, err error)
    // Lee hasta len(p) bytes en p.
    // Retorna el numero de bytes leidos y un error.
    // Al final del stream, retorna io.EOF.
}

// io.Writer — la segunda interfaz mas importante
type Writer interface {
    Write(p []byte) (n int, err error)
    // Escribe len(p) bytes desde p.
    // Retorna el numero de bytes escritos y un error.
    // Si n < len(p), err debe ser != nil.
}

// io.Closer — para recursos que necesitan liberarse
type Closer interface {
    Close() error
}

// Combinaciones comunes:
// io.ReadCloser  = Reader + Closer (ej: http.Response.Body)
// io.WriteCloser = Writer + Closer (ej: os.File para escritura)
// io.ReadWriter  = Reader + Writer (ej: os.File con O_RDWR)
// io.ReadWriteCloser = Reader + Writer + Closer
```

### 3.2 Quien implementa Reader y Writer

```
IMPLEMENTACIONES DE io.Reader:
  os.File              → leer archivos
  strings.Reader       → leer desde un string
  bytes.Reader         → leer desde []byte
  bytes.Buffer         → buffer en memoria (tambien Writer)
  bufio.Reader         → reader con buffer (mejora rendimiento)
  http.Request.Body    → body de request HTTP
  http.Response.Body   → body de response HTTP
  net.Conn             → conexion de red
  gzip.Reader          → descomprimir gzip
  json.Decoder         → stream de JSON
  os.Stdin             → input estandar
  
IMPLEMENTACIONES DE io.Writer:
  os.File              → escribir archivos
  bytes.Buffer         → buffer en memoria (tambien Reader)
  bufio.Writer         → writer con buffer
  http.ResponseWriter  → response HTTP
  net.Conn             → conexion de red
  gzip.Writer          → comprimir gzip
  json.Encoder         → stream de JSON
  os.Stdout, os.Stderr → output estandar
  hash.Hash            → calcular hash (MD5, SHA, etc.)
  tabwriter.Writer     → tabular texto
```

### 3.3 Utilidades de io

```go
import "io"

// io.Copy: copiar de Reader a Writer (el cuchillo suizo del I/O)
// Copia hasta EOF, usando un buffer interno de 32KB
n, err := io.Copy(dst, src) // dst: Writer, src: Reader

// Ejemplo: copiar archivo
src, _ := os.Open("input.txt")
dst, _ := os.Create("output.txt")
io.Copy(dst, src) // Eficiente — usa sendfile() en Linux si es posible
src.Close()
dst.Close()

// Ejemplo: descargar archivo
resp, _ := http.Get("https://example.com/file.zip")
f, _ := os.Create("file.zip")
io.Copy(f, resp.Body) // Copia streaming — no carga todo en memoria
resp.Body.Close()
f.Close()

// io.ReadAll: leer TODO de un Reader a []byte (cuidado con archivos grandes!)
data, err := io.ReadAll(resp.Body) // Antes era ioutil.ReadAll

// io.TeeReader: leer de un Reader y simultáneamente escribir a un Writer
// (como el comando tee de Unix)
body := io.TeeReader(resp.Body, os.Stdout) // Imprime mientras lee
data, _ := io.ReadAll(body) // data tiene todo, Y se imprimio a stdout

// io.LimitReader: limitar cuantos bytes se leen
limited := io.LimitReader(resp.Body, 1024*1024) // Max 1MB
data, _ := io.ReadAll(limited)

// io.MultiReader: concatenar multiples Readers en secuencia
header := strings.NewReader("HEADER\n")
body := strings.NewReader("BODY\n")
footer := strings.NewReader("FOOTER\n")
combined := io.MultiReader(header, body, footer)
io.Copy(os.Stdout, combined) // HEADER\nBODY\nFOOTER\n

// io.MultiWriter: escribir a multiples Writers simultaneamente
logFile, _ := os.Create("app.log")
multi := io.MultiWriter(os.Stdout, logFile)
fmt.Fprintln(multi, "este mensaje va a stdout Y al archivo")

// io.Pipe: par Reader/Writer conectados (como Unix pipe)
pr, pw := io.Pipe()
go func() {
    fmt.Fprintln(pw, "datos desde goroutine")
    pw.Close()
}()
data, _ := io.ReadAll(pr)
fmt.Println(string(data)) // "datos desde goroutine\n"

// io.NopCloser: envolver un Reader como ReadCloser (Close no hace nada)
rc := io.NopCloser(strings.NewReader("data"))
// util cuando una funcion requiere ReadCloser pero tienes solo Reader

// io.Discard: Writer que descarta todo (como /dev/null)
io.Copy(io.Discard, resp.Body) // Consumir body sin usarlo
```

### 3.4 Patron de composicion de I/O

```go
// El poder de io.Reader/io.Writer: se componen como pipes de Unix
// Ejemplo: leer archivo gzip, decodificar JSON, logging

func processCompressedJSON(filename string) ([]Record, error) {
    // 1. Abrir archivo
    f, err := os.Open(filename)
    if err != nil {
        return nil, err
    }
    defer f.Close()
    
    // 2. Wrappear con descompresion gzip
    gz, err := gzip.NewReader(f)
    if err != nil {
        return nil, err
    }
    defer gz.Close()
    
    // 3. (Opcional) Tee para logging
    logged := io.TeeReader(gz, os.Stderr) // Ver los datos mientras se procesan
    
    // 4. Decodificar JSON
    var records []Record
    if err := json.NewDecoder(logged).Decode(&records); err != nil {
        return nil, err
    }
    
    return records, nil
}

// Pipeline conceptual:
// File → GzipReader → TeeReader(stderr) → JsonDecoder → []Record
// Cada capa es un io.Reader que envuelve al anterior
// Todo es streaming — no se carga el archivo entero en memoria
```

---

## 4. Package fmt — formateo de texto

### 4.1 Funciones de impresion

```go
import "fmt"

// --- FAMILIA Print: escriben a stdout ---
fmt.Print("sin newline")                     // Sin newline al final
fmt.Println("con newline")                   // Agrega \n al final
fmt.Printf("nombre: %s, edad: %d\n", "Ana", 30) // Formato C-style

// --- FAMILIA Fprint: escriben a un io.Writer ---
fmt.Fprint(os.Stderr, "error message")       // A stderr
fmt.Fprintf(w, "status: %d\n", 200)          // A http.ResponseWriter
fmt.Fprintln(logFile, "log entry")           // A un archivo

// --- FAMILIA Sprint: retornan string ---
s := fmt.Sprint("hola", " ", "mundo")        // "hola mundo"
s = fmt.Sprintf("user_%d", 42)               // "user_42"
s = fmt.Sprintln("con newline")              // "con newline\n"
```

### 4.2 Verbos de formato

```
VERBOS DE FORMATO (los mas usados)

GENERAL
  %v     → valor en formato por defecto
  %+v    → structs: incluye nombres de campos
  %#v    → sintaxis Go del valor (ej: main.User{Name:"Ana", Age:30})
  %T     → tipo del valor (ej: main.User)
  %%     → literal %

ENTEROS
  %d     → decimal (base 10)
  %b     → binario
  %o     → octal
  %O     → octal con prefijo 0o
  %x     → hexadecimal minusculas
  %X     → hexadecimal mayusculas
  %c     → caracter Unicode correspondiente
  %U     → formato Unicode (U+0041)

FLOATS
  %f     → notacion decimal (ej: 3.141593)
  %e     → notacion cientifica (ej: 3.141593e+00)
  %g     → la mas compacta de %e o %f
  %.2f   → 2 decimales (ej: 3.14)
  %9.2f  → 9 caracteres ancho, 2 decimales (ej: "     3.14")

STRINGS Y BYTES
  %s     → string sin formato
  %q     → string con comillas y caracteres escapados ("hola\n")
  %x     → hex dump (ej: 68656c6c6f)

BOOLEANOS
  %t     → true o false

PUNTEROS
  %p     → direccion de memoria en hex (0xc0000b4000)

ANCHO Y ALINEACION
  %10d   → alinear a la derecha, 10 caracteres
  %-10d  → alinear a la izquierda, 10 caracteres
  %010d  → rellenar con ceros, 10 caracteres
  %+d    → mostrar signo siempre (+42, -42)
```

```go
// Ejemplos practicos
type User struct {
    Name string
    Age  int
}

u := User{Name: "Alice", Age: 30}

fmt.Printf("%v\n", u)       // {Alice 30}
fmt.Printf("%+v\n", u)      // {Name:Alice Age:30}
fmt.Printf("%#v\n", u)      // main.User{Name:"Alice", Age:30}
fmt.Printf("%T\n", u)       // main.User

fmt.Printf("%d %b %o %x\n", 42, 42, 42, 42)  // 42 101010 52 2a

fmt.Printf("%.2f\n", 3.14159)     // 3.14
fmt.Printf("%9.2f\n", 3.14159)    //      3.14
fmt.Printf("%09.2f\n", 3.14159)   // 000003.14

fmt.Printf("%q\n", "hello\nworld")  // "hello\nworld"
fmt.Printf("%x\n", "hello")         // 68656c6c6f

// Tabla alineada
for _, item := range items {
    fmt.Printf("%-20s %8.2f %5d\n", item.Name, item.Price, item.Qty)
}
```

### 4.3 La interface fmt.Stringer

Si tu tipo implementa `String() string`, `fmt` lo usa automaticamente:

```go
type Status int

const (
    Active Status = iota
    Inactive
    Suspended
)

func (s Status) String() string {
    switch s {
    case Active:
        return "active"
    case Inactive:
        return "inactive"
    case Suspended:
        return "suspended"
    default:
        return fmt.Sprintf("unknown(%d)", int(s))
    }
}

// Ahora:
fmt.Println(Active)             // "active" (no "0")
fmt.Printf("status: %v\n", Suspended)  // "status: suspended"
```

### 4.4 fmt.Errorf: crear errores con formato

```go
// Crear error con formato
err := fmt.Errorf("failed to connect to %s:%d", host, port)

// Wrapping de errores (Go 1.13+) — %w envuelve el error original
err = fmt.Errorf("database query failed: %w", originalErr)
// errors.Is(err, originalErr) → true
// errors.Unwrap(err) → originalErr
```

### 4.5 Scanning: leer input formateado

```go
// fmt.Scan: leer desde stdin (separado por espacios)
var name string
var age int
fmt.Print("Nombre y edad: ")
fmt.Scan(&name, &age) // Input: "Alice 30"

// fmt.Scanf: leer con formato
fmt.Scanf("%s %d", &name, &age)

// fmt.Scanln: leer una linea completa
fmt.Scanln(&name) // Lee hasta \n

// fmt.Sscan: leer desde un string
fmt.Sscanf("2024-01-15", "%d-%d-%d", &year, &month, &day)

// fmt.Fscan: leer desde un io.Reader
fmt.Fscan(file, &value)
```

---

## 5. Package strings — manipulacion de strings

### 5.1 Busqueda y comparacion

```go
import "strings"

s := "Hello, World! Hello, Go!"

// Contiene
strings.Contains(s, "World")         // true
strings.ContainsAny(s, "aeiou")      // true (contiene 'e', 'o')
strings.ContainsRune(s, 'W')         // true

// Prefijo y sufijo
strings.HasPrefix(s, "Hello")        // true
strings.HasSuffix(s, "Go!")          // true

// Posicion
strings.Index(s, "World")            // 7
strings.LastIndex(s, "Hello")        // 14
strings.IndexByte(s, 'W')            // 7
strings.IndexRune(s, 'W')            // 7
strings.IndexAny(s, "xyz!")          // 12 (primera '!')

// Conteo
strings.Count(s, "Hello")            // 2
strings.Count("aaaa", "aa")          // 2 (no solapadas)

// Comparacion case-insensitive
strings.EqualFold("hello", "HELLO")  // true
strings.EqualFold("straße", "STRASSE") // true (Unicode-aware)
```

### 5.2 Transformacion

```go
// Mayusculas / minusculas
strings.ToUpper("hello")             // "HELLO"
strings.ToLower("HELLO")             // "hello"
strings.Title("hello world")         // Deprecated — usar cases.Title
strings.ToTitle("hello world")       // "HELLO WORLD" (Unicode ToTitle)

// Recortar
strings.TrimSpace("  hello  ")       // "hello"
strings.Trim("***hello***", "*")     // "hello"
strings.TrimLeft("***hello***", "*") // "hello***"
strings.TrimRight("***hello***", "*")// "***hello"
strings.TrimPrefix("Mr. Smith", "Mr. ") // "Smith"
strings.TrimSuffix("file.txt", ".txt")  // "file"

// Reemplazo
strings.Replace("aaa", "a", "b", 2)    // "bba" (reemplazar 2 ocurrencias)
strings.Replace("aaa", "a", "b", -1)   // "bbb" (todas las ocurrencias)
strings.ReplaceAll("aaa", "a", "b")    // "bbb" (shortcut para -1)

// Repetir
strings.Repeat("ab", 3)              // "ababab"
strings.Repeat("-", 40)              // "----------------------------------------"

// Map: aplicar funcion a cada rune
strings.Map(func(r rune) rune {
    if r == 'a' { return 'A' }
    return r
}, "banana")                          // "bAnAnA"
```

### 5.3 Split y Join

```go
// Split
strings.Split("a,b,c,d", ",")          // ["a", "b", "c", "d"]
strings.SplitN("a,b,c,d", ",", 2)      // ["a", "b,c,d"] (max 2 partes)
strings.SplitAfter("a,b,c", ",")       // ["a,", "b,", "c"] (mantiene delimitador)

// Fields: split por whitespace (uno o mas espacios/tabs/newlines)
strings.Fields("  hello   world  ")    // ["hello", "world"]

// FieldsFunc: split por funcion custom
strings.FieldsFunc("a1b2c3", func(r rune) bool {
    return r >= '0' && r <= '9'
})  // ["a", "b", "c"]

// Join
strings.Join([]string{"a", "b", "c"}, ", ")  // "a, b, c"
strings.Join(parts, "/")                      // "path/to/file"
```

### 5.4 strings.Builder — concatenacion eficiente

```go
// NUNCA concatenar strings en un loop con + (O(n²)):
// s := ""
// for _, item := range items { s += item.String() + "\n" }  // MAL!

// USAR strings.Builder (O(n)):
var b strings.Builder

b.WriteString("Hello")
b.WriteString(", ")
b.WriteString("World!")
b.WriteByte('\n')
b.WriteRune('✓')

result := b.String() // "Hello, World!\n✓"
fmt.Println(b.Len()) // 17

// Preallocar si conoces el tamaño aproximado:
b.Grow(1024) // Reservar espacio para evitar realocaciones

// Builder con fmt:
var b2 strings.Builder
for i, name := range names {
    fmt.Fprintf(&b2, "%d. %s\n", i+1, name) // Builder implementa io.Writer
}
report := b2.String()
```

### 5.5 strings.Replacer — reemplazos multiples eficientes

```go
// Reemplazar multiples pares en una pasada
r := strings.NewReplacer(
    "&", "&amp;",
    "<", "&lt;",
    ">", "&gt;",
    `"`, "&quot;",
)

safe := r.Replace(`<script>alert("xss")</script>`)
// "&lt;script&gt;alert(&quot;xss&quot;)&lt;/script&gt;"

// Replacer es seguro para uso concurrente y reusable
// Mucho mas eficiente que multiples Replace() encadenados

// Tambien puede escribir a un Writer directamente:
r.WriteString(os.Stdout, `Hello <World>`)
```

### 5.6 strings.NewReader

```go
// Crear un io.Reader desde un string
// Util cuando una funcion espera io.Reader pero tienes un string
reader := strings.NewReader("contenido del fake file")

// Implementa: io.Reader, io.ReaderAt, io.Seeker, io.WriterTo, io.ByteScanner, io.RuneScanner
n, _ := io.Copy(os.Stdout, reader) // Escribe a stdout
```

---

## 6. Package strconv — conversion string ↔ tipos numericos

### 6.1 Conversiones basicas

```go
import "strconv"

// --- STRING → NUMERO ---

// Atoi: string → int (atajo de ParseInt(s, 10, 0))
n, err := strconv.Atoi("42")      // n = 42, err = nil
n, err = strconv.Atoi("abc")      // n = 0, err = *NumError

// ParseInt: string → int64 con base y tamaño
n64, err := strconv.ParseInt("42", 10, 64)    // base 10, int64
n64, err = strconv.ParseInt("ff", 16, 64)     // base 16 → 255
n64, err = strconv.ParseInt("101010", 2, 64)  // base 2 → 42
n64, err = strconv.ParseInt("0xff", 0, 64)    // base 0 = auto-detect por prefijo

// ParseUint: string → uint64
u64, err := strconv.ParseUint("42", 10, 64)

// ParseFloat: string → float64
f, err := strconv.ParseFloat("3.14159", 64)   // 64 = float64
f, err = strconv.ParseFloat("1.23e4", 64)     // 12300
f, err = strconv.ParseFloat("NaN", 64)        // math.NaN()
f, err = strconv.ParseFloat("Inf", 64)        // math.Inf(1)

// ParseBool: string → bool
b, err := strconv.ParseBool("true")   // true
b, err = strconv.ParseBool("1")       // true
b, err = strconv.ParseBool("T")       // true
b, err = strconv.ParseBool("false")   // false
b, err = strconv.ParseBool("0")       // false
b, err = strconv.ParseBool("F")       // false

// --- NUMERO → STRING ---

// Itoa: int → string (atajo de FormatInt(int64(i), 10))
s := strconv.Itoa(42)                 // "42"

// FormatInt: int64 → string con base
s = strconv.FormatInt(42, 10)         // "42"
s = strconv.FormatInt(42, 16)         // "2a"
s = strconv.FormatInt(42, 2)          // "101010"
s = strconv.FormatInt(42, 8)          // "52"

// FormatFloat: float64 → string
s = strconv.FormatFloat(3.14159, 'f', 2, 64)    // "3.14" (2 decimales)
s = strconv.FormatFloat(3.14159, 'e', 4, 64)    // "3.1416e+00"
s = strconv.FormatFloat(3.14159, 'g', -1, 64)   // "3.14159" (compacto)
// Formatos: 'f' decimal, 'e' cientifico, 'g' compacto, 'b' binario

// FormatBool: bool → string
s = strconv.FormatBool(true)          // "true"

// FormatUint: uint64 → string
s = strconv.FormatUint(42, 10)        // "42"
```

### 6.2 Append (para rendimiento)

```go
// Las funciones Append* escriben directamente en un []byte
// Evitan crear strings intermedios — ideal para hot paths

buf := make([]byte, 0, 64)
buf = strconv.AppendInt(buf, 42, 10)           // buf = "42"
buf = append(buf, ',')
buf = strconv.AppendFloat(buf, 3.14, 'f', 2, 64) // buf = "42,3.14"
buf = append(buf, ',')
buf = strconv.AppendBool(buf, true)            // buf = "42,3.14,true"
buf = append(buf, ',')
buf = strconv.AppendQuote(buf, "hello")        // buf = `42,3.14,true,"hello"`
```

### 6.3 Quote y Unquote

```go
// Quote: agregar comillas Go y escapar caracteres especiales
s := strconv.Quote(`hello "world"`)    // `"hello \"world\""`
s = strconv.Quote("tab\there")         // `"tab\there"`
s = strconv.Quote("café")              // `"café"` o `"caf\u00e9"`

// QuoteToASCII: como Quote pero escapa todo non-ASCII
s = strconv.QuoteToASCII("café")       // `"caf\u00e9"`

// Unquote: inverso de Quote
original, err := strconv.Unquote(`"hello \"world\""`)
// original = `hello "world"`

// QuoteRune / QuoteRuneToASCII
s = strconv.QuoteRune('A')             // "'A'"
s = strconv.QuoteRune('☺')             // "'☺'"
```

### 6.4 strconv vs fmt.Sprintf — cuando usar cual

```
strconv vs fmt.Sprintf — RENDIMIENTO

strconv.Itoa(42)             → ~3x mas rapido que fmt.Sprintf("%d", 42)
strconv.FormatFloat(...)     → ~5x mas rapido que fmt.Sprintf("%f", ...)

Regla:
  - Para conversion pura (numero ↔ string): usa strconv
  - Para formato complejo con multiples valores: usa fmt.Sprintf
  - Para hot paths (millones de operaciones): siempre strconv + Append*

// strconv: rapido, una conversion a la vez
id := strconv.Itoa(user.ID)

// fmt.Sprintf: flexible, multiples valores con formato
msg := fmt.Sprintf("user %d (%s) logged in at %s", user.ID, user.Name, time.Now())
```

---

## 7. Package filepath — rutas de archivos cross-platform

### 7.1 Manipulacion de rutas

```go
import "path/filepath"

// IMPORTANTE: filepath usa el separador del OS (\\ en Windows, / en Linux/macOS)
// Para URLs o paths que siempre son /, usa "path" (no "path/filepath")

// Join: unir segmentos con el separador correcto
p := filepath.Join("home", "user", "documents", "file.txt")
// Linux:   "home/user/documents/file.txt"
// Windows: "home\\user\\documents\\file.txt"

// Clean: normalizar ruta
filepath.Clean("/a/b/../c/./d")      // "/a/c/d"
filepath.Clean("///a//b")            // "/a/b"

// Split: separar directorio y archivo
dir, file := filepath.Split("/home/user/file.txt")
// dir = "/home/user/", file = "file.txt"

// Dir y Base: como dirname y basename de Unix
filepath.Dir("/home/user/file.txt")   // "/home/user"
filepath.Base("/home/user/file.txt")  // "file.txt"

// Ext: extension del archivo
filepath.Ext("photo.jpg")            // ".jpg"
filepath.Ext("archive.tar.gz")       // ".gz" (solo la ultima)
filepath.Ext("Makefile")             // "" (sin extension)

// Abs: convertir a ruta absoluta
abs, err := filepath.Abs("./relative/path")
// ej: "/home/user/project/relative/path"

// Rel: ruta relativa de base a target
rel, err := filepath.Rel("/home/user", "/home/user/docs/file.txt")
// rel = "docs/file.txt"

// Separador del OS
fmt.Println(string(filepath.Separator))  // "/" en Linux, "\\" en Windows
fmt.Println(string(filepath.ListSeparator)) // ":" en Linux, ";" en Windows

// Match: glob matching
matched, _ := filepath.Match("*.txt", "readme.txt")     // true
matched, _ = filepath.Match("test_*.go", "test_main.go") // true
matched, _ = filepath.Match("data/[0-9]*", "data/42")    // true
```

### 7.2 Recorrer directorios

```go
// filepath.WalkDir: recorrer arbol de directorios recursivamente (Go 1.16+)
// Mas eficiente que filepath.Walk (no llama Stat en cada entry)
err := filepath.WalkDir("/home/user/project", func(path string, d fs.DirEntry, err error) error {
    if err != nil {
        return err // Error accediendo al path
    }
    
    // Saltar directorios ocultos
    if d.IsDir() && strings.HasPrefix(d.Name(), ".") {
        return filepath.SkipDir // No entrar en este directorio
    }
    
    // Procesar solo archivos .go
    if !d.IsDir() && filepath.Ext(path) == ".go" {
        fmt.Println(path)
    }
    
    return nil
})

// filepath.Glob: encontrar archivos con patron
matches, err := filepath.Glob("/home/user/*.txt")
// matches = ["/home/user/readme.txt", "/home/user/notes.txt"]

matches, _ = filepath.Glob("src/**/*.go") // NOTA: ** NO funciona en filepath.Glob
// filepath.Glob solo soporta * y ? y [...], NO ** recursivo
// Para ** recursivo, usa WalkDir con un filtro
```

### 7.3 filepath vs path

```
filepath vs path — CUAL USAR

filepath (path/filepath):
  - Para rutas del SISTEMA DE ARCHIVOS
  - Usa el separador del OS (/ o \\)
  - Funciones como Abs(), Rel(), WalkDir()
  - SIEMPRE para archivos locales

path:
  - Para rutas LOGICAS (URLs, keys de storage, etc.)
  - SIEMPRE usa / como separador
  - Subset de funciones: Join, Split, Base, Dir, Ext, Clean, Match
  - NUNCA para archivos del sistema local

// Regla simple:
// ¿Es un archivo en disco? → filepath
// ¿Es una URL o path logico? → path
```

---

## 8. Package encoding/json — serializar y deserializar JSON

### 8.1 Marshal: struct → JSON

```go
import "encoding/json"

type User struct {
    ID        int       `json:"id"`
    Name      string    `json:"name"`
    Email     string    `json:"email"`
    Age       int       `json:"age,omitempty"`     // Omitir si es zero-value
    Password  string    `json:"-"`                 // NUNCA incluir en JSON
    IsAdmin   bool      `json:"is_admin"`
    CreatedAt time.Time `json:"created_at"`
    Metadata  any       `json:"metadata,omitempty"` // any = interface{}
}

func main() {
    u := User{
        ID:       1,
        Name:     "Alice",
        Email:    "alice@example.com",
        Age:      0, // Se omite por omitempty
        Password: "secret", // Se ignora por json:"-"
        IsAdmin:  false,
        CreatedAt: time.Now(),
    }
    
    // Marshal: struct → []byte (JSON compacto)
    data, err := json.Marshal(u)
    if err != nil {
        log.Fatal(err)
    }
    fmt.Println(string(data))
    // {"id":1,"name":"Alice","email":"alice@example.com","is_admin":false,"created_at":"2024-01-15T10:30:00Z"}
    // Nota: age omitido (zero-value + omitempty), password omitido (json:"-")
    
    // MarshalIndent: JSON formateado (para debug, config files, etc.)
    pretty, err := json.MarshalIndent(u, "", "  ")
    fmt.Println(string(pretty))
    // {
    //   "id": 1,
    //   "name": "Alice",
    //   ...
    // }
}
```

### 8.2 Struct tags de JSON

```
JSON STRUCT TAGS

Tag                          Efecto
──────────────────────────────────────────────────────────────────────
`json:"name"`                Renombrar el campo en JSON
`json:"name,omitempty"`      Omitir si el valor es zero-value
`json:"-"`                   Nunca incluir en JSON
`json:"-,"`                  Usar "-" como nombre literal
`json:",omitempty"`          Mantener nombre del campo, omitir si zero
`json:",string"`             Codificar numero/bool como JSON string
                             (util para APIs JavaScript con int64)

ZERO-VALUES QUE OMITEMPTY OMITE:
  false para bool
  0 para int, float
  "" para string
  nil para pointer, slice, map, interface
  time.Time{} para time.Time (pero cuidado: "0001-01-01T00:00:00Z")

IMPORTANTE: omitempty NO omite:
  - Struct vacio (a menos que sea puntero nil)
  - Array de tamaño fijo con zero-values
```

### 8.3 Unmarshal: JSON → struct

```go
// Unmarshal: []byte (JSON) → struct
jsonData := []byte(`{
    "id": 42,
    "name": "Bob",
    "email": "bob@example.com",
    "is_admin": true,
    "extra_field": "ignored"
}`)

var u User
err := json.Unmarshal(jsonData, &u) // Pasar PUNTERO
if err != nil {
    log.Fatal(err)
}
fmt.Printf("%+v\n", u)
// {ID:42 Name:Bob Email:bob@example.com Age:0 Password: IsAdmin:true ...}
// Nota: "extra_field" se ignora silenciosamente
//        Age=0 porque no esta en el JSON
//        Password="" porque json:"-" no lo decodifica

// Decodificar a map[string]any (cuando no conoces la estructura)
var generic map[string]any
json.Unmarshal(jsonData, &generic)
fmt.Println(generic["name"])    // "Bob" (tipo: string)
fmt.Println(generic["id"])      // 42 (tipo: float64 — JSON numbers son float64!)
```

### 8.4 Tipos numericos: la trampa de float64

```go
// CUIDADO: JSON numbers se decodifican como float64 en map[string]any
jsonData := []byte(`{"count": 42, "big_id": 9007199254740993}`)

var m map[string]any
json.Unmarshal(jsonData, &m)

count := m["count"].(float64)         // 42.0, no int
fmt.Printf("%T: %v\n", count, count) // float64: 42

// Problema con int64 grandes:
bigID := m["big_id"].(float64)
fmt.Printf("%.0f\n", bigID)          // 9007199254740992 — PERDIO PRECISION!
// JavaScript Number.MAX_SAFE_INTEGER = 2^53 - 1 = 9007199254740991
// float64 no puede representar 9007199254740993 exactamente

// Solucion 1: usar json.Number
decoder := json.NewDecoder(bytes.NewReader(jsonData))
decoder.UseNumber() // Decodificar numeros como json.Number (string)
var m2 map[string]any
decoder.Decode(&m2)
bigID2, _ := m2["big_id"].(json.Number).Int64() // Precision completa

// Solucion 2: usar struct con int64 + string tag
type Response struct {
    BigID int64 `json:"big_id,string"` // El JSON tiene "big_id": "9007199254740993"
}
```

### 8.5 Streaming JSON: Encoder y Decoder

```go
// Decoder: leer JSON desde un io.Reader (streaming, eficiente)
// NO carga todo en memoria — ideal para archivos grandes o HTTP bodies

// Decodificar desde un archivo
f, _ := os.Open("data.json")
defer f.Close()

decoder := json.NewDecoder(f)
var data []Record
if err := decoder.Decode(&data); err != nil {
    log.Fatal(err)
}

// Decodificar desde HTTP response
resp, _ := http.Get("https://api.example.com/users")
defer resp.Body.Close()

var users []User
json.NewDecoder(resp.Body).Decode(&users)

// Decodificar stream de objetos JSON (NDJSON / JSON Lines)
// Archivo: {"name":"a"}\n{"name":"b"}\n{"name":"c"}\n
scanner := json.NewDecoder(f)
for scanner.More() {
    var item Item
    if err := scanner.Decode(&item); err != nil {
        break
    }
    process(item)
}

// Encoder: escribir JSON a un io.Writer
// Ideal para HTTP responses

// Escribir a archivo
f, _ = os.Create("output.json")
encoder := json.NewEncoder(f)
encoder.SetIndent("", "  ") // Pretty print
encoder.Encode(data)
f.Close()

// Escribir HTTP response
func handleUsers(w http.ResponseWriter, r *http.Request) {
    users := getUsers()
    w.Header().Set("Content-Type", "application/json")
    json.NewEncoder(w).Encode(users)
}
```

### 8.6 Custom marshaling: MarshalJSON / UnmarshalJSON

```go
// Implementar json.Marshaler y json.Unmarshaler para control total

type Duration struct {
    time.Duration
}

// MarshalJSON: custom serialization
func (d Duration) MarshalJSON() ([]byte, error) {
    // Representar como string "5s", "1m30s" en vez de nanosegundos
    return json.Marshal(d.Duration.String())
}

// UnmarshalJSON: custom deserialization
func (d *Duration) UnmarshalJSON(data []byte) error {
    var s string
    if err := json.Unmarshal(data, &s); err != nil {
        return err
    }
    dur, err := time.ParseDuration(s)
    if err != nil {
        return err
    }
    d.Duration = dur
    return nil
}

// Uso:
type Config struct {
    Timeout  Duration `json:"timeout"`
    Interval Duration `json:"interval"`
}

// JSON: {"timeout": "5s", "interval": "1m30s"}
// En vez de: {"timeout": 5000000000, "interval": 90000000000}
```

### 8.7 json.RawMessage — decodificacion diferida

```go
// RawMessage: mantener JSON sin decodificar
// Util para JSON con estructura variable

type Event struct {
    Type    string          `json:"type"`
    Payload json.RawMessage `json:"payload"` // No se decodifica todavia
}

func processEvent(data []byte) error {
    var event Event
    if err := json.Unmarshal(data, &event); err != nil {
        return err
    }
    
    // Decodificar payload segun el tipo
    switch event.Type {
    case "user_created":
        var user User
        json.Unmarshal(event.Payload, &user)
        handleNewUser(user)
    case "order_placed":
        var order Order
        json.Unmarshal(event.Payload, &order)
        handleNewOrder(order)
    default:
        log.Printf("unknown event type: %s, raw: %s", event.Type, event.Payload)
    }
    return nil
}
```

---

## 9. Package net/http — cliente y servidor HTTP

### 9.1 Cliente HTTP

```go
import "net/http"

// --- GET simple ---
resp, err := http.Get("https://api.example.com/users")
if err != nil {
    log.Fatal(err)
}
defer resp.Body.Close() // SIEMPRE cerrar el body

fmt.Println("Status:", resp.StatusCode)
fmt.Println("Content-Type:", resp.Header.Get("Content-Type"))

body, _ := io.ReadAll(resp.Body)
fmt.Println(string(body))

// --- POST con JSON ---
user := map[string]string{"name": "Alice", "email": "alice@example.com"}
jsonData, _ := json.Marshal(user)

resp, err = http.Post(
    "https://api.example.com/users",
    "application/json",
    bytes.NewBuffer(jsonData),
)
if err != nil {
    log.Fatal(err)
}
defer resp.Body.Close()

// --- Request personalizado ---
req, err := http.NewRequestWithContext(
    ctx,                    // context para cancelacion/timeout
    http.MethodPut,
    "https://api.example.com/users/42",
    bytes.NewBuffer(jsonData),
)
if err != nil {
    log.Fatal(err)
}

req.Header.Set("Content-Type", "application/json")
req.Header.Set("Authorization", "Bearer "+token)
req.Header.Set("X-Request-ID", requestID)

resp, err = http.DefaultClient.Do(req)
if err != nil {
    log.Fatal(err)
}
defer resp.Body.Close()
```

### 9.2 http.Client configurado

```go
// DefaultClient no tiene timeouts — NUNCA usar en produccion sin configurar

client := &http.Client{
    Timeout: 30 * time.Second, // Timeout TOTAL (dial + TLS + headers + body)
    
    Transport: &http.Transport{
        MaxIdleConns:        100,              // Pool de conexiones
        MaxIdleConnsPerHost: 10,               // Por host
        IdleConnTimeout:     90 * time.Second, // Cerrar idle connections
        TLSHandshakeTimeout: 10 * time.Second,
        
        // Para proxies:
        // Proxy: http.ProxyFromEnvironment,
    },
    
    // Controlar redirects
    CheckRedirect: func(req *http.Request, via []*http.Request) error {
        if len(via) >= 10 {
            return fmt.Errorf("too many redirects")
        }
        return nil // Seguir el redirect
    },
}

resp, err := client.Get("https://api.example.com/data")
```

### 9.3 Servidor HTTP

```go
// Servidor basico con ServeMux (Go 1.22+ con patterns mejorados)

func main() {
    mux := http.NewServeMux()
    
    // Go 1.22+: metodos y patrones en el path
    mux.HandleFunc("GET /api/users", listUsers)
    mux.HandleFunc("POST /api/users", createUser)
    mux.HandleFunc("GET /api/users/{id}", getUser)       // Wildcard {id}
    mux.HandleFunc("PUT /api/users/{id}", updateUser)
    mux.HandleFunc("DELETE /api/users/{id}", deleteUser)
    
    // Archivos estaticos
    mux.Handle("GET /static/", http.StripPrefix("/static/",
        http.FileServer(http.Dir("./public")),
    ))
    
    // Health check
    mux.HandleFunc("GET /health", func(w http.ResponseWriter, r *http.Request) {
        w.WriteHeader(http.StatusOK)
        w.Write([]byte("ok"))
    })
    
    server := &http.Server{
        Addr:         ":8080",
        Handler:      mux,
        ReadTimeout:  15 * time.Second,
        WriteTimeout: 15 * time.Second,
        IdleTimeout:  60 * time.Second,
    }
    
    fmt.Println("Server listening on :8080")
    log.Fatal(server.ListenAndServe())
}

func listUsers(w http.ResponseWriter, r *http.Request) {
    users := []User{{ID: 1, Name: "Alice"}, {ID: 2, Name: "Bob"}}
    
    w.Header().Set("Content-Type", "application/json")
    json.NewEncoder(w).Encode(users)
}

func getUser(w http.ResponseWriter, r *http.Request) {
    // Go 1.22+: extraer wildcard del path
    id := r.PathValue("id")
    
    userID, err := strconv.Atoi(id)
    if err != nil {
        http.Error(w, "invalid user id", http.StatusBadRequest)
        return
    }
    
    user, err := findUser(userID)
    if err != nil {
        http.Error(w, "user not found", http.StatusNotFound)
        return
    }
    
    w.Header().Set("Content-Type", "application/json")
    json.NewEncoder(w).Encode(user)
}

func createUser(w http.ResponseWriter, r *http.Request) {
    var user User
    if err := json.NewDecoder(r.Body).Decode(&user); err != nil {
        http.Error(w, "invalid json", http.StatusBadRequest)
        return
    }
    
    // Validar...
    // Guardar...
    
    w.Header().Set("Content-Type", "application/json")
    w.WriteHeader(http.StatusCreated)
    json.NewEncoder(w).Encode(user)
}
```

### 9.4 Middleware

```go
// Middleware = funcion que envuelve un http.Handler

// Logging middleware
func loggingMiddleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        start := time.Now()
        
        // Llamar al siguiente handler
        next.ServeHTTP(w, r)
        
        log.Printf("%s %s %s", r.Method, r.URL.Path, time.Since(start))
    })
}

// Recovery middleware (capturar panics)
func recoveryMiddleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        defer func() {
            if err := recover(); err != nil {
                log.Printf("PANIC: %v", err)
                http.Error(w, "Internal Server Error", http.StatusInternalServerError)
            }
        }()
        next.ServeHTTP(w, r)
    })
}

// CORS middleware
func corsMiddleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        w.Header().Set("Access-Control-Allow-Origin", "*")
        w.Header().Set("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
        w.Header().Set("Access-Control-Allow-Headers", "Content-Type, Authorization")
        
        if r.Method == "OPTIONS" {
            w.WriteHeader(http.StatusOK)
            return
        }
        
        next.ServeHTTP(w, r)
    })
}

// Encadenar middleware
func main() {
    mux := http.NewServeMux()
    mux.HandleFunc("GET /api/users", listUsers)
    
    // Aplicar middleware (se ejecutan de afuera hacia adentro)
    handler := recoveryMiddleware(loggingMiddleware(corsMiddleware(mux)))
    
    http.ListenAndServe(":8080", handler)
}
```

### 9.5 Graceful shutdown

```go
func main() {
    mux := http.NewServeMux()
    mux.HandleFunc("GET /", handler)
    
    server := &http.Server{
        Addr:    ":8080",
        Handler: mux,
    }
    
    // Canal para errores del servidor
    serverErr := make(chan error, 1)
    go func() {
        fmt.Println("Server listening on :8080")
        serverErr <- server.ListenAndServe()
    }()
    
    // Esperar signal de shutdown
    quit := make(chan os.Signal, 1)
    signal.Notify(quit, os.Interrupt, syscall.SIGTERM)
    
    select {
    case err := <-serverErr:
        log.Fatalf("server error: %v", err)
    case sig := <-quit:
        fmt.Printf("\nReceived %v, shutting down...\n", sig)
    }
    
    // Graceful shutdown: esperar hasta 30s a que terminen requests activos
    ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
    defer cancel()
    
    if err := server.Shutdown(ctx); err != nil {
        log.Fatalf("shutdown error: %v", err)
    }
    fmt.Println("Server stopped gracefully")
}
```

---

## 10. Package log/slog — logging estructurado (Go 1.21+)

### 10.1 Que es slog y por que existe

```
LOGGING EN GO — EVOLUCION

Antes de Go 1.21:
  log.Println("user created", userID)
  → "2024/01/15 10:30:00 user created 42"
  
  Problemas:
  - No es estructurado (dificil de parsear con herramientas)
  - No hay niveles (debug, info, warn, error)
  - No hay formato JSON para log aggregators (ELK, Datadog, etc.)
  - Soluciones: zap, zerolog, logrus (dependencias externas)

Go 1.21+ con log/slog:
  slog.Info("user created", "user_id", 42, "email", "alice@example.com")
  → Text:  time=2024-01-15T10:30:00Z level=INFO msg="user created" user_id=42 email=alice@example.com
  → JSON:  {"time":"2024-01-15T10:30:00Z","level":"INFO","msg":"user created","user_id":42,"email":"alice@example.com"}
  
  Ventajas:
  - En la stdlib (sin dependencias)
  - Estructurado (key-value pairs)
  - 4 niveles: Debug, Info, Warn, Error
  - Handlers: Text (human-readable) o JSON (machine-readable)
  - Extensible: puedes crear tus propios handlers
  - Alto rendimiento (comparable con zap/zerolog)
```

### 10.2 Uso basico

```go
import "log/slog"

func main() {
    // Usar el logger por defecto (TextHandler a stderr)
    slog.Debug("this is debug")          // No se muestra (nivel default = Info)
    slog.Info("server started", "port", 8080)
    slog.Warn("cache miss", "key", "user:42", "latency_ms", 150)
    slog.Error("database error", "err", err, "query", "SELECT *")
    
    // Output (TextHandler):
    // time=2024-01-15T10:30:00.000Z level=INFO msg="server started" port=8080
    // time=2024-01-15T10:30:00.001Z level=WARN msg="cache miss" key=user:42 latency_ms=150
    // time=2024-01-15T10:30:00.002Z level=ERROR msg="database error" err="connection refused" query="SELECT *"
}
```

### 10.3 Configurar handler

```go
// TextHandler: formato human-readable (default)
textHandler := slog.NewTextHandler(os.Stdout, &slog.HandlerOptions{
    Level: slog.LevelDebug, // Mostrar todo desde Debug
})
logger := slog.New(textHandler)

// JSONHandler: formato machine-readable (para produccion)
jsonHandler := slog.NewJSONHandler(os.Stdout, &slog.HandlerOptions{
    Level: slog.LevelInfo,
    // AddSource: true, // Incluir file:line en cada log entry
})
logger = slog.New(jsonHandler)

// Establecer como logger global
slog.SetDefault(logger)

// Ahora slog.Info() usa el handler configurado
slog.Info("request handled",
    "method", "GET",
    "path", "/api/users",
    "status", 200,
    "duration_ms", 42,
)

// JSON output:
// {"time":"2024-01-15T10:30:00Z","level":"INFO","msg":"request handled",
//  "method":"GET","path":"/api/users","status":200,"duration_ms":42}
```

### 10.4 Atributos tipados y grupos

```go
// Atributos con tipo explicito (mas eficiente que string key + any value)
slog.Info("request",
    slog.String("method", "POST"),
    slog.Int("status", 201),
    slog.Duration("latency", 42*time.Millisecond),
    slog.Time("timestamp", time.Now()),
    slog.Bool("cached", false),
    slog.Any("headers", r.Header), // Para tipos arbitrarios
)

// Grupos: agrupar atributos bajo un namespace
slog.Info("request handled",
    slog.Group("request",
        slog.String("method", r.Method),
        slog.String("path", r.URL.Path),
        slog.String("remote_addr", r.RemoteAddr),
    ),
    slog.Group("response",
        slog.Int("status", status),
        slog.Int("bytes", bytesWritten),
    ),
    slog.Duration("latency", duration),
)

// JSON output:
// {"time":"...","level":"INFO","msg":"request handled",
//  "request":{"method":"GET","path":"/api/users","remote_addr":"127.0.0.1:5432"},
//  "response":{"status":200,"bytes":1234},
//  "latency":"42ms"}
```

### 10.5 Logger con contexto (With)

```go
// Logger con atributos fijos (ej: por request, por servicio)
// Evita repetir los mismos atributos en cada log call

// Logger por servicio
serviceLogger := slog.With(
    "service", "user-api",
    "version", "1.2.3",
)
serviceLogger.Info("starting")
// {"service":"user-api","version":"1.2.3","msg":"starting",...}

// Logger por request (en middleware)
func loggingMiddleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        requestID := r.Header.Get("X-Request-ID")
        if requestID == "" {
            requestID = uuid.New().String()
        }
        
        // Crear logger con request context
        logger := slog.With(
            "request_id", requestID,
            "method", r.Method,
            "path", r.URL.Path,
        )
        
        // Pasar logger via context
        ctx := context.WithValue(r.Context(), loggerKey, logger)
        
        start := time.Now()
        next.ServeHTTP(w, r.WithContext(ctx))
        
        logger.Info("request completed",
            "duration_ms", time.Since(start).Milliseconds(),
        )
    })
}

// Usar el logger del contexto en handlers
func handler(w http.ResponseWriter, r *http.Request) {
    logger := r.Context().Value(loggerKey).(*slog.Logger)
    logger.Info("processing user request") // Incluye request_id automaticamente
}
```

### 10.6 Nivel dinamico

```go
// Nivel que se puede cambiar en runtime (ej: via HTTP endpoint)
var logLevel = new(slog.LevelVar)
logLevel.Set(slog.LevelInfo) // Default

handler := slog.NewJSONHandler(os.Stdout, &slog.HandlerOptions{
    Level: logLevel, // Referencia al level variable
})
slog.SetDefault(slog.New(handler))

// En algun handler de admin:
func setLogLevel(w http.ResponseWriter, r *http.Request) {
    level := r.URL.Query().Get("level")
    switch level {
    case "debug":
        logLevel.Set(slog.LevelDebug)
    case "info":
        logLevel.Set(slog.LevelInfo)
    case "warn":
        logLevel.Set(slog.LevelWarn)
    case "error":
        logLevel.Set(slog.LevelError)
    default:
        http.Error(w, "invalid level", http.StatusBadRequest)
        return
    }
    fmt.Fprintf(w, "log level set to %s\n", level)
}
// PUT /admin/log-level?level=debug → habilita debug en caliente
```

### 10.7 Comparacion: slog vs zap vs zerolog

```
┌─────────────────────┬──────────────────┬──────────────────┬──────────────────┐
│ Propiedad            │ log/slog (stdlib)│ zap (uber)       │ zerolog          │
├─────────────────────┼──────────────────┼──────────────────┼──────────────────┤
│ Stdlib               │ Si (Go 1.21+)   │ No               │ No               │
│ Structured           │ Si              │ Si               │ Si               │
│ Niveles              │ 4 (D/I/W/E)     │ 6 (+DPanic,Fatal)│ 7 (+Trace,Fatal, │
│                      │                  │                  │  Panic)          │
│ JSON handler         │ Si              │ Si               │ Si (default)     │
│ Text handler         │ Si              │ Si (sugar)       │ Si (ConsoleWriter│
│ Zero-alloc           │ Parcial         │ Si               │ Si               │
│ Rendimiento          │ Bueno           │ Excelente        │ Excelente        │
│ Context support      │ Si              │ Si               │ Si               │
│ Sampling             │ No              │ Si               │ Si               │
│ Caller info          │ AddSource       │ AddCaller        │ Caller           │
│ Custom handlers      │ Si (interface)  │ Si (Core)        │ Si (Hook)        │
│ API style            │ Variadic k/v    │ Typed fields     │ Method chain     │
│ Dependencias         │ 0               │ 2                │ 0                │
└─────────────────────┴──────────────────┴──────────────────┴──────────────────┘

RECOMENDACION:
  - Proyecto nuevo, stdlib es suficiente → slog
  - Necesitas sampling o rendimiento extremo → zap o zerolog
  - slog es compatible con zap/zerolog via bridge handlers
```

---

## 11. Comparacion con C y Rust

```
STDLIB: GO vs C vs RUST

┌──────────────────────┬───────────────────────────┬───────────────────────┬──────────────────────────┐
│ Funcionalidad         │ Go (stdlib)               │ C (libc + POSIX)      │ Rust (std + crates)      │
├──────────────────────┼───────────────────────────┼───────────────────────┼──────────────────────────┤
│ Archivos              │ os.Open/Create/ReadFile   │ fopen/fclose/fread    │ std::fs::File/read/write │
│ Dirs                  │ os.ReadDir, filepath.Walk │ opendir/readdir       │ std::fs::read_dir        │
│ Env vars              │ os.Getenv/Setenv          │ getenv/setenv         │ std::env::var/set_var    │
│ Rutas                 │ path/filepath             │ Manual + realpath()   │ std::path::Path/PathBuf  │
│ String format         │ fmt.Sprintf               │ sprintf               │ format! macro            │
│ String manip          │ strings.*                 │ string.h (strlen..)   │ str methods, String      │
│ Num → string          │ strconv                   │ atoi/itoa/strtol      │ parse::<i32>(), to_string│
│ JSON                  │ encoding/json (stdlib!)   │ No (jansson, cJSON)   │ No std (serde_json crate)│
│ HTTP client           │ net/http (stdlib!)        │ No (libcurl)          │ No std (reqwest crate)   │
│ HTTP server           │ net/http (stdlib!)        │ No (microhttpd, etc)  │ No std (axum, actix)     │
│ Logging               │ log/slog (stdlib!)        │ No (syslog basico)    │ No std (tracing crate)   │
│ Testing               │ testing (stdlib!)         │ No (Unity, CUnit)     │ #[test] (stdlib!)        │
│ Regex                 │ regexp (stdlib)           │ regex.h (POSIX)       │ No std (regex crate)     │
│ Crypto                │ crypto/* (stdlib)         │ No (OpenSSL)          │ Parcial (ring, rustls)   │
│ Compress              │ compress/gzip (stdlib)    │ No (zlib)             │ No std (flate2 crate)    │
│ Templates             │ text/template (stdlib)    │ No                    │ No std (tera, askama)    │
│ SQL                   │ database/sql (stdlib)     │ No (libpq, etc)      │ No std (sqlx, diesel)    │
│                       │                           │                       │                          │
│ TOTAL stdlib coverage │ EXCELENTE                 │ BASICO                │ MODERADO                 │
│ Deps para web app     │ 0-2                       │ 5-10+                 │ 5-10+                    │
└──────────────────────┴───────────────────────────┴───────────────────────┴──────────────────────────┘

CONCLUSION:
  Go tiene la stdlib mas "batteries-included" de los tres.
  Puedes construir un servicio web completo con JSON API,
  logging estructurado, y testing sin UNA sola dependencia externa.
  
  En C necesitas librerias externas para casi todo.
  En Rust, crates.io es excelente pero son dependencias externas.
```

---

## 12. Programa completo: API Server con stdlib pura

Este programa demuestra un servidor API completo usando **exclusivamente** la biblioteca estandar — sin dependencias externas:

```go
package main

import (
    "context"
    "encoding/json"
    "fmt"
    "log/slog"
    "net/http"
    "os"
    "os/signal"
    "path/filepath"
    "strconv"
    "strings"
    "sync"
    "syscall"
    "time"
)

// --- Tipos ---

type Task struct {
    ID        int       `json:"id"`
    Title     string    `json:"title"`
    Status    string    `json:"status"`
    Priority  int       `json:"priority,omitempty"`
    CreatedAt time.Time `json:"created_at"`
    DoneAt    *time.Time `json:"done_at,omitempty"`
}

type CreateTaskRequest struct {
    Title    string `json:"title"`
    Priority int    `json:"priority"`
}

type ErrorResponse struct {
    Error   string `json:"error"`
    Code    int    `json:"code"`
}

type StatsResponse struct {
    Total     int           `json:"total"`
    Pending   int           `json:"pending"`
    Done      int           `json:"done"`
    AvgPriority float64     `json:"avg_priority"`
    Uptime    string        `json:"uptime"`
}

// --- Store (in-memory con mutex) ---

type Store struct {
    mu     sync.RWMutex
    tasks  map[int]*Task
    nextID int
}

func NewStore() *Store {
    return &Store{
        tasks:  make(map[int]*Task),
        nextID: 1,
    }
}

func (s *Store) Create(title string, priority int) *Task {
    s.mu.Lock()
    defer s.mu.Unlock()

    task := &Task{
        ID:        s.nextID,
        Title:     title,
        Status:    "pending",
        Priority:  priority,
        CreatedAt: time.Now(),
    }
    s.tasks[s.nextID] = task
    s.nextID++
    return task
}

func (s *Store) Get(id int) (*Task, bool) {
    s.mu.RLock()
    defer s.mu.RUnlock()
    t, ok := s.tasks[id]
    return t, ok
}

func (s *Store) List(status string) []*Task {
    s.mu.RLock()
    defer s.mu.RUnlock()

    result := make([]*Task, 0, len(s.tasks))
    for _, t := range s.tasks {
        if status == "" || t.Status == status {
            result = append(result, t)
        }
    }
    return result
}

func (s *Store) MarkDone(id int) (*Task, bool) {
    s.mu.Lock()
    defer s.mu.Unlock()

    t, ok := s.tasks[id]
    if !ok {
        return nil, false
    }
    now := time.Now()
    t.Status = "done"
    t.DoneAt = &now
    return t, true
}

func (s *Store) Delete(id int) bool {
    s.mu.Lock()
    defer s.mu.Unlock()
    _, ok := s.tasks[id]
    if ok {
        delete(s.tasks, id)
    }
    return ok
}

func (s *Store) Stats() (total, pending, done int, avgPriority float64) {
    s.mu.RLock()
    defer s.mu.RUnlock()

    sumPriority := 0
    for _, t := range s.tasks {
        total++
        if t.Status == "done" {
            done++
        } else {
            pending++
        }
        sumPriority += t.Priority
    }
    if total > 0 {
        avgPriority = float64(sumPriority) / float64(total)
    }
    return
}

// --- Middleware ---

type contextKey string

const requestIDKey contextKey = "request_id"

var requestCounter int64
var counterMu sync.Mutex

func nextRequestID() string {
    counterMu.Lock()
    defer counterMu.Unlock()
    requestCounter++
    return fmt.Sprintf("req-%d", requestCounter)
}

func loggingMiddleware(logger *slog.Logger, next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        start := time.Now()
        reqID := nextRequestID()

        reqLogger := logger.With(
            "request_id", reqID,
            "method", r.Method,
            "path", r.URL.Path,
            "remote_addr", r.RemoteAddr,
        )

        ctx := context.WithValue(r.Context(), requestIDKey, reqID)
        r = r.WithContext(ctx)

        reqLogger.Info("request started")
        next.ServeHTTP(w, r)
        reqLogger.Info("request completed",
            "duration_ms", time.Since(start).Milliseconds(),
        )
    })
}

func recoveryMiddleware(logger *slog.Logger, next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        defer func() {
            if err := recover(); err != nil {
                logger.Error("panic recovered",
                    "error", fmt.Sprintf("%v", err),
                    "path", r.URL.Path,
                )
                writeJSON(w, http.StatusInternalServerError, ErrorResponse{
                    Error: "internal server error",
                    Code:  http.StatusInternalServerError,
                })
            }
        }()
        next.ServeHTTP(w, r)
    })
}

// --- Helpers ---

func writeJSON(w http.ResponseWriter, status int, data any) {
    w.Header().Set("Content-Type", "application/json")
    w.WriteHeader(status)
    json.NewEncoder(w).Encode(data)
}

func readJSON(r *http.Request, dst any) error {
    decoder := json.NewDecoder(r.Body)
    decoder.DisallowUnknownFields() // Rechazar campos extra
    return decoder.Decode(dst)
}

// --- Handlers ---

type API struct {
    store     *Store
    logger    *slog.Logger
    startTime time.Time
}

func (api *API) handleListTasks(w http.ResponseWriter, r *http.Request) {
    status := r.URL.Query().Get("status")
    if status != "" && status != "pending" && status != "done" {
        writeJSON(w, http.StatusBadRequest, ErrorResponse{
            Error: "status must be 'pending' or 'done'",
            Code:  http.StatusBadRequest,
        })
        return
    }

    tasks := api.store.List(status)
    writeJSON(w, http.StatusOK, tasks)
}

func (api *API) handleCreateTask(w http.ResponseWriter, r *http.Request) {
    var req CreateTaskRequest
    if err := readJSON(r, &req); err != nil {
        writeJSON(w, http.StatusBadRequest, ErrorResponse{
            Error: "invalid json: " + err.Error(),
            Code:  http.StatusBadRequest,
        })
        return
    }

    title := strings.TrimSpace(req.Title)
    if title == "" {
        writeJSON(w, http.StatusBadRequest, ErrorResponse{
            Error: "title is required",
            Code:  http.StatusBadRequest,
        })
        return
    }

    task := api.store.Create(title, req.Priority)
    api.logger.Info("task created",
        "task_id", task.ID,
        "title", task.Title,
    )
    writeJSON(w, http.StatusCreated, task)
}

func (api *API) handleGetTask(w http.ResponseWriter, r *http.Request) {
    id, err := strconv.Atoi(r.PathValue("id"))
    if err != nil {
        writeJSON(w, http.StatusBadRequest, ErrorResponse{
            Error: "invalid task id",
            Code:  http.StatusBadRequest,
        })
        return
    }

    task, ok := api.store.Get(id)
    if !ok {
        writeJSON(w, http.StatusNotFound, ErrorResponse{
            Error: "task not found",
            Code:  http.StatusNotFound,
        })
        return
    }
    writeJSON(w, http.StatusOK, task)
}

func (api *API) handleCompleteTask(w http.ResponseWriter, r *http.Request) {
    id, err := strconv.Atoi(r.PathValue("id"))
    if err != nil {
        writeJSON(w, http.StatusBadRequest, ErrorResponse{
            Error: "invalid task id",
            Code:  http.StatusBadRequest,
        })
        return
    }

    task, ok := api.store.MarkDone(id)
    if !ok {
        writeJSON(w, http.StatusNotFound, ErrorResponse{
            Error: "task not found",
            Code:  http.StatusNotFound,
        })
        return
    }

    api.logger.Info("task completed", "task_id", id, "title", task.Title)
    writeJSON(w, http.StatusOK, task)
}

func (api *API) handleDeleteTask(w http.ResponseWriter, r *http.Request) {
    id, err := strconv.Atoi(r.PathValue("id"))
    if err != nil {
        writeJSON(w, http.StatusBadRequest, ErrorResponse{
            Error: "invalid task id",
            Code:  http.StatusBadRequest,
        })
        return
    }

    if !api.store.Delete(id) {
        writeJSON(w, http.StatusNotFound, ErrorResponse{
            Error: "task not found",
            Code:  http.StatusNotFound,
        })
        return
    }

    api.logger.Info("task deleted", "task_id", id)
    w.WriteHeader(http.StatusNoContent)
}

func (api *API) handleStats(w http.ResponseWriter, r *http.Request) {
    total, pending, done, avgPri := api.store.Stats()
    writeJSON(w, http.StatusOK, StatsResponse{
        Total:       total,
        Pending:     pending,
        Done:        done,
        AvgPriority: avgPri,
        Uptime:      time.Since(api.startTime).Round(time.Second).String(),
    })
}

func (api *API) handleHealth(w http.ResponseWriter, r *http.Request) {
    writeJSON(w, http.StatusOK, map[string]string{
        "status": "ok",
        "time":   time.Now().Format(time.RFC3339),
    })
}

// --- Main ---

func main() {
    // Configuracion desde env vars
    port := os.Getenv("PORT")
    if port == "" {
        port = "8080"
    }
    logLevel := os.Getenv("LOG_LEVEL")

    // Logger
    var level slog.Level
    switch strings.ToLower(logLevel) {
    case "debug":
        level = slog.LevelDebug
    case "warn":
        level = slog.LevelWarn
    case "error":
        level = slog.LevelError
    default:
        level = slog.LevelInfo
    }

    logger := slog.New(slog.NewJSONHandler(os.Stdout, &slog.HandlerOptions{
        Level: level,
    }))
    slog.SetDefault(logger)

    // Inicializar componentes
    store := NewStore()
    api := &API{
        store:     store,
        logger:    logger,
        startTime: time.Now(),
    }

    // Rutas
    mux := http.NewServeMux()
    mux.HandleFunc("GET /api/tasks", api.handleListTasks)
    mux.HandleFunc("POST /api/tasks", api.handleCreateTask)
    mux.HandleFunc("GET /api/tasks/{id}", api.handleGetTask)
    mux.HandleFunc("POST /api/tasks/{id}/done", api.handleCompleteTask)
    mux.HandleFunc("DELETE /api/tasks/{id}", api.handleDeleteTask)
    mux.HandleFunc("GET /api/stats", api.handleStats)
    mux.HandleFunc("GET /health", api.handleHealth)

    // Middleware
    handler := recoveryMiddleware(logger,
        loggingMiddleware(logger, mux),
    )

    // Servidor
    server := &http.Server{
        Addr:         ":" + port,
        Handler:      handler,
        ReadTimeout:  15 * time.Second,
        WriteTimeout: 15 * time.Second,
        IdleTimeout:  60 * time.Second,
    }

    // Informacion de inicio
    exe, _ := os.Executable()
    logger.Info("server starting",
        "port", port,
        "log_level", level.String(),
        "executable", filepath.Base(exe),
        "pid", os.Getpid(),
    )

    // Iniciar en goroutine
    serverErr := make(chan error, 1)
    go func() {
        serverErr <- server.ListenAndServe()
    }()

    // Esperar signal
    quit := make(chan os.Signal, 1)
    signal.Notify(quit, os.Interrupt, syscall.SIGTERM)

    select {
    case err := <-serverErr:
        logger.Error("server failed", "error", err)
        os.Exit(1)
    case sig := <-quit:
        logger.Info("shutdown signal received", "signal", sig.String())
    }

    // Graceful shutdown
    ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
    defer cancel()

    if err := server.Shutdown(ctx); err != nil {
        logger.Error("shutdown error", "error", err)
        os.Exit(1)
    }

    total, _, _, _ := store.Stats()
    logger.Info("server stopped",
        "total_tasks", total,
        "uptime", time.Since(api.startTime).Round(time.Second).String(),
    )
}
```

```
ESTE PROGRAMA USA EXCLUSIVAMENTE STDLIB:

  encoding/json    → serializar/deserializar JSON
  fmt              → formatear strings y errores
  log/slog         → logging estructurado (JSON handler)
  net/http         → servidor HTTP con routing (Go 1.22+)
  os               → env vars, signals, PID, executable path
  os/signal        → capturar SIGINT/SIGTERM
  path/filepath    → extraer nombre del ejecutable
  strconv          → convertir path params a int
  strings          → TrimSpace, ToLower
  sync             → RWMutex para store concurrente
  time             → timestamps, duraciones, timeouts
  context          → shutdown con timeout, request context

  ZERO dependencias externas. Production-ready.

PROBAR:
  go run main.go
  curl http://localhost:8080/api/tasks                          # GET list
  curl -X POST -d '{"title":"Buy milk","priority":2}' http://localhost:8080/api/tasks  # POST create
  curl http://localhost:8080/api/tasks/1                        # GET by id
  curl -X POST http://localhost:8080/api/tasks/1/done           # POST complete
  curl http://localhost:8080/api/stats                          # GET stats
  curl http://localhost:8080/health                             # GET health
```

---

## 13. Ejercicios

### Ejercicio 1: File processor con io
Escribe un programa que:
- Reciba un directorio como argumento (`os.Args[1]`)
- Use `filepath.WalkDir` para encontrar todos los archivos `.log`
- Para cada archivo, lea las lineas que contienen "ERROR" (usa `bufio.Scanner`)
- Escriba todas las lineas de error a un solo archivo `errors_combined.log`
- Use `io.MultiWriter` para tambien imprimirlas a `os.Stdout`
- Al final, imprima stats: total de archivos procesados, total de lineas de error, tamaño total procesado (usa `os.Stat`)

### Ejercicio 2: JSON config loader
Escribe un paquete `config` que:
- Defina un struct `Config` con campos para `Host string`, `Port int`, `Debug bool`, `Database struct { DSN string, MaxConns int }`, `AllowedOrigins []string`
- Implemente `LoadFromFile(path string) (*Config, error)` que lea JSON desde archivo
- Implemente `LoadFromEnv() *Config` que lea desde env vars (con defaults)
- Implemente `Merge(file, env *Config) *Config` donde env vars overridean valores del archivo
- Use `json:"..."` tags con `omitempty` donde sea apropiado
- Implemente `fmt.Stringer` en Config para pretty-print (sin mostrar DSN, que puede tener password)

### Ejercicio 3: HTTP JSON API con stdlib
Construye un API REST de "bookmarks" usando solo stdlib:
- `GET /api/bookmarks` — listar (con query param `?tag=X` para filtrar)
- `POST /api/bookmarks` — crear (titulo, URL, tags)
- `DELETE /api/bookmarks/{id}` — eliminar
- `GET /api/bookmarks/export` — exportar todo como JSON indentado (download con `Content-Disposition`)
- Logging con `slog` (JSON handler)
- Graceful shutdown
- Middleware de request timing

### Ejercicio 4: String processor
Escribe un programa que:
- Lea un archivo de texto (usa `os.ReadFile`)
- Cuente: palabras (usa `strings.Fields`), lineas (usa `strings.Count`), caracteres, bytes
- Encuentre la palabra mas larga (usa `strings.Fields` + iterar)
- Reemplace todas las URLs (usa `strings.Contains("http")` + logica manual o regexp) con `[LINK]`
- Genere un reporte con `fmt.Fprintf` a un `strings.Builder` y luego a archivo
- Convierta todas las metricas a string con `strconv` (no `fmt.Sprintf`) para practicar strconv

---

> **Siguiente**: C09/S01 - Sistema de Paquetes, T03 - Dependencias externas — go get, versiones, replace directive, go mod vendor, modulos privados
