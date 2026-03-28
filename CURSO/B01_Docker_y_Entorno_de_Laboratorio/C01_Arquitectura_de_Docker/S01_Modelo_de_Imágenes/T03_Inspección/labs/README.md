# Lab -- Inspeccion

## Objetivo

Laboratorio practico para verificar de forma empirica los conceptos de
inspeccion de imagenes: extraer metadata con `docker image inspect`,
analizar el historial de capas con `docker history`, entender `<missing>`,
y comparar tamano real vs virtual con `docker system df`.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagenes base descargadas: `docker pull debian:bookworm` y `docker pull nginx:latest`
- Estar en el directorio `labs/` de este topico

## Archivos del laboratorio

| Archivo | Proposito |
|---|---|
| `show-info.sh` | Script que muestra variables de entorno y argumentos recibidos |
| `Dockerfile.inspect` | Imagen con multiples instrucciones de metadata para inspeccionar |
| `Dockerfile.local` | Imagen local simple para comparar history con imagenes remotas |

---

## Parte 1 -- `docker image inspect`

### Objetivo

Extraer campos especificos de la metadata de una imagen usando `--format`,
entender la estructura del JSON y los campos mas relevantes.

### Paso 1.1: Inspeccion completa (observar la estructura)

```bash
docker image inspect debian:bookworm | python3 -m json.tool | head -40
```

La salida es un JSON extenso. Los campos principales estan organizados en
secciones: arquitectura, configuracion, filesystem, etc.

### Paso 1.2: Arquitectura y sistema operativo

```bash
docker image inspect debian:bookworm --format '{{.Architecture}}/{{.Os}}'
```

Salida esperada (depende de tu maquina):

```
amd64/linux
```

Indica para que plataforma fue compilada la imagen que se descargo.

### Paso 1.3: Tamano de la imagen

```bash
docker image inspect debian:bookworm --format '{{.Size}}'
```

El tamano se muestra en bytes. Para convertir a MB:

```bash
docker image inspect debian:bookworm --format '{{printf "%.1f MB" (divf .Size 1048576.0)}}'
```

Salida esperada:

```
~117.0 MB
```

Este es el tamano de la imagen completa (todas sus capas). No considera
deduplicacion con otras imagenes.

### Paso 1.4: Comando por defecto (CMD)

```bash
docker image inspect debian:bookworm --format '{{json .Config.Cmd}}'
```

Salida esperada:

```json
["bash"]
```

Este es el comando que se ejecuta si no se especifica otro en `docker run`.

```bash
docker image inspect nginx:latest --format '{{json .Config.Cmd}}'
```

Salida esperada:

```json
["nginx","-g","daemon off;"]
```

Nginx tiene un CMD mas especifico que Debian.

### Paso 1.5: Entrypoint

```bash
docker image inspect debian:bookworm --format '{{json .Config.Entrypoint}}'
```

Salida esperada:

```
null
```

Debian no tiene ENTRYPOINT. Compara con Nginx:

```bash
docker image inspect nginx:latest --format '{{json .Config.Entrypoint}}'
```

Salida esperada:

```json
["/docker-entrypoint.sh"]
```

Cuando hay ENTRYPOINT y CMD, Docker concatena ambos: el contenedor ejecuta
`/docker-entrypoint.sh nginx -g "daemon off;"`.

### Paso 1.6: Variables de entorno

```bash
docker image inspect nginx:latest --format '{{json .Config.Env}}' | python3 -m json.tool
```

Salida esperada (parcial):

```json
[
    "PATH=/usr/local/sbin:/usr/local/bin:...",
    "NGINX_VERSION=...",
    ...
]
```

Cada variable definida con `ENV` en el Dockerfile (o heredada de la imagen
base) aparece aqui.

### Paso 1.7: Puertos expuestos

```bash
docker image inspect nginx:latest --format '{{json .Config.ExposedPorts}}' | python3 -m json.tool
```

Salida esperada:

```json
{
    "80/tcp": {}
}
```

EXPOSE solo documenta los puertos que la imagen espera usar. No abre ni
publica puertos automaticamente -- para eso se necesita `-p` en `docker run`.

### Paso 1.8: Capas del filesystem (RootFS)

```bash
docker image inspect debian:bookworm --format '{{json .RootFS.Layers}}' | python3 -m json.tool
```

Salida esperada:

```json
[
    "sha256:<hash>"
]
```

Debian tiene una sola capa. Compara con Nginx:

```bash
docker image inspect nginx:latest --format '{{len .RootFS.Layers}} capas'
```

Nginx tiene varias capas (la base Debian mas las capas propias de Nginx).

### Paso 1.9: Construir y inspeccionar una imagen propia

```bash
docker build -f Dockerfile.inspect -t lab-inspect .
```

Ahora inspecciona la imagen construida para verificar que los valores del
Dockerfile se reflejan en la metadata:

```bash
echo "=== Entrypoint ==="
docker image inspect lab-inspect --format '{{json .Config.Entrypoint}}'

echo ""
echo "=== Cmd ==="
docker image inspect lab-inspect --format '{{json .Config.Cmd}}'

echo ""
echo "=== Env ==="
docker image inspect lab-inspect --format '{{json .Config.Env}}' | python3 -m json.tool

echo ""
echo "=== ExposedPorts ==="
docker image inspect lab-inspect --format '{{json .Config.ExposedPorts}}' | python3 -m json.tool

echo ""
echo "=== WorkingDir ==="
docker image inspect lab-inspect --format '{{.Config.WorkingDir}}'

echo ""
echo "=== Labels ==="
docker image inspect lab-inspect --format '{{json .Config.Labels}}' | python3 -m json.tool

echo ""
echo "=== Capas ==="
docker image inspect lab-inspect --format '{{len .RootFS.Layers}} capas'
```

Antes de ejecutar, predice:

- ¿Cuantas capas tendra `lab-inspect`? (Revisa el Dockerfile.inspect y cuenta
  las instrucciones que modifican el filesystem)
- ¿Que ENTRYPOINT y CMD mostrara?
- ¿Que puertos apareceran como expuestos?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.10: Verificar predicciones

Ejecuta el bloque del Paso 1.9 y compara con tus predicciones.

Salida esperada (parcial):

```
=== Entrypoint ===
["/app/show-info.sh"]

=== Cmd ===
["--verbose"]

=== ExposedPorts ===
{
    "443/tcp": {},
    "8080/tcp": {}
}

=== WorkingDir ===
/app

=== Labels ===
{
    "description": "Image for inspect lab",
    "maintainer": "lab-user"
}
```

Las instrucciones de metadata (ENV, EXPOSE, WORKDIR, LABEL, CMD, ENTRYPOINT)
no crean capas con contenido pero si se reflejan en la configuracion de la
imagen.

### Paso 1.11: Ejecutar la imagen para verificar

```bash
docker run --rm lab-inspect
```

Salida esperada:

```
Environment: production
Port: 8080
Working dir: /app
Args: --verbose
```

El ENTRYPOINT (`show-info.sh`) se ejecuta con el CMD (`--verbose`) como
argumento. Las variables de entorno definidas con ENV estan disponibles
dentro del contenedor.

---

## Parte 2 -- `docker history`

### Objetivo

Leer el historial de capas de una imagen, usar `--no-trunc` para ver las
instrucciones completas, y entender por que algunas capas muestran `<missing>`.

### Paso 2.1: Historial basico

```bash
docker history debian:bookworm
```

Salida esperada:

```
IMAGE          CREATED       CREATED BY                                      SIZE      COMMENT
<hash>         2 weeks ago   /bin/sh -c #(nop)  CMD ["bash"]                 0B
<hash>         2 weeks ago   /bin/sh -c #(nop) ADD file:<hash>... in /       ~117MB
```

Observa:

- `#(nop)` indica instrucciones de metadata que no modifican el filesystem
- `CMD ["bash"]` tiene tamano 0B porque es metadata pura
- `ADD file:...` tiene ~117MB porque crea el filesystem base

### Paso 2.2: Historial truncado vs completo

```bash
echo "=== Truncado (por defecto) ==="
docker history lab-inspect

echo ""
echo "=== Sin truncar ==="
docker history --no-trunc lab-inspect
```

Con `--no-trunc`, las instrucciones de `RUN` se muestran completas. Esto es
util para ver exactamente que se ejecuto, especialmente cuando se encadenan
comandos con `&&`.

### Paso 2.3: Formato personalizado

```bash
docker history lab-inspect --format "table {{.CreatedBy}}\t{{.Size}}"
```

Salida esperada (aproximada):

```
CREATED BY                                      SIZE
/bin/sh -c #(nop)  CMD ["--verbose"]            0B
/bin/sh -c #(nop)  ENTRYPOINT ["/app/show-...   0B
/bin/sh -c chmod +x /app/show-info.sh           ~150B
COPY show-info.sh /app/show-info.sh             ~150B
/bin/sh -c apt-get update && apt-get insta...   ~30MB
/bin/sh -c #(nop) WORKDIR /app                  0B
/bin/sh -c #(nop)  EXPOSE 443                   0B
/bin/sh -c #(nop)  EXPOSE 8080                  0B
/bin/sh -c #(nop)  ENV APP_PORT=8080            0B
/bin/sh -c #(nop)  ENV APP_ENV=production       0B
/bin/sh -c #(nop)  LABEL description=Image...   0B
/bin/sh -c #(nop)  LABEL maintainer=lab-user    0B
/bin/sh -c #(nop)  CMD ["bash"]                 0B
/bin/sh -c #(nop) ADD file:<hash>... in /       ~117MB
```

Cada linea corresponde a una instruccion del Dockerfile (leida de abajo
hacia arriba). Las instrucciones con 0B son metadata pura.

### Paso 2.4: Entender `<missing>` en la columna IMAGE

```bash
docker history lab-inspect --format "table {{.ID}}\t{{.CreatedBy}}\t{{.Size}}"
```

Observa la columna ID:

- Las capas **construidas localmente** (las de tu Dockerfile.inspect) tienen
  un IMAGE ID visible.
- Las capas **descargadas del registry** (las de `debian:bookworm`) muestran
  `<missing>`.

`<missing>` es completamente normal. Docker no almacena los IDs intermedios
de las capas descargadas. No indica un error ni datos perdidos.

### Paso 2.5: Comparar con una imagen solo descargada

```bash
docker history nginx:latest --format "table {{.ID}}\t{{.Size}}"
```

Todas (o casi todas) las capas muestran `<missing>` porque la imagen fue
descargada completa del registry, no construida localmente.

---

## Parte 3 -- Imagen local vs imagen remota

### Objetivo

Construir una imagen local y comparar su historial con una imagen descargada,
observando donde aparece `<missing>` y como se diferencian las capas propias
de las capas heredadas de la base.

### Paso 3.1: Examinar el Dockerfile

```bash
cat Dockerfile.local
```

Antes de construir, predice:

- ¿Cuantas capas con contenido (>0 bytes) tendra la imagen?
- ¿Cuantas capas de metadata (0 bytes)?
- ¿Cuales capas mostraran `<missing>` y cuales tendran IMAGE ID?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.2: Construir la imagen local

```bash
docker build -f Dockerfile.local -t lab-local .
```

### Paso 3.3: Ver el historial completo

```bash
docker history lab-local
```

Salida esperada (aproximada):

```
IMAGE          CREATED          CREATED BY                                      SIZE      COMMENT
<hash>         seconds ago      /bin/sh -c #(nop)  CMD ["cat" "/data.txt"]      0B
<hash>         seconds ago      /bin/sh -c echo "layer 2: create file" && ...   ~30B
<hash>         seconds ago      /bin/sh -c echo "layer 1: update" && apt-...    ~20MB
<missing>      2 weeks ago      /bin/sh -c #(nop)  CMD ["bash"]                 0B
<missing>      2 weeks ago      /bin/sh -c #(nop) ADD file:<hash>... in /       ~117MB
```

Observa la linea divisoria:

- **Arriba** (con IMAGE ID): capas construidas localmente por tu Dockerfile
- **Abajo** (con `<missing>`): capas heredadas de `debian:bookworm`

### Paso 3.4: Comparar con la imagen base

```bash
echo "=== Capas de debian:bookworm ==="
docker history debian:bookworm --format "table {{.ID}}\t{{.CreatedBy}}\t{{.Size}}"

echo ""
echo "=== Capas de lab-local ==="
docker history lab-local --format "table {{.ID}}\t{{.CreatedBy}}\t{{.Size}}"
```

Las ultimas capas de `lab-local` son identicas a las de `debian:bookworm`.
`docker history` muestra **todas** las capas, incluyendo las heredadas.

### Paso 3.5: Verificar con RootFS.Layers

```bash
echo "=== Hashes de capas: debian:bookworm ==="
docker image inspect debian:bookworm --format '{{json .RootFS.Layers}}' | python3 -m json.tool

echo ""
echo "=== Hashes de capas: lab-local ==="
docker image inspect lab-local --format '{{json .RootFS.Layers}}' | python3 -m json.tool
```

El primer hash de `lab-local` es identico al unico hash de `debian:bookworm`.
Las capas adicionales son las que agrego el Dockerfile.

### Paso 3.6: Contar capas

```bash
echo "debian:bookworm: $(docker image inspect debian:bookworm --format '{{len .RootFS.Layers}}') capas"
echo "lab-local:       $(docker image inspect lab-local --format '{{len .RootFS.Layers}}') capas"
echo "lab-inspect:     $(docker image inspect lab-inspect --format '{{len .RootFS.Layers}}') capas"
echo "nginx:latest:    $(docker image inspect nginx:latest --format '{{len .RootFS.Layers}}') capas"
```

Esto confirma que el numero de capas depende de cuantas instrucciones
modifican el filesystem (FROM, RUN, COPY, ADD), no del total de instrucciones
del Dockerfile.

### Paso 3.7: Limpiar

```bash
docker rmi lab-local
```

---

## Parte 4 -- Tamano real vs virtual

### Objetivo

Medir el espacio real ocupado por las imagenes, entender la diferencia entre
SHARED SIZE y UNIQUE SIZE, y verificar que la deduplicacion de capas reduce
el espacio real respecto a la suma de tamanos virtuales.

### Paso 4.1: Ver tamanos virtuales individuales

```bash
docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}" \
    | grep -E "debian|nginx|lab-inspect"
```

Suma mentalmente los tamanos de todas las imagenes listadas. Este total es
el tamano "virtual" -- lo que ocuparian si no compartieran capas.

### Paso 4.2: Ver espacio real total

```bash
docker system df
```

Salida esperada (aproximada):

```
TYPE            TOTAL     ACTIVE    SIZE        RECLAIMABLE
Images          3         0         <tamano>    <tamano> (100%)
Containers      0         0         0B          0B
Local Volumes   0         0         0B          0B
Build Cache     0         0         0B          0B
```

El SIZE de Images es el espacio real en disco, con deduplicacion aplicada.
Deberia ser **menor** que la suma de los tamanos individuales.

### Paso 4.3: Desglose detallado con `docker system df -v`

```bash
docker system df -v 2>/dev/null | head -20
```

Salida esperada (aproximada):

```
Images space usage:

REPOSITORY    TAG        IMAGE ID       CREATED        SIZE      SHARED SIZE   UNIQUE SIZE   VIRTUAL SIZE
debian        bookworm   <hash>         2 weeks ago    ~117MB    ~117MB        0B            ~117MB
lab-inspect   latest     <hash>         minutes ago    ~148MB    ~117MB        ~31MB         ~148MB
nginx         latest     <hash>         2 weeks ago    ~190MB    ~117MB        ~73MB         ~190MB
```

### Paso 4.4: Analizar los campos

Observa los campos de cada imagen:

| Campo | Significado |
|---|---|
| SIZE | Tamano total de la imagen (todas sus capas) |
| SHARED SIZE | Capas que esta imagen comparte con al menos otra imagen local |
| UNIQUE SIZE | Capas que solo tiene esta imagen |
| VIRTUAL SIZE | Igual a SIZE (incluido por compatibilidad) |

Preguntas para analizar:

- ¿Por que `debian:bookworm` tiene SHARED SIZE igual a su SIZE total y
  UNIQUE SIZE de 0B?
- ¿De donde viene el SHARED SIZE de `lab-inspect`?

Intenta razonar antes de continuar al siguiente paso.

### Paso 4.5: Respuestas

**Debian tiene SHARED SIZE = SIZE**: porque todas sus capas son compartidas
con `lab-inspect` (que esta basada en Debian). El UNIQUE SIZE es 0B porque
no tiene capas que sean exclusivamente suyas -- todas estan compartidas.

**lab-inspect tiene SHARED SIZE ~117MB**: esos ~117MB son las capas de
`debian:bookworm` que hereda. Su UNIQUE SIZE (~31MB) son las capas propias
(curl, scripts, etc.).

### Paso 4.6: Calcular el ahorro por deduplicacion

```bash
echo "=== Tamanos virtuales individuales ==="
docker images --format "{{.Repository}}:{{.Tag}} {{.Size}}" \
    | grep -E "debian|nginx|lab-inspect"

echo ""
echo "=== Espacio real en disco ==="
docker system df --format "table {{.Type}}\t{{.Size}}" | grep Images
```

La suma de los tamanos virtuales sera significativamente mayor que el espacio
real. La diferencia es el ahorro por deduplicacion: las capas de
`debian:bookworm` se almacenan una sola vez, aunque tres imagenes las
referencien.

### Paso 4.7: Que imagen eliminar para liberar mas espacio

Mentalmente, predice: si eliminas `debian:bookworm`, ¿se liberan ~117MB?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.8: Verificar

```bash
docker rmi debian:bookworm
docker system df
```

No se liberaron ~117MB porque `lab-inspect` (y posiblemente `nginx`) todavia
necesitan esas capas. Docker solo libera una capa cuando **ninguna** imagen
local la referencia.

```bash
# Restaurar debian para la limpieza ordenada
docker pull debian:bookworm
```

---

## Limpieza final

```bash
# Eliminar imagenes construidas en el lab
docker rmi lab-inspect lab-local 2>/dev/null

# Eliminar imagenes descargadas para el lab
docker rmi debian:bookworm nginx:latest 2>/dev/null

# Eliminar imagenes huerfanas
docker image prune -f 2>/dev/null
```

---

## Conceptos reforzados

1. `docker image inspect` devuelve toda la metadata de una imagen en JSON.
   Con `--format` se extraen campos especificos sin necesidad de herramientas
   externas.

2. Los campos mas utiles de `inspect` son: `Config.Cmd`, `Config.Entrypoint`,
   `Config.Env`, `Config.ExposedPorts`, `RootFS.Layers`, `Architecture` y
   `Size`.

3. `docker history` reconstruye el Dockerfile mostrando cada instruccion y su
   tamano. Las instrucciones con `#(nop)` y tamano 0B son metadata pura.

4. `--no-trunc` en `docker history` muestra las instrucciones completas, util
   para depurar RUNs encadenados con `&&`.

5. `<missing>` en la columna IMAGE de `docker history` es normal: indica capas
   descargadas del registry, no un error. Solo las capas construidas localmente
   muestran IMAGE ID.

6. `docker system df -v` muestra el SHARED SIZE (capas compartidas con otras
   imagenes) y UNIQUE SIZE (capas exclusivas). El espacio real en disco es
   menor que la suma de tamanos virtuales gracias a la deduplicacion.

7. Una capa se libera de disco solo cuando **ninguna** imagen local la
   referencia. Eliminar una imagen base no libera espacio si otras imagenes
   derivadas de ella siguen existiendo.
