# Capítulo 4: Podman [A]

**Bloque**: B01 — Docker y Entorno de Laboratorio
**Tipo**: Aislado
**Objetivo**: Entender Podman como alternativa a Docker, sus diferencias arquitectónicas,
y cuándo usar cada uno — con énfasis en que RHCSA requiere Podman.

---

## Sección 1: Podman como Alternativa a Docker

### T01 — Qué es Podman

**Conceptos a cubrir**:
- Podman: herramienta de gestión de contenedores compatible con OCI (Open Container Initiative)
- Arquitectura daemonless:
  - Docker: cliente → daemon (dockerd) → containerd → runc → contenedor
  - Podman: CLI → fork/exec directo → conmon (monitor) → runc → contenedor
  - Sin daemon central = sin punto único de fallo, sin proceso root corriendo permanentemente
- Rootless by default:
  - Podman no necesita root para ejecutar contenedores
  - Usa user namespaces para mapear UIDs (UID 0 dentro del contenedor = UID sin privilegios
    en el host)
  - Docker puede ser rootless pero requiere configuración adicional
- Compatibilidad OCI:
  - Usa las mismas imágenes que Docker (OCI image format)
  - Puede hacer pull de Docker Hub, Quay.io, o cualquier registry OCI
  - Los Dockerfiles se usan sin cambios (Podman usa Buildah internamente para builds)
- Desarrollado por Red Hat, incluido en RHEL, Fedora, CentOS Stream

**Comportamientos importantes**:
- Podman almacena datos por usuario en `~/.local/share/containers/` (rootless)
  vs `/var/lib/containers/` (rootful)
- Las imágenes descargadas por un usuario no son visibles para otro (a diferencia
  de Docker con daemon compartido)
- No necesita un servicio/daemon iniciado — funciona inmediatamente
- `podman info` muestra la configuración completa (storage driver, registries, etc.)

**Práctica**:
- Instalar Podman (si no está instalado)
- Ejecutar `podman info` y comparar con `docker info`
- Ejecutar un contenedor básico con Podman y verificar que las imágenes Docker funcionan

### T02 — Equivalencia de comandos

**Conceptos a cubrir**:
- Podman implementa la misma CLI que Docker — drop-in replacement:
  | Docker | Podman |
  |--------|--------|
  | `docker run` | `podman run` |
  | `docker build` | `podman build` |
  | `docker images` | `podman images` |
  | `docker ps` | `podman ps` |
  | `docker pull` | `podman pull` |
  | `docker push` | `podman push` |
  | `docker volume` | `podman volume` |
  | `docker network` | `podman network` |
  | `docker exec` | `podman exec` |
  | `docker logs` | `podman logs` |
  | `docker inspect` | `podman inspect` |
- Alias para transición: `alias docker=podman` (funciona para la mayoría de comandos)
- `podman system migrate`: migrar configuraciones de Docker a Podman
- Registries: Podman por defecto consulta múltiples registries (configurados en
  `/etc/containers/registries.conf`):
  - docker.io, quay.io, registry.fedoraproject.org
  - Docker solo consulta docker.io por defecto
  - Al hacer `podman pull nginx`, Podman pregunta de qué registry si hay ambigüedad

**Comportamientos importantes**:
- Algunos flags menos comunes pueden diferir o no existir en Podman
- `podman build` usa Buildah internamente — algunas opciones avanzadas de BuildKit
  no están disponibles (ej: cache exports)
- Las imágenes de Podman rootless se almacenan en un directorio diferente que las
  de Podman rootful — `podman images` muestra diferentes listas según el usuario

**Práctica**:
- Ejecutar las mismas operaciones con Docker y con Podman, comparar salidas
- Configurar el alias `docker=podman` y verificar que los scripts funcionan
- Explorar `/etc/containers/registries.conf` y la resolución de nombres de imágenes

### T03 — Diferencias clave

**Conceptos a cubrir**:
- **No hay daemon**:
  - Docker: si dockerd se cae, todos los contenedores quedan huérfanos
  - Podman: cada contenedor es un proceso hijo independiente — no hay punto central de fallo
  - conmon (container monitor): proceso ligero que monitorea cada contenedor,
    recolecta exit codes, gestiona logs
- **Fork/exec model**: Podman forkea directamente el runtime OCI en vez de comunicarse
  con un daemon via socket — más simple, menos overhead
- **Integración con systemd**:
  - `podman generate systemd <container>`: genera un unit file de systemd para el contenedor
  - Permite gestionar contenedores como servicios del sistema: arranque automático,
    logs con journalctl, dependencias
  - Alternativa moderna: Quadlet (Podman 4.4+): archivos `.container` en
    `/etc/containers/systemd/` o `~/.config/containers/systemd/`
  - Docker usa `--restart` policies que dependen del daemon
- **Socket API**: Podman puede exponer un socket compatible con la API de Docker
  (`podman system service`) para herramientas que esperan el socket de Docker
- **SELinux**: Podman tiene mejor integración con SELinux — los volúmenes se etiquetan
  automáticamente con `:z` (shared) o `:Z` (private)

**Comportamientos importantes**:
- `podman run -v /host/path:/container/path:Z` — el `:Z` es necesario en sistemas
  con SELinux enforcing para que el contenedor pueda acceder al volumen
- Sin daemon, los contenedores de Podman rootless no sobreviven al logout del usuario
  a menos que se use `loginctl enable-linger <user>`
- `podman system service` permite usar docker-compose con Podman (via el socket)

**Práctica**:
- Generar un unit file systemd con `podman generate systemd`
- Comparar la gestión del ciclo de vida con systemd vs `podman start/stop`
- Demostrar que un contenedor Podman sobrevive al cierre de la terminal (con linger o systemd)

### T04 — Pods

**Conceptos a cubrir**:
- Pod: agrupación de uno o más contenedores que comparten:
  - Network namespace (misma IP, mismos puertos, se ven en localhost)
  - PID namespace (opcional)
  - Concepto tomado directamente de Kubernetes
- Cada pod tiene un contenedor "infra" (pause container) que mantiene los namespaces
  activos — se crea automáticamente
- Operaciones con pods:
  - `podman pod create --name mypod -p 8080:80`: crear un pod con port mapping
  - `podman run --pod mypod image`: añadir un contenedor al pod
  - `podman pod start/stop/rm mypod`: gestionar el pod completo
  - `podman pod ps`: listar pods
  - `podman pod inspect mypod`: detalles del pod
- Los puertos se mapean en el pod, no en los contenedores individuales
- `podman generate kube mypod`: genera un YAML de Kubernetes a partir del pod
- `podman play kube pod.yaml`: crea pods/contenedores a partir de un YAML de Kubernetes

**Comportamientos importantes**:
- Docker no tiene concepto de pods — es exclusivo de Podman
- Los contenedores dentro de un pod se comunican via localhost (sin red, sin DNS)
- Si un contenedor del pod falla, los demás siguen corriendo (a diferencia de
  Kubernetes que puede tener restart policies por pod)
- `podman generate kube` facilita la migración a Kubernetes — pero el YAML generado
  puede necesitar ajustes para producción

**Práctica**:
- Crear un pod con dos contenedores (web + app) que se comunican por localhost
- Comparar con la comunicación por nombre de servicio en Docker Compose
- Generar YAML de Kubernetes a partir de un pod y revisar el resultado

---

## Sección 2: Podman Compose y Migración

### T01 — podman-compose

**Conceptos a cubrir**:
- `podman-compose`: implementación third-party que lee `docker-compose.yml`
  y ejecuta los comandos equivalentes con Podman
- Instalación: `pip install podman-compose` o paquete de distribución
- Funcionalidad: soporta la mayoría de directivas de Compose (services, volumes,
  networks, build, depends_on, environment, ports)
- Alternativa: usar el socket API de Podman con Docker Compose v2 original:
  ```bash
  systemctl --user start podman.socket
  export DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock
  docker compose up  # usa Podman como backend
  ```
- Limitaciones de podman-compose:
  - No soporta todas las features avanzadas de Compose (ej: healthcheck conditions
    en depends_on no siempre funciona)
  - Desarrollo más lento que Docker Compose
  - No soporta Compose profiles, extensions, ni some advanced networking

**Comportamientos importantes**:
- podman-compose crea pods cuando múltiples servicios comparten red
- Los volúmenes se gestionan con `podman volume`, no con el daemon de Docker
- Archivos `docker-compose.yml` funcionan sin cambios en la mayoría de casos simples

**Práctica**:
- Tomar un docker-compose.yml existente y ejecutarlo con `podman-compose up`
- Comparar el resultado con `docker compose up`
- Probar la alternativa del socket API

### T02 — Migrar de Docker a Podman

**Conceptos a cubrir**:
- Dockerfiles: no requieren cambios — Podman usa Buildah que entiende la misma sintaxis
- Imágenes: compatibles al 100% (formato OCI)
- Comandos CLI: alias `docker=podman` para scripts existentes
- Compose files: funcionan con podman-compose o via socket API
- Diferencias que requieren ajustes:
  - SELinux: añadir `:z` o `:Z` a los bind mounts en sistemas RHEL/Fedora
  - Rootless: algunos Dockerfiles asumen root — pueden necesitar ajustes
  - Networking: la red bridge de Podman rootless usa slirp4netns o pasta (más lento)
  - Systemd: reemplazar `--restart` con systemd units para persistencia
  - Socket: herramientas que conectan al socket de Docker necesitan redirigirse

**Comportamientos importantes**:
- No se pueden migrar los volúmenes ni imágenes locales directamente de Docker a Podman
  (diferentes directorios y formatos de storage)
- `podman pull docker.io/library/nginx` funciona — es la misma imagen
- Algunos CI/CD pipelines asumen Docker — verificar compatibilidad

**Práctica**:
- Migrar el lab de Docker del capítulo anterior a Podman
- Identificar y resolver problemas de compatibilidad

### T03 — Cuándo usar Docker vs Podman

**Conceptos a cubrir**:
- **Usar Docker cuando**:
  - El equipo ya usa Docker y no hay razón para migrar
  - Se necesita Docker Swarm para orquestación
  - Se depende de features de BuildKit (cache mounts, secret mounts)
  - El CI/CD está configurado para Docker
  - Desarrollo en macOS/Windows (Docker Desktop tiene mejor soporte)
- **Usar Podman cuando**:
  - Se trabaja en entornos RHEL/Fedora (Podman viene preinstalado)
  - Seguridad es prioridad (rootless nativo, sin daemon root)
  - Se prepara para RHCSA (el examen usa Podman, no Docker)
  - Se necesita integración con systemd (servicios de contenedor)
  - Se planea migrar a Kubernetes (pods, kube generate/play)
- **En este curso**: usaremos Docker como herramienta principal para el lab,
  pero este capítulo garantiza que el estudiante puede trabajar con ambos

**Comportamientos importantes**:
- La industria está dividida: desarrollo usa Docker mayoritariamente,
  operaciones en RHEL usa Podman cada vez más
- Kubernetes no usa Docker ni Podman directamente en producción (usa containerd o CRI-O)
  — ambos son herramientas de desarrollo/administración

**Práctica**:
- Tabla comparativa: ejecutar las mismas operaciones del lab con Docker y Podman,
  documentar diferencias encontradas
