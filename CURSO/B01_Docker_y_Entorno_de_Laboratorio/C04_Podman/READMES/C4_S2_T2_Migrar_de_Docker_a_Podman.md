# T02 — Migrar de Docker a Podman

## ¿Qué es compatible y qué no?

La migración de Docker a Podman es en su mayoría directa, pero hay diferencias
que requieren ajustes. Esta guía organiza los cambios por nivel de esfuerzo.

## Nivel 1: Compatible sin cambios

### Dockerfiles

Los Dockerfiles funcionan **sin ningún cambio**. Podman usa Buildah internamente,
que entiende la misma sintaxis:

```bash
# El mismo Dockerfile
podman build -t myapp -f Dockerfile .
podman build --build-arg VERSION=2.0 -t myapp .
```

Desde Buildah 1.24+, las directivas de BuildKit para cache, secrets y SSH
también funcionan:

```dockerfile
# Soportado en Buildah 1.24+
RUN --mount=type=cache,target=/root/.cache pip install -r requirements.txt
RUN --mount=type=secret,id=mysecret cat /run/secrets/mysecret
RUN --mount=type=ssh git clone git@github.com:user/repo.git
```

> **No soportado**: La directiva `# syntax=docker/dockerfile:1` y features
> exclusivos de BuildKit como `cache-from`/`cache-to` con registries e inline
> cache.

### Imágenes

Las imágenes son 100% compatibles (formato OCI). La misma imagen funciona en
ambos:

```bash
docker pull nginx:latest
podman pull docker.io/library/nginx:latest
# Idéntica — mismo hash, mismas capas
```

### Comandos CLI básicos

```bash
# docker → podman (cambiar el nombre del comando)
podman run -d --name web -p 8080:80 docker.io/library/nginx
podman exec web nginx -v
podman logs web
podman stop web
podman rm web
```

## Nivel 2: Requiere ajustes menores

### Nombres de imagen completos

Docker asume `docker.io/library/` para imágenes sin prefijo. Podman consulta
múltiples registries:

```bash
# Docker: funciona
docker pull nginx

# Podman: puede preguntar el registry
podman pull nginx
# ? Please select an image...

# Solución: nombre completo
podman pull docker.io/library/nginx
```

**Ajuste en scripts**: reemplazar nombres cortos por nombres completos, o
configurar registries:

```toml
# ~/.config/containers/registries.conf (formato TOML v2)
unqualified-search-registries = ["docker.io"]
```

### SELinux en RHEL/Fedora

En sistemas con SELinux enforcing, los bind mounts necesitan labels:

```bash
# Docker (funciona sin :z en la mayoría de configs)
docker run -v /host/path:/data image

# Podman en RHEL/Fedora con SELinux
podman run -v /host/path:/data:z image
```

**Ajuste**: añadir `:z` o `:Z` a los bind mounts en el Compose file:

```yaml
# Antes (Docker)
volumes:
  - ./src:/app/src

# Después (Podman con SELinux)
volumes:
  - ./src:/app/src:z
```

Recordatorio: `:z` = shared (múltiples contenedores), `:Z` = private (un solo
contenedor).

### Puertos privilegiados (<1024) en rootless

```bash
# Docker rootful: funciona
docker run -p 80:80 nginx

# Podman rootless: falla
podman run -p 80:80 docker.io/library/nginx
# Error: permission denied

# Soluciones:
# 1. Usar un puerto no privilegiado
podman run -p 8080:80 docker.io/library/nginx

# 2. Configurar el kernel (persistente con sysctl.d)
sudo sysctl net.ipv4.ip_unprivileged_port_start=80

# 3. Usar rootful
sudo podman run -p 80:80 docker.io/library/nginx
```

## Nivel 3: Requiere cambios significativos

### Restart policies → systemd/Quadlet

Docker usa `--restart` policies que dependen del daemon. Podman no tiene daemon,
así que la persistencia se gestiona con systemd.

**Método moderno — Quadlet** (Podman 4.4+, recomendado):

```ini
# ~/.config/containers/systemd/web.container
[Container]
ContainerName=web
Image=docker.io/library/nginx:latest
PublishPort=8080:80

[Service]
Restart=always

[Install]
WantedBy=default.target
```

```bash
systemctl --user daemon-reload
systemctl --user enable --now web
loginctl enable-linger $(whoami)
```

**Método legacy — `podman generate systemd`** (deprecado desde Podman 4.4):

```bash
podman run -d --name web docker.io/library/nginx
podman generate systemd --new --name web > ~/.config/systemd/user/container-web.service
systemctl --user enable --now container-web.service
loginctl enable-linger $(whoami)
```

> **Nota**: `podman generate systemd` está deprecado. Para nuevos deployments,
> usar Quadlet (archivos `.container` en `~/.config/containers/systemd/`).

Para Compose files:

```yaml
# Docker Compose
services:
  web:
    restart: unless-stopped

# Con Podman: restart funciona en podman-compose pero sin daemon,
# la persistencia real requiere systemd (Quadlet) para producción
```

### Docker socket en herramientas externas

Herramientas que conectan al socket de Docker (Portainer, Traefik, CI/CD runners)
necesitan redirigirse al socket de Podman:

```bash
# Activar socket API compatible
systemctl --user enable --now podman.socket

# Redirigir DOCKER_HOST
export DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock
```

> **Nota sobre el symlink**: Crear un symlink de `/var/run/docker.sock` al
> socket rootless de Podman funciona para el usuario que lo crea, pero otros
> usuarios y servicios del sistema no pueden acceder a un socket per-user.
> Para servicios del sistema, considerar rootful Podman
> (`/run/podman/podman.sock`).

### Docker Compose → podman-compose o socket

```bash
# Opción 1: podman-compose
podman-compose up -d

# Opción 2: Docker Compose via socket
export DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock
docker compose up -d

# Opción 3: podman compose (Podman 4.7+)
podman compose up -d
```

### Networking rootless

La red bridge de Podman rootless usa **slirp4netns** o **pasta** en lugar de
bridges nativos. Esto puede ser más lento y tiene limitaciones:

```bash
# Docker: bridge nativo con iptables
# Podman rootless: slirp4netns/pasta (userspace networking)
podman info --format '{{.Host.NetworkBackend}}'
```

Para la mayoría de aplicaciones, la diferencia es imperceptible. Para
aplicaciones de alto throughput de red, considerar rootful.

## Lo que NO se puede migrar directamente

### Imágenes y volúmenes locales

Docker y Podman usan **directorios y formatos de storage diferentes**. No se
pueden compartir imágenes ni volúmenes entre ellos:

```bash
# Docker almacena en /var/lib/docker
# Podman almacena en ~/.local/share/containers (rootless)
#                  o /var/lib/containers (rootful)
```

Para migrar una imagen específica sin re-descargar:

```bash
# Exportar desde Docker
docker save myapp:v1 -o myapp.tar

# Importar en Podman
podman load -i myapp.tar
```

### Volúmenes con datos

```bash
# Exportar datos de un volumen de Docker
docker run --rm -v mydata:/data -v $(pwd):/backup alpine \
  tar czf /backup/mydata.tar.gz /data

# Crear volumen en Podman e importar
podman volume create mydata
podman run --rm -v mydata:/data -v $(pwd):/backup:z docker.io/library/alpine \
  tar xzf /backup/mydata.tar.gz -C /
```

### Docker Swarm

Docker Swarm no tiene equivalente en Podman. Las alternativas son:
- **Kubernetes** (con `podman generate kube` / `podman play kube` como puente)
- **systemd** para servicios en un solo nodo

## Tabla de compatibilidad de features

| Feature | Docker | Podman | Ajuste necesario |
|---|---|---|---|
| Dockerfiles estándar | Sí | Sí | Ninguno |
| BuildKit cache/secret/ssh mounts | Sí | Sí (Buildah 1.24+) | Ninguno |
| `# syntax=` directive | Sí | No | Eliminar directiva |
| Imágenes OCI | Sí | Sí | Ninguno |
| `docker run` básico | Sí | Sí | Nombre del comando |
| Compose files simples | Sí | Sí | Nombres de imagen completos |
| Bind mounts | Sí | Sí | `:z`/`:Z` con SELinux |
| Named volumes | Sí | Sí | Ninguno |
| Secrets | Sí | Sí (Podman 3.x+) | `podman secret create` |
| Redes bridge | Sí | Sí | Ligeras diferencias en rootless |
| Puertos <1024 rootless | Sí | No | Puerto >1024 o sysctl |
| Restart policies | Sí | Limitado | Quadlet para producción |
| Healthchecks | Sí | Sí | Igual |
| Docker Swarm | Sí | No | Kubernetes o systemd |

## Checklist de migración

| Paso | Acción | Prioridad |
|---|---|---|
| 1 | Instalar Podman | Obligatorio |
| 2 | Nombres de imagen completos en Dockerfiles y Compose | Obligatorio |
| 3 | Añadir `:z` a bind mounts (si SELinux) | Si aplica |
| 4 | Configurar registries.conf (TOML v2) | Recomendado |
| 5 | Reemplazar `--restart` con Quadlet | Para producción |
| 6 | Activar socket API para herramientas externas | Si aplica |
| 7 | Migrar imágenes locales (`docker save` → `podman load`) | Si necesario |
| 8 | Migrar volúmenes con datos (export/import) | Si necesario |
| 9 | Configurar `loginctl enable-linger` para rootless | Para producción |
| 10 | Ajustar puertos < 1024 o usar rootful | Si aplica |

---

## Ejercicios

### Ejercicio 1 — Migrar una imagen entre Docker y Podman

```bash
# Descargar con Docker
docker pull alpine:latest

# Exportar
docker save alpine:latest -o /tmp/alpine.tar
```

**Predicción**: ¿Qué formato tiene el archivo `/tmp/alpine.tar`? ¿Funciona
directamente con `podman load`?

<details><summary>Respuesta</summary>

El archivo es un tarball que contiene las capas de la imagen en formato OCI (o
Docker v2). `podman load` lo entiende directamente porque ambos usan formatos
compatibles. Tras importar, `podman images` muestra la imagen con el mismo tag.

</details>

```bash
# Importar en Podman
podman load -i /tmp/alpine.tar

# Verificar
podman images | grep alpine

# Limpiar
rm /tmp/alpine.tar
podman rmi docker.io/library/alpine:latest
```

### Ejercicio 2 — Adaptar un Compose file para Podman

```bash
mkdir -p /tmp/migrate_test && cd /tmp/migrate_test

# Compose file original (estilo Docker)
cat > compose.yml << 'EOF'
services:
  web:
    image: nginx:latest
    ports:
      - "8080:80"
    volumes:
      - ./html:/usr/share/nginx/html
    restart: unless-stopped
EOF

mkdir -p html && echo "<h1>Migrated</h1>" > html/index.html
```

**Predicción**: ¿Qué cambios se necesitan para que funcione con
`podman-compose`?

<details><summary>Respuesta</summary>

Dos cambios principales:
1. **Nombre de imagen completo**: `nginx:latest` → `docker.io/library/nginx:latest`
   (si no tienes `unqualified-search-registries` configurado, Podman preguntará
   de qué registry descargar)
2. **Label SELinux**: `./html:/usr/share/nginx/html` → `./html:/usr/share/nginx/html:z`
   (necesario si SELinux está en modo enforcing)

`restart: unless-stopped` funciona con podman-compose, pero sin daemon, la
persistencia real ante reboot requiere systemd/Quadlet.

</details>

```bash
# Versión adaptada
cat > compose.podman.yml << 'EOF'
services:
  web:
    image: docker.io/library/nginx:latest
    ports:
      - "8080:80"
    volumes:
      - ./html:/usr/share/nginx/html:z
    restart: unless-stopped
EOF

# Probar
podman-compose -f compose.podman.yml up -d
curl -s http://localhost:8080
podman-compose -f compose.podman.yml down

cd / && rm -rf /tmp/migrate_test
```

### Ejercicio 3 — Puertos privilegiados en rootless

```bash
# Ver el límite actual
cat /proc/sys/net/ipv4/ip_unprivileged_port_start
```

**Predicción**: ¿Qué pasa al intentar el puerto 80 en rootless?

```bash
podman run -d --name port_test -p 80:80 docker.io/library/nginx 2>&1
```

<details><summary>Respuesta</summary>

Si `ip_unprivileged_port_start` es mayor que 80 (por defecto es 1024), falla
con `Error: permission denied` o `Error: rootlessport cannot expose privileged port`.
Podman rootless no puede hacer bind a puertos por debajo de ese valor sin
ajustar el kernel.

</details>

```bash
# Puerto no privilegiado: funciona
podman rm -f port_test 2>/dev/null
podman run -d --name port_test -p 8080:80 docker.io/library/nginx
curl -s http://localhost:8080 | head -1

# Limpiar
podman rm -f port_test
```

### Ejercicio 4 — Migrar datos de volumen

```bash
# Crear un volumen en Podman y añadir datos
podman volume create source_vol
podman run --rm -v source_vol:/data docker.io/library/alpine \
  sh -c 'echo "important data" > /data/info.txt && date > /data/timestamp.txt'

# Verificar contenido original
podman run --rm -v source_vol:/data docker.io/library/alpine cat /data/info.txt
```

**Predicción**: ¿Se puede exportar este volumen como tarball e importarlo en
otro volumen?

<details><summary>Respuesta</summary>

Sí. El patrón es crear un contenedor temporal que monte tanto el volumen de
origen como un bind mount al host, y usar `tar` para copiar los datos:

```bash
podman run --rm -v source_vol:/data -v /tmp:/backup docker.io/library/alpine \
  tar czf /backup/vol_backup.tar.gz -C /data .
```

Luego se crea un volumen nuevo y se restaura desde el tarball. Este mismo
patrón funciona para migrar volúmenes entre Docker y Podman.

</details>

```bash
# Exportar
podman run --rm -v source_vol:/data -v /tmp:/backup docker.io/library/alpine \
  tar czf /backup/vol_backup.tar.gz -C /data .

# Crear volumen destino e importar
podman volume create dest_vol
podman run --rm -v dest_vol:/data -v /tmp:/backup:z docker.io/library/alpine \
  tar xzf /backup/vol_backup.tar.gz -C /data

# Verificar que los datos se copiaron
podman run --rm -v dest_vol:/data docker.io/library/alpine cat /data/info.txt

# Limpiar
podman volume rm source_vol dest_vol
rm /tmp/vol_backup.tar.gz
```

### Ejercicio 5 — Socket API para herramientas externas

```bash
# Activar el socket de Podman
systemctl --user enable --now podman.socket

# Verificar que el socket existe
ls -la /run/user/$(id -u)/podman/podman.sock
```

**Predicción**: ¿Qué devuelve una petición HTTP al socket?

```bash
curl -s --unix-socket /run/user/$(id -u)/podman/podman.sock \
  http://localhost/v4.0.0/libpod/info | head -c 200
```

<details><summary>Respuesta</summary>

Devuelve un JSON con información del sistema Podman (versión, storage, host
info, etc.). Esto demuestra que el socket habla el API REST — las herramientas
que usan el Docker socket (`/var/run/docker.sock`) pueden redirigirse a este
socket con `DOCKER_HOST`. La API de Podman es compatible con la de Docker en la
ruta `/v1.x/` y tiene su propia API extendida en `/v4.x.x/libpod/`.

</details>

```bash
# Probar compatibilidad con Docker API
curl -s --unix-socket /run/user/$(id -u)/podman/podman.sock \
  http://localhost/v1.40/info | head -c 200
```

### Ejercicio 6 — Persistencia con Quadlet (método moderno)

```bash
# Crear directorio de Quadlet
mkdir -p ~/.config/containers/systemd
```

**Predicción**: ¿Qué pasa al crear un archivo `.container` y hacer
`daemon-reload`?

<details><summary>Respuesta</summary>

systemd procesa los archivos `.container` en `~/.config/containers/systemd/`
como generadores de unidades. `daemon-reload` convierte cada `.container` en
una unidad systemd real que gestiona el ciclo de vida del contenedor. No
necesitas ejecutar `podman generate systemd` — Quadlet lo hace automáticamente.

</details>

```bash
cat > ~/.config/containers/systemd/migrate-test.container << 'EOF'
[Container]
ContainerName=migrate-test
Image=docker.io/library/nginx:latest
PublishPort=8080:80

[Service]
Restart=always

[Install]
WantedBy=default.target
EOF

systemctl --user daemon-reload
systemctl --user start migrate-test
systemctl --user status migrate-test

# Verificar que funciona
curl -s http://localhost:8080 | head -1

# Limpiar
systemctl --user stop migrate-test
rm ~/.config/containers/systemd/migrate-test.container
systemctl --user daemon-reload
```

### Ejercicio 7 — Comparar DNS entre Docker y Podman

```bash
mkdir -p /tmp/dns_compare && cd /tmp/dns_compare

cat > compose.yml << 'EOF'
services:
  server:
    image: docker.io/library/alpine:latest
    command: sleep 3600
  client:
    image: docker.io/library/alpine:latest
    command: sleep 3600
    depends_on:
      - server
EOF

podman-compose up -d
```

**Predicción**: ¿Puede `client` resolver `server` por nombre DNS?

```bash
podman-compose exec client ping -c 1 server 2>&1
```

<details><summary>Respuesta</summary>

Si podman-compose crea una red bridge (modo por defecto clásico), sí — los
contenedores tienen DNS embebido que resuelve nombres de servicio, igual que
Docker Compose. Si usa modo pod, no — los contenedores comparten network
namespace y se comunican por `localhost`.

El DNS funciona porque Podman (con netavark/aardvark-dns) implementa DNS
embebido en redes bridge custom, igual que Docker.

</details>

```bash
# Limpiar
podman-compose down
cd / && rm -rf /tmp/dns_compare
```

### Ejercicio 8 — Verificar compatibilidad de un Dockerfile

```bash
mkdir -p /tmp/dockerfile_test && cd /tmp/dockerfile_test

cat > Dockerfile << 'EOF'
FROM docker.io/library/alpine:latest
RUN apk add --no-cache curl
HEALTHCHECK --interval=30s CMD curl -f http://localhost/ || exit 1
COPY --chmod=755 <<-"SCRIPT" /entrypoint.sh
#!/bin/sh
echo "Running in $(cat /etc/os-release | grep PRETTY_NAME)"
exec sleep 3600
SCRIPT
ENTRYPOINT ["/entrypoint.sh"]
EOF
```

**Predicción**: ¿Funciona este Dockerfile con `podman build` sin cambios?

<details><summary>Respuesta</summary>

Sí. Todos los elementos usados son estándar:
- `FROM`, `RUN`, `HEALTHCHECK`, `COPY`, `ENTRYPOINT` — todos soportados
- `COPY --chmod` — soportado en Buildah
- Heredoc en `COPY` — soportado en versiones recientes de Buildah

Los Dockerfiles estándar son 100% compatibles. Solo fallarían las directivas
exclusivas de BuildKit como `# syntax=` o features avanzados de inline cache.

</details>

```bash
podman build -t compat-test .
podman run --rm compat-test

# Limpiar
podman rmi compat-test
cd / && rm -rf /tmp/dockerfile_test
```

---

## Resumen: niveles de migración

```
┌──────────────────────────────────────────────────────────────────┐
│                    MIGRACIÓN DOCKER → PODMAN                     │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Nivel 1: SIN CAMBIOS                                           │
│  • Dockerfiles estándar                                          │
│  • Imágenes OCI                                                  │
│  • Comandos CLI básicos (docker → podman)                        │
│  • Healthchecks, secrets, cache mounts (Buildah 1.24+)           │
│                                                                  │
│  Nivel 2: AJUSTES MENORES                                       │
│  • Nombres de imagen completos (docker.io/library/...)           │
│  • :z/:Z en bind mounts (si SELinux)                             │
│  • Puertos >1024 para rootless (o sysctl)                        │
│  • registries.conf (TOML v2)                                     │
│                                                                  │
│  Nivel 3: CAMBIOS SIGNIFICATIVOS                                │
│  • --restart → Quadlet (.container files)                        │
│  • Docker socket → podman.socket + DOCKER_HOST                   │
│  • Docker Compose → podman-compose / socket / podman compose     │
│  • Swarm → Kubernetes (sin equivalente directo)                  │
│                                                                  │
│  NO MIGRABLE DIRECTAMENTE                                       │
│  • Storage local (reimportar con save/load)                      │
│  • Volúmenes con datos (export/import con tar)                   │
│  • Docker Swarm orquestación multi-nodo                          │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```
