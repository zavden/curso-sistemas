# T02 — Registros

## Qué es un registry

Un **registry** (registro) es un servidor HTTP que almacena y distribuye imágenes de
contenedores siguiendo la especificación OCI (Open Container Initiative). Es el
equivalente a un repositorio de paquetes (como apt o dnf) pero para imágenes OCI.

Cuando ejecutas `docker pull nginx`, Docker:

1. Resuelve el nombre: `nginx` → `docker.io/library/nginx:latest`
2. Se autentica con el registry (si es necesario)
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
| Autenticado (Pro/Team) | 5,000 pulls | 24 horas | Cuenta de usuario/org |

> **Nota**: los rate limits se cuentan por ventana deslizante, no por hora exacta.
> Además, pull de imágenes que ya tienes localmente no consume rate limit (Docker
> compara el digest local con el remoto antes de descargar).

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

Docker Hub ya no acepta contraseñas de cuenta para `docker login` en muchos contextos.
Se debe usar un **Personal Access Token**:

1. Ir a [Docker Hub Settings → Security → New Access Token](https://hub.docker.com/settings/security)
2. Crear token con nombre descriptivo (ej: "lab-fedora")
3. Seleccionar permisos: `Read, Write, Delete` para push/pull, o `Read Only` para solo pull
4. Usar el token como contraseña en `docker login`

```bash
# Login con token via stdin (más seguro que escribirlo interactivamente)
echo "$DOCKER_TOKEN" | docker login -u tu_usuario --password-stdin
```

### Dónde se almacenan las credenciales

`docker login` guarda las credenciales en `~/.docker/config.json`:

```bash
cat ~/.docker/config.json
# {
#   "auths": {
#     "https://index.docker.io/v1/": {
#       "auth": "dXN1YXJpbzp0b2tlbg=="   ← base64(usuario:token)
#     }
#   }
# }
```

> **Seguridad**: en muchos sistemas las credenciales se almacenan en texto plano
> (codificado en base64, no cifrado). En Linux con un credential helper como
> `docker-credential-pass` o `docker-credential-secretservice`, se almacenan en un
> keyring seguro en su lugar.

```bash
# Cerrar sesión (elimina las credenciales del archivo)
docker logout
# Removing login credentials for https://index.docker.io/v1/

# Cerrar sesión de un registry específico
docker logout ghcr.io
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
- Actualizaciones regulares de seguridad
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
Apr:  debian:12.1 → sha256:bbbb (Debian 12.1)
# Nunca cambia — tag congelado a una release puntual
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

# Hay que hacer pull explícitamente para actualizar
docker pull nginx:latest
```

> **Nota sobre `docker run` y auto-pull**: `docker run nginx` descarga la imagen
> automáticamente si no la tienes localmente, pero si ya la tienes, **usa la local
> sin verificar el registry**. Para forzar que `docker run` siempre verifique si hay
> una versión más nueva:
>
> ```bash
> docker run --pull always nginx:latest
> ```
>
> El flag `--pull always` es de `docker run`, no de `docker pull`.

**Regla profesional**: nunca usar `latest` en Dockerfiles de producción. Especificar
siempre un tag con versión (`nginx:1.25`, `debian:bookworm`, `python:3.12-slim`).

### Tags semánticos

Muchas imágenes usan versionado semántico con distintos niveles de estabilidad:

| Tag | Significado | Estabilidad |
|-----|-------------|-------------|
| `nginx:1.25.4` | Versión exacta (patch) | Muy estable — no cambia |
| `nginx:1.25` | Latest de la serie 1.25.x | Estable — solo patches |
| `nginx:1` | Latest de la serie 1.x | Puede cambiar (minor updates) |
| `nginx:mainline` | Latest mainline | Inestable — cambios frecuentes |

Para desarrollo local, tags de minor version (`1.25`) son convenientes. Para
producción o CI/CD, tags de patch version (`1.25.4`) o digests garantizan
reproducibilidad.

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

### Cómo verificar digests

```bash
# El digest se calcula sobre el manifest, no las capas
docker inspect debian:bookworm --format '{{index .RepoDigests 0}}'
# debian@sha256:a1b2c3d4e5f6...

# Comparar el digest local con el del registry
docker manifest inspect debian:bookworm | python3 -c "
import json, sys
data = json.load(sys.stdin)
# Para manifest lists, muestra el digest de cada plataforma
if 'manifests' in data:
    for m in data['manifests']:
        p = m.get('platform', {})
        print(f\"{p.get('os')}/{p.get('architecture')}: {m['digest'][:30]}...\")
else:
    print(f\"digest: {data.get('config', {}).get('digest', 'N/A')[:30]}...\")
"
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
# Las capas compartidas con imágenes existentes en el registry no se suben

# Ver la imagen en Docker Hub (si es pública)
# https://hub.docker.com/r/myuser/myapp
```

El push también es incremental: solo sube las capas que el registry no tiene.

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

Docker usa autenticación Bearer token sobre HTTP:

```
1. docker pull nginx
2. Registry responde: 401 Unauthorized + WWW-Authenticate header
   WWW-Authenticate: Bearer realm="https://auth.docker.io/token",
                     service="registry.docker.io",
                     scope="repository:library/nginx:pull"
3. Docker pide token al auth server
   GET https://auth.docker.io/token?service=...&scope=...
   (incluyendo credenciales si docker login se ejecutó)
4. Auth server responde con JWT token
5. Docker repite el request con Authorization: Bearer <token>
6. Registry sirve el manifest y las capas
```

Este flujo es transparente para el usuario. Solo es relevante si necesitas debuggear
problemas de autenticación o implementar un cliente de registry custom.

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
- Algunas son excelentes (organizaciones reconocidas como `elastic/`, `grafana/`), otras
  son abandonware

### Imágenes verificadas

Docker Hub tiene un programa de badges que distingue imágenes confiables:

| Badge | Significado |
|-------|-------------|
| **Official Image** (checkmark azul) | Mantenido por Docker + upstream, auditado |
| **Verified Publisher** (checkmark verde) | Publisher verificado por Docker |

### Variantes de imágenes oficiales

Muchas imágenes ofrecen variantes con distinto tamaño y contenido:

| Variante | Tamaño aprox. | Base | Cuándo usarla |
|----------|--------------|------|---------------|
| `*` (default) | ~1GB | Debian completo | Desarrollo local, cuando necesitas herramientas de debug |
| `*-slim` | ~50-150MB | Debian minimal | Producción general — buen balance tamaño/compatibilidad |
| `*-alpine` | ~30-50MB | Alpine + musl | Cuando el tamaño es crítico y no dependes de glibc |
| `*-bookworm`/`*-bullseye` | ~1GB | Debian específica | Cuando necesitas una versión concreta de Debian |

```bash
# Comparar tamaños reales
docker pull python:3.12
docker pull python:3.12-slim
docker pull python:3.12-alpine

docker images | grep python
# python  3.12         ...  1.05GB
# python  3.12-slim    ...  145MB
# python  3.12-alpine  ...  52MB
```

**Cuidado con Alpine**: usa musl libc en vez de glibc. Binarios compilados para glibc
(como los de Debian) no funcionan en Alpine sin recompilación:

```bash
# Verificar qué libc usa cada variante
docker run --rm python:3.12-slim ldd --version 2>&1 | head -1
# ldd (Debian GLIBC 2.36) 2.36

docker run --rm python:3.12-alpine ldd 2>&1 | head -1
# musl libc (x86_64)
```

Si tu app depende de paquetes con extensiones C (NumPy, Pillow, cryptography, etc.),
`-slim` es más seguro que `-alpine`. En Alpine estos paquetes necesitan compilarse
desde fuente, lo que aumenta el tiempo de build y puede fallar.

### Elegir una imagen base

Regla general para elegir imagen base:

1. **Imagen oficial** del lenguaje/runtime si existe (`python:3.12`, `node:20`, `rust:1.75`)
2. **Variante `-slim`** si no necesitas herramientas extra (`python:3.12-slim`)
3. **Variante `-alpine`** si el tamaño es crítico (`python:3.12-alpine`) — pero probar
   compatibilidad de binarios C/C++
4. **Distro base** si necesitas control total (`debian:bookworm`, `almalinux:9`)
5. **`scratch`** para binarios estáticos (Go, Rust release) — imagen vacía, solo el binario

```dockerfile
# Ejemplo: binario Rust estático sobre scratch
FROM rust:1.75 AS build
WORKDIR /src
COPY . .
RUN cargo build --release

FROM scratch
COPY --from=build /src/target/release/myapp /myapp
ENTRYPOINT ["/myapp"]
```

## Buscar imágenes desde CLI

`docker search` permite buscar imágenes en Docker Hub sin salir del terminal:

```bash
# Buscar imágenes de nginx
docker search nginx
# NAME                  DESCRIPTION                     STARS   OFFICIAL
# nginx                 Official build of Nginx         20000   [OK]
# bitnami/nginx         ...                             200
# ...

# Filtrar solo imágenes oficiales
docker search --filter is-official=true nginx

# Limitar resultados
docker search --limit 5 postgres

# Filtrar por estrellas mínimas
docker search --filter stars=100 redis
```

`docker search` solo busca en Docker Hub y no muestra tags. Para ver los tags
disponibles, hay que usar la web de Docker Hub o `docker buildx imagetools inspect`.

## Otros registries públicos

### Quay.io (Red Hat)

Registry gratuito mantenido por Red Hat. Común en entornos Kubernetes/OpenShift.

```bash
docker pull quay.io/prometheus/node-exporter:v1.7.0
```

### GitHub Container Registry (GHCR)

Registry de GitHub. Integrado con GitHub Actions. Imágenes públicas son gratuitas.

```bash
# Login con token de GitHub (necesita permiso packages:read)
echo "$GITHUB_TOKEN" | docker login ghcr.io -u USERNAME --password-stdin

# Pull/Push
docker pull ghcr.io/my-org/my-package:1.0
docker push ghcr.io/my-org/my-package:1.0
```

### Amazon ECR / Google Artifact Registry

Registries privados de AWS y GCP respectivamente. Cobran por almacenamiento y
transferencia. Se usan típicamente en entornos cloud donde la infraestructura ya
está en ese proveedor.

```bash
# ECR (AWS) — autenticación via AWS CLI
aws ecr get-login-password --region us-east-1 | \
  docker login --username AWS --password-stdin ACCOUNT.dkr.ecr.us-east-1.amazonaws.com

# GAR (GCP) — autenticación via gcloud
gcloud auth configure-docker REGION-docker.pkg.dev
```

## Multi-arch images

Muchas imágenes oficiales soportan múltiples arquitecturas bajo el mismo tag.
Esto se logra con **manifest lists** (también llamadas OCI index):

```bash
# Ver las arquitecturas disponibles
docker buildx imagetools inspect nginx:latest
# Name: docker.io/library/nginx:latest
# Manifests:
#   Platform: linux/amd64
#   Platform: linux/arm/v7
#   Platform: linux/arm64/v8
#   Platform: linux/386
#   ...

# Docker selecciona automáticamente según tu host
docker pull nginx:latest
# En un host amd64: descarga la variante amd64
# En un host arm64: descarga la variante arm64

# Forzar una arquitectura específica
docker pull --platform linux/amd64 nginx:latest
docker pull --platform linux/arm64 nginx:latest
```

## Práctica

### Ejercicio 1: Pull por tag y por digest

```bash
# Pull por tag
docker pull debian:bookworm

# Obtener el digest
docker inspect debian:bookworm --format '{{index .RepoDigests 0}}'
# debian@sha256:a1b2c3d4e5f6...

# Pull por digest (usar el digest real obtenido arriba)
docker pull debian@sha256:a1b2c3d4e5f6...

# Ambos referencian la misma imagen localmente
docker images | grep debian
```

### Ejercicio 2: Inspeccionar plataformas disponibles

```bash
# Ver plataformas de nginx
docker manifest inspect nginx:latest | python3 -c "
import json, sys
data = json.load(sys.stdin)
if 'manifests' in data:
    for m in data['manifests']:
        p = m.get('platform', {})
        print(f\"{p.get('architecture')}/{p.get('os')}\")
else:
    print('Single-platform image')
"

# Ver qué arquitectura tiene tu Docker
docker version --format '{{.Server.Arch}}'
```

### Ejercicio 3: Verificar rate limits

```bash
# Consultar el rate limit restante (anónimo)
TOKEN=$(curl -s "https://auth.docker.io/token?service=registry.docker.io&scope=repository:library/nginx:pull" \
  | python3 -c "import json,sys; print(json.load(sys.stdin)['token'])")

curl -s -D - -H "Authorization: Bearer $TOKEN" \
  "https://registry-1.docker.io/v2/library/nginx/manifests/latest" \
  -o /dev/null 2>&1 | grep -i ratelimit
# RateLimit-Limit: 100;w=21600
# RateLimit-Remaining: 99;w=21600
```

### Ejercicio 4: La trampa de latest

```bash
# Observar que "latest" es solo un tag
docker pull debian:latest
docker pull debian:bookworm

# Ver que pueden apuntar a la misma imagen (mismo IMAGE ID)
docker images | grep debian
# debian   latest     a6d8...  117MB
# debian   bookworm   a6d8...  117MB   ← mismo ID = misma imagen

# Si los IDs coinciden, son la misma imagen con dos tags diferentes
```

### Ejercicio 5: Comparar variantes de tamaño

```bash
# Pull de múltiples variantes de python
docker pull python:3.12
docker pull python:3.12-slim
docker pull python:3.12-alpine

# Comparar tamaños
docker images | grep "python.*3.12"
# python  3.12         ...  1.05GB
# python  3.12-slim    ...  145MB
# python  3.12-alpine  ...  52MB

# Ver qué herramientas tiene cada una
docker run --rm python:3.12 which gcc curl wget
# /usr/bin/gcc  /usr/bin/curl  /usr/bin/wget  ← todas presentes

docker run --rm python:3.12-slim which gcc curl wget
# (vacío o error — slim no incluye herramientas de desarrollo)

docker run --rm python:3.12-alpine which gcc curl wget
# (vacío — alpine tampoco las incluye)

# Verificar la diferencia de libc (la razón real por la que alpine es especial)
docker run --rm python:3.12-slim ldd --version 2>&1 | head -1
# ldd (Debian GLIBC ...) ...

docker run --rm python:3.12-alpine ldd 2>&1 | head -1
# musl libc ...

# Limpiar
docker rmi python:3.12 python:3.12-slim python:3.12-alpine
```

### Ejercicio 6: Push a Docker Hub (requiere cuenta)

```bash
# Login
docker login

# Crear una imagen de prueba
echo 'FROM debian:bookworm
RUN echo "hello from my image" > /greeting.txt
CMD ["cat", "/greeting.txt"]' | docker build -t tu_usuario/test-push:v1 -

# Push
docker push tu_usuario/test-push:v1

# Verificar en https://hub.docker.com/r/tu_usuario/test-push

# Limpiar
docker rmi tu_usuario/test-push:v1
```
