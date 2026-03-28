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

Docker requiere privileges de root para crear containers porque internamente necesita:

- Crear namespaces (user, pid, network, mount, etc.)
- Configurar cgroups
- Montar filesystems (overlay, procfs, sysfs, etc.)
- Configurar interfaces de red virtuales (veth pairs)
- Manipular iptables para port forwarding

Sin root, el daemon de Docker (que corre como root) hace estas operaciones por el contenedor.

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
herramientas lo requieren (Portainer, Traefik, CI/CD tools), pero debe hacerse
con plena conciencia del riesgo.

### Escenario 3: Escape de contenedor

Aunque es raro, ha habido vulnerabilidades que permiten escape del contenedor:

- **Container breakout**: exploiting kernel vulnerabilities to escape
- **Privileged containers**: containers run with `--privileged` have all capabilities
- **Shared namespaces**: `--pid=host` allows accessing host processes

```bash
# NUNCA hacer esto en producción
docker run --rm --privileged alpine sh
# Tiene TODAS las capabilities, puede acceder a TODO el host
```

## Capabilities: por qué root en contenedor no es root completo

El kernel Linux divide los privilegios de root en **capabilities** (~40 permisos
granulares). Docker elimina las más peligrosas por defecto, pero mantiene algunas:

```
Root en el host:       TODAS las capabilities
Root en contenedor:    Subconjunto reducido (~14)
```

```bash
# Ver capabilities del proceso dentro del contenedor
docker run --rm alpine cat /proc/1/status | grep -i cap
# CapPrm: 00000000a80425fb
# CapEff: 00000000a80425fb

# Las capabilities se representan como un bitmask hexadecimal
# 00000000a80425fb en binario indica qué capabilities están activas
```

Para decodificar el bitmask en el host:

```bash
# Instalar capsh (included in libcap-progs)
capsh --decode=00000000a80425fb

# Salida:
# 0x00000000a80425fb=cap_chown,cap_dac_override,cap_dac_read_search,
# cap_fowner,cap_fsetid,cap_kill,cap_setgid,cap_setuid,cap_setpcap,
# cap_linux_immutable,cap_net_bind_service,cap_net_broadcast,
# cap_net_admin,cap_net_raw,cap_ipc_lock,cap_ipc_owner,cap_sys_module,
# cap_sys_rawio,cap_sys_chroot,cap_sys_pacct,cap_sys_admin,cap_sys_boot,
# cap_sys_nice,cap_sys_resource,cap_sys_time,cap_sys_tty_config,
# cap_mknod,cap_lease,cap_audit_write,cap_audit_control,cap_setfcap
```

Capabilities que Docker **mantiene** por defecto:

| Capability | Qué permite | Razón para mantenerla |
|-----------|-----------|----------------------|
| `CAP_CHOWN` | Cambiar dueño de archivos | Necesario paraMuchos aplicaciones |
| `CAP_DAC_OVERRIDE` | Leer/escribir archivos ignorando permisos | Facilitates application operation |
| `CAP_FOWNER` |ignorar restrictions on file ownership | Común en aplicaciones |
| `CAP_FSETID` |setuid/setgid files without restrictions | Necesary for certain tools |
| `CAP_KILL` | Enviar signals a otros procesos | Necesary for process management |
| `CAP_SETGID` | Cambiar GID | Necessary for group operations |
| `CAP_SETUID` | Cambiar UID | Necessary for user switching |
| `CAP_SETPCAP` | Modificar capabilities de otros procesos | Requerido por el kernel |
| `CAP_NET_BIND_SERVICE` | Bind to privileged ports (<1024) | Servicios como nginx |
| `CAP_NET_RAW` | Raw sockets para ping, etc. | Diagnostic tools |
| `CAP_SYS_CHROOT` |chroot | Aislamiento de filesystem |
| `CAP_AUDIT_WRITE` | Escribir al audit log | Logging |
| `CAP_SETFCAP` | Set file capabilities | Gestión de capabilities |

Capabilities que Docker **elimina** por defecto:

| Capability | Qué permite | Por qué Docker la elimina |
|-----------|-----------|--------------------------|
| `CAP_SYS_ADMIN` | mount, pivot_root, BPF, sethostname, etc. | **La más peligrosa** — permite escape de contenedor |
| `CAP_NET_ADMIN` | Configurar interfaces, iptables, routing | Podría manipular la red del host |
| `CAP_SYS_PTRACE` | Trazar procesos con ptrace | Podría inspect otros contenedores |
| `CAP_SYS_MODULE` | Cargar módulos del kernel | Acceso directo al kernel |
| `CAP_SYS_RAWIO` | Acceso directo a dispositivos I/O | Acceso al hardware |
| `CAP_SYSLOG` |syslog, dmesg | Información sensible del sistema |
| `CAP_SYS_TIME` |settimeofday, adjtimex | Puede manipular el reloj del sistema |
| `CAP_SYS_RESOURCE` |override resource limits | Puede consumir recursos ilimitados |
| `DAC_READ_SEARCH` | Read any file or directory | Rebyta CAP_DAC_OVERRIDE |

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
| Leer archivos de otros usuarios | `CAP_DAC_READ_SEARCH` |

Esto no es un problema en la mayoría de aplicaciones — el código de la aplicación
normalmente solo necesita leer su propio código y escribir en directorios específicos.

## Opciones avanzadas de seguridad

### `--cap-drop`

Eliminar capabilities específicas:

```bash
# Ejecutar como root pero sin CAP_SYS_ADMIN
docker run --rm --cap-drop=SYS_ADMIN --user root alpine cat /proc/1/status | grep Cap
# SYS_ADMIN ya no estará en la lista

# Drop todas y agregar solo las necesarias
docker run --rm \
  --cap-drop=ALL \
  --cap-add=NET_BIND_SERVICE \
  --user appuser \
  alpine id
```

### `--read-only`

Hacer el filesystem de solo lectura (excluyendo volúmenes):

```bash
docker run --rm --read-only alpine touch /tmp/test
# Read-only file system

# Combinado con volúmenes para datos
docker run --rm \
  --read-only \
  -v /tmp/writable:/data \
  alpine sh -c 'echo "ok" > /data/test'
```

### `--security-opt`

Opciones de seguridad adicionales:

```bash
# Desactivar AppArmor/SELinux para el contenedor
docker run --rm --security-opt seccomp=unconfined alpine sh

# No也不行 (deny) — perfil de seccomp restrictivo
docker run --rm --security-opt no-new-privileges:true alpine sh
# Evita que el contenedor gane más privileges via setuid binaries
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

Cuando usas user namespace remapping (`userns-remap`), el UID 0 del contenedor
se mapea a un UID sin privilegios del host (ej: 100000). Esto añade otra capa
de seguridad, pero tiene implicaciones con bind mounts.

```json
// /etc/docker/daemon.json
{
  "userns-remap": "default"
}
```

Con esta configuración, incluso si ejecutas como root dentro del contenedor,
eres un UID no privilegiado en el host.

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
# CapInh: 00000000a80425fb
# CapPrm: 00000000a80425fb
# CapEff: 00000000a80425fb
# CapBnd: 00000000a80425fb

docker rm -f test_root
```

### Ejercicio 2 — Demostrar el riesgo de bind mount + root

```bash
# Crear un archivo de prueba propiedad de tu usuario
echo "mis datos" > /tmp/test_root_risk.txt
ls -la /tmp/test_root_risk.txt

# El contenedor como root puede sobrescribirlo
docker run --rm -v /tmp/test_root_risk.txt:/data/test.txt alpine \
  sh -c 'echo "sobrescrito por root" > /data/test.txt'

cat /tmp/test_root_risk.txt
# sobrescrito por root — ¡tu archivo fue modificado por UID 0!

rm /tmp/test_root_risk.txt
```

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
docker run --rm nonroot-test sh -c 'apk add curl 2>&1' || echo "Falla: no soy root"
docker run --rm nonroot-test sh -c 'touch /etc/test 2>&1' || echo "Falla: sin permisos"

# Sobrescribir con --user root (para debugging)
docker run --rm --user root nonroot-test id
# uid=0(root)

docker rmi nonroot-test
rm /tmp/Dockerfile.nonroot
```

### Ejercicio 4 — Cap-drop para limitar privilegios

```bash
# Ejecutar como root pero sin capabilities peligrosas
docker run --rm --cap-drop=ALL --user root alpine \
  sh -c 'cat /proc/1/status | grep Cap'

# Comparar con capabilities completas
docker run --rm --user root alpine \
  sh -c 'cat /proc/1/status | grep Cap'

# Intentar cosas que requieren capabilities
docker run --rm --user root --cap-drop=NET_ADMIN alpine \
  ip link add dummy0 type dummy 2>&1
# Operation not permitted — NET_ADMIN fue eliminada
```

### Ejercicio 5 — Read-only filesystem

```bash
# Intentar escribir en filesystem readonly
docker run --rm --read-only alpine sh -c 'echo "test" > /tmp/test'
# Read-only file system

# Con volumen writable
docker run --rm --read-only -v /tmp:/data alpine \
  sh -c 'echo "test" > /data/test && cat /data/test'

# Ver qué directorios son especiales (tmp, proc, etc)
docker run --rm --read-only alpine sh -c 'ls -la /'
# dr-xr-xr-x   1 root root   4096 Jan  ...  sys
# drwxrwxrwt   1 root root   4096 Jan  ...  tmp
# dr-xr-x    1 root root   0 Jan    ...  proc
```

### Ejercicio 6 — Docker socket como riesgo

```bash
# Esto es SOLO para demostración - NUNCA hagas esto en producción

# El socket de Docker permite controlar el daemon
docker run --rm -v /var/run/docker.sock:/var/run/docker.sock \
  docker:cli docker ps
# Lista todos los contenedores del host

# Con acceso al socket puedes crear un contenedor con acceso al host
docker run --rm -v /var/run/docker.sock:/var/run/docker.sock \
  docker:cli docker run --rm -v /:/host alpine ls /host

# Ver quién tiene acceso al socket
ls -la /var/run/docker.sock
# srw-rw---- 1 root docker  0 Jan  ...  /var/run/docker.sock
# Solo el grupo docker puede acceder
```

### Ejercicio 7 — Privileged containers (peligroso)

```bash
# --privileged otorga TODAS las capabilities
docker run --rm --privileged alpine sh -c 'cat /proc/1/status | grep Cap'
# CapInh: 0000003fffffffff
# CapPrm: 0000003fffffffff
# CapEff: 0000003fffffffff
# TODAS las capabilities están presentes

# Un contenedor privileged puede:
# - Acceder a todos los dispositivos
# - Mountar filesystems
# - Configurar red
# - Leer memoria de otros procesos

# NUNCA usar --privileged en producción
```

### Ejercicio 8 — No new privileges

```bash
# --security-opt no-new-privileges:true
# Evita que setuid binaries te den más privileges

# Crear un binary setuid en el host (para demostración)
sudo chmod u+s /bin/ping  # Esto es ilustrativo, no lo hagas en producción

# Con no-new-privileges, even si el binary es setuid, no ganas root
docker run --rm --security-opt no-new-privileges:true alpine \
  ping -c 1 127.0.0.1
# Funciona (CAP_NET_RAW está disponible), pero no ganha privileges adicionales
```
