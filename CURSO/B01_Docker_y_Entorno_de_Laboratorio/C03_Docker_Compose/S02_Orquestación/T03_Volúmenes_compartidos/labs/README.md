# Lab — Volúmenes compartidos

## Objetivo

Laboratorio práctico para trabajar con volúmenes en Docker Compose:
named volumes para persistir datos, bind mounts para desarrollo,
compartir volúmenes entre servicios, y entender el impacto de `down -v`
en la pérdida de datos.

## Prerequisitos

- Docker y Docker Compose v2 instalados (`docker compose version`)
- Imágenes descargadas: `docker pull nginx:alpine && docker pull alpine:latest`
- Estar en el directorio `labs/` de este tópico

## Archivos del laboratorio

| Archivo | Propósito |
|---|---|
| `compose.named.yml` | Servicio con named volume |
| `compose.bind.yml` | Servicio con bind mount (`:ro`) |
| `compose.shared.yml` | Writer + reader compartiendo volumen |
| `compose.persist.yml` | Servicio para demo de persistencia |
| `html/index.html` | Contenido HTML para bind mount |

---

## Parte 1 — Named volumes

### Objetivo

Demostrar que un named volume persiste datos entre ciclos de `down` y
`up`. Los datos sobreviven a la destrucción de contenedores.

### Paso 1.1: Examinar el archivo

```bash
cat compose.named.yml
```

El servicio `app` monta el volumen `app-data` en `/data`. Al arrancar,
escribe un timestamp en `/data/log.txt` y lo muestra.

### Paso 1.2: Primer arranque

```bash
docker compose -f compose.named.yml up -d
docker compose -f compose.named.yml logs app
```

Salida esperada:

```
app-1  | Written at Mon Mar 17 ...
```

Una sola línea en el log.

### Paso 1.3: Destruir y re-crear

```bash
docker compose -f compose.named.yml down
docker compose -f compose.named.yml up -d
docker compose -f compose.named.yml logs app
```

Salida esperada:

```
app-1  | Written at Mon Mar 17 ... (primera ejecución)
app-1  | Written at Mon Mar 17 ... (segunda ejecución)
```

Ahora hay **dos líneas**. El volumen preservó el `log.txt` de la
primera ejecución. `down` destruyó el contenedor pero no el volumen.

### Paso 1.4: Verificar el volumen

```bash
docker volume ls | grep app-data
```

El volumen `labs_app-data` existe independientemente del contenedor.

### Paso 1.5: Tercera ejecución

```bash
docker compose -f compose.named.yml down
docker compose -f compose.named.yml up -d
docker compose -f compose.named.yml logs app
```

Tres líneas. Los datos se acumulan entre ciclos de vida.

### Paso 1.6: Limpiar con -v

```bash
docker compose -f compose.named.yml down -v
```

`-v` elimina el named volume. Los datos se pierden.

```bash
docker volume ls | grep app-data
```

El volumen ya no existe.

---

## Parte 2 — Bind mounts

### Objetivo

Demostrar que un bind mount monta un directorio del host en el
contenedor. Los cambios en el host se reflejan inmediatamente en el
contenedor y viceversa.

### Paso 2.1: Examinar los archivos

```bash
cat compose.bind.yml
cat html/index.html
```

`compose.bind.yml` monta `./html` en `/usr/share/nginx/html` con `:ro`
(read-only). Nginx servirá el contenido del directorio `html/` del host.

### Paso 2.2: Levantar

```bash
docker compose -f compose.bind.yml up -d
```

### Paso 2.3: Verificar contenido

```bash
curl -s http://localhost:8080
```

Salida esperada:

```
<h1>Version 1</h1>
```

Nginx sirve el archivo `html/index.html` del host.

### Paso 2.4: Modificar en el host

```bash
echo '<h1>Version 2 - edited on host</h1>' > html/index.html
```

```bash
curl -s http://localhost:8080
```

Salida esperada:

```
<h1>Version 2 - edited on host</h1>
```

El cambio se refleja **inmediatamente** sin reiniciar el contenedor.
El bind mount vincula directamente el directorio del host con el del
contenedor.

### Paso 2.5: Read-only impide escritura desde el contenedor

```bash
docker compose -f compose.bind.yml exec web sh -c 'echo "test" > /usr/share/nginx/html/hack.html' 2>&1
```

Salida esperada:

```
sh: can't create /usr/share/nginx/html/hack.html: Read-only file system
```

El sufijo `:ro` impide que el contenedor modifique archivos del host.
Esto protege contra vulnerabilidades que intenten escribir en el
filesystem montado.

### Paso 2.6: Restaurar y limpiar

```bash
echo '<h1>Version 1</h1>' > html/index.html
docker compose -f compose.bind.yml down
```

---

## Parte 3 — Volúmenes compartidos entre servicios

### Objetivo

Demostrar que dos servicios pueden compartir el mismo named volume.
Un servicio escribe contenido y el otro lo sirve.

### Paso 3.1: Examinar el archivo

```bash
cat compose.shared.yml
```

Dos servicios comparten el volumen `shared`:
- `writer`: escribe un `index.html` con timestamp cada 5 segundos
- `reader`: nginx sirve el contenido en `:ro` (solo lectura)

Antes de levantar, responde mentalmente:

- ¿Qué mostrará `curl localhost:8080` la primera vez?
- ¿Cambiará el contenido si esperas 10 segundos?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.2: Levantar

```bash
docker compose -f compose.shared.yml up -d
```

### Paso 3.3: Verificar contenido dinámico

```bash
sleep 3
curl -s http://localhost:8080
```

Salida esperada:

```
<h1>Generated at Mon Mar 17 HH:MM:SS UTC 2026</h1>
```

```bash
sleep 6
curl -s http://localhost:8080
```

El timestamp cambió. `writer` actualiza el archivo cada 5 segundos
y `reader` (nginx) sirve la versión más reciente.

### Paso 3.4: Verificar logs del writer

```bash
docker compose -f compose.shared.yml logs writer --tail 3
```

Cada línea muestra cuándo se escribió el archivo.

### Paso 3.5: El reader no puede escribir

```bash
docker compose -f compose.shared.yml exec reader sh -c 'echo "test" > /usr/share/nginx/html/test.html' 2>&1
```

Salida esperada:

```
sh: can't create /usr/share/nginx/html/test.html: Read-only file system
```

`reader` monta el volumen con `:ro` — solo puede leer. Solo `writer`
tiene permisos de escritura.

### Paso 3.6: Limpiar

```bash
docker compose -f compose.shared.yml down -v
```

---

## Parte 4 — down vs down -v

### Objetivo

Demostrar que `down` preserva los named volumes (y los datos) mientras
que `down -v` los elimina permanentemente.

### Paso 4.1: Crear datos en un volumen

```bash
docker compose -f compose.persist.yml up -d
```

```bash
docker compose -f compose.persist.yml exec db sh -c '
    echo "important data" > /data/records.txt
    echo "config value" > /data/config.txt
    ls -la /data/
'
```

Dos archivos escritos en el volumen `db-data`.

### Paso 4.2: down sin -v — datos preservados

```bash
docker compose -f compose.persist.yml down
```

```bash
docker volume ls | grep db-data
```

Salida esperada:

```
local     labs_db-data
```

El volumen sigue existiendo.

```bash
docker compose -f compose.persist.yml up -d
docker compose -f compose.persist.yml exec db cat /data/records.txt
```

Salida esperada:

```
important data
```

Los datos sobrevivieron al ciclo down/up.

### Paso 4.3: down con -v — datos perdidos

Antes de ejecutar, responde mentalmente:

- ¿Qué pasará con los archivos en `/data/`?
- ¿Podrás recuperar `records.txt` después?

Intenta predecir antes de continuar al siguiente paso.

```bash
docker compose -f compose.persist.yml down -v
```

```bash
docker volume ls | grep db-data
```

Salida esperada: sin resultados. El volumen fue eliminado.

```bash
docker compose -f compose.persist.yml up -d
docker compose -f compose.persist.yml exec db ls /data/
```

Salida esperada: directorio vacío. Los datos se perdieron
**permanentemente**.

### Paso 4.4: Limpiar

```bash
docker compose -f compose.persist.yml down -v
```

---

## Limpieza final

```bash
# Eliminar recursos del lab (si alguno quedó)
docker compose -f compose.named.yml down -v 2>/dev/null
docker compose -f compose.bind.yml down 2>/dev/null
docker compose -f compose.shared.yml down -v 2>/dev/null
docker compose -f compose.persist.yml down -v 2>/dev/null

# Restaurar html/index.html
echo '<h1>Version 1</h1>' > html/index.html
```

---

## Conceptos reforzados

1. Los **named volumes** persisten datos entre ciclos de `down` y `up`.
   Los contenedores se destruyen pero el volumen permanece hasta que se
   usa `down -v` o `docker volume rm`.

2. Los **bind mounts** montan un directorio del host en el contenedor.
   Los cambios en el host se reflejan inmediatamente sin reiniciar. El
   sufijo `:ro` protege contra escritura desde el contenedor.

3. Dos servicios pueden **compartir un volumen** montándolo en rutas
   diferentes. Un servicio puede escribir (read-write) mientras el otro
   solo lee (`:ro`).

4. `down` preserva los named volumes. `down -v` los **elimina
   permanentemente** junto con todos sus datos. Usar `-v` con
   precaución, especialmente en entornos con datos de producción.

5. Los paths de bind mounts se resuelven **desde el directorio del
   Compose file**, no desde donde se ejecuta `docker compose`.
