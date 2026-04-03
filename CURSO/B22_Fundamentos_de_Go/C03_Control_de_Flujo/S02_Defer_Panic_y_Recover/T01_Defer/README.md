# defer — Limpieza Garantizada

## 1. Introduccion

`defer` es una de las features mas importantes de Go. Programa una llamada a funcion para que se ejecute **justo antes de que la funcion que la contiene retorne** — sin importar como retorne (return normal, return en un if, panic, o runtime error).

Ya has visto `defer` en accion a lo largo del curso: `defer f.Close()`, `defer wg.Done()`, `defer mu.Unlock()`. Este topico profundiza en la mecanica exacta, las trampas, y los patrones avanzados.

```
┌──────────────────────────────────────────────────────────────┐
│              defer EN GO                                      │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  func doWork() error {                                       │
│      f, err := os.Open("file.txt")                           │
│      if err != nil { return err }                            │
│      defer f.Close()  ← se ejecuta al final de doWork()      │
│                                                              │
│      // ... trabajo con f ...                                │
│      return nil                                              │
│  }  ← f.Close() se ejecuta AQUI, justo antes de retornar    │
│                                                              │
│  TRES REGLAS FUNDAMENTALES:                                  │
│                                                              │
│  1. Los argumentos se evaluan CUANDO se registra el defer    │
│     (no cuando se ejecuta)                                   │
│                                                              │
│  2. Multiples defers se ejecutan en orden LIFO               │
│     (Last In, First Out — pila)                              │
│                                                              │
│  3. defer puede leer y modificar return values nombrados     │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Sintaxis y mecanica basica

```go
// defer registra una llamada de funcion para ejecucion diferida
defer f.Close()

// Lo que sigue a defer DEBE ser una llamada a funcion
defer f.Close()           // ✓ llamada a metodo
defer fmt.Println("bye")  // ✓ llamada a funcion
defer func() { ... }()    // ✓ llamada a closure (nota los () al final)
defer recover()           // ✓ llamada a builtin

defer f.Close             // ✗ ERROR: expression in defer must be function call
defer x + 1               // ✗ ERROR: expression in defer must be function call
defer func() { ... }      // ✗ ERROR: falta () — es una funcion, no una llamada
```

### Cuando se ejecuta

```go
func example() {
    fmt.Println("1: start")
    defer fmt.Println("2: deferred")
    fmt.Println("3: middle")
    return  // o simplemente el final de la funcion
    // defer se ejecuta AQUI, justo antes del return real
}
// Output:
// 1: start
// 3: middle
// 2: deferred
```

### defer se ejecuta al salir de la FUNCION, no del bloque

```go
func example() {
    {
        defer fmt.Println("deferred")
        fmt.Println("inside block")
    }
    fmt.Println("outside block")
    // defer NO se ejecuta al salir del bloque { }
    // Se ejecuta al salir de example()
}
// Output:
// inside block
// outside block
// deferred

// Esto es diferente de Rust (Drop al final del scope)
// y de C++ (RAII al final del scope)
```

---

## 3. Regla 1: Los argumentos se evaluan inmediatamente

Los argumentos de la funcion diferida se evaluan **en el momento en que se registra el defer**, no cuando se ejecuta:

```go
func example() {
    x := 10
    defer fmt.Println("x =", x)  // x se evalua AHORA: x = 10
    x = 20
    x = 30
    fmt.Println("current x =", x)
}
// Output:
// current x = 30
// x = 10          ← NO 30, sino el valor al momento del defer
```

### Por que esta regla existe

```go
// Sin esta regla, este patron clasico seria un bug:
func readFile(path string) error {
    f, err := os.Open(path)
    if err != nil {
        return err
    }
    defer f.Close()  // f se captura AHORA — es este file handle

    // Si f cambiara despues y el defer usara el valor "actual",
    // podriamos cerrar el file handle equivocado
    return processFile(f)
}
```

### Demostracion con multiples variables

```go
func demo() {
    for i := 0; i < 3; i++ {
        defer fmt.Println("defer i =", i)  // i se evalua en cada iteracion
    }
    fmt.Println("loop done")
}
// Output:
// loop done
// defer i = 2    ← LIFO: ultimo registrado, primero ejecutado
// defer i = 1
// defer i = 0
// Nota: los valores son 0, 1, 2 (evaluados al registrar), no 3, 3, 3
```

### Excepcion: closures capturan la variable, no el valor

```go
func demoPointer() {
    x := 10
    defer func() {
        fmt.Println("closure x =", x)  // captura la VARIABLE x, no su valor
    }()
    x = 42
}
// Output:
// closure x = 42  ← la closure ve el valor actual de x
```

```go
// Comparacion directa:
func compare() {
    x := 10

    // Argumento directo: evalua x AHORA
    defer fmt.Println("arg x =", x)     // captura valor: 10

    // Closure: captura la referencia a x
    defer func() {
        fmt.Println("closure x =", x)   // captura variable: veras 42
    }()

    x = 42
}
// Output:
// closure x = 42
// arg x = 10
```

### Patron: Si necesitas el valor actual al ejecutar, usa closure

```go
func processServers(servers []string) {
    var lastProcessed string

    // ✗ Captura lastProcessed al momento del defer (vacio)
    defer fmt.Println("last processed:", lastProcessed)

    // ✓ Closure captura la referencia
    defer func() {
        fmt.Println("last processed:", lastProcessed)
    }()

    for _, s := range servers {
        lastProcessed = s
        // ... procesar ...
    }
}
```

---

## 4. Regla 2: Orden LIFO (Last In, First Out)

Cuando hay multiples defers, se ejecutan en orden inverso al que fueron registrados — como una pila (stack):

```go
func lifoDemo() {
    defer fmt.Println("1st deferred")
    defer fmt.Println("2nd deferred")
    defer fmt.Println("3rd deferred")
    fmt.Println("function body")
}
// Output:
// function body
// 3rd deferred    ← ultimo registrado, primero ejecutado
// 2nd deferred
// 1st deferred    ← primero registrado, ultimo ejecutado
```

### Por que LIFO tiene sentido

El orden LIFO refleja la semantica de adquisicion y liberacion de recursos: lo que se abre primero se cierra ultimo (como parentesis anidados):

```go
func setupInfra() error {
    db, err := openDB()
    if err != nil { return err }
    defer db.Close()         // registrado 1ro → ejecutado 3ro (ultimo)

    cache, err := openCache()
    if err != nil { return err }
    defer cache.Close()      // registrado 2do → ejecutado 2do

    mq, err := openMQ()
    if err != nil { return err }
    defer mq.Close()         // registrado 3ro → ejecutado 1ro (primero)

    return doWork(db, cache, mq)
}
// Orden de cierre: mq → cache → db
// Esto es correcto: si cache depende de db,
// cerramos cache antes de cerrar db
```

```
┌──────────────────────────────────────────────────────────────┐
│  Analogia: parentesis anidados                               │
│                                                              │
│  open(db)                                                    │
│    open(cache)                                               │
│      open(mq)                                                │
│        // trabajo                                            │
│      close(mq)      ← primero                               │
│    close(cache)     ← segundo                               │
│  close(db)          ← tercero                                │
│                                                              │
│  Igual que: ( [ { } ] )                                      │
│  No:        ( [ { ] ) }                                      │
└──────────────────────────────────────────────────────────────┘
```

### Patron SysAdmin: Mutex Lock/Unlock

```go
type SafeConfig struct {
    mu     sync.RWMutex
    values map[string]string
}

func (c *SafeConfig) Get(key string) (string, bool) {
    c.mu.RLock()
    defer c.mu.RUnlock()  // garantizado: unlock incluso si hay panic

    val, ok := c.values[key]
    return val, ok
}

func (c *SafeConfig) Set(key, value string) {
    c.mu.Lock()
    defer c.mu.Unlock()  // LIFO con otros defers? no importa, se ejecuta

    c.values[key] = value
}

// SIEMPRE usa defer para unlock
// Sin defer:
func (c *SafeConfig) SetBad(key, value string) {
    c.mu.Lock()
    if key == "" {
        c.mu.Unlock()  // facil olvidar esto en CADA return
        return
    }
    c.values[key] = value
    c.mu.Unlock()       // Y este tambien
    // Y si agregas mas returns en el futuro...
}
```

---

## 5. Regla 3: defer puede leer y modificar named return values

Esta es la regla mas sutil y poderosa. Una funcion diferida puede acceder a los **return values nombrados** de la funcion que la contiene:

```go
func readValue() (result int) {
    defer func() {
        result *= 2  // modifica el valor que se retorna
    }()

    return 21
    // Internamente:
    // 1. result = 21      (asigna el return value)
    // 2. defer: result *= 2  (modifica result a 42)
    // 3. return 42        (retorna el valor modificado)
}

fmt.Println(readValue())  // 42
```

### Como funciona internamente

```go
// Cuando Go ve:
func f() (result int) {
    defer func() { result++ }()
    return 10
}

// El compilador lo traduce conceptualmente a:
func f() (result int) {
    result = 10        // paso 1: asignar return value
    result++           // paso 2: ejecutar defers
    return             // paso 3: retornar result
}
```

### Patron critico: Agregar contexto a errores

```go
func readConfig(path string) (cfg Config, err error) {
    // defer enriquece el error con contexto
    defer func() {
        if err != nil {
            err = fmt.Errorf("readConfig(%s): %w", path, err)
        }
    }()

    data, err := os.ReadFile(path)
    if err != nil {
        return Config{}, err  // defer agrega: "readConfig(/etc/app.conf): open: permission denied"
    }

    err = json.Unmarshal(data, &cfg)
    if err != nil {
        return Config{}, err  // defer agrega: "readConfig(/etc/app.conf): invalid character..."
    }

    return cfg, nil  // err es nil → defer no hace nada
}
```

### Patron critico: Capturar error de Close()

```go
// El error de Close() normalmente se ignora:
defer f.Close()  // el error de Close se pierde

// Pero para ESCRITURA, el error de Close es importante
// (el OS puede buffear escrituras, y Close() las flushea)
func writeFile(path string, data []byte) (err error) {
    f, err := os.Create(path)
    if err != nil {
        return err
    }

    defer func() {
        closeErr := f.Close()
        if err == nil {
            err = closeErr  // solo asignar si no hubo error previo
        }
        // Si ya habia error, no queremos pisarlo con el error de Close
    }()

    _, err = f.Write(data)
    return err
}
```

### Patron: Medir duracion de funciones

```go
func timedOperation(name string) (err error) {
    start := time.Now()
    defer func() {
        duration := time.Since(start)
        if err != nil {
            log.Printf("[%s] FAILED after %s: %v", name, duration, err)
        } else {
            log.Printf("[%s] completed in %s", name, duration)
        }
    }()

    // ... operacion larga ...
    return nil
}
```

```go
// Variante elegante con funcion helper:
func measureTime(name string) func() {
    start := time.Now()
    return func() {
        log.Printf("[%s] %s", name, time.Since(start))
    }
}

func deploy() {
    defer measureTime("deploy")()  // nota: los () llaman al return de measureTime
    // ... deploy logic ...
}
// Log: [deploy] 3.241s
```

---

## 6. defer en loops — ¡Cuidado!

Este es uno de los errores mas comunes y peligrosos con defer. Recuerda: defer se ejecuta al salir de la **funcion**, no del bloque o la iteracion:

```go
// ✗ PELIGROSO — file leak masivo
func processAllFiles(paths []string) error {
    for _, path := range paths {
        f, err := os.Open(path)
        if err != nil {
            return err
        }
        defer f.Close()  // ← NO se cierra hasta que processAllFiles retorne
                         // Si paths tiene 10,000 archivos, los 10,000 estan
                         // abiertos simultaneamente

        // procesar f...
    }
    return nil
    // TODOS los f.Close() se ejecutan AQUI, al final
}
```

### El problema visualizado

```
┌──────────────────────────────────────────────────────────────┐
│  defer en loop — file descriptors abiertos                   │
│                                                              │
│  Iteracion 1: open(file1)  → defer close(file1) registrado  │
│  Iteracion 2: open(file2)  → defer close(file2) registrado  │
│  Iteracion 3: open(file3)  → defer close(file3) registrado  │
│  ...                                                         │
│  Iteracion N: open(fileN)  → defer close(fileN) registrado  │
│                                                              │
│  Files abiertos simultaneamente: N                           │
│                                                              │
│  Linux default: ulimit -n = 1024                             │
│  Si N > 1024 → "too many open files"                         │
│                                                              │
│  return nil                                                  │
│  ← AQUI se ejecutan todos: close(N), close(N-1), ..., close(1) │
│                                                              │
│  Es un resource leak TEMPORAL pero puede causar:             │
│  ├─ "too many open files" error                              │
│  ├─ Uso excesivo de memoria                                  │
│  └─ Agotamiento de file descriptors del proceso              │
└──────────────────────────────────────────────────────────────┘
```

### Solucion 1: Extraer a una funcion (idiomatica)

```go
// ✓ CORRECTO — cada llamada a processFile tiene su propio scope de defer
func processAllFiles(paths []string) error {
    for _, path := range paths {
        if err := processFile(path); err != nil {
            return err
        }
    }
    return nil
}

func processFile(path string) error {
    f, err := os.Open(path)
    if err != nil {
        return err
    }
    defer f.Close()  // se ejecuta al salir de processFile, no del loop

    // procesar f...
    return nil
}
```

### Solucion 2: Closure inmediata (inline)

```go
// ✓ CORRECTO — la closure crea un scope de funcion
func processAllFiles(paths []string) error {
    for _, path := range paths {
        err := func() error {
            f, err := os.Open(path)
            if err != nil {
                return err
            }
            defer f.Close()  // se ejecuta al salir de la closure

            // procesar f...
            return nil
        }()

        if err != nil {
            return err
        }
    }
    return nil
}
```

### Solucion 3: Close manual (sin defer)

```go
// ✓ CORRECTO — pero mas propenso a errores si agregas returns
func processAllFiles(paths []string) error {
    for _, path := range paths {
        f, err := os.Open(path)
        if err != nil {
            return err
        }

        err = processFileHandle(f)
        f.Close()  // close manual — se ejecuta en cada iteracion

        if err != nil {
            return err
        }
    }
    return nil
}
```

### El mismo problema con conexiones de red

```go
// ✗ PELIGROSO — acumula conexiones abiertas
func checkAllHosts(hosts []string) {
    for _, host := range hosts {
        conn, err := net.DialTimeout("tcp", host+":22", 2*time.Second)
        if err != nil {
            log.Printf("%s: down", host)
            continue
        }
        defer conn.Close()  // ← todas las conexiones se acumulan
        log.Printf("%s: up", host)
    }
    // AQUI se cierran todas
}

// ✓ CORRECTO — cerrar inmediatamente
func checkAllHosts(hosts []string) {
    for _, host := range hosts {
        conn, err := net.DialTimeout("tcp", host+":22", 2*time.Second)
        if err != nil {
            log.Printf("%s: down", host)
            continue
        }
        log.Printf("%s: up", host)
        conn.Close()  // cerrar en cada iteracion
    }
}
```

### Patron SysAdmin: HTTP requests en loop

```go
// ✗ PELIGROSO — los Body no se cierran hasta el final
func healthCheckAll(urls []string) map[string]bool {
    results := make(map[string]bool)
    client := &http.Client{Timeout: 5 * time.Second}

    for _, url := range urls {
        resp, err := client.Get(url)
        if err != nil {
            results[url] = false
            continue
        }
        defer resp.Body.Close()  // ← leak acumulativo
        io.Copy(io.Discard, resp.Body)
        results[url] = resp.StatusCode == 200
    }
    return results
}

// ✓ CORRECTO — funcion separada
func healthCheckAll(urls []string) map[string]bool {
    results := make(map[string]bool)
    client := &http.Client{Timeout: 5 * time.Second}

    for _, url := range urls {
        results[url] = checkURL(client, url)
    }
    return results
}

func checkURL(client *http.Client, url string) bool {
    resp, err := client.Get(url)
    if err != nil {
        return false
    }
    defer resp.Body.Close()  // correcto — se cierra al salir de checkURL
    io.Copy(io.Discard, resp.Body)
    return resp.StatusCode == 200
}
```

---

## 7. defer con metodos y receptores

Cuando usas defer con un metodo, el receptor se evalua al momento del registro (como cualquier argumento):

```go
type Logger struct {
    prefix string
}

func (l Logger) Log(msg string) {
    fmt.Printf("[%s] %s\n", l.prefix, msg)
}

func example() {
    logger := Logger{prefix: "v1"}
    defer logger.Log("done")  // logger se evalua AHORA (prefix = "v1")
    logger.prefix = "v2"
}
// Output: [v1] done   ← no [v2]
```

### Pointer receivers — comportamiento diferente

```go
type Counter struct {
    count int
}

func (c *Counter) Report() {
    fmt.Println("count:", c.count)
}

func example() {
    c := &Counter{}
    defer c.Report()  // c (puntero) se evalua ahora, pero apunta al mismo objeto
    c.count = 42
}
// Output: count: 42
// El puntero se capturo, pero el objeto al que apunta cambio
```

### Patron: Timer con puntero

```go
type Timer struct {
    name  string
    start time.Time
}

func NewTimer(name string) *Timer {
    return &Timer{
        name:  name,
        start: time.Now(),
    }
}

func (t *Timer) Stop() {
    log.Printf("[%s] %s", t.name, time.Since(t.start))
}

// Uso:
func deployService() {
    t := NewTimer("deploy")
    defer t.Stop()  // t es puntero — Stop() vera el start original

    // ... logica de deploy ...
}
```

---

## 8. defer y panic

defer se ejecuta **incluso durante un panic**. Esta es la clave de su poder para cleanup:

```go
func riskyOperation() {
    f, _ := os.Create("/tmp/test.txt")
    defer f.Close()  // se ejecuta INCLUSO si hay panic
    defer fmt.Println("cleanup complete")

    fmt.Println("about to panic")
    panic("something went wrong")
    fmt.Println("this never runs")  // unreachable
}

func main() {
    defer fmt.Println("main: deferred")

    fmt.Println("calling riskyOperation")
    riskyOperation()
    fmt.Println("this never runs")  // unreachable por el panic
}
// Output:
// calling riskyOperation
// about to panic
// cleanup complete          ← defer en riskyOperation
// main: deferred            ← defer en main
// panic: something went wrong
// goroutine 1 [running]:
// ...stack trace...
```

### Orden de ejecucion durante panic

```
┌──────────────────────────────────────────────────────────────┐
│  Cuando ocurre un panic:                                     │
│                                                              │
│  1. La ejecucion normal de la funcion actual se DETIENE       │
│  2. Los defers de la funcion actual se ejecutan (LIFO)       │
│  3. La funcion retorna a su caller                           │
│  4. Los defers del caller se ejecutan (LIFO)                 │
│  5. El caller retorna a SU caller                            │
│  6. ...se repite hasta main()                                │
│  7. Los defers de main se ejecutan                           │
│  8. El programa imprime el panic y stack trace               │
│  9. El programa termina con exit code 2                      │
│                                                              │
│  Un recover() en un defer puede DETENER este proceso         │
│  (lo veremos en T02 panic y recover)                         │
└──────────────────────────────────────────────────────────────┘
```

### Patron SysAdmin: Garantizar liberacion de lock

```go
func (s *Server) handleRequest(w http.ResponseWriter, r *http.Request) {
    s.mu.Lock()
    defer s.mu.Unlock()  // SIEMPRE se ejecuta, incluso si hay panic

    // Si este codigo hace panic, el mutex se libera igual
    // Sin defer, un panic dejaria el mutex bloqueado permanentemente
    // (deadlock en la siguiente request)
    result := s.process(r)
    json.NewEncoder(w).Encode(result)
}
```

### Patron: Logging de panic

```go
func safeHandler(name string, fn http.HandlerFunc) http.HandlerFunc {
    return func(w http.ResponseWriter, r *http.Request) {
        defer func() {
            if r := recover(); r != nil {
                log.Printf("PANIC in %s: %v\nStack:\n%s", name, r, debug.Stack())
                http.Error(w, "Internal Server Error", http.StatusInternalServerError)
            }
        }()
        fn(w, r)
    }
}

// Uso:
mux.HandleFunc("/api/users", safeHandler("getUsers", getUsers))
```

---

## 9. defer y os.Exit

**os.Exit NO ejecuta defers.** Esta es una excepcion importante:

```go
func main() {
    defer fmt.Println("this NEVER runs")
    defer cleanup()  // NUNCA se ejecuta

    fmt.Println("about to exit")
    os.Exit(1)
    // Los defers NO se ejecutan — os.Exit termina el proceso inmediatamente
}
```

### Implicaciones para SysAdmin

```go
// ✗ PROBLEMA — defer no se ejecuta con os.Exit
func main() {
    f, _ := os.Create("/var/run/myapp.pid")
    defer os.Remove("/var/run/myapp.pid")  // ← NUNCA se ejecuta

    // ... logica ...

    if fatalError {
        os.Exit(1)  // defer no se ejecuta, PID file queda huerfano
    }
}

// ✓ SOLUCION — usar funcion wrapper
func main() {
    os.Exit(run())
}

func run() int {
    f, _ := os.Create("/var/run/myapp.pid")
    defer os.Remove("/var/run/myapp.pid")  // se ejecuta al salir de run()

    // ... logica ...

    if fatalError {
        return 1  // return ejecuta defers de run()
    }
    return 0
}
```

### log.Fatal tambien llama os.Exit

```go
// log.Fatal = log.Print + os.Exit(1)
func main() {
    f, _ := os.Open("config.yaml")
    defer f.Close()  // ← NUNCA se ejecuta si Fatal se dispara

    data, err := io.ReadAll(f)
    if err != nil {
        log.Fatal(err)  // llama os.Exit(1) internamente
    }
}

// En tests, testMain, o funciones init, esto es especialmente peligroso:
// log.Fatal en una goroutine = defers de NINGUNA goroutine se ejecutan
```

```
┌──────────────────────────────────────────────────────────────┐
│  Funciones que NO ejecutan defers:                           │
│  ├─ os.Exit(code)         — terminacion inmediata            │
│  ├─ log.Fatal(...)        — log.Print + os.Exit(1)           │
│  ├─ log.Fatalf(...)       — log.Printf + os.Exit(1)         │
│  ├─ log.Fatalln(...)      — log.Println + os.Exit(1)        │
│  └─ runtime.Goexit()      — ¡SI ejecuta defers de la        │
│                              goroutine actual! (excepcion)   │
│                                                              │
│  Funciones que SI ejecutan defers:                           │
│  ├─ return                — siempre                          │
│  ├─ panic()               — siempre (unwinding del stack)    │
│  ├─ runtime.Goexit()      — ejecuta defers, luego termina   │
│  └─ Final de la funcion   — siempre                         │
└──────────────────────────────────────────────────────────────┘
```

---

## 10. Coste de defer

`defer` tiene un costo de rendimiento, aunque Go lo ha optimizado significativamente a lo largo de las versiones:

```go
// Go 1.1-1.12: defer costaba ~50-80ns (heap allocation cada vez)
// Go 1.13:     introduccion de open-coded defers: ~0-6ns
// Go 1.14+:    defers inline en la mayoria de casos: practicamente gratis

// El coste actual es negligible para el 99.9% de los casos
// Solo importa en hot paths de millones de iteraciones por segundo
```

### Benchmark: defer vs manual

```go
// Para la mayoria del codigo SysAdmin, la diferencia es imperceptible
func BenchmarkWithDefer(b *testing.B) {
    for i := 0; i < b.N; i++ {
        mu.Lock()
        defer mu.Unlock()
        // simular trabajo
    }
}
// ~6ns overhead en Go 1.22+

func BenchmarkWithoutDefer(b *testing.B) {
    for i := 0; i < b.N; i++ {
        mu.Lock()
        // simular trabajo
        mu.Unlock()
    }
}
```

### Cuando el coste importa (raro)

```go
// En hot loops de procesamiento de datos masivos:
// Ejemplo: procesar 100 millones de paquetes de red

// ✗ defer en hot loop — overhead innecesario
func processPackets(packets []Packet) {
    for _, pkt := range packets {
        mu.Lock()
        defer mu.Unlock()  // se acumula — ejecuta todos al final
        // ← Ademas, esto es un BUG: el defer acumula unlocks
        process(pkt)
    }
}

// ✓ Correcto Y performante
func processPackets(packets []Packet) {
    for _, pkt := range packets {
        mu.Lock()
        process(pkt)
        mu.Unlock()  // manual, sin overhead de defer
    }
}

// ✓ O extraer a funcion (defer no se acumula)
func processPackets(packets []Packet) {
    for _, pkt := range packets {
        processOnePacket(pkt)
    }
}
func processOnePacket(pkt Packet) {
    mu.Lock()
    defer mu.Unlock()
    process(pkt)
}
```

### Cuando defer NO se inlinea (open-coded)

```go
// defer se inlinea (rapido) en la mayoria de casos
// EXCEPTO:
// 1. Defers en loops (no se puede predecir cuantos habra)
// 2. Mas de 8 defers en una funcion
// 3. Defers con recover()

// En estos casos, Go usa la implementacion heap-allocated (~35-50ns)
// Sigue siendo rapido, pero hay que saberlo para hot paths
```

---

## 11. Patrones idiomaticos con defer

### Patron 1: Acquire/Release (el mas comun)

```go
// Files
f, err := os.Open(path)
if err != nil { return err }
defer f.Close()

// Locks
mu.Lock()
defer mu.Unlock()

// HTTP Response Body
resp, err := http.Get(url)
if err != nil { return err }
defer resp.Body.Close()

// Database transactions
tx, err := db.Begin()
if err != nil { return err }
defer tx.Rollback()  // safe: Rollback after Commit is a no-op

// Context cancel
ctx, cancel := context.WithTimeout(parentCtx, 5*time.Second)
defer cancel()  // liberar recursos del context

// Temp files
tmpFile, err := os.CreateTemp("", "prefix-*.tmp")
if err != nil { return err }
defer os.Remove(tmpFile.Name())
defer tmpFile.Close()
```

### Patron 2: Transaction con commit/rollback

```go
func transferMoney(db *sql.DB, from, to int, amount float64) (err error) {
    tx, err := db.Begin()
    if err != nil {
        return err
    }

    // defer Rollback es SEGURO incluso despues de Commit
    // porque Rollback despues de Commit retorna sql.ErrTxDone (ignorado)
    defer func() {
        if err != nil {
            tx.Rollback()
            return
        }
    }()

    _, err = tx.Exec("UPDATE accounts SET balance = balance - $1 WHERE id = $2", amount, from)
    if err != nil {
        return fmt.Errorf("debit: %w", err)
    }

    _, err = tx.Exec("UPDATE accounts SET balance = balance + $1 WHERE id = $2", amount, to)
    if err != nil {
        return fmt.Errorf("credit: %w", err)
    }

    return tx.Commit()
}
```

### Patron 3: State restore (testing y feature flags)

```go
// Guardar y restaurar estado — comun en tests
func TestWithCustomEnv(t *testing.T) {
    // Guardar valor original
    original := os.Getenv("LOG_LEVEL")
    os.Setenv("LOG_LEVEL", "debug")

    // Restaurar al final del test
    defer func() {
        if original == "" {
            os.Unsetenv("LOG_LEVEL")
        } else {
            os.Setenv("LOG_LEVEL", original)
        }
    }()

    // ... test que depende de LOG_LEVEL=debug ...
}
```

```go
// Helper generico para tests
func setEnv(t *testing.T, key, value string) {
    t.Helper()
    original, existed := os.LookupEnv(key)
    os.Setenv(key, value)
    t.Cleanup(func() {  // t.Cleanup es como defer pero para tests
        if existed {
            os.Setenv(key, original)
        } else {
            os.Unsetenv(key)
        }
    })
}
```

### Patron 4: Tracing y observabilidad

```go
func (s *Server) HandleDeploy(w http.ResponseWriter, r *http.Request) {
    // Tracing span
    ctx, span := tracer.Start(r.Context(), "HandleDeploy")
    defer span.End()

    // Request logging
    start := time.Now()
    defer func() {
        log.Printf("method=%s path=%s status=%d duration=%s",
            r.Method, r.URL.Path, w.(*responseWriter).status, time.Since(start))
    }()

    // Metrics
    activeRequests.Inc()
    defer activeRequests.Dec()

    // ... logica ...
}
```

### Patron 5: Working directory restore

```go
func runInDir(dir string, fn func() error) error {
    original, err := os.Getwd()
    if err != nil {
        return err
    }
    defer os.Chdir(original)  // restaurar directorio original

    if err := os.Chdir(dir); err != nil {
        return err
    }
    return fn()
}

// Uso:
err := runInDir("/tmp/build", func() error {
    return exec.Command("make", "all").Run()
})
// Al retornar, estamos de vuelta en el directorio original
```

---

## 12. defer y goroutines

Cada goroutine tiene su propia pila de defers. Un defer en una goroutine se ejecuta cuando esa goroutine's funcion retorna, no cuando la goroutine padre termina:

```go
func main() {
    defer fmt.Println("main: deferred")

    go func() {
        defer fmt.Println("goroutine: deferred")
        fmt.Println("goroutine: running")
        // "goroutine: deferred" se ejecuta al salir de esta closure
    }()

    time.Sleep(100 * time.Millisecond)
    fmt.Println("main: done")
}
// Output posible:
// goroutine: running
// goroutine: deferred
// main: done
// main: deferred
```

### Patron critico: defer wg.Done()

```go
func processJobs(jobs []Job) {
    var wg sync.WaitGroup

    for _, job := range jobs {
        wg.Add(1)
        go func(j Job) {
            defer wg.Done()  // SIEMPRE defer wg.Done()
            // Si el procesamiento hace panic, wg.Done() se ejecuta igual
            // Sin defer: un panic dejaria al WaitGroup esperando para siempre

            result := j.Execute()
            saveResult(result)
        }(job)
    }

    wg.Wait()
}
```

### Trampa: defer en goroutine de corta vida

```go
// ✗ La goroutine puede terminar antes de que el defer se necesite
func monitor() {
    go func() {
        f, _ := os.Open("/proc/loadavg")
        defer f.Close()
        data, _ := io.ReadAll(f)
        fmt.Println(string(data))
    }()
    // Si monitor() retorna, la goroutine puede o no haber terminado
    // Si la goroutine ya termino, defer se ejecuto
    // Pero si main termina antes, la goroutine se mata sin defer
}

// ✓ Asegurar que la goroutine termina
func monitor() {
    var wg sync.WaitGroup
    wg.Add(1)
    go func() {
        defer wg.Done()
        f, _ := os.Open("/proc/loadavg")
        defer f.Close()
        data, _ := io.ReadAll(f)
        fmt.Println(string(data))
    }()
    wg.Wait()  // esperar a que la goroutine (y sus defers) terminen
}
```

---

## 13. Patrones avanzados con defer

### Patron: Semaphore con defer

```go
// Limitar concurrencia con un channel buffered
type Semaphore chan struct{}

func NewSemaphore(max int) Semaphore {
    return make(Semaphore, max)
}

func (s Semaphore) Acquire() {
    s <- struct{}{}
}

func (s Semaphore) Release() {
    <-s
}

func processWithLimit(items []Item, maxConcurrent int) {
    sem := NewSemaphore(maxConcurrent)
    var wg sync.WaitGroup

    for _, item := range items {
        wg.Add(1)
        sem.Acquire()

        go func(it Item) {
            defer wg.Done()
            defer sem.Release()  // LIFO: Release antes de Done

            process(it)
        }(item)
    }

    wg.Wait()
}
```

### Patron: Conditional defer

```go
// No puedes hacer defer condicionalmente asi:
//   if condition { defer f() }   ← esto SI funciona, pero puede confundir

// Patron claro: registrar un no-op o la operacion real
func processFile(path string, readonly bool) error {
    f, err := os.OpenFile(path, os.O_RDWR, 0)
    if err != nil {
        return err
    }
    defer f.Close()

    if !readonly {
        // defer flush solo si estamos escribiendo
        defer func() {
            if err := f.Sync(); err != nil {
                log.Printf("warning: sync failed for %s: %v", path, err)
            }
        }()
    }

    // ... operaciones ...
    return nil
}
```

### Patron: Stack de cleanup con defer

```go
type CleanupStack struct {
    mu      sync.Mutex
    actions []func()
}

func (cs *CleanupStack) Add(fn func()) {
    cs.mu.Lock()
    defer cs.mu.Unlock()
    cs.actions = append(cs.actions, fn)
}

func (cs *CleanupStack) Run() {
    cs.mu.Lock()
    defer cs.mu.Unlock()

    // Ejecutar en orden inverso (LIFO, como defer)
    for i := len(cs.actions) - 1; i >= 0; i-- {
        cs.actions[i]()
    }
    cs.actions = nil
}

// Uso:
func setupEnvironment() (*Environment, error) {
    env := &Environment{}
    cleanup := &CleanupStack{}

    db, err := openDB()
    if err != nil {
        return nil, err
    }
    cleanup.Add(func() { db.Close() })

    cache, err := openCache()
    if err != nil {
        cleanup.Run()  // limpiar lo que ya se abrio
        return nil, err
    }
    cleanup.Add(func() { cache.Close() })

    // Todo bien — transferir ownership del cleanup al caller
    env.db = db
    env.cache = cache
    env.cleanup = cleanup
    return env, nil
}

func (env *Environment) Close() {
    env.cleanup.Run()
}
```

### Patron: defer con funciones que retornan error

```go
// Cuando el cleanup puede fallar y quieres reportarlo:
func processAndReport(path string) (err error) {
    f, err := os.Create(path)
    if err != nil {
        return err
    }

    // Patron: combinar errores de write y close
    defer func() {
        closeErr := f.Close()
        if err == nil {
            err = closeErr
        } else if closeErr != nil {
            // Ya hay error — agregar el error de close como contexto
            err = fmt.Errorf("%w (also: close failed: %v)", err, closeErr)
        }
    }()

    _, err = f.WriteString("important data")
    return err
}
```

---

## 14. defer en la stdlib de Go

### net/http — Server

```go
// Patron de la stdlib: defer para cerrar el Body
func handler(w http.ResponseWriter, r *http.Request) {
    // http.Request.Body siempre es non-nil (puede ser http.NoBody)
    defer r.Body.Close()  // no estrictamente necesario para requests
                          // pero es buena practica

    body, err := io.ReadAll(r.Body)
    // ...
}

// Cliente HTTP — AQUI SI es obligatorio
func fetchData(url string) ([]byte, error) {
    resp, err := http.Get(url)
    if err != nil {
        return nil, err
    }
    defer resp.Body.Close()  // OBLIGATORIO: sin esto, leak de conexion
    // La conexion TCP se recicla solo si el Body se lee y se cierra

    return io.ReadAll(resp.Body)
}
```

### database/sql — Transacciones

```go
// Patron clasico de la stdlib
func (s *Store) CreateUser(ctx context.Context, user User) error {
    tx, err := s.db.BeginTx(ctx, nil)
    if err != nil {
        return err
    }
    // defer Rollback es un patron SEGURO:
    // - Si Commit se ejecuta antes, Rollback retorna ErrTxDone (no-op)
    // - Si hay error/panic antes de Commit, Rollback limpia la transaccion
    defer tx.Rollback()

    _, err = tx.ExecContext(ctx, "INSERT INTO users (...) VALUES (...)", ...)
    if err != nil {
        return err  // Rollback se ejecuta via defer
    }

    _, err = tx.ExecContext(ctx, "INSERT INTO profiles (...) VALUES (...)", ...)
    if err != nil {
        return err  // Rollback se ejecuta via defer
    }

    return tx.Commit()  // si Commit falla, Rollback se ejecuta via defer
}
```

### sync — Mutex

```go
// Patron universalmente aceptado en Go
func (c *Cache) Get(key string) (interface{}, bool) {
    c.mu.RLock()
    defer c.mu.RUnlock()
    val, ok := c.data[key]
    return val, ok
}
```

### os/exec — Procesos

```go
func runCommand(name string, args ...string) (string, error) {
    ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
    defer cancel()  // SIEMPRE cancel() — libera goroutine interna del context

    cmd := exec.CommandContext(ctx, name, args...)
    output, err := cmd.CombinedOutput()
    return string(output), err
}
```

---

## 15. Ejemplo SysAdmin: Servidor de configuracion con defer

```go
package main

import (
    "bufio"
    "context"
    "encoding/json"
    "errors"
    "fmt"
    "io"
    "log"
    "net/http"
    "os"
    "os/signal"
    "path/filepath"
    "strings"
    "sync"
    "syscall"
    "time"
)

// ── Config store con locks y defer ─────────────────────────────

type ConfigStore struct {
    mu       sync.RWMutex
    data     map[string]string
    filePath string
    watchers []chan ConfigEvent
}

type ConfigEvent struct {
    Key      string `json:"key"`
    OldValue string `json:"old_value,omitempty"`
    NewValue string `json:"new_value"`
    Action   string `json:"action"` // "set", "delete", "reload"
}

func NewConfigStore(path string) *ConfigStore {
    return &ConfigStore{
        data:     make(map[string]string),
        filePath: path,
    }
}

func (cs *ConfigStore) Load() error {
    cs.mu.Lock()
    defer cs.mu.Unlock()

    f, err := os.Open(cs.filePath)
    if err != nil {
        if errors.Is(err, os.ErrNotExist) {
            log.Printf("config file %s not found — starting with empty config", cs.filePath)
            return nil
        }
        return fmt.Errorf("open config: %w", err)
    }
    defer f.Close()

    cs.data = make(map[string]string) // reset

    scanner := bufio.NewScanner(f)
    lineNum := 0
    for scanner.Scan() {
        lineNum++
        line := strings.TrimSpace(scanner.Text())

        if line == "" || strings.HasPrefix(line, "#") {
            continue
        }

        key, value, ok := strings.Cut(line, "=")
        if !ok {
            log.Printf("warning: %s:%d: invalid format (no '=')", cs.filePath, lineNum)
            continue
        }

        cs.data[strings.TrimSpace(key)] = strings.TrimSpace(value)
    }

    if err := scanner.Err(); err != nil {
        return fmt.Errorf("read config: %w", err)
    }

    log.Printf("loaded %d config entries from %s", len(cs.data), cs.filePath)
    return nil
}

func (cs *ConfigStore) Save() (err error) {
    cs.mu.RLock()
    defer cs.mu.RUnlock()

    // Escritura atomica: escribir a temp, luego rename
    tmpPath := cs.filePath + ".tmp"
    f, err := os.Create(tmpPath)
    if err != nil {
        return fmt.Errorf("create temp file: %w", err)
    }

    // defer para cleanup del archivo temporal en caso de error
    defer func() {
        if err != nil {
            os.Remove(tmpPath)
        }
    }()

    // defer para capturar error de Close (importante para escritura)
    defer func() {
        closeErr := f.Close()
        if err == nil {
            err = closeErr
        }
    }()

    // Escribir con header
    w := bufio.NewWriter(f)
    fmt.Fprintf(w, "# Config — auto-generated at %s\n", time.Now().Format(time.RFC3339))
    fmt.Fprintf(w, "# DO NOT EDIT while server is running\n\n")

    for key, value := range cs.data {
        fmt.Fprintf(w, "%s = %s\n", key, value)
    }

    if err = w.Flush(); err != nil {
        return fmt.Errorf("flush: %w", err)
    }

    if err = f.Sync(); err != nil {
        return fmt.Errorf("sync: %w", err)
    }

    // Atomic rename
    if err = os.Rename(tmpPath, cs.filePath); err != nil {
        return fmt.Errorf("rename: %w", err)
    }

    log.Printf("saved %d config entries to %s", len(cs.data), cs.filePath)
    return nil
}

func (cs *ConfigStore) Get(key string) (string, bool) {
    cs.mu.RLock()
    defer cs.mu.RUnlock()
    val, ok := cs.data[key]
    return val, ok
}

func (cs *ConfigStore) Set(key, value string) {
    cs.mu.Lock()
    defer cs.mu.Unlock()

    old := cs.data[key]
    cs.data[key] = value

    cs.notify(ConfigEvent{Key: key, OldValue: old, NewValue: value, Action: "set"})
}

func (cs *ConfigStore) Delete(key string) bool {
    cs.mu.Lock()
    defer cs.mu.Unlock()

    old, existed := cs.data[key]
    if !existed {
        return false
    }
    delete(cs.data, key)

    cs.notify(ConfigEvent{Key: key, OldValue: old, Action: "delete"})
    return true
}

func (cs *ConfigStore) All() map[string]string {
    cs.mu.RLock()
    defer cs.mu.RUnlock()

    // Retornar copia para evitar data races
    copy := make(map[string]string, len(cs.data))
    for k, v := range cs.data {
        copy[k] = v
    }
    return copy
}

func (cs *ConfigStore) Watch() <-chan ConfigEvent {
    cs.mu.Lock()
    defer cs.mu.Unlock()

    ch := make(chan ConfigEvent, 10)
    cs.watchers = append(cs.watchers, ch)
    return ch
}

func (cs *ConfigStore) notify(event ConfigEvent) {
    // Llamado con mu ya locked
    for _, ch := range cs.watchers {
        select {
        case ch <- event:
        default:
            // watcher lento — descartar evento
        }
    }
}

func (cs *ConfigStore) CloseWatchers() {
    cs.mu.Lock()
    defer cs.mu.Unlock()

    for _, ch := range cs.watchers {
        close(ch)
    }
    cs.watchers = nil
}

// ── HTTP handlers ──────────────────────────────────────────────

func (cs *ConfigStore) handleGet(w http.ResponseWriter, r *http.Request) {
    key := r.PathValue("key")
    if key == "" {
        // Retornar todo
        w.Header().Set("Content-Type", "application/json")
        json.NewEncoder(w).Encode(cs.All())
        return
    }

    value, ok := cs.Get(key)
    if !ok {
        http.Error(w, fmt.Sprintf("key %q not found", key), http.StatusNotFound)
        return
    }

    w.Header().Set("Content-Type", "application/json")
    json.NewEncoder(w).Encode(map[string]string{"key": key, "value": value})
}

func (cs *ConfigStore) handleSet(w http.ResponseWriter, r *http.Request) {
    defer r.Body.Close()

    var req struct {
        Key   string `json:"key"`
        Value string `json:"value"`
    }

    if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
        http.Error(w, "invalid JSON", http.StatusBadRequest)
        return
    }

    if req.Key == "" {
        http.Error(w, "key is required", http.StatusBadRequest)
        return
    }

    cs.Set(req.Key, req.Value)

    // Guardar a disco de forma asincrona
    go func() {
        if err := cs.Save(); err != nil {
            log.Printf("error saving config: %v", err)
        }
    }()

    w.WriteHeader(http.StatusCreated)
    json.NewEncoder(w).Encode(map[string]string{"status": "ok"})
}

func (cs *ConfigStore) handleDelete(w http.ResponseWriter, r *http.Request) {
    key := r.PathValue("key")
    if key == "" {
        http.Error(w, "key is required", http.StatusBadRequest)
        return
    }

    if !cs.Delete(key) {
        http.Error(w, fmt.Sprintf("key %q not found", key), http.StatusNotFound)
        return
    }

    go func() {
        if err := cs.Save(); err != nil {
            log.Printf("error saving config: %v", err)
        }
    }()

    w.WriteHeader(http.StatusOK)
    json.NewEncoder(w).Encode(map[string]string{"status": "deleted"})
}

// ── Middleware con defer ───────────────────────────────────────

func loggingMiddleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        start := time.Now()

        // Wrapper para capturar status code
        wrapped := &statusWriter{ResponseWriter: w, status: 200}

        defer func() {
            log.Printf("%s %s %d %s",
                r.Method, r.URL.Path, wrapped.status, time.Since(start).Round(time.Microsecond))
        }()

        next.ServeHTTP(wrapped, r)
    })
}

type statusWriter struct {
    http.ResponseWriter
    status int
}

func (w *statusWriter) WriteHeader(status int) {
    w.status = status
    w.ResponseWriter.WriteHeader(status)
}

func recoveryMiddleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        defer func() {
            if err := recover(); err != nil {
                log.Printf("PANIC: %s %s: %v", r.Method, r.URL.Path, err)
                http.Error(w, "Internal Server Error", http.StatusInternalServerError)
            }
        }()
        next.ServeHTTP(w, r)
    })
}

// ── File watcher con defer ─────────────────────────────────────

func (cs *ConfigStore) watchFile(ctx context.Context) {
    ticker := time.NewTicker(5 * time.Second)
    defer ticker.Stop()

    var lastMod time.Time
    info, err := os.Stat(cs.filePath)
    if err == nil {
        lastMod = info.ModTime()
    }

    for {
        select {
        case <-ctx.Done():
            return
        case <-ticker.C:
            info, err := os.Stat(cs.filePath)
            if err != nil {
                continue
            }
            if info.ModTime().After(lastMod) {
                log.Println("config file changed — reloading")
                if err := cs.Load(); err != nil {
                    log.Printf("reload failed: %v", err)
                }
                lastMod = info.ModTime()
            }
        }
    }
}

// ── PID file con defer ─────────────────────────────────────────

func writePIDFile(path string) (cleanup func(), err error) {
    f, err := os.Create(path)
    if err != nil {
        return nil, fmt.Errorf("create PID file: %w", err)
    }
    defer f.Close()

    _, err = fmt.Fprintf(f, "%d\n", os.Getpid())
    if err != nil {
        os.Remove(path)
        return nil, fmt.Errorf("write PID: %w", err)
    }

    return func() {
        if err := os.Remove(path); err != nil {
            log.Printf("warning: failed to remove PID file: %v", err)
        }
    }, nil
}

// ── Main ───────────────────────────────────────────────────────

func main() {
    os.Exit(run())
}

func run() int {
    // Config
    configPath := filepath.Join(".", "app.conf")
    listenAddr := ":8080"
    pidPath := "/tmp/configserver.pid"

    // PID file
    removePID, err := writePIDFile(pidPath)
    if err != nil {
        log.Printf("warning: %v", err)
    } else {
        defer removePID()  // garantiza limpieza del PID file
    }

    // Config store
    store := NewConfigStore(configPath)
    if err := store.Load(); err != nil {
        log.Printf("Error loading config: %v", err)
        return 1
    }
    defer store.CloseWatchers()
    defer func() {
        if err := store.Save(); err != nil {
            log.Printf("Error saving config on exit: %v", err)
        }
    }()

    // File watcher
    ctx, stop := signal.NotifyContext(context.Background(),
        syscall.SIGINT, syscall.SIGTERM, syscall.SIGHUP)
    defer stop()

    go store.watchFile(ctx)

    // Event logger
    events := store.Watch()
    go func() {
        for event := range events {
            log.Printf("CONFIG EVENT: %s key=%s old=%q new=%q",
                event.Action, event.Key, event.OldValue, event.NewValue)
        }
    }()

    // HTTP server
    mux := http.NewServeMux()
    mux.HandleFunc("GET /config", store.handleGet)
    mux.HandleFunc("GET /config/{key}", store.handleGet)
    mux.HandleFunc("POST /config", store.handleSet)
    mux.HandleFunc("DELETE /config/{key}", store.handleDelete)
    mux.HandleFunc("GET /health", func(w http.ResponseWriter, r *http.Request) {
        w.WriteHeader(http.StatusOK)
        io.WriteString(w, "ok\n")
    })

    handler := recoveryMiddleware(loggingMiddleware(mux))

    server := &http.Server{
        Addr:         listenAddr,
        Handler:      handler,
        ReadTimeout:  5 * time.Second,
        WriteTimeout: 10 * time.Second,
        IdleTimeout:  60 * time.Second,
    }

    // Graceful shutdown
    go func() {
        <-ctx.Done()
        log.Println("Shutting down server...")
        shutdownCtx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
        defer cancel()
        server.Shutdown(shutdownCtx)
    }()

    log.Printf("Config server listening on %s", listenAddr)
    if err := server.ListenAndServe(); err != nil && !errors.Is(err, http.ErrServerClosed) {
        log.Printf("Server error: %v", err)
        return 1
    }

    log.Println("Server stopped gracefully")
    return 0
}
```

---

## 16. Comparacion: Go vs C vs Rust

| Aspecto | Go | C | Rust |
|---|---|---|---|
| Mecanismo | `defer f()` | Manual / `__attribute__((cleanup))` | RAII (Drop trait) |
| Cuando se ejecuta | Al salir de la funcion | N/A (manual) | Al salir del scope |
| Scope | Funcion completa | N/A | Bloque `{ }` |
| Orden | LIFO | Manual | LIFO (orden de declaracion inverso) |
| Evaluacion de args | Al registrar | N/A | N/A (no hay args, Drop toma &mut self) |
| Ejecuta en panic | Si | N/A (no hay panic) | Si (Drop corre durante unwind) |
| Ejecuta en exit | **No** (os.Exit) | **No** (exit()) | **No** (std::process::exit) |
| Modificar return | Si (named returns) | N/A | No (return es antes de Drop) |
| Coste | ~0-6ns (inlined) | 0 (manual) | 0 (compilado en el scope) |
| Error handling | Closure puede inspeccionar err | N/A | Drop no puede retornar error |

### Go vs Rust — La diferencia de scope

```go
// Go — defer se ejecuta al salir de la FUNCION
func goExample() {
    {
        f, _ := os.Open("file.txt")
        defer f.Close()
        // f sigue abierto aqui...
    }
    // f SIGUE ABIERTO aqui!
    // f.Close() se ejecuta al salir de goExample()
}
```

```rust
// Rust — Drop se ejecuta al salir del SCOPE
fn rust_example() {
    {
        let f = File::open("file.txt").unwrap();
        // f se cierra automaticamente aqui
    } // <- Drop se llama aqui, f se cierra
    // f ya no existe aqui
}
```

```go
// Go — para simular scope de Rust, usa funcion o closure:
func goScopedExample() {
    func() {
        f, _ := os.Open("file.txt")
        defer f.Close()
        // usar f...
    }()
    // f esta cerrado aqui
}
```

### C — Cleanup manual (lo que Go resuelve)

```c
// C — cada path de error necesita cleanup manual
int process_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char *buf = malloc(1024);
    if (!buf) {
        fclose(f);          // cleanup f
        return -1;
    }

    int *data = malloc(sizeof(int) * 100);
    if (!data) {
        free(buf);          // cleanup buf
        fclose(f);          // cleanup f
        return -1;
    }

    // usar f, buf, data...

    free(data);             // cleanup en orden inverso
    free(buf);
    fclose(f);
    return 0;
}

// Go — defer maneja todo automaticamente
func processFile(path string) error {
    f, err := os.Open(path)
    if err != nil { return err }
    defer f.Close()

    buf := make([]byte, 1024)
    // buf no necesita free — GC se encarga
    // Solo un punto de cleanup: defer

    return nil
}
```

---

## 17. Errores comunes

| # | Error | Codigo | Solucion |
|---|---|---|---|
| 1 | defer en loop | `for { defer f.Close() }` | Extraer a funcion separada |
| 2 | defer sin () | `defer func() { }` falta `()` | `defer func() { ... }()` |
| 3 | Evaluar args despues | Esperar valor actual en defer arg | Usar closure para capturar referencia |
| 4 | Ignorar error de Close en escritura | `defer f.Close()` para archivos escritos | Usar named return + closure |
| 5 | defer con os.Exit | `defer cleanup(); os.Exit(1)` | Usar `return` o `run()` wrapper |
| 6 | defer con log.Fatal | `defer cleanup(); log.Fatal(err)` | Usar `log.Print + return` o wrapper |
| 7 | Orden incorrecto de defer | Asumir FIFO | Recordar: LIFO (pila) |
| 8 | defer modifica return no nombrado | `defer func() { err = ... }()` sin named return | Usar `(err error)` en la firma |
| 9 | defer demasiado tarde | `f, err := Open(); if err... defer f.Close()` con defer antes del if | Siempre defer DESPUES del check de error |
| 10 | Asumir scope de bloque | Esperar que defer se ejecute al salir de `{ }` | defer es por funcion, no por bloque |
| 11 | defer en goroutine huerfana | `go func() { defer ... }()` sin esperar | Usar WaitGroup o context |
| 12 | defer nil function | `var fn func(); defer fn()` con fn nil | Verificar que fn no sea nil antes del defer |

### Error 9 en detalle — Defer en posicion incorrecta

```go
// ✗ BUG — si Open falla, f es nil, pero Close no se defiere
// Pero si pones el defer ANTES del check...
f, err := os.Open(path)
defer f.Close()  // ← si err != nil, f es nil → panic en Close()
if err != nil {
    return err
}

// ✓ CORRECTO — siempre defer DESPUES del error check
f, err := os.Open(path)
if err != nil {
    return err
}
defer f.Close()  // f es valido aqui, seguro hacer defer
```

### Error 8 en detalle — Named returns requeridos

```go
// ✗ El defer NO puede modificar el return sin named return
func readFile(path string) ([]byte, error) {
    f, err := os.Open(path)
    if err != nil {
        return nil, err
    }
    defer func() {
        // err es la variable LOCAL del defer, no el return value
        if err != nil {
            err = fmt.Errorf("readFile: %w", err)
        }
    }()
    // ...
}

// ✓ Con named return, defer puede modificar err
func readFile(path string) (data []byte, err error) {
    f, err := os.Open(path)
    if err != nil {
        return nil, err
    }
    defer func() {
        // err es el named return — modificarlo cambia lo que se retorna
        if err != nil {
            err = fmt.Errorf("readFile: %w", err)
        }
    }()
    // ...
}
```

---

## 18. Ejercicios

### Ejercicio 1: Prediccion de evaluacion de argumentos
```go
package main

import "fmt"

func main() {
    x := "hello"
    defer fmt.Println("arg:", x)
    defer func() {
        fmt.Println("closure:", x)
    }()
    x = "world"
    fmt.Println("body:", x)
}
```
**Prediccion**: ¿Que imprime cada linea? ¿En que orden? ¿Por que el arg y la closure muestran valores diferentes?

### Ejercicio 2: Prediccion de LIFO
```go
package main

import "fmt"

func main() {
    for i := 0; i < 4; i++ {
        defer fmt.Printf("%d ", i)
    }
    fmt.Print("start ")
}
```
**Prediccion**: ¿Que secuencia de numeros se imprime? ¿Por que los numeros no son todos 4?

### Ejercicio 3: Prediccion de named return
```go
package main

import "fmt"

func double(x int) (result int) {
    defer func() {
        result += 10
    }()
    return x * 2
}

func main() {
    fmt.Println(double(5))
    fmt.Println(double(0))
}
```
**Prediccion**: ¿Que retorna `double(5)` y `double(0)`? ¿Por que no es simplemente 10 y 0?

### Ejercicio 4: Prediccion de defer en loop
```go
package main

import "fmt"

func main() {
    for i := 0; i < 3; i++ {
        defer func(n int) {
            fmt.Printf("deferred: %d\n", n)
        }(i)
        fmt.Printf("registered: %d\n", i)
    }
    fmt.Println("loop done")
}
```
**Prediccion**: ¿Cuantos defers se registran? ¿En que orden se ejecutan? ¿Que imprime completo?

### Ejercicio 5: Prediccion de defer con panic
```go
package main

import "fmt"

func inner() {
    defer fmt.Println("inner: deferred 1")
    defer fmt.Println("inner: deferred 2")
    fmt.Println("inner: about to panic")
    panic("boom")
}

func outer() {
    defer fmt.Println("outer: deferred")
    fmt.Println("outer: calling inner")
    inner()
    fmt.Println("outer: after inner")  // ¿se ejecuta?
}

func main() {
    defer fmt.Println("main: deferred")
    fmt.Println("main: calling outer")
    outer()
    fmt.Println("main: after outer")  // ¿se ejecuta?
}
```
**Prediccion**: ¿Que lineas se imprimen antes del panic trace? ¿En que orden?

### Ejercicio 6: Prediccion de defer con os.Exit
```go
package main

import (
    "fmt"
    "os"
)

func cleanup() {
    fmt.Println("cleanup called")
}

func main() {
    defer cleanup()
    defer fmt.Println("main deferred")
    fmt.Println("about to exit")
    os.Exit(0)
}
```
**Prediccion**: ¿Que se imprime? ¿Se ejecutan los defers?

### Ejercicio 7: Prediccion de defer con metodos
```go
package main

import "fmt"

type DB struct {
    name string
}

func (db DB) Close() {
    fmt.Printf("closing %s\n", db.name)
}

func main() {
    db := DB{name: "primary"}
    defer db.Close()
    db.name = "replica"
    db.Close()
}
```
**Prediccion**: ¿Que se imprime? ¿El defer cierra "primary" o "replica"?

### Ejercicio 8: Safe file writer
Implementa una funcion `SafeWrite(path string, data []byte) error` que:
1. Escriba a un archivo temporal en el mismo directorio
2. Use defer para eliminar el temporal en caso de error
3. Use defer con named return para capturar errores de Close
4. Haga Sync antes de Close
5. Haga Rename atomico solo si todo salio bien

### Ejercicio 9: Connection pool con defer
Implementa un mini connection pool que:
1. Tenga `Acquire()` y `Release()` con un channel buffered
2. Use defer para garantizar que `Release()` siempre se llama
3. Mida el tiempo de uso de cada conexion usando el patron `measureTime`
4. Tenga un timeout para Acquire usando context

### Ejercicio 10: Multi-resource initializer
Implementa una funcion que inicialice 4 recursos en secuencia:
1. Si todo sale bien, retorna todos los recursos
2. Si alguno falla, cierra los que ya se abrieron (en orden inverso)
3. Usa defer con named return y un flag `success` que controla si el defer hace cleanup
4. Compara con el patron goto que vimos en T04

---

## 19. Resumen

```
┌──────────────────────────────────────────────────────────────┐
│              defer EN GO — RESUMEN                            │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  TRES REGLAS                                                 │
│  1. Argumentos se evaluan al REGISTRAR el defer              │
│     (excepto closures, que capturan la referencia)           │
│  2. Multiples defers se ejecutan en orden LIFO (pila)        │
│  3. defer puede leer/modificar named return values           │
│                                                              │
│  CUANDO SE EJECUTA                                           │
│  ├─ Al salir de la FUNCION (no del bloque)                   │
│  ├─ En return normal                                         │
│  ├─ En panic (stack unwinding)                               │
│  ├─ En runtime.Goexit                                        │
│  ├─ NO en os.Exit / log.Fatal                                │
│  └─ Cada goroutine tiene su propia pila de defers            │
│                                                              │
│  TRAMPAS                                                     │
│  ├─ defer en loop = acumula hasta fin de funcion (leak!)     │
│  │   └─ Solucion: extraer a funcion separada                │
│  ├─ defer f.Close() ignora error de Close                    │
│  │   └─ Para escritura: usar named return + closure          │
│  ├─ defer ANTES del error check = posible nil panic          │
│  │   └─ Siempre: if err → return; defer close               │
│  └─ os.Exit / log.Fatal NO ejecuta defers                   │
│      └─ Solucion: usar run() → int + os.Exit(run())         │
│                                                              │
│  PATRONES IDIOMATICOS                                        │
│  ├─ Acquire/Release: Open/Close, Lock/Unlock, Cancel         │
│  ├─ Medir duracion: start → defer log(Since(start))         │
│  ├─ Enriquecer errores: defer func() { err = wrap(err) }    │
│  ├─ Transaction: defer Rollback (no-op after Commit)         │
│  ├─ State restore: guardar/restaurar env vars en tests       │
│  ├─ Recovery: defer func() { recover() }() en servers       │
│  └─ PID file: defer os.Remove(pidPath)                       │
│                                                              │
│  PERFORMANCE                                                 │
│  ├─ Go 1.14+: ~0-6ns (open-coded, practicamente gratis)     │
│  ├─ Excepciones: loops, >8 defers, recover (~35-50ns)        │
│  └─ Para 99.9% del codigo: el coste es irrelevante          │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: T02 - panic y recover — cuando usar panic (programmer errors), recover en deferred functions, no usar como excepciones
