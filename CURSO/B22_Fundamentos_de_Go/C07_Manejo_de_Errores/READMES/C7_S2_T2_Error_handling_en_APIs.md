# Error Handling en APIs — Patrones para HTTP Handlers, Middleware de Errores

## 1. Introduccion

Manejar errores en una API HTTP es fundamentalmente diferente a manejar errores en una libreria o CLI. En una libreria, devuelves `error` al caller y el decide que hacer. En una API HTTP, **tu eres el ultimo caller** — necesitas transformar el error en una respuesta HTTP coherente que el cliente pueda interpretar: status code, body JSON, headers, y todo sin filtrar informacion interna que comprometa la seguridad.

El problema es que la stdlib de Go (`net/http`) no tiene un patron estandar para esto. Un handler tiene la firma `func(w http.ResponseWriter, r *http.Request)` — no devuelve `error`. Si algo falla dentro del handler, debes escribir la respuesta de error manualmente. Esto lleva inevitablemente a un patron repetitivo:

```go
// ⚠️ PATRON REPETITIVO — cada handler repite la misma logica de error
func GetUser(w http.ResponseWriter, r *http.Request) {
    id := r.PathValue("id")
    user, err := db.FindUser(id)
    if err != nil {
        if errors.Is(err, sql.ErrNoRows) {
            w.WriteHeader(http.StatusNotFound)
            json.NewEncoder(w).Encode(map[string]string{"error": "user not found"})
            return
        }
        log.Printf("ERROR: GetUser(%s): %v", id, err)
        w.WriteHeader(http.StatusInternalServerError)
        json.NewEncoder(w).Encode(map[string]string{"error": "internal server error"})
        return
    }
    // ... happy path
}
```

Este capitulo resuelve esa repeticion con patrones idiomaticos de Go: handlers que retornan `error`, middleware de errores, tipos de error API, mapeo error→HTTP, y validacion estructurada.

Para SysAdmin/DevOps, estos patrones son directamente aplicables a:
- APIs de configuracion y deployment
- Endpoints de health check y status
- Webhooks de CI/CD
- APIs de monitoring y alerting
- Cualquier servicio HTTP interno

```
┌──────────────────────────────────────────────────────────────────────────┐
│                  EL FLUJO DE UN ERROR EN UNA API                         │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Request                                                                 │
│  │                                                                       │
│  ├─ Middleware (auth, logging, rate limit)                               │
│  │   └─ Error: 401, 429, etc.                                           │
│  │                                                                       │
│  ├─ Handler                                                              │
│  │   ├─ Validar input                                                   │
│  │   │   └─ Error: 400 Bad Request + detalles de validacion             │
│  │   │                                                                   │
│  │   ├─ Logica de negocio                                               │
│  │   │   ├─ Not found: 404                                              │
│  │   │   ├─ Conflicto: 409                                              │
│  │   │   ├─ Prohibido: 403                                              │
│  │   │   └─ Error interno: 500 (SIN detalles internos)                  │
│  │   │                                                                   │
│  │   └─ Respuesta exitosa: 200/201/204                                  │
│  │                                                                       │
│  Response JSON                                                           │
│  {                                                                       │
│    "error": {                                                            │
│      "code": "NOT_FOUND",                                               │
│      "message": "User not found",                                       │
│      "details": {...}            ← solo en 4xx (errores del cliente)    │
│      "request_id": "abc-123"     ← siempre (para correlacion)          │
│    }                                                                     │
│  }                                                                       │
│                                                                          │
│  REGLA DE ORO:                                                           │
│  - 4xx: el CLIENTE cometio un error → dar detalles para corregirlo      │
│  - 5xx: el SERVIDOR fallo → NO dar detalles (logear internamente)       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 2. El Problema del Handler Estandar

### 2.1 La Firma de net/http

```go
// El handler estandar de Go NO retorna error:
type Handler interface {
    ServeHTTP(ResponseWriter, *Request)
}

// Ni la funcion equivalente:
type HandlerFunc func(ResponseWriter, *Request)

// Esto obliga a manejar errores DENTRO del handler:
func handler(w http.ResponseWriter, r *http.Request) {
    // Si algo falla, debes escribir la respuesta aqui mismo
    // No puedes "propagar" el error como en una funcion normal
}
```

### 2.2 Los Problemas del Patron Manual

```go
// Handler tipico SIN estructura — cada handler repite error handling:
func CreateServer(w http.ResponseWriter, r *http.Request) {
    // 1. Parsear body
    var req CreateServerRequest
    if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
        w.Header().Set("Content-Type", "application/json")
        w.WriteHeader(http.StatusBadRequest)
        json.NewEncoder(w).Encode(map[string]string{
            "error": "invalid JSON: " + err.Error(),
        })
        return
    }

    // 2. Validar
    if req.Name == "" {
        w.Header().Set("Content-Type", "application/json")
        w.WriteHeader(http.StatusBadRequest)
        json.NewEncoder(w).Encode(map[string]string{
            "error": "name is required",
        })
        return
    }

    // 3. Logica de negocio
    server, err := service.CreateServer(r.Context(), req)
    if err != nil {
        if errors.Is(err, ErrDuplicate) {
            w.Header().Set("Content-Type", "application/json")
            w.WriteHeader(http.StatusConflict)
            json.NewEncoder(w).Encode(map[string]string{
                "error": "server with that name already exists",
            })
            return
        }
        // Error interno — NO exponer detalles
        log.Printf("ERROR: CreateServer: %v", err)
        w.Header().Set("Content-Type", "application/json")
        w.WriteHeader(http.StatusInternalServerError)
        json.NewEncoder(w).Encode(map[string]string{
            "error": "internal server error",
        })
        return
    }

    // 4. Respuesta exitosa
    w.Header().Set("Content-Type", "application/json")
    w.WriteHeader(http.StatusCreated)
    json.NewEncoder(w).Encode(server)
}
```

Problemas de este enfoque:

```
┌─────────────────────────────────────────────┬────────────────────────────┐
│  PROBLEMA                                   │  CONSECUENCIA              │
├─────────────────────────────────────────────┼────────────────────────────┤
│  Content-Type repetido en cada rama         │  Facil olvidarlo → text    │
│  json.NewEncoder repetido 4 veces          │  Boilerplate masivo        │
│  Log manual en cada 500                     │  Inconsistente o ausente   │
│  Status code hardcoded                      │  Facil equivocarse         │
│  Formato de error ad-hoc por handler        │  API inconsistente         │
│  No hay request_id para correlacion         │  Debugging en produccion   │
│  No hay metricas automaticas                │  Sin visibilidad de errors │
│  Si olvidas return despues de WriteHeader   │  Doble write → panic       │
└─────────────────────────────────────────────┴────────────────────────────┘
```

---

## 3. Patron AppHandler — Handlers que Retornan Error

### 3.1 El Concepto

La solucion idiomatica en Go es crear un tipo de handler **que retorna error** y un adaptador que lo convierte al `http.HandlerFunc` estandar:

```go
// AppHandler es un handler que puede retornar error
type AppHandler func(w http.ResponseWriter, r *http.Request) error

// ServeHTTP adapta AppHandler a http.Handler
func (fn AppHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
    if err := fn(w, r); err != nil {
        handleError(w, r, err) // UN solo punto de manejo de errores
    }
}
```

### 3.2 Implementacion Completa

```go
package api

import (
    "encoding/json"
    "errors"
    "fmt"
    "log/slog"
    "net/http"
    "time"
)

// ─────────────────────────────────────────────────────────────────
// Tipo de error API
// ─────────────────────────────────────────────────────────────────

// APIError es un error que sabe como representarse en HTTP
type APIError struct {
    // Campos para la respuesta HTTP
    Status  int    `json:"-"`              // HTTP status code (no se serializa)
    Code    string `json:"code"`           // codigo maquina-legible: "NOT_FOUND"
    Message string `json:"message"`        // mensaje humano-legible
    // Campos opcionales
    Details any    `json:"details,omitempty"` // detalles adicionales (validacion, etc.)
    // Campos internos (no se serializan al cliente)
    Internal error  `json:"-"`             // error original (para logging)
}

func (e *APIError) Error() string {
    if e.Internal != nil {
        return fmt.Sprintf("%s: %s: %v", e.Code, e.Message, e.Internal)
    }
    return fmt.Sprintf("%s: %s", e.Code, e.Message)
}

func (e *APIError) Unwrap() error {
    return e.Internal
}

// ─────────────────────────────────────────────────────────────────
// Constructores de errores API comunes
// ─────────────────────────────────────────────────────────────────

func ErrBadRequest(msg string, details ...any) *APIError {
    e := &APIError{
        Status:  http.StatusBadRequest,
        Code:    "BAD_REQUEST",
        Message: msg,
    }
    if len(details) > 0 {
        e.Details = details[0]
    }
    return e
}

func ErrNotFound(resource string, id string) *APIError {
    return &APIError{
        Status:  http.StatusNotFound,
        Code:    "NOT_FOUND",
        Message: fmt.Sprintf("%s '%s' not found", resource, id),
    }
}

func ErrConflict(msg string) *APIError {
    return &APIError{
        Status:  http.StatusConflict,
        Code:    "CONFLICT",
        Message: msg,
    }
}

func ErrUnauthorized(msg string) *APIError {
    return &APIError{
        Status:  http.StatusUnauthorized,
        Code:    "UNAUTHORIZED",
        Message: msg,
    }
}

func ErrForbidden(msg string) *APIError {
    return &APIError{
        Status:  http.StatusForbidden,
        Code:    "FORBIDDEN",
        Message: msg,
    }
}

func ErrUnprocessable(msg string, details any) *APIError {
    return &APIError{
        Status:  http.StatusUnprocessableEntity,
        Code:    "VALIDATION_ERROR",
        Message: msg,
        Details: details,
    }
}

func ErrInternal(err error) *APIError {
    return &APIError{
        Status:   http.StatusInternalServerError,
        Code:     "INTERNAL_ERROR",
        Message:  "an internal error occurred",
        Internal: err, // se logea, NO se envia al cliente
    }
}

func ErrServiceUnavailable(msg string) *APIError {
    return &APIError{
        Status:  http.StatusServiceUnavailable,
        Code:    "SERVICE_UNAVAILABLE",
        Message: msg,
    }
}

func ErrTooManyRequests(retryAfter time.Duration) *APIError {
    return &APIError{
        Status:  http.StatusTooManyRequests,
        Code:    "RATE_LIMITED",
        Message: fmt.Sprintf("rate limit exceeded, retry after %v", retryAfter),
        Details: map[string]any{"retry_after_seconds": retryAfter.Seconds()},
    }
}
```

### 3.3 Respuesta de Error Estandarizada

```go
// ErrorResponse es la estructura JSON que SIEMPRE se envia al cliente
type ErrorResponse struct {
    Error ErrorBody `json:"error"`
}

type ErrorBody struct {
    Code      string `json:"code"`
    Message   string `json:"message"`
    Details   any    `json:"details,omitempty"`
    RequestID string `json:"request_id,omitempty"`
}
```

### 3.4 El Adaptador

```go
// AppHandler es un handler HTTP que puede retornar error
type AppHandler func(w http.ResponseWriter, r *http.Request) error

// ServeHTTP implementa http.Handler — convierte el error en respuesta HTTP
func (fn AppHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
    if err := fn(w, r); err != nil {
        writeError(w, r, err)
    }
}

// writeError convierte cualquier error en una respuesta HTTP JSON
func writeError(w http.ResponseWriter, r *http.Request, err error) {
    // Extraer request_id del contexto (si existe)
    requestID, _ := r.Context().Value(ctxKeyRequestID).(string)

    // Determinar el APIError
    var apiErr *APIError
    if !errors.As(err, &apiErr) {
        // Error desconocido → 500 Internal Server Error
        apiErr = ErrInternal(err)
    }

    // Logear el error (SIEMPRE)
    logger := slog.With(
        "request_id", requestID,
        "method", r.Method,
        "path", r.URL.Path,
        "status", apiErr.Status,
        "code", apiErr.Code,
    )

    if apiErr.Status >= 500 {
        // Errores de servidor: logear con stack trace / error interno
        logger.Error("server error",
            "error", apiErr.Error(),
            "internal", apiErr.Internal,
        )
    } else {
        // Errores de cliente: logear como warning
        logger.Warn("client error", "error", apiErr.Message)
    }

    // Escribir respuesta HTTP
    w.Header().Set("Content-Type", "application/json; charset=utf-8")
    w.Header().Set("X-Content-Type-Options", "nosniff")
    w.WriteHeader(apiErr.Status)

    resp := ErrorResponse{
        Error: ErrorBody{
            Code:      apiErr.Code,
            Message:   apiErr.Message,
            RequestID: requestID,
        },
    }

    // Solo incluir detalles en errores 4xx (errores del cliente)
    if apiErr.Status < 500 && apiErr.Details != nil {
        resp.Error.Details = apiErr.Details
    }

    json.NewEncoder(w).Encode(resp)
}
```

### 3.5 Handler Limpio con AppHandler

Ahora el handler se ve completamente diferente:

```go
// ANTES: 40 lineas con error handling repetitivo
// DESPUES: handler limpio que solo retorna errores

func CreateServer(svc *ServerService) AppHandler {
    return func(w http.ResponseWriter, r *http.Request) error {
        // 1. Parsear body
        var req CreateServerRequest
        if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
            return ErrBadRequest("invalid JSON body")
        }

        // 2. Validar
        if err := req.Validate(); err != nil {
            return err // Validate retorna *APIError con detalles
        }

        // 3. Logica de negocio
        server, err := svc.Create(r.Context(), req)
        if err != nil {
            return mapServiceError(err) // mapear error de servicio a API
        }

        // 4. Respuesta exitosa
        w.Header().Set("Content-Type", "application/json")
        w.WriteHeader(http.StatusCreated)
        return json.NewEncoder(w).Encode(server)
    }
}

func GetServer(svc *ServerService) AppHandler {
    return func(w http.ResponseWriter, r *http.Request) error {
        id := r.PathValue("id")
        server, err := svc.Get(r.Context(), id)
        if err != nil {
            return mapServiceError(err)
        }

        w.Header().Set("Content-Type", "application/json")
        return json.NewEncoder(w).Encode(server)
    }
}

func DeleteServer(svc *ServerService) AppHandler {
    return func(w http.ResponseWriter, r *http.Request) error {
        id := r.PathValue("id")
        if err := svc.Delete(r.Context(), id); err != nil {
            return mapServiceError(err)
        }
        w.WriteHeader(http.StatusNoContent)
        return nil
    }
}
```

### 3.6 Registrar Rutas

```go
mux := http.NewServeMux()

svc := NewServerService(db)

// AppHandler implementa http.Handler — se registra directamente
mux.Handle("POST /api/v1/servers", CreateServer(svc))
mux.Handle("GET /api/v1/servers/{id}", GetServer(svc))
mux.Handle("DELETE /api/v1/servers/{id}", DeleteServer(svc))
```

---

## 4. Mapeo Error de Dominio a Error HTTP

### 4.1 El Problema del Mapeo

La capa de servicio/dominio no debe saber nada de HTTP. Produce errores de dominio (`ErrNotFound`, `ErrDuplicate`, `ErrInvalidState`). La capa API debe traducirlos a status codes HTTP:

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    CAPAS Y RESPONSABILIDAD DE ERRORES                    │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────┐   ┌──────────────────┐   ┌────────────────────┐        │
│  │  HTTP Layer  │   │  Service Layer   │   │  Repository Layer  │        │
│  │  (handlers)  │←──│  (business logic)│←──│  (database)        │        │
│  └─────────────┘   └──────────────────┘   └────────────────────┘        │
│                                                                          │
│  Conoce HTTP        No conoce HTTP         No conoce HTTP               │
│  Mapea errores      Produce errores        Produce errores              │
│  a status codes     de dominio             de DB                        │
│                                                                          │
│  APIError           ErrNotFound            sql.ErrNoRows                │
│  400, 404, 500      ErrDuplicate           *pq.Error (unique)           │
│                     ErrForbidden           context.DeadlineExceeded     │
│                                                                          │
│  FLUJO DE ERROR:                                                        │
│  sql.ErrNoRows → service wraps → ErrNotFound → handler maps → 404     │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Sentinels de Dominio

```go
// package domain (o service)

import "errors"

// Errores de dominio — NO saben de HTTP
var (
    ErrNotFound      = errors.New("not found")
    ErrAlreadyExists = errors.New("already exists")
    ErrForbidden     = errors.New("forbidden")
    ErrInvalidInput  = errors.New("invalid input")
    ErrConflict      = errors.New("conflict")
    ErrExpired       = errors.New("expired")
    ErrRateLimited   = errors.New("rate limited")
)

// Error de dominio con contexto
type DomainError struct {
    Resource string // "server", "user", "deployment"
    ID       string // identificador del recurso
    Op       string // operacion que fallo
    Kind     error  // sentinel (ErrNotFound, etc.)
    Err      error  // error subyacente
}

func (e *DomainError) Error() string {
    return fmt.Sprintf("%s %s(%s): %v", e.Op, e.Resource, e.ID, e.Err)
}

func (e *DomainError) Unwrap() error { return e.Err }

func (e *DomainError) Is(target error) bool {
    return errors.Is(e.Kind, target)
}
```

### 4.3 Servicio que Produce Errores de Dominio

```go
// package service

type ServerService struct {
    repo ServerRepository
}

func (s *ServerService) Get(ctx context.Context, id string) (*Server, error) {
    server, err := s.repo.FindByID(ctx, id)
    if err != nil {
        if errors.Is(err, sql.ErrNoRows) {
            return nil, &DomainError{
                Resource: "server",
                ID:       id,
                Op:       "get",
                Kind:     ErrNotFound,
                Err:      fmt.Errorf("server not found: %w", err),
            }
        }
        return nil, fmt.Errorf("get server %s: %w", id, err) // error opaco → 500
    }
    return server, nil
}

func (s *ServerService) Create(ctx context.Context, req CreateServerRequest) (*Server, error) {
    existing, err := s.repo.FindByName(ctx, req.Name)
    if err != nil && !errors.Is(err, sql.ErrNoRows) {
        return nil, fmt.Errorf("check existing: %w", err)
    }
    if existing != nil {
        return nil, &DomainError{
            Resource: "server",
            ID:       req.Name,
            Op:       "create",
            Kind:     ErrAlreadyExists,
            Err:      fmt.Errorf("server '%s' already exists", req.Name),
        }
    }

    server, err := s.repo.Insert(ctx, req.toServer())
    if err != nil {
        return nil, fmt.Errorf("insert server: %w", err)
    }
    return server, nil
}
```

### 4.4 Funcion de Mapeo Error→HTTP

```go
// package api — el UNICO lugar que conoce HTTP

// mapServiceError traduce errores de dominio a errores API
func mapServiceError(err error) *APIError {
    if err == nil {
        return nil
    }

    // Extraer DomainError si existe
    var domErr *DomainError
    hasDomainErr := errors.As(err, &domErr)

    switch {
    case errors.Is(err, ErrNotFound):
        resource, id := "resource", "unknown"
        if hasDomainErr {
            resource, id = domErr.Resource, domErr.ID
        }
        return ErrNotFound(resource, id)

    case errors.Is(err, ErrAlreadyExists):
        msg := "resource already exists"
        if hasDomainErr {
            msg = fmt.Sprintf("%s '%s' already exists", domErr.Resource, domErr.ID)
        }
        return ErrConflict(msg)

    case errors.Is(err, ErrForbidden):
        return ErrForbidden("you do not have permission to perform this action")

    case errors.Is(err, ErrInvalidInput):
        return ErrBadRequest(err.Error())

    case errors.Is(err, ErrRateLimited):
        return ErrTooManyRequests(60 * time.Second)

    case errors.Is(err, context.DeadlineExceeded):
        return ErrServiceUnavailable("request timed out")

    case errors.Is(err, context.Canceled):
        // Cliente desconecto — logear pero no enviar respuesta
        return &APIError{
            Status:   499, // Convencion de nginx para "client closed"
            Code:     "CLIENT_CLOSED",
            Message:  "client disconnected",
            Internal: err,
        }

    default:
        // Error no reconocido → 500
        return ErrInternal(err)
    }
}
```

### 4.5 Tabla de Mapeo Estandar

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    MAPEO ERROR DE DOMINIO → HTTP                         │
├────────────────────────┬────────┬────────────────────┬──────────────────┤
│  Error de Dominio      │ Status │ Code API           │ Detalles al      │
│                        │        │                    │ cliente          │
├────────────────────────┼────────┼────────────────────┼──────────────────┤
│  ErrNotFound           │ 404    │ NOT_FOUND          │ SI (recurso, id) │
│  ErrAlreadyExists      │ 409    │ CONFLICT           │ SI               │
│  ErrConflict           │ 409    │ CONFLICT           │ SI               │
│  ErrInvalidInput       │ 400    │ BAD_REQUEST        │ SI (campos)      │
│  ErrForbidden          │ 403    │ FORBIDDEN          │ SI (permiso)     │
│  ErrExpired            │ 410    │ GONE               │ SI               │
│  ErrRateLimited        │ 429    │ RATE_LIMITED       │ SI (retry_after) │
│  ValidationError       │ 422    │ VALIDATION_ERROR   │ SI (campos)      │
│  DeadlineExceeded      │ 503    │ SERVICE_UNAVAIL    │ NO               │
│  context.Canceled      │ 499    │ CLIENT_CLOSED      │ NO               │
│  (cualquier otro)      │ 500    │ INTERNAL_ERROR     │ NO (logear)      │
├────────────────────────┼────────┼────────────────────┼──────────────────┤
│                                                                          │
│  REGLA: 4xx → detalles al cliente (su culpa, necesita saber que)        │
│         5xx → NO detalles (nuestra culpa, no exponer internos)          │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Middleware de Errores

### 5.1 Middleware Chain

Los middlewares en Go son funciones que wrappean un handler para anadir comportamiento transversal: logging, autenticacion, metricas, recovery, etc. Para error handling, los middlewares se componen como una cadena:

```
┌──────────────────────────────────────────────────────────────────────────┐
│                       MIDDLEWARE CHAIN                                    │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Request → Recovery → RequestID → Logging → Auth → Handler → Response  │
│                                                                          │
│  Cada middleware:                                                        │
│  func middleware(next http.Handler) http.Handler {                       │
│      return http.HandlerFunc(func(w, r) {                               │
│          // antes del handler                                            │
│          next.ServeHTTP(w, r)                                           │
│          // despues del handler                                          │
│      })                                                                  │
│  }                                                                       │
│                                                                          │
│  Orden IMPORTA:                                                          │
│  - Recovery debe ser el MAS EXTERNO (atrapar panics de cualquier capa)  │
│  - RequestID antes de Logging (para que el logger tenga el ID)          │
│  - Auth antes del Handler (rechazar no autorizados temprano)            │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 5.2 Middleware: Recovery (Panic → 500)

```go
// Recovery convierte panics en errores 500 JSON en lugar de crash
func Recovery(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        defer func() {
            if rec := recover(); rec != nil {
                // Obtener stack trace
                stack := debug.Stack()

                // Logear el panic con todos los detalles
                requestID, _ := r.Context().Value(ctxKeyRequestID).(string)
                slog.Error("panic recovered",
                    "request_id", requestID,
                    "method", r.Method,
                    "path", r.URL.Path,
                    "panic", rec,
                    "stack", string(stack),
                )

                // Respuesta al cliente (SIN detalles del panic)
                w.Header().Set("Content-Type", "application/json; charset=utf-8")
                w.Header().Set("X-Content-Type-Options", "nosniff")
                w.WriteHeader(http.StatusInternalServerError)
                json.NewEncoder(w).Encode(ErrorResponse{
                    Error: ErrorBody{
                        Code:      "INTERNAL_ERROR",
                        Message:   "an unexpected error occurred",
                        RequestID: requestID,
                    },
                })
            }
        }()
        next.ServeHTTP(w, r)
    })
}
```

### 5.3 Middleware: Request ID

```go
type contextKey string

const ctxKeyRequestID contextKey = "request_id"

// RequestID genera o propaga un ID unico por request
func RequestID(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        // Usar el header si viene del cliente/load balancer
        id := r.Header.Get("X-Request-ID")
        if id == "" {
            id = generateID() // UUID v4 o similar
        }

        // Guardar en el contexto
        ctx := context.WithValue(r.Context(), ctxKeyRequestID, id)

        // Propagar en la respuesta
        w.Header().Set("X-Request-ID", id)

        next.ServeHTTP(w, r.WithContext(ctx))
    })
}

func generateID() string {
    b := make([]byte, 16)
    rand.Read(b)
    return fmt.Sprintf("%x-%x-%x-%x-%x", b[0:4], b[4:6], b[6:8], b[8:10], b[10:])
}

// Extraer el request ID en cualquier parte del codigo:
func GetRequestID(ctx context.Context) string {
    id, _ := ctx.Value(ctxKeyRequestID).(string)
    return id
}
```

### 5.4 Middleware: Structured Logging

```go
// responseRecorder captura el status code para logging
type responseRecorder struct {
    http.ResponseWriter
    statusCode int
    written    int64
}

func (rr *responseRecorder) WriteHeader(code int) {
    rr.statusCode = code
    rr.ResponseWriter.WriteHeader(code)
}

func (rr *responseRecorder) Write(b []byte) (int, error) {
    n, err := rr.ResponseWriter.Write(b)
    rr.written += int64(n)
    return n, err
}

// Logging registra cada request con latencia, status, etc.
func Logging(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        start := time.Now()
        rec := &responseRecorder{ResponseWriter: w, statusCode: http.StatusOK}

        next.ServeHTTP(rec, r)

        duration := time.Since(start)
        requestID := GetRequestID(r.Context())

        // Elegir nivel de log segun status code
        attrs := []any{
            "request_id", requestID,
            "method", r.Method,
            "path", r.URL.Path,
            "status", rec.statusCode,
            "duration_ms", duration.Milliseconds(),
            "bytes", rec.written,
            "remote_addr", r.RemoteAddr,
            "user_agent", r.UserAgent(),
        }

        switch {
        case rec.statusCode >= 500:
            slog.Error("request completed", attrs...)
        case rec.statusCode >= 400:
            slog.Warn("request completed", attrs...)
        default:
            slog.Info("request completed", attrs...)
        }
    })
}
```

### 5.5 Middleware: Timeout

```go
// Timeout cancela el contexto si el handler tarda demasiado
func Timeout(d time.Duration) func(http.Handler) http.Handler {
    return func(next http.Handler) http.Handler {
        return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
            ctx, cancel := context.WithTimeout(r.Context(), d)
            defer cancel()
            next.ServeHTTP(w, r.WithContext(ctx))
        })
    }
}
```

### 5.6 Componer Middlewares

```go
// Chain aplica middlewares en orden (el primero es el mas externo)
func Chain(handler http.Handler, middlewares ...func(http.Handler) http.Handler) http.Handler {
    // Aplicar en orden inverso para que el primero sea el mas externo
    for i := len(middlewares) - 1; i >= 0; i-- {
        handler = middlewares[i](handler)
    }
    return handler
}

// Uso:
mux := http.NewServeMux()
mux.Handle("POST /api/v1/servers", CreateServer(svc))
mux.Handle("GET /api/v1/servers/{id}", GetServer(svc))

// Aplicar middlewares a TODAS las rutas
handler := Chain(mux,
    Recovery,           // 1. Atrapar panics (mas externo)
    RequestID,          // 2. Generar request ID
    Logging,            // 3. Logear request/response
    Timeout(30*time.Second), // 4. Timeout global
)

http.ListenAndServe(":8080", handler)
```

---

## 6. Validacion Estructurada

### 6.1 El Problema de la Validacion Ad-Hoc

```go
// ⚠️ Validacion manual — inconsistente y dificil de mantener
func (r CreateServerRequest) Validate() error {
    if r.Name == "" {
        return ErrBadRequest("name is required")
    }
    if len(r.Name) > 63 {
        return ErrBadRequest("name too long")
    }
    if r.CPU < 1 {
        return ErrBadRequest("cpu must be >= 1")
    }
    if r.Memory < 128 {
        return ErrBadRequest("memory must be >= 128MB")
    }
    // Solo retorna el PRIMER error — el cliente tiene que corregir uno a uno
    return nil
}
```

### 6.2 Validacion con Recoleccion de Errores

```go
// ValidationError contiene TODOS los errores de validacion
type ValidationError struct {
    Fields []FieldError `json:"fields"`
}

type FieldError struct {
    Field   string `json:"field"`
    Message string `json:"message"`
    Value   any    `json:"value,omitempty"` // el valor rechazado (si no es sensible)
}

func (e *ValidationError) Error() string {
    msgs := make([]string, len(e.Fields))
    for i, f := range e.Fields {
        msgs[i] = fmt.Sprintf("%s: %s", f.Field, f.Message)
    }
    return "validation failed: " + strings.Join(msgs, "; ")
}

func (e *ValidationError) HasErrors() bool {
    return len(e.Fields) > 0
}

// ToAPIError convierte ValidationError en APIError
func (e *ValidationError) ToAPIError() *APIError {
    return ErrUnprocessable("validation failed", e.Fields)
}

// Validator acumula errores de validacion
type Validator struct {
    errors []FieldError
}

func (v *Validator) Check(ok bool, field, message string) {
    if !ok {
        v.errors = append(v.errors, FieldError{
            Field:   field,
            Message: message,
        })
    }
}

func (v *Validator) CheckValue(ok bool, field, message string, value any) {
    if !ok {
        v.errors = append(v.errors, FieldError{
            Field:   field,
            Message: message,
            Value:   value,
        })
    }
}

func (v *Validator) Valid() error {
    if len(v.errors) == 0 {
        return nil
    }
    ve := &ValidationError{Fields: v.errors}
    return ve.ToAPIError()
}
```

### 6.3 Uso del Validator

```go
type CreateServerRequest struct {
    Name     string   `json:"name"`
    CPU      int      `json:"cpu"`
    MemoryMB int      `json:"memory_mb"`
    Region   string   `json:"region"`
    Tags     []string `json:"tags"`
}

var validRegions = map[string]bool{
    "us-east-1": true, "us-west-2": true,
    "eu-west-1": true, "ap-south-1": true,
}

func (r CreateServerRequest) Validate() error {
    v := Validator{}

    v.Check(r.Name != "", "name", "is required")
    v.Check(len(r.Name) <= 63, "name", "must be 63 characters or less")
    v.Check(
        regexp.MustCompile(`^[a-z][a-z0-9-]*[a-z0-9]$`).MatchString(r.Name),
        "name", "must be lowercase alphanumeric with hyphens, starting with a letter",
    )

    v.Check(r.CPU >= 1, "cpu", "must be at least 1")
    v.Check(r.CPU <= 96, "cpu", "must be at most 96")
    v.CheckValue(r.CPU == 0 || (r.CPU&(r.CPU-1)) == 0, "cpu",
        "must be a power of 2", r.CPU)

    v.Check(r.MemoryMB >= 128, "memory_mb", "must be at least 128")
    v.Check(r.MemoryMB <= 768*1024, "memory_mb", "must be at most 786432 (768 GB)")

    v.Check(r.Region != "", "region", "is required")
    v.CheckValue(validRegions[r.Region], "region",
        "must be a valid region", r.Region)

    v.Check(len(r.Tags) <= 10, "tags", "must have at most 10 tags")
    for i, tag := range r.Tags {
        v.Check(len(tag) <= 128,
            fmt.Sprintf("tags[%d]", i), "must be 128 characters or less")
    }

    return v.Valid()
}

// Respuesta al cliente con TODOS los errores:
// HTTP 422
// {
//   "error": {
//     "code": "VALIDATION_ERROR",
//     "message": "validation failed",
//     "details": [
//       {"field": "name", "message": "is required"},
//       {"field": "cpu", "message": "must be a power of 2", "value": 3},
//       {"field": "region", "message": "must be a valid region", "value": "mars-1"}
//     ],
//     "request_id": "abc-123"
//   }
// }
```

---

## 7. Patron ResponseWriter Wrapeado

### 7.1 El Problema del Doble Write

```go
// ⚠️ BUG COMUN: escribir headers despues del body
func handler(w http.ResponseWriter, r *http.Request) error {
    data, err := fetchData()
    if err != nil {
        // Si ya escribimos algo al body ANTES de este punto,
        // WriteHeader no tiene efecto → el cliente recibe status 200
        // con un body incompleto + un JSON de error concatenado
        return ErrInternal(err)
    }

    w.Header().Set("Content-Type", "application/json")
    json.NewEncoder(w).Encode(data) // escribe headers 200 implicitamente

    // ⚠️ Ahora si hay un error, ya es tarde para enviar 500
    if err := postProcess(data); err != nil {
        return ErrInternal(err) // writeError intenta WriteHeader(500) → ignorado
    }
    return nil
}
```

### 7.2 Solucion: Buffered Writer

```go
// BufferedWriter acumula la respuesta y solo escribe cuando se confirma
type BufferedWriter struct {
    http.ResponseWriter
    statusCode int
    buf        bytes.Buffer
    committed  bool
}

func NewBufferedWriter(w http.ResponseWriter) *BufferedWriter {
    return &BufferedWriter{
        ResponseWriter: w,
        statusCode:     http.StatusOK,
    }
}

func (bw *BufferedWriter) WriteHeader(code int) {
    if bw.committed {
        return // ya se envio — no hacer nada
    }
    bw.statusCode = code
}

func (bw *BufferedWriter) Write(b []byte) (int, error) {
    return bw.buf.Write(b) // acumular en buffer
}

// Flush envia todo al cliente — SOLO llamar cuando la respuesta es definitiva
func (bw *BufferedWriter) Flush() error {
    if bw.committed {
        return errors.New("already committed")
    }
    bw.committed = true
    bw.ResponseWriter.WriteHeader(bw.statusCode)
    _, err := bw.ResponseWriter.Write(bw.buf.Bytes())
    return err
}

// CanWrite retorna true si aun no se ha enviado la respuesta
func (bw *BufferedWriter) CanWrite() bool {
    return !bw.committed
}
```

### 7.3 AppHandler con Buffered Writer

```go
func (fn AppHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
    bw := NewBufferedWriter(w)

    if err := fn(bw, r); err != nil {
        if bw.CanWrite() {
            // Todavia podemos enviar un error limpio
            writeError(w, r, err) // escribir directo al writer original
        } else {
            // Ya se envio algo — solo logear
            slog.Error("error after response committed",
                "request_id", GetRequestID(r.Context()),
                "error", err,
            )
        }
        return
    }

    // Exito — flush el buffer
    if err := bw.Flush(); err != nil {
        slog.Error("failed to flush response",
            "request_id", GetRequestID(r.Context()),
            "error", err,
        )
    }
}
```

---

## 8. Respuestas Exitosas Estandarizadas

### 8.1 Helpers de Respuesta

```go
// JSON escribe una respuesta JSON exitosa
func JSON(w http.ResponseWriter, status int, data any) error {
    w.Header().Set("Content-Type", "application/json; charset=utf-8")
    w.WriteHeader(status)
    return json.NewEncoder(w).Encode(data)
}

// JSONList escribe una lista paginada
func JSONList[T any](w http.ResponseWriter, items []T, total int, page, pageSize int) error {
    resp := ListResponse[T]{
        Items:    items,
        Total:    total,
        Page:     page,
        PageSize: pageSize,
        Pages:    (total + pageSize - 1) / pageSize,
    }
    return JSON(w, http.StatusOK, resp)
}

type ListResponse[T any] struct {
    Items    []T `json:"items"`
    Total    int `json:"total"`
    Page     int `json:"page"`
    PageSize int `json:"page_size"`
    Pages    int `json:"pages"`
}

// NoContent escribe un 204
func NoContent(w http.ResponseWriter) error {
    w.WriteHeader(http.StatusNoContent)
    return nil
}

// Created escribe un 201 con Location header
func Created(w http.ResponseWriter, location string, data any) error {
    w.Header().Set("Location", location)
    return JSON(w, http.StatusCreated, data)
}
```

### 8.2 Handler con Helpers

```go
func ListServers(svc *ServerService) AppHandler {
    return func(w http.ResponseWriter, r *http.Request) error {
        page, pageSize := parsePagination(r)
        filters := parseFilters(r)

        servers, total, err := svc.List(r.Context(), filters, page, pageSize)
        if err != nil {
            return mapServiceError(err)
        }

        return JSONList(w, servers, total, page, pageSize)
    }
}

func CreateServer(svc *ServerService) AppHandler {
    return func(w http.ResponseWriter, r *http.Request) error {
        var req CreateServerRequest
        if err := decodeJSON(r, &req); err != nil {
            return err // ya es *APIError
        }
        if err := req.Validate(); err != nil {
            return err // ya es *APIError
        }

        server, err := svc.Create(r.Context(), req)
        if err != nil {
            return mapServiceError(err)
        }

        return Created(w,
            fmt.Sprintf("/api/v1/servers/%s", server.ID),
            server,
        )
    }
}
```

### 8.3 Decode Helper con Limites

```go
// decodeJSON parsea el body JSON con protecciones
func decodeJSON(r *http.Request, dst any) error {
    // Limitar tamano del body (1MB)
    const maxBodySize = 1 << 20
    r.Body = http.MaxBytesReader(nil, r.Body, maxBodySize)

    dec := json.NewDecoder(r.Body)
    dec.DisallowUnknownFields() // rechazar campos no conocidos

    if err := dec.Decode(dst); err != nil {
        var syntaxErr *json.SyntaxError
        var typeErr *json.UnmarshalTypeError
        var maxBytesErr *http.MaxBytesError

        switch {
        case errors.As(err, &syntaxErr):
            return ErrBadRequest(fmt.Sprintf(
                "malformed JSON at position %d", syntaxErr.Offset))

        case errors.As(err, &typeErr):
            return ErrBadRequest(fmt.Sprintf(
                "invalid type for field '%s': expected %s",
                typeErr.Field, typeErr.Type))

        case errors.As(err, &maxBytesErr):
            return ErrBadRequest(fmt.Sprintf(
                "request body too large (max %d bytes)", maxBodySize))

        case strings.HasPrefix(err.Error(), "json: unknown field"):
            field := strings.TrimPrefix(err.Error(), "json: unknown field ")
            return ErrBadRequest(fmt.Sprintf("unknown field %s", field))

        case errors.Is(err, io.EOF):
            return ErrBadRequest("request body is empty")

        default:
            return ErrBadRequest("invalid JSON")
        }
    }

    // Verificar que no hay datos extra despues del primer objeto JSON
    if dec.More() {
        return ErrBadRequest("request body must contain a single JSON object")
    }

    return nil
}
```

---

## 9. Error Handling en Middleware de Autenticacion

### 9.1 Auth Middleware con Errores Tipados

```go
// AuthError extiende APIError con detalles de autenticacion
type AuthScheme string

const (
    AuthSchemeBearer AuthScheme = "Bearer"
    AuthSchemeAPIKey AuthScheme = "X-API-Key"
)

func AuthMiddleware(validateToken func(string) (*Claims, error)) func(http.Handler) http.Handler {
    return func(next http.Handler) http.Handler {
        return AppHandler(func(w http.ResponseWriter, r *http.Request) error {
            // Extraer token
            authHeader := r.Header.Get("Authorization")
            if authHeader == "" {
                return &APIError{
                    Status:  http.StatusUnauthorized,
                    Code:    "MISSING_AUTH",
                    Message: "Authorization header is required",
                    Details: map[string]string{
                        "scheme": "Bearer",
                        "header": "Authorization: Bearer <token>",
                    },
                }
            }

            // Validar formato "Bearer <token>"
            parts := strings.SplitN(authHeader, " ", 2)
            if len(parts) != 2 || parts[0] != "Bearer" {
                return &APIError{
                    Status:  http.StatusUnauthorized,
                    Code:    "INVALID_AUTH_FORMAT",
                    Message: "Authorization header must use Bearer scheme",
                }
            }

            // Validar token
            claims, err := validateToken(parts[1])
            if err != nil {
                if errors.Is(err, ErrExpired) {
                    return &APIError{
                        Status:  http.StatusUnauthorized,
                        Code:    "TOKEN_EXPIRED",
                        Message: "authentication token has expired",
                    }
                }
                return &APIError{
                    Status:   http.StatusUnauthorized,
                    Code:     "INVALID_TOKEN",
                    Message:  "authentication token is invalid",
                    Internal: err, // logear el motivo real
                }
            }

            // Inyectar claims en el contexto
            ctx := context.WithValue(r.Context(), ctxKeyClaims, claims)
            next.ServeHTTP(w, r.WithContext(ctx))
            return nil
        })
    }
}
```

### 9.2 Authorization (Permisos)

```go
// RequireRole verifica que el usuario tenga un rol especifico
func RequireRole(role string) func(http.Handler) http.Handler {
    return func(next http.Handler) http.Handler {
        return AppHandler(func(w http.ResponseWriter, r *http.Request) error {
            claims, ok := r.Context().Value(ctxKeyClaims).(*Claims)
            if !ok {
                return ErrInternal(fmt.Errorf("claims not in context — auth middleware missing?"))
            }

            if !claims.HasRole(role) {
                return &APIError{
                    Status:  http.StatusForbidden,
                    Code:    "INSUFFICIENT_PERMISSIONS",
                    Message: fmt.Sprintf("role '%s' is required", role),
                    Details: map[string]any{
                        "required_role": role,
                        "your_roles":    claims.Roles,
                    },
                }
            }

            next.ServeHTTP(w, r)
            return nil
        })
    }
}

// Uso:
mux.Handle("DELETE /api/v1/servers/{id}",
    Chain(DeleteServer(svc),
        AuthMiddleware(tokenValidator),   // primero: autenticar
        RequireRole("admin"),             // luego: autorizar
    ),
)
```

---

## 10. Rate Limiting con Error Responses

```go
type RateLimiter struct {
    mu       sync.Mutex
    visitors map[string]*visitor
    rate     rate.Limit
    burst    int
}

type visitor struct {
    limiter  *rate.Limiter
    lastSeen time.Time
}

func NewRateLimiter(r rate.Limit, burst int) *RateLimiter {
    rl := &RateLimiter{
        visitors: make(map[string]*visitor),
        rate:     r,
        burst:    burst,
    }
    // Limpiar visitantes inactivos periodicamente
    go rl.cleanup(3 * time.Minute)
    return rl
}

func (rl *RateLimiter) getVisitor(ip string) *rate.Limiter {
    rl.mu.Lock()
    defer rl.mu.Unlock()

    v, exists := rl.visitors[ip]
    if !exists {
        limiter := rate.NewLimiter(rl.rate, rl.burst)
        rl.visitors[ip] = &visitor{limiter: limiter, lastSeen: time.Now()}
        return limiter
    }
    v.lastSeen = time.Now()
    return v.limiter
}

func (rl *RateLimiter) cleanup(interval time.Duration) {
    ticker := time.NewTicker(interval)
    for range ticker.C {
        rl.mu.Lock()
        for ip, v := range rl.visitors {
            if time.Since(v.lastSeen) > 3*time.Minute {
                delete(rl.visitors, ip)
            }
        }
        rl.mu.Unlock()
    }
}

// Middleware que aplica rate limiting con respuestas estandarizadas
func (rl *RateLimiter) Middleware(next http.Handler) http.Handler {
    return AppHandler(func(w http.ResponseWriter, r *http.Request) error {
        ip := extractIP(r)
        limiter := rl.getVisitor(ip)

        if !limiter.Allow() {
            // Calcular cuando podra reintentar
            reservation := limiter.Reserve()
            delay := reservation.Delay()
            reservation.Cancel() // no consumir el token

            w.Header().Set("Retry-After", fmt.Sprintf("%.0f", delay.Seconds()))
            w.Header().Set("X-RateLimit-Limit", fmt.Sprintf("%.0f", float64(rl.rate)))

            return ErrTooManyRequests(delay)
        }

        next.ServeHTTP(w, r)
        return nil
    })
}

func extractIP(r *http.Request) string {
    // En produccion: verificar X-Forwarded-For si hay reverse proxy de confianza
    // CUIDADO: X-Forwarded-For puede ser falsificado si no hay proxy de confianza
    ip := r.RemoteAddr
    if i := strings.LastIndex(ip, ":"); i != -1 {
        ip = ip[:i]
    }
    return ip
}
```

---

## 11. Testing de Error Responses

### 11.1 Test Helper

```go
// testRequest ejecuta un handler y retorna el response recorder
func testRequest(t *testing.T, handler http.Handler, method, path string, body any) *httptest.ResponseRecorder {
    t.Helper()

    var bodyReader io.Reader
    if body != nil {
        b, err := json.Marshal(body)
        if err != nil {
            t.Fatalf("marshal body: %v", err)
        }
        bodyReader = bytes.NewReader(b)
    }

    req := httptest.NewRequest(method, path, bodyReader)
    req.Header.Set("Content-Type", "application/json")
    rec := httptest.NewRecorder()

    handler.ServeHTTP(rec, req)
    return rec
}

// assertErrorResponse verifica la estructura del error JSON
func assertErrorResponse(t *testing.T, rec *httptest.ResponseRecorder, wantStatus int, wantCode string) {
    t.Helper()

    if rec.Code != wantStatus {
        t.Errorf("status = %d, want %d", rec.Code, wantStatus)
    }

    ct := rec.Header().Get("Content-Type")
    if !strings.HasPrefix(ct, "application/json") {
        t.Errorf("Content-Type = %q, want application/json", ct)
    }

    var resp ErrorResponse
    if err := json.NewDecoder(rec.Body).Decode(&resp); err != nil {
        t.Fatalf("decode response: %v", err)
    }

    if resp.Error.Code != wantCode {
        t.Errorf("error code = %q, want %q", resp.Error.Code, wantCode)
    }
}
```

### 11.2 Tests de Error Scenarios

```go
func TestCreateServer_ValidationErrors(t *testing.T) {
    svc := NewServerService(newMockRepo())
    handler := Chain(CreateServer(svc), Recovery, RequestID)

    tests := []struct {
        name       string
        body       any
        wantStatus int
        wantCode   string
    }{
        {
            name:       "empty body",
            body:       nil,
            wantStatus: 400,
            wantCode:   "BAD_REQUEST",
        },
        {
            name:       "invalid JSON",
            body:       "not json", // el marshal lo convertira a string valido
            wantStatus: 400,
            wantCode:   "BAD_REQUEST",
        },
        {
            name: "missing required fields",
            body: CreateServerRequest{
                // Name vacio, CPU 0, Region vacia
            },
            wantStatus: 422,
            wantCode:   "VALIDATION_ERROR",
        },
        {
            name: "invalid region",
            body: CreateServerRequest{
                Name: "test-server", CPU: 2, MemoryMB: 1024, Region: "mars-1",
            },
            wantStatus: 422,
            wantCode:   "VALIDATION_ERROR",
        },
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            rec := testRequest(t, handler, "POST", "/api/v1/servers", tt.body)
            assertErrorResponse(t, rec, tt.wantStatus, tt.wantCode)
        })
    }
}

func TestCreateServer_DomainErrors(t *testing.T) {
    tests := []struct {
        name       string
        setupRepo  func(*MockRepo) // configurar el mock para producir errores
        wantStatus int
        wantCode   string
    }{
        {
            name: "duplicate name",
            setupRepo: func(m *MockRepo) {
                m.findByNameResult = &Server{Name: "existing"} // ya existe
            },
            wantStatus: 409,
            wantCode:   "CONFLICT",
        },
        {
            name: "database error",
            setupRepo: func(m *MockRepo) {
                m.insertErr = fmt.Errorf("connection refused")
            },
            wantStatus: 500,
            wantCode:   "INTERNAL_ERROR",
        },
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            repo := newMockRepo()
            tt.setupRepo(repo)
            svc := NewServerService(repo)
            handler := Chain(CreateServer(svc), Recovery, RequestID)

            body := CreateServerRequest{
                Name: "test-server", CPU: 2, MemoryMB: 1024, Region: "us-east-1",
            }
            rec := testRequest(t, handler, "POST", "/api/v1/servers", body)
            assertErrorResponse(t, rec, tt.wantStatus, tt.wantCode)
        })
    }
}

func TestRecovery_PanicHandler(t *testing.T) {
    panicHandler := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        panic("unexpected nil pointer")
    })

    handler := Chain(panicHandler, Recovery, RequestID)
    rec := testRequest(t, handler, "GET", "/", nil)

    assertErrorResponse(t, rec, 500, "INTERNAL_ERROR")

    // Verificar que el body NO contiene el mensaje del panic
    body := rec.Body.String()
    if strings.Contains(body, "nil pointer") {
        t.Error("panic message leaked to client response")
    }
}
```

### 11.3 Test de Timeout

```go
func TestTimeout_SlowHandler(t *testing.T) {
    slowHandler := AppHandler(func(w http.ResponseWriter, r *http.Request) error {
        select {
        case <-time.After(5 * time.Second):
            return JSON(w, 200, map[string]string{"status": "ok"})
        case <-r.Context().Done():
            return mapServiceError(r.Context().Err())
        }
    })

    handler := Chain(slowHandler, Recovery, RequestID, Timeout(100*time.Millisecond))
    rec := testRequest(t, handler, "GET", "/slow", nil)

    assertErrorResponse(t, rec, 503, "SERVICE_UNAVAILABLE")
}
```

---

## 12. Comparacion con C y Rust

### 12.1 C — Error Handling en HTTP (con libmicrohttpd)

C no tiene un framework estandar para APIs HTTP. Con `libmicrohttpd` (la opcion mas comun), el error handling es completamente manual:

```c
#include <microhttpd.h>
#include <json-c/json.h>
#include <stdio.h>
#include <string.h>

// Funcion helper para enviar error JSON (repetida en cada proyecto)
static enum MHD_Result
send_json_error(struct MHD_Connection *connection,
                unsigned int status,
                const char *code,
                const char *message) {
    // Construir JSON manualmente o con json-c
    json_object *root = json_object_new_object();
    json_object *error = json_object_new_object();

    json_object_object_add(error, "code", json_object_new_string(code));
    json_object_object_add(error, "message", json_object_new_string(message));
    json_object_object_add(root, "error", error);

    const char *body = json_object_to_json_string(root);

    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(body), (void *)body, MHD_RESPMEM_MUST_COPY);

    MHD_add_response_header(response, "Content-Type", "application/json");

    enum MHD_Result ret = MHD_queue_response(connection, status, response);
    MHD_destroy_response(response);
    json_object_put(root); // liberar JSON

    return ret;
}

// Handler — todo manual, sin abstracciones
static enum MHD_Result
handle_get_server(struct MHD_Connection *connection,
                  const char *server_id) {
    // Buscar en DB
    Server *server = db_find_server(server_id);
    if (server == NULL) {
        if (db_errno == DB_ERR_NOT_FOUND) {
            return send_json_error(connection, 404,
                "NOT_FOUND", "server not found");
        }
        // Error de DB — logear internamente
        fprintf(stderr, "ERROR: db_find_server(%s): %s\n",
                server_id, db_strerror(db_errno));
        return send_json_error(connection, 500,
            "INTERNAL_ERROR", "internal server error");
    }

    // Serializar respuesta — manual con json-c
    json_object *json = server_to_json(server);
    const char *body = json_object_to_json_string(json);

    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(body), (void *)body, MHD_RESPMEM_MUST_COPY);
    MHD_add_response_header(response, "Content-Type", "application/json");

    enum MHD_Result ret = MHD_queue_response(connection, 200, response);
    MHD_destroy_response(response);
    json_object_put(json);
    server_free(server);

    return ret;
}

/*
 * PROBLEMAS vs Go:
 * - No hay middleware chain (implementar manualmente con function pointers)
 * - No hay validacion estructurada (parsear JSON campo por campo)
 * - No hay tipos de error (errno + strings manuales)
 * - Memory management manual (json_object_put, server_free, MHD_destroy)
 * - No hay goroutines (MHD usa thread pool interno, tu no controlas)
 * - No hay contexto con timeout (implementar con alarm/timerfd)
 * - No hay recover de panics (SIGSEGV = muerte del proceso)
 * - Cada handler repite el boilerplate de JSON + response + cleanup
 */
```

### 12.2 Rust — Axum Framework

Rust con Axum tiene el error handling mas avanzado de los tres, con tipos como `IntoResponse` y extractores tipados:

```rust
use axum::{
    extract::{Path, State, Json},
    http::StatusCode,
    response::{IntoResponse, Response},
    routing::{get, post, delete},
    Router,
    middleware,
};
use serde::{Deserialize, Serialize};
use thiserror::Error;

// ─── Error types ────────────────────────────────────────────

#[derive(Error, Debug)]
enum AppError {
    #[error("not found: {resource} '{id}'")]
    NotFound { resource: String, id: String },

    #[error("conflict: {0}")]
    Conflict(String),

    #[error("validation failed")]
    Validation(Vec<FieldError>),

    #[error("unauthorized: {0}")]
    Unauthorized(String),

    #[error("forbidden: {0}")]
    Forbidden(String),

    #[error("internal error")]
    Internal(#[from] anyhow::Error),
}

#[derive(Serialize, Debug)]
struct FieldError {
    field: String,
    message: String,
}

// El trait IntoResponse convierte AppError en respuesta HTTP
// ← equivalente al writeError de Go, pero verificado en compile-time
impl IntoResponse for AppError {
    fn into_response(self) -> Response {
        let (status, code, message, details) = match &self {
            AppError::NotFound { resource, id } => (
                StatusCode::NOT_FOUND,
                "NOT_FOUND",
                format!("{resource} '{id}' not found"),
                None,
            ),
            AppError::Conflict(msg) => (
                StatusCode::CONFLICT,
                "CONFLICT",
                msg.clone(),
                None,
            ),
            AppError::Validation(fields) => (
                StatusCode::UNPROCESSABLE_ENTITY,
                "VALIDATION_ERROR",
                "validation failed".to_string(),
                Some(serde_json::to_value(fields).unwrap()),
            ),
            AppError::Unauthorized(msg) => (
                StatusCode::UNAUTHORIZED,
                "UNAUTHORIZED",
                msg.clone(),
                None,
            ),
            AppError::Forbidden(msg) => (
                StatusCode::FORBIDDEN,
                "FORBIDDEN",
                msg.clone(),
                None,
            ),
            AppError::Internal(err) => {
                // Logear el error interno (NO enviarlo al cliente)
                tracing::error!(?err, "internal server error");
                (
                    StatusCode::INTERNAL_SERVER_ERROR,
                    "INTERNAL_ERROR",
                    "an internal error occurred".to_string(),
                    None,
                )
            }
        };

        let body = serde_json::json!({
            "error": {
                "code": code,
                "message": message,
                "details": details,
            }
        });

        (status, axum::Json(body)).into_response()
    }
}

// ─── Handlers ───────────────────────────────────────────────

// El handler retorna Result<impl IntoResponse, AppError>
// Si retorna Err(AppError), Axum lo convierte automaticamente en HTTP response
// ← El compilador VERIFICA que todos los paths retornan o el valor o el error

async fn get_server(
    State(svc): State<ServerService>,
    Path(id): Path<String>,          // ← extractor tipado (400 si falta)
) -> Result<Json<Server>, AppError> {
    let server = svc.get(&id).await?;  // ← ? propaga el error
    Ok(Json(server))
}

async fn create_server(
    State(svc): State<ServerService>,
    Json(req): Json<CreateServerRequest>,  // ← deserializa y valida JSON (400 si invalido)
) -> Result<(StatusCode, Json<Server>), AppError> {
    req.validate()?;  // ← ? propaga ValidationError
    let server = svc.create(req).await?;  // ← ? propaga domain errors
    Ok((StatusCode::CREATED, Json(server)))
}

async fn delete_server(
    State(svc): State<ServerService>,
    Path(id): Path<String>,
) -> Result<StatusCode, AppError> {
    svc.delete(&id).await?;
    Ok(StatusCode::NO_CONTENT)
}

// ─── Router ─────────────────────────────────────────────────

fn router(svc: ServerService) -> Router {
    Router::new()
        .route("/api/v1/servers", post(create_server))
        .route("/api/v1/servers/:id", get(get_server).delete(delete_server))
        .layer(middleware::from_fn(request_id_middleware))
        .layer(middleware::from_fn(logging_middleware))
        .with_state(svc)
}

/*
 * COMPARACION Go vs Rust:
 *
 * ┌──────────────────────┬─────────────────────────┬──────────────────────────┐
 * │                      │ Go (net/http + custom)   │ Rust (Axum)              │
 * ├──────────────────────┼─────────────────────────┼──────────────────────────┤
 * │ Handler retorna error│ Patron AppHandler        │ Result<T, AppError>      │
 * │                      │ (convencion, no forzado) │ (tipo, compilador forza) │
 * │                      │                         │                          │
 * │ Error → HTTP         │ writeError (runtime)     │ IntoResponse (compile)   │
 * │                      │                         │                          │
 * │ Validacion           │ Manual (Validator)       │ Extractors (compile)     │
 * │                      │                         │                          │
 * │ JSON parsing         │ json.Decoder (runtime)   │ Json<T> extractor        │
 * │                      │ errores manuales        │ (auto 400 si falla)      │
 * │                      │                         │                          │
 * │ Path params          │ r.PathValue("id")        │ Path(id): Path<String>   │
 * │                      │ (puede ser "")           │ (type-safe, auto 400)    │
 * │                      │                         │                          │
 * │ Middleware            │ func(Handler) Handler    │ tower::Layer + Service   │
 * │                      │ (convencion)             │ (trait system)           │
 * │                      │                         │                          │
 * │ Exhaustividad        │ No verificada            │ match verifica todos     │
 * │ de errores           │ (default: 500)           │ los variantes            │
 * │                      │                         │                          │
 * │ ? operator           │ No existe                │ Propaga con conversion   │
 * │                      │ (if err != nil manual)   │ automatica (From trait)  │
 * │                      │                         │                          │
 * │ Panic recovery       │ defer recover (manual)   │ tower::catch_panic       │
 * │                      │                         │ (middleware)              │
 * └──────────────────────┴─────────────────────────┴──────────────────────────┘
 *
 * Go: mas simple, mas explicito, mas control manual
 * Rust: mas seguro (compile-time), mas conciso (? operator), mas complejo
 * C: todo manual — no hay abstracciones built-in para APIs
 */
```

---

## 13. Programa Completo: Infrastructure API Server

Un servidor API completo para gestionar servidores de infraestructura, con todos los patrones de error handling vistos:

```go
package main

import (
    "bytes"
    "context"
    "crypto/rand"
    "encoding/json"
    "errors"
    "fmt"
    "io"
    "log/slog"
    "net/http"
    "os"
    "os/signal"
    "regexp"
    "runtime/debug"
    "strings"
    "sync"
    "syscall"
    "time"
)

// ═════════════════════════════════════════════════════════════════
// 1. TIPOS DE ERROR API
// ═════════════════════════════════════════════════════════════════

type APIError struct {
    Status   int    `json:"-"`
    Code     string `json:"code"`
    Message  string `json:"message"`
    Details  any    `json:"details,omitempty"`
    Internal error  `json:"-"`
}

func (e *APIError) Error() string {
    if e.Internal != nil {
        return fmt.Sprintf("[%d] %s: %s (%v)", e.Status, e.Code, e.Message, e.Internal)
    }
    return fmt.Sprintf("[%d] %s: %s", e.Status, e.Code, e.Message)
}

func (e *APIError) Unwrap() error { return e.Internal }

// Constructores
func errBadRequest(msg string) *APIError {
    return &APIError{Status: 400, Code: "BAD_REQUEST", Message: msg}
}
func errNotFound(resource, id string) *APIError {
    return &APIError{Status: 404, Code: "NOT_FOUND",
        Message: fmt.Sprintf("%s '%s' not found", resource, id)}
}
func errConflict(msg string) *APIError {
    return &APIError{Status: 409, Code: "CONFLICT", Message: msg}
}
func errValidation(fields []FieldError) *APIError {
    return &APIError{Status: 422, Code: "VALIDATION_ERROR",
        Message: "validation failed", Details: fields}
}
func errInternal(err error) *APIError {
    return &APIError{Status: 500, Code: "INTERNAL_ERROR",
        Message: "an internal error occurred", Internal: err}
}

// ═════════════════════════════════════════════════════════════════
// 2. VALIDACION
// ═════════════════════════════════════════════════════════════════

type FieldError struct {
    Field   string `json:"field"`
    Message string `json:"message"`
    Value   any    `json:"value,omitempty"`
}

type Validator struct {
    errors []FieldError
}

func (v *Validator) Check(ok bool, field, msg string) {
    if !ok {
        v.errors = append(v.errors, FieldError{Field: field, Message: msg})
    }
}

func (v *Validator) CheckVal(ok bool, field, msg string, val any) {
    if !ok {
        v.errors = append(v.errors, FieldError{Field: field, Message: msg, Value: val})
    }
}

func (v *Validator) Err() error {
    if len(v.errors) == 0 {
        return nil
    }
    return errValidation(v.errors)
}

// ═════════════════════════════════════════════════════════════════
// 3. ERRORES DE DOMINIO
// ═════════════════════════════════════════════════════════════════

var (
    ErrNotFound      = errors.New("not found")
    ErrAlreadyExists = errors.New("already exists")
)

type DomainError struct {
    Resource string
    ID       string
    Kind     error
    Err      error
}

func (e *DomainError) Error() string {
    return fmt.Sprintf("%s(%s): %v", e.Resource, e.ID, e.Err)
}
func (e *DomainError) Unwrap() error { return e.Err }
func (e *DomainError) Is(target error) bool {
    return errors.Is(e.Kind, target)
}

// Mapeo dominio → API
func mapDomainError(err error) error {
    if err == nil {
        return nil
    }
    var de *DomainError
    switch {
    case errors.As(err, &de) && errors.Is(de.Kind, ErrNotFound):
        return errNotFound(de.Resource, de.ID)
    case errors.As(err, &de) && errors.Is(de.Kind, ErrAlreadyExists):
        return errConflict(fmt.Sprintf("%s '%s' already exists", de.Resource, de.ID))
    case errors.Is(err, context.DeadlineExceeded):
        return &APIError{Status: 504, Code: "TIMEOUT", Message: "request timed out", Internal: err}
    default:
        return errInternal(err)
    }
}

// ═════════════════════════════════════════════════════════════════
// 4. MODELO Y REPOSITORIO (in-memory)
// ═════════════════════════════════════════════════════════════════

type Server struct {
    ID        string    `json:"id"`
    Name      string    `json:"name"`
    CPU       int       `json:"cpu"`
    MemoryMB  int       `json:"memory_mb"`
    Region    string    `json:"region"`
    Status    string    `json:"status"`
    CreatedAt time.Time `json:"created_at"`
}

type ServerRepo struct {
    mu      sync.RWMutex
    servers map[string]*Server
    counter int
}

func NewServerRepo() *ServerRepo {
    return &ServerRepo{servers: make(map[string]*Server)}
}

func (r *ServerRepo) FindByID(_ context.Context, id string) (*Server, error) {
    r.mu.RLock()
    defer r.mu.RUnlock()
    s, ok := r.servers[id]
    if !ok {
        return nil, &DomainError{Resource: "server", ID: id, Kind: ErrNotFound,
            Err: fmt.Errorf("server %s: %w", id, ErrNotFound)}
    }
    return s, nil
}

func (r *ServerRepo) FindByName(_ context.Context, name string) (*Server, error) {
    r.mu.RLock()
    defer r.mu.RUnlock()
    for _, s := range r.servers {
        if s.Name == name {
            return s, nil
        }
    }
    return nil, &DomainError{Resource: "server", ID: name, Kind: ErrNotFound,
        Err: fmt.Errorf("server name=%s: %w", name, ErrNotFound)}
}

func (r *ServerRepo) Insert(_ context.Context, s *Server) (*Server, error) {
    r.mu.Lock()
    defer r.mu.Unlock()
    // Verificar duplicado
    for _, existing := range r.servers {
        if existing.Name == s.Name {
            return nil, &DomainError{Resource: "server", ID: s.Name, Kind: ErrAlreadyExists,
                Err: fmt.Errorf("server name=%s: %w", s.Name, ErrAlreadyExists)}
        }
    }
    r.counter++
    s.ID = fmt.Sprintf("srv-%04d", r.counter)
    s.Status = "provisioning"
    s.CreatedAt = time.Now()
    r.servers[s.ID] = s
    return s, nil
}

func (r *ServerRepo) Delete(_ context.Context, id string) error {
    r.mu.Lock()
    defer r.mu.Unlock()
    if _, ok := r.servers[id]; !ok {
        return &DomainError{Resource: "server", ID: id, Kind: ErrNotFound,
            Err: fmt.Errorf("server %s: %w", id, ErrNotFound)}
    }
    delete(r.servers, id)
    return nil
}

func (r *ServerRepo) List(_ context.Context) ([]*Server, error) {
    r.mu.RLock()
    defer r.mu.RUnlock()
    result := make([]*Server, 0, len(r.servers))
    for _, s := range r.servers {
        result = append(result, s)
    }
    return result, nil
}

// ═════════════════════════════════════════════════════════════════
// 5. RESPONSE HELPERS
// ═════════════════════════════════════════════════════════════════

type ErrorResponse struct {
    Error ErrorBody `json:"error"`
}

type ErrorBody struct {
    Code      string `json:"code"`
    Message   string `json:"message"`
    Details   any    `json:"details,omitempty"`
    RequestID string `json:"request_id,omitempty"`
}

func writeJSON(w http.ResponseWriter, status int, data any) error {
    w.Header().Set("Content-Type", "application/json; charset=utf-8")
    w.WriteHeader(status)
    return json.NewEncoder(w).Encode(data)
}

func writeError(w http.ResponseWriter, r *http.Request, err error) {
    requestID, _ := r.Context().Value(ctxKeyRequestID).(string)

    var apiErr *APIError
    if !errors.As(err, &apiErr) {
        apiErr = errInternal(err)
    }

    // Logging
    logAttrs := []any{
        "request_id", requestID,
        "method", r.Method,
        "path", r.URL.Path,
        "status", apiErr.Status,
        "code", apiErr.Code,
    }
    if apiErr.Status >= 500 {
        slog.Error("server error", append(logAttrs, "internal", apiErr.Internal)...)
    } else {
        slog.Warn("client error", append(logAttrs, "message", apiErr.Message)...)
    }

    resp := ErrorResponse{
        Error: ErrorBody{
            Code:      apiErr.Code,
            Message:   apiErr.Message,
            RequestID: requestID,
        },
    }
    if apiErr.Status < 500 && apiErr.Details != nil {
        resp.Error.Details = apiErr.Details
    }

    w.Header().Set("Content-Type", "application/json; charset=utf-8")
    w.Header().Set("X-Content-Type-Options", "nosniff")
    w.WriteHeader(apiErr.Status)
    json.NewEncoder(w).Encode(resp)
}

// ═════════════════════════════════════════════════════════════════
// 6. APPHANDLER + MIDDLEWARES
// ═════════════════════════════════════════════════════════════════

type AppHandler func(http.ResponseWriter, *http.Request) error

func (fn AppHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
    if err := fn(w, r); err != nil {
        writeError(w, r, err)
    }
}

type contextKey string

const ctxKeyRequestID contextKey = "request_id"

// Middleware: Recovery
func mwRecovery(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        defer func() {
            if rec := recover(); rec != nil {
                stack := debug.Stack()
                reqID, _ := r.Context().Value(ctxKeyRequestID).(string)
                slog.Error("panic recovered",
                    "request_id", reqID,
                    "panic", fmt.Sprintf("%v", rec),
                    "stack", string(stack),
                )
                w.Header().Set("Content-Type", "application/json; charset=utf-8")
                w.WriteHeader(500)
                json.NewEncoder(w).Encode(ErrorResponse{
                    Error: ErrorBody{
                        Code:      "INTERNAL_ERROR",
                        Message:   "an unexpected error occurred",
                        RequestID: reqID,
                    },
                })
            }
        }()
        next.ServeHTTP(w, r)
    })
}

// Middleware: Request ID
func mwRequestID(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        id := r.Header.Get("X-Request-ID")
        if id == "" {
            b := make([]byte, 8)
            rand.Read(b)
            id = fmt.Sprintf("%x", b)
        }
        ctx := context.WithValue(r.Context(), ctxKeyRequestID, id)
        w.Header().Set("X-Request-ID", id)
        next.ServeHTTP(w, r.WithContext(ctx))
    })
}

// Middleware: Logging
type statusRecorder struct {
    http.ResponseWriter
    status int
}

func (sr *statusRecorder) WriteHeader(code int) {
    sr.status = code
    sr.ResponseWriter.WriteHeader(code)
}

func mwLogging(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        start := time.Now()
        sr := &statusRecorder{ResponseWriter: w, status: 200}

        next.ServeHTTP(sr, r)

        reqID, _ := r.Context().Value(ctxKeyRequestID).(string)
        slog.Info("request",
            "request_id", reqID,
            "method", r.Method,
            "path", r.URL.Path,
            "status", sr.status,
            "duration_ms", time.Since(start).Milliseconds(),
        )
    })
}

// Middleware: Timeout
func mwTimeout(d time.Duration) func(http.Handler) http.Handler {
    return func(next http.Handler) http.Handler {
        return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
            ctx, cancel := context.WithTimeout(r.Context(), d)
            defer cancel()
            next.ServeHTTP(w, r.WithContext(ctx))
        })
    }
}

// Chain
func chain(h http.Handler, mws ...func(http.Handler) http.Handler) http.Handler {
    for i := len(mws) - 1; i >= 0; i-- {
        h = mws[i](h)
    }
    return h
}

// ═════════════════════════════════════════════════════════════════
// 7. DECODE + VALIDATE HELPERS
// ═════════════════════════════════════════════════════════════════

func decodeJSON(r *http.Request, dst any) error {
    const maxBody = 1 << 20 // 1MB
    r.Body = http.MaxBytesReader(nil, r.Body, maxBody)

    dec := json.NewDecoder(r.Body)
    dec.DisallowUnknownFields()

    if err := dec.Decode(dst); err != nil {
        var synErr *json.SyntaxError
        var typeErr *json.UnmarshalTypeError
        var maxErr *http.MaxBytesError

        switch {
        case errors.As(err, &synErr):
            return errBadRequest(fmt.Sprintf("malformed JSON at position %d", synErr.Offset))
        case errors.As(err, &typeErr):
            return errBadRequest(fmt.Sprintf("invalid type for '%s': expected %s", typeErr.Field, typeErr.Type))
        case errors.As(err, &maxErr):
            return errBadRequest("request body too large")
        case errors.Is(err, io.EOF):
            return errBadRequest("request body is empty")
        case strings.HasPrefix(err.Error(), "json: unknown field"):
            return errBadRequest(fmt.Sprintf("unknown field %s", strings.TrimPrefix(err.Error(), "json: unknown field ")))
        default:
            return errBadRequest("invalid JSON")
        }
    }

    if dec.More() {
        return errBadRequest("body must contain a single JSON object")
    }
    return nil
}

// ═════════════════════════════════════════════════════════════════
// 8. REQUEST TYPES
// ═════════════════════════════════════════════════════════════════

type CreateServerRequest struct {
    Name     string `json:"name"`
    CPU      int    `json:"cpu"`
    MemoryMB int    `json:"memory_mb"`
    Region   string `json:"region"`
}

var (
    namePattern  = regexp.MustCompile(`^[a-z][a-z0-9-]*[a-z0-9]$`)
    validRegions = map[string]bool{
        "us-east-1": true, "us-west-2": true,
        "eu-west-1": true, "ap-south-1": true,
    }
)

func (r CreateServerRequest) Validate() error {
    v := Validator{}
    v.Check(r.Name != "", "name", "is required")
    if r.Name != "" {
        v.Check(len(r.Name) >= 3, "name", "must be at least 3 characters")
        v.Check(len(r.Name) <= 63, "name", "must be at most 63 characters")
        v.Check(namePattern.MatchString(r.Name), "name",
            "must be lowercase alphanumeric with hyphens")
    }
    v.Check(r.CPU >= 1, "cpu", "must be at least 1")
    v.Check(r.CPU <= 96, "cpu", "must be at most 96")
    v.Check(r.MemoryMB >= 128, "memory_mb", "must be at least 128")
    v.Check(r.MemoryMB <= 786432, "memory_mb", "must be at most 786432")
    v.Check(r.Region != "", "region", "is required")
    if r.Region != "" {
        v.CheckVal(validRegions[r.Region], "region", "invalid region", r.Region)
    }
    return v.Err()
}

// ═════════════════════════════════════════════════════════════════
// 9. HANDLERS
// ═════════════════════════════════════════════════════════════════

func handleListServers(repo *ServerRepo) AppHandler {
    return func(w http.ResponseWriter, r *http.Request) error {
        servers, err := repo.List(r.Context())
        if err != nil {
            return mapDomainError(err)
        }
        return writeJSON(w, http.StatusOK, map[string]any{
            "items": servers,
            "total": len(servers),
        })
    }
}

func handleGetServer(repo *ServerRepo) AppHandler {
    return func(w http.ResponseWriter, r *http.Request) error {
        id := r.PathValue("id")
        if id == "" {
            return errBadRequest("server id is required")
        }

        server, err := repo.FindByID(r.Context(), id)
        if err != nil {
            return mapDomainError(err)
        }
        return writeJSON(w, http.StatusOK, server)
    }
}

func handleCreateServer(repo *ServerRepo) AppHandler {
    return func(w http.ResponseWriter, r *http.Request) error {
        var req CreateServerRequest
        if err := decodeJSON(r, &req); err != nil {
            return err
        }
        if err := req.Validate(); err != nil {
            return err
        }

        server := &Server{
            Name:     req.Name,
            CPU:      req.CPU,
            MemoryMB: req.MemoryMB,
            Region:   req.Region,
        }

        created, err := repo.Insert(r.Context(), server)
        if err != nil {
            return mapDomainError(err)
        }

        w.Header().Set("Location", fmt.Sprintf("/api/v1/servers/%s", created.ID))
        return writeJSON(w, http.StatusCreated, created)
    }
}

func handleDeleteServer(repo *ServerRepo) AppHandler {
    return func(w http.ResponseWriter, r *http.Request) error {
        id := r.PathValue("id")
        if id == "" {
            return errBadRequest("server id is required")
        }

        if err := repo.Delete(r.Context(), id); err != nil {
            return mapDomainError(err)
        }

        w.WriteHeader(http.StatusNoContent)
        return nil
    }
}

// Health check — siempre responde (sin AppHandler)
func handleHealth(w http.ResponseWriter, r *http.Request) {
    w.Header().Set("Content-Type", "application/json")
    json.NewEncoder(w).Encode(map[string]string{
        "status": "ok",
        "time":   time.Now().UTC().Format(time.RFC3339),
    })
}

// Handler que siempre hace panic (para probar Recovery)
func handlePanic() AppHandler {
    return func(w http.ResponseWriter, r *http.Request) error {
        panic("intentional panic for testing recovery middleware")
    }
}

// ═════════════════════════════════════════════════════════════════
// 10. MAIN — COMPOSICION FINAL
// ═════════════════════════════════════════════════════════════════

func main() {
    // Configurar structured logging
    logger := slog.New(slog.NewJSONHandler(os.Stdout, &slog.HandlerOptions{
        Level: slog.LevelInfo,
    }))
    slog.SetDefault(logger)

    // Repositorio in-memory
    repo := NewServerRepo()

    // Seed con datos iniciales
    repo.Insert(context.Background(), &Server{
        Name: "web-prod-01", CPU: 4, MemoryMB: 8192, Region: "us-east-1",
    })
    repo.Insert(context.Background(), &Server{
        Name: "db-prod-01", CPU: 8, MemoryMB: 32768, Region: "us-east-1",
    })
    repo.Insert(context.Background(), &Server{
        Name: "cache-prod-01", CPU: 2, MemoryMB: 4096, Region: "eu-west-1",
    })

    // Router
    mux := http.NewServeMux()

    // Health (sin AppHandler — siempre responde)
    mux.HandleFunc("GET /health", handleHealth)

    // API routes (con AppHandler)
    mux.Handle("GET /api/v1/servers", handleListServers(repo))
    mux.Handle("POST /api/v1/servers", handleCreateServer(repo))
    mux.Handle("GET /api/v1/servers/{id}", handleGetServer(repo))
    mux.Handle("DELETE /api/v1/servers/{id}", handleDeleteServer(repo))

    // Test route (para probar panic recovery)
    mux.Handle("GET /api/v1/panic", handlePanic())

    // Aplicar middleware chain
    handler := chain(mux,
        mwRecovery,
        mwRequestID,
        mwLogging,
        mwTimeout(15*time.Second),
    )

    // Server con graceful shutdown
    srv := &http.Server{
        Addr:         ":8080",
        Handler:      handler,
        ReadTimeout:  10 * time.Second,
        WriteTimeout: 30 * time.Second,
        IdleTimeout:  60 * time.Second,
    }

    // Arrancar en goroutine
    go func() {
        slog.Info("server starting", "addr", srv.Addr)
        if err := srv.ListenAndServe(); err != http.ErrServerClosed {
            slog.Error("server error", "error", err)
            os.Exit(1)
        }
    }()

    // Esperar senal de shutdown
    ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
    defer stop()
    <-ctx.Done()

    slog.Info("shutting down...")
    shutdownCtx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
    defer cancel()

    if err := srv.Shutdown(shutdownCtx); err != nil {
        slog.Error("shutdown error", "error", err)
    }
    slog.Info("server stopped")
}
```

### Probar el servidor:

```bash
# Inicializar modulo
go mod init infra-api
go mod tidy

# Ejecutar
go run main.go

# ═══════════════════════════════════════════════════════════════
# TESTS CON CURL
# ═══════════════════════════════════════════════════════════════

# Health check (siempre OK)
curl -s http://localhost:8080/health | jq .
# {"status":"ok","time":"2026-04-04T10:30:00Z"}

# Listar servidores (seed data)
curl -s http://localhost:8080/api/v1/servers | jq .
# {"items":[...], "total":3}

# Crear servidor — exito
curl -s -X POST http://localhost:8080/api/v1/servers \
  -H "Content-Type: application/json" \
  -d '{"name":"api-staging-01","cpu":2,"memory_mb":4096,"region":"us-west-2"}' | jq .
# {"id":"srv-0004","name":"api-staging-01",...,"status":"provisioning"}

# Crear servidor — validacion fallida (multiples errores)
curl -s -X POST http://localhost:8080/api/v1/servers \
  -H "Content-Type: application/json" \
  -d '{"name":"","cpu":0,"memory_mb":32,"region":"mars-1"}' | jq .
# {"error":{"code":"VALIDATION_ERROR","message":"validation failed",
#   "details":[
#     {"field":"name","message":"is required"},
#     {"field":"cpu","message":"must be at least 1"},
#     {"field":"memory_mb","message":"must be at least 128"},
#     {"field":"region","message":"invalid region","value":"mars-1"}
#   ]}}

# Crear servidor — duplicado
curl -s -X POST http://localhost:8080/api/v1/servers \
  -H "Content-Type: application/json" \
  -d '{"name":"web-prod-01","cpu":2,"memory_mb":4096,"region":"us-east-1"}' | jq .
# {"error":{"code":"CONFLICT","message":"server 'web-prod-01' already exists"}}

# Obtener servidor — exito
curl -s http://localhost:8080/api/v1/servers/srv-0001 | jq .
# {"id":"srv-0001","name":"web-prod-01",...}

# Obtener servidor — no existe
curl -s http://localhost:8080/api/v1/servers/srv-9999 | jq .
# {"error":{"code":"NOT_FOUND","message":"server 'srv-9999' not found"}}

# JSON invalido
curl -s -X POST http://localhost:8080/api/v1/servers \
  -H "Content-Type: application/json" \
  -d '{invalid' | jq .
# {"error":{"code":"BAD_REQUEST","message":"malformed JSON at position 1"}}

# Campo desconocido
curl -s -X POST http://localhost:8080/api/v1/servers \
  -H "Content-Type: application/json" \
  -d '{"name":"test-01","cpu":2,"memory_mb":4096,"region":"us-east-1","unknown":"field"}' | jq .
# {"error":{"code":"BAD_REQUEST","message":"unknown field \"unknown\""}}

# Body vacio
curl -s -X POST http://localhost:8080/api/v1/servers \
  -H "Content-Type: application/json" | jq .
# {"error":{"code":"BAD_REQUEST","message":"request body is empty"}}

# Eliminar servidor
curl -s -X DELETE http://localhost:8080/api/v1/servers/srv-0001 -w "\n%{http_code}"
# 204

# Eliminar servidor que no existe
curl -s -X DELETE http://localhost:8080/api/v1/servers/srv-0001 | jq .
# {"error":{"code":"NOT_FOUND","message":"server 'srv-0001' not found"}}

# Probar panic recovery
curl -s http://localhost:8080/api/v1/panic | jq .
# {"error":{"code":"INTERNAL_ERROR","message":"an unexpected error occurred"}}
# (El servidor NO crashea — el middleware Recovery atrapa el panic)

# Verificar request_id en headers
curl -s -i http://localhost:8080/api/v1/servers/srv-9999
# X-Request-ID: 4a3b2c1d0e9f8a7b
# {"error":{"code":"NOT_FOUND",...,"request_id":"4a3b2c1d0e9f8a7b"}}

# Enviar tu propio request ID (para correlacion con otros sistemas)
curl -s -H "X-Request-ID: my-trace-123" http://localhost:8080/api/v1/servers | jq .
# La respuesta incluye request_id: "my-trace-123"
```

### Analisis del programa:

```
┌──────────────────────────────────────────────────────────────────────────┐
│             PATRONES DE ERROR HANDLING USADOS EN EL PROGRAMA             │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  1. AppHandler (handler que retorna error)                              │
│     → Todos los handlers API retornan error                             │
│     → UN punto central de conversion error→JSON (writeError)            │
│                                                                          │
│  2. APIError tipado                                                     │
│     → Status code, code, message, details, internal                     │
│     → Constructores: errBadRequest, errNotFound, errConflict...         │
│                                                                          │
│  3. Errores de dominio separados de HTTP                                │
│     → DomainError con Kind (sentinel) + Err (causa)                     │
│     → mapDomainError traduce dominio → HTTP                             │
│                                                                          │
│  4. Validacion estructurada                                             │
│     → Validator acumula TODOS los errores de campos                     │
│     → El cliente recibe la lista completa de errores                    │
│                                                                          │
│  5. decodeJSON con errores precisos                                     │
│     → Detecta: syntax error, type error, max body, EOF, unknown field   │
│     → Cada caso produce un mensaje especifico para el cliente           │
│                                                                          │
│  6. Middleware chain                                                    │
│     → Recovery: panic → 500 JSON (no crash)                             │
│     → RequestID: genera/propaga ID para correlacion                     │
│     → Logging: request/response con latencia (structured logging)       │
│     → Timeout: deadline global por request                              │
│                                                                          │
│  7. Seguridad de errores                                                │
│     → 4xx: detalles al cliente (su error)                               │
│     → 5xx: mensaje generico al cliente, detalles solo en logs           │
│     → Internal error nunca se serializa en JSON                         │
│                                                                          │
│  8. Graceful shutdown                                                   │
│     → signal.NotifyContext + srv.Shutdown                               │
│     → Requests en vuelo se completan antes de cerrar                    │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 14. Ejercicios

### Ejercicio 1: API CRUD Completa con Relaciones

Extiende el programa completo con:

```
Requisitos:
- Agregar recurso "Deployment" que referencia un Server (relacion 1:N)
- POST /api/v1/servers/{id}/deployments (crear deployment en un server)
  - Si el server no existe → 404
  - Si el server esta en status "decommissioned" → 409 Conflict
  - Validar: image (required, formato registry/image:tag), replicas (1-100)
- GET /api/v1/servers/{id}/deployments (listar deployments de un server)
- DELETE /api/v1/servers/{id}/deployments/{deploy_id}
  - Solo si el deployment esta en status "stopped"
  - Si esta "running" → 409 con mensaje "stop the deployment first"
- Implementar middleware de autenticacion simple (API key en header X-API-Key)
  - Key invalida → 401 con detalles del formato esperado
  - Key valida pero sin permiso de delete → 403
- Todos los errores deben seguir el formato ErrorResponse estandar
```

### Ejercicio 2: Middleware de Metricas y Error Budget

```
Requisitos:
- Middleware que registra metricas de cada request:
  - Counter de requests por (method, path, status_code)
  - Histogram de latencia por (method, path)
  - Counter de errores por (code — NOT_FOUND, INTERNAL_ERROR, etc.)
- Endpoint GET /metrics que expone todas las metricas en JSON
- Implementar "error budget": si el % de 5xx supera un umbral (ej: 1%),
  el endpoint /health empieza a retornar 503 (para que el load balancer
  deje de enviar trafico)
- Implementar rate limiting per-IP con respuestas 429 estandarizadas
  - Header Retry-After con el tiempo de espera
  - Header X-RateLimit-Remaining con requests restantes
```

### Ejercicio 3: API con Batch Operations

```
Requisitos:
- POST /api/v1/servers/batch — crear multiples servidores en una request
  - Body: {"servers": [{...}, {...}, ...]}
  - Validar CADA servidor individualmente
  - Si alguno falla validacion, rechazar TODO el batch (atomico)
  - Si algunos fallan al insertar (duplicado), retornar partial success:
    HTTP 207 Multi-Status con resultado por item:
    {"results": [
      {"index": 0, "status": "created", "server": {...}},
      {"index": 1, "status": "error", "error": {"code": "CONFLICT", ...}},
      {"index": 2, "status": "created", "server": {...}}
    ]}
- DELETE /api/v1/servers/batch — eliminar multiples servidores
  - Body: {"ids": ["srv-0001", "srv-0002"]}
  - Mismo patron de respuesta parcial (207)
- Limite maximo de batch: 100 items
- Usar errgroup para procesar items en paralelo (SetLimit(10))
- Cada item debe tener su propio error handling (un fallo no cancela otros)
```

---

## Resumen

```
┌──────────────────────────────────────────────────────────────────────────┐
│               ERROR HANDLING EN APIs — RESUMEN COMPLETO                  │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  PROBLEMA:                                                               │
│  ├─ http.Handler no retorna error → manejo manual y repetitivo          │
│  ├─ Cada handler repite: Content-Type, WriteHeader, json.Encode, log   │
│  ├─ Formato de error inconsistente entre handlers                       │
│  └─ Errores internos se filtran al cliente (seguridad)                  │
│                                                                          │
│  SOLUCION — 6 PIEZAS:                                                   │
│                                                                          │
│  1. AppHandler: func(w, r) error → un punto de error handling          │
│  2. APIError: tipo con Status, Code, Message, Details, Internal         │
│  3. Mapeo dominio→HTTP: mapDomainError() — separar capas               │
│  4. Validacion: Validator que acumula TODOS los errores de campo        │
│  5. Middlewares: Recovery, RequestID, Logging, Timeout                  │
│  6. Response helpers: writeJSON, writeError, decodeJSON                 │
│                                                                          │
│  REGLAS:                                                                │
│  ├─ 4xx → detalles al cliente (su error, necesita saber)               │
│  ├─ 5xx → mensaje generico al cliente, detalles en logs                │
│  ├─ Siempre Content-Type: application/json                             │
│  ├─ Siempre request_id (para correlacion en logs)                      │
│  ├─ Recovery protege contra panics (no crash)                          │
│  └─ Timeout previene requests infinitos                                │
│                                                                          │
│  CAPAS:                                                                  │
│  ├─ Repository → produce errores de DB (sql.ErrNoRows, etc.)           │
│  ├─ Service    → wrappea en DomainError (ErrNotFound, ErrConflict)     │
│  └─ Handler    → mapea DomainError a APIError (404, 409, 500)          │
│                                                                          │
│  vs C: todo manual (JSON, HTTP, logging, error format)                  │
│  vs Rust: Result<T, AppError> + IntoResponse (compile-time verified)   │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: C07/S02 - Patrones Avanzados, T03 - Comparacion con Rust — error vs Result<T,E>, wrapping vs From trait, ausencia de ? operator
