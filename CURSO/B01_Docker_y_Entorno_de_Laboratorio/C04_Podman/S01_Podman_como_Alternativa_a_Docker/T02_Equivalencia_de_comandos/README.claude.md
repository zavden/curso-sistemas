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
| `docker top` | `podman top` | Podman añade opciones extra (capabilites, seccomp) |
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
| `podman pod create/ps/rm/start/stop` | Gestionar pods (concepto de Kubernetes) |
| `podman generate kube` | Generar YAML de Kubernetes desde contenedores |
| `podman kube play` | Crear pods desde YAML de Kubernetes |
| `podman system service` | Iniciar API server compatible con Docker |
| `podman machine init/start/stop` | Gestionar VMs (macOS/Windows) |
| `podman healthcheck run` | Ejecutar healthcheck manualmente |

> **Nota sobre `podman generate systemd`**: Este comando existe pero está
> **deprecado desde Podman 4.4**. La alternativa recomendada es **Quadlet**:
> archivos `.container` en `~/.config/containers/systemd/` que systemd
> gestiona directamente.

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
features específicos de Docker (Docker Swarm, plugins de Docker CLI) pueden
necesitar ajustes.

### Limitaciones del alias

```bash
alias docker=podman

# Docker Compose como plugin de Docker CLI
docker compose up -d
# En Podman < 4.7: error ("compose" no es un subcomando de podman)
# En Podman ≥ 4.7: funciona (podman compose delega a un compose externo)

# Docker Swarm no existe en Podman
docker stack deploy ...  # ❌ No hay equivalente
docker service create ...  # ❌ No hay equivalente
```

> **`podman compose`** (Podman 4.7+): Podman delega a un proveedor de compose
> externo (`docker-compose` o `podman-compose`). No es un reimplementación —
> requiere tener instalado uno de esos proveedores.

### `podman-docker`: el alias del sistema

```bash
# En Fedora/RHEL, el paquete podman-docker crea:
# 1. Un symlink /usr/bin/docker → /usr/bin/podman
# 2. Un man page wrapper
sudo dnf install podman-docker

docker --version
# podman version 4.x.x
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
# 1. Usar el nombre completo (recomendado en scripts)
podman pull docker.io/library/nginx

# 2. Configurar solo docker.io como search registry
# En ~/.config/containers/registries.conf:
# unqualified-search-registries = ["docker.io"]

# 3. Usar short-name aliases (preconfigurados en Fedora/RHEL)
# /etc/containers/registries.conf.d/shortnames.conf
# [aliases]
# "nginx" = "docker.io/library/nginx"
```

El archivo de short-name aliases ya viene preconfigurado en Fedora/RHEL con las
imágenes más comunes mapeadas a sus registries oficiales.

## `podman build` y Buildah

Podman usa **Buildah** internamente para construir imágenes. La sintaxis de
Dockerfile es la misma:

```bash
# Funciona igual
podman build -t myapp .
podman build -t myapp -f Dockerfile.prod .
podman build --build-arg VERSION=2.0 -t myapp .
```

### Soporte de features avanzados

Buildah 1.24+ (incluido en Podman 4.x) soporta la mayoría de features de
BuildKit:

```dockerfile
# Cache mounts — soportado en Buildah 1.24+
RUN --mount=type=cache,target=/root/.cache/pip \
    pip install -r requirements.txt

# Secret mounts — soportado en Buildah 1.24+
RUN --mount=type=secret,id=mysecret cat /run/secrets/mysecret

# SSH agent forwarding — soportado en Buildah 1.24+
RUN --mount=type=ssh git clone git@github.com:user/repo.git
```

```bash
# Usar secrets en el build
podman build --secret id=mysecret,src=./secret.txt -t myapp .
```

### Lo que NO tiene equivalente

```dockerfile
# syntax directive — ignorada por Buildah
# syntax=docker/dockerfile:1

# Buildx (multi-architecture builds de Docker) — usar buildah directamente
# docker buildx build --platform linux/amd64,linux/arm64 ...
```

Para multi-arch builds con Podman, se usa `podman build --platform` o Buildah
directamente con `buildah manifest`.

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

## `podman generate kube` y `podman kube play`

Podman puede generar YAML de Kubernetes desde contenedores existentes y
recrearlos desde ese YAML:

```bash
# Crear contenedor
podman run -d --name myapp -p 8080:80 docker.io/library/nginx

# Generar kube YAML
podman generate kube myapp > myapp.yaml

# El YAML generado es un Pod de Kubernetes válido
cat myapp.yaml
# apiVersion: v1
# kind: Pod
# metadata:
#   name: myapp
# ...

# Eliminar y recrear desde YAML
podman rm -f myapp
podman kube play myapp.yaml
```

## API compatible con Docker

Para herramientas que necesitan conectarse al socket de Docker:

```bash
# Activar el socket Podman (rootless, vía systemd)
systemctl --user enable --now podman.socket

# El socket queda en:
echo $XDG_RUNTIME_DIR/podman/podman.sock

# Herramientas que usan DOCKER_HOST pueden conectarse
export DOCKER_HOST=unix://$XDG_RUNTIME_DIR/podman/podman.sock
docker ps    # Usa la API de Podman transparentemente
```

> **Nota**: `podman system service --time=0 &` también funciona para iniciar
> la API manualmente, pero la activación vía systemd socket es preferible
> (se inicia bajo demanda y se detiene automáticamente).

---

## Archivos del lab

```
labs/
├── Dockerfile.build  ← Dockerfile para probar podman build
└── app.sh            ← Script simple para el build
```

---

## Ejercicios

### Ejercicio 1 — Mismas operaciones con Docker y Podman

```bash
echo "=== Docker ==="
docker run --rm docker.io/library/alpine cat /etc/os-release | head -2

echo "=== Podman ==="
podman run --rm docker.io/library/alpine cat /etc/os-release | head -2
```

**Predicción**: ¿La salida será idéntica?

<details><summary>Respuesta</summary>

Sí. Ambos ejecutan la misma imagen OCI (`alpine`) y el mismo comando
(`cat /etc/os-release`). La salida es idéntica porque la imagen es la misma.

</details>

### Ejercicio 2 — El alias docker=podman

```bash
alias docker=podman

docker --version
```

**Predicción**: ¿Qué versión muestra? ¿Docker o Podman?

```bash
docker run --rm docker.io/library/alpine echo "Soy Podman disfrazado"
docker images | head -3
docker ps -a | head -3
```

<details><summary>Respuesta</summary>

Muestra la versión de Podman. El alias redirige `docker` a `podman`
transparentemente. Todos los comandos funcionan porque Podman acepta
los mismos flags y subcomandos.

</details>

**Predicción**: ¿`docker compose version` funcionará con el alias?

```bash
docker compose version 2>&1
```

<details><summary>Respuesta</summary>

Depende de la versión de Podman:
- Podman < 4.7: error — `compose` no existe como subcomando
- Podman ≥ 4.7: funciona si hay un proveedor de compose instalado

`compose` en Docker es un plugin de CLI. Podman ≥ 4.7 lo replica delegando
a `docker-compose` o `podman-compose`.

</details>

```bash
unalias docker
```

### Ejercicio 3 — Explorar registries

```bash
cat /etc/containers/registries.conf 2>/dev/null | grep -v "^#" | grep -v "^$" | head -10
```

**Predicción**: ¿Cuántos registries de búsqueda están configurados?

```bash
cat /etc/containers/registries.conf.d/shortnames.conf 2>/dev/null | head -20
```

<details><summary>Respuesta</summary>

En Fedora típicamente 3: `registry.fedoraproject.org`, `docker.io`, `quay.io`.
El archivo `shortnames.conf` mapea nombres cortos como `nginx`, `redis`,
`postgres` a sus registries oficiales (generalmente `docker.io/library/`).

</details>

```bash
podman pull docker.io/library/alpine:latest
podman images | grep alpine
podman rmi docker.io/library/alpine:latest
```

### Ejercicio 4 — Build con Docker y Podman

Demuestra que el mismo Dockerfile produce la misma imagen en ambos.

```bash
cd labs/

# Examinar los archivos
cat Dockerfile.build
cat app.sh
```

**Predicción**: ¿`podman build` producirá el mismo resultado que `docker build`?

```bash
# Build con Podman
podman build -t lab-equiv -f Dockerfile.build .
podman run --rm lab-equiv

# Build con Docker (si disponible)
docker build -t lab-equiv -f Dockerfile.build . 2>/dev/null
docker run --rm lab-equiv 2>/dev/null || echo "Docker no disponible"
```

<details><summary>Respuesta</summary>

Sí. Buildah (usado por `podman build`) entiende la sintaxis completa de
Dockerfile. El resultado es funcionalmente idéntico: misma imagen OCI,
mismo output.

</details>

```bash
podman rmi lab-equiv 2>/dev/null
docker rmi lab-equiv 2>/dev/null
```

### Ejercicio 5 — Generar YAML de Kubernetes

Demuestra que Podman puede exportar contenedores como YAML de Kubernetes.

```bash
podman run -d --name lab-kube -p 8080:80 docker.io/library/nginx:alpine
```

**Predicción**: ¿`podman generate kube` producirá un Pod o un Deployment?

```bash
podman generate kube lab-kube
```

<details><summary>Respuesta</summary>

Un **Pod** (`kind: Pod`). Podman genera Pods individuales, no Deployments.
Para Deployments, se modifica el YAML manualmente o se usan herramientas
de Kubernetes.

</details>

```bash
# Guardar y verificar que se puede recrear
podman generate kube lab-kube > /tmp/lab-kube.yaml
podman rm -f lab-kube
podman kube play /tmp/lab-kube.yaml
podman ps

# Limpiar
podman kube down /tmp/lab-kube.yaml
rm -f /tmp/lab-kube.yaml
```

### Ejercicio 6 — BuildKit features en Podman

Demuestra que Buildah moderno soporta cache mounts.

```bash
mkdir -p /tmp/build_cache && cd /tmp/build_cache

cat > Dockerfile << 'EOF'
FROM docker.io/library/alpine:latest

# Cache mount: el contenido de /var/cache/apk persiste entre builds
RUN --mount=type=cache,target=/var/cache/apk \
    apk add --no-cache curl

CMD ["curl", "--version"]
EOF

podman build -t lab-cache .
podman run --rm lab-cache | head -1
```

**Predicción**: ¿El `--mount=type=cache` funciona en Podman, o falla?

<details><summary>Respuesta</summary>

Funciona. Buildah 1.24+ (incluido en Podman 4.x) soporta `--mount=type=cache`,
`--mount=type=secret` y `--mount=type=ssh`. Estos features de BuildKit ya no
son exclusivos de Docker.

</details>

```bash
podman rmi lab-cache
cd / && rm -rf /tmp/build_cache
```

### Ejercicio 7 — API compatible con Docker

Demuestra que el socket de Podman puede servir como reemplazo del socket Docker.

```bash
# Verificar si el socket está activo
systemctl --user status podman.socket 2>/dev/null
```

**Predicción**: ¿Se puede usar `curl` contra el socket de Podman como si fuera
el API de Docker?

```bash
# Activar el socket (si no lo está)
systemctl --user start podman.socket 2>/dev/null

# Consultar la API (compatible con Docker API)
curl -s --unix-socket $XDG_RUNTIME_DIR/podman/podman.sock \
  http://localhost/v4.0.0/containers/json 2>/dev/null | head -5 || echo "Socket no disponible"
```

<details><summary>Respuesta</summary>

Sí. El socket de Podman habla el protocolo de la API de Docker. Herramientas
que conectan al socket Docker (portainer, lazydocker, etc.) pueden usar el
socket de Podman configurando `DOCKER_HOST`.

</details>

```bash
systemctl --user stop podman.socket 2>/dev/null
```

### Ejercicio 8 — podman-docker: alias a nivel de sistema

```bash
# Verificar si podman-docker está instalado
rpm -q podman-docker 2>/dev/null || dpkg -l podman-docker 2>/dev/null || echo "No instalado"
```

**Predicción**: ¿Qué hace el paquete `podman-docker`? ¿Es solo un alias?

<details><summary>Respuesta</summary>

`podman-docker` crea un symlink `/usr/bin/docker → /usr/bin/podman` a nivel
de sistema (no un alias de shell). Esto hace que **cualquier** herramienta
o script que llame a `docker` use Podman automáticamente, incluyendo
herramientas que no pasan por el shell (como IDEs o CI/CD).

Es más robusto que `alias docker=podman` porque funciona fuera del shell.

</details>

---

## Resumen de equivalencias

```
Docker                              Podman
───────────────────────────────────────────────────────────────
docker run                          podman run
docker ps                           podman ps
docker images                       podman images
docker build                        podman build (Buildah)
docker pull                         podman pull
docker push                         podman push
docker volume *                     podman volume *
docker network *                    podman network *
docker exec                         podman exec
docker logs                         podman logs
docker stop / start                 podman stop / start
docker compose                      podman compose (4.7+)
───────────────────────────────────────────────────────────────
                                    podman pod *       ← Exclusivo
                                    podman generate    ← Exclusivo
                                    podman kube *      ← Exclusivo
                                    podman machine *   ← Exclusivo
───────────────────────────────────────────────────────────────
docker stack / service / swarm      ❌ Sin equivalente en Podman
```
