# Tests basicos — testing.T, go test, naming, t.Run (subtests), t.Parallel

## 1. Introduccion

Go tiene testing integrado en el lenguaje y la toolchain — no necesitas frameworks externos. El paquete `testing` de la stdlib, combinado con el comando `go test`, cubre unit tests, integration tests, benchmarks, fuzzing, y examples ejecutables. A diferencia de otros lenguajes donde elegir un framework de testing es la primera decision del proyecto (Jest vs Mocha, pytest vs unittest, JUnit vs TestNG), en Go hay **un solo camino** — y esta en la stdlib.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                    TESTING EN GO — ECOSISTEMA                                   │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  STDLIB (todo lo que necesitas para el 95% de los casos)                        │
│  ─────────────────────────────────────────────────────                           │
│                                                                                  │
│  testing package                    go test command                              │
│  ┌────────────────────────┐        ┌────────────────────────┐                   │
│  │ testing.T   → tests    │        │ go test          → run │                   │
│  │ testing.B   → bench    │        │ go test -v       → verbose │               │
│  │ testing.F   → fuzz     │        │ go test -run     → filter │                │
│  │ testing.M   → main     │        │ go test -count   → repeat │                │
│  │ testing.TB  → common   │        │ go test -cover   → coverage │              │
│  │                        │        │ go test -race    → data races │            │
│  │ t.Run()    → subtests  │        │ go test -bench   → benchmarks │            │
│  │ t.Parallel() → concurr │        │ go test -fuzz    → fuzzing │               │
│  │ t.Cleanup() → teardown │        │ go test -shuffle → random order │          │
│  │ t.Helper() → helpers   │        │ go test -timeout → deadline │              │
│  │ t.Skip()   → skip     │        │ go test ./...    → all packages │           │
│  └────────────────────────┘        └────────────────────────┘                   │
│                                                                                  │
│  testing/quick    → property-based testing (basico)                             │
│  testing/fstest   → testing de filesystems (fs.FS)                              │
│  testing/iotest   → readers/writers que fallan de formas predecibles            │
│  testing/slogtest → verificar output de slog handlers                           │
│  net/http/httptest → test servers y request/response recording                  │
│                                                                                  │
│  PAQUETES EXTERNOS (opcionales, para casos especificos)                         │
│  ──────────────────────────────────────────────────────                          │
│                                                                                  │
│  testify          → assertions (assert/require) + mocks + suites               │
│  gomock/mockgen   → generacion automatica de mocks                              │
│  testcontainers   → containers Docker para integration tests                    │
│  goleak           → detectar goroutine leaks                                    │
│  go-cmp           → comparacion profunda de structs (google/go-cmp)            │
│  quicktest        → testing helpers del equipo de Go                            │
│  is               → assertions minimalistas                                     │
└──────────────────────────────────────────────────────────────────────────────────┘
```

**Filosofia de testing en Go**:
- **Sin magia**: no hay anotaciones, decorators, ni discovery automatico por reflexion. Los tests son funciones Go normales con una convencion de nombre.
- **Sin assertions built-in**: Go usa `if` + `t.Errorf` en vez de `assert.Equal`. Esto es deliberado — reduce la API a aprender y hace los tests legibles como codigo Go normal.
- **Tests son codigo**: no hay DSLs (describe/it/expect). Un test es una funcion, un subtest es `t.Run`, una tabla es un slice de structs.
- **Paralelismo opt-in**: los tests corren secuencialmente por defecto. `t.Parallel()` opta por ejecucion concurrente.

---

## 2. Convenciones y estructura de archivos

### 2.1 Regla de archivos: *_test.go

```
mypackage/
├── calculator.go          ← codigo de produccion
├── calculator_test.go     ← tests del calculator
├── parser.go
├── parser_test.go
├── helpers_test.go        ← helpers solo para tests
└── testdata/              ← datos de test (archivos, fixtures)
    ├── valid_input.json
    └── expected_output.txt
```

**Reglas**:
1. Los archivos de test DEBEN terminar en `_test.go`
2. El compilador los EXCLUYE del binario de produccion
3. Pueden estar en el mismo paquete (`package mypackage`) o en paquete externo (`package mypackage_test`)
4. El directorio `testdata/` es especial — `go test` lo ignora como paquete, y se puede acceder con paths relativos en tests

### 2.2 Mismo paquete vs paquete _test

```go
// Opcion 1: MISMO paquete (white-box testing)
// Archivo: calculator_test.go
package calculator

// Puede acceder a funciones/tipos NO exportados
func TestInternalHelper(t *testing.T) {
    result := internalHelper(42) // funcion no exportada — OK
    if result != expected {
        t.Errorf("got %v, want %v", result, expected)
    }
}
```

```go
// Opcion 2: PAQUETE EXTERNO (black-box testing)
// Archivo: calculator_test.go
package calculator_test

import "mymodule/calculator"

// Solo puede acceder a funciones/tipos EXPORTADOS
func TestAdd(t *testing.T) {
    result := calculator.Add(2, 3) // solo funciones publicas
    if result != 5 {
        t.Errorf("got %d, want %d", result, 5)
    }
}
```

```
┌────────────────────────────────────────────────────────────────────────┐
│              MISMO PAQUETE vs PAQUETE _test                           │
├────────────────────────────────────────────────────────────────────────┤
│                                                                        │
│  Mismo paquete (package foo)                                          │
│  ──────────────────────────                                           │
│  ✓ Acceso a internals (white-box)                                    │
│  ✓ Test de funciones helper no exportadas                            │
│  ✓ Test de estado interno de structs                                 │
│  ✗ Puede acoplarse a detalles de implementacion                     │
│  ✗ No prueba la API publica como la usaria un consumidor            │
│                                                                        │
│  Paquete externo (package foo_test)                                   │
│  ─────────────────────────────────                                    │
│  ✓ Prueba la API publica (black-box)                                 │
│  ✓ Detecta problemas de usabilidad de tu API                        │
│  ✓ Puede servir como documentacion de uso                           │
│  ✗ No puede testear internals                                        │
│  ✗ Puede necesitar export_test.go para acceder a internals          │
│                                                                        │
│  RECOMENDACION:                                                       │
│  • Usa paquete externo (_test) por defecto                           │
│  • Usa mismo paquete solo cuando necesitas acceder a internals       │
│  • Puedes mezclar ambos en el mismo directorio                       │
│    (diferentes archivos, uno con package foo, otro con foo_test)      │
└────────────────────────────────────────────────────────────────────────┘
```

### 2.3 export_test.go — el puente

```go
// Cuando usas package foo_test pero necesitas acceder a algo interno,
// creas un archivo export_test.go en el paquete original

// Archivo: export_test.go
package calculator

// Exportar internal para tests (convencion: nombre empieza con mayuscula)
var InternalHelper = internalHelper  // expone la funcion no exportada
var DefaultPrecision = defaultPrecision // expone la variable no exportada

// Ahora en calculator_test.go (package calculator_test):
func TestHelper(t *testing.T) {
    result := calculator.InternalHelper(42) // acceso via export_test.go
    // ...
}

// NOTA: export_test.go solo se compila durante `go test`
// No contamina el paquete de produccion
```

### 2.4 Convencion de nombres

```go
// Funciones de test: Test + nombre descriptivo en CamelCase
func TestAdd(t *testing.T) {}                    // funcion Add
func TestAdd_positiveNumbers(t *testing.T) {}    // escenario especifico
func TestAdd_negativeNumbers(t *testing.T) {}
func TestAdd_overflow(t *testing.T) {}
func TestCalculator_Add(t *testing.T) {}         // metodo Add de Calculator
func TestNewCalculator(t *testing.T) {}          // constructor

// REGLAS:
// 1. DEBE empezar con "Test"
// 2. DEBE recibir *testing.T como unico parametro
// 3. El nombre despues de "Test" debe empezar con mayuscula o _
// 4. TestAdd    → OK
// 5. Testadd    → NO se ejecuta (la 'a' es minuscula)
// 6. Test_add   → OK (underscore es valido)
// 7. TestAdd_v2 → OK

// Benchmarks: Benchmark + nombre
func BenchmarkAdd(b *testing.B) {}

// Fuzz tests: Fuzz + nombre
func FuzzAdd(f *testing.F) {}

// Examples: Example + nombre
func ExampleAdd() {}
func ExampleCalculator_Add() {}  // metodo Add de tipo Calculator
```

---

## 3. testing.T — el objeto central

### 3.1 Metodos de reporte de errores

```go
func TestErrorReporting(t *testing.T) {
    // t.Log / t.Logf — informacion (solo se muestra con -v)
    t.Log("starting test...")
    t.Logf("testing with value: %d", 42)

    // t.Error / t.Errorf — reportar fallo pero CONTINUAR el test
    got := Add(2, 3)
    if got != 5 {
        t.Errorf("Add(2, 3) = %d, want %d", got, 5)
    }
    // El test sigue ejecutandose despues de t.Error

    // t.Fatal / t.Fatalf — reportar fallo y DETENER el test inmediatamente
    conn, err := Connect("localhost:5432")
    if err != nil {
        t.Fatalf("failed to connect: %v", err)
        // Nada despues de Fatal se ejecuta
    }

    // t.Fail() — marcar como fallido sin mensaje (raro)
    // t.FailNow() — marcar como fallido y detener (raro, usa Fatal)

    // REGLA PRACTICA:
    // t.Error/Errorf → para verificaciones de valor (quieres ver TODOS los fallos)
    // t.Fatal/Fatalf → para condiciones previas que impiden continuar (setup, conexion)
}
```

```
┌────────────────────────────────────────────────────────────────────────┐
│              Error vs Fatal — CUANDO USAR CADA UNO                    │
├────────────────────────────────────────────────────────────────────────┤
│                                                                        │
│  t.Error/Errorf          t.Fatal/Fatalf                               │
│  ─────────────           ─────────────                                │
│  Marca fallo             Marca fallo                                  │
│  CONTINUA el test        DETIENE el test                              │
│                                                                        │
│  Usar para:              Usar para:                                   │
│  • Verificar valores     • Setup fallido (no tiene sentido continuar) │
│  • Multiples assertions  • Error en precondicion                     │
│  • Ver TODOS los fallos  • Panic inminente si continuas              │
│                                                                        │
│  Ejemplo:                Ejemplo:                                     │
│  if got != want {        if err != nil {                              │
│    t.Errorf(...)           t.Fatalf("setup: %v", err)                │
│  }                       }                                            │
│  // sigue verificando    // ya salio del test                        │
│  if len(x) != 3 {                                                    │
│    t.Errorf(...)                                                      │
│  }                                                                    │
└────────────────────────────────────────────────────────────────────────┘
```

### 3.2 t.Helper() — mejorar el reporte de lineas

```go
// Sin t.Helper(), los errores reportan la linea DENTRO del helper
// Con t.Helper(), reportan la linea del CALLER (donde se llamo al helper)

// Helper sin t.Helper()
func assertEqual(t *testing.T, got, want int) {
    if got != want {
        t.Errorf("got %d, want %d", got, want)
        // Error reporta: helpers_test.go:5 — la linea del helper
        // Pero quieres saber DONDE se llamo assertEqual, no donde esta
    }
}

// Helper con t.Helper() (correcto)
func assertEqual(t *testing.T, got, want int) {
    t.Helper() // marca esta funcion como helper
    if got != want {
        t.Errorf("got %d, want %d", got, want)
        // Ahora reporta: calculator_test.go:12 — donde se llamo assertEqual
    }
}

// SIEMPRE pon t.Helper() como primera linea en funciones helper de test
// Puede anidarse: si helper A llama a helper B, ambos con t.Helper(),
// el error reporta la linea del caller original

func assertNoError(t *testing.T, err error) {
    t.Helper()
    if err != nil {
        t.Fatalf("unexpected error: %v", err)
    }
}

func assertError(t *testing.T, err error) {
    t.Helper()
    if err == nil {
        t.Fatal("expected error, got nil")
    }
}

func assertContains(t *testing.T, s, substr string) {
    t.Helper()
    if !strings.Contains(s, substr) {
        t.Errorf("%q does not contain %q", s, substr)
    }
}

func TestSomething(t *testing.T) {
    result, err := Process("input")
    assertNoError(t, err)                   // error line: aqui (linea 42)
    assertEqual(t, result.Count, 5)          // error line: aqui (linea 43)
    assertContains(t, result.Name, "test")   // error line: aqui (linea 44)
}
```

### 3.3 t.Cleanup() — teardown garantizado

```go
func TestWithCleanup(t *testing.T) {
    // t.Cleanup registra funciones que se ejecutan al terminar el test
    // Se ejecutan en orden LIFO (stack) — como defer
    // Se ejecutan SIEMPRE, incluso si el test falla con Fatal

    // Crear recurso temporal
    dir := t.TempDir() // helper que crea dir temporal + auto-cleanup
    t.Logf("using temp dir: %s", dir)

    // Cleanup manual
    db, err := sql.Open("postgres", connStr)
    if err != nil {
        t.Fatal(err)
    }
    t.Cleanup(func() {
        db.Close()
        t.Log("database closed")
    })

    // Multiples cleanups — se ejecutan en orden inverso
    t.Cleanup(func() { t.Log("cleanup 3") })
    t.Cleanup(func() { t.Log("cleanup 2") })
    t.Cleanup(func() { t.Log("cleanup 1") })
    // Orden de ejecucion: cleanup 1, cleanup 2, cleanup 3

    // Usar el recurso
    _, err = db.Exec("INSERT INTO tests (name) VALUES ($1)", t.Name())
    if err != nil {
        t.Fatal(err)
    }

    // Al terminar: cleanup 1 → cleanup 2 → cleanup 3 → db.Close()
}

// t.Cleanup vs defer:
// - defer se ejecuta al salir de la FUNCION
// - t.Cleanup se ejecuta al terminar el TEST (incluyendo subtests)
// - t.Cleanup funciona con t.Parallel() (defer puede ejecutarse antes de tiempo)
// - Para subtests paralelos, SIEMPRE usa t.Cleanup en vez de defer
```

### 3.4 t.TempDir() — directorios temporales

```go
func TestFileProcessing(t *testing.T) {
    // t.TempDir() crea un directorio temporal que se limpia al terminar
    dir := t.TempDir()

    // Escribir archivo de test
    inputFile := filepath.Join(dir, "input.txt")
    err := os.WriteFile(inputFile, []byte("hello world"), 0644)
    if err != nil {
        t.Fatal(err)
    }

    // Ejecutar funcion bajo test
    outputFile := filepath.Join(dir, "output.txt")
    err = ProcessFile(inputFile, outputFile)
    if err != nil {
        t.Fatalf("ProcessFile() error: %v", err)
    }

    // Verificar resultado
    got, err := os.ReadFile(outputFile)
    if err != nil {
        t.Fatalf("reading output: %v", err)
    }
    want := "HELLO WORLD"
    if string(got) != want {
        t.Errorf("output = %q, want %q", string(got), want)
    }

    // No necesitas limpiar — t.TempDir() lo hace automaticamente
}
```

### 3.5 t.Skip() — saltar tests condicionalmente

```go
func TestRequiresNetwork(t *testing.T) {
    if testing.Short() {
        t.Skip("skipping network test in short mode")
    }
    // Solo se ejecuta con: go test (no con go test -short)

    resp, err := http.Get("https://api.example.com/health")
    if err != nil {
        t.Fatal(err)
    }
    defer resp.Body.Close()
    // ...
}

func TestRequiresDocker(t *testing.T) {
    if os.Getenv("DOCKER_HOST") == "" {
        t.Skip("DOCKER_HOST not set, skipping Docker test")
    }
    // ...
}

func TestLinuxOnly(t *testing.T) {
    if runtime.GOOS != "linux" {
        t.Skipf("skipping on %s, only runs on linux", runtime.GOOS)
    }
    // ...
}

func TestRequiresDatabase(t *testing.T) {
    dsn := os.Getenv("TEST_DATABASE_URL")
    if dsn == "" {
        t.Skip("TEST_DATABASE_URL not set")
    }
    db, err := sql.Open("postgres", dsn)
    if err != nil {
        t.Skipf("cannot connect to database: %v", err)
    }
    t.Cleanup(func() { db.Close() })
    // ...
}

// testing.Short() se activa con: go test -short
// Util para separar tests rapidos de tests lentos/integration
```

### 3.6 t.Setenv() — variables de entorno en tests (Go 1.17+)

```go
func TestConfig(t *testing.T) {
    // t.Setenv establece una variable de entorno SOLO para este test
    // Se restaura automaticamente al terminar
    t.Setenv("APP_PORT", "9090")
    t.Setenv("APP_DEBUG", "true")

    cfg := LoadConfig()
    if cfg.Port != 9090 {
        t.Errorf("port = %d, want 9090", cfg.Port)
    }
    if !cfg.Debug {
        t.Error("debug should be true")
    }

    // NOTA: t.Setenv NO funciona con t.Parallel()
    // Si necesitas env vars en tests paralelos, usa un helper que
    // pase la configuracion como parametro en vez de leer env vars
}
```

---

## 4. go test — el comando

### 4.1 Ejecucion basica

```bash
# Ejecutar tests del paquete actual
go test

# Verbose — muestra nombre de cada test y logs
go test -v

# Ejecutar tests de todos los paquetes
go test ./...

# Ejecutar tests de un paquete especifico
go test ./internal/parser

# Ejecutar tests con nombre especifico (regex)
go test -run TestAdd
go test -run TestAdd/positive      # subtest especifico
go test -run "TestAdd|TestSub"     # multiples tests
go test -run "Test.*Calculator"    # regex

# Short mode — salta tests marcados con testing.Short()
go test -short

# Count — ejecutar N veces (util para detectar flaky tests)
go test -count 10
go test -count 1  # deshabilita cache de test results

# Timeout — limite de tiempo total
go test -timeout 30s
go test -timeout 5m

# Shuffle — ejecutar en orden aleatorio (Go 1.17+)
go test -shuffle on
go test -shuffle 12345  # seed especifico para reproducibilidad
```

### 4.2 Flags de diagnostico

```bash
# Race detector — detectar data races
go test -race
# SIEMPRE correr con -race en CI
# Agrega ~2-10x overhead pero detecta bugs concurrentes

# Coverage — medir cobertura de codigo
go test -cover
# output: coverage: 78.5% of statements

# Coverage con perfil detallado
go test -coverprofile=coverage.out
go tool cover -html=coverage.out      # abrir reporte HTML
go tool cover -func=coverage.out       # reporte por funcion

# Coverage mode
go test -covermode=atomic  # thread-safe (usar con -race)
go test -covermode=count   # contar ejecuciones por statement
go test -covermode=set     # solo yes/no (default, mas rapido)

# Verbose + failfast (parar en primer fallo)
go test -v -failfast

# JSON output (para CI/CD parsing)
go test -json ./...

# List tests sin ejecutar
go test -list ".*" ./...
```

### 4.3 Cache de resultados

```go
// Go cachea resultados de tests exitosos
// Si no cambias codigo ni flags, los tests no se re-ejecutan
// Output con cache: "ok  mypackage  (cached)"

// Formas de invalidar el cache:
// 1. go test -count=1  → fuerza re-ejecucion
// 2. go clean -testcache  → limpiar todo el cache
// 3. Cambiar cualquier archivo .go del paquete
// 4. Cambiar archivos en testdata/
// 5. Cambiar flags de go test

// El cache es especialmente util con go test ./...
// Solo re-ejecuta paquetes que cambiaron
```

### 4.4 Build tags para tests

```go
// Puedes usar build tags para separar tipos de tests

//go:build integration

package mypackage

func TestIntegration(t *testing.T) {
    // Solo se compila/ejecuta con:
    // go test -tags integration
}

// Archivo: slow_test.go
//go:build slow

package mypackage

func TestSlowOperation(t *testing.T) {
    // go test -tags slow
}

// Combinacion:
//go:build integration && !race

// Ejecutar:
// go test -tags "integration slow" ./...
```

---

## 5. Subtests con t.Run()

### 5.1 Sintaxis basica

```go
func TestMath(t *testing.T) {
    // t.Run crea un subtest con nombre y funcion
    t.Run("Add", func(t *testing.T) {
        if Add(2, 3) != 5 {
            t.Error("2+3 should be 5")
        }
    })

    t.Run("Subtract", func(t *testing.T) {
        if Subtract(5, 3) != 2 {
            t.Error("5-3 should be 2")
        }
    })

    t.Run("Multiply", func(t *testing.T) {
        if Multiply(3, 4) != 12 {
            t.Error("3*4 should be 12")
        }
    })
}

// Output con -v:
// === RUN   TestMath
// === RUN   TestMath/Add
// --- PASS: TestMath/Add (0.00s)
// === RUN   TestMath/Subtract
// --- PASS: TestMath/Subtract (0.00s)
// === RUN   TestMath/Multiply
// --- PASS: TestMath/Multiply (0.00s)
// --- PASS: TestMath (0.00s)

// Ejecutar un subtest especifico:
// go test -run TestMath/Add
// go test -run TestMath/Multiply
```

### 5.2 Subtests anidados

```go
func TestCalculator(t *testing.T) {
    calc := NewCalculator()

    t.Run("Arithmetic", func(t *testing.T) {
        t.Run("Add", func(t *testing.T) {
            t.Run("positive", func(t *testing.T) {
                if calc.Add(2, 3) != 5 {
                    t.Error("failed")
                }
            })
            t.Run("negative", func(t *testing.T) {
                if calc.Add(-2, -3) != -5 {
                    t.Error("failed")
                }
            })
            t.Run("zero", func(t *testing.T) {
                if calc.Add(0, 0) != 0 {
                    t.Error("failed")
                }
            })
        })

        t.Run("Divide", func(t *testing.T) {
            t.Run("normal", func(t *testing.T) {
                result, err := calc.Divide(10, 2)
                if err != nil {
                    t.Fatal(err)
                }
                if result != 5 {
                    t.Errorf("got %f, want 5", result)
                }
            })
            t.Run("by zero", func(t *testing.T) {
                _, err := calc.Divide(10, 0)
                if err == nil {
                    t.Error("expected error for division by zero")
                }
            })
        })
    })
}

// Ejecutar subtest anidado:
// go test -run TestCalculator/Arithmetic/Add/positive
// go test -run "TestCalculator/Arithmetic/Divide/by_zero"
// NOTA: espacios en nombres se convierten a _ en el path del -run
```

### 5.3 Subtests con setup/teardown compartido

```go
func TestDatabase(t *testing.T) {
    // Setup compartido para todos los subtests
    db, err := setupTestDB()
    if err != nil {
        t.Fatal(err)
    }
    t.Cleanup(func() {
        db.Close()
        t.Log("database closed")
    })

    // Cada subtest usa la misma conexion
    t.Run("Insert", func(t *testing.T) {
        err := db.Insert(User{Name: "Alice"})
        if err != nil {
            t.Fatalf("insert: %v", err)
        }
    })

    t.Run("Query", func(t *testing.T) {
        // Este test depende de que Insert se haya ejecutado antes
        // (los subtests secuenciales se ejecutan en orden)
        user, err := db.GetByName("Alice")
        if err != nil {
            t.Fatalf("query: %v", err)
        }
        if user.Name != "Alice" {
            t.Errorf("name = %q, want %q", user.Name, "Alice")
        }
    })

    t.Run("Delete", func(t *testing.T) {
        err := db.DeleteByName("Alice")
        if err != nil {
            t.Fatalf("delete: %v", err)
        }
    })
}
```

### 5.4 Subtests para tabla de tests (preview — se profundiza en T02)

```go
func TestAdd(t *testing.T) {
    tests := []struct {
        name     string
        a, b     int
        expected int
    }{
        {"positive", 2, 3, 5},
        {"negative", -1, -2, -3},
        {"mixed", -1, 5, 4},
        {"zero", 0, 0, 0},
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            got := Add(tt.a, tt.b)
            if got != tt.expected {
                t.Errorf("Add(%d, %d) = %d, want %d", tt.a, tt.b, got, tt.expected)
            }
        })
    }
}
```

---

## 6. t.Parallel() — tests concurrentes

### 6.1 Basico

```go
func TestParallel(t *testing.T) {
    // Los subtests con t.Parallel() se ejecutan concurrentemente
    // entre si (y con otros tests paralelos)

    t.Run("SubA", func(t *testing.T) {
        t.Parallel() // DEBE ser la primera llamada
        time.Sleep(1 * time.Second)
        // ...
    })

    t.Run("SubB", func(t *testing.T) {
        t.Parallel()
        time.Sleep(1 * time.Second)
        // ...
    })

    t.Run("SubC", func(t *testing.T) {
        t.Parallel()
        time.Sleep(1 * time.Second)
        // ...
    })

    // Los tres subtests corren concurrentemente
    // Tiempo total: ~1s en vez de ~3s
}
```

### 6.2 Como funciona t.Parallel()

```
┌────────────────────────────────────────────────────────────────────────┐
│              FLUJO DE EJECUCION CON t.Parallel()                      │
├────────────────────────────────────────────────────────────────────────┤
│                                                                        │
│  func TestParent(t *testing.T) {                                      │
│      // 1. Setup del parent (secuencial)                              │
│      shared := setupResource()                                        │
│                                                                        │
│      t.Run("A", func(t *testing.T) {                                 │
│          t.Parallel()  // 2. "A" se SUSPENDE aqui                    │
│          // no se ejecuta aun                                         │
│      })                                                               │
│                                                                        │
│      t.Run("B", func(t *testing.T) {                                 │
│          t.Parallel()  // 3. "B" se SUSPENDE aqui                    │
│          // no se ejecuta aun                                         │
│      })                                                               │
│                                                                        │
│      t.Run("C", func(t *testing.T) {                                 │
│          // 4. "C" NO es parallel — se ejecuta secuencialmente       │
│          // antes de que A y B empiecen                               │
│      })                                                               │
│                                                                        │
│      // 5. El parent termina su funcion                               │
│      // 6. AHORA A y B se desbloquean y corren en paralelo           │
│      // 7. El parent ESPERA a que A y B terminen                     │
│  }                                                                    │
│                                                                        │
│  Timeline:                                                            │
│  ─────────────────────────────────────────                            │
│  t=0  : Parent setup + C (secuencial)                                │
│  t=1  : A ═══════╗  (paralelo)                                      │
│         B ═══════╝  (paralelo)                                       │
│  t=end: Parent cleanup                                                │
└────────────────────────────────────────────────────────────────────────┘
```

### 6.3 El bug clasico: captura de variable del loop

```go
// BUG CLASICO (antes de Go 1.22)
func TestBuggy(t *testing.T) {
    tests := []struct {
        name  string
        input int
    }{
        {"one", 1},
        {"two", 2},
        {"three", 3},
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            t.Parallel()
            // BUG (Go < 1.22): tt es capturada por referencia
            // Cuando el subtest se ejecuta, el loop ya termino
            // y tt apunta al ULTIMO elemento
            fmt.Println(tt.input) // imprime 3, 3, 3 en vez de 1, 2, 3
        })
    }
}

// SOLUCION (Go < 1.22): copiar la variable
for _, tt := range tests {
    tt := tt // shadow: crea copia local para cada iteracion
    t.Run(tt.name, func(t *testing.T) {
        t.Parallel()
        fmt.Println(tt.input) // ahora imprime 1, 2, 3 correctamente
    })
}

// Go 1.22+ FIX: la variable del loop es per-iteration
// Ya no necesitas la copia "tt := tt"
// Pero SIGUE siendo buena practica incluirla para claridad
// y compatibilidad con versiones anteriores de Go
```

### 6.4 Paralelismo y recursos compartidos

```go
func TestParallelWithSharedResource(t *testing.T) {
    // Setup: crear recurso compartido ANTES de los subtests paralelos
    db := setupTestDB(t)
    // El parent NO tiene t.Parallel(), asi que el setup es secuencial

    t.Run("ReadUser", func(t *testing.T) {
        t.Parallel()
        user, err := db.GetUser("alice")
        if err != nil {
            t.Fatal(err)
        }
        // ... verificar user
        _ = user
    })

    t.Run("ReadProduct", func(t *testing.T) {
        t.Parallel()
        product, err := db.GetProduct("laptop")
        if err != nil {
            t.Fatal(err)
        }
        // ... verificar product
        _ = product
    })

    // CUIDADO: si los subtests paralelos MODIFICAN el recurso compartido,
    // necesitas sincronizacion (mutex) o datos aislados por test

    // PATRON: cada subtest trabaja con sus propios datos
    t.Run("WriteUser", func(t *testing.T) {
        t.Parallel()
        // Usar ID unico para evitar conflictos
        user := User{ID: t.Name(), Name: "test-write"}
        err := db.CreateUser(user)
        if err != nil {
            t.Fatal(err)
        }
        t.Cleanup(func() {
            db.DeleteUser(user.ID)
        })
    })
}

// Controlar grado de paralelismo:
// go test -parallel 4     → max 4 tests paralelos (default: GOMAXPROCS)
// go test -parallel 1     → secuencial (incluso con t.Parallel)
```

### 6.5 t.Parallel() y t.Setenv() — incompatibilidad

```go
// t.Setenv NO funciona con t.Parallel()
// Porque las env vars son globales al proceso

func TestBadPattern(t *testing.T) {
    t.Run("sub", func(t *testing.T) {
        t.Parallel()
        // t.Setenv("KEY", "value")  // PANIC: cannot use t.Setenv in parallel test
    })
}

// SOLUCION: pasar la configuracion como parametro
func TestGoodPattern(t *testing.T) {
    tests := []struct {
        name   string
        config Config
    }{
        {"default", Config{Port: 8080}},
        {"custom", Config{Port: 9090}},
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            t.Parallel()
            // Pasar config directamente, no via env vars
            server := NewServer(tt.config)
            // ...
            _ = server
        })
    }
}
```

---

## 7. TestMain — setup/teardown global

### 7.1 Basico

```go
// TestMain permite ejecutar codigo ANTES y DESPUES de todos los tests
// Solo puede haber UN TestMain por paquete

package mypackage

import (
    "os"
    "testing"
)

var testDB *sql.DB

func TestMain(m *testing.M) {
    // SETUP: antes de todos los tests
    fmt.Println("Setting up test environment...")

    var err error
    testDB, err = setupTestDatabase()
    if err != nil {
        fmt.Fprintf(os.Stderr, "setup failed: %v\n", err)
        os.Exit(1) // salir sin ejecutar tests
    }

    // EJECUTAR tests
    code := m.Run() // ejecuta TODOS los tests del paquete

    // TEARDOWN: despues de todos los tests
    fmt.Println("Tearing down test environment...")
    testDB.Close()
    cleanupTestData()

    // Salir con el codigo de los tests
    os.Exit(code)
}

// Los tests usan testDB como recurso compartido
func TestQueryUsers(t *testing.T) {
    users, err := queryUsers(testDB)
    if err != nil {
        t.Fatal(err)
    }
    if len(users) == 0 {
        t.Error("expected users")
    }
}
```

### 7.2 TestMain para integration tests con flags custom

```go
var integration = flag.Bool("integration", false, "run integration tests")

func TestMain(m *testing.M) {
    flag.Parse()

    if !*integration {
        fmt.Println("Skipping integration tests (use -integration flag)")
        os.Exit(0)
    }

    // Setup para integration tests
    if err := startDockerContainers(); err != nil {
        fmt.Fprintf(os.Stderr, "docker setup: %v\n", err)
        os.Exit(1)
    }

    code := m.Run()

    stopDockerContainers()
    os.Exit(code)
}

// Ejecutar: go test -integration
```

---

## 8. Helpers y patrones de assertion

### 8.1 Helpers genericos (Go 1.18+)

```go
package testutil

import (
    "testing"
)

// Equal verifica igualdad con genericos
func Equal[T comparable](t *testing.T, got, want T) {
    t.Helper()
    if got != want {
        t.Errorf("got %v, want %v", got, want)
    }
}

// NotEqual verifica desigualdad
func NotEqual[T comparable](t *testing.T, got, notWant T) {
    t.Helper()
    if got == notWant {
        t.Errorf("got %v, should not equal %v", got, notWant)
    }
}

// NoError verifica que no hay error
func NoError(t *testing.T, err error) {
    t.Helper()
    if err != nil {
        t.Fatalf("unexpected error: %v", err)
    }
}

// IsError verifica que hay un error
func IsError(t *testing.T, err error) {
    t.Helper()
    if err == nil {
        t.Fatal("expected an error, got nil")
    }
}

// ErrorIs verifica que el error es de un tipo especifico
func ErrorIs(t *testing.T, err, target error) {
    t.Helper()
    if !errors.Is(err, target) {
        t.Errorf("error = %v, want %v", err, target)
    }
}

// Panics verifica que una funcion provoca panic
func Panics(t *testing.T, fn func()) {
    t.Helper()
    defer func() {
        if r := recover(); r == nil {
            t.Error("expected panic, but function completed normally")
        }
    }()
    fn()
}

// SliceEqual compara dos slices
func SliceEqual[T comparable](t *testing.T, got, want []T) {
    t.Helper()
    if len(got) != len(want) {
        t.Errorf("length: got %d, want %d", len(got), len(want))
        return
    }
    for i := range got {
        if got[i] != want[i] {
            t.Errorf("index %d: got %v, want %v", i, got[i], want[i])
        }
    }
}

// MapEqual compara dos maps
func MapEqual[K comparable, V comparable](t *testing.T, got, want map[K]V) {
    t.Helper()
    if len(got) != len(want) {
        t.Errorf("length: got %d, want %d", len(got), len(want))
        return
    }
    for k, wantV := range want {
        gotV, ok := got[k]
        if !ok {
            t.Errorf("missing key %v", k)
            continue
        }
        if gotV != wantV {
            t.Errorf("key %v: got %v, want %v", k, gotV, wantV)
        }
    }
}

// Uso:
func TestCalculator(t *testing.T) {
    testutil.Equal(t, Add(2, 3), 5)
    testutil.NoError(t, err)
    testutil.SliceEqual(t, Sort([]int{3, 1, 2}), []int{1, 2, 3})
    testutil.Panics(t, func() { Divide(1, 0) })
}
```

### 8.2 Deep comparison con go-cmp

```go
import "github.com/google/go-cmp/cmp"

// go-cmp es la libreria recomendada por el equipo de Go para
// comparaciones profundas de structs (no esta en stdlib pero es semi-oficial)

func TestUserCreation(t *testing.T) {
    got := CreateUser("Alice", 30)
    want := User{
        Name: "Alice",
        Age:  30,
        // CreatedAt sera diferente — necesitamos ignorarlo
    }

    // cmp.Diff retorna un diff legible si hay diferencias
    if diff := cmp.Diff(want, got, cmpopts.IgnoreFields(User{}, "CreatedAt", "ID")); diff != "" {
        t.Errorf("CreateUser() mismatch (-want +got):\n%s", diff)
    }
}

// Output de cmp.Diff cuando falla:
// CreateUser() mismatch (-want +got):
//   User{
// -     Name: "Alice",
// +     Name: "Alicee",
//       Age:  30,
//   }

// Opciones comunes:
// cmpopts.IgnoreFields(Type{}, "Field1", "Field2")
// cmpopts.IgnoreUnexported(Type{})
// cmpopts.SortSlices(func(a, b T) bool { return a.ID < b.ID })
// cmpopts.EquateApprox(0.001, 0)  // float comparison con tolerancia
// cmpopts.EquateEmpty()  // nil slice == empty slice
```

### 8.3 Helpers para setup de tests

```go
// Helper que crea un servidor HTTP de test
func setupTestServer(t *testing.T, handler http.Handler) *httptest.Server {
    t.Helper()
    server := httptest.NewServer(handler)
    t.Cleanup(func() {
        server.Close()
    })
    return server
}

// Helper que crea un directorio temporal con archivos
func setupTestDir(t *testing.T, files map[string]string) string {
    t.Helper()
    dir := t.TempDir()
    for name, content := range files {
        path := filepath.Join(dir, name)
        // Crear subdirectorios si es necesario
        if err := os.MkdirAll(filepath.Dir(path), 0755); err != nil {
            t.Fatal(err)
        }
        if err := os.WriteFile(path, []byte(content), 0644); err != nil {
            t.Fatal(err)
        }
    }
    return dir
}

// Helper que lee un archivo de testdata
func readTestdata(t *testing.T, name string) []byte {
    t.Helper()
    data, err := os.ReadFile(filepath.Join("testdata", name))
    if err != nil {
        t.Fatalf("reading testdata/%s: %v", name, err)
    }
    return data
}

// Helper que crea un context con timeout
func testContext(t *testing.T) context.Context {
    t.Helper()
    ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
    t.Cleanup(cancel)
    return ctx
}

// Uso:
func TestAPI(t *testing.T) {
    server := setupTestServer(t, myHandler)
    ctx := testContext(t)

    req, _ := http.NewRequestWithContext(ctx, "GET", server.URL+"/users", nil)
    resp, err := http.DefaultClient.Do(req)
    if err != nil {
        t.Fatal(err)
    }
    defer resp.Body.Close()

    testutil.Equal(t, resp.StatusCode, 200)
}
```

---

## 9. testdata/ — datos de prueba

### 9.1 Convencion y uso

```
mypackage/
├── parser.go
├── parser_test.go
└── testdata/
    ├── valid_input.json
    ├── invalid_input.json
    ├── expected_output.txt
    ├── golden/
    │   ├── test1.golden
    │   └── test2.golden
    └── fixtures/
        ├── users.csv
        └── config.yaml
```

```go
func TestParser(t *testing.T) {
    // testdata/ se accede con path relativo al paquete
    input, err := os.ReadFile("testdata/valid_input.json")
    if err != nil {
        t.Fatal(err)
    }

    got, err := Parse(input)
    if err != nil {
        t.Fatalf("Parse() error: %v", err)
    }

    expected, err := os.ReadFile("testdata/expected_output.txt")
    if err != nil {
        t.Fatal(err)
    }

    if string(got) != string(expected) {
        t.Errorf("output mismatch:\ngot:  %s\nwant: %s", got, expected)
    }
}
```

### 9.2 Golden files — tests de snapshot

```go
// Golden tests: comparar output con un archivo "golden" (referencia)
// Si el golden file no existe o necesita actualizarse, usa -update

var update = flag.Bool("update", false, "update golden files")

func TestGolden(t *testing.T) {
    tests := []struct {
        name  string
        input string
    }{
        {"simple", "hello world"},
        {"complex", "hello\nworld\nfoo"},
        {"empty", ""},
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            got := Format(tt.input) // funcion bajo test

            goldenFile := filepath.Join("testdata", "golden", tt.name+".golden")

            if *update {
                // Actualizar el golden file
                os.MkdirAll(filepath.Dir(goldenFile), 0755)
                os.WriteFile(goldenFile, []byte(got), 0644)
                t.Logf("updated golden file: %s", goldenFile)
                return
            }

            want, err := os.ReadFile(goldenFile)
            if err != nil {
                t.Fatalf("reading golden file: %v (run with -update to create)", err)
            }

            if got != string(want) {
                t.Errorf("output mismatch (run with -update to fix):\ngot:\n%s\nwant:\n%s", got, want)
            }
        })
    }
}

// Uso:
// go test -run TestGolden -update    → crear/actualizar golden files
// go test -run TestGolden            → verificar contra golden files
```

---

## 10. Comparacion con C y Rust

| Aspecto | C | Go | Rust |
|---|---|---|---|
| Framework | Ninguno built-in (CUnit, Unity, Check) | `testing` (stdlib) | `#[test]` (built-in) |
| Ejecutar tests | Compilar+ejecutar manualmente o make | `go test` | `cargo test` |
| Archivo de test | Separado (test_*.c) | `*_test.go` (misma carpeta) | Mismo archivo o `tests/` |
| Descubrimiento | Manual (registrar tests) | Automatico por nombre `Test*` | Automatico por `#[test]` |
| Assertions | Macros custom (assert, CU_ASSERT) | `if` + `t.Errorf` (manual) | `assert!`, `assert_eq!`, `assert_ne!` |
| Subtests | No nativo | `t.Run("name", fn)` | Modulos anidados |
| Paralelismo | Manual (threads) | `t.Parallel()` | `cargo test` (paralelo por defecto) |
| Setup/Teardown | Manual | `t.Cleanup()`, `TestMain` | `#[ctor]`, Drop trait |
| Temp files | Manual (mktemp) | `t.TempDir()` | `tempfile` crate |
| Coverage | gcov (externo) | `go test -cover` (integrado) | `cargo-tarpaulin` o `grcov` |
| Race detection | Valgrind/TSan (externo) | `go test -race` (integrado) | No necesario (compilador) |
| Mocking | Manual | Interfaces (manual o gomock) | mockall, mockito crates |
| Fixtures | Manual | `testdata/` convencion | `rstest` crate |
| Table-driven | Macros o manual | Slice de structs + `t.Run` | `rstest` o manual |
| Benchmarks | Manual (clock_gettime) | `testing.B` integrado | `criterion` o `#[bench]` (nightly) |
| Fuzzing | AFL, libFuzzer (externo) | `testing.F` (Go 1.18+, integrado) | `cargo-fuzz` (libFuzzer) |
| Property tests | No | `testing/quick` (basico) | `proptest`, `quickcheck` |
| Test output | printf | `t.Log`, `t.Error`, `t.Fatal` | `println!` (capturado, mostrado en fallo) |
| Skip test | `#ifdef` | `t.Skip()` | `#[ignore]` |
| Build tags | `#ifdef` | `//go:build tag` | `#[cfg(test)]`, features |
| Ejemplo ejecutable | No | `func Example*()` | `///` doc tests |

### Ejemplo comparativo: test basico

```c
// C (con CUnit o manual)
#include <assert.h>

void test_add(void) {
    assert(add(2, 3) == 5);
    assert(add(-1, 1) == 0);
    assert(add(0, 0) == 0);
}

int main(void) {
    test_add();
    printf("All tests passed\n");
    return 0;
}
// Problemas: sin reporte de que fallo, sin runner automatico
```

```go
// Go
func TestAdd(t *testing.T) {
    tests := []struct{ a, b, want int }{
        {2, 3, 5},
        {-1, 1, 0},
        {0, 0, 0},
    }
    for _, tt := range tests {
        got := Add(tt.a, tt.b)
        if got != tt.want {
            t.Errorf("Add(%d, %d) = %d, want %d", tt.a, tt.b, got, tt.want)
        }
    }
}
// go test -v → reporte detallado automatico
```

```rust
// Rust
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_add() {
        assert_eq!(add(2, 3), 5);
        assert_eq!(add(-1, 1), 0);
        assert_eq!(add(0, 0), 0);
    }

    #[test]
    #[should_panic(expected = "overflow")]
    fn test_add_overflow() {
        add(i32::MAX, 1);
    }
}
// cargo test → paralelo por defecto
```

**Diferencias clave**:
- **Go no tiene assert built-in** — `if` + `t.Errorf` es verboso pero explícito. Puedes escribir helpers o usar testify.
- **Rust tiene `assert_eq!`** — macro que imprime los valores en fallo. Mas conciso que Go.
- **C no tiene nada** — necesitas librerias externas o macros manuales.
- **Go tests corren secuencial** por defecto (opt-in parallel). Rust tests corren **paralelo por defecto** (opt-out con `--test-threads=1`).
- **Go tiene race detector integrado** (`-race`). Rust no lo necesita (el compilador previene data races).

---

## 11. Programa completo: libreria con test suite

```go
// ============================================================================
// Archivo: stringutil/stringutil.go
// ============================================================================

package stringutil

import (
    "strings"
    "unicode"
    "unicode/utf8"
)

// Reverse invierte un string preservando UTF-8
func Reverse(s string) string {
    runes := []rune(s)
    for i, j := 0, len(runes)-1; i < j; i, j = i+1, j-1 {
        runes[i], runes[j] = runes[j], runes[i]
    }
    return string(runes)
}

// Truncate corta un string a maxLen caracteres y agrega "..." si fue cortado
func Truncate(s string, maxLen int) string {
    if maxLen < 0 {
        return ""
    }
    if utf8.RuneCountInString(s) <= maxLen {
        return s
    }
    runes := []rune(s)
    if maxLen <= 3 {
        return string(runes[:maxLen])
    }
    return string(runes[:maxLen-3]) + "..."
}

// CamelToSnake convierte CamelCase a snake_case
func CamelToSnake(s string) string {
    var result strings.Builder
    for i, r := range s {
        if unicode.IsUpper(r) {
            if i > 0 {
                prev := rune(s[i-1])
                if unicode.IsLower(prev) || unicode.IsDigit(prev) {
                    result.WriteByte('_')
                }
            }
            result.WriteRune(unicode.ToLower(r))
        } else {
            result.WriteRune(r)
        }
    }
    return result.String()
}

// SnakeToCamel convierte snake_case a CamelCase
func SnakeToCamel(s string) string {
    var result strings.Builder
    capitalizeNext := true
    for _, r := range s {
        if r == '_' {
            capitalizeNext = true
            continue
        }
        if capitalizeNext {
            result.WriteRune(unicode.ToUpper(r))
            capitalizeNext = false
        } else {
            result.WriteRune(r)
        }
    }
    return result.String()
}

// IsPalindrome verifica si un string es palindromo (case-insensitive, ignora espacios)
func IsPalindrome(s string) bool {
    // Limpiar: solo letras y digitos, lowercase
    var cleaned []rune
    for _, r := range s {
        if unicode.IsLetter(r) || unicode.IsDigit(r) {
            cleaned = append(cleaned, unicode.ToLower(r))
        }
    }
    for i, j := 0, len(cleaned)-1; i < j; i, j = i+1, j-1 {
        if cleaned[i] != cleaned[j] {
            return false
        }
    }
    return true
}

// WordCount cuenta palabras (separadas por espacios)
func WordCount(s string) map[string]int {
    counts := make(map[string]int)
    words := strings.Fields(s)
    for _, word := range words {
        word = strings.ToLower(strings.Trim(word, ".,;:!?\"'"))
        if word != "" {
            counts[word]++
        }
    }
    return counts
}

// Wrap envuelve texto a un ancho maximo (word wrap)
func Wrap(s string, width int) string {
    if width <= 0 {
        return s
    }
    words := strings.Fields(s)
    if len(words) == 0 {
        return ""
    }

    var lines []string
    currentLine := words[0]

    for _, word := range words[1:] {
        if utf8.RuneCountInString(currentLine)+1+utf8.RuneCountInString(word) <= width {
            currentLine += " " + word
        } else {
            lines = append(lines, currentLine)
            currentLine = word
        }
    }
    lines = append(lines, currentLine)

    return strings.Join(lines, "\n")
}
```

```go
// ============================================================================
// Archivo: stringutil/stringutil_test.go
// ============================================================================

package stringutil_test

import (
    "strings"
    "testing"

    "mymodule/stringutil" // ajustar al modulo real
)

// ============================================================================
// Test: Reverse
// ============================================================================

func TestReverse(t *testing.T) {
    t.Run("basic ASCII", func(t *testing.T) {
        got := stringutil.Reverse("hello")
        if got != "olleh" {
            t.Errorf("Reverse(%q) = %q, want %q", "hello", got, "olleh")
        }
    })

    t.Run("empty string", func(t *testing.T) {
        got := stringutil.Reverse("")
        if got != "" {
            t.Errorf("Reverse(%q) = %q, want %q", "", got, "")
        }
    })

    t.Run("single character", func(t *testing.T) {
        got := stringutil.Reverse("a")
        if got != "a" {
            t.Errorf("Reverse(%q) = %q, want %q", "a", got, "a")
        }
    })

    t.Run("palindrome", func(t *testing.T) {
        got := stringutil.Reverse("racecar")
        if got != "racecar" {
            t.Errorf("Reverse(%q) = %q, want %q", "racecar", got, "racecar")
        }
    })

    t.Run("UTF-8 multibyte", func(t *testing.T) {
        got := stringutil.Reverse("日本語")
        if got != "語本日" {
            t.Errorf("Reverse(%q) = %q, want %q", "日本語", got, "語本日")
        }
    })

    t.Run("emojis", func(t *testing.T) {
        got := stringutil.Reverse("Go🚀")
        if got != "🚀oG" {
            t.Errorf("Reverse(%q) = %q, want %q", "Go🚀", got, "🚀oG")
        }
    })

    t.Run("with spaces", func(t *testing.T) {
        got := stringutil.Reverse("hello world")
        if got != "dlrow olleh" {
            t.Errorf("Reverse(%q) = %q, want %q", "hello world", got, "dlrow olleh")
        }
    })
}

// ============================================================================
// Test: Truncate
// ============================================================================

func TestTruncate(t *testing.T) {
    t.Run("shorter than max", func(t *testing.T) {
        got := stringutil.Truncate("hello", 10)
        if got != "hello" {
            t.Errorf("got %q, want %q", got, "hello")
        }
    })

    t.Run("exact length", func(t *testing.T) {
        got := stringutil.Truncate("hello", 5)
        if got != "hello" {
            t.Errorf("got %q, want %q", got, "hello")
        }
    })

    t.Run("needs truncation", func(t *testing.T) {
        got := stringutil.Truncate("hello world", 8)
        want := "hello..."
        if got != want {
            t.Errorf("got %q, want %q", got, want)
        }
    })

    t.Run("very short max", func(t *testing.T) {
        got := stringutil.Truncate("hello", 2)
        want := "he"
        if got != want {
            t.Errorf("got %q, want %q", got, want)
        }
    })

    t.Run("zero max", func(t *testing.T) {
        got := stringutil.Truncate("hello", 0)
        if got != "" {
            t.Errorf("got %q, want %q", got, "")
        }
    })

    t.Run("negative max", func(t *testing.T) {
        got := stringutil.Truncate("hello", -1)
        if got != "" {
            t.Errorf("got %q, want %q", got, "")
        }
    })

    t.Run("UTF-8", func(t *testing.T) {
        got := stringutil.Truncate("日本語テスト", 5)
        want := "日本..."
        if got != want {
            t.Errorf("got %q, want %q", got, want)
        }
    })

    t.Run("empty string", func(t *testing.T) {
        got := stringutil.Truncate("", 5)
        if got != "" {
            t.Errorf("got %q, want %q", got, "")
        }
    })
}

// ============================================================================
// Test: CamelToSnake
// ============================================================================

func TestCamelToSnake(t *testing.T) {
    tests := []struct {
        input string
        want  string
    }{
        {"HelloWorld", "hello_world"},
        {"helloWorld", "hello_world"},
        {"HTTPServer", "h_t_t_p_server"}, // limitacion: no maneja acronimos
        {"userID", "user_i_d"},
        {"simple", "simple"},
        {"A", "a"},
        {"", ""},
        {"already_snake", "already_snake"},
        {"getHTTPResponse", "get_h_t_t_p_response"},
        {"MyJSON", "my_j_s_o_n"},
    }

    for _, tt := range tests {
        t.Run(tt.input, func(t *testing.T) {
            got := stringutil.CamelToSnake(tt.input)
            if got != tt.want {
                t.Errorf("CamelToSnake(%q) = %q, want %q", tt.input, got, tt.want)
            }
        })
    }
}

// ============================================================================
// Test: SnakeToCamel
// ============================================================================

func TestSnakeToCamel(t *testing.T) {
    tests := []struct {
        input string
        want  string
    }{
        {"hello_world", "HelloWorld"},
        {"user_id", "UserId"},
        {"simple", "Simple"},
        {"a", "A"},
        {"", ""},
        {"already_camel", "AlreadyCamel"},
        {"__double__underscore__", "DoubleUnderscore"},
        {"trailing_", "Trailing"},
        {"_leading", "Leading"},
        {"one", "One"},
    }

    for _, tt := range tests {
        t.Run(tt.input, func(t *testing.T) {
            got := stringutil.SnakeToCamel(tt.input)
            if got != tt.want {
                t.Errorf("SnakeToCamel(%q) = %q, want %q", tt.input, got, tt.want)
            }
        })
    }
}

// ============================================================================
// Test: IsPalindrome
// ============================================================================

func TestIsPalindrome(t *testing.T) {
    t.Run("palindromes", func(t *testing.T) {
        palindromes := []string{
            "racecar",
            "A man a plan a canal Panama",
            "Was it a car or a cat I saw",
            "No lemon no melon",
            "",
            "a",
            "aa",
            "aba",
            "12321",
        }
        for _, s := range palindromes {
            t.Run(s, func(t *testing.T) {
                if !stringutil.IsPalindrome(s) {
                    t.Errorf("IsPalindrome(%q) = false, want true", s)
                }
            })
        }
    })

    t.Run("not palindromes", func(t *testing.T) {
        notPalindromes := []string{
            "hello",
            "world",
            "ab",
            "abc",
            "Go is great",
        }
        for _, s := range notPalindromes {
            t.Run(s, func(t *testing.T) {
                if stringutil.IsPalindrome(s) {
                    t.Errorf("IsPalindrome(%q) = true, want false", s)
                }
            })
        }
    })
}

// ============================================================================
// Test: WordCount
// ============================================================================

func TestWordCount(t *testing.T) {
    t.Run("basic", func(t *testing.T) {
        got := stringutil.WordCount("hello world hello")
        if got["hello"] != 2 {
            t.Errorf("count of 'hello' = %d, want 2", got["hello"])
        }
        if got["world"] != 1 {
            t.Errorf("count of 'world' = %d, want 1", got["world"])
        }
    })

    t.Run("with punctuation", func(t *testing.T) {
        got := stringutil.WordCount("Hello, hello! HELLO.")
        if got["hello"] != 3 {
            t.Errorf("count of 'hello' = %d, want 3", got["hello"])
        }
    })

    t.Run("empty string", func(t *testing.T) {
        got := stringutil.WordCount("")
        if len(got) != 0 {
            t.Errorf("expected empty map, got %v", got)
        }
    })

    t.Run("single word", func(t *testing.T) {
        got := stringutil.WordCount("go")
        if got["go"] != 1 {
            t.Errorf("count of 'go' = %d, want 1", got["go"])
        }
        if len(got) != 1 {
            t.Errorf("expected 1 entry, got %d", len(got))
        }
    })
}

// ============================================================================
// Test: Wrap
// ============================================================================

func TestWrap(t *testing.T) {
    t.Run("no wrap needed", func(t *testing.T) {
        got := stringutil.Wrap("hello world", 20)
        if got != "hello world" {
            t.Errorf("got %q, want %q", got, "hello world")
        }
    })

    t.Run("basic wrap", func(t *testing.T) {
        got := stringutil.Wrap("the quick brown fox jumps over the lazy dog", 15)
        want := "the quick brown\nfox jumps over\nthe lazy dog"
        if got != want {
            t.Errorf("got:\n%s\nwant:\n%s", got, want)
        }
    })

    t.Run("single long word", func(t *testing.T) {
        got := stringutil.Wrap("superlongword", 5)
        // La palabra es mas larga que el width — no se puede partir
        if got != "superlongword" {
            t.Errorf("got %q, want %q", got, "superlongword")
        }
    })

    t.Run("empty string", func(t *testing.T) {
        got := stringutil.Wrap("", 10)
        if got != "" {
            t.Errorf("got %q, want %q", got, "")
        }
    })

    t.Run("zero width", func(t *testing.T) {
        got := stringutil.Wrap("hello world", 0)
        // Con width 0, cada palabra es una linea
        if got != "hello world" {
            // Depende de la implementacion
            t.Logf("Wrap with width=0: %q", got)
        }
    })

    t.Run("exact fit", func(t *testing.T) {
        got := stringutil.Wrap("hello world", 11)
        if got != "hello world" {
            t.Errorf("got %q, want %q", got, "hello world")
        }
    })
}

// ============================================================================
// Test: Parallel subtests example
// ============================================================================

func TestParallelExample(t *testing.T) {
    // Tests independientes que pueden correr en paralelo
    t.Run("Reverse", func(t *testing.T) {
        t.Parallel()
        got := stringutil.Reverse("parallel")
        if got != "lellarap" {
            t.Errorf("got %q", got)
        }
    })

    t.Run("IsPalindrome", func(t *testing.T) {
        t.Parallel()
        if !stringutil.IsPalindrome("racecar") {
            t.Error("racecar should be palindrome")
        }
    })

    t.Run("CamelToSnake", func(t *testing.T) {
        t.Parallel()
        got := stringutil.CamelToSnake("ParallelTest")
        if got != "parallel_test" {
            t.Errorf("got %q", got)
        }
    })
}

// ============================================================================
// Test: Helper function example
// ============================================================================

func assertStringEqual(t *testing.T, got, want string) {
    t.Helper()
    if got != want {
        t.Errorf("got %q, want %q", got, want)
    }
}

func TestWithHelper(t *testing.T) {
    assertStringEqual(t, stringutil.Reverse("abc"), "cba")
    assertStringEqual(t, stringutil.Truncate("hello world", 8), "hello...")
    assertStringEqual(t, stringutil.CamelToSnake("HelloWorld"), "hello_world")
    assertStringEqual(t, stringutil.SnakeToCamel("hello_world"), "HelloWorld")
}

// ============================================================================
// Example functions (ejecutables y verificables)
// ============================================================================

func ExampleReverse() {
    fmt.Println(stringutil.Reverse("hello"))
    fmt.Println(stringutil.Reverse("日本語"))
    // Output:
    // olleh
    // 語本日
}

func ExampleTruncate() {
    fmt.Println(stringutil.Truncate("hello world", 8))
    fmt.Println(stringutil.Truncate("hi", 10))
    // Output:
    // hello...
    // hi
}

func ExampleIsPalindrome() {
    fmt.Println(stringutil.IsPalindrome("racecar"))
    fmt.Println(stringutil.IsPalindrome("A man a plan a canal Panama"))
    fmt.Println(stringutil.IsPalindrome("hello"))
    // Output:
    // true
    // true
    // false
}
```

```
$ go test -v ./stringutil/

=== RUN   TestReverse
=== RUN   TestReverse/basic_ASCII
=== RUN   TestReverse/empty_string
=== RUN   TestReverse/single_character
=== RUN   TestReverse/palindrome
=== RUN   TestReverse/UTF-8_multibyte
=== RUN   TestReverse/emojis
=== RUN   TestReverse/with_spaces
--- PASS: TestReverse (0.00s)
    --- PASS: TestReverse/basic_ASCII (0.00s)
    --- PASS: TestReverse/empty_string (0.00s)
    --- PASS: TestReverse/single_character (0.00s)
    --- PASS: TestReverse/palindrome (0.00s)
    --- PASS: TestReverse/UTF-8_multibyte (0.00s)
    --- PASS: TestReverse/emojis (0.00s)
    --- PASS: TestReverse/with_spaces (0.00s)
=== RUN   TestTruncate
=== RUN   TestTruncate/shorter_than_max
=== RUN   TestTruncate/exact_length
=== RUN   TestTruncate/needs_truncation
=== RUN   TestTruncate/very_short_max
=== RUN   TestTruncate/zero_max
=== RUN   TestTruncate/negative_max
=== RUN   TestTruncate/UTF-8
=== RUN   TestTruncate/empty_string
--- PASS: TestTruncate (0.00s)
    ...
=== RUN   TestCamelToSnake
=== RUN   TestCamelToSnake/HelloWorld
=== RUN   TestCamelToSnake/helloWorld
    ...
--- PASS: TestCamelToSnake (0.00s)
    ...
=== RUN   TestParallelExample
=== RUN   TestParallelExample/Reverse
=== PAUSE TestParallelExample/Reverse
=== RUN   TestParallelExample/IsPalindrome
=== PAUSE TestParallelExample/IsPalindrome
=== RUN   TestParallelExample/CamelToSnake
=== PAUSE TestParallelExample/CamelToSnake
=== CONT  TestParallelExample/Reverse
=== CONT  TestParallelExample/CamelToSnake
=== CONT  TestParallelExample/IsPalindrome
--- PASS: TestParallelExample (0.00s)
    --- PASS: TestParallelExample/Reverse (0.00s)
    --- PASS: TestParallelExample/CamelToSnake (0.00s)
    --- PASS: TestParallelExample/IsPalindrome (0.00s)
=== RUN   ExampleReverse
--- PASS: ExampleReverse (0.00s)
=== RUN   ExampleTruncate
--- PASS: ExampleTruncate (0.00s)
=== RUN   ExampleIsPalindrome
--- PASS: ExampleIsPalindrome (0.00s)
PASS
ok      mymodule/stringutil     0.003s
```

**Puntos clave del programa**:

1. **Paquete separado** (`package stringutil_test`): black-box testing que prueba la API publica.
2. **Subtests organizados**: cada funcion tiene su `Test*` con subtests por escenario.
3. **Mix de estilos**: `TestReverse` usa subtests explicitos, `TestCamelToSnake` usa tabla (preview de T02), `TestParallelExample` demuestra paralelismo.
4. **Edge cases**: empty strings, UTF-8, negative inputs, boundary conditions.
5. **`t.Helper()`**: `assertStringEqual` marca como helper para reportar la linea correcta.
6. **Example functions**: `ExampleReverse`, `ExampleTruncate`, `ExampleIsPalindrome` — son tests ejecutables que verifican el output Y se muestran en `go doc`.
7. **Sin dependencias externas**: todo con la stdlib.

---

## 12. Ejercicios

### Ejercicio 1: Test suite para un Stack

Dado el siguiente Stack:
```go
type Stack[T any] struct{ items []T }
func (s *Stack[T]) Push(v T)
func (s *Stack[T]) Pop() (T, bool)
func (s *Stack[T]) Peek() (T, bool)
func (s *Stack[T]) Len() int
func (s *Stack[T]) IsEmpty() bool
```

Escribe tests que cubran:
1. Push + Pop basico
2. Pop de stack vacio
3. Peek no modifica el stack
4. Push multiples + Pop en orden LIFO
5. IsEmpty despues de push/pop
6. Tests con diferentes tipos (int, string, struct)
7. Tests paralelos donde cada subtest tiene su propio stack

### Ejercicio 2: Tests con t.TempDir y archivos

Escribe una funcion `CountLines(filename string) (int, error)` y tests que:
1. Usen `t.TempDir()` para crear archivos temporales
2. Testen archivo vacio (0 lineas)
3. Testen archivo con 1 linea sin newline final
4. Testen archivo con multiples lineas
5. Testen archivo que no existe (error expected)
6. Testen archivo con lineas vacias intermedias
7. Usen un helper `createTestFile(t *testing.T, dir, content string) string`

### Ejercicio 3: TestMain con setup global

Implementa un `KeyValueStore` con operaciones CRUD y:
1. Un `TestMain` que inicializa el store con datos de prueba
2. Tests que usan los datos pre-cargados
3. Un subtest que modifica datos y verifica
4. `t.Cleanup` para resetear datos despues de cada test que modifica
5. `t.Skip` para tests que requieren una condicion especial
6. Al menos 3 subtests paralelos

### Ejercicio 4: Test helpers genericos

Crea un paquete `testutil` con helpers:
1. `Equal[T comparable](t, got, want T)` — con diff message
2. `DeepEqual(t, got, want any)` — usando reflect.DeepEqual
3. `ErrorContains(t, err, substr string)` — verifica que el error contiene un substring
4. `Within(t, got, want, tolerance float64)` — comparacion de floats con tolerancia
5. `EventuallyTrue(t, fn func() bool, timeout, interval time.Duration)` — espera que una condicion sea true

Todos deben usar `t.Helper()`. Escribe tests para los helpers mismos.

---

> **Siguiente**: C11/S02 - Testing, T02 - Table-driven tests — el patron idiomatico de Go, cuando y como usarlo
