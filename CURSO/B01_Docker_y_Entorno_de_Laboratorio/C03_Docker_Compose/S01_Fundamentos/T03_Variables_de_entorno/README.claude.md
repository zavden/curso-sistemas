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
│                   Solo para resolver el YAML, no llega      │
│                   automáticamente a los contenedores        │
│                                                             │
│  2. environment: ──▶ Variables dentro del contenedor        │
│     (inline)         Se definen directamente en el YAML     │
│                                                             │
│  3. env_file: ──────▶ Variables dentro del contenedor       │
│     (archivos)        Se cargan desde archivos externos     │
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

    # Formato mapa (recomendado, más legible)
    environment:
      DB_HOST: postgres
      DB_PORT: "5432"
      DB_NAME: myapp

    # Formato lista (alternativo)
    # environment:
    #   - DB_HOST=postgres
    #   - DB_PORT=5432
    #   - DB_NAME=myapp
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

### Variables con valores especiales en YAML

```yaml
services:
  api:
    environment:
      # Siempre usar comillas para valores que YAML podría interpretar
      DEBUG: "false"      # String "false", no booleano
      VERBOSE: "yes"      # String "yes", no booleano
      PORT: "5432"        # String "5432", no número
```

## 2. `env_file:` — Variables desde archivos

Carga variables desde archivos externos y las pasa al contenedor:

```yaml
services:
  api:
    env_file:
      - .env.db        # Variables de base de datos
      - .env.app       # Variables de aplicación
      - ./config.env   # Path relativo al Compose file
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
# El valor es literalmente "myapp_db" CON comillas

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

```yaml
services:
  web:
    image: myapp:${APP_VERSION}      # ✓ Interpolado en YAML
    environment:
      APP_VERSION: ${APP_VERSION}    # Necesario para que llegue al contenedor
```

Sin la línea de `environment`, el contenedor NO tendrá `APP_VERSION`.

### Ubicación del archivo `.env`

Compose busca `.env` en:
1. El directorio del archivo compose (con `-f`)
2. El directorio de trabajo actual

```bash
# Especificar un archivo .env diferente
docker compose --env-file .env.prod up -d

# Ver qué variables se están usando
docker compose config
```

## Interpolación en el YAML

### Sintaxis completa

| Sintaxis | Comportamiento |
|---|---|
| `${VAR}` | Valor de VAR, o vacío con warning si no definida |
| `${VAR:-default}` | Valor de VAR, o `default` si no definida **o vacía** |
| `${VAR-default}` | Valor de VAR, o `default` si no definida (vacía OK) |
| `${VAR:?error msg}` | Valor de VAR, o **error** con mensaje si no definida **o vacía** |
| `${VAR?error msg}` | Valor de VAR, o **error** si no definida (vacía OK) |

**Nota**: `${VAR}` sin definir NO genera error — da string vacío con un warning.
Para forzar un error, usar `${VAR:?message}`.

### Diferencia entre `:-` y `-`

```
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
```

## Precedencia

Cuando la misma variable se define en múltiples lugares, la prioridad (de mayor a
menor) es:

```
1. Variables del shell del host          ← Mayor prioridad
   VAR=value docker compose up -d
   export VAR=value && docker compose up -d

2. environment: en el servicio
   (definición explícita en el YAML)

3. env_file: en el servicio
   (se procesan en orden, último archivo gana)

4. ENV en el Dockerfile                  ← Menor prioridad
```

El archivo `.env` participa en la **interpolación del YAML** (resuelve
`${VAR}`), no compite directamente con `environment:` o `env_file:` porque
opera en un nivel diferente.

```bash
# .env.file
MY_VAR=from_env_file
```

```yaml
services:
  api:
    env_file:
      - .env.file
    environment:
      MY_VAR: from_environment
```

```bash
# environment gana sobre env_file
docker compose run --rm api sh -c 'echo $MY_VAR'
# from_environment

# Variable del host gana sobre todo
MY_VAR=from_host docker compose run --rm api sh -c 'echo $MY_VAR'
# from_host
```

## `docker compose config` — Verificar variables

`config` muestra el Compose file con todas las variables resueltas. Es la forma
más fiable de verificar qué valores se están usando:

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
# Desarrollo (default)
docker compose up -d
# Carga .env.development

# Producción
ENVIRONMENT=production docker compose up -d
# Carga .env.production
```

**Nota**: `${ENVIRONMENT}` en el path de `env_file:` se interpola desde el shell
o `.env`. Si usas variables como `${PORT}` en `ports:`, deben estar definidas en
`.env` o en el shell — no en los archivos de `env_file:`, porque esos solo pasan
variables al contenedor, no al YAML.

### .env.example para documentación

```bash
# .env.example — Commitear esto, no .env real
APP_VERSION=latest
PORT=8080
DB_HOST=localhost
DB_PORT=5432
```

## Combinación de mecanismos

```bash
# .env (interpolación del YAML)
APP_VERSION=2.0
PORT=8080
```

```bash
# .env.db (env_file, pasa al contenedor)
DB_HOST=postgres
DB_PORT=5432
DB_NAME=myapp
DB_PASSWORD=secret123
```

```yaml
# compose.yml
services:
  api:
    image: myapp:${APP_VERSION}        # 1. .env → interpolación YAML
    ports:
      - "${PORT}:80"                   # 1. .env → interpolación YAML
    env_file:
      - .env.db                        # 2. env_file → contenedor
    environment:
      NODE_ENV: production             # 3. environment → contenedor
      DEBUG: "false"
      APP_VERSION: ${APP_VERSION}      # Pasar .env al contenedor explícitamente
```

Resultado en el contenedor:

```
DB_HOST=postgres      # Del env_file
DB_PORT=5432          # Del env_file
DB_NAME=myapp         # Del env_file
DB_PASSWORD=secret123 # Del env_file
NODE_ENV=production   # Del environment
DEBUG=false           # Del environment
APP_VERSION=2.0       # Del environment (interpolado desde .env)
```

`APP_VERSION` y `PORT` están en `.env` para interpolar el YAML, pero solo
llegan al contenedor si se declaran explícitamente en `environment:`.

## Seguridad: secrets vs variables de entorno

Las variables de entorno son visibles con `docker inspect` y en `/proc/1/environ`
dentro del contenedor:

```bash
docker compose exec api cat /proc/1/environ | tr '\0' '\n' | grep DB_PASSWORD
# DB_PASSWORD=secret  ← visible para cualquiera con acceso al contenedor
```

Para datos sensibles en producción, usar Docker secrets:

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

**Nota**: el directorio `labs/` contiene un laboratorio con 4 partes y
archivos Compose listos para usar. Consultar `LABS.md` para la guía.

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
```

**Predicción**: `config` muestra `image: nginx:latest` (NGINX_TAG no definido,
usa default), `ports: 9090:80` (WEB_PORT=9090 del .env), y
`APP_VERSION: 3.0` en environment.

```bash
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
```

**Predicción**: muestra `MY_VAR=from_environment` porque `environment:` tiene
mayor prioridad que `env_file:`.

```bash
# Variable del host gana sobre todo
MY_VAR=from_host docker compose run --rm test
# MY_VAR=from_host
```

**Predicción**: muestra `MY_VAR=from_host` porque el shell del host tiene la
mayor prioridad de todas.

```bash
docker compose down
cd / && rm -rf /tmp/compose_prec
```

### Ejercicio 3 — env_file con múltiples archivos

```bash
mkdir -p /tmp/compose_multi_env && cd /tmp/compose_multi_env

cat > .env.db << 'EOF'
DB_HOST=postgres
DB_PORT=5432
SHARED=from_db
EOF

cat > .env.app << 'EOF'
LOG_LEVEL=debug
WORKERS=4
SHARED=from_app
EOF

cat > compose.yml << 'EOF'
services:
  api:
    image: alpine:latest
    env_file:
      - .env.db
      - .env.app
    command: ["sh", "-c", "env | sort | grep -E '(DB_|LOG_|WORKERS|SHARED)'"]
EOF

docker compose run --rm api
```

**Predicción**: `SHARED=from_app` porque `.env.app` se procesa después de
`.env.db` — el último archivo gana para variables duplicadas. Las demás
variables aparecen con sus valores sin conflicto.

```bash
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
echo "=== Sin variables ==="
docker compose config | grep -E '(image:|published:|ENV_NAME)'

# Con variables definidas
echo "=== Con variables ==="
NGINX_TAG=1.25 PORT=9090 ENV_NAME=staging docker compose config | grep -E '(image:|published:|ENV_NAME)'
```

**Predicción**: sin variables, usa los defaults (`latest`, `8080`,
`development`). Con variables del host, los defaults se ignoran y se usan
los valores proporcionados.

```bash
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
      WITH_COLON: "${TEST_VAR:-default_colon}"
      WITHOUT_COLON: "${TEST_VAR-default_no_colon}"
    command: ["sh", "-c", "echo 'WITH_COLON=$$WITH_COLON' && echo 'WITHOUT_COLON=$$WITHOUT_COLON'"]
EOF

# Cuando la variable NO existe
echo "=== Variable NO existe ==="
unset TEST_VAR
docker compose run --rm test
```

**Predicción**: ambas muestran "default" porque la variable no existe:
- `WITH_COLON=default_colon`
- `WITHOUT_COLON=default_no_colon`

```bash
# Cuando la variable existe pero está VACÍA
echo "=== Variable VACÍA ==="
TEST_VAR="" docker compose run --rm test
```

**Predicción**: aquí está la diferencia:
- `WITH_COLON=default_colon` — `:-` trata vacío como no definido → usa default
- `WITHOUT_COLON=` — `-` acepta el valor vacío como válido → no usa default

```bash
docker compose down
cd / && rm -rf /tmp/compose_colon
```

### Ejercicio 6 — .env NO pasa automáticamente al contenedor

```bash
mkdir -p /tmp/compose_dotenv && cd /tmp/compose_dotenv

cat > .env << 'EOF'
SECRET_TAG=alpine
MY_SECRET=s3cret123
EOF

cat > compose.yml << 'EOF'
services:
  test:
    image: ${SECRET_TAG:-latest}
    command: ["sh", "-c", "echo 'MY_SECRET=$$MY_SECRET' && echo 'SECRET_TAG=$$SECRET_TAG'"]
EOF

docker compose config | grep image
# image: alpine  ← .env interpoló el YAML correctamente

docker compose run --rm test
```

**Predicción**: ambas variables están vacías en el contenedor:
- `MY_SECRET=` — .env solo interpola YAML, no pasa al contenedor
- `SECRET_TAG=` — igual, no llega al contenedor

Este es el error más común: asumir que `.env` pasa variables al contenedor.

```bash
# Solución: declararlas explícitamente en environment
cat > compose.yml << 'EOF'
services:
  test:
    image: ${SECRET_TAG:-latest}
    environment:
      MY_SECRET: ${MY_SECRET}
      SECRET_TAG: ${SECRET_TAG}
    command: ["sh", "-c", "echo 'MY_SECRET=$$MY_SECRET' && echo 'SECRET_TAG=$$SECRET_TAG'"]
EOF

docker compose run --rm test
# MY_SECRET=s3cret123
# SECRET_TAG=alpine

docker compose down
cd / && rm -rf /tmp/compose_dotenv
```

### Ejercicio 7 — Variables obligatorias con ${VAR:?}

```bash
mkdir -p /tmp/compose_required && cd /tmp/compose_required

cat > compose.yml << 'EOF'
services:
  db:
    image: postgres:16-alpine
    environment:
      POSTGRES_PASSWORD: ${DB_PASSWORD:?DB_PASSWORD must be set - create a .env file}
EOF

# Sin la variable: error con mensaje descriptivo
docker compose config 2>&1 || echo "Error esperado"

# Con la variable: funciona
DB_PASSWORD=secret docker compose config | grep POSTGRES_PASSWORD
# POSTGRES_PASSWORD: secret
```

**Predicción**: sin `DB_PASSWORD`, Compose muestra el error
"DB_PASSWORD must be set" y se detiene. Es la forma correcta de forzar
que variables obligatorias estén definidas antes de ejecutar.

```bash
cd / && rm -rf /tmp/compose_required
```

### Ejercicio 8 — Archivos env por entorno

```bash
mkdir -p /tmp/compose_perenv && cd /tmp/compose_perenv

cat > .env << 'EOF'
ENVIRONMENT=development
EOF

cat > .env.development << 'EOF'
LOG_LEVEL=debug
WORKERS=1
EOF

cat > .env.production << 'EOF'
LOG_LEVEL=warn
WORKERS=4
EOF

cat > compose.yml << 'EOF'
services:
  app:
    image: alpine:latest
    env_file:
      - .env.${ENVIRONMENT:-development}
    environment:
      ENVIRONMENT: ${ENVIRONMENT:-development}
    command: ["sh", "-c", "env | grep -E '(ENVIRONMENT|LOG_LEVEL|WORKERS)' | sort"]
EOF

echo "=== Development ==="
docker compose run --rm app

echo "=== Production ==="
ENVIRONMENT=production docker compose run --rm app
```

**Predicción**: en development, `LOG_LEVEL=debug` y `WORKERS=1`. En production,
`LOG_LEVEL=warn` y `WORKERS=4`. La variable `ENVIRONMENT` del host cambia qué
archivo `.env.X` se carga como `env_file`.

```bash
docker compose down
cd / && rm -rf /tmp/compose_perenv
```

---

## Resumen visual

```
┌─────────────────────────────────────────────────────────────────┐
│                         .env file                               │
│    APP_VERSION=2.0                                              │
│    PORT=8080                                                    │
│         │                                                       │
│         │  Interpolación del YAML                               │
│         ▼                                                       │
│    ┌─────────────────────────────────────────┐                  │
│    │ compose.yml                             │                  │
│    │ image: myapp:${APP_VERSION} → myapp:2.0 │                  │
│    │ ports: ${PORT}:80 → 8080:80             │                  │
│    └─────────────────────────────────────────┘                  │
│         │                                                       │
│         ▼                                                       │
│    environment: / env_file:                                     │
│    (pasan al contenedor)                                        │
│         │                                                       │
│         ▼                                                       │
│    ┌──────────────────────────────────────┐                     │
│    │ Environment del contenedor          │                     │
│    │ DEBUG=false       (de environment:)  │                     │
│    │ DB_HOST=postgres  (de env_file:)     │                     │
│    │ APP_VERSION=2.0   (de environment:   │                     │
│    │                    con interpolación) │                     │
│    └──────────────────────────────────────┘                     │
└─────────────────────────────────────────────────────────────────┘
```

## Checklist

- [ ] Variables que YAML puede malinterpretar entre comillas (`"true"`, `"5432"`)
- [ ] `.env` solo para interpolación YAML — no asumirlo en contenedores
- [ ] Variables para contenedores en `environment:` o `env_file:`
- [ ] `docker compose config` para verificar interpolación
- [ ] Archivos `.env` reales en `.gitignore`, `.env.example` commiteado
- [ ] Variables obligatorias con `${VAR:?message}`
