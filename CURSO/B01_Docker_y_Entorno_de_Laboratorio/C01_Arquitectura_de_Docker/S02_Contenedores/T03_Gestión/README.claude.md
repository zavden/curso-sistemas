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

### Campos de la salida

| Campo | Descripción |
|-------|-------------|
| `CONTAINER ID` | Identificador único (12 caracteres hex del hash completo) |
| `IMAGE` | Imagen usada para crear el contenedor |
| `COMMAND` | Comando + argumentos de PID 1 (truncado) |
| `CREATED` | Hace cuánto se creó el contenedor (no se reinicia con start) |
| `STATUS` | `Up X minutes` (running), `Exited (N) X ago`, `Up X (Paused)` |
| `PORTS` | Puertos publicados (`-p`), formato `host:container/proto` |
| `NAMES` | Nombre del contenedor (único, generado o asignado con `--name`) |

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

# Eliminar todos los contenedores (destructivo)
docker rm -f $(docker ps -aq)
```

#### `-l` (latest)

Muestra solo el último contenedor creado:

```bash
docker ps -l
# Útil para saber qué contenedor acabas de crear

# Combinado con -q para obtener solo el ID
docker ps -lq
```

#### `-n N`

Muestra los últimos N contenedores (independientemente del estado):

```bash
# Los últimos 5 contenedores
docker ps -n 5
```

#### `--filter`

Filtra contenedores por criterios específicos:

```bash
# Por estado
docker ps -a --filter status=exited
docker ps -a --filter status=running
docker ps -a --filter status=paused
docker ps -a --filter status=created

# Por nombre (soporta expresiones regulares parciales)
docker ps --filter name=web
docker ps --filter name=^api-   # nombre que empieza con "api-"

# Por imagen
docker ps --filter ancestor=nginx:latest    # tag exacto
docker ps --filter ancestor=debian          # cualquier tag de debian

# Por label
docker ps --filter label=environment=production
docker ps --filter label=env               # que tenga el label "env" (cualquier valor)

# Por exit code
docker ps -a --filter exited=0     # terminaron exitosamente
docker ps -a --filter exited=137   # fueron matados (SIGKILL)

# Por volumen o red
docker ps --filter volume=my-volume
docker ps --filter network=my-net

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

# JSON (un objeto por contenedor)
docker ps --format json

# Solo nombres (útil para scripts)
docker ps --format '{{.Names}}'

# Nombre con ID
docker ps --format '{{.Names}} -> {{.ID}}'
```

Campos disponibles en `--format`:

| Campo | Descripción |
|-------|-------------|
| `.ID` | Container ID (12 caracteres) |
| `.Image` | Nombre de la imagen |
| `.Command` | Comando de PID 1 (truncado) |
| `.CreatedAt` | Timestamp de creación |
| `.RunningFor` | Tiempo desde que se creó (ej: "5 minutes ago") |
| `.Status` | Estado actual (Up, Exited, Paused) |
| `.Ports` | Puertos publicados |
| `.Names` | Nombre del contenedor |
| `.Labels` | Todos los labels (separados por coma) |
| `.Label` | Un label específico: `{{.Label "key"}}` |
| `.Size` | Tamaño de la capa de escritura (requiere `-s`) |
| `.Mounts` | Volúmenes montados |
| `.Networks` | Redes conectadas |

#### `-s` (size)

Muestra el tamaño de la capa de escritura de cada contenedor:

```bash
docker ps -s
# ... SIZE
# ... 5B (virtual 117MB)
```

- El primer número es el **tamaño de la capa de escritura** (datos únicos del contenedor)
- `virtual` es el **tamaño total** (imagen + capa de escritura)

```bash
# Ejemplo: ver cómo crece el tamaño al escribir datos
docker run -d --name size-test debian:bookworm sleep 300
docker ps -s --filter name=size-test
# Tamaño inicial (muy pequeño)

docker exec size-test bash -c 'dd if=/dev/zero of=/tmp/bigfile bs=1M count=50'
docker ps -s --filter name=size-test
# Ahora muestra ~50MB en el tamaño del contenedor

docker rm -f size-test
```

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

# Ejecutar múltiples comandos
docker exec my-container bash -c 'ls / && pwd'
```

### Diferencia clave con `docker run`

| Aspecto | `docker run` | `docker exec` |
|---------|-------------|---------------|
| Crea contenedor | Sí (nuevo) | No |
| Contenedor debe estar corriendo | No | **Sí** |
| Proceso ejecutado es PID 1 | Sí | No (nuevo PID) |
| Múltiples ejecuciones simultáneas | Una (es PID 1) | Ilimitadas |
| Puede sobrescribir CMD | Sí | No (ejecuta comando nuevo) |

`exec` usa un **contenedor existente que está corriendo**. El proceso que lanza comparte:
- El filesystem del contenedor (misma capa de escritura)
- El mismo network namespace (misma IP, mismo hostname)
- Los mismos volúmenes montados

Pero tiene su propio PID. Ctrl+C en un `exec` mata solo el proceso
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

# Últimas 100 líneas y seguir
docker logs -f --tail 100 my-container

# Logs desde hace 5 minutos
docker logs --since 5m my-container

# Logs hasta hace 1 minuto
docker logs --until 1m my-container

# Logs desde un timestamp específico
docker logs --since "2024-01-15T10:30:00" my-container

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
se llena, rota automáticamente (el más antiguo se elimina).

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

Luego reiniciar Docker: `sudo systemctl restart docker`. Los contenedores existentes
no se ven afectados — solo los nuevos usarán la configuración.

#### Drivers de logging disponibles

| Driver | Descripción |
|--------|-------------|
| `json-file` | Default. JSON en disco (soporta rotación con `--log-opt`) |
| `syslog` | Envía logs a syslog del host |
| `journald` | Envía logs a journald (systemd) |
| `none` | Desactiva logging (`docker logs` no funciona) |
| `fluentd` | Envía a fluentd |
| `awslogs` | CloudWatch Logs (AWS) |
| `gcplogs` | Google Cloud Logging |

```bash
# Usar journald
docker run --log-driver=journald --log-opt tag=myapp ...

# Desactivar logs (cuidado: no hay forma de verlos después)
docker run --log-driver=none myapp:latest
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
# from exec  ← aparece aquí, en tu terminal, NO en docker logs

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

# Formato para scripting (sin header)
docker stats --no-stream --format "{{.Name}}: {{.CPUPerc}}"

# JSON
docker stats --no-stream --format json
```

Salida típica:

```
CONTAINER ID   NAME   CPU %   MEM USAGE / LIMIT     MEM %   NET I/O        BLOCK I/O     PIDS
a1b2c3d4e5f6   web    0.02%   5.5MiB / 15.6GiB      0.03%   1.2kB / 650B   0B / 4.1kB    5
```

| Columna | Significado |
|---------|------------|
| `CPU %` | Porcentaje de CPU usado (puede > 100% en multi-core) |
| `MEM USAGE / LIMIT` | Memoria usada / límite del contenedor |
| `MEM %` | Porcentaje de la memoria asignada |
| `NET I/O` | Bytes enviados / recibidos por red |
| `BLOCK I/O` | Bytes leídos / escritos en disco |
| `PIDS` | Número de procesos dentro del contenedor |

Campos disponibles en `--format`:

| Campo | Descripción |
|-------|-------------|
| `.Container` | Nombre o ID |
| `.Name` | Nombre |
| `.CPUPerc` | Porcentaje de CPU |
| `.MemUsage` | Uso de memoria (usado / límite) |
| `.MemPerc` | Porcentaje de memoria |
| `.NetIO` | I/O de red |
| `.BlockIO` | I/O de bloque |
| `.PIDs` | Número de PIDs |

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
- Si PID 1 ya terminó, `attach` se queda colgado

Para desconectarse sin matar el contenedor: **Ctrl+P, Ctrl+Q** (secuencia de detach).

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
| Ver la salida en tiempo real de PID 1 | `logs -f` (mejor que attach) |
| Enviar input a PID 1 interactivo | `attach` |
| Contenedor que se reinicia constantemente | `exec` (más confiable) |

**Regla práctica**: usar `exec` siempre para tareas de gestión. Usar `attach` casi nunca.

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

# Copiar el contenido de un directorio (sin el directorio padre)
docker cp my-container:/var/log/. ./container-logs/
```

Útil cuando `exec` no es opción (contenedor detenido, imagen sin shell, o para
acceder a archivos directamente).

```bash
# Extraer archivos de una imagen scratch-based (sin shell)
docker cp my-container:/binary-output ./output/

# Backup rápido de logs
docker cp my-container:/var/log/myapp/. ./backups/myapp-logs/
```

## `docker diff`

Muestra los cambios en el filesystem del contenedor respecto a la imagen original:

```bash
docker run -d --name diff-test debian:bookworm bash -c '
  touch /tmp/newfile &&
  echo "modified" >> /etc/hostname &&
  rm /etc/motd &&
  sleep 300
'

docker diff diff-test
# A /tmp/newfile              ← Added
# C /etc/hostname             ← Changed
# D /etc/motd                 ← Deleted

docker rm -f diff-test
```

| Símbolo | Significado |
|---------|------------|
| `A` | Archivo añadido (Added) |
| `C` | Archivo modificado (Changed) |
| `D` | Archivo eliminado (Deleted) |

Los cambios hechos con `docker exec` también aparecen en `docker diff`, porque
comparten la misma capa de escritura.

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

# Filtrar por estado
docker ps -a --filter status=exited

# Formato tabla personalizada
docker ps -a --format "table {{.Names}}\t{{.Image}}\t{{.Status}}\t{{.Ports}}"

# Solo IDs (para scripting)
docker ps -aq --filter status=exited

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

# Desde el host: ver procesos dentro del contenedor
docker exec multi-shell ps aux

# Limpiar
docker rm -f multi-shell
```

### Ejercicio 3: Monitoreo con stats

```bash
# Arrancar un contenedor con carga de CPU
docker run -d --name stress debian:bookworm bash -c "while true; do :; done"

# Monitorear una vez
docker stats --no-stream stress
# CPU% debería estar cerca de 100% (consume 1 core)

# Comparar con un contenedor idle
docker run -d --name idle debian:bookworm sleep 3600
docker stats --no-stream --format "table {{.Name}}\t{{.CPUPerc}}\t{{.MemUsage}}" stress idle

# Limpiar
docker rm -f stress idle
```

### Ejercicio 4: Logs y debugging

```bash
# Crear contenedor que genera logs
docker run -d --name log-demo nginx:latest

# Ver logs completos
docker logs log-demo

# Ver últimas 5 líneas y seguir
docker logs -f --tail 5 log-demo
# (Ctrl+C para salir del follow)

# Ver timestamps
docker logs -t log-demo

# Exportar logs a un archivo
docker logs log-demo > /tmp/nginx-stdout.log 2>/tmp/nginx-stderr.log

# Limpiar
docker rm -f log-demo
```

### Ejercicio 5: docker diff para inspeccionar cambios

```bash
# Crear contenedor y hacer cambios
docker run -d --name diff-demo debian:bookworm bash -c '
  mkdir /data &&
  touch /data/file1.txt &&
  rm /etc/motd &&
  sleep 300
'

# Ver qué cambió
docker diff diff-demo
# A /data
# A /data/file1.txt
# D /etc/motd

# Los cambios con exec también aparecen
docker exec diff-demo bash -c 'echo "new" > /tmp/from-exec.txt'
docker diff diff-demo
# Ahora también muestra: A /tmp/from-exec.txt

docker rm -f diff-demo
```

### Ejercicio 6: Scripting — limpiar contenedores

```bash
# Detener todos los contenedores corriendo
docker stop $(docker ps -q)

# Eliminar todos los contenedores detenidos
docker rm $(docker ps -aq --filter status=exited)

# Eliminar contenedores con cierto patrón de nombre
docker rm -f $(docker ps -aq --filter name=temp-)
```
