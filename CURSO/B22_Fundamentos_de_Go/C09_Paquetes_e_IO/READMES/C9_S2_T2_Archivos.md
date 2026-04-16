# Archivos — os.Open, os.Create, os.ReadFile, os.WriteFile, bufio.Scanner, bufio.Writer

## 1. Introduccion

El manejo de archivos en Go se construye sobre `io.Reader` e `io.Writer` — `os.File` implementa ambas interfaces, y todo el ecosistema de I/O se aplica. Pero mas alla de las interfaces, hay decisiones practicas que importan: cuando usar `os.ReadFile` vs `bufio.Scanner`, como manejar permisos, como hacer escritura atomica, como recorrer directorios eficientemente, y como evitar bugs sutiles como olvidar cerrar archivos o perder datos por no llamar `Close` en un escritor.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                    ARCHIVOS EN GO                                               │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  OPERACIONES RAPIDAS (todo-de-un-golpe)                                        │
│  ├─ os.ReadFile(path)             → leer archivo completo en []byte            │
│  └─ os.WriteFile(path, data, perm)→ escribir archivo completo                  │
│                                                                                  │
│  OPERACIONES CON HANDLE (abrir, operar, cerrar)                                │
│  ├─ os.Open(path)                 → abrir para SOLO LECTURA                    │
│  ├─ os.Create(path)              → crear/truncar para LECTURA+ESCRITURA        │
│  ├─ os.OpenFile(path, flags, perm)→ control total                              │
│  ├─ f.Read(buf) / f.Write(buf)   → I/O basico                                 │
│  ├─ f.Seek(offset, whence)       → mover posicion                             │
│  └─ f.Close()                    → liberar recurso (SIEMPRE con defer)         │
│                                                                                  │
│  LECTURA EFICIENTE                                                              │
│  ├─ bufio.NewReader(f)           → reader buffered                             │
│  ├─ bufio.NewScanner(f)          → leer por lineas/palabras                    │
│  └─ io.Copy(dst, f)             → copiar streaming                            │
│                                                                                  │
│  ESCRITURA EFICIENTE                                                            │
│  ├─ bufio.NewWriter(f)          → writer buffered (FLUSH obligatorio)          │
│  ├─ fmt.Fprintf(f, ...)         → formato a archivo                            │
│  └─ io.Copy(f, src)             → copiar streaming                            │
│                                                                                  │
│  DIRECTORIOS                                                                    │
│  ├─ os.ReadDir(path)            → listar un directorio                         │
│  ├─ filepath.WalkDir(root, fn)  → recorrer recursivamente                     │
│  └─ filepath.Glob(pattern)      → buscar con patron                           │
│                                                                                  │
│  OPERACIONES DE FILESYSTEM                                                      │
│  ├─ os.Stat/Lstat               → informacion del archivo                      │
│  ├─ os.Rename                   → mover/renombrar                              │
│  ├─ os.Remove/RemoveAll         → eliminar                                     │
│  ├─ os.Mkdir/MkdirAll           → crear directorios                            │
│  ├─ os.Chmod/Chown              → cambiar permisos/owner                       │
│  ├─ os.Symlink/Readlink         → symlinks                                     │
│  └─ os.CreateTemp/MkdirTemp     → temporales                                  │
│                                                                                  │
│  COMPARACION                                                                    │
│  ├─ C:    fopen/fclose/fread/fwrite/fprintf + open/close/read/write            │
│  ├─ Rust: File::open/create + Read/Write traits + BufReader/BufWriter          │
│  └─ Go:   os.Open/Create + io.Reader/Writer + bufio                            │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Operaciones rapidas: ReadFile y WriteFile

### 2.1 os.ReadFile — leer archivo completo

```go
// os.ReadFile lee TODO el archivo en memoria y retorna []byte
// Equivale a: Open + ReadAll + Close
data, err := os.ReadFile("config.json")
if err != nil {
    log.Fatal(err)
}
fmt.Println(string(data))

// Internamente hace:
// f, err := os.Open(name)
// defer f.Close()
// data, err := io.ReadAll(f)
```

```
CUANDO USAR os.ReadFile

✓ Archivos de configuracion (JSON, YAML, TOML) — tipicamente < 1MB
✓ Templates — pequenos, se leen una vez
✓ Archivos de texto pequenos
✓ Cuando necesitas todo el contenido como []byte o string

✗ Archivos mayores a ~100MB — consume mucha memoria
✗ Log files que crecen continuamente
✗ Procesamiento linea por linea — mejor bufio.Scanner
✗ Archivos binarios grandes — mejor io.Copy o chunks
✗ Cuando solo necesitas las primeras N lineas

NOTA: os.ReadFile fue agregado en Go 1.16.
Antes se usaba ioutil.ReadFile (ahora deprecated).
```

### 2.2 os.WriteFile — escribir archivo completo

```go
// os.WriteFile escribe []byte a un archivo, creandolo o truncandolo
// Equivale a: Create(con permisos) + Write + Close
content := []byte("linea 1\nlinea 2\nlinea 3\n")
err := os.WriteFile("output.txt", content, 0o644)
if err != nil {
    log.Fatal(err)
}

// Permisos: 0o644 = rw-r--r--
// El archivo se crea con estos permisos (aplicando umask)
// Si el archivo ya existe, se TRUNCA (se sobreescribe completamente)

// Escribir JSON a archivo (patron comun)
type Config struct {
    Host string `json:"host"`
    Port int    `json:"port"`
}

cfg := Config{Host: "localhost", Port: 8080}
data, err := json.MarshalIndent(cfg, "", "  ")
if err != nil {
    log.Fatal(err)
}
os.WriteFile("config.json", data, 0o644)
```

```
CUANDO USAR os.WriteFile

✓ Archivos pequenos que se escriben de un golpe
✓ Archivos de configuracion
✓ Archivos generados (JSON, CSV pequenos)
✓ Cuando ya tienes todo el contenido en []byte

✗ Archivos que se construyen incrementalmente (logs)
✗ Archivos grandes que no caben en memoria
✗ Cuando necesitas append (WriteFile trunca)
✗ Cuando necesitas escritura atomica (ver seccion de patrones)
```

### 2.3 Nota sobre permisos

```
PERMISOS EN GO (notacion octal)

Los permisos usan el formato Unix rwxrwxrwx con notacion octal:

  0o644 = rw-r--r--   Archivos regulares (owner lee/escribe, otros leen)
  0o600 = rw-------   Archivos privados (solo owner)
  0o755 = rwxr-xr-x   Directorios y ejecutables
  0o700 = rwx------   Directorios privados
  0o666 = rw-rw-rw-   Archivos (antes de umask, usado por os.Create)
  0o777 = rwxrwxrwx   Maximo (raro, usado antes de umask)

UMASK: el sistema aplica umask DESPUES del permiso que especificas.
  Si umask = 0022 y pides 0o666 → resultado = 0o644
  Si umask = 0022 y pides 0o777 → resultado = 0o755

RECOMENDACIONES:
  Archivos regulares:    0o644
  Archivos sensibles:    0o600 (keys, tokens, passwords)
  Directorios:           0o755
  Directorios sensibles: 0o700
  Ejecutables:           0o755

Go usa os.FileMode (uint32) para representar permisos:
  os.ModePerm = 0o777     (mascara para permisos rwx)
  os.ModeDir              (flag de directorio)
  os.ModeSymlink          (flag de symlink)
  os.ModeSetuid           (setuid bit)
  os.ModeSetgid           (setgid bit)
  os.ModeSticky           (sticky bit)
```

---

## 3. os.Open — abrir para lectura

### 3.1 Basico

```go
// os.Open abre un archivo para SOLO LECTURA (O_RDONLY)
f, err := os.Open("data.txt")
if err != nil {
    // Tipos de error comunes:
    // - *os.PathError con err.Err = syscall.ENOENT (no existe)
    // - *os.PathError con err.Err = syscall.EACCES (sin permiso)
    // - *os.PathError con err.Err = syscall.EISDIR (es un directorio)
    log.Fatal(err)
}
defer f.Close() // SIEMPRE cerrar con defer

// Ahora f es un *os.File que implementa:
// io.Reader, io.Writer (fallara en escritura porque es O_RDONLY),
// io.Closer, io.Seeker, io.ReaderAt, io.WriterTo
```

### 3.2 Verificar tipos de error

```go
f, err := os.Open("config.json")
if err != nil {
    if os.IsNotExist(err) {
        fmt.Println("El archivo no existe — usando defaults")
        return defaultConfig, nil
    }
    if os.IsPermission(err) {
        return nil, fmt.Errorf("sin permiso para leer %s: %w", path, err)
    }
    return nil, err
}
defer f.Close()

// Go 1.13+ con errors.Is (preferido):
if errors.Is(err, os.ErrNotExist) {
    // No existe
}
if errors.Is(err, os.ErrPermission) {
    // Sin permiso
}
if errors.Is(err, os.ErrExist) {
    // Ya existe (con O_EXCL)
}

// Extraer PathError para mas detalles:
var pathErr *os.PathError
if errors.As(err, &pathErr) {
    fmt.Println("Operacion:", pathErr.Op)    // "open"
    fmt.Println("Path:", pathErr.Path)       // "config.json"
    fmt.Println("Error:", pathErr.Err)       // syscall error
}
```

### 3.3 Leer con os.Open

```go
f, err := os.Open("data.bin")
if err != nil {
    log.Fatal(err)
}
defer f.Close()

// Opcion 1: Read directo (bajo nivel)
buf := make([]byte, 4096)
for {
    n, err := f.Read(buf)
    if n > 0 {
        process(buf[:n])
    }
    if err == io.EOF {
        break
    }
    if err != nil {
        log.Fatal(err)
    }
}

// Opcion 2: io.ReadAll (todo en memoria)
data, err := io.ReadAll(f)

// Opcion 3: io.Copy (streaming a otro Writer)
io.Copy(os.Stdout, f)

// Opcion 4: bufio.Scanner (por lineas)
scanner := bufio.NewScanner(f)
for scanner.Scan() {
    fmt.Println(scanner.Text())
}

// Opcion 5: ReadAt (posicion especifica, sin mover cursor)
buf2 := make([]byte, 100)
n, err := f.ReadAt(buf2, 1000) // Leer 100 bytes en posicion 1000
```

---

## 4. os.Create — crear/truncar para escritura

### 4.1 Basico

```go
// os.Create crea un archivo nuevo o TRUNCA uno existente
// Equivale a: OpenFile(name, O_RDWR|O_CREATE|O_TRUNC, 0666)
f, err := os.Create("output.txt")
if err != nil {
    log.Fatal(err)
}
defer f.Close()

// Escribir
f.WriteString("primera linea\n")
f.Write([]byte("segunda linea\n"))
fmt.Fprintf(f, "tercera linea: %d\n", 42)

// CUIDADO: os.Create TRUNCA si el archivo existe
// Si quieres append, usa os.OpenFile con O_APPEND
```

### 4.2 El peligro de Close en escritura

```go
// MAL — ignorar error de Close en escritura puede perder datos
func writeData(path string, data []byte) error {
    f, err := os.Create(path)
    if err != nil {
        return err
    }
    defer f.Close() // Si Close falla, los datos pueden no haberse escrito

    _, err = f.Write(data)
    return err
}

// BIEN — capturar error de Close
func writeData(path string, data []byte) (retErr error) {
    f, err := os.Create(path)
    if err != nil {
        return err
    }
    defer func() {
        cerr := f.Close()
        if retErr == nil {
            retErr = cerr // Solo si Write no fallo
        }
    }()

    _, err = f.Write(data)
    return err
}

// ALTERNATIVA — Close explicito antes de return
func writeData(path string, data []byte) error {
    f, err := os.Create(path)
    if err != nil {
        return err
    }

    if _, err := f.Write(data); err != nil {
        f.Close() // Cerrar antes de retornar error (ignorar error de Close)
        return err
    }

    return f.Close() // El error de Close importa aqui
}
```

```
POR QUE Close() PUEDE FALLAR EN ESCRITURA

1. EL OS BUFFERIZA WRITES
   Cuando haces f.Write(), los datos pueden ir al buffer del kernel,
   no al disco. Close() flushea ese buffer. Si el disco esta lleno
   o el filesystem tiene errores, Close() es cuando lo descubres.

2. NETWORK FILESYSTEMS (NFS, SMB)
   El write puede parecer exitoso localmente, pero el servidor
   rechaza los datos al hacer Close().

3. DISK FULL
   Writes pequenos pueden caber en el buffer del kernel.
   Al hacer Close(), el kernel intenta escribir y descubre
   que no hay espacio.

4. REGLA:
   Para LECTURA: defer f.Close() es suficiente (ignorar error OK)
   Para ESCRITURA: manejar el error de Close()
```

---

## 5. os.OpenFile — control total

### 5.1 Flags y permisos

```go
// os.OpenFile(name string, flag int, perm os.FileMode) (*os.File, error)
// flag: combinacion de O_* constantes con |
// perm: permisos (solo aplica si se crea el archivo)

// Append al final (log files)
f, err := os.OpenFile("app.log",
    os.O_WRONLY|os.O_CREATE|os.O_APPEND,
    0o644,
)
if err != nil {
    log.Fatal(err)
}
defer f.Close()
fmt.Fprintf(f, "[%s] %s\n", time.Now().Format(time.RFC3339), message)

// Crear archivo NUEVO (falla si ya existe)
f, err = os.OpenFile("lock.pid",
    os.O_WRONLY|os.O_CREATE|os.O_EXCL,
    0o644,
)
if err != nil {
    if os.IsExist(err) {
        log.Fatal("another instance is already running")
    }
    log.Fatal(err)
}
defer os.Remove("lock.pid") // Limpiar al salir
defer f.Close()
fmt.Fprintf(f, "%d", os.Getpid())

// Lectura y escritura (ej: actualizar archivo in-place)
f, err = os.OpenFile("counter.txt",
    os.O_RDWR|os.O_CREATE,
    0o644,
)
if err != nil {
    log.Fatal(err)
}
defer f.Close()

// Leer valor actual
data, _ := io.ReadAll(f)
count, _ := strconv.Atoi(strings.TrimSpace(string(data)))

// Rebobinar y escribir nuevo valor
f.Seek(0, io.SeekStart)
f.Truncate(0) // Limpiar contenido anterior
fmt.Fprintf(f, "%d\n", count+1)

// Escritura sincronizada (flush despues de cada write)
f, err = os.OpenFile("critical.log",
    os.O_WRONLY|os.O_CREATE|os.O_APPEND|os.O_SYNC,
    0o644,
)
// O_SYNC: cada Write espera a que los datos lleguen al disco
// Mas lento, pero garantiza que no se pierden datos
```

### 5.2 Tabla de combinaciones de flags

```
COMBINACIONES COMUNES DE FLAGS

SOLO LECTURA
  os.Open("f")                                  = O_RDONLY
  
CREAR/SOBREESCRIBIR
  os.Create("f")                                = O_RDWR|O_CREATE|O_TRUNC, 0666

APPEND (logs)
  os.OpenFile("f", O_WRONLY|O_CREATE|O_APPEND, 0644)
  
CREAR NUEVO (falla si existe — lock files, generar IDs)
  os.OpenFile("f", O_WRONLY|O_CREATE|O_EXCL, 0644)
  
LEER + ESCRIBIR (actualizar)
  os.OpenFile("f", O_RDWR|O_CREATE, 0644)
  
TRUNCAR EXISTENTE (limpiar y reescribir)
  os.OpenFile("f", O_WRONLY|O_TRUNC, 0644)

SYNC (durabilidad garantizada)
  os.OpenFile("f", O_WRONLY|O_CREATE|O_APPEND|O_SYNC, 0644)
```

---

## 6. Lectura por lineas: bufio.Scanner

`bufio.Scanner` es la forma idiomatica de leer un archivo linea por linea en Go:

### 6.1 Lectura basica por lineas

```go
f, err := os.Open("data.txt")
if err != nil {
    log.Fatal(err)
}
defer f.Close()

scanner := bufio.NewScanner(f)

lineNum := 0
for scanner.Scan() {
    lineNum++
    line := scanner.Text() // String sin \n al final
    fmt.Printf("%4d: %s\n", lineNum, line)
}

// SIEMPRE verificar error del scanner despues del loop
if err := scanner.Err(); err != nil {
    log.Fatal(err)
}
```

### 6.2 Lineas largas

```go
// Por defecto, Scanner tiene un buffer de 64KB.
// Lineas mayores a 64KB causan error: "bufio.Scanner: token too long"

// Solucion: aumentar el buffer
scanner := bufio.NewScanner(f)
scanner.Buffer(make([]byte, 0), 10*1024*1024) // Max 10MB por linea
// Primer argumento: buffer inicial (0 = dejar que Scanner lo cree)
// Segundo argumento: tamaño maximo

for scanner.Scan() {
    line := scanner.Text()
    // Procesar linea potencialmente muy larga
}
```

### 6.3 Diferentes modos de split

```go
// --- Por LINEAS (default) ---
scanner := bufio.NewScanner(f)
// scanner.Split(bufio.ScanLines)  // Implicito
for scanner.Scan() {
    line := scanner.Text()
}

// --- Por PALABRAS ---
scanner = bufio.NewScanner(f)
scanner.Split(bufio.ScanWords)
wordCount := 0
for scanner.Scan() {
    word := scanner.Text()
    wordCount++
}
fmt.Printf("Total palabras: %d\n", wordCount)

// --- Por BYTES ---
scanner = bufio.NewScanner(f)
scanner.Split(bufio.ScanBytes)
for scanner.Scan() {
    b := scanner.Bytes()[0]
    // Procesar byte por byte
}

// --- Por RUNES (caracteres Unicode) ---
scanner = bufio.NewScanner(f)
scanner.Split(bufio.ScanRunes)
for scanner.Scan() {
    r := []rune(scanner.Text())[0]
    fmt.Printf("%c ", r)
}
```

### 6.4 Custom split function

```go
// Leer registros separados por doble newline (parrafos)
scanner := bufio.NewScanner(f)
scanner.Split(func(data []byte, atEOF bool) (advance int, token []byte, err error) {
    // Buscar doble newline
    if i := bytes.Index(data, []byte("\n\n")); i >= 0 {
        return i + 2, bytes.TrimSpace(data[:i]), nil
    }
    // Si estamos en EOF y hay datos, retornar lo que queda
    if atEOF && len(data) > 0 {
        return len(data), bytes.TrimSpace(data), nil
    }
    // Necesitamos mas datos
    return 0, nil, nil
})

for scanner.Scan() {
    paragraph := scanner.Text()
    fmt.Printf("--- Parrafo ---\n%s\n\n", paragraph)
}

// Leer registros delimitados por un string custom
func splitByDelimiter(delim string) bufio.SplitFunc {
    delimBytes := []byte(delim)
    return func(data []byte, atEOF bool) (int, []byte, error) {
        if i := bytes.Index(data, delimBytes); i >= 0 {
            return i + len(delimBytes), data[:i], nil
        }
        if atEOF && len(data) > 0 {
            return len(data), data, nil
        }
        return 0, nil, nil
    }
}

// Uso: registros separados por "---"
scanner.Split(splitByDelimiter("---\n"))
```

### 6.5 Scanner vs ReadString vs ReadLine

```
COMPARACION DE METODOS DE LECTURA POR LINEAS

bufio.Scanner (RECOMENDADO):
  ✓ API limpia: for scanner.Scan() { scanner.Text() }
  ✓ Maneja \n, \r\n, \r automaticamente
  ✓ Retorna string sin delimitador
  ✓ Configurable (split function, buffer size)
  ✗ Limite de tamaño de linea (configurable con Buffer())
  ✗ No se puede leer y escribir al mismo tiempo

bufio.Reader.ReadString('\n'):
  ✓ Sin limite de tamaño
  ✓ Puede usarse con ReadBytes para binario
  ✗ Retorna string CON \n (necesitas TrimRight)
  ✗ El delimitador queda incluido
  ✗ No maneja \r\n automaticamente

bufio.Reader.ReadLine():
  ✗ DEPRECATED — la documentacion dice "use Scanner instead"
  ✗ Puede retornar lineas parciales (isPrefix)
  ✗ API confusa
```

---

## 7. Escritura buffered: bufio.Writer

### 7.1 Basico

```go
f, err := os.Create("output.txt")
if err != nil {
    log.Fatal(err)
}
defer f.Close()

// Crear writer buffered (4KB buffer por defecto)
bw := bufio.NewWriter(f)

// Escribir de diferentes formas
bw.WriteString("linea como string\n")
bw.Write([]byte("linea como bytes\n"))
bw.WriteByte('\n')                      // Un solo byte
bw.WriteRune('☺')                       // Un solo rune (puede ser multi-byte)
fmt.Fprintf(bw, "formato: %d\n", 42)   // Printf a un writer

// CRITICO: FLUSH antes de cerrar
if err := bw.Flush(); err != nil {
    log.Fatal(err)
}
// Sin Flush(), los ultimos datos en el buffer se PIERDEN

// Patron con defer:
bw = bufio.NewWriter(f)
defer bw.Flush()

for i := 0; i < 10000; i++ {
    fmt.Fprintf(bw, "linea %d\n", i)
}
// Flush se ejecuta automaticamente al salir
```

### 7.2 Buffer size personalizado

```go
// Buffer de 256KB (para escritura intensiva)
bw := bufio.NewWriterSize(f, 256*1024)

// Verificar capacidad:
fmt.Println("Disponible:", bw.Available())  // Bytes libres en el buffer
fmt.Println("Buffered:", bw.Buffered())     // Bytes acumulados sin flush

// Flush manual cuando quieras garantizar escritura
bw.WriteString("datos criticos\n")
bw.Flush() // Forzar flush ahora
```

### 7.3 Cuando bufio.Writer mejora rendimiento

```go
// Sin bufio: cada WriteString = 1 syscall
func writeSlow(f *os.File, n int) {
    for i := 0; i < n; i++ {
        f.WriteString("x") // 1 syscall por caracter!
    }
}

// Con bufio: los writes se acumulan, ~1 syscall por 4KB
func writeFast(f *os.File, n int) {
    bw := bufio.NewWriter(f)
    defer bw.Flush()
    for i := 0; i < n; i++ {
        bw.WriteString("x") // Se acumula en el buffer
    }
    // Un solo flush al final (o automatico cada 4KB)
}

// Benchmark tipico para 1,000,000 writes de 1 byte:
// Sin bufio:  ~2.5 segundos (1M syscalls)
// Con bufio:  ~0.02 segundos (~250 syscalls)
// Mejora: ~100x
```

---

## 8. Informacion de archivos: os.Stat

### 8.1 os.FileInfo

```go
info, err := os.Stat("myfile.txt")
if err != nil {
    if os.IsNotExist(err) {
        fmt.Println("no existe")
        return
    }
    log.Fatal(err)
}

fmt.Println("Nombre:", info.Name())            // "myfile.txt" (solo nombre, no path)
fmt.Println("Tamaño:", info.Size())            // bytes
fmt.Println("Permisos:", info.Mode())          // "-rw-r--r--"
fmt.Println("Es directorio:", info.IsDir())
fmt.Println("Modificado:", info.ModTime())      // time.Time
fmt.Println("Sistema:", info.Sys())            // *syscall.Stat_t en Linux

// Mode flags:
mode := info.Mode()
fmt.Println("Es regular:", mode.IsRegular())    // Archivo regular (no dir, no link)
fmt.Println("Es dir:", mode.IsDir())
fmt.Println("Permisos:", mode.Perm())          // Solo la parte rwxrwxrwx (0o644)
fmt.Println("Es symlink:", mode&os.ModeSymlink != 0)
fmt.Println("Es setuid:", mode&os.ModeSetuid != 0)
```

### 8.2 os.Stat vs os.Lstat

```go
// os.Stat SIGUE symlinks
// Si "link.txt" es un symlink a "target.txt":
info, _ := os.Stat("link.txt")
fmt.Println(info.Name()) // "link.txt"
fmt.Println(info.Size()) // Tamaño de TARGET.txt
fmt.Println(info.Mode()) // Permisos de TARGET.txt

// os.Lstat NO sigue symlinks
info, _ = os.Lstat("link.txt")
fmt.Println(info.Name()) // "link.txt"
fmt.Println(info.Size()) // Tamaño del LINK (la ruta del target)
fmt.Println(info.Mode()) // Lrwxrwxrwx (L = symlink)
```

### 8.3 Verificar existencia de archivos

```go
// Patron: verificar si un archivo existe
func fileExists(path string) bool {
    _, err := os.Stat(path)
    return err == nil
}

// Patron: verificar si es directorio
func isDir(path string) bool {
    info, err := os.Stat(path)
    return err == nil && info.IsDir()
}

// CUIDADO: TOCTOU race condition
// Estos patrones tienen un problema: entre el Stat() y el Open(),
// otro proceso puede crear/borrar el archivo.
//
// MEJOR: intentar la operacion directamente y manejar el error
//
// MAL:
if fileExists("data.txt") {
    data, _ := os.ReadFile("data.txt") // Puede fallar aqui
}
//
// BIEN:
data, err := os.ReadFile("data.txt")
if err != nil {
    if os.IsNotExist(err) {
        // Manejar no-existencia
    }
    return err
}
```

### 8.4 Informacion del archivo abierto

```go
// f.Stat() — stat sobre un archivo ya abierto
f, _ := os.Open("data.txt")
defer f.Close()

info, err := f.Stat()
if err != nil {
    log.Fatal(err)
}
fmt.Printf("Leyendo %s (%d bytes)\n", info.Name(), info.Size())

// f.Name() — nombre con el que se abrio
fmt.Println("Path:", f.Name()) // "data.txt" (como se paso a Open)

// f.Fd() — file descriptor del OS (para syscalls de bajo nivel)
fmt.Println("FD:", f.Fd())
```

---

## 9. Operaciones en el filesystem

### 9.1 Crear directorios

```go
// Crear un solo directorio (el padre debe existir)
err := os.Mkdir("logs", 0o755)
if err != nil {
    // os.IsExist(err) si ya existe
    log.Fatal(err)
}

// Crear toda la jerarquia (como mkdir -p)
err = os.MkdirAll("data/cache/thumbnails", 0o755)
// No falla si ya existen los directorios
```

### 9.2 Renombrar y mover

```go
// Renombrar (mismo directorio)
err := os.Rename("old_name.txt", "new_name.txt")

// Mover a otro directorio (funciona si estan en el mismo filesystem)
err = os.Rename("file.txt", "archive/file.txt")
// NOTA: os.Rename NO funciona entre diferentes filesystems
// Para cross-filesystem, hay que copiar + borrar

// Cross-filesystem move (manual):
func moveFile(src, dst string) error {
    // Intentar rename primero (rapido si mismo filesystem)
    if err := os.Rename(src, dst); err == nil {
        return nil
    }
    
    // Fallback: copiar + borrar
    in, err := os.Open(src)
    if err != nil {
        return err
    }
    defer in.Close()
    
    out, err := os.Create(dst)
    if err != nil {
        return err
    }
    
    if _, err := io.Copy(out, in); err != nil {
        out.Close()
        os.Remove(dst) // Limpiar copia parcial
        return err
    }
    
    if err := out.Close(); err != nil {
        return err
    }
    
    return os.Remove(src) // Borrar original solo si la copia fue exitosa
}
```

### 9.3 Eliminar

```go
// Eliminar un archivo o directorio VACIO
err := os.Remove("file.txt")
err = os.Remove("empty_dir") // Solo si esta vacio

// Eliminar recursivamente (como rm -rf)
err = os.RemoveAll("directory")
// CUIDADO: no pide confirmacion, no va a la papelera
// Si path no existe, RemoveAll retorna nil (no error)
// Si path es "", RemoveAll retorna error (proteccion contra borrar ".")
```

### 9.4 Symlinks

```go
// Crear symlink
err := os.Symlink("target.txt", "link.txt")
// link.txt → target.txt

// Leer target de un symlink
target, err := os.Readlink("link.txt")
fmt.Println("Target:", target) // "target.txt"

// Verificar si es symlink (usar Lstat, no Stat)
info, err := os.Lstat("link.txt")
if err == nil && info.Mode()&os.ModeSymlink != 0 {
    fmt.Println("Es un symlink")
}

// Resolver path real (seguir todos los symlinks)
realPath, err := filepath.EvalSymlinks("link.txt")
```

### 9.5 Archivos y directorios temporales

```go
// Crear archivo temporal
// Patron: os.CreateTemp(dir, pattern)
// Si dir == "", usa os.TempDir() (tipicamente /tmp)
// El * en pattern se reemplaza con un string aleatorio

tmpFile, err := os.CreateTemp("", "myapp-*.txt")
if err != nil {
    log.Fatal(err)
}
defer os.Remove(tmpFile.Name()) // Limpiar al salir
defer tmpFile.Close()

fmt.Println("Temp file:", tmpFile.Name())
// Ej: /tmp/myapp-a1b2c3.txt

tmpFile.WriteString("datos temporales\n")

// Crear en un directorio especifico:
tmpFile2, err := os.CreateTemp("./cache", "download-*.dat")

// Crear directorio temporal
tmpDir, err := os.MkdirTemp("", "myapp-*")
if err != nil {
    log.Fatal(err)
}
defer os.RemoveAll(tmpDir) // Limpiar al salir

fmt.Println("Temp dir:", tmpDir)
// Ej: /tmp/myapp-x7y8z9

// Crear archivos dentro del directorio temporal
f, _ := os.Create(filepath.Join(tmpDir, "data.json"))
```

### 9.6 Cambiar permisos y ownership

```go
// Cambiar permisos
err := os.Chmod("script.sh", 0o755) // Hacerlo ejecutable

// Cambiar owner (requiere root o capacidades especiales)
err = os.Chown("file.txt", uid, gid)

// Cambiar tiempos de acceso/modificacion
err = os.Chtimes("file.txt",
    time.Now(),                    // Ultimo acceso
    time.Now().Add(-24*time.Hour), // Ultima modificacion
)
```

---

## 10. Listar y recorrer directorios

### 10.1 os.ReadDir — listar un directorio

```go
// os.ReadDir retorna []os.DirEntry, ordenado por nombre
entries, err := os.ReadDir(".")
if err != nil {
    log.Fatal(err)
}

for _, entry := range entries {
    kind := "FILE"
    if entry.IsDir() {
        kind = "DIR "
    }
    
    // entry.Info() llama Stat() — lazy, solo cuando lo necesitas
    info, err := entry.Info()
    if err != nil {
        continue
    }
    
    fmt.Printf("%-4s %-30s %10d %s\n",
        kind,
        entry.Name(),
        info.Size(),
        info.ModTime().Format("2006-01-02 15:04"),
    )
}

// DirEntry vs FileInfo:
// DirEntry: Name(), IsDir(), Type() — sin Stat syscall (rapido)
// FileInfo: Name(), Size(), Mode(), ModTime(), IsDir() — con Stat (mas lento)
// entry.Info() convierte DirEntry → FileInfo
```

### 10.2 filepath.WalkDir — recorrer recursivamente

```go
// filepath.WalkDir (Go 1.16+) — mas eficiente que filepath.Walk
err := filepath.WalkDir(".", func(path string, d fs.DirEntry, err error) error {
    // err no nil = error accediendo a este path
    if err != nil {
        fmt.Fprintf(os.Stderr, "error: %s: %v\n", path, err)
        return nil // Continuar a pesar del error
        // return err para parar completamente
    }
    
    // Saltar directorios ocultos
    if d.IsDir() && strings.HasPrefix(d.Name(), ".") {
        return filepath.SkipDir // No entrar en este directorio
    }
    
    // Saltar directorios de vendoring
    if d.IsDir() && (d.Name() == "vendor" || d.Name() == "node_modules") {
        return filepath.SkipDir
    }
    
    // Procesar solo archivos .go
    if !d.IsDir() && strings.HasSuffix(d.Name(), ".go") {
        info, err := d.Info()
        if err != nil {
            return nil
        }
        fmt.Printf("%-50s %8d bytes\n", path, info.Size())
    }
    
    return nil
})

if err != nil {
    log.Fatal(err)
}
```

### 10.3 filepath.Glob — buscar con patron

```go
// Buscar archivos con patron glob (*, ?, [...])
matches, err := filepath.Glob("*.go")
// matches = ["main.go", "handler.go", "handler_test.go"]

matches, _ = filepath.Glob("cmd/*/main.go")
// matches = ["cmd/server/main.go", "cmd/worker/main.go"]

matches, _ = filepath.Glob("data/[0-9]*.csv")
// matches = ["data/001.csv", "data/42_results.csv"]

// NOTA: filepath.Glob NO soporta ** (recursivo)
// Para busqueda recursiva, usa filepath.WalkDir con filtro
```

### 10.4 Buscar archivos recursivamente (patron comun)

```go
// Encontrar todos los archivos con una extension
func findFiles(root string, ext string) ([]string, error) {
    var files []string
    err := filepath.WalkDir(root, func(path string, d fs.DirEntry, err error) error {
        if err != nil {
            return nil // Saltar errores
        }
        if !d.IsDir() && filepath.Ext(path) == ext {
            files = append(files, path)
        }
        return nil
    })
    return files, err
}

// Uso:
goFiles, _ := findFiles(".", ".go")
for _, f := range goFiles {
    fmt.Println(f)
}

// Calcular tamaño total de un directorio
func dirSize(path string) (int64, error) {
    var total int64
    err := filepath.WalkDir(path, func(_ string, d fs.DirEntry, err error) error {
        if err != nil || d.IsDir() {
            return nil
        }
        info, err := d.Info()
        if err != nil {
            return nil
        }
        total += info.Size()
        return nil
    })
    return total, err
}
```

---

## 11. Patrones avanzados

### 11.1 Escritura atomica (write-rename)

El problema: si el programa crashea mientras esta escribiendo un archivo, el archivo queda corrupto. La solucion es escribir a un archivo temporal y luego renombrar (rename es atomico en la mayoria de filesystems):

```go
// Escritura atomica: write temp → sync → rename
func writeFileAtomic(path string, data []byte, perm os.FileMode) error {
    // 1. Crear archivo temporal en el MISMO directorio (necesario para rename)
    dir := filepath.Dir(path)
    tmp, err := os.CreateTemp(dir, filepath.Base(path)+".tmp.*")
    if err != nil {
        return fmt.Errorf("create temp: %w", err)
    }
    tmpPath := tmp.Name()
    
    // 2. Cleanup en caso de error
    success := false
    defer func() {
        if !success {
            tmp.Close()
            os.Remove(tmpPath)
        }
    }()
    
    // 3. Escribir datos
    if _, err := tmp.Write(data); err != nil {
        return fmt.Errorf("write: %w", err)
    }
    
    // 4. Sync al disco (asegurar que los datos estan fisicamente escritos)
    if err := tmp.Sync(); err != nil {
        return fmt.Errorf("sync: %w", err)
    }
    
    // 5. Cerrar (tambien flushea buffers del OS)
    if err := tmp.Close(); err != nil {
        return fmt.Errorf("close: %w", err)
    }
    
    // 6. Establecer permisos
    if err := os.Chmod(tmpPath, perm); err != nil {
        return fmt.Errorf("chmod: %w", err)
    }
    
    // 7. Rename atomico
    if err := os.Rename(tmpPath, path); err != nil {
        return fmt.Errorf("rename: %w", err)
    }
    
    success = true
    return nil
}

// Uso:
data, _ := json.MarshalIndent(config, "", "  ")
err := writeFileAtomic("config.json", data, 0o644)
// Si el proceso crashea a mitad de escritura:
// - config.json sigue intacto (la version anterior)
// - Solo queda un .tmp.* suelto que se puede limpiar
```

### 11.2 Lock files

```go
// Lock file para prevenir multiples instancias
func acquireLock(lockPath string) (*os.File, error) {
    f, err := os.OpenFile(lockPath,
        os.O_CREATE|os.O_EXCL|os.O_WRONLY,
        0o644,
    )
    if err != nil {
        if os.IsExist(err) {
            // Verificar si el proceso que tiene el lock sigue vivo
            data, readErr := os.ReadFile(lockPath)
            if readErr == nil {
                pid, _ := strconv.Atoi(strings.TrimSpace(string(data)))
                if pid > 0 {
                    // Verificar si el proceso existe
                    process, err := os.FindProcess(pid)
                    if err == nil {
                        // En Unix, Signal(0) verifica si el proceso existe
                        if process.Signal(syscall.Signal(0)) == nil {
                            return nil, fmt.Errorf("another instance running (PID %d)", pid)
                        }
                    }
                }
                // El proceso ya no existe — lock stale, limpiar
                os.Remove(lockPath)
                return acquireLock(lockPath) // Reintentar
            }
            return nil, fmt.Errorf("lock file exists: %s", lockPath)
        }
        return nil, err
    }
    
    // Escribir nuestro PID
    fmt.Fprintf(f, "%d\n", os.Getpid())
    return f, nil
}

func releaseLock(f *os.File) {
    path := f.Name()
    f.Close()
    os.Remove(path)
}

// Uso:
func main() {
    lock, err := acquireLock("/tmp/myapp.lock")
    if err != nil {
        log.Fatal(err)
    }
    defer releaseLock(lock)
    
    // Ejecutar la aplicacion...
}
```

### 11.3 Tail follow (como tail -f)

```go
// Seguir un archivo que crece (log file)
func tailFollow(path string, lines chan<- string) error {
    f, err := os.Open(path)
    if err != nil {
        return err
    }
    defer f.Close()
    
    // Ir al final del archivo
    f.Seek(0, io.SeekEnd)
    
    reader := bufio.NewReader(f)
    
    for {
        line, err := reader.ReadString('\n')
        if err != nil {
            if err == io.EOF {
                // No hay mas datos — esperar
                time.Sleep(100 * time.Millisecond)
                continue
            }
            return err
        }
        lines <- strings.TrimRight(line, "\n")
    }
}

// Uso:
lines := make(chan string)
go tailFollow("/var/log/app.log", lines)

for line := range lines {
    fmt.Println(line)
}
```

### 11.4 Copiar archivos preservando metadata

```go
func copyFile(src, dst string) error {
    // Abrir source
    in, err := os.Open(src)
    if err != nil {
        return err
    }
    defer in.Close()
    
    // Obtener info del source
    srcInfo, err := in.Stat()
    if err != nil {
        return err
    }
    
    // Crear destino con los mismos permisos
    out, err := os.OpenFile(dst, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, srcInfo.Mode().Perm())
    if err != nil {
        return err
    }
    
    // Copiar contenido
    if _, err := io.Copy(out, in); err != nil {
        out.Close()
        return err
    }
    
    // Cerrar y verificar
    if err := out.Close(); err != nil {
        return err
    }
    
    // Preservar timestamps
    return os.Chtimes(dst, srcInfo.ModTime(), srcInfo.ModTime())
}

// Copiar directorio recursivamente
func copyDir(src, dst string) error {
    srcInfo, err := os.Stat(src)
    if err != nil {
        return err
    }
    
    if err := os.MkdirAll(dst, srcInfo.Mode().Perm()); err != nil {
        return err
    }
    
    entries, err := os.ReadDir(src)
    if err != nil {
        return err
    }
    
    for _, entry := range entries {
        srcPath := filepath.Join(src, entry.Name())
        dstPath := filepath.Join(dst, entry.Name())
        
        if entry.IsDir() {
            if err := copyDir(srcPath, dstPath); err != nil {
                return err
            }
        } else {
            if err := copyFile(srcPath, dstPath); err != nil {
                return err
            }
        }
    }
    
    return nil
}
```

### 11.5 Lectura de archivos binarios con formato

```go
// Leer un archivo binario con formato conocido
// Ejemplo: BMP header (14 bytes)
type BMPHeader struct {
    Magic    [2]byte // "BM"
    FileSize uint32
    Reserved uint32
    Offset   uint32
}

func readBMPHeader(path string) (*BMPHeader, error) {
    f, err := os.Open(path)
    if err != nil {
        return nil, err
    }
    defer f.Close()
    
    var header BMPHeader
    err = binary.Read(f, binary.LittleEndian, &header)
    if err != nil {
        return nil, err
    }
    
    if header.Magic != [2]byte{'B', 'M'} {
        return nil, fmt.Errorf("not a BMP file: magic = %q", header.Magic)
    }
    
    return &header, nil
}

// Escribir datos binarios
func writeBMPHeader(f *os.File, header *BMPHeader) error {
    return binary.Write(f, binary.LittleEndian, header)
}
```

---

## 12. fs.FS — filesystem abstracto (Go 1.16+)

### 12.1 La interface fs.FS

Go 1.16 introdujo `io/fs` — una abstraccion de filesystem que permite operar sobre diferentes "fuentes de archivos" con la misma interface:

```go
// fs.FS es la interface base
type FS interface {
    Open(name string) (fs.File, error)
}

// fs.File es la interface de un archivo abierto
type File interface {
    Stat() (FileInfo, error)
    Read([]byte) (int, error)
    Close() error
}
```

```go
// Implementaciones de fs.FS:
// os.DirFS(".")        → filesystem real (directorio como raiz)
// embed.FS             → archivos embebidos en el binario
// zip.Reader           → archivos dentro de un ZIP
// fstest.MapFS         → filesystem en memoria (para tests)
// http.FS(fs)          → wrapper para servir via HTTP

// Funciones que operan sobre cualquier fs.FS:
fs.ReadFile(fsys, "config.json")        // Leer archivo
fs.ReadDir(fsys, ".")                    // Listar directorio
fs.WalkDir(fsys, ".", walkFn)            // Recorrer recursivamente
fs.Glob(fsys, "*.go")                   // Buscar con glob
fs.Stat(fsys, "file.txt")               // Info del archivo
fs.Sub(fsys, "subdir")                  // Sub-filesystem
```

### 12.2 os.DirFS — filesystem real

```go
// Crear un FS radicado en un directorio
fsys := os.DirFS("/etc")

// Leer archivo dentro de ese FS
data, err := fs.ReadFile(fsys, "hostname") // Lee /etc/hostname
if err != nil {
    log.Fatal(err)
}
fmt.Println(string(data))

// Listar directorio
entries, err := fs.ReadDir(fsys, ".")
for _, e := range entries {
    fmt.Println(e.Name())
}

// NOTA: os.DirFS no permite escapar del directorio raiz
// fs.ReadFile(fsys, "../etc/passwd") → error
// Esto es util para seguridad (sandbox de filesystem)
```

### 12.3 embed.FS — archivos embebidos

```go
import "embed"

//go:embed templates/*
var templateFS embed.FS

func main() {
    // templateFS implementa fs.FS
    data, err := fs.ReadFile(templateFS, "templates/index.html")
    if err != nil {
        log.Fatal(err)
    }
    fmt.Println(string(data))
    
    // Servir via HTTP
    http.Handle("/", http.FileServer(http.FS(templateFS)))
}
```

### 12.4 fstest.MapFS — filesystem en memoria (testing)

```go
import "testing/fstest"

func TestProcess(t *testing.T) {
    // Crear un filesystem falso en memoria
    fsys := fstest.MapFS{
        "config.json": &fstest.MapFile{
            Data: []byte(`{"port": 8080}`),
        },
        "data/users.csv": &fstest.MapFile{
            Data: []byte("id,name\n1,Alice\n2,Bob\n"),
        },
        "empty_dir": &fstest.MapFile{
            Mode: fs.ModeDir,
        },
    }
    
    // Usar el FS falso en tu funcion
    result, err := processFiles(fsys)
    if err != nil {
        t.Fatal(err)
    }
    // Verificar resultado...
    
    // Verificar que el FS es valido
    if err := fstest.TestFS(fsys, "config.json", "data/users.csv"); err != nil {
        t.Fatal(err)
    }
}

// La funcion que quieres testear acepta fs.FS en vez de paths:
func processFiles(fsys fs.FS) (*Result, error) {
    data, err := fs.ReadFile(fsys, "config.json")
    if err != nil {
        return nil, err
    }
    // ...
    return &Result{}, nil
}
```

### 12.5 Escribir codigo que acepta fs.FS

```go
// PATRON: aceptar fs.FS para testabilidad
// En produccion: os.DirFS("./data")
// En tests: fstest.MapFS{...}
// Con archivos embebidos: embed.FS

type Server struct {
    templates fs.FS
    static    fs.FS
}

func NewServer(templates, static fs.FS) *Server {
    return &Server{templates: templates, static: static}
}

func (s *Server) renderTemplate(name string) (string, error) {
    data, err := fs.ReadFile(s.templates, name)
    if err != nil {
        return "", err
    }
    return string(data), nil
}

// Produccion:
server := NewServer(
    os.DirFS("./templates"),
    os.DirFS("./static"),
)

// Test:
server := NewServer(
    fstest.MapFS{
        "index.html": &fstest.MapFile{Data: []byte("<html>test</html>")},
    },
    fstest.MapFS{},
)
```

---

## 13. Comparacion con C y Rust

```
┌──────────────────────────┬───────────────────────────┬────────────────────────┬──────────────────────────┐
│ Operacion                 │ Go                        │ C                      │ Rust                     │
├──────────────────────────┼───────────────────────────┼────────────────────────┼──────────────────────────┤
│ Leer todo el archivo     │ os.ReadFile(path)         │ fopen + fread + fclose │ fs::read_to_string(path) │
│                           │                           │ (manual)               │ fs::read(path)           │
│ Escribir archivo completo│ os.WriteFile(path,d,perm) │ fopen + fwrite + fclose│ fs::write(path, data)    │
│ Abrir solo lectura       │ os.Open(path)             │ fopen(path,"r")        │ File::open(path)         │
│                           │                           │ open(path,O_RDONLY)    │                          │
│ Crear/truncar            │ os.Create(path)           │ fopen(path,"w")        │ File::create(path)       │
│ Abrir con flags          │ os.OpenFile(p,flags,perm) │ open(p,flags,perm)     │ OpenOptions::new()...    │
│ Append                   │ O_WRONLY|O_CREATE|O_APPEND│ fopen(p,"a")           │ .append(true).open(p)    │
│ Cerrar                   │ f.Close()                 │ fclose(f)/close(fd)    │ Drop (automatico)        │
│ Leer bytes               │ f.Read(buf)               │ fread(buf,1,n,f)       │ f.read(&mut buf)         │
│ Escribir bytes           │ f.Write(buf)              │ fwrite(buf,1,n,f)      │ f.write(buf)             │
│ Escribir string          │ f.WriteString(s)          │ fputs(s,f)             │ f.write_all(s.as_bytes())│
│ Printf a archivo         │ fmt.Fprintf(f,...)        │ fprintf(f,...)         │ write!(f, ...)           │
│ Seek                     │ f.Seek(off,whence)        │ fseek(f,off,whence)    │ f.seek(SeekFrom::...)    │
│ Stat                     │ os.Stat(path)             │ stat(path,&sb)         │ fs::metadata(path)       │
│ Leer por lineas          │ bufio.NewScanner(f)       │ fgets()/getline()      │ BufReader::lines()       │
│ Writer buffered          │ bufio.NewWriter(f)        │ setvbuf() / fprintf    │ BufWriter::new(f)        │
│ Flush                    │ bw.Flush()                │ fflush(f)              │ bw.flush()               │
│ Crear directorio         │ os.Mkdir(p,perm)          │ mkdir(p,perm)          │ fs::create_dir(p)        │
│ Crear dirs recursivo     │ os.MkdirAll(p,perm)       │ mkdir -p (manual)      │ fs::create_dir_all(p)    │
│ Listar directorio        │ os.ReadDir(p)             │ opendir()+readdir()    │ fs::read_dir(p)          │
│ Recorrer recursivo       │ filepath.WalkDir(p,fn)    │ nftw()/manual          │ walkdir crate            │
│ Eliminar                 │ os.Remove(p)              │ remove(p)/unlink(p)    │ fs::remove_file(p)       │
│ Eliminar recursivo       │ os.RemoveAll(p)           │ nftw()+unlink (manual) │ fs::remove_dir_all(p)    │
│ Renombrar                │ os.Rename(old,new)        │ rename(old,new)        │ fs::rename(old,new)      │
│ Symlink                  │ os.Symlink(target,link)   │ symlink(target,link)   │ unix::fs::symlink(t,l)   │
│ Temp file                │ os.CreateTemp(dir,pat)    │ mkstemp(template)      │ tempfile crate           │
│ Temp dir                 │ os.MkdirTemp(dir,pat)     │ mkdtemp(template)      │ tempfile crate           │
│ Filesystem abstraccion   │ fs.FS (interface)         │ No                     │ No std (vfs crate)       │
│ Cierre automatico        │ No (defer manual)         │ No (manual)            │ Si (Drop trait)          │
│ Escritura atomica        │ Manual (write+rename)     │ Manual                 │ Manual (o atomicwrites)  │
│ File locking             │ syscall.Flock (Unix)      │ flock()/fcntl()        │ fs2 crate                │
└──────────────────────────┴───────────────────────────┴────────────────────────┴──────────────────────────┘
```

```
DIFERENCIA CLAVE: CIERRE DE ARCHIVOS

C:    fclose(f) — manual, el programador debe recordar
Go:   defer f.Close() — patron manual pero convencional, se olvida a veces
Rust: Drop trait — AUTOMATICO cuando la variable sale del scope

      {
          let f = File::open("data.txt")?;
          // usar f...
      } // f se cierra AUTOMATICAMENTE aqui (Drop)
      
      // En Go necesitas:
      f, _ := os.Open("data.txt")
      defer f.Close() // Manual, pero al menos defer ayuda

DIFERENCIA CLAVE: FILESYSTEM ABSTRACTO

Go tiene fs.FS en la stdlib (Go 1.16+):
  - Facilita testing (fstest.MapFS en memoria)
  - Facilita embed (embed.FS)
  - Facilita sandboxing (os.DirFS limita acceso)
  
C y Rust no tienen esto en sus stdlib — necesitas abstracciones manuales.
```

---

## 14. Programa completo: File Manager CLI

Este programa demuestra todas las operaciones de archivos en un file manager interactivo de linea de comandos:

```go
package main

import (
    "bufio"
    "crypto/sha256"
    "encoding/hex"
    "fmt"
    "io"
    "io/fs"
    "os"
    "path/filepath"
    "sort"
    "strconv"
    "strings"
    "time"
)

func main() {
    if len(os.Args) < 2 {
        printUsage()
        os.Exit(1)
    }

    var err error
    switch os.Args[1] {
    case "ls":
        err = cmdList(getArg(2, "."))
    case "tree":
        err = cmdTree(getArg(2, "."), 0)
    case "cat":
        err = requireArgs(3, func() error { return cmdCat(os.Args[2]) })
    case "head":
        n, _ := strconv.Atoi(getArg(3, "10"))
        err = requireArgs(3, func() error { return cmdHead(os.Args[2], n) })
    case "wc":
        err = requireArgs(3, func() error { return cmdWordCount(os.Args[2]) })
    case "cp":
        err = requireArgs(4, func() error { return cmdCopy(os.Args[2], os.Args[3]) })
    case "mv":
        err = requireArgs(4, func() error { return cmdMove(os.Args[2], os.Args[3]) })
    case "rm":
        err = requireArgs(3, func() error { return cmdRemove(os.Args[2]) })
    case "mkdir":
        err = requireArgs(3, func() error { return cmdMkdir(os.Args[2]) })
    case "touch":
        err = requireArgs(3, func() error { return cmdTouch(os.Args[2]) })
    case "find":
        pattern := getArg(3, "*")
        err = cmdFind(getArg(2, "."), pattern)
    case "sha256":
        err = requireArgs(3, func() error { return cmdHash(os.Args[2]) })
    case "info":
        err = requireArgs(3, func() error { return cmdInfo(os.Args[2]) })
    case "write":
        err = requireArgs(4, func() error { return cmdWrite(os.Args[2], os.Args[3]) })
    case "append":
        err = requireArgs(4, func() error { return cmdAppend(os.Args[2], os.Args[3]) })
    case "du":
        err = cmdDiskUsage(getArg(2, "."))
    default:
        fmt.Fprintf(os.Stderr, "unknown command: %s\n", os.Args[1])
        printUsage()
        os.Exit(1)
    }

    if err != nil {
        fmt.Fprintf(os.Stderr, "error: %v\n", err)
        os.Exit(1)
    }
}

// --- Commands ---

func cmdList(dir string) error {
    entries, err := os.ReadDir(dir)
    if err != nil {
        return err
    }

    type entry struct {
        name    string
        size    int64
        mode    fs.FileMode
        modTime time.Time
        isDir   bool
    }

    var items []entry
    for _, e := range entries {
        info, err := e.Info()
        if err != nil {
            continue
        }
        items = append(items, entry{
            name:    e.Name(),
            size:    info.Size(),
            mode:    info.Mode(),
            modTime: info.ModTime(),
            isDir:   e.IsDir(),
        })
    }

    for _, item := range items {
        icon := " "
        if item.isDir {
            icon = "/"
        } else if item.mode&0o111 != 0 {
            icon = "*"
        }

        fmt.Printf("%s %10s  %s  %s%s\n",
            item.mode.String(),
            formatSize(item.size),
            item.modTime.Format("2006-01-02 15:04"),
            item.name,
            icon,
        )
    }

    return nil
}

func cmdTree(dir string, depth int) error {
    entries, err := os.ReadDir(dir)
    if err != nil {
        return err
    }

    if depth == 0 {
        fmt.Println(dir)
    }

    for i, entry := range entries {
        if strings.HasPrefix(entry.Name(), ".") {
            continue
        }

        prefix := strings.Repeat("│   ", depth)
        connector := "├── "
        if i == len(entries)-1 {
            connector = "└── "
        }

        name := entry.Name()
        if entry.IsDir() {
            name += "/"
        }
        fmt.Printf("%s%s%s\n", prefix, connector, name)

        if entry.IsDir() {
            cmdTree(filepath.Join(dir, entry.Name()), depth+1)
        }
    }

    return nil
}

func cmdCat(path string) error {
    f, err := os.Open(path)
    if err != nil {
        return err
    }
    defer f.Close()

    _, err = io.Copy(os.Stdout, f)
    return err
}

func cmdHead(path string, n int) error {
    f, err := os.Open(path)
    if err != nil {
        return err
    }
    defer f.Close()

    scanner := bufio.NewScanner(f)
    lineNum := 0
    for scanner.Scan() && lineNum < n {
        lineNum++
        fmt.Printf("%4d  %s\n", lineNum, scanner.Text())
    }
    return scanner.Err()
}

func cmdWordCount(path string) error {
    f, err := os.Open(path)
    if err != nil {
        return err
    }
    defer f.Close()

    var lines, words, bytes int
    scanner := bufio.NewScanner(f)
    scanner.Buffer(make([]byte, 0), 1024*1024)

    for scanner.Scan() {
        lines++
        line := scanner.Text()
        bytes += len(line) + 1 // +1 for newline
        words += len(strings.Fields(line))
    }

    if err := scanner.Err(); err != nil {
        return err
    }

    fmt.Printf("%8d lines  %8d words  %8d bytes  %s\n", lines, words, bytes, path)
    return nil
}

func cmdCopy(src, dst string) error {
    // Check if dst is a directory
    dstInfo, err := os.Stat(dst)
    if err == nil && dstInfo.IsDir() {
        dst = filepath.Join(dst, filepath.Base(src))
    }

    in, err := os.Open(src)
    if err != nil {
        return err
    }
    defer in.Close()

    srcInfo, err := in.Stat()
    if err != nil {
        return err
    }

    out, err := os.OpenFile(dst, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, srcInfo.Mode().Perm())
    if err != nil {
        return err
    }

    written, err := io.Copy(out, in)
    if cerr := out.Close(); err == nil {
        err = cerr
    }
    if err != nil {
        return err
    }

    fmt.Printf("copied %s to %s (%s)\n", src, dst, formatSize(written))
    return nil
}

func cmdMove(src, dst string) error {
    dstInfo, err := os.Stat(dst)
    if err == nil && dstInfo.IsDir() {
        dst = filepath.Join(dst, filepath.Base(src))
    }

    if err := os.Rename(src, dst); err != nil {
        return err
    }
    fmt.Printf("moved %s to %s\n", src, dst)
    return nil
}

func cmdRemove(path string) error {
    info, err := os.Stat(path)
    if err != nil {
        return err
    }

    if info.IsDir() {
        err = os.RemoveAll(path)
    } else {
        err = os.Remove(path)
    }
    if err != nil {
        return err
    }

    fmt.Printf("removed %s\n", path)
    return nil
}

func cmdMkdir(path string) error {
    if err := os.MkdirAll(path, 0o755); err != nil {
        return err
    }
    fmt.Printf("created directory %s\n", path)
    return nil
}

func cmdTouch(path string) error {
    f, err := os.OpenFile(path, os.O_CREATE|os.O_WRONLY, 0o644)
    if err != nil {
        return err
    }
    f.Close()

    now := time.Now()
    return os.Chtimes(path, now, now)
}

func cmdFind(root, pattern string) error {
    return filepath.WalkDir(root, func(path string, d fs.DirEntry, err error) error {
        if err != nil {
            return nil
        }

        if d.IsDir() && strings.HasPrefix(d.Name(), ".") && d.Name() != "." {
            return filepath.SkipDir
        }

        matched, err := filepath.Match(pattern, d.Name())
        if err != nil {
            return err
        }
        if matched {
            kind := "f"
            if d.IsDir() {
                kind = "d"
            }
            fmt.Printf("[%s] %s\n", kind, path)
        }

        return nil
    })
}

func cmdHash(path string) error {
    f, err := os.Open(path)
    if err != nil {
        return err
    }
    defer f.Close()

    h := sha256.New()
    n, err := io.Copy(h, f)
    if err != nil {
        return err
    }

    fmt.Printf("SHA256: %s  %s (%s)\n",
        hex.EncodeToString(h.Sum(nil)),
        path,
        formatSize(n),
    )
    return nil
}

func cmdInfo(path string) error {
    info, err := os.Lstat(path)
    if err != nil {
        return err
    }

    fmt.Printf("Name:     %s\n", info.Name())
    fmt.Printf("Size:     %s (%d bytes)\n", formatSize(info.Size()), info.Size())
    fmt.Printf("Mode:     %s\n", info.Mode())
    fmt.Printf("Modified: %s\n", info.ModTime().Format(time.RFC3339))
    fmt.Printf("IsDir:    %v\n", info.IsDir())

    if info.Mode()&os.ModeSymlink != 0 {
        target, err := os.Readlink(path)
        if err == nil {
            fmt.Printf("Target:   %s\n", target)
        }
    }

    abs, err := filepath.Abs(path)
    if err == nil {
        fmt.Printf("AbsPath:  %s\n", abs)
    }

    return nil
}

func cmdWrite(path, content string) error {
    err := os.WriteFile(path, []byte(content+"\n"), 0o644)
    if err != nil {
        return err
    }
    fmt.Printf("wrote %d bytes to %s\n", len(content)+1, path)
    return nil
}

func cmdAppend(path, content string) error {
    f, err := os.OpenFile(path, os.O_WRONLY|os.O_CREATE|os.O_APPEND, 0o644)
    if err != nil {
        return err
    }
    defer f.Close()

    n, err := fmt.Fprintln(f, content)
    if err != nil {
        return err
    }
    fmt.Printf("appended %d bytes to %s\n", n, path)
    return nil
}

func cmdDiskUsage(dir string) error {
    type dirSize struct {
        path string
        size int64
    }

    var sizes []dirSize
    var totalSize int64
    var fileCount int

    err := filepath.WalkDir(dir, func(path string, d fs.DirEntry, err error) error {
        if err != nil {
            return nil
        }
        if !d.IsDir() {
            info, err := d.Info()
            if err != nil {
                return nil
            }
            totalSize += info.Size()
            fileCount++

            parent := filepath.Dir(path)
            found := false
            for i := range sizes {
                if sizes[i].path == parent {
                    sizes[i].size += info.Size()
                    found = true
                    break
                }
            }
            if !found {
                sizes = append(sizes, dirSize{path: parent, size: info.Size()})
            }
        }
        return nil
    })
    if err != nil {
        return err
    }

    sort.Slice(sizes, func(i, j int) bool {
        return sizes[i].size > sizes[j].size
    })

    fmt.Printf("Disk usage for %s\n\n", dir)
    limit := 20
    if len(sizes) < limit {
        limit = len(sizes)
    }
    for _, ds := range sizes[:limit] {
        fmt.Printf("%10s  %s\n", formatSize(ds.size), ds.path)
    }
    fmt.Printf("\nTotal: %s in %d files\n", formatSize(totalSize), fileCount)
    return nil
}

// --- Helpers ---

func getArg(index int, defaultVal string) string {
    if index < len(os.Args) {
        return os.Args[index]
    }
    return defaultVal
}

func requireArgs(minArgs int, fn func() error) error {
    if len(os.Args) < minArgs {
        return fmt.Errorf("not enough arguments")
    }
    return fn()
}

func formatSize(bytes int64) string {
    const unit = 1024
    if bytes < unit {
        return fmt.Sprintf("%d B", bytes)
    }
    div, exp := int64(unit), 0
    for n := bytes / unit; n >= unit; n /= unit {
        div *= unit
        exp++
    }
    return fmt.Sprintf("%.1f %ciB", float64(bytes)/float64(div), "KMGTPE"[exp])
}

func printUsage() {
    fmt.Println("Usage: fm <command> [args...]")
    fmt.Println()
    fmt.Println("File operations:")
    fmt.Println("  cat <file>              Print file contents")
    fmt.Println("  head <file> [n]         Print first n lines (default: 10)")
    fmt.Println("  wc <file>               Count lines, words, bytes")
    fmt.Println("  write <file> <content>  Write content to file")
    fmt.Println("  append <file> <content> Append content to file")
    fmt.Println("  cp <src> <dst>          Copy file")
    fmt.Println("  mv <src> <dst>          Move/rename file")
    fmt.Println("  rm <path>               Remove file or directory")
    fmt.Println("  touch <file>            Create empty file or update timestamp")
    fmt.Println("  sha256 <file>           Compute SHA256 hash")
    fmt.Println()
    fmt.Println("Directory operations:")
    fmt.Println("  ls [dir]                List directory (default: .)")
    fmt.Println("  tree [dir]              Show directory tree")
    fmt.Println("  mkdir <path>            Create directory (recursive)")
    fmt.Println("  find <dir> [pattern]    Find files matching glob pattern")
    fmt.Println("  du [dir]                Disk usage by directory")
    fmt.Println()
    fmt.Println("Info:")
    fmt.Println("  info <path>             Show file/directory information")
}
```

```
CONCEPTOS DEMOSTRADOS EN EL PROGRAMA:

LECTURA:
  os.Open           → cat, head, wc, sha256, cp
  os.ReadDir         → ls
  io.Copy            → cat (a stdout), sha256 (a hash), cp
  bufio.Scanner      → head, wc (por lineas)
  filepath.WalkDir   → tree, find, du

ESCRITURA:
  os.WriteFile       → write
  os.OpenFile+APPEND → append
  os.Create          → cp (destino)
  fmt.Fprintf        → formateo a Writer

FILESYSTEM:
  os.Stat/Lstat      → info, ls
  os.Rename          → mv
  os.Remove/All      → rm
  os.MkdirAll        → mkdir
  os.Chtimes         → touch
  filepath.Match     → find
  filepath.Abs       → info

I/O PATTERNS:
  io.Copy para streaming (sin cargar en memoria)
  bufio.Scanner para lectura por lineas
  defer f.Close() en todas las operaciones
  Manejo de error de Close en cp (escritura)
  sha256.Hash como io.Writer (alimentado via io.Copy)
```

---

## 15. Ejercicios

### Ejercicio 1: Implementar tail
Implementa un comando `tail` que:
- `tail <file>` — muestre las ultimas 10 lineas
- `tail -n 20 <file>` — muestre las ultimas 20 lineas
- `tail -f <file>` — siga el archivo (como `tail -f` en Unix)
Para las ultimas N lineas, usa un ring buffer de N strings con `bufio.Scanner`. Para `-f`, usa `Seek` al final y luego un loop con `ReadString` y `time.Sleep`.

### Ejercicio 2: Escritura atomica con backup
Implementa una funcion `SafeWriteFile(path string, data []byte) error` que:
- Si el archivo ya existe, cree un backup (`path.bak`) antes de sobreescribir
- Use escritura atomica (write-temp + sync + rename)
- Verifique que los datos se escribieron correctamente (relee y compara SHA256)
- Si algo falla, el archivo original queda intacto

### Ejercicio 3: Buscador de archivos duplicados
Escribe un programa que reciba un directorio como argumento y encuentre archivos duplicados:
- Recorre el directorio recursivamente con `filepath.WalkDir`
- Agrupa archivos por tamaño (primer filtro rapido — `os.Stat`)
- Para archivos del mismo tamaño, compara SHA256 (usa `io.Copy` a `sha256.New()`)
- Muestra los grupos de duplicados con sus paths y tamaños

### Ejercicio 4: Log rotator
Implementa un `RotatingWriter` que:
- Escriba a un archivo de log
- Cuando el archivo supere un tamaño maximo (ej: 10MB), lo renombre a `name.1.log`, `name.2.log`, etc.
- Mantenga un maximo de N archivos rotados (borrando los mas viejos)
- Implemente `io.Writer` para que se pueda usar con `log.SetOutput`
- Use un `sync.Mutex` para ser goroutine-safe

---

> **Siguiente**: C09/S02 - I/O, T03 - Encoding — encoding/json (Marshal, Unmarshal, tags, Decoder/Encoder streaming), encoding/csv, encoding/xml
