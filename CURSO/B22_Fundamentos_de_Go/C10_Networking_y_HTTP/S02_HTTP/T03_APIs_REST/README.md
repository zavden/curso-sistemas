# APIs REST — JSON request/response, routing, status codes, patrones de handler

## 1. Introduccion

En T01 y T02 aprendimos las piezas individuales del cliente y servidor HTTP. Este topico las integra en el contexto de **APIs REST** — el patron dominante para servicios web. Cubrimos patrones arquitectonicos, convenciones de diseño, manejo de errores estructurado, validacion, paginacion, filtrado, versionado, testing, y un servidor completo con separacion de capas.

```
┌───────────────────────────────────────────────────────────────────────────────────┐
│                    API REST EN GO — ARQUITECTURA DE CAPAS                       │
├───────────────────────────────────────────────────────────────────────────────────┤
│                                                                                   │
│  ┌────────────────────────────────────────────────────────────────────┐           │
│  │                    CAPA HTTP (Transport)                           │           │
│  │  ├─ Router (ServeMux)    → mapear URL+metodo a handler           │           │
│  │  ├─ Middleware            → logging, auth, CORS, rate limit       │           │
│  │  ├─ Request decoding      → JSON body → struct, path/query params │           │
│  │  ├─ Response encoding     → struct → JSON, status codes          │           │
│  │  └─ Error handling        → errores → JSON responses con codes   │           │
│  └────────────────────────────────┬───────────────────────────────────┘           │
│                                    │                                              │
│  ┌────────────────────────────────▼───────────────────────────────────┐           │
│  │                    CAPA SERVICIO (Business Logic)                  │           │
│  │  ├─ Validacion de negocio  → reglas que no son de HTTP            │           │
│  │  ├─ Orquestacion           → coordinar multiples operaciones     │           │
│  │  ├─ Autorizacion           → puede este usuario hacer esto?      │           │
│  │  └─ Errores de dominio     → NotFound, Conflict, Forbidden       │           │
│  └────────────────────────────────┬───────────────────────────────────┘           │
│                                    │                                              │
│  ┌────────────────────────────────▼───────────────────────────────────┐           │
│  │                    CAPA DATOS (Repository/Store)                   │           │
│  │  ├─ CRUD                   → Create, Read, Update, Delete         │           │
│  │  ├─ Queries                → filtrado, paginacion, ordenamiento  │           │
│  │  ├─ Transacciones          → operaciones atomicas                │           │
│  │  └─ En memoria / SQL / etc → implementacion intercambiable       │           │
│  └────────────────────────────────────────────────────────────────────┘           │
│                                                                                   │
│  POR QUE SEPARAR EN CAPAS:                                                       │
│  ├─ El handler HTTP NO contiene logica de negocio                               │
│  ├─ La logica de negocio NO sabe sobre HTTP (testeable sin servidor)            │
│  ├─ El store NO sabe sobre negocio (intercambiable: memoria, SQL, etc.)         │
│  ├─ Cada capa es testeable independientemente                                    │
│  └─ Los errores fluyen de abajo hacia arriba y se traducen en cada capa         │
│                                                                                   │
│  FLUJO DE UN REQUEST:                                                            │
│  1. Request llega al router                                                      │
│  2. Middleware se ejecuta (log, auth, etc.)                                      │
│  3. Handler decodifica request (JSON body, path params, query)                  │
│  4. Handler llama al servicio                                                    │
│  5. Servicio valida, aplica reglas, llama al store                              │
│  6. Store ejecuta la operacion                                                   │
│  7. Resultado sube: store → servicio → handler → JSON response                 │
│  8. Si hay error en cualquier punto, se traduce al nivel apropiado              │
│                                                                                   │
│  CONVENCIONES REST:                                                              │
│  ├─ Recursos como sustantivos: /users, /posts, /comments                        │
│  ├─ Metodos HTTP como verbos: GET=leer, POST=crear, PUT/PATCH=actualizar       │
│  ├─ Status codes semanticos: 200, 201, 204, 400, 404, 409, 500                 │
│  ├─ JSON como formato de intercambio                                             │
│  ├─ URLs predecibles: /resources/{id}/sub-resources                             │
│  └─ HATEOAS (opcional): links a recursos relacionados en la respuesta          │
└───────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Convenciones REST

### 2.1 Diseño de URLs

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    DISEÑO DE URLs REST                                   │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  RECURSO PRINCIPAL                                                       │
│  GET    /api/v1/users          → listar usuarios                       │
│  POST   /api/v1/users          → crear usuario                         │
│  GET    /api/v1/users/{id}     → obtener un usuario                    │
│  PUT    /api/v1/users/{id}     → reemplazar usuario completo           │
│  PATCH  /api/v1/users/{id}     → actualizar campos parciales           │
│  DELETE /api/v1/users/{id}     → eliminar usuario                      │
│                                                                          │
│  SUB-RECURSOS                                                            │
│  GET    /api/v1/users/{id}/posts        → posts de un usuario          │
│  POST   /api/v1/users/{id}/posts        → crear post para usuario     │
│  GET    /api/v1/users/{id}/posts/{pid}  → un post especifico          │
│                                                                          │
│  ACCIONES NO-CRUD (verbos como excepciones)                             │
│  POST   /api/v1/users/{id}/activate     → activar cuenta              │
│  POST   /api/v1/users/{id}/reset-password → resetear password         │
│  POST   /api/v1/reports/generate        → generar reporte             │
│                                                                          │
│  QUERY PARAMETERS (filtrado, paginacion, ordenamiento)                  │
│  GET /api/v1/users?page=2&limit=20                                     │
│  GET /api/v1/users?sort=created_at&order=desc                          │
│  GET /api/v1/users?role=admin&active=true                              │
│  GET /api/v1/users?search=alice                                        │
│  GET /api/v1/users?fields=id,name,email   (sparse fields)             │
│                                                                          │
│  REGLAS:                                                                 │
│  ✓ Sustantivos plurales: /users (no /user)                             │
│  ✓ Minusculas con guiones: /user-profiles (no /userProfiles)           │
│  ✓ Sin trailing slash: /users (no /users/)                             │
│  ✓ IDs en el path: /users/42 (no /users?id=42)                        │
│  ✓ Anidamiento maximo 2 niveles: /users/{id}/posts                    │
│  ✗ No verbos en URLs: /getUsers → GET /users                          │
│  ✗ No extensiones: /users.json → Accept: application/json             │
└──────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Status codes

```
┌────────┬────────────────────────────────┬────────────────────────────────────┐
│ Code   │ Nombre                         │ Cuando usarlo                      │
├────────┼────────────────────────────────┼────────────────────────────────────┤
│ 2xx    │ EXITO                          │                                    │
├────────┼────────────────────────────────┼────────────────────────────────────┤
│ 200    │ OK                             │ GET exitoso, PATCH exitoso        │
│ 201    │ Created                        │ POST que creo un recurso          │
│ 204    │ No Content                     │ DELETE exitoso, PUT sin body resp  │
├────────┼────────────────────────────────┼────────────────────────────────────┤
│ 3xx    │ REDIRECCION                    │                                    │
├────────┼────────────────────────────────┼────────────────────────────────────┤
│ 301    │ Moved Permanently             │ Recurso se movio (URL canonica)   │
│ 304    │ Not Modified                   │ Cache valido (ETag/If-None-Match) │
├────────┼────────────────────────────────┼────────────────────────────────────┤
│ 4xx    │ ERROR DEL CLIENTE              │                                    │
├────────┼────────────────────────────────┼────────────────────────────────────┤
│ 400    │ Bad Request                    │ JSON invalido, campos faltantes   │
│ 401    │ Unauthorized                   │ No autenticado (falta token)      │
│ 403    │ Forbidden                      │ Autenticado pero sin permiso      │
│ 404    │ Not Found                      │ Recurso no existe                 │
│ 405    │ Method Not Allowed             │ Metodo HTTP no soportado          │
│ 409    │ Conflict                       │ Conflicto (email ya existe, etc.) │
│ 415    │ Unsupported Media Type         │ Content-Type no soportado        │
│ 422    │ Unprocessable Entity           │ Datos validos pero semanticamente  │
│        │                                │ incorrectos (edad=-5)             │
│ 429    │ Too Many Requests              │ Rate limit excedido               │
├────────┼────────────────────────────────┼────────────────────────────────────┤
│ 5xx    │ ERROR DEL SERVIDOR             │                                    │
├────────┼────────────────────────────────┼────────────────────────────────────┤
│ 500    │ Internal Server Error          │ Bug, panic, error inesperado      │
│ 502    │ Bad Gateway                    │ Servicio upstream caido           │
│ 503    │ Service Unavailable            │ Servidor sobrecargado/maintenance │
│ 504    │ Gateway Timeout                │ Servicio upstream timeout         │
└────────┴────────────────────────────────┴────────────────────────────────────┘

REGLA SIMPLE:
  200: GET/PATCH exitoso, retornando datos
  201: POST exitoso, retornando recurso creado + Location header
  204: DELETE exitoso, sin body
  400: el cliente envio datos incorrectos (JSON malformado, campo invalido)
  401: no me dijiste quien eres (falta/invalido token)
  403: se quien eres pero no puedes hacer eso
  404: el recurso no existe
  409: la operacion conflicta con el estado actual (duplicado)
  422: los datos son validos JSON pero no tienen sentido (edad negativa)
  500: algo salio mal en el servidor (bug)
```

### 2.3 PUT vs PATCH

```
PUT:   Reemplazar TODO el recurso
       Debes enviar TODOS los campos
       Los campos no enviados se setean a zero/default
       Idempotente: PUT dos veces = mismo resultado

PATCH: Actualizar SOLO los campos enviados
       Solo envias los campos que cambian
       Los campos no enviados NO se tocan
       Idempotente en la practica (depende de la implementacion)

Ejemplo:
  Recurso actual: {"name": "Alice", "email": "alice@ex.com", "role": "admin"}

  PUT  {"name": "Alice Updated"}
  Resultado: {"name": "Alice Updated", "email": "", "role": ""}
  → Email y role se perdieron!

  PATCH {"name": "Alice Updated"}
  Resultado: {"name": "Alice Updated", "email": "alice@ex.com", "role": "admin"}
  → Solo name cambio

En Go, la diferencia se implementa con punteros:

// PUT: todos los campos requeridos
type UserPut struct {
    Name  string `json:"name"`
    Email string `json:"email"`
    Role  string `json:"role"`
}

// PATCH: campos opcionales (punteros — nil = no enviado)
type UserPatch struct {
    Name  *string `json:"name"`
    Email *string `json:"email"`
    Role  *string `json:"role"`
}

// Si el campo es nil → no fue enviado → no tocar
// Si el campo apunta a "" → fue enviado como vacio → setear a ""
```

---

## 3. Errores estructurados

### 3.1 Formato de errores

```go
// Una API profesional retorna errores consistentes y utiles.
// No solo un string, sino un objeto estructurado.

// Formato de error
type APIError struct {
    Status  int           `json:"-"`                  // HTTP status code (no en JSON)
    Code    string        `json:"code"`               // Codigo de error (estable para parseo)
    Message string        `json:"message"`            // Mensaje human-readable
    Details []FieldError  `json:"details,omitempty"`  // Errores por campo
    TraceID string        `json:"trace_id,omitempty"` // Para debugging
}

type FieldError struct {
    Field   string `json:"field"`
    Message string `json:"message"`
    Code    string `json:"code,omitempty"` // "required", "invalid", "too_long"
}

func (e *APIError) Error() string {
    return fmt.Sprintf("%s: %s", e.Code, e.Message)
}

// Constructores para errores comunes
func ErrBadRequest(message string, details ...FieldError) *APIError {
    return &APIError{
        Status:  http.StatusBadRequest,
        Code:    "bad_request",
        Message: message,
        Details: details,
    }
}

func ErrNotFound(resource, id string) *APIError {
    return &APIError{
        Status:  http.StatusNotFound,
        Code:    "not_found",
        Message: fmt.Sprintf("%s with id %q not found", resource, id),
    }
}

func ErrConflict(message string) *APIError {
    return &APIError{
        Status:  http.StatusConflict,
        Code:    "conflict",
        Message: message,
    }
}

func ErrUnauthorized(message string) *APIError {
    return &APIError{
        Status:  http.StatusUnauthorized,
        Code:    "unauthorized",
        Message: message,
    }
}

func ErrForbidden(message string) *APIError {
    return &APIError{
        Status:  http.StatusForbidden,
        Code:    "forbidden",
        Message: message,
    }
}

func ErrInternal(traceID string) *APIError {
    return &APIError{
        Status:  http.StatusInternalServerError,
        Code:    "internal_error",
        Message: "an internal error occurred",
        TraceID: traceID,
    }
}

func ErrValidation(details []FieldError) *APIError {
    return &APIError{
        Status:  http.StatusUnprocessableEntity,
        Code:    "validation_error",
        Message: "one or more fields are invalid",
        Details: details,
    }
}
```

### 3.2 Respuesta de errores en la practica

```json
// 400 Bad Request — JSON malformado
{
  "code": "bad_request",
  "message": "invalid JSON: unexpected end of JSON input"
}

// 404 Not Found
{
  "code": "not_found",
  "message": "user with id \"42\" not found"
}

// 409 Conflict — email duplicado
{
  "code": "conflict",
  "message": "email already registered"
}

// 422 Validation Error — multiples campos invalidos
{
  "code": "validation_error",
  "message": "one or more fields are invalid",
  "details": [
    {"field": "name", "message": "is required", "code": "required"},
    {"field": "email", "message": "invalid email format", "code": "invalid"},
    {"field": "age", "message": "must be between 1 and 150", "code": "out_of_range"}
  ]
}

// 500 Internal Error — sin detalles al cliente
{
  "code": "internal_error",
  "message": "an internal error occurred",
  "trace_id": "a1b2c3d4e5f6"
}
```

### 3.3 writeError centralizado

```go
func writeJSON(w http.ResponseWriter, status int, data interface{}) {
    w.Header().Set("Content-Type", "application/json")
    w.WriteHeader(status)
    if data != nil {
        json.NewEncoder(w).Encode(data)
    }
}

func writeAPIError(w http.ResponseWriter, r *http.Request, err *APIError) {
    // Agregar trace ID si no tiene
    if err.TraceID == "" {
        if id, ok := r.Context().Value(requestIDKey).(string); ok {
            err.TraceID = id
        }
    }
    
    writeJSON(w, err.Status, err)
}

// Uso en handlers:
func getUser(w http.ResponseWriter, r *http.Request) {
    id := r.PathValue("id")
    
    user, err := userService.GetByID(r.Context(), id)
    if err != nil {
        var apiErr *APIError
        if errors.As(err, &apiErr) {
            writeAPIError(w, r, apiErr)
        } else {
            // Error inesperado — loggear internamente, responder genérico
            slog.Error("unexpected error", "error", err, "path", r.URL.Path)
            writeAPIError(w, r, ErrInternal(""))
        }
        return
    }
    
    writeJSON(w, http.StatusOK, user)
}
```

---

## 4. Request decoding y validacion

### 4.1 Decoder generico de JSON

```go
// Funcion reutilizable que decodifica JSON body con todas las verificaciones

func decodeJSON[T any](r *http.Request, maxBytes int64) (T, *APIError) {
    var v T
    
    // Verificar Content-Type
    ct := r.Header.Get("Content-Type")
    if ct != "" && !strings.HasPrefix(ct, "application/json") {
        return v, &APIError{
            Status:  http.StatusUnsupportedMediaType,
            Code:    "unsupported_media_type",
            Message: "Content-Type must be application/json",
        }
    }
    
    // Limitar tamano
    r.Body = http.MaxBytesReader(nil, r.Body, maxBytes)
    
    decoder := json.NewDecoder(r.Body)
    decoder.DisallowUnknownFields()
    
    if err := decoder.Decode(&v); err != nil {
        var maxBytesErr *http.MaxBytesError
        var syntaxErr *json.SyntaxError
        var typeErr *json.UnmarshalTypeError
        
        switch {
        case errors.As(err, &maxBytesErr):
            return v, &APIError{
                Status:  http.StatusRequestEntityTooLarge,
                Code:    "body_too_large",
                Message: fmt.Sprintf("body must not exceed %d bytes", maxBytes),
            }
        case errors.Is(err, io.EOF):
            return v, ErrBadRequest("request body is empty")
        case errors.As(err, &syntaxErr):
            return v, ErrBadRequest(fmt.Sprintf("malformed JSON at position %d", syntaxErr.Offset))
        case errors.As(err, &typeErr):
            return v, ErrBadRequest(fmt.Sprintf("invalid type for field %q: expected %s", typeErr.Field, typeErr.Type))
        case strings.HasPrefix(err.Error(), "json: unknown field"):
            field := strings.TrimPrefix(err.Error(), "json: unknown field ")
            return v, ErrBadRequest(fmt.Sprintf("unknown field %s", field))
        default:
            return v, ErrBadRequest("invalid JSON: " + err.Error())
        }
    }
    
    // Verificar que no hay mas datos
    if decoder.More() {
        return v, ErrBadRequest("body must contain a single JSON object")
    }
    
    return v, nil
}

// Uso:
func createUser(w http.ResponseWriter, r *http.Request) {
    input, apiErr := decodeJSON[CreateUserInput](r, 1<<20)
    if apiErr != nil {
        writeAPIError(w, r, apiErr)
        return
    }
    // input es de tipo CreateUserInput, listo para validar
}
```

### 4.2 Validacion con interface

```go
// Patron: cada input struct implementa Validate()

type Validatable interface {
    Validate() []FieldError
}

func decodeAndValidate[T Validatable](r *http.Request, maxBytes int64) (T, *APIError) {
    v, apiErr := decodeJSON[T](r, maxBytes)
    if apiErr != nil {
        return v, apiErr
    }
    
    if errs := v.Validate(); len(errs) > 0 {
        return v, ErrValidation(errs)
    }
    
    return v, nil
}

// --- Input types con validacion ---

type CreateUserInput struct {
    Name     string   `json:"name"`
    Email    string   `json:"email"`
    Password string   `json:"password"`
    Role     string   `json:"role"`
    Tags     []string `json:"tags"`
}

func (i CreateUserInput) Validate() []FieldError {
    var errs []FieldError
    
    if strings.TrimSpace(i.Name) == "" {
        errs = append(errs, FieldError{Field: "name", Message: "is required", Code: "required"})
    } else if len(i.Name) > 100 {
        errs = append(errs, FieldError{Field: "name", Message: "must be 100 characters or less", Code: "too_long"})
    }
    
    if i.Email == "" {
        errs = append(errs, FieldError{Field: "email", Message: "is required", Code: "required"})
    } else if !strings.Contains(i.Email, "@") || !strings.Contains(i.Email, ".") {
        errs = append(errs, FieldError{Field: "email", Message: "invalid email format", Code: "invalid"})
    }
    
    if i.Password == "" {
        errs = append(errs, FieldError{Field: "password", Message: "is required", Code: "required"})
    } else if len(i.Password) < 8 {
        errs = append(errs, FieldError{Field: "password", Message: "must be at least 8 characters", Code: "too_short"})
    }
    
    validRoles := map[string]bool{"user": true, "admin": true, "editor": true}
    if i.Role != "" && !validRoles[i.Role] {
        errs = append(errs, FieldError{Field: "role", Message: "must be one of: user, admin, editor", Code: "invalid"})
    }
    
    if len(i.Tags) > 10 {
        errs = append(errs, FieldError{Field: "tags", Message: "maximum 10 tags allowed", Code: "too_many"})
    }
    
    return errs
}

// PATCH input — campos opcionales con punteros
type UpdateUserInput struct {
    Name  *string  `json:"name"`
    Email *string  `json:"email"`
    Role  *string  `json:"role"`
}

func (i UpdateUserInput) Validate() []FieldError {
    var errs []FieldError
    
    if i.Name != nil && strings.TrimSpace(*i.Name) == "" {
        errs = append(errs, FieldError{Field: "name", Message: "cannot be empty", Code: "invalid"})
    }
    if i.Name != nil && len(*i.Name) > 100 {
        errs = append(errs, FieldError{Field: "name", Message: "must be 100 characters or less", Code: "too_long"})
    }
    
    if i.Email != nil && !strings.Contains(*i.Email, "@") {
        errs = append(errs, FieldError{Field: "email", Message: "invalid email format", Code: "invalid"})
    }
    
    validRoles := map[string]bool{"user": true, "admin": true, "editor": true}
    if i.Role != nil && !validRoles[*i.Role] {
        errs = append(errs, FieldError{Field: "role", Message: "must be one of: user, admin, editor", Code: "invalid"})
    }
    
    return errs
}

// Uso:
func createUser(w http.ResponseWriter, r *http.Request) {
    input, apiErr := decodeAndValidate[CreateUserInput](r, 1<<20)
    if apiErr != nil {
        writeAPIError(w, r, apiErr)
        return
    }
    // input esta validado y listo
}
```

---

## 5. Paginacion

### 5.1 Offset-based pagination

```go
// El mas comun: ?page=2&limit=20

type PaginationParams struct {
    Page  int
    Limit int
}

type PaginatedResponse[T any] struct {
    Data       []T        `json:"data"`
    Pagination Pagination `json:"pagination"`
}

type Pagination struct {
    Page       int  `json:"page"`
    Limit      int  `json:"limit"`
    Total      int  `json:"total"`
    TotalPages int  `json:"total_pages"`
    HasNext    bool `json:"has_next"`
    HasPrev    bool `json:"has_prev"`
}

func parsePagination(r *http.Request, defaultLimit, maxLimit int) PaginationParams {
    q := r.URL.Query()
    
    page, _ := strconv.Atoi(q.Get("page"))
    if page < 1 {
        page = 1
    }
    
    limit, _ := strconv.Atoi(q.Get("limit"))
    if limit < 1 {
        limit = defaultLimit
    }
    if limit > maxLimit {
        limit = maxLimit
    }
    
    return PaginationParams{Page: page, Limit: limit}
}

func newPaginatedResponse[T any](data []T, page, limit, total int) PaginatedResponse[T] {
    totalPages := (total + limit - 1) / limit
    
    if data == nil {
        data = []T{} // nunca retornar null
    }
    
    return PaginatedResponse[T]{
        Data: data,
        Pagination: Pagination{
            Page:       page,
            Limit:      limit,
            Total:      total,
            TotalPages: totalPages,
            HasNext:    page < totalPages,
            HasPrev:    page > 1,
        },
    }
}

// Uso en handler:
func listUsers(w http.ResponseWriter, r *http.Request) {
    pg := parsePagination(r, 20, 100)
    
    users, total, err := userService.List(r.Context(), pg.Page, pg.Limit)
    if err != nil {
        writeAPIError(w, r, ErrInternal(""))
        return
    }
    
    writeJSON(w, http.StatusOK, newPaginatedResponse(users, pg.Page, pg.Limit, total))
}
```

```json
// Response:
{
  "data": [
    {"id": "1", "name": "Alice", "email": "alice@example.com"},
    {"id": "2", "name": "Bob", "email": "bob@example.com"}
  ],
  "pagination": {
    "page": 2,
    "limit": 20,
    "total": 55,
    "total_pages": 3,
    "has_next": true,
    "has_prev": true
  }
}
```

### 5.2 Cursor-based pagination

```go
// Mejor para datasets grandes y en tiempo real.
// En vez de page number, usa un cursor (generalmente el ID del ultimo item).

type CursorParams struct {
    Cursor string
    Limit  int
}

type CursorResponse[T any] struct {
    Data       []T     `json:"data"`
    NextCursor *string `json:"next_cursor"` // null si no hay mas
    HasMore    bool    `json:"has_more"`
}

func parseCursor(r *http.Request, defaultLimit, maxLimit int) CursorParams {
    q := r.URL.Query()
    
    limit, _ := strconv.Atoi(q.Get("limit"))
    if limit < 1 {
        limit = defaultLimit
    }
    if limit > maxLimit {
        limit = maxLimit
    }
    
    return CursorParams{
        Cursor: q.Get("cursor"),
        Limit:  limit,
    }
}

// En el store, el query es:
// SELECT * FROM users WHERE id > cursor ORDER BY id LIMIT limit+1
// Pedir limit+1 para saber si hay mas

func (s *UserStore) ListWithCursor(cursor string, limit int) ([]User, *string, error) {
    // Pedir uno extra para detectar has_more
    results := s.queryAfter(cursor, limit+1)
    
    var nextCursor *string
    hasMore := len(results) > limit
    
    if hasMore {
        results = results[:limit] // recortar al limite
        last := results[len(results)-1].ID
        nextCursor = &last
    }
    
    return results, nextCursor, nil
}

// Response:
// {"data": [...], "next_cursor": "user_abc123", "has_more": true}
// Siguiente pagina: GET /api/users?cursor=user_abc123&limit=20
```

### 5.3 Offset vs Cursor

```
┌───────────────────────┬──────────────────────┬──────────────────────┐
│ Aspecto               │ Offset (page+limit)  │ Cursor               │
├───────────────────────┼──────────────────────┼──────────────────────┤
│ Implementacion        │ Simple               │ Mas compleja         │
│ "Ir a pagina 5"      │ ✓ Facil              │ ✗ No soportado       │
│ Total items           │ ✓ Disponible         │ ✗ Opcional/costoso   │
│ Datasets grandes      │ ✗ OFFSET lento       │ ✓ Siempre rapido     │
│ Datos en tiempo real  │ ✗ Duplicados/perdidos│ ✓ Consistente        │
│ Inserciones           │ ✗ Cambia paginas     │ ✓ No afecta          │
│ API publica           │ ✓ Familiar           │ △ Menos conocido     │
│ Uso tipico            │ Admin panels, UIs    │ Feeds, timelines     │
│ Ejemplo               │ GitHub Issues        │ Twitter Timeline     │
└───────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 6. Filtrado y ordenamiento

```go
// Patron: parsear filtros del query string de forma generica

type ListParams struct {
    // Paginacion
    Page  int
    Limit int
    
    // Ordenamiento
    SortBy string
    Order  string // "asc" o "desc"
    
    // Busqueda
    Search string
    
    // Filtros
    Filters map[string]string
}

func parseListParams(r *http.Request, allowedSorts []string, allowedFilters []string) ListParams {
    q := r.URL.Query()
    
    params := ListParams{
        Page:    max(1, queryInt(q, "page", 1)),
        Limit:   clamp(queryInt(q, "limit", 20), 1, 100),
        Search:  q.Get("search"),
        Filters: make(map[string]string),
    }
    
    // Ordenamiento con whitelist
    sortBy := q.Get("sort")
    for _, allowed := range allowedSorts {
        if sortBy == allowed {
            params.SortBy = sortBy
            break
        }
    }
    if params.SortBy == "" && len(allowedSorts) > 0 {
        params.SortBy = allowedSorts[0] // default al primero
    }
    
    order := strings.ToLower(q.Get("order"))
    if order == "asc" || order == "desc" {
        params.Order = order
    } else {
        params.Order = "desc"
    }
    
    // Filtros con whitelist (evitar injection)
    for _, key := range allowedFilters {
        if val := q.Get(key); val != "" {
            params.Filters[key] = val
        }
    }
    
    return params
}

func queryInt(q url.Values, key string, def int) int {
    s := q.Get(key)
    if s == "" {
        return def
    }
    n, err := strconv.Atoi(s)
    if err != nil {
        return def
    }
    return n
}

func clamp(n, min, max int) int {
    if n < min { return min }
    if n > max { return max }
    return n
}

// Uso:
func listUsers(w http.ResponseWriter, r *http.Request) {
    params := parseListParams(r,
        []string{"created_at", "name", "email"},    // sorts permitidos
        []string{"role", "active", "department"},    // filtros permitidos
    )
    
    // params.Filters["role"] = "admin" (si ?role=admin)
    // params.SortBy = "created_at", params.Order = "desc"
    
    users, total, err := userService.List(r.Context(), params)
    // ...
}

// Request ejemplo:
// GET /api/users?page=2&limit=10&sort=name&order=asc&role=admin&search=alice
```

---

## 7. Versionado de APIs

### 7.1 Estrategias

```
┌──────────────────────────────────────────────────────────────────────┐
│                    VERSIONADO DE APIs                               │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  1. URL PREFIX (lo mas comun)                                        │
│     GET /api/v1/users                                                │
│     GET /api/v2/users                                                │
│     ✓ Obvio y facil de entender                                     │
│     ✓ Facil de rutear (un mux por version)                          │
│     ✗ Puede llevar a duplicacion de codigo                          │
│                                                                      │
│  2. HEADER                                                           │
│     Accept: application/vnd.myapi.v2+json                           │
│     X-API-Version: 2                                                 │
│     ✓ URL limpia                                                    │
│     ✗ Menos descubrible                                              │
│     ✗ Dificil de probar en browser                                  │
│                                                                      │
│  3. QUERY PARAMETER                                                  │
│     GET /api/users?version=2                                         │
│     ✓ Facil de probar                                               │
│     ✗ No es muy RESTful                                              │
│                                                                      │
│  RECOMENDACION: URL prefix (/v1, /v2) — simple y efectivo          │
└──────────────────────────────────────────────────────────────────────┘
```

### 7.2 Implementacion con URL prefix

```go
func main() {
    mux := http.NewServeMux()
    
    // V1 handlers
    v1Users := &UserHandlerV1{service: userService}
    mux.HandleFunc("GET /api/v1/users", v1Users.List)
    mux.HandleFunc("POST /api/v1/users", v1Users.Create)
    mux.HandleFunc("GET /api/v1/users/{id}", v1Users.Get)
    
    // V2 handlers (nuevos campos, formato diferente)
    v2Users := &UserHandlerV2{service: userService}
    mux.HandleFunc("GET /api/v2/users", v2Users.List)
    mux.HandleFunc("POST /api/v2/users", v2Users.Create)
    mux.HandleFunc("GET /api/v2/users/{id}", v2Users.Get)
    
    http.ListenAndServe(":8080", mux)
}

// La logica de negocio (servicio) es la misma.
// Solo cambia la capa HTTP (input/output format).
```

---

## 8. Patron handler: separacion HTTP / logica

### 8.1 Estructura del proyecto

```
myapi/
├── main.go                  // setup, wiring, server start
├── internal/
│   ├── api/                 // Capa HTTP
│   │   ├── router.go        // ServeMux + middleware setup
│   │   ├── middleware.go     // logging, auth, cors, etc.
│   │   ├── errors.go        // APIError, writeJSON, writeError
│   │   ├── decode.go        // decodeJSON, parsePagination
│   │   └── handlers/
│   │       ├── users.go     // UserHandler struct + metodos
│   │       ├── posts.go     // PostHandler
│   │       └── health.go    // health check
│   ├── domain/              // Tipos de dominio (puros, sin dependencias)
│   │   ├── user.go          // type User struct, type UserFilter
│   │   └── post.go          // type Post struct
│   ├── service/             // Logica de negocio
│   │   ├── user.go          // UserService con validacion y reglas
│   │   └── post.go          // PostService
│   └── store/               // Acceso a datos
│       ├── memory/          // Implementacion en memoria
│       │   └── user.go
│       └── postgres/        // Implementacion PostgreSQL
│           └── user.go
└── go.mod
```

### 8.2 Domain types

```go
// internal/domain/user.go

package domain

import "time"

type User struct {
    ID        string    `json:"id"`
    Name      string    `json:"name"`
    Email     string    `json:"email"`
    Role      string    `json:"role"`
    Active    bool      `json:"active"`
    CreatedAt time.Time `json:"created_at"`
    UpdatedAt time.Time `json:"updated_at"`
}

// Errores de dominio (NO son errores HTTP)
type ErrNotFound struct {
    Resource string
    ID       string
}

func (e ErrNotFound) Error() string {
    return fmt.Sprintf("%s %q not found", e.Resource, e.ID)
}

type ErrConflict struct {
    Message string
}

func (e ErrConflict) Error() string {
    return e.Message
}

type ErrForbidden struct {
    Message string
}

func (e ErrForbidden) Error() string {
    return e.Message
}
```

### 8.3 Store interface

```go
// internal/domain/user.go (o store interface separada)

type UserStore interface {
    List(ctx context.Context, filter UserFilter) ([]User, int, error)
    GetByID(ctx context.Context, id string) (*User, error)
    GetByEmail(ctx context.Context, email string) (*User, error)
    Create(ctx context.Context, user *User) error
    Update(ctx context.Context, user *User) error
    Delete(ctx context.Context, id string) error
}

type UserFilter struct {
    Search string
    Role   string
    Active *bool
    SortBy string
    Order  string
    Offset int
    Limit  int
}
```

### 8.4 Service layer

```go
// internal/service/user.go

package service

type UserService struct {
    store domain.UserStore
}

func NewUserService(store domain.UserStore) *UserService {
    return &UserService{store: store}
}

func (s *UserService) List(ctx context.Context, filter domain.UserFilter) ([]domain.User, int, error) {
    return s.store.List(ctx, filter)
}

func (s *UserService) GetByID(ctx context.Context, id string) (*domain.User, error) {
    user, err := s.store.GetByID(ctx, id)
    if err != nil {
        return nil, err
    }
    if user == nil {
        return nil, domain.ErrNotFound{Resource: "user", ID: id}
    }
    return user, nil
}

func (s *UserService) Create(ctx context.Context, name, email, password, role string) (*domain.User, error) {
    // Regla de negocio: email unico
    existing, _ := s.store.GetByEmail(ctx, email)
    if existing != nil {
        return nil, domain.ErrConflict{Message: "email already registered"}
    }
    
    // Regla de negocio: role default
    if role == "" {
        role = "user"
    }
    
    now := time.Now()
    user := &domain.User{
        ID:        generateID(),
        Name:      name,
        Email:     strings.ToLower(email),
        Role:      role,
        Active:    true,
        CreatedAt: now,
        UpdatedAt: now,
    }
    
    if err := s.store.Create(ctx, user); err != nil {
        return nil, err
    }
    
    return user, nil
}

func (s *UserService) Update(ctx context.Context, id string, name, email, role *string) (*domain.User, error) {
    user, err := s.GetByID(ctx, id)
    if err != nil {
        return nil, err
    }
    
    if name != nil {
        user.Name = *name
    }
    if email != nil {
        lower := strings.ToLower(*email)
        // Verificar unicidad si cambio el email
        if lower != user.Email {
            existing, _ := s.store.GetByEmail(ctx, lower)
            if existing != nil {
                return nil, domain.ErrConflict{Message: "email already registered"}
            }
        }
        user.Email = lower
    }
    if role != nil {
        user.Role = *role
    }
    user.UpdatedAt = time.Now()
    
    if err := s.store.Update(ctx, user); err != nil {
        return nil, err
    }
    
    return user, nil
}

func (s *UserService) Delete(ctx context.Context, id string) error {
    _, err := s.GetByID(ctx, id)
    if err != nil {
        return err
    }
    return s.store.Delete(ctx, id)
}
```

### 8.5 HTTP handler (traduccion entre HTTP y dominio)

```go
// internal/api/handlers/users.go

package handlers

type UserHandler struct {
    service *service.UserService
}

func NewUserHandler(svc *service.UserService) *UserHandler {
    return &UserHandler{service: svc}
}

func (h *UserHandler) List(w http.ResponseWriter, r *http.Request) {
    params := api.ParseListParams(r,
        []string{"created_at", "name", "email"},
        []string{"role", "active"},
    )
    
    filter := domain.UserFilter{
        Search: params.Search,
        Role:   params.Filters["role"],
        SortBy: params.SortBy,
        Order:  params.Order,
        Offset: (params.Page - 1) * params.Limit,
        Limit:  params.Limit,
    }
    
    if activeStr := params.Filters["active"]; activeStr != "" {
        active := activeStr == "true"
        filter.Active = &active
    }
    
    users, total, err := h.service.List(r.Context(), filter)
    if err != nil {
        handleError(w, r, err)
        return
    }
    
    api.WriteJSON(w, http.StatusOK, api.NewPaginatedResponse(users, params.Page, params.Limit, total))
}

func (h *UserHandler) Get(w http.ResponseWriter, r *http.Request) {
    id := r.PathValue("id")
    
    user, err := h.service.GetByID(r.Context(), id)
    if err != nil {
        handleError(w, r, err)
        return
    }
    
    api.WriteJSON(w, http.StatusOK, user)
}

func (h *UserHandler) Create(w http.ResponseWriter, r *http.Request) {
    input, apiErr := api.DecodeAndValidate[CreateUserInput](r, 1<<20)
    if apiErr != nil {
        api.WriteAPIError(w, r, apiErr)
        return
    }
    
    user, err := h.service.Create(r.Context(), input.Name, input.Email, input.Password, input.Role)
    if err != nil {
        handleError(w, r, err)
        return
    }
    
    w.Header().Set("Location", "/api/v1/users/"+user.ID)
    api.WriteJSON(w, http.StatusCreated, user)
}

func (h *UserHandler) Update(w http.ResponseWriter, r *http.Request) {
    id := r.PathValue("id")
    
    input, apiErr := api.DecodeAndValidate[UpdateUserInput](r, 1<<20)
    if apiErr != nil {
        api.WriteAPIError(w, r, apiErr)
        return
    }
    
    user, err := h.service.Update(r.Context(), id, input.Name, input.Email, input.Role)
    if err != nil {
        handleError(w, r, err)
        return
    }
    
    api.WriteJSON(w, http.StatusOK, user)
}

func (h *UserHandler) Delete(w http.ResponseWriter, r *http.Request) {
    id := r.PathValue("id")
    
    if err := h.service.Delete(r.Context(), id); err != nil {
        handleError(w, r, err)
        return
    }
    
    w.WriteHeader(http.StatusNoContent)
}

// handleError traduce errores de dominio a respuestas HTTP.
// Este es el UNICO lugar donde errores de dominio se convierten en HTTP.
func handleError(w http.ResponseWriter, r *http.Request, err error) {
    var notFound domain.ErrNotFound
    var conflict domain.ErrConflict
    var forbidden domain.ErrForbidden
    
    switch {
    case errors.As(err, &notFound):
        api.WriteAPIError(w, r, api.ErrNotFound(notFound.Resource, notFound.ID))
    case errors.As(err, &conflict):
        api.WriteAPIError(w, r, api.ErrConflict(conflict.Message))
    case errors.As(err, &forbidden):
        api.WriteAPIError(w, r, api.ErrForbidden(forbidden.Message))
    default:
        slog.Error("unexpected error", "error", err, "path", r.URL.Path)
        api.WriteAPIError(w, r, api.ErrInternal(""))
    }
}
```

### 8.6 Router setup

```go
// internal/api/router.go

func NewRouter(userHandler *handlers.UserHandler) http.Handler {
    mux := http.NewServeMux()
    
    // Health
    mux.HandleFunc("GET /health", func(w http.ResponseWriter, r *http.Request) {
        api.WriteJSON(w, 200, map[string]string{"status": "ok"})
    })
    
    // Users API v1
    mux.HandleFunc("GET /api/v1/users", userHandler.List)
    mux.HandleFunc("POST /api/v1/users", userHandler.Create)
    mux.HandleFunc("GET /api/v1/users/{id}", userHandler.Get)
    mux.HandleFunc("PATCH /api/v1/users/{id}", userHandler.Update)
    mux.HandleFunc("DELETE /api/v1/users/{id}", userHandler.Delete)
    
    // Middleware chain
    handler := chain(mux,
        recoveryMW,
        requestIDMW,
        loggingMW,
        corsMW,
    )
    
    return handler
}
```

### 8.7 Main — wiring

```go
// main.go

func main() {
    // Store
    userStore := memory.NewUserStore()
    
    // Service
    userService := service.NewUserService(userStore)
    
    // Handlers
    userHandler := handlers.NewUserHandler(userService)
    
    // Router
    router := api.NewRouter(userHandler)
    
    // Server
    server := &http.Server{
        Addr:              ":8080",
        Handler:           router,
        ReadTimeout:       5 * time.Second,
        WriteTimeout:      10 * time.Second,
        IdleTimeout:       120 * time.Second,
    }
    
    // Start + graceful shutdown
    go func() {
        slog.Info("server starting", "addr", server.Addr)
        if err := server.ListenAndServe(); err != http.ErrServerClosed {
            log.Fatal(err)
        }
    }()
    
    ctx, stop := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
    defer stop()
    <-ctx.Done()
    
    shutdownCtx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
    defer cancel()
    server.Shutdown(shutdownCtx)
    slog.Info("server stopped")
}
```

---

## 9. Testing de APIs

### 9.1 httptest para testing sin red

```go
import (
    "net/http/httptest"
    "testing"
)

func TestListUsers(t *testing.T) {
    // Crear dependencias
    store := memory.NewUserStore()
    svc := service.NewUserService(store)
    handler := handlers.NewUserHandler(svc)
    
    // Crear un usuario de prueba
    svc.Create(context.Background(), "Alice", "alice@test.com", "pass", "user")
    
    // Crear request
    req := httptest.NewRequest("GET", "/api/v1/users", nil)
    rec := httptest.NewRecorder()
    
    // Ejecutar handler
    handler.List(rec, req)
    
    // Verificar
    res := rec.Result()
    defer res.Body.Close()
    
    if res.StatusCode != http.StatusOK {
        t.Errorf("expected 200, got %d", res.StatusCode)
    }
    
    if ct := res.Header.Get("Content-Type"); ct != "application/json" {
        t.Errorf("expected application/json, got %s", ct)
    }
    
    var response api.PaginatedResponse[domain.User]
    json.NewDecoder(res.Body).Decode(&response)
    
    if len(response.Data) != 1 {
        t.Errorf("expected 1 user, got %d", len(response.Data))
    }
    if response.Data[0].Name != "Alice" {
        t.Errorf("expected Alice, got %s", response.Data[0].Name)
    }
}
```

### 9.2 Test de integracion con httptest.Server

```go
func TestAPI_Integration(t *testing.T) {
    // Setup completo (misma configuracion que produccion)
    store := memory.NewUserStore()
    svc := service.NewUserService(store)
    userHandler := handlers.NewUserHandler(svc)
    router := api.NewRouter(userHandler)
    
    // Servidor de test (auto-selecciona un puerto)
    server := httptest.NewServer(router)
    defer server.Close()
    
    client := server.Client()
    baseURL := server.URL
    
    // Test: crear usuario
    body := `{"name":"Alice","email":"alice@test.com","password":"12345678","role":"user"}`
    resp, err := client.Post(baseURL+"/api/v1/users", "application/json", strings.NewReader(body))
    if err != nil {
        t.Fatal(err)
    }
    defer resp.Body.Close()
    
    if resp.StatusCode != http.StatusCreated {
        b, _ := io.ReadAll(resp.Body)
        t.Fatalf("expected 201, got %d: %s", resp.StatusCode, b)
    }
    
    var user domain.User
    json.NewDecoder(resp.Body).Decode(&user)
    
    if user.Name != "Alice" {
        t.Errorf("expected Alice, got %s", user.Name)
    }
    if user.ID == "" {
        t.Error("expected non-empty ID")
    }
    
    // Test: obtener usuario
    resp, _ = client.Get(baseURL + "/api/v1/users/" + user.ID)
    defer resp.Body.Close()
    
    if resp.StatusCode != http.StatusOK {
        t.Errorf("expected 200, got %d", resp.StatusCode)
    }
    
    // Test: usuario no existe
    resp, _ = client.Get(baseURL + "/api/v1/users/nonexistent")
    defer resp.Body.Close()
    
    if resp.StatusCode != http.StatusNotFound {
        t.Errorf("expected 404, got %d", resp.StatusCode)
    }
    
    // Test: crear duplicado (email)
    resp, _ = client.Post(baseURL+"/api/v1/users", "application/json", strings.NewReader(body))
    defer resp.Body.Close()
    
    if resp.StatusCode != http.StatusConflict {
        t.Errorf("expected 409 for duplicate email, got %d", resp.StatusCode)
    }
    
    // Test: validacion
    badBody := `{"name":"","email":"not-an-email","password":"short"}`
    resp, _ = client.Post(baseURL+"/api/v1/users", "application/json", strings.NewReader(badBody))
    defer resp.Body.Close()
    
    if resp.StatusCode != http.StatusUnprocessableEntity {
        t.Errorf("expected 422 for validation, got %d", resp.StatusCode)
    }
    
    var apiErr api.APIError
    json.NewDecoder(resp.Body).Decode(&apiErr)
    if len(apiErr.Details) == 0 {
        t.Error("expected validation details")
    }
}
```

### 9.3 Table-driven tests para handlers

```go
func TestCreateUser_Validation(t *testing.T) {
    store := memory.NewUserStore()
    svc := service.NewUserService(store)
    handler := handlers.NewUserHandler(svc)
    
    tests := []struct {
        name       string
        body       string
        wantStatus int
        wantCode   string
    }{
        {
            name:       "empty body",
            body:       "",
            wantStatus: 400,
            wantCode:   "bad_request",
        },
        {
            name:       "invalid JSON",
            body:       "{invalid}",
            wantStatus: 400,
            wantCode:   "bad_request",
        },
        {
            name:       "missing required fields",
            body:       `{"role":"user"}`,
            wantStatus: 422,
            wantCode:   "validation_error",
        },
        {
            name:       "invalid email",
            body:       `{"name":"Alice","email":"bad","password":"12345678"}`,
            wantStatus: 422,
            wantCode:   "validation_error",
        },
        {
            name:       "password too short",
            body:       `{"name":"Alice","email":"a@b.com","password":"123"}`,
            wantStatus: 422,
            wantCode:   "validation_error",
        },
        {
            name:       "valid input",
            body:       `{"name":"Alice","email":"alice@test.com","password":"12345678"}`,
            wantStatus: 201,
        },
    }
    
    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            req := httptest.NewRequest("POST", "/api/v1/users", strings.NewReader(tt.body))
            req.Header.Set("Content-Type", "application/json")
            rec := httptest.NewRecorder()
            
            handler.Create(rec, req)
            
            if rec.Code != tt.wantStatus {
                t.Errorf("status = %d, want %d (body: %s)", rec.Code, tt.wantStatus, rec.Body.String())
            }
            
            if tt.wantCode != "" {
                var apiErr api.APIError
                json.NewDecoder(rec.Body).Decode(&apiErr)
                if apiErr.Code != tt.wantCode {
                    t.Errorf("code = %q, want %q", apiErr.Code, tt.wantCode)
                }
            }
        })
    }
}
```

---

## 10. Patrones avanzados

### 10.1 Envelope pattern

```go
// Envolver todas las respuestas en un formato consistente

type Envelope struct {
    Data    interface{} `json:"data,omitempty"`
    Error   *APIError   `json:"error,omitempty"`
    Meta    *Meta       `json:"meta,omitempty"`
}

type Meta struct {
    RequestID string `json:"request_id,omitempty"`
    Timestamp string `json:"timestamp"`
    Version   string `json:"version"`
}

func writeSuccess(w http.ResponseWriter, r *http.Request, status int, data interface{}) {
    env := Envelope{
        Data: data,
        Meta: &Meta{
            RequestID: getRequestID(r),
            Timestamp: time.Now().UTC().Format(time.RFC3339),
            Version:   "v1",
        },
    }
    writeJSON(w, status, env)
}

// Response siempre tiene la misma forma:
// Exito:
// {"data": {...}, "meta": {"request_id": "abc", "timestamp": "...", "version": "v1"}}
//
// Error:
// {"error": {"code": "not_found", "message": "..."}, "meta": {...}}
```

### 10.2 HATEOAS (links a recursos relacionados)

```go
// Agregar links para navegabilidad del API

type UserResponse struct {
    *domain.User
    Links map[string]string `json:"_links"`
}

func toUserResponse(u *domain.User) UserResponse {
    return UserResponse{
        User: u,
        Links: map[string]string{
            "self":  fmt.Sprintf("/api/v1/users/%s", u.ID),
            "posts": fmt.Sprintf("/api/v1/users/%s/posts", u.ID),
        },
    }
}

// Response:
// {
//   "id": "abc123",
//   "name": "Alice",
//   "_links": {
//     "self": "/api/v1/users/abc123",
//     "posts": "/api/v1/users/abc123/posts"
//   }
// }
```

### 10.3 Bulk operations

```go
// Crear o actualizar multiples recursos en una request

type BulkCreateInput struct {
    Items []CreateUserInput `json:"items"`
}

type BulkResult struct {
    Succeeded int           `json:"succeeded"`
    Failed    int           `json:"failed"`
    Errors    []BulkError   `json:"errors,omitempty"`
    Items     []domain.User `json:"items,omitempty"`
}

type BulkError struct {
    Index   int    `json:"index"`
    Message string `json:"message"`
}

func (h *UserHandler) BulkCreate(w http.ResponseWriter, r *http.Request) {
    input, apiErr := api.DecodeJSON[BulkCreateInput](r, 10<<20) // 10 MB para bulk
    if apiErr != nil {
        api.WriteAPIError(w, r, apiErr)
        return
    }
    
    if len(input.Items) == 0 {
        api.WriteAPIError(w, r, api.ErrBadRequest("items array is empty"))
        return
    }
    if len(input.Items) > 100 {
        api.WriteAPIError(w, r, api.ErrBadRequest("maximum 100 items per request"))
        return
    }
    
    result := BulkResult{}
    
    for i, item := range input.Items {
        if errs := item.Validate(); len(errs) > 0 {
            result.Failed++
            result.Errors = append(result.Errors, BulkError{
                Index:   i,
                Message: fmt.Sprintf("validation: %s", errs[0].Message),
            })
            continue
        }
        
        user, err := h.service.Create(r.Context(), item.Name, item.Email, item.Password, item.Role)
        if err != nil {
            result.Failed++
            result.Errors = append(result.Errors, BulkError{
                Index:   i,
                Message: err.Error(),
            })
            continue
        }
        
        result.Succeeded++
        result.Items = append(result.Items, *user)
    }
    
    status := http.StatusCreated
    if result.Failed > 0 {
        status = http.StatusMultiStatus // 207
    }
    
    api.WriteJSON(w, status, result)
}
```

### 10.4 Idempotency keys

```go
// Para POSTs que crean recursos, un idempotency key
// permite reintentar sin crear duplicados.

type idempotencyStore struct {
    mu      sync.Mutex
    results map[string]cachedResponse
}

type cachedResponse struct {
    status int
    body   []byte
    expiry time.Time
}

func idempotencyMiddleware(store *idempotencyStore) func(http.Handler) http.Handler {
    return func(next http.Handler) http.Handler {
        return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
            // Solo aplica a POST (operaciones no idempotentes)
            if r.Method != "POST" {
                next.ServeHTTP(w, r)
                return
            }
            
            key := r.Header.Get("Idempotency-Key")
            if key == "" {
                next.ServeHTTP(w, r)
                return
            }
            
            // Verificar si ya tenemos un resultado
            store.mu.Lock()
            if cached, ok := store.results[key]; ok && time.Now().Before(cached.expiry) {
                store.mu.Unlock()
                w.Header().Set("Content-Type", "application/json")
                w.Header().Set("X-Idempotent-Replayed", "true")
                w.WriteHeader(cached.status)
                w.Write(cached.body)
                return
            }
            store.mu.Unlock()
            
            // Ejecutar y capturar resultado
            rec := httptest.NewRecorder()
            next.ServeHTTP(rec, r)
            
            // Guardar resultado
            result := rec.Result()
            body := rec.Body.Bytes()
            
            store.mu.Lock()
            store.results[key] = cachedResponse{
                status: result.StatusCode,
                body:   body,
                expiry: time.Now().Add(24 * time.Hour),
            }
            store.mu.Unlock()
            
            // Copiar respuesta
            for k, v := range result.Header {
                w.Header()[k] = v
            }
            w.WriteHeader(result.StatusCode)
            w.Write(body)
        })
    }
}

// Uso del cliente:
// POST /api/v1/payments
// Idempotency-Key: unique-uuid-123
// Content-Type: application/json
// {"amount": 100}
//
// Si el cliente reenvia la misma request con el mismo key,
// recibe el mismo resultado sin ejecutar la operacion de nuevo.
```

### 10.5 ETag y conditional requests

```go
import "crypto/sha256"

func (h *UserHandler) Get(w http.ResponseWriter, r *http.Request) {
    id := r.PathValue("id")
    
    user, err := h.service.GetByID(r.Context(), id)
    if err != nil {
        handleError(w, r, err)
        return
    }
    
    // Generar ETag basado en el contenido
    data, _ := json.Marshal(user)
    hash := sha256.Sum256(data)
    etag := fmt.Sprintf(`"%x"`, hash[:8])
    
    w.Header().Set("ETag", etag)
    w.Header().Set("Cache-Control", "private, max-age=0, must-revalidate")
    
    // Verificar If-None-Match
    if match := r.Header.Get("If-None-Match"); match == etag {
        w.WriteHeader(http.StatusNotModified) // 304
        return
    }
    
    api.WriteJSON(w, http.StatusOK, user)
}

// Request 1:
// GET /api/v1/users/42
// Response: 200 OK + ETag: "a1b2c3d4"
//
// Request 2 (con cache):
// GET /api/v1/users/42
// If-None-Match: "a1b2c3d4"
// Response: 304 Not Modified (sin body — el cliente usa su cache)
```

---

## 11. Comparacion con C y Rust

```
┌──────────────────────────────────┬─────────────────────┬─────────────────────────┬──────────────────────────┐
│ Aspecto                          │ C                   │ Go                      │ Rust (axum)              │
├──────────────────────────────────┼─────────────────────┼─────────────────────────┼──────────────────────────┤
│ JSON decode request              │ cJSON_Parse(buf)    │ json.Decoder(r.Body)    │ Json<T> extractor        │
│ JSON encode response             │ cJSON_Print(obj)    │ json.Encoder(w)         │ Json(data)               │
│ Validacion                       │ manual if/else      │ manual (Validate())     │ validator crate (derive) │
│ Path params                      │ sscanf/manual       │ r.PathValue("id")       │ Path<(u32,)>             │
│ Query params                     │ parse manual        │ r.URL.Query()           │ Query<T> extractor       │
│ Paginacion                       │ manual              │ manual (helper funcs)   │ manual (o paginator crate)│
│ Error types                      │ enum + switch       │ error interface + As    │ enum + thiserror         │
│ Error → HTTP status              │ switch(err)         │ errors.As → switch      │ IntoResponse trait       │
│ Middleware                       │ function pointer    │ func(Handler)Handler    │ tower Layer              │
│ Request context                  │ void* userdata      │ context.Context         │ Extension<T>             │
│ HTTP testing                     │ manual TCP socket   │ httptest package        │ axum::test               │
│ Separacion de capas              │ manual              │ interfaces + packages   │ traits + modules         │
│ Auth middleware                  │ manual              │ middleware + context    │ middleware + Extension   │
│ CORS                             │ manual headers      │ middleware              │ tower-http CorsLayer     │
│ File upload                      │ manual multipart    │ r.FormFile()            │ Multipart extractor      │
│ Graceful shutdown                │ signal handler      │ server.Shutdown(ctx)    │ axum::serve graceful     │
│ Status codes                     │ printf("HTTP/1.1    │ w.WriteHeader(201)      │ StatusCode::CREATED      │
│                                  │   201 Created\r\n") │                         │                          │
│ PUT vs PATCH                     │ manual              │ *string (nil = no set)  │ Option<T> (None = skip)  │
│ Idempotency                      │ manual              │ middleware              │ middleware               │
│ API versioning                   │ URL prefix manual   │ URL prefix / ServeMux   │ URL prefix / Router      │
│ Database store                   │ libpq/sqlite3       │ database/sql o sqlx     │ sqlx / diesel            │
│ Dependency injection             │ struct + ptrs       │ struct + interfaces     │ State<T> extractor       │
│ Complejidad CRUD basico          │ ~500 lineas         │ ~150 lineas             │ ~100 lineas              │
│ Complejidad API produccion       │ ~5000 lineas        │ ~800 lineas             │ ~600 lineas              │
└──────────────────────────────────┴─────────────────────┴─────────────────────────┴──────────────────────────┘
```

### Insight: validation en tres lenguajes

```
// C: validacion manual
int validate_user(create_user_input *input, char *errors[], int *err_count) {
    *err_count = 0;
    if (strlen(input->name) == 0) {
        errors[(*err_count)++] = "name is required";
    }
    if (!strchr(input->email, '@')) {
        errors[(*err_count)++] = "invalid email";
    }
    if (strlen(input->password) < 8) {
        errors[(*err_count)++] = "password too short";
    }
    return *err_count == 0;
}

// Go: validacion con metodo
func (i CreateUserInput) Validate() []FieldError {
    var errs []FieldError
    if i.Name == "" {
        errs = append(errs, FieldError{Field: "name", Message: "required"})
    }
    if !strings.Contains(i.Email, "@") {
        errs = append(errs, FieldError{Field: "email", Message: "invalid"})
    }
    if len(i.Password) < 8 {
        errs = append(errs, FieldError{Field: "password", Message: "too short"})
    }
    return errs
}

// Rust (validator derive):
#[derive(Deserialize, Validate)]
struct CreateUser {
    #[validate(length(min = 1, message = "required"))]
    name: String,
    #[validate(email(message = "invalid email"))]
    email: String,
    #[validate(length(min = 8, message = "too short"))]
    password: String,
}
// let errs = input.validate()?; // Una linea
```

---

## 12. Programa completo: Task Board API

API REST completa con autenticacion, roles, CRUD, paginacion, filtrado y tests.

```go
package main

import (
    "context"
    "crypto/rand"
    "encoding/json"
    "errors"
    "fmt"
    "io"
    "log"
    "log/slog"
    "net"
    "net/http"
    "os"
    "os/signal"
    "runtime/debug"
    "sort"
    "strconv"
    "strings"
    "sync"
    "syscall"
    "time"
)

// ===== Domain =====

type Task struct {
    ID          string     `json:"id"`
    Title       string     `json:"title"`
    Description string     `json:"description,omitempty"`
    Status      string     `json:"status"` // "todo", "in_progress", "done"
    Priority    string     `json:"priority"` // "low", "medium", "high"
    AssigneeID  string     `json:"assignee_id,omitempty"`
    Tags        []string   `json:"tags,omitempty"`
    CreatedBy   string     `json:"created_by"`
    CreatedAt   time.Time  `json:"created_at"`
    UpdatedAt   time.Time  `json:"updated_at"`
    DoneAt      *time.Time `json:"done_at,omitempty"`
}

type User struct {
    ID       string `json:"id"`
    Username string `json:"username"`
    Role     string `json:"role"` // "user", "admin"
    Token    string `json:"-"`
}

// Domain errors
type errNotFound struct{ resource, id string }
func (e errNotFound) Error() string { return fmt.Sprintf("%s %q not found", e.resource, e.id) }

type errConflict struct{ message string }
func (e errConflict) Error() string { return e.message }

type errForbidden struct{ message string }
func (e errForbidden) Error() string { return e.message }

// ===== Store =====

type TaskStore struct {
    mu    sync.RWMutex
    tasks map[string]*Task
}

func NewTaskStore() *TaskStore {
    return &TaskStore{tasks: make(map[string]*Task)}
}

func (s *TaskStore) Create(task *Task) {
    s.mu.Lock()
    defer s.mu.Unlock()
    s.tasks[task.ID] = task
}

func (s *TaskStore) Get(id string) (*Task, bool) {
    s.mu.RLock()
    defer s.mu.RUnlock()
    t, ok := s.tasks[id]
    if !ok {
        return nil, false
    }
    copy := *t
    return &copy, true
}

func (s *TaskStore) Update(task *Task) {
    s.mu.Lock()
    defer s.mu.Unlock()
    s.tasks[task.ID] = task
}

func (s *TaskStore) Delete(id string) {
    s.mu.Lock()
    defer s.mu.Unlock()
    delete(s.tasks, id)
}

type TaskFilter struct {
    Status     string
    Priority   string
    AssigneeID string
    Tag        string
    Search     string
    SortBy     string
    Order      string
}

func (s *TaskStore) List(filter TaskFilter) []*Task {
    s.mu.RLock()
    defer s.mu.RUnlock()
    
    var result []*Task
    for _, t := range s.tasks {
        if filter.Status != "" && t.Status != filter.Status {
            continue
        }
        if filter.Priority != "" && t.Priority != filter.Priority {
            continue
        }
        if filter.AssigneeID != "" && t.AssigneeID != filter.AssigneeID {
            continue
        }
        if filter.Tag != "" {
            found := false
            for _, tag := range t.Tags {
                if tag == filter.Tag {
                    found = true
                    break
                }
            }
            if !found {
                continue
            }
        }
        if filter.Search != "" {
            s := strings.ToLower(filter.Search)
            if !strings.Contains(strings.ToLower(t.Title), s) &&
                !strings.Contains(strings.ToLower(t.Description), s) {
                continue
            }
        }
        copy := *t
        result = append(result, &copy)
    }
    
    // Sort
    sort.Slice(result, func(i, j int) bool {
        var less bool
        switch filter.SortBy {
        case "title":
            less = result[i].Title < result[j].Title
        case "priority":
            less = priorityOrder(result[i].Priority) < priorityOrder(result[j].Priority)
        case "status":
            less = result[i].Status < result[j].Status
        default: // created_at
            less = result[i].CreatedAt.Before(result[j].CreatedAt)
        }
        if filter.Order == "asc" {
            return less
        }
        return !less
    })
    
    return result
}

func priorityOrder(p string) int {
    switch p {
    case "high": return 3
    case "medium": return 2
    case "low": return 1
    default: return 0
    }
}

func (s *TaskStore) Count() int {
    s.mu.RLock()
    defer s.mu.RUnlock()
    return len(s.tasks)
}

// ===== Auth Store =====

type AuthStore struct {
    mu     sync.RWMutex
    users  map[string]*User // id → user
    tokens map[string]*User // token → user
}

func NewAuthStore() *AuthStore {
    return &AuthStore{
        users:  make(map[string]*User),
        tokens: make(map[string]*User),
    }
}

func (s *AuthStore) Register(username, role string) (*User, string) {
    s.mu.Lock()
    defer s.mu.Unlock()
    
    token := genID() + genID()
    user := &User{
        ID:       genID(),
        Username: username,
        Role:     role,
        Token:    token,
    }
    s.users[user.ID] = user
    s.tokens[token] = user
    return user, token
}

func (s *AuthStore) GetByToken(token string) (*User, bool) {
    s.mu.RLock()
    defer s.mu.RUnlock()
    u, ok := s.tokens[token]
    return u, ok
}

// ===== Service =====

type TaskService struct {
    store *TaskStore
}

func NewTaskService(store *TaskStore) *TaskService {
    return &TaskService{store: store}
}

func (s *TaskService) List(filter TaskFilter, page, limit int) ([]*Task, int) {
    all := s.store.List(filter)
    total := len(all)
    
    offset := (page - 1) * limit
    if offset >= total {
        return []*Task{}, total
    }
    end := offset + limit
    if end > total {
        end = total
    }
    
    return all[offset:end], total
}

func (s *TaskService) Get(id string) (*Task, error) {
    t, ok := s.store.Get(id)
    if !ok {
        return nil, errNotFound{resource: "task", id: id}
    }
    return t, nil
}

func (s *TaskService) Create(title, description, priority, assignee, createdBy string, tags []string) *Task {
    if priority == "" {
        priority = "medium"
    }
    
    now := time.Now()
    task := &Task{
        ID:          genID(),
        Title:       title,
        Description: description,
        Status:      "todo",
        Priority:    priority,
        AssigneeID:  assignee,
        Tags:        tags,
        CreatedBy:   createdBy,
        CreatedAt:   now,
        UpdatedAt:   now,
    }
    s.store.Create(task)
    return task
}

func (s *TaskService) Update(id string, title, description, status, priority, assignee *string, tags *[]string) (*Task, error) {
    task, ok := s.store.Get(id)
    if !ok {
        return nil, errNotFound{resource: "task", id: id}
    }
    
    if title != nil { task.Title = *title }
    if description != nil { task.Description = *description }
    if priority != nil { task.Priority = *priority }
    if assignee != nil { task.AssigneeID = *assignee }
    if tags != nil { task.Tags = *tags }
    if status != nil {
        oldStatus := task.Status
        task.Status = *status
        if *status == "done" && oldStatus != "done" {
            now := time.Now()
            task.DoneAt = &now
        }
        if *status != "done" {
            task.DoneAt = nil
        }
    }
    task.UpdatedAt = time.Now()
    
    s.store.Update(task)
    return task, nil
}

func (s *TaskService) Delete(id string, userRole string) error {
    _, ok := s.store.Get(id)
    if !ok {
        return errNotFound{resource: "task", id: id}
    }
    if userRole != "admin" {
        return errForbidden{message: "only admins can delete tasks"}
    }
    s.store.Delete(id)
    return nil
}

func (s *TaskService) Stats() map[string]interface{} {
    all := s.store.List(TaskFilter{})
    
    byStatus := map[string]int{"todo": 0, "in_progress": 0, "done": 0}
    byPriority := map[string]int{"low": 0, "medium": 0, "high": 0}
    
    for _, t := range all {
        byStatus[t.Status]++
        byPriority[t.Priority]++
    }
    
    return map[string]interface{}{
        "total":       len(all),
        "by_status":   byStatus,
        "by_priority": byPriority,
    }
}

// ===== API Layer =====

// -- Error types --

type apiError struct {
    Status  int          `json:"-"`
    Code    string       `json:"code"`
    Message string       `json:"message"`
    Details []fieldError `json:"details,omitempty"`
}

type fieldError struct {
    Field   string `json:"field"`
    Message string `json:"message"`
}

func (e *apiError) Error() string { return e.Message }

func respondJSON(w http.ResponseWriter, status int, data interface{}) {
    w.Header().Set("Content-Type", "application/json")
    w.WriteHeader(status)
    if data != nil {
        json.NewEncoder(w).Encode(data)
    }
}

func respondError(w http.ResponseWriter, err *apiError) {
    respondJSON(w, err.Status, err)
}

func handleDomainError(w http.ResponseWriter, err error) {
    var nf errNotFound
    var cf errConflict
    var fb errForbidden
    
    switch {
    case errors.As(err, &nf):
        respondError(w, &apiError{Status: 404, Code: "not_found", Message: nf.Error()})
    case errors.As(err, &cf):
        respondError(w, &apiError{Status: 409, Code: "conflict", Message: cf.Error()})
    case errors.As(err, &fb):
        respondError(w, &apiError{Status: 403, Code: "forbidden", Message: fb.Error()})
    default:
        slog.Error("unexpected error", "error", err)
        respondError(w, &apiError{Status: 500, Code: "internal_error", Message: "internal error"})
    }
}

// -- Decode & Validate --

type validatable interface {
    Validate() []fieldError
}

func decodeAndValidate[T validatable](r *http.Request) (T, *apiError) {
    var v T
    r.Body = http.MaxBytesReader(nil, r.Body, 1<<20)
    dec := json.NewDecoder(r.Body)
    dec.DisallowUnknownFields()
    
    if err := dec.Decode(&v); err != nil {
        if errors.Is(err, io.EOF) {
            return v, &apiError{Status: 400, Code: "bad_request", Message: "empty body"}
        }
        return v, &apiError{Status: 400, Code: "bad_request", Message: "invalid JSON: " + err.Error()}
    }
    
    if errs := v.Validate(); len(errs) > 0 {
        return v, &apiError{Status: 422, Code: "validation_error", Message: "validation failed", Details: errs}
    }
    
    return v, nil
}

// -- Input types --

type createTaskInput struct {
    Title       string   `json:"title"`
    Description string   `json:"description"`
    Priority    string   `json:"priority"`
    AssigneeID  string   `json:"assignee_id"`
    Tags        []string `json:"tags"`
}

func (i createTaskInput) Validate() []fieldError {
    var errs []fieldError
    if strings.TrimSpace(i.Title) == "" {
        errs = append(errs, fieldError{"title", "is required"})
    }
    if i.Priority != "" && i.Priority != "low" && i.Priority != "medium" && i.Priority != "high" {
        errs = append(errs, fieldError{"priority", "must be low, medium, or high"})
    }
    return errs
}

type updateTaskInput struct {
    Title       *string   `json:"title"`
    Description *string   `json:"description"`
    Status      *string   `json:"status"`
    Priority    *string   `json:"priority"`
    AssigneeID  *string   `json:"assignee_id"`
    Tags        *[]string `json:"tags"`
}

func (i updateTaskInput) Validate() []fieldError {
    var errs []fieldError
    if i.Title != nil && strings.TrimSpace(*i.Title) == "" {
        errs = append(errs, fieldError{"title", "cannot be empty"})
    }
    validStatuses := map[string]bool{"todo": true, "in_progress": true, "done": true}
    if i.Status != nil && !validStatuses[*i.Status] {
        errs = append(errs, fieldError{"status", "must be todo, in_progress, or done"})
    }
    validPriorities := map[string]bool{"low": true, "medium": true, "high": true}
    if i.Priority != nil && !validPriorities[*i.Priority] {
        errs = append(errs, fieldError{"priority", "must be low, medium, or high"})
    }
    return errs
}

// -- Pagination --

type paginatedResponse struct {
    Data       interface{} `json:"data"`
    Pagination pagination  `json:"pagination"`
}

type pagination struct {
    Page       int  `json:"page"`
    Limit      int  `json:"limit"`
    Total      int  `json:"total"`
    TotalPages int  `json:"total_pages"`
    HasNext    bool `json:"has_next"`
    HasPrev    bool `json:"has_prev"`
}

// -- Middleware --

type ctxKey int
const userCtxKey ctxKey = 0

func getUser(r *http.Request) *User {
    u, _ := r.Context().Value(userCtxKey).(*User)
    return u
}

func authMiddleware(auth *AuthStore) func(http.Handler) http.Handler {
    return func(next http.Handler) http.Handler {
        return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
            header := r.Header.Get("Authorization")
            if header == "" || !strings.HasPrefix(header, "Bearer ") {
                respondError(w, &apiError{Status: 401, Code: "unauthorized", Message: "missing or invalid Authorization header"})
                return
            }
            token := strings.TrimPrefix(header, "Bearer ")
            user, ok := auth.GetByToken(token)
            if !ok {
                respondError(w, &apiError{Status: 401, Code: "unauthorized", Message: "invalid token"})
                return
            }
            ctx := context.WithValue(r.Context(), userCtxKey, user)
            next.ServeHTTP(w, r.WithContext(ctx))
        })
    }
}

func loggingMiddleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        start := time.Now()
        rec := &statusWriter{ResponseWriter: w, status: 200}
        next.ServeHTTP(rec, r)
        slog.Info("request", "method", r.Method, "path", r.URL.RequestURI(),
            "status", rec.status, "duration", time.Since(start).String(), "remote", r.RemoteAddr)
    })
}

type statusWriter struct {
    http.ResponseWriter
    status  int
    written bool
}

func (w *statusWriter) WriteHeader(code int) {
    if !w.written { w.status = code; w.written = true }
    w.ResponseWriter.WriteHeader(code)
}

func recoveryMiddleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        defer func() {
            if err := recover(); err != nil {
                slog.Error("panic", "error", err, "stack", string(debug.Stack()))
                respondError(w, &apiError{Status: 500, Code: "internal_error", Message: "internal error"})
            }
        }()
        next.ServeHTTP(w, r)
    })
}

func corsMiddleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        w.Header().Set("Access-Control-Allow-Origin", "*")
        w.Header().Set("Access-Control-Allow-Methods", "GET, POST, PATCH, DELETE, OPTIONS")
        w.Header().Set("Access-Control-Allow-Headers", "Content-Type, Authorization")
        if r.Method == "OPTIONS" { w.WriteHeader(204); return }
        next.ServeHTTP(w, r)
    })
}

// -- Handlers --

type TaskHandler struct {
    svc *TaskService
}

func (h *TaskHandler) List(w http.ResponseWriter, r *http.Request) {
    q := r.URL.Query()
    page, _ := strconv.Atoi(q.Get("page"))
    if page < 1 { page = 1 }
    limit, _ := strconv.Atoi(q.Get("limit"))
    if limit < 1 || limit > 100 { limit = 20 }
    
    filter := TaskFilter{
        Status:     q.Get("status"),
        Priority:   q.Get("priority"),
        AssigneeID: q.Get("assignee"),
        Tag:        q.Get("tag"),
        Search:     q.Get("search"),
        SortBy:     q.Get("sort"),
        Order:      q.Get("order"),
    }
    if filter.Order != "asc" { filter.Order = "desc" }
    
    tasks, total := h.svc.List(filter, page, limit)
    if tasks == nil { tasks = []*Task{} }
    
    totalPages := (total + limit - 1) / limit
    respondJSON(w, 200, paginatedResponse{
        Data: tasks,
        Pagination: pagination{
            Page: page, Limit: limit, Total: total,
            TotalPages: totalPages,
            HasNext: page < totalPages, HasPrev: page > 1,
        },
    })
}

func (h *TaskHandler) Get(w http.ResponseWriter, r *http.Request) {
    task, err := h.svc.Get(r.PathValue("id"))
    if err != nil { handleDomainError(w, err); return }
    respondJSON(w, 200, task)
}

func (h *TaskHandler) Create(w http.ResponseWriter, r *http.Request) {
    input, apiErr := decodeAndValidate[createTaskInput](r)
    if apiErr != nil { respondError(w, apiErr); return }
    
    user := getUser(r)
    task := h.svc.Create(input.Title, input.Description, input.Priority, input.AssigneeID, user.ID, input.Tags)
    
    w.Header().Set("Location", "/api/v1/tasks/"+task.ID)
    respondJSON(w, 201, task)
}

func (h *TaskHandler) Update(w http.ResponseWriter, r *http.Request) {
    input, apiErr := decodeAndValidate[updateTaskInput](r)
    if apiErr != nil { respondError(w, apiErr); return }
    
    task, err := h.svc.Update(r.PathValue("id"), input.Title, input.Description, input.Status, input.Priority, input.AssigneeID, input.Tags)
    if err != nil { handleDomainError(w, err); return }
    respondJSON(w, 200, task)
}

func (h *TaskHandler) Delete(w http.ResponseWriter, r *http.Request) {
    user := getUser(r)
    if err := h.svc.Delete(r.PathValue("id"), user.Role); err != nil {
        handleDomainError(w, err)
        return
    }
    w.WriteHeader(204)
}

func (h *TaskHandler) Stats(w http.ResponseWriter, r *http.Request) {
    respondJSON(w, 200, h.svc.Stats())
}

// ===== Utilities =====

func genID() string {
    b := make([]byte, 8)
    rand.Read(b)
    return fmt.Sprintf("%x", b)
}

// ===== Main =====

func main() {
    slog.SetDefault(slog.New(slog.NewJSONHandler(os.Stdout, nil)))
    
    // Stores
    taskStore := NewTaskStore()
    authStore := NewAuthStore()
    
    // Create demo users (returns token)
    admin, adminToken := authStore.Register("admin", "admin")
    user1, user1Token := authStore.Register("alice", "user")
    _, _ = authStore.Register("bob", "user")
    
    // Service
    taskSvc := NewTaskService(taskStore)
    
    // Seed data
    taskSvc.Create("Setup CI/CD pipeline", "Configure GitHub Actions", "high", admin.ID, admin.ID, []string{"devops", "infra"})
    taskSvc.Create("Write API docs", "Document all endpoints", "medium", user1.ID, admin.ID, []string{"docs"})
    taskSvc.Create("Fix login bug", "Users getting 500 on login", "high", "", user1.ID, []string{"bug", "auth"})
    taskSvc.Create("Add dark mode", "User requested feature", "low", "", user1.ID, []string{"ui", "feature"})
    
    // Handler
    th := &TaskHandler{svc: taskSvc}
    
    // Router
    mux := http.NewServeMux()
    
    // Public routes
    mux.HandleFunc("GET /health", func(w http.ResponseWriter, r *http.Request) {
        respondJSON(w, 200, map[string]string{"status": "ok"})
    })
    mux.HandleFunc("POST /auth/register", func(w http.ResponseWriter, r *http.Request) {
        var input struct{ Username string `json:"username"` }
        json.NewDecoder(r.Body).Decode(&input)
        if input.Username == "" {
            respondError(w, &apiError{Status: 400, Code: "bad_request", Message: "username required"})
            return
        }
        user, token := authStore.Register(input.Username, "user")
        respondJSON(w, 201, map[string]interface{}{
            "user": map[string]string{"id": user.ID, "username": user.Username, "role": user.Role},
            "token": token,
        })
    })
    
    // Protected routes
    auth := authMiddleware(authStore)
    mux.Handle("GET /api/v1/tasks", auth(http.HandlerFunc(th.List)))
    mux.Handle("POST /api/v1/tasks", auth(http.HandlerFunc(th.Create)))
    mux.Handle("GET /api/v1/tasks/{id}", auth(http.HandlerFunc(th.Get)))
    mux.Handle("PATCH /api/v1/tasks/{id}", auth(http.HandlerFunc(th.Update)))
    mux.Handle("DELETE /api/v1/tasks/{id}", auth(http.HandlerFunc(th.Delete)))
    mux.Handle("GET /api/v1/stats", auth(http.HandlerFunc(th.Stats)))
    
    // Global middleware
    handler := recoveryMiddleware(loggingMiddleware(corsMiddleware(mux)))
    
    // Server
    addr := ":8080"
    if p := os.Getenv("PORT"); p != "" { addr = ":" + p }
    
    server := &http.Server{
        Addr: addr, Handler: handler,
        ReadTimeout: 5 * time.Second, WriteTimeout: 10 * time.Second,
        IdleTimeout: 120 * time.Second,
    }
    
    go func() {
        slog.Info("server starting", "addr", addr)
        slog.Info("demo tokens", "admin", adminToken, "alice", user1Token)
        if err := server.ListenAndServe(); err != http.ErrServerClosed { log.Fatal(err) }
    }()
    
    ctx, stop := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
    defer stop()
    <-ctx.Done()
    
    shutCtx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
    defer cancel()
    server.Shutdown(shutCtx)
    slog.Info("server stopped")
}
```

```
PROBAR EL PROGRAMA:

go run main.go
# Los tokens demo aparecen en el log de inicio

# Guardar token admin (copiarlo del log)
TOKEN="<admin-token-del-log>"

# Health (publico)
curl localhost:8080/health

# Registrar nuevo usuario
curl -X POST localhost:8080/auth/register \
  -H "Content-Type: application/json" \
  -d '{"username":"charlie"}' | jq

# Listar tareas
curl -H "Authorization: Bearer $TOKEN" localhost:8080/api/v1/tasks | jq

# Filtrar por prioridad y status
curl -H "Authorization: Bearer $TOKEN" \
  "localhost:8080/api/v1/tasks?priority=high&status=todo" | jq

# Buscar
curl -H "Authorization: Bearer $TOKEN" \
  "localhost:8080/api/v1/tasks?search=bug" | jq

# Paginacion
curl -H "Authorization: Bearer $TOKEN" \
  "localhost:8080/api/v1/tasks?page=1&limit=2&sort=priority&order=desc" | jq

# Crear tarea
curl -X POST -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"title":"Deploy v2.0","description":"Production deploy","priority":"high","tags":["deploy","prod"]}' \
  localhost:8080/api/v1/tasks | jq

# Actualizar status (PATCH)
TASK_ID="<id-de-la-tarea>"
curl -X PATCH -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"status":"in_progress"}' \
  localhost:8080/api/v1/tasks/$TASK_ID | jq

# Marcar como done (agrega done_at automaticamente)
curl -X PATCH -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"status":"done"}' \
  localhost:8080/api/v1/tasks/$TASK_ID | jq

# Estadisticas
curl -H "Authorization: Bearer $TOKEN" localhost:8080/api/v1/stats | jq

# Eliminar (solo admin)
curl -X DELETE -H "Authorization: Bearer $TOKEN" \
  localhost:8080/api/v1/tasks/$TASK_ID -v
# 204 No Content

# Intentar eliminar como user (403)
USER_TOKEN="<alice-token>"
curl -X DELETE -H "Authorization: Bearer $USER_TOKEN" \
  localhost:8080/api/v1/tasks/$TASK_ID -v
# {"code":"forbidden","message":"only admins can delete tasks"}

# Sin auth (401)
curl localhost:8080/api/v1/tasks -v
# {"code":"unauthorized","message":"missing or invalid Authorization header"}

# Validacion (422)
curl -X POST -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"priority":"ultra"}' \
  localhost:8080/api/v1/tasks | jq
# {"code":"validation_error","details":[{"field":"title","message":"is required"},
#   {"field":"priority","message":"must be low, medium, or high"}]}

CONCEPTOS DEMOSTRADOS:
  Separacion de capas:    Domain → Store → Service → Handler
  Errores de dominio:     errNotFound/errConflict/errForbidden → HTTP status
  REST convenciones:      GET/POST/PATCH/DELETE, status codes, Location header
  Validacion:             Validate() interface, errores por campo
  PATCH con punteros:     *string para campos opcionales
  Paginacion:             offset-based con metadata
  Filtrado/ordenamiento:  query params con whitelist
  Auth middleware:        Bearer token, context con user
  Roles:                  admin vs user (delete solo admin)
  Generics:               decodeAndValidate[T validatable]
  DoneAt automatico:      se setea al cambiar status a "done"
  JSON estructurado:      errores consistentes con code/message/details
```

---

## 13. Ejercicios

### Ejercicio 1: Blog API con relaciones
Crea una API REST de blog con:
- Recursos: Users, Posts, Comments (relaciones: user has many posts, post has many comments)
- CRUD completo para cada recurso con las rutas REST correctas
- `GET /api/v1/users/{id}/posts` — posts de un usuario
- `GET /api/v1/posts/{id}/comments` — comentarios de un post
- Paginacion en todos los list endpoints
- Filtros: posts por tag, por estado (draft/published), por fecha
- Ordenamiento: por fecha, por titulo, por numero de comentarios
- Validacion de inputs con errores detallados
- Solo el autor puede editar/borrar su post (auth + ownership check)
- Separacion de capas: handlers / service / store

### Ejercicio 2: E-commerce API con transacciones
Crea un API para un catalogo con carrito:
- Productos: CRUD con stock, precio, categorias, imagenes (URLs)
- Carrito: agregar item, quitar item, ver carrito, vaciar carrito
- Checkout: `POST /api/v1/cart/checkout` — crear orden, decrementar stock, vaciar carrito
- Stock: `PATCH /api/v1/products/{id}` no debe permitir stock negativo
- Checkout debe ser atomico: si un producto no tiene stock, la orden entera falla (409)
- Idempotency key en checkout (header `Idempotency-Key`)
- Busqueda de productos: texto libre, por categoria, rango de precio, en stock
- Paginacion cursor-based para productos (muchos items)
- Response con HATEOAS links (self, add_to_cart, category)

### Ejercicio 3: API con versionado y migracion
Crea una API con dos versiones:
- V1: `GET /api/v1/users` retorna `{"name": "Alice", "email": "a@b.com"}`
- V2: `GET /api/v2/users` retorna `{"full_name": "Alice", "email_address": "a@b.com", "created_at": "..."}`
- Ambas versiones usan el mismo servicio y store
- Solo los handlers y los response types difieren
- Header `Deprecation: true` en responses de V1
- Header `Sunset: 2025-12-31` en responses de V1
- Un middleware que loggea el uso de V1 para metricas de migracion
- Documentar la diferencia entre las versiones en un endpoint `GET /api/versions`

### Ejercicio 4: API con rate limiting avanzado
Crea una API con rate limiting per-endpoint y per-user:
- Rate limits diferentes por endpoint: `POST /api/v1/messages` = 5/min, `GET /api/v1/messages` = 60/min
- Rate limits diferentes por rol: admin = 2x el limite normal
- Headers de rate limit en cada response: `X-RateLimit-Limit`, `X-RateLimit-Remaining`, `X-RateLimit-Reset`
- Response 429 con body JSON que incluye `retry_after` (segundos)
- Sliding window algorithm (no fixed window)
- Dashboard endpoint: `GET /api/v1/admin/rate-limits` — ver uso actual por IP y por usuario
- Configurable via JSON file que se recarga con SIGHUP

---

> **Siguiente**: C10/S02 - HTTP, T04 - Comparacion con Rust — net/http vs hyper/axum, simplicidad vs rendimiento/seguridad de tipos
