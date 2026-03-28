# T03 — Diferencias clave

## Sin daemon: implicaciones prácticas

La ausencia de daemon en Podman tiene consecuencias directas en cómo se gestionan
los contenedores.

### Docker: daemon como punto central

```bash
# Si dockerd se cae...
sudo systemctl stop docker

docker ps
# Cannot connect to the Docker daemon

# Los contenedores existentes quedan huérfanos:
# - Siguen corriendo (containerd los mantiene)
# - Pero no puedes gestionarlos hasta que dockerd vuelva
# - docker ps, docker exec, docker logs — todo falla
```

### Podman: sin punto único de fallo

```bash
# No hay daemon que pueda caerse
# Cada contenedor es gestionado por su propio conmon

podman run -d --name web docker.io/library/nginx
# conmon (PID 1234) monitorea el contenedor

# podman CLI puede cerrarse — el contenedor sigue
# No hay proceso central del que depender
```

### conmon: el monitor por contenedor

Cada contenedor tiene un proceso **conmon** (container monitor) asociado:

```bash
podman run -d --name test docker.io/library/alpine sleep 3600

# Ver los procesos
ps aux | grep conmon
# user  1234  conmon --cid abc123...

# conmon se encarga de:
# - Recolectar el exit code cuando el proceso termina
# - Gestionar los logs del contenedor (stdout/stderr)
# - Mantener el contenedor corriendo independientemente de podman CLI
```

## Integración con systemd

Una de las ventajas más significativas de Podman es la integración nativa con
**systemd**, el sistema de init de Linux.

### Docker: restart policies

Docker gestiona los reinicios con `--restart`:

```bash
docker run -d --restart unless-stopped --name web nginx
# Docker daemon se encarga de reiniciar si el contenedor se cae
# Depende de que dockerd esté corriendo
```

### Podman: systemd units

Podman puede generar **unit files de systemd** para gestionar contenedores como
servicios del sistema:

```bash
# Crear un contenedor
podman run -d --name web -p 8080:80 docker.io/library/nginx

# Generar un unit file de systemd
podman generate systemd --new --name web > ~/.config/systemd/user/container-web.service
```

El unit file generado:

```ini
# container-web.service
[Unit]
Description=Podman container-web.service
Wants=network-online.target
After=network-online.target

[Service]
Environment=PODMAN_SYSTEMD_UNIT=%n
Restart=on-failure
TimeoutStopSec=70
ExecStartPre=/bin/rm -f %t/%n.ctr-id
ExecStart=/usr/bin/podman run \
    --cidfile=%t/%n.ctr-id \
    --cgroups=no-conmon \
    --rm \
    --sdnotify=conmon \
    -d \
    --replace \
    --name web \
    -p 8080:80 \
    docker.io/library/nginx
ExecStop=/usr/bin/podman stop --ignore --cidfile=%t/%n.ctr-id
ExecStopPost=/usr/bin/podman rm -f --ignore --cidfile=%t/%n.ctr-id

[Install]
WantedBy=default.target
```

```bash
# Gestionar como servicio
systemctl --user daemon-reload
systemctl --user enable --now container-web.service

# Ver estado
systemctl --user status container-web.service

# Logs con journalctl
journalctl --user -u container-web.service -f

# Arranque automático con el sistema
loginctl enable-linger $(whoami)
```

### Quadlet: la forma moderna (Podman 4.4+)

Quadlet simplifica la integración con systemd usando archivos declarativos
`.container`:

```ini
# ~/.config/containers/systemd/web.container
[Container]
Image=docker.io/library/nginx
PublishPort=8080:80
Volume=web-data:/usr/share/nginx/html

[Service]
Restart=always

[Install]
WantedBy=default.target
```

```bash
# Recargar y arrancar
systemctl --user daemon-reload
systemctl --user start web.service

# Quadlet genera el unit file automáticamente
systemctl --user cat web.service
```

Quadlet es más legible y mantenible que los unit files generados por
`podman generate systemd`.

### Comparación: restart policy vs systemd

| Aspecto | Docker `--restart` | Podman systemd |
|---|---|---|
| Gestión de reinicios | Docker daemon | systemd |
| Control de dependencias | Limitado | Full (Wants, After, Before) |
| Logs | `docker logs` | `journalctl` |
| Arranque con el sistema | Depende de daemon | Directo con `WantedBy` |
| Supervisión de procesos | Basic | Advanced (cgroups, limits) |

## Contenedores rootless y persistencia

Sin daemon, los contenedores rootless de Podman **no sobreviven al logout del
usuario** por defecto:

```bash
# Sin linger: el contenedor muere cuando cierras sesión
podman run -d --name test docker.io/library/alpine sleep 3600
# Si haces logout, el contenedor se detiene

# Con linger: los contenedores persisten
loginctl enable-linger $(whoami)
# Ahora los contenedores rootless sobreviven al logout
```

`loginctl enable-linger` permite que los servicios del usuario (incluidos
contenedores con systemd units) sigan corriendo sin sesión activa.

### ¿Qué es linger?

```bash
# Ver si linger está habilitado para tu usuario
loginctl show-user $USER | grep Linger
# Linger=no (por defecto)

# Habilitar linger
loginctl enable-linger $USER

# Ahora los servicios de usuario persisten sin sesión activa
```

## SELinux

En distribuciones con **SELinux enforcing** (RHEL, Fedora, CentOS), Podman tiene
mejor integración que Docker:

### El problema sin labels SELinux

```bash
# En un sistema con SELinux enforcing:
podman run --rm -v /home/user/data:/data docker.io/library/alpine ls /data
# ls: /data: Permission denied  ← SELinux bloquea el acceso
```

SELinux asigna **etiquetas de contexto** a archivos y procesos. Un contenedor
tiene una etiqueta diferente a los archivos del host, y el acceso se deniega.

### Solución: `:z` y `:Z`

```bash
# :z — shared: reetiqueta el directorio para que CUALQUIER contenedor pueda acceder
podman run --rm -v /home/user/data:/data:z docker.io/library/alpine ls /data
# Funciona

# :Z — private: reetiqueta para que SOLO este contenedor pueda acceder
podman run --rm -v /home/user/data:/data:Z docker.io/library/alpine ls /data
# Funciona, pero otros contenedores no pueden acceder a /home/user/data
```

| Opción | SELinux label | Acceso |
|---|---|---|
| Sin `:z`/`:Z` | No modifica | SELinux puede bloquear |
| `:z` | `svirt_sandbox_file_t` (shared) | Cualquier contenedor |
| `:Z` | `svirt_sandbox_file_t` (private) | Solo este contenedor |

**Cuidado con `:Z`**: reetiqueta el directorio de forma que solo el contenedor
tiene acceso. Si lo usas en `/home` o `/etc`, puedes romper el acceso del host
a esos archivos.

Docker también soporta `:z` y `:Z`, pero no es tan crítico porque Docker suele
correr como root (con más permisos SELinux).

## Socket API compatible

Para herramientas que esperan el socket de Docker, Podman puede exponer una API
compatible:

```bash
# Activar el socket (rootless)
systemctl --user enable --now podman.socket

# Verificar
ls -la /run/user/$(id -u)/podman/podman.sock

# Usar con herramientas Docker
DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock docker ps
# Funciona: docker CLI habla con Podman via el socket
```

Esto permite usar **Docker Compose con Podman** como backend:

```bash
export DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock
docker compose up -d
# Docker Compose crea los contenedores usando Podman
```

### Rootful socket

```bash
# Socket para root (sistema)
sudo systemctl enable --now podman.socket
ls -la /var/run/podman/podman.sock

# rootful socket
sudo docker -H unix:///var/run/podman/podman.sock ps
```

## Comparación de storage y layers

### Docker storage

```bash
# Ver storage de Docker
docker info | grep -E '(Root Dir|Storage Driver)'
# Docker Root Dir: /var/lib/docker
# Storage Driver: overlay2
```

### Podman storage

```bash
# Rootless storage (usuario)
podman info | grep -E '(graphRoot|graphDriverName)'
# graphRoot: /home/user/.local/share/containers/storage
# graphDriverName: overlay

# Storage para root (sistema)
sudo podman info | grep graphRoot
# graphRoot: /var/lib/containers/storage
```

### Layers y drivers

| Aspecto | Docker | Podman |
|---|---|---|
| Driver por defecto | `overlay2` | `overlay` |
| Drivers disponibles | overlay2, fuse-overlayfs, vfs, btrfs | overlay, vfs, btrfs, zfs |
| Rootless driver | N/A (usa fuse-overlayfs) | `overlay` (vfs para simpler) |
| Directorio imágenes | `/var/lib/docker` | `~/.local/share/containers` |

## Contenedores vsPods

En Docker, cada contenedor es independiente. En Podman, los **Pods** agrupan
contenedores que comparten namespaces:

```bash
# Docker: contenedores independientes
docker run -d --name web nginx
docker run -d --name db postgres
# web y db no comparten nada

# Podman: pueden usar pods (ver T04)
podman pod create --name myapp
podman run -d --pod myapp --name web nginx
podman run -d --pod myapp --name db postgres
# web y db comparten red y PID namespace
```

## Tabla resumen de diferencias

| Aspecto | Docker | Podman |
|---|---|---|
| Daemon | Sí (dockerd, root) | No (fork/exec) |
| Rootless | Configuración extra | Default |
| systemd | `--restart` policies | Unit files nativos, Quadlet |
| SELinux | Soporte básico | Integración nativa (`:z`, `:Z`) |
| Pods | No | Sí (concepto Kubernetes) |
| Build engine | BuildKit | Buildah |
| Compose | Docker Compose nativo | podman-compose o vía socket |
| Persistencia al logout | Siempre (daemon) | Requiere linger o systemd |
| Socket API | Nativo | `podman system service` |
| RHCSA | No cubre | Requerido |
| Contenedores huérfanos | Si daemon cae | No aplica (sin daemon) |
| Gestor de volúmenes | dockerd | conmon |
| API REST |Sí (dockerd) |Sí (opcional) |

---

## Ejercicios

### Ejercicio 1 — Generar un systemd unit

```bash
# Crear un contenedor
podman run -d --name sys_test -p 8080:80 docker.io/library/nginx

# Generar unit file
mkdir -p ~/.config/systemd/user
podman generate systemd --new --name sys_test > ~/.config/systemd/user/container-sys_test.service

# Ver el contenido
cat ~/.config/systemd/user/container-sys_test.service

# Habilitar como servicio
systemctl --user daemon-reload
systemctl --user enable container-sys_test.service

# Detener el contenedor original y arrancar via systemd
podman stop sys_test && podman rm sys_test
systemctl --user start container-sys_test.service
systemctl --user status container-sys_test.service

curl -s http://localhost:8080 | head -3

# Limpiar
systemctl --user stop container-sys_test.service
systemctl --user disable container-sys_test.service
rm ~/.config/systemd/user/container-sys_test.service
systemctl --user daemon-reload
```

### Ejercicio 2 — SELinux y bind mounts

```bash
# Verificar si SELinux está activo
getenforce 2>/dev/null || echo "SELinux no disponible"

# Si SELinux está enforcing:
mkdir -p /tmp/selinux_test
echo "datos" > /tmp/selinux_test/test.txt

# Sin :z — puede fallar con SELinux enforcing
podman run --rm -v /tmp/selinux_test:/data docker.io/library/alpine cat /data/test.txt 2>&1

# Con :z — funciona
podman run --rm -v /tmp/selinux_test:/data:z docker.io/library/alpine cat /data/test.txt

rm -rf /tmp/selinux_test
```

### Ejercicio 3 — Socket API para Docker Compose

```bash
# Activar el socket de Podman
systemctl --user start podman.socket

# Verificar que existe
ls -la /run/user/$(id -u)/podman/podman.sock

# Usar docker CLI con el socket de Podman
DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock docker info 2>&1 | head -5

# Detener el socket cuando ya no se necesite
systemctl --user stop podman.socket
```

### Ejercicio 4 — Comparar storage rootless vs rootful

```bash
echo "=== Rootless ==="
podman info | grep -E '(graphRoot|graphDriverName|storage)'
ls -la ~/.local/share/containers/ 2>/dev/null | head -5

echo ""
echo "=== Rootful ==="
sudo podman info 2>/dev/null | grep -E '(graphRoot|graphDriverName|storage)' || echo "Rootful no disponible"

echo ""
echo "=== Docker ==="
docker info 2>/dev/null | grep -E '(Root Dir|Storage Driver)' || echo "Docker no disponible"
```

### Ejercicio 5 — Probar linger

```bash
# Ver estado de linger
loginctl show-user $USER 2>/dev/null | grep Linger || echo "No disponible"

# Ver procesos de usuario
ps aux | grep $USER | grep -v grep | head -5

# Habilitar linger si es necesario
# loginctl enable-linger $USER

# Los contenedores rootless ahora sobrevivirían al logout
```

### Ejercicio 6 — Conmon en acción

```bash
# Crear un contenedor
podman run -d --name conmon_test docker.io/library/alpine sleep 3600

# Ver procesos del contenedor
ps aux | grep -E '(conmon|sleep)' | grep -v grep

# Ver los contenedores y sus PIDs
podman ps
podman inspect conmon_test --format '{{.State.Pid}}'

# Los logs pasan por conmon
podman logs conmon_test

# Limpiar
podman rm -f conmon_test
```

---

## Resumen visual

```
┌─────────────────────────────────────────────────────────────────┐
│                 DOCKER vs PODMAN: DIFFERENCES                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  DOCKER                         PODMAN                           │
│  ┌──────────────┐              ┌──────────────┐                │
│  │   dockerd    │              │   (sin      │                │
│  │   (root)    │              │   daemon)   │                │
│  └──────┬───────┘              └──────┬───────┘                │
│         │                            fork/exec                  │
│         ▼                            ┌──────────────┐         │
│    containerd                        │ conmon (x    │         │
│         │                            │  contenedor) │         │
│         ▼                            └──────┬───────┘         │
│       runc                              runc                  │
│                                                                  │
│  restart policies              systemd units + Quadlet          │
│  BuildKit                     Buildah                          │
│  no pods                      pods (Kubernetes-style)          │
│  SELinux básico               SELinux nativo (:z, :Z)          │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```
