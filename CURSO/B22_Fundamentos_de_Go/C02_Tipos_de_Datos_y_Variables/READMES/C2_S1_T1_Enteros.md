# T01 — Enteros

## 1. Sistema de tipos de Go

Go es un lenguaje con **tipado estático y fuerte**. Cada variable tiene
un tipo fijo determinado en tiempo de compilación, y no hay conversiones
implícitas entre tipos. Esto previene una categoría entera de bugs.

```
    ┌──────────────────────────────────────────────────────────────┐
    │               Sistema de tipos numéricos de Go               │
    │                                                              │
    │  ENTEROS CON SIGNO         ENTEROS SIN SIGNO                 │
    │  ┌──────────────────┐     ┌──────────────────┐               │
    │  │ int8    (-128…127)│     │ uint8   (0…255)  │ = byte       │
    │  │ int16 (-32768…)   │     │ uint16  (0…65535)│               │
    │  │ int32 (-2^31…)    │     │ uint32  (0…2^32) │ = rune       │
    │  │ int64 (-2^63…)    │     │ uint64  (0…2^64) │               │
    │  └──────────────────┘     └──────────────────┘               │
    │                                                              │
    │  DEPENDIENTES DE PLATAFORMA                                  │
    │  ┌──────────────────────────────────────────────┐            │
    │  │ int      → 32 bits en 32-bit OS              │            │
    │  │            64 bits en 64-bit OS              │            │
    │  │ uint     → igual que int pero sin signo      │            │
    │  │ uintptr  → tamaño para almacenar un puntero  │            │
    │  └──────────────────────────────────────────────┘            │
    │                                                              │
    │  ALIASES                                                     │
    │  ┌──────────────────────────────────────────────┐            │
    │  │ byte  = uint8   (datos binarios, ASCII)      │            │
    │  │ rune  = int32   (Unicode code point)         │            │
    │  └──────────────────────────────────────────────┘            │
    └──────────────────────────────────────────────────────────────┘
```

### 1.1 ¿Por qué tantos tipos enteros?

```bash
# En Python: solo hay "int" (tamaño ilimitado)
# En JavaScript: solo hay "number" (float64 internamente)
# En Go: 10+ tipos de enteros

# ¿Por qué?
#
# 1. CONTROL DE MEMORIA:
#    int8 usa 1 byte, int64 usa 8 bytes.
#    En un slice de 1 millón de elementos:
#    []int8  = 1 MB
#    []int64 = 8 MB
#    Para un SysAdmin procesando logs o métricas, esto importa.
#
# 2. INTEROPERABILIDAD CON SISTEMAS:
#    Las syscalls de Linux usan tipos de tamaño fijo.
#    Los protocolos de red usan uint16 para puertos,
#    uint32 para IPs, etc.
#
# 3. PREVENCIÓN DE BUGS:
#    Si una función espera uint16 (0-65535) y le pasas -1,
#    el compilador te detiene. En Python, solo falla en runtime.
```

---

## 2. Tipos enteros con signo: int8, int16, int32, int64

Los enteros con signo pueden almacenar valores negativos y positivos.
Usan **complemento a dos** (igual que C y Rust).

```go
package main

import (
    "fmt"
    "math"
)

func main() {
    // Declaración explícita con tipo:
    var a int8 = 127         // máximo de int8
    var b int16 = 32767      // máximo de int16
    var c int32 = 2147483647 // máximo de int32
    var d int64 = 9223372036854775807 // máximo de int64

    fmt.Println(a, b, c, d)

    // Usando constantes de math:
    fmt.Println("int8  range:", math.MinInt8, "to", math.MaxInt8)
    fmt.Println("int16 range:", math.MinInt16, "to", math.MaxInt16)
    fmt.Println("int32 range:", math.MinInt32, "to", math.MaxInt32)
    fmt.Println("int64 range:", math.MinInt64, "to", math.MaxInt64)
}
```

### 2.1 Tabla de rangos

```
    ┌──────────┬───────┬──────────────────────────────────────────┐
    │ Tipo     │ Bytes │ Rango                                    │
    ├──────────┼───────┼──────────────────────────────────────────┤
    │ int8     │   1   │ -128 a 127                               │
    │ int16    │   2   │ -32,768 a 32,767                         │
    │ int32    │   4   │ -2,147,483,648 a 2,147,483,647           │
    │ int64    │   8   │ -9,223,372,036,854,775,808               │
    │          │       │  a 9,223,372,036,854,775,807             │
    ├──────────┼───────┼──────────────────────────────────────────┤
    │ int      │  4/8  │ = int32 en 32-bit, int64 en 64-bit      │
    └──────────┴───────┴──────────────────────────────────────────┘
```

### 2.2 Cuándo usar cada tipo con signo

```go
// Regla general: usa "int" a menos que necesites un tamaño específico.

// int — el tipo por defecto para enteros:
count := 42          // tipo inferido: int
for i := 0; i < 10; i++ { }  // i es int

// int8 — raro, casi solo para protocolos:
var signedByte int8 = -100

// int16 — raro, algunos protocolos legacy:
var portOffset int16 = -1000

// int32 — rune es alias de int32, y algunos campos de struct:
var r rune = '日'    // Unicode code point
var pid int32 = 1234 // PIDs en Linux son int32

// int64 — timestamps, file sizes, counters grandes:
var fileSize int64 = 4294967296   // 4 GB, no cabe en int32
var unixNano int64 = 1702656000000000000 // nanosegundos
```

---

## 3. Tipos enteros sin signo: uint8, uint16, uint32, uint64

Los enteros sin signo solo almacenan valores >= 0.
Todo el rango de bits se usa para magnitud (no hay bit de signo).

```go
package main

import (
    "fmt"
    "math"
)

func main() {
    var a uint8 = 255                    // máximo de uint8
    var b uint16 = 65535                 // máximo de uint16
    var c uint32 = 4294967295            // máximo de uint32
    var d uint64 = 18446744073709551615  // máximo de uint64

    fmt.Println(a, b, c, d)

    fmt.Println("uint8  max:", math.MaxUint8)
    fmt.Println("uint16 max:", math.MaxUint16)
    fmt.Println("uint32 max:", math.MaxUint32)
    fmt.Println("uint64 max:", uint64(math.MaxUint64))
}
```

### 3.1 Tabla de rangos

```
    ┌──────────┬───────┬──────────────────────────────────────────┐
    │ Tipo     │ Bytes │ Rango                                    │
    ├──────────┼───────┼──────────────────────────────────────────┤
    │ uint8    │   1   │ 0 a 255                                  │
    │ uint16   │   2   │ 0 a 65,535                               │
    │ uint32   │   4   │ 0 a 4,294,967,295                        │
    │ uint64   │   8   │ 0 a 18,446,744,073,709,551,615           │
    ├──────────┼───────┼──────────────────────────────────────────┤
    │ uint     │  4/8  │ = uint32 en 32-bit, uint64 en 64-bit     │
    └──────────┴───────┴──────────────────────────────────────────┘
```

### 3.2 Cuándo usar tipos sin signo

```go
// uint8 / byte — datos binarios, colores RGB, protocolos:
var r, g, b uint8 = 255, 128, 0  // color naranja
data := []byte{0x48, 0x65, 0x6c} // "Hel" en ASCII
var mask byte = 0xFF

// uint16 — puertos de red, protocolos:
var port uint16 = 8080
var etherType uint16 = 0x0800  // IPv4

// uint32 — direcciones IPv4, UIDs/GIDs en Linux:
var ip uint32 = 0xC0A80001     // 192.168.0.1
var uid uint32 = 1000          // UID de usuario
var gid uint32 = 1000          // GID de grupo
var permissions uint32 = 0o755 // permisos de archivo

// uint64 — inodes, tamaños de archivo, contadores:
var inode uint64 = 12345678
var diskSize uint64 = 1099511627776 // 1 TB en bytes

// uint — longitud de arrays/slices (len() devuelve int, no uint,
// pero en structs donde el valor nunca es negativo):
var bufferSize uint = 4096
```

---

## 4. int y uint — los tipos dependientes de plataforma

`int` y `uint` tienen un tamaño que depende de la plataforma:
32 bits en sistemas de 32 bits, 64 bits en sistemas de 64 bits.

```go
package main

import (
    "fmt"
    "math"
    "runtime"
    "unsafe"
)

func main() {
    fmt.Printf("Platform: %s/%s\n", runtime.GOOS, runtime.GOARCH)
    fmt.Printf("sizeof(int):     %d bytes\n", unsafe.Sizeof(int(0)))
    fmt.Printf("sizeof(uint):    %d bytes\n", unsafe.Sizeof(uint(0)))
    fmt.Printf("sizeof(uintptr): %d bytes\n", unsafe.Sizeof(uintptr(0)))

    // En Linux amd64 (64-bit):
    // sizeof(int):     8 bytes
    // sizeof(uint):    8 bytes
    // sizeof(uintptr): 8 bytes

    // En ARM 32-bit (ej: Raspberry Pi con OS 32-bit):
    // sizeof(int):     4 bytes
    // sizeof(uint):    4 bytes
    // sizeof(uintptr): 4 bytes

    fmt.Printf("MaxInt: %d\n", math.MaxInt)
    // En 64-bit: 9223372036854775807
    // En 32-bit: 2147483647
}
```

### 4.1 ¿Cuándo importa la diferencia de tamaño?

```go
// La MAYORÍA del tiempo no importa. Usa int y no pienses más.

// PERO importa cuando:

// 1. Serializas datos que cruzan plataformas:
type Message struct {
    ID   int    // ← 4 bytes en 32-bit, 8 bytes en 64-bit
    Size int    // ← ¡El binario serializado tiene diferente tamaño!
}
// Solución: usar tipos de tamaño fijo para protocolos:
type Message struct {
    ID   int64  // siempre 8 bytes
    Size int64  // siempre 8 bytes
}

// 2. Cross-compilas para ARM 32-bit:
var fileSize int = 5000000000  // 5 GB
// En 64-bit: funciona (int = int64)
// En 32-bit: OVERFLOW (int = int32, máx ~2.1 GB)
// Solución: usar int64 para tamaños de archivo

// 3. Calculas con valores grandes:
total := count * price  // si count y price son int32 en 32-bit,
                        // el resultado puede desbordarse
```

### 4.2 Reglas de uso de int

```go
// REGLA 1: Usa "int" por defecto para todo.
count := 42        // int
index := 0         // int
for i := range s { } // i es int

// REGLA 2: len(), cap(), make() usan int.
s := make([]int, 10) // len(s) devuelve int, no uint
n := len(s)           // n es int

// REGLA 3: Usa int64 cuando los valores pueden ser grandes.
var fileSize int64   // archivos pueden ser >2GB
var timestamp int64  // nanosegundos desde epoch

// REGLA 4: Usa uint solo cuando semánticamente no puede ser negativo
// Y el protocolo/API lo requiere.
var port uint16 = 8080   // puertos son 0-65535
var permissions os.FileMode = 0o644  // FileMode es uint32

// REGLA 5: NUNCA uses uint para len/index.
// Go usa int para lengths e indices, no uint.
// Esto es una decisión de diseño para evitar underflow:
// var n uint = len(slice)  // conversión innecesaria
// if n - 1 < 0 { }        // NUNCA será < 0 (uint), bug sutil
```

---

## 5. byte y rune — los alias importantes

`byte` y `rune` son alias de tipo, no tipos nuevos. El compilador
los trata exactamente igual que uint8 e int32 respectivamente.

```go
package main

import "fmt"

func main() {
    // byte = uint8 (datos binarios, ASCII):
    var b byte = 'A'      // valor: 65
    fmt.Printf("byte: %c = %d = 0x%02X\n", b, b, b)
    // byte: A = 65 = 0x41

    // rune = int32 (Unicode code point):
    var r rune = '日'      // valor: 26085
    fmt.Printf("rune: %c = %d = U+%04X\n", r, r, r)
    // rune: 日 = 26085 = U+65E5

    // Son el mismo tipo:
    var x uint8 = 42
    var y byte = x        // ✓ funciona — son el mismo tipo
    _ = y

    var a int32 = 42
    var c rune = a        // ✓ funciona — son el mismo tipo
    _ = c

    // Cuándo usar byte vs uint8:
    // byte  → cuando trabajas con texto ASCII o datos binarios
    // uint8 → cuando trabajas con números (colores, flags)
    //
    // Cuándo usar rune vs int32:
    // rune  → cuando trabajas con caracteres Unicode
    // int32 → cuando trabajas con números que necesitan 32 bits
    //
    // Es solo semántica — no hay diferencia en runtime.
}
```

### 5.1 byte en la práctica de SysAdmin

```go
// Lectura de archivos binarios:
data, err := os.ReadFile("/etc/hostname")
// data es []byte

// Lectura de comandos:
output, err := exec.Command("uname", "-r").Output()
// output es []byte

// Conversión a string:
hostname := string(data)
kernelVersion := string(output)

// Conversión de string a []byte:
payload := []byte("Hello, World!")

// Procesamiento byte a byte:
for i, b := range data {
    if b == '\n' {
        fmt.Printf("Newline at position %d\n", i)
    }
}

// Comparación de bytes:
import "bytes"
if bytes.Equal(data1, data2) { }
if bytes.Contains(data, []byte("error")) { }
```

---

## 6. uintptr — el tipo para punteros como números

`uintptr` es un tipo entero sin signo lo suficientemente grande
para almacenar una dirección de memoria. Se usa casi exclusivamente
con el paquete `unsafe`.

```go
package main

import (
    "fmt"
    "unsafe"
)

func main() {
    x := 42
    ptr := &x

    // Convertir puntero a uintptr:
    addr := uintptr(unsafe.Pointer(ptr))
    fmt.Printf("Address of x: 0x%X\n", addr)
    // Address of x: 0xC0000B4008

    fmt.Printf("sizeof(uintptr): %d bytes\n", unsafe.Sizeof(addr))
    // 8 bytes en 64-bit
    // 4 bytes en 32-bit
}
```

```bash
# ¿Cuándo necesitas uintptr como SysAdmin?

# Casi NUNCA en código normal.

# Usos legítimos:
# 1. Interactuar con syscalls que esperan punteros como números
# 2. Aritmar de punteros con unsafe (acceder a campos de struct por offset)
# 3. Pasar a C via cgo
# 4. Algunas operaciones del paquete reflect

# ⚠️ IMPORTANTE:
# uintptr NO mantiene viva la referencia para el garbage collector.
# Si conviertes un puntero a uintptr y el GC recoge el objeto,
# el uintptr apunta a memoria inválida.
# NUNCA almacenes un uintptr en una variable por más de una expresión.
```

---

## 7. Literales enteros

Go soporta varias formas de escribir literales enteros.

```go
package main

import "fmt"

func main() {
    // Decimal (base 10):
    dec := 42
    dec2 := 1_000_000   // underscores como separadores (Go 1.13+)

    // Hexadecimal (base 16) — prefijo 0x o 0X:
    hex := 0xFF         // 255
    hex2 := 0x00_FF_00  // con separadores: 65280

    // Octal (base 8) — prefijo 0o o 0O (Go 1.13+):
    oct := 0o755        // permisos de archivo: rwxr-xr-x
    oct2 := 0o644       // permisos: rw-r--r--

    // Octal legacy — prefijo 0 (como en C):
    octLegacy := 0755   // funciona, pero 0o755 es más claro

    // Binario (base 2) — prefijo 0b o 0B (Go 1.13+):
    bin := 0b1010_0001  // 161
    bin2 := 0b11111111  // 255

    fmt.Printf("dec:  %d\n", dec)       // 42
    fmt.Printf("hex:  %d (0x%X)\n", hex, hex)  // 255 (0xFF)
    fmt.Printf("oct:  %d (0o%o)\n", oct, oct)  // 493 (0o755)
    fmt.Printf("bin:  %d (0b%b)\n", bin, bin)  // 161 (0b10100001)

    _ = dec2
    _ = hex2
    _ = oct2
    _ = octLegacy
    _ = bin2
}
```

### 7.1 Octal para permisos de archivo — el uso más importante para SysAdmin

```go
package main

import "os"

func main() {
    // Permisos de archivo en Go usan octal:
    os.Mkdir("data", 0o755)          // rwxr-xr-x
    os.WriteFile("config.yaml", data, 0o644)  // rw-r--r--
    os.Chmod("script.sh", 0o755)     // rwxr-xr-x
    os.Chmod("secret.key", 0o600)    // rw-------
    os.MkdirAll("/var/lib/myapp", 0o750) // rwxr-x---

    // os.FileMode es uint32 (usa los 9 bits bajos para rwx):
    //
    //  owner   group   other
    //  rwx     r-x     r-x
    //  111     101     101
    //  7       5       5      → 0o755
    //
    //  rw-     r--     r--
    //  110     100     100
    //  6       4       4      → 0o644

    // Permisos comunes para SysAdmin:
    // 0o755 → directorios, scripts ejecutables
    // 0o644 → archivos de configuración (no sensibles)
    // 0o600 → archivos sensibles (claves, tokens)
    // 0o700 → directorios sensibles (~/.ssh/)
    // 0o750 → directorios de servicio (/var/lib/myapp/)
    // 0o640 → config con grupo de acceso (/etc/myapp/config.yaml)

    // ⚠️ SIEMPRE usa 0o prefijo (octal explícito).
    // 0755 funciona pero 0o755 es más claro y evita ambigüedad.
    // Un SysAdmin que lea tu código sabrá inmediatamente que es un permiso.
}
```

### 7.2 Hexadecimal para redes y protocolos

```go
package main

import (
    "fmt"
    "net"
)

func main() {
    // Direcciones IP como uint32:
    var ip uint32 = 0xC0A80001  // 192.168.0.1
    //                C0 = 192
    //                A8 = 168
    //                00 = 0
    //                01 = 1

    fmt.Printf("IP: %d.%d.%d.%d\n",
        (ip>>24)&0xFF, (ip>>16)&0xFF, (ip>>8)&0xFF, ip&0xFF)
    // IP: 192.168.0.1

    // Máscaras de subred:
    var mask uint32 = 0xFFFFFF00  // /24 = 255.255.255.0
    network := ip & mask
    fmt.Printf("Network: 0x%08X\n", network)  // 0xC0A80000

    // EtherTypes:
    var etherIPv4 uint16 = 0x0800
    var etherIPv6 uint16 = 0x86DD
    var etherARP  uint16 = 0x0806

    // TCP flags:
    var SYN byte = 0x02
    var ACK byte = 0x10
    var SYNACK byte = SYN | ACK  // 0x12

    // HTTP status codes en hex (raro pero a veces en logs):
    var ok uint16 = 0xC8  // 200

    _ = etherIPv4; _ = etherIPv6; _ = etherARP
    _ = SYNACK; _ = ok

    // net.IP usa []byte internamente:
    ipAddr := net.IPv4(192, 168, 0, 1)
    fmt.Println("IP:", ipAddr)  // 192.168.0.1
}
```

### 7.3 Binario para flags y bitmasks

```go
package main

import "fmt"

// Permisos como bitmask (estilo Linux):
const (
    Read    = 0b100  // 4
    Write   = 0b010  // 2
    Execute = 0b001  // 1
)

// Flags de configuración:
const (
    FlagVerbose  = 1 << 0  // 0b0001 = 1
    FlagDebug    = 1 << 1  // 0b0010 = 2
    FlagDryRun   = 1 << 2  // 0b0100 = 4
    FlagForce    = 1 << 3  // 0b1000 = 8
)

func main() {
    // Combinar permisos:
    ownerPerms := Read | Write | Execute  // 0b111 = 7
    groupPerms := Read | Execute          // 0b101 = 5
    otherPerms := Read                    // 0b100 = 4
    fmt.Printf("Permissions: %o%o%o\n", ownerPerms, groupPerms, otherPerms)
    // Permissions: 754

    // Verificar flags:
    flags := FlagVerbose | FlagDryRun  // 0b0101 = 5

    if flags&FlagVerbose != 0 {
        fmt.Println("Verbose mode enabled")
    }
    if flags&FlagDebug != 0 {
        fmt.Println("Debug mode enabled")
    }
    if flags&FlagDryRun != 0 {
        fmt.Println("Dry run mode enabled")
    }
    // Output:
    // Verbose mode enabled
    // Dry run mode enabled
}
```

---

## 8. Operaciones aritméticas

```go
package main

import "fmt"

func main() {
    a, b := 17, 5

    // Aritméticas básicas:
    fmt.Println(a + b)   // 22   suma
    fmt.Println(a - b)   // 12   resta
    fmt.Println(a * b)   // 85   multiplicación
    fmt.Println(a / b)   // 3    división entera (trunca hacia cero)
    fmt.Println(a % b)   // 2    módulo (resto)

    // ⚠️ División entera trunca hacia cero:
    fmt.Println(17 / 5)   //  3  (no 3.4)
    fmt.Println(-17 / 5)  // -3  (no -4, trunca hacia cero)

    // ⚠️ Módulo con negativos:
    fmt.Println(17 % 5)   //  2
    fmt.Println(-17 % 5)  // -2  (el signo sigue al dividendo)

    // Operadores de asignación compuesta:
    x := 10
    x += 5   // x = x + 5 = 15
    x -= 3   // x = x - 3 = 12
    x *= 2   // x = x * 2 = 24
    x /= 4   // x = x / 4 = 6
    x %= 4   // x = x % 4 = 2
    fmt.Println(x)  // 2

    // Go NO tiene operadores ++ y -- como expresiones.
    // Solo como statements (instrucciones):
    x++      // ✓ x = x + 1 (statement)
    x--      // ✓ x = x - 1 (statement)
    // y := x++ // ✗ NO compila — ++ no es una expresión
}
```

### 8.1 Operaciones de bits

```go
package main

import "fmt"

func main() {
    a := uint8(0b1100_1010)  // 202
    b := uint8(0b1010_0110)  // 166

    // AND — solo bits donde ambos son 1:
    fmt.Printf("AND:  %08b\n", a&b)   // 10000010 (130)

    // OR — bits donde al menos uno es 1:
    fmt.Printf("OR:   %08b\n", a|b)   // 11101110 (238)

    // XOR — bits donde exactamente uno es 1:
    fmt.Printf("XOR:  %08b\n", a^b)   // 01101100 (108)

    // AND NOT (bit clear) — limpiar bits de b en a:
    fmt.Printf("ANDN: %08b\n", a&^b)  // 01001000 (72)

    // Left shift — multiplicar por 2^n:
    fmt.Printf("<<2:  %08b\n", a<<2)  // 00101000 (truncado a 8 bits)

    // Right shift — dividir por 2^n:
    fmt.Printf(">>2:  %08b\n", a>>2)  // 00110010 (50)

    // Uso práctico — extraer bytes de un uint32:
    var ip uint32 = 0xC0A80001  // 192.168.0.1
    octet1 := uint8(ip >> 24)   // 192
    octet2 := uint8(ip >> 16)   // 168
    octet3 := uint8(ip >> 8)    // 0
    octet4 := uint8(ip)         // 1
    fmt.Printf("IP: %d.%d.%d.%d\n", octet1, octet2, octet3, octet4)
}
```

### 8.2 Operadores de asignación con bits

```go
x := uint8(0xFF)  // 11111111
x &= 0x0F         // AND: 00001111 — mantener solo 4 bits bajos
x |= 0x30         // OR:  00111111 — activar bits 4 y 5
x ^= 0x0C         // XOR: 00110011 — toggle bits 2 y 3
x &^= 0x02        // AND NOT: 00110001 — limpiar bit 1
x <<= 1           // shift left: 01100010
x >>= 2           // shift right: 00011000
```

---

## 9. Overflow y wraparound

Go **no detecta overflow en runtime** para operaciones aritméticas.
Los valores simplemente "dan la vuelta" (wrap around).

```go
package main

import (
    "fmt"
    "math"
)

func main() {
    // Overflow en enteros con signo:
    var x int8 = math.MaxInt8  // 127
    x++
    fmt.Println(x)  // -128 (¡wrap around!)

    // 127 + 1 en binario:
    // 0111_1111 + 1 = 1000_0000 = -128 en complemento a dos

    // Overflow en enteros sin signo:
    var y uint8 = math.MaxUint8  // 255
    y++
    fmt.Println(y)  // 0 (wrap around)

    // 255 + 1 en binario:
    // 1111_1111 + 1 = 1_0000_0000 → truncado a 8 bits = 0000_0000

    // Underflow:
    var z uint8 = 0
    z--
    fmt.Println(z)  // 255 (wrap around)

    // ⚠️ Go NO genera panic ni error en overflow.
    // A diferencia de Rust (que hace panic en debug mode).
    // Es responsabilidad del programador prevenir overflow.
}
```

### 9.1 Detectar overflow manualmente

```go
package main

import (
    "errors"
    "math"
)

var ErrOverflow = errors.New("integer overflow")

// SafeAddInt64 suma dos int64 con detección de overflow.
func SafeAddInt64(a, b int64) (int64, error) {
    if b > 0 && a > math.MaxInt64-b {
        return 0, ErrOverflow
    }
    if b < 0 && a < math.MinInt64-b {
        return 0, ErrOverflow
    }
    return a + b, nil
}

// SafeMulUint64 multiplica dos uint64 con detección de overflow.
func SafeMulUint64(a, b uint64) (uint64, error) {
    if a == 0 || b == 0 {
        return 0, nil
    }
    result := a * b
    if result/a != b {
        return 0, ErrOverflow
    }
    return result, nil
}

// ¿Cuándo importa para SysAdmin?
// - Calcular tamaños de archivo totales (puede exceder int64 en teoría)
// - Multiplicar count * size para preallocación de memoria
// - Timestamps en nanosegundos (overflow de int64 en año 2262)
// - Contadores de métricas que acumulan por mucho tiempo
```

### 9.2 math/bits — operaciones de bits seguras

```go
package main

import (
    "fmt"
    "math/bits"
)

func main() {
    // Contar bits a 1:
    fmt.Println(bits.OnesCount8(0b1010_0011))   // 4
    fmt.Println(bits.OnesCount32(0xFFFFFF00))    // 24

    // Leading/Trailing zeros:
    fmt.Println(bits.LeadingZeros8(0b0000_1000)) // 4
    fmt.Println(bits.TrailingZeros8(0b0010_0000)) // 5

    // Bit length (posición del bit más significativo + 1):
    fmt.Println(bits.Len8(0b0010_0000))  // 6

    // Rotar bits:
    fmt.Printf("%08b\n", bits.RotateLeft8(0b1010_0001, 2))
    // 10000110

    // Reverse bits:
    fmt.Printf("%08b\n", bits.Reverse8(0b1010_0001))
    // 10000101

    // Suma con carry (para aritmética de precisión extendida):
    sum, carry := bits.Add64(math.MaxUint64, 1, 0)
    fmt.Printf("sum=%d, carry=%d\n", sum, carry) // sum=0, carry=1

    // Multiplicación con resultado doble:
    hi, lo := bits.Mul64(math.MaxUint64, 2)
    fmt.Printf("hi=%d, lo=%d\n", hi, lo) // hi=1, lo=...

    // Uso real: calcular máscara de subred desde prefix length
    prefixLen := 24
    mask := uint32(0xFFFFFFFF) << (32 - prefixLen)
    fmt.Printf("/%d = %d.%d.%d.%d\n", prefixLen,
        mask>>24, (mask>>16)&0xFF, (mask>>8)&0xFF, mask&0xFF)
    // /24 = 255.255.255.0
}
```

---

## 10. Conversiones entre tipos enteros

Go **no tiene conversiones implícitas**. Toda conversión debe ser
explícita, incluso entre int32 e int64.

```go
package main

import "fmt"

func main() {
    // Conversión explícita obligatoria:
    var a int32 = 42
    var b int64 = int64(a)  // ✓ conversión explícita
    // var b int64 = a      // ✗ no compila — tipos diferentes

    // Incluso entre int e int64:
    var c int = 42
    var d int64 = int64(c)  // ✓ necesario aunque int sea 64-bit

    // Conversión con posible pérdida de datos:
    var big int64 = 100000
    var small int8 = int8(big)  // ✓ compila, PERO trunca
    fmt.Println(small)           // -96 (¡overflow silencioso!)

    // 100000 en binario: ...0001_1000_0110_1010_0000
    // Truncado a 8 bits:                    1010_0000 = -96 (int8)

    // Conversión uint a int:
    var u uint64 = 300
    var s int64 = int64(u)     // ✓ funciona si u <= MaxInt64
    fmt.Println(s)              // 300

    // Conversión int a uint (cuidado con negativos):
    var neg int64 = -1
    var pos uint64 = uint64(neg)  // ✓ compila, pero...
    fmt.Println(pos)               // 18446744073709551615 (MaxUint64)

    _ = b; _ = d
}
```

### 10.1 Conversiones seguras

```go
// Verificar antes de convertir:

func Int64ToInt32(n int64) (int32, error) {
    if n < math.MinInt32 || n > math.MaxInt32 {
        return 0, fmt.Errorf("value %d overflows int32", n)
    }
    return int32(n), nil
}

func Uint64ToUint32(n uint64) (uint32, error) {
    if n > math.MaxUint32 {
        return 0, fmt.Errorf("value %d overflows uint32", n)
    }
    return uint32(n), nil
}

func IntToUint(n int) (uint, error) {
    if n < 0 {
        return 0, fmt.Errorf("cannot convert negative value %d to uint", n)
    }
    return uint(n), nil
}
```

### 10.2 Conversiones comunes en SysAdmin

```go
// String a entero:
import "strconv"

n, err := strconv.Atoi("42")          // string → int
n64, err := strconv.ParseInt("42", 10, 64)  // string → int64
u64, err := strconv.ParseUint("42", 10, 64) // string → uint64

// Con base (hex, octal, binario):
hex, _ := strconv.ParseInt("FF", 16, 64)    // 255
oct, _ := strconv.ParseInt("755", 8, 64)    // 493
bin, _ := strconv.ParseInt("1010", 2, 64)   // 10

// Entero a string:
s := strconv.Itoa(42)                        // int → string
s64 := strconv.FormatInt(int64(42), 10)      // int64 → string
shex := strconv.FormatInt(int64(255), 16)    // "ff"
soct := strconv.FormatInt(int64(493), 8)     // "755"
sbin := strconv.FormatInt(int64(10), 2)      // "1010"

// Con fmt (más lento pero más flexible):
s = fmt.Sprintf("%d", 42)     // decimal
s = fmt.Sprintf("%x", 255)    // hexadecimal: "ff"
s = fmt.Sprintf("%X", 255)    // hexadecimal: "FF"
s = fmt.Sprintf("%o", 493)    // octal: "755"
s = fmt.Sprintf("%b", 10)     // binario: "1010"
s = fmt.Sprintf("%04d", 42)   // con padding: "0042"
```

---

## 11. Comparaciones entre tipos

```go
package main

func main() {
    // Go NO permite comparar tipos diferentes directamente:
    var a int32 = 42
    var b int64 = 42

    // if a == b { }  // ✗ no compila: mismatched types int32 and int64

    // Debes convertir explícitamente:
    if int64(a) == b { }  // ✓

    // Esto previene bugs sutiles que existen en C:
    // En C: if (unsigned_int == signed_int) puede dar sorpresas
    // En Go: el compilador te obliga a pensar en la conversión

    // int e int32 son tipos DIFERENTES aunque int sea 32 bits:
    var x int = 42
    var y int32 = 42
    // if x == y { }  // ✗ no compila, incluso si int == int32 en tu plataforma

    // Constantes sin tipo (untyped) SÍ se comparan con cualquier tipo:
    if a == 42 { }   // ✓ 42 es constante sin tipo
    if b == 42 { }   // ✓ funciona con cualquier tipo numérico

    _ = x; _ = y
}
```

---

## 12. Enteros en la stdlib — paquetes importantes

### 12.1 math — constantes y funciones

```go
import "math"

// Constantes de rango:
math.MaxInt8     // 127
math.MinInt8     // -128
math.MaxInt16    // 32767
math.MinInt16    // -32768
math.MaxInt32    // 2147483647
math.MinInt32    // -2147483648
math.MaxInt64    // 9223372036854775807
math.MinInt64    // -9223372036854775808
math.MaxUint8    // 255
math.MaxUint16   // 65535
math.MaxUint32   // 4294967295
math.MaxUint64   // 18446744073709551615

// Desde Go 1.17:
math.MaxInt      // máximo de int (depende de plataforma)
math.MinInt      // mínimo de int

// Funciones para enteros:
math.Abs(-42)    // ⚠️ devuelve float64, no int
                 // Para abs de int: if x < 0 { x = -x }

// Desde Go 1.21 — max y min built-in:
x := max(a, b)     // mayor de los dos
y := min(a, b, c)  // menor de los tres
```

### 12.2 math/big — enteros de precisión arbitraria

```go
import "math/big"

// Para números que no caben en int64:
a := new(big.Int)
a.SetString("123456789012345678901234567890", 10)

b := new(big.Int)
b.SetString("987654321098765432109876543210", 10)

result := new(big.Int)
result.Add(a, b)
fmt.Println(result.String())

// Útil para:
// - Criptografía (RSA, DH)
// - Tokens de blockchain
// - Cálculos financieros de alta precisión
```

### 12.3 encoding/binary — serialización de enteros

```go
import (
    "bytes"
    "encoding/binary"
)

// Escribir enteros en formato binario:
buf := new(bytes.Buffer)
binary.Write(buf, binary.BigEndian, uint16(8080))
binary.Write(buf, binary.BigEndian, uint32(0xC0A80001))
fmt.Printf("% X\n", buf.Bytes())
// 1F 90 C0 A8 00 01

// Leer enteros de datos binarios:
data := []byte{0x1F, 0x90}
port := binary.BigEndian.Uint16(data)
fmt.Println(port)  // 8080

// Big-endian (network byte order) vs Little-endian:
// Big-endian: byte más significativo primero (protocolos de red)
// Little-endian: byte menos significativo primero (x86, ARM)

// Regla: protocolos de red usan Big-endian.
// Datos locales usan Little-endian en la mayoría de hardware.

data32 := make([]byte, 4)
binary.BigEndian.PutUint32(data32, 0xC0A80001)
fmt.Printf("Big-endian:    % X\n", data32)    // C0 A8 00 01
binary.LittleEndian.PutUint32(data32, 0xC0A80001)
fmt.Printf("Little-endian: % X\n", data32)    // 01 00 A8 C0
```

### 12.4 strconv — conversión string ↔ entero

```go
import "strconv"

// String a int:
n, err := strconv.Atoi("42")
if err != nil {
    fmt.Println("not a number:", err)
}

// int a string:
s := strconv.Itoa(42)  // "42"

// ParseInt — más control:
// ParseInt(s string, base int, bitSize int) (int64, error)
n64, err := strconv.ParseInt("FF", 16, 64)  // hex → 255
n64, err = strconv.ParseInt("-42", 10, 32)   // decimal, cabe en int32

// FormatInt — entero a string con base:
s = strconv.FormatInt(255, 16)   // "ff"
s = strconv.FormatInt(255, 2)    // "11111111"
s = strconv.FormatInt(255, 8)    // "377"

// ParseUint — para sin signo:
u64, err := strconv.ParseUint("42", 10, 64)

// ⚠️ bitSize no limita el tipo de retorno (siempre int64/uint64),
// pero SÍ valida que el valor quepa en ese tamaño:
_, err = strconv.ParseInt("200", 10, 8)  // error: 200 overflows int8
```

---

## 13. Enteros en structs para SysAdmin

```go
// Ejemplo real: struct para información de disco

type DiskInfo struct {
    Device     string
    MountPoint string
    FSType     string
    Total      uint64  // bytes totales (uint64 para >4TB)
    Used       uint64  // bytes usados
    Available  uint64  // bytes disponibles
    Inodes     uint64  // inodes totales
    InodesUsed uint64  // inodes usados
}

func (d DiskInfo) UsagePercent() float64 {
    if d.Total == 0 {
        return 0
    }
    return float64(d.Used) / float64(d.Total) * 100
}

// Ejemplo: struct para información de proceso

type ProcessInfo struct {
    PID     int32    // PIDs en Linux son int32 (max ~4 million)
    PPID    int32    // parent PID
    UID     uint32   // user ID
    GID     uint32   // group ID
    Threads int32    // número de threads
    RSS     int64    // Resident Set Size en bytes (puede ser >2GB)
    VSize   uint64   // Virtual Size en bytes (puede ser enorme)
    Uptime  int64    // segundos desde que arrancó
}

// Ejemplo: struct para métricas de red

type NetStats struct {
    BytesSent     uint64  // contadores acumulativos, nunca negativos
    BytesRecv     uint64
    PacketsSent   uint64
    PacketsRecv   uint64
    ErrorsIn      uint64
    ErrorsOut     uint64
    DropsIn       uint64
    DropsOut      uint64
}

// ¿Por qué uint64 para contadores?
// 1. Nunca son negativos
// 2. Acumulan sin resetear → pueden ser MUY grandes
// 3. /proc/net/dev usa unsigned long (uint64 en 64-bit)
// 4. Un servidor saturado a 10Gbps llena un uint64 en ~58 años
```

---

## 14. unsafe.Sizeof y alineamiento de memoria

```go
package main

import (
    "fmt"
    "unsafe"
)

func main() {
    // Tamaño de cada tipo:
    fmt.Println("int8:   ", unsafe.Sizeof(int8(0)))    // 1
    fmt.Println("int16:  ", unsafe.Sizeof(int16(0)))   // 2
    fmt.Println("int32:  ", unsafe.Sizeof(int32(0)))   // 4
    fmt.Println("int64:  ", unsafe.Sizeof(int64(0)))   // 8
    fmt.Println("int:    ", unsafe.Sizeof(int(0)))     // 4 o 8
    fmt.Println("uint:   ", unsafe.Sizeof(uint(0)))    // 4 o 8
    fmt.Println("uintptr:", unsafe.Sizeof(uintptr(0))) // 4 o 8
}
```

### 14.1 Padding en structs — optimizar memoria

```go
// El compilador añade padding para alinear campos en memoria.

// MAL ORDENADO — 24 bytes con padding:
type BadOrder struct {
    A bool    // 1 byte + 7 bytes padding
    B int64   // 8 bytes
    C bool    // 1 byte + 7 bytes padding
}
// Total: 24 bytes (debería ser 10)

// BIEN ORDENADO — 16 bytes:
type GoodOrder struct {
    B int64   // 8 bytes
    A bool    // 1 byte
    C bool    // 1 byte + 6 bytes padding
}
// Total: 16 bytes

// Verificar:
fmt.Println(unsafe.Sizeof(BadOrder{}))   // 24
fmt.Println(unsafe.Sizeof(GoodOrder{}))  // 16

// Regla: ordenar campos de mayor a menor tamaño.

// ¿Cuándo importa?
// Cuando tienes slices de millones de structs (métricas, logs).
// 1 millón de BadOrder:  24 MB
// 1 millón de GoodOrder: 16 MB → 33% menos memoria

// El linter "fieldalignment" de golangci-lint detecta esto:
// golangci-lint run --enable=fieldalignment
```

---

## 15. Patrones comunes con enteros para SysAdmin

### 15.1 Parsear /proc para obtener valores numéricos

```go
package main

import (
    "bufio"
    "fmt"
    "os"
    "strconv"
    "strings"
)

// MemInfo contiene información de /proc/meminfo.
type MemInfo struct {
    MemTotal     uint64
    MemFree      uint64
    MemAvailable uint64
    Buffers      uint64
    Cached       uint64
    SwapTotal    uint64
    SwapFree     uint64
}

func ReadMemInfo() (*MemInfo, error) {
    f, err := os.Open("/proc/meminfo")
    if err != nil {
        return nil, err
    }
    defer f.Close()

    info := &MemInfo{}
    scanner := bufio.NewScanner(f)

    for scanner.Scan() {
        line := scanner.Text()
        parts := strings.Fields(line)
        if len(parts) < 2 {
            continue
        }

        // Quitar el ":" del nombre:
        name := strings.TrimSuffix(parts[0], ":")

        // Parsear el valor (siempre en kB):
        val, err := strconv.ParseUint(parts[1], 10, 64)
        if err != nil {
            continue
        }
        val *= 1024  // convertir kB a bytes

        switch name {
        case "MemTotal":
            info.MemTotal = val
        case "MemFree":
            info.MemFree = val
        case "MemAvailable":
            info.MemAvailable = val
        case "Buffers":
            info.Buffers = val
        case "Cached":
            info.Cached = val
        case "SwapTotal":
            info.SwapTotal = val
        case "SwapFree":
            info.SwapFree = val
        }
    }

    return info, scanner.Err()
}

func formatBytes(b uint64) string {
    const (
        KB = 1024
        MB = KB * 1024
        GB = MB * 1024
        TB = GB * 1024
    )
    switch {
    case b >= TB:
        return fmt.Sprintf("%.1f TB", float64(b)/float64(TB))
    case b >= GB:
        return fmt.Sprintf("%.1f GB", float64(b)/float64(GB))
    case b >= MB:
        return fmt.Sprintf("%.1f MB", float64(b)/float64(MB))
    case b >= KB:
        return fmt.Sprintf("%.1f KB", float64(b)/float64(KB))
    default:
        return fmt.Sprintf("%d B", b)
    }
}

func main() {
    info, err := ReadMemInfo()
    if err != nil {
        fmt.Fprintf(os.Stderr, "error: %v\n", err)
        os.Exit(1)
    }

    fmt.Printf("Total:     %s\n", formatBytes(info.MemTotal))
    fmt.Printf("Available: %s\n", formatBytes(info.MemAvailable))
    fmt.Printf("Swap:      %s / %s\n",
        formatBytes(info.SwapTotal-info.SwapFree),
        formatBytes(info.SwapTotal))
}
```

### 15.2 Calcular tamaños con constantes

```go
// Constantes de tamaño usando shift left:
const (
    _  = iota           // 0 — ignorar
    KB = 1 << (10 * iota) // 1 << 10 = 1024
    MB                     // 1 << 20 = 1,048,576
    GB                     // 1 << 30 = 1,073,741,824
    TB                     // 1 << 40 = 1,099,511,627,776
    PB                     // 1 << 50 = 1,125,899,906,842,624
)

func main() {
    fmt.Println("1 KB =", KB)           // 1024
    fmt.Println("1 MB =", MB)           // 1048576
    fmt.Println("1 GB =", GB)           // 1073741824
    fmt.Println("1 TB =", TB)           // 1099511627776

    // Uso práctico:
    bufferSize := 64 * KB               // 65536 bytes
    maxFileSize := int64(2 * GB)        // 2147483648 bytes
    cacheSize := 256 * MB               // 268435456 bytes

    // Verificar si un archivo es demasiado grande:
    fileSize := int64(1_500_000_000)
    if fileSize > int64(GB) {
        fmt.Println("File is larger than 1 GB!")
    }

    _ = bufferSize; _ = maxFileSize; _ = cacheSize
}
```

### 15.3 Timestamps y durations

```go
import "time"

func main() {
    // time.Duration es int64 (nanosegundos):
    var d time.Duration = 5 * time.Second
    fmt.Println(d)                    // 5s
    fmt.Println(d.Milliseconds())     // 5000
    fmt.Println(d.Nanoseconds())      // 5000000000

    // Unix timestamps:
    now := time.Now()
    epoch := now.Unix()               // int64: seconds since epoch
    epochMilli := now.UnixMilli()     // int64: milliseconds
    epochNano := now.UnixNano()       // int64: nanoseconds

    fmt.Printf("Seconds:      %d\n", epoch)
    fmt.Printf("Milliseconds: %d\n", epochMilli)
    fmt.Printf("Nanoseconds:  %d\n", epochNano)

    // ⚠️ int64 nanoseconds overflow:
    // MaxInt64 nanoseconds ≈ 292 years desde epoch → año ~2262
    // Para la mayoría de aplicaciones esto es suficiente.

    // Crear time.Time desde Unix timestamp:
    t := time.Unix(1702656000, 0)  // 2023-12-15 16:00:00 UTC
    fmt.Println(t)

    // Calcular duración:
    elapsed := time.Since(t)
    fmt.Printf("Elapsed: %v\n", elapsed)
}
```

---

## 16. Comparación con C y Rust

```
    ┌──────────────────┬──────────────┬──────────────┬──────────────┐
    │ Concepto         │ Go           │ C            │ Rust         │
    ├──────────────────┼──────────────┼──────────────┼──────────────┤
    │ Tamaño fijo 8b   │ int8/uint8   │ int8_t       │ i8/u8        │
    │ Tamaño fijo 16b  │ int16/uint16 │ int16_t      │ i16/u16      │
    │ Tamaño fijo 32b  │ int32/uint32 │ int32_t      │ i32/u32      │
    │ Tamaño fijo 64b  │ int64/uint64 │ int64_t      │ i64/u64      │
    │ Plataforma       │ int/uint     │ int (varía)  │ isize/usize  │
    │ Puntero como int │ uintptr      │ uintptr_t    │ usize        │
    │ Byte             │ byte (uint8) │ unsigned char│ u8           │
    │ Char Unicode     │ rune (int32) │ —            │ char (u32)   │
    ├──────────────────┼──────────────┼──────────────┼──────────────┤
    │ Overflow runtime │ silencioso   │ UB (signed)  │ panic (debug)│
    │                  │ (wrap)       │ wrap (unsig.)│ wrap (release│
    │ Conv. implícita  │ NO           │ SÍ           │ NO           │
    │ Literales hex    │ 0xFF         │ 0xFF         │ 0xFF         │
    │ Literales oct    │ 0o77         │ 077          │ 0o77         │
    │ Literales bin    │ 0b1010       │ — (C23: sí)  │ 0b1010       │
    │ Separadores      │ 1_000_000    │ — (C23: sí)  │ 1_000_000    │
    └──────────────────┴──────────────┴──────────────┴──────────────┘
```

---

## 17. Formato de impresión de enteros

```go
package main

import "fmt"

func main() {
    n := 255
    u := uint32(0xDEADBEEF)

    // Verbs de fmt para enteros:
    fmt.Printf("%d\n", n)    // 255         decimal
    fmt.Printf("%+d\n", n)   // +255        decimal con signo
    fmt.Printf("%b\n", n)    // 11111111    binario
    fmt.Printf("%o\n", n)    // 377         octal
    fmt.Printf("%O\n", n)    // 0o377       octal con prefijo 0o
    fmt.Printf("%x\n", n)    // ff          hexadecimal lowercase
    fmt.Printf("%X\n", n)    // FF          hexadecimal uppercase
    fmt.Printf("%#x\n", n)   // 0xff        hex con prefijo 0x
    fmt.Printf("%#X\n", n)   // 0XFF        hex con prefijo 0X
    fmt.Printf("%c\n", 65)   // A           carácter Unicode
    fmt.Printf("%U\n", 65)   // U+0041      Unicode code point

    // Con padding:
    fmt.Printf("%10d\n", n)  //        255  alineado a derecha, 10 chars
    fmt.Printf("%-10d\n", n) // 255         alineado a izquierda
    fmt.Printf("%010d\n", n) // 0000000255  con ceros a la izquierda
    fmt.Printf("%08X\n", u)  // DEADBEEF    hex con padding de ceros

    // Tipo del valor:
    fmt.Printf("%T\n", n)    // int
    fmt.Printf("%T\n", u)    // uint32

    // Representación Go (útil para debugging):
    fmt.Printf("%v\n", n)    // 255
    fmt.Printf("%#v\n", n)   // 255
}
```

---

## 18. Tabla de errores comunes

| Error / Síntoma | Causa | Solución |
|---|---|---|
| `cannot use x (int32) as int64` | Go no convierte tipos implícitamente | Conversión explícita: `int64(x)` |
| `constant 256 overflows uint8` | Literal excede el rango del tipo | Usar tipo más grande o verificar el valor |
| Resultado negativo inesperado | Overflow silencioso (wrap around) | Verificar rango antes de operar, o usar tipo más grande |
| `cannot convert negative constant to uint` | Intentar asignar valor negativo a unsigned | Usar tipo con signo o verificar que el valor es >= 0 |
| `invalid operation: a == b (mismatched types)` | Comparar int32 con int64 | Convertir explícitamente un operando |
| `0644` no es octal correcto | Confusión entre `0644` (octal legacy) y `644` (decimal) | Usar `0o644` (prefijo explícito) |
| Struct ocupa más memoria de lo esperado | Padding por alineamiento | Reordenar campos de mayor a menor, usar linter `fieldalignment` |
| `strconv.ParseInt: parsing "abc": invalid syntax` | String no es un número válido | Validar input antes de parsear |
| `strconv.ParseInt: value out of range` | Número excede el rango del bitSize | Usar bitSize mayor o validar el rango |
| Division by zero panic | `a / b` cuando `b == 0` | Verificar divisor antes: `if b != 0 { result = a / b }` |
| Resultado inesperado en `/` con negativos | División entera trunca hacia cero | Documentar comportamiento: `-17/5 = -3`, no `-4` |
| `unsafe.Sizeof` devuelve tamaño incorrecto | Confusión entre tamaño del tipo y del valor | `Sizeof` mide el tipo, no el contenido (ej: slice header = 24B) |

---

## 19. Ejercicios

### Ejercicio 1 — Tamaños y rangos

```
Crea un programa que imprima para cada tipo entero:
- Nombre del tipo
- Tamaño en bytes (unsafe.Sizeof)
- Valor mínimo
- Valor máximo

Usa math.MinInt8, math.MaxInt8, etc.
Incluye: int8, int16, int32, int64, int, uint8, uint16, uint32, uint64, uint

Predicción: ¿cuántos bytes tiene int en tu plataforma?
¿Y uintptr?
```

### Ejercicio 2 — Permisos de archivo

```
Crea funciones:
- ParsePermissions(perm uint32) string → "rwxr-xr-x"
- FormatOctal(perm uint32) string → "0o755"

Usa operaciones de bits para extraer cada grupo (owner/group/other).
Prueba con: 0o755, 0o644, 0o600, 0o777, 0o000

Predicción: ¿qué operaciones de bits necesitas para extraer
los 3 bits del grupo (bits 3-5)?
```

### Ejercicio 3 — Parsear /proc/meminfo

```
Escribe un programa que:
1. Lea /proc/meminfo
2. Extraiga MemTotal, MemAvailable, SwapTotal, SwapFree
3. Calcule porcentaje de uso de memoria y swap
4. Muestre los valores formateados (GB, MB)

Predicción: ¿los valores en /proc/meminfo están en qué unidad?
¿Qué tipo de entero necesitas para almacenar MemTotal en bytes?
```

### Ejercicio 4 — Dirección IP como uint32

```
Crea funciones:
- IPtoUint32(ip string) (uint32, error) → parsea "192.168.0.1"
- Uint32ToIP(n uint32) string → formatea a "192.168.0.1"
- SubnetMask(prefix int) uint32 → máscara desde /24
- NetworkAddress(ip, mask uint32) uint32
- BroadcastAddress(ip, mask uint32) uint32

Prueba con: 192.168.1.100/24, 10.0.0.1/8, 172.16.0.0/12

Predicción: ¿qué operación de bits calcula la dirección de red?
¿Y la de broadcast?
```

### Ejercicio 5 — Overflow detection

```
Crea funciones SafeAdd, SafeMul, SafeSub para int64:
- Devuelven (int64, error)
- Devuelven error si hay overflow

Prueba con:
- SafeAdd(math.MaxInt64, 1)
- SafeMul(math.MaxInt64, 2)
- SafeSub(math.MinInt64, 1)
- SafeAdd(100, 200) — caso normal

Predicción: ¿cómo detectas overflow ANTES de que ocurra
en la suma de dos int64 positivos?
```

### Ejercicio 6 — Formateo de bytes humano

```
Crea una función FormatBytes(b uint64) string que:
- < 1 KB → "512 B"
- < 1 MB → "42.5 KB"
- < 1 GB → "128.3 MB"
- < 1 TB → "4.2 GB"
- >= 1 TB → "1.5 TB"

Usa constantes con iota y bit shift.
Prueba con: 0, 512, 1024, 1536, 1048576, 1073741824, 5497558138880

Predicción: ¿1 KB es 1000 o 1024 bytes en tu implementación?
¿Cuál es la convención en el mundo de SysAdmin?
```

### Ejercicio 7 — Binary encoding de protocolo

```
Simula el header de un paquete de red simplificado:
- Version: uint8 (4 bits altos del primer byte)
- HeaderLen: uint8 (4 bits bajos del primer byte)
- TotalLength: uint16 (big-endian)
- TTL: uint8
- Protocol: uint8

1. Crea una función Encode que serialice a []byte
2. Crea una función Decode que deserialice de []byte
3. Usa encoding/binary con BigEndian
4. Verifica con round-trip: Encode → Decode → comparar

Predicción: ¿cuántos bytes ocupa el header completo?
¿Cómo empaquetas Version y HeaderLen en un solo byte?
```

### Ejercicio 8 — Comparación de tipos

```
Crea un programa que demuestre:
1. int e int64 son tipos DIFERENTES (no compilan con ==)
2. byte y uint8 son el MISMO tipo (sí compilan con ==)
3. Constante sin tipo "42" se puede comparar con cualquier entero
4. Conversión trunca silenciosamente: int64(300) → int8

Para cada caso, predice si compila y qué resultado da.

Predicción: ¿int(42) == int32(42) compila en una plataforma de 32 bits?
```

### Ejercicio 9 — Struct padding

```
Crea dos versiones de un struct con estos campos:
  active bool, count int64, flags uint8, size int32, id uint16

1. Versión A: ordenar campos como aparecen arriba
2. Versión B: reordenar para minimizar padding
3. Imprimir unsafe.Sizeof de ambas versiones
4. Imprimir unsafe.Offsetof de cada campo

Predicción: ¿cuántos bytes tiene la versión A? ¿Y la B?
¿Cuántos bytes se desperdician en padding en la versión A?
```

### Ejercicio 10 — Monitor de sistema con enteros

```
Crea un programa que cada 2 segundos imprima:
- Uso de CPU (parseando /proc/stat)
- Uso de memoria (parseando /proc/meminfo)
- Load average (parseando /proc/loadavg)
- Uptime (parseando /proc/uptime)

Usa los tipos correctos:
- uint64 para contadores de CPU
- uint64 para memoria en bytes
- int64 para uptime en segundos
- Los load averages son floats (veremos en T02)

Predicción: ¿los contadores de CPU en /proc/stat son
acumulativos o instantáneos? ¿Cómo calculas el porcentaje
de CPU a partir de dos lecturas?
```

---

## 20. Resumen

```
    ┌───────────────────────────────────────────────────────────┐
    │                Enteros en Go — Resumen                    │
    │                                                           │
    │  TIPOS DE TAMAÑO FIJO:                                    │
    │  ┌─ int8/uint8 (1B), int16/uint16 (2B)                   │
    │  ├─ int32/uint32 (4B), int64/uint64 (8B)                 │
    │  └─ byte = uint8, rune = int32                           │
    │                                                           │
    │  TIPOS DEPENDIENTES DE PLATAFORMA:                        │
    │  ┌─ int/uint → 4B en 32-bit, 8B en 64-bit               │
    │  └─ uintptr → tamaño de un puntero                       │
    │                                                           │
    │  REGLA DE ORO:                                            │
    │  ┌─ int para la mayoría de cosas                         │
    │  ├─ int64 para file sizes, timestamps, contadores        │
    │  ├─ uint64 para contadores de métricas, /proc values     │
    │  ├─ uint16 para puertos de red                           │
    │  ├─ uint32 para permisos, UIDs, IPs                      │
    │  └─ byte para datos binarios                             │
    │                                                           │
    │  LITERALES:                                               │
    │  ┌─ 42         decimal                                   │
    │  ├─ 0xFF       hexadecimal                               │
    │  ├─ 0o755      octal (permisos)                          │
    │  ├─ 0b1010     binario (flags)                           │
    │  └─ 1_000_000  separadores                               │
    │                                                           │
    │  CUIDADO CON:                                             │
    │  ┌─ Overflow silencioso (Go no detecta en runtime)       │
    │  ├─ Conversiones que truncan (int64 → int8)              │
    │  ├─ int ≠ int64 (tipos diferentes, no se comparan)       │
    │  └─ uint y valores negativos (underflow → wrap)          │
    └───────────────────────────────────────────────────────────┘
```
