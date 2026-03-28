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
| Verificar dependencias críticas | DB, cache, filesystem, no solo "proceso vivo" |
| Evitar instalar herramientas solo para el health | Aumenta la superficie de ataque |

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
docker ps --format "{{.Names}}: {{.Status}}"
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
