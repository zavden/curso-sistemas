# Table-driven tests — el patron idiomatico de Go, cuando y como usarlo

## 1. Introduccion

Los **table-driven tests** son EL patron idiomatico de testing en Go. No son una libreria ni un framework — son una convencion que combina slices de structs, `t.Run()`, y loops para crear tests concisos, extensibles, y faciles de mantener. Practicamente todo el codigo Go de Google, la stdlib, Kubernetes, y la mayoria de proyectos open source los usan.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                    TABLE-DRIVEN TESTS — ANATOMIA                                │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  func TestAdd(t *testing.T) {                                                   │
│                                                                                  │
│      ┌─ 1. TABLA: slice de structs anonimos                                    │
│      │   (cada struct es un "caso de test")                                     │
│      │                                                                          │
│      │  tests := []struct {                                                     │
│      │      name     string    ← nombre del subtest                            │
│      │      a, b     int       ← inputs                                        │
│      │      want     int       ← output esperado                               │
│      │      wantErr  bool      ← error esperado?                               │
│      │  }{                                                                      │
│      │      {"positive",     2, 3, 5, false},                                  │
│      │      {"negative",    -1,-2,-3, false},                                  │
│      │      {"zero",         0, 0, 0, false},                                  │
│      │      {"overflow",  MAX, 1, 0, true},   ← agregar caso = agregar linea  │
│      │  }                                                                       │
│      │                                                                          │
│      ├─ 2. LOOP: iterar sobre la tabla                                         │
│      │                                                                          │
│      │  for _, tt := range tests {                                             │
│      │      tt := tt  ← captura (Go < 1.22)                                   │
│      │                                                                          │
│      ├─ 3. SUBTEST: t.Run con el nombre del caso                              │
│      │                                                                          │
│      │      t.Run(tt.name, func(t *testing.T) {                               │
│      │                                                                          │
│      ├─ 4. EJECUCION: llamar la funcion bajo test                             │
│      │                                                                          │
│      │          got, err := Add(tt.a, tt.b)                                    │
│      │                                                                          │
│      ├─ 5. VERIFICACION: comparar con expected                                │
│      │                                                                          │
│      │          if (err != nil) != tt.wantErr {                                │
│      │              t.Errorf("error = %v, wantErr %v", err, tt.wantErr)       │
│      │          }                                                               │
│      │          if got != tt.want {                                             │
│      │              t.Errorf("Add(%d,%d) = %d, want %d",                      │
│      │                  tt.a, tt.b, got, tt.want)                             │
│      │          }                                                               │
│      │      })                                                                  │
│      │  }                                                                       │
│      └                                                                          │
│  }                                                                              │
│                                                                                  │
│  BENEFICIOS:                                                                    │
│  • Agregar un caso = agregar UNA LINEA a la tabla                              │
│  • La logica de verificacion se escribe UNA VEZ                                │
│  • Cada caso tiene nombre → go test -run TestAdd/positive                      │
│  • Los casos documentan el comportamiento de la funcion                        │
│  • Facil de revisar en code review (tabla de datos, no logica)                 │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. El patron basico paso a paso

### 2.1 Forma canonica

```go
func TestMultiply(t *testing.T) {
    // Paso 1: definir la tabla
    tests := []struct {
        name string // siempre incluir name
        a, b int    // inputs
        want int    // output esperado
    }{
        // Paso 2: llenar los casos
        {name: "positive", a: 3, b: 4, want: 12},
        {name: "zero", a: 5, b: 0, want: 0},
        {name: "negative", a: -3, b: 4, want: -12},
        {name: "both negative", a: -3, b: -4, want: 12},
        {name: "one", a: 7, b: 1, want: 7},
        {name: "large", a: 1000, b: 1000, want: 1000000},
    }

    // Paso 3: iterar y ejecutar
    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            got := Multiply(tt.a, tt.b)
            if got != tt.want {
                t.Errorf("Multiply(%d, %d) = %d, want %d",
                    tt.a, tt.b, got, tt.want)
            }
        })
    }
}
```

### 2.2 Campos nombrados vs posicionales

```go
// POSICIONAL — conciso pero puede confundir con muchos campos
tests := []struct {
    name string
    a, b int
    want int
}{
    {"positive", 2, 3, 5},
    {"negative", -1, -2, -3},
    {"zero", 0, 0, 0},
}

// NOMBRADO — mas verboso pero mas claro
tests := []struct {
    name string
    a, b int
    want int
}{
    {name: "positive", a: 2, b: 3, want: 5},
    {name: "negative", a: -1, b: -2, want: -3},
    {name: "zero", a: 0, b: 0, want: 0},
}

// RECOMENDACION:
// - Posicional para tablas simples (2-3 campos, tipos obvios)
// - Nombrado cuando hay mas de 3 campos o los tipos son ambiguos
// - SIEMPRE nombrado si hay booleans (wantErr: true es mas claro que ..., true)
```

### 2.3 Convencion de nombres del struct

```go
// La variable se llama "tests" (plural)
// Cada iteracion se llama "tt" (test case) o "tc"
// El subtest usa tt.name

tests := []struct{ ... }{ ... }

for _, tt := range tests {
    t.Run(tt.name, func(t *testing.T) {
        // ...
    })
}

// Alternativas aceptadas:
// - tc (test case)
// - test (singular, menos comun)
// - Evitar: c, x, item, entry
```

---

## 3. Patrones de campos en la tabla

### 3.1 Input + output

```go
// El caso mas simple: inputs → output esperado
func TestAbs(t *testing.T) {
    tests := []struct {
        name  string
        input int
        want  int
    }{
        {"positive", 5, 5},
        {"negative", -5, 5},
        {"zero", 0, 0},
        {"min int32", math.MinInt32, math.MinInt32}, // cuidado con overflow
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            got := Abs(tt.input)
            if got != tt.want {
                t.Errorf("Abs(%d) = %d, want %d", tt.input, got, tt.want)
            }
        })
    }
}
```

### 3.2 Input + output + error

```go
// Funciones que retornan (T, error)
func TestDivide(t *testing.T) {
    tests := []struct {
        name    string
        a, b    float64
        want    float64
        wantErr bool
    }{
        {name: "normal", a: 10, b: 2, want: 5, wantErr: false},
        {name: "decimal", a: 7, b: 2, want: 3.5, wantErr: false},
        {name: "divide by zero", a: 10, b: 0, want: 0, wantErr: true},
        {name: "negative", a: -10, b: 2, want: -5, wantErr: false},
        {name: "zero numerator", a: 0, b: 5, want: 0, wantErr: false},
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            got, err := Divide(tt.a, tt.b)

            // Primero verificar error
            if (err != nil) != tt.wantErr {
                t.Fatalf("Divide(%g, %g) error = %v, wantErr %v",
                    tt.a, tt.b, err, tt.wantErr)
            }

            // Si esperabamos error, no verificar el valor
            if tt.wantErr {
                return
            }

            // Verificar valor
            if got != tt.want {
                t.Errorf("Divide(%g, %g) = %g, want %g",
                    tt.a, tt.b, got, tt.want)
            }
        })
    }
}
```

### 3.3 Error especifico (no solo bool)

```go
// Cuando necesitas verificar el TIPO de error, no solo si hay error
func TestParseAge(t *testing.T) {
    tests := []struct {
        name    string
        input   string
        want    int
        wantErr error // error especifico o nil
    }{
        {name: "valid", input: "25", want: 25, wantErr: nil},
        {name: "zero", input: "0", want: 0, wantErr: nil},
        {name: "negative", input: "-5", want: 0, wantErr: ErrNegativeAge},
        {name: "too old", input: "200", want: 0, wantErr: ErrAgeTooHigh},
        {name: "not a number", input: "abc", want: 0, wantErr: ErrInvalidFormat},
        {name: "empty", input: "", want: 0, wantErr: ErrInvalidFormat},
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            got, err := ParseAge(tt.input)

            if tt.wantErr != nil {
                if !errors.Is(err, tt.wantErr) {
                    t.Fatalf("ParseAge(%q) error = %v, want %v",
                        tt.input, err, tt.wantErr)
                }
                return
            }

            if err != nil {
                t.Fatalf("ParseAge(%q) unexpected error: %v", tt.input, err)
            }

            if got != tt.want {
                t.Errorf("ParseAge(%q) = %d, want %d", tt.input, got, tt.want)
            }
        })
    }
}
```

### 3.4 Verificacion custom con funcion

```go
// Cuando la verificacion es compleja, usa una funcion en la tabla
func TestCreateUser(t *testing.T) {
    tests := []struct {
        name    string
        input   CreateUserInput
        check   func(t *testing.T, user User, err error)
    }{
        {
            name:  "valid user",
            input: CreateUserInput{Name: "Alice", Email: "alice@example.com"},
            check: func(t *testing.T, user User, err error) {
                t.Helper()
                if err != nil {
                    t.Fatalf("unexpected error: %v", err)
                }
                if user.Name != "Alice" {
                    t.Errorf("name = %q, want %q", user.Name, "Alice")
                }
                if user.ID == "" {
                    t.Error("expected non-empty ID")
                }
                if user.CreatedAt.IsZero() {
                    t.Error("expected non-zero CreatedAt")
                }
            },
        },
        {
            name:  "missing name",
            input: CreateUserInput{Email: "bob@example.com"},
            check: func(t *testing.T, user User, err error) {
                t.Helper()
                if err == nil {
                    t.Fatal("expected error for missing name")
                }
                if !strings.Contains(err.Error(), "name") {
                    t.Errorf("error should mention 'name', got: %v", err)
                }
            },
        },
        {
            name:  "invalid email",
            input: CreateUserInput{Name: "Charlie", Email: "not-an-email"},
            check: func(t *testing.T, user User, err error) {
                t.Helper()
                if !errors.Is(err, ErrInvalidEmail) {
                    t.Errorf("error = %v, want ErrInvalidEmail", err)
                }
            },
        },
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            user, err := CreateUser(tt.input)
            tt.check(t, user, err)
        })
    }
}
```

### 3.5 Setup y teardown por caso

```go
// Cuando cada caso necesita su propio setup
func TestFileProcessor(t *testing.T) {
    tests := []struct {
        name    string
        setup   func(t *testing.T) string // retorna path al archivo
        want    int
        wantErr bool
    }{
        {
            name: "normal file",
            setup: func(t *testing.T) string {
                t.Helper()
                dir := t.TempDir()
                path := filepath.Join(dir, "test.txt")
                os.WriteFile(path, []byte("line1\nline2\nline3\n"), 0644)
                return path
            },
            want: 3,
        },
        {
            name: "empty file",
            setup: func(t *testing.T) string {
                t.Helper()
                dir := t.TempDir()
                path := filepath.Join(dir, "empty.txt")
                os.WriteFile(path, []byte(""), 0644)
                return path
            },
            want: 0,
        },
        {
            name: "nonexistent file",
            setup: func(t *testing.T) string {
                return "/nonexistent/file.txt"
            },
            wantErr: true,
        },
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            path := tt.setup(t)
            got, err := CountLines(path)

            if (err != nil) != tt.wantErr {
                t.Fatalf("error = %v, wantErr %v", err, tt.wantErr)
            }
            if !tt.wantErr && got != tt.want {
                t.Errorf("CountLines() = %d, want %d", got, tt.want)
            }
        })
    }
}
```

---

## 4. Table-driven tests con t.Parallel()

### 4.1 El patron correcto

```go
func TestProcessParallel(t *testing.T) {
    tests := []struct {
        name  string
        input string
        want  string
    }{
        {"uppercase", "hello", "HELLO"},
        {"already upper", "HELLO", "HELLO"},
        {"mixed", "HeLLo", "HELLO"},
        {"empty", "", ""},
        {"numbers", "abc123", "ABC123"},
    }

    for _, tt := range tests {
        tt := tt // CAPTURA — necesario en Go < 1.22
        t.Run(tt.name, func(t *testing.T) {
            t.Parallel() // cada caso corre en paralelo

            got := strings.ToUpper(tt.input)
            if got != tt.want {
                t.Errorf("ToUpper(%q) = %q, want %q", tt.input, got, tt.want)
            }
        })
    }
}
```

### 4.2 Por que tt := tt es necesario (Go < 1.22)

```go
// Sin captura — BUG en Go < 1.22:
for _, tt := range tests {
    t.Run(tt.name, func(t *testing.T) {
        t.Parallel()
        // Cuando este subtest se ejecuta (despues de que el loop termina),
        // tt apunta al ULTIMO elemento de tests
        // Todos los subtests verifican el mismo caso
        fmt.Println(tt.name) // imprime el ultimo nombre N veces
    })
}

// Con captura — CORRECTO:
for _, tt := range tests {
    tt := tt // crea variable LOCAL para cada iteracion
    t.Run(tt.name, func(t *testing.T) {
        t.Parallel()
        fmt.Println(tt.name) // cada subtest tiene su propia copia
    })
}

// Go 1.22+ FIX:
// Las variables de loop son per-iteration desde Go 1.22
// tt := tt ya no es estrictamente necesario
// PERO: sigue siendo buena practica incluirlo por claridad
// y compatibilidad con versiones anteriores
```

### 4.3 Parallel con recursos compartidos

```go
func TestDBQueries(t *testing.T) {
    // Setup compartido (secuencial)
    db := setupTestDB(t)

    // Seed data (secuencial, antes de que los subtests paralelos empiecen)
    db.Exec("INSERT INTO users (id, name) VALUES (1, 'Alice'), (2, 'Bob')")

    tests := []struct {
        name   string
        query  string
        wantN  int
    }{
        {"all users", "SELECT * FROM users", 2},
        {"by name", "SELECT * FROM users WHERE name = 'Alice'", 1},
        {"nonexistent", "SELECT * FROM users WHERE name = 'Nobody'", 0},
    }

    for _, tt := range tests {
        tt := tt
        t.Run(tt.name, func(t *testing.T) {
            t.Parallel() // queries de lectura pueden correr en paralelo

            rows, err := db.Query(tt.query)
            if err != nil {
                t.Fatal(err)
            }
            defer rows.Close()

            count := 0
            for rows.Next() {
                count++
            }
            if count != tt.wantN {
                t.Errorf("got %d rows, want %d", count, tt.wantN)
            }
        })
    }
}
```

---

## 5. Variaciones del patron

### 5.1 Map en vez de slice (nombres como keys)

```go
// Usar map cuando los nombres son mas importantes que el orden
func TestStatusCodes(t *testing.T) {
    tests := map[string]struct {
        code int
        want string
    }{
        "OK":           {200, "OK"},
        "Not Found":    {404, "Not Found"},
        "Server Error": {500, "Internal Server Error"},
        "Created":      {201, "Created"},
        "No Content":   {204, ""},
    }

    for name, tt := range tests {
        t.Run(name, func(t *testing.T) {
            got := http.StatusText(tt.code)
            if got != tt.want {
                t.Errorf("StatusText(%d) = %q, want %q", tt.code, got, tt.want)
            }
        })
    }
}

// NOTA: maps NO tienen orden garantizado
// Los subtests se ejecutaran en orden aleatorio
// Esto puede ser bueno (detecta dependencias entre tests)
// o malo (dificil reproducir un fallo especifico)
// go test -shuffle on tiene el mismo efecto con slices
```

### 5.2 Struct nombrado en vez de anonimo

```go
// Cuando la tabla se reutiliza o es muy grande, define el struct aparte

type parseTestCase struct {
    name    string
    input   string
    want    Token
    wantErr error
}

// Los casos se pueden definir como variable de paquete
var parseTests = []parseTestCase{
    {name: "integer", input: "42", want: Token{Type: INT, Value: "42"}},
    {name: "string", input: `"hello"`, want: Token{Type: STRING, Value: "hello"}},
    {name: "float", input: "3.14", want: Token{Type: FLOAT, Value: "3.14"}},
    {name: "invalid", input: "@", wantErr: ErrInvalidToken},
}

func TestParse(t *testing.T) {
    for _, tt := range parseTests {
        t.Run(tt.name, func(t *testing.T) {
            got, err := Parse(tt.input)
            if !errors.Is(err, tt.wantErr) {
                t.Fatalf("error = %v, want %v", err, tt.wantErr)
            }
            if tt.wantErr == nil && got != tt.want {
                t.Errorf("Parse(%q) = %v, want %v", tt.input, got, tt.want)
            }
        })
    }
}

// VENTAJA: la tabla se puede compartir entre TestParse y BenchmarkParse
func BenchmarkParse(b *testing.B) {
    for b.Loop() {
        for _, tt := range parseTests {
            Parse(tt.input)
        }
    }
}
```

### 5.3 Generando la tabla programaticamente

```go
// Cuando los casos siguen un patron, genera la tabla
func TestIsPrime(t *testing.T) {
    // Tabla manual para casos especiales
    tests := []struct {
        name string
        n    int
        want bool
    }{
        {"negative", -1, false},
        {"zero", 0, false},
        {"one", 1, false},
        {"two", 2, true},
        {"three", 3, true},
        {"four", 4, false},
    }

    // Agregar primos conocidos programaticamente
    knownPrimes := []int{5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47}
    for _, p := range knownPrimes {
        tests = append(tests, struct {
            name string
            n    int
            want bool
        }{
            name: fmt.Sprintf("prime_%d", p),
            n:    p,
            want: true,
        })
    }

    // Agregar no-primos
    nonPrimes := []int{4, 6, 8, 9, 10, 12, 14, 15, 16, 18, 20, 21, 25}
    for _, np := range nonPrimes {
        tests = append(tests, struct {
            name string
            n    int
            want bool
        }{
            name: fmt.Sprintf("composite_%d", np),
            n:    np,
            want: false,
        })
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            got := IsPrime(tt.n)
            if got != tt.want {
                t.Errorf("IsPrime(%d) = %v, want %v", tt.n, got, tt.want)
            }
        })
    }
}
```

### 5.4 Multiples outputs

```go
// Funciones que retornan multiples valores
func TestDivmod(t *testing.T) {
    tests := []struct {
        name         string
        a, b         int
        wantQuot     int
        wantRem      int
        wantErr      bool
    }{
        {name: "exact", a: 10, b: 2, wantQuot: 5, wantRem: 0},
        {name: "remainder", a: 10, b: 3, wantQuot: 3, wantRem: 1},
        {name: "negative", a: -10, b: 3, wantQuot: -3, wantRem: -1},
        {name: "zero divisor", a: 10, b: 0, wantErr: true},
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            quot, rem, err := Divmod(tt.a, tt.b)

            if (err != nil) != tt.wantErr {
                t.Fatalf("error = %v, wantErr %v", err, tt.wantErr)
            }
            if tt.wantErr {
                return
            }
            if quot != tt.wantQuot {
                t.Errorf("quotient = %d, want %d", quot, tt.wantQuot)
            }
            if rem != tt.wantRem {
                t.Errorf("remainder = %d, want %d", rem, tt.wantRem)
            }
        })
    }
}
```

### 5.5 Input complejo (structs, slices, maps)

```go
func TestSortUsers(t *testing.T) {
    tests := []struct {
        name  string
        input []User
        sortBy string
        want  []string // solo verificar nombres en orden
    }{
        {
            name: "by name asc",
            input: []User{
                {Name: "Charlie", Age: 25},
                {Name: "Alice", Age: 30},
                {Name: "Bob", Age: 20},
            },
            sortBy: "name",
            want:   []string{"Alice", "Bob", "Charlie"},
        },
        {
            name: "by age asc",
            input: []User{
                {Name: "Charlie", Age: 25},
                {Name: "Alice", Age: 30},
                {Name: "Bob", Age: 20},
            },
            sortBy: "age",
            want:   []string{"Bob", "Charlie", "Alice"},
        },
        {
            name:   "empty",
            input:  []User{},
            sortBy: "name",
            want:   []string{},
        },
        {
            name:   "single",
            input:  []User{{Name: "Solo", Age: 1}},
            sortBy: "name",
            want:   []string{"Solo"},
        },
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            got := SortUsers(tt.input, tt.sortBy)

            names := make([]string, len(got))
            for i, u := range got {
                names[i] = u.Name
            }

            if !slices.Equal(names, tt.want) {
                t.Errorf("names = %v, want %v", names, tt.want)
            }
        })
    }
}
```

---

## 6. Table-driven tests para HTTP handlers

### 6.1 Patron basico con httptest

```go
func TestGetUser(t *testing.T) {
    // Setup: crear store con datos
    store := NewMemoryStore()
    store.Save(User{ID: "1", Name: "Alice", Email: "alice@example.com"})
    store.Save(User{ID: "2", Name: "Bob", Email: "bob@example.com"})

    handler := NewUserHandler(store)

    tests := []struct {
        name       string
        method     string
        path       string
        wantStatus int
        wantBody   string // substring esperado en body
    }{
        {
            name:       "existing user",
            method:     "GET",
            path:       "/users/1",
            wantStatus: http.StatusOK,
            wantBody:   `"name":"Alice"`,
        },
        {
            name:       "not found",
            method:     "GET",
            path:       "/users/999",
            wantStatus: http.StatusNotFound,
            wantBody:   "not found",
        },
        {
            name:       "invalid id",
            method:     "GET",
            path:       "/users/abc",
            wantStatus: http.StatusBadRequest,
            wantBody:   "invalid",
        },
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            req := httptest.NewRequest(tt.method, tt.path, nil)
            rec := httptest.NewRecorder()

            handler.ServeHTTP(rec, req)

            if rec.Code != tt.wantStatus {
                t.Errorf("status = %d, want %d", rec.Code, tt.wantStatus)
            }
            if tt.wantBody != "" && !strings.Contains(rec.Body.String(), tt.wantBody) {
                t.Errorf("body = %q, want to contain %q", rec.Body.String(), tt.wantBody)
            }
        })
    }
}
```

### 6.2 POST requests con body

```go
func TestCreateUser(t *testing.T) {
    tests := []struct {
        name       string
        body       string
        wantStatus int
        checkBody  func(t *testing.T, body string)
    }{
        {
            name:       "valid",
            body:       `{"name":"Alice","email":"alice@example.com"}`,
            wantStatus: http.StatusCreated,
            checkBody: func(t *testing.T, body string) {
                t.Helper()
                var user User
                if err := json.Unmarshal([]byte(body), &user); err != nil {
                    t.Fatalf("unmarshal: %v", err)
                }
                if user.Name != "Alice" {
                    t.Errorf("name = %q, want %q", user.Name, "Alice")
                }
                if user.ID == "" {
                    t.Error("expected non-empty ID")
                }
            },
        },
        {
            name:       "missing name",
            body:       `{"email":"bob@example.com"}`,
            wantStatus: http.StatusBadRequest,
            checkBody: func(t *testing.T, body string) {
                t.Helper()
                if !strings.Contains(body, "name") {
                    t.Errorf("error should mention 'name': %s", body)
                }
            },
        },
        {
            name:       "invalid json",
            body:       `{invalid}`,
            wantStatus: http.StatusBadRequest,
            checkBody:  nil, // no verificar body
        },
        {
            name:       "empty body",
            body:       "",
            wantStatus: http.StatusBadRequest,
            checkBody:  nil,
        },
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            store := NewMemoryStore() // store fresco por test
            handler := NewUserHandler(store)

            req := httptest.NewRequest("POST", "/users",
                strings.NewReader(tt.body))
            req.Header.Set("Content-Type", "application/json")
            rec := httptest.NewRecorder()

            handler.ServeHTTP(rec, req)

            if rec.Code != tt.wantStatus {
                t.Errorf("status = %d, want %d", rec.Code, tt.wantStatus)
            }
            if tt.checkBody != nil {
                tt.checkBody(t, rec.Body.String())
            }
        })
    }
}
```

### 6.3 Headers, auth, y middleware

```go
func TestAuthMiddleware(t *testing.T) {
    tests := []struct {
        name       string
        headers    map[string]string
        wantStatus int
    }{
        {
            name:       "valid token",
            headers:    map[string]string{"Authorization": "Bearer valid-token"},
            wantStatus: http.StatusOK,
        },
        {
            name:       "missing auth header",
            headers:    map[string]string{},
            wantStatus: http.StatusUnauthorized,
        },
        {
            name:       "invalid format",
            headers:    map[string]string{"Authorization": "InvalidFormat"},
            wantStatus: http.StatusUnauthorized,
        },
        {
            name:       "expired token",
            headers:    map[string]string{"Authorization": "Bearer expired-token"},
            wantStatus: http.StatusUnauthorized,
        },
        {
            name: "with extra headers",
            headers: map[string]string{
                "Authorization": "Bearer valid-token",
                "X-Request-ID":  "test-123",
            },
            wantStatus: http.StatusOK,
        },
    }

    handler := AuthMiddleware(tokenValidator)(okHandler)

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            req := httptest.NewRequest("GET", "/protected", nil)
            for k, v := range tt.headers {
                req.Header.Set(k, v)
            }
            rec := httptest.NewRecorder()

            handler.ServeHTTP(rec, req)

            if rec.Code != tt.wantStatus {
                t.Errorf("status = %d, want %d", rec.Code, tt.wantStatus)
            }
        })
    }
}
```

---

## 7. Table-driven tests para funciones con side effects

### 7.1 Verificar que se llamo un metodo

```go
// Usar un mock recorder en la tabla
type mockNotifier struct {
    calls []string
}

func (m *mockNotifier) Notify(msg string) error {
    m.calls = append(m.calls, msg)
    return nil
}

func TestOrderService(t *testing.T) {
    tests := []struct {
        name            string
        order           Order
        wantErr         bool
        wantNotifyCount int
        wantNotifyMsg   string
    }{
        {
            name:            "valid order",
            order:           Order{ID: "o1", Total: 100, UserEmail: "alice@test.com"},
            wantErr:         false,
            wantNotifyCount: 1,
            wantNotifyMsg:   "Order o1 confirmed",
        },
        {
            name:            "zero total",
            order:           Order{ID: "o2", Total: 0},
            wantErr:         true,
            wantNotifyCount: 0, // no notifica en error
        },
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            notifier := &mockNotifier{}
            service := NewOrderService(notifier)

            err := service.PlaceOrder(tt.order)

            if (err != nil) != tt.wantErr {
                t.Fatalf("error = %v, wantErr %v", err, tt.wantErr)
            }

            if len(notifier.calls) != tt.wantNotifyCount {
                t.Errorf("notify calls = %d, want %d",
                    len(notifier.calls), tt.wantNotifyCount)
            }

            if tt.wantNotifyMsg != "" && len(notifier.calls) > 0 {
                if notifier.calls[0] != tt.wantNotifyMsg {
                    t.Errorf("notify msg = %q, want %q",
                        notifier.calls[0], tt.wantNotifyMsg)
                }
            }
        })
    }
}
```

### 7.2 Mock que retorna errores configurables

```go
type mockDB struct {
    getErr    error
    saveErr   error
    savedData []any
}

func (m *mockDB) Get(id string) (User, error) {
    if m.getErr != nil {
        return User{}, m.getErr
    }
    return User{ID: id, Name: "Test"}, nil
}

func (m *mockDB) Save(data any) error {
    if m.saveErr != nil {
        return m.saveErr
    }
    m.savedData = append(m.savedData, data)
    return nil
}

func TestUserService(t *testing.T) {
    tests := []struct {
        name     string
        db       *mockDB
        userID   string
        wantErr  bool
        checkDB  func(t *testing.T, db *mockDB)
    }{
        {
            name:   "success",
            db:     &mockDB{},
            userID: "u1",
            checkDB: func(t *testing.T, db *mockDB) {
                t.Helper()
                if len(db.savedData) != 1 {
                    t.Errorf("expected 1 save, got %d", len(db.savedData))
                }
            },
        },
        {
            name:    "db get error",
            db:      &mockDB{getErr: fmt.Errorf("connection refused")},
            userID:  "u1",
            wantErr: true,
            checkDB: func(t *testing.T, db *mockDB) {
                t.Helper()
                if len(db.savedData) != 0 {
                    t.Error("should not save on get error")
                }
            },
        },
        {
            name:    "db save error",
            db:      &mockDB{saveErr: fmt.Errorf("disk full")},
            userID:  "u1",
            wantErr: true,
        },
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            service := NewUserService(tt.db)
            err := service.ProcessUser(tt.userID)

            if (err != nil) != tt.wantErr {
                t.Fatalf("error = %v, wantErr %v", err, tt.wantErr)
            }
            if tt.checkDB != nil {
                tt.checkDB(t, tt.db)
            }
        })
    }
}
```

---

## 8. Anti-patrones y errores comunes

### 8.1 Tabla demasiado grande

```go
// MAL: tabla con 50+ casos — dificil de leer y mantener
func TestBadHuge(t *testing.T) {
    tests := []struct {
        name string
        // ... 10 campos ...
    }{
        // 50 lineas de casos...
    }
    // ...
}

// MEJOR: dividir en multiples tests por categoria
func TestParse_ValidInputs(t *testing.T) {
    tests := []struct{ name, input, want string }{
        {"integer", "42", "42"},
        {"float", "3.14", "3.14"},
        {"string", `"hello"`, "hello"},
    }
    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) { /* ... */ })
    }
}

func TestParse_InvalidInputs(t *testing.T) {
    tests := []struct{ name, input string }{
        {"empty", ""},
        {"invalid char", "@"},
        {"unterminated string", `"hello`},
    }
    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            _, err := Parse(tt.input)
            if err == nil {
                t.Errorf("Parse(%q) should return error", tt.input)
            }
        })
    }
}
```

### 8.2 Logica en la tabla

```go
// MAL: calcular el expected dentro de la tabla
tests := []struct {
    name string
    a, b int
    want int
}{
    {"case1", 2, 3, 2 + 3},      // MAL: la tabla recalcula la logica
    {"case2", 5, 7, 5 + 7},      // Si Add() esta buggy, el test tambien
}

// BIEN: valores literales (hard-coded)
tests := []struct {
    name string
    a, b int
    want int
}{
    {"case1", 2, 3, 5},   // BIEN: valor literal
    {"case2", 5, 7, 12},  // BIEN: calculado manualmente
}
```

### 8.3 Nombre poco descriptivo

```go
// MAL: nombres genericos
tests := []struct {
    name string
    input int
    want  string
}{
    {"test1", 1, "one"},
    {"test2", 2, "two"},
    {"case_a", -1, "negative one"},
}

// BIEN: nombres que describen el escenario
tests := []struct {
    name string
    input int
    want  string
}{
    {"single digit", 1, "one"},
    {"double digit", 12, "twelve"},
    {"negative number", -1, "negative one"},
    {"zero", 0, "zero"},
    {"large number", 1000000, "one million"},
}

// Los nombres aparecen en la salida de go test -v:
// --- PASS: TestNumberToWords/single_digit (0.00s)
// --- FAIL: TestNumberToWords/negative_number (0.00s)
// Un buen nombre te dice INMEDIATAMENTE que escenario fallo
```

### 8.4 No verificar que el error path funciona

```go
// MAL: solo testear el happy path
tests := []struct {
    name string
    input int
    want  int
}{
    {"double 5", 5, 10},
    {"double 0", 0, 0},
}

// BIEN: testear AMBOS paths
tests := []struct {
    name    string
    input   int
    want    int
    wantErr bool
}{
    {"valid positive", 5, 10, false},
    {"valid zero", 0, 0, false},
    {"negative input", -1, 0, true},      // error path
    {"overflow", math.MaxInt, 0, true},    // edge case
}
```

### 8.5 Tabla cuando no es necesaria

```go
// MAL: forzar tabla para un solo caso o casos muy diferentes
func TestOverEngineered(t *testing.T) {
    tests := []struct {
        name string
        fn   func() error
    }{
        {"connect", func() error { return Connect() }},
        {"query", func() error { return Query("SELECT 1") }},
        {"close", func() error { return Close() }},
    }
    // Estos tests tienen logica muy diferente — no comparten estructura
}

// MEJOR: subtests simples
func TestLifecycle(t *testing.T) {
    t.Run("connect", func(t *testing.T) {
        if err := Connect(); err != nil {
            t.Fatal(err)
        }
    })
    t.Run("query", func(t *testing.T) {
        rows, err := Query("SELECT 1")
        if err != nil {
            t.Fatal(err)
        }
        if rows != 1 {
            t.Errorf("expected 1 row, got %d", rows)
        }
    })
}

// REGLA: usa tabla cuando los casos comparten ESTRUCTURA (mismos campos)
// Usa subtests cuando los casos tienen LOGICA diferente
```

---

## 9. Table-driven tests con genericos (Go 1.18+)

### 9.1 Helper generico para tablas

```go
// RunTable ejecuta una tabla de tests de forma generica
func RunTable[In any, Out comparable](
    t *testing.T,
    fn func(In) Out,
    tests []struct {
        name string
        input In
        want  Out
    },
) {
    t.Helper()
    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            t.Helper()
            got := fn(tt.input)
            if got != tt.want {
                t.Errorf("got %v, want %v", got, tt.want)
            }
        })
    }
}

// Uso:
func TestDouble(t *testing.T) {
    RunTable(t, Double, []struct {
        name  string
        input int
        want  int
    }{
        {"positive", 5, 10},
        {"negative", -3, -6},
        {"zero", 0, 0},
    })
}

// NOTA: este patron es elegante pero tiene limitaciones:
// - No soporta funciones con error
// - No soporta multiples outputs
// - El struct anonimo es verboso
// En la practica, escribir la tabla directamente es mas flexible
```

### 9.2 Table-driven con funciones que retornan error

```go
func RunTableWithError[In any, Out comparable](
    t *testing.T,
    fn func(In) (Out, error),
    tests []struct {
        name    string
        input   In
        want    Out
        wantErr bool
    },
) {
    t.Helper()
    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            t.Helper()
            got, err := fn(tt.input)
            if (err != nil) != tt.wantErr {
                t.Fatalf("error = %v, wantErr %v", err, tt.wantErr)
            }
            if !tt.wantErr && got != tt.want {
                t.Errorf("got %v, want %v", got, tt.want)
            }
        })
    }
}
```

---

## 10. Comparacion con C y Rust

### 10.1 Table-driven en otros lenguajes

```c
// C — simulacion con arrays de structs (manual, sin framework)
typedef struct {
    const char* name;
    int a, b, expected;
} test_case;

void test_add(void) {
    test_case tests[] = {
        {"positive", 2, 3, 5},
        {"negative", -1, -2, -3},
        {"zero", 0, 0, 0},
    };
    int n = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < n; i++) {
        int got = add(tests[i].a, tests[i].b);
        if (got != tests[i].expected) {
            printf("FAIL %s: add(%d,%d) = %d, want %d\n",
                tests[i].name, tests[i].a, tests[i].b, got, tests[i].expected);
        }
    }
}
// Problemas: sin subtests, sin reporte automatico, sin t.Run
```

```rust
// Rust — con rstest (crate para parametrized tests)
use rstest::rstest;

#[rstest]
#[case("positive", 2, 3, 5)]
#[case("negative", -1, -2, -3)]
#[case("zero", 0, 0, 0)]
fn test_add(#[case] name: &str, #[case] a: i32, #[case] b: i32, #[case] expected: i32) {
    assert_eq!(add(a, b), expected, "case: {name}");
}

// Sin rstest (manual, como Go):
#[test]
fn test_add_table() {
    let cases = vec![
        ("positive", 2, 3, 5),
        ("negative", -1, -2, -3),
        ("zero", 0, 0, 0),
    ];

    for (name, a, b, expected) in cases {
        let got = add(a, b);
        assert_eq!(got, expected, "case '{name}': add({a}, {b}) = {got}, want {expected}");
    }
}
// Nota: Rust no tiene subtests nativos
// rstest genera una funcion test por cada #[case]
```

```go
// Go — el patron nativo
func TestAdd(t *testing.T) {
    tests := []struct {
        name string
        a, b int
        want int
    }{
        {"positive", 2, 3, 5},
        {"negative", -1, -2, -3},
        {"zero", 0, 0, 0},
    }
    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            got := Add(tt.a, tt.b)
            if got != tt.want {
                t.Errorf("Add(%d, %d) = %d, want %d", tt.a, tt.b, got, tt.want)
            }
        })
    }
}
```

### 10.2 Comparacion

| Aspecto | C | Go | Rust |
|---|---|---|---|
| Patron nativo | No (manual) | Si (stdlib) | No (rstest crate) |
| Subtests | No | `t.Run()` | `#[case]` via rstest |
| Filtrar un caso | No | `go test -run Test/case` | Nombre de funcion generada |
| Struct anonimo | No (typedef) | Si | No (tuples o struct) |
| Loop sobre tabla | `for` manual | `for range` + `t.Run` | `for` + `assert!` |
| Error messages | `printf` manual | `t.Errorf` con formato | `assert_eq!` con message |
| Parallelismo | Manual (threads) | `t.Parallel()` | Por defecto en cargo |
| Generacion | Manual | Manual o codegen | `#[case]` macro |
| Output | Manual | `go test -v` | `cargo test -- --nocapture` |

---

## 11. Programa completo: validador con table-driven tests

```go
// ============================================================================
// Archivo: validator/validator.go
// ============================================================================

package validator

import (
    "errors"
    "fmt"
    "net/mail"
    "regexp"
    "strings"
    "time"
    "unicode/utf8"
)

var (
    ErrRequired     = errors.New("field is required")
    ErrTooShort     = errors.New("value is too short")
    ErrTooLong      = errors.New("value is too long")
    ErrInvalidEmail = errors.New("invalid email address")
    ErrOutOfRange   = errors.New("value is out of range")
    ErrInvalidDate  = errors.New("invalid date")
    ErrFutureDate   = errors.New("date cannot be in the future")
    ErrPatternMatch = errors.New("value does not match pattern")
    ErrNotUnique    = errors.New("value is not unique")
)

// ValidateRequired verifica que el string no este vacio
func ValidateRequired(field, value string) error {
    if strings.TrimSpace(value) == "" {
        return fmt.Errorf("%s: %w", field, ErrRequired)
    }
    return nil
}

// ValidateLength verifica longitud del string (en runes)
func ValidateLength(field, value string, min, max int) error {
    length := utf8.RuneCountInString(value)
    if length < min {
        return fmt.Errorf("%s: %w (min %d, got %d)", field, ErrTooShort, min, length)
    }
    if length > max {
        return fmt.Errorf("%s: %w (max %d, got %d)", field, ErrTooLong, max, length)
    }
    return nil
}

// ValidateEmail verifica formato de email
func ValidateEmail(field, value string) error {
    _, err := mail.ParseAddress(value)
    if err != nil {
        return fmt.Errorf("%s: %w", field, ErrInvalidEmail)
    }
    return nil
}

// ValidateRange verifica que un int este en [min, max]
func ValidateRange(field string, value, min, max int) error {
    if value < min || value > max {
        return fmt.Errorf("%s: %w (%d not in [%d, %d])",
            field, ErrOutOfRange, value, min, max)
    }
    return nil
}

// ValidateDate parsea y valida una fecha en formato YYYY-MM-DD
func ValidateDate(field, value string) (time.Time, error) {
    t, err := time.Parse("2006-01-02", value)
    if err != nil {
        return time.Time{}, fmt.Errorf("%s: %w (%s)", field, ErrInvalidDate, value)
    }
    return t, nil
}

// ValidateNotFuture verifica que la fecha no sea futura
func ValidateNotFuture(field string, value time.Time) error {
    if value.After(time.Now()) {
        return fmt.Errorf("%s: %w", field, ErrFutureDate)
    }
    return nil
}

// ValidatePattern verifica contra una regex
func ValidatePattern(field, value, pattern string) error {
    matched, err := regexp.MatchString(pattern, value)
    if err != nil {
        return fmt.Errorf("%s: invalid pattern: %v", field, err)
    }
    if !matched {
        return fmt.Errorf("%s: %w", field, ErrPatternMatch)
    }
    return nil
}

// ValidateUnique verifica que no haya duplicados en un slice
func ValidateUnique[T comparable](field string, values []T) error {
    seen := make(map[T]struct{})
    for _, v := range values {
        if _, ok := seen[v]; ok {
            return fmt.Errorf("%s: %w (duplicate: %v)", field, ErrNotUnique, v)
        }
        seen[v] = struct{}{}
    }
    return nil
}
```

```go
// ============================================================================
// Archivo: validator/validator_test.go
// ============================================================================

package validator

import (
    "errors"
    "testing"
    "time"
)

// ============================================================================
// Test: ValidateRequired
// ============================================================================

func TestValidateRequired(t *testing.T) {
    tests := []struct {
        name    string
        field   string
        value   string
        wantErr error
    }{
        {name: "non-empty", field: "name", value: "Alice", wantErr: nil},
        {name: "empty", field: "name", value: "", wantErr: ErrRequired},
        {name: "whitespace only", field: "name", value: "   ", wantErr: ErrRequired},
        {name: "tab only", field: "name", value: "\t", wantErr: ErrRequired},
        {name: "newline only", field: "name", value: "\n", wantErr: ErrRequired},
        {name: "with spaces", field: "name", value: " Alice ", wantErr: nil},
        {name: "single char", field: "name", value: "a", wantErr: nil},
        {name: "unicode", field: "name", value: "日本", wantErr: nil},
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            err := ValidateRequired(tt.field, tt.value)
            if tt.wantErr == nil {
                if err != nil {
                    t.Errorf("ValidateRequired(%q, %q) = %v, want nil",
                        tt.field, tt.value, err)
                }
                return
            }
            if !errors.Is(err, tt.wantErr) {
                t.Errorf("ValidateRequired(%q, %q) = %v, want %v",
                    tt.field, tt.value, err, tt.wantErr)
            }
        })
    }
}

// ============================================================================
// Test: ValidateLength
// ============================================================================

func TestValidateLength(t *testing.T) {
    tests := []struct {
        name    string
        value   string
        min     int
        max     int
        wantErr error
    }{
        {name: "within range", value: "hello", min: 1, max: 10, wantErr: nil},
        {name: "exact min", value: "ab", min: 2, max: 10, wantErr: nil},
        {name: "exact max", value: "abcde", min: 1, max: 5, wantErr: nil},
        {name: "too short", value: "a", min: 2, max: 10, wantErr: ErrTooShort},
        {name: "too long", value: "hello world", min: 1, max: 5, wantErr: ErrTooLong},
        {name: "empty below min", value: "", min: 1, max: 10, wantErr: ErrTooShort},
        {name: "empty with min 0", value: "", min: 0, max: 10, wantErr: nil},
        {name: "unicode chars", value: "日本語", min: 1, max: 5, wantErr: nil},
        {name: "unicode too long", value: "日本語テスト文字列", min: 1, max: 5, wantErr: ErrTooLong},
        {name: "emoji", value: "🚀🌍", min: 1, max: 3, wantErr: nil},
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            err := ValidateLength("field", tt.value, tt.min, tt.max)
            if tt.wantErr == nil {
                if err != nil {
                    t.Errorf("ValidateLength(%q, %d, %d) = %v, want nil",
                        tt.value, tt.min, tt.max, err)
                }
                return
            }
            if !errors.Is(err, tt.wantErr) {
                t.Errorf("ValidateLength(%q, %d, %d) = %v, want %v",
                    tt.value, tt.min, tt.max, err, tt.wantErr)
            }
        })
    }
}

// ============================================================================
// Test: ValidateEmail
// ============================================================================

func TestValidateEmail(t *testing.T) {
    tests := []struct {
        name    string
        value   string
        wantErr error
    }{
        {name: "valid simple", value: "alice@example.com", wantErr: nil},
        {name: "valid with dots", value: "alice.bob@example.com", wantErr: nil},
        {name: "valid with plus", value: "alice+tag@example.com", wantErr: nil},
        {name: "valid subdomain", value: "alice@mail.example.com", wantErr: nil},
        {name: "missing @", value: "aliceexample.com", wantErr: ErrInvalidEmail},
        {name: "missing domain", value: "alice@", wantErr: ErrInvalidEmail},
        {name: "missing local", value: "@example.com", wantErr: ErrInvalidEmail},
        {name: "empty", value: "", wantErr: ErrInvalidEmail},
        {name: "spaces", value: "alice @example.com", wantErr: ErrInvalidEmail},
        {name: "double @", value: "alice@@example.com", wantErr: ErrInvalidEmail},
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            err := ValidateEmail("email", tt.value)
            if tt.wantErr == nil {
                if err != nil {
                    t.Errorf("ValidateEmail(%q) = %v, want nil", tt.value, err)
                }
                return
            }
            if !errors.Is(err, tt.wantErr) {
                t.Errorf("ValidateEmail(%q) = %v, want %v",
                    tt.value, err, tt.wantErr)
            }
        })
    }
}

// ============================================================================
// Test: ValidateRange
// ============================================================================

func TestValidateRange(t *testing.T) {
    tests := []struct {
        name    string
        value   int
        min     int
        max     int
        wantErr error
    }{
        {name: "within range", value: 5, min: 1, max: 10, wantErr: nil},
        {name: "at min", value: 1, min: 1, max: 10, wantErr: nil},
        {name: "at max", value: 10, min: 1, max: 10, wantErr: nil},
        {name: "below min", value: 0, min: 1, max: 10, wantErr: ErrOutOfRange},
        {name: "above max", value: 11, min: 1, max: 10, wantErr: ErrOutOfRange},
        {name: "negative range", value: -5, min: -10, max: -1, wantErr: nil},
        {name: "zero in range", value: 0, min: -5, max: 5, wantErr: nil},
        {name: "min equals max", value: 5, min: 5, max: 5, wantErr: nil},
        {name: "min equals max miss", value: 6, min: 5, max: 5, wantErr: ErrOutOfRange},
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            err := ValidateRange("age", tt.value, tt.min, tt.max)
            if tt.wantErr == nil {
                if err != nil {
                    t.Errorf("ValidateRange(%d, %d, %d) = %v, want nil",
                        tt.value, tt.min, tt.max, err)
                }
                return
            }
            if !errors.Is(err, tt.wantErr) {
                t.Errorf("ValidateRange(%d, %d, %d) = %v, want %v",
                    tt.value, tt.min, tt.max, err, tt.wantErr)
            }
        })
    }
}

// ============================================================================
// Test: ValidateDate
// ============================================================================

func TestValidateDate(t *testing.T) {
    tests := []struct {
        name    string
        value   string
        wantErr error
        wantY   int // expected year (0 = don't check)
        wantM   time.Month
        wantD   int
    }{
        {
            name: "valid", value: "2024-03-15",
            wantY: 2024, wantM: time.March, wantD: 15,
        },
        {
            name: "new year", value: "2025-01-01",
            wantY: 2025, wantM: time.January, wantD: 1,
        },
        {
            name: "leap day", value: "2024-02-29",
            wantY: 2024, wantM: time.February, wantD: 29,
        },
        {
            name: "invalid format", value: "03/15/2024",
            wantErr: ErrInvalidDate,
        },
        {
            name: "invalid month", value: "2024-13-01",
            wantErr: ErrInvalidDate,
        },
        {
            name: "invalid day", value: "2024-02-30",
            wantErr: ErrInvalidDate,
        },
        {
            name: "non-leap year feb 29", value: "2023-02-29",
            wantErr: ErrInvalidDate,
        },
        {
            name: "empty", value: "",
            wantErr: ErrInvalidDate,
        },
        {
            name: "just year", value: "2024",
            wantErr: ErrInvalidDate,
        },
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            got, err := ValidateDate("birthday", tt.value)

            if tt.wantErr != nil {
                if !errors.Is(err, tt.wantErr) {
                    t.Errorf("ValidateDate(%q) error = %v, want %v",
                        tt.value, err, tt.wantErr)
                }
                return
            }

            if err != nil {
                t.Fatalf("ValidateDate(%q) unexpected error: %v", tt.value, err)
            }

            if tt.wantY != 0 {
                y, m, d := got.Date()
                if y != tt.wantY || m != tt.wantM || d != tt.wantD {
                    t.Errorf("ValidateDate(%q) = %d-%02d-%02d, want %d-%02d-%02d",
                        tt.value, y, m, d, tt.wantY, tt.wantM, tt.wantD)
                }
            }
        })
    }
}

// ============================================================================
// Test: ValidatePattern
// ============================================================================

func TestValidatePattern(t *testing.T) {
    tests := []struct {
        name    string
        value   string
        pattern string
        wantErr error
    }{
        {name: "alphanumeric match", value: "hello123", pattern: `^[a-zA-Z0-9]+$`, wantErr: nil},
        {name: "alphanumeric no match", value: "hello 123", pattern: `^[a-zA-Z0-9]+$`, wantErr: ErrPatternMatch},
        {name: "phone number", value: "+1-555-1234", pattern: `^\+\d{1,3}-\d{3}-\d{4}$`, wantErr: nil},
        {name: "phone invalid", value: "555-1234", pattern: `^\+\d{1,3}-\d{3}-\d{4}$`, wantErr: ErrPatternMatch},
        {name: "any string", value: "anything", pattern: `.*`, wantErr: nil},
        {name: "empty vs non-empty", value: "", pattern: `.+`, wantErr: ErrPatternMatch},
        {name: "slug format", value: "my-cool-post", pattern: `^[a-z0-9]+(-[a-z0-9]+)*$`, wantErr: nil},
        {name: "slug with caps", value: "My-Cool-Post", pattern: `^[a-z0-9]+(-[a-z0-9]+)*$`, wantErr: ErrPatternMatch},
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            err := ValidatePattern("field", tt.value, tt.pattern)
            if tt.wantErr == nil {
                if err != nil {
                    t.Errorf("ValidatePattern(%q, %q) = %v, want nil",
                        tt.value, tt.pattern, err)
                }
                return
            }
            if !errors.Is(err, tt.wantErr) {
                t.Errorf("ValidatePattern(%q, %q) = %v, want %v",
                    tt.value, tt.pattern, err, tt.wantErr)
            }
        })
    }
}

// ============================================================================
// Test: ValidateUnique (genericos)
// ============================================================================

func TestValidateUnique(t *testing.T) {
    t.Run("strings", func(t *testing.T) {
        tests := []struct {
            name    string
            values  []string
            wantErr error
        }{
            {name: "all unique", values: []string{"a", "b", "c"}, wantErr: nil},
            {name: "with duplicate", values: []string{"a", "b", "a"}, wantErr: ErrNotUnique},
            {name: "empty", values: []string{}, wantErr: nil},
            {name: "single", values: []string{"a"}, wantErr: nil},
            {name: "all same", values: []string{"x", "x", "x"}, wantErr: ErrNotUnique},
        }

        for _, tt := range tests {
            t.Run(tt.name, func(t *testing.T) {
                err := ValidateUnique("tags", tt.values)
                if tt.wantErr == nil {
                    if err != nil {
                        t.Errorf("ValidateUnique(%v) = %v, want nil", tt.values, err)
                    }
                    return
                }
                if !errors.Is(err, tt.wantErr) {
                    t.Errorf("ValidateUnique(%v) = %v, want %v", tt.values, err, tt.wantErr)
                }
            })
        }
    })

    t.Run("ints", func(t *testing.T) {
        tests := []struct {
            name    string
            values  []int
            wantErr error
        }{
            {name: "unique ints", values: []int{1, 2, 3}, wantErr: nil},
            {name: "dup ints", values: []int{1, 2, 1}, wantErr: ErrNotUnique},
        }

        for _, tt := range tests {
            t.Run(tt.name, func(t *testing.T) {
                err := ValidateUnique("ids", tt.values)
                if tt.wantErr == nil && err != nil {
                    t.Errorf("unexpected error: %v", err)
                }
                if tt.wantErr != nil && !errors.Is(err, tt.wantErr) {
                    t.Errorf("got %v, want %v", err, tt.wantErr)
                }
            })
        }
    })
}

// ============================================================================
// Test: Integration (multiples validaciones juntas)
// ============================================================================

func TestValidateUser(t *testing.T) {
    type userInput struct {
        Name     string
        Email    string
        Age      int
        Birthday string
        Tags     []string
    }

    tests := []struct {
        name       string
        input      userInput
        wantErrors []error // errores sentinel esperados
    }{
        {
            name: "valid user",
            input: userInput{
                Name: "Alice", Email: "alice@example.com",
                Age: 30, Birthday: "1994-05-15", Tags: []string{"go", "rust"},
            },
            wantErrors: nil,
        },
        {
            name: "empty name",
            input: userInput{
                Name: "", Email: "alice@example.com",
                Age: 30, Birthday: "1994-05-15", Tags: []string{"go"},
            },
            wantErrors: []error{ErrRequired},
        },
        {
            name: "invalid email and age",
            input: userInput{
                Name: "Bob", Email: "not-email",
                Age: -5, Birthday: "1990-01-01", Tags: []string{"go"},
            },
            wantErrors: []error{ErrInvalidEmail, ErrOutOfRange},
        },
        {
            name: "duplicate tags",
            input: userInput{
                Name: "Charlie", Email: "charlie@example.com",
                Age: 25, Birthday: "1999-01-01", Tags: []string{"go", "go"},
            },
            wantErrors: []error{ErrNotUnique},
        },
        {
            name: "everything wrong",
            input: userInput{
                Name: "", Email: "bad", Age: 200,
                Birthday: "not-a-date", Tags: []string{"a", "a"},
            },
            wantErrors: []error{ErrRequired, ErrInvalidEmail, ErrOutOfRange, ErrInvalidDate, ErrNotUnique},
        },
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            var errs []error

            if err := ValidateRequired("name", tt.input.Name); err != nil {
                errs = append(errs, err)
            }
            if err := ValidateEmail("email", tt.input.Email); err != nil {
                errs = append(errs, err)
            }
            if err := ValidateRange("age", tt.input.Age, 0, 150); err != nil {
                errs = append(errs, err)
            }
            if _, err := ValidateDate("birthday", tt.input.Birthday); err != nil {
                errs = append(errs, err)
            }
            if err := ValidateUnique("tags", tt.input.Tags); err != nil {
                errs = append(errs, err)
            }

            // Verificar cantidad de errores
            if len(errs) != len(tt.wantErrors) {
                t.Fatalf("got %d errors, want %d\ngot: %v\nwant: %v",
                    len(errs), len(tt.wantErrors), errs, tt.wantErrors)
            }

            // Verificar que cada error esperado esta presente
            for i, wantErr := range tt.wantErrors {
                if !errors.Is(errs[i], wantErr) {
                    t.Errorf("error[%d] = %v, want %v", i, errs[i], wantErr)
                }
            }
        })
    }
}

// ============================================================================
// Test: Parallel table-driven
// ============================================================================

func TestValidateRequired_Parallel(t *testing.T) {
    tests := []struct {
        name    string
        value   string
        wantErr bool
    }{
        {"non-empty", "hello", false},
        {"empty", "", true},
        {"spaces", "   ", true},
        {"tab", "\t\t", true},
        {"valid", "x", false},
    }

    for _, tt := range tests {
        tt := tt
        t.Run(tt.name, func(t *testing.T) {
            t.Parallel()

            err := ValidateRequired("field", tt.value)
            gotErr := err != nil

            if gotErr != tt.wantErr {
                t.Errorf("ValidateRequired(%q) error=%v, wantErr=%v",
                    tt.value, err, tt.wantErr)
            }
        })
    }
}
```

```
$ go test -v ./validator/

=== RUN   TestValidateRequired
=== RUN   TestValidateRequired/non-empty
=== RUN   TestValidateRequired/empty
=== RUN   TestValidateRequired/whitespace_only
=== RUN   TestValidateRequired/tab_only
=== RUN   TestValidateRequired/newline_only
=== RUN   TestValidateRequired/with_spaces
=== RUN   TestValidateRequired/single_char
=== RUN   TestValidateRequired/unicode
--- PASS: TestValidateRequired (0.00s)
    --- PASS: TestValidateRequired/non-empty (0.00s)
    --- PASS: TestValidateRequired/empty (0.00s)
    ...
=== RUN   TestValidateLength
=== RUN   TestValidateLength/within_range
=== RUN   TestValidateLength/exact_min
=== RUN   TestValidateLength/exact_max
=== RUN   TestValidateLength/too_short
=== RUN   TestValidateLength/too_long
=== RUN   TestValidateLength/empty_below_min
=== RUN   TestValidateLength/empty_with_min_0
=== RUN   TestValidateLength/unicode_chars
=== RUN   TestValidateLength/unicode_too_long
=== RUN   TestValidateLength/emoji
--- PASS: TestValidateLength (0.00s)
    ...
=== RUN   TestValidateEmail
    --- PASS: TestValidateEmail (0.00s)
=== RUN   TestValidateRange
    --- PASS: TestValidateRange (0.00s)
=== RUN   TestValidateDate
=== RUN   TestValidateDate/valid
=== RUN   TestValidateDate/leap_day
=== RUN   TestValidateDate/non-leap_year_feb_29
    --- PASS: TestValidateDate (0.00s)
=== RUN   TestValidatePattern
    --- PASS: TestValidatePattern (0.00s)
=== RUN   TestValidateUnique
=== RUN   TestValidateUnique/strings
=== RUN   TestValidateUnique/ints
    --- PASS: TestValidateUnique (0.00s)
=== RUN   TestValidateUser
=== RUN   TestValidateUser/valid_user
=== RUN   TestValidateUser/empty_name
=== RUN   TestValidateUser/invalid_email_and_age
=== RUN   TestValidateUser/duplicate_tags
=== RUN   TestValidateUser/everything_wrong
--- PASS: TestValidateUser (0.00s)
    ...
=== RUN   TestValidateRequired_Parallel
=== RUN   TestValidateRequired_Parallel/non-empty
=== PAUSE TestValidateRequired_Parallel/non-empty
=== RUN   TestValidateRequired_Parallel/empty
=== PAUSE TestValidateRequired_Parallel/empty
...
=== CONT  TestValidateRequired_Parallel/non-empty
=== CONT  TestValidateRequired_Parallel/spaces
=== CONT  TestValidateRequired_Parallel/empty
--- PASS: TestValidateRequired_Parallel (0.00s)
    ...
PASS
ok      mymodule/validator      0.004s
```

**Puntos clave del programa**:

1. **Una funcion = una tabla**: cada funcion de validacion tiene su propia tabla. `TestValidateRequired` tiene 8 casos, `TestValidateLength` tiene 10, `TestValidateEmail` tiene 10, etc.

2. **Error sentinels con `errors.Is`**: los tests verifican errores tipados (`ErrRequired`, `ErrTooShort`, `ErrInvalidEmail`) usando `errors.Is()`, no comparacion de strings.

3. **Boundary testing**: cada tabla cubre los bordes — valor exacto al minimo, al maximo, justo debajo, justo encima, zero, empty.

4. **Integration test como tabla**: `TestValidateUser` demuestra que la tabla puede tener multiples errores esperados — un slice de errores sentinel.

5. **Genericos en table tests**: `TestValidateUnique` tiene sub-tablas para `[]string` y `[]int`, demostrando que la misma funcion generica se puede testear con diferentes tipos.

6. **Parallel**: `TestValidateRequired_Parallel` muestra el patron correcto con `tt := tt` y `t.Parallel()`.

---

## 12. Ejercicios

### Ejercicio 1: Table-driven tests para un parser de URLs

Escribe tests para una funcion `ParseURL(raw string) (URL, error)` que extraiga scheme, host, port, path, y query. La tabla debe cubrir:
1. URL completa con todos los componentes
2. URL sin port (default 80/443)
3. URL sin path
4. URL con query string
5. URL invalida (sin scheme)
6. URL vacia
7. URL con caracteres especiales en path
8. Al menos 15 casos en la tabla

### Ejercicio 2: Table-driven tests para HTTP API

Escribe table-driven tests para un endpoint CRUD (`/items`):
1. `GET /items` — listar (vacio, con datos)
2. `GET /items/{id}` — obtener (existente, no existente, id invalido)
3. `POST /items` — crear (valido, JSON invalido, campos faltantes)
4. `PUT /items/{id}` — actualizar (existente, no existente)
5. `DELETE /items/{id}` — eliminar (existente, no existente)

Cada tabla debe tener: name, method, path, body, wantStatus, y checkBody.

### Ejercicio 3: Table-driven tests paralelos para funciones puras

Dado un set de funciones puras de string (Reverse, ToTitle, Slugify, Truncate):
1. Escribe tablas con al menos 10 casos por funcion
2. Todos los subtests deben usar `t.Parallel()`
3. Incluir la captura `tt := tt`
4. Medir el tiempo: comparar ejecucion con y sin `t.Parallel()`

### Ejercicio 4: Generacion de tablas

Implementa una funcion `FizzBuzz(n int) string` y:
1. Escribe 5 casos manuales en la tabla
2. Genera 100 casos programaticamente con un loop
3. Agrega edge cases (0, negativos, numeros grandes)
4. Usa un golden file (`testdata/fizzbuzz.golden`) con la salida esperada de FizzBuzz(1) a FizzBuzz(100)
5. Implementa el flag `-update` para regenerar el golden file

---

> **Siguiente**: C11/S02 - Testing, T03 - Benchmarks — testing.B, go test -bench, b.ResetTimer, b.ReportAllocs, pprof
