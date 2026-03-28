# T03 — .dockerignore

## El contexto de build

Cuando ejecutas `docker build .`, Docker empaqueta **todo el contenido del
directorio** (el contexto de build) y lo envía al daemon. Solo después de
recibir el contexto, el daemon empieza a procesar el Dockerfile.

```
$ docker build -t myapp .
[internal] load build context
transferring context: 250MB   ← TODO el directorio
```

Si tu directorio contiene `.git/`, `node_modules/`, binarios de build, datasets,
o archivos de log, Docker los envía al daemon aunque nunca los uses en el
Dockerfile. Esto:

1. **Hace el build más lento** (transferencia innecesaria)
2. **Puede incluir archivos sensibles** en la imagen (`.env`, secrets, credentials)
3. **Invalida la caché** innecesariamente (un cambio en `.git/` invalida `COPY . .`)

### ¿Por qué se envía al daemon?

El daemon de Docker (no el CLI) ejecuta los pasos del build. Por eso necesita
tener acceso a todos los archivos del contexto:

```
┌──────────────────────────────────────────────────────┐
│                    TU MÁQUINA                         │
│                                                       │
│  docker build .        Contexto:                      │
│  (CLI)                 ./src/                         │
│       │                ./node_modules/  (500 MB)      │
│       │                ./.git/          (100 MB)      │
│       │                ./file.iso       (1 GB)        │
│       ▼                                               │
│  ┌─────────────────────────────────┐                  │
│  │  context.tar (1.6 GB)           │                  │
│  │  Incluye TODO el directorio     │──────────────►   │
│  └─────────────────────────────────┘                  │
│                                          Docker Daemon│
│                                          (construye   │
│                                           la imagen)  │
└──────────────────────────────────────────────────────┘
```

## .dockerignore

El archivo `.dockerignore` funciona como `.gitignore`: lista patrones de archivos
y directorios que se excluyen del contexto de build. Se coloca en la raíz del
contexto (junto al Dockerfile).

```
# .dockerignore

# Control de versiones
.git
.gitignore

# Dependencias (se instalan en el build)
node_modules
__pycache__
venv
.venv
target

# Artefactos de build
*.o
*.pyc
build/
dist/

# Docker
Dockerfile
Dockerfile.*
docker-compose*.yml
.dockerignore

# IDE y editor
.vscode
.idea
*.swp
*.swo
*~

# Archivos sensibles
.env
.env.*
*.key
*.pem
credentials.json

# Logs y datos temporales
*.log
tmp/
```

## Sintaxis

La sintaxis es similar a `.gitignore`, pero con diferencias importantes:

| Patrón | Significado |
|---|---|
| `file.txt` | Excluye `file.txt` en la raíz del contexto |
| `*.log` | Excluye todos los `.log` en la raíz |
| `**/*.log` | Excluye todos los `.log` en cualquier nivel |
| `dir` | Excluye el directorio o archivo `dir` en la raíz |
| `*/temp*` | Excluye archivos que empiezan con `temp` en subdirectorios de un nivel |
| `**/temp` | Excluye `temp` en cualquier nivel (directorio o archivo) |
| `dir/*.txt` | Excluye `.txt` dentro de `dir/` |
| `dir/**/*.txt` | Excluye `.txt` en cualquier lugar dentro de `dir/` |
| `!important.log` | **Incluye** `important.log` aunque un patrón anterior lo excluya |
| `#` | Comentario (línea ignorada) |

**Nota**: a diferencia de `.gitignore`, `.dockerignore` **no** soporta
continuación de línea con `\`. Cada patrón debe estar en una sola línea.

### Patrones avanzados

```
# Excluir todo dentro de logs/, pero no logs/ mismo
logs/*

# Excluir todos los archivos .tmp en cualquier lugar
**/*.tmp

# Excluir node_modules en cualquier lugar (no solo raíz)
**/node_modules

# Solo en la raíz (explícito)
/*.log

# Excluir todo, luego incluir selectivamente
*
!src/
!package.json
!package-lock.json
```

### La excepción `!`

El operador `!` invierte una exclusión. Se evalúa en orden de arriba a abajo:

```
# Excluir todos los .md
*.md

# Pero incluir README.md
!README.md
```

El orden importa:

```
# Esto NO funciona como esperas:
!README.md    ← se evalúa primero: incluye README.md
*.md          ← se evalúa después: excluye TODOS los .md, incluyendo README.md

# Esto SÍ funciona:
*.md          ← excluye todos los .md
!README.md    ← re-incluye README.md
```

## Impacto en el tamaño del contexto

```bash
# Sin .dockerignore
$ du -sh .
1.2G  .

$ docker build -t test .
# BuildKit: transferring context: 1.2GB

# Con .dockerignore (excluyendo .git, node_modules, etc.)
$ docker build -t test .
# BuildKit: transferring context: 15MB    ← 80x menos
```

En un proyecto Node.js típico, `node_modules/` puede ocupar cientos de MB.
En un repositorio con historial largo, `.git/` puede ser más grande que el
código fuente.

## `.dockerignore` y `COPY . .`

`.dockerignore` se evalúa **antes** de que `COPY` vea los archivos. Los archivos
excluidos no solo no se copian — **no se envían al daemon** en absoluto:

```dockerfile
COPY . /app/
# Solo copia los archivos que NO están en .dockerignore
```

Sin `.dockerignore`:

```bash
docker run --rm myimage ls /app/
# .git/  node_modules/  .env  src/  package.json  ...
# ¡Todo está en la imagen!
```

Con `.dockerignore`:

```bash
docker run --rm myimage ls /app/
# src/  package.json  package-lock.json
# Solo lo necesario
```

## Archivos sensibles en la imagen

Sin `.dockerignore`, un `COPY . .` descuidado puede incluir secrets en la imagen:

```bash
# Sin .dockerignore
docker build -t myapp .
docker run --rm myapp cat /app/.env
# DATABASE_URL=postgres://admin:password@db:5432/prod
# SECRET_KEY=abc123xyz
# ¡Secrets expuestos en la imagen!
```

Cualquiera que tenga acceso a la imagen puede extraer estos archivos. Si la
imagen se publica en Docker Hub, los secrets son públicos.

Incluso con `.dockerignore`, verificar que no se incluyan secrets:

```bash
# Inspeccionar qué hay en la imagen
docker run --rm myimage find /app -name "*.env" -o -name "*.key" -o -name "*.pem"
```

## `.git` en la imagen: un riesgo oculto

El directorio `.git/` contiene el historial completo del repositorio, incluyendo
commits antiguos que podrían haber contenido secrets:

```bash
# Sin .dockerignore que excluya .git:
docker run --rm myimage sh -c 'cd /app && git log --all --oneline | head'
# abc1234 fix: remove hardcoded password
# def5678 add database credentials
# ...

docker run --rm myimage sh -c 'cd /app && git show def5678:config.json'
# { "db_password": "super_secret_password" }
# ¡El historial con secrets está en la imagen!
```

**Siempre excluir `.git`** en `.dockerignore`.

## Invalidación de caché por `.git`

Sin excluir `.git`, cada commit modifica archivos dentro de `.git/`, lo que
invalida la caché de `COPY . .` incluso si el código fuente no cambió:

```
Sin .dockerignore:
  git commit                    ← .git/ cambia
  docker build                  ← COPY . . se invalida (por .git/)
                                   Todo después de COPY se reconstruye

Con .dockerignore (excluye .git):
  git commit                    ← .git/ cambia pero no se envía
  docker build                  ← COPY . . usa caché (código no cambió)
```

## .dockerignore por Dockerfile

Docker soporta `.dockerignore` específicos por Dockerfile:

```
proyecto/
├── Dockerfile.dev
├── Dockerfile.dev.dockerignore   ← se usa con Dockerfile.dev
├── Dockerfile.prod
├── Dockerfile.prod.dockerignore  ← se usa con Dockerfile.prod
└── .dockerignore                 ← fallback general
```

```bash
# Usa Dockerfile.dev.dockerignore automáticamente
docker build -f Dockerfile.dev -t myapp:dev .
```

Esto permite incluir herramientas de testing en dev que se excluyen en prod.

## Debugging de .dockerignore

Para verificar qué archivos se envían al contexto, construir un Dockerfile
temporal que liste los archivos:

```bash
# Listar todos los archivos que llegan al contexto
docker build -f - -t debug-context . << 'EOF'
FROM alpine
WORKDIR /ctx
COPY . .
RUN find /ctx -type f | sort
EOF

docker run --rm debug-context
# Muestra exactamente qué archivos pasaron el filtro de .dockerignore

docker rmi debug-context
```

## Errores comunes

### 1. Orden incorrecto de `!`

```
# MAL: el orden causa que README.md se excluya
!README.md
*.md

# BIEN: la exclusión va primero, la excepción después
*.md
!README.md
```

### 2. No entender la diferencia entre `pattern` y `**/pattern`

```
# Solo excluye node_modules en la raíz del contexto
node_modules

# Excluye node_modules en cualquier nivel de profundidad
**/node_modules
```

Para proyectos con workspaces (monorepos), usar `**/node_modules` para
capturar node_modules en subdirectorios.

### 3. `.dockerignore` solo funciona en la raíz

`.dockerignore` solo funciona en la **raíz del contexto de build**. Un
`.dockerignore` dentro de un subdirectorio no tiene efecto.

### 4. Los patrones son relativos al contexto, no al Dockerfile

```
# .dockerignore
src/tests     ← excluye <contexto>/src/tests, NO /src/tests dentro del contenedor
```

## Plantilla recomendada

```
# .dockerignore — Plantilla general

# === Control de versiones ===
.git
.gitignore
.gitmodules

# === Docker ===
Dockerfile
Dockerfile.*
docker-compose*.yml
.dockerignore

# === Dependencias (se instalan en el build) ===
node_modules
vendor
__pycache__
*.pyc
venv
.venv
target
_build

# === Artefactos de build ===
*.o
*.a
*.so
*.dylib
build/
dist/
out/
*.egg-info

# === IDE y editores ===
.vscode
.idea
*.swp
*.swo
*~
.DS_Store
Thumbs.db

# === Archivos sensibles ===
.env
.env.*
*.key
*.pem
*.p12
*.jks
credentials*
secrets*

# === Documentación y tests (según necesidad) ===
# docs/
# tests/
# *.md

# === Logs y temporales ===
*.log
tmp/
temp/
*.tmp
```

---

**Nota**: el directorio `labs/` contiene un laboratorio completo con 4 partes.
Consultar `LABS.md` para la guía.

---

## Ejercicios

### Ejercicio 1 — Medir el contexto sin y con .dockerignore

```bash
mkdir -p /tmp/docker_ignore && cd /tmp/docker_ignore

# Crear archivos simulados
mkdir -p node_modules/.cache
dd if=/dev/zero of=node_modules/.cache/big_file bs=1M count=50 2>/dev/null
mkdir -p fakegit/objects
dd if=/dev/zero of=fakegit/objects/pack bs=1M count=30 2>/dev/null
echo 'print("app")' > app.py
echo 'DB_PASS=secret' > .env

cat > Dockerfile << 'EOF'
FROM alpine:latest
COPY . /app
CMD ["ls", "-la", "/app"]
EOF

# Sin .dockerignore: envía ~80 MB de contexto
echo "=== Sin .dockerignore ==="
docker build --progress=plain -t no-ignore . 2>&1 | grep -i "transferring context"
```

**Predicción**: sin `.dockerignore`, el contexto incluye los 80 MB de archivos
simulados. Con BuildKit verás `transferring context: ~80MB`.

```bash
# Con .dockerignore
cat > .dockerignore << 'EOF'
fakegit
node_modules
.env
EOF

echo "=== Con .dockerignore ==="
docker build --progress=plain -t with-ignore . 2>&1 | grep -i "transferring context"
# transferring context: unos pocos KB
```

**Predicción**: el contexto baja a unos KB. Solo se envían `app.py`,
`Dockerfile` y `.dockerignore`.

```bash
# Verificar que .env no está en la imagen
docker run --rm with-ignore cat /app/.env 2>&1 || echo ".env no incluido (correcto)"

docker rmi no-ignore with-ignore 2>/dev/null
rm -rf /tmp/docker_ignore
```

### Ejercicio 2 — El operador ! (excepción)

```bash
mkdir -p /tmp/docker_except && cd /tmp/docker_except

echo 'app code' > app.py
echo 'info' > README.md
echo 'notes' > NOTES.md
echo 'changes' > CHANGELOG.md

cat > Dockerfile << 'EOF'
FROM alpine:latest
COPY . /app
RUN ls /app
CMD ["echo", "done"]
EOF

cat > .dockerignore << 'EOF'
*.md
!README.md
EOF

docker build --progress=plain -t except-test .
```

**Predicción**: el `ls /app` en el build mostrará `Dockerfile`, `app.py` y
`README.md`. Los otros `.md` (`NOTES.md`, `CHANGELOG.md`) están excluidos
por `*.md`, pero `README.md` se re-incluye con `!README.md`.

```bash
# Verificar invirtiendo el orden
cat > .dockerignore << 'EOF'
!README.md
*.md
EOF

docker build --progress=plain -t except-bad .
```

**Predicción**: ahora `README.md` también se excluye porque `*.md` se evalúa
después de `!README.md`. El orden importa.

```bash
docker rmi except-test except-bad 2>/dev/null
rm -rf /tmp/docker_except
```

### Ejercicio 3 — Verificar que secrets no están en la imagen

```bash
mkdir -p /tmp/docker_secrets && cd /tmp/docker_secrets

echo 'print("app")' > app.py
echo 'SECRET_KEY=abc123' > .env
echo '{"api_key": "xyz789"}' > credentials.json

cat > Dockerfile << 'EOF'
FROM alpine:latest
COPY . /app
CMD ["echo", "built"]
EOF

# Sin .dockerignore: secrets en la imagen
docker build -t leak-test .
echo "=== Sin .dockerignore ==="
docker run --rm leak-test sh -c 'cat /app/.env; cat /app/credentials.json'
```

**Predicción**: ambos archivos sensibles están en la imagen. Cualquiera con
acceso a la imagen puede leerlos.

```bash
# Con .dockerignore
cat > .dockerignore << 'EOF'
.env
.env.*
credentials*
*.key
*.pem
EOF

docker build -t safe-test .
echo "=== Con .dockerignore ==="
docker run --rm safe-test sh -c 'cat /app/.env 2>&1; cat /app/credentials.json 2>&1'
# "No such file or directory" para ambos
```

**Predicción**: los archivos no existen en la imagen. Nunca se enviaron al
daemon — ni siquiera como capa intermedia que alguien podría extraer.

```bash
docker rmi leak-test safe-test
rm -rf /tmp/docker_secrets
```

### Ejercicio 4 — Patrones con **/ y verificación completa

```bash
mkdir -p /tmp/docker_pattern && cd /tmp/docker_pattern

mkdir -p src/utils logs/old
echo 'code' > src/main.py
echo 'utils' > src/utils/helper.py
echo 'log1' > app.log
echo 'log2' > logs/old/archive.log
echo 'keep' > logs/keep.txt

cat > Dockerfile << 'EOF'
FROM alpine:latest
COPY . /app
RUN find /app -type f | sort
CMD ["echo", "done"]
EOF

cat > .dockerignore << 'EOF'
**/*.log
Dockerfile
.dockerignore
EOF

docker build --progress=plain -t pattern-test . 2>&1 | grep "/app/"
```

**Predicción**: `**/*.log` excluye `.log` en cualquier nivel. Los archivos
que quedan en la imagen son:
- `/app/src/main.py`
- `/app/src/utils/helper.py`
- `/app/logs/keep.txt`

Los excluidos: `app.log` y `logs/old/archive.log` (por `**/*.log`),
`Dockerfile` y `.dockerignore` (explícitamente excluidos).

```bash
docker rmi pattern-test
rm -rf /tmp/docker_pattern
```

### Ejercicio 5 — Caché: con y sin .dockerignore

Este ejercicio demuestra cómo `.git` invalida la caché de COPY.

```bash
mkdir -p /tmp/docker_cache && cd /tmp/docker_cache

echo 'print("v1")' > app.py
mkdir -p fakegit
echo "commit1" > fakegit/HEAD

cat > Dockerfile << 'EOF'
FROM alpine:latest
RUN echo "step 1"
COPY . /app
RUN echo "step 2"
CMD ["cat", "/app/app.py"]
EOF

# Caso 1: SIN .dockerignore — modificar fakegit invalida COPY
echo "=== Build inicial (sin .dockerignore) ==="
docker build --progress=plain -t cache-test1 . 2>&1 | grep -E 'CACHED|COPY|step'

echo "=== Rebuild sin cambios ==="
docker build --progress=plain -t cache-test1 . 2>&1 | grep -E 'CACHED|COPY|step'
# Todo CACHED

echo "commit2" > fakegit/HEAD
echo "=== Rebuild tras modificar fakegit ==="
docker build --progress=plain -t cache-test1 . 2>&1 | grep -E 'CACHED|COPY|step'
# COPY se invalida aunque app.py no cambió
```

**Predicción**: modificar `fakegit/HEAD` invalida `COPY . /app` porque el
checksum del contexto cambió. Todo lo que va después de COPY se reconstruye.

```bash
# Caso 2: CON .dockerignore — modificar fakegit NO invalida COPY
cat > .dockerignore << 'EOF'
fakegit
EOF

echo "=== Build con .dockerignore ==="
docker build --progress=plain -t cache-test2 . 2>&1 | grep -E 'CACHED|COPY|step'

echo "commit3" > fakegit/HEAD
echo "=== Rebuild tras modificar fakegit (con .dockerignore) ==="
docker build --progress=plain -t cache-test2 . 2>&1 | grep -E 'CACHED|COPY|step'
# COPY está CACHED — fakegit no forma parte del contexto
```

**Predicción**: con `.dockerignore` excluyendo `fakegit`, el COPY usa caché
aunque `fakegit/HEAD` cambió, porque `fakegit` no se envía al contexto y no
afecta el checksum.

```bash
docker rmi cache-test1 cache-test2 2>/dev/null
rm -rf /tmp/docker_cache
```

### Ejercicio 6 — .dockerignore por Dockerfile

```bash
mkdir -p /tmp/docker_perfile && cd /tmp/docker_perfile

echo 'print("app")' > app.py
echo 'print("test")' > test_app.py
echo 'debug notes' > debug.txt

cat > Dockerfile.dev << 'EOF'
FROM alpine:latest
COPY . /app
RUN echo "Dev: incluye tests y debug"
RUN ls /app
CMD ["echo", "dev"]
EOF

cat > Dockerfile.prod << 'EOF'
FROM alpine:latest
COPY . /app
RUN echo "Prod: solo código"
RUN ls /app
CMD ["echo", "prod"]
EOF

# .dockerignore para dev: excluir poco
cat > Dockerfile.dev.dockerignore << 'EOF'
Dockerfile*
*.dockerignore
EOF

# .dockerignore para prod: excluir tests y debug
cat > Dockerfile.prod.dockerignore << 'EOF'
Dockerfile*
*.dockerignore
test_*
debug*
EOF

echo "=== Dev build ==="
docker build --progress=plain -f Dockerfile.dev -t perfile-dev . 2>&1 | grep "/app/"

echo "=== Prod build ==="
docker build --progress=plain -f Dockerfile.prod -t perfile-prod . 2>&1 | grep "/app/"
```

**Predicción**: el build con `Dockerfile.dev` incluye `app.py`, `test_app.py`
y `debug.txt`. El build con `Dockerfile.prod` solo incluye `app.py`.
Cada Dockerfile usa su propio `.dockerignore` automáticamente (por convención
de nombre `<Dockerfile>.<dockerignore>`).

```bash
docker rmi perfile-dev perfile-prod 2>/dev/null
rm -rf /tmp/docker_perfile
```

### Ejercicio 7 — Patrón "excluir todo, incluir selectivamente"

```bash
mkdir -p /tmp/docker_whitelist && cd /tmp/docker_whitelist

mkdir -p src config
echo 'print("app")' > src/app.py
echo '{}' > config/settings.json
echo 'secret' > .env
echo 'big data' > dataset.csv
echo 'notes' > README.md

cat > Dockerfile << 'EOF'
FROM alpine:latest
COPY . /app
RUN find /app -type f | sort
CMD ["echo", "done"]
EOF

# Enfoque whitelist: excluir TODO, incluir solo lo necesario
cat > .dockerignore << 'EOF'
*
!src/
!config/
!Dockerfile
EOF

docker build --progress=plain -t whitelist-test . 2>&1 | grep "/app/"
```

**Predicción**: con `*` se excluye todo. Las excepciones `!src/` y `!config/`
re-incluyen solo esos directorios. Los archivos `.env`, `dataset.csv`,
`README.md` quedan fuera. Este patrón es el más seguro: garantiza que solo
lo explícitamente listado entra al contexto.

```bash
docker rmi whitelist-test 2>/dev/null
rm -rf /tmp/docker_whitelist
```

### Ejercicio 8 — Inspeccionar el contexto con Dockerfile temporal

```bash
mkdir -p /tmp/docker_debug && cd /tmp/docker_debug

mkdir -p src vendor logs
echo 'code' > src/app.py
echo 'dep' > vendor/lib.py
echo 'log' > logs/app.log
echo 'secret' > .env

cat > Dockerfile << 'EOF'
FROM alpine:latest
COPY . /app
CMD ["echo", "app"]
EOF

cat > .dockerignore << 'EOF'
.env
*.log
vendor
EOF

# Usar un Dockerfile temporal para inspeccionar qué llega al contexto
docker build --progress=plain -f - -t debug-ctx . << 'INSPECT'
FROM alpine
WORKDIR /ctx
COPY . .
RUN echo "=== Archivos en el contexto ===" && find /ctx -type f | sort
INSPECT
```

**Predicción**: verás solo los archivos que pasan el filtro de `.dockerignore`:
`/ctx/Dockerfile`, `/ctx/.dockerignore`, `/ctx/src/app.py`. Los excluidos
(`.env`, `logs/app.log`, `vendor/lib.py`) no aparecen. Esta técnica es útil
para verificar que tu `.dockerignore` funciona correctamente antes de construir
la imagen real.

```bash
docker rmi debug-ctx 2>/dev/null
rm -rf /tmp/docker_debug
```

---

## Checklist de .dockerignore

- [ ] `.git/` excluido
- [ ] `node_modules/` excluido (si aplica)
- [ ] Archivos `.env` y secrets excluidos
- [ ] `Dockerfile` y `docker-compose*.yml` excluidos
- [ ] `*.log` y `tmp/` excluidos
- [ ] Orden correcto de patrones con `!` (exclusión antes de excepción)
- [ ] Probado que los archivos sensibles no están en la imagen
- [ ] Verificado el tamaño del contexto con `--progress=plain`
