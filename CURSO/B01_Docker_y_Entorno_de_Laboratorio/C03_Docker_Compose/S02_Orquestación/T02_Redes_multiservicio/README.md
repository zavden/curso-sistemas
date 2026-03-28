# T02 — Redes multi-servicio

## Red default de Compose

Cuando ejecutas `docker compose up`, Compose crea automáticamente una **red bridge**
para el proyecto. Todos los servicios se conectan a esta red y pueden comunicarse
entre sí usando el **nombre del servicio** como hostname:

```yaml
# compose.yml
services:
  web:
    image: nginx:latest
  api:
    image: myapi:latest
  db:
    image: postgres:16
```

```
Red: proyecto_default
┌──────────────────────────────────────────┐
│                                          │
│  web ◀──────▶ api ◀──────▶ db           │
│                                          │
│  web puede resolver "api" y "db" por DNS │
│  api puede resolver "web" y "db" por DNS │
│  db puede resolver "web" y "api" por DNS │
│                                          │
└──────────────────────────────────────────┘
```

```bash
docker compose up -d

# Verificar la red creada
docker network ls | grep default
# abc123  proyecto_default  bridge  local

# Desde web, resolver api por nombre
docker compose exec web ping -c 1 api
# PING api (172.20.0.3): 56 data bytes — resuelve automáticamente
```

### El nombre DNS es el nombre del servicio

El hostname DNS es el nombre definido en el YAML (`web`, `api`, `db`), **no** el
nombre del contenedor (`proyecto-web-1`). Ambos funcionan, pero el nombre del
servicio es el canónico:

```bash
# Ambos resuelven al mismo contenedor
docker compose exec api ping -c 1 db          # Nombre de servicio ✓
docker compose exec api ping -c 1 proyecto-db-1  # Nombre de contenedor ✓
```

## Redes custom para aislamiento

La red default conecta **todos** los servicios entre sí. En una arquitectura
real, esto es un problema de seguridad: el proxy web no debería poder hablar
directamente con la base de datos.

La solución es definir **redes separadas**:

```yaml
services:
  proxy:
    image: nginx:latest
    ports:
      - "80:80"
    networks:
      - frontend

  api:
    image: myapi:latest
    networks:
      - frontend
      - backend

  db:
    image: postgres:16
    environment:
      POSTGRES_PASSWORD: secret
    networks:
      - backend

  cache:
    image: redis:7
    networks:
      - backend

networks:
  frontend:
  backend:
```

```
┌── frontend ──────────────────┐
│                              │
│  proxy ◀─────────▶ api ─────┼──┐
│                              │  │
│  proxy NO ve db ni cache     │  │
│                              │  │
└──────────────────────────────┘  │
                                  │
┌── backend ──────────────────────┤
│                                 │
│  api ◀─────────▶ db            │
│    ◀─────────▶ cache            │
│                                 │
│  db y cache NO ven proxy       │
│                                 │
└─────────────────────────────────┘
```

```bash
docker compose up -d

# proxy puede hablar con api (misma red frontend)
docker compose exec proxy ping -c 1 api    # ✓

# proxy NO puede hablar con db (redes diferentes)
docker compose exec proxy ping -c 1 db     # ✗ bad address 'db'

# api puede hablar con db y cache (está en backend)
docker compose exec api ping -c 1 db       # ✓
docker compose exec api ping -c 1 cache    # ✓
```

### Comportamiento importante

Cuando un servicio declara `networks:` explícitamente, **no se conecta a la red
default**. Si quieres que también esté en la default, hay que incluirla:

```yaml
services:
  web:
    networks:
      - frontend
      - default    # Hay que incluirla explícitamente

networks:
  frontend:
```

Si ningún servicio usa `networks:`, todos van a la red default y esta sección
no es necesaria.

## Configuración de redes

```yaml
networks:
  frontend:
    driver: bridge           # Default: bridge
    driver_opts:
      com.docker.network.bridge.name: frontend_br

  backend:
    ipam:                    # Configurar subredes
      config:
        - subnet: 10.0.1.0/24
          gateway: 10.0.1.1

  monitoring:
    internal: true           # Sin acceso a internet
```

### `internal: true`

Una red interna no tiene gateway al exterior. Los contenedores en esta red
pueden comunicarse entre sí pero **no pueden acceder a internet**:

```yaml
networks:
  db_net:
    internal: true

services:
  db:
    image: postgres:16
    networks:
      - db_net
    # db no puede hacer pull de paquetes, no puede hacer DNS externo
    # Solo se comunica con otros contenedores en db_net
```

Útil para bases de datos y servicios que no necesitan (ni deben tener)
conectividad externa.

## Redes externas

Para conectar servicios de Compose a redes creadas fuera del proyecto (por
otro Compose, manualmente, o por otro sistema):

```yaml
networks:
  shared:
    external: true        # No la crea, debe existir previamente
    name: infra_shared    # Nombre real de la red (si difiere de la clave)
```

```bash
# Crear la red externa primero
docker network create infra_shared

# Luego levantar el proyecto
docker compose up -d
```

### Comunicar dos proyectos Compose

```bash
# Proyecto A
# proyecto_a/compose.yml
```

```yaml
services:
  api:
    image: myapi:latest
    networks:
      - shared

networks:
  shared:
    name: shared_network
```

```bash
# Proyecto B
# proyecto_b/compose.yml
```

```yaml
services:
  frontend:
    image: myweb:latest
    networks:
      - shared

networks:
  shared:
    external: true
    name: shared_network
```

```bash
# Levantar proyecto A primero (crea shared_network)
cd proyecto_a && docker compose up -d

# Levantar proyecto B (usa shared_network existente)
cd proyecto_b && docker compose up -d

# frontend puede resolver "api"
docker compose exec frontend ping -c 1 api   # ✓
```

## Aliases de red

Un servicio puede tener **nombres DNS adicionales** en una red específica:

```yaml
services:
  postgres-primary:
    image: postgres:16
    networks:
      backend:
        aliases:
          - db
          - database
          - primary-db

networks:
  backend:
```

```bash
# Todos estos nombres resuelven al mismo contenedor
docker compose exec api ping -c 1 postgres-primary  # ✓
docker compose exec api ping -c 1 db                # ✓
docker compose exec api ping -c 1 database           # ✓
```

Los aliases son específicos por red. Un alias definido en `backend` no es
visible desde la red `frontend`.

## IPs estáticas

En casos especiales, se puede asignar una IP fija a un servicio:

```yaml
services:
  dns:
    image: coredns/coredns
    networks:
      infra:
        ipv4_address: 10.0.0.53

networks:
  infra:
    ipam:
      config:
        - subnet: 10.0.0.0/24
```

En general, **no uses IPs estáticas**. Usa nombres de servicio y DNS. Las IPs
estáticas son frágiles y difíciles de mantener. Solo se justifican para
servicios de infraestructura como DNS servers.

## Inspeccionar redes de Compose

```bash
# Ver qué redes creó el proyecto
docker compose ps --format "{{.Name}}\t{{.Networks}}"

# Inspeccionar una red
docker network inspect proyecto_backend

# Ver contenedores en una red específica
docker network inspect proyecto_backend \
  --format '{{range .Containers}}{{.Name}}: {{.IPv4Address}}{{"\n"}}{{end}}'
```

## Prefijo del proyecto en redes

Compose prefija las redes con el nombre del proyecto:

```yaml
# compose.yml en directorio "myapp"
networks:
  backend:
```

```bash
docker compose up -d
docker network ls | grep backend
# abc123  myapp_backend  bridge  local
```

Para usar un nombre fijo (sin prefijo):

```yaml
networks:
  backend:
    name: my_custom_backend_name
```

---

## Ejercicios

### Ejercicio 1 — Aislamiento con redes separadas

```bash
mkdir -p /tmp/compose_nets && cd /tmp/compose_nets

cat > compose.yml << 'EOF'
services:
  proxy:
    image: alpine:latest
    command: sleep 3600
    networks:
      - frontend

  api:
    image: alpine:latest
    command: sleep 3600
    networks:
      - frontend
      - backend

  db:
    image: alpine:latest
    command: sleep 3600
    networks:
      - backend

networks:
  frontend:
  backend:
EOF

docker compose up -d

# api ve a proxy y a db
docker compose exec api ping -c 1 proxy   # ✓
docker compose exec api ping -c 1 db      # ✓

# proxy ve a api pero NO a db
docker compose exec proxy ping -c 1 api   # ✓
docker compose exec proxy ping -c 1 db 2>&1 || echo "proxy no ve db ✓"

# db ve a api pero NO a proxy
docker compose exec db ping -c 1 api      # ✓
docker compose exec db ping -c 1 proxy 2>&1 || echo "db no ve proxy ✓"

docker compose down
cd / && rm -rf /tmp/compose_nets
```

### Ejercicio 2 — Red interna (sin internet)

```bash
mkdir -p /tmp/compose_internal && cd /tmp/compose_internal

cat > compose.yml << 'EOF'
services:
  isolated:
    image: alpine:latest
    command: sleep 3600
    networks:
      - no_internet

  connected:
    image: alpine:latest
    command: sleep 3600
    networks:
      - no_internet
      - default

networks:
  no_internet:
    internal: true
EOF

docker compose up -d

# isolated no tiene acceso a internet
docker compose exec isolated ping -c 1 -W 3 8.8.8.8 2>&1 || echo "Sin internet ✓"

# connected sí tiene acceso (está también en default)
docker compose exec connected ping -c 1 -W 3 8.8.8.8

# Ambos se ven entre sí en la red interna
docker compose exec isolated ping -c 1 connected   # ✓

docker compose down
cd / && rm -rf /tmp/compose_internal
```

### Ejercicio 3 — Comunicar dos proyectos Compose

```bash
# Proyecto A
mkdir -p /tmp/compose_a && cd /tmp/compose_a
cat > compose.yml << 'EOF'
services:
  service_a:
    image: alpine:latest
    command: sleep 3600
    networks:
      - shared

networks:
  shared:
    name: inter_project_net
EOF

docker compose up -d

# Proyecto B
mkdir -p /tmp/compose_b && cd /tmp/compose_b
cat > compose.yml << 'EOF'
services:
  service_b:
    image: alpine:latest
    command: sleep 3600
    networks:
      - shared

networks:
  shared:
    external: true
    name: inter_project_net
EOF

docker compose up -d

# service_b puede resolver service_a
docker compose exec service_b ping -c 1 service_a   # ✓

# Limpiar
cd /tmp/compose_b && docker compose down
cd /tmp/compose_a && docker compose down
rm -rf /tmp/compose_a /tmp/compose_b
```
