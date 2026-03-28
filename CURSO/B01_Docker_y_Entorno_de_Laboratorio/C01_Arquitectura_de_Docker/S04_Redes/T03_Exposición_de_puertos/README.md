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
│  │   nginx :80 ✓      │  │       │  │   nginx :80 ✓      │  │
│  │   (solo red Docker)│  │       │  │   (mapeado)        │  │
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
| `-p 80` | host :puerto_aleatorio → contenedor :80 |

### Bind a localhost vs todas las interfaces

Esta distinción es **crítica para la seguridad**:

```bash
# TODAS las interfaces (0.0.0.0) — accesible desde la red externa
docker run -d -p 8080:80 nginx
# Equivalente a:
docker run -d -p 0.0.0.0:8080:80 nginx

# SOLO localhost — accesible solo desde el host
docker run -d -p 127.0.0.1:8080:80 nginx
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
aleatorios del host (rango efímero, típicamente 32768+):

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

## Qué pasa por debajo: iptables

Cuando usas `-p`, Docker crea reglas en **iptables** (o nftables) para hacer el
port forwarding. Esto tiene una consecuencia importante:

```bash
# Ver las reglas creadas por Docker
sudo iptables -t nat -L DOCKER -n
# DNAT  tcp  --  0.0.0.0/0  0.0.0.0/0  tcp dpt:8080 to:172.17.0.2:80
```

**Docker bypasea el firewall del host**. Las reglas `-p` se insertan en la cadena
`DOCKER` de iptables, que se evalúa **antes** que las reglas de `ufw` o `firewalld`.

```
Paquete entrante → PREROUTING → cadena DOCKER → DNAT al contenedor
                                     ↑
                               Ignora ufw/firewalld
```

Esto significa que si publicas un puerto con `-p 5432:5432`, ese puerto está
abierto **aunque `ufw` lo bloquee**. Es una fuente frecuente de sorpresas de
seguridad.

**Mitigaciones**:
- Usar `-p 127.0.0.1:puerto:puerto` para servicios internos
- Configurar `DOCKER-USER` chain en iptables para filtrar tráfico de Docker
- En `/etc/docker/daemon.json`, `"iptables": false` desactiva la manipulación
  de iptables (pero pierdes el port forwarding automático)

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
```

El proceso `docker-proxy` es el userspace proxy que Docker usa para reenviar
tráfico en ciertos casos (IPv4/IPv6, localhost). En la mayoría de los casos,
el forwarding real lo hace iptables.

---

## Ejercicios

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
ss -tlnp | grep 8080
# 0.0.0.0:8080
ss -tlnp | grep 8081
# 127.0.0.1:8081

# Limpiar
docker rm -f public_web local_web
```

### Ejercicio 3 — `-P` con puertos automáticos

```bash
# Usar -P para asignar puertos automáticamente
docker run -d -P --name auto_web nginx

# Ver qué puerto se asignó
docker port auto_web
# 80/tcp -> 0.0.0.0:XXXXX

# Extraer el puerto programáticamente
PORT=$(docker port auto_web 80 | cut -d: -f2)
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
