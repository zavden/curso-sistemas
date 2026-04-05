# Constraints — interfaces como constraints, constraints package, union types (~int | ~float64), tilde (~)

## 1. Introduccion

En T01 y T02 introdujimos constraints como `any`, `comparable` y `cmp.Ordered`, y vimos brevemente union types con `~`. Este topico profundiza en el **sistema de constraints de Go** — la pieza mas novedosa y mas sutil de los genericos. Los constraints son interfaces extendidas que definen **type sets**: el conjunto de tipos que un type parameter puede representar. Entender type sets es entender los genericos de Go.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                    CONSTRAINTS EN GO — MODELO MENTAL                            │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  ANTES de Go 1.18: una interface define un METHOD SET                           │
│  ─────────────────────────────────────────────────────                           │
│                                                                                  │
│  type Stringer interface {                                                      │
│      String() string     ← "cualquier tipo con metodo String()"                │
│  }                                                                              │
│  // Method set = { String() string }                                            │
│  // Puede usarse como tipo de variable, parametro, retorno                     │
│                                                                                  │
│  DESPUES de Go 1.18: una interface define un TYPE SET                           │
│  ────────────────────────────────────────────────────                            │
│                                                                                  │
│  type Numeric interface {                                                       │
│      ~int | ~float64     ← "solo tipos cuyo underlying es int o float64"       │
│  }                                                                              │
│  // Type set = { int, float64, MyInt, Celsius, ... }                           │
│  // SOLO puede usarse como constraint, NO como tipo de variable               │
│                                                                                  │
│  type Ordered interface {                                                       │
│      ~int | ~float64 | ~string                                                 │
│      // Type set = todos los tipos con underlying int, float64, o string       │
│      // Permite operadores: <, >, <=, >=, ==, !=                              │
│  }                                                                              │
│                                                                                  │
│  type PrintableNum interface {                                                  │
│      ~int | ~float64      ← type elements (restringen a tipos especificos)     │
│      String() string      ← method element (requiere metodo)                  │
│  }                                                                              │
│  // Type set = INTERSECCION:                                                   │
│  //   tipos con underlying int/float64  ∩  tipos con metodo String()          │
│  //   = solo tipos definidos sobre int/float64 que tengan String()            │
│                                                                                  │
│  ┌───────────────────────────────────────────────────────────────┐              │
│  │  JERARQUIA DE CONSTRAINTS                                     │              │
│  │                                                               │              │
│  │  any                    ← type set = todos los tipos          │              │
│  │   └── comparable        ← types set = tipos comparables      │              │
│  │        └── cmp.Ordered  ← type set = tipos con < > <= >=     │              │
│  │                                                               │              │
│  │  interface{M()}         ← solo methods, usable como tipo      │              │
│  │  interface{~int|~str}   ← type elements, SOLO como constraint │              │
│  │  interface{~int; M()}   ← mixed, SOLO como constraint        │              │
│  └───────────────────────────────────────────────────────────────┘              │
└──────────────────────────────────────────────────────────────────────────────────┘
```

**Concepto fundamental**: cada interface en Go 1.18+ define un **type set** — el conjunto de todos los tipos que la satisfacen. Un constraint es simplemente una interface usada como restriccion en un type parameter. Las interfaces con type elements (unions, tilde) tienen type sets que no pueden representarse con method sets solos — por eso solo se usan como constraints.

---

## 2. Type sets: la teoria

### 2.1 Que es un type set

```go
// Cada interface define un type set — el conjunto de tipos que la satisfacen

// any → type set = {todos los tipos}
// comparable → type set = {int, string, float64, bool, arrays, structs comparables, ...}
//                         excluyendo {slices, maps, funcs, interfaces con types no comparables}

// Interface solo con metodos:
type Writer interface {
    Write([]byte) (int, error)
}
// Type set = {*os.File, *bytes.Buffer, *net.TCPConn, *http.Response, ...}
// = todos los tipos que tienen el metodo Write([]byte)(int,error)
// Este set es ABIERTO — cualquier tipo nuevo puede satisfacerlo

// Interface con type elements:
type Signed interface {
    ~int | ~int8 | ~int16 | ~int32 | ~int64
}
// Type set = {int, int8, int16, int32, int64, MyInt, UserID, ...}
// = tipos cuyo underlying type es un signed integer
// Este set es CERRADO a los underlying types especificados
```

### 2.2 Operaciones sobre type sets

```go
// La INTERSECCION es la operacion fundamental

// Cada linea en una interface es un ELEMENTO del type set
// El type set final es la INTERSECCION de todos los elementos

// Ejemplo 1: solo type elements
type A interface {
    ~int | ~string  // set A = {types basados en int o string}
}

// Ejemplo 2: solo methods
type B interface {
    String() string  // set B = {tipos con metodo String()}
}

// Ejemplo 3: interseccion de type elements y methods
type C interface {
    ~int | ~string   // set 1 = {tipos basados en int o string}
    String() string  // set 2 = {tipos con metodo String()}
}
// Type set de C = set1 ∩ set2
// = tipos basados en int o string QUE ADEMAS tengan metodo String()
// int NO esta (no tiene String()), pero MyInt si (si defines String())

// Ejemplo 4: multiples lineas de type elements
type D interface {
    ~int | ~string | ~float64   // set 1
    ~int | ~bool                // set 2
}
// Type set de D = set1 ∩ set2 = {~int}
// Solo ~int esta en AMBOS sets
// Es como AND logico entre las lineas

// Ejemplo 5: interface embebida
type E interface {
    A          // embeber A = union set A
    B          // embeber B = method set B
}
// Type set de E = type set de A ∩ type set de B
// = tipos basados en int/string que tengan String()
// Identico a C — el embedding es equivalente a poner las lineas directamente
```

### 2.3 El type set del universo

```
┌────────────────────────────────────────────────────────────────────────────┐
│              TYPE SETS — DIAGRAMA DE VENN                                  │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│  any (universo = todos los tipos)                                         │
│  ┌────────────────────────────────────────────────────────────────┐       │
│  │                                                                │       │
│  │   comparable (tipos con == y !=)                              │       │
│  │   ┌──────────────────────────────────────────────────┐        │       │
│  │   │                                                  │        │       │
│  │   │   cmp.Ordered (tipos con < > <= >= == !=)       │        │       │
│  │   │   ┌───────────────────────────────────┐          │        │       │
│  │   │   │  int, int8...int64                │          │        │       │
│  │   │   │  uint, uint8...uint64, uintptr    │          │        │       │
│  │   │   │  float32, float64                 │          │        │       │
│  │   │   │  string                           │          │        │       │
│  │   │   │  MyInt, Celsius, Score...         │          │        │       │
│  │   │   └───────────────────────────────────┘          │        │       │
│  │   │                                                  │        │       │
│  │   │  complex64, complex128 (comparable, no Ordered) │        │       │
│  │   │  bool                                            │        │       │
│  │   │  [N]T (arrays de tipos comparable)              │        │       │
│  │   │  struct{X int; Y string} (structs comparable)   │        │       │
│  │   │  *T (pointers)                                   │        │       │
│  │   │  chan T (channels)                               │        │       │
│  │   └──────────────────────────────────────────────────┘        │       │
│  │                                                                │       │
│  │  []T, map[K]V, func()  (no comparable)                       │       │
│  │  interface{...}         (puede o no ser comparable)           │       │
│  │                                                                │       │
│  └────────────────────────────────────────────────────────────────┘       │
│                                                                            │
│  NOTA: con ~ se incluyen tipos definidos basados en los built-in         │
│  ~int incluye int, MyInt, UserID, etc.                                   │
│  int (sin ~) incluye SOLO int                                            │
└────────────────────────────────────────────────────────────────────────────┘
```

### 2.4 Reglas formales de satisfaccion

```go
// Un tipo T satisface un constraint C si T pertenece al type set de C.
// Las reglas para determinar pertenencia son:

// 1. METHOD ELEMENTS: T debe implementar todos los metodos listados
type HasLen interface {
    Len() int
}
// []int NO satisface HasLen (slices no tienen metodos)
// MySlice si (si defines func (s MySlice) Len() int)

// 2. TYPE ELEMENTS: el underlying type de T debe coincidir
type OnlyInt interface {
    int     // sin tilde: SOLO int, no MyInt
}
type AnyInt interface {
    ~int    // con tilde: int Y tipos definidos sobre int
}

type MyInt int
// MyInt satisface AnyInt (underlying type es int)
// MyInt NO satisface OnlyInt (MyInt != int)
// int satisface ambos

// 3. UNION: T debe coincidir con AL MENOS UN elemento de la union
type IntOrString interface {
    int | string
}
// int satisface (coincide con int)
// string satisface (coincide con string)
// float64 NO satisface (no es ni int ni string)

// 4. INTERSECCION: T debe satisfacer TODOS los elementos (todas las lineas)
type NumberWithString interface {
    ~int | ~float64
    fmt.Stringer
}
// int NO satisface (no tiene String())
// Celsius(float64) con String() SI satisface

// 5. COMPARABLE: es una constraint built-in especial
// Los tipos que la satisfacen son:
//   - booleanos, numericos, string, punteros, channels
//   - arrays de tipos comparable
//   - structs donde TODOS los campos son comparable
// NO la satisfacen:
//   - slices, maps, funciones
//   - structs con campos slice/map/func

// 6. COMPARABLE + interfaces: regla sutil
type Equatable interface {
    comparable
    Equal(other any) bool
}
// T debe ser comparable (== works) Y tener metodo Equal
```

---

## 3. El operador tilde (~) en profundidad

### 3.1 Underlying types en Go

```go
// Para entender ~, necesitas entender "underlying type" en Go

// Regla: cada tipo tiene un underlying type
// Para tipos built-in, el underlying type es el tipo mismo:
//   underlying(int)     = int
//   underlying(string)  = string
//   underlying(bool)    = bool

// Para tipos definidos, el underlying type es el tipo base:
type Celsius float64    // underlying(Celsius)  = float64
type UserID int         // underlying(UserID)   = int
type Name string        // underlying(Name)     = string

// Para tipos compuestos, el underlying type es la estructura:
type IntSlice []int     // underlying(IntSlice) = []int
type UserMap map[string]User // underlying(UserMap) = map[string]User

// Cadena de definiciones — underlying se resuelve al final:
type A int              // underlying(A) = int
type B A                // underlying(B) = int (NO A)
type C B                // underlying(C) = int (NO B, NO A)
// Siempre se resuelve hasta el tipo built-in o literal
```

### 3.2 Como funciona ~ con cada tipo

```go
// ~T en un constraint significa "cualquier tipo cuyo underlying type sea T"
// T debe ser un tipo NO interface

// ~int
type MyInt int
type YourInt int
type Celsius float64

func Double[T ~int](v T) T { return v + v }

Double(42)          // OK: underlying(int) = int
Double(MyInt(5))    // OK: underlying(MyInt) = int
Double(YourInt(3))  // OK: underlying(YourInt) = int
// Double(Celsius(1)) // ERROR: underlying(Celsius) = float64, no int

// ~[]int
type IntList []int
type Numbers []int

func First[T ~[]int](s T) int {
    if len(s) == 0 {
        return 0
    }
    return s[0]
}

First([]int{1, 2})      // OK: underlying([]int) = []int
First(IntList{1, 2})     // OK: underlying(IntList) = []int
First(Numbers{1, 2})     // OK: underlying(Numbers) = []int
// First([]string{"a"})  // ERROR: underlying([]string) = []string, no []int

// ~struct{...}
type Point struct{ X, Y float64 }
type Vector struct{ X, Y float64 }
type Coordinate struct{ X, Y float64 }

// IMPORTANTE: ~ con structs es raro — los structs anonimos
// identicos son el mismo type, pero los named structs no
// underlying(Point) = struct{ X, Y float64 }
// underlying(Vector) = struct{ X, Y float64 }
// Son el MISMO underlying type!

type PointLike interface {
    ~struct{ X, Y float64 }
}

func Distance[T PointLike](a, b T) float64 {
    // Puedes acceder a los campos porque el underlying type los tiene
    ax, ay := float64(a.X), float64(a.Y) // ERROR real: no puedes acceder campos con ~
    // En realidad, type elements no permiten acceder campos
    // Solo permiten OPERADORES del underlying type
    _ = ax; _ = ay
    return 0
}
// NOTA: ~ con structs es legal sintacticamente pero poco util en practica
// porque no puedes acceder a los campos del struct a traves del constraint
```

### 3.3 Restricciones sobre ~

```go
// Regla 1: ~ solo puede aplicarse a tipos NO interface
// ~int         OK
// ~string      OK
// ~[]byte      OK
// ~Stringer    ERROR (Stringer es una interface)
// ~any         ERROR (any es una interface)
// ~comparable  ERROR (comparable es una interface)

// Regla 2: ~ solo se usa dentro de interfaces (constraints)
// No puedes usar ~ en declaraciones de tipo normales
// type X ~int  // ERROR

// Regla 3: el argumento de ~ debe ser un tipo concreto o un type literal
// ~int             OK (tipo built-in)
// ~[]byte          OK (tipo compuesto literal)
// ~map[string]int  OK (tipo compuesto literal)
// ~MyType          ERROR si MyType es interface

// Regla 4: ~ en unions
type Good interface {
    ~int | ~string | ~float64   // OK: cada termino tiene ~
}
type AlsoGood interface {
    int | ~string | float64     // OK: mezclar con y sin ~ esta permitido
}
```

### 3.4 Por que ~ es necesario

```go
// Sin ~, los genericos serian casi inutiles para tipos definidos

type Kilogram float64
type Pound float64
type Newton float64

// Sin ~: solo funciona con float64 literal
type ExactFloat interface { float64 }
func AddExact[T ExactFloat](a, b T) T { return a + b }

AddExact(1.0, 2.0)              // OK: float64
// AddExact(Kilogram(1), Kilogram(2)) // ERROR: Kilogram != float64

// Con ~: funciona con CUALQUIER tipo basado en float64
type AnyFloat interface { ~float64 }
func AddAny[T AnyFloat](a, b T) T { return a + b }

AddAny(1.0, 2.0)                  // OK: float64
AddAny(Kilogram(1), Kilogram(2))   // OK: underlying es float64
AddAny(Pound(1), Pound(2))         // OK: underlying es float64
// AddAny(Kilogram(1), Pound(2))   // ERROR: T es un solo tipo

// El ultimo punto es crucial:
// T se resuelve a UN tipo concreto — Kilogram y Pound son tipos diferentes
// No puedes mezclarlos aunque ambos satisfagan ~float64
// Esto previene errores de unidades: sumar kilogramos con libras
```

---

## 4. Constraints predefinidos en la stdlib

### 4.1 any

```go
// any = interface{} (alias definido en el universo)
// Type set: todos los tipos
// Operaciones disponibles: asignar, pasar como argumento, tomar direccion

func Identity[T any](v T) T { return v }
func Zero[T any]() T { var z T; return z }
func Ptr[T any](v T) *T { return &v }

// any es el constraint "sin restricciones"
// Si tu funcion solo necesita almacenar/pasar/retornar T, usa any
```

### 4.2 comparable

```go
// comparable es un constraint built-in (no es una interface regular)
// Type set: tipos que soportan == y !=
// Operaciones adicionales: usar como map key

// Tipos comparable:
// ✓ bool
// ✓ int, int8..int64, uint, uint8..uint64, uintptr
// ✓ float32, float64
// ✓ complex64, complex128
// ✓ string
// ✓ *T (punteros)
// ✓ chan T (channels)
// ✓ [N]T donde T es comparable
// ✓ struct{...} donde TODOS los campos son comparable
// ✓ interfaces (comparacion de runtime — puede panic con tipos no comparable)

// Tipos NO comparable:
// ✗ []T (slices)
// ✗ map[K]V (maps)
// ✗ func(...) (funciones)
// ✗ structs con campos de los tipos anteriores

// SUTILEZA: comparable como constraint vs comparable como concepto
// Un interface type puede usarse con == (no panics si el valor
// dinamico es comparable), pero los GENERICOS requieren que
// el tipo ESTATICO sea comparable

type I interface{ M() }

func F[T comparable](t T) {}

// Antes de Go 1.20:
// F(I(nil)) — ERROR: I no satisface comparable (es interface)

// Desde Go 1.20:
// F(I(nil)) — OK: interfaces satisfacen comparable
// (pero puede panic en runtime si el valor dinamico no es comparable)

// PRACTICA: si necesitas == y no necesitas map keys, considera
// usar una interface con un metodo Equal en vez de comparable
```

### 4.3 cmp.Ordered (Go 1.21+)

```go
import "cmp"

// cmp.Ordered define todos los tipos que soportan operadores de orden
// Definicion:
// type Ordered interface {
//     ~int | ~int8 | ~int16 | ~int32 | ~int64 |
//     ~uint | ~uint8 | ~uint16 | ~uint32 | ~uint64 | ~uintptr |
//     ~float32 | ~float64 |
//     ~string
// }

// cmp.Ordered es un SUBCONJUNTO de comparable
// Todo Ordered es comparable, pero no todo comparable es Ordered
// Ejemplo: bool es comparable (==, !=) pero NO Ordered (no tiene <)
// Ejemplo: *int es comparable pero NO Ordered

// Funciones de cmp que usan Ordered:
func cmp.Compare[T Ordered](x, y T) int  // -1, 0, +1
func cmp.Less[T Ordered](x, y T) bool
func cmp.Or[T comparable](vals ...T) T   // primer non-zero (usa comparable, no Ordered)
```

### 4.4 El paquete golang.org/x/exp/constraints (historico)

```go
// HISTORICO: antes de Go 1.21, las constraints se definieron en x/exp/constraints
// Este paquete fue el "prototipo" que luego se integro parcialmente en la stdlib

// x/exp/constraints definia:
// - Signed    = ~int | ~int8 | ~int16 | ~int32 | ~int64
// - Unsigned  = ~uint | ~uint8 | ~uint16 | ~uint32 | ~uint64 | ~uintptr
// - Integer   = Signed | Unsigned
// - Float     = ~float32 | ~float64
// - Complex   = ~complex64 | ~complex128
// - Ordered   = Integer | Float | ~string

// EN GO 1.21+: usa cmp.Ordered en vez de constraints.Ordered
// Los otros (Signed, Unsigned, Integer, Float, Complex) NO se incluyeron
// en la stdlib — debes definirlos tu mismo si los necesitas:

type Signed interface {
    ~int | ~int8 | ~int16 | ~int32 | ~int64
}

type Unsigned interface {
    ~uint | ~uint8 | ~uint16 | ~uint32 | ~uint64 | ~uintptr
}

type Integer interface {
    Signed | Unsigned
}

type Float interface {
    ~float32 | ~float64
}

type Complex interface {
    ~complex64 | ~complex128
}

type Numeric interface {
    Integer | Float
}

// NOTA: x/exp/constraints esta DEPRECADO desde Go 1.21
// No lo uses en proyectos nuevos
```

---

## 5. Composicion de constraints

### 5.1 Embedding de constraints (interseccion)

```go
// Embeber interfaces en interfaces produce INTERSECCION de type sets

// Constraint 1: tipos numericos
type Numeric interface {
    ~int | ~int8 | ~int16 | ~int32 | ~int64 |
    ~uint | ~uint8 | ~uint16 | ~uint32 | ~uint64 |
    ~float32 | ~float64
}

// Constraint 2: tipos con String()
type Stringer interface {
    String() string
}

// Interseccion: numerico Y con String()
type StringableNumber interface {
    Numeric
    Stringer
}
// Solo tipos definidos sobre numeros que ADEMAS tengan String()

// Interseccion: comparable Y con String()
type EquatableStringer interface {
    comparable
    Stringer
}

// Interseccion: Ordered Y con String()
type OrderedStringer interface {
    cmp.Ordered
    Stringer
}

// Ejemplo practico
type Priority int

func (p Priority) String() string {
    switch p {
    case 1:
        return "low"
    case 2:
        return "medium"
    case 3:
        return "high"
    default:
        return fmt.Sprintf("priority(%d)", int(p))
    }
}

// Priority satisface StringableNumber:
// ✓ underlying type int → satisface Numeric
// ✓ tiene String() → satisface Stringer

func FormatSorted[T OrderedStringer](items []T) string {
    sorted := slices.Clone(items)
    slices.Sort(sorted)
    strs := make([]string, len(sorted))
    for i, item := range sorted {
        strs[i] = item.String()
    }
    return strings.Join(strs, ", ")
}
```

### 5.2 Union de constraints

```go
// La union de constraints se hace con | dentro de una interface

// Constraint para tipos que pueden ser keys de JSON
type JSONKey interface {
    ~string | ~int | ~int64 | ~float64 | ~bool
}

// Constraint para tipos "printable" (Ordered + bool)
type Printable interface {
    cmp.Ordered | ~bool
    // NOTA: esto NO funciona como esperas
    // No puedes hacer union de interfaces con metodos/constraints embebidos
    // ERROR: cannot use cmp.Ordered in union (cmp.Ordered contains type constraints)
}

// CORRECTO: debes expandir
type Printable interface {
    ~int | ~int8 | ~int16 | ~int32 | ~int64 |
    ~uint | ~uint8 | ~uint16 | ~uint32 | ~uint64 | ~uintptr |
    ~float32 | ~float64 |
    ~string |
    ~bool
}

// IMPORTANTE: las reglas de union son:
// 1. Puedes unir tipos concretos: int | string | bool
// 2. Puedes unir tipos con tilde: ~int | ~string
// 3. Puedes mezclar: int | ~string
// 4. NO puedes unir interfaces que tengan type elements o sean comparable:
//    Numeric | Stringer  — ERROR si Numeric tiene type elements
// 5. Puedes unir interfaces que SOLO tengan metodos:
//    io.Reader | io.Writer  — OK (pero es equivalente a interface vacia
//    porque la union no tiene type set util)
```

### 5.3 Reglas de union detalladas

```go
// La union (|) en interfaces tiene reglas estrictas

// OK: unir tipos concretos
type A interface {
    int | string | float64
}

// OK: unir tipos con ~
type B interface {
    ~int | ~string
}

// OK: mezclar ~ y sin ~
type C interface {
    int | ~string    // exactamente int, o cualquier tipo basado en string
}

// OK: unir con struct literal
type D interface {
    ~int | ~struct{ X int }
}

// ERROR: no puedes unir interfaces con type elements
type Signed interface { ~int | ~int64 }
type Unsigned interface { ~uint | ~uint64 }
// type Number interface { Signed | Unsigned }  // ERROR

// SOLUCION: expandir manualmente
type Number interface {
    ~int | ~int64 | ~uint | ~uint64
}

// OK: puedes EMBEBER (interseccion) pero no UNIR constraints complejos
type SignedAndStringable interface {
    Signed      // interseccion — OK
    fmt.Stringer // interseccion — OK
}

// EXCEPCION: comparable puede embeberse pero no unirse
type ComparableStringer interface {
    comparable   // interseccion — OK
    fmt.Stringer
}
// type CompOrString interface { comparable | fmt.Stringer }  // ERROR
```

### 5.4 Construyendo un sistema de constraints reutilizable

```go
// Definir una libreria de constraints consistente para tu proyecto

package constraints

// === Constraints numericos ===

type Signed interface {
    ~int | ~int8 | ~int16 | ~int32 | ~int64
}

type Unsigned interface {
    ~uint | ~uint8 | ~uint16 | ~uint32 | ~uint64 | ~uintptr
}

type Integer interface {
    Signed | Unsigned
}

type Float interface {
    ~float32 | ~float64
}

type Complex interface {
    ~complex64 | ~complex128
}

type Number interface {
    Integer | Float
}

type RealNumber interface {
    Integer | Float | Complex
}

// === Constraints de comportamiento ===

// Serializable requiere que T pueda convertirse a/desde bytes
type Serializable interface {
    Marshal() ([]byte, error)
    Unmarshal([]byte) error
}

// Validatable requiere que T pueda validarse
type Validatable interface {
    Validate() error
}

// Cloneable requiere que T pueda clonarse
type Cloneable[T any] interface {
    Clone() T
}

// Identifiable requiere que T tenga un ID
type Identifiable[T comparable] interface {
    GetID() T
}

// === Constraints compuestos ===

// Entity combina Identifiable + Validatable
type Entity[ID comparable] interface {
    Identifiable[ID]
    Validatable
}

// Para usar:
// func SaveAll[T Entity[string]](items []T) error { ... }
```

---

## 6. Constraints con type parameters (constraints genericos)

### 6.1 Constraints parametrizados

```go
// Los constraints pueden tener sus propios type parameters

// Constraint generico: T debe poder compararse con U
type ComparableTo[U any] interface {
    CompareTo(other U) int
}

func MaxBy[T ComparableTo[T]](a, b T) T {
    if a.CompareTo(b) > 0 {
        return a
    }
    return b
}

// Implementacion
type Score int

func (s Score) CompareTo(other Score) int {
    return cmp.Compare(int(s), int(other))
}

func main() {
    fmt.Println(MaxBy(Score(85), Score(92))) // 92
}
```

### 6.2 Constraint con puntero generico

```go
// Pattern: constraint que requiere un metodo de puntero
// Problema comun: quieres que *T tenga un metodo, no T

type Decoder interface {
    Decode([]byte) error
}

// Esto NO funciona como esperas:
func DecodeAll[T Decoder](data [][]byte) []T {
    results := make([]T, len(data))
    for i, d := range data {
        results[i].Decode(d) // T.Decode — pero T es value, no pointer
        // Si Decode tiene pointer receiver, esto no compila
    }
    return results
}

// SOLUCION: constraint con pointer type parameter
type Decodable[T any] interface {
    *T
    Decode([]byte) error
}

func DecodeAll[T any, PT Decodable[T]](data [][]byte) []T {
    results := make([]T, len(data))
    for i, d := range data {
        p := PT(&results[i]) // convertir *T a PT (que tiene Decode)
        p.Decode(d)
    }
    return results
}

// Tipo que implementa Decode con pointer receiver
type User struct {
    Name string
}

func (u *User) Decode(data []byte) error {
    u.Name = string(data)
    return nil
}

func main() {
    data := [][]byte{[]byte("Alice"), []byte("Bob")}
    users := DecodeAll[User](data)
    // PT se infiere como *User
    fmt.Println(users) // [{Alice} {Bob}]
}
```

### 6.3 El pointer method constraint pattern en detalle

```go
// Este es uno de los patrones mas sutiles en Go generics
// Se usa cuando necesitas crear valores nuevos de T y llamar metodos de *T

// Caso: deserializar JSON en slices de cualquier tipo
type JSONUnmarshaler interface {
    UnmarshalJSON([]byte) error
}

// Intento naive (no funciona para pointer receivers):
func ParseJSONSliceNaive[T JSONUnmarshaler](records [][]byte) ([]T, error) {
    result := make([]T, len(records))
    for i, data := range records {
        if err := result[i].UnmarshalJSON(data); err != nil {
            return nil, err
        }
    }
    return result, nil
}
// Si T = User y UnmarshalJSON tiene pointer receiver,
// result[i].UnmarshalJSON no compila

// Solucion: dos type parameters
type Unmarshaler[T any] interface {
    *T                           // PT = *T
    UnmarshalJSON([]byte) error  // PT tiene el metodo
}

func ParseJSONSlice[T any, PT Unmarshaler[T]](records [][]byte) ([]T, error) {
    result := make([]T, len(records))
    for i, data := range records {
        pt := PT(&result[i])      // *T → PT (donde PT = *T + tiene UnmarshalJSON)
        if err := pt.UnmarshalJSON(data); err != nil {
            return nil, err
        }
    }
    return result, nil
}

// Uso:
type Item struct {
    ID   int    `json:"id"`
    Name string `json:"name"`
}

func (it *Item) UnmarshalJSON(data []byte) error {
    // implementacion custom
    return json.Unmarshal(data, it)
}

items, err := ParseJSONSlice[Item](records) // PT inferido como *Item
```

### 6.4 Constraint que vincula dos tipos

```go
// Constraint que establece una relacion entre tipos

// Convertible: T puede convertirse a U
type Convertible[U any] interface {
    Convert() U
}

func ConvertAll[T Convertible[U], U any](items []T) []U {
    result := make([]U, len(items))
    for i, item := range items {
        result[i] = item.Convert()
    }
    return result
}

// Ejemplo: convertir entre unidades
type Celsius float64
type Fahrenheit float64

func (c Celsius) Convert() Fahrenheit {
    return Fahrenheit(c*9/5 + 32)
}

func main() {
    temps := []Celsius{0, 100, 36.6}
    fahrenheit := ConvertAll[Celsius, Fahrenheit](temps)
    fmt.Println(fahrenheit) // [32 212 97.88]
}

// Reducer: tipo que puede combinarse con otros del mismo tipo
type Reducer[T any] interface {
    Reduce(other T) T
}

func ReduceAll[T Reducer[T]](items []T) T {
    if len(items) == 0 {
        var zero T
        return zero
    }
    result := items[0]
    for _, item := range items[1:] {
        result = result.Reduce(item)
    }
    return result
}

type Sum int

func (s Sum) Reduce(other Sum) Sum {
    return s + other
}

func main() {
    total := ReduceAll([]Sum{1, 2, 3, 4, 5})
    fmt.Println(total) // 15
}
```

---

## 7. Patrones avanzados de constraints

### 7.1 Enum constraint (simular enums con constraints)

```go
// Go no tiene enums, pero puedes simular "closed set" con constraints

type Direction interface {
    ~int
    isDirection() // unexported method = solo tipos en este paquete
}

type direction int

func (direction) isDirection() {} // implementa el marker method

const (
    North direction = iota
    South
    East
    West
)

func (d direction) String() string {
    names := [...]string{"North", "South", "East", "West"}
    if int(d) < len(names) {
        return names[d]
    }
    return fmt.Sprintf("Direction(%d)", d)
}

// Funcion generica que acepta solo "direcciones"
func Opposite[T Direction](d T) T {
    switch any(d).(type) {
    case direction:
        v := int(any(d).(direction))
        switch v {
        case 0: return T(direction(1)) // North → South
        case 1: return T(direction(0))
        case 2: return T(direction(3)) // East → West
        case 3: return T(direction(2))
        }
    }
    return d
}

// NOTA: Este patron es convoluto en Go.
// En la practica, usa funciones regulares con el tipo concreto
// Los genericos no aportan mucho para "enums" en Go
```

### 7.2 Type-level configuration con constraints

```go
// Usar constraints para configurar comportamiento en compilacion

type Ordering int
const (
    Ascending  Ordering = 1
    Descending Ordering = -1
)

// Constraint que define politica de ordenamiento
type OrderPolicy interface {
    ~int
    Direction() Ordering
}

type Asc int
func (Asc) Direction() Ordering { return Ascending }

type Desc int
func (Desc) Direction() Ordering { return Descending }

type SortedList[T cmp.Ordered, P OrderPolicy] struct {
    items []T
    policy P
}

func NewSortedList[T cmp.Ordered, P OrderPolicy](policy P) *SortedList[T, P] {
    return &SortedList[T, P]{policy: policy}
}

func (sl *SortedList[T, P]) Insert(item T) {
    pos, _ := slices.BinarySearchFunc(sl.items, item, func(a, b T) int {
        c := cmp.Compare(a, b)
        return c * int(sl.policy.Direction())
    })
    sl.items = slices.Insert(sl.items, pos, item)
}

func (sl *SortedList[T, P]) Items() []T {
    return slices.Clone(sl.items)
}

func main() {
    asc := NewSortedList[int](Asc(0))
    asc.Insert(5)
    asc.Insert(3)
    asc.Insert(7)
    asc.Insert(1)
    fmt.Println(asc.Items()) // [1 3 5 7]

    desc := NewSortedList[int](Desc(0))
    desc.Insert(5)
    desc.Insert(3)
    desc.Insert(7)
    desc.Insert(1)
    fmt.Println(desc.Items()) // [7 5 3 1]
}
```

### 7.3 Constraint para slices de un tipo

```go
// Constraint: T debe ser un slice de E
type SliceOf[E any] interface {
    ~[]E
}

func Flatten[S SliceOf[E], E any](slices []S) []E {
    var result []E
    for _, s := range slices {
        result = append(result, s...)
    }
    return result
}

type Names []string
type Tags []string

func main() {
    // Funciona con slices regulares
    regular := [][]int{{1, 2}, {3, 4}, {5}}
    fmt.Println(Flatten(regular)) // [1 2 3 4 5]

    // Funciona con tipos definidos sobre slices
    names := []Names{{"Alice", "Bob"}, {"Charlie"}}
    fmt.Println(Flatten(names)) // [Alice Bob Charlie]
}
```

### 7.4 Constraint para maps

```go
// Constraint: T debe ser un map de K a V
type MapOf[K comparable, V any] interface {
    ~map[K]V
}

func Keys[M MapOf[K, V], K comparable, V any](m M) []K {
    keys := make([]K, 0, len(m))
    for k := range m {
        keys = append(keys, k)
    }
    return keys
}

func Values[M MapOf[K, V], K comparable, V any](m M) []V {
    vals := make([]V, 0, len(m))
    for _, v := range m {
        vals = append(vals, v)
    }
    return vals
}

type Config map[string]string
type Scores map[string]int

func main() {
    cfg := Config{"host": "localhost", "port": "8080"}
    fmt.Println(Keys(cfg))   // [host port] (orden puede variar)
    fmt.Println(Values(cfg)) // [localhost 8080]

    scores := Scores{"Alice": 95, "Bob": 87}
    fmt.Println(Keys(scores))   // [Alice Bob]
    fmt.Println(Values(scores)) // [95 87]

    // Nota: la stdlib maps.Keys ya hace esto con iteradores (Go 1.23+)
    // Este patron es mas educativo que practico
}
```

### 7.5 Constraint para channels

```go
// Constraint: T debe ser un channel de E
type ChanOf[E any] interface {
    ~chan E | ~<-chan E  // bidireccional o solo recepcion
}

type SendChanOf[E any] interface {
    ~chan E | ~chan<- E  // bidireccional o solo envio
}

func Drain[C ChanOf[E], E any](ch C) []E {
    var result []E
    for v := range ch {
        result = append(result, v)
    }
    return result
}

func SendAll[C SendChanOf[E], E any](ch C, items []E) {
    for _, item := range items {
        ch <- item
    }
}

type EventChan chan string
type NotifyChan chan<- string

func main() {
    ch := make(EventChan, 5)
    SendAll(ch, []string{"a", "b", "c"})
    close(ch)
    fmt.Println(Drain(ch)) // [a b c]
}
```

---

## 8. Satisfaccion de constraints: edge cases

### 8.1 Interfaces y comparable

```go
// Una interface puede o no ser comparable, dependiendo del valor dinamico

type I interface {
    M()
}

type A struct{ x int }
func (A) M() {}

type B struct{ x []int } // slices no son comparable
func (B) M() {}

// Antes de Go 1.20:
// func F[T comparable](t T) { }
// F(A{}) — OK
// F(B{}) — ERROR en compilacion (B no es comparable)
// F(I(A{})) — ERROR: I no satisface comparable

// Desde Go 1.20:
// F(A{}) — OK
// F(B{}) — ERROR: B contiene []int, no comparable
// F(I(A{})) — OK: interfaces satisfacen comparable
//   pero puede panic en runtime si el valor dinamico no es comparable:

func F[T comparable](t T) {
    fmt.Println(t == t)
}

var i I = A{1}
F(i) // OK: A es comparable → true

// i = B{[]int{1}}
// F(i) // PANIC en runtime: comparing uncomparable type B
// El compilador permite la llamada, pero falla en ==
```

### 8.2 Untyped constants y constraints

```go
// Los untyped constants se adaptan al constraint

type Float interface {
    ~float32 | ~float64
}

func Half[T Float](v T) T {
    return v / 2  // 2 es untyped constant, se convierte a T
}

Half(10.0)   // T = float64, 2 se convierte a float64
Half(float32(10.0)) // T = float32, 2 se convierte a float32

// Untyped constants en llamadas
func Add[T ~int | ~float64](a, b T) T {
    return a + b
}

Add(1, 2)     // OK: 1 y 2 son untyped, T = int (default de integer constant)
Add(1.0, 2.0) // OK: T = float64 (default de float constant)
Add(1, 2.0)   // OK: ambos se unifican como float64 (2.0 fuerza float)
// Add(1, "2")  // ERROR: no puede inferir T
```

### 8.3 Type parameters en constraints de union

```go
// Un type parameter puede aparecer en una union
type Container[T any] interface {
    []T | map[string]T  // slice de T o map string→T
}

func First[C Container[T], T any](c C) (T, bool) {
    switch v := any(c).(type) {
    case []T:
        if len(v) > 0 {
            return v[0], true
        }
    case map[string]T:
        for _, val := range v {
            return val, true
        }
    }
    var zero T
    return zero, false
}

// NOTA: este patron tiene limitaciones practicas:
// - No puedes acceder a campos/metodos de C directamente
// - Necesitas type assertions via any
// - Es mas un ejercicio que un patron real
```

### 8.4 Structs anonimos y constraints

```go
// Structs anonimos identicos son el mismo tipo

type PointConstraint interface {
    ~struct{ X, Y float64 }
}

type Point struct{ X, Y float64 }
type Vector struct{ X, Y float64 }

// Point y Vector tienen el MISMO underlying type: struct{ X, Y float64 }
// Ambos satisfacen PointConstraint

func Translate[T PointConstraint](p T, dx, dy float64) T {
    // PROBLEMA: no puedes acceder a .X y .Y a traves del constraint
    // El constraint dice que el underlying type es struct{X,Y float64}
    // pero el compilador no permite field access via type parameter

    // Esto NO compila:
    // p.X += dx  // ERROR: p.X undefined (type T has no field or method X)

    // SOLUCION: type assertion via any()
    // Pero esto es fragil y poco practico

    return p
}

// CONCLUSION: ~ con structs es legal pero rara vez util
// Mejor usar interfaces con metodos:
type Translatable interface {
    Translate(dx, dy float64)
    Position() (float64, float64)
}
```

---

## 9. Comparacion profunda con Rust traits

### 9.1 Go constraints vs Rust trait bounds

```go
// GO: constraint = interface (method set + type set)
type Printable interface {
    ~int | ~string
    String() string
}

func Print[T Printable](v T) {
    fmt.Println(v.String())
}
```

```rust
// RUST: trait bound = trait (method signatures + associated types + etc)
trait Printable: Display {
    fn print(&self) {
        println!("{}", self);
    }
}

fn print<T: Printable>(v: &T) {
    v.print();
}
```

### 9.2 Tabla comparativa detallada

| Aspecto | Go constraints | Rust trait bounds |
|---|---|---|
| Definicion | Interface con methods + type elements | Trait con methods + associated types |
| Sintaxis | `type C interface { ~int; M() }` | `trait C: SubTrait { fn m(&self); }` |
| Aplicacion | `func F[T C]()` | `fn f<T: C>()` o `fn f(t: impl C)` |
| Method sets | `String() string` | `fn to_string(&self) -> String` |
| Type sets | `~int \| ~string` | No hay equivalente directo |
| Operadores | Via type elements: `~int` da `+` | Via trait: `Add`, `Mul`, `Ord` |
| Multiple bounds | Embeber interfaces | `T: A + B + C` |
| Associated types | **No** | `type Item;` en trait |
| Default methods | **No** | Si: `fn method(&self) { default }` |
| Const generics | **No** | `<const N: usize>` |
| Where clauses | **No** (todo inline) | `where T: Clone + Debug, U: Into<T>` |
| Negative bounds | **No** | `!Send` (limitado) |
| Supertraits | Embeber: `comparable; M()` | `: SuperTrait` |
| Blanket impls | **No** | `impl<T: Display> ToString for T` |
| Orphan rule | No aplica (interfaces implicitas) | Si (trait o tipo deben ser locales) |
| Coherence | No aplica | Si (no hay impls conflicting) |
| Satisfaccion | Implicita (duck typing) | Explicita (`impl Trait for Type`) |
| Dynamic dispatch | `interface` regular (siempre disponible) | `dyn Trait` (opt-in, con restricciones) |
| Compile-time check | Si | Si (mas exhaustivo) |

### 9.3 Operadores: Go vs Rust

```go
// GO: los operadores se habilitan via TYPE ELEMENTS
// Solo tipos built-in tienen operadores — no puedes agregar + a un struct

type Addable interface {
    ~int | ~int8 | ~int16 | ~int32 | ~int64 |
    ~uint | ~uint8 | ~uint16 | ~uint32 | ~uint64 |
    ~float32 | ~float64 |
    ~string  // string tiene + para concatenacion
    // ~complex64 | ~complex128  // complex tiene + tambien
}

func Add[T Addable](a, b T) T {
    return a + b  // + funciona porque todos los tipos en la union lo soportan
}

// NO puedes hacer:
type Vector2D struct{ X, Y float64 }
// No hay forma de hacer Vector2D + Vector2D
// No hay operator overloading en Go
```

```rust
// RUST: los operadores se habilitan via TRAITS de std::ops
use std::ops::{Add, Sub, Mul};

fn add<T: Add<Output=T>>(a: T, b: T) -> T {
    a + b  // desugars to a.add(b)
}

// PUEDES agregar + a tus tipos:
#[derive(Debug, Clone, Copy)]
struct Vector2D { x: f64, y: f64 }

impl Add for Vector2D {
    type Output = Vector2D;

    fn add(self, rhs: Vector2D) -> Vector2D {
        Vector2D {
            x: self.x + rhs.x,
            y: self.y + rhs.y,
        }
    }
}

let v = Vector2D { x: 1.0, y: 2.0 } + Vector2D { x: 3.0, y: 4.0 };
// v = Vector2D { x: 4.0, y: 6.0 }

// Multiple trait bounds para operaciones aritmeticas completas:
fn magnitude<T>(v: T) -> f64
where
    T: Add<Output=T> + Mul<Output=T> + Into<f64> + Copy,
{
    // ...
}
```

**Diferencia fundamental**: en Go, los operadores estan atados a tipos built-in. En Rust, los operadores son traits que cualquier tipo puede implementar. Esto significa que en Rust puedes escribir funciones genericas que operen sobre "cualquier tipo con +", incluyendo tipos custom; en Go solo puedes operar sobre tipos numericos/string.

### 9.4 Associated types (Rust) vs Go

```rust
// RUST: los traits pueden tener ASSOCIATED TYPES
// Estos tipos se determinan por la implementacion, no por el caller

trait Iterator {
    type Item;  // associated type: cada implementacion define su Item

    fn next(&mut self) -> Option<Self::Item>;
}

// Implementacion: Item se fija a i32
struct Counter { count: i32, max: i32 }

impl Iterator for Counter {
    type Item = i32;  // Counter produce i32

    fn next(&mut self) -> Option<i32> {
        if self.count < self.max {
            self.count += 1;
            Some(self.count)
        } else {
            None
        }
    }
}

// El caller NO especifica Item — esta determinado por el tipo
fn sum_all<I: Iterator<Item=i32>>(iter: I) -> i32 {
    // ...
}
```

```go
// GO: no tiene associated types
// Emula con type parameter en la interface

type Iterator[T any] interface {
    Next() (T, bool)
}

type Counter struct {
    count, max int
}

func (c *Counter) Next() (int, bool) {
    if c.count < c.max {
        c.count++
        return c.count, true
    }
    return 0, false
}

// El caller SI especifica T — debe coincidir con la implementacion
func SumAll[T interface{ ~int | ~int64 }](iter Iterator[T]) T {
    var sum T
    for {
        v, ok := iter.Next()
        if !ok {
            break
        }
        sum += v
    }
    return sum
}

// Diferencia: en Rust, Iterator tiene UN Item por tipo
// En Go, Iterator[T] es parametrizado — el caller elige T
// No hay forma de "fijar" T por tipo como en Rust
```

### 9.5 Blanket implementations (Rust) vs Go

```rust
// RUST: blanket implementation — impl para TODOS los T que cumplan un bound

// Cualquier tipo que implemente Display automaticamente tiene ToString
impl<T: Display> ToString for T {
    fn to_string(&self) -> String {
        format!("{self}")
    }
}

// Ahora i32, String, cualquier tipo con Display tiene .to_string()
let s: String = 42.to_string(); // funciona por blanket impl
```

```go
// GO: no tiene blanket implementations
// Cada tipo debe satisfacer cada interface individualmente

// No puedes decir "todo tipo con String() automaticamente tiene Format()"
// Debes implementar Format() para cada tipo

// Lo mas cercano es un wrapper generico:
type Formatter[T fmt.Stringer] struct {
    Value T
}

func (f Formatter[T]) Format() string {
    return fmt.Sprintf("[%s]", f.Value.String())
}

// Pero el usuario debe envolver explicitamente:
s := Formatter[MyType]{Value: myVal}
s.Format()
// No es equivalente a un blanket impl
```

---

## 10. Constraints y la stdlib: patrones reales

### 10.1 sort.Interface vs slices.SortFunc

```go
// ANTES (Go < 1.21): interface-based sorting
type Interface interface {
    Len() int
    Less(i, j int) bool
    Swap(i, j int)
}

// Cada tipo necesita implementar 3 metodos
type ByAge []Person

func (a ByAge) Len() int           { return len(a) }
func (a ByAge) Less(i, j int) bool { return a[i].Age < a[j].Age }
func (a ByAge) Swap(i, j int)      { a[i], a[j] = a[j], a[i] }

sort.Sort(ByAge(people))

// AHORA (Go 1.21+): generic-based sorting
slices.SortFunc(people, func(a, b Person) int {
    return cmp.Compare(a.Age, b.Age)
})

// O para tipos Ordered:
slices.Sort(numbers) // T inferred, constraint cmp.Ordered

// La firma de slices.Sort:
// func Sort[S ~[]E, E cmp.Ordered](x S)
//   S con constraint ~[]E: acepta cualquier slice type
//   E con constraint cmp.Ordered: los elementos deben ser ordenables
```

### 10.2 Constraint pattern en slices package

```go
// El paquete slices usa un patron especifico de constraints:

// Patron 1: S ~[]E (slice de elementos)
func Sort[S ~[]E, E cmp.Ordered](x S)
func Contains[S ~[]E, E comparable](s S, v E) bool
func Index[S ~[]E, E comparable](s S, v E) int

// Por que S ~[]E en vez de solo []E?
// Para que funcione con tipos definidos sobre slices:
type Names []string    // underlying type = []string
slices.Sort(Names{"Charlie", "Alice", "Bob"})
// S = Names (~[]string), E = string

// Si fuera []E en vez de S ~[]E:
// slices.Sort(Names{...}) NO compilaria (Names != []string)

// Patron 2: func(E, E) int para comparadores custom
func SortFunc[S ~[]E, E any](x S, cmp func(a, b E) int)
func MinFunc[S ~[]E, E any](x S, cmp func(a, b E) int) E
func MaxFunc[S ~[]E, E any](x S, cmp func(a, b E) int) E
func ContainsFunc[S ~[]E, E any](s S, f func(E) bool) bool

// Patron 3: doble constraint para BinarySearch
func BinarySearch[S ~[]E, E cmp.Ordered](x S, target E) (int, bool)
func BinarySearchFunc[S ~[]E, E, T any](x S, target T, cmp func(E, T) int) (int, bool)
// BinarySearchFunc permite buscar con tipo diferente al del slice
```

### 10.3 Constraint pattern en maps package

```go
// El paquete maps usa un patron similar

func Clone[M ~map[K]V, K comparable, V any](m M) M
func Copy[M1 ~map[K]V, M2 ~map[K]V, K comparable, V any](dst M1, src M2)
func Equal[M1 ~map[K]V, M2 ~map[K]V, K comparable, V comparable](m1 M1, m2 M2) bool
func EqualFunc[M1 ~map[K]V1, M2 ~map[K]V2, K comparable, V1, V2 any](
    m1 M1, m2 M2, eq func(V1, V2) bool) bool
func DeleteFunc[M ~map[K]V, K comparable, V any](m M, del func(K, V) bool)

// Notar:
// - M ~map[K]V: acepta tipos definidos sobre maps
// - K comparable: keys siempre deben ser comparable
// - V puede ser any o comparable dependiendo de la funcion
// - Equal requiere V comparable (para ==)
// - EqualFunc permite V any (el caller provee la comparacion)
```

### 10.4 Constraint pattern en sync package

```go
// sync.Map NO es generico (por razones historicas)
// sync.Pool NO es generico

// Pero sync.OnceValue y sync.OnceValues (Go 1.21+) SI son genericos:

func sync.OnceValue[T any](f func() T) func() T
func sync.OnceValues[T1, T2 any](f func() (T1, T2)) func() (T1, T2)

// Uso: lazy initialization thread-safe
var getConfig = sync.OnceValue(func() Config {
    // Solo se ejecuta la primera vez, resultado cacheado
    cfg, err := loadConfig("config.yaml")
    if err != nil {
        panic(err)
    }
    return cfg
})

cfg := getConfig() // primera vez: carga y cachea
cfg = getConfig()  // segunda vez: retorna cacheado
```

---

## 11. Programa completo: sistema de validacion generico

```go
package main

import (
    "cmp"
    "fmt"
    "regexp"
    "slices"
    "strings"
    "time"
    "unicode/utf8"
)

// ============================================================================
// Validator framework: validacion generica con constraints composables
// ============================================================================

// ValidationError representa un error de validacion en un campo
type ValidationError struct {
    Field   string
    Message string
}

func (e ValidationError) Error() string {
    return fmt.Sprintf("%s: %s", e.Field, e.Message)
}

// ValidationResult acumula errores de validacion
type ValidationResult struct {
    Errors []ValidationError
}

func (r *ValidationResult) AddError(field, message string) {
    r.Errors = append(r.Errors, ValidationError{Field: field, Message: message})
}

func (r *ValidationResult) IsValid() bool {
    return len(r.Errors) == 0
}

func (r *ValidationResult) Error() string {
    if r.IsValid() {
        return ""
    }
    msgs := make([]string, len(r.Errors))
    for i, e := range r.Errors {
        msgs[i] = e.Error()
    }
    return strings.Join(msgs, "; ")
}

// Merge combina multiples results
func (r *ValidationResult) Merge(other *ValidationResult) {
    r.Errors = append(r.Errors, other.Errors...)
}

// ============================================================================
// Rule: una regla de validacion generica
// ============================================================================

// Rule es una funcion que valida un valor y retorna un error message o ""
type Rule[T any] func(value T) string

// ============================================================================
// Reglas para tipos Ordered (numericos y strings)
// ============================================================================

// Min verifica que el valor sea >= min
func Min[T cmp.Ordered](min T) Rule[T] {
    return func(value T) string {
        if value < min {
            return fmt.Sprintf("must be >= %v", min)
        }
        return ""
    }
}

// Max verifica que el valor sea <= max
func Max[T cmp.Ordered](max T) Rule[T] {
    return func(value T) string {
        if value > max {
            return fmt.Sprintf("must be <= %v", max)
        }
        return ""
    }
}

// Between verifica que el valor este en [min, max]
func Between[T cmp.Ordered](min, max T) Rule[T] {
    return func(value T) string {
        if value < min || value > max {
            return fmt.Sprintf("must be between %v and %v", min, max)
        }
        return ""
    }
}

// OneOf verifica que el valor sea uno de los permitidos
func OneOf[T comparable](allowed ...T) Rule[T] {
    return func(value T) string {
        if !slices.Contains(allowed, value) {
            return fmt.Sprintf("must be one of %v", allowed)
        }
        return ""
    }
}

// ============================================================================
// Reglas para strings
// ============================================================================

// NotEmpty verifica que el string no este vacio
func NotEmpty() Rule[string] {
    return func(value string) string {
        if strings.TrimSpace(value) == "" {
            return "must not be empty"
        }
        return ""
    }
}

// MinLength verifica longitud minima (en runes, no bytes)
func MinLength(n int) Rule[string] {
    return func(value string) string {
        if utf8.RuneCountInString(value) < n {
            return fmt.Sprintf("must be at least %d characters", n)
        }
        return ""
    }
}

// MaxLength verifica longitud maxima
func MaxLength(n int) Rule[string] {
    return func(value string) string {
        if utf8.RuneCountInString(value) > n {
            return fmt.Sprintf("must be at most %d characters", n)
        }
        return ""
    }
}

// Matches verifica contra un regex
func Matches(pattern string, description string) Rule[string] {
    re := regexp.MustCompile(pattern)
    return func(value string) string {
        if !re.MatchString(value) {
            return fmt.Sprintf("must be %s", description)
        }
        return ""
    }
}

// HasPrefix verifica prefijo
func HasPrefix(prefix string) Rule[string] {
    return func(value string) string {
        if !strings.HasPrefix(value, prefix) {
            return fmt.Sprintf("must start with %q", prefix)
        }
        return ""
    }
}

// ============================================================================
// Reglas para slices
// ============================================================================

// NotEmptySlice verifica que el slice no este vacio
func NotEmptySlice[T any]() Rule[[]T] {
    return func(value []T) string {
        if len(value) == 0 {
            return "must not be empty"
        }
        return ""
    }
}

// MinItems verifica tamano minimo del slice
func MinItems[T any](n int) Rule[[]T] {
    return func(value []T) string {
        if len(value) < n {
            return fmt.Sprintf("must have at least %d items", n)
        }
        return ""
    }
}

// MaxItems verifica tamano maximo del slice
func MaxItems[T any](n int) Rule[[]T] {
    return func(value []T) string {
        if len(value) > n {
            return fmt.Sprintf("must have at most %d items", n)
        }
        return ""
    }
}

// Each aplica una regla a cada elemento del slice
func Each[T any](rule Rule[T]) Rule[[]T] {
    return func(value []T) string {
        for i, item := range value {
            if msg := rule(item); msg != "" {
                return fmt.Sprintf("item[%d]: %s", i, msg)
            }
        }
        return ""
    }
}

// UniqueItems verifica que no haya duplicados
func UniqueItems[T comparable]() Rule[[]T] {
    return func(value []T) string {
        seen := make(map[T]struct{})
        for _, item := range value {
            if _, ok := seen[item]; ok {
                return fmt.Sprintf("duplicate item: %v", item)
            }
            seen[item] = struct{}{}
        }
        return ""
    }
}

// ============================================================================
// Reglas para punteros (opcionales)
// ============================================================================

// Required verifica que el puntero no sea nil
func Required[T any]() Rule[*T] {
    return func(value *T) string {
        if value == nil {
            return "is required"
        }
        return ""
    }
}

// IfPresent aplica reglas solo si el puntero no es nil
func IfPresent[T any](rules ...Rule[T]) Rule[*T] {
    return func(value *T) string {
        if value == nil {
            return ""
        }
        for _, rule := range rules {
            if msg := rule(*value); msg != "" {
                return msg
            }
        }
        return ""
    }
}

// ============================================================================
// Reglas para time.Time
// ============================================================================

// After verifica que la fecha sea despues de ref
func After(ref time.Time) Rule[time.Time] {
    return func(value time.Time) string {
        if !value.After(ref) {
            return fmt.Sprintf("must be after %s", ref.Format("2006-01-02"))
        }
        return ""
    }
}

// Before verifica que la fecha sea antes de ref
func Before(ref time.Time) Rule[time.Time] {
    return func(value time.Time) string {
        if !value.Before(ref) {
            return fmt.Sprintf("must be before %s", ref.Format("2006-01-02"))
        }
        return ""
    }
}

// NotZeroTime verifica que la fecha no sea zero value
func NotZeroTime() Rule[time.Time] {
    return func(value time.Time) string {
        if value.IsZero() {
            return "must not be empty"
        }
        return ""
    }
}

// ============================================================================
// Custom rules
// ============================================================================

// Predicate crea una regla a partir de una funcion booleana
func Predicate[T any](pred func(T) bool, message string) Rule[T] {
    return func(value T) string {
        if !pred(value) {
            return message
        }
        return ""
    }
}

// Transform valida un valor transformado
func Transform[T any, U any](transform func(T) U, rules ...Rule[U]) Rule[T] {
    return func(value T) string {
        transformed := transform(value)
        for _, rule := range rules {
            if msg := rule(transformed); msg != "" {
                return msg
            }
        }
        return ""
    }
}

// ============================================================================
// Combinadores
// ============================================================================

// All combina reglas con AND — todas deben pasar
func All[T any](rules ...Rule[T]) Rule[T] {
    return func(value T) string {
        for _, rule := range rules {
            if msg := rule(value); msg != "" {
                return msg
            }
        }
        return ""
    }
}

// Any combina reglas con OR — al menos una debe pasar
func Any[T any](rules ...Rule[T]) Rule[T] {
    return func(value T) string {
        var msgs []string
        for _, rule := range rules {
            msg := rule(value)
            if msg == "" {
                return "" // al menos una paso
            }
            msgs = append(msgs, msg)
        }
        return strings.Join(msgs, " OR ")
    }
}

// Not invierte una regla
func Not[T any](rule Rule[T], message string) Rule[T] {
    return func(value T) string {
        if msg := rule(value); msg == "" {
            return message // la regla original paso, pero Not la invierte
        }
        return "" // la regla fallo, Not lo convierte en exito
    }
}

// ============================================================================
// FieldValidator: validar campos de un struct
// ============================================================================

// FieldValidator valida un struct campo por campo
type FieldValidator[T any] struct {
    validators []func(T) *ValidationResult
}

func NewValidator[T any]() *FieldValidator[T] {
    return &FieldValidator[T]{}
}

// Field agrega validacion para un campo especifico
func Field[T any, F any](v *FieldValidator[T], name string, extract func(T) F, rules ...Rule[F]) {
    v.validators = append(v.validators, func(value T) *ValidationResult {
        result := &ValidationResult{}
        fieldValue := extract(value)
        for _, rule := range rules {
            if msg := rule(fieldValue); msg != "" {
                result.AddError(name, msg)
                break // primer error por campo
            }
        }
        return result
    })
}

// Validate ejecuta todas las validaciones
func (v *FieldValidator[T]) Validate(value T) *ValidationResult {
    result := &ValidationResult{}
    for _, validator := range v.validators {
        result.Merge(validator(value))
    }
    return result
}

// ============================================================================
// Tipos de dominio y validacion
// ============================================================================

type CreateUserRequest struct {
    Username string
    Email    string
    Age      int
    Bio      *string  // opcional
    Tags     []string
    Birthday time.Time
}

var emailRegex = `^[a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,}$`

func NewUserValidator() *FieldValidator[CreateUserRequest] {
    v := NewValidator[CreateUserRequest]()

    Field(v, "username",
        func(u CreateUserRequest) string { return u.Username },
        NotEmpty(),
        MinLength(3),
        MaxLength(20),
        Matches(`^[a-zA-Z0-9_]+$`, "alphanumeric with underscores"),
    )

    Field(v, "email",
        func(u CreateUserRequest) string { return u.Email },
        NotEmpty(),
        Matches(emailRegex, "a valid email"),
    )

    Field(v, "age",
        func(u CreateUserRequest) int { return u.Age },
        Between(13, 150),
    )

    Field(v, "bio",
        func(u CreateUserRequest) *string { return u.Bio },
        IfPresent(MaxLength(500)),
    )

    Field(v, "tags",
        func(u CreateUserRequest) []string { return u.Tags },
        MaxItems[string](10),
        Each(All(NotEmpty(), MaxLength(30))),
        UniqueItems[string](),
    )

    Field(v, "birthday",
        func(u CreateUserRequest) time.Time { return u.Birthday },
        NotZeroTime(),
        Before(time.Now()),
        After(time.Date(1900, 1, 1, 0, 0, 0, 0, time.UTC)),
    )

    return v
}

// ============================================================================
// Otro tipo de dominio: Product
// ============================================================================

type CreateProductRequest struct {
    Name     string
    SKU      string
    Price    float64
    Quantity int
    Category string
    Sizes    []string
}

func NewProductValidator() *FieldValidator[CreateProductRequest] {
    v := NewValidator[CreateProductRequest]()

    Field(v, "name",
        func(p CreateProductRequest) string { return p.Name },
        NotEmpty(),
        MinLength(2),
        MaxLength(100),
    )

    Field(v, "sku",
        func(p CreateProductRequest) string { return p.SKU },
        NotEmpty(),
        Matches(`^[A-Z]{2,4}-\d{3,6}$`, "format XX-000 to XXXX-000000"),
    )

    Field(v, "price",
        func(p CreateProductRequest) float64 { return p.Price },
        Min(0.01),
        Max(999999.99),
    )

    Field(v, "quantity",
        func(p CreateProductRequest) int { return p.Quantity },
        Min(0),
        Max(1000000),
    )

    Field(v, "category",
        func(p CreateProductRequest) string { return p.Category },
        NotEmpty(),
        OneOf("electronics", "clothing", "food", "furniture", "other"),
    )

    Field(v, "sizes",
        func(p CreateProductRequest) []string { return p.Sizes },
        NotEmptySlice[string](),
        Each(OneOf("XS", "S", "M", "L", "XL", "XXL")),
        UniqueItems[string](),
    )

    return v
}

// ============================================================================
// Main
// ============================================================================

func main() {
    userValidator := NewUserValidator()
    productValidator := NewProductValidator()

    fmt.Println("=== User Validation ===")
    fmt.Println()

    bio := "Go developer and Rust enthusiast"

    // Caso 1: valido
    validUser := CreateUserRequest{
        Username: "alice_42",
        Email:    "alice@example.com",
        Age:      30,
        Bio:      &bio,
        Tags:     []string{"go", "rust", "linux"},
        Birthday: time.Date(1994, 5, 15, 0, 0, 0, 0, time.UTC),
    }
    result := userValidator.Validate(validUser)
    fmt.Printf("Valid user: valid=%v\n", result.IsValid()) // true

    // Caso 2: multiples errores
    invalidUser := CreateUserRequest{
        Username: "ab",                                 // too short
        Email:    "not-an-email",                       // invalid format
        Age:      10,                                   // too young
        Bio:      nil,                                  // OK (optional)
        Tags:     []string{"go", "", "go"},             // empty tag + duplicate
        Birthday: time.Date(2030, 1, 1, 0, 0, 0, 0, time.UTC), // future
    }
    result = userValidator.Validate(invalidUser)
    fmt.Printf("Invalid user: valid=%v\n", result.IsValid())
    for _, err := range result.Errors {
        fmt.Printf("  ✗ %s\n", err)
    }

    // Caso 3: edge cases
    longBio := strings.Repeat("x", 501)
    edgeUser := CreateUserRequest{
        Username: "user with spaces",  // invalid chars
        Email:    "valid@email.com",
        Age:      200,                 // too old
        Bio:      &longBio,            // too long
        Tags:     []string{},          // empty is OK (no NotEmptySlice rule for user)
        Birthday: time.Date(1850, 1, 1, 0, 0, 0, 0, time.UTC), // too old
    }
    result = userValidator.Validate(edgeUser)
    fmt.Printf("\nEdge case user: valid=%v\n", result.IsValid())
    for _, err := range result.Errors {
        fmt.Printf("  ✗ %s\n", err)
    }

    fmt.Println()
    fmt.Println("=== Product Validation ===")
    fmt.Println()

    // Producto valido
    validProduct := CreateProductRequest{
        Name:     "Wireless Keyboard",
        SKU:      "KB-00142",
        Price:    79.99,
        Quantity: 150,
        Category: "electronics",
        Sizes:    []string{"S", "M", "L"},
    }
    result = productValidator.Validate(validProduct)
    fmt.Printf("Valid product: valid=%v\n", result.IsValid())

    // Producto invalido
    invalidProduct := CreateProductRequest{
        Name:     "",                  // empty
        SKU:      "invalid-sku",       // wrong format
        Price:    -10,                 // negative
        Quantity: -5,                  // negative
        Category: "weapons",          // not in allowed list
        Sizes:    []string{"XXL", "XXXL", "M", "XXL"}, // XXXL invalid + dup XXL
    }
    result = productValidator.Validate(invalidProduct)
    fmt.Printf("Invalid product: valid=%v\n", result.IsValid())
    for _, err := range result.Errors {
        fmt.Printf("  ✗ %s\n", err)
    }

    fmt.Println()
    fmt.Println("=== Combinadores ===")
    fmt.Println()

    // All: todas las reglas deben pasar
    strictName := All(NotEmpty(), MinLength(5), MaxLength(50))
    fmt.Printf("'Go': %q\n", strictName("Go"))         // error (too short)
    fmt.Printf("'Golang': %q\n", strictName("Golang")) // "" (valid)

    // Any: al menos una debe pasar
    idOrEmail := Any(
        Matches(`^\d+$`, "a numeric ID"),
        Matches(emailRegex, "a valid email"),
    )
    fmt.Printf("'12345': %q\n", idOrEmail("12345"))          // "" (valid — es numeric)
    fmt.Printf("'a@b.com': %q\n", idOrEmail("a@b.com"))      // "" (valid — es email)
    fmt.Printf("'invalid': %q\n", idOrEmail("invalid"))       // error (neither)

    // Not: invertir
    notAdmin := Not[string](
        Predicate(func(s string) bool { return s == "admin" }, ""),
        "must not be 'admin'",
    )
    fmt.Printf("'user': %q\n", notAdmin("user"))   // "" (valid)
    fmt.Printf("'admin': %q\n", notAdmin("admin")) // error

    // Transform: validar valor transformado
    trimmedNotEmpty := Transform(strings.TrimSpace, NotEmpty())
    fmt.Printf("'  ': %q\n", trimmedNotEmpty("  "))           // error (empty after trim)
    fmt.Printf("' hi ': %q\n", trimmedNotEmpty(" hi "))       // "" (valid after trim)

    // Predicate: regla custom
    isEven := Predicate(func(n int) bool { return n%2 == 0 }, "must be even")
    fmt.Printf("3: %q\n", isEven(3))  // error
    fmt.Printf("4: %q\n", isEven(4))  // "" (valid)
}
```

```
=== User Validation ===

Valid user: valid=true
Invalid user: valid=false
  ✗ username: must be at least 3 characters
  ✗ email: must be a valid email
  ✗ age: must be between 13 and 150
  ✗ tags: item[1]: must not be empty
  ✗ birthday: must be before 2026-04-04

Edge case user: valid=false
  ✗ username: must be alphanumeric with underscores
  ✗ age: must be between 13 and 150
  ✗ bio: must be at most 500 characters
  ✗ birthday: must be after 1900-01-01

=== Product Validation ===

Valid product: valid=true
Invalid product: valid=false
  ✗ name: must not be empty
  ✗ sku: must be format XX-000 to XXXX-000000
  ✗ price: must be >= 0.01
  ✗ quantity: must be >= 0
  ✗ category: must be one of [electronics clothing food furniture other]
  ✗ sizes: item[1]: must be one of [XS S M L XL XXL]

=== Combinadores ===

'Go': "must be at least 5 characters"
'Golang': ""
'12345': ""
'a@b.com': ""
'invalid': "must be a numeric ID OR must be a valid email"
'user': ""
'admin': "must not be 'admin'"
'  ': "must not be empty"
' hi ': ""
3: "must be even"
4: ""
```

**Puntos clave del programa**:

1. **`Rule[T any]`**: una regla de validacion es simplemente `func(T) string`. El type parameter T hace que las reglas sean type-safe — no puedes aplicar `MinLength` a un `int`.

2. **Constraints escalonados**: `Min`/`Max`/`Between` requieren `cmp.Ordered`. `OneOf`/`UniqueItems` requieren `comparable`. `NotEmpty`/`MinLength` son para `string`. `Each` requiere `Rule[T]` y retorna `Rule[[]T]`. Cada regla pide exactamente lo que necesita.

3. **`Field` function top-level**: `Field[T, F]` necesita dos type parameters (struct T, campo F) — por eso es funcion top-level, no metodo de `FieldValidator` (limitacion de method type params).

4. **Combinadores genericos**: `All`, `Any`, `Not`, `Transform` combinan reglas arbitrarias. Son genericos sobre T, composables infinitamente.

5. **`IfPresent[T]`**: demuestra constraints sobre punteros — solo valida si `*T != nil`, convirtiendo `Rule[T]` en `Rule[*T]`.

6. **Reutilizacion**: el mismo framework valida `CreateUserRequest` y `CreateProductRequest` sin duplicacion. Agregar un nuevo tipo solo requiere definir las reglas.

---

## 12. Ejercicios

### Ejercicio 1: Constraint taxonomy completo

Define un sistema completo de constraints para tu proyecto:

```go
// 1. Integer (Signed | Unsigned)
// 2. Float (~float32 | ~float64)
// 3. Number (Integer | Float)
// 4. Hashable (comparable — puede ser key de map)
// 5. Stringish (~string | ~[]byte) — tipos basados en string o byte slice
```

Implementa funciones que usen cada constraint:
- `Sum[T Number]([]T) T`
- `Product[T Number]([]T) T`
- `FrequencyMap[T Hashable]([]T) map[T]int`
- `ToUpper[T Stringish](T) T` — convierte a mayusculas (hint: usa conversion a string/[]byte)

### Ejercicio 2: Validacion con constraints avanzados

Extiende el sistema de validacion del programa completo:

1. **`When[T any]`** — regla condicional: `When(condition func(T) bool, rules ...Rule[T])` — solo aplica reglas si la condicion es true
2. **`Cross[T any]`** — validacion entre campos: `Cross(v *FieldValidator[T], rule func(T) string)` — valida el struct completo
3. **`Async[T any]`** — regla asincrona: `Async(rule func(T) <-chan string)` — ejecuta la validacion en una goroutine y retorna cuando termina
4. **`Memoize[T comparable]`** — cachea resultados de reglas costosas

### Ejercicio 3: Type-safe builder con constraints

Implementa un query builder que use constraints para garantizar type safety:

```go
query := Select[User]().
    Where(Field("age", GreaterThan(18))).
    Where(Field("status", Equals("active"))).
    OrderBy("name", Asc).
    Limit(10).
    Build()
```

Usa constraints para asegurar que:
- Los campos existen (via constraint con metodo `FieldValue(string) any`)
- Los comparadores son apropiados para el tipo del campo
- `Limit` acepta solo enteros positivos

### Ejercicio 4: Algebra de constraints

Experimenta con la interseccion de constraints:

```go
// 1. Define un constraint A = ~int | ~string | ~float64
// 2. Define un constraint B = ~int | ~bool
// 3. Define un constraint C que sea la interseccion (A embeds + B embeds)
// 4. Verifica que C solo acepta ~int (la interseccion)
// 5. Define un constraint D = C + Stringer (interseccion con metodo)
// 6. Crea un tipo MyInt int con String() y verifica que satisface D
// 7. Verifica que int NO satisface D (no tiene String())
```

Escribe funciones que acepten cada constraint y prueba con distintos tipos para verificar que el compilador rechaza los que no pertenecen al type set.

---

> **Fin de la Seccion 1: Genericos (Go 1.18+)**
> **Siguiente**: C11/S02 - Testing, T01 - Tests basicos — testing.T, go test, naming (*_test.go, Test*), t.Run (subtests), t.Parallel
