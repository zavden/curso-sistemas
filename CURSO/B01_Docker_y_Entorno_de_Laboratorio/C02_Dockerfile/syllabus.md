# Capítulo 2: Dockerfile [A]

**Bloque**: B01 — Docker y Entorno de Laboratorio
**Tipo**: Aislado
**Objetivo**: Dominar la construcción de imágenes Docker con Dockerfiles eficientes,
seguros y optimizados.

---

## Sección 1: Sintaxis y Directivas

### T01 — Instrucciones básicas

**Conceptos a cubrir**:
- **FROM**: punto de partida obligatorio (excepto en scratch)
  - `FROM debian:bookworm` — imagen base con tag
  - `FROM debian:bookworm AS builder` — naming para multi-stage
  - `FROM scratch` — imagen vacía, para binarios estáticos (Go, Rust release)
- **RUN**: ejecuta comandos durante el build, cada RUN crea una nueva capa
  - Forma shell: `RUN apt-get update && apt-get install -y gcc`
  - Forma exec: `RUN ["apt-get", "install", "-y", "gcc"]` — sin shell, sin expansión de variables
  - Buena práctica: combinar comandos relacionados en un solo RUN con `&&` para minimizar capas
    y limpiar caches en la misma capa (`rm -rf /var/lib/apt/lists/*`)
- **COPY**: copia archivos del contexto de build al filesystem de la imagen
  - `COPY src/ /app/src/` — rutas relativas al contexto de build
  - `COPY --chown=user:group file /path/` — establecer propietario (evita RUN chown extra)
  - `COPY --from=builder /app/binary /app/` — copiar desde otra stage (multi-stage)
- **ADD**: como COPY pero con features extra:
  - Descomprime automáticamente archivos tar (tar.gz, tar.bz2, tar.xz)
  - Puede descargar de URLs (pero preferir `RUN curl` para control de caché y errores)
  - Regla general: usar COPY salvo que necesites descompresión automática
- **WORKDIR**: establece el directorio de trabajo para RUN, CMD, COPY, ADD, ENTRYPOINT
  - `WORKDIR /app` — si no existe, lo crea
  - Acumula: `WORKDIR /app` + `WORKDIR src` = `/app/src`
  - Preferir WORKDIR sobre `RUN cd /path && ...`
- **CMD**: comando por defecto al iniciar el contenedor (sobrescribible por el usuario)
  - Forma exec (preferida): `CMD ["python3", "app.py"]`
  - Forma shell: `CMD python3 app.py` — se ejecuta como `/bin/sh -c "python3 app.py"`
  - Solo puede haber un CMD — el último gana
- **ENTRYPOINT**: punto de entrada fijo del contenedor
  - Los argumentos de `docker run` se **añaden** al ENTRYPOINT (no lo reemplazan)
  - Forma exec (preferida): `ENTRYPOINT ["python3"]` + `docker run image app.py` ejecuta `python3 app.py`

**Instrucciones adicionales**:
- **EXPOSE**: documenta puertos que el contenedor escucha (no abre nada)
- **LABEL**: metadata key=value para la imagen
- **VOLUME**: crea un mount point y marca como volumen externo
- **SHELL**: cambia el shell por defecto para instrucciones en forma shell
- **STOPSIGNAL**: señal que `docker stop` envía a PID 1 (default: SIGTERM)

**Comportamientos importantes**:
- Cada instrucción (FROM, RUN, COPY, etc.) crea una capa — minimizar el número de capas
  combinando instrucciones cuando sea lógico
- Las instrucciones se ejecutan como root salvo que se haya especificado USER previamente
- `COPY . .` copia todo el contexto de build — usar .dockerignore para excluir archivos

**Práctica**:
- Construir una imagen que compile un programa C simple: FROM, RUN (instalar gcc),
  COPY (código), RUN (compilar), CMD (ejecutar)
- Demostrar la diferencia entre forma shell y exec de RUN/CMD
- Mostrar el efecto de WORKDIR en instrucciones posteriores

### T02 — CMD vs ENTRYPOINT

**Conceptos a cubrir**:
- CMD define el comando por defecto — se reemplaza completamente si el usuario
  pasa argumentos a `docker run`
- ENTRYPOINT define el ejecutable fijo — los argumentos de `docker run` se añaden
- Combinación CMD + ENTRYPOINT:
  ```dockerfile
  ENTRYPOINT ["python3"]
  CMD ["--help"]
  ```
  - `docker run image` → `python3 --help` (CMD como argumentos por defecto)
  - `docker run image app.py` → `python3 app.py` (CMD reemplazado por args del usuario)
- Forma exec vs shell:
  - Exec (`["cmd", "arg"]`): PID 1 es el proceso directamente, recibe señales correctamente
  - Shell (`cmd arg`): PID 1 es `/bin/sh -c "cmd arg"` — sh no propaga señales,
    `docker stop` puede no funcionar limpiamente
  - Regla: siempre usar forma exec para ENTRYPOINT y CMD en producción
- `--entrypoint` en `docker run`: sobrescribe el ENTRYPOINT de la imagen
  (útil para debug: `docker run --entrypoint bash image`)
- Patrón "wrapper script": ENTRYPOINT apunta a un script que hace setup
  (leer secrets, configurar permisos) y luego `exec "$@"` para ejecutar CMD

**Comportamientos importantes**:
- Si ENTRYPOINT usa forma shell, CMD se ignora completamente
- Si solo defines CMD sin ENTRYPOINT, el comportamiento depende de si la imagen
  base tiene un ENTRYPOINT definido
- `docker inspect` muestra ambos por separado — verificar siempre

**Práctica**:
- Crear una imagen con solo CMD y demostrar que se reemplaza con args de docker run
- Crear una imagen con ENTRYPOINT + CMD como default args
- Demostrar un wrapper script con `exec "$@"`
- Probar la forma shell de ENTRYPOINT y observar problemas con señales

### T03 — ARG vs ENV

**Conceptos a cubrir**:
- **ARG**: variable disponible solo durante el build
  - `ARG DEBIAN_FRONTEND=noninteractive`
  - Se pasa con `docker build --build-arg KEY=value`
  - No persiste en la imagen final (no visible en contenedores)
  - Scope: desde la declaración ARG hasta el final del stage
- **ENV**: variable de entorno persistente en la imagen
  - `ENV APP_VERSION=1.0`
  - Disponible durante el build (en instrucciones RUN) y en runtime (contenedores)
  - Se puede sobrescribir con `docker run -e KEY=value`
- ARG antes de FROM: solo disponible para la instrucción FROM
  ```dockerfile
  ARG DEBIAN_VERSION=bookworm
  FROM debian:${DEBIAN_VERSION}
  # ARG no está disponible aquí — hay que redeclararlo
  ARG DEBIAN_VERSION
  ```
- Precedencia: `docker run -e` > ENV en Dockerfile > ARG defaults
- ARG + ENV combo para hacer que un build-arg persista:
  ```dockerfile
  ARG APP_VERSION=1.0
  ENV APP_VERSION=${APP_VERSION}
  ```

**Comportamientos importantes**:
- ARGs quedan registrados en el historial de la imagen (`docker history`) — no usar
  para secrets (contraseñas, tokens)
- `docker build --build-arg` para un ARG no declarado genera un warning (no un error)
- ENV crea una capa (como RUN, COPY) — cada instrucción ENV incrementa el número de capas
- ARGs predefinidos: HTTP_PROXY, HTTPS_PROXY, FTP_PROXY, NO_PROXY (no requieren declaración ARG)

**Práctica**:
- Crear una imagen parametrizable con ARG (versión de la base, paquetes a instalar)
- Demostrar que un ARG no está disponible en runtime (`docker exec env`)
- Usar el combo ARG+ENV para exponer un valor de build en runtime

### T04 — HEALTHCHECK

**Conceptos a cubrir**:
- HEALTHCHECK permite que Docker monitoree si el contenedor está funcionando correctamente
  ```dockerfile
  HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1
  ```
- Parámetros:
  - `--interval`: cada cuánto se ejecuta el check (default: 30s)
  - `--timeout`: cuánto esperar antes de considerar el check como fallido (default: 30s)
  - `--start-period`: tiempo de gracia para que la app arranque (default: 0s)
  - `--retries`: cuántos fallos consecutivos antes de marcar "unhealthy" (default: 3)
- Estados de salud: starting → healthy → unhealthy
- `docker ps` muestra el health status en la columna STATUS
- `docker inspect --format='{{.State.Health}}'` — detalles del último check
- `HEALTHCHECK NONE`: deshabilita el healthcheck heredado de la imagen base
- Los healthchecks se ejecutan dentro del contenedor — necesitan que las herramientas
  estén instaladas (curl, wget, o un binary custom)

**Comportamientos importantes**:
- Docker no reinicia automáticamente un contenedor unhealthy — necesitas una
  restart policy (`--restart on-failure`) o un orquestador (Compose, Swarm)
- Docker Compose usa healthchecks para `depends_on` con condición `service_healthy`
- El healthcheck corre como el mismo usuario que el contenedor — si el proceso corre
  como non-root, el healthcheck también
- Incluir herramientas de diagnóstico (curl, wget) solo para el healthcheck incrementa
  el tamaño de la imagen — alternativa: usar un binary estático pequeño o `/dev/tcp`

**Práctica**:
- Crear una imagen con un servidor web simple y un HEALTHCHECK
- Observar la transición de starting → healthy
- Simular un fallo y observar la transición a unhealthy

---

## Sección 2: Optimización

### T01 — Orden de instrucciones y caché

**Conceptos a cubrir**:
- Docker cachea cada capa; si una instrucción y su input no cambian, reutiliza la capa
- Invalidación de caché: si una capa cambia, todas las capas posteriores se reconstruyen
- Orden óptimo (de menos frecuente a más frecuente en cambios):
  1. FROM (casi nunca cambia)
  2. RUN apt-get install (cambia solo al añadir dependencias)
  3. COPY requirements.txt + RUN pip install (cambia al añadir dependencias)
  4. COPY . . (cambia cada vez que se modifica el código)
  5. CMD (casi nunca cambia)
- Patrón de dependencias primero:
  ```dockerfile
  COPY package.json .
  RUN npm install
  COPY . .            # Solo esto se invalida al cambiar código
  ```
  vs ineficiente:
  ```dockerfile
  COPY . .            # Cualquier cambio invalida TODO lo siguiente
  RUN npm install     # Se reinstalan las deps cada vez
  ```
- `--no-cache`: forzar rebuild completo sin caché
- `DOCKER_BUILDKIT=1`: BuildKit mejora el caching con mount caches y cache exports

**Comportamientos importantes**:
- COPY invalida la caché si cualquier archivo copiado cambia (por checksum, no por timestamp)
- RUN invalida la caché si el string del comando cambia — `apt-get update` sin
  cambiar el comando usa la caché vieja (¡listas de paquetes desactualizadas!)
- Combinar `apt-get update && apt-get install` en un solo RUN evita este problema
- El caché es local por defecto — CI/CD necesita cache export/import o un registry cache

**Práctica**:
- Construir una imagen, modificar solo el código fuente, reconstruir y observar
  qué capas se reutilizan
- Reordenar instrucciones y comparar tiempos de rebuild
- Demostrar la invalidación en cascada

### T02 — Multi-stage builds

**Conceptos a cubrir**:
- Problema: las herramientas de build (compiladores, headers, tests) ocupan mucho espacio
  pero no se necesitan en la imagen final
- Solución: múltiples stages, copiar solo los artefactos necesarios
  ```dockerfile
  # Stage 1: build
  FROM gcc:12 AS builder
  COPY . /src
  WORKDIR /src
  RUN make

  # Stage 2: runtime
  FROM debian:bookworm-slim
  COPY --from=builder /src/myapp /usr/local/bin/
  CMD ["myapp"]
  ```
- Ventajas:
  - Imagen final mucho más pequeña (no incluye gcc, make, headers, código fuente)
  - Menor superficie de ataque (menos binarios instalados)
  - Build reproducible: las dependencias de build están en el Dockerfile
- `--from=stage_name`: referenciar un stage por nombre (AS)
- `--from=0`, `--from=1`: referenciar por índice (frágil, preferir nombres)
- `--target=builder`: construir solo hasta un stage específico (útil para desarrollo)
- Múltiples stages para diferentes targets:
  ```dockerfile
  FROM builder AS test-runner
  RUN make test

  FROM builder AS production
  COPY --from=builder /app/binary /app/
  ```

**Comportamientos importantes**:
- Los stages intermedios no aparecen en `docker images` (son descartados)
- BuildKit puede paralelizar stages independientes
- `--target` permite construir imágenes de desarrollo con herramientas de debug
  y producción sin ellas, desde el mismo Dockerfile
- El tamaño final solo incluye las capas del último stage

**Práctica**:
- Construir una imagen single-stage con gcc y código → ver tamaño
- Refactorizar a multi-stage → comparar tamaño
- Usar `--target` para construir solo el stage de build (para desarrollo)

### T03 — .dockerignore

**Conceptos a cubrir**:
- Contexto de build: al ejecutar `docker build .`, Docker envía todo el directorio
  al daemon como un tar — incluyendo archivos innecesarios
- `.dockerignore`: lista de patrones de archivos a excluir del contexto
  ```
  .git
  node_modules
  *.o
  build/
  Dockerfile
  .dockerignore
  docker-compose.yml
  **/*.log
  !important.log
  ```
- Sintaxis similar a .gitignore:
  - `*` wildcard, `**` recursivo, `!` excepción (incluir a pesar de exclusión previa)
  - Líneas que empiezan con `#` son comentarios
- Impacto sin .dockerignore:
  - Contexto de build más grande → transferencia más lenta al daemon
  - Archivos sensibles (.env, secrets) podrían terminar en la imagen
  - COPY . . incluiría todo (node_modules, .git, binarios de build)
  - El historial de git (.git/) puede ocupar más que el código fuente

**Comportamientos importantes**:
- `.dockerignore` se evalúa antes del COPY — los archivos excluidos ni siquiera
  se envían al daemon
- Si .git está en el contexto y haces COPY . ., el historial completo de git
  queda en la imagen (potencialmente con secrets en commits antiguos)
- Los patrones de .dockerignore se aplican relativos al root del contexto de build

**Práctica**:
- Construir sin .dockerignore y medir el tamaño del contexto
- Crear un .dockerignore apropiado y comparar
- Demostrar que un COPY . . sin .dockerignore incluye archivos no deseados
