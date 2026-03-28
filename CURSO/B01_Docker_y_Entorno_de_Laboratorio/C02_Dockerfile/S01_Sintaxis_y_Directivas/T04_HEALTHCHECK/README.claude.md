# T04 — HEALTHCHECK

## ¿Por qué necesitas healthchecks?

Un contenedor en estado `running` no significa que la aplicación dentro funcione
correctamente. El proceso puede estar corriendo pero:

- Deadlocked (esperando un recurso que nunca llega)
- En un loop infinito sin procesar requests
- Sin conexión a la base de datos
- Sirviendo errores 500 a todas las peticiones

Docker solo sabe si **el proceso está vivo** (PID 1 existe). HEALTHCHECK permite
definir una prueba que verifica si **la aplicación funciona**.

```
Sin HEALTHCHECK:
  docker ps → STATUS: Up 5 minutes          ← ¿Funciona? No se sabe

Con HEALTHCHECK:
  docker ps → STATUS: Up 5 minutes (healthy)  ← Funciona
  docker ps → STATUS: Up 5 minutes (unhealthy) ← Hay un problema
```

## Sintaxis

```dockerfile
HEALTHCHECK [opciones] CMD <comando>
```

```dockerfile
# Healthcheck básico: verificar que el servidor web responde
HEALTHCHECK CMD curl -f http://localhost:8080/health || exit 1

# Con todas las opciones
HEALTHCHECK --interval=30s --timeout=10s --start-period=15s --retries=3 \
  CMD curl -f http://localhost:8080/health || exit 1
```

### Parámetros

| Parámetro | Default | Descripción |
|---|---|---|
| `--interval` | 30s | Cada cuánto se ejecuta el check |
| `--timeout` | 30s | Cuánto esperar antes de considerar el check como fallido |
| `--start-period` | 0s | Tiempo de gracia para que la app arranque |
| `--retries` | 3 | Fallos consecutivos antes de marcar `unhealthy` |
| `--start-interval` | 5s | Intervalo entre checks durante el start-period |

**Importante sobre `--timeout`**: si el comando no termina dentro del timeout,
se considera fallido. Si un healthcheck tarda más que `--interval`, el siguiente
se ejecuta inmediatamente después.

**Sobre `--start-period`**: durante este periodo, los fallos no cuentan para
`--retries`. Esto da tiempo a aplicaciones que tardan en arrancar (bases de datos,
JVMs, etc.).

### Códigos de salida

El comando del healthcheck debe retornar:

| Exit code | Significado |
|---|---|
| 0 | Healthy — la aplicación funciona |
| 1 | Unhealthy — la aplicación falla |
| 2 | Reservado — no usar |

## Estados de salud

```
                  start-period
docker run ─────────────────────▶ Primer check
                                      │
                                      ▼
                                 ┌──────────┐
                                 │ starting  │
                                 └──────────┘
                                      │
                            check OK  │  (retries fallos)
                         ┌────────────┼────────────┐
                         ▼            │             ▼
                   ┌──────────┐      │       ┌────────────┐
                   │ healthy  │      │       │ unhealthy  │
                   └──────────┘      │       └────────────┘
                         │           │             │
                    check falla      │        check OK
                    (retries veces)  │             │
                         │           │             ▼
                         ▼           │       ┌──────────┐
                   ┌────────────┐    │       │ healthy  │
                   │ unhealthy  │    │       └──────────┘
                   └────────────┘    │
                                     │
                    Durante start-period, los fallos no cuentan
```

- **starting**: desde el arranque hasta el primer check exitoso
- **healthy**: el último check (o `retries` consecutivos) fue exitoso
- **unhealthy**: `retries` checks consecutivos fallaron

Un contenedor puede volver de unhealthy a healthy si los checks vuelven a pasar.

## Ejemplos de healthchecks

### Servidor web (HTTP)

```dockerfile
FROM nginx:latest

HEALTHCHECK --interval=15s --timeout=5s --start-period=10s --retries=3 \
  CMD curl -f http://localhost/ || exit 1
```

### Aplicación con endpoint de health dedicado

```dockerfile
FROM python:3.12-slim
# ... setup de la app ...

HEALTHCHECK --interval=30s --timeout=10s --start-period=20s --retries=3 \
  CMD curl -f http://localhost:8080/health || exit 1
```

El endpoint `/health` debería verificar:
- Conexión a la base de datos
- Acceso a servicios dependientes
- Estado interno de la aplicación

### Base de datos (PostgreSQL)

```dockerfile
FROM postgres:16

# La imagen oficial de postgres NO incluye HEALTHCHECK
# Hay que definirlo explícitamente
HEALTHCHECK --interval=10s --timeout=5s --start-period=30s --retries=5 \
  CMD pg_isready -U postgres || exit 1
```

`pg_isready` es una herramienta incluida en la imagen de PostgreSQL que verifica
si el servidor acepta conexiones.

### Redis

```dockerfile
FROM redis:7

HEALTHCHECK --interval=10s --timeout=3s --retries=3 \
  CMD redis-cli ping | grep -q PONG || exit 1
```

### MySQL / MariaDB

```dockerfile
FROM mysql:8

HEALTHCHECK --interval=10s --timeout=5s --start-period=30s --retries=5 \
  CMD mysqladmin ping -h 127.0.0.1 --silent || exit 1
```

### Sin curl ni wget (imagen mínima)

En imágenes mínimas donde curl no está instalado:

```dockerfile
# Usando wget (incluido en Alpine/BusyBox)
HEALTHCHECK CMD wget -qO- http://localhost:8080/health || exit 1

# Usando /dev/tcp (solo funciona en bash, NO en sh/ash de Alpine)
HEALTHCHECK CMD bash -c 'echo > /dev/tcp/localhost/8080' || exit 1

# Usando un binary de healthcheck compilado estáticamente
COPY healthcheck /usr/local/bin/
HEALTHCHECK CMD healthcheck
```

> **Nota sobre `/dev/tcp`**: es un feature de bash, no del kernel. No funciona
> en imágenes Alpine (que usan `ash`/`sh`). Si la imagen no tiene bash, usar
> `wget` o un binario dedicado.

### Con script de healthcheck

```bash
#!/bin/sh
# healthcheck.sh — script de healthcheck personalizado

# Verificar que el proceso está vivo
if ! pgrep -x myapp > /dev/null; then
    echo "Process not running"
    exit 1
fi

# Verificar endpoint de health
if wget -qO- http://localhost:8080/api/health | grep -q '"status":"ok"'; then
    exit 0
fi

echo "Health check failed"
exit 1
```

```dockerfile
COPY healthcheck.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/healthcheck.sh
HEALTHCHECK CMD /usr/local/bin/healthcheck.sh
```

## Observar el healthcheck en acción

```bash
# Ver el estado de salud en docker ps
docker ps
# CONTAINER ID  IMAGE   STATUS                    PORTS
# abc123        myapp   Up 2 minutes (healthy)    8080/tcp
# def456        myapp   Up 5 minutes (unhealthy)  8080/tcp

# Ver detalles del healthcheck
docker inspect --format '{{json .State.Health}}' mycontainer | python3 -m json.tool
```

Salida de inspección:

```json
{
    "Status": "healthy",
    "FailingStreak": 0,
    "Log": [
        {
            "Start": "2024-01-15T10:30:00.000Z",
            "End": "2024-01-15T10:30:00.500Z",
            "ExitCode": 0,
            "Output": "..."
        }
    ]
}
```

El campo `Log` mantiene los últimos 5 resultados de healthchecks.

### docker events

```bash
# Observar eventos de healthcheck en tiempo real
docker events --filter 'event=health_status'

# En otra terminal, verás eventos como:
# container health_status: healthy mycontainer
# container health_status: unhealthy mycontainer
```

## HEALTHCHECK NONE

Deshabilita el healthcheck (útil si heredas una imagen que lo define):

```dockerfile
FROM some-image-with-healthcheck
HEALTHCHECK NONE
```

También se puede deshabilitar en runtime:

```bash
docker run --no-healthcheck some-image
```

## Healthcheck y el proceso CMD

El healthcheck se ejecuta **independientemente** del CMD del contenedor:

```
Contenedor arrancado con: CMD ["python3", "app.py"]

Healthcheck separado:
  HEALTHCHECK CMD curl -f http://localhost:8080/health

Docker ejecuta ambos en paralelo:
  - CMD: el proceso principal (PID 1)
  - HEALTHCHECK CMD: verificación periódica (proceso separado)
```

El healthcheck **no es** un subproceso del CMD. Es un proceso separado que Docker
ejecuta periódicamente.

## Healthcheck y restart policies

**Docker no reinicia automáticamente un contenedor `unhealthy`**. El healthcheck
solo cambia el estado reportado:

```bash
# Restart policies reaccionan a la MUERTE del proceso, no al healthcheck
docker run -d --restart on-failure myapp   # Reinicia si PID 1 sale con error
docker run -d --restart unless-stopped myapp  # Reinicia siempre (excepto stop manual)
```

Un contenedor unhealthy cuyo proceso sigue vivo **no se reinicia**. El
healthcheck solo informa del estado — la acción debe tomarla otro sistema
(orquestador, script de monitoreo, etc.).

### Docker Compose con `depends_on` + healthcheck

Este es uno de los usos más importantes de healthchecks: **coordinar el orden de
arranque** de servicios dependientes:

```yaml
services:
  db:
    image: postgres:16
    environment:
      POSTGRES_PASSWORD: secret
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 10s
      timeout: 5s
      retries: 5
      start_period: 30s

  api:
    image: myapi
    depends_on:
      db:
        condition: service_healthy
    # La API no arranca hasta que la DB sea healthy
```

Sin `condition: service_healthy`, `depends_on` solo espera a que el contenedor
**arranque**, no a que esté listo. Con healthcheck, espera a que realmente
funcione.

## Healthcheck en runtime (sin Dockerfile)

Se puede definir o sobrescribir el healthcheck al ejecutar el contenedor:

```bash
docker run -d \
  --health-cmd "curl -f http://localhost/ || exit 1" \
  --health-interval 15s \
  --health-timeout 5s \
  --health-start-period 10s \
  --health-retries 3 \
  nginx
```

## Buenas prácticas

| Práctica | Razón |
|---|---|
| Usar un endpoint `/health` dedicado | Separa la lógica de salud de la lógica de negocio |
| `start-period` suficiente | Evita falsos unhealthy durante el arranque |
| No checks demasiado frecuentes | Cada check consume CPU/memoria |
| Checks ligeros (no queries pesadas) | El healthcheck no debe degradar el servicio |
| Verificar dependencias críticas | DB, cache, filesystem — no solo "proceso vivo" |
| `--timeout` menor que `--interval` | Evita que los checks se acumulen |
| Evitar instalar herramientas solo para health | Aumenta la superficie de ataque; usar las que ya tiene la imagen |

---

## Práctica

> **Nota**: este tópico tiene un directorio `labs/` con archivos preparados
> para ejercicios más detallados.

### Ejercicio 1 — Observar la transición starting → healthy

```bash
cat > /tmp/Dockerfile.health << 'EOF'
FROM nginx:latest
HEALTHCHECK --interval=5s --timeout=3s --start-period=3s --retries=2 \
  CMD curl -f http://localhost/ || exit 1
EOF

docker build -t health-test -f /tmp/Dockerfile.health /tmp

docker run -d --name hc_test health-test

# Observar la transición (ejecutar varias veces rápidamente)
docker ps --format "{{.Names}}: {{.Status}}"
# hc_test: Up 2 seconds (health: starting)
sleep 10
docker ps --format "{{.Names}}: {{.Status}}"
# hc_test: Up 12 seconds (healthy)

# Ver historial de checks
docker inspect --format '{{json .State.Health.Log}}' hc_test | python3 -m json.tool

docker rm -f hc_test
docker rmi health-test
rm /tmp/Dockerfile.health
```

**Predicción**: inmediatamente después de arrancar, el estado es "starting".
Tras el `start-period` (3s) + un check exitoso, cambia a "healthy". El Log
muestra entradas con `ExitCode: 0` y el output de curl.

### Ejercicio 2 — Simular unhealthy (sin matar el proceso)

```bash
cat > /tmp/Dockerfile.unhealthy << 'EOF'
FROM nginx:latest
HEALTHCHECK --interval=5s --timeout=3s --retries=2 \
  CMD curl -f http://localhost/ || exit 1
EOF

docker build -t unhealthy-test -f /tmp/Dockerfile.unhealthy /tmp
docker run -d --name hc_fail unhealthy-test

# Esperar a que sea healthy
sleep 15
docker ps --format "{{.Names}}: {{.Status}}"
# hc_fail: Up 15 seconds (healthy)

# Romper la respuesta HTTP SIN matar nginx
# (borrar el archivo que sirve — nginx sigue corriendo pero responde 403)
docker exec hc_fail rm /usr/share/nginx/html/index.html

# Esperar a que se detecte el fallo (retries=2, interval=5s → ~15s)
sleep 15
docker ps --format "{{.Names}}: {{.Status}}"
# hc_fail: Up ... (unhealthy)

# nginx sigue corriendo — solo devuelve 403 en lugar de 200
docker exec hc_fail nginx -t
# nginx: configuration file ... test is successful

# Ver el log de healthcheck
docker inspect --format '{{json .State.Health}}' hc_fail | python3 -m json.tool

docker rm -f hc_fail
docker rmi unhealthy-test
rm /tmp/Dockerfile.unhealthy
```

**Predicción**: al borrar `index.html`, nginx responde 403 (Forbidden).
`curl -f` falla con códigos >= 400. Tras `retries` (2) fallos consecutivos,
el estado cambia a "unhealthy". El proceso nginx sigue vivo — el contenedor
NO sale. Esto demuestra que unhealthy ≠ proceso muerto.

### Ejercicio 3 — Healthcheck en runtime (sin Dockerfile)

```bash
# Definir healthcheck directamente con docker run
docker run -d --name hc_runtime \
  --health-cmd "wget -qO- http://localhost/ || exit 1" \
  --health-interval 5s \
  --health-timeout 3s \
  --health-retries 2 \
  nginx

sleep 15
docker ps --format "{{.Names}}: {{.Status}}"
# hc_runtime: Up ... (healthy)

# Ver detalles
docker inspect --format '{{json .State.Health.Status}}' hc_runtime
# "healthy"

docker rm -f hc_runtime
```

**Predicción**: los flags `--health-*` definen el healthcheck sin necesidad de
un Dockerfile personalizado. Útil para imágenes que no traen healthcheck
(como la oficial de nginx).

### Ejercicio 4 — Docker Compose con depends_on + healthcheck

```bash
mkdir -p /tmp/docker_compose_hc

cat > /tmp/docker_compose_hc/compose.yaml << 'EOF'
services:
  db:
    image: postgres:16-alpine
    environment:
      POSTGRES_PASSWORD: secret
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 5s
      timeout: 3s
      retries: 5
      start_period: 10s

  app:
    image: postgres:16-alpine
    depends_on:
      db:
        condition: service_healthy
    entrypoint: ["sh", "-c"]
    command: ["echo 'DB is healthy! App starting...' && pg_isready -h db -U postgres && echo 'Connected!'"]
EOF

cd /tmp/docker_compose_hc
docker compose up
# Observar:
# 1. db arranca primero
# 2. app espera hasta que db sea "healthy"
# 3. app ejecuta pg_isready contra db y confirma conexión

docker compose down
cd -
rm -rf /tmp/docker_compose_hc
```

**Predicción**: sin `condition: service_healthy`, app arrancaría inmediatamente
(y probablemente fallaría al conectar a la DB). Con el healthcheck, Compose
espera a que `pg_isready` retorne 0 en el contenedor `db` antes de arrancar
`app`. Esto resuelve el problema clásico de "la app arranca antes que la DB".

### Ejercicio 5 — Docker no reinicia contenedores unhealthy

```bash
cat > /tmp/Dockerfile.norestart << 'EOF'
FROM nginx:latest
HEALTHCHECK --interval=3s --timeout=2s --retries=2 \
  CMD curl -f http://localhost/ || exit 1
EOF

docker build -t norestart-test -f /tmp/Dockerfile.norestart /tmp

# Crear con restart=no (por defecto)
docker run -d --name norestart_test norestart-test

# Esperar a que sea healthy
sleep 10
docker ps --format "{{.Names}}: {{.Status}}"

# Hacer que falle el healthcheck
docker exec norestart_test rm /usr/share/nginx/html/index.html

# Esperar a unhealthy
sleep 10
docker ps --format "{{.Names}}: {{.Status}}"
# norestart_test: Up ... (unhealthy)

# El contenedor sigue "Up" — Docker NO lo reinicia
# Para verificar, contar cuántos inicios ha habido
docker inspect --format '{{.RestartCount}}' norestart_test
# 0 — nunca se reinició

docker rm -f norestart_test
docker rmi norestart-test
rm /tmp/Dockerfile.norestart
```

**Predicción**: a pesar de estar "unhealthy", el contenedor sigue corriendo.
Docker no toma acción — el healthcheck solo reporta estado. RestartCount
permanece en 0. Para que Docker reinicie, necesitarías un orquestador (Swarm,
Kubernetes) o un script externo que monitoree el estado.

### Ejercicio 6 — HEALTHCHECK NONE y --no-healthcheck

```bash
# Crear imagen con healthcheck
cat > /tmp/Dockerfile.withhc << 'EOF'
FROM nginx:latest
HEALTHCHECK --interval=5s CMD curl -f http://localhost/ || exit 1
EOF

docker build -t with-hc -f /tmp/Dockerfile.withhc /tmp

# 1. Con healthcheck activo
docker run -d --name hc_active with-hc
sleep 10
docker ps --format "{{.Names}}: {{.Status}}"
# hc_active: Up ... (healthy)

# 2. Deshabilitado con --no-healthcheck
docker run -d --name hc_disabled --no-healthcheck with-hc
sleep 10
docker ps --format "{{.Names}}: {{.Status}}"
# hc_disabled: Up ...  (sin "(healthy)" ni "(unhealthy)")

docker rm -f hc_active hc_disabled
docker rmi with-hc
rm /tmp/Dockerfile.withhc
```

**Predicción**: con healthcheck activo, el status muestra "(healthy)". Con
`--no-healthcheck`, el status solo muestra "Up" sin estado de salud — como si
HEALTHCHECK no existiera. Útil cuando quieres desactivar un healthcheck
heredado de la imagen base.

### Ejercicio 7 — Inspeccionar el Log de healthcheck

```bash
cat > /tmp/Dockerfile.hclog << 'EOF'
FROM nginx:latest
HEALTHCHECK --interval=3s --timeout=2s --retries=2 \
  CMD curl -f http://localhost/ || exit 1
EOF

docker build -t hclog-test -f /tmp/Dockerfile.hclog /tmp
docker run -d --name hc_log hclog-test

# Esperar a que haya varios checks
sleep 20

# Ver el estado general
docker inspect --format '{{.State.Health.Status}}' hc_log
# healthy

# Ver los últimos checks (Docker guarda los últimos 5)
docker inspect --format '{{json .State.Health.Log}}' hc_log | python3 -m json.tool

# Observar los campos:
# - Start/End: cuándo se ejecutó
# - ExitCode: 0=healthy, 1=unhealthy
# - Output: lo que imprimió el comando

# Monitorear eventos en tiempo real (Ctrl+C para salir)
# docker events --filter 'event=health_status' --filter "container=hc_log"

docker rm -f hc_log
docker rmi hclog-test
rm /tmp/Dockerfile.hclog
```

**Predicción**: `Health.Log` contiene un array con los últimos 5 checks. Cada
entrada tiene Start, End, ExitCode y Output. Todos deberían mostrar
`ExitCode: 0` porque nginx funciona correctamente.

### Ejercicio 8 — Healthcheck con start-period para aplicación lenta

```bash
cat > /tmp/Dockerfile.slow << 'EOF'
FROM alpine:latest

# Simular una app que tarda 10s en arrancar
CMD ["sh", "-c", "echo 'Starting slow app...' && sleep 10 && echo 'ready' > /tmp/ready && echo 'App ready!' && tail -f /dev/null"]

# Sin start-period: se marcaría unhealthy antes de arrancar
# Con start-period=15s: los fallos durante los primeros 15s no cuentan
HEALTHCHECK --interval=3s --timeout=2s --start-period=15s --retries=2 \
  CMD test -f /tmp/ready || exit 1
EOF

docker build -t slow-test -f /tmp/Dockerfile.slow /tmp
docker run -d --name hc_slow slow-test

# Inmediatamente: starting (la app aún no creó /tmp/ready)
docker ps --format "{{.Names}}: {{.Status}}"
# hc_slow: Up 2 seconds (health: starting)

# Durante los primeros 10s: checks fallan pero no cuenta (start-period)
sleep 5
docker ps --format "{{.Names}}: {{.Status}}"
# hc_slow: Up 7 seconds (health: starting)

# Después de ~15s: la app creó /tmp/ready, check pasa
sleep 15
docker ps --format "{{.Names}}: {{.Status}}"
# hc_slow: Up 22 seconds (healthy)

docker rm -f hc_slow
docker rmi slow-test
rm /tmp/Dockerfile.slow
```

**Predicción**: sin `start-period`, el contenedor se marcaría unhealthy en los
primeros ~10 segundos (antes de que la app cree `/tmp/ready`). Con
`start-period=15s`, los fallos durante ese periodo no cuentan para `retries`.
La app tiene tiempo de arrancar, y cuando finalmente pasa el check, cambia a
"healthy".

---

## Resumen de comandos

```bash
# Dockerfile
HEALTHCHECK [--interval=30s] [--timeout=30s] [--start-period=0s] [--retries=3] CMD comando
HEALTHCHECK NONE  # Deshabilitar

# Runtime
--health-cmd "comando"
--health-interval 30s
--health-timeout 30s
--health-start-period 0s
--health-retries 3
--no-healthcheck    # Deshabilitar

# Inspección
docker inspect --format '{{json .State.Health}}' container
docker events --filter 'event=health_status'
docker ps --format "{{.Names}}: {{.Status}}"
```
