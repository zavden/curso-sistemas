# Conversiones de Tipo en Go

## 1. Introducción

Go es uno de los lenguajes más estrictos en cuanto a tipos: **no existe coerción implícita** (implicit casting). Incluso entre `int` e `int64`, o entre `int` y `float64`, debes convertir explícitamente. Esta decisión de diseño elimina una clase entera de bugs sutiles — a costa de un poco más de verbosidad.

La filosofía es clara: si una conversión puede perder datos (truncar un float, desbordar un entero, cortar bytes de un string), **el programador debe ser explícito** sobre su intención.

```
┌──────────────────────────────────────────────────────────────┐
│          CONVERSIONES EN GO — REGLA FUNDAMENTAL              │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Go NUNCA convierte tipos automáticamente entre variables    │
│                                                              │
│  var a int32 = 42                                            │
│  var b int64 = a          ✗ ERROR                            │
│  var b int64 = int64(a)   ✓ conversión explícita             │
│                                                              │
│  Única excepción: constantes sin tipo (untyped constants)    │
│  const X = 42                                                │
│  var b int64 = X          ✓ X es untyped, se adapta          │
│                                                              │
│  Formas de convertir:                                        │
│  ├─ T(valor)       → conversión de tipo (numéricos, etc.)    │
│  ├─ strconv.*      → string ↔ números                        │
│  ├─ []byte(s)      → string ↔ bytes                          │
│  ├─ []rune(s)      → string ↔ runes                          │
│  └─ T.(tipo)       → type assertion (interfaces)             │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Conversión entre tipos numéricos — Sintaxis `T(valor)`

La forma básica de conversión en Go es `TargetType(value)`:

```go
// Enteros de distinto tamaño
var a int32 = 42
var b int64 = int64(a)     // int32 → int64 (siempre seguro, se amplía)
var c int16 = int16(a)     // int32 → int16 (puede truncar)
var d uint8 = uint8(a)     // int32 → uint8 (puede truncar y cambiar signo)

// int ↔ sized ints
var n int = 100
var n64 int64 = int64(n)   // int → int64
var n32 int32 = int32(n)   // int → int32 (puede truncar en plataformas de 64 bits)
var m int = int(n64)       // int64 → int

// Entero ↔ flotante
var i int = 42
var f float64 = float64(i)  // 42 → 42.0 (seguro para valores razonables)
var j int = int(f)           // trunca decimales, NO redondea

// float32 ↔ float64
var f32 float32 = 3.14
var f64 float64 = float64(f32)  // pierde algo de precisión por representación
var g32 float32 = float32(f64)  // puede perder precisión si f64 es muy preciso
```

### No hay conversión implícita — Ni siquiera "segura"

```go
var a int32 = 42
var b int64 = 42

// ✗ NINGUNA de estas compila — aunque conceptualmente "cabrían"
// var c int64 = a        // cannot use a (variable of type int32) as type int64
// var d int = b           // cannot use b (variable of type int64) as type int
// var e float64 = a       // cannot use a (variable of type int32) as type float64

// ✓ Todas necesitan conversión explícita
var c int64 = int64(a)
var d int = int(b)
var e float64 = float64(a)

// Esto también aplica en operaciones
// var sum = a + b   // ✗ mismatched types int32 and int64
var sum = int64(a) + b  // ✓ convertir al tipo más grande
```

### Excepción: constantes sin tipo

```go
const X = 42          // untyped int constant
var a int32 = X       // ✓ X se adapta a int32
var b int64 = X       // ✓ X se adapta a int64
var c float64 = X     // ✓ X se adapta a float64
var d uint8 = X       // ✓ X cabe en uint8 (42 < 255)

const Big = 300
// var e uint8 = Big   // ✗ 300 overflows uint8 (max 255)
// Esto es un error de COMPILACIÓN, no de runtime
```

---

## 3. Peligros de la conversión numérica

### Truncamiento de enteros — Silencioso y peligroso

```go
package main

import "fmt"

func main() {
    // int32 → int16: trunca bits altos
    var big int32 = 70000
    var small int16 = int16(big)
    fmt.Println(small)  // 4464 (NO 70000)
    // 70000 = 0x00011170
    // int16 toma los últimos 16 bits: 0x1170 = 4464

    // int → uint8: overflow silencioso
    var n int = 256
    var b uint8 = uint8(n)
    fmt.Println(b)  // 0 (256 % 256 = 0)

    var m int = -1
    var u uint8 = uint8(m)
    fmt.Println(u)  // 255 (wrap around)

    // int64 → int32 en sistemas de 64 bits
    var timestamp int64 = 1705334400  // Unix timestamp 2024
    var bad int32 = int32(timestamp)
    fmt.Println(bad)  // 1705334400 (cabe, pero en 2038 no cabrá)
}
```

```
┌──────────────────────────────────────────────────────────────┐
│        TRUNCAMIENTO — QUÉ OCURRE INTERNAMENTE               │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  int32(70000) → int16:                                       │
│                                                              │
│  70000 en binario (32 bits):                                 │
│  00000000 00000001 00010001 01110000                         │
│                    ├────────────────┤                         │
│                    └─ int16 toma solo estos 16 bits          │
│                       = 0001000101110000 = 4464              │
│                                                              │
│  Si el bit 15 es 1 → número negativo en int16               │
│                                                              │
│  Go NO genera panic, NO genera warning, NO genera error      │
│  El programador es 100% responsable                          │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Conversión segura de enteros

```go
import "math"

// Verificar antes de convertir
func safeInt32(n int64) (int32, error) {
    if n < math.MinInt32 || n > math.MaxInt32 {
        return 0, fmt.Errorf("value %d overflows int32 (range %d to %d)",
            n, math.MinInt32, math.MaxInt32)
    }
    return int32(n), nil
}

func safeUint16(n int) (uint16, error) {
    if n < 0 || n > math.MaxUint16 {
        return 0, fmt.Errorf("value %d out of uint16 range (0-%d)",
            n, math.MaxUint16)
    }
    return uint16(n), nil
}

// Uso para puertos de red
func parsePort(s string) (uint16, error) {
    n, err := strconv.Atoi(s)
    if err != nil {
        return 0, fmt.Errorf("invalid port %q: %w", s, err)
    }
    if n < 1 || n > 65535 {
        return 0, fmt.Errorf("port %d out of range (1-65535)", n)
    }
    return uint16(n), nil
}
```

### Truncamiento float → int

```go
// int() trunca hacia cero — NO redondea
fmt.Println(int(3.9))    // 3  (no 4)
fmt.Println(int(-3.9))   // -3 (no -4)
fmt.Println(int(0.999))  // 0

// Para redondear correctamente:
import "math"

fmt.Println(int(math.Round(3.5)))   // 4
fmt.Println(int(math.Round(3.4)))   // 3
fmt.Println(int(math.Round(-3.5)))  // -4 (banker's rounding en Go ≥1.10: -4)

fmt.Println(int(math.Floor(3.9)))   // 3
fmt.Println(int(math.Ceil(3.1)))    // 4

// Valores especiales
fmt.Println(int(math.NaN()))    // comportamiento indefinido, resultado impredecible
fmt.Println(int(math.Inf(1)))   // comportamiento indefinido
// NUNCA convertir NaN o Inf a int — verificar primero
```

### Pérdida de precisión int → float

```go
// float64 tiene 53 bits de mantisa — enteros > 2^53 pierden precisión
var big int64 = 1<<53 + 1  // 9007199254740993
var f float64 = float64(big)
fmt.Println(f == float64(1<<53))  // true — ¡perdió el +1!

var back int64 = int64(f)
fmt.Println(back == big)  // false — no recuperas el valor original

// Para SysAdmin: los contadores de bytes que superan ~9 PB
// pierden precisión en float64. Usar int64 para contadores.
```

---

## 4. Conversión string ↔ números — Paquete `strconv`

El paquete `strconv` es la herramienta principal para convertir entre strings y tipos numéricos. Es fundamental para SysAdmin porque casi toda entrada (env vars, archivos de config, argumentos CLI, /proc) llega como texto:

### `strconv.Atoi` y `strconv.Itoa` — int rápido

```go
import "strconv"

// String → int (Atoi = ASCII to integer)
n, err := strconv.Atoi("8080")
if err != nil {
    log.Fatal(err)  // "invalid syntax" o "out of range"
}
fmt.Println(n)  // 8080

// int → string (Itoa = integer to ASCII)
s := strconv.Itoa(8080)
fmt.Println(s)  // "8080"

// Atoi falla con espacios, decimales, o basura
_, err = strconv.Atoi("  42  ")   // error: invalid syntax
_, err = strconv.Atoi("42.5")     // error: invalid syntax
_, err = strconv.Atoi("42abc")    // error: invalid syntax
_, err = strconv.Atoi("")         // error: invalid syntax
_, err = strconv.Atoi("99999999999999999999") // error: out of range
```

### `strconv.ParseInt` y `strconv.FormatInt` — Control total

```go
// ParseInt(s, base, bitSize)
// base: 0=auto, 2=bin, 8=oct, 10=dec, 16=hex
// bitSize: 0=int, 8=int8, 16=int16, 32=int32, 64=int64

// Decimal
n, err := strconv.ParseInt("42", 10, 64)       // int64(42)

// Hexadecimal (con o sin prefijo)
n, err = strconv.ParseInt("FF", 16, 64)         // int64(255)
n, err = strconv.ParseInt("0xFF", 0, 64)         // int64(255) — base 0 auto-detecta

// Octal (permisos Unix)
n, err = strconv.ParseInt("755", 8, 64)          // int64(493)
n, err = strconv.ParseInt("0o755", 0, 64)         // int64(493) — auto-detecta con 0o

// Binario
n, err = strconv.ParseInt("1010", 2, 64)          // int64(10)
n, err = strconv.ParseInt("0b1010", 0, 64)         // int64(10) — auto-detecta

// Con validación de rango
n, err = strconv.ParseInt("200", 10, 8)   // error: 200 overflows int8 (max 127)

// FormatInt(n, base)
s := strconv.FormatInt(255, 16)   // "ff"
s = strconv.FormatInt(493, 8)     // "755"
s = strconv.FormatInt(10, 2)      // "1010"
s = strconv.FormatInt(42, 10)     // "42"
```

### `strconv.ParseUint` — Para valores sin signo

```go
// Útil para parsing de /proc o datos del kernel
n, err := strconv.ParseUint("4294967295", 10, 32) // uint32 max
n, err = strconv.ParseUint("DEADBEEF", 16, 64)     // uint64
```

### `strconv.ParseFloat` y `strconv.FormatFloat`

```go
// ParseFloat(s, bitSize) — bitSize: 32 o 64
f, err := strconv.ParseFloat("3.14", 64)       // float64(3.14)
f, err = strconv.ParseFloat("1.5e10", 64)       // float64(1.5e10)
f, err = strconv.ParseFloat("NaN", 64)          // math.NaN()
f, err = strconv.ParseFloat("+Inf", 64)         // math.Inf(1)

// FormatFloat(f, fmt, prec, bitSize)
// fmt: 'f'=decimal, 'e'=exponencial, 'g'=compacto, 'b'=binario
s := strconv.FormatFloat(3.14159, 'f', 2, 64)   // "3.14"
s = strconv.FormatFloat(3.14159, 'f', -1, 64)    // "3.14159" (-1 = mínima precisión)
s = strconv.FormatFloat(1.5e10, 'e', 2, 64)      // "1.50e+10"
s = strconv.FormatFloat(1.5e10, 'g', -1, 64)     // "1.5e+10" (compacto)
```

### `strconv.ParseBool` y `strconv.FormatBool`

```go
// ParseBool acepta: 1, t, T, TRUE, true, True, 0, f, F, FALSE, false, False
b, err := strconv.ParseBool("true")    // true
b, err = strconv.ParseBool("1")         // true
b, err = strconv.ParseBool("T")         // true
b, err = strconv.ParseBool("false")     // false
b, err = strconv.ParseBool("0")         // false
b, err = strconv.ParseBool("yes")       // error: invalid syntax

// FormatBool siempre retorna "true" o "false"
s := strconv.FormatBool(true)   // "true"
s = strconv.FormatBool(false)    // "false"
```

---

## 5. `strconv` para SysAdmin — Patrones prácticos

### Parser de variables de entorno con tipos

```go
// Helpers genéricos para leer env vars tipadas
func getEnvStr(key, defaultVal string) string {
    if val := os.Getenv(key); val != "" {
        return val
    }
    return defaultVal
}

func getEnvInt(key string, defaultVal int) int {
    val := os.Getenv(key)
    if val == "" {
        return defaultVal
    }
    n, err := strconv.Atoi(val)
    if err != nil {
        log.Printf("WARNING: invalid %s=%q, using default %d", key, val, defaultVal)
        return defaultVal
    }
    return n
}

func getEnvInt64(key string, defaultVal int64) int64 {
    val := os.Getenv(key)
    if val == "" {
        return defaultVal
    }
    n, err := strconv.ParseInt(val, 10, 64)
    if err != nil {
        log.Printf("WARNING: invalid %s=%q, using default %d", key, val, defaultVal)
        return defaultVal
    }
    return n
}

func getEnvBool(key string, defaultVal bool) bool {
    val := os.Getenv(key)
    if val == "" {
        return defaultVal
    }
    b, err := strconv.ParseBool(val)
    if err != nil {
        log.Printf("WARNING: invalid %s=%q, using default %t", key, val, defaultVal)
        return defaultVal
    }
    return b
}

func getEnvFloat(key string, defaultVal float64) float64 {
    val := os.Getenv(key)
    if val == "" {
        return defaultVal
    }
    f, err := strconv.ParseFloat(val, 64)
    if err != nil {
        log.Printf("WARNING: invalid %s=%q, using default %f", key, val, defaultVal)
        return defaultVal
    }
    return f
}

func getEnvDuration(key string, defaultVal time.Duration) time.Duration {
    val := os.Getenv(key)
    if val == "" {
        return defaultVal
    }
    d, err := time.ParseDuration(val)
    if err != nil {
        log.Printf("WARNING: invalid %s=%q, using default %s", key, val, defaultVal)
        return defaultVal
    }
    return d
}

// Uso
port := getEnvInt("PORT", 8080)
debug := getEnvBool("DEBUG", false)
timeout := getEnvDuration("TIMEOUT", 30*time.Second)
maxMem := getEnvInt64("MAX_MEMORY_BYTES", 1<<30)  // 1 GB default
```

### Parser de /proc/meminfo

```go
func parseMemInfo() (map[string]int64, error) {
    data, err := os.ReadFile("/proc/meminfo")
    if err != nil {
        return nil, err
    }

    result := make(map[string]int64)
    for _, line := range strings.Split(string(data), "\n") {
        if line == "" {
            continue
        }
        // Formato: "MemTotal:       16384000 kB"
        parts := strings.Fields(line)
        if len(parts) < 2 {
            continue
        }
        key := strings.TrimSuffix(parts[0], ":")

        // parts[1] es el valor numérico como string
        val, err := strconv.ParseInt(parts[1], 10, 64)
        if err != nil {
            continue
        }

        // Si tiene unidad "kB", convertir a bytes
        if len(parts) >= 3 && parts[2] == "kB" {
            val *= 1024
        }

        result[key] = val
    }
    return result, nil
}

// Uso
mem, err := parseMemInfo()
if err != nil {
    log.Fatal(err)
}
totalGB := float64(mem["MemTotal"]) / (1024 * 1024 * 1024)
freeGB := float64(mem["MemFree"]) / (1024 * 1024 * 1024)
fmt.Printf("Memory: %.1f GB total, %.1f GB free\n", totalGB, freeGB)
```

### Parser de /proc/stat para CPU usage

```go
type CPUStats struct {
    User    int64
    Nice    int64
    System  int64
    Idle    int64
    IOWait  int64
    IRQ     int64
    SoftIRQ int64
}

func parseCPUStats() (*CPUStats, error) {
    data, err := os.ReadFile("/proc/stat")
    if err != nil {
        return nil, err
    }

    // Primera línea: "cpu  user nice system idle iowait irq softirq ..."
    line := strings.SplitN(string(data), "\n", 2)[0]
    fields := strings.Fields(line)
    if len(fields) < 8 || fields[0] != "cpu" {
        return nil, fmt.Errorf("unexpected /proc/stat format")
    }

    // Convertir cada campo de string a int64
    var stats CPUStats
    values := make([]int64, 7)
    for i := 0; i < 7; i++ {
        val, err := strconv.ParseInt(fields[i+1], 10, 64)
        if err != nil {
            return nil, fmt.Errorf("parsing field %d: %w", i+1, err)
        }
        values[i] = val
    }

    stats.User = values[0]
    stats.Nice = values[1]
    stats.System = values[2]
    stats.Idle = values[3]
    stats.IOWait = values[4]
    stats.IRQ = values[5]
    stats.SoftIRQ = values[6]

    return &stats, nil
}

func cpuUsagePercent(prev, curr *CPUStats) float64 {
    prevTotal := prev.User + prev.Nice + prev.System + prev.Idle +
        prev.IOWait + prev.IRQ + prev.SoftIRQ
    currTotal := curr.User + curr.Nice + curr.System + curr.Idle +
        curr.IOWait + curr.IRQ + curr.SoftIRQ

    totalDelta := float64(currTotal - prevTotal)
    idleDelta := float64(curr.Idle - prev.Idle)

    if totalDelta == 0 {
        return 0
    }
    return (1 - idleDelta/totalDelta) * 100
}
```

---

## 6. Conversión string ↔ []byte

La conversión más fundamental para I/O. Los strings son inmutables, los `[]byte` son mutables:

```go
// string → []byte (copia los bytes)
s := "Hello, World"
b := []byte(s)
b[0] = 'h'           // podemos modificar la copia
fmt.Println(s)        // "Hello, World" — original intacto
fmt.Println(string(b)) // "hello, World"

// []byte → string (copia los bytes)
data := []byte{72, 101, 108, 108, 111}
s = string(data)
fmt.Println(s)  // "Hello"

// Ambas conversiones crean una COPIA — tienen costo O(n)
```

### Optimizaciones del compilador

```go
// El compilador optimiza estos casos para evitar copias:

// 1. Comparación directa — no copia
if string(data) == "expected" { ... }

// 2. Búsqueda en map con key string — no copia
m := map[string]int{"hello": 1}
val := m[string(data)]  // no copia data a un string temporal

// 3. Range sobre []byte como string — no copia
for i, r := range string(data) { ... }

// 4. Concatenación con + — optimizado
s := "prefix" + string(data)  // una sola asignación
```

### Cuándo usar `string` vs `[]byte`

```
┌──────────────────────────────────────────────────────────────┐
│           string vs []byte — DECISIÓN                        │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Usar string cuando:                                         │
│  ├─ El texto es constante/inmutable                          │
│  ├─ Keys de mapas                                            │
│  ├─ Comparaciones frecuentes                                 │
│  ├─ Impresión y logging                                      │
│  └─ Valores de configuración                                 │
│                                                              │
│  Usar []byte cuando:                                         │
│  ├─ Datos binarios (archivos, red, crypto)                   │
│  ├─ Necesitas modificar el contenido                         │
│  ├─ I/O intensivo (io.Reader/io.Writer trabajan con bytes)   │
│  ├─ JSON/encoding (json.Marshal retorna []byte)              │
│  └─ Buffers de lectura/escritura                             │
│                                                              │
│  Tip: evita conversiones innecesarias en loops               │
│  ✗  for _, line := range lines {                             │
│        process([]byte(line))  // copia en cada iteración     │
│     }                                                        │
│  ✓  Trabajar con []byte desde el principio si es posible     │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 7. Conversión string ↔ []rune

Para manipulación de caracteres Unicode (no bytes):

```go
s := "Hello, 世界"  // 13 bytes, 9 caracteres

// string → []rune (cada rune = un code point Unicode)
runes := []rune(s)
fmt.Println(len(runes))  // 9 (caracteres)
fmt.Println(len(s))      // 13 (bytes)

// Acceso por posición de carácter (no byte)
fmt.Println(string(runes[7]))  // "界"
// vs
fmt.Println(s[7])              // byte, no rune — resultado incorrecto

// Modificar un carácter
runes[7] = '🌍'
fmt.Println(string(runes))  // "Hello, 世🌍"

// []rune → string
s2 := string(runes)

// int/rune → string: produce el carácter Unicode
fmt.Println(string(65))    // "A"
fmt.Println(string(0x4E16)) // "世"
fmt.Println(string(128640)) // "🎀" (emoji)

// ⚠ Esto NO convierte un número a su representación decimal
fmt.Println(string(42))    // "*" (ASCII 42), NO "42"
// Para "42" usar: strconv.Itoa(42) o fmt.Sprintf("%d", 42)
```

---

## 8. Conversiones con `fmt.Sprintf` — La navaja suiza

`fmt.Sprintf` puede convertir casi cualquier cosa a string, aunque es más lento que `strconv`:

```go
// Numéricos a string
s := fmt.Sprintf("%d", 42)        // "42"
s = fmt.Sprintf("%x", 255)         // "ff"
s = fmt.Sprintf("%X", 255)         // "FF"
s = fmt.Sprintf("%o", 493)         // "755"
s = fmt.Sprintf("%08b", 42)        // "00101010"
s = fmt.Sprintf("%.2f", 3.14159)   // "3.14"
s = fmt.Sprintf("%e", 1500000.0)   // "1.500000e+06"
s = fmt.Sprintf("%t", true)        // "true"

// Formato con padding
s = fmt.Sprintf("%-20s %5d", "nginx", 80)   // "nginx                   80"
s = fmt.Sprintf("%04d", 42)                   // "0042"

// Cualquier tipo con %v
s = fmt.Sprintf("%v", []int{1, 2, 3})         // "[1 2 3]"
s = fmt.Sprintf("%v", map[string]int{"a": 1}) // "map[a:1]"
s = fmt.Sprintf("%v", time.Now())              // "2024-01-15 14:30:45..."

// Tipo + valor con %#v (útil para debugging)
s = fmt.Sprintf("%#v", struct{ Port int }{8080})
// "struct { Port int }{Port:8080}"
```

### `strconv` vs `fmt.Sprintf` — Performance

```go
// strconv es ~3-5x más rápido que fmt.Sprintf para conversiones simples

// ✓ Preferir strconv para operaciones frecuentes
s := strconv.Itoa(port)                    // rápido
s = strconv.FormatFloat(cpu, 'f', 1, 64)   // rápido

// ✓ fmt.Sprintf para formato complejo o poco frecuente
s = fmt.Sprintf("%s:%d", host, port)        // formato compuesto
s = fmt.Sprintf("CPU: %.1f%%", cpuPercent)  // formato con contexto

// Benchmark típico:
// strconv.Itoa:     ~30 ns/op,  0 allocs
// fmt.Sprintf("%d"): ~100 ns/op, 1 alloc
```

---

## 9. Conversión entre tipos definidos (Named Types)

Un tipo definido crea un nuevo tipo, aunque tenga el mismo tipo subyacente:

```go
type Celsius float64
type Fahrenheit float64
type Kelvin float64

var c Celsius = 100.0
var f Fahrenheit = Fahrenheit(c)  // compila (mismo tipo subyacente float64)
// PERO esto es semánticamente incorrecto — solo convierte el tipo, no el valor

// Conversiones correctas con funciones:
func CelsiusToFahrenheit(c Celsius) Fahrenheit {
    return Fahrenheit(c*9/5 + 32)
}

func CelsiusToKelvin(c Celsius) Kelvin {
    return Kelvin(c + 273.15)
}

func FahrenheitToCelsius(f Fahrenheit) Celsius {
    return Celsius((f - 32) * 5 / 9)
}
```

### Patrón SysAdmin: tipos semánticos para evitar confusiones

```go
// Sin tipos definidos — propenso a errores
func createUser(name string, uid int, gid int) error { ... }
// createUser("admin", 1000, 0)  // ¿uid=1000 gid=0? ¿o al revés?

// Con tipos definidos — el compilador previene confusiones
type UID int
type GID int

func createUser(name string, uid UID, gid GID) error { ... }
// createUser("admin", GID(0), UID(1000))  // ✗ error de tipo
// createUser("admin", UID(1000), GID(0))  // ✓ correcto

// Conversión explícita necesaria
var uid UID = 1000       // ✓ constante sin tipo
var gid GID = 0          // ✓ constante sin tipo
// var id int = uid      // ✗ cannot use uid (type UID) as type int
var id int = int(uid)    // ✓ conversión explícita
```

```go
// Más ejemplos prácticos
type Port uint16
type Bytes int64
type Milliseconds int64
type Percentage float64

func checkDisk(mount string) (usedPct Percentage, freeBytes Bytes) {
    // Los tipos documentan qué retorna cada valor
    return 85.2, 1073741824
}

// No puedes mezclarlos por accidente
pct, free := checkDisk("/")
// var x Bytes = pct  // ✗ error de tipo — confusión imposible
```

---

## 10. Type Assertions — Conversión de interfaces

Las type assertions extraen el **tipo concreto** de un valor de interfaz:

```go
// Sintaxis: value.(Type)
var i interface{} = "hello"

// Type assertion — puede causar panic
s := i.(string)         // "hello" — OK
// n := i.(int)          // panic: interface conversion: interface {} is string, not int

// Type assertion segura — nunca hace panic
s, ok := i.(string)     // s="hello", ok=true
n, ok := i.(int)        // n=0, ok=false
```

### Type switch — Múltiples assertions

```go
func processValue(v interface{}) string {
    switch val := v.(type) {
    case string:
        return fmt.Sprintf("string: %q (len=%d)", val, len(val))
    case int:
        return fmt.Sprintf("int: %d", val)
    case int64:
        return fmt.Sprintf("int64: %d", val)
    case float64:
        return fmt.Sprintf("float64: %.2f", val)
    case bool:
        return fmt.Sprintf("bool: %t", val)
    case []byte:
        return fmt.Sprintf("bytes: %x (len=%d)", val, len(val))
    case nil:
        return "nil"
    default:
        return fmt.Sprintf("unknown type: %T", val)
    }
}
```

### Patrón SysAdmin: parser de JSON genérico

```go
import "encoding/json"

// JSON decode a interface{} produce tipos específicos:
// JSON object  → map[string]interface{}
// JSON array   → []interface{}
// JSON string  → string
// JSON number  → float64 (siempre, no int)
// JSON boolean → bool
// JSON null    → nil

func getConfigValue(jsonData []byte, key string) (string, error) {
    var raw map[string]interface{}
    if err := json.Unmarshal(jsonData, &raw); err != nil {
        return "", err
    }

    val, ok := raw[key]
    if !ok {
        return "", fmt.Errorf("key %q not found", key)
    }

    // Type switch para convertir a string independientemente del tipo JSON
    switch v := val.(type) {
    case string:
        return v, nil
    case float64:
        // JSON numbers son siempre float64
        if v == float64(int64(v)) {
            return strconv.FormatInt(int64(v), 10), nil  // "42" no "42.000000"
        }
        return strconv.FormatFloat(v, 'f', -1, 64), nil
    case bool:
        return strconv.FormatBool(v), nil
    case nil:
        return "", nil
    default:
        return fmt.Sprintf("%v", v), nil
    }
}
```

### Interfaces con type assertion — Patrón de extensión

```go
// io.Reader es la interfaz básica
// Pero algunos readers también implementan io.ReaderAt, io.Seeker, etc.

func processReader(r io.Reader) error {
    // ¿Soporta Seek?
    if seeker, ok := r.(io.Seeker); ok {
        // Podemos rebobinar
        _, err := seeker.Seek(0, io.SeekStart)
        if err != nil {
            return err
        }
    }

    // ¿Soporta ReadAt para lectura aleatoria?
    if ra, ok := r.(io.ReaderAt); ok {
        buf := make([]byte, 100)
        _, err := ra.ReadAt(buf, 0)
        if err != nil {
            return err
        }
    }

    // Procesamiento estándar con Read
    data, err := io.ReadAll(r)
    if err != nil {
        return err
    }
    fmt.Printf("Read %d bytes\n", len(data))
    return nil
}
```

---

## 11. Conversiones con `encoding/binary` — Datos de red y binarios

Para SysAdmin que trabajan con protocolos de red, formatos binarios, o datos de bajo nivel:

```go
import "encoding/binary"

// Entero → bytes (network byte order = Big Endian)
var port uint16 = 8080
buf := make([]byte, 2)
binary.BigEndian.PutUint16(buf, port)
fmt.Printf("% x\n", buf)  // 1f 90

// Bytes → entero
port2 := binary.BigEndian.Uint16(buf)
fmt.Println(port2)  // 8080

// uint32 (direcciones IP)
var ip uint32 = 0xC0A80001  // 192.168.0.1
buf4 := make([]byte, 4)
binary.BigEndian.PutUint32(buf4, ip)
fmt.Printf("%d.%d.%d.%d\n", buf4[0], buf4[1], buf4[2], buf4[3])
// 192.168.0.1

// uint64 (timestamps, counters)
var timestamp uint64 = 1705334400
buf8 := make([]byte, 8)
binary.BigEndian.PutUint64(buf8, timestamp)

// Little Endian (x86, lectura de archivos binarios locales)
binary.LittleEndian.PutUint32(buf4, 0x12345678)
fmt.Printf("% x\n", buf4)  // 78 56 34 12
```

### Leer/escribir structs binarios

```go
import (
    "bytes"
    "encoding/binary"
)

// Header de un protocolo custom (red, IPC)
type PacketHeader struct {
    Magic   uint32
    Version uint8
    Type    uint8
    Length  uint16
}

// Escribir a bytes
func (h *PacketHeader) Marshal() ([]byte, error) {
    buf := new(bytes.Buffer)
    err := binary.Write(buf, binary.BigEndian, h)
    return buf.Bytes(), err
}

// Leer desde bytes
func ParseHeader(data []byte) (*PacketHeader, error) {
    h := &PacketHeader{}
    r := bytes.NewReader(data)
    err := binary.Read(r, binary.BigEndian, h)
    return h, err
}
```

---

## 12. Conversión de tiempo — `time.Duration` y timestamps

El tiempo es una de las conversiones más frecuentes en SysAdmin:

```go
import "time"

// ─── Duraciones ──────────────────────────────────

// int → Duration (multiplicar por la unidad)
seconds := 30
d := time.Duration(seconds) * time.Second
fmt.Println(d)  // "30s"

// ⚠ Error común: multiplicar int directo
// d = seconds * time.Second  // ✗ mismatched types int and time.Duration
d = time.Duration(seconds) * time.Second  // ✓

// Duration → int (dividir por la unidad)
totalMs := d.Milliseconds()   // int64: 30000
totalSec := int(d.Seconds())  // int: 30 (Seconds() retorna float64)
totalNs := d.Nanoseconds()    // int64: 30000000000

// Desde string
d, err := time.ParseDuration("1h30m")   // 1 hora 30 min
d, err = time.ParseDuration("500ms")     // 500 milisegundos
d, err = time.ParseDuration("2.5s")      // 2.5 segundos
// No soporta "d" (días) ni "w" (semanas)

// A string
fmt.Println(d.String())  // "1h30m0s"

// ─── Unix timestamps ────────────────────────────

// time.Time → int64 (Unix epoch)
now := time.Now()
epoch := now.Unix()         // segundos desde 1970-01-01
epochMs := now.UnixMilli()  // milisegundos
epochNs := now.UnixNano()   // nanosegundos

// int64 → time.Time
t := time.Unix(epoch, 0)            // desde segundos
t = time.UnixMilli(epochMs)          // desde milisegundos (Go 1.17+)

// Formato personalizado
s := now.Format("2006-01-02 15:04:05")  // "2024-01-15 14:30:45"
s = now.Format(time.RFC3339)             // "2024-01-15T14:30:45-05:00"

// Desde string
t, err = time.Parse(time.RFC3339, "2024-01-15T14:30:45Z")
t, err = time.Parse("2006-01-02", "2024-01-15")
```

### Conversión de timeout en configs

```go
// Un config file podría tener timeout en segundos (int) o como duración (string)
type Config struct {
    TimeoutSeconds int    `json:"timeout_seconds,omitempty"`
    Timeout        string `json:"timeout,omitempty"` // "30s", "1m", etc.
}

func (c Config) GetTimeout() time.Duration {
    // Priorizar el formato de duración
    if c.Timeout != "" {
        d, err := time.ParseDuration(c.Timeout)
        if err == nil {
            return d
        }
    }
    if c.TimeoutSeconds > 0 {
        return time.Duration(c.TimeoutSeconds) * time.Second
    }
    return 30 * time.Second  // default
}
```

---

## 13. Conversión de direcciones IP

```go
import "net"

// String → net.IP
ip := net.ParseIP("192.168.1.100")
if ip == nil {
    log.Fatal("invalid IP")
}

// net.IP → string
s := ip.String()  // "192.168.1.100"

// net.IP es []byte — se puede inspeccionar
ipv4 := ip.To4()  // nil si es IPv6
if ipv4 != nil {
    fmt.Printf("Octets: %d.%d.%d.%d\n", ipv4[0], ipv4[1], ipv4[2], ipv4[3])
}

// CIDR parsing
ip, network, err := net.ParseCIDR("10.0.0.0/24")
fmt.Println(ip)        // 10.0.0.0
fmt.Println(network)   // 10.0.0.0/24
fmt.Println(network.Mask)  // ffffff00

// Verificar si una IP está en un rango
if network.Contains(net.ParseIP("10.0.0.50")) {
    fmt.Println("IP is in range")
}

// IP → uint32 (para cálculos y almacenamiento)
func ipToUint32(ip net.IP) uint32 {
    ip = ip.To4()
    if ip == nil {
        return 0
    }
    return binary.BigEndian.Uint32(ip)
}

// uint32 → IP
func uint32ToIP(n uint32) net.IP {
    ip := make(net.IP, 4)
    binary.BigEndian.PutUint32(ip, n)
    return ip
}
```

---

## 14. Conversión de permisos de archivo

```go
import "os"

// Octal string → os.FileMode
func parseFileMode(s string) (os.FileMode, error) {
    n, err := strconv.ParseUint(s, 8, 32)
    if err != nil {
        return 0, fmt.Errorf("invalid file mode %q: %w", s, err)
    }
    return os.FileMode(n), nil
}

// os.FileMode → octal string
func formatFileMode(m os.FileMode) string {
    return fmt.Sprintf("%04o", m.Perm())
}

// Uso
mode, err := parseFileMode("0644")
if err != nil {
    log.Fatal(err)
}
os.Chmod("/etc/myapp.conf", mode)

// Inspeccionar permisos
info, _ := os.Stat("/etc/passwd")
mode = info.Mode()
fmt.Printf("Mode: %s (%04o)\n", mode, mode.Perm())
// Mode: -rw-r--r-- (0644)

fmt.Println("Is regular file:", mode.IsRegular())
fmt.Println("Is directory:", mode.IsDir())
fmt.Println("Owner can write:", mode.Perm()&0200 != 0)
fmt.Println("World readable:", mode.Perm()&0004 != 0)
```

---

## 15. Conversiones peligrosas con `unsafe`

Go tiene el paquete `unsafe` para conversiones que rompen el sistema de tipos. **Evitar** salvo en código de muy bajo nivel:

```go
import "unsafe"

// unsafe.Sizeof — tamaño en bytes de un valor
var x int64 = 42
fmt.Println(unsafe.Sizeof(x))  // 8

// unsafe.Pointer — conversión entre punteros de distinto tipo
// ✗ NO hacer esto en código normal
func float64bits(f float64) uint64 {
    return *(*uint64)(unsafe.Pointer(&f))
}

// La stdlib lo usa internamente, pero tú debes preferir:
// math.Float64bits(f) — hace lo mismo, pero seguro y documentado
```

```go
// Conversión string ↔ []byte sin copia (Go 1.20+)
import "unsafe"

// ✗ Peligroso — solo si SABES que el []byte no será modificado
func unsafeString(b []byte) string {
    return unsafe.String(unsafe.SliceData(b), len(b))
}

// ✗ Peligroso — el string subyacente se corrompe si alguien modifica el []byte
func unsafeBytes(s string) []byte {
    return unsafe.Slice(unsafe.StringData(s), len(s))
}

// ✓ Preferir: copia segura
s := string(data)     // copia, pero seguro
b := []byte(s)        // copia, pero seguro
```

### Cuándo es aceptable `unsafe`

```go
// 1. Interop con C (cgo) — conversión de punteros C ↔ Go
// 2. mmap de archivos — leer archivos mapeados a memoria como structs
// 3. Optimizaciones medidas — solo después de profiling que muestre que es necesario
// 4. Código de runtime/sistema de bajo nivel

// Para SysAdmin: prácticamente NUNCA necesitas unsafe
// Si crees que lo necesitas, probablemente hay una forma mejor
```

---

## 16. Tabla completa de conversiones

```
┌───────────────────────────────────────────────────────────────────┐
│               TABLA DE CONVERSIONES EN GO                         │
├──────────────────────┬────────────────────────────────────────────┤
│  De → A              │  Método                                    │
├──────────────────────┼────────────────────────────────────────────┤
│  int → int64         │  int64(n)                                  │
│  int64 → int         │  int(n)  ⚠ puede truncar                  │
│  int → float64       │  float64(n)  ⚠ pierde precisión si >2^53  │
│  float64 → int       │  int(f)  ⚠ trunca decimales               │
│  int → uint          │  uint(n)  ⚠ negativo → número enorme      │
│  uint → int          │  int(u)  ⚠ overflow si > MaxInt            │
│  int → string        │  strconv.Itoa(n)                           │
│  string → int        │  strconv.Atoi(s)                           │
│  int64 → string      │  strconv.FormatInt(n, 10)                  │
│  string → int64      │  strconv.ParseInt(s, 10, 64)               │
│  float64 → string    │  strconv.FormatFloat(f, 'f', prec, 64)    │
│  string → float64    │  strconv.ParseFloat(s, 64)                 │
│  bool → string       │  strconv.FormatBool(b)                     │
│  string → bool       │  strconv.ParseBool(s)                      │
│  string → []byte     │  []byte(s)                                 │
│  []byte → string     │  string(b)                                 │
│  string → []rune     │  []rune(s)                                 │
│  []rune → string     │  string(r)                                 │
│  rune → string       │  string(r) — el carácter Unicode           │
│  int → rune          │  rune(n) — sinónimo de int32(n)            │
│  any → string        │  fmt.Sprintf("%v", x)                      │
│  interface{} → T     │  x.(T) o x.(T) con ok                     │
│  T → interface{}     │  automático (implícito)                    │
│  Type1 → Type2       │  Type2(x) si mismo tipo subyacente         │
│  *T → unsafe.Pointer │  unsafe.Pointer(p)  ⚠ unsafe              │
│  Duration → int      │  int(d.Seconds()) o d.Milliseconds()      │
│  int → Duration      │  time.Duration(n) * time.Second            │
│  IP → string         │  ip.String()                               │
│  string → IP         │  net.ParseIP(s)                            │
│  string → FileMode   │  strconv.ParseUint(s, 8, 32) + cast       │
│  FileMode → string   │  fmt.Sprintf("%04o", m.Perm())            │
└──────────────────────┴────────────────────────────────────────────┘
```

---

## 17. Ejemplo SysAdmin: Parser de configuración multi-formato

```go
package main

import (
    "encoding/json"
    "fmt"
    "log"
    "math"
    "net"
    "os"
    "strconv"
    "strings"
    "time"
)

type ServerConfig struct {
    Host           string        `json:"host"`
    Port           uint16        `json:"port"`
    ReadTimeout    time.Duration `json:"-"`
    ReadTimeoutStr string        `json:"read_timeout"` // "30s", "1m"
    WriteTimeout   time.Duration `json:"-"`
    WriteTimeoutStr string       `json:"write_timeout"`
    MaxBodySize    int64         `json:"max_body_size"` // bytes
    MaxBodySizeStr string        `json:"-"`             // "100MB"
    TLSEnabled     bool          `json:"tls_enabled"`
    AllowedCIDRs   []string      `json:"allowed_cidrs"`
    LogLevel       string        `json:"log_level"`
    Workers        int           `json:"workers"`
}

// ParseSize convierte "100MB", "2GB", "500KB" a bytes
func ParseSize(s string) (int64, error) {
    s = strings.TrimSpace(strings.ToUpper(s))
    if s == "" {
        return 0, fmt.Errorf("empty size string")
    }

    // Encontrar donde termina el número y empieza la unidad
    var numStr, unit string
    for i, ch := range s {
        if ch < '0' || ch > '9' {
            if ch != '.' {
                numStr = s[:i]
                unit = s[i:]
                break
            }
        }
    }
    if numStr == "" {
        numStr = s
    }

    val, err := strconv.ParseFloat(numStr, 64)
    if err != nil {
        return 0, fmt.Errorf("invalid number in size %q: %w", s, err)
    }

    multiplier := int64(1)
    switch strings.TrimSpace(unit) {
    case "", "B":
        multiplier = 1
    case "KB", "K":
        multiplier = 1024
    case "MB", "M":
        multiplier = 1024 * 1024
    case "GB", "G":
        multiplier = 1024 * 1024 * 1024
    case "TB", "T":
        multiplier = 1024 * 1024 * 1024 * 1024
    default:
        return 0, fmt.Errorf("unknown size unit %q", unit)
    }

    result := int64(val * float64(multiplier))
    return result, nil
}

// FormatSize convierte bytes a representación legible
func FormatSize(bytes int64) string {
    const (
        KB = 1024
        MB = KB * 1024
        GB = MB * 1024
        TB = GB * 1024
    )
    switch {
    case bytes >= TB:
        return fmt.Sprintf("%.2f TB", float64(bytes)/float64(TB))
    case bytes >= GB:
        return fmt.Sprintf("%.2f GB", float64(bytes)/float64(GB))
    case bytes >= MB:
        return fmt.Sprintf("%.2f MB", float64(bytes)/float64(MB))
    case bytes >= KB:
        return fmt.Sprintf("%.2f KB", float64(bytes)/float64(KB))
    default:
        return fmt.Sprintf("%d B", bytes)
    }
}

// LoadConfig carga configuración desde JSON con validación y conversión de tipos
func LoadConfig(path string) (*ServerConfig, error) {
    data, err := os.ReadFile(path)
    if err != nil {
        return nil, fmt.Errorf("reading config: %w", err)
    }

    var cfg ServerConfig
    if err := json.Unmarshal(data, &cfg); err != nil {
        return nil, fmt.Errorf("parsing config: %w", err)
    }

    // Aplicar env var overrides (string → tipos concretos)
    if host := os.Getenv("HOST"); host != "" {
        cfg.Host = host
    }
    if portStr := os.Getenv("PORT"); portStr != "" {
        port, err := strconv.ParseUint(portStr, 10, 16)
        if err != nil {
            return nil, fmt.Errorf("invalid PORT %q: %w", portStr, err)
        }
        if port == 0 || port > math.MaxUint16 {
            return nil, fmt.Errorf("PORT %d out of range (1-65535)", port)
        }
        cfg.Port = uint16(port)
    }

    // Convertir strings de duración a time.Duration
    if cfg.ReadTimeoutStr != "" {
        d, err := time.ParseDuration(cfg.ReadTimeoutStr)
        if err != nil {
            return nil, fmt.Errorf("invalid read_timeout %q: %w", cfg.ReadTimeoutStr, err)
        }
        cfg.ReadTimeout = d
    } else {
        cfg.ReadTimeout = 15 * time.Second
    }

    if cfg.WriteTimeoutStr != "" {
        d, err := time.ParseDuration(cfg.WriteTimeoutStr)
        if err != nil {
            return nil, fmt.Errorf("invalid write_timeout %q: %w", cfg.WriteTimeoutStr, err)
        }
        cfg.WriteTimeout = d
    } else {
        cfg.WriteTimeout = 15 * time.Second
    }

    // Validar CIDRs
    for _, cidr := range cfg.AllowedCIDRs {
        _, _, err := net.ParseCIDR(cidr)
        if err != nil {
            return nil, fmt.Errorf("invalid CIDR %q: %w", cidr, err)
        }
    }

    // Defaults
    if cfg.Host == "" {
        cfg.Host = "0.0.0.0"
    }
    if cfg.Port == 0 {
        cfg.Port = 8080
    }
    if cfg.MaxBodySize == 0 {
        cfg.MaxBodySize = 10 * 1024 * 1024 // 10 MB
    }
    if cfg.LogLevel == "" {
        cfg.LogLevel = "info"
    }
    if cfg.Workers == 0 {
        cfg.Workers = 4
    }

    return &cfg, nil
}

func (c *ServerConfig) Print() {
    fmt.Println("=== Server Configuration ===")
    fmt.Printf("  Host:          %s\n", c.Host)
    fmt.Printf("  Port:          %d\n", c.Port)
    fmt.Printf("  Listen:        %s:%d\n", c.Host, c.Port)
    fmt.Printf("  ReadTimeout:   %s\n", c.ReadTimeout)
    fmt.Printf("  WriteTimeout:  %s\n", c.WriteTimeout)
    fmt.Printf("  MaxBodySize:   %s (%d bytes)\n", FormatSize(c.MaxBodySize), c.MaxBodySize)
    fmt.Printf("  TLS:           %t\n", c.TLSEnabled)
    fmt.Printf("  Workers:       %d\n", c.Workers)
    fmt.Printf("  LogLevel:      %s\n", c.LogLevel)
    if len(c.AllowedCIDRs) > 0 {
        fmt.Printf("  AllowedCIDRs:  %s\n", strings.Join(c.AllowedCIDRs, ", "))
    }
}

func main() {
    // Demostrar conversiones
    fmt.Println("=== Size Conversions ===")
    sizes := []string{"100MB", "2GB", "500KB", "1.5TB"}
    for _, s := range sizes {
        bytes, err := ParseSize(s)
        if err != nil {
            log.Printf("Error parsing %q: %v", s, err)
            continue
        }
        fmt.Printf("  %s = %d bytes = %s\n", s, bytes, FormatSize(bytes))
    }

    fmt.Println()

    // Demostrar conversiones numéricas
    fmt.Println("=== Numeric Conversions ===")
    portStr := "8080"
    port, err := strconv.ParseUint(portStr, 10, 16)
    if err != nil {
        log.Fatal(err)
    }
    fmt.Printf("  Port string %q → uint64 %d → uint16 %d\n",
        portStr, port, uint16(port))

    fmt.Println()

    // Demostrar conversiones de tiempo
    fmt.Println("=== Time Conversions ===")
    durations := []string{"30s", "1m30s", "2h", "500ms"}
    for _, ds := range durations {
        d, err := time.ParseDuration(ds)
        if err != nil {
            log.Printf("Error: %v", err)
            continue
        }
        fmt.Printf("  %q → %s → %d ms → %d ns\n",
            ds, d, d.Milliseconds(), d.Nanoseconds())
    }

    fmt.Println()

    // Demostrar conversión de IP
    fmt.Println("=== IP Conversions ===")
    ipStr := "192.168.1.100"
    ip := net.ParseIP(ipStr)
    ipv4 := ip.To4()
    ipUint := uint32(ipv4[0])<<24 | uint32(ipv4[1])<<16 | uint32(ipv4[2])<<8 | uint32(ipv4[3])
    fmt.Printf("  %s → bytes:%v → uint32:%d → hex:0x%08X\n",
        ipStr, []byte(ipv4), ipUint, ipUint)
}
```

---

## 18. Comparación: Go vs C vs Rust

| Aspecto | Go | C | Rust |
|---|---|---|---|
| Coerción implícita | Nunca (excepto untyped const) | Sí (`int` → `long`, `float` → `double`) | Nunca |
| Sintaxis | `T(value)` | `(T)value` o `T(value)` (C++) | `value as T` |
| Truncamiento | Silencioso (sin panic) | Silencioso (UB si signed overflow) | Silencioso con `as`, checked con `try_into()` |
| Checked conversion | Manual (`if n > MaxInt32`) | No estándar | `TryFrom`/`TryInto` (retorna Result) |
| string → int | `strconv.Atoi(s)` | `atoi(s)` (no reporta error) | `s.parse::<i32>()` (Result) |
| int → string | `strconv.Itoa(n)` | `sprintf(buf, "%d", n)` | `n.to_string()` |
| void* / any | `interface{}` / `any` | `void*` | `Box<dyn Any>` |
| Type assertion | `x.(T)` | cast manual | `.downcast()` |
| Unsafe cast | `unsafe.Pointer` | cast a `void*` | `std::mem::transmute` |
| Overflow en cast | Wrap silencioso | UB (signed), wrap (unsigned) | `as`: wrap, `into()`: compile error |

---

## 19. Errores comunes

| # | Error | Código | Solución |
|---|---|---|---|
| 1 | Asignar sin convertir | `var b int64 = intVar` | `int64(intVar)` |
| 2 | string(42) = "42" | `string(42)` produce `"*"` | `strconv.Itoa(42)` |
| 3 | int(3.9) = 4 | `int(3.9)` produce `3` | `int(math.Round(3.9))` |
| 4 | Ignorar error de Atoi | `n, _ := strconv.Atoi(s)` | Verificar err siempre |
| 5 | Overflow en cast | `uint8(256)` produce `0` | Verificar rango antes |
| 6 | Negativo a unsigned | `uint(-1)` produce MaxUint | Verificar `>= 0` antes |
| 7 | JSON number = float64 | `val.(int)` panic | `val.(float64)` + `int()` |
| 8 | int * Duration | `5 * time.Second` (literal OK) | `time.Duration(n) * time.Second` |
| 9 | ParseInt base incorrecta | `ParseInt("0xFF", 10, 64)` falla | Usar base 0 para auto-detect o 16 |
| 10 | Float precision loss | `float64(1<<53 + 1)` pierde el +1 | Usar int64 para contadores grandes |
| 11 | Type assertion sin ok | `x.(string)` puede hacer panic | `s, ok := x.(string)` |
| 12 | []byte string loop | `[]byte(s)` en cada iteración | Convertir una vez fuera del loop |

---

## 20. Ejercicios

### Ejercicio 1: Predicción de truncamiento
```go
package main

import "fmt"

func main() {
    var a int32 = 100000
    var b int16 = int16(a)
    var c int8 = int8(a)
    var d uint8 = uint8(a)

    fmt.Println(b)
    fmt.Println(c)
    fmt.Println(d)

    var neg int = -1
    var u uint = uint(neg)
    var u8 uint8 = uint8(neg)
    fmt.Println(u)
    fmt.Println(u8)
}
```
**Predicción**: ¿Qué imprime cada línea? Calcula el resultado del truncamiento para `b`, `c`, `d`. ¿Qué valor tiene `u` en una plataforma de 64 bits?

### Ejercicio 2: float → int
```go
package main

import (
    "fmt"
    "math"
)

func main() {
    values := []float64{3.7, -3.7, 0.5, -0.5, 99.999, math.Inf(1)}

    for _, v := range values {
        fmt.Printf("int(%6.3f)       = %d\n", v, int(v))
        fmt.Printf("Round(%6.3f)     = %d\n", v, int(math.Round(v)))
        fmt.Printf("Floor(%6.3f)     = %d\n", v, int(math.Floor(v)))
        fmt.Printf("Ceil(%6.3f)      = %d\n", v, int(math.Ceil(v)))
        fmt.Println()
    }
}
```
**Predicción**: ¿Qué imprime para cada valor? ¿Qué pasa con `int(math.Inf(1))`?

### Ejercicio 3: strconv errors
```go
package main

import (
    "fmt"
    "strconv"
)

func main() {
    tests := []string{"42", " 42", "42.5", "0xFF", "0o755", "", "abc", "99999999999999999999"}

    for _, s := range tests {
        n, err := strconv.Atoi(s)
        fmt.Printf("Atoi(%q) = %d, err=%v\n", s, n, err)
    }

    fmt.Println()

    for _, s := range tests {
        n, err := strconv.ParseInt(s, 0, 64)
        fmt.Printf("ParseInt(%q, 0, 64) = %d, err=%v\n", s, n, err)
    }
}
```
**Predicción**: ¿Cuáles tienen éxito con `Atoi`? ¿Cuáles con `ParseInt` base 0? ¿Cuál es la diferencia?

### Ejercicio 4: Typed vs untyped con operaciones
```go
package main

import "fmt"

type Meters float64
type Seconds float64
type Speed float64

func main() {
    var d Meters = 100
    var t Seconds = 9.58

    // ¿Cuáles compilan?
    // speed1 := d / t
    // speed2 := Speed(d / t)
    speed3 := Speed(d) / Speed(t)
    speed4 := Speed(float64(d) / float64(t))

    fmt.Println(speed3)
    fmt.Println(speed4)
}
```
**Predicción**: ¿Por qué `speed1` y `speed2` no compilan? ¿Compilan `speed3` y `speed4`? ¿Dan el mismo resultado?

### Ejercicio 5: Type assertion
```go
package main

import "fmt"

func classify(v interface{}) string {
    switch v.(type) {
    case int:
        return "int"
    case float64:
        return "float64"
    case string:
        return "string"
    case bool:
        return "bool"
    default:
        return fmt.Sprintf("other: %T", v)
    }
}

func main() {
    fmt.Println(classify(42))
    fmt.Println(classify(42.0))
    fmt.Println(classify("hello"))
    fmt.Println(classify(true))
    fmt.Println(classify(int64(42)))
    fmt.Println(classify(nil))

    // ¿Qué pasa con constantes sin tipo?
    const X = 42
    fmt.Println(classify(X))
}
```
**Predicción**: ¿Qué imprime `classify(int64(42))`? ¿Y `classify(X)` donde X es una constante sin tipo?

### Ejercicio 6: JSON numbers
```go
package main

import (
    "encoding/json"
    "fmt"
)

func main() {
    jsonStr := `{"port": 8080, "ratio": 0.75, "name": "test", "enabled": true}`

    var data map[string]interface{}
    json.Unmarshal([]byte(jsonStr), &data)

    for key, val := range data {
        fmt.Printf("%s: type=%T value=%v\n", key, val, val)
    }

    // ¿Cómo obtengo port como int?
    // port := data["port"].(int)  // ¿Funciona?
}
```
**Predicción**: ¿Qué tipo tiene `data["port"]`? ¿Por qué `.(int)` fallaría? ¿Cómo obtenerlo como `int`?

### Ejercicio 7: Parser de tamaños
Implementa una función `ParseSize(s string) (int64, error)` que:
1. Acepte: "100", "10KB", "5MB", "2.5GB", "1TB"
2. Soporte sufijos KB, MB, GB, TB (case-insensitive)
3. Retorne el valor en bytes
4. También implementa `FormatSize(bytes int64) string` para el camino inverso

### Ejercicio 8: Safe numeric converter
Implementa un paquete `safeconv` con funciones:
1. `IntToInt8(n int) (int8, error)`
2. `IntToUint16(n int) (uint16, error)` — para puertos
3. `Int64ToInt32(n int64) (int32, error)` — para timestamps pre-2038
4. `Float64ToInt64(f float64) (int64, error)` — detectar NaN/Inf/overflow

### Ejercicio 9: Config env parser
Escribe una función que lea estas variables de entorno y las convierta a su tipo correcto:
- `PORT` → uint16 (default 8080, rango 1-65535)
- `TIMEOUT` → time.Duration (acepta "30s" o segundos como entero)
- `MAX_SIZE` → int64 bytes (acepta "100MB" o bytes como entero)
- `DEBUG` → bool (acepta true/false/1/0/yes/no)
- `WORKERS` → int (default: número de CPUs)
- `ALLOWED_IPS` → []net.IP (comma-separated)

### Ejercicio 10: Binary protocol parser
Dado un "paquete de red" como []byte con este formato:
```
Offset  Size  Field
0       4     Magic (0xDEADBEEF)
4       1     Version
5       1     Type
6       2     Payload length (uint16, big-endian)
8       N     Payload (bytes)
```
Implementa `ParsePacket(data []byte) (*Packet, error)` que:
1. Valide el magic number
2. Extraiga cada campo con `encoding/binary`
3. Convierta el payload a string
4. Retorne un struct con todos los campos en sus tipos correctos

---

## 21. Resumen

```
┌──────────────────────────────────────────────────────────────┐
│        CONVERSIONES DE TIPO EN GO — RESUMEN                  │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  REGLA FUNDAMENTAL                                           │
│  Go NUNCA convierte tipos implícitamente entre variables     │
│  Toda conversión debe ser explícita: T(value)                │
│  Excepción: constantes sin tipo se adaptan al contexto       │
│                                                              │
│  CONVERSIONES NUMÉRICAS — T(value)                           │
│  ├─ Ampliar (int32→int64): siempre seguro                    │
│  ├─ Reducir (int64→int32): puede truncar silenciosamente     │
│  ├─ float→int: trunca decimales (NO redondea)                │
│  ├─ int→float: pierde precisión si > 2^53                    │
│  └─ signed→unsigned: wrap around con negativos               │
│                                                              │
│  STRING ↔ NÚMEROS — strconv                                  │
│  ├─ Atoi/Itoa: int rápido                                    │
│  ├─ ParseInt/FormatInt: bases, tamaños, control total        │
│  ├─ ParseFloat/FormatFloat: con precisión y formato          │
│  ├─ ParseBool/FormatBool: true/false/1/0/T/F                │
│  └─ Siempre verificar error (nunca usar _)                   │
│                                                              │
│  STRING ↔ BYTES/RUNES                                        │
│  ├─ []byte(s) / string(b): copia O(n)                        │
│  ├─ []rune(s) / string(r): para Unicode                      │
│  └─ string(42) = "*", NO "42"                                │
│                                                              │
│  INTERFACES                                                  │
│  ├─ Type assertion: x.(T) — panic si falla                   │
│  ├─ Safe assertion: v, ok := x.(T)                           │
│  └─ Type switch: switch v := x.(type) { ... }               │
│                                                              │
│  SYSADMIN: strconv para env/config/proc, binary para red,   │
│  net.ParseIP/CIDR, time.ParseDuration, os.FileMode           │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: T04 - Tipo rune — Unicode code points, iteración por runes vs bytes, strings.Builder
