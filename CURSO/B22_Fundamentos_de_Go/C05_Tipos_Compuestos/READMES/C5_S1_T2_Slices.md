# Slices — Estructura Interna, make, append, copy

## 1. Introduccion

Los slices son la **estructura de datos mas usada en Go**. Son vistas dinamicas sobre arrays — pueden crecer, encogerse, y compartir memoria con otros slices. Mientras que los arrays tienen tamano fijo como parte del tipo, los slices son flexibles: `[]int` puede contener 0 o 10 millones de elementos.

Un slice es internamente un **descriptor de 3 campos**: un puntero al array subyacente, la longitud (cuantos elementos tiene), y la capacidad (cuantos puede tener sin realocar). Entender esta estructura interna es **critico** porque explica el 100% de los comportamientos sorprendentes de los slices: por que `append` a veces modifica el slice original y a veces no, por que dos slices pueden compartir memoria, y por que pasar un slice a una funcion permite que la funcion modifique los elementos pero no la longitud.

En SysAdmin/DevOps, los slices son omnipresentes: listas de hosts, resultados de queries, buffers de I/O, argumentos de comandos, lineas de archivos, colecciones de metricas, y pipelines de procesamiento.

```
┌──────────────────────────────────────────────────────────────┐
│              SLICES EN GO                                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Estructura interna (slice header):                          │
│  ┌──────────┬──────────┬──────────┐                          │
│  │ pointer  │  length  │ capacity │   24 bytes en 64-bit     │
│  └──────────┴──────────┴──────────┘                          │
│       ↓                                                      │
│  ┌────┬────┬────┬────┬────┬────┬────┐                        │
│  │ a0 │ a1 │ a2 │ a3 │ a4 │ a5 │ a6 │  array subyacente     │
│  └────┴────┴────┴────┴────┴────┴────┘                        │
│                                                              │
│  s := arr[2:5]                                               │
│  ├─ pointer → &arr[2]                                        │
│  ├─ length  = 3  (5 - 2)                                    │
│  └─ capacity = 5 (len(arr) - 2)                              │
│                                                              │
│  Propiedades:                                                │
│  ├─ Tamano dinamico (puede crecer con append)                │
│  ├─ Se pasa por valor, PERO el header contiene un puntero    │
│  │   → la funcion puede modificar los ELEMENTOS              │
│  │   → la funcion NO puede modificar len/cap del caller      │
│  ├─ Zero value es nil (no empty slice)                       │
│  ├─ nil slice es funcional: len=0, cap=0, append funciona    │
│  ├─ NO comparable con == (solo con nil)                      │
│  └─ make([]T, len, cap) para pre-alocar                      │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Estructura interna: el slice header

### Los 3 campos

```go
// Un slice es un struct de 3 campos (definido en runtime):
// type slice struct {
//     array unsafe.Pointer  // puntero al primer elemento
//     len   int             // numero de elementos accesibles
//     cap   int             // numero de elementos en el backing array desde pointer
// }

s := []int{10, 20, 30, 40, 50}
fmt.Println(len(s)) // 5 — elementos accesibles
fmt.Println(cap(s)) // 5 — capacidad total

// Tamano del header: siempre 24 bytes en 64-bit
// (8 pointer + 8 len + 8 cap)
fmt.Println(unsafe.Sizeof(s)) // 24 (independiente del contenido)
```

### Visualizar la estructura

```go
s := make([]int, 3, 7)
// s = [0, 0, 0] con capacidad para 7

// Internamente:
// ┌──────────┬──────────┬──────────┐
// │ ptr ──→  │  len=3   │  cap=7   │  ← slice header (24 bytes)
// └──────────┴──────────┴──────────┘
//      ↓
// ┌────┬────┬────┬────┬────┬────┬────┐
// │  0 │  0 │  0 │  _ │  _ │  _ │  _ │  ← backing array (7 ints)
// └────┴────┴────┴────┴────┴────┴────┘
//   [0]  [1]  [2]  [3]  [4]  [5]  [6]
//   ← accesible →  ← oculto (cap) →

s[0] = 10
s[1] = 20
s[2] = 30
// Los elementos [3]-[6] existen pero no son accesibles via s
// s[3] → panic: index out of range [3] with length 3
```

### Pasar slice a funcion: copia del header

```go
func modify(s []int) {
    // s es una COPIA del header (ptr, len, cap)
    // PERO el ptr apunta al MISMO array
    
    s[0] = 999 // ✓ Modifica el array original (via el puntero compartido)
    
    s = append(s, 100) // Modifica la COPIA local del header
    // El caller NO ve este append — su header sigue con el len original
}

original := []int{1, 2, 3}
modify(original)
fmt.Println(original) // [999 2 3] — elemento modificado, pero no se agrego 100
fmt.Println(len(original)) // 3 — len no cambio
```

```
┌──────────────────────────────────────────────────────────────┐
│  PASO DE SLICE A FUNCION                                     │
│                                                              │
│  Caller:                    Funcion:                         │
│  ┌───┬───┬───┐              ┌───┬───┬───┐                    │
│  │ptr│ 3 │ 3 │  original    │ptr│ 3 │ 3 │  s (copia)        │
│  └─┬─┴───┴───┘              └─┬─┴───┴───┘                    │
│    │                           │                              │
│    └──────────┬────────────────┘                              │
│               ↓                                              │
│         ┌────┬────┬────┐                                     │
│         │  1 │  2 │  3 │  array compartido                   │
│         └────┴────┴────┘                                     │
│                                                              │
│  s[0] = 999 → modifica el array → caller ve el cambio        │
│  s = append(s, 100) → modifica la copia de s                 │
│                     → caller NO ve el cambio                 │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 3. Creacion de slices

### Literal

```go
// Slice literal (sin tamano, a diferencia de arrays)
nums := []int{1, 2, 3, 4, 5}
names := []string{"web01", "web02", "db01"}
empty := []int{} // Slice vacio (no nil): len=0, cap=0

// Con indices:
sparse := []int{0: 10, 4: 50} // [10, 0, 0, 0, 50]
```

### make()

```go
// make([]T, length) — crea con zero values
s1 := make([]int, 5) // [0, 0, 0, 0, 0], len=5, cap=5

// make([]T, length, capacity) — pre-alocar capacidad extra
s2 := make([]int, 0, 100) // [], len=0, cap=100
// Util cuando sabes cuantos elementos habra: evita realocaciones

// make([]T, length, capacity) donde length > 0
s3 := make([]int, 3, 10) // [0, 0, 0], len=3, cap=10

// Restriccion: len <= cap
// make([]int, 10, 5) // panic: len larger than cap
```

### Cuando usar make con capacidad

```go
// ✗ Sin pre-alocar: multiples realocaciones
func collectHosts() []string {
    var hosts []string // nil slice
    for _, line := range readLines("/etc/hosts") {
        hosts = append(hosts, parseLine(line))
        // append crece: 0→1→2→4→8→16→32→...
        // Cada crecimiento aloca nuevo array y copia
    }
    return hosts
}

// ✓ Con pre-alocacion: una sola alocacion
func collectHosts() []string {
    lines := readLines("/etc/hosts")
    hosts := make([]string, 0, len(lines)) // Pre-alocar
    for _, line := range lines {
        hosts = append(hosts, parseLine(line))
        // No realoca hasta que len > cap
    }
    return hosts
}
```

### Desde un array

```go
arr := [5]int{10, 20, 30, 40, 50}

s1 := arr[:]    // Slice completo: [10,20,30,40,50], len=5, cap=5
s2 := arr[1:4]  // [20,30,40], len=3, cap=4
s3 := arr[2:]   // [30,40,50], len=3, cap=3
s4 := arr[:3]   // [10,20,30], len=3, cap=5

// Full slice expression — limitar capacidad
s5 := arr[1:3:4] // [20,30], len=2, cap=3
//       [low:high:max]
//       low=1, high=3, max=4
//       len = high - low = 2
//       cap = max - low = 3
```

### nil slice vs empty slice

```go
// nil slice: zero value
var s1 []int
fmt.Println(s1 == nil)  // true
fmt.Println(len(s1))    // 0
fmt.Println(cap(s1))    // 0

// empty slice: no nil pero vacio
s2 := []int{}
fmt.Println(s2 == nil)  // false
fmt.Println(len(s2))    // 0
fmt.Println(cap(s2))    // 0

s3 := make([]int, 0)
fmt.Println(s3 == nil)  // false

// Ambos son funcionales — append, len, range funcionan igual
s1 = append(s1, 1) // OK
s2 = append(s2, 1) // OK

for _, v := range s1 {} // OK (no itera si nil/empty)

// ¿Cuando importa la diferencia?
// - JSON: nil → null, empty → []
// - Algunas APIs verifican == nil
data1, _ := json.Marshal(s1) // "null"     (si s1 es nil)
data2, _ := json.Marshal(s2) // "[]"       (empty slice)

// Recomendacion: usar nil slice como default
var hosts []string // nil — idiomatico
// hosts := []string{} // innecesario en la mayoria de casos
```

---

## 4. append — la operacion fundamental

### Mecanica basica

```go
s := []int{1, 2, 3}
s = append(s, 4)       // Agregar un elemento
s = append(s, 5, 6, 7) // Agregar multiples
fmt.Println(s) // [1 2 3 4 5 6 7]

// append RETORNA un nuevo slice header
// SIEMPRE reasignar: s = append(s, ...)
// Nunca: append(s, ...) sin capturar el retorno
```

### Cuando append realoca

```go
s := make([]int, 3, 5) // len=3, cap=5
// [0, 0, 0, _, _]

// Caso 1: len < cap → NO realoca
s = append(s, 10) // len=4, cap=5
// [0, 0, 0, 10, _]
// El puntero NO cambia — misma memoria

// Caso 2: len == cap → REALOCA
s = append(s, 20) // len=5, cap=5 → ahora lleno
s = append(s, 30) // len=6, cap>5 → nuevo array!
// Go crea un nuevo array mas grande, copia los datos, y apunta ahi
// El array viejo sera recolectado por el GC
```

### Estrategia de crecimiento

```go
// Go usa una estrategia de crecimiento que ha cambiado entre versiones:
//
// Go < 1.18:
// cap < 1024:  cap *= 2 (duplicar)
// cap >= 1024: cap *= 1.25 (crecer 25%)
//
// Go 1.18+:
// Formula suavizada que transiciona gradualmente
// de 2x para slices pequenos a ~1.25x para grandes

// Verificar empiricamente:
s := make([]int, 0)
prevCap := cap(s)
for i := 0; i < 20; i++ {
    s = append(s, i)
    if cap(s) != prevCap {
        fmt.Printf("len=%-4d cap changed: %d → %d (factor: %.2f)\n",
            len(s), prevCap, cap(s),
            float64(cap(s))/float64(max(prevCap, 1)))
        prevCap = cap(s)
    }
}
// len=1    cap changed: 0 → 1
// len=2    cap changed: 1 → 2    (2.00x)
// len=3    cap changed: 2 → 4    (2.00x)
// len=5    cap changed: 4 → 8    (2.00x)
// len=9    cap changed: 8 → 16   (2.00x)
// len=17   cap changed: 16 → 32  (2.00x)
```

### append un slice a otro

```go
a := []int{1, 2, 3}
b := []int{4, 5, 6}

// Concatenar: usa ... para expandir
c := append(a, b...)
fmt.Println(c) // [1 2 3 4 5 6]

// ¡Cuidado! Si a tiene capacidad extra, b se escribe en el backing array de a
a = make([]int, 3, 10) // cap=10, mucho espacio extra
copy(a, []int{1, 2, 3})
c = append(a, b...) // NO realoca — usa el espacio de a
// Si alguien mas tiene un slice sobre el backing array de a,
// sus datos fueron sobrescritos
```

### append retorna — por que importa

```go
// ✗ BUG: ignorar el retorno de append
func addHost(hosts []string, host string) {
    append(hosts, host) // El resultado se pierde!
    // hosts del caller no cambia
}

// ✓ Retornar el nuevo slice
func addHost(hosts []string, host string) []string {
    return append(hosts, host)
}

// ✓ O usar puntero al slice
func addHost(hosts *[]string, host string) {
    *hosts = append(*hosts, host)
}
```

---

## 5. copy — copia controlada

### Mecanica

```go
// func copy(dst, src []T) int
// Copia min(len(dst), len(src)) elementos
// Retorna el numero de elementos copiados

src := []int{1, 2, 3, 4, 5}
dst := make([]int, 3)

n := copy(dst, src) // Copia 3 elementos (min(3, 5))
fmt.Println(dst) // [1 2 3]
fmt.Println(n)   // 3

// dst mas grande que src:
dst2 := make([]int, 7)
n = copy(dst2, src) // Copia 5 elementos (min(7, 5))
fmt.Println(dst2) // [1 2 3 4 5 0 0]
fmt.Println(n)    // 5
```

### Copia defensiva

```go
// Crear una copia independiente del backing array
original := []int{1, 2, 3, 4, 5}

// Metodo 1: make + copy
cpy := make([]int, len(original))
copy(cpy, original)

// Metodo 2: append a nil
cpy2 := append([]int(nil), original...)

// Metodo 3: slices.Clone (Go 1.21+)
cpy3 := slices.Clone(original)

// Verificar independencia
cpy[0] = 999
fmt.Println(original[0]) // 1 — no afectado
```

### copy con overlap

```go
// copy maneja correctamente src y dst que se solapan
s := []int{1, 2, 3, 4, 5}

// Shift left: mover elementos hacia la izquierda
copy(s[0:], s[1:]) // [2, 3, 4, 5, 5]
s = s[:len(s)-1]    // [2, 3, 4, 5]

// Esto funciona porque copy usa memmove internamente
// que maneja overlap correctamente
```

### copy de string a []byte

```go
// Caso especial: copy permite copiar string a []byte
b := make([]byte, 5)
n := copy(b, "hello world") // Copia 5 bytes (min(5, 11))
fmt.Println(string(b)) // "hello"
fmt.Println(n)          // 5
```

---

## 6. Slicing — crear sub-slices

### Sintaxis basica

```go
s := []int{0, 10, 20, 30, 40, 50, 60}

s1 := s[2:5]  // [20, 30, 40], len=3, cap=5
s2 := s[:3]   // [0, 10, 20], len=3, cap=7
s3 := s[4:]   // [40, 50, 60], len=3, cap=3
s4 := s[:]    // [0,10,20,30,40,50,60], len=7, cap=7

// Los bounds:
// s[low:high] → len = high - low, cap = cap(s) - low
```

### Comparten backing array

```go
original := []int{0, 10, 20, 30, 40}
sub := original[1:4] // [10, 20, 30]

sub[0] = 999
fmt.Println(original) // [0, 999, 20, 30, 40] — ¡modificado!

// original y sub apuntan al MISMO array:
// ┌────┬─────┬────┬────┬────┐
// │  0 │ 999 │ 20 │ 30 │ 40 │  backing array
// └────┴─────┴────┴────┴────┘
//   ↑         ↑              ↑
//   original  sub            sub puede crecer hasta aqui (cap=4)
```

### Full slice expression: limitar la capacidad

```go
original := []int{0, 10, 20, 30, 40}

// Sin limitar cap:
sub := original[1:3] // len=2, cap=4
sub = append(sub, 999) // Escribe en original[3]!
fmt.Println(original) // [0, 10, 20, 999, 40]

// ✓ Con full slice expression:
original = []int{0, 10, 20, 30, 40}
sub = original[1:3:3] // len=2, cap=2 (cap = max - low = 3 - 1 = 2)
sub = append(sub, 999) // cap agotado → REALOCA → nuevo array
fmt.Println(original) // [0, 10, 20, 30, 40] — intacto!
```

```
┌──────────────────────────────────────────────────────────────┐
│  FULL SLICE EXPRESSION: s[low:high:max]                      │
│                                                              │
│  original := []int{0, 10, 20, 30, 40}                       │
│                                                              │
│  sub := original[1:3]     → len=2, cap=4                     │
│  ┌────┬────┬────┬────┬────┐                                  │
│  │  0 │ 10 │ 20 │ 30 │ 40 │                                  │
│  └────┴─↑──┴────┴────┴────┘                                  │
│         └─ sub ──→ cap hasta el final                        │
│                                                              │
│  sub := original[1:3:3]   → len=2, cap=2                     │
│  ┌────┬────┬────┬────┬────┐                                  │
│  │  0 │ 10 │ 20 │ 30 │ 40 │                                  │
│  └────┴─↑──┴────┴────┴────┘                                  │
│         └─ sub ─┘ cap limitado aqui                          │
│                                                              │
│  append(sub, x) fuerza realocacion → protege original        │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 7. nil slice, empty slice, y zero value

### Las tres variantes

```go
// 1. nil slice (zero value)
var s1 []int          // nil
fmt.Println(s1 == nil) // true
fmt.Println(len(s1))   // 0
fmt.Println(cap(s1))   // 0

// 2. Empty slice (literal)
s2 := []int{}          // not nil
fmt.Println(s2 == nil) // false
fmt.Println(len(s2))   // 0
fmt.Println(cap(s2))   // 0

// 3. Empty slice (make)
s3 := make([]int, 0)   // not nil
fmt.Println(s3 == nil)  // false
```

### Todos son funcionales

```go
var s []int // nil

// Todas estas operaciones funcionan en nil slice:
len(s)           // 0
cap(s)           // 0
s = append(s, 1) // [1] — funciona!
for range s {}   // No itera — funciona!

// Por eso el idiom de Go es:
var results []string // nil — no necesitas make/literal
for _, item := range items {
    if condition(item) {
        results = append(results, item.Name)
    }
}
// results es nil si nada coincidio, o un slice con elementos
```

### Cuando importa nil vs empty

```go
// JSON marshaling:
var nilSlice []int
emptySlice := []int{}

j1, _ := json.Marshal(nilSlice)   // "null"
j2, _ := json.Marshal(emptySlice) // "[]"

// APIs que verifican nil:
func hasResults(items []string) bool {
    return items != nil // nil = no se busco; empty = se busco, 0 resultados
}

// Recomendacion general:
// - Usa nil (var s []T) como default
// - Usa empty ([]T{}) solo cuando necesitas [] en JSON
// - Usa make solo cuando conoces la capacidad necesaria
```

---

## 8. Slices no son comparables

### Solo puedes comparar con nil

```go
s1 := []int{1, 2, 3}
s2 := []int{1, 2, 3}

// ✗ ERROR de compilacion:
// fmt.Println(s1 == s2) // invalid: slice can only be compared to nil

// ✓ Comparar con nil:
fmt.Println(s1 == nil) // false

// ✓ Comparar contenido manualmente:
func equal(a, b []int) bool {
    if len(a) != len(b) {
        return false
    }
    for i := range a {
        if a[i] != b[i] {
            return false
        }
    }
    return true
}

// ✓ slices.Equal (Go 1.21+):
fmt.Println(slices.Equal(s1, s2)) // true

// ✓ reflect.DeepEqual (mas lento, cualquier tipo):
fmt.Println(reflect.DeepEqual(s1, s2)) // true

// ¿Por que no son comparables?
// Porque un slice es un descriptor, no un valor.
// Dos slices pueden apuntar al mismo array — ¿son "iguales"?
// Go evita la ambiguedad prohibiendo ==.
// (Arrays SI son comparables porque son valores completos)
```

### No pueden ser keys de map

```go
// ✗ ERROR:
// m := map[[]int]string{} // invalid map key type []int

// ✓ Alternativa: convertir a tipo comparable
// Usar arrays si el tamano es fijo:
m := map[[4]byte]string{} // IPv4 como key

// Usar string como key derivada:
type HostSet map[string]bool

func key(hosts []string) string {
    sorted := slices.Clone(hosts)
    sort.Strings(sorted)
    return strings.Join(sorted, ",")
}
```

---

## 9. Patrones esenciales con slices

### Patron 1: Filtrar (crear nuevo slice)

```go
func filter(items []string, pred func(string) bool) []string {
    var result []string // nil — crece segun necesidad
    for _, item := range items {
        if pred(item) {
            result = append(result, item)
        }
    }
    return result
}

servers := []string{"web01", "web02", "db01", "db02", "cache01"}
webServers := filter(servers, func(s string) bool {
    return strings.HasPrefix(s, "web")
})
// ["web01", "web02"]
```

### Patron 2: Filtrar in-place (sin alocacion)

```go
// Reutiliza el mismo backing array
func filterInPlace(items []string, pred func(string) bool) []string {
    n := 0
    for _, item := range items {
        if pred(item) {
            items[n] = item
            n++
        }
    }
    // Clear los elementos sobrantes para que el GC pueda recolectarlos
    for i := n; i < len(items); i++ {
        items[i] = "" // Zero value para strings
    }
    return items[:n]
}

// ¡Cuidado! Modifica el slice original
servers := []string{"web01", "dead01", "web02", "dead02"}
alive := filterInPlace(servers, func(s string) bool {
    return !strings.HasPrefix(s, "dead")
})
// alive = ["web01", "web02"]
// servers ya esta modificado
```

### Patron 3: Eliminar un elemento

```go
// Eliminar preservando orden: O(n)
func removeOrdered(s []int, i int) []int {
    return append(s[:i], s[i+1:]...)
}

// Eliminar sin preservar orden: O(1)
func removeUnordered(s []int, i int) []int {
    s[i] = s[len(s)-1] // Reemplazar con el ultimo
    return s[:len(s)-1] // Encoger
}

nums := []int{10, 20, 30, 40, 50}
nums = removeOrdered(nums, 2) // Eliminar 30
// [10, 20, 40, 50]

nums2 := []int{10, 20, 30, 40, 50}
nums2 = removeUnordered(nums2, 2) // Eliminar 30
// [10, 20, 50, 40] — 50 tomo el lugar de 30
```

### Patron 4: Insertar un elemento

```go
func insert(s []int, i int, val int) []int {
    s = append(s, 0) // Hacer espacio
    copy(s[i+1:], s[i:]) // Shift derecha
    s[i] = val
    return s
}

nums := []int{10, 20, 40, 50}
nums = insert(nums, 2, 30) // Insertar 30 en posicion 2
// [10, 20, 30, 40, 50]
```

### Patron 5: Stack con slices

```go
// Push: append
// Pop: slice
type Stack[T any] struct {
    items []T
}

func (s *Stack[T]) Push(item T) {
    s.items = append(s.items, item)
}

func (s *Stack[T]) Pop() (T, bool) {
    if len(s.items) == 0 {
        var zero T
        return zero, false
    }
    item := s.items[len(s.items)-1]
    s.items = s.items[:len(s.items)-1]
    return item, true
}

func (s *Stack[T]) Peek() (T, bool) {
    if len(s.items) == 0 {
        var zero T
        return zero, false
    }
    return s.items[len(s.items)-1], true
}

func (s *Stack[T]) Len() int {
    return len(s.items)
}
```

### Patron 6: Deduplicar

```go
func deduplicate(items []string) []string {
    seen := make(map[string]bool, len(items))
    var result []string
    for _, item := range items {
        if !seen[item] {
            seen[item] = true
            result = append(result, item)
        }
    }
    return result
}

hosts := []string{"web01", "db01", "web01", "cache01", "db01"}
unique := deduplicate(hosts)
// ["web01", "db01", "cache01"]
```

### Patron 7: Chunk / Batch

```go
func chunk[T any](items []T, size int) [][]T {
    var chunks [][]T
    for size < len(items) {
        items, chunks = items[size:], append(chunks, items[:size:size])
    }
    if len(items) > 0 {
        chunks = append(chunks, items)
    }
    return chunks
}

hosts := []string{"h1", "h2", "h3", "h4", "h5", "h6", "h7"}
batches := chunk(hosts, 3)
// [["h1","h2","h3"], ["h4","h5","h6"], ["h7"]]

// Util para procesamiento en lotes:
for i, batch := range batches {
    fmt.Printf("Deploying batch %d: %v\n", i+1, batch)
    deployBatch(batch)
}
```

---

## 10. Gotchas criticas

### Gotcha 1: append puede o no realocar

```go
a := make([]int, 3, 5) // cap=5, hay espacio
a[0], a[1], a[2] = 1, 2, 3

b := a[:2]   // [1, 2], cap=5 (comparte backing array)
b = append(b, 99) // No realoca — escribe en a[2]!

fmt.Println(a) // [1, 2, 99] — ¡b sobreescribio a[2]!

// ✓ Solucion: full slice expression
a = make([]int, 3, 5)
a[0], a[1], a[2] = 1, 2, 3
b = a[:2:2]   // cap=2 — fuerza realocacion en append
b = append(b, 99)
fmt.Println(a) // [1, 2, 3] — intacto
```

### Gotcha 2: Memory leak con sub-slices

```go
// El backing array entero se mantiene en memoria
// mientras exista CUALQUIER slice que lo referencie

func getFirstLine(data []byte) []byte {
    idx := bytes.IndexByte(data, '\n')
    if idx < 0 {
        return data
    }
    return data[:idx] // ✗ Mantiene TODO data en memoria
}

// Si data es 100MB, y la primera linea es 50 bytes,
// los 100MB no pueden ser recolectados por el GC

// ✓ Solucion: copiar
func getFirstLine(data []byte) []byte {
    idx := bytes.IndexByte(data, '\n')
    if idx < 0 {
        return append([]byte(nil), data...)
    }
    return append([]byte(nil), data[:idx]...) // Copia independiente
}

// ✓ Alternativa: bytes.Clone (Go 1.20+)
func getFirstLine(data []byte) []byte {
    idx := bytes.IndexByte(data, '\n')
    if idx < 0 {
        return bytes.Clone(data)
    }
    return bytes.Clone(data[:idx])
}
```

### Gotcha 3: Slice de structs con punteros — GC leak

```go
type Entry struct {
    Data  []byte
    Name  string
}

entries := make([]Entry, 1000)
// ... llenar entries ...

// Cuando eliminas elementos, los punteros internos
// (strings, slices) no se liberan si no los limpias:
entries = entries[:100] // Los 900 entries "eliminados"
// siguen en el backing array — sus Data y Name no son recolectados

// ✓ Limpiar antes de encoger:
for i := 100; i < len(entries); i++ {
    entries[i] = Entry{} // Zero value libera punteros internos
}
entries = entries[:100]
```

### Gotcha 4: Modificar slice en range

```go
// Agregar durante range: comportamiento indefinido
s := []int{1, 2, 3}
for i, v := range s {
    if v == 2 {
        s = append(s, 4) // ¡Peligro!
        // range evaluo len(s) al inicio
        // Si append realoca, el range sigue sobre el array viejo
    }
    fmt.Println(i, v)
}

// ✓ Iterar con indice si necesitas modificar:
for i := 0; i < len(s); i++ {
    // len(s) se re-evalua en cada iteracion
}

// ✓ O iterar sobre una copia:
snapshot := slices.Clone(s)
for _, v := range snapshot {
    // Modificar s es seguro, snapshot no cambia
}
```

### Gotcha 5: Nil slice en JSON

```go
type Response struct {
    Items []string `json:"items"`
}

// nil slice → "items": null
r1 := Response{}
j1, _ := json.Marshal(r1) // {"items":null}

// empty slice → "items": []
r2 := Response{Items: []string{}}
j2, _ := json.Marshal(r2) // {"items":[]}

// Muchos clientes JSON no manejan null vs [] igual
// Si tu API debe retornar [], inicializa con []string{}
```

---

## 11. slices package (Go 1.21+)

### Funciones principales

```go
import "slices"

// Comparar
slices.Equal([]int{1,2,3}, []int{1,2,3})    // true
slices.Compare([]int{1,2}, []int{1,3})       // -1 (a < b)

// Buscar
slices.Contains([]string{"a","b","c"}, "b")  // true
slices.Index([]string{"a","b","c"}, "b")     // 1

// Ordenar
s := []int{3, 1, 4, 1, 5}
slices.Sort(s) // [1, 1, 3, 4, 5] — in-place

// Ordenar con funcion custom
type Host struct {
    Name string
    Load float64
}
hosts := []Host{{"web01", 0.8}, {"web02", 0.3}, {"db01", 0.6}}
slices.SortFunc(hosts, func(a, b Host) int {
    return cmp.Compare(a.Load, b.Load) // Por carga
})

// Clone (copia independiente)
original := []int{1, 2, 3}
clone := slices.Clone(original)

// Compact (eliminar duplicados consecutivos, requiere sort previo)
s = []int{1, 1, 2, 3, 3, 3, 4}
s = slices.Compact(s) // [1, 2, 3, 4]

// Delete (eliminar rango)
s = []int{0, 1, 2, 3, 4, 5}
s = slices.Delete(s, 2, 4) // Eliminar [2:4] → [0, 1, 4, 5]

// Insert
s = []int{1, 2, 5, 6}
s = slices.Insert(s, 2, 3, 4) // [1, 2, 3, 4, 5, 6]

// Reverse
slices.Reverse(s) // [6, 5, 4, 3, 2, 1]

// Min/Max
slices.Min([]int{3, 1, 4, 1, 5}) // 1
slices.Max([]int{3, 1, 4, 1, 5}) // 5

// Grow (pre-alocar capacidad extra sin cambiar len)
s = slices.Grow(s, 100) // Asegura cap >= len+100

// Clip (reduce cap a len)
s = slices.Clip(s) // cap = len
```

### BinarySearch

```go
// Requiere slice ordenado
sorted := []string{"alpha", "bravo", "charlie", "delta", "echo"}

idx, found := slices.BinarySearch(sorted, "charlie")
fmt.Println(idx, found) // 2 true

idx, found = slices.BinarySearch(sorted, "foxtrot")
fmt.Println(idx, found) // 5 false (posicion de insercion)

// BinarySearchFunc para tipos custom:
type Server struct {
    Name string
    Port int
}
servers := []Server{{"api", 8080}, {"db", 5432}, {"web", 80}}
slices.SortFunc(servers, func(a, b Server) int {
    return cmp.Compare(a.Name, b.Name)
})
idx, found = slices.BinarySearchFunc(servers, Server{Name: "db"}, func(a, b Server) int {
    return cmp.Compare(a.Name, b.Name)
})
```

---

## 12. Patrones SysAdmin con slices

### Patron: Pipeline de procesamiento de logs

```go
type LogEntry struct {
    Timestamp time.Time
    Level     string
    Service   string
    Message   string
}

func parseLogs(lines []string) []LogEntry {
    entries := make([]LogEntry, 0, len(lines))
    for _, line := range lines {
        entry, err := parseLogLine(line)
        if err != nil {
            continue
        }
        entries = append(entries, entry)
    }
    return entries
}

func filterByLevel(entries []LogEntry, levels ...string) []LogEntry {
    levelSet := make(map[string]bool, len(levels))
    for _, l := range levels {
        levelSet[l] = true
    }
    var result []LogEntry
    for _, e := range entries {
        if levelSet[e.Level] {
            result = append(result, e)
        }
    }
    return result
}

func filterByTimeRange(entries []LogEntry, from, to time.Time) []LogEntry {
    var result []LogEntry
    for _, e := range entries {
        if !e.Timestamp.Before(from) && !e.Timestamp.After(to) {
            result = append(result, e)
        }
    }
    return result
}

func groupByService(entries []LogEntry) map[string][]LogEntry {
    groups := make(map[string][]LogEntry)
    for _, e := range entries {
        groups[e.Service] = append(groups[e.Service], e)
    }
    return groups
}

// Pipeline:
// lines := readLines("/var/log/app.log")
// entries := parseLogs(lines)
// errors := filterByLevel(entries, "ERROR", "FATAL")
// recent := filterByTimeRange(errors, time.Now().Add(-1*time.Hour), time.Now())
// byService := groupByService(recent)
```

### Patron: Parallel batch processor

```go
func processBatches[T any](items []T, batchSize int, process func([]T) error) error {
    var errs []error
    
    for i := 0; i < len(items); i += batchSize {
        end := i + batchSize
        if end > len(items) {
            end = len(items)
        }
        
        // Usar full slice expression para proteger el batch
        batch := items[i:end:end]
        
        if err := process(batch); err != nil {
            errs = append(errs, fmt.Errorf("batch %d-%d: %w", i, end, err))
        }
    }
    
    if len(errs) > 0 {
        return errors.Join(errs...)
    }
    return nil
}

// Uso:
hosts := loadHosts() // []string con 1000 hosts
err := processBatches(hosts, 50, func(batch []string) error {
    return deployToBatch(batch) // Deploya a 50 hosts a la vez
})
```

### Patron: Sliding window para metricas

```go
type SlidingWindow struct {
    data   []float64
    size   int
    pos    int
    filled bool
}

func NewSlidingWindow(size int) *SlidingWindow {
    return &SlidingWindow{
        data: make([]float64, size),
        size: size,
    }
}

func (w *SlidingWindow) Add(value float64) {
    w.data[w.pos] = value
    w.pos = (w.pos + 1) % w.size
    if w.pos == 0 {
        w.filled = true
    }
}

func (w *SlidingWindow) Average() float64 {
    count := w.size
    if !w.filled {
        count = w.pos
    }
    if count == 0 {
        return 0
    }
    
    sum := 0.0
    for i := 0; i < count; i++ {
        sum += w.data[i]
    }
    return sum / float64(count)
}

func (w *SlidingWindow) Max() float64 {
    count := w.size
    if !w.filled {
        count = w.pos
    }
    if count == 0 {
        return 0
    }
    m := w.data[0]
    for i := 1; i < count; i++ {
        if w.data[i] > m {
            m = w.data[i]
        }
    }
    return m
}

// Uso: monitorear latencia
window := NewSlidingWindow(60) // Ultimos 60 valores
for latency := range latencyChannel {
    window.Add(latency)
    if window.Average() > 500 { // ms
        alert("High average latency: %.0fms", window.Average())
    }
}
```

---

## 13. Comparacion: Go vs C vs Rust

### Go: slices

```go
// Slice = descriptor de 3 campos (ptr, len, cap)
s := make([]int, 0, 10)
s = append(s, 1, 2, 3)

// Crece automaticamente
// GC limpia arrays abandonados
// Bounds checking en runtime (panic)
// No comparable (solo nil)
```

### C: arrays dinamicos manuales

```c
// C no tiene slices. Simulacion manual:
typedef struct {
    int *data;
    int len;
    int cap;
} IntSlice;

IntSlice make_slice(int cap) {
    return (IntSlice){
        .data = malloc(cap * sizeof(int)),
        .len = 0,
        .cap = cap,
    };
}

void append(IntSlice *s, int value) {
    if (s->len == s->cap) {
        s->cap *= 2;
        s->data = realloc(s->data, s->cap * sizeof(int));
        // realloc puede fallar → NULL → crash
    }
    s->data[s->len++] = value;
}

// Manual: malloc, realloc, free
// No bounds checking (buffer overflow → UB/CVE)
// No GC — free manual
// Puede olvidar free → memory leak
```

### Rust: Vec<T> y slices (&[T])

```rust
// Vec<T> = similar a Go slice (heap, crece)
let mut v: Vec<i32> = Vec::new();
v.push(1);
v.push(2);
v.push(3);

// &[T] = slice reference (como Go slice pero sin ownership)
let slice: &[i32] = &v[1..3]; // [2, 3]

// Diferencias clave:
// - Vec owns the data; &[T] borrows it
// - Borrow checker previene data races en compile time
// - Bounds checking en runtime (panic, no UB)
// - No GC — RAII/Drop limpia automaticamente

// Rust previene el gotcha de Go:
let mut a = vec![1, 2, 3];
let b = &a[0..2]; // borrow inmutable
// a.push(4); // ERROR: cannot borrow `a` as mutable while borrowed as immutable
// El compilador previene que push invalide b
println!("{:?}", b); // b es valido
// Despues de que b deja de usarse, a.push(4) funciona
```

### Tabla comparativa

```
┌──────────────────────┬──────────────────────┬──────────────────────┬──────────────────────┐
│ Aspecto              │ Go slice             │ C (manual)           │ Rust Vec/&[T]        │
├──────────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤
│ Estructura           │ {ptr, len, cap}      │ Manual struct        │ Vec{ptr, len, cap}   │
│ Crecimiento          │ append (auto)        │ realloc (manual)     │ push (auto)          │
│ Memoria              │ GC                   │ free manual          │ RAII/Drop            │
│ Bounds check         │ Si (panic)           │ No (UB)              │ Si (panic)           │
│ Sharing              │ Sub-slices comparten │ Punteros manuales    │ Borrow checker       │
│ Data race prevenc.   │ Runtime (mutex)      │ Nada (UB)            │ Compile time         │
│ Comparable           │ No (solo nil)        │ memcmp manual        │ Si (PartialEq)       │
│ Nil/null             │ nil funcional        │ NULL → crash         │ Option<Vec>          │
│ Memory leak riesgo   │ Sub-slice retains    │ Olvidar free         │ Raro (RAII)          │
│ Sub-slice append     │ Puede corromper      │ N/A (manual)         │ Borrow previene      │
│ Pre-alocacion        │ make([]T, 0, cap)    │ malloc(cap*sizeof)   │ Vec::with_capacity   │
│ Zero cost slicing    │ Si                   │ Si (puntero arith.)  │ Si                   │
└──────────────────────┴──────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 14. Performance: alocacion y crecimiento

### Benchmark: con vs sin pre-alocacion

```go
func BenchmarkAppendNoPrealloc(b *testing.B) {
    for i := 0; i < b.N; i++ {
        var s []int
        for j := 0; j < 10000; j++ {
            s = append(s, j)
        }
    }
}

func BenchmarkAppendPrealloc(b *testing.B) {
    for i := 0; i < b.N; i++ {
        s := make([]int, 0, 10000)
        for j := 0; j < 10000; j++ {
            s = append(s, j)
        }
    }
}

// Resultados tipicos:
// BenchmarkAppendNoPrealloc-8    ~150µs/op   ~400KB alloc   ~20 allocs
// BenchmarkAppendPrealloc-8      ~40µs/op    ~80KB alloc     1 alloc
// ~3.7x mas rapido con pre-alocacion
```

### Cuando pre-alocar

```go
// ✓ Pre-alocar cuando conoces el tamano (o una buena estimacion):
lines := strings.Split(data, "\n")
results := make([]Result, 0, len(lines))

// ✓ Pre-alocar en map→slice:
hosts := make([]string, 0, len(hostMap))
for k := range hostMap {
    hosts = append(hosts, k)
}

// ✗ No pre-alocar cuando el tamano es impredecible:
var matches []string // nil — crece segun necesidad
for scanner.Scan() {
    if condition(scanner.Text()) {
        matches = append(matches, scanner.Text())
    }
}
```

### Reducir capacidad con Clip

```go
// Despues de filtrar, el slice puede tener capacidad desperdiciada
s := make([]int, 1000)
// ... llenar ...
filtered := filter(s) // Supongamos que retorna 50 elementos

// filtered tiene len=50 pero cap puede ser 1000
// Los 950 elementos "eliminados" siguen en memoria

// Go 1.21+:
filtered = slices.Clip(filtered) // cap = len = 50

// Pre-1.21:
filtered = append(filtered[:0:0], filtered...)
```

---

## 15. Programa completo: Server Fleet Manager

Un administrador de flota de servidores que usa slices extensivamente para gestionar inventario, procesar en batches, filtrar, y agregar metricas.

```go
package main

import (
	"cmp"
	"encoding/json"
	"fmt"
	"math/rand"
	"os"
	"slices"
	"strings"
	"time"
)

// ─── Tipos ──────────────────────────────────────────────────────

type ServerStatus string

const (
	StatusOnline  ServerStatus = "online"
	StatusOffline ServerStatus = "offline"
	StatusDrain   ServerStatus = "draining"
)

type Server struct {
	Name     string       `json:"name"`
	IP       string       `json:"ip"`
	Role     string       `json:"role"`
	Status   ServerStatus `json:"status"`
	CPU      float64      `json:"cpu_pct"`
	Memory   float64      `json:"memory_pct"`
	DiskUsed float64      `json:"disk_pct"`
	Uptime   time.Duration `json:"uptime_sec"`
	Tags     []string     `json:"tags"`
}

// ─── Fleet ──────────────────────────────────────────────────────

type Fleet struct {
	servers []Server
}

func NewFleet(servers ...Server) *Fleet {
	// Copia defensiva: no retener referencia al slice del caller
	s := make([]Server, len(servers))
	copy(s, servers)
	return &Fleet{servers: s}
}

func (f *Fleet) Add(servers ...Server) {
	f.servers = append(f.servers, servers...)
}

func (f *Fleet) Len() int {
	return len(f.servers)
}

// All retorna copia — proteger internals
func (f *Fleet) All() []Server {
	return slices.Clone(f.servers)
}

// ─── Filtrado (retorna nuevos slices) ───────────────────────────

func (f *Fleet) ByRole(role string) []Server {
	var result []Server
	for _, s := range f.servers {
		if s.Role == role {
			result = append(result, s)
		}
	}
	return result
}

func (f *Fleet) ByStatus(status ServerStatus) []Server {
	var result []Server
	for _, s := range f.servers {
		if s.Status == status {
			result = append(result, s)
		}
	}
	return result
}

func (f *Fleet) ByTag(tag string) []Server {
	var result []Server
	for _, s := range f.servers {
		if slices.Contains(s.Tags, tag) {
			result = append(result, s)
		}
	}
	return result
}

func (f *Fleet) Where(pred func(Server) bool) []Server {
	var result []Server
	for _, s := range f.servers {
		if pred(s) {
			result = append(result, s)
		}
	}
	return result
}

// ─── Ordenamiento ───────────────────────────────────────────────

func SortByCPU(servers []Server) {
	slices.SortFunc(servers, func(a, b Server) int {
		return cmp.Compare(b.CPU, a.CPU) // Descendente
	})
}

func SortByMemory(servers []Server) {
	slices.SortFunc(servers, func(a, b Server) int {
		return cmp.Compare(b.Memory, a.Memory)
	})
}

func SortByName(servers []Server) {
	slices.SortFunc(servers, func(a, b Server) int {
		return cmp.Compare(a.Name, b.Name)
	})
}

// ─── Agregacion ─────────────────────────────────────────────────

type FleetStats struct {
	Total     int                   `json:"total"`
	ByRole    map[string]int        `json:"by_role"`
	ByStatus  map[string]int        `json:"by_status"`
	AvgCPU    float64               `json:"avg_cpu"`
	AvgMemory float64               `json:"avg_memory"`
	TopCPU    []string              `json:"top_cpu_servers"`
	TopMem    []string              `json:"top_memory_servers"`
	Alerts    []string              `json:"alerts"`
}

func (f *Fleet) Stats() FleetStats {
	stats := FleetStats{
		Total:    len(f.servers),
		ByRole:   make(map[string]int),
		ByStatus: make(map[string]int),
	}

	if len(f.servers) == 0 {
		return stats
	}

	var totalCPU, totalMem float64
	for _, s := range f.servers {
		stats.ByRole[s.Role]++
		stats.ByStatus[string(s.Status)]++
		totalCPU += s.CPU
		totalMem += s.Memory
	}

	stats.AvgCPU = totalCPU / float64(len(f.servers))
	stats.AvgMemory = totalMem / float64(len(f.servers))

	// Top 3 CPU — trabajar con copia para no modificar original
	cpuSorted := slices.Clone(f.servers)
	SortByCPU(cpuSorted)
	topN := min(3, len(cpuSorted))
	stats.TopCPU = make([]string, topN)
	for i := 0; i < topN; i++ {
		stats.TopCPU[i] = fmt.Sprintf("%s (%.1f%%)", cpuSorted[i].Name, cpuSorted[i].CPU)
	}

	// Top 3 Memory
	memSorted := slices.Clone(f.servers)
	SortByMemory(memSorted)
	stats.TopMem = make([]string, topN)
	for i := 0; i < topN; i++ {
		stats.TopMem[i] = fmt.Sprintf("%s (%.1f%%)", memSorted[i].Name, memSorted[i].Memory)
	}

	// Alertas
	for _, s := range f.servers {
		if s.CPU > 90 {
			stats.Alerts = append(stats.Alerts, fmt.Sprintf("HIGH CPU: %s at %.1f%%", s.Name, s.CPU))
		}
		if s.Memory > 90 {
			stats.Alerts = append(stats.Alerts, fmt.Sprintf("HIGH MEM: %s at %.1f%%", s.Name, s.Memory))
		}
		if s.DiskUsed > 85 {
			stats.Alerts = append(stats.Alerts, fmt.Sprintf("DISK WARN: %s at %.1f%%", s.Name, s.DiskUsed))
		}
		if s.Status == StatusOffline {
			stats.Alerts = append(stats.Alerts, fmt.Sprintf("OFFLINE: %s", s.Name))
		}
	}

	return stats
}

// ─── Batch operations ───────────────────────────────────────────

func chunk[T any](items []T, size int) [][]T {
	var chunks [][]T
	for size < len(items) {
		items, chunks = items[size:], append(chunks, items[:size:size])
	}
	if len(items) > 0 {
		chunks = append(chunks, items)
	}
	return chunks
}

func (f *Fleet) DeployInBatches(batchSize int, deploy func(batch []Server) error) error {
	online := f.ByStatus(StatusOnline)
	batches := chunk(online, batchSize)

	for i, batch := range batches {
		names := make([]string, len(batch))
		for j, s := range batch {
			names[j] = s.Name
		}
		fmt.Printf("  Batch %d/%d: %s\n", i+1, len(batches), strings.Join(names, ", "))

		if err := deploy(batch); err != nil {
			return fmt.Errorf("batch %d failed: %w", i+1, err)
		}
	}
	return nil
}

// ─── Dedup & Diff ───────────────────────────────────────────────

func UniqueRoles(servers []Server) []string {
	seen := make(map[string]bool)
	var roles []string
	for _, s := range servers {
		if !seen[s.Role] {
			seen[s.Role] = true
			roles = append(roles, s.Role)
		}
	}
	slices.Sort(roles)
	return roles
}

func ServerNames(servers []Server) []string {
	names := make([]string, len(servers))
	for i, s := range servers {
		names[i] = s.Name
	}
	return names
}

func Diff(before, after []string) (added, removed []string) {
	beforeSet := make(map[string]bool, len(before))
	afterSet := make(map[string]bool, len(after))
	for _, s := range before {
		beforeSet[s] = true
	}
	for _, s := range after {
		afterSet[s] = true
	}
	for _, s := range after {
		if !beforeSet[s] {
			added = append(added, s)
		}
	}
	for _, s := range before {
		if !afterSet[s] {
			removed = append(removed, s)
		}
	}
	return
}

// ─── Display ────────────────────────────────────────────────────

func printTable(servers []Server) {
	fmt.Printf("%-12s %-16s %-10s %-8s %6s %6s %6s %s\n",
		"NAME", "IP", "ROLE", "STATUS", "CPU%", "MEM%", "DISK%", "TAGS")
	fmt.Println(strings.Repeat("─", 90))
	for _, s := range servers {
		tags := strings.Join(s.Tags, ",")
		fmt.Printf("%-12s %-16s %-10s %-8s %5.1f%% %5.1f%% %5.1f%% %s\n",
			s.Name, s.IP, s.Role, s.Status, s.CPU, s.Memory, s.DiskUsed, tags)
	}
}

// ─── Main ───────────────────────────────────────────────────────

func main() {
	// Generar flota de ejemplo
	fleet := NewFleet(
		Server{"web01", "10.0.1.1", "web", StatusOnline, 45.2, 62.0, 55.0, 72 * time.Hour, []string{"production", "us-east"}},
		Server{"web02", "10.0.1.2", "web", StatusOnline, 38.7, 58.3, 52.0, 72 * time.Hour, []string{"production", "us-east"}},
		Server{"web03", "10.0.1.3", "web", StatusDrain, 12.1, 35.0, 48.0, 72 * time.Hour, []string{"production", "us-west"}},
		Server{"api01", "10.0.2.1", "api", StatusOnline, 72.5, 81.2, 60.0, 48 * time.Hour, []string{"production", "us-east"}},
		Server{"api02", "10.0.2.2", "api", StatusOnline, 68.3, 77.8, 58.0, 48 * time.Hour, []string{"production", "us-west"}},
		Server{"db01", "10.0.3.1", "database", StatusOnline, 55.0, 88.5, 78.0, 720 * time.Hour, []string{"production", "primary"}},
		Server{"db02", "10.0.3.2", "database", StatusOnline, 22.1, 45.2, 75.0, 720 * time.Hour, []string{"production", "replica"}},
		Server{"cache01", "10.0.4.1", "cache", StatusOnline, 15.3, 92.1, 30.0, 168 * time.Hour, []string{"production", "us-east"}},
		Server{"cache02", "10.0.4.2", "cache", StatusOffline, 0, 0, 30.0, 0, []string{"production", "us-west"}},
		Server{"mon01", "10.0.5.1", "monitoring", StatusOnline, 35.0, 55.0, 82.0, 2160 * time.Hour, []string{"infra"}},
	)

	// === Listar todos ===
	fmt.Println("=== Fleet Overview ===\n")
	all := fleet.All()
	SortByName(all)
	printTable(all)

	// === Filtrar por rol ===
	fmt.Println("\n=== Web Servers ===\n")
	webServers := fleet.ByRole("web")
	SortByCPU(webServers)
	printTable(webServers)

	// === Filtrar por estado ===
	fmt.Println("\n=== Offline/Draining ===\n")
	problems := fleet.Where(func(s Server) bool {
		return s.Status != StatusOnline
	})
	printTable(problems)

	// === Filtrar por tag ===
	fmt.Println("\n=== US-East Region ===\n")
	usEast := fleet.ByTag("us-east")
	printTable(usEast)

	// === Custom filter: high resource usage ===
	fmt.Println("\n=== High Resource Usage (CPU>50% or MEM>80%) ===\n")
	highUsage := fleet.Where(func(s Server) bool {
		return s.CPU > 50 || s.Memory > 80
	})
	SortByCPU(highUsage)
	printTable(highUsage)

	// === Stats ===
	fmt.Println("\n=== Fleet Statistics ===\n")
	stats := fleet.Stats()
	statsJSON, _ := json.MarshalIndent(stats, "", "  ")
	fmt.Println(string(statsJSON))

	// === Unique roles ===
	fmt.Println("\n=== Roles ===")
	roles := UniqueRoles(fleet.All())
	fmt.Println(strings.Join(roles, ", "))

	// === Batch deploy simulation ===
	fmt.Println("\n=== Rolling Deploy (batch size 3) ===\n")
	err := fleet.DeployInBatches(3, func(batch []Server) error {
		// Simular deploy
		time.Sleep(10 * time.Millisecond)
		fmt.Printf("    → Deployed to %d servers\n", len(batch))
		return nil
	})
	if err != nil {
		fmt.Fprintf(os.Stderr, "Deploy failed: %v\n", err)
	}

	// === Diff ===
	fmt.Println("\n=== Fleet Changes ===")
	oldFleet := []string{"web01", "web02", "web03", "db01", "old-app01"}
	newFleet := ServerNames(fleet.All())
	added, removed := Diff(oldFleet, newFleet)
	if len(added) > 0 {
		fmt.Printf("  Added:   %s\n", strings.Join(added, ", "))
	}
	if len(removed) > 0 {
		fmt.Printf("  Removed: %s\n", strings.Join(removed, ", "))
	}
}
```

---

## 16. Tabla de errores comunes

```
┌────┬──────────────────────────────────────┬────────────────────────────────────────┬─────────┐
│ #  │ Error                                │ Solucion                               │ Nivel   │
├────┼──────────────────────────────────────┼────────────────────────────────────────┼─────────┤
│  1 │ No reasignar resultado de append     │ s = append(s, x) — siempre reasignar  │ Logic   │
│  2 │ Sub-slice append corrompe original   │ Full slice expression s[a:b:b]         │ Logic   │
│  3 │ Memory leak por sub-slice retenido   │ Copiar con clone/append/copy           │ Memory  │
│  4 │ nil slice en JSON produce null       │ Usar []T{} si necesitas []             │ Logic   │
│  5 │ Comparar slices con ==               │ slices.Equal o loop manual             │ Compile │
│  6 │ Slice como key de map                │ Convertir a array o string             │ Compile │
│  7 │ Pasar slice esperando que len cambie │ Retornar nuevo slice o pasar *[]T      │ Logic   │
│  8 │ Range copia structs grandes          │ Usar indice: &s[i]                     │ Perf    │
│  9 │ Modificar slice durante range        │ Iterar con indice o sobre copia        │ Logic   │
│ 10 │ make([]T, n) vs make([]T, 0, n)     │ Primer forma crea n zero values        │ Logic   │
│ 11 │ No pre-alocar con tamano conocido    │ make([]T, 0, expectedLen)              │ Perf    │
│ 12 │ No limpiar punteros al encoger slice │ Zerear elementos removidos para GC     │ Memory  │
└────┴──────────────────────────────────────┴────────────────────────────────────────┴─────────┘
```

---

## 17. Ejercicios de prediccion

**Ejercicio 1**: ¿Que imprime?

```go
s := make([]int, 3, 5)
fmt.Println(len(s), cap(s))
```

<details>
<summary>Respuesta</summary>

```
3 5
```

`make([]int, 3, 5)` crea un slice con 3 elementos (zero value) y capacidad 5.
</details>

**Ejercicio 2**: ¿Que imprime?

```go
a := []int{1, 2, 3}
b := a
b[0] = 999
fmt.Println(a[0])
```

<details>
<summary>Respuesta</summary>

```
999
```

`b := a` copia el slice header, pero ambos apuntan al mismo backing array. Modificar `b[0]` modifica `a[0]`.
</details>

**Ejercicio 3**: ¿Que imprime?

```go
var s []int
fmt.Println(s == nil, len(s))
s = append(s, 1)
fmt.Println(s == nil, len(s))
```

<details>
<summary>Respuesta</summary>

```
true 0
false 1
```

`var s []int` es nil. `append` a nil funciona y retorna un slice no-nil.
</details>

**Ejercicio 4**: ¿Que imprime?

```go
s := []int{1, 2, 3, 4, 5}
t := s[1:3]
fmt.Println(len(t), cap(t))
```

<details>
<summary>Respuesta</summary>

```
2 4
```

`s[1:3]` tiene len = 3-1 = 2, cap = 5-1 = 4 (desde indice 1 hasta el final del backing array).
</details>

**Ejercicio 5**: ¿Que imprime?

```go
a := make([]int, 3, 5)
a[0], a[1], a[2] = 1, 2, 3
b := a[0:2]
b = append(b, 99)
fmt.Println(a)
```

<details>
<summary>Respuesta</summary>

```
[1 2 99]
```

`b` tiene cap=5 (hereda de a). `append(b, 99)` no realoca — escribe en `a[2]`, sobreescribiendo el 3.
</details>

**Ejercicio 6**: ¿Que imprime?

```go
a := []int{1, 2, 3}
b := a[0:2:2]
b = append(b, 99)
fmt.Println(a)
fmt.Println(b)
```

<details>
<summary>Respuesta</summary>

```
[1 2 3]
[1 2 99]
```

Full slice expression `a[0:2:2]` limita cap a 2. `append` realoca un nuevo array. `a` queda intacto.
</details>

**Ejercicio 7**: ¿Que imprime?

```go
func modify(s []int) {
    s = append(s, 4)
    s[0] = 999
}

a := []int{1, 2, 3}
modify(a)
fmt.Println(a)
```

<details>
<summary>Respuesta</summary>

```
[1 2 3]
```

`a` tiene len=3, cap=3. `append` en `modify` realoca (cap agotado), creando un nuevo backing array. `s[0] = 999` modifica el nuevo array, no el original. El header de `a` en el caller no cambia.
</details>

**Ejercicio 8**: ¿Que imprime?

```go
s := make([]int, 5)
s = s[2:4]
fmt.Println(len(s), cap(s))
```

<details>
<summary>Respuesta</summary>

```
2 3
```

`make([]int, 5)` tiene cap=5. `s[2:4]` tiene len = 4-2 = 2, cap = 5-2 = 3.
</details>

**Ejercicio 9**: ¿Que imprime?

```go
src := []int{1, 2, 3, 4, 5}
dst := make([]int, 3)
n := copy(dst, src)
fmt.Println(dst, n)
```

<details>
<summary>Respuesta</summary>

```
[1 2 3] 3
```

`copy` copia `min(len(dst), len(src))` = `min(3, 5)` = 3 elementos.
</details>

**Ejercicio 10**: ¿Que imprime?

```go
s := []int{10, 20, 30, 40, 50}
s = append(s[:2], s[3:]...)
fmt.Println(s)
```

<details>
<summary>Respuesta</summary>

```
[10 20 40 50]
```

Esto elimina el elemento en indice 2 (30). `s[:2]` = [10,20], `s[3:]` = [40,50]. `append` concatena: [10, 20, 40, 50].
</details>

---

## 18. Resumen visual

```
┌──────────────────────────────────────────────────────────────┐
│              SLICES — RESUMEN                                │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ESTRUCTURA INTERNA                                          │
│  ┌──────────┬──────────┬──────────┐                          │
│  │ pointer  │  length  │ capacity │  24 bytes (header)       │
│  └──────────┴──────────┴──────────┘                          │
│  Se pasa por valor (copia del header), ptr es compartido     │
│                                                              │
│  CREACION                                                    │
│  var s []int              → nil (funcional: len=0, append ok)│
│  s := []int{1, 2, 3}     → literal (not nil)                │
│  s := make([]int, len)    → zero values                      │
│  s := make([]int, 0, cap) → vacio con capacidad (pre-alloc)  │
│  s := arr[low:high]       → slice desde array                │
│  s := arr[low:high:max]   → full slice expression (cap ctrl) │
│                                                              │
│  OPERACIONES                                                 │
│  s = append(s, x, y)     → SIEMPRE reasignar                │
│  copy(dst, src)           → copia min(len) elementos         │
│  s[low:high]              → sub-slice (comparte backing)     │
│  len(s), cap(s)           → longitud y capacidad             │
│  slices.Clone(s)          → copia independiente (Go 1.21+)   │
│                                                              │
│  APPEND MECANICA                                             │
│  ├─ len < cap → escribe en espacio existente (NO realoca)    │
│  ├─ len == cap → aloca nuevo array, copia, apunta ahi        │
│  └─ Crecimiento: ~2x pequenos, ~1.25x grandes               │
│                                                              │
│  GOTCHAS CRITICOS                                            │
│  ├─ Sub-slice append: puede corromper original               │
│  │   Solucion: full slice expression s[a:b:b]                │
│  ├─ Memory leak: sub-slice retiene todo el backing array     │
│  │   Solucion: copiar con Clone/append/copy                  │
│  ├─ nil vs empty: nil→JSON null, empty→JSON []               │
│  ├─ Pasar a func: func ve elementos pero no cambios a len   │
│  └─ No comparable: solo con nil, usar slices.Equal           │
│                                                              │
│  PATRONES                                                    │
│  ├─ Filter: crear nuevo slice con predicado                  │
│  ├─ Filter in-place: reusar backing array                    │
│  ├─ Remove ordered: append(s[:i], s[i+1:]...)                │
│  ├─ Remove unordered: swap con ultimo, encoger               │
│  ├─ Insert: append + copy shift                              │
│  ├─ Stack: append = push, s[:n-1] = pop                      │
│  ├─ Chunk/Batch: dividir en sub-slices                       │
│  ├─ Dedup: map[T]bool + append                               │
│  └─ Sliding window: array fijo + indice circular             │
│                                                              │
│  PERFORMANCE                                                 │
│  ├─ Pre-alocar: make([]T, 0, expectedLen) — ~3.7x mas rapido│
│  ├─ Clip: reducir cap a len para liberar memoria             │
│  ├─ Range copia structs grandes: usar indice                 │
│  └─ slices package (Go 1.21+): Sort, BinarySearch, etc.     │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: T03 - Slicing y gotchas — s[low:high:max], comparticion de array subyacente, slice de slice, memory leaks
