# T02 — Registros

## ¿Qué es un registry?

Un **registry** (registro) es un servidor que almacena y distribuye imágenes de
contenedores. Es el equivalente a un repositorio de paquetes (como apt o dnf) pero
para imágenes OCI (Open Container Initiative).

Cuando ejecutas `docker pull nginx`, Docker se conecta a un registry, descarga las
capas de la imagen, y las almacena localmente.

## Docker Hub

[Docker Hub](https://hub.docker.com) es el registry público por defecto. Cuando no
especificas un registry en el nombre de la imagen, Docker asume Docker Hub.

### Rate limits

Docker Hub impone límites de descarga para evitar abuso:

| Tipo de usuario | Límite | Periodo |
|----------------|--------|---------|
| Anónimo | 100 pulls | 6 horas (por IP) |
| Autenticado (free) | 200 pulls | 6 horas (por cuenta) |
| Autenticado (Pro/Team) | 5000 pulls | 24 horas |

En entornos con IP compartida (oficinas, universidades, CI/CD), el límite anónimo
se agota rápidamente. Autenticarse duplica el límite y lo vincula a la cuenta en vez
de la IP.

```bash
# Autenticarse en Docker Hub
docker login
# Username: tu_usuario
# Password: tu_token (NO la contraseña — usar un Personal Access Token)

# Verificar la autenticación
docker info | grep Username
```

## Nomenclatura de imágenes

La referencia completa de una imagen sigue este formato:

```
[registry/][namespace/]name[:tag][@sha256:digest]
```

### Componentes

| Componente | Ejemplo | Valor por defecto |
|-----------|---------|-------------------|
| registry | `docker.io`, `quay.io`, `ghcr.io` | `docker.io` (Docker Hub) |
| namespace | `library`, `myuser`, `myorg` | `library` (imágenes oficiales) |
| name | `nginx`, `postgres`, `myapp` | — (obligatorio) |
| tag | `latest`, `bookworm`, `15-alpine` | `latest` |
| digest | `sha256:a1b2c3...` | — (opcional) |

### Ejemplos resueltos

```
nginx                       → docker.io/library/nginx:latest
nginx:1.25                  → docker.io/library/nginx:1.25
myuser/myapp:v2             → docker.io/myuser/myapp:v2
quay.io/prometheus/node-exporter:v1.7.0
ghcr.io/my-org/my-tool:main
```

### El caso especial de `library/`

Las imágenes oficiales de Docker Hub están bajo el namespace `library/`, pero este se
omite por convención:

```bash
# Estas dos líneas son equivalentes:
docker pull nginx
docker pull docker.io/library/nginx:latest
```

Las imágenes bajo `library/` son mantenidas por Docker Inc. junto con los equipos
upstream del proyecto (el equipo de Nginx mantiene `library/nginx`, el equipo de
Postgres mantiene `library/postgres`, etc.). Tienen:
- Escaneo de seguridad automático
- Documentación estandarizada
- Actualizaciones regulares de seguridad
- Revisión de Dockerfiles por Docker Inc.

Las imágenes de usuarios u organizaciones (`myuser/myapp`) no tienen estas garantías.

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
# nginx  latest  abc123  6 months ago

# Actualizar manualmente
docker pull nginx:latest
# Descarga la versión actual del registry
```

**Regla profesional**: nunca usar `latest` en Dockerfiles de producción. Especificar
siempre un tag con versión (`nginx:1.25`, `debian:bookworm`, `python:3.12-slim`).

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
| Uso recomendado | Desarrollo | Producción, CI/CD |

En producción y CI/CD, usar digests (o al menos tags de versión específica) garantiza
que siempre se despliega exactamente la misma imagen.

## Pull y Push

### `docker pull`

Descarga una imagen de un registry al almacenamiento local.

```bash
# Pull básico (desde Docker Hub)
docker pull nginx:1.25

# Pull desde otro registry
docker pull quay.io/prometheus/node-exporter:v1.7.0

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
# Tagear una imagen local con tu namespace
docker tag myapp:latest myuser/myapp:v1.0

# Subir al registry
docker push myuser/myapp:v1.0
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

## Imágenes oficiales vs community

### Imágenes oficiales (`library/`)

- Mantenidas por Docker Inc. + upstream maintainers
- Escaneo de seguridad automático y regular
- Documentación estándar en Docker Hub
- Dockerfiles públicos y auditables
- Variantes disponibles: `-slim` (menos paquetes), `-alpine` (basadas en Alpine Linux,
  muy pequeñas)
- Ejemplos: `nginx`, `postgres`, `python`, `node`, `debian`, `ubuntu`, `alpine`

### Imágenes community (`user/image`)

- Mantenidas por usuarios individuales u organizaciones
- Sin garantía de calidad, seguridad o actualizaciones
- Pueden contener software malicioso, dependencias desactualizadas, o configuraciones inseguras
- Algunas son excelentes (organizaciones reconocidas), otras son abandonware

### Imágenes verificadas

Docker Hub tiene un programa de "Docker Official Images" y "Verified Publisher" que
distingue imágenes confiables. En la interfaz web de Docker Hub, buscar el badge azul
de "Official Image" o "Verified Publisher".

### Elegir una imagen base

Regla general para elegir imagen base:

1. **Imagen oficial** del lenguaje/runtime si existe (`python:3.12`, `node:20`, `rust:1.75`)
2. **Variante `-slim`** si no necesitas herramientas extra (`python:3.12-slim`)
3. **Variante `-alpine`** si el tamaño es crítico (`python:3.12-alpine`) — pero cuidado:
   Alpine usa musl en vez de glibc, lo que puede causar incompatibilidades con algunas
   bibliotecas C
4. **Distro base** si necesitas control total (`debian:bookworm`, `almalinux:9`)
5. **`scratch`** para binarios estáticos (Go, Rust release)

## Práctica

### Ejercicio 1: Pull por tag y por digest

```bash
# Pull por tag
docker pull debian:bookworm

# Obtener el digest
docker images --digests debian
# Copiar el sha256:...

# Pull por digest (usar el digest real obtenido arriba)
docker pull debian@sha256:<digest_copiado>

# Ambos referencian la misma imagen
docker images | grep debian
```

### Ejercicio 2: Inspeccionar tags disponibles

```bash
# Ver los tags de una imagen con el manifest
docker manifest inspect --verbose nginx:latest | head -30

# Ver la plataforma (architecture)
docker manifest inspect nginx:latest | python3 -c "
import json, sys
data = json.load(sys.stdin)
if 'manifests' in data:
    for m in data['manifests']:
        p = m.get('platform', {})
        print(f\"{p.get('architecture')}/{p.get('os')}\")
"
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
```

### Ejercicio 4: La trampa de latest

```bash
# Observar que "latest" es solo un tag
docker pull debian:latest
docker pull debian:bookworm

# Ver que pueden apuntar a la misma imagen (mismo IMAGE ID)
docker images | grep debian

# Si los IDs coinciden, son literalmente la misma imagen con dos tags diferentes
```
