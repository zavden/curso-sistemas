# Labels y goto — Control de Flujo Avanzado

## 1. Introduccion

Go tiene labels y `goto`. Si vienes de la escuela de "goto considered harmful" (Dijkstra, 1968), esto puede sorprender. Pero Go los incluye por una razon pragmatica: hay situaciones reales — especialmente en code generation, parsers, state machines y cleanup de recursos — donde `goto` produce codigo mas claro que la alternativa con flags booleanos anidados.

Dicho esto, el uso de labels en Go se divide en dos categorias muy diferentes:

```
┌──────────────────────────────────────────────────────────────┐
│              LABELS EN GO                                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  USO COMUN (lo veras en codigo real):                        │
│  ├─ break label     → salir de un for/switch/select externo  │
│  └─ continue label  → siguiente iteracion de un for externo  │
│                                                              │
│  USO RARO (pero existe):                                     │
│  └─ goto label      → salto incondicional a cualquier punto  │
│                                                              │
│  Sintaxis de un label:                                       │
│  NombreDelLabel:                                             │
│      sentencia                                               │
│                                                              │
│  Reglas:                                                     │
│  ├─ Un label DEBE usarse — si no, error de compilacion       │
│  ├─ Nombres: PascalCase o camelCase por convencion           │
│  ├─ Scope: la funcion donde se declara                       │
│  ├─ goto no puede saltar al interior de un bloque            │
│  ├─ goto no puede saltar sobre declaraciones de variables    │
│  └─ goto no puede saltar entre funciones                     │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Anatomia de un label

Un label es un identificador seguido de dos puntos (`:`), colocado antes de una sentencia:

```go
// Sintaxis:
// LabelName:
//     statement

myLabel:
    fmt.Println("this statement is labeled")
```

### Reglas del compilador

```go
// 1. Un label declarado DEBE usarse — si no, error de compilacion
func f() {
    unused:         // ✗ ERROR: label unused defined and not used
        fmt.Println("hello")
}

// 2. Labels son locales a la funcion — no son visibles fuera
func f() {
    myLabel:
        for { break myLabel }
}
func g() {
    goto myLabel  // ✗ ERROR: label myLabel not defined
}

// 3. No puedes tener dos labels con el mismo nombre en la misma funcion
func f() {
    dup:
        fmt.Println("first")
    dup:                     // ✗ ERROR: label dup already defined
        fmt.Println("second")
}

// 4. Un label puede ir antes de CUALQUIER sentencia
// (no solo antes de for/switch/select)
cleanup:
    os.Remove(tmpFile)

start:
    count++

end:
    return result
```

### Convencion de nombres

```go
// La stdlib de Go usa diferentes estilos:
// - PascalCase: Loop, Retry, Search, Done
// - camelCase: outerLoop, nextHost
// - UPPER_CASE: no es comun en Go (mas de C)

// Recomendacion: nombres descriptivos y cortos
outer:       // ✓ generico pero claro para loops anidados
scanHosts:   // ✓ describe la accion
retry:       // ✓ describe la intencion
Loop:        // ✓ la stdlib lo usa
```

---

## 3. break con label

`break` sin label sale de la estructura de control **mas interna** (`for`, `switch`, o `select`). Con label, sale de la estructura marcada por ese label:

```go
// Sin label — break sale del for mas interno
for i := 0; i < 10; i++ {
    for j := 0; j < 10; j++ {
        if i+j > 5 {
            break  // sale del for de j, pero el for de i continua
        }
    }
}

// Con label — break sale del for externo
outer:
for i := 0; i < 10; i++ {
    for j := 0; j < 10; j++ {
        if i+j > 5 {
            break outer  // sale de AMBOS loops
        }
    }
}
// ejecucion continua aqui
```

### Regla: el label debe estar en un for, switch, o select

```go
// El label para break DEBE estar inmediatamente antes de un for, switch, o select
// No puedes hacer break a un label arbitrario

myLabel:
    fmt.Println("hello")     // ← no es for/switch/select

for {
    break myLabel  // ✗ ERROR: invalid break label myLabel
}

// Correcto:
myLabel:
    for {
        break myLabel  // ✓ el label esta en un for
    }
```

### break label en for + switch (el caso mas comun)

El caso clasico donde NECESITAS `break label`: salir de un `for` desde dentro de un `switch` o `select`:

```go
// Sin label — BUG: break sale del switch, no del for
for {
    switch msg := receive(); msg.Type {
    case "quit":
        break          // ← sale del SWITCH, el for sigue!
    case "data":
        process(msg)
    }
}
// Este loop nunca termina

// Con label — CORRECTO
loop:
for {
    switch msg := receive(); msg.Type {
    case "quit":
        break loop     // ← sale del FOR
    case "data":
        process(msg)
    }
}
// Ejecucion continua aqui
```

### break label en for + select (event loops)

```go
func eventLoop(ctx context.Context, events <-chan Event, done <-chan struct{}) {
    ticker := time.NewTicker(30 * time.Second)
    defer ticker.Stop()

loop:
    for {
        select {
        case <-ctx.Done():
            log.Println("context cancelled")
            break loop  // ← sin label, solo saldria del select

        case <-done:
            log.Println("done signal received")
            break loop

        case event := <-events:
            handleEvent(event)

        case <-ticker.C:
            reportStatus()
        }
    }

    log.Println("event loop terminated")
    // cleanup aqui
}
```

### break label en loops anidados

```go
// Buscar un valor en una matriz — salir al encontrarlo
func findInMatrix(matrix [][]int, target int) (row, col int, found bool) {
search:
    for i, row := range matrix {
        for j, val := range row {
            if val == target {
                return i, j, true
                // Nota: return es mejor que break aqui
                // pero si necesitaras hacer algo despues del loop:
                // break search
            }
        }
    }
    return -1, -1, false
}
```

### Patron SysAdmin: Escaneo de hosts con break label

```go
func findVulnerableHost(hosts []string, ports []int) (string, int, bool) {
scan:
    for _, host := range hosts {
        for _, port := range ports {
            if checkVulnerability(host, port) {
                log.Printf("CRITICAL: %s:%d is vulnerable!", host, port)
                // Encontramos uno — no necesitamos seguir
                return host, port, true

                // Si no pudieramos retornar, usariamos:
                // vulnHost, vulnPort = host, port
                // break scan
            }
        }
    }
    return "", 0, false
}
```

---

## 4. continue con label

`continue` sin label salta a la siguiente iteracion del `for` mas interno. Con label, salta a la siguiente iteracion del `for` marcado:

```go
// Sin label — continue salta al siguiente j
for i := 0; i < 5; i++ {
    for j := 0; j < 5; j++ {
        if j == 2 {
            continue  // siguiente j (j=3)
        }
        fmt.Printf("(%d,%d) ", i, j)
    }
}

// Con label — continue salta al siguiente i
outer:
for i := 0; i < 5; i++ {
    for j := 0; j < 5; j++ {
        if j == 2 {
            continue outer  // siguiente i, salta el resto de j
        }
        fmt.Printf("(%d,%d) ", i, j)
    }
}
// Solo imprime: (0,0) (0,1) (1,0) (1,1) (2,0) (2,1) ...
// Nunca llega a j >= 2
```

### continue label es SOLO para for

```go
// continue solo tiene sentido con for — no con switch ni select
mySwitch:
    switch x {
    case 1:
        continue mySwitch  // ✗ ERROR: invalid continue label mySwitch
    }

// El label de continue DEBE estar en un for
outer:
    for i := 0; i < 10; i++ {
        switch x {
        case 1:
            continue outer  // ✓ outer esta en un for
        }
    }
```

### Patron SysAdmin: Procesar multiples archivos, saltar al siguiente en error

```go
func processConfigFiles(paths []string) error {
    var errors []string

nextFile:
    for _, path := range paths {
        data, err := os.ReadFile(path)
        if err != nil {
            errors = append(errors, fmt.Sprintf("%s: %v", path, err))
            continue nextFile  // siguiente archivo
        }

        lines := strings.Split(string(data), "\n")
        for lineNum, line := range lines {
            line = strings.TrimSpace(line)

            // Saltar lineas vacias y comentarios
            if line == "" || strings.HasPrefix(line, "#") {
                continue  // siguiente linea (for interno)
            }

            // Validar formato key=value
            if !strings.Contains(line, "=") {
                log.Printf("WARNING: %s:%d: invalid format, skipping file", path, lineNum+1)
                continue nextFile  // saltar TODA la archivo, ir al siguiente
            }

            key, value, _ := strings.Cut(line, "=")
            processConfig(strings.TrimSpace(key), strings.TrimSpace(value))
        }

        log.Printf("OK: processed %s", path)
    }

    if len(errors) > 0 {
        return fmt.Errorf("errors in %d files: %s", len(errors), strings.Join(errors, "; "))
    }
    return nil
}
```

### Patron SysAdmin: Verificar servicios en multiples servidores

```go
type ServerCheck struct {
    Host     string
    Services []string
    Results  map[string]bool
}

func checkAllServers(servers []ServerCheck, timeout time.Duration) {
nextServer:
    for i := range servers {
        servers[i].Results = make(map[string]bool)
        log.Printf("Checking %s...", servers[i].Host)

        // Primero verificar conectividad base
        conn, err := net.DialTimeout("tcp", servers[i].Host+":22", timeout)
        if err != nil {
            log.Printf("  SKIP %s — unreachable: %v", servers[i].Host, err)
            continue nextServer  // no tiene sentido verificar servicios
        }
        conn.Close()

        for _, svc := range servers[i].Services {
            port := servicePort(svc)
            addr := fmt.Sprintf("%s:%d", servers[i].Host, port)
            conn, err := net.DialTimeout("tcp", addr, timeout)
            if err != nil {
                servers[i].Results[svc] = false
                log.Printf("  ✗ %s:%d (%s) — down", servers[i].Host, port, svc)
                continue  // siguiente servicio (for interno)
            }
            conn.Close()
            servers[i].Results[svc] = true
            log.Printf("  ✓ %s:%d (%s) — up", servers[i].Host, port, svc)
        }
    }
}
```

---

## 5. goto — Salto incondicional

`goto` transfiere la ejecucion a un label dentro de la misma funcion. Es un salto incondicional y directo:

```go
func example() {
    fmt.Println("A")
    goto skip
    fmt.Println("B")  // nunca se ejecuta
skip:
    fmt.Println("C")  // ejecucion continua aqui
}
// Imprime: A, C
```

### Restricciones del compilador (muy importantes)

Go impone restricciones estrictas al `goto` para prevenir los problemas historicos del goto en C:

```go
// ─── Restriccion 1: No puede saltar SOBRE declaraciones de variables ───

func f() {
    goto end
    x := 5        // ✗ ERROR: goto end jumps over declaration of x
end:
    fmt.Println(x)
}

// ¿Por que? Porque x no estaria inicializada al llegar a `end:`

// Solucion A: declarar antes del goto
func f() {
    var x int
    goto end
    x = 5         // asignacion, no declaracion — OK
end:
    fmt.Println(x)  // x = 0 (zero value)
}

// Solucion B: declarar despues del label
func f() {
    goto end
end:
    x := 5
    fmt.Println(x)
}
```

```go
// ─── Restriccion 2: No puede saltar AL INTERIOR de un bloque ───

func f() {
    goto inside
    {
    inside:          // ✗ ERROR: goto inside jumps into block
        fmt.Println("inside")
    }
}

func f() {
    goto inside
    if true {
    inside:          // ✗ ERROR: goto inside jumps into block
        fmt.Println("inside")
    }
}

func f() {
    goto inside
    for {
    inside:          // ✗ ERROR: goto inside jumps into block
        break
    }
}

// ¿Por que? Porque el estado del bloque (variables locales, 
// condiciones de loop) no estaria definido
```

```go
// ─── Restriccion 3: No puede saltar ENTRE funciones ───

func f() {
    goto there   // ✗ ERROR: label there not defined
}
func g() {
there:
    fmt.Println("hello")
}

// Los labels son locales a la funcion — no existen fuera
```

```go
// ─── Lo que SI puede hacer ───

// Saltar hacia adelante (forward jump)
func f() {
    goto end
    fmt.Println("skipped")
end:
    fmt.Println("reached")
}

// Saltar hacia atras (backward jump) — ¡crea un loop!
func f() {
    i := 0
start:
    if i >= 5 {
        goto done
    }
    fmt.Println(i)
    i++
    goto start  // loop manual
done:
    fmt.Println("finished")
}
// Imprime: 0 1 2 3 4 finished

// Saltar fuera de un bloque (de dentro hacia afuera)
func f() {
    for i := 0; i < 10; i++ {
        if i == 3 {
            goto done  // ✓ sale del for
        }
    }
done:
    fmt.Println("done")
}
```

### Resumen de restricciones

```
┌──────────────────────────────────────────────────────────────┐
│  goto PUEDE:                                                 │
│  ├─ Saltar hacia adelante (forward)                          │
│  ├─ Saltar hacia atras (backward — loop manual)              │
│  └─ Saltar de dentro de un bloque hacia afuera               │
│                                                              │
│  goto NO PUEDE:                                              │
│  ├─ Saltar sobre declaraciones de variables (:= o var)       │
│  ├─ Saltar al interior de un bloque (if, for, switch, {})    │
│  └─ Saltar entre funciones                                   │
│                                                              │
│  Estas restricciones son verificadas en COMPILE TIME          │
│  No puedes crear los bugs clasicos de goto en C              │
└──────────────────────────────────────────────────────────────┘
```

---

## 6. Cuando goto es legitimo

`goto` es raro en Go pero la stdlib lo usa en situaciones especificas. Veamos los patrones legitimos:

### Patron 1: Cleanup centralizado (el mas comun)

Antes de que Go tuviera `defer`, el pattern de cleanup con goto era muy comun en C. En Go, `defer` cubre la mayoria de casos, pero goto sigue siendo util cuando necesitas cleanup diferenciado segun donde fallo:

```go
// Con defer (preferido en la mayoria de casos)
func processFile(path string) error {
    f, err := os.Open(path)
    if err != nil {
        return err
    }
    defer f.Close()

    // ... procesamiento ...
    return nil
}

// Con goto — util cuando el cleanup es complejo y diferenciado
func complexSetup() error {
    db, err := openDB()
    if err != nil {
        return fmt.Errorf("database: %w", err)
    }

    cache, err := openCache()
    if err != nil {
        goto closeDB  // cleanup parcial: solo cerrar DB
    }

    mq, err := openMessageQueue()
    if err != nil {
        goto closeCache  // cleanup parcial: cerrar cache y DB
    }

    // Todo bien — usar los recursos
    err = doWork(db, cache, mq)
    mq.Close()

closeCache:
    cache.Close()
closeDB:
    db.Close()
    return err
}
```

Comparemos con la alternativa sin goto:

```go
// Sin goto — flags y nesting
func complexSetup() error {
    db, err := openDB()
    if err != nil {
        return fmt.Errorf("database: %w", err)
    }
    defer db.Close()

    cache, err := openCache()
    if err != nil {
        return fmt.Errorf("cache: %w", err)
    }
    defer cache.Close()

    mq, err := openMessageQueue()
    if err != nil {
        return fmt.Errorf("message queue: %w", err)
    }
    defer mq.Close()

    return doWork(db, cache, mq)
}
// En este caso, defer es mas limpio — preferirlo cuando sea posible
```

La diferencia real aparece cuando el orden de cleanup importa o cuando necesitas logica condicional en el cleanup:

```go
func migrationStep(db *sql.DB) error {
    tx, err := db.Begin()
    if err != nil {
        return err
    }

    _, err = tx.Exec("ALTER TABLE users ADD COLUMN email_verified BOOLEAN DEFAULT FALSE")
    if err != nil {
        goto rollback
    }

    _, err = tx.Exec("CREATE INDEX idx_users_email ON users(email)")
    if err != nil {
        goto rollback
    }

    _, err = tx.Exec("UPDATE users SET email_verified = TRUE WHERE email IS NOT NULL")
    if err != nil {
        goto rollback
    }

    return tx.Commit()

rollback:
    tx.Rollback()
    return fmt.Errorf("migration failed: %w", err)
}
```

### Patron 2: State machines generadas (code generation)

Herramientas que generan parsers (como `goyacc`, `ragel`, o protocol buffers) producen codigo con goto porque es la traduccion mas directa de un automata de estados:

```go
// Codigo generado por un lexer/parser (simplificado)
// NO escribirias esto a mano, pero lo veras en codigo generado
func lex(input []byte) []Token {
    var tokens []Token
    pos := 0

start:
    if pos >= len(input) {
        goto done
    }

    switch {
    case input[pos] == ' ' || input[pos] == '\t' || input[pos] == '\n':
        pos++
        goto start  // saltar whitespace

    case input[pos] >= '0' && input[pos] <= '9':
        goto number

    case input[pos] >= 'a' && input[pos] <= 'z':
        goto identifier

    case input[pos] == '"':
        goto stringLiteral

    default:
        tokens = append(tokens, Token{Type: TokError, Pos: pos})
        pos++
        goto start
    }

number:
    {
        start := pos
        for pos < len(input) && input[pos] >= '0' && input[pos] <= '9' {
            pos++
        }
        tokens = append(tokens, Token{Type: TokNumber, Value: string(input[start:pos])})
        goto start
    }

identifier:
    {
        start := pos
        for pos < len(input) && input[pos] >= 'a' && input[pos] <= 'z' {
            pos++
        }
        tokens = append(tokens, Token{Type: TokIdent, Value: string(input[start:pos])})
        goto start
    }

stringLiteral:
    {
        pos++ // saltar opening quote
        start := pos
        for pos < len(input) && input[pos] != '"' {
            pos++
        }
        tokens = append(tokens, Token{Type: TokString, Value: string(input[start:pos])})
        if pos < len(input) {
            pos++ // saltar closing quote
        }
        goto start
    }

done:
    return tokens
}
```

### Patron 3: Retry con cleanup en la stdlib de Go

La stdlib de Go usa goto en casos reales. Ejemplo simplificado del patron:

```go
// Patron inspirado en net/http de la stdlib
func (c *Client) do(req *Request) (*Response, error) {
    var resp *Response
    var err error

retry:
    resp, err = c.send(req)
    if err != nil {
        return nil, err
    }

    // Seguir redirects
    switch resp.StatusCode {
    case 301, 302, 307, 308:
        resp.Body.Close()

        if c.redirectCount >= c.maxRedirects {
            return nil, errors.New("too many redirects")
        }
        c.redirectCount++

        req, err = c.buildRedirect(req, resp)
        if err != nil {
            return nil, err
        }
        goto retry
    }

    return resp, nil
}
```

---

## 7. goto en la stdlib de Go — Casos reales

Para entender que goto no es taboo en Go, veamos donde la stdlib lo usa:

```go
// Estos son patrones REALES de la stdlib (simplificados):

// 1. compress/flate — decodificacion Huffman
//    goto nextBlock, goto osError
//    Razon: state machine de decompresion

// 2. encoding/json — scanner
//    goto scanError
//    Razon: multiples puntos de fallo que necesitan la misma logica de error

// 3. math/big — aritmetica de precision arbitraria
//    goto done
//    Razon: cleanup compartido despues de multiples caminos de ejecucion

// 4. net — resolver DNS
//    goto retry
//    Razon: reintentar lookup con diferentes servidores

// 5. runtime — scheduler de goroutines
//    goto top
//    Razon: loop principal del scheduler
```

### Estadisticas

```
┌──────────────────────────────────────────────────────────────┐
│  goto en la stdlib de Go (aproximado):                       │
│                                                              │
│  ~350 usos de goto en ~2 millones de lineas                  │
│  = ~0.018% de lineas usan goto                               │
│                                                              │
│  La mayoria en:                                               │
│  ├─ runtime/     (~100) — scheduler, GC, memory allocator    │
│  ├─ encoding/    (~40)  — parsers JSON, XML, CSV             │
│  ├─ compress/    (~35)  — algoritmos de compresion            │
│  ├─ crypto/      (~30)  — implementaciones criptograficas    │
│  ├─ math/big     (~20)  — aritmetica de precision arbitraria │
│  └─ net/         (~15)  — DNS resolver, HTTP client          │
│                                                              │
│  Casi todos son: cleanup centralizado, retry, o              │
│  state machines en codigo de bajo nivel                      │
│                                                              │
│  En codigo de aplicacion (cmd/, tool/):                      │
│  Practicamente 0 usos de goto                                │
└──────────────────────────────────────────────────────────────┘
```

---

## 8. goto vs alternativas — Cuando NO usar goto

La regla general: si puedes resolver el problema con `for`, `if`, `defer`, `break label`, o `continue label`, **no uses goto**. Solo considera goto cuando las alternativas producen codigo mas complejo.

### Caso 1: Loop — Usa for, no goto

```go
// ✗ Loop con goto — ilegible
func countDown() {
    i := 10
start:
    if i <= 0 {
        goto done
    }
    fmt.Println(i)
    i--
    goto start
done:
    fmt.Println("Launch!")
}

// ✓ for — obvio y claro
func countDown() {
    for i := 10; i > 0; i-- {
        fmt.Println(i)
    }
    fmt.Println("Launch!")
}
```

### Caso 2: Error handling — Usa if/return, no goto

```go
// ✗ goto para error handling simple
func process(path string) error {
    data, err := os.ReadFile(path)
    if err != nil {
        goto handleError
    }
    fmt.Println(string(data))
    return nil

handleError:
    return fmt.Errorf("failed: %w", err)
}

// ✓ Retorno directo — idiomatico
func process(path string) error {
    data, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed: %w", err)
    }
    fmt.Println(string(data))
    return nil
}
```

### Caso 3: Cleanup — Usa defer, no goto

```go
// ✗ goto para cleanup simple
func readConfig(path string) (Config, error) {
    var cfg Config
    f, err := os.Open(path)
    if err != nil {
        return cfg, err
    }

    err = json.NewDecoder(f).Decode(&cfg)
    if err != nil {
        goto cleanup
    }

    // validar
    if cfg.Port == 0 {
        err = errors.New("port is required")
        goto cleanup
    }

    return cfg, nil

cleanup:
    f.Close()
    return cfg, err
}

// ✓ defer — mas claro
func readConfig(path string) (Config, error) {
    f, err := os.Open(path)
    if err != nil {
        return Config{}, err
    }
    defer f.Close()

    var cfg Config
    if err := json.NewDecoder(f).Decode(&cfg); err != nil {
        return Config{}, err
    }

    if cfg.Port == 0 {
        return Config{}, errors.New("port is required")
    }

    return cfg, nil
}
```

### Caso 4: Salir de loops anidados — Usa break label, no goto

```go
// ✗ goto para salir de loops anidados
func findHost(matrix [][]string, target string) bool {
    for _, row := range matrix {
        for _, val := range row {
            if val == target {
                goto found
            }
        }
    }
    return false
found:
    return true
}

// ✓ break label — expresa la intencion
func findHost(matrix [][]string, target string) bool {
search:
    for _, row := range matrix {
        for _, val := range row {
            if val == target {
                break search
            }
        }
    }
    return false
    // Nota: en este caso, return directo es aun mejor
}
```

### Caso 5: Donde goto SI es justificable

```go
// ✓ Multiples pasos con cleanup diferenciado
func provision(cfg ProvisionConfig) error {
    vm, err := createVM(cfg)
    if err != nil {
        return fmt.Errorf("create VM: %w", err)
    }

    err = attachStorage(vm, cfg.DiskSize)
    if err != nil {
        goto destroyVM
    }

    err = configureNetwork(vm, cfg.Network)
    if err != nil {
        goto detachStorage
    }

    err = installOS(vm, cfg.Image)
    if err != nil {
        goto removeNetwork
    }

    err = startVM(vm)
    if err != nil {
        goto removeOS
    }

    log.Printf("VM %s provisioned successfully", vm.ID)
    return nil

removeOS:
    cleanupOS(vm)
removeNetwork:
    removeNetworkConfig(vm)
detachStorage:
    detachStorageDevice(vm)
destroyVM:
    destroyVM(vm)
    return fmt.Errorf("provision failed: %w", err)
}
```

### Decision tree

```
┌──────────────────────────────────────────────────────────────┐
│  ¿Necesito salir de un loop/switch anidado?                  │
│  └─ Si → break label                                        │
│                                                              │
│  ¿Necesito ir a la siguiente iteracion de un loop externo?   │
│  └─ Si → continue label                                     │
│                                                              │
│  ¿Necesito cerrar/liberar recursos?                          │
│  └─ Si, simple → defer                                       │
│  └─ Si, diferenciado por punto de fallo → considerar goto    │
│                                                              │
│  ¿Estoy generando codigo automaticamente?                    │
│  └─ Si → goto es aceptable                                  │
│                                                              │
│  ¿Estoy implementando un parser/lexer de bajo nivel?         │
│  └─ Si → goto puede ser mas claro que la alternativa         │
│                                                              │
│  En cualquier otro caso → NO uses goto                       │
└──────────────────────────────────────────────────────────────┘
```

---

## 9. Labels con select (event loops avanzados)

En el contexto SysAdmin, los event loops con `select` dentro de `for` son muy comunes. Labels son esenciales aqui:

```go
func monitorServices(ctx context.Context, services []Service) {
    ticker := time.NewTicker(10 * time.Second)
    defer ticker.Stop()

    healthCh := make(chan HealthEvent, 100)
    alertCh := make(chan Alert, 10)

    // Lanzar health checkers
    for _, svc := range services {
        go svc.StartHealthCheck(ctx, healthCh)
    }

loop:
    for {
        select {
        case <-ctx.Done():
            log.Println("monitor: shutting down")
            break loop

        case event := <-healthCh:
            switch event.Status {
            case StatusDown:
                log.Printf("ALERT: %s is DOWN", event.Service)
                alertCh <- Alert{Service: event.Service, Level: Critical}
                // No break loop aqui — seguir monitoreando
            case StatusDegraded:
                log.Printf("WARN: %s is degraded", event.Service)
            case StatusUp:
                log.Printf("OK: %s is healthy", event.Service)
            }

        case alert := <-alertCh:
            sendPagerDuty(alert)

        case <-ticker.C:
            printStatusSummary(services)
        }
    }

    // Cleanup despues del loop
    close(alertCh)
    log.Println("monitor: shutdown complete")
}
```

### Triple nesting: for + select + switch

```go
func processEvents(ctx context.Context, eventCh <-chan Event) error {
    var errorCount int

mainLoop:
    for {
        select {
        case <-ctx.Done():
            break mainLoop

        case event, ok := <-eventCh:
            if !ok {
                break mainLoop  // channel cerrado
            }

            switch event.Type {
            case EventCritical:
                if errorCount > 10 {
                    log.Println("too many errors, stopping")
                    break mainLoop  // sale del FOR (no del switch ni del select)
                }
                errorCount++
                handleCritical(event)

            case EventWarning:
                handleWarning(event)

            case EventInfo:
                // no hacer nada para info
                continue mainLoop  // siguiente iteracion del for
            }

            // Este codigo se ejecuta para Critical y Warning, NO para Info
            logEvent(event)
        }
    }

    return nil
}
```

---

## 10. Labels y goroutines

Los labels son locales a la funcion — no puedes usarlos para controlar goroutines. Pero puedes usarlos dentro de una goroutine:

```go
// Labels dentro de una goroutine — independientes del parent
func startWorker(ctx context.Context, jobs <-chan Job) {
    go func() {
    loop:
        for {
            select {
            case <-ctx.Done():
                break loop  // sale del for de esta goroutine
            case job, ok := <-jobs:
                if !ok {
                    break loop  // channel cerrado
                }
                process(job)
            }
        }
        log.Println("worker stopped")
    }()
}

// El label "loop" dentro de la goroutine NO interfiere
// con un label "loop" en la funcion parent — diferentes scopes
```

### Patron: Pool de workers con graceful shutdown

```go
func runWorkerPool(ctx context.Context, numWorkers int, jobs <-chan Job) {
    var wg sync.WaitGroup

    for i := 0; i < numWorkers; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            log.Printf("worker %d started", id)

        workerLoop:
            for {
                select {
                case <-ctx.Done():
                    break workerLoop

                case job, ok := <-jobs:
                    if !ok {
                        break workerLoop
                    }
                    log.Printf("worker %d processing: %s", id, job.Name)

                    if err := job.Execute(ctx); err != nil {
                        log.Printf("worker %d error: %v", id, err)
                        // continue workerLoop — siguiente job, no crashear
                        continue workerLoop
                    }
                }
            }

            log.Printf("worker %d stopped", id)
        }(i)
    }

    wg.Wait()
    log.Println("all workers stopped")
}
```

---

## 11. Labels en la stdlib de Go — Ejemplos reales

### net/http — Servidor HTTP (simplificado)

```go
// Patron del servidor HTTP de Go para leer requests
func (c *conn) serve(ctx context.Context) {
    defer c.close()

    for {
        w, err := c.readRequest(ctx)
        if err != nil {
            // ... error handling ...
            return
        }

        // Manejar la request en el mismo goroutine
        serverHandler{c.server}.ServeHTTP(w, w.req)

        // Verificar si debemos cerrar la conexion
        if w.closeAfterReply {
            // break no necesita label aqui porque no hay switch/select anidado
            break
        }
    }
}
```

### encoding/json — Scanner con break label

```go
// Patron del scanner JSON (simplificado)
func (d *decodeState) scanWhile(op int) {
    var newOp int
scan:
    for {
        if d.off >= len(d.data) {
            newOp = d.scan.eof()
            d.off = len(d.data) + 1  // mark eof
        } else {
            c := d.data[d.off]
            d.off++
            newOp = d.scan.step(c)
        }

        if newOp != op {
            break scan
        }
    }
    d.opcode = newOp
}
```

---

## 12. Anti-patrones con labels y goto

### Anti-patron 1: Spaghetti code con multiples gotos

```go
// ✗ TERRIBLE — ilegible, imposible de mantener
func process(data []byte) error {
    if len(data) == 0 {
        goto empty
    }
    if data[0] == '{' {
        goto jsonParse
    }
    if data[0] == '<' {
        goto xmlParse
    }
    goto unknown

jsonParse:
    // ...
    if err != nil {
        goto errorHandle
    }
    goto success

xmlParse:
    // ...
    if err != nil {
        goto errorHandle
    }
    goto success

empty:
    return errors.New("empty input")

unknown:
    return errors.New("unknown format")

errorHandle:
    return fmt.Errorf("parse error: %w", err)

success:
    return nil
}

// ✓ CORRECTO — funciones y switch
func process(data []byte) error {
    if len(data) == 0 {
        return errors.New("empty input")
    }

    switch data[0] {
    case '{':
        return parseJSON(data)
    case '<':
        return parseXML(data)
    default:
        return errors.New("unknown format")
    }
}
```

### Anti-patron 2: goto para simular excepciones

```go
// ✗ NO hagas esto — no es Java/Python
func doStuff() error {
    val, err := step1()
    if err != nil {
        goto catch
    }
    val2, err := step2(val)
    if err != nil {
        goto catch
    }
    err = step3(val2)
    if err != nil {
        goto catch
    }
    return nil

catch:
    log.Printf("ERROR: %v", err)
    return err
}

// ✓ Idiomatico — early return
func doStuff() error {
    val, err := step1()
    if err != nil {
        return fmt.Errorf("step1: %w", err)
    }
    val2, err := step2(val)
    if err != nil {
        return fmt.Errorf("step2: %w", err)
    }
    if err := step3(val2); err != nil {
        return fmt.Errorf("step3: %w", err)
    }
    return nil
}
```

### Anti-patron 3: Labels innecesarios

```go
// ✗ Label innecesario — break sin label hace lo mismo
loop:
    for i := 0; i < 10; i++ {
        if i == 5 {
            break loop  // break sin label tendria el mismo efecto
        }
    }

// ✓ Sin label — mas limpio
for i := 0; i < 10; i++ {
    if i == 5 {
        break
    }
}

// Los labels SOLO son necesarios cuando hay anidamiento:
// for + for, for + switch, for + select
```

---

## 13. Interaccion entre labels, defer y panic

Los labels interactuan con `defer` y `panic` de formas especificas:

### defer se ejecuta aunque salgas con break label

```go
func process() {
    f, err := os.Open("config.yaml")
    if err != nil {
        return
    }
    defer f.Close()  // se ejecuta SIEMPRE que la funcion retorne

loop:
    for {
        select {
        case <-done:
            break loop  // defer se ejecuta al salir de la funcion, no del loop
        case data := <-ch:
            processData(data)
        }
    }

    // break loop trae la ejecucion aqui
    fmt.Println("loop ended")
    // cuando la funcion retorne (al final o con return), defer se ejecuta
}
```

### goto no salta sobre defers — defer es por funcion, no por linea

```go
func example() {
    f, _ := os.Open("file.txt")
    defer f.Close()  // registrado

    goto skip
    // El defer ya se registro — no importa si saltamos sobre otras lineas
    fmt.Println("this is skipped")

skip:
    fmt.Println("reached skip")
    // cuando example() retorne, f.Close() se ejecuta
}
```

### goto no puede saltar sobre declaraciones, pero si sobre defers existentes

```go
func example() {
    var f *os.File

    goto after
    // Esto no es una declaracion (:=), es una asignacion
    f, _ = os.Open("file.txt")  // se salta
    defer f.Close()              // ¡este defer nunca se registra!
after:
    fmt.Println("f is nil:", f == nil)  // true
    // f.Close() NO se llama — el defer nunca se registro
}

// Moraleja: ten cuidado con goto saltando sobre defers
// Si necesitas defer, asegurate de que se registre antes del goto
```

---

## 14. Patron SysAdmin: Inicializacion con multiple cleanup

El patron de inicializacion secuencial con cleanup inverso es donde goto brilla en codigo SysAdmin real:

```go
type InfraStack struct {
    DB       *sql.DB
    Cache    *redis.Client
    MQ       *amqp.Connection
    Listener net.Listener
}

func setupInfrastructure(cfg Config) (*InfraStack, error) {
    stack := &InfraStack{}

    // Paso 1: Base de datos
    var err error
    stack.DB, err = sql.Open("postgres", cfg.DatabaseURL)
    if err != nil {
        return nil, fmt.Errorf("database connection: %w", err)
    }
    if err = stack.DB.Ping(); err != nil {
        goto closeDB
    }
    log.Println("✓ Database connected")

    // Paso 2: Cache
    stack.Cache = redis.NewClient(&redis.Options{Addr: cfg.RedisAddr})
    if err = stack.Cache.Ping(context.Background()).Err(); err != nil {
        goto closeDB
    }
    log.Println("✓ Cache connected")

    // Paso 3: Message Queue
    stack.MQ, err = amqp.Dial(cfg.AMQPURL)
    if err != nil {
        goto closeCache
    }
    log.Println("✓ Message queue connected")

    // Paso 4: Listener
    stack.Listener, err = net.Listen("tcp", cfg.ListenAddr)
    if err != nil {
        goto closeMQ
    }
    log.Printf("✓ Listening on %s", cfg.ListenAddr)

    return stack, nil

    // Cleanup inverso — cada label cierra lo que se abrio hasta ese punto
closeMQ:
    stack.MQ.Close()
closeCache:
    stack.Cache.Close()
closeDB:
    stack.DB.Close()
    return nil, fmt.Errorf("infrastructure setup failed: %w", err)
}
```

Comparando con la alternativa usando defer:

```go
// Alternativa con defer — tambien valida pero diferente semantica
func setupInfrastructure(cfg Config) (*InfraStack, error) {
    stack := &InfraStack{}
    var cleanups []func()

    cleanup := func() {
        for i := len(cleanups) - 1; i >= 0; i-- {
            cleanups[i]()
        }
    }

    var err error
    stack.DB, err = sql.Open("postgres", cfg.DatabaseURL)
    if err != nil {
        return nil, fmt.Errorf("database: %w", err)
    }
    cleanups = append(cleanups, func() { stack.DB.Close() })

    if err = stack.DB.Ping(); err != nil {
        cleanup()
        return nil, fmt.Errorf("database ping: %w", err)
    }

    stack.Cache = redis.NewClient(&redis.Options{Addr: cfg.RedisAddr})
    if err = stack.Cache.Ping(context.Background()).Err(); err != nil {
        cleanup()
        return nil, fmt.Errorf("cache: %w", err)
    }
    cleanups = append(cleanups, func() { stack.Cache.Close() })

    // ... mas pasos ...

    // Exito — NO llamar cleanup
    return stack, nil
}
// La version con goto es mas directa para este patron especifico
```

---

## 15. Patron: Retry con backoff y goto

```go
func connectWithRetry(addr string, maxAttempts int) (net.Conn, error) {
    var (
        conn     net.Conn
        err      error
        attempt  int
        backoff  = time.Second
    )

retry:
    attempt++
    conn, err = net.DialTimeout("tcp", addr, 5*time.Second)
    if err != nil {
        if attempt >= maxAttempts {
            return nil, fmt.Errorf("failed after %d attempts: %w", attempt, err)
        }

        log.Printf("attempt %d/%d failed: %v — retrying in %s",
            attempt, maxAttempts, err, backoff)
        time.Sleep(backoff)

        backoff *= 2
        if backoff > 30*time.Second {
            backoff = 30 * time.Second
        }

        goto retry
    }

    log.Printf("connected to %s on attempt %d", addr, attempt)
    return conn, nil
}
```

Comparando con un for — generalmente el for es preferido:

```go
// ✓ Preferido — for es mas idiomatico para retry
func connectWithRetry(addr string, maxAttempts int) (net.Conn, error) {
    backoff := time.Second

    for attempt := 1; attempt <= maxAttempts; attempt++ {
        conn, err := net.DialTimeout("tcp", addr, 5*time.Second)
        if err == nil {
            log.Printf("connected to %s on attempt %d", addr, attempt)
            return conn, nil
        }

        if attempt == maxAttempts {
            return nil, fmt.Errorf("failed after %d attempts: %w", attempt, err)
        }

        log.Printf("attempt %d/%d failed: %v — retrying in %s",
            attempt, maxAttempts, err, backoff)
        time.Sleep(backoff)

        backoff *= 2
        if backoff > 30*time.Second {
            backoff = 30 * time.Second
        }
    }

    return nil, errors.New("unreachable")
}
```

---

## 16. Ejemplo completo: Pipeline de deploy con labels

```go
package main

import (
    "context"
    "errors"
    "fmt"
    "log"
    "math/rand"
    "net"
    "os"
    "os/signal"
    "strings"
    "sync"
    "syscall"
    "time"
)

// ── Tipos ──────────────────────────────────────────────────────

type Server struct {
    Host     string
    Port     int
    Role     string  // "web", "api", "worker"
    Healthy  bool
}

type DeployResult struct {
    Server  Server
    Success bool
    Message string
}

// ── Health check con break label ───────────────────────────────

func healthCheckAll(servers []Server, timeout time.Duration) map[string]bool {
    results := make(map[string]bool)
    var mu sync.Mutex
    var wg sync.WaitGroup

    for _, srv := range servers {
        wg.Add(1)
        go func(s Server) {
            defer wg.Done()
            addr := fmt.Sprintf("%s:%d", s.Host, s.Port)
            conn, err := net.DialTimeout("tcp", addr, timeout)
            healthy := err == nil
            if healthy {
                conn.Close()
            }

            mu.Lock()
            results[addr] = healthy
            mu.Unlock()
        }(srv)
    }

    wg.Wait()
    return results
}

// ── Deploy con continue label ──────────────────────────────────

func deployToServers(ctx context.Context, servers []Server, artifact string) []DeployResult {
    var results []DeployResult

nextServer:
    for _, srv := range servers {
        // Verificar contexto antes de cada deploy
        select {
        case <-ctx.Done():
            results = append(results, DeployResult{
                Server:  srv,
                Success: false,
                Message: "cancelled: " + ctx.Err().Error(),
            })
            continue nextServer
        default:
        }

        log.Printf("[deploy] Starting deploy to %s (%s)...", srv.Host, srv.Role)

        // Paso 1: Pre-flight check
        if !srv.Healthy {
            log.Printf("[deploy] SKIP %s — marked unhealthy", srv.Host)
            results = append(results, DeployResult{
                Server:  srv,
                Success: false,
                Message: "skipped: server unhealthy",
            })
            continue nextServer
        }

        // Paso 2: Transfer artifact
        log.Printf("[deploy] Transferring %s to %s...", artifact, srv.Host)
        if err := transferArtifact(srv, artifact); err != nil {
            log.Printf("[deploy] FAIL %s — transfer failed: %v", srv.Host, err)
            results = append(results, DeployResult{
                Server:  srv,
                Success: false,
                Message: fmt.Sprintf("transfer failed: %v", err),
            })
            continue nextServer  // no intentar activar si la transferencia fallo
        }

        // Paso 3: Activate
        log.Printf("[deploy] Activating on %s...", srv.Host)
        if err := activateArtifact(srv, artifact); err != nil {
            log.Printf("[deploy] FAIL %s — activation failed: %v", srv.Host, err)
            // Intentar rollback
            if rbErr := rollback(srv); rbErr != nil {
                log.Printf("[deploy] CRITICAL %s — rollback also failed: %v", srv.Host, rbErr)
            }
            results = append(results, DeployResult{
                Server:  srv,
                Success: false,
                Message: fmt.Sprintf("activation failed: %v", err),
            })
            continue nextServer
        }

        // Paso 4: Verify
        log.Printf("[deploy] Verifying %s...", srv.Host)
        if err := verifyDeploy(srv); err != nil {
            log.Printf("[deploy] FAIL %s — verification failed: %v", srv.Host, err)
            _ = rollback(srv)
            results = append(results, DeployResult{
                Server:  srv,
                Success: false,
                Message: fmt.Sprintf("verification failed: %v", err),
            })
            continue nextServer
        }

        log.Printf("[deploy] OK %s — deploy successful", srv.Host)
        results = append(results, DeployResult{
            Server:  srv,
            Success: true,
            Message: "deployed successfully",
        })
    }

    return results
}

// ── Rolling deploy con break label ─────────────────────────────

func rollingDeploy(ctx context.Context, servers []Server, artifact string, maxFailures int) error {
    failures := 0

deploy:
    for i, srv := range servers {
        select {
        case <-ctx.Done():
            return fmt.Errorf("deploy cancelled at server %d/%d: %w", i+1, len(servers), ctx.Err())
        default:
        }

        log.Printf("[rolling %d/%d] Deploying to %s (%s)...", i+1, len(servers), srv.Host, srv.Role)

        result := deployToServers(ctx, []Server{srv}, artifact)
        if len(result) > 0 && !result[0].Success {
            failures++
            log.Printf("[rolling] Failure %d/%d — %s: %s", failures, maxFailures, srv.Host, result[0].Message)

            if failures >= maxFailures {
                log.Printf("[rolling] ABORT — max failures (%d) reached", maxFailures)
                break deploy
            }
            continue deploy
        }

        // Esperar entre deploys para detectar problemas
        if i < len(servers)-1 {
            log.Printf("[rolling] Waiting 5s before next server...")
            select {
            case <-ctx.Done():
                break deploy
            case <-time.After(5 * time.Second):
            }
        }
    }

    if failures > 0 {
        return fmt.Errorf("deploy completed with %d failures", failures)
    }
    return nil
}

// ── Event loop con label ───────────────────────────────────────

func monitorDeploy(ctx context.Context, servers []Server) {
    ticker := time.NewTicker(10 * time.Second)
    defer ticker.Stop()

    alertCh := make(chan string, 10)
    var consecutiveFailures int

monitor:
    for {
        select {
        case <-ctx.Done():
            log.Println("[monitor] Shutting down")
            break monitor

        case alert := <-alertCh:
            log.Printf("[monitor] ALERT: %s", alert)
            consecutiveFailures++
            if consecutiveFailures >= 3 {
                log.Println("[monitor] Too many consecutive failures — escalating")
                // En produccion: enviar a PagerDuty, Slack, etc.
                break monitor
            }

        case <-ticker.C:
            health := healthCheckAll(servers, 3*time.Second)
            allHealthy := true

            for addr, ok := range health {
                if !ok {
                    allHealthy = false
                    alertCh <- fmt.Sprintf("%s is unreachable", addr)
                }
            }

            if allHealthy {
                consecutiveFailures = 0  // reset on success
                log.Printf("[monitor] All %d servers healthy", len(servers))
            }
        }
    }

    close(alertCh)
}

// ── Funciones simuladas ────────────────────────────────────────

func transferArtifact(srv Server, artifact string) error {
    time.Sleep(100 * time.Millisecond)
    if rand.Float64() < 0.1 {
        return errors.New("connection reset during transfer")
    }
    return nil
}

func activateArtifact(srv Server, artifact string) error {
    time.Sleep(50 * time.Millisecond)
    if rand.Float64() < 0.05 {
        return errors.New("service failed to start")
    }
    return nil
}

func verifyDeploy(srv Server) error {
    time.Sleep(200 * time.Millisecond)
    if rand.Float64() < 0.05 {
        return errors.New("health endpoint returned 503")
    }
    return nil
}

func rollback(srv Server) error {
    log.Printf("[rollback] Rolling back %s...", srv.Host)
    time.Sleep(100 * time.Millisecond)
    return nil
}

// ── Main ───────────────────────────────────────────────────────

func main() {
    // Configuracion
    servers := []Server{
        {"web-1.prod", 80, "web", true},
        {"web-2.prod", 80, "web", true},
        {"api-1.prod", 8080, "api", true},
        {"api-2.prod", 8080, "api", true},
        {"worker-1.prod", 9090, "worker", true},
    }

    artifact := "myapp-v2.3.1.tar.gz"
    maxFailures := 2

    // Signal handling
    ctx, stop := signal.NotifyContext(context.Background(),
        syscall.SIGINT, syscall.SIGTERM)
    defer stop()

    // Banner
    fmt.Println(strings.Repeat("═", 60))
    fmt.Printf("  Rolling Deploy: %s\n", artifact)
    fmt.Printf("  Servers: %d | Max failures: %d\n", len(servers), maxFailures)
    fmt.Println(strings.Repeat("═", 60))

    // Pre-flight: health check
    fmt.Println("\n[pre-flight] Checking server health...")
    health := healthCheckAll(servers, 3*time.Second)
    for i := range servers {
        addr := fmt.Sprintf("%s:%d", servers[i].Host, servers[i].Port)
        if !health[addr] {
            servers[i].Healthy = false
            fmt.Printf("  ✗ %s — unreachable\n", addr)
        } else {
            fmt.Printf("  ✓ %s — healthy\n", addr)
        }
    }

    // Launch monitor in background
    monitorCtx, monitorCancel := context.WithCancel(ctx)
    go monitorDeploy(monitorCtx, servers)

    // Deploy
    fmt.Println("\n[deploy] Starting rolling deploy...")
    err := rollingDeploy(ctx, servers, artifact, maxFailures)

    // Stop monitor
    monitorCancel()
    time.Sleep(100 * time.Millisecond) // dar tiempo al monitor para cleanup

    // Report
    fmt.Println()
    fmt.Println(strings.Repeat("═", 60))
    if err != nil {
        fmt.Printf("  RESULT: FAILED — %v\n", err)
        fmt.Println(strings.Repeat("═", 60))
        os.Exit(1)
    }
    fmt.Println("  RESULT: SUCCESS — all servers deployed")
    fmt.Println(strings.Repeat("═", 60))
}
```

---

## 17. Comparacion: Go vs C vs Rust

| Aspecto | Go | C | Rust |
|---|---|---|---|
| Labels | `name:` | `name:` | `'name:` (lifetime syntax) |
| goto | Si, con restricciones | Si, sin restricciones | **No existe** |
| break con label | `break name` | No (goto) | `break 'name` |
| continue con label | `continue name` | No (goto) | `continue 'name` |
| Scope de labels | Funcion | Funcion | Bloque/loop |
| Saltar sobre decl. | Prohibido | Permitido (UB posible) | N/A |
| Saltar a bloque | Prohibido | Permitido (UB posible) | N/A |
| Label no usado | Error de compilacion | Warning | Error de compilacion |
| Label en for | Si | N/A | Si (obligatorio con `'`) |
| Label en switch | Si | N/A | N/A (match no necesita) |
| Label en select | Si | N/A | N/A |

### Equivalencias

```go
// Go — break con label
outer:
    for i := 0; i < 10; i++ {
        for j := 0; j < 10; j++ {
            if i*j > 50 {
                break outer
            }
        }
    }
```

```c
// C — solo tiene goto
for (int i = 0; i < 10; i++) {
    for (int j = 0; j < 10; j++) {
        if (i * j > 50) {
            goto done;
        }
    }
}
done:
    ;  // label necesita un statement en C
```

```rust
// Rust — labels con lifetime syntax, sin goto
'outer: for i in 0..10 {
    for j in 0..10 {
        if i * j > 50 {
            break 'outer;
        }
    }
}
```

### Rust elimino goto intencionalmente

```
┌──────────────────────────────────────────────────────────────┐
│  Rust NO tiene goto — por diseno                             │
│                                                              │
│  Filosofia: si necesitas goto, hay una abstraccion mejor     │
│                                                              │
│  Equivalencias en Rust:                                      │
│  ├─ goto cleanup → Drop trait (RAII automatico)              │
│  ├─ goto retry   → loop { match try_op() { ... } }          │
│  ├─ goto done    → break 'label value                        │
│  ├─ goto state   → enum State + loop { match state { } }    │
│  └─ goto error   → ? operator + Result<T, E>                 │
│                                                              │
│  Go mantiene goto porque:                                    │
│  ├─ Compatibilidad con patrones de C (su herencia)           │
│  ├─ Code generation (parsers, lexers)                        │
│  └─ Pragmatismo: a veces es la solucion mas simple           │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 18. Linting y herramientas

### golangci-lint: detectar goto sospechoso

```yaml
# .golangci.yml
linters:
  enable:
    - gocritic

linters-settings:
  gocritic:
    enabled-checks:
      # No hay un linter especifico para goto,
      # pero gocritic detecta patrones sospechosos
      - unnecessaryBlock
      - unnamedResult

# Si quieres prohibir goto completamente en tu proyecto:
# Usa un linter custom o una regla de code review
# Nota: la stdlib lo usa — prohibirlo es una decision de equipo
```

### go vet: detecta labels no usados

```bash
# go vet detecta labels declarados pero no usados
# (el compilador tambien lo hace — es un error de compilacion)

$ go vet ./...
# Si tienes un label sin usar, no compila, asi que go vet
# no necesita verificarlo por separado

# Ejemplo:
func f() {
    unused:           // error de compilacion: label unused defined and not used
        for {}
}
```

### staticcheck y labels

```bash
# staticcheck puede detectar:
# - Labels innecesarios (break label cuando break simple basta)
# - Patrones que serian mas claros sin goto

$ staticcheck ./...
```

---

## 19. Errores comunes

| # | Error | Codigo | Solucion |
|---|---|---|---|
| 1 | Label no usado | `myLabel:` sin referencia | Eliminar el label o usarlo |
| 2 | break label en non-breakable | `break l` donde `l:` no es for/switch/select | Poner el label en el for/switch/select correcto |
| 3 | continue label en non-for | `continue l` donde `l:` es switch | continue solo funciona con for |
| 4 | goto sobre declaracion | `goto end` saltando `x := 5` | Usar `var x int` antes del goto |
| 5 | goto al interior de bloque | `goto inside` donde inside esta en un if/for | Reestructurar el flujo |
| 6 | Label duplicado | Dos labels con el mismo nombre en una funcion | Usar nombres unicos |
| 7 | Confundir break de switch con break de for | `break` en switch dentro de for | Usar `break label` en el for |
| 8 | goto entre funciones | `goto` a un label en otra funcion | Imposible — los labels son locales |
| 9 | goto como control de flujo principal | Multiples gotos en lugar de if/for/switch | Reestructurar con constructos normales |
| 10 | Label sin indentar | `label:` con indentacion confusa | Convencion: labels al nivel de la funcion o un nivel menos que el for |

### Error 4 en detalle

```go
// ✗ No compila
func f() {
    goto skip
    name := "test"    // declaracion con :=
    fmt.Println(name)
skip:
    fmt.Println("done")
}
// Error: goto skip jumps over declaration of name

// ✓ Declarar antes del goto
func f() {
    var name string   // declaracion sin inicializacion
    goto skip
    name = "test"     // asignacion (no declaracion)
    fmt.Println(name)
skip:
    fmt.Println("done", name)  // name = "" (zero value)
}

// ✓ O mejor: reestructurar para no necesitar goto
func f() {
    name := "test"
    // ... usar name ...
    fmt.Println("done")
}
```

### Convencion de indentacion de labels

```go
// La convencion en la stdlib es: labels al nivel de la funcion
// o un nivel menos que el for que etiquetan

func processItems(items [][]Item) {
outer:                           // ← un nivel menos que el for
    for _, group := range items {
        for _, item := range group {
            if item.Invalid {
                continue outer
            }
            process(item)
        }
    }
}

// Algunos equipos prefieren labels al margen izquierdo:
func processItems(items [][]Item) {
Loop:
for _, group := range items {
        for _, item := range group {
            // ...
            break Loop
        }
    }
}
// gofmt NO ajusta la indentacion de labels — es decision del equipo
```

---

## 20. Ejercicios

### Ejercicio 1: Prediccion de break label
```go
package main

import "fmt"

func main() {
    matrix := [][]int{
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9},
    }

outer:
    for i, row := range matrix {
        for j, val := range row {
            fmt.Printf("checking [%d][%d]=%d\n", i, j, val)
            if val == 5 {
                break outer
            }
        }
    }
    fmt.Println("done")
}
```
**Prediccion**: ¿Cuantas veces se imprime "checking..."? ¿Se imprime "done"? ¿Que pasa si cambias `break outer` por `break`?

### Ejercicio 2: Prediccion de continue label
```go
package main

import "fmt"

func main() {
    words := [][]string{
        {"hello", "skip", "world"},
        {"foo", "bar"},
        {"skip", "baz", "qux"},
    }

outer:
    for _, group := range words {
        for _, word := range group {
            if word == "skip" {
                continue outer
            }
            fmt.Println(word)
        }
        fmt.Println("--- group done ---")
    }
}
```
**Prediccion**: ¿Que palabras se imprimen? ¿Se imprime "--- group done ---" para algun grupo? ¿Para cuales?

### Ejercicio 3: Prediccion de goto con variables
```go
package main

import "fmt"

func main() {
    i := 0

loop:
    if i >= 3 {
        goto end
    }
    fmt.Println(i)
    i++
    goto loop

end:
    fmt.Println("finished at i =", i)
}
```
**Prediccion**: ¿Que imprime? ¿Cuantas veces se ejecuta el cuerpo del "loop"? ¿Compilaria si `i := 0` estuviera entre `goto loop` y `loop:`?

### Ejercicio 4: Prediccion de break label con select
```go
package main

import (
    "fmt"
    "time"
)

func main() {
    ch := make(chan int)
    go func() {
        for i := 0; i < 5; i++ {
            ch <- i
            time.Sleep(100 * time.Millisecond)
        }
        close(ch)
    }()

    count := 0
loop:
    for {
        select {
        case val, ok := <-ch:
            if !ok {
                break loop
            }
            count++
            fmt.Printf("received: %d\n", val)
            if count >= 3 {
                break loop
            }
        }
    }
    fmt.Printf("total received: %d\n", count)
}
```
**Prediccion**: ¿Cuantos valores recibe antes de salir? ¿Se imprime "total received"? ¿Que pasa si usas `break` sin label?

### Ejercicio 5: Prediccion de goto con defer
```go
package main

import "fmt"

func main() {
    fmt.Println("start")
    defer fmt.Println("deferred 1")

    goto skip

    defer fmt.Println("deferred 2")  // ¿se registra?
    fmt.Println("middle")

skip:
    defer fmt.Println("deferred 3")
    fmt.Println("end")
}
```
**Prediccion**: ¿Que defers se ejecutan? ¿En que orden? ¿Que lineas se imprimen y en que secuencia?

### Ejercicio 6: Multi-server health checker
Implementa una funcion que:
1. Reciba una lista de servidores con sus puertos esperados
2. Use `continue label` para saltar al siguiente servidor si el ping falla
3. Dentro de cada servidor, verifique multiples puertos
4. Use `break label` para abortar todo si mas de N servidores fallan
5. Retorne un reporte con el estado de cada servidor

### Ejercicio 7: Config file parser con goto cleanup
Implementa un parser que:
1. Abra un archivo de configuracion
2. Lea y valide el header
3. Parsee secciones y claves
4. Si alguna validacion falla en cualquier punto, use goto para ir a un cleanup centralizado que cierre archivos y reporte el error con contexto (numero de linea, seccion)
5. Compara tu implementacion con una version usando defer — ¿cual es mas clara?

### Ejercicio 8: Pipeline con labels
Implementa un pipeline de procesamiento de datos que:
1. Lea archivos CSV de un directorio
2. Para cada archivo, procese cada fila
3. Use `continue outer` para saltar archivos con errores de formato
4. Use `break outer` si encuentra un archivo con datos criticos invalidos
5. Acumule resultados y estadisticas de lo procesado

### Ejercicio 9: Event dispatcher con label + select
Implementa un event dispatcher que:
1. Reciba eventos de multiples channels (logs, metrics, alerts)
2. Use un `for` + `select` con label para el loop principal
3. Cuente eventos por tipo
4. Salga del loop (`break label`) si recibe 3 alertas criticas consecutivas
5. Use `continue label` para saltar processing de eventos con timestamp en el futuro

### Ejercicio 10: Comparacion goto vs defer
Implementa la MISMA funcionalidad dos veces:
1. Una funcion que abre 4 recursos en secuencia (archivo, conexion TCP, base de datos, cache). Si cualquier paso falla, cierra los recursos abiertos en orden inverso. Implementa con goto.
2. La misma funcion, implementada con defer.
3. Compara: lineas de codigo, legibilidad, orden de cleanup, manejo de errores.

---

## 21. Resumen

```
┌──────────────────────────────────────────────────────────────┐
│              LABELS Y GOTO EN GO — RESUMEN                    │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  LABELS                                                      │
│  ├─ Sintaxis: NombreLabel:                                   │
│  ├─ Scope: local a la funcion                                │
│  ├─ DEBEN usarse — label sin usar = error de compilacion     │
│  └─ Convencion: PascalCase o camelCase (outer, loop, retry)  │
│                                                              │
│  break label                                                 │
│  ├─ Sale de un for, switch, o select marcado con el label    │
│  ├─ Uso principal: salir de for desde switch o select         │
│  └─ Esencial en event loops (for + select)                   │
│                                                              │
│  continue label                                              │
│  ├─ Salta a la siguiente iteracion de un for externo         │
│  ├─ SOLO funciona con for (no switch, no select)             │
│  └─ Util en loops anidados para saltar al nivel exterior     │
│                                                              │
│  goto                                                        │
│  ├─ Salto incondicional a un label en la misma funcion       │
│  ├─ Restricciones (compile time):                            │
│  │   ├─ No puede saltar sobre declaraciones de variables     │
│  │   ├─ No puede saltar al interior de bloques               │
│  │   └─ No puede saltar entre funciones                      │
│  ├─ Usos legitimos:                                          │
│  │   ├─ Cleanup centralizado diferenciado                    │
│  │   ├─ Code generation (parsers, lexers)                    │
│  │   └─ State machines de bajo nivel                         │
│  └─ En cualquier otro caso: NO usar goto                     │
│                                                              │
│  SYSADMIN                                                    │
│  ├─ break label: event loops (for + select), deploy abort    │
│  ├─ continue label: saltar servidores/archivos fallidos      │
│  ├─ goto: inicializacion de infraestructura con cleanup      │
│  └─ Labels en goroutines: independientes del parent          │
│                                                              │
│  REGLA DE ORO                                                │
│  ├─ break/continue label → usalo cuando lo necesites         │
│  ├─ goto → casi nunca lo necesitas                           │
│  └─ Si goto hace el codigo MAS claro, usalo con comentario   │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: S02/T01 - defer — LIFO, evaluacion de argumentos, defer en loops (¡cuidado!), patrones de cleanup
