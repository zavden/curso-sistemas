# Bridge Networking Avanzado en Docker

## Índice

1. [Recapitulación: la red bridge por defecto](#1-recapitulación-la-red-bridge-por-defecto)
2. [Redes bridge definidas por el usuario](#2-redes-bridge-definidas-por-el-usuario)
3. [Subnets y gateways personalizados](#3-subnets-y-gateways-personalizados)
4. [Resolución DNS entre contenedores](#4-resolución-dns-entre-contenedores)
5. [Comunicación entre redes bridge](#5-comunicación-entre-redes-bridge)
6. [Aislamiento de red](#6-aislamiento-de-red)
7. [Publicación de puertos](#7-publicación-de-puertos)
8. [Inspección y diagnóstico](#8-inspección-y-diagnóstico)
9. [Redes bridge en Docker Compose](#9-redes-bridge-en-docker-compose)
10. [Internals: cómo funciona por debajo](#10-internals-cómo-funciona-por-debajo)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Recapitulación: la red bridge por defecto

Al instalar Docker, se crea automáticamente una red bridge llamada `bridge`
(interfaz `docker0` en el host). Todos los contenedores que no especifican
red se conectan a esta red.

```
Host Linux
┌──────────────────────────────────────────────────┐
│                                                  │
│    eth0 (IP pública/LAN)                         │
│      │                                           │
│      │  iptables NAT (MASQUERADE)                │
│      │                                           │
│    docker0 (172.17.0.1/16)  ← bridge por defecto │
│      │                                           │
│   ┌──┴──────────────┐                            │
│   │  veth pairs      │                           │
│   ├────┐  ├────┐     │                           │
│   │    │  │    │     │                           │
│ ┌─┴──┐ ┌─┴──┐       │                           │
│ │C1  │ │C2  │       │                           │
│ │.2  │ │.3  │       │                           │
│ └────┘ └────┘       │                           │
└──────────────────────────────────────────────────┘
```

### Limitaciones de la red bridge por defecto

| Limitación | Consecuencia |
|-----------|-------------|
| Sin DNS automático | Los contenedores no se resuelven por nombre, solo por IP |
| Subnet fija (172.17.0.0/16) | No se puede configurar el rango |
| Todos los contenedores juntos | Sin aislamiento entre grupos de servicios |
| Sin opciones avanzadas | No se puede configurar MTU, driver options, etc. |
| `--link` deprecado | La forma legacy de comunicación entre contenedores |

**Recomendación de Docker**: siempre usar redes definidas por el usuario
en lugar de la red bridge por defecto.

---

## 2. Redes bridge definidas por el usuario

### Crear una red bridge

```bash
# Red bridge simple
docker network create myapp

# Verificar
docker network ls
# NETWORK ID     NAME      DRIVER    SCOPE
# a1b2c3d4e5f6   bridge    bridge    local
# f6e5d4c3b2a1   host      host      local
# 1a2b3c4d5e6f   none      null      local
# 9z8y7x6w5v4u   myapp     bridge    local
```

### Usar la red

```bash
# Iniciar contenedor en la red personalizada
docker run -d --name web --network myapp nginx
docker run -d --name api --network myapp node:20-alpine

# Los contenedores se resuelven por nombre automáticamente
docker exec web ping -c 2 api
# PING api (172.18.0.3): 56 data bytes
# 64 bytes from 172.18.0.3: seq=0 ttl=64 time=0.087 ms

docker exec api ping -c 2 web
# PING web (172.18.0.2): 56 data bytes
# 64 bytes from 172.18.0.2: seq=0 ttl=64 time=0.095 ms
```

### Ventajas sobre la red por defecto

```
Red bridge por defecto              Red bridge definida por usuario
──────────────────────              ───────────────────────────────
Sin DNS entre contenedores    →     DNS automático por nombre
Todos juntos                  →     Aislamiento por red
Sin configurar subnet        →     Subnet personalizable
--link (deprecated)           →     No necesita --link
172.17.0.0/16 fija            →     Cualquier rango CIDR
```

---

## 3. Subnets y gateways personalizados

### Especificar subnet

```bash
# Red con subnet específico
docker network create \
    --subnet=10.10.0.0/24 \
    --gateway=10.10.0.1 \
    backend

# Red con subnet y rango de IPs asignables
docker network create \
    --subnet=10.20.0.0/24 \
    --gateway=10.20.0.1 \
    --ip-range=10.20.0.128/25 \
    frontend
# Los contenedores recibirán IPs entre 10.20.0.128 y 10.20.0.255
# Las IPs 10.20.0.2-127 quedan libres para asignación estática
```

### Asignar IP estática a un contenedor

```bash
# IP fija (debe estar dentro del subnet de la red)
docker run -d --name db \
    --network backend \
    --ip 10.10.0.100 \
    postgres:16

# Verificar
docker inspect db --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'
# 10.10.0.100
```

### Múltiples subnets (dual-stack)

```bash
# Red con IPv4 e IPv6
docker network create \
    --subnet=10.30.0.0/24 \
    --gateway=10.30.0.1 \
    --ipv6 \
    --subnet=fd00:dead:beef::/48 \
    --gateway=fd00:dead:beef::1 \
    dualstack
```

### Opciones del driver bridge

```bash
# MTU personalizado (útil en entornos con VPN o tunnels)
docker network create \
    --opt com.docker.network.bridge.name=br-myapp \
    --opt com.docker.network.driver.mtu=1400 \
    myapp

# Nombre del bridge en el host
# Sin --opt: Docker crea br-<ID_aleatorio>
# Con --opt com.docker.network.bridge.name: nombre legible

# Deshabilitar ICC (Inter-Container Communication)
docker network create \
    --opt com.docker.network.bridge.enable_icc=false \
    isolated
# Los contenedores en esta red NO pueden hablar entre sí
# Solo pueden salir a Internet y recibir tráfico publicado
```

---

## 4. Resolución DNS entre contenedores

### DNS embebido de Docker

Docker ejecuta un servidor DNS embebido (127.0.0.11) para todas las redes
definidas por el usuario. Este DNS resuelve nombres de contenedores y
aliases de red.

```bash
docker network create myapp
docker run -d --name web --network myapp nginx
docker run -d --name api --network myapp node:20-alpine

# Resolución por nombre de contenedor
docker exec web nslookup api
# Server:    127.0.0.11
# Address:   127.0.0.11:53
# Name:      api
# Address:   172.18.0.3

# También funciona como hostname
docker exec web curl -s http://api:3000/
```

### Aliases de red

```bash
# Un contenedor puede tener aliases adicionales en una red
docker run -d --name postgres-primary \
    --network myapp \
    --network-alias db \
    --network-alias database \
    postgres:16

# Ahora se resuelve por cualquiera de estos nombres:
# postgres-primary, db, database
docker exec web ping -c 1 db
docker exec web ping -c 1 database
docker exec web ping -c 1 postgres-primary
# Todos resuelven a la misma IP
```

### DNS round-robin con aliases

Si varios contenedores comparten el mismo alias, Docker distribuye las
peticiones DNS entre ellos:

```bash
# Tres réplicas del mismo servicio
docker run -d --name worker1 --network myapp --network-alias worker alpine sleep 3600
docker run -d --name worker2 --network myapp --network-alias worker alpine sleep 3600
docker run -d --name worker3 --network myapp --network-alias worker alpine sleep 3600

# El alias "worker" resuelve a diferentes IPs
docker exec web nslookup worker
# Name:      worker
# Address:   172.18.0.4
# Address:   172.18.0.5
# Address:   172.18.0.6
```

**Predicción**: este DNS round-robin es rudimentario — no verifica salud de
los contenedores. Si worker2 muere, su IP sigue en las respuestas DNS hasta
que Docker lo elimine. Para balanceo real, usa un reverse proxy (Nginx,
Traefik) o Docker Swarm.

### DNS y la red bridge por defecto

```bash
# En la red bridge por defecto NO hay DNS
docker run -d --name test1 alpine sleep 3600
docker run -d --name test2 alpine sleep 3600
docker exec test1 ping -c 1 test2
# ping: bad address 'test2'    ← no resuelve

# En red definida por usuario SÍ hay DNS
docker network create testnet
docker run -d --name test3 --network testnet alpine sleep 3600
docker run -d --name test4 --network testnet alpine sleep 3600
docker exec test3 ping -c 1 test4
# PING test4 (172.19.0.3): OK  ← resuelve ✓
```

---

## 5. Comunicación entre redes bridge

Por defecto, los contenedores en redes bridge diferentes **no pueden
comunicarse**. Cada red bridge es un dominio de broadcast aislado.

```
Red "frontend"                      Red "backend"
┌────────────────┐                 ┌────────────────┐
│  172.18.0.0/24 │                 │  172.19.0.0/24 │
│                │                 │                │
│  ┌────┐ ┌────┐│                 │┌────┐  ┌─────┐│
│  │web │ │proxy││    ✗ AISLADO    ││api │  │db   ││
│  │.2  │ │.3  ││◄───────────────▶││.2  │  │.3   ││
│  └────┘ └────┘│                 │└────┘  └─────┘│
└────────────────┘                 └────────────────┘
```

### Conectar un contenedor a múltiples redes

La solución para que un contenedor se comunique con ambas redes es
conectarlo a las dos:

```bash
# Crear dos redes
docker network create frontend --subnet=10.10.0.0/24
docker network create backend --subnet=10.20.0.0/24

# Contenedores en frontend
docker run -d --name web --network frontend nginx

# Contenedores en backend
docker run -d --name db --network backend postgres:16

# API necesita hablar con ambas redes
docker run -d --name api --network frontend node:20-alpine

# Conectar api también a backend
docker network connect backend api

# Ahora api tiene dos interfaces:
docker exec api ip addr
# eth0: 10.10.0.x  (frontend)
# eth1: 10.20.0.x  (backend)

# api puede hablar con web (frontend) Y con db (backend)
# web NO puede hablar con db (solo están en sus respectivas redes)
```

```
Red "frontend"                    Red "backend"
┌────────────────┐               ┌────────────────┐
│  10.10.0.0/24  │               │  10.20.0.0/24  │
│                │               │                │
│  ┌────┐ ┌─────┤               ├─────┐  ┌─────┐│
│  │web │ │ api ├───────────────┤ api │  │ db  ││
│  │.2  │ │ .3  │  conectado    │ .2  │  │ .3  ││
│  └────┘ └─────┤  a ambas      ├─────┘  └─────┘│
└────────────────┘               └────────────────┘
```

### Desconectar de una red

```bash
# Desconectar un contenedor de una red
docker network disconnect frontend api

# Verificar
docker inspect api --format '{{json .NetworkSettings.Networks}}' | jq .
```

---

## 6. Aislamiento de red

### Patrón de arquitectura: redes por capa

```bash
# Red pública (accesible desde fuera)
docker network create public --subnet=10.10.0.0/24

# Red interna de aplicación (no accesible desde fuera)
docker network create --internal app_internal --subnet=10.20.0.0/24

# Red de base de datos (no accesible desde fuera)
docker network create --internal db_internal --subnet=10.30.0.0/24
```

### Redes internas (--internal)

Una red `--internal` **no tiene acceso a Internet** ni al exterior. Los
contenedores solo pueden comunicarse entre sí.

```bash
# Crear red interna
docker network create --internal secure_db --subnet=10.99.0.0/24

# El contenedor no tiene salida a Internet
docker run -it --rm --network secure_db alpine ping -c 1 8.8.8.8
# ping: sendto: Network is unreachable

# Pero sí se comunica con otros contenedores en la misma red
docker run -d --name db1 --network secure_db postgres:16
docker run -d --name db2 --network secure_db postgres:16
docker exec db1 ping -c 1 db2
# OK ✓
```

**Cómo funciona**: Docker no añade la regla MASQUERADE de iptables para
redes `--internal`, por lo que el tráfico no sale del host.

### Deshabilitar ICC (Inter-Container Communication)

```bash
# Red donde los contenedores NO pueden hablar entre sí
docker network create \
    --opt com.docker.network.bridge.enable_icc=false \
    no_icc

# Cada contenedor solo puede:
# - Salir a Internet (a través de NAT)
# - Recibir tráfico en puertos publicados (-p)
# - NO puede hablar con otros contenedores en la misma red
```

### Ejemplo: arquitectura segura de 3 capas

```bash
# 1. Red pública: reverse proxy
docker network create dmz --subnet=10.10.0.0/24

# 2. Red de aplicación: entre proxy y app (interna)
docker network create --internal app_net --subnet=10.20.0.0/24

# 3. Red de base de datos: entre app y DB (interna)
docker network create --internal db_net --subnet=10.30.0.0/24

# Nginx (solo en dmz)
docker run -d --name nginx --network dmz -p 80:80 -p 443:443 nginx

# App (en dmz Y app_net)
docker run -d --name app --network dmz node:20-alpine
docker network connect app_net app

# API interna (en app_net Y db_net)
docker run -d --name api --network app_net node:20-alpine
docker network connect db_net api

# Base de datos (solo en db_net)
docker run -d --name postgres --network db_net postgres:16

# Resultado:
# Internet → nginx → app (dmz)
# app → api (app_net, interna)
# api → postgres (db_net, interna)
# Internet ✗→ api (no accesible)
# Internet ✗→ postgres (no accesible)
# nginx ✗→ postgres (no comparten red)
```

---

## 7. Publicación de puertos

### Sintaxis de -p

```bash
# hostPort:containerPort
docker run -d -p 8080:80 --name web nginx
# Accesible en host:8080 → container:80

# IP específica + hostPort:containerPort
docker run -d -p 127.0.0.1:8080:80 --name web nginx
# Solo accesible desde localhost del host

# Puerto aleatorio del host
docker run -d -p 80 --name web nginx
# Docker asigna un puerto alto aleatorio
docker port web
# 80/tcp -> 0.0.0.0:32768

# Rango de puertos
docker run -d -p 8000-8010:8000-8010 --name app myapp

# UDP
docker run -d -p 5353:53/udp --name dns dnsmasq

# TCP y UDP en el mismo puerto
docker run -d -p 5353:53/tcp -p 5353:53/udp --name dns dnsmasq
```

### ¿Qué hace Docker con -p internamente?

```bash
# Docker crea reglas iptables/nftables automáticamente:

# 1. DNAT en PREROUTING (tráfico externo)
# -A DOCKER -p tcp --dport 8080 -j DNAT --to-destination 172.17.0.2:80

# 2. Regla en FORWARD (permitir el tráfico al contenedor)
# -A DOCKER -d 172.17.0.2 -p tcp --dport 80 -j ACCEPT

# 3. MASQUERADE en POSTROUTING (para respuestas)
# -A POSTROUTING -s 172.17.0.0/16 ! -o docker0 -j MASQUERADE

# Ver las reglas Docker
sudo iptables -t nat -L DOCKER -n -v
sudo iptables -L DOCKER -n -v
```

### Publicar solo en localhost

```bash
# ✗ Publicar en todas las interfaces (accesible desde la red)
docker run -d -p 5432:5432 postgres:16
# Cualquiera en la red puede acceder a PostgreSQL

# ✓ Publicar solo en localhost
docker run -d -p 127.0.0.1:5432:5432 postgres:16
# Solo accesible desde el propio host

# Verificar
ss -tlnp 'sport == 5432'
# LISTEN  127.0.0.1:5432  → solo local ✓
```

**Predicción**: un error muy frecuente en producción es publicar puertos de
bases de datos en `0.0.0.0`. Docker bypasa el firewall del host (iptables)
con sus propias reglas de NAT, por lo que incluso con firewall activo, el
puerto queda expuesto.

---

## 8. Inspección y diagnóstico

### Listar redes

```bash
# Todas las redes
docker network ls

# Con formato personalizado
docker network ls --format "table {{.Name}}\t{{.Driver}}\t{{.Scope}}\t{{.Internal}}"
```

### Inspeccionar una red

```bash
# Detalle completo de una red
docker network inspect myapp

# Información clave que muestra:
# - Subnet y gateway
# - Contenedores conectados con sus IPs
# - Opciones del driver
# - Si es interna

# Formato específico
docker network inspect myapp --format '{{range .Containers}}{{.Name}}: {{.IPv4Address}}{{"\n"}}{{end}}'
# web: 172.18.0.2/24
# api: 172.18.0.3/24
# db:  172.18.0.4/24
```

### Inspeccionar la red de un contenedor

```bash
# Ver redes de un contenedor
docker inspect web --format '{{json .NetworkSettings.Networks}}' | jq .

# Ver IP de un contenedor
docker inspect web --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'

# Ver todas las redes a las que está conectado
docker inspect api --format '{{range $net, $config := .NetworkSettings.Networks}}{{$net}}: {{$config.IPAddress}}{{"\n"}}{{end}}'
# frontend: 10.10.0.3
# backend: 10.20.0.2
```

### Diagnóstico desde dentro del contenedor

```bash
# Entrar al contenedor para diagnosticar
docker exec -it web sh

# Herramientas de red (si están instaladas)
ping api
nslookup api
curl http://api:3000/
wget -qO- http://api:3000/

# Si el contenedor no tiene herramientas de red:
# Usar un contenedor de diagnóstico en la misma red
docker run -it --rm --network myapp nicolaka/netshoot
# netshoot incluye: ping, curl, dig, nslookup, tcpdump, ss, ip, nmap, etc.

# Desde netshoot:
dig api
curl -v http://web:80/
tcpdump -i eth0 -nn port 80
ss -tlnp
```

### Ver las interfaces bridge en el host

```bash
# Bridges creados por Docker
ip link show type bridge
# br-a1b2c3d4e5f6  (red personalizada)
# docker0           (red por defecto)

# Ver qué interfaces están conectadas al bridge
bridge link show

# Ver la tabla de forwarding del bridge
bridge fdb show br br-a1b2c3d4e5f6

# Ver interfaces veth (pares virtuales host↔contenedor)
ip link show type veth
```

---

## 9. Redes bridge en Docker Compose

Docker Compose crea automáticamente una red bridge para cada proyecto.

### Red por defecto en Compose

```yaml
# docker-compose.yml
services:
  web:
    image: nginx
    ports:
      - "80:80"
  api:
    image: node:20-alpine
  db:
    image: postgres:16
```

```bash
docker compose up -d
# Crea automáticamente la red: myproject_default
# Todos los servicios se conectan a ella
# Se resuelven por nombre de servicio: web, api, db
```

El nombre de la red es `<nombre_directorio>_default`. Los contenedores
se resuelven por el nombre del servicio (no del contenedor).

### Redes personalizadas en Compose

```yaml
# docker-compose.yml
services:
  nginx:
    image: nginx
    ports:
      - "80:80"
      - "443:443"
    networks:
      - frontend

  app:
    image: myapp:latest
    networks:
      - frontend
      - backend

  api:
    image: myapi:latest
    networks:
      - backend
      - database

  postgres:
    image: postgres:16
    environment:
      POSTGRES_PASSWORD: secret
    networks:
      - database

  redis:
    image: redis:7-alpine
    networks:
      - backend

networks:
  frontend:
    driver: bridge
    ipam:
      config:
        - subnet: 10.10.0.0/24
          gateway: 10.10.0.1

  backend:
    driver: bridge
    internal: true            # sin acceso a Internet
    ipam:
      config:
        - subnet: 10.20.0.0/24

  database:
    driver: bridge
    internal: true
    ipam:
      config:
        - subnet: 10.30.0.0/24
```

### IP estática en Compose

```yaml
services:
  db:
    image: postgres:16
    networks:
      database:
        ipv4_address: 10.30.0.100

networks:
  database:
    ipam:
      config:
        - subnet: 10.30.0.0/24
```

### Aliases en Compose

```yaml
services:
  postgres-primary:
    image: postgres:16
    networks:
      database:
        aliases:
          - db
          - database
          - postgres
```

### Red externa (preexistente)

```yaml
# Usar una red que ya existe (creada con docker network create)
services:
  app:
    image: myapp
    networks:
      - shared

networks:
  shared:
    external: true
    name: my_shared_network    # nombre real de la red existente
```

Esto permite que contenedores de diferentes proyectos Compose compartan
la misma red.

---

## 10. Internals: cómo funciona por debajo

### Componentes del stack de red

```
Contenedor A                 Contenedor B
┌──────────────┐            ┌──────────────┐
│  eth0        │            │  eth0        │
│  (veth par)  │            │  (veth par)  │
└──────┬───────┘            └──────┬───────┘
       │                           │
       │ veth-A                    │ veth-B
       │                           │
┌──────┴───────────────────────────┴───────┐
│              br-myapp                     │
│         (Linux bridge)                    │
│         10.10.0.1/24                      │
└──────────────────┬────────────────────────┘
                   │
                   │ iptables NAT
                   │ (MASQUERADE)
                   │
              ┌────┴────┐
              │  eth0    │
              │ (host)   │
              └──────────┘
```

### veth pairs

Cada contenedor tiene una interfaz `eth0` que es un extremo de un **par
virtual Ethernet (veth)**. El otro extremo está conectado al bridge en el
host.

```bash
# Ver los veth pairs en el host
ip link show type veth

# Identificar qué veth pertenece a qué contenedor
# Método 1: buscar el ifindex
docker exec web cat /sys/class/net/eth0/iflink
# 15
ip link | grep "^15:"
# 15: vethab12cd@if14: ...

# Método 2: inspeccionar el contenedor
docker inspect web --format '{{.NetworkSettings.SandboxKey}}'
# /var/run/docker/netns/abc123
nsenter --net=/var/run/docker/netns/abc123 ip addr
```

### Reglas NAT generadas

```bash
# Docker crea una cadena DOCKER en iptables
sudo iptables -t nat -L -n -v

# Cadena POSTROUTING: MASQUERADE para salida a Internet
# -A POSTROUTING -s 10.10.0.0/24 ! -o br-myapp -j MASQUERADE

# Cadena DOCKER: DNAT para puertos publicados
# -A DOCKER ! -i br-myapp -p tcp --dport 8080 -j DNAT --to 10.10.0.2:80

# Cadena DOCKER-ISOLATION: aislamiento entre bridges
# -A DOCKER-ISOLATION-STAGE-1 -i br-frontend ! -o br-frontend -j DOCKER-ISOLATION-STAGE-2
# -A DOCKER-ISOLATION-STAGE-2 -o br-backend -j DROP
```

### Docker y el firewall del host

```
⚠ IMPORTANTE: Docker inserta sus reglas ANTES que las del firewall

Orden de evaluación iptables con Docker:
  PREROUTING → DOCKER (DNAT) → FORWARD → DOCKER (allow) → POSTROUTING

Resultado: un puerto publicado con -p BYPASA el firewall del host
(iptables/nftables/firewalld) porque Docker inserta reglas con
prioridad alta.
```

```bash
# Ejemplo del problema:
# El firewall del host bloquea el puerto 5432
firewall-cmd --zone=public --list-all
# No incluye postgresql

# Pero Docker publica 5432
docker run -d -p 5432:5432 postgres:16
# ¡PostgreSQL es accesible desde la red! El firewall no lo bloquea

# Solución 1: publicar solo en localhost
docker run -d -p 127.0.0.1:5432:5432 postgres:16

# Solución 2: configurar DOCKER-USER chain
sudo iptables -I DOCKER-USER -i eth0 -p tcp --dport 5432 -j DROP
# DOCKER-USER es evaluada ANTES que DOCKER, por lo que sí filtra
```

---

## 11. Errores comunes

### Error 1: usar la red bridge por defecto en lugar de una personalizada

```bash
# ✗ Sin especificar red (usa bridge por defecto)
docker run -d --name web nginx
docker run -d --name api node:20-alpine
docker exec web ping api
# ping: bad address 'api'    ← no hay DNS

# ✓ Crear red personalizada
docker network create myapp
docker run -d --name web --network myapp nginx
docker run -d --name api --network myapp node:20-alpine
docker exec web ping api
# OK ✓
```

### Error 2: publicar puertos de bases de datos en 0.0.0.0

```bash
# ✗ Base de datos accesible desde toda la red
docker run -d -p 5432:5432 postgres:16

# ✓ Solo localhost
docker run -d -p 127.0.0.1:5432:5432 postgres:16

# ✓ O mejor: sin publicar, solo accesible por red Docker interna
docker run -d --name db --network backend postgres:16
# La app se conecta a db:5432 por la red Docker
```

### Error 3: contenedores en redes diferentes que no se comunican

```bash
# ✗ "api no puede conectar a db"
docker network create frontend
docker network create backend
docker run -d --name api --network frontend node:20-alpine
docker run -d --name db --network backend postgres:16
docker exec api ping db
# ping: bad address 'db'    ← están en redes diferentes

# ✓ Conectar api a ambas redes
docker network connect backend api
docker exec api ping db
# OK ✓
```

### Error 4: confundir nombre de contenedor con nombre de servicio en Compose

```bash
# En docker-compose.yml:
# services:
#   web-server:
#     image: nginx
#     container_name: my_nginx

# ✗ Usar container_name para DNS
docker exec api curl http://my_nginx/
# Puede no resolver

# ✓ Usar el nombre del servicio
docker exec api curl http://web-server/
# OK — Compose registra el nombre del servicio en DNS
```

### Error 5: asumir que el firewall del host protege puertos publicados por Docker

```bash
# ✗ "El firewall bloquea el puerto 3306, estamos seguros"
sudo ufw deny 3306
docker run -d -p 3306:3306 mysql:8
# ¡MySQL accesible desde la red! Docker bypasa ufw/iptables

# ✓ Usar DOCKER-USER chain o publicar en 127.0.0.1
sudo iptables -I DOCKER-USER -i eth0 -p tcp --dport 3306 -j DROP
# o
docker run -d -p 127.0.0.1:3306:3306 mysql:8
```

---

## 12. Cheatsheet

```bash
# ╔══════════════════════════════════════════════════════════════════╗
# ║           DOCKER BRIDGE NETWORKING — CHEATSHEET                ║
# ╠══════════════════════════════════════════════════════════════════╣
# ║                                                                ║
# ║  REDES:                                                       ║
# ║  docker network create myapp                                   ║
# ║  docker network create --subnet=10.10.0.0/24 myapp            ║
# ║  docker network create --internal db_net                       ║
# ║  docker network ls                                             ║
# ║  docker network inspect myapp                                  ║
# ║  docker network rm myapp                                       ║
# ║  docker network prune                    # eliminar sin usar   ║
# ║                                                                ║
# ║  CONECTAR CONTENEDORES:                                       ║
# ║  docker run -d --network myapp --name web nginx                ║
# ║  docker run -d --network myapp --ip 10.10.0.100 --name db pg  ║
# ║  docker network connect backend api                            ║
# ║  docker network disconnect frontend api                        ║
# ║                                                                ║
# ║  DNS:                                                         ║
# ║  - Automático en redes user-defined (no en bridge default)     ║
# ║  - Resolver por nombre de contenedor o alias                   ║
# ║  --network-alias db       # alias adicional                   ║
# ║                                                                ║
# ║  PUBLICAR PUERTOS:                                            ║
# ║  -p 8080:80               # host:container                    ║
# ║  -p 127.0.0.1:8080:80     # solo localhost                    ║
# ║  -p 80                    # puerto aleatorio del host         ║
# ║  -p 5353:53/udp           # UDP                               ║
# ║                                                                ║
# ║  DIAGNÓSTICO:                                                 ║
# ║  docker network inspect myapp                                  ║
# ║  docker inspect web --format '..Networks..'                    ║
# ║  docker run --rm --network myapp nicolaka/netshoot             ║
# ║  ip link show type bridge                                      ║
# ║  sudo iptables -t nat -L DOCKER -n -v                         ║
# ║                                                                ║
# ║  SEGURIDAD:                                                   ║
# ║  --internal              # sin acceso a Internet               ║
# ║  --opt enable_icc=false  # sin comunicación entre containers   ║
# ║  -p 127.0.0.1:P:P        # publicar solo en localhost          ║
# ║  DOCKER-USER chain       # filtrar tráfico publicado           ║
# ║                                                                ║
# ╚══════════════════════════════════════════════════════════════════╝
```

---

## 13. Ejercicios

### Ejercicio 1: arquitectura de red multi-capa

**Contexto**: necesitas desplegar una aplicación web con esta arquitectura:
- Nginx (reverse proxy): accesible desde Internet
- App Node.js: accesible solo desde Nginx
- PostgreSQL: accesible solo desde App
- Redis: accesible solo desde App

**Tareas**:

1. Diseña las redes necesarias (¿cuántas? ¿cuáles internas?)
2. Crea las redes con subnets personalizados
3. Lanza cada contenedor en las redes correctas
4. Conecta los contenedores que necesitan estar en múltiples redes
5. Verifica el aislamiento:
   - Nginx puede alcanzar App ✓
   - Nginx NO puede alcanzar PostgreSQL ✗
   - Nginx NO puede alcanzar Redis ✗
   - App puede alcanzar PostgreSQL ✓
   - App puede alcanzar Redis ✓
   - PostgreSQL NO tiene acceso a Internet ✗
6. Publica solo el puerto 80 y 443 de Nginx

**Pistas**:
- Necesitas al menos 2 redes (frontend, backend)
- Las redes de backend deben ser `--internal`
- App debe conectarse a ambas redes
- Usa `nicolaka/netshoot` para verificar aislamiento

> **Pregunta de reflexión**: ¿por qué es importante que PostgreSQL esté en
> una red `--internal`? Si un atacante compromete Nginx, ¿puede llegar a
> la base de datos en esta arquitectura? ¿Y si compromete App?

---

### Ejercicio 2: DNS y service discovery

**Contexto**: necesitas implementar un patrón de service discovery básico
usando el DNS de Docker.

**Tareas**:

1. Crea una red `services`
2. Lanza 3 contenedores con el alias `worker` en la red
3. Lanza un contenedor `dispatcher` en la misma red
4. Desde `dispatcher`, resuelve `worker` con `nslookup` o `dig` y
   verifica que devuelve las 3 IPs
5. Detén uno de los workers y verifica que DNS se actualiza
6. Lanza un worker adicional y verifica que aparece en DNS
7. Implementa el mismo patrón en un archivo docker-compose.yml usando
   `deploy.replicas: 3` (o 3 servicios con el mismo alias)

**Pistas**:
- `--network-alias worker` para compartir el alias
- `dig worker` desde netshoot muestra todas las IPs
- Docker actualiza DNS cuando contenedores se crean/destruyen
- En Compose, el nombre del servicio ya actúa como DNS compartido

> **Pregunta de reflexión**: el DNS round-robin de Docker no verifica la
> salud de los contenedores. ¿Qué pasa si un worker se cuelga (proceso
> vivo pero no responde)? ¿Cómo solucionarías esto con un reverse proxy
> (Nginx upstream con health checks, o Traefik)?

---

### Ejercicio 3: auditoría de seguridad de red Docker

**Contexto**: heredas un servidor con contenedores Docker y necesitas
auditar la configuración de red.

**Tareas**:

1. Lista todas las redes Docker y sus propiedades
2. Para cada red, identifica:
   - ¿Es la red por defecto o personalizada?
   - ¿Es interna o tiene acceso a Internet?
   - ¿Qué contenedores están conectados?
3. Lista todos los puertos publicados:
   ```bash
   docker ps --format "{{.Names}}: {{.Ports}}"
   ```
4. Identifica problemas de seguridad:
   - ¿Hay puertos de bases de datos publicados en 0.0.0.0?
   - ¿Hay contenedores en la red bridge por defecto?
   - ¿Falta aislamiento entre capas (web, app, db en la misma red)?
5. Verifica si Docker bypasa el firewall:
   ```bash
   sudo iptables -t nat -L DOCKER -n -v
   ```
6. Propón y aplica correcciones

**Pistas**:
- `docker network ls` + `docker network inspect` para cada red
- `docker ps --format` para puertos publicados
- Buscar `0.0.0.0:3306`, `0.0.0.0:5432`, `0.0.0.0:6379`, `0.0.0.0:27017`
- Verificar si existen reglas en la cadena DOCKER-USER

> **Pregunta de reflexión**: Docker modifica iptables directamente para
> publicar puertos, lo que bypasa firewalls como ufw y firewalld. ¿Es esto
> un error de diseño o una característica? ¿Cómo debería un administrador
> gestionar la coexistencia de Docker y el firewall del host?

---

> **Siguiente tema**: T02 — Host networking (cuándo usar, implicaciones de seguridad)
