# T02 — Registros

## ¿Qué es un registry?

Un **registry** (registro) es un servidor HTTP que almacena y distribuye imágenes de
contenedores siguiendo la especificación OCI (Open Container Initiative). Es el
equivalente a un repositorio de paquetes (como apt o dnf) pero para imágenes OCI.

Cuando ejecutas `docker pull nginx`, Docker:

1. Resuelve el nombre: `nginx` → `docker.io/library/nginx:latest`
2. Autenticarse con el registry (si es necesario)
3. Descarga el **manifiesto** de la imagen (JSON con metadata y referencias a capas)
4. Descarga solo las capas que no tiene localmente
5. Almacena la imagen en `/var/lib/docker/overlay2/`

### Registry vs Repository

| Concepto | Descripción | Ejemplo |
|----------|-------------|---------|
| **Registry** | El servidor HTTP que almacena imágenes | `docker.io`, `quay.io`, `ghcr.io` |
| **Repository** | Una colección de imágenes con el mismo nombre pero diferentes tags | `docker.io/library/nginx` |
| **Image** | Una imagen específica identificada por tag o digest | `nginx:1.25` |
| **Manifest** | JSON que describe la imagen (capas, arquitectura, config) | — |

Un registry puede contener muchos repositories:

```
docker.io/
├── library/nginx         ← repository (official)
├── library/postgres      ← repository (official)
├── myuser/myapp          ← repository (community)
└── myorg/web-service     ← repository (organization)
```

## Docker Hub

[Docker Hub](https://hub.docker.com) es el registry público por defecto de Docker.
Cuando no especificas un registry en el nombre de la imagen, Docker asume Docker Hub.

### Rate limits

Docker Hub impone límites de descarga para evitar abuso:

| Tipo de usuario | Límite | Período | Base del límite |
|----------------|--------|---------|-----------------|
| Anónimo | 100 pulls | 6 horas | IP compartida |
| Autenticado (free) | 200 pulls | 6 horas | Cuenta de usuario |
| Autenticado (Pro) | 5,000 pulls | 24 horas | Cuenta de usuario |
| Authenticated (Team) | 5,000 pulls | 24 horas | Cuenta de organización |

> **Importante**: los rate limits se cuentan por ventana deslizante, no por hora exacta.
> Además, pull de imágenes que ya tienes localmente no consume rate limit.

En entornos con IP compartida (oficinas, universidades, CI/CD con runners compartidos),
el límite anónimo se agota rápidamente. Autenticarse duplica el límite y lo vincula
a la cuenta en vez de la IP.

```bash
# Autenticarse en Docker Hub
docker login
# Username: tu_usuario
# Password: tu_token (NO la contraseña — usar un Personal Access Token)

# Verificar la autenticación
docker info | grep Username
# Username: tu_usuario
```

### Personal Access Token (PAT)

Por seguridad, Docker Hub ya no acepta contraseñas de cuenta para `docker login`.
Se debe usar un **Personal Access Token**:

1. Ir a [Docker Hub Settings → Security → New Access Token](https://hub.docker.com/settings/security)
2. Crear token con nombre descriptivo (ej: "ci-runner-prod")
3. Seleccionar permisos: `Read, Write, Delete` para push/pull, o `Read Only` para solo pull
4. Usar el token como contraseña en `docker login`

```bash
# Login con token
echo $DOCKER_TOKEN | docker login -u tu_usuario --password-stdin
# Más seguro que escribir el token interactivamente

# Ver tokens activos (sin ver los secretos)
docker hub token list  # Requiere hub CLI o web
```

## Nomenclatura de imágenes

La referencia completa de una imagen sigue este formato:

```
[registry/][namespace/]name[:tag][@sha256:digest]
```

### Componentes

| Componente | Ejemplo | Valor por defecto |
|-----------|---------|-------------------|
| `registry` | `docker.io`, `quay.io`, `ghcr.io` | `docker.io` (Docker Hub) |
| `namespace` | `library`, `myuser`, `myorg` | `library` (imágenes oficiales) |
| `name` | `nginx`, `postgres`, `myapp` | — (obligatorio) |
| `tag` | `latest`, `bookworm`, `15-alpine` | `latest` |
| `digest` | `sha256:a1b2c3...` | — (opcional) |

### Ejemplos resueltos

```
nginx                       → docker.io/library/nginx:latest
nginx:1.25                  → docker.io/library/nginx:1.25
myuser/myapp:v2             → docker.io/myuser/myapp:v2
quay.io/prometheus/node-exporter:v1.7.0
ghcr.io/my-org/my-tool:main
registry.example.com:5000/myimage:1.0   # registry con puerto custom
```

### El namespace `library/`

Las imágenes oficiales de Docker Hub están bajo el namespace `library/`, pero este se
omite por convención:

```bash
# Estas dos líneas son equivalentes:
docker pull nginx
docker pull docker.io/library/nginx:latest
```

Las imágenes bajo `library/` son mantenidas por Docker Inc. junto con los equipos
upstream del proyecto:

| Imagen | Mantenedor upstream |
|--------|---------------------|
| `library/nginx` | Nginx, Inc. |
| `library/postgres` | PostgreSQL Global Development Group |
| `library/python` | Python Software Foundation |
| `library/node` | OpenJS Foundation |

Tienen:
- Escaneo de seguridad automático en cada push
- Documentación estandarizada en Docker Hub
- Actualizaciones regulares de seguridad (patch rápido)
- Revisión de Dockerfiles por Docker Inc.
- Multi-arquitectura (amd64, arm64, arm/v7)

## Tags

Un **tag** es una etiqueta que apunta a una versión específica de una imagen. Es
un nombre legible que referencia un digest interno.

```bash
docker pull debian:bookworm     # Tag: bookworm
docker pull debian:12           # Tag: 12 (misma imagen que bookworm)
docker pull debian:12.5         # Tag: 12.5 (versión específica)
docker pull debian:latest       # Tag: latest
```

### Tags son mutables

Un tag puede reasignarse a una imagen diferente en cualquier momento. Cuando el equipo
de Debian publica una actualización de seguridad, el tag `debian:bookworm` se actualiza
para apuntar a la nueva imagen. El tag `debian:12.5` probablemente no cambiará, pero
`debian:12` sí (apuntará a 12.6 cuando salga).

```
Timeline de debian:12:
Jan:  debian:12 → sha256:aaaa (Debian 12.0)
Apr:  debian:12 → sha256:bbbb (Debian 12.1)
Aug:  debian:12 → sha256:cccc (Debian 12.2)
Nov:  debian:12 → sha256:dddd (Debian 12.3)

Timeline de debian:12.1:
Jan:  debian:12.1 → sha256:bbbb (Debian 12.1)
# Nunca cambia — tag congelado
```

### La trampa de `latest`

`latest` es un tag convencional, **no es un mecanismo automático**:

- `latest` no significa "la versión más reciente" — es solo un nombre de tag como
  cualquier otro
- Si el mantenedor de una imagen no actualiza el tag `latest`, puede estar desactualizado
- `docker pull image:latest` descarga la versión que el mantenedor haya taggeado como
  `latest`, que podría ser de hace meses
- Un `docker pull` anterior de `image:latest` **no se actualiza automáticamente** —
  tu copia local es la que descargaste, no la versión actual en el registry

```bash
# Tu local puede estar desactualizado
docker images | grep nginx
# nginx  latest  abc123  6 months ago  ...

# docker pull no sobreescribe si ya tienes la imagen — fuerza con --pull
docker pull --pull always nginx:latest
# Always pull 1.25.1: Pulling from library/nginx
# ...

# O elimina la imagen local primero
docker rmi nginx:latest
docker pull nginx:latest
```

**Regla profesional**: nunca usar `latest` en Dockerfiles de producción. Especificar
siempre un tag con versión (`nginx:1.25`, `debian:bookworm`, `python:3.12-slim`).

### Tags semánticos

Muchas imágenes usan versionado semántico:

| Tag | Significado | Estabilidad |
|-----|-------------|-------------|
| `nginx:1.25` | Versión menor específica | Estable |
| `nginx:1` | Latest de la serie 1.x | Puede cambiar (minor updates) |
| `nginx:mainline` | Latest mainline (1.27.x) | Inestable |
| `alpine:3.19` | Versión menor congelada | Muy estable |

## Digests

Un **digest** es el hash SHA256 del manifiesto de la imagen. A diferencia de un tag,
un digest es **inmutable**: siempre referencia exactamente la misma imagen.

```bash
# Ver el digest de una imagen local
docker images --digests | grep debian
# debian  bookworm  sha256:a1b2c3d4e5f6...  abc123def  3 weeks ago  120MB

# Hacer pull por digest (reproducibilidad garantizada)
docker pull debian@sha256:a1b2c3d4e5f6...
```

### Tag vs digest

| Característica | Tag | Digest |
|---------------|-----|--------|
| Legibilidad | `nginx:1.25` | `nginx@sha256:a1b2c3...` |
| Mutabilidad | Sí (puede cambiar) | No (inmutable) |
| Reproducibilidad | No garantizada | Garantizada |
| Verificación de integridad | No | Sí (hash del contenido) |
| Uso recomendado | Desarrollo | Producción, CI/CD |

En producción y CI/CD, usar digests (o al menos tags de versión específica) garantiza
que siempre se despliega exactamente la misma imagen.

### Cómo verificar digests

```bash
# El digest se calcula sobre el manifest, no las capas
docker inspect debian:bookworm --format '{{index .RepoDigests 0}}'
# debian@sha256:a1b2c3d4e5f6...

# Verificar integridad después de un pull
docker images --digests debian
# Compara el digest local con el del registry
docker manifest inspect debian:bookworm | grep digest
```

## Pull y Push

### `docker pull`

Descarga una imagen de un registry al almacenamiento local.

```bash
# Pull básico (desde Docker Hub)
docker pull nginx:1.25

# Pull desde otro registry
docker pull quay.io/prometheus/node-exporter:v1.7.0
docker pull ghcr.io/my-org/my-package:1.0

# Pull por digest
docker pull nginx@sha256:abc123...

# Pull de todas las tags de una imagen (rara vez necesario)
docker pull --all-tags nginx
```

La descarga es incremental: Docker solo descarga las capas que no existen localmente.
Si ya tienes `debian:bookworm` y haces pull de `python:3.12-bookworm` (que está basada
en Debian bookworm), las capas de Debian no se descargan de nuevo.

```
Pulling python:3.12-bookworm
a1b2c3d4e5f6: Already exists     ← Capa de Debian, ya la tenemos
b2c3d4e5f6a7: Already exists     ← Otra capa compartida
c3d4e5f6a7b8: Pull complete      ← Capa nueva (Python)
d4e5f6a7b8c9: Pull complete      ← Capa nueva (pip, setuptools)
```

### `docker push`

Sube una imagen local a un registry. Requiere autenticación.

```bash
# Loguearse primero
docker login docker.io

# Tagear una imagen local con tu namespace
docker tag myapp:latest myuser/myapp:v1.0

# Push al registry
docker push myuser/myapp:v1.0
# 1. Layer already exists
# 2. ...
# Pushing myuser/myapp:v1.0

# Ver la imagen en Docker Hub (si es pública)
# https://hub.docker.com/r/myuser/myapp
```

El push también es incremental: solo sube las capas que el registry no tiene.
Esto hace que pushes subsequentes sean muy rápidos si solo cambiaste capas superiores.

### `docker pull` no actualiza automáticamente

Si hace 3 meses hiciste `docker pull nginx:latest` y hoy usas esa imagen, estás
usando la versión de hace 3 meses. Docker **no verifica** si hay una versión más
nueva en el registry a menos que hagas `pull` explícitamente.

```bash
# Ver cuándo se descargó una imagen local
docker images | grep nginx
# nginx  latest  abc123  3 months ago  ...

# Forzar actualización
docker pull nginx:latest
# Descarga las capas nuevas (si las hay)
```

### Flujo de autenticación con el registry

Docker usa autenticación Bearer token con HTTP:

```
1. docker pull nginx
2. Registry responde: 401 Unauthorized + WWW-Authenticate header
   WWW-Authenticate: Bearer realm="https://auth.docker.io/token",
                     service="registry.docker.io",
                     scope="repository:library/nginx:pull"
3. Docker pide token a auth server
   GET https://auth.docker.io/token?service=...&scope=...
4. Auth server responde con JWT token
5. Docker repite request con Authorization: Bearer <token>
6. Registry sirve la imagen
```

## Imágenes oficiales vs community

### Imágenes oficiales (`library/`)

- Mantenidas por Docker Inc. + upstream maintainers
- Escaneo de seguridad automático y regular en cada push
- Documentación estándar en Docker Hub
- Dockerfiles públicos y auditables
- Variantes disponibles
- Ejemplos: `nginx`, `postgres`, `python`, `node`, `debian`, `ubuntu`, `alpine`

### Imágenes community (`user/image`)

- Mantenidas por usuarios individuales u organizaciones
- Sin garantía de calidad, seguridad o actualizaciones
- Pueden contener software malicioso, dependencias desactualizadas, o configuraciones inseguras
- Algunas son excelentes (organizaciones reconocidas como `elastic/`), otras son abandonware

### Imágenes verificadas

Docker Hub tiene un programa de "Docker Official Images" y "Verified Publisher" que
distingue imágenes confiables:

| Badge | Significado |
|-------|-------------|
| **Official Image** (checkmark azul) | Mantenido por Docker + upstream, auditado |
| **Verified Publisher** (checkmark verde) | Publisher verificado por Docker |

### Variantes de imágenes oficiales

Muchas imágenes tienen variantes:

```bash
# Variantes comunes
python:3.12            # Default (basada en Debian)
python:3.12-slim       # Menos paquetes (~50MB vs ~1GB), recomendada para producción
python:3.12-alpine     # Muy pequeña (~50MB), usa musl en vez de glibc
python:3.12-bookworm   # Explicita la base Debian
python:3.12-bullseye   # Debian 11 (más antigua)
python:3.12-windows    # Windows (requiere Docker Desktop en Windows)
```

| Variante | Tamaño aprox. | Base | Casos de uso |
|----------|--------------|------|-------------|
| `*-default` | ~1GB | Debian completo | Desarrollo local |
| `*-slim` | ~50-150MB | Debian slim | Producción (si no necesitas herramientas extra) |
| `*-alpine` | ~50MB | Alpine + musl | Producción con restricciones de tamaño |
| `*-distroless` | ~10-30MB | Distroless (solo runtime) | Máxima seguridad |

**Cuidado con Alpine**: usa musl libc en vez de glibc. Algunas bibliotecas C compiladas
para glibc (como las de Debian) no funcionan en Alpine sin recompilación.

### Elegir una imagen base

Regla general para elegir imagen base:

1. **Imagen oficial** del lenguaje/runtime si existe (`python:3.12`, `node:20`, `rust:1.75`)
2. **Variante `-slim`** si no necesitas herramientas extra (`python:3.12-slim`)
3. **Variante `-alpine`** si el tamaño es crítico (`python:3.12-alpine`) — pero probar
   compatibilidad de binarios C/C++
4. **Distro base** si necesitas control total (`debian:bookworm`, `almalinux:9`)
5. **`scratch`** para binarios estáticos (Go, Rust release) — imagen vacía, solo el binary

```dockerfile
# Ejemplo: binario Rust estático
FROM rust:1.75 AS build
WORKDIR /src
COPY . .
RUN cargo build --release --manifest-path Cargo.toml
# Imagen final: solo el binary estático
FROM scratch
COPY --from=build /src/target/release/myapp /myapp
ENTRYPOINT ["/myapp"]
```

## Otros registries públicos

### Quay.io (Red Hat)

Registry gratuito mantenido por Red Hat. Común en entornos Kubernetes/Openshift.

```bash
docker pull quay.io/prometheus/node-exporter:v1.7.0
```

### GitHub Container Registry (GHCR)

Registry de GitHub. Integrado con GitHub Actions.

```bash
# Login
echo $GITHUB_TOKEN | docker login ghcr.io -u USERNAME --password-stdin

# Pull/Push
docker pull ghcr.io/my-org/my-package:1.0
docker push ghcr.io/my-org/my-package:1.0
```

### Amazon ECR (Elastic Container Registry)

Registry privado de AWS. Cobra por almacenamiento y transferencia.

```bash
# Autenticarse con AWS
aws ecr get-login-password --region us-east-1 | \
  docker login --username AWS --password-stdin ACCOUNT.dkr.ecr.us-east-1.amazonaws.com

# Pull/Push
docker pull ACCOUNT.dkr.ecr.us-east-1.amazonaws.com/my-repo:1.0
```

### Google Artifact Registry (GAR)

Registry de Google Cloud. Integrado con Cloud Build y GKE.

```bash
# Autenticarse
gcloud auth configure-docker REGION-docker.pkg.dev

# Pull/Push
docker pull REGION-docker.pkg.dev/PROJECT/repo/image:1.0
```

## Multi-arch images

Muchas imágenes oficiales soportan múltiples arquitecturas:

```bash
# Ver las arquitecturas disponibles
docker buildx imagetools inspect nginx:latest
# Name: docker.io/library/nginx:latest
# Manifests:
#   arch: linux/amd64
#   arch: linux/arm/v6
#   arch: linux/arm/v7
#   arch: linux/arm64/v8
#   arch: linux/386
#   arch: linux/ppc64le
#   arch: linux/s390x

# Docker selecciona automáticamente según tu host
docker pull nginx:latest
# Docker transparently pulls the right image for your architecture
```

Si buildas en un host arm64 (Apple Silicon M-series) y empujas a un registry,
tu imagen será arm64. Un host amd64 descargará la versión amd64.

```bash
# Forzar una arquitectura específica
docker pull --platform linux/amd64 nginx:latest
```

## Práctica

### Ejercicio 1: Pull por tag y por digest

```bash
# Pull por tag
docker pull debian:bookworm

# Obtener el digest
docker images --digests debian
# Verás algo como: sha256:a1b2c3d4e5f6... cuando haya digest disponible

# Si el digest no aparece (algunas imágenes no lo reportan así)
docker inspect debian:bookworm --format '{{.RepoDigests}}'

# Pull por digest (usar el digest real obtenido arriba)
docker pull debian@sha256:a1b2c3d4e5f6...

# Ambos referencian la misma imagen localmente
docker images | grep debian
```

### Ejercicio 2: Inspeccionar tags y platforms disponibles

```bash
# Ver los tags de una imagen con el manifest
docker manifest inspect --verbose nginx:latest | head -50

# Ver la información de plataformas
docker manifest inspect nginx:latest | python3 -c "
import json, sys
data = json.load(sys.stdin)
if 'manifests' in data:
    for m in data['manifests']:
        p = m.get('platform', {})
        print(f\"{p.get('architecture')}/{p.get('os')} - digest: {m.get('digest', 'N/A')[:20]}...\")
else:
    # Imagen de una sola arquitectura
    print(f\"OS: {data.get('os')}, Architecture: {data.get('architecture')}\")
    print(f\"Layers: {len(data.get('rootfs', {}).get('diff_ids', []))}\")
"

# Ver qué arquitectura tiene tu Docker
docker version --format '{{.Server.Arch}}'
```

### Ejercicio 3: Verificar rate limits

```bash
# Consultar el rate limit restante (anónimo)
TOKEN=$(curl -s "https://auth.docker.io/token?service=registry.docker.io&scope=repository:library/nginx:pull" | python3 -c "import json,sys; print(json.load(sys.stdin)['token'])")

curl -s -D - -H "Authorization: Bearer $TOKEN" \
  "https://registry-1.docker.io/v2/library/nginx/manifests/latest" \
  -o /dev/null 2>&1 | grep -i ratelimit
# RateLimit-Limit: 100;w=21600
# RateLimit-Remaining: 99;w=21600
# RateLimit-Reset: 1234567890 (Unix timestamp)

# Si estás autenticado, el límite es diferente
docker login -u tu_usuario  # Luego repetir el curl
```

### Ejercicio 4: La trampa de latest

```bash
# Observar que "latest" es solo un tag
docker pull debian:latest
docker pull debian:bookworm

# Ver que pueden apuntar a la misma imagen (mismo IMAGE ID)
docker images | grep debian
# debian   latest     a6d8...  117MB   # Mismo ID que bookworm
# debian   bookworm   a6d8...  117MB   # Si la versión coincide

# ¿Cuándo divergen? Cuando sale una nueva Debian stable
# docker pull debian:latest # Obtiene la nueva versión
# docker pull debian:bookworm # Se mantiene en bookworm
```

### Ejercicio 5: Push a Docker Hub (necesitas cuenta)

```bash
# Login
docker login

# Crear imagen de prueba
docker run -d --name test-container debian:bookworm sleep 10
docker commit test-container tu_usuario/test-image:1.0
docker rm test-container

# Push
docker push tu_usuario/test-image:1.0

# Ver en https://hub.docker.com/r/tu_usuario/test-image
# Limpiar
docker rmi tu_usuario/test-image:1.0
```

### Ejercicio 6: Comparar variantes de tamaño

```bash
# Pull de múltiples variantes de python
docker pull python:3.12
docker pull python:3.12-slim
docker pull python:3.12-alpine

# Comparar tamaños
docker images | grep "^python.*3.12"
# python  3.12         a1b2...  1.05GB   # Default (Debian completo)
# python  3.12-slim    c3d4...  145MB    # Debian slim
# python  3.12-alpine  e5f6...  52MB     # Alpine

# Limpiar
docker rmi python:3.12 python:3.12-slim python:3.12-alpine
```

### Ejercicio 7: Usar un registry privado (GHCR ejemplo)

```bash
# Configurar GHCR (requiere GitHub token con permisos packages:write)
export GITHUB_TOKEN=ghp_xxxx

# Login
echo $GITHUB_TOKEN | docker login ghcr.io -u USERNAME --password-stdin

# Tag y push de una imagen existente
docker tag debian:bookworm ghcr.io/USERNAME/test:1.0
docker push ghcr.io/USERNAME/test:1.0

# Ver en https://github.com/USERNAME?tab=packages
# Limpiar
docker rmi ghcr.io/USERNAME/test:1.0
```
