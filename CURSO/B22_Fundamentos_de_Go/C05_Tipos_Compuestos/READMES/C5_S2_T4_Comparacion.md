# Comparacion — == en Structs, reflect.DeepEqual, maps/slices packages y Patrones Custom

## 1. Introduccion

La comparacion de valores en Go sigue una regla simple pero con muchas implicaciones: **solo los tipos comparables pueden usar `==`**. Los tipos primitivos (int, string, bool, float, pointer) son comparables. Los arrays y structs son comparables si todos sus elementos/campos lo son. Pero slices, maps y funciones **no son comparables** — el compilador rechaza `==` sobre ellos.

Esta restriccion tiene consecuencias profundas: determina que tipos pueden ser keys de map, que structs pueden usarse con `==`, y cuando necesitas `reflect.DeepEqual` o los packages `maps`/`slices` (Go 1.21+). Entender las reglas de comparacion es entender que tipos son "first-class citizens" en las tablas hash de Go y cuales requieren comparacion manual.

En SysAdmin/DevOps, la comparacion aparece en: detectar cambios en configuraciones, comparar estados de infraestructura, implementar caches (necesitas keys comparables), verificar igualdad en tests, y determinar si un deployment difiere del estado actual.

```
┌──────────────────────────────────────────────────────────────┐
│              COMPARACION EN GO                               │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Tipos COMPARABLES (== y != funcionan):                      │
│  ├─ bool, int*, uint*, float*, complex*, string, rune        │
│  ├─ pointer (*T)                                             │
│  ├─ channel (chan T)                                          │
│  ├─ interface (compara tipo + valor dinamico)                │
│  ├─ array ([N]T si T es comparable)                          │
│  └─ struct (si TODOS los campos son comparables)             │
│                                                              │
│  Tipos NO comparables (== es error de compilacion):          │
│  ├─ slice ([]T)                                              │
│  ├─ map (map[K]V)                                            │
│  └─ func (solo con nil)                                      │
│                                                              │
│  Opciones para no-comparables:                               │
│  ├─ reflect.DeepEqual (universal, lento)                     │
│  ├─ slices.Equal / maps.Equal (Go 1.21+, rapido)            │
│  ├─ Metodo Equal custom (mas rapido, type-safe)              │
│  └─ Comparar con nil: s == nil (slices, maps, funcs)         │
│                                                              │
│  Implicacion clave:                                          │
│  Solo tipos comparables pueden ser keys de map               │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Reglas de comparabilidad por tipo

### Tipos primitivos: siempre comparables

```go
// Todos los tipos primitivos soportan == y !=

// Enteros
fmt.Println(42 == 42)     // true
fmt.Println(int8(1) == 1) // true

// Flotantes
fmt.Println(3.14 == 3.14) // true

// ⚠ NaN: el unico valor que NO es igual a si mismo
import "math"
nan := math.NaN()
fmt.Println(nan == nan) // false (!) — IEEE 754

// Strings
fmt.Println("go" == "go")     // true — compara contenido, no puntero
fmt.Println("go" == "Go")     // false — case sensitive

// Booleans
fmt.Println(true == true)   // true
fmt.Println(true == false)  // false

// Runes (alias de int32)
fmt.Println('A' == 65)     // true
```

### Punteros: comparan direccion, no contenido

```go
a := 42
b := 42
pa := &a
pb := &b
pc := &a

fmt.Println(pa == pb) // false — diferentes direcciones
fmt.Println(pa == pc) // true  — misma direccion
fmt.Println(*pa == *pb) // true — mismos valores (dereferenciar)

// nil pointers
var p1, p2 *int
fmt.Println(p1 == p2)  // true — ambos nil
fmt.Println(p1 == nil) // true

// Punteros a structs
type Point struct{ X, Y int }
s1 := &Point{1, 2}
s2 := &Point{1, 2}
fmt.Println(s1 == s2)  // false — diferentes alocaciones
fmt.Println(*s1 == *s2) // true — mismos valores
```

### Arrays: comparables si el elemento es comparable

```go
a := [3]int{1, 2, 3}
b := [3]int{1, 2, 3}
c := [3]int{1, 2, 4}

fmt.Println(a == b) // true  — mismos elementos
fmt.Println(a == c) // false — c[2] difiere

// Arrays de diferente tamaño son tipos diferentes:
// [3]int != [4]int — no se pueden comparar

// Arrays de strings
s1 := [2]string{"hello", "world"}
s2 := [2]string{"hello", "world"}
fmt.Println(s1 == s2) // true

// Arrays de arrays
m1 := [2][2]int{{1, 2}, {3, 4}}
m2 := [2][2]int{{1, 2}, {3, 4}}
fmt.Println(m1 == m2) // true — comparacion recursiva

// ✗ Arrays de slices: NO comparable
// [3][]int{} == [3][]int{} // ERROR: [3][]int cannot be compared
```

### Structs: comparables si TODOS los campos lo son

```go
// ✓ Struct comparable
type Point struct {
    X, Y int
}
p1 := Point{3, 4}
p2 := Point{3, 4}
p3 := Point{5, 6}
fmt.Println(p1 == p2) // true  — todos los campos iguales
fmt.Println(p1 == p3) // false

// ✓ Struct con multiples tipos comparables
type Server struct {
    Name   string
    Port   int
    Active bool
}
s1 := Server{"web01", 80, true}
s2 := Server{"web01", 80, true}
fmt.Println(s1 == s2) // true

// ✗ Struct NO comparable — contiene slice
type Config struct {
    Name  string
    Ports []int // slice → no comparable
}
// c1 := Config{"web", []int{80}}
// c2 := Config{"web", []int{80}}
// fmt.Println(c1 == c2) // ERROR: struct containing []int cannot be compared

// ✗ Struct NO comparable — contiene map
type Labels struct {
    Data map[string]string // map → no comparable
}

// ✗ Struct NO comparable — contiene func
type Handler struct {
    Fn func() // func → no comparable
}
```

### Interfaces: comparan tipo dinamico + valor

```go
var a, b interface{}

a = 42
b = 42
fmt.Println(a == b) // true — mismo tipo (int) y mismo valor

a = 42
b = "42"
fmt.Println(a == b) // false — diferentes tipos

a = 42
b = int64(42)
fmt.Println(a == b) // false — int != int64 (diferentes tipos)

// ⚠ Panic si el tipo dinamico no es comparable:
a = []int{1, 2}
b = []int{1, 2}
// fmt.Println(a == b) // PANIC en runtime:
// "comparing uncomparable type []int"
// El compilador no puede verificar en compile time
// porque interface{} acepta cualquier tipo

// nil interfaces
var x, y interface{}
fmt.Println(x == y)   // true — ambos nil
fmt.Println(x == nil) // true
```

### La trampa: interface nil vs interface con valor nil

```go
type MyError struct{ msg string }
func (e *MyError) Error() string { return e.msg }

var err *MyError  // nil pointer del tipo concreto
var iface error = err // interface con tipo=*MyError, valor=nil

fmt.Println(err == nil)   // true  — el puntero es nil
fmt.Println(iface == nil) // false (!) — la interface tiene tipo (*MyError)
// Una interface es nil SOLO si tanto tipo como valor son nil
// iface tiene tipo = *MyError, valor = nil → NO es nil interface

// ¿Como comparar correctamente?
fmt.Println(iface == (*MyError)(nil)) // true — compara con nil del mismo tipo

// O usar reflect:
fmt.Println(reflect.ValueOf(iface).IsNil()) // true
```

---

## 3. Tipos no comparables: por que y como manejarlos

### Por que slices no son comparables

```go
// Go tiene razones de diseño para prohibir slice == slice:
//
// 1. Ambiguedad semantica:
//    ¿Dos slices son "iguales" si apuntan al mismo backing array?
//    ¿O si tienen los mismos elementos?
//    s1 := []int{1, 2, 3}
//    s2 := s1[:] // Mismo backing array, diferentes headers
//    ¿s1 == s2? Ambas respuestas son "correctas"
//
// 2. Mutabilidad:
//    Un slice puede cambiar entre la comparacion y el uso del resultado
//    La "igualdad" de slices es efimera
//
// 3. Consistencia con maps:
//    Si slices fueran comparables, podrian ser keys de map
//    Pero el hash de un slice cambiaria al mutar → corrompe el map
//
// Lo que SI puedes hacer:
var s []int
fmt.Println(s == nil) // true — comparar con nil es valido
```

### Por que maps no son comparables

```go
// Mismas razones que slices, mas:
// - Los maps son punteros internos — ¿comparas la identidad o el contenido?
// - Un map puede tener millones de entries — comparacion O(n)
// - La iteracion no tiene orden → definir "igualdad" es complejo

var m map[string]int
fmt.Println(m == nil) // true — comparar con nil es valido
```

### Por que funciones no son comparables

```go
// Las funciones (closures) capturan estado — dos funciones
// con el mismo codigo pero diferente estado capturado
// ¿son "iguales"? Go evita la ambiguedad.

var f func()
fmt.Println(f == nil) // true — solo comparar con nil
```

---

## 4. reflect.DeepEqual: comparacion universal

### Como funciona

```go
import "reflect"

// reflect.DeepEqual compara CUALQUIER par de valores recursivamente
// Funciona con todos los tipos, incluyendo slices, maps y structs anidados

// Slices
s1 := []int{1, 2, 3}
s2 := []int{1, 2, 3}
s3 := []int{1, 2, 4}
fmt.Println(reflect.DeepEqual(s1, s2)) // true
fmt.Println(reflect.DeepEqual(s1, s3)) // false

// Maps
m1 := map[string]int{"a": 1, "b": 2}
m2 := map[string]int{"a": 1, "b": 2}
m3 := map[string]int{"a": 1, "b": 3}
fmt.Println(reflect.DeepEqual(m1, m2)) // true
fmt.Println(reflect.DeepEqual(m1, m3)) // false

// Structs con campos no comparables
type Config struct {
    Name  string
    Ports []int
    Labels map[string]string
}
c1 := Config{"web", []int{80, 443}, map[string]string{"env": "prod"}}
c2 := Config{"web", []int{80, 443}, map[string]string{"env": "prod"}}
fmt.Println(reflect.DeepEqual(c1, c2)) // true

// Structs anidados
type Deployment struct {
    Name    string
    Config  Config
    Servers []Server
}
// reflect.DeepEqual compara recursivamente todos los niveles
```

### Reglas de DeepEqual

```go
// Reglas detalladas de reflect.DeepEqual:

// 1. Slices: iguales si mismo len y mismos elementos (posicional)
//    nil slice != empty slice
fmt.Println(reflect.DeepEqual([]int(nil), []int{}))  // false (!)
fmt.Println(reflect.DeepEqual([]int(nil), []int(nil))) // true

// 2. Maps: iguales si mismas keys con mismos valores
//    nil map != empty map
fmt.Println(reflect.DeepEqual(map[string]int(nil), map[string]int{})) // false

// 3. Punteros: sigue el puntero y compara los valores apuntados
a := &Point{1, 2}
b := &Point{1, 2}
fmt.Println(reflect.DeepEqual(a, b)) // true (compara valores, no direcciones)
fmt.Println(a == b)                   // false (compara direcciones)

// 4. Interfaces: compara tipo + valor dinamico
var i1 interface{} = []int{1, 2}
var i2 interface{} = []int{1, 2}
fmt.Println(reflect.DeepEqual(i1, i2)) // true (no panic, a diferencia de ==)

// 5. Ciclos: DeepEqual detecta ciclos y no entra en loop infinito
type Node struct {
    Val  int
    Next *Node
}
n := &Node{Val: 1}
n.Next = n // Ciclo
fmt.Println(reflect.DeepEqual(n, n)) // true — detecta ciclo

// 6. Funciones: solo iguales si ambas son nil
var f1, f2 func()
fmt.Println(reflect.DeepEqual(f1, f2)) // true (ambas nil)
f1 = func() {}
fmt.Println(reflect.DeepEqual(f1, f2)) // false
// f2 = func() {}
// fmt.Println(reflect.DeepEqual(f1, f2)) // false — funciones no-nil nunca iguales

// 7. Tipos diferentes: siempre false
fmt.Println(reflect.DeepEqual(42, int64(42))) // false — int != int64
```

### Performance de DeepEqual

```go
// reflect.DeepEqual es LENTO porque:
// 1. Usa reflexion (analisis de tipos en runtime)
// 2. Aloca memoria para el tracking de ciclos
// 3. Es recursivo con muchas llamadas

// Benchmark tipico (struct con 5 campos simples):
// BenchmarkReflectDeepEqual     ~500ns/op   ~112 B/alloc
// BenchmarkManualEqual          ~5ns/op     0 B/alloc
// → reflect.DeepEqual es ~100x mas lento

// Reglas de uso:
// ✓ Tests: la velocidad no importa, la convenience si
// ✓ Codigo que se ejecuta raramente (config reload, startup)
// ✗ Hot paths: loops, handlers HTTP, procesamiento de metricas
// ✗ Codigo donde el rendimiento importa

// En tests es idiomatico:
func TestConfig(t *testing.T) {
    got := loadConfig()
    want := Config{Name: "web", Ports: []int{80, 443}}
    if !reflect.DeepEqual(got, want) {
        t.Errorf("got %+v, want %+v", got, want)
    }
}
```

---

## 5. slices package: comparacion moderna (Go 1.21+)

### slices.Equal

```go
import "slices"

// slices.Equal compara dos slices elemento a elemento
// Requiere que T sea comparable
a := []int{1, 2, 3}
b := []int{1, 2, 3}
c := []int{1, 2, 4}

fmt.Println(slices.Equal(a, b))   // true
fmt.Println(slices.Equal(a, c))   // false
fmt.Println(slices.Equal(a, nil)) // false (len difiere)

// Nil slices
var nilSlice []int
fmt.Println(slices.Equal(nilSlice, nil)) // true — ambos nil
fmt.Println(slices.Equal(nilSlice, []int{})) // true (!) — ambos len 0
// ¡Diferente de reflect.DeepEqual! slices.Equal trata nil == empty

// slices.Equal funciona con cualquier tipo comparable:
s1 := []string{"a", "b", "c"}
s2 := []string{"a", "b", "c"}
fmt.Println(slices.Equal(s1, s2)) // true
```

### slices.EqualFunc: comparacion custom

```go
// Para tipos no comparables o comparacion personalizada

type Server struct {
    Name string
    Port int
    Tags []string // No comparable
}

servers1 := []Server{
    {"web01", 80, []string{"prod"}},
    {"db01", 5432, []string{"prod"}},
}
servers2 := []Server{
    {"web01", 80, []string{"prod"}},
    {"db01", 5432, []string{"prod"}},
}

// slices.EqualFunc con comparador custom
equal := slices.EqualFunc(servers1, servers2, func(a, b Server) bool {
    return a.Name == b.Name &&
           a.Port == b.Port &&
           slices.Equal(a.Tags, b.Tags)
})
fmt.Println(equal) // true

// Case-insensitive string comparison:
names1 := []string{"Web01", "DB01"}
names2 := []string{"web01", "db01"}
fmt.Println(slices.EqualFunc(names1, names2, strings.EqualFold)) // true
```

### slices.Compare: orden lexicografico

```go
// slices.Compare retorna -1, 0, +1 (como cmp.Compare)
// Compara elemento a elemento, el primer diferente determina el resultado

a := []int{1, 2, 3}
b := []int{1, 2, 4}
c := []int{1, 2, 3}

fmt.Println(slices.Compare(a, b)) // -1 (a < b, porque 3 < 4)
fmt.Println(slices.Compare(a, c)) // 0  (iguales)
fmt.Println(slices.Compare(b, a)) // 1  (b > a)

// Longitud diferente:
short := []int{1, 2}
long := []int{1, 2, 3}
fmt.Println(slices.Compare(short, long)) // -1 (prefijo → mas corto es menor)

// slices.CompareFunc para tipos custom
```

---

## 6. maps package: comparacion de maps (Go 1.21+)

### maps.Equal

```go
import "maps"

m1 := map[string]int{"a": 1, "b": 2, "c": 3}
m2 := map[string]int{"a": 1, "b": 2, "c": 3}
m3 := map[string]int{"a": 1, "b": 2}

fmt.Println(maps.Equal(m1, m2)) // true — mismas keys y valores
fmt.Println(maps.Equal(m1, m3)) // false — m3 le falta "c"

// nil maps
var nilMap map[string]int
fmt.Println(maps.Equal(nilMap, nil)) // true

// maps.Equal requiere que K y V sean comparable
// Para V no comparable, usar maps.EqualFunc
```

### maps.EqualFunc: valores no comparables

```go
// Comparar maps donde los valores contienen slices
type ServiceConfig struct {
    Port  int
    Hosts []string // No comparable
}

configs1 := map[string]ServiceConfig{
    "web": {Port: 80, Hosts: []string{"10.0.1.1", "10.0.1.2"}},
    "api": {Port: 8080, Hosts: []string{"10.0.2.1"}},
}
configs2 := map[string]ServiceConfig{
    "web": {Port: 80, Hosts: []string{"10.0.1.1", "10.0.1.2"}},
    "api": {Port: 8080, Hosts: []string{"10.0.2.1"}},
}

equal := maps.EqualFunc(configs1, configs2, func(a, b ServiceConfig) bool {
    return a.Port == b.Port && slices.Equal(a.Hosts, b.Hosts)
})
fmt.Println(equal) // true
```

---

## 7. nil vs empty: la diferencia sutil

### nil slice vs empty slice

```go
var nilSlice []int        // nil
emptySlice := []int{}     // empty (not nil)
makeSlice := make([]int, 0) // empty (not nil)

// == nil
fmt.Println(nilSlice == nil)   // true
fmt.Println(emptySlice == nil) // false
fmt.Println(makeSlice == nil)  // false

// reflect.DeepEqual: nil != empty
fmt.Println(reflect.DeepEqual(nilSlice, emptySlice)) // false

// slices.Equal: nil == empty (ambos len 0)
fmt.Println(slices.Equal(nilSlice, emptySlice)) // true

// JSON: nil → null, empty → []
j1, _ := json.Marshal(nilSlice)   // null
j2, _ := json.Marshal(emptySlice) // []
```

### nil map vs empty map

```go
var nilMap map[string]int
emptyMap := map[string]int{}

fmt.Println(nilMap == nil)  // true
fmt.Println(emptyMap == nil) // false

fmt.Println(reflect.DeepEqual(nilMap, emptyMap)) // false
fmt.Println(maps.Equal(nilMap, emptyMap))         // true (ambos len 0)

// JSON
j1, _ := json.Marshal(nilMap)   // null
j2, _ := json.Marshal(emptyMap) // {}
```

### Tabla de comportamiento nil vs empty

```
┌──────────────────────────────┬──────────────┬──────────────┬──────────────┐
│ Operacion                    │ nil          │ empty        │ Resultado    │
├──────────────────────────────┼──────────────┼──────────────┼──────────────┤
│ Slice:                       │              │              │              │
│  s == nil                    │ true         │ false        │              │
│  len(s)                      │ 0            │ 0            │ iguales      │
│  reflect.DeepEqual(nil, emp) │              │              │ false        │
│  slices.Equal(nil, emp)      │              │              │ true         │
│  json.Marshal                │ null         │ []           │ diferentes   │
│  append(s, x)                │ funciona     │ funciona     │ iguales      │
│  for range s                 │ 0 iteraciones│ 0 iteraciones│ iguales      │
├──────────────────────────────┼──────────────┼──────────────┼──────────────┤
│ Map:                         │              │              │              │
│  m == nil                    │ true         │ false        │              │
│  len(m)                      │ 0            │ 0            │ iguales      │
│  reflect.DeepEqual(nil, emp) │              │              │ false        │
│  maps.Equal(nil, emp)        │              │              │ true         │
│  json.Marshal                │ null         │ {}           │ diferentes   │
│  m[key] (read)               │ zero value   │ zero value   │ iguales      │
│  m[key] = v (write)          │ PANIC        │ funciona     │ DIFERENTES   │
│  delete(m, key)              │ no-op        │ no-op        │ iguales      │
└──────────────────────────────┴──────────────┴──────────────┴──────────────┘
```

---

## 8. Metodos Equal custom: la mejor opcion para produccion

### Por que escribir Equal manual

```go
// 1. Type-safe: el compilador verifica tipos
// 2. Rapido: sin reflexion, sin alocaciones
// 3. Control: decides QUE campos comparar (ej: ignorar timestamps)
// 4. Documentacion: el metodo explicita la semantica de igualdad

type Server struct {
    Name   string
    IP     string
    Port   int
    Tags   []string
    Labels map[string]string
}

func (s Server) Equal(other Server) bool {
    if s.Name != other.Name || s.IP != other.IP || s.Port != other.Port {
        return false
    }
    if !slices.Equal(s.Tags, other.Tags) {
        return false
    }
    if !maps.Equal(s.Labels, other.Labels) {
        return false
    }
    return true
}

s1 := Server{
    Name: "web01", IP: "10.0.1.1", Port: 80,
    Tags: []string{"prod", "us-east"},
    Labels: map[string]string{"env": "prod"},
}
s2 := s1 // copia
fmt.Println(s1.Equal(s2)) // true
```

### Equal que ignora campos especificos

```go
type Deployment struct {
    Name      string
    Version   string
    Replicas  int
    Labels    map[string]string
    UpdatedAt time.Time // No comparar — cambia en cada update
    revision  int       // No comparar — interno
}

// Equal semantico: ignora campos de metadata
func (d Deployment) Equal(other Deployment) bool {
    return d.Name == other.Name &&
           d.Version == other.Version &&
           d.Replicas == other.Replicas &&
           maps.Equal(d.Labels, other.Labels)
    // UpdatedAt y revision se ignoran intencionalmente
}

// EqualStrict: compara todo
func (d Deployment) EqualStrict(other Deployment) bool {
    return d.Name == other.Name &&
           d.Version == other.Version &&
           d.Replicas == other.Replicas &&
           maps.Equal(d.Labels, other.Labels) &&
           d.UpdatedAt.Equal(other.UpdatedAt) &&
           d.revision == other.revision
}
```

### Patron generico para comparaciones

```go
// Para structs donde todos los campos no-comparable son slices o maps
// de tipos comparable, el Equal es mecanico:

type Config struct {
    Name       string
    Port       int
    Debug      bool
    AllowedIPs []string
    Env        map[string]string
    Tags       []string
}

func (c Config) Equal(other Config) bool {
    // Campos comparables: comparacion directa
    if c.Name != other.Name || c.Port != other.Port || c.Debug != other.Debug {
        return false
    }
    // Slices
    if !slices.Equal(c.AllowedIPs, other.AllowedIPs) {
        return false
    }
    if !slices.Equal(c.Tags, other.Tags) {
        return false
    }
    // Maps
    if !maps.Equal(c.Env, other.Env) {
        return false
    }
    return true
}
```

---

## 9. Comparacion en tests: patrones y herramientas

### reflect.DeepEqual en tests

```go
func TestLoadConfig(t *testing.T) {
    got, err := LoadConfig("testdata/config.json")
    if err != nil {
        t.Fatal(err)
    }

    want := &Config{
        Name: "api",
        Port: 8080,
        Tags: []string{"prod", "us-east"},
        Env:  map[string]string{"LOG_LEVEL": "info"},
    }

    if !reflect.DeepEqual(got, want) {
        t.Errorf("LoadConfig:\n  got  %+v\n  want %+v", got, want)
    }
}
```

### Table-driven tests con comparacion

```go
func TestParseHosts(t *testing.T) {
    tests := []struct {
        name  string
        input string
        want  []string
    }{
        {"empty", "", nil},
        {"single", "web01", []string{"web01"}},
        {"multiple", "web01,db01,cache01", []string{"web01", "db01", "cache01"}},
        {"spaces", " web01 , db01 ", []string{"web01", "db01"}},
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            got := parseHosts(tt.input)
            // slices.Equal trata nil == empty (ambos len 0)
            // reflect.DeepEqual distingue nil de empty
            // Elegir segun la semantica deseada:
            if !slices.Equal(got, tt.want) {
                t.Errorf("parseHosts(%q) = %v, want %v", tt.input, got, tt.want)
            }
        })
    }
}
```

### cmp package de Google (go-cmp)

```go
// go-cmp es la libreria de comparacion mas popular para tests
// Mas potente que reflect.DeepEqual, con diffs legibles

import "github.com/google/go-cmp/cmp"

func TestDeployment(t *testing.T) {
    got := createDeployment()
    want := Deployment{
        Name:     "api",
        Replicas: 3,
        Labels:   map[string]string{"env": "prod"},
    }

    if diff := cmp.Diff(want, got); diff != "" {
        t.Errorf("Deployment mismatch (-want +got):\n%s", diff)
    }
    // Output (si difiere):
    // Deployment mismatch (-want +got):
    //   Deployment{
    //       Name: "api",
    // -     Replicas: 3,
    // +     Replicas: 5,
    //       Labels: {"env": "prod"},
    //   }
}

// cmp.Options para personalizar:
// - cmpopts.IgnoreFields: ignorar campos especificos
// - cmpopts.SortSlices: comparar slices sin importar orden
// - cmpopts.EquateEmpty: tratar nil == empty
// - cmp.Comparer: funcion de comparacion custom

import "github.com/google/go-cmp/cmp/cmpopts"

opts := cmp.Options{
    cmpopts.IgnoreFields(Deployment{}, "UpdatedAt", "revision"),
    cmpopts.SortSlices(func(a, b string) bool { return a < b }),
    cmpopts.EquateEmpty(), // nil == empty slices/maps
}

if diff := cmp.Diff(want, got, opts...); diff != "" {
    t.Errorf("mismatch:\n%s", diff)
}
```

---

## 10. Comparable constraint y generics

### La constraint comparable

```go
// Go 1.18+ introdujo la constraint `comparable`
// Se usa para restringir type parameters a tipos que soportan ==

func contains[T comparable](items []T, target T) bool {
    for _, item := range items {
        if item == target { // Solo funciona si T es comparable
            return true
        }
    }
    return false
}

contains([]int{1, 2, 3}, 2)           // true
contains([]string{"a", "b"}, "b")     // true
// contains([][]int{{1}}, []int{1})    // ✗ ERROR: []int does not satisfy comparable

// Usar como key de map en generics:
func index[K comparable, V any](items []V, keyFn func(V) K) map[K]V {
    m := make(map[K]V, len(items))
    for _, item := range items {
        m[keyFn(item)] = item
    }
    return m
}
```

### comparable vs any en generics

```go
// any = interface{} — acepta cualquier tipo, no puedes usar ==
// comparable = solo tipos que soportan == — puedes usar ==

func printEqual[T any](a, b T) {
    // fmt.Println(a == b) // ✗ ERROR: cannot compare a == b
    // any no garantiza comparabilidad
    fmt.Println(reflect.DeepEqual(a, b)) // ✓ Pero es lento
}

func printEqual[T comparable](a, b T) {
    fmt.Println(a == b) // ✓ Funciona porque T es comparable
}

// ¿Que pasa con structs que tienen campos no comparables?
type Config struct {
    Tags []string
}
// printEqual(Config{}, Config{}) // ✗ ERROR: Config does not satisfy comparable
// Aunque el struct existe, no satisface comparable por sus campos
```

### Hacer un tipo "comparable" artificialmente

```go
// Si necesitas usar un tipo como key de map pero no es comparable:
// → Derivar una key comparable

type Config struct {
    Name string
    Tags []string // No comparable
}

// Opcion 1: key string derivada
func (c Config) Key() string {
    return c.Name + "|" + strings.Join(c.Tags, ",")
}

cache := make(map[string]Result)
cfg := Config{Name: "web", Tags: []string{"prod", "us"}}
cache[cfg.Key()] = result

// Opcion 2: struct key con solo campos comparables
type ConfigKey struct {
    Name string
    Tags string // tags serializado como string
}

func (c Config) CacheKey() ConfigKey {
    sorted := slices.Clone(c.Tags)
    slices.Sort(sorted)
    return ConfigKey{
        Name: c.Name,
        Tags: strings.Join(sorted, ","),
    }
}

cache2 := make(map[ConfigKey]Result)
cache2[cfg.CacheKey()] = result
```

---

## 11. Patrones SysAdmin: comparacion de estado

### Detectar cambios en configuracion

```go
type ServiceState struct {
    Name     string
    Version  string
    Replicas int
    Ports    []int
    Env      map[string]string
    Labels   map[string]string
}

func (s ServiceState) Equal(other ServiceState) bool {
    if s.Name != other.Name || s.Version != other.Version || s.Replicas != other.Replicas {
        return false
    }
    if !slices.Equal(s.Ports, other.Ports) {
        return false
    }
    if !maps.Equal(s.Env, other.Env) {
        return false
    }
    if !maps.Equal(s.Labels, other.Labels) {
        return false
    }
    return true
}

type Change struct {
    Field    string
    OldValue string
    NewValue string
}

func (s ServiceState) Diff(other ServiceState) []Change {
    var changes []Change

    if s.Name != other.Name {
        changes = append(changes, Change{"name", s.Name, other.Name})
    }
    if s.Version != other.Version {
        changes = append(changes, Change{"version", s.Version, other.Version})
    }
    if s.Replicas != other.Replicas {
        changes = append(changes, Change{"replicas",
            fmt.Sprint(s.Replicas), fmt.Sprint(other.Replicas)})
    }
    if !slices.Equal(s.Ports, other.Ports) {
        changes = append(changes, Change{"ports",
            fmt.Sprint(s.Ports), fmt.Sprint(other.Ports)})
    }

    // Diff de maps: encontrar adds, removes, changes
    for k, v1 := range s.Env {
        if v2, ok := other.Env[k]; !ok {
            changes = append(changes, Change{"env." + k, v1, "(removed)"})
        } else if v1 != v2 {
            changes = append(changes, Change{"env." + k, v1, v2})
        }
    }
    for k, v2 := range other.Env {
        if _, ok := s.Env[k]; !ok {
            changes = append(changes, Change{"env." + k, "(added)", v2})
        }
    }

    return changes
}

// Uso:
current := ServiceState{
    Name: "api", Version: "2.1.0", Replicas: 3,
    Ports: []int{8080}, Env: map[string]string{"LOG": "info"},
}
desired := ServiceState{
    Name: "api", Version: "2.2.0", Replicas: 5,
    Ports: []int{8080, 8443}, Env: map[string]string{"LOG": "debug", "NEW": "val"},
}

if !current.Equal(desired) {
    for _, c := range current.Diff(desired) {
        fmt.Printf("  %s: %q → %q\n", c.Field, c.OldValue, c.NewValue)
    }
}
// version: "2.1.0" → "2.2.0"
// replicas: "3" → "5"
// ports: "[8080]" → "[8080 8443]"
// env.LOG: "info" → "debug"
// env.NEW: "(added)" → "val"
```

### Comparar inventarios con set operations

```go
func compareInventories(before, after []string) (added, removed, unchanged []string) {
    beforeSet := make(map[string]bool, len(before))
    afterSet := make(map[string]bool, len(after))

    for _, h := range before {
        beforeSet[h] = true
    }
    for _, h := range after {
        afterSet[h] = true
    }

    for _, h := range after {
        if !beforeSet[h] {
            added = append(added, h)
        } else {
            unchanged = append(unchanged, h)
        }
    }
    for _, h := range before {
        if !afterSet[h] {
            removed = append(removed, h)
        }
    }

    slices.Sort(added)
    slices.Sort(removed)
    slices.Sort(unchanged)
    return
}
```

---

## 12. Comparacion: Go vs C vs Rust

### Go: comparable restrictivo, reflect para el resto

```go
// == para tipos comparables (compile-time check)
// reflect.DeepEqual para cualquier tipo (runtime)
// slices.Equal, maps.Equal (Go 1.21+, rapido, type-safe)
// comparable constraint para generics

p1 := Point{1, 2}
p2 := Point{1, 2}
fmt.Println(p1 == p2) // true — compile-time safe

s1 := []int{1, 2, 3}
s2 := []int{1, 2, 3}
// s1 == s2 // ERROR de compilacion
fmt.Println(slices.Equal(s1, s2))          // true — Go 1.21+
fmt.Println(reflect.DeepEqual(s1, s2))     // true — universal, lento
```

### C: memcmp y comparacion manual

```c
#include <string.h>

typedef struct {
    int x, y;
} Point;

Point p1 = {1, 2};
Point p2 = {1, 2};

// memcmp: compara bytes — funciona para structs SIN padding ni punteros
if (memcmp(&p1, &p2, sizeof(Point)) == 0) {
    printf("equal\n");
}

// ⚠ PELIGROSO con padding:
typedef struct {
    char a;    // 1 byte
    // 3 bytes padding (basura!)
    int b;     // 4 bytes
} Padded;

Padded x = {.a = 1, .b = 2};
Padded y = {.a = 1, .b = 2};
// memcmp puede dar false porque el padding tiene valores diferentes
// Solucion: memset(&x, 0, sizeof(x)) antes de inicializar
// O comparar campo por campo

// No hay == para structs en C
// if (p1 == p2) // ERROR de compilacion
// Siempre manual o memcmp
```

### Rust: PartialEq, Eq, derive macros

```rust
// Rust: derive(PartialEq) genera == automaticamente
#[derive(PartialEq, Debug)]
struct Point {
    x: i32,
    y: i32,
}

let p1 = Point { x: 1, y: 2 };
let p2 = Point { x: 1, y: 2 };
assert_eq!(p1, p2); // == funciona gracias a derive

// Funciona con Vec, HashMap, etc. — TODOS los tipos que implementan PartialEq
let v1 = vec![1, 2, 3];
let v2 = vec![1, 2, 3];
assert_eq!(v1, v2); // ✓ — Vec implementa PartialEq

use std::collections::HashMap;
let m1: HashMap<&str, i32> = [("a", 1)].into();
let m2: HashMap<&str, i32> = [("a", 1)].into();
assert_eq!(m1, m2); // ✓ — HashMap implementa PartialEq

// Custom PartialEq:
impl PartialEq for Config {
    fn eq(&self, other: &Self) -> bool {
        self.name == other.name && self.port == other.port
        // Ignoring timestamp
    }
}

// PartialEq vs Eq:
// PartialEq: ==, puede ser no-reflexivo (NaN != NaN)
// Eq: marker trait, garantiza reflexividad (a == a siempre true)
// f64 implementa PartialEq pero NO Eq (por NaN)
// HashMap keys requieren Eq (no solo PartialEq)
```

### Tabla comparativa

```
┌──────────────────────┬──────────────────────┬──────────────────────┬──────────────────────┐
│ Aspecto              │ Go                   │ C                    │ Rust                 │
├──────────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤
│ Struct ==             │ Si (si comparable)   │ No (memcmp manual)   │ Si (derive PartialEq)│
│ Slice/Vec ==          │ No (slices.Equal)    │ No (memcmp manual)   │ Si (PartialEq impl)  │
│ Map/HashMap ==        │ No (maps.Equal)      │ No                   │ Si (PartialEq impl)  │
│ Function ==           │ No (solo nil)        │ Si (punteros)        │ No                   │
│ Custom ==             │ Metodo Equal manual  │ Manual siempre       │ derive o impl manual │
│ Deep comparison       │ reflect.DeepEqual    │ No (manual)          │ PartialEq recursivo  │
│ Performance           │ Manual >> reflect     │ memcmp ≈ manual      │ derive ≈ manual      │
│ Compile-time check    │ Si (comparable)      │ No                   │ Si (trait bounds)    │
│ NaN handling          │ NaN != NaN (float)   │ NaN != NaN           │ NaN != NaN (no Eq)   │
│ nil/null comparacion  │ Si (== nil)          │ Si (== NULL)         │ Option == None       │
│ Test assertion        │ reflect.DeepEqual    │ assert + manual      │ assert_eq! (deriv.)  │
│                       │ go-cmp (externo)     │                      │                      │
│ Map key constraint    │ comparable           │ N/A (manual hash)    │ Eq + Hash traits     │
│ Zero-cost             │ reflect: No          │ memcmp: Si           │ derive: Si           │
│                       │ manual: Si           │                      │                      │
└──────────────────────┴──────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 13. Programa completo: Configuration State Comparator

Herramienta que compara dos estados de configuracion de infraestructura, detecta cambios campo por campo, genera un reporte de diferencias, y determina si se necesita un re-deploy.

```go
package main

import (
	"cmp"
	"encoding/json"
	"fmt"
	"maps"
	"reflect"
	"slices"
	"strings"
)

// ─── Domain Types ───────────────────────────────────────────────

type Port struct {
	Number   int    `json:"number"`
	Protocol string `json:"protocol"`
}

type ServiceSpec struct {
	Name       string            `json:"name"`
	Image      string            `json:"image"`
	Replicas   int               `json:"replicas"`
	Ports      []Port            `json:"ports"`
	Env        map[string]string `json:"env,omitempty"`
	Labels     map[string]string `json:"labels,omitempty"`
	Command    []string          `json:"command,omitempty"`
	MemoryMB   int               `json:"memory_mb"`
	CPUMillis  int               `json:"cpu_millis"`
	HealthPath string            `json:"health_path,omitempty"`
}

// ─── Comparison Types ───────────────────────────────────────────

type FieldChange struct {
	Path     string `json:"path"`
	OldValue string `json:"old_value"`
	NewValue string `json:"new_value"`
	Severity string `json:"severity"` // "info", "warning", "critical"
}

type ServiceDiff struct {
	Name    string        `json:"name"`
	Status  string        `json:"status"` // "unchanged", "modified", "added", "removed"
	Changes []FieldChange `json:"changes,omitempty"`
}

// ─── Port comparison ────────────────────────────────────────────

// Port es comparable (solo campos primitivos)
// Podemos usar == directamente
func portsEqual(a, b []Port) bool {
	return slices.EqualFunc(a, b, func(x, y Port) bool {
		return x == y // Port es comparable
	})
}

func portsDiff(name string, old, new []Port) []FieldChange {
	if portsEqual(old, new) {
		return nil
	}

	var changes []FieldChange

	// Indexar por numero
	oldByNum := make(map[int]Port)
	for _, p := range old {
		oldByNum[p.Number] = p
	}
	newByNum := make(map[int]Port)
	for _, p := range new {
		newByNum[p.Number] = p
	}

	// Ports removed
	for num, op := range oldByNum {
		if _, exists := newByNum[num]; !exists {
			changes = append(changes, FieldChange{
				Path:     fmt.Sprintf("ports[%d]", num),
				OldValue: fmt.Sprintf("%d/%s", op.Number, op.Protocol),
				NewValue: "(removed)",
				Severity: "warning",
			})
		}
	}

	// Ports added or changed
	for num, np := range newByNum {
		op, exists := oldByNum[num]
		if !exists {
			changes = append(changes, FieldChange{
				Path:     fmt.Sprintf("ports[%d]", num),
				OldValue: "(new)",
				NewValue: fmt.Sprintf("%d/%s", np.Number, np.Protocol),
				Severity: "warning",
			})
		} else if op != np { // Struct comparison with ==
			changes = append(changes, FieldChange{
				Path:     fmt.Sprintf("ports[%d].protocol", num),
				OldValue: op.Protocol,
				NewValue: np.Protocol,
				Severity: "warning",
			})
		}
	}

	return changes
}

// ─── Map diff ───────────────────────────────────────────────────

func mapDiff(prefix string, old, new map[string]string, severity string) []FieldChange {
	if maps.Equal(old, new) {
		return nil
	}

	var changes []FieldChange

	// Check each old key
	for k, v1 := range old {
		if v2, ok := new[k]; !ok {
			changes = append(changes, FieldChange{
				Path: prefix + "." + k, OldValue: v1, NewValue: "(removed)", Severity: severity,
			})
		} else if v1 != v2 {
			changes = append(changes, FieldChange{
				Path: prefix + "." + k, OldValue: v1, NewValue: v2, Severity: severity,
			})
		}
	}
	// Check added keys
	for k, v2 := range new {
		if _, ok := old[k]; !ok {
			changes = append(changes, FieldChange{
				Path: prefix + "." + k, OldValue: "(new)", NewValue: v2, Severity: severity,
			})
		}
	}

	slices.SortFunc(changes, func(a, b FieldChange) int {
		return cmp.Compare(a.Path, b.Path)
	})
	return changes
}

// ─── Service comparison ─────────────────────────────────────────

func (s ServiceSpec) Equal(other ServiceSpec) bool {
	// Campos comparables: comparacion directa
	if s.Name != other.Name || s.Image != other.Image ||
		s.Replicas != other.Replicas || s.MemoryMB != other.MemoryMB ||
		s.CPUMillis != other.CPUMillis || s.HealthPath != other.HealthPath {
		return false
	}
	// Slices
	if !portsEqual(s.Ports, other.Ports) {
		return false
	}
	if !slices.Equal(s.Command, other.Command) {
		return false
	}
	// Maps
	if !maps.Equal(s.Env, other.Env) {
		return false
	}
	if !maps.Equal(s.Labels, other.Labels) {
		return false
	}
	return true
}

func compareService(old, new ServiceSpec) ServiceDiff {
	diff := ServiceDiff{Name: new.Name}

	if old.Equal(new) {
		diff.Status = "unchanged"
		return diff
	}
	diff.Status = "modified"

	// Compare each field
	if old.Image != new.Image {
		sev := "critical"
		if extractTag(old.Image) != extractTag(new.Image) {
			sev = "critical" // Image change = redeploy
		}
		diff.Changes = append(diff.Changes, FieldChange{
			Path: "image", OldValue: old.Image, NewValue: new.Image, Severity: sev,
		})
	}

	if old.Replicas != new.Replicas {
		diff.Changes = append(diff.Changes, FieldChange{
			Path: "replicas", OldValue: fmt.Sprint(old.Replicas),
			NewValue: fmt.Sprint(new.Replicas), Severity: "warning",
		})
	}

	if old.MemoryMB != new.MemoryMB {
		diff.Changes = append(diff.Changes, FieldChange{
			Path: "memory_mb", OldValue: fmt.Sprint(old.MemoryMB),
			NewValue: fmt.Sprint(new.MemoryMB), Severity: "warning",
		})
	}

	if old.CPUMillis != new.CPUMillis {
		diff.Changes = append(diff.Changes, FieldChange{
			Path: "cpu_millis", OldValue: fmt.Sprint(old.CPUMillis),
			NewValue: fmt.Sprint(new.CPUMillis), Severity: "warning",
		})
	}

	if old.HealthPath != new.HealthPath {
		diff.Changes = append(diff.Changes, FieldChange{
			Path: "health_path", OldValue: old.HealthPath,
			NewValue: new.HealthPath, Severity: "info",
		})
	}

	diff.Changes = append(diff.Changes, portsDiff(new.Name, old.Ports, new.Ports)...)
	diff.Changes = append(diff.Changes, mapDiff("env", old.Env, new.Env, "warning")...)
	diff.Changes = append(diff.Changes, mapDiff("labels", old.Labels, new.Labels, "info")...)

	if !slices.Equal(old.Command, new.Command) {
		diff.Changes = append(diff.Changes, FieldChange{
			Path: "command", OldValue: strings.Join(old.Command, " "),
			NewValue: strings.Join(new.Command, " "), Severity: "critical",
		})
	}

	return diff
}

func extractTag(image string) string {
	if i := strings.LastIndex(image, ":"); i >= 0 {
		return image[i+1:]
	}
	return "latest"
}

// ─── Fleet comparison ───────────────────────────────────────────

func compareFleet(old, new []ServiceSpec) []ServiceDiff {
	oldMap := make(map[string]ServiceSpec, len(old))
	for _, s := range old {
		oldMap[s.Name] = s
	}
	newMap := make(map[string]ServiceSpec, len(new))
	for _, s := range new {
		newMap[s.Name] = s
	}

	var diffs []ServiceDiff

	// Modified and removed
	for _, o := range old {
		if n, exists := newMap[o.Name]; exists {
			diffs = append(diffs, compareService(o, n))
		} else {
			diffs = append(diffs, ServiceDiff{
				Name:   o.Name,
				Status: "removed",
			})
		}
	}

	// Added
	for _, n := range new {
		if _, exists := oldMap[n.Name]; !exists {
			diffs = append(diffs, ServiceDiff{
				Name:   n.Name,
				Status: "added",
			})
		}
	}

	slices.SortFunc(diffs, func(a, b ServiceDiff) int {
		statusOrder := map[string]int{"removed": 0, "added": 1, "modified": 2, "unchanged": 3}
		if c := cmp.Compare(statusOrder[a.Status], statusOrder[b.Status]); c != 0 {
			return c
		}
		return cmp.Compare(a.Name, b.Name)
	})

	return diffs
}

// ─── Display ────────────────────────────────────────────────────

func printDiff(diffs []ServiceDiff) {
	for _, d := range diffs {
		icon := map[string]string{
			"unchanged": "=", "modified": "~",
			"added": "+", "removed": "-",
		}[d.Status]

		fmt.Printf("\n  [%s] %s (%s)\n", icon, d.Name, d.Status)

		for _, c := range d.Changes {
			sevIcon := map[string]string{
				"info": " ", "warning": "!", "critical": "!!",
			}[c.Severity]
			fmt.Printf("      %2s %-25s %s → %s\n", sevIcon, c.Path, c.OldValue, c.NewValue)
		}
	}
}

func needsRedeploy(diffs []ServiceDiff) []string {
	var services []string
	for _, d := range diffs {
		if d.Status == "added" || d.Status == "removed" {
			services = append(services, d.Name)
			continue
		}
		for _, c := range d.Changes {
			if c.Severity == "critical" {
				services = append(services, d.Name)
				break
			}
		}
	}
	return services
}

// ─── Main ───────────────────────────────────────────────────────

func main() {
	fmt.Println(strings.Repeat("═", 70))
	fmt.Println("  CONFIGURATION STATE COMPARATOR")
	fmt.Println(strings.Repeat("═", 70))

	// Current state
	current := []ServiceSpec{
		{
			Name: "api-server", Image: "api:2.1.0", Replicas: 3,
			Ports:    []Port{{8080, "tcp"}, {8443, "tcp"}},
			Env:      map[string]string{"LOG_LEVEL": "info", "DB_HOST": "db01"},
			Labels:   map[string]string{"tier": "backend", "env": "prod"},
			Command:  []string{"./api", "--config", "/etc/api.yaml"},
			MemoryMB: 512, CPUMillis: 500, HealthPath: "/healthz",
		},
		{
			Name: "web-frontend", Image: "web:3.0.0", Replicas: 2,
			Ports:    []Port{{80, "tcp"}, {443, "tcp"}},
			Env:      map[string]string{"API_URL": "http://api:8080"},
			Labels:   map[string]string{"tier": "frontend", "env": "prod"},
			MemoryMB: 256, CPUMillis: 250, HealthPath: "/",
		},
		{
			Name: "postgres", Image: "postgres:14.2", Replicas: 1,
			Ports:    []Port{{5432, "tcp"}},
			Env:      map[string]string{"POSTGRES_DB": "app"},
			Labels:   map[string]string{"tier": "data", "env": "prod"},
			MemoryMB: 1024, CPUMillis: 1000,
		},
		{
			Name: "old-worker", Image: "worker:1.0.0", Replicas: 1,
			Ports:    []Port{{9090, "tcp"}},
			MemoryMB: 128, CPUMillis: 100,
		},
	}

	// Desired state
	desired := []ServiceSpec{
		{
			Name: "api-server", Image: "api:2.2.0", Replicas: 5, // image + replicas changed
			Ports:    []Port{{8080, "tcp"}, {8443, "tcp"}, {9090, "tcp"}}, // port added
			Env:      map[string]string{"LOG_LEVEL": "debug", "DB_HOST": "db01", "CACHE": "redis:6379"}, // env changed + added
			Labels:   map[string]string{"tier": "backend", "env": "prod", "version": "2.2.0"}, // label added
			Command:  []string{"./api", "--config", "/etc/api.yaml"},
			MemoryMB: 1024, CPUMillis: 500, HealthPath: "/healthz",
		},
		{
			Name: "web-frontend", Image: "web:3.0.0", Replicas: 2,
			Ports:    []Port{{80, "tcp"}, {443, "tcp"}},
			Env:      map[string]string{"API_URL": "http://api:8080"},
			Labels:   map[string]string{"tier": "frontend", "env": "prod"},
			MemoryMB: 256, CPUMillis: 250, HealthPath: "/",
		},
		{
			Name: "postgres", Image: "postgres:14.2", Replicas: 2, // replicas changed
			Ports:    []Port{{5432, "tcp"}},
			Env:      map[string]string{"POSTGRES_DB": "app"},
			Labels:   map[string]string{"tier": "data", "env": "prod"},
			MemoryMB: 2048, CPUMillis: 1000, // memory changed
		},
		{
			Name: "redis", Image: "redis:7.2", Replicas: 1, // NEW service
			Ports:    []Port{{6379, "tcp"}},
			Labels:   map[string]string{"tier": "data", "env": "prod"},
			MemoryMB: 512, CPUMillis: 250,
		},
	}

	// ═══ Compare ═══
	diffs := compareFleet(current, desired)

	fmt.Println("\n── Diff Report ──")
	printDiff(diffs)

	// ═══ Summary ═══
	fmt.Println("\n── Summary ──")
	statusCount := make(map[string]int)
	totalChanges := 0
	for _, d := range diffs {
		statusCount[d.Status]++
		totalChanges += len(d.Changes)
	}
	for _, status := range []string{"added", "removed", "modified", "unchanged"} {
		if c := statusCount[status]; c > 0 {
			fmt.Printf("  %-12s %d services\n", status, c)
		}
	}
	fmt.Printf("  Total field changes: %d\n", totalChanges)

	// ═══ Redeploy needed? ═══
	redeploy := needsRedeploy(diffs)
	fmt.Println("\n── Requires Redeploy ──")
	if len(redeploy) == 0 {
		fmt.Println("  No redeploys needed")
	} else {
		for _, name := range redeploy {
			fmt.Printf("  → %s\n", name)
		}
	}

	// ═══ Comparison methods demo ═══
	fmt.Println("\n── Comparison Methods Used ──")

	// Port is comparable → == works
	p1 := Port{80, "tcp"}
	p2 := Port{80, "tcp"}
	fmt.Printf("\n  Port == Port:              %v (struct comparable)\n", p1 == p2)

	// ServiceSpec is NOT comparable → needs Equal method
	fmt.Printf("  ServiceSpec.Equal():       %v (custom method with slices/maps)\n",
		current[1].Equal(desired[1]))

	// reflect.DeepEqual works on anything
	fmt.Printf("  reflect.DeepEqual:         %v (universal, slower)\n",
		reflect.DeepEqual(current[1], desired[1]))

	// slices.Equal for comparable slices
	fmt.Printf("  slices.Equal(commands):    %v\n",
		slices.Equal(current[0].Command, desired[0].Command))

	// slices.EqualFunc for non-comparable element slices
	fmt.Printf("  slices.EqualFunc(ports):   %v\n",
		slices.EqualFunc(current[0].Ports, desired[0].Ports, func(a, b Port) bool { return a == b }))

	// maps.Equal for maps
	fmt.Printf("  maps.Equal(labels):        %v\n",
		maps.Equal(current[0].Labels, desired[0].Labels))

	fmt.Println("\n── Patterns ──")
	patterns := []string{
		"Port{} == Port{}            — struct with only comparable fields",
		"ServiceSpec.Equal(other)    — custom method, ignores nothing",
		"ServiceSpec.Diff(other)     — field-by-field change detection",
		"portsEqual() via EqualFunc  — comparing []Port (non-comparable slice element)",
		"mapDiff() via maps.Equal    — detecting map changes (add/remove/modify)",
		"slices.Equal(commands)      — comparing []string (comparable elements)",
		"compareFleet() set-based    — detect added/removed/modified services",
		"needsRedeploy() by severity — critical changes trigger redeploy",
	}
	for _, p := range patterns {
		fmt.Printf("  • %s\n", p)
	}
}
```

---

## 14. Tabla de errores comunes

```
┌────┬──────────────────────────────────────────┬──────────────────────────────────────────┬─────────┐
│ #  │ Error                                    │ Solucion                                 │ Nivel   │
├────┼──────────────────────────────────────────┼──────────────────────────────────────────┼─────────┤
│  1 │ Comparar slices con == (compile error)   │ slices.Equal o reflect.DeepEqual          │ Compile │
│  2 │ Comparar maps con == (compile error)     │ maps.Equal o reflect.DeepEqual            │ Compile │
│  3 │ == en interface{} con valor no-comparable│ panic en runtime — verificar tipo primero  │ Panic   │
│  4 │ NaN == NaN es false                      │ Usar math.IsNaN() para detectar NaN       │ Logic   │
│  5 │ reflect.DeepEqual(nil, []int{}) = false  │ slices.Equal trata nil == empty            │ Logic   │
│  6 │ Comparar punteros (compara direccion)    │ Dereferir: *p1 == *p2 para comparar valor │ Logic   │
│  7 │ interface nil vs interface con nil value  │ i == nil verifica ambos tipo y valor       │ Logic   │
│  8 │ reflect.DeepEqual en hot path (lento)    │ Metodo Equal manual o slices/maps.Equal    │ Perf    │
│  9 │ Struct con []T usado como map key        │ Derivar key comparable (string o struct)   │ Compile │
│ 10 │ Comparar structs de diferente tipo       │ Conversion explicita si campos identicos   │ Compile │
│ 11 │ memcmp-style en Go (no existe)           │ No hay equivalente — siempre campo a campo │ Design  │
│ 12 │ Olvidar actualizar Equal al agregar campo│ Test que Equal cubre todos los campos      │ Maint.  │
└────┴──────────────────────────────────────────┴──────────────────────────────────────────┴─────────┘
```

---

## 15. Ejercicios de prediccion

**Ejercicio 1**: ¿Que imprime?

```go
type Point struct{ X, Y int }
p1 := Point{1, 2}
p2 := Point{1, 2}
fmt.Println(p1 == p2)
```

<details>
<summary>Respuesta</summary>

```
true
```

`Point` tiene solo campos `int` (comparables). `==` compara todos los campos: `X == X` y `Y == Y`.
</details>

**Ejercicio 2**: ¿Compila?

```go
type Config struct {
    Name string
    Tags []string
}
c1 := Config{"web", []string{"prod"}}
c2 := Config{"web", []string{"prod"}}
fmt.Println(c1 == c2)
```

<details>
<summary>Respuesta</summary>

No compila. Error: `struct containing []string cannot be compared using ==`. El campo `Tags` es un slice, que no es comparable.
</details>

**Ejercicio 3**: ¿Que imprime?

```go
a := &Point{1, 2}
b := &Point{1, 2}
fmt.Println(a == b)
fmt.Println(*a == *b)
```

<details>
<summary>Respuesta</summary>

```
false
true
```

`a == b` compara direcciones de memoria (diferentes alocaciones → false). `*a == *b` compara los valores apuntados (mismos campos → true).
</details>

**Ejercicio 4**: ¿Que imprime?

```go
var s1 []int
s2 := []int{}
fmt.Println(reflect.DeepEqual(s1, s2))
fmt.Println(slices.Equal(s1, s2))
```

<details>
<summary>Respuesta</summary>

```
false
true
```

`reflect.DeepEqual` distingue nil de empty (nil != `[]int{}`). `slices.Equal` trata ambos como iguales (ambos tienen len 0).
</details>

**Ejercicio 5**: ¿Que imprime?

```go
var i1 interface{} = 42
var i2 interface{} = int64(42)
fmt.Println(i1 == i2)
```

<details>
<summary>Respuesta</summary>

```
false
```

Las interfaces comparan tipo + valor. `i1` tiene tipo dinamico `int`, `i2` tiene tipo `int64`. Tipos diferentes → false, aunque el valor numerico sea el mismo.
</details>

**Ejercicio 6**: ¿Que pasa?

```go
var i1 interface{} = []int{1, 2}
var i2 interface{} = []int{1, 2}
fmt.Println(i1 == i2)
```

<details>
<summary>Respuesta</summary>

Panic en runtime: `comparing uncomparable type []int`. El compilador no puede verificar comparabilidad de `interface{}` en compile time. En runtime, descubre que `[]int` no es comparable y hace panic.
</details>

**Ejercicio 7**: ¿Que imprime?

```go
import "math"
nan := math.NaN()
fmt.Println(nan == nan)
fmt.Println(math.IsNaN(nan))
```

<details>
<summary>Respuesta</summary>

```
false
true
```

IEEE 754: NaN no es igual a si mismo. `nan == nan` es false. `math.IsNaN()` es la forma correcta de detectar NaN.
</details>

**Ejercicio 8**: ¿Que imprime?

```go
a := [3]int{1, 2, 3}
b := [3]int{1, 2, 3}
fmt.Println(a == b)

m := map[[3]int]string{a: "hello"}
fmt.Println(m[b])
```

<details>
<summary>Respuesta</summary>

```
true
hello
```

Arrays son comparables y pueden ser keys de map. `a == b` es true. `m[b]` encuentra `m[a]` porque `a == b`.
</details>

**Ejercicio 9**: ¿Que imprime?

```go
type S struct{ X int }
var p *S
var i interface{} = p
fmt.Println(p == nil)
fmt.Println(i == nil)
```

<details>
<summary>Respuesta</summary>

```
true
false
```

`p` es un puntero nil → `p == nil` es true. `i` es una interfaz con tipo dinamico `*S` y valor nil → `i == nil` es false. Una interfaz solo es nil si tanto tipo como valor son nil.
</details>

**Ejercicio 10**: ¿Que imprime?

```go
m1 := map[string]int{"a": 1, "b": 2}
m2 := map[string]int{"b": 2, "a": 1}
fmt.Println(maps.Equal(m1, m2))
```

<details>
<summary>Respuesta</summary>

```
true
```

`maps.Equal` compara contenido, no orden (los maps no tienen orden). Mismas keys con mismos valores → true.
</details>

---

## 16. Resumen visual

```
┌──────────────────────────────────────────────────────────────┐
│              COMPARACION — RESUMEN                           │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  COMPARABLE (== funciona):                                   │
│  ├─ bool, int*, uint*, float*, complex*, string, rune        │
│  ├─ pointer (*T) — compara DIRECCION, no valor               │
│  ├─ channel, interface                                       │
│  ├─ array [N]T — si T comparable                             │
│  └─ struct — si TODOS los campos comparables                 │
│                                                              │
│  NO COMPARABLE (== es error):                                │
│  ├─ slice ([]T) — solo == nil                                │
│  ├─ map (map[K]V) — solo == nil                              │
│  └─ func — solo == nil                                       │
│                                                              │
│  HERRAMIENTAS DE COMPARACION                                 │
│  ┌────────────────────────┬──────────┬───────────┬──────────┐│
│  │ Herramienta            │ Speed    │ Type-safe │ Coverage ││
│  ├────────────────────────┼──────────┼───────────┼──────────┤│
│  │ == (nativo)            │ Fastest  │ Yes       │ Compar.  ││
│  │ Equal() manual         │ Fast     │ Yes       │ Custom   ││
│  │ slices.Equal (1.21+)   │ Fast     │ Yes       │ []T comp.││
│  │ slices.EqualFunc       │ Fast     │ Yes       │ []T any  ││
│  │ maps.Equal (1.21+)     │ Fast     │ Yes       │ map comp.││
│  │ maps.EqualFunc         │ Fast     │ Yes       │ map any  ││
│  │ reflect.DeepEqual      │ ~100x ↓  │ No        │ Universal││
│  │ go-cmp (externo)       │ Medium   │ Config.   │ Universal││
│  └────────────────────────┴──────────┴───────────┴──────────┘│
│                                                              │
│  TRAMPAS                                                     │
│  ├─ NaN != NaN                  → math.IsNaN()               │
│  ├─ ptr == ptr compara dir.     → *p1 == *p2 para valor      │
│  ├─ interface con nil valor     → i == nil es false           │
│  ├─ reflect.DeepEqual(nil,[])   → false (distingue nil/empty)│
│  ├─ slices.Equal(nil, [])       → true (ambos len 0)         │
│  └─ == en interface{} con []T   → panic runtime              │
│                                                              │
│  MAP KEYS                                                    │
│  Solo tipos comparables pueden ser keys de map               │
│  Struct key: map[StructKey]V (si struct comparable)          │
│  Para no-comparable: derivar key string/int                  │
│                                                              │
│  GENERICS                                                    │
│  comparable constraint: [T comparable] permite ==            │
│  any constraint: [T any] NO permite ==                       │
│                                                              │
│  EN TESTS                                                    │
│  ├─ reflect.DeepEqual — simple, lento, OK para tests         │
│  ├─ go-cmp — diff legible, opciones de ignore/sort           │
│  └─ Equal() custom — mas rapido, explicito                   │
│                                                              │
│  EN PRODUCCION                                               │
│  ├─ Equal() manual o slices/maps.Equal — rapido, type-safe   │
│  ├─ Diff() para detectar cambios campo a campo               │
│  └─ Nunca reflect.DeepEqual en hot paths                     │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: C06/S01/T01 - Interfaces implicitas — satisfaccion sin declaracion explicita, duck typing estatico
