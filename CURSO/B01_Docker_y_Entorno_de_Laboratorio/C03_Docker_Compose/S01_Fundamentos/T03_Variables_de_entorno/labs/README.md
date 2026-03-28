# Lab — Variables de entorno

## Objetivo

Laboratorio práctico para dominar las tres formas de manejar variables de
entorno en Docker Compose: `environment:` inline en el YAML, `env_file:`
para cargar desde archivo, interpolación con `.env` para el propio Compose
file, y la precedencia entre todas las fuentes.

## Prerequisitos

- Docker y Docker Compose v2 instalados (`docker compose version`)
- Imágenes base descargadas: `docker pull nginx:alpine && docker pull alpine:latest`
- Estar en el directorio `labs/` de este tópico

## Archivos del laboratorio

| Archivo | Propósito |
|---|---|
| `compose.env-inline.yml` | Variables con `environment:` inline |
| `compose.env-file.yml` | Variables con `env_file:` |
| `compose.env-interp.yml` | Interpolación `${VAR}` desde `.env` |
| `compose.precedence.yml` | Combinación de `environment:` y `env_file:` |
| `app.env` | Archivo de variables para `env_file:` |

---

## Parte 1 — environment: inline

### Objetivo

Definir variables de entorno directamente en el Compose file usando
`environment:` y verificar que están disponibles dentro del contenedor.

### Paso 1.1: Examinar el archivo

```bash
cat compose.env-inline.yml
```

Cuatro variables definidas inline: `APP_NAME`, `APP_ENV`, `DB_HOST`,
`DB_PORT`. Los valores están escritos directamente en el YAML.

### Paso 1.2: Verificar con config

```bash
docker compose -f compose.env-inline.yml config
```

`config` muestra las variables expandidas. Verificar que las 4
variables aparecen bajo `environment:`.

### Paso 1.3: Ejecutar y ver variables

```bash
docker compose -f compose.env-inline.yml run --rm app
```

Salida esperada (entre otras variables del sistema):

```
APP_NAME=my-application
APP_ENV=production
DB_HOST=localhost
DB_PORT=5432
```

El comando del servicio es `env`, que imprime todas las variables de
entorno. Las 4 definidas en `environment:` están presentes.

### Paso 1.4: Filtrar variables específicas

```bash
docker compose -f compose.env-inline.yml run --rm app sh -c 'env | grep -E "^(APP_|DB_)"'
```

Muestra solo las variables que empiezan con `APP_` o `DB_`.

---

## Parte 2 — env_file:

### Objetivo

Cargar variables de entorno desde un archivo externo usando `env_file:`,
separando la configuración del Compose file.

### Paso 2.1: Examinar los archivos

```bash
cat app.env
```

Salida:

```
APP_NAME=from-env-file
DB_HOST=db.example.com
DB_PORT=5432
DB_USER=admin
```

Formato `CLAVE=valor`, una variable por línea. Sin comillas, sin
`export`, sin espacios alrededor del `=`.

```bash
cat compose.env-file.yml
```

El servicio usa `env_file: - app.env` en lugar de `environment:`.

### Paso 2.2: Ejecutar y verificar

```bash
docker compose -f compose.env-file.yml run --rm app sh -c 'env | grep -E "^(APP_|DB_)"'
```

Salida esperada:

```
APP_NAME=from-env-file
DB_HOST=db.example.com
DB_PORT=5432
DB_USER=admin
```

Las variables vienen del archivo `app.env`. El Compose file no tiene
ningún valor hardcodeado — solo apunta al archivo.

### Paso 2.3: Ventaja de env_file

`env_file:` permite:
- Separar configuración sensible del Compose file
- Compartir el mismo archivo entre servicios
- Usar archivos diferentes por entorno (`dev.env`, `prod.env`)
- Agregar `*.env` al `.gitignore` para no commitear secrets

---

## Parte 3 — Interpolación con .env

### Objetivo

Demostrar que Compose sustituye `${VAR}` en el Compose file usando
valores de un archivo `.env` en el mismo directorio. Esto es diferente
de `env_file:` — la interpolación afecta al YAML, no al contenedor
directamente.

### Paso 3.1: Examinar el Compose file

```bash
cat compose.env-interp.yml
```

Salida:

```yaml
services:
  web:
    image: nginx:${IMAGE_TAG:-alpine}
    ports:
      - "${HOST_PORT:-8080}:80"
    environment:
      APP_ENV: ${APP_ENV:-development}
```

Tres variables con interpolación: `${IMAGE_TAG}`, `${HOST_PORT}`,
`${APP_ENV}`. Cada una tiene un valor por defecto con `:-`.

### Paso 3.2: Sin .env — valores por defecto

```bash
docker compose -f compose.env-interp.yml config
```

Salida esperada:

```yaml
services:
  web:
    image: nginx:alpine
    ports:
      - "8080:80"
    environment:
      APP_ENV: development
```

Sin archivo `.env`, Compose usa los valores por defecto definidos con
`:-`.

### Paso 3.3: Crear .env

```bash
cat > .env << 'EOF'
IMAGE_TAG=latest
HOST_PORT=9090
APP_ENV=staging
EOF
```

### Paso 3.4: Config con .env

```bash
docker compose -f compose.env-interp.yml config
```

Salida esperada:

```yaml
services:
  web:
    image: nginx:latest
    ports:
      - "9090:80"
    environment:
      APP_ENV: staging
```

Compose sustituyó `${IMAGE_TAG}` por `latest`, `${HOST_PORT}` por
`9090`, y `${APP_ENV}` por `staging`. Los valores vienen del `.env`.

### Paso 3.5: Levantar con interpolación

```bash
docker compose -f compose.env-interp.yml up -d
docker compose -f compose.env-interp.yml ps
```

Verificar que el puerto es 9090:

```bash
curl -s http://localhost:9090 | head -3
```

### Paso 3.6: Variable de host sobrescribe .env

```bash
docker compose -f compose.env-interp.yml down
HOST_PORT=7070 docker compose -f compose.env-interp.yml config 2>&1 | grep -A1 ports
```

Salida esperada:

```
    ports:
      - "7070:80"
```

La variable de shell (`HOST_PORT=7070`) sobrescribe el valor del
`.env` (`HOST_PORT=9090`). Las variables del host siempre tienen
la mayor prioridad.

### Paso 3.7: Limpiar

```bash
docker compose -f compose.env-interp.yml down 2>/dev/null
rm -f .env
```

---

## Parte 4 — Precedencia

### Objetivo

Demostrar el orden de prioridad cuando la misma variable se define en
múltiples fuentes: `environment:` sobrescribe `env_file:`.

### Paso 4.1: Examinar los archivos

```bash
cat app.env
```

`app.env` define `APP_NAME=from-env-file`, `DB_HOST`, `DB_PORT`,
`DB_USER`.

```bash
cat compose.precedence.yml
```

El servicio tiene **ambos**: `environment:` con `APP_NAME: from-environment`
y `env_file:` apuntando a `app.env` (que tiene `APP_NAME=from-env-file`).

Antes de ejecutar, responde mentalmente:

- ¿Qué valor tendrá `APP_NAME`? ¿`from-environment` o `from-env-file`?
- ¿De dónde vendrán `DB_HOST` y `DB_PORT`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.2: Ejecutar

```bash
docker compose -f compose.precedence.yml run --rm app
```

Salida esperada:

```
APP_NAME=from-environment
DB_HOST=db.example.com
DB_PORT=5432
```

`APP_NAME` viene de `environment:` (no de `env_file:`). `DB_HOST` y
`DB_PORT` vienen de `env_file:` porque no están definidos en
`environment:`.

**Regla**: `environment:` sobrescribe `env_file:` para la misma variable.

### Paso 4.3: docker compose config muestra el resultado final

```bash
docker compose -f compose.precedence.yml config
```

`config` muestra la configuración final después de resolver todas las
fuentes. Es la herramienta para verificar qué valor ganó cuando hay
múltiples fuentes.

### Paso 4.4: Variable de host sobrescribe todo

```bash
APP_NAME=from-host docker compose -f compose.precedence.yml run --rm app
```

Salida esperada:

```
APP_NAME=from-host
DB_HOST=db.example.com
DB_PORT=5432
```

La variable de shell (`APP_NAME=from-host`) sobrescribe tanto
`environment:` como `env_file:`.

Resumen de precedencia (mayor a menor):

```
1. Variable del host (shell)
2. environment: en compose.yml
3. env_file: en compose.yml
4. ENV en Dockerfile
```

---

## Limpieza final

```bash
# Eliminar recursos del lab (si alguno quedó)
docker compose -f compose.env-inline.yml down 2>/dev/null
docker compose -f compose.env-file.yml down 2>/dev/null
docker compose -f compose.env-interp.yml down 2>/dev/null
docker compose -f compose.precedence.yml down 2>/dev/null

# Eliminar archivo .env si quedó
rm -f .env
```

---

## Conceptos reforzados

1. `environment:` define variables directamente en el Compose file. Los
   valores están hardcodeados en el YAML y son visibles en el repositorio.

2. `env_file:` carga variables desde un archivo externo. Permite separar
   configuración sensible del Compose file y usar diferentes archivos
   por entorno.

3. La interpolación `${VAR}` sustituye valores en el **Compose file
   mismo** (no en el contenedor). Los valores vienen del archivo `.env`
   o de variables del host. La sintaxis `${VAR:-default}` proporciona
   valores por defecto.

4. Las variables del host (shell) siempre tienen la mayor prioridad y
   sobrescriben tanto `.env` como `environment:`.

5. Cuando la misma variable se define en `environment:` y `env_file:`,
   `environment:` gana. `docker compose config` muestra la configuración
   final después de resolver todas las fuentes.
