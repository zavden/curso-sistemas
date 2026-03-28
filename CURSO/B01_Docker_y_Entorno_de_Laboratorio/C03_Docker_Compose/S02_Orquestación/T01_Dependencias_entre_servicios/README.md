# T01 — Dependencias entre servicios

## El problema del orden de arranque

En una aplicación multi-contenedor, los servicios tienen dependencias entre sí:
la API necesita que la base de datos esté lista antes de conectarse, el frontend
necesita que la API esté aceptando requests, etc.

Sin `depends_on`, Compose arranca todos los servicios **en paralelo**. Si la API
intenta conectarse a PostgreSQL antes de que éste haya terminado de inicializar,
la conexión falla.

```
Sin depends_on (paralelo):
  t=0s  ─── web arranca ──── db arranca ──── redis arranca
  t=1s  ─── web intenta conectar a db ──── db aún inicializando
  t=2s  ─── web: "Connection refused" ✗
```

## `depends_on` simple

Define el **orden de arranque**: Compose arranca las dependencias antes del
servicio que las declara.

```yaml
services:
  web:
    image: myapi:latest
    depends_on:
      - db
      - redis
  db:
    image: postgres:16
    environment:
      POSTGRES_PASSWORD: secret
  redis:
    image: redis:7
```

```bash
docker compose up -d
# 1. Arranca db y redis (en paralelo entre sí)
# 2. Cuando ambos contenedores están "running", arranca web
```

```
Con depends_on simple:
  t=0s  ─── db arranca ──── redis arranca
  t=1s  ─── db contenedor "running" ──── redis contenedor "running"
  t=2s  ─── web arranca
  t=3s  ─── web intenta conectar a db ──── db aún inicializando
  t=4s  ─── web: "Connection refused" ✗  ← ¡sigue fallando!
```

### La limitación de `depends_on` simple

`depends_on` sin condición solo espera a que el **contenedor esté corriendo**
(estado `running`), no a que el **servicio esté listo para aceptar conexiones**.

Un contenedor PostgreSQL tarda unos segundos después de arrancar para:
1. Inicializar el directorio de datos
2. Ejecutar scripts de inicialización
3. Empezar a aceptar conexiones TCP

Durante ese tiempo, el contenedor está `running` pero PostgreSQL aún no acepta
conexiones.

## `depends_on` con condiciones

Para esperar a que un servicio esté **realmente listo**, se combina `depends_on`
con `healthcheck`:

```yaml
services:
  web:
    image: myapi:latest
    depends_on:
      db:
        condition: service_healthy
      redis:
        condition: service_healthy

  db:
    image: postgres:16
    environment:
      POSTGRES_PASSWORD: secret
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 5s
      timeout: 5s
      retries: 5
      start_period: 10s

  redis:
    image: redis:7
    healthcheck:
      test: ["CMD", "redis-cli", "ping"]
      interval: 5s
      timeout: 3s
      retries: 5
```

```
Con depends_on + healthcheck:
  t=0s   ─── db arranca ──── redis arranca
  t=3s   ─── redis: healthy ✓
  t=8s   ─── db: pg_isready → OK, healthy ✓
  t=8s   ─── web arranca (ambas dependencias healthy)
  t=9s   ─── web conecta a db ──── db acepta conexión ✓
```

### Condiciones disponibles

| Condición | Espera a que... | Requiere |
|---|---|---|
| `service_started` | El contenedor esté running | Nada (default) |
| `service_healthy` | El healthcheck pase | `healthcheck:` definido |
| `service_completed_successfully` | El contenedor termine con exit 0 | — |

## `service_completed_successfully`

Espera a que un servicio **termine exitosamente** (exit code 0). Es útil para
tareas de inicialización que deben completarse antes de arrancar otros servicios:

```yaml
services:
  migrate:
    image: myapi:latest
    command: python3 manage.py migrate
    depends_on:
      db:
        condition: service_healthy

  web:
    image: myapi:latest
    command: python3 manage.py runserver
    depends_on:
      db:
        condition: service_healthy
      migrate:
        condition: service_completed_successfully

  db:
    image: postgres:16
    environment:
      POSTGRES_PASSWORD: secret
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 5s
      timeout: 5s
      retries: 5
```

```
Flujo:
  1. db arranca
  2. db se vuelve healthy (pg_isready OK)
  3. migrate arranca (ejecuta migraciones)
  4. migrate termina con exit 0
  5. web arranca (db healthy + migrate completado)
```

Si `migrate` falla (exit code != 0), `web` **no arranca** y Compose muestra
un error.

## Healthchecks comunes para servicios populares

| Servicio | Healthcheck |
|---|---|
| PostgreSQL | `pg_isready -U postgres` |
| MySQL/MariaDB | `mysqladmin ping -h localhost` |
| Redis | `redis-cli ping` |
| MongoDB | `mongosh --eval "db.adminCommand('ping')"` |
| Elasticsearch | `curl -f http://localhost:9200/_cluster/health` |
| Nginx | `curl -f http://localhost/` |
| RabbitMQ | `rabbitmq-diagnostics -q ping` |

```yaml
# Ejemplo completo con healthchecks típicos
services:
  db:
    image: postgres:16
    environment:
      POSTGRES_PASSWORD: secret
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 5s
      timeout: 5s
      retries: 5
      start_period: 10s

  cache:
    image: redis:7
    healthcheck:
      test: ["CMD", "redis-cli", "ping"]
      interval: 5s
      timeout: 3s
      retries: 3

  api:
    image: myapi:latest
    depends_on:
      db:
        condition: service_healthy
      cache:
        condition: service_healthy
```

## Orden de apagado

`depends_on` afecta al **arranque** pero el apagado sigue el orden **inverso**:

```bash
docker compose up -d
# Arranque: db → redis → web

docker compose down
# Apagado: web → redis → db (inverso)
```

Esto tiene sentido: primero se detienen los servicios que dependen de otros,
y después las dependencias.

## Limitaciones de `depends_on`

### No reinicia dependencias caídas

Si `db` se cae después del arranque, Compose **no reinicia `web`**
automáticamente. `depends_on` solo controla el orden de arranque inicial:

```bash
# db se cae
docker compose stop db
# web sigue corriendo pero no puede conectar a db
# Compose no hace nada automáticamente
```

Para resiliencia después del arranque, la aplicación debe implementar:
- Reintentos de conexión con backoff exponencial
- Circuit breakers
- Restart policies (`restart: on-failure`)

### No funciona con `docker compose run`

```bash
# run NO arranca las dependencias por defecto
docker compose run --rm web bash
# web arranca pero db y redis no

# Para arrancar dependencias con run:
docker compose run --rm --service-ports web bash
# Aún no arranca dependencias

# Hay que arrancar las dependencias primero:
docker compose up -d db redis
docker compose run --rm web python3 manage.py migrate
```

## Estrategia de resiliencia: wait-for-it

Para entornos donde `depends_on` con healthcheck no es suficiente (o no está
disponible), se puede usar un script wrapper que espera la conexión:

```yaml
services:
  web:
    image: myapi:latest
    depends_on:
      db:
        condition: service_healthy
    # Doble seguridad: esperar activamente a que db esté lista
    command: >
      sh -c "
        until pg_isready -h db -U postgres; do
          echo 'Esperando a PostgreSQL...'
          sleep 2
        done
        echo 'PostgreSQL listo'
        python3 app.py
      "
```

En la práctica, `depends_on: condition: service_healthy` es suficiente para
la mayoría de casos. Los scripts de espera son un backup para situaciones
donde el healthcheck no captura toda la inicialización.

---

## Ejercicios

### Ejercicio 1 — depends_on simple vs con healthcheck

```bash
mkdir -p /tmp/compose_deps && cd /tmp/compose_deps

cat > compose.yml << 'EOF'
services:
  api:
    image: alpine:latest
    depends_on:
      db:
        condition: service_healthy
    command: sh -c 'echo "API arrancó a las $$(date +%T)" && sleep 3600'

  db:
    image: postgres:16-alpine
    environment:
      POSTGRES_PASSWORD: secret
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 3s
      timeout: 3s
      retries: 10
      start_period: 5s
EOF

docker compose up -d

# Observar el orden: db se vuelve healthy antes de que api arranque
docker compose logs --tail 5 api
docker compose logs --tail 5 db

# Ver que db es healthy
docker compose ps

docker compose down -v
cd / && rm -rf /tmp/compose_deps
```

### Ejercicio 2 — service_completed_successfully

```bash
mkdir -p /tmp/compose_init && cd /tmp/compose_init

cat > compose.yml << 'EOF'
services:
  init:
    image: alpine:latest
    command: sh -c 'echo "Inicialización completada" && sleep 2'
    # Se ejecuta y termina

  app:
    image: alpine:latest
    depends_on:
      init:
        condition: service_completed_successfully
    command: sh -c 'echo "App arrancó después de init" && sleep 3600'
EOF

docker compose up -d

# init se ejecuta, termina con exit 0, luego app arranca
sleep 5
docker compose ps
# init: exited (0)
# app: running

docker compose logs
# init: Inicialización completada
# app: App arrancó después de init

docker compose down
cd / && rm -rf /tmp/compose_init
```

### Ejercicio 3 — init fallida bloquea el arranque

```bash
mkdir -p /tmp/compose_fail && cd /tmp/compose_fail

cat > compose.yml << 'EOF'
services:
  migrate:
    image: alpine:latest
    command: sh -c 'echo "Migración fallida" && exit 1'

  app:
    image: alpine:latest
    depends_on:
      migrate:
        condition: service_completed_successfully
    command: echo "Nunca debería llegar aquí"
EOF

docker compose up -d 2>&1
# migrate falla con exit 1 → app no arranca

docker compose ps
# migrate: exited (1)
# app: no está listado (nunca se creó)

docker compose down
cd / && rm -rf /tmp/compose_fail
```
