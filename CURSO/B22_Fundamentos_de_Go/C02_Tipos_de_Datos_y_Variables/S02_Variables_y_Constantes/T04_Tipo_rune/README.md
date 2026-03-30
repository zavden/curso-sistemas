# Tipo Rune en Go

## 1. Introducción

En Go, un `rune` representa un **Unicode code point** — un carácter individual, independientemente de cuántos bytes ocupe en UTF-8. El tipo `rune` es un alias de `int32`, capaz de representar los 1,114,112 code points de Unicode (de U+0000 a U+10FFFF).

La distinción entre **bytes** y **runes** es fundamental para cualquier SysAdmin que procese texto: nombres de usuario internacionales, paths con caracteres especiales, logs en múltiples idiomas, parseo de archivos de configuración con caracteres UTF-8.

```
┌──────────────────────────────────────────────────────────────┐
│             BYTES vs RUNES EN GO                             │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  string en Go = secuencia de BYTES (no caracteres)           │
│  UTF-8 codifica cada carácter en 1-4 bytes                   │
│                                                              │
│  "Hello" → 5 bytes, 5 runes (ASCII = 1 byte por carácter)   │
│  "Hola"  → 4 bytes, 4 runes                                 │
│  "日本語"  → 9 bytes, 3 runes (CJK = 3 bytes cada uno)       │
│  "🎉🚀"  → 8 bytes, 2 runes (emoji = 4 bytes cada uno)      │
│                                                              │
│  byte = uint8  → un byte individual (0-255)                  │
│  rune = int32  → un Unicode code point (0-1,114,111)         │
│                                                              │
│  len("日本語")                    = 9  (bytes)                │
│  utf8.RuneCountInString("日本語") = 3  (caracteres)           │
│  []rune("日本語")                 = [26085, 26412, 35486]     │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Unicode y UTF-8 — Fundamentos

### Unicode: el catálogo universal de caracteres

```
┌──────────────────────────────────────────────────────────────┐
│  Unicode asigna un número (code point) a cada carácter       │
│                                                              │
│  Code Point    Carácter    Nombre                            │
│  U+0041        A           LATIN CAPITAL LETTER A            │
│  U+00E9        é           LATIN SMALL LETTER E WITH ACUTE   │
│  U+00F1        ñ           LATIN SMALL LETTER N WITH TILDE   │
│  U+4E16        世          CJK UNIFIED IDEOGRAPH-4E16        │
│  U+1F680       🚀          ROCKET                            │
│                                                              │
│  Planos Unicode:                                             │
│  U+0000–U+FFFF     BMP (Basic Multilingual Plane)            │
│  U+10000–U+1FFFF   SMP (emojis, matemáticas, históricos)    │
│  ...hasta U+10FFFF (17 planos × 65536 = 1,114,112 puntos)  │
│                                                              │
│  rune (int32) puede representar TODOS los code points        │
│  Un byte (uint8) solo puede representar U+0000–U+00FF       │
└──────────────────────────────────────────────────────────────┘
```

### UTF-8: la codificación que Go usa internamente

UTF-8 es una codificación de **longitud variable** — cada code point ocupa de 1 a 4 bytes:

```
┌──────────────────────────────────────────────────────────────┐
│  Rango code point     Bytes   Patrón de bits                 │
├──────────────────────────────────────────────────────────────┤
│  U+0000  – U+007F    1       0xxxxxxx                       │
│  U+0080  – U+07FF    2       110xxxxx 10xxxxxx              │
│  U+0800  – U+FFFF    3       1110xxxx 10xxxxxx 10xxxxxx     │
│  U+10000 – U+10FFFF  4       11110xxx 10xxxxxx 10xx.. 10xx..│
└──────────────────────────────────────────────────────────────┘
│                                                              │
│  Ejemplos:                                                   │
│  'A'  = U+0041 → 0x41                     (1 byte)          │
│  'é'  = U+00E9 → 0xC3 0xA9               (2 bytes)         │
│  'ñ'  = U+00F1 → 0xC3 0xB1               (2 bytes)         │
│  '世' = U+4E16 → 0xE4 0xB8 0x96          (3 bytes)         │
│  '🚀' = U+1F680 → 0xF0 0x9F 0x9A 0x80   (4 bytes)         │
│                                                              │
│  Propiedad clave: ASCII (0-127) es idéntico en UTF-8         │
│  → Archivos ASCII son automáticamente UTF-8 válidos          │
│  → /etc/hosts, /etc/passwd etc. funcionan sin cambios        │
└──────────────────────────────────────────────────────────────┘
```

```go
package main

import (
    "fmt"
    "unicode/utf8"
)

func main() {
    s := "Hola, 世界! 🚀"

    fmt.Printf("String:    %s\n", s)
    fmt.Printf("Bytes:     %d\n", len(s))                          // 18
    fmt.Printf("Runes:     %d\n", utf8.RuneCountInString(s))       // 11
    fmt.Printf("Hex bytes: % x\n", []byte(s))
    // 48 6f 6c 61 2c 20 e4 b8 96 e7 95 8c 21 20 f0 9f 9a 80

    // Inspeccionar cada rune
    for i, r := range s {
        fmt.Printf("  byte[%2d] rune=U+%04X (%c) size=%d\n",
            i, r, r, utf8.RuneLen(r))
    }
}
```

---

## 3. Declaración y literales de rune

```go
// rune es alias de int32
var r rune = 'A'          // 65
var r2 rune = 'ñ'         // 241 (U+00F1)
var r3 rune = '世'        // 19990 (U+4E16)
var r4 rune = '🚀'       // 128640 (U+1F680)

// Escapes en literales de rune
var tab rune = '\t'       // 9 (tabulador)
var newline rune = '\n'   // 10 (nueva línea)
var backslash rune = '\\' // 92
var quote rune = '\''     // 39

// Escapes Unicode
var r5 rune = '\u00F1'    // ñ (4 dígitos hex — BMP)
var r6 rune = '\U0001F680' // 🚀 (8 dígitos hex — cualquier plane)

// Escape octal y hex (byte value)
var r7 rune = '\x41'      // 'A' (hex byte)
var r8 rune = '\101'      // 'A' (octal byte)
```

### rune es int32 — aritmética

```go
// Puedes hacer aritmética con runes
r := 'A'
fmt.Println(r + 1)         // 66
fmt.Println(string(r + 1)) // "B"
fmt.Println(r + 32)        // 97 ('a' — lowercase en ASCII)
fmt.Println(string(r + 32)) // "a"

// Verificar rangos
fmt.Println(r >= 'A' && r <= 'Z')  // true — es mayúscula ASCII
fmt.Println(r >= '0' && r <= '9')  // false — no es dígito

// Distancia entre caracteres
fmt.Println('Z' - 'A')   // 25
fmt.Println('z' - 'a')   // 25
fmt.Println('9' - '0')   // 9
```

---

## 4. Iteración: `for range` sobre strings

La forma idiomática de iterar sobre caracteres (runes) en un string:

```go
s := "Café ☕"

// for range itera por RUNES (no bytes)
for i, r := range s {
    fmt.Printf("index=%d rune=%c (U+%04X) size=%d bytes\n",
        i, r, r, utf8.RuneLen(r))
}
// index=0 rune=C (U+0043) size=1 bytes
// index=1 rune=a (U+0061) size=1 bytes
// index=2 rune=f (U+0066) size=1 bytes
// index=3 rune=é (U+00E9) size=2 bytes
// index=5 rune=  (U+0020) size=1 bytes    ← index saltó de 3 a 5
// index=6 rune=☕ (U+2615) size=3 bytes
```

### El índice es posición en BYTES, no en runes

```go
s := "日本語"

for i, r := range s {
    fmt.Printf("byte_pos=%d rune=%c\n", i, r)
}
// byte_pos=0 rune=日
// byte_pos=3 rune=本   ← saltó 3 bytes (cada CJK = 3 bytes UTF-8)
// byte_pos=6 rune=語

// Si necesitas el índice de rune (posición del carácter):
for runeIdx, r := range []rune(s) {
    fmt.Printf("rune_pos=%d rune=%c\n", runeIdx, r)
}
// rune_pos=0 rune=日
// rune_pos=1 rune=本
// rune_pos=2 rune=語
```

### Iteración por bytes vs runes

```go
s := "Hélène"

// Por BYTES — acceso con s[i]
fmt.Println("--- Bytes ---")
for i := 0; i < len(s); i++ {
    fmt.Printf("s[%d] = 0x%02X (%c)\n", i, s[i], s[i])
}
// s[0] = 0x48 (H)
// s[1] = 0xC3 (Ã)   ← primer byte de 'é'
// s[2] = 0xA9 (©)   ← segundo byte de 'é' — ¡NO es un carácter válido solo!
// ...

// Por RUNES — acceso con range
fmt.Println("--- Runes ---")
for _, r := range s {
    fmt.Printf("%c (U+%04X)\n", r, r)
}
// H (U+0048)
// é (U+00E9)   ← decodificado correctamente de 2 bytes
// l (U+006C)
// è (U+00E8)
// n (U+006E)
// e (U+0065)
```

---

## 5. Acceso por posición — `s[i]` vs `[]rune(s)[i]`

```go
s := "Hello, 世界"

// s[i] retorna el BYTE en posición i, no el carácter
fmt.Println(s[0])          // 72 ('H' — ASCII, coincide)
fmt.Println(s[7])          // 184 (0xB8 — byte medio de '世', NO un carácter)
fmt.Printf("%c\n", s[7])  // ¸ — carácter basura

// Para acceder al carácter n, convertir a []rune
runes := []rune(s)
fmt.Println(string(runes[7]))  // 界 — correcto

// Costo: []rune(s) es O(n) — recorre todo el string
// Para acceso aleatorio frecuente, convertir una vez y reusar
```

### Slicing de strings — cuidado con UTF-8

```go
s := "café"

// Slicing opera en BYTES
fmt.Println(s[:3])   // "caf" — correcto por suerte (ASCII)
fmt.Println(s[:4])   // "caf\xc3" — ¡CORTA 'é' por la mitad!
fmt.Println(s[:5])   // "café" — correcto (incluye los 2 bytes de 'é')

// Slicing seguro: usar posiciones de rune
runes := []rune(s)
fmt.Println(string(runes[:3]))  // "caf" — 3 primeros caracteres
fmt.Println(string(runes[:4]))  // "café" — 4 primeros caracteres
```

---

## 6. Paquete `unicode/utf8`

Funciones esenciales para trabajar con UTF-8:

```go
import "unicode/utf8"

s := "Hello, 世界! 🎉"

// ─── Contar runes ──────────────────────────
count := utf8.RuneCountInString(s)   // 12 (caracteres)
bytes := len(s)                       // 18 (bytes)

// También funciona con []byte
data := []byte(s)
count = utf8.RuneCount(data)         // 12

// ─── Decodificar primera rune ──────────────
r, size := utf8.DecodeRuneInString(s)
fmt.Printf("%c occupies %d bytes\n", r, size)  // H occupies 1 bytes

// Decodificar última rune
r, size = utf8.DecodeLastRuneInString(s)
fmt.Printf("%c occupies %d bytes\n", r, size)  // 🎉 occupies 4 bytes

// ─── Tamaño de una rune en UTF-8 ──────────
fmt.Println(utf8.RuneLen('A'))    // 1
fmt.Println(utf8.RuneLen('ñ'))    // 2
fmt.Println(utf8.RuneLen('世'))   // 3
fmt.Println(utf8.RuneLen('🚀'))  // 4

// ─── Codificar rune a bytes ────────────────
buf := make([]byte, 4)  // máximo 4 bytes por rune
n := utf8.EncodeRune(buf, '世')
fmt.Printf("% x (wrote %d bytes)\n", buf[:n], n)  // e4 b8 96 (wrote 3 bytes)

// ─── Validación ────────────────────────────
fmt.Println(utf8.ValidString("hello"))           // true
fmt.Println(utf8.ValidString("hello\xff"))       // false — byte inválido
fmt.Println(utf8.Valid([]byte{0xc3, 0xa9}))      // true — 'é'
fmt.Println(utf8.Valid([]byte{0xc3}))             // false — secuencia incompleta

// ─── Verificar si un byte inicia una rune ──
fmt.Println(utf8.RuneStart(0x48))   // true — 'H' (0xxxxxxx)
fmt.Println(utf8.RuneStart(0xC3))   // true — inicio de secuencia 2-byte
fmt.Println(utf8.RuneStart(0xA9))   // false — byte de continuación (10xxxxxx)

// ─── Rune inválida ─────────────────────────
// utf8.RuneError = U+FFFD (�) — el carácter de reemplazo
// Aparece cuando decodificas UTF-8 inválido
bad := "\xff\xfe"
for _, r := range bad {
    fmt.Printf("U+%04X ", r)  // U+FFFD U+FFFD — reemplazados
}

// FullRune: ¿los bytes forman una rune completa?
fmt.Println(utf8.FullRune([]byte{0xc3, 0xa9}))  // true — 'é' completa
fmt.Println(utf8.FullRune([]byte{0xc3}))          // false — falta segundo byte
```

---

## 7. Paquete `unicode` — Clasificación de caracteres

```go
import "unicode"

// ─── Clasificación ─────────────────────────
unicode.IsLetter('A')     // true
unicode.IsLetter('7')     // false
unicode.IsDigit('7')      // true
unicode.IsDigit('A')      // false
unicode.IsSpace(' ')      // true
unicode.IsSpace('\t')     // true
unicode.IsSpace('\n')     // true
unicode.IsPunct('.')      // true
unicode.IsControl('\x00') // true (NULL)
unicode.IsControl('\n')   // true
unicode.IsGraphic('A')    // true (imprimible)
unicode.IsGraphic('\n')   // false
unicode.IsPrint('A')      // true
unicode.IsPrint('\t')     // false

// ─── Mayúsculas y minúsculas ───────────────
unicode.IsUpper('A')      // true
unicode.IsLower('a')      // true
unicode.ToUpper('a')      // 'A'
unicode.ToLower('A')      // 'a'
unicode.ToTitle('a')      // 'A' (title case)

// Funciona con Unicode completo, no solo ASCII
unicode.IsLetter('ñ')     // true
unicode.IsLetter('世')    // true
unicode.IsUpper('Ñ')      // true
unicode.ToLower('Ñ')      // 'ñ'
unicode.IsDigit('٣')      // true (dígito árabe 3)

// ─── Tablas de rangos Unicode ──────────────
unicode.Is(unicode.Latin, 'A')       // true
unicode.Is(unicode.Latin, 'ñ')       // true
unicode.Is(unicode.Latin, '世')      // false
unicode.Is(unicode.Han, '世')        // true (caracteres chinos)
unicode.Is(unicode.Cyrillic, 'Д')    // true (ruso)
unicode.Is(unicode.Greek, 'Ω')       // true
unicode.Is(unicode.Arabic, 'ع')      // true

// ─── Categorías especiales ─────────────────
unicode.In('🚀', unicode.So)  // true — So = Symbol, other
// Las categorías siguen la base de datos Unicode:
// Lu = Letter, uppercase
// Ll = Letter, lowercase
// Nd = Number, decimal digit
// Zs = Separator, space
// Pc = Punctuation, connector
// So = Symbol, other (emojis caen aquí)
```

---

## 8. `strings.Builder` — Construcción eficiente de strings

Cuando necesitas construir un string rune por rune o en múltiples pasos, `strings.Builder` es la herramienta correcta:

```go
import "strings"

// ─── Problema: concatenar con + crea copias ────────
// Cada + crea un nuevo string (inmutable) — O(n²) en total
func badConcat(words []string) string {
    result := ""
    for _, w := range words {
        result += w + " "  // copia todo result + w cada vez
    }
    return result
}

// ─── Solución: strings.Builder ─────────────────────
func goodConcat(words []string) string {
    var b strings.Builder
    for _, w := range words {
        b.WriteString(w)
        b.WriteByte(' ')  // o b.WriteRune(' ')
    }
    return b.String()
}
```

### API de `strings.Builder`

```go
var b strings.Builder

// Zero value funcional — no necesita New() ni make()
b.WriteString("Hello")        // append string
b.WriteByte(',')               // append un byte
b.WriteRune(' ')               // append una rune (1-4 bytes)
b.WriteRune('世')              // append rune multi-byte
b.Write([]byte("!"))           // append []byte

fmt.Println(b.String())       // "Hello, 世!"
fmt.Println(b.Len())          // 12 (bytes)
fmt.Println(b.Cap())          // capacidad del buffer interno

b.Reset()                     // vaciar y reutilizar
fmt.Println(b.Len())          // 0
```

### Pre-alocar para mejor performance

```go
func buildLogEntry(parts []string) string {
    // Estimar tamaño total para evitar realocaciones
    totalLen := 0
    for _, p := range parts {
        totalLen += len(p) + 1  // +1 por separador
    }

    var b strings.Builder
    b.Grow(totalLen)  // pre-alocar capacidad exacta

    for i, p := range parts {
        if i > 0 {
            b.WriteByte('|')
        }
        b.WriteString(p)
    }
    return b.String()
}
```

### Builder vs otros métodos de concatenación

```
┌──────────────────────────────────────────────────────────────┐
│       CONCATENACIÓN — PERFORMANCE COMPARADA                  │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Método                  Complejidad   Allocations           │
│  ─────────────────────────────────────────────────           │
│  s += "x" (loop)         O(n²)        n allocs               │
│  fmt.Sprintf             O(n)         reflection overhead    │
│  strings.Join(slice)     O(n)         1 alloc                │
│  strings.Builder         O(n)         ~1-2 allocs            │
│  bytes.Buffer            O(n)         ~1-2 allocs            │
│                                                              │
│  Recomendación:                                              │
│  ├─ Pocas strings conocidas → "a" + "b" + "c" (el           │
│  │    compilador optimiza concatenaciones constantes)        │
│  ├─ Slice de strings → strings.Join(slice, sep)              │
│  ├─ Construcción iterativa → strings.Builder                 │
│  └─ Mixto bytes/strings → bytes.Buffer                       │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 9. `bytes.Buffer` — La alternativa para I/O

`bytes.Buffer` es similar a `strings.Builder` pero implementa `io.Reader` e `io.Writer`, haciéndolo útil para I/O:

```go
import "bytes"

var buf bytes.Buffer

// Escribir
buf.WriteString("Host: ")
buf.WriteString(hostname)
buf.WriteByte('\n')
buf.WriteRune('→')
buf.Write([]byte(" status: OK"))

// Leer como string
result := buf.String()

// Leer como []byte (sin copia — referencia al buffer interno)
data := buf.Bytes()

// Como io.Reader — pasar a funciones que esperan Reader
_, err := io.Copy(os.Stdout, &buf)

// Diferencia clave con Builder:
// Builder: solo escritura, String() final
// Buffer: lectura Y escritura, implementa io.Reader/io.Writer
```

### Cuándo usar Builder vs Buffer

```go
// strings.Builder — construir un string resultado
func formatReport(entries []LogEntry) string {
    var b strings.Builder
    for _, e := range entries {
        fmt.Fprintf(&b, "%s [%s] %s\n", e.Time, e.Level, e.Message)
    }
    return b.String()
}

// bytes.Buffer — cuando necesitas io.Reader/io.Writer
func sendReport(entries []LogEntry) error {
    var buf bytes.Buffer
    for _, e := range entries {
        fmt.Fprintf(&buf, "%s [%s] %s\n", e.Time, e.Level, e.Message)
    }
    // buf implementa io.Reader — se puede pasar a http.Post
    resp, err := http.Post(webhookURL, "text/plain", &buf)
    // ...
}
```

---

## 10. Rune inválida — `utf8.RuneError` y U+FFFD

Cuando Go encuentra UTF-8 inválido, usa la rune de reemplazo `U+FFFD` (�):

```go
import "unicode/utf8"

// Secuencia UTF-8 inválida
bad := "\xff\xfe\xfd"

// Range itera y reemplaza bytes inválidos con RuneError
for i, r := range bad {
    if r == utf8.RuneError {
        fmt.Printf("byte[%d]: invalid UTF-8 → U+FFFD (�)\n", i)
    }
}

// Validar antes de procesar
if !utf8.ValidString(bad) {
    fmt.Println("WARNING: string contains invalid UTF-8")
}
```

### Sanitizar UTF-8 inválido

```go
import "strings"

// strings.ToValidUTF8 reemplaza secuencias inválidas (Go 1.13+)
clean := strings.ToValidUTF8(dirtyInput, "?")
// Cada secuencia inválida se reemplaza por "?"

clean = strings.ToValidUTF8(dirtyInput, "\uFFFD")
// Reemplaza con el carácter de reemplazo oficial: �

clean = strings.ToValidUTF8(dirtyInput, "")
// Elimina bytes inválidos (sin reemplazo)
```

### Relevancia SysAdmin — Logs con encoding mixto

```go
// Los logs del sistema pueden tener encoding mixto:
// - syslog suele ser ASCII/UTF-8
// - Algunas apps legacy usan Latin-1 (ISO 8859-1)
// - Datos binarios en logs de debug
// - Nombres de archivo con encoding incorrecto

func sanitizeLogLine(line []byte) string {
    if utf8.Valid(line) {
        return string(line)
    }

    // Intentar tratar como Latin-1 (cada byte = un code point)
    var b strings.Builder
    b.Grow(len(line))
    for _, by := range line {
        b.WriteRune(rune(by))  // Latin-1: byte value = Unicode code point para 0-255
    }
    return b.String()
}

// Limpiar nombre de archivo con caracteres problemáticos
func sanitizeFilename(name string) string {
    var b strings.Builder
    for _, r := range name {
        switch {
        case r == utf8.RuneError:
            b.WriteRune('_')  // reemplazar inválidos
        case unicode.IsControl(r):
            continue  // eliminar caracteres de control
        case r == '/' || r == '\x00':
            b.WriteRune('_')  // caracteres ilegales en paths
        default:
            b.WriteRune(r)
        }
    }
    return b.String()
}
```

---

## 11. Strings multilínea y templates con runes

### Raw strings con backticks — Seguro para Unicode

```go
// Backtick strings preservan todo literalmente
template := `
server {
    server_name café.example.com;
    # Soporte para dominios internacionales
    charset utf-8;
    location /日本語/ {
        proxy_pass http://backend;
    }
}
`
// No necesita escapes: \n, \t, ", etc. son literales
```

### `text/template` con Unicode

```go
import "text/template"

const reportTmpl = `
╔══════════════════════════════════════╗
║  Reporte de Sistema — {{.Hostname}}  ║
╠══════════════════════════════════════╣
{{range .Services -}}
║  {{printf "%-20s" .Name}} {{if .Up}}✓{{else}}✗{{end}} {{printf "%8s" .Latency}}  ║
{{end -}}
╚══════════════════════════════════════╝
`

type ReportData struct {
    Hostname string
    Services []ServiceInfo
}

type ServiceInfo struct {
    Name    string
    Up      bool
    Latency string
}

func generateReport(data ReportData) (string, error) {
    tmpl, err := template.New("report").Parse(reportTmpl)
    if err != nil {
        return "", err
    }
    var buf strings.Builder
    err = tmpl.Execute(&buf, data)
    return buf.String(), err
}
```

---

## 12. Conteo, truncamiento y padding con runes

### Contar caracteres reales (no bytes)

```go
import "unicode/utf8"

// Para validaciones que requieren longitud en caracteres
func validateUsername(name string) error {
    length := utf8.RuneCountInString(name)
    if length < 3 {
        return fmt.Errorf("username too short: %d characters (min 3)", length)
    }
    if length > 32 {
        return fmt.Errorf("username too long: %d characters (max 32)", length)
    }

    // Verificar caracteres permitidos
    for _, r := range name {
        if !unicode.IsLetter(r) && !unicode.IsDigit(r) && r != '_' && r != '-' {
            return fmt.Errorf("invalid character in username: %c (U+%04X)", r, r)
        }
    }
    return nil
}
```

### Truncar por caracteres (no bytes)

```go
// ✗ INCORRECTO — puede cortar un carácter UTF-8 por la mitad
func badTruncate(s string, maxBytes int) string {
    if len(s) <= maxBytes {
        return s
    }
    return s[:maxBytes]  // puede producir UTF-8 inválido
}

// ✓ CORRECTO — truncar por runes
func truncateRunes(s string, maxRunes int) string {
    runes := []rune(s)
    if len(runes) <= maxRunes {
        return s
    }
    return string(runes[:maxRunes])
}

// ✓ Truncar por bytes pero respetando fronteras de rune
func truncateBytes(s string, maxBytes int) string {
    if len(s) <= maxBytes {
        return s
    }
    // Retroceder hasta encontrar una frontera de rune válida
    for maxBytes > 0 && !utf8.RuneStart(s[maxBytes]) {
        maxBytes--
    }
    return s[:maxBytes]
}

// Uso en logs (truncar mensajes largos)
func logMessage(level, msg string) {
    truncated := truncateRunes(msg, 200)
    if len(msg) != len(truncated) {
        truncated += "..."
    }
    log.Printf("[%s] %s", level, truncated)
}
```

### Padding con awareness de runes

```go
import "unicode/utf8"

// fmt.Sprintf con %s padding opera en bytes, no runes
// Para texto UTF-8, necesitamos padding manual

func padRight(s string, width int) string {
    runeCount := utf8.RuneCountInString(s)
    if runeCount >= width {
        return s
    }
    return s + strings.Repeat(" ", width-runeCount)
}

func padLeft(s string, width int) string {
    runeCount := utf8.RuneCountInString(s)
    if runeCount >= width {
        return s
    }
    return strings.Repeat(" ", width-runeCount) + s
}

// Tabla alineada con caracteres Unicode
func printTable(rows [][]string, widths []int) {
    for _, row := range rows {
        for i, cell := range row {
            if i < len(widths) {
                fmt.Print(padRight(cell, widths[i]))
            } else {
                fmt.Print(cell)
            }
            fmt.Print("  ")
        }
        fmt.Println()
    }
}
```

---

## 13. Normalización Unicode — Peligros sutiles

Dos strings pueden **verse idénticas** pero tener representaciones UTF-8 distintas:

```go
// 'é' se puede representar de dos formas:
// 1. NFC (precompuesto): U+00E9 (un solo code point)
// 2. NFD (descompuesto): U+0065 U+0301 (e + combining acute accent)

s1 := "caf\u00E9"      // "café" — NFC (1 rune para é)
s2 := "cafe\u0301"      // "café" — NFD (e + combining accent)

fmt.Println(s1)          // café
fmt.Println(s2)          // café  — se ven igual
fmt.Println(s1 == s2)    // false — ¡NO son iguales!
fmt.Println(len(s1))     // 5 bytes
fmt.Println(len(s2))     // 6 bytes

// Contar runes también difiere
fmt.Println(utf8.RuneCountInString(s1))  // 4 runes
fmt.Println(utf8.RuneCountInString(s2))  // 5 runes (e + combining = 2)
```

### Solución: normalizar con `golang.org/x/text`

```go
import "golang.org/x/text/unicode/norm"

// Normalizar ambos a NFC antes de comparar
n1 := norm.NFC.String(s1)
n2 := norm.NFC.String(s2)
fmt.Println(n1 == n2)  // true — ahora son iguales

// O normalizar a NFD
d1 := norm.NFD.String(s1)
d2 := norm.NFD.String(s2)
fmt.Println(d1 == d2)  // true
```

### Relevancia SysAdmin — Nombres de archivo

```go
// macOS usa NFD para nombres de archivo
// Linux usa NFC (generalmente)
// Esto causa problemas con NFS, rsync, git, etc.

// Normalizar nombres de archivo para comparación consistente
func normalizeFilename(name string) string {
    return norm.NFC.String(name)
}

// Verificar si un path tiene caracteres que podrían causar problemas
func checkPathPortability(path string) []string {
    var warnings []string

    for _, r := range path {
        // Combining characters (marcas diacríticas separadas)
        if unicode.Is(unicode.Mn, r) {  // Mn = Mark, nonspacing
            warnings = append(warnings,
                fmt.Sprintf("combining character U+%04X at position — may cause cross-platform issues", r))
        }
        // Caracteres de ancho cero
        if r == '\u200B' || r == '\u200C' || r == '\u200D' || r == '\uFEFF' {
            warnings = append(warnings,
                fmt.Sprintf("zero-width character U+%04X — invisible but present", r))
        }
    }

    // Verificar NFC vs NFD
    if norm.NFC.String(path) != norm.NFD.String(path) {
        warnings = append(warnings,
            "path has different NFC and NFD normalizations — cross-platform issue")
    }

    return warnings
}
```

---

## 14. Case folding — Comparación insensible a mayúsculas

```go
// strings.EqualFold — comparación case-insensitive correcta para Unicode
fmt.Println(strings.EqualFold("hello", "HELLO"))     // true
fmt.Println(strings.EqualFold("café", "CAFÉ"))       // true
fmt.Println(strings.EqualFold("straße", "STRASSE"))   // true — ß ↔ SS

// ✗ No usar ToLower/ToUpper para comparar — no funciona con todos los scripts
// strings.ToLower("STRASSE") = "strasse" ≠ "straße"

// strings.ToLower/ToUpper para transformar (no comparar)
fmt.Println(strings.ToUpper("café"))    // "CAFÉ"
fmt.Println(strings.ToLower("MÜNCHEN")) // "münchen"

// strings.ToTitle — title case (primera letra de cada "palabra")
fmt.Println(strings.ToTitle("hello world"))  // "HELLO WORLD" (¡no lo que esperas!)
// ToTitle capitaliza TODOS los caracteres

// Para title case real, usar golang.org/x/text/cases
import "golang.org/x/text/cases"
import "golang.org/x/text/language"

caser := cases.Title(language.Spanish)
fmt.Println(caser.String("hola mundo"))  // "Hola Mundo"
```

---

## 15. Grapheme clusters — Más allá de runes

Un "carácter visible" a veces requiere múltiples runes (grapheme cluster):

```go
// Emojis con modificadores — un "carácter" visible = múltiples runes
emoji := "👨‍👩‍👧‍👦"  // familia emoji

fmt.Println(len(emoji))                          // 25 bytes
fmt.Println(utf8.RuneCountInString(emoji))        // 7 runes
// Pero visualmente es UN solo emoji

// Las 7 runes:
for _, r := range emoji {
    fmt.Printf("U+%04X ", r)
}
// U+1F468 U+200D U+1F469 U+200D U+1F467 U+200D U+1F466
// 👨 + ZWJ + 👩 + ZWJ + 👧 + ZWJ + 👦

// Flags — dos regional indicator symbols
flag := "🇪🇸"  // bandera de España
fmt.Println(utf8.RuneCountInString(flag))  // 2 runes (🇪 + 🇸)
// Pero visualmente es UNA bandera
```

```
┌──────────────────────────────────────────────────────────────┐
│        BYTES vs RUNES vs GRAPHEME CLUSTERS                   │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  String:  "Hello"   "café"    "👨‍👩‍👧‍👦"    "🇪🇸"           │
│  Bytes:     5         5         25         8                 │
│  Runes:     5         4          7         2                 │
│  Graphemes: 5         4          1         1                 │
│                                                              │
│  Go opera a nivel de bytes y runes de forma nativa           │
│  Para grapheme clusters necesitas:                           │
│    github.com/rivo/uniseg                                    │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

```go
// Para contar grapheme clusters correctamente:
import "github.com/rivo/uniseg"

text := "Hello, 👨‍👩‍👧‍👦! 🇪🇸"
fmt.Println(uniseg.GraphemeClusterCount(text))  // 11

// Iterar por grapheme clusters
gr := uniseg.NewGraphemes(text)
for gr.Next() {
    fmt.Printf("%q ", gr.Str())
}
// "H" "e" "l" "l" "o" "," " " "👨‍👩‍👧‍👦" "!" " " "🇪🇸"
```

---

## 16. Ejemplo SysAdmin: Procesador de logs multi-idioma

```go
package main

import (
    "bufio"
    "fmt"
    "os"
    "strings"
    "unicode"
    "unicode/utf8"
)

// LogStats recopila estadísticas de un archivo de log
type LogStats struct {
    TotalLines    int
    TotalBytes    int64
    TotalRunes    int
    ASCIILines    int
    NonASCIILines int
    InvalidUTF8   int
    MaxLineRunes  int
    MaxLineBytes  int
    CharCategories map[string]int
    ScriptsFound   map[string]int
}

func NewLogStats() *LogStats {
    return &LogStats{
        CharCategories: make(map[string]int),
        ScriptsFound:   make(map[string]int),
    }
}

// isASCII verifica si un string contiene solo ASCII
func isASCII(s string) bool {
    for i := 0; i < len(s); i++ {
        if s[i] > 127 {
            return false
        }
    }
    return true
}

// detectScript detecta el script Unicode de una rune
func detectScript(r rune) string {
    scripts := []struct {
        table *unicode.RangeTable
        name  string
    }{
        {unicode.Latin, "Latin"},
        {unicode.Cyrillic, "Cyrillic"},
        {unicode.Han, "Han (Chinese)"},
        {unicode.Hiragana, "Hiragana"},
        {unicode.Katakana, "Katakana"},
        {unicode.Arabic, "Arabic"},
        {unicode.Greek, "Greek"},
        {unicode.Devanagari, "Devanagari"},
        {unicode.Thai, "Thai"},
        {unicode.Korean, "Korean"},
    }

    for _, s := range scripts {
        if unicode.Is(s.table, r) {
            return s.name
        }
    }

    if r > 127 {
        return "Other"
    }
    return "ASCII"
}

// categorizeRune clasifica una rune en categoría legible
func categorizeRune(r rune) string {
    switch {
    case unicode.IsUpper(r):
        return "uppercase"
    case unicode.IsLower(r):
        return "lowercase"
    case unicode.IsDigit(r):
        return "digit"
    case unicode.IsSpace(r):
        return "whitespace"
    case unicode.IsPunct(r):
        return "punctuation"
    case unicode.IsSymbol(r):
        return "symbol"
    case unicode.IsControl(r):
        return "control"
    default:
        return "other"
    }
}

func analyzeFile(path string) (*LogStats, error) {
    file, err := os.Open(path)
    if err != nil {
        return nil, err
    }
    defer file.Close()

    stats := NewLogStats()
    scanner := bufio.NewScanner(file)

    // Incrementar buffer para líneas muy largas
    buf := make([]byte, 0, 64*1024)
    scanner.Buffer(buf, 1024*1024)

    for scanner.Scan() {
        line := scanner.Text()
        stats.TotalLines++
        stats.TotalBytes += int64(len(line)) + 1 // +1 por \n

        // Validar UTF-8
        if !utf8.ValidString(line) {
            stats.InvalidUTF8++
            continue
        }

        // ASCII vs non-ASCII
        if isASCII(line) {
            stats.ASCIILines++
        } else {
            stats.NonASCIILines++
        }

        // Contar runes y estadísticas por carácter
        runeCount := 0
        for _, r := range line {
            runeCount++
            stats.TotalRunes++

            cat := categorizeRune(r)
            stats.CharCategories[cat]++

            if !unicode.IsSpace(r) && !unicode.IsControl(r) {
                script := detectScript(r)
                stats.ScriptsFound[script]++
            }
        }

        if runeCount > stats.MaxLineRunes {
            stats.MaxLineRunes = runeCount
        }
        if len(line) > stats.MaxLineBytes {
            stats.MaxLineBytes = len(line)
        }
    }

    return stats, scanner.Err()
}

// truncateDisplay trunca un string para display respetando runes
func truncateDisplay(s string, maxRunes int) string {
    runes := []rune(s)
    if len(runes) <= maxRunes {
        return s
    }
    return string(runes[:maxRunes-3]) + "..."
}

func printStats(path string, stats *LogStats) {
    fmt.Println(strings.Repeat("═", 60))
    fmt.Printf("  Log Analysis: %s\n", truncateDisplay(path, 45))
    fmt.Println(strings.Repeat("═", 60))

    fmt.Printf("\n  Lines:            %d\n", stats.TotalLines)
    fmt.Printf("  Total bytes:      %d (%.2f KB)\n",
        stats.TotalBytes, float64(stats.TotalBytes)/1024)
    fmt.Printf("  Total runes:      %d\n", stats.TotalRunes)
    fmt.Printf("  ASCII-only lines: %d (%.1f%%)\n",
        stats.ASCIILines, float64(stats.ASCIILines)/float64(stats.TotalLines)*100)
    fmt.Printf("  Non-ASCII lines:  %d (%.1f%%)\n",
        stats.NonASCIILines, float64(stats.NonASCIILines)/float64(stats.TotalLines)*100)
    fmt.Printf("  Invalid UTF-8:    %d\n", stats.InvalidUTF8)
    fmt.Printf("  Max line (runes): %d\n", stats.MaxLineRunes)
    fmt.Printf("  Max line (bytes): %d\n", stats.MaxLineBytes)

    if stats.TotalRunes > 0 {
        avgBytesPerRune := float64(stats.TotalBytes) / float64(stats.TotalRunes)
        fmt.Printf("  Avg bytes/rune:   %.2f\n", avgBytesPerRune)
    }

    fmt.Println("\n  Character Categories:")
    for cat, count := range stats.CharCategories {
        pct := float64(count) / float64(stats.TotalRunes) * 100
        fmt.Printf("    %-15s %8d  (%5.1f%%)\n", cat, count, pct)
    }

    if len(stats.ScriptsFound) > 1 || (len(stats.ScriptsFound) == 1 && stats.ScriptsFound["ASCII"] == 0) {
        fmt.Println("\n  Unicode Scripts Found:")
        for script, count := range stats.ScriptsFound {
            fmt.Printf("    %-20s %8d\n", script, count)
        }
    }

    fmt.Println(strings.Repeat("═", 60))
}

func main() {
    if len(os.Args) < 2 {
        fmt.Fprintf(os.Stderr, "Usage: %s <logfile> [logfile...]\n", os.Args[0])
        os.Exit(1)
    }

    for _, path := range os.Args[1:] {
        stats, err := analyzeFile(path)
        if err != nil {
            fmt.Fprintf(os.Stderr, "Error analyzing %s: %v\n", path, err)
            continue
        }
        printStats(path, stats)
    }
}
```

---

## 17. Comparación: Go vs C vs Rust

| Aspecto | Go | C | Rust |
|---|---|---|---|
| Tipo carácter | `rune` (int32) | `char` (1 byte) / `wchar_t` | `char` (4 bytes, Unicode scalar) |
| Encoding string | UTF-8 (por convención) | Sin encoding definido | UTF-8 (garantizado) |
| Indexación string | `s[i]` = byte | `s[i]` = byte | `s.as_bytes()[i]` = byte |
| Iteración chars | `for _, r := range s` | Manual con `mbrtowc()` | `s.chars()` |
| Longitud bytes | `len(s)` | `strlen(s)` | `s.len()` |
| Longitud chars | `utf8.RuneCountInString(s)` | `mbstowcs(NULL, s, 0)` | `s.chars().count()` |
| Builder | `strings.Builder` | `strcat()`/manual | `String::new()` + `push_str` |
| Validación UTF-8 | `utf8.ValidString(s)` | No estándar | Garantizado en compile-time |
| Char inválido | `utf8.RuneError` (U+FFFD) | Depende de locale | Imposible (`char` siempre válido) |
| Unicode classify | `unicode.IsLetter(r)` | `iswalpha()` (locale-dependent) | `r.is_alphabetic()` |
| Normalización | `x/text/unicode/norm` | ICU library | `unicode-normalization` crate |
| Grapheme clusters | `rivo/uniseg` (externo) | ICU library | `unicode-segmentation` crate |

---

## 18. Errores comunes

| # | Error | Código | Solución |
|---|---|---|---|
| 1 | `len(s)` = caracteres | `len("café")` retorna 5, no 4 | `utf8.RuneCountInString(s)` |
| 2 | `s[i]` = carácter | `"café"[3]` retorna 0xC3, no 'é' | `[]rune(s)[3]` o `for range` |
| 3 | Slice corta rune | `"café"[:4]` corta 'é' por la mitad | Truncar respetando fronteras |
| 4 | `string(42)` = "42" | `string(42)` produce `"*"` | `strconv.Itoa(42)` |
| 5 | `+` en loop | `s += x` en loop es O(n²) | `strings.Builder` |
| 6 | NFC ≠ NFD | `"café"` NFC ≠ `"café"` NFD | Normalizar antes de comparar |
| 7 | ToLower para comparar | `ToLower(a) == ToLower(b)` falla con ß/SS | `strings.EqualFold(a, b)` |
| 8 | Rune = grapheme | `len([]rune("👨‍👩‍👧"))` = 5, no 1 | Usar `rivo/uniseg` |
| 9 | UTF-8 inválido | No validar input externo | `utf8.ValidString()` + sanitizar |
| 10 | Builder no crece | Muchas escrituras pequeñas | `b.Grow(estimatedSize)` |
| 11 | Buffer vs Builder | Usar Buffer cuando no necesitas io.Reader | Builder es más eficiente para string puro |
| 12 | Range índice = rune pos | `for i, r := range s` — `i` es byte pos | Convertir a `[]rune` si necesitas rune pos |

---

## 19. Ejercicios

### Ejercicio 1: Predicción bytes vs runes
```go
package main

import (
    "fmt"
    "unicode/utf8"
)

func main() {
    strings := []string{
        "Hello",
        "Hola",
        "Привет",
        "こんにちは",
        "🎉🎊🎈",
        "",
        "café",
    }

    for _, s := range strings {
        fmt.Printf("%-12s bytes=%-3d runes=%-3d\n",
            s, len(s), utf8.RuneCountInString(s))
    }
}
```
**Predicción**: Calcula bytes y runes para cada string. ¿Cuántos bytes ocupa cada carácter cirílico? ¿Y cada carácter japonés? ¿Y cada emoji?

### Ejercicio 2: Predicción de indexación
```go
package main

import "fmt"

func main() {
    s := "año"

    fmt.Printf("len=%d\n", len(s))
    fmt.Printf("s[0]=%d (%c)\n", s[0], s[0])
    fmt.Printf("s[1]=%d (0x%02X)\n", s[1], s[1])
    fmt.Printf("s[2]=%d (0x%02X)\n", s[2], s[2])
    fmt.Printf("s[3]=%d (%c)\n", s[3], s[3])

    for i, r := range s {
        fmt.Printf("range: i=%d r=%c (U+%04X)\n", i, r, r)
    }
}
```
**Predicción**: ¿Cuántos bytes tiene "año"? ¿Qué valor tiene `s[1]`? ¿Por qué el `range` salta del índice 1 al 3?

### Ejercicio 3: Predicción de slicing
```go
package main

import "fmt"

func main() {
    s := "naïve"

    fmt.Println(s[:2])   // ?
    fmt.Println(s[:3])   // ?
    fmt.Println(s[:4])   // ?

    runes := []rune(s)
    fmt.Println(string(runes[:3]))  // ?
}
```
**Predicción**: ¿Qué imprime cada línea? ¿Cuál produce output incorrecto/corrupto?

### Ejercicio 4: Builder vs concatenación
```go
package main

import (
    "fmt"
    "strings"
)

func withPlus(n int) string {
    s := ""
    for i := 0; i < n; i++ {
        s += "x"
    }
    return s
}

func withBuilder(n int) string {
    var b strings.Builder
    for i := 0; i < n; i++ {
        b.WriteByte('x')
    }
    return b.String()
}

func main() {
    s1 := withPlus(10)
    s2 := withBuilder(10)
    fmt.Println(s1 == s2)  // ?
    fmt.Println(len(s1))   // ?
}
```
**Predicción**: ¿Son iguales? Si llamamos con `n=1000000`, ¿cuál es más rápido y por qué?

### Ejercicio 5: Clasificación Unicode
```go
package main

import (
    "fmt"
    "unicode"
)

func main() {
    runes := []rune{'A', 'a', '7', ' ', '.', 'ñ', 'Ñ', '世', '٣', '🚀', '\n', '\t'}

    for _, r := range runes {
        fmt.Printf("'%c' Letter=%t Digit=%t Upper=%t Lower=%t Space=%t Print=%t\n",
            r,
            unicode.IsLetter(r),
            unicode.IsDigit(r),
            unicode.IsUpper(r),
            unicode.IsLower(r),
            unicode.IsSpace(r),
            unicode.IsPrint(r),
        )
    }
}
```
**Predicción**: ¿Es '世' una letra? ¿Es '٣' un dígito? ¿Es '🚀' imprimible? ¿Es '\n' un espacio?

### Ejercicio 6: EqualFold vs ToLower
```go
package main

import (
    "fmt"
    "strings"
)

func main() {
    pairs := [][2]string{
        {"hello", "HELLO"},
        {"café", "CAFÉ"},
        {"straße", "STRASSE"},
        {"İstanbul", "istanbul"},  // Turkish İ vs i
    }

    for _, p := range pairs {
        lower := strings.ToLower(p[0]) == strings.ToLower(p[1])
        fold := strings.EqualFold(p[0], p[1])
        fmt.Printf("%-12s vs %-12s ToLower=%t EqualFold=%t\n",
            p[0], p[1], lower, fold)
    }
}
```
**Predicción**: ¿Cuáles tienen resultados diferentes entre ToLower y EqualFold?

### Ejercicio 7: Validador de hostname RFC 952/1123
Implementa `ValidateHostname(s string) error` que:
1. Verifique longitud total 1-253 caracteres (runes)
2. Cada label (separada por `.`) tenga 1-63 caracteres
3. Solo contenga letras ASCII, dígitos, y guión (NO Unicode)
4. No empiece ni termine con guión
5. Contenga al menos una letra

### Ejercicio 8: Contador de palabras multi-idioma
Implementa `WordCount(text string) map[string]int` que:
1. Separe por espacios Unicode (`unicode.IsSpace`)
2. Normalice a minúsculas con `strings.ToLower`
3. Elimine puntuación con `unicode.IsPunct`
4. Cuente frecuencia de cada palabra
5. Funcione con texto en cualquier idioma

### Ejercicio 9: Sanitizador de input
Implementa `SanitizeInput(s string) string` que:
1. Valide UTF-8, reemplace secuencias inválidas con `�`
2. Elimine caracteres de control excepto `\n` y `\t`
3. Elimine caracteres de ancho cero (ZWJ, ZWNJ, BOM, ZWSP)
4. Normalice a NFC (si tienes `golang.org/x/text`)
5. Trunce a máximo 1000 caracteres (runes)
6. Retorne el string limpio

### Ejercicio 10: Log file encoding detector
Escribe un programa que analice un archivo y:
1. Cuente bytes válidos ASCII (0-127)
2. Cuente secuencias UTF-8 válidas multi-byte
3. Cuente bytes que no forman UTF-8 válido
4. Determine si el archivo es: "ASCII puro", "UTF-8", "UTF-8 con errores", o "binario"
5. Si tiene errores UTF-8, muestre las posiciones (byte offset) de los bytes inválidos

---

## 20. Resumen

```
┌──────────────────────────────────────────────────────────────┐
│              TIPO RUNE EN GO — RESUMEN                       │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  FUNDAMENTOS                                                 │
│  ├─ rune = int32 = Unicode code point                        │
│  ├─ byte = uint8 = un byte individual                        │
│  ├─ string = secuencia de bytes (UTF-8 por convención)       │
│  ├─ len(s) = bytes, utf8.RuneCountInString(s) = runes        │
│  └─ s[i] = byte, for _, r := range s → r = rune             │
│                                                              │
│  UTF-8 ENCODING                                              │
│  ├─ 1 byte: ASCII (U+0000–U+007F)                           │
│  ├─ 2 bytes: Latin, Cyrillic, etc. (U+0080–U+07FF)          │
│  ├─ 3 bytes: CJK, BMP (U+0800–U+FFFF)                       │
│  └─ 4 bytes: Emojis, SMP (U+10000–U+10FFFF)                 │
│                                                              │
│  PAQUETES CLAVE                                              │
│  ├─ unicode/utf8: RuneCountInString, ValidString, DecodeRune │
│  ├─ unicode: IsLetter, IsDigit, IsSpace, IsUpper, ToLower    │
│  ├─ strings: Builder, EqualFold, ToValidUTF8                 │
│  └─ bytes: Buffer (para io.Reader/io.Writer)                 │
│                                                              │
│  CONSTRUCCIÓN DE STRINGS                                     │
│  ├─ strings.Builder: construir strings eficientemente        │
│  ├─ strings.Join: unir slices                                │
│  ├─ bytes.Buffer: cuando necesitas io.Reader/Writer          │
│  └─ Grow(n) para pre-alocar y evitar realocaciones           │
│                                                              │
│  PELIGROS                                                    │
│  ├─ len(s) ≠ número de caracteres                            │
│  ├─ Slicing puede cortar runes multi-byte                    │
│  ├─ NFC ≠ NFD — normalizar antes de comparar                │
│  ├─ ToLower no es universal — usar EqualFold                 │
│  ├─ Rune ≠ grapheme cluster (emojis compuestos)              │
│  └─ Input externo puede no ser UTF-8 válido                  │
│                                                              │
│  SYSADMIN: validar UTF-8 en logs, sanitizar nombres de       │
│  archivo, normalizar para comparación cross-platform,        │
│  detectar encoding, procesar texto multi-idioma              │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: C03 - Control de Flujo — S01: Condicionales y Bucles (if/else, for, switch, labels/goto)
