# Bloque 22: Fundamentos de Go

**Objetivo**: Dominar Go desde cero hasta nivel intermedio, con énfasis en
concurrencia, networking y las herramientas del ecosistema DevOps/SysAdmin.

## Capítulo 1: Toolchain y Ecosistema [A]

### Sección 1: Herramientas
- **T01 - Instalación y go command**: go build, go run, go install, go vet, go fmt — GOPATH vs módulos
- **T02 - Go Modules**: go mod init, go.mod, go.sum, versionado semántico, go mod tidy, go mod vendor
- **T03 - Estructura de proyecto**: layout convencional (cmd/, internal/, pkg/), main package, múltiples binarios
- **T04 - Herramientas auxiliares**: gofmt, goimports, golangci-lint, go doc, godoc, delve (debugger)

### Sección 2: Compilación y Cross-compilation
- **T01 - Compilación estática**: binarios autocontenidos, CGO_ENABLED=0, ventajas para contenedores
- **T02 - Cross-compilation**: GOOS, GOARCH, compilar para Linux/Windows/macOS/ARM desde cualquier OS
- **T03 - Build tags y ldflags**: //go:build, -ldflags "-X main.version=...", inyección de metadatos en build

## Capítulo 2: Tipos de Datos y Variables [A]

### Sección 1: Tipos Primitivos
- **T01 - Enteros**: int, int8-int64, uint, uint8-uint64, uintptr — tamaño de int depende de plataforma
- **T02 - Flotantes y complejos**: float32, float64, complex64, complex128 — math package
- **T03 - Booleanos y strings**: bool, string (inmutable, UTF-8, byte slice subyacente), rune vs byte
- **T04 - Zero values**: cada tipo tiene su zero value, no existe null para tipos valor

### Sección 2: Variables y Constantes
- **T01 - Declaración**: var, := (short declaration), múltiples declaraciones, scope de bloque
- **T02 - Constantes**: const, iota, constantes sin tipo (untyped constants), enumeraciones con iota
- **T03 - Conversiones de tipo**: casting explícito obligatorio, no hay coerción implícita, strconv package
- **T04 - Tipo rune**: Unicode code points, iteración por runes vs bytes, strings.Builder

## Capítulo 3: Control de Flujo [A]

### Sección 1: Condicionales y Bucles
- **T01 - if/else**: if con init statement, no hay paréntesis, scope del init
- **T02 - for**: el único loop — for clásico, for-range, for condicional, for infinito
- **T03 - switch**: sin fall-through por defecto, fallthrough keyword, switch sin expresión (if-else chain), type switch
- **T04 - Labels y goto**: break/continue con labels, goto (raro pero existe), cuándo es legítimo

### Sección 2: Defer, Panic y Recover
- **T01 - defer**: LIFO, evaluación de argumentos, defer en loops (¡cuidado!), patrones de cleanup
- **T02 - panic y recover**: cuándo usar panic (programador errors), recover en deferred functions, no usar como excepciones
- **T03 - Comparación con Rust**: defer vs Drop, panic vs panic!, recover vs catch_unwind

## Capítulo 4: Funciones [A]

### Sección 1: Fundamentos
- **T01 - Declaración y múltiples retornos**: func, retornos nombrados, naked return (¡evitar!), blank identifier _
- **T02 - Variadic functions**: ...T, paso de slices con ..., fmt.Println como ejemplo
- **T03 - Funciones como valores**: asignación a variables, paso como argumento, retorno de funciones

### Sección 2: Closures y Funciones Avanzadas
- **T01 - Closures**: captura de variables, closure sobre loop variable (pre-Go 1.22 vs post), goroutine closures
- **T02 - init()**: función especial, orden de ejecución, múltiples init() por archivo, cuándo usarla (raro)
- **T03 - Métodos**: receptor (value receiver vs pointer receiver), method sets, cuándo usar cada uno

## Capítulo 5: Tipos Compuestos [A]

### Sección 1: Arrays y Slices
- **T01 - Arrays**: tamaño fijo, parte del tipo ([3]int ≠ [4]int), poco usados directamente
- **T02 - Slices**: estructura interna (puntero, longitud, capacidad), make, append, copy
- **T03 - Slicing y gotchas**: s[low:high:max], compartición de array subyacente, slice de slice, memory leaks
- **T04 - Patrones con slices**: filtrado in-place, stack con append/slice, sort.Slice, slices package (Go 1.21+)

### Sección 2: Maps y Structs
- **T01 - Maps**: make, inserción, acceso (comma ok idiom), delete, iteración (orden no determinista)
- **T02 - Structs**: declaración, inicialización, campos exportados (mayúscula), struct tags (json, yaml)
- **T03 - Embedding**: composición sobre herencia, promoted methods, embedding de interfaces
- **T04 - Comparación**: == en structs (solo si todos los campos son comparables), reflect.DeepEqual, maps package

## Capítulo 6: Interfaces [A]

### Sección 1: Fundamentos
- **T01 - Interfaces implícitas**: satisfacción sin declaración explícita, duck typing estático
- **T02 - Interface vacía (any)**: interface{} / any (Go 1.18+), type assertions, type switch
- **T03 - Interfaces comunes**: io.Reader, io.Writer, fmt.Stringer, error, sort.Interface

### Sección 2: Patrones y Diseño
- **T01 - Interfaces pequeñas**: principio de Go, aceptar interfaces devolver structs
- **T02 - Composición de interfaces**: io.ReadWriter, io.ReadCloser, embedding de interfaces
- **T03 - nil interface vs interface con valor nil**: la trampa clásica de Go, por qué interface(nil) ≠ (*T)(nil)
- **T04 - Comparación con Rust**: interfaces vs traits, satisfacción implícita vs explícita, method sets vs impl blocks

## Capítulo 7: Manejo de Errores [A]

### Sección 1: El Patrón de Go
- **T01 - Error como valor**: el tipo error (interfaz), errors.New, fmt.Errorf, por qué no excepciones
- **T02 - El patrón if err != nil**: idiomático, no repetitivo (según la filosofía Go), named returns para cleanup
- **T03 - Wrapping de errores**: fmt.Errorf con %w, errors.Is, errors.As, cadena de errores
- **T04 - Sentinel errors y tipos de error**: io.EOF, os.ErrNotExist, errores custom con campos

### Sección 2: Patrones Avanzados
- **T01 - Errgroup**: golang.org/x/sync/errgroup, goroutines con propagación de errores
- **T02 - Error handling en APIs**: patrones para HTTP handlers, middleware de errores
- **T03 - Comparación con Rust**: error vs Result<T,E>, wrapping vs From trait, ausencia de ? operator

## Capítulo 8: Concurrencia [A]

### Sección 1: Goroutines
- **T01 - Goroutines**: go keyword, modelo M:N (goroutines sobre OS threads), scheduler de Go
- **T02 - Goroutines vs threads**: coste (~2KB stack vs ~1MB), crecimiento dinámico de stack, contexto de scheduling
- **T03 - WaitGroup**: sync.WaitGroup, Add, Done, Wait — coordinación básica
- **T04 - Data races**: race detector (-race flag), acceso concurrente a memoria compartida

### Sección 2: Channels
- **T01 - Channels básicos**: make(chan T), send (<-), receive, channels con y sin buffer
- **T02 - Dirección de channels**: chan<- (send-only), <-chan (receive-only), cuándo anotar dirección
- **T03 - select**: multiplexación de channels, default case, timeout con time.After
- **T04 - Patrones con channels**: fan-out/fan-in, pipeline, done channel, semáforos

### Sección 3: Sincronización
- **T01 - Mutex**: sync.Mutex, sync.RWMutex, Lock/Unlock, defer unlock, cuándo preferir sobre channels
- **T02 - Once, Map, Pool**: sync.Once (singleton), sync.Map (map concurrente), sync.Pool (reciclaje de objetos)
- **T03 - Context**: context.Background, WithCancel, WithTimeout, WithValue, propagación de cancelación
- **T04 - Comparación con Rust**: goroutines vs tokio tasks, channels vs mpsc, mutex vs Mutex<T>, share-memory vs message-passing

## Capítulo 9: Paquetes e I/O [A]

### Sección 1: Sistema de Paquetes
- **T01 - Paquetes**: convenciones de nombrado, visibilidad (mayúscula/minúscula), internal packages
- **T02 - Biblioteca estándar destacada**: os, io, fmt, strings, strconv, filepath, encoding/json, net/http, log/slog
- **T03 - Dependencias externas**: go get, versiones, replace directive, go mod vendor, módulos privados

### Sección 2: I/O
- **T01 - io.Reader y io.Writer**: la abstracción central, composición, io.Copy, io.TeeReader
- **T02 - Archivos**: os.Open, os.Create, os.ReadFile, os.WriteFile, bufio.Scanner, bufio.Writer
- **T03 - Encoding**: encoding/json (Marshal, Unmarshal, tags, Decoder/Encoder streaming), encoding/csv, encoding/xml
- **T04 - Línea de comandos**: os.Args, flag package, cobra (tercero), stdin/stdout/stderr

## Capítulo 10: Networking y HTTP [A]

### Sección 1: TCP/UDP
- **T01 - net package**: net.Dial, net.Listen, net.Conn, TCP client/server básico
- **T02 - Servidores concurrentes**: goroutine por conexión, patrones de timeout, graceful shutdown

### Sección 2: HTTP
- **T01 - net/http cliente**: http.Get, http.Client, timeouts, headers, cookies
- **T02 - net/http servidor**: http.HandleFunc, http.ServeMux (Go 1.22+ con métodos y patrones), middleware
- **T03 - APIs REST**: JSON request/response, routing, status codes, patrones de handler
- **T04 - Comparación con Rust**: net/http vs hyper/axum, simplicidad vs rendimiento/seguridad de tipos

## Capítulo 11: Genéricos y Testing [A]

### Sección 1: Genéricos (Go 1.18+)
- **T01 - Funciones genéricas**: type parameters, constraints, comparable, any
- **T02 - Tipos genéricos**: structs y métodos con type parameters, limitaciones actuales (no method type params)
- **T03 - Constraints**: interfaces como constraints, constraints package, union types (~int | ~float64), tilde (~)

### Sección 2: Testing
- **T01 - Tests básicos**: testing.T, go test, naming (*_test.go, Test*), t.Run (subtests), t.Parallel
- **T02 - Table-driven tests**: el patrón idiomático de Go, cuándo y cómo usarlo
- **T03 - Benchmarks**: testing.B, go test -bench, b.ResetTimer, b.ReportAllocs, pprof
- **T04 - Testcontainers y mocking**: interfaces para testing, dependency injection, testcontainers-go
