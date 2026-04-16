# T02 — Go Modules

## 1. Qué son los Go Modules

Un **módulo** es la unidad de distribución y versionado de código en Go.
Es un conjunto de paquetes Go que se versionan juntos, definidos por un
archivo `go.mod` en la raíz del proyecto.

```
    Antes de modules (pre-Go 1.11)         Con modules (Go 1.11+)
    ┌───────────────────────────┐          ┌───────────────────────────┐
    │ Todo en $GOPATH/src/      │          │ Proyecto en cualquier     │
    │                           │          │ directorio                │
    │ Sin versionado explícito  │          │                           │
    │ go get = última versión   │          │ go.mod declara versiones  │
    │ Dependencias globales     │          │ go.sum verifica integridad│
    │ Sin reproducibilidad      │          │ Builds reproducibles      │
    └───────────────────────────┘          └───────────────────────────┘
```

Desde Go 1.16, los modules son el **único** modo soportado. Si encuentras
tutoriales que mencionan `GOPATH mode` o `GO111MODULE=on`, son obsoletos.

### 1.1 Anatomía de un módulo

```
    myproject/
    ├── go.mod          ← identidad del módulo + dependencias
    ├── go.sum          ← checksums de integridad
    ├── main.go         ← paquete main (punto de entrada)
    ├── internal/       ← paquetes privados
    │   └── config/
    │       └── config.go
    ├── pkg/            ← paquetes públicos reutilizables
    │   └── api/
    │       └── client.go
    └── vendor/         ← (opcional) copia local de dependencias
        └── github.com/
            └── ...
```

### 1.2 Módulo vs paquete

```
    ┌─────────────────────────────────────────────────┐
    │ Módulo (module)                                  │
    │ = unidad de versionado y distribución            │
    │ = definido por go.mod                            │
    │ = puede contener múltiples paquetes              │
    │                                                  │
    │   ┌────────────────┐  ┌────────────────┐        │
    │   │ Paquete (pkg)  │  │ Paquete (pkg)  │        │
    │   │ = un directorio│  │ = un directorio│        │
    │   │ = archivos .go │  │ = archivos .go │        │
    │   │ con mismo      │  │ con mismo      │        │
    │   │ package name   │  │ package name   │        │
    │   └────────────────┘  └────────────────┘        │
    └─────────────────────────────────────────────────┘

    Ejemplo:
    github.com/prometheus/client_golang          ← módulo
    github.com/prometheus/client_golang/prometheus  ← paquete
    github.com/prometheus/client_golang/api         ← paquete
```

---

## 2. go mod init — crear un módulo

```bash
# Sintaxis:
go mod init <module-path>

# El module path es la ruta de importación del módulo.
# Para proyectos públicos, usa el repositorio:
go mod init github.com/usuario/proyecto

# Para proyectos privados o locales:
go mod init empresa.com/equipo/herramienta

# Para ejercicios y pruebas:
go mod init example.com/myapp
```

```bash
# Ejemplo completo:
mkdir -p /tmp/syscheck && cd /tmp/syscheck
go mod init github.com/user/syscheck

# Esto crea go.mod:
cat go.mod
# module github.com/user/syscheck
#
# go 1.24.1
```

### 2.1 Convenciones de module path

| Tipo de proyecto | Module path | Ejemplo |
|------------------|-------------|---------|
| Público en GitHub | `github.com/user/repo` | `github.com/hashicorp/terraform` |
| Público en GitLab | `gitlab.com/user/repo` | `gitlab.com/org/tool` |
| Empresa privada | `empresa.com/equipo/repo` | `internal.corp.com/infra/deployer` |
| Ejercicio/local | `example.com/nombre` | `example.com/hello` |
| Stdlib de Go | `std` | (solo la biblioteca estándar) |

```bash
# El module path importa porque:
# 1. Es la ruta que otros usan para importar tu código
#    import "github.com/user/syscheck/pkg/monitor"
#
# 2. Go lo usa para descargar el módulo (go get)
#    go get github.com/user/syscheck@v1.2.0
#
# 3. Define el namespace de tus paquetes internos
#    Los imports internos usan el module path como prefijo
```

---

## 3. El archivo go.mod

`go.mod` es el manifiesto del módulo. Declara la identidad del módulo,
la versión mínima de Go requerida, y todas las dependencias directas.

### 3.1 Estructura de go.mod

```go
// go.mod
module github.com/user/myapp

go 1.24.1

require (
    github.com/spf13/cobra v1.8.1
    github.com/prometheus/client_golang v1.20.5
    gopkg.in/yaml.v3 v3.0.1
)

require (
    github.com/inconshreveable/mousetrap v1.1.0 // indirect
    github.com/spf13/pflag v1.0.5 // indirect
    google.golang.org/protobuf v1.35.2 // indirect
)
```

```bash
# Desglose de cada directiva:

# module — ruta del módulo (identidad)
module github.com/user/myapp

# go — versión mínima de Go requerida
# No es la versión con la que se compiló, sino la mínima para compilar
go 1.24.1

# require — dependencias
# Formato: <module-path> <version>
require github.com/spf13/cobra v1.8.1

# // indirect — dependencia no importada directamente por tu código
# Es una dependencia de una dependencia (transitiva)
require github.com/spf13/pflag v1.0.5 // indirect
```

### 3.2 Directivas de go.mod

| Directiva | Uso | Ejemplo |
|-----------|-----|---------|
| `module` | Identidad del módulo | `module github.com/user/app` |
| `go` | Versión mínima de Go | `go 1.24.1` |
| `require` | Dependencia con versión | `require pkg v1.2.3` |
| `replace` | Reemplazar un módulo por otro | `replace pkg => ../local-pkg` |
| `exclude` | Excluir una versión específica | `exclude pkg v1.2.0` |
| `retract` | Marcar versiones propias como retiradas | `retract v1.0.0` |
| `toolchain` | Versión del toolchain de Go | `toolchain go1.24.1` |

### 3.3 La directiva replace

`replace` es extremadamente útil para desarrollo local y en entornos
corporativos:

```go
// go.mod

// 1. Desarrollo local: usar un módulo local en vez del remoto
// (mientras desarrollas un fix en una dependencia)
replace github.com/some/library => ../my-local-fork

// 2. Fork corporativo: redirigir a un fork interno
replace github.com/original/pkg => github.com/empresa/pkg-fork v1.2.3

// 3. Versión específica no publicada:
replace github.com/some/pkg => github.com/some/pkg v0.0.0-20250301120000-abc123def456
```

```bash
# Caso SysAdmin típico:
# Necesitas parchear una dependencia que tiene un bug,
# pero el mantenedor no ha publicado un fix.

# 1. Clonar la dependencia:
git clone https://github.com/some/library ../library-fix
cd ../library-fix
# Aplicar el fix...

# 2. En tu proyecto, agregar replace:
# replace github.com/some/library => ../library-fix

# 3. go mod tidy actualiza el go.sum

# 4. Cuando el fix se publique upstream, eliminar el replace
```

### 3.4 La directiva toolchain

Desde Go 1.21, `go.mod` puede especificar la versión exacta del toolchain:

```go
// go.mod
module github.com/user/app

go 1.22.0
toolchain go1.24.1
```

```bash
# go 1.22.0     → versión mínima para compilar el código
# toolchain go1.24.1 → versión preferida para compilar
#
# Si tienes Go 1.23.0 instalado:
# - go 1.22.0 se satisface (1.23.0 >= 1.22.0)
# - toolchain go1.24.1 → Go intentará descargar 1.24.1 automáticamente
#
# Para desactivar la descarga automática de toolchain:
# GOTOOLCHAIN=local go build
```

---

## 4. go.sum — integridad y seguridad

`go.sum` contiene los checksums criptográficos (SHA-256) de cada versión
de cada módulo que el proyecto usa (directa e indirectamente).

```bash
cat go.sum
# github.com/spf13/cobra v1.8.1 h1:e5/vxKd/rZsfSJMUX1agtjeTDf+qv1/JdBF8lex5IeA=
# github.com/spf13/cobra v1.8.1/go.mod h1:FjbLRSCAPaFV...=
# ...

# Cada línea tiene:
# <module> <version> h1:<hash-base64>
# <module> <version>/go.mod h1:<hash-base64>
#
# Hay DOS entradas por módulo:
# 1. Hash del contenido completo del módulo (.zip)
# 2. Hash solo del go.mod del módulo
```

### 4.1 ¿Para qué sirve go.sum?

```
    ┌──────────────────────────────────────────────────────┐
    │ Ataque supply chain:                                  │
    │                                                       │
    │ Un atacante compromete un repositorio y modifica      │
    │ el código de una versión ya publicada (v1.2.3).       │
    │                                                       │
    │ Sin go.sum:                                           │
    │   go get descarga el código modificado. El build      │
    │   usa código malicioso sin que lo sepas.              │
    │                                                       │
    │ Con go.sum:                                           │
    │   go detecta que el hash no coincide con el           │
    │   registrado en go.sum y RECHAZA la descarga.         │
    │   Error: "checksum mismatch"                          │
    │                                                       │
    │ go.sum + sum.golang.org = verificación de integridad  │
    └──────────────────────────────────────────────────────┘
```

```bash
# Go verifica checksums contra TRES fuentes:
#
# 1. go.sum local     — checksums de tu proyecto
# 2. Cache local      — ~/go/pkg/mod/cache/download/sumdb/
# 3. sum.golang.org   — base de datos global de checksums (transparency log)
#
# Si un módulo cambia después de ser publicado, la verificación falla.
# Esto protege contra:
# - Repositorios comprometidos
# - Proxies maliciosos
# - Ataques man-in-the-middle
```

### 4.2 go.sum en el repositorio

```bash
# ¿Se debe commitear go.sum al repositorio git?
# SÍ, siempre.
#
# go.mod  → COMMIT (manifiesto de dependencias)
# go.sum  → COMMIT (verificación de integridad)
# vendor/ → DEPENDE (ver sección de vendor)
#
# .gitignore NO debe incluir go.sum
```

### 4.3 GONOSUMCHECK y GONOSUMDB

```bash
# Para módulos privados que no están en sum.golang.org:
go env -w GONOSUMCHECK=github.com/mi-empresa/*
go env -w GONOSUMDB=github.com/mi-empresa/*
go env -w GOPRIVATE=github.com/mi-empresa/*

# GOPRIVATE establece ambos (GONOSUMCHECK + GONOSUMDB)
# Úsalo para repositorios corporativos internos.
```

---

## 5. Versionado semántico

Go Modules usa **Semantic Versioning (SemVer)** estrictamente:

```
    v MAJOR . MINOR . PATCH
    │   │       │       │
    │   │       │       └─ Bug fixes, no cambia API
    │   │       └───────── Nuevas features, compatible
    │   └───────────────── Cambios incompatibles (breaking)
    └───────────────────── Prefijo obligatorio en Go

    Ejemplos:
    v1.0.0  → primera versión estable
    v1.2.3  → compatible con v1.0.0
    v2.0.0  → rompe compatibilidad con v1.x
    v0.3.1  → pre-estable (no hay garantías de compatibilidad)
```

### 5.1 Regla de compatibilidad de imports

```bash
# En Go, v2+ de un módulo es un MÓDULO DIFERENTE con una ruta diferente.
# Esto es único de Go y sorprende a quienes vienen de otros lenguajes.

# v0.x y v1.x:
import "github.com/user/pkg"

# v2.x:
import "github.com/user/pkg/v2"

# v3.x:
import "github.com/user/pkg/v3"

# Esto permite que un programa use v1 y v2 simultáneamente
# (durante una migración gradual, por ejemplo).
```

```go
// go.mod para un módulo v2+:
module github.com/user/pkg/v2

go 1.24.1
```

### 5.2 Pre-releases y pseudo-versiones

```bash
# Pre-release (SemVer estándar):
go get github.com/user/pkg@v1.3.0-beta.1
go get github.com/user/pkg@v2.0.0-rc.1

# Pseudo-versión — para commits sin tag:
# Formato: vX.Y.Z-YYYYMMDDHHMMSS-<hash12>
go get github.com/user/pkg@v0.0.0-20250315120000-abc123def456

# Go genera pseudo-versiones automáticamente cuando haces:
go get github.com/user/pkg@abc123def456  # commit hash
go get github.com/user/pkg@main          # nombre de branch
```

### 5.3 Minimum Version Selection (MVS)

Go usa un algoritmo único para resolver dependencias: **MVS** (Minimum
Version Selection). A diferencia de npm, pip o cargo, Go elige la
**versión mínima** que satisface todos los requisitos, no la máxima.

```
    Ejemplo:
    Tu app requiere pkg >= v1.2.0
    Tu dependencia A requiere pkg >= v1.3.0
    Tu dependencia B requiere pkg >= v1.1.0

    npm/pip/cargo: instala v1.5.0 (última compatible)
    Go MVS:        instala v1.3.0 (mínima que satisface todo)

    ¿Por qué?
    - Builds más reproducibles (menos variación)
    - Menos riesgo de regressions por nuevas versiones
    - El resultado es determinista sin lockfile separado
      (go.mod + go.sum SON el lockfile)
```

---

## 6. go get — gestionar dependencias

`go get` añade, actualiza o elimina dependencias en `go.mod`.

```bash
# Añadir una dependencia (última versión):
go get github.com/spf13/cobra@latest

# Añadir una versión específica:
go get github.com/spf13/cobra@v1.8.1

# Añadir una versión mínima:
go get github.com/spf13/cobra@>=v1.7.0

# Actualizar a la última versión minor/patch:
go get -u github.com/spf13/cobra

# Actualizar SOLO patches (seguridad):
go get -u=patch github.com/spf13/cobra

# Actualizar TODAS las dependencias:
go get -u ./...

# Actualizar solo patches de TODO:
go get -u=patch ./...

# Eliminar una dependencia:
go get github.com/spf13/cobra@none
```

### 6.1 go get vs go install (recordatorio)

```bash
# go get → MODIFICA go.mod (gestión de dependencias)
# Úsalo dentro de un proyecto para agregar/actualizar/eliminar deps
cd myproject
go get github.com/some/library@v1.2.3

# go install → INSTALA BINARIOS en $GOPATH/bin
# Úsalo para instalar herramientas ejecutables
go install github.com/golangci/golangci-lint/cmd/golangci-lint@latest

# ERROR COMÚN:
# go get github.com/some/tool@latest   ← NO instala binarios desde Go 1.17
# go install github.com/some/tool@latest ← SÍ instala binarios
```

---

## 7. go mod tidy — limpiar dependencias

`go mod tidy` es el comando más importante del día a día. Añade
dependencias que faltan y elimina las que ya no se usan.

```bash
go mod tidy

# Lo que hace:
# 1. Escanea todos los archivos .go del módulo
# 2. Encuentra todos los import que se usan
# 3. Añade a go.mod las dependencias que faltan
# 4. Elimina de go.mod las que ya no se importan
# 5. Actualiza go.sum con los checksums correctos
# 6. Descarga los módulos que falten
```

```bash
# Workflow típico:
# 1. Escribir código, añadir imports nuevos
# 2. go mod tidy  ← actualiza go.mod y go.sum
# 3. go build     ← compila
# 4. git add go.mod go.sum
# 5. git commit

# En CI/CD, verificar que go.mod está limpio:
go mod tidy
git diff --exit-code go.mod go.sum
# Si hay diferencias, alguien olvidó ejecutar go mod tidy
```

### 7.1 go mod tidy en CI

```bash
# Script de CI para verificar dependencias:
#!/bin/bash
set -euo pipefail

echo "Checking go.mod tidiness..."
cp go.mod go.mod.bak
cp go.sum go.sum.bak

go mod tidy

if ! diff -q go.mod go.mod.bak > /dev/null 2>&1 || \
   ! diff -q go.sum go.sum.bak > /dev/null 2>&1; then
    echo "ERROR: go.mod or go.sum is not tidy"
    echo "Run 'go mod tidy' and commit the changes"
    diff go.mod.bak go.mod || true
    exit 1
fi

echo "go.mod is tidy ✓"
rm -f go.mod.bak go.sum.bak
```

---

## 8. go mod vendor — vendoring de dependencias

`go mod vendor` copia todas las dependencias a un directorio `vendor/`
dentro del proyecto. Esto crea un snapshot local de todas las dependencias.

```bash
go mod vendor

# Crea:
# vendor/
# ├── github.com/
# │   └── spf13/
# │       └── cobra/
# │           ├── command.go
# │           └── ...
# ├── modules.txt    ← manifiesto de qué hay en vendor
# └── ...
```

### 8.1 ¿Cuándo usar vendor?

```
    ┌─────────────────────────────────────────────────────────────┐
    │ ¿Vendorear o no vendorear?                                  │
    ├─────────────────────────────────┬───────────────────────────┤
    │ SÍ vendor                       │ NO vendor                 │
    ├─────────────────────────────────┼───────────────────────────┤
    │ Entornos sin acceso a internet  │ Proyectos pequeños        │
    │ CI/CD hermético (airgapped)     │ Desarrollo rápido         │
    │ Requisito de auditoría          │ Repositorios ya grandes   │
    │ Builds reproducibles estrictos  │ Proxy de módulos confiable│
    │ Protección contra left-pad      │ Open source (convención)  │
    │ Regulación financiera/gobierno  │                           │
    └─────────────────────────────────┴───────────────────────────┘
```

```bash
# Compilar usando vendor (sin descargar nada):
go build -mod=vendor ./...

# En Go 1.22+, si existe vendor/, se usa automáticamente.
# Para forzar el uso del vendor:
go build -mod=vendor

# Para forzar NO usar vendor:
go build -mod=mod

# Verificar que vendor está sincronizado:
go mod verify
# all modules verified
```

### 8.2 vendor en SysAdmin/DevOps

```bash
# Escenario: despliegar en un servidor sin acceso a internet
#
# 1. En tu máquina de desarrollo (con internet):
go mod vendor
git add vendor/
git commit -m "vendor dependencies"
git push

# 2. En el servidor (sin internet):
git pull
go build -mod=vendor -o myapp
# Compila sin necesitar internet

# Escenario: entorno regulado (finanzas, gobierno)
# El código de terceros debe ser auditado.
# Con vendor, todo el código está en el repo y pasa por code review.
```

### 8.3 vendor en .gitignore

```bash
# Política del equipo:
#
# Opción A: commitear vendor/ (seguridad máxima)
# No agregar vendor/ a .gitignore
# PRO: builds herméticos, auditoría de código
# CON: repositorio grande, diffs ruidosos en updates

# Opción B: NO commitear vendor/ (agilidad)
# Agregar vendor/ a .gitignore
# En CI: go mod download o go mod vendor antes de build
# PRO: repo limpio, diffs limpios
# CON: depende de internet y proxy para builds
```

---

## 9. go mod download, verify, graph, why

### 9.1 go mod download

```bash
# Descargar todas las dependencias a la cache local
# (sin compilar nada):
go mod download

# Útil en Dockerfiles para aprovechar cache de capas:
# El truco: copiar go.mod y go.sum ANTES del código fuente
```

```dockerfile
# Dockerfile optimizado para cache de dependencias
FROM golang:1.24-alpine AS builder

WORKDIR /app

# Primero: solo copiar archivos de dependencias
COPY go.mod go.sum ./
RUN go mod download

# Después: copiar el código fuente
COPY . .
RUN CGO_ENABLED=0 go build -ldflags "-s -w" -o /app/server ./cmd/server

# Imagen final mínima
FROM scratch
COPY --from=builder /app/server /server
ENTRYPOINT ["/server"]
```

```bash
# ¿Por qué separar go.mod del código fuente en Docker?
#
# Docker cachea capas. Si go.mod no cambió, la capa de
# "go mod download" se reutiliza aunque el código cambie.
#
# Sin la separación:
# Cambiar main.go → re-descarga TODAS las dependencias (~30s-2min)
#
# Con la separación:
# Cambiar main.go → reutiliza cache de deps (~0s) + solo recompila
#
# Esto reduce builds de producción de minutos a segundos.
```

### 9.2 go mod verify

```bash
# Verificar que los módulos en la cache no han sido modificados:
go mod verify
# all modules verified

# Si alguien modificó un módulo en la cache:
go mod verify
# github.com/some/pkg v1.2.3: dir has been modified (...)
# Solución: go clean -modcache && go mod download
```

### 9.3 go mod graph

```bash
# Mostrar el grafo completo de dependencias:
go mod graph
# github.com/user/app github.com/spf13/cobra@v1.8.1
# github.com/user/app gopkg.in/yaml.v3@v3.0.1
# github.com/spf13/cobra@v1.8.1 github.com/spf13/pflag@v1.0.5
# ...

# Visualizar como árbol (con herramientas externas):
go mod graph | sed 's/@[^ ]*//g' | sort -u

# ¿Cuántas dependencias transitivas tengo?
go mod graph | wc -l
```

### 9.4 go mod why

```bash
# ¿Por qué tengo esta dependencia?
go mod why github.com/spf13/pflag
# # github.com/spf13/pflag
# github.com/user/app
# github.com/spf13/cobra
# github.com/spf13/pflag

# Lectura: tu app importa cobra, cobra importa pflag.
# pflag es una dependencia transitiva (indirect).

# Útil para entender por qué go.mod incluye un módulo
# que no importas directamente.
```

---

## 10. go mod edit — edición programática

```bash
# Modificar go.mod desde la línea de comandos (sin editar el archivo):

# Añadir un require:
go mod edit -require github.com/spf13/cobra@v1.8.1

# Eliminar un require:
go mod edit -droprequire github.com/spf13/cobra

# Añadir un replace:
go mod edit -replace github.com/old/pkg=github.com/new/pkg@v1.0.0

# Eliminar un replace:
go mod edit -dropreplace github.com/old/pkg

# Cambiar la versión de Go:
go mod edit -go=1.24.1

# Imprimir go.mod como JSON (útil para scripting):
go mod edit -json
```

```bash
# Caso SysAdmin: script para actualizar una dependencia en todos los repos
#!/bin/bash
for repo in repo1 repo2 repo3; do
    cd "$repo"
    go mod edit -require "github.com/critical/security-fix@v1.5.2"
    go mod tidy
    git add go.mod go.sum
    git commit -m "security: update critical/security-fix to v1.5.2"
    cd ..
done
```

---

## 11. Proxy de módulos y GOPROXY

Go descarga módulos a través de un **proxy** en vez de directamente
de los repositorios. Esto proporciona cache, disponibilidad y verificación.

```
    go get pkg@v1.2.3

    ┌─────────┐     ┌────────────────────┐     ┌──────────────┐
    │ Tu      │────→│ proxy.golang.org   │────→│ GitHub/GitLab│
    │ máquina │     │ (cache + checksums)│     │ (origen)     │
    └─────────┘     └────────────────────┘     └──────────────┘
         │                   │
         │          ┌────────────────────┐
         └─────────→│ sum.golang.org     │
                    │ (transparency log) │
                    └────────────────────┘
```

### 11.1 Configuración de GOPROXY

```bash
# Ver configuración actual:
go env GOPROXY
# https://proxy.golang.org,direct

# Formato: lista de proxies separados por coma
# "direct" = descargar directamente del repositorio como fallback

# Cambiar el proxy:
go env -w GOPROXY=https://proxy.golang.org,https://goproxy.io,direct

# Proxy privado (para empresas):
go env -w GOPROXY=https://goproxy.empresa.com,direct
# Athens (proxy open source): https://gomods.io
# GoProxy.io (alternativa): https://goproxy.io
```

### 11.2 GOPRIVATE — módulos privados

```bash
# Para módulos privados (repos corporativos), hay que indicar a Go
# que NO use el proxy ni sum.golang.org:

go env -w GOPRIVATE=github.com/mi-empresa/*

# GOPRIVATE establece implícitamente:
# GONOSUMDB=github.com/mi-empresa/*
# GONOSUMCHECK=github.com/mi-empresa/*
#
# Múltiples patrones:
go env -w GOPRIVATE=github.com/mi-empresa/*,gitlab.interno.com/*

# También se puede configurar por variable de entorno (en CI):
export GOPRIVATE=github.com/mi-empresa/*
```

### 11.3 Autenticación para repos privados

```bash
# Para repos privados en GitHub/GitLab, Go usa git internamente.
# Configurar git para autenticarse:

# Opción 1: HTTPS con token (recomendado para CI):
git config --global url."https://TOKEN:x-oauth-basic@github.com/".insteadOf \
    "https://github.com/"

# Opción 2: SSH:
git config --global url."git@github.com:".insteadOf "https://github.com/"

# Opción 3: .netrc (para CI/servidores):
cat > ~/.netrc << EOF
machine github.com
login TOKEN
password x-oauth-basic
EOF
chmod 600 ~/.netrc

# Verificar que funciona:
GOPRIVATE=github.com/mi-empresa/* go get github.com/mi-empresa/pkg@latest
```

---

## 12. Workspaces — multi-módulo local

Desde Go 1.18, los **workspaces** permiten trabajar con múltiples módulos
locales sin usar `replace` en go.mod.

```bash
# Escenario: desarrollas una app y una librería al mismo tiempo
# Sin workspaces, necesitas replace en go.mod (y recordar quitarlo)
# Con workspaces, Go sabe que son módulos relacionados

# Estructura:
# myworkspace/
# ├── go.work          ← archivo de workspace
# ├── app/
# │   ├── go.mod
# │   └── main.go
# └── library/
#     ├── go.mod
#     └── lib.go
```

```bash
# Crear un workspace:
mkdir myworkspace && cd myworkspace

# Inicializar:
go work init ./app ./library

# go.work generado:
cat go.work
# go 1.24.1
#
# use (
#     ./app
#     ./library
# )
```

```bash
# Ahora, dentro del workspace:
# - app/ puede importar library/ sin replace
# - Los cambios en library/ se reflejan inmediatamente en app/
# - No hay que modificar go.mod de ninguno

# Agregar otro módulo al workspace:
go work use ./another-module

# Sincronizar go.work con los módulos:
go work sync
```

```bash
# ¿Se commitea go.work al repositorio?
#
# Generalmente NO.
# go.work es para desarrollo local, no para producción.
# Cada módulo debe poder compilarse independientemente.
#
# .gitignore:
# go.work
# go.work.sum
```

### 12.1 Workspaces para contribuir a open source

```bash
# Caso típico: quieres contribuir un fix a una dependencia
# y probarlo con tu proyecto al mismo tiempo.

# 1. Clonar la dependencia:
mkdir workspace && cd workspace
git clone https://github.com/some/dependency
git clone https://github.com/user/myapp

# 2. Crear workspace:
go work init ./myapp ./dependency

# 3. Modificar dependency (el fix)
# 4. En myapp, los cambios se usan automáticamente
# 5. go build ./myapp/...  ← usa el código local de dependency

# 6. Cuando el fix esté listo:
# - Push al fork de dependency
# - Abrir PR
# - Cuando se merge, actualizar myapp: go get dependency@latest
# - Eliminar go.work
```

---

## 13. Actualizaciones y seguridad de dependencias

### 13.1 Listar dependencias con actualizaciones disponibles

```bash
# Ver todas las dependencias y su versión actual:
go list -m all

# Ver dependencias con actualizaciones disponibles:
go list -m -u all
# github.com/spf13/cobra v1.8.1 [v1.9.0]
# El [v1.9.0] indica que hay una versión más nueva

# Formato JSON (para scripting):
go list -m -u -json all
```

### 13.2 Actualizar dependencias

```bash
# Actualizar una dependencia específica:
go get -u github.com/spf13/cobra

# Actualizar solo patches (más seguro):
go get -u=patch github.com/spf13/cobra

# Actualizar TODAS las dependencias directas:
go get -u ./...

# Actualizar todo incluyendo indirectas:
go get -u all

# Después de actualizar:
go mod tidy    # limpiar
go test ./...  # verificar que nada se rompió
```

### 13.3 govulncheck — vulnerabilidades conocidas

```bash
# Instalar govulncheck (herramienta oficial de seguridad):
go install golang.org/x/vuln/cmd/govulncheck@latest

# Escanear el proyecto:
govulncheck ./...

# Ejemplo de salida:
# Vulnerability #1: GO-2024-2631
#   Stdlib: net/http
#   Fixed in: go1.22.1
#   ...
#
# Vulnerability #2: GO-2024-2599
#   Module: github.com/some/pkg
#   Fixed in: v1.5.2
#   ...

# govulncheck es más preciso que scanners genéricos porque
# analiza qué funciones vulnerables REALMENTE llama tu código,
# no solo qué módulos tienes como dependencia.
```

```bash
# Integrar en CI:
govulncheck ./...
# Exit code 0: sin vulnerabilidades
# Exit code 3: vulnerabilidades encontradas
```

### 13.4 Estrategia de actualización para SysAdmin

```bash
# Rutina recomendada (semanal/quincenal):
#
# 1. Verificar vulnerabilidades:
govulncheck ./...
#
# 2. Ver qué actualizaciones hay:
go list -m -u all
#
# 3. Actualizar patches (bajo riesgo):
go get -u=patch ./...
go mod tidy
go test ./...
#
# 4. Actualizar minor (riesgo medio):
go get -u ./...
go mod tidy
go test ./...
#
# 5. Commit y deploy:
git add go.mod go.sum
git commit -m "deps: update dependencies"
```

---

## 14. Cache de módulos

```bash
# Go cachea todos los módulos descargados en:
go env GOMODCACHE
# /home/user/go/pkg/mod

# Ver cuánto espacio ocupa:
du -sh $(go env GOMODCACHE)
# 1.2G /home/user/go/pkg/mod

# Los módulos en cache son de SOLO LECTURA:
ls -la $(go env GOMODCACHE)/github.com/spf13/cobra@v1.8.1/
# dr-xr-xr-x  ...  (read-only)
#
# Esto previene modificaciones accidentales.
# go mod verify comprueba que no se han alterado.

# Limpiar toda la cache de módulos:
go clean -modcache

# Limpiar la cache de build (compilaciones):
go clean -cache

# Limpiar ambas:
go clean -cache -modcache

# Ver cuánto ocupa la cache de build:
du -sh $(go env GOCACHE)
```

---

## 15. Ejemplo completo: proyecto SysAdmin con dependencias

```bash
# Crear un proyecto que usa dependencias reales:
mkdir -p /tmp/sysinfo && cd /tmp/sysinfo
go mod init example.com/sysinfo
```

```go
// main.go
package main

import (
    "encoding/json"
    "fmt"
    "log"
    "os"
    "runtime"
    "time"

    "github.com/shirou/gopsutil/v4/cpu"
    "github.com/shirou/gopsutil/v4/mem"
    "github.com/shirou/gopsutil/v4/host"
)

type SystemInfo struct {
    Hostname    string  `json:"hostname"`
    OS          string  `json:"os"`
    Arch        string  `json:"arch"`
    CPUs        int     `json:"cpus"`
    CPUModel    string  `json:"cpu_model"`
    MemTotalMB  uint64  `json:"mem_total_mb"`
    MemUsedMB   uint64  `json:"mem_used_mb"`
    MemPercent  float64 `json:"mem_percent"`
    Uptime      string  `json:"uptime"`
}

func main() {
    info := SystemInfo{
        OS:   runtime.GOOS,
        Arch: runtime.GOARCH,
        CPUs: runtime.NumCPU(),
    }

    info.Hostname, _ = os.Hostname()

    if cpuInfo, err := cpu.Info(); err == nil && len(cpuInfo) > 0 {
        info.CPUModel = cpuInfo[0].ModelName
    }

    if memInfo, err := mem.VirtualMemory(); err == nil {
        info.MemTotalMB = memInfo.Total / 1024 / 1024
        info.MemUsedMB = memInfo.Used / 1024 / 1024
        info.MemPercent = memInfo.UsedPercent
    }

    if hostInfo, err := host.Info(); err == nil {
        info.Uptime = (time.Duration(hostInfo.Uptime) * time.Second).String()
    }

    enc := json.NewEncoder(os.Stdout)
    enc.SetIndent("", "  ")
    if err := enc.Encode(info); err != nil {
        log.Fatal(err)
    }
}
```

```bash
# Descargar dependencias:
go mod tidy

# Ver qué se añadió a go.mod:
cat go.mod
# module example.com/sysinfo
# go 1.24.1
# require github.com/shirou/gopsutil/v4 v4.25.1
# require (
#     ... dependencias indirectas ...
# )

# Compilar:
CGO_ENABLED=0 go build -ldflags "-s -w" -o sysinfo

# Ejecutar:
./sysinfo
# {
#   "hostname": "server01",
#   "os": "linux",
#   "arch": "amd64",
#   "cpus": 8,
#   "cpu_model": "AMD Ryzen 7 5800X",
#   "mem_total_mb": 32768,
#   "mem_used_mb": 8192,
#   "mem_percent": 25.0,
#   "uptime": "720h15m30s"
# }

# Ver el grafo de dependencias:
go mod graph | head -10

# ¿Por qué tengo golang.org/x/sys?
go mod why golang.org/x/sys
# gopsutil necesita acceso a /proc y syscalls
```

---

## 16. Errores comunes

| Error | Causa | Solución |
|-------|-------|----------|
| `go: cannot find main module` | No hay `go.mod` | Ejecutar `go mod init` |
| `cannot find module providing package X` | Falta dependencia | Ejecutar `go mod tidy` o `go get X` |
| `ambiguous import` | Dos módulos exportan el mismo path | Usar `replace` o actualizar dependencias |
| `checksum mismatch` | Módulo modificado después de publicar | Limpiar cache: `go clean -modcache` |
| `invalid version: unknown revision` | Tag o commit no existe | Verificar que el tag existe en el repo |
| `go.sum is out of sync` | Falta ejecutar `go mod tidy` | Ejecutar `go mod tidy` |
| `module declares its path as X but was required as Y` | Module path no coincide con ruta de importación | Verificar `module` en go.mod del módulo |
| `go mod vendor: open vendor/modules.txt: permission denied` | Permisos en vendor/ | `rm -rf vendor && go mod vendor` |
| `reading https://proxy.golang.org/...: 410 Gone` | Versión retractada | Usar otra versión: `go get pkg@v1.2.4` |
| `GONOSUMCHECK not set for private module` | Módulo privado sin GOPRIVATE | `go env -w GOPRIVATE=empresa.com/*` |
| `go.mod tidy: unused require` | Dependencia eliminada del código | `go mod tidy` la elimina |
| `cannot use v2 module without /v2 in path` | Import de v2+ sin sufijo | Cambiar import a `pkg/v2` |

---

## 17. Ejercicios

### Ejercicio 1 — Crear un módulo desde cero

```bash
# 1. Crear un directorio y un módulo:
mkdir -p /tmp/mod-ex1 && cd /tmp/mod-ex1
go mod init example.com/greet

# 2. Crear main.go que imprima "Hello, modules!"

# 3. Examinar go.mod: ¿qué contiene?

# 4. Ejecutar go build y go run
```

**Predicción**: ¿Cuántas líneas tendrá go.mod después de `go mod init`?
¿Existirá go.sum?

<details>
<summary>Ver solución</summary>

```go
// main.go
package main

import "fmt"

func main() {
    fmt.Println("Hello, modules!")
}
```

```bash
cat go.mod
# module example.com/greet
#
# go 1.24.1

# go.mod tiene solo 3 líneas: module, línea vacía, directiva go.
# NO existe go.sum porque no hay dependencias externas.
# go.sum solo aparece cuando se descarga al menos un módulo.

go build -o greet && ./greet
# Hello, modules!
```

go.sum se crea cuando ejecutas `go get`, `go mod tidy` con
dependencias, o `go mod download`. Sin dependencias externas,
solo se necesita go.mod.
</details>

---

### Ejercicio 2 — Añadir una dependencia

```bash
cd /tmp/mod-ex1

# 1. Añadir color al output:
#    Importar "github.com/fatih/color" en main.go
#    Usar color.Green("Hello, modules!") en vez de fmt.Println

# 2. Ejecutar go mod tidy

# 3. Examinar go.mod: ¿qué cambió?

# 4. Examinar go.sum: ¿cuántas líneas tiene?

# 5. Compilar y ejecutar
```

**Predicción**: ¿`go mod tidy` descargará solo `fatih/color` o también
otras dependencias?

<details>
<summary>Ver solución</summary>

```go
package main

import "github.com/fatih/color"

func main() {
    color.Green("Hello, modules!")
}
```

```bash
go mod tidy

cat go.mod
# module example.com/greet
#
# go 1.24.1
#
# require github.com/fatih/color v1.18.0
#
# require (
#     github.com/mattn/go-colorable v0.1.14 // indirect
#     github.com/mattn/go-isatty v0.0.20 // indirect
#     golang.org/x/sys v0.29.0 // indirect
# )

wc -l go.sum
# ~16 líneas (varía según versión)
```

`go mod tidy` descarga `fatih/color` **y sus dependencias transitivas**:
`go-colorable` (colores en Windows), `go-isatty` (detectar terminal),
`golang.org/x/sys` (syscalls). Estas aparecen como `// indirect`.
</details>

---

### Ejercicio 3 — go mod graph y go mod why

```bash
cd /tmp/mod-ex1

# 1. Ejecutar go mod graph y analizar la salida

# 2. ¿Por qué el proyecto depende de golang.org/x/sys?
#    Usar go mod why para averiguarlo

# 3. ¿Cuántas dependencias transitivas hay?
```

**Predicción**: ¿Cuántos niveles de profundidad tiene el grafo de
dependencias de `fatih/color`?

<details>
<summary>Ver solución</summary>

```bash
go mod graph
# example.com/greet github.com/fatih/color@v1.18.0
# github.com/fatih/color@v1.18.0 github.com/mattn/go-colorable@v0.1.14
# github.com/fatih/color@v1.18.0 github.com/mattn/go-isatty@v0.0.20
# github.com/mattn/go-colorable@v0.1.14 github.com/mattn/go-isatty@v0.0.20
# github.com/mattn/go-colorable@v0.1.14 golang.org/x/sys@v0.29.0
# github.com/mattn/go-isatty@v0.0.20 golang.org/x/sys@v0.29.0

go mod why golang.org/x/sys
# # golang.org/x/sys
# example.com/greet
# github.com/fatih/color
# github.com/mattn/go-colorable
# golang.org/x/sys

# 3 niveles de profundidad:
# greet → color → go-colorable → golang.org/x/sys
#                  go-isatty → golang.org/x/sys

# Total dependencias transitivas: 4
# (color, go-colorable, go-isatty, golang.org/x/sys)
```

El grafo de `fatih/color` tiene 3 niveles. `golang.org/x/sys` está
al final porque proporciona acceso a syscalls del SO (necesario para
detectar si stdout es una terminal y soportar colores ANSI).
</details>

---

### Ejercicio 4 — Vendoring

```bash
cd /tmp/mod-ex1

# 1. Vendorear las dependencias:
go mod vendor

# 2. ¿Qué hay en vendor/?
ls vendor/
ls -R vendor/ | head -20

# 3. Compilar usando solo vendor (sin internet):
go build -mod=vendor -o greet

# 4. ¿Cuánto espacio ocupa vendor/?
du -sh vendor/

# 5. Verificar integridad:
go mod verify
```

**Predicción**: ¿El binario compilado con `-mod=vendor` será idéntico
al compilado sin vendor?

<details>
<summary>Ver solución</summary>

```bash
go mod vendor
ls vendor/
# github.com/
# golang.org/
# modules.txt

du -sh vendor/
# ~2.5M

go build -mod=vendor -o greet-vendor
go build -o greet-normal

# Los binarios son idénticos en contenido (mismo código fuente),
# pero el hash del archivo puede diferir ligeramente por timestamps
# de compilación. Con -trimpath, los binarios son reproducibles:
CGO_ENABLED=0 go build -trimpath -mod=vendor -o v1
CGO_ENABLED=0 go build -trimpath -o v2
sha256sum v1 v2
# Deberían ser idénticos con trimpath + mismas flags

go mod verify
# all modules verified
```

El código compilado es el mismo independientemente de si viene de
vendor/ o de la cache de módulos. La diferencia es de dónde se lee
el código fuente, no qué se compila.
</details>

---

### Ejercicio 5 — replace para desarrollo local

```bash
# 1. Crear dos módulos: app y library
mkdir -p /tmp/mod-ex5/app /tmp/mod-ex5/library

# 2. En library: crear un paquete con una función Greet(name) string
cd /tmp/mod-ex5/library
go mod init example.com/library
# Crear greet.go con func Greet(name string) string

# 3. En app: importar library y usar Greet
cd /tmp/mod-ex5/app
go mod init example.com/app
# Crear main.go que importe example.com/library

# 4. Sin replace, ¿qué error da go build?

# 5. Agregar replace en go.mod de app y verificar que compila
```

**Predicción**: ¿Qué error dará `go build` sin `replace`?

<details>
<summary>Ver solución</summary>

```go
// library/greet.go
package library

import "fmt"

func Greet(name string) string {
    return fmt.Sprintf("Hello, %s!", name)
}
```

```go
// app/main.go
package main

import (
    "fmt"
    "example.com/library"
)

func main() {
    fmt.Println(library.Greet("SysAdmin"))
}
```

```bash
cd /tmp/mod-ex5/app
go build
# go: example.com/library@v0.0.0: reading https://proxy.golang.org/...
# 410 Gone
# (el módulo no existe en internet)
```

Solución — añadir replace en go.mod:

```bash
go mod edit -replace example.com/library=../library
go mod tidy
go build -o app && ./app
# Hello, SysAdmin!

cat go.mod
# module example.com/app
# go 1.24.1
# require example.com/library v0.0.0-00010101000000-000000000000
# replace example.com/library => ../library
```

Sin `replace`, Go intenta descargar `example.com/library` del proxy,
pero no existe. `replace` redirige la resolución al directorio local.
</details>

---

### Ejercicio 6 — Workspace multi-módulo

```bash
# Usar los módulos del ejercicio 5, pero con workspaces en vez de replace.

# 1. En /tmp/mod-ex5, crear un workspace:
cd /tmp/mod-ex5
go work init ./app ./library

# 2. Eliminar el replace de app/go.mod

# 3. Verificar que go build ./app/... funciona

# 4. Examinar go.work
```

**Predicción**: ¿Con workspaces, app/go.mod necesita `replace`?

<details>
<summary>Ver solución</summary>

```bash
cd /tmp/mod-ex5

# Eliminar replace de app/go.mod:
cd app
go mod edit -dropreplace example.com/library
cd ..

# Crear workspace:
go work init ./app ./library

cat go.work
# go 1.24.1
#
# use (
#     ./app
#     ./library
# )

# Compilar:
go build ./app/...
./app/app
# Hello, SysAdmin!
```

**No**, con workspaces `app/go.mod` no necesita `replace`. El workspace
(`go.work`) le dice a Go que `example.com/library` se resuelve en
`./library`. Esto es más limpio que `replace` porque:

1. No modifica go.mod (cada módulo se mantiene independiente)
2. go.work no se commitea (es local de desarrollo)
3. Se puede agregar/quitar módulos sin tocar go.mod

Cuando el desarrollo termina, se publica `library`, se actualiza
`app/go.mod` con la versión real, y se elimina `go.work`.
</details>

---

### Ejercicio 7 — Dockerfile optimizado con modules

```bash
# Crear un Dockerfile que:
# 1. Use multi-stage build
# 2. Separe go mod download del código fuente (cache de capas)
# 3. Genere un binario estático
# 4. Use FROM scratch como imagen final
#
# Usar el proyecto sysinfo de la sección 15 o cualquier otro con deps.
```

**Predicción**: ¿Cuánto pesará la imagen final? ¿Más o menos que
`golang:1.24-alpine` (~250MB)?

<details>
<summary>Ver solución</summary>

```dockerfile
# Dockerfile
FROM golang:1.24-alpine AS builder
WORKDIR /app

# Capa 1: dependencias (se cachea si go.mod/go.sum no cambian)
COPY go.mod go.sum ./
RUN go mod download

# Capa 2: código fuente + compilación
COPY . .
RUN CGO_ENABLED=0 go build -ldflags "-s -w" -trimpath -o /server

# Imagen final: FROM scratch = 0 bytes de base
FROM scratch
COPY --from=builder /server /server
ENTRYPOINT ["/server"]
```

```bash
docker build -t myapp .
docker images myapp
# REPOSITORY   TAG     SIZE
# myapp        latest  ~3-6MB   (solo el binario Go)

# Comparación:
# golang:1.24         ~800MB
# golang:1.24-alpine  ~250MB
# alpine:3.20         ~7MB
# scratch + binario   ~3-6MB  ← nuestra imagen
```

La imagen final pesa **solo lo que pesa el binario** (~3-6MB),
porque `FROM scratch` es una imagen vacía (0 bytes). Esto es posible
porque `CGO_ENABLED=0` genera un binario estáticamente enlazado que
no necesita libc ni ninguna otra biblioteca del sistema.

La separación `COPY go.mod go.sum` + `RUN go mod download` antes de
`COPY .` hace que cambios en el código fuente no invaliden la cache
de dependencias. Esto reduce los tiempos de build de minutos a segundos
en builds incrementales.
</details>

---

### Ejercicio 8 — Seguridad de dependencias

```bash
# 1. Instalar govulncheck:
go install golang.org/x/vuln/cmd/govulncheck@latest

# 2. Crear un proyecto con una dependencia intencionalmente vieja:
mkdir -p /tmp/mod-ex8 && cd /tmp/mod-ex8
go mod init example.com/vulntest

# 3. Añadir una versión antigua de una librería popular:
go get golang.org/x/text@v0.3.7

# 4. Crear un main.go que use golang.org/x/text/language

# 5. Ejecutar govulncheck ./...

# 6. ¿Qué vulnerabilidades encuentra? ¿Cómo se solucionan?
```

**Predicción**: ¿govulncheck reportará vulnerabilidades aunque no uses
directamente la función vulnerable?

<details>
<summary>Ver solución</summary>

```go
// main.go
package main

import (
    "fmt"
    "golang.org/x/text/language"
)

func main() {
    tag, _ := language.Parse("es")
    fmt.Println("Language:", tag)
}
```

```bash
go mod tidy
govulncheck ./...
# Scanning your code and 42 packages across 3 modules...
#
# No vulnerabilities found.
# (o puede encontrar alguna si v0.3.7 afecta a language)
```

`govulncheck` es más inteligente que un scanner genérico: analiza
el **call graph** real de tu código. Solo reporta vulnerabilidades
en funciones que tu código realmente llama, no en todo el módulo.

Si una vulnerabilidad está en `golang.org/x/text/transform` pero
tu código solo usa `language.Parse`, govulncheck **no la reporta**
(aunque un scanner como `snyk` o `dependabot` sí lo haría).

Para solucionar las que sí se reporten:

```bash
go get -u golang.org/x/text@latest
go mod tidy
govulncheck ./...
# No vulnerabilities found.
```
</details>

---

### Ejercicio 9 — Scripting con go mod edit

```bash
# Escribe un script bash que:
# 1. Recorra todos los directorios que contengan go.mod
# 2. Para cada uno, muestre el module path y la versión de Go
# 3. Liste las dependencias directas (no indirect)
#
# Pista: go mod edit -json produce salida JSON procesable con jq
```

**Predicción**: ¿Qué campo del JSON de `go mod edit -json` contiene
las dependencias directas?

<details>
<summary>Ver solución</summary>

```bash
#!/bin/bash
# list-modules.sh — lista módulos Go en subdirectorios

set -euo pipefail

find . -name go.mod -type f | sort | while read -r modfile; do
    dir=$(dirname "$modfile")
    echo "=== $dir ==="

    # Module path y versión de Go:
    mod=$(go mod edit -json "$modfile" | jq -r '.Module.Path')
    goversion=$(go mod edit -json "$modfile" | jq -r '.Go')
    echo "  Module:  $mod"
    echo "  Go:      $goversion"

    # Dependencias directas (Require sin Indirect):
    deps=$(go mod edit -json "$modfile" | \
        jq -r '.Require[]? | select(.Indirect != true) | "  - \(.Path) \(.Version)"')

    if [ -n "$deps" ]; then
        echo "  Dependencies:"
        echo "$deps"
    else
        echo "  Dependencies: (none)"
    fi
    echo
done
```

El campo `Require` del JSON contiene un array de objetos:

```json
{
  "Require": [
    {"Path": "github.com/spf13/cobra", "Version": "v1.8.1"},
    {"Path": "github.com/spf13/pflag", "Version": "v1.0.5", "Indirect": true}
  ]
}
```

Las dependencias directas son las que **no** tienen `"Indirect": true`.
`jq` permite filtrarlas con `select(.Indirect != true)`.
</details>

---

### Ejercicio 10 — Migrar de GOPATH a modules

```bash
# Simula un proyecto legacy sin go.mod:
mkdir -p /tmp/mod-ex10 && cd /tmp/mod-ex10

# 1. Crear main.go que use fmt y os (solo stdlib):
# package main
# import ("fmt"; "os")
# func main() { fmt.Println("Args:", os.Args[1:]) }

# 2. Intentar compilar SIN go.mod: ¿qué error da?

# 3. Inicializar el módulo con go mod init

# 4. Ahora añadir una dependencia:
#    Importar "github.com/spf13/pflag" en vez de os.Args
#    pflag.Parse()

# 5. Ejecutar go mod tidy y verificar que go.mod y go.sum se crearon

# 6. Compilar y ejecutar: ./app --help
```

**Predicción**: ¿Se puede compilar un archivo Go sin `go.mod`?

<details>
<summary>Ver solución</summary>

```bash
# Sin go.mod:
cat > main.go << 'EOF'
package main

import (
    "fmt"
    "os"
)

func main() {
    fmt.Println("Args:", os.Args[1:])
}
EOF

go build main.go
# go: cannot find main module, but found .go files in /tmp/mod-ex10
#     to create a module there, run:
#     go mod init
```

**No**, desde Go 1.16 no se puede compilar sin `go.mod` (excepto
con `go run file.go` de un solo archivo sin dependencias externas).

```bash
# Inicializar:
go mod init example.com/app

# Ahora con pflag:
cat > main.go << 'EOF'
package main

import (
    "fmt"
    "github.com/spf13/pflag"
)

var name = pflag.String("name", "World", "who to greet")

func main() {
    pflag.Parse()
    fmt.Printf("Hello, %s!\n", *name)
}
EOF

go mod tidy
go build -o app

./app
# Hello, World!

./app --name=SysAdmin
# Hello, SysAdmin!

./app --help
# Usage of ./app:
#       --name string   who to greet (default "World")
```

`go mod tidy` detecta el import de `github.com/spf13/pflag`,
lo descarga, lo añade a go.mod con la última versión, y genera
go.sum con los checksums.
</details>
