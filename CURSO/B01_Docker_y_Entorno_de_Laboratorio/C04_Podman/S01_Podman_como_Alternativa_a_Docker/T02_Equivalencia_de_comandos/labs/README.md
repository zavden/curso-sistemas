# Lab — Equivalencia de comandos

## Objetivo

Laboratorio práctico para demostrar que Podman es un drop-in replacement
de Docker CLI: ejecutar los mismos comandos cambiando solo `docker` por
`podman`, usar un alias para transparencia total, construir imágenes con
Buildah, y entender la diferencia de registries.

## Prerequisitos

- Podman y Docker instalados
- Imagen descargada: `podman pull docker.io/library/alpine:latest`
- Estar en el directorio `labs/` de este tópico

## Archivos del laboratorio

| Archivo | Propósito |
|---|---|
| `Dockerfile.build` | Dockerfile para probar `podman build` |
| `app.sh` | Script simple para el build |

---

## Parte 1 — Mismos comandos

### Objetivo

Demostrar que los comandos básicos de Docker y Podman son idénticos:
solo cambia el nombre del binario.

### Paso 1.1: run

```bash
echo "=== Docker ==="
docker run --rm docker.io/library/alpine echo "Hello from Docker"

echo "=== Podman ==="
podman run --rm docker.io/library/alpine echo "Hello from Podman"
```

Salida idéntica en ambos casos. Solo cambia `docker` por `podman`.

### Paso 1.2: Ciclo completo con Docker

```bash
docker run -d --name lab-docker docker.io/library/alpine sleep 300
docker ps --filter name=lab-docker --format "{{.Names}} {{.Status}}"
docker exec lab-docker cat /etc/os-release | head -2
docker logs lab-docker
docker stop lab-docker
docker rm lab-docker
```

### Paso 1.3: Mismo ciclo con Podman

```bash
podman run -d --name lab-podman docker.io/library/alpine sleep 300
podman ps --filter name=lab-podman --format "{{.Names}} {{.Status}}"
podman exec lab-podman cat /etc/os-release | head -2
podman logs lab-podman
podman stop lab-podman
podman rm lab-podman
```

Exactamente los mismos comandos y flags. La salida es funcionalmente
idéntica.

### Paso 1.4: Volúmenes y redes

```bash
podman volume create lab-vol
podman volume ls | grep lab-vol
podman volume inspect lab-vol | head -5
podman volume rm lab-vol

podman network create lab-net
podman network ls | grep lab-net
podman network rm lab-net
```

Los subcomandos de volúmenes y redes también son idénticos.

---

## Parte 2 — Alias docker=podman

### Objetivo

Demostrar que con un alias, scripts escritos para Docker funcionan
con Podman sin cambiar una sola línea.

### Paso 2.1: Configurar el alias

```bash
alias docker=podman
```

### Paso 2.2: Ejecutar "Docker" (que en realidad es Podman)

```bash
docker --version
```

Salida esperada:

```
podman version X.X.X
```

El alias redirige `docker` a `podman` transparentemente.

### Paso 2.3: Operaciones completas via alias

```bash
docker run --rm docker.io/library/alpine echo "Soy Podman disfrazado"
docker images | head -3
docker ps -a | head -3
```

Todo funciona. Scripts existentes que usan `docker` funcionarían
sin modificación.

### Paso 2.4: Limitaciones del alias

```bash
docker compose version 2>&1
```

Salida esperada: error. `docker compose` (subcomando de Docker CLI)
no existe en Podman. El alias solo funciona para comandos que Podman
soporta directamente.

### Paso 2.5: Restaurar

```bash
unalias docker
docker --version
```

Ahora `docker` vuelve a ser Docker.

---

## Parte 3 — Build con Podman

### Objetivo

Demostrar que `podman build` usa Buildah internamente y construye
imágenes a partir de Dockerfiles estándar sin cambios.

### Paso 3.1: Examinar los archivos

```bash
cat Dockerfile.build
cat app.sh
```

Un Dockerfile estándar que instala curl, copia un script, y lo
ejecuta. Funciona con Docker y con Podman.

### Paso 3.2: Construir con Podman

```bash
podman build -t lab-equiv -f Dockerfile.build .
```

Podman usa Buildah internamente. La sintaxis y flags son las mismas
que `docker build`.

### Paso 3.3: Ejecutar

```bash
podman run --rm lab-equiv
```

Salida esperada:

```
Container runtime: ...
Hostname: <container-id>
User: uid=0(root) gid=0(root)
```

### Paso 3.4: Inspeccionar la imagen

```bash
podman history lab-equiv
```

```bash
podman inspect lab-equiv --format '{{.Config.Cmd}}'
```

Los comandos de inspección también son idénticos a Docker.

### Paso 3.5: Build con argumento

```bash
podman build --build-arg VERSION=2.0 -t lab-equiv -f Dockerfile.build .
```

`--build-arg` funciona igual. Los flags estándar de Dockerfile
(`FROM`, `RUN`, `COPY`, `CMD`, `ENV`, `ARG`, `WORKDIR`, etc.) son
todos soportados.

### Paso 3.6: Limpiar

```bash
podman rmi lab-equiv
```

---

## Parte 4 — Registries

### Objetivo

Demostrar la diferencia más visible entre Docker y Podman: la
resolución de nombres de imagen y la configuración de registries.

### Paso 4.1: Docker resuelve automáticamente

```bash
docker pull alpine:latest 2>&1 | head -3
```

Docker siempre busca en `docker.io/library/` sin preguntar.

### Paso 4.2: Podman puede preguntar

```bash
podman pull alpine 2>&1 | head -5
```

Podman puede mostrar una lista de registries para elegir, dependiendo
de la configuración.

### Paso 4.3: Nombre completo evita la ambigüedad

```bash
podman pull docker.io/library/alpine:latest
```

Con el nombre completo (`docker.io/library/nombre:tag`), Podman
descarga sin preguntar. Es la forma recomendada.

### Paso 4.4: Ver configuración de registries

```bash
cat /etc/containers/registries.conf 2>/dev/null | grep -v "^#" | grep -v "^$" | head -10
```

El archivo `registries.conf` define qué registries consulta Podman
para nombres sin prefijo.

### Paso 4.5: Ver short-name aliases

```bash
cat /etc/containers/registries.conf.d/shortnames.conf 2>/dev/null | head -15
```

En Fedora/RHEL, este archivo mapea nombres cortos a registries
específicos (ej. `"nginx" = "docker.io/library/nginx"`).

### Paso 4.6: Limpiar

```bash
podman rmi docker.io/library/alpine:latest 2>/dev/null
```

---

## Limpieza final

```bash
# Eliminar recursos del lab (si alguno quedó)
podman rm -f lab-podman 2>/dev/null
docker rm -f lab-docker 2>/dev/null
podman rmi lab-equiv 2>/dev/null
podman volume rm lab-vol 2>/dev/null
podman network rm lab-net 2>/dev/null
```

---

## Conceptos reforzados

1. Los comandos de Docker y Podman son **idénticos** para la mayoría
   de operaciones: run, ps, exec, logs, stop, rm, build, images,
   volume, network.

2. Un alias `docker=podman` permite que scripts existentes funcionen
   con Podman sin cambios. La limitación son los subcomandos exclusivos
   de Docker (compose, swarm).

3. `podman build` usa **Buildah** internamente y soporta la sintaxis
   completa de Dockerfile. Los flags estándar (`-t`, `-f`,
   `--build-arg`) son idénticos.

4. La diferencia más visible es la **resolución de registries**: Docker
   siempre busca en `docker.io`; Podman consulta múltiples registries
   configurados. Usar nombres completos
   (`docker.io/library/nombre:tag`) evita ambigüedad.

5. Podman tiene comandos **exclusivos** que Docker no tiene:
   `podman pod`, `podman generate systemd`, `podman generate kube`,
   `podman play kube`.
