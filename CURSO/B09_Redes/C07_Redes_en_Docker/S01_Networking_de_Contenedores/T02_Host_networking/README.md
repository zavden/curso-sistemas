# Host Networking en Docker

## Índice

1. [Qué es host networking](#1-qué-es-host-networking)
2. [Cómo funciona](#2-cómo-funciona)
3. [Cuándo usar host networking](#3-cuándo-usar-host-networking)
4. [Uso básico](#4-uso-básico)
5. [Comparación con bridge](#5-comparación-con-bridge)
6. [Implicaciones de seguridad](#6-implicaciones-de-seguridad)
7. [Host networking en Docker Compose](#7-host-networking-en-docker-compose)
8. [Otros modos de red: none y macvlan](#8-otros-modos-de-red-none-y-macvlan)
9. [Escenarios prácticos](#9-escenarios-prácticos)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué es host networking

Con `--network host`, el contenedor **comparte directamente el stack de red
del host**. No hay bridge, no hay veth pairs, no hay NAT, no hay network
namespace separado. El contenedor ve las mismas interfaces, IPs y puertos
que el host.

```
Bridge networking (default):                Host networking:

Host                                        Host
┌──────────────────────────┐               ┌──────────────────────────┐
│  eth0 (192.168.1.10)     │               │  eth0 (192.168.1.10)     │
│    │                     │               │    │                     │
│    │ iptables NAT        │               │    │ (sin NAT)           │
│    │                     │               │    │                     │
│  docker0 (172.17.0.1)    │               │  El contenedor usa       │
│    │                     │               │  directamente eth0       │
│  ┌─┴────┐                │               │                          │
│  │ C1   │                │               │  ┌──────────────────┐    │
│  │172.17 │                │               │  │ C1               │    │
│  │.0.2  │                │               │  │ (mismo stack     │    │
│  └──────┘                │               │  │  de red que      │    │
│                          │               │  │  el host)        │    │
│  Namespace separado      │               │  └──────────────────┘    │
│  Bridge + NAT            │               │                          │
│  Aislamiento ✓           │               │  Sin namespace separado  │
│  Overhead pequeño        │               │  Sin NAT                 │
└──────────────────────────┘               │  Sin aislamiento ✗       │
                                           │  Sin overhead            │
                                           └──────────────────────────┘
```

---

## 2. Cómo funciona

### Sin network namespace

En Docker bridge, cada contenedor tiene su propio network namespace (espacio
de nombres de red aislado con sus propias interfaces, tabla de rutas y reglas
iptables). Con `--network host`, el contenedor **no crea un network namespace
nuevo** — usa el del host directamente.

```bash
# Bridge: el contenedor tiene su propia pila de red
docker run --rm alpine ip addr
# 1: lo: <LOOPBACK,UP>
#     inet 127.0.0.1/8
# 42: eth0@if43: <BROADCAST,MULTICAST,UP>
#     inet 172.17.0.2/16        ← IP del contenedor

# Host: el contenedor ve las interfaces del host
docker run --rm --network host alpine ip addr
# 1: lo: <LOOPBACK,UP>
#     inet 127.0.0.1/8
# 2: eth0: <BROADCAST,MULTICAST,UP>
#     inet 192.168.1.10/24      ← IP del HOST
# 3: docker0: <BROADCAST,MULTICAST,UP>
#     inet 172.17.0.1/16        ← bridge Docker del host
```

### Sin traducción de puertos

En modo bridge, Docker hace DNAT para mapear puertos del host al contenedor.
Con host networking, el proceso dentro del contenedor **escucha directamente
en los puertos del host**:

```bash
# Bridge: nginx en el contenedor escucha en 172.17.0.2:80
# Docker hace NAT: host:8080 → 172.17.0.2:80
docker run -d -p 8080:80 nginx

# Host: nginx escucha directamente en 192.168.1.10:80
# No hay traducción de puertos
docker run -d --network host nginx
# nginx escucha en puerto 80 del host directamente
```

**Predicción**: con `--network host`, la opción `-p` se **ignora
silenciosamente**. Docker no la necesita porque el contenedor ya está en
el stack de red del host.

---

## 3. Cuándo usar host networking

### Casos de uso legítimos

| Caso | Por qué host networking |
|------|------------------------|
| **Rendimiento máximo** | Sin overhead de NAT, bridge y veth. Latencia mínima |
| **Muchos puertos** | Servicios que usan cientos de puertos (ej. RTP en VoIP) |
| **Protocolos no TCP/UDP** | Multicast, IGMP, protocolos raw que NAT no soporta |
| **Herramientas de red** | tcpdump, iperf, nmap que necesitan acceso directo a interfaces |
| **Monitorización del host** | Prometheus node_exporter, collectd que leen métricas de red del host |
| **DHCP/DNS server** | Servicios que necesitan escuchar en broadcast o puertos privilegiados de la interfaz real |
| **Alto throughput** | Aplicaciones sensibles a latencia donde cada microsegundo cuenta |

### Cuándo NO usar host networking

| Caso | Por qué no |
|------|-----------|
| **Aplicaciones web** | Bridge funciona bien, host no aporta nada y pierde aislamiento |
| **Múltiples instancias** | No puedes correr dos Nginx en puerto 80 — conflicto de puertos |
| **Entornos multi-tenant** | Sin aislamiento entre contenedores y el host |
| **Seguridad importante** | El contenedor tiene acceso total al stack de red |
| **Portabilidad** | No funciona en Docker Desktop (macOS/Windows) |

---

## 4. Uso básico

### Ejecutar con host networking

```bash
# Contenedor con red del host
docker run -d --network host --name web nginx

# Verificar que nginx escucha en el puerto del host
ss -tlnp 'sport == 80'
# LISTEN  0  511  0.0.0.0:80  0.0.0.0:*  users:(("nginx",pid=12345,fd=6))

# Accesible directamente sin -p
curl http://localhost/
curl http://192.168.1.10/
```

### La opción -p se ignora

```bash
# Con bridge: -p es necesario y funcional
docker run -d -p 8080:80 nginx

# Con host: -p no tiene efecto
docker run -d --network host -p 8080:80 nginx
# WARNING: Published ports are discarded when using host network mode
# nginx sigue escuchando en puerto 80, no en 8080
```

### Verificar el modo de red

```bash
docker inspect web --format '{{.HostConfig.NetworkMode}}'
# host

docker inspect web --format '{{json .NetworkSettings.Networks}}' | jq .
# {
#   "host": {
#     "IPAddress": "",           ← sin IP propia
#     "Gateway": "",             ← sin gateway propio
#     "MacAddress": "",          ← sin MAC propia
#     ...
#   }
# }
```

---

## 5. Comparación con bridge

### Rendimiento

```
Métrica               Bridge           Host            Diferencia
──────────────────   ──────────       ──────          ──────────────
Latencia extra        ~10-50μs         0               Bridge añade overhead
Throughput máx        ~30-40 Gbps      Wire speed      Bridge 5-15% menos
Paquetes/segundo      Limitado por     Sin límite      Bridge: veth + bridge
                      veth + NAT       de Docker       + NAT = más procesamiento
Conexiones nuevas/s   Limitado por     Sin límite      conntrack puede ser
                      conntrack        de Docker       cuello de botella
```

Para la mayoría de aplicaciones web, la diferencia es irrelevante. Se
nota en:
- Tráfico > 10 Gbps
- > 100.000 conexiones nuevas/segundo
- Aplicaciones de latencia ultra-baja (trading, gaming)

### Funcionalidad

```
Característica         Bridge    Host
─────────────────     ──────    ──────
DNS entre containers    ✓         ✗
Aislamiento de red      ✓         ✗
Múltiples instancias    ✓         ✗ (conflicto de puertos)
Puerto custom (-p)      ✓         ✗ (ignorado)
Funciona en macOS       ✓         ✗
IP propia               ✓         ✗ (usa IP del host)
Redes internas          ✓         ✗
Multi-red               ✓         ✗
iptables del host       Bypasa    Respeta
```

### Acceso a servicios del host

```bash
# Desde un contenedor bridge, ¿cómo llegar a un servicio del host?
# Opción 1: host.docker.internal (Docker Desktop)
# Opción 2: IP de la interfaz docker0 (172.17.0.1)
# Opción 3: --add-host host.docker.internal:host-gateway

docker run --rm --add-host host.docker.internal:host-gateway alpine \
    wget -qO- http://host.docker.internal:8080

# Desde un contenedor host: simplemente localhost
docker run --rm --network host alpine wget -qO- http://localhost:8080
# Funciona directamente porque comparte el stack ✓
```

---

## 6. Implicaciones de seguridad

### Superficie de ataque ampliada

```
Bridge:
  El contenedor solo ve su red virtual.
  Un contenedor comprometido puede:
  - Comunicarse con contenedores en la misma red bridge
  - Salir a Internet (NAT)
  - NO puede acceder a puertos del host directamente
  - NO puede ver el tráfico de otras interfaces
  - NO puede modificar iptables del host

Host:
  El contenedor ve TODO el stack de red.
  Un contenedor comprometido puede:
  - Escuchar en CUALQUIER puerto del host
  - Capturar tráfico de TODAS las interfaces (tcpdump)
  - Ver y modificar reglas iptables/nftables
  - Acceder a TODOS los servicios locales del host
  - Hacer ARP spoofing en la red local
  - Acceder a otros contenedores por puertos publicados
```

### Conflicto de puertos

```bash
# Con bridge: dos Nginx pueden coexistir
docker run -d -p 8080:80 --name web1 nginx
docker run -d -p 8081:80 --name web2 nginx
# OK ✓ — cada uno en su puerto del host

# Con host: conflicto inmediato
docker run -d --network host --name web1 nginx
docker run -d --network host --name web2 nginx
# web2 falla: "bind: address already in use" para el puerto 80
```

### Mitigaciones

```bash
# 1. No ejecutar como root dentro del contenedor
docker run -d --network host --user 1000:1000 myapp
# El proceso no puede escuchar en puertos < 1024 ni capturar tráfico

# 2. Limitar capabilities
docker run -d --network host \
    --cap-drop ALL \
    --cap-add NET_BIND_SERVICE \
    myapp
# Solo puede bind a puertos, no puede capturar ni modificar iptables

# 3. Usar read-only filesystem
docker run -d --network host --read-only myapp

# 4. Security profiles (seccomp, AppArmor)
docker run -d --network host \
    --security-opt seccomp=default.json \
    myapp
```

### Principio de mínimo privilegio

```
¿Necesitas host networking?
     │
     ├── No → Usar bridge (default, seguro, suficiente para el 90% de casos)
     │
     └── Sí → Minimizar el impacto:
              ├── --user (no root)
              ├── --cap-drop ALL + solo caps necesarias
              ├── --read-only
              └── Documentar POR QUÉ host es necesario
```

---

## 7. Host networking en Docker Compose

### Sintaxis

```yaml
# docker-compose.yml
services:
  monitoring:
    image: prom/node-exporter:latest
    network_mode: host
    pid: host                    # también puede necesitar PID del host
    volumes:
      - /proc:/host/proc:ro
      - /sys:/host/sys:ro
    command:
      - '--path.procfs=/host/proc'
      - '--path.sysfs=/host/sys'
```

### Combinar host y bridge en el mismo Compose

```yaml
services:
  # Monitorización: necesita acceso al stack de red del host
  node-exporter:
    image: prom/node-exporter:latest
    network_mode: host

  # Aplicación: red bridge normal con aislamiento
  web:
    image: nginx
    ports:
      - "80:80"
    networks:
      - frontend

  app:
    image: myapp
    networks:
      - frontend
      - backend

  db:
    image: postgres:16
    networks:
      - backend

networks:
  frontend:
  backend:
    internal: true
```

**Nota**: un servicio con `network_mode: host` no puede participar en redes
Compose (`networks:`). Son mutuamente excluyentes.

### Restricciones en Compose

```yaml
services:
  myservice:
    image: myapp
    network_mode: host

    # ✗ Estas opciones se ignoran o causan error con network_mode: host
    # ports:             ← ignorado
    # networks:          ← error (incompatible)
    # network_mode: host + networks: ← error

    # ✓ Estas opciones sí funcionan
    # volumes:
    # environment:
    # restart:
```

---

## 8. Otros modos de red: none y macvlan

### none — sin red

```bash
docker run --rm --network none alpine ip addr
# 1: lo: <LOOPBACK,UP,LOWER_UP>
#     inet 127.0.0.1/8
# Solo loopback, sin ninguna conexión de red
```

Casos de uso de `--network none`:
- Contenedores que no necesitan red (procesamiento batch, generación de archivos)
- Máximo aislamiento de red
- Configuración manual de red posterior (con `nsenter`)

```yaml
# En Compose:
services:
  processor:
    image: myprocessor
    network_mode: none
    volumes:
      - ./input:/data/input:ro
      - ./output:/data/output
```

### macvlan — IP real en la red física

macvlan asigna una **dirección MAC real** al contenedor, haciéndolo aparecer
como un dispositivo físico más en la red LAN.

```
Red LAN (192.168.1.0/24)
┌─────────────────────────────────────────┐
│                                         │
│  Host (192.168.1.10)                    │
│    │                                    │
│  PC (.20)  Printer (.30)  Container (.50)
│                           ▲              │
│                           │ macvlan       │
│                           │ MAC propia    │
│                           │ IP de la LAN  │
└─────────────────────────────────────────┘
```

```bash
# Crear red macvlan
docker network create -d macvlan \
    --subnet=192.168.1.0/24 \
    --gateway=192.168.1.1 \
    -o parent=eth0 \
    my_macvlan

# Contenedor con IP en la LAN real
docker run -d --network my_macvlan \
    --ip 192.168.1.50 \
    --name direct_access \
    nginx

# El contenedor es accesible directamente desde la LAN
# como si fuera otra máquina física
ping 192.168.1.50    # desde cualquier PC de la LAN
```

### macvlan: limitación importante

```
⚠ El host NO puede comunicarse con el contenedor macvlan

Host (192.168.1.10) ──✗──▶ Container macvlan (192.168.1.50)
  ↑                                    │
  └────────────────────────────────────┘
           ✗ No funciona tampoco

Otros PCs de la LAN ──✓──▶ Container macvlan (192.168.1.50)
           ✓ Funciona

Solución: crear sub-interfaz macvlan en el host
```

### ipvlan — similar a macvlan pero comparte MAC

```bash
# ipvlan L2: como macvlan pero todos comparten la MAC del host
docker network create -d ipvlan \
    --subnet=192.168.1.0/24 \
    --gateway=192.168.1.1 \
    -o parent=eth0 \
    my_ipvlan

# ipvlan L3: enrutamiento L3, sin broadcast
docker network create -d ipvlan \
    --subnet=10.99.0.0/24 \
    -o parent=eth0 \
    -o ipvlan_mode=l3 \
    my_ipvlan_l3
```

### Comparación de drivers

```
Driver     Aislamiento  Rendimiento  IP propia   MAC propia   Caso de uso
────────   ──────────   ──────────   ─────────   ──────────   ──────────────
bridge     ✓ (NAT)      Bueno        Virtual     Virtual      General (default)
host       ✗            Máximo       Host        Host         Alto rendimiento
none       Máximo       N/A          No          No           Sin red
macvlan    ✓ (L2)       Muy bueno    LAN real    Propia       Dispositivo virtual
ipvlan     ✓ (L2/L3)    Muy bueno    LAN real    Compartida   Entornos con límite MAC
overlay    ✓ (L2)       Moderado     Overlay     Virtual      Multi-host (Swarm)
```

---

## 9. Escenarios prácticos

### Escenario 1: herramienta de diagnóstico de red

```bash
# tcpdump del tráfico del host desde un contenedor
docker run -it --rm --network host --cap-add NET_RAW \
    nicolaka/netshoot \
    tcpdump -i eth0 -nn port 80 -c 10

# iperf3 server para medir throughput
docker run -d --rm --network host --name iperf \
    networkstatic/iperf3 -s

# Desde otro equipo:
iperf3 -c 192.168.1.10
```

### Escenario 2: Prometheus + node_exporter

```yaml
# docker-compose.yml
services:
  prometheus:
    image: prom/prometheus:latest
    ports:
      - "9090:9090"
    networks:
      - monitoring
    volumes:
      - ./prometheus.yml:/etc/prometheus/prometheus.yml:ro

  node-exporter:
    image: prom/node-exporter:latest
    network_mode: host          # necesita métricas de red del host
    pid: host
    volumes:
      - /proc:/host/proc:ro
      - /sys:/host/sys:ro
      - /:/rootfs:ro
    command:
      - '--path.procfs=/host/proc'
      - '--path.sysfs=/host/sys'
      - '--path.rootfs=/rootfs'
      - '--collector.filesystem.mount-points-exclude=^/(sys|proc|dev|host|etc)($$|/)'

networks:
  monitoring:
```

node_exporter necesita `--network host` para reportar correctamente las
interfaces de red, tráfico por interfaz y métricas de conectividad del host.

### Escenario 3: DNS local (Pi-hole, dnsmasq)

```bash
# Pi-hole necesita escuchar en la IP real para ser el DNS de la LAN
docker run -d --network host \
    --name pihole \
    --cap-add NET_BIND_SERVICE \
    -e TZ=Europe/Madrid \
    -e WEBPASSWORD=admin \
    -v pihole_data:/etc/pihole \
    -v pihole_dns:/etc/dnsmasq.d \
    pihole/pihole:latest

# Los clientes de la LAN configuran 192.168.1.10 como DNS
# Pi-hole escucha directamente en esa IP
```

### Escenario 4: comparar rendimiento bridge vs host

```bash
# Servidor iperf3 en el host
iperf3 -s -p 5201 &

# Test desde contenedor bridge
docker run --rm networkstatic/iperf3 -c 172.17.0.1 -p 5201
# Throughput: ~35 Gbps (ejemplo)

# Test desde contenedor host
docker run --rm --network host networkstatic/iperf3 -c 127.0.0.1 -p 5201
# Throughput: ~45 Gbps (ejemplo)
# La diferencia es el overhead de bridge + NAT
```

---

## 10. Errores comunes

### Error 1: usar -p con --network host

```bash
# ✗ Publicar puertos con host networking
docker run -d --network host -p 8080:80 nginx
# WARNING: Published ports are discarded when using host network mode
# nginx escucha en puerto 80, NO en 8080

# ✓ Con host, el contenedor usa los puertos directamente
docker run -d --network host nginx
# nginx escucha en 80 — si quieres otro puerto, configura nginx.conf
```

### Error 2: asumir que host networking funciona en Docker Desktop

```bash
# ✗ En macOS/Windows, --network host no expone puertos en tu máquina
docker run -d --network host nginx    # en Docker Desktop macOS
curl http://localhost/
# No funciona — Docker Desktop corre en una VM Linux

# ✓ En Docker Desktop, usar bridge con -p
docker run -d -p 80:80 nginx

# --network host solo funciona nativamente en Linux
```

### Error 3: conflictos de puertos no diagnosticados

```bash
# ✗ El contenedor falla sin mensaje claro
docker run -d --network host --name web2 nginx
docker logs web2
# nginx: [emerg] bind() to 0.0.0.0:80 failed (98: Address already in use)

# ✓ Verificar puertos ANTES de lanzar
ss -tlnp 'sport == 80'
# Si algo ya escucha → cambiar puerto en la config del contenedor
# o detener el servicio que ocupa el puerto
```

### Error 4: usar host networking "por si acaso"

```bash
# ✗ "No sé qué puertos necesita, así que uso host"
docker run -d --network host mysterious_app
# Expone TODO al contenedor — superficie de ataque máxima

# ✓ Descubrir puertos y usar bridge
docker run -d mysterious_app
docker exec mysterious_app ss -tlnp
# Identificar puertos y publicar solo los necesarios
docker run -d -p 8080:8080 -p 9090:9090 mysterious_app
```

### Error 5: contenedor host sin limitar capabilities

```bash
# ✗ Host networking + root + todas las capabilities
docker run -d --network host myapp
# El contenedor puede: capturar tráfico, modificar iptables,
# hacer ARP spoofing, bind a cualquier puerto...

# ✓ Limitar capabilities al mínimo
docker run -d --network host \
    --user 1000:1000 \
    --cap-drop ALL \
    --cap-add NET_BIND_SERVICE \
    myapp
```

---

## 11. Cheatsheet

```bash
# ╔══════════════════════════════════════════════════════════════════╗
# ║         DOCKER HOST NETWORKING — CHEATSHEET                    ║
# ╠══════════════════════════════════════════════════════════════════╣
# ║                                                                ║
# ║  USO BÁSICO:                                                  ║
# ║  docker run -d --network host nginx                            ║
# ║  docker run -d --network none alpine   # sin red               ║
# ║                                                                ║
# ║  EN COMPOSE:                                                  ║
# ║  services:                                                    ║
# ║    myservice:                                                  ║
# ║      network_mode: host     # o "none"                        ║
# ║                                                                ║
# ║  VERIFICAR:                                                   ║
# ║  docker inspect X --format '{{.HostConfig.NetworkMode}}'       ║
# ║  docker exec X ip addr      # debe mostrar IPs del host       ║
# ║  ss -tlnp                   # puertos visibles en el host      ║
# ║                                                                ║
# ║  CUÁNDO USAR HOST:                                            ║
# ║  ✓ Rendimiento máximo (>10Gbps, ultra-low latency)            ║
# ║  ✓ Muchos puertos (VoIP RTP, media streaming)                 ║
# ║  ✓ Herramientas de red (tcpdump, iperf, nmap)                 ║
# ║  ✓ Monitorización (node_exporter, collectd)                   ║
# ║  ✓ DNS/DHCP del host (Pi-hole, dnsmasq)                       ║
# ║                                                                ║
# ║  CUÁNDO NO USAR HOST:                                         ║
# ║  ✗ Apps web normales (bridge funciona bien)                    ║
# ║  ✗ Múltiples instancias (conflictos de puertos)               ║
# ║  ✗ Docker Desktop macOS/Windows (no funciona)                 ║
# ║  ✗ Cuando el aislamiento importa                              ║
# ║                                                                ║
# ║  SEGURIDAD CON HOST:                                          ║
# ║  --user 1000:1000            # no root                        ║
# ║  --cap-drop ALL              # quitar todas las capabilities  ║
# ║  --cap-add NET_BIND_SERVICE  # solo lo necesario              ║
# ║  --read-only                 # filesystem de solo lectura     ║
# ║                                                                ║
# ║  OTROS DRIVERS:                                               ║
# ║  bridge    → default, NAT, aislamiento (90% de los casos)     ║
# ║  host      → sin namespace, rendimiento máximo                ║
# ║  none      → sin red                                          ║
# ║  macvlan   → IP real en la LAN, MAC propia                    ║
# ║  ipvlan    → IP real en la LAN, MAC compartida                ║
# ║  overlay   → multi-host (Docker Swarm)                        ║
# ║                                                                ║
# ╚══════════════════════════════════════════════════════════════════╝
```

---

## 12. Ejercicios

### Ejercicio 1: comparar bridge vs host

**Contexto**: necesitas decidir qué modo de red usar para un servicio y
quieres ver las diferencias de forma práctica.

**Tareas**:

1. Lanza un contenedor Nginx con bridge networking (`-p 8080:80`)
2. Lanza otro Nginx con host networking
3. Compara:
   - `docker exec` + `ip addr` en cada contenedor
   - `ss -tlnp` en el host — ¿qué proceso aparece escuchando?
   - Las interfaces de red que ve cada contenedor
   - El resultado de `docker inspect --format NetworkMode`
4. Mide latencia con `curl -w` a cada uno desde el host
5. Intenta lanzar un segundo contenedor host con Nginx — ¿qué pasa?
6. Verifica las reglas iptables de Docker para el contenedor bridge
   (`sudo iptables -t nat -L DOCKER -n`)

**Pistas**:
- En bridge, `ss -tlnp` mostrará `docker-proxy` escuchando
- En host, `ss -tlnp` mostrará `nginx` directamente
- El segundo Nginx host fallará con "address already in use"
- La diferencia de latencia será mínima en localhost

> **Pregunta de reflexión**: Docker Desktop en macOS/Windows ejecuta Docker
> dentro de una VM Linux. ¿Por qué `--network host` no funciona como
> esperarías en esos entornos? ¿Qué stack de red "ve" el contenedor cuando
> usas host en Docker Desktop?

---

### Ejercicio 2: despliegue seguro con host networking

**Contexto**: necesitas ejecutar Prometheus node_exporter con `--network host`
para monitorizar las interfaces de red del servidor.

**Tareas**:

1. Lanza node_exporter con `--network host` y las mitigaciones de seguridad:
   - Usuario no root
   - Capabilities mínimas
   - Filesystem read-only
   - Volúmenes de /proc y /sys como read-only
2. Verifica que las métricas de red están disponibles:
   ```bash
   curl http://localhost:9100/metrics | grep node_network
   ```
3. Verifica que el contenedor NO puede:
   - Modificar archivos del host
   - Escuchar en puertos adicionales no previstos
4. Escribe el docker-compose.yml completo con todas las restricciones
5. Integra con Prometheus (en bridge) que scrape node_exporter (en host)

**Pistas**:
- node_exporter necesita `--cap-add SYS_PTRACE` para algunas métricas
- Prometheus puede alcanzar node_exporter en `host.docker.internal:9100`
  o en la IP del host
- `--read-only` puede necesitar un tmpfs para archivos temporales

> **Pregunta de reflexión**: node_exporter con `--network host` ve todas
> las conexiones de red del host. Si alguien accede al endpoint de métricas
> (puerto 9100), ¿qué información sensible podría obtener? ¿Cómo
> protegerías este endpoint?

---

### Ejercicio 3: elegir el driver correcto

**Contexto**: tienes estos 5 servicios para desplegar en Docker. Para cada
uno, elige el driver de red más apropiado y justifica.

1. **Servidor web Nginx** — reverse proxy público, puertos 80/443
2. **PostgreSQL** — base de datos, solo accesible desde la app
3. **node_exporter** — monitorización de métricas del host
4. **Servidor DHCP** — servir direcciones IP a la red local
5. **Job de procesamiento batch** — lee archivos, genera PDFs, sin red

**Tareas**:

1. Para cada servicio, indica: driver (bridge/host/none/macvlan),
   justificación, y configuración de seguridad necesaria
2. Diseña la arquitectura de redes (qué redes bridge crear, cuáles
   internas, quién se conecta a cuál)
3. Escribe un docker-compose.yml que implemente toda la arquitectura
4. Documenta qué pasaría si usaras el driver incorrecto para cada caso

**Pistas**:
- Solo 2 de los 5 necesitan algo diferente a bridge
- El job batch es candidato perfecto para `none`
- El servidor DHCP necesita acceso a broadcast de la LAN
- PostgreSQL NUNCA debería usar host

> **Pregunta de reflexión**: si tuvieras que correr un servidor DNS
> autoritativo (Bind, PowerDNS) que sirve tanto a la LAN como a Internet,
> ¿usarías host, macvlan, o bridge con `-p 53:53/udp`? ¿Cuáles son las
> implicaciones de rendimiento y seguridad de cada opción?

---

> **Siguiente tema**: T03 — Lab de red multi-contenedor (simular topologías de red con Docker Compose)
