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
| `service_started` | El contenedor esté running | Nada (default si solo pones `- db`) |
| `service_healthy` | El healthcheck pase | `healthcheck:` definido en dependencia |
| `service_completed_successfully` | El contenedor termine con exit 0 | — |

### Formas de expresar depends_on

```yaml
services:
  web:
    # Forma corta (equivalente a service_started)
    depends_on:
      - db
      - redis

    # Forma larga (explícita)
    depends_on:
      db:
        condition: service_started
      redis:
        condition: service_started

    # Forma con healthcheck
    depends_on:
      db:
        condition: service_healthy
```

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
      start_period: 5s

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

### `docker compose run` y dependencias

`docker compose run` **sí arranca las dependencias** del servicio por defecto.
Para ejecutar un servicio sin arrancar sus dependencias, usar `--no-deps`:

```bash
# run SÍ arranca las dependencias (db, redis, etc.)
docker compose run --rm web python3 manage.py migrate
# Arranca db y redis primero, luego ejecuta el comando en web

# --no-deps evita arrancar dependencias
docker compose run --rm --no-deps web python3 -c "print('hello')"
# Solo ejecuta web, sin arrancar db ni redis
```

**Nota**: `--service-ports` es un flag diferente — publica los puertos del
servicio en el host (que `run` no hace por defecto). No tiene relación con
las dependencias.

## Estrategia de resiliencia: wait-for

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

La mejor práctica es que la **aplicación** maneje la reconexión internamente
(reintentos con backoff exponencial), sin depender del entorno de despliegue.

---

**Nota**: el directorio `labs/` contiene un laboratorio con 4 partes y
archivos Compose listos para usar. Consultar `LABS.md` para la guía.

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
sleep 10
```

**Predicción**: `db` arranca primero. `api` NO arranca hasta que `db` pase el
healthcheck (`pg_isready` devuelve OK). En `docker compose ps`, `db` aparece
como "healthy" y `api` como "running".

```bash
docker compose ps
docker compose logs --tail 3 api
docker compose logs --tail 3 db

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

  app:
    image: alpine:latest
    depends_on:
      init:
        condition: service_completed_successfully
    command: sh -c 'echo "App arrancó después de init" && sleep 3600'
EOF

docker compose up -d
sleep 5
docker compose ps
```

**Predicción**: `init` se ejecuta, duerme 2 segundos, y termina con exit 0.
Solo entonces `app` arranca. En `docker compose ps`, `init` aparece como
"exited (0)" y `app` como "running".

```bash
docker compose logs
# init: Inicialización completada
# app: App arrancó después de init

docker compose down
cd / && rm -rf /tmp/compose_init
```

### Ejercicio 3 — Init fallida bloquea el arranque

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
```

**Predicción**: `migrate` falla con exit 1. Como `app` espera
`service_completed_successfully`, Compose NO crea `app`. En `docker compose ps`,
solo aparece `migrate` con estado "exited (1)". `app` no existe.

```bash
docker compose ps
# migrate: exited (1)
# app: no aparece

docker compose down
cd / && rm -rf /tmp/compose_fail
```

### Ejercicio 4 — Comparar tiempos de arranque

```bash
mkdir -p /tmp/compose_timing && cd /tmp/compose_timing

cat > compose.yml << 'EOF'
services:
  instant:
    image: alpine:latest
    command: sh -c 'echo "instant arrancó a las $$(date +%T)" && sleep 3600'

  db:
    image: postgres:16-alpine
    environment:
      POSTGRES_PASSWORD: secret
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 2s
      timeout: 2s
      retries: 5
      start_period: 5s

  delayed:
    image: alpine:latest
    depends_on:
      db:
        condition: service_healthy
    command: sh -c 'echo "delayed arrancó a las $$(date +%T)" && sleep 3600'
EOF

docker compose up -d
sleep 15
```

**Predicción**: `instant` arranca inmediatamente (no depende de nadie).
`delayed` arranca varios segundos después, cuando `db` pasa su healthcheck.
Los timestamps en los logs muestran la diferencia.

```bash
echo "=== instant ==="
docker compose logs instant | head -1

echo "=== delayed ==="
docker compose logs delayed | head -1

echo "=== db estado ==="
docker compose ps db

docker compose down -v
cd / && rm -rf /tmp/compose_timing
```

### Ejercicio 5 — Wait script con nc

```bash
mkdir -p /tmp/compose_wait && cd /tmp/compose_wait

cat > wait.sh << 'SCRIPT'
#!/bin/sh
HOST=$1
PORT=$2
echo "Esperando a $HOST:$PORT..."
i=0
until nc -z "$HOST" "$PORT" 2>/dev/null; do
  i=$((i+1))
  echo "  intento $i..."
  sleep 1
done
echo "$HOST:$PORT está disponible"
SCRIPT
chmod +x wait.sh

cat > compose.yml << 'EOF'
services:
  db:
    image: postgres:16-alpine
    environment:
      POSTGRES_PASSWORD: secret
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 2s
      timeout: 2s
      retries: 10
      start_period: 3s

  app:
    image: alpine:latest
    depends_on:
      db:
        condition: service_healthy
    command: >
      sh -c "
        /wait.sh db 5432 &&
        echo 'DB lista! App iniciada a las $$(date +%T)'
      "
    volumes:
      - ./wait.sh:/wait.sh:ro
EOF

docker compose up -d
sleep 15
docker compose logs app
```

**Predicción**: `app` espera a que `db` sea healthy (por `depends_on`), luego
ejecuta el wait script que verifica con `nc -z` que el puerto 5432 esté
abierto. El script hace redundancia — en la práctica el healthcheck ya
garantiza que Postgres está listo. Verás pocos o cero reintentos en el script
porque el healthcheck ya esperó.

```bash
docker compose down -v
cd / && rm -rf /tmp/compose_wait
```

### Ejercicio 6 — docker compose run con dependencias

```bash
mkdir -p /tmp/compose_run_deps && cd /tmp/compose_run_deps

cat > compose.yml << 'EOF'
services:
  db:
    image: postgres:16-alpine
    environment:
      POSTGRES_PASSWORD: secret
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 2s
      timeout: 2s
      retries: 10
      start_period: 5s

  app:
    image: postgres:16-alpine
    depends_on:
      db:
        condition: service_healthy
    environment:
      PGPASSWORD: secret
    command: sleep 3600
EOF

# run SÍ arranca dependencias (db)
docker compose run --rm app psql -h db -U postgres -c "SELECT 'hello from run';"
```

**Predicción**: `docker compose run` arranca `db` automáticamente (porque `app`
depende de `db`), espera a que sea healthy, y luego ejecuta `psql`. La consulta
devuelve "hello from run".

```bash
# run con --no-deps NO arranca dependencias
docker compose down -v
docker compose run --rm --no-deps app psql -h db -U postgres -c "SELECT 1;" 2>&1 || echo "Conexión falló — db no está corriendo (esperado)"
```

**Predicción**: con `--no-deps`, `db` no arranca y la conexión falla con
"Connection refused".

```bash
docker compose down -v
cd / && rm -rf /tmp/compose_run_deps
```

### Ejercicio 7 — Cadena de dependencias: db → migrate → app

```bash
mkdir -p /tmp/compose_chain && cd /tmp/compose_chain

cat > compose.yml << 'EOF'
services:
  db:
    image: postgres:16-alpine
    environment:
      POSTGRES_PASSWORD: secret
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 2s
      timeout: 2s
      retries: 10
      start_period: 5s

  migrate:
    image: postgres:16-alpine
    depends_on:
      db:
        condition: service_healthy
    environment:
      PGPASSWORD: secret
    command: >
      sh -c "
        echo 'Ejecutando migración...' &&
        psql -h db -U postgres -c 'CREATE TABLE IF NOT EXISTS users (id SERIAL, name TEXT);' &&
        psql -h db -U postgres -c \"INSERT INTO users (name) VALUES ('admin');\" &&
        echo 'Migración completada'
      "
    restart: "no"

  app:
    image: postgres:16-alpine
    depends_on:
      db:
        condition: service_healthy
      migrate:
        condition: service_completed_successfully
    environment:
      PGPASSWORD: secret
    command: >
      sh -c "
        echo 'App arrancó — consultando datos:' &&
        psql -h db -U postgres -c 'SELECT * FROM users;' &&
        sleep 3600
      "
EOF

docker compose up -d
sleep 15
```

**Predicción**: flujo secuencial:
1. `db` arranca y se vuelve healthy
2. `migrate` se ejecuta: crea tabla `users`, inserta "admin", termina con exit 0
3. `app` arranca y consulta la tabla — muestra el registro insertado

```bash
docker compose logs migrate
# Ejecutando migración...
# Migración completada

docker compose logs app
# App arrancó — consultando datos:
#  id | name
#   1 | admin

docker compose ps
# db: running (healthy)
# migrate: exited (0)
# app: running

docker compose down -v
cd / && rm -rf /tmp/compose_chain
```

### Ejercicio 8 — Orden de apagado inverso

```bash
mkdir -p /tmp/compose_shutdown && cd /tmp/compose_shutdown

cat > compose.yml << 'EOF'
services:
  db:
    image: alpine:latest
    command: sh -c 'echo "db arrancó" && trap "echo db recibió SIGTERM" TERM && sleep 3600 & wait'

  cache:
    image: alpine:latest
    depends_on:
      - db
    command: sh -c 'echo "cache arrancó" && trap "echo cache recibió SIGTERM" TERM && sleep 3600 & wait'

  app:
    image: alpine:latest
    depends_on:
      - db
      - cache
    command: sh -c 'echo "app arrancó" && trap "echo app recibió SIGTERM" TERM && sleep 3600 & wait'
EOF

docker compose up -d
sleep 2

echo "=== Orden de arranque ==="
docker compose logs | grep arrancó

echo "=== Apagando ==="
docker compose down 2>&1 | grep -E '(Stopping|Removed)'
```

**Predicción**: arranque: `db` → `cache` → `app` (respetando dependencias).
Apagado: `app` → `cache` → `db` (orden inverso). Los servicios que dependen
de otros se detienen primero para evitar conexiones rotas.

```bash
cd / && rm -rf /tmp/compose_shutdown
```

---

## Resumen visual

```
┌─────────────────────────────────────────────────────────────────┐
│                    ARRANQUE CON depends_on                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  depends_on simple (service_started):                            │
│  ┌─────────┐                                                    │
│  │   db    │──container running──┐                              │
│  └─────────┘                      │                              │
│  ┌─────────┐                      │                              │
│  │  redis  │──container running──┼──► web arranca               │
│  └─────────┘                      │                              │
│                                   ▼                              │
│  ✗ db puede no estar listo aún cuando web arranca               │
│                                                                  │
│  depends_on + healthcheck (service_healthy):                     │
│  ┌─────────┐                                                    │
│  │   db    │──healthy──┐                                        │
│  └─────────┘           │                                        │
│  ┌─────────┐           │                                        │
│  │  redis  │──healthy──┼──► web arranca                         │
│  └─────────┘           │                                        │
│                        ▼                                        │
│  ✓ db y redis están listos cuando web arranca                   │
│                                                                  │
│  Cadena con init (service_completed_successfully):              │
│  ┌─────────┐      ┌──────────┐                                  │
│  │   db    │──►   │ migrate  │──exit 0──► web arranca           │
│  │ healthy │      │          │                                   │
│  └─────────┘      └──────────┘                                  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## Checklist para orquestar servicios

- [ ] Servicios que otros dependen tienen `healthcheck` definido
- [ ] `depends_on` usa `condition: service_healthy` para esperar readiness
- [ ] Tareas de inicialización usan `condition: service_completed_successfully`
- [ ] `start_period` en healthcheck es suficiente para la inicialización
- [ ] La aplicación implementa reintentos de conexión (no depende solo de depends_on)
- [ ] `restart: "no"` en servicios de inicialización (para ver errores)
- [ ] `restart: on-failure` para servicios que deben reiniciarse si fallan
