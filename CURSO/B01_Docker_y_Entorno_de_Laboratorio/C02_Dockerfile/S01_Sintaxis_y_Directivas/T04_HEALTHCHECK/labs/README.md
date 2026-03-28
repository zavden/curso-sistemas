# Lab — HEALTHCHECK

## Objetivo

Laboratorio práctico para entender la instrucción HEALTHCHECK: observar la
transición de estados (starting → healthy → unhealthy), controlar
manualmente el estado de salud, experimentar con los parámetros del
healthcheck, y definir healthchecks en runtime.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imágenes base descargadas:
  - `docker pull nginx:latest`
  - `docker pull alpine:latest`
- Estar en el directorio `labs/` de este tópico

## Archivos del laboratorio

| Archivo | Propósito |
|---|---|
| `app.sh` | App con arranque rápido y marcador de salud controlable (Parte 2) |
| `slow-app.sh` | App con arranque lento — 12s de inicialización (Parte 3) |
| `Dockerfile.health-basic` | Nginx con healthcheck básico (Parte 1) |
| `Dockerfile.health-app` | App con salud controlable (Parte 2) |
| `Dockerfile.health-slow` | App lenta con start-period (Parte 3) |

---

## Parte 1 — Transición starting → healthy

### Objetivo

Observar cómo Docker reporta el estado de salud de un contenedor con
HEALTHCHECK: la transición desde `starting` hasta `healthy`.

### Paso 1.1: Examinar el Dockerfile

```bash
cat Dockerfile.health-basic
```

Este Dockerfile extiende nginx con un healthcheck que verifica que el
servidor responde a HTTP requests.

Parámetros configurados:

- `--interval=5s` — verificar cada 5 segundos
- `--timeout=3s` — esperar máximo 3 segundos por respuesta
- `--start-period=5s` — 5 segundos de gracia para arrancar
- `--retries=2` — 2 fallos consecutivos para marcar unhealthy

### Paso 1.2: Construir y ejecutar

```bash
docker build -f Dockerfile.health-basic -t lab-health-basic .
docker run -d --name hc-basic lab-health-basic
```

### Paso 1.3: Observar la transición

Ejecutar varias veces con intervalos cortos:

```bash
docker ps --format "{{.Names}}: {{.Status}}"
```

Inmediatamente después de arrancar:

```
hc-basic: Up 2 seconds (health: starting)
```

Esperar ~15 segundos:

```bash
sleep 15
docker ps --format "{{.Names}}: {{.Status}}"
```

Ahora:

```
hc-basic: Up 17 seconds (healthy)
```

La transición fue: sin healthcheck → starting → healthy.

### Paso 1.4: Inspeccionar el log de salud

```bash
docker inspect --format '{{json .State.Health}}' hc-basic | python3 -m json.tool
```

Salida esperada (parcial):

```json
{
    "Status": "healthy",
    "FailingStreak": 0,
    "Log": [
        {
            "Start": "...",
            "End": "...",
            "ExitCode": 0,
            "Output": "..."
        }
    ]
}
```

- `Status`: estado actual (`healthy`)
- `FailingStreak`: fallos consecutivos actuales (0)
- `Log`: historial de los últimos 5 healthchecks con exit code y output

### Paso 1.5: Limpiar

```bash
docker rm -f hc-basic
docker rmi lab-health-basic
```

---

## Parte 2 — Provocar unhealthy y recuperar

### Objetivo

Usar una app con estado de salud controlable para provocar manualmente la
transición a unhealthy, inspeccionar el estado, y luego recuperar la salud.

### Paso 2.1: Examinar los archivos

```bash
cat app.sh
```

La app crea `/tmp/healthy` después de 2 segundos. El healthcheck verifica
que ese archivo existe.

```bash
cat Dockerfile.health-app
```

El healthcheck es: `[ -f /tmp/healthy ] || exit 1` — si el archivo existe,
healthy; si no, unhealthy.

### Paso 2.2: Construir y ejecutar

```bash
docker build -f Dockerfile.health-app -t lab-health-app .
docker run -d --name hc-app lab-health-app
```

### Paso 2.3: Esperar a que sea healthy

```bash
sleep 10
docker ps --format "{{.Names}}: {{.Status}}"
```

Salida esperada:

```
hc-app: Up 10 seconds (healthy)
```

### Paso 2.4: Provocar unhealthy

Eliminar el marcador de salud:

```bash
docker exec hc-app rm /tmp/healthy
```

Antes de verificar, responde mentalmente:

- ¿Cuánto tardará en pasar a unhealthy?
- ¿Cuántos checks tienen que fallar? (mira `--retries` en el Dockerfile)

Intenta predecir antes de continuar al siguiente paso.

```bash
# Esperar a que se detecte (interval=3s * retries=2 = ~6-9 segundos)
sleep 10
docker ps --format "{{.Names}}: {{.Status}}"
```

Salida esperada:

```
hc-app: Up 20 seconds (unhealthy)
```

### Paso 2.5: Inspeccionar el estado unhealthy

```bash
docker inspect --format '{{json .State.Health}}' hc-app | python3 -m json.tool
```

Observa:

- `Status`: `unhealthy`
- `FailingStreak`: incrementando con cada check fallido
- `Log`: los últimos checks muestran `ExitCode: 1`

### Paso 2.6: Recuperar la salud

Recrear el marcador:

```bash
docker exec hc-app touch /tmp/healthy
sleep 10
docker ps --format "{{.Names}}: {{.Status}}"
```

Salida esperada:

```
hc-app: Up ... (healthy)
```

El contenedor vuelve a `healthy` en cuanto un check pasa. La transición
unhealthy → healthy es inmediata (no requiere `retries` checks exitosos).

### Paso 2.7: Docker no reinicia contenedores unhealthy

Verificar que el contenedor sigue corriendo aunque sea unhealthy:

```bash
docker exec hc-app rm /tmp/healthy
sleep 10
docker ps --format "{{.Names}}: {{.Status}}"
# hc-app: Up ... (unhealthy) — pero sigue corriendo

docker exec hc-app echo "Sigo vivo"
# Sigo vivo
```

Docker **no** reinicia automáticamente un contenedor unhealthy. El healthcheck
solo cambia el estado reportado. Para acción automática se necesitan
orquestadores (Compose con `restart`, Kubernetes, Swarm).

### Paso 2.8: Limpiar

```bash
docker rm -f hc-app
docker rmi lab-health-app
```

---

## Parte 3 — start-period y parámetros

### Objetivo

Demostrar la importancia de `--start-period` para aplicaciones que tardan
en arrancar, y cómo los parámetros del healthcheck afectan el comportamiento.

### Paso 3.1: Examinar la app lenta

```bash
cat slow-app.sh
```

Esta app tarda 12 segundos en crear el marcador de salud (simulando una
inicialización lenta: cargar datos, conectar a base de datos, etc.).

### Paso 3.2: Sin start-period suficiente

Ejecutar la app lenta con un healthcheck **sin start-period**:

```bash
docker run -d --name hc-no-grace \
    --health-cmd "[ -f /tmp/healthy ] || exit 1" \
    --health-interval 3s \
    --health-timeout 2s \
    --health-retries 2 \
    --health-start-period 0s \
    lab-health-slow 2>/dev/null || \
docker run -d --name hc-no-grace \
    $(docker build -q -f Dockerfile.health-slow .) sh -c "sleep 12 && touch /tmp/healthy && while true; do sleep 1; done"
```

Construir primero si no se ha hecho:

```bash
docker build -f Dockerfile.health-slow -t lab-health-slow .
```

Ejecutar sin start-period (usando flags de runtime que sobrescriben el
Dockerfile):

```bash
docker run -d --name hc-no-grace \
    --health-cmd "[ -f /tmp/healthy ] || exit 1" \
    --health-interval 3s \
    --health-timeout 2s \
    --health-retries 2 \
    --health-start-period 0s \
    lab-health-slow
```

```bash
# Observar inmediatamente
sleep 8
docker ps --format "{{.Names}}: {{.Status}}"
```

Salida esperada:

```
hc-no-grace: Up 8 seconds (unhealthy)
```

La app aún no ha terminado de arrancar (tarda 12s), pero el healthcheck ya
la marcó como unhealthy después de 2 fallos (retries=2, interval=3s → ~6s).

```bash
docker rm -f hc-no-grace
```

### Paso 3.3: Con start-period suficiente

```bash
docker run -d --name hc-with-grace \
    --health-cmd "[ -f /tmp/healthy ] || exit 1" \
    --health-interval 3s \
    --health-timeout 2s \
    --health-retries 2 \
    --health-start-period 15s \
    lab-health-slow
```

```bash
# Durante el start-period, los fallos no cuentan
sleep 8
docker ps --format "{{.Names}}: {{.Status}}"
```

Salida esperada:

```
hc-with-grace: Up 8 seconds (health: starting)
```

Sigue en `starting` — no `unhealthy` — porque estamos dentro del start-period
de 15 segundos. Los fallos de healthcheck se ignoran.

```bash
# Después de que la app arranque (12s) y pase un check
sleep 10
docker ps --format "{{.Names}}: {{.Status}}"
```

Salida esperada:

```
hc-with-grace: Up 18 seconds (healthy)
```

### Paso 3.4: Analizar

| Escenario | Resultado |
|---|---|
| Sin start-period | unhealthy antes de que la app arranque (falso positivo) |
| Con start-period=15s | starting → healthy (la app tuvo tiempo de inicializarse) |

**Regla**: configurar `--start-period` con un margen suficiente para el peor
caso de arranque de la aplicación. Es mejor un start-period largo que un
falso unhealthy.

### Paso 3.5: Limpiar

```bash
docker rm -f hc-with-grace
docker rmi lab-health-slow
```

---

## Parte 4 — Runtime y HEALTHCHECK NONE

### Objetivo

Demostrar cómo definir healthchecks en runtime (sin Dockerfile) y cómo
deshabilitar un healthcheck heredado de la imagen.

### Paso 4.1: Definir healthcheck en runtime

```bash
docker run -d --name hc-runtime \
    --health-cmd "curl -f http://localhost/ || exit 1" \
    --health-interval 5s \
    --health-timeout 3s \
    --health-retries 2 \
    --health-start-period 5s \
    nginx:latest
```

```bash
sleep 15
docker ps --format "{{.Names}}: {{.Status}}"
```

Salida esperada:

```
hc-runtime: Up 15 seconds (healthy)
```

No se necesitó un Dockerfile — el healthcheck se definió completamente con
flags de `docker run`.

### Paso 4.2: Inspeccionar

```bash
docker inspect --format '{{json .State.Health.Status}}' hc-runtime
docker inspect --format '{{json .State.Health.FailingStreak}}' hc-runtime
```

### Paso 4.3: Deshabilitar healthcheck con --no-healthcheck

Si una imagen tiene un healthcheck definido en el Dockerfile, se puede
deshabilitar en runtime:

```bash
docker run -d --name hc-disabled \
    --no-healthcheck \
    lab-health-basic 2>/dev/null || \
docker build -f Dockerfile.health-basic -t lab-health-basic . && \
docker run -d --name hc-disabled --no-healthcheck lab-health-basic
```

```bash
sleep 10
docker ps --format "{{.Names}}: {{.Status}}"
```

Salida esperada:

```
hc-disabled: Up 10 seconds
```

Sin `(healthy)` ni `(unhealthy)` — el healthcheck fue deshabilitado. Docker
no ejecuta ningún check.

### Paso 4.4: Limpiar

```bash
docker rm -f hc-runtime hc-disabled
docker rmi lab-health-basic 2>/dev/null
```

---

## Limpieza final

```bash
# Eliminar imágenes del lab (si alguna quedó)
docker rmi lab-health-basic lab-health-app lab-health-slow 2>/dev/null

# Eliminar contenedores huérfanos (si alguno quedó)
docker rm -f hc-basic hc-app hc-no-grace hc-with-grace \
    hc-runtime hc-disabled 2>/dev/null
```

---

## Conceptos reforzados

1. Un contenedor en estado `running` no garantiza que la aplicación funcione.
   **HEALTHCHECK** permite verificar si la aplicación responde correctamente.

2. Los estados de salud son: **starting** (dentro del start-period),
   **healthy** (checks pasando), **unhealthy** (retries fallos consecutivos).

3. **start-period** es crucial para aplicaciones con arranque lento. Sin él,
   la app puede marcarse unhealthy antes de estar lista (falso positivo).

4. Docker **no reinicia** automáticamente contenedores unhealthy. El
   healthcheck solo cambia el estado reportado. Se necesitan restart policies
   u orquestadores para acción automática.

5. Los healthchecks se pueden definir en el Dockerfile o en runtime con
   flags de `docker run` (`--health-cmd`, `--health-interval`, etc.).
   Se deshabilitan con `--no-healthcheck`.

6. `docker inspect` muestra el historial de healthchecks: los últimos 5
   resultados con exit code, output, y timestamps.
