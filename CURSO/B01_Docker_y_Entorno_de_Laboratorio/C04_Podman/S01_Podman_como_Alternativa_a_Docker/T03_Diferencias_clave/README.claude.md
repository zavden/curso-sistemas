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

> **Nota**: conmon NO gestiona volúmenes ni redes. Solo gestiona el ciclo de
> vida del proceso, exit codes y logs. Los volúmenes y redes los gestiona el
> runtime OCI (crun/runc) y el backend de storage.

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

> **Nota**: `podman generate systemd` está **deprecado desde Podman 4.4**.
> Se mantiene por compatibilidad, pero la forma recomendada es Quadlet (ver
> siguiente sección).

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

# Arranque automático con el sistema (requiere linger)
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
`podman generate systemd`. No requiere crear contenedores primero — la
definición es declarativa.

### Otros tipos de archivos Quadlet

```ini
# ~/.config/containers/systemd/app-network.network
[Network]
Subnet=10.89.0.0/24
Gateway=10.89.0.1
```

```ini
# ~/.config/containers/systemd/app-data.volume
[Volume]
# Quadlet también puede gestionar volúmenes
```

### Comparación: restart policy vs systemd

| Aspecto | Docker `--restart` | Podman systemd |
|---|---|---|
| Gestión de reinicios | Docker daemon | systemd |
| Control de dependencias | Limitado | Full (Wants, After, Before) |
| Logs | `docker logs` | `journalctl` (integrado con sistema) |
| Arranque con el sistema | Depende de daemon | Directo con `WantedBy` |
| Supervisión de procesos | Básica | Avanzada (cgroups, limits, watchdog) |

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

### ¿Qué es linger?

`loginctl enable-linger` permite que los servicios del usuario (incluidos
contenedores con systemd units) sigan corriendo sin sesión activa:

```bash
# Ver si linger está habilitado para tu usuario
loginctl show-user $USER | grep Linger
# Linger=no (por defecto)

# Habilitar linger
loginctl enable-linger $USER

# Ahora los servicios de usuario persisten sin sesión activa
```

En Docker, los contenedores persisten siempre porque el daemon root los
mantiene. En Podman rootless, se necesita linger + systemd para lograr
lo mismo.

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
correr como root (con más permisos SELinux por defecto).

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

## Tabla resumen de diferencias

| Aspecto | Docker | Podman |
|---|---|---|
| Daemon | Sí (dockerd, root) | No (fork/exec) |
| Rootless | Soportado (config extra) | Default |
| systemd | `--restart` policies | Unit files, Quadlet |
| SELinux | Soporte `:z`/`:Z` | Integración nativa |
| Pods | No | Sí (concepto Kubernetes) |
| Build engine | BuildKit | Buildah |
| Compose | Docker Compose nativo | `podman compose` o vía socket |
| Persistencia al logout | Siempre (daemon root) | Requiere linger + systemd |
| Socket API | Nativo (dockerd) | Opcional (`podman.socket`) |
| RHCSA | No cubre | Requerido en el examen |
| Contenedores huérfanos | Si daemon cae | No aplica (sin daemon) |

---

## Ejercicios

### Ejercicio 1 — Sin daemon: conmon en acción

```bash
# Verificar que Docker tiene un daemon
ps aux | grep "[d]ockerd"
```

**Predicción**: ¿Existe un proceso `podman` permanente equivalente?

```bash
# Podman no tiene daemon
ps aux | grep "[p]odman" | grep -v "run\|exec"
```

<details><summary>Respuesta</summary>

No. No hay proceso `podman` permanente. Podman CLI es efímero: se ejecuta,
hace su trabajo, y termina. Los contenedores los mantiene `conmon`, un
proceso ligero por contenedor.

</details>

```bash
# Crear un contenedor y ver conmon
podman run -d --name lab-conmon docker.io/library/alpine sleep 300
ps aux | grep "[c]onmon"

podman rm -f lab-conmon
```

### Ejercicio 2 — Integración systemd (generate systemd)

Genera un unit file para gestionar un contenedor como servicio.

```bash
podman run -d --name lab-systemd -p 8888:80 docker.io/library/nginx

curl -s http://localhost:8888 | head -3

# Generar unit file
mkdir -p ~/.config/systemd/user
podman generate systemd --new --name lab-systemd > \
  ~/.config/systemd/user/container-lab-systemd.service
```

**Predicción**: Después de detener el contenedor original y arrancarlo vía
systemd, ¿seguirá sirviendo en el puerto 8888?

```bash
podman stop lab-systemd && podman rm lab-systemd
systemctl --user daemon-reload
systemctl --user start container-lab-systemd.service

systemctl --user status container-lab-systemd.service
curl -s http://localhost:8888 | head -3
```

<details><summary>Respuesta</summary>

Sí. El unit file contiene el comando `podman run` completo con `-p 8888:80`.
systemd crea un contenedor nuevo (por `--new`) cada vez que arranca el servicio.
Los logs están disponibles en `journalctl --user -u container-lab-systemd.service`.

</details>

```bash
# Limpiar
systemctl --user stop container-lab-systemd.service
systemctl --user disable container-lab-systemd.service 2>/dev/null
rm ~/.config/systemd/user/container-lab-systemd.service
systemctl --user daemon-reload
```

### Ejercicio 3 — Quadlet: la forma moderna

Crea un contenedor gestionado por systemd usando Quadlet (sin `generate systemd`).

```bash
mkdir -p ~/.config/containers/systemd

cat > ~/.config/containers/systemd/lab-quadlet.container << 'EOF'
[Container]
Image=docker.io/library/nginx
PublishPort=8889:80

[Service]
Restart=on-failure

[Install]
WantedBy=default.target
EOF

systemctl --user daemon-reload
```

**Predicción**: ¿Cómo se llamará el servicio systemd generado por Quadlet?

```bash
systemctl --user start lab-quadlet.service
systemctl --user status lab-quadlet.service
curl -s http://localhost:8889 | head -3
```

<details><summary>Respuesta</summary>

`lab-quadlet.service`. Quadlet genera automáticamente un unit file a partir
del archivo `.container`, usando el nombre del archivo como nombre del servicio.
Es mucho más simple que `podman generate systemd`: no necesitas crear el
contenedor primero.

</details>

```bash
# Ver el unit file generado por Quadlet
systemctl --user cat lab-quadlet.service

# Limpiar
systemctl --user stop lab-quadlet.service
rm ~/.config/containers/systemd/lab-quadlet.container
systemctl --user daemon-reload
```

### Ejercicio 4 — SELinux y bind mounts

```bash
getenforce 2>/dev/null || echo "SELinux no disponible"
```

**Predicción**: En un sistema con SELinux enforcing, ¿funcionará un bind mount
sin `:z`?

```bash
mkdir -p /tmp/lab-selinux
echo "test data" > /tmp/lab-selinux/test.txt

# Sin :z
podman run --rm -v /tmp/lab-selinux:/data docker.io/library/alpine cat /data/test.txt 2>&1

# Con :z
podman run --rm -v /tmp/lab-selinux:/data:z docker.io/library/alpine cat /data/test.txt
```

<details><summary>Respuesta</summary>

Con SELinux enforcing: sin `:z` falla con `Permission denied`. Con `:z` funciona.

Sin SELinux (o en modo permissive): ambos funcionan. Pero es importante
conocer `:z` y `:Z` para entornos RHEL donde SELinux enforcing es obligatorio.

</details>

```bash
# Ver el label SELinux del archivo
ls -lZ /tmp/lab-selinux/test.txt 2>/dev/null

rm -rf /tmp/lab-selinux
```

### Ejercicio 5 — Socket API para Docker Compose

Demuestra que Docker Compose puede usar Podman como backend.

```bash
systemctl --user start podman.socket

ls -la /run/user/$(id -u)/podman/podman.sock
```

**Predicción**: ¿`docker info` via el socket de Podman mostrará información
de Docker o de Podman?

```bash
DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock docker info 2>&1 | head -10
```

<details><summary>Respuesta</summary>

Mostrará información de **Podman**, no de Docker. El socket habla el
protocolo de la API de Docker, pero el backend es Podman. Docker CLI no
sabe que está hablando con Podman.

</details>

```bash
systemctl --user stop podman.socket
```

### Ejercicio 6 — Linger: persistencia al logout

```bash
loginctl show-user $USER 2>/dev/null | grep Linger
```

**Predicción**: Si `Linger=no` y cierras sesión SSH, ¿sobrevivirán los
contenedores rootless?

<details><summary>Respuesta</summary>

No. Sin linger, systemd mata todos los procesos del usuario al cerrar sesión,
incluyendo conmon y los contenedores. Con Docker no hay este problema porque
el daemon root mantiene los contenedores independientemente de las sesiones
de usuario.

Para habilitar linger: `loginctl enable-linger $USER`. Esto permite que
servicios del usuario (y contenedores gestionados por systemd) persistan
sin sesión activa.

</details>

### Ejercicio 7 — Comparar storage rootless vs rootful

```bash
echo "=== Rootless ==="
podman info | grep -E '(graphRoot|graphDriverName)'

echo ""
echo "=== Rootful ==="
sudo podman info 2>/dev/null | grep -E '(graphRoot|graphDriverName)' || echo "Rootful no disponible"

echo ""
echo "=== Docker ==="
docker info 2>/dev/null | grep -E '(Root Dir|Storage Driver)' || echo "Docker no disponible"
```

**Predicción**: ¿Cuántos storage backends separados existen en un sistema con
Docker + Podman rootless + Podman rootful?

<details><summary>Respuesta</summary>

Tres completamente independientes:
1. Docker: `/var/lib/docker` (overlay2)
2. Podman rootless: `~/.local/share/containers/storage` (overlay)
3. Podman rootful: `/var/lib/containers/storage` (overlay)

Las imágenes descargadas en uno no son visibles en los otros.

</details>

### Ejercicio 8 — restart policy de Podman

Podman también soporta `--restart` (sin systemd), pero funciona diferente:

```bash
podman run -d --restart on-failure --name lab-restart docker.io/library/alpine sh -c 'exit 1'
sleep 3
podman ps -a --filter name=lab-restart
```

**Predicción**: ¿Quién gestiona el restart en Podman si no hay daemon?

<details><summary>Respuesta</summary>

Podman usa `conmon` para detectar el exit code y reiniciar el contenedor.
Sin embargo, `--restart` en Podman rootless **solo funciona mientras haya
una sesión de usuario activa** (o linger habilitado). Por eso, la forma
recomendada es systemd/Quadlet en lugar de `--restart`.

</details>

```bash
podman rm -f lab-restart
```

---

## Resumen visual

```
┌─────────────────────────────────────────────────────────────────┐
│                 DOCKER vs PODMAN: DIFERENCIAS                  │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  DOCKER                         PODMAN                          │
│  ┌──────────────┐              ┌──────────────┐                │
│  │   dockerd    │              │  (sin daemon) │                │
│  │   (root)     │              └──────┬───────┘                │
│  └──────┬───────┘                fork/exec                     │
│         │                      ┌──────────────┐                │
│     containerd                 │ conmon (x    │                │
│         │                      │  contenedor) │                │
│         ▼                      └──────┬───────┘                │
│       runc                          runc                        │
│                                                                 │
│  --restart policies          systemd units + Quadlet            │
│  BuildKit                    Buildah                            │
│  No pods                     Pods (Kubernetes-style)            │
│  SELinux básico              SELinux nativo (:z, :Z)            │
│  Siempre persiste            Requiere linger + systemd          │
│                                                                 │
│  Quadlet (Podman 4.4+):                                        │
│  ┌──────────────────────────────────────────┐                  │
│  │ ~/.config/containers/systemd/web.container │                  │
│  │ [Container]                               │                  │
│  │ Image=nginx                               │                  │
│  │ PublishPort=8080:80                       │                  │
│  └──────────────────────────────────────────┘                  │
│  → systemctl --user start web.service                          │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```
