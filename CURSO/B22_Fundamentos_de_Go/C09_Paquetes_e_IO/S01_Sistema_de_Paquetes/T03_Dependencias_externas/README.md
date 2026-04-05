# Dependencias externas — go get, versiones, replace directive, go mod vendor, modulos privados

## 1. Introduccion

Go Modules (introducido en Go 1.11, estable desde Go 1.16) es el sistema oficial de gestion de dependencias. Antes de modules, Go usaba `GOPATH` — un workspace global donde todos los proyectos compartian las mismas versiones de todas las dependencias. Era fragil, no reproducible, y generaba conflictos constantes. Go Modules resolvio esto con versionado semantico, checksums verificados, un proxy central, y la garantia de builds reproducibles.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                    GO MODULES — SISTEMA DE DEPENDENCIAS                         │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  ARCHIVOS CLAVE                                                                 │
│  ├─ go.mod   → declara el modulo, version de Go, y dependencias directas       │
│  ├─ go.sum   → checksums criptograficos de TODAS las dependencias              │
│  └─ vendor/  → (opcional) copia local de dependencias                          │
│                                                                                  │
│  COMANDOS PRINCIPALES                                                           │
│  ├─ go mod init     → crear un nuevo modulo                                    │
│  ├─ go get          → agregar/actualizar dependencias                          │
│  ├─ go mod tidy     → limpiar: agregar faltantes, eliminar sobrantes           │
│  ├─ go mod vendor   → copiar dependencias a vendor/                            │
│  ├─ go mod download → descargar dependencias al cache                          │
│  ├─ go mod verify   → verificar checksums                                      │
│  ├─ go mod graph    → imprimir grafo de dependencias                           │
│  ├─ go mod edit     → editar go.mod programaticamente                          │
│  └─ go mod why      → explicar por que se necesita un modulo                   │
│                                                                                  │
│  INFRAESTRUCTURA                                                                │
│  ├─ proxy.golang.org  → proxy/cache central (GOPROXY)                          │
│  ├─ sum.golang.org    → base de datos de checksums (GOSUM)                     │
│  └─ pkg.go.dev        → documentacion publica de modulos                       │
│                                                                                  │
│  VERSIONADO                                                                      │
│  ├─ Semantic Versioning: vMAJOR.MINOR.PATCH                                    │
│  ├─ Minimum Version Selection (MVS): elige la version MINIMA que satisface     │
│  ├─ Major version suffix: v2+ cambia el import path (/v2, /v3, etc.)          │
│  └─ Pseudo-versions: v0.0.0-YYYYMMDDHHMMSS-commitHash                         │
│                                                                                  │
│  COMPARACION                                                                    │
│  ├─ C: no tiene (CMake/Conan/vcpkg son externos, no estandar)                 │
│  ├─ Rust: Cargo (Cargo.toml + Cargo.lock ≈ go.mod + go.sum)                   │
│  └─ Go: go mod (integrado en el toolchain, no es una herramienta separada)    │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. go.mod — el archivo central

### 2.1 Crear un modulo

```bash
# Crear un nuevo modulo
mkdir myproject && cd myproject
go mod init github.com/user/myproject

# El module path es la URL donde se hospeda el codigo
# Para proyectos personales/locales que no se publicaran:
go mod init myproject
go mod init example.com/myproject
```

Esto crea `go.mod`:

```
module github.com/user/myproject

go 1.22
```

### 2.2 Anatomia de go.mod

```
module github.com/user/myproject

go 1.22.0

require (
    github.com/gorilla/mux v1.8.1
    go.uber.org/zap v1.27.0
    golang.org/x/sync v0.6.0
)

require (
    go.uber.org/multierr v1.11.0 // indirect
)

replace (
    github.com/broken/lib => github.com/user/lib-fork v1.0.1
    example.com/local/pkg => ../local-pkg
)

exclude (
    github.com/buggy/pkg v1.2.3
)

retract (
    v1.0.0 // Publicado accidentalmente con bug critico
    [v1.1.0, v1.2.0] // Rango de versiones retiradas
)
```

### 2.3 Directivas de go.mod

```
DIRECTIVAS DE go.mod

module <path>
  Declara el module path. Este es el import path raiz que otros usan
  para importar tus paquetes.
  
  module github.com/user/myproject
  →  import "github.com/user/myproject/internal/auth"

go <version>
  Version minima de Go requerida. Desde Go 1.21, esto afecta el
  comportamiento del lenguaje (ej: Go 1.22 habilita range-over-int).
  
  go 1.22.0

toolchain <version>
  (Go 1.21+) Especifica la version exacta del toolchain a usar.
  Diferente de "go": go es el MINIMO, toolchain es la PREFERIDA.
  
  toolchain go1.22.2

require <path> <version>
  Declara una dependencia y su version.
  // indirect: dependencia transitiva (no importada directamente por tu codigo)
  
  require github.com/gorilla/mux v1.8.1
  require go.uber.org/multierr v1.11.0 // indirect

replace <old> => <new>
  Reemplaza un modulo por otro (fork, path local, version diferente).
  Solo tiene efecto en el modulo raiz (se ignora en dependencias).
  
  replace github.com/pkg => ../local-fork
  replace github.com/pkg v1.0.0 => github.com/user/pkg-fork v1.0.1

exclude <path> <version>
  Prohibe una version especifica. Go elegira otra version.
  Solo tiene efecto en el modulo raiz.
  
  exclude github.com/buggy/pkg v1.2.3

retract <version>
  Marca versiones de TU PROPIO modulo como retiradas.
  go get no las elegira automaticamente (pero se pueden forzar).
  
  retract v1.0.0 // Contiene bug de seguridad
```

---

## 3. go get — agregar y actualizar dependencias

### 3.1 Agregar dependencias

```bash
# Agregar la ultima version estable
go get github.com/gorilla/mux
# → require github.com/gorilla/mux v1.8.1

# Agregar una version especifica
go get github.com/gorilla/mux@v1.8.0
# → require github.com/gorilla/mux v1.8.0

# Agregar la ultima version de una major version especifica
go get github.com/gorilla/mux@v1
# → require github.com/gorilla/mux v1.8.1 (ultima v1.x.x)

# Agregar desde un commit especifico (genera pseudo-version)
go get github.com/user/pkg@a1b2c3d
# → require github.com/user/pkg v0.0.0-20240115103000-a1b2c3d4e5f6

# Agregar desde una rama
go get github.com/user/pkg@main
# → require github.com/user/pkg v0.5.1-0.20240115103000-a1b2c3d4e5f6

# Agregar desde un tag no-semver
go get github.com/user/pkg@some-tag
```

### 3.2 Actualizar dependencias

```bash
# Actualizar a la ultima version minor/patch (respeta semver)
go get -u github.com/gorilla/mux
# v1.8.0 → v1.8.1 (si existe)

# Actualizar SOLO patches (mas conservador)
go get -u=patch github.com/gorilla/mux
# v1.8.0 → v1.8.1 (patch), pero NO v1.9.0 (minor)

# Actualizar TODAS las dependencias
go get -u ./...

# Actualizar todas solo patches
go get -u=patch ./...

# "Actualizar" a una version ANTERIOR (downgrade)
go get github.com/gorilla/mux@v1.7.4
```

### 3.3 Eliminar dependencias

```bash
# No hay "go get -remove". En su lugar:
# 1. Elimina el import del codigo fuente
# 2. Ejecuta go mod tidy
go mod tidy

# go mod tidy hace dos cosas:
# - Agrega require para dependencias que estan en el codigo pero no en go.mod
# - Elimina require para dependencias que estan en go.mod pero no en el codigo
```

### 3.4 go get vs go install

```
go get vs go install — DIFERENCIA CRITICA

go get:
  - MODIFICA go.mod (agrega/actualiza dependencias del proyecto actual)
  - Se ejecuta DENTRO de un modulo (directorio con go.mod)
  - Para gestionar dependencias de tu proyecto
  
  go get github.com/gorilla/mux@v1.8.1
  → Agrega "require github.com/gorilla/mux v1.8.1" a go.mod

go install:
  - INSTALA un binario ejecutable en $GOBIN (o $GOPATH/bin)
  - NO modifica go.mod
  - Para instalar herramientas CLI
  
  go install golang.org/x/tools/cmd/goimports@latest
  → Instala el binario "goimports" en $GOPATH/bin/
  
  go install github.com/golangci/golangci-lint/cmd/golangci-lint@v1.56.0
  → Instala "golangci-lint" en $GOPATH/bin/

REGLA SIMPLE:
  ¿Es una dependencia de tu codigo?     → go get
  ¿Es una herramienta que quieres usar?  → go install
```

---

## 4. go.sum — verificacion de integridad

### 4.1 Que es go.sum

`go.sum` contiene hashes criptograficos (SHA-256) de cada version de cada dependencia. Su proposito es garantizar que las dependencias no cambien entre builds — ni por error, ni por un ataque a la cadena de suministro.

```
# Formato: <modulo> <version> <hash>
# Cada modulo tiene 2 entradas: el codigo y el go.mod

github.com/gorilla/mux v1.8.1 h1:TuMoUvkrechsal...=
github.com/gorilla/mux v1.8.1/go.mod h1:DVbg23sWSpF...=
go.uber.org/zap v1.27.0 h1:aJMhYGrd5QSml...=
go.uber.org/zap v1.27.0/go.mod h1:7GSXEN8Rp/uf...=
```

### 4.2 Reglas de go.sum

```
REGLAS DE go.sum

1. go.sum se GENERA AUTOMATICAMENTE — nunca lo edites a mano

2. go.sum contiene hashes de TODAS las dependencias (directas y transitivas)
   Puede tener mas entradas que go.mod (que solo lista las directas)

3. go.sum DEBE commitearse al repositorio
   Es parte de la reproducibilidad del build.
   Sin go.sum, no hay garantia de integridad.

4. Verificacion:
   go mod verify
   → Compara los hashes del cache local con go.sum
   → Si difieren, algo cambio (posible ataque o error)

5. Si go.sum tiene conflictos de merge:
   Resuelve el merge y ejecuta "go mod tidy" para regenerar

6. sum.golang.org
   Base de datos publica de checksums.
   Go verifica que el hash de una dependencia sea el mismo que
   TODOS los demas usuarios ven — detecta ataques de intercepcion.
   Desactivar: GONOSUMCHECK=pattern o GONOSUMDB=pattern
```

---

## 5. Versionado semantico (Semantic Versioning)

### 5.1 Formato de version

```
vMAJOR.MINOR.PATCH[-prerelease][+metadata]

v1.2.3          → version estable
v0.1.0          → desarrollo inicial (API inestable)
v2.0.0          → major version 2 (breaking changes)
v1.2.3-beta.1   → pre-release (ignorada por go get por defecto)
v1.2.3-rc.1     → release candidate

REGLAS SEMVER:
  MAJOR: cambios incompatibles (breaking changes)
  MINOR: funcionalidad nueva compatible
  PATCH: bug fixes compatibles

REGLAS DE GO MODULES:
  v0.x.x: API inestable — cualquier cambio es aceptable
  v1.x.x: API estable — v1.0.0 a v1.99.99 deben ser compatibles
  v2.0.0: NUEVO import path — "github.com/user/pkg/v2"
```

### 5.2 Minimum Version Selection (MVS)

Go usa un algoritmo unico llamado **Minimum Version Selection**: de todas las versiones que satisfacen los requires, elige la **minima**. Esto es opuesto a la mayoria de package managers (npm, pip, cargo) que eligen la **maxima**.

```
MVS — MINIMUM VERSION SELECTION

Tu proyecto requiere:
  require A v1.2.0
  
La dependencia B requiere:
  require A v1.3.0

Otro package manager (npm, pip, cargo):
  → Instalaria A v1.5.0 (la ULTIMA compatible)
  → Podria romper cosas si v1.5.0 tiene un bug nuevo

Go MVS:
  → Instala A v1.3.0 (la MINIMA que satisface TODOS los requires)
  → No v1.2.0 (no satisface B) y no v1.5.0 (no es necesaria)
  → Builds mas reproducibles — solo sube de version cuando es necesario

VENTAJAS DE MVS:
  1. Reproducible: mismo go.mod → mismas versiones SIEMPRE
  2. Predecible: nunca se sube de version "por sorpresa"
  3. Sin solver: no hay SAT solver complejo (O(n) vs NP-complete)
  4. Sin lock file separado: go.mod ES suficiente para reproducir
     (go.sum es para integridad, no para reproducibilidad)

DESVENTAJA:
  Puedes quedarte con versiones viejas con bugs conocidos.
  go get -u actualiza explicitamente.
```

```
EJEMPLO DE MVS

Tu go.mod:
  require (
      A v1.2.0
      B v1.0.0
  )

A v1.2.0 requiere:
  C v1.1.0

B v1.0.0 requiere:
  C v1.3.0

MVS resuelve:
  A v1.2.0  (lo que pides)
  B v1.0.0  (lo que pides)
  C v1.3.0  (minimo que satisface A Y B: max(v1.1.0, v1.3.0) = v1.3.0)
```

### 5.3 Major version suffix

Cuando un modulo publica v2.0.0 o superior, el import path **debe cambiar**:

```go
// v1: import path normal
import "github.com/user/mylib"

// v2: import path con /v2
import "github.com/user/mylib/v2"

// v3: import path con /v3
import "github.com/user/mylib/v3"
```

```
// go.mod del modulo v2:
module github.com/user/mylib/v2

go 1.22

// Codigo del modulo v2:
package mylib // El package name NO cambia

// Los usuarios importan:
import "github.com/user/mylib/v2"
mylib.DoSomething() // Usan el nombre del package, no "v2"
```

```
MAJOR VERSION SUFFIX — por que?

Permite que v1 y v2 coexistan en el mismo build.
Si una de tus dependencias usa mylib/v1 y otra usa mylib/v2,
ambas pueden coexistir porque son packages diferentes.

Sin suffix: diamond dependency problem
  Tu codigo → A v1 → mylib v1
  Tu codigo → B v1 → mylib v2  ← CONFLICTO! Solo puedes tener un mylib

Con suffix: coexistencia
  Tu codigo → A v1 → mylib v1      (import "github.com/user/mylib")
  Tu codigo → B v1 → mylib/v2 v2   (import "github.com/user/mylib/v2")
  Ambos son packages diferentes — no hay conflicto.
```

### 5.4 Pseudo-versions

Cuando referencias un commit que no tiene tag semver, Go genera una pseudo-version:

```
FORMATO DE PSEUDO-VERSION

v0.0.0-YYYYMMDDHHMMSS-abcdefabcdef

Ejemplo:
  v0.0.0-20240115103000-a1b2c3d4e5f6
  │       │              │
  │       │              └─ 12 chars del commit hash
  │       └─ timestamp del commit (UTC)
  └─ base version (la ultima tag anterior, o v0.0.0 si no hay tags)

Si existe un tag anterior v1.2.3:
  v1.2.4-0.20240115103000-a1b2c3d4e5f6
  │
  └─ Un patch mas que la ultima tag (indica "despues de v1.2.3")

GENERAR:
  go get github.com/user/pkg@main          → pseudo-version de HEAD de main
  go get github.com/user/pkg@a1b2c3d       → pseudo-version de ese commit
  go get github.com/user/pkg@branch-name   → pseudo-version de HEAD de esa rama
```

---

## 6. go mod tidy — el comando mas usado

`go mod tidy` es el comando que mas vas a ejecutar. Sincroniza `go.mod` y `go.sum` con el codigo fuente real:

```bash
go mod tidy

# Lo que hace:
# 1. Escanea TODOS los archivos .go del modulo (y tests)
# 2. Agrega a go.mod las dependencias que estan en imports pero no en go.mod
# 3. Elimina de go.mod las dependencias que ya no se importan
# 4. Actualiza go.sum con los hashes correctos
# 5. Descarga dependencias faltantes al cache local

# Flags utiles:
go mod tidy -v          # Verbose: muestra que agrega/elimina
go mod tidy -go=1.22    # Ajustar la directiva go version
go mod tidy -e          # Continuar incluso con errores de importacion
```

```
CUANDO EJECUTAR go mod tidy

SIEMPRE:
  - Despues de agregar/eliminar un import en tu codigo
  - Despues de un go get
  - Antes de un commit
  - Despues de resolver merge conflicts en go.mod/go.sum
  - Antes de go mod vendor

NUNCA ES PELIGROSO:
  go mod tidy no modifica tu codigo — solo go.mod y go.sum.
  Si algo sale mal, git checkout go.mod go.sum restaura el estado anterior.
```

---

## 7. replace — reemplazar dependencias

### 7.1 Casos de uso

`replace` redirige un modulo a otro lugar. Solo funciona en el **modulo raiz** (se ignora en dependencias transitivas):

```bash
# Sintaxis en go.mod:
# replace <modulo-original> [version] => <reemplazo> [version]

# O via comando:
go mod edit -replace github.com/original/pkg=github.com/user/fork@v1.0.1
go mod edit -replace github.com/original/pkg=../local-fork
```

### 7.2 Replace con path local (desarrollo)

```
// Caso: estas desarrollando 2 modulos simultaneamente y quieres
// testear cambios sin publicar

module github.com/user/myproject

go 1.22

require github.com/user/mylib v1.0.0

// Usar la copia local en vez de la version publicada
replace github.com/user/mylib => ../mylib
```

```
ESTRUCTURA EN DISCO:

~/projects/
├── myproject/
│   ├── go.mod       (tiene el replace => ../mylib)
│   ├── go.sum
│   └── main.go      (importa github.com/user/mylib)
└── mylib/
    ├── go.mod       (module github.com/user/mylib)
    └── mylib.go

IMPORTANTE:
  - El path local debe contener un go.mod
  - El module path en el go.mod local debe coincidir con el import
  - Quita el replace ANTES de publicar/commitear
  - El path local es RELATIVO al go.mod que tiene el replace
```

### 7.3 Replace con fork

```
// Caso: una dependencia tiene un bug y el maintainer no responde.
// Haces un fork, aplicas el fix, y usas tu fork.

module github.com/user/myproject

go 1.22

require github.com/broken/lib v1.2.3

replace github.com/broken/lib v1.2.3 => github.com/user/lib-fixed v1.2.4
// Tu codigo sigue importando "github.com/broken/lib"
// Pero Go usa "github.com/user/lib-fixed" internamente
```

### 7.4 Replace sin version (todas las versiones)

```
// Reemplazar CUALQUIER version de un modulo
replace github.com/broken/lib => github.com/user/lib-fixed v1.0.0

// vs reemplazar UNA version especifica
replace github.com/broken/lib v1.2.3 => github.com/user/lib-fixed v1.0.0
```

### 7.5 Antipatrones con replace

```
ANTIPATRONES DE replace

1. COMMITEAR replaces con path local
   replace github.com/user/mylib => ../mylib
   Esto ROMPE el build para cualquier otra persona
   (no tienen ../mylib en la misma ruta).
   
   Solucion: usar replace solo en desarrollo local.
   Quitarlo antes de commitear.
   CI puede verificar: grep "=> \.\./" go.mod && exit 1

2. USAR replace para fijar versiones
   replace github.com/pkg v1.5.0 => github.com/pkg v1.4.0
   Mejor: simplemente go get github.com/pkg@v1.4.0
   replace no es necesario para downgrade.

3. CADENA de replaces
   Si tienes 10+ replaces, probablemente tu modulo tiene problemas
   de organizacion o dependencias incompatibles.
```

---

## 8. go mod vendor — vendoring de dependencias

### 8.1 Que es vendoring

Vendoring copia todas las dependencias al directorio `vendor/` dentro de tu proyecto. Tu build usa estas copias locales en vez de descargarlas:

```bash
# Crear/actualizar vendor/
go mod vendor

# El directorio vendor/ se crea con:
vendor/
├── github.com/
│   └── gorilla/
│       └── mux/
│           ├── mux.go
│           ├── route.go
│           └── ...
├── go.uber.org/
│   └── zap/
│       └── ...
└── modules.txt          # Manifiesto de que hay en vendor/
```

```bash
# Build usando vendor/ (automatico si vendor/ existe y tiene modules.txt)
go build ./...

# Forzar uso de vendor/ (falla si vendor/ esta desactualizado)
go build -mod=vendor ./...

# Forzar NO usar vendor/ (usar cache global)
go build -mod=mod ./...

# Verificar que vendor/ esta sincronizado con go.mod
go mod vendor           # Regenerar
git diff vendor/        # Ver si cambio algo
```

### 8.2 Cuando usar vendoring

```
USAR VENDORING CUANDO:

1. BUILDS REPRODUCIBLES SIN RED
   CI/CD en ambientes air-gapped o con proxy limitado.
   Con vendor/, el build no necesita descargar nada.

2. AUDITAR DEPENDENCIAS
   Todo el codigo de terceros esta en tu repo.
   Puedes revisar exactamente que ejecutas.
   Herramientas de security scanning pueden verificar vendor/.

3. PROTECCION CONTRA DESAPARICION
   Si un autor borra un modulo de GitHub, tu build sigue funcionando.
   left-pad problem de npm no aplica si tienes vendor/.

4. REQUISITO EMPRESARIAL
   Algunas empresas requieren vendoring por politica de seguridad.

NO USAR VENDORING CUANDO:

1. PROYECTO OPEN-SOURCE PEQUEÑO
   vendor/ infla el repositorio innecesariamente.
   Los contributors ya tienen Go instalado y pueden descargar deps.

2. MONOREPO GRANDE
   vendor/ duplica dependencias entre sub-modulos.
   Mejor usar un proxy privado (Athens, Artifactory).

3. NO QUIERES RUIDO EN DIFFS
   Actualizar una dependencia genera un diff enorme en vendor/.
   Solucion: commitear vendor/ en un commit separado.
```

### 8.3 vendor/ y CI/CD

```bash
# CI: verificar que vendor/ esta actualizado
go mod vendor
git diff --exit-code vendor/
# Si hay diferencia, el developer olvido re-vendor despues de cambiar deps

# CI: build con vendor/ (sin acceso a red)
go build -mod=vendor ./...
go test -mod=vendor ./...

# Docker: vendor/ simplifica el Dockerfile
# Con vendor/ (no necesita descargar):
FROM golang:1.22 AS builder
WORKDIR /app
COPY . .
RUN go build -mod=vendor -o /app/server ./cmd/server

# Sin vendor/ (necesita descargar):
FROM golang:1.22 AS builder
WORKDIR /app
COPY go.mod go.sum ./
RUN go mod download          # Capa cacheada por Docker
COPY . .
RUN go build -o /app/server ./cmd/server
```

---

## 9. GOPROXY — el proxy de modulos

### 9.1 Como funciona

Cuando Go descarga un modulo, no va directamente a GitHub. Pasa por un **proxy** que cachea y valida los modulos:

```
FLUJO DE DESCARGA

go get github.com/gorilla/mux@v1.8.1

  1. Go consulta GOPROXY (default: https://proxy.golang.org)
  2. El proxy busca en su cache
  3. Si no lo tiene, el proxy descarga de GitHub
  4. El proxy lo cachea (inmutable — esa version nunca cambia)
  5. Go descarga el modulo del proxy
  6. Go verifica el checksum contra sum.golang.org
  7. Go actualiza go.sum con el hash verificado

┌──────────┐     ┌──────────────────┐     ┌────────────┐
│  go get  │────→│ proxy.golang.org │────→│  GitHub    │
│          │←────│  (cache)         │←────│  (source)  │
└──────────┘     └──────────────────┘     └────────────┘
                         │
                         ↓
                 ┌──────────────────┐
                 │  sum.golang.org  │
                 │  (verificacion)  │
                 └──────────────────┘
```

### 9.2 Configurar GOPROXY

```bash
# Ver configuracion actual
go env GOPROXY
# Default: https://proxy.golang.org,direct

# El valor es una lista separada por comas.
# "direct" = ir directamente a la fuente (sin proxy)

# Solo proxy (falla si el proxy no tiene el modulo):
go env -w GOPROXY=https://proxy.golang.org

# Proxy con fallback a directo:
go env -w GOPROXY=https://proxy.golang.org,direct

# Proxy privado (empresa) + publico + directo:
go env -w GOPROXY=https://goproxy.mycompany.com,https://proxy.golang.org,direct

# Sin proxy (ir siempre directo a la fuente):
go env -w GOPROXY=direct

# Off (no descargar nada — solo usa cache local o vendor):
go env -w GOPROXY=off
```

### 9.3 GONOSUMCHECK y GONOSUMDB

```bash
# No verificar checksums para ciertos modulos (modulos privados)
go env -w GONOSUMCHECK=github.com/mycompany/*

# No consultar la base de datos de sumas para ciertos modulos
go env -w GONOSUMDB=github.com/mycompany/*

# Ambos suelen configurarse juntos para modulos privados:
go env -w GOPRIVATE=github.com/mycompany/*
# GOPRIVATE es un shortcut que configura GONOSUMCHECK Y GONOSUMDB
```

---

## 10. Modulos privados

### 10.1 El problema

Por defecto, Go intenta descargar modulos via `proxy.golang.org` y verificar checksums via `sum.golang.org`. Esto **no funciona** para repositorios privados:

```
PROBLEMAS CON MODULOS PRIVADOS

1. proxy.golang.org no puede acceder a repos privados
   → Error: 410 Gone o 404 Not Found

2. sum.golang.org no conoce modulos privados
   → Error: "verifying module: checksum database does not have ..."

3. go get necesita autenticacion para repos privados
   → Error: "authentication required"
```

### 10.2 Configuracion para modulos privados

```bash
# GOPRIVATE: la variable mas importante para modulos privados
# Define que modulos NO deben pasar por proxy NI checksum DB

# Un solo dominio:
go env -w GOPRIVATE=github.com/mycompany

# Multiples patrones (separados por coma):
go env -w GOPRIVATE=github.com/mycompany,gitlab.internal.com,*.corp.example.com

# Todo un dominio con subdominios:
go env -w GOPRIVATE=*.mycompany.com
```

### 10.3 Autenticacion con repositorios privados

```bash
# --- GITHUB ---

# Opcion 1: Token de acceso personal via .netrc
# Crear ~/.netrc:
cat > ~/.netrc << 'EOF'
machine github.com
login oauth2
password ghp_YOUR_PERSONAL_ACCESS_TOKEN
EOF
chmod 600 ~/.netrc

# Opcion 2: SSH insteadOf (si ya tienes SSH keys configuradas)
git config --global url."git@github.com:".insteadOf "https://github.com/"

# Opcion 3: Credential helper de Git
git config --global credential.helper store
# Y luego git pide credenciales la primera vez

# --- GITLAB ---
# Similar, con token en .netrc:
# machine gitlab.com login oauth2 password glpat-YOUR_TOKEN

# --- VERIFICAR ---
# Probar que funciona:
GOPRIVATE=github.com/mycompany go get github.com/mycompany/private-lib@latest
```

### 10.4 CI/CD con modulos privados

```bash
# GitHub Actions:
# Usar GITHUB_TOKEN o un PAT (Personal Access Token)

# .github/workflows/build.yml:
# env:
#   GOPRIVATE: github.com/mycompany
# steps:
#   - name: Configure git for private modules
#     run: git config --global url."https://x-access-token:${{ secrets.GH_PAT }}@github.com/".insteadOf "https://github.com/"
#   - name: Build
#     run: go build ./...

# Docker:
# Pasar credenciales como build args (con BuildKit secrets)

# Dockerfile:
# FROM golang:1.22 AS builder
# ARG GH_TOKEN
# RUN git config --global url."https://x-access-token:${GH_TOKEN}@github.com/".insteadOf "https://github.com/"
# ENV GOPRIVATE=github.com/mycompany
# WORKDIR /app
# COPY go.mod go.sum ./
# RUN go mod download
# COPY . .
# RUN go build -o /app/server ./cmd/server

# Mejor con BuildKit secrets (no quedan en layers):
# docker build --secret id=gh_token,src=./token.txt .
```

### 10.5 Proxy privado (empresa)

```
PROXY PRIVADO — para cuando GOPRIVATE no es suficiente

Herramientas:
  ├─ Athens (https://gomods.io) — open-source, popular
  ├─ Artifactory (JFrog) — enterprise, soporta Go modules
  ├─ Nexus (Sonatype) — enterprise
  └─ goproxy (https://github.com/goproxy/goproxy) — self-hosted simple

Configuracion:
  GOPROXY=https://athens.mycompany.com,https://proxy.golang.org,direct
  GONOSUMDB=github.com/mycompany/*
  
Ventajas del proxy privado:
  1. Cache de TODOS los modulos (publicos y privados) internamente
  2. Control sobre que modulos pueden usarse (allowlist/blocklist)
  3. Auditoria de que dependencias usa cada equipo
  4. Inmutabilidad — incluso si un autor borra un modulo de GitHub,
     tu proxy lo tiene cacheado
  5. Velocidad — descargas desde tu red interna
```

---

## 11. go mod graph, go mod why, go mod edit

### 11.1 go mod graph — visualizar dependencias

```bash
# Imprimir el grafo completo de dependencias
go mod graph

# Output (cada linea: parent child):
# github.com/user/myproject github.com/gorilla/mux@v1.8.1
# github.com/user/myproject go.uber.org/zap@v1.27.0
# go.uber.org/zap@v1.27.0 go.uber.org/multierr@v1.11.0
# ...

# Buscar todas las dependencias de un modulo especifico:
go mod graph | grep gorilla

# Visualizar con graphviz (requiere graphviz instalado):
go mod graph | sed 's/@[^ ]*//g' | sort -u | \
  awk '{print "\"" $1 "\" -> \"" $2 "\""}' | \
  (echo "digraph deps {"; cat; echo "}") | dot -Tpng -o deps.png
```

### 11.2 go mod why — por que necesito esta dependencia?

```bash
# Explicar por que se necesita un modulo
go mod why github.com/gorilla/mux
# Output:
# # github.com/gorilla/mux
# github.com/user/myproject/internal/handler
# github.com/gorilla/mux

# Para todos los modulos:
go mod why -m all

# Si go mod why dice "(main module does not need ...)",
# entonces puedes eliminarlo con go mod tidy
```

### 11.3 go mod edit — editar go.mod programaticamente

```bash
# Agregar require
go mod edit -require github.com/gorilla/mux@v1.8.1

# Eliminar require
go mod edit -droprequire github.com/gorilla/mux

# Agregar replace
go mod edit -replace github.com/old/pkg=github.com/new/pkg@v1.0.0
go mod edit -replace github.com/old/pkg=../local-fork

# Eliminar replace
go mod edit -dropreplace github.com/old/pkg

# Agregar exclude
go mod edit -exclude github.com/buggy/pkg@v1.2.3

# Cambiar version de Go
go mod edit -go=1.22

# Imprimir go.mod como JSON (para scripts)
go mod edit -json

# Formatear go.mod (como gofmt para go.mod)
go mod edit -fmt
```

---

## 12. Workspaces (go.work) — multi-modulo local

### 12.1 El problema que resuelven

Antes de workspaces (Go 1.18+), trabajar con multiples modulos locales simultaneamente requeria `replace` en cada go.mod. Workspaces permiten trabajar con multiples modulos sin modificar sus go.mod:

```bash
# Crear un workspace
cd ~/projects
go work init ./myproject ./mylib ./myutil
```

Esto crea `go.work`:

```
go 1.22

use (
    ./myproject
    ./mylib
    ./myutil
)
```

### 12.2 Estructura de un workspace

```
~/projects/
├── go.work              # Define el workspace
├── myproject/
│   ├── go.mod           # module github.com/user/myproject
│   ├── go.sum
│   └── main.go          # importa github.com/user/mylib
├── mylib/
│   ├── go.mod           # module github.com/user/mylib
│   ├── go.sum
│   └── mylib.go
└── myutil/
    ├── go.mod           # module github.com/user/myutil
    ├── go.sum
    └── util.go
```

### 12.3 Comandos de workspace

```bash
# Crear workspace
go work init ./module1 ./module2

# Agregar un modulo al workspace
go work use ./new-module

# Agregar todos los modulos recursivamente
go work use -r .

# Sincronizar go.work con los go.mod de los modulos
go work sync

# Editar go.work
go work edit -use ./another-module
go work edit -dropuse ./removed-module

# Build/test en modo workspace (automatico si go.work existe)
go build ./...
go test ./...

# Ignorar go.work temporalmente
GOWORK=off go build ./...
```

### 12.4 Workspace vs Replace

```
WORKSPACE vs REPLACE

Replace:
  ✗ Modifica go.mod (no commitear)
  ✗ Un replace por modulo por go.mod
  ✗ Facil olvidar quitarlo antes de push
  ✓ No necesita archivos extra

Workspace:
  ✓ NO modifica go.mod (los go.mod quedan limpios)
  ✓ Un go.work para TODOS los modulos
  ✓ go.work NO se commitea (agregar a .gitignore)
  ✗ Requiere Go 1.18+

REGLA:
  - Desarrollo multi-modulo local → workspace
  - Fork de dependencia para fix permanente → replace
  - CI/CD o produccion → ninguno (usar versiones publicadas)
```

---

## 13. Herramientas utiles para dependencias

### 13.1 Audit de vulnerabilidades

```bash
# govulncheck: verificar vulnerabilidades conocidas en tus dependencias
# (oficial de Go, basado en la Go Vulnerability Database)
go install golang.org/x/vuln/cmd/govulncheck@latest

govulncheck ./...
# Output:
# Scanning your code and 45 packages across 12 modules...
# 
# Vulnerability #1: GO-2024-1234
#   stdlib: net/http has a request smuggling vulnerability
#   Fixed in: go1.22.1
#   Your code calls: net/http.Server.ListenAndServe
#
# === Informational ===
# Vulnerability #2: GO-2024-5678
#   Found in: github.com/gorilla/mux
#   Fixed in: v1.8.2
#   Your code does NOT call the vulnerable function.

# Solo verificar (sin analizar codigo):
govulncheck -mode=binary ./myapp
```

### 13.2 Listar dependencias

```bash
# Listar todas las dependencias con versiones
go list -m all

# Listar solo dependencias directas
go list -m -f '{{if not .Indirect}}{{.Path}} {{.Version}}{{end}}' all

# Listar dependencias con actualizaciones disponibles
go list -m -u all
# github.com/gorilla/mux v1.8.0 [v1.8.1]  ← update disponible

# Listar dependencias con formato JSON
go list -m -json all

# Listar licencias (requiere go-licenses)
go install github.com/google/go-licenses@latest
go-licenses report ./...
```

### 13.3 Limpiar cache

```bash
# Ver ubicacion y tamaño del cache
go env GOMODCACHE
# Normalmente: ~/go/pkg/mod/

du -sh $(go env GOMODCACHE)
# Puede crecer a varios GB con el tiempo

# Limpiar TODA la cache de modulos
go clean -modcache

# Limpiar cache de build (no modulos)
go clean -cache
```

---

## 14. Comparacion con C y Rust

```
GESTION DE DEPENDENCIAS: GO vs C vs RUST

┌─────────────────────┬───────────────────────────┬────────────────────────┬──────────────────────────┐
│ Concepto             │ Go                        │ C                      │ Rust                     │
├─────────────────────┼───────────────────────────┼────────────────────────┼──────────────────────────┤
│ Tool                 │ go mod (integrado)        │ CMake + Conan/vcpkg    │ Cargo (integrado)        │
│                      │                           │ (NO estandar)          │                          │
│ Manifest             │ go.mod                    │ CMakeLists.txt +       │ Cargo.toml               │
│                      │                           │ conanfile.txt          │                          │
│ Lock file            │ go.sum (checksums)        │ conan.lock (si usas    │ Cargo.lock               │
│                      │                           │ Conan)                 │                          │
│ Registry             │ proxy.golang.org          │ No hay central         │ crates.io                │
│ Vendoring            │ go mod vendor             │ Manual o git submodule │ cargo vendor             │
│ Local dev            │ go.work (workspace)       │ -I/path manual         │ [patch] en Cargo.toml    │
│ Version selection    │ MVS (minima)              │ Depende del tool       │ MaxSat solver (maxima)   │
│ Semver               │ Obligatorio               │ No estandar            │ Obligatorio              │
│ Major version        │ Suffix: /v2, /v3          │ N/A                    │ Nombre diferente o major │
│ Private repos        │ GOPRIVATE + .netrc        │ Depende del tool       │ [registries] + git auth  │
│ Vulnerability check  │ govulncheck               │ No estandar            │ cargo audit              │
│ Reproducible builds  │ Si (go.sum + MVS)         │ Dificil sin lock file  │ Si (Cargo.lock)          │
│ Seguridad supply     │ sum.golang.org (checksums)│ No hay estandar        │ crates.io audited        │
│ Subir dependencia    │ go get -u                 │ Manual                 │ cargo update             │
│ Quitar dependencia   │ go mod tidy               │ Manual                 │ cargo remove (no hay*)   │
│                      │                           │                        │ Borrar de Cargo.toml     │
│ Checksum DB          │ sum.golang.org (global)   │ No existe              │ No existe (local locks)  │
│ Workspace            │ go.work (Go 1.18+)        │ N/A                    │ [workspace] (Cargo.toml) │
└─────────────────────┴───────────────────────────┴────────────────────────┴──────────────────────────┘
```

### 14.1 go.mod vs Cargo.toml

```
# Go: go.mod
module github.com/user/myproject

go 1.22

require (
    github.com/gorilla/mux v1.8.1
    go.uber.org/zap v1.27.0
)
```

```toml
# Rust: Cargo.toml
[package]
name = "myproject"
version = "0.1.0"
edition = "2021"

[dependencies]
axum = "0.7"           # Equivale a ^0.7 (compatible con 0.7.x)
tokio = { version = "1", features = ["full"] }
tracing = "0.1"

[dev-dependencies]     # Go no distingue (todo en require)
mockall = "0.12"
```

```
DIFERENCIAS CLAVE

1. FEATURES
   Rust: features = ["full"] — activar subconjuntos de funcionalidad
   Go: no existe. Importas el paquete completo siempre.
       (build tags son lo mas cercano, pero son del compilador, no del paquete)

2. DEV-DEPENDENCIES
   Rust: [dev-dependencies] — solo para tests/benchmarks
   Go: todo va en require. Los imports en _test.go se marcan // indirect
       si no se usan en codigo de produccion, pero se descargan igual.

3. VERSION RANGES
   Rust: "0.7" = ^0.7.0 = compatible con 0.7.x
         ">=1.0, <2.0" = rango explicito
   Go: siempre version EXACTA. MVS decide como resolver.

4. WORKSPACES
   Rust: [workspace] members = ["crate1", "crate2"]
         Vive en Cargo.toml de la raiz
   Go: go.work con use (./module1, ./module2)
         Archivo separado, NO se commitea

5. LOCK FILE
   Rust: Cargo.lock — contiene TODAS las versiones exactas resueltas
         Se commitea para binarios, no para libraries
   Go: go.sum — contiene CHECKSUMS, no versiones resueltas
         go.mod contiene las versiones. go.sum siempre se commitea.
```

---

## 15. Programa completo: proyecto multi-dependencia

Este programa demuestra un proyecto realista con dependencias externas, versionado, y buenas practicas de go.mod:

### Estructura del proyecto

```
bookshelf/
├── go.mod
├── go.sum
├── cmd/
│   └── bookshelf/
│       └── main.go
├── internal/
│   ├── handler/
│   │   └── handler.go
│   ├── model/
│   │   └── book.go
│   └── store/
│       └── memory.go
└── .gitignore          (incluye go.work)
```

### go.mod

```
module github.com/user/bookshelf

go 1.22.0

require (
    github.com/google/uuid v1.6.0
    go.uber.org/zap v1.27.0
)

require (
    go.uber.org/multierr v1.11.0 // indirect
)
```

### cmd/bookshelf/main.go

```go
package main

import (
    "context"
    "fmt"
    "net/http"
    "os"
    "os/signal"
    "syscall"
    "time"

    "go.uber.org/zap"

    "github.com/user/bookshelf/internal/handler"
    "github.com/user/bookshelf/internal/store"
)

func main() {
    // Dependencia externa: zap para logging
    logger, err := zap.NewProduction()
    if err != nil {
        fmt.Fprintf(os.Stderr, "failed to create logger: %v\n", err)
        os.Exit(1)
    }
    defer logger.Sync()

    sugar := logger.Sugar()

    // Componentes internos
    bookStore := store.NewMemory()
    h := handler.New(bookStore, sugar)

    mux := http.NewServeMux()
    mux.HandleFunc("GET /api/books", h.ListBooks)
    mux.HandleFunc("POST /api/books", h.CreateBook)
    mux.HandleFunc("GET /api/books/{id}", h.GetBook)
    mux.HandleFunc("DELETE /api/books/{id}", h.DeleteBook)
    mux.HandleFunc("GET /health", h.Health)

    port := os.Getenv("PORT")
    if port == "" {
        port = "8080"
    }

    server := &http.Server{
        Addr:         ":" + port,
        Handler:      mux,
        ReadTimeout:  10 * time.Second,
        WriteTimeout: 10 * time.Second,
    }

    // Graceful shutdown
    go func() {
        sugar.Infow("server starting", "port", port)
        if err := server.ListenAndServe(); err != http.ErrServerClosed {
            sugar.Fatalw("server error", "error", err)
        }
    }()

    quit := make(chan os.Signal, 1)
    signal.Notify(quit, os.Interrupt, syscall.SIGTERM)
    sig := <-quit

    sugar.Infow("shutdown signal received", "signal", sig.String())

    ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
    defer cancel()

    if err := server.Shutdown(ctx); err != nil {
        sugar.Errorw("shutdown error", "error", err)
    }

    sugar.Info("server stopped")
}
```

### internal/model/book.go

```go
package model

import "time"

// Book represents a book in the store.
type Book struct {
    ID        string    `json:"id"`
    Title     string    `json:"title"`
    Author    string    `json:"author"`
    ISBN      string    `json:"isbn,omitempty"`
    Year      int       `json:"year,omitempty"`
    CreatedAt time.Time `json:"created_at"`
}

// CreateBookRequest is the payload for creating a new book.
type CreateBookRequest struct {
    Title  string `json:"title"`
    Author string `json:"author"`
    ISBN   string `json:"isbn"`
    Year   int    `json:"year"`
}

// Validate checks required fields.
func (r CreateBookRequest) Validate() error {
    if r.Title == "" {
        return ErrTitleRequired
    }
    if r.Author == "" {
        return ErrAuthorRequired
    }
    return nil
}

// Sentinel errors for validation.
type ValidationError struct {
    Field   string `json:"field"`
    Message string `json:"message"`
}

func (e ValidationError) Error() string {
    return e.Field + ": " + e.Message
}

var (
    ErrTitleRequired  = ValidationError{Field: "title", Message: "is required"}
    ErrAuthorRequired = ValidationError{Field: "author", Message: "is required"}
)
```

### internal/store/memory.go

```go
package store

import (
    "fmt"
    "sync"
    "time"

    "github.com/google/uuid" // Dependencia externa: generar UUIDs

    "github.com/user/bookshelf/internal/model"
)

// Memory is an in-memory book store.
type Memory struct {
    mu    sync.RWMutex
    books map[string]*model.Book
}

// NewMemory creates a new in-memory store.
func NewMemory() *Memory {
    return &Memory{
        books: make(map[string]*model.Book),
    }
}

// Create adds a new book and returns it with a generated ID.
func (m *Memory) Create(req model.CreateBookRequest) *model.Book {
    m.mu.Lock()
    defer m.mu.Unlock()

    book := &model.Book{
        ID:        uuid.New().String(), // UUID externo
        Title:     req.Title,
        Author:    req.Author,
        ISBN:      req.ISBN,
        Year:      req.Year,
        CreatedAt: time.Now(),
    }
    m.books[book.ID] = book
    return book
}

// Get retrieves a book by ID.
func (m *Memory) Get(id string) (*model.Book, error) {
    m.mu.RLock()
    defer m.mu.RUnlock()

    book, ok := m.books[id]
    if !ok {
        return nil, fmt.Errorf("book not found: %s", id)
    }
    return book, nil
}

// List returns all books.
func (m *Memory) List() []*model.Book {
    m.mu.RLock()
    defer m.mu.RUnlock()

    result := make([]*model.Book, 0, len(m.books))
    for _, b := range m.books {
        result = append(result, b)
    }
    return result
}

// Delete removes a book by ID.
func (m *Memory) Delete(id string) error {
    m.mu.Lock()
    defer m.mu.Unlock()

    if _, ok := m.books[id]; !ok {
        return fmt.Errorf("book not found: %s", id)
    }
    delete(m.books, id)
    return nil
}
```

### internal/handler/handler.go

```go
package handler

import (
    "encoding/json"
    "net/http"

    "go.uber.org/zap" // Dependencia externa: logging

    "github.com/user/bookshelf/internal/model"
    "github.com/user/bookshelf/internal/store"
)

// Handler manages HTTP endpoints.
type Handler struct {
    store  *store.Memory
    logger *zap.SugaredLogger
}

// New creates a new Handler.
func New(s *store.Memory, logger *zap.SugaredLogger) *Handler {
    return &Handler{store: s, logger: logger}
}

func (h *Handler) ListBooks(w http.ResponseWriter, r *http.Request) {
    books := h.store.List()
    h.logger.Infow("listing books", "count", len(books))
    writeJSON(w, http.StatusOK, books)
}

func (h *Handler) CreateBook(w http.ResponseWriter, r *http.Request) {
    var req model.CreateBookRequest
    if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
        writeJSON(w, http.StatusBadRequest, map[string]string{"error": "invalid json"})
        return
    }

    if err := req.Validate(); err != nil {
        writeJSON(w, http.StatusBadRequest, map[string]string{"error": err.Error()})
        return
    }

    book := h.store.Create(req)
    h.logger.Infow("book created",
        "id", book.ID,
        "title", book.Title,
        "author", book.Author,
    )
    writeJSON(w, http.StatusCreated, book)
}

func (h *Handler) GetBook(w http.ResponseWriter, r *http.Request) {
    id := r.PathValue("id")

    book, err := h.store.Get(id)
    if err != nil {
        writeJSON(w, http.StatusNotFound, map[string]string{"error": err.Error()})
        return
    }
    writeJSON(w, http.StatusOK, book)
}

func (h *Handler) DeleteBook(w http.ResponseWriter, r *http.Request) {
    id := r.PathValue("id")

    if err := h.store.Delete(id); err != nil {
        writeJSON(w, http.StatusNotFound, map[string]string{"error": err.Error()})
        return
    }

    h.logger.Infow("book deleted", "id", id)
    w.WriteHeader(http.StatusNoContent)
}

func (h *Handler) Health(w http.ResponseWriter, r *http.Request) {
    writeJSON(w, http.StatusOK, map[string]string{"status": "ok"})
}

func writeJSON(w http.ResponseWriter, status int, data any) {
    w.Header().Set("Content-Type", "application/json")
    w.WriteHeader(status)
    json.NewEncoder(w).Encode(data)
}
```

```
PUNTOS CLAVE DEL PROGRAMA

1. DEPENDENCIAS DIRECTAS (2):
   github.com/google/uuid  → generar IDs unicos
   go.uber.org/zap          → logging estructurado de alto rendimiento

2. DEPENDENCIAS INDIRECTAS (1):
   go.uber.org/multierr     → requerido por zap internamente
   Marcado "// indirect" en go.mod — no lo importamos nosotros

3. IMPORTS AGRUPADOS (3 grupos):
   - stdlib (net/http, encoding/json, etc.)
   - externos (go.uber.org/zap, github.com/google/uuid)
   - internos (github.com/user/bookshelf/internal/...)

4. INTERNAL CORRECTAMENTE USADO:
   handler, model, store → detalles de implementacion
   Solo cmd/ los usa directamente

5. WORKFLOW PARA CREAR ESTO:
   mkdir bookshelf && cd bookshelf
   go mod init github.com/user/bookshelf
   # Escribir codigo con imports...
   go mod tidy          # Descarga uuid y zap, actualiza go.mod/go.sum
   go build ./...       # Verificar que compila
   go vet ./...         # Verificar errores comunes
   go test ./...        # Correr tests

6. WORKFLOW PARA ACTUALIZAR DEPENDENCIAS:
   go list -m -u all                    # Ver updates disponibles
   go get -u go.uber.org/zap           # Actualizar zap
   go get -u=patch ./...               # Actualizar todo (solo patches)
   go mod tidy                          # Limpiar
   go test ./...                        # Verificar que nada se rompio
   govulncheck ./...                    # Verificar vulnerabilidades
```

---

## 16. Ejercicios

### Ejercicio 1: Crear un modulo con dependencias
Crea un proyecto `weathercli` que:
- Use `go mod init` con un module path adecuado
- Importe `github.com/fatih/color` para output colorizado
- Importe `github.com/joho/godotenv` para cargar config de `.env`
- Lea una ciudad de `os.Args` y simule mostrar el clima (no necesitas API real)
- Ejecuta `go mod tidy` y examina el go.mod y go.sum resultantes
- Ejecuta `go mod graph` y `go mod why` para cada dependencia
- Actualiza `fatih/color` a la ultima version con `go get -u`

### Ejercicio 2: Workspace multi-modulo
Crea un workspace con 2 modulos:
```
workspace/
├── go.work
├── mathlib/      (module example.com/mathlib — exporta Add, Multiply)
└── calculator/   (module example.com/calculator — importa mathlib)
```
- Inicializa cada modulo con `go mod init`
- Crea `go.work` con `go work init`
- Verifica que calculator puede importar mathlib sin `replace`
- Modifica mathlib (agrega funcion Divide) y verifica que calculator la ve inmediatamente
- Ejecuta `GOWORK=off go build ./...` en calculator y observa el error

### Ejercicio 3: Replace para development
Simula un escenario de fix de dependencia:
- Crea un modulo `logger` con una funcion `Log(msg string)` que imprime a stdout
- Crea un modulo `app` que importa `logger` con una version tagged (v1.0.0)
- "Descubre un bug" en logger: Log no agrega newline
- Usa `replace` en `app/go.mod` para apuntar a la copia local de logger
- Arregla el bug en la copia local y verifica que app usa la version corregida
- Explica (en un comentario) por que deberias quitar el replace antes de publicar

### Ejercicio 4: Vendoring y audit
Con el proyecto del ejercicio 1:
- Ejecuta `go mod vendor` y examina el directorio vendor/
- Verifica que `go build -mod=vendor ./...` funciona
- Instala `govulncheck` y ejecutalo sobre tu proyecto
- Lista todas las licencias con `go-licenses report ./...` (o manualmente revisando vendor/)
- Crea un Dockerfile multi-stage que use vendor/ para el build

---

> **Fin de C09/S01 — Sistema de Paquetes** (3/3 topicos completados). El siguiente topico inicia la Seccion 2: I/O.
>
> **Siguiente**: C09/S02 - I/O, T01 - io.Reader y io.Writer — la abstraccion central, composicion, io.Copy, io.TeeReader
