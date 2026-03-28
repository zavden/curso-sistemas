# Lab — Sintaxis YAML y estructura de Compose

## Objetivo

Laboratorio práctico para trabajar con la estructura de un archivo Compose:
crear un servicio mínimo, agregar opciones de servicio, construir imágenes
localmente con `build:`, y gestionar múltiples servicios con naming de
proyecto.

## Prerequisitos

- Docker y Docker Compose v2 instalados (`docker compose version`)
- Imágenes base descargadas: `docker pull nginx:alpine && docker pull alpine:latest`
- Estar en el directorio `labs/` de este tópico

## Archivos del laboratorio

| Archivo | Propósito |
|---|---|
| `compose.minimal.yml` | Compose file mínimo con un solo servicio |
| `compose.options.yml` | Servicio con ports, environment, restart |
| `compose.build.yml` | Servicio que construye imagen localmente |
| `compose.multi.yml` | Dos servicios (web + api) |
| `Dockerfile.app` | Dockerfile para demo de build local |
| `app.sh` | Script simple que simula una aplicación |

---

## Parte 1 — Compose file mínimo

### Objetivo

Crear el Compose file más simple posible, validar su sintaxis con
`docker compose config`, y ejecutar el ciclo básico up/ps/down.

### Paso 1.1: Examinar el archivo mínimo

```bash
cat compose.minimal.yml
```

Salida:

```yaml
services:
  web:
    image: nginx:alpine
```

Este es el Compose file más pequeño posible: un servicio con una imagen.
No hay ports, volumes, networks — solo un servicio llamado `web` que usa
`nginx:alpine`.

### Paso 1.2: Validar con config

```bash
docker compose -f compose.minimal.yml config
```

`config` parsea el YAML y muestra la configuración expandida. Si hay
errores de sintaxis, los muestra antes de crear nada.

Salida esperada (expandida por Compose):

```yaml
name: labs
services:
  web:
    image: nginx:alpine
    networks:
      default: null
networks:
  default:
    name: labs_default
```

Compose agrega automáticamente la red `default` y el nombre del proyecto
(derivado del directorio: `labs`).

### Paso 1.3: Levantar el servicio

```bash
docker compose -f compose.minimal.yml up -d
```

`-d` ejecuta en segundo plano (detached). Sin `-d`, los logs se muestran
en la terminal y Ctrl+C detiene los servicios.

### Paso 1.4: Inspeccionar

```bash
docker compose -f compose.minimal.yml ps
```

Salida esperada:

```
NAME        IMAGE          COMMAND                  SERVICE   CREATED       STATUS       PORTS
labs-web-1  nginx:alpine   "/docker-entrypoint.…"   web       seconds ago   Up seconds   80/tcp
```

El contenedor se llama `labs-web-1`: `labs` (proyecto) + `web` (servicio)
+ `1` (instancia). El puerto 80 está expuesto internamente pero no
mapeado al host.

### Paso 1.5: Destruir

```bash
docker compose -f compose.minimal.yml down
```

`down` detiene y elimina contenedores y redes del proyecto.

---

## Parte 2 — Opciones de servicio

### Objetivo

Demostrar las opciones más comunes de un servicio: `ports`, `environment`
y `restart`.

### Paso 2.1: Examinar el archivo

```bash
cat compose.options.yml
```

Antes de levantar, responde mentalmente:

- ¿En qué puerto del host se accederá al servicio?
- ¿Qué variables de entorno tendrá el contenedor?
- ¿Qué pasará si el contenedor se cae?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.2: Levantar

```bash
docker compose -f compose.options.yml up -d
```

### Paso 2.3: Verificar puerto mapeado

```bash
docker compose -f compose.options.yml ps
```

Salida esperada:

```
NAME        IMAGE          SERVICE   STATUS    PORTS
labs-web-1  nginx:alpine   web       Up        0.0.0.0:8080->80/tcp
```

El puerto 8080 del host está mapeado al 80 del contenedor. Ahora nginx
es accesible desde el host:

```bash
curl -s http://localhost:8080 | head -5
```

### Paso 2.4: Verificar variables de entorno

```bash
docker compose -f compose.options.yml exec web env | grep APP_
```

Salida esperada:

```
APP_NAME=lab-compose
APP_ENV=production
```

Las variables definidas en `environment:` están disponibles dentro del
contenedor.

### Paso 2.5: Probar restart

```bash
docker compose -f compose.options.yml kill web
```

Esperar unos segundos y verificar:

```bash
sleep 3
docker compose -f compose.options.yml ps
```

Con `restart: unless-stopped`, Docker reinicia el contenedor
automáticamente después de un fallo. El STATUS muestra un tiempo de
vida corto (recién reiniciado).

### Paso 2.6: Limpiar

```bash
docker compose -f compose.options.yml down
```

---

## Parte 3 — build local

### Objetivo

Demostrar `build:` para construir una imagen desde un Dockerfile local
en lugar de descargarla con `image:`.

### Paso 3.1: Examinar los archivos

```bash
cat Dockerfile.app
```

```bash
cat app.sh
```

```bash
cat compose.build.yml
```

`compose.build.yml` usa `build:` con `context: .` y `dockerfile:
Dockerfile.app`. Compose construirá la imagen antes de crear el
contenedor.

### Paso 3.2: Construir y levantar

```bash
docker compose -f compose.build.yml up -d --build
```

`--build` fuerza la construcción de la imagen. Sin `--build`, Compose
reutiliza la imagen si ya existe.

### Paso 3.3: Verificar

```bash
docker compose -f compose.build.yml logs
```

Salida esperada:

```
app-1  | App running — PORT=8080 ENV=development
```

El script `app.sh` imprime las variables de entorno. Como no definimos
`APP_ENV` en el Compose file, usa el valor por defecto del script
(`development`).

### Paso 3.4: Verificar la imagen construida

```bash
docker images | grep labs
```

Compose creó una imagen con nombre `labs-app` (proyecto + servicio).

### Paso 3.5: Rebuild y ver cambios

```bash
docker compose -f compose.build.yml down
docker compose -f compose.build.yml up -d --build
docker compose -f compose.build.yml logs --tail 1
```

Sin `--build`, Compose no detecta cambios en el Dockerfile ni en los
archivos copiados. Usar siempre `--build` cuando el código fuente cambia.

### Paso 3.6: Limpiar

```bash
docker compose -f compose.build.yml down --rmi local
```

`--rmi local` elimina las imágenes construidas localmente por este
Compose file.

---

## Parte 4 — Múltiples servicios y nombre del proyecto

### Objetivo

Demostrar un Compose file con múltiples servicios y entender el naming
del proyecto (project name).

### Paso 4.1: Examinar el archivo

```bash
cat compose.multi.yml
```

Dos servicios: `web` (nginx con puerto 8080) y `api` (alpine con loop
que imprime cada 5 segundos). Cada uno es un contenedor independiente.

### Paso 4.2: Levantar ambos servicios

```bash
docker compose -f compose.multi.yml up -d
```

### Paso 4.3: Inspeccionar

```bash
docker compose -f compose.multi.yml ps
```

Salida esperada:

```
NAME        IMAGE           SERVICE   STATUS    PORTS
labs-api-1  alpine:latest   api       Up
labs-web-1  nginx:alpine    web       Up        0.0.0.0:8080->80/tcp
```

Ambos contenedores tienen prefijo `labs-` (nombre del directorio actual).

### Paso 4.4: Logs de un servicio específico

```bash
docker compose -f compose.multi.yml logs api
```

```bash
docker compose -f compose.multi.yml logs
```

Compose diferencia los logs por servicio con prefijos (`api-1 |`,
`web-1 |`).

### Paso 4.5: Nombre del proyecto con -p

```bash
docker compose -f compose.multi.yml down
docker compose -f compose.multi.yml -p mi-app up -d
docker compose -f compose.multi.yml -p mi-app ps
```

Salida esperada:

```
NAME           IMAGE           SERVICE   STATUS    PORTS
mi-app-api-1   alpine:latest   api       Up
mi-app-web-1   nginx:alpine    web       Up        0.0.0.0:8080->80/tcp
```

Ahora los contenedores tienen prefijo `mi-app-` en lugar de `labs-`.

Antes de limpiar, responde mentalmente:

- ¿Qué pasa si ejecutas `docker compose -f compose.multi.yml down`
  (sin `-p`)?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.6: Limpiar

```bash
docker compose -f compose.multi.yml -p mi-app down
```

Si usas `-p` al crear, debes usarlo también al destruir. Sin `-p`,
Compose busca el proyecto `labs` (que no tiene contenedores activos de
este archivo).

---

## Limpieza final

```bash
# Eliminar recursos del lab (si alguno quedó)
docker compose -f compose.minimal.yml down 2>/dev/null
docker compose -f compose.options.yml down 2>/dev/null
docker compose -f compose.build.yml down --rmi local 2>/dev/null
docker compose -f compose.multi.yml down 2>/dev/null
```

---

## Conceptos reforzados

1. El Compose file más simple posible necesita solo `services:` con un
   servicio y su `image:`. Compose agrega automáticamente la red default
   y el nombre del proyecto.

2. `docker compose config` valida el YAML y muestra la configuración
   expandida. Usarlo siempre antes del primer `up` para detectar errores
   de sintaxis.

3. Las opciones `ports`, `environment` y `restart` se definen a nivel
   de servicio. `restart: unless-stopped` reinicia el contenedor
   automáticamente después de un fallo.

4. `build:` construye la imagen desde un Dockerfile local. `--build`
   fuerza la reconstrucción; sin `--build`, Compose reutiliza la imagen
   existente. `--rmi local` elimina imágenes construidas al hacer `down`.

5. El nombre del proyecto (project name) se deriva del directorio. Se
   puede sobrescribir con `-p` o `COMPOSE_PROJECT_NAME`. Los contenedores
   se nombran `{proyecto}-{servicio}-{instancia}`.
