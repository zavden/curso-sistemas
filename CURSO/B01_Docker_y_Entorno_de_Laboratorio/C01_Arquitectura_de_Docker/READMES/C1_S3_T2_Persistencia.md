# T02 — Persistencia

## ¿Qué se pierde al destruir un contenedor?

Cuando ejecutas `docker rm`, el contenedor y su **capa de escritura** desaparecen
permanentemente. Todo lo que el proceso escribió fuera de un volumen se pierde:

```
┌──────────────────────────────────────────────┐
│            CONTENEDOR EN EJECUCIÓN           │
│                                              │
│  /app/config.json  (modificado)         ✗    │
│  /var/log/app.log  (creado)             ✗    │
│  /tmp/session_data (creado)             ✗    │
│  /app/data/db.sqlite (creado)           ✗    │
│                                              │
│  ─── capa de escritura (efímera) ───    ✗    │
│                                              │
│  ─── capas de la imagen (read-only) ─── ✓   │
│                                              │
│  /vol/datos  → named volume             ✓    │
│  /app/src    → bind mount               ✓    │
│                                              │
└──────────────────────────────────────────────┘
                     │
               docker rm
                     │
                     ▼
         ✗ = destruido permanentemente
         ✓ = sobrevive
```

### Tabla de supervivencia

| Dato | `docker stop` + `start` | `docker rm` | `docker rm -v` |
|---|---|---|---|
| Capa de escritura | ✓ Sobrevive | ✗ Se pierde | ✗ Se pierde |
| Named volumes | ✓ Sobrevive | ✓ Sobrevive | ✓ Sobrevive |
| Volúmenes anónimos | ✓ Sobrevive | ✓ Sobrevive* | ✗ Se pierde |
| Bind mounts | ✓ Sobrevive | ✓ Sobrevive | ✓ Sobrevive |
| Imagen original | ✓ Intacta | ✓ Intacta | ✓ Intacta |
| Logs (`docker logs`) | ✓ Disponibles | ✗ Se pierden | ✗ Se pierden |

\* Los volúmenes anónimos sobreviven a `docker rm` sin `-v`, pero quedan huérfanos
(sin contenedor asociado). Se acumulan hasta que se limpien con `docker volume prune`.

## `docker stop` + `docker start` vs `docker rm` + `docker run`

Esta es una distinción fundamental que muchos usuarios confunden:

```bash
# Escenario 1: stop + start — preserva la capa de escritura
docker run -d --name myapp alpine sh -c 'echo "datos" > /tmp/test.txt && sleep 3600'
docker stop myapp
docker start myapp
docker exec myapp cat /tmp/test.txt   # "datos" — sigue ahí
docker rm -f myapp

# Escenario 2: rm + run — destruye la capa de escritura
docker run -d --name myapp2 alpine sh -c 'echo "datos" > /tmp/test.txt && sleep 3600'
docker rm -f myapp2
docker run --rm --name myapp2 alpine cat /tmp/test.txt
# cat: can't open '/tmp/test.txt': No such file or directory
```

Con `stop` + `start`, el contenedor conserva su identidad, su filesystem, y sus
datos. Es el equivalente a apagar y encender una máquina. Con `rm` + `run`, se
destruye el contenedor anterior y se crea uno nuevo desde cero.

## Estrategias de persistencia

No todo se persiste de la misma forma. La elección del mecanismo depende del
**tipo de dato** y del **entorno** (desarrollo vs producción):

### Datos de aplicación → Named volumes

Bases de datos, archivos subidos por usuarios, colas de mensajes — cualquier dato
que la aplicación genera y necesita mantener entre reinicios.

```bash
# PostgreSQL con volumen para datos
docker run -d --name postgres \
  -v pgdata:/var/lib/postgresql/data \
  -e POSTGRES_PASSWORD=secret \
  postgres:16

# El volumen pgdata contiene toda la base de datos
# Se puede destruir y recrear el contenedor sin perder datos
docker rm -f postgres
docker run -d --name postgres \
  -v pgdata:/var/lib/postgresql/data \
  -e POSTGRES_PASSWORD=secret \
  postgres:16
# La base de datos está intacta
```

**Regla**: nunca almacenes datos de bases de datos en la capa de escritura. Un
`docker rm` accidental destruiría toda la base de datos.

### Código fuente (desarrollo) → Bind mounts

En desarrollo, quieres editar código en tu editor del host y que el contenedor
vea los cambios inmediatamente:

```bash
# Montar el código fuente del host
docker run -d --name dev \
  -v $(pwd)/src:/app/src \
  -v $(pwd)/config:/app/config:ro \
  -p 8080:8080 \
  node:20 npm run dev

# Editar src/index.js en tu editor favorito
# El contenedor ve los cambios en tiempo real (hot reload si el framework lo soporta)
```

### Configuración → Bind mounts (read-only)

Archivos de configuración que viven en el host y se montan en el contenedor:

```bash
# Nginx con configuración del host
docker run -d --name web \
  -v /home/user/nginx.conf:/etc/nginx/nginx.conf:ro \
  -v /home/user/certs:/etc/nginx/certs:ro \
  -p 443:443 \
  nginx
```

El flag `:ro` (read-only) previene que el contenedor modifique accidentalmente
la configuración del host.

### Logs → Logging driver o bind mount

Docker captura stdout/stderr del proceso principal y los almacena como logs del
contenedor. Pero estos logs se pierden con `docker rm`. Opciones:

```bash
# Opción 1: Logging driver con rotación
docker run -d --log-driver=json-file --log-opt max-size=10m --log-opt max-file=3 \
  --name app myimage

# Opción 2: Bind mount del directorio de logs
docker run -d -v /var/log/myapp:/var/log/app --name app myimage

# Opción 3: Enviar logs a un sistema externo (syslog, fluentd)
docker run -d --log-driver=syslog --log-opt syslog-address=tcp://logserver:514 \
  --name app myimage
```

### Secrets temporales → tmpfs

Secrets como contraseñas, tokens, claves API que no deben tocar disco:

```bash
# Secrets en memoria RAM — nunca se escriben a disco
docker run -d --name app \
  --tmpfs /run/secrets \
  myapp
```

### Tabla resumen de estrategias

| Tipo de dato | Mecanismo | Modo | Ejemplo |
|---|---|---|---|
| Base de datos | Named volume | rw | `-v pgdata:/var/lib/postgresql/data` |
| Archivos de usuario | Named volume | rw | `-v uploads:/app/uploads` |
| Código fuente (dev) | Bind mount | rw | `-v $(pwd)/src:/app/src` |
| Configuración | Bind mount | ro | `-v ./nginx.conf:/etc/nginx/nginx.conf:ro` |
| Certificados TLS | Bind mount | ro | `-v /etc/certs:/etc/certs:ro` |
| Secretos temporales | tmpfs | rw | `--tmpfs /run/secrets` |
| Cache de compilación | tmpfs o named volume | rw | `--tmpfs /tmp/ccache` |
| Logs (stdout) | Logging driver | — | `--log-driver=json-file` |
| Logs (archivos) | Bind mount | rw | `-v /var/log/myapp:/var/log/app` |

## `docker commit` — Capturar la capa de escritura

Es posible crear una nueva imagen a partir del estado actual de un contenedor,
incluyendo su capa de escritura:

```bash
# Instalar herramientas adicionales en un contenedor
docker run -it --name debug debian:bookworm bash
# Dentro del contenedor:
apt-get update && apt-get install -y strace ltrace gdb
exit

# Crear una imagen a partir del contenedor modificado
docker commit debug debug-tools:v1

# Usar la nueva imagen
docker run -it --rm debug-tools:v1 strace -V
```

`docker commit` tiene usos legítimos para **debugging rápido** y **prototipado**,
pero no es una buena práctica para workflows de producción:

| `docker commit` | Dockerfile |
|---|---|
| Estado opaco (no sabes qué cambió) | Pasos documentados y reproducibles |
| No se puede reconstruir automáticamente | `docker build` reproduce la imagen |
| Acumula capas sin optimizar | Multi-stage, caché, optimización |
| Difícil de versionar | Se versiona en Git como código |
| No hay auditoría de cambios | Cada capa tiene historia (`docker history`) |

### Cuándo sí usar docker commit

- Capturar el estado de un contenedor durante debugging
- Snapshot antes de experimentar con cambios destructivos
- Prototipado rápido (luego trasladar a Dockerfile)

### Cuándo NO usar docker commit

- Despliegues de producción
- Compartir imágenes entre equipos
- Cualquier imagen que necesite ser reconstruida

## Instrucción VOLUME en Dockerfile

La instrucción `VOLUME` en un Dockerfile crea un punto de montaje que se
convertirá en un **volumen anónimo** automáticamente en cada `docker run`:

```dockerfile
FROM postgres:16
VOLUME /var/lib/postgresql/data
```

```bash
# Cada docker run crea un nuevo volumen anónimo para /var/lib/postgresql/data
docker run -d postgres:16
docker run -d postgres:16
docker run -d postgres:16

docker volume ls
# Se crearon 3 volúmenes anónimos distintos (uno por contenedor)
```

**Cuidado**: la instrucción `VOLUME` no crea named volumes. Si no especificas
un volumen explícitamente con `-v nombre:/path`, Docker crea un volumen anónimo
con un hash como nombre. Estos se acumulan rápidamente.

Para evitar este problema, siempre especifica un named volume explícitamente:

```bash
# Bien: named volume explícito
docker run -d -v pgdata:/var/lib/postgresql/data postgres:16

# Problemático: se crea un volumen anónimo automáticamente
docker run -d postgres:16
```

### Por qué VOLUME existe

La instrucción `VOLUME` es una **declaración de intención**: dice "este directorio
contiene datos que el usuario probablemente querrá persistir". También asegura que
los datos se escriben en un volumen (fuera de la capa de escritura), lo cual es más
eficiente para escrituras frecuentes como las de una base de datos.

## El patrón stateless vs stateful

### Aplicaciones stateless

No almacenan estado local entre ejecuciones. Si se destruyen y recrean, siguen
funcionando igual:

```
Stateless: nginx, API servers, web servers
└── Todo el estado vive fuera (DB, cache externo, storage)
└── docker rm + docker run = sin impacto
```

### Aplicaciones stateful

Almacenan estado local que debe persistir:

```
Stateful: postgres, mysql, redis, elasticsearch
└── El estado local ES el servicio
└── docker rm sin volumen = pérdida de datos
```

**Patrón de producción**: mantener los contenedores lo más stateless posible,
externalizando todo el estado a volúmenes o servicios externos.

## Inspeccionar montajes de un contenedor

```bash
# Ver todos los mounts en formato JSON
docker inspect --format '{{json .Mounts}}' mycontainer | python3 -m json.tool

# Ver nombre y destino de cada mount
docker inspect --format '{{range .Mounts}}{{.Name}}:{{.Destination}} {{end}}' mycontainer

# Ver si un mount es de solo lectura
docker inspect --format '{{range .Mounts}}{{.Destination}}: {{if .RW}}rw{{else}}ro{{end}} {{end}}' mycontainer
```

Salida de ejemplo:

```json
[
    {
        "Type": "volume",
        "Name": "pgdata",
        "Source": "/var/lib/docker/volumes/pgdata/_data",
        "Destination": "/var/lib/postgresql/data",
        "Driver": "local",
        "Mode": "z",
        "RW": true,
        "Propagation": ""
    }
]
```

| Campo | Significado |
|-------|------------|
| `Type` | `volume`, `bind`, o `tmpfs` |
| `Name` | Nombre del volumen (hash si es anónimo, vacío para bind/tmpfs) |
| `Source` | Ubicación en el host |
| `Destination` | Ubicación dentro del contenedor |
| `RW` | `true` si es lectura/escritura, `false` si es `:ro` |

## Actualización de contenedores con datos persistentes

Un patrón común: actualizar la versión de una imagen sin perder datos.

```bash
# Paso 1: Detener y eliminar el contenedor viejo (el volumen sobrevive)
docker rm -f postgres

# Paso 2: Crear el nuevo contenedor con la imagen actualizada y el mismo volumen
docker run -d --name postgres \
  -v pgdata:/var/lib/postgresql/data \
  -e POSTGRES_PASSWORD=secret \
  postgres:16
# Los datos están intactos
```

> **Importante**: siempre haz backup antes de actualizar versiones mayores de una
> base de datos. Algunas actualizaciones requieren migración del formato de datos.

## Práctica

### Ejercicio 1 — Simular pérdida de datos

```bash
# Crear un contenedor sin volumen y escribir datos
docker run -d --name efimero alpine sh -c 'echo "datos importantes" > /data.txt && sleep 3600'
docker exec efimero cat /data.txt
# datos importantes

# Destruir el contenedor
docker rm -f efimero

# Intentar recuperar los datos (nuevo contenedor, imagen limpia)
docker run --rm alpine cat /data.txt 2>&1
# cat: can't open '/data.txt': No such file or directory
```

### Ejercicio 2 — Patrón de desarrollo con bind mounts

```bash
# Crear un directorio de proyecto
mkdir -p /tmp/dev_test && echo '<h1>Version 1</h1>' > /tmp/dev_test/index.html

# Levantar nginx con bind mount
docker run -d --name dev_web \
  -v /tmp/dev_test:/usr/share/nginx/html:ro \
  -p 8080:80 nginx

# Verificar
curl http://localhost:8080
# <h1>Version 1</h1>

# Editar en el host (simula tu editor)
echo '<h1>Version 2</h1>' > /tmp/dev_test/index.html

# Verificar que el cambio se refleja inmediatamente
curl http://localhost:8080
# <h1>Version 2</h1>

# Limpiar
docker rm -f dev_web
rm -rf /tmp/dev_test
```

### Ejercicio 3 — Volúmenes anónimos huérfanos

```bash
# Crear 3 contenedores que generan volúmenes anónimos
# (postgres tiene VOLUME declarado en su Dockerfile)
for i in 1 2 3; do
  docker run -d --name "pg$i" -e POSTGRES_PASSWORD=test postgres:16-alpine
done

# Esperar a que arranquen
sleep 3

# Ver volúmenes creados (3 anónimos + posiblemente otros)
docker volume ls

# Eliminar contenedores SIN -v
docker rm -f pg1 pg2 pg3

# Los volúmenes anónimos siguen ahí (huérfanos)
docker volume ls -f dangling=true

# Limpiar
docker volume prune -f
```

### Ejercicio 4 — docker commit para debugging

```bash
# Crear contenedor interactivo y modificarlo
docker run -it --name commit-test debian:bookworm bash
# Dentro del contenedor:
apt-get update && apt-get install -y curl vim
exit

# Commit para crear imagen (contenedor ya exited)
docker commit commit-test my-debug-tools:v1

# Verificar que la imagen tiene las herramientas
docker run --rm my-debug-tools:v1 curl --version
docker run --rm my-debug-tools:v1 vim --version | head -1

# Comparar con docker history — no se ve qué se instaló
docker history my-debug-tools:v1

# Limpiar
docker rm commit-test
docker rmi my-debug-tools:v1
```

### Ejercicio 5 — Actualización de contenedor con volumen

```bash
# Crear un contenedor con named volume y escribir datos
docker run --rm -v testdata:/data alpine sh -c '
  echo "version 1 data" > /data/info.txt
  echo "important records" > /data/records.txt
'

# Verificar datos
docker run --rm -v testdata:/data alpine cat /data/info.txt
# version 1 data

# Simular "actualización" — nuevo contenedor, mismo volumen
docker run --rm -v testdata:/data alpine sh -c '
  cat /data/info.txt
  echo "version 2 data" > /data/info.txt
  cat /data/info.txt
'
# version 1 data
# version 2 data

# Los datos originales que no tocamos siguen intactos
docker run --rm -v testdata:/data alpine cat /data/records.txt
# important records

# Limpiar
docker volume rm testdata
```

### Ejercicio 6 — Inspeccionar mounts de múltiples contenedores

```bash
# Crear contenedores con diferentes mount types
docker run -d --name vol-container -v myvol:/data alpine sleep 300
docker run -d --name bind-container -v /tmp:/hostdata alpine sleep 300
docker run -d --name tmpfs-container --tmpfs /tmp:size=50m alpine sleep 300

# Inspeccionar cada uno
for name in vol-container bind-container tmpfs-container; do
  echo "=== $name ==="
  docker inspect --format '{{range .Mounts}}Type: {{.Type}}, Dest: {{.Destination}}, RW: {{.RW}}
{{end}}' "$name"
done

# Limpiar
docker rm -f vol-container bind-container tmpfs-container
docker volume rm myvol
```

### Ejercicio 7 — Estrategia de logs con bind mount

```bash
# Bind mount para persistir logs fuera de la capa de escritura
mkdir -p /tmp/app-logs

docker run -d --name log-test \
  -v /tmp/app-logs:/var/log/app \
  alpine sh -c 'while true; do echo "log $(date)" >> /var/log/app/app.log; sleep 1; done'

# Esperar unos segundos para que genere logs
sleep 5

# Ver logs directamente en el host (sin docker logs)
tail -5 /tmp/app-logs/app.log

# Destruir el contenedor — los logs sobreviven
docker rm -f log-test
cat /tmp/app-logs/app.log | wc -l
# Los logs siguen en el host

# Limpiar
rm -rf /tmp/app-logs
```
