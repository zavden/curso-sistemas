# Benchmarks — testing.B, go test -bench, b.ResetTimer, b.ReportAllocs, pprof

## 1. Introduccion

Los benchmarks en Go estan integrados en la misma toolchain que los tests — mismo paquete `testing`, mismos archivos `*_test.go`, mismo comando `go test`. No necesitas librerias externas (como Criterion en Rust o Google Benchmark en C++). El sistema de benchmarks de Go mide tiempo de ejecucion, allocations de memoria, y se integra con `pprof` para profiling detallado.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                    BENCHMARKS EN GO — VISION GENERAL                            │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  HERRAMIENTAS                                                                   │
│  ────────────                                                                   │
│                                                                                  │
│  testing.B          → ejecutar y medir benchmarks                               │
│  go test -bench     → correr benchmarks                                         │
│  go test -benchmem  → incluir stats de memoria                                  │
│  go test -cpuprofile → generar profile de CPU                                   │
│  go test -memprofile → generar profile de memoria                               │
│  go tool pprof       → analizar profiles interactivamente                       │
│  benchstat           → comparar resultados entre runs                           │
│                                                                                  │
│  FLUJO TIPICO                                                                   │
│  ────────────                                                                   │
│                                                                                  │
│  1. Escribir benchmark (func BenchmarkXxx(b *testing.B))                       │
│  2. Correr: go test -bench=. -benchmem                                         │
│  3. Guardar resultado: go test -bench=. > old.txt                              │
│  4. Hacer cambio (optimizacion)                                                │
│  5. Correr de nuevo: go test -bench=. > new.txt                                │
│  6. Comparar: benchstat old.txt new.txt                                        │
│  7. Si no es claro, profiling: go test -bench=. -cpuprofile cpu.prof           │
│  8. Analizar: go tool pprof cpu.prof                                           │
│                                                                                  │
│  ANATOMIA DE UN BENCHMARK                                                       │
│  ────────────────────────                                                       │
│                                                                                  │
│  func BenchmarkFoo(b *testing.B) {                                             │
│      // setup (no se mide)                                                      │
│      data := prepareData()                                                      │
│      b.ResetTimer()  ← reiniciar timer despues del setup                       │
│                                                                                  │
│      for b.Loop() {  ← Go 1.24+ (o: for i := 0; i < b.N; i++)                │
│          result = Foo(data)  ← codigo medido                                   │
│      }                                                                          │
│  }                                                                              │
│                                                                                  │
│  OUTPUT                                                                         │
│  ──────                                                                         │
│  BenchmarkFoo-8   5000000   234 ns/op   48 B/op   2 allocs/op                  │
│  ─────────────    ───────   ─────────   ───────   ───────────                   │
│  nombre+GOMAXP    iters     tiempo      memoria   allocaciones                  │
│                                                                                  │
│  El runtime ajusta b.N automaticamente hasta que el resultado                   │
│  es estadisticamente estable (minimo 1 segundo por defecto)                    │
└──────────────────────────────────────────────────────────────────────────────────┘
```

**Por que benchmarkear?**
- **Medir antes de optimizar**: "premature optimization is the root of all evil" — pero necesitas datos para saber QUE optimizar
- **Detectar regresiones**: un benchmark que se integra en CI detecta cuando un cambio hace el codigo mas lento
- **Comparar implementaciones**: "es mas rapido usar `strings.Builder` o `fmt.Sprintf`?" — el benchmark te dice
- **Entender el costo**: "cuanto cuesta una allocation? un JSON marshal? un mutex lock?" — el benchmark cuantifica

---

## 2. testing.B — el objeto central

### 2.1 El benchmark minimo

```go
package main

import (
    "testing"
)

func Sum(numbers []int) int {
    total := 0
    for _, n := range numbers {
        total += n
    }
    return total
}

// Benchmark: DEBE empezar con "Benchmark" y recibir *testing.B
func BenchmarkSum(b *testing.B) {
    numbers := []int{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}

    for b.Loop() { // Go 1.24+
        Sum(numbers)
    }
}

// Equivalente pre-Go 1.24:
func BenchmarkSumClassic(b *testing.B) {
    numbers := []int{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}

    for i := 0; i < b.N; i++ {
        Sum(numbers)
    }
}
```

```bash
$ go test -bench=BenchmarkSum -benchmem

BenchmarkSum-8          500000000     2.34 ns/op    0 B/op    0 allocs/op
BenchmarkSumClassic-8   500000000     2.35 ns/op    0 B/op    0 allocs/op
```

### 2.2 b.N y b.Loop(): como funciona la iteracion

```go
// El runtime de Go ejecuta tu benchmark MULTIPLES veces con diferentes b.N:
// 1. Primera ejecucion: b.N = 1
// 2. Si termina en < 1s: incrementa b.N (100, 10000, 1000000, ...)
// 3. Repite hasta que el benchmark tome >= 1 segundo
// 4. Reporta: tiempo_total / b.N = tiempo por operacion

// b.Loop() (Go 1.24+) es preferido porque:
// 1. El compilador no puede optimizar eliminando la llamada
//    (con b.N el compilador puede detectar que el resultado no se usa)
// 2. Mas limpio sintacticamente
// 3. Internamente maneja b.N por ti

// IMPORTANTE: el codigo DENTRO del loop debe ser lo que quieres medir
// El codigo FUERA del loop es setup/teardown

func BenchmarkCorrect(b *testing.B) {
    // Setup (NO medido)
    data := make([]int, 1000)
    for i := range data {
        data[i] = i
    }
    b.ResetTimer() // reiniciar timer despues del setup

    // Medido
    for b.Loop() {
        Sum(data)
    }
}
```

### 2.3 Prevenir eliminacion por el compilador

```go
// El compilador de Go puede eliminar llamadas cuyos resultados no se usan
// Esto hace que el benchmark mida "nada" y reporte tiempos irrealistamente bajos

// MAL: el compilador puede eliminar Fibonacci(30) porque result no se usa
func BenchmarkBad(b *testing.B) {
    for b.Loop() {
        Fibonacci(30) // el compilador puede optimizar esto fuera
    }
}

// BIEN (pre-Go 1.24): usar variable de paquete para "escapar" el resultado
var benchResult int

func BenchmarkGood(b *testing.B) {
    var r int
    for i := 0; i < b.N; i++ {
        r = Fibonacci(30)
    }
    benchResult = r // asignar a variable de paquete previene eliminacion
}

// BIEN (Go 1.24+): b.Loop() previene la eliminacion automaticamente
func BenchmarkBest(b *testing.B) {
    for b.Loop() {
        Fibonacci(30) // b.Loop() usa runtime.KeepAlive internamente
    }
}

// NOTA sobre b.Loop():
// Go 1.24 introdujo b.Loop() que reemplaza el patron for i := 0; i < b.N; i++
// Ventajas: previene eliminacion por el compilador, mas ergonomico
// Si usas Go < 1.24, usa el patron con variable de paquete
```

---

## 3. Metodos de testing.B

### 3.1 b.ResetTimer() — excluir setup del timing

```go
func BenchmarkWithSetup(b *testing.B) {
    // Setup costoso: crear 1M de elementos
    data := make([]string, 1_000_000)
    for i := range data {
        data[i] = fmt.Sprintf("item-%d", i)
    }

    b.ResetTimer() // ← CRUCIAL: el setup no se cuenta en el tiempo

    for b.Loop() {
        Sort(data) // solo esto se mide
    }
}

// Sin b.ResetTimer(), el setup se incluye en el tiempo
// y el benchmark reporta un numero inflado y variable
```

### 3.2 b.StopTimer() y b.StartTimer() — pausar el timer

```go
func BenchmarkWithPerIterationSetup(b *testing.B) {
    for b.Loop() {
        b.StopTimer() // pausar
        data := generateRandomData(1000) // setup por iteracion (no medido)
        b.StartTimer() // reanudar

        Sort(data) // solo esto se mide
    }
}

// CUIDADO: StopTimer/StartTimer tienen overhead
// Si tu setup es rapido comparado con la operacion, el overhead
// de Stop/Start puede distorsionar los resultados
// En ese caso, es mejor hacer el setup fuera del loop y usar b.ResetTimer()
```

### 3.3 b.ReportAllocs() — reportar allocations

```go
func BenchmarkStringConcat(b *testing.B) {
    b.ReportAllocs() // incluir stats de memoria en el output

    for b.Loop() {
        s := ""
        for i := 0; i < 100; i++ {
            s += fmt.Sprintf("item-%d,", i) // muchas allocations
        }
    }
}

// Output:
// BenchmarkStringConcat-8   10000   150234 ns/op   53792 B/op   301 allocs/op
//                                                  ─────────    ───────────
//                                                  bytes/op     allocs/op

// Alternativa: usar -benchmem flag (aplica a TODOS los benchmarks)
// go test -bench=. -benchmem
```

### 3.4 b.ReportMetric() — metricas custom

```go
func BenchmarkThroughput(b *testing.B) {
    data := make([]byte, 1024*1024) // 1MB de datos
    rand.Read(data)

    b.ResetTimer()
    b.SetBytes(int64(len(data))) // reportar throughput en MB/s

    for b.Loop() {
        Compress(data)
    }
}

// Output:
// BenchmarkThroughput-8   100   10234567 ns/op   102.45 MB/s

// b.SetBytes() hace que Go calcule y reporte MB/s automaticamente
// Ideal para benchmarks de I/O, compresion, hashing, serialization

// Metricas completamente custom:
func BenchmarkCustomMetric(b *testing.B) {
    var totalItems int

    for b.Loop() {
        items := ProcessBatch()
        totalItems += len(items)
    }

    b.ReportMetric(float64(totalItems)/float64(b.N), "items/op")
}

// Output:
// BenchmarkCustomMetric-8   1000   1234567 ns/op   42.00 items/op
```

### 3.5 b.RunParallel() — benchmark paralelo

```go
// Medir throughput con multiples goroutines (simula carga concurrente)
func BenchmarkConcurrentMap(b *testing.B) {
    m := sync.Map{}

    // Pre-cargar datos
    for i := 0; i < 1000; i++ {
        m.Store(i, fmt.Sprintf("value-%d", i))
    }

    b.ResetTimer()
    b.RunParallel(func(pb *testing.PB) {
        // Cada goroutine ejecuta este closure
        i := 0
        for pb.Next() { // equivalente a b.Loop() pero para parallel
            key := i % 1000
            m.Load(key)
            i++
        }
    })
}

// Output:
// BenchmarkConcurrentMap-8   20000000   65.3 ns/op
// El "-8" indica GOMAXPROCS=8 goroutines

// Comparar con acceso secuencial:
func BenchmarkSequentialMap(b *testing.B) {
    m := make(map[int]string)
    for i := 0; i < 1000; i++ {
        m[i] = fmt.Sprintf("value-%d", i)
    }

    b.ResetTimer()
    for b.Loop() {
        _ = m[42]
    }
}

// El benchmark paralelo muestra el throughput TOTAL del sistema
// no el tiempo por operacion individual
// Util para comparar sync.Map vs RWMutex+map bajo contention
```

### 3.6 b.Run() — sub-benchmarks

```go
func BenchmarkSort(b *testing.B) {
    sizes := []int{10, 100, 1000, 10000, 100000}

    for _, size := range sizes {
        b.Run(fmt.Sprintf("size=%d", size), func(b *testing.B) {
            // Setup para este tamano
            data := make([]int, size)
            for i := range data {
                data[i] = rand.Intn(size * 10)
            }

            b.ResetTimer()
            for b.Loop() {
                // Copiar para no sortear datos ya sorteados
                tmp := make([]int, len(data))
                copy(tmp, data)
                slices.Sort(tmp)
            }
        })
    }
}

// Output:
// BenchmarkSort/size=10-8        5000000    234 ns/op
// BenchmarkSort/size=100-8        500000   3456 ns/op
// BenchmarkSort/size=1000-8        30000  45678 ns/op
// BenchmarkSort/size=10000-8        2000 567890 ns/op
// BenchmarkSort/size=100000-8        200 7890123 ns/op

// Ejecutar un sub-benchmark especifico:
// go test -bench=BenchmarkSort/size=1000
```

---

## 4. go test -bench — el comando

### 4.1 Flags basicas

```bash
# Ejecutar TODOS los benchmarks
go test -bench=.

# Ejecutar benchmarks que matchean un regex
go test -bench=BenchmarkSort
go test -bench="BenchmarkSort/size=100$"
go test -bench="Benchmark(Sort|Search)"

# Con memory stats
go test -bench=. -benchmem

# Controlar duracion (default: 1s por benchmark)
go test -bench=. -benchtime=5s        # 5 segundos por benchmark
go test -bench=. -benchtime=1000x     # exactamente 1000 iteraciones
go test -bench=. -benchtime=1x        # 1 iteracion (para profiling)

# Controlar repeticiones (para estabilidad estadistica)
go test -bench=. -count=5             # repetir 5 veces cada benchmark

# NO ejecutar tests regulares (solo benchmarks)
go test -bench=. -run=^$              # regex que no matchea ningun test

# Combinar todo
go test -bench=. -benchmem -count=5 -run=^$ -timeout=10m
```

### 4.2 Interpretando el output

```
BenchmarkConcat/builder-8       10000000       112.3 ns/op      256 B/op     4 allocs/op
─────────────────────────       ────────       ──────────       ────────     ────────────
│                                │              │               │            │
│                                │              │               │            └── allocations
│                                │              │               └── bytes por operacion
│                                │              └── nanosegundos por operacion
│                                └── numero de iteraciones ejecutadas
└── nombre-GOMAXPROCS

# Unidades de tiempo:
# ns/op = nanosegundos (10⁻⁹ s) — operaciones rapidas
# µs/op = microsegundos (10⁻⁶ s) — operaciones moderadas
# ms/op = milisegundos (10⁻³ s)  — operaciones lentas
# s/op  = segundos               — operaciones muy lentas

# Memory:
# B/op     = bytes allocados por operacion (en heap)
# allocs/op = numero de allocaciones por operacion
# 0 B/op 0 allocs/op = zero allocation (ideal para hot paths)
```

### 4.3 Profiling con go test

```bash
# Generar CPU profile
go test -bench=. -cpuprofile=cpu.prof

# Generar memory profile
go test -bench=. -memprofile=mem.prof

# Generar block profile (contention de locks)
go test -bench=. -blockprofile=block.prof

# Generar mutex profile (contention de mutexes)
go test -bench=. -mutexprofile=mutex.prof

# Generar trace (ejecucion completa)
go test -bench=. -trace=trace.out

# Generar multiples profiles a la vez
go test -bench=. -cpuprofile=cpu.prof -memprofile=mem.prof

# El test binary se guarda automaticamente para pprof
# Los archivos .prof se analizan con go tool pprof
```

---

## 5. pprof — analisis de profiles

### 5.1 CPU profiling

```bash
# Generar profile
go test -bench=BenchmarkProcess -cpuprofile=cpu.prof -run=^$

# Abrir en modo interactivo
go tool pprof cpu.prof

# Comandos dentro de pprof:
# top        → funciones que mas CPU consumen
# top 20     → top 20
# list Foo   → codigo fuente anotado de la funcion Foo
# web        → abrir grafo en navegador (requiere graphviz)
# png        → generar imagen PNG del grafo

# Ejemplo de output de 'top':
# Showing top 10 nodes out of 45
#       flat  flat%   sum%        cum   cum%
#     620ms 31.00% 31.00%      620ms 31.00%  runtime.memmove
#     340ms 17.00% 48.00%      340ms 17.00%  encoding/json.(*encodeState).string
#     200ms 10.00% 58.00%     1200ms 60.00%  mypackage.Process
#
# flat  = tiempo en ESA funcion (excluyendo llamadas)
# cum   = tiempo incluyendo funciones llamadas
# sum%  = acumulado de flat%
```

```bash
# Modo web (preferido — interfaz grafica)
go tool pprof -http=:8080 cpu.prof
# Abre navegador con:
# - Flame graph (vista jerarquica de llamadas)
# - Top functions (tabla ordenable)
# - Graph (grafo de llamadas)
# - Source (codigo anotado)
# - Disassembly (asm anotado)
```

### 5.2 Memory profiling

```bash
# Generar memory profile
go test -bench=BenchmarkProcess -memprofile=mem.prof -run=^$

# Analizar allocations
go tool pprof mem.prof

# Dentro de pprof:
# top           → funciones que mas memoria alloquean
# top -cum      → ordenar por memoria acumulada
# list Process  → ver donde ocurren allocations en Process()

# Tipos de memory profile:
# -alloc_objects → numero de objetos allocados (default)
# -alloc_space   → bytes allocados
# -inuse_objects → objetos actualmente en uso (para leak detection)
# -inuse_space   → bytes actualmente en uso

go tool pprof -alloc_space mem.prof   # ordenar por bytes
go tool pprof -inuse_space mem.prof   # memoria actualmente en uso
```

### 5.3 Ejemplo completo de profiling workflow

```go
// Archivo: process.go
package process

import (
    "encoding/json"
    "strings"
)

type Record struct {
    ID    int      `json:"id"`
    Name  string   `json:"name"`
    Tags  []string `json:"tags"`
    Score float64  `json:"score"`
}

// Version original (lenta)
func ProcessRecords(records []Record) []string {
    var result []string
    for _, r := range records {
        // Concatenacion con + (nueva string por iteracion)
        s := r.Name + ":"
        for _, tag := range r.Tags {
            s += tag + ","
        }
        // JSON encoding
        data, _ := json.Marshal(r)
        s += string(data)
        result = append(result, s)
    }
    return result
}
```

```go
// Archivo: process_test.go
package process

import (
    "fmt"
    "testing"
)

func generateRecords(n int) []Record {
    records := make([]Record, n)
    for i := range records {
        records[i] = Record{
            ID:    i,
            Name:  fmt.Sprintf("record-%d", i),
            Tags:  []string{"tag1", "tag2", "tag3"},
            Score: float64(i) * 1.5,
        }
    }
    return records
}

func BenchmarkProcessRecords(b *testing.B) {
    records := generateRecords(1000)
    b.ResetTimer()
    b.ReportAllocs()

    for b.Loop() {
        ProcessRecords(records)
    }
}
```

```bash
# 1. Baseline
$ go test -bench=BenchmarkProcessRecords -benchmem -count=3 | tee old.txt
BenchmarkProcessRecords-8    200    5234567 ns/op    4521344 B/op    15023 allocs/op

# 2. Profiling — donde esta el bottleneck?
$ go test -bench=BenchmarkProcessRecords -cpuprofile=cpu.prof -memprofile=mem.prof -run=^$
$ go tool pprof -http=:8080 cpu.prof
# → Descubrimos: 40% en json.Marshal, 30% en string concatenation

# 3. Optimizar (ver version optimizada abajo)

# 4. Nuevo benchmark
$ go test -bench=BenchmarkProcessRecords -benchmem -count=3 | tee new.txt

# 5. Comparar con benchstat
$ go install golang.org/x/perf/cmd/benchstat@latest
$ benchstat old.txt new.txt
```

```go
// Version optimizada
func ProcessRecordsOptimized(records []Record) []string {
    result := make([]string, 0, len(records)) // pre-allocar capacity
    var buf strings.Builder

    for _, r := range records {
        buf.Reset()
        buf.WriteString(r.Name)
        buf.WriteByte(':')
        for _, tag := range r.Tags {
            buf.WriteString(tag)
            buf.WriteByte(',')
        }
        // Encoder reusable
        enc := json.NewEncoder(&buf)
        enc.Encode(r)
        result = append(result, buf.String())
    }
    return result
}
```

---

## 6. benchstat — comparacion estadistica

### 6.1 Instalacion y uso basico

```bash
# Instalar
go install golang.org/x/perf/cmd/benchstat@latest

# Generar datos con count > 1 para significancia estadistica
go test -bench=. -benchmem -count=10 > old.txt

# Hacer cambios al codigo...

go test -bench=. -benchmem -count=10 > new.txt

# Comparar
benchstat old.txt new.txt
```

### 6.2 Interpretando benchstat

```
                          │   old.txt   │              new.txt               │
                          │   sec/op    │   sec/op     vs base               │
ProcessRecords-8            5.234m ± 3%   2.156m ± 2%  -58.81% (p=0.000 n=10)

                          │   old.txt    │              new.txt                │
                          │    B/op      │    B/op      vs base                │
ProcessRecords-8            4.521Mi ± 0%   1.234Mi ± 0%  -72.70% (p=0.000 n=10)

                          │   old.txt   │              new.txt               │
                          │  allocs/op  │  allocs/op   vs base               │
ProcessRecords-8            15.02k ± 0%    3.01k ± 0%  -79.96% (p=0.000 n=10)

# ± 3%     = variacion entre runs (menor es mejor, >5% indica inestabilidad)
# p=0.000  = p-value (< 0.05 significa diferencia estadisticamente significativa)
# n=10     = numero de samples
# -58.81%  = mejora de 58.81% en tiempo
# Mi       = Mebibytes (1 MiB = 1024*1024 bytes)
# k        = miles

# Si ves "~ (p=0.345)" en vez de un porcentaje:
# Significa que la diferencia NO es estadisticamente significativa
# Necesitas mas runs (count) o la diferencia es demasiado pequena
```

### 6.3 Cuantos runs necesitas?

```bash
# Minimo recomendado: -count=5
# Para diferencias pequenas (<5%): -count=10 o mas
# Para benchmarks estables: -count=6 es suficiente

# Si benchstat muestra alta variacion (± >5%):
# 1. Cerrar otros programas
# 2. Fijar CPU frequency: sudo cpupower frequency-set -g performance
# 3. Usar -benchtime=3s (mas tiempo por benchmark)
# 4. Usar taskset para fijar core: taskset -c 0 go test -bench=.

# Formato para benchstat:
# NECESITAS el output completo de go test, no solo las lineas de benchmark
# go test -bench=. -benchmem -count=10 > results.txt
```

---

## 7. Patrones comunes de benchmarks

### 7.1 Comparar implementaciones

```go
func BenchmarkStringConcat(b *testing.B) {
    words := []string{"hello", "world", "foo", "bar", "baz"}

    b.Run("plus", func(b *testing.B) {
        for b.Loop() {
            s := ""
            for _, w := range words {
                s += w + " "
            }
            _ = s
        }
    })

    b.Run("Sprintf", func(b *testing.B) {
        for b.Loop() {
            _ = fmt.Sprintf("%s %s %s %s %s", words[0], words[1], words[2], words[3], words[4])
        }
    })

    b.Run("Join", func(b *testing.B) {
        for b.Loop() {
            _ = strings.Join(words, " ")
        }
    })

    b.Run("Builder", func(b *testing.B) {
        for b.Loop() {
            var sb strings.Builder
            for _, w := range words {
                sb.WriteString(w)
                sb.WriteByte(' ')
            }
            _ = sb.String()
        }
    })

    b.Run("Builder_prealloc", func(b *testing.B) {
        for b.Loop() {
            var sb strings.Builder
            sb.Grow(50) // pre-allocar
            for _, w := range words {
                sb.WriteString(w)
                sb.WriteByte(' ')
            }
            _ = sb.String()
        }
    })
}

// Output tipico:
// BenchmarkStringConcat/plus-8              2000000   523.4 ns/op   176 B/op    9 allocs/op
// BenchmarkStringConcat/Sprintf-8           3000000   412.1 ns/op   112 B/op    6 allocs/op
// BenchmarkStringConcat/Join-8             10000000   123.4 ns/op    48 B/op    2 allocs/op
// BenchmarkStringConcat/Builder-8           8000000   145.6 ns/op    64 B/op    3 allocs/op
// BenchmarkStringConcat/Builder_prealloc-8 12000000    98.7 ns/op    64 B/op    2 allocs/op
```

### 7.2 Benchmark por tamano de input

```go
func BenchmarkSearch(b *testing.B) {
    for _, size := range []int{10, 100, 1000, 10000, 100000} {
        data := make([]int, size)
        for i := range data {
            data[i] = i * 2
        }
        target := data[size-1] // worst case: ultimo elemento

        b.Run(fmt.Sprintf("linear/n=%d", size), func(b *testing.B) {
            for b.Loop() {
                LinearSearch(data, target)
            }
        })

        sorted := slices.Clone(data)
        slices.Sort(sorted)

        b.Run(fmt.Sprintf("binary/n=%d", size), func(b *testing.B) {
            for b.Loop() {
                BinarySearch(sorted, target)
            }
        })
    }
}

// Output muestra como escala cada algoritmo:
// BenchmarkSearch/linear/n=10-8        100000000    12.3 ns/op
// BenchmarkSearch/binary/n=10-8        200000000     5.6 ns/op
// BenchmarkSearch/linear/n=100-8        20000000    67.8 ns/op
// BenchmarkSearch/binary/n=100-8       100000000    10.2 ns/op
// BenchmarkSearch/linear/n=1000-8        2000000   678.9 ns/op
// BenchmarkSearch/binary/n=1000-8       80000000    15.4 ns/op
// BenchmarkSearch/linear/n=10000-8        200000  6789.0 ns/op
// BenchmarkSearch/binary/n=10000-8      50000000    20.1 ns/op
// BenchmarkSearch/linear/n=100000-8        20000 67890.0 ns/op
// BenchmarkSearch/binary/n=100000-8     30000000    25.3 ns/op
//
// Linear: O(n) — tiempo crece linealmente
// Binary: O(log n) — tiempo crece logaritmicamente
```

### 7.3 Benchmark de JSON serialization

```go
type User struct {
    ID       int      `json:"id"`
    Name     string   `json:"name"`
    Email    string   `json:"email"`
    Tags     []string `json:"tags"`
    Settings map[string]string `json:"settings"`
}

func BenchmarkJSON(b *testing.B) {
    user := User{
        ID:    12345,
        Name:  "Alice Johnson",
        Email: "alice@example.com",
        Tags:  []string{"admin", "developer", "team-lead"},
        Settings: map[string]string{
            "theme":    "dark",
            "language": "en",
            "timezone": "UTC",
        },
    }

    b.Run("Marshal", func(b *testing.B) {
        b.ReportAllocs()
        for b.Loop() {
            json.Marshal(user)
        }
    })

    data, _ := json.Marshal(user)

    b.Run("Unmarshal", func(b *testing.B) {
        b.ReportAllocs()
        for b.Loop() {
            var u User
            json.Unmarshal(data, &u)
        }
    })

    b.Run("Encoder", func(b *testing.B) {
        b.ReportAllocs()
        for b.Loop() {
            enc := json.NewEncoder(io.Discard)
            enc.Encode(user)
        }
    })

    b.Run("Decoder", func(b *testing.B) {
        b.ReportAllocs()
        for b.Loop() {
            var u User
            dec := json.NewDecoder(bytes.NewReader(data))
            dec.Decode(&u)
        }
    })
}
```

### 7.4 Benchmark de concurrencia (mutex vs RWMutex vs sync.Map)

```go
func BenchmarkMapAccess(b *testing.B) {
    const numEntries = 1000
    const readPct = 90 // 90% reads, 10% writes

    b.Run("Mutex", func(b *testing.B) {
        var mu sync.Mutex
        m := make(map[int]int, numEntries)
        for i := 0; i < numEntries; i++ {
            m[i] = i
        }

        b.ResetTimer()
        b.RunParallel(func(pb *testing.PB) {
            i := 0
            for pb.Next() {
                i++
                if i%100 < readPct {
                    mu.Lock()
                    _ = m[i%numEntries]
                    mu.Unlock()
                } else {
                    mu.Lock()
                    m[i%numEntries] = i
                    mu.Unlock()
                }
            }
        })
    })

    b.Run("RWMutex", func(b *testing.B) {
        var mu sync.RWMutex
        m := make(map[int]int, numEntries)
        for i := 0; i < numEntries; i++ {
            m[i] = i
        }

        b.ResetTimer()
        b.RunParallel(func(pb *testing.PB) {
            i := 0
            for pb.Next() {
                i++
                if i%100 < readPct {
                    mu.RLock()
                    _ = m[i%numEntries]
                    mu.RUnlock()
                } else {
                    mu.Lock()
                    m[i%numEntries] = i
                    mu.Unlock()
                }
            }
        })
    })

    b.Run("SyncMap", func(b *testing.B) {
        var m sync.Map
        for i := 0; i < numEntries; i++ {
            m.Store(i, i)
        }

        b.ResetTimer()
        b.RunParallel(func(pb *testing.PB) {
            i := 0
            for pb.Next() {
                i++
                if i%100 < readPct {
                    m.Load(i % numEntries)
                } else {
                    m.Store(i%numEntries, i)
                }
            }
        })
    })
}

// Output tipico (90% reads):
// BenchmarkMapAccess/Mutex-8        5000000    234.5 ns/op
// BenchmarkMapAccess/RWMutex-8     20000000     78.9 ns/op   ← RWMutex gana con muchos reads
// BenchmarkMapAccess/SyncMap-8     15000000     89.2 ns/op
//
// Con 50% reads:
// Mutex ≈ RWMutex ≈ SyncMap (la diferencia disminuye con mas writes)
```

### 7.5 Benchmark de allocations (zero-alloc patterns)

```go
func BenchmarkBufferReuse(b *testing.B) {
    data := []byte("hello world, this is some data to process")

    b.Run("new_buffer_each_time", func(b *testing.B) {
        b.ReportAllocs()
        for b.Loop() {
            buf := make([]byte, 0, len(data))
            buf = append(buf, data...)
            _ = buf
        }
    })

    b.Run("sync_pool", func(b *testing.B) {
        pool := sync.Pool{
            New: func() any {
                return make([]byte, 0, 128)
            },
        }

        b.ReportAllocs()
        for b.Loop() {
            buf := pool.Get().([]byte)
            buf = buf[:0]
            buf = append(buf, data...)
            pool.Put(buf)
        }
    })

    b.Run("bytes_buffer_pool", func(b *testing.B) {
        pool := sync.Pool{
            New: func() any {
                return new(bytes.Buffer)
            },
        }

        b.ReportAllocs()
        for b.Loop() {
            buf := pool.Get().(*bytes.Buffer)
            buf.Reset()
            buf.Write(data)
            _ = buf.Bytes()
            pool.Put(buf)
        }
    })
}

// Output:
// new_buffer_each_time-8   10000000   120.3 ns/op   64 B/op   1 allocs/op
// sync_pool-8              30000000    45.6 ns/op    0 B/op   0 allocs/op  ← zero alloc
// bytes_buffer_pool-8      25000000    52.1 ns/op    0 B/op   0 allocs/op  ← zero alloc
```

---

## 8. Errores comunes en benchmarks

### 8.1 No resetear el timer despues del setup

```go
// MAL: el setup se cuenta en el tiempo
func BenchmarkBad(b *testing.B) {
    data := generateHugeDataset() // 2 segundos de setup
    for b.Loop() {
        Process(data) // 1ms por operacion
    }
}
// Resultado: inflado porque incluye 2s de setup

// BIEN:
func BenchmarkGood(b *testing.B) {
    data := generateHugeDataset()
    b.ResetTimer() // ← excluir setup
    for b.Loop() {
        Process(data)
    }
}
```

### 8.2 Medir algo que no es lo que crees

```go
// MAL: medir la generacion de datos, no el sort
func BenchmarkBadSort(b *testing.B) {
    for b.Loop() {
        data := rand.Perm(10000) // genera datos nuevos cada iteracion
        slices.Sort(data)        // la generacion puede dominar el tiempo
    }
}

// BIEN: separar generacion de medicion
func BenchmarkGoodSort(b *testing.B) {
    original := rand.Perm(10000)
    b.ResetTimer()

    for b.Loop() {
        data := make([]int, len(original))
        copy(data, original)
        slices.Sort(data)
    }
}

// NOTA: el copy y make estan dentro del loop porque Sort es in-place
// Si no copias, la segunda iteracion sortea datos ya sorteados
// (sort de datos sorteados es O(n), no O(n log n))
```

### 8.3 Compilador eliminando el codigo

```go
// MAL (pre-Go 1.24): resultado descartado
func BenchmarkBad(b *testing.B) {
    data := []int{1, 2, 3, 4, 5}
    for i := 0; i < b.N; i++ {
        Sum(data) // compilador puede eliminar esto
    }
}
// Resultado: 0.3 ns/op (irreal — esta midiendo nada)

// BIEN (pre-Go 1.24): escapar resultado
var result int
func BenchmarkGood(b *testing.B) {
    data := []int{1, 2, 3, 4, 5}
    var r int
    for i := 0; i < b.N; i++ {
        r = Sum(data)
    }
    result = r
}

// BIEN (Go 1.24+): b.Loop() previene la eliminacion
func BenchmarkBest(b *testing.B) {
    data := []int{1, 2, 3, 4, 5}
    for b.Loop() {
        Sum(data)
    }
}
```

### 8.4 Benchmark no reproducible

```go
// MAL: depende de estado externo
func BenchmarkUnstable(b *testing.B) {
    for b.Loop() {
        resp, _ := http.Get("https://api.example.com/data")
        resp.Body.Close()
    }
}
// Problema: latencia de red variable, servidor puede throttlear

// BIEN: usar servidor local
func BenchmarkStable(b *testing.B) {
    server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        w.Write([]byte(`{"status":"ok"}`))
    }))
    defer server.Close()

    client := server.Client()
    b.ResetTimer()

    for b.Loop() {
        resp, _ := client.Get(server.URL)
        io.Copy(io.Discard, resp.Body)
        resp.Body.Close()
    }
}
```

### 8.5 Olvidar -benchmem

```go
// Sin -benchmem solo ves tiempo, no allocations
// Dos implementaciones pueden tener el mismo tiempo pero diferentes allocations
// Las allocations importan para:
// 1. Presion sobre el GC (mas allocations = mas GC pauses)
// 2. Cache locality (allocations dispersas = cache misses)
// 3. Memoria total consumida

// SIEMPRE usar -benchmem o b.ReportAllocs()
```

---

## 9. Comparacion con C y Rust

| Aspecto | C | Go | Rust |
|---|---|---|---|
| Framework | Manual / Google Benchmark | `testing.B` (stdlib) | `criterion` / `#[bench]` (nightly) |
| Ejecucion | Manual | `go test -bench` | `cargo bench` |
| Iteraciones | Manual (loops) | `b.N` / `b.Loop()` auto | `iter` closure auto |
| Timer control | `clock_gettime` manual | `b.ResetTimer()` | Automatico en criterion |
| Memory stats | Valgrind/massif (externo) | `-benchmem` integrado | Requiere herramientas externas |
| Comparacion | Diff manual | `benchstat` | `criterion` (automatico con graficos) |
| Profiling | perf, gprof, Valgrind | `pprof` integrado | `perf`, `flamegraph` |
| Paralelismo | Manual (pthreads) | `b.RunParallel()` | `criterion` con threads |
| Sub-benchmarks | Manual | `b.Run()` | Benchmark groups |
| Estadisticas | Manual | `benchstat` (p-values) | `criterion` (confidence intervals) |
| Flame graphs | `perf record` + `flamegraph` | `pprof -http` (integrado) | `cargo-flamegraph` |
| Output format | Custom | Texto estandarizado | HTML con graficos (criterion) |

### 9.1 Ejemplo comparativo: benchmarkear una funcion

```c
// C — con clock_gettime (manual)
#include <time.h>
#include <stdio.h>

int sum(int* arr, int n) {
    int total = 0;
    for (int i = 0; i < n; i++) total += arr[i];
    return total;
}

int main() {
    int arr[1000];
    for (int i = 0; i < 1000; i++) arr[i] = i;

    struct timespec start, end;
    int iterations = 10000000;

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < iterations; i++) {
        volatile int r = sum(arr, 1000); // volatile previene eliminacion
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    double ns = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
    printf("%.2f ns/op\n", ns / iterations);
    return 0;
}
// Problemas: elegir iterations manualmente, sin memory stats,
// sin comparacion, sin profiling integrado
```

```go
// Go — integrado
func BenchmarkSum(b *testing.B) {
    arr := make([]int, 1000)
    for i := range arr { arr[i] = i }
    b.ResetTimer()
    b.ReportAllocs()
    for b.Loop() {
        Sum(arr)
    }
}
// go test -bench=BenchmarkSum -benchmem
// Automatico: iterations, timer, memory, output estandar
```

```rust
// Rust — con criterion (lo mas parecido a Go)
use criterion::{criterion_group, criterion_main, Criterion};

fn sum(arr: &[i32]) -> i32 {
    arr.iter().sum()
}

fn bench_sum(c: &mut Criterion) {
    let arr: Vec<i32> = (0..1000).collect();
    c.bench_function("sum_1000", |b| {
        b.iter(|| sum(criterion::black_box(&arr)))
    });
}

criterion_group!(benches, bench_sum);
criterion_main!(benches);

// cargo bench
// Output: HTML report con graficos, confidence intervals,
// comparacion automatica con run anterior
// black_box() = equivalente a Go 1.24 b.Loop() (previene eliminacion)
```

**Diferencias clave**:
- **Go**: todo integrado en la toolchain. Ejecutar benchmarks es tan facil como ejecutar tests. `pprof` para profiling tambien integrado.
- **Rust (criterion)**: output mas sofisticado (HTML con graficos, confidence intervals automaticos, comparacion con baseline). Pero requiere dependencia externa.
- **C**: todo manual. Elegir iteraciones, medir tiempo, calcular stats. Profiling con herramientas externas (perf, Valgrind).

---

## 10. Programa completo: benchmark suite para un cache

```go
// ============================================================================
// Archivo: cache/cache.go
// ============================================================================

package cache

import (
    "container/list"
    "sync"
)

// LRU implements a thread-safe Least Recently Used cache
type LRU[K comparable, V any] struct {
    mu       sync.RWMutex
    capacity int
    items    map[K]*list.Element
    order    *list.List // front = most recent, back = least recent
}

type entry[K comparable, V any] struct {
    key   K
    value V
}

func NewLRU[K comparable, V any](capacity int) *LRU[K, V] {
    return &LRU[K, V]{
        capacity: capacity,
        items:    make(map[K]*list.Element, capacity),
        order:    list.New(),
    }
}

func (c *LRU[K, V]) Get(key K) (V, bool) {
    c.mu.Lock()
    defer c.mu.Unlock()

    if elem, ok := c.items[key]; ok {
        c.order.MoveToFront(elem)
        return elem.Value.(*entry[K, V]).value, true
    }
    var zero V
    return zero, false
}

func (c *LRU[K, V]) Put(key K, value V) {
    c.mu.Lock()
    defer c.mu.Unlock()

    if elem, ok := c.items[key]; ok {
        c.order.MoveToFront(elem)
        elem.Value.(*entry[K, V]).value = value
        return
    }

    if c.order.Len() >= c.capacity {
        oldest := c.order.Back()
        if oldest != nil {
            c.order.Remove(oldest)
            delete(c.items, oldest.Value.(*entry[K, V]).key)
        }
    }

    elem := c.order.PushFront(&entry[K, V]{key: key, value: value})
    c.items[key] = elem
}

func (c *LRU[K, V]) Len() int {
    c.mu.RLock()
    defer c.mu.RUnlock()
    return c.order.Len()
}

func (c *LRU[K, V]) Delete(key K) {
    c.mu.Lock()
    defer c.mu.Unlock()

    if elem, ok := c.items[key]; ok {
        c.order.Remove(elem)
        delete(c.items, key)
    }
}

// SimpleMap is a trivial map-based cache for comparison (no eviction)
type SimpleMap[K comparable, V any] struct {
    mu sync.RWMutex
    m  map[K]V
}

func NewSimpleMap[K comparable, V any]() *SimpleMap[K, V] {
    return &SimpleMap[K, V]{m: make(map[K]V)}
}

func (s *SimpleMap[K, V]) Get(key K) (V, bool) {
    s.mu.RLock()
    defer s.mu.RUnlock()
    v, ok := s.m[key]
    return v, ok
}

func (s *SimpleMap[K, V]) Put(key K, value V) {
    s.mu.Lock()
    defer s.mu.Unlock()
    s.m[key] = value
}
```

```go
// ============================================================================
// Archivo: cache/cache_test.go
// ============================================================================

package cache

import (
    "fmt"
    "math/rand"
    "sync"
    "testing"
)

// ============================================================================
// Unit tests (para verificar correctness antes de benchmarkear)
// ============================================================================

func TestLRU(t *testing.T) {
    t.Run("basic get/put", func(t *testing.T) {
        c := NewLRU[string, int](3)
        c.Put("a", 1)
        c.Put("b", 2)
        c.Put("c", 3)

        v, ok := c.Get("b")
        if !ok || v != 2 {
            t.Errorf("Get(b) = %d, %v; want 2, true", v, ok)
        }
    })

    t.Run("eviction", func(t *testing.T) {
        c := NewLRU[string, int](2)
        c.Put("a", 1)
        c.Put("b", 2)
        c.Put("c", 3) // should evict "a"

        if _, ok := c.Get("a"); ok {
            t.Error("expected 'a' to be evicted")
        }
        if v, ok := c.Get("b"); !ok || v != 2 {
            t.Errorf("Get(b) = %d, %v; want 2, true", v, ok)
        }
        if v, ok := c.Get("c"); !ok || v != 3 {
            t.Errorf("Get(c) = %d, %v; want 3, true", v, ok)
        }
    })

    t.Run("update existing", func(t *testing.T) {
        c := NewLRU[string, int](2)
        c.Put("a", 1)
        c.Put("a", 10)

        v, _ := c.Get("a")
        if v != 10 {
            t.Errorf("Get(a) = %d, want 10", v)
        }
        if c.Len() != 1 {
            t.Errorf("Len() = %d, want 1", c.Len())
        }
    })

    t.Run("access refreshes order", func(t *testing.T) {
        c := NewLRU[string, int](2)
        c.Put("a", 1)
        c.Put("b", 2)
        c.Get("a")    // refresh "a" — now "b" is least recent
        c.Put("c", 3) // should evict "b", not "a"

        if _, ok := c.Get("b"); ok {
            t.Error("expected 'b' to be evicted")
        }
        if _, ok := c.Get("a"); !ok {
            t.Error("expected 'a' to survive")
        }
    })
}

// ============================================================================
// Benchmarks
// ============================================================================

// --- Benchmark: Put ---

func BenchmarkLRU_Put(b *testing.B) {
    for _, cap := range []int{100, 1000, 10000} {
        b.Run(fmt.Sprintf("cap=%d", cap), func(b *testing.B) {
            c := NewLRU[int, int](cap)
            b.ResetTimer()
            b.ReportAllocs()

            for b.Loop() {
                c.Put(rand.Intn(cap*2), 42)
            }
        })
    }
}

// --- Benchmark: Get (cache hit) ---

func BenchmarkLRU_Get_Hit(b *testing.B) {
    for _, cap := range []int{100, 1000, 10000} {
        b.Run(fmt.Sprintf("cap=%d", cap), func(b *testing.B) {
            c := NewLRU[int, int](cap)
            for i := 0; i < cap; i++ {
                c.Put(i, i)
            }
            b.ResetTimer()

            for b.Loop() {
                c.Get(rand.Intn(cap))
            }
        })
    }
}

// --- Benchmark: Get (cache miss) ---

func BenchmarkLRU_Get_Miss(b *testing.B) {
    c := NewLRU[int, int](1000)
    for i := 0; i < 1000; i++ {
        c.Put(i, i)
    }
    b.ResetTimer()

    for b.Loop() {
        c.Get(rand.Intn(10000) + 1000) // keys fuera del rango del cache
    }
}

// --- Benchmark: Mixed workload ---

func BenchmarkLRU_Mixed(b *testing.B) {
    type workload struct {
        name    string
        readPct int
    }

    workloads := []workload{
        {"read_heavy_90", 90},
        {"balanced_50", 50},
        {"write_heavy_10", 10},
    }

    for _, wl := range workloads {
        b.Run(wl.name, func(b *testing.B) {
            c := NewLRU[int, int](1000)
            for i := 0; i < 1000; i++ {
                c.Put(i, i)
            }
            b.ResetTimer()
            b.ReportAllocs()

            for b.Loop() {
                key := rand.Intn(2000)
                if rand.Intn(100) < wl.readPct {
                    c.Get(key)
                } else {
                    c.Put(key, key)
                }
            }
        })
    }
}

// --- Benchmark: LRU vs SimpleMap ---

func BenchmarkComparison(b *testing.B) {
    const numKeys = 1000

    b.Run("LRU", func(b *testing.B) {
        c := NewLRU[int, int](numKeys)
        for i := 0; i < numKeys; i++ {
            c.Put(i, i)
        }
        b.ResetTimer()

        for b.Loop() {
            key := rand.Intn(numKeys)
            if v, ok := c.Get(key); ok {
                _ = v
            } else {
                c.Put(key, key)
            }
        }
    })

    b.Run("SimpleMap", func(b *testing.B) {
        c := NewSimpleMap[int, int]()
        for i := 0; i < numKeys; i++ {
            c.Put(i, i)
        }
        b.ResetTimer()

        for b.Loop() {
            key := rand.Intn(numKeys)
            if v, ok := c.Get(key); ok {
                _ = v
            } else {
                c.Put(key, key)
            }
        }
    })

    b.Run("SyncMap", func(b *testing.B) {
        var m sync.Map
        for i := 0; i < numKeys; i++ {
            m.Store(i, i)
        }
        b.ResetTimer()

        for b.Loop() {
            key := rand.Intn(numKeys)
            if v, ok := m.Load(key); ok {
                _ = v
            } else {
                m.Store(key, key)
            }
        }
    })
}

// --- Benchmark: Concurrent access ---

func BenchmarkLRU_Concurrent(b *testing.B) {
    for _, nGoroutines := range []int{1, 2, 4, 8, 16} {
        b.Run(fmt.Sprintf("goroutines=%d", nGoroutines), func(b *testing.B) {
            c := NewLRU[int, int](1000)
            for i := 0; i < 1000; i++ {
                c.Put(i, i)
            }

            b.ResetTimer()
            b.SetParallelism(nGoroutines)
            b.RunParallel(func(pb *testing.PB) {
                i := rand.Intn(10000)
                for pb.Next() {
                    i++
                    key := i % 2000
                    if i%10 < 9 { // 90% reads
                        c.Get(key)
                    } else {
                        c.Put(key, i)
                    }
                }
            })
        })
    }
}

// --- Benchmark: Allocation profile ---

func BenchmarkLRU_Allocations(b *testing.B) {
    b.Run("put_new", func(b *testing.B) {
        b.ReportAllocs()
        c := NewLRU[int, int](b.N + 1) // capacity mayor que N
        b.ResetTimer()
        for i := 0; i < b.N; i++ {
            c.Put(i, i)
        }
    })

    b.Run("put_evict", func(b *testing.B) {
        b.ReportAllocs()
        c := NewLRU[int, int](100) // capacity chica → eviction constante
        b.ResetTimer()
        for i := 0; i < b.N; i++ {
            c.Put(i, i)
        }
    })

    b.Run("get_only", func(b *testing.B) {
        b.ReportAllocs()
        c := NewLRU[int, int](1000)
        for i := 0; i < 1000; i++ {
            c.Put(i, i)
        }
        b.ResetTimer()
        for i := 0; i < b.N; i++ {
            c.Get(i % 1000)
        }
    })
}
```

```
$ go test -bench=. -benchmem -run=^$ ./cache/

BenchmarkLRU_Put/cap=100-8           5000000    234.5 ns/op    96 B/op    2 allocs/op
BenchmarkLRU_Put/cap=1000-8          5000000    245.6 ns/op    96 B/op    2 allocs/op
BenchmarkLRU_Put/cap=10000-8         5000000    256.7 ns/op    96 B/op    2 allocs/op

BenchmarkLRU_Get_Hit/cap=100-8      10000000    112.3 ns/op     0 B/op    0 allocs/op
BenchmarkLRU_Get_Hit/cap=1000-8     10000000    115.6 ns/op     0 B/op    0 allocs/op
BenchmarkLRU_Get_Hit/cap=10000-8     8000000    123.4 ns/op     0 B/op    0 allocs/op

BenchmarkLRU_Get_Miss-8             30000000     45.6 ns/op     0 B/op    0 allocs/op

BenchmarkLRU_Mixed/read_heavy_90-8   8000000    145.6 ns/op    10 B/op    0 allocs/op
BenchmarkLRU_Mixed/balanced_50-8     5000000    189.0 ns/op    48 B/op    1 allocs/op
BenchmarkLRU_Mixed/write_heavy_10-8  5000000    212.3 ns/op    77 B/op    1 allocs/op

BenchmarkComparison/LRU-8            8000000    145.6 ns/op     0 B/op    0 allocs/op
BenchmarkComparison/SimpleMap-8     20000000     67.8 ns/op     0 B/op    0 allocs/op
BenchmarkComparison/SyncMap-8       15000000     89.2 ns/op    16 B/op    1 allocs/op

BenchmarkLRU_Concurrent/goroutines=1-8    8000000   145.6 ns/op
BenchmarkLRU_Concurrent/goroutines=2-8    5000000   234.5 ns/op
BenchmarkLRU_Concurrent/goroutines=4-8    3000000   345.6 ns/op
BenchmarkLRU_Concurrent/goroutines=8-8    2000000   567.8 ns/op
BenchmarkLRU_Concurrent/goroutines=16-8   1000000  1234.5 ns/op

BenchmarkLRU_Allocations/put_new-8     5000000   223.4 ns/op   112 B/op   2 allocs/op
BenchmarkLRU_Allocations/put_evict-8   3000000   345.6 ns/op    96 B/op   2 allocs/op
BenchmarkLRU_Allocations/get_only-8   10000000   112.3 ns/op     0 B/op   0 allocs/op
```

**Analisis de los resultados**:

1. **Put escala bien**: el tiempo de Put es casi constante sin importar la capacity (234-256 ns), porque el map y la linked list son O(1).

2. **Get es zero-alloc**: Get no alloquea memoria en heap — solo mueve nodos en la lista. Es ~50% mas rapido que Put.

3. **Get miss es mas rapido que hit**: no necesita MoveToFront, solo lookup en map.

4. **SimpleMap es 2x mas rapido que LRU**: pero SimpleMap no tiene eviction. LRU paga el overhead de la linked list por la funcionalidad de eviction.

5. **Concurrencia escala mal con Mutex**: de 1 a 16 goroutines, el throughput per-goroutine cae ~8x. Esto es esperado para un cache con Mutex exclusivo — la contention domina. Para mejorar: sharded LRU (multiples buckets con locks independientes).

6. **Put alloquea 2 allocations**: una para el `entry` y una para el `list.Element`. Get no alloquea. Esto guia la optimizacion: si los puts son el bottleneck, considerar un pool.

---

## 11. Ejercicios

### Ejercicio 1: Benchmark de hashing

Benchmarkea diferentes formas de hashear strings:
1. `md5.Sum()`
2. `sha256.Sum256()`
3. `crc32.ChecksumIEEE()`
4. `fnv.New32a()` (FNV hash)

Para cada una, medir con inputs de 16, 256, 4096, y 65536 bytes. Usar `b.SetBytes()` para reportar throughput en MB/s. Generar un cuadro comparativo con los resultados.

### Ejercicio 2: Optimizar y medir

Dado este codigo ineficiente:
```go
func CountWords(texts []string) map[string]int {
    result := map[string]int{}
    for _, text := range texts {
        words := strings.Split(text, " ")
        for _, word := range words {
            word = strings.ToLower(word)
            word = strings.Trim(word, ".,;:!?\"'")
            result[word]++
        }
    }
    return result
}
```

1. Escribe un benchmark baseline con 10,000 textos
2. Usa `pprof` para identificar los bottlenecks
3. Optimiza (hint: `strings.Builder`, pre-allocation, evitar `strings.Split`)
4. Benchmarkea la version optimizada
5. Usa `benchstat` para comparar con p-values

### Ejercicio 3: Benchmark de estructuras de datos

Compara el rendimiento de diferentes estructuras para un "set" de strings:
1. `map[string]struct{}`
2. `map[string]bool`
3. Sorted slice + binary search
4. `sync.Map` (para acceso concurrente)

Para cada una, benchmarkear: Insert, Contains (hit), Contains (miss), Delete. Con tamaños 100, 1000, 10000.

### Ejercicio 4: Benchmark paralelo de servidor HTTP

1. Crea un handler HTTP que consulta un cache y retorna JSON
2. Benchmarkea con `b.RunParallel` simulando 1, 4, 8, y 16 clientes concurrentes
3. Mide throughput (requests/segundo) y latencia (ns/op)
4. Compara con Mutex vs RWMutex vs sync.Map como backend del cache
5. Genera CPU y memory profiles, analiza con `pprof -http`

---

> **Siguiente**: C11/S02 - Testing, T04 - Testcontainers y mocking — interfaces para testing, dependency injection, testcontainers-go
