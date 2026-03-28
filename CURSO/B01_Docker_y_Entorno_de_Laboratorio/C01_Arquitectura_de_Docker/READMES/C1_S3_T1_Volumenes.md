# T01 — Volúmenes

## El problema del almacenamiento en contenedores

Cada contenedor tiene una **capa de escritura** efímera. Cuando el contenedor se
elimina (`docker rm`), esa capa desaparece junto con todo lo que se escribió en ella.
Esto es un problema real para cualquier aplicación que necesite **persistir datos**:
bases de datos, archivos de configuración, logs, código fuente en desarrollo, etc.

```
Problema:
┌─────────────────────────────────────────┐
│ Contenedor                              │
│  ┌─────────────────┐                    │
│  │  Capa RW (upperdir)  │  ← Datos     │
│  ├─────────────────┤      escritos se   │
│  │  Capas imagen   │      pierden al    │
│  └─────────────────┘      eliminar      │
└─────────────────────────────────────────┘
              │
              ▼ docker rm
┌─────────────────────────────────────────┐
│  Capas de imagen    ← Se conservan      │
│  Capa RW            ← SE DESTRUYE       │
└─────────────────────────────────────────┘
```

Docker resuelve esto con tres mecanismos de montaje que conectan el filesystem del
contenedor con almacenamiento externo a la capa de escritura.

## Los tres tipos de montaje

```
┌─────────────────────────────────────────────────────────┐
│                     CONTENEDOR                          │
│                                                         │
│   /app/data ──────┐    /app/src ──────┐    /tmp/cache   │
│                   │                   │        │        │
└───────────────────┼───────────────────┼────────┼────────┘
                    │                   │        │
                    ▼                   ▼        ▼
            ┌──────────────┐   ┌──────────┐  ┌──────┐
            │ Named Volume │   │   Bind   │  │ tmpfs│
            │              │   │  Mount   │  │      │
            │ /var/lib/    │   │          │  │ RAM  │
            │ docker/      │   │ /home/   │  │      │
            │ volumes/     │   │ user/    │  │      │
            │ mydata/_data │   │ project/ │  │      │
            └──────────────┘   └──────────┘  └──────┘
              Gestionado         Path del     Memoria
              por Docker         host         volátil
```

### 1. Named volumes

Son el **mecanismo recomendado** por Docker para persistir datos. Docker gestiona
completamente su ciclo de vida.

```bash
# Crear un named volume explícitamente
docker volume create mydata

# Usar un named volume (se crea automáticamente si no existe)
docker run -v mydata:/app/data alpine

# Listar volúmenes
docker volume ls

# Inspeccionar un volumen
docker volume inspect mydata
```

Salida de `docker volume inspect`:

```json
[
    {
        "CreatedAt": "2024-01-15T10:30:00Z",
        "Driver": "local",
        "Labels": {},
        "Mountpoint": "/var/lib/docker/volumes/mydata/_data",
        "Name": "mydata",
        "Options": {},
        "Scope": "local"
    }
]
```

**Características clave**:

| Propiedad | Comportamiento |
|---|---|
| Ubicación | `/var/lib/docker/volumes/<nombre>/_data` |
| Gestión | Docker (`docker volume` subcomandos) |
| Persistencia | Sobrevive a `docker rm` del contenedor |
| Compartir | Múltiples contenedores pueden montar el mismo volumen |
| Inicialización | Copia contenido de la imagen la primera vez (ver sección abajo) |
| Backup | `docker run --rm -v mydata:/data -v $(pwd):/backup alpine tar czf /backup/backup.tar.gz /data` |
| Portabilidad | Se pueden usar drivers remotos (NFS, cloud storage) |

### 2. Bind mounts

Montan un **directorio o archivo del host** directamente en el contenedor. El
contenedor ve exactamente los mismos archivos que existen en el host.

```bash
# Montar el directorio actual en /app del contenedor
docker run -v $(pwd):/app alpine ls /app

# Montar un archivo específico
docker run -v /etc/localtime:/etc/localtime:ro alpine date

# Montar en modo solo lectura
docker run -v $(pwd):/app:ro alpine cat /app/README.md
```

**Características clave**:

| Propiedad | Comportamiento |
|---|---|
| Ubicación | Cualquier path del host |
| Gestión | El usuario (no aparecen en `docker volume ls`) |
| Caso de uso principal | Desarrollo (código fuente), configuración |
| Contenido de la imagen | Se **oculta** bajo el bind mount (se ve el contenido del host) |
| Rendimiento | Nativo en Linux, con overhead en macOS/Windows (Docker Desktop usa una VM) |
| Permisos | Hereda UIDs/GIDs del host (puede causar problemas — ver sección abajo) |

### 3. tmpfs mounts

Filesystem en **memoria RAM**. Los datos no se escriben a disco y desaparecen
cuando el contenedor se detiene o se elimina.

```bash
# Montar tmpfs en /tmp/cache
docker run --tmpfs /tmp/cache alpine df -h /tmp/cache

# Con opciones de tamaño y permisos
docker run --tmpfs /tmp/cache:size=100m,mode=1777 alpine df -h /tmp/cache

# Con --mount (más explícito)
docker run --mount type=tmpfs,destination=/tmp/cache,tmpfs-size=100m,tmpfs-mode=1777 alpine df -h /tmp/cache
```

**Casos de uso**:
- Datos sensibles que no deben tocar disco (tokens, secrets temporales)
- Caches de alta velocidad
- Archivos temporales de compilación
- Buffers de trabajo que no necesitan persistencia

**Limitaciones**:
- Consume memoria del host (o de la VM de Docker Desktop)
- No hay forma de inspeccionar el contenido desde fuera del contenedor
- Los datos desaparecen al detener el contenedor (no solo al eliminarlo)

## Sintaxis: `-v` vs `--mount`

Docker ofrece dos formas de especificar montajes. Hacen lo mismo, pero con
diferencias sutiles en comportamiento ante errores.

### `-v` (forma corta)

```bash
# Named volume
docker run -v mydata:/app/data alpine

# Bind mount (requiere path absoluto o ./)
docker run -v /home/user/project:/app alpine
docker run -v ./src:/app/src alpine

# Solo lectura
docker run -v mydata:/app/data:ro alpine

# Volumen anónimo (sin nombre)
docker run -v /app/data alpine
```

### `--mount` (forma explícita)

```bash
# Named volume
docker run --mount type=volume,source=mydata,target=/app/data alpine

# Bind mount
docker run --mount type=bind,source=/home/user/project,target=/app alpine

# Solo lectura
docker run --mount type=volume,source=mydata,target=/app/data,readonly alpine

# tmpfs
docker run --mount type=tmpfs,destination=/tmp/cache alpine
```

### Diferencias críticas

| Comportamiento | `-v` | `--mount` |
|---|---|---|
| Si el source no existe (volume) | Lo crea automáticamente | Lo crea automáticamente |
| Si el source no existe (bind) | **Crea el directorio silenciosamente** | **Error explícito** |
| Legibilidad | Compacta | Explícita |
| Opciones avanzadas | Limitado | Completo (tmpfs-size, tmpfs-mode, bind-propagation, etc.) |
| Documentación oficial | Recomendada para uso simple | Recomendada para producción |

La diferencia más importante: con `-v`, si especificas un bind mount a un path
que no existe en el host, Docker **crea el directorio silenciosamente**. Con
`--mount`, Docker **falla con un error explícito**. Esto puede ahorrarte horas
de debugging cuando tienes un typo en un path.

```bash
# -v: crea /home/user/proejct (typo) sin avisar
docker run -v /home/user/proejct:/app alpine ls /app
# /app está vacío, sin error — difícil de diagnosticar

# --mount: falla inmediatamente
docker run --mount type=bind,source=/home/user/proejct,target=/app alpine ls /app
# Error: bind mount source path does not exist: /home/user/proejct
```

## Volúmenes anónimos

Se crean cuando usas `-v` solo con el path destino (sin especificar un nombre):

```bash
# Esto crea un volumen anónimo
docker run -v /app/data alpine

# También se crean con la instrucción VOLUME en Dockerfile
# VOLUME /app/data
```

```bash
$ docker volume ls
DRIVER    VOLUME NAME
local     a3f1c2d4e5...  # Volumen anónimo — nombre hash ilegible
local     mydata          # Named volume — nombre legible
```

**Problema**: los volúmenes anónimos son difíciles de rastrear. Si no los eliminas
explícitamente (con `docker rm -v` o `docker volume prune`), se acumulan y
consumen espacio en disco sin que sea obvio a qué contenedor pertenecían.

## Comportamiento de copia inicial (named volumes)

Cuando un named volume se monta sobre un directorio que **ya contiene datos en la
imagen**, Docker copia esos datos al volumen la primera vez:

```bash
# Nginx tiene archivos en /usr/share/nginx/html
docker run -v web_content:/usr/share/nginx/html nginx

# El volumen web_content ahora contiene index.html y 50x.html
# que estaban en la imagen original
docker run --rm -v web_content:/data alpine ls /data
# 50x.html  index.html
```

Este comportamiento **solo ocurre con named volumes** y **solo la primera vez**
(si el volumen ya tiene datos, no se sobrescriben).

Los bind mounts **no** copian datos — simplemente ocultan lo que había en la imagen:

```bash
# Bind mount oculta el contenido original de /usr/share/nginx/html
mkdir -p /tmp/empty_dir
docker run -v /tmp/empty_dir:/usr/share/nginx/html nginx ls /usr/share/nginx/html
# (vacío — lo que había en la imagen está oculto)

rm -rf /tmp/empty_dir
```

### Tabla comparativa de inicialización

| Tipo | Copia datos de la imagen | Oculta datos de la imagen | Si el volumen ya tiene datos |
|------|--------------------------|---------------------------|------------------------------|
| Named volume | Sí (solo la primera vez) | No | Usa datos existentes del volumen |
| Bind mount | No | Sí (totalmente) | Muestra lo que hay en el host |
| tmpfs | No | No | Siempre vacío |

## Permisos y propiedad (bind mounts)

Un problema común con bind mounts es que los UIDs/GIDs del host se pasan
literalmente al contenedor. Si el UID no tiene entrada en `/etc/passwd` del
contenedor, se muestra como número:

```bash
# En el host: archivo propiedad de UID 1000
$ ls -ln ./project/file.txt
-rw-r--r--  1  1000  1000  128 Jan 15 10:30  file.txt

# Dentro del contenedor (Alpine): UID 1000 no tiene nombre
$ docker run -v $(pwd)/project:/app alpine ls -ln /app
-rw-r--r--  1  1000  1000  128 Jan 15 10:30  file.txt
# Muestra "1000:1000" (numérico, porque UID 1000 no existe en el /etc/passwd de Alpine)
```

**Soluciones**:

```bash
# 1. Ejecutar el contenedor con el mismo UID del host
docker run -v $(pwd):/app --user 1000:1000 alpine touch /app/newfile
# El archivo se crea con UID 1000 tanto en el host como en el contenedor

# 2. Cambiar permisos en el host antes de montar
sudo chown -R 1000:1000 ./project
docker run -v $(pwd)/project:/app alpine ls -la /app

# 3. Usar --mount con solo lectura cuando no necesitas escribir
docker run --mount type=bind,source=$(pwd),target=/app,readonly alpine cat /app/config.yml
```

> **Nota**: Este tema se cubre en detalle en T03_Permisos_en_bind_mounts.

## Gestión de volúmenes

```bash
# Crear
docker volume create mydata

# Listar
docker volume ls

# Listar con filtro
docker volume ls -f dangling=true   # Volúmenes sin contenedores asociados

# Inspeccionar
docker volume inspect mydata

# Eliminar un volumen específico
docker volume rm mydata

# Eliminar todos los volúmenes sin usar (dangling)
docker volume prune

# Eliminar TODOS los volúmenes no usados por ningún contenedor
docker volume prune -a
```

### Volúmenes huérfanos (dangling)

Un volumen huérfano es uno que no está montado en ningún contenedor pero sigue
ocupando espacio:

```bash
# Encontrar volúmenes huérfanos
docker volume ls -f dangling=true

# Ver espacio ocupado por volúmenes
docker system df -v | grep -A 5 "Volumes"

# Eliminar todos los huérfanos (con confirmación)
docker volume prune

# Eliminar sin confirmación
docker volume prune -f
```

## Compartir volúmenes entre contenedores

Un named volume puede montarse en **múltiples contenedores simultáneamente**:

```bash
# Contenedor que escribe
docker run -d --name writer -v shared:/data alpine sh -c \
  'while true; do date >> /data/log.txt; sleep 2; done'

# Contenedor que lee (en otra terminal)
docker run --rm -v shared:/data alpine tail -f /data/log.txt
```

No hay mecanismo de locking integrado. Si múltiples contenedores escriben al mismo
archivo simultáneamente, pueden ocurrir race conditions. La sincronización es
responsabilidad de la aplicación.

### `--volumes-from`

Copia los mount points de otro contenedor (hereda sus volúmenes):

```bash
# Crear un contenedor con volúmenes
docker run -d --name db -v dbdata:/var/lib/postgresql/data postgres

# Otro contenedor hereda los mismos mounts
docker run --rm --volumes-from db alpine ls /var/lib/postgresql/data
```

## Drivers de volumen

El driver por defecto es `local` (almacena en el filesystem del host). Existen
drivers para almacenamiento remoto:

```bash
# Crear un volumen con driver NFS
docker volume create --driver local \
  --opt type=nfs \
  --opt o=addr=192.168.1.100,rw \
  --opt device=:/export/data \
  nfs_data

# Driver CIFS/SMB (Windows file sharing)
docker volume create --driver local \
  --opt type=cifs \
  --opt o=username=user,password=pass,domain=WORKGROUP \
  --opt device=//server/share \
  smb_data
```

Drivers comunes:

| Driver | Uso |
|--------|-----|
| `local` | Default, almacenamiento en disco local |
| `local` (tipo nfs) | Network File System (con `--opt type=nfs`) |
| `local` (tipo cifs) | SMB/Windows file shares (con `--opt type=cifs`) |
| plugins (rexray, etc.) | Multi-backend cloud (EBS, Azure Disk, GCP PD) |

> **Nota**: NFS y CIFS no son drivers separados — se configuran como opciones del
> driver `local`. Los drivers de terceros (como rexray) se instalan como plugins
> de Docker.

## Casos de uso recomendados

| Caso | Tipo recomendado | Ejemplo |
|------|-----------------|---------|
| Base de datos | Named volume | `-v postgres_data:/var/lib/postgresql/data` |
| Logs de aplicación | Bind mount o named volume | `-v /var/log/myapp:/var/log/myapp` |
| Código fuente (desarrollo) | Bind mount | `-v $(pwd):/app` |
| Configuración del host | Bind mount (ro) | `-v /etc/myapp.conf:/app.conf:ro` |
| Secrets temporales | tmpfs | `--tmpfs /run/secrets` |
| Cache de compilación | tmpfs o named volume | `--tmpfs /tmp/ccache` |
| Archivos estáticos web | Named volume | `-v nginx_html:/usr/share/nginx/html` |

## Práctica

### Ejercicio 1 — Persistencia con named volume

Demostrar que un named volume sobrevive a la destrucción del contenedor:

```bash
# 1. Crear un contenedor que escribe datos en un named volume
docker run --rm -v test_data:/data alpine sh -c 'echo "datos persistentes" > /data/test.txt'

# 2. Verificar que los datos persisten (nuevo contenedor, mismo volumen)
docker run --rm -v test_data:/data alpine cat /data/test.txt
# datos persistentes

# 3. Crear otro contenedor que añade más datos
docker run --rm -v test_data:/data alpine sh -c 'echo "más datos" >> /data/test.txt'

# 4. Verificar que ambos persisten
docker run --rm -v test_data:/data alpine cat /data/test.txt
# datos persistentes
# más datos

# 5. Limpiar
docker volume rm test_data
```

### Ejercicio 2 — Diferencia entre bind mount y named volume

```bash
# Named volume: copia el contenido preexistente de la imagen
docker run --rm -v nginx_vol:/usr/share/nginx/html nginx ls /usr/share/nginx/html
# Verás: 50x.html  index.html

# Bind mount: oculta el contenido preexistente
mkdir /tmp/empty_dir
docker run --rm -v /tmp/empty_dir:/usr/share/nginx/html nginx ls /usr/share/nginx/html
# Verás: (vacío)

# Limpiar
docker volume rm nginx_vol
rm -rf /tmp/empty_dir
```

### Ejercicio 3 — tmpfs mount

```bash
# Crear un contenedor con tmpfs
docker run --rm --tmpfs /tmp/cache:size=50m alpine sh -c \
  'echo "temporal" > /tmp/cache/test.txt && cat /tmp/cache/test.txt && df -h /tmp/cache'

# Verificar que no persiste (nuevo contenedor sin tmpfs)
docker run --rm alpine cat /tmp/cache/test.txt 2>&1 || echo "No existe: tmpfs es volátil"

# Con --mount para más control
docker run --rm --mount type=tmpfs,destination=/tmp/cache,tmpfs-size=50m,tmpfs-mode=1777 alpine \
  sh -c 'echo "test" > /tmp/cache/file && df -h /tmp/cache'
```

### Ejercicio 4 — Diferencia entre `-v` y `--mount` con path inexistente

```bash
# -v crea el directorio silenciosamente
docker run --rm -v /tmp/no_existe_xyz:/app alpine ls -la /app
ls -la /tmp/no_existe_xyz  # Se creó en el host

# --mount falla explícitamente
docker run --rm --mount type=bind,source=/tmp/no_existe_abc,target=/app alpine ls /app
# Error: source path does not exist

# Limpiar
rm -rf /tmp/no_existe_xyz
```

### Ejercicio 5 — Compartir volumen entre contenedores

```bash
# Crear volumen compartido
docker volume create shared-data

# Contenedor 1: escribir datos
docker run -d --name producer -v shared-data:/data alpine \
  sh -c 'for i in 1 2 3; do echo "Line $i from producer" >> /data/file.txt; sleep 1; done'

# Esperar a que escriba datos
sleep 4

# Contenedor 2: leer datos
docker run --rm -v shared-data:/data alpine cat /data/file.txt
# Line 1 from producer
# Line 2 from producer
# Line 3 from producer

# Contenedor 3: añadir más datos
docker run --rm -v shared-data:/data alpine sh -c 'echo "From consumer" >> /data/file.txt'

# Ver resultado final
docker run --rm -v shared-data:/data alpine cat /data/file.txt

# Limpiar
docker rm -f producer
docker volume rm shared-data
```

### Ejercicio 6 — Permisos con bind mount

```bash
# Crear directorio con permisos específicos en el host
mkdir -p /tmp/perm-test
echo "host content" > /tmp/perm-test/file.txt

# Ver permisos en el host
ls -ln /tmp/perm-test/file.txt
# Muestra UID:GID del host (probablemente 1000:1000)

# Montar en contenedor — UIDs se preservan tal cual
docker run --rm -v /tmp/perm-test:/app alpine ls -ln /app
# Muestra el mismo UID:GID numérico

# Con usuario específico que coincide con el del host
docker run --rm -v /tmp/perm-test:/app --user 1000:1000 alpine touch /app/newfile
ls -ln /tmp/perm-test/newfile
# Creado con UID:GID 1000:1000

# Limpiar
rm -rf /tmp/perm-test
```

### Ejercicio 7 — Backup y restauración de volumen

```bash
# Crear datos de prueba
docker run --rm -v db_backup:/data alpine sh -c \
  'echo "important data" > /data/db.sql && echo "schema" > /data/schema.sql'

# Backup usando un contenedor temporal
docker run --rm -v db_backup:/data -v $(pwd):/backup alpine \
  tar czf /backup/db-backup.tar.gz -C /data .

# Verificar backup
tar -tzf db-backup.tar.gz

# Restaurar a un nuevo volumen
docker volume create db_restore
docker run --rm -v db_restore:/data -v $(pwd):/backup alpine \
  tar xzf /backup/db-backup.tar.gz -C /data

# Verificar restauración
docker run --rm -v db_restore:/data alpine ls -la /data

# Limpiar
docker volume rm db_backup db_restore
rm -f db-backup.tar.gz
```

### Ejercicio 8 — Inspección de volúmenes en uso

```bash
# Ver qué volúmenes están montados en cada contenedor
docker ps -a --format '{{.Names}}' | while read name; do
  mounts=$(docker inspect --format '{{range .Mounts}}{{.Name}}:{{.Destination}} {{end}}' "$name" 2>/dev/null)
  if [ -n "$mounts" ]; then
    echo "$name: $mounts"
  fi
done

# Ver contenedores que usan un volumen específico
docker ps -a --filter volume=mi_volumen

# Ver tamaño de un volumen
docker run --rm -v mi_volumen:/data alpine du -sh /data 2>/dev/null || echo "volumen vacío o inexistente"
```
