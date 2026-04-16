# Funciones genericas — type parameters, constraints, comparable, any

## 1. Introduccion

Antes de Go 1.18 (marzo 2022), Go no tenia genericos. Si querias una funcion que operara sobre multiples tipos, tenias tres opciones malas:

1. **Duplicar codigo** — escribir `MaxInt`, `MaxFloat64`, `MaxString` por separado
2. **Usar `interface{}`** — perder type safety, necesitar type assertions en runtime
3. **Usar generacion de codigo** — `go generate` con herramientas como `genny`

Go 1.18 introdujo **type parameters** (genericos) con un diseno deliberadamente minimalista: resuelve el 90% de los casos con la menor complejidad posible. No es el sistema de genericos de Rust (traits, associated types, const generics, GATs) ni el de Java (wildcards, variance, type erasure) — es algo mas simple, con limitaciones conscientes.

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                    GENERICOS EN GO — VISION GENERAL                           │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                │
│  ANTES de 1.18                         DESPUES de 1.18                        │
│  ─────────────────                     ──────────────────                     │
│                                                                                │
│  func MaxInt(a, b int) int {           func Max[T cmp.Ordered](a, b T) T {   │
│      if a > b { return a }                 if a > b { return a }              │
│      return b                              return b                           │
│  }                                     }                                      │
│  func MaxFloat(a, b float64) float64 {                                       │
│      if a > b { return a }             // UNA funcion para todos los tipos    │
│      return b                          Max(42, 17)         // int             │
│  }                                     Max(3.14, 2.71)     // float64         │
│  func MaxStr(a, b string) string {     Max("zebra", "ant") // string          │
│      if a > b { return a }                                                    │
│      return b                                                                 │
│  }                                                                            │
│  // ...y asi para cada tipo            // cmp.Ordered permite <, >, <=, >=   │
│                                                                                │
│  ┌─────────────────────────────────────────────────────────────────────┐      │
│  │  TERMINOLOGIA                                                       │      │
│  │                                                                     │      │
│  │  func Max[T cmp.Ordered](a, b T) T                                 │      │
│  │       ─── ─ ───────────  ────── ─                                   │      │
│  │       │   │     │         │     └── tipo de retorno (usa T)        │      │
│  │       │   │     │         └── parametros regulares (tipo T)        │      │
│  │       │   │     └── constraint (que puede hacer T)                │      │
│  │       │   └── type parameter (nombre del tipo generico)           │      │
│  │       └── nombre de la funcion                                    │      │
│  │                                                                     │      │
│  │  Max[int](42, 17)     ← type argument explicito                   │      │
│  │  Max(42, 17)          ← type argument inferido (preferido)        │      │
│  └─────────────────────────────────────────────────────────────────────┘      │
│                                                                                │
│  MECANISMO DE COMPILACION                                                     │
│  ────────────────────────                                                     │
│  Go usa un hibrido de monomorphization y dictionary-passing:                  │
│  • Para tipos de tamano conocido (int, float64): genera codigo especializado │
│  • Para tipos via interface: usa diccionarios de metodos (vtable-like)        │
│  • Resultado: menos code bloat que C++ templates, mas rapido que Java erasure│
└────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Sintaxis basica de funciones genericas

### 2.1 Funcion generica minimal

```go
package main

import "fmt"

// Una funcion generica tiene type parameters entre corchetes []
// T es el type parameter, any es la constraint (acepta cualquier tipo)
func Print[T any](value T) {
    fmt.Println(value)
}

func main() {
    Print[int](42)          // type argument explicito
    Print[string]("hello")  // type argument explicito
    Print(3.14)             // type argument inferido → float64
    Print(true)             // type argument inferido → bool

    // El compilador infiere T del argumento
    // Casi nunca necesitas especificarlo manualmente
}
```

### 2.2 Multiples type parameters

```go
// Dos type parameters independientes
func Convert[From any, To any](value From, converter func(From) To) To {
    return converter(value)
}

// Uso
result := Convert(42, func(n int) string {
    return fmt.Sprintf("numero: %d", n)
})
fmt.Println(result) // "numero: 42"

// Tres type parameters
func Map[In any, Mid any, Out any](
    input In,
    first func(In) Mid,
    second func(Mid) Out,
) Out {
    return second(first(input))
}
```

### 2.3 Type parameter en retorno solamente

```go
// T aparece solo en el retorno — no se puede inferir, debe ser explicito
func Zero[T any]() T {
    var zero T
    return zero
}

// DEBE especificar el type argument
n := Zero[int]()        // 0
s := Zero[string]()     // ""
b := Zero[bool]()       // false
p := Zero[*int]()       // nil

// Esto NO compila:
// n := Zero()  // error: cannot infer T
```

### 2.4 Funciones genericas como valores

```go
// Las funciones genericas NO pueden usarse como valores directamente
func identity[T any](v T) T { return v }

// Esto NO compila:
// fn := identity  // error: cannot use generic function without instantiation

// Debes instanciarla primero
fn := identity[int] // OK: fn es func(int) int
fmt.Println(fn(42)) // 42

// Esto limita el uso de genericos con funciones de orden superior
// comparado con Rust donde closures genericas son comunes
```

### 2.5 Funciones genericas variadicas

```go
// Variadic con type parameter
func PrintAll[T any](values ...T) {
    for _, v := range values {
        fmt.Println(v)
    }
}

PrintAll(1, 2, 3)           // T = int
PrintAll("a", "b", "c")     // T = string

// IMPORTANTE: todos los argumentos deben ser del MISMO tipo
// Esto NO compila:
// PrintAll(1, "two", 3.0)  // error: no puedo inferir T

// Para tipos mixtos, necesitas any
func PrintAny(values ...any) {
    for _, v := range values {
        fmt.Println(v)
    }
}
PrintAny(1, "two", 3.0) // OK, pero pierde type safety
```

---

## 3. Constraints: que puede hacer T

### 3.1 La constraint `any`

```go
// any es un alias para interface{} — acepta cualquier tipo
// Pero como acepta cualquier tipo, no puedes hacer NADA con T
// excepto asignarlo, pasarlo, compararlo con nil (si es puntero)

func Store[T any](value T) *T {
    v := value // OK: asignar
    return &v  // OK: tomar direccion
}

func BadAdd[T any](a, b T) T {
    // return a + b  // ERROR: operator + not defined on T
    // return a < b  // ERROR: operator < not defined on T
    // return a.Len() // ERROR: T has no method Len
    var zero T
    return zero
}
```

### 3.2 La constraint `comparable`

```go
// comparable es un constraint built-in que permite == y !=
// Incluye: todos los tipos basicos, arrays, structs cuyos campos son comparable
// NO incluye: slices, maps, funciones

func Contains[T comparable](slice []T, target T) bool {
    for _, v := range slice {
        if v == target { // OK: comparable permite ==
            return true
        }
    }
    return false
}

func Index[T comparable](slice []T, target T) int {
    for i, v := range slice {
        if v == target {
            return i
        }
    }
    return -1
}

func Unique[T comparable](slice []T) []T {
    seen := make(map[T]struct{}) // OK: comparable se puede usar como map key
    result := make([]T, 0)
    for _, v := range slice {
        if _, ok := seen[v]; !ok {
            seen[v] = struct{}{}
            result = append(result, v)
        }
    }
    return result
}

func main() {
    fmt.Println(Contains([]int{1, 2, 3}, 2))           // true
    fmt.Println(Contains([]string{"a", "b"}, "c"))      // false
    fmt.Println(Unique([]int{1, 2, 2, 3, 3, 3}))       // [1 2 3]

    // Esto NO compila:
    // Contains([][]int{{1}, {2}}, []int{1})
    // error: []int does not satisfy comparable
}
```

### 3.3 Interfaces como constraints

```go
// Cualquier interface puede usarse como constraint
// La interface define que metodos debe tener T

type Stringer interface {
    String() string
}

func Join[T Stringer](items []T, sep string) string {
    var result string
    for i, item := range items {
        if i > 0 {
            result += sep
        }
        result += item.String() // OK: T tiene metodo String()
    }
    return result
}

// Tipo que implementa Stringer
type Color struct {
    R, G, B uint8
}

func (c Color) String() string {
    return fmt.Sprintf("#%02x%02x%02x", c.R, c.G, c.B)
}

func main() {
    colors := []Color{
        {255, 0, 0},
        {0, 255, 0},
        {0, 0, 255},
    }
    fmt.Println(Join(colors, ", ")) // "#ff0000, #00ff00, #0000ff"
}
```

### 3.4 Constraint con multiples metodos

```go
// Un constraint puede requerir multiples metodos
type ReadWriter interface {
    Read(p []byte) (n int, err error)
    Write(p []byte) (n int, err error)
}

func Copy[T ReadWriter](dst, src T) error {
    buf := make([]byte, 1024)
    for {
        n, err := src.Read(buf)
        if n > 0 {
            if _, werr := dst.Write(buf[:n]); werr != nil {
                return werr
            }
        }
        if err != nil {
            if err == io.EOF {
                return nil
            }
            return err
        }
    }
}

// Interface embebida como constraint
type Comparable interface {
    comparable        // puede usar == y !=
    String() string   // tiene metodo String
}
```

### 3.5 `cmp.Ordered` — el constraint mas util

```go
import "cmp"

// cmp.Ordered es un constraint predefinido en la stdlib (Go 1.21+)
// Permite: <, >, <=, >=, ==, !=
// Incluye: todos los tipos numericos + string

func Max[T cmp.Ordered](a, b T) T {
    if a > b {
        return a
    }
    return b
}

func Min[T cmp.Ordered](a, b T) T {
    if a < b {
        return a
    }
    return b
}

func Clamp[T cmp.Ordered](value, min, max T) T {
    if value < min {
        return min
    }
    if value > max {
        return max
    }
    return value
}

func main() {
    fmt.Println(Max(10, 20))         // 20
    fmt.Println(Min(3.14, 2.71))     // 2.71
    fmt.Println(Max("zebra", "ant")) // "zebra"
    fmt.Println(Clamp(150, 0, 100))  // 100
}
```

### 3.6 Definicion de cmp.Ordered (como funciona internamente)

```go
// En el paquete cmp (Go 1.21+), Ordered se define asi:
type Ordered interface {
    ~int | ~int8 | ~int16 | ~int32 | ~int64 |
    ~uint | ~uint8 | ~uint16 | ~uint32 | ~uint64 | ~uintptr |
    ~float32 | ~float64 |
    ~string
}

// El ~ (tilde) significa "este tipo O cualquier tipo definido basado en el"
// Los | son union types — T puede ser CUALQUIERA de estos

// Sin ~:  int     → solo el tipo built-in int
// Con ~:  ~int    → int Y cualquier type MyInt int

type Celsius float64   // tipo definido basado en float64
type Score int         // tipo definido basado en int

// Sin ~, Celsius NO satisfaria Ordered (no es float64, es Celsius)
// Con ~, Celsius SI satisface Ordered (su underlying type es float64)

func main() {
    fmt.Println(Max(Celsius(36.5), Celsius(37.2))) // 37.2 — funciona por ~float64
    fmt.Println(Max(Score(85), Score(92)))           // 92 — funciona por ~int
}
```

---

## 4. Type inference (inferencia de tipos)

### 4.1 Como funciona la inferencia

```go
// El compilador infiere type arguments de los argumentos de la funcion

func First[T any](slice []T) T {
    return slice[0]
}

// El compilador ve []int, infiere T = int
result := First([]int{1, 2, 3})  // T inferido como int

// Inferencia con multiples type parameters
func Zip[A any, B any](as []A, bs []B) []struct{ A A; B B } {
    min := len(as)
    if len(bs) < min {
        min = len(bs)
    }
    result := make([]struct{ A A; B B }, min)
    for i := 0; i < min; i++ {
        result[i] = struct{ A A; B B }{as[i], bs[i]}
    }
    return result
}

// El compilador infiere A = string, B = int
pairs := Zip([]string{"a", "b"}, []int{1, 2})
// pairs = [{A:"a" B:1}, {A:"b" B:2}]
```

### 4.2 Cuando la inferencia falla

```go
// Caso 1: T no aparece en los parametros
func New[T any]() *T {
    return new(T)
}
// new := New()  // ERROR: cannot infer T
p := New[int]()  // OK: debe ser explicito

// Caso 2: Ambiguedad
func Convert[To any, From any](f From) To {
    // ...
}
// result := Convert(42)  // ERROR: cannot infer To
result := Convert[string](42) // ERROR: From tambien necesita ser explicito
result := Convert[string, int](42) // OK

// Caso 3: Untyped constants
func Double[T cmp.Ordered](v T) T {
    return v + v
}
// Double(42)   // OK: 42 untyped constant, se infiere como int (default)
// Double(3.14) // OK: 3.14 untyped constant, se infiere como float64 (default)
```

### 4.3 Inferencia parcial (Go 1.21+)

```go
// Desde Go 1.21, puedes especificar ALGUNOS type arguments
// y dejar que el compilador infiera el resto

func Transform[In any, Out any](input In, fn func(In) Out) Out {
    return fn(input)
}

// Especificar solo Out, inferir In del argumento
// (Nota: esto funciona en algunos casos, no en todos)
result := Transform(42, func(n int) string {
    return fmt.Sprint(n)
})
// In = int (inferido de 42), Out = string (inferido del retorno de fn)
```

---

## 5. Patrones comunes con funciones genericas

### 5.1 Funciones de slices

```go
package sliceutil

// Filter retorna los elementos que cumplen el predicado
func Filter[T any](slice []T, predicate func(T) bool) []T {
    result := make([]T, 0)
    for _, v := range slice {
        if predicate(v) {
            result = append(result, v)
        }
    }
    return result
}

// Map transforma cada elemento
func Map[In any, Out any](slice []In, transform func(In) Out) []Out {
    result := make([]Out, len(slice))
    for i, v := range slice {
        result[i] = transform(v)
    }
    return result
}

// Reduce acumula un resultado
func Reduce[T any, Acc any](slice []T, initial Acc, fn func(Acc, T) Acc) Acc {
    result := initial
    for _, v := range slice {
        result = fn(result, v)
    }
    return result
}

// Find retorna el primer elemento que cumple el predicado
func Find[T any](slice []T, predicate func(T) bool) (T, bool) {
    for _, v := range slice {
        if predicate(v) {
            return v, true
        }
    }
    var zero T
    return zero, false
}

// GroupBy agrupa elementos por una clave
func GroupBy[T any, K comparable](slice []T, keyFn func(T) K) map[K][]T {
    groups := make(map[K][]T)
    for _, v := range slice {
        key := keyFn(v)
        groups[key] = append(groups[key], v)
    }
    return groups
}
```

```go
package main

import "fmt"

func main() {
    numbers := []int{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}

    // Filter: numeros pares
    evens := Filter(numbers, func(n int) bool { return n%2 == 0 })
    fmt.Println(evens) // [2 4 6 8 10]

    // Map: convertir a strings
    strs := Map(numbers, func(n int) string { return fmt.Sprintf("#%d", n) })
    fmt.Println(strs) // [#1 #2 #3 #4 #5 #6 #7 #8 #9 #10]

    // Reduce: sumar
    sum := Reduce(numbers, 0, func(acc, n int) int { return acc + n })
    fmt.Println(sum) // 55

    // Find: primer numero > 5
    n, found := Find(numbers, func(n int) bool { return n > 5 })
    fmt.Println(n, found) // 6 true

    // Chaining (no es tan ergonomico como en Rust)
    // Duplicar los pares
    result := Map(
        Filter(numbers, func(n int) bool { return n%2 == 0 }),
        func(n int) int { return n * 2 },
    )
    fmt.Println(result) // [4 8 12 16 20]

    // GroupBy
    words := []string{"go", "rust", "zig", "gleam", "roc", "grain"}
    byLen := GroupBy(words, func(s string) int { return len(s) })
    fmt.Println(byLen)
    // map[2:[go] 3:[zig roc] 4:[rust roc] 5:[gleam grain]]

    type Person struct {
        Name string
        Age  int
    }
    people := []Person{
        {"Alice", 30}, {"Bob", 25}, {"Charlie", 30}, {"Diana", 25},
    }
    byAge := GroupBy(people, func(p Person) int { return p.Age })
    fmt.Println(byAge)
    // map[25:[{Bob 25} {Diana 25}] 30:[{Alice 30} {Charlie 30}]]
}
```

### 5.2 La stdlib: paquete slices (Go 1.21+)

```go
import "slices"

// La stdlib incluye funciones genericas para slices desde Go 1.21
// NO necesitas escribir las tuyas para operaciones basicas

numbers := []int{3, 1, 4, 1, 5, 9, 2, 6}

// Sort (in-place)
slices.Sort(numbers)
fmt.Println(numbers) // [1 1 2 3 4 5 6 9]

// SortFunc (con comparador custom)
slices.SortFunc(numbers, func(a, b int) int {
    return b - a // descendente
})
fmt.Println(numbers) // [9 6 5 4 3 2 1 1]

// Contains
fmt.Println(slices.Contains(numbers, 5)) // true
fmt.Println(slices.Contains(numbers, 7)) // false

// Index
fmt.Println(slices.Index(numbers, 5)) // 2 (posicion en slice ordenado desc)

// Min, Max
fmt.Println(slices.Min(numbers)) // 1
fmt.Println(slices.Max(numbers)) // 9

// BinarySearch (slice debe estar ordenado)
slices.Sort(numbers)
pos, found := slices.BinarySearch(numbers, 5)
fmt.Println(pos, found) // 5 true

// Compact (eliminar duplicados consecutivos — sort primero)
slices.Sort(numbers)
numbers = slices.Compact(numbers)
fmt.Println(numbers) // [1 2 3 4 5 6 9]

// Reverse (in-place)
slices.Reverse(numbers)
fmt.Println(numbers) // [9 6 5 4 3 2 1]

// Equal
fmt.Println(slices.Equal([]int{1, 2}, []int{1, 2})) // true
fmt.Println(slices.Equal([]int{1, 2}, []int{2, 1})) // false

// Clone (copia superficial)
copy := slices.Clone(numbers)

// Insert, Delete, Replace
s := []int{1, 2, 5, 6}
s = slices.Insert(s, 2, 3, 4)  // insertar 3,4 en posicion 2
fmt.Println(s)                   // [1 2 3 4 5 6]
s = slices.Delete(s, 1, 3)      // eliminar indices [1,3)
fmt.Println(s)                   // [1 4 5 6]
s = slices.Replace(s, 1, 3, 10, 20) // reemplazar [1,3) con 10,20
fmt.Println(s)                        // [1 10 20 6]

// Grow (pre-allocar capacidad sin cambiar length)
s = slices.Grow(s, 100) // len sin cambio, cap += 100

// Clip (recortar cap a len)
s = slices.Clip(s) // cap == len
```

### 5.3 La stdlib: paquete maps (Go 1.21+)

```go
import "maps"

m := map[string]int{
    "alice": 30,
    "bob":   25,
    "charlie": 35,
}

// Keys — retorna las claves (orden no determinista sin sort)
keys := slices.Sorted(maps.Keys(m))
fmt.Println(keys) // [alice bob charlie]

// Values
vals := slices.Sorted(maps.Values(m))
fmt.Println(vals) // [25 30 35]

// Clone (copia superficial)
m2 := maps.Clone(m)

// Equal
fmt.Println(maps.Equal(m, m2)) // true

// Copy (copia entries de src a dst, sobrescribe existentes)
dst := map[string]int{"alice": 0, "diana": 40}
maps.Copy(dst, m)
// dst = {"alice": 30, "bob": 25, "charlie": 35, "diana": 40}

// DeleteFunc (eliminar entries que cumplan condicion)
maps.DeleteFunc(m, func(k string, v int) bool {
    return v < 30
})
// m = {"alice": 30, "charlie": 35}

// Collect (Go 1.23+ — construir map desde iterador)
// Requiere entender iteradores, que veremos en el futuro
```

### 5.4 Funciones de maps genericas

```go
// Merge combina multiples maps (ultimo gana en conflictos)
func Merge[K comparable, V any](maps ...map[K]V) map[K]V {
    result := make(map[K]V)
    for _, m := range maps {
        for k, v := range m {
            result[k] = v
        }
    }
    return result
}

// MapValues transforma los valores de un map
func MapValues[K comparable, V any, R any](m map[K]V, fn func(V) R) map[K]R {
    result := make(map[K]R, len(m))
    for k, v := range m {
        result[k] = fn(v)
    }
    return result
}

// FilterMap retorna entries que cumplen el predicado
func FilterMap[K comparable, V any](m map[K]V, pred func(K, V) bool) map[K]V {
    result := make(map[K]V)
    for k, v := range m {
        if pred(k, v) {
            result[k] = v
        }
    }
    return result
}

// Invert intercambia keys y values
func Invert[K comparable, V comparable](m map[K]V) map[V]K {
    result := make(map[V]K, len(m))
    for k, v := range m {
        result[v] = k
    }
    return result
}

func main() {
    defaults := map[string]int{"timeout": 30, "retries": 3, "port": 8080}
    overrides := map[string]int{"timeout": 60, "port": 9090}

    config := Merge(defaults, overrides)
    fmt.Println(config) // {timeout:60 retries:3 port:9090}

    doubled := MapValues(config, func(v int) int { return v * 2 })
    fmt.Println(doubled) // {timeout:120 retries:6 port:18180}

    large := FilterMap(config, func(k string, v int) bool { return v > 50 })
    fmt.Println(large) // {timeout:60 port:9090}

    codes := map[string]int{"OK": 200, "NotFound": 404, "Error": 500}
    inverted := Invert(codes)
    fmt.Println(inverted) // {200:OK 404:NotFound 500:Error}
}
```

### 5.5 Funciones utilitarias genericas

```go
// Ptr retorna un puntero al valor — util para literales
func Ptr[T any](v T) *T {
    return &v
}

// Ejemplo: muy util con APIs que esperan *string, *int, etc.
type Config struct {
    Name    *string
    Timeout *int
    Debug   *bool
}

cfg := Config{
    Name:    Ptr("myapp"),    // sin Ptr: s := "myapp"; Name: &s (3 lineas)
    Timeout: Ptr(30),
    Debug:   Ptr(true),
}

// Deref retorna el valor apuntado o un default si nil
func Deref[T any](ptr *T, defaultVal T) T {
    if ptr == nil {
        return defaultVal
    }
    return *ptr
}

name := Deref(cfg.Name, "unnamed")        // "myapp"
var nilPtr *int
timeout := Deref(nilPtr, 60)              // 60 (default)

// Must convierte (T, error) en T, panic si error
func Must[T any](val T, err error) T {
    if err != nil {
        panic(err)
    }
    return val
}

// Util para inicializacion donde el error es fatal
tmpl := Must(template.ParseFiles("index.html"))
re := Must(regexp.Compile(`\d+`))

// Ternary — Go no tiene operador ternario, esta funcion lo simula
// NOTA: a diferencia del ternario real, ambas ramas se evaluan
func Ternary[T any](condition bool, ifTrue, ifFalse T) T {
    if condition {
        return ifTrue
    }
    return ifFalse
}

label := Ternary(count == 1, "item", "items")

// Coalesce — retorna el primer valor no-zero
func Coalesce[T comparable](values ...T) T {
    var zero T
    for _, v := range values {
        if v != zero {
            return v
        }
    }
    return zero
}

name = Coalesce(userInput, envVar, defaultName, "fallback")
```

### 5.6 Funciones con constraints de metodo

```go
// Constraint que requiere un metodo
type Validator interface {
    Validate() error
}

func ValidateAll[T Validator](items []T) []error {
    var errs []error
    for _, item := range items {
        if err := item.Validate(); err != nil {
            errs = append(errs, err)
        }
    }
    return errs
}

// Constraint que combina comparable + metodo
type Named interface {
    comparable
    Name() string
}

func FindByName[T Named](items []T, name string) (T, bool) {
    for _, item := range items {
        if item.Name() == name {
            return item, true
        }
    }
    var zero T
    return zero, false
}
```

### 5.7 Pattern: resultado generico con error

```go
// Result generico — similar a Result<T, E> de Rust (pero en Go)
type Result[T any] struct {
    Value T
    Err   error
}

func Ok[T any](value T) Result[T] {
    return Result[T]{Value: value}
}

func Err[T any](err error) Result[T] {
    return Result[T]{Err: err}
}

func (r Result[T]) Unwrap() T {
    if r.Err != nil {
        panic(r.Err)
    }
    return r.Value
}

func (r Result[T]) UnwrapOr(defaultVal T) T {
    if r.Err != nil {
        return defaultVal
    }
    return r.Value
}

func (r Result[T]) IsOk() bool {
    return r.Err == nil
}

// Nota: este patron NO es idiomatico en Go
// Go prefiere retornar (T, error) directamente
// Se incluye como ejemplo de como genericos permiten tipos parametricos
```

---

## 6. La funcion `cmp.Compare` y `cmp.Or`

### 6.1 cmp.Compare (Go 1.21+)

```go
import "cmp"

// cmp.Compare retorna:
//  -1 si x < y
//   0 si x == y
//  +1 si x > y
// (como strcmp de C, pero generico y tipado)

fmt.Println(cmp.Compare(1, 2))     // -1
fmt.Println(cmp.Compare(2, 2))     //  0
fmt.Println(cmp.Compare(3, 2))     //  1
fmt.Println(cmp.Compare("a", "b")) // -1

// Muy util con slices.SortFunc para sort multi-campo
type Employee struct {
    Department string
    Name       string
    Salary     int
}

employees := []Employee{
    {"Engineering", "Alice", 120000},
    {"Marketing", "Bob", 90000},
    {"Engineering", "Charlie", 110000},
    {"Marketing", "Diana", 95000},
    {"Engineering", "Eve", 120000},
}

// Sort por Department ASC, luego Salary DESC, luego Name ASC
slices.SortFunc(employees, func(a, b Employee) int {
    // Primer criterio: Department
    if c := cmp.Compare(a.Department, b.Department); c != 0 {
        return c
    }
    // Segundo criterio: Salary descendente (invertir)
    if c := cmp.Compare(b.Salary, a.Salary); c != 0 {
        return c // b,a invertido = descendente
    }
    // Tercer criterio: Name
    return cmp.Compare(a.Name, b.Name)
})

for _, e := range employees {
    fmt.Printf("%-12s %-8s %d\n", e.Department, e.Name, e.Salary)
}
// Engineering  Alice    120000
// Engineering  Eve      120000
// Engineering  Charlie  110000
// Marketing    Diana    95000
// Marketing    Bob      90000
```

### 6.2 cmp.Or (Go 1.22+)

```go
// cmp.Or retorna el primer valor que no sea el zero value
// Es como COALESCE de SQL

import "cmp"

// Para tipos Ordered
name := cmp.Or(userInput, envDefault, "fallback")
// Si userInput != "", retorna userInput
// Si envDefault != "", retorna envDefault
// Si no, retorna "fallback"

port := cmp.Or(configPort, envPort, 8080)

// Especialmente util con sort multi-campo (alternativa al patron if c != 0)
slices.SortFunc(employees, func(a, b Employee) int {
    return cmp.Or(
        cmp.Compare(a.Department, b.Department),  // primero por dept
        cmp.Compare(b.Salary, a.Salary),           // luego salary desc
        cmp.Compare(a.Name, b.Name),               // luego nombre
    )
    // cmp.Or retorna el primer resultado != 0
    // Si Department es diferente, retorna esa comparacion
    // Si es igual (0), pasa a Salary
    // Si tambien es igual, pasa a Name
})
```

---

## 7. Constraints avanzados: union types y tilde

### 7.1 Union types en interfaces (Go 1.18+)

```go
// Go 1.18 extendio las interfaces para incluir TYPE ELEMENTS
// Esto permite definir constraints que restringen a tipos especificos

// Interface con union de tipos
type Numeric interface {
    int | int8 | int16 | int32 | int64 |
    uint | uint8 | uint16 | uint32 | uint64 |
    float32 | float64
}

func Sum[T Numeric](numbers []T) T {
    var total T
    for _, n := range numbers {
        total += n // OK: + esta definido para todos los tipos en la union
    }
    return total
}

func main() {
    fmt.Println(Sum([]int{1, 2, 3}))         // 6
    fmt.Println(Sum([]float64{1.1, 2.2}))    // 3.3
    fmt.Println(Sum([]uint8{10, 20, 30}))    // 60

    // Esto NO compila:
    // Sum([]string{"a", "b"}) // string no esta en la union
    // Sum([]bool{true})       // bool no esta en la union
}
```

### 7.2 El operador tilde (~)

```go
// ~ (tilde) significa "tipos cuyo underlying type es..."
// Sin tilde, SOLO el tipo exacto

// Sin tilde: SOLO acepta el tipo built-in int
type ExactInt interface {
    int
}

// Con tilde: acepta int Y cualquier tipo definido sobre int
type AnyInt interface {
    ~int
}

type UserID int    // underlying type: int
type Score int     // underlying type: int
type Meters int    // underlying type: int

func DoubleExact[T ExactInt](v T) T { return v + v }
func DoubleAny[T AnyInt](v T) T { return v + v }

func main() {
    DoubleExact(42)         // OK: int
    // DoubleExact(UserID(1)) // ERROR: UserID no es int

    DoubleAny(42)           // OK: int
    DoubleAny(UserID(1))    // OK: underlying type es int
    DoubleAny(Score(100))   // OK: underlying type es int
    DoubleAny(Meters(5))    // OK: underlying type es int
}
```

### 7.3 Combinando tilde y union types

```go
// Constraint para cualquier tipo signed integer (incluyendo types definidos)
type Signed interface {
    ~int | ~int8 | ~int16 | ~int32 | ~int64
}

// Constraint para cualquier tipo unsigned
type Unsigned interface {
    ~uint | ~uint8 | ~uint16 | ~uint32 | ~uint64 | ~uintptr
}

// Constraint para cualquier tipo numerico
type Number interface {
    Signed | Unsigned | ~float32 | ~float64
}

// Ahora puedes hacer operaciones aritmeticas
func Abs[T Signed](n T) T {
    if n < 0 {
        return -n
    }
    return n
}

func Average[T Number](values []T) float64 {
    if len(values) == 0 {
        return 0
    }
    var sum T
    for _, v := range values {
        sum += v
    }
    return float64(sum) / float64(len(values))
}

type Temperature float64
type Velocity float64

func main() {
    fmt.Println(Abs(-42))              // 42
    fmt.Println(Abs(int8(-127)))       // 127

    type Kelvin int
    fmt.Println(Abs(Kelvin(-10)))      // 10 (funciona por ~int)

    temps := []Temperature{36.5, 37.2, 36.8}
    fmt.Println(Average(temps)) // 36.833...
}
```

### 7.4 Interfaces con tipos y metodos combinados

```go
// Puedes combinar type elements con method requirements

// Un tipo que es numerico Y tiene String()
type PrintableNumber interface {
    ~int | ~float64
    String() string
}

// En la practica, esto es raro — pero posible
type Celsius float64

func (c Celsius) String() string {
    return fmt.Sprintf("%.1f°C", float64(c))
}

func PrintValue[T PrintableNumber](v T) {
    fmt.Println(v.String()) // puede usar String() del constraint
    doubled := v + v        // puede usar + del type element
    _ = doubled
}

// PrintValue(42)            // ERROR: int no tiene String()
PrintValue(Celsius(36.5))   // OK: ~float64 + tiene String()
```

### 7.5 Restricciones sobre type elements

```go
// IMPORTANTE: interfaces con type elements solo pueden usarse como constraints
// NO pueden usarse como tipos de variable

type Numeric interface {
    ~int | ~float64
}

// OK: como constraint
func Add[T Numeric](a, b T) T { return a + b }

// ERROR: no se puede usar como tipo de variable
// var n Numeric = 42  // error: cannot use interface with type elements

// ERROR: no se puede usar como tipo de parametro regular
// func Print(n Numeric) { }  // error

// Esto es una limitacion deliberada de Go:
// Las interfaces con type elements son SOLO para genericos
// Las interfaces regulares (solo metodos) siguen funcionando como antes
```

---

## 8. Funciones genericas vs interfaces: cuando usar cada una

### 8.1 El dilema

```go
// ANTES de genericos — todo se hacia con interfaces
type Sorter interface {
    Len() int
    Less(i, j int) bool
    Swap(i, j int)
}

// El tipo debe implementar 3 metodos para ser sorteable
// sort.Sort(data) opera sobre la interfaz

// CON genericos — funciones que trabajan directamente con tipos
func Sort[T cmp.Ordered](s []T) {
    slices.Sort(s) // opera directamente sobre el slice
}
// No necesitas implementar ninguna interfaz
```

### 8.2 Regla general

```
┌────────────────────────────────────────────────────────────────────────────┐
│              INTERFACES vs GENERICOS — CUANDO USAR CADA UNA              │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│  USA INTERFACES cuando:                                                   │
│  ──────────────────────                                                   │
│  • Necesitas POLIMORFISMO en runtime (diferentes tipos tras la misma      │
│    interfaz — no sabes el tipo hasta que el programa corre)               │
│  • El comportamiento depende de METODOS del tipo                         │
│  • Quieres almacenar valores heterogeneos en la misma coleccion          │
│  • La abstraccion es parte de tu API publica                             │
│                                                                            │
│  Ejemplo: io.Reader — miles de tipos lo implementan                      │
│                                                                            │
│  type Writer interface { Write([]byte) (int, error) }                    │
│  func Copy(dst Writer, src Reader) (int64, error)                        │
│                                                                            │
│  USA GENERICOS cuando:                                                    │
│  ─────────────────────                                                    │
│  • Necesitas la MISMA logica para DIFERENTES tipos (el tipo es un         │
│    parametro, pero la logica es identica)                                 │
│  • Necesitas OPERADORES del tipo (<, +, ==)                              │
│  • Quieres evitar conversiones de tipo (type assertions)                 │
│  • Quieres type safety en compilacion (no runtime panics)                │
│  • Trabajas con CONTENEDORES de datos (Stack[T], Queue[T])               │
│                                                                            │
│  Ejemplo: slices.Sort — misma logica, diferente tipo de elemento         │
│                                                                            │
│  func Sort[T cmp.Ordered](s []T)                                        │
│                                                                            │
│  ─────────────────────────────────────────────────────────────────────    │
│                                                                            │
│  PIENSA ASI:                                                              │
│  • "Diferentes tipos, diferente comportamiento" → interface              │
│  • "Diferentes tipos, mismo comportamiento" → genericos                  │
│  • "Necesito operadores (+, <, ==)" → genericos                         │
│  • "Coleccion de tipos mixtos" → interface                               │
│  • "Coleccion de UN tipo" → genericos                                    │
└────────────────────────────────────────────────────────────────────────────┘
```

### 8.3 Ejemplo comparativo

```go
// INTERFAZ: diferentes tipos, diferente comportamiento
type Shape interface {
    Area() float64
}

type Circle struct{ Radius float64 }
func (c Circle) Area() float64 { return math.Pi * c.Radius * c.Radius }

type Rectangle struct{ Width, Height float64 }
func (r Rectangle) Area() float64 { return r.Width * r.Height }

// Cada tipo tiene su PROPIA implementacion de Area
func TotalArea(shapes []Shape) float64 {
    total := 0.0
    for _, s := range shapes {
        total += s.Area() // dispatch dinamico — cada tipo hace algo diferente
    }
    return total
}

// Puedes mezclar tipos:
shapes := []Shape{Circle{5}, Rectangle{3, 4}, Circle{2}}


// GENERICOS: diferentes tipos, MISMA logica
func Sum[T Number](values []T) T {
    var total T
    for _, v := range values {
        total += v // MISMA operacion para todos los tipos
    }
    return total
}

// No mezclas tipos — cada llamada es un tipo:
Sum([]int{1, 2, 3})        // T = int
Sum([]float64{1.1, 2.2})   // T = float64
```

### 8.4 Anti-patron: genericos innecesarios

```go
// MAL — genericos donde una interface funciona mejor
func BadPrint[T fmt.Stringer](v T) {
    fmt.Println(v.String())
}
// Esto es equivalente a:
func GoodPrint(v fmt.Stringer) {
    fmt.Println(v.String())
}
// La version con interface es mas simple y no ganas nada
// con genericos aqui (no necesitas el tipo concreto)

// MAL — genericos donde no hay type parameter real
func BadLen[T any](s []T) int {
    return len(s)
}
// Esto es equivalente a:
func GoodLen(s any) int {
    return reflect.ValueOf(s).Len()
}
// O mejor: no escribas esta funcion, usa len() directamente

// BIEN — genericos necesarios (no puedes hacer esto con interfaces)
func SortedKeys[K cmp.Ordered, V any](m map[K]V) []K {
    keys := make([]K, 0, len(m))
    for k := range m {
        keys = append(keys, k)
    }
    slices.Sort(keys)
    return keys
}
// El tipo de retorno []K DEPENDE del type parameter
// No hay forma de expresar esto con interfaces
```

---

## 9. Comparacion con C y Rust

| Aspecto | C | Go (1.18+) | Rust |
|---|---|---|---|
| Nombre | "Templates" (via macros/void*) | Type parameters | Generics (traits) |
| Sintaxis | `void*` o macros `#define` | `func F[T C](...)` | `fn f<T: Trait>(...)` |
| Verificacion | Ninguna (void*) o texto (macros) | Compilacion (constraints) | Compilacion (traits) |
| Mecanismo | N/A / texto expansion | Hibrido mono+dict | Monomorphization |
| Code bloat | Ningun (void*) o si (macros) | Minimo (dict para interfaces) | Si (una copia por tipo) |
| Error messages | Incomprensibles (macros) | Claros (constraint names) | Buenos (trait bounds) |
| Const generics | No | No | Si (`[T; N]`) |
| Associated types | No | No | Si |
| Default type params | No | No | Si (`T = String`) |
| Specialization | No | No | Parcial (nightly) |
| GATs | No | No | Si (1.65+) |
| Higher-kinded types | No | No | No (workarounds) |
| Where clauses | No | No | Si (`where T: Trait + 'a`) |
| Type-level computation | No | No | Si (const generics + traits) |
| Funciones genericas | Macros o void* | `func F[T any]()` | `fn f<T>()` |
| Structs genericos | Macros o void* | `type S[T any] struct{}` | `struct S<T> {}` |
| Methods en genericos | N/A | Si (con limitaciones) | Si (impl blocks) |
| Enums genericos | N/A | No (Go no tiene enums) | Si (`Option<T>`, `Result<T,E>`) |
| Closures genericas | N/A | No (no se pueden pasar) | Si (fn traits) |
| Trait objects | N/A | Interfaces (dynamic dispatch) | `dyn Trait` (dynamic) |
| Variadic generics | No | No | No (pero tuples) |

### 9.1 Ejemplo comparativo: funcion generica Max

```c
// C — void* + comparador (como qsort)
// No hay type safety, facil equivocarse

void* max(void* a, void* b, size_t size, int (*cmp)(const void*, const void*)) {
    if (cmp(a, b) > 0) return a;
    return b;
}

int cmp_int(const void* a, const void* b) {
    return *(int*)a - *(int*)b;
}

int main() {
    int x = 42, y = 17;
    int* result = (int*)max(&x, &y, sizeof(int), cmp_int);
    // Nada previene pasar tipos incorrectos:
    // max(&x, "hello", sizeof(int), cmp_int); // UB silencioso
}

// C — con macro (type-safe pero fragil)
#define MAX(a, b) ((a) > (b) ? (a) : (b))
int m = MAX(42, 17);      // OK
// MAX(i++, j++);          // BUG: doble evaluacion
```

```go
// Go — type parameter con constraint
func Max[T cmp.Ordered](a, b T) T {
    if a > b {
        return a
    }
    return b
}

Max(42, 17)       // OK: int
Max("z", "a")     // OK: string
// Max(42, "17")  // ERROR en compilacion: no puedo inferir T
```

```rust
// Rust — generic con trait bound
fn max<T: PartialOrd>(a: T, b: T) -> T {
    if a > b { a } else { b }
}

// O con la funcion de stdlib:
use std::cmp;
let m = cmp::max(42, 17);

// Rust ademas permite:
fn max_with_default<T: PartialOrd + Default>(a: T, b: T) -> T {
    // multiples trait bounds
    if a > b { a } else { b }
}

// Where clauses para bounds complejos:
fn complex<T, U>(t: T, u: U) -> T
where
    T: PartialOrd + Clone + Display,
    U: Into<T>,
{
    let converted = u.into();
    if t > converted { t } else { converted }
}
```

### 9.2 Ejemplo comparativo: contenedor generico

```c
// C — "generico" via void* (inseguro)
typedef struct {
    void** items;
    size_t len;
    size_t cap;
} Vector;

void vector_push(Vector* v, void* item) {
    if (v->len >= v->cap) {
        v->cap = v->cap ? v->cap * 2 : 4;
        v->items = realloc(v->items, v->cap * sizeof(void*));
    }
    v->items[v->len++] = item;
}

void* vector_get(Vector* v, size_t i) {
    return v->items[i]; // sin bounds check, sin type check
}
```

```go
// Go — struct generico con type safety
type Stack[T any] struct {
    items []T
}

func (s *Stack[T]) Push(v T) {
    s.items = append(s.items, v)
}

func (s *Stack[T]) Pop() (T, bool) {
    if len(s.items) == 0 {
        var zero T
        return zero, false
    }
    v := s.items[len(s.items)-1]
    s.items = s.items[:len(s.items)-1]
    return v, true
}

// Type safe:
intStack := &Stack[int]{}
intStack.Push(42)
intStack.Push(17)
// intStack.Push("hello")  // ERROR en compilacion
```

```rust
// Rust — Vec<T> (stdlib, equivalente a Stack)
let mut stack: Vec<i32> = Vec::new();
stack.push(42);
stack.push(17);
let v = stack.pop(); // Option<i32>
// stack.push("hello"); // ERROR en compilacion

// Custom stack con metodos adicionales:
struct Stack<T> {
    items: Vec<T>,
}

impl<T> Stack<T> {
    fn new() -> Self {
        Stack { items: Vec::new() }
    }

    fn push(&mut self, item: T) {
        self.items.push(item);
    }

    fn pop(&mut self) -> Option<T> {
        self.items.pop()
    }

    fn peek(&self) -> Option<&T> {
        self.items.last()
    }

    fn is_empty(&self) -> bool {
        self.items.is_empty()
    }
}

// Con trait bounds:
impl<T: Display> Stack<T> {
    fn print_all(&self) {
        for item in &self.items {
            println!("{item}");
        }
    }
}
// print_all solo existe para Stack<T> donde T: Display
// Go no puede hacer esto (no hay "impl condicional")
```

### 9.3 Lo que Go NO puede hacer (y Rust si)

```rust
// 1. Const generics — tamano como parametro de tipo
struct Matrix<const ROWS: usize, const COLS: usize> {
    data: [[f64; COLS]; ROWS],
}
// Go: no hay forma de parametrizar por tamano en compilacion

// 2. Associated types — tipo definido por el trait
trait Iterator {
    type Item;
    fn next(&mut self) -> Option<Self::Item>;
}
// Go: no tiene concepto equivalente

// 3. Specialization — implementacion especifica para un tipo
impl<T: Display> ToString for T {
    fn to_string(&self) -> String { format!("{self}") }
}
// Especializacion para String (mas eficiente):
impl ToString for String {
    fn to_string(&self) -> String { self.clone() }
}
// Go: no hay forma de especializar para un tipo especifico

// 4. Enums genericos
enum Result<T, E> {
    Ok(T),
    Err(E),
}
enum Option<T> {
    Some(T),
    None,
}
// Go: no tiene enums, no puede parametrizarlos

// 5. Method type parameters (Go lo prohibe explicitamente)
// En Rust:
impl<T> MyStruct<T> {
    fn convert<U>(&self) -> U where T: Into<U> {
        // U es un type parameter del METODO, no del struct
    }
}
// Go: los metodos NO pueden tener sus propios type parameters
// Solo pueden usar los type parameters del tipo receptor
```

---

## 10. Limitaciones conocidas de los genericos en Go

### 10.1 No method type parameters

```go
type Container[T any] struct {
    items []T
}

// ESTO NO COMPILA — metodos no pueden tener type parameters propios
// func (c *Container[T]) Map[U any](fn func(T) U) []U {
//     ...
// }

// SOLUCION: usar funciones top-level en vez de metodos
func MapContainer[T any, U any](c *Container[T], fn func(T) U) []U {
    result := make([]U, len(c.items))
    for i, item := range c.items {
        result[i] = fn(item)
    }
    return result
}

// Esto es una limitacion deliberada:
// Permitir method type params complicaria la relacion interface-metodo
// El equipo de Go decidio que la complejidad no vale la pena (por ahora)
```

### 10.2 No type assertions en type parameters

```go
// No puedes hacer type assertion/switch en un type parameter
func Process[T any](v T) {
    // ESTO NO COMPILA:
    // switch v.(type) {
    //     case int: ...
    //     case string: ...
    // }

    // Tampoco esto:
    // if n, ok := v.(int); ok { ... }

    // SOLUCION: convertir a any primero
    switch any(v).(type) {
    case int:
        fmt.Println("es int")
    case string:
        fmt.Println("es string")
    default:
        fmt.Println("otro tipo")
    }
}

// Pero si necesitas type switches, probablemente no necesitas genericos
// Genericos son para cuando la logica es IDENTICA para todos los tipos
```

### 10.3 No operadores custom

```go
// No puedes definir operadores para tus tipos via constraints
type Addable interface {
    Add(other ???) ???  // no puedes referir "Self" en Go
}

// En Rust puedes:
// impl Add for MyType { type Output = MyType; fn add(self, rhs: Self) -> Self { ... } }
// Y luego usar + con tu tipo

// Go: los operadores solo estan definidos para tipos built-in
// Las constraints con type elements (int | float64) dan acceso a +, <, etc.
// pero no puedes agregar + a tus propios tipos
```

### 10.4 No covariance/contravariance

```go
type Animal interface {
    Name() string
}

type Dog struct{}
func (d Dog) Name() string { return "dog" }

// Esto NO funciona:
// var animals []Animal = []Dog{{}, {}}
// error: cannot use []Dog as []Animal

// Tampoco funciona con genericos:
func PrintNames[T Animal](animals []T) {
    for _, a := range animals {
        fmt.Println(a.Name())
    }
}

// Esto SI funciona:
PrintNames([]Dog{{}, {}}) // T = Dog, que satisface Animal

// Pero no puedes asignar []Dog a []Animal
// Go no tiene covariance en genericos
```

### 10.5 No partial application de type parameters

```go
// No puedes fijar ALGUNOS type parameters y dejar otros abiertos
type Pair[A any, B any] struct {
    First  A
    Second B
}

// Quisieras:
// type IntPair[B any] = Pair[int, B]  // NO COMPILA

// SOLUCION: definir un tipo nuevo
type IntPair[B any] struct {
    First  int
    Second B
}
// Pero pierdes la relacion con Pair
```

### 10.6 No self-referential constraints

```go
// No puedes escribir un constraint que se refiera a si mismo elegantemente

// Quisieras algo como:
// type Comparable[T] interface {
//     CompareTo(other T) int
// }
// func Sort[T Comparable[T]](s []T) { ... }

// Esto SI funciona en Go, pero es verboso:
type Comparable[T any] interface {
    CompareTo(other T) int
}

func Sort[T Comparable[T]](s []T) {
    // ...insertion sort simple
    for i := 1; i < len(s); i++ {
        for j := i; j > 0 && s[j].CompareTo(s[j-1]) < 0; j-- {
            s[j], s[j-1] = s[j-1], s[j]
        }
    }
}

// El tipo debe referenciar su propio tipo en el constraint:
type Age int
func (a Age) CompareTo(other Age) int {
    return cmp.Compare(int(a), int(other))
}

ages := []Age{30, 25, 35}
Sort(ages) // OK
```

---

## 11. Programa completo: coleccion generica con operaciones funcionales

```go
package main

import (
    "cmp"
    "fmt"
    "slices"
    "strings"
)

// ============================================================================
// Collection — wrapper generico sobre slice con operaciones funcionales
// ============================================================================

// Collection envuelve un slice con operaciones de alto nivel
type Collection[T any] struct {
    items []T
}

// NewCollection crea una coleccion desde elementos
func NewCollection[T any](items ...T) Collection[T] {
    return Collection[T]{items: slices.Clone(items)}
}

// FromSlice crea una coleccion desde un slice existente (copia)
func FromSlice[T any](s []T) Collection[T] {
    return Collection[T]{items: slices.Clone(s)}
}

// Items retorna una copia del slice interno
func (c Collection[T]) Items() []T {
    return slices.Clone(c.items)
}

// Len retorna el numero de elementos
func (c Collection[T]) Len() int {
    return len(c.items)
}

// IsEmpty retorna true si la coleccion esta vacia
func (c Collection[T]) IsEmpty() bool {
    return len(c.items) == 0
}

// Get retorna el elemento en la posicion i, o false si fuera de rango
func (c Collection[T]) Get(i int) (T, bool) {
    if i < 0 || i >= len(c.items) {
        var zero T
        return zero, false
    }
    return c.items[i], true
}

// First retorna el primer elemento
func (c Collection[T]) First() (T, bool) {
    return c.Get(0)
}

// Last retorna el ultimo elemento
func (c Collection[T]) Last() (T, bool) {
    return c.Get(len(c.items) - 1)
}

// ============================================================================
// Operaciones que retornan Collection[T] (mismo tipo)
// ============================================================================

// Filter retorna elementos que cumplen el predicado
func (c Collection[T]) Filter(pred func(T) bool) Collection[T] {
    var result []T
    for _, item := range c.items {
        if pred(item) {
            result = append(result, item)
        }
    }
    return Collection[T]{items: result}
}

// Take retorna los primeros n elementos
func (c Collection[T]) Take(n int) Collection[T] {
    if n > len(c.items) {
        n = len(c.items)
    }
    return Collection[T]{items: slices.Clone(c.items[:n])}
}

// Skip retorna los elementos despues de los primeros n
func (c Collection[T]) Skip(n int) Collection[T] {
    if n > len(c.items) {
        n = len(c.items)
    }
    return Collection[T]{items: slices.Clone(c.items[n:])}
}

// Reverse retorna la coleccion en orden inverso
func (c Collection[T]) Reverse() Collection[T] {
    items := slices.Clone(c.items)
    slices.Reverse(items)
    return Collection[T]{items: items}
}

// ============================================================================
// Operaciones que requieren comparable
// ============================================================================

// Contains verifica si el elemento existe (requiere comparable)
func Contains[T comparable](c Collection[T], target T) bool {
    return slices.Contains(c.items, target)
}

// Unique elimina duplicados (requiere comparable)
func Unique[T comparable](c Collection[T]) Collection[T] {
    seen := make(map[T]struct{})
    var result []T
    for _, item := range c.items {
        if _, ok := seen[item]; !ok {
            seen[item] = struct{}{}
            result = append(result, item)
        }
    }
    return Collection[T]{items: result}
}

// ============================================================================
// Operaciones que requieren Ordered
// ============================================================================

// Sort ordena la coleccion (requiere Ordered)
func Sort[T cmp.Ordered](c Collection[T]) Collection[T] {
    items := slices.Clone(c.items)
    slices.Sort(items)
    return Collection[T]{items: items}
}

// Min retorna el minimo (requiere Ordered)
func Min[T cmp.Ordered](c Collection[T]) (T, bool) {
    if c.IsEmpty() {
        var zero T
        return zero, false
    }
    return slices.Min(c.items), true
}

// Max retorna el maximo (requiere Ordered)
func Max[T cmp.Ordered](c Collection[T]) (T, bool) {
    if c.IsEmpty() {
        var zero T
        return zero, false
    }
    return slices.Max(c.items), true
}

// ============================================================================
// Funciones top-level (porque los metodos no pueden tener type params propios)
// ============================================================================

// Map transforma elementos de tipo A a tipo B
func Map[A any, B any](c Collection[A], fn func(A) B) Collection[B] {
    result := make([]B, len(c.items))
    for i, item := range c.items {
        result[i] = fn(item)
    }
    return Collection[B]{items: result}
}

// FlatMap transforma y aplana
func FlatMap[A any, B any](c Collection[A], fn func(A) []B) Collection[B] {
    var result []B
    for _, item := range c.items {
        result = append(result, fn(item)...)
    }
    return Collection[B]{items: result}
}

// Reduce acumula un resultado
func Reduce[T any, Acc any](c Collection[T], initial Acc, fn func(Acc, T) Acc) Acc {
    result := initial
    for _, item := range c.items {
        result = fn(result, item)
    }
    return result
}

// GroupBy agrupa elementos por clave
func GroupBy[T any, K comparable](c Collection[T], keyFn func(T) K) map[K]Collection[T] {
    groups := make(map[K][]T)
    for _, item := range c.items {
        key := keyFn(item)
        groups[key] = append(groups[key], item)
    }
    result := make(map[K]Collection[T], len(groups))
    for k, v := range groups {
        result[k] = Collection[T]{items: v}
    }
    return result
}

// Partition divide en dos colecciones segun el predicado
func Partition[T any](c Collection[T], pred func(T) bool) (Collection[T], Collection[T]) {
    var yes, no []T
    for _, item := range c.items {
        if pred(item) {
            yes = append(yes, item)
        } else {
            no = append(no, item)
        }
    }
    return Collection[T]{items: yes}, Collection[T]{items: no}
}

// Zip combina dos colecciones elemento a elemento
func Zip[A any, B any](ca Collection[A], cb Collection[B]) Collection[struct{ A A; B B }] {
    n := min(ca.Len(), cb.Len())
    result := make([]struct{ A A; B B }, n)
    for i := 0; i < n; i++ {
        result[i] = struct{ A A; B B }{ca.items[i], cb.items[i]}
    }
    return Collection[struct{ A A; B B }]{items: result}
}

// ============================================================================
// ForEach y otras terminales
// ============================================================================

// ForEach ejecuta una funcion para cada elemento
func (c Collection[T]) ForEach(fn func(T)) {
    for _, item := range c.items {
        fn(item)
    }
}

// Any retorna true si al menos un elemento cumple el predicado
func (c Collection[T]) Any(pred func(T) bool) bool {
    for _, item := range c.items {
        if pred(item) {
            return true
        }
    }
    return false
}

// All retorna true si todos los elementos cumplen el predicado
func (c Collection[T]) All(pred func(T) bool) bool {
    for _, item := range c.items {
        if !pred(item) {
            return false
        }
    }
    return true
}

// None retorna true si ningun elemento cumple el predicado
func (c Collection[T]) None(pred func(T) bool) bool {
    return !c.Any(pred)
}

// Count retorna cuantos elementos cumplen el predicado
func (c Collection[T]) Count(pred func(T) bool) int {
    count := 0
    for _, item := range c.items {
        if pred(item) {
            count++
        }
    }
    return count
}

// ============================================================================
// Punto de entrada
// ============================================================================

type Product struct {
    Name     string
    Category string
    Price    float64
    InStock  bool
}

func main() {
    products := NewCollection(
        Product{"Laptop", "Electronics", 999.99, true},
        Product{"Mouse", "Electronics", 29.99, true},
        Product{"Keyboard", "Electronics", 79.99, false},
        Product{"Desk", "Furniture", 299.99, true},
        Product{"Chair", "Furniture", 449.99, true},
        Product{"Monitor", "Electronics", 399.99, true},
        Product{"Lamp", "Furniture", 49.99, false},
        Product{"Headphones", "Electronics", 149.99, true},
        Product{"Webcam", "Electronics", 69.99, true},
        Product{"Bookshelf", "Furniture", 189.99, true},
    )

    fmt.Println("=== Todos los productos ===")
    fmt.Printf("Total: %d productos\n\n", products.Len())

    // Filter: solo en stock
    inStock := products.Filter(func(p Product) bool { return p.InStock })
    fmt.Printf("En stock: %d productos\n", inStock.Len())

    // Filter chain: electronicos en stock con precio < 200
    affordable := products.
        Filter(func(p Product) bool { return p.InStock }).
        Filter(func(p Product) bool { return p.Category == "Electronics" }).
        Filter(func(p Product) bool { return p.Price < 200 })

    fmt.Println("\n=== Electronicos en stock < $200 ===")
    affordable.ForEach(func(p Product) {
        fmt.Printf("  %s: $%.2f\n", p.Name, p.Price)
    })

    // Map: extraer nombres
    names := Map(products, func(p Product) string { return p.Name })
    fmt.Printf("\nNombres: %v\n", names.Items())

    // Map: extraer precios y calcular estadisticas
    prices := Map(products, func(p Product) float64 { return p.Price })
    minPrice, _ := Min(prices)
    maxPrice, _ := Max(prices)
    avgPrice := Reduce(prices, 0.0, func(acc, p float64) float64 { return acc + p }) / float64(prices.Len())
    fmt.Printf("\nPrecios — Min: $%.2f, Max: $%.2f, Avg: $%.2f\n", minPrice, maxPrice, avgPrice)

    // GroupBy: por categoria
    byCategory := GroupBy(products, func(p Product) string { return p.Category })
    fmt.Println("\n=== Por categoria ===")
    for cat, items := range byCategory {
        total := Reduce(items, 0.0, func(acc float64, p Product) float64 { return acc + p.Price })
        fmt.Printf("  %s: %d productos, total $%.2f\n", cat, items.Len(), total)
    }

    // Partition: en stock vs agotados
    available, outOfStock := Partition(products, func(p Product) bool { return p.InStock })
    fmt.Printf("\nDisponibles: %d, Agotados: %d\n", available.Len(), outOfStock.Len())

    // Any, All, None
    fmt.Printf("\nAlguno > $500? %v\n", products.Any(func(p Product) bool { return p.Price > 500 }))
    fmt.Printf("Todos en stock? %v\n", products.All(func(p Product) bool { return p.InStock }))
    fmt.Printf("Ninguno gratis? %v\n", products.None(func(p Product) bool { return p.Price == 0 }))

    // Count
    electronics := products.Count(func(p Product) bool { return p.Category == "Electronics" })
    fmt.Printf("Electronicos: %d\n", electronics)

    // FlatMap: tags por producto
    type TaggedProduct struct {
        Name string
        Tags []string
    }
    tagged := NewCollection(
        TaggedProduct{"Laptop", []string{"tech", "work", "portable"}},
        TaggedProduct{"Desk", []string{"furniture", "work", "office"}},
        TaggedProduct{"Headphones", []string{"tech", "audio", "portable"}},
    )
    allTags := FlatMap(tagged, func(tp TaggedProduct) []string { return tp.Tags })
    uniqueTags := Unique(allTags)
    sortedTags := Sort(uniqueTags)
    fmt.Printf("\nTags unicos ordenados: %v\n", sortedTags.Items())

    // Take y Skip (paginacion)
    page1 := products.Take(3)
    page2 := products.Skip(3).Take(3)
    fmt.Printf("\nPagina 1: %d items, Pagina 2: %d items\n", page1.Len(), page2.Len())

    // Zip
    indices := NewCollection(1, 2, 3, 4, 5)
    topNames := Map(products.Take(5), func(p Product) string { return p.Name })
    numbered := Zip(indices, topNames)
    fmt.Println("\n=== Top 5 (numbered) ===")
    numbered.ForEach(func(pair struct{ A int; B string }) {
        fmt.Printf("  %d. %s\n", pair.A, pair.B)
    })

    // Ejemplo con tipos basicos
    fmt.Println("\n=== Colecciones basicas ===")
    nums := NewCollection(5, 3, 8, 1, 9, 2, 7, 4, 6)
    sorted := Sort(nums)
    fmt.Printf("Sorted: %v\n", sorted.Items())
    fmt.Printf("Reversed: %v\n", sorted.Reverse().Items())

    evens := nums.Filter(func(n int) bool { return n%2 == 0 })
    sum := Reduce(evens, 0, func(a, b int) int { return a + b })
    fmt.Printf("Pares: %v, Suma: %d\n", evens.Items(), sum)

    words := NewCollection("go", "rust", "c", "python", "zig", "gleam", "c")
    unique := Unique(words)
    byLength := GroupBy(unique, func(s string) int { return len(s) })
    fmt.Println("\nPalabras por longitud:")
    for length, group := range byLength {
        fmt.Printf("  %d letras: %v\n", length, group.Items())
    }

    // Demostrar que chaining funciona, aunque no tan ergonomico como Rust
    result := Map(
        Sort(Unique(NewCollection("banana", "apple", "cherry", "apple", "banana"))),
        func(s string) string { return strings.ToUpper(s) },
    )
    fmt.Printf("\nFrutas unicas ordenadas: %v\n", result.Items())
}
```

```
=== Todos los productos ===
Total: 10 productos

En stock: 8 productos

=== Electronicos en stock < $200 ===
  Mouse: $29.99
  Headphones: $149.99
  Webcam: $69.99

Nombres: [Laptop Mouse Keyboard Desk Chair Monitor Lamp Headphones Webcam Bookshelf]

Precios — Min: $29.99, Max: $999.99, Avg: $271.99

=== Por categoria ===
  Electronics: 6 productos, total $1729.94
  Furniture: 4 productos, total $989.96

Disponibles: 8, Agotados: 2

Alguno > $500? true
Todos en stock? false
Ninguno gratis? true
Electronicos: 6

Tags unicos ordenados: [audio furniture office portable tech work]

Pagina 1: 3 items, Pagina 2: 3 items

=== Top 5 (numbered) ===
  1. Laptop
  2. Mouse
  3. Keyboard
  4. Desk
  5. Chair

=== Colecciones basicas ===
Sorted: [1 2 3 4 5 6 7 8 9]
Reversed: [9 8 7 6 5 4 3 2 1]
Pares: [8 2 4 6], Suma: 20

Palabras por longitud:
  2 letras: [go]
  4 letras: [rust]
  1 letras: [c]
  6 letras: [python]
  3 letras: [zig]
  5 letras: [gleam]

Frutas unicas ordenadas: [APPLE BANANA CHERRY]
```

**Puntos clave del programa**:

1. **Metodos vs funciones top-level**: `Filter`, `Take`, `Skip`, `Reverse` son metodos porque retornan `Collection[T]` (mismo tipo). `Map`, `FlatMap`, `Reduce`, `GroupBy` son funciones top-level porque necesitan type parameters adicionales (la limitacion de no method type params).

2. **Constraints escalonados**: `Collection[T any]` acepta cualquier tipo. `Contains` requiere `comparable`. `Sort`/`Min`/`Max` requieren `cmp.Ordered`. Cada funcion pide exactamente lo que necesita.

3. **Chaining limitado**: `products.Filter(...).Filter(...)` funciona (retorna `Collection[T]`). Pero `products.Filter(...).Map(...)` NO funciona como metodo (Map necesita nuevo type param). Usas la funcion top-level: `Map(products.Filter(...), fn)`.

4. **Zero values**: cuando una operacion falla (coleccion vacia, indice fuera de rango), Go no tiene `Option<T>`. Usamos el patron `(T, bool)` donde `T` es el zero value del tipo.

---

## 12. Ejercicios

### Ejercicio 1: Funciones genericas basicas

Implementa las siguientes funciones:
1. `Chunk[T any](slice []T, size int) [][]T` — divide un slice en sub-slices de tamano `size`
2. `Flatten[T any](nested [][]T) []T` — aplana un slice de slices
3. `Repeat[T any](value T, n int) []T` — crea un slice con `n` copias de `value`
4. `Window[T any](slice []T, size int) [][]T` — sliding window de tamano `size`
5. `Interleave[T any](a, b []T) []T` — intercala elementos de dos slices

Verifica con:
```go
fmt.Println(Chunk([]int{1,2,3,4,5}, 2))   // [[1 2] [3 4] [5]]
fmt.Println(Flatten([][]int{{1,2},{3},{4,5}})) // [1 2 3 4 5]
fmt.Println(Repeat("go", 3))               // [go go go]
fmt.Println(Window([]int{1,2,3,4,5}, 3))   // [[1 2 3] [2 3 4] [3 4 5]]
fmt.Println(Interleave([]int{1,3,5}, []int{2,4,6})) // [1 2 3 4 5 6]
```

### Ejercicio 2: Set generico

Implementa un tipo `Set[T comparable]` con las operaciones:
1. `New[T comparable]() Set[T]` — crear set vacio
2. `Add(item T)` — agregar elemento
3. `Remove(item T)` — eliminar elemento
4. `Contains(item T) bool` — verificar existencia
5. `Len() int` — numero de elementos
6. `Items() []T` — todos los elementos
7. `Union(other Set[T]) Set[T]` — union de conjuntos
8. `Intersection(other Set[T]) Set[T]` — interseccion
9. `Difference(other Set[T]) Set[T]` — diferencia (A - B)
10. `IsSubset(other Set[T]) bool` — es subconjunto?

Verifica con:
```go
a := New[int]()
a.Add(1); a.Add(2); a.Add(3)
b := New[int]()
b.Add(2); b.Add(3); b.Add(4)
fmt.Println(a.Union(b).Items())        // [1 2 3 4] (orden puede variar)
fmt.Println(a.Intersection(b).Items()) // [2 3]
fmt.Println(a.Difference(b).Items())   // [1]
fmt.Println(a.IsSubset(a.Union(b)))    // true
```

### Ejercicio 3: Pipeline builder generico

Implementa un pipeline de transformaciones encadenadas:

```go
// Pipeline que procesa datos paso a paso
type Pipeline[In any, Out any] struct { ... }

func NewPipeline[T any]() Pipeline[T, T]
func Then[A any, B any, C any](p Pipeline[A, B], fn func(B) C) Pipeline[A, C]
func (p Pipeline[In, Out]) Execute(input In) Out
```

Ejemplo de uso:
```go
pipeline := Then(
    Then(
        NewPipeline[string](),
        strings.ToUpper,
    ),
    func(s string) int { return len(s) },
)
fmt.Println(pipeline.Execute("hello")) // 5

// Nota: el chaining requiere funciones top-level por la limitacion
// de method type params
```

### Ejercicio 4: Cache generico con TTL

Implementa un cache thread-safe con expiracion:

```go
type Cache[K comparable, V any] struct { ... }

func NewCache[K comparable, V any](defaultTTL time.Duration) *Cache[K, V]
func (c *Cache[K, V]) Set(key K, value V)
func (c *Cache[K, V]) SetWithTTL(key K, value V, ttl time.Duration)
func (c *Cache[K, V]) Get(key K) (V, bool)
func (c *Cache[K, V]) Delete(key K)
func (c *Cache[K, V]) Len() int
func (c *Cache[K, V]) Clear()
func (c *Cache[K, V]) Keys() []K
```

Requisitos:
- Thread-safe (usa `sync.RWMutex`)
- Entries expiran despues del TTL
- `Get` no retorna entries expiradas
- Limpieza periodica de entries expiradas (goroutine en background)

---

> **Siguiente**: C11/S01 - Genericos (Go 1.18+), T02 - Tipos genericos — structs y metodos con type parameters, limitaciones actuales (no method type params)
