# T03 — Variables de entorno

## Tres mecanismos diferentes

Docker Compose tiene tres formas de manejar variables de entorno, cada una con
un propósito y alcance distinto:

```
┌─────────────────────────────────────────────────────────────┐
│                   Compose file (YAML)                       │
│                                                             │
│  1. .env ──────▶ Interpolación en el YAML                   │
│     (archivo)     image: myapp:${VERSION}                   │
│                   Solo para resolver el YAML, no llega       │
│                   automáticamente a los contenedores        │
│                                                             │
│  2. environment: ──▶ Variables dentro del contenedor        │
│     (inline)         Se definen directamente en el YAML      │
│                                                             │
│  3. env_file: ──────▶ Variables dentro del contenedor     │
│     (archivos)        Se cargan desde archivos externos      │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Tabla comparativa

| Mecanismo | Propósito | Llega a contenedores | Cuándo se procesa |
|---|---|---|---|
| `.env` | Interpolación del YAML | **No** (solo YAML) | Al cargar compose |
| `environment:` | Definir variables | **Sí** | Al crear contenedor |
| `env_file:` | Cargar desde archivos | **Sí** | Al crear contenedor |

## 1. `environment:` — Variables inline

Se definen directamente en el Compose file y se pasan al contenedor:

```yaml
services:
  api:
    image: myapi:latest

    # Formato lista (key=value)
    environment:
      - DB_HOST=postgres
      - DB_PORT=5432
      - DB_NAME=myapp

    # Formato mapa (equivalente, más legible)
    environment:
      DB_HOST: postgres
      DB_PORT: "5432"
      DB_NAME: myapp
```

### Pasar variables del host

Si defines una variable sin valor, Compose pasa la variable del host:

```yaml
services:
  api:
    environment:
      - API_KEY    # Sin =valor → toma el valor del host
      - DEBUG      # Toma DEBUG del host
      - HOME       # Toma HOME del host
```

```bash
API_KEY=abc123 docker compose up -d
# El contenedor recibe API_KEY=abc123
```

### Variables booleanas

```yaml
services:
  api:
    environment:
      # YAML interpreta "true" como booleano, usar strings
      DEBUG: "false"      # String "false", no booleano
      VERBOSE: "yes"     # String "yes", no booleano
      DEBUG: "true"      # String "true"
```

## 2. `env_file:` — Variables desde archivos

Carga variables desde archivos externos y las pasa al contenedor:

```yaml
services:
  api:
    env_file:
      - .env.db        # Variables de base de datos
      - .env.app       # Variables de aplicación
      - ./config.env   # Path relativo al contexto
```

Formato del archivo env:

```bash
# .env.db
DB_HOST=postgres
DB_PORT=5432
DB_USER=admin
DB_PASSWORD=secret

# Líneas vacías y comentarios se ignoran
# Las comillas son literales (no se quitan)
DB_NAME="myapp_db"

# Valor vacío (distinto de no definir)
EMPTY_VAR=
```

**Reglas del formato**:
- Una variable por línea: `KEY=VALUE`
- Comentarios con `#` al inicio de línea (el `#` inline no funciona igual)
- Las comillas alrededor del valor se incluyen como parte del valor
  (`KEY="value"` → el valor es `"value"` con comillas)
- Sin expansión de variables (`$VAR` se toma literal)
- Líneas vacías se ignoran

### Múltiples env_files

```yaml
services:
  api:
    env_file:
      - path: .env.common
      - path: .env.prod
        required: true    # Fallar si no existe (default: true)
      - path: .env.local
        required: false   # No fallar si no existe
```

Los archivos se procesan **en orden**, y si una variable aparece en varios,
el último valor gana.

## 3. Archivo `.env` — Interpolación del YAML

Un archivo `.env` en el mismo directorio que el Compose file se carga
automáticamente para **interpolar valores en el YAML**:

```bash
# .env (en el directorio del Compose file)
APP_VERSION=2.0
POSTGRES_TAG=16
EXTERNAL_PORT=8080
```

```yaml
# compose.yml — las variables se resuelven antes de crear los contenedores
services:
  web:
    image: myapp:${APP_VERSION}
    ports:
      - "${EXTERNAL_PORT}:80"
  db:
    image: postgres:${POSTGRES_TAG}
```

**Diferencia crítica**: `.env` se usa para resolver el YAML, pero **no se pasa
automáticamente a los contenedores**:

```bash
# .env
APP_VERSION=2.0

# El contenedor NO tiene APP_VERSION a menos que lo declares explícitamente:
```

```yaml
services:
  web:
    image: myapp:${APP_VERSION}      # ✓ Interpolado en YAML
    environment:
      APP_VERSION: ${APP_VERSION}    # Necesario para que llegue al contenedor
```

### Ubicación del archivo `.env`

Compose busca `.env` en:
1. El directorio del archivo compose (con `-f`)
2. El directorio de trabajo actual

```bash
# compose.yml en /opt/proyecto
cd /opt/proyecto
docker compose up -d   # Busca .env en /opt/proyecto

# O especificado explícitamente
docker compose --env-file /path/to/.env up -d
```

### Compose y .env

```bash
# Especificar archivo .env diferente
docker compose --env-file .env.prod up -d

# Ver qué variables se están usando
docker compose config
```

## Interpolación en el YAML

### Sintaxis completa

| Sintaxis | Comportamiento |
|---|---|
| `${VAR}` | Valor de VAR (error si no definida en Docker Compose v2.20+) |
| `${VAR:-default}` | Valor de VAR, o `default` si no definida o vacía |
| `${VAR-default}` | Valor de VAR, o `default` si no definida (vacía OK) |
| `${VAR:?error msg}` | Valor de VAR, o **error** con mensaje si no definida |
| `${VAR?error msg}` | Valor de VAR, o **error** si no definida (vacía OK) |

### Diferencia entre :- y -

```bash
# Con :- (trata vacío como no definido)
unset VAR           # VAR no existe
${VAR:-default}    # → "default"
VAR=""             # VAR existe pero está vacía
${VAR:-default}    # → "default" (vacío se trata como no definido)

# Con - (solo si no existe)
unset VAR           # VAR no existe
${VAR-default}     # → "default"
VAR=""             # VAR existe y está vacía
${VAR-default}     # → "" (usa el valor vacío, no default)
```

### Ejemplos prácticos

```yaml
services:
  web:
    # Valor por defecto si la variable no existe
    image: myapp:${APP_VERSION:-latest}

    # Error descriptivo si no se define (obligatorio)
    environment:
      DATABASE_URL: ${DATABASE_URL:?DATABASE_URL must be set in .env}

    ports:
      - "${PORT:-8080}:80"

    # Default con valor complejo
    command: ["python", "app.py", "--port", "${PORT:-8080}"]
```

### Escapar `$`

Si necesitas un literal `$` en el YAML, usa `$$`:

```yaml
services:
  web:
    # $$ se convierte en $ en el comando final
    command: sh -c "echo $$HOME"   # → shell recibe $HOME normalmente
    environment:
      DOLLAR: "$$USD"              # → DOLLAR=$USD

    # Para un literal $$, usa $$$$
    command: sh -c "echo $$$$"     # → shell recibe $$
```

### Valores nulos y vacíos

```yaml
services:
  web:
    # Si VAR no existe: "" (vacío)
    env1: ${UNDEFINED_VAR}

    # Si VAR no existe: "default_value"
    env2: ${UNDEFINED_VAR:-default_value}

    # Si VAR existe y es vacía: ""
    # Si VAR no existe: "default"
    env3: ${DEFINED_EMPTY_VAR-default}
```

## Precedencia

Cuando la misma variable se define en múltiples lugares, la prioridad (de mayor a
menor) es:

```
1. Variables del shell del host          ← Mayor prioridad
   docker compose up -d VAR=value
   export VAR=value; docker compose up -d

2. Archivo .env (interpolación YAML)
   (solo para interpolación del YAML, NO pasa a contenedores)

3. env_file en el servicio
   (se procesan en orden, último gana)

4. environment en el servicio
   (sobrescribe env_file para misma variable)

5. ENV en el Dockerfile                  ← Menor prioridad
```

```bash
# .env
DB_HOST=from_dotenv

# .env.db
DB_HOST=from_env_file
```

```yaml
services:
  api:
    env_file:
      - .env.db
    environment:
      DB_HOST: from_environment_block
```

```bash
# Sin variable en el host
docker compose run --rm api sh -c 'echo $DB_HOST'
# from_environment_block ← environment gana sobre env_file

# Con variable en el host
DB_HOST=from_host docker compose run --rm api sh -c 'echo $DB_HOST'
# from_host ← el host gana sobre todo
```

**Nota**: la precedencia entre `environment` y `env_file` es que `environment`
gana. Pero la variable del host siempre tiene la mayor prioridad.

## `docker compose config` — Verificar variables

`config` muestra el Compose file con todas las variables resueltas. Es la forma
más fiable de verificar qué valores se están usando:

```bash
# .env
APP_VERSION=2.0
PORT=9090
```

```bash
docker compose config
# services:
#   web:
#     image: myapp:2.0          ← ${APP_VERSION} resuelto
#     ports:
#       - "9090:80"             ← ${PORT} resuelto
```

Si una variable no está definida:

```bash
docker compose config
# WARN[0000] The "UNDEFINED_VAR" variable is not set. Defaulting to a blank string.
```

## Separación de archivos env por entorno

Un patrón común es tener archivos env por entorno:

```
proyecto/
├── compose.yml
├── .env                  # Variables del YAML (puerto, versiones)
├── .env.development      # Variables de desarrollo
├── .env.production       # Variables de producción (NO commitear)
└── .env.example          # Template (sí commitear)
```

```yaml
# compose.yml
services:
  api:
    env_file:
      - .env.${ENVIRONMENT:-development}
```

```bash
# Desarrollo
docker compose up -d
# Carga .env.development

# Producción
ENVIRONMENT=production docker compose up -d
# Carga .env.production
```

### .env.example para documentación

```bash
# .env.example — Commitear esto, no .env real
# .env — No commitear, crear desde example
APP_VERSION=latest
PORT=8080
DB_HOST=localhost
DB_PORT=5432
```

## Combinación de mecanismos

```yaml
# .env
APP_VERSION=2.0
PORT=8080
```

```yaml
# .env.db
DB_HOST=postgres
DB_PORT=5432
DB_NAME=myapp
DB_PASSWORD=secret123
```

```yaml
# compose.yml
services:
  api:
    image: myapp:${APP_VERSION}        # 1. .env interpolation
    ports:
      - "${PORT}:80"
    env_file:
      - .env.db                        # 2. env_file (para contenedores)
    environment:
      NODE_ENV: production             # 3. environment inline
      DEBUG: "false"
```

Resultado en el contenedor:
```
APP_VERSION=         # No existe (solo interpolation)
PORT=                # No existe (solo interpolation)
DB_HOST=postgres     # Del env_file
DB_PORT=5432         # Del env_file
DB_NAME=myapp        # Del env_file
DB_PASSWORD=secret123 # Del env_file
NODE_ENV=production  # Del environment
DEBUG=false          # Del environment
```

## Seguridad: secrets vs variables de entorno

Las variables de entorno son visibles con `docker inspect` y en `/proc/1/environ`
dentro del contenedor:

```bash
docker compose exec api cat /proc/1/environ | tr '\0' '\n' | grep DB_PASSWORD
# DB_PASSWORD=secret  ← visible para cualquiera con acceso al contenedor
```

Para datos sensibles en producción, usar Docker secrets (Compose con Swarm) o
montar archivos de secrets:

```yaml
services:
  api:
    secrets:
      - db_password
    environment:
      DB_PASSWORD_FILE: /run/secrets/db_password

secrets:
  db_password:
    file: ./secrets/db_password.txt
```

Para el laboratorio de este curso, `environment` y `env_file` son suficientes.
Los secrets se usan en producción real.

---

## Ejercicios

### Ejercicio 1 — Archivo .env e interpolación

```bash
mkdir -p /tmp/compose_env && cd /tmp/compose_env

cat > .env << 'EOF'
APP_VERSION=3.0
WEB_PORT=9090
EOF

cat > compose.yml << 'EOF'
services:
  web:
    image: nginx:${NGINX_TAG:-latest}
    ports:
      - "${WEB_PORT:-8080}:80"
    environment:
      APP_VERSION: ${APP_VERSION}
EOF

# Verificar interpolación
docker compose config

# Levantar
docker compose up -d

# Verificar que APP_VERSION llegó al contenedor
docker compose exec web sh -c 'echo $APP_VERSION'
# 3.0

# Verificar el puerto
curl -s http://localhost:9090 | head -3

docker compose down
cd / && rm -rf /tmp/compose_env
```

### Ejercicio 2 — Precedencia de variables

```bash
mkdir -p /tmp/compose_prec && cd /tmp/compose_prec

cat > .env.file << 'EOF'
MY_VAR=from_env_file
EOF

cat > compose.yml << 'EOF'
services:
  test:
    image: alpine:latest
    env_file:
      - .env.file
    environment:
      MY_VAR: from_environment
    command: sh -c 'echo "MY_VAR=$$MY_VAR"'
EOF

# environment gana sobre env_file
docker compose run --rm test
# MY_VAR=from_environment

# Variable del host gana sobre todo
MY_VAR=from_host docker compose run --rm test
# MY_VAR=from_host

docker compose down
cd / && rm -rf /tmp/compose_prec
```

### Ejercicio 3 — env_file con múltiples archivos

```bash
mkdir -p /tmp/compose_multi_env && cd /tmp/compose_multi_env

cat > .env.db << 'EOF'
DB_HOST=postgres
DB_PORT=5432
DB_NAME=myapp
EOF

cat > .env.app << 'EOF'
LOG_LEVEL=debug
WORKERS=4
EOF

cat > compose.yml << 'EOF'
services:
  api:
    image: alpine:latest
    env_file:
      - .env.db
      - .env.app
    command: ["sh", "-c", "env | sort | grep -E '(DB_|LOG_|WORKERS)'"]
EOF

docker compose run --rm api
# DB_HOST=postgres
# DB_NAME=myapp
# DB_PORT=5432
# LOG_LEVEL=debug
# WORKERS=4

docker compose down
cd / && rm -rf /tmp/compose_multi_env
```

### Ejercicio 4 — Valores por defecto con ${VAR:-default}

```bash
mkdir -p /tmp/compose_defaults && cd /tmp/compose_defaults

cat > compose.yml << 'EOF'
services:
  web:
    image: nginx:${NGINX_TAG:-latest}
    ports:
      - "${PORT:-8080}:80"
    environment:
      ENV_NAME: ${ENV_NAME:-development}
EOF

# Sin variables definidas: usa defaults
docker compose config | grep -E '(image|ports|ENV_NAME)'
# image: nginx:latest
# - 8080:80
# ENV_NAME: development

# Con variables definidas
NGINX_TAG=1.25 PORT=9090 ENV_NAME=staging docker compose config | grep -E '(image|ports|ENV_NAME)'
# image: nginx:1.25
# - 9090:80
# ENV_NAME: staging

cd / && rm -rf /tmp/compose_defaults
```

### Ejercicio 5 — Diferencia entre :- y -

```bash
mkdir -p /tmp/compose_colon && cd /tmp/compose_colon

cat > compose.yml << 'EOF'
services:
  test:
    image: alpine:latest
    environment:
      WITH_COLON: "${EMPTY_VAR:-default_with_colon}"
      WITHOUT_COLON: "${EMPTY_VAR-default_without_colon}"
    command: ["sh", "-c", "echo 'WITH_COLON=$$WITH_COLON' && echo 'WITHOUT_COLON=$$WITHOUT_COLON'"]
EOF

# Cuando la variable NO existe
echo "=== Variable NO existe ==="
docker compose run --rm test

# El resultado muestra ambas con "default"

# Modificar para simular variable vacía
export EMPTY_VAR=""
echo "=== Variable existe pero VACÍA ==="
docker compose run --rm test
# WITH_COLON=default_with_colon (trata vacío como no definido)
# WITHOUT_COLON= (usa el valor vacío)

docker compose down
cd / && rm -rf /tmp/compose_colon
```

### Ejercicio 6 — Archivos env por entorno

```bash
mkdir -p /tmp/compose_env_env && cd /tmp/compose_env_env

cat > .env.development << 'EOF'
ENVIRONMENT=development
LOG_LEVEL=debug
PORT=8080
EOF

cat > .env.production << 'EOF'
ENVIRONMENT=production
LOG_LEVEL=info
PORT=80
EOF

cat > compose.yml << 'EOF'
services:
  web:
    image: nginx:latest
    ports:
      - "${PORT}:80"
    environment:
      ENVIRONMENT: ${ENVIRONMENT}
      LOG_LEVEL: ${LOG_LEVEL}
    env_file:
      - .env.${ENVIRONMENT:-development}
EOF

echo "=== Desarrollo ==="
ENVIRONMENT=development docker compose config

echo "=== Producción ==="
ENVIRONMENT=production docker compose config

docker compose down
cd / && rm -rf /tmp/compose_env_env
```

---

## Resumen visual

```
┌─────────────────────────────────────────────────────────────────┐
│                         .env file                                │
│    APP_VERSION=2.0           ┌─────────────────────────────┐    │
│    PORT=8080                  │ docker compose config       │    │
│                               │ (interpolación del YAML)    │    │
│                               └─────────────────────────────┘    │
│                                        │                         │
│                                        ▼                         │
│                               ┌─────────────────┐               │
│                               │ compose.yml     │               │
│                               │ image: app:${  │               │
│                               │   APP_VERSION} │               │
│                               │ ports: ${PORT} │               │
│                               └─────────────────┘               │
└─────────────────────────────────────────────────────────────────┘
                                        │
                                        ▼
┌─────────────────────────────────────────────────────────────────┐
│                   environment: / env_file:                      │
│                        (pasan al contenedor)                    │
│                                                                  │
│   environment:           ┌──────────────────────────────────┐  │
│     DEBUG: "false"       │ environment del contenedor       │  │
│                          │ DEBUG=false                       │  │
│   env_file:              │ DB_HOST=postgres                  │  │
│     - .env.db            │ APP_VERSION=2.0                   │  │
│                          └──────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```
