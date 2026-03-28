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

| Dockerfile | `docker run image` | `docker run image arg1` |
|---|---|---|
| `CMD ["a","b"]` | `a b` | `arg1` |
| `ENTRYPOINT ["x"]` | `x` | `x arg1` |
| `ENTRYPOINT ["x"]` + `CMD ["a","b"]` | `x a b` | `x arg1` |
| `ENTRYPOINT ["x","y"]` + `CMD ["a"]` | `x y a` | `x y arg1` |

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
CMD --port 8080
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

**Regla**: siempre usar forma exec (`["cmd", "arg"]`) para ENTRYPOINT y CMD en
producción.

## Sobrescribir ENTRYPOINT en runtime

`--entrypoint` permite reemplazar el ENTRYPOINT de la imagen. Es útil para
debugging:

```bash
# La imagen tiene ENTRYPOINT ["python3"]
# Necesitas un shell para investigar un problema
docker run -it --entrypoint bash myimage

# Ejecutar un comando diferente
docker run --entrypoint cat myimage /etc/os-release

# NOTA: --entrypoint va ANTES del nombre de la imagen
docker run --entrypoint /bin/sh myimage -c "echo hello"
# Ejecuta: /bin/sh -c "echo hello"
```

## Patrón: wrapper script con `exec "$@"`

Muchas imágenes usan un script de entrypoint que hace setup y luego ejecuta
el CMD:

```bash
#!/bin/bash
# docker-entrypoint.sh

# Setup: crear directorios, ajustar permisos, leer secrets
echo "Inicializando..."
mkdir -p /app/data
chown -R appuser:appuser /app/data

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

## ¿Cuál usar?

| Caso de uso | Recomendación | Ejemplo |
|---|---|---|
| Aplicación genérica | CMD solo | `CMD ["python3", "app.py"]` |
| Imagen = herramienta CLI | ENTRYPOINT solo | `ENTRYPOINT ["curl"]` |
| Servicio con configuración | ENTRYPOINT + CMD | `ENTRYPOINT ["entrypoint.sh"]` + `CMD ["serve"]` |
| Debugging fácil | CMD solo (o ENTRYPOINT + CMD) | Permite `docker run image bash` |

---

## Ejercicios

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

# Forma shell: docker stop tarda 10s (timeout completo)
docker run -d --name test_shell signal-shell
time docker stop test_shell  # ~10s — sh no propaga SIGTERM, espera al SIGKILL

docker rm test_exec test_shell
docker rmi signal-exec signal-shell
rm -rf /tmp/docker_signal
```
