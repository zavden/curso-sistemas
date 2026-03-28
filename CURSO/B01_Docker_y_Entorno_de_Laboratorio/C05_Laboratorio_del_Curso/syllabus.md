# Capítulo 5: Laboratorio del Curso [M]

**Bloque**: B01 — Docker y Entorno de Laboratorio
**Tipo**: Multi-temático (requiere C01-C04)
**Objetivo**: Construir el entorno de desarrollo completo del curso con contenedores Docker
personalizados para Debian y Alma Linux, y establecer las prácticas de mantenimiento
y limpieza que se usarán durante todo el curso.

---

## Sección 1: Construcción del Entorno

### T01 — Dockerfile Debian dev

**Conceptos a cubrir**:
- Imagen base: `debian:bookworm` (Debian 12, soporte hasta 2028)
- Herramientas a instalar:
  - **Compilación C**: gcc, g++, make, cmake, pkg-config
  - **Debugging**: gdb, valgrind, strace, ltrace
  - **Documentación**: man-db, manpages-dev, manpages-posix-dev (man pages de POSIX)
  - **Utilidades**: git, curl, wget, vim/nano, less, tree, file, bc
  - **Rust**: rustup (instalación del toolchain stable, components: rustfmt, clippy)
  - **Desarrollo de red**: netcat-openbsd, iproute2, iputils-ping, dnsutils, tcpdump
- Estructura del Dockerfile:
  - Multi-stage no es necesario aquí (es una imagen de desarrollo, no de producción)
  - Combinar RUN de apt en una sola capa con limpieza:
    ```dockerfile
    RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc g++ make cmake gdb valgrind ... \
        && rm -rf /var/lib/apt/lists/*
    ```
  - Instalar rustup como usuario non-root (no como root):
    ```dockerfile
    RUN useradd -m -s /bin/bash dev
    USER dev
    RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | \
        sh -s -- -y --default-toolchain stable --component rustfmt clippy
    ```
  - ENV PATH para incluir `~/.cargo/bin`
  - WORKDIR `/home/dev/workspace`
- Configuración de locale: `LANG=C.UTF-8` para soporte de caracteres especiales

**Comportamientos importantes**:
- `--no-install-recommends` reduce significativamente el tamaño de la imagen
  (evita paquetes sugeridos que no se necesitan)
- Las man pages están excluidas por defecto en imágenes Docker Debian — hay que
  revertir la exclusión modificando `/etc/dpkg/dpkg.cfg.d/docker` antes de instalar paquetes
- rustup instala en `$HOME/.rustup` y `$HOME/.cargo` — como el USER cambia,
  estos directorios deben ser del usuario correcto
- gdb necesita `--cap-add=SYS_PTRACE` en runtime para funcionar correctamente
  con contenedores

**Práctica**:
- Escribir el Dockerfile completo
- Construir la imagen y verificar que todas las herramientas funcionan
- Compilar y ejecutar un "Hello World" en C y en Rust dentro del contenedor
- Verificar que gdb y valgrind funcionan (con SYS_PTRACE)

### T02 — Dockerfile Alma dev

**Conceptos a cubrir**:
- Imagen base: `almalinux:9` (compatible RHEL 9, soporte hasta 2032)
- Herramientas a instalar:
  - **Compilación C**: gcc, gcc-c++, make, cmake, pkgconf-pkg-config
  - **Debugging**: gdb, valgrind, strace, ltrace
  - **Documentación**: man-db, man-pages (POSIX man pages vía paquetes adicionales)
  - **Utilidades**: git, curl, wget, vim, less, tree, file, bc
  - **Rust**: rustup (misma instalación que Debian)
  - **Desarrollo de red**: nmap-ncat, iproute, iputils, bind-utils, tcpdump
  - **Repositorios extra**: EPEL (Extra Packages for Enterprise Linux) para paquetes
    no incluidos en los repos base
- Diferencias con Debian:
  - Gestor de paquetes: `dnf` en vez de `apt`
  - Nombres de paquetes diferentes (gcc-c++ vs g++, bind-utils vs dnsutils, etc.)
  - Groups de paquetes: `dnf group install "Development Tools"` (alternativa a instalar uno a uno)
  - Cache: `dnf clean all` en vez de `rm -rf /var/lib/apt/lists/*`
- Configuración EPEL:
  ```dockerfile
  RUN dnf install -y epel-release && dnf clean all
  ```
- El resto de la estructura (usuario, rustup, WORKDIR) es idéntica a Debian

**Comportamientos importantes**:
- AlmaLinux 9 usa gcc 11 por defecto — para versiones más recientes se necesita
  Application Streams (AppStream): `dnf install gcc-toolset-13`
- dnf tiene mejor resolución de dependencias que yum (su predecesor)
- Los paquetes de EPEL son community-maintained — no tienen el mismo nivel de soporte
  que los repos base
- En RHEL real, se necesita una suscripción para los repositorios; AlmaLinux los tiene
  abiertos

**Práctica**:
- Escribir el Dockerfile completo
- Construir y verificar paridad funcional con la imagen Debian
- Documentar las diferencias de nombres de paquetes encontradas

### T03 — docker-compose.yml del curso

**Conceptos a cubrir**:
- Diseño del Compose file del laboratorio:
  ```yaml
  services:
    debian-dev:
      build:
        context: ./dockerfiles/debian
      volumes:
        - workspace:/home/dev/workspace
        - ./src:/home/dev/workspace/src
      cap_add:
        - SYS_PTRACE        # Para gdb y strace
      stdin_open: true       # -i
      tty: true              # -t
      
    alma-dev:
      build:
        context: ./dockerfiles/alma
      volumes:
        - workspace:/home/dev/workspace
        - ./src:/home/dev/workspace/src
      cap_add:
        - SYS_PTRACE
      stdin_open: true
      tty: true

  volumes:
    workspace:
  ```
- Volúmenes:
  - Named volume `workspace`: datos persistentes del trabajo
  - Bind mount `./src`: código fuente compartido entre host y contenedores
  - Ambos contenedores comparten el mismo código fuente: escribir una vez,
    compilar en ambas distros
- Networking: la red default de Compose es suficiente para el lab
  (los contenedores pueden comunicarse si es necesario)
- `cap_add: SYS_PTRACE`: necesario para gdb, strace, valgrind dentro de los contenedores
- Consideraciones de espacio:
  - Las dos imágenes comparten pocas capas (bases diferentes) — ocuparán espacio independiente
  - Estimar ~500MB-1GB por imagen de desarrollo

**Comportamientos importantes**:
- `stdin_open` + `tty` equivale a `docker run -it` — permite `docker compose exec` interactivo
- Los bind mounts del código fuente hacen que los cambios sean visibles inmediatamente
  en ambos contenedores sin rebuild
- Al compartir `./src`, los binarios compilados en Debian y Alma se mezclan si se compila
  en el mismo directorio — usar subdirectorios de build por distro, o build directories
  separados (build-debian/, build-alma/)

**Práctica**:
- Crear la estructura de archivos y directorios del lab
- Ejecutar `docker compose build` para construir ambas imágenes
- Verificar que el mismo código fuente es accesible desde ambos contenedores
- Compilar un programa C en ambas distros y verificar que funciona en ambas

### T04 — Verificación

**Conceptos a cubrir**:
- Script de validación del entorno (`verify-env.sh`):
  - Verificar que Docker/Podman está instalado y funcionando
  - Verificar que las imágenes se construyeron correctamente
  - Verificar herramientas dentro de cada contenedor:
    ```bash
    docker compose exec debian-dev gcc --version
    docker compose exec debian-dev rustc --version
    docker compose exec debian-dev gdb --version
    docker compose exec debian-dev valgrind --version
    docker compose exec debian-dev make --version
    docker compose exec debian-dev cmake --version
    ```
  - Compilar y ejecutar un programa C de prueba en ambos contenedores
  - Compilar y ejecutar un programa Rust de prueba en ambos contenedores
  - Verificar que gdb funciona (requiere SYS_PTRACE)
  - Verificar que los volúmenes comparten datos correctamente
  - Mostrar resumen con estado de cada verificación (OK / FAIL)
- Automatización: el script debe ser ejecutable desde el host y probar ambos contenedores

**Comportamientos importantes**:
- Si gdb muestra "Operation not permitted", falta SYS_PTRACE en el Compose file
- Si rustup no está en el PATH, verificar que ENV PATH incluye `$HOME/.cargo/bin`
- Si las man pages no funcionan, verificar que se revirtió la exclusión de Docker Debian

**Práctica**:
- Escribir el script de verificación completo
- Ejecutarlo y corregir cualquier problema encontrado
- Documentar los requisitos del sistema host (Docker, espacio en disco, memoria)

---

## Sección 2: Mantenimiento y Limpieza

### T01 — Monitoreo de espacio

**Conceptos a cubrir**:
- `docker system df`: muestra el espacio usado por imágenes, contenedores, volúmenes y caché
  - `-v` (verbose): desglose por imagen/contenedor/volumen individual
  - Campos: TYPE, TOTAL, ACTIVE, SIZE, RECLAIMABLE
  - RECLAIMABLE: espacio que se puede liberar sin afectar contenedores activos
- `du -sh /var/lib/docker/`: espacio total del directorio de Docker en el host
  (requiere root, o rootless: `~/.local/share/docker/`)
- Identificar qué ocupa más espacio:
  - Imágenes no utilizadas (no referenciadas por ningún contenedor)
  - Contenedores detenidos con sus capas de escritura
  - Volúmenes huérfanos (no asociados a ningún contenedor)
  - Build cache (capas intermedias de builds anteriores)
- `docker image ls --filter dangling=true`: imágenes sin tag (resultado de rebuilds)

**Comportamientos importantes**:
- Docker puede consumir decenas de GB sin que el usuario lo note — especialmente
  con builds frecuentes y múltiples imágenes
- Las capas compartidas entre imágenes se cuentan una sola vez en `docker system df`
  pero `docker images` puede mostrar un total mayor (cuenta tamaño virtual por imagen)
- Los build caches de BuildKit se acumulan rápidamente — `docker builder prune`

**Práctica**:
- Ejecutar `docker system df` y `docker system df -v`
- Identificar las imágenes y volúmenes que más espacio consumen
- Comparar el espacio reportado con `du` en el host

### T02 — Limpieza selectiva

**Conceptos a cubrir**:
- **Imágenes**: `docker image prune`
  - Sin flags: solo elimina imágenes "dangling" (sin tag, `<none>:<none>`)
  - `-a`: elimina todas las imágenes no usadas por contenedores activos
  - `--filter "until=24h"`: solo imágenes creadas hace más de 24 horas
- **Contenedores**: `docker container prune`
  - Elimina todos los contenedores en estado "exited" o "dead"
  - Útil después de sesiones de desarrollo con muchos `docker run` sin `--rm`
- **Volúmenes**: `docker volume prune`
  - Elimina volúmenes no referenciados por ningún contenedor (activo o detenido)
  - Peligroso: puede eliminar datos persistentes de contenedores que se eliminaron
    pero cuyos volúmenes se pretendía mantener
  - `-a`: incluye volúmenes anónimos
- **Build cache**: `docker builder prune`
  - Elimina capas de build cacheadas
  - `-a`: elimina todo el cache (no solo lo no referenciado)
  - `--keep-storage 5GB`: mantener al menos 5GB de cache
- **Redes**: `docker network prune`
  - Elimina redes custom no usadas por ningún contenedor

**Comportamientos importantes**:
- `docker volume prune` es la operación de limpieza más peligrosa — siempre verificar
  con `docker volume ls` antes de ejecutar
- Los volúmenes nombrados con datos importantes no deben limpiarse — documentar cuáles
  se usan para qué
- Las operaciones de prune piden confirmación antes de ejecutar (salvo con `-f`)

**Práctica**:
- Crear contenedores y volúmenes de prueba, luego limpiar selectivamente
- Practicar la identificación de recursos purgables antes de ejecutar prune
- Verificar cuánto espacio se libera con cada tipo de limpieza

### T03 — Limpieza agresiva

**Conceptos a cubrir**:
- `docker system prune`: limpieza integral
  - Elimina: contenedores detenidos, redes no usadas, imágenes dangling, build cache
  - `-a`: también elimina imágenes no usadas (no solo dangling)
  - `-f`: sin confirmación
  - `--volumes`: también elimina volúmenes no usados (no incluido por defecto)
- Cuándo es seguro:
  - En máquinas de desarrollo personal sin datos importantes en volúmenes
  - Después de terminar un proyecto y antes de empezar otro
  - En CI/CD para liberar espacio entre builds
- Cuándo NO es seguro:
  - En máquinas con bases de datos en volúmenes Docker
  - Si hay contenedores detenidos con datos en la capa de escritura que se necesitan
  - Si hay imágenes custom que tardan mucho en reconstruirse
- Qué se pierde con `docker system prune -a --volumes`:
  - Todas las imágenes (se re-descargan/reconstruyen al necesitarlas)
  - Todos los volúmenes no usados (datos persistentes)
  - Todo el build cache (el próximo build será desde cero)
  - Todos los contenedores detenidos

**Comportamientos importantes**:
- `docker system prune -a` puede liberar decenas de GB pero el siguiente `docker compose build`
  tardará mucho más (sin cache)
- Los volúmenes con datos que no se quieren perder deben estar listados en un Compose file
  activo o documentados explícitamente

**Práctica**:
- Ejecutar `docker system prune` (sin -a) y ver qué se libera
- Documentar el espacio antes y después
- Discutir escenarios donde `-a` sería apropiado vs peligroso

### T04 — Estrategia del curso

**Conceptos a cubrir**:
- Rutina de limpieza recomendada para este curso:
  1. **Después de cada sesión de trabajo**: `docker container prune` (limpiar contenedores efímeros)
  2. **Semanalmente**: `docker image prune` (limpiar imágenes dangling de rebuilds)
  3. **Mensualmente**: `docker builder prune --keep-storage 2GB` (limpiar build cache viejo)
  4. **Nunca**: `docker volume prune` sin verificar primero qué volúmenes contienen datos
- Reutilización de contenedores:
  - Preferir `docker compose exec` sobre `docker compose run` (reusar contenedor
    existente en vez de crear uno nuevo)
  - Mantener los contenedores del lab corriendo (o al menos creados, no eliminados)
  - No usar `docker compose down -v` salvo que se quiera resetear el lab completo
- Monitoreo con alias:
  ```bash
  alias docker-space='docker system df'
  alias docker-clean='docker container prune -f && docker image prune -f'
  ```
- Documentar el espacio de disco requerido:
  - ~500MB-1GB por imagen de desarrollo (Debian + Alma ≈ 1-2GB)
  - ~200MB por toolchain Rust
  - ~100MB por volúmenes del workspace
  - Total estimado del lab: ~2-3GB

**Comportamientos importantes**:
- Los contenedores del lab deben mantenerse vivos para preservar el estado del workspace
  (paquetes instalados adicionalmente, configuraciones, historial de shell)
- Si se necesita resetear un contenedor, es mejor reconstruir la imagen con el Dockerfile
  actualizado que instalar paquetes manualmente dentro del contenedor

**Práctica**:
- Configurar los alias de limpieza en el shell
- Ejecutar la rutina de limpieza completa y documentar el espacio liberado
- Verificar que el lab sigue funcional después de la limpieza
