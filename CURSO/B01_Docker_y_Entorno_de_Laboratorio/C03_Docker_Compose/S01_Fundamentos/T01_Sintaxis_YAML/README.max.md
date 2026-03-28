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
| `true`, `false`, `yes`, `no`, `on`, `off` | Booleano |
| `5432` | Número entero |
| `3.14` | Número decimal |
| `null`, `~` | Null |
| `2024-01-15` | Fecha |
| `0x10` | Hexadecimal (16) |
| `1e5` | Notación científica (100000) |

Para forzar string, usar comillas: `"yes"`, `"5432"`, `"null"`.

### Multi-line strings

```yaml
# | preserva newlines y espacios (literal block)
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

# + y - modifiers
long_text: |+
  First line
  Second line

  Third line (with extra newline)
# Preserve trailing newlines

folded_text: |-
  Line one
  Line two
# Remove trailing newline
```

## Estructura de un archivo Compose

Un archivo `compose.yml` (o `compose.yaml`, o legacy `docker-compose.yml`) tiene
tres secciones principales:

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
      - "127.0.0.1:8080:80"   # solo localhost en el host
      - "8080-8090:80-90"     # rango de puertos
      - "80"                   # container port, host port aleatorio
      - "443/tcp"              # protocolo específico
      - "8000-9000:8000"       # host range : container port
```

**Siempre usar comillas** en los puertos. Sin comillas, `8080:80` podría dar
problemas en YAML parsers estrictos.

### `expose`

```yaml
services:
  web:
    expose:
      - "3000"
      - "4000"
    # Expone los puertos a servicios conectados, NO al host
```

A diferencia de `ports`, `expose` solo hace accesible el puerto a otros
servicios dentro de la misma red de Compose.

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
      - type: tmpfs
        target: /app/tmp

      # Named volume con opciones
      - type: volume
        source: db-data
        target: /var/lib/postgresql/data
        read_only: true

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

    # Formato mapa (equivalente, más legible)
    environment:
      DB_HOST: postgres
      DB_PORT: "5432"

    # Archivo externo
    env_file:
      - .env.db
      - ./config/.env.app

    # Múltiples archivos (se procesan en orden)
    env_file:
      - path: .env.common
      - path: .env.prod
        required: true
      - path: .env.local
        required: false
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
    restart: always        # Siempre reiniciar (incluso después de reboot)
    restart: on-failure     # Solo si exit code != 0
    restart: unless-stopped # Siempre, excepto si fue detenido manualmente
```

**Importante**: `"no"` debe ir entre comillas porque YAML interpreta `no` como
booleano.

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

### `healthcheck`

```yaml
services:
  db:
    image: postgres:16
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 10s
      timeout: 5s
      retries: 5
      start_period: 10s
```

El healthcheck aparece en `docker compose ps` y puede usarse con `depends_on`
condition.

### `profiles`

```yaml
services:
  web:
    image: nginx

  admin:
    image: nginx
    profiles:
      - admin

  debug:
    image: alpine
    profiles:
      - debug
    command: sleep infinity
```

```bash
# Solo levanta servicios sin profile o con profile "admin"
docker compose up --profile admin

# Levanta servicios con profiles "debug" y "admin"
docker compose --profile debug --profile admin up
```

Los servicios con profiles solo se inician si se especifica el profile.

### `logging`

```yaml
services:
  web:
    image: nginx
    logging:
      driver: "json-file"
      options:
        max-size: "10m"
        max-file: "3"
```

Drivers disponibles: `json-file`, `syslog`, `none`, `local`, `fluentd`, `awslogs`.

### `ulimits`

```yaml
services:
  app:
    image: alpine
    ulimits:
      nofile:
        soft: 1024
        hard: 2048
      nproc:
        soft: 512
        hard: 1024
```

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

##extends

El campo `extends` permite heredar configuración de otro servicio:

```yaml
# base.yml
services:
  web:
    image: nginx
    restart: always
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost/"]
```

```yaml
# compose.yml
services:
  web:
    extends:
      file: base.yml
      service: web
    ports:
      - "8080:80"

  admin:
    extends:
      file: base.yml
      service: web
    profiles:
      - admin
```

**Nota**: `extends` no está disponible en Compose v2 con el formato spec
moderno. Usar `include` en su lugar.

## include

```yaml
# compose.yml
include:
  - compose.common.yml
  - path: compose.database.yml
  - path: compose.cache.yml
    project_directory: ./subproject
```

Permite modularizar Compose files en múltiples archivos.

## Recursos resultante en Docker

Cada servicio crea **contenedores** con nombres predecibles:

| Recurso | Naming pattern |
|---|---|
| Contenedor | `{proyecto}-{servicio}-{número}` |
| Red | `{proyecto}_default` |
| Volumen | `{proyecto}_{volumen}` |
| Red custom | `{proyecto}_{nombre}` |

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

# Ver los contenedores
docker compose ls

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
      HEX_TRAP: 0xff
      SCI_TRAP: 1e3
    command: ["sh", "-c", "env | sort | grep -E '(STRING|BOOL|NUMBER|HEX|SCI)'"]
EOF

docker compose run --rm test
# STRING_OK=yes
# BOOL_TRAP=true      ← YAML convirtió "yes" a booleano "true"
# NUMBER_OK=3306
# NUMBER_TRAP=3306    ← puede variar según parser
# HEX_TRAP=255        ← YAML convierte hex a decimal
# SCI_TRAP=1000       ← YAML convierte notación científica

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

### Ejercicio 4 — Named volumes

```bash
mkdir -p /tmp/compose_vol && cd /tmp/compose_vol

cat > compose.yml << 'EOF'
services:
  app:
    image: alpine:latest
    command: ["sh", "-c", "echo 'data' > /data/file.txt && sleep 3600"]
    volumes:
      - app-data:/data

volumes:
  app-data:
EOF

# Crear y verificar
docker compose up -d

# El archivo persiste
docker compose exec app cat /data/file.txt

# Eliminar el contenedor (no el volumen)
docker compose down

# Volver a crear — el archivo persiste
docker compose up -d
docker compose exec app cat /data/file.txt
# 'data' sigue ahí

# Ver el volumen en Docker
docker volume ls | grep compose_vol

# Limpiar
docker compose down -v  # -v elimina los volúmenes
cd / && rm -rf /tmp/compose_vol
```

### Ejercicio 5 — Logging y restart policy

```bash
mkdir -p /tmp/compose_log && cd /tmp/compose_log

cat > compose.yml << 'EOF'
services:
  web:
    image: nginx:latest
    restart: always
    logging:
      driver: "json-file"
      options:
        max-size: "1m"
        max-file: "3"

  crash:
    image: alpine:latest
    restart: on-failure
    command: ["sh", "-c", "exit 1"]
EOF

docker compose up -d web
docker compose logs web | tail -5

# Ver restart policy
docker inspect compose_log-web-1 --format '{{.HostConfig.RestartPolicy.Name}}'

docker compose down
cd / && rm -rf /tmp/compose_log
```

### Ejercicio 6 — Profiles

```bash
mkdir -p /tmp/compose_profile && cd /tmp/compose_profile

cat > compose.yml << 'EOF'
services:
  web:
    image: nginx:latest
    ports:
      - "8080:80"

  admin:
    image: nginx:latest
    profiles:
      - admin
    command: ["sh", "-c", "sleep infinity"]

  debug:
    image: alpine:latest
    profiles:
      - debug
    command: ["sh", "-c", "sleep infinity"]
EOF

# Solo levanta web
docker compose up -d
docker compose ps

# Levanta también admin
docker compose up -d --profile admin
docker compose ps

# Limpiar
docker compose down
cd / && rm -rf /tmp/compose_profile
```

---

## Resumen de comandos

```bash
# Levantary detener
docker compose up -d
docker compose down
docker compose down -v        # + eliminar volúmenes
docker compose down --remove-orphans

# Ver estado
docker compose ps
docker compose ls
docker compose logs -f

# Ejecutar en servicios
docker compose exec <service> <command>
docker compose run --rm <service> <command>

# rebuild
docker compose build
docker compose build --no-cache
docker compose up --build

# Escalar
docker compose up -d --scale <service>=<count>

# Pausar
docker compose pause
docker compose unpause

# Reiniciar
docker compose restart
```

## Checklist de buena configuración

- [ ] Puertos siempre entre comillas (`"8080:80"`)
- [ ] Valores numéricos de environment entre comillas si son strings
- [ ] `restart: "no"` con comillas para deshabilitar restart
- [ ] Named volumes declarados en sección `volumes:`
- [ ] Redes custom declaradas si se usan en `networks:`
- [ ] `depends_on` para servicios que deben iniciar en orden
- [ ] `env_file` para separar configuración sensible
