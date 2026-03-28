# Lab -- Registros

## Objetivo

Laboratorio practico para verificar de forma empirica los conceptos de
nomenclatura de Docker Hub, pull por tag y por digest, la trampa de `latest`,
mutabilidad de tags, rate limits, y manifiestos multi-platform.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Acceso a Internet (para interactuar con Docker Hub)
- Imagen base descargada: `docker pull alpine:3.20`
- Estar en el directorio `labs/` de este topico

## Archivos del laboratorio

| Archivo | Proposito |
|---|---|
| `Dockerfile.tag-test` | Imagen simple parametrizada para demostrar mutabilidad de tags |
| `show-platform.sh` | Script que muestra la plataforma donde se ejecuta el contenedor |

---

## Parte 1 -- Nomenclatura de Docker Hub

### Objetivo

Entender como Docker resuelve los nombres cortos de imagenes a paths completos,
y verificar que el namespace `library/` es el valor por defecto para imagenes
oficiales.

### Paso 1.1: Resolver nombres cortos mentalmente

Antes de ejecutar cualquier comando, resuelve mentalmente cada nombre corto
a su referencia completa (`registry/namespace/name:tag`):

- `alpine`
- `nginx:1.25`
- `myuser/myapp:v2`
- `quay.io/prometheus/node-exporter:v1.7.0`

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.2: Verificar la resolucion

```bash
docker pull alpine:3.20
```

Observa la salida: Docker muestra de donde descarga la imagen. El output
incluye el registry (`docker.io`) y el namespace implicito (`library`).

Salida esperada:

```
3.20: Pulling from library/alpine
...
```

`library/alpine` confirma que `alpine` se resuelve a `docker.io/library/alpine`.

### Paso 1.3: Pull con nombre explicito

```bash
docker pull docker.io/library/alpine:3.20
```

Salida esperada:

```
3.20: Pulling from library/alpine
Digest: sha256:<hash>
Status: Image is up to date for alpine:3.20
```

El pull no descarga nada nuevo porque es la misma imagen que ya tenemos.
Docker resolvio el nombre corto `alpine:3.20` a `docker.io/library/alpine:3.20`.

### Paso 1.4: Verificar que ambas referencias son identicas

```bash
docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.ID}}" | grep alpine
```

Solo aparece una entrada. Docker no crea dos imagenes cuando se usa el nombre
corto vs el nombre completo -- son la misma referencia.

### Paso 1.5: Intentar un pull de imagen no oficial

```bash
docker pull hashicorp/terraform:latest
```

Salida esperada:

```
latest: Pulling from hashicorp/terraform
...
```

Observa: el "from" dice `hashicorp/terraform`, no `library/terraform`.
Las imagenes bajo un namespace de usuario/organizacion nunca se resuelven
a `library/`.

### Paso 1.6: Limpiar

```bash
docker rmi hashicorp/terraform:latest
```

---

## Parte 2 -- Tags y la trampa de `latest`

### Objetivo

Demostrar que `latest` es solo un tag convencional (no un mecanismo automatico),
que multiples tags pueden apuntar a la misma imagen, y que los tags son mutables.

### Paso 2.1: Descargar la misma imagen con dos tags diferentes

```bash
docker pull alpine:latest
docker pull alpine:3.20
```

### Paso 2.2: Comparar los IMAGE IDs

```bash
docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.ID}}" | grep alpine
```

Antes de mirar la salida, predice: ¿tendran el mismo IMAGE ID o diferente?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.3: Analizar el resultado

Si `alpine:latest` y `alpine:3.20` tienen el **mismo IMAGE ID**, es porque
el mantenedor de Alpine asigno el tag `latest` a la misma imagen que `3.20`.
Son dos etiquetas para la misma imagen.

Si tienen **IDs diferentes**, es porque `latest` apunta a otra version (por
ejemplo, una version edge o una actualizacion reciente que aun no se refleja
en `3.20`).

En cualquier caso, `latest` no tiene un significado especial. Es un tag como
cualquier otro, que el mantenedor asigna manualmente.

### Paso 2.4: Verificar que un pull anterior no se actualiza solo

```bash
# Ver la fecha de descarga de la imagen local
docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.CreatedAt}}" | grep alpine
```

La imagen local es una fotografia de lo que habia en Docker Hub en el momento
del pull. Docker no la actualiza automaticamente. Si el mantenedor sube una
nueva version de `alpine:latest` manana, tu copia local seguira siendo la de
hoy hasta que hagas `docker pull` explicitamente.

### Paso 2.5: Tags son mutables -- demostracion

Construye dos versiones de una imagen y asignales el mismo tag:

```bash
docker build -f Dockerfile.tag-test --build-arg APP_VERSION=1.0 -t lab-tag-v1 .
docker run --rm lab-tag-v1
```

Salida esperada:

```
app version: 1.0
```

```bash
docker build -f Dockerfile.tag-test --build-arg APP_VERSION=2.0 -t lab-tag-v2 .
docker run --rm lab-tag-v2
```

Salida esperada:

```
app version: 2.0
```

Ahora reasigna el tag `lab-tag-v1` a la imagen v2:

```bash
# Guardar el IMAGE ID actual de lab-tag-v1
docker images --format "{{.ID}}" lab-tag-v1
```

```bash
# Retagear: asignar lab-tag-v1 a la imagen de v2
docker tag lab-tag-v2 lab-tag-v1

# Verificar
docker run --rm lab-tag-v1
```

Salida esperada:

```
app version: 2.0
```

El tag `lab-tag-v1` ahora apunta a una imagen completamente diferente.
Esto es exactamente lo que puede ocurrir en un registry publico: un tag
como `nginx:1.25` puede ser actualizado por el mantenedor para incluir
parches de seguridad, y apunta a una imagen diferente de la que tenia
hace un mes.

### Paso 2.6: Verificar con IMAGE IDs

```bash
docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.ID}}" | grep lab-tag
```

Salida esperada:

```
lab-tag-v1     latest    <mismo-id-que-v2>
lab-tag-v2     latest    <mismo-id-que-v2>
```

Ambos tags apuntan al mismo IMAGE ID. La imagen original de v1 quedo huerfana
(sin tag). Puedes verla con:

```bash
docker images --filter "dangling=true"
```

### Paso 2.7: Descargar una version diferente de Alpine

```bash
docker pull alpine:3.19
docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.ID}}" | grep alpine
```

Ahora tienes tres tags de Alpine. Observa los IMAGE IDs: `3.19` tendra un ID
diferente a `3.20` porque son versiones distintas.

### Paso 2.8: Limpiar

```bash
docker rmi lab-tag-v1 lab-tag-v2
docker image prune -f
docker rmi alpine:3.19 2>/dev/null
```

---

## Parte 3 -- Digests e inmutabilidad

### Objetivo

Demostrar que un digest es inmutable y referencia exactamente una imagen,
a diferencia de un tag que puede cambiar.

### Paso 3.1: Obtener el digest de una imagen

```bash
docker images --digests --format "table {{.Repository}}\t{{.Tag}}\t{{.Digest}}" | grep alpine
```

Salida esperada:

```
alpine    3.20    sha256:<hash-largo>
```

Copia el digest completo (`sha256:...`). Este hash es el SHA256 del manifiesto
de la imagen y es unico e inmutable.

### Paso 3.2: Pull por digest

```bash
# Usar el digest obtenido en el paso anterior
# Reemplaza <digest> con el valor real
docker pull alpine@sha256:<digest>
```

Salida esperada:

```
sha256:<digest>: Pulling from library/alpine
Digest: sha256:<digest>
Status: Image is up to date for alpine@sha256:<digest>
```

Docker confirma que la imagen ya existe localmente. El pull por digest garantiza
reproducibilidad: siempre obtendras exactamente la misma imagen.

### Paso 3.3: Verificar la equivalencia

```bash
docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.ID}}\t{{.Digest}}" | grep alpine
```

La imagen descargada por digest y la descargada por tag tienen el mismo
IMAGE ID (son la misma imagen).

### Paso 3.4: Entender la diferencia clave

Ejecuta el siguiente resumen mental:

- **Tag `alpine:3.20`**: hoy apunta a la imagen X, manana podria apuntar a
  la imagen Y (si Alpine publica una actualizacion de seguridad para 3.20).
- **Digest `alpine@sha256:<digest>`**: siempre apunta a la imagen X,
  independientemente de lo que haga el mantenedor.

En produccion y CI/CD, usar digests garantiza que el despliegue es reproducible.
En desarrollo, los tags son mas comodos.

### Paso 3.5: Ver el digest durante un pull fresco

```bash
# Eliminar la imagen local para forzar un pull completo
docker rmi alpine:3.20

docker pull alpine:3.20
```

Observa la salida: Docker muestra el `Digest: sha256:...` al final del pull.
Este digest es el mismo que obtuviste en el Paso 3.1 (siempre que la imagen
no haya sido actualizada en el registry).

---

## Parte 4 -- Rate limits de Docker Hub

### Objetivo

Consultar los limites de descarga de Docker Hub y entender la diferencia
entre acceso anonimo y autenticado.

### Paso 4.1: Consultar rate limits como anonimo

```bash
TOKEN=$(curl -s \
  "https://auth.docker.io/token?service=registry.docker.io&scope=repository:library/alpine:pull" \
  | python3 -c "import json,sys; print(json.load(sys.stdin)['token'])")

curl -s -D - -H "Authorization: Bearer $TOKEN" \
  "https://registry-1.docker.io/v2/library/alpine/manifests/latest" \
  -o /dev/null 2>&1 | grep -i ratelimit
```

Salida esperada (aproximada):

```
ratelimit-limit: 100;w=21600
ratelimit-remaining: <valor>;w=21600
```

- `ratelimit-limit: 100;w=21600` significa 100 pulls en 21600 segundos (6 horas)
- `ratelimit-remaining` muestra cuantos pulls quedan en la ventana actual

### Paso 4.2: Verificar autenticacion actual

```bash
docker info 2>/dev/null | grep -i username
```

Si no aparece nada, no estas autenticado. Si aparece tu nombre de usuario,
estas autenticado y tus rate limits son mas altos (200 pulls/6h para cuentas
free).

### Paso 4.3: Que pasa cuando se agotan los limites

Cuando se alcanza el limite, Docker Hub responde con HTTP 429 (Too Many
Requests). Los pulls fallan con un mensaje como:

```
toomanyrequests: You have reached your pull rate limit.
```

En entornos con IP compartida (oficinas, universidades, CI/CD), el limite
anonimo por IP se agota rapidamente. Autenticarse vincula el limite a la
cuenta en vez de la IP.

> Nota: no vamos a agotar los limites en este lab. Solo verificamos el estado
> actual.

---

## Parte 5 -- Manifiestos multi-platform

### Objetivo

Inspeccionar los manifiestos multi-platform de una imagen para ver las
plataformas disponibles, y entender como Docker selecciona la variante
correcta.

### Paso 5.1: Inspeccionar el manifiesto de Alpine

```bash
docker manifest inspect alpine:3.20
```

La salida es un JSON con una lista de manifiestos, uno por cada plataforma
soportada.

### Paso 5.2: Extraer las plataformas disponibles

```bash
docker manifest inspect alpine:3.20 | python3 -c "
import json, sys
data = json.load(sys.stdin)
if 'manifests' in data:
    for m in data['manifests']:
        p = m.get('platform', {})
        arch = p.get('architecture', '?')
        os_name = p.get('os', '?')
        variant = p.get('variant', '')
        full = f'{os_name}/{arch}'
        if variant:
            full += f'/{variant}'
        print(full)
"
```

Salida esperada (aproximada):

```
linux/amd64
linux/arm/v6
linux/arm/v7
linux/arm64
linux/386
linux/ppc64le
linux/s390x
```

Alpine soporta multiples arquitecturas. Cuando haces `docker pull alpine:3.20`,
Docker descarga automaticamente la variante que corresponde a tu maquina.

### Paso 5.3: Verificar que plataforma se descargo localmente

```bash
docker image inspect alpine:3.20 --format '{{.Architecture}}/{{.Os}}'
```

Salida esperada (depende de tu maquina):

```
amd64/linux
```

Este es el resultado de la seleccion automatica que hizo Docker al descargar
la imagen.

### Paso 5.4: Comparar digests por plataforma

```bash
docker manifest inspect alpine:3.20 | python3 -c "
import json, sys
data = json.load(sys.stdin)
if 'manifests' in data:
    for m in data['manifests']:
        p = m.get('platform', {})
        arch = p.get('architecture', '?')
        variant = p.get('variant', '')
        label = arch
        if variant:
            label += f'/{variant}'
        digest = m.get('digest', '?')
        print(f'{label:12s}  {digest[:30]}...')
"
```

Cada plataforma tiene su propio digest. `alpine:3.20` para amd64 y
`alpine:3.20` para arm64 son imagenes diferentes (capas diferentes,
binarios diferentes), pero se acceden con el mismo tag.

### Paso 5.5: Ver la plataforma desde dentro de un contenedor

```bash
docker run --rm -v "$(pwd)/show-platform.sh:/show-platform.sh:ro" \
    alpine:3.20 sh /show-platform.sh
```

Salida esperada (depende de tu maquina):

```
Architecture: x86_64
OS:           Linux
Kernel:       <version-del-kernel>
```

La arquitectura que reporta el contenedor coincide con la plataforma que
Docker selecciono automaticamente.

---

## Limpieza final

```bash
# Eliminar imagenes descargadas para este lab
docker rmi alpine:3.20 alpine:latest 2>/dev/null

# Eliminar imagenes de build (si quedaron por un paso saltado)
docker rmi lab-tag-v1 lab-tag-v2 2>/dev/null

# Eliminar imagenes huerfanas
docker image prune -f 2>/dev/null
```

---

## Conceptos reforzados

1. Docker resuelve nombres cortos automaticamente: `alpine` equivale a
   `docker.io/library/alpine:latest`. El namespace `library/` es exclusivo
   de las imagenes oficiales.

2. `latest` es un tag convencional asignado por el mantenedor, **no un
   mecanismo automatico**. Una imagen local con tag `latest` puede estar
   desactualizada si no se hace `docker pull` explicitamente.

3. Los tags son **mutables**: el mismo tag puede apuntar a imagenes diferentes
   a lo largo del tiempo. Un `docker tag` local o una actualizacion en el
   registry pueden cambiar a que imagen apunta un tag.

4. Los digests (`sha256:...`) son **inmutables** y garantizan reproducibilidad.
   Pull por digest siempre descarga exactamente la misma imagen.

5. Docker Hub impone rate limits: 100 pulls/6h anonimo, 200 pulls/6h
   autenticado (free). En entornos con IP compartida, autenticarse es
   esencial.

6. Las imagenes multi-platform comparten el mismo tag pero tienen manifiestos
   y capas diferentes por arquitectura. Docker selecciona la variante correcta
   automaticamente segun la plataforma del host.
