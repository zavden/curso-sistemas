# T01 — podman-compose

## El problema: Compose con Podman

Docker Compose es la herramienta estándar para definir aplicaciones
multi-contenedor, pero depende del daemon de Docker. Para usar archivos
`docker-compose.yml` con Podman, existen dos alternativas:

1. **podman-compose**: herramienta third-party que traduce Compose a comandos Podman
2. **Socket API**: exponer un socket compatible para que Docker Compose nativo
   hable con Podman

## podman-compose

### Instalación

```bash
# Con pip
pip install podman-compose

# En Fedora/RHEL
sudo dnf install podman-compose

# En Debian/Ubuntu
sudo apt-get install podman-compose

# Verificar
podman-compose --version
```

### Uso básico

`podman-compose` lee los mismos archivos `docker-compose.yml` y ejecuta los
comandos equivalentes con Podman:

```yaml
# compose.yml — el mismo archivo funciona con Docker y Podman
services:
  web:
    image: docker.io/library/nginx:latest
    ports:
      - "8080:80"
    volumes:
      - web-data:/usr/share/nginx/html

  db:
    image: docker.io/library/postgres:16-alpine
    environment:
      POSTGRES_PASSWORD: secret
    volumes:
      - pgdata:/var/lib/postgresql/data

volumes:
  web-data:
  pgdata:
```

```bash
# Levantar (equivalente a docker compose up)
podman-compose up -d

# Ver estado
podman-compose ps

# Logs
podman-compose logs -f

# Detener y eliminar
podman-compose down
```

### Comandos soportados

| Comando | Soporte |
|---|---|
| `up` / `down` | Sí |
| `start` / `stop` / `restart` | Sí |
| `ps` | Sí |
| `logs` | Sí |
| `exec` | Sí |
| `run` | Sí |
| `build` | Sí |
| `pull` | Sí |
| `config` | Parcial |

### Cómo funciona internamente

`podman-compose` no es un daemon ni un plugin — es un script Python que:

1. Parsea el archivo YAML
2. Traduce cada servicio a comandos `podman run`, `podman volume create`,
   `podman network create`
3. Ejecuta los comandos secuencialmente

```bash
# Ver qué comandos ejecutaría (dry run)
podman-compose --dry-run up
# podman network create compose_default
# podman run -d --name compose_web_1 --network compose_default ...
# podman run -d --name compose_db_1 --network compose_default ...
```

### Limitaciones

| Feature | Docker Compose | podman-compose |
|---|---|---|
| `depends_on: condition: service_healthy` | Sí | Parcial/no siempre |
| Profiles | Sí | No |
| Extensions (`x-`) | Sí | Parcial |
| Build con BuildKit | Sí | No (usa Buildah) |
| `docker compose config` | Completo | Parcial |
| Detección de cambios en `up` | Sí | Limitada |
| Watch/hot reload | Sí | No |
| Velocidad de desarrollo | Rápida | Más lenta |

Para archivos Compose simples (la mayoría de casos de desarrollo), podman-compose
funciona correctamente. Los problemas aparecen con features avanzados.

## Alternativa: Docker Compose via socket API

La segunda opción es usar **Docker Compose v2 nativo** con Podman como backend,
a través del socket API compatible:

```bash
# 1. Activar el socket de Podman
systemctl --user enable --now podman.socket

# 2. Verificar que el socket existe
ls -la /run/user/$(id -u)/podman/podman.sock

# 3. Configurar Docker Compose para usar el socket de Podman
export DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock

# 4. Usar Docker Compose normalmente
docker compose up -d
docker compose ps
docker compose logs
docker compose down
```

### Ventajas sobre podman-compose

| Aspecto | podman-compose | Docker Compose + socket |
|---|---|---|
| Compatibilidad de features | Parcial | Completa |
| Detección de cambios | Limitada | Nativa |
| `depends_on` con healthcheck | Parcial | Completo |
| Profiles, extensions | No | Sí |
| Velocidad de desarrollo | Lenta | Rápida (equipo de Docker) |
| Instalación | `pip install` | Requiere Docker Compose CLI |

### Configuración persistente

Para no exportar `DOCKER_HOST` en cada terminal:

```bash
# En ~/.bashrc o ~/.zshrc
export DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock
```

O en `~/.config/containers/containers.conf`:

```ini
[engine]
service_destinations = ["unix:///run/user/1000/podman/podman.sock"]
```

### Limitaciones del socket

El socket API de Podman es **compatible pero no idéntico** al de Docker. Algunas
operaciones pueden comportarse ligeramente diferente:

- Build usa Buildah internamente (sin cache mounts de BuildKit)
- Networking rootless usa slirp4netns/pasta
- Algunos eventos y callbacks pueden diferir

## Nombres de imágenes en Compose con Podman

Con Podman, las imágenes en el Compose file deben usar **nombres completos**
para evitar la pregunta interactiva de selección de registry:

```yaml
# MAL: Podman puede preguntar de qué registry
services:
  web:
    image: nginx

# BIEN: nombre completo
services:
  web:
    image: docker.io/library/nginx:latest
```

Si configuras `unqualified-search-registries = ["docker.io"]` en
`~/.config/containers/registries.conf`, los nombres cortos funcionan sin
preguntar.

## Pods en podman-compose

Por defecto, `podman-compose` crea una **red** para los servicios (como Docker
Compose). Pero puede configurarse para crear **pods**:

```bash
# Modo por defecto: red (como Docker Compose)
podman-compose up -d

# Modo pods: todos los servicios en un pod
podman-compose --podman-run-args="--pod" up -d
```

En modo pod, los servicios se comunican por `localhost` en lugar de por nombre
de servicio. Esto puede romper aplicaciones que esperan resolver `db` o `redis`
por DNS.

---

## Ejercicios

### Ejercicio 1 — podman-compose básico

```bash
mkdir -p /tmp/podman_compose && cd /tmp/podman_compose

cat > compose.yml << 'EOF'
services:
  web:
    image: docker.io/library/nginx:latest
    ports:
      - "8080:80"
  app:
    image: docker.io/library/alpine:latest
    command: sleep 3600
EOF

# Levantar con podman-compose
podman-compose up -d

# Verificar
podman-compose ps
curl -s http://localhost:8080 | head -3

# Logs
podman-compose logs --tail 5

# Limpiar
podman-compose down
cd / && rm -rf /tmp/podman_compose
```

### Ejercicio 2 — Docker Compose via socket API

```bash
mkdir -p /tmp/socket_compose && cd /tmp/socket_compose

cat > compose.yml << 'EOF'
services:
  web:
    image: docker.io/library/nginx:latest
    ports:
      - "8080:80"
EOF

# Activar socket
systemctl --user start podman.socket

# Usar Docker Compose con el socket de Podman
DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock docker compose up -d

# Verificar
DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock docker compose ps
curl -s http://localhost:8080 | head -3

# Limpiar
DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock docker compose down
cd / && rm -rf /tmp/socket_compose
```

### Ejercicio 3 — Comparar podman-compose vs Docker Compose

```bash
mkdir -p /tmp/compose_compare && cd /tmp/compose_compare

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

echo "=== Con podman-compose ==="
podman-compose up -d
podman-compose ps
podman-compose exec client ping -c 1 server 2>&1 || echo "DNS no resolvió"
podman-compose down

echo ""
echo "=== Con Docker Compose ==="
docker compose up -d 2>/dev/null
docker compose ps 2>/dev/null
docker compose exec client ping -c 1 server 2>&1 || echo "DNS no resolvió"
docker compose down 2>/dev/null

cd / && rm -rf /tmp/compose_compare
```
