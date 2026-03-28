# T02 — Ciclo de vida

## Comandos principales

Docker Compose gestiona el ciclo de vida de todos los servicios de un proyecto
con comandos simples. El flujo habitual es:

```
docker compose up    ──▶  Servicios corriendo
       │                       │
       │ (modificar YAML)      │ docker compose stop
       │                       │
docker compose up    ──▶  Recrea solo lo     docker compose start
(recrea lo cambiado)      que cambió              │
                                           Servicios corriendo
                                                  │
                                           docker compose down
                                                  │
                                           Contenedores, redes eliminados
                                           (volúmenes preservados)
```

## Estados de un servicio

```
┌─────────────────────────────────────────────────────────────────┐
│                                                                 │
│  created → running → paused → exited                            │
│     │         │         │        │                              │
│     │         │         │        └────────────────────────┐     │
│     │         │         │                                 │     │
│     │         │         └──── restart ────────────────────┘     │
│     │         │                                               │
│     │         └────────── stop ─────────────────────────────┘ │
│     │                                                          │
│     └────────────────── start ────────────────────────────────┘ │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘

created    : Contenedor existe pero no ha iniciado
running    : Contenedor activo y ejecutándose
paused     : Contenedor pausado (procesos suspendidos)
exited     : Contenedor detuvo (puede haber sido stop/kill o error)
restarting : Contenedor en proceso de reinicio
dead       : Estado de error (limpio con docker system prune)
```

## `docker compose up`

Crea y arranca todos los servicios definidos en el Compose file. Es el comando
más usado:

```bash
# Arrancar todos los servicios en foreground (logs en terminal)
docker compose up

# Arrancar en background (detached)
docker compose up -d

# Arrancar solo un servicio (y sus dependencias)
docker compose up -d web

# Forzar rebuild de imágenes antes de arrancar
docker compose up -d --build

# Recrear contenedores aunque la config no haya cambiado
docker compose up -d --force-recreate

# Eliminar contenedores de servicios que ya no están en el YAML
docker compose up -d --remove-orphans

# Crear sin arrancar (equivalente a create + start)
docker compose up --no-start
```

### `up` es idempotente

Si los contenedores ya existen y la configuración no cambió, `up` no hace nada:

```bash
docker compose up -d
# Creating network "myapp_default" ...
# Creating myapp-web-1 ...
# Creating myapp-db-1 ...

docker compose up -d
# myapp-web-1 is up-to-date
# myapp-db-1 is up-to-date
# Nada cambió — no recrea nada
```

### `up` detecta cambios

Si modificas el Compose file, `up` recrea **solo los servicios afectados**:

```bash
# Cambiar el puerto de web de 8080 a 9090
# Solo web se recrea, db permanece intacta
docker compose up -d
# myapp-db-1 is up-to-date
# Recreating myapp-web-1 ...
```

### `up` con scaling

```bash
# Escalar servicios (v2 compat)
docker compose up -d --scale web=3

# Con limit flags
docker compose up -d --scale web=3 --scale worker=2
```

## `docker compose create`

Crea contenedores sin arrancarlos (útil para preparar antes de un evento):

```bash
docker compose create
docker compose create --no-recreate  # No recrear existentes
docker compose create web db         # Solo servicios específicos
```

Luego se pueden iniciar con `start` por separado.

## `docker compose down`

Detiene y elimina contenedores y redes del proyecto:

```bash
# Detener y eliminar contenedores + redes
docker compose down

# También eliminar named volumes (¡cuidado con datos!)
docker compose down -v

# También eliminar imágenes construidas con build
docker compose down --rmi all

# Eliminar solo imágenes sin tag custom
docker compose down --rmi local

# Eliminar contenedores de servicios que ya no existen en el YAML
docker compose down --remove-orphans
```

**Importante**: `down` sin `-v` **preserva los volúmenes**. Esto protege los datos
de bases de datos y otros servicios con estado. Usar `-v` solo cuando
intencionalmente quieras borrar los datos.

```bash
# down preserva volúmenes
docker compose down
docker volume ls | grep myapp
# myapp_db-data   ← sigue ahí

# down -v los elimina
docker compose down -v
docker volume ls | grep myapp
# (vacío)
```

## `start`, `stop` y `restart`

Estos comandos operan sobre contenedores **existentes**, sin crear ni destruir:

```bash
# Detener servicios sin destruir contenedores
docker compose stop
# Los contenedores quedan en estado "exited"

# Arrancar servicios detenidos
docker compose start
# Los contenedores vuelven a "running" con su estado previo

# Reiniciar (stop + start)
docker compose restart

# Operar sobre un servicio específico
docker compose stop web
docker compose start web
docker compose restart db
```

### `down` + `up` vs `restart`

| Operación | Contenedores | Red | Volúmenes | Config |
|---|---|---|---|---|
| `restart` | Mismos (stop+start) | Misma | Preservados | No se aplica |
| `down` + `up` | Nuevos | Recreada | Preservados* | Se aplican cambios |

\* Salvo con `down -v`.

`restart` es más rápido pero no aplica cambios del Compose file. `down` + `up`
aplica cambios pero destruye y recrea contenedores.

## `docker compose ps`

Lista los servicios del proyecto y su estado:

```bash
docker compose ps
# NAME            IMAGE        COMMAND                  SERVICE   STATUS          PORTS
# myapp-web-1     nginx        "/docker-entrypoint.…"   web       Up 5 minutes    0.0.0.0:8080->80/tcp
# myapp-db-1      postgres:16  "docker-entrypoint.s…"   db        Up 5 minutes    5432/tcp

# Solo servicios corriendo
docker compose ps --status running

# Servicios pausados
docker compose ps --status paused

# Servicios detenidos
docker compose ps --status exited

# Formato personalizado
docker compose ps --format "{{.Name}}\t{{.Status}}\t{{.Ports}}"

# Formato JSON
docker compose ps --format json
```

## `docker compose ls`

Lista los proyectos de Compose activos:

```bash
docker compose ls
# NAME                VERSION         CONFIG
# myapp               compose.v2      /path/to/compose.yml
# another_project     compose.v2      /path/to/another/

# Filtrar por formato
docker compose ls --format json
```

## `docker compose logs`

Muestra los logs de todos los servicios, coloreados por servicio:

```bash
# Logs de todos los servicios
docker compose logs

# Follow (como tail -f)
docker compose logs -f

# Logs de un servicio específico
docker compose logs db

# Últimas N líneas
docker compose logs --tail 50

# Follow de un servicio específico
docker compose logs -f web

# Con timestamps
docker compose logs -t

# Combinar opciones
docker compose logs -f -t --tail 100 db

#since y --until
docker compose logs --since 30m
docker compose logs --since "2024-01-15T10:00:00"
docker compose logs --until "2024-01-15T11:00:00"
```

Los colores son asignados automáticamente: cada servicio tiene un color distinto
para distinguirlos visualmente.

## `docker compose exec`

Ejecuta un comando en un contenedor **que ya está corriendo**:

```bash
# Shell interactiva en el servicio web
docker compose exec web bash

# Comando específico
docker compose exec db psql -U postgres

# Como usuario específico
docker compose exec --user root web sh

# Sin TTY (para scripts)
docker compose exec -T web cat /etc/nginx/nginx.conf

# Con entorno específico
docker compose exec -e PGUSER=admin db psql

# Directorio de trabajo
docker compose exec -w /app web pwd
```

`exec` usa el contenedor existente — no crea uno nuevo.

## `docker compose run`

Crea un **contenedor nuevo y efímero** para ejecutar un comando one-shot:

```bash
# Ejecutar un comando en un contenedor nuevo del servicio web
docker compose run web python3 manage.py migrate

# Con --rm para limpiar el contenedor después
docker compose run --rm web python3 manage.py shell

# Sin arrancar los servicios dependientes
docker compose run --no-deps --rm web python3 -c "print('hello')"

# Sin publicar puertos (evita conflictos con el contenedor principal)
docker compose run --rm --no-deps -p 9090:8080 web python3 app.py

# Con service account específica
docker compose run --user nobody web id

# Override del command del servicio
docker compose run --rm web echo "override"
```

### `exec` vs `run`

| | `exec` | `run` |
|---|---|---|
| Contenedor | Usa el existente | Crea uno nuevo |
| Requiere servicio corriendo | Sí | No |
| Publica puertos | No aplica | No (salvo con `-p`) |
| Limpieza | No aplica | Necesita `--rm` |
| Puede especificar comando | No (usa el del servicio) | Sí (override) |
| Crea volumenes efímeros | No | Sí |
| Caso de uso | Shell, debugging | Migraciones, tareas one-shot |

```bash
# exec: entrar al contenedor web corriendo
docker compose exec web bash

# run: ejecutar una migración en un contenedor nuevo y temporal
docker compose run --rm web python3 manage.py migrate
```

## `docker compose build`

Construye (o reconstruye) las imágenes de los servicios con `build:`:

```bash
# Construir todas las imágenes
docker compose build

# Construir un servicio específico
docker compose build web

# Sin caché
docker compose build --no-cache

# Con build args
docker compose build --build-arg APP_VERSION=2.0 web

# Pull imágenes base antes de construir
docker compose build --pull

#build + up
docker compose up --build
```

## `docker compose config`

Valida y muestra el Compose file con todas las variables interpoladas:

```bash
# Mostrar el YAML resuelto
docker compose config

# Solo validar (sin output)
docker compose config --quiet

# Mostrar solo los servicios
docker compose config --services

# Mostrar solo los volúmenes
docker compose config --volumes

# Mostrar networks
docker compose config --networks

# Guardar la configuración resuelta
docker compose config > compose.resolved.yml
```

Muy útil para verificar que la interpolación de variables funciona correctamente.

## `docker compose pull` y `push`

```bash
# Descargar las imágenes más recientes de todos los servicios
docker compose pull

# Pull de un servicio específico
docker compose pull db

# Pull con force para obtener siempre latest
docker compose pull --force

# Push de imágenes construidas a un registry
docker compose push

# Push de un servicio específico
docker compose push web
```

## Otros comandos útiles

### `docker compose top`

Muestra los procesos corriendo en cada servicio:

```bash
docker compose top
# web
#   PID    USER    CMD
#   1234   root    nginx: master process
#   1245   nginx   nginx: worker process

# db
#   PID    USER    CMD
#   2345   postgres   postgres: postgres
```

### `docker compose events`

Muestra eventos del daemon en tiempo real:

```bash
# Ver eventos en vivo
docker compose events

# Output en json
docker compose events --json
# {"type":"container","action":"start","service":"web","container":"myapp-web-1"}
```

### `docker compose cp`

Copiar archivos entre contenedor y host:

```bash
# Del contenedor al host
docker compose cp web:/etc/nginx/nginx.conf ./nginx.conf

# Del host al contenedor
docker compose cp ./config.json web:/app/config.json

# Copiar directorios
docker compose cp -r web:/app/logs ./logs
```

### `docker compose port`

Ver el puerto publicado de un servicio:

```bash
docker compose port web 80
# 0.0.0.0:8080
```

### `docker compose pause` y `unpause`

Pausar y despausar servicios (suspende procesos):

```bash
docker compose pause
# Pausa todos los servicios

docker compose unpause
# Continúa la ejecución
```

### `docker compose kill`

Fuerza la detención de servicios:

```bash
# Matar todos los servicios
docker compose kill

# Matar un servicio específico
docker compose kill web

# Matar con señal específica
docker compose kill -s SIGUSR1 web
```

## Escalar servicios

```bash
# Escalar web a 3 instancias
docker compose up -d --scale web=3

# Con límites
docker compose up -d --scale web=3 --scale worker=2

# En el YAML (Compose v2.5+)
services:
  web:
    image: nginx
    deploy:
      replicas: 2
```

**Limitación**: cuando usas `ports:` con `--scale`, solo el primer contenedor
puede usar el puerto publicado. Los demás deben usar `expose:`.

## Flujo de trabajo típico

```bash
# 1. Desarrollo: levantar el proyecto
docker compose up -d

# 2. Ver logs
docker compose logs -f

# 3. Debugging: shell en un servicio
docker compose exec api bash

# 4. Ejecutar tarea one-shot (migración, seed)
docker compose run --rm api python3 manage.py migrate

# 5. Modificar docker-compose.yml o código
#    ... editar ...

# 6. Aplicar cambios
docker compose up -d --build

# 7. Al terminar
docker compose down      # Preserva volúmenes
# docker compose down -v  # Si quieres empezar desde cero
```

---

## Ejercicios

### Ejercicio 1 — Ciclo de vida completo

```bash
mkdir -p /tmp/compose_lifecycle && cd /tmp/compose_lifecycle

cat > compose.yml << 'EOF'
services:
  web:
    image: nginx:latest
    ports:
      - "8080:80"
  db:
    image: postgres:16-alpine
    environment:
      POSTGRES_PASSWORD: secret
    volumes:
      - pgdata:/var/lib/postgresql/data
volumes:
  pgdata:
EOF

# Crear y arrancar
docker compose up -d
docker compose ps

# Ver logs
docker compose logs --tail 5

# Verificar
curl -s http://localhost:8080 | head -3

# Detener sin destruir
docker compose stop
docker compose ps   # STATUS: exited

# Volver a arrancar (mismos contenedores)
docker compose start
docker compose ps   # STATUS: running

# Destruir (preservando volúmenes)
docker compose down
docker volume ls | grep pgdata   # El volumen sigue ahí

# Destruir incluyendo volúmenes
docker compose down -v
docker volume ls | grep pgdata   # Ya no existe

cd / && rm -rf /tmp/compose_lifecycle
```

### Ejercicio 2 — Detección de cambios en `up`

```bash
mkdir -p /tmp/compose_changes && cd /tmp/compose_changes

cat > compose.yml << 'EOF'
services:
  web:
    image: nginx:latest
    ports:
      - "8080:80"
  db:
    image: alpine:latest
    command: sleep 3600
EOF

docker compose up -d

# Cambiar el puerto del web
sed -i 's/8080/9090/' compose.yml

# up recrea solo web, db no se toca
docker compose up -d
# compose_changes-db-1 is up-to-date
# Recreating compose_changes-web-1 ...

curl -s http://localhost:9090 | head -3

docker compose down
cd / && rm -rf /tmp/compose_changes
```

### Ejercicio 3 — exec vs run

```bash
mkdir -p /tmp/compose_exec && cd /tmp/compose_exec

cat > compose.yml << 'EOF'
services:
  app:
    image: alpine:latest
    command: sleep 3600
EOF

docker compose up -d

# exec: ejecutar en el contenedor existente
docker compose exec app hostname
docker compose exec app cat /etc/os-release | head -2

# run: crear un contenedor nuevo
docker compose run --rm app hostname
# Nombre diferente al de exec — es un contenedor nuevo

# run sin el servicio corriendo
docker compose stop app
docker compose run --rm app echo "Funciona sin servicio corriendo"
# run crea su propio contenedor

docker compose down
cd / && rm -rf /tmp/compose_exec
```

### Ejercicio 4 — docker compose events

```bash
mkdir -p /tmp/compose_events && cd /tmp/compose_events

cat > compose.yml << 'EOF'
services:
  web:
    image: nginx:latest
EOF

# En una terminal, observar eventos
timeout 15 docker compose -f compose.yml events | while read event; do
  echo "$event"
done

# En otra terminal, ejecutar acciones
docker compose -f compose.yml up -d
docker compose -f compose.yml stop
docker compose -f compose.yml start
docker compose -f compose.yml down

cd / && rm -rf /tmp/compose_events
```

### Ejercicio 5 — docker compose top y docker compose logs

```bash
mkdir -p /tmp/compose_top && cd /tmp/compose_top

cat > compose.yml << 'EOF'
services:
  nginx:
    image: nginx:latest
  postgres:
    image: postgres:16-alpine
    environment:
      POSTGRES_PASSWORD: secret
EOF

docker compose up -d

# Ver procesos
docker compose top

# Ver logs con timestamps
docker compose logs -t --tail 10

# Follow con colors
docker compose logs -f --tail 20 &

docker compose down
cd / && rm -rf /tmp/compose_top
```

### Ejercicio 6 — docker compose cp

```bash
mkdir -p /tmp/compose_cp && cd /tmp/compose_cp

cat > compose.yml << 'EOF'
services:
  app:
    image: alpine:latest
    command: sleep 3600
EOF

docker compose up -d

# Crear archivo en el contenedor
docker compose exec app sh -c 'echo "hello from container" > /app/data.txt'

# Copiar del contenedor al host
docker compose cp app:/app/data.txt ./copied.txt
cat ./copied.txt

# Copiar del host al contenedor
echo "hello from host" > ./input.txt
docker compose cp ./input.txt app:/app/received.txt
docker compose exec app cat /app/received.txt

docker compose down
rm -f ./copied.txt ./input.txt
cd / && rm -rf /tmp/compose_cp
```

---

## Resumen de comandos de ciclo de vida

| Comando | Función |
|---|---|
| `up -d` | Crear y arrancar |
| `start` | Arrancar existentes |
| `stop` | Detener sin destruir |
| `down` | Detener y destruir |
| `restart` | Reiniciar |
| `pause` | Pausar |
| `unpause` | Despausar |
| `kill` | Matar forzosamente |
| `create` | Crear sin arrancar |
| `rm` | Eliminar contenedores |
| `exec` | Ejecutar en existente |
| `run` | Ejecutar en nuevo |

## Resumen de opciones comunes

```bash
# up
-d, --detach
--build
--no-build
--force-recreate
--no-recreate
--remove-orphans
--scale SERVICE=NUM
--no-start

# down
-v, --volumes
--rmi all|local
--remove-orphans

# logs
-f, --follow
-t, --timestamps
--tail N
--since TIME
--until TIME

# exec
-d, --detach
-i, --interactive
-T, --no-TTY
--user USER
-e, --env KEY=VAL
-w, --workdir DIR

# run
--rm
--no-deps
--no-start
-p, --publish PORT
--user USER
-e, --env KEY=VAL
```
