# Linea de comandos — os.Args, flag package, cobra (tercero), stdin/stdout/stderr

## 1. Introduccion

Go es uno de los lenguajes mas populares para escribir herramientas de linea de comandos. La stdlib incluye soporte basico con `os.Args` y el paquete `flag`, mientras que el ecosistema ofrece frameworks como `cobra` para CLIs complejas. Ademas, Go maneja stdin/stdout/stderr como `io.Reader`/`io.Writer`, permitiendo composicion con pipes Unix.

```
┌───────────────────────────────────────────────────────────────────────────────────┐
│                    CLI EN GO — NIVELES DE ABSTRACCION                            │
├───────────────────────────────────────────────────────────────────────────────────┤
│                                                                                   │
│  NIVEL 1: RAW (os.Args)                                                          │
│  ├─ os.Args []string       → argumentos crudos del proceso                      │
│  ├─ os.Stdin  *os.File     → io.Reader desde terminal o pipe                    │
│  ├─ os.Stdout *os.File     → io.Writer hacia terminal o pipe                    │
│  ├─ os.Stderr *os.File     → io.Writer para errores (nunca se pipea)            │
│  └─ os.Exit(code)          → terminar con exit code                              │
│                                                                                   │
│  NIVEL 2: STDLIB (flag)                                                          │
│  ├─ flag.String/Int/Bool   → definir flags con defaults                          │
│  ├─ flag.Parse()           → parsear os.Args[1:]                                 │
│  ├─ flag.Args()            → argumentos posicionales (despues de flags)          │
│  ├─ flag.Value interface   → tipos custom para flags                             │
│  └─ flag.FlagSet           → multiples conjuntos de flags (subcomandos)          │
│                                                                                   │
│  NIVEL 3: POSIX (pflag — github.com/spf13/pflag)                                │
│  ├─ --long-flag            → flags largos estilo GNU                             │
│  ├─ -s (shorthand)         → flags cortos de una letra                           │
│  ├─ --flag=value           → asignacion con =                                    │
│  └─ API compatible con flag→ drop-in replacement                                 │
│                                                                                   │
│  NIVEL 4: FRAMEWORK (cobra — github.com/spf13/cobra)                            │
│  ├─ Subcomandos            → git-style (app cmd subcmd --flag)                   │
│  ├─ Auto-completado        → bash, zsh, fish, powershell                         │
│  ├─ Help automatico        → --help generado                                     │
│  ├─ Validacion             → args required, exact args, etc.                     │
│  └─ Integracion con viper  → config files + env vars + flags                     │
│                                                                                   │
│  PIPES Y COMPOSICION                                                             │
│  ┌──────┐    ┌──────┐    ┌──────┐                                               │
│  │ cmd1 │───→│ cmd2 │───→│ cmd3 │                                               │
│  │stdout│    │stdin │    │stdin │                                               │
│  └──────┘    │stdout│    │stdout│                                               │
│              └──────┘    └──────┘                                               │
│  Cada programa lee de stdin (io.Reader) y escribe a stdout (io.Writer).         │
│  stderr es independiente del pipeline.                                           │
│                                                                                   │
│  COMPARACION                                                                     │
│  ├─ C:    argc/argv, getopt(), getopt_long()                                    │
│  ├─ Rust: std::env::args(), clap (derive macros, builder)                       │
│  └─ Go:   os.Args, flag, cobra (runtime parsing)                               │
└───────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. os.Args — acceso crudo

`os.Args` es un `[]string` que contiene los argumentos del proceso. Es la forma mas basica de recibir input desde la linea de comandos.

### 2.1 Anatomia de os.Args

```go
// os.Args[0] es SIEMPRE el nombre/path del ejecutable
// os.Args[1:] son los argumentos del usuario

// Si ejecutas: ./myapp hello --verbose 42
// os.Args = ["./myapp", "hello", "--verbose", "42"]
// os.Args[0] = "./myapp"
// os.Args[1] = "hello"
// os.Args[2] = "--verbose"
// os.Args[3] = "42"

package main

import (
    "fmt"
    "os"
)

func main() {
    fmt.Printf("Programa: %s\n", os.Args[0])
    fmt.Printf("Argumentos: %v\n", os.Args[1:])
    fmt.Printf("Total argumentos: %d\n", len(os.Args)-1)
    
    // Iterar sobre argumentos
    for i, arg := range os.Args[1:] {
        fmt.Printf("  [%d] %q\n", i, arg)
    }
}
```

### 2.2 Cuidados con os.Args

```go
// PELIGRO: acceder sin verificar longitud
func main() {
    // MAL — panic si no hay argumentos
    name := os.Args[1]
    
    // BIEN — verificar primero
    if len(os.Args) < 2 {
        fmt.Fprintf(os.Stderr, "uso: %s <nombre>\n", os.Args[0])
        os.Exit(1)
    }
    name := os.Args[1]
    fmt.Printf("Hola, %s\n", name)
}
```

### 2.3 os.Args[0] y filepath.Base

```go
import "path/filepath"

func main() {
    // os.Args[0] puede ser:
    //   "./myapp"              (ejecucion relativa)
    //   "/usr/local/bin/myapp" (path absoluto)
    //   "myapp"                (en $PATH)
    //   "/tmp/go-build.../main" (go run)
    
    progName := filepath.Base(os.Args[0])
    // Siempre retorna solo el nombre: "myapp" o "main"
    
    fmt.Fprintf(os.Stderr, "uso: %s [opciones] <archivo>\n", progName)
}
```

### 2.4 Parsing manual de argumentos

```go
// Para CLIs muy simples, puedes parsear os.Args manualmente
// NOTA: flag o cobra son mejores para cualquier cosa no trivial

func main() {
    args := os.Args[1:]
    
    verbose := false
    var files []string
    
    for i := 0; i < len(args); i++ {
        switch args[i] {
        case "-v", "--verbose":
            verbose = true
        case "-h", "--help":
            printUsage()
            os.Exit(0)
        case "--":
            // Todo despues de -- son archivos (convencion POSIX)
            files = append(files, args[i+1:]...)
            i = len(args) // salir del loop
        default:
            if strings.HasPrefix(args[i], "-") {
                fmt.Fprintf(os.Stderr, "opcion desconocida: %s\n", args[i])
                os.Exit(1)
            }
            files = append(files, args[i])
        }
    }
    
    if verbose {
        fmt.Printf("Archivos: %v\n", files)
    }
}
```

### 2.5 Subcomandos con os.Args

```go
// Patron basico para subcomandos sin frameworks

func main() {
    if len(os.Args) < 2 {
        fmt.Fprintf(os.Stderr, "uso: %s <comando> [args]\n", os.Args[0])
        fmt.Fprintf(os.Stderr, "comandos: add, list, delete\n")
        os.Exit(1)
    }
    
    switch os.Args[1] {
    case "add":
        cmdAdd(os.Args[2:])
    case "list":
        cmdList(os.Args[2:])
    case "delete":
        cmdDelete(os.Args[2:])
    default:
        fmt.Fprintf(os.Stderr, "comando desconocido: %s\n", os.Args[1])
        os.Exit(1)
    }
}

func cmdAdd(args []string) {
    if len(args) < 1 {
        fmt.Fprintln(os.Stderr, "uso: add <item>")
        os.Exit(1)
    }
    fmt.Printf("Agregando: %s\n", args[0])
}
```

---

## 3. flag package — la stdlib

El paquete `flag` es la forma estandar de parsear flags en Go. Soporta flags booleanos, strings, enteros, floats y duraciones, con help automatico.

### 3.1 Tres formas de definir flags

```go
package main

import (
    "flag"
    "fmt"
    "time"
)

// FORMA 1: flag.Tipo() — retorna *Tipo (puntero)
var verbose = flag.Bool("verbose", false, "enable verbose output")
var name = flag.String("name", "world", "name to greet")
var count = flag.Int("count", 1, "number of greetings")

func main() {
    // FORMA 2: flag.TipoVar() — usa variable existente (no puntero)
    var port int
    flag.IntVar(&port, "port", 8080, "server port")
    
    var timeout time.Duration
    flag.DurationVar(&timeout, "timeout", 30*time.Second, "request timeout")
    
    // SIEMPRE llamar Parse() despues de definir todos los flags
    flag.Parse()
    
    // Acceder a los valores
    fmt.Printf("verbose: %v\n", *verbose)  // desreferenciar puntero
    fmt.Printf("name: %s\n", *name)
    fmt.Printf("count: %d\n", *count)
    fmt.Printf("port: %d\n", port)         // ya es valor directo
    fmt.Printf("timeout: %v\n", timeout)
    
    // flag.Args() retorna argumentos posicionales (despues de los flags)
    fmt.Printf("args: %v\n", flag.Args())
    fmt.Printf("nargs: %d\n", flag.NArg())
}
```

```
EJECUCION:

$ ./app -verbose -name=Alice -count 3 -port 9090 -timeout 5s file1.txt file2.txt

verbose: true
name: Alice
count: 3
port: 9090
timeout: 5s
args: [file1.txt file2.txt]
nargs: 2
```

### 3.2 Tipos de flags soportados

```go
// Todos los tipos nativos de flag:

flag.Bool("b", false, "boolean flag")        // -b o -b=true o -b=false
flag.Int("i", 0, "int flag")                 // -i 42 o -i=42
flag.Int64("i64", 0, "int64 flag")
flag.Uint("u", 0, "uint flag")
flag.Uint64("u64", 0, "uint64 flag")
flag.Float64("f", 0.0, "float64 flag")
flag.String("s", "", "string flag")          // -s "hello" o -s=hello
flag.Duration("d", 0, "duration flag")       // -d 5s o -d=1m30s

// NOTA: No hay flag.StringSlice ni flag.Map en la stdlib
// Para eso necesitas pflag o cobra, o implementar flag.Value
```

### 3.3 Sintaxis de flags (IMPORTANTE)

```
flag de Go usa UNA sintaxis no-POSIX:
  -flag        → flag booleano (true)
  -flag=value  → asignacion con =
  -flag value  → asignacion por posicion (no booleans)

DIFERENCIAS CON POSIX/GNU:
  Go:   -verbose          (un guion, palabra completa)
  POSIX: --verbose         (dos guiones para largo)
  POSIX: -v                (un guion, una letra)
  
  Go:   -v -e -r -b       (cada flag separado)
  POSIX: -verb             (flags combinados → -v -e -r -b)

  Go:    -flag=value       (= para asignacion)
  POSIX: --flag=value      (= solo con --)
  POSIX: -f value          (espacio con flag corto)

flag de Go NO soporta:
  ✗ Doble guion (--verbose)  → usa -verbose
  ✗ Flags combinados (-abc)  → usa -a -b -c
  ✗ Shorthand (-v para --verbose)
  
  Para soporte POSIX, usa pflag (seccion 5)
```

### 3.4 flag.Parse() y el comportamiento con errores

```go
// flag.Parse() tiene un ErrorHandling configurable:

// El flag.CommandLine (el FlagSet global) usa ExitOnError por defecto:
// - Flag desconocido → imprime error + usage, os.Exit(2)
// - Valor invalido   → imprime error + usage, os.Exit(2)

// Ejemplo: si ejecutas ./app -unknown
// Output:
//   flag provided but not defined: -unknown
//   Usage of ./app:
//     -name string
//       name to greet (default "world")

// Puedes customizar el output de usage:
flag.Usage = func() {
    fmt.Fprintf(os.Stderr, "Uso: %s [opciones] <archivo...>\n\n", os.Args[0])
    fmt.Fprintf(os.Stderr, "Opciones:\n")
    flag.PrintDefaults()  // imprime cada flag con su tipo, default y descripcion
    fmt.Fprintf(os.Stderr, "\nEjemplos:\n")
    fmt.Fprintf(os.Stderr, "  %s -verbose datos.json\n", os.Args[0])
    fmt.Fprintf(os.Stderr, "  %s -port 9090 -timeout 5s\n", os.Args[0])
}
```

### 3.5 flag.Value — tipos custom para flags

```go
// flag.Value es una interface:
type Value interface {
    String() string   // representacion del valor actual
    Set(string) error // parsear el string y setear el valor
}

// Ejemplo 1: StringSlice — flag que se puede repetir
// Uso: -tag=api -tag=v2 -tag=beta

type StringSlice []string

func (s *StringSlice) String() string {
    return strings.Join(*s, ", ")
}

func (s *StringSlice) Set(val string) error {
    *s = append(*s, val)
    return nil
}

func main() {
    var tags StringSlice
    flag.Var(&tags, "tag", "tag to apply (can be repeated)")
    flag.Parse()
    
    fmt.Printf("Tags: %v\n", tags)
    // ./app -tag=api -tag=v2 → Tags: [api v2]
}
```

```go
// Ejemplo 2: Enum — flag con valores validos predefinidos

type LogLevel string

func (l *LogLevel) String() string {
    return string(*l)
}

func (l *LogLevel) Set(val string) error {
    switch val {
    case "debug", "info", "warn", "error":
        *l = LogLevel(val)
        return nil
    default:
        return fmt.Errorf("invalid log level %q (valid: debug, info, warn, error)", val)
    }
}

func main() {
    level := LogLevel("info")
    flag.Var(&level, "level", "log level (debug, info, warn, error)")
    flag.Parse()
    
    fmt.Printf("Level: %s\n", level)
    // ./app -level=debug → Level: debug
    // ./app -level=trace → error: invalid log level "trace" (valid: debug, info, warn, error)
}
```

```go
// Ejemplo 3: KeyValue — flag con formato key=value

type KeyValue map[string]string

func (kv *KeyValue) String() string {
    pairs := make([]string, 0, len(*kv))
    for k, v := range *kv {
        pairs = append(pairs, k+"="+v)
    }
    return strings.Join(pairs, ", ")
}

func (kv *KeyValue) Set(val string) error {
    parts := strings.SplitN(val, "=", 2)
    if len(parts) != 2 {
        return fmt.Errorf("expected key=value, got %q", val)
    }
    if *kv == nil {
        *kv = make(KeyValue)
    }
    (*kv)[parts[0]] = parts[1]
    return nil
}

func main() {
    var labels KeyValue
    flag.Var(&labels, "label", "label in key=value format (can be repeated)")
    flag.Parse()
    
    fmt.Printf("Labels: %v\n", labels)
    // ./app -label=env=prod -label=team=api → Labels: map[env:prod team:api]
}
```

### 3.6 flag.FlagSet — subcomandos con la stdlib

```go
// flag.FlagSet permite crear conjuntos independientes de flags.
// Util para implementar subcomandos sin cobra.

package main

import (
    "flag"
    "fmt"
    "os"
)

func main() {
    // Definir FlagSets para cada subcomando
    serveCmd := flag.NewFlagSet("serve", flag.ExitOnError)
    servePort := serveCmd.Int("port", 8080, "port to listen on")
    serveHost := serveCmd.String("host", "localhost", "host to bind to")
    serveTLS := serveCmd.Bool("tls", false, "enable TLS")
    
    buildCmd := flag.NewFlagSet("build", flag.ExitOnError)
    buildOutput := buildCmd.String("output", "build/", "output directory")
    buildVerbose := buildCmd.Bool("verbose", false, "verbose output")
    buildTags := buildCmd.String("tags", "", "build tags (comma-separated)")
    
    migrateCmd := flag.NewFlagSet("migrate", flag.ExitOnError)
    migrateDB := migrateCmd.String("db", "", "database URL (required)")
    migrateSteps := migrateCmd.Int("steps", 0, "number of steps (0 = all)")
    migrateDry := migrateCmd.Bool("dry-run", false, "show SQL without executing")

    if len(os.Args) < 2 {
        fmt.Fprintf(os.Stderr, "uso: %s <comando> [opciones]\n\n", os.Args[0])
        fmt.Fprintf(os.Stderr, "Comandos:\n")
        fmt.Fprintf(os.Stderr, "  serve    Iniciar el servidor HTTP\n")
        fmt.Fprintf(os.Stderr, "  build    Compilar el proyecto\n")
        fmt.Fprintf(os.Stderr, "  migrate  Ejecutar migraciones de BD\n")
        os.Exit(1)
    }

    switch os.Args[1] {
    case "serve":
        serveCmd.Parse(os.Args[2:])
        fmt.Printf("Servidor en %s:%d (TLS: %v)\n", *serveHost, *servePort, *serveTLS)
        
    case "build":
        buildCmd.Parse(os.Args[2:])
        fmt.Printf("Build → %s (verbose: %v, tags: %s)\n", *buildOutput, *buildVerbose, *buildTags)
        // buildCmd.Args() tiene argumentos posicionales
        if buildCmd.NArg() > 0 {
            fmt.Printf("Targets: %v\n", buildCmd.Args())
        }
        
    case "migrate":
        migrateCmd.Parse(os.Args[2:])
        if *migrateDB == "" {
            fmt.Fprintln(os.Stderr, "error: -db es requerido")
            migrateCmd.Usage()
            os.Exit(1)
        }
        fmt.Printf("Migrate DB=%s steps=%d dry=%v\n", *migrateDB, *migrateSteps, *migrateDry)
        
    default:
        fmt.Fprintf(os.Stderr, "comando desconocido: %s\n", os.Args[1])
        os.Exit(1)
    }
}
```

```
EJECUCION:

$ ./app serve -port 9090 -tls
Servidor en localhost:9090 (TLS: true)

$ ./app build -verbose -tags "integration" ./cmd/api ./cmd/worker
Build → build/ (verbose: true, tags: integration)
Targets: [./cmd/api ./cmd/worker]

$ ./app migrate -db "postgres://localhost/mydb" -dry-run
Migrate DB=postgres://localhost/mydb steps=0 dry=true

$ ./app migrate
error: -db es requerido
Usage of migrate:
  -db string
      database URL (required)
  -dry-run
      show SQL without executing
  -steps int
      number of steps (0 = all)
```

### 3.7 Patron: flags requeridos

```go
// flag no tiene concepto de "required" — todos los flags son opcionales.
// Patron para validar flags requeridos:

func main() {
    dbURL := flag.String("db", "", "database URL (required)")
    configFile := flag.String("config", "", "config file path (required)")
    port := flag.Int("port", 8080, "server port")
    flag.Parse()
    
    // Validar required flags
    var missing []string
    if *dbURL == "" {
        missing = append(missing, "-db")
    }
    if *configFile == "" {
        missing = append(missing, "-config")
    }
    
    if len(missing) > 0 {
        fmt.Fprintf(os.Stderr, "error: flags requeridos faltantes: %s\n", strings.Join(missing, ", "))
        flag.Usage()
        os.Exit(1)
    }
    
    fmt.Printf("DB: %s, Config: %s, Port: %d\n", *dbURL, *configFile, *port)
}
```

---

## 4. stdin, stdout, stderr — las tres corrientes

### 4.1 Los tres archivos estandar

```go
// En Go, stdin/stdout/stderr son variables globales de tipo *os.File
// *os.File implementa io.Reader (stdin) y io.Writer (stdout, stderr)

package main

import (
    "fmt"
    "os"
)

func main() {
    // os.Stdin  → *os.File → implementa io.Reader
    // os.Stdout → *os.File → implementa io.Writer
    // os.Stderr → *os.File → implementa io.Writer
    
    // Escribir a stdout (lo normal)
    fmt.Fprintln(os.Stdout, "esto va a stdout")
    // equivalente a:
    fmt.Println("esto va a stdout")
    
    // Escribir a stderr (errores, progreso, debug)
    fmt.Fprintln(os.Stderr, "esto va a stderr")
    
    // Leer de stdin
    var input string
    fmt.Fscan(os.Stdin, &input)
    
    // File descriptors subyacentes
    fmt.Println(os.Stdin.Fd())  // 0
    fmt.Println(os.Stdout.Fd()) // 1
    fmt.Println(os.Stderr.Fd()) // 2
}
```

### 4.2 Detectar si stdin es terminal o pipe

```go
import (
    "os"
    "golang.org/x/term" // o verificar manualmente
)

// Metodo 1: usando os.File.Stat()
func isInputFromPipe() bool {
    fi, err := os.Stdin.Stat()
    if err != nil {
        return false
    }
    // Si no es CharDevice, es un pipe o archivo
    return fi.Mode()&os.ModeCharDevice == 0
}

// Metodo 2: usando golang.org/x/term (mas robusto)
func isTerminal() bool {
    return term.IsTerminal(int(os.Stdin.Fd()))
}

func main() {
    if isInputFromPipe() {
        // Leer todo de stdin (modo pipe)
        data, _ := io.ReadAll(os.Stdin)
        process(data)
    } else {
        // Modo interactivo — pedir input al usuario
        fmt.Print("Ingresa datos: ")
        scanner := bufio.NewScanner(os.Stdin)
        scanner.Scan()
        process([]byte(scanner.Text()))
    }
}
```

### 4.3 Leer stdin linea por linea

```go
// Patron clasico: leer lineas de stdin con bufio.Scanner

func main() {
    scanner := bufio.NewScanner(os.Stdin)
    
    lineNum := 0
    for scanner.Scan() {
        lineNum++
        line := scanner.Text()
        // Procesar cada linea
        fmt.Printf("%4d: %s\n", lineNum, line)
    }
    
    if err := scanner.Err(); err != nil {
        fmt.Fprintf(os.Stderr, "error leyendo stdin: %v\n", err)
        os.Exit(1)
    }
}

// Uso: cat archivo.txt | ./app
//      echo "hola" | ./app
//      ./app < archivo.txt
```

### 4.4 Leer stdin o archivos (patron Unix)

```go
// Patron comun en herramientas Unix:
// - Si se pasan archivos como args, leer esos archivos
// - Si no hay args, leer de stdin
// - Si un arg es "-", leer de stdin en esa posicion

func main() {
    flag.Parse()
    
    readers, err := getInputReaders(flag.Args())
    if err != nil {
        fmt.Fprintf(os.Stderr, "error: %v\n", err)
        os.Exit(1)
    }
    
    for _, r := range readers {
        if closer, ok := r.(io.Closer); ok {
            defer closer.Close()
        }
        processInput(r)
    }
}

func getInputReaders(args []string) ([]io.Reader, error) {
    if len(args) == 0 {
        // Sin argumentos → leer de stdin
        return []io.Reader{os.Stdin}, nil
    }
    
    var readers []io.Reader
    for _, arg := range args {
        if arg == "-" {
            readers = append(readers, os.Stdin)
        } else {
            f, err := os.Open(arg)
            if err != nil {
                return nil, err
            }
            readers = append(readers, f)
        }
    }
    return readers, nil
}

func processInput(r io.Reader) {
    scanner := bufio.NewScanner(r)
    for scanner.Scan() {
        fmt.Println(strings.ToUpper(scanner.Text()))
    }
}

// Uso:
//   ./app archivo1.txt archivo2.txt    → lee ambos archivos
//   cat datos.txt | ./app              → lee de stdin
//   ./app archivo1.txt - archivo2.txt  → archivo1, stdin, archivo2
```

### 4.5 stdout vs stderr — cuando usar cada uno

```
┌─────────────────────────────────────────────────────────────────┐
│                 stdout vs stderr                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  stdout (os.Stdout, fmt.Println, fmt.Printf)                  │
│  ├─ Datos del programa (el resultado)                          │
│  ├─ Output que se puede pipear a otro programa                │
│  ├─ Output estructurado (JSON, CSV, lineas de texto)          │
│  └─ Ejemplo: resultados de busqueda, datos procesados         │
│                                                                 │
│  stderr (os.Stderr, fmt.Fprintln(os.Stderr, ...))            │
│  ├─ Mensajes de error                                          │
│  ├─ Mensajes de progreso y estado                              │
│  ├─ Mensajes de debug/verbose                                  │
│  ├─ Mensajes de warning                                        │
│  ├─ Prompts al usuario                                         │
│  └─ Cualquier cosa que NO es el dato principal                 │
│                                                                 │
│  POR QUE IMPORTA:                                              │
│  $ ./app 2>/dev/null        → solo ver datos, no errores      │
│  $ ./app 2>errors.log       → guardar errores aparte          │
│  $ ./app | jq '.'           → jq solo ve stdout               │
│  $ ./app | ./next           → stderr no entra al pipe         │
│                                                                 │
│  REGLA: si no estarias contento viendolo en la salida de       │
│  "./app | jq", va a stderr.                                   │
└─────────────────────────────────────────────────────────────────┘
```

```go
// Ejemplo: programa que descarga URLs y reporta progreso

func download(urls []string) {
    for i, url := range urls {
        // Progreso → stderr (no contamina el pipe)
        fmt.Fprintf(os.Stderr, "[%d/%d] Descargando %s...\n", i+1, len(urls), url)
        
        resp, err := http.Get(url)
        if err != nil {
            // Error → stderr
            fmt.Fprintf(os.Stderr, "ERROR: %s: %v\n", url, err)
            continue
        }
        
        // Datos → stdout (se puede pipear)
        io.Copy(os.Stdout, resp.Body)
        resp.Body.Close()
    }
    
    // Resumen → stderr
    fmt.Fprintf(os.Stderr, "Completado: %d URLs\n", len(urls))
}
```

### 4.6 Buffering de stdout

```go
// os.Stdout no tiene buffering a nivel de Go (cada Write es un syscall).
// Para mejor rendimiento con muchas writes pequenas, usa bufio.Writer:

func main() {
    // Sin buffer: cada Fprintln es un syscall write()
    for i := 0; i < 100000; i++ {
        fmt.Fprintln(os.Stdout, i)
    }
    
    // Con buffer: acumula en memoria, menos syscalls
    w := bufio.NewWriter(os.Stdout)
    for i := 0; i < 100000; i++ {
        fmt.Fprintln(w, i)
    }
    w.Flush() // CRITICO: no olvidar Flush al final
}

// Benchmark tipico:
// Sin bufio: ~400ms para 100K lineas
// Con bufio: ~40ms para 100K lineas (10x mas rapido)
```

### 4.7 Redireccion de stdout/stderr en el programa

```go
// Redirigir stdout a un archivo (util para testing/logging)
func main() {
    // Guardar stdout original
    origStdout := os.Stdout
    
    // Crear archivo para capturar output
    f, err := os.Create("output.log")
    if err != nil {
        log.Fatal(err)
    }
    defer f.Close()
    
    // Redirigir stdout
    os.Stdout = f
    
    fmt.Println("esto va al archivo, no al terminal")
    
    // Restaurar stdout
    os.Stdout = origStdout
    fmt.Println("esto vuelve al terminal")
}

// Para testing, es mejor usar io.Writer como dependencia:
func process(w io.Writer) {
    fmt.Fprintln(w, "resultado")
}

// En produccion: process(os.Stdout)
// En tests:      var buf bytes.Buffer; process(&buf)
```

---

## 5. pflag — flags POSIX-compatibles

El paquete `github.com/spf13/pflag` es un drop-in replacement para `flag` que soporta flags estilo POSIX/GNU con `--long` y `-s` shorthand.

### 5.1 Diferencias con flag

```
┌────────────────────────────┬──────────────────┬──────────────────────┐
│ Feature                    │ flag (stdlib)     │ pflag                │
├────────────────────────────┼──────────────────┼──────────────────────┤
│ Flag largo                 │ -verbose          │ --verbose            │
│ Flag corto                 │ (no soportado)    │ -v                   │
│ Ambos                      │ (no)              │ --verbose, -v        │
│ Asignacion                 │ -flag=val         │ --flag=val o -f val  │
│ Flags combinados           │ (no)              │ -abc = -a -b -c      │
│ Booleanos                  │ -flag=false       │ --no-flag            │
│ Deprecation                │ (no)              │ MarkDeprecated()     │
│ Hidden flags               │ (no)              │ MarkHidden()         │
│ Slash flag types           │ (no)              │ StringSlice, etc.    │
│ API compatible con flag    │ —                 │ Si (import alias)    │
└────────────────────────────┴──────────────────┴──────────────────────┘
```

### 5.2 Uso basico de pflag

```go
package main

import (
    "fmt"
    flag "github.com/spf13/pflag" // Drop-in replacement
)

func main() {
    // Definir flags con shorthand
    verbose := flag.BoolP("verbose", "v", false, "enable verbose output")
    name := flag.StringP("name", "n", "world", "name to greet")
    count := flag.IntP("count", "c", 1, "number of times")
    port := flag.Int("port", 8080, "server port") // sin shorthand
    
    // Tipos adicionales no disponibles en flag stdlib
    tags := flag.StringSlice("tag", nil, "tags (can be repeated or comma-separated)")
    headers := flag.StringToString("header", nil, "headers in key=value format")
    
    flag.Parse()
    
    fmt.Printf("verbose: %v\n", *verbose)
    fmt.Printf("name: %s\n", *name)
    fmt.Printf("count: %d\n", *count)
    fmt.Printf("port: %d\n", *port)
    fmt.Printf("tags: %v\n", *tags)
    fmt.Printf("headers: %v\n", *headers)
    fmt.Printf("args: %v\n", flag.Args())
}
```

```
EJECUCION:

$ ./app -v --name=Alice -c 3 --tag=api --tag=v2 --header=auth=token file.txt

# Tambien funciona:
$ ./app -vn Alice -c3 --tag api,v2 --header auth=token file.txt

# Flags combinados:
$ ./app -vc 3   # -v (verbose) + -c 3 (count)
```

### 5.3 Features avanzadas de pflag

```go
// Deprecar un flag
flag.String("output-dir", ".", "output directory")
flag.String("outdir", ".", "output directory")
flag.MarkDeprecated("outdir", "use --output-dir instead")
// Si alguien usa --outdir: funciona pero imprime warning

// Ocultar un flag (no aparece en --help)
flag.String("internal-debug", "", "internal debug flag")
flag.MarkHidden("internal-debug")

// Flag que normaliza nombres
// Ejemplo: hacer que --my_flag y --my-flag sean equivalentes
flag.CommandLine.SetNormalizeFunc(func(f *pflag.FlagSet, name string) pflag.NormalizedName {
    return pflag.NormalizedName(strings.ReplaceAll(name, "_", "-"))
})
```

---

## 6. cobra — el framework CLI de Go

`github.com/spf13/cobra` es el framework CLI mas popular de Go. Lo usan kubectl, docker, hugo, gh, helm, y cientos de CLIs conocidas. Cobra se construye sobre pflag y agrega subcomandos, auto-completado, help generado, y validacion de argumentos.

### 6.1 Conceptos fundamentales

```
┌───────────────────────────────────────────────────────────────────────────────┐
│                    COBRA — ESTRUCTURA                                        │
├───────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│  CONCEPTOS                                                                   │
│  ├─ Command:  una accion o grupo (root, sub, sub-sub...)                    │
│  ├─ Args:     argumentos posicionales del comando                            │
│  ├─ Flags:    opciones con -- (persistent = heredados por hijos)            │
│  └─ Run:      la funcion que ejecuta el comando                              │
│                                                                               │
│  ESTRUCTURA DE PROYECTO TIPICA                                               │
│  myapp/                                                                      │
│  ├── main.go           → solo llama cmd.Execute()                           │
│  ├── cmd/                                                                    │
│  │   ├── root.go       → rootCmd (el comando base)                          │
│  │   ├── serve.go      → serveCmd (subcomando)                              │
│  │   ├── build.go      → buildCmd (subcomando)                              │
│  │   └── migrate.go    → migrateCmd (subcomando)                            │
│  └── go.mod                                                                  │
│                                                                               │
│  ARBOL DE COMANDOS                                                           │
│  myapp                      → rootCmd                                        │
│  ├── serve                  → myapp serve --port 8080                       │
│  ├── build                  → myapp build --output ./dist                   │
│  │   ├── docker             → myapp build docker --tag v1.0                 │
│  │   └── binary             → myapp build binary --os linux                 │
│  ├── migrate                → myapp migrate                                  │
│  │   ├── up                 → myapp migrate up --steps 1                    │
│  │   └── down               → myapp migrate down --steps 1                  │
│  ├── version                → myapp version                                  │
│  └── completion             → myapp completion bash (auto-generado)         │
│                                                                               │
│  LIFECYCLE DE UN COMANDO                                                     │
│  PersistentPreRun → PreRun → Run → PostRun → PersistentPostRun             │
│  (heredado)       (local)  (local) (local)   (heredado)                     │
│                                                                               │
│  TIPOS DE FLAGS                                                              │
│  ├─ Local flag:      solo para este comando                                  │
│  ├─ Persistent flag: heredado por todos los subcomandos                      │
│  └─ Required flag:   obligatorio (MarkFlagRequired)                         │
└───────────────────────────────────────────────────────────────────────────────┘
```

### 6.2 Ejemplo completo: estructura minima

```go
// ===== main.go =====
package main

import "myapp/cmd"

func main() {
    cmd.Execute()
}
```

```go
// ===== cmd/root.go =====
package cmd

import (
    "fmt"
    "os"
    
    "github.com/spf13/cobra"
)

// Flags persistentes (disponibles en todos los subcomandos)
var (
    verbose bool
    cfgFile string
)

var rootCmd = &cobra.Command{
    Use:   "myapp",
    Short: "MyApp — una herramienta de ejemplo",
    Long: `MyApp es una herramienta CLI de ejemplo que demuestra
el uso de cobra para crear CLIs profesionales en Go.

Soporta subcomandos, flags, auto-completado y mas.`,
    // Si rootCmd tiene Run, se ejecuta cuando no hay subcomando
    // Si no tiene Run, muestra help automaticamente
    PersistentPreRun: func(cmd *cobra.Command, args []string) {
        // Se ejecuta ANTES de cualquier subcomando
        if verbose {
            fmt.Fprintln(os.Stderr, "[verbose mode enabled]")
        }
    },
}

func Execute() {
    if err := rootCmd.Execute(); err != nil {
        // cobra ya imprime el error, solo salimos
        os.Exit(1)
    }
}

func init() {
    // Persistent flags — disponibles en TODOS los subcomandos
    rootCmd.PersistentFlags().BoolVarP(&verbose, "verbose", "v", false, "verbose output")
    rootCmd.PersistentFlags().StringVar(&cfgFile, "config", "", "config file (default: $HOME/.myapp.yaml)")
}
```

```go
// ===== cmd/serve.go =====
package cmd

import (
    "fmt"
    "net/http"
    
    "github.com/spf13/cobra"
)

var serveCmd = &cobra.Command{
    Use:   "serve",
    Short: "Inicia el servidor HTTP",
    Long:  `Inicia el servidor HTTP en el puerto especificado con las opciones dadas.`,
    Example: `  myapp serve --port 9090
  myapp serve --port 8080 --host 0.0.0.0 --tls`,
    RunE: func(cmd *cobra.Command, args []string) error {
        port, _ := cmd.Flags().GetInt("port")
        host, _ := cmd.Flags().GetString("host")
        tls, _ := cmd.Flags().GetBool("tls")
        
        addr := fmt.Sprintf("%s:%d", host, port)
        fmt.Printf("Servidor en %s (TLS: %v, verbose: %v)\n", addr, tls, verbose)
        
        // verbose viene del rootCmd (persistent flag)
        if verbose {
            fmt.Println("Rutas registradas:")
            fmt.Println("  GET  /health")
            fmt.Println("  GET  /api/v1/items")
        }
        
        http.HandleFunc("/health", func(w http.ResponseWriter, r *http.Request) {
            w.Write([]byte("OK"))
        })
        
        return http.ListenAndServe(addr, nil)
    },
}

func init() {
    // Flags locales — solo para 'serve'
    serveCmd.Flags().IntP("port", "p", 8080, "port to listen on")
    serveCmd.Flags().String("host", "localhost", "host to bind to")
    serveCmd.Flags().Bool("tls", false, "enable TLS")
    
    // Registrar como subcomando de root
    rootCmd.AddCommand(serveCmd)
}
```

```go
// ===== cmd/build.go =====
package cmd

import (
    "fmt"
    
    "github.com/spf13/cobra"
)

var buildCmd = &cobra.Command{
    Use:   "build [targets...]",
    Short: "Compila el proyecto",
    Long:  `Compila el proyecto con las opciones especificadas.`,
    Args:  cobra.MinimumNArgs(0), // 0+ argumentos posicionales
    RunE: func(cmd *cobra.Command, args []string) error {
        output, _ := cmd.Flags().GetString("output")
        tags, _ := cmd.Flags().GetStringSlice("tags")
        
        targets := args
        if len(targets) == 0 {
            targets = []string{"./..."}
        }
        
        fmt.Printf("Building targets: %v\n", targets)
        fmt.Printf("Output: %s\n", output)
        if len(tags) > 0 {
            fmt.Printf("Tags: %v\n", tags)
        }
        
        return nil
    },
}

func init() {
    buildCmd.Flags().StringP("output", "o", "build/", "output directory")
    buildCmd.Flags().StringSlice("tags", nil, "build tags")
    
    rootCmd.AddCommand(buildCmd)
}
```

### 6.3 Validacion de argumentos

```go
// cobra tiene validadores de argumentos built-in:

&cobra.Command{
    Use:  "delete <id>",
    Args: cobra.ExactArgs(1),   // exactamente 1 argumento
    // ...
}

// Validadores disponibles:
cobra.NoArgs              // no acepta argumentos
cobra.ArbitraryArgs       // cualquier cantidad (default)
cobra.MinimumNArgs(n)     // al menos n
cobra.MaximumNArgs(n)     // maximo n
cobra.ExactArgs(n)        // exactamente n
cobra.RangeArgs(min, max) // entre min y max

// Validador custom:
&cobra.Command{
    Use:  "set <key> <value>",
    Args: func(cmd *cobra.Command, args []string) error {
        if len(args) != 2 {
            return fmt.Errorf("requires exactly 2 arguments: key and value, got %d", len(args))
        }
        if strings.Contains(args[0], " ") {
            return fmt.Errorf("key cannot contain spaces: %q", args[0])
        }
        return nil
    },
    RunE: func(cmd *cobra.Command, args []string) error {
        key, value := args[0], args[1]
        fmt.Printf("Set %s = %s\n", key, value)
        return nil
    },
}

// ValidArgs — lista de argumentos validos (para auto-completado)
&cobra.Command{
    Use:       "status <env>",
    ValidArgs: []string{"dev", "staging", "prod"},
    Args:      cobra.ExactArgs(1),
    // Solo acepta: myapp status dev|staging|prod
}
```

### 6.4 Run vs RunE

```go
// Run — no retorna error (usa os.Exit internamente)
&cobra.Command{
    Run: func(cmd *cobra.Command, args []string) {
        // Si algo falla, debes manejar el error manualmente
        if err := doSomething(); err != nil {
            fmt.Fprintf(os.Stderr, "error: %v\n", err)
            os.Exit(1)
        }
    },
}

// RunE — retorna error (cobra lo maneja)
&cobra.Command{
    RunE: func(cmd *cobra.Command, args []string) error {
        // Retornar error → cobra lo imprime y sale con code 1
        return doSomething()
    },
}

// PREFERIR RunE:
// - Permite que cobra maneje errores consistentemente
// - Facilita testing (puedes verificar el error retornado)
// - Permite que PersistentPostRun se ejecute incluso en error
```

### 6.5 Flags: locales, persistentes, requeridos

```go
func init() {
    // FLAG LOCAL: solo disponible en este comando
    myCmd.Flags().String("output", "", "output file")
    
    // FLAG PERSISTENTE: disponible en este comando y TODOS sus hijos
    myCmd.PersistentFlags().String("log-level", "info", "log level")
    
    // MARCAR COMO REQUERIDO
    myCmd.Flags().String("db", "", "database URL")
    myCmd.MarkFlagRequired("db")
    // Si el usuario no pasa --db, cobra muestra error automaticamente
    
    // MARCAR COMO FILENAME (para auto-completado de archivos)
    myCmd.Flags().String("config", "", "config file")
    myCmd.MarkFlagFilename("config", "yaml", "yml", "json")
    // Shell completion sugerira archivos .yaml, .yml, .json
    
    // MARCAR COMO DIRNAME (auto-completar directorios)
    myCmd.Flags().String("output-dir", ".", "output directory")
    myCmd.MarkFlagDirname("output-dir")
    
    // FLAGS MUTUAMENTE EXCLUSIVOS
    myCmd.Flags().String("format", "", "output format")
    myCmd.Flags().Bool("json", false, "output as JSON")
    myCmd.MarkFlagsMutuallyExclusive("format", "json")
    // Error si ambos se pasan
    
    // FLAGS QUE REQUIEREN OTRO
    myCmd.Flags().String("tls-cert", "", "TLS certificate")
    myCmd.Flags().String("tls-key", "", "TLS key")
    myCmd.MarkFlagsRequiredTogether("tls-cert", "tls-key")
    // Error si solo uno se pasa
}
```

### 6.6 Lifecycle hooks

```go
// Orden de ejecucion de los hooks:

var rootCmd = &cobra.Command{
    PersistentPreRunE: func(cmd *cobra.Command, args []string) error {
        // 1. Se ejecuta primero, heredado por todos los hijos
        // Buen lugar para: cargar config, setup logging, validar auth
        return loadConfig()
    },
}

var serveCmd = &cobra.Command{
    PreRunE: func(cmd *cobra.Command, args []string) error {
        // 2. Antes del Run, solo para este comando
        // Buen lugar para: validaciones especificas del comando
        return validateServeConfig()
    },
    RunE: func(cmd *cobra.Command, args []string) error {
        // 3. La logica principal
        return startServer()
    },
    PostRunE: func(cmd *cobra.Command, args []string) error {
        // 4. Despues del Run, solo para este comando
        // Buen lugar para: cleanup local
        return nil
    },
    PersistentPostRunE: func(cmd *cobra.Command, args []string) error {
        // 5. Al final, heredado por todos los hijos
        // Buen lugar para: flush logs, cerrar conexiones
        return closeConnections()
    },
}

// Orden completo:
// rootCmd.PersistentPreRunE (heredado)
// serveCmd.PreRunE          (local)
// serveCmd.RunE             (local)
// serveCmd.PostRunE         (local)
// serveCmd.PersistentPostRunE (local, sobreescribe el de root si existe)
```

### 6.7 Subcomandos anidados

```go
// myapp migrate up --steps 3
// myapp migrate down --steps 1
// myapp migrate status

var migrateCmd = &cobra.Command{
    Use:   "migrate",
    Short: "Gestionar migraciones de base de datos",
    // Sin Run/RunE → muestra help cuando se usa sin subcomando
}

var migrateUpCmd = &cobra.Command{
    Use:   "up",
    Short: "Aplicar migraciones pendientes",
    RunE: func(cmd *cobra.Command, args []string) error {
        steps, _ := cmd.Flags().GetInt("steps")
        dryRun, _ := cmd.Flags().GetBool("dry-run")
        
        fmt.Printf("Aplicando %d migraciones (dry-run: %v)\n", steps, dryRun)
        return nil
    },
}

var migrateDownCmd = &cobra.Command{
    Use:   "down",
    Short: "Revertir migraciones",
    RunE: func(cmd *cobra.Command, args []string) error {
        steps, _ := cmd.Flags().GetInt("steps")
        if steps == 0 {
            return fmt.Errorf("--steps es requerido para migrate down (seguridad)")
        }
        fmt.Printf("Revirtiendo %d migraciones\n", steps)
        return nil
    },
}

var migrateStatusCmd = &cobra.Command{
    Use:   "status",
    Short: "Mostrar estado de migraciones",
    RunE: func(cmd *cobra.Command, args []string) error {
        fmt.Println("Migraciones aplicadas:")
        fmt.Println("  001_create_users    ✓ 2024-01-15")
        fmt.Println("  002_add_email       ✓ 2024-01-16")
        fmt.Println("  003_create_orders   ✗ pendiente")
        return nil
    },
}

func init() {
    // Flag compartido por up y down (persistent en migrateCmd)
    migrateCmd.PersistentFlags().Bool("dry-run", false, "show SQL without executing")
    
    // Flags locales
    migrateUpCmd.Flags().Int("steps", 0, "number of migrations to apply (0 = all)")
    migrateDownCmd.Flags().Int("steps", 0, "number of migrations to revert")
    migrateDownCmd.MarkFlagRequired("steps") // down SIEMPRE requiere steps
    
    // Registrar jerarquia
    migrateCmd.AddCommand(migrateUpCmd)
    migrateCmd.AddCommand(migrateDownCmd)
    migrateCmd.AddCommand(migrateStatusCmd)
    rootCmd.AddCommand(migrateCmd)
}
```

### 6.8 Auto-completado de shell

```go
// cobra genera scripts de auto-completado automaticamente

var completionCmd = &cobra.Command{
    Use:   "completion [bash|zsh|fish|powershell]",
    Short: "Generar script de auto-completado",
    Long: `Para cargar completions:

Bash:
  $ source <(myapp completion bash)
  # Para cargar en cada sesion:
  $ myapp completion bash > /etc/bash_completion.d/myapp

Zsh:
  $ source <(myapp completion zsh)
  # Para cargar en cada sesion:
  $ myapp completion zsh > "${fpath[1]}/_myapp"

Fish:
  $ myapp completion fish | source
  $ myapp completion fish > ~/.config/fish/completions/myapp.fish
`,
    Args:      cobra.ExactValidArgs(1),
    ValidArgs: []string{"bash", "zsh", "fish", "powershell"},
    RunE: func(cmd *cobra.Command, args []string) error {
        switch args[0] {
        case "bash":
            return rootCmd.GenBashCompletionV2(os.Stdout, true)
        case "zsh":
            return rootCmd.GenZshCompletion(os.Stdout)
        case "fish":
            return rootCmd.GenFishCompletion(os.Stdout, true)
        case "powershell":
            return rootCmd.GenPowerShellCompletionWithDesc(os.Stdout)
        }
        return nil
    },
}

func init() {
    rootCmd.AddCommand(completionCmd)
}
```

### 6.9 Completado dinamico de argumentos

```go
// cobra soporta completado dinamico para sugerir valores en runtime

var deployCmd = &cobra.Command{
    Use:   "deploy [environment]",
    Short: "Deploy to environment",
    // ValidArgsFunction se ejecuta en runtime para generar sugerencias
    ValidArgsFunction: func(cmd *cobra.Command, args []string, toComplete string) ([]string, cobra.ShellCompDirective) {
        if len(args) != 0 {
            // Ya hay un argumento, no sugerir mas
            return nil, cobra.ShellCompDirectiveNoFileComp
        }
        
        // Sugerir environments disponibles
        // Podrias consultar una API o leer un archivo de config aqui
        envs := []string{
            "dev\tDevelopment environment",
            "staging\tStaging environment",
            "prod\tProduction environment (requires approval)",
        }
        return envs, cobra.ShellCompDirectiveNoFileComp
    },
    RunE: func(cmd *cobra.Command, args []string) error {
        fmt.Printf("Deploying to %s\n", args[0])
        return nil
    },
}

// Completado dinamico para flags
func init() {
    deployCmd.Flags().String("region", "", "deploy region")
    deployCmd.RegisterFlagCompletionFunc("region", func(cmd *cobra.Command, args []string, toComplete string) ([]string, cobra.ShellCompDirective) {
        regions := []string{"us-east-1", "us-west-2", "eu-west-1", "ap-southeast-1"}
        return regions, cobra.ShellCompDirectiveNoFileComp
    })
}
```

### 6.10 cobra-cli: generador de scaffolding

```
# Instalar cobra-cli
go install github.com/spf13/cobra-cli@latest

# Inicializar proyecto
cobra-cli init myapp
# Crea:
#   main.go
#   cmd/root.go

# Agregar subcomandos
cobra-cli add serve
cobra-cli add build
cobra-cli add migrate

# Agregar subcomando anidado
cobra-cli add up -p migrateCmd
cobra-cli add down -p migrateCmd

# Cada comando genera un archivo en cmd/ con boilerplate
```

---

## 7. Senales del sistema operativo

Las senales Unix permiten comunicacion entre procesos. En Go, `os/signal` permite interceptarlas para hacer cleanup o graceful shutdown.

### 7.1 Senales comunes

```
┌──────────┬───────────┬──────────────────────────────────────────┐
│ Senal    │ Numero    │ Significado                              │
├──────────┼───────────┼──────────────────────────────────────────┤
│ SIGINT   │ 2         │ Ctrl+C en terminal                      │
│ SIGTERM  │ 15        │ kill <pid> (terminacion graceful)        │
│ SIGKILL  │ 9         │ kill -9 (NO se puede interceptar)       │
│ SIGHUP   │ 1         │ Terminal cerrado / reload config         │
│ SIGUSR1  │ 10        │ User-defined (reload, dump state)       │
│ SIGUSR2  │ 12        │ User-defined                            │
│ SIGPIPE  │ 13        │ Escritura a pipe cerrado                │
│ SIGQUIT  │ 3         │ Ctrl+\ (quit con core dump)             │
└──────────┴───────────┴──────────────────────────────────────────┘
```

### 7.2 Interceptar senales

```go
package main

import (
    "context"
    "fmt"
    "os"
    "os/signal"
    "syscall"
    "time"
)

func main() {
    // Metodo moderno (Go 1.16+): signal.NotifyContext
    ctx, stop := signal.NotifyContext(context.Background(), 
        syscall.SIGINT, syscall.SIGTERM)
    defer stop()
    
    fmt.Println("Servidor iniciado. Ctrl+C para detener.")
    
    // Simular servidor
    go func() {
        <-ctx.Done()
        fmt.Println("\nSenal recibida, cerrando...")
    }()
    
    // Esperar hasta que el contexto se cancele
    <-ctx.Done()
    
    // Graceful shutdown: dar tiempo para terminar requests
    shutdownCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
    defer cancel()
    
    // Aqui iria server.Shutdown(shutdownCtx)
    fmt.Printf("Shutdown completado (deadline: %v)\n", shutdownCtx.Err() == nil)
}
```

```go
// Metodo clasico: signal.Notify con canal

func main() {
    sigChan := make(chan os.Signal, 1)
    signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM, syscall.SIGHUP)
    
    go func() {
        for sig := range sigChan {
            switch sig {
            case syscall.SIGINT, syscall.SIGTERM:
                fmt.Printf("\nRecibido %v, cerrando...\n", sig)
                cleanup()
                os.Exit(0)
            case syscall.SIGHUP:
                fmt.Println("Recibido SIGHUP, recargando config...")
                reloadConfig()
            }
        }
    }()
    
    // ... programa principal ...
    select {} // bloquear forever
}
```

### 7.3 Graceful shutdown de servidor HTTP

```go
func main() {
    mux := http.NewServeMux()
    mux.HandleFunc("GET /health", func(w http.ResponseWriter, r *http.Request) {
        w.Write([]byte("OK"))
    })
    
    server := &http.Server{
        Addr:    ":8080",
        Handler: mux,
    }
    
    // Iniciar servidor en goroutine
    go func() {
        fmt.Println("Servidor en :8080")
        if err := server.ListenAndServe(); err != http.ErrServerClosed {
            fmt.Fprintf(os.Stderr, "error: %v\n", err)
            os.Exit(1)
        }
    }()
    
    // Esperar senal
    ctx, stop := signal.NotifyContext(context.Background(),
        syscall.SIGINT, syscall.SIGTERM)
    defer stop()
    <-ctx.Done()
    
    // Graceful shutdown: dejar de aceptar, terminar requests en curso
    fmt.Println("\nCerrando servidor...")
    shutdownCtx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
    defer cancel()
    
    if err := server.Shutdown(shutdownCtx); err != nil {
        fmt.Fprintf(os.Stderr, "shutdown error: %v\n", err)
        os.Exit(1)
    }
    
    fmt.Println("Servidor cerrado correctamente")
}
```

---

## 8. Exit codes y convenciones

### 8.1 Convenciones de exit codes

```
┌───────┬──────────────────────────────────────────────────────────┐
│ Code  │ Significado                                              │
├───────┼──────────────────────────────────────────────────────────┤
│ 0     │ Exito                                                    │
│ 1     │ Error general                                            │
│ 2     │ Uso incorrecto (flags invalidos, etc.) — flag usa esto  │
│ 126   │ Comando encontrado pero no ejecutable                    │
│ 127   │ Comando no encontrado                                    │
│ 128+N │ Terminado por senal N (128+9=137 para SIGKILL)          │
│ 130   │ Ctrl+C (128 + 2 SIGINT)                                 │
└───────┴──────────────────────────────────────────────────────────┘
```

### 8.2 os.Exit vs return

```go
// os.Exit termina INMEDIATAMENTE:
// - NO ejecuta deferred functions
// - NO ejecuta defers de main
// - NO ejecuta finalizers
// - NO da oportunidad de cleanup

func main() {
    f, _ := os.Create("data.txt")
    defer f.Close() // NUNCA se ejecuta si usas os.Exit
    
    f.Write([]byte("datos importantes"))
    
    os.Exit(1) // f.Close() nunca se llama
    // Los datos pueden no haberse escrito al disco
}

// PATRON CORRECTO: usar una funcion interna que retorna error

func main() {
    if err := run(); err != nil {
        fmt.Fprintf(os.Stderr, "error: %v\n", err)
        os.Exit(1) // defers de run() YA se ejecutaron
    }
}

func run() error {
    f, err := os.Create("data.txt")
    if err != nil {
        return err
    }
    defer f.Close() // se ejecuta normalmente al retornar de run()
    
    _, err = f.Write([]byte("datos importantes"))
    return err
}
```

### 8.3 Patron run() para CLIs limpias

```go
// Este patron es el ESTANDAR en CLIs bien escritas en Go

package main

import (
    "context"
    "fmt"
    "os"
    "os/signal"
    "syscall"
)

func main() {
    ctx, stop := signal.NotifyContext(context.Background(),
        syscall.SIGINT, syscall.SIGTERM)
    defer stop()
    
    if err := run(ctx, os.Args[1:], os.Stdin, os.Stdout, os.Stderr); err != nil {
        fmt.Fprintf(os.Stderr, "error: %v\n", err)
        os.Exit(1)
    }
}

// run es la funcion real del programa.
// Recibe contexto, args, y los tres streams como dependencias.
// Esto hace que sea testeable: puedes inyectar buffers en tests.
func run(ctx context.Context, args []string, stdin io.Reader, stdout, stderr io.Writer) error {
    // ... toda la logica del programa ...
    return nil
}
```

---

## 9. Variables de entorno

### 9.1 Leer variables de entorno

```go
import "os"

func main() {
    // os.Getenv — retorna "" si no existe (no distingue "no existe" de "existe y es vacia")
    home := os.Getenv("HOME")
    port := os.Getenv("PORT")
    
    // os.LookupEnv — distingue entre "no existe" y "existe pero vacia"
    val, exists := os.LookupEnv("DATABASE_URL")
    if !exists {
        fmt.Println("DATABASE_URL no esta definida")
    } else if val == "" {
        fmt.Println("DATABASE_URL esta definida pero vacia")
    } else {
        fmt.Printf("DATABASE_URL = %s\n", val)
    }
    
    // Patron: default value
    port = os.Getenv("PORT")
    if port == "" {
        port = "8080"
    }
    
    // Funcion helper comun:
    func envOrDefault(key, defaultVal string) string {
        if val := os.Getenv(key); val != "" {
            return val
        }
        return defaultVal
    }
}
```

### 9.2 Listar y modificar variables de entorno

```go
// Listar todas las variables
for _, env := range os.Environ() {
    // Cada entrada es "KEY=VALUE"
    parts := strings.SplitN(env, "=", 2)
    key, value := parts[0], parts[1]
    fmt.Printf("%s = %s\n", key, value)
}

// Setear (solo afecta al proceso actual y sus hijos)
os.Setenv("MY_VAR", "hello")

// Borrar
os.Unsetenv("MY_VAR")

// Limpiar TODAS (peligroso — raramente util)
os.Clearenv()
```

### 9.3 Patron: configuracion desde env + flags

```go
// Las CLIs profesionales priorizan: flag > env > config file > default
// cobra+viper hacen esto automaticamente, pero puedes hacerlo manual:

func main() {
    portFlag := flag.Int("port", 0, "server port")
    flag.Parse()
    
    port := 8080 // default
    
    // Env override
    if envPort := os.Getenv("PORT"); envPort != "" {
        p, err := strconv.Atoi(envPort)
        if err != nil {
            fmt.Fprintf(os.Stderr, "invalid PORT: %s\n", envPort)
            os.Exit(1)
        }
        port = p
    }
    
    // Flag override (maximo prioridad)
    if *portFlag != 0 {
        port = *portFlag
    }
    
    fmt.Printf("Usando port %d\n", port)
}
```

---

## 10. Colores y formato en terminal

### 10.1 Codigos ANSI

```go
// Los terminales modernos soportan codigos de escape ANSI para colores

const (
    Reset     = "\033[0m"
    Bold      = "\033[1m"
    Dim       = "\033[2m"
    Italic    = "\033[3m"
    Underline = "\033[4m"
    
    // Colores de texto
    Red     = "\033[31m"
    Green   = "\033[32m"
    Yellow  = "\033[33m"
    Blue    = "\033[34m"
    Magenta = "\033[35m"
    Cyan    = "\033[36m"
    White   = "\033[37m"
    
    // Colores de fondo
    BgRed   = "\033[41m"
    BgGreen = "\033[42m"
)

func main() {
    fmt.Printf("%s%sERROR%s: archivo no encontrado\n", Bold, Red, Reset)
    fmt.Printf("%sSUCCESS%s: operacion completada\n", Green, Reset)
    fmt.Printf("%sWARNING%s: disco casi lleno\n", Yellow, Reset)
}
```

### 10.2 Colores solo cuando es un terminal

```go
// REGLA: no imprimir colores si stdout no es un terminal
// (sino ensucian la salida de pipes y archivos)

import "golang.org/x/term"

var (
    colorRed    = "\033[31m"
    colorGreen  = "\033[32m"
    colorYellow = "\033[33m"
    colorReset  = "\033[0m"
)

func init() {
    // Desactivar colores si no es un terminal
    if !term.IsTerminal(int(os.Stderr.Fd())) {
        colorRed = ""
        colorGreen = ""
        colorYellow = ""
        colorReset = ""
    }
    
    // Tambien respetar NO_COLOR (estandar comunitario)
    // https://no-color.org/
    if _, exists := os.LookupEnv("NO_COLOR"); exists {
        colorRed = ""
        colorGreen = ""
        colorYellow = ""
        colorReset = ""
    }
}

func logError(msg string) {
    fmt.Fprintf(os.Stderr, "%sERROR%s: %s\n", colorRed, colorReset, msg)
}

func logSuccess(msg string) {
    fmt.Fprintf(os.Stderr, "%sOK%s: %s\n", colorGreen, colorReset, msg)
}
```

### 10.3 Barras de progreso y spinners

```go
// Barra de progreso simple (sin dependencias)

func progressBar(current, total int, width int) string {
    percent := float64(current) / float64(total)
    filled := int(percent * float64(width))
    
    bar := strings.Repeat("█", filled) + strings.Repeat("░", width-filled)
    return fmt.Sprintf("\r[%s] %3.0f%% (%d/%d)", bar, percent*100, current, total)
}

func main() {
    total := 100
    for i := 0; i <= total; i++ {
        fmt.Fprint(os.Stderr, progressBar(i, total, 40))
        time.Sleep(50 * time.Millisecond)
    }
    fmt.Fprintln(os.Stderr) // nueva linea al final
}

// Spinner simple
func spinner(ctx context.Context, message string) {
    chars := []rune{'⠋', '⠙', '⠹', '⠸', '⠼', '⠴', '⠦', '⠧', '⠇', '⠏'}
    i := 0
    for {
        select {
        case <-ctx.Done():
            fmt.Fprintf(os.Stderr, "\r%s done\n", message)
            return
        default:
            fmt.Fprintf(os.Stderr, "\r%c %s", chars[i%len(chars)], message)
            i++
            time.Sleep(80 * time.Millisecond)
        }
    }
}
```

---

## 11. Input interactivo

### 11.1 Leer input del usuario

```go
// Metodo 1: fmt.Scan (simple pero limitado)
func main() {
    var name string
    fmt.Print("Nombre: ")
    fmt.Scan(&name) // lee una palabra (hasta whitespace)
    
    var age int
    fmt.Print("Edad: ")
    fmt.Scan(&age)
    
    fmt.Printf("Hola %s, tienes %d anos\n", name, age)
}

// Metodo 2: bufio.Scanner (lee lineas completas)
func main() {
    scanner := bufio.NewScanner(os.Stdin)
    
    fmt.Print("Nombre completo: ")
    scanner.Scan()
    name := scanner.Text()
    
    fmt.Print("Ciudad: ")
    scanner.Scan()
    city := scanner.Text()
    
    fmt.Printf("Hola %s de %s\n", name, city)
}

// Metodo 3: fmt.Scanln (lee hasta newline)
func main() {
    var first, last string
    fmt.Print("Nombre y apellido: ")
    fmt.Scanln(&first, &last) // lee dos palabras de una linea
}
```

### 11.2 Leer passwords (sin echo)

```go
import "golang.org/x/term"

func readPassword(prompt string) (string, error) {
    fmt.Fprint(os.Stderr, prompt)
    
    // ReadPassword desactiva echo en el terminal
    password, err := term.ReadPassword(int(os.Stdin.Fd()))
    fmt.Fprintln(os.Stderr) // nueva linea despues del password
    
    if err != nil {
        return "", err
    }
    return string(password), nil
}

func main() {
    pass, err := readPassword("Password: ")
    if err != nil {
        fmt.Fprintf(os.Stderr, "error: %v\n", err)
        os.Exit(1)
    }
    
    fmt.Printf("Password tiene %d caracteres\n", len(pass))
    // El password nunca se mostro en pantalla
}
```

### 11.3 Confirmaciones (y/n)

```go
func confirm(prompt string) bool {
    reader := bufio.NewReader(os.Stdin)
    
    for {
        fmt.Fprintf(os.Stderr, "%s [y/n]: ", prompt)
        input, _ := reader.ReadString('\n')
        input = strings.TrimSpace(strings.ToLower(input))
        
        switch input {
        case "y", "yes", "si", "s":
            return true
        case "n", "no":
            return false
        default:
            fmt.Fprintln(os.Stderr, "Por favor responde 'y' o 'n'")
        }
    }
}

func main() {
    if confirm("Eliminar todos los archivos?") {
        fmt.Println("Eliminando...")
    } else {
        fmt.Println("Cancelado")
    }
}
```

### 11.4 Menu interactivo

```go
func menu(title string, options []string) (int, error) {
    fmt.Fprintln(os.Stderr, title)
    fmt.Fprintln(os.Stderr, strings.Repeat("-", len(title)))
    
    for i, opt := range options {
        fmt.Fprintf(os.Stderr, "  %d. %s\n", i+1, opt)
    }
    
    reader := bufio.NewReader(os.Stdin)
    for {
        fmt.Fprintf(os.Stderr, "\nSelecciona [1-%d]: ", len(options))
        input, _ := reader.ReadString('\n')
        input = strings.TrimSpace(input)
        
        choice, err := strconv.Atoi(input)
        if err != nil || choice < 1 || choice > len(options) {
            fmt.Fprintln(os.Stderr, "Opcion invalida")
            continue
        }
        
        return choice - 1, nil // retorna indice 0-based
    }
}

func main() {
    idx, _ := menu("Formato de salida", []string{"JSON", "CSV", "XML", "Tabla"})
    formats := []string{"json", "csv", "xml", "table"}
    fmt.Printf("Seleccionado: %s\n", formats[idx])
}
```

---

## 12. Pipes y composicion Unix

### 12.1 Programa que funciona como filtro Unix

```go
// Un filtro lee de stdin, transforma, y escribe a stdout.
// Es el bloque basico de la filosofia Unix.

package main

import (
    "bufio"
    "fmt"
    "os"
    "strings"
)

func main() {
    // Este programa convierte todo a mayusculas
    // Uso: echo "hello world" | ./upper
    //      cat file.txt | ./upper > output.txt
    
    scanner := bufio.NewScanner(os.Stdin)
    writer := bufio.NewWriter(os.Stdout)
    defer writer.Flush()
    
    for scanner.Scan() {
        fmt.Fprintln(writer, strings.ToUpper(scanner.Text()))
    }
    
    if err := scanner.Err(); err != nil {
        fmt.Fprintf(os.Stderr, "error: %v\n", err)
        os.Exit(1)
    }
}
```

### 12.2 Programa que emite y consume pipes

```go
// Ejecutar un comando externo y procesar su salida

package main

import (
    "bufio"
    "fmt"
    "os"
    "os/exec"
    "strings"
)

func main() {
    // Ejecutar "ls -la" y procesar la salida
    cmd := exec.Command("ls", "-la")
    
    // Obtener stdout del comando como io.Reader
    stdout, err := cmd.StdoutPipe()
    if err != nil {
        fmt.Fprintf(os.Stderr, "error: %v\n", err)
        os.Exit(1)
    }
    
    // Stderr del comando va a nuestro stderr
    cmd.Stderr = os.Stderr
    
    if err := cmd.Start(); err != nil {
        fmt.Fprintf(os.Stderr, "error starting: %v\n", err)
        os.Exit(1)
    }
    
    // Leer y filtrar la salida linea por linea
    scanner := bufio.NewScanner(stdout)
    for scanner.Scan() {
        line := scanner.Text()
        // Solo mostrar archivos .go
        if strings.HasSuffix(line, ".go") {
            fmt.Println(line)
        }
    }
    
    if err := cmd.Wait(); err != nil {
        fmt.Fprintf(os.Stderr, "command failed: %v\n", err)
        os.Exit(1)
    }
}
```

### 12.3 Crear pipelines programaticamente

```go
// Simular: cat file.txt | grep "error" | wc -l

func main() {
    // Crear comandos
    cat := exec.Command("cat", "server.log")
    grep := exec.Command("grep", "-i", "error")
    wc := exec.Command("wc", "-l")
    
    // Conectar pipes
    var err error
    grep.Stdin, err = cat.StdoutPipe()
    if err != nil {
        log.Fatal(err)
    }
    wc.Stdin, err = grep.StdoutPipe()
    if err != nil {
        log.Fatal(err)
    }
    
    // Capturar salida final
    var out bytes.Buffer
    wc.Stdout = &out
    
    // Iniciar en orden inverso (o cualquier orden, realmente)
    wc.Start()
    grep.Start()
    cat.Start()
    
    // Esperar en orden
    cat.Wait()
    grep.Wait()
    wc.Wait()
    
    fmt.Printf("Lineas con error: %s", out.String())
}
```

### 12.4 Manejar SIGPIPE correctamente

```go
// Cuando un programa escribe a un pipe cerrado (e.g., "head" cierra el pipe
// despues de leer N lineas), Go recibe SIGPIPE.
//
// Por defecto, Go ignora SIGPIPE en stdout/stderr (exit code 0).
// Esto es generalmente lo correcto para filtros Unix.
//
// Si necesitas detectarlo:

import "golang.org/x/sys/unix"

func main() {
    writer := bufio.NewWriter(os.Stdout)
    defer writer.Flush()
    
    for i := 0; ; i++ {
        _, err := fmt.Fprintln(writer, i)
        if err != nil {
            // Probablemente SIGPIPE — el lector cerro el pipe
            // Esto es NORMAL, no es un error
            break
        }
    }
}

// Uso: ./count | head -5
// Imprime 0-4, luego head cierra el pipe, SIGPIPE llega, programa termina
```

---

## 13. Comparacion con C y Rust

```
┌──────────────────────────────┬────────────────────────┬─────────────────────────┬──────────────────────────┐
│ Aspecto                      │ C                      │ Go                      │ Rust                     │
├──────────────────────────────┼────────────────────────┼─────────────────────────┼──────────────────────────┤
│ Argumentos crudos            │ argc, argv[]           │ os.Args []string        │ std::env::args()         │
│ Tipo de argv                 │ char* argv[]           │ []string (UTF-8)        │ Iterator<String>         │
│ argv[0]                      │ nombre programa        │ os.Args[0]              │ env::args().next()       │
│ Parser basico stdlib         │ getopt()               │ flag package            │ (no — clap es de facto)  │
│ Parser POSIX                 │ getopt_long()          │ pflag (tercero)         │ clap (tercero)           │
│ Framework CLI                │ (no hay estandar)      │ cobra                   │ clap (derive + builder)  │
│ Subcomandos                  │ manual                 │ cobra.Command           │ clap Subcommand derive   │
│ Auto-completado              │ manual                 │ cobra (automatico)      │ clap_complete            │
│ Definir flags                │ struct option[]        │ flag.String("n",...)    │ #[arg(short, long)]      │
│ Tipo de flag                 │ siempre string         │ tipado (Int, Bool, etc.)│ tipado (derive)          │
│ Validacion de args           │ manual                 │ cobra.ExactArgs(n)      │ clap value_parser        │
│ stdin                        │ FILE* stdin            │ os.Stdin (*os.File)     │ io::stdin()              │
│ stdout                       │ FILE* stdout           │ os.Stdout (*os.File)    │ io::stdout()             │
│ stderr                       │ FILE* stderr           │ os.Stderr (*os.File)    │ io::stderr()             │
│ Escribir a stderr            │ fprintf(stderr,...)    │ fmt.Fprintln(os.Stderr) │ eprintln!()              │
│ Leer linea                   │ fgets/getline          │ bufio.Scanner           │ BufRead::read_line       │
│ Exit code                    │ exit(n) / return n     │ os.Exit(n)              │ std::process::exit(n)    │
│ Defers y exit                │ atexit() handlers      │ os.Exit SALTA defers    │ exit SALTA Drop          │
│ Variables de entorno         │ getenv()               │ os.Getenv()             │ env::var()               │
│ Env result type              │ char* (NULL si no)     │ string ("" si no)       │ Result<String>           │
│ Env distinguir vacio/ausente │ getenv()→NULL vs ""    │ os.LookupEnv()          │ env::var() Err vs Ok("") │
│ Senales                      │ signal()/sigaction()   │ os/signal.Notify        │ signal-hook / tokio      │
│ SIGINT handling              │ handler en C           │ signal.NotifyContext    │ ctrlc crate o tokio      │
│ Password sin echo            │ termios / getpass()    │ term.ReadPassword()     │ rpassword crate          │
│ Colores ANSI                 │ printf("\033[31m...")   │ fmt.Print("\033[31m")   │ println!("\x1b[31m...")   │
│ Check si es terminal         │ isatty()               │ term.IsTerminal()       │ atty/is-terminal crate   │
│ Ejecutar comandos            │ system()/popen()       │ os/exec.Command         │ std::process::Command    │
│ Pipe entre comandos          │ pipe()/dup2()          │ exec.Command.StdoutPipe │ .stdout(Stdio::piped())  │
│ Config files                 │ manual                 │ viper (tercero)         │ config-rs crate          │
│ Filosofia CLI                │ getopt (minimalista)   │ cobra (pragmatico)      │ clap (type-safe derive)  │
│ Compilacion                  │ binario estatico       │ binario estatico        │ binario estatico         │
│ Cross-compilation            │ complicado             │ GOOS=x GOARCH=y        │ cargo +target            │
│ Tamano binario CLI tipico    │ ~50KB                  │ ~5MB                    │ ~1MB                     │
│ Tiempo de startup            │ ~1ms                   │ ~3ms                    │ ~1ms                     │
└──────────────────────────────┴────────────────────────┴─────────────────────────┴──────────────────────────┘
```

### Insights clave

```
DEFINICION DE FLAGS:

// C (getopt_long):
static struct option long_options[] = {
    {"verbose", no_argument,       NULL, 'v'},
    {"output",  required_argument, NULL, 'o'},
    {"count",   required_argument, NULL, 'c'},
    {0, 0, 0, 0}
};
int opt;
while ((opt = getopt_long(argc, argv, "vo:c:", long_options, NULL)) != -1) {
    switch (opt) {
        case 'v': verbose = 1; break;
        case 'o': output = optarg; break;
        case 'c': count = atoi(optarg); break;
    }
}

// Go (flag):
verbose := flag.Bool("verbose", false, "enable verbose output")
output := flag.String("output", "", "output file")
count := flag.Int("count", 1, "count")
flag.Parse()

// Go (cobra):
cmd.Flags().BoolVarP(&verbose, "verbose", "v", false, "enable verbose output")
cmd.Flags().StringVarP(&output, "output", "o", "", "output file")
cmd.Flags().IntVarP(&count, "count", "c", 1, "count")

// Rust (clap derive):
#[derive(Parser)]
struct Args {
    #[arg(short, long)]
    verbose: bool,
    #[arg(short, long)]
    output: Option<String>,
    #[arg(short, long, default_value_t = 1)]
    count: usize,
}
let args = Args::parse();

FILOSOFIA:
  C:    manual, flexible, propenso a errores, nada de "magia"
  Go:   pragmatico, runtime reflection, framework (cobra) hace mucho
  Rust: type-safe, compile-time derive, si compila los flags son correctos
```

---

## 14. Programa completo: Task CLI

Un gestor de tareas CLI completo con cobra, persistencia en JSON, y composicion con pipes Unix.

```go
// ===== main.go =====
package main

import "taskcli/cmd"

func main() {
    cmd.Execute()
}
```

```go
// ===== cmd/root.go =====
package cmd

import (
    "fmt"
    "os"
    "path/filepath"
    
    "github.com/spf13/cobra"
)

var (
    dataFile string
    verbose  bool
    noColor  bool
)

var rootCmd = &cobra.Command{
    Use:   "task",
    Short: "Task — gestor de tareas en linea de comandos",
    Long: `Task es un gestor de tareas CLI que demuestra el uso de cobra,
persistencia en JSON, stdin/stdout para composicion con pipes,
y patrones idomiaticos de Go para herramientas de terminal.`,
    SilenceUsage: true, // No mostrar usage en errores de RunE
}

func Execute() {
    if err := rootCmd.Execute(); err != nil {
        os.Exit(1)
    }
}

func init() {
    // Persistent flags — disponibles en todos los subcomandos
    home, _ := os.UserHomeDir()
    defaultFile := filepath.Join(home, ".tasks.json")
    
    rootCmd.PersistentFlags().StringVar(&dataFile, "file", defaultFile, "data file path")
    rootCmd.PersistentFlags().BoolVarP(&verbose, "verbose", "v", false, "verbose output")
    rootCmd.PersistentFlags().BoolVar(&noColor, "no-color", false, "disable color output")
}
```

```go
// ===== cmd/model.go =====
package cmd

import (
    "encoding/json"
    "fmt"
    "os"
    "time"
)

type Priority int

const (
    PriorityLow Priority = iota
    PriorityMedium
    PriorityHigh
    PriorityCritical
)

func (p Priority) String() string {
    switch p {
    case PriorityLow:
        return "low"
    case PriorityMedium:
        return "medium"
    case PriorityHigh:
        return "high"
    case PriorityCritical:
        return "critical"
    default:
        return "unknown"
    }
}

func (p Priority) Color() string {
    if noColor {
        return ""
    }
    switch p {
    case PriorityLow:
        return "\033[36m" // cyan
    case PriorityMedium:
        return "\033[33m" // yellow
    case PriorityHigh:
        return "\033[31m" // red
    case PriorityCritical:
        return "\033[1;31m" // bold red
    default:
        return ""
    }
}

func parsePriority(s string) (Priority, error) {
    switch s {
    case "low", "l":
        return PriorityLow, nil
    case "medium", "med", "m":
        return PriorityMedium, nil
    case "high", "h":
        return PriorityHigh, nil
    case "critical", "crit", "c":
        return PriorityCritical, nil
    default:
        return 0, fmt.Errorf("invalid priority %q (valid: low, medium, high, critical)", s)
    }
}

type Task struct {
    ID          int       `json:"id"`
    Title       string    `json:"title"`
    Description string    `json:"description,omitempty"`
    Priority    Priority  `json:"priority"`
    Done        bool      `json:"done"`
    Tags        []string  `json:"tags,omitempty"`
    CreatedAt   time.Time `json:"created_at"`
    DoneAt      *time.Time `json:"done_at,omitempty"`
}

type TaskStore struct {
    Tasks  []Task `json:"tasks"`
    NextID int    `json:"next_id"`
}

func loadStore() (*TaskStore, error) {
    data, err := os.ReadFile(dataFile)
    if err != nil {
        if os.IsNotExist(err) {
            return &TaskStore{NextID: 1}, nil
        }
        return nil, fmt.Errorf("read %s: %w", dataFile, err)
    }
    
    var store TaskStore
    if err := json.Unmarshal(data, &store); err != nil {
        return nil, fmt.Errorf("parse %s: %w", dataFile, err)
    }
    
    return &store, nil
}

func saveStore(store *TaskStore) error {
    data, err := json.MarshalIndent(store, "", "  ")
    if err != nil {
        return fmt.Errorf("marshal: %w", err)
    }
    
    // Atomic write: write to temp, then rename
    tmpFile := dataFile + ".tmp"
    if err := os.WriteFile(tmpFile, data, 0644); err != nil {
        return fmt.Errorf("write %s: %w", tmpFile, err)
    }
    
    if err := os.Rename(tmpFile, dataFile); err != nil {
        os.Remove(tmpFile)
        return fmt.Errorf("rename: %w", err)
    }
    
    return nil
}
```

```go
// ===== cmd/add.go =====
package cmd

import (
    "bufio"
    "fmt"
    "os"
    "strings"
    "time"
    
    "github.com/spf13/cobra"
)

var addCmd = &cobra.Command{
    Use:   "add [title]",
    Short: "Agregar una nueva tarea",
    Long: `Agregar una nueva tarea. El titulo puede darse como argumento o leerse de stdin.
Si stdin es un pipe, cada linea se agrega como una tarea separada.`,
    Example: `  task add "Comprar leche"
  task add -p high -t urgent "Arreglar el bug"
  echo "Tarea desde pipe" | task add
  cat tareas.txt | task add -p medium`,
    RunE: func(cmd *cobra.Command, args []string) error {
        priority, _ := cmd.Flags().GetString("priority")
        description, _ := cmd.Flags().GetString("description")
        tags, _ := cmd.Flags().GetStringSlice("tag")
        
        prio := PriorityMedium
        if priority != "" {
            var err error
            prio, err = parsePriority(priority)
            if err != nil {
                return err
            }
        }
        
        store, err := loadStore()
        if err != nil {
            return err
        }
        
        var titles []string
        
        if len(args) > 0 {
            // Titulo desde argumentos
            titles = append(titles, strings.Join(args, " "))
        } else {
            // Leer desde stdin
            fi, _ := os.Stdin.Stat()
            if fi.Mode()&os.ModeCharDevice == 0 {
                // Stdin es un pipe — leer lineas
                scanner := bufio.NewScanner(os.Stdin)
                for scanner.Scan() {
                    line := strings.TrimSpace(scanner.Text())
                    if line != "" {
                        titles = append(titles, line)
                    }
                }
                if err := scanner.Err(); err != nil {
                    return fmt.Errorf("reading stdin: %w", err)
                }
            } else {
                return fmt.Errorf("titulo requerido: task add <titulo> o pipe")
            }
        }
        
        for _, title := range titles {
            task := Task{
                ID:          store.NextID,
                Title:       title,
                Description: description,
                Priority:    prio,
                Tags:        tags,
                CreatedAt:   time.Now(),
            }
            store.Tasks = append(store.Tasks, task)
            store.NextID++
            
            if verbose {
                fmt.Fprintf(os.Stderr, "Tarea #%d creada: %s\n", task.ID, task.Title)
            }
        }
        
        if err := saveStore(store); err != nil {
            return err
        }
        
        fmt.Fprintf(os.Stderr, "%d tarea(s) agregada(s)\n", len(titles))
        return nil
    },
}

func init() {
    addCmd.Flags().StringP("priority", "p", "medium", "priority (low, medium, high, critical)")
    addCmd.Flags().StringP("description", "d", "", "task description")
    addCmd.Flags().StringSliceP("tag", "t", nil, "tags (can be repeated)")
    
    rootCmd.AddCommand(addCmd)
}
```

```go
// ===== cmd/list.go =====
package cmd

import (
    "encoding/json"
    "fmt"
    "os"
    "strings"
    "text/tabwriter"
    "time"
    
    "github.com/spf13/cobra"
)

var listCmd = &cobra.Command{
    Use:     "list",
    Aliases: []string{"ls", "l"},
    Short:   "Listar tareas",
    Long: `Listar tareas con filtros opcionales. 
Por defecto muestra solo tareas pendientes.
Usa --all para ver todas incluyendo completadas.`,
    Example: `  task list
  task list --all
  task list --priority high
  task list --tag urgent
  task list --format json | jq '.[] | .title'
  task list --done`,
    RunE: func(cmd *cobra.Command, args []string) error {
        showAll, _ := cmd.Flags().GetBool("all")
        showDone, _ := cmd.Flags().GetBool("done")
        filterPriority, _ := cmd.Flags().GetString("priority")
        filterTag, _ := cmd.Flags().GetString("tag")
        format, _ := cmd.Flags().GetString("format")
        
        store, err := loadStore()
        if err != nil {
            return err
        }
        
        // Filtrar
        var filtered []Task
        for _, t := range store.Tasks {
            // Filtro done/pending
            if !showAll {
                if showDone && !t.Done {
                    continue
                }
                if !showDone && t.Done {
                    continue
                }
            }
            
            // Filtro por prioridad
            if filterPriority != "" {
                prio, err := parsePriority(filterPriority)
                if err != nil {
                    return err
                }
                if t.Priority != prio {
                    continue
                }
            }
            
            // Filtro por tag
            if filterTag != "" {
                found := false
                for _, tag := range t.Tags {
                    if tag == filterTag {
                        found = true
                        break
                    }
                }
                if !found {
                    continue
                }
            }
            
            filtered = append(filtered, t)
        }
        
        // Output
        switch format {
        case "json":
            return outputJSON(filtered)
        case "csv":
            return outputCSV(filtered)
        case "ids":
            return outputIDs(filtered)
        default:
            return outputTable(filtered)
        }
    },
}

func outputTable(tasks []Task) error {
    if len(tasks) == 0 {
        fmt.Fprintln(os.Stderr, "No hay tareas")
        return nil
    }
    
    reset := "\033[0m"
    dim := "\033[2m"
    green := "\033[32m"
    strikethrough := "\033[9m"
    if noColor {
        reset = ""
        dim = ""
        green = ""
        strikethrough = ""
    }
    
    w := tabwriter.NewWriter(os.Stdout, 0, 0, 2, ' ', 0)
    fmt.Fprintf(w, "%sID\tSTATUS\tPRIORITY\tTITLE\tTAGS\tAGE%s\n", dim, reset)
    
    for _, t := range tasks {
        status := "○"
        titleStyle := ""
        titleEnd := ""
        if t.Done {
            status = green + "✓" + reset
            titleStyle = strikethrough + dim
            titleEnd = reset
        }
        
        age := formatAge(time.Since(t.CreatedAt))
        tagsStr := strings.Join(t.Tags, ",")
        
        prioColor := t.Priority.Color()
        prioReset := reset
        if noColor {
            prioReset = ""
        }
        
        fmt.Fprintf(w, "%d\t%s\t%s%s%s\t%s%s%s\t%s\t%s\n",
            t.ID, status,
            prioColor, t.Priority, prioReset,
            titleStyle, t.Title, titleEnd,
            tagsStr, age)
    }
    
    return w.Flush()
}

func outputJSON(tasks []Task) error {
    encoder := json.NewEncoder(os.Stdout)
    encoder.SetIndent("", "  ")
    return encoder.Encode(tasks)
}

func outputCSV(tasks []Task) error {
    fmt.Println("id,title,priority,done,tags,created_at")
    for _, t := range tasks {
        tags := strings.Join(t.Tags, ";")
        fmt.Printf("%d,%q,%s,%v,%q,%s\n",
            t.ID, t.Title, t.Priority, t.Done, tags,
            t.CreatedAt.Format(time.RFC3339))
    }
    return nil
}

func outputIDs(tasks []Task) error {
    for _, t := range tasks {
        fmt.Println(t.ID)
    }
    return nil
}

func formatAge(d time.Duration) string {
    switch {
    case d < time.Minute:
        return "just now"
    case d < time.Hour:
        return fmt.Sprintf("%dm", int(d.Minutes()))
    case d < 24*time.Hour:
        return fmt.Sprintf("%dh", int(d.Hours()))
    default:
        days := int(d.Hours() / 24)
        if days == 1 {
            return "1d"
        }
        return fmt.Sprintf("%dd", days)
    }
}

func init() {
    listCmd.Flags().BoolP("all", "a", false, "show all tasks (including done)")
    listCmd.Flags().Bool("done", false, "show only completed tasks")
    listCmd.Flags().String("priority", "", "filter by priority")
    listCmd.Flags().String("tag", "", "filter by tag")
    listCmd.Flags().StringP("format", "f", "table", "output format (table, json, csv, ids)")
    
    rootCmd.AddCommand(listCmd)
}
```

```go
// ===== cmd/done.go =====
package cmd

import (
    "bufio"
    "fmt"
    "os"
    "strconv"
    "strings"
    "time"
    
    "github.com/spf13/cobra"
)

var doneCmd = &cobra.Command{
    Use:   "done <id> [id...]",
    Short: "Marcar tareas como completadas",
    Long: `Marcar una o mas tareas como completadas.
Los IDs pueden pasarse como argumentos o por stdin (uno por linea).`,
    Example: `  task done 1
  task done 1 2 3
  task list --format ids --priority high | task done
  echo "5" | task done`,
    RunE: func(cmd *cobra.Command, args []string) error {
        store, err := loadStore()
        if err != nil {
            return err
        }
        
        // Recoger IDs de args y stdin
        var ids []int
        
        for _, arg := range args {
            id, err := strconv.Atoi(arg)
            if err != nil {
                return fmt.Errorf("invalid id %q: %w", arg, err)
            }
            ids = append(ids, id)
        }
        
        // Leer de stdin si es pipe
        fi, _ := os.Stdin.Stat()
        if fi.Mode()&os.ModeCharDevice == 0 {
            scanner := bufio.NewScanner(os.Stdin)
            for scanner.Scan() {
                line := strings.TrimSpace(scanner.Text())
                if line == "" {
                    continue
                }
                id, err := strconv.Atoi(line)
                if err != nil {
                    return fmt.Errorf("invalid id from stdin %q: %w", line, err)
                }
                ids = append(ids, id)
            }
        }
        
        if len(ids) == 0 {
            return fmt.Errorf("at least one task ID is required")
        }
        
        // Marcar como done
        now := time.Now()
        completed := 0
        for _, id := range ids {
            for i := range store.Tasks {
                if store.Tasks[i].ID == id {
                    if store.Tasks[i].Done {
                        fmt.Fprintf(os.Stderr, "tarea #%d ya estaba completada\n", id)
                    } else {
                        store.Tasks[i].Done = true
                        store.Tasks[i].DoneAt = &now
                        completed++
                        if verbose {
                            fmt.Fprintf(os.Stderr, "✓ #%d: %s\n", id, store.Tasks[i].Title)
                        }
                    }
                    break
                }
            }
        }
        
        if err := saveStore(store); err != nil {
            return err
        }
        
        fmt.Fprintf(os.Stderr, "%d tarea(s) completada(s)\n", completed)
        return nil
    },
}

func init() {
    rootCmd.AddCommand(doneCmd)
}
```

```go
// ===== cmd/delete.go =====
package cmd

import (
    "bufio"
    "fmt"
    "os"
    "strconv"
    "strings"
    
    "github.com/spf13/cobra"
)

var deleteCmd = &cobra.Command{
    Use:     "delete <id> [id...]",
    Aliases: []string{"rm", "del"},
    Short:   "Eliminar tareas",
    Example: `  task delete 1
  task delete 1 2 3
  task list --done --format ids | task delete`,
    RunE: func(cmd *cobra.Command, args []string) error {
        force, _ := cmd.Flags().GetBool("force")
        
        store, err := loadStore()
        if err != nil {
            return err
        }
        
        // Recoger IDs
        var ids []int
        for _, arg := range args {
            id, err := strconv.Atoi(arg)
            if err != nil {
                return fmt.Errorf("invalid id %q: %w", arg, err)
            }
            ids = append(ids, id)
        }
        
        // Stdin
        fi, _ := os.Stdin.Stat()
        if fi.Mode()&os.ModeCharDevice == 0 {
            scanner := bufio.NewScanner(os.Stdin)
            for scanner.Scan() {
                line := strings.TrimSpace(scanner.Text())
                if line == "" {
                    continue
                }
                id, err := strconv.Atoi(line)
                if err != nil {
                    return fmt.Errorf("invalid id from stdin %q: %w", line, err)
                }
                ids = append(ids, id)
            }
        }
        
        if len(ids) == 0 {
            return fmt.Errorf("at least one task ID is required")
        }
        
        // Confirmacion (si no es force y es interactivo)
        if !force {
            fmt.Fprintf(os.Stderr, "Eliminar %d tarea(s)? [y/n]: ", len(ids))
            reader := bufio.NewReader(os.Stdin)
            input, _ := reader.ReadString('\n')
            input = strings.TrimSpace(strings.ToLower(input))
            if input != "y" && input != "yes" {
                fmt.Fprintln(os.Stderr, "Cancelado")
                return nil
            }
        }
        
        // Eliminar
        idSet := make(map[int]bool)
        for _, id := range ids {
            idSet[id] = true
        }
        
        var remaining []Task
        deleted := 0
        for _, t := range store.Tasks {
            if idSet[t.ID] {
                deleted++
                if verbose {
                    fmt.Fprintf(os.Stderr, "✗ #%d: %s\n", t.ID, t.Title)
                }
            } else {
                remaining = append(remaining, t)
            }
        }
        
        store.Tasks = remaining
        
        if err := saveStore(store); err != nil {
            return err
        }
        
        fmt.Fprintf(os.Stderr, "%d tarea(s) eliminada(s)\n", deleted)
        return nil
    },
}

func init() {
    deleteCmd.Flags().BoolP("force", "f", false, "skip confirmation")
    rootCmd.AddCommand(deleteCmd)
}
```

```go
// ===== cmd/stats.go =====
package cmd

import (
    "encoding/json"
    "fmt"
    "os"
    "sort"
    "strings"
    "text/tabwriter"
    
    "github.com/spf13/cobra"
)

var statsCmd = &cobra.Command{
    Use:   "stats",
    Short: "Mostrar estadisticas de tareas",
    RunE: func(cmd *cobra.Command, args []string) error {
        format, _ := cmd.Flags().GetString("format")
        
        store, err := loadStore()
        if err != nil {
            return err
        }
        
        total := len(store.Tasks)
        done := 0
        byPriority := make(map[string]int)
        byTag := make(map[string]int)
        
        for _, t := range store.Tasks {
            if t.Done {
                done++
            }
            byPriority[t.Priority.String()]++
            for _, tag := range t.Tags {
                byTag[tag]++
            }
        }
        
        pending := total - done
        
        if format == "json" {
            stats := map[string]interface{}{
                "total":       total,
                "done":        done,
                "pending":     pending,
                "by_priority": byPriority,
                "by_tag":      byTag,
            }
            encoder := json.NewEncoder(os.Stdout)
            encoder.SetIndent("", "  ")
            return encoder.Encode(stats)
        }
        
        // Table format
        fmt.Printf("Total: %d  |  Pendientes: %d  |  Completadas: %d\n\n", total, pending, done)
        
        if total > 0 {
            pct := float64(done) / float64(total) * 100
            barWidth := 30
            filled := int(pct / 100 * float64(barWidth))
            bar := strings.Repeat("█", filled) + strings.Repeat("░", barWidth-filled)
            fmt.Printf("Progreso: [%s] %.0f%%\n\n", bar, pct)
        }
        
        w := tabwriter.NewWriter(os.Stdout, 0, 0, 2, ' ', 0)
        fmt.Fprintln(w, "PRIORIDAD\tCANTIDAD")
        for _, p := range []string{"critical", "high", "medium", "low"} {
            if count, ok := byPriority[p]; ok {
                fmt.Fprintf(w, "%s\t%d\n", p, count)
            }
        }
        w.Flush()
        
        if len(byTag) > 0 {
            fmt.Println()
            
            // Ordenar tags por frecuencia
            type tagCount struct {
                tag   string
                count int
            }
            var tags []tagCount
            for tag, count := range byTag {
                tags = append(tags, tagCount{tag, count})
            }
            sort.Slice(tags, func(i, j int) bool {
                return tags[i].count > tags[j].count
            })
            
            w = tabwriter.NewWriter(os.Stdout, 0, 0, 2, ' ', 0)
            fmt.Fprintln(w, "TAG\tCANTIDAD")
            for _, tc := range tags {
                fmt.Fprintf(w, "%s\t%d\n", tc.tag, tc.count)
            }
            w.Flush()
        }
        
        return nil
    },
}

func init() {
    statsCmd.Flags().StringP("format", "f", "table", "output format (table, json)")
    rootCmd.AddCommand(statsCmd)
}
```

```go
// ===== cmd/import.go =====
package cmd

import (
    "bufio"
    "encoding/json"
    "fmt"
    "os"
    "strings"
    "time"
    
    "github.com/spf13/cobra"
)

var importCmd = &cobra.Command{
    Use:   "import [file]",
    Short: "Importar tareas desde JSON o texto plano",
    Long: `Importar tareas desde un archivo o stdin.
Formatos soportados:
  - JSON: array de objetos con campos title, priority, tags
  - Texto: una tarea por linea (se asigna prioridad medium)`,
    Example: `  task import tasks.json
  cat tasks.json | task import
  echo -e "Tarea 1\nTarea 2\nTarea 3" | task import --format text`,
    RunE: func(cmd *cobra.Command, args []string) error {
        format, _ := cmd.Flags().GetString("format")
        
        var reader *os.File
        var err error
        
        if len(args) > 0 {
            reader, err = os.Open(args[0])
            if err != nil {
                return err
            }
            defer reader.Close()
        } else {
            reader = os.Stdin
        }
        
        store, err := loadStore()
        if err != nil {
            return err
        }
        
        var count int
        
        switch format {
        case "json":
            count, err = importJSON(reader, store)
        case "text":
            count, err = importText(reader, store)
        default:
            return fmt.Errorf("unknown format %q (valid: json, text)", format)
        }
        
        if err != nil {
            return err
        }
        
        if err := saveStore(store); err != nil {
            return err
        }
        
        fmt.Fprintf(os.Stderr, "%d tarea(s) importada(s)\n", count)
        return nil
    },
}

type importTask struct {
    Title       string   `json:"title"`
    Description string   `json:"description"`
    Priority    string   `json:"priority"`
    Tags        []string `json:"tags"`
}

func importJSON(f *os.File, store *TaskStore) (int, error) {
    var tasks []importTask
    decoder := json.NewDecoder(f)
    if err := decoder.Decode(&tasks); err != nil {
        return 0, fmt.Errorf("json decode: %w", err)
    }
    
    count := 0
    for _, it := range tasks {
        if it.Title == "" {
            continue
        }
        
        prio := PriorityMedium
        if it.Priority != "" {
            var err error
            prio, err = parsePriority(it.Priority)
            if err != nil {
                fmt.Fprintf(os.Stderr, "warning: %v, using medium\n", err)
                prio = PriorityMedium
            }
        }
        
        task := Task{
            ID:          store.NextID,
            Title:       it.Title,
            Description: it.Description,
            Priority:    prio,
            Tags:        it.Tags,
            CreatedAt:   time.Now(),
        }
        store.Tasks = append(store.Tasks, task)
        store.NextID++
        count++
    }
    
    return count, nil
}

func importText(f *os.File, store *TaskStore) (int, error) {
    scanner := bufio.NewScanner(f)
    count := 0
    
    for scanner.Scan() {
        line := strings.TrimSpace(scanner.Text())
        if line == "" || strings.HasPrefix(line, "#") {
            continue // skip empty lines and comments
        }
        
        task := Task{
            ID:        store.NextID,
            Title:     line,
            Priority:  PriorityMedium,
            CreatedAt: time.Now(),
        }
        store.Tasks = append(store.Tasks, task)
        store.NextID++
        count++
    }
    
    return count, scanner.Err()
}

func init() {
    importCmd.Flags().String("format", "json", "input format (json, text)")
    rootCmd.AddCommand(importCmd)
}
```

```go
// ===== cmd/export.go =====
package cmd

import (
    "encoding/json"
    "fmt"
    "os"
    
    "github.com/spf13/cobra"
)

var exportCmd = &cobra.Command{
    Use:   "export",
    Short: "Exportar todas las tareas a JSON",
    Long: `Exporta todas las tareas en formato JSON a stdout.
Util para backup o para transferir entre maquinas.`,
    Example: `  task export > backup.json
  task export | jq '.tasks[] | select(.done == false)'
  task export | python3 -m json.tool`,
    RunE: func(cmd *cobra.Command, args []string) error {
        compact, _ := cmd.Flags().GetBool("compact")
        
        store, err := loadStore()
        if err != nil {
            return err
        }
        
        encoder := json.NewEncoder(os.Stdout)
        if !compact {
            encoder.SetIndent("", "  ")
        }
        
        return encoder.Encode(store)
    },
}

func init() {
    exportCmd.Flags().Bool("compact", false, "compact JSON (no indentation)")
    rootCmd.AddCommand(exportCmd)
}
```

```go
// ===== cmd/version.go =====
package cmd

import (
    "fmt"
    "runtime"
    
    "github.com/spf13/cobra"
)

// Estos se setean con ldflags en build time:
// go build -ldflags "-X taskcli/cmd.version=1.0.0 -X taskcli/cmd.commit=abc123"
var (
    version = "dev"
    commit  = "unknown"
    date    = "unknown"
)

var versionCmd = &cobra.Command{
    Use:   "version",
    Short: "Mostrar version del programa",
    Run: func(cmd *cobra.Command, args []string) {
        fmt.Printf("task v%s\n", version)
        fmt.Printf("  commit:  %s\n", commit)
        fmt.Printf("  built:   %s\n", date)
        fmt.Printf("  go:      %s\n", runtime.Version())
        fmt.Printf("  os/arch: %s/%s\n", runtime.GOOS, runtime.GOARCH)
    },
}

func init() {
    rootCmd.AddCommand(versionCmd)
}
```

```
PROBAR EL PROGRAMA:

# Build
cd taskcli
go mod init taskcli
go get github.com/spf13/cobra
go build -o task .

# Agregar tareas
./task add "Aprender cobra" -p high -t go -t cli
./task add "Escribir tests" -p medium -t go
./task add "Deploy a produccion" -p critical -t devops -t urgent
./task add "Actualizar documentacion" -p low -t docs

# Importar desde texto
echo -e "Revisar PR\nActualizar dependencias\nConfigurar CI" | ./task add

# Listar
./task list                          # tareas pendientes
./task ls --all                      # todas
./task list -f json | jq '.[].title' # JSON + jq
./task list --priority high          # solo high
./task list --tag go                 # solo con tag "go"

# Completar
./task done 1 2
./task list --format ids --priority critical | ./task done  # pipe IDs

# Estadisticas
./task stats
./task stats -f json

# Export/Import
./task export > backup.json
./task import backup.json

# Eliminar completadas
./task list --done --format ids | ./task delete -f

# Version
./task version

# Auto-completado
source <(./task completion bash)
./task <TAB>            # muestra: add, list, done, delete, stats, export, import, version
./task list --<TAB>     # muestra: --all, --done, --priority, --tag, --format

CONCEPTOS DEMOSTRADOS:
  cobra:        subcomandos, flags locales/persistentes, aliases, examples
  pflag:        --long, -s shorthand, StringSlice, flag requerido
  os.Args:      acceso indirecto via cobra
  stdin/stdout: lectura de pipe, output estructurado, composicion
  stderr:       mensajes de progreso y error separados de datos
  signals:      (via cobra internamente)
  JSON:         persistencia, import/export, output formateado
  tabwriter:    tablas alineadas en terminal
  ANSI:         colores con NO_COLOR support
  Pipes:        task list --format ids | task done (composicion Unix)
  ldflags:      version/commit inyectados en build time
  Atomic write: write-rename para guardar datos
```

---

## 15. Ejercicios

### Ejercicio 1: Filtro de texto configurable
Crea una herramienta de linea de comandos `textfilter` que:
- Lea de stdin o de archivos pasados como argumentos (patron Unix: si no hay args, usa stdin)
- Soporte estos filtros con flags: `--upper` (mayusculas), `--lower` (minusculas), `--trim` (eliminar whitespace), `--number` (agregar numeros de linea), `--grep <patron>` (filtrar lineas que contengan el patron)
- Los filtros se apliquen en el orden en que se pasan
- Escriba resultados a stdout y errores a stderr
- Soporte `--output <archivo>` para escribir a archivo en vez de stdout
- Usa solo el paquete `flag` de la stdlib (no cobra)

### Ejercicio 2: CLI con subcomandos usando flag.FlagSet
Crea un `kvstore` (key-value store) CLI usando solo la stdlib (`flag.FlagSet`, no cobra):
- Subcomandos: `set <key> <value>`, `get <key>`, `delete <key>`, `list`, `export`
- Persistencia en un archivo JSON (configurable con `-file`)
- `list` con filtro opcional `-prefix <prefijo>`
- `export` escribe a stdout en formato JSON o CSV (flag `-format`)
- Valida que `set` recibe exactamente 2 args posicionales, `get`/`delete` exactamente 1
- Implementa `--help` manual para cada subcomando

### Ejercicio 3: CLI con cobra y composicion de pipes
Crea un `logparser` con cobra que:
- Subcomando `parse`: lea logs de stdin o archivo, extraiga timestamp, level, y message
- Subcomando `filter`: filtre por level (`--level error`), rango de fechas (`--since`, `--until`), patron en message (`--grep`)
- Subcomando `stats`: cuente logs por level, muestre distribucion por hora
- Soporte formato de salida: `--format table|json|csv`
- Los subcomandos se puedan componer: `./logparser parse access.log | ./logparser filter --level error | ./logparser stats`
- Implemente auto-completado para shell (bash/zsh)

### Ejercicio 4: Herramienta interactiva con senales
Crea un `monitor` que:
- Muestre stats del sistema cada N segundos (flag `--interval`): CPU, memoria, disco (usa `/proc/stat`, `/proc/meminfo`, `df`)
- Soporte SIGINT (Ctrl+C) para graceful shutdown con resumen final
- Soporte SIGUSR1 para forzar una lectura inmediata (fuera del intervalo)
- Soporte SIGHUP para recargar config (un archivo JSON con thresholds de alerta)
- Escriba output a stdout (datos) y alertas a stderr (cuando un valor excede threshold)
- Soporte `--format table|json` para output (json es una linea NDJSON por lectura)
- Funcione con pipes: `./monitor --format json --interval 5 | jq --unbuffered '.cpu'`

---

> **Siguiente**: C10/S01 - TCP/UDP, T01 - net package — net.Dial, net.Listen, net.Conn, TCP client/server basico
