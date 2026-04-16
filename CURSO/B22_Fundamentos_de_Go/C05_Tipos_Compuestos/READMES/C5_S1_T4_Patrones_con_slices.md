# Patrones con Slices — Algoritmos, sort, Operaciones de Conjunto y Pipelines

## 1. Introduccion

T02 introdujo los patrones basicos (filter, remove, insert, stack, dedup, chunk) y las funciones del package `slices`. T03 profundizo en los gotchas de comparticion y memory leaks. Este topico cierra la seccion de slices con los **patrones algoritmicos avanzados** que un SysAdmin/DevOps usa en produccion: ordenamiento multi-criterio, busqueda binaria para mantener colecciones ordenadas, operaciones de conjunto (union, interseccion, diferencia), tecnicas de dos punteros, pipelines funcionales encadenados, y la integracion completa de `sort.Slice` / `sort.Interface` con el package `slices` de Go 1.21+.

Estos patrones aparecen constantemente en herramientas CLI, procesamiento de inventario, analisis de logs, gestion de configuracion, y cualquier codigo que manipule listas de servidores, metricas, eventos, o reglas.

```
┌──────────────────────────────────────────────────────────────┐
│              PATRONES CON SLICES                             │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Ordenamiento:                                               │
│  ├─ sort.Slice / sort.SliceStable (pre-generics)             │
│  ├─ slices.SortFunc / slices.SortStableFunc (Go 1.21+)       │
│  ├─ Multi-key: ordenar por role, luego por name              │
│  ├─ sort.Interface para tipos custom                         │
│  └─ Estabilidad: estable preserva orden de iguales           │
│                                                              │
│  Busqueda:                                                   │
│  ├─ sort.Search (binaria clasica)                            │
│  ├─ slices.BinarySearch / BinarySearchFunc                   │
│  └─ Insercion ordenada (mantener slice sorted)               │
│                                                              │
│  Operaciones de conjunto (sorted slices):                    │
│  ├─ Union, Interseccion, Diferencia                          │
│  ├─ Contains (lineal vs binaria)                             │
│  └─ Dedup sobre sorted (slices.Compact)                      │
│                                                              │
│  Tecnicas algoritmicas:                                      │
│  ├─ Dos punteros: merge, partition, compress                 │
│  ├─ Rotate / Shift                                           │
│  ├─ Flatten ([][]T → []T)                                    │
│  ├─ Zip / Unzip                                              │
│  ├─ GroupBy / Partition por predicado                         │
│  └─ Reduce / Fold                                            │
│                                                              │
│  Pipelines funcionales:                                      │
│  ├─ Map → Filter → Sort → Take                               │
│  ├─ Encadenar transformaciones                               │
│  └─ Generics para reutilizacion                              │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Ordenamiento: sort.Slice vs slices.SortFunc

### sort.Slice (pre-Go 1.21, sigue siendo valido)

```go
import "sort"

type Server struct {
    Name   string
    CPU    float64
    Memory float64
    Role   string
}

servers := []Server{
    {"web03", 45.2, 62.0, "web"},
    {"web01", 78.1, 81.3, "web"},
    {"db01", 22.5, 90.2, "database"},
    {"web02", 45.2, 55.0, "web"},
}

// sort.Slice: ordenar con funcion less
// No es estable — elementos iguales pueden reordenarse
sort.Slice(servers, func(i, j int) bool {
    return servers[i].CPU < servers[j].CPU
})
// Resultado ordenado por CPU ascendente:
// db01(22.5), web03(45.2), web02(45.2), web01(78.1)
// Nota: web03 y web02 tienen misma CPU — su orden relativo es indeterminado

// sort.SliceStable: preserva orden original de iguales
sort.SliceStable(servers, func(i, j int) bool {
    return servers[i].CPU < servers[j].CPU
})
// Si web03 estaba antes que web02 en el input,
// seguira antes en el output (ambos tienen CPU=45.2)

// sort.SliceIsSorted: verificar si ya esta ordenado
isSorted := sort.SliceIsSorted(servers, func(i, j int) bool {
    return servers[i].CPU < servers[j].CPU
})
```

### slices.SortFunc (Go 1.21+ — preferido)

```go
import (
    "cmp"
    "slices"
)

// slices.SortFunc usa una funcion cmp (no less)
// cmp retorna: negativo (a<b), 0 (a==b), positivo (a>b)
slices.SortFunc(servers, func(a, b Server) int {
    return cmp.Compare(a.CPU, b.CPU) // Ascendente
})

// Descendente: invertir argumentos
slices.SortFunc(servers, func(a, b Server) int {
    return cmp.Compare(b.CPU, a.CPU) // b antes de a = descendente
})

// slices.SortStableFunc: version estable
slices.SortStableFunc(servers, func(a, b Server) int {
    return cmp.Compare(a.Name, b.Name)
})

// slices.IsSortedFunc
isSorted := slices.IsSortedFunc(servers, func(a, b Server) int {
    return cmp.Compare(a.CPU, b.CPU)
})
```

### Diferencias entre sort.Slice y slices.SortFunc

```
┌─────────────────────────┬──────────────────────────┬──────────────────────────┐
│ Aspecto                 │ sort.Slice               │ slices.SortFunc          │
├─────────────────────────┼──────────────────────────┼──────────────────────────┤
│ Disponible desde        │ Go 1.8                   │ Go 1.21                  │
│ Firma                   │ func(i, j int) bool      │ func(a, b T) int         │
│ Acceso a elementos      │ Por indice (closure)     │ Por valor (parametro)    │
│ Tipo de retorno         │ bool (less)              │ int (cmp: -1, 0, +1)     │
│ Generics                │ No                       │ Si                       │
│ Estabilidad por defecto │ No (usar SliceStable)    │ No (usar SortStableFunc) │
│ Algoritmo               │ Pattern-defeating QSort  │ Pattern-defeating QSort  │
│ Preferencia             │ Legacy, compatible       │ Moderno, recomendado     │
└─────────────────────────┴──────────────────────────┴──────────────────────────┘
```

---

## 3. Ordenamiento multi-criterio

### Patron: ordenar por multiples campos

```go
// El caso mas comun en SysAdmin: ordenar servidores por role, luego por name
type Host struct {
    Name   string
    Role   string
    Region string
    CPU    float64
}

hosts := []Host{
    {"web03", "web", "us-east", 45.0},
    {"db01", "database", "us-east", 30.0},
    {"web01", "web", "us-west", 72.0},
    {"db02", "database", "us-west", 25.0},
    {"web02", "web", "us-east", 45.0},
    {"cache01", "cache", "us-east", 15.0},
}

// Multi-key con slices.SortFunc:
slices.SortFunc(hosts, func(a, b Host) int {
    // Primer criterio: role (alfabetico)
    if c := cmp.Compare(a.Role, b.Role); c != 0 {
        return c
    }
    // Segundo criterio: region
    if c := cmp.Compare(a.Region, b.Region); c != 0 {
        return c
    }
    // Tercer criterio: name
    return cmp.Compare(a.Name, b.Name)
})

// Resultado:
// cache01 cache    us-east
// db01    database us-east
// db02    database us-west
// web02   web      us-east
// web03   web      us-east
// web01   web      us-west
```

### Patron: orden configurable en runtime

```go
// SysAdmin necesita: "ordenar por CPU descendente, luego por nombre"
// El criterio se decide en runtime (flags, config)

type SortKey struct {
    Field string // "name", "cpu", "memory", "role"
    Desc  bool   // true = descendente
}

func buildComparator(keys []SortKey) func(a, b Host) int {
    return func(a, b Host) int {
        for _, key := range keys {
            var c int
            switch key.Field {
            case "name":
                c = cmp.Compare(a.Name, b.Name)
            case "role":
                c = cmp.Compare(a.Role, b.Role)
            case "region":
                c = cmp.Compare(a.Region, b.Region)
            case "cpu":
                c = cmp.Compare(a.CPU, b.CPU)
            }
            if key.Desc {
                c = -c
            }
            if c != 0 {
                return c
            }
        }
        return 0
    }
}

// Uso:
criteria := []SortKey{
    {Field: "role", Desc: false},
    {Field: "cpu", Desc: true},
}
slices.SortFunc(hosts, buildComparator(criteria))
// Ordena por role ascendente, y dentro de cada role por CPU descendente
```

### sort.Interface: para tipos que se ordenan frecuentemente

```go
// Cuando ordenas un tipo repetidamente, implementar sort.Interface
// es mas eficiente que crear closures cada vez

type ByNameAndRole []Host

func (h ByNameAndRole) Len() int      { return len(h) }
func (h ByNameAndRole) Swap(i, j int) { h[i], h[j] = h[j], h[i] }
func (h ByNameAndRole) Less(i, j int) bool {
    if h[i].Role != h[j].Role {
        return h[i].Role < h[j].Role
    }
    return h[i].Name < h[j].Name
}

// Uso:
sort.Sort(ByNameAndRole(hosts))

// sort.Reverse para invertir cualquier sort.Interface:
sort.Sort(sort.Reverse(ByNameAndRole(hosts)))

// sort.Stable para ordenamiento estable:
sort.Stable(ByNameAndRole(hosts))
```

---

## 4. Busqueda binaria y colecciones ordenadas

### sort.Search (clasico, pre-Go 1.21)

```go
// sort.Search encuentra el menor indice i donde f(i) es true
// Requiere que f sea false para indices [0, i) y true para [i, n)
// Es decir: el slice debe estar ordenado respecto al criterio

sorted := []int{10, 20, 30, 40, 50, 60, 70, 80}

// Buscar 40
idx := sort.Search(len(sorted), func(i int) bool {
    return sorted[i] >= 40
})
if idx < len(sorted) && sorted[idx] == 40 {
    fmt.Println("Encontrado en indice", idx) // 3
} else {
    fmt.Println("No encontrado, insertaria en", idx)
}

// Buscar 35 (no existe)
idx = sort.Search(len(sorted), func(i int) bool {
    return sorted[i] >= 35
})
// idx = 3 (posicion donde insertarias 35)
```

### slices.BinarySearch (Go 1.21+ — preferido)

```go
// Mas ergonomico: retorna indice y bool found
sorted := []string{"alpha", "bravo", "charlie", "delta", "echo"}

idx, found := slices.BinarySearch(sorted, "charlie")
fmt.Println(idx, found) // 2 true

idx, found = slices.BinarySearch(sorted, "cisco")
fmt.Println(idx, found) // 3 false (punto de insercion)

// BinarySearchFunc para structs:
type Service struct {
    Name string
    Port int
}

services := []Service{
    {"dns", 53},
    {"http", 80},
    {"https", 443},
    {"ssh", 22},
}

// Primero: asegurar que esta ordenado por el campo de busqueda
slices.SortFunc(services, func(a, b Service) int {
    return cmp.Compare(a.Name, b.Name)
})

// Buscar por nombre
idx, found = slices.BinarySearchFunc(services, Service{Name: "http"}, func(a, b Service) int {
    return cmp.Compare(a.Name, b.Name)
})
if found {
    fmt.Printf("Found: %s on port %d\n", services[idx].Name, services[idx].Port)
}
```

### Patron: mantener slice siempre ordenado (sorted insert)

```go
// Insertar manteniendo el orden — O(log n) busqueda + O(n) shift
func sortedInsert[T cmp.Ordered](s []T, val T) []T {
    idx, _ := slices.BinarySearch(s, val)
    return slices.Insert(s, idx, val)
}

// Para tipos custom:
func sortedInsertFunc[T any](s []T, val T, cmpFn func(T, T) int) []T {
    idx, _ := slices.BinarySearchFunc(s, val, cmpFn)
    return slices.Insert(s, idx, val)
}

// Uso: lista de hosts siempre ordenada
var hosts []string
hosts = sortedInsert(hosts, "web03")  // [web03]
hosts = sortedInsert(hosts, "web01")  // [web01, web03]
hosts = sortedInsert(hosts, "db01")   // [db01, web01, web03]
hosts = sortedInsert(hosts, "web02")  // [db01, web01, web02, web03]
// Siempre ordenado — busqueda binaria funciona en O(log n)
```

### Patron: sorted delete

```go
func sortedDelete[T cmp.Ordered](s []T, val T) []T {
    idx, found := slices.BinarySearch(s, val)
    if !found {
        return s
    }
    return slices.Delete(s, idx, idx+1)
}

hosts = sortedDelete(hosts, "web02")
// [db01, web01, web03]
```

### Patron: sorted contains (O(log n) vs O(n))

```go
// Lineal: O(n) — simple pero lento para listas grandes
func containsLinear[T comparable](s []T, val T) bool {
    return slices.Contains(s, val)
}

// Binaria: O(log n) — requiere slice ordenado
func containsSorted[T cmp.Ordered](s []T, val T) bool {
    _, found := slices.BinarySearch(s, val)
    return found
}

// Performance:
// 10 elementos:    lineal ~5 comparaciones, binaria ~3.3
// 1000 elementos:  lineal ~500, binaria ~10
// 1,000,000:       lineal ~500,000, binaria ~20

// Regla practica:
// < 20 elementos → slices.Contains (mas simple)
// >= 20 elementos y se busca frecuentemente → mantener sorted + BinarySearch
// Muchas busquedas sobre el mismo conjunto → usar map[T]bool
```

---

## 5. Operaciones de conjunto con slices ordenados

### Union de dos slices ordenados

```go
// Merge de dos slices ordenados — similar a merge sort
// O(n + m) donde n y m son las longitudes
func union[T cmp.Ordered](a, b []T) []T {
    result := make([]T, 0, len(a)+len(b))
    i, j := 0, 0

    for i < len(a) && j < len(b) {
        switch cmp.Compare(a[i], b[j]) {
        case -1: // a[i] < b[j]
            result = append(result, a[i])
            i++
        case 1: // a[i] > b[j]
            result = append(result, b[j])
            j++
        case 0: // iguales — solo agregar una vez
            result = append(result, a[i])
            i++
            j++
        }
    }
    // Agregar los restantes
    result = append(result, a[i:]...)
    result = append(result, b[j:]...)
    return result
}

// Uso SysAdmin: combinar listas de hosts de dos clusters
cluster1 := []string{"db01", "web01", "web02"}
cluster2 := []string{"cache01", "db01", "web03"}
all := union(cluster1, cluster2)
// [cache01, db01, web01, web02, web03]
```

### Interseccion de dos slices ordenados

```go
func intersection[T cmp.Ordered](a, b []T) []T {
    var result []T
    i, j := 0, 0

    for i < len(a) && j < len(b) {
        switch cmp.Compare(a[i], b[j]) {
        case -1:
            i++
        case 1:
            j++
        case 0:
            result = append(result, a[i])
            i++
            j++
        }
    }
    return result
}

// Uso: hosts presentes en ambos clusters
shared := intersection(cluster1, cluster2)
// [db01]
```

### Diferencia (A - B)

```go
func difference[T cmp.Ordered](a, b []T) []T {
    var result []T
    i, j := 0, 0

    for i < len(a) && j < len(b) {
        switch cmp.Compare(a[i], b[j]) {
        case -1: // a[i] no esta en b
            result = append(result, a[i])
            i++
        case 1:
            j++
        case 0: // esta en ambos — excluir
            i++
            j++
        }
    }
    // Restantes de a no estan en b
    result = append(result, a[i:]...)
    return result
}

// Uso: hosts en cluster1 que NO estan en cluster2
only1 := difference(cluster1, cluster2)
// [web01, web02]

// Diferencia simetrica: (A-B) ∪ (B-A)
func symmetricDifference[T cmp.Ordered](a, b []T) []T {
    return union(difference(a, b), difference(b, a))
}

changed := symmetricDifference(cluster1, cluster2)
// [cache01, web01, web02, web03]
```

### Diagrama de operaciones de conjunto

```
Cluster A: {db01, web01, web02}
Cluster B: {cache01, db01, web03}

Union (A ∪ B):               {cache01, db01, web01, web02, web03}
Interseccion (A ∩ B):        {db01}
Diferencia (A - B):          {web01, web02}
Diferencia (B - A):          {cache01, web03}
Diferencia simetrica (A △ B): {cache01, web01, web02, web03}

┌────────────────────────────────────┐
│ A: {db01, web01, web02}            │
│       ┌──────────────┐             │
│       │     db01     │             │
│       │  ┌───────┐   │             │
│ web01 │  │       │   │ cache01     │
│ web02 │  │  A∩B  │   │ web03      │
│       │  │       │   │             │
│       │  └───────┘   │             │
│       │              │             │
│       └──────────────┘             │
│           B: {cache01, db01, web03}│
└────────────────────────────────────┘
```

---

## 6. Tecnica de dos punteros

### Merge de dos slices ordenados (con duplicados)

```go
// A diferencia de union (que deduplica), merge preserva todos los elementos
func merge[T cmp.Ordered](a, b []T) []T {
    result := make([]T, 0, len(a)+len(b))
    i, j := 0, 0

    for i < len(a) && j < len(b) {
        if a[i] <= b[j] {
            result = append(result, a[i])
            i++
        } else {
            result = append(result, b[j])
            j++
        }
    }
    result = append(result, a[i:]...)
    result = append(result, b[j:]...)
    return result
}

// Uso: combinar dos listas de timestamps ordenados
timestamps1 := []int64{100, 200, 300, 500}
timestamps2 := []int64{150, 200, 400}
merged := merge(timestamps1, timestamps2)
// [100, 150, 200, 200, 300, 400, 500]
```

### Partition por predicado

```go
// Reordenar slice poniendo los que cumplen el predicado primero
// Retorna el indice de particion — similar a quicksort partition
func partition[T any](s []T, pred func(T) bool) int {
    pivot := 0
    for i := range s {
        if pred(s[i]) {
            s[pivot], s[i] = s[i], s[pivot]
            pivot++
        }
    }
    return pivot
    // s[:pivot] → elementos que cumplen pred
    // s[pivot:] → elementos que NO cumplen pred
}

servers := []string{"web01", "db01", "web02", "cache01", "web03", "db02"}
idx := partition(servers, func(s string) bool {
    return strings.HasPrefix(s, "web")
})
fmt.Println(servers[:idx]) // [web01 web02 web03]
fmt.Println(servers[idx:]) // [db01 cache01 db02]
// Nota: el orden dentro de cada particion NO esta garantizado
```

### Stable partition (preserva orden)

```go
// Partition que preserva el orden relativo de los elementos
func stablePartition[T any](s []T, pred func(T) bool) (trueItems, falseItems []T) {
    for _, item := range s {
        if pred(item) {
            trueItems = append(trueItems, item)
        } else {
            falseItems = append(falseItems, item)
        }
    }
    return
}

webServers, others := stablePartition(servers, func(s string) bool {
    return strings.HasPrefix(s, "web")
})
// webServers: [web01, web02, web03] — orden preservado
// others:     [db01, cache01, db02] — orden preservado
```

### Compress / Remove consecutive duplicates in-place

```go
// Eliminar duplicados consecutivos in-place (requiere slice ordenado)
// Equivalente a std::unique en C++ o slices.Compact en Go 1.21+
func compressInPlace[T comparable](s []T) []T {
    if len(s) <= 1 {
        return s
    }

    write := 1
    for read := 1; read < len(s); read++ {
        if s[read] != s[read-1] {
            s[write] = s[read]
            write++
        }
    }
    // Limpiar elementos sobrantes para GC
    var zero T
    for i := write; i < len(s); i++ {
        s[i] = zero
    }
    return s[:write]
}

// O simplemente usar slices.Compact (Go 1.21+):
nums := []int{1, 1, 2, 2, 2, 3, 3, 4}
nums = slices.Compact(nums) // [1, 2, 3, 4]

// slices.CompactFunc para criterio custom:
type Event struct {
    Host string
    Type string
}
events := []Event{
    {"web01", "cpu_high"},
    {"web01", "cpu_high"}, // duplicado
    {"web01", "mem_high"},
    {"web02", "cpu_high"},
    {"web02", "cpu_high"}, // duplicado
}
events = slices.CompactFunc(events, func(a, b Event) bool {
    return a.Host == b.Host && a.Type == b.Type
})
// Elimina duplicados consecutivos
```

---

## 7. Rotate, Reverse y Shift

### Rotate (desplazar circularmente)

```go
// Rotate left: mover los primeros k elementos al final
// [1,2,3,4,5] rotate left 2 → [3,4,5,1,2]
// Algoritmo de las 3 reversiones: O(n) tiempo, O(1) espacio
func rotateLeft[T any](s []T, k int) {
    if len(s) == 0 {
        return
    }
    k = k % len(s)
    if k == 0 {
        return
    }
    // Reversar [0:k], reversar [k:n], reversar [0:n]
    slices.Reverse(s[:k])
    slices.Reverse(s[k:])
    slices.Reverse(s)
}

// Rotate right: mover los ultimos k elementos al inicio
func rotateRight[T any](s []T, k int) {
    if len(s) == 0 {
        return
    }
    rotateLeft(s, len(s)-(k%len(s)))
}

// Uso SysAdmin: rotar lista de servidores para round-robin
hosts := []string{"web01", "web02", "web03", "web04"}
rotateLeft(hosts, 1)
// [web02, web03, web04, web01]
// El siguiente request va a hosts[0] = web02
```

### Shift left / right (con perdida)

```go
// Shift left: descartar los primeros k elementos, rellenar con zero
func shiftLeft[T any](s []T, k int) {
    if k >= len(s) {
        var zero T
        for i := range s {
            s[i] = zero
        }
        return
    }
    copy(s, s[k:])
    var zero T
    for i := len(s) - k; i < len(s); i++ {
        s[i] = zero
    }
}

// Uso: descartar metricas antiguas en buffer fijo
metrics := []float64{1.0, 2.0, 3.0, 4.0, 5.0}
shiftLeft(metrics, 2)
// [3.0, 4.0, 5.0, 0.0, 0.0]
// Ahora puedes escribir nuevas metricas en las posiciones vacias
```

---

## 8. Flatten, Zip, Unzip

### Flatten: [][]T → []T

```go
func flatten[T any](nested [][]T) []T {
    // Calcular tamano total para pre-alocar
    total := 0
    for _, inner := range nested {
        total += len(inner)
    }

    result := make([]T, 0, total)
    for _, inner := range nested {
        result = append(result, inner...)
    }
    return result
}

// Uso: combinar resultados de multiples queries
results := [][]string{
    {"web01", "web02"},      // cluster A
    {"db01"},                // cluster B
    {"cache01", "cache02"},  // cluster C
}
all := flatten(results)
// [web01, web02, db01, cache01, cache02]
```

### Zip: combinar dos slices en pares

```go
type Pair[A, B any] struct {
    First  A
    Second B
}

func zip[A, B any](a []A, b []B) []Pair[A, B] {
    n := min(len(a), len(b))
    result := make([]Pair[A, B], n)
    for i := 0; i < n; i++ {
        result[i] = Pair[A, B]{a[i], b[i]}
    }
    return result
}

// Uso: correlacionar hosts con sus IPs
hosts := []string{"web01", "web02", "db01"}
ips := []string{"10.0.1.1", "10.0.1.2", "10.0.2.1"}
pairs := zip(hosts, ips)
// [{web01 10.0.1.1}, {web02 10.0.1.2}, {db01 10.0.2.1}]

for _, p := range pairs {
    fmt.Printf("  %s → %s\n", p.First, p.Second)
}
```

### Unzip: separar pares en dos slices

```go
func unzip[A, B any](pairs []Pair[A, B]) ([]A, []B) {
    as := make([]A, len(pairs))
    bs := make([]B, len(pairs))
    for i, p := range pairs {
        as[i] = p.First
        bs[i] = p.Second
    }
    return as, bs
}
```

---

## 9. GroupBy y Reduce

### GroupBy: agrupar por criterio

```go
func groupBy[T any, K comparable](items []T, keyFn func(T) K) map[K][]T {
    groups := make(map[K][]T)
    for _, item := range items {
        key := keyFn(item)
        groups[key] = append(groups[key], item)
    }
    return groups
}

type LogEntry struct {
    Host    string
    Level   string
    Message string
}

logs := []LogEntry{
    {"web01", "ERROR", "connection timeout"},
    {"web01", "WARN", "high latency"},
    {"db01", "ERROR", "disk full"},
    {"web02", "ERROR", "connection timeout"},
    {"db01", "INFO", "backup complete"},
}

// Agrupar por host
byHost := groupBy(logs, func(e LogEntry) string { return e.Host })
// byHost["web01"] = [{web01 ERROR ...}, {web01 WARN ...}]
// byHost["db01"]  = [{db01 ERROR ...}, {db01 INFO ...}]
// byHost["web02"] = [{web02 ERROR ...}]

// Agrupar por nivel
byLevel := groupBy(logs, func(e LogEntry) string { return e.Level })
// byLevel["ERROR"] = 3 entries
// byLevel["WARN"]  = 1 entry
// byLevel["INFO"]  = 1 entry

// Agrupar por combinacion de campos
type HostLevel struct {
    Host  string
    Level string
}
byHostLevel := groupBy(logs, func(e LogEntry) HostLevel {
    return HostLevel{e.Host, e.Level}
})
```

### Reduce / Fold

```go
func reduce[T, R any](items []T, initial R, fn func(R, T) R) R {
    result := initial
    for _, item := range items {
        result = fn(result, item)
    }
    return result
}

// Suma de CPU
type Server struct {
    Name string
    CPU  float64
}

servers := []Server{
    {"web01", 45.0},
    {"web02", 72.0},
    {"db01", 30.0},
}

totalCPU := reduce(servers, 0.0, func(acc float64, s Server) float64 {
    return acc + s.CPU
})
// totalCPU = 147.0

avgCPU := totalCPU / float64(len(servers))
// avgCPU = 49.0

// Encontrar el servidor con mayor CPU
maxServer := reduce(servers, servers[0], func(best, s Server) Server {
    if s.CPU > best.CPU {
        return s
    }
    return best
})
// maxServer = {web02, 72.0}

// Construir string de reporte
report := reduce(servers, "", func(acc string, s Server) string {
    return acc + fmt.Sprintf("  %-8s CPU=%.1f%%\n", s.Name, s.CPU)
})
```

### Map (transform)

```go
func mapSlice[T, R any](items []T, fn func(T) R) []R {
    result := make([]R, len(items))
    for i, item := range items {
        result[i] = fn(item)
    }
    return result
}

// Extraer nombres de servidores
names := mapSlice(servers, func(s Server) string {
    return s.Name
})
// [web01, web02, db01]

// Crear reporte de estado
statuses := mapSlice(servers, func(s Server) string {
    status := "OK"
    if s.CPU > 70 {
        status = "HIGH"
    }
    return fmt.Sprintf("%s: %s (%.0f%%)", s.Name, status, s.CPU)
})
// [web01: OK (45%), web02: HIGH (72%), db01: OK (30%)]
```

---

## 10. Pipeline funcional: encadenar operaciones

### Pipeline con generics

```go
// Problema: encadenar Map → Filter → Sort → Take es verbose con funciones separadas
// Solucion: un tipo Pipeline que encadena operaciones fluentemente

type Pipeline[T any] struct {
    items []T
}

func From[T any](items []T) Pipeline[T] {
    return Pipeline[T]{items: slices.Clone(items)}
}

func (p Pipeline[T]) Filter(pred func(T) bool) Pipeline[T] {
    var result []T
    for _, item := range p.items {
        if pred(item) {
            result = append(result, item)
        }
    }
    return Pipeline[T]{items: result}
}

func (p Pipeline[T]) Sort(cmpFn func(T, T) int) Pipeline[T] {
    slices.SortFunc(p.items, cmpFn)
    return p
}

func (p Pipeline[T]) Take(n int) Pipeline[T] {
    if n > len(p.items) {
        n = len(p.items)
    }
    return Pipeline[T]{items: p.items[:n]}
}

func (p Pipeline[T]) Skip(n int) Pipeline[T] {
    if n > len(p.items) {
        n = len(p.items)
    }
    return Pipeline[T]{items: p.items[n:]}
}

func (p Pipeline[T]) Each(fn func(T)) {
    for _, item := range p.items {
        fn(item)
    }
}

func (p Pipeline[T]) Result() []T {
    return p.items
}

func (p Pipeline[T]) Count() int {
    return len(p.items)
}

// Uso en SysAdmin — pipeline de servidor:
type ServerInfo struct {
    Name   string
    Role   string
    CPU    float64
    Status string
}

fleet := []ServerInfo{
    {"web01", "web", 78.5, "online"},
    {"web02", "web", 45.2, "online"},
    {"web03", "web", 12.0, "draining"},
    {"db01", "database", 55.0, "online"},
    {"db02", "database", 92.1, "online"},
    {"cache01", "cache", 15.0, "online"},
    {"cache02", "cache", 0.0, "offline"},
}

// Top 3 servidores online con mayor CPU
top3 := From(fleet).
    Filter(func(s ServerInfo) bool {
        return s.Status == "online"
    }).
    Sort(func(a, b ServerInfo) int {
        return cmp.Compare(b.CPU, a.CPU) // Descendente
    }).
    Take(3).
    Result()

for _, s := range top3 {
    fmt.Printf("  %s (%.1f%%)\n", s.Name, s.CPU)
}
// db02 (92.1%)
// web01 (78.5%)
// db01 (55.0%)
```

### Pipeline con Map (requiere funcion libre por limitacion de generics)

```go
// Go no permite metodos con type parameters adicionales al del tipo
// Asi que Map no puede ser un metodo de Pipeline[T] que retorne Pipeline[R]
// Solucion: funcion libre

func Map[T, R any](p Pipeline[T], fn func(T) R) Pipeline[R] {
    return Pipeline[R]{
        items: mapSlice(p.items, fn),
    }
}

// Pipeline con Map:
names := Map(
    From(fleet).
        Filter(func(s ServerInfo) bool { return s.CPU > 50 }).
        Sort(func(a, b ServerInfo) int { return cmp.Compare(b.CPU, a.CPU) }),
    func(s ServerInfo) string { return s.Name },
).Result()
// [db02, web01, db01]
```

---

## 11. Patrones SysAdmin completos

### Patron: Inventario con operaciones de diff

```go
// Comparar el estado actual del inventario con el deseado
type HostEntry struct {
    Name   string
    IP     string
    Role   string
    Active bool
}

func diffInventory(current, desired []HostEntry) (toAdd, toRemove, toUpdate []HostEntry) {
    // Indexar por nombre
    currentMap := make(map[string]HostEntry, len(current))
    for _, h := range current {
        currentMap[h.Name] = h
    }
    desiredMap := make(map[string]HostEntry, len(desired))
    for _, h := range desired {
        desiredMap[h.Name] = h
    }

    // Encontrar adds y updates
    for _, d := range desired {
        c, exists := currentMap[d.Name]
        if !exists {
            toAdd = append(toAdd, d)
        } else if c.IP != d.IP || c.Role != d.Role || c.Active != d.Active {
            toUpdate = append(toUpdate, d)
        }
    }

    // Encontrar removes
    for _, c := range current {
        if _, exists := desiredMap[c.Name]; !exists {
            toRemove = append(toRemove, c)
        }
    }

    // Ordenar para output determinista
    sortByName := func(a, b HostEntry) int { return cmp.Compare(a.Name, b.Name) }
    slices.SortFunc(toAdd, sortByName)
    slices.SortFunc(toRemove, sortByName)
    slices.SortFunc(toUpdate, sortByName)
    return
}

// Uso:
current := []HostEntry{
    {"web01", "10.0.1.1", "web", true},
    {"web02", "10.0.1.2", "web", true},
    {"db01", "10.0.2.1", "database", true},
    {"old01", "10.0.9.1", "legacy", false},
}

desired := []HostEntry{
    {"web01", "10.0.1.1", "web", true},
    {"web02", "10.0.1.5", "web", true},   // IP cambio
    {"db01", "10.0.2.1", "database", true},
    {"cache01", "10.0.3.1", "cache", true}, // nuevo
}

add, remove, update := diffInventory(current, desired)
// add:    [cache01]
// remove: [old01]
// update: [web02] — cambio de IP
```

### Patron: Top-N con multiples metricas

```go
type MetricSnapshot struct {
    Host      string
    CPU       float64
    Memory    float64
    DiskIO    float64
    NetworkIO float64
}

// Top N servidores por una metrica arbitraria
func topN(snapshots []MetricSnapshot, n int, metric func(MetricSnapshot) float64) []MetricSnapshot {
    sorted := slices.Clone(snapshots)
    slices.SortFunc(sorted, func(a, b MetricSnapshot) int {
        return cmp.Compare(metric(b), metric(a)) // Descendente
    })
    if n > len(sorted) {
        n = len(sorted)
    }
    return sorted[:n]
}

// Top N por percentil combinado (multi-metrica)
func topNComposite(snapshots []MetricSnapshot, n int, weights map[string]float64) []MetricSnapshot {
    score := func(m MetricSnapshot) float64 {
        return m.CPU*weights["cpu"] +
            m.Memory*weights["memory"] +
            m.DiskIO*weights["disk"] +
            m.NetworkIO*weights["network"]
    }
    return topN(snapshots, n, score)
}

// Uso:
snapshots := []MetricSnapshot{
    {"web01", 80, 60, 30, 50},
    {"web02", 40, 90, 20, 30},
    {"db01", 60, 70, 90, 10},
    {"cache01", 20, 95, 10, 5},
}

// Top 2 por CPU
fmt.Println("Top CPU:", topN(snapshots, 2, func(m MetricSnapshot) float64 { return m.CPU }))

// Top 2 por score compuesto
weights := map[string]float64{"cpu": 0.4, "memory": 0.3, "disk": 0.2, "network": 0.1}
fmt.Println("Top composite:", topNComposite(snapshots, 2, weights))
```

### Patron: Deployment ordering con dependencias

```go
// Ordenar servicios para deploy respetando dependencias
// (topological sort simplificado para slices)
type DeployUnit struct {
    Name      string
    DependsOn []string
}

func deployOrder(units []DeployUnit) ([]string, error) {
    // Construir grafo de dependencias
    deps := make(map[string][]string)
    all := make(map[string]bool)
    for _, u := range units {
        deps[u.Name] = u.DependsOn
        all[u.Name] = true
    }

    var order []string
    visited := make(map[string]bool)
    inStack := make(map[string]bool) // Para detectar ciclos

    var visit func(name string) error
    visit = func(name string) error {
        if inStack[name] {
            return fmt.Errorf("circular dependency detected at %s", name)
        }
        if visited[name] {
            return nil
        }
        inStack[name] = true
        for _, dep := range deps[name] {
            if !all[dep] {
                return fmt.Errorf("unknown dependency %q required by %s", dep, name)
            }
            if err := visit(dep); err != nil {
                return err
            }
        }
        inStack[name] = false
        visited[name] = true
        order = append(order, name)
        return nil
    }

    // Visitar en orden alfabetico para determinismo
    names := make([]string, 0, len(all))
    for name := range all {
        names = append(names, name)
    }
    slices.Sort(names)

    for _, name := range names {
        if err := visit(name); err != nil {
            return nil, err
        }
    }
    return order, nil
}

// Uso:
units := []DeployUnit{
    {Name: "api", DependsOn: []string{"database", "cache"}},
    {Name: "database", DependsOn: nil},
    {Name: "cache", DependsOn: nil},
    {Name: "frontend", DependsOn: []string{"api"}},
    {Name: "monitoring", DependsOn: []string{"api", "database"}},
}

order, err := deployOrder(units)
if err != nil {
    log.Fatal(err)
}
fmt.Println("Deploy order:", order)
// [cache, database, api, frontend, monitoring]
```

---

## 12. slices package: funciones avanzadas (Go 1.21+)

### Funciones menos conocidas pero utiles

```go
import "slices"

// ── Grow: pre-alocar capacidad sin cambiar len ──
s := []int{1, 2, 3}
s = slices.Grow(s, 100) // Asegurar cap >= len+100
fmt.Println(len(s), cap(s)) // 3, >= 103
// Util antes de un loop de appends donde conoces la cantidad

// ── Clip: reducir cap a len ──
s = make([]int, 3, 100)
s = slices.Clip(s)
fmt.Println(len(s), cap(s)) // 3, 3
// Libera memoria del backing array sobrante

// ── Replace: reemplazar rango ──
s = []int{0, 1, 2, 3, 4, 5}
s = slices.Replace(s, 2, 4, 20, 30, 40)
// Reemplaza s[2:4] con [20, 30, 40]
fmt.Println(s) // [0, 1, 20, 30, 40, 4, 5]

// ── ContainsFunc: buscar con predicado ──
type Alert struct {
    Host     string
    Severity string
}
alerts := []Alert{
    {"web01", "warning"},
    {"db01", "critical"},
    {"web02", "info"},
}
hasCritical := slices.ContainsFunc(alerts, func(a Alert) bool {
    return a.Severity == "critical"
})
// true

// ── IndexFunc: indice del primer match con predicado ──
idx := slices.IndexFunc(alerts, func(a Alert) bool {
    return a.Host == "db01"
})
// idx = 1

// ── MinFunc / MaxFunc: para tipos no-Ordered ──
mostSevere := slices.MaxFunc(alerts, func(a, b Alert) int {
    severityOrder := map[string]int{"info": 0, "warning": 1, "critical": 2}
    return cmp.Compare(severityOrder[a.Severity], severityOrder[b.Severity])
})
// mostSevere = {db01 critical}

// ── Delete / DeleteFunc ──
// Delete(s, i, j) elimina s[i:j]
s = []int{0, 1, 2, 3, 4, 5}
s = slices.Delete(s, 1, 3)
fmt.Println(s) // [0, 3, 4, 5]

// DeleteFunc elimina todos los que cumplen predicado
hosts := []string{"web01", "dead-host", "web02", "dead-host2", "db01"}
hosts = slices.DeleteFunc(hosts, func(h string) bool {
    return strings.HasPrefix(h, "dead")
})
// [web01, web02, db01]
```

### Comparacion con patrones manuales

```
┌─────────────────────────────┬──────────────────────────────┬──────────────────────────┐
│ Operacion                   │ Manual (pre-1.21)            │ slices package (1.21+)   │
├─────────────────────────────┼──────────────────────────────┼──────────────────────────┤
│ Clonar                      │ make + copy / append(nil,..) │ slices.Clone(s)          │
│ Comparar                    │ Loop / reflect.DeepEqual     │ slices.Equal(a, b)       │
│ Ordenar                     │ sort.Slice(s, less)          │ slices.SortFunc(s, cmp)  │
│ Buscar                      │ sort.Search(n, f)            │ slices.BinarySearch(s,v) │
│ Contiene                    │ Loop                         │ slices.Contains(s, v)    │
│ Indice                      │ Loop                         │ slices.Index(s, v)       │
│ Insertar                    │ append + copy                │ slices.Insert(s, i, v)   │
│ Eliminar rango              │ append(s[:i], s[j:]...)      │ slices.Delete(s, i, j)   │
│ Eliminar por predicado      │ Filter inverso               │ slices.DeleteFunc(s, f)  │
│ Dedup sorted                │ Loop con write pointer       │ slices.Compact(s)        │
│ Reversar                    │ Loop con swap                │ slices.Reverse(s)        │
│ Min/Max                     │ Loop                         │ slices.Min(s)/Max(s)     │
│ Pre-alocar                  │ Manual cap calc              │ slices.Grow(s, n)        │
│ Recortar cap                │ append(s[:0:0], s...)        │ slices.Clip(s)           │
│ Reemplazar rango            │ append + copy complicado     │ slices.Replace(s,i,j,..) │
└─────────────────────────────┴──────────────────────────────┴──────────────────────────┘
```

---

## 13. Comparacion: Go vs C vs Rust — patrones de coleccion

### Go: slices + generics + slices package

```go
// Filtrar, mapear, ordenar — con generics y funciones de stdlib
servers := []Server{...}

// Filter + Sort + Map es verbose pero claro
var online []Server
for _, s := range servers {
    if s.Status == "online" {
        online = append(online, s)
    }
}
slices.SortFunc(online, func(a, b Server) int {
    return cmp.Compare(b.CPU, a.CPU)
})
names := make([]string, len(online))
for i, s := range online {
    names[i] = s.Name
}

// Con Go 1.23+ iterators (rangefunc):
// for s := range Filter(servers, pred) { ... }
// Experimental, pero la direccion del lenguaje
```

### C: qsort + bsearch + macros

```c
// C usa funciones de stdlib con void* y casts
#include <stdlib.h>
#include <string.h>

typedef struct {
    char name[32];
    double cpu;
} Server;

// Comparador para qsort — recibe void* genericos
int cmp_by_cpu(const void *a, const void *b) {
    double diff = ((Server*)b)->cpu - ((Server*)a)->cpu;
    return (diff > 0) - (diff < 0);
}

void sort_servers(Server *servers, int n) {
    qsort(servers, n, sizeof(Server), cmp_by_cpu);
}

// bsearch requiere array ordenado
Server *found = bsearch(&key, servers, n, sizeof(Server), cmp_by_name);

// No hay generics → macros o void* para cada tipo
// No hay filter/map → loops manuales
// No hay slice header → (pointer, length) siempre manual
// Facil de cometer buffer overflow
```

### Rust: iterators + closures + collect

```rust
// Rust tiene el pipeline mas ergonomico con iteradores lazy
let servers: Vec<Server> = vec![...];

let top_names: Vec<&str> = servers.iter()
    .filter(|s| s.status == "online")
    .sorted_by(|a, b| b.cpu.partial_cmp(&a.cpu).unwrap())
    .take(5)
    .map(|s| s.name.as_str())
    .collect();

// Ventajas sobre Go:
// - Lazy: filter no crea Vec intermedio
// - Encadenable: cada operacion retorna un Iterator
// - Zero-cost: el compilador fusiona operaciones
// - Type-safe: el compilador infiere tipos en cada paso

// Operaciones de conjunto (BTreeSet o HashSet):
use std::collections::HashSet;
let a: HashSet<&str> = ["web01", "db01"].into();
let b: HashSet<&str> = ["db01", "cache01"].into();
let union: HashSet<_> = a.union(&b).collect();
let inter: HashSet<_> = a.intersection(&b).collect();
let diff:  HashSet<_> = a.difference(&b).collect();
// Built-in, zero-allocation iterators
```

### Tabla comparativa

```
┌──────────────────────┬──────────────────────┬──────────────────────┬──────────────────────┐
│ Aspecto              │ Go                   │ C                    │ Rust                 │
├──────────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤
│ Sort                 │ slices.SortFunc      │ qsort (void*)        │ .sort_by() / .sorted │
│ Search               │ slices.BinarySearch  │ bsearch (void*)      │ .binary_search()     │
│ Filter               │ Loop + append        │ Loop manual          │ .filter() lazy       │
│ Map/Transform        │ Loop + make          │ Loop manual          │ .map() lazy          │
│ Pipeline             │ Verbose (eager)      │ No existe            │ Iterator chain (lazy)│
│ Generics             │ Go 1.18+ (limitados) │ No (void* / macros)  │ Si (full monomorphiz)│
│ Set operations       │ Manual con sorted    │ Manual               │ HashSet/BTreeSet     │
│ Reduce               │ Manual loop          │ Manual loop          │ .fold() / .reduce()  │
│ Lazy evaluation      │ No (Go 1.23 rangefu) │ No                   │ Si (iterators)       │
│ Memory overhead      │ Eager copies         │ In-place (manual)    │ Zero-cost iterators  │
│ Ergonomia            │ Media                │ Baja                 │ Alta                 │
│ Debugging            │ Claro (cada paso)    │ gdb                  │ Algo opaco (chain)   │
└──────────────────────┴──────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 14. Programa completo: Infrastructure Config Diff Tool

Herramienta CLI que compara dos estados de infraestructura (listas de servidores), calcula diferencias, aplica filtros y genera un plan de accion ordenado por dependencias.

```go
package main

import (
	"cmp"
	"fmt"
	"slices"
	"strings"
)

// ─── Tipos ──────────────────────────────────────────────────────

type Role string

const (
	RoleWeb      Role = "web"
	RoleAPI      Role = "api"
	RoleDB       Role = "database"
	RoleCache    Role = "cache"
	RoleMonitor  Role = "monitoring"
	RoleLB       Role = "loadbalancer"
)

// rolePriority define el orden de deploy
func rolePriority(r Role) int {
	switch r {
	case RoleDB:
		return 0
	case RoleCache:
		return 1
	case RoleAPI:
		return 2
	case RoleWeb:
		return 3
	case RoleLB:
		return 4
	case RoleMonitor:
		return 5
	default:
		return 99
	}
}

type Host struct {
	Name    string
	IP      string
	Role    Role
	Version string
	Tags    []string
}

func (h Host) String() string {
	tags := ""
	if len(h.Tags) > 0 {
		tags = " [" + strings.Join(h.Tags, ",") + "]"
	}
	return fmt.Sprintf("%-12s %-16s %-14s v%-6s%s", h.Name, h.IP, h.Role, h.Version, tags)
}

// ─── Generics Utilities ─────────────────────────────────────────

func mapSlice[T, R any](items []T, fn func(T) R) []R {
	result := make([]R, len(items))
	for i, item := range items {
		result[i] = fn(item)
	}
	return result
}

func filterSlice[T any](items []T, pred func(T) bool) []T {
	var result []T
	for _, item := range items {
		if pred(item) {
			result = append(result, item)
		}
	}
	return result
}

func groupBy[T any, K comparable](items []T, keyFn func(T) K) map[K][]T {
	groups := make(map[K][]T)
	for _, item := range items {
		key := keyFn(item)
		groups[key] = append(groups[key], item)
	}
	return groups
}

func reduce[T, R any](items []T, initial R, fn func(R, T) R) R {
	acc := initial
	for _, item := range items {
		acc = fn(acc, item)
	}
	return acc
}

// ─── Set Operations (sorted slices) ─────────────────────────────

func sortedNames(hosts []Host) []string {
	names := mapSlice(hosts, func(h Host) string { return h.Name })
	slices.Sort(names)
	return names
}

func setDifference(a, b []string) []string {
	var result []string
	i, j := 0, 0
	for i < len(a) && j < len(b) {
		switch cmp.Compare(a[i], b[j]) {
		case -1:
			result = append(result, a[i])
			i++
		case 1:
			j++
		case 0:
			i++
			j++
		}
	}
	result = append(result, a[i:]...)
	return result
}

func setIntersection(a, b []string) []string {
	var result []string
	i, j := 0, 0
	for i < len(a) && j < len(b) {
		switch cmp.Compare(a[i], b[j]) {
		case -1:
			i++
		case 1:
			j++
		case 0:
			result = append(result, a[i])
			i++
			j++
		}
	}
	return result
}

// ─── Diff Engine ────────────────────────────────────────────────

type DiffAction string

const (
	ActionAdd    DiffAction = "ADD"
	ActionRemove DiffAction = "REMOVE"
	ActionUpdate DiffAction = "UPDATE"
	ActionKeep   DiffAction = "KEEP"
)

type DiffEntry struct {
	Action  DiffAction
	Host    Host
	OldHost *Host   // Solo para UPDATE
	Changes []string // Descripcion de cambios
}

func computeDiff(current, desired []Host) []DiffEntry {
	currentMap := make(map[string]Host, len(current))
	for _, h := range current {
		currentMap[h.Name] = h
	}
	desiredMap := make(map[string]Host, len(desired))
	for _, h := range desired {
		desiredMap[h.Name] = h
	}

	var entries []DiffEntry

	// Adds y updates
	for _, d := range desired {
		c, exists := currentMap[d.Name]
		if !exists {
			entries = append(entries, DiffEntry{
				Action: ActionAdd,
				Host:   d,
			})
		} else {
			var changes []string
			if c.IP != d.IP {
				changes = append(changes, fmt.Sprintf("IP: %s → %s", c.IP, d.IP))
			}
			if c.Role != d.Role {
				changes = append(changes, fmt.Sprintf("Role: %s → %s", c.Role, d.Role))
			}
			if c.Version != d.Version {
				changes = append(changes, fmt.Sprintf("Version: %s → %s", c.Version, d.Version))
			}
			if !slices.Equal(c.Tags, d.Tags) {
				changes = append(changes, fmt.Sprintf("Tags: %v → %v", c.Tags, d.Tags))
			}

			if len(changes) > 0 {
				entries = append(entries, DiffEntry{
					Action:  ActionUpdate,
					Host:    d,
					OldHost: &c,
					Changes: changes,
				})
			} else {
				entries = append(entries, DiffEntry{
					Action: ActionKeep,
					Host:   d,
				})
			}
		}
	}

	// Removes
	for _, c := range current {
		if _, exists := desiredMap[c.Name]; !exists {
			entries = append(entries, DiffEntry{
				Action: ActionRemove,
				Host:   c,
			})
		}
	}

	return entries
}

// ─── Action Plan ────────────────────────────────────────────────

type ActionPlan struct {
	ToRemove []Host
	ToAdd    []Host
	ToUpdate []DiffEntry
	ToKeep   []Host
}

func buildPlan(diff []DiffEntry) ActionPlan {
	var plan ActionPlan

	for _, d := range diff {
		switch d.Action {
		case ActionAdd:
			plan.ToAdd = append(plan.ToAdd, d.Host)
		case ActionRemove:
			plan.ToRemove = append(plan.ToRemove, d.Host)
		case ActionUpdate:
			plan.ToUpdate = append(plan.ToUpdate, d)
		case ActionKeep:
			plan.ToKeep = append(plan.ToKeep, d.Host)
		}
	}

	// Ordenar removes: inverso al deploy (quitar LB primero, DB ultimo)
	slices.SortFunc(plan.ToRemove, func(a, b Host) int {
		if c := cmp.Compare(rolePriority(b.Role), rolePriority(a.Role)); c != 0 {
			return c
		}
		return cmp.Compare(a.Name, b.Name)
	})

	// Ordenar adds: en orden de deploy (DB primero, LB ultimo)
	slices.SortFunc(plan.ToAdd, func(a, b Host) int {
		if c := cmp.Compare(rolePriority(a.Role), rolePriority(b.Role)); c != 0 {
			return c
		}
		return cmp.Compare(a.Name, b.Name)
	})

	// Ordenar updates: por prioridad de role, luego por nombre
	slices.SortFunc(plan.ToUpdate, func(a, b DiffEntry) int {
		if c := cmp.Compare(rolePriority(a.Host.Role), rolePriority(b.Host.Role)); c != 0 {
			return c
		}
		return cmp.Compare(a.Host.Name, b.Host.Name)
	})

	return plan
}

// ─── Batch Generator ────────────────────────────────────────────

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

// ─── Display ────────────────────────────────────────────────────

func printSection(title string) {
	fmt.Printf("\n%s\n%s\n", title, strings.Repeat("─", len(title)))
}

func printHosts(hosts []Host) {
	if len(hosts) == 0 {
		fmt.Println("  (none)")
		return
	}
	for _, h := range hosts {
		fmt.Printf("  %s\n", h)
	}
}

// ─── Main ───────────────────────────────────────────────────────

func main() {
	// Estado actual de la infraestructura
	current := []Host{
		{"lb01", "10.0.0.1", RoleLB, "2.1.0", []string{"prod", "us-east"}},
		{"web01", "10.0.1.1", RoleWeb, "3.2.0", []string{"prod", "us-east"}},
		{"web02", "10.0.1.2", RoleWeb, "3.2.0", []string{"prod", "us-east"}},
		{"web03", "10.0.1.3", RoleWeb, "3.1.0", []string{"prod", "us-west"}},
		{"api01", "10.0.2.1", RoleAPI, "1.5.0", []string{"prod", "us-east"}},
		{"api02", "10.0.2.2", RoleAPI, "1.5.0", []string{"prod", "us-west"}},
		{"db01", "10.0.3.1", RoleDB, "14.2", []string{"prod", "primary"}},
		{"db02", "10.0.3.2", RoleDB, "14.2", []string{"prod", "replica"}},
		{"cache01", "10.0.4.1", RoleCache, "7.0.0", []string{"prod", "us-east"}},
		{"mon01", "10.0.5.1", RoleMonitor, "2.45", []string{"infra"}},
		{"old-proxy", "10.0.9.1", RoleLB, "1.0.0", []string{"deprecated"}},
	}

	// Estado deseado (despues de planificacion)
	desired := []Host{
		{"lb01", "10.0.0.1", RoleLB, "2.2.0", []string{"prod", "us-east"}},        // version update
		{"web01", "10.0.1.1", RoleWeb, "3.3.0", []string{"prod", "us-east"}},       // version update
		{"web02", "10.0.1.2", RoleWeb, "3.3.0", []string{"prod", "us-east"}},       // version update
		{"web03", "10.0.1.3", RoleWeb, "3.3.0", []string{"prod", "us-west"}},       // version update
		{"web04", "10.0.1.4", RoleWeb, "3.3.0", []string{"prod", "us-west"}},       // NEW
		{"api01", "10.0.2.1", RoleAPI, "1.6.0", []string{"prod", "us-east"}},       // version update
		{"api02", "10.0.2.2", RoleAPI, "1.6.0", []string{"prod", "us-west"}},       // version update
		{"api03", "10.0.2.3", RoleAPI, "1.6.0", []string{"prod", "us-east"}},       // NEW
		{"db01", "10.0.3.1", RoleDB, "14.2", []string{"prod", "primary"}},          // unchanged
		{"db02", "10.0.3.2", RoleDB, "14.2", []string{"prod", "replica"}},          // unchanged
		{"db03", "10.0.3.3", RoleDB, "14.2", []string{"prod", "replica"}},          // NEW
		{"cache01", "10.0.4.1", RoleCache, "7.2.0", []string{"prod", "us-east"}},   // version update
		{"cache02", "10.0.4.2", RoleCache, "7.2.0", []string{"prod", "us-west"}},   // NEW
		{"mon01", "10.0.5.1", RoleMonitor, "2.50", []string{"infra", "us-east"}},   // version + tags
		// old-proxy REMOVED
	}

	fmt.Println(strings.Repeat("═", 70))
	fmt.Println("  INFRASTRUCTURE CONFIG DIFF TOOL")
	fmt.Println(strings.Repeat("═", 70))

	// ═══ Set Analysis ═══
	printSection("SET ANALYSIS")
	currentNames := sortedNames(current)
	desiredNames := sortedNames(desired)

	added := setDifference(desiredNames, currentNames)
	removed := setDifference(currentNames, desiredNames)
	common := setIntersection(currentNames, desiredNames)

	fmt.Printf("  Current hosts:  %d\n", len(current))
	fmt.Printf("  Desired hosts:  %d\n", len(desired))
	fmt.Printf("  To add:         %d  %v\n", len(added), added)
	fmt.Printf("  To remove:      %d  %v\n", len(removed), removed)
	fmt.Printf("  Common:         %d\n", len(common))

	// ═══ Detailed Diff ═══
	diff := computeDiff(current, desired)

	// Stats por accion
	byAction := groupBy(diff, func(d DiffEntry) DiffAction { return d.Action })

	printSection("DIFF SUMMARY")
	for _, action := range []DiffAction{ActionAdd, ActionRemove, ActionUpdate, ActionKeep} {
		entries := byAction[action]
		fmt.Printf("  %-8s %d hosts\n", action, len(entries))
	}

	// ═══ Changes Detail ═══
	updates := filterSlice(diff, func(d DiffEntry) bool { return d.Action == ActionUpdate })
	if len(updates) > 0 {
		printSection("CHANGES DETAIL")
		for _, u := range updates {
			fmt.Printf("  %s:\n", u.Host.Name)
			for _, change := range u.Changes {
				fmt.Printf("    • %s\n", change)
			}
		}
	}

	// ═══ Action Plan (ordered) ═══
	plan := buildPlan(diff)

	printSection("ACTION PLAN")

	// Phase 1: Remove (inverse dependency order)
	if len(plan.ToRemove) > 0 {
		fmt.Println("\n  Phase 1: REMOVE (reverse dependency order)")
		for i, h := range plan.ToRemove {
			fmt.Printf("    %d. [REMOVE] %s (%s)\n", i+1, h.Name, h.Role)
		}
	}

	// Phase 2: Add (dependency order)
	if len(plan.ToAdd) > 0 {
		fmt.Println("\n  Phase 2: ADD (dependency order)")
		for i, h := range plan.ToAdd {
			fmt.Printf("    %d. [ADD] %s (%s) → %s\n", i+1, h.Name, h.Role, h.IP)
		}
	}

	// Phase 3: Update (batched by role)
	if len(plan.ToUpdate) > 0 {
		fmt.Println("\n  Phase 3: UPDATE (batched by role)")
		updateHosts := mapSlice(plan.ToUpdate, func(d DiffEntry) Host { return d.Host })
		byRole := groupBy(updateHosts, func(h Host) Role { return h.Role })

		// Procesar roles en orden de prioridad
		roleOrder := []Role{RoleDB, RoleCache, RoleAPI, RoleWeb, RoleLB, RoleMonitor}
		batchNum := 0
		for _, role := range roleOrder {
			hosts, ok := byRole[role]
			if !ok {
				continue
			}
			slices.SortFunc(hosts, func(a, b Host) int {
				return cmp.Compare(a.Name, b.Name)
			})
			batches := chunk(hosts, 2) // Max 2 por batch (rolling update)
			for _, batch := range batches {
				batchNum++
				names := mapSlice(batch, func(h Host) string { return h.Name })
				fmt.Printf("    Batch %d [%s]: %s\n", batchNum, role,
					strings.Join(names, ", "))
			}
		}
	}

	// ═══ Summary stats ═══
	printSection("SUMMARY")

	// Contar por role en estado deseado
	desiredByRole := groupBy(desired, func(h Host) Role { return h.Role })
	fmt.Println("  Desired fleet by role:")
	for _, role := range []Role{RoleDB, RoleCache, RoleAPI, RoleWeb, RoleLB, RoleMonitor} {
		hosts := desiredByRole[role]
		if len(hosts) > 0 {
			names := mapSlice(hosts, func(h Host) string { return h.Name })
			slices.Sort(names)
			fmt.Printf("    %-14s %d  (%s)\n", role, len(hosts), strings.Join(names, ", "))
		}
	}

	// Version consistency check
	printSection("VERSION CONSISTENCY CHECK")
	versionByRole := groupBy(desired, func(h Host) string {
		return string(h.Role)
	})
	for roleName, hosts := range versionByRole {
		versions := mapSlice(hosts, func(h Host) string { return h.Version })
		versions = slices.Compact(slices.Clone(versions))
		slices.Sort(versions)
		versions = slices.Compact(versions)
		if len(versions) > 1 {
			fmt.Printf("  ⚠ %s: mixed versions %v\n", roleName, versions)
		} else {
			fmt.Printf("  ✓ %s: consistent v%s\n", roleName, versions[0])
		}
	}

	// Total actions
	totalActions := len(plan.ToAdd) + len(plan.ToRemove) + len(plan.ToUpdate)
	fmt.Printf("\n  Total actions: %d (add=%d, remove=%d, update=%d, keep=%d)\n",
		totalActions, len(plan.ToAdd), len(plan.ToRemove),
		len(plan.ToUpdate), len(plan.ToKeep))

	// Techniques used
	printSection("SLICE PATTERNS USED IN THIS PROGRAM")
	patterns := []string{
		"mapSlice[T,R]     — transform Host→string, DiffEntry→Host",
		"filterSlice[T]    — select by predicate (action type, status)",
		"groupBy[T,K]      — group hosts by role, diffs by action",
		"reduce[T,R]       — aggregate counts and statistics",
		"setDifference     — sorted two-pointer A-B",
		"setIntersection   — sorted two-pointer A∩B",
		"sortedNames       — map+sort pipeline for set operations",
		"slices.SortFunc   — multi-key sorting (role priority + name)",
		"slices.Clone      — defensive copy for sort-in-place",
		"slices.Compact    — dedup sorted versions",
		"slices.Equal      — compare tag slices for diff detection",
		"chunk[T]          — batch hosts for rolling updates",
		"rolePriority      — lookup table for deploy ordering",
	}
	for _, p := range patterns {
		fmt.Printf("  • %s\n", p)
	}
}
```

---

## 15. Tabla de errores comunes

```
┌────┬──────────────────────────────────────────┬──────────────────────────────────────────┬─────────┐
│ #  │ Error                                    │ Solucion                                 │ Nivel   │
├────┼──────────────────────────────────────────┼──────────────────────────────────────────┼─────────┤
│  1 │ sort.Slice con less que no es estricto   │ Usar < no <=, o cmp.Compare que retorna │ Logic   │
│    │ (a <= b viola el contrato de less)        │ -1/0/+1                                  │         │
│  2 │ BinarySearch en slice no ordenado        │ Siempre sort antes de binary search       │ Logic   │
│  3 │ sort.Slice modifica slice compartido     │ Clonar antes de ordenar                   │ Logic   │
│  4 │ Multi-key sort con condiciones erroneas  │ Comparar campo por campo con cortocircuito│ Logic   │
│  5 │ Sorted insert sin reasignar resultado    │ s = slices.Insert(s, idx, val) — siempre  │ Logic   │
│    │                                          │ reasignar                                │         │
│  6 │ Set operations en slices no ordenados    │ Ordenar antes o usar map[T]bool           │ Logic   │
│  7 │ Partition inestable pierde orden          │ Usar stablePartition si importa el orden  │ Logic   │
│  8 │ Filter crea slice con cap grande heredado│ slices.Clip despues de filtrar si la       │ Memory  │
│    │                                          │ diferencia es significativa               │         │
│  9 │ Pipeline eager aloca muchos intermedios  │ Combinar operaciones en un solo loop       │ Perf    │
│    │                                          │ cuando el rendimiento importa             │         │
│ 10 │ groupBy con map no tiene orden de keys   │ Extraer keys + slices.Sort si necesitas   │ Logic   │
│    │                                          │ orden determinista                        │         │
│ 11 │ slices.Delete no limpia punteros         │ En Go 1.21.0 no limpiaba, fixeado en      │ Memory  │
│    │ (versiones iniciales)                    │ 1.21.5+, 1.22+ siempre limpia            │         │
│ 12 │ Reduce sin caso base para slice vacio   │ Verificar len > 0 o usar initial value    │ Panic   │
└────┴──────────────────────────────────────────┴──────────────────────────────────────────┴─────────┘
```

---

## 16. Ejercicios de prediccion

**Ejercicio 1**: ¿Que imprime?

```go
s := []int{5, 3, 1, 4, 2}
slices.Sort(s)
idx, found := slices.BinarySearch(s, 3)
fmt.Println(idx, found)
```

<details>
<summary>Respuesta</summary>

```
2 true
```

`slices.Sort` ordena in-place: `[1, 2, 3, 4, 5]`. `BinarySearch(s, 3)` encuentra 3 en el indice 2.
</details>

**Ejercicio 2**: ¿Que imprime?

```go
s := []int{5, 3, 1, 4, 2}
idx, found := slices.BinarySearch(s, 3)
fmt.Println(idx, found)
```

<details>
<summary>Respuesta</summary>

```
// Resultado impredecible — posiblemente 1 false o similar
```

`BinarySearch` requiere un slice **ordenado**. En un slice desordenado, el algoritmo compara incorrectamente y puede retornar cualquier indice con `found = false`. El resultado es indeterminado pero nunca panic.
</details>

**Ejercicio 3**: ¿Que imprime?

```go
a := []string{"a", "b", "c", "d"}
b := []string{"b", "d", "e"}

var inter []string
i, j := 0, 0
for i < len(a) && j < len(b) {
    switch cmp.Compare(a[i], b[j]) {
    case -1:
        i++
    case 1:
        j++
    case 0:
        inter = append(inter, a[i])
        i++
        j++
    }
}
fmt.Println(inter)
```

<details>
<summary>Respuesta</summary>

```
[b d]
```

Interseccion de dos slices ordenados usando dos punteros. `b` y `d` estan en ambos slices.
</details>

**Ejercicio 4**: ¿Que imprime?

```go
s := []int{3, 1, 4, 1, 5, 9, 2, 6}
slices.Sort(s)
s = slices.Compact(s)
fmt.Println(s)
```

<details>
<summary>Respuesta</summary>

```
[1 2 3 4 5 6 9]
```

`Sort` ordena: `[1, 1, 2, 3, 4, 5, 6, 9]`. `Compact` elimina duplicados **consecutivos**: el segundo `1` se elimina. Resultado: `[1, 2, 3, 4, 5, 6, 9]`.
</details>

**Ejercicio 5**: ¿Que imprime?

```go
s := []int{1, 2, 3, 4, 5}
idx := partition(s, func(n int) bool { return n%2 == 0 })
fmt.Println(s[:idx])
fmt.Println(s[idx:])
```

<details>
<summary>Respuesta</summary>

```
[2 4]
[3 1 5]
```

`partition` mueve los pares al inicio. El indice de particion es 2 (dos pares: 2 y 4). Los impares quedan en `s[2:]`. El orden exacto dentro de cada particion puede variar, pero habra 2 pares en `s[:2]` y 3 impares en `s[2:]`.
</details>

**Ejercicio 6**: ¿Que imprime?

```go
s := []string{"web01", "web02", "web03", "web04"}
rotateLeft(s, 2)
fmt.Println(s)
```

<details>
<summary>Respuesta</summary>

```
[web03 web04 web01 web02]
```

`rotateLeft` con k=2: los primeros 2 elementos se mueven al final. El algoritmo de 3 reversiones: reverse `[web01,web02]` → `[web02,web01]`, reverse `[web03,web04]` → `[web04,web03]`, reverse todo → `[web03,web04,web01,web02]`.
</details>

**Ejercicio 7**: ¿Que imprime?

```go
type S struct {
    Name string
    Val  int
}
items := []S{{"a", 2}, {"b", 1}, {"c", 2}, {"d", 1}}
slices.SortStableFunc(items, func(a, b S) int {
    return cmp.Compare(a.Val, b.Val)
})
names := make([]string, len(items))
for i, item := range items {
    names[i] = item.Name
}
fmt.Println(names)
```

<details>
<summary>Respuesta</summary>

```
[b d a c]
```

Sort **estable** por `Val`. Los elementos con `Val=1` son `b` y `d` (en ese orden original). Los de `Val=2` son `a` y `c`. El sort estable preserva el orden relativo de iguales: `[b, d, a, c]`.
</details>

**Ejercicio 8**: ¿Que imprime?

```go
sorted := []int{10, 20, 30, 40, 50}
idx, found := slices.BinarySearch(sorted, 25)
fmt.Println(idx, found)
sorted = slices.Insert(sorted, idx, 25)
fmt.Println(sorted)
```

<details>
<summary>Respuesta</summary>

```
2 false
[10 20 25 30 40 50]
```

`BinarySearch(sorted, 25)` no encuentra 25. Retorna indice 2 (punto de insercion: entre 20 y 30) y `found=false`. `Insert` en posicion 2 produce `[10, 20, 25, 30, 40, 50]` — slice sigue ordenado.
</details>

**Ejercicio 9**: ¿Que imprime?

```go
hosts := []string{"web01", "dead01", "web02", "dead02", "db01"}
hosts = slices.DeleteFunc(hosts, func(h string) bool {
    return strings.HasPrefix(h, "dead")
})
fmt.Println(hosts)
```

<details>
<summary>Respuesta</summary>

```
[web01 web02 db01]
```

`DeleteFunc` elimina todos los elementos que cumplen el predicado (los que empiezan con "dead"). El orden de los restantes se preserva.
</details>

**Ejercicio 10**: ¿Que imprime?

```go
nested := [][]int{{1, 2}, {3}, {4, 5, 6}, {}}
result := flatten(nested)
fmt.Println(result, len(result))
```

<details>
<summary>Respuesta</summary>

```
[1 2 3 4 5 6] 6
```

`flatten` concatena todos los sub-slices. El slice vacio `{}` no aporta elementos. Total: 2+1+3+0 = 6 elementos.
</details>

---

## 17. Resumen visual

```
┌──────────────────────────────────────────────────────────────┐
│              PATRONES CON SLICES — RESUMEN                   │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ORDENAMIENTO                                                │
│  ├─ sort.Slice(s, less)           — clasico, closure         │
│  ├─ slices.SortFunc(s, cmp)       — moderno, generics        │
│  ├─ slices.SortStableFunc(s, cmp) — preserva orden de =      │
│  ├─ Multi-key: if c := cmp(a.X, b.X); c != 0 { return c }  │
│  ├─ sort.Interface para tipos que se ordenan frecuentemente  │
│  └─ Comparador configurable en runtime con buildComparator   │
│                                                              │
│  BUSQUEDA                                                    │
│  ├─ slices.BinarySearch    — O(log n), retorna (idx, found)  │
│  ├─ slices.BinarySearchFunc — para tipos custom              │
│  ├─ Sorted insert: BinarySearch + slices.Insert              │
│  └─ < 20 items → Contains; >= 20 sorted → BinarySearch      │
│                                                              │
│  OPERACIONES DE CONJUNTO (sorted slices, dos punteros)       │
│  ├─ Union:        A ∪ B  — merge sin duplicados              │
│  ├─ Interseccion: A ∩ B  — solo los comunes                  │
│  ├─ Diferencia:   A - B  — en A pero no en B                 │
│  └─ Todas son O(n+m) con dos punteros                        │
│                                                              │
│  TECNICAS ALGORITMICAS                                       │
│  ├─ Partition: separar por predicado (estable / no estable)  │
│  ├─ Rotate: desplazamiento circular (3 reversiones)          │
│  ├─ Flatten: [][]T → []T                                     │
│  ├─ Zip/Unzip: combinar/separar pares                        │
│  ├─ GroupBy: agrupar por clave → map[K][]T                   │
│  ├─ Reduce: acumular → R                                     │
│  ├─ Map: transformar []T → []R                               │
│  └─ Compact: dedup consecutivos (sorted + Compact)           │
│                                                              │
│  PIPELINE FUNCIONAL                                          │
│  From(items).Filter(pred).Sort(cmp).Take(n).Result()         │
│  ├─ Eager (aloca intermedios) — suficiente en Go             │
│  ├─ Map requiere funcion libre (limitacion de generics)      │
│  └─ Para hot paths: combinar en un solo loop                 │
│                                                              │
│  slices PACKAGE (Go 1.21+)                                   │
│  ├─ Sort/SortFunc/SortStableFunc                             │
│  ├─ BinarySearch/BinarySearchFunc                            │
│  ├─ Contains/ContainsFunc/Index/IndexFunc                    │
│  ├─ Delete/DeleteFunc/Insert/Replace                         │
│  ├─ Clone/Compact/CompactFunc/Reverse                        │
│  ├─ Min/Max/MinFunc/MaxFunc                                  │
│  ├─ Grow/Clip                                                │
│  └─ Equal/Compare/EqualFunc                                  │
│                                                              │
│  COMPARACION                                                 │
│  Go:   verbose pero claro, eager, generics limitados         │
│  C:    qsort/bsearch con void*, sin type safety              │
│  Rust: .iter().filter().map().collect() — lazy, zero-cost    │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: S02/T01 - Maps — make, insercion, acceso (comma ok idiom), delete, iteracion (orden no determinista)
