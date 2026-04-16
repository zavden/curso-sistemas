# T03 — Build tags y ldflags

## 1. ¿Qué son build tags y ldflags?

Son dos mecanismos independientes que controlan **qué se compila** y
**qué información se inyecta** en el binario. Juntos permiten crear
builds condicionales y binarios que saben su propia versión, commit
y fecha de compilación.

```
    ┌──────────────────────────────────────────────────────────────┐
    │           Build tags y ldflags — dos mecanismos              │
    │                                                              │
    │  BUILD TAGS (//go:build)                                     │
    │  ┌───────────────────────────────────────────────────────┐   │
    │  │ Controlan QUÉ ARCHIVOS se incluyen en la compilación  │   │
    │  │                                                       │   │
    │  │ //go:build linux                                      │   │
    │  │ → solo compila este archivo en Linux                  │   │
    │  │                                                       │   │
    │  │ //go:build !production                                │   │
    │  │ → solo compila si NO se pasa -tags production         │   │
    │  │                                                       │   │
    │  │ Se evalúan en TIEMPO DE COMPILACIÓN.                  │   │
    │  │ Los archivos excluidos ni se parsean.                 │   │
    │  └───────────────────────────────────────────────────────┘   │
    │                                                              │
    │  LDFLAGS (-ldflags "-X ...")                                  │
    │  ┌───────────────────────────────────────────────────────┐   │
    │  │ Inyectan VALORES en variables del binario compilado   │   │
    │  │                                                       │   │
    │  │ -ldflags "-X main.version=v1.2.3"                     │   │
    │  │ → la variable main.version tendrá el valor "v1.2.3"   │   │
    │  │                                                       │   │
    │  │ -ldflags "-s -w"                                      │   │
    │  │ → strip symbol table y DWARF debug info               │   │
    │  │                                                       │   │
    │  │ Se aplican en TIEMPO DE LINKEO (después de compilar). │   │
    │  └───────────────────────────────────────────────────────┘   │
    └──────────────────────────────────────────────────────────────┘
```

---

## 2. Build tags — compilación condicional

### 2.1 Sintaxis moderna: //go:build

```go
// La directiva //go:build (Go 1.17+) controla si un archivo
// se incluye en la compilación.

// DEBE ser la primera línea del archivo (antes de package).
// Se puede poner un comentario de copyright antes, pero nada más.

//go:build linux

package sysinfo

// Este archivo SOLO se compila cuando GOOS=linux.
// En macOS, Windows, etc. este archivo no existe para el compilador.
```

```go
// Sintaxis antigua (pre-Go 1.17) — aún funciona pero evitar:
// +build linux

// Diferencias:
// - //go:build usa expresiones booleanas legibles
// - // +build usa convenciones de espacios y comas (confuso)
// - Si ambas están presentes, //go:build tiene prioridad
// - gofmt sincroniza automáticamente ambas si las dos están presentes

// Siempre usa //go:build en código nuevo.
```

### 2.2 Operadores lógicos

```go
// AND — todas las condiciones deben cumplirse:
//go:build linux && amd64

// OR — al menos una condición:
//go:build linux || darwin

// NOT — excluir una condición:
//go:build !windows

// Combinaciones complejas (paréntesis):
//go:build (linux || darwin) && amd64

// Múltiples negaciones:
//go:build !windows && !plan9 && !js

// Ejemplo real: código para Unix-like systems:
//go:build linux || darwin || freebsd || openbsd || netbsd
```

### 2.3 Condiciones disponibles automáticamente

```bash
# Go define automáticamente estas condiciones (sin que hagas nada):

# GOOS — el sistema operativo:
# linux, darwin, windows, freebsd, openbsd, netbsd, dragonfly,
# solaris, aix, plan9, js, wasip1, ios, android

# GOARCH — la arquitectura:
# amd64, arm64, arm, 386, riscv64, mips, mips64, mipsle, mips64le,
# ppc64, ppc64le, s390x, loong64, wasm

# Compilador:
# gc (compilador estándar), gccgo (frontend GCC)

# CGO:
# cgo (true si CGO_ENABLED=1)

# Versión de Go (desde Go 1.21):
# go1.21, go1.22, go1.23, etc.
# //go:build go1.22 → solo se compila con Go 1.22+
```

### 2.4 Build tags vs file naming

```bash
# Go tiene DOS formas de compilación condicional:
# 1. Build tags (//go:build) — flexibles, expresiones booleanas
# 2. File naming (_GOOS.go, _GOARCH.go) — solo OS/ARCH, simple

# Equivalencias:

# Archivo: config_linux.go
# Equivale a: //go:build linux

# Archivo: config_linux_amd64.go
# Equivale a: //go:build linux && amd64

# ¿Cuándo usar cada una?

# File naming:
#   - Solo necesitas filtrar por GOOS y/o GOARCH
#   - Quieres que sea obvio al ver el nombre del archivo
#   - Ejemplo: syscall_linux.go, syscall_darwin.go

# Build tags:
#   - Necesitas lógica booleana (OR, NOT, combinaciones)
#   - Filtras por algo que no es OS/ARCH (tags custom, cgo, versión)
#   - Ejemplo: //go:build (linux || darwin) && !cgo
#   - Ejemplo: //go:build integration

# AMBOS se pueden combinar:
# sysinfo_linux.go con //go:build !cgo
# → solo en Linux Y sin cgo
```

---

## 3. Build tags personalizados

Puedes definir tus propios tags que se activan con `-tags`.
Esto es extremadamente útil para builds condicionales.

### 3.1 Tag personalizado básico

```go
// debug.go — solo se incluye con -tags debug
//go:build debug

package main

import "fmt"

func init() {
    fmt.Println("[DEBUG MODE ENABLED]")
}

func debugLog(msg string) {
    fmt.Printf("[DEBUG] %s\n", msg)
}
```

```go
// debug_stub.go — se incluye cuando NO hay tag debug
//go:build !debug

package main

// debugLog es un no-op en builds de producción.
// El compilador puede inlinear y eliminar la llamada vacía.
func debugLog(msg string) {}
```

```go
// main.go — usa debugLog sin importar el tag
package main

import "fmt"

func main() {
    debugLog("starting application")
    fmt.Println("Hello, World!")
    debugLog("application started")
}
```

```bash
# Build de producción (sin debug):
go build -o myapp ./cmd/myapp
./myapp
# Hello, World!

# Build de desarrollo (con debug):
go build -tags debug -o myapp-debug ./cmd/myapp
./myapp-debug
# [DEBUG MODE ENABLED]
# [DEBUG] starting application
# Hello, World!
# [DEBUG] application started

# El binario de producción NO contiene el código de debug.
# No es un if/else en runtime — el código literalmente no existe.
```

### 3.2 Tags para entornos (development, staging, production)

```go
// config_dev.go
//go:build dev

package config

const (
    DefaultLogLevel = "debug"
    DefaultDBHost   = "localhost"
    DefaultDBPort   = 5432
    DefaultDBName   = "myapp_dev"
    EnablePprof     = true
    EnableSwagger   = true
)
```

```go
// config_staging.go
//go:build staging

package config

const (
    DefaultLogLevel = "info"
    DefaultDBHost   = "db-staging.internal"
    DefaultDBPort   = 5432
    DefaultDBName   = "myapp_staging"
    EnablePprof     = true
    EnableSwagger   = false
)
```

```go
// config_prod.go
//go:build production

package config

const (
    DefaultLogLevel = "warn"
    DefaultDBHost   = "db-prod.internal"
    DefaultDBPort   = 5432
    DefaultDBName   = "myapp"
    EnablePprof     = false
    EnableSwagger   = false
)
```

```go
// config_default.go — fallback cuando no se pasa ningún tag
//go:build !dev && !staging && !production

package config

const (
    DefaultLogLevel = "info"
    DefaultDBHost   = "localhost"
    DefaultDBPort   = 5432
    DefaultDBName   = "myapp"
    EnablePprof     = false
    EnableSwagger   = false
)
```

```bash
# Compilar para cada entorno:
go build -tags dev -o myapp-dev ./cmd/myapp
go build -tags staging -o myapp-staging ./cmd/myapp
go build -tags production -o myapp-prod ./cmd/myapp

# Sin tag → usa config_default.go
go build -o myapp ./cmd/myapp

# ⚠️ Consideración: esta técnica embebe la configuración en el binario.
# Es útil para valores por defecto y feature flags,
# pero la configuración real debería venir de env vars o config files.
# Patrón: build tags para defaults, env vars para overrides.
```

### 3.3 Feature flags con build tags

```go
// feature_metrics.go — incluye integración con Prometheus
//go:build metrics

package main

import (
    "net/http"

    "github.com/prometheus/client_golang/prometheus/promhttp"
)

func init() {
    http.Handle("/metrics", promhttp.Handler())
}

// Con tag metrics: el binario incluye Prometheus y expone /metrics
// Sin tag: el binario NO incluye la dependencia de Prometheus
// → binario más pequeño, menos superficie de ataque
```

```go
// feature_metrics_stub.go
//go:build !metrics

package main

// Sin métricas — nada que inicializar.
```

```bash
# Build sin métricas (más ligero):
go build -o myapp-light ./cmd/myapp

# Build con métricas:
go build -tags metrics -o myapp-full ./cmd/myapp

# Comparar tamaños:
ls -lh myapp-light myapp-full
# myapp-light: 8.2MB
# myapp-full:  12.1MB  ← incluye todo Prometheus
```

### 3.4 Múltiples tags

```bash
# Puedes pasar múltiples tags separados por coma o espacio:

# Coma (método recomendado desde Go 1.17):
go build -tags "debug,metrics" -o myapp ./cmd/myapp

# Espacio (también funciona):
go build -tags "debug metrics" -o myapp ./cmd/myapp

# Cada tag activa los archivos que lo requieren:
# -tags "debug,metrics" activa:
#   //go:build debug     ← debug.go
#   //go:build metrics   ← feature_metrics.go
```

---

## 4. Tags para tests de integración

Uno de los usos más importantes de build tags para SysAdmin:
separar tests unitarios de tests de integración.

### 4.1 Patrón: tests de integración con tag

```go
// internal/db/postgres_integration_test.go
//go:build integration

package db

import (
    "database/sql"
    "os"
    "testing"

    _ "github.com/lib/pq"
)

func TestPostgresConnection(t *testing.T) {
    dsn := os.Getenv("TEST_DATABASE_URL")
    if dsn == "" {
        t.Skip("TEST_DATABASE_URL not set")
    }

    db, err := sql.Open("postgres", dsn)
    if err != nil {
        t.Fatalf("failed to open database: %v", err)
    }
    defer db.Close()

    if err := db.Ping(); err != nil {
        t.Fatalf("failed to ping database: %v", err)
    }
}

func TestCreateAndFetchUser(t *testing.T) {
    dsn := os.Getenv("TEST_DATABASE_URL")
    if dsn == "" {
        t.Skip("TEST_DATABASE_URL not set")
    }

    // ... test que requiere una base de datos real ...
}
```

```bash
# Tests unitarios (rápidos, sin dependencias externas):
go test ./...

# Tests de integración (requieren PostgreSQL, Redis, etc.):
go test -tags integration ./...

# Ambos:
go test -tags integration -race ./...

# En CI:
# Job 1 (rápido): go test ./...
# Job 2 (lento):  go test -tags integration ./... (con docker-compose)
```

### 4.2 Tests E2E, smoke tests, y otros tags

```go
// test_e2e_test.go
//go:build e2e

package main

import (
    "net/http"
    "os"
    "testing"
)

func TestHealthEndpoint(t *testing.T) {
    baseURL := os.Getenv("TEST_BASE_URL")
    if baseURL == "" {
        baseURL = "http://localhost:8080"
    }

    resp, err := http.Get(baseURL + "/health")
    if err != nil {
        t.Fatalf("health check failed: %v", err)
    }
    defer resp.Body.Close()

    if resp.StatusCode != http.StatusOK {
        t.Errorf("expected 200, got %d", resp.StatusCode)
    }
}
```

```bash
# Organización de tags de tests:
# (sin tag)     → tests unitarios (siempre corren)
# integration   → tests que requieren servicios externos
# e2e           → tests end-to-end contra un entorno desplegado
# smoke         → tests básicos post-deploy
# load          → tests de carga

# CI pipeline:
# 1. go test ./...                              (unitarios: ~30s)
# 2. go test -tags integration ./...            (integración: ~5min)
# 3. deploy to staging
# 4. go test -tags smoke ./...                  (smoke: ~1min)
# 5. go test -tags e2e ./...                    (e2e: ~10min)
```

### 4.3 Makefile con tags de testing

```makefile
.PHONY: test
test: ## tests unitarios
	go test -race -count=1 -timeout=2m ./...

.PHONY: test-integration
test-integration: ## tests de integración (requiere servicios)
	go test -race -count=1 -timeout=10m -tags integration ./...

.PHONY: test-e2e
test-e2e: ## tests end-to-end
	go test -count=1 -timeout=15m -tags e2e ./...

.PHONY: test-all
test-all: ## todos los tests
	go test -race -count=1 -timeout=15m -tags "integration,e2e" ./...

.PHONY: test-smoke
test-smoke: ## smoke tests post-deploy
	go test -count=1 -timeout=2m -tags smoke ./...
```

---

## 5. Tag ignore — excluir archivos de la compilación

```go
// El tag especial "ignore" excluye un archivo de toda compilación.
// Útil para archivos de referencia, templates, o código en progreso.

//go:build ignore

package main

// Este archivo NUNCA se compila, ni con ninguna combinación de tags.
// Es el equivalente a comentar todo el archivo.

// Usos legítimos:
// 1. Código de ejemplo que no debe compilarse como parte del proyecto
// 2. Templates usados por go generate (ej: stringer templates)
// 3. Código work-in-progress que no está listo
// 4. Archivos de documentación con ejemplos Go (raro)
```

```go
// Otro uso: archivos que son scripts Go (go run):
//go:build ignore

// Este archivo se ejecuta con: go run tools/migrate.go
// pero NO se incluye en el build normal del proyecto.

package main

import "fmt"

func main() {
    fmt.Println("Running migration tool...")
}
```

```bash
# go run ejecuta el archivo directamente (ignora //go:build ignore):
go run tools/migrate.go

# go build NO lo incluye:
go build ./...
# tools/migrate.go no se compila
```

---

## 6. Tag cgo — código condicional según CGO

```go
// cgo_enabled.go — solo se compila cuando cgo está habilitado
//go:build cgo

package resolver

/*
#include <netdb.h>
#include <stdlib.h>
*/
import "C"

import (
    "fmt"
    "unsafe"
)

func ResolveHost(host string) (string, error) {
    chost := C.CString(host)
    defer C.free(unsafe.Pointer(chost))

    var hints C.struct_addrinfo
    hints.ai_family = C.AF_INET
    var result *C.struct_addrinfo

    ret := C.getaddrinfo(chost, nil, &hints, &result)
    if ret != 0 {
        return "", fmt.Errorf("getaddrinfo failed: %d", ret)
    }
    defer C.freeaddrinfo(result)

    // ... procesar resultado ...
    return "resolved via cgo", nil
}
```

```go
// cgo_disabled.go — implementación pure-Go cuando cgo no está disponible
//go:build !cgo

package resolver

import "net"

func ResolveHost(host string) (string, error) {
    addrs, err := net.LookupHost(host)
    if err != nil {
        return "", err
    }
    if len(addrs) == 0 {
        return "", fmt.Errorf("no addresses for %s", host)
    }
    return addrs[0], nil
}
```

```bash
# Con cgo → usa getaddrinfo de libc:
CGO_ENABLED=1 go build -o myapp ./cmd/myapp

# Sin cgo → usa resolver Go puro:
CGO_ENABLED=0 go build -o myapp ./cmd/myapp

# El tag "cgo" se define automáticamente por el valor de CGO_ENABLED.
# No necesitas pasarlo con -tags.
```

---

## 7. Tags de versión de Go

Desde Go 1.21, puedes usar tags basados en la versión de Go
para utilizar features nuevas con fallback para versiones antiguas.

```go
// slices_go122.go — usa slices.SortFunc (Go 1.22+)
//go:build go1.22

package utils

import "slices"

func SortStrings(s []string) {
    slices.Sort(s)
}
```

```go
// slices_legacy.go — fallback para Go 1.21 y anteriores
//go:build !go1.22

package utils

import "sort"

func SortStrings(s []string) {
    sort.Strings(s)
}
```

```bash
# Si compilas con Go 1.22+: usa slices_go122.go
# Si compilas con Go 1.21: usa slices_legacy.go

# Esto es útil para bibliotecas que soportan múltiples versiones de Go.
# Para aplicaciones propias donde controlas la versión, no es necesario.
```

---

## 8. ldflags — inyectar valores en el binario

`-ldflags` pasa flags al linker de Go. El uso más importante
es `-X` para inyectar valores en variables string del código.

### 8.1 -X: inyectar versión, commit y fecha

```go
// cmd/myapp/main.go
package main

import "fmt"

// Estas variables se inyectan en tiempo de compilación con -ldflags -X.
// Si no se inyectan, mantienen su zero value (string vacío "").
var (
    version   = "dev"
    commit    = "unknown"
    buildTime = "unknown"
)

func main() {
    fmt.Printf("myapp %s (commit: %s, built: %s)\n", version, commit, buildTime)
}
```

```bash
# Compilar inyectando valores:
go build -ldflags="-X main.version=v1.2.3 \
    -X main.commit=$(git rev-parse --short HEAD) \
    -X main.buildTime=$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
    -o myapp ./cmd/myapp

./myapp
# myapp v1.2.3 (commit: abc1234, built: 2024-12-15T10:30:00Z)

# Sin ldflags:
go build -o myapp ./cmd/myapp
./myapp
# myapp dev (commit: unknown, built: unknown)
```

### 8.2 Sintaxis de -X

```bash
# La sintaxis completa es:
# -X importpath.varname=value

# Variable en main:
-X main.version=v1.2.3

# Variable en otro paquete:
-X example.com/myapp/internal/build.Version=v1.2.3

# Múltiples variables:
-X main.version=v1.2.3 -X main.commit=abc1234 -X main.date=2024-12-15

# Con espacios en el valor (usar comillas):
-X 'main.buildInfo=built by CI on server-01'

# ⚠️ Restricciones de -X:
# 1. Solo funciona con variables de tipo string
# 2. La variable debe existir en el código (no la crea)
# 3. La variable debe ser de nivel paquete (no local)
# 4. La variable puede ser exportada o no exportada
# 5. No funciona con constantes (const) — solo variables (var)
```

### 8.3 Patrón completo: paquete version

```go
// internal/build/version.go
package build

import "fmt"

// Variables inyectadas por ldflags.
var (
    Version   = "dev"
    Commit    = "unknown"
    BuildTime = "unknown"
    GoVersion = "unknown"
)

// String returns a human-readable version string.
func String() string {
    return fmt.Sprintf("%s (commit: %s, built: %s, go: %s)",
        Version, Commit, BuildTime, GoVersion)
}

// Short returns just the version tag.
func Short() string {
    return Version
}
```

```go
// cmd/myapp/main.go
package main

import (
    "fmt"
    "os"

    "example.com/myapp/internal/build"
)

func main() {
    // --version flag:
    if len(os.Args) > 1 && (os.Args[1] == "--version" || os.Args[1] == "-v") {
        fmt.Println(build.String())
        os.Exit(0)
    }

    fmt.Printf("Starting myapp %s...\n", build.Short())
    // ... aplicación ...
}
```

```bash
# Compilar con todas las variables:
go build -ldflags="\
    -X example.com/myapp/internal/build.Version=v1.2.3 \
    -X example.com/myapp/internal/build.Commit=$(git rev-parse --short HEAD) \
    -X example.com/myapp/internal/build.BuildTime=$(date -u +%Y-%m-%dT%H:%M:%SZ) \
    -X example.com/myapp/internal/build.GoVersion=$(go version | awk '{print $3}')" \
    -o myapp ./cmd/myapp

./myapp --version
# v1.2.3 (commit: abc1234, built: 2024-12-15T10:30:00Z, go: go1.23.0)
```

### 8.4 Combinar ldflags: -X con -s -w

```bash
# -s = strip symbol table
# -w = strip DWARF debug info
# -X = inyectar valores

# Combinación completa para producción:
go build -ldflags="-s -w \
    -X main.version=v1.2.3 \
    -X main.commit=abc1234 \
    -X main.buildTime=2024-12-15T10:30:00Z" \
    -o myapp ./cmd/myapp

# Equivalente con variable para legibilidad:
LDFLAGS="-s -w \
    -X main.version=$(git describe --tags --always --dirty) \
    -X main.commit=$(git rev-parse --short HEAD) \
    -X main.buildTime=$(date -u +%Y-%m-%dT%H:%M:%SZ)"

go build -ldflags="$LDFLAGS" -o myapp ./cmd/myapp
```

---

## 9. ldflags avanzados

### 9.1 -extldflags — flags para el linker externo

```bash
# -extldflags pasa flags al linker de C (ld) cuando cgo está habilitado.

# Crear binario estático con cgo:
go build -ldflags='-extldflags "-static"' -o myapp ./cmd/myapp

# Combinación completa: static + strip + version:
go build -ldflags='-s -w -extldflags "-static" \
    -X main.version=v1.2.3' -o myapp ./cmd/myapp

# Para PIE (Position Independent Executable):
go build -ldflags='-extldflags "-pie"' -o myapp ./cmd/myapp

# Múltiples flags externos:
go build -ldflags='-extldflags "-static -lpthread"' -o myapp ./cmd/myapp
```

### 9.2 Todos los flags del linker de Go

```bash
# Referencia completa de flags del linker (go tool link):

# -s          → strip symbol table
# -w          → strip DWARF debug info
# -X          → set string variable value
# -extldflags → pass flags to external linker
# -buildid    → set build ID (para reproducibilidad)
# -linkmode   → internal (Go linker) o external (system linker)
# -race       → enable race detector data

# Ver todos:
go tool link -help 2>&1 | head -50

# Uso avanzado — forzar linkeo interno (Go linker):
go build -ldflags='-linkmode internal' -o myapp ./cmd/myapp

# Uso avanzado — forzar linkeo externo (system linker):
go build -ldflags='-linkmode external' -o myapp ./cmd/myapp
# Necesario cuando cgo requiere features del linker de sistema
```

### 9.3 -buildid: control de reproducibilidad

```bash
# El build ID es un identificador único del binario.
# Go lo genera automáticamente basándose en el contenido.

# Ver el build ID:
go tool buildid myapp
# rQm5vLf.../abc123.../xyz789.../final123

# Establecer un build ID personalizado (raro, pero posible):
go build -ldflags='-buildid=' -o myapp ./cmd/myapp
# Build ID vacío → útil para builds reproducibles bit-a-bit

# En la práctica, no necesitas tocar el build ID.
# Go lo maneja automáticamente.
```

---

## 10. Makefile completo con ldflags y tags

```makefile
# Makefile — build con inyección de versión y tags

APP_NAME    := myapp
MODULE      := example.com/myapp
VERSION     := $(shell git describe --tags --always --dirty 2>/dev/null || echo "dev")
COMMIT      := $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")
BUILD_TIME  := $(shell date -u '+%Y-%m-%dT%H:%M:%SZ')
GO_VERSION  := $(shell go version | awk '{print $$3}')
BRANCH      := $(shell git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")

# Paquete donde están las variables de versión:
VERSION_PKG := $(MODULE)/internal/build

LDFLAGS := -s -w \
    -X $(VERSION_PKG).Version=$(VERSION) \
    -X $(VERSION_PKG).Commit=$(COMMIT) \
    -X $(VERSION_PKG).BuildTime=$(BUILD_TIME) \
    -X $(VERSION_PKG).GoVersion=$(GO_VERSION) \
    -X $(VERSION_PKG).Branch=$(BRANCH)

.PHONY: help
help: ## mostrar esta ayuda
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | \
	    awk 'BEGIN {FS = ":.*?## "}; {printf "  %-20s %s\n", $$1, $$2}'

.PHONY: build
build: ## build de producción
	CGO_ENABLED=0 go build -trimpath -ldflags='$(LDFLAGS)' \
	    -o bin/$(APP_NAME) ./cmd/$(APP_NAME)

.PHONY: build-dev
build-dev: ## build de desarrollo (con debug y pprof)
	go build -tags "dev,metrics" -ldflags='-X $(VERSION_PKG).Version=$(VERSION)' \
	    -o bin/$(APP_NAME)-dev ./cmd/$(APP_NAME)

.PHONY: build-debug
build-debug: ## build con debug info y log verbose
	go build -tags debug -gcflags="all=-N -l" \
	    -ldflags='-X $(VERSION_PKG).Version=$(VERSION)-debug' \
	    -o bin/$(APP_NAME)-debug ./cmd/$(APP_NAME)

.PHONY: build-staging
build-staging: ## build para staging
	CGO_ENABLED=0 go build -trimpath -tags staging \
	    -ldflags='$(LDFLAGS)' -o bin/$(APP_NAME)-staging ./cmd/$(APP_NAME)

.PHONY: build-prod
build-prod: ## build para producción (con tag production)
	CGO_ENABLED=0 go build -trimpath -tags production \
	    -ldflags='$(LDFLAGS)' -o bin/$(APP_NAME) ./cmd/$(APP_NAME)

.PHONY: test
test: ## tests unitarios
	go test -race -count=1 -timeout=2m ./...

.PHONY: test-integration
test-integration: ## tests de integración
	go test -race -count=1 -timeout=10m -tags integration ./...

.PHONY: test-all
test-all: test test-integration ## todos los tests

.PHONY: version
version: ## mostrar información de versión
	@echo "Version:    $(VERSION)"
	@echo "Commit:     $(COMMIT)"
	@echo "Branch:     $(BRANCH)"
	@echo "Build Time: $(BUILD_TIME)"
	@echo "Go Version: $(GO_VERSION)"

.PHONY: clean
clean: ## limpiar artefactos
	rm -rf bin/ dist/
```

```bash
# Uso:
make build                    # build de producción
make build-dev                # build de desarrollo (con métricas y debug)
make build-debug              # build para debugging con delve
make build-staging            # build para staging
make build-prod               # build para producción con tag
make version                  # ver info de versión

make test                     # tests unitarios
make test-integration         # tests de integración
make test-all                 # todos los tests
```

---

## 11. runtime/debug.ReadBuildInfo() — alternativa a ldflags

Desde Go 1.18, Go embede información de build automáticamente
en el binario. `debug.ReadBuildInfo()` permite leerla sin ldflags.

```go
package main

import (
    "fmt"
    "runtime/debug"
)

func main() {
    info, ok := debug.ReadBuildInfo()
    if !ok {
        fmt.Println("No build info available")
        return
    }

    // Información del módulo:
    fmt.Printf("Module:     %s\n", info.Main.Path)
    fmt.Printf("Go version: %s\n", info.GoVersion)

    // Información de VCS (Git):
    for _, s := range info.Settings {
        switch s.Key {
        case "vcs.revision":
            fmt.Printf("Commit:     %s\n", s.Value)
        case "vcs.time":
            fmt.Printf("Commit at:  %s\n", s.Value)
        case "vcs.modified":
            fmt.Printf("Dirty:      %s\n", s.Value)
        case "GOOS":
            fmt.Printf("GOOS:       %s\n", s.Value)
        case "GOARCH":
            fmt.Printf("GOARCH:     %s\n", s.Value)
        case "CGO_ENABLED":
            fmt.Printf("CGO:        %s\n", s.Value)
        }
    }

    // Dependencias:
    fmt.Println("\nDependencies:")
    for _, dep := range info.Deps {
        fmt.Printf("  %s@%s\n", dep.Path, dep.Version)
    }
}
```

### 11.1 ReadBuildInfo vs ldflags: ¿cuándo usar cada uno?

```bash
# ReadBuildInfo (automático, sin esfuerzo):
# ✓ Commit hash completo
# ✓ Timestamp del commit
# ✓ Si el working tree está dirty
# ✓ Versión de Go
# ✓ GOOS, GOARCH, CGO_ENABLED
# ✓ Todas las dependencias con versión
# ✗ NO incluye un tag de versión semántica
# ✗ NO incluye fecha/hora de BUILD (solo del commit)
# ✗ Requiere .git presente durante el build

# ldflags -X (manual, más control):
# ✓ Puedes inyectar lo que quieras
# ✓ Tag de versión semántica (git describe --tags)
# ✓ Fecha/hora de build
# ✓ Nombre del builder (CI job ID, hostname, etc.)
# ✓ Funciona sin .git (puedes pasar valores manualmente)
# ✗ Requiere configurar el build command

# Recomendación para SysAdmin:
# Usar AMBOS. ReadBuildInfo como fallback, ldflags para versión semántica.
```

### 11.2 Patrón híbrido

```go
// internal/build/version.go
package build

import (
    "fmt"
    "runtime/debug"
)

// Inyectadas por ldflags:
var (
    Version   = ""
    Commit    = ""
    BuildTime = ""
)

// Info returns the complete build information.
func Info() string {
    v := Version
    c := Commit
    t := BuildTime

    // Fallback a ReadBuildInfo si ldflags no inyectó valores:
    if v == "" || c == "" {
        if info, ok := debug.ReadBuildInfo(); ok {
            if v == "" {
                v = info.Main.Version
                if v == "" || v == "(devel)" {
                    v = "dev"
                }
            }
            for _, s := range info.Settings {
                if c == "" && s.Key == "vcs.revision" {
                    c = s.Value
                    if len(c) > 8 {
                        c = c[:8]
                    }
                }
                if t == "" && s.Key == "vcs.time" {
                    t = s.Value
                }
            }
        }
    }

    return fmt.Sprintf("%s (commit: %s, built: %s)", v, c, t)
}
```

```bash
# Con ldflags (CI/producción):
go build -ldflags="-X example.com/myapp/internal/build.Version=v1.2.3 \
    -X example.com/myapp/internal/build.Commit=abc1234" ./cmd/myapp
./myapp --version
# v1.2.3 (commit: abc1234, built: 2024-12-15T10:30:00Z)

# Sin ldflags (desarrollo local — go run):
go run ./cmd/myapp --version
# dev (commit: abc12345, built: 2024-12-15T10:25:00Z)
# ↑ ReadBuildInfo proporcionó commit y timestamp automáticamente
```

---

## 12. -gcflags — flags del compilador

`-gcflags` pasa flags al compilador de Go (no al linker).
Son menos comunes que ldflags pero importantes para debugging
y optimización.

### 12.1 Desactivar optimizaciones para debugging

```bash
# Para que delve pueda inspeccionar todas las variables:
go build -gcflags="all=-N -l" -o myapp-debug ./cmd/myapp

# -N = sin optimizaciones
# -l = sin inlining
# all= = aplicar a todos los paquetes (no solo main)

# Sin "all=", solo aplica al paquete que compilas:
go build -gcflags="-N -l" -o myapp-debug ./cmd/myapp
# ↑ solo main sin optimizar, el resto optimizado

# Con "all=":
go build -gcflags="all=-N -l" -o myapp-debug ./cmd/myapp
# ↑ TODO sin optimizar (mejor para debugging)
```

### 12.2 Ver decisiones de optimización

```bash
# Ver qué funciones se inlinean:
go build -gcflags="-m" ./cmd/myapp 2>&1 | head -20
# ./cmd/myapp/main.go:15:6: can inline setupRoutes
# ./cmd/myapp/main.go:22:6: can inline handler
# ./cmd/myapp/main.go:30:6: inlining call to handler

# Más detalle:
go build -gcflags="-m -m" ./cmd/myapp 2>&1 | head -30
# Muestra POR QUÉ cada función se inlinea o no

# Ver escape analysis (¿va al heap o al stack?):
go build -gcflags="-m" ./cmd/myapp 2>&1 | grep "escapes to heap"
# ./cmd/myapp/main.go:18:17: &Config{...} escapes to heap
# Significa que esa variable se aloca en el heap (más lento, GC)

# Esto es útil para optimización de rendimiento:
# Si algo "escapes to heap" innecesariamente, puedes reestructurar
# el código para mantenerlo en el stack.
```

### 12.3 Generar assembly

```bash
# Ver el assembly generado:
go build -gcflags="-S" ./cmd/myapp 2>&1 | head -50

# Para una función específica:
go tool compile -S main.go 2>&1 | grep -A 20 '"".main'

# O usar objdump en el binario:
go tool objdump -s 'main\.main' myapp

# Esto es avanzado — útil para:
# - Verificar que el compilador optimiza correctamente
# - Entender el coste de operaciones específicas
# - Comparar código generado entre versiones de Go
```

---

## 13. -tags y ldflags en Dockerfile

### 13.1 Dockerfile con build tags

```dockerfile
# Dockerfile con build tags para producción

FROM golang:1.23-alpine AS builder

ARG VERSION=dev
ARG COMMIT=unknown
ARG BUILD_TAGS=production

WORKDIR /app
COPY go.mod go.sum ./
RUN go mod download && go mod verify
COPY . .

RUN CGO_ENABLED=0 GOOS=linux go build \
    -trimpath \
    -tags "${BUILD_TAGS}" \
    -ldflags="-s -w \
        -X example.com/myapp/internal/build.Version=${VERSION} \
        -X example.com/myapp/internal/build.Commit=${COMMIT} \
        -X example.com/myapp/internal/build.BuildTime=$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
    -o /app/myapp ./cmd/myapp

FROM scratch
COPY --from=builder /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/
COPY --from=builder /app/myapp /myapp
USER 10001
ENTRYPOINT ["/myapp"]
```

```bash
# Build de producción:
docker build \
    --build-arg VERSION=$(git describe --tags --always) \
    --build-arg COMMIT=$(git rev-parse --short HEAD) \
    --build-arg BUILD_TAGS=production \
    -t myapp:prod .

# Build de desarrollo (con métricas y debug):
docker build \
    --build-arg BUILD_TAGS="dev,metrics" \
    -t myapp:dev .

# Build de staging:
docker build \
    --build-arg VERSION=$(git describe --tags --always) \
    --build-arg COMMIT=$(git rev-parse --short HEAD) \
    --build-arg BUILD_TAGS=staging \
    -t myapp:staging .
```

### 13.2 docker-compose con diferentes builds

```yaml
# docker-compose.yml

services:
  # Producción:
  myapp:
    build:
      context: .
      args:
        VERSION: "${VERSION:-dev}"
        COMMIT: "${COMMIT:-unknown}"
        BUILD_TAGS: "production"
    ports:
      - "8080:8080"

  # Desarrollo (con pprof y métricas):
  myapp-dev:
    build:
      context: .
      args:
        BUILD_TAGS: "dev,metrics"
    ports:
      - "8080:8080"
      - "6060:6060"    # pprof
    environment:
      - LOG_LEVEL=debug
```

---

## 14. CI/CD con tags y ldflags

### 14.1 GitHub Actions

```yaml
# .github/workflows/build.yml

name: Build

on:
  push:
    branches: [main]
    tags: ['v*']
  pull_request:

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0   # necesario para git describe

      - uses: actions/setup-go@v5
        with:
          go-version: stable

      - name: Determine version
        id: version
        run: |
          if [[ $GITHUB_REF == refs/tags/v* ]]; then
            echo "version=${GITHUB_REF#refs/tags/}" >> $GITHUB_OUTPUT
          else
            echo "version=$(git describe --tags --always --dirty)" >> $GITHUB_OUTPUT
          fi
          echo "commit=$(git rev-parse --short HEAD)" >> $GITHUB_OUTPUT

      - name: Test
        run: go test -race -count=1 ./...

      - name: Test integration
        if: github.event_name == 'push'
        run: go test -tags integration -race -count=1 ./...

      - name: Build
        run: |
          CGO_ENABLED=0 go build -trimpath \
            -tags production \
            -ldflags="-s -w \
              -X example.com/myapp/internal/build.Version=${{ steps.version.outputs.version }} \
              -X example.com/myapp/internal/build.Commit=${{ steps.version.outputs.commit }} \
              -X example.com/myapp/internal/build.BuildTime=$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
            -o myapp ./cmd/myapp

      - name: Verify version
        run: ./myapp --version
```

### 14.2 GitLab CI

```yaml
# .gitlab-ci.yml

variables:
  VERSION: "${CI_COMMIT_TAG:-${CI_COMMIT_SHORT_SHA}}"
  COMMIT: "${CI_COMMIT_SHORT_SHA}"
  MODULE: "example.com/myapp"
  VERSION_PKG: "${MODULE}/internal/build"

stages:
  - test
  - build
  - deploy

test:unit:
  stage: test
  image: golang:1.23
  script:
    - go test -race -count=1 ./...

test:integration:
  stage: test
  image: golang:1.23
  services:
    - postgres:16
  variables:
    TEST_DATABASE_URL: "postgres://postgres:postgres@postgres:5432/test?sslmode=disable"
  script:
    - go test -tags integration -race -count=1 ./...

build:
  stage: build
  image: golang:1.23
  script:
    - |
      CGO_ENABLED=0 go build -trimpath \
        -tags production \
        -ldflags="-s -w \
          -X ${VERSION_PKG}.Version=${VERSION} \
          -X ${VERSION_PKG}.Commit=${COMMIT} \
          -X ${VERSION_PKG}.BuildTime=$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
        -o myapp ./cmd/myapp
    - ./myapp --version
  artifacts:
    paths:
      - myapp
```

---

## 15. Patrones avanzados con build tags

### 15.1 Backend de base de datos intercambiable

```go
// store/store.go — interfaz común
package store

type Store interface {
    Get(key string) (string, error)
    Set(key, value string) error
    Close() error
}

// New creates a store based on build tags.
// Only one implementation is compiled into the binary.
func New(dsn string) (Store, error)
```

```go
// store/postgres.go
//go:build postgres

package store

import "database/sql"

type postgresStore struct {
    db *sql.DB
}

func New(dsn string) (Store, error) {
    db, err := sql.Open("postgres", dsn)
    if err != nil {
        return nil, err
    }
    return &postgresStore{db: db}, nil
}

func (s *postgresStore) Get(key string) (string, error) { /* ... */ return "", nil }
func (s *postgresStore) Set(key, value string) error     { /* ... */ return nil }
func (s *postgresStore) Close() error                    { return s.db.Close() }
```

```go
// store/sqlite.go
//go:build sqlite

package store

import "database/sql"

type sqliteStore struct {
    db *sql.DB
}

func New(dsn string) (Store, error) {
    db, err := sql.Open("sqlite3", dsn)
    if err != nil {
        return nil, err
    }
    return &sqliteStore{db: db}, nil
}

func (s *sqliteStore) Get(key string) (string, error) { /* ... */ return "", nil }
func (s *sqliteStore) Set(key, value string) error     { /* ... */ return nil }
func (s *sqliteStore) Close() error                    { return s.db.Close() }
```

```go
// store/memory.go — fallback sin tags
//go:build !postgres && !sqlite

package store

import "sync"

type memoryStore struct {
    mu   sync.RWMutex
    data map[string]string
}

func New(_ string) (Store, error) {
    return &memoryStore{data: make(map[string]string)}, nil
}

func (s *memoryStore) Get(key string) (string, error) {
    s.mu.RLock()
    defer s.mu.RUnlock()
    return s.data[key], nil
}

func (s *memoryStore) Set(key, value string) error {
    s.mu.Lock()
    defer s.mu.Unlock()
    s.data[key] = value
    return nil
}

func (s *memoryStore) Close() error { return nil }
```

```bash
# Compilar con diferentes backends:
go build -tags postgres -o myapp-pg ./cmd/myapp
go build -tags sqlite -o myapp-sqlite ./cmd/myapp
go build -o myapp-memory ./cmd/myapp  # sin tag → memory

# El binario de PostgreSQL NO incluye el código de SQLite ni memory.
# El binario de SQLite NO incluye el código de PostgreSQL ni memory.
# Cada binario solo contiene lo necesario.
```

### 15.2 Logging condicional con zero cost

```go
// log_verbose.go — logging detallado
//go:build verbose

package logger

import (
    "fmt"
    "runtime"
    "time"
)

func Trace(msg string, args ...any) {
    _, file, line, _ := runtime.Caller(1)
    fmt.Printf("[TRACE %s %s:%d] %s\n",
        time.Now().Format("15:04:05.000"), file, line,
        fmt.Sprintf(msg, args...))
}
```

```go
// log_quiet.go — logging mínimo (producción)
//go:build !verbose

package logger

// Trace is a no-op in production builds.
// The compiler will inline and eliminate these calls entirely.
func Trace(msg string, args ...any) {}
```

```bash
# El compilador de Go es lo suficientemente inteligente para:
# 1. Inlinear la función vacía Trace()
# 2. Eliminar la llamada completa como dead code
# 3. NO evaluar los argumentos fmt.Sprintf si Trace es no-op

# Resultado: zero cost en producción.
# No hay overhead de runtime, no hay branching.
```

### 15.3 Incluir/excluir endpoints de admin

```go
// admin_endpoints.go — endpoints de administración
//go:build admin

package handlers

import (
    "encoding/json"
    "net/http"
    "runtime"
    "runtime/debug"
)

func init() {
    http.HandleFunc("/admin/gc", func(w http.ResponseWriter, r *http.Request) {
        runtime.GC()
        w.Write([]byte("GC triggered"))
    })

    http.HandleFunc("/admin/buildinfo", func(w http.ResponseWriter, r *http.Request) {
        info, _ := debug.ReadBuildInfo()
        json.NewEncoder(w).Encode(info)
    })

    http.HandleFunc("/admin/goroutines", func(w http.ResponseWriter, r *http.Request) {
        buf := make([]byte, 1<<20)
        n := runtime.Stack(buf, true)
        w.Write(buf[:n])
    })
}
```

```bash
# Build interno (con endpoints de admin):
go build -tags admin -o myapp-internal ./cmd/myapp

# Build público (sin endpoints de admin):
go build -o myapp-public ./cmd/myapp

# Los endpoints de admin ni siquiera existen en el binario público.
# No hay riesgo de que alguien los descubra.
```

---

## 16. go list -tags — inspeccionar qué se compila

```bash
# Ver qué archivos se compilan con diferentes tags:

# Sin tags:
go list -f '{{.GoFiles}}' ./internal/config/
# [config.go config_default.go]

# Con tag "dev":
go list -tags dev -f '{{.GoFiles}}' ./internal/config/
# [config.go config_dev.go]

# Con tag "production":
go list -tags production -f '{{.GoFiles}}' ./internal/config/
# [config.go config_prod.go]

# Ver archivos ignorados:
go list -f '{{.IgnoredGoFiles}}' ./internal/config/
# [config_dev.go config_staging.go config_prod.go]

# Ver build constraints de cada archivo:
go list -f '{{range .GoFiles}}{{.}}: {{end}}' ./internal/config/

# Archivos de test con tags:
go list -tags integration -f '{{.TestGoFiles}}' ./internal/db/
# [postgres_test.go postgres_integration_test.go]
```

---

## 17. Errores comunes con build tags

### 17.1 Build tag en la posición incorrecta

```go
// ✗ MAL — build tag después de package:
package main

//go:build linux   // ← IGNORADO: debe estar ANTES de package

import "fmt"

func main() { fmt.Println("hello") }
```

```go
// ✓ BIEN — build tag antes de package:
//go:build linux

package main

import "fmt"

func main() { fmt.Println("hello") }
```

```go
// ✓ BIEN — con copyright antes:
// Copyright 2024 My Org. All rights reserved.
// Use of this source code is governed by a MIT license.

//go:build linux

package main
```

### 17.2 Línea vacía faltante

```go
// ✗ MAL — sin línea vacía entre //go:build y package:
//go:build linux
package main     // ← Go trata el build tag como comentario de package

// ✓ BIEN — línea vacía obligatoria:
//go:build linux

package main
```

### 17.3 Archivo sin implementación para un target

```bash
# Si compilas para freebsd pero no tienes sysinfo_freebsd.go:

$ GOOS=freebsd go build ./...
# ./internal/sysinfo/sysinfo.go:8:6: missing function body
# (o similar — depende de cómo declaraste la interfaz)

# Soluciones:
# 1. Crear sysinfo_freebsd.go con implementación
# 2. Crear un archivo genérico con //go:build !linux && !darwin && !windows
# 3. Usar build tags más amplios: //go:build unix (Go 1.19+)
```

### 17.4 Tag "unix" y "windows" (Go 1.19+)

```go
// Go 1.19 añadió el tag "unix" que incluye todos los Unix-like:
// linux, darwin, freebsd, openbsd, netbsd, dragonfly, solaris, aix, illumos

//go:build unix

package sysinfo

// Este código funciona en TODOS los Unix-like.
// Equivale a: //go:build linux || darwin || freebsd || openbsd || ...
// Pero mucho más limpio.

func GetUptime() (float64, error) {
    // /proc/uptime solo existe en Linux,
    // pero syscall.Sysctl funciona en todos los Unix.
    // Elegir la implementación correcta.
    return 0, nil
}
```

---

## 18. Tabla de errores comunes

| Error / Síntoma | Causa | Solución |
|---|---|---|
| Build tag ignorado silenciosamente | Tag después de `package` o sin línea vacía | Mover `//go:build` antes de `package` con línea vacía entre ambos |
| `missing function body` al cross-compilar | No hay archivo con implementación para ese GOOS | Crear archivo `_<goos>.go` o usar tag genérico `//go:build unix` |
| `undefined: debugLog` | Compilaste sin `-tags debug` y no hay stub | Crear archivo con `//go:build !debug` que defina la función vacía |
| ldflags `-X` no tiene efecto | Variable es `const` en vez de `var`, o import path incorrecto | Usar `var`, verificar import path completo con `go list -m` |
| `cannot use -X with non-string variable` | Intentas inyectar en un int, bool, etc. | `-X` solo funciona con `string`; convertir en el código |
| Binario muestra "dev" como versión en CI | `git describe` falla (shallow clone, sin tags) | `actions/checkout` con `fetch-depth: 0`, o crear tag primero |
| `-gcflags` no aplica a dependencias | Falta `all=` prefix | Usar `-gcflags="all=-N -l"` en lugar de `-gcflags="-N -l"` |
| Build tag `// +build` no funciona en Go 1.17+ | Sintaxis antigua sin sincronización | Ejecutar `gofmt` para que añada `//go:build` o migrar manualmente |
| Múltiples archivos definen la misma función | Dos archivos activos para el mismo target sin tags excluyentes | Revisar que los tags son mutuamente excluyentes |
| `-tags "integration"` no ejecuta test files | El archivo test tiene `//go:build integration` pero no `_test.go` en el nombre | El archivo debe llamarse `*_test.go` además de tener el build tag |
| `go version -m` no muestra tags de build | Los tags no se almacenan en build info | Inyectar el build tag como variable con `-X main.buildTags=production` |
| Binario demasiado grande con `-tags metrics` | Prometheus y dependencias se incluyen | Es normal (~4MB extra); usar `-ldflags="-s -w"` para reducir |

---

## 19. Ejercicios

### Ejercicio 1 — Build tag básico

```
Crea un programa con:
- greeting_es.go (//go:build es) → imprime "Hola, mundo"
- greeting_en.go (//go:build en) → imprime "Hello, world"
- greeting_default.go (//go:build !es && !en) → imprime "Hi"

1. Compila sin tags → ¿qué imprime?
2. Compila con -tags es → ¿qué imprime?
3. Compila con -tags en → ¿qué imprime?
4. Intenta -tags "es,en" → ¿qué pasa?

Predicción: ¿compilará con -tags "es,en"? ¿Por qué sí o por qué no?
(pista: ¿cuántas definiciones de main habrá?)
```

### Ejercicio 2 — ldflags versión

```
Crea un programa con variables:
  var version, commit, buildTime string

1. Compila sin ldflags → ¿qué muestra --version?
2. Compila con -ldflags="-X main.version=v1.0.0"
3. Compila con versión, commit y buildTime inyectados
4. Mueve las variables a internal/build/ y ajusta el -X path
5. Verifica con go version -m que los ldflags aparecen

Predicción: si usas const en vez de var, ¿funcionará -X?
¿Qué muestra go version -m sobre ldflags?
```

### Ejercicio 3 — Tests de integración con tags

```
Crea un paquete con:
- unit_test.go (sin tag) → tests que no necesitan nada externo
- integration_test.go (//go:build integration) → test que
  necesita una variable de entorno DATABASE_URL

1. Ejecuta go test ./... → ¿se ejecutan los integration tests?
2. Ejecuta go test -tags integration ./...
3. Verifica que sin DATABASE_URL el test se salta (t.Skip)
4. Crea un Makefile con targets test y test-integration

Predicción: ¿go test -v ./... mostrará los tests de integración
en la lista aunque no los ejecute?
```

### Ejercicio 4 — Feature flags

```
Crea un servidor HTTP con estas features condicionales:
- Tag "metrics" → endpoint /metrics con info básica
- Tag "admin" → endpoint /admin/info con runtime info
- Tag "debug" → logging verbose en cada request

1. Compila sin tags → ¿cuántos endpoints tiene?
2. Compila con -tags metrics → ¿cuántos?
3. Compila con -tags "metrics,admin,debug" → ¿cuántos?
4. Compara los tamaños de los binarios

Predicción: ¿el binario con "metrics,admin,debug" será
significativamente más grande? ¿Cuánto?
```

### Ejercicio 5 — Debug vs release build

```
Crea un paquete logger con:
- logger_debug.go (//go:build debug) → log con file:line y timestamp
- logger_release.go (//go:build !debug) → funciones vacías (no-op)

1. Compila con -tags debug y sin tags
2. Añade llamadas logger.Debug() en un loop de 1M iteraciones
3. Benchmarkea ambas versiones (go test -bench)
4. Verifica que la versión release tiene zero cost

Predicción: ¿cuál será la diferencia de rendimiento?
¿El compilador eliminará las llamadas vacías completamente?
```

### Ejercicio 6 — Código específico de plataforma con tags

```
Crea un programa que obtenga el uso de disco:
- disk_linux.go → lee /proc/mounts + syscall.Statfs
- disk_darwin.go → usa exec.Command("df", "-h")
- disk_other.go (//go:build !linux && !darwin) → "not supported"

1. Compila para tu plataforma y verifica que funciona
2. Cross-compila para las otras dos y verifica con file
3. Usa go list para ver qué archivos se incluyen en cada caso
4. Añade el tag "unix" para combinar linux y darwin

Predicción: ¿compilará para windows sin un disk_windows.go
si tienes disk_other.go con //go:build !linux && !darwin?
```

### Ejercicio 7 — ldflags con paquete version

```
Crea el patrón completo de versionado:
- internal/build/version.go con Version, Commit, BuildTime, GoVersion
- Función Info() que devuelve un string formateado
- Fallback con debug.ReadBuildInfo()
- cmd/myapp/main.go con flag --version

1. Ejecuta go run → ¿funciona el fallback?
2. Compila con ldflags completos → ¿los valores son correctos?
3. Ejecuta go version -m → ¿se ven los ldflags?
4. Crea un Makefile que automatice los ldflags

Predicción: ¿go run inyecta la información VCS automáticamente?
¿Y go build sin ldflags?
```

### Ejercicio 8 — gcflags para debugging

```
1. Compila con -gcflags="-m" y busca "escapes to heap"
2. Compila con -gcflags="-m -m" para más detalle
3. Compila con -gcflags="all=-N -l" y verifica el tamaño
4. Inicia dlv en el binario sin optimizar y verifica que
   puedes inspeccionar todas las variables
5. Compara con dlv en un binario optimizado — ¿qué cambia?

Predicción: ¿el binario sin optimizar será más grande?
¿Cuánto más? ¿dlv mostrará <optimized out> en el optimizado?
```

### Ejercicio 9 — Dockerfile con tags y ldflags

```
Crea un Dockerfile que acepte build args:
- VERSION
- COMMIT
- BUILD_TAGS (default: production)

1. Construye con BUILD_TAGS=production
2. Construye con BUILD_TAGS="dev,metrics"
3. Ejecuta cada imagen con --version y compara
4. Inspecciona con docker inspect el tamaño

Predicción: ¿la imagen con "dev,metrics" será más grande?
¿Se puede pasar BUILD_TAGS vacío? ¿Qué pasa entonces?
```

### Ejercicio 10 — Pipeline CI completa

```
Crea un workflow de GitHub Actions que:
1. En PRs: go test ./... (solo unitarios)
2. En push a main: go test -tags integration ./...
3. En tags v*: build con -tags production y ldflags,
   multi-plataforma (linux/amd64, linux/arm64, darwin/arm64)
4. Publique los binarios como release assets

Verifica:
- ¿Los tests de integración corren en PRs?
- ¿Los binarios de release tienen la versión correcta?
- ¿Los checksums son correctos?

Predicción: ¿qué pasa si haces push de un tag sin que los
tests de integración hayan pasado?
```

---

## 20. Resumen

```
    ┌───────────────────────────────────────────────────────────┐
    │          Build tags y ldflags — Resumen                   │
    │                                                           │
    │  BUILD TAGS (//go:build):                                 │
    │  ┌─ Controlan QUÉ ARCHIVOS se compilan                  │
    │  ├─ Automáticos: GOOS, GOARCH, cgo, go1.XX              │
    │  ├─ Personalizados: debug, integration, production       │
    │  ├─ Operadores: && (AND), || (OR), ! (NOT), ()           │
    │  ├─ Mutualmente excluyentes con stubs (!tag)             │
    │  └─ Se pasan con: go build -tags "tag1,tag2"             │
    │                                                           │
    │  LDFLAGS (-ldflags):                                      │
    │  ┌─ Inyectan VALORES en variables string                 │
    │  ├─ -X path.variable=value                               │
    │  ├─ -s (strip symbols) -w (strip DWARF)                  │
    │  ├─ -extldflags (flags para linker externo)              │
    │  └─ Solo var string, NO const ni otros tipos             │
    │                                                           │
    │  GCFLAGS (-gcflags):                                      │
    │  ┌─ -N -l → desactivar optimizaciones (para delve)       │
    │  ├─ -m → ver decisiones de inlining y escape analysis    │
    │  └─ all= → aplicar a todos los paquetes                 │
    │                                                           │
    │  PATRONES PARA SYSADMIN:                                  │
    │  ┌─ Versión: -X main.version=$(git describe --tags)      │
    │  ├─ Tests: -tags integration para tests con DB/red       │
    │  ├─ Features: -tags metrics para incluir Prometheus      │
    │  ├─ Entornos: -tags production para defaults de prod     │
    │  └─ Debug: -tags debug + stub no-op para zero cost       │
    │                                                           │
    │  COMANDO DE PRODUCCIÓN:                                   │
    │  CGO_ENABLED=0 go build -trimpath                        │
    │    -tags production                                       │
    │    -ldflags="-s -w                                        │
    │      -X main.version=$(git describe --tags)               │
    │      -X main.commit=$(git rev-parse --short HEAD)         │
    │      -X main.buildTime=$(date -u +%Y-%m-%dT%H:%M:%SZ)"   │
    │    -o myapp ./cmd/myapp                                   │
    └───────────────────────────────────────────────────────────┘
```
