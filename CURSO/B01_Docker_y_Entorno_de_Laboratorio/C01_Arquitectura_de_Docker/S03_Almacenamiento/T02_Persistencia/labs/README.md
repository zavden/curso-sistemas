# Lab -- Persistencia

## Objetivo

Laboratorio practico para verificar de forma empirica que datos sobreviven a
cada operacion del ciclo de vida de un contenedor (`stop`, `rm`, `rm -v`),
demostrar la perdida de datos sin volumenes, la persistencia con named volumes,
la trampa de la instruccion VOLUME en Dockerfiles, y el uso de `docker commit`.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagenes base descargadas: `docker pull alpine:latest` y `docker pull debian:bookworm`
- Estar en el directorio `labs/` de este topico

## Archivos del laboratorio

| Archivo | Proposito |
|---|---|
| `Dockerfile.vol-trap-bad` | Demuestra la trampa: VOLUME antes de escribir datos |
| `Dockerfile.vol-trap-good` | Patron correcto: escribir datos antes de VOLUME |

---

## Parte 1 -- Tabla de supervivencia

### Objetivo

Verificar empiricamente que datos sobreviven a `docker stop` + `docker start`,
cuales se pierden con `docker rm`, y cuales se pierden con `docker rm -v`.

### Paso 1.1: Perdida de datos en la capa de escritura

```bash
docker run -d --name ephemeral-test alpine sh -c \
    'echo "important data" > /data.txt && sleep 3600'
```

Verifica que los datos existen:

```bash
docker exec ephemeral-test cat /data.txt
```

Salida esperada:

```
important data
```

Ahora elimina el contenedor:

```bash
docker rm -f ephemeral-test
```

Intenta recuperar los datos con un nuevo contenedor desde la misma imagen:

```bash
docker run --rm alpine cat /data.txt 2>&1
```

Salida esperada:

```
cat: can't open '/data.txt': No such file or directory
```

El archivo existia en la capa de escritura del contenedor anterior. Al hacer
`docker rm`, esa capa fue destruida permanentemente. Un nuevo contenedor
parte de la imagen original, donde `/data.txt` nunca existio.

### Paso 1.2: La capa de escritura sobrevive a `stop` + `start`

```bash
docker run -d --name survive-stop alpine sh -c \
    'echo "survives stop" > /data.txt && sleep 3600'
```

```bash
docker exec survive-stop cat /data.txt
```

Salida esperada:

```
survives stop
```

Detener y reiniciar:

```bash
docker stop survive-stop
docker start survive-stop
docker exec survive-stop cat /data.txt
```

Salida esperada:

```
survives stop
```

La capa de escritura se preserva con `stop` + `start`. Es el equivalente a
apagar y encender la maquina: el disco no se borra.

### Paso 1.3: `docker rm` destruye la capa de escritura

```bash
docker rm -f survive-stop
```

Los datos de `/data.txt` fueron destruidos permanentemente.

### Paso 1.4: Named volume sobrevive a `docker rm`

```bash
docker run -d --name survive-rm -v lab-persist:/data alpine sh -c \
    'echo "survives rm" > /data/test.txt && sleep 3600'
```

```bash
docker exec survive-rm cat /data/test.txt
```

Salida esperada:

```
survives rm
```

Antes de ejecutar, responde mentalmente:

- Si eliminamos el contenedor con `docker rm -f`, ¿seguiran los datos en
  el volumen `lab-persist`?

Intenta predecir antes de continuar al siguiente paso.

```bash
docker rm -f survive-rm
docker run --rm -v lab-persist:/data alpine cat /data/test.txt
```

Salida esperada:

```
survives rm
```

El named volume `lab-persist` sobrevivio a la destruccion del contenedor.

### Paso 1.5: Volumen anonimo con `docker rm` vs `docker rm -v`

```bash
# Create a container with an anonymous volume
docker run -d --name anon-vol-rm -v /anon-data alpine sh -c \
    'echo "anonymous volume data" > /anon-data/test.txt && sleep 3600'
```

Inspecciona para ver el volumen anonimo:

```bash
docker inspect anon-vol-rm --format '{{range .Mounts}}{{.Name}}{{end}}'
```

Guarda mentalmente el hash que aparece. Ahora:

```bash
# docker rm WITHOUT -v: anonymous volume survives
docker rm -f anon-vol-rm
docker volume ls -f dangling=true
```

El volumen anonimo aparece como dangling (huerfano). Si lo hubieras
eliminado con `docker rm -v`, el volumen tambien se habria eliminado.

### Paso 1.6: Tabla resumen empirica

Resumen de lo verificado:

| Dato | `stop` + `start` | `rm` | `rm -v` |
|---|---|---|---|
| Capa de escritura | Sobrevive | Se pierde | Se pierde |
| Named volume | Sobrevive | Sobrevive | Sobrevive |
| Volumen anonimo | Sobrevive | Sobrevive (queda huerfano) | Se pierde |

### Paso 1.7: Limpiar

```bash
docker rm -f ephemeral-test survive-stop survive-rm anon-vol-rm 2>/dev/null
docker volume rm lab-persist 2>/dev/null
docker volume prune -f
```

---

## Parte 2 -- Persistencia con named volumes

### Objetivo

Simular un patron realista: escribir datos en un named volume, destruir el
contenedor, recrearlo con el mismo volumen, y verificar que los datos persisten.

### Paso 2.1: Crear el volumen y escribir datos

```bash
docker volume create lab-persist
```

```bash
docker run --name persist-writer -v lab-persist:/data alpine sh -c '
    echo "record 1: initial entry" > /data/database.txt
    echo "record 2: second entry" >> /data/database.txt
    echo "config: version=1" > /data/config.txt
    ls -la /data/
'
```

Salida esperada:

```
...
-rw-r--r--    1 root   root   ... config.txt
-rw-r--r--    1 root   root   ... database.txt
```

### Paso 2.2: Destruir el contenedor

```bash
docker rm persist-writer
```

### Paso 2.3: Verificar que los datos persisten

Antes de ejecutar, responde mentalmente:

- El contenedor fue eliminado. Los archivos `database.txt` y `config.txt`,
  ¿siguen existiendo?

Intenta predecir antes de continuar al siguiente paso.

```bash
docker run --rm -v lab-persist:/data alpine sh -c \
    'echo "=== database.txt ===" && cat /data/database.txt && echo "" && echo "=== config.txt ===" && cat /data/config.txt'
```

Salida esperada:

```
=== database.txt ===
record 1: initial entry
record 2: second entry

=== config.txt ===
config: version=1
```

Los datos persisten intactos. El volumen existe independientemente del
contenedor.

### Paso 2.4: Recrear el contenedor y agregar datos

```bash
docker run --name persist-reader -v lab-persist:/data alpine sh -c '
    echo "record 3: added after recreation" >> /data/database.txt
    echo "config: version=2" > /data/config.txt
    cat /data/database.txt
'
```

Salida esperada:

```
record 1: initial entry
record 2: second entry
record 3: added after recreation
```

### Paso 2.5: Verificar una vez mas

```bash
docker rm persist-reader
docker run --rm -v lab-persist:/data alpine cat /data/database.txt
```

Salida esperada:

```
record 1: initial entry
record 2: second entry
record 3: added after recreation
```

Este es el patron fundamental para persistir datos en Docker: los contenedores
son desechables, los volumenes son permanentes.

### Paso 2.6: Limpiar

```bash
docker rm -f persist-writer persist-reader 2>/dev/null
docker volume rm lab-persist
```

---

## Parte 3 -- La trampa de VOLUME

### Objetivo

Demostrar que los datos escritos en un Dockerfile **despues** de la instruccion
`VOLUME` se pierden en tiempo de ejecucion, porque Docker ya ha marcado ese
path como punto de montaje de un volumen anonimo.

### Paso 3.1: Examinar los dos Dockerfiles

```bash
echo "=== Patron INCORRECTO ==="
cat Dockerfile.vol-trap-bad
```

```bash
echo ""
echo "=== Patron CORRECTO ==="
cat Dockerfile.vol-trap-good
```

Antes de construir, responde mentalmente:

- En `Dockerfile.vol-trap-bad`, el `RUN echo` se ejecuta despues de `VOLUME`.
  ¿Estara el archivo `important.txt` disponible cuando el contenedor arranque?
- En `Dockerfile.vol-trap-good`, el `RUN echo` se ejecuta antes de `VOLUME`.
  ¿Hay alguna diferencia?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.2: Construir ambas imagenes

```bash
docker build -f Dockerfile.vol-trap-bad -t lab-vol-trap-bad .
docker build -f Dockerfile.vol-trap-good -t lab-vol-trap-good .
```

### Paso 3.3: Ejecutar la imagen con el patron incorrecto

```bash
docker run --rm lab-vol-trap-bad
```

Salida esperada:

```
cat: can't open '/app/data/important.txt': No such file or directory
```

El archivo fue creado durante el build (en una capa de la imagen), pero en
tiempo de ejecucion Docker monta un volumen anonimo vacio sobre `/app/data`,
ocultando el contenido de la capa de la imagen. Como el volumen es nuevo y
vacio, `important.txt` no existe.

**Nota**: este comportamiento ocurre porque los datos fueron escritos **despues**
de la instruccion `VOLUME`. Docker descarta los cambios a ese path en capas
posteriores al `VOLUME`.

### Paso 3.4: Ejecutar la imagen con el patron correcto

```bash
docker run --rm lab-vol-trap-good
```

Salida esperada:

```
this data will be preserved
```

Aqui los datos se escribieron **antes** de `VOLUME`. La capa de la imagen
contiene `important.txt` en `/app/data`. Cuando Docker crea el volumen anonimo,
aplica la **copia inicial**: como el volumen esta vacio, Docker copia el
contenido preexistente de `/app/data` (de la imagen) al volumen.

### Paso 3.5: Verificar la diferencia inspeccionando las imagenes

```bash
echo "=== Layers of vol-trap-bad ==="
docker history lab-vol-trap-bad --format "table {{.CreatedBy}}\t{{.Size}}"

echo ""
echo "=== Layers of vol-trap-good ==="
docker history lab-vol-trap-good --format "table {{.CreatedBy}}\t{{.Size}}"
```

Observa que en ambos casos la capa del `RUN echo` tiene un tamano mayor a 0B.
Los datos SI se escribieron en la imagen durante el build. La diferencia es que
en `vol-trap-bad`, esos datos quedan ocultos en runtime por el volumen anonimo
montado antes de que la capa posterior surta efecto.

### Paso 3.6: Limpiar

```bash
docker rmi lab-vol-trap-bad lab-vol-trap-good
docker volume prune -f
```

---

## Parte 4 -- `docker commit`

### Objetivo

Usar `docker commit` para capturar el estado de un contenedor (incluyendo su
capa de escritura) como una nueva imagen, y entender sus limitaciones frente
a un Dockerfile.

### Paso 4.1: Crear un contenedor base y modificarlo

```bash
docker run -it --name commit-base debian:bookworm bash
```

Dentro del contenedor, ejecuta:

```bash
apt-get update && apt-get install -y curl
curl --version
echo "environment configured" > /setup-done.txt
exit
```

### Paso 4.2: Verificar los cambios con `docker diff`

```bash
docker diff commit-base | head -20
```

Salida esperada (parcial):

```
C /etc
A /etc/...
C /usr
A /usr/bin/curl
...
A /setup-done.txt
```

Las lineas con `A` muestran archivos agregados (curl y sus dependencias).
La linea `A /setup-done.txt` muestra el archivo que creamos manualmente.

### Paso 4.3: Crear una imagen con `docker commit`

```bash
docker commit commit-base lab-committed
```

Salida esperada:

```
sha256:<hash>
```

### Paso 4.4: Verificar la nueva imagen

```bash
docker run --rm lab-committed curl --version
```

Salida esperada:

```
curl <version> ...
...
```

```bash
docker run --rm lab-committed cat /setup-done.txt
```

Salida esperada:

```
environment configured
```

La nueva imagen contiene todo lo que se instalo y creo en el contenedor
original.

### Paso 4.5: Comparar tamanos

```bash
docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}" | grep -E "debian|lab-committed"
```

Salida esperada (aproximada):

```
lab-committed    latest    ~160MB
debian           bookworm  ~117MB
```

La imagen committed es mas grande porque incluye curl y todas sus dependencias
en una nueva capa.

### Paso 4.6: Inspeccionar el historial

```bash
docker history lab-committed --format "table {{.CreatedBy}}\t{{.Size}}"
```

Observa que la capa del commit aparece con el comentario generico del commit,
no con los comandos exactos que ejecutaste. Este es el problema principal de
`docker commit`: la imagen es opaca. Nadie puede saber que se instalo o
modifico sin inspeccionarla manualmente.

Con un Dockerfile, cada paso esta documentado y el build es reproducible.

### Paso 4.7: Limpiar

```bash
docker rm commit-base
docker rmi lab-committed
```

---

## Limpieza final

```bash
# Remove containers (if any remain from skipped steps)
docker rm -f ephemeral-test survive-stop survive-rm anon-vol-rm \
    persist-writer persist-reader commit-base 2>/dev/null

# Remove images
docker rmi lab-vol-trap-bad lab-vol-trap-good lab-committed 2>/dev/null

# Remove volumes
docker volume rm lab-persist 2>/dev/null

# Remove dangling anonymous volumes
docker volume prune -f
```

---

## Conceptos reforzados

1. La capa de escritura sobrevive a `docker stop` + `docker start`, pero se
   destruye permanentemente con `docker rm`. Los datos escritos fuera de
   volumenes se pierden al eliminar el contenedor.
2. Los **named volumes** sobreviven a `docker rm` y a `docker rm -v`. Son el
   mecanismo correcto para datos que deben persistir.
3. Los **volumenes anonimos** sobreviven a `docker rm` (quedan huerfanos),
   pero se eliminan con `docker rm -v`. Se acumulan si no se limpian
   explicitamente.
4. Escribir datos en un Dockerfile **despues** de la instruccion `VOLUME` es
   una trampa: esos datos se pierden en runtime porque Docker monta un volumen
   anonimo vacio que oculta las capas posteriores. El orden correcto es
   escribir primero y declarar `VOLUME` despues.
5. `docker commit` captura el estado completo de un contenedor como imagen,
   pero produce imagenes opacas e irreproducibles. Es util para debugging
   rapido, no para workflows de produccion.
6. El patron fundamental de persistencia en Docker es: **contenedores
   desechables, volumenes permanentes**. Los datos de aplicacion siempre deben
   estar en named volumes.
