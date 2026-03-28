# T03 — Gestión

## `docker ps`

Lista contenedores. Por defecto solo muestra los que están en estado `running`.

```bash
# Contenedores corriendo
docker ps

# TODOS los contenedores (incluyendo detenidos)
docker ps -a
```

Salida típica:

```
CONTAINER ID   IMAGE           COMMAND       CREATED          STATUS          PORTS     NAMES
a1b2c3d4e5f6   nginx:latest    "/docker…"    5 minutes ago    Up 5 minutes    80/tcp    web
b2c3d4e5f6a7   debian:bookworm "bash"        10 minutes ago   Up 10 minutes             dev
```

### Flags útiles

#### `-q` (quiet)

Solo muestra container IDs. Indispensable para scripting:

```bash
# Detener TODOS los contenedores corriendo
docker stop $(docker ps -q)

# Eliminar TODOS los contenedores detenidos
docker rm $(docker ps -aq --filter status=exited)

# Contar contenedores corriendo
docker ps -q | wc -l
```

#### `--filter`

Filtra contenedores por criterios específicos:

```bash
# Por estado
docker ps -a --filter status=exited
docker ps -a --filter status=running
docker ps -a --filter status=paused

# Por nombre (soporta expresiones regulares parciales)
docker ps --filter name=web
docker ps --filter name=dev

# Por imagen
docker ps --filter ancestor=nginx:latest
docker ps --filter ancestor=debian

# Por label
docker ps --filter label=environment=production

# Por exit code
docker ps -a --filter exited=0     # terminaron exitosamente
docker ps -a --filter exited=137   # fueron matados (SIGKILL)

# Combinar filtros (AND lógico)
docker ps -a --filter status=exited --filter ancestor=debian
```

#### `--format`

Formatea la salida con Go templates:

```bash
# Tabla custom
docker ps --format "table {{.Names}}\t{{.Image}}\t{{.Status}}\t{{.Ports}}"

# Una línea por contenedor con info específica
docker ps --format "{{.Names}}: {{.Image}} ({{.Status}})"

# JSON
docker ps --format json

# Solo nombres
docker ps --format '{{.Names}}'
```

Campos disponibles en `--format`:

| Campo | Descripción |
|-------|------------|
| `.ID` | Container ID |
| `.Image` | Nombre de la imagen |
| `.Command` | Comando ejecutado |
| `.CreatedAt` | Timestamp de creación |
| `.Status` | Estado actual |
| `.Ports` | Puertos publicados |
| `.Names` | Nombre del contenedor |
| `.Labels` | Labels asignados |
| `.Size` | Tamaño de la capa de escritura |
| `.Mounts` | Volúmenes montados |
| `.Networks` | Redes conectadas |

#### `-s` (size)

Muestra el tamaño de la capa de escritura de cada contenedor:

```bash
docker ps -s
# ... SIZE
# ... 5B (virtual 117MB)
```

- El primer número es el tamaño de la capa de escritura (datos del contenedor)
- `virtual` es el tamaño total (imagen + capa de escritura)

## `docker exec`

Ejecuta un comando **dentro de un contenedor que ya está corriendo**. Crea un proceso
nuevo, separado de PID 1.

```bash
# Abrir una shell interactiva
docker exec -it my-container bash

# Ejecutar un comando y salir
docker exec my-container cat /etc/hostname

# Ejecutar como root (aunque el contenedor corra como otro usuario)
docker exec -u root my-container apt-get update

# Ejecutar como usuario específico
docker exec -u 1000:1000 my-container id

# Ejecutar en un directorio específico
docker exec -w /var/log my-container ls -la

# Establecer variables de entorno para el comando
docker exec -e DEBUG=true my-container env | grep DEBUG
```

### Diferencia clave con `docker run`

`docker exec` usa un **contenedor existente que está corriendo**. No crea un contenedor
nuevo. El proceso que `exec` lanza comparte:
- El filesystem del contenedor (misma capa de escritura)
- El mismo network namespace (misma IP)
- Los mismos volúmenes montados

Pero tiene su propio PID (no es PID 1). Ctrl+C en un `exec` mata solo el proceso
ejecutado, no el contenedor.

### `exec` falla si el contenedor no está corriendo

```bash
docker create --name stopped debian:bookworm bash
docker exec stopped ls
# Error response from daemon: Container stopped is not running

docker rm stopped
```

Para inspeccionar un contenedor detenido, usar `docker cp`, `docker logs`, o
`docker start` primero.

## `docker logs`

Muestra stdout y stderr del **proceso principal** (PID 1) del contenedor.

```bash
# Ver todos los logs
docker logs my-container

# Follow (como tail -f)
docker logs -f my-container

# Últimas 20 líneas
docker logs --tail 20 my-container

# Logs desde hace 5 minutos
docker logs --since 5m my-container

# Logs hasta hace 1 minuto
docker logs --until 1m my-container

# Combinar: follow desde las últimas 10 líneas
docker logs -f --tail 10 my-container

# Con timestamps
docker logs -t my-container
# 2024-01-15T10:30:00.123456789Z  mensaje de log...
```

### Logging drivers

Docker almacena los logs por defecto en JSON en el host:

```
/var/lib/docker/containers/<container-id>/<container-id>-json.log
```

En contenedores de larga vida, estos archivos crecen indefinidamente. Configurar
límites:

```bash
# Limitar tamaño de logs por contenedor
docker run -d \
  --log-opt max-size=10m \
  --log-opt max-file=3 \
  --name web nginx:latest
```

Esto mantiene máximo 3 archivos de 10MB cada uno (30MB total por contenedor). Cuando
se llena, rota automáticamente.

Para configurar el límite globalmente, editar `/etc/docker/daemon.json`:

```json
{
  "log-driver": "json-file",
  "log-opts": {
    "max-size": "10m",
    "max-file": "3"
  }
}
```

### Solo PID 1

`docker logs` solo captura la salida de PID 1. Si un proceso lanzado con `docker exec`
imprime algo en stdout, **no aparece en `docker logs`** — solo se muestra en la
terminal donde se ejecutó el `exec`.

```bash
docker run -d --name log-test debian:bookworm bash -c "echo 'from PID 1'; sleep 300"
docker logs log-test
# from PID 1

docker exec log-test bash -c "echo 'from exec'"
# from exec  ← aparece aquí, en tu terminal

docker logs log-test
# from PID 1  ← el echo de exec NO aparece aquí

docker rm -f log-test
```

## `docker stats`

Monitoreo de recursos en tiempo real, similar a `top`:

```bash
# Todos los contenedores corriendo
docker stats

# Contenedores específicos
docker stats web db

# Una sola lectura (sin streaming)
docker stats --no-stream

# Formato custom
docker stats --format "table {{.Name}}\t{{.CPUPerc}}\t{{.MemUsage}}"
```

Salida típica:

```
CONTAINER ID   NAME   CPU %   MEM USAGE / LIMIT     MEM %   NET I/O        BLOCK I/O     PIDS
a1b2c3d4e5f6   web    0.02%   5.5MiB / 15.6GiB      0.03%   1.2kB / 650B   0B / 4.1kB    5
```

| Columna | Significado |
|---------|------------|
| CPU % | Porcentaje de CPU usado (puede > 100% en multi-core) |
| MEM USAGE / LIMIT | Memoria usada / límite de memoria del contenedor |
| MEM % | Porcentaje de la memoria asignada |
| NET I/O | Datos enviados / recibidos por red |
| BLOCK I/O | Datos leídos / escritos en disco |
| PIDS | Número de procesos dentro del contenedor |

## `docker attach` vs `docker exec`

Estos dos comandos se confunden con frecuencia pero son fundamentalmente diferentes.

### `docker attach`

Conecta tu terminal al **stdin/stdout/stderr de PID 1**. No crea un proceso nuevo.

```bash
docker run -dit --name attach-test debian:bookworm bash
docker attach attach-test
# Ahora estás "dentro" del bash que es PID 1
# Si escribes exit, PID 1 termina → el contenedor se detiene
```

Problemas de `attach`:
- Ctrl+C envía SIGINT a PID 1 → puede detener el contenedor
- Si PID 1 no es interactivo (ej: nginx), `attach` muestra su salida pero no puedes
  hacer mucho
- Solo hay un PID 1 — múltiples `attach` comparten el mismo stdin/stdout

Para desconectarse sin matar el contenedor: **Ctrl+P, Ctrl+Q** (la secuencia de
detach por defecto).

### `docker exec`

Crea un **proceso nuevo** dentro del contenedor, separado de PID 1.

```bash
docker run -d --name exec-test debian:bookworm sleep 300
docker exec -it exec-test bash
# Esto es un bash nuevo, no PID 1
# exit aquí NO detiene el contenedor (PID 1 sigue siendo sleep 300)
```

### Cuándo usar cada uno

| Situación | Usar |
|-----------|------|
| Abrir una shell para inspeccionar | `exec -it bash` |
| Ejecutar un comando puntual | `exec command` |
| Ver la salida en tiempo real de PID 1 | `attach` (o mejor: `logs -f`) |
| Enviar input a PID 1 (raro) | `attach` |

**Regla práctica**: usar `exec` siempre. Usar `attach` casi nunca.

## `docker cp`

Copia archivos entre el host y un contenedor (funciona con contenedores corriendo O
detenidos):

```bash
# Del host al contenedor
docker cp ./local-file.txt my-container:/tmp/

# Del contenedor al host
docker cp my-container:/etc/hostname ./hostname-copy.txt

# Directorios completos
docker cp my-container:/var/log/ ./container-logs/
```

Útil cuando `exec` no es opción (contenedor detenido, sin shell instalada).

## Práctica

### Ejercicio 1: Filtrado y formato

```bash
# Crear varios contenedores
docker run -d --name web1 --label env=prod nginx:latest
docker run -d --name web2 --label env=staging nginx:latest
docker run --name batch debian:bookworm echo "done"

# Filtrar solo los de nginx
docker ps -a --filter ancestor=nginx

# Filtrar por label
docker ps --filter label=env=prod

# Formato tabla personalizada
docker ps -a --format "table {{.Names}}\t{{.Image}}\t{{.Status}}"

# Limpiar
docker rm -f web1 web2 batch
```

### Ejercicio 2: Múltiples shells con exec

```bash
# Arrancar un contenedor de larga vida
docker run -d --name multi-shell debian:bookworm sleep 3600

# En terminal 1: abrir bash
docker exec -it multi-shell bash
# Dentro: echo "shell 1" > /tmp/from-shell1

# En terminal 2: abrir otro bash
docker exec -it multi-shell bash
# Dentro: cat /tmp/from-shell1   → "shell 1"
# Dentro: echo "shell 2" > /tmp/from-shell2

# En terminal 1:
# Dentro: cat /tmp/from-shell2   → "shell 2"
# Ambas shells comparten el mismo filesystem

# Limpiar
docker rm -f multi-shell
```

### Ejercicio 3: Monitoreo con stats

```bash
# Arrancar un contenedor con carga de CPU
docker run -d --name stress debian:bookworm bash -c "while true; do :; done"

# Monitorear
docker stats --no-stream stress
# CPU% debería estar cerca de 100%

# Comparar con un contenedor idle
docker run -d --name idle debian:bookworm sleep 3600
docker stats --no-stream stress idle

# Limpiar
docker rm -f stress idle
```
