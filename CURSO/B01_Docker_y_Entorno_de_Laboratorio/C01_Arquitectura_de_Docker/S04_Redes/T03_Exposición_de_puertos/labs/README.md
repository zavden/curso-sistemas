# Lab — Exposicion de puertos

## Objetivo

Laboratorio practico para verificar de forma empirica como funciona la
exposicion de puertos en Docker: que EXPOSE es solo metadata, que `-p` es lo
que realmente publica puertos, las variantes de binding, la publicacion
automatica con `-P`, y que los contenedores en la misma red se comunican
sin necesidad de publicar puertos.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagenes base descargadas: `docker pull nginx:alpine` y `docker pull alpine:latest`
- Estar en el directorio `labs/` de este topico

## Archivos del laboratorio

| Archivo | Proposito |
|---|---|
| `Dockerfile.expose-test` | Imagen custom con EXPOSE para demostrar que no publica puertos |

---

## Parte 1 — EXPOSE es solo metadata

### Objetivo

Demostrar que la instruccion `EXPOSE` en un Dockerfile **no publica puertos**.
Solo documenta que la aplicacion utiliza esos puertos. Para que el host pueda
acceder al puerto, se necesita `-p`.

### Paso 1.1: Examinar el Dockerfile

```bash
cat Dockerfile.expose-test
```

Antes de construir, responde mentalmente:

- ¿La instruccion EXPOSE abrira el puerto 80 al host?
- ¿El host podra acceder al servicio nginx sin `-p`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.2: Construir la imagen

```bash
docker build -f Dockerfile.expose-test -t lab-expose-test .
```

### Paso 1.3: Ver la metadata de EXPOSE

```bash
docker inspect lab-expose-test --format '{{json .Config.ExposedPorts}}' | python3 -m json.tool
```

Salida esperada:

```json
{
    "80/tcp": {},
    "8080/tcp": {}
}
```

EXPOSE registra los puertos como metadata en la imagen. Esta informacion
es visible con `docker inspect`, pero no tiene ningun efecto en la red.

### Paso 1.4: Ejecutar sin -p

```bash
docker run -d --name expose-only lab-expose-test
```

### Paso 1.5: Intentar acceder desde el host

```bash
curl -s --connect-timeout 3 http://localhost:80 || echo "No accesible desde el host"
curl -s --connect-timeout 3 http://localhost:8080 || echo "No accesible desde el host"
```

Salida esperada:

```
No accesible desde el host
No accesible desde el host
```

A pesar de tener EXPOSE 80 y EXPOSE 8080 en el Dockerfile, **ningun puerto
es accesible desde el host**. EXPOSE es solo documentacion.

### Paso 1.6: Verificar con docker port

```bash
docker port expose-only
```

Salida esperada: vacia (sin mapeos). No hay puertos publicados.

### Paso 1.7: Limpiar el contenedor sin -p

```bash
docker rm -f expose-only
```

### Paso 1.8: Ejecutar con -p

```bash
docker run -d --name published-web -p 8080:80 nginx:alpine
```

### Paso 1.9: Acceder desde el host

```bash
curl -s http://localhost:8080 | head -5
```

Salida esperada (parcial):

```html
<!DOCTYPE html>
<html>
<head>
<title>Welcome to nginx!</title>
</head>
```

Ahora si funciona. La diferencia es `-p 8080:80`, que crea una regla de
reenvio del puerto 8080 del host al puerto 80 del contenedor.

### Paso 1.10: Verificar el mapeo

```bash
docker port published-web
```

Salida esperada:

```
80/tcp -> 0.0.0.0:8080
```

### Paso 1.11: Limpiar

```bash
docker rm -f published-web
```

---

## Parte 2 — Variantes de -p

### Objetivo

Explorar las diferentes formas de `-p`: binding a todas las interfaces vs
solo localhost, y como verificar los mapeos con `docker port`.

### Paso 2.1: Publicar en todas las interfaces

```bash
docker run -d --name public-web -p 8080:80 nginx:alpine
```

### Paso 2.2: Publicar solo en localhost

```bash
docker run -d --name local-web -p 127.0.0.1:8081:80 nginx:alpine
```

### Paso 2.3: Comparar los mapeos

```bash
echo "=== public-web ==="
docker port public-web

echo ""
echo "=== local-web ==="
docker port local-web
```

Salida esperada:

```
=== public-web ===
80/tcp -> 0.0.0.0:8080

=== local-web ===
80/tcp -> 127.0.0.1:8081
```

La diferencia es critica para la seguridad:

- `0.0.0.0:8080` — accesible desde **cualquier interfaz** de red (incluida
  la IP publica si el host la tiene)
- `127.0.0.1:8081` — accesible **solo desde el host** (localhost)

### Paso 2.4: Verificar con ss

```bash
ss -tlnp | grep -E "8080|8081"
```

Salida esperada (parcial):

```
LISTEN  0  4096  0.0.0.0:8080      0.0.0.0:*
LISTEN  0  4096  127.0.0.1:8081    0.0.0.0:*
```

Observa la columna de direccion local: `0.0.0.0` vs `127.0.0.1`.

### Paso 2.5: Acceder a ambos desde localhost

```bash
curl -s http://localhost:8080 | head -3
curl -s http://localhost:8081 | head -3
```

Ambos responden desde localhost. Pero si este host tuviera una IP publica,
solo el puerto 8080 seria accesible desde el exterior.

**Regla de seguridad**: para servicios internos (bases de datos, caches,
paneles de administracion), siempre usar `-p 127.0.0.1:puerto:puerto`.

### Paso 2.6: Consultar un puerto especifico

```bash
docker port public-web 80
```

Salida esperada:

```
0.0.0.0:8080
```

`docker port` acepta un segundo argumento para consultar un puerto especifico
del contenedor.

### Paso 2.7: Limpiar

```bash
docker rm -f public-web local-web
```

---

## Parte 3 — -P y puertos automaticos

### Objetivo

Demostrar que `-P` (mayuscula) publica automaticamente todos los puertos
declarados con EXPOSE a puertos aleatorios del host.

### Paso 3.1: Ejecutar con -P

Antes de ejecutar, predice: ¿que puertos del host se asignaran? ¿Seran
los mismos 80 y 8080 del EXPOSE, u otros?

Intenta predecir antes de continuar al siguiente paso.

```bash
docker run -d --name auto-web -P lab-expose-test
```

### Paso 3.2: Ver los puertos asignados

```bash
docker port auto-web
```

Salida esperada:

```
80/tcp -> 0.0.0.0:<puerto_aleatorio_1>
8080/tcp -> 0.0.0.0:<puerto_aleatorio_2>
```

`-P` asigno puertos aleatorios del host (tipicamente en el rango 32768+)
para cada puerto declarado con EXPOSE. No usa los puertos 80 y 8080 del
host; los mapea a puertos efimeros.

### Paso 3.3: Acceder usando el puerto asignado

```bash
# Extraer el puerto asignado al puerto 80 del contenedor
PORT=$(docker port auto-web 80 | cut -d: -f2)
echo "Puerto asignado: $PORT"

curl -s http://localhost:$PORT | head -5
```

Salida esperada (parcial):

```html
<!DOCTYPE html>
<html>
<head>
<title>Welcome to nginx!</title>
</head>
```

### Paso 3.4: Comparar -P con -p

`-P` es util para testing rapido cuando no importa el puerto del host.
En produccion, siempre se usa `-p` con puertos especificos para tener
control sobre los mapeos.

| Flag | Uso | Puerto del host |
|---|---|---|
| `-p 8080:80` | Mapeo explicito | 8080 (fijo) |
| `-P` | Mapeo automatico | Aleatorio (rango efimero) |

### Paso 3.5: Limpiar

```bash
docker rm -f auto-web
```

---

## Parte 4 — Comunicacion interna sin -p

### Objetivo

Demostrar que los contenedores en la misma red Docker se comunican
directamente a traves de los puertos internos, **sin necesidad de `-p`**.
La publicacion de puertos solo es necesaria para el acceso desde el host.

### Paso 4.1: Crear la red

```bash
docker network create lab-internal-net
```

### Paso 4.2: Servidor sin puertos publicados

```bash
docker run -d --name internal-server --network lab-internal-net nginx:alpine
```

Observa: **no se usa `-p`**. El puerto 80 de nginx no esta publicado en el
host.

### Paso 4.3: Verificar que el host no puede acceder

Antes de ejecutar, predice: ¿el host podra acceder al puerto 80 de
`internal-server` sin `-p`?

Intenta predecir antes de continuar al siguiente paso.

```bash
curl -s --connect-timeout 3 http://localhost:80 || echo "No accesible desde el host"
```

Salida esperada:

```
No accesible desde el host
```

Sin `-p`, el host no puede llegar al servicio.

### Paso 4.4: Otro contenedor en la misma red SI puede acceder

```bash
docker run --rm --name internal-client --network lab-internal-net alpine \
    wget -qO- http://internal-server:80 | head -5
```

Salida esperada (parcial):

```html
<!DOCTYPE html>
<html>
<head>
<title>Welcome to nginx!</title>
</head>
```

El contenedor `internal-client` accede al puerto 80 de `internal-server`
directamente, usando el nombre DNS y el puerto interno. No se necesita `-p`
para la comunicacion **dentro de la red Docker**.

### Paso 4.5: Limpiar el servidor sin publicar

```bash
docker rm -f internal-server
```

### Paso 4.6: Servidor con puerto publicado

```bash
docker run -d --name published-server --network lab-internal-net -p 9080:80 nginx:alpine
```

### Paso 4.7: Ahora el host SI puede acceder

```bash
curl -s http://localhost:9080 | head -5
```

Salida esperada (parcial):

```html
<!DOCTYPE html>
<html>
<head>
<title>Welcome to nginx!</title>
</head>
```

### Paso 4.8: Y otro contenedor tambien (sin usar el puerto publicado)

```bash
docker run --rm --network lab-internal-net alpine \
    wget -qO- http://published-server:80 | head -5
```

Salida esperada (parcial):

```html
<!DOCTYPE html>
<html>
<head>
<title>Welcome to nginx!</title>
</head>
```

Observa: el contenedor accede al puerto **80** (el puerto interno), no al
**9080** (el puerto publicado en el host). `-p` es una puerta de entrada
desde el host, no afecta la comunicacion interna entre contenedores.

### Paso 4.9: Resumen visual

```
                    Host
                     |
              -p 9080:80
                     |
              +------v------+
              |published-    |
              |server :80   |<---- otro contenedor en la misma red
              +-------------+      (accede directo al puerto 80,
               lab-internal-net     sin necesidad de -p)
```

**Patron de seguridad**: solo publica (`-p`) los puertos que necesitan ser
accesibles desde fuera de la red Docker. Las bases de datos, caches y
servicios internos se comunican por la red Docker interna sin publicar
puertos.

### Paso 4.10: Limpiar

```bash
docker rm -f published-server
docker network rm lab-internal-net
```

---

## Limpieza final

```bash
# Eliminar contenedores huerfanos (si quedo alguno por un paso saltado)
docker rm -f expose-only published-web public-web local-web \
    auto-web internal-server internal-client published-server 2>/dev/null

# Eliminar redes del lab
docker network rm lab-internal-net 2>/dev/null

# Eliminar imagenes del lab
docker rmi lab-expose-test 2>/dev/null
```

---

## Conceptos reforzados

1. `EXPOSE` en un Dockerfile es **solo metadata**. Documenta que puertos usa
   la aplicacion, pero no los publica ni los abre. Sin `-p`, el host no
   puede acceder a esos puertos.

2. `-p hostPort:containerPort` es lo que realmente crea el port mapping. Docker
   crea reglas de reenvio (via iptables/nftables) para dirigir trafico del
   host al contenedor.

3. `-p 127.0.0.1:hostPort:containerPort` restringe el acceso al host local.
   Sin la IP, el puerto se publica en todas las interfaces (`0.0.0.0`),
   potencialmente exponiendolo a la red externa.

4. `-P` (mayuscula) publica automaticamente todos los puertos declarados con
   EXPOSE a puertos aleatorios del host. Util para testing, no para produccion.

5. `docker port` muestra los mapeos de puertos activos de un contenedor.
   Acepta un segundo argumento para consultar un puerto especifico.

6. Los contenedores en la **misma red Docker** se comunican directamente por
   el puerto interno, sin necesidad de `-p`. La publicacion de puertos es
   exclusivamente para acceso desde el host o desde fuera de la red Docker.
