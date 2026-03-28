# T01 — Root en contenedor

## El problema: root por defecto

Por defecto, el proceso principal de un contenedor Docker corre como **root (UID 0)**.
Esto no es un accidente — muchas operaciones comunes (instalar paquetes, escribir
en directorios del sistema, bindear puertos privilegiados) requieren root.

```bash
# ¿Quién soy dentro de un contenedor?
docker run --rm alpine id
# uid=0(root) gid=0(root) groups=0(root)

docker run --rm debian:bookworm whoami
# root
```

El aislamiento de namespaces evita que ese root tenga acceso directo al host, pero
**no es una barrera absoluta**. Si un atacante explota una vulnerabilidad del kernel
o de Docker, ese root dentro del contenedor puede convertirse en root en el host.

### Por qué Docker necesita root

Docker requiere privilegios de root para crear contenedores porque internamente
necesita:

- Crear namespaces (user, pid, network, mount, etc.)
- Configurar cgroups
- Montar filesystems (overlay, procfs, sysfs, etc.)
- Configurar interfaces de red virtuales (veth pairs)
- Manipular iptables para port forwarding

Sin root, el daemon de Docker (que corre como root) hace estas operaciones en
nombre del contenedor.

## ¿Por qué es peligroso?

### Escenario 1: bind mount + root = acceso al host

Si montas directorios sensibles del host y el contenedor corre como root, el
contenedor puede leer y escribir libremente:

```bash
# El contenedor como root puede leer /etc/shadow del host
docker run --rm -v /etc:/host-etc:ro alpine cat /host-etc/shadow
# root:$6$hash...:19000:0:99999:7:::
# ¡Hashes de contraseñas del host expuestos!

# Peor aún: sin :ro, podría MODIFICAR archivos del host
docker run --rm -v /etc:/host-etc alpine sh -c \
  'echo "hacker:x:0:0::/root:/bin/bash" >> /host-etc/passwd'
# Se añadió un usuario root al host (si no usaste :ro)
```

### Escenario 2: Docker socket + root = control total del host

El socket de Docker (`/var/run/docker.sock`) permite controlar el daemon:

```bash
# Si montas el socket, el contenedor puede crear otros contenedores
docker run --rm -v /var/run/docker.sock:/var/run/docker.sock \
  docker docker ps
# Puede ver y controlar TODOS los contenedores del host

# Puede crear un contenedor con acceso total al filesystem del host
docker run --rm -v /var/run/docker.sock:/var/run/docker.sock \
  docker docker run -v /:/host alpine cat /host/etc/shadow
```

Montar el Docker socket es equivalente a dar **acceso root al host**. Algunas
herramientas lo requieren (Portainer, Traefik), pero debe hacerse con plena
conciencia del riesgo.

### Escenario 3: Escape de contenedor

Aunque es raro, ha habido vulnerabilidades que permiten escapar del contenedor:

- **Container breakout**: explotar vulnerabilidades del kernel para escapar del
  aislamiento de namespaces
- **Contenedores privilegiados**: `--privileged` otorga TODAS las capabilities
- **Namespaces compartidos**: `--pid=host` permite ver y acceder a procesos del host

```bash
# NUNCA hacer esto en producción
docker run --rm --privileged alpine sh
# Tiene TODAS las capabilities, puede acceder a TODO el host
```

## Capabilities: por qué root en contenedor no es root completo

El kernel Linux divide los privilegios de root en **capabilities** (~40 permisos
granulares). Docker elimina las más peligrosas por defecto, pero mantiene algunas:

```
Root en el host:       TODAS las capabilities (~40)
Root en contenedor:    Subconjunto reducido (14)
```

```bash
# Ver capabilities del proceso dentro del contenedor
docker run --rm alpine cat /proc/1/status | grep -i cap
# CapPrm: 00000000a80425fb
# CapEff: 00000000a80425fb

# Decodificar en el host (necesitas capsh, del paquete libcap)
capsh --decode=00000000a80425fb
# 0x00000000a80425fb=cap_chown,cap_dac_override,cap_fowner,cap_fsetid,
# cap_kill,cap_setgid,cap_setuid,cap_setpcap,cap_net_bind_service,
# cap_net_raw,cap_sys_chroot,cap_mknod,cap_audit_write,cap_setfcap
```

Capabilities que Docker **mantiene** por defecto (14):

| Capability | Qué permite |
|---|---|
| `CAP_CHOWN` | Cambiar propietario de archivos |
| `CAP_DAC_OVERRIDE` | Ignorar permisos de lectura/escritura (lo que vimos en T03 bind mounts) |
| `CAP_FOWNER` | Ignorar restricciones sobre propietario de archivos |
| `CAP_FSETID` | Establecer setuid/setgid en archivos |
| `CAP_KILL` | Enviar señales a otros procesos |
| `CAP_SETGID` | Cambiar GID del proceso |
| `CAP_SETUID` | Cambiar UID del proceso |
| `CAP_SETPCAP` | Modificar capabilities de otros procesos |
| `CAP_NET_BIND_SERVICE` | Bind a puertos privilegiados (<1024) |
| `CAP_NET_RAW` | Raw sockets (necesario para `ping`) |
| `CAP_SYS_CHROOT` | Usar `chroot` |
| `CAP_MKNOD` | Crear dispositivos especiales |
| `CAP_AUDIT_WRITE` | Escribir al audit log del kernel |
| `CAP_SETFCAP` | Establecer capabilities en archivos |

Capabilities que Docker **elimina** por defecto (las más relevantes):

| Capability | Qué permite | Por qué Docker la elimina |
|---|---|---|
| `CAP_SYS_ADMIN` | mount, pivot_root, BPF, sethostname | **La más peligrosa** — permite escape de contenedor |
| `CAP_NET_ADMIN` | Configurar interfaces, iptables, routing | Podría manipular la red del host |
| `CAP_SYS_PTRACE` | Trazar procesos con ptrace | Podría inspeccionar otros contenedores |
| `CAP_SYS_MODULE` | Cargar módulos del kernel | Acceso directo al kernel |
| `CAP_SYS_RAWIO` | Acceso directo a dispositivos I/O | Acceso al hardware |
| `CAP_SYS_TIME` | Modificar el reloj del sistema | Afecta a todo el host |

Aunque Docker limita las capabilities, sigue siendo una superficie de ataque.
Correr como non-root elimina el riesgo desde la raíz.

## Solución: correr como non-root

### En el Dockerfile con `USER`

```dockerfile
FROM debian:bookworm

# Instalar dependencias como root
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 python3-pip && \
    rm -rf /var/lib/apt/lists/*

# Crear un usuario sin privilegios
RUN useradd -r -s /bin/false -m appuser

# Copiar código con propiedad del usuario
COPY --chown=appuser:appuser app/ /app/

# Cambiar al usuario sin privilegios antes de CMD
USER appuser
WORKDIR /home/appuser

CMD ["python3", "/app/main.py"]
```

```bash
docker build -t myapp .
docker run --rm myapp id
# uid=999(appuser) gid=999(appuser) groups=999(appuser)
```

Después de `USER appuser`, todas las instrucciones (`RUN`, `CMD`, `ENTRYPOINT`)
se ejecutan como ese usuario. El `USER` no afecta a las instrucciones anteriores
(la instalación de paquetes se hizo como root).

### En `docker run` con `--user`

```bash
# Sobrescribir el usuario de la imagen
docker run --rm --user 1000:1000 alpine id
# uid=1000 gid=1000

# Funciona incluso si la imagen dice USER root
docker run --rm --user 1000:1000 nginx id
# uid=1000 gid=1000
```

### Limitaciones de non-root

Un proceso non-root no puede:

| Operación | Requiere |
|---|---|
| `apt-get install` | root o sudo |
| Escribir en `/etc`, `/usr`, `/var` | root o permisos explícitos |
| Bindear puertos < 1024 | `CAP_NET_BIND_SERVICE` (root o capability) |
| Cambiar propietario de archivos | `CAP_CHOWN` |

Esto no es un problema en la mayoría de aplicaciones — el código de la aplicación
normalmente solo necesita leer su propio código y escribir en directorios específicos.

## Opciones avanzadas de seguridad

### `--cap-drop` y `--cap-add`

Eliminar o añadir capabilities específicas:

```bash
# Drop todas y agregar solo las necesarias (principio de menor privilegio)
docker run --rm \
  --cap-drop=ALL \
  --cap-add=NET_BIND_SERVICE \
  nginx

# Eliminar una capability específica
docker run --rm --cap-drop=NET_RAW alpine ping -c 1 127.0.0.1
# ping: permission denied (operation not permitted)
# Sin CAP_NET_RAW, ping no funciona
```

### `--read-only`

Hacer el filesystem del contenedor de solo lectura (excluyendo volúmenes):

```bash
docker run --rm --read-only alpine touch /tmp/test
# Read-only file system

# Combinado con tmpfs para directorios temporales
docker run --rm \
  --read-only \
  --tmpfs /tmp \
  alpine sh -c 'echo "ok" > /tmp/test && cat /tmp/test'
# ok — tmpfs es writable aunque el rootfs sea read-only
```

### `--security-opt no-new-privileges`

Evita que un proceso gane más privilegios de los que tiene al iniciar (por ejemplo,
a través de binarios setuid):

```bash
docker run --rm --security-opt no-new-privileges:true alpine \
  cat /proc/1/status | grep NoNewPrivs
# NoNewPrivs: 1  — no puede escalar privilegios

docker run --rm alpine cat /proc/1/status | grep NoNewPrivs
# NoNewPrivs: 0  — por defecto, la escalación es posible
```

## Drop de privilegios en imágenes oficiales

Las imágenes oficiales de servicios implementan un patrón de dos fases:

1. **Arrancar como root**: para tareas de inicialización (crear directorios, ajustar
   permisos, bindear puertos privilegiados)
2. **Cambiar a non-root**: para ejecutar el servicio real

```bash
# Nginx arranca como root (para bindear puerto 80) y luego cambia a "nginx"
docker run -d --name web nginx
docker exec web ps aux
# PID 1: root — master process
# PID 2: nginx — worker process (no-root)

# PostgreSQL arranca como root y cambia a "postgres"
docker run -d --name db -e POSTGRES_PASSWORD=secret postgres:16
docker exec db ps aux
# PID 1: postgres (UID 999) — ya no es root
```

El entrypoint script de estas imágenes usa `gosu` o `su-exec` para cambiar de
usuario después de la inicialización. Es el patrón correcto para servicios que
necesitan inicialización como root.

## La instrucción `USER` en la cadena de Dockerfiles

`USER` afecta a todas las capas siguientes y a las imágenes que hereden con `FROM`:

```dockerfile
# Base: establece non-root
FROM debian:bookworm
RUN useradd -m appuser
USER appuser

# --- En otro Dockerfile ---
FROM mybase:latest
# Aquí el USER es appuser (heredado)
RUN whoami  # appuser

# Si necesitas root temporalmente:
USER root
RUN apt-get update && apt-get install -y curl
USER appuser
# Volver a non-root para CMD
```

## User namespace remapping (revisión)

Cuando usas user namespace remapping (`userns-remap` en daemon.json), el UID 0 del
contenedor se mapea a un UID sin privilegios del host (ej: 100000). Esto añade otra
capa de seguridad: incluso si el proceso corre como root dentro del contenedor, en
el host es un usuario sin privilegios.

Se configuró en detalle en S03 (permisos en bind mounts) y se profundizará en
T02 (rootless containers).

---

## Práctica

### Ejercicio 1 — Verificar root por defecto

```bash
# Verificar el usuario por defecto en varias imágenes
docker run --rm alpine id
docker run --rm debian:bookworm id
docker run --rm nginx id

# Ver el UID del proceso principal
docker run -d --name test_root alpine sleep 3600
docker exec test_root cat /proc/1/status | grep -E '^(Uid|Gid)'
# Uid: 0  0  0  0  — root

# Ver capabilities
docker exec test_root cat /proc/1/status | grep Cap
# CapPrm: 00000000a80425fb
# CapEff: 00000000a80425fb

docker rm -f test_root
```

**Predicción**: todas las imágenes corren como root (UID 0) por defecto.
Las capabilities muestran el subconjunto reducido (14 de ~40).

### Ejercicio 2 — Demostrar el riesgo de bind mount + root

```bash
# Crear un archivo de prueba propiedad de tu usuario
echo "mis datos" > /tmp/test_root_risk.txt
ls -la /tmp/test_root_risk.txt

# El contenedor como root puede sobrescribirlo
docker run --rm -v /tmp/test_root_risk.txt:/data/test.txt alpine \
  sh -c 'echo "sobrescrito por root" > /data/test.txt'

cat /tmp/test_root_risk.txt
# sobrescrito por root — tu archivo fue modificado por UID 0

rm /tmp/test_root_risk.txt
```

**Predicción**: root en el contenedor puede modificar cualquier archivo del host
al que tenga acceso vía bind mount, sin importar el propietario original.

### Ejercicio 3 — Dockerfile con USER non-root

```bash
cat > /tmp/Dockerfile.nonroot << 'EOF'
FROM alpine:latest
RUN adduser -D -s /bin/sh appuser
USER appuser
WORKDIR /home/appuser
CMD ["id"]
EOF

docker build -t nonroot-test -f /tmp/Dockerfile.nonroot /tmp

# Verificar que corre como non-root
docker run --rm nonroot-test
# uid=1000(appuser) gid=1000(appuser)

# Intentar operaciones privilegiadas
docker run --rm nonroot-test sh -c 'apk add curl 2>&1 || echo "Falla: no soy root"'
docker run --rm nonroot-test sh -c 'touch /etc/test 2>&1 || echo "Falla: sin permisos"'

# Sobrescribir con --user root (para debugging)
docker run --rm --user root nonroot-test id
# uid=0(root)

docker rmi nonroot-test
rm /tmp/Dockerfile.nonroot
```

**Predicción**: como `appuser`, no puede instalar paquetes ni escribir en `/etc`.
Con `--user root` se puede volver a root temporalmente.

### Ejercicio 4 — Cap-drop para limitar privilegios

```bash
# Root con TODAS las capabilities por defecto
docker run --rm alpine cat /proc/1/status | grep CapEff
# CapEff: 00000000a80425fb

# Root con TODAS las capabilities eliminadas
docker run --rm --cap-drop=ALL alpine cat /proc/1/status | grep CapEff
# CapEff: 0000000000000000  — sin capabilities

# Efecto práctico: sin CAP_NET_RAW, ping falla
docker run --rm --cap-drop=NET_RAW alpine ping -c 1 127.0.0.1 2>&1
# ping: permission denied (operation not permitted)

# Con CAP_NET_RAW (default), ping funciona
docker run --rm alpine ping -c 1 127.0.0.1
# PING 127.0.0.1: 56 data bytes — funciona
```

**Predicción**: `--cap-drop=ALL` reduce CapEff a 0. Sin `NET_RAW`, `ping` no
tiene permiso para crear raw sockets.

### Ejercicio 5 — Read-only filesystem

```bash
# Intentar escribir en filesystem readonly
docker run --rm --read-only alpine sh -c 'echo "test" > /tmp/test' 2>&1
# Read-only file system

# Con tmpfs para directorios que necesitan ser writables
docker run --rm --read-only --tmpfs /tmp alpine \
  sh -c 'echo "test" > /tmp/test && cat /tmp/test'
# test — tmpfs es writable

# Combinación segura: read-only + tmpfs + volumen para datos
docker run --rm \
  --read-only \
  --tmpfs /tmp \
  --tmpfs /run \
  -v /tmp/test_data:/data \
  alpine sh -c 'echo "data" > /data/file && echo "temp" > /tmp/file && echo "OK"'
# OK

rm -rf /tmp/test_data
```

**Predicción**: `--read-only` impide escribir en el rootfs del contenedor.
`--tmpfs` crea directorios writables en memoria. Volúmenes siguen siendo writables.

### Ejercicio 6 — Docker socket como riesgo

```bash
# SOLO para demostración — NUNCA hagas esto en producción

# El socket de Docker permite controlar el daemon
docker run --rm -v /var/run/docker.sock:/var/run/docker.sock \
  docker:cli docker ps
# Lista todos los contenedores del host

# Ver quién tiene acceso al socket en el host
ls -la /var/run/docker.sock
# srw-rw---- 1 root docker  0 ...  /var/run/docker.sock
# Solo root y el grupo docker pueden acceder
```

**Predicción**: con acceso al socket, el contenedor puede controlar el daemon
Docker del host — es equivalente a tener acceso root al host.

### Ejercicio 7 — Contenedor privilegiado vs normal

```bash
# Capabilities normales (subconjunto reducido)
docker run --rm alpine cat /proc/1/status | grep CapEff
# CapEff: 00000000a80425fb  — 14 capabilities

# --privileged otorga TODAS las capabilities
docker run --rm --privileged alpine cat /proc/1/status | grep CapEff
# CapEff: 000001ffffffffff  — TODAS las capabilities

# Un contenedor privilegiado puede hacer cosas que normalmente no puede:
docker run --rm --privileged alpine mount -t tmpfs none /mnt 2>&1
# Funciona — tiene CAP_SYS_ADMIN

docker run --rm alpine mount -t tmpfs none /mnt 2>&1
# Operation not permitted — no tiene CAP_SYS_ADMIN

# NUNCA usar --privileged en producción
```

**Predicción**: `--privileged` cambia CapEff de `a80425fb` (14 caps) a
`1ffffffffff` (todas). Esto permite operaciones como `mount` que normalmente
están bloqueadas.

### Ejercicio 8 — no-new-privileges

```bash
# Por defecto, un proceso puede escalar privilegios (via setuid, etc.)
docker run --rm alpine cat /proc/1/status | grep NoNewPrivs
# NoNewPrivs: 0

# Con no-new-privileges, la escalación está bloqueada
docker run --rm --security-opt no-new-privileges:true alpine \
  cat /proc/1/status | grep NoNewPrivs
# NoNewPrivs: 1

# Combinar con non-root para máxima seguridad
docker run --rm \
  --user 1000:1000 \
  --cap-drop=ALL \
  --read-only \
  --security-opt no-new-privileges:true \
  alpine id
# uid=1000 gid=1000 — mínimos privilegios posibles
```

**Predicción**: `NoNewPrivs: 1` impide que el proceso gane capabilities
adicionales durante su ejecución, incluso si encuentra un binario setuid.
