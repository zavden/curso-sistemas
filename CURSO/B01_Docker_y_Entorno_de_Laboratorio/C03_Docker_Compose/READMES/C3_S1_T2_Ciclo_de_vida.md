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
created    : Contenedor existe pero no ha iniciado
running    : Contenedor activo y ejecutándose
paused     : Procesos suspendidos (SIGSTOP), reanudable con unpause
exited     : Contenedor detenido (por stop, kill, o error)
restarting : En proceso de reinicio (por restart policy)
dead       : Estado de error (limpiar con docker system prune)
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

# Crear sin arrancar
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
# Escalar servicios
docker compose up -d --scale web=3 --scale worker=2
```

**Limitación de puertos con scaling**: si el servicio publica un puerto fijo
(`"8080:80"`), solo la primera instancia puede usarlo. Las demás fallan por
conflicto. Soluciones:

- Usar puerto aleatorio: `"80"` (sin host port)
- Usar un load balancer (nginx, traefik) delante
- No publicar puertos y acceder por la red interna

## `docker compose create`

Crea contenedores sin arrancarlos (útil para preparar antes de un evento):

```bash
docker compose create
docker compose create --no-recreate  # No recrear existentes
docker compose create web db         # Solo servicios específicos
```

Luego se pueden iniciar con `start`.

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
# NAME                STATUS         CONFIG FILES
# myapp               running(2)     /path/to/compose.yml
# another_project     running(1)     /path/to/another/compose.yml
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

# Filtrar por tiempo
docker compose logs --since 30m
docker compose logs --since "2024-01-15T10:00:00"
docker compose logs --until "2024-01-15T11:00:00"
```

Los colores son asignados automáticamente: cada servicio tiene un color distinto
para distinguirlos visualmente.

## `docker compose exec`

Ejecuta un comando **adicional** en un contenedor **que ya está corriendo**:

```bash
# Shell interactiva en el servicio web
docker compose exec web bash

# Comando específico
docker compose exec db psql -U postgres

# Como usuario específico
docker compose exec --user root web sh

# Sin TTY (para scripts)
docker compose exec -T web cat /etc/nginx/nginx.conf

# Con variable de entorno
docker compose exec -e PGUSER=admin db psql

# Directorio de trabajo
docker compose exec -w /app web pwd
```

`exec` ejecuta un proceso adicional dentro del contenedor existente — el proceso
principal del servicio sigue corriendo.

## `docker compose run`

Crea un **contenedor nuevo y efímero** para ejecutar un comando one-shot:

```bash
# Ejecutar un comando en un contenedor nuevo del servicio web
docker compose run web python3 manage.py migrate

# Con --rm para limpiar el contenedor después
docker compose run --rm web python3 manage.py shell

# Sin arrancar los servicios dependientes
docker compose run --no-deps --rm web python3 -c "print('hello')"

# Con puerto explícito (run no publica puertos del servicio por defecto)
docker compose run --rm -p 9090:8080 web python3 app.py

# Override del command del servicio
docker compose run --rm web echo "override"
```

### `exec` vs `run`

| | `exec` | `run` |
|---|---|---|
| Contenedor | Usa el existente | Crea uno nuevo |
| Requiere servicio corriendo | Sí | No |
| Publica puertos del servicio | No aplica (ya publicados) | No (salvo con `-p`) |
| Limpieza | No aplica | Necesita `--rm` |
| Proceso principal | Sigue corriendo | El comando ES el proceso |
| Caso de uso | Shell, debugging, consultas | Migraciones, seeds, tareas one-shot |

```bash
# exec: abrir shell en el contenedor web que ya corre
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

# Build + up combinado
docker compose up --build -d
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

# Guardar la configuración resuelta
docker compose config > compose.resolved.yml
```

## `docker compose pull`

```bash
# Descargar las imágenes más recientes de todos los servicios
docker compose pull

# Pull de un servicio específico
docker compose pull db

# Pull con dependencias incluidas
docker compose pull --include-deps
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
#
# db
#   PID    USER    CMD
#   2345   postgres   postgres: postgres
```

### `docker compose events`

Muestra eventos del daemon en tiempo real:

```bash
# Ver eventos en vivo (Ctrl+C para salir)
docker compose events

# Output en JSON
docker compose events --json
```

### `docker compose cp`

Copiar archivos entre contenedor y host:

```bash
# Del contenedor al host
docker compose cp web:/etc/nginx/nginx.conf ./nginx.conf

# Del host al contenedor
docker compose cp ./config.json web:/app/config.json
```

### `docker compose port`

Ver el puerto publicado de un servicio:

```bash
docker compose port web 80
# 0.0.0.0:8080
```

### `docker compose pause` y `unpause`

Pausar y despausar servicios (suspende procesos con SIGSTOP):

```bash
docker compose pause       # Pausa todos los servicios
docker compose unpause     # Continúa la ejecución
docker compose pause web   # Solo un servicio
```

### `docker compose kill`

Fuerza la detención de servicios (envía SIGKILL por defecto):

```bash
docker compose kill           # Matar todos
docker compose kill web       # Matar un servicio
docker compose kill -s SIGUSR1 web   # Señal específica
```

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

# 5. Modificar compose.yml o código
#    ... editar ...

# 6. Aplicar cambios
docker compose up -d --build

# 7. Al terminar
docker compose down      # Preserva volúmenes
# docker compose down -v  # Si quieres empezar desde cero
```

---

**Nota**: el directorio `labs/` contiene un laboratorio con 4 partes y
archivos Compose listos para usar. Consultar `LABS.md` para la guía.

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
```

**Predicción**: `docker compose ps` muestra dos contenedores: `web` (nginx) y
`db` (postgres), ambos con STATUS "running".

```bash
# Ver logs
docker compose logs --tail 5

# Verificar nginx
curl -s http://localhost:8080 | head -3

# Detener sin destruir
docker compose stop
docker compose ps   # STATUS: exited — contenedores existen pero detenidos

# Volver a arrancar (mismos contenedores)
docker compose start
docker compose ps   # STATUS: running — mismos contenedores, misma data
```

**Predicción**: tras `stop` + `start`, los contenedores son los mismos (mismo
ID). No se pierde estado.

```bash
# Destruir (preservando volúmenes)
docker compose down
docker volume ls | grep pgdata   # El volumen sigue ahí

# Destruir incluyendo volúmenes
docker compose up -d && docker compose down -v
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

# Anotar los IDs de los contenedores
docker compose ps --format "{{.Name}}: {{.ID}}"
```

**Predicción**: al cambiar solo el puerto de `web`, `up -d` recrea `web` (nuevo
ID) pero deja `db` intacto (mismo ID).

```bash
# Cambiar el puerto del web
sed -i 's/8080/9090/' compose.yml

# up recrea solo web, db no se toca
docker compose up -d

# Verificar IDs
docker compose ps --format "{{.Name}}: {{.ID}}"
# web tiene ID nuevo, db tiene el mismo ID

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
```

**Predicción**: `exec` muestra el hostname del contenedor corriendo. `run`
crea un contenedor nuevo con hostname diferente.

```bash
# run: crear un contenedor nuevo
docker compose run --rm app hostname
# Hostname diferente — es un contenedor nuevo

# run funciona sin el servicio corriendo
docker compose stop app
docker compose exec app echo "test" 2>&1 || echo "exec falla sin servicio corriendo"
docker compose run --rm app echo "run funciona sin servicio corriendo"

docker compose down
cd / && rm -rf /tmp/compose_exec
```

### Ejercicio 4 — pause vs stop

```bash
mkdir -p /tmp/compose_pause && cd /tmp/compose_pause

cat > compose.yml << 'EOF'
services:
  counter:
    image: alpine:latest
    command: ["sh", "-c", "i=0; while true; do echo \"count: $$i\"; i=$$((i+1)); sleep 1; done"]
EOF

docker compose up -d
sleep 3
docker compose logs --tail 3 counter
# count: 0, 1, 2 ...

# Pausar
docker compose pause counter
docker compose ps
# STATUS: paused
sleep 3

# Despausar
docker compose unpause counter
sleep 2
docker compose logs --tail 5 counter
```

**Predicción**: durante la pausa, el contador se detiene. Al despausar,
continúa desde donde quedó (no desde 0). `pause` suspende el proceso
(SIGSTOP) sin destruirlo. A diferencia de `stop`, el contenedor sigue en
memoria — la reanudación es instantánea.

```bash
docker compose down
cd / && rm -rf /tmp/compose_pause
```

### Ejercicio 5 — docker compose top y logs con filtros

```bash
mkdir -p /tmp/compose_top && cd /tmp/compose_top

cat > compose.yml << 'EOF'
services:
  nginx:
    image: nginx:latest
    ports:
      - "8080:80"
  postgres:
    image: postgres:16-alpine
    environment:
      POSTGRES_PASSWORD: secret
EOF

docker compose up -d
sleep 3

# Ver procesos de cada servicio
docker compose top
```

**Predicción**: `top` muestra los procesos reales. nginx tiene un master
process (root) y worker processes (nginx). Postgres muestra el proceso
principal y sus workers internos.

```bash
# Logs con timestamps
docker compose logs -t --tail 5

# Logs solo de un servicio
docker compose logs --tail 10 postgres

# Generar actividad en nginx
curl -s http://localhost:8080 > /dev/null
curl -s http://localhost:8080 > /dev/null

# Ver logs recientes
docker compose logs --since 10s nginx

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

# Crear archivo en el contenedor (mkdir -p porque /tmp ya existe en Alpine)
docker compose exec app sh -c 'echo "hello from container" > /tmp/data.txt'

# Copiar del contenedor al host
docker compose cp app:/tmp/data.txt ./copied.txt
cat ./copied.txt
# hello from container
```

**Predicción**: `cp` copia archivos entre host y contenedor, como `docker cp`
pero usando nombres de servicio en lugar de nombres de contenedor.

```bash
# Copiar del host al contenedor
echo "hello from host" > ./input.txt
docker compose cp ./input.txt app:/tmp/received.txt
docker compose exec app cat /tmp/received.txt
# hello from host

docker compose down
cd / && rm -rf /tmp/compose_cp
```

### Ejercicio 7 — down con --rmi

```bash
mkdir -p /tmp/compose_rmi && cd /tmp/compose_rmi

cat > app.sh << 'EOF'
#!/bin/sh
echo "Custom app running"
sleep 3600
EOF

cat > Dockerfile << 'EOF'
FROM alpine:latest
COPY app.sh /app.sh
RUN chmod +x /app.sh
CMD ["/app.sh"]
EOF

cat > compose.yml << 'EOF'
services:
  custom:
    build: .
    image: my-custom-app:latest
  stock:
    image: nginx:latest
EOF

docker compose up -d --build
docker images | grep -E "(my-custom|nginx)"
```

**Predicción**: `down --rmi local` elimina solo la imagen construida localmente
(`my-custom-app`). `down --rmi all` eliminaría también la imagen de nginx.

```bash
docker compose down --rmi local
docker images | grep my-custom-app || echo "Imagen custom eliminada (correcto)"
docker images | grep nginx | head -1 && echo "nginx sigue (correcto)"

cd / && rm -rf /tmp/compose_rmi
```

### Ejercicio 8 — Flujo de trabajo de desarrollo

```bash
mkdir -p /tmp/compose_workflow && cd /tmp/compose_workflow

cat > index.html << 'EOF'
<h1>Version 1</h1>
EOF

cat > compose.yml << 'EOF'
services:
  web:
    image: nginx:latest
    ports:
      - "8080:80"
    volumes:
      - ./index.html:/usr/share/nginx/html/index.html:ro
EOF

# 1. Levantar
docker compose up -d

# 2. Verificar
curl -s http://localhost:8080
# <h1>Version 1</h1>

# 3. Modificar código (con bind mount, el cambio es inmediato)
echo '<h1>Version 2</h1>' > index.html
curl -s http://localhost:8080
```

**Predicción**: con bind mount, los cambios en `index.html` se reflejan
inmediatamente sin rebuild ni restart. Esto es la ventaja de bind mounts
para desarrollo.

```bash
# 4. Si cambiamos la configuración de Compose (ej: agregar un servicio)
cat >> compose.yml << 'EXTRA'
  api:
    image: alpine:latest
    command: sleep 3600
EXTRA

docker compose up -d
# web is up-to-date — no se recrea
# Creating compose_workflow-api-1

docker compose ps
# web y api corriendo

# 5. Al terminar
docker compose down
cd / && rm -rf /tmp/compose_workflow
```

---

## Resumen de comandos de ciclo de vida

| Comando | Función |
|---|---|
| `up -d` | Crear y arrancar |
| `start` | Arrancar existentes |
| `stop` | Detener sin destruir |
| `down` | Detener y destruir |
| `restart` | Reiniciar (stop+start) |
| `pause` | Pausar (SIGSTOP) |
| `unpause` | Despausar |
| `kill` | Matar forzosamente |
| `create` | Crear sin arrancar |
| `exec` | Comando adicional en existente |
| `run` | Comando en contenedor nuevo |
| `cp` | Copiar archivos host ↔ contenedor |
| `top` | Ver procesos |
| `logs` | Ver logs |

## Resumen de opciones comunes

```bash
# up
-d, --detach
--build
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
-T, --no-TTY
--user USER
-e, --env KEY=VAL
-w, --workdir DIR

# run
--rm
--no-deps
-p, --publish PORT
--user USER
-e, --env KEY=VAL
```
