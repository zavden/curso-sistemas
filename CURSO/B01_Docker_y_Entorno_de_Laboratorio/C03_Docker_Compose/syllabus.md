# Capítulo 3: Docker Compose [A]

**Bloque**: B01 — Docker y Entorno de Laboratorio
**Tipo**: Aislado
**Objetivo**: Gestionar aplicaciones multi-contenedor con Docker Compose, definiendo
servicios, redes y volúmenes de forma declarativa.

---

## Sección 1: Fundamentos

### T01 — Sintaxis YAML

**Conceptos a cubrir**:
- YAML básico para Compose:
  - Indentación con espacios (nunca tabs), 2 espacios es la convención
  - Listas con `-`, mapas con `key: value`
  - Strings sin comillas (salvo que contengan caracteres especiales)
  - Multi-line con `|` (preserva newlines) y `>` (folds newlines)
- Estructura de un `docker-compose.yml` (Compose v2):
  ```yaml
  services:
    web:
      image: nginx
      ports:
        - "8080:80"
    db:
      image: postgres:15
      environment:
        POSTGRES_PASSWORD: secret
  volumes:
    db-data:
  networks:
    backend:
  ```
- Secciones principales:
  - `services`: define cada contenedor, su imagen/build, config, etc.
  - `volumes`: define named volumes (los que no se definen aquí son efímeros)
  - `networks`: define redes custom (si no se definen, Compose crea una default)
- Versiones del formato:
  - Compose v1 (legacy): requería `version: "3.8"` al inicio
  - Compose v2 (actual): no requiere `version:`, se detecta automáticamente
  - Docker Compose v2 es un plugin de Docker CLI (`docker compose` sin guión)
    vs v1 que era un binario separado (`docker-compose` con guión)
- Opciones de servicio más usadas:
  - `image`: imagen a usar
  - `build`: path al Dockerfile (o configuración de build)
  - `ports`: mapeo de puertos
  - `volumes`: montajes
  - `environment`: variables de entorno
  - `command`: sobrescribe CMD
  - `restart`: política de reinicio

**Comportamientos importantes**:
- Compose crea automáticamente una red bridge para el proyecto (nombre: `<directorio>_default`)
- Todos los servicios del mismo Compose file se ven entre sí por nombre de servicio
- Los nombres de contenedores generados siguen el patrón `<proyecto>-<servicio>-<instancia>`
- El nombre del proyecto se deriva del directorio por defecto (configurable con `-p`)
- YAML es sensible a tipos: `yes`/`no` se interpretan como booleanos — para strings,
  usar comillas: `"yes"`

**Práctica**:
- Crear un Compose file minimal con un servicio web
- Explorar la red default creada por Compose
- Verificar que `docker compose` (v2) funciona

### T02 — Ciclo de vida

**Conceptos a cubrir**:
- `docker compose up`: crea y arranca todos los servicios
  - `-d`: detached (background)
  - `--build`: fuerza rebuild de imágenes antes de arrancar
  - `--force-recreate`: recrea contenedores incluso si la config no cambió
  - `--remove-orphans`: elimina contenedores de servicios que ya no están en el YAML
  - `docker compose up servicio`: arranca solo un servicio (y sus dependencias)
- `docker compose down`: detiene y elimina contenedores, redes
  - `-v`: también elimina named volumes definidos en el Compose file
  - `--rmi all`: también elimina imágenes construidas
  - `down` no elimina volúmenes por defecto (protección de datos)
- `docker compose start/stop`: arranca/detiene servicios sin crear/destruir
- `docker compose restart`: detiene y vuelve a arrancar
- `docker compose ps`: lista servicios del proyecto y su estado
- `docker compose logs`: logs de todos los servicios (coloreados por servicio)
  - `-f`: follow
  - `docker compose logs servicio`: logs de un servicio específico
- `docker compose exec servicio bash`: shell interactiva en un servicio corriendo
- `docker compose run servicio command`: ejecuta un comando one-shot en un servicio
  nuevo (no usa el contenedor corriendo, crea uno nuevo)

**Comportamientos importantes**:
- `up` es idempotente: si los contenedores ya existen y la config no cambió, no hace nada
- Si modificas el Compose file y haces `up`, Compose recrea solo los servicios que cambiaron
- `down` + `up` vs `restart`: `down+up` destruye y recrea (limpio), `restart` solo detiene
  y reinicia (preserva estado)
- `run` crea un contenedor efímero — no se limpia automáticamente salvo con `--rm`

**Práctica**:
- Arrancar un proyecto, verificar estado con `ps`, ver logs, detener, destruir
- Modificar un servicio y hacer `up` — observar que solo se recrea el servicio cambiado
- Comparar `exec` vs `run`

### T03 — Variables de entorno

**Conceptos a cubrir**:
- Tres formas de definir variables de entorno en Compose:
  1. Inline en el YAML:
     ```yaml
     environment:
       - DB_HOST=postgres
       - DB_PORT=5432
     ```
     o en formato map:
     ```yaml
     environment:
       DB_HOST: postgres
       DB_PORT: 5432
     ```
  2. Archivo externo:
     ```yaml
     env_file:
       - .env.db
       - .env.app
     ```
  3. Archivo `.env` en el directorio del Compose file: se carga automáticamente para
     interpolación en el YAML (no se pasa a los contenedores automáticamente)
- Interpolación en el YAML:
  ```yaml
  services:
    web:
      image: myapp:${APP_VERSION:-latest}
  ```
  - `${VAR}`: error si no definida
  - `${VAR:-default}`: default si no definida o vacía
  - `${VAR-default}`: default solo si no definida
  - `${VAR:?error message}`: error con mensaje custom si no definida
- Precedencia (de mayor a menor):
  1. Variables del shell del host
  2. Archivo `.env`
  3. `env_file` en el servicio
  4. `environment` en el servicio
  5. Variables definidas en la imagen (ENV en Dockerfile)

**Comportamientos importantes**:
- El `.env` se usa para interpolación del YAML, no se pasa a los contenedores
  (a menos que se referencie explícitamente con `environment` o `env_file`)
- Los archivos `env_file` se pasan directamente a los contenedores como variables de entorno
- Nunca commitear archivos `.env` con secrets al control de versiones
- YAML interpreta algunos valores como tipos: `DB_PORT: 5432` es un número, no un string.
  Para forzar string: `DB_PORT: "5432"`

**Práctica**:
- Crear un `.env` con variables y usarlas con interpolación en el Compose file
- Demostrar la precedencia: misma variable en `.env`, `env_file`, y `environment`
- Usar `docker compose config` para ver el YAML resuelto con las variables expandidas

---

## Sección 2: Orquestación

### T01 — Dependencias entre servicios

**Conceptos a cubrir**:
- `depends_on` simple: define orden de arranque
  ```yaml
  services:
    web:
      depends_on:
        - db
        - redis
    db:
      image: postgres:15
  ```
  - Compose arranca `db` y `redis` antes que `web`
  - Pero solo espera a que el contenedor arranque, no a que el servicio esté listo
- `depends_on` con condiciones (requiere healthcheck):
  ```yaml
  services:
    web:
      depends_on:
        db:
          condition: service_healthy
    db:
      image: postgres:15
      healthcheck:
        test: ["CMD-SHELL", "pg_isready -U postgres"]
        interval: 5s
        timeout: 5s
        retries: 5
  ```
  - Condiciones: `service_started` (default), `service_healthy`, `service_completed_successfully`
- `service_completed_successfully`: esperar a que un servicio termine con exit 0
  (útil para migraciones de base de datos antes de arrancar la app)

**Comportamientos importantes**:
- Sin healthcheck, `depends_on` solo garantiza que el contenedor se inició, no que
  el servicio esté aceptando conexiones — la app debe manejar reintentos
- `depends_on` afecta al arranque pero no al apagado (el orden de apagado es inverso)
- Compose no reinicia un servicio si su dependencia se cae después del arranque

**Práctica**:
- Crear un setup con web + db donde web depende de db
- Demostrar que sin healthcheck, web puede arrancar antes de que db esté lista
- Añadir healthcheck y condición `service_healthy`, verificar que espera correctamente

### T02 — Redes multi-servicio

**Conceptos a cubrir**:
- Compose crea una red bridge por defecto donde todos los servicios se ven
- Los servicios se comunican usando el nombre del servicio como hostname:
  `ping db` desde el servicio `web` funciona automáticamente
- Redes custom para aislamiento:
  ```yaml
  services:
    web:
      networks:
        - frontend
        - backend
    db:
      networks:
        - backend
    proxy:
      networks:
        - frontend
  networks:
    frontend:
    backend:
  ```
  - `db` no es accesible desde `proxy` (están en redes diferentes)
  - `web` puede hablar con ambos (está en ambas redes)
- Redes externas: conectar a redes creadas fuera de Compose
  ```yaml
  networks:
    external-net:
      external: true
  ```
- Aliases de red: dar nombres alternativos a un servicio en una red específica

**Comportamientos importantes**:
- Si un servicio no especifica `networks:`, se conecta a la red default del proyecto
- Si un servicio especifica `networks:`, NO se conecta a la default (hay que incluirla
  explícitamente si se desea)
- Los nombres DNS son los nombres de servicio, no los nombres de contenedor
- Compose añade el prefijo del proyecto a las redes (ej: `myproject_backend`)

**Práctica**:
- Crear un setup con frontend, backend y base de datos en redes separadas
- Verificar que el frontend no puede acceder directamente a la base de datos
- Usar `docker network inspect` para ver qué contenedores están en cada red

### T03 — Volúmenes compartidos

**Conceptos a cubrir**:
- Named volumes definidos en la sección `volumes:` del Compose file
  ```yaml
  services:
    web:
      volumes:
        - app-data:/var/www/data
    worker:
      volumes:
        - app-data:/var/www/data
  volumes:
    app-data:
  ```
  - Ambos servicios leen y escriben en el mismo volumen
- Bind mounts para desarrollo:
  ```yaml
  services:
    web:
      volumes:
        - ./src:/app/src     # Bind mount: código del host
        - node-modules:/app/node_modules  # Named volume: deps separadas
  volumes:
    node-modules:
  ```
- Volúmenes read-only:
  ```yaml
  volumes:
    - ./config/nginx.conf:/etc/nginx/nginx.conf:ro
  ```
- Opciones de driver: volúmenes con drivers custom (NFS, cloud storage)

**Comportamientos importantes**:
- Múltiples contenedores escribiendo al mismo volumen simultáneamente pueden causar
  corrupción si la aplicación no maneja concurrencia — las bases de datos usan
  volúmenes exclusivos
- `docker compose down -v` elimina los named volumes — cuidado con datos persistentes
- Bind mounts con paths relativos se resuelven desde el directorio del Compose file
- Named volumes persisten entre `down` y `up` (sin `-v`)

**Práctica**:
- Compartir un volumen entre dos servicios y verificar la visibilidad de datos
- Usar un bind mount para desarrollo: editar en el host, ver el cambio en el contenedor
- Demostrar el efecto de `down` vs `down -v` sobre los volúmenes
