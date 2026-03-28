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
| `--start-interval` | 5s | Intervalo entre checks durante el start-period (BuildKit) |

**Importante sobre `--timeout`**: Si el comando no termina dentro del timeout,
se considera fallido. Si un healthcheck tarda más que `--interval`, el siguiente
se ejecuta inmediatamente después.

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
                                 │ starting │
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

HEALTHCHECK --interval=10s --timeout=5s --start-period=30s --retries=5 \
  CMD pg_isready -U postgres || exit 1
```

`pg_isready` es una herramienta incluida en la imagen de PostgreSQL que verifica
si el servidor acepta conexiones.

### MySQL / MariaDB

```dockerfile
FROM mysql:8

HEALTHCHECK --interval=10s --timeout=5s --start-period=30s --retries=5 \
  CMD mysqladmin ping -h localhost -u root -p${MYSQL_ROOT_PASSWORD} || exit 1
```

### MongoDB

```dockerfile
FROM mongo:7

HEALTHCHECK --interval=10s --timeout=5s --start-period=15s --retries=3 \
  CMD mongosh --eval "db.adminCommand('ping')" || exit 1
```

### Redis

```dockerfile
FROM redis:7

HEALTHCHECK --interval=10s --timeout=3s --retries=3 \
  CMD redis-cli ping | grep -q PONG || exit 1
```

### Sin curl ni wget (imagen mínima)

En imágenes mínimas donde curl no está instalado, alternativas:

```dockerfile
# Usando wget (incluido en Alpine/BusyBox)
HEALTHCHECK CMD wget -qO- http://localhost:8080/health || exit 1

# Usando /dev/tcp (bash built-in, sin herramientas externas)
HEALTHCHECK CMD bash -c 'echo > /dev/tcp/localhost/8080' || exit 1

# Usando un binary estático compilado para el healthcheck
COPY healthcheck /usr/local/bin/
HEALTHCHECK CMD healthcheck
```

### Con script de healthcheck

```bash
#!/bin/sh
# healthcheck.sh — script de healthcheck personalizado

# Verificar que el proceso está vivo
if ! pgrep -x myapp > /dev/null; then
    echo "Process not running"
    exit 1
fi

# Verificar que puede escribir al filesystem
if ! touch /app/healthcheck.tmp 2>/dev/null; then
    echo "Cannot write to /app"
    exit 1
fi
rm /app/healthcheck.tmp

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
            "Output": "HTTP/1.1 200 OK\n..."
        },
        {
            "Start": "2024-01-15T10:30:30.000Z",
            "End": "2024-01-15T10:30:30.450Z",
            "ExitCode": 0,
            "Output": "HTTP/1.1 200 OK\n..."
        }
    ]
}
```

El campo `Log` mantiene los últimos 5 resultados de healthchecks.

### docker events

```bash
# Observar eventos de healthcheck
docker events --filter 'event=health_status'

# En otra terminal, hacer que un contenedor se vuelva unhealthy
# Verás eventos como:
# 2024-01-15T10:35:00.000000000Z container health_status: healthy mycontainer
# 2024-01-15T10:40:00.000000000Z container health_status: unhealthy mycontainer
```

## HEALTHCHECK NONE

Deshabilita el healthcheck heredado de la imagen base:

```dockerfile
FROM postgres:16
# postgres tiene un healthcheck definido

# Deshabilitarlo
HEALTHCHECK NONE
```

También se puede deshabilitar en runtime:

```bash
docker run --no-healthcheck postgres:16
```

## Healthcheck y restart policies

**Docker no reinicia automáticamente un contenedor `unhealthy`**. El healthcheck
solo cambia el estado reportado. Para que Docker actúe, necesitas:

### Restart policy

```bash
# Reiniciar si el proceso muere (no si es unhealthy)
docker run -d --restart on-failure myapp

# Reiniciar siempre (incluyendo reboot del host)
docker run -d --restart unless-stopped myapp
```

Las restart policies reaccionan a la **muerte del proceso** (exit), no al estado
de salud. Un contenedor unhealthy cuyo proceso sigue vivo no se reinicia.

### Docker Compose con `depends_on` + healthcheck

```yaml
services:
  db:
    image: postgres:16
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

Este es uno de los usos más importantes de healthchecks: **coordinar el orden de
arranque** de servicios dependientes en Docker Compose.

### Docker Swarm y healthchecks

En Swarm mode, healthchecks afectan el scheduling de servicios:

```yaml
version: "3.8"
services:
  api:
    image: myapi
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8080/health"]
      interval: 30s
      timeout: 10s
      retries: 3
    deploy:
      replicas: 3
    # Si un contenedor es unhealthy, Swarm no le envía tráfico
    # Y puede reiniciar el contenedor si usa --restart-condition
```

```bash
# Crear servicio con healthcheck en Swarm
docker service create \
  --health-cmd "curl -f http://localhost/ || exit 1" \
  --health-interval 15s \
  --health-retries 3 \
  --name myapi \
  myapi:1.0

# Ver estado de salud del servicio
docker service ps myapi
```

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

### Sobrescribir healthcheck de la imagen

```bash
# La imagen tiene un healthcheck, pero quieres uno diferente
docker run -d \
  --health-cmd "wget -qO- http://localhost:8080/health || exit 1" \
  mycustomimage
```

## Healthcheck y el comando CMD

El healthcheck se ejecuta **independientemente** del CMD del contenedor:

```
Contenedor arrancado con: CMD ["python3", "app.py"]

Healthcheck separado:
  HEALTHCHECK CMD curl -f http://localhost:8080/health

Docker ejecuta ambos:
  - CMD: el proceso principal
  - HEALTHCHECK CMD: verificación periódica (en un proceso separado)
```

El healthcheck **no es** un subproceso del CMD. Es un proceso separado que Docker
gestiona internamente.

## Buenas prácticas

| Práctica | Razón |
|---|---|
| Usar un endpoint `/health` dedicado | Separa la lógica de salud de la lógica de negocio |
| `start-period` suficiente | Evita falsos unhealthy durante el arranque |
| No checks demasiado frecuentes | Cada check consume CPU/memoria |
| Checks ligeros (no queries pesadas) | El healthcheck no debe degradar el servicio |
| Verificar dependencias críticas | DB, cache, filesystem, no solo "proceso vivo" |
| Evitar instalar herramientas solo para el health | Aumenta la superficie de ataque |
| `--timeout` menor que `--interval` | Evita que los checks se acumulen |

## Casos borde

### Healthcheck que tarda más que el interval

Si `--timeout=30s` pero el healthcheck tarda 45s, Docker:
1. Espera a que termine (máximo hasta el timeout)
2. Lo marca como fallido
3. Programa el siguiente check inmediatamente

### Healthcheck durante el start-period

Durante el `start-period`:
- Los checks se ejecutan según `--start-interval` (no `--interval`)
- Los fallos **no cuentan** para `--retries`
- No se marca como unhealthy hasta que termine el start-period

###-healthcheck con exit code 2

El exit code 2 está reservado. Usarlo produce comportamiento indefinido:

```dockerfile
# MAL: exit code 2
HEALTHCHECK CMD some_command || exit 2

# BIEN: solo 0 (healthy) o 1 (unhealthy)
HEALTHCHECK CMD some_command || exit 1
```

---

## Ejercicios

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

### Ejercicio 2 — Simular unhealthy

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
docker ps --format "{{{{.Names}}}}: {{{{.Status}}}}"
# hc_fail: Up 15 seconds (healthy)

# Romper nginx (borrar la configuración)
docker exec hc_fail sh -c 'nginx -s stop'

# Esperar a que se detecte el fallo
sleep 15
docker ps --format "{{.Names}}: {{.Status}}"
# hc_fail: Up 30 seconds (unhealthy)

docker rm -f hc_fail
docker rmi unhealthy-test
rm /tmp/Dockerfile.unhealthy
```

### Ejercicio 3 — Healthcheck en runtime

```bash
# Definir healthcheck sin Dockerfile
docker run -d --name hc_runtime \
  --health-cmd "wget -qO- http://localhost/ || exit 1" \
  --health-interval 5s \
  --health-timeout 3s \
  --health-retries 2 \
  nginx

sleep 15
docker ps --format "{{.Names}}: {{.Status}}"
# hc_runtime: Up ... (healthy)

docker rm -f hc_runtime
```

### Ejercicio 4 — Docker Compose con depends_on

```bash
mkdir -p /tmp/docker_compose_hc

cat > /tmp/docker_compose_hc/docker-compose.yaml << 'EOF'
version: "3.8"
services:
  db:
    image: postgres:16
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 5s
      timeout: 3s
      retries: 5
      start_period: 10s
    environment:
      POSTGRES_PASSWORD: secret

  api:
    image: alpine:latest
    depends_on:
      db:
        condition: service_healthy
    command: |
      sh -c '
        echo "Esperando a que DB esté healthy..."
        while ! pg_isready -h db -U postgres; do
          sleep 1
        done
        echo "DB lista! Ejecutando API..."
        tail -f /dev/null
      '
    # Nota: el depends_on con condition: service_healthy
    # garantiza que db esté healthy antes de iniciar api
EOF

cd /tmp/docker_compose_hc
docker compose up -d

# Observar que api no inicia hasta que db sea healthy
docker compose ps

# Verificar el orden de arranque
sleep 35
docker compose ps

docker compose down
rm -rf /tmp/docker_compose_hc
```

### Ejercicio 5 — Verificar que Docker no reinicia contenedores unhealthy

```bash
# Crear un contenedor unhealthy que NO se reinicia automáticamente
cat > /tmp/Dockerfile.noauto << 'EOF'
FROM nginx:latest
HEALTHCHECK --interval=3s --timeout=2s --retries=3 \
  CMD curl -f http://localhost/ || exit 1
EOF

docker build -t noauto-test -f /tmp/Dockerfile.noauto /tmp

# Crear con restart=no para evidencia clara
docker run -d --name noauto_test --restart=no noauto-test

# Esperar a que sea healthy
sleep 15
docker ps --format "{{.Names}}: {{.Status}}"

# Matar nginx (el proceso muere, pero el contenedor sigue "running" vació)
docker exec noauto_test sh -c 'nginx -s stop'

# Esperar a que se detecte como unhealthy
sleep 15
docker ps --format "{{.Names}}: {{.Status}}"
# Verás: noauto_test: Up ... (unhealthy)
# PERO el contenedor NO se ha reiniciado (aún existe con unhealthy)

# Ver que el proceso principal murió
docker logs noauto_test
# nginx: [alert] ... (lo que sea que haya en logs)

docker rm -f noauto_test
docker rmi noauto-test
rm /tmp/Dockerfile.noauto
```

---

## Resumen de comandos

```bash
# Dockerfile
HEALTHCHECK [--interval=30s] [--timeout=30s] [--start-period=0s] [--retries=3] CMD comando

# Runtime
--health-cmd comando
--health-interval 30s
--health-timeout 30s
--health-start-period 0s
--health-retries 3
--no-healthcheck    # Deshabilitar

# Inspección
docker inspect --format '{{.State.Health}}' container
docker events --filter 'event=health_status'

# docker ps
docker ps --format "{{.Names}}: {{.Status}}"
```
