# Encoding — encoding/json (Marshal, Unmarshal, tags, Decoder/Encoder streaming), encoding/csv, encoding/xml

## 1. Introduccion

Los paquetes `encoding/*` de la stdlib manejan la serializacion y deserializacion de datos en distintos formatos. En Go, "encoding" significa convertir estructuras de Go a un formato de intercambio (bytes, texto), y "decoding" significa el proceso inverso. Los tres formatos mas usados son JSON (APIs, configuracion), CSV (datos tabulares, importacion/exportacion), y XML (integraciones legacy, feeds, SOAP).

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                    ENCODING EN GO                                               │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  PAQUETES                                                                       │
│  ├─ encoding/json  → JSON (el mas usado — APIs REST, config, logging)          │
│  ├─ encoding/csv   → CSV (datos tabulares, hojas de calculo)                   │
│  ├─ encoding/xml   → XML (legacy APIs, RSS/Atom, SOAP, HTML)                   │
│  ├─ encoding/gob   → Gob (formato binario nativo de Go — RPC, cache)          │
│  ├─ encoding/binary→ Binario (read/write structs en formatos raw)              │
│  ├─ encoding/base64→ Base64 (encoding de datos binarios en texto)              │
│  ├─ encoding/hex   → Hexadecimal                                               │
│  └─ encoding/pem   → PEM (certificados, claves)                               │
│                                                                                  │
│  DOS MODELOS DE API                                                             │
│                                                                                  │
│  1. BATCH (todo en memoria)                                                     │
│     json.Marshal(v)       → struct → []byte (JSON)                             │
│     json.Unmarshal(data,&v) → []byte (JSON) → struct                           │
│     Bueno para: datos pequenos, respuestas de API                              │
│                                                                                  │
│  2. STREAMING (io.Reader/io.Writer)                                             │
│     json.NewEncoder(w).Encode(v)  → struct → io.Writer                         │
│     json.NewDecoder(r).Decode(&v) → io.Reader → struct                         │
│     Bueno para: archivos grandes, HTTP bodies, NDJSON                          │
│                                                                                  │
│  STRUCT TAGS                                                                    │
│  type User struct {                                                              │
│      ID   int    `json:"id"   xml:"id"   csv:"id"`                             │
│      Name string `json:"name" xml:"name" csv:"name"`                           │
│  }                                                                               │
│  Los tags controlan como se mapean los campos al formato externo.              │
│                                                                                  │
│  COMPARACION                                                                    │
│  ├─ C:    no tiene (jansson para JSON, libxml2 para XML — todo externo)       │
│  ├─ Rust: no tiene en std (serde + serde_json/csv/quick-xml — crates)         │
│  └─ Go:   en la stdlib (encoding/json, encoding/csv, encoding/xml)            │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. encoding/json — lo esencial

### 2.1 Marshal: Go → JSON

```go
import "encoding/json"

type User struct {
    ID        int       `json:"id"`
    Name      string    `json:"name"`
    Email     string    `json:"email"`
    Age       int       `json:"age,omitempty"`
    IsAdmin   bool      `json:"is_admin"`
    Password  string    `json:"-"`
    CreatedAt time.Time `json:"created_at"`
}

func main() {
    u := User{
        ID:        1,
        Name:      "Alice",
        Email:     "alice@example.com",
        Age:       0,         // Se omite (omitempty + zero-value)
        IsAdmin:   true,
        Password:  "secret",  // Se ignora (json:"-")
        CreatedAt: time.Now(),
    }
    
    // Marshal: struct → []byte (JSON compacto, una sola linea)
    data, err := json.Marshal(u)
    if err != nil {
        log.Fatal(err)
    }
    fmt.Println(string(data))
    // {"id":1,"name":"Alice","email":"alice@example.com","is_admin":true,"created_at":"2024-01-15T10:30:00Z"}
    
    // MarshalIndent: struct → []byte (JSON formateado)
    pretty, err := json.MarshalIndent(u, "", "  ")
    if err != nil {
        log.Fatal(err)
    }
    fmt.Println(string(pretty))
    // {
    //   "id": 1,
    //   "name": "Alice",
    //   ...
    // }
    
    // MarshalIndent con prefijo (raro pero valido):
    // json.MarshalIndent(v, ">> ", "  ")
    // Cada linea empieza con ">> " como prefijo
}
```

### 2.2 Unmarshal: JSON → Go

```go
jsonData := []byte(`{
    "id": 42,
    "name": "Bob",
    "email": "bob@example.com",
    "is_admin": false,
    "unknown_field": "ignored",
    "created_at": "2024-01-15T10:30:00Z"
}`)

var u User
err := json.Unmarshal(jsonData, &u) // Pasar PUNTERO
if err != nil {
    log.Fatal(err)
}

fmt.Printf("%+v\n", u)
// {ID:42 Name:Bob Email:bob@example.com Age:0 IsAdmin:false Password: CreatedAt:2024-01-15 10:30:00 +0000 UTC}

// Observaciones:
// - "unknown_field" se ignora silenciosamente (por defecto)
// - Age=0 porque no estaba en el JSON
// - Password="" porque json:"-" lo excluye del decoding
// - time.Time se decodifica desde formato RFC3339 automaticamente
```

### 2.3 Unmarshal a tipos genericos

```go
// Cuando no conoces la estructura: map[string]any
var generic map[string]any
json.Unmarshal(jsonData, &generic)

fmt.Println(generic["name"])     // "Bob" (string)
fmt.Println(generic["id"])       // 42 (¡tipo float64, no int!)
fmt.Println(generic["is_admin"]) // false (bool)

// TRAMPA: todos los numeros JSON se decodifican como float64
// Esto puede perder precision para int64 grandes
id := generic["id"].(float64) // 42.0, no 42

// Unmarshal a slice
var users []User
jsonArray := []byte(`[{"id":1,"name":"A"},{"id":2,"name":"B"}]`)
json.Unmarshal(jsonArray, &users)

// Unmarshal a tipo primitivo
var count int
json.Unmarshal([]byte("42"), &count) // count = 42

var msg string
json.Unmarshal([]byte(`"hello"`), &msg) // msg = "hello"
```

---

## 3. Struct tags de JSON — referencia completa

### 3.1 Todas las opciones

```go
type Example struct {
    // Renombrar campo
    UserID int `json:"user_id"`
    // JSON: "user_id": 42
    
    // Omitir si es zero-value
    Age int `json:"age,omitempty"`
    // JSON: se omite si Age == 0
    
    // Nunca incluir (ni marshal ni unmarshal)
    Secret string `json:"-"`
    // Nunca aparece en JSON
    
    // Nombre literal "-" (raro)
    Dash string `json:"-,"`
    // JSON: "-": "value"
    
    // Mantener nombre Go, con omitempty
    Active bool `json:",omitempty"`
    // JSON: "Active": true (o se omite si false)
    
    // Sin tag: usa el nombre del campo Go tal cual
    RawField string
    // JSON: "RawField": "value"
    
    // Codificar numero como string JSON
    BigID int64 `json:"big_id,string"`
    // JSON: "big_id": "9007199254740993"
    // Util para IDs > 2^53 que JavaScript no puede representar como Number
}
```

### 3.2 Que es un zero-value para omitempty

```
ZERO-VALUES QUE omitempty OMITE:

Tipo          Zero-value     Se omite?
─────────────────────────────────────────
bool          false          Si
int/float     0 / 0.0        Si
string        ""             Si
pointer       nil            Si
slice         nil            Si (pero []T{} con len=0 NO se omite)
map           nil            Si (pero map[K]V{} NO se omite)
interface     nil            Si
struct        T{}            NO se omite (esto sorprende a muchos)
time.Time     time.Time{}    NO se omite (es un struct)
array         [N]T{}         NO se omite

SORPRESAS COMUNES:
  
  // Struct vacio NO se omite con omitempty
  type Config struct {
      DB DatabaseConfig `json:"db,omitempty"` // NO se omite aunque DB sea zero
  }
  // Solucion: usar puntero
  type Config struct {
      DB *DatabaseConfig `json:"db,omitempty"` // Se omite si DB es nil
  }
  
  // Slice vacio vs nil
  var s1 []int        // nil — se omite con omitempty
  s2 := []int{}       // empty, no nil — NO se omite (aparece como [])
  s3 := make([]int,0) // empty, no nil — NO se omite
  
  // time.Time zero NO se omite (aparece como "0001-01-01T00:00:00Z")
  // Solucion: usar puntero
  type Event struct {
      EndedAt *time.Time `json:"ended_at,omitempty"` // nil → se omite
  }
```

### 3.3 Campos embebidos (promoted)

```go
type Address struct {
    Street string `json:"street"`
    City   string `json:"city"`
}

type Person struct {
    Name    string `json:"name"`
    Address        // Embebido — campos se "promueven" al nivel de Person
}

p := Person{
    Name:    "Alice",
    Address: Address{Street: "123 Main St", City: "Springfield"},
}

data, _ := json.Marshal(p)
fmt.Println(string(data))
// {"name":"Alice","street":"123 Main St","city":"Springfield"}
// Los campos de Address aparecen al mismo nivel que Name

// Si quieres Address como sub-objeto:
type PersonNested struct {
    Name    string  `json:"name"`
    Address Address `json:"address"` // Campo con nombre, no embebido
}
// {"name":"Alice","address":{"street":"123 Main St","city":"Springfield"}}
```

### 3.4 Campos con conflicto de nombre

```go
// Si un campo embebido y un campo directo tienen el mismo nombre JSON:
type Base struct {
    ID int `json:"id"`
}

type Extended struct {
    Base
    ID string `json:"id"` // Mismo nombre — Extended.ID gana
}

// Extended.ID tiene prioridad sobre Base.ID
// Regla: el campo mas "cercano" (menos niveles de embedding) gana
// Si estan al mismo nivel → ambos se ignoran (conflicto sin resolver)
```

---

## 4. Tipos especiales en JSON

### 4.1 json.RawMessage — decodificacion diferida

```go
// RawMessage mantiene JSON crudo sin decodificar
// Util para JSON con estructura variable

type Event struct {
    Type    string          `json:"type"`
    Payload json.RawMessage `json:"payload"` // Se guarda como bytes crudos
}

func processEvent(data []byte) error {
    var event Event
    if err := json.Unmarshal(data, &event); err != nil {
        return err
    }
    
    // Decodificar payload segun el tipo
    switch event.Type {
    case "user.created":
        var user User
        if err := json.Unmarshal(event.Payload, &user); err != nil {
            return err
        }
        return handleUserCreated(user)
        
    case "order.placed":
        var order Order
        if err := json.Unmarshal(event.Payload, &order); err != nil {
            return err
        }
        return handleOrderPlaced(order)
        
    default:
        // Guardar el payload raw para logging/debugging
        log.Printf("unknown event %s: %s", event.Type, event.Payload)
        return nil
    }
}

// Otro uso: pasar JSON sin modificar
type ProxyResponse struct {
    Status int             `json:"status"`
    Data   json.RawMessage `json:"data"` // Pasar sin decodificar
}
```

### 4.2 json.Number — precision numerica

```go
// Problema: JSON numbers se decodifican como float64 en map[string]any
// float64 pierde precision para int64 > 2^53

jsonData := []byte(`{"id": 9007199254740993}`) // > Number.MAX_SAFE_INTEGER

// MAL: pierde precision
var m map[string]any
json.Unmarshal(jsonData, &m)
fmt.Printf("%.0f\n", m["id"].(float64)) // 9007199254740992 — INCORRECTO!

// BIEN: usar Decoder con UseNumber
decoder := json.NewDecoder(bytes.NewReader(jsonData))
decoder.UseNumber()
var m2 map[string]any
decoder.Decode(&m2)

num := m2["id"].(json.Number)
id, _ := num.Int64()   // 9007199254740993 — CORRECTO
fmt.Println(id)

// json.Number es un string internamente:
fmt.Println(num.String()) // "9007199254740993"
f, _ := num.Float64()     // Tambien puedes obtener float64
i, _ := num.Int64()       // O int64
```

### 4.3 Punteros: null en JSON

```go
type User struct {
    Name     string  `json:"name"`
    Nickname *string `json:"nickname"` // Puntero — puede ser null
    Age      *int    `json:"age"`      // Puntero — puede ser null
}

// JSON con null:
jsonData := []byte(`{"name":"Alice","nickname":null,"age":null}`)
var u User
json.Unmarshal(jsonData, &u)
// u.Nickname == nil (no "", sino nil — distingue "no hay valor" de "string vacio")
// u.Age == nil

// JSON sin el campo:
jsonData = []byte(`{"name":"Alice"}`)
json.Unmarshal(jsonData, &u)
// u.Nickname == nil (mismo resultado que null)
// u.Age == nil

// JSON con valor:
jsonData = []byte(`{"name":"Alice","nickname":"Ali","age":30}`)
json.Unmarshal(jsonData, &u)
// u.Nickname == &"Ali" (puntero a string)
// u.Age == &30 (puntero a int)

// Marshal: nil → null, &valor → valor
nick := "Ali"
u = User{Name: "Alice", Nickname: &nick, Age: nil}
data, _ := json.Marshal(u)
// {"name":"Alice","nickname":"Ali","age":null}

// Con omitempty: nil → se omite
type UserOmit struct {
    Name     string  `json:"name"`
    Nickname *string `json:"nickname,omitempty"` // nil → se omite
}
u2 := UserOmit{Name: "Alice", Nickname: nil}
data, _ = json.Marshal(u2)
// {"name":"Alice"} — nickname omitido porque es nil
```

### 4.4 Interfaces y tipos polimorficos

```go
// any (interface{}) se decodifica segun el tipo JSON:
// JSON object  → map[string]any
// JSON array   → []any
// JSON string  → string
// JSON number  → float64 (o json.Number con UseNumber)
// JSON bool    → bool
// JSON null    → nil

type Response struct {
    Data any `json:"data"` // Puede ser cualquier tipo JSON
}

// Marshaling: el tipo Go determina el JSON
r1 := Response{Data: "hello"}      // {"data":"hello"}
r2 := Response{Data: 42}           // {"data":42}
r3 := Response{Data: []int{1,2,3}} // {"data":[1,2,3]}
r4 := Response{Data: nil}          // {"data":null}
r5 := Response{Data: map[string]int{"a":1}} // {"data":{"a":1}}
```

---

## 5. Custom marshaling/unmarshaling

### 5.1 json.Marshaler y json.Unmarshaler

```go
// Implementar json.Marshaler para controlar como se serializa un tipo
type json.Marshaler interface {
    MarshalJSON() ([]byte, error)
}

// Implementar json.Unmarshaler para controlar como se deserializa
type json.Unmarshaler interface {
    UnmarshalJSON([]byte) error
}
```

### 5.2 Ejemplo: Duration human-readable

```go
// time.Duration se serializa como nanosegundos por defecto (no util)
// Queremos: "5s", "1m30s", "2h"

type Duration struct {
    time.Duration
}

func (d Duration) MarshalJSON() ([]byte, error) {
    return json.Marshal(d.Duration.String()) // "5s", "1m30s"
}

func (d *Duration) UnmarshalJSON(data []byte) error {
    var s string
    if err := json.Unmarshal(data, &s); err != nil {
        // Intentar como numero (nanosegundos) para compatibilidad
        var ns int64
        if err2 := json.Unmarshal(data, &ns); err2 != nil {
            return err
        }
        d.Duration = time.Duration(ns)
        return nil
    }
    
    dur, err := time.ParseDuration(s)
    if err != nil {
        return fmt.Errorf("invalid duration %q: %w", s, err)
    }
    d.Duration = dur
    return nil
}

// Uso:
type Config struct {
    Timeout  Duration `json:"timeout"`
    Interval Duration `json:"interval"`
}

// JSON: {"timeout": "5s", "interval": "1m30s"}
// En vez de: {"timeout": 5000000000, "interval": 90000000000}
```

### 5.3 Ejemplo: Enum como string

```go
type Status int

const (
    StatusPending Status = iota
    StatusActive
    StatusClosed
)

var statusNames = map[Status]string{
    StatusPending: "pending",
    StatusActive:  "active",
    StatusClosed:  "closed",
}

var statusValues = map[string]Status{
    "pending": StatusPending,
    "active":  StatusActive,
    "closed":  StatusClosed,
}

func (s Status) MarshalJSON() ([]byte, error) {
    name, ok := statusNames[s]
    if !ok {
        return nil, fmt.Errorf("unknown status: %d", s)
    }
    return json.Marshal(name)
}

func (s *Status) UnmarshalJSON(data []byte) error {
    var name string
    if err := json.Unmarshal(data, &name); err != nil {
        return err
    }
    val, ok := statusValues[name]
    if !ok {
        return fmt.Errorf("unknown status: %q", name)
    }
    *s = val
    return nil
}

// JSON: "status": "active"
// En vez de: "status": 1
```

### 5.4 Ejemplo: Timestamp unix

```go
// Representar time.Time como Unix timestamp (entero) en vez de RFC3339

type UnixTime struct {
    time.Time
}

func (t UnixTime) MarshalJSON() ([]byte, error) {
    return json.Marshal(t.Unix())
}

func (t *UnixTime) UnmarshalJSON(data []byte) error {
    var unix int64
    if err := json.Unmarshal(data, &unix); err != nil {
        // Fallback: intentar RFC3339
        var s string
        if err2 := json.Unmarshal(data, &s); err2 != nil {
            return err
        }
        parsed, err := time.Parse(time.RFC3339, s)
        if err != nil {
            return err
        }
        t.Time = parsed
        return nil
    }
    t.Time = time.Unix(unix, 0)
    return nil
}

type Event struct {
    Name      string   `json:"name"`
    Timestamp UnixTime `json:"timestamp"`
}

// JSON: {"name":"click","timestamp":1705312200}
// En vez de: {"name":"click","timestamp":"2024-01-15T10:30:00Z"}
```

### 5.5 Ejemplo: Flatten (aplanar struct anidado)

```go
// Aplanar campos de sub-structs durante marshaling
type DatabaseConfig struct {
    Host     string `json:"host"`
    Port     int    `json:"port"`
    Name     string `json:"name"`
    User     string `json:"user"`
    Password string `json:"password"`
}

type AppConfig struct {
    AppName string         `json:"app_name"`
    DB      DatabaseConfig `json:"db"` // Esto genera sub-objeto: "db": {...}
}

// Si quieres todo plano: "db_host", "db_port", etc.
// Usa MarshalJSON custom:
func (c AppConfig) MarshalJSON() ([]byte, error) {
    flat := map[string]any{
        "app_name":    c.AppName,
        "db_host":     c.DB.Host,
        "db_port":     c.DB.Port,
        "db_name":     c.DB.Name,
        "db_user":     c.DB.User,
        "db_password": c.DB.Password,
    }
    return json.Marshal(flat)
}
// {"app_name":"myapp","db_host":"localhost","db_port":5432,...}
```

---

## 6. Streaming JSON: Encoder y Decoder

### 6.1 json.NewDecoder — leer de io.Reader

```go
// Decoder lee JSON de un io.Reader (streaming, eficiente)
// NO carga todo en memoria — ideal para archivos grandes o HTTP

// --- Desde archivo ---
f, _ := os.Open("users.json")
defer f.Close()

var users []User
if err := json.NewDecoder(f).Decode(&users); err != nil {
    log.Fatal(err)
}

// --- Desde HTTP response ---
resp, _ := http.Get("https://api.example.com/users")
defer resp.Body.Close()

var data ApiResponse
if err := json.NewDecoder(resp.Body).Decode(&data); err != nil {
    log.Fatal(err)
}

// --- Con opciones ---
decoder := json.NewDecoder(resp.Body)

// Rechazar campos que no estan en el struct
decoder.DisallowUnknownFields()

// Usar json.Number en vez de float64
decoder.UseNumber()

var result map[string]any
if err := decoder.Decode(&result); err != nil {
    log.Fatal(err)
}
```

### 6.2 json.NewEncoder — escribir a io.Writer

```go
// Encoder escribe JSON a un io.Writer

// --- A archivo ---
f, _ := os.Create("output.json")
defer f.Close()

encoder := json.NewEncoder(f)
encoder.SetIndent("", "  ") // Pretty print
encoder.Encode(users)       // Escribe JSON + newline

// --- A HTTP response ---
func handleUsers(w http.ResponseWriter, r *http.Request) {
    users := getUsers()
    w.Header().Set("Content-Type", "application/json")
    w.WriteHeader(http.StatusOK)
    json.NewEncoder(w).Encode(users)
}

// --- A stdout (debug) ---
json.NewEncoder(os.Stdout).Encode(data)

// --- A buffer ---
var buf bytes.Buffer
json.NewEncoder(&buf).Encode(data)
```

### 6.3 NDJSON / JSON Lines — stream de objetos

```
NDJSON (Newline Delimited JSON) / JSON Lines:
Cada linea es un objeto JSON independiente.

{"id":1,"name":"Alice"}
{"id":2,"name":"Bob"}
{"id":3,"name":"Charlie"}

Ventajas sobre un JSON array:
  - Puedes procesar linea por linea (streaming)
  - Puedes append sin reescribir todo el archivo
  - Cada linea es parseable de forma independiente
  - Facil de generar con Encoder (agrega newline despues de cada Encode)
```

```go
// ESCRIBIR NDJSON
func writeNDJSON(w io.Writer, items []Item) error {
    encoder := json.NewEncoder(w)
    // SetEscapeHTML(false) evita escapar <, >, & innecesariamente
    encoder.SetEscapeHTML(false)
    
    for _, item := range items {
        if err := encoder.Encode(item); err != nil {
            // Encode agrega '\n' automaticamente
            return err
        }
    }
    return nil
}

// LEER NDJSON
func readNDJSON(r io.Reader) ([]Item, error) {
    var items []Item
    decoder := json.NewDecoder(r)
    
    for decoder.More() { // More() retorna true si hay mas tokens
        var item Item
        if err := decoder.Decode(&item); err != nil {
            return nil, err
        }
        items = append(items, item)
    }
    return items, nil
}

// STREAMING: procesar sin cargar todo en memoria
func processNDJSON(r io.Reader, fn func(Item) error) error {
    decoder := json.NewDecoder(r)
    for decoder.More() {
        var item Item
        if err := decoder.Decode(&item); err != nil {
            return err
        }
        if err := fn(item); err != nil {
            return err
        }
    }
    return nil
}

// Uso con archivo grande:
f, _ := os.Open("events.ndjson")
defer f.Close()

err := processNDJSON(f, func(item Item) error {
    fmt.Printf("Processing: %s\n", item.Name)
    return nil
})
```

### 6.4 Decoder con tokens: parsear JSON manualmente

```go
// Para JSON muy grande con estructura conocida,
// puedes usar Decoder.Token() para parsear sin cargar todo

func countItems(r io.Reader) (int, error) {
    decoder := json.NewDecoder(r)
    
    // Esperar el inicio del array '['
    t, err := decoder.Token()
    if err != nil {
        return 0, err
    }
    if delim, ok := t.(json.Delim); !ok || delim != '[' {
        return 0, fmt.Errorf("expected '[', got %v", t)
    }
    
    // Contar elementos sin decodificarlos completamente
    count := 0
    for decoder.More() {
        var raw json.RawMessage
        if err := decoder.Decode(&raw); err != nil {
            return 0, err
        }
        count++
    }
    
    return count, nil
}

// Parsear un objeto grande campo por campo:
func parseSelective(r io.Reader) (string, error) {
    decoder := json.NewDecoder(r)
    
    // Consumir '{'
    decoder.Token()
    
    for decoder.More() {
        // Leer nombre del campo
        key, _ := decoder.Token()
        
        switch key.(string) {
        case "name":
            var name string
            decoder.Decode(&name)
            return name, nil
        default:
            // Saltar valor que no nos interesa
            var skip json.RawMessage
            decoder.Decode(&skip)
        }
    }
    
    return "", fmt.Errorf("name not found")
}
```

### 6.5 Marshal vs Encoder: cuando usar cual

```
MARSHAL vs ENCODER — DECISION

json.Marshal(v):
  ✓ Retorna []byte — facil de manipular
  ✓ No necesitas un io.Writer
  ✓ Util cuando necesitas el resultado como string/[]byte
  ✗ Carga todo en memoria
  
  Usar para: datos pequenos, construir JSON en memoria,
             cuando necesitas el []byte para otra operacion

json.NewEncoder(w).Encode(v):
  ✓ Escribe directamente a un io.Writer
  ✓ Streaming — no carga todo en memoria
  ✓ Ideal para HTTP responses
  ✗ Agrega '\n' al final (puede sorprender)
  ✗ No puedes "ver" el JSON antes de enviarlo
  
  Usar para: HTTP responses, archivos, NDJSON,
             cualquier caso donde el destino es un Writer

EQUIVALENCIA:
  json.NewEncoder(w).Encode(v)
  ≈
  data, _ := json.Marshal(v)
  w.Write(data)
  w.Write([]byte("\n")) // Encoder agrega newline!
  
  // NOTA: la '\n' extra puede causar problemas con algunos parsers
  // Si no quieres la '\n':
  data, _ := json.Marshal(v)
  w.Write(data)
```

---

## 7. Validacion y errores de JSON

### 7.1 json.Valid — verificar JSON valido

```go
// Verificar si un []byte es JSON valido (sin decodificar)
data := []byte(`{"name": "Alice", "age": 30}`)
fmt.Println(json.Valid(data)) // true

data = []byte(`{"name": "Alice", "age":}`) // Invalido
fmt.Println(json.Valid(data)) // false

data = []byte(`not json`)
fmt.Println(json.Valid(data)) // false
```

### 7.2 Tipos de errores

```go
// json.SyntaxError — JSON mal formado
var u User
err := json.Unmarshal([]byte(`{"name": "Alice",}`), &u) // Trailing comma
if err != nil {
    var syntaxErr *json.SyntaxError
    if errors.As(err, &syntaxErr) {
        fmt.Printf("Syntax error at byte %d: %v\n", syntaxErr.Offset, err)
    }
}

// json.UnmarshalTypeError — tipo JSON no coincide con tipo Go
err = json.Unmarshal([]byte(`{"id": "not_a_number"}`), &u) // "string" → int
if err != nil {
    var typeErr *json.UnmarshalTypeError
    if errors.As(err, &typeErr) {
        fmt.Printf("Type error: field %s expects %s, got JSON %s\n",
            typeErr.Field, typeErr.Type, typeErr.Value)
    }
}

// json.InvalidUnmarshalError — destino invalido (no puntero)
err = json.Unmarshal([]byte(`{}`), u) // Sin &
// "json: Unmarshal(non-pointer main.User)"

// Campos desconocidos (con DisallowUnknownFields):
decoder := json.NewDecoder(bytes.NewReader(jsonData))
decoder.DisallowUnknownFields()
err = decoder.Decode(&u)
// "json: unknown field \"extra_field\""
```

### 7.3 Patron de validacion post-unmarshal

```go
// JSON Unmarshal no valida reglas de negocio — solo estructura.
// Implementar validacion manualmente:

type CreateUserRequest struct {
    Name     string `json:"name"`
    Email    string `json:"email"`
    Age      int    `json:"age"`
    Password string `json:"password"`
}

func (r CreateUserRequest) Validate() error {
    var errs []string
    
    if strings.TrimSpace(r.Name) == "" {
        errs = append(errs, "name is required")
    }
    if !strings.Contains(r.Email, "@") {
        errs = append(errs, "email is invalid")
    }
    if r.Age < 0 || r.Age > 150 {
        errs = append(errs, "age must be 0-150")
    }
    if len(r.Password) < 8 {
        errs = append(errs, "password must be at least 8 characters")
    }
    
    if len(errs) > 0 {
        return fmt.Errorf("validation failed: %s", strings.Join(errs, "; "))
    }
    return nil
}

// Uso en HTTP handler:
func createUser(w http.ResponseWriter, r *http.Request) {
    var req CreateUserRequest
    
    // 1. Decodificar
    decoder := json.NewDecoder(r.Body)
    decoder.DisallowUnknownFields()
    if err := decoder.Decode(&req); err != nil {
        writeError(w, http.StatusBadRequest, "invalid json: "+err.Error())
        return
    }
    
    // 2. Validar
    if err := req.Validate(); err != nil {
        writeError(w, http.StatusBadRequest, err.Error())
        return
    }
    
    // 3. Procesar...
}
```

---

## 8. Patrones avanzados de JSON

### 8.1 Composicion con embedding anonimo

```go
// Agregar campos a JSON sin modificar el tipo original
type User struct {
    ID   int    `json:"id"`
    Name string `json:"name"`
}

// Agregar campos en marshal:
func userWithLinks(u User, baseURL string) any {
    return struct {
        User           // Promueve id, name
        Self   string  `json:"self"`
        Avatar string  `json:"avatar"`
    }{
        User:   u,
        Self:   fmt.Sprintf("%s/users/%d", baseURL, u.ID),
        Avatar: fmt.Sprintf("%s/users/%d/avatar", baseURL, u.ID),
    }
}

data, _ := json.Marshal(userWithLinks(user, "https://api.example.com"))
// {"id":1,"name":"Alice","self":"https://api.example.com/users/1","avatar":"..."}
```

### 8.2 Ocultar campos selectivamente

```go
// Distintas "vistas" del mismo tipo
type User struct {
    ID       int    `json:"id"`
    Name     string `json:"name"`
    Email    string `json:"email"`
    Password string `json:"password,omitempty"`
    IsAdmin  bool   `json:"is_admin"`
    SSN      string `json:"ssn,omitempty"`
}

// Vista publica (API response):
func (u User) PublicView() any {
    return struct {
        ID   int    `json:"id"`
        Name string `json:"name"`
    }{ID: u.ID, Name: u.Name}
}

// Vista para admin:
func (u User) AdminView() any {
    return struct {
        ID      int    `json:"id"`
        Name    string `json:"name"`
        Email   string `json:"email"`
        IsAdmin bool   `json:"is_admin"`
    }{ID: u.ID, Name: u.Name, Email: u.Email, IsAdmin: u.IsAdmin}
}

// En handler:
func handleGetUser(w http.ResponseWriter, r *http.Request) {
    user := getUser(id)
    
    var view any
    if isAdmin(r) {
        view = user.AdminView()
    } else {
        view = user.PublicView()
    }
    
    json.NewEncoder(w).Encode(view)
}
```

### 8.3 Map con tipo conocido vs map[string]any

```go
// Cuando el JSON tiene claves dinamicas pero valores con estructura conocida:
// {"user_1": {"name":"Alice"}, "user_2": {"name":"Bob"}}

type UserInfo struct {
    Name string `json:"name"`
    Age  int    `json:"age"`
}

var users map[string]UserInfo
json.Unmarshal(data, &users)
// users["user_1"].Name == "Alice"

// Esto es MUCHO mejor que map[string]any
// porque cada valor ya tiene tipo correcto
```

---

## 9. encoding/csv — datos tabulares

### 9.1 Leer CSV

```go
import "encoding/csv"

// --- Leer todo de un golpe ---
f, _ := os.Open("data.csv")
defer f.Close()

reader := csv.NewReader(f)
records, err := reader.ReadAll() // [][]string
if err != nil {
    log.Fatal(err)
}

for i, record := range records {
    fmt.Printf("Linea %d: %v\n", i, record)
}
// records[0] = ["id", "name", "email"]  (header)
// records[1] = ["1", "Alice", "alice@example.com"]
// records[2] = ["2", "Bob", "bob@example.com"]

// --- Leer linea por linea (streaming) ---
f, _ = os.Open("large.csv")
defer f.Close()

reader = csv.NewReader(f)

// Leer header
header, err := reader.Read()
if err != nil {
    log.Fatal(err)
}
fmt.Println("Columnas:", header)

// Leer registros uno por uno
for {
    record, err := reader.Read()
    if err == io.EOF {
        break
    }
    if err != nil {
        log.Fatal(err)
    }
    // record es []string
    fmt.Printf("ID=%s Name=%s Email=%s\n", record[0], record[1], record[2])
}
```

### 9.2 Opciones del reader

```go
reader := csv.NewReader(f)

// Cambiar delimitador (TSV, punto y coma, etc.)
reader.Comma = '\t'    // Tab-separated values (TSV)
reader.Comma = ';'     // Punto y coma (comun en Europa)
reader.Comma = '|'     // Pipe-separated

// Comentarios: ignorar lineas que empiezan con #
reader.Comment = '#'

// Numero de campos esperado (-1 = variable)
reader.FieldsPerRecord = 3    // Error si una linea no tiene 3 campos
reader.FieldsPerRecord = -1   // Permitir distinto numero de campos
reader.FieldsPerRecord = 0    // Determinar automaticamente del primer record

// Permitir lazy quotes (campos con comillas mal formadas)
reader.LazyQuotes = true

// Eliminar leading whitespace de cada campo
reader.TrimLeadingSpace = true

// Reusar el slice del record anterior (optimizacion de memoria)
reader.ReuseRecord = true
// CUIDADO: el slice retornado por Read() se sobreescribe en el siguiente Read()
// Si necesitas guardar el record, haz una copia:
// saved := append([]string{}, record...)
```

### 9.3 Escribir CSV

```go
// --- Escribir todo de un golpe ---
f, _ := os.Create("output.csv")
defer f.Close()

writer := csv.NewWriter(f)

// Escribir header
writer.Write([]string{"id", "name", "email", "score"})

// Escribir registros
records := [][]string{
    {"1", "Alice", "alice@example.com", "95.5"},
    {"2", "Bob", "bob@example.com", "87.3"},
    {"3", "Charlie", "charlie@example.com", "92.1"},
}

writer.WriteAll(records) // Escribe todo y hace Flush

// Verificar errores (WriteAll ya chequea)
if err := writer.Error(); err != nil {
    log.Fatal(err)
}

// --- Escribir linea por linea ---
writer = csv.NewWriter(f)

writer.Write([]string{"id", "name", "score"})
for _, user := range users {
    writer.Write([]string{
        strconv.Itoa(user.ID),
        user.Name,
        strconv.FormatFloat(user.Score, 'f', 2, 64),
    })
}
writer.Flush() // IMPORTANTE: siempre flush al final

if err := writer.Error(); err != nil {
    log.Fatal(err)
}
```

### 9.4 Opciones del writer

```go
writer := csv.NewWriter(f)

// Cambiar delimitador
writer.Comma = '\t'  // TSV
writer.Comma = ';'   // Punto y coma

// Usar \r\n en vez de \n (Windows)
writer.UseCRLF = true
```

### 9.5 CSV a structs y viceversa (manualmente)

```go
// Go NO tiene tags para CSV en la stdlib (como json tags)
// Hay que mapear manualmente

type Employee struct {
    ID         int
    Name       string
    Department string
    Salary     float64
}

// CSV → structs
func readEmployees(r io.Reader) ([]Employee, error) {
    reader := csv.NewReader(r)
    
    // Leer y validar header
    header, err := reader.Read()
    if err != nil {
        return nil, err
    }
    
    // Crear mapa de columna por nombre → indice
    colIndex := make(map[string]int)
    for i, name := range header {
        colIndex[strings.TrimSpace(strings.ToLower(name))] = i
    }
    
    var employees []Employee
    for {
        record, err := reader.Read()
        if err == io.EOF {
            break
        }
        if err != nil {
            return nil, err
        }
        
        id, _ := strconv.Atoi(record[colIndex["id"]])
        salary, _ := strconv.ParseFloat(record[colIndex["salary"]], 64)
        
        employees = append(employees, Employee{
            ID:         id,
            Name:       record[colIndex["name"]],
            Department: record[colIndex["department"]],
            Salary:     salary,
        })
    }
    
    return employees, nil
}

// Structs → CSV
func writeEmployees(w io.Writer, employees []Employee) error {
    writer := csv.NewWriter(w)
    defer writer.Flush()
    
    writer.Write([]string{"id", "name", "department", "salary"})
    
    for _, e := range employees {
        writer.Write([]string{
            strconv.Itoa(e.ID),
            e.Name,
            e.Department,
            strconv.FormatFloat(e.Salary, 'f', 2, 64),
        })
    }
    
    return writer.Error()
}
```

### 9.6 Caracteres especiales en CSV

```go
// El writer maneja automaticamente:
// - Campos con comas → se envuelven en comillas
// - Campos con comillas → se escapan doblando ("")
// - Campos con newlines → se envuelven en comillas

writer := csv.NewWriter(os.Stdout)
writer.Write([]string{"name", "description", "notes"})
writer.Write([]string{
    "Alice",
    "Developer, Senior",                // Contiene coma
    `She said "hello"`,                  // Contiene comillas
})
writer.Write([]string{
    "Bob",
    "Manager",
    "Line 1\nLine 2",                   // Contiene newline
})
writer.Flush()

// Output:
// name,description,notes
// Alice,"Developer, Senior","She said ""hello"""
// Bob,Manager,"Line 1
// Line 2"
```

---

## 10. encoding/xml — datos XML

### 10.1 XML struct tags

```go
import "encoding/xml"

// XML tags son similares a JSON tags pero con sintaxis adicional
// para atributos, namespaces, y estructura XML

type Person struct {
    XMLName   xml.Name `xml:"person"`          // Nombre del elemento raiz
    ID        int      `xml:"id,attr"`         // Atributo del elemento
    Name      string   `xml:"name"`            // Elemento hijo
    Email     string   `xml:"contact>email"`   // Elemento anidado: <contact><email>
    Phone     string   `xml:"contact>phone"`   // Mismo padre: <contact><phone>
    Nickname  string   `xml:"nickname,omitempty"` // Omitir si vacio
    Bio       string   `xml:",cdata"`          // Envolver en CDATA
    Notes     string   `xml:",comment"`        // Como comentario XML
    Extra     string   `xml:",innerxml"`       // XML crudo sin escapar
    Ignore    string   `xml:"-"`               // Nunca incluir
}
```

### 10.2 Marshal XML

```go
type RSS struct {
    XMLName xml.Name `xml:"rss"`
    Version string   `xml:"version,attr"`
    Channel Channel  `xml:"channel"`
}

type Channel struct {
    Title       string `xml:"title"`
    Link        string `xml:"link"`
    Description string `xml:"description"`
    Items       []Item `xml:"item"`
}

type Item struct {
    Title       string `xml:"title"`
    Link        string `xml:"link"`
    Description string `xml:"description"`
    PubDate     string `xml:"pubDate"`
}

func main() {
    rss := RSS{
        Version: "2.0",
        Channel: Channel{
            Title:       "Mi Blog",
            Link:        "https://example.com",
            Description: "Un blog de ejemplo",
            Items: []Item{
                {
                    Title:       "Primer post",
                    Link:        "https://example.com/post-1",
                    Description: "Contenido del primer post",
                    PubDate:     "Mon, 15 Jan 2024 10:30:00 GMT",
                },
            },
        },
    }
    
    // Marshal con indentacion
    data, err := xml.MarshalIndent(rss, "", "  ")
    if err != nil {
        log.Fatal(err)
    }
    
    // Agregar declaracion XML
    fmt.Println(xml.Header + string(data))
    // <?xml version="1.0" encoding="UTF-8"?>
    // <rss version="2.0">
    //   <channel>
    //     <title>Mi Blog</title>
    //     ...
    //   </channel>
    // </rss>
}
```

### 10.3 Unmarshal XML

```go
xmlData := []byte(`<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0">
  <channel>
    <title>Mi Blog</title>
    <link>https://example.com</link>
    <item>
      <title>Post 1</title>
      <link>https://example.com/1</link>
    </item>
    <item>
      <title>Post 2</title>
      <link>https://example.com/2</link>
    </item>
  </channel>
</rss>`)

var rss RSS
err := xml.Unmarshal(xmlData, &rss)
if err != nil {
    log.Fatal(err)
}

fmt.Println("Titulo:", rss.Channel.Title)
for _, item := range rss.Channel.Items {
    fmt.Printf("  - %s (%s)\n", item.Title, item.Link)
}
```

### 10.4 XML con namespaces

```go
type Envelope struct {
    XMLName xml.Name `xml:"http://schemas.xmlsoap.org/soap/envelope/ Envelope"`
    Body    Body     `xml:"http://schemas.xmlsoap.org/soap/envelope/ Body"`
}

type Body struct {
    Content string `xml:",innerxml"` // Contenido XML crudo
}

// Para XML con namespaces, el tag usa el formato:
// `xml:"namespace localname"`
// O para atributos con namespace:
// `xml:"namespace localname,attr"`
```

### 10.5 Encoder/Decoder streaming

```go
// XML tambien tiene streaming como JSON

// Decoder streaming
f, _ := os.Open("large.xml")
defer f.Close()

decoder := xml.NewDecoder(f)
for {
    token, err := decoder.Token()
    if err == io.EOF {
        break
    }
    if err != nil {
        log.Fatal(err)
    }
    
    switch t := token.(type) {
    case xml.StartElement:
        if t.Name.Local == "item" {
            var item Item
            decoder.DecodeElement(&item, &t) // Decodificar solo este elemento
            processItem(item)
        }
    case xml.CharData:
        // Texto entre tags
    case xml.Comment:
        // Comentario XML
    }
}

// Encoder streaming
encoder := xml.NewEncoder(os.Stdout)
encoder.Indent("", "  ")
encoder.Encode(data)
encoder.Flush()
```

### 10.6 XML tags — referencia completa

```
XML STRUCT TAGS

Tag                         Significado
──────────────────────────────────────────────────────────────────────
`xml:"name"`                Elemento hijo con nombre "name"
`xml:"name,attr"`           Atributo del elemento padre
`xml:"name,omitempty"`      Omitir si zero-value
`xml:"-"`                   Nunca incluir
`xml:",chardata"`           Contenido de texto del elemento
`xml:",cdata"`              Envolver en <![CDATA[...]]>
`xml:",comment"`            Comentario XML <!-- ... -->
`xml:",innerxml"`           XML crudo (sin escapar)
`xml:"a>b>c"`              Elementos anidados: <a><b><c>...</c></b></a>
`xml:"ns localname"`        Con namespace
`xml:",any"`                Capturar cualquier elemento hijo no mapeado

COMPARACION CON JSON:

JSON                        XML
──────────────────────────────────────────────
`json:"name"`               `xml:"name"`
`json:"name,omitempty"`     `xml:"name,omitempty"`
`json:"-"`                  `xml:"-"`
`json:",string"`            (no equivalente)
(no equivalente)            `xml:"name,attr"` (atributo)
(no equivalente)            `xml:"a>b"` (anidamiento)
(no equivalente)            `xml:",cdata"` (CDATA)
json.RawMessage             `xml:",innerxml"` (raw)
```

---

## 11. Comparacion entre formatos

```
┌───────────────────────┬──────────────────────┬──────────────────────┬──────────────────────┐
│ Propiedad              │ JSON                 │ CSV                  │ XML                  │
├───────────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤
│ Uso principal          │ APIs, config, logs   │ Datos tabulares      │ Legacy APIs, feeds   │
│ Package                │ encoding/json        │ encoding/csv         │ encoding/xml         │
│ Struct tags            │ `json:"..."`         │ No (manual)          │ `xml:"..."`          │
│ Tipos de datos         │ string,number,bool,  │ Solo string          │ Solo string (+attrs) │
│                        │ null,array,object    │                      │                      │
│ Anidamiento            │ Si (objetos/arrays)  │ No (plano)           │ Si (elementos)       │
│ Schema                 │ No (JSON Schema ext) │ No                   │ Si (XSD, DTD)        │
│ Human readable         │ Si                   │ Si                   │ Si (verbose)         │
│ Streaming              │ Encoder/Decoder      │ Reader/Writer        │ Encoder/Decoder      │
│ Batch                  │ Marshal/Unmarshal    │ ReadAll/WriteAll     │ Marshal/Unmarshal    │
│ Velocidad (Go)         │ Rapido               │ Muy rapido           │ Lento               │
│ Tamano de output       │ Medio                │ Pequeno              │ Grande               │
│ Comentarios            │ No                   │ Si (con Comment)     │ Si (<!-- -->)        │
│ Nil/null               │ null                 │ "" (vacio)           │ Omitir elemento      │
│ Binario                │ Base64 en string     │ Base64 en campo      │ Base64 o CDATA       │
│ Atributos              │ No (todo son campos) │ No                   │ Si (XML attrs)       │
│ Popular en             │ Web APIs, JS, config │ Hojas de calculo,    │ SOAP, RSS/Atom,      │
│                        │                      │ datos cientificos    │ enterprise           │
└───────────────────────┴──────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 12. Comparacion con C y Rust

```
ENCODING: GO vs C vs RUST

┌──────────────────────┬───────────────────────────┬────────────────────────┬──────────────────────────┐
│ Formato               │ Go                        │ C                      │ Rust                     │
├──────────────────────┼───────────────────────────┼────────────────────────┼──────────────────────────┤
│ JSON parse            │ encoding/json (stdlib)    │ cJSON, jansson (ext)   │ serde_json (crate)       │
│ JSON struct mapping   │ struct tags               │ Manual campo por campo │ #[derive(Deserialize)]   │
│ JSON streaming        │ NewDecoder/NewEncoder     │ Depende de libreria    │ serde_json::from_reader  │
│ CSV parse             │ encoding/csv (stdlib)     │ Manual o libcsv (ext)  │ csv crate                │
│ CSV struct mapping    │ Manual                    │ Manual                 │ #[derive(Deserialize)]   │
│ XML parse             │ encoding/xml (stdlib)     │ libxml2 (ext)          │ quick-xml (crate)        │
│ XML struct mapping    │ struct tags               │ Manual                 │ #[derive(Deserialize)]   │
│ Custom serialization  │ MarshalJSON/UnmarshalJSON │ Manual                 │ #[serde(with = "...")]   │
│ Binary                │ encoding/binary (stdlib)  │ Nativo (structs raw)   │ bincode, postcard (crate)│
│ Rendimiento JSON      │ Bueno (reflection-based)  │ Excelente (manual)     │ Excelente (serde = zero- │
│                        │                           │                        │  overhead, compile-time) │
│ Validacion             │ Manual post-unmarshal     │ Manual                 │ validator crate          │
│ Schema generation      │ No                        │ No                     │ schemars crate           │
└──────────────────────┴───────────────────────────┴────────────────────────┴──────────────────────────┘
```

```
DIFERENCIA CLAVE: REFLECTION vs CODEGEN

Go (encoding/json):
  - Usa REFLECTION en runtime para inspeccionar struct tags
  - Mas lento que codegen pero funciona sin paso de compilacion extra
  - No necesitas derive ni macros — solo struct tags
  - Versatil pero con overhead

Rust (serde):
  - Usa MACROS PROCEDURALES en compile-time
  - #[derive(Serialize, Deserialize)] genera codigo especializado
  - Zero overhead — el codigo generado es tan rapido como codigo manual
  - Necesita dependencia (serde) pero es quasi-universal en Rust

C:
  - Todo MANUAL — recorrer el JSON nodo por nodo
  - Sin reflection ni codegen (del lado del formato)
  - Maximo rendimiento posible pero maximo esfuerzo

RENDIMIENTO TIPICO (deserializar 1MB de JSON):
  C (cJSON manual):     ~5ms   (mas rapido, mas codigo)
  Rust (serde_json):    ~8ms   (casi igual a C, con derive)
  Go (encoding/json):   ~25ms  (reflection tiene overhead)
  Go (easyjson codegen):~10ms  (con generacion de codigo)
```

---

## 13. Programa completo: Data Converter

Este programa convierte entre JSON, CSV y XML, demostrando los tres paquetes de encoding:

```go
package main

import (
    "encoding/csv"
    "encoding/json"
    "encoding/xml"
    "fmt"
    "io"
    "os"
    "strconv"
    "strings"
)

// --- Tipos ---

type Record struct {
    ID         int     `json:"id"         xml:"id,attr"`
    Name       string  `json:"name"       xml:"name"`
    Department string  `json:"department" xml:"department"`
    Salary     float64 `json:"salary"     xml:"salary"`
    Active     bool    `json:"active"     xml:"active"`
}

type Records struct {
    XMLName xml.Name `json:"-"       xml:"records"`
    Items   []Record `json:"records" xml:"record"`
}

// --- Parse ---

func parseJSON(r io.Reader) ([]Record, error) {
    var data Records
    if err := json.NewDecoder(r).Decode(&data); err != nil {
        // Intentar como array directo
        var records []Record
        // Necesitamos re-leer, asi que esta rama asume el formato Records
        return nil, fmt.Errorf("json decode: %w", err)
    }
    return data.Items, nil
}

func parseJSONBytes(data []byte) ([]Record, error) {
    var wrapper Records
    if err := json.Unmarshal(data, &wrapper); err != nil {
        // Intentar como array plano
        var records []Record
        if err2 := json.Unmarshal(data, &records); err2 != nil {
            return nil, fmt.Errorf("json: %w", err)
        }
        return records, nil
    }
    return wrapper.Items, nil
}

func parseCSV(r io.Reader) ([]Record, error) {
    reader := csv.NewReader(r)
    reader.TrimLeadingSpace = true
    
    // Leer header
    header, err := reader.Read()
    if err != nil {
        return nil, fmt.Errorf("csv header: %w", err)
    }
    
    // Mapear columnas
    colIdx := make(map[string]int)
    for i, h := range header {
        colIdx[strings.TrimSpace(strings.ToLower(h))] = i
    }
    
    // Verificar columnas requeridas
    required := []string{"id", "name", "department", "salary", "active"}
    for _, col := range required {
        if _, ok := colIdx[col]; !ok {
            return nil, fmt.Errorf("missing column: %s", col)
        }
    }
    
    var records []Record
    lineNum := 1
    for {
        row, err := reader.Read()
        if err == io.EOF {
            break
        }
        if err != nil {
            return nil, fmt.Errorf("csv line %d: %w", lineNum+1, err)
        }
        lineNum++
        
        id, err := strconv.Atoi(strings.TrimSpace(row[colIdx["id"]]))
        if err != nil {
            return nil, fmt.Errorf("line %d: invalid id: %w", lineNum, err)
        }
        
        salary, err := strconv.ParseFloat(strings.TrimSpace(row[colIdx["salary"]]), 64)
        if err != nil {
            return nil, fmt.Errorf("line %d: invalid salary: %w", lineNum, err)
        }
        
        active := strings.TrimSpace(strings.ToLower(row[colIdx["active"]]))
        
        records = append(records, Record{
            ID:         id,
            Name:       strings.TrimSpace(row[colIdx["name"]]),
            Department: strings.TrimSpace(row[colIdx["department"]]),
            Salary:     salary,
            Active:     active == "true" || active == "1" || active == "yes",
        })
    }
    
    return records, nil
}

func parseXML(r io.Reader) ([]Record, error) {
    var data Records
    decoder := xml.NewDecoder(r)
    if err := decoder.Decode(&data); err != nil {
        return nil, fmt.Errorf("xml decode: %w", err)
    }
    return data.Items, nil
}

// --- Format ---

func formatJSON(w io.Writer, records []Record) error {
    data := Records{Items: records}
    encoder := json.NewEncoder(w)
    encoder.SetIndent("", "  ")
    return encoder.Encode(data)
}

func formatCSV(w io.Writer, records []Record) error {
    writer := csv.NewWriter(w)
    defer writer.Flush()
    
    // Header
    writer.Write([]string{"id", "name", "department", "salary", "active"})
    
    // Data
    for _, r := range records {
        writer.Write([]string{
            strconv.Itoa(r.ID),
            r.Name,
            r.Department,
            strconv.FormatFloat(r.Salary, 'f', 2, 64),
            strconv.FormatBool(r.Active),
        })
    }
    
    return writer.Error()
}

func formatXML(w io.Writer, records []Record) error {
    data := Records{Items: records}
    
    // XML declaration
    io.WriteString(w, xml.Header)
    
    encoder := xml.NewEncoder(w)
    encoder.Indent("", "  ")
    if err := encoder.Encode(data); err != nil {
        return err
    }
    io.WriteString(w, "\n")
    return encoder.Close()
}

func formatTable(w io.Writer, records []Record) error {
    // Header
    fmt.Fprintf(w, "%-6s %-20s %-15s %12s %-8s\n",
        "ID", "NAME", "DEPARTMENT", "SALARY", "ACTIVE")
    fmt.Fprintf(w, "%s\n", strings.Repeat("-", 65))
    
    // Data
    totalSalary := 0.0
    for _, r := range records {
        active := "no"
        if r.Active {
            active = "yes"
        }
        fmt.Fprintf(w, "%-6d %-20s %-15s %12.2f %-8s\n",
            r.ID, r.Name, r.Department, r.Salary, active)
        totalSalary += r.Salary
    }
    
    fmt.Fprintf(w, "%s\n", strings.Repeat("-", 65))
    fmt.Fprintf(w, "%-6s %-20s %-15s %12.2f\n",
        "", fmt.Sprintf("%d records", len(records)), "TOTAL", totalSalary)
    
    return nil
}

// --- Main ---

func main() {
    if len(os.Args) < 3 {
        fmt.Fprintln(os.Stderr, "Usage: converter <input-format> <output-format> [file]")
        fmt.Fprintln(os.Stderr, "  Formats: json, csv, xml, table")
        fmt.Fprintln(os.Stderr, "  If no file given, reads from stdin")
        fmt.Fprintln(os.Stderr)
        fmt.Fprintln(os.Stderr, "Examples:")
        fmt.Fprintln(os.Stderr, "  converter json csv data.json")
        fmt.Fprintln(os.Stderr, "  converter csv xml < data.csv")
        fmt.Fprintln(os.Stderr, "  converter json table data.json")
        os.Exit(1)
    }
    
    inFormat := strings.ToLower(os.Args[1])
    outFormat := strings.ToLower(os.Args[2])
    
    // Determinar input source
    var input io.Reader = os.Stdin
    if len(os.Args) >= 4 {
        f, err := os.Open(os.Args[3])
        if err != nil {
            fmt.Fprintf(os.Stderr, "open: %v\n", err)
            os.Exit(1)
        }
        defer f.Close()
        input = f
    }
    
    // Parse input
    var records []Record
    var err error
    
    switch inFormat {
    case "json":
        data, readErr := io.ReadAll(input)
        if readErr != nil {
            fmt.Fprintf(os.Stderr, "read: %v\n", readErr)
            os.Exit(1)
        }
        records, err = parseJSONBytes(data)
    case "csv":
        records, err = parseCSV(input)
    case "xml":
        records, err = parseXML(input)
    default:
        fmt.Fprintf(os.Stderr, "unknown input format: %s\n", inFormat)
        os.Exit(1)
    }
    
    if err != nil {
        fmt.Fprintf(os.Stderr, "parse %s: %v\n", inFormat, err)
        os.Exit(1)
    }
    
    // Format output
    switch outFormat {
    case "json":
        err = formatJSON(os.Stdout, records)
    case "csv":
        err = formatCSV(os.Stdout, records)
    case "xml":
        err = formatXML(os.Stdout, records)
    case "table":
        err = formatTable(os.Stdout, records)
    default:
        fmt.Fprintf(os.Stderr, "unknown output format: %s\n", outFormat)
        os.Exit(1)
    }
    
    if err != nil {
        fmt.Fprintf(os.Stderr, "format %s: %v\n", outFormat, err)
        os.Exit(1)
    }
}
```

```
PROBAR EL PROGRAMA:

# Crear datos de prueba en JSON
cat > data.json << 'EOF'
{"records": [
  {"id": 1, "name": "Alice Johnson", "department": "Engineering", "salary": 95000.00, "active": true},
  {"id": 2, "name": "Bob Smith", "department": "Marketing", "salary": 72000.50, "active": true},
  {"id": 3, "name": "Charlie Brown", "department": "Engineering", "salary": 88000.00, "active": false},
  {"id": 4, "name": "Diana Prince", "department": "Management", "salary": 120000.00, "active": true}
]}
EOF

# JSON → CSV
go run main.go json csv data.json

# JSON → XML
go run main.go json xml data.json

# JSON → Table
go run main.go json table data.json

# CSV → JSON (pipe)
go run main.go json csv data.json | go run main.go csv json

# JSON → XML → JSON (roundtrip)
go run main.go json xml data.json | go run main.go xml json

CONCEPTOS DEMOSTRADOS:
  encoding/json: Decoder/Encoder streaming, Marshal, struct tags, RawMessage
  encoding/csv:  Reader/Writer, header parsing, column mapping
  encoding/xml:  Decoder/Encoder, struct tags (,attr), xml.Header
  io:            Reader/Writer como abstraccion central (stdin/stdout/file)
  strconv:       Conversion string ↔ numeros para CSV
  strings:       TrimSpace, ToLower, Split para parsing
  Dual tags:     Un struct con `json:"..." xml:"..."` simultaneos
```

---

## 14. Ejercicios

### Ejercicio 1: JSON API con validacion
Crea un servidor HTTP que acepte POST de un JSON con este formato:
```json
{"name": "Alice", "email": "alice@example.com", "age": 30, "tags": ["admin", "user"]}
```
Implementa:
- Struct con tags JSON correctos (omitempty para tags)
- `DisallowUnknownFields()` para rechazar campos extra
- Metodo `Validate()` que verifique: name no vacio, email contiene @, age 1-150, max 10 tags
- Errores detallados en JSON: `{"errors": [{"field": "age", "message": "must be 1-150"}]}`
- Endpoint GET que retorne todos los registros como JSON array

### Ejercicio 2: CSV transformer
Escribe un programa que:
- Lea un CSV con columnas: date, category, amount, description
- Agrupe por category y calcule: count, sum, avg, min, max de amount
- Genere un CSV de resumen con esas estadisticas
- Opcionalmente genere un JSON con el resumen (flag `-format json`)
- Maneje archivos con delimitadores `,` `;` y `\t` (auto-deteccion o flag)

### Ejercicio 3: Custom JSON marshaling
Crea un tipo `SensitiveString` que:
- Se decodifique normalmente desde JSON
- Al serializarse, muestre solo los primeros 3 caracteres + "***" (ej: "sec***")
- Tenga un metodo `Reveal() string` que retorne el valor completo
- Usalo en un struct `User` con campos `Password SensitiveString` y `SSN SensitiveString`
- Verifica que `json.Marshal` oculta los datos pero internamente siguen disponibles

### Ejercicio 4: XML RSS reader
Crea un programa que:
- Lea un feed RSS/Atom desde una URL (usa `http.Get`)
- Parse el XML con `encoding/xml` y struct tags
- Muestre los items en formato tabla (titulo, fecha, link)
- Opcionalmente exporte a CSV o JSON (flag `-output json|csv|table`)
- Maneje tanto RSS 2.0 (`<rss><channel><item>`) como Atom (`<feed><entry>`)

---

> **Siguiente**: C09/S02 - I/O, T04 - Linea de comandos — os.Args, flag package, cobra (tercero), stdin/stdout/stderr
