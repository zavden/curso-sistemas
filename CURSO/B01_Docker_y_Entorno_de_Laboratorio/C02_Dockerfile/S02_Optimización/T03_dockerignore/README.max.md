# T03 — .dockerignore

## El contexto de build

Cuando ejecutas `docker build .`, Docker empaqueta **todo el contenido del
directorio** (el contexto de build) en un archivo tar y lo envía al daemon.
Solo después de recibir el contexto, el daemon empieza a procesar el Dockerfile.

```
$ docker build -t myapp .
Sending build context to Docker daemon  250MB   ← TODO el directorio
Step 1/5 : FROM python:3.12-slim
...
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
┌─────────────────────────────────────────────────────────────────┐
│                      TU MÁQUINA                                 │
│  ┌─────────────────┐                                          │
│  │  docker build .  │  1. Empaqueta contexto ────────────┐    │
│  │  (CLI)           │──────────────────────────────────────│───►│
│  └─────────────────┘                                      │    │
│                                                             │    │
│  Contexto:                                                  │    │
│  ./Dockerfile        ┌─────────────────────────────────┐   │    │
│  ./src/              │  context.tar (250MB)              │   │    │
│  ./node_modules/    │  ├── src/                         │   │    │
│  ./.git/            │  ├── node_modules/                 │   │    │
│  ./file.iso (1GB)   │  ├── .git/                        │   │    │
│                      │  └── file.iso                    │   │    │
│                      └─────────────────────────────────┘   │    │
└─────────────────────────────────────────────────────────────│────┘
                                                              │
                                                              ▼
                                                   ┌─────────────────┐
                                                   │  Docker Daemon  │
                                                   │  (construye la  │
                                                   │   imagen)       │
                                                   └─────────────────┘
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

La sintaxis es similar a `.gitignore`:

| Patrón | Significado |
|---|---|
| `file.txt` | Excluye `file.txt` en la raíz |
| `*.log` | Excluye todos los `.log` en la raíz |
| `**/*.log` | Excluye todos los `.log` en cualquier nivel |
| `dir/` | Excluye el directorio `dir` completo |
| `*/temp*` | Excluye archivos que empiezan con `temp` en subdirectorios de un nivel |
| `**/temp` | Excluye `temp` en cualquier nivel (directorio o archivo) |
| `dir/*.txt` | Excluye `.txt` dentro de `dir/` |
| `dir/**/*.txt` | Excluye `.txt` en cualquier lugar dentro de `dir/` |
| `!important.log` | **Incluye** `important.log` aunque un patrón anterior lo excluya |
| `#` | Comentario (línea ignorada) |
| `\` | Continuación de línea (para patrones largos) |

### Patrones avanzados

```
# Excluir todo dentro de logs/, pero no logs/ mismo
logs/*

# Excluir todos los archivos .tmp en cualquier lugar
**/*.tmp

# Excluir node_modules en cualquier lugar
**/node_modules

# Excluir archivos que terminan en .debug
**/*.debug

# Solo en la raíz
/*.log

# Excluir todo, luego incluir algo
*
!src/
!package.json
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
Sending build context to Docker daemon  1.2GB   ← 1.2 GB transferidos

# Con .dockerignore (excluyendo .git, node_modules, etc.)
$ docker build -t test .
Sending build context to Docker daemon  15MB    ← 80x menos
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

```bash
# Mostrar qué archivos se excluyen
docker build --progress=plain -t debug . 2>&1 | grep -E '^# ' | head -20

# Con BuildKit, ver el contexto transmitido
docker build --progress=plain --no-cache -t debug . 2>&1 | grep -E '^\[.*\]'

# Listar archivos que se envían (usando un stage temporal)
cat > Dockerfile.debug << 'EOF'
FROM alpine
WORKDIR /src
COPY . /src
RUN find /src -type f | head -100
EOF
docker build -f Dockerfile.debug -t debug-files .
docker run --rm debug-files
```

## Errores comunes

### 1. No excluir `Dockerfile`

```dockerignore
# INCORRECTO
*.txt
Dockerfile  # ¡Esto no funciona si el archivo se llama Dockerfile!

# CORRECTO (dockerignore busca en raíz por nombre)
Dockerfile
```

El patrón debe ser exactamente el nombre del archivo.

### 2. Excluir demasiado con `!`

```dockerignore
# MAL: el orden causa que README.md se excluya
!README.md
*.md

# BIEN:
*.md
!README.md
```

### 3. No entender `**/`

```dockerignore
# Solo excluye node_modules en la raíz
node_modules

# Excluye node_modules en cualquier lugar
**/node_modules
```

### 4. .dockerignore dentro de subdirectorios

`.dockerignore` solo funciona en la **raíz del contexto**. Un `.dockerignore`
en un subdirectorio no tiene efecto.

### 5. Archivos con el mismo nombre que .dockerignore

```dockerignore
# Este patrón NO excluye el archivo .dockerignore en la raíz
.dockerignore

# Se excluye así:
.dockerignore
```

## Casos borde

### .dockerignore vacío

```bash
# Un archivo .dockerignore vacío = no se excluye nada
touch .dockerignore
docker build ...  # Todo se envía al contexto
```

### Archivo .dockerignore que no existe

```bash
# Si .dockerignore no existe, Docker continúa sin error
rm .dockerignore
docker build ...  # Funciona, pero envía todo
```

### Patrones con rutas

Los patrones en `.dockerignore` son relativos a la raíz del contexto, **no**
al directorio donde está el Dockerfile:

```dockerignore
# Esto excluye /app/src en la máquina de build, no /src
/app/src

# Esto excluye src/ en cualquier lugar
**/src
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

## Ejercicios

### Ejercicio 1 — Medir el contexto sin y con .dockerignore

```bash
mkdir -p /tmp/docker_ignore && cd /tmp/docker_ignore

# Crear archivos simulados
mkdir -p node_modules/.cache
dd if=/dev/zero of=node_modules/.cache/big_file bs=1M count=50 2>/dev/null
mkdir -p .git/objects
dd if=/dev/zero of=.git/objects/pack bs=1M count=30 2>/dev/null
echo 'print("app")' > app.py
echo 'DB_PASS=secret' > .env

cat > Dockerfile << 'EOF'
FROM alpine:latest
COPY . /app
CMD ["ls", "-la", "/app"]
EOF

# Sin .dockerignore: envía ~80 MB de contexto
docker build -t no-ignore . 2>&1 | grep "Sending build context"

# Con .dockerignore
cat > .dockerignore << 'EOF'
.git
node_modules
.env
EOF

# Con .dockerignore: envía solo unos KB
docker build -t with-ignore . 2>&1 | grep "Sending build context"

# Verificar que .env no está en la imagen
docker run --rm with-ignore cat /app/.env 2>&1 || echo ".env no incluido"

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
CMD ["ls", "/app"]
EOF

cat > .dockerignore << 'EOF'
*.md
!README.md
EOF

docker build -t except-test .
docker run --rm except-test
# Muestra: Dockerfile app.py README.md
# NOTES.md y CHANGELOG.md excluidos, README.md incluido por !

docker rmi except-test
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
# Archivos no encontrados — excluidos del contexto

docker rmi leak-test safe-test
rm -rf /tmp/docker_secrets
```

### Ejercicio 4 — Patrones con **/

```bash
mkdir -p /tmp/docker_pattern && cd /tmp/docker_pattern

mkdir -p src/utils temp/nested
echo 'code' > src/main.py
echo 'utils' > src/utils/helper.py
echo 'temp file' > temp/nested/tmp.txt
echo 'keep' > temp/nested/keep.txt

cat > Dockerfile << 'EOF'
FROM alpine:latest
COPY . /app
CMD ["find", "/app", "-type", "f"]
EOF

cat > .dockerignore << 'EOF'
**/tmp*
**/__pycache__
EOF

docker build -t pattern-test .
docker run --rm pattern-test
# Muestra solo: /app/src/main.py, /app/temp/nested/keep.txt
# tmp.txt y __pycache__ excluidos en cualquier nivel

docker rmi pattern-test
rm -rf /tmp/docker_pattern
```

### Ejercicio 5 — Invalidación de caché

```bash
mkdir -p /tmp/docker_cache && cd /tmp/docker_cache

echo 'print("v1")' > app.py
mkdir -p .git/objects
dd if=/dev/zero of=.git/objects/pack bs=1M count=10 2>/dev/null

cat > Dockerfile << 'EOF'
FROM alpine:latest
COPY . /app
CMD ["cat", "/app/app.py"]
EOF

# Primer build
docker build -t cache-test .

# Segundo build sin cambios
echo "=== Sin modificar nada ==="
docker build -t cache-test . 2>&1 | grep -E 'CACHED|COPY'

# Modificar solo .git (con .dockerignore, esto no afecta)
dd if=/dev/zero of=.git/objects/pack bs=1M count=15 2>/dev/null

# Agregar .git a .dockerignore
cat > .dockerignore << 'EOF'
.git
EOF

echo "=== Con .git excluido ==="
docker build -t cache-test . 2>&1 | grep -E 'CACHED|COPY'
# COPY debería estar CACHED porque solo cambió .git

docker rmi cache-test
rm -rf /tmp/docker_cache
```

---

## Checklist de .dockerignore

- [ ] `.git/` excluido
- [ ] `node_modules/` excluido (si aplica)
- [ ] Archivos `.env` y secrets excluidos
- [ ] `Dockerfile` y `docker-compose*.yml` excluidos
- [ ] `*.log` y `tmp/` excluidos
- [ ] Orden correcto de patrones con `!`
- [ ] Probado que los archivos sensibles no están en la imagen
- [ ] `docker build` muestra contexto pequeño (~MB, no GB)
