# Instalación y go command

## 1. Qué es Go y por qué importa para SysAdmin

Go (también llamado Golang) es un lenguaje compilado, estáticamente tipado,
creado en Google en 2009 por Robert Griesemer, Rob Pike y Ken Thompson.
Su diseño prioriza la **simplicidad**, la **concurrencia** y la **velocidad
de compilación**.

```
    ¿Por qué Go domina el ecosistema DevOps/SysAdmin?

    Docker ─────────────────── escrito en Go
    Kubernetes ─────────────── escrito en Go
    Terraform ──────────────── escrito en Go
    Prometheus ─────────────── escrito en Go
    Grafana ────────────────── escrito en Go
    etcd ───────────────────── escrito en Go
    containerd ─────────────── escrito en Go
    Consul / Vault / Nomad ─── escritos en Go
    Traefik ────────────────── escrito en Go
    CockroachDB ────────────── escrito en Go
    Hugo ───────────────────── escrito en Go
```

Saber Go no es solo saber otro lenguaje: es poder **leer el código fuente**
de las herramientas que usas a diario, escribir operadores de Kubernetes,
plugins de Terraform, exporters de Prometheus y CLIs profesionales.

### 1.1 Características clave

```
    ┌────────────────────────────────────────────────────────────┐
    │                  Diseño de Go                              │
    ├──────────────────┬─────────────────────────────────────────┤
    │ Compilado        │ Binario nativo, sin VM ni intérprete    │
    │ Estático         │ Tipos resueltos en compilación          │
    │ Garbage collected│ GC de baja latencia, sin gestión manual │
    │ Concurrente      │ Goroutines + channels nativos           │
    │ Compilación rápida│ Segundos incluso en proyectos grandes  │
    │ Binario único    │ Todo enlazado estáticamente (CGO=0)     │
    │ Cross-compile    │ GOOS=linux GOARCH=arm64 go build        │
    │ Toolchain unificada│ fmt, test, vet, doc en el propio go   │
    └──────────────────┴─────────────────────────────────────────┘
```

### 1.2 Go vs C vs Rust — posicionamiento

| Aspecto | C | Rust | Go |
|---------|---|------|----|
| Gestión de memoria | Manual (malloc/free) | Ownership (compile-time) | Garbage collector |
| Compilación | Rápida | Lenta | Muy rápida |
| Concurrencia | Manual (pthreads) | async/await + tokio | Goroutines nativas |
| Binario estático | Con flags (-static) | Por defecto (musl) | Por defecto (CGO_ENABLED=0) |
| Curva de aprendizaje | Media-alta | Alta | Baja |
| Ecosistema cloud-native | Limitado | Creciendo | Dominante |
| Error handling | Códigos de retorno | Result<T,E> | `if err != nil` |

Go no reemplaza a C ni a Rust: ocupa un nicho diferente. Donde C y Rust
buscan control máximo y zero-cost abstractions, Go busca **productividad**
y **simplicidad** para software de red, servicios y herramientas CLI.

---

## 2. Instalación

### 2.1 Método oficial (recomendado)

Go se instala descargando el tarball oficial desde `go.dev/dl` y
extrayéndolo en `/usr/local`:

```bash
# 1. Descargar la versión más reciente (verificar en go.dev/dl)
GO_VERSION="1.24.1"
curl -OL "https://go.dev/dl/go${GO_VERSION}.linux-amd64.tar.gz"

# 2. Eliminar instalación previa si existe
sudo rm -rf /usr/local/go

# 3. Extraer en /usr/local
sudo tar -C /usr/local -xzf "go${GO_VERSION}.linux-amd64.tar.gz"

# 4. Agregar al PATH (en ~/.bashrc o ~/.zshrc)
echo 'export PATH=$PATH:/usr/local/go/bin' >> ~/.bashrc
echo 'export PATH=$PATH:$(go env GOPATH)/bin' >> ~/.bashrc
source ~/.bashrc

# 5. Verificar
go version
# go version go1.24.1 linux/amd64
```

```bash
# ¿Qué se instaló?
ls /usr/local/go/
# bin/     → go, gofmt
# src/     → código fuente de la stdlib
# pkg/     → paquetes precompilados
# doc/     → documentación
# lib/     → bibliotecas internas
# misc/    → herramientas auxiliares

ls /usr/local/go/bin/
# go       → el comando principal
# gofmt    → formateador de código
```

### 2.2 Instalación con gestor de paquetes

```bash
# Fedora/RHEL (puede no ser la última versión):
sudo dnf install golang

# Debian/Ubuntu (puede no ser la última versión):
sudo apt install golang-go

# Verificar versión disponible vs última oficial:
go version
# Comparar con go.dev/dl
```

> **Nota**: los gestores de paquetes suelen tener versiones atrasadas.
> Para Go, el método oficial es preferible porque garantiza la última
> versión y el equipo de Go mantiene la retrocompatibilidad entre releases.

### 2.3 Múltiples versiones

```bash
# Instalar una versión específica en paralelo:
go install golang.org/dl/go1.22.0@latest
go1.22.0 download

# Usar la versión específica:
go1.22.0 version
# go version go1.22.0 linux/amd64

# La versión principal sigue siendo la de /usr/local/go:
go version
# go version go1.24.1 linux/amd64
```

### 2.4 Estructura de directorios

```bash
# Go usa dos directorios principales:

# GOROOT — donde está instalado Go
go env GOROOT
# /usr/local/go

# GOPATH — workspace del usuario (módulos descargados, binarios instalados)
go env GOPATH
# /home/user/go (por defecto)

# Estructura de GOPATH:
# ~/go/
# ├── bin/      → binarios instalados con go install
# │   ├── gopls
# │   ├── golangci-lint
# │   └── dlv
# ├── pkg/      → cache de módulos compilados
# │   └── mod/  → código fuente de dependencias descargadas
# │       ├── cache/
# │       ├── github.com/
# │       └── golang.org/
# └── src/      → (legacy, ya no se usa con go modules)
```

```bash
# Asegurar que ~/go/bin está en el PATH:
# Esto permite ejecutar herramientas instaladas con go install

echo $PATH | tr ':' '\n' | grep go
# /usr/local/go/bin          → el compilador
# /home/user/go/bin          → herramientas instaladas por el usuario

# Si ~/go/bin no aparece, agregar al shell profile:
echo 'export PATH=$PATH:$(go env GOPATH)/bin' >> ~/.bashrc
```

### 2.5 Desinstalación

```bash
# Eliminar Go completamente:
sudo rm -rf /usr/local/go

# Eliminar el workspace del usuario (opcional):
rm -rf ~/go

# Eliminar líneas de PATH del shell profile:
# Editar ~/.bashrc o ~/.zshrc y eliminar las líneas de export PATH
```

---

## 3. El comando go — subcomandos principales

El binario `go` es la **herramienta unificada** de Go. A diferencia de C
(gcc, make, gdb, valgrind) o Rust (rustc, cargo, clippy, rustfmt), Go
concentra compilación, testing, formateo, análisis y documentación en un
solo comando.

```
    ┌─────────────────────────────────────────────────────────┐
    │                    go (subcomandos)                      │
    ├─────────────┬───────────────────────────────────────────┤
    │ Compilar    │ go build, go run, go install              │
    │ Testing     │ go test                                   │
    │ Módulos     │ go mod init, go mod tidy, go get          │
    │ Análisis    │ go vet                                    │
    │ Formato     │ go fmt (invoca gofmt)                     │
    │ Documentar  │ go doc                                    │
    │ Herramientas│ go tool, go generate, go env              │
    │ Workspace   │ go work                                   │
    └─────────────┴───────────────────────────────────────────┘
```

---

## 4. go build — compilar

`go build` compila un paquete Go y genera un binario ejecutable.

```bash
# Crear un proyecto de ejemplo:
mkdir -p /tmp/hello && cd /tmp/hello
go mod init example.com/hello
```

```go
// main.go
package main

import "fmt"

func main() {
    fmt.Println("Hello, Go!")
}
```

```bash
# Compilar:
go build
# Genera el binario "hello" (nombre del módulo) en el directorio actual

ls -la hello
# -rwxr-xr-x 1 user user 1.8M ... hello

./hello
# Hello, Go!
```

```bash
# Especificar nombre del binario:
go build -o myapp
./myapp
# Hello, Go!

# Compilar un archivo específico:
go build -o myapp main.go

# Compilar un paquete por ruta:
go build ./cmd/server

# Compilar todos los paquetes del proyecto:
go build ./...
```

### 4.1 Flags útiles de go build

```bash
# -v — verbose: muestra los paquetes que se compilan
go build -v
# example.com/hello

# -x — muestra los comandos ejecutados internamente
go build -x
# (muestra las invocaciones a compile, link, etc.)

# -race — habilita el race detector (para detectar data races)
go build -race -o myapp
# El binario será más lento pero detectará accesos concurrentes

# -ldflags — pasar flags al linker
# Inyectar versión en el binario:
go build -ldflags "-X main.version=1.2.3" -o myapp

# Reducir tamaño del binario:
go build -ldflags "-s -w" -o myapp
# -s → omitir tabla de símbolos
# -w → omitir información de depuración DWARF
ls -la myapp
# El binario es notablemente más pequeño

# -trimpath — eliminar rutas absolutas del binario
go build -trimpath -o myapp
# Mejora la reproducibilidad del build
```

### 4.2 Binario estático — la killer feature para SysAdmin

```bash
# Por defecto, Go enlaza dinámicamente si usa paquetes como net:
go build -o myapp
file myapp
# ELF 64-bit, dynamically linked, ...
ldd myapp
# linux-vdso.so.1, libc.so.6, libpthread.so.0, ...

# Para generar un binario 100% estático:
CGO_ENABLED=0 go build -o myapp
file myapp
# ELF 64-bit, statically linked, ...
ldd myapp
# not a dynamic executable

# ¿Por qué importa? Un binario estático:
# - Funciona en cualquier Linux sin dependencias
# - Se puede copiar a un contenedor FROM scratch (0 bytes de base)
# - No depende de la versión de glibc del host
# - Ideal para distribución: un solo archivo = toda la aplicación
```

```dockerfile
# Ejemplo: contenedor Docker mínimo con binario Go
# Dockerfile
FROM scratch
COPY myapp /myapp
ENTRYPOINT ["/myapp"]

# docker build -t myapp .
# docker images myapp
# REPOSITORY   TAG     IMAGE ID      SIZE
# myapp        latest  abc123...     1.8MB  ← solo el binario!
```

---

## 5. go run — compilar y ejecutar

`go run` compila y ejecuta en un solo paso. No genera un binario
persistente — es para desarrollo rápido.

```bash
# Ejecutar directamente:
go run main.go
# Hello, Go!

# Ejecutar un paquete completo (con múltiples archivos):
go run .
# Hello, Go!

# Pasar argumentos al programa:
go run main.go arg1 arg2

# Ejecutar con race detector:
go run -race main.go
```

```bash
# go run vs go build:
#
# go run main.go
#   1. Compila a un binario temporal en /tmp
#   2. Ejecuta el binario
#   3. Elimina el binario al terminar
#   Uso: desarrollo, scripts rápidos, pruebas
#
# go build
#   1. Compila a un binario en el directorio actual
#   2. El binario queda para reutilizar
#   Uso: distribución, deployment, contenedores
```

---

## 6. go install — compilar e instalar

`go install` compila y coloca el binario en `$GOPATH/bin` (o `$GOBIN`
si está definido), haciéndolo disponible en el PATH.

```bash
# Instalar un programa desde un módulo remoto:
go install golang.org/x/tools/gopls@latest
# Descarga, compila e instala gopls en ~/go/bin/

which gopls
# /home/user/go/bin/gopls

# Instalar una versión específica:
go install github.com/golangci/golangci-lint/cmd/golangci-lint@v1.62.0

# Instalar el binario del proyecto actual:
cd /tmp/hello
go install
# Instala "hello" en ~/go/bin/hello
```

```bash
# Herramientas útiles para SysAdmin instalables con go install:

# Linter oficial:
go install github.com/golangci/golangci-lint/cmd/golangci-lint@latest

# Debugger:
go install github.com/go-delve/delve/cmd/dlv@latest

# Servidor LSP (para editores):
go install golang.org/x/tools/gopls@latest

# Gestión de imports:
go install golang.org/x/tools/cmd/goimports@latest
```

### 6.1 go install vs go get

```bash
# go install → instalar binarios ejecutables
go install github.com/some/tool@latest   # ✓ correcto

# go get → gestionar dependencias del módulo actual
cd my-project
go get github.com/some/library@v1.2.3   # ✓ correcto

# Antes de Go 1.17, go get hacía ambas cosas.
# Desde Go 1.17, las funciones están separadas:
#   go install = instalar binarios
#   go get     = gestionar dependencias del go.mod
```

---

## 7. go fmt — formateo obligatorio

Go tiene un formateador de código **canónico**: `gofmt`. El comando
`go fmt` es un wrapper que invoca `gofmt -l -w` sobre los archivos
del paquete.

```bash
# Formatear todos los archivos del proyecto:
go fmt ./...

# Ver qué archivos se modificarían (sin modificar):
gofmt -d .
# Muestra el diff de cambios

# Verificar que todo está formateado (útil en CI):
gofmt -l .
# Lista archivos que no siguen el formato estándar
# Si la lista está vacía, todo está correcto
```

```go
// Antes de go fmt:
package main
import "fmt"
func main(){
x:=42
if x>0{
fmt.Println(x)
}
}

// Después de go fmt:
package main

import "fmt"

func main() {
	x := 42
	if x > 0 {
		fmt.Println(x)
	}
}
```

```bash
# ¿Por qué es importante?
#
# 1. TODOS los proyectos Go tienen el mismo estilo
#    No hay debates sobre tabs vs spaces (Go usa tabs)
#    No hay debates sobre posición de llaves (siempre misma línea)
#
# 2. Los diffs de git son limpios
#    Sin ruido de reformateo entre commits
#
# 3. Se integra en CI: si gofmt -l . produce salida, el build falla
#
# En Go, el formato no es una preferencia: es una regla del ecosistema.
```

---

## 8. go vet — análisis estático

`go vet` examina el código Go buscando errores comunes que el compilador
no detecta: argumentos incorrectos a `fmt.Printf`, comparaciones siempre
verdaderas/falsas, código inalcanzable, etc.

```bash
# Ejecutar go vet en todo el proyecto:
go vet ./...
```

```go
// Ejemplo: error que go vet detecta

package main

import "fmt"

func main() {
    name := "Alice"
    fmt.Printf("Hello, %d\n", name)  // %d con string
}
```

```bash
go vet .
# ./main.go:7:2: fmt.Printf format %d has arg name of wrong type string

# El compilador NO detecta esto — compila sin error.
# go vet SÍ lo detecta.
```

```bash
# Otros errores que go vet detecta:
#
# - Printf con formato incorrecto (%d para string, args de más/menos)
# - Copiar un sync.Mutex (debe pasarse por puntero)
# - Loops con goroutines que capturan la variable del loop (pre-Go 1.22)
# - Unreachable code después de return/panic
# - Tests con firma incorrecta
# - Struct tags mal formadas (json:"name" vs json: "name")
# - Comparación de funciones con nil
```

---

## 9. go test — testing integrado

Go tiene testing como parte del lenguaje, no como dependencia externa.

```bash
# Ejecutar tests del paquete actual:
go test

# Ejecutar tests de todo el proyecto:
go test ./...

# Verbose (mostrar cada test individual):
go test -v ./...

# Ejecutar un test específico por nombre:
go test -run TestSum ./...

# Con race detector:
go test -race ./...

# Con cobertura:
go test -cover ./...

# Generar informe de cobertura HTML:
go test -coverprofile=coverage.out ./...
go tool cover -html=coverage.out
```

```go
// math.go
package math

func Sum(a, b int) int {
    return a + b
}
```

```go
// math_test.go
package math

import "testing"

func TestSum(t *testing.T) {
    got := Sum(2, 3)
    want := 5
    if got != want {
        t.Errorf("Sum(2, 3) = %d, want %d", got, want)
    }
}
```

```bash
go test -v
# === RUN   TestSum
# --- PASS: TestSum (0.00s)
# PASS
# ok      example.com/hello/math    0.001s
```

---

## 10. go doc — documentación

```bash
# Ver documentación de un paquete:
go doc fmt
# package fmt // import "fmt"
# Package fmt implements formatted I/O...

# Ver documentación de una función:
go doc fmt.Println
# func Println(a ...any) (n int, err error)
#     Println formats using the default formats...

# Ver documentación de un tipo:
go doc os.File

# Ver el código fuente:
go doc -src fmt.Println

# Ver toda la documentación del paquete:
go doc -all fmt
```

---

## 11. go env — variables de entorno

`go env` muestra y modifica las variables de entorno que controlan el
comportamiento de Go.

```bash
# Ver todas las variables:
go env

# Ver una variable específica:
go env GOPATH
# /home/user/go

go env GOROOT
# /usr/local/go

go env GOOS
# linux

go env GOARCH
# amd64
```

### 11.1 Variables más importantes

| Variable | Descripción | Valor típico |
|----------|-------------|--------------|
| `GOROOT` | Directorio de instalación de Go | `/usr/local/go` |
| `GOPATH` | Workspace del usuario | `~/go` |
| `GOBIN` | Directorio para binarios de `go install` | `$GOPATH/bin` |
| `GOOS` | Sistema operativo destino | `linux`, `darwin`, `windows` |
| `GOARCH` | Arquitectura destino | `amd64`, `arm64`, `386` |
| `CGO_ENABLED` | Habilita/deshabilita CGO | `1` (habilitado) |
| `GOMODCACHE` | Cache de módulos descargados | `$GOPATH/pkg/mod` |
| `GOPROXY` | Proxy para descargar módulos | `https://proxy.golang.org,direct` |
| `GOPRIVATE` | Módulos que no pasan por proxy | (vacío) |
| `GOFLAGS` | Flags por defecto para go commands | (vacío) |

```bash
# Modificar una variable de entorno de Go:
go env -w GOBIN=/usr/local/bin
# Se persiste en ~/go/env (no en el shell)

# Resetear a valor por defecto:
go env -u GOBIN

# Archivo de configuración:
cat $(go env GOENV)
# /home/user/.config/go/env
```

---

## 12. go tool — herramientas internas

```bash
# Listar herramientas disponibles:
go tool

# Herramientas útiles:
# go tool pprof   → profiling (CPU, memoria)
# go tool trace   → tracing de goroutines
# go tool cover   → cobertura de tests
# go tool compile → acceso directo al compilador
# go tool link    → acceso directo al linker

# Ejemplo: ver el ensamblador generado
go build -gcflags="-S" main.go

# Ejemplo: optimizaciones del compilador
go build -gcflags="-m" main.go
# Muestra qué variables escapan al heap (escape analysis)
```

---

## 13. Flujo de trabajo típico

```bash
# 1. Crear un proyecto nuevo:
mkdir myproject && cd myproject
go mod init example.com/myproject

# 2. Escribir código:
# editor main.go

# 3. Formatear:
go fmt ./...

# 4. Análisis estático:
go vet ./...

# 5. Compilar:
go build

# 6. Ejecutar tests:
go test ./...

# 7. Compilar para producción:
CGO_ENABLED=0 go build -ldflags "-s -w" -trimpath -o myapp

# 8. Instalar herramientas externas:
go install github.com/some/tool@latest
```

```
    Flujo de desarrollo Go

    ┌──────────┐    ┌─────────┐    ┌─────────┐    ┌──────────┐
    │ Escribir │───→│ go fmt  │───→│ go vet  │───→│ go test  │
    │ código   │    │ formato │    │ análisis│    │ tests    │
    └──────────┘    └─────────┘    └─────────┘    └────┬─────┘
                                                       │
                    ┌──────────────────────────────────┘
                    ▼
    ┌──────────┐    ┌─────────────────────────────────────┐
    │ go build │───→│ CGO_ENABLED=0 go build -ldflags ... │
    │ (dev)    │    │ (producción / Docker)                │
    └──────────┘    └─────────────────────────────────────┘
```

---

## 14. GOPATH vs Go Modules — evolución histórica

Antes de Go 1.11, todo el código Go **debía** vivir dentro de `$GOPATH/src`.
Esto era incómodo: si tu GOPATH era `~/go`, tu proyecto debía estar en
`~/go/src/github.com/user/project`.

```bash
# Modelo GOPATH (legacy, pre-Go 1.11):
# ~/go/
# └── src/
#     └── github.com/
#         └── user/
#             └── project/       ← tu proyecto aquí obligatoriamente
#                 ├── main.go
#                 └── utils.go

# Modelo Go Modules (actual, desde Go 1.11+, por defecto desde 1.16):
# ~/projects/               ← donde quieras
# └── myproject/
#     ├── go.mod             ← define el módulo y dependencias
#     ├── go.sum             ← checksums de dependencias
#     ├── main.go
#     └── internal/
#         └── utils.go
```

```bash
# Verificar que estás usando módulos (debería ser el caso):
go env GO111MODULE
# "" o "on"  → módulos habilitados (comportamiento por defecto)
# "off"      → modo GOPATH legacy (evitar)
# "auto"     → módulos si hay go.mod en el directorio o ancestros

# No debería ser necesario tocar GO111MODULE en versiones modernas.
# Si encuentras un tutorial que dice export GO111MODULE=on, es antiguo.
```

---

## 15. Referencia rápida de subcomandos

| Subcomando | Uso | Ejemplo |
|------------|-----|---------|
| `go build` | Compilar paquete | `go build -o app ./cmd/server` |
| `go run` | Compilar y ejecutar | `go run main.go` |
| `go install` | Compilar e instalar en `$GOPATH/bin` | `go install tool@latest` |
| `go test` | Ejecutar tests | `go test -v -race ./...` |
| `go fmt` | Formatear código | `go fmt ./...` |
| `go vet` | Análisis estático | `go vet ./...` |
| `go doc` | Ver documentación | `go doc fmt.Println` |
| `go get` | Agregar/actualizar dependencia | `go get pkg@v1.2.3` |
| `go mod init` | Inicializar módulo | `go mod init example.com/app` |
| `go mod tidy` | Limpiar dependencias | `go mod tidy` |
| `go env` | Variables de entorno | `go env GOPATH` |
| `go tool` | Herramientas internas | `go tool pprof` |
| `go generate` | Ejecutar directivas `//go:generate` | `go generate ./...` |
| `go clean` | Limpiar cache de build | `go clean -cache` |
| `go version` | Mostrar versión | `go version` |
| `go work` | Gestionar workspaces multi-módulo | `go work init ./mod1 ./mod2` |

---

## 16. Errores comunes

| Error | Causa | Solución |
|-------|-------|----------|
| `go: command not found` | `/usr/local/go/bin` no está en PATH | Agregar `export PATH=$PATH:/usr/local/go/bin` al shell profile |
| `go: cannot find main module` | No hay `go.mod` en el directorio | Ejecutar `go mod init module/name` |
| `$GOPATH/go.mod exists but should not` | Proyecto dentro de `$GOPATH` con modules | Mover el proyecto fuera de `$GOPATH` o eliminar `go.mod` de GOPATH |
| `package X is not in std` | Falta dependencia | Ejecutar `go get X` o `go mod tidy` |
| `go install: version is required` | Falta `@version` o `@latest` | Usar `go install tool@latest` |
| `cannot find package` en GOPATH | Modo GOPATH legacy | Cambiar a modules: `go mod init` |
| Binario con `ldd` muestra dependencias | CGO habilitado | Compilar con `CGO_ENABLED=0 go build` |
| `go vet` no detecta nada pero hay bugs | `go vet` es conservador | Usar `golangci-lint` para análisis más profundo |
| `go fmt` no modifica nada | Archivos ya formateados (o error de sintaxis) | Verificar que compila: `go build` |
| Binario muy grande (~10MB para hello world) | Go incluye runtime + GC | Usar `-ldflags "-s -w"` para reducir |

---

## 17. Ejercicios

### Ejercicio 1 — Instalación y verificación

Verifica tu instalación de Go.

```bash
# 1. Ejecuta estos comandos y anota la salida:
go version
go env GOROOT
go env GOPATH
go env GOOS
go env GOARCH
```

**Predicción**: ¿Qué valor tendrá `GOARCH` en tu máquina? ¿Y `GOOS`?

<details>
<summary>Ver solución</summary>

En una máquina Linux x86_64:

```
go version go1.24.1 linux/amd64
GOROOT=/usr/local/go
GOPATH=/home/user/go
GOOS=linux
GOARCH=amd64
```

`GOOS` es el sistema operativo (`linux`, `darwin`, `windows`).
`GOARCH` es la arquitectura del procesador (`amd64` para x86_64,
`arm64` para ARM de 64 bits). Estos valores determinan para qué
plataforma compila Go por defecto.
</details>

---

### Ejercicio 2 — Primer programa

Crea un programa Hello World y compílalo de tres formas diferentes.

```bash
# 1. Crear el proyecto:
mkdir -p /tmp/go-ex2 && cd /tmp/go-ex2
go mod init example.com/ex2

# 2. Crear main.go con un Hello World

# 3. Compilar y ejecutar con go run

# 4. Compilar con go build y ejecutar el binario

# 5. Compilar con nombre personalizado: go build -o greet
```

**Predicción**: ¿Cuánto pesará el binario? ¿Más o menos que un programa
equivalente en C compilado con `gcc -static`?

<details>
<summary>Ver solución</summary>

```go
// main.go
package main

import "fmt"

func main() {
    fmt.Println("Hello, Go!")
}
```

```bash
# Con go run:
go run main.go
# Hello, Go!

# Con go build:
go build
ls -lh ex2
# ~1.8MB (incluye runtime + GC + fmt)
./ex2
# Hello, Go!

# Con nombre personalizado:
go build -o greet
./greet
# Hello, Go!
```

El binario Go es **mucho más grande** que un Hello World en C (~15KB
estático) porque incluye el runtime completo: garbage collector,
scheduler de goroutines, y el paquete `fmt` (que usa reflection).
Esto es normal y aceptable: el overhead fijo se amortiza rápidamente
en programas reales.

Para reducir tamaño: `go build -ldflags "-s -w" -o greet` elimina
información de depuración (~30% menos).
</details>

---

### Ejercicio 3 — go fmt y go vet

Crea un archivo con errores de formato y problemas que `go vet` detecte.

```go
// main.go — escríbelo exactamente así (sin formatear)
package main
import "fmt"
func main(){
name:="Alice"
age:=30
fmt.Printf("Name: %d, Age: %s\n",name,age)
}
```

```bash
# 1. Ejecuta go fmt y observa qué cambia
# 2. Ejecuta go vet y observa qué reporta
# 3. Corrige los errores que go vet señala
```

**Predicción**: ¿Qué cambiará `go fmt`? ¿Cuántos errores detectará `go vet`?

<details>
<summary>Ver solución</summary>

`go fmt` corrige el formato:

```go
package main

import "fmt"

func main() {
	name := "Alice"
	age := 30
	fmt.Printf("Name: %d, Age: %s\n", name, age)
}
```

Cambios: añade espacios después de comas, indentación con tabs, espacio
antes de `{`, separación entre `import` y `func`.

`go vet` detecta **2 errores**:

```
./main.go:7:2: fmt.Printf format %d has arg name of wrong type string
./main.go:7:2: fmt.Printf format %s has arg age of wrong type int
```

`%d` es para enteros pero `name` es string. `%s` es para strings pero
`age` es int. Versión corregida:

```go
fmt.Printf("Name: %s, Age: %d\n", name, age)
```

Nota: el programa **compila** sin error con los formatos incorrectos.
`go vet` detecta lo que el compilador no puede.
</details>

---

### Ejercicio 4 — Binario estático vs dinámico

Compara un binario Go compilado con y sin CGO.

```bash
# 1. Crear un programa que use net/http:
mkdir -p /tmp/go-ex4 && cd /tmp/go-ex4
go mod init example.com/ex4

# main.go:
# package main
# import ("fmt"; "net/http")
# func main() {
#     resp, err := http.Get("https://httpbin.org/ip")
#     if err != nil { fmt.Println(err); return }
#     defer resp.Body.Close()
#     fmt.Println("Status:", resp.Status)
# }

# 2. Compilar con CGO habilitado (por defecto):
go build -o app-dynamic
file app-dynamic
ldd app-dynamic

# 3. Compilar con CGO deshabilitado:
CGO_ENABLED=0 go build -o app-static
file app-static
ldd app-static

# 4. Comparar tamaños
```

**Predicción**: ¿Cuál será más grande? ¿Qué dirá `ldd` para cada uno?

<details>
<summary>Ver solución</summary>

```bash
# Dinámico (CGO_ENABLED=1):
file app-dynamic
# ELF 64-bit LSB executable, x86-64, dynamically linked,
# interpreter /lib64/ld-linux-x86-64.so.2, ...

ldd app-dynamic
# linux-vdso.so.1
# libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6
# libpthread.so.0 => ...
# ...

# Estático (CGO_ENABLED=0):
file app-static
# ELF 64-bit LSB executable, x86-64, statically linked, ...

ldd app-static
# not a dynamic executable
```

El binario estático es ligeramente **más grande** (~1-2MB más) porque
incluye las implementaciones Go puras de DNS y TLS en vez de usar las
de la libc del sistema. A cambio, es completamente autocontenido y
funciona en cualquier Linux (incluso en `FROM scratch` de Docker).

Para SysAdmin/DevOps, el binario estático es casi siempre preferible:
un solo archivo que se copia al servidor o al contenedor, sin
preocuparse de versiones de glibc.
</details>

---

### Ejercicio 5 — go install: herramientas del ecosistema

Instala una herramienta Go y úsala.

```bash
# 1. Instala goimports (organiza imports automáticamente):
go install golang.org/x/tools/cmd/goimports@latest

# 2. Verifica que se instaló:
which goimports
goimports --help

# 3. Crea un archivo que use fmt y strings pero no importe strings:
# package main
# func main() {
#     fmt.Println(strings.ToUpper("hello"))
# }

# 4. Ejecuta goimports sobre el archivo
# 5. Observa qué cambió
```

**Predicción**: ¿Dónde se instalará el binario `goimports`?
¿Qué hará `goimports` con el archivo que no importa `strings`?

<details>
<summary>Ver solución</summary>

```bash
which goimports
# /home/user/go/bin/goimports
# Se instala en $GOPATH/bin (~/go/bin por defecto)
```

El archivo original no compila porque usa `fmt` y `strings` sin
importarlos. `goimports` añade automáticamente los imports necesarios:

```go
// Antes:
package main

func main() {
    fmt.Println(strings.ToUpper("hello"))
}

// Después de goimports:
package main

import (
    "fmt"
    "strings"
)

func main() {
    fmt.Println(strings.ToUpper("hello"))
}
```

`goimports` hace lo mismo que `gofmt` (formatea) **más** gestiona
imports automáticamente: añade los que faltan y elimina los que sobran.
Por eso muchos desarrolladores Go configuran su editor para ejecutar
`goimports` en vez de `gofmt` al guardar.
</details>

---

### Ejercicio 6 — go doc: leer documentación sin internet

Usa `go doc` para explorar la biblioteca estándar.

```bash
# 1. ¿Qué hace el paquete os?
go doc os

# 2. ¿Qué firma tiene os.ReadFile?
go doc os.ReadFile

# 3. ¿Qué métodos tiene os.File?
go doc os.File

# 4. ¿Cómo se usa fmt.Fprintf?
go doc fmt.Fprintf

# 5. Busca la función que compara strings ignorando mayúsculas
go doc strings.EqualFold
```

**Predicción**: `os.ReadFile` ¿devuelve un `string` o `[]byte`?
¿Por qué?

<details>
<summary>Ver solución</summary>

```bash
go doc os.ReadFile
# func ReadFile(name string) ([]byte, error)
#     ReadFile reads the named file and returns the contents.
```

Devuelve `[]byte` (slice de bytes), no `string`. En Go, `string` es
inmutable y `[]byte` es mutable. `ReadFile` devuelve bytes porque:

1. No todo archivo contiene texto válido UTF-8
2. `[]byte` permite modificar el contenido sin copiar
3. La conversión a string es trivial: `string(data)`

```go
data, err := os.ReadFile("config.yaml")
if err != nil {
    log.Fatal(err)
}
text := string(data)  // convertir a string si necesitas
```

`go doc` funciona sin internet porque la documentación se genera
directamente del código fuente instalado localmente. Cada comentario
que precede a una función, tipo o paquete es su documentación.
</details>

---

### Ejercicio 7 — Variables de entorno y go env

Explora y modifica las variables de entorno de Go.

```bash
# 1. Muestra GOPATH, GOROOT, GOBIN, GOPROXY
# 2. ¿Dónde se almacenan los módulos descargados?
go env GOMODCACHE
# 3. ¿Cuánto espacio ocupa la cache de módulos?
du -sh $(go env GOMODCACHE)
# 4. Limpiar la cache:
go clean -modcache
# 5. Verificar que se limpió:
du -sh $(go env GOMODCACHE)
```

**Predicción**: ¿Qué contiene `GOPROXY` por defecto? ¿Para qué sirve?

<details>
<summary>Ver solución</summary>

```bash
go env GOPROXY
# https://proxy.golang.org,direct
```

`GOPROXY` define de dónde se descargan los módulos:

1. **`https://proxy.golang.org`** — Proxy oficial de Google. Cachea
   módulos para mayor velocidad y disponibilidad. También verifica
   checksums contra `sum.golang.org` para detectar tampering.

2. **`,direct`** — Si el proxy no tiene el módulo, se descarga
   directamente del repositorio origen (GitHub, GitLab, etc.).

Para módulos privados (repositorios corporativos), se usa `GOPRIVATE`:

```bash
# Indicar que los módulos de tu empresa no pasen por el proxy:
go env -w GOPRIVATE=github.com/mi-empresa/*

# O usar un proxy privado (Athens, GoProxy.io, etc.)
go env -w GOPROXY=https://mi-proxy.empresa.com,direct
```

La cache de módulos (`GOMODCACHE`) puede crecer significativamente.
`go clean -modcache` la vacía, pero las dependencias se re-descargan
la próxima vez que compiles un proyecto.
</details>

---

### Ejercicio 8 — Tamaño del binario: técnicas de reducción

Compara el tamaño del binario Hello World con diferentes flags.

```bash
cd /tmp/go-ex2  # (el proyecto del ejercicio 2)

# 1. Compilación normal:
go build -o v1 && ls -lh v1

# 2. Sin debug info:
go build -ldflags "-s -w" -o v2 && ls -lh v2

# 3. Estático sin debug:
CGO_ENABLED=0 go build -ldflags "-s -w" -o v3 && ls -lh v3

# 4. Con trimpath:
CGO_ENABLED=0 go build -ldflags "-s -w" -trimpath -o v4 && ls -lh v4

# 5. Comparar todos:
ls -lh v1 v2 v3 v4
```

**Predicción**: ¿Cuánto se reduce con `-ldflags "-s -w"`? ¿Un 10%, 30%, 50%?

<details>
<summary>Ver solución</summary>

```bash
ls -lh v1 v2 v3 v4
# v1   ~1.8M  (normal)
# v2   ~1.2M  (sin debug info)      → ~33% menos
# v3   ~1.2M  (estático sin debug)  → similar a v2
# v4   ~1.2M  (+ trimpath)          → similar a v3
```

`-ldflags "-s -w"` reduce **~30%** eliminando:
- `-s`: tabla de símbolos (symbol table)
- `-w`: información de depuración DWARF

`-trimpath` no reduce tamaño significativamente pero elimina rutas
absolutas del binario (mejora reproducibilidad y seguridad — no expone
la ruta del filesystem del build server).

Para un Hello World, 1.2MB parece mucho. Pero en programas reales el
overhead fijo (~1MB de runtime) es despreciable: una CLI que en C
pesaría 500KB, en Go pesa 3MB — aceptable para un binario que se
despliega una vez y no tiene dependencias.

Si necesitas binarios aún más pequeños, existen herramientas como
`upx` (compresión de ejecutables), pero suelen causar problemas con
antivirus y detección de malware.
</details>

---

### Ejercicio 9 — go test rápido

Crea una función y su test.

```bash
mkdir -p /tmp/go-ex9 && cd /tmp/go-ex9
go mod init example.com/ex9
```

```go
// calc.go
package main

func Double(n int) int {
    return n * 2
}

func main() {}
```

```bash
# 1. Crea calc_test.go con un test para Double
# 2. Ejecuta go test -v
# 3. Añade un caso que falle (ej: Double(0) == 1)
# 4. Observa cómo se reporta el fallo
```

**Predicción**: ¿Cuál es la convención de nombres para archivos de test
en Go? ¿Y para funciones de test?

<details>
<summary>Ver solución</summary>

```go
// calc_test.go
package main

import "testing"

func TestDouble(t *testing.T) {
    if Double(5) != 10 {
        t.Errorf("Double(5) = %d, want 10", Double(5))
    }
    if Double(0) != 0 {
        t.Errorf("Double(0) = %d, want 0", Double(0))
    }
}

func TestDoubleFail(t *testing.T) {
    // Este test falla intencionalmente:
    if Double(0) != 1 {
        t.Errorf("Double(0) = %d, want 1", Double(0))
    }
}
```

```bash
go test -v
# === RUN   TestDouble
# --- PASS: TestDouble (0.00s)
# === RUN   TestDoubleFail
#     calc_test.go:15: Double(0) = 0, want 1
# --- FAIL: TestDoubleFail (0.00s)
# FAIL
# exit status 1
```

Convenciones:
- **Archivos**: `*_test.go` — Go ignora estos archivos en `go build`
  pero los incluye en `go test`
- **Funciones**: `func TestXxx(t *testing.T)` — debe empezar con
  `Test` seguido de una letra mayúscula
- **Paquete**: generalmente el mismo que el código testeado (misma
  carpeta), pero puede usar `package main_test` para tests de caja negra
</details>

---

### Ejercicio 10 — Script de verificación de entorno

Escribe un programa Go que verifique que el entorno de desarrollo está
correctamente configurado (simulando lo que haría un script de setup).

```go
// Debe verificar:
// 1. Que go version es >= 1.22
// 2. Que $GOPATH/bin está en el PATH
// 3. Que gofmt existe y es ejecutable
// 4. Imprimir GOOS, GOARCH, CGO_ENABLED
// 5. Reportar el estado de cada verificación con ✓ o ✗
```

**Predicción**: ¿Cómo obtienes variables de entorno en Go? ¿Con
`os.Getenv` o con `go env` desde dentro del programa?

<details>
<summary>Ver solución</summary>

```go
package main

import (
	"fmt"
	"os"
	"os/exec"
	"runtime"
	"strings"
)

func main() {
	fmt.Println("=== Go Environment Check ===\n")

	// 1. Versión de Go
	version := runtime.Version() // "go1.24.1"
	fmt.Printf("[✓] Go version: %s\n", version)

	// 2. GOPATH/bin en PATH
	gopath := os.Getenv("GOPATH")
	if gopath == "" {
		gopath = os.Getenv("HOME") + "/go"
	}
	gopathBin := gopath + "/bin"
	path := os.Getenv("PATH")
	if strings.Contains(path, gopathBin) {
		fmt.Printf("[✓] %s is in PATH\n", gopathBin)
	} else {
		fmt.Printf("[✗] %s is NOT in PATH\n", gopathBin)
	}

	// 3. gofmt disponible
	gofmtPath, err := exec.LookPath("gofmt")
	if err != nil {
		fmt.Println("[✗] gofmt not found")
	} else {
		fmt.Printf("[✓] gofmt found at %s\n", gofmtPath)
	}

	// 4. Variables de entorno relevantes
	fmt.Printf("\n--- Platform ---\n")
	fmt.Printf("GOOS:        %s\n", runtime.GOOS)
	fmt.Printf("GOARCH:      %s\n", runtime.GOARCH)
	fmt.Printf("NumCPU:      %d\n", runtime.NumCPU())
	fmt.Printf("GOPATH:      %s\n", gopath)

	// CGO_ENABLED no está en runtime, se obtiene del entorno
	cgo := os.Getenv("CGO_ENABLED")
	if cgo == "" {
		cgo = "(default, likely 1)"
	}
	fmt.Printf("CGO_ENABLED: %s\n", cgo)
}
```

```bash
go run main.go
# === Go Environment Check ===
#
# [✓] Go version: go1.24.1
# [✓] /home/user/go/bin is in PATH
# [✓] gofmt found at /usr/local/go/bin/gofmt
#
# --- Platform ---
# GOOS:        linux
# GOARCH:      amd64
# NumCPU:      8
# GOPATH:      /home/user/go
# CGO_ENABLED: (default, likely 1)
```

**Variables de entorno en Go**:
- `os.Getenv("VAR")` — lee variables del shell (PATH, HOME, etc.)
- `runtime.GOOS`, `runtime.GOARCH` — constantes del compilador
  (determinadas en tiempo de compilación, no de ejecución)
- `runtime.Version()` — versión de Go usada para compilar

No se usa `go env` desde dentro del programa. Las variables como GOOS
y GOARCH son constantes de compilación accesibles vía el paquete
`runtime`.
</details>
