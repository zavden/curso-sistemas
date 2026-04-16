# T04 — Herramientas auxiliares

## 1. El ecosistema de herramientas de Go

Go tiene un ecosistema de herramientas que va más allá del comando `go`.
Estas herramientas cubren formateo, importaciones, linting, documentación
y depuración. Lo que distingue a Go es que las herramientas **no son
opcionales** — son parte integral del flujo de trabajo.

```
    ┌──────────────────────────────────────────────────────────┐
    │                Herramientas del ecosistema Go             │
    │                                                          │
    │  ┌──────────────┐  ┌──────────────┐  ┌───────────────┐  │
    │  │  Formateo     │  │  Análisis     │  │  Depuración    │ │
    │  │              │  │  estático     │  │               │  │
    │  │  gofmt       │  │              │  │  delve (dlv)  │  │
    │  │  goimports   │  │  go vet      │  │  pprof        │  │
    │  │  gofumpt     │  │  staticcheck │  │  trace        │  │
    │  │              │  │  golangci-   │  │  race detector│  │
    │  │              │  │    lint      │  │               │  │
    │  └──────────────┘  └──────────────┘  └───────────────┘  │
    │                                                          │
    │  ┌──────────────┐  ┌──────────────┐  ┌───────────────┐  │
    │  │  Documentación│  │  Testing      │  │  Seguridad    │ │
    │  │              │  │              │  │               │  │
    │  │  go doc      │  │  go test     │  │  govulncheck  │  │
    │  │  godoc       │  │  go test     │  │  gosec        │  │
    │  │  pkgsite     │  │    -bench    │  │               │  │
    │  │              │  │  go test     │  │               │  │
    │  │              │  │    -cover    │  │               │  │
    │  └──────────────┘  └──────────────┘  └───────────────┘  │
    └──────────────────────────────────────────────────────────┘
```

### 1.1 ¿Por qué tantas herramientas?

```bash
# Filosofía de Go: herramientas que eliminan debates

# En otros lenguajes se debate:
# - ¿Tabs o espacios? → gofmt decide (tabs)
# - ¿Qué linter usar? → golangci-lint los agrupa todos
# - ¿Cómo ordenar imports? → goimports lo hace automático
# - ¿Cómo documentar? → godoc extrae comentarios, no hay formato especial

# Resultado: todo proyecto Go tiene el mismo estilo.
# Un SysAdmin puede leer cualquier herramienta Go sin sorpresas.
```

| Herramienta      | Incluida en Go | Propósito                              | Cuándo usar                |
|------------------|:--------------:|----------------------------------------|----------------------------|
| `gofmt`          | ✓              | Formateo canónico                      | Cada save                  |
| `goimports`      | go install     | gofmt + gestión automática de imports  | Cada save (reemplaza gofmt)|
| `gofumpt`        | go install     | gofmt estricto (más opinionado)        | Cada save (alternativa)    |
| `go vet`         | ✓              | Análisis estático básico               | Cada build / CI            |
| `staticcheck`    | go install     | Análisis estático avanzado             | CI / pre-commit            |
| `golangci-lint`  | binario externo| Meta-linter (ejecuta 100+ linters)     | CI / pre-commit            |
| `go doc`         | ✓              | Documentación en terminal              | Consultas rápidas          |
| `godoc`          | go install     | Servidor web de documentación          | Desarrollo / revisión      |
| `pkgsite`        | go install     | godoc moderno (pkg.go.dev local)       | Desarrollo / revisión      |
| `delve` (`dlv`)  | go install     | Debugger nativo de Go                  | Depuración                 |
| `pprof`          | ✓              | Profiling CPU/memoria/goroutines       | Optimización               |
| `govulncheck`    | go install     | Escáner de vulnerabilidades            | CI / auditoría             |
| `gosec`          | go install     | Análisis de seguridad                  | CI / auditoría             |

---

## 2. gofmt — el formateador canónico

`gofmt` es la herramienta de formateo que viene incluida con Go. Formatea
código Go según un estilo único, sin opciones de configuración.

```bash
# gofmt en la terminal (la herramienta directa):
gofmt archivo.go          # muestra el resultado formateado en stdout
gofmt -w archivo.go       # modifica el archivo in-place (-w = write)
gofmt -d archivo.go       # muestra el diff sin modificar (-d = diff)
gofmt -w .                # formatea todos los .go en el directorio actual
gofmt -l .                # lista archivos que necesitan formateo (-l = list)

# go fmt (el subcomando de go — wrapper de gofmt):
go fmt ./...              # formatea todos los paquetes del módulo
go fmt ./internal/...     # formatea solo los paquetes en internal/
```

### 2.1 gofmt vs go fmt

```bash
# gofmt — la herramienta raw:
#   - Trabaja con archivos individuales o directorios
#   - Tiene más flags (-s para simplificar, -r para reescribir)
#   - No entiende de módulos ni paquetes

# go fmt — el wrapper:
#   - Trabaja con paquetes (go fmt ./...)
#   - Internamente ejecuta: gofmt -l -w <archivos del paquete>
#   - Entiende el sistema de módulos

# En la práctica:
# - Usa go fmt ./... para formatear todo el proyecto
# - Usa gofmt -d para ver diffs sin modificar
# - Usa gofmt -s para simplificar código
```

### 2.2 gofmt -s: simplificación de código

```go
// gofmt -s simplifica construcciones redundantes.
// Antes de gofmt -s:

// 1. Tipo redundante en composite literals:
var points []Point = []Point{Point{1, 2}, Point{3, 4}}

// Después de gofmt -s:
var points []Point = []Point{{1, 2}, {3, 4}}

// 2. Slice expression redundante:
s[0:len(s)]

// Después de gofmt -s:
s[:]

// 3. Range con variable no usada:
for _ = range items { }

// Después de gofmt -s:
for range items { }
```

```bash
# Aplicar simplificación a todo el proyecto:
gofmt -s -w .

# Ver qué cambiaría sin modificar:
gofmt -s -d .

# En CI (verificar que el código está formateado Y simplificado):
if [ -n "$(gofmt -s -l .)" ]; then
    echo "Código no formateado:"
    gofmt -s -l .
    exit 1
fi
```

### 2.3 gofmt -r: reescritura de patrones

```bash
# gofmt -r permite transformaciones automáticas de código.
# Sintaxis: gofmt -r 'patrón -> reemplazo'

# Ejemplo: renombrar un campo:
gofmt -r 'a.Old -> a.New' -w .

# Ejemplo: cambiar una llamada:
gofmt -r 'bytes.Compare(a, b) == 0 -> bytes.Equal(a, b)' -w .

# Ejemplo: simplificar un patrón:
gofmt -r 'a != nil && len(a) > 0 -> len(a) > 0' -w .

# Las variables a, b, c... en el patrón son wildcards
# que matchean cualquier expresión Go.
```

### 2.4 Lo que gofmt decide por ti

```go
// gofmt tiene cero opciones de configuración. Decide:

// 1. Indentación: TABS (no espacios)
// 2. Alineamiento: espacios para alinear dentro de una línea
// 3. Llaves: siempre en la misma línea (estilo K&R)
if x > 0 {    // ✓ llave en misma línea
    // ...
}
// No existe:
// if x > 0     // ✗ no compila — llave en siguiente línea
// {

// 4. Espaciado entre operadores y paréntesis
x = a + b      // no: x=a+b ni x = a  +  b

// 5. Trailing commas obligatorias en multiline:
config := Config{
    Host: "localhost",
    Port: 8080,       // ← coma obligatoria en última línea
}
```

```bash
# ¿Por qué esto importa para SysAdmin?
#
# 1. Code reviews: NUNCA discutes sobre estilo
# 2. Diffs limpios: los cambios son solo lógica, no formateo
# 3. Onboarding: cualquier ingeniero nuevo produce código
#    visualmente idéntico desde el día 1
# 4. Automatización: gofmt es idempotente, seguro de ejecutar
#    en pre-commit hooks sin riesgo
```

---

## 3. goimports — gofmt + gestión automática de imports

`goimports` hace todo lo que hace `gofmt` más gestión automática de
imports: añade los que faltan, elimina los que sobran y los ordena en
grupos.

```bash
# Instalar goimports:
go install golang.org/x/tools/cmd/goimports@latest

# Uso básico:
goimports -w archivo.go      # formatea + arregla imports in-place
goimports -d archivo.go      # muestra diff
goimports -l .               # lista archivos que necesitan cambios
goimports -w .               # procesa todos los .go del directorio

# Flag -local para separar imports del proyecto:
goimports -local example.com/myapp -w .
```

### 3.1 Qué hace goimports automáticamente

```go
// ANTES (archivo con imports incorrectos):
package main

import (
    "os"          // ← no se usa
    "fmt"
)

func main() {
    http.ListenAndServe(":8080", nil)   // ← http no importado
    fmt.Println("hello")
    json.NewEncoder(os.Stdout).Encode(map[string]string{"status": "ok"})
    // ← json no importado, y ahora os SÍ se usa
}

// DESPUÉS de goimports -w:
package main

import (
    "encoding/json"    // ← añadido automáticamente
    "fmt"
    "net/http"         // ← añadido automáticamente
    "os"               // ← mantenido porque ahora sí se usa
)

func main() {
    http.ListenAndServe(":8080", nil)
    fmt.Println("hello")
    json.NewEncoder(os.Stdout).Encode(map[string]string{"status": "ok"})
}
```

### 3.2 Agrupación de imports con -local

```go
// Sin -local, goimports agrupa en 2 grupos:
import (
    "fmt"                                    // stdlib
    "net/http"

    "github.com/gin-gonic/gin"              // todo lo externo junto
    "github.com/prometheus/client_golang/prometheus"
    "example.com/myapp/internal/handlers"   // ← mezclado con externos
)

// Con goimports -local example.com/myapp, separa en 3 grupos:
import (
    "fmt"                                    // 1. stdlib
    "net/http"

    "github.com/gin-gonic/gin"              // 2. dependencias externas
    "github.com/prometheus/client_golang/prometheus"

    "example.com/myapp/internal/handlers"   // 3. paquetes del proyecto
    "example.com/myapp/internal/config"
)
```

### 3.3 Configurar goimports en el editor

```bash
# VS Code (settings.json):
# {
#   "go.formatTool": "goimports",
#   "editor.formatOnSave": true,
#   "[go]": {
#     "editor.defaultFormatter": "golang.go"
#   }
# }

# Neovim (con gopls, el flag -local se configura en gopls):
# gopls settings:
# {
#   "goimports.local": "example.com/myapp"
# }

# Vim (vim-go):
# let g:go_fmt_command = "goimports"
# let g:go_fmt_options = { 'goimports': '-local example.com/myapp' }

# JetBrains GoLand:
# Settings → Go → Imports → Group by: stdlib / third-party / project
# (GoLand hace esto nativo, no necesita goimports externo)
```

### 3.4 goimports vs gofumpt

```bash
# gofumpt es un formateador más estricto creado por Daniel Martí.
# Hace todo lo de gofmt + reglas adicionales.

go install mvdan.cc/gofumpt@latest

# Reglas adicionales de gofumpt:
# 1. No permite líneas vacías al inicio/final de bloques
# 2. No permite líneas vacías redundantes consecutivas
# 3. Requiere línea vacía antes de return si el bloque tiene más de 1 stmt
# 4. Alineación más consistente de asignaciones

# gofumpt NO gestiona imports → combinar con goimports:
# En el editor, usar gopls con ambas opciones:
# gopls settings:
# {
#   "formatting.gofumpt": true,
#   "goimports.local": "example.com/myapp"
# }

# Esto aplica gofumpt (formateo estricto) + goimports (imports)
# simultáneamente al guardar.
```

---

## 4. golangci-lint — el meta-linter

`golangci-lint` es la herramienta de linting estándar de facto en el
ecosistema Go. No es un linter — es un **runner de linters** que ejecuta
100+ linters diferentes en paralelo.

```bash
# ¿Por qué golangci-lint y no linters individuales?
#
# 1. Ejecuta 100+ linters en paralelo (mucho más rápido que uno por uno)
# 2. Reutiliza el AST parseado entre linters (no parsea N veces)
# 3. Un solo archivo de configuración (.golangci.yml)
# 4. Cache inteligente (solo analiza archivos cambiados)
# 5. Integración nativa con CI (GitHub Actions, GitLab CI, etc.)
# 6. Modo --new-from-rev: solo reporta errores en código NUEVO
```

### 4.1 Instalación

```bash
# Método recomendado — binario precompilado:
curl -sSfL https://raw.githubusercontent.com/golangci/golangci-lint/HEAD/install.sh | \
    sh -s -- -b $(go env GOPATH)/bin v1.62.2

# Verificar versión:
golangci-lint --version

# ⚠️ NO instalar con go install:
# go install github.com/golangci/golangci-lint/cmd/golangci-lint@latest
# Esto funciona pero NO es recomendado porque:
# 1. El binario no incluye la versión correcta en --version
# 2. Puede compilar con una versión de Go diferente a la testeada
# 3. No se beneficia de las optimizaciones de build del release

# Con Homebrew (macOS/Linux):
brew install golangci-lint

# Con Docker:
docker run --rm -v $(pwd):/app -w /app \
    golangci/golangci-lint:v1.62.2 \
    golangci-lint run

# En CI (GitHub Actions):
# - uses: golangci/golangci-lint-action@v6
#   with:
#     version: v1.62.2
```

### 4.2 Uso básico

```bash
# Ejecutar con linters por defecto:
golangci-lint run

# Ejecutar en paquetes específicos:
golangci-lint run ./internal/...
golangci-lint run ./cmd/myapp/...

# Ejecutar un linter específico:
golangci-lint run --enable=errcheck --disable-all

# Modo verbose (ver qué linters se ejecutan):
golangci-lint run -v

# Solo analizar archivos cambiados desde main:
golangci-lint run --new-from-rev=main

# Solo archivos cambiados desde el último commit:
golangci-lint run --new-from-rev=HEAD~1

# Mostrar todos los linters disponibles:
golangci-lint linters

# Formatear output como JSON:
golangci-lint run --out-format=json
```

### 4.3 Linters que vienen habilitados por defecto

```bash
# golangci-lint v1.62 habilita estos linters por defecto:

# errcheck    — detecta errores no chequeados:
#               f, _ := os.Open("file")  ← reporta que ignoras el error

# gosimple    — sugiere simplificaciones:
#               if err != nil { return err }; return nil  →  return err

# govet       — el go vet estándar (printf format, struct tags, etc.)

# ineffassign — detecta asignaciones ineficientes:
#               x := 5; x = 10  ← la primera asignación es inútil

# staticcheck — análisis estático avanzado (SA*, S*, ST*, QF*):
#               SA1019: uso de API deprecated
#               SA4006: variable asignada y nunca usada
#               S1005: range innecesario (for i, _ := range → for i := range)

# unused      — detecta código no utilizado:
#               funciones, tipos, constantes, variables no usadas
```

### 4.4 Linters recomendados para SysAdmin/DevOps

```yaml
# .golangci.yml — configuración recomendada para proyectos SysAdmin

run:
  timeout: 5m
  tests: true           # también analizar archivos _test.go

linters:
  enable:
    # --- Errores comunes ---
    - errcheck           # errores no chequeados (CRÍTICO)
    - govet              # análisis estático de go vet
    - staticcheck        # análisis avanzado
    - ineffassign        # asignaciones sin efecto
    - unused             # código muerto

    # --- Seguridad ---
    - gosec              # detecta patrones inseguros (SQL injection,
                         # crypto débil, permisos de archivo, etc.)
    - bodyclose          # detecta http.Response.Body sin Close()

    # --- Errores de concurrencia ---
    - gocritic           # diagnósticos variados (100+ checks)

    # --- Estilo y mantenibilidad ---
    - gofmt              # verifica formateo
    - goimports          # verifica imports
    - misspell           # detecta typos en comentarios y strings
    - revive             # golint modernizado y configurable
    - nakedret           # detecta naked returns en funciones largas
    - prealloc           # sugiere preallocación de slices

    # --- Complejidad ---
    - gocyclo            # complejidad ciclomática
    - nestif             # detecta if anidados excesivos

    # --- Testing ---
    - tparallel          # detecta tests que deberían usar t.Parallel()

    # --- Específicos para CLI/SysAdmin ---
    - noctx              # detecta http requests sin context
    - exhaustive         # verifica que los switch sobre enums cubran
                         # todos los casos
    - errorlint          # errores mal comparados (== vs errors.Is)

linters-settings:
  govet:
    enable-all: true
  gocyclo:
    min-complexity: 15
  nestif:
    min-complexity: 5
  nakedret:
    max-func-lines: 30
  gosec:
    excludes:
      - G104      # unhandled errors (ya lo cubre errcheck)
  revive:
    rules:
      - name: exported
        arguments:
          - "checkPrivateReceivers"
      - name: unexported-return
        disabled: true

issues:
  # No limitar el número de issues por linter:
  max-issues-per-linter: 0
  max-same-issues: 0

  exclude-rules:
    # Permitir magic numbers en tests:
    - path: _test\.go
      linters:
        - gomnd
    # Permitir dot imports en tests:
    - path: _test\.go
      linters:
        - revive
      text: "dot-imports"
```

### 4.5 Suprimir warnings con nolint

```go
// A veces un linter reporta un falso positivo o una excepción justificada.
// Usar //nolint para suprimir — SIEMPRE con razón:

// ✓ Bien — con explicación:
//nolint:gosec // G404: math/rand es suficiente aquí, no es crypto
func generateID() int {
    return rand.Intn(1000000)
}

// ✗ Mal — sin explicación:
//nolint:errcheck
func doSomething() { }

// Suprimir múltiples linters:
//nolint:errcheck,gosec // razón aquí

// Suprimir en una línea específica:
func handler(w http.ResponseWriter, r *http.Request) {
    w.Write([]byte("ok")) //nolint:errcheck // HTTP write error is non-recoverable
}

// En .golangci.yml puedes requerir explicación obligatoria:
// linters-settings:
//   nolintlint:
//     require-explanation: true
//     require-specific: true   # prohibe //nolint genérico
```

### 4.6 golangci-lint en CI/CD

```yaml
# GitHub Actions — .github/workflows/lint.yml

name: lint
on:
  push:
    branches: [main]
  pull_request:

permissions:
  contents: read

jobs:
  golangci:
    name: lint
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-go@v5
        with:
          go-version: stable
      - name: golangci-lint
        uses: golangci/golangci-lint-action@v6
        with:
          version: v1.62.2
          # Solo reportar errores en código nuevo en PRs:
          # args: --new-from-rev=HEAD~1
```

```bash
# GitLab CI — .gitlab-ci.yml

# lint:
#   image: golangci/golangci-lint:v1.62.2
#   stage: test
#   script:
#     - golangci-lint run --out-format=code-climate > gl-code-quality-report.json
#   artifacts:
#     reports:
#       codequality: gl-code-quality-report.json
```

```bash
# Pre-commit hook — .git/hooks/pre-commit

#!/bin/bash
set -euo pipefail

# Solo lint de archivos staged:
STAGED_GO_FILES=$(git diff --cached --name-only --diff-filter=ACM | grep '\.go$' || true)

if [ -n "$STAGED_GO_FILES" ]; then
    echo "Running golangci-lint..."
    golangci-lint run --new-from-rev=HEAD
    echo "Lint passed ✓"
fi
```

### 4.7 Migrar un proyecto legacy a golangci-lint

```bash
# Problema: habilitar linters en un proyecto con 500+ warnings existentes.
# Solución: --new-from-rev

# 1. Habilitar todos los linters que quieras en .golangci.yml

# 2. En CI, solo reportar warnings en código NUEVO:
golangci-lint run --new-from-rev=main

# 3. Esto permite:
#    - Todo código NUEVO debe cumplir las reglas
#    - El código existente no bloquea el CI
#    - Con el tiempo, al tocar archivos viejos, se van arreglando

# 4. Para un reporte completo (sin bloquear CI):
golangci-lint run || true

# 5. Para un fix masivo inicial:
golangci-lint run --fix   # algunos linters pueden auto-corregir
```

---

## 5. staticcheck — el analizador estático avanzado

`staticcheck` es el analizador estático más potente del ecosistema Go.
Está incluido en golangci-lint, pero puede usarse independientemente.

```bash
# Instalar:
go install honnef.co/go/tools/cmd/staticcheck@latest

# Uso:
staticcheck ./...                  # analizar todo el proyecto
staticcheck ./internal/...        # analizar paquetes específicos
staticcheck -checks all ./...     # habilitar TODOS los checks

# Categorías de checks:
# SA — análisis estático (errores reales)
# S  — simplificaciones (código que se puede simplificar)
# ST — style (convenciones de Go)
# QF — quick fixes (transformaciones automáticas)
```

### 5.1 Checks más útiles para SysAdmin

```go
// SA1019: uso de API deprecated
import "io/ioutil"
data, _ := ioutil.ReadFile("config.yaml")  // ← deprecated desde Go 1.16
// Fix: data, _ := os.ReadFile("config.yaml")

// SA4006: valor asignado nunca leído
func process() error {
    err := step1()    // ← SA4006: err assigned and not used
    err = step2()
    return err
}

// SA1029: key de context inapropiada
ctx = context.WithValue(ctx, "user", u)    // ← SA1029: should not use string key
// Fix: usar tipo propio como key

// SA5001: defer en loop
for _, f := range files {
    conn, _ := open(f)
    defer conn.Close()  // ← SA5001: deferred call in loop
}
// Fix: usar función anónima o cerrar explícitamente

// SA9003: empty branch
if err != nil {
    // TODO
}

// S1005: range innecesario
for i, _ := range items { }  // ← S1005: unnecessary blank assignment
// Fix: for i := range items { }

// S1039: call of fmt.Sprintf redundante
fmt.Sprintf("%s", myString)  // ← S1039: redundante
// Fix: myString

// ST1003: nombres que no siguen convenciones
func Get_user() { }  // ← ST1003: should be GetUser (MixedCaps)
var my_var int        // ← ST1003: should be myVar
```

### 5.2 Configurar staticcheck

```bash
# staticcheck.conf (en la raíz del proyecto):

# checks = ["all", "-ST1000", "-ST1003"]
# initialisms = ["ACL", "API", "ASCII", "CPU", "CSS", "DNS", "EOF",
#                "GUID", "HTML", "HTTP", "HTTPS", "ID", "IP", "JSON",
#                "QPS", "RAM", "RPC", "SLA", "SMTP", "SQL", "SSH",
#                "TCP", "TLS", "TTL", "UDP", "UI", "GID", "UID",
#                "UUID", "URI", "URL", "UTF8", "VM", "XML", "XMPP",
#                "XSRF", "XSS", "SaaS", "IoT"]

# Si ya usas golangci-lint, staticcheck se configura ahí:
# .golangci.yml:
# linters-settings:
#   staticcheck:
#     checks: ["all", "-SA1029"]
```

---

## 6. go doc — documentación en terminal

`go doc` muestra la documentación de paquetes, tipos, funciones y
métodos directamente en la terminal. Es la herramienta de referencia
rápida para el día a día.

```bash
# Documentación de un paquete:
go doc fmt                    # resumen del paquete fmt
go doc net/http               # resumen de net/http
go doc os/exec                # resumen de os/exec

# Documentación de una función:
go doc fmt.Println            # firma + doc de fmt.Println
go doc os.Open                # firma + doc de os.Open
go doc strings.Contains       # firma + doc

# Documentación de un tipo:
go doc http.Request           # tipo + campos exportados
go doc http.Client            # tipo + campos exportados

# Documentación de un método:
go doc http.Client.Do         # método Do del tipo Client
go doc os.File.Read           # método Read del tipo File

# Ver TODA la documentación (incluye unexported):
go doc -all fmt               # documentación completa de fmt
go doc -all net/http          # ¡cuidado, es MUCHO!

# Ver código fuente:
go doc -src fmt.Println       # muestra el source code
go doc -src http.ListenAndServe

# Ver unexported (privado):
go doc -u -all ./internal/config
```

### 6.1 go doc para tus propios paquetes

```go
// internal/config/config.go

// Package config handles application configuration loading and validation.
// It supports YAML, JSON, and environment variable sources with
// a priority-based merge strategy.
//
// Configuration is loaded in order:
//  1. Default values (embedded)
//  2. Config file (if exists)
//  3. Environment variables (highest priority)
package config

// Config holds all application configuration.
// Fields are populated from the config file and environment variables.
type Config struct {
    // Host is the address to listen on. Default: "0.0.0.0".
    Host string `yaml:"host" env:"APP_HOST"`

    // Port is the port to listen on. Default: 8080.
    Port int `yaml:"port" env:"APP_PORT"`

    // LogLevel sets the minimum log level.
    // Valid values: debug, info, warn, error.
    LogLevel string `yaml:"log_level" env:"APP_LOG_LEVEL"`
}

// Load reads configuration from file and environment.
// Environment variables take precedence over file values.
// If the config file does not exist, only defaults and env vars are used.
func Load(path string) (*Config, error) {
    // ...
}
```

```bash
# Ahora puedes consultar la documentación:
go doc ./internal/config
go doc ./internal/config.Config
go doc ./internal/config.Load

# La documentación se genera automáticamente desde los comentarios.
# No hay anotaciones especiales (no @param, no @return, no JavaDoc).
# Solo comentarios normales que preceden al símbolo.
```

### 6.2 Convenciones de documentación en Go

```go
// Reglas para escribir buena documentación en Go:

// 1. El comentario de paquete empieza con "Package <name> ..."
// Package metrics provides Prometheus metric collection utilities.
package metrics

// 2. Los comentarios de funciones empiezan con el nombre de la función:
// Open opens the named file for reading.
func Open(name string) (*File, error) { }

// 3. Texto deprecated:
// ParseConfig parses the configuration file at path.
//
// Deprecated: Use LoadConfig instead, which supports environment variables.
func ParseConfig(path string) (*Config, error) { }

// 4. Ejemplos de código en documentación (en archivos _test.go):
// func ExampleOpen() {
//     f, err := mypackage.Open("test.txt")
//     if err != nil {
//         log.Fatal(err)
//     }
//     defer f.Close()
//     // Output:
// }

// 5. Listas y bloques de código en comentarios:
// Connect establishes a connection to the database.
//
// Supported drivers:
//   - postgres: PostgreSQL 12+
//   - mysql: MySQL 8+
//   - sqlite: SQLite 3.35+
//
// Example DSN:
//
//	postgres://user:pass@host:5432/dbname?sslmode=require
func Connect(driver, dsn string) (*DB, error) { }

// 6. Headings (Go 1.19+):
// # Connection Pooling
//
// The client maintains a pool of connections...
```

---

## 7. godoc y pkgsite — servidores de documentación

`godoc` y `pkgsite` generan un sitio web de documentación a partir de
tu código. `pkgsite` es la versión moderna (la que usa pkg.go.dev).

```bash
# godoc (versión legacy, aún funcional):
go install golang.org/x/tools/cmd/godoc@latest
godoc -http=:6060
# Abrir http://localhost:6060
# Muestra documentación de stdlib + todos tus paquetes en GOPATH/módulos

# pkgsite (versión moderna — replica pkg.go.dev localmente):
go install golang.org/x/pkgsite/cmd/pkgsite@latest
pkgsite -http=:8080
# Abrir http://localhost:8080
# Muestra tu módulo local con el mismo look que pkg.go.dev
```

### 7.1 Cuándo usar cada herramienta

```bash
# go doc → consulta rápida en terminal:
#   "¿cuáles son los campos de http.Server?"
go doc http.Server

# godoc/pkgsite → revisión visual de documentación:
#   "quiero ver la documentación completa de mi paquete"
#   "quiero revisar cómo se ve la API antes de publicar"
pkgsite -http=:8080

# pkg.go.dev → documentación de paquetes públicos:
#   Automáticamente genera docs para cualquier módulo público en Go
#   https://pkg.go.dev/github.com/usuario/paquete
```

### 7.2 Documentación como herramienta de diseño

```bash
# Técnica: escribir la documentación ANTES del código.
# pkg.go.dev/pkgsite genera la documentación al instante.

# Flujo:
# 1. Crear el archivo con funciones vacías y comentarios completos
# 2. Ejecutar pkgsite y revisar la API documentada
# 3. ¿Se entiende la API solo leyendo la doc? → Implementar
# 4. ¿No se entiende? → Rediseñar antes de escribir código

# Esto es especialmente útil para paquetes internos que usarán
# otros equipos o tú mismo en 6 meses.
```

---

## 8. delve (dlv) — el debugger de Go

`delve` es el debugger nativo de Go. A diferencia de GDB, entiende
goroutines, canales, interfaces y el runtime de Go. Es la herramienta
estándar para depuración.

```bash
# Instalar delve:
go install github.com/go-delve/delve/cmd/dlv@latest

# Verificar instalación:
dlv version
```

### 8.1 Modos de ejecución

```bash
# 1. Debug — compilar y depurar el paquete actual:
dlv debug
dlv debug ./cmd/myapp
dlv debug ./cmd/myapp -- --config=dev.yaml  # pasar args al programa

# 2. Test — depurar tests:
dlv test
dlv test ./internal/config -- -run TestLoad
dlv test ./internal/config -- -v

# 3. Exec — depurar un binario ya compilado:
go build -gcflags="all=-N -l" -o myapp ./cmd/myapp
dlv exec ./myapp
dlv exec ./myapp -- --flag=value

# 4. Attach — conectar a un proceso en ejecución:
dlv attach <PID>
dlv attach $(pgrep myapp)

# 5. Core — analizar un core dump:
dlv core ./myapp core.12345

# 6. Connect — conectar a un delve server remoto:
# En el servidor:
dlv debug --headless --listen=:2345 --api-version=2
# En tu máquina:
dlv connect servidor:2345

# ⚠️ -gcflags="all=-N -l" desactiva optimizaciones para que
# el debugger pueda inspeccionar todas las variables.
# -N = sin optimización, -l = sin inlining
```

### 8.2 Comandos esenciales de delve

```bash
# Dentro de la sesión dlv:

# --- Breakpoints ---
break main.main              # breakpoint en función
break main.go:42             # breakpoint en archivo:línea
break ./internal/config/config.go:55  # ruta completa
break config.Load            # breakpoint en función de paquete

# Breakpoint condicional:
break main.go:42
condition 1 i > 100          # solo se detiene si i > 100

# Listar breakpoints:
breakpoints                  # o: bp

# Eliminar breakpoint:
clear 1                      # eliminar breakpoint #1
clearall                     # eliminar todos

# --- Ejecución ---
continue                     # ejecutar hasta el siguiente breakpoint (c)
next                         # siguiente línea (no entra en funciones) (n)
step                         # siguiente línea (entra en funciones) (s)
stepout                      # salir de la función actual (so)
restart                      # reiniciar el programa (r)

# --- Inspección ---
print myVar                  # imprimir valor de variable (p)
print config.Port            # imprimir campo de struct
print *ptr                   # derreferenciar puntero
print slice[0:5]             # imprimir rango de slice
print len(mySlice)           # llamar funciones built-in
locals                       # todas las variables locales
args                         # argumentos de la función actual
whatis myVar                 # tipo de la variable

# --- Call stack ---
stack                        # ver call stack (bt)
frame 2                      # cambiar al frame 2
up                           # subir un frame
down                         # bajar un frame

# --- Goroutines ---
goroutines                   # listar todas las goroutines (grs)
goroutine 5                  # cambiar a goroutine 5 (gr 5)
goroutines -exec bt          # stack trace de TODAS las goroutines

# --- Otros ---
list                         # mostrar código alrededor de la línea actual
exit                         # salir de delve
help                         # lista completa de comandos
```

### 8.3 Ejemplo práctico: depurar un servidor HTTP

```bash
# Escenario: tu servidor devuelve 500 y no sabes por qué

# 1. Iniciar delve en modo debug:
dlv debug ./cmd/server -- --port=8080

# 2. Poner breakpoint en el handler sospechoso:
(dlv) break internal/handlers/user.go:45

# 3. Continuar (el servidor arranca y espera requests):
(dlv) continue

# 4. Desde otra terminal, hacer el request que falla:
# curl http://localhost:8080/api/users/42

# 5. delve se detiene en el breakpoint:
(dlv) print r.URL.Path
"/api/users/42"
(dlv) print userID
42
(dlv) next
(dlv) print err
*errors.errorString {s: "sql: no rows in result set"}

# 6. ¡El error está claro! El handler no maneja "not found" correctamente.
(dlv) continue
(dlv) exit
```

### 8.4 Depurar goroutines

```go
// Este es uno de los superpoderes de delve vs GDB:
// delve entiende goroutines nativamente.

package main

import (
    "fmt"
    "sync"
    "time"
)

func worker(id int, wg *sync.WaitGroup) {
    defer wg.Done()
    time.Sleep(time.Second)
    fmt.Printf("worker %d done\n", id) // ← breakpoint aquí
}

func main() {
    var wg sync.WaitGroup
    for i := 0; i < 5; i++ {
        wg.Add(1)
        go worker(i, &wg)
    }
    wg.Wait()
}
```

```bash
# Depurar:
dlv debug main.go

(dlv) break main.worker:3         # línea del Printf
(dlv) continue

# Se detiene en una goroutine:
(dlv) goroutines
  Goroutine 1 - User: main.go:19 main.main (0x4a1234)
  Goroutine 6 - User: main.go:12 main.worker (0x4a1100) (current)
  Goroutine 7 - User: main.go:12 main.worker (0x4a1100)
  Goroutine 8 - User: runtime/time.go:100 time.Sleep (0x4001fc)
  Goroutine 9 - User: runtime/time.go:100 time.Sleep (0x4001fc)
  Goroutine 10 - User: runtime/time.go:100 time.Sleep (0x4001fc)

# Inspeccionar goroutine actual:
(dlv) print id
2

# Cambiar a otra goroutine:
(dlv) goroutine 7
(dlv) print id
3

# Stack trace de TODAS las goroutines:
(dlv) goroutines -exec bt
```

### 8.5 Delve con editores e IDEs

```bash
# VS Code — la integración más usada:
# 1. Instalar extensión "Go" (by Google)
# 2. F5 inicia la depuración con delve automáticamente
# 3. Soporta breakpoints, watch, call stack, goroutines
# 4. Configurar en .vscode/launch.json:

# {
#   "version": "0.2.0",
#   "configurations": [
#     {
#       "name": "Launch Package",
#       "type": "go",
#       "request": "launch",
#       "mode": "auto",
#       "program": "${workspaceFolder}/cmd/myapp",
#       "args": ["--config", "dev.yaml"],
#       "env": {
#         "APP_ENV": "development"
#       }
#     },
#     {
#       "name": "Debug Tests",
#       "type": "go",
#       "request": "launch",
#       "mode": "test",
#       "program": "${workspaceFolder}/internal/config",
#       "args": ["-run", "TestLoad"]
#     },
#     {
#       "name": "Attach to Process",
#       "type": "go",
#       "request": "attach",
#       "mode": "local",
#       "processId": 0
#     }
#   ]
# }

# Neovim — con nvim-dap y nvim-dap-go:
# Instalar: nvim-dap, nvim-dap-ui, nvim-dap-go
# Configuración automática con require('dap-go').setup()

# JetBrains GoLand:
# Depuración integrada nativa, no necesita configuración extra
```

### 8.6 Depuración remota (servidores / contenedores)

```bash
# Escenario SysAdmin: depurar un servicio corriendo en un servidor remoto
# o dentro de un contenedor Docker.

# --- Opción 1: delve headless en servidor remoto ---

# En el servidor remoto:
dlv debug ./cmd/myapp --headless --listen=:2345 --api-version=2 \
    --accept-multiclient

# En tu máquina local (con SSH port forwarding):
ssh -L 2345:localhost:2345 usuario@servidor
dlv connect localhost:2345

# --- Opción 2: Dentro de Docker ---

# Dockerfile.debug:
# FROM golang:1.23
# RUN go install github.com/go-delve/delve/cmd/dlv@latest
# WORKDIR /app
# COPY . .
# RUN go build -gcflags="all=-N -l" -o /app/myapp ./cmd/myapp
# EXPOSE 8080 2345
# CMD ["dlv", "exec", "/app/myapp", "--headless", "--listen=:2345",
#      "--api-version=2", "--accept-multiclient", "--", "--port=8080"]

# docker-compose.debug.yml:
# services:
#   myapp:
#     build:
#       context: .
#       dockerfile: Dockerfile.debug
#     ports:
#       - "8080:8080"
#       - "2345:2345"
#     security_opt:
#       - "seccomp:unconfined"   # necesario para que delve funcione
#     cap_add:
#       - SYS_PTRACE            # necesario para attach

# Conectar desde VS Code:
# {
#   "name": "Docker Attach",
#   "type": "go",
#   "request": "attach",
#   "mode": "remote",
#   "remotePath": "/app",
#   "port": 2345,
#   "host": "127.0.0.1"
# }
```

### 8.7 Core dumps en Go

```bash
# Go soporta core dumps para análisis post-mortem.

# Habilitar core dumps:
export GOTRACEBACK=crash

# Ejecutar el programa (si crashea, genera core dump):
ulimit -c unlimited
./myapp

# Analizar el core dump con delve:
dlv core ./myapp core.12345

# Dentro de delve, puedes inspeccionar:
(dlv) goroutines           # ver todas las goroutines al momento del crash
(dlv) goroutine 1
(dlv) bt                   # stack trace
(dlv) frame 3              # ir al frame del crash
(dlv) locals               # variables locales
(dlv) print myStruct       # inspeccionar estado

# ¿Por qué importa para SysAdmin?
# Un servicio crasheó a las 3am → el core dump te permite
# hacer "autopsia" sin necesidad de reproducir el bug.
```

---

## 9. Race detector — detectar data races

El race detector de Go es una herramienta integrada que detecta accesos
concurrentes no sincronizados a memoria compartida.

```bash
# Activar race detector:
go run -race main.go           # ejecutar con race detector
go test -race ./...            # tests con race detector
go build -race -o myapp .      # compilar con race detector

# ⚠️ El race detector tiene un coste:
# - ~2-10x más lento
# - ~5-10x más memoria
# - NO usar en producción (excepto canary deployments)
# - SÍ usar en tests y development
```

### 9.1 Ejemplo de data race detectada

```go
package main

import (
    "fmt"
    "sync"
)

func main() {
    counter := 0
    var wg sync.WaitGroup

    for i := 0; i < 1000; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            counter++    // ← DATA RACE: escritura no sincronizada
        }()
    }
    wg.Wait()
    fmt.Println(counter) // ← DATA RACE: lectura no sincronizada
}
```

```bash
$ go run -race main.go

==================
WARNING: DATA RACE
Write at 0x00c0000b4010 by goroutine 7:
  main.main.func1()
      /home/user/main.go:15 +0x38

Previous write at 0x00c0000b4010 by goroutine 6:
  main.main.func1()
      /home/user/main.go:15 +0x38

Goroutine 7 (running) created at:
  main.main()
      /home/user/main.go:13 +0x84

Goroutine 6 (finished) created at:
  main.main()
      /home/user/main.go:13 +0x84
==================
```

```go
// Fix: usar sync.Mutex o sync/atomic
import "sync/atomic"

func main() {
    var counter int64
    var wg sync.WaitGroup

    for i := 0; i < 1000; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            atomic.AddInt64(&counter, 1)  // ✓ atómico
        }()
    }
    wg.Wait()
    fmt.Println(atomic.LoadInt64(&counter))
}
```

### 9.2 Race detector en CI

```bash
# SIEMPRE ejecutar tests con race detector en CI:
go test -race -count=1 ./...

# -count=1 desactiva el cache de tests (importante con -race
# porque un test cacheado no re-ejecuta el race detector)

# En Makefile:
# test:
#     go test -race -count=1 -timeout=5m ./...

# En GitHub Actions:
# - name: Test with race detector
#   run: go test -race -count=1 -timeout=5m ./...
```

---

## 10. pprof — profiling de rendimiento

`pprof` es la herramienta integrada de profiling. Permite analizar
uso de CPU, memoria, goroutines y bloqueos.

```go
// Habilitar pprof en un servidor HTTP:
import (
    "net/http"
    _ "net/http/pprof"  // blank import registra endpoints de pprof
)

func main() {
    // Si ya tienes un servidor HTTP, pprof se registra automáticamente.
    // Si no, crear uno en un puerto separado:
    go func() {
        http.ListenAndServe("localhost:6060", nil)
    }()

    // ... tu aplicación ...
}
```

```bash
# Acceder a los perfiles:

# Perfil de CPU (30 segundos de muestreo):
go tool pprof http://localhost:6060/debug/pprof/profile?seconds=30

# Perfil de memoria (heap):
go tool pprof http://localhost:6060/debug/pprof/heap

# Goroutines activas:
go tool pprof http://localhost:6060/debug/pprof/goroutine

# Bloqueos (requiere runtime.SetBlockProfileRate):
go tool pprof http://localhost:6060/debug/pprof/block

# Mutex contention (requiere runtime.SetMutexProfileFraction):
go tool pprof http://localhost:6060/debug/pprof/mutex

# Dentro de pprof:
(pprof) top 10          # las 10 funciones más costosas
(pprof) top -cum        # ordenar por tiempo acumulado
(pprof) list myFunction # ver línea por línea el coste de una función
(pprof) web             # abrir gráfico interactivo en navegador
(pprof) svg > out.svg   # exportar a SVG

# Ver goroutines sin pprof interactivo:
curl http://localhost:6060/debug/pprof/goroutine?debug=2
# Esto muestra TODAS las goroutines con stack traces en texto plano
```

### 10.1 pprof para diagnosticar en producción

```bash
# ¿Por qué importa para SysAdmin?
#
# Escenario: el servicio consume 2GB de RAM y no sabes por qué.
#
# 1. curl http://servicio:6060/debug/pprof/heap > heap.prof
# 2. go tool pprof heap.prof
# 3. (pprof) top
#    → descubres que json.Unmarshal consume 1.5GB
#    → hay un leak de buffers
#
# Escenario: el servicio tiene latencia alta intermitente.
#
# 1. go tool pprof http://servicio:6060/debug/pprof/profile?seconds=30
# 2. (pprof) top -cum
#    → descubres que 80% del tiempo se gasta en DNS lookups
#    → necesitas un DNS cache local

# ⚠️ Seguridad: NUNCA exponer pprof a internet.
# Siempre usar localhost o red interna:
# http.ListenAndServe("localhost:6060", nil)     # ✓ solo local
# http.ListenAndServe(":6060", nil)              # ✗ expuesto a internet
```

---

## 11. gosec — análisis de seguridad

`gosec` analiza código Go buscando patrones de seguridad problemáticos.
Está incluido como linter en golangci-lint.

```bash
# Instalar:
go install github.com/securego/gosec/v2/cmd/gosec@latest

# Uso:
gosec ./...                           # analizar todo el proyecto
gosec -fmt=json -out=results.json ./... # output JSON
gosec -severity=medium ./...          # solo medium y high severity
gosec -exclude=G104 ./...             # excluir regla específica
```

### 11.1 Reglas más relevantes para SysAdmin

```go
// G101: credenciales hardcodeadas
var password = "admin123"            // ← G101
var apiKey = "sk-xxxxxxxxxxxx"       // ← G101

// G104: error no chequeado (también lo cubre errcheck)
f, _ := os.Open("/etc/config")      // ← G104

// G107: URL proporcionada por variable (SSRF potencial)
resp, _ := http.Get(userInput)      // ← G107

// G108: pprof expuesto en endpoint público
import _ "net/http/pprof"           // ← G108 (en handler público)

// G110: decompression bomb
io.Copy(w, gzip.NewReader(r))       // ← G110: sin límite de tamaño

// G114: http.ListenAndServe sin timeouts
http.ListenAndServe(":8080", nil)   // ← G114
// Fix:
srv := &http.Server{
    Addr:         ":8080",
    ReadTimeout:  10 * time.Second,
    WriteTimeout: 10 * time.Second,
    IdleTimeout:  120 * time.Second,
}
srv.ListenAndServe()

// G204: command injection
cmd := exec.Command("bash", "-c", userInput)  // ← G204

// G301: permisos de directorio demasiado amplios
os.Mkdir("/tmp/data", 0777)          // ← G301: usar 0750

// G304: file path desde variable (path traversal)
data, _ := os.ReadFile(r.URL.Query().Get("file"))  // ← G304

// G306: permisos de archivo demasiado amplios
os.WriteFile("config.yaml", data, 0666)  // ← G306: usar 0644
```

---

## 12. govulncheck — escáner de vulnerabilidades

`govulncheck` analiza las dependencias de tu proyecto contra la base
de datos de vulnerabilidades de Go (vuln.go.dev).

```bash
# Instalar:
go install golang.org/x/vuln/cmd/govulncheck@latest

# Analizar el proyecto:
govulncheck ./...

# Analizar un binario compilado:
govulncheck -mode=binary ./myapp

# Output JSON:
govulncheck -json ./...
```

```bash
# Ejemplo de output:

# Scanning your code and 45 packages across 12 modules...
#
# Vulnerability #1: GO-2024-2687
#   net/http: memory exhaustion in Request.ParseMultipartForm
#   More info: https://pkg.go.dev/vuln/GO-2024-2687
#   Module: stdlib
#   Found in: go@1.22.0
#   Fixed in: go@1.22.1
#   Example traces found:
#     #1: cmd/myapp/main.go:42:18 → net/http.(*Request).ParseMultipartForm

# govulncheck solo reporta vulnerabilidades que tu código
# REALMENTE LLAMA (no solo las que están en dependencias).
# Esto reduce dramáticamente los falsos positivos.
```

```bash
# En CI — fallar el pipeline si hay vulnerabilidades:
govulncheck ./... || exit 1

# En Makefile:
# vuln:
#     govulncheck ./...

# En GitHub Actions:
# - name: Check vulnerabilities
#   run: |
#     go install golang.org/x/vuln/cmd/govulncheck@latest
#     govulncheck ./...
```

---

## 13. Herramientas de testing avanzado

Además de `go test` (cubierto en T01), Go tiene herramientas de
testing avanzado integradas.

### 13.1 go test -cover: cobertura de tests

```bash
# Ver porcentaje de cobertura:
go test -cover ./...

# Generar perfil de cobertura:
go test -coverprofile=coverage.out ./...

# Visualizar cobertura en HTML:
go tool cover -html=coverage.out -o coverage.html
# Abre coverage.html en el navegador — verde=cubierto, rojo=no cubierto

# Ver cobertura por función:
go tool cover -func=coverage.out

# Cobertura acumulada de todo el proyecto:
go test -coverprofile=coverage.out -coverpkg=./... ./...
# -coverpkg=./... incluye paquetes que se llaman desde tests
# de otros paquetes (no solo tests del mismo paquete)

# En CI — requerir mínimo de cobertura:
COVERAGE=$(go test -coverprofile=coverage.out ./... 2>&1 | \
    grep "^ok" | awk '{print $NF}' | sed 's/%//' | \
    awk '{sum+=$1; n++} END {print sum/n}')
if (( $(echo "$COVERAGE < 80" | bc -l) )); then
    echo "Coverage $COVERAGE% is below 80% threshold"
    exit 1
fi
```

### 13.2 go test -bench: benchmarks

```go
// internal/hash/hash_test.go
package hash

import "testing"

func BenchmarkSHA256(b *testing.B) {
    data := []byte("hello world")
    for b.Loop() {  // Go 1.24+: reemplaza b.N
        sha256Sum(data)
    }
}

func BenchmarkMD5(b *testing.B) {
    data := []byte("hello world")
    for b.Loop() {
        md5Sum(data)
    }
}
```

```bash
# Ejecutar benchmarks:
go test -bench=. ./internal/hash/
go test -bench=BenchmarkSHA256 ./internal/hash/

# Con información de memoria:
go test -bench=. -benchmem ./internal/hash/

# Output:
# BenchmarkSHA256-8    5000000    312 ns/op    32 B/op    1 allocs/op
# BenchmarkMD5-8       8000000    198 ns/op    16 B/op    1 allocs/op

# Comparar benchmarks entre versiones (benchstat):
go install golang.org/x/perf/cmd/benchstat@latest
go test -bench=. -count=10 ./... > old.txt
# ... hacer cambios ...
go test -bench=. -count=10 ./... > new.txt
benchstat old.txt new.txt
```

### 13.3 go test -fuzz: fuzzing (Go 1.18+)

```go
// internal/parser/parser_test.go
package parser

import "testing"

func FuzzParseConfig(f *testing.F) {
    // Seed corpus — ejemplos iniciales:
    f.Add("host=localhost port=8080")
    f.Add("")
    f.Add("key=value\nkey2=value2")

    f.Fuzz(func(t *testing.T, input string) {
        cfg, err := ParseConfig(input)
        if err != nil {
            return  // errores están bien, no deben ser panics
        }
        // Si parseó correctamente, el resultado debe ser válido:
        if cfg.Host == "" && cfg.Port == 0 {
            t.Error("parsed successfully but config is empty")
        }
    })
}
```

```bash
# Ejecutar fuzzing (corre indefinidamente hasta encontrar un bug):
go test -fuzz=FuzzParseConfig ./internal/parser/

# Ejecutar por tiempo limitado:
go test -fuzz=FuzzParseConfig -fuzztime=30s ./internal/parser/

# Los inputs que causan fallos se guardan en:
# testdata/fuzz/FuzzParseConfig/
# Estos archivos se commitean y se ejecutan como tests normales.
```

---

## 14. Flujo de herramientas integrado

```
    ┌──────────────────────────────────────────────────────────────┐
    │              Flujo de desarrollo Go con herramientas          │
    │                                                              │
    │  ┌──────────┐   ┌──────────┐   ┌──────────┐                │
    │  │  Editor   │──▶│ goimports│──▶│ gopls    │                │
    │  │  save     │   │ (format  │   │ (LSP:    │                │
    │  │          │   │ +imports) │   │ errores, │                │
    │  │          │   │          │   │ refactor)│                │
    │  └──────────┘   └──────────┘   └──────────┘                │
    │       │                                                      │
    │       ▼                                                      │
    │  ┌──────────┐   ┌──────────┐   ┌──────────┐                │
    │  │ go vet   │──▶│golangci- │──▶│ go test  │                │
    │  │ (básico) │   │  lint    │   │ -race    │                │
    │  │          │   │ (100+    │   │ -cover   │                │
    │  │          │   │ linters) │   │          │                │
    │  └──────────┘   └──────────┘   └──────────┘                │
    │       │                                                      │
    │       ▼                                                      │
    │  ┌──────────┐   ┌──────────┐   ┌──────────┐                │
    │  │govulnchk │──▶│ go build │──▶│  deploy  │                │
    │  │ (vulns)  │   │ (static  │   │ (Docker/ │                │
    │  │          │   │  binary) │   │  scratch) │                │
    │  └──────────┘   └──────────┘   └──────────┘                │
    │                                                              │
    │  ┌──────────────────────────────────────────────────┐       │
    │  │  En caso de bug: delve (dlv) para depurar        │       │
    │  │  En caso de lentitud: pprof para profiling       │       │
    │  └──────────────────────────────────────────────────┘       │
    └──────────────────────────────────────────────────────────────┘
```

### 14.1 Makefile completo con todas las herramientas

```makefile
# Makefile — integración de herramientas Go para SysAdmin

APP_NAME    := myapp
VERSION     := $(shell git describe --tags --always --dirty 2>/dev/null || echo "dev")
COMMIT      := $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")
BUILD_TIME  := $(shell date -u '+%Y-%m-%dT%H:%M:%SZ')
LDFLAGS     := -ldflags "-s -w \
    -X main.version=$(VERSION) \
    -X main.commit=$(COMMIT) \
    -X main.buildTime=$(BUILD_TIME)"

.PHONY: help
help: ## mostrar esta ayuda
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | \
	    awk 'BEGIN {FS = ":.*?## "}; {printf "  %-15s %s\n", $$1, $$2}'

.PHONY: fmt
fmt: ## formatear código
	goimports -local example.com/$(APP_NAME) -w .

.PHONY: lint
lint: ## ejecutar linters
	golangci-lint run

.PHONY: vet
vet: ## go vet
	go vet ./...

.PHONY: test
test: ## ejecutar tests con race detector
	go test -race -count=1 -timeout=5m ./...

.PHONY: test-cover
test-cover: ## tests con reporte de cobertura
	go test -race -coverprofile=coverage.out -coverpkg=./... ./...
	go tool cover -html=coverage.out -o coverage.html
	@echo "Coverage report: coverage.html"

.PHONY: bench
bench: ## ejecutar benchmarks
	go test -bench=. -benchmem ./...

.PHONY: vuln
vuln: ## verificar vulnerabilidades
	govulncheck ./...

.PHONY: sec
sec: ## análisis de seguridad
	gosec ./...

.PHONY: build
build: ## compilar binario estático
	CGO_ENABLED=0 go build -trimpath $(LDFLAGS) -o bin/$(APP_NAME) ./cmd/$(APP_NAME)

.PHONY: check
check: fmt lint vet test vuln ## ejecutar todos los checks

.PHONY: ci
ci: check build ## pipeline de CI completo

.PHONY: clean
clean: ## limpiar artefactos
	rm -rf bin/ coverage.out coverage.html

.PHONY: tools
tools: ## instalar herramientas de desarrollo
	go install golang.org/x/tools/cmd/goimports@latest
	go install github.com/go-delve/delve/cmd/dlv@latest
	go install golang.org/x/vuln/cmd/govulncheck@latest
	go install golang.org/x/pkgsite/cmd/pkgsite@latest
	go install golang.org/x/perf/cmd/benchstat@latest
	@echo "golangci-lint: install from https://golangci-lint.run/usage/install/"
```

---

## 15. gopls — el Language Server Protocol de Go

`gopls` es el servidor LSP oficial de Go. Es lo que hace que los
editores (VS Code, Neovim, Emacs, Sublime) entiendan Go.

```bash
# Instalar (normalmente la extensión del editor lo hace automáticamente):
go install golang.org/x/tools/gopls@latest

# gopls proporciona:
# - Autocompletado inteligente
# - Go to definition
# - Find references
# - Rename symbol (refactoring seguro)
# - Hover documentation
# - Signature help
# - Code actions (auto-import, generate methods, extract function)
# - Diagnostics (errores de compilación en tiempo real)
# - Formateo (gofmt/gofumpt + goimports)
# - Folding ranges
# - Semantic highlighting
```

### 15.1 Configuración de gopls en editores

```bash
# VS Code settings.json — configuración recomendada:
# {
#   "go.useLanguageServer": true,
#   "gopls": {
#     "formatting.gofumpt": true,
#     "ui.semanticTokens": true,
#     "ui.completion.usePlaceholders": true,
#     "ui.diagnostic.analyses": {
#       "fieldalignment": true,
#       "shadow": true,
#       "unusedvariable": true,
#       "useany": true
#     }
#   },
#   "[go]": {
#     "editor.formatOnSave": true,
#     "editor.codeActionsOnSave": {
#       "source.organizeImports": "explicit"
#     }
#   }
# }

# Neovim (con nvim-lspconfig):
# require('lspconfig').gopls.setup({
#   settings = {
#     gopls = {
#       gofumpt = true,
#       analyses = {
#         shadow = true,
#         unusedvariable = true,
#         useany = true,
#       },
#       staticcheck = true,
#       hints = {
#         parameterNames = true,
#         compositeLiteralTypes = true,
#       },
#     },
#   },
# })
```

---

## 16. Tabla de errores comunes

| Error / Síntoma | Causa | Solución |
|---|---|---|
| `goimports: command not found` | No instalado o no en PATH | `go install golang.org/x/tools/cmd/goimports@latest` y verificar que `$(go env GOPATH)/bin` está en PATH |
| `golangci-lint: command not found` | No instalado | Instalar desde releases oficiales (no con `go install`) |
| `dlv: command not found` | Delve no instalado | `go install github.com/go-delve/delve/cmd/dlv@latest` |
| `could not attach to process` | Sin permisos ptrace | `sudo setcap cap_sys_ptrace+ep $(which dlv)` o `echo 0 > /proc/sys/kernel/yama/ptrace_scope` |
| `could not launch process: debugserver or lldb-server not found` | macOS sin Xcode CLI tools | `xcode-select --install` |
| golangci-lint muy lento | Sin cache o demasiados linters | Verificar que `.golangci-lint-cache` existe, reducir linters habilitados, usar `--new-from-rev` |
| `no required module provides package` (goimports) | goimports busca en módulos no descargados | `go mod download` para descargar dependencias |
| delve muestra `<optimized out>` | Binario compilado con optimizaciones | Compilar con `-gcflags="all=-N -l"` |
| `WARINING: DATA RACE` en tests | Acceso concurrente no sincronizado | Usar `sync.Mutex`, `sync/atomic`, o canales |
| pprof no responde en `localhost:6060` | No se importó `net/http/pprof` o el handler no está corriendo | Verificar `import _ "net/http/pprof"` y que el servidor HTTP esté arrancado |
| govulncheck no encuentra vulnerabilidades en binario | Binario stripped o sin debug info | Compilar sin `-s -w` para analysis de binario |
| `File is not goimports-ed` en CI | goimports local usa versión diferente al CI | Fijar versión de goimports en CI y local |

---

## 17. Ejercicios

### Ejercicio 1 — Formateo y simplificación

```
Crea un archivo con código Go deliberadamente mal formateado:
- Indentación con espacios
- Espaciado inconsistente
- Composite literals con tipos redundantes
- Slice expressions redundantes (s[0:len(s)])

1. Ejecuta gofmt -d para ver el diff sin modificar
2. Ejecuta gofmt -s -d para ver las simplificaciones adicionales
3. Aplica ambos con gofmt -s -w

Predicción: ¿cuántas líneas cambiará gofmt? ¿Y gofmt -s?
```

### Ejercicio 2 — goimports en acción

```
Crea un archivo que use fmt.Println, http.ListenAndServe
y json.Marshal pero sin ningún import. También incluye
un import de "os" que NO se usa.

1. Ejecuta goimports -d para ver qué haría
2. Ejecuta goimports -w para aplicar los cambios
3. ¿Añadió los imports faltantes? ¿Eliminó "os"?
4. Configura la opción -local con tu module path y verifica
   que los imports se separan en 3 grupos

Predicción: ¿en qué orden aparecerán los imports?
```

### Ejercicio 3 — golangci-lint desde cero

```
1. Instala golangci-lint siguiendo el método recomendado
2. Crea un proyecto Go con estos problemas deliberados:
   - Un error no chequeado: os.Remove("file")
   - Una variable asignada pero no leída
   - Un fmt.Sprintf redundante: fmt.Sprintf("%s", myString)
   - Un http.ListenAndServe sin timeouts
3. Ejecuta golangci-lint run y verifica qué linters detectan qué
4. Crea un .golangci.yml habilitando gosec y errcheck
5. Ejecuta de nuevo y compara el output

Predicción: ¿cuántos linters por defecto detectarán el error
no chequeado?
```

### Ejercicio 4 — Configurar .golangci.yml completo

```
1. Copia la configuración .golangci.yml de la sección 4.4
2. Crea un proyecto con al menos 5 archivos .go que tengan
   problemas variados (seguridad, estilo, errores, concurrencia)
3. Ejecuta golangci-lint run y clasifica cada warning:
   - ¿Qué linter lo detectó?
   - ¿Es un error real o un falso positivo?
4. Suprime un falso positivo con //nolint y explicación
5. Ejecuta golangci-lint run --new-from-rev=HEAD~1 y compara

Predicción: ¿qué warnings desaparecen con --new-from-rev?
```

### Ejercicio 5 — go doc y documentación propia

```
Crea un paquete internal/sysinfo con estas funciones:
- Hostname() (string, error)
- Uptime() (time.Duration, error)
- MemoryUsage() (*MemInfo, error)

1. Escribe los comentarios de documentación siguiendo las
   convenciones de Go (Package sysinfo..., función empieza
   con el nombre, etc.)
2. Usa go doc ./internal/sysinfo para ver la documentación
3. Usa go doc -all ./internal/sysinfo para ver todo
4. Usa go doc -src ./internal/sysinfo.Hostname para ver el source

Predicción: ¿qué aparece si escribes go doc ./internal/sysinfo.memHelper
(función no exportada)?
```

### Ejercicio 6 — Delve básico

```
Crea un programa que lea un archivo de configuración YAML,
lo parsee en un struct, y falle silenciosamente en algún campo.

1. Compila con -gcflags="all=-N -l"
2. Inicia dlv debug
3. Pon un breakpoint en la línea que parsea el YAML
4. Ejecuta continue, inspecciona el struct con print
5. Usa next para avanzar paso a paso
6. Identifica dónde se pierde el valor del campo

Predicción: ¿qué mostrará print config después del parseo?
¿Qué campo tendrá el zero value?
```

### Ejercicio 7 — Depurar goroutines con delve

```
Crea un programa con 5 goroutines que procesan items de un canal.
Incluye un bug deliberado: una goroutine que se queda bloqueada
(deadlock parcial).

1. Inicia dlv debug
2. Deja que corra unos segundos y luego presiona Ctrl+C
3. Usa goroutines para listar todas las goroutines
4. Identifica cuál está bloqueada y en qué línea
5. Usa goroutine <id> y bt para ver el stack trace

Predicción: ¿cuántas goroutines verás en total? (pista: no solo
las tuyas — el runtime crea goroutines propias)
```

### Ejercicio 8 — Race detector

```
Crea un programa con un mapa compartido entre goroutines:
- 3 goroutines escriben en el mapa
- 2 goroutines leen del mapa
- Sin ninguna sincronización

1. Ejecuta con go run main.go (¿funciona?)
2. Ejecuta con go run -race main.go
3. Lee el output del race detector y identifica qué goroutines
   colisionan
4. Arregla el bug con sync.RWMutex
5. Ejecuta de nuevo con -race para verificar

Predicción: ¿el programa sin -race crasheará o parecerá funcionar?
¿Por qué esto es peligroso?
```

### Ejercicio 9 — pprof básico

```
Crea un servidor HTTP que tenga un endpoint que hace trabajo
CPU-intensivo (ej: calcular hashes SHA256 en un loop).

1. Importa _ "net/http/pprof"
2. Arranca el servidor
3. Genera carga con: hey -n 1000 http://localhost:8080/hash
   (instalar hey: go install github.com/rakyll/hey@latest)
4. Mientras genera carga:
   go tool pprof http://localhost:6060/debug/pprof/profile?seconds=10
5. Usa top y list para identificar el hotspot

Predicción: ¿qué función aparecerá en el top del perfil de CPU?
```

### Ejercicio 10 — Pipeline CI completa

```
Crea un proyecto Go completo con:
- cmd/myapp/main.go (servidor HTTP simple)
- internal/handlers/health.go (health check handler)
- .golangci.yml (configuración de linters)
- Makefile con targets: fmt, lint, vet, test, vuln, build, ci

1. Ejecuta make ci y verifica que todo pasa
2. Introduce un bug deliberado (error no chequeado)
3. Ejecuta make ci — ¿qué target falla?
4. Arregla el bug y ejecuta make ci de nuevo
5. Ejecuta make test-cover y abre el HTML de cobertura

Predicción: ¿en qué orden se ejecutan los targets de make ci?
¿Cuál es el primero en fallar con el bug?
```

---

## 18. Resumen

```
    ┌───────────────────────────────────────────────────────────┐
    │                Herramientas Go — Resumen                  │
    │                                                           │
    │  FORMATEO:                                                │
    │  └─ gofmt → formato canónico, sin opciones               │
    │  └─ goimports → gofmt + imports automáticos               │
    │  └─ gofumpt → gofmt estricto (opcional)                   │
    │                                                           │
    │  LINTING:                                                 │
    │  └─ go vet → análisis estático básico (incluido)          │
    │  └─ staticcheck → análisis avanzado                       │
    │  └─ golangci-lint → meta-linter (100+ linters)            │
    │  └─ gosec → seguridad específica                          │
    │                                                           │
    │  DOCUMENTACIÓN:                                           │
    │  └─ go doc → consulta rápida en terminal                  │
    │  └─ pkgsite → servidor web local (como pkg.go.dev)        │
    │                                                           │
    │  DEPURACIÓN:                                              │
    │  └─ delve (dlv) → debugger nativo, entiende goroutines    │
    │  └─ -race → detector de data races                        │
    │  └─ pprof → profiling CPU/memoria/goroutines              │
    │                                                           │
    │  SEGURIDAD:                                               │
    │  └─ govulncheck → vulnerabilidades en dependencias        │
    │  └─ gosec → patrones inseguros en código propio           │
    │                                                           │
    │  LSP:                                                     │
    │  └─ gopls → Language Server para editores                 │
    │                                                           │
    │  REGLA DE ORO:                                            │
    │  Editor: goimports + gopls (al guardar)                   │
    │  Pre-commit: golangci-lint                                │
    │  CI: golangci-lint + go test -race + govulncheck          │
    │  Debug: delve cuando lo necesites                         │
    │  Performance: pprof cuando lo necesites                   │
    └───────────────────────────────────────────────────────────┘
```
