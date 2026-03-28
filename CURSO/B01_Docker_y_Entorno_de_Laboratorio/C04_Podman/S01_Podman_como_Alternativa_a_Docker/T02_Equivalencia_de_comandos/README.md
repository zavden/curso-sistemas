# T02 — Equivalencia de comandos

## Drop-in replacement

Podman fue diseñado como un **reemplazo directo** de Docker CLI. La mayoría de
comandos son idénticos — solo cambia `docker` por `podman`:

## Tabla de equivalencia

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

### Volúmenes y redes

| Docker | Podman | Notas |
|---|---|---|
| `docker volume create` | `podman volume create` | Idéntico |
| `docker volume ls` | `podman volume ls` | Idéntico |
| `docker volume rm` | `podman volume rm` | Idéntico |
| `docker volume inspect` | `podman volume inspect` | Idéntico |
| `docker network create` | `podman network create` | Idéntico |
| `docker network ls` | `podman network ls` | Idéntico |
| `docker network rm` | `podman network rm` | Idéntico |
| `docker network connect` | `podman network connect` | Idéntico |

### Sistema

| Docker | Podman | Notas |
|---|---|---|
| `docker info` | `podman info` | Diferente formato de salida |
| `docker system df` | `podman system df` | Idéntico |
| `docker system prune` | `podman system prune` | Idéntico |
| `docker login` | `podman login` | Idéntico |
| `docker logout` | `podman logout` | Idéntico |

### Exclusivos de Podman

| Comando | Descripción |
|---|---|
| `podman pod create` | Crear pods (sin equivalente en Docker) |
| `podman pod ps` | Listar pods |
| `podman generate systemd` | Generar unit files de systemd |
| `podman generate kube` | Generar YAML de Kubernetes |
| `podman play kube` | Crear pods desde YAML de Kubernetes |
| `podman system service` | Iniciar API server compatible con Docker |
| `podman machine` | Gestionar VMs (macOS/Windows) |
| `podman healthcheck run` | Ejecutar healthcheck manualmente |

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

## `podman build` y Buildah

Podman usa **Buildah** internamente para construir imágenes. La sintaxis de
Dockerfile es la misma, pero hay diferencias con BuildKit:

```bash
# Funciona igual
podman build -t myapp .
podman build -t myapp -f Dockerfile.prod .
podman build --build-arg VERSION=2.0 -t myapp .
```

### Lo que NO funciona (features de BuildKit)

```dockerfile
# BuildKit cache mounts — NO soportado por Buildah
RUN --mount=type=cache,target=/root/.cache/pip pip install -r requirements.txt

# BuildKit secret mounts — NO soportado por Buildah
RUN --mount=type=secret,id=mysecret cat /run/secrets/mysecret

# syntax directive — ignorada por Buildah
# syntax=docker/dockerfile:1
```

Para la mayoría de Dockerfiles estándar, la diferencia es irrelevante. Solo afecta
a optimizaciones avanzadas de BuildKit.

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
