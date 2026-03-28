# T02 — Rootless containers

## El daemon como superficie de ataque

En una instalación estándar de Docker, el daemon (`dockerd`) corre como **root**.
Cualquier proceso que pueda comunicarse con el daemon (a través del socket
`/var/run/docker.sock`) puede instruirlo a realizar operaciones como root.

```
┌───────────────────────────────────────────────┐
│               Docker rootful (default)         │
│                                                 │
│  docker CLI ─── socket ───▶ dockerd (root)    │
│                                │               │
│                                ▼               │
│                           contenedor           │
│                           (root o non-root)    │
│                                                 │
│  Riesgo: si un atacante accede al socket      │
│  o escapa del contenedor → root en el host    │
└───────────────────────────────────────────────┘
```

### El problema del socket

El socket `/var/run/docker.sock` permite controlar Docker:

```bash
# Quien tiene acceso al socket puede:
# 1. Crear/eliminar contenedores
# 2. Crear/eliminar imágenes
# 3. Montar archivosystems
# 4. Obtener información sensible

# El socket es accesible si:
# - Tienes permisos de root
# - Estás en el grupo "docker"
# - Tienes acceso al socket via bind mount
```

Docker rootless elimina este riesgo ejecutando el daemon completo como un usuario
sin privilegios.

## Docker rootless

En modo rootless, **todo el stack de Docker** (daemon, contenedores, redes,
almacenamiento) corre bajo un usuario sin privilegios:

```
┌─────────────────────────────────────────────────┐
│               Docker rootless                     │
│                                                   │
│  docker CLI ─── socket ───▶ dockerd (UID 1000) │
│                                │                 │
│                                ▼                 │
│                           contenedor             │
│                           (user namespace)       │
│                                                   │
│  Escape del contenedor → UID 1000 (sin root)    │
│  No hay forma de ser root en el host            │
└─────────────────────────────────────────────────┘
```

### Cómo funciona internamente

Docker rootless usa varias técnicas:

1. **User namespaces**: los UIDs dentro del contenedor se mapean a UIDs sin
   privilegios en el host (subordinate UIDs en `/etc/subuid`)
2. **SLIRP4netns o pasta**: networking en userspace sin necesidad de root
3. **fuse-overlayfs**: filesystem sin privilegios de root
4. **newuid/newgid mappings**: el kernel traduce los UIDs

### Instalación y uso

```bash
# Instalar docker rootless (Debian/Ubuntu)
dockerd-rootless-setuptool.sh install

# O instalar el paquete manualmente
# sudo apt install docker-ce-rootless-extras

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
|---------|------------------|---------|
| Daemon corre como | root (UID 0) | Tu usuario (UID 1000) |
| Datos almacenados en | `/var/lib/docker` | `~/.local/share/docker` |
| Socket | `/var/run/docker.sock` | `$XDG_RUNTIME_DIR/docker.sock` |
| Puertos < 1024 | Sí | No (sin configuración extra) |
| Networking | Bridge nativo (veth + iptables) | slirp4netns o pasta |
| Rendimiento de red | Nativo | Overhead por userspace networking |
| Storage drivers | Todos | overlay2 (con fuse-overlayfs si no hay soporte nativo) |
| cgroup v2 | Completo | Requiere delegación |
| CPU/memory limits | Completo | Limitado |

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

# slirp4netns: stack de red en userspace
# pasta: más nuevo, usa el namespace de red del host directamente
```

**Rendimiento I/O**: con `fuse-overlayfs`, el rendimiento de escritura a disco puede
ser menor que con overlay2 nativo.

**Delegación de cgroups**: para límites de CPU/memoria completos:

```bash
# En el host, delegar cgroups v2
sudo systemctl set-property docker MemoryLimit=2G
# O para el usuario específico
systemctl --user set-property docker MemoryLimit=2G
```

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
                                          │
Docker rootless:    CLI ──▶ daemon (user) ──▶ containerd ──▶ runc
                                          │                    │
Podman:             CLI ──▶ fork/exec ──▶ conmon ───────────▶ runc
                                          (sin daemon)
```

### Arquitectura de Podman

Podman no tiene daemon. Cada contenedor tiene su propio proceso `conmon`
(Container Monitor) que lo supervisa:

```bash
# Ver los procesos de un contenedor
podman run -d --name demo alpine sleep 300

# Ver conmon
ps aux | grep conmon
# user  12345  /usr/bin/conmon --cid demo --socket /run/user/1000...

# El contenedor mismo
ps aux | grep sleep
# user  12346  sleep 300
```

Esto significa:
- No hay un punto único de fallo (daemon)
- No hay socket permanente al que atacar
- Si podman muere, los contenedores siguen corriendo
- Los contenedores pueden outlive la sesión del usuario que los creó

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
#           0       1000          1
#           1     100000      65536
```

Esto significa que incluso si un proceso escapa del contenedor como "root" (UID 0
del contenedor), en el host es tu usuario sin privilegios (UID 1000) o un UID
subordinado sin permisos especiales.

### Podman vs Docker CLI

```bash
# Comandos son casi idénticos
docker run --rm alpine echo hello
podman run --rm alpine echo hello

# Alias para no cambiar habits
alias docker=podman
```

## Comparación de los tres modos

| Característica | Docker rootful | Docker rootless | Podman rootless |
|----------------|---------------|-----------------|-----------------|
| Daemon | Sí (root) | Sí (user) | No |
| Escape = root en host | Sí | No | No |
| Configuración | Default | Requiere setup | Default |
| Puertos < 1024 | Sí | No (sin sysctl) | No (sin sysctl) |
| Rendimiento red | Nativo | Overhead | Overhead |
| Superficie de ataque | Alta | Baja | Mínima |
| RHCSA/RHEL estándar | No | No | Sí |
| Compatibilidad con Docker | Máxima | Alta | Alta (compat docker) |
| Contenedores outlive CLI | Sí (daemon los mantiene) | Sí | Sí |
| requiere systemd | No | Sí | No |

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
-某些 operaciones de sistema de archivos

## El archivo /etc/subuid y /etc/subgid

Para que rootless funcione, necesitas subordinate UIDs:

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

Si estos archivos no existen o están vacíos, rootless no funcionará.

## Seguridad de rootless

### Reducción de superficie de ataque

Con Docker rootful:
```
Atacante → acceso al socket → daemon (root) → cualquier operación
```

Con rootless:
```
Atacante → acceso al socket → daemon (UID 1000) → operaciones limitadas
```

Con Podman:
```
No hay socket de daemon accesible
Cada contenedor tiene su propio supervisor (conmon)
```

### Comparación de riesgo

| Escenario | Docker rootful | Docker rootless | Podman |
|----------|--------------|-----------------|--------|
| Acceso al socket | root completo | UID limitado | N/A |
| Escape de contenedor | root en host | UID sin privilegios | UID sin privilegios |
| Vulnerabilidad en daemon | Compromete root | Compromete usuario | No aplica |

## Práctica

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

# Ver los UIDs dentro del contenedor
docker exec perm_check cat /proc/1/status | grep Uid
# Uid:     0   0   0   0  → root DENTRO

# Pero en el host:
ps aux | grep sleep
# Muestra el UID real (0 en rootful, 1000 en rootless)

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

### Ejercicio 4 — Ver subordinate UIDs

```bash
# Ver si tienes subordinate UIDs configurados
cat /etc/subuid
cat /etc/subgid

# Si no tienes, necesitas configurar
# sudo usermod --add-subuids 100000-165535 --add-subgids 100000-165535 $USER

# Ver cómo rootless docker usa estos UIDs
dockerd-rootless-setuptool.sh check
```

### Ejercicio 5 — Comparar Docker vs Podman

```bash
# Si tienes podman instalado
which podman && echo "Podman installed" || echo "Podman not installed"

# Ver la diferencia en arquitectura
# Docker: daemon corriendo
ps aux | grep dockerd

# Podman: sin daemon
ps aux | grep dockerd  # No hay daemon
ps aux | grep conmon   # Hay conmon por cada contenedor

# Los comandos son casi idénticos
docker run --rm alpine echo "docker"
podman run --rm alpine echo "podman"
```

### Ejercicio 6 — Limitaciones de rootless

```bash
# Intentar bindear puerto privilegiado
docker run -d -p 80:80 nginx 2>&1 | grep -i "permission denied" || echo "Funcionó (rootful)"

# Intentar bind mount fuera del home
# docker run -v /tmp:/data alpine ls /data
# En rootless esto puede fallar si /tmp no es accesible

# Ver qué limitaciones tienes
docker info 2>/dev/null | grep -i "security"
# Shows: Security Options: rootless (if rootless)
```

### Ejercicio 7 — User namespace mappings

```bash
# Ver los UID mappings de un contenedor
docker run -d --name uid-demo alpine sleep 300

# Dentro del contenedor
docker exec uid-demo cat /proc/1/uid_map
# En rootful: 0 0 0 (sin mapeo, UID 0 real)
# En rootless: 0 1000 1  1000 100000 65536 (mapeado a subordinate UIDs)

# Lo mismo con Podman
# podman run -d --name podman-uid alpine sleep 300
# podman exec podman-uid cat /proc/1/uid_map
# Siempre muestra mapeo en Podman

docker rm -f uid-demo
```

### Ejercicio 8 — Comparar rendimiento de red

```bash
# En rootful, el networking usa el stack nativo del kernel
# En rootless, usa slirp4netns que tiene overhead

# Puedes medir la diferencia
docker run --rm alpine sh -c 'time wget -q -O /dev/null http://example.com'

# En rootless vs rootful verás diferencias en latencia
```
