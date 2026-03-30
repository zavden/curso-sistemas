# T02 — Cross-compilation

## 1. ¿Qué es cross-compilation?

Cross-compilation es compilar código en una plataforma (host) para que
se ejecute en otra plataforma diferente (target). En Go, esto es
trivial: basta con cambiar dos variables de entorno.

```
    ┌─────────────────────────────────────────────────────────────┐
    │                Cross-compilation en Go                       │
    │                                                             │
    │  Tu máquina (host):                                         │
    │  ┌────────────────────────┐                                 │
    │  │ Linux amd64            │                                 │
    │  │                        │    GOOS=darwin GOARCH=arm64     │
    │  │  go build ─────────────┼──────▶ binario macOS Apple M1   │
    │  │                        │                                 │
    │  │                        │    GOOS=windows GOARCH=amd64    │
    │  │  go build ─────────────┼──────▶ binario Windows x86_64   │
    │  │                        │                                 │
    │  │                        │    GOOS=linux GOARCH=arm64      │
    │  │  go build ─────────────┼──────▶ binario Linux ARM64      │
    │  │                        │        (Raspberry Pi, AWS       │
    │  │                        │         Graviton, etc.)         │
    │  └────────────────────────┘                                 │
    │                                                             │
    │  Sin instalar cross-toolchains.                             │
    │  Sin configurar sysroots.                                   │
    │  Sin descargar SDKs del target.                             │
    │  Solo dos variables de entorno.                             │
    └─────────────────────────────────────────────────────────────┘
```

### 1.1 ¿Por qué esto es revolucionario?

```bash
# Comparación con otros lenguajes:

# C: cross-compilar de Linux a ARM
# 1. Instalar cross-toolchain: apt install gcc-aarch64-linux-gnu
# 2. Instalar headers del kernel para ARM
# 3. Instalar sysroot con las libraries del target
# 4. Configurar CC, CXX, SYSROOT, PKG_CONFIG_PATH...
# 5. Recompilar cada dependencia de C para el target
# 6. aarch64-linux-gnu-gcc -o myapp main.c -L/path/to/sysroot/lib...
# → 30 minutos de configuración, puede fallar por cualquier header

# Rust: cross-compilar de Linux a ARM
# 1. rustup target add aarch64-unknown-linux-gnu
# 2. apt install gcc-aarch64-linux-gnu (still needs a C linker)
# 3. Configurar .cargo/config.toml con linker = "aarch64-linux-gnu-gcc"
# 4. cargo build --target aarch64-unknown-linux-gnu
# → 5 minutos, pero aún necesita cross-toolchain para el linker

# Go: cross-compilar de Linux a ARM
# 1. GOOS=linux GOARCH=arm64 go build -o myapp ./cmd/myapp
# → 0 minutos de configuración. Un comando. Funciona.
```

### 1.2 ¿Por qué es tan fácil en Go?

```bash
# Go tiene su propio compilador y linker escritos en Go.
# No depende de gcc, ld, ni ninguna herramienta del sistema.
#
# Cuando instalas Go, estás instalando compiladores para
# TODAS las plataformas soportadas al mismo tiempo.
#
# El compilador de Go genera código máquina directamente,
# sin pasar por un assembler externo (excepto con cgo).
#
# Por esto CGO_ENABLED=0 es clave para cross-compilation:
# si activas cgo, necesitas un cross-compiler de C para el target.
```

---

## 2. GOOS y GOARCH — las dos variables clave

### 2.1 GOOS — sistema operativo target

```bash
# GOOS define el sistema operativo del binario generado.

# Valores principales:
# GOOS=linux        → Linux (servidores, contenedores, embebidos)
# GOOS=darwin       → macOS
# GOOS=windows      → Windows
# GOOS=freebsd      → FreeBSD
# GOOS=openbsd      → OpenBSD
# GOOS=netbsd       → NetBSD
# GOOS=dragonfly    → DragonFly BSD
# GOOS=solaris      → Oracle Solaris / illumos
# GOOS=aix          → IBM AIX
# GOOS=plan9        → Plan 9
# GOOS=js           → JavaScript (WebAssembly browser)
# GOOS=wasip1       → WASI Preview 1 (WebAssembly standalone)
# GOOS=ios          → iOS (requiere cgo)
# GOOS=android      → Android (requiere NDK)

# Ver tu GOOS actual:
go env GOOS
```

### 2.2 GOARCH — arquitectura del procesador target

```bash
# GOARCH define la arquitectura del CPU.

# Valores principales:
# GOARCH=amd64      → x86_64 (la mayoría de servidores y PCs)
# GOARCH=arm64      → AArch64 (Apple M1/M2/M3, AWS Graviton,
#                     Raspberry Pi 3/4/5 con OS de 64 bits,
#                     Ampere Altra, servidores ARM en general)
# GOARCH=arm        → ARM 32-bit (Raspberry Pi con OS de 32 bits,
#                     dispositivos IoT legacy)
# GOARCH=386        → x86 32-bit (legacy, cada vez más raro)
# GOARCH=riscv64    → RISC-V 64-bit (emergente)
# GOARCH=mips       → MIPS (routers, dispositivos de red)
# GOARCH=mips64     → MIPS 64-bit
# GOARCH=mipsle     → MIPS little-endian
# GOARCH=mips64le   → MIPS 64-bit little-endian
# GOARCH=ppc64      → PowerPC 64-bit
# GOARCH=ppc64le    → PowerPC 64-bit little-endian (IBM Power)
# GOARCH=s390x      → IBM System z (mainframes)
# GOARCH=loong64    → LoongArch 64-bit
# GOARCH=wasm       → WebAssembly

# Ver tu GOARCH actual:
go env GOARCH
```

### 2.3 Subversiones de arquitectura

```bash
# Algunas arquitecturas tienen variantes que controlan el nivel
# mínimo de instrucciones del CPU.

# GOAMD64 (desde Go 1.18):
# GOAMD64=v1  → baseline x86-64 (default, compatible con todos)
# GOAMD64=v2  → + CMPXCHG16B, LAHF-SAHF, POPCNT, SSE3, SSE4.1,
#                SSE4.2, SSSE3 (mayoría de CPUs desde ~2009)
# GOAMD64=v3  → + AVX, AVX2, BMI1, BMI2, F16C, FMA, LZCNT, MOVBE,
#                OSXSAVE (Haswell 2013+, Zen 2017+)
# GOAMD64=v4  → + AVX-512 (Skylake-X 2017+, Zen 4 2022+)

# GOARM (para GOARCH=arm):
# GOARM=5     → ARM v5 (legacy, sin hardware float)
# GOARM=6     → ARM v6 (Raspberry Pi 1, Pi Zero)
# GOARM=7     → ARM v7 (Raspberry Pi 2, la mayoría de ARM 32-bit)

# GOARM64 (desde Go 1.23, para GOARCH=arm64):
# GOARM64=v8.0  → ARMv8.0-A (default, todos los ARM64)
# GOARM64=v9.0  → ARMv9.0-A (Cortex-X2+, Neoverse V2+)

# GOPPC64 (para GOARCH=ppc64/ppc64le):
# GOPPC64=power8  → POWER8 (default)
# GOPPC64=power9  → POWER9
# GOPPC64=power10 → POWER10

# GOMIPS (para GOARCH=mips*):
# GOMIPS=hardfloat  → hardware FPU (default)
# GOMIPS=softfloat  → software FPU (para CPUs sin FPU)
```

```bash
# Ejemplo: compilar para AMD64 v3 (optimizado para servidores modernos):
GOOS=linux GOARCH=amd64 GOAMD64=v3 go build -o myapp-optimized ./cmd/myapp

# El binario usará instrucciones AVX2, BMI2, etc.
# Más rápido en servidores modernos, pero NO funciona en CPUs pre-2013.

# ¿Cuándo usar v3 o v4?
# - En servidores donde controlas el hardware (tu flota)
# - En contenedores donde sabes el CPU del host
# - NO en binarios de distribución pública (usa v1 para compatibilidad)
```

---

## 3. Combinaciones GOOS/GOARCH soportadas

```bash
# Ver TODAS las combinaciones soportadas por tu versión de Go:
go tool dist list

# Output (Go 1.23, parcial):
# aix/ppc64
# android/386
# android/amd64
# android/arm
# android/arm64
# darwin/amd64
# darwin/arm64
# dragonfly/amd64
# freebsd/386
# freebsd/amd64
# freebsd/arm
# freebsd/arm64
# freebsd/riscv64
# illumos/amd64
# ios/amd64
# ios/arm64
# js/wasm
# linux/386
# linux/amd64
# linux/arm
# linux/arm64
# linux/loong64
# linux/mips
# linux/mips64
# linux/mips64le
# linux/mipsle
# linux/ppc64
# linux/ppc64le
# linux/riscv64
# linux/s390x
# netbsd/386
# netbsd/amd64
# netbsd/arm
# netbsd/arm64
# openbsd/386
# openbsd/amd64
# openbsd/arm
# openbsd/arm64
# plan9/386
# plan9/amd64
# plan9/arm
# solaris/amd64
# wasip1/wasm
# windows/386
# windows/amd64
# windows/arm
# windows/arm64

# Contar combinaciones:
go tool dist list | wc -l
# ~50+ combinaciones soportadas
```

### 3.1 Las combinaciones que importan para SysAdmin

```bash
# Las 6 combinaciones que cubren el 99% de los escenarios:

# 1. Linux x86_64 — servidores estándar, VMs, contenedores
GOOS=linux GOARCH=amd64

# 2. Linux ARM64 — AWS Graviton, servidores ARM, Raspberry Pi 4/5
GOOS=linux GOARCH=arm64

# 3. macOS Apple Silicon — MacBooks M1/M2/M3 de los developers
GOOS=darwin GOARCH=arm64

# 4. macOS Intel — MacBooks más antiguos
GOOS=darwin GOARCH=amd64

# 5. Windows x86_64 — PCs Windows de la empresa
GOOS=windows GOARCH=amd64

# 6. Linux ARM 32-bit — Raspberry Pi (OS 32-bit), IoT
GOOS=linux GOARCH=arm GOARM=7
```

---

## 4. Cross-compilation en la práctica

### 4.1 Compilar para un target diferente

```bash
# Desde tu Linux amd64, compilar para macOS ARM:
GOOS=darwin GOARCH=arm64 go build -o myapp-darwin-arm64 ./cmd/myapp

# Verificar el binario:
file myapp-darwin-arm64
# myapp-darwin-arm64: Mach-O 64-bit arm64 executable

# Para Windows:
GOOS=windows GOARCH=amd64 go build -o myapp.exe ./cmd/myapp

file myapp.exe
# myapp.exe: PE32+ executable (console) x86-64, for MS Windows

# Para Linux ARM64:
GOOS=linux GOARCH=arm64 go build -o myapp-linux-arm64 ./cmd/myapp

file myapp-linux-arm64
# myapp-linux-arm64: ELF 64-bit LSB executable, ARM aarch64, ...

# Para Raspberry Pi (32-bit):
GOOS=linux GOARCH=arm GOARM=7 go build -o myapp-linux-armv7 ./cmd/myapp

file myapp-linux-armv7
# myapp-linux-armv7: ELF 32-bit LSB executable, ARM, EABI5, ...
```

### 4.2 Nombre de binario con convención de plataforma

```bash
# Convención estándar: nombre-GOOS-GOARCH[.exe]

# Compilar con nombre descriptivo:
GOOS=linux   GOARCH=amd64 go build -o myapp-linux-amd64     ./cmd/myapp
GOOS=linux   GOARCH=arm64 go build -o myapp-linux-arm64     ./cmd/myapp
GOOS=darwin  GOARCH=amd64 go build -o myapp-darwin-amd64    ./cmd/myapp
GOOS=darwin  GOARCH=arm64 go build -o myapp-darwin-arm64    ./cmd/myapp
GOOS=windows GOARCH=amd64 go build -o myapp-windows-amd64.exe ./cmd/myapp

# Nota: Windows necesita la extensión .exe
# Go la añade automáticamente si GOOS=windows y no especificas -o,
# pero con -o debes ponerla tú.
```

### 4.3 Script de build multi-plataforma

```bash
#!/bin/bash
# build-all.sh — compilar para todas las plataformas

set -euo pipefail

APP_NAME="myapp"
VERSION="${1:-dev}"
COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
BUILD_TIME=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
OUTPUT_DIR="dist"

LDFLAGS="-s -w \
    -X main.version=${VERSION} \
    -X main.commit=${COMMIT} \
    -X main.buildTime=${BUILD_TIME}"

# Plataformas a compilar:
PLATFORMS=(
    "linux/amd64"
    "linux/arm64"
    "linux/arm/7"
    "darwin/amd64"
    "darwin/arm64"
    "windows/amd64"
    "windows/arm64"
    "freebsd/amd64"
)

mkdir -p "${OUTPUT_DIR}"

for platform in "${PLATFORMS[@]}"; do
    # Parsear GOOS/GOARCH/GOARM:
    IFS='/' read -r goos goarch goarm <<< "${platform}"

    output="${OUTPUT_DIR}/${APP_NAME}-${goos}-${goarch}"

    # Añadir GOARM al nombre si aplica:
    if [ -n "${goarm}" ]; then
        output="${output}v${goarm}"
    fi

    # Añadir .exe para Windows:
    if [ "${goos}" = "windows" ]; then
        output="${output}.exe"
    fi

    echo "Building ${output}..."

    env CGO_ENABLED=0 \
        GOOS="${goos}" \
        GOARCH="${goarch}" \
        GOARM="${goarm}" \
        go build \
            -trimpath \
            -ldflags="${LDFLAGS}" \
            -o "${output}" \
            ./cmd/"${APP_NAME}"
done

echo ""
echo "Builds completados:"
ls -lh "${OUTPUT_DIR}/"

# Generar checksums:
cd "${OUTPUT_DIR}"
sha256sum * > checksums.txt
echo ""
echo "Checksums:"
cat checksums.txt
```

```bash
# Uso:
chmod +x build-all.sh
./build-all.sh v1.2.3

# Output:
# Building dist/myapp-linux-amd64...
# Building dist/myapp-linux-arm64...
# Building dist/myapp-linux-armv7...
# Building dist/myapp-darwin-amd64...
# Building dist/myapp-darwin-arm64...
# Building dist/myapp-windows-amd64.exe...
# Building dist/myapp-windows-arm64.exe...
# Building dist/myapp-freebsd-amd64...
#
# Builds completados:
# -rwxr-xr-x 1 user user 8.2M myapp-linux-amd64
# -rwxr-xr-x 1 user user 7.9M myapp-linux-arm64
# -rwxr-xr-x 1 user user 7.7M myapp-linux-armv7
# -rwxr-xr-x 1 user user 8.4M myapp-darwin-amd64
# -rwxr-xr-x 1 user user 8.1M myapp-darwin-arm64
# -rwxr-xr-x 1 user user 8.5M myapp-windows-amd64.exe
# -rwxr-xr-x 1 user user 8.2M myapp-windows-arm64.exe
# -rwxr-xr-x 1 user user 8.2M myapp-freebsd-amd64
```

---

## 5. Makefile con cross-compilation

```makefile
# Makefile para cross-compilation

APP_NAME    := myapp
VERSION     := $(shell git describe --tags --always --dirty 2>/dev/null || echo "dev")
COMMIT      := $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")
BUILD_TIME  := $(shell date -u '+%Y-%m-%dT%H:%M:%SZ')
LDFLAGS     := -s -w \
    -X main.version=$(VERSION) \
    -X main.commit=$(COMMIT) \
    -X main.buildTime=$(BUILD_TIME)
DIST_DIR    := dist

.PHONY: help
help: ## mostrar esta ayuda
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | \
	    awk 'BEGIN {FS = ":.*?## "}; {printf "  %-20s %s\n", $$1, $$2}'

.PHONY: build
build: ## compilar para la plataforma actual
	CGO_ENABLED=0 go build -trimpath -ldflags='$(LDFLAGS)' \
	    -o bin/$(APP_NAME) ./cmd/$(APP_NAME)

.PHONY: build-linux-amd64
build-linux-amd64: ## compilar para Linux x86_64
	CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -trimpath \
	    -ldflags='$(LDFLAGS)' -o $(DIST_DIR)/$(APP_NAME)-linux-amd64 \
	    ./cmd/$(APP_NAME)

.PHONY: build-linux-arm64
build-linux-arm64: ## compilar para Linux ARM64 (Graviton, RPi 4/5)
	CGO_ENABLED=0 GOOS=linux GOARCH=arm64 go build -trimpath \
	    -ldflags='$(LDFLAGS)' -o $(DIST_DIR)/$(APP_NAME)-linux-arm64 \
	    ./cmd/$(APP_NAME)

.PHONY: build-linux-armv7
build-linux-armv7: ## compilar para Linux ARMv7 (RPi 2/3 32-bit)
	CGO_ENABLED=0 GOOS=linux GOARCH=arm GOARM=7 go build -trimpath \
	    -ldflags='$(LDFLAGS)' -o $(DIST_DIR)/$(APP_NAME)-linux-armv7 \
	    ./cmd/$(APP_NAME)

.PHONY: build-darwin-amd64
build-darwin-amd64: ## compilar para macOS Intel
	CGO_ENABLED=0 GOOS=darwin GOARCH=amd64 go build -trimpath \
	    -ldflags='$(LDFLAGS)' -o $(DIST_DIR)/$(APP_NAME)-darwin-amd64 \
	    ./cmd/$(APP_NAME)

.PHONY: build-darwin-arm64
build-darwin-arm64: ## compilar para macOS Apple Silicon (M1/M2/M3)
	CGO_ENABLED=0 GOOS=darwin GOARCH=arm64 go build -trimpath \
	    -ldflags='$(LDFLAGS)' -o $(DIST_DIR)/$(APP_NAME)-darwin-arm64 \
	    ./cmd/$(APP_NAME)

.PHONY: build-windows-amd64
build-windows-amd64: ## compilar para Windows x86_64
	CGO_ENABLED=0 GOOS=windows GOARCH=amd64 go build -trimpath \
	    -ldflags='$(LDFLAGS)' -o $(DIST_DIR)/$(APP_NAME)-windows-amd64.exe \
	    ./cmd/$(APP_NAME)

.PHONY: build-all
build-all: ## compilar para todas las plataformas
	@mkdir -p $(DIST_DIR)
	$(MAKE) build-linux-amd64
	$(MAKE) build-linux-arm64
	$(MAKE) build-linux-armv7
	$(MAKE) build-darwin-amd64
	$(MAKE) build-darwin-arm64
	$(MAKE) build-windows-amd64
	@echo ""
	@echo "All builds:"
	@ls -lh $(DIST_DIR)/

.PHONY: checksums
checksums: ## generar checksums SHA256
	cd $(DIST_DIR) && sha256sum * > checksums.txt
	@cat $(DIST_DIR)/checksums.txt

.PHONY: clean
clean: ## limpiar binarios
	rm -rf bin/ $(DIST_DIR)/

.PHONY: release
release: build-all checksums ## build + checksums para release
	@echo ""
	@echo "Release $(VERSION) ready in $(DIST_DIR)/"
```

```bash
# Uso:
make build-all           # compilar todo
make build-linux-arm64   # solo Linux ARM64
make release             # build + checksums
make clean               # limpiar

# Build paralelo (make ejecuta targets en paralelo con -j):
make -j$(nproc) build-all
```

---

## 6. Cross-compilation en Docker

### 6.1 Compilar dentro de Docker para un target específico

```dockerfile
# Dockerfile para cross-compilation
# Compilar DENTRO de Docker para Linux ARM64 (aunque Docker corra en amd64)

FROM golang:1.23-alpine AS builder
WORKDIR /app
COPY go.mod go.sum ./
RUN go mod download
COPY . .

# Cross-compilar para ARM64:
RUN CGO_ENABLED=0 GOOS=linux GOARCH=arm64 go build \
    -trimpath -ldflags="-s -w" \
    -o /app/myapp-arm64 ./cmd/myapp

FROM scratch
COPY --from=builder /app/myapp-arm64 /myapp
ENTRYPOINT ["/myapp"]
```

```bash
# Construir imagen para ARM64 (el binario se cross-compila dentro de Docker):
docker build -t myapp:arm64 .

# ⚠️ Esta imagen contiene un binario ARM64 pero la imagen Docker
# es del MISMO manifiesto que tu host (amd64).
# Para crear una imagen Docker ARM64 real, usa buildx.
```

### 6.2 Docker buildx — imágenes multi-arquitectura

```bash
# docker buildx permite crear imágenes Docker para múltiples
# arquitecturas desde una sola máquina.

# Crear un builder multi-plataforma:
docker buildx create --name multiarch --use
docker buildx inspect --bootstrap

# Dockerfile optimizado para multi-arch:
```

```dockerfile
# Dockerfile.multiarch

FROM --platform=$BUILDPLATFORM golang:1.23-alpine AS builder

# BUILDPLATFORM = la plataforma donde corre Docker (tu máquina)
# TARGETPLATFORM = la plataforma para la que construyes
# TARGETOS, TARGETARCH = componentes de TARGETPLATFORM

ARG TARGETOS
ARG TARGETARCH

WORKDIR /app
COPY go.mod go.sum ./
RUN go mod download && go mod verify
COPY . .

# Cross-compilar usando las variables de buildx:
RUN CGO_ENABLED=0 GOOS=${TARGETOS} GOARCH=${TARGETARCH} go build \
    -trimpath -ldflags="-s -w" \
    -o /app/myapp ./cmd/myapp

FROM scratch
COPY --from=builder /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/
COPY --from=builder /app/myapp /myapp
ENTRYPOINT ["/myapp"]
```

```bash
# Construir y pushear imagen multi-arch:
docker buildx build \
    --platform linux/amd64,linux/arm64,linux/arm/v7 \
    -t registry.example.com/myapp:latest \
    --push \
    .

# Esto crea UN manifest que contiene binarios para 3 arquitecturas.
# Cuando alguien hace docker pull, Docker descarga automáticamente
# la imagen correcta para su plataforma.

# Construir solo para inspección local (sin push):
docker buildx build \
    --platform linux/amd64,linux/arm64 \
    -t myapp:multiarch \
    --load \
    .
# ⚠️ --load solo funciona con una plataforma.
# Para múltiples, usa --push o --output.

# Exportar imágenes a tarball:
docker buildx build \
    --platform linux/amd64,linux/arm64 \
    -t myapp:multiarch \
    --output type=oci,dest=myapp-multiarch.tar \
    .
```

### 6.3 Variables de buildx explicadas

```bash
# docker buildx inyecta estas variables ARG automáticamente:

# BUILDPLATFORM     → donde corre Docker (ej: linux/amd64)
# TARGETPLATFORM    → target de la iteración (ej: linux/arm64)
# TARGETOS          → OS del target (ej: linux)
# TARGETARCH        → arch del target (ej: arm64)
# TARGETVARIANT     → variante (ej: v7 para arm/v7)

# FROM --platform=$BUILDPLATFORM golang:1.23-alpine
# ^^^ Esto asegura que el stage builder siempre usa la imagen
#     nativa de tu máquina (rápido, sin emulación).
#     La cross-compilation la hace Go, no Docker/QEMU.

# Si NO usas --platform=$BUILDPLATFORM, Docker usa QEMU para
# emular el target completo → 10-50x más lento.
```

### 6.4 Comparación: cross-compile nativo vs QEMU

```
    ┌──────────────────────────────────────────────────────────────┐
    │  Método 1: Go cross-compilation (RÁPIDO)                     │
    │                                                              │
    │  FROM --platform=$BUILDPLATFORM golang:1.23                  │
    │  RUN GOOS=$TARGETOS GOARCH=$TARGETARCH go build ...          │
    │                                                              │
    │  ┌──────────────┐    cross-compile     ┌──────────────┐      │
    │  │ Docker amd64 │ ──────────────────▶  │ binario arm64│      │
    │  │ (nativo)     │    (Go hace todo)    │              │      │
    │  └──────────────┘                      └──────────────┘      │
    │  Tiempo: ~30 segundos                                        │
    │                                                              │
    │  ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─    │
    │                                                              │
    │  Método 2: QEMU emulation (LENTO)                            │
    │                                                              │
    │  FROM golang:1.23  (sin --platform=$BUILDPLATFORM)           │
    │  RUN go build ...                                            │
    │                                                              │
    │  ┌──────────────┐      QEMU           ┌──────────────┐      │
    │  │ Docker amd64 │ ──emula ARM64──▶   │ golang arm64 │      │
    │  │ (host)       │    instrucción      │ (emulado)    │      │
    │  │              │    por instrucción  │ go build ... │      │
    │  └──────────────┘                     └──────────────┘      │
    │  Tiempo: ~15-30 minutos                                      │
    │                                                              │
    │  SIEMPRE usa el Método 1 para Go.                            │
    └──────────────────────────────────────────────────────────────┘
```

---

## 7. AWS Graviton y ARM64 en la nube

ARM64 en servidores es cada vez más importante. AWS Graviton, Ampere
Altra, y procesadores ARM en Azure/GCP ofrecen mejor relación
precio/rendimiento que x86_64 en muchas cargas de trabajo.

### 7.1 Compilar para AWS Graviton

```bash
# AWS Graviton (1, 2, 3, 4) usa ARM64:
GOOS=linux GOARCH=arm64 CGO_ENABLED=0 go build \
    -trimpath -ldflags="-s -w" \
    -o myapp-graviton ./cmd/myapp

# Verificar:
file myapp-graviton
# myapp-graviton: ELF 64-bit LSB executable, ARM aarch64, ...

# El mismo binario funciona en:
# - AWS Graviton (EC2 m7g, c7g, r7g, t4g...)
# - Ampere Altra (Oracle Cloud, Azure Cobalt)
# - Raspberry Pi 4/5 (con OS 64-bit)
# - Hetzner CAX (ARM Cloud VMs)
# - Cualquier servidor ARM64 con Linux
```

### 7.2 Imagen Docker multi-arch para ECS/EKS

```bash
# Si tu cluster tiene nodos amd64 Y arm64 (mixto):

docker buildx build \
    --platform linux/amd64,linux/arm64 \
    -t 123456789.dkr.ecr.us-east-1.amazonaws.com/myapp:v1.0.0 \
    --push \
    .

# Kubernetes/ECS selecciona automáticamente la imagen correcta
# según la arquitectura del nodo donde schedula el pod.
```

### 7.3 Ahorros con ARM64

```bash
# Referencia de precios AWS (us-east-1, on-demand, a fecha de ejemplo):
#
# x86_64 (m6i.large):   2 vCPU, 8GB RAM  → $0.096/h
# ARM64  (m7g.large):   2 vCPU, 8GB RAM  → $0.081/h
#
# Ahorro: ~16% en el mismo tier
#
# Graviton también suele ofrecer mejor rendimiento por vCPU,
# así que en muchos casos necesitas MENOS instancias.
#
# Para un SysAdmin con 50+ instancias, la migración a ARM64
# puede ahorrar miles de dólares al mes.
# Y con Go, la migración es cambiar una variable de entorno.
```

---

## 8. Raspberry Pi y dispositivos embebidos

Go es excelente para herramientas de monitoreo, agentes y utilidades
que corren en Raspberry Pi y dispositivos ARM similares.

### 8.1 Compilar para Raspberry Pi

```bash
# Raspberry Pi 1 / Pi Zero (ARMv6, 32-bit):
GOOS=linux GOARCH=arm GOARM=6 CGO_ENABLED=0 go build \
    -trimpath -ldflags="-s -w" \
    -o myapp-pi-zero ./cmd/myapp

# Raspberry Pi 2 (ARMv7, 32-bit):
GOOS=linux GOARCH=arm GOARM=7 CGO_ENABLED=0 go build \
    -trimpath -ldflags="-s -w" \
    -o myapp-pi2 ./cmd/myapp

# Raspberry Pi 3/4/5 con Raspberry Pi OS de 64-bit:
GOOS=linux GOARCH=arm64 CGO_ENABLED=0 go build \
    -trimpath -ldflags="-s -w" \
    -o myapp-pi64 ./cmd/myapp

# Raspberry Pi 3/4/5 con Raspberry Pi OS de 32-bit:
GOOS=linux GOARCH=arm GOARM=7 CGO_ENABLED=0 go build \
    -trimpath -ldflags="-s -w" \
    -o myapp-pi32 ./cmd/myapp

# ¿Cómo saber si mi Pi tiene OS de 32 o 64 bits?
# ssh pi@raspberrypi 'uname -m'
# aarch64 → 64-bit → usa GOARCH=arm64
# armv7l  → 32-bit → usa GOARCH=arm GOARM=7
# armv6l  → 32-bit → usa GOARCH=arm GOARM=6
```

### 8.2 Deploy a Raspberry Pi

```bash
# El deploy es trivial: scp + systemd

# 1. Cross-compilar en tu máquina:
GOOS=linux GOARCH=arm64 CGO_ENABLED=0 go build \
    -trimpath -ldflags="-s -w" \
    -o monitor-agent ./cmd/agent

# 2. Copiar al Pi:
scp monitor-agent pi@192.168.1.100:/usr/local/bin/

# 3. Crear unit file de systemd:
ssh pi@192.168.1.100 'sudo tee /etc/systemd/system/monitor-agent.service' << 'EOF'
[Unit]
Description=Monitoring Agent
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=/usr/local/bin/monitor-agent
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

# 4. Habilitar y arrancar:
ssh pi@192.168.1.100 '
    sudo systemctl daemon-reload
    sudo systemctl enable monitor-agent
    sudo systemctl start monitor-agent
    sudo systemctl status monitor-agent
'

# No necesitas instalar Go en el Pi.
# No necesitas compilar en el Pi (que es lento).
# Cross-compilas en tu máquina rápida → copias → funciona.
```

### 8.3 Caso de uso: agente de monitoreo para fleet de Pis

```go
// Ejemplo: agente que envía métricas de temperatura y CPU

package main

import (
    "fmt"
    "net/http"
    "os"
    "strings"
    "time"
)

func getCPUTemp() (float64, error) {
    // En Raspberry Pi, la temperatura está en:
    data, err := os.ReadFile("/sys/class/thermal/thermal_zone0/temp")
    if err != nil {
        return 0, err
    }
    var milliC float64
    fmt.Sscanf(strings.TrimSpace(string(data)), "%f", &milliC)
    return milliC / 1000.0, nil
}

func main() {
    hostname, _ := os.Hostname()

    http.HandleFunc("/metrics", func(w http.ResponseWriter, r *http.Request) {
        temp, err := getCPUTemp()
        if err != nil {
            http.Error(w, err.Error(), 500)
            return
        }
        // Formato Prometheus:
        fmt.Fprintf(w, "# HELP cpu_temperature_celsius CPU temperature\n")
        fmt.Fprintf(w, "# TYPE cpu_temperature_celsius gauge\n")
        fmt.Fprintf(w, "cpu_temperature_celsius{host=%q} %.1f\n", hostname, temp)
    })

    fmt.Printf("[%s] Agent started on :9100\n", time.Now().Format(time.RFC3339))
    http.ListenAndServe(":9100", nil)
}

// Cross-compilar:
// GOOS=linux GOARCH=arm64 CGO_ENABLED=0 go build -ldflags="-s -w" -o agent
// scp agent pi@192.168.1.100:/usr/local/bin/monitor-agent
```

---

## 9. Compilar para routers y dispositivos MIPS

Muchos routers y dispositivos de red usan procesadores MIPS.
Go puede compilar para estos dispositivos.

```bash
# Routers OpenWrt comunes:
# - MediaTek MT7621 → mipsel (MIPS little-endian)
# - Qualcomm Atheros → mips (MIPS big-endian)
# - Broadcom BCM → mipsel

# Compilar para MIPS little-endian (la mayoría de routers modernos):
GOOS=linux GOARCH=mipsle GOMIPS=softfloat CGO_ENABLED=0 go build \
    -trimpath -ldflags="-s -w" \
    -o myapp-mipsle ./cmd/myapp

# MIPS big-endian:
GOOS=linux GOARCH=mips GOMIPS=softfloat CGO_ENABLED=0 go build \
    -trimpath -ldflags="-s -w" \
    -o myapp-mips ./cmd/myapp

# MIPS64 little-endian:
GOOS=linux GOARCH=mips64le CGO_ENABLED=0 go build \
    -trimpath -ldflags="-s -w" \
    -o myapp-mips64le ./cmd/myapp

# GOMIPS=softfloat es importante para routers que no tienen FPU.
# Sin este flag, el binario puede crashear con "illegal instruction".
```

```bash
# Caso de uso: herramienta de diagnóstico de red para OpenWrt

# 1. Cross-compilar en tu máquina:
GOOS=linux GOARCH=mipsle GOMIPS=softfloat CGO_ENABLED=0 go build \
    -trimpath -ldflags="-s -w" \
    -o netcheck ./cmd/netcheck

# 2. Copiar al router:
scp netcheck root@192.168.1.1:/tmp/

# 3. Ejecutar:
ssh root@192.168.1.1 '/tmp/netcheck --interface=eth0'

# ⚠️ Consideraciones para MIPS:
# - RAM limitada (64-256MB) → Go runtime consume ~4MB mínimo
# - Flash limitada → mantener binarios pequeños con -ldflags="-s -w"
# - Sin FPU → usar GOMIPS=softfloat
# - El binario estático de Go puede ser grande para estos dispositivos
```

---

## 10. WebAssembly (WASM)

Go puede compilar a WebAssembly para ejecutar en navegadores
o en runtimes WASI (Wasmtime, Wasmer, wazero).

### 10.1 WASM para navegador (js/wasm)

```bash
# Compilar para WebAssembly (navegador):
GOOS=js GOARCH=wasm go build -o myapp.wasm ./cmd/myapp

# Necesitas el glue JS que viene con Go:
cp "$(go env GOROOT)/lib/wasm/wasm_exec.js" .

# HTML mínimo para ejecutar:
# <html>
# <head>
#   <script src="wasm_exec.js"></script>
#   <script>
#     const go = new Go();
#     WebAssembly.instantiateStreaming(fetch("myapp.wasm"), go.importObject)
#       .then(result => go.run(result.instance));
#   </script>
# </head>
# </html>
```

### 10.2 WASI (wasip1/wasm) — ejecución standalone

```bash
# WASI = WebAssembly System Interface
# Permite ejecutar Wasm fuera del navegador (como un "contenedor ligero").

# Compilar para WASI:
GOOS=wasip1 GOARCH=wasm go build -o myapp.wasm ./cmd/myapp

# Ejecutar con Wasmtime:
wasmtime myapp.wasm

# Ejecutar con wazero (runtime Wasm escrito en Go):
go install github.com/nicholasgasior/wazero-cli@latest
wazero run myapp.wasm

# ¿Por qué WASI importa para SysAdmin?
# - Sandboxing: Wasm tiene un modelo de seguridad estricto
#   (capability-based, no puede acceder al FS sin permiso explícito)
# - Portabilidad: un .wasm funciona en cualquier OS con un runtime
# - Plugins: herramientas como Envoy, Istio, y Terraform usan
#   Wasm para plugins extensibles
# - Edge computing: Cloudflare Workers, Fastly Compute usan Wasm
```

---

## 11. Cross-compilation y cgo

Cuando necesitas cgo para cross-compilation, la situación se complica
porque necesitas un cross-compiler de C para el target.

### 11.1 El problema

```bash
# Sin cgo: Go se encarga de todo → trivial
GOOS=linux GOARCH=arm64 CGO_ENABLED=0 go build ...  # ✓ funciona

# Con cgo: necesitas un compilador de C para el target
GOOS=linux GOARCH=arm64 CGO_ENABLED=1 go build ...
# error: gcc: error: unrecognized command-line option '--64'
# Necesitas aarch64-linux-gnu-gcc, no gcc del host.
```

### 11.2 Instalar cross-compilers de C

```bash
# Debian/Ubuntu:

# Para ARM64:
sudo apt install gcc-aarch64-linux-gnu

# Para ARM 32-bit:
sudo apt install gcc-arm-linux-gnueabihf

# Para Windows (mingw):
sudo apt install gcc-mingw-w64-x86-64

# Para MIPS:
sudo apt install gcc-mipsel-linux-gnu

# Fedora:
sudo dnf install gcc-aarch64-linux-gnu
sudo dnf install mingw64-gcc
```

### 11.3 Cross-compilar con cgo

```bash
# Especificar el cross-compiler con CC:

# Linux ARM64 con cgo:
CGO_ENABLED=1 \
CC=aarch64-linux-gnu-gcc \
GOOS=linux GOARCH=arm64 \
go build -o myapp-arm64 ./cmd/myapp

# Linux ARM64 estático con cgo:
CGO_ENABLED=1 \
CC=aarch64-linux-gnu-gcc \
GOOS=linux GOARCH=arm64 \
go build \
    -ldflags='-extldflags "-static"' \
    -o myapp-arm64-static ./cmd/myapp

# Windows con cgo (usando mingw):
CGO_ENABLED=1 \
CC=x86_64-w64-mingw32-gcc \
GOOS=windows GOARCH=amd64 \
go build -o myapp.exe ./cmd/myapp
```

### 11.4 Docker como cross-compile environment

```bash
# Usar Docker para cross-compilar con cgo sin instalar nada localmente:

# Ejemplo: cross-compilar para ARM64 con SQLite (cgo):
docker run --rm -v "$(pwd)":/app -w /app \
    -e CGO_ENABLED=1 \
    -e GOOS=linux \
    -e GOARCH=arm64 \
    -e CC=aarch64-linux-gnu-gcc \
    golang:1.23 \
    bash -c '
        apt-get update && apt-get install -y gcc-aarch64-linux-gnu
        go build -ldflags="-extldflags -static" -o myapp-arm64 ./cmd/myapp
    '
```

### 11.5 zig cc — el cross-compiler universal

```bash
# zig cc es un drop-in replacement de gcc que puede cross-compilar
# a CUALQUIER target sin instalar cross-toolchains separados.

# Instalar zig:
# https://ziglang.org/download/

# Cross-compilar Go con cgo usando zig:
CGO_ENABLED=1 \
CC="zig cc -target aarch64-linux-gnu" \
CXX="zig c++ -target aarch64-linux-gnu" \
GOOS=linux GOARCH=arm64 \
go build -o myapp-arm64 ./cmd/myapp

# zig cc soporta casi TODOS los targets que Go soporta.
# No necesitas instalar gcc-aarch64-linux-gnu, gcc-arm-linux-gnueabihf, etc.
# Un solo binario de zig los reemplaza a todos.

# Targets útiles para SysAdmin:
# zig cc -target aarch64-linux-gnu       → Linux ARM64
# zig cc -target arm-linux-gnueabihf     → Linux ARM 32-bit
# zig cc -target x86_64-linux-gnu        → Linux x86_64
# zig cc -target x86_64-linux-musl       → Linux x86_64 (musl/Alpine)
# zig cc -target aarch64-linux-musl      → Linux ARM64 (musl/Alpine)
# zig cc -target x86_64-windows-gnu      → Windows x86_64
```

---

## 12. CI/CD con cross-compilation

### 12.1 GitHub Actions — build multi-plataforma

```yaml
# .github/workflows/release.yml

name: Release
on:
  push:
    tags:
      - 'v*'

permissions:
  contents: write

jobs:
  build:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        include:
          - goos: linux
            goarch: amd64
            suffix: linux-amd64
          - goos: linux
            goarch: arm64
            suffix: linux-arm64
          - goos: linux
            goarch: arm
            goarm: "7"
            suffix: linux-armv7
          - goos: darwin
            goarch: amd64
            suffix: darwin-amd64
          - goos: darwin
            goarch: arm64
            suffix: darwin-arm64
          - goos: windows
            goarch: amd64
            suffix: windows-amd64.exe

    steps:
      - uses: actions/checkout@v4

      - uses: actions/setup-go@v5
        with:
          go-version: stable

      - name: Build
        env:
          GOOS: ${{ matrix.goos }}
          GOARCH: ${{ matrix.goarch }}
          GOARM: ${{ matrix.goarm }}
          CGO_ENABLED: "0"
        run: |
          go build -trimpath \
            -ldflags="-s -w -X main.version=${{ github.ref_name }}" \
            -o myapp-${{ matrix.suffix }} \
            ./cmd/myapp

      - name: Upload artifact
        uses: actions/upload-artifact@v4
        with:
          name: myapp-${{ matrix.suffix }}
          path: myapp-${{ matrix.suffix }}

  release:
    needs: build
    runs-on: ubuntu-latest
    steps:
      - name: Download all artifacts
        uses: actions/download-artifact@v4

      - name: Create checksums
        run: |
          find . -type f -name 'myapp-*' -exec sha256sum {} + > checksums.txt
          cat checksums.txt

      - name: Create GitHub Release
        uses: softprops/action-gh-release@v2
        with:
          files: |
            myapp-*/myapp-*
            checksums.txt
          generate_release_notes: true
```

### 12.2 GitHub Actions con GoReleaser

```yaml
# GoReleaser automatiza TODA la release pipeline:
# build multi-plataforma + checksums + release + changelog + homebrew + Docker

# .github/workflows/goreleaser.yml
name: GoReleaser
on:
  push:
    tags:
      - 'v*'

permissions:
  contents: write

jobs:
  goreleaser:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - uses: actions/setup-go@v5
        with:
          go-version: stable

      - uses: goreleaser/goreleaser-action@v6
        with:
          version: '~> v2'
          args: release --clean
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

### 12.3 GoReleaser config

```yaml
# .goreleaser.yaml

version: 2

project_name: myapp

before:
  hooks:
    - go mod tidy
    - go generate ./...

builds:
  - id: myapp
    main: ./cmd/myapp
    binary: myapp
    env:
      - CGO_ENABLED=0
    goos:
      - linux
      - darwin
      - windows
      - freebsd
    goarch:
      - amd64
      - arm64
      - arm
    goarm:
      - "7"
    ignore:
      - goos: windows
        goarch: arm
      - goos: freebsd
        goarch: arm
    ldflags:
      - -s -w
      - -X main.version={{.Version}}
      - -X main.commit={{.Commit}}
      - -X main.date={{.Date}}
    flags:
      - -trimpath

archives:
  - id: default
    formats:
      - tar.gz
    format_overrides:
      - goos: windows
        formats:
          - zip
    name_template: >-
      {{ .ProjectName }}_
      {{- .Version }}_
      {{- .Os }}_
      {{- .Arch }}
      {{- if .Arm }}v{{ .Arm }}{{ end }}

checksum:
  name_template: 'checksums.txt'
  algorithm: sha256

changelog:
  sort: asc
  filters:
    exclude:
      - '^docs:'
      - '^test:'
      - '^ci:'
      - Merge pull request

dockers:
  - image_templates:
      - "ghcr.io/org/myapp:{{ .Version }}-amd64"
    use: buildx
    dockerfile: Dockerfile
    build_flag_templates:
      - "--platform=linux/amd64"
    goos: linux
    goarch: amd64

  - image_templates:
      - "ghcr.io/org/myapp:{{ .Version }}-arm64"
    use: buildx
    dockerfile: Dockerfile
    build_flag_templates:
      - "--platform=linux/arm64"
    goos: linux
    goarch: arm64

docker_manifests:
  - name_template: "ghcr.io/org/myapp:{{ .Version }}"
    image_templates:
      - "ghcr.io/org/myapp:{{ .Version }}-amd64"
      - "ghcr.io/org/myapp:{{ .Version }}-arm64"
  - name_template: "ghcr.io/org/myapp:latest"
    image_templates:
      - "ghcr.io/org/myapp:{{ .Version }}-amd64"
      - "ghcr.io/org/myapp:{{ .Version }}-arm64"

# Publicar en Homebrew:
brews:
  - repository:
      owner: org
      name: homebrew-tap
    homepage: "https://github.com/org/myapp"
    description: "My CLI tool"
    license: "MIT"
```

```bash
# GoReleaser genera:
# - Binarios para 6 plataformas (linux, darwin, windows × amd64, arm64)
# - Archives (.tar.gz para Linux/macOS, .zip para Windows)
# - Checksums SHA256
# - Changelog automático desde commits
# - Release en GitHub con todos los assets
# - Imágenes Docker multi-arch
# - Fórmula Homebrew para instalación fácil en macOS/Linux

# Instalar GoReleaser:
go install github.com/goreleaser/goreleaser/v2@latest

# Test local (sin publicar):
goreleaser release --snapshot --clean

# Verificar config:
goreleaser check
```

---

## 13. Herramientas de distribución de binarios

### 13.1 Homebrew (macOS y Linux)

```bash
# Si tu herramienta CLI está en GitHub con GoReleaser,
# puedes distribuirla via Homebrew.

# Los usuarios instalan con:
brew tap org/tap
brew install myapp

# O si tienes la fórmula en homebrew-core:
brew install myapp

# GoReleaser genera la fórmula automáticamente.
# La fórmula descarga el binario precompilado correcto
# según el OS y arch del usuario.
```

### 13.2 go install (distribución directa)

```bash
# Los usuarios pueden instalar tu herramienta directamente:
go install github.com/org/myapp/cmd/myapp@latest

# Ventaja: no necesitan nada más que Go instalado.
# Desventaja: requiere Go instalado, compila desde fuente.

# Para que funcione, tu módulo debe ser público en GitHub/GitLab
# y tener tags de versión semántica (v1.2.3).
```

### 13.3 apt/yum/dnf (paquetes de sistema)

```bash
# Para distribución empresarial, empaquetar como .deb/.rpm.

# GoReleaser soporta esto:
# .goreleaser.yaml:
# nfpms:
#   - id: myapp
#     package_name: myapp
#     vendor: My Org
#     homepage: https://github.com/org/myapp
#     maintainer: Admin <admin@example.com>
#     description: My CLI tool
#     license: MIT
#     formats:
#       - deb
#       - rpm
#     contents:
#       - src: myapp.service
#         dst: /etc/systemd/system/myapp.service
#       - src: myapp.yaml
#         dst: /etc/myapp/config.yaml
#         type: config

# Resultado:
# myapp_1.2.3_amd64.deb
# myapp_1.2.3_arm64.deb
# myapp-1.2.3-1.x86_64.rpm
# myapp-1.2.3-1.aarch64.rpm

# Instalar:
# sudo dpkg -i myapp_1.2.3_amd64.deb
# sudo rpm -i myapp-1.2.3-1.x86_64.rpm
```

---

## 14. Verificar binarios cross-compilados

### 14.1 Verificar sin hardware del target

```bash
# Comprobar con file que el binario es del formato correcto:
file myapp-linux-arm64
# myapp-linux-arm64: ELF 64-bit LSB executable, ARM aarch64, ...

file myapp-windows-amd64.exe
# myapp-windows-amd64.exe: PE32+ executable (console) x86-64, for MS Windows

file myapp-darwin-arm64
# myapp-darwin-arm64: Mach-O 64-bit arm64 executable

file myapp-linux-armv7
# myapp-linux-armv7: ELF 32-bit LSB executable, ARM, EABI5, ...

file myapp-linux-mipsle
# myapp-linux-mipsle: ELF 32-bit LSB executable, MIPS, MIPS32 ...

# Verificar metadatos con go version -m:
go version -m myapp-linux-arm64
# myapp-linux-arm64: go1.23.0
# ...
# build   GOARCH=arm64
# build   GOOS=linux
# build   CGO_ENABLED=0
```

### 14.2 Ejecutar binarios de otro arch con QEMU user-mode

```bash
# QEMU user-mode te permite ejecutar binarios de otras architecturas
# en tu máquina actual. El kernel intercepta la syscall y la traduce.

# Instalar QEMU user-mode:
sudo apt install qemu-user-static binfmt-support

# Ejecutar binario ARM64 en tu máquina amd64:
qemu-aarch64-static ./myapp-linux-arm64 --version
# v1.2.3 (¡funciona!)

# Ejecutar binario ARMv7:
qemu-arm-static ./myapp-linux-armv7 --version

# Ejecutar binario MIPS:
qemu-mipsel-static ./myapp-linux-mipsle --version

# Con binfmt-support configurado, puedes ejecutar directamente:
./myapp-linux-arm64 --version
# El kernel detecta el formato ELF ARM64 y usa QEMU automáticamente

# Esto es MUY útil para testear cross-compiled binaries en CI:
# 1. Cross-compilar para ARM64
# 2. Ejecutar tests con QEMU
# 3. Todo en un runner x86_64 estándar
```

### 14.3 Tests cross-platform en CI

```yaml
# GitHub Actions con QEMU para tests multi-arch:

# name: Cross-platform tests
# on: push
# jobs:
#   test:
#     runs-on: ubuntu-latest
#     strategy:
#       matrix:
#         include:
#           - goos: linux
#             goarch: amd64
#           - goos: linux
#             goarch: arm64
#     steps:
#       - uses: actions/checkout@v4
#       - uses: actions/setup-go@v5
#         with:
#           go-version: stable
#
#       - name: Setup QEMU
#         if: matrix.goarch != 'amd64'
#         uses: docker/setup-qemu-action@v3
#
#       - name: Build
#         env:
#           GOOS: ${{ matrix.goos }}
#           GOARCH: ${{ matrix.goarch }}
#           CGO_ENABLED: "0"
#         run: go build -o myapp ./cmd/myapp
#
#       - name: Test binary
#         run: |
#           if [ "${{ matrix.goarch }}" = "arm64" ]; then
#             qemu-aarch64-static ./myapp --version
#           else
#             ./myapp --version
#           fi
```

---

## 15. Consideraciones de rendimiento cross-platform

### 15.1 Diferencias de rendimiento entre arquitecturas

```bash
# El mismo código Go puede tener rendimiento diferente
# en diferentes arquitecturas.

# Factores:
# 1. Tamaño de cache lines (64B en x86, 64-128B en ARM)
# 2. Instrucciones SIMD disponibles (SSE/AVX vs NEON)
# 3. Modelo de memoria (x86 es TSO, ARM es weakly ordered)
# 4. Branch prediction y pipeline depth

# Go maneja la mayoría de estas diferencias internamente,
# pero si haces código performance-critical:
# - Ejecutar benchmarks en el hardware real del target
# - Usar GOAMD64=v3 para aprovechar AVX2 en x86_64
# - El runtime de Go optimiza para ARM64 desde Go 1.17+
```

### 15.2 Atomic operations y memory model

```go
// El memory model de Go es el mismo en todas las plataformas.
// Go inserta las barreras de memoria necesarias automáticamente.

// PERO: si usas sync/atomic, funciona correctamente en todas
// las arquitecturas sin cambios en tu código:

import "sync/atomic"

var counter int64

// Funciona idénticamente en amd64, arm64, mips, s390x...
atomic.AddInt64(&counter, 1)
val := atomic.LoadInt64(&counter)

// Go abstrae las diferencias del modelo de memoria del hardware.
// En x86 (TSO), algunas barreras son no-ops.
// En ARM (weakly ordered), Go inserta DMB/DSB donde es necesario.
// Tú NO necesitas pensar en esto — el compilador lo maneja.
```

---

## 16. Extensiones de archivo por plataforma

Go tiene un sistema de convenciones de archivos que permiten
código específico por plataforma.

### 16.1 Build constraints basados en nombre de archivo

```bash
# Go usa el nombre del archivo para decidir en qué plataforma compilar:

# Solo se compila en Linux:
# config_linux.go

# Solo se compila en Windows:
# config_windows.go

# Solo se compila en macOS:
# config_darwin.go

# Solo se compila en Linux ARM64:
# config_linux_arm64.go

# Solo se compila en amd64 (cualquier OS):
# config_amd64.go

# Patrón: filename_GOOS.go
#          filename_GOARCH.go
#          filename_GOOS_GOARCH.go
```

### 16.2 Ejemplo práctico: código específico por OS

```go
// sysinfo.go — interfaz común
package sysinfo

// MemoryInfo contains memory usage statistics.
type MemoryInfo struct {
    Total     uint64
    Available uint64
    Used      uint64
}

// GetMemory returns current memory usage.
// Implementation is platform-specific.
func GetMemory() (*MemoryInfo, error)
```

```go
// sysinfo_linux.go — implementación Linux
package sysinfo

import (
    "bufio"
    "fmt"
    "os"
    "strings"
)

func GetMemory() (*MemoryInfo, error) {
    f, err := os.Open("/proc/meminfo")
    if err != nil {
        return nil, err
    }
    defer f.Close()

    info := &MemoryInfo{}
    scanner := bufio.NewScanner(f)
    for scanner.Scan() {
        line := scanner.Text()
        switch {
        case strings.HasPrefix(line, "MemTotal:"):
            fmt.Sscanf(line, "MemTotal: %d kB", &info.Total)
            info.Total *= 1024
        case strings.HasPrefix(line, "MemAvailable:"):
            fmt.Sscanf(line, "MemAvailable: %d kB", &info.Available)
            info.Available *= 1024
        }
    }
    info.Used = info.Total - info.Available
    return info, nil
}
```

```go
// sysinfo_darwin.go — implementación macOS
package sysinfo

import (
    "os/exec"
    "strconv"
    "strings"
)

func GetMemory() (*MemoryInfo, error) {
    // macOS: usar sysctl
    out, err := exec.Command("sysctl", "-n", "hw.memsize").Output()
    if err != nil {
        return nil, err
    }
    total, _ := strconv.ParseUint(strings.TrimSpace(string(out)), 10, 64)

    // vm_stat para obtener páginas libres
    // (simplificado — en producción usar syscall)
    return &MemoryInfo{
        Total:     total,
        Available: total / 2, // simplificación
        Used:      total / 2,
    }, nil
}
```

```go
// sysinfo_windows.go — implementación Windows
package sysinfo

import (
    "syscall"
    "unsafe"
)

func GetMemory() (*MemoryInfo, error) {
    // Windows: usar GlobalMemoryStatusEx
    type memoryStatusEx struct {
        Length               uint32
        MemoryLoad           uint32
        TotalPhys            uint64
        AvailPhys            uint64
        TotalPageFile        uint64
        AvailPageFile        uint64
        TotalVirtual         uint64
        AvailVirtual         uint64
        AvailExtendedVirtual uint64
    }

    kernel32 := syscall.MustLoadDLL("kernel32.dll")
    globalMemoryStatusEx := kernel32.MustFindProc("GlobalMemoryStatusEx")

    var ms memoryStatusEx
    ms.Length = uint32(unsafe.Sizeof(ms))
    globalMemoryStatusEx.Call(uintptr(unsafe.Pointer(&ms)))

    return &MemoryInfo{
        Total:     ms.TotalPhys,
        Available: ms.AvailPhys,
        Used:      ms.TotalPhys - ms.AvailPhys,
    }, nil
}
```

```bash
# Cuando compilas:
# GOOS=linux  → usa sysinfo_linux.go (lee /proc/meminfo)
# GOOS=darwin → usa sysinfo_darwin.go (usa sysctl)
# GOOS=windows → usa sysinfo_windows.go (usa kernel32.dll)

# El compilador SOLO incluye el archivo del GOOS actual.
# Los otros archivos ni se parsean ni se compilan.
```

---

## 17. Tabla de errores comunes

| Error / Síntoma | Causa | Solución |
|---|---|---|
| `exec format error` | Binario compilado para otra arquitectura | Verificar GOOS/GOARCH y recompilar |
| `cannot execute binary file` | Formato incorrecto (ej: ELF en macOS) | Verificar con `file` que GOOS coincide |
| `illegal instruction` en ARM/MIPS | GOARM/GOMIPS incorrecto para el hardware | Usar GOARM=6 o GOMIPS=softfloat |
| `binary not found` en Windows | Falta extensión .exe | Añadir .exe en `-o` para GOOS=windows |
| `CGO_ENABLED=1` falla en cross-compile | No hay cross-compiler de C | Usar CGO_ENABLED=0, o instalar cross-gcc, o usar zig cc |
| `undefined: syscall.XYZ` | Syscall no disponible en GOOS target | Usar build constraints para código específico de plataforma |
| Docker buildx muy lento | Usando QEMU para todo en lugar de cross-compile nativo | Usar `FROM --platform=$BUILDPLATFORM` en el stage builder |
| `qemu-aarch64-static: not found` | QEMU user-mode no instalado | `apt install qemu-user-static binfmt-support` |
| Binario funciona en local pero no en target | OS/arch correcto pero libc diferente (musl vs glibc) | Usar CGO_ENABLED=0 para binario estático |
| `go tool dist list` no muestra una plataforma | Plataforma no soportada en tu versión de Go | Actualizar Go o verificar el target |
| GoReleaser falla con `no matching files` | `builds.main` no apunta al paquete correcto | Verificar `main: ./cmd/myapp` en `.goreleaser.yaml` |
| Docker manifest push falla | No autenticado en registry o imágenes no existen | `docker login` y verificar que las imágenes fueron pusheadas |

---

## 18. Ejercicios

### Ejercicio 1 — Cross-compilation básica

```
Crea un programa Go que muestre:
- GOOS/GOARCH del binario (usando runtime.GOOS, runtime.GOARCH)
- Hostname del sistema
- Número de CPUs

1. Compila para linux/amd64, linux/arm64, darwin/arm64, windows/amd64
2. Usa file en cada binario para verificar el formato
3. Ejecuta go version -m en cada uno y compara GOARCH

Predicción: ¿runtime.GOARCH mostrará la arch del host o del target?
¿Qué formato tiene el binario de Windows? ¿Y el de macOS?
```

### Ejercicio 2 — Script build-all.sh

```
Usando el script de la sección 4.3 como referencia:

1. Escribe tu propio build-all.sh con al menos 5 plataformas
2. Añade inyección de versión con ldflags
3. Genera checksums SHA256
4. Ejecuta y verifica los binarios

Predicción: ¿cuánto tarda compilar 5 plataformas secuencialmente?
¿Y si usas GNU parallel o & para paralelizar?
```

### Ejercicio 3 — Makefile multi-plataforma

```
Crea un Makefile con targets:
- build (plataforma actual)
- build-linux-amd64
- build-linux-arm64
- build-darwin-arm64
- build-all (todos los anteriores)
- checksums
- release (build-all + checksums)
- clean

1. Ejecuta make build-all
2. Ejecuta make release
3. Verifica los checksums

Predicción: ¿make -j4 build-all compilará en paralelo?
¿Qué pasa si un target falla? ¿Los otros se ejecutan?
```

### Ejercicio 4 — Docker multi-arch con buildx

```
1. Crea un Dockerfile multi-stage usando --platform=$BUILDPLATFORM
2. Construye para linux/amd64 y linux/arm64 con buildx
3. Inspecciona el manifest con docker manifest inspect
4. Compara el tiempo de build vs un Dockerfile sin $BUILDPLATFORM

Predicción: ¿cuántas veces más rápido es el cross-compile de Go
vs emulación QEMU? ¿La imagen final tiene el mismo digest
para ambas plataformas?
```

### Ejercicio 5 — Raspberry Pi deploy

```
Si tienes un Raspberry Pi (o una VM ARM):

1. Cross-compila un servidor HTTP que sirva /health
2. Determina si tu Pi es ARMv6, ARMv7, o ARM64 (uname -m)
3. Compila con el GOARCH/GOARM correcto
4. scp al Pi y ejecuta
5. Crea un unit file de systemd

Si NO tienes Pi, usa QEMU:
1. Cross-compila para arm64
2. Instala qemu-user-static
3. Ejecuta ./myapp-arm64 con QEMU

Predicción: ¿tu binario ARM64 funciona en tu máquina amd64
con QEMU instalado? ¿Y sin QEMU?
```

### Ejercicio 6 — Código específico de plataforma

```
Crea un paquete internal/sysinfo que implemente GetUptime():
- Linux: leer /proc/uptime
- macOS: usar sysctl kern.boottime
- Windows: usar GetTickCount64

1. Crea sysinfo_linux.go, sysinfo_darwin.go, sysinfo_windows.go
2. Compila para cada plataforma — ¿compila sin errores?
3. Ejecuta en tu plataforma — ¿funciona?
4. ¿Qué pasa si compilas para freebsd sin un sysinfo_freebsd.go?

Predicción: ¿el compilador te dará error si falta la
implementación para una plataforma? ¿En tiempo de compilación
o de ejecución?
```

### Ejercicio 7 — GOAMD64 microarchitecture levels

```
1. Compila tu programa con GOAMD64=v1, v2, v3
2. Compara los tamaños de los binarios
3. Ejecuta los 3 en tu máquina — ¿todos funcionan?
4. Usa objdump para buscar instrucciones AVX en v3 vs v1

Predicción: ¿v3 será más grande que v1?
¿v3 funcionará en tu máquina?
(pista: depende de la edad de tu CPU)
```

### Ejercicio 8 — GoReleaser local

```
1. Instala GoReleaser
2. Crea un .goreleaser.yaml básico (3 plataformas)
3. Ejecuta goreleaser release --snapshot --clean
4. Inspecciona el directorio dist/

Predicción: ¿qué archivos genera GoReleaser además de los binarios?
¿En qué formato están empaquetados los binarios Linux vs Windows?
```

### Ejercicio 9 — WASM

```
1. Compila tu programa para GOOS=wasip1 GOARCH=wasm
2. Instala wasmtime (curl https://wasmtime.dev/install.sh -sSf | bash)
3. Ejecuta wasmtime myapp.wasm
4. Compara el tamaño del .wasm con el binario Linux nativo

Predicción: ¿el .wasm será más grande o más pequeño que el
binario nativo? ¿Más rápido o más lento?
```

### Ejercicio 10 — CI pipeline con matrix build

```
Crea un workflow de GitHub Actions que:
1. Compile para 4 plataformas usando strategy.matrix
2. Ejecute tests en la plataforma nativa
3. Suba los binarios como artifacts
4. En releases (tags v*), cree un GitHub Release con los binarios

Predicción: ¿los 4 builds de la matrix corren en paralelo?
¿Cada job tiene su propio runner o comparten uno?
```

---

## 19. Resumen

```
    ┌───────────────────────────────────────────────────────────┐
    │           Cross-compilation en Go — Resumen               │
    │                                                           │
    │  EL COMANDO:                                              │
    │  GOOS=<os> GOARCH=<arch> CGO_ENABLED=0 go build          │
    │                                                           │
    │  LAS 6 COMBINACIONES QUE IMPORTAN:                       │
    │  ┌─ linux/amd64    → servidores estándar                 │
    │  ├─ linux/arm64    → AWS Graviton, RPi 4/5               │
    │  ├─ darwin/arm64   → macOS Apple Silicon                 │
    │  ├─ darwin/amd64   → macOS Intel                         │
    │  ├─ windows/amd64  → PCs Windows                         │
    │  └─ linux/arm (v7) → RPi 2/3, IoT                       │
    │                                                           │
    │  POR QUÉ GO ES ÚNICO:                                    │
    │  ┌─ Sin cross-toolchains                                 │
    │  ├─ Sin sysroots                                         │
    │  ├─ Sin SDKs del target                                  │
    │  ├─ Un compilador para TODOS los targets                 │
    │  └─ Dos variables de entorno = cross-compilation         │
    │                                                           │
    │  DOCKER MULTI-ARCH:                                      │
    │  ┌─ FROM --platform=$BUILDPLATFORM (builder nativo)      │
    │  ├─ GOOS=$TARGETOS GOARCH=$TARGETARCH (Go cross-compiles)│
    │  └─ docker buildx --platform linux/amd64,linux/arm64     │
    │                                                           │
    │  DISTRIBUCIÓN:                                            │
    │  ┌─ GoReleaser → builds + releases + Docker + Homebrew   │
    │  ├─ GitHub Actions matrix → CI multi-plataforma          │
    │  └─ Checksums SHA256 → verificación de integridad        │
    │                                                           │
    │  REGLA DE ORO:                                            │
    │  CGO_ENABLED=0 → cross-compilation trivial.              │
    │  CGO_ENABLED=1 → necesitas cross-compiler de C.          │
    │  Intenta siempre CGO_ENABLED=0 primero.                  │
    └───────────────────────────────────────────────────────────┘
```
