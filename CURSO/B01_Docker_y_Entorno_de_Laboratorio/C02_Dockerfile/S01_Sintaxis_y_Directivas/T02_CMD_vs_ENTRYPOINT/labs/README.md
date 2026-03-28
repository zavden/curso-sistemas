# Lab — CMD vs ENTRYPOINT

## Objetivo

Laboratorio práctico para entender las diferencias entre CMD y ENTRYPOINT:
cuándo se reemplaza el comando, cuándo se añaden argumentos, cómo interactúan
ambos, el impacto de la forma shell vs exec en señales, y el patrón de wrapper
script usado en imágenes de producción.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada: `docker pull alpine:latest`
- Estar en el directorio `labs/` de este tópico

## Archivos del laboratorio

| Archivo | Propósito |
|---|---|
| `Dockerfile.cmd-only` | CMD solo: comando por defecto reemplazable (Parte 1) |
| `Dockerfile.entrypoint-only` | ENTRYPOINT solo: ejecutable fijo (Parte 2) |
| `Dockerfile.ep-cmd` | ENTRYPOINT + CMD combinados (Parte 3) |
| `Dockerfile.exec-form` | ENTRYPOINT en forma exec (Parte 4) |
| `Dockerfile.shell-form` | ENTRYPOINT en forma shell (Parte 4) |
| `entrypoint.sh` | Wrapper script con `exec "$@"` (Parte 5) |
| `Dockerfile.wrapper` | Imagen con wrapper script (Parte 5) |

---

## Parte 1 — CMD solo: comando por defecto reemplazable

### Objetivo

Demostrar que CMD define un comando por defecto que se **reemplaza
completamente** cuando el usuario pasa argumentos a `docker run`.

### Paso 1.1: Examinar el Dockerfile

```bash
cat Dockerfile.cmd-only
```

Antes de ejecutar, responde mentalmente:

- ¿Qué saldrá si ejecutas `docker run lab-cmd-only`?
- ¿Qué saldrá si ejecutas `docker run lab-cmd-only ls /`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.2: Construir la imagen

```bash
docker build -f Dockerfile.cmd-only -t lab-cmd-only .
```

### Paso 1.3: Ejecutar sin argumentos (CMD por defecto)

```bash
docker run --rm lab-cmd-only
```

Salida esperada:

```
Default CMD: no arguments provided
```

Sin argumentos, Docker ejecuta el CMD definido en el Dockerfile.

### Paso 1.4: Ejecutar con argumentos (CMD reemplazado)

```bash
docker run --rm lab-cmd-only echo "CMD completamente reemplazado"
```

Salida esperada:

```
CMD completamente reemplazado
```

```bash
docker run --rm lab-cmd-only cat /etc/os-release
```

Salida esperada (parcial):

```
NAME="Alpine Linux"
...
```

```bash
docker run --rm lab-cmd-only ls /bin
```

Los argumentos de `docker run` **reemplazan** CMD por completo. El CMD
original `["echo", "Default CMD: no arguments provided"]` desaparece.

### Paso 1.5: Inspeccionar CMD

```bash
docker inspect lab-cmd-only --format '{{json .Config.Cmd}}'
```

Salida esperada:

```json
["echo","Default CMD: no arguments provided"]
```

```bash
docker inspect lab-cmd-only --format '{{json .Config.Entrypoint}}'
```

Salida esperada:

```
null
```

No hay ENTRYPOINT — solo CMD.

### Paso 1.6: Limpiar

```bash
docker rmi lab-cmd-only
```

---

## Parte 2 — ENTRYPOINT solo: ejecutable fijo

### Objetivo

Demostrar que ENTRYPOINT define un ejecutable fijo al que los argumentos de
`docker run` se **añaden**, en lugar de reemplazarlo.

### Paso 2.1: Examinar el Dockerfile

```bash
cat Dockerfile.entrypoint-only
```

Antes de ejecutar, responde mentalmente:

- ¿Qué saldrá si ejecutas `docker run lab-ep-only`?
- ¿Qué saldrá si ejecutas `docker run lab-ep-only hello world`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.2: Construir la imagen

```bash
docker build -f Dockerfile.entrypoint-only -t lab-ep-only .
```

### Paso 2.3: Ejecutar con argumentos

```bash
docker run --rm lab-ep-only "Hello from ENTRYPOINT"
```

Salida esperada:

```
Hello from ENTRYPOINT
```

Docker ejecuta: `echo "Hello from ENTRYPOINT"`. El ENTRYPOINT (`echo`) es
fijo y los argumentos se añaden.

```bash
docker run --rm lab-ep-only one two three
```

Salida esperada:

```
one two three
```

Docker ejecuta: `echo one two three`.

### Paso 2.4: Ejecutar sin argumentos

```bash
docker run --rm lab-ep-only
```

Salida esperada: una línea vacía (echo sin argumentos).

Sin argumentos y sin CMD, Docker ejecuta solo el ENTRYPOINT: `echo` (sin
nada que imprimir).

### Paso 2.5: Sobrescribir ENTRYPOINT con --entrypoint

```bash
docker run --rm --entrypoint cat lab-ep-only /etc/hostname
```

Salida esperada: el hostname del contenedor.

`--entrypoint` reemplaza el ENTRYPOINT de la imagen. Los argumentos después
del nombre de la imagen se pasan al nuevo entrypoint.

```bash
docker run --rm --entrypoint ls lab-ep-only /
```

Muestra el contenido de `/`. El ENTRYPOINT original (`echo`) fue reemplazado
por `ls`.

### Paso 2.6: Limpiar

```bash
docker rmi lab-ep-only
```

---

## Parte 3 — ENTRYPOINT + CMD: el patrón combinado

### Objetivo

Demostrar que cuando se combinan ENTRYPOINT y CMD, CMD proporciona argumentos
por defecto que el usuario puede sobrescribir, mientras ENTRYPOINT permanece
fijo.

### Paso 3.1: Examinar el Dockerfile

```bash
cat Dockerfile.ep-cmd
```

Antes de ejecutar, responde mentalmente:

- ¿Qué ejecuta Docker si no pasas argumentos?
- ¿Qué ejecuta Docker si pasas `"custom argument"`?
- ¿Cuál parte cambia y cuál permanece?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.2: Construir la imagen

```bash
docker build -f Dockerfile.ep-cmd -t lab-ep-cmd .
```

### Paso 3.3: Sin argumentos — ENTRYPOINT + CMD

```bash
docker run --rm lab-ep-cmd
```

Salida esperada:

```
default argument from CMD
```

Docker ejecuta: `echo default argument from CMD`. ENTRYPOINT (`echo`) +
CMD (`default argument from CMD`).

### Paso 3.4: Con argumentos — CMD reemplazado, ENTRYPOINT fijo

```bash
docker run --rm lab-ep-cmd "custom argument"
```

Salida esperada:

```
custom argument
```

Docker ejecuta: `echo custom argument`. ENTRYPOINT se mantiene, CMD fue
reemplazado.

```bash
docker run --rm lab-ep-cmd one two three
```

Salida esperada:

```
one two three
```

Docker ejecuta: `echo one two three`.

### Paso 3.5: Tabla de interacción

Verificar empíricamente la tabla de interacción:

| Dockerfile | `docker run image` | `docker run image arg1` |
|---|---|---|
| CMD solo | Ejecuta CMD | arg1 reemplaza CMD |
| ENTRYPOINT solo | Ejecuta ENTRYPOINT | ENTRYPOINT + arg1 |
| ENTRYPOINT + CMD | ENTRYPOINT + CMD | ENTRYPOINT + arg1 (CMD reemplazado) |

La combinación ENTRYPOINT + CMD permite que la imagen tenga un comportamiento
por defecto (útil) pero que el usuario pueda personalizar los argumentos
sin cambiar el ejecutable.

### Paso 3.6: Inspeccionar la metadata

```bash
docker inspect lab-ep-cmd --format 'ENTRYPOINT={{json .Config.Entrypoint}} CMD={{json .Config.Cmd}}'
```

Salida esperada:

```
ENTRYPOINT=["echo"] CMD=["default argument from CMD"]
```

### Paso 3.7: Limpiar

```bash
docker rmi lab-ep-cmd
```

---

## Parte 4 — Forma shell vs exec: PID 1 y señales

### Objetivo

Demostrar que la forma shell de ENTRYPOINT causa problemas con señales:
`docker stop` tarda ~10 segundos porque `/bin/sh` (PID 1) no propaga SIGTERM
al proceso hijo. La forma exec no tiene este problema.

### Paso 4.1: Examinar los Dockerfiles

```bash
echo "=== Forma exec ==="
cat Dockerfile.exec-form

echo ""
echo "=== Forma shell ==="
cat Dockerfile.shell-form
```

Antes de ejecutar, responde mentalmente:

- ¿Cuál será PID 1 en cada contenedor?
- ¿Cuál responderá más rápido a `docker stop`? ¿Por qué?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.2: Construir ambas imágenes

```bash
docker build -f Dockerfile.exec-form  -t lab-exec-form  .
docker build -f Dockerfile.shell-form -t lab-shell-form .
```

### Paso 4.3: Verificar PID 1 en cada contenedor

```bash
docker run -d --name test-exec lab-exec-form
docker run -d --name test-shell lab-shell-form

# PID 1 en forma exec
docker exec test-exec ps aux
```

Salida esperada:

```
PID   USER     COMMAND
    1 root     sleep 300
```

PID 1 es directamente `sleep`.

```bash
# PID 1 en forma shell
docker exec test-shell ps aux
```

Salida esperada:

```
PID   USER     COMMAND
    1 root     /bin/sh -c sleep 300
    7 root     sleep 300
```

PID 1 es `/bin/sh`, y `sleep` es un proceso hijo.

### Paso 4.4: Medir el tiempo de docker stop

```bash
# Forma exec: docker stop es rápido
echo "=== Exec form ==="
time docker stop test-exec
```

Salida esperada: ~0.3 segundos. `sleep` recibe SIGTERM como PID 1 y termina
inmediatamente.

```bash
# Forma shell: docker stop tarda ~10 segundos
echo "=== Shell form ==="
time docker stop test-shell
```

Salida esperada: ~10 segundos. `/bin/sh` recibe SIGTERM pero no lo propaga a
`sleep`. Docker espera el timeout completo (10s) y envía SIGKILL.

### Paso 4.5: Analizar

```
Forma exec:
  PID 1: sleep 300
    └── docker stop → SIGTERM → sleep termina → ~0.3s

Forma shell:
  PID 1: /bin/sh -c "sleep 300"
    └── PID 7: sleep 300
    └── docker stop → SIGTERM → /bin/sh lo ignora
        → 10s timeout → SIGKILL → terminación forzosa
```

**Regla**: siempre usar forma exec (`["cmd", "arg"]`) para ENTRYPOINT y CMD
en producción. La forma shell causa shutdowns lentos y potencialmente
corrupción de datos si el proceso no puede hacer cleanup.

Además, si ENTRYPOINT usa forma shell, **CMD se ignora completamente** porque
el shell no puede recibirlo como argumento adicional.

### Paso 4.6: Limpiar

```bash
docker rm test-exec test-shell
docker rmi lab-exec-form lab-shell-form
```

---

## Parte 5 — Wrapper script con `exec "$@"`

### Objetivo

Demostrar el patrón de wrapper script usado en imágenes de producción:
un script de entrypoint que hace setup (inicialización, configuración) y
luego ejecuta CMD a través de `exec "$@"`.

### Paso 5.1: Examinar el wrapper script

```bash
cat entrypoint.sh
```

Puntos clave:

- El script hace tareas de inicialización (imprimir info, verificar variables)
- `exec "$@"` al final reemplaza el shell por el comando CMD
- Después de `exec`, el proceso CMD se convierte en PID 1

### Paso 5.2: Examinar el Dockerfile

```bash
cat Dockerfile.wrapper
```

ENTRYPOINT apunta al script, CMD define el comando por defecto.

### Paso 5.3: Construir la imagen

```bash
docker build -f Dockerfile.wrapper -t lab-wrapper .
```

### Paso 5.4: Ejecutar sin argumentos (CMD por defecto)

```bash
docker run --rm lab-wrapper
```

Salida esperada:

```
[entrypoint] Initializing...
[entrypoint] Arguments received: echo Default CMD via wrapper
Default CMD via wrapper
```

El script de entrypoint se ejecuta primero (setup), luego `exec "$@"` ejecuta
el CMD: `echo "Default CMD via wrapper"`.

### Paso 5.5: Ejecutar con argumentos (CMD reemplazado)

```bash
docker run --rm lab-wrapper cat /etc/os-release
```

Salida esperada:

```
[entrypoint] Initializing...
[entrypoint] Arguments received: cat /etc/os-release
NAME="Alpine Linux"
...
```

El entrypoint sigue ejecutándose (setup), pero ahora `exec "$@"` ejecuta
`cat /etc/os-release` en lugar del CMD por defecto.

### Paso 5.6: Con variable de entorno

```bash
docker run --rm -e APP_ENV=production lab-wrapper
```

Salida esperada:

```
[entrypoint] Initializing...
[entrypoint] Arguments received: echo Default CMD via wrapper
[entrypoint] Environment: production
Default CMD via wrapper
```

El script detecta la variable `APP_ENV` y la procesa durante el setup.
Este patrón permite configurar la aplicación a través de variables de entorno
en runtime.

### Paso 5.7: Verificar que exec hace CMD el PID 1

```bash
docker run -d --name test-wrapper lab-wrapper sleep 60
docker exec test-wrapper ps aux
```

Salida esperada:

```
PID   USER     COMMAND
    1 root     sleep 60
```

Gracias a `exec "$@"`, `sleep` es PID 1 — no el shell del script. El script
de entrypoint ya no existe como proceso. Esto garantiza que `docker stop`
propague SIGTERM correctamente.

```bash
docker rm -f test-wrapper
```

### Paso 5.8: Limpiar

```bash
docker rmi lab-wrapper
```

---

## Limpieza final

```bash
# Eliminar imágenes del lab (si alguna quedó)
docker rmi lab-cmd-only lab-ep-only lab-ep-cmd \
    lab-exec-form lab-shell-form lab-wrapper 2>/dev/null

# Eliminar contenedores huérfanos (si alguno quedó)
docker rm -f test-exec test-shell test-wrapper 2>/dev/null
```

---

## Conceptos reforzados

1. **CMD** define un comando por defecto que se **reemplaza completamente**
   cuando el usuario pasa argumentos a `docker run`.

2. **ENTRYPOINT** define un ejecutable fijo al que los argumentos de
   `docker run` se **añaden**. Se puede sobrescribir con `--entrypoint`.

3. La combinación **ENTRYPOINT + CMD** permite que CMD proporcione argumentos
   por defecto que el usuario puede sobrescribir sin cambiar el ejecutable.

4. La **forma exec** (`["cmd", "arg"]`) ejecuta el proceso directamente como
   PID 1, recibe señales correctamente. La **forma shell** ejecuta a través de
   `/bin/sh`, que no propaga SIGTERM — causando `docker stop` de ~10 segundos.

5. El patrón **wrapper script** (`exec "$@"`) permite que un entrypoint haga
   setup (inicialización, configuración) y luego transfiera el control al CMD,
   que se convierte en PID 1.

6. Siempre usar **forma exec** para ENTRYPOINT y CMD en producción. Si
   ENTRYPOINT usa forma shell, CMD se ignora completamente.
