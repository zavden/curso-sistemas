# B22 Fundamentos de Go — Roadmap de Tópicos Pendientes

**Generado**: 2026-03-29
**Total tópicos definidos en BLOQUE_22.md**: 76
**Tópicos completados (README.md creados)**: 18
**Tópicos pendientes**: 58
**Progreso**: 23.7%

---

## Estado por Capítulo

| Capítulo | Tópicos | Hechos | Pendientes | Progreso |
|---|:---:|:---:|:---:|:---:|
| C01 Toolchain y Ecosistema | 7 | 7 | 0 | **100%** |
| C02 Tipos de Datos y Variables | 8 | 8 | 0 | **100%** |
| C03 Control de Flujo | 7 | 3 | 4 | 43% |
| C04 Funciones | 6 | 0 | 6 | 0% |
| C05 Tipos Compuestos | 8 | 0 | 8 | 0% |
| C06 Interfaces | 7 | 0 | 7 | 0% |
| C07 Manejo de Errores | 7 | 0 | 7 | 0% |
| C08 Concurrencia | 11 | 0 | 11 | 0% |
| C09 Paquetes e I/O | 7 | 0 | 7 | 0% |
| C10 Networking y HTTP | 6 | 0 | 6 | 0% |
| C11 Genéricos y Testing | 7 | 0 | 7 | 0% |

---

## Tópicos Completados (18)

### C01 Toolchain y Ecosistema — COMPLETO

- [x] S01/T01 — Instalación y go command
- [x] S01/T02 — Go Modules
- [x] S01/T03 — Estructura de proyecto
- [x] S01/T04 — Herramientas auxiliares
- [x] S02/T01 — Compilación estática
- [x] S02/T02 — Cross-compilation
- [x] S02/T03 — Build tags y ldflags

### C02 Tipos de Datos y Variables — COMPLETO

- [x] S01/T01 — Enteros
- [x] S01/T02 — Flotantes y complejos
- [x] S01/T03 — Booleanos y strings
- [x] S01/T04 — Zero values
- [x] S02/T01 — Declaración
- [x] S02/T02 — Constantes
- [x] S02/T03 — Conversiones de tipo
- [x] S02/T04 — Tipo rune

### C03 Control de Flujo — EN PROGRESO

- [x] S01/T01 — if/else
- [x] S01/T02 — for
- [x] S01/T03 — switch

---

## Tópicos Pendientes (58)

### C03 Control de Flujo — 4 pendientes
- [ ] **S01/T04 — Labels y goto**: break/continue con labels, goto (raro pero existe), cuándo es legítimo
- [ ] **S02/T01 — defer**: LIFO, evaluación de argumentos, defer en loops (¡cuidado!), patrones de cleanup
- [ ] **S02/T02 — panic y recover**: cuándo usar panic (programador errors), recover en deferred functions, no usar como excepciones
- [ ] **S02/T03 — Comparación con Rust**: defer vs Drop, panic vs panic!, recover vs catch_unwind

### C04 Funciones — 6 pendientes

- [ ] **S01/T01 — Declaración y múltiples retornos**: func, retornos nombrados, naked return (¡evitar!), blank identifier _
- [ ] **S01/T02 — Variadic functions**: ...T, paso de slices con ..., fmt.Println como ejemplo
- [ ] **S01/T03 — Funciones como valores**: asignación a variables, paso como argumento, retorno de funciones
- [ ] **S02/T01 — Closures**: captura de variables, closure sobre loop variable (pre-Go 1.22 vs post), goroutine closures
- [ ] **S02/T02 — init()**: función especial, orden de ejecución, múltiples init() por archivo, cuándo usarla (raro)
- [ ] **S02/T03 — Métodos**: receptor (value receiver vs pointer receiver), method sets, cuándo usar cada uno

### C05 Tipos Compuestos — 8 pendientes

- [ ] **S01/T01 — Arrays**: tamaño fijo, parte del tipo ([3]int ≠ [4]int), poco usados directamente
- [ ] **S01/T02 — Slices**: estructura interna (puntero, longitud, capacidad), make, append, copy
- [ ] **S01/T03 — Slicing y gotchas**: s[low:high:max], compartición de array subyacente, slice de slice, memory leaks
- [ ] **S01/T04 — Patrones con slices**: filtrado in-place, stack con append/slice, sort.Slice, slices package (Go 1.21+)
- [ ] **S02/T01 — Maps**: make, inserción, acceso (comma ok idiom), delete, iteración (orden no determinista)
- [ ] **S02/T02 — Structs**: declaración, inicialización, campos exportados (mayúscula), struct tags (json, yaml)
- [ ] **S02/T03 — Embedding**: composición sobre herencia, promoted methods, embedding de interfaces
- [ ] **S02/T04 — Comparación**: == en structs (solo si todos los campos son comparables), reflect.DeepEqual, maps package

### C06 Interfaces — 7 pendientes

- [ ] **S01/T01 — Interfaces implícitas**: satisfacción sin declaración explícita, duck typing estático
- [ ] **S01/T02 — Interface vacía (any)**: interface{} / any (Go 1.18+), type assertions, type switch
- [ ] **S01/T03 — Interfaces comunes**: io.Reader, io.Writer, fmt.Stringer, error, sort.Interface
- [ ] **S02/T01 — Interfaces pequeñas**: principio de Go, aceptar interfaces devolver structs
- [ ] **S02/T02 — Composición de interfaces**: io.ReadWriter, io.ReadCloser, embedding de interfaces
- [ ] **S02/T03 — nil interface vs interface con valor nil**: la trampa clásica de Go, por qué interface(nil) ≠ (*T)(nil)
- [ ] **S02/T04 — Comparación con Rust**: interfaces vs traits, satisfacción implícita vs explícita, method sets vs impl blocks

### C07 Manejo de Errores — 7 pendientes

- [ ] **S01/T01 — Error como valor**: el tipo error (interfaz), errors.New, fmt.Errorf, por qué no excepciones
- [ ] **S01/T02 — El patrón if err != nil**: idiomático, no repetitivo (según la filosofía Go), named returns para cleanup
- [ ] **S01/T03 — Wrapping de errores**: fmt.Errorf con %w, errors.Is, errors.As, cadena de errores
- [ ] **S01/T04 — Sentinel errors y tipos de error**: io.EOF, os.ErrNotExist, errores custom con campos
- [ ] **S02/T01 — Errgroup**: golang.org/x/sync/errgroup, goroutines con propagación de errores
- [ ] **S02/T02 — Error handling en APIs**: patrones para HTTP handlers, middleware de errores
- [ ] **S02/T03 — Comparación con Rust**: error vs Result<T,E>, wrapping vs From trait, ausencia de ? operator

### C08 Concurrencia — 11 pendientes

- [ ] **S01/T01 — Goroutines**: go keyword, modelo M:N (goroutines sobre OS threads), scheduler de Go
- [ ] **S01/T02 — Goroutines vs threads**: coste (~2KB stack vs ~1MB), crecimiento dinámico de stack, contexto de scheduling
- [ ] **S01/T03 — WaitGroup**: sync.WaitGroup, Add, Done, Wait — coordinación básica
- [ ] **S01/T04 — Data races**: race detector (-race flag), acceso concurrente a memoria compartida
- [ ] **S02/T01 — Channels básicos**: make(chan T), send (<-), receive, channels con y sin buffer
- [ ] **S02/T02 — Dirección de channels**: chan<- (send-only), <-chan (receive-only), cuándo anotar dirección
- [ ] **S02/T03 — select**: multiplexación de channels, default case, timeout con time.After
- [ ] **S02/T04 — Patrones con channels**: fan-out/fan-in, pipeline, done channel, semáforos
- [ ] **S03/T01 — Mutex**: sync.Mutex, sync.RWMutex, Lock/Unlock, defer unlock, cuándo preferir sobre channels
- [ ] **S03/T02 — Once, Map, Pool**: sync.Once (singleton), sync.Map (map concurrente), sync.Pool (reciclaje de objetos)
- [ ] **S03/T03 — Context**: context.Background, WithCancel, WithTimeout, WithValue, propagación de cancelación
- [ ] **S03/T04 — Comparación con Rust**: goroutines vs tokio tasks, channels vs mpsc, mutex vs Mutex<T>

### C09 Paquetes e I/O — 7 pendientes

- [ ] **S01/T01 — Paquetes**: convenciones de nombrado, visibilidad (mayúscula/minúscula), internal packages
- [ ] **S01/T02 — Biblioteca estándar destacada**: os, io, fmt, strings, strconv, filepath, encoding/json, net/http, log/slog
- [ ] **S01/T03 — Dependencias externas**: go get, versiones, replace directive, go mod vendor, módulos privados
- [ ] **S02/T01 — io.Reader y io.Writer**: la abstracción central, composición, io.Copy, io.TeeReader
- [ ] **S02/T02 — Archivos**: os.Open, os.Create, os.ReadFile, os.WriteFile, bufio.Scanner, bufio.Writer
- [ ] **S02/T03 — Encoding**: encoding/json (Marshal, Unmarshal, tags, streaming), encoding/csv, encoding/xml
- [ ] **S02/T04 — Línea de comandos**: os.Args, flag package, cobra (tercero), stdin/stdout/stderr

### C10 Networking y HTTP — 6 pendientes

- [ ] **S01/T01 — net package**: net.Dial, net.Listen, net.Conn, TCP client/server básico
- [ ] **S01/T02 — Servidores concurrentes**: goroutine por conexión, patrones de timeout, graceful shutdown
- [ ] **S02/T01 — net/http cliente**: http.Get, http.Client, timeouts, headers, cookies
- [ ] **S02/T02 — net/http servidor**: http.HandleFunc, http.ServeMux (Go 1.22+), middleware
- [ ] **S02/T03 — APIs REST**: JSON request/response, routing, status codes, patrones de handler
- [ ] **S02/T04 — Comparación con Rust**: net/http vs hyper/axum, simplicidad vs rendimiento

### C11 Genéricos y Testing — 7 pendientes

- [ ] **S01/T01 — Funciones genéricas**: type parameters, constraints, comparable, any
- [ ] **S01/T02 — Tipos genéricos**: structs y métodos con type parameters, limitaciones actuales
- [ ] **S01/T03 — Constraints**: interfaces como constraints, constraints package, union types (~int | ~float64), tilde (~)
- [ ] **S02/T01 — Tests básicos**: testing.T, go test, naming (*_test.go, Test*), t.Run (subtests), t.Parallel
- [ ] **S02/T02 — Table-driven tests**: el patrón idiomático de Go, cuándo y cómo usarlo
- [ ] **S02/T03 — Benchmarks**: testing.B, go test -bench, b.ResetTimer, b.ReportAllocs, pprof
- [ ] **S02/T04 — Testcontainers y mocking**: interfaces para testing, dependency injection, testcontainers-go

---

## Orden de Creación Sugerido

El siguiente tópico a crear sigue el orden secuencial de BLOQUE_22.md:

```
Próximo:  C03/S01/T04 — Labels y goto
Después:  C03/S02/T01 — defer
          C03/S02/T01 — defer
          C03/S02/T02 — panic y recover
          C03/S02/T03 — Comparación con Rust
          C04/S01/T01 — Declaración y múltiples retornos
          ...continúa secuencialmente...
```
