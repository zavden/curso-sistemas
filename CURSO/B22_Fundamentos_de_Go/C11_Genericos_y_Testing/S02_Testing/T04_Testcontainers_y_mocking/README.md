# Testcontainers y Mocking — interfaces para testing, dependency injection, testcontainers-go

## 1. Introduccion

Go no tiene un framework de mocking "magico" como Mockito en Java o unittest.mock en Python. En Go, el mocking se basa en un principio fundamental del lenguaje: **las interfaces se satisfacen implicitamente**. Esto significa que cualquier tipo que implemente los metodos de una interfaz la satisface sin declararlo — y eso hace que crear test doubles sea trivial. Combina esto con dependency injection (que en Go es simplemente "pasar interfaces como parametros") y tienes un sistema de testing poderoso sin necesidad de frameworks.

Para integration tests que requieren infraestructura real (bases de datos, caches, message brokers), **testcontainers-go** levanta contenedores Docker efimeros que viven solo durante el test. No mas "funciona en mi maquina" ni mocks que divergen del comportamiento real.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│               MOCKING Y TESTING EN GO — VISION GENERAL                          │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  NIVELES DE TEST                                                                │
│  ────────────────                                                               │
│                                                                                  │
│  Unit Tests          → mocks/stubs/fakes manuales via interfaces                │
│  Integration Tests   → testcontainers (Docker), bases de datos reales           │
│  End-to-End Tests    → compose, staging environments                            │
│                                                                                  │
│  ESTRATEGIA DE MOCKING                                                          │
│  ─────────────────────                                                          │
│                                                                                  │
│  Interfaces pequeñas    → definidas por el consumidor, no el productor          │
│  Dependency injection   → pasar dependencias como parametros/campos             │
│  Test doubles manuales  → structs que implementan la interfaz                   │
│  Generacion automatica  → gomock/mockgen, moq, counterfeiter                   │
│                                                                                  │
│  TESTCONTAINERS                                                                 │
│  ────────────────                                                               │
│                                                                                  │
│  testcontainers-go   → contenedores Docker efimeros para tests                  │
│  Modulos             → PostgreSQL, Redis, MySQL, Kafka, etc.                    │
│  Lifecycle           → crear → configurar → start → test → terminate            │
│                                                                                  │
│  FLUJO                                                                          │
│  ─────                                                                          │
│                                                                                  │
│  ┌─────────┐    ┌──────────┐    ┌──────────┐    ┌───────────────┐              │
│  │ Diseñar │───→│ Inyectar │───→│ Mockear  │───→│ Integration   │              │
│  │interfaz │    │ depend.  │    │ en unit  │    │ con container │              │
│  └─────────┘    └──────────┘    └──────────┘    └───────────────┘              │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Interfaces para testing — el fundamento

### 2.1 El principio: interfaces implicitas

En Go, una interfaz define un conjunto de metodos. Cualquier tipo que tenga esos metodos la satisface **sin declaracion explicita**. Esto es radicalmente diferente a Java (`implements`) o Rust (`impl Trait for Type`):

```go
// La interfaz — define el contrato
type UserStore interface {
    GetUser(id string) (User, error)
    SaveUser(user User) error
}

// Implementacion real — satisface UserStore sin declararlo
type PostgresUserStore struct {
    db *sql.DB
}

func (s *PostgresUserStore) GetUser(id string) (User, error) {
    var u User
    err := s.db.QueryRow("SELECT id, name, email FROM users WHERE id=$1", id).
        Scan(&u.ID, &u.Name, &u.Email)
    return u, err
}

func (s *PostgresUserStore) SaveUser(user User) error {
    _, err := s.db.Exec(
        "INSERT INTO users (id, name, email) VALUES ($1, $2, $3)",
        user.ID, user.Name, user.Email,
    )
    return err
}

// Mock — tambien satisface UserStore sin declararlo
type mockUserStore struct {
    users map[string]User
    err   error
}

func (m *mockUserStore) GetUser(id string) (User, error) {
    if m.err != nil {
        return User{}, m.err
    }
    u, ok := m.users[id]
    if !ok {
        return User{}, fmt.Errorf("user %s not found", id)
    }
    return u, nil
}

func (m *mockUserStore) SaveUser(user User) error {
    if m.err != nil {
        return m.err
    }
    m.users[user.ID] = user
    return nil
}
```

El compilador verifica en compile time que ambos tipos satisfacen `UserStore`. No hay posibilidad de que el mock "se desincronice" con la interfaz real — si cambias la interfaz, el mock deja de compilar.

### 2.2 Interfaces pequeñas — la regla de Go

Go favorece interfaces pequeñas, a menudo de un solo metodo. La stdlib esta llena de ejemplos:

```go
// stdlib — interfaces de 1 metodo
type Reader interface { Read(p []byte) (n int, err error) }
type Writer interface { Write(p []byte) (n int, err error) }
type Closer interface { Close() error }
type Stringer interface { String() string }
type Handler interface { ServeHTTP(ResponseWriter, *Request) }
```

**La regla**: define la interfaz **donde se consume**, no donde se produce. Esto invierte la relacion respecto a Java/C#:

```go
// ❌ MAL — interfaz definida junto a la implementacion (estilo Java)
// package postgres
type UserRepository interface {
    GetUser(id string) (User, error)
    SaveUser(user User) error
    DeleteUser(id string) error
    ListUsers(limit, offset int) ([]User, error)
    SearchUsers(query string) ([]User, error)
    CountUsers() (int, error)
}

// ✅ BIEN — interfaz definida donde se consume (estilo Go)
// package orderservice
type UserGetter interface {
    GetUser(id string) (User, error)
}

// package adminservice
type UserManager interface {
    GetUser(id string) (User, error)
    SaveUser(user User) error
    DeleteUser(id string) error
}
```

Cada consumidor define **solo los metodos que necesita**. Esto tiene multiples beneficios:

1. **Mocks mas pequeños** — solo implementas 1-3 metodos, no 20
2. **Desacoplamiento real** — el consumidor no depende de metodos que no usa
3. **Interface Segregation Principle** — SOLID aplicado naturalmente
4. **Testabilidad** — menos superficie para mockear = menos fragilidad

### 2.3 Interfaces compuestas

Go permite componer interfaces por embedding:

```go
type Reader interface {
    Read(p []byte) (n int, err error)
}

type Writer interface {
    Write(p []byte) (n int, err error)
}

type Closer interface {
    Close() error
}

// Composicion — ReadWriteCloser tiene los 3 metodos
type ReadWriteCloser interface {
    Reader
    Writer
    Closer
}

// Para testing: puedes mockear ReadWriteCloser O solo Reader
// segun lo que necesite tu funcion
func ProcessData(r Reader) error {
    // Solo necesita Read — facil de mockear
    buf := make([]byte, 1024)
    n, err := r.Read(buf)
    // ...
}
```

### 2.4 El patron accept interfaces, return structs

Este es el patron central del diseño para testabilidad en Go:

```go
// Funciones/metodos ACEPTAN interfaces
func NewOrderService(users UserGetter, products ProductLister, notifier Notifier) *OrderService {
    return &OrderService{
        users:    users,
        products: products,
        notifier: notifier,
    }
}

// Constructores RETORNAN tipos concretos (structs)
// Esto permite que el caller acceda a metodos adicionales
// y que el compilador optimice mejor
type OrderService struct {
    users    UserGetter
    products ProductLister
    notifier Notifier
}
```

**¿Por que retornar structs y no interfaces?**

- El caller puede usar todos los metodos del struct, no solo los de una interfaz
- El compilador puede hacer inline y devirtualization
- No escondes la implementacion concreta innecesariamente
- Los interfaces se definen donde se necesitan, no donde se producen

---

## 3. Test doubles — tipos y patrones

El termino "mock" se usa coloquialmente para cualquier test double, pero hay diferencias importantes:

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                     TIPOS DE TEST DOUBLES                                       │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  DUMMY                                                                          │
│  ─────                                                                          │
│  Objeto que se pasa pero nunca se usa. Satisface la firma.                      │
│  type dummyLogger struct{}                                                      │
│  func (d *dummyLogger) Log(msg string) {}    ← no hace nada                    │
│                                                                                  │
│  STUB                                                                           │
│  ────                                                                           │
│  Retorna valores predefinidos. No verifica llamadas.                            │
│  type stubClock struct{ now time.Time }                                         │
│  func (s *stubClock) Now() time.Time { return s.now }                           │
│                                                                                  │
│  FAKE                                                                           │
│  ────                                                                           │
│  Implementacion funcional simplificada (in-memory DB, etc.)                     │
│  type fakeDB struct{ data map[string]User }                                     │
│  func (f *fakeDB) Get(id string) (User, error) { ... }  ← logica real          │
│                                                                                  │
│  SPY                                                                            │
│  ───                                                                            │
│  Registra llamadas para verificarlas despues.                                   │
│  type spyEmailer struct{ calls []string }                                       │
│  func (s *spyEmailer) Send(to string) { s.calls = append(s.calls, to) }        │
│                                                                                  │
│  MOCK                                                                           │
│  ────                                                                           │
│  Tiene expectativas predefinidas. Falla si no se cumplen.                       │
│  mock.EXPECT().GetUser("123").Return(user, nil).Times(1)                        │
│                                                                                  │
│  COMPLEJIDAD:  dummy < stub < spy < fake < mock                                │
│  FRAGILIDAD:   dummy < stub < spy < fake < mock                                │
│                                                                                  │
│  REGLA DE ORO: usa el test double mas simple que cubra tu caso.                 │
│  Prefiere stubs/fakes manuales. Reserva mocks (gomock) para verificar           │
│  interacciones complejas.                                                       │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 3.1 Dummy — solo satisface la firma

```go
type Logger interface {
    Info(msg string, args ...any)
    Error(msg string, args ...any)
}

// Dummy — descarta todo, solo existe para satisfacer la interfaz
type discardLogger struct{}

func (d *discardLogger) Info(msg string, args ...any)  {}
func (d *discardLogger) Error(msg string, args ...any) {}

func TestProcessOrder_Success(t *testing.T) {
    // No nos importa el logging en este test
    svc := NewOrderService(validStore, &discardLogger{})
    err := svc.Process(order)
    if err != nil {
        t.Fatal(err)
    }
}
```

### 3.2 Stub — retorna valores predefinidos

```go
type TimeProvider interface {
    Now() time.Time
}

// Stub — retorna siempre el mismo valor
type fixedClock struct {
    t time.Time
}

func (f *fixedClock) Now() time.Time { return f.t }

func TestTokenExpiration(t *testing.T) {
    // Controlamos el tiempo: siempre es 2024-01-15 12:00:00
    clock := &fixedClock{t: time.Date(2024, 1, 15, 12, 0, 0, 0, time.UTC)}

    token := NewToken(clock, 1*time.Hour)

    // Avanzamos el reloj 30 minutos — no deberia expirar
    clock.t = clock.t.Add(30 * time.Minute)
    if token.IsExpired(clock) {
        t.Error("token should not be expired after 30 minutes")
    }

    // Avanzamos a 61 minutos — deberia expirar
    clock.t = clock.t.Add(31 * time.Minute)
    if !token.IsExpired(clock) {
        t.Error("token should be expired after 61 minutes")
    }
}
```

### 3.3 Spy — registra llamadas

```go
type Notifier interface {
    Send(to, subject, body string) error
}

// Spy — registra cada llamada para verificar despues
type spyNotifier struct {
    calls []notifyCall
    err   error // error configurable para tests de error
}

type notifyCall struct {
    to, subject, body string
}

func (s *spyNotifier) Send(to, subject, body string) error {
    s.calls = append(s.calls, notifyCall{to, subject, body})
    return s.err
}

func TestUserRegistration_SendsWelcomeEmail(t *testing.T) {
    spy := &spyNotifier{}
    svc := NewUserService(fakeDB, spy)

    err := svc.Register(User{Email: "alice@example.com", Name: "Alice"})
    if err != nil {
        t.Fatal(err)
    }

    // Verificar que se envio exactamente 1 email
    if got := len(spy.calls); got != 1 {
        t.Fatalf("expected 1 notification, got %d", got)
    }

    // Verificar contenido
    call := spy.calls[0]
    if call.to != "alice@example.com" {
        t.Errorf("expected to=alice@example.com, got %s", call.to)
    }
    if call.subject != "Welcome!" {
        t.Errorf("expected subject=Welcome!, got %s", call.subject)
    }
}
```

### 3.4 Fake — implementacion simplificada

```go
type UserRepository interface {
    Get(id string) (User, error)
    Save(user User) error
    List(limit, offset int) ([]User, error)
    Delete(id string) error
}

// Fake — implementacion in-memory funcional
type fakeUserRepo struct {
    mu    sync.Mutex
    users map[string]User
}

func newFakeUserRepo() *fakeUserRepo {
    return &fakeUserRepo{users: make(map[string]User)}
}

func (f *fakeUserRepo) Get(id string) (User, error) {
    f.mu.Lock()
    defer f.mu.Unlock()
    u, ok := f.users[id]
    if !ok {
        return User{}, ErrNotFound
    }
    return u, nil
}

func (f *fakeUserRepo) Save(user User) error {
    f.mu.Lock()
    defer f.mu.Unlock()
    f.users[user.ID] = user
    return nil
}

func (f *fakeUserRepo) List(limit, offset int) ([]User, error) {
    f.mu.Lock()
    defer f.mu.Unlock()

    all := make([]User, 0, len(f.users))
    for _, u := range f.users {
        all = append(all, u)
    }

    // Ordenar por ID para determinismo
    sort.Slice(all, func(i, j int) bool { return all[i].ID < all[j].ID })

    if offset >= len(all) {
        return nil, nil
    }
    end := offset + limit
    if end > len(all) {
        end = len(all)
    }
    return all[offset:end], nil
}

func (f *fakeUserRepo) Delete(id string) error {
    f.mu.Lock()
    defer f.mu.Unlock()
    if _, ok := f.users[id]; !ok {
        return ErrNotFound
    }
    delete(f.users, id)
    return nil
}

func TestListUsers_Pagination(t *testing.T) {
    repo := newFakeUserRepo()

    // Insertar 25 usuarios
    for i := range 25 {
        repo.Save(User{ID: fmt.Sprintf("u%03d", i), Name: fmt.Sprintf("User %d", i)})
    }

    // Pagina 1: primeros 10
    page1, err := repo.List(10, 0)
    if err != nil {
        t.Fatal(err)
    }
    if len(page1) != 10 {
        t.Errorf("expected 10 users, got %d", len(page1))
    }

    // Pagina 3: ultimos 5
    page3, err := repo.List(10, 20)
    if err != nil {
        t.Fatal(err)
    }
    if len(page3) != 5 {
        t.Errorf("expected 5 users, got %d", len(page3))
    }
}
```

### 3.5 Mock con expectativas — patron configurable

Sin frameworks, puedes construir mocks con expectativas manuales:

```go
type mockUserRepo struct {
    // Configuracion
    getFunc    func(id string) (User, error)
    saveFunc   func(user User) error
    listFunc   func(limit, offset int) ([]User, error)
    deleteFunc func(id string) error

    // Tracking
    getCalls    []string
    saveCalls   []User
    deleteCalls []string
}

func (m *mockUserRepo) Get(id string) (User, error) {
    m.getCalls = append(m.getCalls, id)
    if m.getFunc != nil {
        return m.getFunc(id)
    }
    return User{}, fmt.Errorf("Get not configured")
}

func (m *mockUserRepo) Save(user User) error {
    m.saveCalls = append(m.saveCalls, user)
    if m.saveFunc != nil {
        return m.saveFunc(user)
    }
    return nil
}

func (m *mockUserRepo) List(limit, offset int) ([]User, error) {
    if m.listFunc != nil {
        return m.listFunc(limit, offset)
    }
    return nil, nil
}

func (m *mockUserRepo) Delete(id string) error {
    m.deleteCalls = append(m.deleteCalls, id)
    if m.deleteFunc != nil {
        return m.deleteFunc(id)
    }
    return nil
}

func TestUpdateUser_GetsAndSaves(t *testing.T) {
    existing := User{ID: "123", Name: "Old Name", Email: "old@test.com"}

    mock := &mockUserRepo{
        getFunc: func(id string) (User, error) {
            if id == "123" {
                return existing, nil
            }
            return User{}, ErrNotFound
        },
        saveFunc: func(user User) error {
            if user.Name != "New Name" {
                t.Errorf("expected name=New Name, got %s", user.Name)
            }
            return nil
        },
    }

    svc := NewUserService(mock)
    err := svc.UpdateName("123", "New Name")
    if err != nil {
        t.Fatal(err)
    }

    // Verificar interacciones
    if len(mock.getCalls) != 1 || mock.getCalls[0] != "123" {
        t.Errorf("expected 1 Get call with id=123, got %v", mock.getCalls)
    }
    if len(mock.saveCalls) != 1 {
        t.Errorf("expected 1 Save call, got %d", len(mock.saveCalls))
    }
}
```

---

## 4. Dependency injection en Go

### 4.1 DI no es un framework — es un patron

En Go, dependency injection es **pasar cosas como parametros** en vez de crearlas internamente. No necesitas frameworks como Spring, Dagger, o Wire (aunque Wire existe para proyectos grandes). La mayoria de proyectos Go usan DI manual.

```go
// ❌ SIN dependency injection — imposible de testear
type OrderService struct {
    // Dependencia creada internamente
}

func (s *OrderService) PlaceOrder(order Order) error {
    // Crea la conexion internamente — no hay forma de reemplazarla en tests
    db, _ := sql.Open("postgres", "host=localhost ...")
    _, err := db.Exec("INSERT INTO orders ...", order.ID)
    if err != nil {
        return err
    }

    // Envia email internamente — no hay forma de interceptarlo
    smtp.SendMail("smtp.gmail.com:587", auth, from, []string{order.Email}, body)
    return nil
}
```

```go
// ✅ CON dependency injection — completamente testeable
type OrderPlacer interface {
    Save(order Order) error
}

type OrderNotifier interface {
    NotifyOrderPlaced(order Order) error
}

type OrderService struct {
    store    OrderPlacer
    notifier OrderNotifier
}

func NewOrderService(store OrderPlacer, notifier OrderNotifier) *OrderService {
    return &OrderService{
        store:    store,
        notifier: notifier,
    }
}

func (s *OrderService) PlaceOrder(order Order) error {
    if err := s.store.Save(order); err != nil {
        return fmt.Errorf("saving order: %w", err)
    }
    if err := s.notifier.NotifyOrderPlaced(order); err != nil {
        // Log but don't fail — the order was saved
        log.Printf("notification failed for order %s: %v", order.ID, err)
    }
    return nil
}
```

### 4.2 Tres formas de inyectar dependencias

#### Constructor injection (preferida)

```go
// La mas comun y recomendada en Go
func NewUserService(repo UserRepository, logger Logger) *UserService {
    return &UserService{
        repo:   repo,
        logger: logger,
    }
}

// En tests:
func TestUserService(t *testing.T) {
    svc := NewUserService(fakeRepo, discardLogger)
    // ...
}
```

#### Method injection (para dependencias per-request)

```go
// Cuando la dependencia cambia entre llamadas
func (s *Service) ProcessWithContext(ctx context.Context, tracer Tracer) error {
    span := tracer.StartSpan(ctx, "process")
    defer span.End()
    // ...
}
```

#### Functional options (para configuracion flexible)

```go
type Option func(*Server)

func WithLogger(l Logger) Option {
    return func(s *Server) { s.logger = l }
}

func WithTimeout(d time.Duration) Option {
    return func(s *Server) { s.timeout = d }
}

func WithStore(store DataStore) Option {
    return func(s *Server) { s.store = store }
}

func NewServer(opts ...Option) *Server {
    s := &Server{
        logger:  defaultLogger,     // defaults sensatos
        timeout: 30 * time.Second,
        store:   defaultStore,
    }
    for _, opt := range opts {
        opt(s)
    }
    return s
}

// En tests — inyectar solo lo que necesitas:
func TestServer(t *testing.T) {
    srv := NewServer(
        WithStore(fakeStore),
        WithLogger(&discardLogger{}),
        WithTimeout(1 * time.Second), // timeout corto para tests
    )
    // ...
}
```

### 4.3 DI con generics

Go 1.18+ permite escribir patrones de DI mas expresivos con generics:

```go
// Registro de servicios generico
type ServiceRegistry struct {
    mu       sync.RWMutex
    services map[string]any
}

func Register[T any](r *ServiceRegistry, name string, svc T) {
    r.mu.Lock()
    defer r.mu.Unlock()
    r.services[name] = svc
}

func Resolve[T any](r *ServiceRegistry, name string) (T, error) {
    r.mu.RLock()
    defer r.mu.RUnlock()

    svc, ok := r.services[name]
    if !ok {
        var zero T
        return zero, fmt.Errorf("service %q not registered", name)
    }
    concrete, ok := svc.(T)
    if !ok {
        var zero T
        return zero, fmt.Errorf("service %q is not of type %T", name, zero)
    }
    return concrete, nil
}

// Uso en tests:
func TestWithRegistry(t *testing.T) {
    reg := &ServiceRegistry{services: make(map[string]any)}
    Register[UserRepository](reg, "users", newFakeUserRepo())
    Register[Logger](reg, "logger", &discardLogger{})

    repo, _ := Resolve[UserRepository](reg, "users")
    // ...
}
```

### 4.4 El patron Wire (Google Wire)

Para proyectos grandes, **Wire** (google.github.io/wire) genera codigo de DI en compile time. No es reflection — genera funciones Go normales:

```go
// wire.go — archivo de configuracion (build tag para que no compile normalmente)
//go:build wireinject

package main

import "github.com/google/wire"

func InitializeApp() (*App, error) {
    wire.Build(
        NewPostgresDB,     // provee *sql.DB
        NewUserRepo,       // provee UserRepository (usa *sql.DB)
        NewEmailNotifier,  // provee Notifier
        NewUserService,    // provee *UserService (usa UserRepository, Notifier)
        NewApp,            // provee *App (usa *UserService)
    )
    return nil, nil // Wire lo reemplaza
}

// Wire genera wire_gen.go con el codigo real:
// func InitializeApp() (*App, error) {
//     db, err := NewPostgresDB()
//     if err != nil { return nil, err }
//     repo := NewUserRepo(db)
//     notifier := NewEmailNotifier()
//     svc := NewUserService(repo, notifier)
//     app := NewApp(svc)
//     return app, nil
// }
```

Wire es util cuando el grafo de dependencias es complejo (>20 servicios), pero para la mayoria de proyectos, **DI manual es suficiente y preferible** por su claridad.

---

## 5. Patrones avanzados de mocking

### 5.1 Mock con errores por invocacion

```go
// Mock que retorna diferentes resultados en llamadas sucesivas
type sequenceMock struct {
    responses []response
    callCount int
    mu        sync.Mutex
}

type response struct {
    user User
    err  error
}

func (m *sequenceMock) GetUser(id string) (User, error) {
    m.mu.Lock()
    defer m.mu.Unlock()

    if m.callCount >= len(m.responses) {
        return User{}, fmt.Errorf("unexpected call #%d", m.callCount+1)
    }
    r := m.responses[m.callCount]
    m.callCount++
    return r.user, r.err
}

func TestRetryLogic(t *testing.T) {
    mock := &sequenceMock{
        responses: []response{
            {err: fmt.Errorf("connection timeout")},    // 1er intento falla
            {err: fmt.Errorf("connection refused")},     // 2do intento falla
            {user: User{ID: "1", Name: "Alice"}, err: nil}, // 3er intento OK
        },
    }

    svc := NewUserServiceWithRetry(mock, 3)
    user, err := svc.GetUser("1")
    if err != nil {
        t.Fatalf("expected success after retries, got: %v", err)
    }
    if user.Name != "Alice" {
        t.Errorf("expected Alice, got %s", user.Name)
    }
    if mock.callCount != 3 {
        t.Errorf("expected 3 calls, got %d", mock.callCount)
    }
}
```

### 5.2 Mock de time.Now

El tiempo es una de las dependencias mas comunes que necesita mocking:

```go
// Opcion 1: Interfaz
type Clock interface {
    Now() time.Time
    Since(t time.Time) time.Duration
    After(d time.Duration) <-chan time.Time
}

type realClock struct{}

func (realClock) Now() time.Time                         { return time.Now() }
func (realClock) Since(t time.Time) time.Duration        { return time.Since(t) }
func (realClock) After(d time.Duration) <-chan time.Time  { return time.After(d) }

type fakeClock struct {
    now time.Time
}

func (f *fakeClock) Now() time.Time                         { return f.now }
func (f *fakeClock) Since(t time.Time) time.Duration        { return f.now.Sub(t) }
func (f *fakeClock) After(d time.Duration) <-chan time.Time  {
    ch := make(chan time.Time, 1)
    ch <- f.now.Add(d)
    return ch
}

func (f *fakeClock) Advance(d time.Duration) {
    f.now = f.now.Add(d)
}

// Opcion 2: Variable de funcion (mas simple, menos formal)
var nowFunc = time.Now

func TokenExpiresAt() time.Time {
    return nowFunc().Add(24 * time.Hour)
}

func TestTokenExpiry(t *testing.T) {
    fixed := time.Date(2024, 6, 15, 10, 0, 0, 0, time.UTC)
    nowFunc = func() time.Time { return fixed }
    t.Cleanup(func() { nowFunc = time.Now }) // restaurar

    expires := TokenExpiresAt()
    expected := fixed.Add(24 * time.Hour)
    if !expires.Equal(expected) {
        t.Errorf("expected %v, got %v", expected, expires)
    }
}
```

> **Nota**: La opcion 2 (variable de funcion) no es thread-safe y puede causar problemas con `t.Parallel`. Prefiere la opcion 1 (interfaz) cuando los tests corren en paralelo.

### 5.3 Mock de HTTP externo

Para funciones que llaman APIs externas:

```go
// httptest.Server — servidor HTTP real en localhost para tests
func TestFetchWeather(t *testing.T) {
    // Crear un servidor falso que responde con datos conocidos
    server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        // Verificar la request
        if r.URL.Path != "/api/weather" {
            t.Errorf("expected path /api/weather, got %s", r.URL.Path)
        }
        if r.URL.Query().Get("city") != "Madrid" {
            t.Errorf("expected city=Madrid, got %s", r.URL.Query().Get("city"))
        }

        // Responder con datos conocidos
        w.Header().Set("Content-Type", "application/json")
        w.WriteHeader(http.StatusOK)
        json.NewEncoder(w).Encode(WeatherResponse{
            City: "Madrid",
            Temp: 25.5,
            Unit: "celsius",
        })
    }))
    defer server.Close()

    // Usar el URL del servidor falso en vez del real
    client := NewWeatherClient(server.URL)
    weather, err := client.GetWeather("Madrid")
    if err != nil {
        t.Fatal(err)
    }
    if weather.Temp != 25.5 {
        t.Errorf("expected temp=25.5, got %f", weather.Temp)
    }
}

// Mock mas sofisticado — multiples endpoints y estados
func TestAPIClient_MultipleEndpoints(t *testing.T) {
    mux := http.NewServeMux()

    mux.HandleFunc("GET /api/users/{id}", func(w http.ResponseWriter, r *http.Request) {
        id := r.PathValue("id")
        switch id {
        case "1":
            json.NewEncoder(w).Encode(User{ID: "1", Name: "Alice"})
        case "999":
            w.WriteHeader(http.StatusNotFound)
            json.NewEncoder(w).Encode(APIError{Code: "NOT_FOUND", Message: "user not found"})
        default:
            w.WriteHeader(http.StatusInternalServerError)
        }
    })

    mux.HandleFunc("POST /api/users", func(w http.ResponseWriter, r *http.Request) {
        var u User
        if err := json.NewDecoder(r.Body).Decode(&u); err != nil {
            w.WriteHeader(http.StatusBadRequest)
            return
        }
        u.ID = "new-id"
        w.WriteHeader(http.StatusCreated)
        json.NewEncoder(w).Encode(u)
    })

    server := httptest.NewServer(mux)
    defer server.Close()

    client := NewAPIClient(server.URL)

    t.Run("get existing user", func(t *testing.T) {
        user, err := client.GetUser("1")
        if err != nil {
            t.Fatal(err)
        }
        if user.Name != "Alice" {
            t.Errorf("expected Alice, got %s", user.Name)
        }
    })

    t.Run("get non-existing user", func(t *testing.T) {
        _, err := client.GetUser("999")
        if err == nil {
            t.Fatal("expected error for non-existing user")
        }
    })

    t.Run("create user", func(t *testing.T) {
        user, err := client.CreateUser(User{Name: "Bob"})
        if err != nil {
            t.Fatal(err)
        }
        if user.ID == "" {
            t.Error("expected user to have an ID")
        }
    })
}
```

### 5.4 Mock de filesystem

```go
// Interfaz para operaciones de filesystem
type FileSystem interface {
    ReadFile(name string) ([]byte, error)
    WriteFile(name string, data []byte, perm os.FileMode) error
    Stat(name string) (os.FileInfo, error)
    MkdirAll(path string, perm os.FileMode) error
}

// Implementacion real
type osFS struct{}

func (osFS) ReadFile(name string) ([]byte, error)                 { return os.ReadFile(name) }
func (osFS) WriteFile(name string, data []byte, p os.FileMode) error { return os.WriteFile(name, data, p) }
func (osFS) Stat(name string) (os.FileInfo, error)                { return os.Stat(name) }
func (osFS) MkdirAll(path string, perm os.FileMode) error         { return os.MkdirAll(path, perm) }

// Fake in-memory filesystem para tests
type memFS struct {
    files map[string][]byte
}

func newMemFS() *memFS {
    return &memFS{files: make(map[string][]byte)}
}

func (m *memFS) ReadFile(name string) ([]byte, error) {
    data, ok := m.files[name]
    if !ok {
        return nil, &os.PathError{Op: "read", Path: name, Err: os.ErrNotExist}
    }
    return data, nil
}

func (m *memFS) WriteFile(name string, data []byte, perm os.FileMode) error {
    m.files[name] = append([]byte(nil), data...) // copia defensiva
    return nil
}

func (m *memFS) Stat(name string) (os.FileInfo, error) {
    if _, ok := m.files[name]; !ok {
        return nil, &os.PathError{Op: "stat", Path: name, Err: os.ErrNotExist}
    }
    return nil, nil // simplificado — retorna nil FileInfo
}

func (m *memFS) MkdirAll(path string, perm os.FileMode) error {
    return nil // no-op en memoria
}

func TestConfigLoader(t *testing.T) {
    fs := newMemFS()
    fs.WriteFile("/etc/app/config.yaml", []byte("port: 8080\nhost: localhost"), 0644)

    loader := NewConfigLoader(fs)
    cfg, err := loader.Load("/etc/app/config.yaml")
    if err != nil {
        t.Fatal(err)
    }
    if cfg.Port != 8080 {
        t.Errorf("expected port=8080, got %d", cfg.Port)
    }
}
```

### 5.5 Interfaces parciales con embedding

Cuando una interfaz es grande y solo necesitas mockear unos pocos metodos:

```go
type LargeService interface {
    MethodA() error
    MethodB(id string) (Result, error)
    MethodC(data []byte) error
    MethodD() []string
    MethodE(ctx context.Context) error
    // ... 10 metodos mas
}

// En vez de implementar los 15 metodos, embebe una implementacion base
// y sobreescribe solo lo que necesitas

// Implementacion base que paniquea (detecta metodos no mockeados)
type baseLargeService struct{}

func (baseLargeService) MethodA() error                     { panic("MethodA not mocked") }
func (baseLargeService) MethodB(id string) (Result, error)  { panic("MethodB not mocked") }
func (baseLargeService) MethodC(data []byte) error          { panic("MethodC not mocked") }
func (baseLargeService) MethodD() []string                  { panic("MethodD not mocked") }
func (baseLargeService) MethodE(ctx context.Context) error  { panic("MethodE not mocked") }

// Mock parcial — solo sobreescribe MethodB
type mockOnlyMethodB struct {
    baseLargeService // embebe base — todos los metodos paniquean excepto B
    result Result
    err    error
}

func (m *mockOnlyMethodB) MethodB(id string) (Result, error) {
    return m.result, m.err
}

func TestSomethingThatOnlyUsesMethodB(t *testing.T) {
    mock := &mockOnlyMethodB{
        result: Result{Value: 42},
    }
    // Si el codigo bajo test llama MethodA, C, D, o E → panic
    // Esto es una señal de que el test o el codigo necesitan refactoring
    output := ProcessResult(mock)
    // ...
}
```

---

## 6. Generacion automatica de mocks

### 6.1 gomock y mockgen

`gomock` es la libreria oficial de Google para mocking en Go. `mockgen` es su herramienta de generacion de codigo:

```bash
# Instalar mockgen
go install go.uber.org/mock/mockgen@latest

# Generar mocks desde una interfaz (mode: source)
mockgen -source=repository.go -destination=mock_repository_test.go -package=service_test

# Generar mocks desde un paquete (mode: reflect)
mockgen -destination=mock_store_test.go -package=service_test \
    github.com/myapp/store UserStore,ProductStore

# Con go:generate (lo comun)
//go:generate mockgen -source=interfaces.go -destination=mock_interfaces_test.go -package=service_test
```

Uso de mocks generados:

```go
import (
    "testing"
    "go.uber.org/mock/gomock"
)

func TestOrderService_PlaceOrder(t *testing.T) {
    ctrl := gomock.NewController(t)
    // ctrl.Finish() no es necesario desde Go 1.14 — se llama automaticamente via t.Cleanup

    mockStore := NewMockOrderStore(ctrl)
    mockNotifier := NewMockNotifier(ctrl)

    order := Order{ID: "ord-1", UserID: "u-1", Total: 99.99}

    // Configurar expectativas
    mockStore.EXPECT().
        Save(gomock.Eq(order)).
        Return(nil).
        Times(1)

    mockNotifier.EXPECT().
        NotifyOrderPlaced(gomock.Any()). // cualquier argumento
        Return(nil).
        Times(1)

    svc := NewOrderService(mockStore, mockNotifier)
    err := svc.PlaceOrder(order)
    if err != nil {
        t.Fatal(err)
    }
    // ctrl verifica que TODAS las expectativas se cumplieron
}
```

**Matchers de gomock:**

```go
gomock.Eq(value)       // igualdad exacta
gomock.Any()           // cualquier valor
gomock.Nil()           // nil
gomock.Not(matcher)    // negacion
gomock.Len(n)          // slice/map de longitud n

// Matcher personalizado
type hasIDMatcher struct{ id string }
func (m hasIDMatcher) Matches(x interface{}) bool {
    order, ok := x.(Order)
    return ok && order.ID == m.id
}
func (m hasIDMatcher) String() string { return fmt.Sprintf("has ID %q", m.id) }

func HasID(id string) gomock.Matcher { return hasIDMatcher{id: id} }

// Uso:
mockStore.EXPECT().Save(HasID("ord-1")).Return(nil)
```

**Control de orden:**

```go
func TestOrderedCalls(t *testing.T) {
    ctrl := gomock.NewController(t)
    mockDB := NewMockDB(ctrl)

    // InOrder — las llamadas DEBEN ocurrir en este orden
    gomock.InOrder(
        mockDB.EXPECT().Begin().Return(nil),
        mockDB.EXPECT().Insert(gomock.Any()).Return(nil),
        mockDB.EXPECT().Commit().Return(nil),
    )

    svc := NewService(mockDB)
    svc.DoTransaction()
}
```

**Do — ejecutar logica en el mock:**

```go
mockStore.EXPECT().
    Save(gomock.Any()).
    DoAndReturn(func(order Order) error {
        // Validar cosas complejas
        if order.Total <= 0 {
            t.Error("order total should be positive")
        }
        return nil
    })
```

### 6.2 moq — alternativa ligera

`moq` genera mocks mas simples y legibles que gomock. Los mocks generados son structs con campos de funcion:

```bash
go install github.com/matryer/moq@latest

# Generar mock
moq -out mock_store_test.go -pkg service_test ./store UserStore
```

Codigo generado:

```go
// Generado por moq — NO EDITAR
type UserStoreMock struct {
    GetUserFunc func(id string) (User, error)
    SaveUserFunc func(user User) error
}

func (m *UserStoreMock) GetUser(id string) (User, error) {
    return m.GetUserFunc(id)
}

func (m *UserStoreMock) SaveUser(user User) error {
    return m.SaveUserFunc(user)
}
```

Uso:

```go
func TestWithMoq(t *testing.T) {
    mock := &UserStoreMock{
        GetUserFunc: func(id string) (User, error) {
            return User{ID: id, Name: "Alice"}, nil
        },
        SaveUserFunc: func(user User) error {
            return nil
        },
    }

    svc := NewUserService(mock)
    // ...
}
```

**moq vs gomock:**

| Aspecto | gomock | moq |
|---------|--------|-----|
| Estilo | Expectativas (EXPECT) | Funciones asignables |
| Verificacion | Automatica (ctrl.Finish) | Manual |
| Orden de llamadas | gomock.InOrder | Manual |
| Legibilidad del mock | Verbose, API propia | Go puro, campos de func |
| Curva de aprendizaje | Mayor | Menor |
| ¿Recomendacion? | Interacciones complejas | Casos simples/medianos |

### 6.3 counterfeiter

Otra alternativa popular, especialmente en proyectos Cloud Foundry:

```bash
go install github.com/maxbrunsfeld/counterfeiter/v6@latest

# Generar
//go:generate counterfeiter -o fakes/fake_store.go . UserStore
counterfeiter -o fakes/fake_store.go . UserStore
```

counterfeiter genera fakes con tracking de invocaciones incorporado:

```go
func TestWithCounterfeiter(t *testing.T) {
    fake := &fakes.FakeUserStore{}
    fake.GetUserReturns(User{ID: "1", Name: "Alice"}, nil)

    svc := NewUserService(fake)
    user, _ := svc.GetUser("1")

    // Verificar invocaciones
    if fake.GetUserCallCount() != 1 {
        t.Errorf("expected 1 call, got %d", fake.GetUserCallCount())
    }

    // Inspeccionar argumentos
    id := fake.GetUserArgsForCall(0)
    if id != "1" {
        t.Errorf("expected id=1, got %s", id)
    }
}
```

### 6.4 go:generate para automatizar

```go
// interfaces.go
package service

//go:generate mockgen -source=interfaces.go -destination=mock_interfaces_test.go -package=service_test

type UserStore interface {
    GetUser(id string) (User, error)
    SaveUser(user User) error
}

type ProductStore interface {
    GetProduct(id string) (Product, error)
    ListProducts(category string) ([]Product, error)
}

type Notifier interface {
    Send(to, subject, body string) error
}
```

```bash
# Regenerar todos los mocks del proyecto
go generate ./...

# Solo un paquete
go generate ./service/...
```

---

## 7. Mocking sin interfaces — tecnicas alternativas

### 7.1 Monkey patching con variables de paquete

Cuando no puedes cambiar la firma de una funcion para aceptar interfaces (codigo legado, stdlib):

```go
// production code
var osExit = os.Exit
var execCommand = exec.Command

func RunDangerous() {
    if err := doWork(); err != nil {
        fmt.Fprintf(os.Stderr, "fatal: %v\n", err)
        osExit(1)
    }
}

func ExecuteScript(name string) error {
    cmd := execCommand("bash", "-c", name)
    return cmd.Run()
}

// test code
func TestRunDangerous_ExitsOnError(t *testing.T) {
    var exitCode int
    osExit = func(code int) { exitCode = code }
    t.Cleanup(func() { osExit = os.Exit })

    // forzar error en doWork...
    RunDangerous()

    if exitCode != 1 {
        t.Errorf("expected exit code 1, got %d", exitCode)
    }
}
```

> **Advertencia**: Este patron NO es thread-safe. No usar con `t.Parallel()`. Es aceptable para tests secuenciales de codigo legado, pero prefiere interfaces cuando puedas refactorizar.

### 7.2 Campos de funcion en structs

```go
type EmailSender struct {
    // Campos de funcion — inyectables en tests
    sendFunc    func(to, subject, body string) error
    templateFn  func(name string) (*template.Template, error)
}

func NewEmailSender() *EmailSender {
    return &EmailSender{
        sendFunc:   defaultSendEmail,
        templateFn: defaultLoadTemplate,
    }
}

func (e *EmailSender) SendWelcome(user User) error {
    tmpl, err := e.templateFn("welcome")
    if err != nil {
        return fmt.Errorf("loading template: %w", err)
    }
    var body bytes.Buffer
    if err := tmpl.Execute(&body, user); err != nil {
        return fmt.Errorf("executing template: %w", err)
    }
    return e.sendFunc(user.Email, "Welcome!", body.String())
}

// En tests:
func TestSendWelcome(t *testing.T) {
    var sentTo, sentSubject, sentBody string

    sender := &EmailSender{
        sendFunc: func(to, subject, body string) error {
            sentTo = to
            sentSubject = subject
            sentBody = body
            return nil
        },
        templateFn: func(name string) (*template.Template, error) {
            return template.New(name).Parse("Hello {{.Name}}!"), nil
        },
    }

    err := sender.SendWelcome(User{Name: "Alice", Email: "alice@test.com"})
    if err != nil {
        t.Fatal(err)
    }
    if sentTo != "alice@test.com" {
        t.Errorf("expected alice@test.com, got %s", sentTo)
    }
    if sentBody != "Hello Alice!" {
        t.Errorf("expected 'Hello Alice!', got %s", sentBody)
    }
}
```

### 7.3 Interfaces inline (anonymous)

Para tests rapidos donde definir un tipo es excesivo:

```go
// Funcion que acepta interfaz
type Processor interface {
    Process(data []byte) ([]byte, error)
}

func RunPipeline(steps []Processor, input []byte) ([]byte, error) {
    current := input
    for _, step := range steps {
        var err error
        current, err = step.Process(current)
        if err != nil {
            return nil, err
        }
    }
    return current, nil
}

// Mock inline usando un tipo adaptador (processorFunc)
type processorFunc func([]byte) ([]byte, error)

func (f processorFunc) Process(data []byte) ([]byte, error) { return f(data) }

func TestPipeline(t *testing.T) {
    upper := processorFunc(func(data []byte) ([]byte, error) {
        return bytes.ToUpper(data), nil
    })
    trim := processorFunc(func(data []byte) ([]byte, error) {
        return bytes.TrimSpace(data), nil
    })

    result, err := RunPipeline([]Processor{trim, upper}, []byte("  hello  "))
    if err != nil {
        t.Fatal(err)
    }
    if string(result) != "HELLO" {
        t.Errorf("expected HELLO, got %s", result)
    }
}
```

---

## 8. Testcontainers-go — integration tests con Docker

### 8.1 ¿Que es testcontainers?

**testcontainers-go** es una libreria que permite levantar contenedores Docker efimeros directamente desde tus tests de Go. Cada test (o suite de tests) levanta un contenedor limpio, corre los tests contra el, y lo destruye al terminar. Esto garantiza:

- **Aislamiento**: cada test tiene su propia instancia
- **Reproducibilidad**: misma version en CI y local
- **Realismo**: testeas contra el servicio real, no un mock

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│               TESTCONTAINERS — FLUJO DE VIDA                                    │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│    Test starts                                                                  │
│        │                                                                        │
│        ▼                                                                        │
│    ┌──────────────────┐                                                         │
│    │ Pull image       │  ← solo la primera vez (cached)                         │
│    │ (postgres:16)    │                                                         │
│    └────────┬─────────┘                                                         │
│             │                                                                   │
│             ▼                                                                   │
│    ┌──────────────────┐                                                         │
│    │ Create container │  ← puertos aleatorios (evita conflictos)                │
│    │ + wait strategy  │                                                         │
│    └────────┬─────────┘                                                         │
│             │                                                                   │
│             ▼                                                                   │
│    ┌──────────────────┐                                                         │
│    │ Container ready  │  ← wait verifica que acepta conexiones                  │
│    │ (health check)   │                                                         │
│    └────────┬─────────┘                                                         │
│             │                                                                   │
│             ▼                                                                   │
│    ┌──────────────────┐                                                         │
│    │ Run tests        │  ← tests usan la conexion al container                  │
│    │ against real DB  │                                                         │
│    └────────┬─────────┘                                                         │
│             │                                                                   │
│             ▼                                                                   │
│    ┌──────────────────┐                                                         │
│    │ Terminate        │  ← t.Cleanup o defer — container destruido              │
│    │ container        │                                                         │
│    └──────────────────┘                                                         │
│                                                                                  │
│  REQUISITOS: Docker daemon corriendo (local o CI)                               │
│  CI: GitHub Actions tiene Docker; GitLab CI usa Docker-in-Docker (dind)         │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 8.2 Instalacion y setup

```bash
# Instalar el paquete base
go get github.com/testcontainers/testcontainers-go

# Modulos especificos (tienen helpers preconfigurados)
go get github.com/testcontainers/testcontainers-go/modules/postgres
go get github.com/testcontainers/testcontainers-go/modules/redis
go get github.com/testcontainers/testcontainers-go/modules/mysql
go get github.com/testcontainers/testcontainers-go/modules/kafka
go get github.com/testcontainers/testcontainers-go/modules/mongodb
go get github.com/testcontainers/testcontainers-go/modules/rabbitmq
```

### 8.3 Container generico — API base

```go
package integration_test

import (
    "context"
    "testing"
    "time"

    "github.com/testcontainers/testcontainers-go"
    "github.com/testcontainers/testcontainers-go/wait"
)

func TestWithGenericContainer(t *testing.T) {
    if testing.Short() {
        t.Skip("skipping integration test in short mode")
    }

    ctx := context.Background()

    // Definir el container
    req := testcontainers.ContainerRequest{
        Image:        "postgres:16-alpine",
        ExposedPorts: []string{"5432/tcp"},
        Env: map[string]string{
            "POSTGRES_USER":     "testuser",
            "POSTGRES_PASSWORD": "testpass",
            "POSTGRES_DB":       "testdb",
        },
        WaitingFor: wait.ForAll(
            wait.ForLog("database system is ready to accept connections").
                WithOccurrence(2).           // PostgreSQL emite esto 2 veces
                WithStartupTimeout(30 * time.Second),
            wait.ForListeningPort("5432/tcp"),
        ),
    }

    // Crear y arrancar el container
    container, err := testcontainers.GenericContainer(ctx, testcontainers.GenericContainerRequest{
        ContainerRequest: req,
        Started:          true, // arrancar inmediatamente
    })
    if err != nil {
        t.Fatalf("failed to start container: %v", err)
    }

    // Limpiar al terminar
    t.Cleanup(func() {
        if err := container.Terminate(ctx); err != nil {
            t.Logf("failed to terminate container: %v", err)
        }
    })

    // Obtener host y puerto mapeado
    host, err := container.Host(ctx)
    if err != nil {
        t.Fatal(err)
    }
    mappedPort, err := container.MappedPort(ctx, "5432")
    if err != nil {
        t.Fatal(err)
    }

    // Construir connection string
    dsn := fmt.Sprintf("postgres://testuser:testpass@%s:%s/testdb?sslmode=disable",
        host, mappedPort.Port())

    t.Logf("PostgreSQL available at: %s", dsn)

    // Conectar y testear
    db, err := sql.Open("pgx", dsn)
    if err != nil {
        t.Fatal(err)
    }
    defer db.Close()

    // Verificar conexion
    if err := db.Ping(); err != nil {
        t.Fatal(err)
    }

    // Crear tabla y testear
    _, err = db.Exec(`
        CREATE TABLE users (
            id SERIAL PRIMARY KEY,
            name TEXT NOT NULL,
            email TEXT UNIQUE NOT NULL
        )
    `)
    if err != nil {
        t.Fatal(err)
    }

    _, err = db.Exec("INSERT INTO users (name, email) VALUES ($1, $2)", "Alice", "alice@test.com")
    if err != nil {
        t.Fatal(err)
    }

    var name string
    err = db.QueryRow("SELECT name FROM users WHERE email=$1", "alice@test.com").Scan(&name)
    if err != nil {
        t.Fatal(err)
    }
    if name != "Alice" {
        t.Errorf("expected Alice, got %s", name)
    }
}
```

### 8.4 Modulo PostgreSQL — API simplificada

Los modulos de testcontainers proveen APIs de alto nivel preconfiguradas:

```go
package integration_test

import (
    "context"
    "database/sql"
    "path/filepath"
    "testing"
    "time"

    "github.com/testcontainers/testcontainers-go"
    "github.com/testcontainers/testcontainers-go/modules/postgres"
    "github.com/testcontainers/testcontainers-go/wait"
    _ "github.com/jackc/pgx/v5/stdlib"
)

func TestWithPostgresModule(t *testing.T) {
    if testing.Short() {
        t.Skip("skipping integration test")
    }

    ctx := context.Background()

    // El modulo configura imagen, puertos, wait strategy, etc.
    pgContainer, err := postgres.Run(ctx,
        "postgres:16-alpine",
        postgres.WithDatabase("testdb"),
        postgres.WithUsername("testuser"),
        postgres.WithPassword("testpass"),

        // Ejecutar scripts de inicializacion
        postgres.WithInitScripts(
            filepath.Join("testdata", "init.sql"),
        ),

        // Configurar snapshot para restaurar entre tests
        postgres.WithSQLDriver("pgx"),

        // Wait strategy personalizada
        testcontainers.WithWaitStrategy(
            wait.ForLog("database system is ready to accept connections").
                WithOccurrence(2).
                WithStartupTimeout(30*time.Second),
        ),
    )
    if err != nil {
        t.Fatal(err)
    }
    t.Cleanup(func() {
        if err := pgContainer.Terminate(ctx); err != nil {
            t.Log(err)
        }
    })

    // Obtener connection string directamente
    connStr, err := pgContainer.ConnectionString(ctx, "sslmode=disable")
    if err != nil {
        t.Fatal(err)
    }

    db, err := sql.Open("pgx", connStr)
    if err != nil {
        t.Fatal(err)
    }
    defer db.Close()

    // Los scripts de testdata/init.sql ya corrieron
    // Puedes testear directamente
    var count int
    err = db.QueryRow("SELECT COUNT(*) FROM users").Scan(&count)
    if err != nil {
        t.Fatal(err)
    }
    t.Logf("found %d users from init script", count)
}
```

**Snapshot y Restore** — restaurar el estado entre tests:

```go
func TestWithSnapshot(t *testing.T) {
    ctx := context.Background()

    pgContainer, err := postgres.Run(ctx,
        "postgres:16-alpine",
        postgres.WithDatabase("testdb"),
        postgres.WithUsername("testuser"),
        postgres.WithPassword("testpass"),
        postgres.WithInitScripts("testdata/schema.sql", "testdata/seed.sql"),
        postgres.WithSQLDriver("pgx"),
        testcontainers.WithWaitStrategy(
            wait.ForLog("database system is ready to accept connections").
                WithOccurrence(2).
                WithStartupTimeout(30*time.Second),
        ),
    )
    if err != nil {
        t.Fatal(err)
    }
    t.Cleanup(func() { pgContainer.Terminate(ctx) })

    // Tomar snapshot del estado inicial
    err = pgContainer.Snapshot(ctx, postgres.WithSnapshotName("initial"))
    if err != nil {
        t.Fatal(err)
    }

    connStr, _ := pgContainer.ConnectionString(ctx, "sslmode=disable")
    db, _ := sql.Open("pgx", connStr)
    defer db.Close()

    t.Run("test that modifies data", func(t *testing.T) {
        // Insertar datos
        db.Exec("INSERT INTO users (name) VALUES ('New User')")

        var count int
        db.QueryRow("SELECT COUNT(*) FROM users").Scan(&count)
        t.Logf("after insert: %d users", count)
    })

    // Restaurar al estado inicial — los datos insertados desaparecen
    err = pgContainer.Restore(ctx, postgres.WithSnapshotName("initial"))
    if err != nil {
        t.Fatal(err)
    }

    t.Run("test with clean state", func(t *testing.T) {
        var count int
        db.QueryRow("SELECT COUNT(*) FROM users").Scan(&count)
        t.Logf("after restore: %d users (same as seed)", count)
    })
}
```

### 8.5 Modulo Redis

```go
package integration_test

import (
    "context"
    "testing"

    "github.com/redis/go-redis/v9"
    tcredis "github.com/testcontainers/testcontainers-go/modules/redis"
)

func TestWithRedis(t *testing.T) {
    if testing.Short() {
        t.Skip("skipping integration test")
    }

    ctx := context.Background()

    redisContainer, err := tcredis.Run(ctx, "redis:7-alpine")
    if err != nil {
        t.Fatal(err)
    }
    t.Cleanup(func() { redisContainer.Terminate(ctx) })

    // Obtener connection string
    connStr, err := redisContainer.ConnectionString(ctx)
    if err != nil {
        t.Fatal(err)
    }

    // Conectar con go-redis
    opts, err := redis.ParseURL(connStr)
    if err != nil {
        t.Fatal(err)
    }
    client := redis.NewClient(opts)
    defer client.Close()

    // Testear operaciones
    t.Run("set and get", func(t *testing.T) {
        err := client.Set(ctx, "greeting", "hello", 0).Err()
        if err != nil {
            t.Fatal(err)
        }

        val, err := client.Get(ctx, "greeting").Result()
        if err != nil {
            t.Fatal(err)
        }
        if val != "hello" {
            t.Errorf("expected hello, got %s", val)
        }
    })

    t.Run("list operations", func(t *testing.T) {
        client.RPush(ctx, "queue", "a", "b", "c")

        length, _ := client.LLen(ctx, "queue").Result()
        if length != 3 {
            t.Errorf("expected length 3, got %d", length)
        }

        val, _ := client.LPop(ctx, "queue").Result()
        if val != "a" {
            t.Errorf("expected 'a', got %s", val)
        }
    })
}
```

### 8.6 Modulo MySQL

```go
func TestWithMySQL(t *testing.T) {
    if testing.Short() {
        t.Skip("skipping integration test")
    }

    ctx := context.Background()

    mysqlContainer, err := mysql.Run(ctx,
        "mysql:8.0",
        mysql.WithDatabase("testdb"),
        mysql.WithUsername("testuser"),
        mysql.WithPassword("testpass"),
        mysql.WithScripts("testdata/schema.sql"),
    )
    if err != nil {
        t.Fatal(err)
    }
    t.Cleanup(func() { mysqlContainer.Terminate(ctx) })

    connStr, err := mysqlContainer.ConnectionString(ctx)
    if err != nil {
        t.Fatal(err)
    }

    db, err := sql.Open("mysql", connStr)
    if err != nil {
        t.Fatal(err)
    }
    defer db.Close()

    // Testear contra MySQL real
    _, err = db.Exec("INSERT INTO products (name, price) VALUES (?, ?)", "Widget", 9.99)
    if err != nil {
        t.Fatal(err)
    }
}
```

### 8.7 Container con Dockerfile custom

Cuando necesitas una imagen personalizada:

```go
func TestWithCustomDockerfile(t *testing.T) {
    ctx := context.Background()

    req := testcontainers.ContainerRequest{
        FromDockerfile: testcontainers.FromDockerfile{
            Context:    ".",                    // directorio de contexto
            Dockerfile: "testdata/Dockerfile",  // path relativo al contexto
            // O usar un Dockerfile inline:
            // Dockerfile: "Dockerfile.test",
        },
        ExposedPorts: []string{"8080/tcp"},
        WaitingFor:   wait.ForHTTP("/health").WithPort("8080/tcp"),
    }

    container, err := testcontainers.GenericContainer(ctx, testcontainers.GenericContainerRequest{
        ContainerRequest: req,
        Started:          true,
    })
    if err != nil {
        t.Fatal(err)
    }
    t.Cleanup(func() { container.Terminate(ctx) })

    // ...
}
```

### 8.8 Wait strategies

testcontainers necesita saber cuando el container esta listo. Hay varias estrategias:

```go
// Esperar por un log especifico
wait.ForLog("Server started on port 8080")

// Esperar por log con ocurrencia
wait.ForLog("ready").WithOccurrence(2) // aparece 2 veces

// Esperar por puerto TCP abierto
wait.ForListeningPort("5432/tcp")

// Esperar por endpoint HTTP
wait.ForHTTP("/health").
    WithPort("8080/tcp").
    WithStatusCodeMatcher(func(status int) bool {
        return status == http.StatusOK
    })

// Esperar por SQL query
wait.ForSQL("5432/tcp", "pgx",
    func(host string, port nat.Port) string {
        return fmt.Sprintf("postgres://user:pass@%s:%s/db?sslmode=disable", host, port.Port())
    },
)

// Combinar multiples estrategias (ALL deben cumplirse)
wait.ForAll(
    wait.ForLog("accepting connections"),
    wait.ForListeningPort("5432/tcp"),
)

// Timeouts
wait.ForLog("ready").WithStartupTimeout(60 * time.Second)
wait.ForLog("ready").WithPollInterval(500 * time.Millisecond)
```

### 8.9 Patron helper — reutilizar setup

```go
// testhelpers_test.go — helpers reutilizables

type testPostgres struct {
    container *postgres.PostgresContainer
    connStr   string
    db        *sql.DB
}

func setupPostgres(t *testing.T) *testPostgres {
    t.Helper()

    if testing.Short() {
        t.Skip("skipping integration test in short mode")
    }

    ctx := context.Background()

    pgContainer, err := postgres.Run(ctx,
        "postgres:16-alpine",
        postgres.WithDatabase("testdb"),
        postgres.WithUsername("test"),
        postgres.WithPassword("test"),
        postgres.WithInitScripts("testdata/schema.sql"),
        postgres.WithSQLDriver("pgx"),
        testcontainers.WithWaitStrategy(
            wait.ForLog("database system is ready to accept connections").
                WithOccurrence(2).
                WithStartupTimeout(30*time.Second),
        ),
    )
    if err != nil {
        t.Fatal(err)
    }

    connStr, err := pgContainer.ConnectionString(ctx, "sslmode=disable")
    if err != nil {
        t.Fatal(err)
    }

    db, err := sql.Open("pgx", connStr)
    if err != nil {
        t.Fatal(err)
    }

    t.Cleanup(func() {
        db.Close()
        pgContainer.Terminate(ctx)
    })

    return &testPostgres{
        container: pgContainer,
        connStr:   connStr,
        db:        db,
    }
}

// Uso en tests — una sola linea de setup:
func TestUserRepo_Create(t *testing.T) {
    pg := setupPostgres(t)
    repo := NewPostgresUserRepo(pg.db)

    user, err := repo.Create(User{Name: "Alice", Email: "alice@test.com"})
    if err != nil {
        t.Fatal(err)
    }
    if user.ID == 0 {
        t.Error("expected user to have an ID")
    }
}

func TestUserRepo_FindByEmail(t *testing.T) {
    pg := setupPostgres(t)
    repo := NewPostgresUserRepo(pg.db)

    repo.Create(User{Name: "Bob", Email: "bob@test.com"})

    found, err := repo.FindByEmail("bob@test.com")
    if err != nil {
        t.Fatal(err)
    }
    if found.Name != "Bob" {
        t.Errorf("expected Bob, got %s", found.Name)
    }
}
```

### 8.10 TestMain para container compartido

Cuando multiples tests usan el mismo container y quieres evitar re-crearlo:

```go
package integration_test

import (
    "context"
    "database/sql"
    "fmt"
    "os"
    "testing"

    "github.com/testcontainers/testcontainers-go/modules/postgres"
    "github.com/testcontainers/testcontainers-go/wait"
    "github.com/testcontainers/testcontainers-go"
    _ "github.com/jackc/pgx/v5/stdlib"
)

var testDB *sql.DB

func TestMain(m *testing.M) {
    ctx := context.Background()

    pgContainer, err := postgres.Run(ctx,
        "postgres:16-alpine",
        postgres.WithDatabase("testdb"),
        postgres.WithUsername("test"),
        postgres.WithPassword("test"),
        postgres.WithInitScripts("testdata/schema.sql"),
        postgres.WithSQLDriver("pgx"),
        testcontainers.WithWaitStrategy(
            wait.ForLog("database system is ready to accept connections").
                WithOccurrence(2),
        ),
    )
    if err != nil {
        fmt.Fprintf(os.Stderr, "failed to start postgres: %v\n", err)
        os.Exit(1)
    }

    connStr, err := pgContainer.ConnectionString(ctx, "sslmode=disable")
    if err != nil {
        fmt.Fprintf(os.Stderr, "failed to get connection string: %v\n", err)
        pgContainer.Terminate(ctx)
        os.Exit(1)
    }

    testDB, err = sql.Open("pgx", connStr)
    if err != nil {
        fmt.Fprintf(os.Stderr, "failed to connect: %v\n", err)
        pgContainer.Terminate(ctx)
        os.Exit(1)
    }

    // Correr todos los tests
    code := m.Run()

    // Cleanup
    testDB.Close()
    pgContainer.Terminate(ctx)

    os.Exit(code)
}

// Todos los tests comparten testDB
func TestCreateUser(t *testing.T) {
    // Limpiar tabla entre tests
    t.Cleanup(func() {
        testDB.Exec("DELETE FROM users")
    })

    _, err := testDB.Exec("INSERT INTO users (name, email) VALUES ($1, $2)", "Alice", "a@test.com")
    if err != nil {
        t.Fatal(err)
    }

    var count int
    testDB.QueryRow("SELECT COUNT(*) FROM users").Scan(&count)
    if count != 1 {
        t.Errorf("expected 1 user, got %d", count)
    }
}
```

---

## 9. Build tags para separar tests

### 9.1 Separar unit tests de integration tests

Los integration tests (con testcontainers) son lentos y requieren Docker. Usa build tags para separarlos:

```go
// user_repo_integration_test.go
//go:build integration

package repo_test

import (
    "testing"
    // ... testcontainers imports
)

func TestUserRepo_Integration(t *testing.T) {
    pg := setupPostgres(t)
    // ... tests contra DB real
}
```

```go
// user_repo_test.go (sin build tag — siempre se ejecuta)
package repo_test

import "testing"

func TestUserRepo_Unit(t *testing.T) {
    repo := NewUserRepo(newFakeDB())
    // ... tests con fake
}
```

```bash
# Solo unit tests (default — no incluye tag integration)
go test ./...

# Solo integration tests
go test -tags=integration ./...

# Ambos
go test -tags=integration -count=1 ./...

# testing.Short() como alternativa (sin build tags)
go test -short ./...   # salta tests que llaman t.Skip en Short mode
```

### 9.2 Patron con -short flag

Alternativa mas simple a build tags:

```go
func TestWithDatabase(t *testing.T) {
    if testing.Short() {
        t.Skip("skipping integration test in short mode")
    }

    pg := setupPostgres(t)
    // ...
}
```

```bash
# Rapido — solo unit tests
go test -short ./...

# Completo — incluye integration
go test -count=1 ./...
```

### 9.3 Organizacion de archivos

```
myapp/
├── service/
│   ├── user.go                    # codigo de produccion
│   ├── user_test.go               # unit tests (siempre corren)
│   ├── user_integration_test.go   # integration tests (build tag o -short)
│   ├── testdata/
│   │   ├── schema.sql             # DDL para testcontainers
│   │   ├── seed.sql               # datos de prueba
│   │   └── golden/                # golden files
│   └── testhelpers_test.go        # helpers compartidos (setupPostgres, etc.)
├── store/
│   ├── postgres.go
│   ├── postgres_test.go           # unit tests con fakes
│   └── postgres_integration_test.go  # tests con testcontainers
└── go.mod
```

---

## 10. Patron completo — diseño para testabilidad

### 10.1 Ejemplo real: servicio de pedidos

Veamos un ejemplo completo que integra todos los conceptos — interfaces, DI, mocks manuales, y testcontainers:

```go
// ============================================================================
// domain.go — tipos del dominio (sin dependencias)
// ============================================================================

package order

import (
    "fmt"
    "time"
)

type Order struct {
    ID        string
    UserID    string
    Items     []OrderItem
    Status    OrderStatus
    Total     float64
    CreatedAt time.Time
}

type OrderItem struct {
    ProductID string
    Name      string
    Price     float64
    Quantity  int
}

type OrderStatus string

const (
    StatusPending   OrderStatus = "pending"
    StatusConfirmed OrderStatus = "confirmed"
    StatusShipped   OrderStatus = "shipped"
    StatusCancelled OrderStatus = "cancelled"
)

var ErrNotFound = fmt.Errorf("not found")
var ErrInsufficientStock = fmt.Errorf("insufficient stock")

// ============================================================================
// ports.go — interfaces (definidas por el consumidor)
// ============================================================================

// OrderRepository — persistencia de pedidos
type OrderRepository interface {
    Save(order Order) error
    FindByID(id string) (Order, error)
    FindByUser(userID string) ([]Order, error)
    UpdateStatus(id string, status OrderStatus) error
}

// ProductChecker — verificar disponibilidad de productos
type ProductChecker interface {
    CheckStock(productID string, quantity int) (bool, error)
    ReserveStock(productID string, quantity int) error
}

// PaymentProcessor — procesar pagos
type PaymentProcessor interface {
    Charge(userID string, amount float64) (transactionID string, err error)
    Refund(transactionID string) error
}

// Notifier — enviar notificaciones
type Notifier interface {
    OrderConfirmed(order Order) error
    OrderShipped(order Order) error
    OrderCancelled(order Order) error
}

// Clock — abstraccion del tiempo
type Clock interface {
    Now() time.Time
}

// ============================================================================
// service.go — logica de negocio (solo depende de interfaces)
// ============================================================================

type Service struct {
    orders   OrderRepository
    products ProductChecker
    payments PaymentProcessor
    notifier Notifier
    clock    Clock
}

func NewService(
    orders OrderRepository,
    products ProductChecker,
    payments PaymentProcessor,
    notifier Notifier,
    clock Clock,
) *Service {
    return &Service{
        orders:   orders,
        products: products,
        payments: payments,
        notifier: notifier,
        clock:    clock,
    }
}

func (s *Service) PlaceOrder(userID string, items []OrderItem) (Order, error) {
    // 1. Verificar stock
    for _, item := range items {
        available, err := s.products.CheckStock(item.ProductID, item.Quantity)
        if err != nil {
            return Order{}, fmt.Errorf("checking stock for %s: %w", item.ProductID, err)
        }
        if !available {
            return Order{}, fmt.Errorf("product %s: %w", item.ProductID, ErrInsufficientStock)
        }
    }

    // 2. Calcular total
    var total float64
    for _, item := range items {
        total += item.Price * float64(item.Quantity)
    }

    // 3. Procesar pago
    txID, err := s.payments.Charge(userID, total)
    if err != nil {
        return Order{}, fmt.Errorf("processing payment: %w", err)
    }

    // 4. Reservar stock
    for _, item := range items {
        if err := s.products.ReserveStock(item.ProductID, item.Quantity); err != nil {
            // Rollback: reembolsar el pago
            s.payments.Refund(txID)
            return Order{}, fmt.Errorf("reserving stock for %s: %w", item.ProductID, err)
        }
    }

    // 5. Crear pedido
    order := Order{
        ID:        fmt.Sprintf("ord-%d", s.clock.Now().UnixNano()),
        UserID:    userID,
        Items:     items,
        Status:    StatusConfirmed,
        Total:     total,
        CreatedAt: s.clock.Now(),
    }

    if err := s.orders.Save(order); err != nil {
        return Order{}, fmt.Errorf("saving order: %w", err)
    }

    // 6. Notificar (no-fail: logueamos pero no frenamos el flujo)
    _ = s.notifier.OrderConfirmed(order)

    return order, nil
}

func (s *Service) CancelOrder(orderID string) error {
    order, err := s.orders.FindByID(orderID)
    if err != nil {
        return fmt.Errorf("finding order: %w", err)
    }

    if order.Status == StatusShipped {
        return fmt.Errorf("cannot cancel shipped order")
    }

    if err := s.orders.UpdateStatus(orderID, StatusCancelled); err != nil {
        return fmt.Errorf("updating status: %w", err)
    }

    _ = s.notifier.OrderCancelled(order)
    return nil
}

func (s *Service) GetUserOrders(userID string) ([]Order, error) {
    return s.orders.FindByUser(userID)
}
```

### 10.2 Unit tests con mocks manuales

```go
// ============================================================================
// service_test.go — unit tests con test doubles manuales
// ============================================================================

package order

import (
    "fmt"
    "sync"
    "testing"
    "time"
)

// --- Test doubles ---

type fakeOrderRepo struct {
    mu     sync.Mutex
    orders map[string]Order
}

func newFakeOrderRepo() *fakeOrderRepo {
    return &fakeOrderRepo{orders: make(map[string]Order)}
}

func (f *fakeOrderRepo) Save(order Order) error {
    f.mu.Lock()
    defer f.mu.Unlock()
    f.orders[order.ID] = order
    return nil
}

func (f *fakeOrderRepo) FindByID(id string) (Order, error) {
    f.mu.Lock()
    defer f.mu.Unlock()
    o, ok := f.orders[id]
    if !ok {
        return Order{}, ErrNotFound
    }
    return o, nil
}

func (f *fakeOrderRepo) FindByUser(userID string) ([]Order, error) {
    f.mu.Lock()
    defer f.mu.Unlock()
    var result []Order
    for _, o := range f.orders {
        if o.UserID == userID {
            result = append(result, o)
        }
    }
    return result, nil
}

func (f *fakeOrderRepo) UpdateStatus(id string, status OrderStatus) error {
    f.mu.Lock()
    defer f.mu.Unlock()
    o, ok := f.orders[id]
    if !ok {
        return ErrNotFound
    }
    o.Status = status
    f.orders[id] = o
    return nil
}

// ---

type fakeProducts struct {
    stock map[string]int // productID -> available
}

func (f *fakeProducts) CheckStock(productID string, qty int) (bool, error) {
    avail, ok := f.stock[productID]
    if !ok {
        return false, nil
    }
    return avail >= qty, nil
}

func (f *fakeProducts) ReserveStock(productID string, qty int) error {
    avail := f.stock[productID]
    if avail < qty {
        return ErrInsufficientStock
    }
    f.stock[productID] = avail - qty
    return nil
}

// ---

type fakePayments struct {
    chargeErr error
    refunded  []string
}

func (f *fakePayments) Charge(userID string, amount float64) (string, error) {
    if f.chargeErr != nil {
        return "", f.chargeErr
    }
    return fmt.Sprintf("tx-%s-%.0f", userID, amount), nil
}

func (f *fakePayments) Refund(txID string) error {
    f.refunded = append(f.refunded, txID)
    return nil
}

// ---

type spyNotifier struct {
    confirmed []Order
    shipped   []Order
    cancelled []Order
}

func (s *spyNotifier) OrderConfirmed(order Order) error {
    s.confirmed = append(s.confirmed, order)
    return nil
}

func (s *spyNotifier) OrderShipped(order Order) error {
    s.shipped = append(s.shipped, order)
    return nil
}

func (s *spyNotifier) OrderCancelled(order Order) error {
    s.cancelled = append(s.cancelled, order)
    return nil
}

// ---

type fixedClock struct{ t time.Time }

func (f *fixedClock) Now() time.Time { return f.t }

// --- Helper para crear el servicio de tests ---

type testEnv struct {
    svc      *Service
    orders   *fakeOrderRepo
    products *fakeProducts
    payments *fakePayments
    notifier *spyNotifier
    clock    *fixedClock
}

func setupTestEnv() *testEnv {
    orders := newFakeOrderRepo()
    products := &fakeProducts{stock: map[string]int{
        "prod-1": 100,
        "prod-2": 50,
        "prod-3": 0, // sin stock
    }}
    payments := &fakePayments{}
    notifier := &spyNotifier{}
    clock := &fixedClock{t: time.Date(2024, 6, 15, 10, 0, 0, 0, time.UTC)}

    svc := NewService(orders, products, payments, notifier, clock)

    return &testEnv{
        svc:      svc,
        orders:   orders,
        products: products,
        payments: payments,
        notifier: notifier,
        clock:    clock,
    }
}

// --- Tests ---

func TestPlaceOrder_Success(t *testing.T) {
    env := setupTestEnv()

    items := []OrderItem{
        {ProductID: "prod-1", Name: "Widget", Price: 9.99, Quantity: 2},
        {ProductID: "prod-2", Name: "Gadget", Price: 19.99, Quantity: 1},
    }

    order, err := env.svc.PlaceOrder("user-1", items)
    if err != nil {
        t.Fatalf("unexpected error: %v", err)
    }

    // Verificar pedido
    if order.UserID != "user-1" {
        t.Errorf("expected UserID=user-1, got %s", order.UserID)
    }
    if order.Status != StatusConfirmed {
        t.Errorf("expected status=confirmed, got %s", order.Status)
    }

    expectedTotal := 9.99*2 + 19.99*1
    if order.Total != expectedTotal {
        t.Errorf("expected total=%.2f, got %.2f", expectedTotal, order.Total)
    }

    // Verificar que se guardo
    saved, err := env.orders.FindByID(order.ID)
    if err != nil {
        t.Fatalf("order not found in repo: %v", err)
    }
    if saved.ID != order.ID {
        t.Error("saved order ID mismatch")
    }

    // Verificar que se notifico
    if len(env.notifier.confirmed) != 1 {
        t.Errorf("expected 1 confirmation, got %d", len(env.notifier.confirmed))
    }

    // Verificar que el stock se redujo
    if env.products.stock["prod-1"] != 98 {
        t.Errorf("expected stock=98, got %d", env.products.stock["prod-1"])
    }
    if env.products.stock["prod-2"] != 49 {
        t.Errorf("expected stock=49, got %d", env.products.stock["prod-2"])
    }
}

func TestPlaceOrder_InsufficientStock(t *testing.T) {
    env := setupTestEnv()

    items := []OrderItem{
        {ProductID: "prod-3", Name: "Rare Item", Price: 99.99, Quantity: 1},
    }

    _, err := env.svc.PlaceOrder("user-1", items)
    if err == nil {
        t.Fatal("expected error for out-of-stock product")
    }

    // Verificar que NO se proceso pago
    if len(env.payments.refunded) != 0 {
        t.Error("should not have charged/refunded")
    }

    // Verificar que NO se notifico
    if len(env.notifier.confirmed) != 0 {
        t.Error("should not have sent confirmation")
    }
}

func TestPlaceOrder_PaymentFailure(t *testing.T) {
    env := setupTestEnv()
    env.payments.chargeErr = fmt.Errorf("card declined")

    items := []OrderItem{
        {ProductID: "prod-1", Name: "Widget", Price: 9.99, Quantity: 1},
    }

    _, err := env.svc.PlaceOrder("user-1", items)
    if err == nil {
        t.Fatal("expected error for payment failure")
    }

    // Stock no debio cambiar (pago fallo antes de reservar)
    if env.products.stock["prod-1"] != 100 {
        t.Errorf("stock should be unchanged, got %d", env.products.stock["prod-1"])
    }
}

func TestPlaceOrder_StockReservationFailure_RefundsPayment(t *testing.T) {
    env := setupTestEnv()

    // Producto 1 tiene stock, producto 2 no tiene suficiente
    env.products.stock["prod-2"] = 0

    items := []OrderItem{
        {ProductID: "prod-1", Name: "Widget", Price: 9.99, Quantity: 1},
        {ProductID: "prod-2", Name: "Gadget", Price: 19.99, Quantity: 5}, // stock=0 pero CheckStock retorna false
    }

    // CheckStock retorna false para prod-2 con qty=5 (stock=0)
    _, err := env.svc.PlaceOrder("user-1", items)
    if err == nil {
        t.Fatal("expected error for insufficient stock")
    }
}

func TestCancelOrder_Success(t *testing.T) {
    env := setupTestEnv()

    // Crear pedido primero
    order := Order{
        ID:     "ord-1",
        UserID: "user-1",
        Status: StatusConfirmed,
    }
    env.orders.Save(order)

    err := env.svc.CancelOrder("ord-1")
    if err != nil {
        t.Fatalf("unexpected error: %v", err)
    }

    // Verificar estado
    updated, _ := env.orders.FindByID("ord-1")
    if updated.Status != StatusCancelled {
        t.Errorf("expected status=cancelled, got %s", updated.Status)
    }

    // Verificar notificacion
    if len(env.notifier.cancelled) != 1 {
        t.Errorf("expected 1 cancellation notification, got %d", len(env.notifier.cancelled))
    }
}

func TestCancelOrder_CannotCancelShipped(t *testing.T) {
    env := setupTestEnv()

    order := Order{
        ID:     "ord-1",
        UserID: "user-1",
        Status: StatusShipped,
    }
    env.orders.Save(order)

    err := env.svc.CancelOrder("ord-1")
    if err == nil {
        t.Fatal("expected error when cancelling shipped order")
    }
}

func TestCancelOrder_NotFound(t *testing.T) {
    env := setupTestEnv()

    err := env.svc.CancelOrder("nonexistent")
    if err == nil {
        t.Fatal("expected error for non-existent order")
    }
}

func TestGetUserOrders(t *testing.T) {
    env := setupTestEnv()

    // Crear pedidos para dos usuarios
    env.orders.Save(Order{ID: "ord-1", UserID: "user-1", Status: StatusConfirmed})
    env.orders.Save(Order{ID: "ord-2", UserID: "user-1", Status: StatusShipped})
    env.orders.Save(Order{ID: "ord-3", UserID: "user-2", Status: StatusConfirmed})

    orders, err := env.svc.GetUserOrders("user-1")
    if err != nil {
        t.Fatal(err)
    }

    if len(orders) != 2 {
        t.Errorf("expected 2 orders for user-1, got %d", len(orders))
    }
}
```

### 10.3 Integration tests con testcontainers

```go
// ============================================================================
// service_integration_test.go — tests contra PostgreSQL real
// ============================================================================

//go:build integration

package order_test

import (
    "context"
    "database/sql"
    "fmt"
    "os"
    "testing"
    "time"

    "github.com/testcontainers/testcontainers-go/modules/postgres"
    "github.com/testcontainers/testcontainers-go"
    "github.com/testcontainers/testcontainers-go/wait"
    _ "github.com/jackc/pgx/v5/stdlib"

    "myapp/order"
)

var testDB *sql.DB

func TestMain(m *testing.M) {
    ctx := context.Background()

    pgContainer, err := postgres.Run(ctx,
        "postgres:16-alpine",
        postgres.WithDatabase("ordertest"),
        postgres.WithUsername("test"),
        postgres.WithPassword("test"),
        postgres.WithInitScripts("testdata/schema.sql"),
        postgres.WithSQLDriver("pgx"),
        testcontainers.WithWaitStrategy(
            wait.ForLog("database system is ready to accept connections").
                WithOccurrence(2).
                WithStartupTimeout(30*time.Second),
        ),
    )
    if err != nil {
        fmt.Fprintf(os.Stderr, "postgres: %v\n", err)
        os.Exit(1)
    }

    connStr, _ := pgContainer.ConnectionString(ctx, "sslmode=disable")
    testDB, err = sql.Open("pgx", connStr)
    if err != nil {
        fmt.Fprintf(os.Stderr, "connect: %v\n", err)
        os.Exit(1)
    }

    code := m.Run()

    testDB.Close()
    pgContainer.Terminate(ctx)
    os.Exit(code)
}

func cleanDB(t *testing.T) {
    t.Helper()
    t.Cleanup(func() {
        testDB.Exec("DELETE FROM order_items")
        testDB.Exec("DELETE FROM orders")
    })
}

func TestPostgresOrderRepo_SaveAndFind(t *testing.T) {
    cleanDB(t)

    repo := order.NewPostgresOrderRepo(testDB)

    ord := order.Order{
        ID:        "ord-integration-1",
        UserID:    "user-1",
        Status:    order.StatusConfirmed,
        Total:     49.97,
        CreatedAt: time.Now().UTC().Truncate(time.Microsecond),
        Items: []order.OrderItem{
            {ProductID: "p-1", Name: "Widget", Price: 9.99, Quantity: 5},
        },
    }

    err := repo.Save(ord)
    if err != nil {
        t.Fatalf("Save failed: %v", err)
    }

    found, err := repo.FindByID("ord-integration-1")
    if err != nil {
        t.Fatalf("FindByID failed: %v", err)
    }

    if found.UserID != "user-1" {
        t.Errorf("expected UserID=user-1, got %s", found.UserID)
    }
    if found.Total != 49.97 {
        t.Errorf("expected Total=49.97, got %f", found.Total)
    }
    if len(found.Items) != 1 {
        t.Errorf("expected 1 item, got %d", len(found.Items))
    }
}

func TestPostgresOrderRepo_FindByUser(t *testing.T) {
    cleanDB(t)

    repo := order.NewPostgresOrderRepo(testDB)

    // Insertar pedidos
    for i := range 3 {
        repo.Save(order.Order{
            ID:        fmt.Sprintf("ord-%d", i),
            UserID:    "user-1",
            Status:    order.StatusConfirmed,
            Total:     float64(i) * 10,
            CreatedAt: time.Now().UTC(),
        })
    }
    repo.Save(order.Order{
        ID:        "ord-other",
        UserID:    "user-2",
        Status:    order.StatusConfirmed,
        CreatedAt: time.Now().UTC(),
    })

    orders, err := repo.FindByUser("user-1")
    if err != nil {
        t.Fatal(err)
    }
    if len(orders) != 3 {
        t.Errorf("expected 3 orders for user-1, got %d", len(orders))
    }
}

func TestPostgresOrderRepo_UpdateStatus(t *testing.T) {
    cleanDB(t)

    repo := order.NewPostgresOrderRepo(testDB)

    repo.Save(order.Order{
        ID:        "ord-status",
        UserID:    "user-1",
        Status:    order.StatusPending,
        CreatedAt: time.Now().UTC(),
    })

    err := repo.UpdateStatus("ord-status", order.StatusShipped)
    if err != nil {
        t.Fatal(err)
    }

    updated, _ := repo.FindByID("ord-status")
    if updated.Status != order.StatusShipped {
        t.Errorf("expected status=shipped, got %s", updated.Status)
    }
}

func TestPostgresOrderRepo_FindByID_NotFound(t *testing.T) {
    cleanDB(t)

    repo := order.NewPostgresOrderRepo(testDB)

    _, err := repo.FindByID("nonexistent")
    if err == nil {
        t.Fatal("expected error for non-existent order")
    }
}
```

---

## 11. Comparacion con C y Rust

| Aspecto | Go | C | Rust |
|---------|-----|---|------|
| **Interfaces** | Implicitas (duck typing) | No existen (punteros a funciones) | Traits explicitos (`impl Trait for Type`) |
| **Satisfaccion** | Automatica por metodos | Manual (vtable manual) | `impl` block obligatorio |
| **Test doubles** | Structs que implementan interfaz | Punteros a funciones mock | `mockall`, `mockito` crates |
| **DI** | Constructor injection manual | Punteros a funciones en struct | Generics + traits bounds |
| **DI framework** | Wire (opcional) | No | No (traits hacen DI innecesario) |
| **Mock generation** | gomock, moq, counterfeiter | No hay estandar | `mockall` proc macro |
| **Mock complejidad** | Baja (interfaces pequenas) | Alta (vtables manuales) | Media (macros generan codigo) |
| **Test containers** | testcontainers-go | No hay estandar | testcontainers-rs |
| **Build tags** | `//go:build integration` | `#ifdef TEST` | `#[cfg(test)]` |
| **Test files** | `_test.go` (mismo/diferente paquete) | Archivos separados (no hay convencion) | `#[cfg(test)] mod tests` en mismo archivo |
| **Parallelism** | `t.Parallel()` | Manual (pthreads) | `#[test]` paralelo por default |
| **time mock** | Interfaz Clock | Funcion pointer | `mock_instant`, traits |
| **HTTP mock** | `httptest.NewServer` | Manual | `mockito`, `wiremock` |
| **Filesystem** | Interfaz + fake | No (depende de POSIX) | `tempfile` crate |
| **Patron mock** | Accept interfaces, return structs | Struct con function pointers | Traits bounds en generics |

### Ejemplo comparativo — mock de repositorio

**Go:**
```go
// Interfaz
type UserStore interface {
    Get(id string) (User, error)
}

// Mock
type mockStore struct {
    user User
    err  error
}
func (m *mockStore) Get(id string) (User, error) { return m.user, m.err }

// Test
func TestHandler(t *testing.T) {
    mock := &mockStore{user: User{Name: "Alice"}}
    handler := NewHandler(mock)
    // ...
}
```

**Rust:**
```rust
// Trait
trait UserStore {
    fn get(&self, id: &str) -> Result<User, Error>;
}

// Mock con mockall
#[cfg(test)]
use mockall::automock;

#[automock]
trait UserStore {
    fn get(&self, id: &str) -> Result<User, Error>;
}

#[test]
fn test_handler() {
    let mut mock = MockUserStore::new();
    mock.expect_get()
        .with(eq("123"))
        .returning(|_| Ok(User { name: "Alice".into() }));
    let handler = Handler::new(mock);
    // ...
}
```

**C:**
```c
// "Interfaz" con function pointers
typedef struct {
    int (*get)(const char *id, User *out);
} UserStore;

// Mock
static User mock_user;
static int mock_get(const char *id, User *out) {
    *out = mock_user;
    return 0;
}

// Test
void test_handler(void) {
    UserStore store = { .get = mock_get };
    mock_user = (User){ .name = "Alice" };
    Handler h = new_handler(&store);
    // ...
}
```

**Diferencias clave:**

1. **Go vs Rust**: Go usa interfaces implicitas (no hay `impl` block) lo que hace que crear test doubles sea mas rapido pero con menos garantias estaticas. Rust usa traits explicitos con macros (`mockall`) que generan mocks con type safety completo.

2. **Go vs C**: C no tiene interfaces — usa structs con punteros a funciones como vtables manuales. Esto es fragil (no hay verificacion automatica de que el mock tiene los metodos correctos) y verbose.

3. **Testcontainers**: Go y Rust tienen librerias maduras. C no tiene equivalente estandar — normalmente se usa Docker Compose o scripts de shell.

4. **DI en Go vs Rust**: En Go, DI es pasar interfaces como parametros. En Rust, los generics con trait bounds logran lo mismo en compile time: `fn new<S: UserStore>(store: S)`. No hay "DI frameworks" en Rust porque los traits resuelven el problema estaticamente.

---

## 12. Programa de practica

Implementa un **sistema de cache con persistencia** que use interfaces, DI, mocks, y testcontainers. El sistema tiene un cache en memoria con fallback a Redis, y los tests cubren unit (con mocks) e integration (con testcontainers).

```go
// ============================================================================
// Programa: Cache con persistencia — interfaces, DI, mocking, testcontainers
// ============================================================================
//
// Estructura:
//   cache/
//   ├── cache.go              ← codigo de produccion
//   ├── cache_test.go         ← unit tests con mocks
//   ├── cache_integration_test.go  ← integration tests con testcontainers
//   └── testdata/
//       └── (vacio — Redis no necesita schema)
//
// Ejecutar:
//   go test ./cache/...                          # unit tests
//   go test -tags=integration ./cache/...        # integration tests
//   go test -tags=integration -v ./cache/...     # verbose
//   go test -short ./cache/...                   # solo unit (rapido)

// ============================================================================
// cache.go
// ============================================================================

package cache

import (
    "context"
    "fmt"
    "sync"
    "time"
)

// --- Interfaces (definidas por el consumidor) ---

// Store — backend de almacenamiento (Redis, memcached, etc.)
type Store interface {
    Get(ctx context.Context, key string) ([]byte, error)
    Set(ctx context.Context, key string, value []byte, ttl time.Duration) error
    Delete(ctx context.Context, key string) error
    Exists(ctx context.Context, key string) (bool, error)
}

// Serializer — serializacion/deserializacion de valores
type Serializer interface {
    Marshal(v any) ([]byte, error)
    Unmarshal(data []byte, v any) error
}

// Logger — logging configurable
type Logger interface {
    Info(msg string, args ...any)
    Error(msg string, args ...any)
}

// Clock — abstraccion de tiempo
type Clock interface {
    Now() time.Time
}

// Metrics — metricas observables
type Metrics interface {
    IncHit()
    IncMiss()
    IncError()
    ObserveLatency(operation string, duration time.Duration)
}

// --- Errores ---

var (
    ErrNotFound  = fmt.Errorf("cache: key not found")
    ErrExpired   = fmt.Errorf("cache: key expired")
    ErrSerialization = fmt.Errorf("cache: serialization error")
)

// --- Implementacion: Cache con L1 (memoria) + L2 (Store) ---

type entry struct {
    data      []byte
    expiresAt time.Time
}

type Cache struct {
    l1       sync.Map       // in-memory L1
    l2       Store          // external L2 (Redis, etc.)
    serial   Serializer
    logger   Logger
    clock    Clock
    metrics  Metrics
    defaultTTL time.Duration
}

type Option func(*Cache)

func WithDefaultTTL(ttl time.Duration) Option {
    return func(c *Cache) { c.defaultTTL = ttl }
}

func WithLogger(l Logger) Option {
    return func(c *Cache) { c.logger = l }
}

func WithMetrics(m Metrics) Option {
    return func(c *Cache) { c.metrics = m }
}

func WithClock(cl Clock) Option {
    return func(c *Cache) { c.clock = cl }
}

func New(store Store, serial Serializer, opts ...Option) *Cache {
    c := &Cache{
        l2:         store,
        serial:     serial,
        logger:     &noopLogger{},
        clock:      &realClock{},
        metrics:    &noopMetrics{},
        defaultTTL: 5 * time.Minute,
    }
    for _, opt := range opts {
        opt(c)
    }
    return c
}

// Get — busca en L1, luego L2. Retorna ErrNotFound si no existe.
func (c *Cache) Get(ctx context.Context, key string, dest any) error {
    start := c.clock.Now()
    defer func() {
        c.metrics.ObserveLatency("get", c.clock.Now().Sub(start))
    }()

    // L1: memoria
    if val, ok := c.l1.Load(key); ok {
        e := val.(*entry)
        if c.clock.Now().Before(e.expiresAt) {
            c.metrics.IncHit()
            c.logger.Info("L1 hit", "key", key)
            return c.serial.Unmarshal(e.data, dest)
        }
        // Expirado — eliminar de L1
        c.l1.Delete(key)
    }

    // L2: store externo
    data, err := c.l2.Get(ctx, key)
    if err != nil {
        c.metrics.IncMiss()
        c.logger.Info("L2 miss", "key", key)
        return ErrNotFound
    }

    c.metrics.IncHit()
    c.logger.Info("L2 hit", "key", key)

    // Promover a L1
    c.l1.Store(key, &entry{
        data:      data,
        expiresAt: c.clock.Now().Add(c.defaultTTL),
    })

    return c.serial.Unmarshal(data, dest)
}

// Set — escribe en L1 y L2.
func (c *Cache) Set(ctx context.Context, key string, value any, ttl time.Duration) error {
    start := c.clock.Now()
    defer func() {
        c.metrics.ObserveLatency("set", c.clock.Now().Sub(start))
    }()

    if ttl == 0 {
        ttl = c.defaultTTL
    }

    data, err := c.serial.Marshal(value)
    if err != nil {
        c.metrics.IncError()
        return fmt.Errorf("%w: %v", ErrSerialization, err)
    }

    // L1
    c.l1.Store(key, &entry{
        data:      data,
        expiresAt: c.clock.Now().Add(ttl),
    })

    // L2
    if err := c.l2.Set(ctx, key, data, ttl); err != nil {
        c.metrics.IncError()
        c.logger.Error("L2 set failed", "key", key, "error", err)
        return fmt.Errorf("store set: %w", err)
    }

    c.logger.Info("set", "key", key, "ttl", ttl)
    return nil
}

// Delete — elimina de L1 y L2.
func (c *Cache) Delete(ctx context.Context, key string) error {
    c.l1.Delete(key)
    if err := c.l2.Delete(ctx, key); err != nil {
        c.logger.Error("L2 delete failed", "key", key, "error", err)
        return err
    }
    return nil
}

// Invalidate — elimina de L1 solamente (L2 sigue teniendo el valor).
func (c *Cache) Invalidate(key string) {
    c.l1.Delete(key)
}

// --- Implementaciones por defecto ---

type realClock struct{}
func (realClock) Now() time.Time { return time.Now() }

type noopLogger struct{}
func (noopLogger) Info(msg string, args ...any)  {}
func (noopLogger) Error(msg string, args ...any) {}

type noopMetrics struct{}
func (noopMetrics) IncHit()                                        {}
func (noopMetrics) IncMiss()                                       {}
func (noopMetrics) IncError()                                      {}
func (noopMetrics) ObserveLatency(operation string, d time.Duration) {}

// ============================================================================
// cache_test.go — Unit tests con test doubles manuales
// ============================================================================

package cache

import (
    "context"
    "encoding/json"
    "fmt"
    "sync"
    "testing"
    "time"
)

// --- Test doubles ---

// fakeStore — implementacion in-memory de Store
type fakeStore struct {
    mu   sync.Mutex
    data map[string]storeEntry
}

type storeEntry struct {
    value     []byte
    expiresAt time.Time
}

func newFakeStore() *fakeStore {
    return &fakeStore{data: make(map[string]storeEntry)}
}

func (f *fakeStore) Get(ctx context.Context, key string) ([]byte, error) {
    f.mu.Lock()
    defer f.mu.Unlock()
    e, ok := f.data[key]
    if !ok {
        return nil, ErrNotFound
    }
    return e.value, nil
}

func (f *fakeStore) Set(ctx context.Context, key string, val []byte, ttl time.Duration) error {
    f.mu.Lock()
    defer f.mu.Unlock()
    f.data[key] = storeEntry{value: append([]byte(nil), val...), expiresAt: time.Now().Add(ttl)}
    return nil
}

func (f *fakeStore) Delete(ctx context.Context, key string) error {
    f.mu.Lock()
    defer f.mu.Unlock()
    delete(f.data, key)
    return nil
}

func (f *fakeStore) Exists(ctx context.Context, key string) (bool, error) {
    f.mu.Lock()
    defer f.mu.Unlock()
    _, ok := f.data[key]
    return ok, nil
}

// failingStore — siempre falla
type failingStore struct {
    err error
}

func (f *failingStore) Get(ctx context.Context, key string) ([]byte, error)                { return nil, f.err }
func (f *failingStore) Set(ctx context.Context, key string, val []byte, ttl time.Duration) error { return f.err }
func (f *failingStore) Delete(ctx context.Context, key string) error                       { return f.err }
func (f *failingStore) Exists(ctx context.Context, key string) (bool, error)               { return false, f.err }

// jsonSerializer — usa encoding/json
type jsonSerializer struct{}

func (jsonSerializer) Marshal(v any) ([]byte, error)          { return json.Marshal(v) }
func (jsonSerializer) Unmarshal(data []byte, v any) error     { return json.Unmarshal(data, v) }

// fakeClock — reloj controlable
type fakeClock struct {
    mu  sync.Mutex
    now time.Time
}

func newFakeClock(t time.Time) *fakeClock { return &fakeClock{now: t} }
func (f *fakeClock) Now() time.Time       { f.mu.Lock(); defer f.mu.Unlock(); return f.now }
func (f *fakeClock) Advance(d time.Duration) { f.mu.Lock(); defer f.mu.Unlock(); f.now = f.now.Add(d) }

// spyMetrics — registra metricas
type spyMetrics struct {
    mu       sync.Mutex
    hits     int
    misses   int
    errors   int
    latencies map[string][]time.Duration
}

func newSpyMetrics() *spyMetrics {
    return &spyMetrics{latencies: make(map[string][]time.Duration)}
}

func (s *spyMetrics) IncHit()    { s.mu.Lock(); s.hits++; s.mu.Unlock() }
func (s *spyMetrics) IncMiss()   { s.mu.Lock(); s.misses++; s.mu.Unlock() }
func (s *spyMetrics) IncError()  { s.mu.Lock(); s.errors++; s.mu.Unlock() }
func (s *spyMetrics) ObserveLatency(op string, d time.Duration) {
    s.mu.Lock()
    s.latencies[op] = append(s.latencies[op], d)
    s.mu.Unlock()
}

// spyLogger — registra mensajes
type spyLogger struct {
    mu       sync.Mutex
    messages []logEntry
}

type logEntry struct {
    level string
    msg   string
    args  []any
}

func (s *spyLogger) Info(msg string, args ...any) {
    s.mu.Lock()
    s.messages = append(s.messages, logEntry{"info", msg, args})
    s.mu.Unlock()
}

func (s *spyLogger) Error(msg string, args ...any) {
    s.mu.Lock()
    s.messages = append(s.messages, logEntry{"error", msg, args})
    s.mu.Unlock()
}

// --- Helper ---

type testCacheEnv struct {
    cache   *Cache
    store   *fakeStore
    clock   *fakeClock
    metrics *spyMetrics
    logger  *spyLogger
}

func setupCache() *testCacheEnv {
    store := newFakeStore()
    clock := newFakeClock(time.Date(2024, 6, 15, 10, 0, 0, 0, time.UTC))
    metrics := newSpyMetrics()
    logger := &spyLogger{}

    c := New(store, jsonSerializer{},
        WithDefaultTTL(10*time.Minute),
        WithClock(clock),
        WithMetrics(metrics),
        WithLogger(logger),
    )

    return &testCacheEnv{
        cache:   c,
        store:   store,
        clock:   clock,
        metrics: metrics,
        logger:  logger,
    }
}

// --- Tests ---

func TestCache_SetAndGet(t *testing.T) {
    env := setupCache()
    ctx := context.Background()

    type User struct {
        Name  string `json:"name"`
        Email string `json:"email"`
    }

    // Set
    err := env.cache.Set(ctx, "user:1", User{Name: "Alice", Email: "alice@test.com"}, 0)
    if err != nil {
        t.Fatalf("Set failed: %v", err)
    }

    // Get — debe venir de L1 (memoria)
    var got User
    err = env.cache.Get(ctx, "user:1", &got)
    if err != nil {
        t.Fatalf("Get failed: %v", err)
    }
    if got.Name != "Alice" {
        t.Errorf("expected Alice, got %s", got.Name)
    }
    if got.Email != "alice@test.com" {
        t.Errorf("expected alice@test.com, got %s", got.Email)
    }

    // Verificar metricas
    if env.metrics.hits != 1 {
        t.Errorf("expected 1 hit, got %d", env.metrics.hits)
    }
}

func TestCache_L1Expiry_FallsBackToL2(t *testing.T) {
    env := setupCache()
    ctx := context.Background()

    // Set con TTL de 5 minutos
    err := env.cache.Set(ctx, "key1", "hello", 5*time.Minute)
    if err != nil {
        t.Fatal(err)
    }

    // Get inmediato — L1 hit
    var val string
    err = env.cache.Get(ctx, "key1", &val)
    if err != nil {
        t.Fatal(err)
    }
    if val != "hello" {
        t.Errorf("expected hello, got %s", val)
    }

    // Avanzar reloj 6 minutos — L1 expira, L2 aun tiene el valor
    env.clock.Advance(6 * time.Minute)

    // Get — L1 miss (expirado), L2 hit
    var val2 string
    err = env.cache.Get(ctx, "key1", &val2)
    if err != nil {
        t.Fatalf("expected L2 fallback, got error: %v", err)
    }
    if val2 != "hello" {
        t.Errorf("expected hello from L2, got %s", val2)
    }
}

func TestCache_Miss(t *testing.T) {
    env := setupCache()
    ctx := context.Background()

    var val string
    err := env.cache.Get(ctx, "nonexistent", &val)
    if err != ErrNotFound {
        t.Errorf("expected ErrNotFound, got %v", err)
    }

    if env.metrics.misses != 1 {
        t.Errorf("expected 1 miss, got %d", env.metrics.misses)
    }
}

func TestCache_Delete(t *testing.T) {
    env := setupCache()
    ctx := context.Background()

    env.cache.Set(ctx, "key1", "value1", 0)

    err := env.cache.Delete(ctx, "key1")
    if err != nil {
        t.Fatal(err)
    }

    // Debe ser miss
    var val string
    err = env.cache.Get(ctx, "key1", &val)
    if err != ErrNotFound {
        t.Errorf("expected ErrNotFound after delete, got %v", err)
    }
}

func TestCache_Invalidate_OnlyL1(t *testing.T) {
    env := setupCache()
    ctx := context.Background()

    env.cache.Set(ctx, "key1", "value1", 0)

    // Invalidar solo L1
    env.cache.Invalidate("key1")

    // Get — L1 miss, L2 hit (repromovido a L1)
    var val string
    err := env.cache.Get(ctx, "key1", &val)
    if err != nil {
        t.Fatalf("expected L2 hit after L1 invalidate, got: %v", err)
    }
    if val != "value1" {
        t.Errorf("expected value1, got %s", val)
    }
}

func TestCache_L2Failure_Set(t *testing.T) {
    failing := &failingStore{err: fmt.Errorf("redis connection refused")}
    metrics := newSpyMetrics()

    c := New(failing, jsonSerializer{}, WithMetrics(metrics))
    ctx := context.Background()

    err := c.Set(ctx, "key", "value", 0)
    if err == nil {
        t.Fatal("expected error when L2 fails")
    }

    if metrics.errors != 1 {
        t.Errorf("expected 1 error metric, got %d", metrics.errors)
    }
}

func TestCache_Logging(t *testing.T) {
    env := setupCache()
    ctx := context.Background()

    env.cache.Set(ctx, "key1", "val", 0)

    var val string
    env.cache.Get(ctx, "key1", &val) // L1 hit

    // Verificar que se loguearon los mensajes
    env.logger.mu.Lock()
    defer env.logger.mu.Unlock()

    found := false
    for _, entry := range env.logger.messages {
        if entry.msg == "L1 hit" {
            found = true
            break
        }
    }
    if !found {
        t.Error("expected 'L1 hit' log message")
    }
}

func TestCache_SetDifferentTypes(t *testing.T) {
    env := setupCache()
    ctx := context.Background()

    tests := []struct {
        name  string
        key   string
        value any
        dest  any
        check func(t *testing.T, dest any)
    }{
        {
            name:  "string",
            key:   "str",
            value: "hello world",
            dest:  new(string),
            check: func(t *testing.T, dest any) {
                if *dest.(*string) != "hello world" {
                    t.Errorf("got %s", *dest.(*string))
                }
            },
        },
        {
            name:  "integer",
            key:   "num",
            value: 42,
            dest:  new(float64), // JSON unmarshals numbers as float64
            check: func(t *testing.T, dest any) {
                if *dest.(*float64) != 42 {
                    t.Errorf("got %f", *dest.(*float64))
                }
            },
        },
        {
            name:  "slice",
            key:   "list",
            value: []string{"a", "b", "c"},
            dest:  &[]string{},
            check: func(t *testing.T, dest any) {
                got := *dest.(*[]string)
                if len(got) != 3 || got[0] != "a" {
                    t.Errorf("got %v", got)
                }
            },
        },
        {
            name:  "map",
            key:   "config",
            value: map[string]int{"port": 8080, "workers": 4},
            dest:  &map[string]float64{},
            check: func(t *testing.T, dest any) {
                got := *dest.(*map[string]float64)
                if got["port"] != 8080 {
                    t.Errorf("got %v", got)
                }
            },
        },
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            err := env.cache.Set(ctx, tt.key, tt.value, 0)
            if err != nil {
                t.Fatal(err)
            }

            err = env.cache.Get(ctx, tt.key, tt.dest)
            if err != nil {
                t.Fatal(err)
            }

            tt.check(t, tt.dest)
        })
    }
}

// ============================================================================
// cache_integration_test.go — Integration tests con testcontainers (Redis)
// ============================================================================

//go:build integration

package cache_test

import (
    "context"
    "encoding/json"
    "fmt"
    "os"
    "testing"
    "time"

    "github.com/redis/go-redis/v9"
    tcredis "github.com/testcontainers/testcontainers-go/modules/redis"

    "myapp/cache"
)

// redisStore — implementacion real de Store usando Redis
type redisStore struct {
    client *redis.Client
}

func (r *redisStore) Get(ctx context.Context, key string) ([]byte, error) {
    val, err := r.client.Get(ctx, key).Bytes()
    if err == redis.Nil {
        return nil, cache.ErrNotFound
    }
    return val, err
}

func (r *redisStore) Set(ctx context.Context, key string, val []byte, ttl time.Duration) error {
    return r.client.Set(ctx, key, val, ttl).Err()
}

func (r *redisStore) Delete(ctx context.Context, key string) error {
    return r.client.Del(ctx, key).Err()
}

func (r *redisStore) Exists(ctx context.Context, key string) (bool, error) {
    n, err := r.client.Exists(ctx, key).Result()
    return n > 0, err
}

// --- Setup ---

var testRedisClient *redis.Client

func TestMain(m *testing.M) {
    ctx := context.Background()

    redisContainer, err := tcredis.Run(ctx, "redis:7-alpine")
    if err != nil {
        fmt.Fprintf(os.Stderr, "redis container: %v\n", err)
        os.Exit(1)
    }

    connStr, err := redisContainer.ConnectionString(ctx)
    if err != nil {
        fmt.Fprintf(os.Stderr, "redis connection: %v\n", err)
        os.Exit(1)
    }

    opts, err := redis.ParseURL(connStr)
    if err != nil {
        fmt.Fprintf(os.Stderr, "redis parse: %v\n", err)
        os.Exit(1)
    }

    testRedisClient = redis.NewClient(opts)

    code := m.Run()

    testRedisClient.Close()
    redisContainer.Terminate(ctx)

    os.Exit(code)
}

type jsonSer struct{}
func (jsonSer) Marshal(v any) ([]byte, error)      { return json.Marshal(v) }
func (jsonSer) Unmarshal(data []byte, v any) error  { return json.Unmarshal(data, v) }

func cleanRedis(t *testing.T) {
    t.Helper()
    t.Cleanup(func() {
        testRedisClient.FlushDB(context.Background())
    })
}

// --- Integration Tests ---

func TestRedisCache_SetAndGet(t *testing.T) {
    cleanRedis(t)

    store := &redisStore{client: testRedisClient}
    c := cache.New(store, jsonSer{})
    ctx := context.Background()

    type Product struct {
        ID    string  `json:"id"`
        Name  string  `json:"name"`
        Price float64 `json:"price"`
    }

    product := Product{ID: "p-1", Name: "Widget", Price: 9.99}
    err := c.Set(ctx, "product:p-1", product, 5*time.Minute)
    if err != nil {
        t.Fatal(err)
    }

    var got Product
    err = c.Get(ctx, "product:p-1", &got)
    if err != nil {
        t.Fatal(err)
    }
    if got.Name != "Widget" {
        t.Errorf("expected Widget, got %s", got.Name)
    }
    if got.Price != 9.99 {
        t.Errorf("expected 9.99, got %f", got.Price)
    }
}

func TestRedisCache_L1InvalidateRecoverFromL2(t *testing.T) {
    cleanRedis(t)

    store := &redisStore{client: testRedisClient}
    c := cache.New(store, jsonSer{})
    ctx := context.Background()

    c.Set(ctx, "key1", "hello", 10*time.Minute)

    // Invalidar L1
    c.Invalidate("key1")

    // Debe recuperar de Redis (L2)
    var val string
    err := c.Get(ctx, "key1", &val)
    if err != nil {
        t.Fatalf("expected L2 recovery, got: %v", err)
    }
    if val != "hello" {
        t.Errorf("expected hello, got %s", val)
    }
}

func TestRedisCache_Delete(t *testing.T) {
    cleanRedis(t)

    store := &redisStore{client: testRedisClient}
    c := cache.New(store, jsonSer{})
    ctx := context.Background()

    c.Set(ctx, "temp", "data", 10*time.Minute)
    c.Delete(ctx, "temp")

    var val string
    err := c.Get(ctx, "temp", &val)
    if err != cache.ErrNotFound {
        t.Errorf("expected ErrNotFound, got %v (val=%s)", err, val)
    }

    // Verificar que tambien se elimino de Redis
    exists, _ := testRedisClient.Exists(ctx, "temp").Result()
    if exists > 0 {
        t.Error("key should not exist in Redis after delete")
    }
}

func TestRedisCache_TTLExpiration(t *testing.T) {
    cleanRedis(t)

    store := &redisStore{client: testRedisClient}
    c := cache.New(store, jsonSer{})
    ctx := context.Background()

    // Set con TTL de 1 segundo
    c.Set(ctx, "ephemeral", "gone soon", 1*time.Second)

    // Inmediatamente — debe existir
    var val string
    err := c.Get(ctx, "ephemeral", &val)
    if err != nil {
        t.Fatalf("should exist immediately: %v", err)
    }

    // Esperar a que expire en Redis
    time.Sleep(2 * time.Second)

    // Invalidar L1 para forzar consulta a Redis
    c.Invalidate("ephemeral")

    // Ahora debe ser miss — Redis TTL expiro
    err = c.Get(ctx, "ephemeral", &val)
    if err != cache.ErrNotFound {
        t.Errorf("expected ErrNotFound after TTL, got %v", err)
    }
}

func TestRedisCache_ConcurrentAccess(t *testing.T) {
    cleanRedis(t)

    store := &redisStore{client: testRedisClient}
    c := cache.New(store, jsonSer{})
    ctx := context.Background()

    // Escrituras concurrentes
    var wg sync.WaitGroup
    for i := range 50 {
        wg.Add(1)
        go func() {
            defer wg.Done()
            key := fmt.Sprintf("concurrent:%d", i)
            c.Set(ctx, key, i, 5*time.Minute)
        }()
    }
    wg.Wait()

    // Lecturas concurrentes
    for i := range 50 {
        wg.Add(1)
        go func() {
            defer wg.Done()
            key := fmt.Sprintf("concurrent:%d", i)
            var val float64
            err := c.Get(ctx, key, &val)
            if err != nil {
                t.Errorf("get %s failed: %v", key, err)
            }
        }()
    }
    wg.Wait()
}

func TestRedisCache_LargeValues(t *testing.T) {
    cleanRedis(t)

    store := &redisStore{client: testRedisClient}
    c := cache.New(store, jsonSer{})
    ctx := context.Background()

    // 10K items
    type Payload struct {
        Items []string `json:"items"`
    }

    items := make([]string, 10_000)
    for i := range items {
        items[i] = fmt.Sprintf("item-%d-with-some-extra-data", i)
    }

    payload := Payload{Items: items}

    err := c.Set(ctx, "large", payload, 5*time.Minute)
    if err != nil {
        t.Fatal(err)
    }

    var got Payload
    err = c.Get(ctx, "large", &got)
    if err != nil {
        t.Fatal(err)
    }
    if len(got.Items) != 10_000 {
        t.Errorf("expected 10000 items, got %d", len(got.Items))
    }
}
```

---

## 13. Ejercicios

### Ejercicio 1: Mock completo de HTTP API
Implementa un `WeatherService` que consulta una API HTTP externa para obtener el clima de una ciudad. Escribe:
1. La interfaz `WeatherAPI` con metodo `GetForecast(city string) (Forecast, error)`
2. Un spy que registre las ciudades consultadas
3. Un stub que retorne datos fijos por ciudad (map)
4. Tests usando `httptest.NewServer` que simulen la API real (respuestas exitosas, errores 404, timeouts)
5. Un test table-driven que cubra al menos 6 escenarios

### Ejercicio 2: Fake de sistema de archivos
Crea un `ConfigManager` que:
1. Lee configuracion de un archivo YAML
2. Escribe configuracion modificada
3. Hace backup antes de escribir
4. Implementa la interfaz `FileSystem` con `ReadFile`, `WriteFile`, `Rename`

Escribe tests con un fake in-memory filesystem que cubra:
- Lectura exitosa, archivo no encontrado, YAML invalido
- Escritura con backup (verificar que el archivo original se renombro)
- Multiples writes sucesivos (verificar que los backups se acumulan)

### Ejercicio 3: Testcontainers con PostgreSQL
Implementa un `TodoRepository` con PostgreSQL:
1. Metodos: `Create(todo Todo) error`, `Get(id string) (Todo, error)`, `List(userID string) ([]Todo, error)`, `MarkDone(id string) error`, `Delete(id string) error`
2. Schema: `todos(id UUID, user_id TEXT, title TEXT, done BOOLEAN, created_at TIMESTAMP)`
3. Escribe integration tests con testcontainers que cubran: CRUD completo, filtrado por usuario, marcar como hecho, intentar marcar/eliminar inexistente, ordenamiento por fecha
4. Usa `TestMain` para compartir el container y `t.Cleanup` para limpiar entre tests

### Ejercicio 4: Pipeline con DI completa
Diseña un pipeline de procesamiento de datos con DI completa:
1. Interfaces: `DataSource` (Read), `Transformer` (Transform), `DataSink` (Write), `Logger`
2. `Pipeline` struct que acepta source, N transformers, sink, logger via constructor
3. Unit tests con: fake source (datos en memoria), spy transformers (registran entradas/salidas), spy sink (registra lo escrito), spy logger
4. Verifica: orden de transformaciones, manejo de errores en cada etapa, que el pipeline se detiene al primer error, metricas (contadores de items procesados)

---

> **Fin del Capitulo 11 — Genericos y Testing**. Este capitulo cubrio desde type parameters y constraints hasta testing completo con mocks, DI, y testcontainers. Con esto tienes las herramientas para escribir codigo generico type-safe y tests robustos tanto unitarios como de integracion.
