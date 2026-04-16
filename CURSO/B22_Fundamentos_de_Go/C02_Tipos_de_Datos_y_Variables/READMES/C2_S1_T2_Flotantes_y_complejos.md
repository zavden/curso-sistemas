# T02 — Flotantes y complejos

## 1. Tipos de punto flotante en Go

Go tiene dos tipos de punto flotante que siguen el estándar
**IEEE 754**, el mismo que usan C, Rust, Python, Java y casi
todos los lenguajes modernos.

```
    ┌──────────────────────────────────────────────────────────────┐
    │              Tipos de punto flotante en Go                   │
    │                                                              │
    │  ┌────────────────────────────────────────────────────────┐  │
    │  │ float32 — precisión simple (single precision)          │  │
    │  │                                                        │  │
    │  │ Tamaño:     4 bytes (32 bits)                          │  │
    │  │ Signo:      1 bit                                      │  │
    │  │ Exponente:  8 bits                                     │  │
    │  │ Mantisa:    23 bits                                    │  │
    │  │ Precisión:  ~7 dígitos decimales                       │  │
    │  │ Rango:      ±1.18e-38 a ±3.4e+38                      │  │
    │  └────────────────────────────────────────────────────────┘  │
    │                                                              │
    │  ┌────────────────────────────────────────────────────────┐  │
    │  │ float64 — precisión doble (double precision)           │  │
    │  │                                                        │  │
    │  │ Tamaño:     8 bytes (64 bits)                          │  │
    │  │ Signo:      1 bit                                      │  │
    │  │ Exponente:  11 bits                                    │  │
    │  │ Mantisa:    52 bits                                    │  │
    │  │ Precisión:  ~15-16 dígitos decimales                   │  │
    │  │ Rango:      ±2.23e-308 a ±1.8e+308                    │  │
    │  └────────────────────────────────────────────────────────┘  │
    │                                                              │
    │  Regla: usa float64 siempre, excepto razones muy específicas.│
    └──────────────────────────────────────────────────────────────┘
```

### 1.1 ¿Por qué float64 por defecto?

```go
package main

import "fmt"

func main() {
    // Go infiere float64 para literales con punto decimal:
    x := 3.14
    fmt.Printf("Type: %T, Value: %f\n", x, x)
    // Type: float64, Value: 3.140000

    // Para float32 necesitas conversión explícita:
    var y float32 = 3.14
    fmt.Printf("Type: %T, Value: %f\n", y, y)

    // ¿Por qué float64 por defecto?
    //
    // 1. PRECISIÓN: float32 solo tiene ~7 dígitos significativos.
    //    float32(123456789.0) → 123456792.0 (¡ya pierde precisión!)
    //    float64(123456789.0) → 123456789.0 (correcto)
    //
    // 2. RENDIMIENTO: en CPUs modernos de 64-bit, float64 es igual
    //    de rápido que float32 (las FPU trabajan en 64-bit internamente).
    //
    // 3. COMPATIBILIDAD: math package usa float64 en todas sus funciones.
    //    math.Sqrt(float64) → float64
    //    math.Sin(float64) → float64
    //    Si usas float32, necesitas conversiones constantes.
    //
    // 4. CONSISTENCIA: JSON, CSV, bases de datos — todos producen float64
    //    al deserializar.
}
```

### 1.2 ¿Cuándo usar float32?

```bash
# float32 solo se justifica en estos casos:

# 1. Protocolos/formatos que requieren 32 bits:
#    - OpenGL/Vulkan (coordenadas 3D)
#    - Algunos formatos binarios de sensores
#    - gRPC con campos float

# 2. Ahorro masivo de memoria:
#    Si tienes un slice de 10 millones de valores:
#    []float32 = 40 MB
#    []float64 = 80 MB
#    Pero esto es raro en SysAdmin.

# 3. NUNCA para cálculos intermedios:
#    Incluso si el resultado final es float32,
#    calcula en float64 y convierte al final.
```

---

## 2. Literales de punto flotante

```go
package main

import "fmt"

func main() {
    // Notación decimal:
    a := 3.14
    b := .5        // 0.5 (el 0 antes del punto es opcional)
    c := 42.       // 42.0 (el 0 después del punto es opcional)

    // Notación científica:
    d := 6.022e23    // 6.022 × 10^23 (Avogadro)
    e := 1.6e-19     // 1.6 × 10^-19 (carga del electrón)
    f := 1E6         // 1000000.0 (no necesita punto decimal)

    // Con separadores (Go 1.13+):
    g := 1_000_000.50
    h := 6.022_140_76e23

    // Hexadecimal flotante (Go 1.13+, raro):
    i := 0x1.fp4     // 1.9375 × 2^4 = 31.0

    fmt.Println(a, b, c, d, e, f, g, h, i)
}
```

---

## 3. Representación IEEE 754 — por qué los floats "mienten"

Entender IEEE 754 no es académico — explica bugs reales que verás
como SysAdmin al comparar métricas, calcular porcentajes o agregar
valores.

### 3.1 El problema fundamental

```go
package main

import "fmt"

func main() {
    // El ejemplo clásico:
    fmt.Println(0.1 + 0.2)
    // 0.30000000000000004  ← ¡NO es 0.3!

    // ¿Por qué?
    // 0.1 en base 10 = 0.0001100110011... (periódico) en base 2.
    // Es como 1/3 = 0.333... en base 10: no se puede representar
    // con un número finito de dígitos.

    // Más ejemplos:
    fmt.Println(0.1 + 0.2 == 0.3)  // false
    fmt.Println(1.0 - 0.9)         // 0.09999999999999998 (no 0.1)

    // Acumulación de errores:
    sum := 0.0
    for i := 0; i < 1000; i++ {
        sum += 0.1
    }
    fmt.Println(sum)  // 99.9999999999986 (no 100.0)
}
```

### 3.2 Visualización de la representación binaria

```go
package main

import (
    "fmt"
    "math"
)

func main() {
    // Ver la representación binaria de un float64:
    x := 0.1
    bits := math.Float64bits(x)
    fmt.Printf("0.1 = %064b\n", bits)
    // 0.1 = 0011111110111001100110011001100110011001100110011001100110011010
    //        S EEEEEEEEEEE MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
    //        ↑ exponente    mantisa (la parte periódica truncada)

    // Reconstruir desde bits:
    y := math.Float64frombits(bits)
    fmt.Printf("Reconstructed: %.20f\n", y)
    // 0.10000000000000000555

    // Valores exactamente representables:
    fmt.Printf("0.5   = %.20f\n", 0.5)    // 0.50000000000000000000 (exacto)
    fmt.Printf("0.25  = %.20f\n", 0.25)   // 0.25000000000000000000 (exacto)
    fmt.Printf("0.125 = %.20f\n", 0.125)  // 0.12500000000000000000 (exacto)
    // Potencias de 2 fraccionarias son exactas: 1/2, 1/4, 1/8, 1/16...
    // Todo lo demás es una aproximación.
}
```

### 3.3 Consecuencias prácticas para SysAdmin

```bash
# 1. NUNCA compares floats con ==
#    if cpuUsage == 100.0  →  INCORRECTO
#    Puede ser 99.9999999 o 100.0000001

# 2. Métricas de Prometheus/Grafana usan float64
#    Un counter que llega a 2^53 (9 petabytes) pierde precisión
#    de entero. Prometheus lo documenta como limitación.

# 3. Timestamps como float (Python time.time())
#    Python: time.time() → 1702656000.123456
#    Solo ~15 dígitos de precisión total.
#    Con epoch desde 1970, ya usamos 10 dígitos para los segundos.
#    Solo quedan ~5-6 dígitos para microsegundos.
#    → Go usa int64 nanosegundos, que es más preciso.

# 4. Formateo de JSON
#    JSON no tiene tipos enteros — todo es "number" (float64).
#    Un ID int64 = 9007199254740993 se pierde como float64 en JSON.
#    → Serializar IDs grandes como strings en JSON.
```

---

## 4. Comparar floats correctamente

```go
package main

import (
    "fmt"
    "math"
)

// AlmostEqual compara dos float64 con una tolerancia (epsilon).
func AlmostEqual(a, b, epsilon float64) bool {
    diff := math.Abs(a - b)

    // Si ambos son exactamente iguales (incluye ±0 e infinitos):
    if a == b {
        return true
    }

    // Comparación relativa para valores grandes,
    // comparación absoluta para valores cercanos a cero:
    if a == 0 || b == 0 || diff < math.SmallestNonzeroFloat64 {
        return diff < epsilon
    }

    return diff/(math.Abs(a)+math.Abs(b)) < epsilon
}

func main() {
    a := 0.1 + 0.2
    b := 0.3

    // MAL:
    fmt.Println(a == b)  // false

    // BIEN:
    fmt.Println(AlmostEqual(a, b, 1e-9))  // true

    // Para la mayoría de casos SysAdmin, basta con:
    const epsilon = 1e-9
    if math.Abs(a-b) < epsilon {
        fmt.Println("approximately equal")
    }

    // Porcentajes de uso (0-100):
    cpuUsage := 99.9999999
    threshold := 100.0
    if cpuUsage >= threshold-0.01 {
        fmt.Println("CPU at 100%!")
    }
}
```

---

## 5. Valores especiales: Inf, -Inf, NaN

IEEE 754 define tres valores especiales que representan resultados
de operaciones inválidas o fuera de rango.

```go
package main

import (
    "fmt"
    "math"
)

func main() {
    // +Inf (infinito positivo):
    posInf := math.Inf(1)
    fmt.Println(posInf)        // +Inf
    fmt.Println(1.0 / 0.0)    // +Inf (no es panic en floats)

    // -Inf (infinito negativo):
    negInf := math.Inf(-1)
    fmt.Println(negInf)        // -Inf
    fmt.Println(-1.0 / 0.0)   // -Inf

    // NaN (Not a Number):
    nan := math.NaN()
    fmt.Println(nan)           // NaN
    fmt.Println(0.0 / 0.0)    // NaN
    fmt.Println(math.Sqrt(-1)) // NaN
    fmt.Println(math.Log(-1))  // NaN

    // ⚠️ NaN tiene un comportamiento ÚNICO:
    fmt.Println(nan == nan)     // false (¡NaN no es igual a sí mismo!)
    fmt.Println(nan != nan)     // true
    fmt.Println(nan < 0)        // false
    fmt.Println(nan > 0)        // false
    fmt.Println(nan == 0)       // false

    // Detectar NaN:
    fmt.Println(math.IsNaN(nan))     // true
    fmt.Println(math.IsNaN(42.0))    // false

    // Detectar Inf:
    fmt.Println(math.IsInf(posInf, 1))   // true (+Inf)
    fmt.Println(math.IsInf(negInf, -1))  // true (-Inf)
    fmt.Println(math.IsInf(posInf, 0))   // true (cualquier Inf)

    // Operaciones con Inf:
    fmt.Println(posInf + 1)        // +Inf
    fmt.Println(posInf + posInf)   // +Inf
    fmt.Println(posInf - posInf)   // NaN (indeterminado)
    fmt.Println(posInf * 0)        // NaN
    fmt.Println(1 / posInf)        // 0

    // ⚠️ Diferencia con enteros:
    // fmt.Println(1 / 0)   ← PANIC: integer divide by zero
    // fmt.Println(1.0/0.0) ← +Inf (no hay panic en floats)
}
```

### 5.1 NaN en métricas y datos — un problema real

```go
// Escenario SysAdmin: calcular promedio de latencias.
// Algunas requests devuelven NaN (timeout, error, sin datos).

func average(values []float64) float64 {
    // ⚠️ Si hay un NaN en values, el resultado es NaN:
    sum := 0.0
    for _, v := range values {
        sum += v  // si v es NaN, sum se convierte en NaN para siempre
    }
    return sum / float64(len(values))  // NaN
}

// Solución: filtrar NaN:
func safeAverage(values []float64) (float64, int) {
    sum := 0.0
    count := 0
    for _, v := range values {
        if !math.IsNaN(v) && !math.IsInf(v, 0) {
            sum += v
            count++
        }
    }
    if count == 0 {
        return 0, 0
    }
    return sum / float64(count), count
}

// NaN en JSON:
// JSON no soporta NaN ni Inf.
// encoding/json devuelve error si intentas serializar NaN.
// Solución: convertir NaN a null o a un valor sentinel antes de serializar.
```

---

## 6. Operaciones aritméticas con floats

```go
package main

import (
    "fmt"
    "math"
)

func main() {
    a, b := 17.0, 5.0

    // Aritméticas básicas:
    fmt.Println(a + b)   // 22
    fmt.Println(a - b)   // 12
    fmt.Println(a * b)   // 85
    fmt.Println(a / b)   // 3.4 (no trunca como con enteros)
    // fmt.Println(a % b) // ✗ NO compila — % no existe para floats

    // Para módulo de floats, usar math.Mod:
    fmt.Println(math.Mod(17.0, 5.0))   // 2
    fmt.Println(math.Remainder(17.0, 5.0)) // 2 (IEEE remainder)

    // Funciones de math:
    fmt.Println(math.Sqrt(144))    // 12
    fmt.Println(math.Pow(2, 10))   // 1024
    fmt.Println(math.Abs(-42.5))   // 42.5
    fmt.Println(math.Ceil(3.2))    // 4
    fmt.Println(math.Floor(3.8))   // 3
    fmt.Println(math.Round(3.5))   // 4
    fmt.Println(math.Round(4.5))   // 5 (Go usa "round half to even"
                                   //    solo para math.RoundToEven)

    // Trigonométricas:
    fmt.Println(math.Sin(math.Pi / 2))  // 1
    fmt.Println(math.Cos(0))            // 1
    fmt.Println(math.Atan2(1, 1))       // 0.7853... (π/4)

    // Logaritmos y exponenciales:
    fmt.Println(math.Log(math.E))    // 1 (ln)
    fmt.Println(math.Log2(1024))     // 10
    fmt.Println(math.Log10(1000))    // 3
    fmt.Println(math.Exp(1))         // 2.7182... (e^1)

    // Min/Max:
    fmt.Println(math.Max(3.14, 2.72))  // 3.14
    fmt.Println(math.Min(3.14, 2.72))  // 2.72
    // Desde Go 1.21, también existen los built-ins:
    fmt.Println(max(3.14, 2.72))       // 3.14
    fmt.Println(min(3.14, 2.72))       // 2.72
}
```

---

## 7. Conversiones con floats

```go
package main

import "fmt"

func main() {
    // Entero a float (seguro si el entero no es demasiado grande):
    n := 42
    f := float64(n)   // 42.0
    fmt.Println(f)

    // Float a entero (TRUNCA hacia cero, no redondea):
    x := 3.7
    i := int(x)        // 3 (trunca, NO redondea)
    fmt.Println(i)

    x = -3.7
    i = int(x)         // -3 (trunca hacia cero)
    fmt.Println(i)

    // Si quieres redondear:
    import "math"
    i = int(math.Round(3.7))   // 4
    i = int(math.Round(-3.7))  // -4
    i = int(math.Floor(3.7))   // 3
    i = int(math.Ceil(3.2))    // 4

    // ⚠️ Pérdida de precisión con enteros grandes:
    big := int64(9007199254740993)  // 2^53 + 1
    f64 := float64(big)
    back := int64(f64)
    fmt.Println(big)   // 9007199254740993
    fmt.Println(back)  // 9007199254740992 (¡perdió 1!)
    // float64 tiene 52 bits de mantisa → 2^53 enteros exactos.
    // Más allá de 2^53, no todos los enteros son representables.

    // float32 a float64 (seguro):
    var f32 float32 = 3.14
    var f64_2 float64 = float64(f32)
    fmt.Printf("%.20f\n", f64_2) // 3.14000010490417480469
    // ¡La imprecisión de float32 se hace visible al convertir a float64!

    // float64 a float32 (puede perder precisión):
    var precise float64 = 3.141592653589793
    var imprecise float32 = float32(precise)
    fmt.Printf("float64: %.15f\n", precise)   // 3.141592653589793
    fmt.Printf("float32: %.15f\n", imprecise) // 3.141592741012573
    //                                                    ^^^^^^^^^ diferente
}
```

---

## 8. Formateo de floats con fmt

```go
package main

import "fmt"

func main() {
    x := 3.141592653589793
    big := 123456789.123456
    small := 0.000000123

    // %f — formato decimal (default 6 decimales):
    fmt.Printf("%f\n", x)        // 3.141593

    // %.Nf — N decimales:
    fmt.Printf("%.2f\n", x)      // 3.14
    fmt.Printf("%.4f\n", x)      // 3.1416
    fmt.Printf("%.10f\n", x)     // 3.1415926536

    // %e — notación científica:
    fmt.Printf("%e\n", big)      // 1.234568e+08
    fmt.Printf("%e\n", small)    // 1.230000e-07
    fmt.Printf("%.2e\n", big)    // 1.23e+08

    // %E — notación científica con E mayúscula:
    fmt.Printf("%E\n", big)      // 1.234568E+08

    // %g — formato compacto (elige %e o %f automáticamente):
    fmt.Printf("%g\n", big)      // 1.23456789123456e+08
    fmt.Printf("%g\n", x)        // 3.141592653589793
    fmt.Printf("%g\n", small)    // 1.23e-07
    fmt.Printf("%.4g\n", x)     // 3.142 (4 dígitos significativos)

    // %G — como %g pero con E mayúscula:
    fmt.Printf("%G\n", big)      // 1.23456789123456E+08

    // Con padding y alineamiento:
    fmt.Printf("%10.2f\n", x)    //       3.14  (ancho 10, 2 decimales)
    fmt.Printf("%-10.2f\n", x)   // 3.14        (alineado a izquierda)
    fmt.Printf("%010.2f\n", x)   // 0000003.14  (padding con ceros)
    fmt.Printf("%+.2f\n", x)     // +3.14       (siempre con signo)

    // Tipo:
    fmt.Printf("%T\n", x)        // float64
}
```

### 8.1 strconv para floats

```go
import "strconv"

// String a float:
f, err := strconv.ParseFloat("3.14", 64)     // string → float64
f32, err := strconv.ParseFloat("3.14", 32)   // string → float32-precision

// Float a string:
s := strconv.FormatFloat(3.14, 'f', 2, 64)   // "3.14"
// Argumentos: value, format, precision, bitSize
//   format: 'f' decimal, 'e' científica, 'g' compacta
//   precision: -1 = mínimo necesario

s = strconv.FormatFloat(3.14, 'f', -1, 64)   // "3.14"
s = strconv.FormatFloat(3.14, 'e', 2, 64)    // "3.14e+00"
s = strconv.FormatFloat(3.14, 'g', -1, 64)   // "3.14"

// Caso especial: máxima fidelidad (round-trip):
original := 0.1
s = strconv.FormatFloat(original, 'g', -1, 64)
recovered, _ := strconv.ParseFloat(s, 64)
fmt.Println(original == recovered)  // true
// Con -1, FormatFloat produce los dígitos mínimos para
// reconstruir el valor exacto con ParseFloat.
```

---

## 9. Paquete math — referencia para SysAdmin

### 9.1 Constantes

```go
import "math"

// Constantes fundamentales:
math.Pi         // 3.141592653589793
math.E          // 2.718281828459045
math.Phi        // 1.618033988749895 (golden ratio)
math.Ln2        // 0.6931471805599453
math.Ln10       // 2.302585092994046
math.Log2E      // 1.4426950408889634
math.Log10E     // 0.4342944819032518
math.Sqrt2      // 1.4142135623730951
math.SqrtPi     // 1.7724538509055159
math.SqrtE      // 1.6487212707001282

// Límites de float:
math.MaxFloat32         // 3.4028234663852886e+38
math.SmallestNonzeroFloat32  // 1.401298464324817e-45
math.MaxFloat64         // 1.7976931348623157e+308
math.SmallestNonzeroFloat64  // 5e-324
```

### 9.2 Funciones más útiles para SysAdmin

```go
import "math"

// --- Redondeo ---
math.Floor(3.7)      // 3   (hacia abajo)
math.Ceil(3.2)       // 4   (hacia arriba)
math.Round(3.5)      // 4   (al más cercano)
math.Trunc(3.7)      // 3   (truncar decimales)
math.RoundToEven(3.5) // 4  (banker's rounding: .5 al par más cercano)
math.RoundToEven(4.5) // 4  (4 es par, 5 es impar → 4)

// --- Valor absoluto ---
math.Abs(-42.5)      // 42.5

// --- Potencias y raíces ---
math.Pow(2, 10)      // 1024
math.Pow10(3)        // 1000 (10^3, más rápido que Pow)
math.Sqrt(144)       // 12
math.Cbrt(27)        // 3 (raíz cúbica)

// --- Logaritmos ---
math.Log(math.E)     // 1   (ln — logaritmo natural)
math.Log2(1024)      // 10  (log base 2)
math.Log10(1000)     // 3   (log base 10)
math.Log1p(0.001)    // 0.0009995... (ln(1+x), preciso para x pequeño)

// --- Exponenciales ---
math.Exp(1)          // 2.718... (e^x)
math.Exp2(10)        // 1024 (2^x)
math.Expm1(0.001)    // 0.001000500... (e^x - 1, preciso para x pequeño)

// --- Min/Max ---
math.Max(a, b)       // mayor
math.Min(a, b)       // menor
// ⚠️ math.Max/Min NO manejan NaN como esperarías:
math.Max(math.NaN(), 42)  // NaN (¡no 42!)

// --- Verificación ---
math.IsNaN(x)        // true si x es NaN
math.IsInf(x, 0)     // true si x es +Inf o -Inf
math.IsInf(x, 1)     // true si x es +Inf
math.IsInf(x, -1)    // true si x es -Inf
math.Signbit(x)      // true si x es negativo (incluye -0)

// --- Copiar signo ---
math.Copysign(5, -1) // -5 (copia el signo del segundo al primero)

// --- Rango de float ---
math.Nextafter(1.0, 2.0)  // 1.0000000000000002 (siguiente float representable)
```

---

## 10. Floats en contextos de SysAdmin/DevOps

### 10.1 Porcentajes de uso de recursos

```go
package main

import (
    "fmt"
    "math"
)

// CPUPercent calcula el porcentaje de uso de CPU entre dos lecturas
// de /proc/stat.
func CPUPercent(idle1, total1, idle2, total2 uint64) float64 {
    idleDelta := float64(idle2 - idle1)
    totalDelta := float64(total2 - total1)

    if totalDelta == 0 {
        return 0
    }
    return (1.0 - idleDelta/totalDelta) * 100.0
}

// DiskUsagePercent calcula el porcentaje de disco usado.
func DiskUsagePercent(used, total uint64) float64 {
    if total == 0 {
        return 0
    }
    return float64(used) / float64(total) * 100.0
}

func main() {
    // CPU:
    cpu := CPUPercent(1000, 5000, 1200, 5500)
    fmt.Printf("CPU: %.1f%%\n", cpu)  // CPU: 60.0%

    // Disk:
    disk := DiskUsagePercent(750_000_000_000, 1_000_000_000_000)
    fmt.Printf("Disk: %.1f%%\n", disk)  // Disk: 75.0%

    // ⚠️ Clamping (limitar al rango 0-100):
    cpu = math.Max(0, math.Min(100, cpu))
}
```

### 10.2 Latencias y tiempos de respuesta

```go
import (
    "math"
    "sort"
    "time"
)

// Calcular percentiles de latencias:
func percentile(latencies []float64, p float64) float64 {
    if len(latencies) == 0 {
        return 0
    }

    sorted := make([]float64, len(latencies))
    copy(sorted, latencies)
    sort.Float64s(sorted)

    index := p / 100.0 * float64(len(sorted)-1)
    lower := int(math.Floor(index))
    upper := int(math.Ceil(index))

    if lower == upper {
        return sorted[lower]
    }

    fraction := index - float64(lower)
    return sorted[lower]*(1-fraction) + sorted[upper]*fraction
}

func main() {
    // Latencias en milisegundos:
    latencies := []float64{
        1.2, 2.3, 1.5, 4.2, 1.8, 2.1, 50.3, 1.4, 3.2, 1.9,
    }

    fmt.Printf("p50: %.1f ms\n", percentile(latencies, 50))   // ~2.0 ms
    fmt.Printf("p95: %.1f ms\n", percentile(latencies, 95))   // ~32.7 ms
    fmt.Printf("p99: %.1f ms\n", percentile(latencies, 99))   // ~46.8 ms

    // Convertir time.Duration a milliseconds como float:
    d := 2*time.Second + 500*time.Millisecond
    ms := float64(d.Nanoseconds()) / 1e6
    fmt.Printf("%.1f ms\n", ms)  // 2500.0 ms

    // O más limpio:
    ms = float64(d) / float64(time.Millisecond)
    fmt.Printf("%.1f ms\n", ms)  // 2500.0 ms
}
```

### 10.3 Rate calculations (bytes/s, requests/s)

```go
// Calcular throughput entre dos puntos de datos:
func throughput(bytes1, bytes2 uint64, interval time.Duration) float64 {
    delta := float64(bytes2 - bytes1)
    seconds := interval.Seconds()
    if seconds == 0 {
        return 0
    }
    return delta / seconds  // bytes per second
}

func formatRate(bytesPerSec float64) string {
    const (
        KBps = 1024.0
        MBps = KBps * 1024
        GBps = MBps * 1024
    )
    switch {
    case bytesPerSec >= GBps:
        return fmt.Sprintf("%.2f GB/s", bytesPerSec/GBps)
    case bytesPerSec >= MBps:
        return fmt.Sprintf("%.2f MB/s", bytesPerSec/MBps)
    case bytesPerSec >= KBps:
        return fmt.Sprintf("%.2f KB/s", bytesPerSec/KBps)
    default:
        return fmt.Sprintf("%.0f B/s", bytesPerSec)
    }
}

func main() {
    rate := throughput(1_000_000_000, 1_500_000_000, 5*time.Second)
    fmt.Println(formatRate(rate))  // 95.37 MB/s
}
```

### 10.4 Load average

```go
// /proc/loadavg contiene load averages como floats:
// 0.52 0.58 0.59 2/347 12345

import (
    "fmt"
    "os"
    "strconv"
    "strings"
)

type LoadAvg struct {
    Load1  float64  // último minuto
    Load5  float64  // últimos 5 minutos
    Load15 float64  // últimos 15 minutos
}

func ReadLoadAvg() (*LoadAvg, error) {
    data, err := os.ReadFile("/proc/loadavg")
    if err != nil {
        return nil, err
    }

    fields := strings.Fields(string(data))
    if len(fields) < 3 {
        return nil, fmt.Errorf("unexpected format: %s", data)
    }

    load := &LoadAvg{}
    load.Load1, err = strconv.ParseFloat(fields[0], 64)
    if err != nil {
        return nil, err
    }
    load.Load5, err = strconv.ParseFloat(fields[1], 64)
    if err != nil {
        return nil, err
    }
    load.Load15, err = strconv.ParseFloat(fields[2], 64)
    if err != nil {
        return nil, err
    }

    return load, nil
}

func main() {
    load, err := ReadLoadAvg()
    if err != nil {
        fmt.Fprintf(os.Stderr, "error: %v\n", err)
        os.Exit(1)
    }

    fmt.Printf("Load: %.2f %.2f %.2f\n", load.Load1, load.Load5, load.Load15)

    // Alertar si load > número de CPUs:
    import "runtime"
    numCPU := float64(runtime.NumCPU())
    if load.Load1 > numCPU {
        fmt.Printf("⚠ Load (%.2f) exceeds CPU count (%d)\n",
            load.Load1, runtime.NumCPU())
    }
}
```

---

## 11. Errores comunes con floats en Go

### 11.1 División por cero

```go
// Enteros: PANIC
// fmt.Println(1 / 0)      // panic: runtime error: integer divide by zero

// Floats: NO PANIC, devuelve Inf o NaN
fmt.Println(1.0 / 0.0)   // +Inf
fmt.Println(-1.0 / 0.0)  // -Inf
fmt.Println(0.0 / 0.0)   // NaN

// Esto puede causar problemas silenciosos:
total := 0.0
avg := sum / total  // NaN — no hay error, se propaga silenciosamente

// Siempre verificar el divisor:
if total > 0 {
    avg = sum / total
} else {
    avg = 0
}
```

### 11.2 Acumulación de errores

```go
// MAL — acumular 0.1 mil veces:
sum := 0.0
for i := 0; i < 1000; i++ {
    sum += 0.1
}
fmt.Println(sum)  // 99.9999999999986 (no 100.0)

// MEJOR — Kahan summation (compensated summation):
func KahanSum(values []float64) float64 {
    sum := 0.0
    compensation := 0.0

    for _, v := range values {
        y := v - compensation
        t := sum + y
        compensation = (t - sum) - y
        sum = t
    }
    return sum
}

// O MEJOR — usar enteros para acumular y convertir al final:
// En vez de sumar 0.1 mil veces:
sumCents := 0  // int, en centavos
for i := 0; i < 1000; i++ {
    sumCents += 10  // 10 centavos = 0.1
}
result := float64(sumCents) / 100.0
fmt.Println(result)  // 100 (exacto)
```

### 11.3 NaN en maps y slices

```go
// NaN como key de map:
m := make(map[float64]string)
m[math.NaN()] = "first"
m[math.NaN()] = "second"

fmt.Println(len(m))  // 2 (¡cada NaN es una key diferente!)
// NaN != NaN, así que cada asignación crea una key nueva.
// Esto es un memory leak si no se controla.

// NaN en sort:
values := []float64{3.0, math.NaN(), 1.0, math.NaN(), 2.0}
sort.Float64s(values)
fmt.Println(values)
// Resultado impredecible — NaN rompe las invariantes de sort.

// Solución: filtrar NaN ANTES de operar con slices/maps.
```

---

## 12. Floats y JSON

```go
import "encoding/json"

type Metrics struct {
    CPUUsage    float64 `json:"cpu_usage"`
    MemoryUsage float64 `json:"memory_usage"`
    DiskIO      float64 `json:"disk_io_mbps"`
}

func main() {
    // Serializar:
    m := Metrics{
        CPUUsage:    45.73,
        MemoryUsage: 82.1,
        DiskIO:      125.456,
    }

    data, err := json.Marshal(m)
    fmt.Println(string(data))
    // {"cpu_usage":45.73,"memory_usage":82.1,"disk_io_mbps":125.456}

    // ⚠️ NaN e Inf NO son válidos en JSON:
    bad := Metrics{CPUUsage: math.NaN()}
    _, err = json.Marshal(bad)
    fmt.Println(err)
    // json: unsupported value: NaN

    // Solución: custom marshaler o limpiar antes de serializar:
    if math.IsNaN(m.CPUUsage) || math.IsInf(m.CPUUsage, 0) {
        m.CPUUsage = 0  // o usar *float64 con nil para "sin dato"
    }

    // ⚠️ Precisión en JSON:
    // JSON no especifica precisión para numbers.
    // Go usa float64 para todos los JSON numbers por defecto.
    // Un JSON con "id": 9007199254740993 pierde precisión.

    // Deserializar number genérico como json.Number:
    var raw map[string]json.Number
    json.Unmarshal([]byte(`{"count": "42"}`), &raw)
    n, _ := raw["count"].Int64()   // preciso para enteros
    f, _ := raw["count"].Float64() // para floats
    _ = n; _ = f
}
```

---

## 13. Tipos complejos: complex64 y complex128

Go tiene soporte nativo para números complejos. Son raramente
usados en SysAdmin pero están en el lenguaje.

```go
package main

import (
    "fmt"
    "math/cmplx"
)

func main() {
    // complex128 = float64 real + float64 imaginaria (16 bytes):
    var c1 complex128 = 3 + 4i
    var c2 complex128 = complex(3, 4)  // equivalente

    // complex64 = float32 real + float32 imaginaria (8 bytes):
    var c3 complex64 = 3 + 4i

    fmt.Println(c1)  // (3+4i)
    fmt.Println(c3)  // (3+4i)

    // Partes real e imaginaria:
    fmt.Println(real(c1))  // 3
    fmt.Println(imag(c1))  // 4

    // Operaciones:
    c4 := c1 + c2         // (6+8i)
    c5 := c1 * c2         // (-7+24i)
    fmt.Println(c4, c5)

    // math/cmplx:
    fmt.Println(cmplx.Abs(c1))    // 5 (módulo: sqrt(3² + 4²))
    fmt.Println(cmplx.Phase(c1))  // 0.9272... (ángulo en radianes)
    fmt.Println(cmplx.Conj(c1))   // (3-4i) (conjugado)
    fmt.Println(cmplx.Sqrt(-1))   // (0+1i) (raíz cuadrada de -1)

    _ = c2
}
```

### 13.1 ¿Cuándo se usan los complejos?

```bash
# En SysAdmin/DevOps: CASI NUNCA.

# Usos reales de números complejos:
# 1. Procesamiento de señales (FFT)
# 2. Análisis de circuitos eléctricos
# 3. Simulaciones físicas
# 4. Criptografía (curvas elípticas, internamente)
# 5. Fractales (Mandelbrot set como ejemplo clásico)

# Para SysAdmin lo relevante es SABER QUE EXISTEN
# por si encuentras un tipo complex128 en alguna API
# o librería de procesamiento de datos.
```

---

## 14. math/rand — números aleatorios

```go
package main

import (
    "fmt"
    "math/rand/v2"
)

func main() {
    // Go 1.22+ con math/rand/v2 (NO necesita seed):

    // Entero aleatorio:
    n := rand.IntN(100)     // [0, 100)
    fmt.Println(n)

    // Float aleatorio:
    f := rand.Float64()     // [0.0, 1.0)
    fmt.Println(f)

    // Float en un rango:
    min, max := 10.0, 50.0
    rangeF := min + rand.Float64()*(max-min)
    fmt.Printf("Random [%.0f, %.0f): %.2f\n", min, max, rangeF)

    // ⚠️ math/rand NO es criptográficamente seguro.
    // Para tokens, passwords, claves: usar crypto/rand.
}
```

```go
// Para uso criptográfico:
import "crypto/rand"

// Generar bytes aleatorios seguros:
token := make([]byte, 32)
_, err := crypto_rand.Read(token)
if err != nil {
    panic(err)
}
fmt.Printf("Token: %x\n", token)
```

---

## 15. Comparación con C y Rust

```
    ┌──────────────────┬──────────────┬──────────────┬──────────────┐
    │ Concepto         │ Go           │ C            │ Rust         │
    ├──────────────────┼──────────────┼──────────────┼──────────────┤
    │ Simple prec.     │ float32      │ float        │ f32          │
    │ Doble prec.      │ float64      │ double       │ f64          │
    │ Default literal  │ float64      │ double       │ f64          │
    │ Complejo         │ complex128   │ _Complex     │ — (crate)    │
    │ Módulo float     │ math.Mod()   │ fmod()       │ f64::rem()   │
    │ NaN check        │ math.IsNaN() │ isnan()      │ f64::is_nan()│
    │ Inf check        │ math.IsInf() │ isinf()      │ f64::is_inf()│
    │ División por 0   │ ±Inf/NaN     │ UB           │ ±Inf/NaN     │
    │ Conv int→float   │ explícita    │ implícita    │ explícita    │
    │ Conv float→int   │ trunca       │ trunca (UB)  │ saturating   │
    └──────────────────┴──────────────┴──────────────┴──────────────┘
```

```bash
# Diferencia notable: Go trunca float→int, Rust satura.
# Go:   int(math.Inf(1)) → MinInt64 o MaxInt64 (implementation-defined)
# Rust: f64::INFINITY as i64 → i64::MAX (saturating cast desde Rust 1.45)
# C:    (int)INFINITY → undefined behavior
```

---

## 16. Tabla de errores comunes

| Error / Síntoma | Causa | Solución |
|---|---|---|
| `0.1 + 0.2 != 0.3` | Representación IEEE 754 imprecisa | Comparar con tolerancia: `math.Abs(a-b) < 1e-9` |
| NaN se propaga por todos los cálculos | Operación inválida no detectada (0/0, sqrt(-1)) | Verificar NaN con `math.IsNaN()` en datos de entrada |
| `json: unsupported value: NaN` | NaN/Inf no son válidos en JSON | Filtrar NaN/Inf antes de serializar, o usar puntero nil |
| Resultado es `+Inf` inesperadamente | División por zero float (no causa panic) | Verificar divisor antes de dividir |
| `cannot use x (float32) as float64` | Tipos diferentes, no se convierten implícitamente | Conversión explícita: `float64(x)` |
| Pérdida de precisión con enteros grandes en float64 | float64 solo tiene 52 bits de mantisa (~15 dígitos) | Usar int64 para enteros, o json.Number |
| Resultados diferentes entre float32 y float64 | float32 tiene ~7 dígitos de precisión vs ~15 | Usar float64 siempre, calcular en float64 |
| `sum += 0.1` da 99.999... después de 1000 iteraciones | Acumulación de error de redondeo | Kahan summation, o acumular en enteros |
| NaN como key de map crea entries infinitas | NaN != NaN, cada inserción crea nueva key | No usar floats como keys de map, o filtrar NaN |
| `math.Max(NaN, 42)` devuelve NaN | math.Max no filtra NaN | Verificar NaN antes de llamar math.Max |
| `int(3.7)` da 3 en vez de 4 | float→int trunca hacia cero, no redondea | Usar `int(math.Round(x))` para redondear |
| `% not defined for float64` | Operador `%` no existe para floats | Usar `math.Mod(a, b)` |

---

## 17. Ejercicios

### Ejercicio 1 — Precisión de float32 vs float64

```
Crea un programa que:
1. Asigne 123456789.0 a float32 y float64
2. Imprima ambos con %.0f
3. Asigne 0.1 + 0.2 a float32 y float64
4. Imprima ambos con %.20f
5. Compara los resultados

Predicción: ¿float32(123456789.0) mostrará 123456789?
¿Cuántos dígitos de 0.1+0.2 serán iguales entre float32 y float64?
```

### Ejercicio 2 — Comparar floats

```
Implementa una función AlmostEqual(a, b, epsilon float64) bool.
Prueba con:
- AlmostEqual(0.1+0.2, 0.3, 1e-9) → true
- AlmostEqual(1000000.1+0.2, 1000000.3, 1e-9) → true
- AlmostEqual(1.0, 2.0, 1e-9) → false
- AlmostEqual(0.0, 0.0, 1e-9) → true

Predicción: ¿un epsilon de 1e-9 funciona tanto para valores
cercanos a 0 como para valores cercanos a 1 millón?
```

### Ejercicio 3 — Valores especiales

```
Crea un programa que produzca y detecte:
1. +Inf (división positiva por zero)
2. -Inf (división negativa por zero)
3. NaN (0.0 / 0.0)

Para cada uno:
- Imprímelo
- Verifica con math.IsInf / math.IsNaN
- Suma 1 — ¿qué resultado da?
- Compara consigo mismo — ¿es igual?

Predicción: ¿Inf + 1 == Inf? ¿NaN + 1 == NaN?
¿NaN == NaN?
```

### Ejercicio 4 — Porcentaje de uso de CPU

```
Crea un programa que:
1. Lea /proc/stat dos veces con 1 segundo de intervalo
2. Calcule el porcentaje de uso de CPU
3. Muestre el resultado con 1 decimal

Fórmula: CPU% = (1 - idle_delta/total_delta) × 100

Los campos de /proc/stat (primera línea):
cpu  user nice system idle iowait irq softirq steal

Predicción: ¿qué tipo de dato usarás para los contadores
de /proc/stat? ¿Y para el porcentaje resultante?
```

### Ejercicio 5 — Percentiles de latencia

```
Dado un slice de 1000 latencias (float64, en milisegundos)
generadas con rand.Float64() * 100 + rand.ExpFloat64() * 10:

1. Calcula p50, p90, p95, p99, p99.9
2. Calcula media y desviación estándar
3. Formatea la salida como:
   p50=2.3ms p90=15.2ms p95=42.1ms p99=89.3ms p99.9=142.5ms

Predicción: ¿p99 será mucho más alto que p95?
¿Por qué las latencias no siguen una distribución normal?
```

### Ejercicio 6 — Formateo de rates

```
Crea funciones:
- FormatBytesRate(bytesPerSec float64) string → "125.3 MB/s"
- FormatBitsRate(bitsPerSec float64) string → "1.0 Gbps"

Prueba con:
- 1024 bytes/s → "1.0 KB/s"
- 1_500_000_000 bits/s → "1.5 Gbps"
- 0.5 bytes/s → "0.5 B/s"

Predicción: ¿usarás base 1000 (SI) o base 1024 (IEC)
para bits/s? ¿Y para bytes/s?
```

### Ejercicio 7 — NaN-safe statistics

```
Crea un paquete stats con funciones NaN-safe:
- Mean(values []float64) float64
- StdDev(values []float64) float64
- Min(values []float64) float64
- Max(values []float64) float64

Todas deben ignorar NaN e Inf.
Prueba con un slice que contenga NaN, Inf y valores normales.

Predicción: ¿qué devuelve Mean si TODOS los valores son NaN?
```

### Ejercicio 8 — JSON con floats

```
Crea un struct ServerMetrics con campos float64:
- CPUUsage, MemoryUsage, DiskIO, NetworkBandwidth

1. Serializa a JSON con valores normales
2. Intenta serializar con CPUUsage = NaN → ¿qué pasa?
3. Crea un MarshalJSON custom que convierta NaN a null
4. Deserializa el JSON resultante con null → ¿qué valor tiene el campo?

Predicción: ¿encoding/json pone 0 o NaN cuando el JSON tiene null
para un campo float64?
```

### Ejercicio 9 — Acumulación de error

```
Compara tres formas de sumar 0.1 un millón de veces:
1. Acumulación simple: sum += 0.1
2. Kahan summation
3. Acumular en enteros: sumCents += 10; result = sumCents/100.0

Imprime el resultado con 20 decimales para cada método.

Predicción: ¿cuántos dígitos correctos tiene cada método?
¿Cuál da exactamente 100000.0?
```

### Ejercicio 10 — Monitor de load average

```
Crea un programa que:
1. Lea /proc/loadavg cada 5 segundos
2. Almacene los últimos 12 valores de load1 (1 minuto de historia)
3. Calcule la tendencia (subiendo, bajando, estable)
4. Alerte si load1 > numCPU (usando runtime.NumCPU())
5. Muestre un "sparkline" ASCII: ▁▂▃▄▅▆▇█ basado en el valor

Predicción: ¿cómo determinas si la tendencia es "subiendo"?
¿Qué pasa si el load es 0.01 — qué carácter de sparkline usas?
```

---

## 18. Resumen

```
    ┌───────────────────────────────────────────────────────────┐
    │        Flotantes y complejos en Go — Resumen              │
    │                                                           │
    │  TIPOS:                                                   │
    │  ┌─ float32     → 4 bytes, ~7 dígitos (RARO)            │
    │  ├─ float64     → 8 bytes, ~15 dígitos (SIEMPRE ESTE)   │
    │  ├─ complex64   → 8 bytes (float32 + float32)           │
    │  └─ complex128  → 16 bytes (float64 + float64)          │
    │                                                           │
    │  REGLA DE ORO: usa float64 siempre.                      │
    │                                                           │
    │  IEEE 754 — LO QUE DEBES SABER:                          │
    │  ┌─ 0.1 + 0.2 != 0.3 (representación binaria)           │
    │  ├─ NUNCA compares con == → usa tolerancia (epsilon)     │
    │  ├─ NaN != NaN (NaN no es igual a sí mismo)              │
    │  ├─ 1.0/0.0 = +Inf (floats no causan panic)             │
    │  └─ NaN se propaga silenciosamente por los cálculos      │
    │                                                           │
    │  VALORES ESPECIALES:                                      │
    │  ┌─ math.Inf(1)  → +Inf                                 │
    │  ├─ math.Inf(-1) → -Inf                                 │
    │  ├─ math.NaN()   → NaN                                  │
    │  ├─ math.IsNaN() → detectar NaN                         │
    │  └─ math.IsInf() → detectar Inf                         │
    │                                                           │
    │  PARA SYSADMIN:                                           │
    │  ┌─ CPU%: float64 con 1 decimal                          │
    │  ├─ Latencias: float64 en ms, calcular percentiles       │
    │  ├─ Rates: float64 bytes/s → formatear como MB/s         │
    │  ├─ Load average: float64 desde /proc/loadavg            │
    │  └─ JSON: NaN/Inf no son válidos, filtrar antes          │
    │                                                           │
    │  CONVERSIONES:                                            │
    │  ┌─ int → float64: float64(n) (seguro si n < 2^53)      │
    │  ├─ float64 → int: int(f) TRUNCA (usar math.Round)      │
    │  └─ float32 → float64: float64(f) (seguro pero impreciso)│
    └───────────────────────────────────────────────────────────┘
```
