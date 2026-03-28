# T01 — Drivers de red

## El modelo de red de Docker

Cuando Docker se instala, crea automáticamente una interfaz de red virtual llamada
`docker0` en el host. Esta interfaz actúa como un **bridge** (puente) de red que
conecta los contenedores entre sí y con el mundo exterior a través de NAT.

Cada contenedor recibe su propia interfaz de red virtual (`veth`) conectada al
bridge, con una IP privada asignada automáticamente.

```
┌───────────────────────────────────────────────────────────┐
│                          HOST                             │
│                                                           │
│  eth0 (IP pública)                                        │
│    │                                                      │
│    │  NAT (iptables/nftables)                             │
│    │                                                      │
│  docker0 (172.17.0.1)  ← bridge por defecto               │
│    │                                                      │
│    ├── veth1 ──── contenedor A (172.17.0.2)               │
│    ├── veth2 ──── contenedor B (172.17.0.3)               │
│    └── veth3 ──── contenedor C (172.17.0.4)               │
│                                                           │
│  Los contenedores pueden verse entre sí por IP.           │
│  El host también puede alcanzarlos por IP (172.17.0.x).   │
│  -p publica puertos en las interfaces del host            │
│  para acceso externo o vía localhost.                      │
└───────────────────────────────────────────────────────────┘
```

### Anatomía de una conexión de red

Cada vez que creas un contenedor en la red bridge, Docker:

1. Crea un par de interfaces `veth` (virtual Ethernet)
2. Conecta un extremo al bridge `docker0` del host
3. Conecta el otro extremo dentro del contenedor (generalmente como `eth0`)
4. Asigna una IP del rango del bridge (típicamente 172.17.0.0/16)
5. Configura el NAT en iptables para permitir salida a internet

En redes bridge **custom**, además:

6. Configura un servidor DNS interno (127.0.0.11) para resolver nombres de contenedores

Este paso 6 **no ocurre** en la red bridge por defecto — es una de las razones
principales para usar redes custom.

```bash
# Ver las interfaces veth en el host
ip link show type veth

# Ver el bridge docker0
ip addr show docker0
# docker0: <BROADCAST,MULTICAST,UP> mtu 1500
#     inet 172.17.0.1/16

# Dentro de un contenedor
docker run --rm alpine ip addr
# eth0@if5: <BROADCAST,MULTICAST,UP> mtu 1500
#     inet 172.17.0.2/16
```

## Drivers disponibles

Docker implementa la red a través de **drivers** intercambiables. Cada driver
ofrece un modelo de conectividad diferente.

| Driver | Uso | Aislamiento | Casos de uso |
|---|---|---|---|
| `bridge` | Default | Red propia por contenedor | **General** |
| `host` | `--network host` | Sin aislamiento | Alto rendimiento, monitoreo |
| `none` | `--network none` | Total (solo loopback) | Tareas offline, sandboxing |
| `overlay` | Docker Swarm | Multi-host | Clústeres |
| `macvlan` | `--driver macvlan` | Total (dispositivo físico) | Apps legacy |

### bridge (por defecto)

Red aislada donde los contenedores obtienen IPs en una subred privada. La
comunicación con el exterior pasa por NAT.

```bash
# Cualquier contenedor sin --network usa bridge por defecto
docker run -d --name web nginx

# Inspeccionar la IP del contenedor
docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' web
# 172.17.0.2

# Inspeccionar la red bridge
docker network inspect bridge
```

#### Dos tipos de bridge networks

| Característica | Bridge por defecto (`docker0`) | Bridge custom |
|---|---|---|
| DNS interno | **No** — solo comunicación por IP | **Sí** — resolución por nombre |
| Aislamiento | Todos los contenedores comparten la red | Solo contenedores explícitamente conectados |
| Configuración | Automática, no modificable | `--subnet`, `--gateway`, labels |
| Red de contenedores existentes | Todos los que no especifican `--network` | Solo los que conectas explícitamente |
| Recomendación | Solo para pruebas rápidas | **Usar siempre en producción** |

```bash
# Crear una red bridge custom
docker network create mynet

# Crear una red con subred específica
docker network create --subnet 10.0.1.0/24 --gateway 10.0.1.1 backend

# Especificar driver explícitamente
docker network create --driver bridge --subnet 192.168.100.0/24 my_custom_net

# Ejecutar un contenedor en la red custom
docker run -d --name api --network mynet nginx
```

#### Por qué el bridge default no tiene DNS

El bridge default (`docker0`) fue diseñado antes de que Docker incorporara
un servidor DNS interno. Las redes custom incluyen **dockerd como DNS server**
que responde en 127.0.0.11.

### host

El contenedor **comparte directamente el network stack del host**. No hay
aislamiento de red, no hay bridge, no hay NAT. El contenedor ve las mismas
interfaces que el host.

```bash
# El contenedor usa directamente las interfaces del host
docker run --rm --network host alpine ip addr
# Muestra eth0, lo, wlan0, etc. del HOST — no interfaces virtuales

# Un servidor en el contenedor escucha directamente en el host
docker run -d --network host nginx
# nginx escucha en el puerto 80 del host — sin -p necesario
curl http://localhost
```

**Cuándo usarlo**:
- Aplicaciones sensibles a latencia de red que necesitan máximo rendimiento
- Herramientas de monitoreo o debugging que necesitan ver el tráfico del host
- Cuando el overhead de NAT es inaceptable

**Limitaciones**:
- No funciona en Docker Desktop (macOS/Windows) — solo en Linux nativo
- No hay aislamiento: conflictos de puertos con servicios del host
- No se puede usar `-p` (no tiene sentido, el contenedor ya es el host)
- No funciona bien con Docker Swarm

### none

Sin red. El contenedor solo tiene la interfaz **loopback** (`lo`).

```bash
docker run --rm --network none alpine ip addr
# 1: lo: <LOOPBACK,UP> ...
#     inet 127.0.0.1/8 ...
# Solo loopback, nada más

docker run --rm --network none alpine ping -c 1 8.8.8.8
# ping: sendto: Network unreachable
```

**Cuándo usarlo**:
- Contenedores que procesan datos locales sin necesidad de red
- Tareas de compilación donde la red es un riesgo de seguridad
- Sandboxing estricto
- Jobs de procesamiento batch que reciben datos por volumen

### overlay

Redes que abarcan **múltiples hosts Docker** (Docker Swarm). Permite que
contenedores en máquinas físicas diferentes se comuniquen como si estuvieran
en la misma red local.

```
Host 1                          Host 2
┌─────────────┐                ┌─────────────┐
│ Container A │◄──── Overlay ──►│ Container B │
│ 10.0.0.2    │                │ 10.0.0.3    │
└─────────────┘                └─────────────┘
```

No se cubre en este curso — se menciona para contexto.

### macvlan

Asigna una **dirección MAC real** al contenedor, haciéndolo aparecer como un
dispositivo físico en la red LAN. Útil para aplicaciones legacy que esperan estar
en la red física. Uso avanzado, no cubierto.

## Gestión de redes

```bash
# Listar redes
docker network ls
# NETWORK ID     NAME      DRIVER    SCOPE
# abc123...      bridge    bridge    local
# def456...      host      host      local
# ghi789...      none      null      local

# Las tres redes (bridge, host, none) existen siempre — no se pueden eliminar

# Crear una red
docker network create mynet

# Crear con opciones completas
docker network create \
  --driver bridge \
  --subnet 10.0.2.0/24 \
  --gateway 10.0.2.1 \
  --label env=development \
  dev_network

# Inspeccionar una red
docker network inspect mynet

# Conectar un contenedor ya existente a una red
docker run -d --name api alpine sleep 300
docker network connect mynet api

# Desconectar
docker network disconnect mynet api

# Eliminar una red (debe estar vacía — sin contenedores conectados)
docker network rm mynet

# Eliminar todas las redes sin usar
docker network prune
```

## Inspección de la red bridge por defecto

```bash
docker network inspect bridge
```

Salida relevante (simplificada):

```json
[
    {
        "Name": "bridge",
        "Driver": "bridge",
        "IPAM": {
            "Config": [
                {
                    "Subnet": "172.17.0.0/16",
                    "Gateway": "172.17.0.1"
                }
            ]
        },
        "Containers": {
            "abc123...": {
                "Name": "web",
                "IPv4Address": "172.17.0.2/16",
                "MacAddress": "02:42:ac:11:00:02"
            }
        },
        "Options": {
            "com.docker.network.bridge.name": "docker0",
            "com.docker.network.bridge.enable_icc": "true",
            "com.docker.network.bridge.enable_ip_masquerade": "true"
        }
    }
]
```

Campos clave:

| Campo | Significado |
|---|---|
| `IPAM.Config.Subnet` | Rango de IPs disponibles para contenedores |
| `IPAM.Config.Gateway` | IP del bridge en el host (`docker0`) |
| `Containers` | Contenedores actualmente conectados |
| `Options.bridge.name` | Nombre de la interfaz del bridge en el host |
| `Options.enable_icc` | Si la comunicación entre contenedores está habilitada (Inter-Container Communication) |
| `Options.enable_ip_masquerade` | Si NAT está habilitado (para salida a internet) |

## Verificar la red desde el host

```bash
# Ver la interfaz docker0
ip addr show docker0
# docker0: <BROADCAST,MULTICAST,UP> ...
#     inet 172.17.0.1/16 ...

# Ver las interfaces veth (un par por contenedor)
ip link show type veth

# Ver las reglas NAT creadas por Docker
sudo iptables -t nat -L -n | grep DOCKER
# DNAT   tcp  --  0.0.0.0/0  0.0.0.0/0  tcp dpt:8080 to:172.17.0.2:80

# Ver reglas de FORWARD
sudo iptables -L FORWARD -n | grep DOCKER
```

Docker manipula `iptables` (o `nftables` en distribuciones modernas) para crear
las reglas de NAT y port forwarding. Esto es transparente al usuario, pero es
importante entenderlo para diagnosticar problemas de red.

## Cómo funciona el DNS interno

En redes bridge custom, Docker incluye un servidor DNS en 127.0.0.11:

```bash
# Ver el DNS server del contenedor
docker run --rm --network mynet alpine cat /etc/resolv.conf
# nameserver 127.0.0.11
# options ndots:0

# El contenedor puede resolver nombres de otros contenedores en la misma red
docker run -d --name svc1 --network mynet alpine sleep 300
docker run -d --name svc2 --network mynet alpine sleep 300
docker run --rm --network mynet alpine ping -c 1 svc1
# PING svc1 (172.18.0.2): 56 data bytes
```

El servidor DNS de Docker responde a queries para nombres de contenedores
y reenvía todo lo demás al DNS del host (configurado en `/etc/resolv.conf`
del host).

## Conectar contenedores a múltiples redes

Un contenedor puede estar conectado a múltiples redes simultáneamente usando
`docker network connect`:

```bash
# Crear dos redes
docker network create frontend
docker network create backend

# Crear contenedor en una red
docker run -d --name app --network frontend alpine sleep 300

# Conectar a la segunda red
docker network connect backend app

# Ahora 'app' tiene una IP en cada red y puede comunicarse con
# contenedores en ambas

# Ver IPs en cada red
docker inspect app --format '{{range $name, $net := .NetworkSettings.Networks}}{{$name}}: {{$net.IPAddress}}{{"\n"}}{{end}}'

# Desconectar de una red
docker network disconnect backend app
```

---

## Práctica

### Ejercicio 1 — Inspeccionar la red por defecto

```bash
# Levantar dos contenedores
docker run -d --name net_test1 alpine sleep 3600
docker run -d --name net_test2 alpine sleep 3600

# Inspeccionar la red bridge
docker network inspect bridge --format '{{range .Containers}}{{.Name}}: {{.IPv4Address}}{{"\n"}}{{end}}'

# Verificar conectividad por IP entre contenedores
# (usar la IP real de net_test2 del comando anterior)
docker exec net_test1 ping -c 2 172.17.0.3

# Intentar conectar por nombre (falla en la red por defecto)
docker exec net_test1 ping -c 2 net_test2
# ping: bad address 'net_test2' — no hay DNS en la red por defecto

# Ver las interfaces veth en el host
ip link show type veth

# Limpiar
docker rm -f net_test1 net_test2
```

**Predicción**: la comunicación por IP funciona, pero por nombre falla porque
la red bridge por defecto no incluye DNS interno.

### Ejercicio 2 — Red custom con DNS

```bash
# Crear una red custom
docker network create test_net

# Levantar contenedores en la red custom
docker run -d --name dns_test1 --network test_net alpine sleep 3600
docker run -d --name dns_test2 --network test_net alpine sleep 3600

# Conectar por nombre (funciona en redes custom)
docker exec dns_test1 ping -c 2 dns_test2
# PING dns_test2 (172.19.0.3): 56 data bytes — resuelve el nombre

# Ver el DNS server del contenedor
docker exec dns_test1 cat /etc/resolv.conf
# nameserver 127.0.0.11  — DNS embebido de Docker

# Ver la IP de cada contenedor
docker inspect dns_test1 --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'
docker inspect dns_test2 --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'

# Limpiar
docker rm -f dns_test1 dns_test2
docker network rm test_net
```

**Predicción**: `ping dns_test2` resuelve el nombre porque la red custom
incluye DNS en 127.0.0.11.

### Ejercicio 3 — Red host

```bash
# Ejecutar con red host (solo Linux nativo)
docker run --rm --network host alpine ip addr
# Muestra las interfaces del HOST (eth0, lo, wlan0, docker0, etc.)

# Comparar con bridge
docker run --rm alpine ip addr
# Muestra solo eth0@ifX (virtual) y lo

# Servidor en red host — escucha directamente en el host
docker run -d --name host_nginx --network host nginx
curl http://localhost  # Funciona sin -p

ss -tlnp | grep :80   # nginx escuchando directamente en el host

# Limpiar
docker rm -f host_nginx
```

**Predicción**: con `--network host`, `ip addr` mostrará todas las interfaces
reales del host (no virtuales). nginx escuchará directamente en el puerto 80
del host sin necesidad de `-p`.

### Ejercicio 4 — Red none (aislamiento total)

```bash
# Contenedor completamente aislado
docker run --rm --network none alpine ip addr
# Solo ve loopback

docker run --rm --network none alpine ping -c 1 8.8.8.8
# Network unreachable

# Útil para jobs de procesamiento offline que reciben datos por volumen
mkdir -p /tmp/net_test
echo "datos de entrada" > /tmp/net_test/input.txt
docker run --rm --network none \
  -v /tmp/net_test:/data \
  alpine sh -c 'wc -l /data/input.txt > /data/output.txt'
cat /tmp/net_test/output.txt
# 1 /data/input.txt

rm -rf /tmp/net_test
```

**Predicción**: el contenedor no puede alcanzar ninguna dirección de red, pero
puede procesar datos recibidos por volumen sin problemas.

### Ejercicio 5 — Conectar contenedor a múltiples redes

```bash
# Crear dos redes
docker network create net_a
docker network create net_b

# Crear contenedores en redes separadas
docker run -d --name multi_net1 --network net_a alpine sleep 300
docker run -d --name multi_net2 --network net_b alpine sleep 300

# No pueden comunicarse — redes diferentes
docker exec multi_net1 ping -c 1 -W 2 multi_net2 2>&1
# ping: bad address 'multi_net2'

# Conectar multi_net1 a net_b también
docker network connect net_b multi_net1

# Ahora multi_net1 está en ambas redes y puede resolver multi_net2
docker exec multi_net1 ping -c 1 multi_net2
# PING multi_net2 (172.x.0.x): 56 data bytes — funciona

# Ver las IPs de multi_net1 en cada red
docker inspect multi_net1 --format '{{range $name, $net := .NetworkSettings.Networks}}{{$name}}: {{$net.IPAddress}}{{"\n"}}{{end}}'
# net_a: 172.x.0.2
# net_b: 172.y.0.3

# Desconectar de net_b
docker network disconnect net_b multi_net1
docker exec multi_net1 ping -c 1 -W 2 multi_net2 2>&1
# Falla de nuevo — ya no están conectados

# Limpiar
docker rm -f multi_net1 multi_net2
docker network rm net_a net_b
```

**Predicción**: un contenedor conectado a dos redes tiene una IP en cada una y
puede comunicarse con contenedores de ambas. Al desconectarlo, pierde acceso.

### Ejercicio 6 — Inspección de reglas NAT

```bash
# Crear un contenedor con puerto publicado
docker run -d --name web -p 8080:80 nginx

# Ver reglas NAT (la cadena DOCKER en la tabla nat)
sudo iptables -t nat -L DOCKER -n -v
# Buscar la regla DNAT que mapea 8080 → 172.17.0.x:80

# Ver reglas de forward
sudo iptables -L FORWARD -n -v | grep DOCKER

# Verificar que el puerto está publicado
curl -s http://localhost:8080 | head -5

# Limpiar
docker rm -f web
```

**Predicción**: `iptables` mostrará una regla DNAT que traduce el tráfico entrante
al puerto 8080 del host hacia el puerto 80 del contenedor en su IP privada.

### Ejercicio 7 — Red bridge custom con subred e IP fija

```bash
# Crear red con subred específica
docker network create \
  --driver bridge \
  --subnet 192.168.50.0/24 \
  --gateway 192.168.50.1 \
  --ip-range 192.168.50.128/25 \
  custom_bridge

# Ver la configuración IPAM
docker network inspect custom_bridge --format '{{json .IPAM.Config}}'

# Crear contenedor con IP fija
docker run -d --name fixed_ip \
  --network custom_bridge \
  --ip 192.168.50.10 \
  alpine sleep 300

# Verificar IP
docker inspect fixed_ip --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'
# 192.168.50.10

# Verificar que el contenedor puede salir a internet
docker exec fixed_ip ping -c 1 8.8.8.8

# Limpiar
docker rm -f fixed_ip
docker network rm custom_bridge
```

**Predicción**: el contenedor obtiene la IP fija 192.168.50.10 especificada con
`--ip`, no una del rango automático.
