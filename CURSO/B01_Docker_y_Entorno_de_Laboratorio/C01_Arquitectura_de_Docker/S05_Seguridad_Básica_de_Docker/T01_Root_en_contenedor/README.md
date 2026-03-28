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

## Capabilities: por qué root en contenedor no es root completo

El kernel Linux divide los privilegios de root en **capabilities** (~40 permisos
granulares). Docker elimina las más peligrosas por defecto, pero mantiene algunas:

```
Root en el host:       TODAS las capabilities
Root en contenedor:    Subconjunto reducido
```

```bash
# Ver capabilities del proceso dentro del contenedor
docker run --rm alpine cat /proc/1/status | grep -i cap
# CapPrm: 00000000a80425fb
# CapEff: 00000000a80425fb

# Decodificar (necesitas capsh en el host)
# Las capabilities que Docker mantiene por defecto incluyen:
# CHOWN, DAC_OVERRIDE, FOWNER, FSETID, KILL, SETGID, SETUID,
# SETPCAP, NET_BIND_SERVICE, NET_RAW, SYS_CHROOT, MKNOD,
# AUDIT_WRITE, SETFCAP
```

Capabilities eliminadas por defecto (las más relevantes):

| Capability | Qué permite | Por qué Docker la elimina |
|---|---|---|
| `CAP_SYS_ADMIN` | mount, pivot_root, BPF, etc. | La más peligrosa — permite escapar del contenedor |
| `CAP_NET_ADMIN` | Configurar interfaces, iptables | Podría manipular la red del host |
| `CAP_SYS_PTRACE` | Trazar procesos con ptrace | Podría inspeccionar otros contenedores |
| `CAP_SYS_MODULE` | Cargar módulos del kernel | Acceso directo al kernel |
| `CAP_SYS_RAWIO` | Acceso directo a dispositivos I/O | Acceso al hardware |

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

---

## Ejercicios

### Ejercicio 1 — Verificar root por defecto

```bash
# Verificar el usuario por defecto
docker run --rm alpine id
docker run --rm debian:bookworm id
docker run --rm nginx id

# Ver el UID del proceso principal
docker run -d --name test_root alpine sleep 3600
docker exec test_root cat /proc/1/status | grep -E '^(Uid|Gid)'
# Uid: 0  0  0  0  — root

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
docker run --rm nonroot-test sh -c 'apk add curl 2>&1 || echo "Falla: no soy root"'
docker run --rm nonroot-test sh -c 'touch /etc/test 2>&1 || echo "Falla: sin permisos"'

# Sobrescribir con --user root (para debugging)
docker run --rm --user root nonroot-test id
# uid=0(root)

docker rmi nonroot-test
rm /tmp/Dockerfile.nonroot
```
