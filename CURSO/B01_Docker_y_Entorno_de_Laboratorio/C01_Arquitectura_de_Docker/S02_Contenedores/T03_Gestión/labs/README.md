# Lab — Gestion

## Objetivo

Laboratorio practico para verificar de forma empirica las herramientas de
gestion de contenedores Docker: listado y filtrado con `docker ps`, inspeccion
con `docker inspect`, logs con `docker logs`, procesos con `docker top`,
copia de archivos con `docker cp`, renombrado con `docker rename`, y monitoreo
con `docker stats`.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada: `docker pull debian:bookworm`
- Estar en el directorio `labs/` de este topico

## Archivos del laboratorio

| Archivo | Proposito |
|---|---|
| `README.md` | Este documento: guia paso a paso |
| `Dockerfile.log-demo` | Imagen que genera logs periodicos con timestamps |

---

## Parte 1 — Listado y filtrado

### Objetivo

Dominar `docker ps` con sus flags `-a`, `-q`, `--filter`, `--format` y `-s`
para localizar y analizar contenedores.

### Paso 1.1: Preparar contenedores en distintos estados

```bash
# Contenedor corriendo
docker run -d --name ps-run --label env=production debian:bookworm sleep 300

# Contenedor detenido
docker run --name ps-stop --label env=staging debian:bookworm echo "batch job done"

# Contenedor con label especifico
docker run -d --name ps-label --label env=production --label team=backend debian:bookworm sleep 300
```

### Paso 1.2: `docker ps` vs `docker ps -a`

```bash
echo "=== docker ps (solo running) ==="
docker ps --filter "name=ps-" --format 'table {{.Names}}\t{{.Status}}'

echo ""
echo "=== docker ps -a (todos) ==="
docker ps -a --filter "name=ps-" --format 'table {{.Names}}\t{{.Status}}'
```

Salida esperada:

```
=== docker ps (solo running) ===
NAMES      STATUS
ps-label   Up X seconds
ps-run     Up X seconds

=== docker ps -a (todos) ===
NAMES      STATUS
ps-label   Up X seconds
ps-run     Up X seconds
ps-stop    Exited (0) X seconds ago
```

### Paso 1.3: Filtrar por label

Antes de ejecutar, predice: ¿cuantos contenedores tienen el label
`env=production`?

Intenta predecir antes de continuar al siguiente paso.

```bash
docker ps --filter label=env=production --format 'table {{.Names}}\t{{.Labels}}'
```

Salida esperada:

```
NAMES      LABELS
ps-label   env=production,team=backend
ps-run     env=production
```

Los filtros se pueden combinar (AND logico):

```bash
docker ps --filter label=env=production --filter label=team=backend \
    --format 'table {{.Names}}\t{{.Labels}}'
```

Salida esperada:

```
NAMES      LABELS
ps-label   env=production,team=backend
```

### Paso 1.4: Filtrar por estado y exit code

```bash
# Contenedores que terminaron con exit code 0
docker ps -a --filter exited=0 --filter "name=ps-" --format 'table {{.Names}}\t{{.Status}}'
```

Salida esperada:

```
NAMES     STATUS
ps-stop   Exited (0) X seconds ago
```

### Paso 1.5: Formato personalizado

```bash
docker ps -a --filter "name=ps-" \
    --format 'table {{.Names}}\t{{.Image}}\t{{.Status}}\t{{.Command}}'
```

```bash
# Solo nombres, uno por linea
docker ps --filter "name=ps-" --format '{{.Names}}'
```

```bash
# Formato JSON
docker ps --filter "name=ps-" --format json | head -1
```

### Paso 1.6: `-q` para scripting

```bash
# Solo IDs
docker ps -q --filter "name=ps-"

# Contar contenedores running
echo "Running: $(docker ps -q --filter 'name=ps-' | wc -l)"
```

### Paso 1.7: `-s` para ver tamano

```bash
docker ps -s --filter "name=ps-" --format 'table {{.Names}}\t{{.Size}}'
```

La primera cifra es el tamano de la capa de escritura. `virtual` es la suma
de imagen + capa de escritura.

### Paso 1.8: Limpiar

```bash
docker rm -f ps-run ps-stop ps-label
```

---

## Parte 2 — Inspeccion profunda

### Objetivo

Usar `docker inspect` para extraer informacion detallada de un contenedor:
configuracion, estado, red, y mounts.

### Paso 2.1: Crear un contenedor con configuracion rica

```bash
docker run -d --name inspect-demo \
    -e APP_ENV=development \
    -e APP_VERSION=1.0 \
    -w /opt/app \
    --label project=lab-gestion \
    debian:bookworm sleep 300
```

### Paso 2.2: Inspeccion completa (JSON)

```bash
docker inspect inspect-demo | head -30
```

`docker inspect` devuelve un JSON completo con toda la informacion del
contenedor. Es un documento extenso — en la practica se usan filtros.

### Paso 2.3: Extraer el estado

```bash
docker inspect inspect-demo --format '{{.State.Status}}'
docker inspect inspect-demo --format 'PID: {{.State.Pid}}'
docker inspect inspect-demo --format 'Started: {{.State.StartedAt}}'
```

### Paso 2.4: Extraer configuracion

```bash
# Imagen usada
docker inspect inspect-demo --format 'Image: {{.Config.Image}}'

# Comando ejecutado
docker inspect inspect-demo --format 'Cmd: {{.Config.Cmd}}'

# Directorio de trabajo
docker inspect inspect-demo --format 'WorkDir: {{.Config.WorkingDir}}'

# Variables de entorno
docker inspect inspect-demo --format '{{range .Config.Env}}{{println .}}{{end}}'
```

### Paso 2.5: Extraer informacion de red

```bash
docker inspect inspect-demo --format 'IP: {{.NetworkSettings.IPAddress}}'
docker inspect inspect-demo --format 'Gateway: {{.NetworkSettings.Gateway}}'
docker inspect inspect-demo --format 'MAC: {{.NetworkSettings.MacAddress}}'
```

### Paso 2.6: Extraer informacion del storage driver

```bash
docker inspect inspect-demo --format '{{json .GraphDriver.Data}}' | python3 -m json.tool
```

Salida esperada (estructura):

```json
{
    "LowerDir": "...",
    "MergedDir": "...",
    "UpperDir": "...",
    "WorkDir": "..."
}
```

Estos son los directorios de OverlayFS:
- **LowerDir**: capas de solo lectura (la imagen)
- **UpperDir**: capa de escritura del contenedor
- **MergedDir**: vista unificada que ve el proceso

### Paso 2.7: Extraer labels

```bash
docker inspect inspect-demo --format '{{json .Config.Labels}}' | python3 -m json.tool
```

### Paso 2.8: Limpiar

```bash
docker rm -f inspect-demo
```

---

## Parte 3 — Logs y procesos

### Objetivo

Practicar `docker logs` con sus diferentes flags y `docker top` para ver
procesos dentro de un contenedor.

### Paso 3.1: Construir la imagen generadora de logs

```bash
cat Dockerfile.log-demo
```

El Dockerfile crea un script que imprime un mensaje cada 2 segundos y un
WARNING cada 5 mensajes a stderr.

```bash
docker build -f Dockerfile.log-demo -t lab-log-demo .
```

### Paso 3.2: Arrancar el generador de logs

```bash
docker run -d --name log-source lab-log-demo
```

Espera unos segundos para que se generen logs.

### Paso 3.3: Ver todos los logs

```bash
docker logs log-source
```

Salida esperada (parcial):

```
[HH:MM:SS] Event #1: processing request
[HH:MM:SS] Event #2: processing request
...
[HH:MM:SS] Event #5: processing request
[HH:MM:SS] WARNING: high latency detected
...
```

### Paso 3.4: Ultimas N lineas

```bash
docker logs --tail 5 log-source
```

Solo muestra las ultimas 5 lineas de logs.

### Paso 3.5: Logs desde un momento especifico

```bash
# Logs de los ultimos 10 segundos
docker logs --since 10s log-source
```

### Paso 3.6: Follow (streaming en tiempo real)

```bash
# Seguir los logs en tiempo real (como tail -f)
# Presiona Ctrl+C para salir despues de observar unas lineas
docker logs -f --tail 3 log-source
```

Observa como aparecen nuevas lineas cada 2 segundos. Ctrl+C para salir
del follow (no afecta al contenedor).

### Paso 3.7: Logs con timestamps

```bash
docker logs -t --tail 5 log-source
```

Salida esperada (parcial):

```
<timestamp>  [HH:MM:SS] Event #N: processing request
```

El flag `-t` agrega el timestamp de Docker (cuando recibio la linea),
distinto del timestamp generado por la aplicacion.

### Paso 3.8: Los logs de exec NO aparecen en docker logs

Antes de ejecutar, predice: si ejecutamos un `echo` con `docker exec`,
¿aparecera en `docker logs`?

Intenta predecir antes de continuar al siguiente paso.

```bash
docker exec log-source bash -c "echo 'this was from exec'"
docker logs --tail 3 log-source
```

El `echo` de exec aparecio en tu terminal, pero **no** en `docker logs`.
`docker logs` solo captura stdout/stderr de PID 1.

### Paso 3.9: `docker top` — procesos dentro del contenedor

```bash
docker run -d --name top-demo debian:bookworm bash -c "sleep 100 & sleep 200 & sleep 300 & wait"
docker top top-demo
```

Salida esperada (parcial):

```
UID   PID    PPID   ...  CMD
root  <pid>  <pid>  ...  bash -c sleep 100 & sleep 200 & sleep 300 & wait
root  <pid>  <pid>  ...  sleep 100
root  <pid>  <pid>  ...  sleep 200
root  <pid>  <pid>  ...  sleep 300
```

`docker top` muestra los procesos que corren dentro del contenedor, similares
a lo que veria `ps` ejecutado desde dentro.

```bash
# Con formato custom de ps
docker top top-demo -o pid,ppid,user,args
```

### Paso 3.10: Limpiar

```bash
docker rm -f log-source top-demo
```

---

## Parte 4 — Copia de archivos y monitoreo

### Objetivo

Practicar `docker cp` para copiar archivos entre host y contenedor,
`docker rename` para renombrar contenedores, y `docker stats` para
monitorear recursos.

### Paso 4.1: Preparar un contenedor

```bash
docker run -d --name cp-demo debian:bookworm sleep 300
```

### Paso 4.2: Copiar archivos del host al contenedor

```bash
# Crear un archivo temporal en el host
echo "data from host" > /tmp/host-file.txt

# Copiar al contenedor
docker cp /tmp/host-file.txt cp-demo:/tmp/

# Verificar dentro del contenedor
docker exec cp-demo cat /tmp/host-file.txt
```

Salida esperada:

```
data from host
```

### Paso 4.3: Copiar archivos del contenedor al host

```bash
# Crear un archivo dentro del contenedor
docker exec cp-demo bash -c "echo 'data from container' > /tmp/container-file.txt"

# Copiar al host
docker cp cp-demo:/tmp/container-file.txt /tmp/

# Verificar en el host
cat /tmp/container-file.txt
```

Salida esperada:

```
data from container
```

### Paso 4.4: Copiar directorios completos

```bash
# Crear estructura dentro del contenedor
docker exec cp-demo bash -c "mkdir -p /tmp/app && echo 'config' > /tmp/app/config.txt && echo 'data' > /tmp/app/data.txt"

# Copiar directorio completo al host
docker cp cp-demo:/tmp/app /tmp/container-app

# Verificar
ls /tmp/container-app/
cat /tmp/container-app/config.txt
```

Salida esperada:

```
config.txt  data.txt
config
```

### Paso 4.5: `docker cp` funciona con contenedores detenidos

Antes de ejecutar, predice: ¿podras copiar archivos de un contenedor en
estado `exited`?

Intenta predecir antes de continuar al siguiente paso.

```bash
docker stop cp-demo
docker cp cp-demo:/tmp/container-file.txt /tmp/from-stopped.txt
cat /tmp/from-stopped.txt
```

Salida esperada:

```
data from container
```

`docker cp` funciona con contenedores detenidos, a diferencia de `docker exec`.

### Paso 4.6: `docker rename`

```bash
docker start cp-demo
docker rename cp-demo cp-renamed

# Verificar
docker ps --filter name=cp-renamed --format 'table {{.Names}}\t{{.Status}}'
```

Salida esperada:

```
NAMES        STATUS
cp-renamed   Up X seconds
```

El contenedor sigue corriendo con el nuevo nombre. El nombre anterior deja
de existir.

```bash
docker rm -f cp-renamed
```

### Paso 4.7: Monitoreo con `docker stats`

```bash
# Contenedor con carga de CPU
docker run -d --name stats-busy debian:bookworm bash -c "while true; do :; done"

# Contenedor idle
docker run -d --name stats-idle debian:bookworm sleep 300

# Ver stats de ambos (una lectura)
docker stats --no-stream --filter "name=stats-" \
    --format 'table {{.Name}}\t{{.CPUPerc}}\t{{.MemUsage}}\t{{.PIDs}}'
```

Salida esperada (aproximada):

```
NAME         CPU %     MEM USAGE / LIMIT     PIDS
stats-busy   ~100%     ~1MiB / XGiB          1
stats-idle   ~0.00%    ~500KiB / XGiB        1
```

El contenedor `stats-busy` consume casi 100% de un core de CPU por el loop
infinito. El contenedor `stats-idle` apenas consume recursos.

### Paso 4.8: Stats con formato personalizado

```bash
docker stats --no-stream \
    --format 'table {{.Name}}\t{{.CPUPerc}}\t{{.MemUsage}}\t{{.NetIO}}\t{{.BlockIO}}'
```

### Paso 4.9: Limpiar

```bash
docker rm -f stats-busy stats-idle

# Limpiar archivos temporales del host
rm -f /tmp/host-file.txt /tmp/container-file.txt /tmp/from-stopped.txt
rm -rf /tmp/container-app
```

---

## Limpieza final

```bash
# Eliminar contenedores huerfanos (si quedo alguno por un paso saltado)
docker rm -f ps-run ps-stop ps-label inspect-demo \
    log-source top-demo cp-demo cp-renamed stats-busy stats-idle 2>/dev/null

# Eliminar imagen del lab
docker rmi lab-log-demo 2>/dev/null

# Limpiar archivos temporales del host
rm -f /tmp/host-file.txt /tmp/container-file.txt /tmp/from-stopped.txt 2>/dev/null
rm -rf /tmp/container-app 2>/dev/null
```

---

## Conceptos reforzados

1. `docker ps` sin `-a` solo muestra contenedores `running`. Con `-a` muestra
   todos los estados. `-q` devuelve solo IDs para scripting.
2. `--filter` permite filtrar por `status`, `name`, `ancestor`, `label`, y
   `exited`. Multiples filtros se combinan con AND logico.
3. `--format` con Go templates permite personalizar la salida. `{{json .}}`
   produce JSON para procesamiento programatico.
4. `docker inspect` devuelve toda la informacion del contenedor en JSON.
   `--format` con Go templates extrae campos especificos.
5. `docker logs` captura **solo** stdout/stderr de PID 1. La salida de
   procesos lanzados con `docker exec` no aparece en los logs.
6. `docker top` muestra los procesos dentro del contenedor sin necesidad
   de tener `ps` instalado en la imagen.
7. `docker cp` funciona con contenedores corriendo **y** detenidos, a
   diferencia de `docker exec` que requiere estado `running`.
8. `docker stats` muestra CPU, memoria, red y disco en tiempo real.
   `--no-stream` captura un solo snapshot.
