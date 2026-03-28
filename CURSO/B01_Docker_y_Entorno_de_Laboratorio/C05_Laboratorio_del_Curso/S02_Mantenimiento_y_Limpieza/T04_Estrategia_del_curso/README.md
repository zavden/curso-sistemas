# T04 — Estrategia del curso

## Objetivo

Definir una rutina de mantenimiento práctica para el laboratorio del curso:
cuándo limpiar, qué conservar, cómo reutilizar contenedores, y cuánto espacio
mantener disponible.

## Rutina de limpieza recomendada

### Después de cada sesión de trabajo

```bash
docker container prune -f
```

Limpia contenedores efímeros creados con `docker run` durante ejercicios. Los
contenedores del lab (`debian-dev`, `alma-dev`) no se ven afectados si están
corriendo o si se detuvieron con `docker compose stop` (quedan en estado
"exited" pero referenciados por Compose).

### Semanalmente

```bash
docker image prune -f
```

Limpia imágenes dangling (`<none>:<none>`) generadas por rebuilds de las
imágenes del lab. Cada vez que se modifica un Dockerfile y se ejecuta
`docker compose build`, la versión anterior queda como dangling.

### Mensualmente

```bash
docker builder prune --keep-storage 2GB -f
```

Limpia build cache viejo manteniendo al menos 2GB de las capas más recientes.
Esto permite que los rebuilds del lab sigan siendo rápidos sin acumular cache
de builds antiguos.

### Nunca sin verificar

```bash
# SIEMPRE listar antes
docker volume ls

# Solo entonces decidir si limpiar
docker volume prune
```

`docker volume prune` puede eliminar el volumen `workspace` del lab si los
contenedores no están corriendo. Verificar siempre qué volúmenes existen antes
de ejecutar prune.

## Reutilización de contenedores

### Preferir `exec` sobre `run`

```bash
# BIEN: reutiliza el contenedor existente
docker compose exec debian-dev gcc -o hello hello.c

# EVITAR: crea un contenedor nuevo cada vez
docker compose run debian-dev gcc -o hello hello.c
```

`exec` ejecuta un comando dentro de un contenedor ya corriendo — no crea nada
nuevo. `run` crea un contenedor nuevo cada vez, que queda en estado "exited"
después de terminar.

```
Después de 20 ejercicios con "run":
$ docker ps -a | wc -l
22    ← 20 contenedores detenidos + 2 activos del lab

Después de 20 ejercicios con "exec":
$ docker ps -a | wc -l
3     ← solo los 2 contenedores del lab + header
```

### Mantener los contenedores del lab

El flujo diario del laboratorio:

```bash
# Al empezar a trabajar
docker compose up -d

# Durante la sesión: exec para todo
docker compose exec debian-dev bash
docker compose exec alma-dev bash -c 'cd src && make'

# Al terminar (opción 1: detener sin eliminar)
docker compose stop
# Los contenedores quedan en "exited", estado preservado

# Al terminar (opción 2: dejar corriendo)
# No hacer nada — los contenedores idle consumen <1MB de RAM

# Al día siguiente
docker compose start    # si se usó stop
# o simplemente exec si siguen corriendo
```

### Cuándo usar `down`

```bash
# down: elimina contenedores y red, preserva volúmenes e imágenes
docker compose down

# down -v: elimina TODO, incluyendo volúmenes
# Solo usar si se quiere resetear el lab completo
docker compose down -v
```

`down` requiere un `up -d` posterior que recrea los contenedores. El estado
dentro de los contenedores (paquetes instalados manualmente, configuraciones
temporales) se pierde, pero el volumen `workspace` conserva los archivos del
workspace.

`down -v` elimina también el volumen `workspace`. Usar solo para reset total.

## Monitoreo con alias

Agregar al `~/.bashrc` o `~/.zshrc`:

```bash
# Alias de monitoreo
alias docker-space='docker system df'
alias docker-space-detail='docker system df -v'

# Alias de limpieza segura
alias docker-clean='docker container prune -f && docker image prune -f'

# Estado del lab
alias lab-status='docker compose ps'
```

Uso:

```bash
docker-space
# Rápido vistazo al espacio

docker-clean
# Limpieza rápida después de una sesión

lab-status
# Ver si los contenedores del lab están corriendo
```

## Espacio en disco requerido

### Desglose del lab

| Componente | Espacio aproximado |
|---|---|
| Imagen Debian dev | ~700 MB |
| Imagen AlmaLinux dev | ~750 MB |
| Toolchain Rust (por imagen) | ~200 MB |
| Volumen workspace | ~50-100 MB |
| Build cache (activo) | ~500 MB |
| **Total del lab** | **~2-3 GB** |

### Espacio adicional durante el curso

| Actividad | Espacio extra |
|---|---|
| Ejercicios con imágenes adicionales | ~500 MB |
| Compilaciones (binarios, objetos) | ~100 MB |
| Imágenes dangling (entre rebuilds) | ~1 GB |
| Build cache acumulado | ~1-2 GB |
| **Total con uso activo** | **~4-6 GB** |

### Recomendación

Mantener al menos **5 GB libres** en la partición donde Docker almacena datos
(`/var/lib/docker/` o `~/.local/share/docker/`). Si el espacio baja de 2 GB,
ejecutar la rutina de limpieza.

## Qué hacer si algo se rompe

### Se eliminó el volumen workspace

```bash
# Reconstruir: los datos del volumen se pierden
# pero el bind mount (src/) está en el host y es seguro
docker compose down
docker compose up -d
# El volumen se recrea vacío
# src/ sigue intacto (es un bind mount)
```

### Se eliminaron las imágenes

```bash
# Reconstruir desde los Dockerfiles
docker compose build
docker compose up -d
```

Los Dockerfiles están versionados en el proyecto. Todo se puede reconstruir.

### Se eliminó un contenedor del lab

```bash
# Recrear solo el contenedor eliminado
docker compose up -d debian-dev
# o
docker compose up -d alma-dev
```

### Principio general

Todo lo que importa está en dos lugares:

1. **Código fuente**: en `src/` del host (bind mount) — protegido por Git
2. **Dockerfiles y compose.yml**: en el proyecto — protegidos por Git

Los contenedores, imágenes y volúmenes son **reconstruibles**. La pérdida de un
volumen workspace implica perder configuraciones temporales del contenedor
(historial de shell, paquetes extra instalados a mano), pero no el código fuente.

---

## Ejercicios

### Ejercicio 1 — Configurar los alias

```bash
# Agregar al shell config
cat >> ~/.bashrc << 'EOF'

# Docker lab aliases
alias docker-space='docker system df'
alias docker-clean='docker container prune -f && docker image prune -f'
alias lab-status='docker compose ps'
EOF

# Cargar
source ~/.bashrc

# Probar
docker-space
lab-status
```

### Ejercicio 2 — Ejecutar la rutina completa

```bash
echo "=== Espacio antes ==="
docker system df

echo ""
echo "=== Limpieza segura ==="
docker container prune -f
docker image prune -f
docker builder prune --keep-storage 2GB -f

echo ""
echo "=== Espacio después ==="
docker system df

echo ""
echo "=== Estado del lab ==="
docker compose ps
```

### Ejercicio 3 — Verificar resiliencia del lab

```bash
# 1. Verificar que el lab funciona
./verify-env.sh

# 2. Ejecutar limpieza segura
docker container prune -f
docker image prune -f

# 3. Verificar que el lab sigue funcionando
./verify-env.sh

# Resultado esperado: el lab no se ve afectado por la limpieza segura
# porque sus contenedores están corriendo y sus imágenes están en uso
```
