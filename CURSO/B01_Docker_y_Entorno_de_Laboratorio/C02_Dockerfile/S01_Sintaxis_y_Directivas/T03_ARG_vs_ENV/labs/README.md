# Lab — ARG vs ENV

## Objetivo

Laboratorio práctico para entender las diferencias entre ARG y ENV en un
Dockerfile: ciclo de vida (build vs runtime), scope en multi-stage builds,
el patrón ARG+ENV para persistir valores, y por qué ARG no debe usarse
para secrets.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada: `docker pull alpine:latest`
- Estar en el directorio `labs/` de este tópico

## Archivos del laboratorio

| Archivo | Propósito |
|---|---|
| `Dockerfile.arg-only` | ARG: disponible en build, no en runtime (Parte 1) |
| `Dockerfile.env-only` | ENV: disponible en build y runtime (Parte 2) |
| `Dockerfile.arg-env` | Patrón ARG + ENV combo (Parte 3) |
| `Dockerfile.arg-before-from` | ARG antes de FROM y re-declaración (Parte 4) |
| `Dockerfile.arg-scope` | Scope de ARG en multi-stage (Parte 4) |
| `Dockerfile.arg-security` | ARG visible en docker history (Parte 5) |

---

## Parte 1 — ARG: solo existe durante el build

### Objetivo

Demostrar que ARG define variables disponibles durante `docker build` pero
que **no persisten** en el contenedor en runtime.

### Paso 1.1: Examinar el Dockerfile

```bash
cat Dockerfile.arg-only
```

Antes de construir, responde mentalmente:

- ¿Qué valor tendrá `$GREETING` durante el build?
- ¿Qué valor tendrá `$GREETING` cuando ejecutes el contenedor?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.2: Construir con valores por defecto

```bash
docker build -f Dockerfile.arg-only -t lab-arg-only .
```

### Paso 1.3: Ejecutar y verificar

```bash
docker run --rm lab-arg-only
```

Salida esperada:

```
=== Build info ===
Build: Hello (build=unknown)
=== Runtime vars ===
GREETING=
BUILD_ID=
```

La información de build se guardó en el archivo (porque RUN se ejecutó durante
el build). Pero las variables `GREETING` y `BUILD_ID` están **vacías** en
runtime — ARG no persiste.

### Paso 1.4: Sobrescribir ARG con --build-arg

```bash
docker build -f Dockerfile.arg-only \
    --build-arg GREETING=Hola \
    --build-arg BUILD_ID=42 \
    -t lab-arg-custom .

docker run --rm lab-arg-custom
```

Salida esperada:

```
=== Build info ===
Build: Hola (build=42)
=== Runtime vars ===
GREETING=
BUILD_ID=
```

Los valores de `--build-arg` se usaron durante el build, pero siguen sin
estar disponibles en runtime.

### Paso 1.5: Limpiar

```bash
docker rmi lab-arg-only lab-arg-custom
```

---

## Parte 2 — ENV: persiste en runtime

### Objetivo

Demostrar que ENV define variables que persisten en la imagen y están
disponibles tanto durante el build como en runtime, y que se pueden
sobrescribir con `docker run -e`.

### Paso 2.1: Examinar el Dockerfile

```bash
cat Dockerfile.env-only
```

Antes de ejecutar, responde mentalmente:

- ¿Las variables estarán disponibles en runtime?
- ¿Qué pasará si usas `docker run -e APP_PORT=9090`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.2: Construir y ejecutar

```bash
docker build -f Dockerfile.env-only -t lab-env-only .
docker run --rm lab-env-only
```

Salida esperada:

```
=== Build info ===
Build: APP_NAME=myapp PORT=8080
=== Runtime vars ===
APP_NAME=myapp
APP_PORT=8080
LOG_LEVEL=info
```

A diferencia de ARG, las variables ENV están disponibles en runtime.

### Paso 2.3: Sobrescribir con -e en runtime

```bash
docker run --rm -e APP_PORT=9090 -e LOG_LEVEL=debug lab-env-only
```

Salida esperada:

```
=== Build info ===
Build: APP_NAME=myapp PORT=8080
=== Runtime vars ===
APP_NAME=myapp
APP_PORT=9090
LOG_LEVEL=debug
```

El build info no cambia (fue grabado durante el build). Pero las variables
de runtime reflejan los valores pasados con `-e`, que tienen prioridad
sobre los valores definidos con ENV en el Dockerfile.

### Paso 2.4: Ver todas las variables de entorno

```bash
docker run --rm lab-env-only env | sort
```

Las variables definidas con ENV aparecen junto a las variables del sistema
(HOME, PATH, HOSTNAME, etc.).

### Paso 2.5: Limpiar

```bash
docker rmi lab-env-only
```

---

## Parte 3 — Patrón ARG + ENV

### Objetivo

Demostrar el patrón que combina ARG y ENV para que un valor pasado durante
el build con `--build-arg` también esté disponible en runtime.

### Paso 3.1: Examinar el Dockerfile

```bash
cat Dockerfile.arg-env
```

El patrón clave es:

```dockerfile
ARG APP_VERSION=1.0.0
ENV APP_VERSION=${APP_VERSION}
```

ARG recibe el valor durante el build. ENV lo persiste en la imagen.

### Paso 3.2: Construir con versión por defecto

```bash
docker build -f Dockerfile.arg-env -t lab-arg-env .
docker run --rm lab-arg-env
```

Salida esperada:

```
=== Build info ===
Build version: 1.0.0
=== Runtime ===
APP_VERSION=1.0.0
```

El valor por defecto (`1.0.0`) se usó en build y persiste en runtime.

### Paso 3.3: Construir con versión personalizada

```bash
docker build -f Dockerfile.arg-env \
    --build-arg APP_VERSION=3.2.1 \
    -t lab-arg-env .

docker run --rm lab-arg-env
```

Salida esperada:

```
=== Build info ===
Build version: 3.2.1
=== Runtime ===
APP_VERSION=3.2.1
```

El valor de `--build-arg` se propagó a través de ENV al runtime.

### Paso 3.4: Sobrescribir en runtime

```bash
docker run --rm -e APP_VERSION=override lab-arg-env
```

Salida esperada:

```
=== Build info ===
Build version: 3.2.1
=== Runtime ===
APP_VERSION=override
```

La precedencia es: `docker run -e` > ENV del Dockerfile > ARG.

### Paso 3.5: Limpiar

```bash
docker rmi lab-arg-env
```

---

## Parte 4 — Scope de ARG

### Objetivo

Demostrar las reglas de scope de ARG: antes de FROM solo es visible en
instrucciones FROM, y cada stage de un multi-stage build tiene su propio
scope.

### Paso 4.1: ARG antes de FROM

```bash
cat Dockerfile.arg-before-from
```

Antes de construir, responde mentalmente:

- ¿Qué valor tendrá `BASE_TAG` en el primer RUN?
- ¿Y después de re-declarar con `ARG BASE_TAG`?

Intenta predecir antes de continuar al siguiente paso.

```bash
docker build -f Dockerfile.arg-before-from -t lab-arg-before-from .
docker run --rm lab-arg-before-from
```

Salida esperada:

```
BASE_TAG=
BASE_TAG (re-declared)=latest
```

La primera línea está vacía porque ARG antes de FROM solo es visible en la
instrucción FROM misma. Para usarlo dentro del stage, hay que re-declararlo.

### Paso 4.2: Cambiar la imagen base con ARG

```bash
docker build -f Dockerfile.arg-before-from \
    --build-arg BASE_TAG=3.19 \
    -t lab-arg-before-from .

docker run --rm lab-arg-before-from
```

Salida esperada:

```
BASE_TAG=
BASE_TAG (re-declared)=3.19
```

El `--build-arg BASE_TAG=3.19` cambió el FROM a `alpine:3.19` y el valor
re-declarado también refleja `3.19`.

### Paso 4.3: Scope en multi-stage

```bash
cat Dockerfile.arg-scope
```

```bash
docker build -f Dockerfile.arg-scope \
    --build-arg SHARED_ARG=custom-value \
    -t lab-arg-scope .

docker run --rm lab-arg-scope
```

Salida esperada:

```
Stage 1: SHARED_ARG=custom-value
Stage 2 (before re-declare): SHARED_ARG=
Stage 2 (after re-declare): SHARED_ARG=custom-value
```

Cada stage tiene su propio scope. El ARG del stage 1 no se hereda al stage 2.
Hay que re-declararlo con `ARG SHARED_ARG` (sin valor por defecto) para que
tome el valor de `--build-arg`.

### Paso 4.4: Limpiar

```bash
docker rmi lab-arg-before-from lab-arg-scope
```

---

## Parte 5 — Seguridad: ARG en docker history

### Objetivo

Demostrar que aunque ARG no persiste como variable de entorno, los valores
quedan **visibles en el historial de la imagen** — por lo que nunca debe
usarse para secrets.

### Paso 5.1: Examinar el Dockerfile

```bash
cat Dockerfile.arg-security
```

### Paso 5.2: Construir con un "secret"

```bash
docker build -f Dockerfile.arg-security \
    --build-arg SECRET_TOKEN=mi_password_super_secreta \
    -t lab-arg-security .
```

### Paso 5.3: Verificar que ARG no está en runtime

```bash
docker run --rm lab-arg-security sh -c 'echo "SECRET_TOKEN=$SECRET_TOKEN"'
```

Salida esperada:

```
SECRET_TOKEN=
```

Parece seguro — la variable no existe en el contenedor.

### Paso 5.4: Pero el valor está en docker history

```bash
docker history lab-arg-security --no-trunc --format "{{.CreatedBy}}"
```

Salida esperada (parcial):

```
|1 SECRET_TOKEN=mi_password_super_secreta /bin/sh -c echo "Setup complete (token used internally)" > /setup.txt
```

El valor del ARG es **completamente visible**. Cualquier persona con acceso
a la imagen puede extraerlo.

### Paso 5.5: Analizar

ARG **no** es seguro para secrets porque:

1. Los valores aparecen en `docker history`
2. Si la imagen se publica en un registry, cualquiera puede verlos
3. Los valores se guardan en las capas intermedias del build cache

**Alternativas seguras**:

- **BuildKit secret mounts**: `docker build --secret id=token,src=./token.txt`
  con `RUN --mount=type=secret,id=token` en el Dockerfile
- **Runtime secrets**: `docker run -e` o Docker secrets en Compose/Swarm
- **Multi-stage builds**: usar el secret solo en el stage de build y no
  copiarlo al stage final

### Paso 5.6: Limpiar

```bash
docker rmi lab-arg-security
```

---

## Limpieza final

```bash
# Eliminar imágenes del lab (si alguna quedó)
docker rmi lab-arg-only lab-arg-custom lab-env-only \
    lab-arg-env lab-arg-before-from lab-arg-scope \
    lab-arg-security 2>/dev/null
```

---

## Conceptos reforzados

1. **ARG** define variables disponibles solo durante `docker build`. No
   persisten en el contenedor. Se sobrescriben con `--build-arg`.

2. **ENV** define variables que persisten en la imagen y están disponibles en
   runtime. Se sobrescriben con `docker run -e`.

3. El patrón **ARG + ENV** (`ARG VAR=default` seguido de `ENV VAR=${VAR}`)
   permite que un valor de build también esté disponible en runtime.

4. El **scope de ARG** es por stage. Un ARG antes de FROM solo es visible en
   instrucciones FROM. En multi-stage, cada stage necesita su propia
   declaración de ARG.

5. ARG **no es seguro para secrets** — los valores son visibles en
   `docker history`. Usar BuildKit secret mounts para secrets en build time.

6. La **precedencia** es: `docker run -e` > ENV del Dockerfile > ARG.
