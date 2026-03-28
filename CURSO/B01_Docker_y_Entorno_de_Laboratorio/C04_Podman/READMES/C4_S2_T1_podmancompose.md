# T01 — podman-compose

## El problema: Compose con Podman

Docker Compose es la herramienta estándar para definir aplicaciones
multi-contenedor, pero depende del daemon de Docker. Para usar archivos
`compose.yml` con Podman, existen tres alternativas:

1. **podman-compose**: herramienta third-party (Python) que traduce Compose a
   comandos Podman
2. **Docker Compose via socket API**: Docker Compose v2 nativo usando el socket
   compatible de Podman como backend
3. **`podman compose`** (sin guión, Podman 4.7+): subcomando integrado en Podman
   que delega a un proveedor de compose externo

## Opción 1: podman-compose

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

`podman-compose` lee los mismos archivos `compose.yml` y ejecuta los comandos
equivalentes con Podman:

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
podman-compose logs --tail 20

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
| `build` | Sí (usa Buildah) |
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

### Limitaciones de podman-compose

| Feature | Docker Compose | podman-compose |
|---|---|---|
| `depends_on: condition: service_healthy` | Sí | Parcial |
| Profiles | Sí | No |
| Extensions (`x-`) | Sí | Parcial |
| Build con BuildKit | Sí | No (usa Buildah) |
| `docker compose config` | Completo | Parcial |
| Detección de cambios en `up` | Sí | Limitada |
| Watch/hot reload | Sí | No |

Para archivos Compose simples (la mayoría de casos de desarrollo), podman-compose
funciona correctamente. Los problemas aparecen con features avanzados.

## Opción 2: Docker Compose via socket API

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

> **Requisito**: Docker Compose v2 debe estar instalado en el sistema. Es un
> binario independiente (`docker-compose-plugin` o standalone). No necesitas el
> daemon de Docker — solo el CLI de compose.

### Configuración persistente

Para no exportar `DOCKER_HOST` en cada terminal:

```bash
# En ~/.bashrc o ~/.zshrc
export DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock
```

Alternativa con Docker contexts (no requiere variable de entorno):

```bash
# Crear un contexto que apunta al socket de Podman
docker context create podman \
  --docker "host=unix:///run/user/$(id -u)/podman/podman.sock"

# Activar el contexto
docker context use podman

# Ahora docker compose usa Podman automáticamente
docker compose up -d
```

### Ventajas sobre podman-compose

| Aspecto | podman-compose | Docker Compose + socket |
|---|---|---|
| Compatibilidad de features | Parcial | Casi completa |
| Detección de cambios | Limitada | Nativa |
| `depends_on` con healthcheck | Parcial | Completo |
| Profiles, extensions | No | Sí |
| Velocidad de desarrollo | Lenta | Rápida (equipo de Docker) |
| Instalación | `pip install` | Requiere Docker Compose CLI |

### Limitaciones del socket

El socket API de Podman es **compatible pero no idéntico** al de Docker:

- Build usa Buildah internamente. Desde Buildah 1.24+, las cache mounts
  (`--mount=type=cache`), secret mounts y SSH mounts **sí** funcionan. Sin
  embargo, features exclusivos de BuildKit como inline cache o `cache-from`/
  `cache-to` con registries no están disponibles
- Networking rootless usa slirp4netns o pasta (en lugar de Docker's userland
  proxy)
- Algunos eventos y callbacks pueden diferir ligeramente

## Opción 3: `podman compose` (Podman 4.7+)

Desde Podman 4.7, existe el subcomando integrado `podman compose` (sin guión).
Este comando **no implementa compose** — delega a un proveedor externo:

```bash
# Verifica qué proveedor usa
podman compose version

# Uso (idéntico a docker compose / podman-compose)
podman compose up -d
podman compose ps
podman compose down
```

`podman compose` busca automáticamente un proveedor en este orden:
1. `docker-compose` (si está instalado)
2. `podman-compose` (si está instalado)

La ventaja es que proporciona un punto de entrada unificado — el usuario escribe
`podman compose` sin preocuparse por qué herramienta está detrás.

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

Si configuras `unqualified-search-registries` en
`~/.config/containers/registries.conf`, los nombres cortos funcionan sin
preguntar:

```toml
# ~/.config/containers/registries.conf (formato TOML v2)
unqualified-search-registries = ["docker.io"]
```

> **Nota**: El formato v1 (`[registries.search]` + `registries = [...]`) está
> deprecado. Usar siempre el formato TOML v2 mostrado arriba.

## Pods en podman-compose

Por defecto, `podman-compose` crea una **red** para los servicios (como Docker
Compose). En algunas versiones recientes del proyecto containers/podman-compose,
el comportamiento puede variar — algunas versiones crean un pod por defecto.

En modo pod, los servicios se comunican por `localhost` en lugar de por nombre
de servicio. Esto puede romper aplicaciones que esperan resolver `db` o `redis`
por DNS:

```
Modo red (default clásico):
  web → http://db:5432         (resolución DNS por nombre de servicio)

Modo pod:
  web → http://localhost:5432  (mismo network namespace)
```

> **Nota**: Si tu compose.yml usa nombres de servicio para comunicación
> inter-servicio (lo más común), el modo red es más compatible.

## Comparación completa de opciones

| Aspecto | podman-compose | Docker Compose + socket | `podman compose` |
|---|---|---|---|
| Backend | Podman CLI | Podman socket | Delegado (varía) |
| Features completas | Parcial | Casi completas | Depende del proveedor |
| `depends_on` healthcheck | Parcial | Sí | Depende del proveedor |
| Profiles | No | Sí | Depende del proveedor |
| Cache mounts (Buildah 1.24+) | Sí | Sí | Depende del proveedor |
| Instalación extra | Sí (pip/dnf) | Sí (Docker Compose CLI) | No (integrado) |
| Rootless nativo | Sí | Sí | Sí |

## Recomendación

1. **Para desarrollo general**: Usa `docker compose` con el socket de Podman
   (`DOCKER_HOST=unix://...`). Es la opción más compatible con features
   avanzados.

2. **Para simplificar**: Usa `podman compose` (4.7+) que delega automáticamente
   al proveedor disponible.

3. **Para CI/CD sin Docker**: Usa `podman-compose`. La mayoría de archivos
   Compose simples funcionan.

4. **Para producción**: Considera si realmente necesitas Compose o si
   systemd + Quadlet (o Kubernetes) es más apropiado.

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
```

**Predicción**: ¿Qué muestra `podman-compose ps`?

<details><summary>Respuesta</summary>

Muestra dos contenedores: `web` y `app`, ambos con estado `Up`. Los nombres
incluyen el prefijo del proyecto (nombre del directorio) y un sufijo numérico,
por ejemplo `podman_compose_web_1` y `podman_compose_app_1`.

</details>

```bash
podman-compose ps
curl -s http://localhost:8080 | head -3

# Ver logs
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
```

**Predicción**: ¿Qué hace el siguiente comando? ¿Con qué runtime se ejecuta
el contenedor?

```bash
DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock docker compose up -d
```

<details><summary>Respuesta</summary>

Docker Compose lee el `compose.yml` y envía las instrucciones al socket de
Podman (no al daemon de Docker). El contenedor se ejecuta con **Podman** como
runtime (puedes verificar con `podman ps`), pero Docker Compose cree que está
hablando con Docker. El socket de Podman traduce las peticiones API.

</details>

```bash
# Verificar con ambas herramientas
DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock docker compose ps
podman ps
curl -s http://localhost:8080 | head -3

# Limpiar
DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock docker compose down
cd / && rm -rf /tmp/socket_compose
```

### Ejercicio 3 — Comparar DNS entre herramientas

```bash
mkdir -p /tmp/compose_dns && cd /tmp/compose_dns

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

**Predicción**: ¿Puede `client` resolver `server` por DNS?

```bash
podman-compose exec client ping -c 1 server
```

<details><summary>Respuesta</summary>

Depende del modo de podman-compose. En **modo red** (default clásico), sí —
los contenedores están en una red bridge con DNS embebido, igual que Docker
Compose. En **modo pod**, no — los contenedores comparten network namespace
y se comunican por `localhost`, sin DNS por nombre de servicio.

Si falla, podman-compose puede estar usando modo pod. Intenta con `localhost`:

```bash
podman-compose exec client ping -c 1 localhost
```

</details>

```bash
# Limpiar
podman-compose down
cd / && rm -rf /tmp/compose_dns
```

### Ejercicio 4 — Dry run para ver comandos internos

```bash
mkdir -p /tmp/dryrun && cd /tmp/dryrun

cat > compose.yml << 'EOF'
services:
  web:
    image: docker.io/library/nginx:latest
    ports:
      - "8080:80"
  db:
    image: docker.io/library/postgres:16-alpine
    environment:
      POSTGRES_PASSWORD: secret
EOF
```

**Predicción**: ¿Qué tipo de comandos genera podman-compose internamente?

```bash
podman-compose --dry-run up -d
```

<details><summary>Respuesta</summary>

Muestra los comandos `podman` que ejecutaría sin ejecutarlos realmente:
- `podman network create ...` (red para el proyecto)
- `podman run -d --name ... --network ... -p 8080:80 nginx` (servicio web)
- `podman run -d --name ... --network ... -e POSTGRES_PASSWORD=secret postgres` (servicio db)

Esto demuestra que podman-compose es solo un traductor de YAML a comandos
Podman — no hay daemon ni orquestación sofisticada.

</details>

```bash
# Limpiar
cd / && rm -rf /tmp/dryrun
```

### Ejercicio 5 — Verificar registries para short names

```bash
# Ver si existe configuración de registries
cat ~/.config/containers/registries.conf 2>/dev/null || echo "No existe"
```

**Predicción**: ¿Qué pasa al hacer `podman pull nginx` sin configuración de
`unqualified-search-registries`?

<details><summary>Respuesta</summary>

Podman muestra un menú interactivo preguntando de qué registry descargar la
imagen (docker.io, quay.io, etc.). Esto es un problema en scripts no
interactivos y en podman-compose, porque el pull se queda esperando input.

Con `unqualified-search-registries = ["docker.io"]` configurado, Podman
asume docker.io automáticamente.

</details>

```bash
# Configurar (si no existe)
mkdir -p ~/.config/containers

# IMPORTANTE: formato TOML v2 (el formato v1 con [registries.search] está deprecado)
# Solo ejecutar si el archivo no existe o no tiene esta línea:
grep -q 'unqualified-search-registries' ~/.config/containers/registries.conf 2>/dev/null || \
  echo 'unqualified-search-registries = ["docker.io"]' >> ~/.config/containers/registries.conf

# Verificar
cat ~/.config/containers/registries.conf
```

### Ejercicio 6 — Compose con volúmenes y healthcheck

```bash
mkdir -p /tmp/compose_health && cd /tmp/compose_health

cat > compose.yml << 'EOF'
services:
  db:
    image: docker.io/library/postgres:16-alpine
    environment:
      POSTGRES_PASSWORD: secret
    volumes:
      - pgdata:/var/lib/postgresql/data
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 5s
      timeout: 3s
      retries: 5
  app:
    image: docker.io/library/alpine:latest
    command: sleep 3600
    depends_on:
      db:
        condition: service_healthy
volumes:
  pgdata:
EOF
```

**Predicción**: ¿Se levanta `app` inmediatamente con `podman-compose up -d`?

<details><summary>Respuesta</summary>

Depende de la versión de podman-compose. El `depends_on` con
`condition: service_healthy` tiene soporte **parcial** en podman-compose — en
algunas versiones se ignora la condición y `app` se levanta sin esperar a que
`db` esté healthy. Con Docker Compose (via socket), sí espera correctamente.

Este es uno de los casos donde Docker Compose + socket es más confiable.

</details>

```bash
podman-compose up -d

# Ver estado del healthcheck
podman ps --format "{{.Names}}\t{{.Status}}"

# Limpiar
podman-compose down -v
cd / && rm -rf /tmp/compose_health
```

### Ejercicio 7 — Socket persistente con Docker context

```bash
# Asegurar que el socket está activo
systemctl --user enable --now podman.socket

# Verificar
ls -la /run/user/$(id -u)/podman/podman.sock
```

**Predicción**: ¿Cuál es la diferencia entre configurar `DOCKER_HOST` y crear
un Docker context?

<details><summary>Respuesta</summary>

- `DOCKER_HOST` es una variable de entorno que afecta a la sesión actual (o
  permanente si se añade a `.bashrc`). Sobreescribe cualquier otra configuración.
- Docker context es una configuración persistente del CLI de Docker que se
  almacena en `~/.docker/contexts/`. Permite cambiar entre backends
  fácilmente con `docker context use`.

La ventaja de contexts es que puedes tener múltiples backends configurados
(Docker local, Podman, remoto) y cambiar con un comando sin modificar variables
de entorno.

</details>

```bash
# Crear context para Podman (si Docker CLI está instalado)
docker context create podman \
  --docker "host=unix:///run/user/$(id -u)/podman/podman.sock" 2>/dev/null \
  && echo "Context creado" || echo "Docker CLI no disponible"

# Listar contexts
docker context ls 2>/dev/null

# Para usar: docker context use podman
# Para volver: docker context use default
```

### Ejercicio 8 — Mismo compose.yml con ambas herramientas

```bash
mkdir -p /tmp/compose_both && cd /tmp/compose_both

cat > compose.yml << 'EOF'
services:
  web:
    image: docker.io/library/nginx:latest
    ports:
      - "8080:80"
EOF

echo "=== Con podman-compose ==="
podman-compose up -d
podman ps --format "{{.Names}}\t{{.Image}}\t{{.Status}}"
curl -s http://localhost:8080 | head -1
podman-compose down
```

**Predicción**: ¿Los nombres de contenedor son iguales con ambas herramientas?

<details><summary>Respuesta</summary>

No necesariamente. podman-compose tiende a usar el formato
`<directorio>_<servicio>_<número>` (estilo Compose v1 con guiones bajos),
mientras que Docker Compose v2 usa `<directorio>-<servicio>-<número>` (con
guiones). El contenedor funciona igual, pero los nombres difieren.

Esto puede importar si tus scripts hacen referencia a contenedores por nombre.

</details>

```bash
echo ""
echo "=== Con Docker Compose + socket ==="
DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock docker compose up -d 2>/dev/null
podman ps --format "{{.Names}}\t{{.Image}}\t{{.Status}}"
curl -s http://localhost:8080 | head -1
DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock docker compose down 2>/dev/null

# Limpiar
cd / && rm -rf /tmp/compose_both
```

---

## Resumen de opciones

```
┌───────────────────────────────────────────────────────────────┐
│              OPCIONES PARA COMPOSE CON PODMAN                 │
├───────────────────────────────────────────────────────────────┤
│                                                               │
│  1. podman-compose (herramienta third-party)                  │
│     pip install podman-compose                                │
│     podman-compose up -d                                      │
│     └─→ Traduce YAML a comandos podman                        │
│                                                               │
│  2. Docker Compose + Socket API                               │
│     systemctl --user enable --now podman.socket               │
│     export DOCKER_HOST=unix:///run/user/$UID/podman/...       │
│     docker compose up -d                                      │
│     └─→ Usa Docker Compose nativo con Podman backend          │
│                                                               │
│  3. podman compose (Podman 4.7+, integrado)                   │
│     podman compose up -d                                      │
│     └─→ Delega a docker-compose o podman-compose              │
│                                                               │
└───────────────────────────────────────────────────────────────┘
```
