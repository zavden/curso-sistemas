# T02 — CMD vs ENTRYPOINT

## La diferencia fundamental

CMD y ENTRYPOINT definen qué se ejecuta cuando un contenedor arranca, pero con
comportamientos opuestos ante los argumentos de `docker run`:

```
CMD ["python3", "app.py"]
  docker run image            → python3 app.py
  docker run image bash       → bash            (CMD reemplazado)

ENTRYPOINT ["python3"]
  docker run image            → python3
  docker run image app.py     → python3 app.py  (args añadidos)
```

| | CMD | ENTRYPOINT |
|---|---|---|
| Argumentos de `docker run` | **Reemplazan** CMD completo | Se **añaden** al ENTRYPOINT |
| Propósito | Comando por defecto, flexible | Ejecutable fijo de la imagen |
| Sobrescribir | `docker run image otro_cmd` | `docker run --entrypoint otro image` |
| Múltiples instrucciones | Solo el **último** CMD tiene efecto | Solo el **último** ENTRYPOINT tiene efecto |

## CMD solo

La forma más simple. El contenedor tiene un comando por defecto que el usuario
puede reemplazar:

```dockerfile
FROM python:3.12-slim
WORKDIR /app
COPY . .
CMD ["python3", "app.py"]
```

```bash
# Usa el CMD por defecto
docker run myimage
# Ejecuta: python3 app.py

# Reemplaza CMD completamente
docker run myimage python3 -c "print('hello')"
# Ejecuta: python3 -c "print('hello')"

# Reemplaza CMD con bash para debugging
docker run -it myimage bash
# Abre un shell interactivo
```

CMD solo es apropiado cuando quieres **flexibilidad total** para el usuario.

## ENTRYPOINT solo

El contenedor siempre ejecuta el mismo programa. Los argumentos de `docker run`
se pasan como argumentos del programa:

```dockerfile
FROM python:3.12-slim
ENTRYPOINT ["python3"]
```

```bash
docker run myimage app.py
# Ejecuta: python3 app.py

docker run myimage --version
# Ejecuta: python3 --version

docker run myimage -c "print(42)"
# Ejecuta: python3 -c "print(42)"

# Sin argumentos:
docker run myimage
# Ejecuta: python3 (abre el REPL interactivo)
```

ENTRYPOINT solo es apropiado cuando la imagen **es** el programa (ej: una imagen
que siempre ejecuta `curl`, `jq`, o `ffmpeg`).

## ENTRYPOINT + CMD — El patrón más potente

Cuando se combinan, CMD proporciona **argumentos por defecto** que el usuario
puede sobrescribir, mientras ENTRYPOINT permanece fijo:

```dockerfile
FROM python:3.12-slim
ENTRYPOINT ["python3"]
CMD ["--help"]
```

```bash
# Sin argumentos: CMD se usa como args por defecto
docker run myimage
# Ejecuta: python3 --help

# Con argumentos: CMD se reemplaza, ENTRYPOINT permanece
docker run myimage app.py
# Ejecuta: python3 app.py

docker run myimage -c "print('hello')"
# Ejecuta: python3 -c "print('hello')"
```

### Ejemplo real: imagen de base de datos

```dockerfile
FROM debian:bookworm-slim
# ... instalación de PostgreSQL ...
ENTRYPOINT ["docker-entrypoint.sh"]
CMD ["postgres"]
```

```bash
# Uso normal: arranca PostgreSQL
docker run postgres:16
# Ejecuta: docker-entrypoint.sh postgres

# Modo debug: arranca bash en lugar de postgres
docker run postgres:16 bash
# Ejecuta: docker-entrypoint.sh bash
# El entrypoint script detecta que no es "postgres" y lo ejecuta directamente
```

## Tabla de interacción completa

| Dockerfile | `docker run image` | `docker run image arg1` | `docker run image arg1 arg2` |
|---|---|---|---|
| `CMD ["a","b"]` | `a b` | `arg1` | `arg1 arg2` |
| `ENTRYPOINT ["x"]` | `x` | `x arg1` | `x arg1 arg2` |
| `ENTRYPOINT ["x"]` + `CMD ["a","b"]` | `x a b` | `x arg1` | `x arg1 arg2` |
| `ENTRYPOINT ["x","y"]` + `CMD ["a"]` | `x y a` | `x y arg1` | `x y arg1 arg2` |

**Nota**: cuando hay argumentos de `docker run`, estos reemplazan CMD completo.
ENTRYPOINT siempre permanece fijo (a menos que uses `--entrypoint`).

## Forma shell vs forma exec

### Forma exec (recomendada)

```dockerfile
ENTRYPOINT ["python3", "app.py"]
CMD ["--port", "8080"]
```

El proceso especificado se convierte en **PID 1** directamente. Recibe señales
correctamente (`docker stop` envía SIGTERM al proceso):

```
PID 1: python3 app.py --port 8080
  └── SIGTERM → python3 lo recibe → shutdown limpio
```

### Forma shell

```dockerfile
ENTRYPOINT python3 app.py
```

PID 1 es `/bin/sh -c "python3 app.py"`. El shell **no propaga SIGTERM** al
proceso hijo:

```
PID 1: /bin/sh -c "python3 app.py"
  └── PID 2: python3 app.py
        └── SIGTERM → /bin/sh lo recibe pero NO lo propaga a python3
            → docker stop espera 10s y envía SIGKILL (terminación abrupta)
```

Además, si ENTRYPOINT usa forma shell, **CMD se ignora completamente**:

```dockerfile
# CMD se ignora porque ENTRYPOINT está en forma shell
ENTRYPOINT python3 app.py
CMD ["--port", "8080"]
# Ejecuta: /bin/sh -c "python3 app.py" — sin --port 8080
```

Esto ocurre porque la forma shell envuelve todo en `/bin/sh -c "..."`, y CMD
no tiene dónde concatenarse.

**Regla**: siempre usar forma exec (`["cmd", "arg"]`) para ENTRYPOINT y CMD en
producción.

### Resumen de diferencias entre formas

| Aspecto | Forma exec `["cmd", "arg"]` | Forma shell `cmd arg` |
|---|---|---|
| PID 1 | El proceso directo | `/bin/sh -c "cmd arg"` |
| Propagación de señales | Correcta | No funciona |
| Procesamiento de variables | No (raw) | Sí (`$VAR`, `$PATH`) |
| Procesamiento de redirecciones | No | Sí (`>`, `|`) |
| CMD como argumentos por defecto | Sí | No (se ignora si ENTRYPOINT es shell) |

## Sobrescribir ENTRYPOINT en runtime

`--entrypoint` permite reemplazar el ENTRYPOINT de la imagen. Es útil para
debugging:

```bash
# La imagen tiene ENTRYPOINT ["python3"] y CMD ["app.py"]
# Necesitas un shell para investigar un problema
docker run -it --entrypoint bash myimage
# Ejecuta: bash app.py
# (CMD sigue aplicando como argumentos al nuevo ENTRYPOINT)

# Para abrir bash SIN argumentos, pasar un argumento vacío explícito
docker run -it --entrypoint bash myimage
# o sobreescribir también CMD:
docker run -it --entrypoint /bin/sh myimage -c "echo hello"
# Ejecuta: /bin/sh -c "echo hello"

# NOTA: --entrypoint va ANTES del nombre de la imagen
# Los argumentos DESPUÉS de la imagen reemplazan CMD
docker run --entrypoint cat myimage /etc/os-release
# Ejecuta: cat /etc/os-release (CMD reemplazado por /etc/os-release)
```

> **Importante**: `--entrypoint` solo reemplaza ENTRYPOINT. CMD sigue
> aplicando como argumentos por defecto a menos que se proporcionen argumentos
> después del nombre de imagen.

## Múltiples instrucciones CMD/ENTRYPOINT

Solo el **último** CMD o ENTRYPOINT en el Dockerfile tiene efecto. Los anteriores
se sobrescriben:

```dockerfile
# Este Dockerfile NO ejecuta "echo uno" ni "echo dos"
# Solo ejecuta "echo tres"
CMD ["echo", "uno"]
CMD ["echo", "dos"]
CMD ["echo", "tres"]
```

Esto también aplica a la herencia de imágenes base:

```dockerfile
# python:3.12-slim tiene CMD ["python3"]
FROM python:3.12-slim
CMD ["echo", "override"]
# Este CMD reemplaza completamente el de la base
```

## Patrón: wrapper script con `exec "$@"`

Muchas imágenes oficiales usan un script de entrypoint que hace setup y luego
ejecuta el CMD. Este es el patrón más importante de este tópico:

```bash
#!/bin/sh
# docker-entrypoint.sh

# Setup: crear directorios, ajustar permisos, leer secrets
echo "Inicializando..."
mkdir -p /app/data

# Si hay variables de entorno para configurar, procesarlas
if [ -n "$DATABASE_URL" ]; then
    echo "Configurando conexión a base de datos..."
fi

# exec reemplaza el shell por el comando — CMD se convierte en PID 1
exec "$@"
```

```dockerfile
FROM debian:bookworm-slim
COPY docker-entrypoint.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/docker-entrypoint.sh

ENTRYPOINT ["docker-entrypoint.sh"]
CMD ["python3", "app.py"]
```

```bash
# Uso normal:
docker run myimage
# 1. Ejecuta docker-entrypoint.sh python3 app.py
# 2. El script hace setup
# 3. exec python3 app.py → python3 se convierte en PID 1

# Debug:
docker run myimage bash
# 1. Ejecuta docker-entrypoint.sh bash
# 2. El script hace setup
# 3. exec bash → bash se convierte en PID 1
```

El `exec "$@"` es crucial:

- `exec` reemplaza el proceso del shell por el comando (el shell desaparece)
- `"$@"` expande a todos los argumentos pasados al script (que son CMD)
- Resultado: el comando CMD se convierte en PID 1, recibe señales correctamente

Sin `exec`, el shell permanecería como PID 1 y no propagaría señales.

## Inspeccionar CMD y ENTRYPOINT de una imagen

```bash
# Ver CMD
docker inspect --format '{{json .Config.Cmd}}' nginx
# ["nginx","-g","daemon off;"]

# Ver ENTRYPOINT
docker inspect --format '{{json .Config.Entrypoint}}' nginx
# ["/docker-entrypoint.sh"]

# Ambos juntos
docker inspect --format 'ENTRYPOINT={{json .Config.Entrypoint}} CMD={{json .Config.Cmd}}' nginx
# ENTRYPOINT=["/docker-entrypoint.sh"] CMD=["nginx","-g","daemon off;"]
```

## Casos borde

### 1. Variables de entorno en forma exec

```dockerfile
ENV NAME=world
CMD ["echo", "Hello, $NAME"]
# Output: Hello, $NAME  (NO sustituye $NAME — forma exec es literal)
```

```dockerfile
ENV NAME=world
CMD echo "Hello, $NAME"
# Output: Hello, world  (forma shell procesa variables)
```

Si necesitas variables en forma exec, envuélvelo en shell explícitamente:

```dockerfile
ENV NAME=world
CMD ["sh", "-c", "echo Hello, $NAME"]
# Output: Hello, world  (el shell interno procesa $NAME)
```

### 2. Redirecciones en forma exec

```dockerfile
# Forma exec — las redirecciones NO funcionan
CMD ["python3", "-c", "print('hello')", ">", "/tmp/out.txt"]
# Intenta usar ">" como argumento de python3, no como redirección

# Forma shell — las redirecciones SÍ funcionan
CMD python3 -c "print('hello')" > /tmp/out.txt
```

### 3. ENTRYPOINT con CMD vacío

```dockerfile
ENTRYPOINT ["python3"]
CMD []
```

`docker run image` ejecuta solo `python3` (sin argumentos — abre el REPL).

## ¿Cuál usar?

| Caso de uso | Recomendación | Ejemplo |
|---|---|---|
| Aplicación genérica | CMD solo | `CMD ["python3", "app.py"]` |
| Imagen = herramienta CLI | ENTRYPOINT solo | `ENTRYPOINT ["curl"]` |
| Servicio con config por defecto | ENTRYPOINT + CMD | `ENTRYPOINT ["entrypoint.sh"]` + `CMD ["serve"]` |
| Imagen que debe aceptar cualquier comando | CMD solo | `CMD ["bash"]` |
| Necesitas setup antes de ejecutar | ENTRYPOINT + wrapper script | `ENTRYPOINT ["setup.sh"]` + `CMD ["app"]` |
| Debugging fácil | CMD solo | Permite `docker run image bash` sin `--entrypoint` |

## Resumen visual

```
┌─────────────────────────────────────────────────────────────┐
│                    docker run image args                    │
└─────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┴───────────────┐
              │                               │
              ▼                               ▼
      ┌───────────────┐               ┌───────────────┐
      │   ENTRYPOINT  │               │      CMD      │
      │   (siempre)   │               │  (por defecto) │
      └───────┬───────┘               └───────┬───────┘
              │                               │
              │     ┌─────────────────┐       │
              └────►│   CONCATENAR    │◄──────┘
                    │  EP + CMD/args  │
                    └────────┬────────┘
                             │
                             ▼
                      ┌─────────────┐
                      │   EJECUTAR  │
                      └─────────────┘

CMD solo:         run → [CMD]
ENTRYPOINT solo:  run args → [EP] args
EP + CMD:         run → [EP] [CMD]
                  run args → [EP] args  (CMD reemplazado)
```

---

## Práctica

> **Nota**: este tópico tiene un directorio `labs/` con Dockerfiles preparados
> para ejercicios más detallados.

### Ejercicio 1 — CMD reemplazable

```bash
mkdir -p /tmp/docker_cmd && cat > /tmp/docker_cmd/Dockerfile << 'EOF'
FROM alpine:latest
CMD ["echo", "Soy el CMD por defecto"]
EOF

docker build -t cmd-test /tmp/docker_cmd

# CMD por defecto
docker run --rm cmd-test
# Soy el CMD por defecto

# Reemplazar CMD
docker run --rm cmd-test echo "CMD reemplazado"
# CMD reemplazado

docker run --rm cmd-test cat /etc/os-release
# NAME="Alpine Linux" ...

docker rmi cmd-test
rm -rf /tmp/docker_cmd
```

**Predicción**: sin argumentos, ejecuta el CMD del Dockerfile. Con argumentos,
el CMD se reemplaza completamente — `cat /etc/os-release` no tiene nada que ver
con `echo`, demuestra que CMD es flexible.

### Ejercicio 2 — ENTRYPOINT + CMD como args por defecto

```bash
mkdir -p /tmp/docker_ep && cat > /tmp/docker_ep/Dockerfile << 'EOF'
FROM alpine:latest
ENTRYPOINT ["echo"]
CMD ["argumento por defecto"]
EOF

docker build -t ep-test /tmp/docker_ep

# Sin args: usa CMD como args
docker run --rm ep-test
# argumento por defecto

# Con args: reemplaza CMD
docker run --rm ep-test "argumento del usuario"
# argumento del usuario

docker run --rm ep-test uno dos tres
# uno dos tres

docker rmi ep-test
rm -rf /tmp/docker_ep
```

**Predicción**: ENTRYPOINT `echo` permanece fijo. Sin argumentos, CMD
`["argumento por defecto"]` se concatena: `echo argumento por defecto`. Con
argumentos, CMD se reemplaza: `echo uno dos tres`.

### Ejercicio 3 — Wrapper script con exec "$@"

```bash
mkdir -p /tmp/docker_wrapper

cat > /tmp/docker_wrapper/entrypoint.sh << 'SCRIPT'
#!/bin/sh
echo "[entrypoint] Setup: $(date)"
echo "[entrypoint] Argumentos recibidos: $@"
exec "$@"
SCRIPT

cat > /tmp/docker_wrapper/Dockerfile << 'EOF'
FROM alpine:latest
COPY entrypoint.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/entrypoint.sh
ENTRYPOINT ["entrypoint.sh"]
CMD ["echo", "CMD por defecto"]
EOF

docker build -t wrapper-test /tmp/docker_wrapper

# CMD por defecto
docker run --rm wrapper-test
# [entrypoint] Setup: ...
# [entrypoint] Argumentos recibidos: echo CMD por defecto
# CMD por defecto

# CMD reemplazado
docker run --rm wrapper-test sh -c "echo 'Hola desde shell'"
# [entrypoint] Setup: ...
# [entrypoint] Argumentos recibidos: sh -c echo 'Hola desde shell'
# Hola desde shell

docker rmi wrapper-test
rm -rf /tmp/docker_wrapper
```

**Predicción**: el entrypoint script siempre ejecuta primero (setup), luego
`exec "$@"` reemplaza el script con CMD. El script es PID 1 brevemente; después
de `exec`, CMD se convierte en PID 1. Sin `exec`, el script permanecería como
PID 1 y no propagaría señales.

### Ejercicio 4 — Forma shell y el problema de señales

```bash
mkdir -p /tmp/docker_signal

cat > /tmp/docker_signal/Dockerfile.exec << 'EOF'
FROM alpine:latest
ENTRYPOINT ["sleep", "3600"]
EOF

cat > /tmp/docker_signal/Dockerfile.shell << 'EOF'
FROM alpine:latest
ENTRYPOINT sleep 3600
EOF

docker build -t signal-exec -f /tmp/docker_signal/Dockerfile.exec /tmp/docker_signal
docker build -t signal-shell -f /tmp/docker_signal/Dockerfile.shell /tmp/docker_signal

# Forma exec: docker stop es rápido
docker run -d --name test_exec signal-exec
time docker stop test_exec   # ~0.3s — sleep recibe SIGTERM y termina

# Forma shell: docker stop tarda ~10s (timeout completo)
docker run -d --name test_shell signal-shell
time docker stop test_shell  # ~10s — sh no propaga SIGTERM, espera al SIGKILL

docker rm test_exec test_shell
docker rmi signal-exec signal-shell
rm -rf /tmp/docker_signal
```

**Predicción**: forma exec termina en <1 segundo porque `sleep` (PID 1) recibe
SIGTERM directamente y sale. Forma shell tarda ~10 segundos porque PID 1 es
`/bin/sh` que no propaga SIGTERM a `sleep`; Docker espera el timeout y envía
SIGKILL. Este es el argumento principal para usar siempre forma exec.

### Ejercicio 5 — Variables de entorno en forma exec vs shell

```bash
mkdir -p /tmp/docker_env

cat > /tmp/docker_env/Dockerfile.exec << 'EOF'
FROM alpine:latest
ENV GREETING=hello
CMD ["echo", "$GREETING world"]
EOF

cat > /tmp/docker_env/Dockerfile.shell << 'EOF'
FROM alpine:latest
ENV GREETING=hello
CMD echo "$GREETING world"
EOF

docker build -t env-exec -f /tmp/docker_env/Dockerfile.exec /tmp/docker_env
docker build -t env-shell -f /tmp/docker_env/Dockerfile.shell /tmp/docker_env

echo "=== Forma exec ==="
docker run --rm env-exec
# $GREETING world  ← literal, sin expandir

echo "=== Forma shell ==="
docker run --rm env-shell
# hello world  ← variable expandida por /bin/sh

docker rmi env-exec env-shell
rm -rf /tmp/docker_env
```

**Predicción**: forma exec no tiene shell que procese `$GREETING`, así que se
imprime como texto literal. Forma shell pasa por `/bin/sh -c`, que expande la
variable. Si necesitas variables en forma exec, usa `CMD ["sh", "-c", "echo $GREETING world"]`.

### Ejercicio 6 — Inspeccionar imágenes oficiales

```bash
# Ver cómo las imágenes oficiales combinan ENTRYPOINT y CMD

echo "=== nginx ==="
docker inspect --format 'ENTRYPOINT={{json .Config.Entrypoint}} CMD={{json .Config.Cmd}}' nginx:alpine
# ENTRYPOINT=["/docker-entrypoint.sh"] CMD=["nginx","-g","daemon off;"]

echo "=== postgres ==="
docker pull postgres:16-alpine
docker inspect --format 'ENTRYPOINT={{json .Config.Entrypoint}} CMD={{json .Config.Cmd}}' postgres:16-alpine
# ENTRYPOINT=["docker-entrypoint.sh"] CMD=["postgres"]

echo "=== alpine ==="
docker inspect --format 'ENTRYPOINT={{json .Config.Entrypoint}} CMD={{json .Config.Cmd}}' alpine:latest
# ENTRYPOINT=<nil> CMD=["/bin/sh"]

echo "=== redis ==="
docker pull redis:alpine
docker inspect --format 'ENTRYPOINT={{json .Config.Entrypoint}} CMD={{json .Config.Cmd}}' redis:alpine
# ENTRYPOINT=["docker-entrypoint.sh"] CMD=["redis-server"]
```

**Predicción**: nginx, postgres y redis usan el patrón ENTRYPOINT (wrapper
script) + CMD (servicio por defecto). alpine usa solo CMD (`/bin/sh`), sin
ENTRYPOINT. El wrapper script (`docker-entrypoint.sh`) hace setup y luego
`exec "$@"` con el CMD.

### Ejercicio 7 — ENTRYPOINT shell ignora CMD

```bash
mkdir -p /tmp/docker_shell_ep

cat > /tmp/docker_shell_ep/Dockerfile << 'EOF'
FROM alpine:latest
ENTRYPOINT echo "Solo ENTRYPOINT"
CMD ["echo", "Esto es CMD"]
EOF

docker build -t shell-ep-test /tmp/docker_shell_ep

# CMD se ignora porque ENTRYPOINT usa forma shell
docker run --rm shell-ep-test
# Solo ENTRYPOINT
# (NO aparece "Esto es CMD")

# Los argumentos también se ignoran
docker run --rm shell-ep-test argumento extra
# Solo ENTRYPOINT
# (los argumentos se pierden)

docker rmi shell-ep-test
rm -rf /tmp/docker_shell_ep
```

**Predicción**: con ENTRYPOINT en forma shell, Docker ejecuta
`/bin/sh -c "echo Solo ENTRYPOINT"`. CMD no tiene dónde concatenarse y se
ignora. Los argumentos de `docker run` también se ignoran. Esta es una de
las razones principales para usar siempre forma exec.

### Ejercicio 8 — Sobrescribir ENTRYPOINT con --entrypoint

```bash
mkdir -p /tmp/docker_override && cat > /tmp/docker_override/Dockerfile << 'EOF'
FROM alpine:latest
ENTRYPOINT ["echo", "ENTRYPOINT fijo:"]
CMD ["arg-por-defecto"]
EOF

docker build -t override-test /tmp/docker_override

# Ejecución normal
docker run --rm override-test
# ENTRYPOINT fijo: arg-por-defecto

# Reemplazar solo CMD (args después de la imagen)
docker run --rm override-test nuevo-arg
# ENTRYPOINT fijo: nuevo-arg

# Reemplazar ENTRYPOINT con --entrypoint
docker run --rm --entrypoint cat override-test /etc/hostname
# (muestra el hostname del contenedor — ENTRYPOINT reemplazado)

# --entrypoint para debugging: abrir shell
docker run --rm -it --entrypoint /bin/sh override-test
# Abre shell interactivo (la shell ignora CMD "arg-por-defecto")

docker rmi override-test
rm -rf /tmp/docker_override
```

**Predicción**: `--entrypoint cat` reemplaza ENTRYPOINT, y `/etc/hostname`
(después de la imagen) reemplaza CMD. `--entrypoint /bin/sh` abre un shell
interactivo. Nota que `--entrypoint` va antes del nombre de imagen, los
argumentos después.
