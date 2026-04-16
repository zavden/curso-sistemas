# Arrays — Tamano Fijo, Parte del Tipo

## 1. Introduccion

Los arrays en Go son **colecciones de tamano fijo** donde el tamano es **parte del tipo**. Esto significa que `[3]int` y `[4]int` son tipos completamente diferentes — no puedes asignar uno al otro ni pasarlos a la misma funcion. Esta rigidez es intencional: Go utiliza arrays como la **base sobre la que se construyen los slices**, que son la estructura que realmente usaras en el 99% de los casos.

¿Entonces por que dedicar un topico entero a arrays? Porque entender arrays es prerequisito para entender slices (T02), y porque hay casos especificos donde los arrays son la herramienta correcta: buffers de tamano conocido, claves de hash, representacion de direcciones IP/MAC, y optimizaciones de rendimiento donde el tamano fijo permite stack allocation.

En SysAdmin/DevOps, encontraras arrays en: direcciones IPv4 (`[4]byte`), IPv6 (`[16]byte`), SHA-256 hashes (`[32]byte`), MAC addresses (`[6]byte`), y buffers fijos para I/O.

```
┌──────────────────────────────────────────────────────────────┐
│              ARRAYS EN GO                                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Declaracion:                                                │
│  var a [5]int          → [0, 0, 0, 0, 0] (zero values)      │
│  b := [3]string{"a","b","c"}                                 │
│  c := [...]int{1,2,3}  → compilador cuenta: [3]int          │
│                                                              │
│  Propiedades:                                                │
│  ├─ Tamano FIJO (no puede crecer ni encoger)                 │
│  ├─ Tamano es PARTE DEL TIPO ([3]int ≠ [4]int)              │
│  ├─ Se pasan por VALOR (copia completa)                      │
│  ├─ Son COMPARABLES con == (si el tipo elemento lo es)       │
│  ├─ Zero value: todos los elementos en zero value del tipo   │
│  ├─ Longitud conocida en compile time: len() es constante    │
│  └─ Pueden vivir en el STACK (no siempre, depende de escape) │
│                                                              │
│  Arrays vs Slices:                                           │
│  ├─ Array: [N]T — tamano fijo, valor, comparable             │
│  └─ Slice: []T  — tamano variable, referencia, no comparable │
│                                                              │
│  En la practica:                                             │
│  ├─ Slices: 99% del codigo                                   │
│  ├─ Arrays: buffers fijos, crypto hashes, IPs, interop C     │
│  └─ Los arrays son el backing store de los slices            │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Declaracion e inicializacion

### Zero value

```go
// Todos los elementos se inicializan al zero value del tipo
var nums [5]int
fmt.Println(nums) // [0 0 0 0 0]

var names [3]string
fmt.Println(names) // [  ] (tres strings vacios)

var flags [4]bool
fmt.Println(flags) // [false false false false]

var ptrs [2]*int
fmt.Println(ptrs) // [<nil> <nil>]
```

### Literal con valores

```go
// Especificar todos los elementos
a := [5]int{10, 20, 30, 40, 50}

// El compilador cuenta con ...
b := [...]int{10, 20, 30, 40, 50}
// b es de tipo [5]int — el compilador infiere el tamano

// Parcial: los no especificados son zero value
c := [5]int{10, 20} // [10, 20, 0, 0, 0]

// Con indices especificos
d := [5]int{0: 100, 4: 500} // [100, 0, 0, 0, 500]

// El indice mas alto determina el tamano con ...
e := [...]int{9: 1} // [0,0,0,0,0,0,0,0,0,1] — tamano 10
```

### Arrays de structs

```go
type Server struct {
    Name string
    Port int
}

servers := [3]Server{
    {"web01", 80},
    {"web02", 80},
    {"db01", 5432},
}

// Con nombres de campo
servers2 := [...]Server{
    {Name: "cache01", Port: 6379},
    {Name: "cache02", Port: 6379},
}
// tipo: [2]Server
```

### Arrays multidimensionales

```go
// Matriz 3x3
var matrix [3][3]int
matrix[0][0] = 1
matrix[1][1] = 1
matrix[2][2] = 1
// Matriz identidad

// Inicializar inline
grid := [2][3]int{
    {1, 2, 3},
    {4, 5, 6},
}

fmt.Println(grid[0][1]) // 2
fmt.Println(grid[1][2]) // 6
```

---

## 3. El tamano es parte del tipo

### Tipos diferentes

```go
var a [3]int
var b [4]int
var c [3]int

// a y c son del mismo tipo: [3]int
// b es de un tipo diferente: [4]int

// ✓ Mismo tipo — asignacion valida
c = a

// ✗ ERROR de compilacion:
// a = b // cannot use b (variable of type [4]int) as [3]int in assignment

// Esto implica que no puedes escribir una funcion que acepte
// "cualquier array de ints":
func sum3(arr [3]int) int { ... } // Solo acepta [3]int
func sum4(arr [4]int) int { ... } // Solo acepta [4]int
// No hay forma de escribir func sum(arr [N]int) sin generics

// Con generics (Go 1.18+) tampoco puedes parametrizar el tamano:
// func sum[N int](arr [N]int) int { ... } // No soportado
// Go no tiene const generics (Rust si: [T; N])
```

### Implicacion: usar slices para flexibilidad

```go
// ✗ Poco util: atado a un tamano
func sumArray(arr [5]int) int {
    total := 0
    for _, v := range arr {
        total += v
    }
    return total
}

// ✓ Flexible: acepta cualquier cantidad
func sumSlice(nums []int) int {
    total := 0
    for _, v := range nums {
        total += v
    }
    return total
}

// Puedes pasar un array a una funcion de slice:
arr := [5]int{1, 2, 3, 4, 5}
result := sumSlice(arr[:]) // arr[:] convierte a slice
```

### sizeof y alignment

```go
// El tamano en memoria es exactamente N * sizeof(element)
fmt.Println(unsafe.Sizeof([3]int{}))    // 24 (3 * 8 bytes en 64-bit)
fmt.Println(unsafe.Sizeof([100]int{}))  // 800
fmt.Println(unsafe.Sizeof([4]byte{}))   // 4
fmt.Println(unsafe.Sizeof([32]byte{}))  // 32

// No hay overhead de header como en slices
// Un slice tiene 24 bytes de header (pointer + len + cap) en 64-bit
// Un array [3]int tiene exactamente 24 bytes (solo los datos)
```

---

## 4. Paso por valor — copia completa

### Arrays se copian al asignar

```go
a := [3]int{1, 2, 3}
b := a        // COPIA completa — b es independiente de a
b[0] = 999
fmt.Println(a) // [1 2 3]   — no cambio
fmt.Println(b) // [999 2 3] — solo b cambio
```

### Arrays se copian al pasar a funciones

```go
func modify(arr [3]int) {
    arr[0] = 999 // Modifica la COPIA
}

original := [3]int{1, 2, 3}
modify(original)
fmt.Println(original) // [1 2 3] — no cambio

// Para modificar el original: pasar puntero
func modifyPtr(arr *[3]int) {
    arr[0] = 999 // Modifica el original via puntero
}

modifyPtr(&original)
fmt.Println(original) // [999 2 3] — cambio
```

### Implicacion de rendimiento

```go
// Pasar arrays grandes por valor es costoso:
func process(data [1000000]byte) { // ¡Copia 1MB en cada llamada!
    // ...
}

// ✓ Pasar puntero:
func processPtr(data *[1000000]byte) { // Copia 8 bytes (puntero)
    // ...
}

// ✓ Mejor aun: usar slice
func processSlice(data []byte) { // Copia 24 bytes (header del slice)
    // ...
}
```

### Comparacion con C y Rust

```go
// Go:   arrays se pasan por valor (copia)
// C:    arrays decaen a puntero al pasarlos (por referencia implicita)
// Rust: arrays se pasan por valor por defecto (como Go),
//       pero con borrowing (&[T; N]) se pasan por referencia

// El comportamiento de Go es mas seguro que C:
// No hay decaimiento implicito a puntero
// Pero puede ser sorprendente si vienes de C
```

---

## 5. Iteracion

### for con indice

```go
servers := [4]string{"web01", "web02", "db01", "cache01"}

for i := 0; i < len(servers); i++ {
    fmt.Printf("[%d] %s\n", i, servers[i])
}
```

### for-range (idiomatico)

```go
// Indice y valor
for i, server := range servers {
    fmt.Printf("[%d] %s\n", i, server)
}

// Solo valor
for _, server := range servers {
    fmt.Println(server)
}

// Solo indice
for i := range servers {
    fmt.Printf("Index: %d\n", i)
}

// Go 1.22+: range sobre entero
for i := range len(servers) {
    fmt.Printf("[%d] %s\n", i, servers[i])
}
```

### range copia el valor (no referencia)

```go
arr := [3]int{10, 20, 30}

// ✗ Esto NO modifica el array:
for _, v := range arr {
    v *= 2 // Modifica la copia local, no arr[i]
}
fmt.Println(arr) // [10 20 30] — sin cambios

// ✓ Modificar via indice:
for i := range arr {
    arr[i] *= 2
}
fmt.Println(arr) // [20 40 60]

// ✓ O con puntero al array:
for i := range &arr { // range sobre *[3]int
    arr[i] *= 2
}
```

---

## 6. Comparacion de arrays

### Operador == y !=

```go
// Los arrays son comparables si su tipo elemento es comparable

a := [3]int{1, 2, 3}
b := [3]int{1, 2, 3}
c := [3]int{1, 2, 4}

fmt.Println(a == b) // true — todos los elementos iguales
fmt.Println(a == c) // false
fmt.Println(a != c) // true

// Comparacion elemento por elemento, en orden
```

### Arrays como keys de map

```go
// Como los arrays son comparables, pueden ser keys de map
// (los slices NO pueden)

type IPAddr [4]byte

connections := make(map[IPAddr]int)
connections[IPAddr{192, 168, 1, 1}] = 5
connections[IPAddr{10, 0, 0, 1}] = 12

for ip, count := range connections {
    fmt.Printf("%d.%d.%d.%d: %d connections\n",
        ip[0], ip[1], ip[2], ip[3], count)
}

// Otros usos como key:
type Coordinate [2]float64
visited := make(map[Coordinate]bool)
visited[Coordinate{40.7128, -74.0060}] = true // NYC

// Hash como key:
type SHA256Hash [32]byte
fileCache := make(map[SHA256Hash][]byte)
```

### Tipos no comparables

```go
// ✗ ERROR: arrays de slices, maps, o funciones no son comparables
// var a, b [3][]int
// fmt.Println(a == b) // invalid operation: cannot compare a == b

// ✗ Arrays de structs con campos no comparables
type S struct {
    Data []int // slice no es comparable
}
// var x, y [2]S
// x == y // ERROR

// ✓ Arrays de structs con todos los campos comparables
type Point struct {
    X, Y int
}
a := [2]Point{{1, 2}, {3, 4}}
b := [2]Point{{1, 2}, {3, 4}}
fmt.Println(a == b) // true
```

---

## 7. Arrays en la stdlib y el ecosistema

### net.IP y direcciones

```go
// La stdlib usa [4]byte y [16]byte para IPs

// net.IP es un slice, pero internamente:
// IPv4: 4 bytes
// IPv6: 16 bytes

// Para representacion fija:
type IPv4 [4]byte
type IPv6 [16]byte

func (ip IPv4) String() string {
    return fmt.Sprintf("%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3])
}

func ParseIPv4(s string) (IPv4, error) {
    var ip IPv4
    parts := strings.Split(s, ".")
    if len(parts) != 4 {
        return ip, fmt.Errorf("invalid IPv4: %s", s)
    }
    for i, part := range parts {
        n, err := strconv.Atoi(part)
        if err != nil || n < 0 || n > 255 {
            return ip, fmt.Errorf("invalid octet: %s", part)
        }
        ip[i] = byte(n)
    }
    return ip, nil
}

func (ip IPv4) IsPrivate() bool {
    return ip[0] == 10 ||
        (ip[0] == 172 && ip[1] >= 16 && ip[1] <= 31) ||
        (ip[0] == 192 && ip[1] == 168)
}

func (ip IPv4) Network(mask IPv4) IPv4 {
    return IPv4{
        ip[0] & mask[0],
        ip[1] & mask[1],
        ip[2] & mask[2],
        ip[3] & mask[3],
    }
}
```

### crypto/sha256

```go
// sha256.Sum256 retorna [32]byte, no un slice
import "crypto/sha256"

data := []byte("hello world")
hash := sha256.Sum256(data) // tipo: [32]byte

fmt.Printf("%x\n", hash)
// b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9

// Puedes usar el hash como key de map:
type FileHash [32]byte
dedup := make(map[FileHash]string) // hash → filename

// Comparar hashes directamente:
hash2 := sha256.Sum256([]byte("hello world"))
fmt.Println(hash == hash2) // true
```

### MAC addresses

```go
type MACAddr [6]byte

func (m MACAddr) String() string {
    return fmt.Sprintf("%02x:%02x:%02x:%02x:%02x:%02x",
        m[0], m[1], m[2], m[3], m[4], m[5])
}

func (m MACAddr) IsMulticast() bool {
    return m[0]&0x01 != 0
}

func (m MACAddr) IsLocal() bool {
    return m[0]&0x02 != 0
}

mac := MACAddr{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}
fmt.Println(mac)             // aa:bb:cc:dd:ee:ff
fmt.Println(mac.IsLocal())   // true (bit U/L set)
```

### Buffers de I/O con tamano fijo

```go
// Leer archivos en chunks de tamano fijo
func countLines(path string) (int, error) {
    f, err := os.Open(path)
    if err != nil {
        return 0, err
    }
    defer f.Close()

    var buf [32 * 1024]byte // 32KB buffer en el stack
    count := 0

    for {
        n, err := f.Read(buf[:]) // buf[:] convierte a slice
        for i := 0; i < n; i++ {
            if buf[i] == '\n' {
                count++
            }
        }
        if err != nil {
            if err == io.EOF {
                break
            }
            return 0, err
        }
    }
    return count, nil
}

// El buffer vive en el stack si no escapa
// Esto es mas eficiente que make([]byte, 32*1024) que va al heap
```

---

## 8. Array como backing store de slices

### La relacion fundamental

```go
// Un slice es un descriptor (header) que referencia un array subyacente

arr := [5]int{10, 20, 30, 40, 50}

// Crear slice desde array:
s := arr[1:4] // slice que apunta a arr[1], arr[2], arr[3]

fmt.Println(s)       // [20 30 40]
fmt.Println(len(s))  // 3
fmt.Println(cap(s))  // 4 (desde arr[1] hasta el final de arr)

// Modificar el slice modifica el array:
s[0] = 999
fmt.Println(arr) // [10 999 30 40 50] — arr[1] cambio

// Modificar el array afecta al slice:
arr[2] = 888
fmt.Println(s) // [999 888 40] — s[1] cambio
```

```
┌──────────────────────────────────────────────────────────────┐
│  ARRAY Y SLICE: RELACION                                     │
│                                                              │
│  Array arr:                                                  │
│  ┌─────┬─────┬─────┬─────┬─────┐                            │
│  │  10 │  20 │  30 │  40 │  50 │                            │
│  └─────┴─────┴─────┴─────┴─────┘                            │
│    [0]   [1]   [2]   [3]   [4]                               │
│           ↑                 ↑                                │
│           │    s = arr[1:4] │                                │
│           │                 │                                │
│  Slice s: ptr───→ len=3    cap=4                             │
│                                                              │
│  s[0] = arr[1]                                               │
│  s[1] = arr[2]                                               │
│  s[2] = arr[3]                                               │
│                                                              │
│  Comparten la misma memoria — NO es una copia                │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Convertir array a slice

```go
arr := [5]int{1, 2, 3, 4, 5}

// Slice completo:
s1 := arr[:]    // todos los elementos
s2 := arr[0:5]  // equivalente

// Slice parcial:
s3 := arr[2:]   // desde indice 2 hasta el final: [3, 4, 5]
s4 := arr[:3]   // desde el inicio hasta indice 3: [1, 2, 3]
s5 := arr[1:4]  // [2, 3, 4]

// Full slice expression (controla capacidad):
s6 := arr[1:4:4] // len=3, cap=3 (no puede ver mas alla)
```

### Por que esto importa

```go
// Cuando pasas arr[:] a una funcion, pasas un SLICE
// El slice apunta al array original — modificaciones se reflejan

func zeroFirst(s []int) {
    if len(s) > 0 {
        s[0] = 0
    }
}

arr := [5]int{1, 2, 3, 4, 5}
zeroFirst(arr[:])
fmt.Println(arr) // [0 2 3 4 5] — modificado!

// Para evitar esto: copiar
cpy := arr // Copia del array
zeroFirst(cpy[:])
fmt.Println(arr) // [1 2 3 4 5] — original intacto
```

---

## 9. Arrays y punteros

### Puntero a array

```go
// Pasar puntero evita la copia
func clear(arr *[5]int) {
    for i := range arr {
        arr[i] = 0 // Go permite arr[i] en lugar de (*arr)[i]
    }
}

data := [5]int{1, 2, 3, 4, 5}
clear(&data)
fmt.Println(data) // [0 0 0 0 0]
```

### new() con arrays

```go
// new retorna un puntero a zero value
p := new([100]int)      // *[100]int, todos ceros
fmt.Println(len(p))     // No: len espera array, no *array
fmt.Println(len(*p))    // 100
fmt.Println(p[0])       // 0 — Go permite indexar *array directamente

// Equivalente a:
arr := [100]int{}
p2 := &arr
```

### Array vs puntero a array en range

```go
arr := [3]int{1, 2, 3}

// range sobre array: itera sobre COPIA del array
for i, v := range arr {
    arr[i] = 0 // Modifica el original, pero v viene de la copia
    fmt.Println(v) // 1, 2, 3 (valores originales de la copia)
}
fmt.Println(arr) // [0 0 0]

// range sobre puntero a array: itera sobre el original
arr = [3]int{1, 2, 3}
for i, v := range &arr {
    arr[i] = v * 10 // Modifica durante iteracion
}
fmt.Println(arr) // [10, 20, 30]
// Nota: los valores v son del original, que se va modificando
// i=0: v=1, arr[0]=10
// i=1: v=2 (original aun no cambiado en [1]), arr[1]=20
// i=2: v=3 (original aun no cambiado en [2]), arr[2]=30
```

---

## 10. Funciones utiles con arrays

### len() — constante en compile time

```go
arr := [5]int{1, 2, 3, 4, 5}
fmt.Println(len(arr)) // 5 — conocido en compile time

// El compilador puede optimizar porque len es constante
// No hay overhead de runtime como con slices
```

### Copiar con asignacion

```go
// Para arrays, asignacion = copia profunda (si elementos son valor)
original := [3]int{1, 2, 3}
backup := original // Copia completa

original[0] = 999
fmt.Println(original) // [999 2 3]
fmt.Println(backup)   // [1 2 3] — intacto

// Para arrays de punteros: copia SHALLOW
type Node struct{ Name string }
ptrs := [2]*Node{{Name: "a"}, {Name: "b"}}
ptrsCopy := ptrs
ptrsCopy[0].Name = "modified" // Modifica el struct compartido!
fmt.Println(ptrs[0].Name)     // "modified" — ambos apuntan al mismo struct
```

### Initializar a un valor especifico

```go
// No hay funcion memset. Para llenar un array:

// Opcion 1: loop
var buf [1024]byte
for i := range buf {
    buf[i] = 0xFF
}

// Opcion 2: literal (solo para tamanos pequenos)
filled := [5]int{1, 1, 1, 1, 1}

// Opcion 3: para arrays de bytes, copy desde un pattern
pattern := []byte{0xFF}
var buf2 [1024]byte
for i := range buf2 {
    buf2[i] = pattern[0]
}
```

---

## 11. Arrays y el compilador

### Stack vs heap allocation

```go
// Arrays pequenos generalmente viven en el stack
func small() {
    var buf [64]byte // Stack — rapido
    _ = buf
}

// Arrays grandes pueden moverse al heap
func large() {
    var buf [10_000_000]byte // Probablemente heap
    _ = buf
}

// El escape analysis decide:
func escape() *[100]int {
    var arr [100]int
    return &arr // arr escapa al heap porque se retorna
}

// Verificar:
// go build -gcflags="-m" ./...
// "moved to heap: arr"
```

### Constantes y arrays

```go
// len() de un array es una constante en compile time
// Puede usarse donde se requiere una constante
var arr [5]int
const size = len(arr) // Err: len(arr) is not constant (variable)
// ↑ Solo funciona si arr es una constante, que los arrays no son

// Pero el compilador SI sabe el tamano:
// El tamano se resuelve en compile time para optimizaciones

// Para constantes de tamano, usa const:
const bufSize = 4096
var buf [bufSize]byte
```

### Inlining con arrays pequenos

```go
// El compilador puede inlinear funciones que usan arrays pequenos
// porque el tamano es conocido y la copia es predecible

func sum4(arr [4]int) int { // Candidato a inlining
    return arr[0] + arr[1] + arr[2] + arr[3]
}

// Con slices, el compilador tiene menos certeza
// porque len() es runtime y puede necesitar bounds checks
```

---

## 12. Patrones SysAdmin con arrays

### Patron 1: Tabla de permisos Unix

```go
type Permission [3]uint8 // owner, group, other

func (p Permission) String() string {
    toStr := func(perm uint8) string {
        r := "-"
        w := "-"
        x := "-"
        if perm&4 != 0 { r = "r" }
        if perm&2 != 0 { w = "w" }
        if perm&1 != 0 { x = "x" }
        return r + w + x
    }
    return toStr(p[0]) + toStr(p[1]) + toStr(p[2])
}

func (p Permission) Octal() string {
    return fmt.Sprintf("%d%d%d", p[0], p[1], p[2])
}

func (p Permission) OwnerCanWrite() bool {
    return p[0]&2 != 0
}

func (p Permission) WorldReadable() bool {
    return p[2]&4 != 0
}

perm := Permission{7, 5, 5}
fmt.Println(perm)             // rwxr-xr-x
fmt.Println(perm.Octal())     // 755
fmt.Println(perm.WorldReadable()) // true
```

### Patron 2: Subnet con IP y mask

```go
type IPv4Addr [4]byte
type Subnet struct {
    Network IPv4Addr
    Mask    IPv4Addr
}

func (s Subnet) Contains(ip IPv4Addr) bool {
    for i := 0; i < 4; i++ {
        if ip[i]&s.Mask[i] != s.Network[i]&s.Mask[i] {
            return false
        }
    }
    return true
}

func (s Subnet) String() string {
    ones := 0
    for _, b := range s.Mask {
        for bit := 7; bit >= 0; bit-- {
            if b&(1<<uint(bit)) != 0 {
                ones++
            }
        }
    }
    return fmt.Sprintf("%d.%d.%d.%d/%d",
        s.Network[0], s.Network[1], s.Network[2], s.Network[3], ones)
}

subnet := Subnet{
    Network: IPv4Addr{192, 168, 1, 0},
    Mask:    IPv4Addr{255, 255, 255, 0},
}

fmt.Println(subnet) // 192.168.1.0/24
fmt.Println(subnet.Contains(IPv4Addr{192, 168, 1, 100})) // true
fmt.Println(subnet.Contains(IPv4Addr{10, 0, 0, 1}))      // false
```

### Patron 3: File checksum verification

```go
type FileChecksum struct {
    Path string
    SHA  [32]byte
}

func checksumFile(path string) (FileChecksum, error) {
    data, err := os.ReadFile(path)
    if err != nil {
        return FileChecksum{}, err
    }
    return FileChecksum{
        Path: path,
        SHA:  sha256.Sum256(data),
    }, nil
}

func verifyChecksums(files []FileChecksum) []string {
    var mismatches []string
    for _, fc := range files {
        current, err := checksumFile(fc.Path)
        if err != nil {
            mismatches = append(mismatches, fmt.Sprintf("%s: %v", fc.Path, err))
            continue
        }
        if current.SHA != fc.SHA { // Comparacion directa de arrays!
            mismatches = append(mismatches, fmt.Sprintf("%s: checksum mismatch", fc.Path))
        }
    }
    return mismatches
}
```

### Patron 4: Ring buffer con array fijo

```go
type RingBuffer struct {
    data  [64]string // Tamano fijo — predecible en memoria
    head  int
    tail  int
    count int
}

func (rb *RingBuffer) Push(s string) bool {
    if rb.count == len(rb.data) {
        return false // Buffer lleno
    }
    rb.data[rb.tail] = s
    rb.tail = (rb.tail + 1) % len(rb.data)
    rb.count++
    return true
}

func (rb *RingBuffer) Pop() (string, bool) {
    if rb.count == 0 {
        return "", false
    }
    s := rb.data[rb.head]
    rb.data[rb.head] = "" // Clear para GC
    rb.head = (rb.head + 1) % len(rb.data)
    rb.count--
    return s, true
}

func (rb *RingBuffer) Len() int {
    return rb.count
}

// Uso: buffer de ultimos N logs
var logBuffer RingBuffer
logBuffer.Push("2024-01-01 server started")
logBuffer.Push("2024-01-01 request received")
// El ring buffer sobrescribe los mas antiguos cuando se llena
```

---

## 13. Comparacion: Go vs C vs Rust

### Go: arrays como valores

```go
// Tamano es parte del tipo
var buf [256]byte

// Paso por valor (copia)
func process(arr [256]byte) { ... }
process(buf) // Copia 256 bytes

// Paso por referencia (puntero)
func processRef(arr *[256]byte) { ... }
processRef(&buf) // Copia 8 bytes

// No hay decaimiento a puntero
// Arrays son comparables
buf1 := [3]int{1, 2, 3}
buf2 := [3]int{1, 2, 3}
fmt.Println(buf1 == buf2) // true
```

### C: arrays decaen a punteros

```c
// Tamano es parte del tipo (parcialmente)
int buf[256];

// ¡¡Array decae a puntero al pasar a funcion!!
void process(int arr[]) {    // arr es int*, NO int[256]
    sizeof(arr);              // sizeof(int*) = 8, NO sizeof(int[256])
}
process(buf);                 // Pasa PUNTERO, no copia

// Para preservar el tamano:
void process_fixed(int (*arr)[256]) { // Puntero a array de 256
    sizeof(*arr); // sizeof(int[256]) = 1024
}

// Arrays NO son comparables con ==
// int a[3] = {1,2,3}, b[3] = {1,2,3};
// a == b; // Compara DIRECCIONES, no contenido → probablemente false
// Necesitas memcmp(a, b, sizeof(a))

// No hay bounds checking (fuente de CVEs)
buf[256] = 42; // Buffer overflow — UB, no error
```

### Rust: arrays con const generics

```rust
// Tamano es parte del tipo (igual que Go)
let buf: [u8; 256] = [0; 256];

// Paso por valor (Copy trait para tipos pequenos)
fn process(arr: [u8; 256]) { ... }
process(buf); // Copia o move segun Copy trait

// Paso por referencia (borrow)
fn process_ref(arr: &[u8; 256]) { ... }
process_ref(&buf); // Referencia, sin copia

// Const generics: funciones genericas sobre tamano
fn sum<const N: usize>(arr: [i32; N]) -> i32 {
    arr.iter().sum()
}
sum([1, 2, 3]);      // N = 3
sum([1, 2, 3, 4, 5]); // N = 5
// ↑ Go NO puede hacer esto

// Arrays son comparables
let a = [1, 2, 3];
let b = [1, 2, 3];
assert_eq!(a, b); // true

// Bounds checking en runtime (panic, no UB)
// buf[256]; // panic: index out of bounds
```

### Tabla comparativa

```
┌──────────────────────┬─────────────────────┬──────────────────────┬──────────────────────┐
│ Aspecto              │ Go                  │ C                    │ Rust                 │
├──────────────────────┼─────────────────────┼──────────────────────┼──────────────────────┤
│ Tamano en tipo       │ Si ([3]int)         │ Parcial (decae)      │ Si ([i32; 3])        │
│ Paso por defecto     │ Valor (copia)       │ Puntero (decay)      │ Valor (move/copy)    │
│ Bounds checking      │ Si (panic)          │ No (UB)              │ Si (panic)           │
│ Comparacion ==       │ Si                  │ No (compara ptrs)    │ Si (PartialEq)       │
│ Como key de map      │ Si                  │ No                   │ Si (Hash+Eq)         │
│ Const generics       │ No                  │ No (macros)          │ Si (const N: usize)  │
│ Inicializacion       │ Zero value auto     │ Basura (local)       │ Obligatoria          │
│ Stack allocation     │ Si (si no escapa)   │ Si (local)           │ Si (por defecto)     │
│ Multidimensional     │ [M][N]T             │ T[M][N]              │ [[T; N]; M]          │
│ Slice desde array    │ arr[:]              │ Manual (ptr+len)     │ &arr[..] o arr.as_   │
│ Sizeof               │ N * sizeof(T)       │ N * sizeof(T)        │ N * sizeof(T)        │
│ VLA (variable len)   │ No                  │ Si (C99, no siempre) │ No                   │
└──────────────────────┴─────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 14. Programa completo: Network Subnet Calculator

Un calculador de subnets que usa arrays para representar IPs, mascaras, y MAC addresses, demostrando arrays como tipos, comparacion, y uso como keys de map.

```go
package main

import (
	"crypto/sha256"
	"fmt"
	"math/bits"
	"os"
	"sort"
	"strconv"
	"strings"
)

// ─── Tipos basados en arrays ────────────────────────────────────

type IPv4 [4]byte
type MACAddr [6]byte
type Checksum [32]byte

// ─── IPv4 methods ───────────────────────────────────────────────

func (ip IPv4) String() string {
	return fmt.Sprintf("%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3])
}

func ParseIPv4(s string) (IPv4, error) {
	var ip IPv4
	parts := strings.Split(s, ".")
	if len(parts) != 4 {
		return ip, fmt.Errorf("invalid IPv4 format: %q", s)
	}
	for i, part := range parts {
		n, err := strconv.Atoi(part)
		if err != nil || n < 0 || n > 255 {
			return ip, fmt.Errorf("invalid octet %q in %q", part, s)
		}
		ip[i] = byte(n)
	}
	return ip, nil
}

func (ip IPv4) IsPrivate() bool {
	return ip[0] == 10 ||
		(ip[0] == 172 && ip[1] >= 16 && ip[1] <= 31) ||
		(ip[0] == 192 && ip[1] == 168)
}

func (ip IPv4) IsLoopback() bool {
	return ip[0] == 127
}

func (ip IPv4) IsMulticast() bool {
	return ip[0] >= 224 && ip[0] <= 239
}

func (ip IPv4) Class() string {
	switch {
	case ip[0] < 128:
		return "A"
	case ip[0] < 192:
		return "B"
	case ip[0] < 224:
		return "C"
	case ip[0] < 240:
		return "D (multicast)"
	default:
		return "E (reserved)"
	}
}

func (ip IPv4) ToUint32() uint32 {
	return uint32(ip[0])<<24 | uint32(ip[1])<<16 | uint32(ip[2])<<8 | uint32(ip[3])
}

func IPv4FromUint32(n uint32) IPv4 {
	return IPv4{
		byte(n >> 24),
		byte(n >> 16),
		byte(n >> 8),
		byte(n),
	}
}

// ─── Subnet ─────────────────────────────────────────────────────

type Subnet struct {
	Network IPv4
	Mask    IPv4
	CIDR    int
}

func NewSubnet(network IPv4, cidr int) (Subnet, error) {
	if cidr < 0 || cidr > 32 {
		return Subnet{}, fmt.Errorf("invalid CIDR: %d", cidr)
	}

	// Generar mascara desde CIDR
	var maskBits uint32
	if cidr > 0 {
		maskBits = ^uint32(0) << (32 - cidr)
	}
	mask := IPv4FromUint32(maskBits)

	// Calcular network address (aplicar mascara)
	netAddr := IPv4{
		network[0] & mask[0],
		network[1] & mask[1],
		network[2] & mask[2],
		network[3] & mask[3],
	}

	return Subnet{Network: netAddr, Mask: mask, CIDR: cidr}, nil
}

func ParseSubnet(s string) (Subnet, error) {
	parts := strings.Split(s, "/")
	if len(parts) != 2 {
		return Subnet{}, fmt.Errorf("expected format IP/CIDR, got %q", s)
	}
	ip, err := ParseIPv4(parts[0])
	if err != nil {
		return Subnet{}, err
	}
	cidr, err := strconv.Atoi(parts[1])
	if err != nil {
		return Subnet{}, fmt.Errorf("invalid CIDR: %q", parts[1])
	}
	return NewSubnet(ip, cidr)
}

func (s Subnet) String() string {
	return fmt.Sprintf("%s/%d", s.Network, s.CIDR)
}

func (s Subnet) Broadcast() IPv4 {
	invertedMask := IPv4{
		^s.Mask[0],
		^s.Mask[1],
		^s.Mask[2],
		^s.Mask[3],
	}
	return IPv4{
		s.Network[0] | invertedMask[0],
		s.Network[1] | invertedMask[1],
		s.Network[2] | invertedMask[2],
		s.Network[3] | invertedMask[3],
	}
}

func (s Subnet) FirstHost() IPv4 {
	n := s.Network.ToUint32() + 1
	return IPv4FromUint32(n)
}

func (s Subnet) LastHost() IPv4 {
	b := s.Broadcast().ToUint32() - 1
	return IPv4FromUint32(b)
}

func (s Subnet) TotalHosts() uint32 {
	if s.CIDR >= 31 {
		return 0 // Point-to-point o host route
	}
	return (1 << (32 - s.CIDR)) - 2
}

func (s Subnet) Contains(ip IPv4) bool {
	// Comparacion de arrays directa despues de aplicar mascara
	maskedIP := IPv4{
		ip[0] & s.Mask[0],
		ip[1] & s.Mask[1],
		ip[2] & s.Mask[2],
		ip[3] & s.Mask[3],
	}
	return maskedIP == s.Network // ← Comparacion de arrays con ==
}

func (s Subnet) Overlaps(other Subnet) bool {
	return s.Contains(other.Network) || s.Contains(other.Broadcast()) ||
		other.Contains(s.Network) || other.Contains(s.Broadcast())
}

// ─── MAC address methods ────────────────────────────────────────

func (m MACAddr) String() string {
	return fmt.Sprintf("%02x:%02x:%02x:%02x:%02x:%02x",
		m[0], m[1], m[2], m[3], m[4], m[5])
}

func (m MACAddr) Vendor() string {
	// OUI lookup (primeros 3 bytes)
	oui := [3]byte{m[0], m[1], m[2]}
	vendors := map[[3]byte]string{
		{0x00, 0x50, 0x56}: "VMware",
		{0x08, 0x00, 0x27}: "VirtualBox",
		{0x52, 0x54, 0x00}: "QEMU/KVM",
		{0xDC, 0xA6, 0x32}: "Raspberry Pi",
		{0xB8, 0x27, 0xEB}: "Raspberry Pi",
	}
	if vendor, ok := vendors[oui]; ok {
		return vendor
	}
	return "Unknown"
}

// ─── Inventory: arrays como keys de map ─────────────────────────

type NetworkDevice struct {
	Hostname string
	IP       IPv4
	MAC      MACAddr
	Subnet   string
}

type Inventory struct {
	byIP   map[IPv4]*NetworkDevice    // IPv4 array como key
	byMAC  map[MACAddr]*NetworkDevice // MACAddr array como key
	byHash map[Checksum]string        // SHA256 como key
}

func NewInventory() *Inventory {
	return &Inventory{
		byIP:   make(map[IPv4]*NetworkDevice),
		byMAC:  make(map[MACAddr]*NetworkDevice),
		byHash: make(map[Checksum]string),
	}
}

func (inv *Inventory) Add(dev NetworkDevice) {
	inv.byIP[dev.IP] = &dev
	inv.byMAC[dev.MAC] = &dev
}

func (inv *Inventory) FindByIP(ip IPv4) (*NetworkDevice, bool) {
	dev, ok := inv.byIP[ip]
	return dev, ok
}

func (inv *Inventory) FindByMAC(mac MACAddr) (*NetworkDevice, bool) {
	dev, ok := inv.byMAC[mac]
	return dev, ok
}

func (inv *Inventory) AddFileHash(path string, hash Checksum) {
	inv.byHash[hash] = path
}

func (inv *Inventory) HasFileHash(hash Checksum) (string, bool) {
	path, ok := inv.byHash[hash]
	return path, ok
}

// ─── Formateo ───────────────────────────────────────────────────

func printSubnetInfo(s Subnet) {
	fmt.Printf("%-18s %s\n", "Subnet:", s)
	fmt.Printf("%-18s %s\n", "Network:", s.Network)
	fmt.Printf("%-18s %s\n", "Mask:", s.Mask)
	fmt.Printf("%-18s %s\n", "Broadcast:", s.Broadcast())
	fmt.Printf("%-18s %s\n", "First host:", s.FirstHost())
	fmt.Printf("%-18s %s\n", "Last host:", s.LastHost())
	fmt.Printf("%-18s %d\n", "Total hosts:", s.TotalHosts())
	fmt.Printf("%-18s %s\n", "Class:", s.Network.Class())
	fmt.Printf("%-18s %v\n", "Private:", s.Network.IsPrivate())
}

// ─── Main ───────────────────────────────────────────────────────

func main() {
	// Parsear subnets
	subnets := []string{
		"192.168.1.0/24",
		"10.0.0.0/8",
		"172.16.0.0/12",
		"192.168.1.128/25",
	}

	fmt.Println("=== Subnet Calculator ===\n")

	var parsed []Subnet
	for _, s := range subnets {
		subnet, err := ParseSubnet(s)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error parsing %s: %v\n", s, err)
			continue
		}
		parsed = append(parsed, subnet)
	}

	for i, s := range parsed {
		printSubnetInfo(s)
		fmt.Println()

		// Verificar overlap con otros
		for j := i + 1; j < len(parsed); j++ {
			if s.Overlaps(parsed[j]) {
				fmt.Printf("  ⚠ %s overlaps with %s\n\n", s, parsed[j])
			}
		}
	}

	// Test containment (usa comparacion de arrays)
	testIPs := []string{
		"192.168.1.50",
		"192.168.1.200",
		"10.0.0.1",
		"8.8.8.8",
	}

	fmt.Println("=== IP Containment ===\n")
	fmt.Printf("%-18s", "IP")
	for _, s := range parsed {
		fmt.Printf("%-20s", s.String())
	}
	fmt.Println()
	fmt.Println(strings.Repeat("-", 18+20*len(parsed)))

	for _, ipStr := range testIPs {
		ip, err := ParseIPv4(ipStr)
		if err != nil {
			continue
		}
		fmt.Printf("%-18s", ip)
		for _, s := range parsed {
			if s.Contains(ip) {
				fmt.Printf("%-20s", "YES")
			} else {
				fmt.Printf("%-20s", "no")
			}
		}
		fmt.Println()
	}

	// Inventory demo (arrays como keys de map)
	fmt.Println("\n=== Device Inventory ===\n")

	inv := NewInventory()
	inv.Add(NetworkDevice{
		Hostname: "web01",
		IP:       IPv4{192, 168, 1, 10},
		MAC:      MACAddr{0x00, 0x50, 0x56, 0xAA, 0xBB, 0xCC},
		Subnet:   "192.168.1.0/24",
	})
	inv.Add(NetworkDevice{
		Hostname: "db01",
		IP:       IPv4{192, 168, 1, 20},
		MAC:      MACAddr{0x08, 0x00, 0x27, 0x11, 0x22, 0x33},
		Subnet:   "192.168.1.0/24",
	})
	inv.Add(NetworkDevice{
		Hostname: "kvm-host",
		IP:       IPv4{10, 0, 0, 5},
		MAC:      MACAddr{0x52, 0x54, 0x00, 0xDE, 0xAD, 0x01},
		Subnet:   "10.0.0.0/8",
	})

	// Lookup by IP (array como key)
	lookupIP := IPv4{192, 168, 1, 10}
	if dev, ok := inv.FindByIP(lookupIP); ok {
		fmt.Printf("Found by IP %s: %s (MAC: %s, vendor: %s)\n",
			lookupIP, dev.Hostname, dev.MAC, dev.MAC.Vendor())
	}

	// Lookup by MAC (array como key)
	lookupMAC := MACAddr{0x52, 0x54, 0x00, 0xDE, 0xAD, 0x01}
	if dev, ok := inv.FindByMAC(lookupMAC); ok {
		fmt.Printf("Found by MAC %s: %s (IP: %s, vendor: %s)\n",
			lookupMAC, dev.Hostname, dev.IP, lookupMAC.Vendor())
	}

	// File hash demo (SHA-256 arrays como keys)
	fmt.Println("\n=== File Checksums ===\n")

	files := map[string]string{
		"/etc/hostname":    "webserver01",
		"/etc/resolv.conf": "nameserver 8.8.8.8\nnameserver 8.8.4.4\n",
	}

	for path, content := range files {
		hash := sha256.Sum256([]byte(content)) // Retorna [32]byte
		inv.AddFileHash(path, hash)
		fmt.Printf("%-20s SHA256: %x\n", path, hash[:8]) // Primeros 8 bytes
	}

	// Verificar integridad
	checkHash := sha256.Sum256([]byte("webserver01"))
	if path, ok := inv.HasFileHash(checkHash); ok {
		fmt.Printf("\nHash match found: %s\n", path)
	}

	// Array comparison demo
	fmt.Println("\n=== Array Comparisons ===\n")
	ip1 := IPv4{192, 168, 1, 1}
	ip2 := IPv4{192, 168, 1, 1}
	ip3 := IPv4{192, 168, 1, 2}
	fmt.Printf("%s == %s : %v\n", ip1, ip2, ip1 == ip2) // true
	fmt.Printf("%s == %s : %v\n", ip1, ip3, ip1 == ip3) // false

	// Sort by IP (using ToUint32)
	allIPs := []IPv4{
		{192, 168, 1, 50},
		{10, 0, 0, 1},
		{192, 168, 1, 10},
		{172, 16, 0, 1},
	}
	sort.Slice(allIPs, func(i, j int) bool {
		return allIPs[i].ToUint32() < allIPs[j].ToUint32()
	})
	fmt.Println("\nSorted IPs:")
	for _, ip := range allIPs {
		fmt.Printf("  %s\n", ip)
	}
}
```

### Uso de arrays en el programa

```
┌──────────────────────────────────────────────────────────────┐
│  ARRAYS EN EL SUBNET CALCULATOR                              │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Tipos basados en arrays:                                    │
│  ├─ IPv4 [4]byte      → direccion IP                         │
│  ├─ MACAddr [6]byte   → direccion MAC                        │
│  ├─ Checksum [32]byte → hash SHA-256                         │
│  └─ [3]byte           → OUI prefix (vendor lookup)           │
│                                                              │
│  Arrays como valores:                                        │
│  ├─ IPv4{192,168,1,0} — literal de array                     │
│  ├─ sha256.Sum256()   — retorna [32]byte                     │
│  └─ IPv4FromUint32()  — construye array desde entero         │
│                                                              │
│  Arrays como keys de map (comparables):                      │
│  ├─ map[IPv4]*Device    — lookup por IP                      │
│  ├─ map[MACAddr]*Device — lookup por MAC                     │
│  ├─ map[Checksum]string — dedup por hash                     │
│  └─ map[[3]byte]string  — vendor lookup por OUI              │
│                                                              │
│  Comparacion directa con ==:                                 │
│  ├─ maskedIP == s.Network  — containment check               │
│  ├─ ip1 == ip2             — igualdad                        │
│  └─ hash1 == hash2         — verificacion de checksum        │
│                                                              │
│  Operaciones bitwise en arrays:                              │
│  ├─ ip & mask              — calcular network                │
│  ├─ ^mask                  — mascara invertida               │
│  ├─ network | invMask      — broadcast address               │
│  └─ mac[0] & 0x01          — multicast bit                   │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 15. Gotchas y errores comunes

### Gotcha 1: Array se copia al pasar a funcion

```go
func clear(arr [3]int) {
    for i := range arr {
        arr[i] = 0
    }
    // Solo modifica la copia local
}

data := [3]int{1, 2, 3}
clear(data)
fmt.Println(data) // [1 2 3] — no cambio!

// ✓ Pasar puntero o usar slice
```

### Gotcha 2: Array size mismatch

```go
func process(data [4]int) {}

arr := [3]int{1, 2, 3}
// process(arr) // ERROR: cannot use arr (type [3]int) as type [4]int
```

### Gotcha 3: range copia el valor

```go
type Large struct {
    Data [1000000]byte
}

items := [3]Large{...}
for _, item := range items {
    // item es una COPIA de Large — 1MB copiado por iteracion!
    _ = item
}

// ✓ Usar indice para evitar copia:
for i := range items {
    process(&items[i]) // Sin copia
}
```

### Gotcha 4: Array de punteros — shallow copy

```go
type Config struct{ Name string }

arr1 := [2]*Config{{Name: "a"}, {Name: "b"}}
arr2 := arr1 // Copia los PUNTEROS, no los Config

arr2[0].Name = "modified"
fmt.Println(arr1[0].Name) // "modified" — compartido!

// ✓ Deep copy manual:
arr3 := arr1
for i, ptr := range arr1 {
    cpy := *ptr // Copia el Config
    arr3[i] = &cpy
}
```

### Gotcha 5: No puedes usar variable como tamano

```go
n := 5
// var arr [n]int // ERROR: non-constant array bound n

// ✓ Constante:
const size = 5
var arr [size]int // OK

// ✓ O usar slice:
s := make([]int, n) // OK
```

---

## 16. Tabla de errores comunes

```
┌────┬──────────────────────────────────────┬────────────────────────────────────────┬─────────┐
│ #  │ Error                                │ Solucion                               │ Nivel   │
├────┼──────────────────────────────────────┼────────────────────────────────────────┼─────────┤
│  1 │ Esperar modificacion en funcion      │ Pasar puntero (*[N]T) o slice ([]T)   │ Logic   │
│  2 │ [3]int asignado a [4]int             │ Tipos diferentes — usar slice          │ Compile │
│  3 │ Variable como tamano de array        │ Usar const o slice con make()          │ Compile │
│  4 │ Range copia structs grandes          │ Iterar con indice: &items[i]           │ Perf    │
│  5 │ Array de punteros = shallow copy     │ Deep copy manual si necesitas          │ Logic   │
│  6 │ Comparar arrays de tipos no          │ Usar reflect.DeepEqual o loop manual   │ Compile │
│    │ comparables                          │                                        │         │
│  7 │ Array grande en stack overflow       │ Usar slice (heap) o puntero a array    │ Runtime │
│  8 │ Olvidar [:] al pasar a func([]T)     │ arr[:] convierte [N]T a []T            │ Compile │
│  9 │ Multidimensional con tamanos         │ Cada dimension es un tipo separado     │ Compile │
│    │ diferentes                           │                                        │         │
│ 10 │ Asumir que arrays son como slices    │ Arrays son valores fijos, slices refs  │ Design  │
└────┴──────────────────────────────────────┴────────────────────────────────────────┴─────────┘
```

---

## 17. Ejercicios de prediccion

**Ejercicio 1**: ¿Que imprime?

```go
a := [3]int{1, 2, 3}
b := a
b[0] = 999
fmt.Println(a[0])
```

<details>
<summary>Respuesta</summary>

```
1
```

`b := a` copia el array completo. Modificar `b` no afecta a `a`.
</details>

**Ejercicio 2**: ¿Compila?

```go
var a [3]int
var b [4]int
a = b
```

<details>
<summary>Respuesta</summary>

No compila. Error: `cannot use b (variable of type [4]int) as type [3]int`. El tamano es parte del tipo.
</details>

**Ejercicio 3**: ¿Que imprime?

```go
a := [...]int{5, 10, 15}
fmt.Printf("%T\n", a)
```

<details>
<summary>Respuesta</summary>

```
[3]int
```

`[...]` hace que el compilador cuente los elementos. El tipo real es `[3]int`.
</details>

**Ejercicio 4**: ¿Que imprime?

```go
arr := [5]int{2: 100, 4: 200}
fmt.Println(arr)
```

<details>
<summary>Respuesta</summary>

```
[0 0 100 0 200]
```

Indices especificos: `arr[2]=100`, `arr[4]=200`, el resto es zero value.
</details>

**Ejercicio 5**: ¿Que imprime?

```go
a := [2]int{1, 2}
b := [2]int{1, 2}
c := [2]int{2, 1}
fmt.Println(a == b, a == c)
```

<details>
<summary>Respuesta</summary>

```
true false
```

Comparacion elemento por elemento. `a` y `b` son iguales; `a` y `c` no.
</details>

**Ejercicio 6**: ¿Que imprime?

```go
arr := [3]int{10, 20, 30}
s := arr[1:]
s[0] = 999
fmt.Println(arr)
```

<details>
<summary>Respuesta</summary>

```
[10 999 30]
```

`s` es un slice que apunta al array `arr`. `s[0]` es `arr[1]`. Modificar el slice modifica el array.
</details>

**Ejercicio 7**: ¿Que imprime?

```go
func modify(arr [3]int) {
    arr[0] = 999
}

data := [3]int{1, 2, 3}
modify(data)
fmt.Println(data[0])
```

<details>
<summary>Respuesta</summary>

```
1
```

Arrays se pasan por valor. `modify` recibe una copia. El original no cambia.
</details>

**Ejercicio 8**: ¿Que imprime?

```go
arr := [...]int{9: 42}
fmt.Println(len(arr), arr[9])
```

<details>
<summary>Respuesta</summary>

```
10 42
```

`[...]int{9: 42}` crea un array con el indice mas alto siendo 9, por lo que el tamano es 10. `arr[9]=42`, el resto es 0.
</details>

**Ejercicio 9**: ¿Compila?

```go
n := 3
var arr [n]int
```

<details>
<summary>Respuesta</summary>

No compila. Error: `non-constant array bound n`. El tamano de un array debe ser una constante en compile time.
</details>

**Ejercicio 10**: ¿Que imprime?

```go
type IP [4]byte

m := map[IP]string{
    {127, 0, 0, 1}: "localhost",
    {8, 8, 8, 8}:   "google-dns",
}

lookup := IP{8, 8, 8, 8}
fmt.Println(m[lookup])
```

<details>
<summary>Respuesta</summary>

```
google-dns
```

Los arrays son comparables y pueden ser keys de map. `IP{8,8,8,8}` es igual a la key del map.
</details>

---

## 18. Resumen visual

```
┌──────────────────────────────────────────────────────────────┐
│              ARRAYS — RESUMEN                                │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  DECLARACION                                                 │
│  var a [5]int              → zero values                     │
│  b := [3]int{1, 2, 3}     → literal                         │
│  c := [...]int{1, 2, 3}   → compilador cuenta               │
│  d := [5]int{2: 100}      → indices especificos             │
│                                                              │
│  PROPIEDADES FUNDAMENTALES                                   │
│  ├─ Tamano es PARTE DEL TIPO: [3]int ≠ [4]int               │
│  ├─ Paso por VALOR: copia completa al asignar/pasar          │
│  ├─ COMPARABLES con ==: si el tipo elemento lo es            │
│  ├─ Pueden ser keys de MAP (slices no)                       │
│  ├─ Tamano fijo: no crece ni encoge                          │
│  ├─ Zero value: todos elementos en zero value                │
│  └─ len() conocido en compile time                           │
│                                                              │
│  RELACION CON SLICES                                         │
│  ├─ arr[:] convierte array a slice                           │
│  ├─ Slice apunta al array (backing store)                    │
│  ├─ Modificar slice modifica array                           │
│  └─ Usar slices para el 99% del codigo                       │
│                                                              │
│  USOS LEGITIMOS                                              │
│  ├─ IPv4 [4]byte, IPv6 [16]byte                             │
│  ├─ SHA-256 [32]byte (crypto/sha256.Sum256)                  │
│  ├─ MAC [6]byte                                              │
│  ├─ Buffers fijos para I/O (stack allocation)                │
│  ├─ Keys de map (IPs, hashes, coordenadas)                   │
│  └─ Interop con C (cgo: arrays de tamano fijo)               │
│                                                              │
│  PASO A FUNCIONES                                            │
│  ├─ f(arr [N]T) → copia completa (costoso si grande)        │
│  ├─ f(arr *[N]T) → puntero (eficiente)                      │
│  └─ f(arr[:]) → slice (idiomatico, flexible)                 │
│                                                              │
│  ERRORES FRECUENTES                                          │
│  ├─ Esperar mutacion con value pass                          │
│  ├─ Mezclar [3]T con [4]T (tipos diferentes)                │
│  ├─ Variable como tamano (debe ser const)                    │
│  ├─ Range copia structs grandes (usar indice)                │
│  └─ Asumir comportamiento de slice (referencia)              │
│                                                              │
│  GO vs C vs RUST                                             │
│  ├─ Go: valor, comparable, bounds check, no const generics   │
│  ├─ C: decay a puntero, no comparable, no bounds check       │
│  └─ Rust: valor, comparable, bounds check, const generics    │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: T02 - Slices — estructura interna (puntero, longitud, capacidad), make, append, copy
