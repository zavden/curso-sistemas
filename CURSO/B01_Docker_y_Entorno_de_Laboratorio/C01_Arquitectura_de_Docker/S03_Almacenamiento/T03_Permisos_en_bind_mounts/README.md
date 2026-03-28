# T03 — Permisos en bind mounts

## El kernel compartido y los UIDs

Un contenedor Linux **no tiene su propio kernel**. Comparte el kernel del host.
Esto tiene una consecuencia directa en los permisos de archivos: los **UIDs y GIDs
dentro del contenedor son los mismos que en el host**. No hay traducción ni capa
de abstracción — el kernel ve un solo espacio de UIDs.

```
┌────────────────────────────────────┐
│              HOST                  │
│                                    │
│   Usuario: zavden (UID 1000)       │
│   Archivo: ./proyecto/main.c       │
│   Propietario: UID 1000            │
│                                    │
│   ┌──────────────────────────┐     │
│   │       CONTENEDOR         │     │
│   │                          │     │
│   │   Proceso: root (UID 0)  │     │
│   │   Bind mount: /app       │     │
│   │                          │     │
│   │   echo "x" > /app/out.txt│     │
│   │   → Propietario: UID 0   │     │
│   └──────────────────────────┘     │
│                                    │
│   ls -la ./proyecto/out.txt        │
│   → root:root  out.txt  ← ¡!      │
│   → Tu usuario no puede editarlo   │
└────────────────────────────────────┘
```

## El problema clásico

El escenario más común: montas tu directorio de trabajo como bind mount, el
contenedor corre como root (UID 0), y los archivos generados por el contenedor
pertenecen a root en tu host.

```bash
# Tu usuario tiene UID 1000
id
# uid=1000(zavden) gid=1000(zavden) ...

# Montar el directorio actual como bind mount
docker run --rm -v $(pwd):/app alpine sh -c 'echo "generado" > /app/output.txt'

# Ver permisos del archivo creado
ls -la output.txt
# -rw-r--r-- 1 root root 9 Jan 15 10:30 output.txt

# Intentar editar/borrar
rm output.txt
# rm: cannot remove 'output.txt': Permission denied
```

Este problema ocurre porque:
1. El proceso dentro del contenedor corre como **root (UID 0)** por defecto
2. Crea el archivo `output.txt` con propietario **UID 0**
3. En tu host, UID 0 es **root** — tu usuario (UID 1000) no tiene permisos

## Solución 1: `--user` en `docker run`

La forma más directa: ejecutar el contenedor con tu UID/GID:

```bash
# Pasar tu UID y GID al contenedor
docker run --rm \
  --user $(id -u):$(id -g) \
  -v $(pwd):/app \
  alpine sh -c 'echo "generado" > /app/output.txt'

# Verificar permisos
ls -la output.txt
# -rw-r--r-- 1 zavden zavden 9 Jan 15 10:30 output.txt
# ✓ Correcto: propiedad de tu usuario
```

**Efecto secundario**: el proceso dentro del contenedor no corre como root, por lo
que no puede:
- Instalar paquetes (`apt-get install`)
- Escribir en directorios del sistema (`/etc`, `/usr`)
- Hacer bind a puertos privilegiados (<1024)

**Warning "I have no name!"**: si el UID que pasas no existe en `/etc/passwd` del
contenedor, verás este mensaje en el prompt:

```bash
docker run --rm -it --user 1000:1000 alpine sh
# I have no name!@abc123:/$
```

Esto es cosmético. Las operaciones de I/O sobre el bind mount funcionan
correctamente. El proceso tiene UID 1000, que es lo que importa para los permisos.

## Solución 2: Crear un usuario en el Dockerfile

Para evitar el warning y tener un entorno más limpio, crea un usuario en el
Dockerfile con el mismo UID que usarás:

```dockerfile
FROM debian:bookworm

# Crear un usuario con UID configurable
ARG USER_UID=1000
ARG USER_GID=1000

RUN groupadd --gid $USER_GID devuser && \
    useradd --uid $USER_UID --gid $USER_GID --create-home devuser

# Cambiar al usuario no-root
USER devuser
WORKDIR /home/devuser
```

```bash
# Build con tu UID
docker build --build-arg USER_UID=$(id -u) --build-arg USER_GID=$(id -g) -t dev .

# Los archivos creados tendrán tu UID
docker run --rm -v $(pwd):/app dev sh -c 'echo "ok" > /app/test.txt'
ls -la test.txt
# -rw-r--r-- 1 zavden zavden 3 Jan 15 10:30 test.txt
```

**Ventaja sobre `--user`**: el usuario existe en `/etc/passwd`, tiene home directory,
y la imagen es reutilizable.

**Desventaja**: el UID está fijado en build time. Si otro desarrollador tiene
UID 1001, necesita reconstruir la imagen con su UID.

## Solución 3: Entrypoint que ajusta permisos

Algunas imágenes usan un entrypoint script que detecta el UID del propietario
del bind mount y ajusta los permisos dinámicamente:

```bash
#!/bin/bash
# entrypoint.sh

# Detectar el UID del directorio montado
HOST_UID=$(stat -c '%u' /app)
HOST_GID=$(stat -c '%g' /app)

# Si no somos ese usuario, cambiar el UID del usuario dentro del contenedor
if [ "$HOST_UID" != "0" ]; then
    usermod -u "$HOST_UID" devuser 2>/dev/null
    groupmod -g "$HOST_GID" devuser 2>/dev/null
fi

# Ejecutar el comando como devuser
exec gosu devuser "$@"
```

Este patrón es común en imágenes de desarrollo (Node.js, Python). Requiere que
el contenedor arranque como root temporalmente para hacer el `usermod`, y luego
desescale privilegios con `gosu` o `su-exec`.

## Solución 4: User namespace remapping (avanzado)

Docker puede configurarse para **mapear UIDs del contenedor a UIDs sin privilegios
del host**. Así, UID 0 dentro del contenedor se mapea a (por ejemplo) UID 100000
en el host:

```
┌────────────────────────────┐
│        CONTENEDOR          │
│   root = UID 0             │
│   user1 = UID 1000         │
│                            │
│   Mapeado por el kernel:   │
│   UID 0 → UID 100000       │
│   UID 1000 → UID 101000    │
└────────────────────────────┘
```

Configuración en `/etc/docker/daemon.json`:

```json
{
  "userns-remap": "default"
}
```

Esto crea un rango de UIDs subordinados en `/etc/subuid` y `/etc/subgid`. El
root del contenedor ya no tiene privilegios reales en el host.

**Impacto**: esta es una configuración global del daemon de Docker. Afecta a
todos los contenedores y puede romper bind mounts existentes (los archivos del host
tendrán UIDs que no coinciden con los UIDs mapeados). Se estudia en más detalle en
la sección de seguridad (S05).

## `COPY --chown` en Dockerfile

Al copiar archivos al construir la imagen, puedes especificar el propietario:

```dockerfile
FROM debian:bookworm

RUN useradd --create-home --uid 1000 devuser

# Copiar archivos con propiedad del usuario correcto
COPY --chown=devuser:devuser src/ /app/src/

# Sin --chown, los archivos pertenecen a root
COPY config/ /app/config/

USER devuser
```

Sin `--chown`, los archivos copiados con `COPY` y `ADD` pertenecen a **root:root**
por defecto, independientemente de la instrucción `USER`.

## Named volumes vs bind mounts: permisos

Los named volumes **no tienen el problema de permisos** de la misma forma que los
bind mounts. Docker gestiona el almacenamiento internamente, y los permisos dentro
del volumen siguen las reglas del contenedor:

```bash
# Con named volume: root dentro del contenedor = root dentro del volumen
docker run --rm -v mydata:/data alpine sh -c 'echo "ok" > /data/test.txt'

# Esto no afecta a tu directorio de trabajo en el host
# Los archivos del volumen están en /var/lib/docker/volumes/ (propiedad de root)
```

La diferencia es que con named volumes **no te importa** quién es el propietario en
el host, porque nunca accedes directamente a `/var/lib/docker/volumes/`. Solo
accedes a los archivos a través de contenedores.

Con bind mounts, los permisos importan porque los archivos son **tu directorio
de trabajo** en el host.

## macOS y Windows: diferencias

En **macOS y Windows**, Docker Desktop ejecuta los contenedores dentro de una
VM Linux. Los bind mounts pasan a través de una capa de virtualización (gRPC-FUSE,
VirtioFS, osxfs) que **traduce los permisos automáticamente**:

```
Linux nativo:    host UID 1000 ←→ contenedor UID — mismo espacio
Docker Desktop:  host UID ←→ VM ←→ contenedor UID — traducción automática
```

En la práctica, en macOS/Windows los permisos de bind mounts **no son un problema**.
El problema de permisos descrito en este tópico es específico de **Linux nativo**,
que es exactamente el entorno de este curso.

## Resumen de soluciones

| Solución | Complejidad | Portabilidad | Cuándo usarla |
|---|---|---|---|
| `--user $(id -u):$(id -g)` | Baja | Alta | Desarrollo rápido, scripts |
| `USER` en Dockerfile con ARG | Media | Media | Imágenes de desarrollo del equipo |
| Entrypoint con `gosu` | Alta | Alta | Imágenes públicas, multi-usuario |
| User namespace remapping | Alta | Baja | Seguridad en producción |
| Named volumes (evitar bind) | Baja | Alta | Cuando no necesitas acceso directo del host |

Para el **laboratorio de este curso**, la solución recomendada es combinar:
1. Un `USER` en el Dockerfile de desarrollo con UID configurable via `ARG`
2. `--user` como override cuando sea necesario para tareas específicas

---

## Ejercicios

### Ejercicio 1 — Reproducir el problema

```bash
# Crear un directorio de prueba
mkdir -p /tmp/perm_test

# Crear un archivo como tu usuario
echo "original" > /tmp/perm_test/mine.txt

# Crear un archivo desde un contenedor (como root)
docker run --rm -v /tmp/perm_test:/data alpine sh -c 'echo "from container" > /data/theirs.txt'

# Comparar propietarios
ls -la /tmp/perm_test/
# mine.txt   → tu_usuario:tu_grupo
# theirs.txt → root:root

# Intentar eliminar el archivo de root
rm /tmp/perm_test/theirs.txt 2>&1 || echo "Sin permisos para eliminar"

# Limpiar (necesitas sudo)
sudo rm -rf /tmp/perm_test
```

### Ejercicio 2 — Solucionar con `--user`

```bash
mkdir -p /tmp/perm_test

# Ejecutar con tu UID/GID
docker run --rm \
  --user $(id -u):$(id -g) \
  -v /tmp/perm_test:/data \
  alpine sh -c 'echo "from container" > /data/fixed.txt'

# Verificar propietario
ls -la /tmp/perm_test/fixed.txt
# fixed.txt → tu_usuario:tu_grupo  ← correcto

# Limpiar (sin sudo)
rm -rf /tmp/perm_test
```

### Ejercicio 3 — Dockerfile con usuario configurable

```bash
# Crear un Dockerfile temporal
cat > /tmp/Dockerfile.perm << 'DOCKERFILE'
FROM alpine:latest
ARG USER_UID=1000
ARG USER_GID=1000
RUN addgroup -g $USER_GID devuser && \
    adduser -u $USER_UID -G devuser -D devuser
USER devuser
WORKDIR /home/devuser
DOCKERFILE

# Build con tu UID
docker build --build-arg USER_UID=$(id -u) --build-arg USER_GID=$(id -g) \
  -t perm-test -f /tmp/Dockerfile.perm /tmp

# Probar
mkdir -p /tmp/perm_test
docker run --rm -v /tmp/perm_test:/data perm-test sh -c 'echo "ok" > /data/test.txt'
ls -la /tmp/perm_test/test.txt
# test.txt → tu_usuario:tu_grupo

# Verificar el usuario dentro del contenedor
docker run --rm perm-test id
# uid=1000(devuser) gid=1000(devuser)

# Limpiar
rm -rf /tmp/perm_test /tmp/Dockerfile.perm
docker rmi perm-test
```
