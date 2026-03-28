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

### El problema del socket

El socket `/var/run/docker.sock` permite controlar Docker. Quien tiene acceso
al socket puede:

1. Crear/eliminar contenedores e imágenes
2. Montar filesystems del host
3. Obtener información sensible del sistema

El socket es accesible si tienes permisos de root, perteneces al grupo `docker`,
o lo montas como bind mount dentro de un contenedor.

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

### Cómo funciona internamente

Docker rootless usa varias técnicas para operar sin root:

1. **User namespaces**: los UIDs dentro del contenedor se mapean a UIDs sin
   privilegios en el host (subordinate UIDs en `/etc/subuid`)
2. **slirp4netns o pasta**: networking en userspace sin necesidad de root
3. **fuse-overlayfs**: overlay filesystem sin privilegios (si el kernel no soporta
   overlay2 nativo en user namespaces)
4. **newuidmap/newgidmap**: binarios setuid que configuran el mapeo de UIDs

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

### Arquitectura de Podman

Podman no tiene daemon. Cada contenedor tiene su propio proceso `conmon`
(Container Monitor) que lo supervisa:

```bash
# Ver los procesos de un contenedor Podman
podman run -d --name demo alpine sleep 300

# Ver conmon
ps aux | grep conmon
# user  12345  /usr/bin/conmon --cid demo ...

# El contenedor mismo
ps aux | grep sleep
# user  12346  sleep 300
```

Esto significa:
- No hay un punto único de fallo (daemon)
- No hay socket permanente al que atacar
- Si podman muere, los contenedores siguen corriendo
- Para que contenedores sobrevivan el cierre de sesión, se necesita
  `loginctl enable-linger $USER`

### User namespaces en Podman

Podman usa **user namespaces** de forma transparente. UID 0 dentro del contenedor
se mapea a tu UID en el host a través de `/etc/subuid`:

```bash
# Tu usuario tiene un rango de UIDs subordinados
cat /etc/subuid
# user:100000:65536
# → user puede mapear UIDs 100000-165535

# Dentro del contenedor:
# UID 0 (root) → UID de tu usuario en el host
# UID 1-65536 → UIDs 100000-165535 del host

podman run --rm alpine cat /proc/1/uid_map
#          0       1000          1
#          1     100000      65536
```

Esto significa que incluso si un proceso escapa del contenedor como "root" (UID 0
del contenedor), en el host es tu usuario sin privilegios (UID 1000) o un UID
subordinado sin permisos especiales.

### Podman vs Docker CLI

```bash
# Comandos son casi idénticos
docker run --rm alpine echo hello
podman run --rm alpine echo hello

# Alias para mantener compatibilidad
alias docker=podman
```

## Comparación de los tres modos

| Característica | Docker rootful | Docker rootless | Podman rootless |
|---|---|---|---|
| Daemon | Sí (root) | Sí (user) | No |
| Escape = root en host | Sí | No | No |
| Configuración | Default | Requiere setup | Default |
| Puertos < 1024 | Sí | No (sin sysctl) | No (sin sysctl) |
| Rendimiento red | Nativo | Overhead | Overhead |
| Superficie de ataque | Alta | Baja | Mínima |
| RHCSA/RHEL estándar | No | No | Sí |
| Compatibilidad Docker | Máxima | Alta | Alta (compat docker) |

### Seguridad: flujo de un ataque

```
Docker rootful:
  Atacante → acceso al socket → daemon (root) → cualquier operación como root

Docker rootless:
  Atacante → acceso al socket → daemon (UID 1000) → operaciones limitadas al usuario

Podman:
  No hay socket de daemon permanente — cada contenedor es independiente
```

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
- Ciertas operaciones de filesystem que requieren privilegios reales

## El archivo /etc/subuid y /etc/subgid

Para que rootless funcione (Docker o Podman), necesitas subordinate UIDs:

```bash
# Ver los rangos de UID subordinados
cat /etc/subuid
# user:100000:65536

cat /etc/subgid
# user:100000:65536

# Significa:
# - Tu usuario (UID 1000) tiene derecho a usar
# - UIDs 100000-165535 (65536 UIDs) para user namespaces
```

Si estos archivos no existen o están vacíos, rootless no funcionará. Para
configurarlos:

```bash
sudo usermod --add-subuids 100000-165535 --add-subgids 100000-165535 $USER
```

---

## Práctica

### Ejercicio 1 — Verificar el modo de Docker

```bash
# Comprobar si Docker corre como rootful o rootless
docker info 2>/dev/null | grep -i rootless
# Si no aparece "rootless: true", es rootful

# Ver el directorio de datos del daemon
docker info | grep -i "docker root dir"
# /var/lib/docker → rootful
# /home/user/.local/share/docker → rootless

# Ver el propietario del socket
ls -la /var/run/docker.sock
# srw-rw---- root docker ... → rootful
```

**Predicción**: en nuestro laboratorio usamos Docker rootful. El socket pertenece
a `root:docker` y los datos están en `/var/lib/docker`.

### Ejercicio 2 — Comparar permisos de procesos

```bash
# Ejecutar un contenedor de larga duración
docker run -d --name perm_check alpine sleep 3600

# Ver el usuario del proceso en el HOST
ps aux | grep "sleep 3600" | grep -v grep
# En rootful: root   PID ... sleep 3600
# En rootless: user  PID ... sleep 3600

# Ver los UIDs dentro del contenedor
docker exec perm_check cat /proc/1/status | grep Uid
# Uid:     0   0   0   0  → root DENTRO del contenedor

# Ver el uid_map del contenedor
docker exec perm_check cat /proc/1/uid_map
# En rootful:  0          0 4294967295  → mapeo 1:1 (UID 0 real)
# En rootless: 0       1000          1  → UID 0 mapeado a UID 1000

docker rm -f perm_check
```

**Predicción**: en rootful, `ps aux` muestra `root` como propietario del proceso.
El `uid_map` muestra mapeo 1:1 (sin user namespaces). Root en el contenedor ES
root real en el host.

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

# Ver quién tiene acceso al socket
ls -la /var/run/docker.sock
# srw-rw---- 1 root docker ... → solo root y grupo docker
```

**Predicción**: el contenedor con acceso al socket puede listar y controlar
todos los contenedores del host. Acceso al socket = acceso root al host.

### Ejercicio 4 — Ver subordinate UIDs

```bash
# Ver si tienes subordinate UIDs configurados
cat /etc/subuid
cat /etc/subgid
# user:100000:65536  → tienes 65536 UIDs subordinados

# Si no tienes, configurar:
# sudo usermod --add-subuids 100000-165535 --add-subgids 100000-165535 $USER

# Estos UIDs son necesarios para que user namespaces funcionen
# (tanto en Docker rootless como en Podman)
```

**Predicción**: si usas Fedora, los subordinate UIDs probablemente ya estén
configurados (se crean automáticamente al crear el usuario).

### Ejercicio 5 — Comparar Docker vs Podman (si disponible)

```bash
# Verificar si Podman está instalado
which podman && echo "Podman instalado" || echo "Podman no instalado"

# Si está instalado:

# Docker: daemon corriendo permanentemente
ps aux | grep dockerd | grep -v grep
# root  PID ... /usr/bin/dockerd ...

# Podman: sin daemon
ps aux | grep podman | grep -v grep
# No hay proceso daemon de Podman

# Los comandos son casi idénticos
docker run --rm alpine echo "desde docker"
podman run --rm alpine echo "desde podman"

# Ver la diferencia en user namespaces
docker run --rm alpine cat /proc/1/uid_map
podman run --rm alpine cat /proc/1/uid_map
# Docker rootful: 0  0  4294967295  (sin mapeo)
# Podman:         0  1000  1        (mapeado a tu UID)
```

**Predicción**: Docker rootful tiene un daemon como root y mapeo 1:1 de UIDs.
Podman no tiene daemon y usa user namespaces por defecto.

### Ejercicio 6 — Limitaciones de rootless

```bash
# Este ejercicio solo aplica si tienes Docker rootless.
# En rootful, todo funciona sin restricciones.

# Intentar bindear puerto privilegiado
docker run --rm -d -p 80:80 nginx 2>&1 | head -1
# rootful: funciona
# rootless: Error: permission denied

# Ver si tu sistema permite puertos bajos sin privilegios
sysctl net.ipv4.ip_unprivileged_port_start
# Si es 1024, rootless no puede usar puertos <1024
# Si es 80, rootless puede usar puertos >=80

# Verificar qué modo usamos
docker info 2>/dev/null | grep -E "(rootless|Root Dir)"
```

**Predicción**: en nuestro laboratorio (rootful), el puerto 80 funciona
directamente. En rootless, fallaría sin configurar `sysctl`.
