# T01 — Sintaxis YAML

## ¿Qué es Docker Compose?

Docker Compose permite definir y ejecutar **aplicaciones multi-contenedor** de
forma declarativa. En lugar de ejecutar múltiples `docker run` con flags largos,
describes toda la aplicación en un archivo YAML y la levantas con un solo comando.

```bash
# Sin Compose: múltiples comandos manuales
docker network create myapp
docker volume create pgdata
docker run -d --name db --network myapp -v pgdata:/var/lib/postgresql/data \
  -e POSTGRES_PASSWORD=secret postgres:16
docker run -d --name web --network myapp -p 8080:80 \
  -e DATABASE_URL=postgres://postgres:secret@db:5432/app nginx

# Con Compose: un archivo + un comando
docker compose up -d
```

## YAML básico

YAML (YAML Ain't Markup Language) es el formato del archivo de configuración de
Compose. Las reglas fundamentales:

### Indentación

- **Siempre espacios**, nunca tabs (YAML no permite tabs)
- Convención: **2 espacios** por nivel de indentación
- La indentación define la estructura (como Python)

```yaml
# Correcto: 2 espacios
services:
  web:
    image: nginx
    ports:
      - "8080:80"

# ERROR: tabs
services:
	web:          # ← tab — YAML falla
```

### Key-value (mapas)

```yaml
# Mapa simple
image: nginx
container_name: web_server
restart: always

# Mapa anidado
environment:
  DB_HOST: postgres
  DB_PORT: "5432"
```

### Listas

```yaml
# Lista con -
ports:
  - "8080:80"
  - "8443:443"

# Lista inline (menos legible pero válida)
ports: ["8080:80", "8443:443"]
```

### Strings

```yaml
# Sin comillas (funciona para la mayoría de strings)
image: nginx

# Con comillas (necesario para caracteres especiales o para evitar parsing de tipos)
ports:
  - "8080:80"         # Comillas necesarias: contiene ":"
environment:
  DB_PORT: "5432"     # Sin comillas, YAML lo interpreta como número
  ENABLED: "true"     # Sin comillas, YAML lo interpreta como booleano
```

**Trampa de tipos YAML**: sin comillas, estos valores se interpretan
automáticamente:

| Valor sin comillas | YAML lo interpreta como |
|---|---|
| `true`, `false`, `yes`, `no` | Booleano |
| `5432` | Número entero |
| `3.14` | Número decimal |
| `null`, `~` | Null |
| `2024-01-15` | Fecha |

Para forzar string, usar comillas: `"yes"`, `"5432"`, `"null"`.

### Multi-line

```yaml
# | preserva newlines (literal block)
command: |
  sh -c "
    echo 'Starting...' &&
    python3 app.py
  "

# > folds newlines en espacios (folded block)
description: >
  This is a long
  description that
  becomes one line.
# Resultado: "This is a long description that becomes one line."
```

## Estructura de un archivo Compose

Un archivo `docker-compose.yml` (o `compose.yml`) tiene tres secciones
principales:

```yaml
# Sección principal: define los contenedores
services:
  web:
    image: nginx:latest
    ports:
      - "8080:80"
    volumes:
      - web-data:/usr/share/nginx/html
    networks:
      - frontend

  db:
    image: postgres:16
    environment:
      POSTGRES_PASSWORD: secret
    volumes:
      - db-data:/var/lib/postgresql/data
    networks:
      - backend

# Named volumes (persisten entre down/up)
volumes:
  web-data:
  db-data:

# Redes custom
networks:
  frontend:
  backend:
```

### `services`

Cada clave dentro de `services` es un **servicio** (un tipo de contenedor).
Compose crea uno o más contenedores por servicio.

### `volumes`

Define named volumes. Si un volumen referenciado en un servicio no está declarado
aquí, Compose genera un warning. Los volúmenes declarados aquí persisten entre
`docker compose down` y `docker compose up`.

### `networks`

Define redes custom. Si no defines ninguna red, Compose crea automáticamente una
red bridge llamada `<proyecto>_default` donde todos los servicios se conectan.

## Versiones del formato

### Compose v1 (legacy)

```yaml
version: "3.8"   # ← Obligatorio en v1
services:
  web:
    image: nginx
```

Usaba el binario `docker-compose` (con guión). Está **deprecated**.

### Compose v2 (actual)

```yaml
# No requiere "version:" — se detecta automáticamente
services:
  web:
    image: nginx
```

Usa el plugin `docker compose` (sin guión, como subcomando de docker). Es el
formato actual y recomendado.

```bash
# v1 (deprecated)
docker-compose up

# v2 (actual)
docker compose up
```

Si encuentras `version: "3.8"` en un archivo, puedes eliminarlo — Compose v2
lo ignora.

## Opciones de servicio

### `image` y `build`

```yaml
services:
  # Usar una imagen existente
  db:
    image: postgres:16

  # Construir desde Dockerfile
  api:
    build: ./api
    # Equivalente expandido:
    # build:
    #   context: ./api
    #   dockerfile: Dockerfile

  # Build con opciones
  web:
    build:
      context: ./web
      dockerfile: Dockerfile.prod
      args:
        APP_VERSION: "2.0"
    image: myregistry/web:2.0   # Tag para la imagen construida
```

Si especificas `build` e `image`, Compose construye la imagen y le asigna el tag
de `image`.

### `ports`

```yaml
services:
  web:
    ports:
      - "8080:80"              # host:container
      - "127.0.0.1:8080:80"   # solo localhost
      - "8080-8090:80-90"     # rango
      - "80"                   # container port, host port aleatorio
```

**Siempre usar comillas** en los puertos. Sin comillas, `80:80` se interpreta
correctamente, pero `8080:80` podría dar problemas en YAML parsers estrictos.

### `volumes`

```yaml
services:
  web:
    volumes:
      # Named volume
      - app-data:/app/data

      # Bind mount (path relativo al Compose file)
      - ./src:/app/src

      # Bind mount read-only
      - ./config/nginx.conf:/etc/nginx/nginx.conf:ro

      # tmpfs (solo tipo largo)

volumes:
  app-data:   # Declarar named volumes usados
```

### `environment` y `env_file`

```yaml
services:
  api:
    # Formato lista
    environment:
      - DB_HOST=postgres
      - DB_PORT=5432

    # Formato mapa (equivalente)
    environment:
      DB_HOST: postgres
      DB_PORT: "5432"

    # Archivo externo
    env_file:
      - .env.db
      - .env.app
```

### `command` y `entrypoint`

```yaml
services:
  web:
    image: python:3.12
    # Sobrescribe CMD de la imagen
    command: python3 app.py --port 8080

    # Forma lista (recomendada)
    command: ["python3", "app.py", "--port", "8080"]

    # Sobrescribe ENTRYPOINT
    entrypoint: ["python3"]
```

### `restart`

```yaml
services:
  web:
    restart: "no"           # Default: no reiniciar
    # restart: always       # Siempre reiniciar
    # restart: on-failure   # Solo si exit code != 0
    # restart: unless-stopped  # Siempre, excepto si fue detenido manualmente
```

### `depends_on`

```yaml
services:
  web:
    depends_on:
      - db
      - redis
  db:
    image: postgres:16
  redis:
    image: redis:7
```

Se cubre en detalle en S02 (Orquestación).

## Nombre del proyecto

Compose asigna un **nombre de proyecto** que prefija todos los recursos creados
(contenedores, redes, volúmenes). Por defecto, es el nombre del directorio:

```
proyecto/
├── docker-compose.yml

$ cd proyecto
$ docker compose up -d
# Crea:
#   Contenedores: proyecto-web-1, proyecto-db-1
#   Red: proyecto_default
#   Volúmenes: proyecto_db-data
```

```bash
# Cambiar el nombre del proyecto
docker compose -p myapp up -d
# Crea: myapp-web-1, myapp-db-1, myapp_default

# O con variable de entorno
COMPOSE_PROJECT_NAME=myapp docker compose up -d
```

## Red default de Compose

Compose crea automáticamente una red bridge donde **todos los servicios se
ven por nombre**:

```yaml
services:
  web:
    image: nginx
  db:
    image: postgres:16
```

```bash
docker compose up -d

# web puede conectarse a db usando el nombre del servicio
docker compose exec web ping -c 1 db
# PING db (172.20.0.3): 56 data bytes — resuelve automáticamente

# Ver la red creada
docker network ls | grep default
# abc123  proyecto_default  bridge  local
```

El DNS interno resuelve nombres de **servicio** (no de contenedor). `db`
resuelve al contenedor del servicio `db`, independientemente de cómo se
llame el contenedor.

## Nombres de archivo

Compose busca archivos en este orden:

1. `compose.yml`
2. `compose.yaml`
3. `docker-compose.yml`
4. `docker-compose.yaml`

```bash
# Usar un archivo específico
docker compose -f docker-compose.prod.yml up -d

# Combinar múltiples archivos (merge)
docker compose -f docker-compose.yml -f docker-compose.override.yml up -d
```

El archivo `docker-compose.override.yml` se aplica automáticamente si existe
junto a `docker-compose.yml` (sin necesidad de `-f`).

---

## Ejercicios

### Ejercicio 1 — Compose file mínimo

```bash
mkdir -p /tmp/compose_test && cd /tmp/compose_test

cat > compose.yml << 'EOF'
services:
  web:
    image: nginx:latest
    ports:
      - "8080:80"
EOF

docker compose up -d

# Verificar
docker compose ps
curl -s http://localhost:8080 | head -3

# Ver la red creada
docker network ls | grep compose_test

# Limpiar
docker compose down
cd / && rm -rf /tmp/compose_test
```

### Ejercicio 2 — Trampa de tipos YAML

```bash
mkdir -p /tmp/compose_types && cd /tmp/compose_types

cat > compose.yml << 'EOF'
services:
  test:
    image: alpine:latest
    environment:
      STRING_OK: "yes"
      BOOL_TRAP: yes
      NUMBER_OK: "3306"
      NUMBER_TRAP: 3306
    command: ["sh", "-c", "env | sort | grep -E '(STRING|BOOL|NUMBER)'"]
EOF

docker compose run --rm test
# STRING_OK=yes
# BOOL_TRAP=true      ← YAML convirtió "yes" a booleano "true"
# NUMBER_OK=3306
# NUMBER_TRAP=3306     ← puede variar según parser

docker compose down
cd / && rm -rf /tmp/compose_types
```

### Ejercicio 3 — Red default y DNS

```bash
mkdir -p /tmp/compose_dns && cd /tmp/compose_dns

cat > compose.yml << 'EOF'
services:
  server:
    image: nginx:latest
  client:
    image: alpine:latest
    command: sleep 3600
    depends_on:
      - server
EOF

docker compose up -d

# El cliente puede resolver "server" por DNS
docker compose exec client ping -c 2 server

# Ver la red
docker network inspect compose_dns_default --format \
  '{{range .Containers}}{{.Name}}: {{.IPv4Address}}{{"\n"}}{{end}}'

docker compose down
cd / && rm -rf /tmp/compose_dns
```
