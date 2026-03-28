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
└───────────────────────────────────────────────────────────┘
```

## Drivers disponibles

Docker implementa la red a través de **drivers** intercambiables. Cada driver
ofrece un modelo de conectividad diferente.

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

Hay dos tipos de redes bridge:

| Característica | Bridge por defecto (`docker0`) | Bridge custom |
|---|---|---|
| DNS interno | No — solo comunicación por IP | Sí — resolución por nombre |
| Aislamiento | Todos los contenedores comparten la red | Solo contenedores explícitamente conectados |
| Configuración | Automática, no modificable | `--subnet`, `--gateway`, labels |
| Recomendación | Solo para pruebas rápidas | **Usar siempre en producción** |

```bash
# Crear una red bridge custom
docker network create mynet

# Crear una red con subred específica
docker network create --subnet 10.0.1.0/24 --gateway 10.0.1.1 backend

# Ejecutar un contenedor en la red custom
docker run -d --name api --network mynet nginx
```

### host

El contenedor **comparte directamente el network stack del host**. No hay
aislamiento de red, no hay bridge, no hay NAT. El contenedor ve las mismas
interfaces que el host.

```bash
# El contenedor usa directamente las interfaces del host
docker run --rm --network host alpine ip addr
# Muestra eth0, lo, etc. del HOST — no interfaces virtuales

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

### overlay

Redes que abarcan **múltiples hosts Docker** (Docker Swarm). Permite que
contenedores en máquinas físicas diferentes se comuniquen como si estuvieran
en la misma red local. No se cubre en este curso — se menciona para contexto.

### macvlan

Asigna una **dirección MAC real** al contenedor, haciéndolo aparecer como un
dispositivo físico en la red LAN. Útil para aplicaciones legacy que esperan estar
en la red física. Uso avanzado, no cubierto.

## Gestión de redes

```bash
# Listar redes
docker network ls

# Salida típica
NETWORK ID     NAME      DRIVER    SCOPE
a1b2c3d4e5f6   bridge    bridge    local
f6e5d4c3b2a1   host      host      local
1a2b3c4d5e6f   none      null      local
```

Las tres redes (`bridge`, `host`, `none`) existen siempre — Docker las crea al
instalarse y no se pueden eliminar.

```bash
# Crear una red
docker network create mynet

# Crear con opciones
docker network create \
  --driver bridge \
  --subnet 10.0.2.0/24 \
  --gateway 10.0.2.1 \
  --label env=development \
  dev_network

# Inspeccionar
docker network inspect mynet

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
                "IPv4Address": "172.17.0.2/16"
            }
        },
        "Options": {
            "com.docker.network.bridge.name": "docker0"
        }
    }
]
```

Campos clave:
- `IPAM.Config.Subnet`: rango de IPs disponibles para contenedores
- `IPAM.Config.Gateway`: IP del bridge en el host (`docker0`)
- `Containers`: contenedores actualmente conectados a esta red

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
# DNAT ... tcp dpt:8080 to:172.17.0.2:80
```

Docker manipula `iptables` (o `nftables` en distribuciones modernas) para crear
las reglas de NAT y port forwarding. Esto es transparente al usuario, pero es
importante entenderlo para diagnosticar problemas de red.

---

## Ejercicios

### Ejercicio 1 — Inspeccionar la red por defecto

```bash
# Levantar dos contenedores
docker run -d --name net_test1 alpine sleep 3600
docker run -d --name net_test2 alpine sleep 3600

# Inspeccionar la red bridge
docker network inspect bridge --format '{{range .Containers}}{{.Name}}: {{.IPv4Address}}{{"\n"}}{{end}}'

# Verificar conectividad por IP entre contenedores
docker exec net_test1 ping -c 2 172.17.0.3  # Usar la IP de net_test2

# Intentar conectar por nombre (falla en la red por defecto)
docker exec net_test1 ping -c 2 net_test2
# ping: bad address 'net_test2' — no hay DNS en la red por defecto

# Limpiar
docker rm -f net_test1 net_test2
```

### Ejercicio 2 — Red custom con DNS

```bash
# Crear una red custom
docker network create test_net

# Levantar contenedores en la red custom
docker run -d --name dns_test1 --network test_net alpine sleep 3600
docker run -d --name dns_test2 --network test_net alpine sleep 3600

# Conectar por nombre (funciona en redes custom)
docker exec dns_test1 ping -c 2 dns_test2
# PING dns_test2 (10.0.0.3): 56 data bytes — ¡resuelve!

# Ver el DNS server del contenedor
docker exec dns_test1 cat /etc/resolv.conf
# nameserver 127.0.0.11  — DNS embebido de Docker

# Limpiar
docker rm -f dns_test1 dns_test2
docker network rm test_net
```

### Ejercicio 3 — Red host

```bash
# Ejecutar con red host (solo Linux nativo)
docker run --rm --network host alpine ip addr
# Muestra las interfaces del HOST

# Comparar con bridge
docker run --rm alpine ip addr
# Muestra solo eth0 (virtual) y lo

# Servidor en red host — escucha directamente en el host
docker run -d --name host_nginx --network host nginx
curl http://localhost  # Funciona sin -p
ss -tlnp | grep :80   # nginx escuchando directamente

# Limpiar
docker rm -f host_nginx
```
