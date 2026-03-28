# Lab — Ciclo de vida de servicios

## Objetivo

Laboratorio práctico para dominar el ciclo de vida de servicios en Docker
Compose: crear y destruir con up/down, detección de cambios en la
configuración, diferencia entre stop/start y down, y ejecución de
comandos con exec y run.

## Prerequisitos

- Docker y Docker Compose v2 instalados (`docker compose version`)
- Imágenes base descargadas: `docker pull nginx:alpine && docker pull alpine:latest`
- Estar en el directorio `labs/` de este tópico

## Archivos del laboratorio

| Archivo | Propósito |
|---|---|
| `compose.yml` | Web (nginx, puerto 8080) + worker (alpine con loop) |
| `compose.updated.yml` | Versión modificada: puerto cambiado a 9090 |

---

## Parte 1 — up y down

### Objetivo

Ejecutar el ciclo completo de vida: levantar servicios en segundo plano,
inspeccionar su estado, ver logs, y destruir todo.

### Paso 1.1: Examinar el archivo

```bash
cat compose.yml
```

Dos servicios: `web` (nginx con puerto 8080 mapeado al host) y `worker`
(alpine ejecutando un loop que imprime cada 3 segundos).

### Paso 1.2: Levantar en segundo plano

```bash
docker compose up -d
```

Salida esperada:

```
[+] Running 3/3
 ✔ Network labs_default     Created
 ✔ Container labs-worker-1  Started
 ✔ Container labs-web-1     Started
```

Compose creó la red default y ambos contenedores.

### Paso 1.3: Inspeccionar estado

```bash
docker compose ps
```

Salida esperada:

```
NAME             IMAGE           SERVICE   STATUS    PORTS
labs-web-1       nginx:alpine    web       Up        0.0.0.0:8080->80/tcp
labs-worker-1    alpine:latest   worker    Up
```

### Paso 1.4: Ver logs

```bash
docker compose logs worker
```

Muestra las iteraciones del worker acumuladas hasta ahora.

```bash
docker compose logs -f worker
```

`-f` sigue los logs en tiempo real. Presionar **Ctrl+C** para salir
(no detiene el contenedor).

### Paso 1.5: Logs combinados

```bash
docker compose logs --tail 3
```

Muestra las últimas 3 líneas de **todos** los servicios. Compose
diferencia cada servicio con un prefijo (`web-1 |`, `worker-1 |`).

### Paso 1.6: Destruir

```bash
docker compose down
```

Salida esperada:

```
[+] Running 3/3
 ✔ Container labs-web-1     Removed
 ✔ Container labs-worker-1  Removed
 ✔ Network labs_default     Removed
```

`down` detiene, elimina los contenedores, y elimina la red.

---

## Parte 2 — Detección de cambios

### Objetivo

Demostrar que `up -d` es idempotente: no recrea contenedores sin
cambios, pero detecta y recrea solo los servicios cuya configuración
cambió.

### Paso 2.1: Levantar servicios

```bash
docker compose up -d
```

### Paso 2.2: Ejecutar up otra vez sin cambios

```bash
docker compose up -d
```

Salida esperada:

```
 ✔ Container labs-web-1     Running
 ✔ Container labs-worker-1  Running
```

Ambos muestran `Running`, no `Recreated`. Compose detecta que la
configuración no cambió y no recrea nada.

### Paso 2.3: Examinar el archivo modificado

```bash
diff compose.yml compose.updated.yml
```

Salida esperada:

```
<       - "8080:80"
---
>       - "9090:80"
```

El único cambio es el puerto de `web`: de 8080 a 9090. `worker` es
idéntico.

Antes de aplicar el cambio, responde mentalmente:

- ¿Compose recreará `web`?
- ¿Compose recreará `worker`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.4: Aplicar cambio

```bash
docker compose -f compose.updated.yml up -d
```

Salida esperada:

```
 ✔ Container labs-worker-1  Running
 ✔ Container labs-web-1     Recreated
```

`worker` sigue `Running` (sin cambios). `web` fue `Recreated` porque
su puerto cambió. Compose recrea solo lo necesario.

### Paso 2.5: Verificar

```bash
docker compose -f compose.updated.yml ps
```

`web` ahora está en el puerto 9090:

```bash
curl -s http://localhost:9090 | head -5
```

### Paso 2.6: Limpiar

```bash
docker compose -f compose.updated.yml down
```

---

## Parte 3 — stop/start vs up/down

### Objetivo

Demostrar la diferencia entre `stop` (detiene pero preserva contenedores)
y `down` (destruye todo).

### Paso 3.1: Levantar servicios

```bash
docker compose up -d
```

### Paso 3.2: Detener con stop

```bash
docker compose stop
```

```bash
docker compose ps -a
```

Salida esperada:

```
NAME             IMAGE           SERVICE   STATUS
labs-web-1       nginx:alpine    web       Exited (0)
labs-worker-1    alpine:latest   worker    Exited (137)
```

Los contenedores están detenidos (`Exited`) pero **siguen existiendo**.
`-a` muestra todos, incluyendo los detenidos.

### Paso 3.3: Reanudar con start

```bash
docker compose start
```

```bash
docker compose ps
```

Salida esperada:

```
NAME             IMAGE           SERVICE   STATUS    PORTS
labs-web-1       nginx:alpine    web       Up        0.0.0.0:8080->80/tcp
labs-worker-1    alpine:latest   worker    Up
```

`start` reanuda los contenedores existentes. No crea nada nuevo.

### Paso 3.4: Comparar con down

```bash
docker compose down
```

```bash
docker compose ps -a
```

Salida esperada: sin resultados. `down` eliminó los contenedores
completamente.

### Paso 3.5: start sin contenedores previos

```bash
docker compose start 2>&1
```

Salida esperada:

```
no container found for project "labs": not found
```

`start` solo reanuda contenedores existentes. Si fueron destruidos con
`down`, no hay nada que reanudar. Para crear desde cero, usar `up`.

### Paso 3.6: restart

```bash
docker compose up -d
docker compose restart worker
docker compose ps
```

`restart` detiene y arranca un servicio que ya está corriendo. Es útil
para recargar configuración sin destruir el contenedor.

### Paso 3.7: Limpiar

```bash
docker compose down
```

---

## Parte 4 — exec y run

### Objetivo

Ejecutar comandos dentro de contenedores: `exec` en uno existente
(corriendo), `run` en uno nuevo temporal.

### Paso 4.1: Levantar servicios

```bash
docker compose up -d
```

### Paso 4.2: exec — comando en contenedor existente

```bash
docker compose exec web cat /etc/os-release | head -2
```

Salida esperada:

```
NAME="Alpine Linux"
ID=alpine
```

`exec` ejecuta el comando **dentro del contenedor `web` que ya está
corriendo**. No crea un contenedor nuevo.

```bash
docker compose exec web ls /usr/share/nginx/html/
```

### Paso 4.3: exec interactivo

```bash
docker compose exec web sh
```

Abre un shell interactivo dentro de `web`. Explorar y salir con `exit`.

### Paso 4.4: run — contenedor temporal

```bash
docker compose run --rm worker echo "one-off task completed"
```

Salida esperada:

```
one-off task completed
```

`run` crea un **nuevo contenedor** basado en la definición del servicio
`worker`, ejecuta el comando, y `--rm` lo elimina al terminar.

### Paso 4.5: Diferencia clave entre exec y run

```bash
docker compose exec worker hostname
```

```bash
docker compose run --rm worker hostname
```

Los hostnames son diferentes. `exec` ejecuta en el contenedor
`labs-worker-1` que ya existe. `run` creó un contenedor nuevo con
un nombre e ID diferentes.

Antes de limpiar, responde mentalmente:

- ¿Cuántos contenedores `worker` hay ahora corriendo?
- ¿El contenedor de `run` sigue existiendo?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.6: Verificar

```bash
docker compose ps
```

Solo hay un `worker` corriendo (el original). El de `run --rm` fue
eliminado al terminar.

### Paso 4.7: Limpiar

```bash
docker compose down
```

---

## Limpieza final

```bash
# Eliminar recursos del lab (si alguno quedó)
docker compose down 2>/dev/null
docker compose -f compose.updated.yml down 2>/dev/null
```

---

## Conceptos reforzados

1. `up -d` crea contenedores, redes y los arranca en segundo plano.
   `down` detiene y elimina contenedores y redes.

2. `up -d` es **idempotente**: no recrea contenedores sin cambios. Si
   la configuración de un servicio cambió, solo recrea ese servicio.

3. `stop` detiene contenedores pero los preserva (estado `Exited`).
   `start` los reanuda. `down` los destruye completamente.

4. `exec` ejecuta comandos en un contenedor **que ya está corriendo**.
   `run` crea un **contenedor nuevo** basado en la definición del
   servicio.

5. `logs -f` sigue los logs en tiempo real sin detener el servicio.
   `--tail N` limita la cantidad de líneas mostradas.
