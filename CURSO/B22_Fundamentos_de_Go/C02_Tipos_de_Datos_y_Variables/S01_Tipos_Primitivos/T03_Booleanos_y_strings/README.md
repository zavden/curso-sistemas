# T03 — Booleanos y strings

## 1. El tipo bool

Go tiene un tipo booleano `bool` con dos valores posibles:
`true` y `false`. No hay conversión implícita entre bool y enteros.

```go
package main

import "fmt"

func main() {
    var a bool = true
    var b bool = false
    c := true          // tipo inferido: bool

    fmt.Println(a, b, c)       // true false true
    fmt.Printf("%T\n", a)     // bool
    fmt.Printf("%v\n", a)     // true
    fmt.Printf("%t\n", a)     // true (verb específico de bool)

    // Zero value de bool es false:
    var d bool
    fmt.Println(d)  // false
}
```

### 1.1 Go no convierte entre bool e int

```go
// En C/Python: 0 es false, cualquier otro número es true.
// En Go: NO HAY conversión implícita.

// ✗ NO compila:
// var n int = 1
// if n { }                 // cannot use n (int) as bool
// var b bool = 1           // cannot use 1 (int) as bool
// var x int = true         // cannot use true (bool) as int

// ✓ Conversión explícita si la necesitas:
func boolToInt(b bool) int {
    if b {
        return 1
    }
    return 0
}

func intToBool(n int) bool {
    return n != 0
}
```

### 1.2 Operadores lógicos

```go
package main

import "fmt"

func main() {
    a, b := true, false

    // AND lógico:
    fmt.Println(a && b)   // false
    fmt.Println(a && a)   // true

    // OR lógico:
    fmt.Println(a || b)   // true
    fmt.Println(b || b)   // false

    // NOT lógico:
    fmt.Println(!a)       // false
    fmt.Println(!b)       // true

    // Short-circuit evaluation:
    // Go evalúa de izquierda a derecha y se detiene cuando
    // el resultado ya está determinado.

    // Con &&: si el primero es false, no evalúa el segundo.
    // Con ||: si el primero es true, no evalúa el segundo.

    // Esto es IMPORTANTE para evitar panics:
    var s []int
    if len(s) > 0 && s[0] == 42 {
        // Si len(s) es 0, s[0] NO se evalúa → no hay panic
        fmt.Println("found")
    }

    // Sin short-circuit, s[0] causaría index out of range.
}
```

### 1.3 Operadores de comparación (producen bool)

```go
// Todos los operadores de comparación devuelven bool:

a, b := 10, 20
fmt.Println(a == b)    // false    (igual)
fmt.Println(a != b)    // true     (diferente)
fmt.Println(a < b)     // true     (menor que)
fmt.Println(a > b)     // false    (mayor que)
fmt.Println(a <= b)    // true     (menor o igual)
fmt.Println(a >= b)    // false    (mayor o igual)

// Strings se comparan lexicográficamente:
fmt.Println("abc" < "abd")    // true
fmt.Println("abc" == "abc")   // true
fmt.Println("ABC" < "abc")    // true (mayúsculas < minúsculas en ASCII)
```

### 1.4 Bool en SysAdmin

```go
// Uso típico en configuración:
type Config struct {
    Verbose     bool `yaml:"verbose" json:"verbose"`
    DryRun      bool `yaml:"dry_run" json:"dry_run"`
    ForceUpdate bool `yaml:"force_update" json:"force_update"`
    EnableTLS   bool `yaml:"enable_tls" json:"enable_tls"`
    Debug       bool `yaml:"debug" json:"debug"`
}

// Parsear bool desde string (flags, env vars):
import "strconv"

s := os.Getenv("ENABLE_TLS")
enabled, err := strconv.ParseBool(s)
// ParseBool acepta: "1", "t", "T", "TRUE", "true", "True"
//                   "0", "f", "F", "FALSE", "false", "False"
// Cualquier otro valor devuelve error.

// Bool a string:
s = strconv.FormatBool(true)   // "true"
s = strconv.FormatBool(false)  // "false"

// Formato con fmt:
fmt.Sprintf("%t", true)   // "true"
fmt.Sprintf("%v", false)  // "false"
```

---

## 2. El tipo string

El tipo `string` de Go es una **secuencia de bytes inmutable**
con semántica UTF-8.

```
    ┌──────────────────────────────────────────────────────────────┐
    │                    string en Go                              │
    │                                                              │
    │  ┌─────────────────────────────────────────────────────┐     │
    │  │ Un string es:                                       │     │
    │  │                                                     │     │
    │  │ • Una secuencia de BYTES (no caracteres)            │     │
    │  │ • Inmutable (no se puede modificar una vez creado)  │     │
    │  │ • UTF-8 por convención (no forzado por el runtime)  │     │
    │  │ • Puede contener bytes arbitrarios (incluso 0x00)   │     │
    │  └─────────────────────────────────────────────────────┘     │
    │                                                              │
    │  Internamente un string es un header de 16 bytes:            │
    │                                                              │
    │  ┌──────────────┬──────────────┐                             │
    │  │ ptr *byte    │ len int      │                             │
    │  │ (8 bytes)    │ (8 bytes)    │                             │
    │  └──────┬───────┴──────────────┘                             │
    │         │                                                    │
    │         ▼                                                    │
    │  ┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐                   │
    │  │H │e │l │l │o │, │  │ä│ä│ä│W │o │! │                   │
    │  │48│65│6C│6C│6F│2C│20│C3│A4│  │57│6F│21│                   │
    │  └──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┘                   │
    │  byte 0                              byte 12                 │
    │                                                              │
    │  len("Hello, äWo!") = 13 (BYTES, no caracteres)             │
    │  'ä' ocupa 2 bytes en UTF-8 (0xC3 0xA4)                     │
    └──────────────────────────────────────────────────────────────┘
```

### 2.1 String es inmutable

```go
package main

import "fmt"

func main() {
    s := "Hello"

    // ✗ NO se puede modificar un byte del string:
    // s[0] = 'h'  // cannot assign to s[0] (value of type byte)

    // ✓ Se puede crear un string NUEVO:
    s = "hello"          // reasignar la variable
    s2 := "h" + s[1:]   // concatenar partes

    // ¿Por qué inmutable?
    // 1. Thread-safe: múltiples goroutines pueden leer sin locks
    // 2. Seguridad: no se puede alterar una key de map accidentalmente
    // 3. Eficiencia: copiar un string copia solo el header (16 bytes),
    //    no los bytes subyacentes

    // Para modificar caracteres, convertir a []byte:
    b := []byte(s)   // copia los bytes
    b[0] = 'H'
    s = string(b)    // crea nuevo string
    fmt.Println(s)   // "Hello"
}
```

### 2.2 Literales de string

```go
package main

import "fmt"

func main() {
    // String interpretado (comillas dobles):
    s1 := "Hello, World!\n"    // \n es newline
    s2 := "Tab:\there"         // \t es tab
    s3 := "Quote: \""          // \" es comilla escapada
    s4 := "Backslash: \\"     // \\ es backslash
    s5 := "Unicode: \u00e4"   // ä (Unicode escape)
    s6 := "Hex byte: \x41"    // A (hex byte escape)

    // String raw (backticks) — SIN escape sequences:
    s7 := `Hello, World!\n`    // \n es literal, no newline
    s8 := `Line 1
Line 2
Line 3`                        // puede contener newlines reales
    s9 := `C:\Users\admin\docs` // backslashes sin escapar
    s10 := `{"key": "value"}`   // JSON sin escapar comillas

    fmt.Println(s1)  // Hello, World! (con newline)
    fmt.Println(s7)  // Hello, World!\n (literal)
    fmt.Println(s8)  // tres líneas
    fmt.Println(s9)  // C:\Users\admin\docs

    _ = s2; _ = s3; _ = s4; _ = s5; _ = s6; _ = s10
}
```

### 2.3 Raw strings para SysAdmin — uso práctico

```go
// Raw strings son PERFECTOS para:

// 1. Rutas de Windows:
path := `C:\Program Files\MyApp\config.yaml`

// 2. Expresiones regulares:
pattern := `^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})`
// Sin raw string necesitarías: "^(\\d{4})-(\\d{2})-(\\d{2})..."

// 3. JSON embebido:
jsonTemplate := `{
    "apiVersion": "v1",
    "kind": "ConfigMap",
    "metadata": {
        "name": "my-config"
    },
    "data": {
        "config.yaml": "key: value"
    }
}`

// 4. SQL queries:
query := `
    SELECT hostname, cpu_usage, memory_usage
    FROM server_metrics
    WHERE timestamp > NOW() - INTERVAL '5 minutes'
    ORDER BY cpu_usage DESC
    LIMIT 10
`

// 5. Shell commands:
script := `#!/bin/bash
set -euo pipefail
echo "Starting backup..."
rsync -avz /data/ /backup/
echo "Backup complete"
`

// 6. Certificados PEM:
cert := `-----BEGIN CERTIFICATE-----
MIICpDCCAYwCCQDU+pQ4pHgSpDANBgkqhkiG9w0BAQsFADAUMRIwEAYD
...
-----END CERTIFICATE-----`

// 7. YAML templates:
yamlConfig := `
server:
  host: 0.0.0.0
  port: 8080
  tls:
    enabled: true
    cert_file: /etc/ssl/cert.pem
    key_file: /etc/ssl/key.pem
logging:
  level: info
  format: json
`
```

---

## 3. UTF-8 — el encoding de Go

Go usa UTF-8 como encoding por defecto para strings.
Entender UTF-8 es esencial para trabajar correctamente con texto.

```
    ┌──────────────────────────────────────────────────────────────┐
    │                    UTF-8 Encoding                            │
    │                                                              │
    │  Bytes   Rango de code points      Patrón binario            │
    │  ──────  ─────────────────────     ──────────────────────    │
    │  1 byte  U+0000 - U+007F          0xxxxxxx                  │
    │          (ASCII: A-Z, 0-9, etc.)                             │
    │                                                              │
    │  2 bytes U+0080 - U+07FF          110xxxxx 10xxxxxx          │
    │          (ñ, ü, ä, é, ø, etc.)                               │
    │                                                              │
    │  3 bytes U+0800 - U+FFFF          1110xxxx 10xxxxxx 10xxxxxx │
    │          (日, 中, €, ₹, etc.)                                 │
    │                                                              │
    │  4 bytes U+10000 - U+10FFFF       11110xxx 10xx... 10xx...   │
    │          (emojis: 🎉, 🚀, etc.)                              │
    │                                                              │
    │  Ventajas de UTF-8:                                          │
    │  ✓ ASCII es un subconjunto (texto ASCII es válido UTF-8)    │
    │  ✓ Autodetección de límites de carácter                     │
    │  ✓ No hay byte order mark (BOM) necesario                   │
    │  ✓ Usado por Linux, web, Go, Rust, JSON, YAML, TOML        │
    └──────────────────────────────────────────────────────────────┘
```

```go
package main

import "fmt"

func main() {
    s := "Hello, 日本語!"

    // len() devuelve BYTES, no caracteres:
    fmt.Println(len(s))  // 18 (no 11)
    // H=1, e=1, l=1, l=1, o=1, ,=1, " "=1,
    // 日=3, 本=3, 語=3, !=1 → total = 18 bytes

    // Iterar por BYTES:
    for i := 0; i < len(s); i++ {
        fmt.Printf("%d: %02X ", i, s[i])
    }
    fmt.Println()
    // 0:48 1:65 2:6C 3:6C 4:6F 5:2C 6:20
    // 7:E6 8:97 9:A5 10:E6 11:AC 12:AC 13:E8 14:AA 15:9E 16:21
    // Los bytes 7-15 son los 3 caracteres japoneses (3 bytes cada uno)

    // Iterar por RUNES (caracteres Unicode):
    for i, r := range s {
        fmt.Printf("%d: %c (U+%04X)\n", i, r, r)
    }
    // 0: H (U+0048)
    // 1: e (U+0065)
    // ...
    // 7: 日 (U+65E5)   ← index 7 (no 7,8,9 — salta bytes multi-byte)
    // 10: 本 (U+672C)
    // 13: 語 (U+8A9E)
    // 16: ! (U+0021)
}
```

### 3.1 byte vs rune — la distinción fundamental

```go
package main

import (
    "fmt"
    "unicode/utf8"
)

func main() {
    s := "café"

    // len() = bytes:
    fmt.Println(len(s))  // 5 (c=1, a=1, f=1, é=2)

    // utf8.RuneCountInString() = caracteres Unicode:
    fmt.Println(utf8.RuneCountInString(s))  // 4

    // s[i] devuelve un BYTE (uint8):
    fmt.Printf("s[3] = %d (byte)\n", s[3])   // 195 (0xC3, primer byte de é)

    // range devuelve RUNES (int32):
    for i, r := range s {
        fmt.Printf("index=%d, rune=%c, bytes=%d\n", i, r, utf8.RuneLen(r))
    }
    // index=0, rune=c, bytes=1
    // index=1, rune=a, bytes=1
    // index=2, rune=f, bytes=1
    // index=3, rune=é, bytes=2   ← index 3, ocupa 2 bytes
    // (NO hay index=4 — range salta al siguiente rune)

    // Convertir a []rune para acceso por posición de carácter:
    runes := []rune(s)
    fmt.Println(len(runes))  // 4
    fmt.Println(string(runes[3]))  // é (el 4to carácter, no el 4to byte)
}
```

### 3.2 Cuándo usar byte vs rune

```bash
# Usa byte (o []byte) cuando:
# - Trabajas con datos binarios (archivos, protocolos, crypto)
# - Trabajas con texto ASCII puro (logs, config, hostnames)
# - Interactúas con I/O (os.ReadFile devuelve []byte)
# - Performance es crítica (no necesitas decodificar UTF-8)

# Usa rune (o []rune) cuando:
# - Necesitas contar CARACTERES (no bytes)
# - Necesitas acceder al N-ésimo carácter
# - Necesitas manipular texto internacionalizado
# - Necesitas uppercase/lowercase de caracteres no-ASCII

# En la práctica de SysAdmin:
# La mayoría del trabajo es ASCII (hostnames, IPs, paths, logs)
# → byte y string son suficientes.
# Pero si procesas nombres de usuario, comentarios, o datos
# de fuentes internacionales → ten cuidado con multi-byte.
```

---

## 4. Operaciones con strings

### 4.1 Concatenación

```go
package main

import (
    "fmt"
    "strings"
)

func main() {
    // Operador +:
    s := "Hello" + ", " + "World!"
    fmt.Println(s)  // Hello, World!

    // += (ineficiente en loops — crea un string nuevo cada vez):
    result := ""
    for i := 0; i < 5; i++ {
        result += fmt.Sprintf("item-%d ", i)
    }
    fmt.Println(result)

    // fmt.Sprintf (para formateo):
    name := "server-01"
    port := 8080
    url := fmt.Sprintf("http://%s:%d/health", name, port)
    fmt.Println(url)  // http://server-01:8080/health

    // strings.Join (EFICIENTE para concatenar slices):
    parts := []string{"usr", "local", "bin", "myapp"}
    path := strings.Join(parts, "/")
    fmt.Println(path)  // usr/local/bin/myapp

    // strings.Builder (EFICIENTE para concatenación en loop):
    var b strings.Builder
    for i := 0; i < 1000; i++ {
        fmt.Fprintf(&b, "line %d\n", i)
    }
    output := b.String()
    _ = output
}
```

### 4.2 strings.Builder — concatenación eficiente

```go
package main

import (
    "fmt"
    "strings"
)

func main() {
    // strings.Builder es el método RECOMENDADO para construir
    // strings en loops. Minimiza allocaciones de memoria.

    var b strings.Builder

    // Preallocar si conoces el tamaño aproximado:
    b.Grow(1024)  // reserva 1024 bytes

    // Escribir:
    b.WriteString("Hello")
    b.WriteString(", ")
    b.WriteString("World!")
    b.WriteByte('\n')
    b.WriteRune('日')

    // Obtener el resultado:
    result := b.String()
    fmt.Println(result)  // Hello, World!\n日

    // Reset para reutilizar:
    b.Reset()

    // Ejemplo SysAdmin: generar un informe:
    b.WriteString("=== Server Report ===\n")
    servers := []string{"web-01", "web-02", "db-01", "cache-01"}
    for _, s := range servers {
        fmt.Fprintf(&b, "  - %s: OK\n", s)
    }
    b.WriteString("=====================\n")
    fmt.Print(b.String())
}
```

```bash
# ¿Por qué Builder y no +=?
#
# s += "text" en un loop:
#   Iteración 1: alloca "text" (4 bytes)
#   Iteración 2: alloca "texttext" (8 bytes), copia, libera anterior
#   Iteración 3: alloca "texttexttext" (12 bytes), copia, libera anterior
#   ...
#   1000 iteraciones: ~500,000 bytes allocados y copiados total
#
# strings.Builder:
#   Alloca un buffer interno que crece exponencialmente.
#   ~10 allocaciones para 1000 iteraciones.
#   Con Grow(): 1 allocación.
#
# Para 10,000 concatenaciones: Builder es ~100x más rápido.
```

---

## 5. Paquete strings — referencia completa

### 5.1 Búsqueda

```go
import "strings"

s := "Hello, World! Hello, Go!"

// Contiene:
strings.Contains(s, "World")       // true
strings.Contains(s, "world")       // false (case-sensitive)

// Prefijo y sufijo:
strings.HasPrefix(s, "Hello")      // true
strings.HasSuffix(s, "Go!")        // true

// Posición:
strings.Index(s, "World")          // 7
strings.Index(s, "xyz")            // -1 (no encontrado)
strings.LastIndex(s, "Hello")      // 14

// Contar ocurrencias:
strings.Count(s, "Hello")          // 2
strings.Count(s, "l")              // 4

// Contiene algún carácter:
strings.ContainsAny(s, "xyz!")     // true (contiene '!')
strings.ContainsRune(s, '!')      // true
```

### 5.2 Transformación

```go
import "strings"

// Mayúsculas y minúsculas:
strings.ToUpper("hello")          // "HELLO"
strings.ToLower("HELLO")          // "hello"
strings.Title("hello world")      // "Hello World" (deprecated)

// Desde Go 1.18+, usar cases package para Title:
// import "golang.org/x/text/cases"
// cases.Title(language.English).String("hello world")

// Reemplazar:
strings.Replace("foo-bar-baz", "-", "_", -1)  // "foo_bar_baz"
// -1 = reemplazar todas. 1 = solo la primera.
strings.ReplaceAll("foo-bar-baz", "-", "_")    // "foo_bar_baz"

// Nuevo reemplazador (eficiente para múltiples reemplazos):
r := strings.NewReplacer(
    "${HOST}", "server-01",
    "${PORT}", "8080",
    "${ENV}", "production",
)
result := r.Replace("http://${HOST}:${PORT} (${ENV})")
// "http://server-01:8080 (production)"

// Recortar espacios:
strings.TrimSpace("  hello  \n")        // "hello"
strings.Trim("***hello***", "*")        // "hello"
strings.TrimLeft("###hello", "#")       // "hello"
strings.TrimRight("hello###", "#")      // "hello"
strings.TrimPrefix("hello.txt", "hello") // ".txt"
strings.TrimSuffix("hello.txt", ".txt")  // "hello"

// Repetir:
strings.Repeat("-", 40)               // "----------------------------------------"
strings.Repeat("ab", 3)               // "ababab"
```

### 5.3 Split y Join

```go
import "strings"

// Split:
strings.Split("a,b,c,d", ",")           // ["a", "b", "c", "d"]
strings.SplitN("a,b,c,d", ",", 2)       // ["a", "b,c,d"] (max 2 partes)
strings.SplitAfter("a,b,c", ",")        // ["a,", "b,", "c"]

// Fields (split por whitespace — ignora múltiples espacios):
strings.Fields("  hello   world  \t go  ")  // ["hello", "world", "go"]

// Join:
strings.Join([]string{"a", "b", "c"}, ", ")  // "a, b, c"
strings.Join([]string{"usr", "bin"}, "/")     // "usr/bin"

// Ejemplo SysAdmin — parsear líneas de configuración:
line := "server web-01 192.168.1.10 8080"
fields := strings.Fields(line)
// fields = ["server", "web-01", "192.168.1.10", "8080"]
role := fields[0]     // "server"
name := fields[1]     // "web-01"
ip := fields[2]       // "192.168.1.10"
port := fields[3]     // "8080"

// Parsear key=value:
kv := "LOG_LEVEL=debug"
parts := strings.SplitN(kv, "=", 2)  // ["LOG_LEVEL", "debug"]
key := parts[0]
value := parts[1]

_ = role; _ = name; _ = ip; _ = port; _ = key; _ = value
```

### 5.4 Map y funciones de carácter

```go
import "strings"

// Map aplica una función a cada rune:
result := strings.Map(func(r rune) rune {
    if r == '-' {
        return '_'
    }
    return r
}, "my-app-name")
// "my_app_name"

// Ejemplo: eliminar caracteres no-ASCII:
clean := strings.Map(func(r rune) rune {
    if r > 127 {
        return -1  // -1 = eliminar el carácter
    }
    return r
}, "héllo wörld")
// "hllo wrld"

// Cut (Go 1.18+) — split en la primera ocurrencia:
before, after, found := strings.Cut("host:8080", ":")
// before="host", after="8080", found=true

before, after, found = strings.Cut("localhost", ":")
// before="localhost", after="", found=false

// CutPrefix y CutSuffix (Go 1.20+):
rest, found := strings.CutPrefix("/api/v1/users", "/api/")
// rest="v1/users", found=true

rest, found = strings.CutSuffix("backup.tar.gz", ".tar.gz")
// rest="backup", found=true

_ = before; _ = after; _ = found; _ = rest
```

---

## 6. Paquete strconv — conversiones string ↔ tipos

```go
import "strconv"

// --- String a tipos ---

// Enteros:
n, err := strconv.Atoi("42")                  // string → int
n64, err := strconv.ParseInt("-42", 10, 64)    // string → int64
u64, err := strconv.ParseUint("42", 10, 64)   // string → uint64

// Floats:
f, err := strconv.ParseFloat("3.14", 64)      // string → float64

// Bool:
b, err := strconv.ParseBool("true")           // string → bool
// Acepta: "1", "t", "T", "TRUE", "true", "True"
//         "0", "f", "F", "FALSE", "false", "False"

// --- Tipos a string ---

s := strconv.Itoa(42)                          // int → string
s = strconv.FormatInt(-42, 10)                 // int64 → string
s = strconv.FormatUint(42, 16)                 // uint64 → string (hex)
s = strconv.FormatFloat(3.14, 'f', 2, 64)     // float64 → string
s = strconv.FormatBool(true)                   // bool → string

// --- Quoting ---
s = strconv.Quote("Hello\nWorld")              // `"Hello\nWorld"`
s = strconv.QuoteToASCII("日本語")              // `"\u65e5\u672c\u8a9e"`
s, err = strconv.Unquote(`"Hello\nWorld"`)     // Hello\nWorld

// --- Append (para builders eficientes) ---
buf := []byte("result: ")
buf = strconv.AppendInt(buf, 42, 10)           // "result: 42"
buf = strconv.AppendFloat(buf, 3.14, 'f', 2, 64)
```

---

## 7. Paquete fmt — formateo de strings

### 7.1 Verbs para strings

```go
import "fmt"

s := "Hello, World!"

fmt.Printf("%s\n", s)     // Hello, World!         (string)
fmt.Printf("%q\n", s)     // "Hello, World!"       (quoted string)
fmt.Printf("%x\n", s)     // 48656c6c6f2c20576f726c6421  (hex bytes)
fmt.Printf("%X\n", s)     // 48656C6C6F2C20576F726C6421  (hex uppercase)

// Padding:
fmt.Printf("[%20s]\n", s)   // [       Hello, World!]  (right-aligned)
fmt.Printf("[%-20s]\n", s)  // [Hello, World!       ]  (left-aligned)
fmt.Printf("[%.5s]\n", s)   // [Hello]                 (truncate to 5)
fmt.Printf("[%-10.5s]\n", s) // [Hello     ]           (truncate + pad)

// Verbs generales:
fmt.Printf("%v\n", s)       // Hello, World!  (valor por defecto)
fmt.Printf("%#v\n", s)      // "Hello, World!" (representación Go)
fmt.Printf("%T\n", s)       // string (tipo)
```

### 7.2 Sprintf — construir strings formateados

```go
// Sprintf es la navaja suiza para construir strings en Go:

// URL:
url := fmt.Sprintf("https://%s:%d/api/v%d/%s",
    "api.example.com", 443, 2, "users")
// "https://api.example.com:443/api/v2/users"

// Mensajes de log:
msg := fmt.Sprintf("[%s] %s: %s (code=%d)",
    "2024-12-15T10:30:00Z", "ERROR", "connection refused", 503)

// Paths:
path := fmt.Sprintf("/var/log/%s/%s.log", "myapp", "2024-12-15")

// Comandos SSH:
cmd := fmt.Sprintf("ssh %s@%s 'systemctl status %s'",
    "admin", "server-01", "nginx")

// Tablas:
for _, srv := range servers {
    fmt.Printf("%-15s %-20s %5d %6.1f%%\n",
        srv.Name, srv.IP, srv.Port, srv.CPUUsage)
}
// web-01          192.168.1.10          8080   45.2%
// web-02          192.168.1.11          8080   32.7%
// db-01           192.168.1.20          5432   78.9%
```

---

## 8. Bytes y strings — conversión e interoperabilidad

### 8.1 Conversión entre string y []byte

```go
package main

import "fmt"

func main() {
    // String a []byte (COPIA los datos):
    s := "Hello, World!"
    b := []byte(s)
    fmt.Println(b)  // [72 101 108 108 111 44 32 87 111 114 108 100 33]

    // []byte a string (COPIA los datos):
    b2 := []byte{72, 101, 108, 108, 111}
    s2 := string(b2)
    fmt.Println(s2)  // "Hello"

    // ⚠️ Cada conversión COPIA los bytes subyacentes.
    // Esto es necesario porque strings son inmutables
    // y []byte es mutable.

    // Si solo necesitas leer los bytes, usa un index directamente:
    firstByte := s[0]   // 72 (byte), sin copia
    fmt.Println(firstByte)
}
```

### 8.2 I/O trabaja con []byte

```go
import (
    "io"
    "os"
)

// La mayoría de I/O en Go usa []byte, no string:

// Leer archivo → []byte:
data, err := os.ReadFile("/etc/hostname")
// data es []byte

// Convertir a string (si necesitas operar como texto):
hostname := strings.TrimSpace(string(data))

// Escribir []byte a archivo:
os.WriteFile("/tmp/output.txt", []byte("Hello\n"), 0o644)

// io.Reader y io.Writer trabajan con []byte:
buf := make([]byte, 1024)
n, err := reader.Read(buf)     // lee bytes
writer.Write([]byte("hello"))  // escribe bytes

// Tip: si lees un archivo y lo procesas como string,
// la conversión string(data) es inevitable.
// Pero si solo lo reenvías (proxy, pipe), mantén []byte.
```

### 8.3 bytes package — el espejo de strings

```go
import "bytes"

// El paquete bytes tiene las mismas funciones que strings,
// pero trabaja con []byte en vez de string.

data := []byte("Hello, World!")

bytes.Contains(data, []byte("World"))      // true
bytes.HasPrefix(data, []byte("Hello"))     // true
bytes.Split(data, []byte(", "))            // [[]byte("Hello"), []byte("World!")]
bytes.TrimSpace(data)                       // []byte("Hello, World!")
bytes.ToUpper(data)                         // []byte("HELLO, WORLD!")
bytes.Replace(data, []byte("World"), []byte("Go"), 1)
bytes.Equal(data, []byte("Hello, World!")) // true
bytes.Index(data, []byte("World"))         // 7

// bytes.Buffer — como strings.Builder pero para []byte:
var buf bytes.Buffer
buf.WriteString("Hello")
buf.WriteByte(' ')
buf.Write([]byte("World"))
result := buf.Bytes()   // []byte
text := buf.String()    // string

// ¿Cuándo bytes vs strings?
// Si el dato llega como []byte (I/O) y lo devuelves como []byte → bytes
// Si trabajas con texto legible (logs, config) → strings
// Evitar conversiones innecesarias []byte ↔ string
```

---

## 9. Strings en contextos de SysAdmin

### 9.1 Parsear archivos de configuración

```go
package main

import (
    "bufio"
    "fmt"
    "os"
    "strings"
)

// ParseEnvFile lee un archivo .env y devuelve un map.
func ParseEnvFile(path string) (map[string]string, error) {
    f, err := os.Open(path)
    if err != nil {
        return nil, err
    }
    defer f.Close()

    env := make(map[string]string)
    scanner := bufio.NewScanner(f)

    for scanner.Scan() {
        line := strings.TrimSpace(scanner.Text())

        // Ignorar líneas vacías y comentarios:
        if line == "" || strings.HasPrefix(line, "#") {
            continue
        }

        // Parsear KEY=VALUE:
        key, value, found := strings.Cut(line, "=")
        if !found {
            continue
        }

        key = strings.TrimSpace(key)
        value = strings.TrimSpace(value)

        // Quitar comillas si las tiene:
        if len(value) >= 2 {
            if (value[0] == '"' && value[len(value)-1] == '"') ||
                (value[0] == '\'' && value[len(value)-1] == '\'') {
                value = value[1 : len(value)-1]
            }
        }

        env[key] = value
    }

    return env, scanner.Err()
}

func main() {
    env, err := ParseEnvFile(".env")
    if err != nil {
        fmt.Fprintf(os.Stderr, "error: %v\n", err)
        os.Exit(1)
    }

    for k, v := range env {
        fmt.Printf("%s=%s\n", k, v)
    }
}
```

### 9.2 Parsear output de comandos

```go
package main

import (
    "fmt"
    "os/exec"
    "strings"
)

// ParseDiskUsage parsea la salida de df -h.
func ParseDiskUsage() ([]map[string]string, error) {
    out, err := exec.Command("df", "-h").Output()
    if err != nil {
        return nil, err
    }

    lines := strings.Split(string(out), "\n")
    if len(lines) < 2 {
        return nil, fmt.Errorf("unexpected df output")
    }

    var disks []map[string]string

    // Saltar header (primera línea):
    for _, line := range lines[1:] {
        fields := strings.Fields(line)
        if len(fields) < 6 {
            continue
        }

        disk := map[string]string{
            "filesystem": fields[0],
            "size":       fields[1],
            "used":       fields[2],
            "available":  fields[3],
            "use_pct":    fields[4],
            "mounted_on": fields[5],
        }
        disks = append(disks, disk)
    }

    return disks, nil
}
```

### 9.3 Manipulación de paths

```go
import "path/filepath"

// filepath trabaja con paths del OS actual:
filepath.Join("var", "log", "myapp", "app.log")
// Linux: "var/log/myapp/app.log"
// Windows: "var\\log\\myapp\\app.log"

filepath.Base("/var/log/myapp/app.log")   // "app.log"
filepath.Dir("/var/log/myapp/app.log")    // "/var/log/myapp"
filepath.Ext("/var/log/myapp/app.log")    // ".log"

filepath.Abs("relative/path")             // "/home/user/relative/path"
filepath.Clean("/var/log/../lib/./myapp") // "/var/lib/myapp"

// Glob — buscar archivos por patrón:
matches, _ := filepath.Glob("/var/log/*.log")
// ["/var/log/syslog.log", "/var/log/auth.log", ...]

// Walk — recorrer directorios:
filepath.WalkDir("/var/log", func(path string, d os.DirEntry, err error) error {
    if err != nil {
        return err
    }
    if !d.IsDir() && strings.HasSuffix(path, ".log") {
        fmt.Println(path)
    }
    return nil
})
```

```go
import "path"

// path (sin /filepath) trabaja con paths estilo Unix siempre:
// Útil para URLs y rutas en protocolos (no filesystem).
path.Join("api", "v1", "users")  // "api/v1/users" (siempre /)
path.Base("api/v1/users")        // "users"
path.Dir("api/v1/users")         // "api/v1"
path.Ext("file.tar.gz")          // ".gz"
```

### 9.4 Templates de texto

```go
import "text/template"

// text/template es ideal para generar configuración:
const configTemplate = `# Generated config for {{.Name}}
server {
    listen {{.Port}};
    server_name {{.Domain}};

    location / {
        proxy_pass http://{{.Upstream}};
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
    {{if .EnableTLS}}
    listen 443 ssl;
    ssl_certificate /etc/ssl/{{.Domain}}.crt;
    ssl_certificate_key /etc/ssl/{{.Domain}}.key;
    {{end}}
}
`

type NginxConfig struct {
    Name      string
    Port      int
    Domain    string
    Upstream  string
    EnableTLS bool
}

func main() {
    tmpl, err := template.New("nginx").Parse(configTemplate)
    if err != nil {
        panic(err)
    }

    config := NginxConfig{
        Name:      "web-frontend",
        Port:      80,
        Domain:    "example.com",
        Upstream:  "127.0.0.1:8080",
        EnableTLS: true,
    }

    // Escribir a stdout:
    tmpl.Execute(os.Stdout, config)

    // Escribir a archivo:
    f, _ := os.Create("/tmp/nginx.conf")
    defer f.Close()
    tmpl.Execute(f, config)
}
```

### 9.5 Expresiones regulares

```go
import "regexp"

// Compilar regex (hazlo una sola vez, no en cada llamada):
var (
    ipRegex   = regexp.MustCompile(`\b(\d{1,3}\.){3}\d{1,3}\b`)
    logRegex  = regexp.MustCompile(`^(\S+)\s+(\S+)\s+\[(.+?)\]\s+"(.+?)"\s+(\d+)\s+(\d+)`)
    emailRegex = regexp.MustCompile(`[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}`)
)

func main() {
    // Match:
    fmt.Println(ipRegex.MatchString("192.168.1.1"))  // true

    // Find:
    log := `192.168.1.10 - [15/Dec/2024:10:30:00] "GET /api/health" 200 42`
    ip := ipRegex.FindString(log)
    fmt.Println(ip)  // 192.168.1.10

    // FindAll:
    text := "Servers: 10.0.0.1, 10.0.0.2, 10.0.0.3"
    ips := ipRegex.FindAllString(text, -1)
    fmt.Println(ips)  // [10.0.0.1 10.0.0.2 10.0.0.3]

    // Submatch (grupos de captura):
    matches := logRegex.FindStringSubmatch(log)
    if matches != nil {
        fmt.Println("IP:", matches[1])
        fmt.Println("Date:", matches[3])
        fmt.Println("Request:", matches[4])
        fmt.Println("Status:", matches[5])
    }

    // Replace:
    redacted := ipRegex.ReplaceAllString(text, "[REDACTED]")
    fmt.Println(redacted)
    // Servers: [REDACTED], [REDACTED], [REDACTED]

    // Split:
    parts := regexp.MustCompile(`\s*,\s*`).Split("a, b,  c ,d", -1)
    fmt.Println(parts)  // [a b c d]
}
```

```bash
# ⚠️ Go usa RE2 (no PCRE). Diferencias:
# - NO soporta lookahead (?=...) ni lookbehind (?<=...)
# - NO soporta backreferences (\1)
# - GARANTIZA ejecución en tiempo lineal (no hay regex DoS)
# - Para SysAdmin esto rara vez es un problema
```

---

## 10. Unicode y unicode/utf8

```go
import (
    "unicode"
    "unicode/utf8"
)

// --- utf8 package ---

// Contar runes en un string:
utf8.RuneCountInString("Hello, 日本語")  // 10

// Verificar si es UTF-8 válido:
utf8.ValidString("Hello")               // true
utf8.Valid([]byte{0xFF, 0xFE})          // false

// Decodificar primer rune:
r, size := utf8.DecodeRuneInString("日本語")
// r = '日', size = 3 (bytes)

// Codificar rune a bytes:
buf := make([]byte, 4)
n := utf8.EncodeRune(buf, '日')
// buf[:n] = [0xE6, 0x97, 0xA5], n = 3

// Tamaño de un rune en bytes:
utf8.RuneLen('A')   // 1
utf8.RuneLen('ñ')   // 2
utf8.RuneLen('日')  // 3
utf8.RuneLen('🎉')  // 4

// --- unicode package ---

// Clasificar caracteres:
unicode.IsLetter('A')    // true
unicode.IsDigit('5')     // true
unicode.IsSpace(' ')     // true
unicode.IsSpace('\t')    // true
unicode.IsPunct('!')     // true
unicode.IsUpper('A')     // true
unicode.IsLower('a')     // true

// Convertir:
unicode.ToUpper('a')     // 'A'
unicode.ToLower('A')     // 'a'
```

---

## 11. Strings multilinea y heredocs

```go
// Go no tiene heredocs como Bash, pero los raw strings
// con backticks cumplen la misma función:

// Equivalente a bash heredoc:
script := `#!/bin/bash
set -euo pipefail

echo "Starting deploy..."
rsync -avz /app/ server:/opt/app/
ssh server 'systemctl restart myapp'
echo "Deploy complete"
`

// Para indentación controlada, usar strings.TrimSpace
// o un patrón de trim manual:
import "strings"

yaml := strings.TrimSpace(`
apiVersion: v1
kind: ConfigMap
metadata:
  name: app-config
data:
  config.yaml: |
    server:
      port: 8080
`)

// Interpolación en strings multilinea:
// Go no tiene interpolación directa ($variable).
// Usa fmt.Sprintf o text/template:
config := fmt.Sprintf(`
[server]
host = %s
port = %d
workers = %d
`, host, port, workers)
```

---

## 12. String interning y performance

```go
// Go NO hace string interning automático (a diferencia de Python/Java).
// Cada string literal es una instancia separada... excepto las
// constantes que el compilador puede optimizar.

// Para comparar strings, Go compara primero la longitud
// y luego los bytes. Es O(n) en el peor caso.

// Tips de performance:

// 1. Evitar conversiones repetidas []byte ↔ string:
// MAL:
for _, line := range lines {
    if strings.Contains(string(lineBytes), "error") { }
}
// BIEN: convertir una vez:
lineStr := string(lineBytes)
if strings.Contains(lineStr, "error") { }

// 2. Usar strings.Builder para concatenación en loops.

// 3. Preallocar con Builder.Grow():
var b strings.Builder
b.Grow(estimatedSize)

// 4. Para búsquedas repetidas, compilar regex una sola vez:
var errorRegex = regexp.MustCompile(`(?i)error|fatal|panic`)
// NO: regexp.MustCompile dentro de un loop

// 5. strings.EqualFold es más rápido que ToLower+==:
strings.EqualFold("Hello", "hello")  // true (case-insensitive)
// vs:
// strings.ToLower("Hello") == strings.ToLower("hello")
// ↑ Alloca dos strings nuevos
```

---

## 13. Patrones de strings para SysAdmin

### 13.1 Parsear logs

```go
package main

import (
    "bufio"
    "fmt"
    "os"
    "strings"
    "time"
)

// LogEntry representa una línea de log.
type LogEntry struct {
    Timestamp time.Time
    Level     string
    Message   string
    Fields    map[string]string
}

// ParseLogLine parsea una línea con formato:
// 2024-12-15T10:30:00Z INFO Starting server port=8080 host=0.0.0.0
func ParseLogLine(line string) (*LogEntry, error) {
    fields := strings.Fields(line)
    if len(fields) < 3 {
        return nil, fmt.Errorf("invalid log line: %s", line)
    }

    ts, err := time.Parse(time.RFC3339, fields[0])
    if err != nil {
        return nil, err
    }

    entry := &LogEntry{
        Timestamp: ts,
        Level:     fields[1],
        Fields:    make(map[string]string),
    }

    // El mensaje es todo lo que no tiene =:
    var msgParts []string
    for _, f := range fields[2:] {
        if key, value, found := strings.Cut(f, "="); found {
            entry.Fields[key] = value
        } else {
            msgParts = append(msgParts, f)
        }
    }
    entry.Message = strings.Join(msgParts, " ")

    return entry, nil
}

func main() {
    scanner := bufio.NewScanner(os.Stdin)
    for scanner.Scan() {
        entry, err := ParseLogLine(scanner.Text())
        if err != nil {
            continue
        }
        if entry.Level == "ERROR" {
            fmt.Printf("[%s] %s %v\n",
                entry.Timestamp.Format("15:04:05"),
                entry.Message,
                entry.Fields)
        }
    }
}
```

### 13.2 Generar tablas ASCII

```go
package main

import (
    "fmt"
    "strings"
)

type ServerStatus struct {
    Name   string
    IP     string
    Status string
    CPU    float64
    Memory float64
}

func PrintTable(servers []ServerStatus) {
    // Header:
    fmt.Printf("%-15s %-18s %-8s %6s %6s\n",
        "NAME", "IP", "STATUS", "CPU%", "MEM%")
    fmt.Println(strings.Repeat("-", 58))

    // Rows:
    for _, s := range servers {
        status := s.Status
        fmt.Printf("%-15s %-18s %-8s %5.1f%% %5.1f%%\n",
            s.Name, s.IP, status, s.CPU, s.Memory)
    }

    fmt.Println(strings.Repeat("-", 58))
    fmt.Printf("Total: %d servers\n", len(servers))
}

func main() {
    servers := []ServerStatus{
        {"web-01", "192.168.1.10", "healthy", 45.2, 62.1},
        {"web-02", "192.168.1.11", "healthy", 32.7, 58.3},
        {"db-01", "192.168.1.20", "warning", 78.9, 91.2},
        {"cache-01", "192.168.1.30", "healthy", 12.4, 45.0},
    }

    PrintTable(servers)
}
// NAME            IP                 STATUS     CPU%   MEM%
// ----------------------------------------------------------
// web-01          192.168.1.10       healthy   45.2%  62.1%
// web-02          192.168.1.11       healthy   32.7%  58.3%
// db-01           192.168.1.20       warning   78.9%  91.2%
// cache-01        192.168.1.30       healthy   12.4%  45.0%
// ----------------------------------------------------------
// Total: 4 servers
```

### 13.3 Sanitización de input

```go
import (
    "strings"
    "unicode"
)

// SanitizeHostname limpia un hostname para uso seguro.
func SanitizeHostname(input string) string {
    // Trim whitespace:
    input = strings.TrimSpace(input)

    // Lowercase:
    input = strings.ToLower(input)

    // Solo permitir caracteres válidos para hostname (RFC 1123):
    var b strings.Builder
    for _, r := range input {
        if unicode.IsLetter(r) || unicode.IsDigit(r) || r == '-' || r == '.' {
            b.WriteRune(r)
        }
    }

    result := b.String()

    // Trim leading/trailing hyphens and dots:
    result = strings.Trim(result, "-.")

    // Limitar longitud (253 chars max para FQDN):
    if len(result) > 253 {
        result = result[:253]
    }

    return result
}

// SanitizeFilePath previene path traversal.
func SanitizeFilePath(input string) string {
    // Eliminar . y .. components:
    cleaned := filepath.Clean(input)

    // Asegurar que no empiece con / (path relativo):
    cleaned = strings.TrimPrefix(cleaned, "/")

    // Eliminar cualquier .. que quede:
    parts := strings.Split(cleaned, string(filepath.Separator))
    var safe []string
    for _, p := range parts {
        if p != ".." && p != "." && p != "" {
            safe = append(safe, p)
        }
    }

    return filepath.Join(safe...)
}
```

---

## 14. Comparación con C y Rust

```
    ┌──────────────────┬──────────────────┬──────────────────┬──────────────────┐
    │ Concepto         │ Go               │ C                │ Rust             │
    ├──────────────────┼──────────────────┼──────────────────┼──────────────────┤
    │ Bool tipo        │ bool             │ _Bool / bool     │ bool             │
    │ Bool valores     │ true, false      │ 1, 0 (+ true,   │ true, false      │
    │                  │                  │  false con       │                  │
    │                  │                  │  stdbool.h)      │                  │
    │ Bool ↔ int       │ NO (explícito)   │ SÍ (implícito)   │ NO               │
    ├──────────────────┼──────────────────┼──────────────────┼──────────────────┤
    │ String tipo      │ string           │ char* / char[]   │ &str / String    │
    │ String inmutable │ SÍ               │ Si es literal    │ &str sí,         │
    │                  │                  │                  │ String mutable   │
    │ Encoding         │ UTF-8            │ bytes (ninguno)  │ UTF-8 (forzado)  │
    │ Null-terminated  │ NO               │ SÍ ('\0')        │ NO               │
    │ Length O(1)      │ SÍ (len field)   │ NO (strlen O(n)) │ SÍ (len field)   │
    │ Indexing         │ byte (no char)   │ byte             │ error (debe usar │
    │                  │                  │                  │ .chars())        │
    │ Concatenation    │ + o Builder      │ strcat (unsafe)  │ + o String::push │
    │ Comparison       │ == (contenido)   │ strcmp()          │ == (contenido)   │
    │ Substring        │ s[i:j] (bytes)   │ pointer arith    │ &s[i..j] (bytes) │
    └──────────────────┴──────────────────┴──────────────────┴──────────────────┘
```

```bash
# Diferencias clave para alguien que viene de C:
# 1. Go strings NO están null-terminated — len() es O(1)
# 2. Go strings son inmutables — no hay buffer overflow
# 3. s[i] devuelve byte, no char — para Unicode usar range o []rune
# 4. == compara contenido (no puntero) — no necesitas strcmp()
# 5. Concatenación con + crea nuevo string — no modifica in-place

# Diferencias clave para alguien que viene de Rust:
# 1. Go no distingue &str vs String — solo tiene string
# 2. Go permite indexar bytes sin error — Rust prohíbe s[0]
# 3. Go no garantiza UTF-8 válido — puedes tener bytes inválidos
# 4. Go no tiene lifetime en strings — GC se encarga
```

---

## 15. Tabla de errores comunes

| Error / Síntoma | Causa | Solución |
|---|---|---|
| `len("café") = 5` (no 4) | `len()` cuenta bytes, no caracteres | `utf8.RuneCountInString("café")` = 4 |
| `s[3]` de "café" da byte 0xC3 (no 'é') | Indexar string devuelve byte | `[]rune(s)[3]` o iterar con `range` |
| `cannot assign to s[0]` | Strings son inmutables | Convertir a `[]byte`, modificar, convertir de vuelta |
| `+=` en loop es lento | Cada `+=` alloca string nuevo y copia | Usar `strings.Builder` |
| `cannot use s (string) as []byte` | Tipos diferentes, sin conversión implícita | `[]byte(s)` (copia) |
| Regex con `\d` no funciona | Olvidaste raw string, `\d` se interpreta como escape | Usar backtick: `` `\d+` `` |
| `regexp.Compile` en loop es lento | Compila regex en cada iteración | Compilar una vez con `regexp.MustCompile` a nivel paquete |
| `strings.Contains` es case-sensitive | `Contains("Hello", "hello")` = false | `strings.EqualFold` o `strings.Contains(strings.ToLower(...))` |
| `strings.Split("", ",")` devuelve `[""]` | Split de string vacío devuelve slice con un elemento vacío | Verificar `len(s) > 0` antes de split |
| Path traversal con `../` en input de usuario | `filepath.Join("/base", userInput)` puede escapar | Usar `filepath.Clean` + verificar prefijo |
| JSON con caracteres Unicode sale escaped | `json.Marshal` escapa `<`, `>`, `&` por defecto | Usar `json.Encoder` con `SetEscapeHTML(false)` |
| `strconv.Atoi` falla con espacios | `" 42 "` no es parseable | `strconv.Atoi(strings.TrimSpace(s))` |

---

## 16. Ejercicios

### Ejercicio 1 — Bool y operadores lógicos

```
Crea un programa que tome un hostname y verifique:
- ¿Es un FQDN? (contiene al menos un punto)
- ¿Empieza con un dígito? (inválido según RFC)
- ¿Tiene más de 253 caracteres? (inválido)
- ¿Contiene caracteres no permitidos? (solo a-z, 0-9, -, .)

Combina todas las verificaciones con operadores lógicos.
Imprime el resultado como: "hostname valid: true/false"

Predicción: "my-server.example.com" ¿es válido?
"192.168.1.1" ¿es un hostname válido? ¿Un FQDN?
```

### Ejercicio 2 — len() vs RuneCountInString

```
Crea un programa que para cada uno de estos strings:
- "Hello"
- "café"
- "日本語"
- "Hello, 🌍!"

Imprima:
- len() (bytes)
- utf8.RuneCountInString() (caracteres)
- Cada byte en hexadecimal
- Cada rune con su código Unicode (U+XXXX)

Predicción: ¿cuántos bytes tiene "🌍"?
¿len("Hello, 🌍!") es 11 o más?
```

### Ejercicio 3 — strings.Builder vs +=

```
Crea un benchmark que concatene 100,000 strings "hello "
usando:
1. += en un loop
2. strings.Builder
3. strings.Join con un slice preallocado

Mide el tiempo de cada uno con time.Now() y time.Since().

Predicción: ¿cuántas veces más rápido es Builder vs +=?
¿10x? ¿100x? ¿1000x?
```

### Ejercicio 4 — Parsear archivo .env

```
Crea un parser de archivos .env que soporte:
- Líneas KEY=VALUE
- Comentarios con #
- Valores con comillas simples y dobles
- Valores con espacios
- Líneas vacías

Archivo de test:
# Database config
DB_HOST=localhost
DB_PORT=5432
DB_NAME="my_database"
DB_PASSWORD='s3cr3t p@ss'

# App config
APP_ENV=production

Predicción: ¿strings.Cut o strings.SplitN es mejor para
parsear KEY=VALUE? ¿Qué pasa si el VALUE contiene "="?
```

### Ejercicio 5 — Parsear output de comando

```
Crea un programa que ejecute "ps aux" y parsee la salida:
1. Extraer USER, PID, %CPU, %MEM, COMMAND de cada línea
2. Filtrar procesos con >5% CPU
3. Ordenar por %CPU descendente
4. Imprimir como tabla formateada

Predicción: ¿strings.Fields es suficiente para parsear ps?
¿Qué pasa con el campo COMMAND que puede contener espacios?
```

### Ejercicio 6 — Regex para logs

```
Crea un programa que parsee logs en formato Apache combined:
127.0.0.1 - frank [10/Oct/2000:13:55:36 -0700] "GET /apache_pb.gif HTTP/1.0" 200 2326

1. Compila la regex UNA sola vez
2. Extrae: IP, usuario, fecha, método, path, status, bytes
3. Filtra las líneas con status >= 400
4. Cuenta ocurrencias por status code

Predicción: ¿puedes usar lookahead en Go regex?
¿Qué alternativa tienes si necesitas un patrón complejo?
```

### Ejercicio 7 — Template de configuración

```
Crea un generador de archivos /etc/hosts usando text/template:
- Input: slice de structs {Hostname, IP, Aliases []string}
- Output: formato estándar /etc/hosts

Template:
# Generated by myapp on {{.Date}}
127.0.0.1 localhost
::1       localhost
{{range .Hosts}}
{{.IP}}   {{.Hostname}} {{range .Aliases}}{{.}} {{end}}
{{end}}

Predicción: ¿text/template o html/template?
¿Qué pasa con la indentación en los {{range}}?
```

### Ejercicio 8 — Sanitización de input

```
Crea funciones:
- SanitizeHostname(input string) string
- SanitizeFilePath(basedir, input string) (string, error)
- SanitizeShellArg(input string) string

Prueba con inputs maliciosos:
- "server; rm -rf /"
- "../../../etc/passwd"
- "hello\x00world" (null byte injection)
- "   VALID-HOST.example.com   "

Predicción: ¿filepath.Clean elimina los ../? ¿Siempre?
¿Qué pasa con null bytes dentro de un string de Go?
```

### Ejercicio 9 — Procesamiento de CSV

```
Sin usar encoding/csv, crea un parser simple de CSV:
- Soporta campos con comillas (que pueden contener comas)
- Soporta newlines dentro de campos entrecomillados
- Devuelve [][]string

Prueba con:
name,ip,port
"web server",192.168.1.10,8080
"db, primary",192.168.1.20,5432

Predicción: ¿strings.Split(line, ",") es suficiente?
¿Qué rompe con el campo "db, primary"?
```

### Ejercicio 10 — Dashboard de sistema en terminal

```
Crea un programa que imprima cada 2 segundos:
1. Hostname
2. Uptime (parseado de /proc/uptime, formateado como "2d 5h 30m")
3. Load average
4. Memory usage con barra de progreso ASCII:
   MEM: [████████████░░░░░░░░] 62.5% (8.0/12.8 GB)
5. Top 5 procesos por CPU

Usa:
- strings.Repeat para las barras
- fmt.Sprintf para el formateo
- strings.Fields para parsear /proc

Predicción: ¿cómo haces la barra de progreso con
strings.Repeat("█", n) + strings.Repeat("░", 20-n)?
```

---

## 17. Resumen

```
    ┌───────────────────────────────────────────────────────────┐
    │          Booleanos y strings en Go — Resumen              │
    │                                                           │
    │  BOOL:                                                    │
    │  ┌─ true, false (no 0/1 — no hay conversión implícita)   │
    │  ├─ && (AND), || (OR), ! (NOT) con short-circuit         │
    │  ├─ Zero value: false                                    │
    │  └─ strconv.ParseBool para "true"/"false"/"1"/"0"        │
    │                                                           │
    │  STRING:                                                  │
    │  ┌─ Secuencia de bytes inmutable, UTF-8 por convención   │
    │  ├─ Header: ptr (8B) + len (8B) = 16 bytes               │
    │  ├─ len() = BYTES, utf8.RuneCountInString() = CHARS      │
    │  ├─ s[i] = byte, range s = rune                          │
    │  ├─ "interpreted" vs `raw string`                        │
    │  └─ Zero value: "" (string vacío)                        │
    │                                                           │
    │  CONCATENACIÓN:                                           │
    │  ┌─ + o += → OK para pocas concatenaciones               │
    │  ├─ strings.Builder → loops (eficiente)                  │
    │  ├─ strings.Join → unir slices                           │
    │  └─ fmt.Sprintf → formateo con tipos mixtos              │
    │                                                           │
    │  PAQUETES CLAVE:                                          │
    │  ┌─ strings     → buscar, split, join, replace, trim     │
    │  ├─ strconv     → parsear/formatear tipos ↔ string       │
    │  ├─ fmt         → formateo con verbs (%s, %q, %v)        │
    │  ├─ bytes       → como strings pero para []byte          │
    │  ├─ regexp      → expresiones regulares (RE2)            │
    │  ├─ unicode     → clasificar/transformar runes           │
    │  ├─ path/filepath → manipular paths del filesystem       │
    │  └─ text/template → generar texto con templates          │
    │                                                           │
    │  PARA SYSADMIN:                                           │
    │  ┌─ Raw strings `...` → regex, JSON, YAML, SQL, paths   │
    │  ├─ strings.Fields → parsear output de comandos          │
    │  ├─ strings.Cut → parsear KEY=VALUE                      │
    │  ├─ filepath.Join → construir paths seguros              │
    │  ├─ text/template → generar configs (nginx, hosts, etc.) │
    │  └─ Sanitizar SIEMPRE input de usuario                   │
    └───────────────────────────────────────────────────────────┘
```
