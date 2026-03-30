# T03 — Estructura de proyecto

## 1. El package main y el punto de entrada

Todo programa Go ejecutable necesita un paquete `main` con una función
`main()`. Este es el punto de entrada del programa.

```go
// main.go
package main    // ← obligatorio para ejecutables

import "fmt"

func main() {  // ← obligatorio, sin argumentos ni retorno
    fmt.Println("Hello")
}
```

```bash
# Reglas del package main:
#
# 1. El nombre del paquete DEBE ser "main"
# 2. DEBE tener una función main() sin argumentos ni retorno
# 3. Solo puede haber UN package main por binario
# 4. Los argumentos se obtienen con os.Args (no como parámetros de main)
# 5. El exit code se establece con os.Exit(n) (no con return)
```

```go
// ¿Por qué main() no recibe args ni devuelve int?
// Go usa os.Args y os.Exit explícitamente:

package main

import (
    "fmt"
    "os"
)

func main() {
    if len(os.Args) < 2 {
        fmt.Fprintf(os.Stderr, "usage: %s <name>\n", os.Args[0])
        os.Exit(1)   // ← exit code explícito
    }
    fmt.Printf("Hello, %s\n", os.Args[1])
    // exit code 0 implícito al terminar main()
}
```

### 1.1 Paquetes en Go — un directorio = un paquete

```
    ┌────────────────────────────────────────────────────────┐
    │ Regla fundamental:                                     │
    │                                                        │
    │ Un directorio = un paquete                             │
    │ Todos los .go en un directorio DEBEN tener el mismo    │
    │ "package nombre" (excepto archivos _test.go)           │
    │                                                        │
    │ myproject/                                             │
    │ ├── main.go         package main                       │
    │ ├── helpers.go      package main    ← MISMO paquete    │
    │ ├── utils/                                             │
    │ │   ├── strings.go  package utils   ← otro paquete     │
    │ │   └── numbers.go  package utils   ← MISMO que strings│
    │ └── config/                                            │
    │     └── load.go     package config  ← otro paquete     │
    └────────────────────────────────────────────────────────┘
```

```bash
# Error: mezclar package names en un directorio
# myproject/
# ├── main.go    → package main
# └── helper.go  → package helper   ← ERROR: no puede ser diferente

# go build
# can't load package: found packages main (main.go) and helper (helper.go)
```

### 1.2 Visibilidad — mayúscula = exportado

```go
// En Go no existe public/private/protected.
// La visibilidad se controla con la primera letra del nombre:

package config

// Exportado (visible desde otros paquetes) — empieza con MAYÚSCULA
var DefaultPort = 8080
func LoadConfig(path string) (*Config, error) { ... }
type Config struct { ... }

// No exportado (privado al paquete) — empieza con minúscula
var maxRetries = 3
func parseYAML(data []byte) (*Config, error) { ... }
type internalState struct { ... }
```

```go
// Desde otro paquete:
import "example.com/myapp/config"

cfg, err := config.LoadConfig("/etc/myapp/config.yaml")  // ✓ exportado
port := config.DefaultPort                                 // ✓ exportado
n := config.maxRetries                                     // ✗ no exportado
// config.maxRetries undefined (cannot refer to unexported field)
```

---

## 2. Proyecto mínimo — un solo archivo

Para scripts, herramientas pequeñas y prototipos, un solo archivo basta:

```
    myapp/
    ├── go.mod
    └── main.go
```

```bash
mkdir myapp && cd myapp
go mod init example.com/myapp
# Escribir main.go
go build -o myapp
```

Esto es perfectamente válido para:
- Scripts de automatización
- CLIs simples (un solo comando)
- Prototipos rápidos
- Herramientas internas de un solo propósito

---

## 3. Proyecto pequeño — múltiples archivos, un paquete

Cuando el código crece, se divide en múltiples archivos dentro del
mismo paquete `main`:

```
    myapp/
    ├── go.mod
    ├── go.sum
    ├── main.go          ← punto de entrada
    ├── config.go        ← lógica de configuración
    ├── handlers.go      ← handlers HTTP (o lógica principal)
    └── helpers.go       ← funciones auxiliares
```

```go
// main.go
package main

func main() {
    cfg := loadConfig()       // definida en config.go
    startServer(cfg)          // definida en handlers.go
}
```

```go
// config.go
package main

type AppConfig struct {
    Port    int
    LogPath string
}

func loadConfig() *AppConfig {
    return &AppConfig{Port: 8080, LogPath: "/var/log/myapp.log"}
}
```

```go
// handlers.go
package main

import (
    "fmt"
    "net/http"
)

func startServer(cfg *AppConfig) {
    http.HandleFunc("/health", healthHandler)
    fmt.Printf("Listening on :%d\n", cfg.Port)
    http.ListenAndServe(fmt.Sprintf(":%d", cfg.Port), nil)
}

func healthHandler(w http.ResponseWriter, r *http.Request) {
    fmt.Fprintln(w, "OK")
}
```

```bash
# Todos los archivos con package main se compilan juntos:
go build -o myapp
# Equivale a: go build -o myapp main.go config.go handlers.go helpers.go

# go build sin argumentos compila TODO el paquete del directorio actual
```

---

## 4. El layout estándar de Go

No existe un layout oficial dictado por el equipo de Go, pero la
comunidad ha convergido en una estructura de facto basada en los
proyectos más exitosos del ecosistema (Docker, Kubernetes, Prometheus,
Terraform).

```
    myproject/
    ├── cmd/                    ← binarios ejecutables
    │   ├── server/
    │   │   └── main.go         ← package main para "server"
    │   ├── cli/
    │   │   └── main.go         ← package main para "cli"
    │   └── worker/
    │       └── main.go         ← package main para "worker"
    │
    ├── internal/               ← paquetes PRIVADOS (Go los protege)
    │   ├── config/
    │   │   └── config.go
    │   ├── database/
    │   │   └── postgres.go
    │   └── middleware/
    │       └── auth.go
    │
    ├── pkg/                    ← paquetes públicos reutilizables
    │   ├── api/
    │   │   └── client.go
    │   └── models/
    │       └── user.go
    │
    ├── api/                    ← definiciones de API (OpenAPI, protobuf)
    │   └── v1/
    │       └── api.proto
    │
    ├── configs/                ← archivos de configuración de ejemplo
    │   ├── config.yaml
    │   └── config.example.yaml
    │
    ├── deployments/            ← configs de despliegue
    │   ├── docker/
    │   │   └── Dockerfile
    │   └── kubernetes/
    │       ├── deployment.yaml
    │       └── service.yaml
    │
    ├── scripts/                ← scripts de build, CI, instalación
    │   ├── build.sh
    │   └── test.sh
    │
    ├── docs/                   ← documentación adicional
    │   └── architecture.md
    │
    ├── go.mod
    ├── go.sum
    ├── Makefile                ← targets de build/test/deploy
    ├── .goreleaser.yaml        ← (opcional) release automation
    ├── .golangci.yaml          ← (opcional) config del linter
    └── README.md
```

### 4.1 Cómo se ven los proyectos reales

```bash
# Docker (moby):
# github.com/moby/moby/
# ├── cmd/
# │   └── dockerd/
# │       └── main.go
# ├── internal/
# ├── pkg/
# ├── api/
# └── ...

# Kubernetes:
# github.com/kubernetes/kubernetes/
# ├── cmd/
# │   ├── kubectl/
# │   ├── kube-apiserver/
# │   ├── kube-controller-manager/
# │   ├── kube-scheduler/
# │   └── kubelet/
# ├── pkg/
# ├── staging/
# └── ...

# Prometheus:
# github.com/prometheus/prometheus/
# ├── cmd/
# │   ├── prometheus/
# │   └── promtool/
# ├── model/
# ├── storage/
# └── ...

# Terraform:
# github.com/hashicorp/terraform/
# ├── command/
# ├── internal/
# ├── version/
# └── main.go        ← un solo binario, main.go en la raíz
```

---

## 5. cmd/ — múltiples binarios

El directorio `cmd/` contiene un subdirectorio por cada binario que
el proyecto produce. Cada subdirectorio tiene su propio `package main`.

```
    cmd/
    ├── server/
    │   └── main.go       package main → binario "server"
    ├── cli/
    │   └── main.go       package main → binario "cli"
    └── migrate/
        └── main.go       package main → binario "migrate"
```

```go
// cmd/server/main.go
package main

import (
    "fmt"
    "log"
    "net/http"

    "example.com/myapp/internal/config"
    "example.com/myapp/internal/handlers"
)

func main() {
    cfg, err := config.Load()
    if err != nil {
        log.Fatalf("failed to load config: %v", err)
    }

    router := handlers.NewRouter(cfg)
    addr := fmt.Sprintf(":%d", cfg.Port)
    log.Printf("Starting server on %s", addr)
    log.Fatal(http.ListenAndServe(addr, router))
}
```

```go
// cmd/cli/main.go
package main

import (
    "fmt"
    "os"

    "example.com/myapp/internal/config"
    "example.com/myapp/pkg/api"
)

func main() {
    cfg, _ := config.Load()
    client := api.NewClient(cfg.APIEndpoint)

    result, err := client.GetStatus()
    if err != nil {
        fmt.Fprintf(os.Stderr, "error: %v\n", err)
        os.Exit(1)
    }
    fmt.Println(result)
}
```

```go
// cmd/migrate/main.go
package main

import (
    "log"

    "example.com/myapp/internal/config"
    "example.com/myapp/internal/database"
)

func main() {
    cfg, _ := config.Load()
    db, err := database.Connect(cfg.DatabaseURL)
    if err != nil {
        log.Fatal(err)
    }
    defer db.Close()

    if err := database.RunMigrations(db); err != nil {
        log.Fatal(err)
    }
    log.Println("Migrations completed successfully")
}
```

### 5.1 Compilar binarios desde cmd/

```bash
# Compilar un binario específico:
go build -o bin/server ./cmd/server
go build -o bin/cli ./cmd/cli
go build -o bin/migrate ./cmd/migrate

# Compilar todos los binarios:
go build -o bin/server ./cmd/server && \
go build -o bin/cli ./cmd/cli && \
go build -o bin/migrate ./cmd/migrate

# O con un loop:
for cmd in server cli migrate; do
    go build -o "bin/$cmd" "./cmd/$cmd"
done

# O con Makefile (ver sección 12)
```

### 5.2 cmd/ mínimo — el patrón "thin main"

```go
// ANTI-PATRÓN: lógica de negocio en cmd/server/main.go
// ✗ main.go con cientos de líneas de lógica

// PATRÓN CORRECTO: main.go solo conecta componentes (thin main)
// cmd/server/main.go
package main

import (
    "log"
    "os"

    "example.com/myapp/internal/app"
)

func main() {
    if err := app.Run(os.Args[1:]); err != nil {
        log.Fatal(err)
    }
}
```

```bash
# ¿Por qué thin main?
#
# 1. main() no se puede testear (no recibe ni devuelve)
# 2. La lógica en internal/app/app.go SÍ se puede testear
# 3. Múltiples binarios pueden reutilizar la misma lógica
# 4. Separación clara: main = wiring, internal = lógica
```

### 5.3 main.go en la raíz — cuándo es válido

```bash
# Si el proyecto produce UN solo binario, es aceptable
# poner main.go en la raíz:

# myapp/
# ├── go.mod
# ├── main.go         ← package main directamente
# ├── internal/
# └── ...

# Esto es lo que hace Terraform (main.go en raíz)
# y muchas CLIs pequeñas.

# Ventaja: go install example.com/myapp@latest funciona directamente
# Sin cmd/: go install example.com/myapp@latest → binario "myapp"
# Con cmd/: go install example.com/myapp/cmd/server@latest → binario "server"
```

---

## 6. internal/ — paquetes privados protegidos por el compilador

`internal/` es **especial en Go**: el compilador **impide** que código
externo al módulo importe paquetes dentro de `internal/`.

```
    myproject/
    ├── internal/
    │   └── config/
    │       └── config.go     ← package config
    ├── pkg/
    │   └── api/
    │       └── client.go     ← package api
    └── cmd/
        └── server/
            └── main.go
```

```go
// Desde cmd/server/main.go (DENTRO del módulo):
import "example.com/myproject/internal/config"   // ✓ permitido

// Desde otro módulo externo:
import "example.com/myproject/internal/config"   // ✗ PROHIBIDO
// use of internal package example.com/myproject/internal/config
// not allowed
```

```go
// Desde otro módulo externo:
import "example.com/myproject/pkg/api"           // ✓ permitido
// pkg/ no tiene restricciones de acceso
```

### 6.1 Qué poner en internal/

```bash
# internal/ es para código que:
# - Solo tiene sentido dentro de este proyecto
# - No quieres que otros dependan de él
# - Puede cambiar sin previo aviso (no es API pública)

# Ejemplos típicos:
# internal/
# ├── config/       ← carga de configuración del proyecto
# ├── database/     ← conexión a la DB específica del proyecto
# ├── handlers/     ← handlers HTTP del servidor
# ├── middleware/    ← middleware específico
# ├── models/       ← modelos de dominio internos
# ├── metrics/      ← configuración de Prometheus
# ├── worker/       ← lógica del worker de background
# └── testutil/     ← helpers para tests
```

### 6.2 Niveles de internal/

```bash
# internal/ puede estar a cualquier nivel del árbol:

# myproject/
# ├── internal/              ← privado a todo el módulo
# │   └── core/
# ├── pkg/
# │   └── api/
# │       └── internal/      ← privado solo al paquete api
# │           └── auth.go
# └── cmd/
#     └── server/
#         └── internal/      ← privado solo al binario server
#             └── setup.go

# La regla: solo código en el directorio PADRE de internal/
# (y sus subdirectorios) puede importar de ese internal/.
```

### 6.3 internal/ en proyectos reales del ecosistema

```bash
# Kubernetes usa internal/ extensivamente:
# k8s.io/kubernetes/
# ├── pkg/              ← API pública estable
# │   ├── api/
# │   └── apis/
# └── internal/         ← implementación interna
#     └── ...

# Docker (moby):
# github.com/moby/moby/
# └── internal/
#     ├── test/
#     ├── safepath/
#     └── ...

# HashiCorp Consul:
# github.com/hashicorp/consul/
# └── internal/
#     ├── resource/
#     ├── catalog/
#     └── ...
```

---

## 7. pkg/ — paquetes públicos reutilizables

`pkg/` es un directorio **convencional** (no especial para Go) para
código que está diseñado para ser importado por otros módulos.

```bash
# pkg/ contiene código que:
# - Forma parte de tu API pública
# - Puede ser importado por otros proyectos
# - Tiene una interfaz estable (o versionada)

# Ejemplos:
# pkg/
# ├── api/          ← cliente HTTP para tu servicio
# │   └── client.go
# ├── models/       ← tipos compartidos (request/response)
# │   └── user.go
# ├── metrics/      ← helpers de métricas reutilizables
# │   └── prometheus.go
# └── health/       ← health check reutilizable
#     └── checker.go
```

### 7.1 ¿Es necesario pkg/?

```bash
# Opinión del equipo de Go (Russ Cox):
# "No usen pkg/ si no lo necesitan"
#
# El directorio pkg/ es una convención de la comunidad, no del lenguaje.
# Muchos proyectos exitosos NO usan pkg/:
#
# Terraform   → command/, internal/, version/ (sin pkg/)
# Hugo        → commands/, hugolib/, resources/ (sin pkg/)
# CockroachDB → pkg/ (sí lo usa)
# Kubernetes  → pkg/ (sí lo usa)
#
# Regla práctica:
# - Si tu proyecto es una aplicación (un servidor, una CLI):
#   probablemente NO necesitas pkg/
# - Si tu proyecto es una librería o tiene código reutilizable:
#   pkg/ ayuda a separar la API pública del internal
```

### 7.2 pkg/ vs internal/ — cuándo usar cada uno

```
    ┌──────────────────────────────────────────────────────────┐
    │                   ¿Dónde va mi código?                    │
    │                                                           │
    │   ¿Lo importará otro módulo/proyecto?                     │
    │        │                                                  │
    │        ├── SÍ → pkg/  (API pública, estable)             │
    │        │                                                  │
    │        └── NO → internal/  (implementación privada)      │
    │                                                           │
    │   ¿Es una aplicación sin código reutilizable?            │
    │        │                                                  │
    │        └── SÍ → internal/ solamente (sin pkg/)           │
    └──────────────────────────────────────────────────────────┘
```

---

## 8. Otros directorios convencionales

### 8.1 api/ — definiciones de interfaz

```bash
# api/ contiene las definiciones de la API:
# - OpenAPI/Swagger specs
# - Protocol Buffer definitions (.proto)
# - GraphQL schemas
# - JSON Schema

# api/
# ├── v1/
# │   ├── service.proto
# │   └── openapi.yaml
# └── v2/
#     └── service.proto
```

### 8.2 configs/ — configuración de ejemplo

```bash
# configs/ (o config/) contiene archivos de configuración de ejemplo:
#
# configs/
# ├── config.yaml           ← configuración por defecto
# ├── config.example.yaml   ← template para nuevos deployments
# ├── config.dev.yaml       ← override para desarrollo
# └── config.prod.yaml      ← override para producción

# NUNCA commitear secrets en este directorio.
# Usar variables de entorno o secret managers para datos sensibles.
```

### 8.3 deployments/ — infraestructura como código

```bash
# deployments/ (o deploy/) contiene toda la config de despliegue:
#
# deployments/
# ├── docker/
# │   ├── Dockerfile
# │   └── docker-compose.yaml
# ├── kubernetes/
# │   ├── deployment.yaml
# │   ├── service.yaml
# │   ├── configmap.yaml
# │   └── kustomization.yaml
# ├── terraform/
# │   ├── main.tf
# │   ├── variables.tf
# │   └── outputs.tf
# ├── helm/
# │   └── myapp/
# │       ├── Chart.yaml
# │       ├── values.yaml
# │       └── templates/
# └── systemd/
#     └── myapp.service

# Para SysAdmin/DevOps, este directorio es tan importante como el código.
```

### 8.4 scripts/ — automatización

```bash
# scripts/ contiene scripts de soporte:
#
# scripts/
# ├── build.sh          ← compilar todos los binarios
# ├── test.sh           ← ejecutar suite de tests completa
# ├── lint.sh           ← ejecutar linters
# ├── install.sh        ← instalación en el sistema
# ├── release.sh        ← publicar nueva versión
# ├── dev-setup.sh      ← configurar entorno de desarrollo
# └── generate.sh       ← ejecutar go generate
```

### 8.5 test/ — tests de integración y datos

```bash
# test/ o testdata/ para recursos de testing:
#
# test/
# ├── integration/
# │   └── api_test.go        ← tests de integración
# ├── e2e/
# │   └── smoke_test.go      ← tests end-to-end
# └── testdata/
#     ├── fixtures/
#     │   ├── valid.json
#     │   └── invalid.json
#     └── golden/
#         └── expected_output.txt

# testdata/ es especial: Go ignora este directorio al compilar
# pero está disponible para tests.
```

### 8.6 tools/ — dependencias de herramientas

```bash
# tools/ (o tools.go) declara dependencias de herramientas de desarrollo:

# tools/tools.go
# //go:build tools
#
# package tools
#
# import (
#     _ "github.com/golangci/golangci-lint/cmd/golangci-lint"
#     _ "google.golang.org/protobuf/cmd/protoc-gen-go"
#     _ "github.com/sqlc-dev/sqlc/cmd/sqlc"
# )

# El build tag "tools" evita que se compile con el proyecto.
# Pero go mod tidy las incluye en go.mod, permitiendo
# instalarlas con:
# go install github.com/golangci/golangci-lint/cmd/golangci-lint
```

### 8.7 build/ — artefactos de build

```bash
# build/ contiene empaquetado y CI:
#
# build/
# ├── ci/               ← scripts de CI/CD
# │   ├── Jenkinsfile
# │   └── .github/
# │       └── workflows/
# │           └── ci.yaml
# └── package/           ← empaquetado del sistema
#     ├── rpm/
#     │   └── myapp.spec
#     └── deb/
#         └── control
```

---

## 9. El directorio bin/ y .gitignore

```bash
# bin/ contiene los binarios compilados (NO se commitea):
#
# bin/
# ├── server
# ├── cli
# └── migrate

# .gitignore típico para un proyecto Go:
cat > .gitignore << 'EOF'
# Binarios compilados
bin/
*.exe
*.exe~
*.dll
*.so
*.dylib

# Archivos de test
*.test
*.out
coverage.out
coverage.html

# Directorios de herramientas
.idea/
.vscode/
*.swp
*~

# Workspace (desarrollo local)
go.work
go.work.sum

# OS files
.DS_Store
Thumbs.db

# Vendor (si no se commitea)
# vendor/

# Archivos de entorno con secrets
.env
.env.local
*.pem
*.key
EOF
```

---

## 10. Imports — cómo se referencian los paquetes

### 10.1 Import paths

```go
// Los imports en Go usan la ruta completa del paquete:

import (
    // Standard library — sin prefijo de dominio
    "fmt"
    "net/http"
    "os"
    "encoding/json"

    // Paquetes del propio módulo — prefijo del module path
    "example.com/myapp/internal/config"
    "example.com/myapp/internal/handlers"
    "example.com/myapp/pkg/api"

    // Paquetes externos — ruta completa del repositorio
    "github.com/spf13/cobra"
    "github.com/prometheus/client_golang/prometheus"
    "go.uber.org/zap"
)
```

### 10.2 Convención de agrupación de imports

```go
// goimports agrupa automáticamente en tres bloques
// separados por líneas en blanco:

import (
    // 1. Standard library
    "context"
    "fmt"
    "net/http"

    // 2. Paquetes externos (terceros)
    "github.com/gorilla/mux"
    "go.uber.org/zap"

    // 3. Paquetes del propio módulo
    "example.com/myapp/internal/config"
    "example.com/myapp/internal/handlers"
)

// goimports hace esto automáticamente.
// Por eso los equipos de Go lo ejecutan al guardar en el editor.
```

### 10.3 Import aliases

```go
// Renombrar un import para evitar colisiones o mejorar claridad:

import (
    "crypto/rand"
    mathrand "math/rand"   // alias para evitar colisión con crypto/rand

    "github.com/sirupsen/logrus"
    log "github.com/sirupsen/logrus"  // alias corto

    // Blank import — solo ejecuta init() del paquete
    _ "github.com/lib/pq"  // registra el driver PostgreSQL

    // Dot import — importa todo al namespace actual (EVITAR)
    // . "fmt"  // permite Println() en vez de fmt.Println()
    // Casi nunca es buena idea, excepto en archivos _test.go
)
```

### 10.4 Importar paquetes internos del mismo módulo

```go
// module example.com/deployer (definido en go.mod)

// cmd/deployer/main.go puede importar:
import (
    "example.com/deployer/internal/ssh"       // ✓
    "example.com/deployer/internal/inventory"  // ✓
    "example.com/deployer/pkg/report"          // ✓
)

// Los imports SIEMPRE usan la ruta completa desde el module path.
// No hay imports relativos en Go (no existe import "./config").
```

---

## 11. go generate — generación de código

`go generate` ejecuta comandos embebidos en comentarios `//go:generate`
dentro de archivos Go. Es el mecanismo estándar para generación de código.

```go
// models/user.go

//go:generate stringer -type=Role
type Role int

const (
    Admin Role = iota
    Editor
    Viewer
)

// Ejecutar: go generate ./...
// Genera: models/role_string.go con func (r Role) String() string
```

### 11.1 Usos comunes de go generate

```go
// 1. Protocol Buffers:
//go:generate protoc --go_out=. --go-grpc_out=. api/v1/service.proto

// 2. SQL type-safe (sqlc):
//go:generate sqlc generate

// 3. Mock generation (mockgen):
//go:generate mockgen -source=interfaces.go -destination=mocks/mock_store.go

// 4. Embed static files (antes de Go 1.16):
//go:generate go-bindata -o assets.go -pkg main ./static/...

// 5. Stringer (enum → String()):
//go:generate stringer -type=Status

// 6. Enumer (enum con métodos adicionales):
//go:generate enumer -type=Color -json -text
```

```bash
# Ejecutar todos los go:generate del proyecto:
go generate ./...

# Solo un paquete:
go generate ./internal/models/...

# go generate NO se ejecuta automáticamente.
# Debes ejecutarlo explícitamente y commitear el código generado.
# En CI, verificar que el código generado está actualizado:
go generate ./...
git diff --exit-code
```

---

## 12. go:embed — embeber archivos en el binario

Desde Go 1.16, `//go:embed` permite incluir archivos estáticos
directamente en el binario compilado.

```go
package main

import (
    "embed"
    "fmt"
    "net/http"
)

// Embeber un solo archivo:
//go:embed configs/default.yaml
var defaultConfig string

// Embeber múltiples archivos:
//go:embed static/*
var staticFiles embed.FS

// Embeber archivos como bytes:
//go:embed version.txt
var version []byte

func main() {
    fmt.Println("Config:", defaultConfig)
    fmt.Println("Version:", string(version))

    // Servir archivos estáticos embebidos:
    http.Handle("/static/", http.FileServer(http.FS(staticFiles)))
    http.ListenAndServe(":8080", nil)
}
```

```bash
# ¿Por qué importa para SysAdmin?
#
# 1. Templates de configuración embebidos en la CLI:
#    tu-cli init → genera archivos desde templates embebidos
#
# 2. Assets de una web UI embebidos en el binario:
#    Prometheus, Grafana, Consul UI → todo en un solo binario
#
# 3. Certificados TLS embebidos (para mTLS interno)
#
# 4. Scripts SQL de migración embebidos:
#    Un solo binario que incluye todas las migraciones
#
# El resultado: UN archivo ejecutable que contiene TODO.
# Copiar un archivo al servidor = deploy completo.
```

### 12.1 Patrones de embed para SysAdmin

```go
// Embeber templates de configuración:
//go:embed templates/*.yaml
var configTemplates embed.FS

// Embeber migraciones SQL:
//go:embed migrations/*.sql
var migrations embed.FS

// Embeber scripts de setup:
//go:embed scripts/setup.sh
var setupScript string

// Embeber man pages o help text:
//go:embed docs/help.txt
var helpText string
```

---

## 13. Makefile — automatizar el flujo de trabajo

Casi todos los proyectos Go usan un `Makefile` para automatizar
tareas comunes. Esto es especialmente importante para proyectos con
múltiples binarios.

```makefile
# Makefile para un proyecto Go

# Variables
APP_NAME    := myapp
VERSION     := $(shell git describe --tags --always --dirty 2>/dev/null || echo "dev")
COMMIT      := $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")
BUILD_TIME  := $(shell date -u '+%Y-%m-%dT%H:%M:%SZ')
LDFLAGS     := -ldflags "-s -w \
    -X main.version=$(VERSION) \
    -X main.commit=$(COMMIT) \
    -X main.buildTime=$(BUILD_TIME)"

# Go settings
GOBIN       := $(shell go env GOPATH)/bin
CGO_ENABLED ?= 0

# Directorios
BIN_DIR     := bin
CMD_DIR     := cmd

## help: mostrar esta ayuda
.PHONY: help
help:
	@grep -E '^##' $(MAKEFILE_LIST) | sed 's/## //' | column -t -s ':'

## build: compilar todos los binarios
.PHONY: build
build:
	@mkdir -p $(BIN_DIR)
	CGO_ENABLED=$(CGO_ENABLED) go build $(LDFLAGS) -trimpath -o $(BIN_DIR)/server ./$(CMD_DIR)/server
	CGO_ENABLED=$(CGO_ENABLED) go build $(LDFLAGS) -trimpath -o $(BIN_DIR)/cli ./$(CMD_DIR)/cli
	CGO_ENABLED=$(CGO_ENABLED) go build $(LDFLAGS) -trimpath -o $(BIN_DIR)/migrate ./$(CMD_DIR)/migrate

## build-all: compilar para múltiples plataformas
.PHONY: build-all
build-all:
	@mkdir -p $(BIN_DIR)
	GOOS=linux   GOARCH=amd64 CGO_ENABLED=0 go build $(LDFLAGS) -trimpath -o $(BIN_DIR)/server-linux-amd64 ./$(CMD_DIR)/server
	GOOS=linux   GOARCH=arm64 CGO_ENABLED=0 go build $(LDFLAGS) -trimpath -o $(BIN_DIR)/server-linux-arm64 ./$(CMD_DIR)/server
	GOOS=darwin  GOARCH=arm64 CGO_ENABLED=0 go build $(LDFLAGS) -trimpath -o $(BIN_DIR)/server-darwin-arm64 ./$(CMD_DIR)/server
	GOOS=windows GOARCH=amd64 CGO_ENABLED=0 go build $(LDFLAGS) -trimpath -o $(BIN_DIR)/server-windows-amd64.exe ./$(CMD_DIR)/server

## test: ejecutar tests
.PHONY: test
test:
	go test -race -cover ./...

## test-verbose: ejecutar tests con output detallado
.PHONY: test-verbose
test-verbose:
	go test -v -race -cover ./...

## lint: ejecutar linters
.PHONY: lint
lint:
	golangci-lint run ./...

## fmt: formatear código
.PHONY: fmt
fmt:
	gofmt -w .
	goimports -w .

## vet: análisis estático
.PHONY: vet
vet:
	go vet ./...

## check: fmt + vet + lint + test (ejecutar antes de commit)
.PHONY: check
check: fmt vet lint test

## tidy: limpiar dependencias
.PHONY: tidy
tidy:
	go mod tidy
	go mod verify

## vuln: verificar vulnerabilidades
.PHONY: vuln
vuln:
	govulncheck ./...

## clean: limpiar artefactos
.PHONY: clean
clean:
	rm -rf $(BIN_DIR)
	go clean -cache -testcache

## docker: construir imagen Docker
.PHONY: docker
docker:
	docker build -t $(APP_NAME):$(VERSION) -f deployments/docker/Dockerfile .

## generate: ejecutar go generate
.PHONY: generate
generate:
	go generate ./...

## install-tools: instalar herramientas de desarrollo
.PHONY: install-tools
install-tools:
	go install github.com/golangci/golangci-lint/cmd/golangci-lint@latest
	go install golang.org/x/tools/cmd/goimports@latest
	go install golang.org/x/vuln/cmd/govulncheck@latest
	go install github.com/go-delve/delve/cmd/dlv@latest
```

```bash
# Uso:
make help         # ver todos los targets
make build        # compilar
make test         # tests
make check        # validación completa (pre-commit)
make docker       # imagen Docker
make build-all    # cross-compilation
make install-tools # instalar herramientas de desarrollo
```

### 13.1 Inyección de versión con ldflags

```go
// cmd/server/main.go
package main

import "fmt"

// Variables inyectadas en compilación por -ldflags:
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
# Sin ldflags:
go build -o server ./cmd/server && ./server
# myapp dev (commit: unknown, built: unknown)

# Con ldflags (Makefile lo hace automáticamente):
go build -ldflags "-X main.version=v1.3.0 -X main.commit=abc123 \
  -X main.buildTime=2025-03-29T12:00:00Z" -o server ./cmd/server
./server
# myapp v1.3.0 (commit: abc123, built: 2025-03-29T12:00:00Z)

# El flag --version es patrón universal en herramientas SysAdmin:
# docker --version
# kubectl version
# terraform version
# prometheus --version
# Todas inyectan la versión con ldflags.
```

---

## 14. Proyecto completo: CLI de SysAdmin

Ejemplo de una herramienta CLI real con la estructura completa:

```
    sysctl-tool/
    ├── cmd/
    │   └── sysctl-tool/
    │       └── main.go
    ├── internal/
    │   ├── check/
    │   │   ├── cpu.go
    │   │   ├── disk.go
    │   │   ├── memory.go
    │   │   └── network.go
    │   ├── config/
    │   │   └── config.go
    │   └── output/
    │       ├── json.go
    │       ├── table.go
    │       └── writer.go
    ├── configs/
    │   └── sysctl-tool.example.yaml
    ├── deployments/
    │   ├── docker/
    │   │   └── Dockerfile
    │   └── systemd/
    │       └── sysctl-tool.service
    ├── scripts/
    │   └── install.sh
    ├── go.mod
    ├── go.sum
    ├── Makefile
    └── README.md
```

```go
// cmd/sysctl-tool/main.go
package main

import (
    "encoding/json"
    "flag"
    "fmt"
    "os"

    "example.com/sysctl-tool/internal/check"
    "example.com/sysctl-tool/internal/config"
)

var (
    version  = "dev"
    flagJSON = flag.Bool("json", false, "output as JSON")
    flagConf = flag.String("config", "", "config file path")
)

func main() {
    flag.Parse()

    args := flag.Args()
    if len(args) == 0 {
        args = []string{"all"}
    }

    cfg, _ := config.Load(*flagConf)
    results := check.RunAll(cfg, args)

    if *flagJSON {
        enc := json.NewEncoder(os.Stdout)
        enc.SetIndent("", "  ")
        enc.Encode(results)
    } else {
        for _, r := range results {
            status := "OK"
            if !r.Passed {
                status = "FAIL"
            }
            fmt.Printf("[%s] %-20s %s\n", status, r.Name, r.Message)
        }
    }
}
```

```go
// internal/check/cpu.go
package check

import "runtime"

type Result struct {
    Name    string `json:"name"`
    Passed  bool   `json:"passed"`
    Message string `json:"message"`
}

func CheckCPU() Result {
    cores := runtime.NumCPU()
    passed := cores >= 2
    msg := fmt.Sprintf("%d cores available", cores)
    if !passed {
        msg += " (minimum 2 recommended)"
    }
    return Result{Name: "cpu_cores", Passed: passed, Message: msg}
}
```

```go
// internal/config/config.go
package config

import (
    "os"

    "gopkg.in/yaml.v3"
)

type Config struct {
    MinCPU      int    `yaml:"min_cpu"`
    MinMemoryMB int    `yaml:"min_memory_mb"`
    MinDiskGB   int    `yaml:"min_disk_gb"`
    CheckNet    bool   `yaml:"check_network"`
    LogLevel    string `yaml:"log_level"`
}

func Load(path string) (*Config, error) {
    cfg := &Config{
        MinCPU: 2, MinMemoryMB: 1024, MinDiskGB: 10,
        CheckNet: true, LogLevel: "info",
    }
    if path == "" {
        return cfg, nil
    }
    data, err := os.ReadFile(path)
    if err != nil {
        return cfg, err
    }
    err = yaml.Unmarshal(data, cfg)
    return cfg, err
}
```

```bash
# Compilar:
make build

# Usar:
./bin/sysctl-tool
# [OK]   cpu_cores            8 cores available
# [OK]   memory               32768 MB available
# [FAIL] disk_space           8 GB available (minimum 10 recommended)
# [OK]   network              internet connectivity OK

./bin/sysctl-tool --json
# {
#   "name": "cpu_cores",
#   "passed": true,
#   "message": "8 cores available"
# }

./bin/sysctl-tool --config /etc/sysctl-tool.yaml cpu disk
```

---

## 15. Antipatrones de estructura

```
    ┌──────────────────────────────────────────────────────────┐
    │ ANTIPATRÓN                   │ POR QUÉ ES MALO          │
    ├──────────────────────────────┼───────────────────────────┤
    │ src/ (al estilo Java)        │ Go no usa src/. El código │
    │                              │ va en la raíz o en cmd/,  │
    │                              │ internal/, pkg/.          │
    ├──────────────────────────────┼───────────────────────────┤
    │ Paquetes por tipo            │ models/, controllers/,    │
    │ (MVC al estilo Ruby/Django)  │ services/ → Go prefiere   │
    │                              │ paquetes por dominio:     │
    │                              │ user/, order/, payment/   │
    ├──────────────────────────────┼───────────────────────────┤
    │ Paquetes util/ o common/     │ Nombres vacíos de          │
    │                              │ significado. Dividir en   │
    │                              │ paquetes con nombres      │
    │                              │ descriptivos.             │
    ├──────────────────────────────┼───────────────────────────┤
    │ Demasiados paquetes          │ Un paquete con 2 archivos │
    │ prematuros                   │ no necesita ser un        │
    │                              │ paquete separado.         │
    ├──────────────────────────────┼───────────────────────────┤
    │ Imports circulares           │ Paquete A importa B y B   │
    │                              │ importa A → error de      │
    │                              │ compilación. Refactorizar │
    │                              │ extrayendo interfaz.      │
    ├──────────────────────────────┼───────────────────────────┤
    │ Toda la lógica en main.go    │ No se puede testear.      │
    │                              │ Separar en paquetes con   │
    │                              │ funciones que reciben     │
    │                              │ args y devuelven valores. │
    └──────────────────────────────┴───────────────────────────┘
```

```go
// ✗ Antipatrón: paquetes por tipo (MVC)
// models/user.go, controllers/user.go, services/user.go
// Tres paquetes para una sola entidad. Imports circulares probables.

// ✓ Patrón Go: paquetes por dominio
// user/user.go, user/store.go, user/handler.go
// Todo lo de "user" junto. Sin imports cruzados innecesarios.
```

```go
// ✗ Antipatrón: paquete "util"
package util
func FormatDate(t time.Time) string { ... }
func ParseConfig(path string) (*Config, error) { ... }
func HashPassword(pw string) string { ... }
// Tres funciones sin relación en un paquete genérico.

// ✓ Patrón Go: nombres descriptivos
// timeutil/format.go  → timeutil.FormatDate()
// config/parse.go     → config.Parse()
// auth/hash.go        → auth.HashPassword()
```

---

## 16. Decisiones de estructura por tamaño del proyecto

```
    ┌─────────────────────────────────────────────────────────────┐
    │ Tamaño del proyecto → estructura recomendada                │
    ├─────────────────┬───────────────────────────────────────────┤
    │ Script/tool     │ main.go + go.mod                         │
    │ (< 500 líneas)  │ Un solo archivo, package main            │
    ├─────────────────┼───────────────────────────────────────────┤
    │ CLI pequeña     │ main.go + internal/ + go.mod             │
    │ (500-2000 LoC)  │ main.go en raíz, lógica en internal/    │
    ├─────────────────┼───────────────────────────────────────────┤
    │ Servicio medio  │ cmd/ + internal/ + go.mod + Makefile     │
    │ (2000-10000 LoC)│ Un binario en cmd/, lógica en internal/  │
    ├─────────────────┼───────────────────────────────────────────┤
    │ Proyecto grande  │ cmd/ + internal/ + pkg/ + deployments/  │
    │ (> 10000 LoC)   │ Múltiples binarios, API pública en pkg/ │
    │                 │ + api/ + configs/ + scripts/ + Makefile  │
    ├─────────────────┼───────────────────────────────────────────┤
    │ Monorepo        │ go.work + múltiples módulos              │
    │                 │ Cada servicio con su go.mod              │
    └─────────────────┴───────────────────────────────────────────┘
```

---

## 17. init() — función de inicialización

Cada paquete puede tener una función `init()` que se ejecuta
automáticamente al importar el paquete, antes de `main()`.

```go
package main

import "fmt"

func init() {
    fmt.Println("1. init() ejecutada")
}

func main() {
    fmt.Println("2. main() ejecutada")
}
// Output:
// 1. init() ejecutada
// 2. main() ejecutada
```

```go
// Orden de ejecución:
//
// 1. Se inicializan las variables globales (en orden de declaración)
// 2. Se ejecutan TODAS las init() del paquete
//    (puede haber múltiples init() en un paquete, incluso en un archivo)
// 3. Se repite para cada paquete importado (en orden de dependencias)
// 4. Finalmente se ejecuta main()
```

```go
// Usos legítimos de init():
// - Registrar drivers de base de datos:
import _ "github.com/lib/pq"  // pq.init() registra "postgres" en sql.Drivers

// - Registrar codecs/formatos:
import _ "image/png"           // registra decodificador PNG

// - Verificaciones en startup:
func init() {
    if os.Getenv("DATABASE_URL") == "" {
        log.Fatal("DATABASE_URL must be set")
    }
}

// ¿Cuándo EVITAR init()?
// - Lógica compleja (difícil de testear)
// - Side effects no obvios
// - Cuando una función explícita (Setup, NewClient) es más clara
```

---

## 18. Errores comunes

| Error | Causa | Solución |
|-------|-------|----------|
| `found packages X and Y` | Dos `package` names en un directorio | Un directorio = un package name |
| `cannot refer to unexported name` | Usar función/variable con minúscula desde otro paquete | Renombrar con mayúscula para exportar |
| `import cycle not allowed` | Paquete A importa B y B importa A | Extraer interfaz en paquete separado |
| `cannot find package` | Import path incorrecto | Verificar module path en go.mod |
| `use of internal package not allowed` | Importar `internal/` desde fuera del módulo | Mover a `pkg/` si debe ser público |
| `main redeclared in this block` | Dos funciones `main()` en el mismo paquete | Solo una `main()` por paquete main |
| `no Go files in directory` | `go build ./empty/` sobre directorio sin .go | Verificar que hay archivos Go |
| `build constraints exclude all Go files` | Build tags incompatibles | Verificar `//go:build` tags |
| `no required module provides package X` | Falta `go mod tidy` | Ejecutar `go mod tidy` |
| Binario se llama como el directorio padre | `go build` sin `-o` | Usar `go build -o nombre` |
| `testdata/` no se compila | Comportamiento esperado (es una feature) | `testdata/` es solo para tests |
| `init()` se ejecuta en orden inesperado | Múltiples `init()` en archivos distintos | Evitar depender del orden de `init()` |

---

## 19. Ejercicios

### Ejercicio 1 — Proyecto mínimo con múltiples archivos

```bash
# 1. Crear un proyecto con tres archivos en package main:
#    - main.go (punto de entrada, llama a run())
#    - app.go (define func run() error)
#    - version.go (define var version = "dev")
#
# 2. main.go debe imprimir la versión y ejecutar run()
# 3. Compilar con go build
# 4. Compilar inyectando versión: go build -ldflags "-X main.version=v1.0.0"
```

**Predicción**: ¿`go build` compila solo main.go o todos los archivos
del directorio?

<details>
<summary>Ver solución</summary>

```go
// main.go
package main

import (
    "fmt"
    "os"
)

func main() {
    fmt.Printf("myapp %s\n", version)
    if err := run(); err != nil {
        fmt.Fprintf(os.Stderr, "error: %v\n", err)
        os.Exit(1)
    }
}
```

```go
// version.go
package main

var version = "dev"
```

```go
// app.go
package main

import "fmt"

func run() error {
    fmt.Println("Running application logic...")
    return nil
}
```

```bash
go build -o myapp
./myapp
# myapp dev
# Running application logic...

go build -ldflags "-X main.version=v1.0.0" -o myapp
./myapp
# myapp v1.0.0
# Running application logic...
```

`go build` compila **todos** los archivos `.go` del directorio que
tengan el mismo `package name`. No hace falta listarlos. Excluye
archivos `_test.go` y archivos con build tags incompatibles.
</details>

---

### Ejercicio 2 — Crear la estructura cmd/internal

```bash
# 1. Crear un proyecto con dos binarios:
#    - cmd/greeter/main.go → imprime "Hello, <name>!"
#    - cmd/counter/main.go → imprime números del 1 al N
#
# 2. Ambos usan un paquete compartido:
#    - internal/util/format.go → func Bold(s string) string
#      (devuelve el string con códigos ANSI de negrita)
#
# 3. Compilar ambos binarios en bin/
```

**Predicción**: ¿Puede un tercer proyecto externo importar
`internal/util`?

<details>
<summary>Ver solución</summary>

```bash
mkdir -p /tmp/ex2/{cmd/greeter,cmd/counter,internal/util,bin}
cd /tmp/ex2
go mod init example.com/ex2
```

```go
// internal/util/format.go
package util

import "fmt"

func Bold(s string) string {
    return fmt.Sprintf("\033[1m%s\033[0m", s)
}
```

```go
// cmd/greeter/main.go
package main

import (
    "fmt"
    "os"

    "example.com/ex2/internal/util"
)

func main() {
    name := "World"
    if len(os.Args) > 1 {
        name = os.Args[1]
    }
    fmt.Println(util.Bold("Hello, " + name + "!"))
}
```

```go
// cmd/counter/main.go
package main

import (
    "fmt"
    "strconv"
    "os"

    "example.com/ex2/internal/util"
)

func main() {
    n := 5
    if len(os.Args) > 1 {
        n, _ = strconv.Atoi(os.Args[1])
    }
    for i := 1; i <= n; i++ {
        fmt.Println(util.Bold(strconv.Itoa(i)))
    }
}
```

```bash
go build -o bin/greeter ./cmd/greeter
go build -o bin/counter ./cmd/counter

./bin/greeter SysAdmin
# Hello, SysAdmin!  (en negrita)

./bin/counter 3
# 1
# 2
# 3
```

**No**, un proyecto externo no puede importar `internal/util`.
El compilador de Go impide que código fuera del módulo que contiene
`internal/` importe cualquier paquete dentro de él. Eso es
exactamente lo que hace `internal/` útil: protección a nivel del
compilador, no solo convención.
</details>

---

### Ejercicio 3 — Visibilidad exportada

```go
// Dado este paquete:
// internal/stats/stats.go
package stats

type serverStatus struct {
    hostname string
    uptime   int
    load     float64
}

func newStatus(host string) *serverStatus {
    return &serverStatus{hostname: host}
}

func GetStatus(host string) string {
    s := newStatus(host)
    return s.hostname
}
```

```bash
# Desde cmd/main.go, ¿cuáles de estas líneas compilan?
#   a) s := stats.GetStatus("web01")
#   b) s := stats.newStatus("web01")
#   c) var s stats.serverStatus
#   d) fmt.Println(s.hostname)
```

**Predicción**: ¿Cuántas de las 4 líneas compilan correctamente?

<details>
<summary>Ver solución</summary>

Solo **a)** compila.

```go
a) s := stats.GetStatus("web01")   // ✓ GetStatus es exportada (mayúscula)
b) s := stats.newStatus("web01")   // ✗ newStatus no exportada (minúscula)
c) var s stats.serverStatus         // ✗ serverStatus no exportado (minúscula)
d) fmt.Println(s.hostname)          // ✗ hostname no exportado (minúscula)
```

Regla en Go: la primera letra determina la visibilidad.
- `GetStatus` → mayúscula → **exportado** (visible desde otros paquetes)
- `newStatus` → minúscula → **no exportado** (solo visible dentro de `stats`)
- `serverStatus` → minúscula → **no exportado**
- `hostname` → minúscula → **no exportado** (campo del struct)

Para exportar: renombrar a `ServerStatus`, `Hostname`, etc.
No hay keywords `public`/`private` en Go.
</details>

---

### Ejercicio 4 — go:embed para templates de configuración

```bash
# 1. Crear un proyecto que:
#    - Embeba un archivo configs/default.yaml
#    - Al ejecutar con --init, escriba el archivo en el directorio actual
#    - Sin --init, lea la configuración del archivo o use el embebido
#
# Estructura:
#   embed-ex/
#   ├── cmd/app/main.go
#   ├── configs/
#   │   └── default.yaml
#   └── go.mod
```

**Predicción**: ¿El archivo `configs/default.yaml` se incluye dentro
del binario compilado o se lee del disco en runtime?

<details>
<summary>Ver solución</summary>

```yaml
# configs/default.yaml
server:
  port: 8080
  host: "0.0.0.0"
log:
  level: "info"
  format: "json"
database:
  host: "localhost"
  port: 5432
  name: "myapp"
```

```go
// cmd/app/main.go
package main

import (
    _ "embed"
    "flag"
    "fmt"
    "os"
)

//go:embed ../../configs/default.yaml
var defaultConfig string

func main() {
    initFlag := flag.Bool("init", false, "write default config to current directory")
    flag.Parse()

    if *initFlag {
        err := os.WriteFile("config.yaml", []byte(defaultConfig), 0644)
        if err != nil {
            fmt.Fprintf(os.Stderr, "error writing config: %v\n", err)
            os.Exit(1)
        }
        fmt.Println("config.yaml created with defaults")
        return
    }

    // Si existe config.yaml local, leerlo
    data, err := os.ReadFile("config.yaml")
    if err != nil {
        // Si no existe, usar el embebido
        data = []byte(defaultConfig)
        fmt.Println("(using embedded default config)")
    }

    fmt.Println(string(data))
}
```

```bash
go build -o app ./cmd/app

./app --init
# config.yaml created with defaults

cat config.yaml
# server:
#   port: 8080
# ...

./app
# (lee config.yaml del directorio actual)
```

El archivo se incluye **dentro del binario** en tiempo de compilación.
`//go:embed` copia el contenido del archivo como una constante string
en el ejecutable. No necesita el archivo en disco para funcionar.
Esto es lo que hace muchas CLIs de infraestructura: `terraform init`,
`helm create`, etc., generan templates desde datos embebidos.

> **Nota**: la ruta en `//go:embed` es relativa al archivo `.go`,
> no al módulo. Para proyectos reales, es mejor poner el embed
> en un paquete junto a los archivos embebidos (ej: `configs/embed.go`).
</details>

---

### Ejercicio 5 — Makefile con cross-compilation

```bash
# Crear un Makefile que:
# 1. Compile el binario para linux/amd64, linux/arm64 y darwin/arm64
# 2. Inyecte versión desde git tags
# 3. Tenga targets: build, build-all, test, clean
# 4. Use CGO_ENABLED=0 para binarios estáticos
```

**Predicción**: ¿Es necesario tener gcc instalado para cross-compilar
Go a ARM64?

<details>
<summary>Ver solución</summary>

```makefile
APP_NAME := mytool
VERSION  := $(shell git describe --tags --always --dirty 2>/dev/null || echo "dev")
LDFLAGS  := -ldflags "-s -w -X main.version=$(VERSION)"
BIN_DIR  := bin

.PHONY: build build-all test clean

build:
	@mkdir -p $(BIN_DIR)
	CGO_ENABLED=0 go build $(LDFLAGS) -trimpath -o $(BIN_DIR)/$(APP_NAME) .

build-all:
	@mkdir -p $(BIN_DIR)
	GOOS=linux   GOARCH=amd64 CGO_ENABLED=0 go build $(LDFLAGS) -trimpath -o $(BIN_DIR)/$(APP_NAME)-linux-amd64 .
	GOOS=linux   GOARCH=arm64 CGO_ENABLED=0 go build $(LDFLAGS) -trimpath -o $(BIN_DIR)/$(APP_NAME)-linux-arm64 .
	GOOS=darwin  GOARCH=arm64 CGO_ENABLED=0 go build $(LDFLAGS) -trimpath -o $(BIN_DIR)/$(APP_NAME)-darwin-arm64 .

test:
	go test -race -cover ./...

clean:
	rm -rf $(BIN_DIR)
```

```bash
make build-all
ls -lh bin/
# mytool-linux-amd64   1.8M
# mytool-linux-arm64   1.7M
# mytool-darwin-arm64  1.8M

file bin/mytool-linux-arm64
# ELF 64-bit LSB executable, ARM aarch64, statically linked
```

**No**, Go **no necesita gcc** para cross-compilar con `CGO_ENABLED=0`.
El compilador Go genera código nativo directamente para cualquier
target soportado. Solo si `CGO_ENABLED=1` (que usa código C) necesitarías
un cross-compiler C (como `aarch64-linux-gnu-gcc`).

Esto es una ventaja enorme de Go sobre C/Rust para SysAdmin: desde
tu laptop x86_64 puedes compilar binarios para Raspberry Pi (ARM64),
servidores cloud ARM (Graviton), o macOS, sin instalar nada adicional.
</details>

---

### Ejercicio 6 — Import circular y cómo resolverlo

```go
// Dado este import circular:
//   internal/user/user.go   → importa internal/order
//   internal/order/order.go → importa internal/user
//
// Error: import cycle not allowed

// user/user.go
package user

import "example.com/app/internal/order"

type User struct {
    ID   int
    Name string
}

func (u *User) Orders() []order.Order {
    return order.FindByUser(u.ID)
}
```

```go
// order/order.go
package order

import "example.com/app/internal/user"

type Order struct {
    ID     int
    UserID int
    Total  float64
}

func FindByUser(userID int) []Order { ... }

func (o *Order) Owner() *user.User {
    return user.FindByID(o.UserID)
}
```

```bash
# ¿Cómo romper este ciclo?
```

**Predicción**: ¿Go permite import cycles? ¿Cuál es la solución canónica?

<details>
<summary>Ver solución</summary>

**No**, Go prohíbe estrictamente import cycles. Es un error de compilación.

Solución canónica: **extraer una interfaz o un tipo compartido** en un
tercer paquete.

```go
// Opción 1: interfaz en un paquete separado
// internal/domain/types.go
package domain

type User struct {
    ID   int
    Name string
}

type Order struct {
    ID     int
    UserID int
    Total  float64
}
```

```go
// internal/user/user.go
package user

import "example.com/app/internal/domain"

func FindByID(id int) *domain.User { ... }
```

```go
// internal/order/order.go
package order

import "example.com/app/internal/domain"

func FindByUser(userID int) []domain.Order { ... }
```

```go
// Opción 2: interfaz (más Go-idiomático)
// internal/user/user.go
package user

type OrderFinder interface {
    FindByUser(userID int) []Order
}

type User struct {
    ID     int
    Name   string
    orders OrderFinder  // inyectada, no importada
}
```

La regla en Go: si dos paquetes se necesitan mutuamente, hay un
problema de diseño. Las soluciones son:
1. Unir los paquetes en uno solo
2. Extraer tipos compartidos a un tercer paquete
3. Usar interfaces para invertir la dependencia
</details>

---

### Ejercicio 7 — testdata y archivos de test

```bash
# 1. Crear un paquete internal/parser/ que:
#    - Lea archivos JSON y devuelva datos parseados
#    - Use testdata/ para fixtures de test
#
# Estructura:
#   internal/parser/
#   ├── parser.go
#   ├── parser_test.go
#   └── testdata/
#       ├── valid.json
#       └── invalid.json
```

**Predicción**: ¿El directorio `testdata/` se incluye en el binario
compilado con `go build`?

<details>
<summary>Ver solución</summary>

```go
// internal/parser/parser.go
package parser

import (
    "encoding/json"
    "os"
)

type Server struct {
    Host string `json:"host"`
    Port int    `json:"port"`
}

func ParseServers(path string) ([]Server, error) {
    data, err := os.ReadFile(path)
    if err != nil {
        return nil, err
    }
    var servers []Server
    err = json.Unmarshal(data, &servers)
    return servers, err
}
```

```json
// internal/parser/testdata/valid.json
[
  {"host": "web01", "port": 8080},
  {"host": "web02", "port": 8081}
]
```

```json
// internal/parser/testdata/invalid.json
not valid json {{{
```

```go
// internal/parser/parser_test.go
package parser

import "testing"

func TestParseServers_Valid(t *testing.T) {
    servers, err := ParseServers("testdata/valid.json")
    if err != nil {
        t.Fatalf("unexpected error: %v", err)
    }
    if len(servers) != 2 {
        t.Errorf("got %d servers, want 2", len(servers))
    }
    if servers[0].Host != "web01" {
        t.Errorf("got host %q, want %q", servers[0].Host, "web01")
    }
}

func TestParseServers_Invalid(t *testing.T) {
    _, err := ParseServers("testdata/invalid.json")
    if err == nil {
        t.Fatal("expected error for invalid JSON, got nil")
    }
}
```

```bash
go test -v ./internal/parser/
# === RUN   TestParseServers_Valid
# --- PASS: TestParseServers_Valid (0.00s)
# === RUN   TestParseServers_Invalid
# --- PASS: TestParseServers_Invalid (0.00s)
# PASS
```

**No**, `testdata/` **no se incluye** en el binario. Go ignora
explícitamente los directorios llamados `testdata`, `vendor` y
directorios que empiezan con `.` o `_` al compilar. `testdata/` solo
es accesible durante `go test`, con rutas relativas al paquete.
</details>

---

### Ejercicio 8 — go generate para generar código

```go
// 1. Crear un tipo enum con iota:
//    type LogLevel int
//    const (Info LogLevel = iota; Warning; Error; Critical)
//
// 2. Instalar stringer: go install golang.org/x/tools/cmd/stringer@latest
// 3. Añadir //go:generate stringer -type=LogLevel
// 4. Ejecutar go generate ./...
// 5. Examinar el archivo generado
```

**Predicción**: ¿Qué archivo generará `stringer`? ¿Qué método añade?

<details>
<summary>Ver solución</summary>

```go
// internal/log/level.go
package log

//go:generate stringer -type=LogLevel
type LogLevel int

const (
    Info     LogLevel = iota  // 0
    Warning                   // 1
    Error                     // 2
    Critical                  // 3
)
```

```bash
go install golang.org/x/tools/cmd/stringer@latest
go generate ./internal/log/...

ls internal/log/
# level.go
# loglevel_string.go  ← archivo generado
```

```go
// loglevel_string.go (generado automáticamente)
// Code generated by "stringer -type=LogLevel"; DO NOT EDIT.
package log

import "strconv"

func _() {
    var x [1]struct{}
    _ = x[Info-0]
    _ = x[Warning-1]
    _ = x[Error-2]
    _ = x[Critical-3]
}

const _LogLevel_name = "InfoWarningErrorCritical"
var _LogLevel_index = [...]uint8{0, 4, 11, 16, 24}

func (i LogLevel) String() string {
    if i < 0 || i >= LogLevel(len(_LogLevel_index)-1) {
        return "LogLevel(" + strconv.FormatInt(int64(i), 10) + ")"
    }
    return _LogLevel_name[_LogLevel_index[i]:_LogLevel_index[i+1]]
}
```

`stringer` genera `loglevel_string.go` con un método `String()`
para el tipo `LogLevel`. Esto hace que `fmt.Println(Info)` imprima
`"Info"` en vez de `"0"`. El archivo generado se commitea al repositorio
para que `go build` funcione sin necesitar `stringer` instalado.
</details>

---

### Ejercicio 9 — Refactorizar un monolito en paquetes

```go
// Este main.go monolítico tiene toda la lógica junta.
// Refactorizarlo en cmd/ + internal/.

package main

import (
    "encoding/json"
    "flag"
    "fmt"
    "net/http"
    "os"
    "runtime"
    "time"
)

var version = "dev"

type HealthResponse struct {
    Status  string `json:"status"`
    Version string `json:"version"`
    Uptime  string `json:"uptime"`
}

var startTime = time.Now()

func healthHandler(w http.ResponseWriter, r *http.Request) {
    resp := HealthResponse{
        Status:  "ok",
        Version: version,
        Uptime:  time.Since(startTime).String(),
    }
    w.Header().Set("Content-Type", "application/json")
    json.NewEncoder(w).Encode(resp)
}

func main() {
    port := flag.Int("port", 8080, "server port")
    flag.Parse()
    addr := fmt.Sprintf(":%d", *port)
    http.HandleFunc("/health", healthHandler)
    fmt.Printf("Server %s (%s/%s) on %s\n", version, runtime.GOOS, runtime.GOARCH, addr)
    if err := http.ListenAndServe(addr, nil); err != nil {
        fmt.Fprintf(os.Stderr, "error: %v\n", err)
        os.Exit(1)
    }
}
```

```bash
# Estructura objetivo:
# cmd/server/main.go      ← thin main
# internal/health/handler.go ← handler + response type
# internal/server/server.go  ← lógica de setup del servidor
```

**Predicción**: ¿Cuántas líneas tendrá `cmd/server/main.go` después
de refactorizar?

<details>
<summary>Ver solución</summary>

```go
// cmd/server/main.go (~15 líneas)
package main

import (
    "flag"
    "log"

    "example.com/app/internal/server"
)

var version = "dev"

func main() {
    port := flag.Int("port", 8080, "server port")
    flag.Parse()
    log.Fatal(server.Run(version, *port))
}
```

```go
// internal/health/handler.go
package health

import (
    "encoding/json"
    "net/http"
    "time"
)

type Response struct {
    Status  string `json:"status"`
    Version string `json:"version"`
    Uptime  string `json:"uptime"`
}

var startTime = time.Now()

func Handler(version string) http.HandlerFunc {
    return func(w http.ResponseWriter, r *http.Request) {
        resp := Response{
            Status:  "ok",
            Version: version,
            Uptime:  time.Since(startTime).String(),
        }
        w.Header().Set("Content-Type", "application/json")
        json.NewEncoder(w).Encode(resp)
    }
}
```

```go
// internal/server/server.go
package server

import (
    "fmt"
    "net/http"
    "runtime"

    "example.com/app/internal/health"
)

func Run(version string, port int) error {
    http.HandleFunc("/health", health.Handler(version))
    addr := fmt.Sprintf(":%d", port)
    fmt.Printf("Server %s (%s/%s) on %s\n", version, runtime.GOOS, runtime.GOARCH, addr)
    return http.ListenAndServe(addr, nil)
}
```

`cmd/server/main.go` tiene **~15 líneas** (thin main). Toda la lógica
está en `internal/` donde es testeable:
- `health.Handler` puede testearse con `httptest.NewRecorder`
- `server.Run` puede testearse con un puerto efímero
- `main()` solo conecta componentes: no necesita tests
</details>

---

### Ejercicio 10 — Analizar un proyecto real

```bash
# Examinar la estructura de un proyecto Go real del ecosistema SysAdmin.
# Elegir UNO:
#   github.com/prometheus/node_exporter
#   github.com/traefik/traefik
#   github.com/grafana/loki

# 1. Clonar el repositorio (shallow):
git clone --depth 1 https://github.com/prometheus/node_exporter /tmp/node_exporter

# 2. Analizar la estructura:
#    - ¿Usa cmd/?
#    - ¿Usa internal/?
#    - ¿Usa pkg/?
#    - ¿Tiene Makefile?
#    - ¿Cuántos binarios produce?
#    - ¿Usa go:generate?
#    - ¿Tiene deployments/ o configs/?

# 3. Leer el Makefile: ¿qué targets tiene?
# 4. Leer cmd/*/main.go: ¿es un thin main?
```

**Predicción**: ¿Prometheus node_exporter usa `internal/` o `pkg/`?

<details>
<summary>Ver solución</summary>

```bash
git clone --depth 1 https://github.com/prometheus/node_exporter /tmp/node_exporter
cd /tmp/node_exporter

# Estructura de primer nivel:
ls
# CHANGELOG.md  Dockerfile  MAINTAINERS.md  Makefile  README.md
# collector/    cmd/        docs/           e2e/      go.mod  go.sum
# https/

# ¿Usa cmd/? SÍ
ls cmd/
# node_exporter/

# ¿Usa internal/? NO (pero collector/ es de facto internal)
# ¿Usa pkg/? NO

# ¿Cuántos binarios? 1 (node_exporter)
head -20 cmd/node_exporter/main.go
# package main
# ... thin main que configura collectors y arranca HTTP server

# ¿Makefile?
head -5 Makefile
# Usa promu (herramienta de build de Prometheus)

# ¿go:generate?
grep -r "go:generate" --include="*.go" | head -5
```

node_exporter tiene una estructura **simple**:
- Un solo binario en `cmd/node_exporter/`
- Sin `internal/` ni `pkg/` explícitos
- `collector/` contiene todos los collectors (CPU, disk, memory...)
- Cada collector es un archivo separado (cpu_linux.go, disk_linux.go...)
- Usa build tags para código específico de OS (`_linux.go`, `_darwin.go`)
- El Makefile usa `promu` (herramienta de Prometheus) en vez de `go build` directo

Los proyectos reales no siempre siguen el layout canónico al 100%.
Lo importante es la consistencia interna y que la estructura tenga
sentido para el proyecto.
</details>
