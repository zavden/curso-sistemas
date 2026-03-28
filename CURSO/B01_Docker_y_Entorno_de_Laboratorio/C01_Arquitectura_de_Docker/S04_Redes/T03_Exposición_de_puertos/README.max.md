# T03 — Exposición de puertos

## Puertos dentro vs fuera del contenedor

Un contenedor tiene su propia **interfaz de red y su propio espacio de puertos**.
Un servidor web escuchando en el puerto 80 dentro del contenedor no es accesible
desde el host a menos que se establezca un **mapeo de puertos** explícito.

```
                  Sin -p                              Con -p 8080:80
┌────────────────────────────────┐    ┌────────────────────────────────┐
│              HOST               │    │              HOST               │
│                                │    │                                │
│   curl localhost:80            │    │   curl localhost:8080           │
│   → Connection refused ✗       │    │   → ¡Responde! ✓            │
│                                │    │            │                  │
│   ┌────────────────────────┐  │    │   ┌────────┼────────────────┐ │
│   │      CONTENEDOR        │  │    │   │    CONTENEDOR          │ │
│   │                        │  │    │   │                        │ │
│   │   nginx :80 ✓           │  │    │   │   nginx :80 ✓          │ │
│   │   (solo red Docker)    │  │    │   │   (mapeado via NAT)   │ │
│   └────────────────────────┘  │    │   └────────────────────────┘ │
└────────────────────────────────┘    └────────────────────────────────┘
```

Sin port mapping, el puerto 80 del contenedor **solo es accesible desde la red
Docker** (otros contenedores en la misma red pueden conectarse directamente).
El host no puede acceder a menos que use `-p`.

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
| `-p 192.168.1.10:8080:80` | solo esa IP específica del host → contenedor :80 |
| `-p 8080:80/udp` | puerto UDP |
| `-p 8080:80/tcp -p 8080:80/udp` | TCP y UDP en el mismo puerto |
| `-p 8080-8090:80-90` | rango de puertos (10 puertos mapeados) |
| `-p 80` | host :puerto_aleatorio → contenedor :80 (rango efímero) |
| `-p :::80` | bind a todas las interfaces IPv6 |
| `-p 80-80:80-80` | misma notación para rangos |

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

# En un servidor con IP pública, curl desde OTRO host NO puede acceder
# curl http://servidor:8080  # No funciona
# ssh usuario@servidor; curl localhost:8080  # Sí funciona
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

# API interna solo accesible desde localhost
docker run -d -p 127.0.0.1:3000:3000 myapi
```

## `-P` — Publicar todos los puertos EXPOSE

`-P` (mayúscula) mapea **todos los puertos declarados con EXPOSE** a puertos
aleatorios del host (rango efímero, típicamente 32768-60999):

```bash
docker run -d -P --name auto_port nginx

# Ver los puertos asignados
docker port auto_port
# 80/tcp -> 0.0.0.0:32768
# 443/tcp -> 0.0.0.0:32769

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

docker port web 443
# 0.0.0.0:8443

# Ver en formato JSON
docker inspect --format '{{json .NetworkSettings.Ports}}' web | jq '.'

docker rm -f web
```

## Rangos de puertos

```bash
# Mapear un rango连续 de puertos
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
sudo iptables -t nat -L DOCKER -n -v

# DNAT para port forwarding
# DNAT  tcp  --  *  0.0.0.0/0  0.0.0.0/0  tcp dpt:8080 to:172.17.0.2:80
```

**Docker bypassea el firewall del host**. Las reglas `-p` se insertan en la cadena
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

Puedes agregar tus propias reglas en la cadena `DOCKER-USER` para filtrar antes
de que Docker procese el tráfico:

```bash
# Permitir solo desde localhost
sudo iptables -I DOCKER-USER -p tcp --dport 8080 -s 127.0.0.1 -j ACCEPT
sudo iptables -I DOCKER-USER -p tcp --dport 8080 -j DROP

# Ahora aunque -p 8080:80 exponga el puerto, solo localhost puede acceder
```

### Desactivar manipulación de iptables por Docker

En `/etc/docker/daemon.json`:

```json
{
  "iptables": false
}
```

Esto desactiva la manipulación de iptables por Docker — pierdes el port
forwarding automático, y debes configurar las reglas manualmente.

### El proceso docker-proxy

En algunos casos (IPv6, ciertos configuraciones), Docker usa un proxy en
userspace llamado `docker-proxy`:

```bash
# Ver procesos docker-proxy
ps aux | grep docker-proxy

# Ver qué procesos escuchan en un puerto
ss -tlnp | grep :8080
# LISTEN 0  4096  0.0.0.0:8080  0.0.0.0:*  users:(("docker-proxy",...))
```

El `docker-proxy` es un programa que reenvía tráfico desde el puerto del host
al contenedor. En la mayoría de los casos modernos, el forwarding lo hace
iptables directamente sin docker-proxy.

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
│   │  └─ :80, :443 EXPOSTADO con -p                  │
│   │                                                    │
│   │  /api → :3000 (NO expuesto, solo red interna)   │
│   │                                                    │
│   └─ api (contenedor)                               │
│         │                                              │
│         │ → :5432 (NO expuesto, solo red interna)    │
│         │                                              │
│         └─ db (contenedor)                          │
│                                                      │
│   iptables: -p 80:80, -p 443:443                  │
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
# En el host
ss -tlnp | grep :8080
# LISTEN  0  4096  0.0.0.0:8080  0.0.0.0:*  users:(("docker-proxy",...))

# Con netstat (alternativa)
netstat -tlnp | grep :8080

# Encontrar el PID del proceso
lsof -i :8080
```

## Verificar conectividad

```bash
# Ver si el puerto está escuchando
ss -tlnp | grep :80

# Probar conectividad local
curl -v http://localhost:8080

# Probar desde una IP específica
curl -v http://192.168.1.10:8080

# Ver logs de nginx para confirmar que llegan requests
docker logs web

# Ver conexiones activas
docker exec web ss -tlnp
```

## Casos de uso típicos

| Servicio | Port mapping | Razón |
|----------|---------------|--------|
| Nginx/Apache | `-p 80:80 -p 443:443` | Debe ser accesible desde internet |
| API REST interna | Sin `-p` o `-p 127.0.0.1:3000:3000` | Solo accesible internamente |
| Base de datos | Sin `-p` o `-p 127.0.0.1:5432:5432` | Solo accesible desde la red Docker |
| Redis | Sin `-p` o `-p 127.0.0.1:6379:6379` | Solo accesible internamente |
| Panel admin web | `-p 127.0.0.1:8080:8080` | Solo acceso local |
| Webhook endpoint | `-p 80:80` | Debe ser accesible desde internet |

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
ss -tlnp | grep -E "8080|8081"
# 0.0.0.0:8080     ← todas las interfaces
# 127.0.0.1:8081   ← solo localhost

# Limpiar
docker rm -f public_web local_web
```

### Ejercicio 3 — `-P` con puertos automáticos

```bash
# Usar -P para asignar puertos automáticamente
docker run -d -P --name auto_web nginx

# Ver qué puerto se asignó
docker port auto_web

# Extraer el puerto programáticamente
PORT=$(docker port auto_web 80 | rev | cut -d: -f1 | rev)
echo "Puerto asignado: $PORT"
curl -s http://localhost:$PORT | head -3

# Limpiar
docker rm -f auto_web
```

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

### Ejercicio 5 — Rangos de puertos

```bash
# Mapear rango de puertos
docker run -d -p 8000-8002:80-82 --name range_test nginx

# Ver los mapeos
docker port range_test

# Probar cada uno
curl http://localhost:8000 | head -1
curl http://localhost:8001 | head -1
curl http://localhost:8002 | head -1

# Limpiar
docker rm -f range_test
```

### Ejercicio 6 — Ver reglas iptables

```bash
# Crear un contenedor con puerto publicado
docker run -d -p 8080:80 --name iptables_test nginx

# Ver reglas NAT de Docker
sudo iptables -t nat -L DOCKER -n -v

# Ver reglas en la cadena FORWARD
sudo iptables -L FORWARD -n -v | grep DOCKER

# Ver la cadena DOCKER-USER (vacía por defecto)
sudo iptables -L DOCKER-USER -n

# Agregar una regla de filtro (ejemplo)
sudo iptables -I DOCKER-USER -p tcp --dport 8080 -s 127.0.0.1 -j ACCEPT
sudo iptables -L DOCKER-USER -n

# Limpiar
docker rm -f iptables_test

# Las reglas DOCKER-USER persisten después de eliminar el contenedor
sudo iptables -L DOCKER-USER -n  # Still shows the rule

# Eliminar la regla manual
sudo iptables -F DOCKER-USER
```

### Ejercicio 7 — Conflictos de puertos

```bash
# Primer contenedor
docker run -d -p 8080:80 --name conflict1 nginx

# Segundo contenedor en el mismo puerto
docker run -d -p 8080:80 --name conflict2 nginx 2>&1
# Error: bind: address already in use

# Solución 1: puerto diferente
docker run -d -p 8081:80 --name conflict2 nginx

# Ver qué está usando el puerto 8080
ss -tlnp | grep :8080

# Solución 2: matar el proceso docker-proxy
docker rm -f conflict1

# Ahora el puerto está libre
docker rm -f conflict2
```

### Ejercicio 8 — Patrón de seguridad

```bash
# Arquitectura de 3 capas
docker network create -d bridge frontend
docker network create -d bridge backend

# Base de datos — NO expuesta, solo red backend
docker run -d --name db \
  --network backend \
  -e POSTGRES_PASSWORD=secret \
  postgres:16

# API — en ambas redes, solo expuesta en localhost
docker run -d --name api \
  --network frontend \
  --network backend \
  -p 127.0.0.1:3000:3000 \
  alpine sleep 300

# Nginx — expuesto públicamente
docker run -d --name nginx \
  --network frontend \
  -p 80:80 \
  nginx:alpine

# Verificar aislamientos
# 1. Desde internet (asumiendo IP pública), nginx es accesible
# 2. nginx puede ver api
docker exec nginx ping -c 1 api  # ✓
# 3. api puede ver db
docker exec api ping -c 1 db    # ✓
# 4. db NO es accesible desde nginx
# ( db no tiene -p, y no está en la red frontend)
# 5. api solo accesible desde localhost
# ( curl desde otra máquina a :3000 no funciona)

# Limpiar
docker rm -f nginx api db
docker network rm frontend backend
```
