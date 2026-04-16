# Tipos genericos — structs y metodos con type parameters, limitaciones actuales

## 1. Introduccion

En T01 vimos funciones genericas — funciones parametrizadas por tipos. Este topico extiende los genericos a **tipos**: structs, interfaces y metodos que operan sobre type parameters. Aqui es donde los genericos se vuelven realmente potentes: puedes construir **contenedores de datos** (Stack, Queue, LinkedList, Tree), **abstracciones reutilizables** (Result, Optional, Pair) y **patrones arquitectonicos** (Repository, EventBus, Pool) que son type-safe sin duplicacion.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                    TIPOS GENERICOS EN GO — ANATOMIA                             │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  DEFINICION DE TIPO GENERICO                                                    │
│  ────────────────────────────                                                   │
│                                                                                  │
│  type Stack[T any] struct {                                                     │
│       ───── ─ ───  ──────                                                       │
│       │     │  │    └── campos pueden usar T                                   │
│       │     │  └── constraint (que puede hacer T)                              │
│       │     └── type parameter                                                 │
│       └── nombre del tipo                                                      │
│                                                                                  │
│  METODO EN TIPO GENERICO                                                        │
│  ───────────────────────                                                        │
│                                                                                  │
│  func (s *Stack[T]) Push(v T) {                                                │
│        ──────────    ────────                                                   │
│        │             └── parametros pueden usar T                              │
│        └── receiver repite [T] pero SIN constraint                             │
│            (el constraint ya se declaro en el tipo)                             │
│                                                                                  │
│  INSTANCIACION                                                                  │
│  ─────────────                                                                  │
│                                                                                  │
│  var s Stack[int]              ← tipo explicito                                │
│  s := Stack[string]{items: x}  ← literal con type argument                    │
│  s := NewStack[int]()          ← constructor generico                          │
│                                                                                  │
│  LIMITACION FUNDAMENTAL                                                         │
│  ─────────────────────                                                          │
│                                                                                  │
│  func (s *Stack[T]) Map[U any](fn func(T) U) Stack[U]  ← NO COMPILA          │
│                          ^^^^^                                                  │
│  Los metodos NO pueden declarar type parameters propios                        │
│  Solo pueden usar los type parameters del tipo receptor                        │
│  Solucion: funciones top-level                                                 │
│                                                                                  │
│  func Map[T any, U any](s Stack[T], fn func(T) U) Stack[U]  ← OK             │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Structs genericos: sintaxis y basicos

### 2.1 Definicion basica

```go
// Pair — contiene dos valores de tipos posiblemente diferentes
type Pair[A any, B any] struct {
    First  A
    Second B
}

// Constructor (opcional, pero util para inferencia)
func NewPair[A any, B any](first A, second B) Pair[A, B] {
    return Pair[A, B]{First: first, Second: second}
}

func main() {
    // Instanciacion explicita
    p1 := Pair[string, int]{First: "age", Second: 30}

    // Via constructor (inferencia de tipos)
    p2 := NewPair("name", "Alice")  // Pair[string, string]
    p3 := NewPair(42, true)         // Pair[int, bool]

    // Via var (zero values)
    var p4 Pair[float64, string]    // {0, ""}

    fmt.Println(p1) // {age 30}
    fmt.Println(p2) // {name Alice}
    fmt.Println(p3) // {42 true}
    fmt.Println(p4) // {0 }
}
```

### 2.2 Multiples type parameters y anidamiento

```go
// Triple — tres valores
type Triple[A, B, C any] struct {
    First  A
    Second B
    Third  C
}

// Nota: "A, B, C any" es shorthand para "A any, B any, C any"
// Solo funciona si todos tienen la misma constraint

// Anidamiento de tipos genericos
type Tree[T any] struct {
    Value T
    Left  *Tree[T]  // referencia recursiva — OK con punteros
    Right *Tree[T]
}

func NewLeaf[T any](value T) *Tree[T] {
    return &Tree[T]{Value: value}
}

func NewNode[T any](value T, left, right *Tree[T]) *Tree[T] {
    return &Tree[T]{Value: value, Left: left, Right: right}
}

func main() {
    //       5
    //      / \
    //     3   8
    //    / \
    //   1   4
    tree := NewNode(5,
        NewNode(3,
            NewLeaf(1),
            NewLeaf(4),
        ),
        NewLeaf(8),
    )
    fmt.Println(tree.Value)             // 5
    fmt.Println(tree.Left.Value)        // 3
    fmt.Println(tree.Left.Left.Value)   // 1
}
```

### 2.3 Tipo generico con constraint especifico

```go
import "cmp"

// OrderedSet requiere que T sea comparable Y ordered
type OrderedSet[T cmp.Ordered] struct {
    items []T
}

func NewOrderedSet[T cmp.Ordered](items ...T) *OrderedSet[T] {
    s := &OrderedSet[T]{}
    for _, item := range items {
        s.Add(item)
    }
    return s
}

func (s *OrderedSet[T]) Add(item T) {
    // Insercion ordenada sin duplicados
    pos, found := slices.BinarySearch(s.items, item)
    if found {
        return // ya existe
    }
    s.items = slices.Insert(s.items, pos, item)
}

func (s *OrderedSet[T]) Contains(item T) bool {
    _, found := slices.BinarySearch(s.items, item)
    return found
}

func (s *OrderedSet[T]) Items() []T {
    return slices.Clone(s.items)
}

func main() {
    s := NewOrderedSet(5, 3, 8, 1, 3, 5) // duplicados ignorados
    fmt.Println(s.Items())                 // [1 3 5 8] — siempre ordenado

    s.Add(4)
    fmt.Println(s.Items())                 // [1 3 4 5 8]
    fmt.Println(s.Contains(4))             // true
    fmt.Println(s.Contains(7))             // false

    // Funciona con strings
    words := NewOrderedSet("go", "rust", "zig", "go")
    fmt.Println(words.Items()) // [go rust zig]

    // NO compila con tipos que no son Ordered:
    // NewOrderedSet([]int{1}, []int{2}) // error: []int does not satisfy cmp.Ordered
}
```

---

## 3. Metodos en tipos genericos

### 3.1 Reglas basicas de metodos

```go
type Box[T any] struct {
    value T
}

// El receiver repite el type parameter SIN constraint
// El constraint ya esta en la definicion del tipo
func (b Box[T]) Get() T {
    return b.value
}

func (b *Box[T]) Set(v T) {
    b.value = v
}

// Puedes usar T en parametros y retornos
func (b Box[T]) Map(fn func(T) T) Box[T] {
    return Box[T]{value: fn(b.value)}
}

// Puedes retornar otros tipos que usen T
func (b Box[T]) Unwrap() (T, bool) {
    // En Go no hay forma de saber si T es zero value "intencionalmente"
    // Retornamos siempre true aqui como ejemplo
    return b.value, true
}

func main() {
    b := Box[string]{value: "hello"}
    fmt.Println(b.Get())                                    // "hello"

    b2 := b.Map(func(s string) string { return s + " world" })
    fmt.Println(b2.Get())                                   // "hello world"

    b3 := Box[int]{value: 42}
    b3.Set(100)
    fmt.Println(b3.Get())                                   // 100
}
```

### 3.2 La limitacion: metodos NO pueden tener type parameters propios

```go
type Container[T any] struct {
    items []T
}

// ESTO NO COMPILA:
// func (c Container[T]) Map[U any](fn func(T) U) Container[U] {
//     result := make([]U, len(c.items))
//     for i, item := range c.items {
//         result[i] = fn(item)
//     }
//     return Container[U]{items: result}
// }
// Error: method must have no type parameters

// SOLUCION: funcion top-level
func MapContainer[T any, U any](c Container[T], fn func(T) U) Container[U] {
    result := make([]U, len(c.items))
    for i, item := range c.items {
        result[i] = fn(item)
    }
    return Container[U]{items: result}
}

func main() {
    c := Container[int]{items: []int{1, 2, 3}}

    // No puedes hacer: c.Map(func(n int) string { ... })
    // Debes hacer:
    result := MapContainer(c, func(n int) string {
        return fmt.Sprintf("#%d", n)
    })
    fmt.Println(result.items) // [#1 #2 #3]
}
```

### 3.3 Por que esta limitacion?

```
┌──────────────────────────────────────────────────────────────────────────────┐
│         POR QUE LOS METODOS NO PUEDEN TENER TYPE PARAMETERS               │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  El problema es la interaccion con INTERFACES.                              │
│                                                                              │
│  Si metodos pudieran tener type params, las interfaces necesitarian        │
│  type params tambien — y la implementacion implica que cualquier           │
│  interface value necesitaria un numero INFINITO de vtable entries          │
│  (una por cada posible instanciacion del type param del metodo).           │
│                                                                              │
│  Ejemplo hipotetico (NO compila):                                          │
│                                                                              │
│  type Mapper interface {                                                    │
│      Map[U any](func(T) U) Container[U]  // infinitos U posibles          │
│  }                                                                          │
│                                                                              │
│  var m Mapper = myContainer                                                │
│  m.Map[int](fn1)     // necesita vtable entry para int                    │
│  m.Map[string](fn2)  // necesita vtable entry para string                 │
│  m.Map[MyType](fn3)  // necesita vtable entry para MyType                 │
│  // ... infinitas combinaciones                                            │
│                                                                              │
│  Go resuelve esto con una regla simple:                                    │
│  "Los metodos usan los type params del receptor, nada mas"                │
│  Esto mantiene las interfaces y el dispatch virtual simples.               │
│                                                                              │
│  RUST resuelve esto de otra forma:                                         │
│  impl<T> Container<T> {                                                    │
│      fn map<U>(&self, f: impl Fn(&T) -> U) -> Container<U> { ... }       │
│  }                                                                          │
│  // Funciona porque Rust monomorphiza todo — no necesita vtable           │
│  // Pero: dyn Trait (trait objects) tienen la misma limitacion que Go     │
│  // No puedes tener dyn Mapper donde Mapper tiene metodos genericos       │
└──────────────────────────────────────────────────────────────────────────────┘
```

### 3.4 Workaround: metodos que operan en el mismo tipo

```go
// Los metodos SI pueden operar cuando el resultado es del mismo tipo T
type List[T any] struct {
    items []T
}

// OK: retorna List[T] (mismo T)
func (l List[T]) Filter(pred func(T) bool) List[T] {
    var result []T
    for _, item := range l.items {
        if pred(item) {
            result = append(result, item)
        }
    }
    return List[T]{items: result}
}

// OK: opera sobre T
func (l List[T]) ForEach(fn func(T)) {
    for _, item := range l.items {
        fn(item)
    }
}

// OK: retorna otro tipo pero SIN type params nuevos
func (l List[T]) Len() int {
    return len(l.items)
}

// OK: retorna T
func (l List[T]) First() (T, bool) {
    if len(l.items) == 0 {
        var zero T
        return zero, false
    }
    return l.items[0], true
}

// El chaining funciona para operaciones T → T
func main() {
    l := List[int]{items: []int{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}}

    // Chainable — todo opera en el mismo tipo
    result := l.
        Filter(func(n int) bool { return n%2 == 0 }).  // pares
        Filter(func(n int) bool { return n > 4 })       // mayores que 4

    result.ForEach(func(n int) {
        fmt.Println(n) // 6, 8, 10
    })
}
```

### 3.5 Receiver: pointer vs value

```go
type Counter[T comparable] struct {
    counts map[T]int
}

func NewCounter[T comparable]() *Counter[T] {
    return &Counter[T]{counts: make(map[T]int)}
}

// Pointer receiver — modifica el estado
func (c *Counter[T]) Add(item T) {
    c.counts[item]++
}

func (c *Counter[T]) Remove(item T) {
    if c.counts[item] > 1 {
        c.counts[item]--
    } else {
        delete(c.counts, item)
    }
}

// Value receiver — solo lee (pero copia el map header, no el contenido)
// Para maps y slices, value receiver aun permite modificacion accidental
// Usa pointer receiver para consistencia
func (c *Counter[T]) Count(item T) int {
    return c.counts[item]
}

func (c *Counter[T]) Total() int {
    total := 0
    for _, count := range c.counts {
        total += count
    }
    return total
}

func (c *Counter[T]) MostCommon(n int) []Pair[T, int] {
    pairs := make([]Pair[T, int], 0, len(c.counts))
    for item, count := range c.counts {
        pairs = append(pairs, Pair[T, int]{First: item, Second: count})
    }
    slices.SortFunc(pairs, func(a, b Pair[T, int]) int {
        return cmp.Compare(b.Second, a.Second) // descendente por count
    })
    if n > len(pairs) {
        n = len(pairs)
    }
    return pairs[:n]
}

type Pair[A, B any] struct {
    First  A
    Second B
}

func main() {
    c := NewCounter[string]()
    for _, word := range []string{"go", "rust", "go", "zig", "go", "rust"} {
        c.Add(word)
    }
    fmt.Println(c.Count("go"))    // 3
    fmt.Println(c.Total())        // 6
    fmt.Println(c.MostCommon(2))  // [{go 3} {rust 2}]
}
```

---

## 4. Interfaces genericas

### 4.1 Interfaces con type parameters

```go
// Las interfaces tambien pueden tener type parameters
type Repository[T any] interface {
    Get(id string) (T, error)
    List() ([]T, error)
    Create(item T) error
    Update(id string, item T) error
    Delete(id string) error
}

// Implementacion para cualquier tipo
type MemoryRepo[T any] struct {
    mu    sync.RWMutex
    items map[string]T
}

func NewMemoryRepo[T any]() *MemoryRepo[T] {
    return &MemoryRepo[T]{items: make(map[string]T)}
}

func (r *MemoryRepo[T]) Get(id string) (T, error) {
    r.mu.RLock()
    defer r.mu.RUnlock()
    item, ok := r.items[id]
    if !ok {
        var zero T
        return zero, fmt.Errorf("not found: %s", id)
    }
    return item, nil
}

func (r *MemoryRepo[T]) List() ([]T, error) {
    r.mu.RLock()
    defer r.mu.RUnlock()
    items := make([]T, 0, len(r.items))
    for _, item := range r.items {
        items = append(items, item)
    }
    return items, nil
}

func (r *MemoryRepo[T]) Create(item T) error {
    // Necesitamos un ID — el tipo T no lo tiene
    // Esto muestra una limitacion: necesitamos mas informacion sobre T
    // Ver seccion sobre constraints con metodos
    return nil
}

func (r *MemoryRepo[T]) Update(id string, item T) error {
    r.mu.Lock()
    defer r.mu.Unlock()
    if _, ok := r.items[id]; !ok {
        return fmt.Errorf("not found: %s", id)
    }
    r.items[id] = item
    return nil
}

func (r *MemoryRepo[T]) Delete(id string) error {
    r.mu.Lock()
    defer r.mu.Unlock()
    if _, ok := r.items[id]; !ok {
        return fmt.Errorf("not found: %s", id)
    }
    delete(r.items, id)
    return nil
}

// Verificar que MemoryRepo implementa Repository
var _ Repository[any] = (*MemoryRepo[any])(nil)
```

### 4.2 Interface generica con constraint en T

```go
// Interface que requiere que T tenga cierta capacidad
type Identifiable interface {
    ID() string
}

type CRUDStore[T Identifiable] interface {
    Get(id string) (T, error)
    List() ([]T, error)
    Save(item T) error    // usa item.ID() para determinar create vs update
    Delete(id string) error
}

// Implementacion
type MemStore[T Identifiable] struct {
    mu    sync.RWMutex
    items map[string]T
}

func NewMemStore[T Identifiable]() *MemStore[T] {
    return &MemStore[T]{items: make(map[string]T)}
}

func (s *MemStore[T]) Save(item T) error {
    s.mu.Lock()
    defer s.mu.Unlock()
    s.items[item.ID()] = item // T.ID() esta garantizado por el constraint
    return nil
}

func (s *MemStore[T]) Get(id string) (T, error) {
    s.mu.RLock()
    defer s.mu.RUnlock()
    item, ok := s.items[id]
    if !ok {
        var zero T
        return zero, fmt.Errorf("not found: %s", id)
    }
    return item, nil
}

func (s *MemStore[T]) List() ([]T, error) {
    s.mu.RLock()
    defer s.mu.RUnlock()
    result := make([]T, 0, len(s.items))
    for _, item := range s.items {
        result = append(result, item)
    }
    return result, nil
}

func (s *MemStore[T]) Delete(id string) error {
    s.mu.Lock()
    defer s.mu.Unlock()
    delete(s.items, id)
    return nil
}

// Tipo concreto que implementa Identifiable
type User struct {
    UserID string
    Name   string
    Email  string
}

func (u User) ID() string { return u.UserID }

func main() {
    store := NewMemStore[User]()

    store.Save(User{UserID: "1", Name: "Alice", Email: "alice@example.com"})
    store.Save(User{UserID: "2", Name: "Bob", Email: "bob@example.com"})

    user, _ := store.Get("1")
    fmt.Printf("%+v\n", user) // {UserID:1 Name:Alice Email:alice@example.com}

    users, _ := store.List()
    for _, u := range users {
        fmt.Printf("  %s: %s\n", u.Name, u.Email)
    }
}
```

### 4.3 Interfaces genericas vs interfaces regulares

```go
// INTERFACE REGULAR — polimorfismo en runtime
type Writer interface {
    Write([]byte) (int, error)
}
// Cualquier tipo que tenga Write es un Writer
// Lo descubres en runtime, no necesitas declarar nada

// INTERFACE GENERICA — polimorfismo parametrizado
type Processor[T any] interface {
    Process(input T) (T, error)
}
// El tipo T se fija en compilacion
// Processor[int] y Processor[string] son interfaces DIFERENTES

// Consecuencia: no puedes mezclar processors de diferentes tipos
// var processors []Processor[???]  // que tipo pones aqui?

// Si necesitas heterogeneidad, usa interface regular:
type AnyProcessor interface {
    Process(input any) (any, error)  // pierde type safety
}
```

---

## 5. Type aliases y tipos derivados con genericos

### 5.1 Type alias generico (Go 1.24+)

```go
// ANTES de Go 1.24: no podia hacer alias de tipos genericos
// Go 1.24 agrega soporte para generic type aliases

// Type alias — el MISMO tipo con otro nombre
type IntSlice = []int        // alias no generico (siempre funciono)

// Go 1.24+: alias generico
type List[T any] = []T       // List[int] es exactamente []int
type Map[K comparable, V any] = map[K]V  // Map[string, int] es map[string]int]

// Son IDENTICOS al tipo original — no son tipos nuevos
var l List[int] = []int{1, 2, 3}
var s []int = l  // OK: son el mismo tipo

// Antes de 1.24, usabas type definitions (tipo NUEVO):
type MyList[T any] struct {
    items []T
}
// MyList[int] NO es []int — es un tipo diferente
```

### 5.2 Tipo definido basado en tipo generico

```go
// Type definition: crea un NUEVO tipo
type StringStack struct {
    items []string
}
// Esto es independiente de cualquier tipo generico

// Puedes "fijar" un type parameter creando un tipo concreto:
type Stack[T any] struct {
    items []T
}

// Instanciacion especifica (no es alias, es uso del tipo)
type IntStack = Stack[int]       // alias: IntStack ES Stack[int]

// O como tipo nuevo (pierde los metodos de Stack):
type IntStack2 Stack[int]        // tipo nuevo basado en Stack[int]
// IntStack2 NO tiene los metodos de Stack[int]
// Necesitarias redefinirlos

// Recomendacion: usa alias (=) si quieres mantener los metodos
// Usa tipo nuevo (sin =) si quieres un tipo limpio
```

### 5.3 Embedding de tipos genericos

```go
// Puedes embeber tipos genericos en structs
type Base[T any] struct {
    Data T
}

func (b Base[T]) GetData() T {
    return b.Data
}

// Embeber en struct no generico (fijando T)
type IntContainer struct {
    Base[int]         // embeber con T=int
    Label string
}

func main() {
    ic := IntContainer{
        Base:  Base[int]{Data: 42},
        Label: "answer",
    }
    fmt.Println(ic.GetData()) // 42 — metodo promovido de Base[int]
    fmt.Println(ic.Label)     // "answer"
}

// Embeber en struct generico (propagando T)
type LabeledBox[T any] struct {
    Base[T]           // propaga T
    Label string
}

func main() {
    box := LabeledBox[string]{
        Base:  Base[string]{Data: "hello"},
        Label: "greeting",
    }
    fmt.Println(box.GetData()) // "hello" — metodo promovido
    fmt.Println(box.Label)     // "greeting"
}
```

---

## 6. Patrones de diseno con tipos genericos

### 6.1 Optional (Maybe/Option)

```go
// Go no tiene Option<T> como Rust, pero podemos construirlo
// NOTA: esto es educativo — Go idiomatico usa (T, bool) o (T, error)

type Optional[T any] struct {
    value T
    valid bool
}

func Some[T any](value T) Optional[T] {
    return Optional[T]{value: value, valid: true}
}

func None[T any]() Optional[T] {
    return Optional[T]{}
}

func (o Optional[T]) IsPresent() bool {
    return o.valid
}

func (o Optional[T]) Get() (T, bool) {
    return o.value, o.valid
}

func (o Optional[T]) OrElse(defaultVal T) T {
    if o.valid {
        return o.value
    }
    return defaultVal
}

func (o Optional[T]) OrElseGet(fn func() T) T {
    if o.valid {
        return o.value
    }
    return fn()
}

// Map necesita ser funcion top-level (limitacion de method type params)
func MapOptional[T any, U any](o Optional[T], fn func(T) U) Optional[U] {
    if !o.valid {
        return None[U]()
    }
    return Some(fn(o.value))
}

// FlatMap para encadenar operaciones que pueden fallar
func FlatMapOptional[T any, U any](o Optional[T], fn func(T) Optional[U]) Optional[U] {
    if !o.valid {
        return None[U]()
    }
    return fn(o.value)
}

func main() {
    name := Some("Alice")
    empty := None[string]()

    fmt.Println(name.OrElse("unknown"))  // "Alice"
    fmt.Println(empty.OrElse("unknown")) // "unknown"

    upper := MapOptional(name, strings.ToUpper)
    fmt.Println(upper.OrElse(""))        // "ALICE"

    // Encadenar con FlatMap
    findUser := func(id int) Optional[string] {
        users := map[int]string{1: "Alice", 2: "Bob"}
        if name, ok := users[id]; ok {
            return Some(name)
        }
        return None[string]()
    }

    user := findUser(1)
    fmt.Println(user.OrElse("not found")) // "Alice"
    user = findUser(999)
    fmt.Println(user.OrElse("not found")) // "not found"
}
```

### 6.2 Result (Either)

```go
// Result encapsula exito o error con tipo seguro
type Result[T any] struct {
    value T
    err   error
}

func Ok[T any](value T) Result[T] {
    return Result[T]{value: value}
}

func Fail[T any](err error) Result[T] {
    return Result[T]{err: err}
}

func (r Result[T]) IsOk() bool {
    return r.err == nil
}

func (r Result[T]) IsErr() bool {
    return r.err != nil
}

func (r Result[T]) Unwrap() (T, error) {
    return r.value, r.err
}

func (r Result[T]) UnwrapOr(defaultVal T) T {
    if r.err != nil {
        return defaultVal
    }
    return r.value
}

func (r Result[T]) MustUnwrap() T {
    if r.err != nil {
        panic(r.err)
    }
    return r.value
}

// MapResult — funcion top-level por la limitacion de method type params
func MapResult[T any, U any](r Result[T], fn func(T) U) Result[U] {
    if r.err != nil {
        return Fail[U](r.err)
    }
    return Ok(fn(r.value))
}

// FlatMapResult — para encadenar operaciones que retornan Result
func FlatMapResult[T any, U any](r Result[T], fn func(T) Result[U]) Result[U] {
    if r.err != nil {
        return Fail[U](r.err)
    }
    return fn(r.value)
}

// Collect — convierte []Result[T] en Result[[]T]
// Si algun resultado es error, retorna el primer error
func Collect[T any](results []Result[T]) Result[[]T] {
    values := make([]T, len(results))
    for i, r := range results {
        if r.err != nil {
            return Fail[[]T](r.err)
        }
        values[i] = r.value
    }
    return Ok(values)
}

func main() {
    divide := func(a, b float64) Result[float64] {
        if b == 0 {
            return Fail[float64](fmt.Errorf("division by zero"))
        }
        return Ok(a / b)
    }

    r1 := divide(10, 3)
    fmt.Println(r1.UnwrapOr(0)) // 3.333...

    r2 := divide(10, 0)
    fmt.Println(r2.UnwrapOr(0)) // 0

    // Encadenar
    result := FlatMapResult(
        divide(100, 4),
        func(v float64) Result[float64] {
            return divide(v, 5)
        },
    )
    fmt.Println(result.UnwrapOr(0)) // 5

    // Collect
    results := []Result[int]{Ok(1), Ok(2), Ok(3)}
    all := Collect(results)
    fmt.Println(all.UnwrapOr(nil)) // [1 2 3]

    results2 := []Result[int]{Ok(1), Fail[int](fmt.Errorf("boom")), Ok(3)}
    all2 := Collect(results2)
    fmt.Println(all2.IsErr()) // true
}
```

**Nota importante**: `Optional` y `Result` son patrones educativos. Go idiomatico usa:
- `(T, bool)` en vez de `Optional[T]`
- `(T, error)` en vez de `Result[T]`

Pero entender estos patrones genericos prepara para Rust y para librerias Go que los implementan.

### 6.3 Pool generico

```go
import "sync"

// Pool generico — reutiliza objetos costosos de crear
type Pool[T any] struct {
    pool sync.Pool
}

func NewPool[T any](factory func() T) *Pool[T] {
    return &Pool[T]{
        pool: sync.Pool{
            New: func() any {
                return factory()
            },
        },
    }
}

func (p *Pool[T]) Get() T {
    return p.pool.Get().(T) // type assertion segura (siempre es T)
}

func (p *Pool[T]) Put(item T) {
    p.pool.Put(item)
}

// Uso: pool de buffers
func main() {
    bufPool := NewPool(func() *bytes.Buffer {
        return bytes.NewBuffer(make([]byte, 0, 4096))
    })

    buf := bufPool.Get()
    buf.WriteString("hello world")
    fmt.Println(buf.String()) // "hello world"

    buf.Reset()
    bufPool.Put(buf) // reutilizar

    // Pool de slices pre-allocated
    slicePool := NewPool(func() []byte {
        return make([]byte, 0, 1024)
    })

    s := slicePool.Get()
    s = append(s, "data..."...)
    // ... usar s ...
    slicePool.Put(s[:0]) // devolver slice vacío pero con capacidad
}
```

### 6.4 EventBus generico

```go
import "sync"

// EventBus con type-safe events
type EventBus[T any] struct {
    mu       sync.RWMutex
    handlers []func(T)
}

func NewEventBus[T any]() *EventBus[T] {
    return &EventBus[T]{}
}

func (eb *EventBus[T]) Subscribe(handler func(T)) {
    eb.mu.Lock()
    defer eb.mu.Unlock()
    eb.handlers = append(eb.handlers, handler)
}

func (eb *EventBus[T]) Publish(event T) {
    eb.mu.RLock()
    handlers := make([]func(T), len(eb.handlers))
    copy(handlers, eb.handlers)
    eb.mu.RUnlock()

    for _, handler := range handlers {
        handler(event)
    }
}

// Diferentes buses para diferentes tipos de eventos
type UserCreated struct {
    ID   string
    Name string
}

type OrderPlaced struct {
    OrderID string
    UserID  string
    Amount  float64
}

func main() {
    userBus := NewEventBus[UserCreated]()
    orderBus := NewEventBus[OrderPlaced]()

    // Suscribirse — type safe
    userBus.Subscribe(func(e UserCreated) {
        fmt.Printf("User created: %s (%s)\n", e.Name, e.ID)
    })

    orderBus.Subscribe(func(e OrderPlaced) {
        fmt.Printf("Order %s: $%.2f for user %s\n", e.OrderID, e.Amount, e.UserID)
    })

    // Publicar — type safe
    userBus.Publish(UserCreated{ID: "u1", Name: "Alice"})
    orderBus.Publish(OrderPlaced{OrderID: "o1", UserID: "u1", Amount: 99.99})

    // Esto NO compila:
    // userBus.Publish(OrderPlaced{...})  // error: OrderPlaced != UserCreated
}
```

### 6.5 Future/Promise generico

```go
// Future representa un valor que se calculara asincronamente
type Future[T any] struct {
    ch   chan struct{} // señal de completado
    val  T
    err  error
    once sync.Once
}

func NewFuture[T any](fn func() (T, error)) *Future[T] {
    f := &Future[T]{
        ch: make(chan struct{}),
    }
    go func() {
        f.val, f.err = fn()
        close(f.ch) // señalar completado
    }()
    return f
}

// Await bloquea hasta que el resultado este listo
func (f *Future[T]) Await() (T, error) {
    <-f.ch
    return f.val, f.err
}

// AwaitTimeout espera con timeout
func (f *Future[T]) AwaitTimeout(d time.Duration) (T, error) {
    select {
    case <-f.ch:
        return f.val, f.err
    case <-time.After(d):
        var zero T
        return zero, fmt.Errorf("timeout after %v", d)
    }
}

// AwaitAll espera por multiples futures del mismo tipo
func AwaitAll[T any](futures ...*Future[T]) ([]T, error) {
    results := make([]T, len(futures))
    for i, f := range futures {
        val, err := f.Await()
        if err != nil {
            return nil, fmt.Errorf("future %d failed: %w", i, err)
        }
        results[i] = val
    }
    return results, nil
}

// MapFuture transforma el resultado de un future
func MapFuture[T any, U any](f *Future[T], fn func(T) U) *Future[U] {
    return NewFuture(func() (U, error) {
        val, err := f.Await()
        if err != nil {
            var zero U
            return zero, err
        }
        return fn(val), nil
    })
}

func main() {
    // Lanzar computaciones concurrentes
    f1 := NewFuture(func() (int, error) {
        time.Sleep(100 * time.Millisecond)
        return 42, nil
    })

    f2 := NewFuture(func() (int, error) {
        time.Sleep(200 * time.Millisecond)
        return 17, nil
    })

    // Esperar ambos
    results, err := AwaitAll(f1, f2)
    if err != nil {
        panic(err)
    }
    fmt.Println(results) // [42 17]

    // MapFuture
    f3 := NewFuture(func() (string, error) {
        return "hello", nil
    })
    f4 := MapFuture(f3, func(s string) int {
        return len(s)
    })
    val, _ := f4.Await()
    fmt.Println(val) // 5
}
```

### 6.6 Registry generico (patron de servicio)

```go
// Registry generico para registrar y obtener componentes por nombre
type Registry[T any] struct {
    mu       sync.RWMutex
    entries  map[string]T
    fallback T
    hasFB    bool
}

func NewRegistry[T any]() *Registry[T] {
    return &Registry[T]{entries: make(map[string]T)}
}

func (r *Registry[T]) Register(name string, item T) {
    r.mu.Lock()
    defer r.mu.Unlock()
    r.entries[name] = item
}

func (r *Registry[T]) SetFallback(item T) {
    r.mu.Lock()
    defer r.mu.Unlock()
    r.fallback = item
    r.hasFB = true
}

func (r *Registry[T]) Get(name string) (T, bool) {
    r.mu.RLock()
    defer r.mu.RUnlock()
    if item, ok := r.entries[name]; ok {
        return item, true
    }
    if r.hasFB {
        return r.fallback, true
    }
    var zero T
    return zero, false
}

func (r *Registry[T]) MustGet(name string) T {
    item, ok := r.Get(name)
    if !ok {
        panic(fmt.Sprintf("registry: %q not found", name))
    }
    return item
}

func (r *Registry[T]) Names() []string {
    r.mu.RLock()
    defer r.mu.RUnlock()
    names := make([]string, 0, len(r.entries))
    for name := range r.entries {
        names = append(names, name)
    }
    slices.Sort(names)
    return names
}

// Uso: registry de parsers, handlers, validators, etc.
type Parser func(input string) (any, error)

func main() {
    parsers := NewRegistry[Parser]()

    parsers.Register("json", func(input string) (any, error) {
        var v any
        err := json.Unmarshal([]byte(input), &v)
        return v, err
    })

    parsers.Register("csv", func(input string) (any, error) {
        // ... parse CSV
        return nil, nil
    })

    parsers.SetFallback(func(input string) (any, error) {
        return input, nil // fallback: retornar raw string
    })

    p, _ := parsers.Get("json")
    result, _ := p(`{"name":"Alice"}`)
    fmt.Println(result) // map[name:Alice]

    fmt.Println(parsers.Names()) // [csv json]
}
```

---

## 7. Tipos genericos recursivos

### 7.1 Linked List

```go
// Los tipos genericos pueden ser recursivos (con punteros)
type Node[T any] struct {
    Value T
    Next  *Node[T]
}

type LinkedList[T any] struct {
    head *Node[T]
    size int
}

func NewLinkedList[T any]() *LinkedList[T] {
    return &LinkedList[T]{}
}

// Prepend agrega al inicio (O(1))
func (l *LinkedList[T]) Prepend(value T) {
    l.head = &Node[T]{
        Value: value,
        Next:  l.head,
    }
    l.size++
}

// Head retorna el primer valor
func (l *LinkedList[T]) Head() (T, bool) {
    if l.head == nil {
        var zero T
        return zero, false
    }
    return l.head.Value, true
}

// Pop remueve y retorna el primer valor
func (l *LinkedList[T]) Pop() (T, bool) {
    if l.head == nil {
        var zero T
        return zero, false
    }
    val := l.head.Value
    l.head = l.head.Next
    l.size--
    return val, true
}

// Len retorna el tamano
func (l *LinkedList[T]) Len() int {
    return l.size
}

// ToSlice convierte a slice
func (l *LinkedList[T]) ToSlice() []T {
    result := make([]T, 0, l.size)
    current := l.head
    for current != nil {
        result = append(result, current.Value)
        current = current.Next
    }
    return result
}

// ForEach recorre la lista
func (l *LinkedList[T]) ForEach(fn func(T)) {
    current := l.head
    for current != nil {
        fn(current.Value)
        current = current.Next
    }
}

// Reverse invierte la lista (in-place)
func (l *LinkedList[T]) Reverse() {
    var prev *Node[T]
    current := l.head
    for current != nil {
        next := current.Next
        current.Next = prev
        prev = current
        current = next
    }
    l.head = prev
}

func main() {
    list := NewLinkedList[int]()
    list.Prepend(3)
    list.Prepend(2)
    list.Prepend(1)

    fmt.Println(list.ToSlice()) // [1 2 3]
    fmt.Println(list.Len())     // 3

    list.Reverse()
    fmt.Println(list.ToSlice()) // [3 2 1]

    val, _ := list.Pop()
    fmt.Println(val)            // 3
    fmt.Println(list.ToSlice()) // [2 1]
}
```

### 7.2 Binary Search Tree

```go
type BST[T cmp.Ordered] struct {
    root *bstNode[T]
    size int
}

type bstNode[T cmp.Ordered] struct {
    Value T
    Left  *bstNode[T]
    Right *bstNode[T]
}

func NewBST[T cmp.Ordered]() *BST[T] {
    return &BST[T]{}
}

func (t *BST[T]) Insert(value T) {
    t.root = t.insert(t.root, value)
}

func (t *BST[T]) insert(node *bstNode[T], value T) *bstNode[T] {
    if node == nil {
        t.size++
        return &bstNode[T]{Value: value}
    }
    if value < node.Value {
        node.Left = t.insert(node.Left, value)
    } else if value > node.Value {
        node.Right = t.insert(node.Right, value)
    }
    // value == node.Value: no insertar duplicados
    return node
}

func (t *BST[T]) Contains(value T) bool {
    return t.contains(t.root, value)
}

func (t *BST[T]) contains(node *bstNode[T], value T) bool {
    if node == nil {
        return false
    }
    if value < node.Value {
        return t.contains(node.Left, value)
    }
    if value > node.Value {
        return t.contains(node.Right, value)
    }
    return true // value == node.Value
}

// InOrder retorna elementos ordenados
func (t *BST[T]) InOrder() []T {
    var result []T
    t.inOrder(t.root, &result)
    return result
}

func (t *BST[T]) inOrder(node *bstNode[T], result *[]T) {
    if node == nil {
        return
    }
    t.inOrder(node.Left, result)
    *result = append(*result, node.Value)
    t.inOrder(node.Right, result)
}

// Min retorna el valor minimo
func (t *BST[T]) Min() (T, bool) {
    if t.root == nil {
        var zero T
        return zero, false
    }
    node := t.root
    for node.Left != nil {
        node = node.Left
    }
    return node.Value, true
}

// Max retorna el valor maximo
func (t *BST[T]) Max() (T, bool) {
    if t.root == nil {
        var zero T
        return zero, false
    }
    node := t.root
    for node.Right != nil {
        node = node.Right
    }
    return node.Value, true
}

func (t *BST[T]) Len() int {
    return t.size
}

func main() {
    tree := NewBST[int]()
    for _, v := range []int{5, 3, 7, 1, 4, 6, 8, 2} {
        tree.Insert(v)
    }

    fmt.Println(tree.InOrder())    // [1 2 3 4 5 6 7 8]
    fmt.Println(tree.Contains(4))  // true
    fmt.Println(tree.Contains(9))  // false

    min, _ := tree.Min()
    max, _ := tree.Max()
    fmt.Printf("Min: %d, Max: %d, Size: %d\n", min, max, tree.Len())
    // Min: 1, Max: 8, Size: 8

    // Funciona con strings tambien
    words := NewBST[string]()
    for _, w := range []string{"go", "rust", "zig", "c", "python"} {
        words.Insert(w)
    }
    fmt.Println(words.InOrder()) // [c go python rust zig]
}
```

### 7.3 Trie generico

```go
// Trie para mapear claves de tipo slice a valores
type Trie[K comparable, V any] struct {
    children map[K]*Trie[K, V]
    value    V
    hasValue bool
}

func NewTrie[K comparable, V any]() *Trie[K, V] {
    return &Trie[K, V]{children: make(map[K]*Trie[K, V])}
}

func (t *Trie[K, V]) Insert(key []K, value V) {
    node := t
    for _, k := range key {
        child, ok := node.children[k]
        if !ok {
            child = NewTrie[K, V]()
            node.children[k] = child
        }
        node = child
    }
    node.value = value
    node.hasValue = true
}

func (t *Trie[K, V]) Get(key []K) (V, bool) {
    node := t
    for _, k := range key {
        child, ok := node.children[k]
        if !ok {
            var zero V
            return zero, false
        }
        node = child
    }
    return node.value, node.hasValue
}

func main() {
    // Trie clasico con runes
    t := NewTrie[rune, string]()
    t.Insert([]rune("hello"), "mundo")
    t.Insert([]rune("help"), "ayuda")
    t.Insert([]rune("he"), "el")

    val, ok := t.Get([]rune("hello"))
    fmt.Println(val, ok) // "mundo" true

    val, ok = t.Get([]rune("hel"))
    fmt.Println(val, ok) // "" false (no hay valor en "hel")

    // Trie con ints (como un path router)
    router := NewTrie[int, string]()
    router.Insert([]int{1, 2, 3}, "ruta-123")
    router.Insert([]int{1, 2, 4}, "ruta-124")

    route, _ := router.Get([]int{1, 2, 3})
    fmt.Println(route) // "ruta-123"
}
```

---

## 8. Tipos genericos con sync (thread-safe)

### 8.1 SafeMap generico

```go
import "sync"

// SafeMap — map thread-safe con genericos
type SafeMap[K comparable, V any] struct {
    mu sync.RWMutex
    m  map[K]V
}

func NewSafeMap[K comparable, V any]() *SafeMap[K, V] {
    return &SafeMap[K, V]{m: make(map[K]V)}
}

func (sm *SafeMap[K, V]) Set(key K, value V) {
    sm.mu.Lock()
    defer sm.mu.Unlock()
    sm.m[key] = value
}

func (sm *SafeMap[K, V]) Get(key K) (V, bool) {
    sm.mu.RLock()
    defer sm.mu.RUnlock()
    v, ok := sm.m[key]
    return v, ok
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

// GetOrSet retorna el valor existente o inserta el default
func (sm *SafeMap[K, V]) GetOrSet(key K, defaultVal V) V {
    sm.mu.Lock()
    defer sm.mu.Unlock()
    if v, ok := sm.m[key]; ok {
        return v
    }
    sm.m[key] = defaultVal
    return defaultVal
}

// GetOrCompute retorna el valor existente o lo computa y lo inserta
func (sm *SafeMap[K, V]) GetOrCompute(key K, compute func() V) V {
    // Primero intentar con read lock (fast path)
    sm.mu.RLock()
    if v, ok := sm.m[key]; ok {
        sm.mu.RUnlock()
        return v
    }
    sm.mu.RUnlock()

    // Slow path con write lock
    sm.mu.Lock()
    defer sm.mu.Unlock()
    // Double-check despues de obtener write lock
    if v, ok := sm.m[key]; ok {
        return v
    }
    v := compute()
    sm.m[key] = v
    return v
}

// Range itera sobre todos los elementos (copia las keys para evitar lock prolongado)
func (sm *SafeMap[K, V]) Range(fn func(K, V) bool) {
    sm.mu.RLock()
    // Copiar keys para no mantener lock durante iteracion
    keys := make([]K, 0, len(sm.m))
    for k := range sm.m {
        keys = append(keys, k)
    }
    sm.mu.RUnlock()

    for _, k := range keys {
        sm.mu.RLock()
        v, ok := sm.m[k]
        sm.mu.RUnlock()
        if ok {
            if !fn(k, v) {
                return
            }
        }
    }
}

func main() {
    cache := NewSafeMap[string, int]()

    // Concurrente
    var wg sync.WaitGroup
    for i := 0; i < 100; i++ {
        wg.Add(1)
        go func(n int) {
            defer wg.Done()
            key := fmt.Sprintf("key-%d", n%10)
            cache.Set(key, n)
        }(i)
    }
    wg.Wait()

    fmt.Println(cache.Len()) // 10 (keys unicas 0-9)

    // GetOrCompute (lazy initialization)
    expensive := cache.GetOrCompute("computed", func() int {
        time.Sleep(100 * time.Millisecond) // simular calculo costoso
        return 42
    })
    fmt.Println(expensive) // 42
}
```

### 8.2 Channel wrapper generico

```go
// TypedChan — wrapper sobre channels con operaciones comunes
type TypedChan[T any] struct {
    ch chan T
}

func NewTypedChan[T any](capacity int) TypedChan[T] {
    return TypedChan[T]{ch: make(chan T, capacity)}
}

func (tc TypedChan[T]) Send(val T) {
    tc.ch <- val
}

func (tc TypedChan[T]) Receive() T {
    return <-tc.ch
}

func (tc TypedChan[T]) TryReceive() (T, bool) {
    select {
    case val := <-tc.ch:
        return val, true
    default:
        var zero T
        return zero, false
    }
}

func (tc TypedChan[T]) Close() {
    close(tc.ch)
}

// FanOut distribuye valores de un canal a N canales
func FanOut[T any](input <-chan T, outputs ...chan<- T) {
    go func() {
        for val := range input {
            for _, out := range outputs {
                out <- val
            }
        }
        for _, out := range outputs {
            close(out)
        }
    }()
}

// FanIn combina N canales en uno
func FanIn[T any](inputs ...<-chan T) <-chan T {
    output := make(chan T)
    var wg sync.WaitGroup

    for _, input := range inputs {
        wg.Add(1)
        go func(ch <-chan T) {
            defer wg.Done()
            for val := range ch {
                output <- val
            }
        }(input)
    }

    go func() {
        wg.Wait()
        close(output)
    }()

    return output
}

// Pipeline crea un pipeline de transformacion
func Pipeline[In any, Out any](input <-chan In, transform func(In) Out) <-chan Out {
    output := make(chan Out)
    go func() {
        defer close(output)
        for val := range input {
            output <- transform(val)
        }
    }()
    return output
}

func main() {
    // FanIn: combinar 3 productores
    ch1 := make(chan int)
    ch2 := make(chan int)
    ch3 := make(chan int)

    go func() { defer close(ch1); for i := 0; i < 3; i++ { ch1 <- i } }()
    go func() { defer close(ch2); for i := 10; i < 13; i++ { ch2 <- i } }()
    go func() { defer close(ch3); for i := 100; i < 103; i++ { ch3 <- i } }()

    merged := FanIn(ch1, ch2, ch3)
    var results []int
    for val := range merged {
        results = append(results, val)
    }
    slices.Sort(results)
    fmt.Println(results) // [0 1 2 10 11 12 100 101 102]

    // Pipeline: transformaciones encadenadas
    nums := make(chan int)
    go func() {
        defer close(nums)
        for i := 1; i <= 5; i++ {
            nums <- i
        }
    }()

    doubled := Pipeline(nums, func(n int) int { return n * 2 })
    strings := Pipeline(doubled, func(n int) string { return fmt.Sprintf("#%d", n) })

    for s := range strings {
        fmt.Println(s) // #2, #4, #6, #8, #10
    }
}
```

---

## 9. Comparacion con C y Rust

### 9.1 Tabla comparativa de tipos genericos

| Aspecto | C | Go | Rust |
|---|---|---|---|
| Struct generico | Macros / void* | `type S[T any] struct{}` | `struct S<T> {}` |
| Method en generico | N/A | `func (s S[T]) M()` | `impl<T> S<T> { fn m() }` |
| Method type params | N/A | **NO** (limitacion) | Si: `fn m<U>()` en impl |
| Enum generico | N/A | **NO** (Go no tiene enums) | `enum E<T> { A(T), B }` |
| Interface generica | N/A | `type I[T any] interface{}` | trait generico: `trait I<T> {}` |
| Impl condicional | N/A | **NO** | `impl<T: Display> S<T> {}` |
| Type alias generico | N/A | Si (Go 1.24+) | `type A<T> = S<T>;` |
| Embedding | Struct embedding | Si: `type S[T] struct { Base[T] }` | No (usa composicion) |
| Recursion | Punteros | Punteros: `*Node[T]` | `Box<Node<T>>` |
| Default values | N/A | `var zero T` | `T: Default` trait |
| Type bounds | N/A | Constraints | Trait bounds |
| Multiple bounds | N/A | Interface con metodos+tipos | `T: A + B + C` |
| Lifetime params | N/A | **NO** (GC) | `struct S<'a, T> { r: &'a T }` |
| Const generics | `#define BUF(N)` | **NO** | `struct S<const N: usize>` |
| Phantom types | N/A | No (sin uso real de T = error) | `PhantomData<T>` |
| Variance | N/A | Invariante | Covariant/Contravariant |

### 9.2 Ejemplo lado a lado: Stack generico

```c
// C — void* (sin type safety)
typedef struct {
    void** data;
    size_t len;
    size_t cap;
} Stack;

Stack stack_new(void) {
    return (Stack){
        .data = malloc(sizeof(void*) * 4),
        .cap = 4,
    };
}

void stack_push(Stack* s, void* item) {
    if (s->len >= s->cap) {
        s->cap *= 2;
        s->data = realloc(s->data, sizeof(void*) * s->cap);
    }
    s->data[s->len++] = item;
}

void* stack_pop(Stack* s) {
    if (s->len == 0) return NULL; // no way to distinguish NULL item from empty
    return s->data[--s->len];
}

// Uso: casts manuales, sin verificacion
int x = 42;
stack_push(&s, &x);
int* result = (int*)stack_pop(&s);
// Nada previene: stack_push(&s, "hello"); + int* bad = (int*)stack_pop(&s);
```

```go
// Go — type-safe con genericos
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

// Uso: completamente type safe
var intStack Stack[int]
intStack.Push(42)
intStack.Push(17)
val, ok := intStack.Pop() // val es int, no any

// intStack.Push("hello") // ERROR en compilacion
```

```rust
// Rust — type-safe con metodos genericos completos
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
        self.items.pop() // retorna Option<T>, no (T, bool)
    }

    fn peek(&self) -> Option<&T> {
        self.items.last() // retorna referencia, no copia
    }

    fn len(&self) -> usize {
        self.items.len()
    }

    fn is_empty(&self) -> bool {
        self.items.is_empty()
    }

    // METHOD type params — Rust SI puede
    fn map<U>(&self, f: impl Fn(&T) -> U) -> Stack<U> {
        Stack {
            items: self.items.iter().map(f).collect(),
        }
    }
}

// Impl condicional — solo para T: Display
impl<T: std::fmt::Display> Stack<T> {
    fn print_all(&self) {
        for item in &self.items {
            println!("{item}");
        }
    }
}
// Stack<i32>.print_all() funciona, Stack<Vec<i32>>.print_all() no compila

// Uso
let mut stack: Stack<i32> = Stack::new();
stack.push(42);
stack.push(17);
let val: Option<i32> = stack.pop(); // Option, no tuple

// stack.push("hello"); // ERROR en compilacion

// Method type params en accion:
let string_stack = stack.map(|n| format!("#{n}"));
// string_stack es Stack<String>
```

### 9.3 Lo que Go no puede expresar

```rust
// 1. IMPL CONDICIONAL — metodos solo para ciertos T
impl<T: Display> Stack<T> {
    fn join(&self, sep: &str) -> String {
        self.items.iter().map(|i| i.to_string()).collect::<Vec<_>>().join(sep)
    }
}
// Go: no hay forma de agregar metodos "solo si T satisface X"
// Todos los metodos de Stack[T] estan disponibles para TODOS los T

// 2. DERIVE TRAITS — implementacion automatica
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
struct Stack<T> { items: Vec<T> }
// Go: no hay derive, debes implementar interfaces manualmente

// 3. INTO/FROM — conversiones genericas
impl<T> From<Vec<T>> for Stack<T> {
    fn from(items: Vec<T>) -> Self {
        Stack { items }
    }
}
let stack: Stack<i32> = vec![1, 2, 3].into();
// Go: no hay From/Into, las conversiones son manuales

// 4. ITERATOR — iteration generica
impl<T> IntoIterator for Stack<T> {
    type Item = T;
    type IntoIter = std::vec::IntoIter<T>;
    fn into_iter(self) -> Self::IntoIter {
        self.items.into_iter()
    }
}
for item in stack { println!("{item}"); }
// Go: el range solo funciona con built-in types y iteradores (Go 1.23+)

// 5. OPERATOR OVERLOADING
impl<T: Add<Output=T>> Add for Stack<T> {
    type Output = Stack<T>;
    fn add(mut self, other: Stack<T>) -> Stack<T> {
        self.items.extend(other.items);
        self
    }
}
let combined = stack1 + stack2;
// Go: los operadores no se pueden sobrecargar
```

---

## 10. Anti-patrones y errores comunes

### 10.1 Generico innecesario

```go
// MAL: generico donde un tipo concreto funciona
type Logger[T any] struct {
    prefix string
}

func (l Logger[T]) Log(msg T) {
    fmt.Printf("[%s] %v\n", l.prefix, msg)
}

// T no aporta nada — usas %v que acepta any
// MEJOR:
type Logger struct {
    prefix string
}

func (l Logger) Log(msg any) {
    fmt.Printf("[%s] %v\n", l.prefix, msg)
}

// REGLA: si usas fmt.Println(v) o similar con T, probablemente no necesitas genericos
```

### 10.2 Generico donde interface funciona mejor

```go
// MAL: generico para polimorfismo de comportamiento
func Process[T interface{ Handle() error }](item T) error {
    return item.Handle()
}

// MEJOR: interface regular
func Process(item interface{ Handle() error }) error {
    return item.Handle()
}

// O con interface nombrada:
type Handler interface {
    Handle() error
}

func Process(item Handler) error {
    return item.Handle()
}

// El generico solo anade valor cuando necesitas:
// 1. El tipo concreto en el retorno
// 2. Operadores del tipo (<, +, ==)
// 3. El tipo como key de map o en slice tipado
```

### 10.3 Sobre-abstraccion generica

```go
// MAL: crear frameworks genericos para problemas simples
type Processor[In any, Out any, Ctx any, Err error] struct {
    transform func(Ctx, In) (Out, Err)
    validate  func(In) Err
    enrich    func(Ctx, Out) (Out, Err)
}
// Esto es sobre-ingenieria — probablemente solo necesitas una funcion

// MEJOR: funciones simples
func processOrder(ctx context.Context, order Order) (Receipt, error) {
    if err := order.Validate(); err != nil {
        return Receipt{}, err
    }
    // ... logica directa
}
```

### 10.4 Olvidar que los zero values dependen de T

```go
// CUIDADO: el zero value de T puede no ser lo que esperas
type Cache[K comparable, V any] struct {
    data map[K]V
}

func (c *Cache[K, V]) Get(key K) V {
    return c.data[key] // retorna zero value de V si no existe
}

// Si V es int: retorna 0 (pero 0 puede ser un valor valido!)
// Si V es *User: retorna nil
// Si V es bool: retorna false

// MEJOR: siempre retornar (V, bool)
func (c *Cache[K, V]) Get(key K) (V, bool) {
    v, ok := c.data[key]
    return v, ok
}
```

### 10.5 No pensar en la experiencia del usuario de tu API

```go
// MAL: forzar al usuario a especificar tipos que podrian inferirse
type Config[DB any, Cache any, Logger any] struct {
    db     DB
    cache  Cache
    logger Logger
}

// El usuario tiene que escribir:
cfg := Config[*PostgresDB, *RedisCache, *ZapLogger]{
    db:     pgDB,
    cache:  redisCache,
    logger: zapLogger,
}

// MEJOR: usar un constructor que permita inferencia
func NewConfig[DB any, Cache any, Logger any](db DB, cache Cache, logger Logger) Config[DB, Cache, Logger] {
    return Config[DB, Cache, Logger]{db: db, cache: cache, logger: logger}
}

cfg := NewConfig(pgDB, redisCache, zapLogger) // tipos inferidos
```

---

## 11. Programa completo: base de datos en memoria generica

```go
package main

import (
    "cmp"
    "encoding/json"
    "fmt"
    "slices"
    "strings"
    "sync"
    "time"
)

// ============================================================================
// Domain: interfaces y tipos base
// ============================================================================

// Entity es la constraint para cualquier tipo almacenable
type Entity interface {
    GetID() string
}

// Timestamps agrega campos de auditoria
type Timestamps struct {
    CreatedAt time.Time `json:"created_at"`
    UpdatedAt time.Time `json:"updated_at"`
}

// ============================================================================
// Table: tabla generica con indices y queries
// ============================================================================

// Table almacena entidades de tipo T con indice primario por ID
type Table[T Entity] struct {
    mu      sync.RWMutex
    records map[string]T
    indices map[string]*Index[T]
}

// NewTable crea una tabla vacia
func NewTable[T Entity]() *Table[T] {
    return &Table[T]{
        records: make(map[string]T),
        indices: make(map[string]*Index[T]),
    }
}

// Index es un indice secundario sobre un campo
type Index[T any] struct {
    keyFn   func(T) string
    entries map[string][]string // index value → list of IDs
}

// AddIndex agrega un indice secundario
func (t *Table[T]) AddIndex(name string, keyFn func(T) string) {
    t.mu.Lock()
    defer t.mu.Unlock()
    idx := &Index[T]{
        keyFn:   keyFn,
        entries: make(map[string][]string),
    }
    // Indexar records existentes
    for _, record := range t.records {
        key := keyFn(record)
        id := record.GetID()
        idx.entries[key] = append(idx.entries[key], id)
    }
    t.indices[name] = idx
}

// Insert agrega un nuevo record
func (t *Table[T]) Insert(record T) error {
    t.mu.Lock()
    defer t.mu.Unlock()

    id := record.GetID()
    if _, exists := t.records[id]; exists {
        return fmt.Errorf("duplicate id: %s", id)
    }

    t.records[id] = record

    // Actualizar indices
    for _, idx := range t.indices {
        key := idx.keyFn(record)
        idx.entries[key] = append(idx.entries[key], id)
    }

    return nil
}

// Get retorna un record por ID
func (t *Table[T]) Get(id string) (T, bool) {
    t.mu.RLock()
    defer t.mu.RUnlock()
    record, ok := t.records[id]
    return record, ok
}

// Update actualiza un record existente
func (t *Table[T]) Update(record T) error {
    t.mu.Lock()
    defer t.mu.Unlock()

    id := record.GetID()
    old, exists := t.records[id]
    if !exists {
        return fmt.Errorf("not found: %s", id)
    }

    // Actualizar indices: remover old, agregar new
    for _, idx := range t.indices {
        oldKey := idx.keyFn(old)
        newKey := idx.keyFn(record)
        if oldKey != newKey {
            // Remover de indice viejo
            ids := idx.entries[oldKey]
            for i, existingID := range ids {
                if existingID == id {
                    idx.entries[oldKey] = slices.Delete(ids, i, i+1)
                    break
                }
            }
            if len(idx.entries[oldKey]) == 0 {
                delete(idx.entries, oldKey)
            }
            // Agregar a indice nuevo
            idx.entries[newKey] = append(idx.entries[newKey], id)
        }
    }

    t.records[id] = record
    return nil
}

// Delete elimina un record
func (t *Table[T]) Delete(id string) error {
    t.mu.Lock()
    defer t.mu.Unlock()

    record, exists := t.records[id]
    if !exists {
        return fmt.Errorf("not found: %s", id)
    }

    // Remover de indices
    for _, idx := range t.indices {
        key := idx.keyFn(record)
        ids := idx.entries[key]
        for i, existingID := range ids {
            if existingID == id {
                idx.entries[key] = slices.Delete(ids, i, i+1)
                break
            }
        }
        if len(idx.entries[key]) == 0 {
            delete(idx.entries, key)
        }
    }

    delete(t.records, id)
    return nil
}

// All retorna todos los records
func (t *Table[T]) All() []T {
    t.mu.RLock()
    defer t.mu.RUnlock()
    result := make([]T, 0, len(t.records))
    for _, record := range t.records {
        result = append(result, record)
    }
    return result
}

// Count retorna el numero de records
func (t *Table[T]) Count() int {
    t.mu.RLock()
    defer t.mu.RUnlock()
    return len(t.records)
}

// FindByIndex busca records usando un indice secundario
func (t *Table[T]) FindByIndex(indexName, value string) []T {
    t.mu.RLock()
    defer t.mu.RUnlock()

    idx, ok := t.indices[indexName]
    if !ok {
        return nil
    }

    ids, ok := idx.entries[value]
    if !ok {
        return nil
    }

    result := make([]T, 0, len(ids))
    for _, id := range ids {
        if record, ok := t.records[id]; ok {
            result = append(result, record)
        }
    }
    return result
}

// ============================================================================
// Query: builder de queries generico
// ============================================================================

// Query permite construir consultas sobre una Table
type Query[T Entity] struct {
    table   *Table[T]
    filters []func(T) bool
    sortFn  func(a, b T) int
    limit   int
    offset  int
}

// From crea un query a partir de una tabla
func From[T Entity](table *Table[T]) *Query[T] {
    return &Query[T]{table: table, limit: -1}
}

// Where agrega un filtro
func (q *Query[T]) Where(pred func(T) bool) *Query[T] {
    q.filters = append(q.filters, pred)
    return q
}

// OrderBy establece el ordenamiento
func (q *Query[T]) OrderBy(cmpFn func(a, b T) int) *Query[T] {
    q.sortFn = cmpFn
    return q
}

// Limit establece el numero maximo de resultados
func (q *Query[T]) Limit(n int) *Query[T] {
    q.limit = n
    return q
}

// Offset establece cuantos resultados saltar
func (q *Query[T]) Offset(n int) *Query[T] {
    q.offset = n
    return q
}

// Execute ejecuta el query y retorna los resultados
func (q *Query[T]) Execute() []T {
    all := q.table.All()

    // Aplicar filtros
    var result []T
    for _, record := range all {
        pass := true
        for _, filter := range q.filters {
            if !filter(record) {
                pass = false
                break
            }
        }
        if pass {
            result = append(result, record)
        }
    }

    // Ordenar
    if q.sortFn != nil {
        slices.SortFunc(result, q.sortFn)
    }

    // Offset
    if q.offset > 0 && q.offset < len(result) {
        result = result[q.offset:]
    } else if q.offset >= len(result) {
        return nil
    }

    // Limit
    if q.limit >= 0 && q.limit < len(result) {
        result = result[:q.limit]
    }

    return result
}

// Count ejecuta el query y retorna solo el conteo
func (q *Query[T]) Count() int {
    return len(q.Execute())
}

// First ejecuta el query y retorna el primer resultado
func (q *Query[T]) First() (T, bool) {
    q.limit = 1
    results := q.Execute()
    if len(results) == 0 {
        var zero T
        return zero, false
    }
    return results[0], true
}

// ============================================================================
// Funciones genericas sobre tablas (top-level por limitacion de method type params)
// ============================================================================

// MapTable transforma records de tipo T a tipo U
func MapTable[T Entity, U any](table *Table[T], fn func(T) U) []U {
    all := table.All()
    result := make([]U, len(all))
    for i, record := range all {
        result[i] = fn(record)
    }
    return result
}

// ReduceTable acumula un resultado sobre los records
func ReduceTable[T Entity, Acc any](table *Table[T], initial Acc, fn func(Acc, T) Acc) Acc {
    result := initial
    for _, record := range table.All() {
        result = fn(result, record)
    }
    return result
}

// GroupTable agrupa records por una clave
func GroupTable[T Entity, K comparable](table *Table[T], keyFn func(T) K) map[K][]T {
    groups := make(map[K][]T)
    for _, record := range table.All() {
        key := keyFn(record)
        groups[key] = append(groups[key], record)
    }
    return groups
}

// ============================================================================
// Tipos de dominio concretos
// ============================================================================

type Product struct {
    ID       string  `json:"id"`
    Name     string  `json:"name"`
    Category string  `json:"category"`
    Price    float64 `json:"price"`
    Stock    int     `json:"stock"`
    Timestamps
}

func (p Product) GetID() string { return p.ID }

type Order struct {
    ID         string    `json:"id"`
    CustomerID string    `json:"customer_id"`
    ProductID  string    `json:"product_id"`
    Quantity   int       `json:"quantity"`
    Total      float64   `json:"total"`
    Status     string    `json:"status"`
    Timestamps
}

func (o Order) GetID() string { return o.ID }

// ============================================================================
// Main
// ============================================================================

func main() {
    now := time.Now()

    // ---- Productos ----
    products := NewTable[Product]()
    products.AddIndex("category", func(p Product) string { return p.Category })

    productData := []Product{
        {ID: "p1", Name: "Laptop Pro", Category: "Electronics", Price: 1299.99, Stock: 50,
            Timestamps: Timestamps{CreatedAt: now, UpdatedAt: now}},
        {ID: "p2", Name: "Wireless Mouse", Category: "Electronics", Price: 29.99, Stock: 200,
            Timestamps: Timestamps{CreatedAt: now, UpdatedAt: now}},
        {ID: "p3", Name: "Standing Desk", Category: "Furniture", Price: 499.99, Stock: 30,
            Timestamps: Timestamps{CreatedAt: now, UpdatedAt: now}},
        {ID: "p4", Name: "Ergonomic Chair", Category: "Furniture", Price: 699.99, Stock: 25,
            Timestamps: Timestamps{CreatedAt: now, UpdatedAt: now}},
        {ID: "p5", Name: "Monitor 4K", Category: "Electronics", Price: 449.99, Stock: 75,
            Timestamps: Timestamps{CreatedAt: now, UpdatedAt: now}},
        {ID: "p6", Name: "Mechanical Keyboard", Category: "Electronics", Price: 89.99, Stock: 150,
            Timestamps: Timestamps{CreatedAt: now, UpdatedAt: now}},
        {ID: "p7", Name: "Desk Lamp", Category: "Furniture", Price: 59.99, Stock: 100,
            Timestamps: Timestamps{CreatedAt: now, UpdatedAt: now}},
        {ID: "p8", Name: "USB Hub", Category: "Electronics", Price: 19.99, Stock: 300,
            Timestamps: Timestamps{CreatedAt: now, UpdatedAt: now}},
    }

    for _, p := range productData {
        products.Insert(p)
    }

    // ---- Ordenes ----
    orders := NewTable[Order]()
    orders.AddIndex("customer_id", func(o Order) string { return o.CustomerID })
    orders.AddIndex("status", func(o Order) string { return o.Status })

    orderData := []Order{
        {ID: "o1", CustomerID: "c1", ProductID: "p1", Quantity: 1, Total: 1299.99, Status: "completed",
            Timestamps: Timestamps{CreatedAt: now, UpdatedAt: now}},
        {ID: "o2", CustomerID: "c1", ProductID: "p2", Quantity: 2, Total: 59.98, Status: "completed",
            Timestamps: Timestamps{CreatedAt: now, UpdatedAt: now}},
        {ID: "o3", CustomerID: "c2", ProductID: "p3", Quantity: 1, Total: 499.99, Status: "pending",
            Timestamps: Timestamps{CreatedAt: now, UpdatedAt: now}},
        {ID: "o4", CustomerID: "c2", ProductID: "p5", Quantity: 2, Total: 899.98, Status: "shipped",
            Timestamps: Timestamps{CreatedAt: now, UpdatedAt: now}},
        {ID: "o5", CustomerID: "c3", ProductID: "p6", Quantity: 3, Total: 269.97, Status: "completed",
            Timestamps: Timestamps{CreatedAt: now, UpdatedAt: now}},
        {ID: "o6", CustomerID: "c1", ProductID: "p4", Quantity: 1, Total: 699.99, Status: "pending",
            Timestamps: Timestamps{CreatedAt: now, UpdatedAt: now}},
    }

    for _, o := range orderData {
        orders.Insert(o)
    }

    // ========================================================
    // Queries
    // ========================================================

    fmt.Println("=== Todos los productos ===")
    fmt.Printf("Total: %d\n\n", products.Count())

    // Query: electronicos con precio < $100, ordenados por precio
    fmt.Println("=== Electronicos baratos (< $100) ===")
    cheap := From(products).
        Where(func(p Product) bool { return p.Category == "Electronics" }).
        Where(func(p Product) bool { return p.Price < 100 }).
        OrderBy(func(a, b Product) int { return cmp.Compare(a.Price, b.Price) }).
        Execute()

    for _, p := range cheap {
        fmt.Printf("  %-20s $%.2f (stock: %d)\n", p.Name, p.Price, p.Stock)
    }

    // Index lookup: productos en categoria Furniture
    fmt.Println("\n=== Furniture (via indice) ===")
    furniture := products.FindByIndex("category", "Furniture")
    for _, p := range furniture {
        fmt.Printf("  %-20s $%.2f\n", p.Name, p.Price)
    }

    // Query con paginacion
    fmt.Println("\n=== Pagina 2 de productos (3 por pagina, por precio desc) ===")
    page2 := From(products).
        OrderBy(func(a, b Product) int { return cmp.Compare(b.Price, a.Price) }).
        Offset(3).
        Limit(3).
        Execute()

    for _, p := range page2 {
        fmt.Printf("  %-20s $%.2f\n", p.Name, p.Price)
    }

    // MapTable: extraer solo nombres
    fmt.Println("\n=== Nombres de productos ===")
    names := MapTable(products, func(p Product) string { return p.Name })
    slices.Sort(names)
    fmt.Println(" ", strings.Join(names, ", "))

    // ReduceTable: valor total del inventario
    totalInventory := ReduceTable(products, 0.0, func(acc float64, p Product) float64 {
        return acc + p.Price*float64(p.Stock)
    })
    fmt.Printf("\n=== Valor total inventario: $%.2f ===\n", totalInventory)

    // GroupTable: productos por categoria
    fmt.Println("\n=== Resumen por categoria ===")
    byCategory := GroupTable(products, func(p Product) string { return p.Category })
    for cat, prods := range byCategory {
        total := 0.0
        for _, p := range prods {
            total += p.Price
        }
        fmt.Printf("  %-15s %d productos, promedio $%.2f\n", cat, len(prods), total/float64(len(prods)))
    }

    // Ordenes: buscar por indice
    fmt.Println("\n=== Ordenes del cliente c1 (via indice) ===")
    c1Orders := orders.FindByIndex("customer_id", "c1")
    for _, o := range c1Orders {
        fmt.Printf("  Order %s: product %s, qty %d, total $%.2f (%s)\n",
            o.ID, o.ProductID, o.Quantity, o.Total, o.Status)
    }

    // Query: ordenes pendientes ordenadas por total desc
    fmt.Println("\n=== Ordenes pendientes ===")
    pending := From(orders).
        Where(func(o Order) bool { return o.Status == "pending" }).
        OrderBy(func(a, b Order) int { return cmp.Compare(b.Total, a.Total) }).
        Execute()

    for _, o := range pending {
        fmt.Printf("  Order %s: $%.2f (customer %s)\n", o.ID, o.Total, o.CustomerID)
    }

    // Revenue por status
    fmt.Println("\n=== Revenue por status ===")
    byStatus := GroupTable(orders, func(o Order) string { return o.Status })
    for status, ords := range byStatus {
        revenue := 0.0
        for _, o := range ords {
            revenue += o.Total
        }
        fmt.Printf("  %-12s %d orders, $%.2f\n", status, len(ords), revenue)
    }

    // CRUD: actualizar stock
    fmt.Println("\n=== Update: reducir stock del Laptop ===")
    laptop, _ := products.Get("p1")
    fmt.Printf("  Antes: %s stock=%d\n", laptop.Name, laptop.Stock)
    laptop.Stock -= 5
    laptop.UpdatedAt = time.Now()
    products.Update(laptop)
    laptop, _ = products.Get("p1")
    fmt.Printf("  Despues: %s stock=%d\n", laptop.Name, laptop.Stock)

    // CRUD: eliminar producto
    fmt.Println("\n=== Delete: eliminar USB Hub ===")
    fmt.Printf("  Antes: %d productos\n", products.Count())
    products.Delete("p8")
    fmt.Printf("  Despues: %d productos\n", products.Count())

    // Verificar que indice se actualizo
    electronics := products.FindByIndex("category", "Electronics")
    fmt.Printf("  Electronics ahora: %d productos\n", len(electronics))

    // Serializar a JSON
    fmt.Println("\n=== JSON export (primer producto) ===")
    first, _ := From(products).
        OrderBy(func(a, b Product) int { return cmp.Compare(a.Name, b.Name) }).
        First()
    data, _ := json.MarshalIndent(first, "  ", "  ")
    fmt.Println(" ", string(data))
}
```

```
=== Todos los productos ===
Total: 8

=== Electronicos baratos (< $100) ===
  USB Hub               $19.99 (stock: 300)
  Wireless Mouse        $29.99 (stock: 200)
  Mechanical Keyboard   $89.99 (stock: 150)

=== Furniture (via indice) ===
  Standing Desk         $499.99
  Ergonomic Chair       $699.99
  Desk Lamp             $59.99

=== Pagina 2 de productos (3 por pagina, por precio desc) ===
  Monitor 4K            $449.99
  Mechanical Keyboard   $89.99
  Desk Lamp             $59.99

=== Nombres de productos ===
  Desk Lamp, Ergonomic Chair, Laptop Pro, Mechanical Keyboard, Monitor 4K, Standing Desk, USB Hub, Wireless Mouse

=== Valor total inventario: $162221.25 ===

=== Resumen por categoria ===
  Electronics     5 productos, promedio $377.99
  Furniture       3 productos, promedio $419.99

=== Ordenes del cliente c1 ===
  Order o1: product p1, qty 1, total $1299.99 (completed)
  Order o2: product p2, qty 2, total $59.98 (completed)
  Order o6: product p4, qty 1, total $699.99 (pending)

=== Ordenes pendientes ===
  Order o6: $699.99 (customer c1)
  Order o3: $499.99 (customer c2)

=== Revenue por status ===
  completed    3 orders, $1629.94
  pending      2 orders, $1199.98
  shipped      1 orders, $899.98

=== Update: reducir stock del Laptop ===
  Antes: Laptop Pro stock=50
  Despues: Laptop Pro stock=45

=== Delete: eliminar USB Hub ===
  Antes: 8 productos
  Despues: 7 productos
  Electronics ahora: 4 productos

=== JSON export (primer producto) ===
  {
    "id": "p7",
    "name": "Desk Lamp",
    "category": "Furniture",
    "price": 59.99,
    "stock": 100,
    "created_at": "2026-04-04T...",
    "updated_at": "2026-04-04T..."
  }
```

**Puntos clave del programa**:

1. **`Entity` constraint**: la interface `Entity` con `GetID() string` es la base de la tabla genérica. Cualquier struct que la implemente puede almacenarse.

2. **`Table[T Entity]`**: tabla genérica con CRUD completo, índices secundarios, y thread-safety. Un solo tipo sirve para `Product`, `Order`, `User`, o cualquier entidad.

3. **`Query[T Entity]`**: query builder genérico con Where/OrderBy/Limit/Offset/First. Demuestra que los métodos que retornan `*Query[T]` pueden encadenarse.

4. **Funciones top-level**: `MapTable`, `ReduceTable`, `GroupTable` son funciones porque necesitan type parameters adicionales que los métodos no pueden declarar.

5. **Índices secundarios**: `AddIndex` recibe una función que extrae la clave del índice. Los índices se mantienen actualizados en Insert/Update/Delete.

---

## 12. Ejercicios

### Ejercicio 1: Deque generico (double-ended queue)

Implementa un `Deque[T any]` con las operaciones:

```go
type Deque[T any] struct { ... }

func NewDeque[T any]() *Deque[T]
func (d *Deque[T]) PushFront(v T)
func (d *Deque[T]) PushBack(v T)
func (d *Deque[T]) PopFront() (T, bool)
func (d *Deque[T]) PopBack() (T, bool)
func (d *Deque[T]) PeekFront() (T, bool)
func (d *Deque[T]) PeekBack() (T, bool)
func (d *Deque[T]) Len() int
func (d *Deque[T]) IsEmpty() bool
```

Implementa internamente con un ring buffer (slice circular) que crece dinámicamente. Verifica con:
```go
d := NewDeque[int]()
d.PushBack(1); d.PushBack(2); d.PushBack(3)
d.PushFront(0)
fmt.Println(d.PopFront()) // 0, true
fmt.Println(d.PopBack())  // 3, true
```

### Ejercicio 2: PriorityQueue generica

Implementa una cola de prioridad con heap:

```go
type PriorityQueue[T any] struct { ... }

func NewPriorityQueue[T any](less func(a, b T) bool) *PriorityQueue[T]
func (pq *PriorityQueue[T]) Push(item T)
func (pq *PriorityQueue[T]) Pop() (T, bool)
func (pq *PriorityQueue[T]) Peek() (T, bool)
func (pq *PriorityQueue[T]) Len() int
```

Internamente usa un binary heap (min o max según `less`). No uses `container/heap` — implementa los métodos `siftUp` y `siftDown` directamente.

### Ejercicio 3: Observable generico (patron Observer)

Implementa un sistema de observables con type safety:

```go
type Observable[T any] struct { ... }
type Unsubscribe func()

func NewObservable[T any](initial T) *Observable[T]
func (o *Observable[T]) Get() T
func (o *Observable[T]) Set(value T)
func (o *Observable[T]) Subscribe(fn func(old, new T)) Unsubscribe
```

Requisitos:
- `Subscribe` retorna una funcion para des-suscribirse
- Los observers se notifican sincronamente cuando cambia el valor
- Thread-safe (usa `sync.RWMutex`)
- Implementa tambien `Computed[T]` que se re-calcula automaticamente cuando cambian sus dependencias

### Ejercicio 4: State Machine generica

Implementa una maquina de estados generica:

```go
type State comparable
type Event comparable

type StateMachine[S State, E Event] struct { ... }

func NewStateMachine[S State, E Event](initial S) *StateMachine[S, E]
func (sm *StateMachine[S, E]) AddTransition(from S, event E, to S, action func(S, E, S))
func (sm *StateMachine[S, E]) Send(event E) error
func (sm *StateMachine[S, E]) Current() S
func (sm *StateMachine[S, E]) CanSend(event E) bool
```

Ejemplo:
```go
type OrderState string
type OrderEvent string

const (
    Created   OrderState = "created"
    Confirmed OrderState = "confirmed"
    Shipped   OrderState = "shipped"
    Delivered OrderState = "delivered"
    Cancelled OrderState = "cancelled"
)

sm := NewStateMachine[OrderState, OrderEvent](Created)
sm.AddTransition(Created, "confirm", Confirmed, func(from, event, to) { ... })
sm.AddTransition(Created, "cancel", Cancelled, nil)
sm.AddTransition(Confirmed, "ship", Shipped, nil)
sm.AddTransition(Shipped, "deliver", Delivered, nil)

sm.Send("confirm") // Created → Confirmed
sm.Send("ship")    // Confirmed → Shipped
```

---

> **Siguiente**: C11/S01 - Genericos (Go 1.18+), T03 - Constraints — interfaces como constraints, constraints package, union types (~int | ~float64), tilde (~)
