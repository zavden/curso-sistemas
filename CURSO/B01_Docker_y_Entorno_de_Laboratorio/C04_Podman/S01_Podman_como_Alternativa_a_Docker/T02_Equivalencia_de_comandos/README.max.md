# T02 — Equivalencia de comandos

## Drop-in replacement

Podman fue diseñado como un **reemplazo directo** de Docker CLI. La mayoría de
comandos son idénticos — solo cambia `docker` por `podman`:

```bash
# La mayoría de comandos son idénticos
docker run -d nginx
podman run -d nginx

docker ps
podman ps

docker images
podman images
```

## Tabla de equivalencia completa

### Contenedores

| Docker | Podman | Notas |
|---|---|---|
| `docker run` | `podman run` | Idéntico |
| `docker start` | `podman start` | Idéntico |
| `docker stop` | `podman stop` | Idéntico |
| `docker restart` | `podman restart` | Idéntico |
| `docker rm` | `podman rm` | Idéntico |
| `docker ps` | `podman ps` | Idéntico |
| `docker exec` | `podman exec` | Idéntico |
| `docker logs` | `podman logs` | Idéntico |
| `docker inspect` | `podman inspect` | Idéntico |
| `docker attach` | `podman attach` | Idéntico |
| `docker cp` | `podman cp` | Idéntico |
| `docker stats` | `podman stats` | Idéntico |
| `docker top` | `podman top` | Podman tiene opciones extra |
| `docker wait` | `podman wait` | Idéntico |
| `docker kill` | `podman kill` | Idéntico |
| `docker commit` | `podman commit` | Idéntico |
| `docker diff` | `podman diff` | Idéntico |
| `docker rename` | `podman rename` | Idéntico |
| `docker pause` | `podman pause` | Idéntico |
| `docker unpause` | `podman unpause` | Idéntico |

### Imágenes

| Docker | Podman | Notas |
|---|---|---|
| `docker pull` | `podman pull` | Podman puede preguntar el registry |
| `docker push` | `podman push` | Idéntico |
| `docker build` | `podman build` | Usa Buildah internamente |
| `docker images` | `podman images` | Idéntico |
| `docker rmi` | `podman rmi` | Idéntico |
| `docker tag` | `podman tag` | Idéntico |
| `docker save` | `podman save` | Idéntico |
| `docker load` | `podman load` | Idéntico |
| `docker history` | `podman history` | Idéntico |
| `docker image prune` | `podman image prune` | Idéntico |
| `docker import` | `podman import` | Idéntico |
| `docker export` | `podman export` | Idéntico |

### Volúmenes y redes

| Docker | Podman | Notas |
|---|---|---|
| `docker volume create` | `podman volume create` | Idéntico |
| `docker volume ls` | `podman volume ls` | Idéntico |
| `docker volume rm` | `podman volume rm` | Idéntico |
| `docker volume inspect` | `podman volume inspect` | Idéntico |
| `docker volume prune` | `podman volume prune` | Idéntico |
| `docker network create` | `podman network create` | Idéntico |
| `docker network ls` | `podman network ls` | Idéntico |
| `docker network rm` | `podman network rm` | Idéntico |
| `docker network inspect` | `podman network inspect` | Idéntico |
| `docker network connect` | `podman network connect` | Idéntico |
| `docker network disconnect` | `podman network disconnect` | Idéntico |

### Sistema

| Docker | Podman | Notas |
|---|---|---|
| `docker info` | `podman info` | Diferente formato de salida |
| `docker system df` | `podman system df` | Idéntico |
| `docker system prune` | `podman system prune` | Idéntico |
| `docker login` | `podman login` | Idéntico |
| `docker logout` | `podman logout` | Idéntico |
| `docker version` | `podman version` | Idéntico |
| `docker search` | `podman search` | Idéntico |
| `docker events` | `podman events` | Idéntico |

### Exclusivos de Podman

| Comando | Descripción |
|---|---|
| `podman pod create` | Crear pods (sin equivalente en Docker) |
| `podman pod ps` | Listar pods |
| `podman pod inspect` | Inspeccionar un pod |
| `podman pod rm` | Eliminar pods |
| `podman pod start` | Iniciar pods |
| `podman pod stop` | Detener pods |
| `podman generate systemd` | Generar unit files de systemd |
| `podman generate kube` | Generar YAML de Kubernetes |
| `podman play kube` | Crear pods desde YAML de Kubernetes |
| `podman system service` | Iniciar API server compatible con Docker |
| `podman machine init` | Crear VM (macOS/Windows) |
| `podman machine start` | Iniciar VM |
| `podman machine stop` | Detener VM |
| `podman healthcheck run` | Ejecutar healthcheck manualmente |
| `podman image trust` | Gestionar confianza de imágenes |

## El alias `docker=podman`

Para scripts existentes que usan `docker`, un alias permite la transición sin
cambiar código:

```bash
# Añadir al .bashrc o .zshrc
alias docker=podman

# Verificar
docker --version
# podman version 4.x.x

docker run --rm alpine echo "Esto usa Podman"
docker ps
docker images
```

El alias funciona para la **mayoría** de casos. Los scripts que dependen de
features específicos de Docker (BuildKit, Docker Compose, Docker Swarm) pueden
necesitar ajustes.

### Limitaciones del alias

```bash
# NO funciona con Docker Compose
alias docker=podman
docker compose up -d  # ❌ compose busca "docker" socket

# Funciona con podman-compose (herramienta separada)
podman-compose up -d

# NI con Docker BuildKit flags
docker build --secret id=mysecret ...  # ❌ BuildKit no disponible

# NI con docker stack/deploy (Swarm)
docker stack deploy ...  # ❌ Swarm no existe en Podman
```

## Registries: la diferencia más visible

La diferencia más notoria en el uso diario es la resolución de nombres de imagen:

### Docker

```bash
docker pull nginx
# Siempre busca en docker.io/library/nginx — sin preguntar
```

### Podman

```bash
podman pull nginx
# ? Please select an image:
#   registry.fedoraproject.org/nginx
#   docker.io/library/nginx
#   quay.io/nginx
```

Podman consulta los registries configurados en `/etc/containers/registries.conf`:

```bash
cat /etc/containers/registries.conf
# unqualified-search-registries = ["registry.fedoraproject.org", "docker.io", "quay.io"]
```

### Soluciones

```bash
# 1. Usar el nombre completo (recomendado)
podman pull docker.io/library/nginx

# 2. Configurar solo docker.io como search registry
# En ~/.config/containers/registries.conf:
# unqualified-search-registries = ["docker.io"]

# 3. Usar short-name aliases
# /etc/containers/registries.conf.d/shortnames.conf
# [aliases]
# "nginx" = "docker.io/library/nginx"
```

El archivo de short-name aliases ya viene preconfigurado en Fedora/RHEL con las
imágenes más comunes mapeadas a sus registries oficiales.

### Shortnames.conf típico

```bash
# /etc/containers/registries.conf.d/shortnames.conf
[aliases]
"nginx" = "docker.io/library/nginx"
"redis" = "docker.io/library/redis"
"postgres" = "docker.io/library/postgres"
"mysql" = "docker.io/library/mysql"
"ubuntu" = "docker.io/library/ubuntu"
"alpine" = "docker.io/library/alpine"
"centos" = "docker.io/library/centos"
```

## `podman build` y Buildah

Podman usa **Buildah** internamente para construir imágenes. La sintaxis de
Dockerfile es la misma, pero hay diferencias con BuildKit:

```bash
# Funciona igual
podman build -t myapp .
podman build -t myapp -f Dockerfile.prod .
podman build --build-arg VERSION=2.0 -t myapp .
```

### Lo que SÍ funciona

```bash
# ARG y ENV
ARG VERSION=1.0
ENV APP_VERSION=${VERSION}

# COPY y RUN
COPY . /app
RUN pip install -r requirements.txt

# Multi-stage builds
FROM golang AS builder
...

# .dockerignore
```

### Lo que NO funciona (features de BuildKit)

```dockerfile
# BuildKit cache mounts — NO soportado por Buildah
RUN --mount=type=cache,target=/root/.cache/pip \
    pip install -r requirements.txt

# BuildKit secret mounts — NO soportado por Buildah
RUN --mount=type=secret,id=mysecret cat /run/secrets/mysecret

# BuildKit ssh agent — NO soportado por Buildah
# RUN --mount=type=ssh,... make

# syntax directive — ignorada por Buildah
# syntax=docker/dockerfile:1
```

### Alternativas a BuildKit cache en Podman

```bash
# Opción 1: Usar volumen para cache
podman run -v pip-cache:/root/.cache/podman/buildah ...

# Opción 2: Buildah directo con cache mount
buildah bud --layers --Volumes /root/cache:/ccache ...

# Opción 3: Aceptar que cada build recompila
# (No recomendado para CI/CD frecuentes)
```

## Ejemplo: mismas operaciones en Docker y Podman

```bash
# ===== Docker =====
docker pull nginx:latest
docker run -d --name web -p 8080:80 nginx
docker exec web nginx -v
docker logs web --tail 5
docker stop web
docker rm web
docker rmi nginx:latest

# ===== Podman =====
podman pull docker.io/library/nginx:latest
podman run -d --name web -p 8080:80 docker.io/library/nginx
podman exec web nginx -v
podman logs web --tail 5
podman stop web
podman rm web
podman rmi docker.io/library/nginx:latest
```

La única diferencia en el flujo es la referencia completa al registry en Podman.

## `podman generate`: generar systemd y Kubernetes

Podman puede generar unit files de systemd y YAML de Kubernetes:

### Generar systemd units

```bash
# Crear un contenedor
podman run -d --name myapp -p 8080:80 nginx

# Generar unit file
podman generate systemd myapp
# [Unit]
# Description=Container myapp
# ...
```

```bash
# Generar y guardar
podman generate systemd --files --name myapp
# Genera: container-myapp.service

# Con restart policy
podman generate systemd --restart-policy=always --name myapp
```

### Generar YAML de Kubernetes

```bash
# Crear contenedor
podman run -d --name myapp nginx

# Generar kube YAML
podman generate kube myapp
# apiVersion: v1
# kind: Pod
# metadata:
#   name: myapp
# ...
```

```bash
# Guardar a archivo
podman generate kube myapp > myapp.yaml

# Recrear desde YAML
podman play kube myapp.yaml
```

## `podman system service`: API compatible con Docker

Para herramientas que usan el socket Docker:

```bash
# Iniciar el servicio (socket TCP o Unix)
podman system service --time=0 tcp://localhost:2375 &

# Ahora docker CLI puede conectarse
DOCKER_HOST=tcp://localhost:2375 docker ps

# O crear un symlink al socket
ln -sf /run/user/$(id -u)/podman/podman.sock /var/run/docker.sock
```

---

## Ejercicios

### Ejercicio 1 — Mismas operaciones con Docker y Podman

```bash
# Con Docker
echo "=== Docker ==="
docker run --rm docker.io/library/alpine cat /etc/os-release | head -2

# Con Podman
echo "=== Podman ==="
podman run --rm docker.io/library/alpine cat /etc/os-release | head -2

# Salida idéntica
```

### Ejercicio 2 — El alias docker=podman

```bash
# Configurar alias
alias docker=podman

# Ejecutar comandos "Docker" que en realidad usan Podman
docker run --rm docker.io/library/alpine echo "Soy Podman disfrazado de Docker"
docker images
docker ps -a

# Restaurar
unalias docker
```

### Ejercicio 3 — Explorar registries

```bash
# Ver configuración de registries
cat /etc/containers/registries.conf 2>/dev/null | grep -v "^#" | grep -v "^$" | head -10

# Ver short-name aliases
cat /etc/containers/registries.conf.d/shortnames.conf 2>/dev/null | head -20

# Pull con nombre completo (sin ambigüedad)
podman pull docker.io/library/alpine:latest
podman images | grep alpine
podman rmi docker.io/library/alpine:latest
```

### Ejercicio 4 — Generar systemd unit

```bash
# Crear un contenedor
podman run -d --name web -p 8080:80 docker.io/library/nginx

# Generar unit file
podman generate systemd --name web --files

# Ver el contenido
cat container-web.service

# Limpiar
podman rm -f web
```

### Ejercicio 5 — Generar kube YAML

```bash
# Crear contenedores
podman run -d --name app1 docker.io/library/nginx
podman run -d --name app2 docker.io/library/alpine sleep 3600

# Generar kube YAML
podman generate kube app1 > app1.yaml

# Ver el YAML
cat app1.yaml

# Limpiar
podman rm -f app1 app2
rm app1.yaml
```

### Ejercicio 6 — Comparar docker build y podman build

```bash
mkdir -p /tmp/build_test

cat > /tmp/build_test/Dockerfile << 'EOF'
FROM docker.io/library/alpine:latest
RUN echo "Build test: $(date)" > /build_time.txt
CMD cat /build_time.txt
EOF

cd /tmp/build_test

# Build con Docker
docker build -t test:docker .
docker run --rm test:docker

# Build con Podman
podman build -t test:podman .
podman run --rm test:podman

# Limpiar
docker rmi test:docker
podman rmi test:podman
rm -rf /tmp/build_test
```

---

## Resumen de equivalencias

```
Docker                              Podman
───────────────────────────────────────────────────────────────
docker run                          podman run
docker ps                           podman ps
docker images                       podman images
docker build                        podman build
docker pull                         podman pull
docker push                         podman push
docker volume create                podman volume create
docker network create               podman network create
docker exec                         podman exec
docker logs                         podman logs
docker stop / start                 podman stop / start
───────────────────────────────────────────────────────────────
                                    podman pod *      ← Exclusivo
                                    podman generate   ← Exclusivo
                                    podman kube       ← Exclusivo
```
