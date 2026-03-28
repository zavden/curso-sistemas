# Lab — .dockerignore

## Objetivo

Laboratorio práctico para entender el impacto de `.dockerignore` en el build
de Docker: reducción del tamaño del contexto, protección contra secrets en la
imagen, uso del operador `!` para excepciones, y prevención de invalidación
de caché innecesaria.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada: `docker pull alpine:latest`
- Estar en el directorio `labs/` de este tópico

## Archivos del laboratorio

| Archivo | Propósito |
|---|---|
| `app.py` | Script Python simple (simula código fuente) |
| `Dockerfile.context` | Dockerfile básico con COPY . (para todas las partes) |

Los archivos de prueba (`.env`, `node_modules/`, `.git/`, `.dockerignore`)
se crean y eliminan durante el lab.

---

## Parte 1 — Contexto de build

### Objetivo

Demostrar que Docker envía **todo el contenido del directorio** al daemon
como contexto de build, y que `.dockerignore` reduce dramáticamente este
tamaño.

### Paso 1.1: Crear archivos pesados simulados

```bash
mkdir -p node_modules/.cache .git/objects
dd if=/dev/zero of=node_modules/.cache/big_file bs=1M count=50 2>/dev/null
dd if=/dev/zero of=.git/objects/pack bs=1M count=30 2>/dev/null
```

Ahora el directorio tiene ~80MB de archivos que nunca se necesitan en la
imagen.

### Paso 1.2: Build sin .dockerignore

```bash
docker build -f Dockerfile.context -t lab-no-ignore . 2>&1 | head -3
```

Salida esperada (aproximada):

```
Sending build context to Docker daemon  ~84MB
```

Docker envió ~84MB al daemon, incluyendo `node_modules/` y `.git/`.

### Paso 1.3: Ver qué hay en la imagen

```bash
docker run --rm lab-no-ignore
```

Verás `node_modules/`, `.git/`, y todos los archivos de prueba dentro de
`/app/`. Están en la imagen aunque no los necesitas.

### Paso 1.4: Crear .dockerignore

```bash
cat > .dockerignore << 'EOF'
node_modules
.git
Dockerfile*
.dockerignore
EOF
```

### Paso 1.5: Build con .dockerignore

```bash
docker build -f Dockerfile.context -t lab-with-ignore . 2>&1 | head -3
```

Salida esperada:

```
Sending build context to Docker daemon  ~5KB
```

De ~84MB a ~5KB. El `.dockerignore` excluyó `node_modules/` y `.git/` del
contexto.

### Paso 1.6: Verificar la imagen

```bash
docker run --rm lab-with-ignore
```

Solo aparecen `app.py` y los archivos necesarios. Sin `node_modules/` ni
`.git/`.

### Paso 1.7: Limpiar

```bash
docker rmi lab-no-ignore lab-with-ignore
rm -f .dockerignore
```

---

## Parte 2 — Secrets en la imagen

### Objetivo

Demostrar que sin `.dockerignore`, un `COPY . .` puede incluir archivos
sensibles (`.env`, credenciales) en la imagen, accesibles para cualquiera
que tenga la imagen.

### Paso 2.1: Crear archivos sensibles simulados

```bash
echo 'DATABASE_URL=postgres://admin:SuperSecret123@db:5432/prod' > .env
echo '{"api_key": "sk-abc123xyz789", "secret": "do-not-share"}' > credentials.json
echo 'private_key_content_here' > server.key
```

### Paso 2.2: Build sin .dockerignore

```bash
docker build -f Dockerfile.context -t lab-leak .
```

### Paso 2.3: Extraer secrets de la imagen

```bash
echo "=== .env ==="
docker run --rm lab-leak cat /app/.env

echo "=== credentials.json ==="
docker run --rm lab-leak cat /app/credentials.json

echo "=== server.key ==="
docker run --rm lab-leak cat /app/server.key
```

Todos los secrets son visibles. Si esta imagen se publica en Docker Hub,
cualquiera puede extraerlos.

### Paso 2.4: Proteger con .dockerignore

```bash
cat > .dockerignore << 'EOF'
.env
.env.*
*.key
*.pem
credentials*
secrets*
node_modules
.git
Dockerfile*
.dockerignore
EOF
```

```bash
docker build -f Dockerfile.context -t lab-safe .
```

### Paso 2.5: Verificar que los secrets no están

```bash
docker run --rm lab-safe sh -c '
    echo "=== .env ===" && cat /app/.env 2>&1
    echo "=== credentials.json ===" && cat /app/credentials.json 2>&1
    echo "=== server.key ===" && cat /app/server.key 2>&1
'
```

Los archivos no existen en la imagen. `.dockerignore` los excluyó del
contexto de build.

### Paso 2.6: Limpiar

```bash
docker rmi lab-leak lab-safe
rm -f .env credentials.json server.key .dockerignore
```

---

## Parte 3 — Operador ! (excepciones)

### Objetivo

Demostrar el operador `!` que permite re-incluir archivos que fueron
excluidos por un patrón más amplio.

### Paso 3.1: Crear archivos de prueba

```bash
echo 'README content' > README.md
echo 'API docs' > API.md
echo 'Changelog' > CHANGELOG.md
echo 'Contributing guide' > CONTRIBUTING.md
```

### Paso 3.2: Excluir todos los .md excepto README.md

```bash
cat > .dockerignore << 'EOF'
node_modules
.git
Dockerfile*
.dockerignore
*.md
!README.md
EOF
```

Antes de construir, responde mentalmente:

- ¿Aparecerá README.md en la imagen?
- ¿Aparecerá API.md?
- ¿Aparecerá CHANGELOG.md?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.3: Construir y verificar

```bash
docker build -f Dockerfile.context -t lab-except .
docker run --rm lab-except
```

Salida esperada: solo `README.md` aparece de los archivos `.md`. Los demás
(`API.md`, `CHANGELOG.md`, `CONTRIBUTING.md`) fueron excluidos.

### Paso 3.4: El orden importa

```bash
cat > .dockerignore << 'EOF'
node_modules
.git
!README.md
*.md
EOF
```

```bash
docker build -f Dockerfile.context -t lab-except .
docker run --rm lab-except
```

Ahora README.md **tampoco** aparece. El patrón `*.md` se evalúa después de
`!README.md` y lo vuelve a excluir.

**Regla**: el operador `!` debe ir **después** del patrón que excluye.

### Paso 3.5: Limpiar

```bash
docker rmi lab-except
rm -f README.md API.md CHANGELOG.md CONTRIBUTING.md .dockerignore
```

---

## Parte 4 — .git y la caché

### Objetivo

Demostrar que `.git/` causa invalidación innecesaria de la caché de
`COPY . .` porque cada commit modifica archivos dentro de `.git/`.

### Paso 4.1: Simular un repositorio

```bash
mkdir -p .git/objects
echo "initial" > .git/HEAD
```

### Paso 4.2: Build inicial

```bash
docker build -f Dockerfile.context -t lab-cache-test . 2>&1 | grep -E 'CACHED|COPY'
```

### Paso 4.3: Simular un commit (sin cambiar el código)

```bash
echo "after-commit" > .git/HEAD
```

`app.py` no cambió, pero `.git/HEAD` sí.

### Paso 4.4: Rebuild sin .dockerignore

```bash
docker build -f Dockerfile.context -t lab-cache-test . 2>&1 | grep -E 'CACHED|COPY'
```

`COPY . .` se **invalida** porque el contenido de `.git/` cambió. Todas las
capas posteriores se reconstruyen.

### Paso 4.5: Agregar .dockerignore y repetir

```bash
cat > .dockerignore << 'EOF'
.git
node_modules
Dockerfile*
.dockerignore
EOF

# Rebuild para establecer caché
docker build -f Dockerfile.context -t lab-cache-test .

# Simular otro commit
echo "another-commit" > .git/HEAD

# Rebuild con .dockerignore
docker build -f Dockerfile.context -t lab-cache-test . 2>&1 | grep -E 'CACHED|COPY'
```

Ahora `COPY . .` está en `CACHED` porque `.git/` no se envía al contexto.
Los cambios en `.git/` no invalidan la caché.

### Paso 4.6: Limpiar

```bash
docker rmi lab-cache-test
rm -rf .git node_modules .dockerignore
```

---

## Limpieza final

```bash
# Eliminar imágenes del lab (si alguna quedó)
docker rmi lab-no-ignore lab-with-ignore lab-leak lab-safe \
    lab-except lab-cache-test 2>/dev/null

# Eliminar archivos de prueba
rm -rf node_modules .git .env credentials.json server.key \
    README.md API.md CHANGELOG.md CONTRIBUTING.md .dockerignore
```

---

## Conceptos reforzados

1. Docker envía **todo el directorio** como contexto de build al daemon.
   Sin `.dockerignore`, archivos pesados como `node_modules/` y `.git/`
   ralentizan el build innecesariamente.

2. Un `COPY . .` sin `.dockerignore` puede incluir **secrets** (`.env`,
   claves, credenciales) en la imagen. Cualquiera con acceso a la imagen
   puede extraerlos.

3. El operador `!` permite re-incluir archivos excluidos por un patrón
   más amplio. El **orden importa**: `!` debe ir después del patrón que
   excluye.

4. `.git/` causa **invalidación de caché** en cada commit porque sus
   archivos internos cambian. Excluirlo con `.dockerignore` preserva
   la caché de `COPY . .`.

5. Siempre crear un `.dockerignore` al empezar un proyecto Docker. La
   plantilla mínima debe excluir: `.git`, `node_modules`/`venv`/`target`,
   `.env`, `*.key`, `*.pem`, `Dockerfile*`, `.dockerignore`.
