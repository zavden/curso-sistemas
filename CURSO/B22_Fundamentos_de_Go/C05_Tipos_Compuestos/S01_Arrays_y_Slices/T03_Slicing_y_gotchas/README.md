# Slicing y Gotchas — Comparticion Avanzada, Cadenas de Reslicing y Memory Leaks en Produccion

## 1. Introduccion

En T02 vimos las bases de slicing (`s[low:high]`, `s[low:high:max]`) y los gotchas fundamentales. Este topico profundiza en los **escenarios avanzados** que causan bugs en produccion: cadenas de reslicing donde multiples slices comparten el mismo backing array, corrupcion silenciosa por append en escenarios complejos, memory leaks diagnosticables con `pprof`, y las trampas de slices en goroutines concurrentes.

Estos no son problemas academicos — son los bugs que un SysAdmin/DevOps encuentra cuando un servicio Go consume memoria creciente sin razon aparente, cuando un pipeline de procesamiento corrompe datos intermitentemente, o cuando un deploy concurrente produce resultados no deterministas.

```
┌──────────────────────────────────────────────────────────────┐
│              SLICING Y GOTCHAS AVANZADOS                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Topicos profundos:                                          │
│                                                              │
│  1. Cadenas de reslicing: slice de slice de slice            │
│     a → b := a[1:4] → c := b[0:2] → d := c[1:]             │
│     Todos comparten UN solo backing array                    │
│                                                              │
│  2. Corrupcion multi-slice por append:                       │
│     Cuando 3+ slices comparten backing array,                │
│     un append en uno puede corromper TODOS los demas          │
│                                                              │
│  3. Memory leaks en produccion:                              │
│     ├─ Slice header retiene backing array gigante            │
│     ├─ Goroutine leaks con slices capturados                 │
│     ├─ Map values con slices retenidos                       │
│     └─ Diagnostico con pprof y runtime.MemStats              │
│                                                              │
│  4. Concurrencia y slices:                                   │
│     ├─ Data race: multiples goroutines + append              │
│     ├─ Range + goroutine: captura de variable                │
│     └─ Slice header no es atomico                            │
│                                                              │
│  5. Inspeccion interna con unsafe:                           │
│     Ver ptr/len/cap del slice header en runtime              │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Anatomia del reslicing: que ocurre internamente

### Los invariantes fundamentales

Cuando haces `s[low:high]`, Go crea un **nuevo slice header** que apunta al **mismo backing array**. Las reglas son:

```go
// Para s[low:high]:
// - ptr_nuevo = ptr_original + low * sizeof(T)
// - len_nuevo = high - low
// - cap_nuevo = cap_original - low
//
// Para s[low:high:max]:
// - ptr_nuevo = ptr_original + low * sizeof(T)
// - len_nuevo = high - low
// - cap_nuevo = max - low    ← la diferencia clave

// Restricciones (compile/runtime panic si se violan):
// 0 <= low <= high <= cap(s)         para s[low:high]
// 0 <= low <= high <= max <= cap(s)  para s[low:high:max]
```

### Verificar con unsafe: ver el slice header real

```go
package main

import (
    "fmt"
    "unsafe"
)

// SliceHeader expone los campos internos del slice
type SliceHeader struct {
    Data uintptr
    Len  int
    Cap  int
}

func inspectSlice[T any](name string, s []T) {
    sh := (*SliceHeader)(unsafe.Pointer(&s))
    fmt.Printf("%-8s → ptr=0x%x  len=%-3d cap=%-3d\n",
        name, sh.Data, sh.Len, sh.Cap)
}

func main() {
    arr := [8]int{10, 20, 30, 40, 50, 60, 70, 80}
    
    a := arr[:]     // slice completo
    b := a[2:5]     // sub-slice
    c := b[1:3]     // sub-sub-slice
    d := c[0:1:1]   // sub-sub-sub-slice con cap limitado
    
    inspectSlice("a", a)
    inspectSlice("b", b)
    inspectSlice("c", c)
    inspectSlice("d", d)
}
// Salida tipica (direcciones varian):
// a        → ptr=0xc000018080  len=8   cap=8
// b        → ptr=0xc000018090  len=3   cap=6   (ptr = a.ptr + 2*8)
// c        → ptr=0xc000018098  len=2   cap=5   (ptr = a.ptr + 3*8)
// d        → ptr=0xc000018098  len=1   cap=1   (ptr = a.ptr + 3*8, cap limitado)
```

### Diagrama de punteros compartidos

```
arr := [8]int{10, 20, 30, 40, 50, 60, 70, 80}

Backing array (memoria contigua):
┌────┬────┬────┬────┬────┬────┬────┬────┐
│ 10 │ 20 │ 30 │ 40 │ 50 │ 60 │ 70 │ 80 │
└────┴────┴────┴────┴────┴────┴────┴────┘
  [0]  [1]  [2]  [3]  [4]  [5]  [6]  [7]
  ↑                                    ↑
  a.ptr                          a.ptr + 7*8

a = arr[:]        →  ptr=[0]  len=8  cap=8
                     [10, 20, 30, 40, 50, 60, 70, 80]

b = a[2:5]        →  ptr=[2]  len=3  cap=6
                     [30, 40, 50]
                     ───────── visible
                     [30, 40, 50, 60, 70, 80] ← cap alcanza

c = b[1:3]        →  ptr=[3]  len=2  cap=5
                     [40, 50]
                     c comparte con b y con a

d = c[0:1:1]      →  ptr=[3]  len=1  cap=1
                     [40]
                     cap limitado → append realoca

La clave: a, b, c apuntan al MISMO array. Modificar c[0]
modifica b[1] y a[3] simultaneamente:

c[0] = 999
→ arr[3] = 999
→ a = [10, 20, 30, 999, 50, 60, 70, 80]
→ b = [30, 999, 50]
→ c = [999, 50]
```

---

## 3. Cadenas de reslicing: multiples niveles

### El problema de la capacidad heredada

Cada reslicing hereda la capacidad desde el inicio del sub-slice hasta el final del backing array. Esto significa que un slice profundamente anidado puede tener mucha mas capacidad de la que aparenta:

```go
original := make([]int, 0, 1000)
for i := 0; i < 1000; i++ {
    original = append(original, i)
}

// Extraer "solo 3 elementos":
small := original[500:503]
fmt.Println(len(small), cap(small)) // 3 500

// ¡small tiene cap=500! Porque:
// cap = cap_original - low = 1000 - 500 = 500
// Esos 500 slots de capacidad son el espacio restante
// del backing array de 1000 elementos

// ¿Que pasa si haces append a small?
small = append(small, 999)
fmt.Println(original[503]) // 999 — ¡corrupcion!
// El append no realoco porque cap(small) = 500 >> len(small) = 3
```

### Cadena de 4 niveles: rastrear la corrupcion

```go
data := []int{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}
// data: len=10, cap=10

level1 := data[2:8]   // [2,3,4,5,6,7]  len=6  cap=8
level2 := level1[1:4] // [3,4,5]        len=3  cap=7
level3 := level2[0:2] // [3,4]          len=2  cap=7

// Ahora: append a level3
level3 = append(level3, 999)

// ¿Que ocurrio?
// level3 tenia cap=7 (hereda de level2, que hereda de level1, que hereda de data)
// append escribe en level3[2], que es data[5]

fmt.Println(data)   // [0 1 2 3 4 999 6 7 8 9]  ← data[5] corrompido
fmt.Println(level1) // [2 3 4 999 6 7]           ← level1[3] corrompido
fmt.Println(level2) // [3 4 999]                  ← level2[2] corrompido
fmt.Println(level3) // [3 4 999]                  ← el que hizo el append

// UN append corrompido CUATRO slices a la vez
```

### Solucion: limitar capacidad en cada nivel

```go
data := []int{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}

// ✓ Siempre usar full slice expression al crear sub-slices
level1 := data[2:8:8]     // cap = 8-2 = 6 (solo lo visible)
level2 := level1[1:4:4]   // cap = 4-1 = 3
level3 := level2[0:2:2]   // cap = 2-0 = 2

level3 = append(level3, 999) // cap agotado → REALOCA
fmt.Println(data)   // [0 1 2 3 4 5 6 7 8 9]  ← intacto
fmt.Println(level1) // [2 3 4 5 6 7]           ← intacto
fmt.Println(level2) // [3 4 5]                  ← intacto
fmt.Println(level3) // [3 4 999]                ← nuevo backing array
```

### La regla de oro del reslicing

```
┌──────────────────────────────────────────────────────────────┐
│  REGLA DE ORO                                                │
│                                                              │
│  Cada vez que creas un sub-slice que:                        │
│  1. Sera pasado a otra funcion, o                            │
│  2. Recibira append, o                                       │
│  3. Sera almacenado para uso posterior                       │
│                                                              │
│  → USA FULL SLICE EXPRESSION: s[low:high:high]               │
│                                                              │
│  Esto fuerza cap == len, y cualquier append futuro            │
│  realocara un nuevo backing array independiente.              │
│                                                              │
│  Si necesitas una copia completamente independiente:          │
│  → slices.Clone(s[low:high])                                 │
│  → append([]T(nil), s[low:high]...)                          │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 4. Corrupcion por append: escenarios complejos

### Escenario 1: Dos sub-slices del mismo segmento

```go
original := []int{1, 2, 3, 4, 5, 6, 7, 8}

// Dos "vistas" del mismo rango:
viewA := original[2:4]  // [3, 4]       cap=6
viewB := original[2:6]  // [3, 4, 5, 6] cap=6

// append a viewA
viewA = append(viewA, 99)

// viewA = [3, 4, 99]
// Pero 99 se escribio en original[4], que era 5

fmt.Println(original) // [1 2 3 4 99 6 7 8]  ← corrompido
fmt.Println(viewB)    // [3 4 99 6]           ← ¡viewB tambien corrompido!
// viewB[2] era 5, ahora es 99

// El bug es sutil: viewA y viewB parecen independientes
// pero comparten el mismo backing array
```

### Escenario 2: Funcion que retorna sub-slice

```go
func firstN(items []string, n int) []string {
    if n > len(items) {
        n = len(items)
    }
    return items[:n] // ✗ Retorna sub-slice con cap = cap(items)
}

func main() {
    all := make([]string, 0, 100)
    all = append(all, "a", "b", "c", "d", "e")

    first3 := firstN(all, 3) // ["a", "b", "c"]  cap=100!
    
    // El caller agrega a first3 sin saber que comparte memoria
    first3 = append(first3, "X")
    
    fmt.Println(all) // [a b c X e] ← all[3] corrompido!
}

// ✓ Solucion: la funcion debe proteger
func firstN(items []string, n int) []string {
    if n > len(items) {
        n = len(items)
    }
    return items[:n:n] // Full slice expression: cap = n
}

// ✓ O retornar copia
func firstN(items []string, n int) []string {
    if n > len(items) {
        n = len(items)
    }
    return slices.Clone(items[:n])
}
```

### Escenario 3: Delete corrompe slices hermanos

```go
// La operacion "delete" clasica:
// append(s[:i], s[i+1:]...)
// Esto hace un shift-left que modifica el backing array

original := []int{10, 20, 30, 40, 50}
snapshot := original[:]  // Snapshot "de seguridad"

// Eliminar elemento en indice 2 (valor 30)
original = append(original[:2], original[3:]...)

fmt.Println(original) // [10 20 40 50]     — len=4
fmt.Println(snapshot)  // [10 20 40 50 50]  — ¡corrompido!

// ¿Que paso?
// append(original[:2], original[3:]...) hace:
// 1. dst = original[:2] → [10, 20] con cap del backing array
// 2. Copia [40, 50] a dst[2] y dst[3]
// 3. El backing array ahora es: [10, 20, 40, 50, 50]
//                                                ↑ viejo valor
// 4. snapshot apunta al mismo array → ve la corrupcion
```

```
Antes de delete:
backing array:  [10, 20, 30, 40, 50]
                  ↑                ↑
original ───────────────────────────  len=5
snapshot ───────────────────────────  len=5

Despues de append(original[:2], original[3:]...):
backing array:  [10, 20, 40, 50, 50]
                  ↑           ↑    ↑
original ─────────────────────┘    │  len=4 (nuevo header)
snapshot ──────────────────────────┘  len=5 (viejo header)
                     shift-left──→ duplicado
```

### Escenario 4: Append condicional no determinista

```go
// El gotcha mas sutil: el resultado depende de si append realoca
func process(items []int) []int {
    result := items[:0] // Reutilizar backing array
    for _, item := range items {
        if item > 0 {
            result = append(result, item)
        }
    }
    return result
}

data := []int{1, -2, 3, -4, 5}
filtered := process(data)
fmt.Println(filtered) // [1 3 5]
fmt.Println(data)     // [1 3 5 -4 5] ← data corrompido

// items[:0] tiene len=0 pero cap=cap(items)
// append escribe sobre los elementos originales
// El resultado "funciona" pero el input quedo destrozado

// ✓ Solucion: no reutilizar backing array ajeno
func process(items []int) []int {
    var result []int // nil — su propio backing array
    for _, item := range items {
        if item > 0 {
            result = append(result, item)
        }
    }
    return result
}
```

---

## 5. Memory leaks por sub-slices: diagnostico en profundidad

### El problema fundamental

El GC de Go no puede recolectar un backing array mientras **cualquier** slice lo referencie. Un slice de 10 bytes puede mantener vivo un array de 100 MB:

```go
func processFile(path string) []byte {
    data, _ := os.ReadFile(path) // 100 MB
    
    // Extraer solo la cabecera (64 bytes)
    header := data[:64] // ✗ header retiene 100 MB
    
    return header
    // data sale de scope, pero header apunta al mismo backing array
    // GC NO puede recolectar los 100 MB
}

// ✓ Solucion: copiar los bytes necesarios
func processFile(path string) []byte {
    data, _ := os.ReadFile(path) // 100 MB
    header := make([]byte, 64)
    copy(header, data)
    return header
    // data sale de scope → 100 MB recolectados por GC
}
```

### Leak acumulativo: el patron real en produccion

```go
// Escenario: servicio que procesa logs continuamente
type LogProcessor struct {
    recentErrors []string // Almacena mensajes de error recientes
}

func (lp *LogProcessor) ProcessLine(line string) {
    // Simular parseo: extraer nivel
    if idx := strings.Index(line, "ERROR"); idx >= 0 {
        // ✗ El slice de string retiene el string completo
        // En Go, strings son inmutables pero un substring
        // COMPARTE el backing array del string original
        // (en Go < 1.? esto era cierto, en Go moderno strings.Clone ayuda)
        
        // Pero con []byte el problema es claro:
        lp.recentErrors = append(lp.recentErrors,
            string(line[idx:])) // Esto crea copia — OK para strings
    }
}

// El problema real es con []byte:
type ByteLogProcessor struct {
    signatures [][]byte
}

func (blp *ByteLogProcessor) ProcessChunk(chunk []byte) {
    // chunk puede ser 1 MB del archivo
    lines := bytes.Split(chunk, []byte("\n"))
    for _, line := range lines {
        if bytes.Contains(line, []byte("CRITICAL")) {
            // ✗ line es sub-slice de chunk → retiene 1 MB entero
            blp.signatures = append(blp.signatures, line)
        }
    }
    // Despues de 1000 chunks: 1000 * 1 MB = 1 GB retenido
    // aunque solo guardamos unos KB de "signatures"
}

// ✓ Solucion:
func (blp *ByteLogProcessor) ProcessChunk(chunk []byte) {
    lines := bytes.Split(chunk, []byte("\n"))
    for _, line := range lines {
        if bytes.Contains(line, []byte("CRITICAL")) {
            blp.signatures = append(blp.signatures, bytes.Clone(line))
        }
    }
}
```

### Leak por map con slices como valores

```go
type Cache struct {
    data map[string][]byte
}

func (c *Cache) Store(key string, fullResponse []byte) {
    // fullResponse es 10 KB del HTTP response
    // Solo queremos guardar los primeros 100 bytes
    
    // ✗ Sub-slice retiene los 10 KB completos
    c.data[key] = fullResponse[:100]
    
    // Con 10,000 entradas: 10,000 * 10 KB = 100 MB
    // Esperado:            10,000 * 100 B  = 1 MB
    // Factor de desperdicio: 100x
}

// ✓ Copiar antes de almacenar
func (c *Cache) Store(key string, fullResponse []byte) {
    c.data[key] = bytes.Clone(fullResponse[:100])
}
```

### Leak por struct con slice fields

```go
type Request struct {
    RawBody  []byte
    Headers  map[string]string
    Metadata []byte // Solo 32 bytes de metadata
}

func extractMetadata(raw []byte) *Request {
    // raw es el request completo: 50 KB
    return &Request{
        Metadata: raw[4:36], // ✗ Retiene 50 KB
    }
    // RawBody es nil — no retiene nada
    // Pero Metadata es sub-slice de raw → retiene 50 KB
}

// Despues de 100,000 requests:
// Esperado: 100,000 * 32 B = 3.2 MB
// Real:     100,000 * 50 KB = 5 GB

// ✓ Copiar metadata
func extractMetadata(raw []byte) *Request {
    meta := make([]byte, 32)
    copy(meta, raw[4:36])
    return &Request{
        Metadata: meta,
    }
}
```

---

## 6. Diagnostico de memory leaks con pprof

### Setup basico de pprof

```go
import (
    "net/http"
    _ "net/http/pprof" // Side-effect import: registra handlers
    "runtime"
)

func main() {
    // Endpoint de debug (NUNCA exponer en produccion sin auth)
    go func() {
        http.ListenAndServe("localhost:6060", nil)
    }()
    
    // ... tu aplicacion ...
}
```

### Comandos de diagnostico

```bash
# Capturar heap profile
go tool pprof http://localhost:6060/debug/pprof/heap

# En el shell interactivo de pprof:
# top20              — top 20 funciones por uso de memoria
# top20 -cum         — top 20 por uso acumulativo
# list processChunk  — ver linea por linea el uso en esa funcion
# web                — abrir grafo en navegador
# png > heap.png     — exportar grafo

# Comparar dos snapshots (antes y despues):
curl -o before.prof http://localhost:6060/debug/pprof/heap
# ... esperar o provocar el leak ...
curl -o after.prof http://localhost:6060/debug/pprof/heap

go tool pprof -base before.prof after.prof
# Ahora `top` muestra solo el DELTA — las alocaciones nuevas
```

### Usar runtime.MemStats para deteccion programatica

```go
func memUsage() {
    var m runtime.MemStats
    runtime.ReadMemStats(&m)
    
    fmt.Printf("Alloc = %v MB\n", m.Alloc/1024/1024)
    fmt.Printf("TotalAlloc = %v MB\n", m.TotalAlloc/1024/1024)
    fmt.Printf("Sys = %v MB\n", m.Sys/1024/1024)
    fmt.Printf("NumGC = %v\n", m.NumGC)
    fmt.Printf("HeapObjects = %v\n", m.HeapObjects)
}

// Patron para detectar leaks en tests:
func TestNoMemoryLeak(t *testing.T) {
    runtime.GC() // Forzar recoleccion
    var before runtime.MemStats
    runtime.ReadMemStats(&before)
    
    // Ejecutar la operacion sospechosa
    for i := 0; i < 10000; i++ {
        processChunk(generateChunk())
    }
    
    runtime.GC()
    var after runtime.MemStats
    runtime.ReadMemStats(&after)
    
    // Verificar que la memoria no crecio desproporcionadamente
    growth := after.Alloc - before.Alloc
    if growth > 10*1024*1024 { // 10 MB
        t.Errorf("possible memory leak: grew %d MB", growth/1024/1024)
    }
}
```

### Patron completo: monitor de memoria en servicio

```go
func startMemoryMonitor(interval time.Duration, threshold uint64) {
    ticker := time.NewTicker(interval)
    defer ticker.Stop()
    
    var baseline uint64
    for range ticker.C {
        var m runtime.MemStats
        runtime.ReadMemStats(&m)
        
        if baseline == 0 {
            baseline = m.Alloc
            continue
        }
        
        if m.Alloc > baseline+threshold {
            log.Printf("MEMORY WARNING: current=%dMB baseline=%dMB delta=%dMB objects=%d",
                m.Alloc/1024/1024,
                baseline/1024/1024,
                (m.Alloc-baseline)/1024/1024,
                m.HeapObjects)
        }
    }
}

// En main:
go startMemoryMonitor(30*time.Second, 100*1024*1024) // Alerta si crece >100MB
```

---

## 7. Concurrencia y slices: data races

### Data race basica: append concurrente

```go
// ✗ BUG: data race — append no es thread-safe
func collectResults() []string {
    var results []string
    var wg sync.WaitGroup
    
    for i := 0; i < 100; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            result := fmt.Sprintf("result-%d", id)
            results = append(results, result) // ✗ DATA RACE
        }(i)
    }
    
    wg.Wait()
    return results // Longitud no determinista, posible corrupcion
}

// ¿Por que es una data race?
// append hace:
// 1. Lee len(results) y cap(results) — lectura
// 2. Posiblemente aloca nuevo array — escritura de ptr
// 3. Escribe el nuevo elemento — escritura
// 4. Actualiza len — escritura
// Si dos goroutines hacen esto simultaneamente, pueden leer
// el mismo len, escribir en la misma posicion, y una escritura se pierde
```

### Deteccion con race detector

```bash
# Compilar y ejecutar con race detector
go run -race main.go

# Salida tipica:
# WARNING: DATA RACE
# Write at 0x00c000014090 by goroutine 7:
#   runtime.growslice()
#   main.collectResults.func1()
# Previous write at 0x00c000014090 by goroutine 6:
#   runtime.growslice()
#   main.collectResults.func1()

# En tests:
go test -race ./...

# En CI — SIEMPRE activar en tests:
# go test -race -count=1 ./...
```

### Solucion 1: Mutex

```go
func collectResults() []string {
    var (
        results []string
        mu      sync.Mutex
    )
    var wg sync.WaitGroup
    
    for i := 0; i < 100; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            result := fmt.Sprintf("result-%d", id)
            
            mu.Lock()
            results = append(results, result)
            mu.Unlock()
        }(i)
    }
    
    wg.Wait()
    return results
}
```

### Solucion 2: Channel collector

```go
func collectResults() []string {
    ch := make(chan string, 100)
    var wg sync.WaitGroup
    
    for i := 0; i < 100; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            ch <- fmt.Sprintf("result-%d", id)
        }(i)
    }
    
    // Cerrar channel cuando todas las goroutines terminen
    go func() {
        wg.Wait()
        close(ch)
    }()
    
    // Recolectar resultados en una sola goroutine — sin race
    var results []string
    for result := range ch {
        results = append(results, result)
    }
    return results
}
```

### Solucion 3: Pre-alocar por indice

```go
func collectResults() []string {
    results := make([]string, 100) // Pre-alocar posiciones
    var wg sync.WaitGroup
    
    for i := 0; i < 100; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            results[id] = fmt.Sprintf("result-%d", id) // Cada goroutine escribe en su indice
        }(i)
    }
    
    wg.Wait()
    return results
}

// ✓ Esto es SEGURO porque:
// 1. El slice no crece (no hay append → no hay realocacion)
// 2. Cada goroutine escribe en un indice DIFERENTE
// 3. No hay lectura/escritura concurrente en la misma posicion
// El race detector NO reporta esto como data race
```

### Slice header no es atomico

```go
// El slice header tiene 3 campos (ptr, len, cap)
// Una lectura concurrente mientras otra goroutine
// hace append puede leer un estado inconsistente:
// - ptr viejo con len nuevo
// - ptr nuevo con len viejo

// Esto puede causar:
// - Acceso a memoria invalida
// - Leer datos parcialmente escritos
// - Panic por out-of-bounds

// ✗ Nunca compartir un slice sin sincronizacion
// si alguna goroutine puede modificar el header (append, reslice)
```

---

## 8. Goroutines y closures sobre slices

### Captura del slice en closure

```go
// ✗ BUG: todas las goroutines ven el mismo slice header final
func processAll(items []string) {
    for i := 0; i < len(items); i++ {
        sub := items[i : i+1] // Sub-slice cambia en cada iteracion
        go func() {
            fmt.Println(sub) // ¿Que sub ve cada goroutine?
        }()
    }
    // Las goroutines capturan la VARIABLE sub por referencia
    // No capturan el valor del slice en ese momento
    // Cuando se ejecutan, sub tiene el valor de la ultima iteracion
}

// ✓ Solucion: pasar como parametro
func processAll(items []string) {
    for i := 0; i < len(items); i++ {
        sub := items[i : i+1]
        go func(s []string) {
            fmt.Println(s) // Copia del header — valor correcto
        }(sub)
    }
}

// ✓ En Go 1.22+ con GOEXPERIMENT=loopvar (Go 1.23+ por defecto):
// La variable del loop es per-iteration, asi que la captura es segura
func processAll(items []string) {
    for i := 0; i < len(items); i++ {
        sub := items[i : i+1] // sub es unica por iteracion en Go 1.22+
        go func() {
            fmt.Println(sub) // OK en Go 1.22+
        }()
    }
}
```

### Goroutine con slice que crece

```go
// ✗ BUG: goroutine captura referencia a slice que muta
func monitor(hosts []string) {
    go func() {
        for {
            time.Sleep(time.Second)
            // La goroutine lee len(hosts) — pero el caller
            // puede estar haciendo append que cambia el header
            for _, h := range hosts {
                checkHost(h) // ✗ hosts puede haber cambiado
            }
        }
    }()
    
    // En otro lugar:
    hosts = append(hosts, "new-host") // Puede realocar
    // La goroutine tiene el header viejo → datos stale o panic
}

// ✓ Solucion: pasar copia
func monitor(hosts []string) {
    snapshot := slices.Clone(hosts) // Copia independiente
    go func() {
        for {
            time.Sleep(time.Second)
            for _, h := range snapshot {
                checkHost(h) // snapshot nunca cambia
            }
        }
    }()
}

// ✓ O usar channel para actualizaciones
func monitor(hostsCh <-chan []string) {
    go func() {
        var current []string
        for {
            select {
            case hosts := <-hostsCh:
                current = hosts // Actualizar snapshot
            default:
                for _, h := range current {
                    checkHost(h)
                }
                time.Sleep(time.Second)
            }
        }
    }()
}
```

---

## 9. Reslicing trampas: bounds y panics

### Reslicing mas alla de len (pero dentro de cap)

```go
s := make([]int, 3, 10)
s[0], s[1], s[2] = 1, 2, 3

// s[5] → panic: index out of range [5] with length 3
// Pero s[0:5] es valido si cap >= 5:
extended := s[0:5]
fmt.Println(extended) // [1 2 3 0 0] — accede a los zero values ocultos

// Esto "extiende" el slice revelando el backing array
// Es legal pero peligroso — esos zero values podrian ser
// basura de una iteracion anterior si append reuso el array
```

### El gotcha del reslicing tras delete

```go
items := []string{"a", "b", "c", "d", "e"}

// Eliminar "c" (indice 2)
items = append(items[:2], items[3:]...)
fmt.Println(items) // [a b d e]
fmt.Println(len(items), cap(items)) // 4 5

// El backing array ahora es: ["a", "b", "d", "e", "e"]
//                                                  ↑ fantasma
// El antiguo items[4] = "e" sigue ahi

// Si alguien extiende el slice:
extended := items[:5] // Legal (cap=5)
fmt.Println(extended[4]) // "e" — el fantasma

// Para strings, esto es un leak (el string no se recolecta)
// Para punteros, es un leak del objeto apuntado

// ✓ Solucion: limpiar el ultimo elemento despues de delete
items = append(items[:2], items[3:]...)
items[len(items)-1] = "" // Limpiar el fantasma (si es un elemento del final)
// Nota: en Go 1.21+ slices.Delete ya limpia automaticamente
```

### Negative slice indices — Go no los tiene

```go
// Python: s[-1] = ultimo elemento
// Go: NO existe indexacion negativa
// s[-1] → error de compilacion (constant index is negative)

// var i int = -1
// s[i] → runtime panic: index out of range [-1]

// Para acceder al ultimo elemento:
last := s[len(s)-1]

// Para los ultimos N elementos:
lastN := s[len(s)-n:]
```

### Bounds checking y compilador

```go
// Go hace bounds checking en cada acceso a indice.
// El compilador puede eliminar checks cuando demuestra seguridad:

// ✗ Check en cada iteracion:
func sum(s []int) int {
    total := 0
    for i := 0; i < len(s); i++ {
        total += s[i] // Bounds check
    }
    return total
}

// ✓ Range elimina bounds checks:
func sum(s []int) int {
    total := 0
    for _, v := range s {
        total += v // Sin bounds check — el compilador sabe que es seguro
    }
    return total
}

// ✓ BCE (Bounds Check Elimination) — el compilador infiere:
func process(s []int) {
    if len(s) < 4 {
        return
    }
    // Despues de este check, el compilador sabe que s[0..3] es seguro
    _ = s[3] // BCE elimina el check
    _ = s[2] // BCE elimina el check
    _ = s[1] // BCE elimina el check
    _ = s[0] // BCE elimina el check
}

// Ver donde quedan bounds checks:
// go build -gcflags="-d=ssa/check_bce" ./...
```

---

## 10. Slice aliasing: detectar dependencias ocultas

### Que es aliasing

Dos slices estan "aliased" cuando comparten el mismo backing array. Cualquier modificacion a uno es visible en el otro. Esto es invisible en el tipo — no hay forma en tiempo de compilacion de saber si dos `[]int` comparten memoria.

```go
// ¿Estos dos slices estan aliased?
func process(a, b []int) {
    // No hay forma de saberlo sin inspeccion
    // Si el caller paso:
    //   process(s[0:3], s[2:5]) → SI, se solapan
    //   process(s[0:3], s[5:8]) → SI, mismo backing array (pero no se solapan)
    //   process(x, y) → NO, si x e y se crearon independientemente
}
```

### Detectar aliasing en runtime

```go
import "unsafe"

func slicesOverlap[T any](a, b []T) bool {
    if len(a) == 0 || len(b) == 0 {
        return false
    }
    
    aStart := uintptr(unsafe.Pointer(&a[0]))
    aEnd := aStart + uintptr(len(a))*unsafe.Sizeof(a[0])
    bStart := uintptr(unsafe.Pointer(&b[0]))
    bEnd := bStart + uintptr(len(b))*unsafe.Sizeof(b[0])
    
    return aStart < bEnd && bStart < aEnd
}

func slicesSameArray[T any](a, b []T) bool {
    if len(a) == 0 || len(b) == 0 {
        return false
    }
    // Dos slices vienen del mismo array si sus rangos
    // de cap se solapan
    aBase := uintptr(unsafe.Pointer(&a[0]))
    bBase := uintptr(unsafe.Pointer(&b[0]))
    aCap := aBase + uintptr(cap(a))*unsafe.Sizeof(a[0])
    bCap := bBase + uintptr(cap(b))*unsafe.Sizeof(b[0])
    
    return aBase < bCap && bBase < aCap
}

// Uso en debugging:
data := []int{1, 2, 3, 4, 5, 6, 7, 8}
a := data[0:3]
b := data[5:8]
c := data[2:6]

fmt.Println(slicesOverlap(a, b)) // false (no se solapan)
fmt.Println(slicesOverlap(a, c)) // true  (indice 2 compartido)
fmt.Println(slicesSameArray(a, b)) // true  (mismo backing array)
```

### Patron defensivo: documentar aliasing

```go
// En APIs que aceptan multiples slices, documentar si pueden
// estar aliased y que pasa si lo estan:

// Sort ordena s in-place. Si otros slices comparten el backing
// array de s, veran la reordenacion.
func Sort(s []int) { ... }

// Merge combina a y b en un nuevo slice.
// a y b NO deben estar aliased — comportamiento indefinido si lo estan.
func Merge(a, b []int) []int { ... }

// SafeMerge funciona correctamente incluso si a y b estan aliased.
func SafeMerge(a, b []int) []int {
    result := make([]int, 0, len(a)+len(b))
    result = append(result, a...)
    result = append(result, b...)
    sort.Ints(result)
    return result
}
```

---

## 11. Append y el "stale slice" problem

### El header viejo apunta a array abandonado

```go
func grow(s []int) []int {
    return append(s, 42)
}

original := make([]int, 3, 3) // cap = len = 3
original[0], original[1], original[2] = 1, 2, 3

// Guardar referencia al header viejo
stale := original

// Provocar realocacion
original = grow(original) // append realoca → nuevo backing array

// stale apunta al array viejo
stale[0] = 999
fmt.Println(original[0]) // 1 — no afectado
fmt.Println(stale[0])    // 999 — solo afecta el array viejo

// ¿El array viejo se libera? NO — stale lo mantiene vivo
// Si stale dura mucho tiempo, es un memory leak
```

### Stale slice en struct fields

```go
type Service struct {
    hosts []string
}

func (s *Service) UpdateHosts(newHosts []string) {
    s.hosts = newHosts
}

func (s *Service) CurrentHosts() []string {
    return s.hosts // Retorna referencia directa
}

// Escenario de bug:
svc := &Service{}
svc.UpdateHosts([]string{"a", "b", "c"})

cached := svc.CurrentHosts() // cached apunta al backing array de svc.hosts

svc.UpdateHosts([]string{"x", "y"}) // svc.hosts ahora apunta a nuevo array

fmt.Println(cached) // [a b c] — datos stale
// cached sigue vivo → el array viejo no se recolecta

// Esto puede causar:
// 1. Datos desactualizados (usar hosts viejos)
// 2. Memory leak (arrays viejos se acumulan)
```

---

## 12. strings y slicing: el caso especial

### strings.Builder y la trampa de string slicing

```go
// En Go, strings son inmutables y se almacenan como (ptr, len)
// Un substring comparte el backing array (igual que slices)

original := "esto es una cadena muy larga con mucho contenido innecesario"
sub := original[0:4] // "esto"

// sub comparte el backing array de original
// PERO: como strings son inmutables, no hay corrupcion posible
// El unico riesgo es memory leak: sub retiene toda la cadena

// ✓ strings.Clone (Go 1.20+)
sub = strings.Clone(original[0:4]) // Copia independiente

// Pre-Go 1.20:
sub = string([]byte(original[0:4])) // Fuerza copia
```

### string ↔ []byte: cero copias vs seguridad

```go
// Conversion string → []byte SIEMPRE copia (por inmutabilidad de strings)
s := "hello"
b := []byte(s) // Alocacion + copia
b[0] = 'H'     // OK — b es independiente
fmt.Println(s)  // "hello" — no afectado

// Conversion []byte → string SIEMPRE copia
b2 := []byte{72, 101}
s2 := string(b2)
b2[0] = 0
fmt.Println(s2) // "He" — no afectado

// Optimizaciones del compilador — NO copia cuando:
// 1. map lookup: m[string(b)] — no aloca
// 2. string comparison: string(b) == "hello" — no aloca
// 3. range: for _, r := range string(b) — no aloca
// 4. Concatenacion en contextos simples

// unsafe: conversion sin copia (peligroso)
import "unsafe"

func unsafeString(b []byte) string {
    return unsafe.String(&b[0], len(b))
}
// ✗ Si b se modifica despues, el string "cambia" — viola inmutabilidad
// Solo usar en hot paths con benchmarks que lo justifiquen
```

---

## 13. Patrones SysAdmin avanzados: proteccion contra gotchas

### Patron: Safe Slice API wrapper

```go
// Envolver operaciones de slice para prevenir los gotchas comunes
// en codigo de infraestructura donde la correccion es critica

type SafeList[T any] struct {
    items []T
}

func NewSafeList[T any](items ...T) *SafeList[T] {
    return &SafeList[T]{
        items: slices.Clone(items), // Copia defensiva
    }
}

func (sl *SafeList[T]) Add(items ...T) {
    sl.items = append(sl.items, items...)
}

// Retorna copia — el caller no puede corromper el estado interno
func (sl *SafeList[T]) All() []T {
    return slices.Clone(sl.items)
}

// Sub-range retorna copia independiente
func (sl *SafeList[T]) Range(from, to int) []T {
    if from < 0 {
        from = 0
    }
    if to > len(sl.items) {
        to = len(sl.items)
    }
    return slices.Clone(sl.items[from:to])
}

func (sl *SafeList[T]) Len() int {
    return len(sl.items)
}

// Filter retorna nueva SafeList — inmutable desde fuera
func (sl *SafeList[T]) Filter(pred func(T) bool) *SafeList[T] {
    var result []T
    for _, item := range sl.items {
        if pred(item) {
            result = append(result, item)
        }
    }
    return &SafeList[T]{items: result}
}
```

### Patron: Immutable configuration slices

```go
// En configuracion de servicios, los slices de config
// no deben ser modificables despues de inicializacion

type ServiceConfig struct {
    allowedIPs   []string
    blockedPorts []int
    dnsServers   []string
}

func NewServiceConfig(ips []string, ports []int, dns []string) *ServiceConfig {
    return &ServiceConfig{
        allowedIPs:   slices.Clone(ips),   // Copia
        blockedPorts: slices.Clone(ports), // Copia
        dnsServers:   slices.Clone(dns),   // Copia
    }
}

// Retornar copias — prevenir modificacion externa
func (c *ServiceConfig) AllowedIPs() []string {
    return slices.Clone(c.allowedIPs)
}

func (c *ServiceConfig) IsIPAllowed(ip string) bool {
    return slices.Contains(c.allowedIPs, ip)
}

// Si necesitas modificar, crea nueva config (inmutabilidad funcional)
func (c *ServiceConfig) WithAdditionalIP(ip string) *ServiceConfig {
    newIPs := append(slices.Clone(c.allowedIPs), ip)
    return &ServiceConfig{
        allowedIPs:   newIPs,
        blockedPorts: slices.Clone(c.blockedPorts),
        dnsServers:   slices.Clone(c.dnsServers),
    }
}
```

### Patron: Batch processor con proteccion de backing array

```go
// Procesar hosts en batches sin corrupcion entre batches
func DeployFleet(hosts []string, batchSize int) error {
    // Clonar para no afectar el slice del caller
    work := slices.Clone(hosts)
    
    for len(work) > 0 {
        n := batchSize
        if n > len(work) {
            n = len(work)
        }
        
        // Full slice expression: batch aislado
        batch := work[:n:n]
        work = work[n:]
        
        if err := deployBatch(batch); err != nil {
            return fmt.Errorf("batch deploy failed at %d remaining: %w",
                len(work), err)
        }
        
        log.Printf("Deployed %d hosts, %d remaining", n, len(work))
    }
    return nil
}
```

---

## 14. Comparacion: Go vs C vs Rust — manejo de aliasing y safety

### Go: aliasing implicito, deteccion en runtime

```go
// Go permite aliasing libremente
// No hay forma en compile time de saber si dos slices comparten memoria
// La responsabilidad recae en el programador

data := []int{1, 2, 3, 4, 5}
a := data[0:3]
b := data[2:5]
// a y b se solapan — Go no te avisa
a[2] = 999 // Modifica b[0] silenciosamente

// Herramientas:
// - go vet: NO detecta aliasing
// - race detector: detecta acceso concurrente, no aliasing single-thread
// - Disciplina del programador + code review
```

### C: aliasing total, restrict keyword

```c
// C tiene el mismo problema pero peor: no hay bounds checking
int data[5] = {1, 2, 3, 4, 5};
int *a = &data[0];
int *b = &data[2];
// a y b pueden solaparse — no hay proteccion

// C99 introdujo 'restrict' para prometer no-aliasing al compilador
void process(int *restrict a, int *restrict b, int n) {
    // El programador PROMETE que a y b no se solapan
    // El compilador puede optimizar mas agresivamente
    // Si mientes → comportamiento indefinido
    for (int i = 0; i < n; i++) {
        a[i] = b[i] * 2;
    }
}

// En la practica, aliasing en C causa:
// - Buffer overflows (CVEs)
// - Optimizaciones incorrectas por strict aliasing rules
// - memcpy vs memmove (memcpy asume no-overlap)
```

### Rust: borrow checker previene aliasing mutable

```rust
// Rust resuelve el problema en compile time con ownership + borrowing
let mut data = vec![1, 2, 3, 4, 5];

// ✗ ERROR: no puedes tener dos borrows mutables
let a = &mut data[0..3];
let b = &mut data[2..5];
// error[E0499]: cannot borrow `data` as mutable more than once
// error[E0502]: cannot borrow `data` as mutable because
//               it is also borrowed as immutable

// ✓ OK: un borrow mutable a la vez
{
    let a = &mut data[0..3];
    a[2] = 999;
} // a sale de scope
{
    let b = &mut data[2..5];
    // Seguro: a ya no existe
}

// ✓ OK: multiples borrows inmutables
let a = &data[0..3];
let b = &data[2..5];
// Solo lectura — no hay riesgo de corrupcion

// split_at_mut: particion segura sin overlap
let (left, right) = data.split_at_mut(3);
// left = &mut [1,2,3], right = &mut [4,5]
// Garantizado sin overlap por la API
```

### Tabla comparativa: seguridad de aliasing

```
┌──────────────────────┬──────────────────────┬──────────────────────┬──────────────────────┐
│ Aspecto              │ Go                   │ C                    │ Rust                 │
├──────────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤
│ Aliasing mutable     │ Permitido            │ Permitido            │ Prohibido (compile)  │
│ Deteccion            │ Ninguna auto         │ Ninguna              │ Borrow checker       │
│ Corrupcion por       │ Posible (silenciosa) │ Posible (UB)         │ Imposible            │
│ append/push          │                      │                      │                      │
│ Proteccion manual    │ Full slice expr,     │ restrict, memcpy     │ No necesaria         │
│                      │ Clone                │ vs memmove           │                      │
│ Sub-slice retiene    │ Si (GC eventual)     │ N/A (manual)         │ Lifetime prevents    │
│ backing array        │                      │                      │                      │
│ Concurrencia         │ Race detector        │ Nada (prayer-based)  │ Send+Sync traits     │
│                      │ (runtime)            │                      │ (compile time)       │
│ Costo de seguridad   │ Copias defensivas    │ Disciplina + UB      │ Zero cost (compile)  │
│ Herramientas         │ go vet, -race        │ valgrind, ASan       │ Compilador           │
│ Facilidad de uso     │ Alta                 │ Alta (pero peligrosa)│ Media (learning      │
│                      │                      │                      │ curve)               │
│ Gotcha principal     │ Append en sub-slice  │ Buffer overflow      │ Fighting borrow      │
│                      │ corrompe otros       │                      │ checker              │
└──────────────────────┴──────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 15. Programa completo: Slice Integrity Analyzer

Un analizador que demuestra y detecta todos los gotchas de slicing — comparticion de backing array, corrupcion por append, memory leaks, y aliasing. Util como herramienta de aprendizaje y para debugging en produccion.

```go
package main

import (
	"bytes"
	"fmt"
	"runtime"
	"slices"
	"strings"
	"unsafe"
)

// ─── Slice Inspector ─────────────────────────────────────────────

// SliceInfo captura el estado interno de un slice para analisis
type SliceInfo struct {
	Name    string
	DataPtr uintptr
	Len     int
	Cap     int
	EndPtr  uintptr // ptr + cap * sizeof(T)
}

func inspectIntSlice(name string, s []int) SliceInfo {
	info := SliceInfo{
		Name: name,
		Len:  len(s),
		Cap:  cap(s),
	}
	if len(s) > 0 {
		info.DataPtr = uintptr(unsafe.Pointer(&s[0]))
		info.EndPtr = info.DataPtr + uintptr(cap(s))*unsafe.Sizeof(s[0])
	}
	return info
}

func inspectByteSlice(name string, s []byte) SliceInfo {
	info := SliceInfo{
		Name: name,
		Len:  len(s),
		Cap:  cap(s),
	}
	if len(s) > 0 {
		info.DataPtr = uintptr(unsafe.Pointer(&s[0]))
		info.EndPtr = info.DataPtr + uintptr(cap(s))
	}
	return info
}

func (si SliceInfo) String() string {
	if si.DataPtr == 0 {
		return fmt.Sprintf("%-12s → nil  len=%-4d cap=%-4d", si.Name, si.Len, si.Cap)
	}
	return fmt.Sprintf("%-12s → ptr=0x%x  len=%-4d cap=%-4d  end=0x%x",
		si.Name, si.DataPtr, si.Len, si.Cap, si.EndPtr)
}

// SharesBackingArray determina si dos SliceInfo comparten backing array
func SharesBackingArray(a, b SliceInfo) bool {
	if a.DataPtr == 0 || b.DataPtr == 0 {
		return false
	}
	return a.DataPtr < b.EndPtr && b.DataPtr < a.EndPtr
}

// Overlaps determina si los elementos visibles se solapan
func Overlaps(a, b SliceInfo) bool {
	if a.DataPtr == 0 || b.DataPtr == 0 || a.Len == 0 || b.Len == 0 {
		return false
	}
	aVisibleEnd := a.DataPtr + uintptr(a.Len)*8 // sizeof(int)
	bVisibleEnd := b.DataPtr + uintptr(b.Len)*8
	return a.DataPtr < bVisibleEnd && b.DataPtr < aVisibleEnd
}

// ─── Demo: Reslicing Chain ──────────────────────────────────────

func demoResplicingChain() {
	fmt.Println("═══ DEMO 1: Cadena de Reslicing ═══")
	fmt.Println()

	data := []int{10, 20, 30, 40, 50, 60, 70, 80, 90, 100}

	level0 := data
	level1 := level0[2:7]      // [30,40,50,60,70]
	level2 := level1[1:4]      // [40,50,60]
	level3 := level2[0:2]      // [40,50]
	level3safe := level2[0:2:2] // [40,50] con cap limitado

	infos := []SliceInfo{
		inspectIntSlice("data", data),
		inspectIntSlice("level1", level1),
		inspectIntSlice("level2", level2),
		inspectIntSlice("level3", level3),
		inspectIntSlice("level3safe", level3safe),
	}

	fmt.Println("Estado inicial:")
	for _, info := range infos {
		fmt.Printf("  %s\n", info)
	}

	// Analisis de aliasing
	fmt.Println("\nAnalisis de aliasing:")
	for i := 0; i < len(infos); i++ {
		for j := i + 1; j < len(infos); j++ {
			shared := SharesBackingArray(infos[i], infos[j])
			overlap := Overlaps(infos[i], infos[j])
			if shared {
				status := "backing array compartido"
				if overlap {
					status += " + elementos SOLAPADOS"
				}
				fmt.Printf("  %s ↔ %s: %s\n", infos[i].Name, infos[j].Name, status)
			}
		}
	}

	// Demostrar corrupcion
	fmt.Println("\nAppend a level3 (sin proteccion):")
	fmt.Printf("  Antes: data[5]=%d, level1[3]=%d, level2[2]=%d\n",
		data[5], level1[3], level2[2])

	level3 = append(level3, 999)

	fmt.Printf("  Despues: data[5]=%d, level1[3]=%d, level2[2]=%d\n",
		data[5], level1[3], level2[2])
	fmt.Println("  → level3 append CORROMPIO data[5], level1[3] y level2[2]!")

	// Restaurar y demostrar proteccion
	data[5] = 60 // Restaurar

	fmt.Println("\nAppend a level3safe (con full slice expression):")
	fmt.Printf("  Antes: data[5]=%d\n", data[5])

	level3safe = append(level3safe, 888)

	fmt.Printf("  Despues: data[5]=%d\n", data[5])
	fmt.Println("  → data INTACTO! level3safe realoco su propio array")

	newInfo := inspectIntSlice("level3safe", level3safe)
	fmt.Printf("  level3safe nuevo: %s\n", newInfo)
	fmt.Printf("  ¿Comparte con data? %v\n",
		SharesBackingArray(inspectIntSlice("data", data), newInfo))

	fmt.Println()
}

// ─── Demo: Delete Corruption ────────────────────────────────────

func demoDeleteCorruption() {
	fmt.Println("═══ DEMO 2: Corrupcion por Delete ═══")
	fmt.Println()

	original := []string{"alpha", "bravo", "charlie", "delta", "echo"}
	backup := original[:] // "backup" apunta al mismo array

	fmt.Printf("  original: %v\n", original)
	fmt.Printf("  backup:   %v\n", backup)
	fmt.Printf("  Comparten backing array: %v\n",
		SharesBackingArray(
			inspectByteSlice("orig", []byte(strings.Join(original, ""))),
			inspectByteSlice("back", []byte(strings.Join(backup, ""))),
		))

	// Eliminar "charlie" (indice 2) de original
	original = append(original[:2], original[3:]...)

	fmt.Println("\nDespues de delete 'charlie' de original:")
	fmt.Printf("  original: %v (len=%d)\n", original, len(original))
	fmt.Printf("  backup:   %v (len=%d)\n", backup, len(backup))
	fmt.Println("  → backup[3] y backup[4] estan corruptos!")
	fmt.Println("    El shift-left de delete arrastro 'delta' y 'echo',")
	fmt.Println("    dejando un 'echo' fantasma en backup[4]")

	// Solucion con Clone
	fmt.Println("\n--- Con Clone (correcto) ---")
	original2 := []string{"alpha", "bravo", "charlie", "delta", "echo"}
	backup2 := slices.Clone(original2) // Copia independiente

	original2 = append(original2[:2], original2[3:]...)

	fmt.Printf("  original2: %v\n", original2)
	fmt.Printf("  backup2:   %v\n", backup2)
	fmt.Println("  → backup2 intacto!")

	fmt.Println()
}

// ─── Demo: Memory Leak ──────────────────────────────────────────

func demoMemoryLeak() {
	fmt.Println("═══ DEMO 3: Memory Leak por Sub-Slice ═══")
	fmt.Println()

	// Simular procesamiento de "archivos" grandes
	type LeakExample struct {
		name      string
		retained  int // Bytes realmente necesarios
		wasted    int // Bytes retenidos innecesariamente
	}

	var examples []LeakExample

	// Escenario 1: Sub-slice retiene array grande
	bigData := make([]byte, 10*1024*1024) // 10 MB
	copy(bigData, []byte("HEADER:important-data"))

	// ✗ Leak: header retiene 10 MB
	headerBad := bigData[:20]

	// ✓ Fix: copiar solo lo necesario
	headerGood := bytes.Clone(bigData[:20])

	examples = append(examples, LeakExample{
		name:     "Sub-slice de 10MB",
		retained: cap(headerBad),
		wasted:   cap(headerBad) - len(headerBad),
	})
	examples = append(examples, LeakExample{
		name:     "Clone de 20 bytes",
		retained: cap(headerGood),
		wasted:   0,
	})

	// Escenario 2: Filtrado que retiene array original
	servers := make([]string, 1000)
	for i := range servers {
		servers[i] = fmt.Sprintf("server-%04d", i)
	}

	// ✗ Leak: filtered comparte backing array de 1000 strings
	var filteredBad []string
	for _, s := range servers[:100] { // Solo mirar los primeros 100
		if strings.HasPrefix(s, "server-00") {
			filteredBad = append(filteredBad, s)
		}
	}
	// filteredBad tiene ~10 elementos pero el slice original de 1000
	// NO puede ser recolectado porque filteredBad viene de un sub-slice?
	// En realidad, para strings el peligro es menor porque strings
	// son valores (ptr, len) — el peligro mayor es con []byte y []struct

	// Medir memoria
	runtime.GC()
	var m runtime.MemStats
	runtime.ReadMemStats(&m)

	fmt.Printf("  Memoria heap despues de ejemplos:\n")
	fmt.Printf("    Alloc:       %d KB\n", m.Alloc/1024)
	fmt.Printf("    HeapObjects: %d\n", m.HeapObjects)
	fmt.Println()

	fmt.Println("  Comparacion de retencion:")
	for _, ex := range examples {
		wastedPct := 0.0
		if ex.retained > 0 {
			wastedPct = float64(ex.wasted) / float64(ex.retained) * 100
		}
		fmt.Printf("    %-25s retained=%10d B  wasted=%10d B  (%.1f%% desperdicio)\n",
			ex.name, ex.retained, ex.wasted, wastedPct)
	}

	// Mantener referencias vivas para que no se optimicen
	_ = headerBad
	_ = headerGood
	_ = filteredBad

	fmt.Println()
}

// ─── Demo: Append Non-Determinism ───────────────────────────────

func demoAppendNonDeterminism() {
	fmt.Println("═══ DEMO 4: Append No-Determinista ═══")
	fmt.Println()

	// Caso 1: Mismo codigo, resultado depende de capacidad
	fmt.Println("  Caso 1: append depende de cap")

	// Con capacidad extra → NO realoca → corrompe
	withCap := make([]int, 3, 10)
	withCap[0], withCap[1], withCap[2] = 1, 2, 3
	sub1 := withCap[:2]
	sub1 = append(sub1, 99)
	fmt.Printf("    cap=10: withCap=%v (corrompido)\n", withCap)

	// Sin capacidad extra → REALOCA → independiente
	noCap := []int{1, 2, 3} // cap=3
	sub2 := noCap[:2]
	sub2 = append(sub2, 99)
	fmt.Printf("    cap=3:  noCap=%v  (intacto)\n", noCap)

	fmt.Println("    → MISMO codigo, resultado DIFERENTE segun capacidad")

	// Caso 2: Multiples slices, append secuencial
	fmt.Println("\n  Caso 2: Multiples appends sobreescriben")

	base := make([]int, 2, 10)
	base[0], base[1] = 1, 2

	a := append(base, 10)
	b := append(base, 20) // ¡Sobreescribe lo que append(base, 10) escribio!
	c := append(base, 30) // ¡Sobreescribe de nuevo!

	fmt.Printf("    base=%v\n", base)
	fmt.Printf("    a=%v\n", a) // [1 2 30] — ¡NO [1 2 10]!
	fmt.Printf("    b=%v\n", b) // [1 2 30] — ¡NO [1 2 20]!
	fmt.Printf("    c=%v\n", c) // [1 2 30]

	fmt.Println("    → a, b, c TODOS apuntan al mismo backing array")
	fmt.Println("      El ultimo append (30) sobreescribio los anteriores")
	fmt.Println("      a y b muestran 30 porque leen base[2] que es 30")

	// Solucion
	fmt.Println("\n  Solucion: limitar cap antes de append")
	base2 := make([]int, 2, 10)
	base2[0], base2[1] = 1, 2

	a2 := append(base2[:2:2], 10) // cap=2 → realoca
	b2 := append(base2[:2:2], 20) // cap=2 → realoca
	c2 := append(base2[:2:2], 30) // cap=2 → realoca

	fmt.Printf("    a2=%v\n", a2) // [1 2 10]
	fmt.Printf("    b2=%v\n", b2) // [1 2 20]
	fmt.Printf("    c2=%v\n", c2) // [1 2 30]
	fmt.Println("    → Cada uno tiene su propio backing array")

	fmt.Println()
}

// ─── Demo: Concurrent Slice ─────────────────────────────────────

func demoConcurrentSlice() {
	fmt.Println("═══ DEMO 5: Pre-Alocacion por Indice (Safe Concurrent) ═══")
	fmt.Println()

	const n = 20

	// ✓ SEGURO: pre-alocar y cada goroutine escribe en su indice
	results := make([]string, n)
	done := make(chan struct{})

	for i := 0; i < n; i++ {
		go func(id int) {
			results[id] = fmt.Sprintf("worker-%02d-done", id)
			done <- struct{}{}
		}(i)
	}

	// Esperar todas
	for i := 0; i < n; i++ {
		<-done
	}

	fmt.Println("  Resultados (pre-alocados por indice):")
	// Verificar que todos estan presentes
	complete := 0
	for i, r := range results {
		if r != "" {
			complete++
		}
		if i < 5 {
			fmt.Printf("    [%2d] %s\n", i, r)
		}
	}
	fmt.Printf("    ... (%d total)\n", complete)
	fmt.Printf("  Todos completos: %v\n", complete == n)
	fmt.Println("  → Sin mutex, sin channel collector, sin race condition")
	fmt.Println("    Porque cada goroutine escribe en un indice unico")

	fmt.Println()
}

// ─── Demo: Bounds Check Elimination ─────────────────────────────

func demoBoundsCheck() {
	fmt.Println("═══ DEMO 6: Reslicing Mas Alla de len ═══")
	fmt.Println()

	s := make([]int, 3, 8)
	s[0], s[1], s[2] = 10, 20, 30

	fmt.Printf("  s = %v, len=%d, cap=%d\n", s, len(s), cap(s))
	fmt.Println()

	// s[5] → panic
	fmt.Println("  s[5] → panic: index out of range")

	// Pero s[0:6] es valido si cap >= 6
	extended := s[0:6]
	fmt.Printf("  s[0:6] = %v, len=%d, cap=%d\n", extended, len(extended), cap(extended))
	fmt.Println("  → Los elementos [3:6] son zero values del backing array")
	fmt.Println("    Esto 'revela' la capacidad oculta del slice")

	// Demostrar que comparten memoria
	extended[1] = 999
	fmt.Printf("\n  Despues de extended[1]=999:")
	fmt.Printf("\n    s = %v\n", s)
	fmt.Printf("    extended = %v\n", extended)
	fmt.Println("    → s[1] tambien cambio — mismo backing array")

	fmt.Println()
}

// ─── Main ───────────────────────────────────────────────────────

func main() {
	fmt.Println(strings.Repeat("═", 60))
	fmt.Println("  SLICE INTEGRITY ANALYZER")
	fmt.Println("  Demostracion de gotchas avanzados de slicing en Go")
	fmt.Println(strings.Repeat("═", 60))
	fmt.Println()

	demoResplicingChain()
	demoDeleteCorruption()
	demoMemoryLeak()
	demoAppendNonDeterminism()
	demoConcurrentSlice()
	demoBoundsCheck()

	// Resumen
	fmt.Println(strings.Repeat("═", 60))
	fmt.Println("  RESUMEN DE PROTECCIONES")
	fmt.Println(strings.Repeat("═", 60))
	fmt.Println()
	fmt.Println("  1. Sub-slice que recibira append → s[low:high:high]")
	fmt.Println("  2. Sub-slice para almacenar      → slices.Clone()")
	fmt.Println("  3. Slice de datos grandes         → bytes.Clone()")
	fmt.Println("  4. Multiples forks de un slice    → base[:n:n] cada vez")
	fmt.Println("  5. Slice a goroutine              → pasar copia o por indice")
	fmt.Println("  6. API publica que retorna slice  → retornar Clone")
	fmt.Println("  7. Delete de slice compartido     → limpiar fantasmas")
	fmt.Println("  8. Config inmutable               → Clone en constructor + getters")
}
```

---

## 16. Tabla de errores comunes (avanzados)

```
┌────┬──────────────────────────────────────────┬──────────────────────────────────────────┬─────────┐
│ #  │ Error                                    │ Solucion                                 │ Nivel   │
├────┼──────────────────────────────────────────┼──────────────────────────────────────────┼─────────┤
│  1 │ Cadena de reslicing hereda cap excesivo  │ Full slice expression en cada nivel       │ Logic   │
│  2 │ Multiples appends al mismo base slice    │ base[:n:n] para cada fork                │ Logic   │
│  3 │ Delete corrompe slices hermanos          │ Clone antes de delete, o limpiar          │ Logic   │
│    │                                          │ fantasmas despues                        │         │
│  4 │ Sub-slice de 10 bytes retiene 100 MB     │ bytes.Clone() o make+copy                │ Memory  │
│  5 │ Map values acumulan backing arrays       │ Clone antes de almacenar en map           │ Memory  │
│  6 │ Struct fields retienen arrays grandes    │ Copiar el campo relevante                 │ Memory  │
│  7 │ Append concurrente sin sincronizacion    │ Mutex, channel, o indice pre-alocado      │ Race    │
│  8 │ Goroutine captura slice que muta         │ slices.Clone() o pasar como parametro     │ Race    │
│  9 │ Reslicing mas alla de len revela basura  │ No extender slices con datos ocultos      │ Logic   │
│ 10 │ Stale slice header tras realocacion      │ No retener referencias al header viejo    │ Memory  │
│ 11 │ Filter con items[:0] corrompe input      │ Usar var result []T (nuevo backing array) │ Logic   │
│ 12 │ Funcion retorna sub-slice sin proteger   │ Retornar Clone o usar s[:n:n]             │ API     │
│ 13 │ No limpiar punteros tras delete/shrink   │ Zerear elementos removidos para GC        │ Memory  │
│ 14 │ string substring retiene string grande   │ strings.Clone() (Go 1.20+)               │ Memory  │
└────┴──────────────────────────────────────────┴──────────────────────────────────────────┴─────────┘
```

---

## 17. Ejercicios de prediccion

**Ejercicio 1**: ¿Que imprime?

```go
data := []int{1, 2, 3, 4, 5, 6, 7, 8}
a := data[2:5]
b := a[1:3]
b[0] = 999
fmt.Println(data[3])
```

<details>
<summary>Respuesta</summary>

```
999
```

`a = data[2:5]` → `a` empieza en `data[2]`. `b = a[1:3]` → `b` empieza en `a[1]` = `data[3]`. `b[0] = 999` modifica `data[3]`. Todos comparten el mismo backing array.
</details>

**Ejercicio 2**: ¿Que imprime?

```go
base := make([]int, 2, 5)
base[0], base[1] = 1, 2
x := append(base, 10)
y := append(base, 20)
fmt.Println(x[2], y[2])
```

<details>
<summary>Respuesta</summary>

```
20 20
```

`base` tiene cap=5, asi que ambos `append` NO realocaron. Ambos escriben en `base[2]`. `y` escribio despues, asi que `base[2] = 20`. `x` y `y` comparten el backing array, asi que ambos leen 20.
</details>

**Ejercicio 3**: ¿Que imprime?

```go
s := []int{0, 1, 2, 3, 4}
t := s[1:3:3]
t = append(t, 99)
fmt.Println(s)
fmt.Println(len(t), cap(t))
```

<details>
<summary>Respuesta</summary>

```
[0 1 2 3 4]
3 4
```

`s[1:3:3]` limita cap a 2 (3-1). `append` necesita mas espacio → realoca un nuevo backing array. `s` queda intacto. `t` ahora tiene len=3 en su propio array, con cap probablemente 4 (duplicacion de 2).
</details>

**Ejercicio 4**: ¿Que imprime?

```go
original := []string{"a", "b", "c", "d", "e"}
snapshot := original[:]
original = append(original[:2], original[3:]...)
fmt.Println(snapshot)
```

<details>
<summary>Respuesta</summary>

```
[a b d e e]
```

`snapshot` comparte el backing array. El delete (append de shift-left) modifica el backing array in-place: copia "d" y "e" sobre las posiciones 2 y 3. La posicion 4 queda con "e" (viejo valor). `snapshot` tiene len=5 y ve todo el array: `[a, b, d, e, e]`.
</details>

**Ejercicio 5**: ¿Que imprime?

```go
s := make([]int, 3, 3)
s[0], s[1], s[2] = 1, 2, 3
old := s
s = append(s, 4)
s[0] = 999
fmt.Println(old[0])
```

<details>
<summary>Respuesta</summary>

```
1
```

`s` tiene cap=3. `append(s, 4)` realoca un nuevo backing array (cap agotado). `s[0] = 999` modifica el nuevo array. `old` apunta al array viejo donde `old[0]` sigue siendo 1.
</details>

**Ejercicio 6**: ¿Que imprime?

```go
s := make([]int, 5, 10)
for i := range s {
    s[i] = i * 10
}
t := s[2:4]
fmt.Println(len(t), cap(t))
t = append(t, 999, 888, 777)
fmt.Println(s[4])
```

<details>
<summary>Respuesta</summary>

```
2 8
999
```

`t = s[2:4]` tiene len=2, cap=10-2=8. `append(t, 999, 888, 777)` no realoca (cap=8 tiene espacio). Escribe 999 en posicion `s[4]`, 888 en `s[5]`, 777 en `s[6]` del backing array. `s[4]` era 40, ahora es 999.
</details>

**Ejercicio 7**: ¿Que imprime?

```go
a := []int{1, 2, 3, 4, 5}
b := a[1:3]
c := b[0:4]
fmt.Println(c)
```

<details>
<summary>Respuesta</summary>

```
[2 3 4 5]
```

`b = a[1:3]` → `[2, 3]`, len=2, cap=4. `c = b[0:4]` es valido porque cap(b)=4. `c` revela los elementos ocultos del backing array: `[2, 3, 4, 5]`. Esto extiende `b` mas alla de su len pero dentro de su cap.
</details>

**Ejercicio 8**: ¿Que imprime?

```go
data := []byte("Hello, World! This is a very long string for testing")
header := data[:5]
data = nil
runtime.GC()
fmt.Println(string(header))
```

<details>
<summary>Respuesta</summary>

```
Hello
```

Aunque `data` se puso a nil y se forzo GC, `header` sigue apuntando al backing array original. El GC no puede recolectar el array de ~50 bytes porque `header` lo referencia. Esto es un memory leak en miniatura.
</details>

**Ejercicio 9**: ¿Que imprime?

```go
results := make([]int, 5)
for i := range results {
    results[i] = i
}
filtered := results[:0]
for _, v := range results {
    if v%2 == 0 {
        filtered = append(filtered, v)
    }
}
fmt.Println(results)
fmt.Println(filtered)
```

<details>
<summary>Respuesta</summary>

```
[0 2 4 3 4]
[0 2 4]
```

`filtered := results[:0]` tiene len=0 pero cap=5, compartiendo el backing array. `append` escribe directamente sobre `results`: posicion 0→0, posicion 1→2, posicion 2→4. `results` queda `[0, 2, 4, 3, 4]` — las posiciones 0-2 fueron sobreescritas. Los valores originales 3 y 4 en posiciones 3-4 no fueron tocados.
</details>

**Ejercicio 10**: ¿Que imprime?

```go
base := make([]int, 0, 5)
base = append(base, 1, 2, 3)

fork1 := append(base, 10, 20)
fork2 := append(base, 30, 40)

fmt.Println(fork1)
fmt.Println(fork2)
fmt.Println(fork1[3] == fork2[3])
```

<details>
<summary>Respuesta</summary>

```
[1 2 3 30 40]
[1 2 3 30 40]
true
```

`base` tiene len=3, cap=5. `fork1 = append(base, 10, 20)` escribe 10,20 en posiciones 3,4 del backing array. `fork2 = append(base, 30, 40)` escribe 30,40 en las MISMAS posiciones 3,4 (porque `base` sigue siendo len=3). `fork1` y `fork2` apuntan al mismo backing array, asi que ambos ven los valores escritos por `fork2` (el ultimo). `fork1[3] == fork2[3]` es `30 == 30` → `true`.
</details>

---

## 18. Resumen visual

```
┌──────────────────────────────────────────────────────────────┐
│              SLICING Y GOTCHAS — RESUMEN                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  RESLICING CHAIN                                             │
│  a → b := a[i:j] → c := b[x:y] → d := c[m:n]               │
│  TODOS comparten UN backing array                            │
│  cap se hereda: cap(b) = cap(a) - i                          │
│  Modificar c[0] modifica a, b, y d si se solapan             │
│                                                              │
│  APPEND NO-DETERMINISTA                                      │
│  ├─ Si cap tiene espacio → escribe en backing array (corr.)  │
│  ├─ Si cap agotado → realoca → independiente                 │
│  └─ Mismo codigo, resultado diferente segun cap              │
│                                                              │
│  CORRUPCION MULTI-FORK                                       │
│  fork1 := append(base, 10)                                   │
│  fork2 := append(base, 20)  ← sobreescribe fork1!           │
│  Solucion: base[:n:n] fuerza realocacion                     │
│                                                              │
│  MEMORY LEAKS                                                │
│  ├─ Sub-slice retiene backing array completo                 │
│  ├─ Map con sub-slices acumula backing arrays                │
│  ├─ Structs con slice fields retienen arrays                 │
│  ├─ Diagnostico: pprof heap, runtime.MemStats                │
│  └─ Fix: bytes.Clone, slices.Clone, make+copy                │
│                                                              │
│  CONCURRENCIA                                                │
│  ├─ append NO es thread-safe → mutex o channel               │
│  ├─ Slice header no es atomico → lectura inconsistente       │
│  ├─ Pre-alocar por indice → seguro sin sync                  │
│  └─ go vet -race detecta, pero no previene                   │
│                                                              │
│  PROTECCIONES                                                │
│  ┌─────────────────────────────┬─────────────────────────┐   │
│  │ Situacion                   │ Proteccion              │   │
│  ├─────────────────────────────┼─────────────────────────┤   │
│  │ Sub-slice + append          │ s[low:high:high]        │   │
│  │ Almacenar sub-slice         │ slices.Clone()          │   │
│  │ Retornar de API publica     │ Clone en getter         │   │
│  │ Fork multiple de base       │ base[:n:n] cada vez     │   │
│  │ Pasar a goroutine           │ Clone o parametro       │   │
│  │ Delete de slice compartido  │ Limpiar fantasmas       │   │
│  │ Config inmutable            │ Clone en constructor    │   │
│  └─────────────────────────────┴─────────────────────────┘   │
│                                                              │
│  COMPARACION                                                 │
│  Go:   aliasing implicito, responsabilidad del programador   │
│  C:    aliasing + sin bounds check → UB                      │
│  Rust: borrow checker previene aliasing mutable en compile   │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: T04 - Patrones con slices — filtrado in-place, stack con append/slice, sort.Slice, slices package (Go 1.21+)
