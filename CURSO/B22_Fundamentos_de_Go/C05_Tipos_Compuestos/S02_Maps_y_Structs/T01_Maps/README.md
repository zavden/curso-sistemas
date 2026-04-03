# Maps — Tablas Hash, comma ok idiom, Iteracion y Patrones de Uso

## 1. Introduccion

Los maps son la **estructura de datos asociativa** de Go — tablas hash que mapean claves a valores. Son la forma idomiatica de implementar diccionarios, conjuntos, caches, indices, contadores, y cualquier lookup por clave en O(1) amortizado.

A diferencia de los slices (secuencias ordenadas), los maps son **colecciones no ordenadas** — la iteracion sobre un map no garantiza ningun orden, y Go aleatoriza deliberadamente el orden de iteracion para prevenir que el codigo dependa de un orden accidental.

En SysAdmin/DevOps, los maps son omnipresentes: configuraciones clave-valor, variables de entorno, labels de Kubernetes, headers HTTP, conteo de eventos, inventario de hosts por role, cache de DNS, registros de servicios, y dispatch tables de comandos.

```
┌──────────────────────────────────────────────────────────────┐
│              MAPS EN GO                                      │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Estructura:                                                 │
│  map[K]V — K debe ser comparable, V puede ser cualquier tipo │
│                                                              │
│  ┌─────────────────────────────────────────────┐             │
│  │         Hash Table                          │             │
│  │  ┌──────┬──────────────────┐                │             │
│  │  │ key1 │ → value1         │   bucket 0     │             │
│  │  ├──────┼──────────────────┤                │             │
│  │  │ key2 │ → value2         │   bucket 1     │             │
│  │  ├──────┼──────────────────┤                │             │
│  │  │ key3 │ → value3         │   bucket 2     │             │
│  │  ├──────┼──────────────────┤                │             │
│  │  │ ...  │ → ...            │   ...          │             │
│  │  └──────┴──────────────────┘                │             │
│  └─────────────────────────────────────────────┘             │
│                                                              │
│  Propiedades:                                                │
│  ├─ Referencia interna (puntero a hash table)                │
│  ├─ Zero value es nil (no funcional — panic en write)        │
│  ├─ Lookup, insert, delete: O(1) amortizado                  │
│  ├─ Iteracion: orden NO determinista (aleatorizado)          │
│  ├─ NO thread-safe (data race si acceso concurrente)         │
│  ├─ Keys deben ser comparable (== debe funcionar)            │
│  └─ No comparable: map == map es error de compilacion        │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Creacion de maps

### Literal

```go
// Map literal — la forma mas comun para datos conocidos en compile time
config := map[string]string{
    "host":     "10.0.1.1",
    "port":     "8080",
    "protocol": "https",
    "timeout":  "30s",
}

// Ultimo elemento tambien necesita coma (trailing comma obligatoria)
ports := map[string]int{
    "http":  80,
    "https": 443,
    "ssh":   22,
    "dns":   53, // ← coma obligatoria si cierra en linea separada
}

// Map vacio (no nil):
empty := map[string]int{}
fmt.Println(empty == nil) // false
fmt.Println(len(empty))   // 0
```

### make()

```go
// make(map[K]V) — crea map vacio, listo para usar
m := make(map[string]int)
m["web01"] = 80
m["db01"] = 5432

// make(map[K]V, hint) — pre-alocar capacidad (hint, no garantia)
// Util cuando conoces el tamano aproximado: evita rehashing
m2 := make(map[string]int, 100) // Optimiza para ~100 entradas
// No hay funcion cap() para maps — el hint es opaco

// Cuando usar make con hint:
// ✓ Cuando vas a llenar el map en un loop con tamano conocido
lines := readLines("/etc/hosts")
hosts := make(map[string]string, len(lines))
for _, line := range lines {
    ip, name := parseLine(line)
    hosts[name] = ip
}
```

### Zero value: nil map

```go
// El zero value de un map es nil
var m map[string]int
fmt.Println(m == nil) // true
fmt.Println(len(m))   // 0 — funciona en nil map

// Lectura de nil map: retorna zero value del tipo V (no panic)
fmt.Println(m["key"]) // 0 — zero value de int

// ✗ Escritura en nil map: PANIC
// m["key"] = 1 // panic: assignment to entry in nil map

// ✗ Este es uno de los panics mas comunes en Go
// Ocurre frecuentemente en struct fields no inicializados:
type Service struct {
    Labels map[string]string
}
svc := Service{} // svc.Labels es nil
// svc.Labels["app"] = "web" // panic!

// ✓ Siempre inicializar antes de escribir:
svc.Labels = make(map[string]string)
svc.Labels["app"] = "web" // OK

// ✓ O en el constructor:
func NewService() *Service {
    return &Service{
        Labels: make(map[string]string),
    }
}
```

```
┌──────────────────────────────────────────────────────────────┐
│  nil map vs empty map                                        │
│                                                              │
│  var m map[string]int          →  nil    (no inicializado)   │
│  m := map[string]int{}        →  empty  (inicializado)      │
│  m := make(map[string]int)    →  empty  (inicializado)      │
│                                                              │
│  nil map:                                                    │
│  ├─ len(m) → 0                   ✓                           │
│  ├─ m["key"] → 0 (zero value)    ✓                           │
│  ├─ for range m { } → no itera   ✓                           │
│  ├─ delete(m, "key") → no-op     ✓                           │
│  └─ m["key"] = 1 → PANIC         ✗                           │
│                                                              │
│  empty map:                                                  │
│  ├─ Todas las operaciones funcionan                          │
│  └─ m["key"] = 1 → OK            ✓                           │
│                                                              │
│  Resumen: nil map es read-only + delete, panic en write      │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 3. Operaciones basicas: insert, read, delete

### Insertar y actualizar

```go
m := make(map[string]int)

// Insertar — si la key no existe, la crea
m["web01"] = 80
m["db01"] = 5432
m["ssh"] = 22

// Actualizar — si la key ya existe, sobreescribe
m["web01"] = 8080 // Antes era 80, ahora 8080

// No hay metodo "insert if not exists" nativo
// Patron manual:
if _, exists := m["web01"]; !exists {
    m["web01"] = 80
}

// len() retorna el numero de entradas
fmt.Println(len(m)) // 3
```

### Leer: el comma ok idiom

```go
ports := map[string]int{
    "http":  80,
    "https": 443,
    "ssh":   22,
}

// Lectura simple — retorna zero value si no existe
port := ports["http"]  // 80
port2 := ports["ftp"]  // 0 (zero value de int)
// ¿ftp no existe o su puerto es 0? Ambiguedad

// ✓ Comma ok idiom — distinguir "no existe" de "existe con zero value"
port, ok := ports["http"]
fmt.Println(port, ok) // 80 true

port, ok = ports["ftp"]
fmt.Println(port, ok) // 0 false — no existe

// Patron idiomatico: verificar existencia
if port, ok := ports["https"]; ok {
    fmt.Printf("HTTPS on port %d\n", port)
} else {
    fmt.Println("HTTPS not configured")
}
// port no es visible fuera del if (scope del init statement)

// Solo verificar existencia (descartar valor):
if _, ok := ports["ssh"]; ok {
    fmt.Println("SSH is configured")
}
```

### El comma ok idiom en detalle

```go
// Go usa comma ok en varias situaciones:
// 1. Maps:         val, ok := m[key]
// 2. Type assert:  val, ok := iface.(Type)
// 3. Channel recv: val, ok := <-ch
// 4. Range map:    for k, v := range m

// En maps, el segundo valor (ok) indica:
// true  → la key existe en el map
// false → la key NO existe, val es el zero value del tipo

// Esto es critico para tipos donde el zero value es un valor valido:
counts := map[string]int{"errors": 0, "warnings": 5}

// ✗ Sin comma ok:
if counts["errors"] == 0 {
    // ¿No hay errores, o la key no existe?
    // Ambos casos producen 0
}

// ✓ Con comma ok:
if count, ok := counts["errors"]; ok {
    fmt.Printf("Errors: %d\n", count) // "Errors: 0" — explicitamente 0
} else {
    fmt.Println("Error counter not initialized")
}
```

### Delete

```go
m := map[string]int{
    "web01": 80,
    "db01":  5432,
    "old":   9999,
}

// delete(map, key) — elimina la entrada
delete(m, "old")
fmt.Println(len(m)) // 2

// delete de key que no existe — no hace nada, no panic
delete(m, "nonexistent") // OK, no-op

// delete en nil map — no panic (a diferencia de write)
var nilMap map[string]int
delete(nilMap, "key") // OK, no-op

// No hay "delete and return" nativo
// Patron manual:
func pop(m map[string]int, key string) (int, bool) {
    val, ok := m[key]
    if ok {
        delete(m, key)
    }
    return val, ok
}
```

### clear (Go 1.21+)

```go
// clear() vacia un map completamente (elimina todas las entradas)
m := map[string]int{"a": 1, "b": 2, "c": 3}
clear(m)
fmt.Println(len(m)) // 0
fmt.Println(m)       // map[]

// clear no pone el map a nil — sigue siendo usable
m["d"] = 4 // OK

// Pre-Go 1.21: borrar manualmente
for k := range m {
    delete(m, k) // Seguro: delete durante range es valido
}
```

---

## 4. Tipos de key: restriccion comparable

### Que tipos pueden ser keys

```go
// La restriccion: K debe satisfacer `comparable`
// comparable = tipos que soportan == y !=

// ✓ Tipos validos como key:
map[int]string{}         // Enteros
map[string]int{}         // Strings
map[float64]bool{}       // Flotantes (¡cuidado con NaN!)
map[bool]string{}        // Booleanos
map[rune]int{}           // Runes (alias de int32)
map[[4]byte]string{}     // Arrays de tamaño fijo
map[struct{X,Y int}]bool{} // Structs (si todos los campos son comparables)
map[*int]string{}        // Punteros (compara direccion, no valor)
map[interface{}]int{}    // Interfaces (compara tipo + valor dinamico)

// ✗ Tipos NO validos como key:
// map[[]int]string{}       // Slices — no comparables
// map[map[string]int]bool{} // Maps — no comparables
// map[func()]int{}          // Funciones — no comparables
```

### Struct como key — patron poderoso

```go
// Struct keys son utiles para claves compuestas
type Endpoint struct {
    Host string
    Port int
}

connections := map[Endpoint]int{
    {"web01", 80}:   150,
    {"web01", 443}:  230,
    {"db01", 5432}:  45,
}

// Lookup:
count := connections[Endpoint{"web01", 80}]
fmt.Println(count) // 150

// Muy util para:
// - Contadores multi-dimension (host+metric)
// - Cache con clave compuesta (user+resource)
// - Grafos (edge = {from, to})
// - Indices multi-columna

type MetricKey struct {
    Host   string
    Metric string
}

metrics := make(map[MetricKey]float64)
metrics[MetricKey{"web01", "cpu"}] = 72.5
metrics[MetricKey{"web01", "mem"}] = 85.0
metrics[MetricKey{"db01", "cpu"}] = 30.2
```

### Arrays como key (slices no, arrays si)

```go
// Arrays son comparables → validos como key
type IPv4 = [4]byte

dnsCache := map[IPv4]string{
    {10, 0, 1, 1}:  "web01.internal",
    {10, 0, 2, 1}:  "db01.internal",
    {10, 0, 3, 1}:  "cache01.internal",
}

name := dnsCache[IPv4{10, 0, 1, 1}]
fmt.Println(name) // "web01.internal"

// MAC address como key:
type MAC = [6]byte
arpTable := map[MAC]IPv4{
    {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x01}: {10, 0, 1, 1},
}
```

### Cuidado con float keys y NaN

```go
import "math"

m := map[float64]string{}
m[1.0] = "one"
m[math.NaN()] = "nan1"
m[math.NaN()] = "nan2"

fmt.Println(len(m)) // 4 (!)

// NaN != NaN en IEEE 754
// Cada NaN crea una nueva entrada que NUNCA puedes recuperar
// Porque NaN == NaN es false, el lookup siempre falla
fmt.Println(m[math.NaN()]) // "" — no encontrado

// Regla: NUNCA usar float como key en maps
// Si necesitas, convierte a int/string:
// key := fmt.Sprintf("%.2f", value)
// key := int64(value * 100) // Para 2 decimales
```

---

## 5. Iteracion: range y orden no determinista

### range basico

```go
config := map[string]string{
    "host":    "10.0.1.1",
    "port":    "8080",
    "timeout": "30s",
}

// Iterar sobre key-value
for key, value := range config {
    fmt.Printf("  %s = %s\n", key, value)
}
// El orden es DIFERENTE en cada ejecucion del programa
// Go aleatoriza deliberadamente el orden de iteracion

// Solo keys:
for key := range config {
    fmt.Println(key)
}

// Solo values (descartar key):
for _, value := range config {
    fmt.Println(value)
}
```

### Por que el orden es aleatorio

```go
// Go aleatoriza el orden de iteracion desde Go 1.0
// Razon: prevenir que el codigo dependa de un orden accidental
// de la implementacion interna de la hash table.
//
// En otros lenguajes (Python < 3.7, Java HashMap):
// El orden podia parecer estable en tests pero cambiar
// con diferentes datos, versiones, o plataformas.
// Go elimina este bug por diseno.
//
// Si necesitas orden determinista, ordena las keys primero:

// Patron: iteracion ordenada
keys := make([]string, 0, len(config))
for k := range config {
    keys = append(keys, k)
}
sort.Strings(keys) // o slices.Sort(keys)

for _, k := range keys {
    fmt.Printf("  %s = %s\n", k, config[k])
}
// Ahora el output es siempre el mismo

// Go 1.21+ con maps.Keys:
import "maps"

keys = slices.Sorted(maps.Keys(config))
for _, k := range keys {
    fmt.Printf("  %s = %s\n", k, config[k])
}
```

### Modificar map durante iteracion

```go
// ✓ Es SEGURO agregar y eliminar durante range
m := map[string]int{
    "a": 1, "b": 2, "c": 3, "d": 4,
}

for k, v := range m {
    if v%2 == 0 {
        delete(m, k) // ✓ Seguro: delete durante range
    }
}
fmt.Println(m) // Solo impares: map[a:1 c:3]

// PERO: elementos agregados durante range pueden o no ser visitados
// La especificacion dice: "If a map entry that has not yet been reached
// is removed during iteration, the corresponding iteration value
// will not be produced. If a map entry is created during iteration,
// that entry may be produced during the iteration or may be skipped."

for k := range m {
    m[k+"_copy"] = m[k] // Los nuevos pueden o no aparecer en este range
}
// Resultado no determinista — evitar agregar durante range

// ✓ Patron seguro: recolectar keys a eliminar, luego eliminar
var toDelete []string
for k, v := range m {
    if shouldDelete(v) {
        toDelete = append(toDelete, k)
    }
}
for _, k := range toDelete {
    delete(m, k)
}
```

---

## 6. Maps son referencias

### El map header es un puntero

```go
// Un map es internamente un puntero a una estructura runtime.hmap
// Pasar un map a una funcion NO copia los datos — comparte la tabla hash
// (A diferencia de slices donde se copia el header de 24 bytes)

func addEntry(m map[string]int, key string, val int) {
    m[key] = val // Modifica el map del caller
}

original := map[string]int{"a": 1}
addEntry(original, "b", 2)
fmt.Println(original) // map[a:1 b:2] — modificado

// Esto es diferente de slices:
// - Slice: pasar copia del header (ptr, len, cap)
//   → func puede modificar elementos pero no el header del caller
//   → append puede no ser visible
// - Map: pasar copia del puntero a hmap
//   → func puede hacer TODO: insert, delete, update
//   → cambios siempre visibles en el caller

// PERO: reasignar el map local no afecta al caller
func replaceMap(m map[string]int) {
    m = map[string]int{"x": 99} // Reasigna la variable local
    // El caller no ve este cambio
}

replaceMap(original)
fmt.Println(original) // map[a:1 b:2] — no afectado
```

### Sizeof de un map

```go
import "unsafe"

var m map[string]int
fmt.Println(unsafe.Sizeof(m)) // 8 — es un puntero (64-bit)

// El map variable es solo un puntero de 8 bytes
// Los datos (buckets, keys, values) estan en el heap
// El tamaño real depende del numero de entradas
```

---

## 7. Patrones fundamentales con maps

### Patron 1: Set (conjunto)

```go
// Go no tiene un tipo set nativo
// Patron: map[T]bool o map[T]struct{}

// Con bool — mas legible:
allowed := map[string]bool{
    "web01": true,
    "web02": true,
    "db01":  true,
}

if allowed["web01"] {
    fmt.Println("Access granted")
}
// Para keys que no existen, m["unknown"] retorna false — funciona como set

// Con struct{} — mas eficiente (0 bytes por value):
type Set[T comparable] map[T]struct{}

func NewSet[T comparable](items ...T) Set[T] {
    s := make(Set[T], len(items))
    for _, item := range items {
        s[item] = struct{}{}
    }
    return s
}

func (s Set[T]) Add(item T) {
    s[item] = struct{}{}
}

func (s Set[T]) Has(item T) bool {
    _, ok := s[item]
    return ok
}

func (s Set[T]) Remove(item T) {
    delete(s, item)
}

func (s Set[T]) Len() int {
    return len(s)
}

// Uso:
hosts := NewSet("web01", "web02", "db01")
hosts.Add("cache01")
fmt.Println(hosts.Has("web01")) // true
fmt.Println(hosts.Has("web99")) // false
hosts.Remove("db01")

// ¿bool o struct{}?
// - map[T]bool:     +legible, if m[k] funciona directo, 1 byte/entry
// - map[T]struct{}: +eficiente (0 bytes/entry), requiere _, ok := m[k]
// Para < 10,000 entries la diferencia es negligible — prefiere bool
```

### Patron 2: Contador / Histogram

```go
// Contar ocurrencias de cada valor
func countWords(text string) map[string]int {
    counts := make(map[string]int)
    for _, word := range strings.Fields(text) {
        counts[word]++ // Zero value de int es 0, asi que 0++ = 1
    }
    return counts
}

// Contar errores por tipo en logs
func countErrors(logs []string) map[string]int {
    counts := make(map[string]int)
    for _, line := range logs {
        if level := extractLevel(line); level == "ERROR" {
            errType := extractErrorType(line)
            counts[errType]++
        }
    }
    return counts
}

// Top N valores
func topN(counts map[string]int, n int) []string {
    type entry struct {
        key   string
        count int
    }
    entries := make([]entry, 0, len(counts))
    for k, v := range counts {
        entries = append(entries, entry{k, v})
    }
    slices.SortFunc(entries, func(a, b entry) int {
        return cmp.Compare(b.count, a.count) // Descendente
    })
    if n > len(entries) {
        n = len(entries)
    }
    result := make([]string, n)
    for i := 0; i < n; i++ {
        result[i] = entries[i].key
    }
    return result
}
```

### Patron 3: Agrupacion (GroupBy)

```go
type Server struct {
    Name   string
    Role   string
    Region string
    Status string
}

// Agrupar servidores por role
func groupByRole(servers []Server) map[string][]Server {
    groups := make(map[string][]Server)
    for _, s := range servers {
        groups[s.Role] = append(groups[s.Role], s)
    }
    return groups
}

// Agrupar por multiples criterios: role + region
type GroupKey struct {
    Role   string
    Region string
}

func groupByRoleAndRegion(servers []Server) map[GroupKey][]Server {
    groups := make(map[GroupKey][]Server)
    for _, s := range servers {
        key := GroupKey{s.Role, s.Region}
        groups[key] = append(groups[key], s)
    }
    return groups
}
```

### Patron 4: Cache / Memoize

```go
// Cache simple de DNS lookups
type DNSCache struct {
    entries map[string]string // hostname → IP
}

func NewDNSCache() *DNSCache {
    return &DNSCache{
        entries: make(map[string]string),
    }
}

func (c *DNSCache) Resolve(hostname string) (string, error) {
    // Cache hit
    if ip, ok := c.entries[hostname]; ok {
        return ip, nil
    }
    
    // Cache miss — resolver y almacenar
    ip, err := net.LookupHost(hostname)
    if err != nil {
        return "", err
    }
    c.entries[hostname] = ip[0]
    return ip[0], nil
}

// Memoize generico
func memoize[K comparable, V any](fn func(K) V) func(K) V {
    cache := make(map[K]V)
    return func(key K) V {
        if val, ok := cache[key]; ok {
            return val
        }
        val := fn(key)
        cache[key] = val
        return val
    }
}

// Uso:
expensiveLookup := memoize(func(host string) string {
    // Simular lookup lento
    time.Sleep(100 * time.Millisecond)
    return lookupIP(host)
})
ip1 := expensiveLookup("web01") // Lento — primera vez
ip2 := expensiveLookup("web01") // Instantaneo — cache hit
```

### Patron 5: Indice inverso

```go
// Dado un map K→V, construir V→K
func invertMap[K, V comparable](m map[K]V) map[V]K {
    inv := make(map[V]K, len(m))
    for k, v := range m {
        inv[v] = k // Si hay duplicados en V, el ultimo gana
    }
    return inv
}

portToService := map[int]string{
    80:   "http",
    443:  "https",
    22:   "ssh",
    5432: "postgres",
}

serviceToPort := invertMap(portToService)
fmt.Println(serviceToPort["ssh"]) // 22

// Indice inverso con multiples valores
func invertMulti[K comparable, V comparable](m map[K]V) map[V][]K {
    inv := make(map[V][]K)
    for k, v := range m {
        inv[v] = append(inv[v], k)
    }
    return inv
}

hostsByRole := map[string]string{
    "web01": "web",
    "web02": "web",
    "db01":  "database",
    "db02":  "database",
}
roleToHosts := invertMulti(hostsByRole)
// roleToHosts["web"] = ["web01", "web02"]
// roleToHosts["database"] = ["db01", "db02"]
```

### Patron 6: Default value

```go
// getOrDefault: retornar valor o un default si no existe
func getOrDefault[K comparable, V any](m map[K]V, key K, defaultVal V) V {
    if val, ok := m[key]; ok {
        return val
    }
    return defaultVal
}

config := map[string]string{
    "host": "10.0.1.1",
}
host := getOrDefault(config, "host", "localhost")     // "10.0.1.1"
port := getOrDefault(config, "port", "8080")           // "8080" (default)
timeout := getOrDefault(config, "timeout", "30s")      // "30s" (default)

// getOrInsert: retornar valor existente o insertar y retornar default
func getOrInsert[K comparable, V any](m map[K]V, key K, defaultVal V) V {
    if val, ok := m[key]; ok {
        return val
    }
    m[key] = defaultVal
    return defaultVal
}
```

### Patron 7: Dispatch table

```go
// Map de string a funcion — para CLI commands, HTTP routing, event handling
type CommandFunc func(args []string) error

commands := map[string]CommandFunc{
    "status": func(args []string) error {
        fmt.Println("Service is running")
        return nil
    },
    "restart": func(args []string) error {
        fmt.Println("Restarting service...")
        return restartService()
    },
    "logs": func(args []string) error {
        n := 100
        if len(args) > 0 {
            n, _ = strconv.Atoi(args[0])
        }
        return showLogs(n)
    },
}

func executeCommand(name string, args []string) error {
    cmd, ok := commands[name]
    if !ok {
        return fmt.Errorf("unknown command: %s", name)
    }
    return cmd(args)
}
```

---

## 8. maps package (Go 1.21+)

### Funciones disponibles

```go
import "maps"

m := map[string]int{"a": 1, "b": 2, "c": 3}

// ── Clone: copia superficial (shallow copy) ──
m2 := maps.Clone(m)
m2["d"] = 4
fmt.Println(m)  // map[a:1 b:2 c:3] — no afectado
fmt.Println(m2) // map[a:1 b:2 c:3 d:4]

// ¡Shallow copy! Si V es un tipo referencia (slice, map, pointer),
// los valores siguen compartidos:
m3 := map[string][]int{"hosts": {1, 2, 3}}
m4 := maps.Clone(m3)
m4["hosts"][0] = 999
fmt.Println(m3["hosts"]) // [999, 2, 3] — ¡compartido!

// ── Equal: comparar dos maps ──
a := map[string]int{"x": 1, "y": 2}
b := map[string]int{"x": 1, "y": 2}
fmt.Println(maps.Equal(a, b)) // true

c := map[string]int{"x": 1}
fmt.Println(maps.Equal(a, c)) // false

// ── EqualFunc: comparar con funcion custom ──
m5 := map[string]string{"host": "WEB01"}
m6 := map[string]string{"host": "web01"}
equal := maps.EqualFunc(m5, m6, func(a, b string) bool {
    return strings.EqualFold(a, b) // Case-insensitive
})
fmt.Println(equal) // true

// ── DeleteFunc: eliminar entries por predicado ──
m7 := map[string]int{"a": 1, "b": 2, "c": 3, "d": 4}
maps.DeleteFunc(m7, func(k string, v int) bool {
    return v%2 == 0 // Eliminar pares
})
fmt.Println(m7) // map[a:1 c:3]

// ── Copy: copiar entries de src a dst (sobreescribe) ──
dst := map[string]int{"a": 10, "b": 20}
src := map[string]int{"b": 200, "c": 300}
maps.Copy(dst, src) // src entries sobrescriben dst
fmt.Println(dst) // map[a:10 b:200 c:300]

// ── Keys: obtener todas las keys (Go 1.23 iterator) ──
// maps.Keys retorna un iterator (no un slice)
// Para obtener slice ordenado:
keys := slices.Sorted(maps.Keys(m))

// ── Values: obtener todos los values (Go 1.23 iterator) ──
vals := slices.Collect(maps.Values(m))

// ── Insert: insertar desde iterator (Go 1.23) ──
// maps.Insert(m, maps.All(otherMap))

// ── Collect: construir map desde iterator (Go 1.23) ──
// m := maps.Collect(someIterator)
```

### Antes de Go 1.21: operaciones manuales

```go
// Clone manual
func cloneMap[K comparable, V any](m map[K]V) map[K]V {
    result := make(map[K]V, len(m))
    for k, v := range m {
        result[k] = v
    }
    return result
}

// Equal manual
func equalMaps[K, V comparable](a, b map[K]V) bool {
    if len(a) != len(b) {
        return false
    }
    for k, va := range a {
        vb, ok := b[k]
        if !ok || va != vb {
            return false
        }
    }
    return true
}

// Merge manual
func mergeMaps[K comparable, V any](dst, src map[K]V) {
    for k, v := range src {
        dst[k] = v
    }
}
```

---

## 9. Map de maps y map de slices

### Map de slices: el patron mas comun

```go
// Agrupar items por key
type Event struct {
    Host  string
    Level string
    Msg   string
}

func groupEvents(events []Event) map[string][]Event {
    groups := make(map[string][]Event)
    for _, e := range events {
        // append funciona en nil slice — no necesitas inicializar
        groups[e.Host] = append(groups[e.Host], e)
    }
    return groups
}

// ¿Por que funciona sin inicializar?
// groups[e.Host] retorna nil (zero value de []Event) si no existe
// append(nil, e) retorna []Event{e} — crea un slice nuevo
// El resultado se asigna de vuelta al map
```

### Map de maps: requiere inicializacion

```go
// Map anidado: region → role → []hosts
type Inventory map[string]map[string][]string

func buildInventory(hosts []Server) Inventory {
    inv := make(Inventory)
    for _, h := range hosts {
        // ✗ Esto falla si inv[h.Region] es nil:
        // inv[h.Region][h.Role] = append(...) // panic!
        
        // ✓ Inicializar el map interno si no existe
        if inv[h.Region] == nil {
            inv[h.Region] = make(map[string][]string)
        }
        inv[h.Region][h.Role] = append(inv[h.Region][h.Role], h.Name)
    }
    return inv
}

// Acceso seguro a map anidado:
func getHosts(inv Inventory, region, role string) []string {
    regionMap, ok := inv[region]
    if !ok {
        return nil
    }
    return regionMap[role] // nil si no existe
}

// Alternativa: usar struct key en vez de map anidado
type LocationKey struct {
    Region string
    Role   string
}

// Mas simple, mas rapido, sin riesgo de nil inner map:
flat := make(map[LocationKey][]string)
for _, h := range hosts {
    key := LocationKey{h.Region, h.Role}
    flat[key] = append(flat[key], h.Name)
}
```

### Regla: preferir struct key sobre map anidado

```
┌──────────────────────────────────────────────────────────────┐
│  map[string]map[string]V vs map[StructKey]V                  │
│                                                              │
│  Map anidado:                                                │
│  ├─ Requiere inicializar cada nivel                          │
│  ├─ Acceso: 2 lookups + nil check                            │
│  ├─ Mas memoria (overhead de cada map interno)               │
│  └─ Mas propenso a bugs (nil inner map)                      │
│                                                              │
│  Struct key:                                                 │
│  ├─ Un solo make, sin nil checks                             │
│  ├─ Acceso: 1 lookup                                         │
│  ├─ Menos memoria                                            │
│  └─ Type-safe: el compilador valida los campos               │
│                                                              │
│  Excepcion: usa map anidado cuando necesitas iterar           │
│  sobre las keys del primer nivel independientemente.          │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 10. Maps y concurrencia

### Maps NO son thread-safe

```go
// ✗ DATA RACE: leer y escribir concurrentemente produce panic
m := make(map[string]int)

// Writer
go func() {
    for i := 0; ; i++ {
        m[fmt.Sprintf("key-%d", i)] = i // ✗ Write
    }
}()

// Reader
go func() {
    for {
        _ = m["key-0"] // ✗ Read
    }
}()

// Esto produce:
// fatal error: concurrent map read and map write
// (no es un panic normal — es un runtime fatal que no se puede recuperar)
```

### Solucion 1: sync.RWMutex

```go
type SafeMap[K comparable, V any] struct {
    mu sync.RWMutex
    m  map[K]V
}

func NewSafeMap[K comparable, V any]() *SafeMap[K, V] {
    return &SafeMap[K, V]{
        m: make(map[K]V),
    }
}

func (sm *SafeMap[K, V]) Get(key K) (V, bool) {
    sm.mu.RLock() // Read lock — multiples lectores simultaneos
    defer sm.mu.RUnlock()
    val, ok := sm.m[key]
    return val, ok
}

func (sm *SafeMap[K, V]) Set(key K, val V) {
    sm.mu.Lock() // Write lock — exclusivo
    defer sm.mu.Unlock()
    sm.m[key] = val
}

func (sm *SafeMap[K, V]) Delete(key K) {
    sm.mu.Lock()
    defer sm.mu.Unlock()
    delete(sm.m, key)
}

func (sm *SafeMap[K, V]) Len() int {
    sm.mu.RLock()
    defer sm.mu.RUnlock()
    return len(sm.m)
}
```

### Solucion 2: sync.Map (para casos especificos)

```go
// sync.Map esta optimizado para 2 casos:
// 1. Key se escribe una vez y se lee muchas veces (cache inmutable)
// 2. Multiples goroutines leen/escriben keys disjuntas
// Para otros casos, sync.RWMutex es mas rapido

var m sync.Map

// Store
m.Store("web01", "10.0.1.1")
m.Store("db01", "10.0.2.1")

// Load
val, ok := m.Load("web01")
if ok {
    fmt.Println(val.(string)) // Type assertion necesaria (interface{})
}

// LoadOrStore — atómico: carga si existe, almacena si no
actual, loaded := m.LoadOrStore("web01", "10.0.1.99")
fmt.Println(actual, loaded) // "10.0.1.1" true (ya existia)

// Delete
m.Delete("web01")

// Range — iterar (NO se puede usar for range directamente)
m.Range(func(key, value any) bool {
    fmt.Printf("  %s → %s\n", key, value)
    return true // true = continuar, false = break
})

// LoadAndDelete — atomico (Go 1.15+)
val, loaded = m.LoadAndDelete("db01")

// Swap — reemplazar y retornar viejo (Go 1.20+)
prev, loaded := m.Swap("web01", "10.0.1.2")

// CompareAndSwap — CAS (Go 1.20+)
swapped := m.CompareAndSwap("web01", "10.0.1.2", "10.0.1.3")
```

### Cuando usar sync.Map vs sync.RWMutex

```
┌────────────────────────────┬──────────────────────┬──────────────────────┐
│ Escenario                  │ sync.RWMutex + map   │ sync.Map             │
├────────────────────────────┼──────────────────────┼──────────────────────┤
│ Keys conocidas, muchas     │ ✓ Mejor              │                      │
│ lecturas                   │                      │                      │
│ Keys disjuntas por         │                      │ ✓ Mejor              │
│ goroutine                  │                      │                      │
│ Write-once, read-many      │                      │ ✓ Mejor              │
│ (cache inmutable)          │                      │                      │
│ Lecturas y escrituras      │ ✓ Mejor              │                      │
│ equilibradas               │                      │                      │
│ Type safety                │ ✓ Generics           │ ✗ interface{}        │
│ Iteracion completa         │ ✓ for range          │ ✗ Range callback     │
│ len()                      │ ✓ O(1)               │ ✗ No tiene           │
│ General purpose            │ ✓ Recomendado        │ Solo casos optim.    │
└────────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 11. Performance y internals

### Como funciona el hash map de Go

```
┌──────────────────────────────────────────────────────────────┐
│  INTERNALS DE MAP EN GO (runtime/map.go)                     │
│                                                              │
│  Header (hmap):                                              │
│  ├─ count   int    — numero de entradas                      │
│  ├─ B       uint8  — log2(numero de buckets)                 │
│  ├─ buckets *array — puntero al array de buckets             │
│  └─ ...            — overflow, hash seed, etc.               │
│                                                              │
│  Cada bucket:                                                │
│  ├─ tophash [8]uint8  — 8 top-bytes del hash (fast reject)   │
│  ├─ keys    [8]K      — 8 keys contiguas                     │
│  ├─ values  [8]V      — 8 values contiguos                   │
│  └─ overflow *bucket  — linked list para overflow             │
│                                                              │
│  Lookup: hash(key) → bucket → scan tophash → compare key     │
│  Load factor: 6.5 entries/bucket promedio antes de grow       │
│  Grow: duplicar buckets + rehash incremental                  │
│                                                              │
│  Crecimiento es incremental — no "stop the world"            │
│  Los buckets viejos se evacuan gradualmente                   │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Pre-alocacion con make hint

```go
// Sin hint: multiples grows
func buildMapSlow(n int) map[int]int {
    m := make(map[int]int) // Sin hint
    for i := 0; i < n; i++ {
        m[i] = i * i // Provoca grows: 8→16→32→64→...
    }
    return m
}

// Con hint: una sola alocacion
func buildMapFast(n int) map[int]int {
    m := make(map[int]int, n) // Hint de tamaño
    for i := 0; i < n; i++ {
        m[i] = i * i // No crece si n es correcto
    }
    return m
}

// Benchmark tipico:
// BenchmarkSlow-8    n=10000   ~800µs/op    ~600KB alloc
// BenchmarkFast-8    n=10000   ~400µs/op    ~300KB alloc
// ~2x mas rapido con hint correcto
```

### Maps no se encogen

```go
// Una vez que un map crece, NO se reduce automaticamente
// Incluso si eliminas todas las entradas

m := make(map[int]int)
for i := 0; i < 1_000_000; i++ {
    m[i] = i
}
// m ahora usa mucha memoria

for i := 0; i < 1_000_000; i++ {
    delete(m, i)
}
// m tiene 0 entradas PERO la memoria de los buckets
// NO se libera — el map mantiene su tamaño maximo

// ✓ Solucion: crear un nuevo map
m = make(map[int]int) // El viejo m sera recolectado por GC

// ✓ O usar clear (Go 1.21+):
clear(m) // Vacia el map — PUEDE liberar memoria interna
// (la especificacion no garantiza liberacion, pero la
// implementacion actual de Go si la hace con clear)
```

### Costo de hashing por tipo de key

```go
// El rendimiento del map depende del costo de hash(key):
//
// int, uint, float:     Muy rapido — hash trivial
// string corto (<32B):  Rapido — AES hash intrinsics
// string largo:         Proporcional a len(string)
// [N]byte pequeno:      Rapido
// struct:               Hash de cada campo concatenado
// interface{}:          Hash del tipo + hash del valor (mas lento)
//
// Regla: keys pequenas (int, string corta) son las mas eficientes
// Evitar keys que son strings muy largas o structs con muchos campos
```

---

## 12. No puedes tomar la direccion de un map element

### El problema

```go
m := map[string]int{"a": 1, "b": 2}

// ✗ ERROR: no puedes obtener puntero a un valor del map
// ptr := &m["a"]
// error: cannot take address of m["a"]

// ¿Por que?
// Porque el map puede rehashear en cualquier momento (en un insert),
// lo que mueve los valores a nuevos buckets.
// Un puntero a m["a"] podria quedar dangling despues de un insert.

// ✓ Solucion 1: copiar a variable local
val := m["a"]
ptr := &val // OK — apunta a la copia, no al map

// ✓ Solucion 2: usar map de punteros
m2 := map[string]*int{
    "a": new(int),
}
*m2["a"] = 42
ptr2 := m2["a"] // OK — el puntero es el valor, no la direccion interna
```

### Implicacion: no puedes modificar un struct field de un map value

```go
type Config struct {
    Host    string
    Port    int
    Enabled bool
}

configs := map[string]Config{
    "web": {Host: "10.0.1.1", Port: 80, Enabled: true},
}

// ✗ ERROR: no puedes modificar un campo directamente
// configs["web"].Port = 8080
// error: cannot assign to struct field configs["web"].Port in map

// ¿Por que? Porque configs["web"] retorna una COPIA del struct.
// Modificar la copia no afectaria al map.
// Go previene este error en compile time.

// ✓ Solucion 1: copiar, modificar, reasignar
cfg := configs["web"]
cfg.Port = 8080
configs["web"] = cfg

// ✓ Solucion 2: usar map de punteros
configsPtr := map[string]*Config{
    "web": {Host: "10.0.1.1", Port: 80, Enabled: true},
}
configsPtr["web"].Port = 8080 // ✓ OK — modificando el struct via puntero

// ¿Cual preferir?
// - map[K]V:  mas simple, inmutabilidad de valores, copia al acceder
// - map[K]*V: mas eficiente para structs grandes, mutabilidad directa,
//             cuidado con nil pointers y aliasing
```

---

## 13. Comparacion: Go vs C vs Rust

### Go: map built-in, simple, GC-managed

```go
// Map built-in con syntax especial
m := map[string]int{"a": 1, "b": 2}
m["c"] = 3
val, ok := m["a"] // Comma ok idiom
delete(m, "b")
for k, v := range m { ... }

// Propiedades:
// + Extremadamente facil de usar
// + GC maneja la memoria
// + Iteracion randomizada (previene dependencia de orden)
// - No thread-safe
// - No comparable (no puedes hacer m1 == m2 sin maps.Equal)
// - No puedes tomar direccion de valores
// - No se encoge automaticamente
```

### C: sin hash map nativo

```c
// C no tiene hash map. Opciones:
// 1. Implementar desde cero (educativo pero tedioso)
// 2. Usar una libreria: uthash, glib GHashTable, khash

// Con uthash (header-only, popular):
#include "uthash.h"

typedef struct {
    char name[64];       // Key
    int port;            // Value
    UT_hash_handle hh;   // Requerido por uthash
} ServiceEntry;

ServiceEntry *services = NULL;

void add_service(const char *name, int port) {
    ServiceEntry *s = malloc(sizeof(ServiceEntry));
    strncpy(s->name, name, sizeof(s->name));
    s->port = port;
    HASH_ADD_STR(services, name, s);
}

ServiceEntry *find_service(const char *name) {
    ServiceEntry *s;
    HASH_FIND_STR(services, name, s);
    return s; // NULL si no existe
}

// Manual: malloc, free, string handling
// Sin type safety (void* o macros)
// Facil de tener memory leaks y buffer overflows
```

### Rust: HashMap y BTreeMap

```rust
use std::collections::HashMap;

let mut m: HashMap<String, i32> = HashMap::new();
m.insert("a".to_string(), 1);
m.insert("b".to_string(), 2);

// Acceso seguro con Option
match m.get("a") {
    Some(val) => println!("Found: {}", val),
    None => println!("Not found"),
}

// Entry API — mas poderosa que Go's comma ok
m.entry("c".to_string()).or_insert(3);
// Si "c" no existe, inserta 3; si existe, no hace nada

// Counter pattern con entry:
let mut counts: HashMap<String, i32> = HashMap::new();
for word in text.split_whitespace() {
    *counts.entry(word.to_string()).or_insert(0) += 1;
}

// Iteracion (orden no determinista, como Go)
for (key, value) in &m {
    println!("{}: {}", key, value);
}

// BTreeMap: igual pero con orden (como Java TreeMap)
use std::collections::BTreeMap;
let mut ordered: BTreeMap<String, i32> = BTreeMap::new();
// Iteracion siempre en orden de key

// Thread safety: HashMap no es thread-safe
// Opciones: Arc<RwLock<HashMap>>, dashmap (crate)
```

### Tabla comparativa

```
┌──────────────────────┬──────────────────────┬──────────────────────┬──────────────────────┐
│ Aspecto              │ Go map               │ C (uthash/manual)    │ Rust HashMap         │
├──────────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤
│ Built-in             │ Si (keyword)         │ No (libreria)        │ Si (stdlib)          │
│ Type safety          │ Si (generics impl.)  │ No (void*/macros)    │ Si (generics)        │
│ Key constraint       │ comparable           │ Custom hash fn       │ Hash + Eq traits     │
│ Existencia check     │ _, ok := m[k]        │ ptr == NULL          │ .get() → Option      │
│ Insert-if-absent     │ Manual if !ok        │ Manual               │ .entry().or_insert() │
│ Delete               │ delete(m, k)         │ HASH_DEL(m, entry)   │ .remove(&k)          │
│ Iteracion            │ for k,v := range m   │ Loop sobre linked    │ for (k,v) in &m      │
│ Orden de iteracion   │ Random (by design)   │ Depende de impl.     │ Random (HashMap)     │
│                      │                      │                      │ Sorted (BTreeMap)    │
│ Thread safety        │ No (fatal error)     │ No                   │ No (use RwLock)      │
│ Concurrent map       │ sync.Map             │ Manual + mutex       │ dashmap crate        │
│ Comparable           │ No (maps.Equal)      │ Manual               │ Si (PartialEq)       │
│ Memoria              │ GC                   │ Manual free          │ RAII/Drop            │
│ Shrinking            │ No (create new)      │ Manual               │ .shrink_to_fit()     │
│ Address of value     │ No (rehashing)       │ Si (punteros)        │ No (similar razon)   │
│ Ordered variant      │ No builtin           │ Custom tree          │ BTreeMap             │
│ Performance          │ Buena                │ Mejor (tunable)      │ Mejor (SipHash/FxH.) │
└──────────────────────┴──────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 14. Programa completo: Service Registry & Discovery

Un registro de servicios con descubrimiento, health checking, labels, y versionado — demostrando todos los patrones de maps en un sistema SysAdmin real.

```go
package main

import (
	"cmp"
	"fmt"
	"maps"
	"slices"
	"strings"
	"sync"
	"time"
)

// ─── Tipos ──────────────────────────────────────────────────────

type ServiceStatus string

const (
	StatusHealthy   ServiceStatus = "healthy"
	StatusUnhealthy ServiceStatus = "unhealthy"
	StatusUnknown   ServiceStatus = "unknown"
)

type ServiceInstance struct {
	ID       string
	Name     string
	Host     string
	Port     int
	Version  string
	Status   ServiceStatus
	Labels   map[string]string
	LastSeen time.Time
}

func (si ServiceInstance) Addr() string {
	return fmt.Sprintf("%s:%d", si.Host, si.Port)
}

func (si ServiceInstance) String() string {
	labels := make([]string, 0, len(si.Labels))
	for k, v := range si.Labels {
		labels = append(labels, k+"="+v)
	}
	slices.Sort(labels)
	return fmt.Sprintf("%-12s %-18s %-10s v%-8s %s  {%s}",
		si.ID, si.Addr(), si.Status, si.Version,
		si.LastSeen.Format("15:04:05"), strings.Join(labels, ", "))
}

// ─── Registry ───────────────────────────────────────────────────

type Registry struct {
	mu sync.RWMutex

	// Indice primario: ID → instancia
	instances map[string]ServiceInstance

	// Indices secundarios para busquedas rapidas
	byName map[string]map[string]bool // name → set of IDs
	byHost map[string]map[string]bool // host → set of IDs
	byLabel map[string]map[string]map[string]bool
	// ^label_key → label_value → set of IDs
}

func NewRegistry() *Registry {
	return &Registry{
		instances: make(map[string]ServiceInstance),
		byName:    make(map[string]map[string]bool),
		byHost:    make(map[string]map[string]bool),
		byLabel:   make(map[string]map[string]map[string]bool),
	}
}

// ── Register ────────────────────────────────────────────────────

func (r *Registry) Register(inst ServiceInstance) {
	r.mu.Lock()
	defer r.mu.Unlock()

	// Si ya existe, desindexar primero
	if old, exists := r.instances[inst.ID]; exists {
		r.removeFromIndices(old)
	}

	inst.LastSeen = time.Now()
	r.instances[inst.ID] = inst

	// Actualizar indices
	r.addToIndex(r.byName, inst.Name, inst.ID)
	r.addToIndex(r.byHost, inst.Host, inst.ID)

	for k, v := range inst.Labels {
		if r.byLabel[k] == nil {
			r.byLabel[k] = make(map[string]map[string]bool)
		}
		r.addToIndex(r.byLabel[k], v, inst.ID)
	}
}

func (r *Registry) addToIndex(idx map[string]map[string]bool, key, id string) {
	if idx[key] == nil {
		idx[key] = make(map[string]bool)
	}
	idx[key][id] = true
}

func (r *Registry) removeFromIndices(inst ServiceInstance) {
	delete(r.byName[inst.Name], inst.ID)
	if len(r.byName[inst.Name]) == 0 {
		delete(r.byName, inst.Name)
	}

	delete(r.byHost[inst.Host], inst.ID)
	if len(r.byHost[inst.Host]) == 0 {
		delete(r.byHost, inst.Host)
	}

	for k, v := range inst.Labels {
		if r.byLabel[k] != nil && r.byLabel[k][v] != nil {
			delete(r.byLabel[k][v], inst.ID)
			if len(r.byLabel[k][v]) == 0 {
				delete(r.byLabel[k], v)
			}
			if len(r.byLabel[k]) == 0 {
				delete(r.byLabel, k)
			}
		}
	}
}

// ── Deregister ──────────────────────────────────────────────────

func (r *Registry) Deregister(id string) bool {
	r.mu.Lock()
	defer r.mu.Unlock()

	inst, exists := r.instances[id]
	if !exists {
		return false
	}

	r.removeFromIndices(inst)
	delete(r.instances, id)
	return true
}

// ── Lookup ──────────────────────────────────────────────────────

func (r *Registry) Get(id string) (ServiceInstance, bool) {
	r.mu.RLock()
	defer r.mu.RUnlock()
	inst, ok := r.instances[id]
	return inst, ok
}

func (r *Registry) ByName(name string) []ServiceInstance {
	r.mu.RLock()
	defer r.mu.RUnlock()
	return r.resolveIDs(r.byName[name])
}

func (r *Registry) ByHost(host string) []ServiceInstance {
	r.mu.RLock()
	defer r.mu.RUnlock()
	return r.resolveIDs(r.byHost[host])
}

func (r *Registry) ByLabel(key, value string) []ServiceInstance {
	r.mu.RLock()
	defer r.mu.RUnlock()
	if r.byLabel[key] == nil {
		return nil
	}
	return r.resolveIDs(r.byLabel[key][value])
}

func (r *Registry) resolveIDs(ids map[string]bool) []ServiceInstance {
	if len(ids) == 0 {
		return nil
	}
	result := make([]ServiceInstance, 0, len(ids))
	for id := range ids {
		if inst, ok := r.instances[id]; ok {
			result = append(result, inst)
		}
	}
	slices.SortFunc(result, func(a, b ServiceInstance) int {
		return cmp.Compare(a.ID, b.ID)
	})
	return result
}

// ── Discovery: buscar healthy instances ─────────────────────────

func (r *Registry) Discover(name string) []ServiceInstance {
	instances := r.ByName(name)
	var healthy []ServiceInstance
	for _, inst := range instances {
		if inst.Status == StatusHealthy {
			healthy = append(healthy, inst)
		}
	}
	return healthy
}

// ── Health Update ───────────────────────────────────────────────

func (r *Registry) UpdateStatus(id string, status ServiceStatus) bool {
	r.mu.Lock()
	defer r.mu.Unlock()

	inst, exists := r.instances[id]
	if !exists {
		return false
	}
	// Copiar, modificar, reasignar (no se puede modificar in-place)
	inst.Status = status
	inst.LastSeen = time.Now()
	r.instances[id] = inst
	return true
}

// ── Stats ───────────────────────────────────────────────────────

type RegistryStats struct {
	Total      int
	ByName     map[string]int
	ByHost     map[string]int
	ByStatus   map[ServiceStatus]int
	ByVersion  map[string]map[string]int // name → version → count
	LabelKeys  []string
}

func (r *Registry) Stats() RegistryStats {
	r.mu.RLock()
	defer r.mu.RUnlock()

	stats := RegistryStats{
		Total:     len(r.instances),
		ByName:    make(map[string]int),
		ByHost:    make(map[string]int),
		ByStatus:  make(map[ServiceStatus]int),
		ByVersion: make(map[string]map[string]int),
	}

	for _, inst := range r.instances {
		stats.ByName[inst.Name]++
		stats.ByHost[inst.Host]++
		stats.ByStatus[inst.Status]++

		if stats.ByVersion[inst.Name] == nil {
			stats.ByVersion[inst.Name] = make(map[string]int)
		}
		stats.ByVersion[inst.Name][inst.Version]++
	}

	// Label keys unicos
	labelKeySet := make(map[string]bool)
	for k := range r.byLabel {
		labelKeySet[k] = true
	}
	stats.LabelKeys = slices.Sorted(maps.Keys(labelKeySet))

	return stats
}

// ── All instances ───────────────────────────────────────────────

func (r *Registry) All() []ServiceInstance {
	r.mu.RLock()
	defer r.mu.RUnlock()

	result := make([]ServiceInstance, 0, len(r.instances))
	for _, inst := range r.instances {
		result = append(result, inst)
	}
	slices.SortFunc(result, func(a, b ServiceInstance) int {
		return cmp.Compare(a.ID, b.ID)
	})
	return result
}

func (r *Registry) Len() int {
	r.mu.RLock()
	defer r.mu.RUnlock()
	return len(r.instances)
}

// ─── Display ────────────────────────────────────────────────────

func printInstances(title string, instances []ServiceInstance) {
	fmt.Printf("\n%s (%d)\n", title, len(instances))
	fmt.Println(strings.Repeat("─", 90))
	if len(instances) == 0 {
		fmt.Println("  (none)")
		return
	}
	for _, inst := range instances {
		fmt.Printf("  %s\n", inst)
	}
}

// ─── Main ───────────────────────────────────────────────────────

func main() {
	reg := NewRegistry()

	fmt.Println(strings.Repeat("═", 70))
	fmt.Println("  SERVICE REGISTRY & DISCOVERY")
	fmt.Println(strings.Repeat("═", 70))

	// ═══ Registrar servicios ═══
	services := []ServiceInstance{
		{
			ID: "api-1", Name: "api", Host: "10.0.2.1", Port: 8080,
			Version: "2.1.0", Status: StatusHealthy,
			Labels: map[string]string{"env": "prod", "region": "us-east", "tier": "backend"},
		},
		{
			ID: "api-2", Name: "api", Host: "10.0.2.2", Port: 8080,
			Version: "2.1.0", Status: StatusHealthy,
			Labels: map[string]string{"env": "prod", "region": "us-west", "tier": "backend"},
		},
		{
			ID: "api-3", Name: "api", Host: "10.0.2.3", Port: 8080,
			Version: "2.0.0", Status: StatusUnhealthy,
			Labels: map[string]string{"env": "prod", "region": "us-east", "tier": "backend"},
		},
		{
			ID: "web-1", Name: "web", Host: "10.0.1.1", Port: 80,
			Version: "3.5.0", Status: StatusHealthy,
			Labels: map[string]string{"env": "prod", "region": "us-east", "tier": "frontend"},
		},
		{
			ID: "web-2", Name: "web", Host: "10.0.1.2", Port: 80,
			Version: "3.5.0", Status: StatusHealthy,
			Labels: map[string]string{"env": "prod", "region": "us-west", "tier": "frontend"},
		},
		{
			ID: "db-1", Name: "postgres", Host: "10.0.3.1", Port: 5432,
			Version: "14.2", Status: StatusHealthy,
			Labels: map[string]string{"env": "prod", "region": "us-east", "role": "primary"},
		},
		{
			ID: "db-2", Name: "postgres", Host: "10.0.3.2", Port: 5432,
			Version: "14.2", Status: StatusHealthy,
			Labels: map[string]string{"env": "prod", "region": "us-east", "role": "replica"},
		},
		{
			ID: "cache-1", Name: "redis", Host: "10.0.4.1", Port: 6379,
			Version: "7.0.0", Status: StatusHealthy,
			Labels: map[string]string{"env": "prod", "region": "us-east"},
		},
		{
			ID: "mon-1", Name: "prometheus", Host: "10.0.5.1", Port: 9090,
			Version: "2.45.0", Status: StatusHealthy,
			Labels: map[string]string{"env": "prod", "tier": "infra"},
		},
	}

	for _, svc := range services {
		reg.Register(svc)
	}

	fmt.Printf("\nRegistered %d service instances\n", reg.Len())

	// ═══ Listar todos ═══
	printInstances("ALL SERVICES", reg.All())

	// ═══ Lookup por ID (comma ok) ═══
	fmt.Println("\n── Lookup by ID ──")
	if inst, ok := reg.Get("api-1"); ok {
		fmt.Printf("  Found: %s\n", inst)
	}
	if _, ok := reg.Get("api-99"); !ok {
		fmt.Println("  api-99: not found")
	}

	// ═══ Discovery: encontrar instancias healthy ═══
	fmt.Println("\n── Service Discovery ──")
	apiEndpoints := reg.Discover("api")
	fmt.Printf("  Healthy 'api' instances: %d\n", len(apiEndpoints))
	for _, ep := range apiEndpoints {
		fmt.Printf("    → %s (v%s)\n", ep.Addr(), ep.Version)
	}

	// ═══ Buscar por host ═══
	printInstances("SERVICES ON 10.0.2.1", reg.ByHost("10.0.2.1"))

	// ═══ Buscar por label ═══
	printInstances("REGION=us-east", reg.ByLabel("region", "us-east"))
	printInstances("TIER=frontend", reg.ByLabel("tier", "frontend"))

	// ═══ Health update ═══
	fmt.Println("\n── Health Update ──")
	reg.UpdateStatus("api-3", StatusHealthy)
	fmt.Println("  api-3 → healthy")
	apiEndpoints = reg.Discover("api")
	fmt.Printf("  Healthy 'api' instances after update: %d\n", len(apiEndpoints))

	// ═══ Deregister ═══
	fmt.Println("\n── Deregister ──")
	reg.Deregister("cache-1")
	fmt.Printf("  Removed cache-1. Total instances: %d\n", reg.Len())

	// ═══ Stats ═══
	fmt.Println("\n── Registry Stats ──")
	stats := reg.Stats()
	fmt.Printf("  Total instances: %d\n", stats.Total)

	fmt.Println("  By name:")
	for _, name := range slices.Sorted(maps.Keys(stats.ByName)) {
		fmt.Printf("    %-15s %d instances\n", name, stats.ByName[name])
	}

	fmt.Println("  By status:")
	for status, count := range stats.ByStatus {
		fmt.Printf("    %-12s %d\n", status, count)
	}

	fmt.Println("  By version:")
	for _, name := range slices.Sorted(maps.Keys(stats.ByVersion)) {
		versions := stats.ByVersion[name]
		parts := make([]string, 0, len(versions))
		for v, count := range versions {
			parts = append(parts, fmt.Sprintf("v%s(%d)", v, count))
		}
		slices.Sort(parts)
		fmt.Printf("    %-15s %s\n", name, strings.Join(parts, ", "))
	}

	fmt.Printf("  Label keys: %s\n", strings.Join(stats.LabelKeys, ", "))

	// ═══ Patrones de maps usados ═══
	fmt.Println("\n── Map Patterns Used ──")
	patterns := []string{
		"map[string]ServiceInstance   — indice primario ID → datos",
		"map[string]map[string]bool  — indice secundario name → IDs (set)",
		"map[string]map[string]M     — indice anidado label_key → val → IDs",
		"map[string]string           — labels de cada instancia",
		"map[string]int              — contadores (ByName, ByHost)",
		"map[ServiceStatus]int       — contador por status (enum key)",
		"map[string]map[string]int   — version count anidado",
		"_, ok := m[key]             — comma ok en Get, Deregister",
		"delete(m, key)              — cleanup de indices",
		"maps.Keys + slices.Sorted   — iteracion ordenada",
		"maps.Clone                  — (disponible para shallow copy)",
		"sync.RWMutex + map          — concurrencia segura",
		"copiar-modificar-reasignar  — actualizar struct en map",
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
│  1 │ Escribir en nil map (panic)              │ Siempre make() o literal antes de write  │ Panic   │
│  2 │ No usar comma ok → ambiguedad con zero   │ val, ok := m[key]; if ok { ... }         │ Logic   │
│    │ value                                    │                                          │         │
│  3 │ Asumir orden de iteracion                │ Extraer keys, sort, iterar keys          │ Logic   │
│  4 │ Lectura/escritura concurrente → fatal    │ sync.RWMutex o sync.Map                  │ Race    │
│  5 │ Modificar struct field de map value      │ Copiar → modificar → reasignar, o *V     │ Compile │
│  6 │ Tomar direccion de map element           │ Copiar a variable, o usar map[K]*V       │ Compile │
│  7 │ nil inner map en map anidado             │ Inicializar cada nivel, o usar struct key │ Panic   │
│  8 │ float/NaN como key                       │ Nunca usar float como key                │ Logic   │
│  9 │ Esperar que map se encoja tras delete    │ Crear nuevo map o usar clear (Go 1.21+)  │ Memory  │
│ 10 │ maps.Clone en map de slices (shallow)    │ Deep copy manual si V tiene referencias  │ Logic   │
│ 11 │ Agregar entries durante range            │ Comportamiento no determinista — evitar   │ Logic   │
│ 12 │ Olvidar pre-alocar con make(map, hint)   │ make(map[K]V, expectedSize)              │ Perf    │
│ 13 │ sync.Map sin type assertion              │ Usar generics wrapper o SafeMap con mutex │ Type    │
│ 14 │ No limpiar map en struct reutilizado     │ clear(m) o asignar nuevo make()          │ Logic   │
└────┴──────────────────────────────────────────┴──────────────────────────────────────────┴─────────┘
```

---

## 16. Ejercicios de prediccion

**Ejercicio 1**: ¿Que imprime?

```go
var m map[string]int
fmt.Println(m["key"])
fmt.Println(len(m))
```

<details>
<summary>Respuesta</summary>

```
0
0
```

`m` es nil. Leer de nil map retorna zero value (0 para int). `len(nil map)` es 0. Ambas operaciones son seguras en nil maps.
</details>

**Ejercicio 2**: ¿Que imprime?

```go
m := map[string]int{"a": 0, "b": 1}
v1 := m["a"]
v2 := m["z"]
fmt.Println(v1 == v2)
```

<details>
<summary>Respuesta</summary>

```
true
```

`m["a"]` retorna 0 (existe, valor es 0). `m["z"]` retorna 0 (no existe, zero value de int). Ambos son 0, asi que `v1 == v2` es true. Esta es la razon por la que el comma ok idiom es esencial.
</details>

**Ejercicio 3**: ¿Que imprime?

```go
m := map[string]int{"a": 1, "b": 2}
delete(m, "c")
fmt.Println(len(m))
```

<details>
<summary>Respuesta</summary>

```
2
```

`delete` de una key que no existe es un no-op. No panic, no error. El map queda inalterado con 2 entradas.
</details>

**Ejercicio 4**: ¿Que imprime?

```go
m := map[string]int{"a": 1}

func modify(m map[string]int) {
    m["b"] = 2
}

modify(m)
fmt.Println(len(m))
```

<details>
<summary>Respuesta</summary>

```
2
```

Maps son referencias (puntero a hmap). `modify` recibe una copia del puntero — apunta a la misma tabla hash. `m["b"] = 2` modifica el map original. El caller ve la nueva entrada.
</details>

**Ejercicio 5**: ¿Que imprime?

```go
m := map[string]int{"x": 10, "y": 20, "z": 30}
for k := range m {
    if k == "y" {
        delete(m, k)
    }
}
fmt.Println(len(m))
```

<details>
<summary>Respuesta</summary>

```
2
```

Es seguro hacer `delete` durante `range`. La key "y" se elimina. Quedan "x" y "z". `len(m) = 2`.
</details>

**Ejercicio 6**: ¿Que imprime?

```go
type Point struct {
    X, Y int
}
m := map[Point]string{
    {1, 2}: "A",
    {3, 4}: "B",
}
fmt.Println(m[Point{1, 2}])
fmt.Println(m[Point{5, 6}])
```

<details>
<summary>Respuesta</summary>

```
A

```

Structs con campos comparables son keys validos. `Point{1,2}` existe → "A". `Point{5,6}` no existe → "" (zero value de string, imprime linea vacia).
</details>

**Ejercicio 7**: ¿Que imprime?

```go
m1 := map[string]int{"a": 1}
m2 := m1
m2["a"] = 99
fmt.Println(m1["a"])
```

<details>
<summary>Respuesta</summary>

```
99
```

`m2 := m1` copia el puntero, no los datos. `m1` y `m2` apuntan al mismo hash map. Modificar `m2["a"]` modifica `m1["a"]`.
</details>

**Ejercicio 8**: ¿Que imprime?

```go
m := map[string][]int{
    "nums": {1, 2, 3},
}
m2 := maps.Clone(m)
m2["nums"][0] = 999
fmt.Println(m["nums"][0])
```

<details>
<summary>Respuesta</summary>

```
999
```

`maps.Clone` es **shallow copy**. Copia las keys y los valores, pero los valores de tipo slice siguen siendo referencias al mismo backing array. `m2["nums"]` y `m["nums"]` apuntan al mismo `[]int`. Modificar uno afecta al otro.
</details>

**Ejercicio 9**: ¿Que imprime?

```go
counts := make(map[string]int)
words := []string{"go", "rust", "go", "c", "go", "rust"}
for _, w := range words {
    counts[w]++
}
fmt.Println(counts["go"], counts["python"])
```

<details>
<summary>Respuesta</summary>

```
3 0
```

`counts["go"]++` empieza en 0 (zero value) y se incrementa 3 veces. `counts["python"]` nunca se escribio — retorna 0 (zero value). El patron de counter funciona gracias a que el zero value de int es 0.
</details>

**Ejercicio 10**: ¿Que imprime?

```go
type Config struct {
    Port int
}
m := map[string]Config{
    "web": {Port: 80},
}
// m["web"].Port = 8080  ← ¿compila?
cfg := m["web"]
cfg.Port = 8080
fmt.Println(m["web"].Port)
```

<details>
<summary>Respuesta</summary>

```
80
```

La linea comentada (`m["web"].Port = 8080`) **no compila** — no puedes modificar un campo de un struct almacenado como valor en un map. La alternativa de copiar (`cfg := m["web"]`) crea una copia local. Modificar `cfg.Port` no afecta el map. Para que el cambio persista, necesitas `m["web"] = cfg` despues de modificar.
</details>

---

## 17. Resumen visual

```
┌──────────────────────────────────────────────────────────────┐
│              MAPS — RESUMEN                                  │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  CREACION                                                    │
│  var m map[K]V              → nil (read OK, write PANIC)     │
│  m := map[K]V{k:v, ...}    → literal (funcional)            │
│  m := make(map[K]V)         → vacio (funcional)              │
│  m := make(map[K]V, hint)   → vacio pre-alocado (perf)      │
│                                                              │
│  OPERACIONES                                                 │
│  m[key] = val               → insert / update                │
│  val := m[key]              → read (zero value si no existe) │
│  val, ok := m[key]          → comma ok (existe? val:zero)    │
│  delete(m, key)             → delete (no-op si no existe)    │
│  len(m)                     → numero de entries              │
│  clear(m)                   → vaciar (Go 1.21+)              │
│  for k, v := range m        → iteracion (orden RANDOM)       │
│                                                              │
│  KEY CONSTRAINT                                              │
│  ✓ int, string, float, bool, rune, array, struct, pointer    │
│  ✗ slice, map, func (no comparables)                         │
│  ⚠ float: NaN crea entries irrecuperables                    │
│                                                              │
│  REFERENCIA                                                  │
│  Map es un puntero (8 bytes) → func puede modificar el map   │
│  m2 := m1 → ambos apuntan al MISMO hash map                 │
│  maps.Clone → shallow copy (valores ref siguen compartidos)  │
│                                                              │
│  PATRONES                                                    │
│  ├─ Set:       map[T]bool o map[T]struct{}                   │
│  ├─ Counter:   m[key]++ (zero value = 0, incrementa)         │
│  ├─ GroupBy:   m[key] = append(m[key], item)                 │
│  ├─ Cache:     if v, ok := m[k]; ok { return v }             │
│  ├─ Inverse:   invertir K→V a V→K                            │
│  ├─ Default:   if !ok { return defaultVal }                  │
│  ├─ Dispatch:  map[string]func(args) → command tables        │
│  └─ Config:    map[string]string → key-value pairs           │
│                                                              │
│  RESTRICCIONES                                               │
│  ├─ No thread-safe → sync.RWMutex o sync.Map                │
│  ├─ No comparable → maps.Equal (Go 1.21+)                   │
│  ├─ No tomar &m[key] → copiar a variable                    │
│  ├─ No m[key].Field = x → copiar, modificar, reasignar      │
│  ├─ No se encoge → crear nuevo o clear()                     │
│  └─ Iteracion random → sort keys para determinismo           │
│                                                              │
│  maps PACKAGE (Go 1.21+)                                     │
│  Clone, Equal, EqualFunc, DeleteFunc, Copy, Keys, Values     │
│                                                              │
│  CONCURRENCIA                                                │
│  ├─ map normal + sync.RWMutex → general purpose              │
│  ├─ sync.Map → write-once/read-many o keys disjuntas         │
│  └─ Acceso concurrente sin sync → fatal error (no panic)     │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: T02 - Structs — declaracion, inicializacion, campos exportados (mayuscula), struct tags (json, yaml)
