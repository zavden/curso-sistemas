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

### Comparación directa

| Aspecto | Docker | Podman |
|---|---|---|
| Daemon | `dockerd` corriendo como root | **Sin daemon** |
| Modelo | Cliente-daemon | Fork-exec |
| Contenedores huérfanos si... | El daemon se cae | No aplica (no hay daemon) |
| Superficie de ataque | Alta (daemon root) | Menor |
| CLI como root | Necesario para daemon | No necesario |
| Rootless | Posible pero no default | **Default** |

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

### Requisitos para rootless

```bash
# Verificar que tu usuario tiene rango de subuids asignado
grep $USER /etc/subuid
# debe tener al menos 65536 UIDs

# La forma más fiable de verificar: simplemente comprobar que funciona
podman info | grep rootless
# rootless: true
```

> **Nota**: En Fedora/RHEL, user namespaces están habilitados por defecto.
> En Debian/Ubuntu antiguos (<22.04), puede ser necesario verificar
> `/proc/sys/kernel/unprivileged_userns_clone` (debe ser `1`). Este archivo
> no existe en Fedora — si no existe, los user namespaces están habilitados.

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

### Estructura del storage rootless

```
~/.local/share/containers/
└── storage/
    ├── overlay/              # Capas de imágenes (overlay driver)
    ├── overlay-images/       # Metadata de imágenes
    ├── overlay-layers/       # Metadata de capas
    └── overlay-containers/   # Metadata de contenedores

~/.config/containers/
├── registries.conf           # Configuración de registries (usuario)
├── storage.conf              # Configuración de storage (usuario)
└── containers.conf           # Configuración general (usuario)
```

> **Nota**: Si el sistema usa `vfs` en lugar de `overlay` como storage driver,
> los directorios se llaman `vfs/`, `vfs-images/`, etc.

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

### Docker Socket y Podman

Para usar Podman con herramientas diseñadas para Docker (como `docker-compose`
o herramientas que conectan al socket Docker):

```bash
# Opción 1: instalar el paquete de compatibilidad
sudo dnf install podman-docker    # Crea alias docker → podman

# Opción 2: activar el socket API compatible con Docker
systemctl --user enable --now podman.socket
# Esto crea un socket en $XDG_RUNTIME_DIR/podman/podman.sock
# que entiende el protocolo de la API de Docker
```

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

### Configuración de registries (formato v2)

```toml
# /etc/containers/registries.conf (sistema)
# o ~/.config/containers/registries.conf (usuario)

# Registries para búsqueda sin cualificar
unqualified-search-registries = ["docker.io", "quay.io"]

# Registry inseguro (sin TLS)
[[registry]]
location = "my-registry.local:5000"
insecure = true

# Bloquear un registry
[[registry]]
location = "untrusted-registry.local"
blocked = true
```

> **Nota**: El formato antiguo con `[registries.search]`, `[registries.insecure]`
> y `[registries.block]` está **deprecado**. Usar el formato v2 con `[[registry]]`.

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

### macOS / Windows

Podman funciona nativamente en Linux. Para macOS/Windows, se usa **Podman
Machine** (una VM Linux ligera):

```bash
# Instalar con brew (macOS) o winget (Windows)
# Luego inicializar la VM
podman machine init
podman machine start

# A partir de aquí, los comandos podman funcionan normalmente
podman run --rm alpine echo "Hello"
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

## Conceptos clave de Podman

### conmon

**conmon** (Container Monitor) es un proceso ligero que:
- Monitoriza el contenedor durante su vida
- Recolecta exit codes cuando termina
- Gestiona los logs del contenedor
- Se comunica con Podman CLI vía pipes

### OCI runtime

Podman no crea contenedores directamente. Usa un runtime OCI compatible:

| Runtime | Notas |
|---|---|
| `crun` | Default en Fedora/RHEL, escrito en C, rápido y optimizado para rootless |
| `runc` | Reference implementation, escrito en Go |
| `youki` | Escrito en Rust, proyecto de la comunidad containers |

```bash
# Ver qué runtime usa Podman
podman info | grep ociRuntime -A2
# ociRuntime:
#   name: crun
#   path: /usr/bin/crun
```

### Buildah

Buildah es la herramienta que Podman usa internamente para construir imágenes.
Construye imágenes OCI-compliant sin daemon:

```bash
# Podman build usa Buildah internamente
podman build -t myimage:latest .

# Buildah también puede usarse directamente
buildah bud -t myimage:latest .
```

---

## Archivos del lab

```
labs/
└── Dockerfile.compat  ← Dockerfile para probar compatibilidad OCI
```

---

## Ejercicios

### Ejercicio 1 — Primer contenedor rootless

Verifica que Podman funciona sin root ni daemon.

```bash
# Verificar modo rootless
podman info | grep rootless
```

**Predicción**: ¿Necesitas `sudo` para ejecutar un contenedor con Podman?

```bash
podman run --rm docker.io/library/alpine echo "Hello from rootless Podman"
```

<details><summary>Respuesta</summary>

No. Podman ejecuta directamente el runtime OCI (crun/runc) sin daemon ni
privilegios de root. El resultado: `Hello from rootless Podman`.

</details>

```bash
# Ver dónde almacena las imágenes
podman info | grep graphRoot
```

### Ejercicio 2 — Comparar Docker info vs Podman info

```bash
echo "=== Docker ==="
docker info 2>/dev/null | grep -E '(Root Dir|Storage|Server Version|rootless)' || echo "Docker no disponible"

echo ""
echo "=== Podman ==="
podman info | grep -E '(graphRoot|graphDriverName|rootless|version)'
```

**Predicción**: ¿Qué storage driver usa Docker? ¿Y Podman?

<details><summary>Respuesta</summary>

Docker usa `overlay2`. Podman usa `overlay`. Ambos son el mismo driver del
kernel (OverlayFS), pero Docker le pone el nombre `overlay2` (versión
mejorada que soportó desde Docker 17.06).

</details>

### Ejercicio 3 — Imágenes Docker funcionan en Podman

```bash
# Pull explícito desde Docker Hub
podman pull docker.io/library/nginx:latest

# Ejecutar
podman run -d --name test_nginx -p 8080:80 docker.io/library/nginx
curl -s http://localhost:8080 | head -3
```

**Predicción**: ¿El Dockerfile de nginx de Docker Hub funciona sin cambios
en Podman?

<details><summary>Respuesta</summary>

Sí. Las imágenes OCI son idénticas entre Docker y Podman. El formato es
estándar — lo que Docker Hub sirve, Podman lo consume sin modificaciones.

</details>

```bash
# Limpiar
podman rm -f test_nginx
podman rmi docker.io/library/nginx:latest
```

### Ejercicio 4 — Root vs Rootless: dónde se almacenan los datos

```bash
echo "=== Rootless ==="
podman info | grep graphRoot

echo ""
echo "=== Rootful ==="
sudo podman info 2>/dev/null | grep graphRoot || echo "Rootful no disponible"
```

**Predicción**: Si descargas `alpine` con `podman pull`, ¿`sudo podman images`
la verá?

```bash
podman pull docker.io/library/alpine:latest
podman images | grep alpine

sudo podman images 2>/dev/null | grep alpine
```

<details><summary>Respuesta</summary>

No. El storage rootless (`~/.local/share/containers/storage`) es completamente
independiente del rootful (`/var/lib/containers/storage`). Son dos mundos
separados: imágenes, contenedores y volúmenes no se comparten entre ellos.

</details>

### Ejercicio 5 — Ver el mapeo de namespaces

Demuestra que dentro del contenedor eres root, pero en el host eres tu usuario.

```bash
# Crear un contenedor que duerma
podman run -d --name sleepy docker.io/library/alpine sleep 3600

# Dentro del contenedor: eres root
podman exec sleepy id
```

**Predicción**: ¿El proceso `sleep 3600` corre como root en el host?

```bash
# En el host: el proceso pertenece a tu usuario
podman top sleepy user,pid,args
```

<details><summary>Respuesta</summary>

No. `podman top` muestra que el proceso corre como tu usuario (UID mapeado),
no como root. Dentro del contenedor parece root (UID 0), pero es un mapeo
vía user namespaces. Si un atacante escapa del contenedor, obtiene los
permisos de tu usuario, no de root.

</details>

```bash
podman rm -f sleepy
```

### Ejercicio 6 — Sin daemon: conmon como monitor

Demuestra que no hay daemon central y que cada contenedor tiene su propio
monitor.

```bash
# Verificar que no hay proceso podman corriendo permanentemente
ps aux | grep "[p]odman" | grep -v "podman run\|podman exec"
```

**Predicción**: ¿Qué proceso gestiona el contenedor cuando Podman CLI no
está corriendo?

```bash
podman run -d --name lab-conmon docker.io/library/alpine sleep 300

# Ver el proceso conmon asociado
ps aux | grep "[c]onmon"

podman rm -f lab-conmon
```

<details><summary>Respuesta</summary>

**conmon** (container monitor). Es un proceso ligero que monitoriza cada
contenedor individualmente. Podman CLI es efímero — se ejecuta, hace su
trabajo, y termina. Los contenedores siguen corriendo gestionados por conmon.

Si Docker's daemon se cae, todos los contenedores quedan huérfanos. Con
Podman, no hay punto único de fallo.

</details>

### Ejercicio 7 — Compatibilidad OCI: migrar imagen entre herramientas

Demuestra que las imágenes son intercambiables entre Docker y Podman.

```bash
cd labs/

# Construir con Podman
podman build -t lab-compat -f Dockerfile.compat .
podman run --rm lab-compat
```

**Predicción**: Si exportas la imagen de Podman con `podman save` y la cargas
en Docker con `docker load`, ¿funcionará?

```bash
# Exportar desde Podman
podman save lab-compat -o /tmp/lab-compat.tar

# Cargar en Docker
docker load -i /tmp/lab-compat.tar 2>/dev/null
docker run --rm lab-compat 2>/dev/null || echo "Docker no disponible"
```

<details><summary>Respuesta</summary>

Sí. El formato OCI es estándar. `podman save` y `docker load` (y viceversa)
son intercambiables. La imagen produce el mismo resultado en ambas
herramientas.

</details>

```bash
# Limpiar
podman rmi lab-compat 2>/dev/null
docker rmi lab-compat 2>/dev/null
rm -f /tmp/lab-compat.tar
```

### Ejercicio 8 — Registries: pull sin cualificar

Explora cómo Podman maneja los registries múltiples.

```bash
# Ver registries configurados
podman info | grep -A5 "registries"
```

**Predicción**: ¿Qué pasa si haces `podman pull nginx` sin especificar
`docker.io/library/`?

<details><summary>Respuesta</summary>

Podman muestra un menú interactivo preguntando de qué registry descargar
(`docker.io`, `quay.io`, `registry.fedoraproject.org`, etc.). Docker asume
`docker.io/library/` automáticamente. Para evitar la ambigüedad en scripts,
siempre usa el nombre completo: `docker.io/library/nginx:latest`.

</details>

```bash
# Pull explícito (recomendado en scripts)
podman pull docker.io/library/nginx:latest

podman images | grep nginx

podman rmi docker.io/library/nginx:latest
```

---

## Resumen visual

```
┌─────────────────────────────────────────────────────────────────┐
│                    DOCKER vs PODMAN                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  DOCKER:                     PODMAN:                            │
│  ┌──────────┐              ┌──────────┐                        │
│  │ docker   │               │ podman   │                        │
│  │ CLI      │               │ CLI      │                        │
│  └────┬─────┘               └────┬─────┘                        │
│       │                          │                              │
│       ▼                     fork/exec                           │
│  ┌──────────────┐                │                              │
│  │   dockerd    │                │                              │
│  │   (root)     │                │                              │
│  └──────┬───────┘                ▼                              │
│         │                  ┌──────────┐                         │
│     containerd             │  conmon   │                        │
│         │                  │  (por     │                        │
│         ▼                  │  contened)│                        │
│       runc                 └────┬─────┘                         │
│         │                       │                               │
│         ▼                       ▼                               │
│    ┌──────────┐            ┌──────────┐                        │
│    │contenedor│            │contenedor│                        │
│    └──────────┘            └──────────┘                        │
│                                                                 │
│    Daemon root              Sin daemon                          │
│    Rootful default          Rootless default                    │
│    Huérfanos si             Contenedores siguen                 │
│    daemon cae               si CLI se cierra                    │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```
