# T01 — Qué es Podman

## Podman en una línea

Podman es una herramienta de gestión de contenedores **compatible con OCI**,
desarrollada por Red Hat, que no requiere daemon ni privilegios de root.

## Arquitectura: Docker vs Podman

La diferencia fundamental entre Docker y Podman es arquitectónica. Docker tiene
un daemon central; Podman no.

### Docker: modelo cliente-daemon

```
docker run nginx
      │
      ▼
┌──────────┐     socket      ┌──────────────┐
│  docker  │ ──────────────▶ │   dockerd    │  ← daemon (root)
│  CLI     │                 │              │
└──────────┘                 │  containerd  │
                             │      │       │
                             │      ▼       │
                             │    runc      │  → crea el contenedor
                             │      │       │
                             │   contenedor │
                             └──────────────┘
```

- El daemon `dockerd` corre como **root** permanentemente
- Todas las operaciones pasan por el daemon via un socket Unix
- Si el daemon se cae, **todos los contenedores quedan huérfanos**
- El daemon es un punto único de fallo y una superficie de ataque

### Podman: modelo fork/exec

```
podman run nginx
      │
      ▼
┌──────────┐    fork/exec    ┌──────────┐
│  podman  │ ──────────────▶ │  conmon   │  ← monitor (por contenedor)
│  CLI     │                 │    │      │
└──────────┘                 │    ▼      │
                             │  runc     │  → crea el contenedor
                             │    │      │
                             │ contenedor│
                             └──────────┘
```

- **Sin daemon**: Podman ejecuta directamente el runtime OCI
- Cada contenedor tiene su propio **conmon** (container monitor): un proceso
  ligero que recolecta exit codes y gestiona logs
- Si Podman CLI se cierra, los contenedores **siguen corriendo** (gestionados
  por conmon)
- No hay proceso root corriendo permanentemente

## Rootless by default

Podman está diseñado para ejecutarse **sin privilegios de root**:

```bash
# Como usuario normal (sin sudo)
podman run --rm alpine echo "Hello from rootless Podman"
# Funciona directamente — sin daemon, sin root
```

### Cómo funciona rootless

Podman usa **user namespaces** del kernel para mapear UIDs:

```
Host:                          Contenedor:
  UID 1000 (tu usuario) ──────▶ UID 0 (root)
  UID 100000-165535     ──────▶ UID 1-65535
```

```bash
# Ver el mapeo de UIDs
cat /etc/subuid
# user:100000:65536

# Dentro del contenedor eres "root"
podman run --rm alpine id
# uid=0(root) gid=0(root)

# Pero en el host, el proceso pertenece a tu usuario
ps aux | grep "sleep" | grep -v grep
# user  12345 ... sleep 3600  ← UID 1000, no root
```

Si un atacante escapa del contenedor, tiene los permisos de **tu usuario**, no
de root. Con Docker rootful, un escape equivale a root en el host.

## Almacenamiento

Podman almacena imágenes y contenedores en directorios **por usuario**:

| Modo | Directorio | Quién accede |
|---|---|---|
| Rootless | `~/.local/share/containers/` | Solo tu usuario |
| Rootful (`sudo podman`) | `/var/lib/containers/` | Root |

```bash
# Rootless: tus imágenes
podman images
# Solo tus imágenes

# Rootful: imágenes del sistema
sudo podman images
# Diferentes imágenes — almacenamiento separado
```

Las imágenes descargadas por un usuario **no son visibles** para otro usuario ni
para `sudo podman`. Cada contexto tiene su propio almacenamiento.

## Compatibilidad OCI

Podman usa el mismo formato de imágenes que Docker (OCI image format). Las
imágenes son 100% intercambiables:

```bash
# Descargar una imagen de Docker Hub
podman pull docker.io/library/nginx:latest

# Descargar de otros registries
podman pull quay.io/prometheus/prometheus
podman pull registry.fedoraproject.org/fedora:latest

# Los Dockerfiles funcionan sin cambios
podman build -t myapp -f Dockerfile .
```

Internamente, Podman usa **Buildah** para construir imágenes. Buildah entiende
la sintaxis completa de Dockerfile.

## Registries

A diferencia de Docker (que solo consulta `docker.io`), Podman consulta
**múltiples registries** configurados en `/etc/containers/registries.conf`:

```bash
podman pull nginx
# ? Please select an image:
#   registry.fedoraproject.org/nginx
#   docker.io/library/nginx    ← elegir esta para imágenes Docker
#   quay.io/nginx
```

Para evitar la pregunta interactiva, especifica el registry completo:

```bash
# Explícito: sin ambigüedad
podman pull docker.io/library/nginx:latest
```

O configura Docker Hub como registry por defecto:

```bash
# ~/.config/containers/registries.conf
unqualified-search-registries = ["docker.io"]
```

## Instalación

### Fedora / RHEL / CentOS Stream / AlmaLinux / Rocky Linux

```bash
# Ya viene preinstalado en la mayoría de instalaciones
podman --version

# Si no está instalado:
sudo dnf install podman
```

### Debian / Ubuntu

```bash
sudo apt-get update
sudo apt-get install -y podman
```

### Verificar la instalación

```bash
podman info | head -20
# host:
#   arch: amd64
#   os: linux
#   rootless: true
#   ...

podman run --rm hello-world
```

## `podman info`

Muestra la configuración completa del entorno:

```bash
podman info
# host:
#   arch: amd64
#   os: linux
#   rootless: true           ← modo rootless
#   security:
#     rootless: true
#     selinuxEnabled: true
# store:
#   graphDriverName: overlay  ← storage driver
#   graphRoot: /home/user/.local/share/containers/storage
# registries:
#   search: [docker.io, quay.io]
```

Comparación con Docker:

```bash
docker info | grep -E '(Root Dir|Storage|Security|Rootless)'
# Docker Root Dir: /var/lib/docker
# Storage Driver: overlay2
# Security Options: seccomp, apparmor
```

---

## Ejercicios

### Ejercicio 1 — Primer contenedor con Podman

```bash
# Ejecutar un contenedor básico
podman run --rm docker.io/library/alpine echo "Hello from Podman"

# Verificar que es rootless
podman info | grep rootless
# rootless: true

# Ver dónde se almacenan las imágenes
podman info | grep graphRoot
# graphRoot: /home/user/.local/share/containers/storage
```

### Ejercicio 2 — Comparar docker info vs podman info

```bash
echo "=== Docker ==="
docker info 2>/dev/null | grep -E '(Root Dir|Storage|Server Version|rootless)' || echo "Docker no disponible"

echo ""
echo "=== Podman ==="
podman info | grep -E '(graphRoot|graphDriverName|rootless|version)'
```

### Ejercicio 3 — Imágenes Docker funcionan en Podman

```bash
# Pull explícito desde Docker Hub
podman pull docker.io/library/nginx:latest

# Ejecutar
podman run -d --name test_nginx -p 8080:80 docker.io/library/nginx
curl -s http://localhost:8080 | head -3

# Limpiar
podman rm -f test_nginx
podman rmi docker.io/library/nginx:latest
```
