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

### Campos explicados

| Campo | Descripción |
|-------|-------------|
| `CONTAINER ID` | Identificador único (12 caracteres hex del hash interno) |
| `IMAGE` | Imagen usada para crear el contenedor |
| `COMMAND` | Comando + argumentos de PID 1 |
| `CREATED` | Hace cuánto se creó el contenedor (no se reinicia) |
| `STATUS` | `Up X minutes` (running) o `Exited (0) X seconds ago` |
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

# Parar y eliminar todos (destructivo)
docker rm -f $(docker ps -aq)

# Guardar IDs antes de hacer algo
docker ps -aq > /tmp/container_ids.txt
```

#### `-l` (latest)

Muestra solo el último contenedor creado:

```bash
# Mostrar solo el último contenedor
docker ps -l
# Útil para saber qué contenedor acabas de crear

# Comúnmente usado con -q para obtener solo el ID
docker ps -lq
```

#### `-n N`

Muestra los últimos N contenedores (independientemente del estado):

```bash
# Los últimos 5 contenedores
docker ps -n 5

# Último contenedor
docker ps -n 1
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
docker ps --filter name=dev
docker ps --filter name=^api-   # nombre que empieza con "api-"

# Por imagen
docker ps --filter ancestor=nginx:latest    # exacta
docker ps --filter ancestor=debian          # cualquier tag de debian

# Por label
docker ps --filter label=environment=production
docker ps --filter label=env               # que tenga cualquier label "env"

# Por exit code
docker ps -a --filter exited=0     # terminaron exitosamente
docker ps -a --filter exited=137   # fueron matados (SIGKILL)
docker ps -a --filter exited=1     # error genérico

# Por volumen montado
docker ps --filter volume=my-volume

# Por red
docker ps --filter network=my-net

# Combinar filtros (AND lógico)
docker ps -a --filter status=exited --filter ancestor=debian

# Excluir (NOT) con filtros --format + grep
docker ps -a --format '{{.Names}}' | grep -v "^db-"
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

# Con ID corto
docker ps --format '{{.Names}} -> {{.ID}}'

# Headers custom
docker ps --format 'CONTAINER: {{.Names}}\tIMAGE: {{.Image}}'
```

Campos disponibles en `--format`:

| Campo | Descripción |
|-------|-------------|
| `.ID` | Container ID (12 caracteres) |
| `.Image` | Nombre de la imagen |
| `.Command` | Comando ejecutado por PID 1 (truncado) |
| `.CreatedAt` | Timestamp de creación |
| `.RunningFor` | Tiempo desde que se creó (ej: "5 minutes ago") |
| `.Status` | Estado actual (Up, Exited, Paused) |
| `.Ports` | Puertos publicados |
| `.Names` | Nombre del contenedor |
| `.Labels` | Todos los labels (separados por coma) |
| `.Label` | Un label específico: `{{.Label "key"}}` |
| `.Size` | Tamaño de la capa de escritura |
| `.Mounts` | Volúmenes montados |
| `.Networks` | Redes conectadas (separados por coma) |
| `.Network` | Una red específica: `{{.Network "netname"}}` |

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
# Ejemplo práctico: crear un archivo en un contenedor y ver el tamaño crecer
docker run -d --name size-test debian:bookworm sleep 300
docker ps -s --filter name=size-test
# Tamaño inicial (muy pequeño, solo metadatos)

docker exec size-test bash -c 'dd if=/dev/zero of=/tmp/bigfile bs=1M count=50'
docker ps -s --filter name=size-test
# Ahora muestra 50MB más en el tamaño del contenedor

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
| Puede sobrescribir CMD | Sí | No (siempre ejecuta comando nuevo) |

`exec` usa un **contenedor existente que está corriendo**. El proceso que lanza comparte:
- El filesystem del contenedor (misma capa de escritura)
- El mismo network namespace (misma IP, mismo hostname)
- Los mismos volúmenes montados
- Los mismos entornos y límites de recursos

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

### Flags específicos de exec

```bash
# -u: usuario
docker exec -u root my-container id
docker exec -u 0 my-container id  # 0 es UID de root

# -w: directorio de trabajo
docker exec -w /tmp my-container pwd

# -e: variable de entorno
docker exec -e MY_VAR=value my-container env | grep MY_VAR

# -it juntos para sesión interactiva
docker exec -it my-container bash

# --privileged: dar privilegios extendidos al exec
docker exec --privileged my-container cat /proc/self/status | grep Cap
```

## `docker logs`

Muestra stdout y stderr del **proceso principal** (PID 1) del contenedor.

```bash
# Ver todos los logs
docker logs my-container

# Follow (como tail -f)
docker logs -f my-container

# Últimas 20 líneas
docker logs --tail 20 my-container

# Últimas 100 líneas y seguir (como tail -100f)
docker logs -f --tail 100 my-container

# Logs desde hace 5 minutos
docker logs --since 5m my-container

# Logs hasta hace 1 minuto
docker logs --until 1m my-container

# Logs desde timestamp específico
docker logs --since "2024-01-15T10:30:00" my-container

# Con timestamps
docker logs -t my-container
# 2024-01-15T10:30:00.123456789Z  mensaje de log...

# Sin timestamp (solo mensajes)
docker logs my-container

# Solo stderr (si el programa los separa)
docker logs my-container 2>&1
```

### Logging drivers

Docker almacen los logs por defecto en JSON en el host:

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

Luego reiniciar Docker: `sudo systemctl restart docker`.

**Nota**: cambiar el logging driver requiere reconstruir los contenedores si quieres
que se aplique a todos.

### Drivers de logging disponibles

| Driver | Descripción |
|--------|-------------|
| `json-file` | Default. JSON en disco (soporta `--log-opt`) |
| `syslog` | Envía logs a syslog del host |
| `journald` | Envía logs a journald (systemd) |
| `none` | Desactiva logging completamente |
| `fluentd` | Envía a fluentd |
| `awslogs` | CloudWatch Logs |
| `gcplogs` | Google Cloud Logging |

```bash
# Usar syslog en vez de json-file
docker run --log-driver=syslog --log-opt syslog-tag=myapp ...

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

# Formato JSON
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
| `MEM USAGE / LIMIT` | Memoria usada / límite de memoria del contenedor |
| `MEM %` | Porcentaje de la memoria asignada |
| `NET I/O` | Bytes enviados / recibidos por red |
| `BLOCK I/O` | Bytes leídos / escritos en disco (capa RW) |
| `PIDS` | Número de procesos dentro del contenedor (incluyendo PID 1 y sus hijos) |

Campos disponibles en `--format`:

| Campo | Descripción |
|-------|-------------|
| `.Container` | Nombre o ID del contenedor |
| `.Name` | Nombre |
| `.CPUPerc` | Porcentaje de CPU |
| `.MemUsage` | Uso de memoria (formato: usado / total) |
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
- Si el PID 1 ya terminó, `attach` se queda colgado

Para desconectarse sin matar el contenedor: **Ctrl+P, Ctrl+Q** (envía el signal de
detach al shell que es PID 1).

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
| Debugging de proceso que murió | `logs` (ya muerto) |
| Attach a un contenedor que se reinicia constantemente | `exec` (más confiable) |

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

# Copiar todo lo que hay en un directorio del contenedor
docker cp my-container:/var/log/. ./container-logs/

# El contenido del contenedor sobreescribe si ya existe en el host
```

Útil cuando `exec` no es opción (contenedor detenido, sin shell instalada, o para
acceder a archivos directamente).

```bash
# Ejemplo: extraer archivos de un contenedor que no tiene shell (scratch-based)
docker cp my-container:/binary-output ./output/

# Backup rápido de logs
docker cp my-container:/var/log/myapp/. ./backups/myapp-logs/

# Restaurar configuración
docker cp ./config.yml my-container:/app/config.yml
```

## `docker diff`

Muestra los cambios en el filesystem del contenedor respecto a la imagen original:

```bash
docker run -d --name diff-test debian:bookworm bash -c 'touch /tmp/newfile && echo "modified" >> /etc/hostname && rm /etc/motd'

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

# Formato JSON
docker ps -a --format json | jq '.'

# Listar solo IDs (para scripting)
docker ps -aq
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
docker stats --no-stream stress idle

# Monitorear múltiples contenedores en tiempo real
docker stats --no-stream --format "table {{.Name}}\t{{.CPUPerc}}\t{{.MemUsage}}"

# Ver PIDs (procesos)
docker exec stress ps aux | wc -l

# Limpiar
docker rm -f stress idle
```

### Ejercicio 4: Logs y debugging

```bash
# Crear contenedor que genera logs
docker run -d --name log-demo nginx:latest

# Ver logs completos
docker logs log-demo

# Ver últimos 5 líneas y seguir
docker logs -f --tail 5 log-demo
# En otra terminal: docker restart log-demo

# Ver timestamps
docker logs -t log-demo

# Limpiar
docker rm -f log-demo
```

### Ejercicio 5: docker diff para inspeccionar cambios

```bash
# Crear contenedor y hacer cambios
docker run -d --name diff-demo debian:bookworm bash -c 'mkdir /data && touch /data/file1.txt && rm /etc/motd'

# Ver qué cambió
docker diff diff-demo
# A /data                 ← directorio creado
# A /data/file1.txt      ← archivo creado
# D /etc/motd            ← archivo eliminado

# Lo mismo pasa si exec creates/modifies/deletes
docker exec diff-demo bash -c 'echo "new content" > /tmp/from-exec.txt'
docker diff diff-demo
# Ahora también muestra: A /tmp/from-exec.txt

docker rm -f diff-demo
```

### Ejercicio 6: Scripting - limpiar todos los contenedores

```bash
# Detener todos los contenedores corriendo
docker stop $(docker ps -q)

# Eliminar todos los contenedores detenidos
docker rm $(docker ps -aq --filter status=exited)

# Versión en una sola línea (cuidadoso!)
docker rm -f $(docker ps -aq)

# Listar solo contenedores que llevan más de 1 día corriendo
docker ps --filter "status=running" --format '{{.Names}}' | while read name; do
  created=$(docker inspect --format '{{.Created}}' "$name")
  echo "$name: created $created"
done

# Eliminar contenedores con cierto patrón de nombre
docker rm -f $(docker ps -aq --filter name=temp-)
```

### Ejercicio 7: Exportar logs para análisis

```bash
# Crear contenedor con logs
docker run -d --name log-archive nginx:latest

# Extraer logs del contenedor al host
docker logs log-archive > /tmp/nginx-logs.txt 2>&1

# Ver solo errores (stderr)
docker logs log-archive 2>&1 | grep -i error

# Ver líneas con timestamp específico
docker logs -t log-archive | grep "2024-01"

# Logs de múltiples contenedores
for container in web1 web2 api; do
  docker logs "$container" > "/tmp/${container}.log" 2>&1
done

# Limpiar
docker rm -f log-archive
```
