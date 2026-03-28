# Lab — Monitoreo de espacio

## Objetivo

Aprender a medir cuanto espacio consume Docker en el sistema, identificar
recursos que se pueden limpiar, y entender la diferencia entre tamano
virtual y real de las imagenes.

## Prerequisitos

- Docker instalado y funcionando
- Estar en el directorio `labs/` de este topico

## Archivos del laboratorio

| Archivo | Proposito |
|---|---|
| `Dockerfile.space` | Dockerfile para generar imagenes de prueba con padding |

---

## Parte 1 — docker system df

### Objetivo

Usar `docker system df` para obtener un resumen del espacio consumido
por Docker.

### Paso 1.1: Resumen de espacio

```bash
docker system df
```

Salida esperada (los valores dependen de tu sistema):

```
TYPE            TOTAL     ACTIVE    SIZE      RECLAIMABLE
Images          X         Y         ...       ... (XX%)
Containers      X         Y         ...       ... (XX%)
Local Volumes   X         Y         ...       ... (XX%)
Build Cache     X         Y         ...       ... (XX%)
```

Los campos clave:

- **TOTAL** — cantidad total de recursos
- **ACTIVE** — recursos en uso (contenedores corriendo, imagenes referenciadas)
- **SIZE** — espacio total en disco
- **RECLAIMABLE** — espacio que se puede liberar sin afectar recursos activos

### Paso 1.2: Modo verbose

```bash
docker system df -v
```

El modo verbose muestra el desglose individual de cada recurso:
imagenes con su SHARED SIZE y UNIQUE SIZE, contenedores con su LOCAL
VOLUMES y SIZE, volumenes con LINKS.

Busca en la salida:

- Imagenes `<none>:<none>` — son dangling (candidatas a limpieza)
- Volumenes con LINKS = 0 — son huerfanos (candidatos a limpieza)

### Paso 1.3: Solo imagenes

```bash
docker image ls --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}"
```

Esta vista muestra el tamano **virtual** de cada imagen (incluyendo
capas compartidas con otras imagenes).

---

## Parte 2 — Identificar consumidores

### Objetivo

Crear recursos de prueba para practicar la identificacion de imagenes
dangling, volumenes huerfanos, y contenedores detenidos.

### Paso 2.1: Crear imagenes dangling

```bash
docker build -t lab-space-v1 --build-arg VERSION=1 -f Dockerfile.space .
docker build -t lab-space-v1 --build-arg VERSION=2 -f Dockerfile.space .
```

El segundo build usa el mismo tag `lab-space-v1`. La imagen anterior
pierde su tag y queda como `<none>:<none>`.

### Paso 2.2: Verificar dangling

```bash
docker image ls --filter dangling=true
```

Salida esperada:

```
REPOSITORY   TAG       IMAGE ID       CREATED          SIZE
<none>       <none>    <hash>         <time>           ~10MB
```

La imagen con VERSION=1 perdio su tag y ahora es dangling.

### Paso 2.3: Crear contenedores detenidos

```bash
docker run --name lab-space-exit1 alpine echo "test 1"
docker run --name lab-space-exit2 alpine echo "test 2"
docker run --name lab-space-exit3 alpine echo "test 3"
```

### Paso 2.4: Verificar contenedores detenidos

```bash
docker ps -a --filter status=exited --format "table {{.Names}}\t{{.Status}}\t{{.Size}}"
```

Cada contenedor detenido ocupa espacio con su capa de escritura.

### Paso 2.5: Crear volumenes huerfanos

```bash
docker volume create lab-space-data
docker run -d --name lab-space-vol -v lab-space-used:/data alpine sleep 60
```

`lab-space-data` no tiene ningun contenedor asociado (huerfano).
`lab-space-used` esta en uso por el contenedor corriendo.

### Paso 2.6: Verificar volumenes

```bash
docker volume ls --filter dangling=true
```

Solo `lab-space-data` aparece como huerfano. `lab-space-used` no
aparece porque tiene un contenedor asociado.

### Paso 2.7: Ver el impacto en docker system df

```bash
docker system df
```

Compara con la salida del paso 1.1. Ahora RECLAIMABLE deberia ser
mayor porque hay imagenes dangling, contenedores detenidos, y
volumenes huerfanos.

### Paso 2.8: Build cache

```bash
docker builder du 2>/dev/null || echo "BuildKit du no disponible"
```

Muestra el espacio usado por el cache de BuildKit con desglose por
tipo.

---

## Parte 3 — Espacio real vs virtual

### Objetivo

Entender la diferencia entre el tamano que reporta `docker image ls`
(virtual) y el espacio real en disco.

### Paso 3.1: Construir dos imagenes con capas compartidas

```bash
docker build -t lab-space-v1 --build-arg VERSION=1 -f Dockerfile.space .
docker build -t lab-space-v2 --build-arg VERSION=2 -f Dockerfile.space .
```

### Paso 3.2: Tamano virtual

```bash
docker image ls --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}" | grep lab-space
```

Ambas imagenes muestran un tamano similar (~10MB cada una). La suma
directa seria ~20MB.

### Paso 3.3: Tamano real

```bash
docker system df
```

El campo SIZE en la fila Images muestra el espacio **real** en disco.
Es menor que la suma de tamanos individuales porque ambas imagenes
comparten la capa base de `alpine:latest`.

### Paso 3.4: Ver capas compartidas (verbose)

```bash
docker system df -v 2>&1 | head -20
```

Busca las columnas SHARED SIZE y UNIQUE SIZE:

- **SHARED SIZE** — capas compartidas con otras imagenes
- **UNIQUE SIZE** — capas exclusivas de esta imagen

La capa de `alpine:latest` aparece como SHARED en ambas imagenes
pero se almacena solo una vez en disco.

### Paso 3.5: Espacio en el host

```bash
echo "=== Espacio Docker en el host ==="
sudo du -sh /var/lib/docker/ 2>/dev/null || \
    du -sh ~/.local/share/docker/ 2>/dev/null || \
    echo "No se puede determinar (rootless o path diferente)"
```

Este comando muestra el espacio real que Docker usa en el filesystem
del host. Puede diferir ligeramente de `docker system df` por
metadata, logs, y archivos temporales.

---

## Limpieza final

```bash
docker rm -f lab-space-vol lab-space-exit1 lab-space-exit2 lab-space-exit3
docker rmi lab-space-v1 lab-space-v2
docker image prune -f
docker volume rm lab-space-data lab-space-used 2>/dev/null
```

---

## Conceptos reforzados

1. `docker system df` muestra el espacio **real** consumido por Docker,
   con el campo RECLAIMABLE indicando cuanto se puede liberar.

2. `docker system df -v` desglosa cada recurso individual, mostrando
   SHARED SIZE vs UNIQUE SIZE para imagenes y LINKS para volumenes.

3. Las **imagenes dangling** (`<none>:<none>`) se crean al hacer rebuild
   con un tag existente. Son candidatas inmediatas a limpieza.

4. `docker image ls` muestra el tamano **virtual** (incluyendo capas
   compartidas). La suma de tamanos individuales es mayor que el
   espacio real porque las capas compartidas se cuentan una vez.

5. Los **volumenes con LINKS = 0** son huerfanos: no tienen ningun
   contenedor asociado y pueden contener datos obsoletos.
