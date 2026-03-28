# Lab — Limpieza selectiva

## Objetivo

Practicar la limpieza granular de recursos Docker: contenedores
detenidos, imagenes dangling, volumenes huerfanos, build cache y redes,
entendiendo el riesgo y el impacto de cada operacion.

## Prerequisitos

- Docker instalado y funcionando
- Estar en el directorio `labs/` de este topico

## Archivos del laboratorio

| Archivo | Proposito |
|---|---|
| `Dockerfile.prune` | Dockerfile simple para generar imagenes de prueba |

---

## Parte 1 — Container prune

### Objetivo

Crear contenedores detenidos y limpiarlos con `docker container prune`.

### Paso 1.1: Crear contenedores efimeros

```bash
for i in 1 2 3 4 5; do
    docker run --name "lab-prune-$i" alpine echo "test $i"
done
```

Cada `docker run` sin `--rm` crea un contenedor que queda en estado
"exited" despues de terminar.

### Paso 1.2: Ver contenedores detenidos

```bash
docker ps -a --filter "name=lab-prune" --format "table {{.Names}}\t{{.Status}}\t{{.Size}}"
```

Los 5 contenedores estan en estado "Exited". Cada uno mantiene su capa
de escritura en disco.

### Paso 1.3: Espacio antes

```bash
docker system df | head -2
```

Anota el valor de RECLAIMABLE en la fila Containers.

### Paso 1.4: Limpiar

```bash
docker container prune
```

Docker pide confirmacion. Responde `y`. Muestra cuantos contenedores
se eliminaron y el espacio liberado.

### Paso 1.5: Verificar

```bash
docker ps -a --filter "name=lab-prune"
```

Los contenedores ya no existen.

### Paso 1.6: Filtros

Los filtros permiten limpiar selectivamente:

```bash
docker run --name lab-prune-old alpine echo "old"
docker run --name lab-prune-new alpine echo "new"
```

```bash
docker container prune --filter "until=1h" -f
```

Solo elimina contenedores detenidos hace mas de 1 hora. El contenedor
recien creado deberia sobrevivir.

```bash
docker ps -a --filter "name=lab-prune"
docker rm lab-prune-new lab-prune-old 2>/dev/null
```

---

## Parte 2 — Image prune

### Objetivo

Generar imagenes dangling y limpiarlas con `docker image prune`.

### Paso 2.1: Crear imagenes dangling

```bash
docker build -t lab-prune --build-arg VERSION=1 -f Dockerfile.prune .
docker build -t lab-prune --build-arg VERSION=2 -f Dockerfile.prune .
```

El segundo build reutiliza el tag `lab-prune`. La imagen anterior
(VERSION=1) pierde su tag y queda como `<none>:<none>`.

### Paso 2.2: Ver imagenes dangling

```bash
docker image ls --filter dangling=true
```

Salida esperada:

```
REPOSITORY   TAG       IMAGE ID       CREATED          SIZE
<none>       <none>    <hash>         <time>           ~7MB
```

### Paso 2.3: Prune solo dangling (por defecto)

```bash
docker image prune
```

Responde `y`. Solo elimina imagenes `<none>:<none>`.

```bash
docker image ls --filter dangling=true
```

Vacio. Pero `lab-prune` sigue existiendo:

```bash
docker image ls lab-prune
```

### Paso 2.4: Prune todas las no usadas

```bash
docker image prune -a --filter "label!=keep" -f
```

Antes de ejecutar, predice: se eliminara `lab-prune`?

Si no hay ningun contenedor (activo o detenido) usando `lab-prune`,
se elimina. `-a` incluye todas las imagenes no usadas, no solo dangling.

```bash
docker image ls lab-prune
```

### Paso 2.5: Reconstruir

```bash
docker build -t lab-prune --build-arg VERSION=3 -f Dockerfile.prune .
```

Las imagenes eliminadas se pueden reconstruir desde el Dockerfile.

---

## Parte 3 — Volume prune

### Objetivo

Entender el riesgo de `docker volume prune` y practicar la verificacion
antes de limpiar.

### Paso 3.1: Crear volumenes

```bash
docker volume create lab-prune-safe
docker volume create lab-prune-important
```

### Paso 3.2: Asociar un volumen a un contenedor

```bash
docker run -d --name lab-prune-vol -v lab-prune-important:/data alpine sleep 300
```

`lab-prune-important` esta en uso. `lab-prune-safe` no tiene ningun
contenedor asociado.

### Paso 3.3: Verificar huerfanos

```bash
docker volume ls --filter dangling=true
```

Solo `lab-prune-safe` aparece como dangling. `lab-prune-important`
no aparece porque tiene un contenedor asociado.

### Paso 3.4: Escribir datos en el volumen huerfano

```bash
docker run --rm -v lab-prune-safe:/data alpine sh -c 'echo "datos temporales" > /data/test.txt'
docker run --rm -v lab-prune-safe:/data alpine cat /data/test.txt
```

El volumen tiene datos. `docker volume prune` los eliminaria.

### Paso 3.5: Prune volumenes

```bash
docker volume prune
```

Responde `y`. Solo elimina `lab-prune-safe` (el huerfano).

```bash
docker volume ls | grep lab-prune
```

`lab-prune-important` sigue existiendo (esta en uso).

### Paso 3.6: El escenario peligroso

```bash
docker rm -f lab-prune-vol
docker volume ls --filter dangling=true | grep lab-prune
```

Al eliminar el contenedor, `lab-prune-important` queda huerfano.
Un `docker volume prune` ahora lo eliminaria, perdiendo datos
permanentemente.

### Paso 3.7: Inspeccionar antes de eliminar

```bash
docker volume inspect lab-prune-important
```

Siempre inspeccionar volumenes antes de prune para entender que
contienen y a que proyecto pertenecen (las labels de Compose ayudan).

### Paso 3.8: Limpiar

```bash
docker volume rm lab-prune-important
```

---

## Parte 4 — Builder y network prune

### Objetivo

Limpiar build cache y redes no usadas.

### Paso 4.1: Build cache

```bash
docker builder du 2>/dev/null || echo "BuildKit du no disponible"
```

```bash
docker builder prune -f
```

Elimina cache no referenciado. El proximo build descarga y ejecuta
todo desde cero.

### Paso 4.2: Build con cache vs sin cache

```bash
echo "=== Build con cache ==="
time docker build -t lab-prune --build-arg VERSION=4 -f Dockerfile.prune . 2>&1 | tail -3

echo ""
echo "=== Build sin cache ==="
docker builder prune -a -f > /dev/null 2>&1
time docker build -t lab-prune --build-arg VERSION=5 -f Dockerfile.prune . 2>&1 | tail -3
```

Sin cache el build es mas lento. Con imagenes grandes (las del curso),
la diferencia es de segundos vs minutos.

### Paso 4.3: Redes

```bash
docker network create lab-prune-net
docker network ls | grep lab-prune
```

```bash
docker network prune -f
```

Elimina redes custom no usadas. Las redes predefinidas (`bridge`,
`host`, `none`) nunca se eliminan.

```bash
docker network ls | grep lab-prune
```

La red ya no existe. Es la operacion de prune con menor riesgo: las
redes no contienen datos y se recrean automaticamente.

---

## Limpieza final

```bash
docker rm -f lab-prune-vol 2>/dev/null
docker rmi lab-prune 2>/dev/null
docker volume rm lab-prune-safe lab-prune-important 2>/dev/null
docker network rm lab-prune-net 2>/dev/null
```

---

## Conceptos reforzados

1. Docker ofrece un comando `prune` para cada tipo de recurso. El orden
   de riesgo es: redes (minimo) < contenedores < imagenes dangling <
   build cache < imagenes no usadas < **volumenes** (maximo).

2. `docker image prune` (sin `-a`) solo elimina dangling
   (`<none>:<none>`). Con `-a` elimina todas las no usadas.

3. `docker volume prune` es **peligroso** porque los volumenes contienen
   datos persistentes. Un volumen huerfano puede tener datos importantes
   de un contenedor que se elimino.

4. **Siempre verificar** antes de volume prune: listar volumenes,
   inspeccionar los sospechosos, confirmar que no contienen datos
   importantes.

5. `docker network prune` es la operacion de menor riesgo: las redes
   no contienen datos y se recrean automaticamente con Compose.

6. Los **filtros** (`--filter "until=24h"`, `--filter "label=test"`)
   permiten limpiezas mas selectivas y seguras.
