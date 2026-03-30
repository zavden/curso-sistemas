# T01 — Compilación estática

## 1. ¿Qué es un binario estático?

Un binario estático contiene **todo el código que necesita para ejecutarse**
dentro del propio archivo. No depende de bibliotecas compartidas (.so en Linux,
.dll en Windows, .dylib en macOS) que estén instaladas en el sistema.

```
    ┌──────────────────────────────────────────────────────────┐
    │            Binario dinámico vs binario estático           │
    │                                                          │
    │  DINÁMICO (típico en C):                                 │
    │                                                          │
    │  ./myapp ──requires──▶ libc.so.6                         │
    │          ──requires──▶ libpthread.so.0                   │
    │          ──requires──▶ libssl.so.3                       │
    │          ──requires──▶ libz.so.1                         │
    │                                                          │
    │  Si alguna .so no existe o tiene versión incompatible:   │
    │  error while loading shared libraries: libssl.so.3       │
    │                                                          │
    │  ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─    │
    │                                                          │
    │  ESTÁTICO (Go por defecto):                              │
    │                                                          │
    │  ./myapp ──contains──▶ todo el runtime de Go             │
    │          ──contains──▶ toda la stdlib usada              │
    │          ──contains──▶ todas las dependencias            │
    │                                                          │
    │  Copiar el archivo = deploy completo.                    │
    │  Funciona en cualquier Linux (mismo GOOS/GOARCH).        │
    └──────────────────────────────────────────────────────────┘
```

### 1.1 ¿Por qué esto importa para SysAdmin/DevOps?

```bash
# Problemas clásicos con binarios dinámicos:

# 1. "Dependency hell" — la biblioteca no existe o tiene otra versión:
$ ./monitoring-agent
error while loading shared libraries: libssl.so.1.1: cannot open shared object file

# 2. Distribuir a 200 servidores con distros diferentes:
#    - Ubuntu 22.04 tiene glibc 2.35
#    - CentOS 7 tiene glibc 2.17
#    - Alpine NO tiene glibc (usa musl)
#    → El binario dinámico no es portable entre ellos

# 3. Contenedores "FROM scratch":
#    - Imagen Docker de 0 bytes + tu binario estático = imagen mínima
#    - Sin shell, sin package manager, sin surface de ataque

# Con Go estático:
$ scp myapp servidor:/usr/local/bin/
$ ssh servidor 'myapp --version'
# Funciona. Sin instalar nada. Sin apt/yum. Sin dependencias.
```

---

## 2. Cómo Go compila: el linker de Go vs cgo

Go tiene **su propio linker** que no depende del linker del sistema (ld).
Cuando compilas código Go puro (sin cgo), el resultado es estático por defecto.

```
    ┌────────────────────────────────────────────────────────────┐
    │                 Pipeline de compilación de Go               │
    │                                                            │
    │  .go files                                                 │
    │      │                                                     │
    │      ▼                                                     │
    │  ┌──────────┐                                              │
    │  │ go/parser│  Parse → AST                                 │
    │  └────┬─────┘                                              │
    │       ▼                                                    │
    │  ┌──────────┐                                              │
    │  │ go/types │  Type check                                  │
    │  └────┬─────┘                                              │
    │       ▼                                                    │
    │  ┌──────────┐                                              │
    │  │   SSA    │  Intermediate representation                 │
    │  └────┬─────┘                                              │
    │       ▼                                                    │
    │  ┌──────────┐                                              │
    │  │ codegen  │  Generate machine code (.o files)            │
    │  └────┬─────┘                                              │
    │       ▼                                                    │
    │  ┌──────────────────────────────────────────┐              │
    │  │              LINKER                       │              │
    │  │                                          │              │
    │  │  CGO_ENABLED=0 → Go linker (estático)    │              │
    │  │  CGO_ENABLED=1 → Go + system linker (ld) │              │
    │  │                  puede ser dinámico       │              │
    │  └────┬─────────────────────────────────────┘              │
    │       ▼                                                    │
    │  binario ejecutable                                        │
    └────────────────────────────────────────────────────────────┘
```

### 2.1 CGO_ENABLED — el interruptor clave

```bash
# CGO_ENABLED controla si Go puede llamar código C.

# Ver el valor actual:
go env CGO_ENABLED
# En la mayoría de sistemas: 1 (habilitado)

# ¿Por qué está habilitado por defecto?
# Porque algunos paquetes de la stdlib usan C bajo el capó:
#   - net (DNS resolver) → usa getaddrinfo() de libc
#   - os/user → usa getpwnam() de libc
#   - plugin → usa dlopen() de libc
#
# Con CGO_ENABLED=1 (y esos paquetes), el binario enlaza glibc dinámicamente.

# ¿Qué pasa con CGO_ENABLED=0?
# Go usa implementaciones PURAS EN GO de esos paquetes:
#   - net → resolver DNS nativo de Go (no usa libc)
#   - os/user → lee /etc/passwd directamente
#   - plugin → no disponible
#
# El resultado es un binario 100% estático.
```

---

## 3. Compilar binarios estáticos

### 3.1 Método simple: CGO_ENABLED=0

```bash
# La forma más directa y recomendada:
CGO_ENABLED=0 go build -o myapp ./cmd/myapp

# Verificar que el binario es estático:
file myapp
# myapp: ELF 64-bit LSB executable, x86-64, version 1 (SYSV),
# statically linked, Go BuildID=xxx, not stripped

ldd myapp
# not a dynamic executable    ← ✓ estático

# Comparar con un binario dinámico (CGO_ENABLED=1):
CGO_ENABLED=1 go build -o myapp-dynamic ./cmd/myapp
file myapp-dynamic
# myapp-dynamic: ELF 64-bit LSB executable, x86-64, version 1 (SYSV),
# dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, ...

ldd myapp-dynamic
# linux-vdso.so.1 (0x00007fff...)
# libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f...)
# libpthread.so.0 => /lib/x86_64-linux-gnu/libpthread.so.0 (0x00007f...)
# /lib64/ld-linux-x86-64.so.2 (0x00007f...)
```

### 3.2 Método con flags adicionales de producción

```bash
# Build de producción completo:
CGO_ENABLED=0 go build \
    -trimpath \
    -ldflags="-s -w" \
    -o myapp \
    ./cmd/myapp

# Desglose de cada flag:
#
# CGO_ENABLED=0   → binario estático (sin libc)
# -trimpath        → elimina rutas locales del binario
#                    (seguridad: no revela /home/usuario/src/...)
# -ldflags="-s -w" → strip debug info:
#                    -s = sin symbol table
#                    -w = sin DWARF debug info
#                    Reduce tamaño ~30%
# -o myapp         → nombre del binario de salida
# ./cmd/myapp      → paquete a compilar
```

### 3.3 Comparar tamaños

```bash
# Sin optimizaciones:
CGO_ENABLED=0 go build -o myapp-debug ./cmd/myapp
ls -lh myapp-debug
# -rwxr-xr-x 1 user user 12M myapp-debug

# Con strip:
CGO_ENABLED=0 go build -ldflags="-s -w" -o myapp-stripped ./cmd/myapp
ls -lh myapp-stripped
# -rwxr-xr-x 1 user user 8.2M myapp-stripped

# Con UPX (compresión de ejecutables — opcional):
upx --best myapp-stripped -o myapp-upx
ls -lh myapp-upx
# -rwxr-xr-x 1 user user 3.1M myapp-upx

# ⚠️ UPX tiene trade-offs:
# ✓ Reduce tamaño drásticamente (~60-70%)
# ✗ Mayor tiempo de inicio (descompresión)
# ✗ Mayor uso de RAM al inicio
# ✗ Algunos antivirus lo marcan como sospechoso
# ✗ No compatible con todos los formatos (problemas en macOS/ARM)
# Recomendación: usar UPX solo para CLIs de distribución amplia,
# NO para servicios de producción
```

---

## 4. El resolver DNS: net package y CGO

Uno de los aspectos más importantes de la compilación estática es el
**resolver DNS**. Este es el punto donde la decisión CGO_ENABLED=0 vs
CGO_ENABLED=1 tiene más impacto práctico.

```
    ┌────────────────────────────────────────────────────────────┐
    │               DNS Resolution en Go                         │
    │                                                            │
    │  CGO_ENABLED=1 (default en Linux):                         │
    │  ┌─────────┐   ┌─────────────┐   ┌─────────────────────┐  │
    │  │ net.Dial │──▶│ cgo resolver│──▶│ glibc getaddrinfo() │  │
    │  └─────────┘   └─────────────┘   └──────────┬──────────┘  │
    │                                              │             │
    │                                    Respeta:  │             │
    │                                    - /etc/nsswitch.conf    │
    │                                    - /etc/resolv.conf      │
    │                                    - mDNS (Avahi)          │
    │                                    - LDAP/NIS              │
    │                                                            │
    │  ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─    │
    │                                                            │
    │  CGO_ENABLED=0 (o GODEBUG=netdns=go):                      │
    │  ┌─────────┐   ┌─────────────┐   ┌─────────────────────┐  │
    │  │ net.Dial │──▶│  Go resolver│──▶│ Lee /etc/resolv.conf│  │
    │  └─────────┘   └─────────────┘   │ DNS directo (UDP/TCP)│  │
    │                                   └─────────────────────┘  │
    │                                                            │
    │                                    Respeta:                │
    │                                    - /etc/resolv.conf ✓    │
    │                                    - /etc/hosts ✓          │
    │                                    - nsswitch.conf parcial │
    │                                    - mDNS ✗                │
    │                                    - LDAP/NIS ✗            │
    └────────────────────────────────────────────────────────────┘
```

### 4.1 ¿Cuándo importa la diferencia?

```bash
# En el 99% de los casos: NO importa.
# El resolver puro de Go funciona perfectamente para:
#   ✓ DNS estándar (A, AAAA, CNAME, MX, TXT, SRV, NS)
#   ✓ /etc/hosts
#   ✓ /etc/resolv.conf (nameserver, search, options)
#   ✓ Kubernetes DNS (CoreDNS)
#   ✓ Docker DNS
#   ✓ Consul DNS
#   ✓ AWS Route53 / GCP Cloud DNS / Azure DNS

# Casos donde SÍ importa (raros):
#   ✗ mDNS / Avahi (resolución .local en redes locales)
#   ✗ LDAP-based name resolution (nsswitch: ldap)
#   ✗ NIS/NIS+ (entornos legacy)
#   ✗ SSSD integration (FreeIPA/AD)
#   ✗ Configuraciones nsswitch.conf exóticas

# Si necesitas compatibilidad total con nsswitch:
GODEBUG=netdns=cgo ./myapp     # forzar resolver cgo en runtime
# Pero entonces el binario DEBE estar compilado con CGO_ENABLED=1
```

### 4.2 Controlar el resolver en runtime

```go
// Puedes elegir el resolver por programa, no solo por variable de entorno.

import "net"

func init() {
    // Forzar resolver puro Go (incluso si CGO está habilitado):
    net.DefaultResolver = &net.Resolver{
        PreferGo: true,
    }
}

// O usar un resolver DNS personalizado:
func customResolver() *net.Resolver {
    return &net.Resolver{
        PreferGo: true,
        Dial: func(ctx context.Context, network, address string) (net.Conn, error) {
            // Usar un DNS específico (ej: Cloudflare):
            d := net.Dialer{Timeout: 5 * time.Second}
            return d.DialContext(ctx, "udp", "1.1.1.1:53")
        },
    }
}
```

```bash
# Variables de entorno para controlar DNS:

# Forzar resolver Go:
GODEBUG=netdns=go ./myapp

# Forzar resolver cgo:
GODEBUG=netdns=cgo ./myapp

# Ver qué resolver se está usando (verbose):
GODEBUG=netdns=go+2 ./myapp 2>&1 | grep "dns"
# go package net: using Go's DNS resolver

GODEBUG=netdns=cgo+2 ./myapp 2>&1 | grep "dns"
# go package net: using cgo DNS resolver
```

---

## 5. os/user y compilación estática

El paquete `os/user` es el otro paquete de la stdlib que se comporta
diferente con CGO_ENABLED=0.

```go
// Con CGO_ENABLED=1: usa getpwnam()/getgrnam() de libc
// Con CGO_ENABLED=0: lee directamente /etc/passwd y /etc/group

package main

import (
    "fmt"
    "os/user"
)

func main() {
    // Obtener usuario actual:
    u, err := user.Current()
    if err != nil {
        fmt.Println("Error:", err)
        return
    }
    fmt.Printf("User: %s (UID: %s, GID: %s)\n", u.Username, u.Uid, u.Gid)
    fmt.Printf("Home: %s\n", u.HomeDir)

    // Buscar usuario por nombre:
    admin, err := user.Lookup("root")
    if err != nil {
        fmt.Println("Error:", err)
        return
    }
    fmt.Printf("Root UID: %s\n", admin.Uid)

    // Buscar grupo:
    g, err := user.LookupGroup("docker")
    if err != nil {
        fmt.Println("docker group not found:", err)
        return
    }
    fmt.Printf("Docker GID: %s\n", g.Gid)
}
```

```bash
# Con CGO_ENABLED=0, os/user funciona leyendo /etc/passwd directamente.
# Esto es suficiente en el 99% de los casos.

# NO funciona con CGO_ENABLED=0:
# - Usuarios LDAP (no están en /etc/passwd)
# - Usuarios NIS/SSSD
# - Usuarios de Active Directory (vía SSSD/Winbind)
# - Resolución que depende de nss-ldap, nss-sss, etc.

# Para servidores que usan identidad centralizada (FreeIPA, AD):
# Opción 1: CGO_ENABLED=1 (pierdes binario estático)
# Opción 2: No usar os/user — obtener UID/GID de otra forma
# Opción 3: Configurar sssd para cachear en /etc/passwd
```

---

## 6. Cuándo necesitas cgo (y cómo compilar estático con cgo)

A veces **necesitas** cgo: tu código llama a bibliotecas C directamente
(SQLite, OpenSSL, libpcap, etc.) o usas paquetes Go que wrappean C.

### 6.1 Paquetes comunes que requieren cgo

```bash
# Paquetes que usan cgo internamente:

# Base de datos:
# - github.com/mattn/go-sqlite3      → wrappea libsqlite3
# - github.com/lib/pq                → NO necesita cgo (pure Go ✓)
# - github.com/jackc/pgx/v5          → NO necesita cgo (pure Go ✓)
# - github.com/go-sql-driver/mysql   → NO necesita cgo (pure Go ✓)

# Seguridad/Crypto:
# - crypto/... (stdlib)              → NO necesita cgo (pure Go ✓)
# - github.com/spacemonkeygo/openssl → wrappea libssl (necesita cgo)

# Red/Captura:
# - github.com/google/gopacket       → wrappea libpcap (necesita cgo)

# Sistema:
# - github.com/shirou/gopsutil       → parcialmente (pure Go en Linux ✓)
# - github.com/tklauser/go-sysconf   → wrappea sysconf() (cgo en Linux)

# GUI:
# - fyne.io/fyne                     → necesita cgo
# - github.com/therecipe/qt          → necesita cgo

# La regla: si un paquete Go necesita .h headers de C para compilar,
# necesita cgo.
```

### 6.2 Alternativas pure-Go

```bash
# Antes de usar cgo, busca alternativas pure-Go:

# SQLite:
# ✗ github.com/mattn/go-sqlite3     → cgo (wrappea libsqlite3 en C)
# ✓ modernc.org/sqlite              → pure Go (compilación de C a Go)
# ✓ github.com/cznic/sqlite         → pure Go (transpiled)
# ✓ github.com/ncruces/go-sqlite3   → pure Go vía Wasm

# Imágen:
# ✗ No hay alternativa pure-Go para libvips/imagemagick
# ✓ stdlib image/png, image/jpeg    → pure Go (básico pero funcional)
# ✓ github.com/disintegration/imaging → pure Go (resize, crop, etc.)

# Crypto (TLS):
# ✓ crypto/tls (stdlib)             → pure Go, FIPS-capable desde Go 1.24
# ✗ OpenSSL bindings                → solo si necesitas hardware-specific

# DNS avanzado:
# ✓ github.com/miekg/dns            → pure Go, DNS completo
```

### 6.3 Compilación estática CON cgo

```bash
# Si necesitas cgo pero también quieres un binario estático,
# debes indicarle al linker de C que enlace estáticamente:

# Método 1: -extldflags "-static"
CGO_ENABLED=1 go build \
    -ldflags='-s -w -extldflags "-static"' \
    -o myapp ./cmd/myapp

# Método 2: con tags netgo (fuerza resolver Go incluso con cgo):
CGO_ENABLED=1 go build \
    -tags netgo \
    -ldflags='-s -w -extldflags "-static"' \
    -o myapp ./cmd/myapp

# Verificar:
file myapp
# myapp: ELF 64-bit LSB executable, x86-64, statically linked, ...
ldd myapp
# not a dynamic executable

# ⚠️ Requisito: necesitas las versiones estáticas de las bibliotecas C:
# En Debian/Ubuntu:
sudo apt install libc6-dev        # glibc headers + static lib
sudo apt install libsqlite3-dev   # si usas SQLite

# En Fedora/RHEL:
sudo dnf install glibc-static
sudo dnf install sqlite-devel

# En Alpine (musl — más fácil para static):
apk add musl-dev sqlite-dev
```

### 6.4 Alpine + musl = el camino fácil para estático con cgo

```bash
# Alpine Linux usa musl en lugar de glibc.
# musl está diseñado para linkeo estático desde el principio.

# ¿Por qué musl es mejor que glibc para estático?
#
# glibc estático tiene problemas:
# - NSS (Name Service Switch) usa dlopen() internamente
# - Incluso con -static, glibc puede intentar cargar .so en runtime
# - Warnings: "Using 'getaddrinfo' in statically linked applications..."
#
# musl no tiene estos problemas:
# - No usa dlopen() para NSS
# - El binario estático es realmente 100% autocontenido
# - Sin warnings, sin sorpresas

# Dockerfile para compilar con musl:
# FROM golang:1.23-alpine AS builder
# RUN apk add --no-cache gcc musl-dev
# WORKDIR /app
# COPY go.mod go.sum ./
# RUN go mod download
# COPY . .
# RUN CGO_ENABLED=1 go build \
#     -ldflags='-s -w -extldflags "-static"' \
#     -o /app/myapp ./cmd/myapp
#
# FROM scratch
# COPY --from=builder /app/myapp /myapp
# ENTRYPOINT ["/myapp"]
```

---

## 7. Imágenes Docker mínimas con binarios estáticos

Esta es una de las **killer features** de Go para SysAdmin/DevOps:
compilar un binario estático y meterlo en una imagen Docker de
prácticamente 0 bytes.

### 7.1 FROM scratch — la imagen vacía

```dockerfile
# Dockerfile (multi-stage build)

# Stage 1: compilar
FROM golang:1.23-alpine AS builder
WORKDIR /app
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN CGO_ENABLED=0 go build \
    -trimpath \
    -ldflags="-s -w" \
    -o /app/myapp ./cmd/myapp

# Stage 2: imagen final
FROM scratch
COPY --from=builder /app/myapp /myapp
ENTRYPOINT ["/myapp"]
```

```bash
# Construir y ver el tamaño:
docker build -t myapp:static .
docker images myapp:static
# REPOSITORY   TAG      IMAGE ID       SIZE
# myapp        static   abc123def456   8.2MB

# Comparar con una imagen basada en distro:
# golang:1.23        → ~820MB
# golang:1.23-alpine → ~260MB
# ubuntu:22.04       → ~77MB
# alpine:3.19        → ~7MB
# scratch            → 0B (+ tu binario ≈ 8MB)
#
# 8MB vs 820MB = 99% de reducción
```

### 7.2 ¿Qué incluye FROM scratch?

```bash
# FROM scratch literalmente no tiene NADA:
# - Sin shell (no puedes hacer docker exec sh)
# - Sin ls, cat, curl, wget
# - Sin /etc/passwd, /etc/group
# - Sin certificados TLS (!)
# - Sin timezone data (!)
# - Sin /tmp directory

# Esto significa que necesitas incluir lo que uses:
```

### 7.3 Certificados TLS en scratch

```dockerfile
# Si tu aplicación hace HTTPS requests (cliente), necesita
# certificados raíz de CAs.

# Opción 1: copiar desde la imagen builder
FROM golang:1.23-alpine AS builder
# ... build steps ...

FROM scratch
COPY --from=builder /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/
COPY --from=builder /app/myapp /myapp
ENTRYPOINT ["/myapp"]

# Opción 2: usar go:embed para incluirlos en el binario
# (ver sección 7.7)

# Opción 3: usar la imagen distroless de Google (incluye certs)
# FROM gcr.io/distroless/static-debian12
# COPY --from=builder /app/myapp /myapp
# ENTRYPOINT ["/myapp"]
```

### 7.4 Timezone data en scratch

```dockerfile
# Si tu aplicación usa time.LoadLocation() (zonas horarias),
# necesita la base de datos de timezones.

FROM golang:1.23-alpine AS builder
RUN apk add --no-cache tzdata
# ... build steps ...

FROM scratch
# Copiar timezone data:
COPY --from=builder /usr/share/zoneinfo /usr/share/zoneinfo
# Copiar certs TLS:
COPY --from=builder /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/
COPY --from=builder /app/myapp /myapp

# Opcional: establecer timezone por defecto:
ENV TZ=UTC
ENTRYPOINT ["/myapp"]
```

```go
// Alternativa: embeder timezone data en el binario.
// Desde Go 1.15, puedes importar time/tzdata:

import _ "time/tzdata"   // embede timezone database (~450KB)

// Ahora time.LoadLocation("America/New_York") funciona
// incluso sin /usr/share/zoneinfo en el sistema.
```

### 7.5 Usuario no-root en scratch

```dockerfile
# Por seguridad, tu contenedor no debería correr como root.
# En scratch no existe /etc/passwd, así que hay que crearlo.

FROM golang:1.23-alpine AS builder
# Crear usuario no-root:
RUN adduser -D -u 10001 appuser
# ... build steps ...

FROM scratch
# Copiar passwd para que el contenedor conozca al usuario:
COPY --from=builder /etc/passwd /etc/passwd
COPY --from=builder /etc/group /etc/group
COPY --from=builder /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/
COPY --from=builder /app/myapp /myapp

USER appuser
ENTRYPOINT ["/myapp"]
```

### 7.6 Dockerfile de producción completo

```dockerfile
# Dockerfile de producción para un servicio Go

# ============================================================
# Stage 1: Build
# ============================================================
FROM golang:1.23-alpine AS builder

# Instalar certificados y timezone data para copiar al final
RUN apk add --no-cache ca-certificates tzdata

# Crear usuario no-root
RUN adduser -D -u 10001 -g "" appuser

WORKDIR /app

# Copiar dependencias primero (cache de Docker layers)
COPY go.mod go.sum ./
RUN go mod download && go mod verify

# Copiar código fuente
COPY . .

# Build args para inyectar versión
ARG VERSION=dev
ARG COMMIT=unknown

# Compilar binario estático
RUN CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build \
    -trimpath \
    -ldflags="-s -w \
        -X main.version=${VERSION} \
        -X main.commit=${COMMIT} \
        -X main.buildTime=$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
    -o /app/server ./cmd/server

# ============================================================
# Stage 2: Final (scratch)
# ============================================================
FROM scratch

# Metadatos OCI
LABEL org.opencontainers.image.source="https://github.com/org/repo"
LABEL org.opencontainers.image.description="My Go Service"

# Copiar artefactos del builder
COPY --from=builder /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/
COPY --from=builder /usr/share/zoneinfo /usr/share/zoneinfo
COPY --from=builder /etc/passwd /etc/passwd
COPY --from=builder /etc/group /etc/group

# Copiar binario
COPY --from=builder /app/server /server

# Ejecutar como no-root
USER appuser:appuser

# Puerto por defecto
EXPOSE 8080

# Health check (nota: no hay curl en scratch, usa el binario)
# HEALTHCHECK se define en docker-compose, no aquí

ENTRYPOINT ["/server"]
```

```bash
# Construir con versión inyectada:
docker build \
    --build-arg VERSION=$(git describe --tags --always) \
    --build-arg COMMIT=$(git rev-parse --short HEAD) \
    -t myservice:latest .

# Resultado:
docker images myservice:latest
# REPOSITORY   TAG      SIZE
# myservice    latest   12MB    ← todo incluido

# Comparar con Python/Node equivalente:
# python:3.12-slim  → ~150MB + tu código
# node:20-slim      → ~180MB + node_modules
# Go scratch        → ~12MB total
```

### 7.7 Alternativa a scratch: distroless

```dockerfile
# Google distroless: imagen mínima pero con más utilidades que scratch.
# static-debian12 incluye: ca-certs, tzdata, /etc/passwd con nobody

FROM golang:1.23 AS builder
WORKDIR /app
COPY . .
RUN CGO_ENABLED=0 go build -trimpath -ldflags="-s -w" -o /app/server ./cmd/server

FROM gcr.io/distroless/static-debian12
COPY --from=builder /app/server /server
USER nonroot:nonroot
ENTRYPOINT ["/server"]

# Ventajas sobre scratch:
# ✓ Incluye ca-certificates
# ✓ Incluye tzdata
# ✓ Incluye /etc/passwd con usuario "nonroot"
# ✓ Actualizaciones de seguridad automáticas del base image
# ✗ Ligeramente más grande (~2MB vs 0)
# ✗ No tiene shell (igual que scratch)
```

```bash
# ¿Cuándo usar cada una?

# scratch:
#   - Control total sobre lo que incluyes
#   - Tamaño mínimo absoluto
#   - Sabes exactamente lo que necesitas

# distroless:
#   - No quieres gestionar certs/tzdata manualmente
#   - Quieres actualizaciones de seguridad del base image
#   - Equipo menos familiarizado con containers mínimos

# alpine:
#   - Necesitas shell para debugging (docker exec sh)
#   - Necesitas instalar paquetes extra (curl, etc.)
#   - En desarrollo / staging
```

---

## 8. -trimpath — eliminar rutas locales

El flag `-trimpath` elimina las rutas absolutas del sistema de ficheros
local del binario compilado. Esto es importante tanto para seguridad
como para reproducibilidad.

```bash
# Sin -trimpath, el binario contiene rutas como:
# /home/developer/projects/myapp/internal/config/config.go
# /home/developer/go/pkg/mod/github.com/gin-gonic/gin@v1.9.1/gin.go

# Estas rutas aparecen en:
# - Stack traces de panic
# - Información de debug (DWARF)
# - runtime.Caller() output

# Con -trimpath:
# internal/config/config.go
# github.com/gin-gonic/gin@v1.9.1/gin.go

# ¿Por qué importa?
# 1. Seguridad: no revela la estructura de directorios del build server
# 2. Reproducibilidad: builds desde diferentes máquinas producen
#    el mismo binario (si se usa la misma versión de Go y deps)
# 3. Stack traces más limpios y legibles
```

```bash
# Demostración:

# Sin trimpath:
go build -o app-notrim ./cmd/myapp
go tool objdump app-notrim | grep -o '/home/[^ ]*' | head -5
# /home/developer/projects/myapp/cmd/myapp/main.go
# /home/developer/projects/myapp/internal/server/server.go

# Con trimpath:
go build -trimpath -o app-trim ./cmd/myapp
go tool objdump app-trim | grep -o '/home/[^ ]*' | head -5
# (sin resultados — las rutas absolutas fueron eliminadas)
```

---

## 9. Verificar las propiedades de un binario Go

Herramientas para inspeccionar binarios Go y verificar que cumplen
con lo esperado.

### 9.1 file — tipo de binario

```bash
# Verificar si es estático o dinámico:
file myapp

# Estático:
# myapp: ELF 64-bit LSB executable, x86-64, version 1 (SYSV),
# statically linked, Go BuildID=..., with debug_info, not stripped

# Dinámico:
# myapp: ELF 64-bit LSB executable, x86-64, version 1 (SYSV),
# dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, ...
```

### 9.2 ldd — dependencias dinámicas

```bash
# Listar shared libraries que necesita:
ldd myapp

# Estático:
# not a dynamic executable

# Dinámico:
# linux-vdso.so.1 (0x00007ffc...)
# libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f...)
# /lib64/ld-linux-x86-64.so.2 (0x00007f...)

# ⚠️ Nunca ejecutes ldd en binarios de fuentes no confiables.
# ldd ejecuta el binario para resolver dependencias.
# Alternativa segura:
readelf -d myapp | grep NEEDED
# Si no hay output → estático
```

### 9.3 go version -m — metadatos del binario

```bash
# Ver la versión de Go y dependencias compiladas en el binario:
go version -m myapp

# Output:
# myapp: go1.23.0
#     path    example.com/myapp
#     mod     example.com/myapp   (devel)
#     dep     github.com/gin-gonic/gin    v1.9.1  h5:abc123...
#     dep     github.com/prometheus/client_golang  v1.18.0 h5:def456...
#     build   -buildmode=exe
#     build   -compiler=gc
#     build   -trimpath=true
#     build   CGO_ENABLED=0
#     build   GOARCH=amd64
#     build   GOOS=linux
#     build   GOAMD64=v1
#     build   -ldflags="-s -w -X main.version=v1.2.3"

# Esto es INCREÍBLEMENTE útil para SysAdmin:
# 1. ¿Qué versión de Go compiló este binario? → go1.23.0
# 2. ¿Es estático? → CGO_ENABLED=0
# 3. ¿Para qué plataforma? → GOOS=linux, GOARCH=amd64
# 4. ¿Qué dependencias tiene? → todas listadas con versión y hash
# 5. ¿Se usó trimpath? → -trimpath=true
```

### 9.4 go tool nm — símbolos del binario

```bash
# Listar símbolos (funciones, variables):
go tool nm myapp | head -20

# Buscar un símbolo específico:
go tool nm myapp | grep 'main\.version'
# 5a4320 D main.version

# Verificar que la versión fue inyectada:
go tool nm myapp | grep -i version
```

### 9.5 readelf y objdump — análisis ELF

```bash
# Ver headers del ELF:
readelf -h myapp

# Ver secciones:
readelf -S myapp

# Ver si tiene información DWARF (debug):
readelf -S myapp | grep debug
# Sin -ldflags="-w": verás .debug_info, .debug_line, etc.
# Con -ldflags="-w": no hay secciones debug

# Tamaño de cada sección:
readelf -S myapp | awk '{print $2, $6}' | sort -k2 -n -r | head

# Desensamblador:
go tool objdump myapp | head -30
go tool objdump -s 'main\.main' myapp   # solo la función main
```

---

## 10. Build cache y compilación incremental

Go mantiene un **cache de compilación** que acelera builds
sucesivos dramáticamente.

```bash
# Ver dónde está el cache:
go env GOCACHE
# /home/user/.cache/go-build (Linux)
# ~/Library/Caches/go-build (macOS)

# Ver tamaño del cache:
du -sh $(go env GOCACHE)
# 1.2G /home/user/.cache/go-build

# Limpiar el cache:
go clean -cache       # limpiar build cache
go clean -testcache   # limpiar cache de tests
go clean -cache -testcache -modcache  # limpiar TODO
```

### 10.1 Cómo funciona el cache

```bash
# Go cachea a nivel de PAQUETE, no de archivo.
# Si cambias un archivo en un paquete, se recompila ese paquete
# y todos los que dependen de él. El resto se usa del cache.

# Primer build (todo desde cero):
$ time go build ./cmd/myapp
real    0m12.340s    ← compila todo

# Segundo build (sin cambios):
$ time go build ./cmd/myapp
real    0m0.120s     ← todo del cache

# Cambiar un archivo en internal/config/:
$ time go build ./cmd/myapp
real    0m1.200s     ← solo recompila config + lo que depende de config

# El cache usa hashes del contenido del archivo, flags de compilación,
# versión de Go, y tags de build para determinar si un resultado
# cacheado es válido.
```

### 10.2 Cache en CI/CD

```yaml
# GitHub Actions — cachear el build y module cache:

# name: Build
# on: push
# jobs:
#   build:
#     runs-on: ubuntu-latest
#     steps:
#       - uses: actions/checkout@v4
#       - uses: actions/setup-go@v5
#         with:
#           go-version: '1.23'
#           cache: true    # ← cachea GOMODCACHE y GOCACHE automáticamente
#       - run: go build ./...
#       - run: go test ./...

# GitLab CI:
# build:
#   image: golang:1.23
#   cache:
#     key: go-${CI_COMMIT_REF_SLUG}
#     paths:
#       - .go-cache/
#   variables:
#     GOMODCACHE: ${CI_PROJECT_DIR}/.go-cache/mod
#     GOCACHE: ${CI_PROJECT_DIR}/.go-cache/build
#   script:
#     - go build ./...
#     - go test ./...
```

### 10.3 Docker layer cache para Go builds

```dockerfile
# Orden de COPY importa para el cache de Docker layers:

# ✗ MAL — cualquier cambio en código invalida el layer de go mod download:
FROM golang:1.23 AS builder
COPY . .
RUN go mod download
RUN go build -o /app ./cmd/myapp

# ✓ BIEN — copiar go.mod/go.sum PRIMERO:
FROM golang:1.23 AS builder
WORKDIR /app

# Layer 1: dependencias (cambia poco)
COPY go.mod go.sum ./
RUN go mod download && go mod verify

# Layer 2: código fuente (cambia mucho)
COPY . .
RUN CGO_ENABLED=0 go build -trimpath -ldflags="-s -w" -o /app/myapp ./cmd/myapp

# Si solo cambiaste código Go (no go.mod), Docker reutiliza
# el layer de go mod download → build mucho más rápido.
```

---

## 11. Build reproducible

Un build es **reproducible** cuando compilar el mismo código con la misma
versión de Go produce el **mismo binario bit a bit**, sin importar
cuándo o dónde se compile.

```bash
# Go busca reproducibilidad por defecto, pero necesitas:

# 1. Fijar la versión de Go (go.mod):
#    go 1.23.0
#    toolchain go1.23.5

# 2. Fijar las dependencias (go.sum existe para esto)

# 3. Usar -trimpath (eliminar rutas del build machine)

# 4. No usar time.Now() ni variables de entorno en el build
#    (ldflags con fecha fija o commit hash es OK)

# 5. Mismo GOOS, GOARCH, GOAMD64

# Build reproducible:
CGO_ENABLED=0 go build \
    -trimpath \
    -ldflags="-s -w -X main.version=v1.2.3 -X main.commit=abc123" \
    -o myapp ./cmd/myapp

# Verificar reproducibilidad:
sha256sum myapp
# a1b2c3d4... myapp

# En otra máquina (misma versión de Go, mismo código):
sha256sum myapp
# a1b2c3d4... myapp    ← mismo hash
```

### 11.1 go tool buildinfo — inspeccionar reproducibilidad

```bash
# Ver la información de build embebida:
go version -m myapp

# Dos builds reproducibles tendrán el mismo Go BuildID:
go tool buildid myapp
# rQm5vLf.../abc123.../xyz789.../final123

# El BuildID cambia si CUALQUIER cosa es diferente:
# - Versión de Go
# - Código fuente
# - Dependencias
# - Flags de compilación
# - GOOS/GOARCH
```

### 11.2 ¿Por qué importa la reproducibilidad?

```bash
# Para SysAdmin/DevOps:

# 1. Auditoría de seguridad:
#    "¿Este binario en producción fue compilado desde este commit?"
#    → Compilar de nuevo + comparar hash → sí/no

# 2. Supply chain security:
#    Si alguien comprometió el servidor de CI, los hashes no coincidirán

# 3. Compliance:
#    Regulaciones (SOC2, HIPAA, PCI-DSS) pueden requerir trazabilidad
#    del build al código fuente

# 4. Rollback confiable:
#    Puedes recompilar cualquier versión anterior y obtener
#    exactamente el mismo binario
```

---

## 12. go build -buildvcs — información de VCS

Desde Go 1.18, `go build` embede automáticamente información del
sistema de control de versiones en el binario.

```bash
# Go embede automáticamente (si estás en un repo git):
# - vcs = git
# - vcs.revision = commit hash completo
# - vcs.time = timestamp del commit
# - vcs.modified = true/false (dirty working tree)

# Ver la información:
go version -m myapp | grep vcs
# build   vcs=git
# build   vcs.revision=abc123def456789...
# build   vcs.time=2024-01-15T10:30:00Z
# build   vcs.modified=false

# Acceder desde código:
```

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

    for _, setting := range info.Settings {
        switch setting.Key {
        case "vcs.revision":
            fmt.Printf("Commit: %s\n", setting.Value)
        case "vcs.time":
            fmt.Printf("Built from commit at: %s\n", setting.Value)
        case "vcs.modified":
            fmt.Printf("Dirty: %s\n", setting.Value)
        }
    }
    // También puedes acceder a:
    fmt.Printf("Go version: %s\n", info.GoVersion)
    fmt.Printf("Module: %s\n", info.Main.Path)
    fmt.Printf("Module version: %s\n", info.Main.Version)
}
```

```bash
# Deshabilitar VCS info (si el build falla por git issues):
go build -buildvcs=false -o myapp ./cmd/myapp

# Esto es útil en CI donde el checkout puede no tener .git completo
# (shallow clone con depth=1)
```

---

## 13. go install — instalar binarios

`go install` compila e instala el binario en `$(go env GOPATH)/bin`.
Es la forma estándar de instalar herramientas Go.

```bash
# Instalar un binario desde un módulo remoto:
go install github.com/rakyll/hey@latest
go install golang.org/x/tools/cmd/goimports@latest
go install github.com/go-delve/delve/cmd/dlv@latest

# Instalar una versión específica:
go install github.com/golangci/golangci-lint/cmd/golangci-lint@v1.62.2

# Instalar el binario del proyecto actual:
go install ./cmd/myapp

# ¿Dónde se instala?
echo $(go env GOPATH)/bin
# /home/user/go/bin

# Asegurarse de que está en PATH:
export PATH="$PATH:$(go env GOPATH)/bin"
# Añadir a ~/.bashrc o ~/.zshrc para que sea permanente
```

### 13.1 go install vs go build

```bash
# go build:
# - Compila y genera el binario en el directorio actual (o -o path)
# - Para desarrollo y builds de producción
# - Permite todos los flags (-trimpath, -ldflags, etc.)

# go install:
# - Compila e instala en $GOPATH/bin
# - Para instalar herramientas de desarrollo
# - Con @version, ignora go.mod local (instala aislado)
# - Sin @version, usa go.mod local

# En la práctica:
# go install → para herramientas CLI de desarrollo
# go build   → para tu aplicación de producción
```

---

## 14. Comparación: binarios estáticos en Go vs C vs Rust

```
    ┌─────────────────┬──────────────────┬──────────────────┬──────────────────┐
    │                 │       Go         │       C          │       Rust       │
    ├─────────────────┼──────────────────┼──────────────────┼──────────────────┤
    │ Estático por    │ Sí (con          │ No (requiere     │ Sí (musl target) │
    │ defecto         │ CGO_ENABLED=0)   │ -static flag)    │                  │
    ├─────────────────┼──────────────────┼──────────────────┼──────────────────┤
    │ Dificultad      │ Trivial          │ Difícil          │ Moderada         │
    │                 │ (1 env var)      │ (gestionar deps  │ (musl target)    │
    │                 │                  │  estáticas)      │                  │
    ├─────────────────┼──────────────────┼──────────────────┼──────────────────┤
    │ Tamaño típico   │ 8-15 MB          │ 0.5-2 MB         │ 2-8 MB           │
    │ (hello world→   │ (runtime GC      │ (sin runtime)    │ (runtime         │
    │  HTTP server)   │  incluido)       │                  │  mínimo)         │
    ├─────────────────┼──────────────────┼──────────────────┼──────────────────┤
    │ FROM scratch    │ ✓ trivial        │ ✓ posible        │ ✓ posible        │
    │ Docker          │                  │ (más trabajo)    │ (musl)           │
    ├─────────────────┼──────────────────┼──────────────────┼──────────────────┤
    │ DNS estático    │ ✓ resolver Go    │ Problema con     │ Depende de libc  │
    │                 │ puro funciona    │ glibc NSS        │ binding          │
    ├─────────────────┼──────────────────┼──────────────────┼──────────────────┤
    │ Cross-compile   │ ✓ nativo         │ Requiere cross-  │ Via rustup       │
    │ estático        │ (GOOS/GOARCH)    │ toolchain        │ target add       │
    ├─────────────────┼──────────────────┼──────────────────┼──────────────────┤
    │ Cgo/FFI         │ CGO_ENABLED=1    │ N/A (es C)       │ extern "C" +     │
    │ rompe estático  │ puede romperlo   │                  │ puede romperlo   │
    └─────────────────┴──────────────────┴──────────────────┴──────────────────┘
```

---

## 15. Patrones de deploy con binarios estáticos

### 15.1 scp + systemd — deploy directo

```bash
# El deploy más simple posible:

# 1. Compilar:
CGO_ENABLED=0 go build -trimpath -ldflags="-s -w" -o myapp ./cmd/myapp

# 2. Copiar al servidor:
scp myapp servidor:/usr/local/bin/myapp-new

# 3. Intercambiar binario y reiniciar:
ssh servidor '
    systemctl stop myapp
    mv /usr/local/bin/myapp-new /usr/local/bin/myapp
    systemctl start myapp
'

# Ventajas:
# - Sin Docker, sin Kubernetes, sin orquestador
# - Un archivo = un deploy
# - Rollback: tener el binario anterior guardado

# Unit file de systemd:
# /etc/systemd/system/myapp.service
# [Unit]
# Description=My Go Application
# After=network-online.target
# Wants=network-online.target
#
# [Service]
# Type=simple
# User=myapp
# Group=myapp
# ExecStart=/usr/local/bin/myapp --config=/etc/myapp/config.yaml
# Restart=on-failure
# RestartSec=5
# LimitNOFILE=65536
#
# # Hardening:
# NoNewPrivileges=yes
# ProtectSystem=strict
# ProtectHome=yes
# ReadWritePaths=/var/lib/myapp /var/log/myapp
# PrivateTmp=yes
#
# [Install]
# WantedBy=multi-user.target
```

### 15.2 Packer + AMI con binario embebido

```bash
# Para AWS: crear una AMI con el binario preinstalado.

# packer.json (simplificado):
# {
#   "builders": [{
#     "type": "amazon-ebs",
#     "source_ami": "ami-ubuntu-22.04",
#     "instance_type": "t3.micro",
#     "ssh_username": "ubuntu",
#     "ami_name": "myapp-{{timestamp}}"
#   }],
#   "provisioners": [{
#     "type": "file",
#     "source": "myapp",
#     "destination": "/tmp/myapp"
#   }, {
#     "type": "shell",
#     "inline": [
#       "sudo mv /tmp/myapp /usr/local/bin/myapp",
#       "sudo chmod +x /usr/local/bin/myapp",
#       "sudo systemctl enable myapp"
#     ]
#   }]
# }

# El binario estático hace esto trivial:
# No necesitas instalar Go, ni paquetes, ni dependencias en la AMI.
```

### 15.3 Despliegue blue-green con binarios

```bash
# Blue-green con nginx y binarios estáticos:

# 1. Dos directorios con versiones diferentes:
/opt/myapp/blue/myapp    # versión actual (v1.2.3)
/opt/myapp/green/myapp   # nueva versión (v1.2.4)

# 2. Symlink activo:
/usr/local/bin/myapp -> /opt/myapp/blue/myapp

# 3. Deploy:
scp myapp-v1.2.4 servidor:/opt/myapp/green/myapp
ssh servidor '
    # Test the new binary:
    /opt/myapp/green/myapp --self-test

    # Switch:
    ln -sfn /opt/myapp/green/myapp /usr/local/bin/myapp
    systemctl restart myapp

    # Verify:
    curl -f http://localhost:8080/health || {
        # Rollback:
        ln -sfn /opt/myapp/blue/myapp /usr/local/bin/myapp
        systemctl restart myapp
        echo "ROLLBACK: green failed health check"
        exit 1
    }
    echo "Deploy successful"
'
```

---

## 16. Seguridad de binarios estáticos

### 16.1 Checksums y firmas

```bash
# Generar checksums para distribución:
sha256sum myapp > myapp.sha256

# Verificar en el servidor destino:
sha256sum -c myapp.sha256

# Firma GPG:
gpg --armor --detach-sign myapp
# Genera myapp.asc

# Verificar firma:
gpg --verify myapp.asc myapp
```

### 16.2 SBOM (Software Bill of Materials)

```bash
# Go facilita generar SBOMs porque las dependencias están en el binario:

# Método 1: desde el binario compilado
go version -m myapp

# Método 2: desde el código fuente con herramientas SBOM
# cyclonedx-gomod (genera SBOM en formato CycloneDX):
go install github.com/CycloneDX/cyclonedx-gomod/cmd/cyclonedx-gomod@latest
cyclonedx-gomod mod -json -output sbom.json

# Método 3: syft (genera SBOM de binarios o imágenes Docker)
# syft myapp -o cyclonedx-json > sbom.json
# syft docker:myservice:latest -o spdx-json > sbom.json

# SBOM + govulncheck = gestión de vulnerabilidades completa.
# Sabes EXACTAMENTE qué dependencias tiene cada binario en producción.
```

### 16.3 Hardening del binario

```bash
# 1. PIE (Position Independent Executable):
go build -buildmode=pie -o myapp ./cmd/myapp
# PIE permite ASLR (Address Space Layout Randomization)
# Nota: en Go 1.15+, PIE es default en algunos OS

# 2. Verificar protecciones con checksec:
checksec --file=myapp
# RELRO           STACK CANARY      NX            PIE
# Full RELRO      Canary found      NX enabled    PIE enabled

# 3. FIPS 140 (Go 1.24+):
# Go 1.24 incluye FIPS 140-3 certified crypto nativo.
# GOEXPERIMENT=systemcrypto go build ...
# Para entornos que requieren compliance FIPS (gobierno, finanzas, salud)
```

---

## 17. Tabla de errores comunes

| Error / Síntoma | Causa | Solución |
|---|---|---|
| `error while loading shared libraries: libc.so.6` | Binario dinámico en sistema sin glibc compatible | Recompilar con `CGO_ENABLED=0` |
| `ldd myapp` muestra dependencias | CGO_ENABLED=1 (o cgo requerido por una dependencia) | `CGO_ENABLED=0 go build` o `-extldflags "-static"` |
| `cannot find -lsqlite3` | Compilando con cgo pero sin las libs C instaladas | `apt install libsqlite3-dev` o usar alternativa pure-Go |
| Binario no funciona en Alpine | Compilado con glibc (Alpine usa musl) | `CGO_ENABLED=0` o compilar en Alpine |
| DNS no resuelve con CGO_ENABLED=0 | Resolver Go no soporta tu config NSS | `GODEBUG=netdns=cgo` (requiere CGO_ENABLED=1) o cambiar a DNS estándar |
| `os/user: lookup username: no such file or directory` | CGO_ENABLED=0 en scratch sin /etc/passwd | COPY /etc/passwd en Dockerfile |
| Binario demasiado grande (>50MB) | Debug info incluida | `-ldflags="-s -w"` y verificar dependencias innecesarias |
| Stack traces muestran rutas del build server | No se usó -trimpath | Añadir `-trimpath` al build |
| `go version -m` no muestra VCS info | Shallow clone en CI | `actions/checkout` con `fetch-depth: 0` o `go build -buildvcs=false` |
| Imagen Docker scratch no arranca | Falta ENTRYPOINT o el binario no es estático | Verificar con `file` y `ldd` que es estático |
| TLS handshake error en scratch | Sin certificados CA | COPY ca-certificates.crt desde builder |
| `time: missing Location` en scratch | Sin timezone data | COPY zoneinfo o `import _ "time/tzdata"` |

---

## 18. Ejercicios

### Ejercicio 1 — Binario estático vs dinámico

```
Crea un programa Go que use:
- net/http (servidor HTTP)
- os/user (obtener usuario actual)
- fmt (imprimir información)

1. Compila con CGO_ENABLED=1 y CGO_ENABLED=0
2. Usa file y ldd en ambos binarios
3. Compara los tamaños con ls -lh
4. Ejecuta ambos — ¿se comportan igual?

Predicción: ¿cuál será más grande? ¿Cuánto más grande?
¿Ambos resolverán DNS igual?
```

### Ejercicio 2 — Flags de producción

```
Con el programa del ejercicio 1:

1. Compila sin flags especiales
2. Compila con -ldflags="-s -w"
3. Compila con -ldflags="-s -w" -trimpath
4. Compara los tamaños de los 3 binarios
5. Ejecuta strings myapp | grep /home en cada uno

Predicción: ¿cuánto reduce -ldflags="-s -w" el tamaño?
¿El tercer binario mostrará rutas de tu home?
```

### Ejercicio 3 — Verificar con go version -m

```
Compila tu programa con diferentes flags:
1. go build sin flags
2. CGO_ENABLED=0 go build -trimpath -ldflags="-s -w"
3. go build -buildvcs=false

Ejecuta go version -m en cada binario y compara:
- ¿Cuál muestra CGO_ENABLED=0?
- ¿Cuál muestra trimpath=true?
- ¿Cuál tiene información VCS?

Predicción: ¿el binario con -s -w aún muestra la información
de go version -m? (pista: -s quita symbol table, -w quita DWARF,
pero build info se almacena en otra sección)
```

### Ejercicio 4 — Docker scratch

```
1. Crea un servidor HTTP simple en Go (health check en /health)
2. Escribe un Dockerfile multi-stage con FROM scratch
3. Construye la imagen y anota su tamaño
4. Intenta ejecutarla — ¿funciona?
5. Haz un request HTTPS a una API externa desde el handler
   — ¿funciona sin certificados?
6. Añade los certificados CA y prueba de nuevo

Predicción: ¿qué error darás al intentar HTTPS sin certs?
¿Cuánto añade copiar ca-certificates.crt al tamaño de la imagen?
```

### Ejercicio 5 — Scratch completo con usuario no-root

```
Expande el Dockerfile del ejercicio 4:

1. Añade un usuario no-root (appuser, UID 10001)
2. Añade timezone data
3. Añade import _ "time/tzdata" como alternativa
4. Compara el tamaño de la imagen con y sin zoneinfo copiado
5. Verifica con docker exec que no hay shell disponible

Predicción: ¿cuánto pesa la timezone database? ¿Cuánto añade
import _ "time/tzdata" al binario? ¿Cuál prefieres?
```

### Ejercicio 6 — Distroless vs scratch

```
1. Construye la misma aplicación con 3 images base:
   - FROM scratch
   - FROM gcr.io/distroless/static-debian12
   - FROM alpine:3.19
2. Compara los tamaños de las 3 imágenes
3. Intenta docker exec sh en cada una
4. Verifica que HTTPS funciona en las 3 sin copiar certs manualmente

Predicción: ¿cuál es más grande? ¿En cuál puedes hacer exec?
¿En cuál HTTPS funciona sin configurar certs?
```

### Ejercicio 7 — Resolver DNS

```
Crea un programa Go que resuelva un hostname y muestre
qué resolver está usando:

1. Compila con CGO_ENABLED=0 y CGO_ENABLED=1
2. Ejecuta ambos con GODEBUG=netdns=go+2 y GODEBUG=netdns=cgo+2
3. Observa los logs de qué resolver se usa
4. Añade una entrada en /etc/hosts y verifica que ambos la respetan

Predicción: ¿el resolver Go puro respeta /etc/hosts?
¿Qué pasa si intentas GODEBUG=netdns=cgo con CGO_ENABLED=0?
```

### Ejercicio 8 — Build reproducible

```
1. Compila tu programa con flags reproducibles:
   CGO_ENABLED=0 go build -trimpath -ldflags="-s -w
   -X main.version=v1.0.0" -o myapp ./cmd/myapp
2. Calcula sha256sum
3. Limpia el cache: go clean -cache
4. Compila de nuevo con los mismos flags
5. Calcula sha256sum otra vez — ¿coincide?
6. Cambia un comentario en el código y repite

Predicción: ¿cambiar un comentario cambia el hash del binario?
¿Por qué sí o por qué no?
```

### Ejercicio 9 — SBOM y auditoría

```
Crea un proyecto con 3-4 dependencias externas
(ej: gin, zap, viper, cobra).

1. Compila el binario
2. Ejecuta go version -m sobre el binario
3. Genera un SBOM con cyclonedx-gomod
4. Ejecuta govulncheck ./...
5. Si hay vulnerabilidades, actualiza la dependencia afectada

Predicción: ¿go version -m mostrará TODAS las dependencias
transitivas o solo las directas?
```

### Ejercicio 10 — Deploy completo con systemd

```
1. Crea un servidor HTTP Go con /health y /metrics endpoints
2. Compila como binario estático
3. Crea un unit file de systemd con hardening:
   - NoNewPrivileges, ProtectSystem, ProtectHome
   - Usuario dedicado
   - Restart on failure
4. Despliega en un servidor (local VM o remoto)
5. Verifica con systemctl status, journalctl, y curl

Predicción: ¿qué pasa si el binario crashea? ¿Cuánto tarda
systemd en reiniciarlo con RestartSec=5?
```

---

## 19. Resumen

```
    ┌───────────────────────────────────────────────────────────┐
    │          Compilación estática en Go — Resumen             │
    │                                                           │
    │  EL COMANDO:                                              │
    │  CGO_ENABLED=0 go build -trimpath -ldflags="-s -w" \     │
    │      -o myapp ./cmd/myapp                                 │
    │                                                           │
    │  QUÉ PRODUCE:                                             │
    │  Un binario autocontenido que funciona en cualquier       │
    │  Linux del mismo GOARCH sin dependencias externas.        │
    │                                                           │
    │  POR QUÉ IMPORTA:                                        │
    │  ┌─ scp + systemctl = deploy completo                    │
    │  ├─ FROM scratch = imagen Docker de ~10MB                │
    │  ├─ Sin dependency hell                                  │
    │  ├─ Portabilidad total (Ubuntu, CentOS, Alpine, scratch) │
    │  └─ SBOM integrado (go version -m)                       │
    │                                                           │
    │  FLAGS ESENCIALES:                                        │
    │  ┌─ CGO_ENABLED=0   → sin libc → estático               │
    │  ├─ -trimpath        → sin rutas locales → seguridad     │
    │  ├─ -ldflags="-s -w" → sin debug info → -30% tamaño     │
    │  └─ -ldflags="-X"   → inyectar versión/commit           │
    │                                                           │
    │  SCRATCH CHECKLIST:                                       │
    │  ☐ Binario estático (CGO_ENABLED=0)                      │
    │  ☐ ca-certificates.crt (si HTTPS cliente)                │
    │  ☐ zoneinfo o import _ "time/tzdata"                     │
    │  ☐ /etc/passwd con usuario no-root                       │
    │  ☐ USER nonroot en Dockerfile                            │
    │                                                           │
    │  REGLA DE ORO:                                            │
    │  Si puedes evitar cgo → CGO_ENABLED=0 y no pienses más.  │
    │  Si necesitas cgo → Alpine + musl + extldflags -static.  │
    └───────────────────────────────────────────────────────────┘
```
