# for — El Único Loop en Go

## 1. Introducción

Go tiene **un solo** mecanismo de bucle: `for`. No hay `while`, no hay `do...while`, no hay `foreach`, no hay `loop`. El `for` de Go cubre todos estos casos con una sintaxis unificada y limpia.

Esta decisión de diseño reduce la carga cognitiva — no tienes que elegir entre tipos de loop. Solo `for`, con cuatro variantes:

```
┌──────────────────────────────────────────────────────────────┐
│            LAS 4 FORMAS DEL for EN GO                        │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  1. for clásico (3 componentes):                             │
│     for i := 0; i < 10; i++ { }                             │
│                                                              │
│  2. for condicional (equivale a while):                      │
│     for condition { }                                        │
│                                                              │
│  3. for infinito (equivale a while true):                    │
│     for { }                                                  │
│                                                              │
│  4. for range (iteración sobre colecciones):                 │
│     for index, value := range collection { }                 │
│                                                              │
│  Sin paréntesis, llaves obligatorias                         │
│  break, continue y labels funcionan con for                  │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. for clásico — Tres componentes

La forma más familiar, equivalente al for de C/Java:

```go
// for init; condition; post { body }
for i := 0; i < 10; i++ {
    fmt.Println(i)
}
// i NO existe fuera del for (scope limitado)
```

### Anatomía

```
for init; condition; post {
    ↑       ↑          ↑
    │       │          └─ se ejecuta DESPUÉS de cada iteración
    │       └─ se evalúa ANTES de cada iteración (incluida la primera)
    └─ se ejecuta UNA vez al inicio
}
```

### Variaciones del for clásico

```go
// Todos los componentes son opcionales (pero los ; son obligatorios)

// Sin init (variable declarada antes)
i := 0
for ; i < 10; i++ {
    fmt.Println(i)
}
fmt.Println("final i:", i)  // 10 — i disponible fuera

// Sin post
for i := 0; i < 10; {
    fmt.Println(i)
    i += 2  // incremento manual
}

// Sin init ni post → se convierte en for condicional
// for ; condition; { }  ≡  for condition { }

// Múltiples variables
for i, j := 0, 10; i < j; i, j = i+1, j-1 {
    fmt.Printf("i=%d j=%d\n", i, j)
}
// i=0 j=10, i=1 j=9, i=2 j=8, i=3 j=7, i=4 j=6

// Decremento
for i := 10; i > 0; i-- {
    fmt.Printf("countdown: %d\n", i)
}
```

### Patrones SysAdmin con for clásico

```go
// Escanear puertos
func scanPorts(host string, start, end int) []int {
    var openPorts []int
    for port := start; port <= end; port++ {
        addr := fmt.Sprintf("%s:%d", host, port)
        conn, err := net.DialTimeout("tcp", addr, 200*time.Millisecond)
        if err == nil {
            openPorts = append(openPorts, port)
            conn.Close()
        }
    }
    return openPorts
}

// Retry con conteo de intentos
for attempt := 1; attempt <= maxRetries; attempt++ {
    err := doWork()
    if err == nil {
        break
    }
    log.Printf("attempt %d/%d failed: %v", attempt, maxRetries, err)
    time.Sleep(time.Duration(attempt) * time.Second)
}
```

---

## 3. for condicional — El "while" de Go

Solo una condición, sin init ni post. Es el equivalente exacto de `while` en otros lenguajes:

```go
// Leer hasta EOF
scanner := bufio.NewScanner(os.Stdin)
for scanner.Scan() {
    line := scanner.Text()
    fmt.Println(">>>", line)
}
if err := scanner.Err(); err != nil {
    log.Fatal(err)
}

// Esperar a que un servicio esté listo
for !isServiceReady(addr) {
    time.Sleep(time.Second)
}

// Consumir de un channel
for msg := range messages {
    process(msg)
}
```

### Patrones SysAdmin con for condicional

```go
// Esperar con timeout
func waitForPort(host string, port int, timeout time.Duration) error {
    deadline := time.Now().Add(timeout)
    addr := net.JoinHostPort(host, strconv.Itoa(port))

    for time.Now().Before(deadline) {
        conn, err := net.DialTimeout("tcp", addr, time.Second)
        if err == nil {
            conn.Close()
            return nil
        }
        time.Sleep(500 * time.Millisecond)
    }
    return fmt.Errorf("timeout waiting for %s after %s", addr, timeout)
}

// Leer un archivo línea por línea
func processLog(path string) error {
    file, err := os.Open(path)
    if err != nil {
        return err
    }
    defer file.Close()

    scanner := bufio.NewScanner(file)
    lineNum := 0
    for scanner.Scan() {
        lineNum++
        line := scanner.Text()
        if strings.Contains(line, "ERROR") {
            fmt.Printf("Line %d: %s\n", lineNum, line)
        }
    }
    return scanner.Err()
}

// Cola de trabajo manual
for len(queue) > 0 {
    // Pop del frente
    task := queue[0]
    queue = queue[1:]
    process(task)
}
```

---

## 4. for infinito — Loop sin condición

Sin ningún componente, `for` crea un loop infinito. Se sale con `break`, `return` o `os.Exit`:

```go
// Servidor/daemon principal
for {
    conn, err := listener.Accept()
    if err != nil {
        log.Printf("accept error: %v", err)
        continue
    }
    go handleConnection(conn)
}

// Event loop
for {
    select {
    case event := <-events:
        handleEvent(event)
    case <-ctx.Done():
        return
    }
}
```

### Patrones SysAdmin con for infinito

```go
// Monitor de servicios con intervalo
func monitorLoop(ctx context.Context, services []Service, interval time.Duration) {
    ticker := time.NewTicker(interval)
    defer ticker.Stop()

    // Primera ejecución inmediata
    runChecks(services)

    for {
        select {
        case <-ticker.C:
            runChecks(services)
        case <-ctx.Done():
            log.Println("monitor stopped")
            return
        }
    }
}

// Tail de un archivo (similar a tail -f)
func tailFile(path string) error {
    file, err := os.Open(path)
    if err != nil {
        return err
    }
    defer file.Close()

    // Ir al final del archivo
    file.Seek(0, io.SeekEnd)

    reader := bufio.NewReader(file)
    for {
        line, err := reader.ReadString('\n')
        if err != nil {
            if err == io.EOF {
                time.Sleep(100 * time.Millisecond)
                continue
            }
            return err
        }
        fmt.Print(line)
    }
}

// Lectura interactiva (REPL-style)
func repl() {
    scanner := bufio.NewScanner(os.Stdin)
    fmt.Print("> ")
    for scanner.Scan() {
        line := strings.TrimSpace(scanner.Text())
        if line == "exit" || line == "quit" {
            break
        }
        if line == "" {
            fmt.Print("> ")
            continue
        }
        execute(line)
        fmt.Print("> ")
    }
}
```

---

## 5. for range — Iteración sobre colecciones

La forma más versátil. Itera sobre slices, arrays, maps, strings, channels e integers (Go 1.22+):

```
┌──────────────────────────────────────────────────────────────┐
│           for range — TIPOS SOPORTADOS                       │
├────────────────┬─────────────────────────────────────────────┤
│  Tipo          │  index/key        │  value                  │
├────────────────┼───────────────────┼─────────────────────────┤
│  []T / [N]T    │  int (índice)     │  T (copia del elemento) │
│  string        │  int (byte pos)   │  rune (code point)      │
│  map[K]V       │  K (key)          │  V (copia del valor)    │
│  chan T         │  T (valor)        │  — (no hay segundo)     │
│  int (Go 1.22) │  int (0..n-1)     │  — (no hay segundo)     │
│  func (Go 1.23)│  depende del iter │  depende del iter       │
└────────────────┴───────────────────┴─────────────────────────┘
```

### Range sobre slices y arrays

```go
services := []string{"nginx", "postgres", "redis", "prometheus"}

// Índice y valor
for i, svc := range services {
    fmt.Printf("[%d] %s\n", i, svc)
}

// Solo índice (ignorar valor — más eficiente para structs grandes)
for i := range services {
    fmt.Printf("[%d] %s\n", i, services[i])
}

// Solo valor (ignorar índice)
for _, svc := range services {
    fmt.Println(svc)
}

// Go 1.22+: range sobre entero
for i := range 5 {
    fmt.Println(i)  // 0, 1, 2, 3, 4
}
// Equivale a: for i := 0; i < 5; i++
```

### Range sobre strings

```go
s := "café ☕"

// Range itera por RUNES (no bytes)
for i, r := range s {
    fmt.Printf("byte_offset=%d rune=%c (U+%04X)\n", i, r, r)
}
// byte_offset=0 rune=c (U+0063)
// byte_offset=1 rune=a (U+0061)
// byte_offset=2 rune=f (U+0066)
// byte_offset=3 rune=é (U+00E9)  ← 2 bytes, salta al 5
// byte_offset=5 rune=  (U+0020)
// byte_offset=6 rune=☕ (U+2615)  ← 3 bytes

// ⚠ El primer valor NO es el índice del carácter, sino la posición en bytes
// Para índice de carácter, usar []rune primero
```

### Range sobre maps

```go
config := map[string]string{
    "host":      "0.0.0.0",
    "port":      "8080",
    "log_level": "info",
}

// ⚠ El orden de iteración es ALEATORIO (no determinista)
for key, value := range config {
    fmt.Printf("%s = %s\n", key, value)
}

// Solo keys
for key := range config {
    fmt.Println(key)
}

// Si necesitas orden determinista — ordenar las keys
keys := make([]string, 0, len(config))
for k := range config {
    keys = append(keys, k)
}
sort.Strings(keys)
for _, k := range keys {
    fmt.Printf("%s = %s\n", k, config[k])
}
```

### Range sobre channels

```go
// Range itera hasta que el channel se cierra
jobs := make(chan string, 10)

// Producer
go func() {
    for _, job := range []string{"deploy", "backup", "cleanup"} {
        jobs <- job
    }
    close(jobs)  // ← IMPORTANTE: sin close, el range bloquea para siempre
}()

// Consumer — itera hasta que se cierra el channel
for job := range jobs {
    fmt.Println("Processing:", job)
}
fmt.Println("All jobs done")
```

---

## 6. Semántica de range — Copias y evaluación

### Range hace COPIAS del valor

```go
type Server struct {
    Name   string
    Port   int
    Status string
}

servers := []Server{
    {"web1", 80, "running"},
    {"web2", 80, "stopped"},
    {"db1", 5432, "running"},
}

// ✗ INCORRECTO — svc es una COPIA, modificarla no afecta el slice
for _, svc := range servers {
    svc.Status = "updated"  // modifica la copia, NO el original
}
fmt.Println(servers[0].Status)  // "running" — sin cambio

// ✓ CORRECTO — usar índice para modificar el original
for i := range servers {
    servers[i].Status = "updated"  // modifica el original
}
fmt.Println(servers[0].Status)  // "updated"

// ✓ CORRECTO — usar slice de punteros
ptrs := []*Server{
    {"web1", 80, "running"},
    {"web2", 80, "stopped"},
}
for _, svc := range ptrs {
    svc.Status = "updated"  // svc es puntero, modifica el original
}
```

### La expresión range se evalúa UNA vez

```go
// El slice se evalúa al inicio — agregar elementos no cambia la iteración
nums := []int{1, 2, 3}
for _, n := range nums {
    nums = append(nums, n*10)  // agrega al slice pero NO extiende la iteración
    fmt.Println(n)
}
// Imprime: 1, 2, 3 (solo los originales)
fmt.Println(nums)  // [1 2 3 10 20 30]

// La LONGITUD se captura al inicio
// Pero el CONTENIDO se lee en vivo (si modificas un elemento no visitado, lo verás)
```

### Range sobre map — Seguro para delete

```go
// Es seguro eliminar elementos del map durante range
for key, val := range config {
    if val == "" {
        delete(config, key)  // ✓ seguro durante range
    }
}

// También es seguro agregar, pero no se garantiza que los nuevos se visiten
for key := range config {
    config[key+"_backup"] = config[key]  // puede o no visitarse
}
```

---

## 7. `break` y `continue`

### break — Salir del loop

```go
// Break sale del for más interno
for i := 0; i < 100; i++ {
    if i*i > 1000 {
        fmt.Printf("first square > 1000: %d^2 = %d\n", i, i*i)
        break  // sale del for
    }
}

// Buscar en un slice
func findService(services []Service, name string) (Service, bool) {
    for _, svc := range services {
        if svc.Name == name {
            return svc, true  // return también sale, obviamente
        }
    }
    return Service{}, false
}
```

### continue — Saltar a la siguiente iteración

```go
// continue salta el resto del body y va al post (o la siguiente iteración de range)
for _, line := range lines {
    // Saltar líneas vacías y comentarios
    line = strings.TrimSpace(line)
    if line == "" {
        continue
    }
    if strings.HasPrefix(line, "#") {
        continue
    }

    // Procesar solo líneas válidas
    processLine(line)
}
```

### Patrón SysAdmin: filtrado con continue

```go
// Parsear /etc/passwd filtrando con continue
func listSystemUsers(minUID, maxUID int) ([]string, error) {
    file, err := os.Open("/etc/passwd")
    if err != nil {
        return nil, err
    }
    defer file.Close()

    var users []string
    scanner := bufio.NewScanner(file)

    for scanner.Scan() {
        line := scanner.Text()

        // Saltar comentarios
        if strings.HasPrefix(line, "#") {
            continue
        }

        fields := strings.Split(line, ":")
        if len(fields) < 7 {
            continue  // línea malformada
        }

        uid, err := strconv.Atoi(fields[2])
        if err != nil {
            continue  // UID no numérico
        }

        // Filtrar por rango de UID
        if uid < minUID || uid > maxUID {
            continue
        }

        // Solo shells interactivas
        shell := fields[6]
        if shell == "/usr/sbin/nologin" || shell == "/bin/false" {
            continue
        }

        users = append(users, fields[0])
    }

    return users, scanner.Err()
}
```

---

## 8. Loops anidados y labels

### Loops anidados — Patrón común

```go
// Escanear puertos en múltiples hosts
hosts := []string{"192.168.1.1", "192.168.1.2", "192.168.1.3"}
ports := []int{22, 80, 443, 8080}

for _, host := range hosts {
    for _, port := range ports {
        addr := fmt.Sprintf("%s:%d", host, port)
        conn, err := net.DialTimeout("tcp", addr, 200*time.Millisecond)
        if err != nil {
            continue  // ← solo salta este port, no este host
        }
        conn.Close()
        fmt.Printf("OPEN: %s\n", addr)
    }
}
```

### Labels — break/continue del loop exterior

```go
// break solo sale del loop más interno
// Para salir de un loop exterior, necesitas un label

outer:
    for _, host := range hosts {
        for _, port := range ports {
            addr := fmt.Sprintf("%s:%d", host, port)
            conn, err := net.DialTimeout("tcp", addr, 200*time.Millisecond)
            if err != nil {
                continue  // siguiente port
            }
            conn.Close()
            fmt.Printf("Found open port on %s:%d\n", host, port)
            continue outer  // ← siguiente HOST (no siguiente port)
        }
    }
```

```go
// break con label — salir de ambos loops
search:
    for _, host := range hosts {
        for _, port := range ports {
            if isVulnerable(host, port) {
                fmt.Printf("ALERT: %s:%d is vulnerable!\n", host, port)
                break search  // ← sale de AMBOS loops
            }
        }
    }
    // ejecución continúa aquí después del break search
```

El tema de labels se profundizará en T04, pero es esencial entender su relación con `for` aquí.

---

## 9. for range con Go 1.22+ — Integer ranges y loop variable fix

### Range sobre enteros (Go 1.22+)

```go
// Antes de Go 1.22
for i := 0; i < 5; i++ {
    fmt.Println(i)
}

// Go 1.22+ — más limpio
for i := range 5 {
    fmt.Println(i)  // 0, 1, 2, 3, 4
}

// Útil para repetir N veces sin necesitar el índice
for range 3 {
    fmt.Println("retry...")
}
```

### Fix de la variable de loop (Go 1.22+)

Antes de Go 1.22, la variable de iteración se **reutilizaba** en cada iteración, causando bugs con goroutines y closures:

```go
// ✗ BUG antes de Go 1.22 — todas las goroutines ven el mismo i
// En Go 1.22+, esto funciona correctamente
values := []int{1, 2, 3, 4, 5}

var wg sync.WaitGroup
for _, v := range values {
    wg.Add(1)
    go func() {
        defer wg.Done()
        fmt.Println(v)  // Go <1.22: todas imprimían 5
                         // Go 1.22+: cada una imprime su valor
    }()
}
wg.Wait()
```

```go
// Pre-Go 1.22: el fix manual era capturar la variable
for _, v := range values {
    v := v  // shadow: crea copia local para esta iteración
    wg.Add(1)
    go func() {
        fmt.Println(v)  // cada goroutine tiene su copia
    }()
}

// O pasar como argumento
for _, v := range values {
    wg.Add(1)
    go func(val int) {
        fmt.Println(val)
    }(v)
}
```

---

## 10. Iteradores con `for range` (Go 1.23+)

Go 1.23 introduce **range over functions** — iteradores custom que funcionan con `for range`:

```go
// Un iterador es una función que acepta una función yield
// Tipos de iteradores:
//   func(yield func() bool)           — sin valores
//   func(yield func(V) bool)          — un valor
//   func(yield func(K, V) bool)       — key y value

// Ejemplo: iterar sobre líneas de un archivo
func Lines(path string) iter.Seq[string] {
    return func(yield func(string) bool) {
        file, err := os.Open(path)
        if err != nil {
            return
        }
        defer file.Close()

        scanner := bufio.NewScanner(file)
        for scanner.Scan() {
            if !yield(scanner.Text()) {
                return  // el consumidor hizo break
            }
        }
    }
}

// Uso — se ve como un range normal
for line := range Lines("/var/log/syslog") {
    if strings.Contains(line, "error") {
        fmt.Println(line)
    }
}
```

```go
// Iterador con key-value
func Enumerate[T any](s []T) iter.Seq2[int, T] {
    return func(yield func(int, T) bool) {
        for i, v := range s {
            if !yield(i, v) {
                return
            }
        }
    }
}

// Iterar sobre un rango de IPs
func IPRange(start, end net.IP) iter.Seq[net.IP] {
    return func(yield func(net.IP) bool) {
        current := make(net.IP, len(start))
        copy(current, start)

        for bytes.Compare(current, end) <= 0 {
            ip := make(net.IP, len(current))
            copy(ip, current)
            if !yield(ip) {
                return
            }
            // Incrementar IP
            incrementIP(current)
        }
    }
}

// Uso
for ip := range IPRange(net.ParseIP("192.168.1.1"), net.ParseIP("192.168.1.10")) {
    fmt.Println(ip)
}
```

### La stdlib de Go 1.23+ incluye iteradores

```go
import (
    "maps"
    "slices"
)

// slices.All — como enumerate
for i, v := range slices.All(mySlice) {
    fmt.Printf("[%d] %v\n", i, v)
}

// slices.Values — solo valores
for v := range slices.Values(mySlice) {
    fmt.Println(v)
}

// slices.Backward — iteración inversa
for i, v := range slices.Backward(mySlice) {
    fmt.Printf("[%d] %v\n", i, v)
}

// maps.Keys — iterar solo keys (orden aleatorio)
for k := range maps.Keys(myMap) {
    fmt.Println(k)
}

// maps.Values — iterar solo values
for v := range maps.Values(myMap) {
    fmt.Println(v)
}

// slices.Sorted + maps.Keys para orden determinista
for _, k := range slices.Sorted(maps.Keys(myMap)) {
    fmt.Printf("%s = %v\n", k, myMap[k])
}
```

---

## 11. Patrones de loop para SysAdmin

### Poll/Watch con ticker

```go
func watchFile(ctx context.Context, path string, interval time.Duration) error {
    ticker := time.NewTicker(interval)
    defer ticker.Stop()

    var lastMod time.Time

    for {
        select {
        case <-ctx.Done():
            return ctx.Err()
        case <-ticker.C:
            info, err := os.Stat(path)
            if err != nil {
                log.Printf("cannot stat %s: %v", path, err)
                continue
            }
            if info.ModTime().After(lastMod) {
                if !lastMod.IsZero() {
                    log.Printf("%s modified at %s", path, info.ModTime().Format(time.DateTime))
                    // Recargar configuración, por ejemplo
                }
                lastMod = info.ModTime()
            }
        }
    }
}
```

### Worker pool con for

```go
func processJobs(ctx context.Context, jobs []Job, workers int) []Result {
    jobCh := make(chan Job, len(jobs))
    resultCh := make(chan Result, len(jobs))

    // Lanzar workers
    var wg sync.WaitGroup
    for i := 0; i < workers; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            for job := range jobCh {
                log.Printf("worker %d processing %s", id, job.Name)
                result := job.Execute(ctx)
                resultCh <- result
            }
        }(i)
    }

    // Enviar jobs
    for _, job := range jobs {
        jobCh <- job
    }
    close(jobCh)

    // Esperar y recolectar
    go func() {
        wg.Wait()
        close(resultCh)
    }()

    var results []Result
    for r := range resultCh {
        results = append(results, r)
    }
    return results
}
```

### Batch processing

```go
// Procesar en lotes de N
func processBatch[T any](items []T, batchSize int, fn func(batch []T) error) error {
    for i := 0; i < len(items); i += batchSize {
        end := i + batchSize
        if end > len(items) {
            end = len(items)
        }
        batch := items[i:end]
        if err := fn(batch); err != nil {
            return fmt.Errorf("batch starting at %d: %w", i, err)
        }
    }
    return nil
}

// Uso
servers := loadServers()  // []Server
err := processBatch(servers, 10, func(batch []Server) error {
    for _, s := range batch {
        if err := deployTo(s); err != nil {
            return err
        }
    }
    log.Printf("deployed batch of %d servers", len(batch))
    return nil
})
```

### Exponential backoff

```go
func withBackoff(ctx context.Context, maxRetries int, fn func() error) error {
    backoff := time.Second

    for attempt := 0; attempt < maxRetries; attempt++ {
        err := fn()
        if err == nil {
            return nil
        }

        if attempt == maxRetries-1 {
            return fmt.Errorf("all %d attempts failed: %w", maxRetries, err)
        }

        log.Printf("attempt %d failed: %v, retrying in %s", attempt+1, err, backoff)

        select {
        case <-ctx.Done():
            return ctx.Err()
        case <-time.After(backoff):
        }

        backoff *= 2
        if backoff > 30*time.Second {
            backoff = 30 * time.Second  // cap
        }
    }
    return nil
}
```

---

## 12. Iteración sobre /proc y filesystem

### Listar procesos desde /proc

```go
type ProcessInfo struct {
    PID     int
    Name    string
    State   string
    Memory  int64  // en KB
}

func listProcesses() ([]ProcessInfo, error) {
    entries, err := os.ReadDir("/proc")
    if err != nil {
        return nil, err
    }

    var procs []ProcessInfo
    for _, entry := range entries {
        // Solo directorios numéricos
        pid, err := strconv.Atoi(entry.Name())
        if err != nil {
            continue
        }

        // Leer /proc/PID/status
        statusPath := fmt.Sprintf("/proc/%d/status", pid)
        data, err := os.ReadFile(statusPath)
        if err != nil {
            continue  // proceso puede haber terminado
        }

        proc := ProcessInfo{PID: pid}
        for _, line := range strings.Split(string(data), "\n") {
            if strings.HasPrefix(line, "Name:") {
                proc.Name = strings.TrimSpace(strings.TrimPrefix(line, "Name:"))
            } else if strings.HasPrefix(line, "State:") {
                proc.State = strings.TrimSpace(strings.TrimPrefix(line, "State:"))
            } else if strings.HasPrefix(line, "VmRSS:") {
                fields := strings.Fields(line)
                if len(fields) >= 2 {
                    proc.Memory, _ = strconv.ParseInt(fields[1], 10, 64)
                }
            }
        }

        procs = append(procs, proc)
    }

    return procs, nil
}
```

### Walk de directorio recursivo

```go
// filepath.WalkDir usa for internamente, pero tú iteras con la callback
func findLargeFiles(root string, minSize int64) ([]string, error) {
    var largeFiles []string

    err := filepath.WalkDir(root, func(path string, d fs.DirEntry, err error) error {
        if err != nil {
            // No poder acceder a un subdirectorio no es fatal
            log.Printf("warning: %v", err)
            return nil  // o return fs.SkipDir para saltar el directorio
        }

        // Saltar directorios ocultos
        if d.IsDir() && strings.HasPrefix(d.Name(), ".") {
            return fs.SkipDir
        }

        if !d.IsDir() {
            info, err := d.Info()
            if err != nil {
                return nil
            }
            if info.Size() > minSize {
                largeFiles = append(largeFiles, path)
            }
        }
        return nil
    })

    return largeFiles, err
}
```

---

## 13. Performance de loops

### Preallocate slices

```go
// ✗ Sin pre-alocar — el slice crece y se reubica múltiples veces
func collectIPs(hosts []string) []net.IP {
    var ips []net.IP  // len=0, cap=0
    for _, host := range hosts {
        addrs, err := net.LookupHost(host)
        if err != nil {
            continue
        }
        for _, addr := range addrs {
            ips = append(ips, net.ParseIP(addr))
        }
    }
    return ips
}

// ✓ Con pre-alocación — una sola asignación
func collectIPs(hosts []string) []net.IP {
    ips := make([]net.IP, 0, len(hosts))  // estimamos 1 IP por host
    for _, host := range hosts {
        addrs, err := net.LookupHost(host)
        if err != nil {
            continue
        }
        for _, addr := range addrs {
            ips = append(ips, net.ParseIP(addr))
        }
    }
    return ips
}
```

### Evitar copias innecesarias con range

```go
type LargeStruct struct {
    Data [1024]byte
    Name string
    // ... muchos campos
}

items := make([]LargeStruct, 10000)

// ✗ Range copia cada LargeStruct (1KB+ cada vez)
for _, item := range items {
    process(item.Name)  // item es copia de 1KB
}

// ✓ Usar índice — acceso directo sin copia
for i := range items {
    process(items[i].Name)  // sin copia
}

// ✓ O usar slice de punteros
ptrItems := make([]*LargeStruct, 10000)
for _, item := range ptrItems {
    process(item.Name)  // item es puntero (8 bytes)
}
```

### Loop unrolling — Raro en Go

```go
// Go no hace auto-unrolling agresivo
// Si el performance es crítico, medir con benchmarks primero
// El compilador de Go es bueno optimizando loops simples

// Para la mayoría de código SysAdmin, la claridad > micro-optimización
```

---

## 14. Errores comunes con for

### Modificar slice durante range

```go
// ✗ PELIGROSO — eliminar elementos durante iteración
items := []string{"a", "b", "c", "d"}
for i, item := range items {
    if item == "b" {
        items = append(items[:i], items[i+1:]...)
        // El slice se acortó, pero range usa la longitud original
        // Puede causar panic por índice fuera de rango
    }
}

// ✓ CORRECTO — iterar al revés para eliminar
for i := len(items) - 1; i >= 0; i-- {
    if items[i] == "b" {
        items = append(items[:i], items[i+1:]...)
    }
}

// ✓ CORRECTO — construir nuevo slice
filtered := items[:0]  // reusar la memoria del slice original
for _, item := range items {
    if item != "b" {
        filtered = append(filtered, item)
    }
}
items = filtered

// ✓ Go 1.21+: slices.DeleteFunc
items = slices.DeleteFunc(items, func(s string) bool {
    return s == "b"
})
```

### Olvidar `close` en channel con range

```go
// ✗ DEADLOCK — range espera por siempre si no se cierra el channel
ch := make(chan int)
go func() {
    for i := 0; i < 5; i++ {
        ch <- i
    }
    // ← falta close(ch)
}()
for v := range ch {  // bloquea para siempre después de recibir 5 valores
    fmt.Println(v)
}

// ✓ Cerrar el channel cuando el producer termina
go func() {
    for i := 0; i < 5; i++ {
        ch <- i
    }
    close(ch)  // ← range termina cuando el channel se cierra
}()
```

### Off-by-one

```go
// ✗ off-by-one: no incluye el último
items := []string{"a", "b", "c"}
for i := 0; i < len(items)-1; i++ {  // ← pierde "c"
    fmt.Println(items[i])
}

// ✓ Correcto
for i := 0; i < len(items); i++ {
    fmt.Println(items[i])
}

// ✓ O simplemente usar range
for _, item := range items {
    fmt.Println(item)
}
```

---

## 15. Ejemplo SysAdmin: Monitor de sistema con loops

```go
package main

import (
    "bufio"
    "context"
    "fmt"
    "log"
    "net"
    "os"
    "os/signal"
    "sort"
    "strconv"
    "strings"
    "syscall"
    "time"
)

type SystemSnapshot struct {
    Timestamp  time.Time
    CPUPercent float64
    MemUsedMB  int64
    MemTotalMB int64
    DiskUsedGB float64
    DiskTotalGB float64
    LoadAvg1   float64
    LoadAvg5   float64
    LoadAvg15  float64
    ProcessCount int
    OpenPorts  []int
}

// readCPUStats lee /proc/stat y retorna total e idle jiffies
func readCPUStats() (total, idle int64, err error) {
    data, err := os.ReadFile("/proc/stat")
    if err != nil {
        return 0, 0, err
    }

    // Primera línea: cpu user nice system idle iowait irq softirq
    line := strings.SplitN(string(data), "\n", 2)[0]
    fields := strings.Fields(line)
    if len(fields) < 8 {
        return 0, 0, fmt.Errorf("unexpected /proc/stat format")
    }

    for i := 1; i < len(fields); i++ {
        val, _ := strconv.ParseInt(fields[i], 10, 64)
        total += val
        if i == 4 {
            idle = val
        }
    }
    return total, idle, nil
}

// getCPUPercent calcula % CPU entre dos lecturas
func getCPUPercent(prevTotal, prevIdle, currTotal, currIdle int64) float64 {
    totalDelta := float64(currTotal - prevTotal)
    idleDelta := float64(currIdle - prevIdle)
    if totalDelta == 0 {
        return 0
    }
    return (1 - idleDelta/totalDelta) * 100
}

// getMemInfo lee /proc/meminfo
func getMemInfo() (totalMB, usedMB int64, err error) {
    data, err := os.ReadFile("/proc/meminfo")
    if err != nil {
        return 0, 0, err
    }

    var total, available int64
    for _, line := range strings.Split(string(data), "\n") {
        fields := strings.Fields(line)
        if len(fields) < 2 {
            continue
        }
        switch fields[0] {
        case "MemTotal:":
            total, _ = strconv.ParseInt(fields[1], 10, 64)
        case "MemAvailable:":
            available, _ = strconv.ParseInt(fields[1], 10, 64)
        }
    }

    totalMB = total / 1024
    usedMB = (total - available) / 1024
    return totalMB, usedMB, nil
}

// getLoadAvg lee /proc/loadavg
func getLoadAvg() (load1, load5, load15 float64, err error) {
    data, err := os.ReadFile("/proc/loadavg")
    if err != nil {
        return 0, 0, 0, err
    }

    fields := strings.Fields(string(data))
    if len(fields) < 3 {
        return 0, 0, 0, fmt.Errorf("unexpected loadavg format")
    }

    load1, _ = strconv.ParseFloat(fields[0], 64)
    load5, _ = strconv.ParseFloat(fields[1], 64)
    load15, _ = strconv.ParseFloat(fields[2], 64)
    return load1, load5, load15, nil
}

// countProcesses cuenta PIDs en /proc
func countProcesses() int {
    entries, err := os.ReadDir("/proc")
    if err != nil {
        return 0
    }
    count := 0
    for _, e := range entries {
        if _, err := strconv.Atoi(e.Name()); err == nil {
            count++
        }
    }
    return count
}

// checkPorts verifica qué puertos están escuchando
func checkPorts(ports []int) []int {
    var open []int
    for _, port := range ports {
        addr := fmt.Sprintf("localhost:%d", port)
        conn, err := net.DialTimeout("tcp", addr, 200*time.Millisecond)
        if err == nil {
            open = append(open, port)
            conn.Close()
        }
    }
    return open
}

// collectSnapshot recopila una instantánea del sistema
func collectSnapshot(prevTotal, prevIdle int64, monitorPorts []int) (SystemSnapshot, int64, int64) {
    snap := SystemSnapshot{Timestamp: time.Now()}

    // CPU
    currTotal, currIdle, err := readCPUStats()
    if err == nil && prevTotal > 0 {
        snap.CPUPercent = getCPUPercent(prevTotal, prevIdle, currTotal, currIdle)
    }

    // Memoria
    totalMB, usedMB, err := getMemInfo()
    if err == nil {
        snap.MemTotalMB = totalMB
        snap.MemUsedMB = usedMB
    }

    // Load average
    snap.LoadAvg1, snap.LoadAvg5, snap.LoadAvg15, _ = getLoadAvg()

    // Procesos
    snap.ProcessCount = countProcesses()

    // Puertos
    snap.OpenPorts = checkPorts(monitorPorts)

    return snap, currTotal, currIdle
}

func printSnapshot(snap SystemSnapshot, monitorPorts []int) {
    fmt.Printf("\033[2J\033[H")  // clear screen
    fmt.Println("╔══════════════════════════════════════════════════╗")
    fmt.Printf("║  System Monitor — %s  ║\n", snap.Timestamp.Format("2006-01-02 15:04:05"))
    fmt.Println("╠══════════════════════════════════════════════════╣")

    // CPU bar
    cpuBar := int(snap.CPUPercent / 2)  // 50 chars = 100%
    fmt.Printf("║  CPU:  [%-50s] %5.1f%%  \n",
        strings.Repeat("█", cpuBar)+strings.Repeat("░", 50-cpuBar),
        snap.CPUPercent)

    // Memory bar
    memPercent := float64(snap.MemUsedMB) / float64(snap.MemTotalMB) * 100
    memBar := int(memPercent / 2)
    fmt.Printf("║  MEM:  [%-50s] %5.1f%%  \n",
        strings.Repeat("█", memBar)+strings.Repeat("░", 50-memBar),
        memPercent)
    fmt.Printf("║         %d MB / %d MB\n", snap.MemUsedMB, snap.MemTotalMB)

    // Load
    fmt.Printf("║  Load: %.2f / %.2f / %.2f (1/5/15 min)\n",
        snap.LoadAvg1, snap.LoadAvg5, snap.LoadAvg15)

    // Procesos
    fmt.Printf("║  Processes: %d\n", snap.ProcessCount)

    // Puertos
    fmt.Println("╠══════════════════════════════════════════════════╣")
    fmt.Println("║  Services:")
    openSet := make(map[int]bool)
    for _, p := range snap.OpenPorts {
        openSet[p] = true
    }
    for _, port := range monitorPorts {
        status := "\033[31mDOWN\033[0m"
        if openSet[port] {
            status = "\033[32mUP  \033[0m"
        }
        fmt.Printf("║    Port %-5d  [%s]\n", port, status)
    }

    fmt.Println("╚══════════════════════════════════════════════════╝")
    fmt.Println("  Press Ctrl+C to exit")
}

func main() {
    // Configuración
    monitorPorts := []int{22, 80, 443, 3306, 5432, 6379, 8080, 9090}
    interval := 2 * time.Second

    sort.Ints(monitorPorts)

    // Graceful shutdown con signal
    ctx, stop := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
    defer stop()

    log.Println("Starting system monitor (Ctrl+C to stop)")

    // Primera lectura de CPU (necesitamos dos para calcular %)
    prevTotal, prevIdle, _ := readCPUStats()
    time.Sleep(time.Second)

    // ─── Loop principal ───────────────────────────────────
    ticker := time.NewTicker(interval)
    defer ticker.Stop()

    // Primera iteración inmediata
    snap, prevTotal, prevIdle := collectSnapshot(prevTotal, prevIdle, monitorPorts)
    printSnapshot(snap, monitorPorts)

    for {
        select {
        case <-ctx.Done():
            fmt.Println("\nShutting down monitor...")
            return
        case <-ticker.C:
            snap, prevTotal, prevIdle = collectSnapshot(prevTotal, prevIdle, monitorPorts)
            printSnapshot(snap, monitorPorts)
        }
    }
}
```

---

## 16. Ejemplo SysAdmin: Procesador de logs con pipeline

```go
package main

import (
    "bufio"
    "fmt"
    "os"
    "regexp"
    "sort"
    "strings"
)

// Pipeline de procesamiento con for encadenados
func main() {
    if len(os.Args) < 2 {
        fmt.Fprintf(os.Stderr, "Usage: %s <logfile>\n", os.Args[0])
        os.Exit(1)
    }

    file, err := os.Open(os.Args[1])
    if err != nil {
        fmt.Fprintf(os.Stderr, "Error: %v\n", err)
        os.Exit(1)
    }
    defer file.Close()

    // Paso 1: Leer líneas y clasificar
    var (
        errorLines   []string
        warnLines    []string
        ipCounts     = make(map[string]int)
        statusCounts = make(map[string]int)
        totalLines   int
    )

    // Regex para extraer IP y status de logs tipo Apache/nginx
    logPattern := regexp.MustCompile(
        `^(\S+)\s+\S+\s+\S+\s+\[.*?\]\s+"(\S+)\s+\S+\s+\S+"\s+(\d{3})`,
    )

    scanner := bufio.NewScanner(file)
    for scanner.Scan() {
        line := scanner.Text()
        totalLines++

        // Clasificar por severidad
        upper := strings.ToUpper(line)
        if strings.Contains(upper, "ERROR") || strings.Contains(upper, "FATAL") {
            errorLines = append(errorLines, line)
        } else if strings.Contains(upper, "WARN") {
            warnLines = append(warnLines, line)
        }

        // Extraer IP y status code
        if matches := logPattern.FindStringSubmatch(line); len(matches) >= 4 {
            ipCounts[matches[1]]++
            statusCounts[matches[3]]++
        }
    }

    if err := scanner.Err(); err != nil {
        fmt.Fprintf(os.Stderr, "Read error: %v\n", err)
        os.Exit(1)
    }

    // Paso 2: Reportar
    fmt.Printf("=== Log Analysis: %s ===\n\n", os.Args[1])
    fmt.Printf("Total lines:   %d\n", totalLines)
    fmt.Printf("Errors:        %d\n", len(errorLines))
    fmt.Printf("Warnings:      %d\n", len(warnLines))

    // Top IPs por frecuencia
    if len(ipCounts) > 0 {
        fmt.Println("\n--- Top 10 IPs ---")
        type ipCount struct {
            IP    string
            Count int
        }
        var sorted []ipCount
        for ip, count := range ipCounts {
            sorted = append(sorted, ipCount{ip, count})
        }
        sort.Slice(sorted, func(i, j int) bool {
            return sorted[i].Count > sorted[j].Count
        })
        for i, ic := range sorted {
            if i >= 10 {
                break
            }
            fmt.Printf("  %-20s %d requests\n", ic.IP, ic.Count)
        }
    }

    // Status codes
    if len(statusCounts) > 0 {
        fmt.Println("\n--- Status Codes ---")
        var codes []string
        for code := range statusCounts {
            codes = append(codes, code)
        }
        sort.Strings(codes)
        for _, code := range codes {
            bar := strings.Repeat("█", statusCounts[code]/10)
            fmt.Printf("  %s: %5d %s\n", code, statusCounts[code], bar)
        }
    }

    // Últimos 5 errores
    if len(errorLines) > 0 {
        fmt.Println("\n--- Last 5 Errors ---")
        start := len(errorLines) - 5
        if start < 0 {
            start = 0
        }
        for _, line := range errorLines[start:] {
            if len(line) > 120 {
                line = line[:117] + "..."
            }
            fmt.Printf("  %s\n", line)
        }
    }
}
```

---

## 17. Comparación: Go vs C vs Rust

| Aspecto | Go | C | Rust |
|---|---|---|---|
| While loop | `for condition { }` | `while (cond) { }` | `while cond { }` |
| Infinite loop | `for { }` | `while (1) { }` / `for(;;)` | `loop { }` |
| For clásico | `for i:=0; i<n; i++` | `for(i=0;i<n;i++)` | No tiene (usar `for i in 0..n`) |
| Foreach | `for _, v := range s` | No nativo | `for v in iter` |
| Range entero | `for i := range n` (1.22+) | No | `for i in 0..n` |
| do-while | No existe | `do { } while(c);` | No existe (`loop` + `break`) |
| Variable de loop | Copia nueva por iter (1.22+) | Reutilizada | Copia nueva por iter |
| break con valor | No | No | `break value` (en loop) |
| Labels | `label: for` + `break label` | `goto` (no labels en for) | `'label: for` + `break 'label` |
| Iteradores | `iter.Seq` (1.23+) | No nativo | `Iterator` trait |
| Map iteration | Orden aleatorio | N/A (no hay map nativo) | Orden de inserción (IndexMap) |
| Range over chan | `for v := range ch` | N/A | No nativo |

---

## 18. Errores comunes

| # | Error | Código | Solución |
|---|---|---|---|
| 1 | `while` keyword | `while true { }` | `for { }` |
| 2 | Paréntesis | `for (i := 0; i < 10; i++)` | Sin paréntesis |
| 3 | Range copia valor | Modificar `v` en `for _, v := range` | Usar `for i := range` y `s[i]` |
| 4 | Range map orden | Asumir orden determinista | Ordenar keys si necesitas orden |
| 5 | Closure en goroutine | `go func() { use(v) }()` pre-1.22 | Go 1.22+ lo arregla; o `v := v` |
| 6 | Channel sin close | `for v := range ch` deadlock | `close(ch)` cuando el producer termine |
| 7 | Modificar slice en range | `delete` durante iteración | Iterar al revés o crear nuevo slice |
| 8 | Off-by-one | `i < len(s)-1` pierde último | `i < len(s)` o `range` |
| 9 | Append en range | `for range s { s = append(s, x) }` | Length se captura al inicio, loop finito |
| 10 | for sin body útil | `for i := 0; i < n; i++ { }` vacío | Eliminar o agregar comentario de intención |
| 11 | range string = bytes | `for i := 0; i < len(s); i++` para chars | `for _, r := range s` para runes |
| 12 | break en switch | `break` en switch solo sale del switch | Usar label para salir del for exterior |

---

## 19. Ejercicios

### Ejercicio 1: Predicción de range sobre slice
```go
package main

import "fmt"

func main() {
    nums := []int{10, 20, 30, 40, 50}

    for i, v := range nums {
        nums[i] = v * 2
    }
    fmt.Println(nums)

    for i := range nums {
        nums = append(nums, nums[i]+1)
    }
    fmt.Println(len(nums))
}
```
**Predicción**: ¿Qué imprime después del primer loop? En el segundo loop, ¿`len(nums)` crece infinitamente o se detiene? ¿Por qué?

### Ejercicio 2: Predicción de range sobre map
```go
package main

import "fmt"

func main() {
    m := map[string]int{"a": 1, "b": 2, "c": 3}

    for k, v := range m {
        fmt.Printf("%s=%d ", k, v)
        if k == "a" {
            m["d"] = 4
        }
    }
    fmt.Println()
    fmt.Println("len:", len(m))
}
```
**Predicción**: ¿Siempre imprime la misma secuencia? ¿Se imprime "d=4"? ¿Cuál es `len(m)` al final?

### Ejercicio 3: Predicción de range sobre string
```go
package main

import "fmt"

func main() {
    s := "Go 日本語"

    count := 0
    for range s {
        count++
    }
    fmt.Println("range count:", count)
    fmt.Println("len:", len(s))

    for i := 0; i < len(s); i++ {
        fmt.Printf("%02x ", s[i])
    }
    fmt.Println()
}
```
**Predicción**: ¿Cuánto vale `count` vs `len(s)`? ¿Cuántos bytes en hex se imprimen?

### Ejercicio 4: Predicción de break en loop anidado
```go
package main

import "fmt"

func main() {
    matrix := [][]int{
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9},
    }

    target := 5
    found := false
    for i, row := range matrix {
        for j, val := range row {
            if val == target {
                fmt.Printf("Found %d at [%d][%d]\n", target, i, j)
                found = true
                break
            }
        }
        if found {
            break
        }
    }
}
```
**Predicción**: ¿Cuántas iteraciones totales se ejecutan? ¿Por qué se necesitan dos `break`?

### Ejercicio 5: Range value copy
```go
package main

import "fmt"

type Config struct {
    Name  string
    Value int
}

func main() {
    configs := []Config{
        {"timeout", 30},
        {"retries", 3},
        {"port", 8080},
    }

    for _, cfg := range configs {
        cfg.Value *= 2
    }
    fmt.Println("after range:", configs)

    for i := range configs {
        configs[i].Value *= 2
    }
    fmt.Println("after index:", configs)
}
```
**Predicción**: ¿Los valores se duplican en el primer loop? ¿Y en el segundo?

### Ejercicio 6: Channel range
```go
package main

import "fmt"

func main() {
    ch := make(chan int, 5)
    for i := range 5 {
        ch <- i * 10
    }
    close(ch)

    for v := range ch {
        fmt.Println(v)
    }

    // ¿Qué pasa si quitamos close(ch)?
}
```
**Predicción**: ¿Qué imprime? ¿Qué pasa sin `close(ch)`?

### Ejercicio 7: Port scanner concurrente
Implementa un port scanner que:
1. Reciba host, rango de puertos (start-end), y número de workers
2. Use un channel de jobs y un channel de resultados
3. Lance N goroutines worker con `for range jobsCh`
4. Recopile resultados e imprima puertos abiertos ordenados

### Ejercicio 8: File watcher
Implementa una función que:
1. Use un `for` infinito con `time.Ticker` para verificar cada 2 segundos
2. Compare `ModTime` del archivo con la última conocida
3. Si cambió, reimprima el contenido (como `watch cat file`)
4. Use `context.Context` para shutdown limpio con Ctrl+C

### Ejercicio 9: Batch DNS resolver
Implementa un resolver DNS que:
1. Reciba una lista de hostnames
2. Los procese en batches de 10 (usar for con step de 10)
3. Dentro de cada batch, resuelva en paralelo con goroutines
4. Imprima resultados formato: `hostname → IP1, IP2, ...`

### Ejercicio 10: Log stream processor
Implementa un procesador que:
1. Lea líneas de stdin (simula un `tail -f | program`)
2. Use `for scanner.Scan()` como loop principal
3. Filtre por patterns (ERROR, WARN, regex custom)
4. Cuente errores por minuto con un map `[minuto]count`
5. Si supera 10 errores/minuto, imprima alerta

---

## 20. Resumen

```
┌──────────────────────────────────────────────────────────────┐
│              for EN GO — RESUMEN                             │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  FORMAS                                                      │
│  ├─ for i := 0; i < n; i++ { }    → clásico (3 partes)      │
│  ├─ for condition { }              → while                   │
│  ├─ for { }                        → infinito                │
│  ├─ for i, v := range collection   → iteración               │
│  └─ for i := range n              → entero (Go 1.22+)       │
│                                                              │
│  RANGE TYPES                                                 │
│  ├─ []T, [N]T  → index (int), value (T copy)                │
│  ├─ string     → index (byte pos), value (rune)             │
│  ├─ map[K]V    → key (K), value (V) — orden aleatorio       │
│  ├─ chan T      → value (T) — bloquea, termina con close     │
│  ├─ int (1.22) → 0..n-1                                     │
│  └─ func (1.23)→ iteradores custom                           │
│                                                              │
│  CLAVE                                                       │
│  ├─ Range copia el valor — usar índice para modificar        │
│  ├─ Length del range se captura al inicio                    │
│  ├─ Maps: safe para delete durante range                     │
│  ├─ Go 1.22+ arregla bug de closure con variable de loop    │
│  ├─ break sale del for más interno (usar labels para outer)  │
│  └─ continue salta a la siguiente iteración                  │
│                                                              │
│  SYSADMIN                                                    │
│  ├─ for scanner.Scan() — leer archivos/stdin                 │
│  ├─ for { select { } } — event loop con channels            │
│  ├─ for range ch — consumer de worker pool                   │
│  ├─ Ticker + for — polling periódico                         │
│  ├─ for + backoff — retry con exponential delay              │
│  └─ Batch processing con for i += batchSize                  │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: T03 - switch — sin fall-through por defecto, fallthrough keyword, switch sin expresión, type switch
