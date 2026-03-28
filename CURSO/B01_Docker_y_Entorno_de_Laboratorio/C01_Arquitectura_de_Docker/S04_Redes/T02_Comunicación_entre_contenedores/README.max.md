# T02 — Comunicación entre contenedores

## DNS interno de Docker

En redes bridge custom, Docker ejecuta un **servidor DNS embebido** (accesible en
`127.0.0.11`) que resuelve nombres de contenedores a sus direcciones IP. El nombre
que se registra en DNS es el que se asigna con `--name`.

```
┌─────────────────────────────────────────────────────────────┐
│                    Red custom: app_net                       │
│                                                               │
│   DNS embebido (127.0.0.11)                                 │
│     api   → 10.0.1.2                                        │
│     db    → 10.0.1.3                                        │
│     cache → 10.0.1.4                                        │
│                                                               │
│   ┌─────────┐     ┌─────────┐     ┌─────────┐              │
│   │   api   │────▶│   db    │     │  cache  │              │
│   │ 10.0.1.2│     │ 10.0.1.3│◀────│ 10.0.1.4│              │
│   └─────────┘     └─────────┘     └─────────┘              │
│       ping db → 10.0.1.3 ✓                                  │
│       ping cache → 10.0.1.4 ✓                               │
└─────────────────────────────────────────────────────────────┘
```

```bash
# Crear red y contenedores
docker network create app_net
docker run -d --name db --network app_net postgres:16-alpine \
  -e POSTGRES_PASSWORD=secret
docker run -d --name api --network app_net alpine sleep 3600

# Desde api, resolver el nombre "db"
docker exec api nslookup db
# Server:    127.0.0.11
# Name:      db
# Address 1: 10.0.1.3 db.app_net

# Conectar por nombre
docker exec api ping -c 2 db
# PING db (10.0.1.3): 56 data bytes — funciona
```

### Cómo funciona internamente

Docker injecta un servidor DNS en cada contenedor a través de `/etc/resolv.conf`:

```bash
# Dentro de un contenedor en red custom
cat /etc/resolv.conf
# nameserver 127.0.0.11
# options ndots:0

# 127.0.0.11 es un servidor DNS que:
# 1. Responde a queries de nombres de contenedores en esta red
# 2. Reenvía todo lo demás al DNS del host (/etc/resolv.conf del host)
```

### Por qué la red por defecto no tiene DNS?

La red bridge por defecto (`docker0`) es un remanente de las primeras versiones
de Docker. No tiene DNS integrado por razones de compatibilidad histórica. En la
red por defecto, los contenedores solo pueden comunicarse por IP:

```bash
# Red por defecto — sin DNS
docker run -d --name no_dns_1 alpine sleep 3600
docker run -d --name no_dns_2 alpine sleep 3600

docker exec no_dns_1 ping -c 1 no_dns_2
# ping: bad address 'no_dns_2'  ← falla

# Hay que obtener la IP manualmente
docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' no_dns_2
# 172.17.0.3

docker exec no_dns_1 ping -c 1 172.17.0.3
# PING 172.17.0.3: 64 bytes from 172.17.0.3  ← funciona por IP

docker rm -f no_dns_1 no_dns_2
```

**Regla práctica**: nunca uses la red por defecto para conectar contenedores.
Siempre crea una red custom.

## Aliases de red

Un contenedor puede tener **alias adicionales** en una red, además de su nombre:

```bash
docker network create mynet

# --network-alias permite definir nombres alternativos
docker run -d --name postgres-primary \
  --network mynet \
  --network-alias db \
  --network-alias database \
  postgres:16-alpine -e POSTGRES_PASSWORD=secret

# Cualquiera de los tres nombres resuelve al mismo contenedor
docker run --rm --network mynet alpine nslookup db
docker run --rm --network mynet alpine nslookup database
docker run --rm --network mynet alpine nslookup postgres-primary
# Los tres apuntan a la misma IP
```

Los aliases son útiles cuando:
- Varios contenedores pueden cumplir el mismo rol (load balancing DNS round-robin)
- Quieres nombres genéricos (`db`) junto con nombres específicos (`postgres-primary`)
- Tienes múltiples réplicas de un servicio que responden al mismo nombre

### DNS round-robin con aliases

Si múltiples contenedores comparten el mismo alias, Docker distribuye las consultas
DNS entre ellos (no es balanceo de carga real):

```bash
docker network create lb_net

# Tres servidores web con el mismo alias
docker run -d --name web1 --network lb_net --network-alias web alpine sleep 3600
docker run -d --name web2 --network lb_net --network-alias web alpine sleep 3600
docker run -d --name web3 --network lb_net --network-alias web alpine sleep 3600

# Consultar el alias "web" devuelve las tres IPs
docker run --rm --network lb_net alpine nslookup web
# Name:      web
# Address 1: 10.0.0.2
# Address 2: 10.0.0.3
# Address 3: 10.0.0.4

# Cada query nslookup puede devolverlas en orden diferente
# (el orden es round-robin, pero clientes pueden cachear)

docker rm -f web1 web2 web3
docker network rm lb_net
```

**Limitaciones del DNS round-robin**:
- No es balanceo de carga real (sin health checks)
- Los clientes pueden cachear DNS y no probar otros IPs
- No hay persistencia de sesión
- Para producción, usa un balanceador de carga real (nginx, haproxy, Traefik)

## Links (legacy — obsoleto)

Antes de las redes custom, Docker usaba `--link` para conectar contenedores:

```bash
# NO USAR — solo para referencia histórica
docker run -d --name old_db postgres:16-alpine
docker run -d --name old_app --link old_db:db alpine sleep 3600
docker exec old_app ping -c 1 db  # Funciona, pero...
```

`--link` está **deprecated** desde Docker 1.9 (2015). Problemas:

| Problema | Descripción |
|----------|-------------|
| Solo red por defecto | No funciona con redes custom |
| Unidireccional | El contenedor destino no puede resolver al origen |
| Complexidad | No soporta redes multi-contenedor complejas |
| Implementación | Modifica `/etc/hosts` en lugar de DNS (no escala) |
| Seguridad | Expone nombres entre contenedores sin control |

Si encuentras `--link` en documentación o scripts antiguos, reemplázalo por redes
custom.

## Conectar un contenedor a múltiples redes

Un contenedor puede estar en **varias redes simultáneamente**, actuando como
puente entre dominios de red aislados:

```bash
# Crear dos redes aisladas
docker network create frontend
docker network create backend

# El contenedor API está en ambas redes
docker run -d --name api --network frontend alpine sleep 3600
docker network connect backend api

# Web solo está en frontend
docker run -d --name web --network frontend alpine sleep 3600

# DB solo está en backend
docker run -d --name db --network backend alpine sleep 3600
```

```
┌── frontend ──────────────┐
│                            │
│  web ◀────────▶ api ─────┼──┐
│                            │  │
└────────────────────────────┘  │
                               │
┌── backend ──────────────────┤
│                              │
│  db ◀───────────── api ────┘
│                              │
└──────────────────────────────┘
```

```bash
# web puede hablar con api
docker exec web ping -c 1 api    # ✓

# api puede hablar con db
docker exec api ping -c 1 db     # ✓

# web NO puede hablar con db (redes diferentes, sin ruta)
docker exec web ping -c 1 db     # ✗ bad address 'db'
```

Este patrón es fundamental para **segmentar redes en producción**: el frontend
solo ve la API, la API ve el frontend y la base de datos, la base de datos solo
ve la API.

### Patrón de seguridad de red

```
Internet
    │
    ▼
┌─────────┐
│  nginx  │ (proxy inverso)
└────┬────┘
     │ 443
┌────▼────┐
│   api    │ (API server)
└────┬────┘
     │ TCP
┌────▼────┐
│   db     │ (base de datos — solo acepta de api)
└─────────┘

frontend_net: nginx ↔ api
backend_net: api ↔ db (api puede ver db, db no puede ver nginx ni api desde fuera)
```

## Conectar y desconectar en caliente

No es necesario detener un contenedor para modificar sus conexiones de red:

```bash
# Conectar un contenedor existente a una red
docker network connect mynet mycontainer

# Desconectar
docker network disconnect mynet mycontainer

# Verificar las redes de un contenedor
docker inspect -f '{{range $net, $config := .NetworkSettings.Networks}}{{$net}} {{end}}' mycontainer

# Ver IPs en cada red
docker inspect -f '{{json .NetworkSettings.Networks}}' mycontainer | jq 'keys'
```

## Actualización DNS al reiniciar

Cuando un contenedor se reinicia, puede recibir una **IP diferente**. El DNS
interno de Docker actualiza automáticamente la entrada:

```bash
docker network create test_dns
docker run -d --name svc --network test_dns alpine sleep 3600

# Ver IP actual
docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' svc
# 10.0.0.2

# Reiniciar
docker restart svc

# La IP puede cambiar, pero el nombre sigue resolviendo
docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' svc
# 10.0.0.2 (o puede ser diferente si el rango se agotó)

# Otros contenedores siguen pudiendo resolver "svc" sin importar la IP
docker run --rm --network test_dns alpine ping -c 1 svc
# Funciona independientemente de la IP asignada

docker rm -f svc
docker network rm test_dns
```

Por esto es importante **nunca hardcodear IPs** de contenedores en la configuración
de tu aplicación. Siempre usa nombres de contenedor o aliases DNS.

## /etc/hosts en contenedores

Cada contenedor tiene un `/etc/hosts` que Docker mantiene automáticamente:

```bash
docker network create hosts_test
docker run -d --name svc1 --network hosts_test alpine sleep 3600

docker run --rm --network hosts_test alpine cat /etc/hosts
# 127.0.0.1       localhost
# ::1     localhost ip6-localhost ip6-loopback
# 10.0.0.2        svc1

# Los nombres de la red también se añaden
docker run -d --name svc2 --network hosts_test alpine sleep 3600
docker run --rm --network hosts_test alpine cat /etc/hosts
# 127.0.0.1       localhost
# ::1     localhost ip6-localhost ip6-loopback
# 10.0.0.2        svc1
# 10.0.0.3        svc2 svc2.hosts_test

docker rm -f svc1 svc2
docker network rm hosts_test
```

Docker añade entradas para:
- El propio nombre del contenedor
- `hostname.networkname` como alias adicional
- Cada `--network-alias` especificado

## Docker Compose y las redes

Docker Compose crea automáticamente una red para los servicios:

```yaml
# docker-compose.yml
version: "3.8"
services:
  api:
    image: nginx
  db:
    image: postgres
```

```bash
# docker-compose up crea automáticamente:
# 1. Una red llamada "proyecto_default"
# 2. Los servicios conectados a esa red
# 3. DNS resolution por nombre de servicio
# api puede hacer ping a db directamente
```

Para comunicar servicios entre compose files, usa `external_networks`.

## Práctica

### Ejercicio 1 — Red por defecto vs custom

```bash
# 1. Red por defecto: sin DNS
docker run -d --name default1 alpine sleep 3600
docker run -d --name default2 alpine sleep 3600
docker exec default1 ping -c 1 default2 2>&1 || echo "Falla: sin DNS en red por defecto"
docker rm -f default1 default2

# 2. Red custom: con DNS
docker network create dns_test
docker run -d --name custom1 --network dns_test alpine sleep 3600
docker run -d --name custom2 --network dns_test alpine sleep 3600
docker exec custom1 ping -c 2 custom2
# Éxito: el DNS resuelve "custom2"

# Ver el DNS
docker exec custom1 cat /etc/resolv.conf

# Limpiar
docker rm -f custom1 custom2
docker network rm dns_test
```

### Ejercicio 2 — Contenedor en múltiples redes

```bash
docker network create net_a
docker network create net_b

# Contenedor en ambas redes
docker run -d --name router --network net_a alpine sleep 3600
docker network connect net_b router

# Contenedores aislados
docker run -d --name in_a --network net_a alpine sleep 3600
docker run -d --name in_b --network net_b alpine sleep 3600

# router puede hablar con ambos
docker exec router ping -c 1 in_a   # ✓
docker exec router ping -c 1 in_b   # ✓

# in_a y in_b no se ven
docker exec in_a ping -c 1 in_b 2>&1 || echo "Aislados: redes diferentes"

# Verificar redes del router
docker inspect -f '{{range $net, $_ := .NetworkSettings.Networks}}{{$net}} {{end}}' router
# net_a net_b

# Limpiar
docker rm -f router in_a in_b
docker network rm net_a net_b
```

### Ejercicio 3 — DNS round-robin

```bash
docker network create rr_net

# Tres contenedores con el mismo alias
for i in 1 2 3; do
  docker run -d --name worker$i --network rr_net --network-alias worker alpine sleep 3600
done

# Consultar "worker" muestra múltiples IPs
docker run --rm --network rr_net alpine nslookup worker

# Hacer múltiples queries para ver round-robin
for i in 1 2 3; do
  docker run --rm --network rr_net alpine nslookup worker | grep Address | tail -1
done

# Limpiar
docker rm -f worker1 worker2 worker3
docker network rm rr_net
```

### Ejercicio 4 — /etc/hosts

```bash
docker network create hosts_test

# Crear dos servicios
docker run -d --name api --network hosts_test alpine sleep 3600
docker run -d --name db --network hosts_test --network-alias database --network-alias postgres --network hosts_test alpine sleep 3600

# Ver /etc/hosts del contenedor api
docker exec api cat /etc/hosts
# 127.0.0.1       localhost
# ::1     localhost ip6-localhost ip6-loopback
# 10.0.0.2        api hosts_test
# 10.0.0.3        db db.hosts_test database postgres

# Probar que todas las formas de llamar a db funcionan
docker exec api ping -c 1 db
docker exec api ping -c 1 database
docker exec api ping -c 1 postgres

# Limpiar
docker rm -f api db
docker network rm hosts_test
```

### Ejercicio 5 — Reconexión DNS después de restart

```bash
docker network create dns_refresh
docker run -d --name svc --network dns_refresh alpine sleep 3600

# Ver IP actual
docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' svc

# Guardar la IP
IP_BEFORE=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' svc)

# Reiniciar (puede cambiar de IP)
docker restart svc

# Ver nueva IP
IP_AFTER=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' svc)

echo "IP before: $IP_BEFORE, IP after: $IP_AFTER"

# El nombre sigue funcionando
docker run --rm --network dns_refresh alpine ping -c 1 svc

# Limpiar
docker rm -f svc
docker network rm dns_refresh
```

### Ejercicio 6 — Patrón de arquitectura de 3 capas

```bash
# Crear las redes
docker network create public  # frontend
docker network create private # backend

# Servicio web en red pública
docker run -d --name nginx --network public nginx:alpine

# API en ambas redes
docker run -d --name api --network public --network private alpine sleep 3600

# DB solo en red privada
docker run -d --name db --network private --network-alias database alpine sleep 3600

# Verificar el aislamiento
# nginx puede ver api
docker exec nginx ping -c 1 api   # ✓

# nginx NO puede ver db
docker exec nginx ping -c 1 db 2>&1
# bad address 'db'

# api puede ver db
docker exec api ping -c 1 db     # ✓
docker exec api ping -c 1 database  # ✓

# db NO puede ver nginx
docker exec db ping -c 1 nginx 2>&1
# bad address 'nginx'

# Limpiar
docker rm -f nginx api db
docker network rm public private
```

### Ejercicio 7 — Conectar en caliente

```bash
docker network create net1
docker network create net2

# Crear contenedor
docker run -d --name mobile --network net1 alpine sleep 3600

# Ver sus redes
docker inspect -f '{{range $net, $_ := .NetworkSettings.Networks}}{{$net}} {{end}}' mobile
# net1

# Conectar a net2 sin detenerlo
docker network connect net2 mobile

# Verificar
docker inspect -f '{{range $net, $_ := .NetworkSettings.Networks}}{{$net}} {{end}}' mobile
# net1 net2

# Desconectar
docker network disconnect net2 mobile

# Verificar
docker inspect -f '{{range $net, $_ := .NetworkSettings.Networks}}{{$net}} {{end}}' mobile
# net1

# Limpiar
docker rm -f mobile
docker network rm net1 net2
```
