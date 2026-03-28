# T02 — Rootless containers

## El daemon como superficie de ataque

En una instalación estándar de Docker, el daemon (`dockerd`) corre como **root**.
Cualquier proceso que pueda comunicarse con el daemon (a través del socket
`/var/run/docker.sock`) puede instruirlo a realizar operaciones como root.

```
┌───────────────────────────────────────────────┐
│               Docker rootful (default)        │
│                                               │
│  docker CLI ─── socket ───▶ dockerd (root)    │
│                               │               │
│                               ▼               │
│                          contenedor           │
│                          (root o non-root)    │
│                                               │
│  Riesgo: si un atacante accede al socket      │
│  o escapa del contenedor → root en el host    │
└───────────────────────────────────────────────┘
```

Docker rootless elimina este riesgo ejecutando el daemon completo como un usuario
sin privilegios.

## Docker rootless

En modo rootless, **todo el stack de Docker** (daemon, contenedores, redes,
almacenamiento) corre bajo un usuario sin privilegios:

```
┌───────────────────────────────────────────────┐
│               Docker rootless                 │
│                                               │
│  docker CLI ─── socket ───▶ dockerd (UID 1000)│
│                               │               │
│                               ▼               │
│                          contenedor           │
│                          (user namespace)     │
│                                               │
│  Escape del contenedor → UID 1000             │
│  (sin privilegios en el host)                 │
└───────────────────────────────────────────────┘
```

### Instalación y uso

```bash
# Instalar docker rootless (Debian/Ubuntu)
dockerd-rootless-setuptool.sh install

# El daemon escucha en un socket del usuario
export DOCKER_HOST=unix://$XDG_RUNTIME_DIR/docker.sock

# Verificar
docker info | grep -i "root"
# rootless: true
# Security Options: rootless

docker info | grep -i "storage"
# Storage Driver: overlay2
# Docker Root Dir: /home/user/.local/share/docker
```

### Diferencias respecto a Docker rootful

| Aspecto | rootful (default) | rootless |
|---|---|---|
| Daemon corre como | root (UID 0) | Tu usuario (UID 1000) |
| Datos almacenados en | `/var/lib/docker` | `~/.local/share/docker` |
| Socket | `/var/run/docker.sock` | `$XDG_RUNTIME_DIR/docker.sock` |
| Puertos < 1024 | Sí | No (sin configuración extra) |
| Networking | Bridge nativo (veth + iptables) | slirp4netns o pasta |
| Rendimiento de red | Nativo | Overhead por userspace networking |
| Storage drivers | Todos | overlay2 (con fuse-overlayfs si no hay soporte nativo) |
| cgroup v2 | Completo | Requiere delegación |

### Limitaciones

**Puertos privilegiados**: por defecto, rootless no puede bindear puertos menores
a 1024 (como 80 o 443):

```bash
# Esto falla en rootless
docker run -d -p 80:80 nginx
# Error: failed to expose ports: bind: permission denied

# Solución 1: usar un puerto no privilegiado
docker run -d -p 8080:80 nginx

# Solución 2: configurar el kernel para permitirlo
sudo sysctl net.ipv4.ip_unprivileged_port_start=80
```

**Networking**: rootless usa `slirp4netns` (o `pasta` en versiones recientes) para
networking en userspace. Esto añade overhead pero no requiere privilegios:

```bash
# Ver el modo de networking
docker info | grep -i network
# Network: slirp4netns
```

**Rendimiento I/O**: con `fuse-overlayfs`, el rendimiento de escritura a disco puede
ser menor que con overlay2 nativo.

## Podman rootless

Podman fue diseñado desde el inicio para funcionar **sin daemon y sin root**. No
necesita un modo especial — rootless es su modo natural:

```bash
# Podman no necesita configuración especial para rootless
podman run --rm alpine id
# uid=0(root) gid=0(root)  — root DENTRO del contenedor

# Pero en el host, el proceso pertenece a tu usuario
ps aux | grep -i "conmon"
# user  12345  ... conmon --cid ...  — UID 1000, no root
```

La diferencia arquitectónica es fundamental:

```
Docker rootful:     CLI ──▶ daemon (root) ──▶ containerd ──▶ runc
Docker rootless:    CLI ──▶ daemon (user) ──▶ containerd ──▶ runc
Podman:             CLI ──▶ fork/exec ──▶ conmon ──▶ runc (sin daemon)
```

Podman no tiene daemon, así que no hay un proceso de larga ejecución con socket
al que un atacante pueda conectarse.

### User namespaces en Podman

Podman usa **user namespaces** de forma transparente. UID 0 dentro del contenedor
se mapea a tu UID en el host a través de `/etc/subuid`:

```bash
# Tu usuario tiene un rango de UIDs subordinados
cat /etc/subuid
# user:100000:65536
# → user puede mapear UIDs 100000-165535

# Dentro del contenedor, root (UID 0) se mapea a UID de tu usuario
# UID 1 → 100000, UID 2 → 100001, etc.

podman run --rm alpine cat /proc/1/uid_map
#          0       1000          1
#          1     100000      65536
```

Esto significa que incluso si un proceso escapa del contenedor como "root" (UID 0
del contenedor), en el host es tu usuario sin privilegios (UID 1000) o un UID
subordinado sin permisos especiales.

## Comparación de los tres modos

| Característica | Docker rootful | Docker rootless | Podman rootless |
|---|---|---|---|
| Daemon | Sí (root) | Sí (user) | No |
| Escape = root en host | Sí | No | No |
| Configuración | Default | Requiere setup | Default |
| Puertos < 1024 | Sí | No (sin sysctl) | No (sin sysctl) |
| Rendimiento red | Nativo | Overhead | Overhead |
| RHCSA/RHEL estándar | No | No | Sí |
| Compatibilidad | Máxima | Alta | Alta (compat docker) |

### ¿Cuál usar?

- **Desarrollo local**: Docker rootful es lo más común y compatible. Aceptable si
  confías en las imágenes que ejecutas.
- **Servidores de producción**: Docker rootless o Podman rootless para reducir la
  superficie de ataque.
- **RHEL / entorno empresarial**: Podman rootless es la opción estándar y soportada
  por Red Hat. En RHCSA, los ejercicios de contenedores usan Podman.
- **Este curso**: usamos Docker rootful para el laboratorio (máxima compatibilidad),
  y cubrimos Podman en el Capítulo 4 como alternativa.

## Rootless en la práctica: qué cambia

Lo que **funciona igual** en rootless:
- `docker run`, `docker build`, `docker compose`
- Named volumes, bind mounts (dentro del home del usuario)
- Redes bridge entre contenedores
- La mayoría de imágenes

Lo que **puede fallar** en rootless:
- Bind mounts fuera del home (`-v /opt/data:/data` — sin permisos)
- Puertos privilegiados (<1024)
- Operaciones que requieren cgroup completo (limitaciones de CPU/memoria)
- Imágenes que asumen acceso a dispositivos del host (`--device`)

---

## Ejercicios

### Ejercicio 1 — Verificar el modo de Docker

```bash
# Comprobar si Docker corre como rootful o rootless
docker info 2>/dev/null | grep -i rootless
# Si no aparece "rootless: true", es rootful

# Ver el usuario del daemon
docker info | grep -i "docker root dir"
# /var/lib/docker → rootful
# /home/user/.local/share/docker → rootless

# Ver el propietario del socket
ls -la /var/run/docker.sock
# srw-rw---- root docker ... → rootful
```

### Ejercicio 2 — Comparar permisos de procesos

```bash
# Ejecutar un contenedor de larga duración
docker run -d --name perm_check alpine sleep 3600

# Ver el usuario del proceso en el HOST
ps aux | grep "sleep 3600" | grep -v grep
# En rootful: root   PID ... sleep 3600
# En rootless: user  PID ... sleep 3600

docker rm -f perm_check
```

### Ejercicio 3 — Demostrar el riesgo del Docker socket

```bash
# Esto demuestra por qué montar el socket es peligroso
# El contenedor puede ver todos los contenedores del host
docker run --rm -v /var/run/docker.sock:/var/run/docker.sock \
  docker docker ps

# Podría crear un contenedor con acceso total al host:
# (NO ejecutar en producción, solo para entender el riesgo)
# docker run -v /var/run/docker.sock:/var/run/docker.sock \
#   docker docker run -v /:/hostfs alpine cat /hostfs/etc/shadow
```
