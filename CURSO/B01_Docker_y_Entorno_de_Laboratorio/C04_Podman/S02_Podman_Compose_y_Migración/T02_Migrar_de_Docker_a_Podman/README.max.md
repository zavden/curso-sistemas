# T02 — Migrar de Docker a Podman

## ¿Qué es compatible y qué no?

La migración de Docker a Podman es en su mayoría directa, pero hay diferencias
que requieren ajustes. Esta es la guía completa.

## Nivel 1: Compatible sin cambios

### Dockerfiles

Los Dockerfiles funcionan **sin ningún cambio**. Podman usa Buildah internamente,
que entiende la misma sintaxis:

```bash
# El mismo Dockerfile
podman build -t myapp -f Dockerfile .
podman build --build-arg VERSION=2.0 -t myapp .
```

Excepción: directivas específicas de BuildKit (`# syntax=`, `RUN --mount=type=cache`,
`RUN --mount=type=secret`) no son soportadas por Buildah.

### Imágenes

Las imágenes son 100% compatibles (formato OCI). La misma imagen funciona en
ambos:

```bash
# La misma imagen
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
configurar registries.conf.

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

### Puertos privilegiados (<1024) en rootless

```bash
# Docker rootful: funciona
docker run -p 80:80 nginx

# Podman rootless: falla
podman run -p 80:80 nginx
# Error: permission denied

# Soluciones:
# 1. Usar un puerto no privilegiado
podman run -p 8080:80 nginx

# 2. Configurar el kernel
sudo sysctl net.ipv4.ip_unprivileged_port_start=80

# 3. Usar rootful
sudo podman run -p 80:80 nginx
```

## Nivel 3: Requiere cambios significativos

### Restart policies → systemd

Docker usa `--restart` policies que dependen del daemon. Podman no tiene daemon,
así que la persistencia se gestiona con systemd:

```bash
# Docker
docker run -d --restart unless-stopped --name web nginx

# Podman — equivalente con systemd
podman run -d --name web nginx
podman generate systemd --new --name web > ~/.config/systemd/user/container-web.service
systemctl --user enable --now container-web.service
loginctl enable-linger $(whoami)
```

Para Compose files:

```yaml
# Docker Compose
services:
  web:
    restart: unless-stopped

# Con Podman: restart funciona en podman-compose pero sin daemon,
# la persistencia real requiere systemd o Quadlet
```

### Docker socket en herramientas externas

Herramientas que conectan al socket de Docker (Portainer, Traefik, CI/CD runners)
necesitan redirigirse al socket de Podman:

```bash
# Activar socket API compatible
systemctl --user enable --now podman.socket

# Redirigir DOCKER_HOST
export DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock

# O crear un symlink (requiere root)
sudo ln -sf /run/user/$(id -u)/podman/podman.sock /var/run/docker.sock
```

### Docker Compose → podman-compose o socket

```bash
# Opción 1: podman-compose
podman-compose up -d

# Opción 2: Docker Compose via socket
export DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock
docker compose up -d
```

### Networking rootless

La red bridge de Podman rootless usa **slirp4netns** o **pasta** en lugar de
bridges nativos. Esto puede ser más lento y tiene limitaciones:

```bash
# Docker: bridge nativo con iptables
# Podman rootless: slirp4netns (userspace networking)
podman info | grep -i network
# networkBackend: netavark (rootful) o slirp4netns/pasta (rootless)
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

# Las imágenes hay que descargarlas de nuevo
docker images   # ← imágenes de Docker
podman images   # ← imágenes de Podman (diferentes)
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

## Tabla de compatibilidad de features

| Feature | Docker | Podman | Ajuste necesario |
|---|---|---|---|
| Dockerfiles estándar | Sí | Sí | Ninguno |
| Imágenes OCI | Sí | Sí | Ninguno |
| `docker run` basico | Sí | Sí | Nombre del comando |
| `docker-compose.yml` simple | Sí | Sí (compose) | Nombres de imagen |
| Bind mounts | Sí | Sí | `:z`/`:Z` con SELinux |
| Named volumes | Sí | Sí | Ninguno |
| Redes bridge | Sí | Sí |少许差异 |
| Puertos <1024 rootless | Sí | No | Usar puerto >1024 o rootful |
| Restart policies | Sí | Limitado | systemd para producción |
| BuildKit cache mounts | Sí | No | Buildah cache |
| Docker Swarm | Sí | No | No migrable |
| Docker secrets | Sí | No | Docker secrets o alternativas |
| Healthchecks | Sí | Sí | Igual |

## Migración paso a paso

### Paso 1: Instalación básica

```bash
# Instalar Podman
sudo dnf install podman  # Fedora/RHEL
sudo apt-get install podman  # Debian/Ubuntu

# Verificar
podman --version
```

### Paso 2: Configurar registries

```bash
# ~/.config/containers/registries.conf
[registries.search]
registries = ["docker.io"]
```

### Paso 3: Probar imágenes existentes

```bash
# Probar que las imágenes funcionan
podman pull docker.io/library/nginx:latest
podman run -d --name test -p 8080:80 docker.io/library/nginx
curl http://localhost:8080
podman rm -f test
```

### Paso 4: Adaptar Dockerfiles

```dockerfile
# No se necesitan cambios para la mayoría de Dockerfiles
# Solo verificar que no usen features de BuildKit

# Features NO soportados:
# RUN --mount=type=cache
# RUN --mount=type=secret
# syntax=docker/dockerfile:1
```

### Paso 5: Adaptar Compose files

```yaml
# Changes needed:
# 1. image: docker.io/library/nginx (full name)
# 2. volumes: ./src:/app/src:z (add :z for SELinux)
# 3. restart: still works in podman-compose
```

### Paso 6: Configurar persistencia (producción)

```bash
# Habilitar linger para contenedores rootless persistentes
loginctl enable-linger $(whoami)

# Para restart automático, generar systemd units
podman run -d --name web nginx
podman generate systemd --new --name web > ~/.config/systemd/user/container-web.service
systemctl --user enable container-web.service
```

## Checklist de migración

| Paso | Acción | Prioridad |
|---|---|---|
| 1 | Instalar Podman | Obligatorio |
| 2 | Nombres de imagen completos en Dockerfiles y Compose | Obligatorio |
| 3 | Añadir `:z` a bind mounts (si SELinux) | Si aplica |
| 4 | Configurar registries.conf | Recomendado |
| 5 | Reemplazar `--restart` con systemd units | Para producción |
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

# Compose file original (Docker)
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

# Adaptar para Podman:
# 1. Nombre completo de imagen
# 2. :z en bind mounts (para SELinux)
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

# Probar con podman-compose
podman-compose -f compose.podman.yml up -d
curl -s http://localhost:8080
podman-compose -f compose.podman.yml down

cd / && rm -rf /tmp/migrate_test
```

### Ejercicio 3 — Comparar comportamiento de red

```bash
mkdir -p /tmp/net_compare && cd /tmp/net_compare

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

# Con Docker
echo "=== Docker ==="
docker compose up -d
docker compose exec client ping -c 1 server
docker compose down

# Con podman-compose
echo "=== Podman ==="
podman-compose up -d
podman-compose exec client ping -c 1 server 2>&1
podman-compose down

cd / && rm -rf /tmp/net_compare
```

### Ejercicio 4 — Puertos privilegiados

```bash
# Ver el rango de puertos privilegiados
cat /proc/sys/net/ipv4/ip_unprivileged_port_start

# Intentar puerto 80 (privilegiado) en rootless
podman run -d --name port_test -p 80:80 docker.io/library/alpine sleep 3600 2>&1

# Usar puerto no privilegiado funciona
podman rm -f port_test
podman run -d --name port_test -p 8080:80 docker.io/library/alpine sleep 3600
curl http://localhost:8080

podman rm -f port_test
```

### Ejercicio 5 — Migración de systemd units

```bash
# Crear un contenedor
podman run -d --name migrate_app -p 8080:80 docker.io/library/nginx

# Generar systemd unit
mkdir -p ~/.config/systemd/user
podman generate systemd --new --name migrate_app > ~/.config/systemd/user/container-migrate_app.service

# Ver el contenido
cat ~/.config/systemd/user/container-migrate_app.service

# Habilitar
systemctl --user daemon-reload
systemctl --user enable container-migrate_app.service

# Limpiar el contenedor original
podman stop migrate_app
podman rm migrate_app

# Arrancar via systemd
systemctl --user start container-migrate_app.service

# Verificar
curl http://localhost:8080

# Limpiar
systemctl --user stop container-migrate_app.service
systemctl --user disable container-migrate_app.service
rm ~/.config/systemd/user/container-migrate_app.service
```
