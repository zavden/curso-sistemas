# T03 — Exposición de puertos

## Puertos dentro vs fuera del contenedor

Un contenedor tiene su propia **interfaz de red y su propio espacio de puertos**.
Un servidor web escuchando en el puerto 80 dentro del contenedor no es accesible
desde el host a menos que se establezca un **mapeo de puertos** explícito.

```
                Sin -p                           Con -p 8080:80
┌──────────────────────────┐       ┌──────────────────────────┐
│         HOST              │       │         HOST              │
│                          │       │                          │
│  curl localhost:80       │       │  curl localhost:8080     │
│  → Connection refused    │       │  → ¡Responde!           │
│                          │       │         │                │
│  ┌────────────────────┐  │       │  ┌──────┼─────────────┐  │
│  │   CONTENEDOR       │  │       │  │   CONTENEDOR       │  │
│  │   nginx :80        │  │       │  │   nginx :80        │  │
│  │   (solo red Docker)│  │       │  │   (mapeado via NAT)│  │
│  └────────────────────┘  │       │  └────────────────────┘  │
└──────────────────────────┘       └──────────────────────────┘
```

Sin port mapping, el puerto 80 del contenedor **solo es accesible desde la red
Docker** (otros contenedores en la misma red pueden conectarse directamente).

## `EXPOSE` en Dockerfile — Solo documentación

La instrucción `EXPOSE` en un Dockerfile **no abre ni publica puertos**. Es
exclusivamente metadata que documenta qué puertos utiliza la aplicación:

```dockerfile
FROM nginx:latest
EXPOSE 80
EXPOSE 443
# Estos puertos NO están abiertos al host — solo documentan la intención
```

```bash
# Ver los puertos que EXPOSE declara
docker inspect -f '{{json .Config.ExposedPorts}}' nginx
# {"80/tcp":{}}

# Pero sin -p, el puerto no es accesible desde el host
docker run -d --name no_port nginx
curl http://localhost:80  # Connection refused
docker rm -f no_port
```

`EXPOSE` es útil para:
- Documentar qué puertos usa la imagen
- Que `-P` (mayúscula) sepa qué puertos mapear automáticamente
- Que herramientas como Docker Compose lean la metadata
- Auditoría de qué servicios expone una imagen

## `-p` — Port mapping

El flag `-p` crea una regla de reenvío de puertos del host al contenedor:

```bash
# Sintaxis: -p [IP_host:]puerto_host:puerto_contenedor[/protocolo]

# Forma básica: host:8080 → contenedor:80
docker run -d -p 8080:80 --name web nginx
curl http://localhost:8080  # Responde nginx

# Múltiples puertos
docker run -d -p 8080:80 -p 8443:443 --name web nginx

# Mismo puerto en host y contenedor
docker run -d -p 80:80 --name web nginx
```

### Variantes de `-p`

| Sintaxis | Efecto |
|---|---|
| `-p 8080:80` | host 0.0.0.0:8080 → contenedor :80 (todas las interfaces) |
| `-p 127.0.0.1:8080:80` | solo localhost:8080 → contenedor :80 |
| `-p 192.168.1.10:8080:80` | solo esa IP:8080 → contenedor :80 |
| `-p 8080:80/udp` | puerto UDP |
| `-p 8080:80/tcp -p 8080:80/udp` | TCP y UDP en el mismo puerto |
| `-p 8080-8090:80-90` | rango de puertos (10 puertos mapeados) |
| `-p 80` | host :puerto_aleatorio → contenedor :80 (rango efímero) |

### Bind a localhost vs todas las interfaces

Esta distinción es **crítica para la seguridad**:

```bash
# TODAS las interfaces (0.0.0.0) — accesible desde la red externa
docker run -d -p 8080:80 nginx
# Equivalente a:
docker run -d -p 0.0.0.0:8080:80 nginx

# En un servidor con IP pública, esto expone el servicio a internet

# SOLO localhost — accesible solo desde el host
docker run -d -p 127.0.0.1:8080:80 nginx

# En un servidor con IP pública:
# curl http://servidor:8080    ← no funciona desde otra máquina
# curl http://localhost:8080   ← sí funciona desde el host
```

En un servidor con IP pública, `-p 8080:80` expone el puerto a **todo internet**.
Para servicios internos (bases de datos, caches, paneles de administración), siempre
usa `-p 127.0.0.1:puerto:puerto`.

```bash
# PostgreSQL solo accesible desde localhost
docker run -d -p 127.0.0.1:5432:5432 \
  -e POSTGRES_PASSWORD=secret \
  postgres:16

# Redis solo accesible desde localhost
docker run -d -p 127.0.0.1:6379:6379 redis:7
```

## `-P` — Publicar todos los puertos EXPOSE

`-P` (mayúscula) mapea **todos los puertos declarados con EXPOSE** a puertos
aleatorios del host (rango efímero, típicamente 32768-60999):

```bash
docker run -d -P --name auto_port nginx

# Ver los puertos asignados
docker port auto_port
# 80/tcp -> 0.0.0.0:32768

# Acceder con el puerto asignado
curl http://localhost:32768

docker rm -f auto_port
```

`-P` es útil para testing rápido o cuando no te importa el puerto del host, pero
en producción siempre usarás `-p` con puertos específicos.

## `docker port` — Ver mapeos activos

```bash
docker run -d -p 8080:80 -p 8443:443 --name web nginx

# Ver todos los mapeos
docker port web
# 80/tcp -> 0.0.0.0:8080
# 443/tcp -> 0.0.0.0:8443

# Ver mapeo de un puerto específico
docker port web 80
# 0.0.0.0:8080

docker rm -f web
```

## Rangos de puertos

```bash
# Mapear un rango consecutivo de puertos
docker run -d -p 8000-8002:80-82 --name range_test nginx

# Ver los mapeos
docker port range_test
# 80/tcp -> 0.0.0.0:8000
# 81/tcp -> 0.0.0.0:8001
# 82/tcp -> 0.0.0.0:8002

# Equivalente a:
# docker run -p 8000:80 -p 8001:81 -p 8002:82 nginx

docker rm -f range_test
```

## Qué pasa por debajo: iptables

Cuando usas `-p`, Docker crea reglas en **iptables** (o nftables) para hacer el
port forwarding. Esto tiene consecuencias importantes:

```bash
# Ver las reglas creadas por Docker
sudo iptables -t nat -L DOCKER -n
# DNAT  tcp  --  0.0.0.0/0  0.0.0.0/0  tcp dpt:8080 to:172.17.0.2:80
```

**Docker ignora el firewall del host**. Las reglas `-p` se insertan en la cadena
`DOCKER` de iptables, que se evalúa **antes** que las reglas de `ufw` o `firewalld`.

```
Paquete entrante → PREROUTING → cadena DOCKER → DNAT al contenedor
                                     ↑
                               Ignora ufw/firewalld
```

Esto significa que si publicas un puerto con `-p 5432:5432`, ese puerto está
abierto **aunque `ufw` lo bloquee**. Es una fuente frecuente de sorpresas de
seguridad.

### La cadena DOCKER-USER

Para controlar qué tráfico llega a los contenedores, Docker proporciona la cadena
`DOCKER-USER` que se evalúa **antes** que la cadena `DOCKER`:

```bash
# Ejemplo: solo permitir acceso al puerto 8080 desde localhost
sudo iptables -I DOCKER-USER -p tcp --dport 8080 -s 127.0.0.1 -j ACCEPT
sudo iptables -I DOCKER-USER -p tcp --dport 8080 -j DROP

# Ahora aunque -p 8080:80 exponga el puerto, solo localhost puede acceder
```

### Desactivar manipulación de iptables

En `/etc/docker/daemon.json`:

```json
{
  "iptables": false
}
```

Esto desactiva **toda** la manipulación de iptables por Docker — pierdes el port
forwarding automático, el NAT, y la comunicación entre contenedores. Solo para
situaciones donde necesitas control manual total de las reglas de red.

### El proceso docker-proxy

En algunos casos, Docker usa un proxy en userspace llamado `docker-proxy`:

```bash
# Ver procesos docker-proxy
ps aux | grep docker-proxy

# Ver qué procesos escuchan en un puerto
ss -tlnp | grep :8080
# LISTEN 0  4096  0.0.0.0:8080  0.0.0.0:*  users:(("docker-proxy",...))
```

El `docker-proxy` reenvía tráfico desde el puerto del host al contenedor. En la
mayoría de los casos modernos, el forwarding real lo hace iptables directamente;
docker-proxy actúa como fallback (especialmente para tráfico desde localhost al
propio host).

## Puertos sin `-p`: acceso entre contenedores

Dentro de la misma red Docker, los contenedores se comunican directamente **sin
necesidad de publicar puertos**. `-p` solo es necesario para acceso desde el host
o desde fuera.

```bash
docker network create internal

# PostgreSQL sin -p — no accesible desde el host
docker run -d --name db --network internal \
  -e POSTGRES_PASSWORD=secret postgres:16

# La aplicación en la misma red puede conectarse por nombre y puerto interno
docker run --rm --network internal postgres:16 \
  psql -h db -U postgres -c '\conninfo'
# Conectado a db:5432 — funciona sin -p

# Desde el host, 5432 no es accesible
psql -h localhost -U postgres 2>&1 || echo "No accesible desde el host"

docker rm -f db
docker network rm internal
```

**Patrón de seguridad**: solo publica (`-p`) los puertos que necesitan ser
accesibles desde fuera de la red Docker. Bases de datos, caches, servicios
internos deben comunicarse por la red Docker interna.

### Diagrama de comunicación

```
Internet
    │
    │ :80, :443
    ▼
┌──────────────────────────────────────────────────────┐
│                    HOST                               │
│                                                      │
│   nginx (contenedor)                                 │
│   │  └─ :80, :443 EXPUESTO con -p                   │
│   │                                                  │
│   │  /api → :3000 (NO expuesto, solo red interna)    │
│   │                                                  │
│   └─ api (contenedor)                                │
│         │                                            │
│         │ → :5432 (NO expuesto, solo red interna)    │
│         │                                            │
│         └─ db (contenedor)                           │
│                                                      │
│   La DB NO es accesible desde internet               │
└──────────────────────────────────────────────────────┘
```

## Conflictos de puertos

Si el puerto del host ya está en uso, Docker falla:

```bash
# Primer contenedor: OK
docker run -d -p 8080:80 --name web1 nginx

# Segundo contenedor: ERROR — el puerto 8080 del host ya está ocupado
docker run -d -p 8080:80 --name web2 nginx
# Error: bind: address already in use

# Solución: usar un puerto diferente
docker run -d -p 8081:80 --name web2 nginx

docker rm -f web1 web2
```

Para encontrar qué proceso ocupa un puerto:

```bash
ss -tlnp | grep :8080
# LISTEN  0  4096  0.0.0.0:8080  0.0.0.0:*  users:(("docker-proxy",...))
```

## Casos de uso típicos

| Servicio | Port mapping | Razón |
|---|---|---|
| Nginx/Apache | `-p 80:80 -p 443:443` | Debe ser accesible desde internet |
| API REST interna | Sin `-p` o `-p 127.0.0.1:3000:3000` | Solo accesible internamente |
| Base de datos | Sin `-p` o `-p 127.0.0.1:5432:5432` | Solo accesible desde la red Docker |
| Redis/Memcached | Sin `-p` o `-p 127.0.0.1:6379:6379` | Solo accesible internamente |
| Panel admin | `-p 127.0.0.1:8080:8080` | Solo acceso local |

## Verificar conectividad

```bash
# Ver si el puerto está escuchando
ss -tlnp | grep :8080

# Probar conectividad local
curl -v http://localhost:8080

# Ver logs del servicio para confirmar que llegan requests
docker logs web

# Ver conexiones activas dentro del contenedor
docker exec web ss -tlnp
```

---

## Práctica

### Ejercicio 1 — Port mapping básico

```bash
# Levantar nginx con port mapping
docker run -d -p 8080:80 --name port_test nginx

# Acceder desde el host
curl -s http://localhost:8080 | head -5

# Ver el mapeo
docker port port_test

# Verificar que el puerto está escuchando en el host
ss -tlnp | grep :8080

# Ver reglas NAT
sudo iptables -t nat -L DOCKER -n | grep 8080

# Limpiar
docker rm -f port_test
```

**Predicción**: `curl` devuelve la página de bienvenida de nginx. `docker port`
muestra `80/tcp -> 0.0.0.0:8080`. `ss` muestra docker-proxy escuchando en 8080.

### Ejercicio 2 — localhost vs todas las interfaces

```bash
# Publicar en todas las interfaces
docker run -d -p 8080:80 --name public_web nginx

# Publicar solo en localhost
docker run -d -p 127.0.0.1:8081:80 --name local_web nginx

# Ver la diferencia
docker port public_web
# 80/tcp -> 0.0.0.0:8080  ← accesible desde cualquier interfaz

docker port local_web
# 80/tcp -> 127.0.0.1:8081  ← solo localhost

# Verificar con ss
ss -tlnp | grep 8080
# 0.0.0.0:8080     ← todas las interfaces
ss -tlnp | grep 8081
# 127.0.0.1:8081   ← solo localhost

# Limpiar
docker rm -f public_web local_web
```

**Predicción**: `public_web` escucha en `0.0.0.0` (todas las interfaces),
`local_web` solo en `127.0.0.1`.

### Ejercicio 3 — `-P` con puertos automáticos

```bash
# Usar -P para asignar puertos automáticamente
docker run -d -P --name auto_web nginx

# Ver qué puerto se asignó
docker port auto_web
# 80/tcp -> 0.0.0.0:XXXXX

# Extraer el puerto programáticamente
PORT=$(docker port auto_web 80 | head -1 | cut -d: -f2)
echo "Puerto asignado: $PORT"
curl -s http://localhost:$PORT | head -3

# Limpiar
docker rm -f auto_web
```

**Predicción**: Docker asigna un puerto aleatorio del rango efímero (32768+).
`-P` mapea todos los puertos declarados con `EXPOSE` en la imagen.

### Ejercicio 4 — Comunicación interna sin `-p`

```bash
docker network create port_test_net

# Servidor sin puertos publicados
docker run -d --name internal_server --network port_test_net nginx

# Otro contenedor en la misma red puede acceder al puerto 80 directamente
docker run --rm --network port_test_net alpine wget -qO- http://internal_server | head -3
# Responde nginx — sin -p necesario dentro de la red Docker

# Desde el host, no es accesible
curl -s --connect-timeout 2 http://localhost:80 || echo "No accesible desde el host"

# Limpiar
docker rm -f internal_server
docker network rm port_test_net
```

**Predicción**: nginx responde a `wget` desde otro contenedor en la misma red,
pero `curl` desde el host falla. Los contenedores se comunican directamente sin `-p`.

### Ejercicio 5 — Rangos de puertos

```bash
# Mapear rango de puertos
docker run -d -p 8000-8002:80-82 --name range_test nginx

# Ver los mapeos
docker port range_test
# 80/tcp -> 0.0.0.0:8000
# 81/tcp -> 0.0.0.0:8001
# 82/tcp -> 0.0.0.0:8002

# Solo el puerto 80 está activo en nginx, los demás darán connection refused
curl -s http://localhost:8000 | head -1
# <!DOCTYPE html> ...
curl -s --connect-timeout 2 http://localhost:8001 || echo "Puerto 81 no escucha en nginx"

# Limpiar
docker rm -f range_test
```

**Predicción**: los 3 puertos del host están mapeados, pero solo el 8000 (→80)
responde porque nginx solo escucha en el 80.

### Ejercicio 6 — Inspección de reglas iptables

```bash
# Crear un contenedor con puerto publicado
docker run -d -p 8080:80 --name iptables_test nginx

# Ver reglas NAT de Docker
sudo iptables -t nat -L DOCKER -n -v

# Ver la cadena DOCKER-USER (vacía por defecto, aquí van tus reglas)
sudo iptables -L DOCKER-USER -n

# Limpiar
docker rm -f iptables_test
```

**Predicción**: la tabla NAT mostrará una regla DNAT que redirige tráfico del
puerto 8080 hacia la IP del contenedor (172.17.0.x:80).

### Ejercicio 7 — Conflictos de puertos

```bash
# Primer contenedor
docker run -d -p 8080:80 --name conflict1 nginx

# Segundo contenedor en el mismo puerto
docker run -d -p 8080:80 --name conflict2 nginx 2>&1
# Error: bind: address already in use

# Ver qué está usando el puerto
ss -tlnp | grep :8080

# Solución: puerto diferente
docker run -d -p 8081:80 --name conflict2 nginx

# Ambos funcionan en puertos distintos
curl -s http://localhost:8080 | head -1
curl -s http://localhost:8081 | head -1

# Limpiar
docker rm -f conflict1 conflict2
```

**Predicción**: el segundo `docker run` falla porque el puerto 8080 del host
ya está ocupado por el primer contenedor.

### Ejercicio 8 — Patrón de seguridad: 3 capas

```bash
# Crear las redes
docker network create fe_net
docker network create be_net

# Base de datos — NO expuesta, solo red backend
docker run -d --name db \
  --network be_net \
  -e POSTGRES_PASSWORD=secret \
  postgres:16-alpine

# API — en ambas redes, solo expuesta en localhost
docker run -d --name api \
  --network fe_net \
  -p 127.0.0.1:3000:3000 \
  alpine sleep 300
docker network connect be_net api

# Nginx — expuesto públicamente
docker run -d --name nginx \
  --network fe_net \
  -p 8080:80 \
  nginx:alpine

# Verificar aislamiento:
# nginx puede ver api
docker exec nginx ping -c 1 api
# funciona

# api puede ver db
docker exec api ping -c 1 db
# funciona

# nginx NO puede ver db
docker exec nginx ping -c 1 db 2>&1
# bad address 'db'

# db NO tiene puertos publicados — no accesible desde el host
psql -h localhost -p 5432 -U postgres 2>&1 || echo "DB no accesible desde host"

# Limpiar
docker rm -f nginx api db
docker network rm fe_net be_net
```

**Predicción**: nginx solo ve api (fe_net). api ve nginx y db (ambas redes).
db solo ve api (be_net). La DB no es accesible desde el host ni desde internet.
