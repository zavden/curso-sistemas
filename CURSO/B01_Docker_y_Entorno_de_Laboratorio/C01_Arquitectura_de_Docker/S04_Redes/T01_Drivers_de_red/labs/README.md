# Lab — Drivers de red

## Objetivo

Laboratorio practico para verificar de forma empirica los drivers de red de
Docker: inspeccionar las redes predeterminadas, comparar el bridge por defecto
con un bridge custom, probar las redes host y none, y gestionar conexiones
de red en caliente.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada: `docker pull alpine:latest`
- Estar en el directorio `labs/` de este topico

---

## Parte 1 — Redes predeterminadas

### Objetivo

Identificar las tres redes que Docker crea al instalarse, entender que driver
usa cada una, e inspeccionar la configuracion de la red bridge por defecto.

### Paso 1.1: Listar las redes existentes

```bash
docker network ls
```

Salida esperada:

```
NETWORK ID     NAME      DRIVER    SCOPE
<hash>         bridge    bridge    local
<hash>         host      host      local
<hash>         none      null      local
```

Estas tres redes existen siempre. Docker las crea al instalarse y no se
pueden eliminar.

### Paso 1.2: Inspeccionar la red bridge por defecto

```bash
docker network inspect bridge
```

Salida esperada (simplificada):

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
        "Containers": {},
        "Options": {
            "com.docker.network.bridge.name": "docker0"
        }
    }
]
```

Observa:
- **Subnet**: rango de IPs disponibles para contenedores en esta red
- **Gateway**: IP del bridge en el host (la interfaz `docker0`)
- **Containers**: vacio si no hay contenedores ejecutandose en esta red
- **Options**: el nombre de la interfaz de red en el host es `docker0`

### Paso 1.3: Ver la interfaz docker0 en el host

```bash
ip addr show docker0
```

Salida esperada (parcial):

```
docker0: <NO-CARRIER,BROADCAST,MULTICAST,UP> ...
    inet 172.17.0.1/16 brd 172.17.255.255 scope global docker0
```

La IP coincide con el Gateway que vimos en `docker network inspect`. Esta
interfaz es el puente virtual al que se conectan los contenedores.

### Paso 1.4: Inspeccionar la red host

```bash
docker network inspect host --format '{{.Driver}}'
```

Salida esperada:

```
host
```

La red host no tiene configuracion IPAM (sin subred ni gateway) porque
comparte directamente el network stack del host.

### Paso 1.5: Inspeccionar la red none

```bash
docker network inspect none --format '{{.Driver}}'
```

Salida esperada:

```
null
```

El driver `null` significa: sin red. Los contenedores en esta red solo tienen
la interfaz loopback.

---

## Parte 2 — Bridge por defecto vs bridge custom

### Objetivo

Demostrar la diferencia critica entre la red bridge por defecto y una red
bridge custom: la resolucion DNS. Los contenedores en el bridge por defecto
**no pueden** encontrarse por nombre, mientras que en un bridge custom **si**.

### Paso 2.1: Dos contenedores en el bridge por defecto

```bash
docker run -d --name bridge-a alpine sleep 300
docker run -d --name bridge-b alpine sleep 300
```

Ambos se conectan automaticamente a la red bridge por defecto porque no se
especifico `--network`.

### Paso 2.2: Verificar que estan en la red bridge

```bash
docker network inspect bridge --format '{{range .Containers}}{{.Name}}: {{.IPv4Address}}{{"\n"}}{{end}}'
```

Salida esperada:

```
bridge-a: 172.17.0.X/16
bridge-b: 172.17.0.Y/16
```

### Paso 2.3: Intentar ping por nombre

Antes de ejecutar, predice: ¿podra `bridge-a` hacer ping a `bridge-b`
usando su nombre?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.4: Comprobar que DNS falla en el bridge por defecto

```bash
docker exec bridge-a ping -c 1 bridge-b
```

Salida esperada:

```
ping: bad address 'bridge-b'
```

El bridge por defecto **no tiene servidor DNS**. Los contenedores no pueden
resolverse por nombre. Solo pueden comunicarse por IP directa.

### Paso 2.5: Comunicacion por IP (funciona)

```bash
# Obtener la IP de bridge-b
docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' bridge-b
```

Usa la IP obtenida en el siguiente comando:

```bash
docker exec bridge-a ping -c 2 <IP_de_bridge-b>
```

Salida esperada:

```
PING <IP> (<IP>): 56 data bytes
64 bytes from <IP>: seq=0 ttl=64 time=...
64 bytes from <IP>: seq=1 ttl=64 time=...
```

Por IP si funciona. Pero depender de IPs es fragil: las IPs pueden cambiar
cuando un contenedor se reinicia.

### Paso 2.6: Crear una red bridge custom

```bash
docker network create lab-custom-bridge
```

### Paso 2.7: Verificar la red custom

```bash
docker network inspect lab-custom-bridge --format '{{.Driver}} | Subnet: {{(index .IPAM.Config 0).Subnet}} | Gateway: {{(index .IPAM.Config 0).Gateway}}'
```

Salida esperada:

```
bridge | Subnet: 172.X.0.0/16 | Gateway: 172.X.0.1
```

Docker asigna automaticamente una subred diferente a la del bridge por defecto.

### Paso 2.8: Dos contenedores en la red custom

```bash
docker run -d --name custom-a --network lab-custom-bridge alpine sleep 300
docker run -d --name custom-b --network lab-custom-bridge alpine sleep 300
```

### Paso 2.9: Ping por nombre en la red custom

Antes de ejecutar, predice: ¿funcionara el ping por nombre ahora que estan
en una red custom?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.10: Comprobar que DNS funciona en la red custom

```bash
docker exec custom-a ping -c 2 custom-b
```

Salida esperada:

```
PING custom-b (172.X.0.3): 56 data bytes
64 bytes from 172.X.0.3: seq=0 ttl=64 time=...
64 bytes from 172.X.0.3: seq=1 ttl=64 time=...
```

El nombre `custom-b` se resuelve automaticamente a su IP. La red custom
tiene un servidor DNS embebido en `127.0.0.11`.

### Paso 2.11: Verificar el DNS embebido

```bash
docker exec custom-a cat /etc/resolv.conf
```

Salida esperada:

```
nameserver 127.0.0.11
options ndots:0
```

El contenedor usa el DNS embebido de Docker (`127.0.0.11`), que resuelve
los nombres de los contenedores en la misma red.

### Paso 2.12: Limpiar

```bash
docker rm -f bridge-a bridge-b custom-a custom-b
docker network rm lab-custom-bridge
```

---

## Parte 3 — Redes host y none

### Objetivo

Probar los drivers host y none. Con host, el contenedor comparte el network
stack del host. Con none, el contenedor no tiene red.

### Paso 3.1: Red host — interfaces del host

```bash
docker run --rm --network host alpine ip addr show
```

Compara la salida con las interfaces del host:

```bash
ip addr show
```

Antes de comparar, predice: ¿las interfaces que ve el contenedor seran
las mismas que las del host, o tendra las suyas propias?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.2: Comparar con un contenedor bridge normal

```bash
docker run --rm alpine ip addr show
```

Salida esperada (parcial):

```
1: lo: <LOOPBACK,UP,LOWER_UP> ...
    inet 127.0.0.1/8 scope host lo
...
X: eth0@ifY: <BROADCAST,MULTICAST,UP,LOWER_UP> ...
    inet 172.17.0.X/16 ...
```

El contenedor bridge tiene su propia `eth0` virtual con una IP de la subred
Docker. El contenedor host ve **todas** las interfaces del host (eth0, docker0,
wlan0, etc.).

### Paso 3.3: Red host — servidor escuchando en el host

```bash
docker run -d --name host-check --network host nginx:alpine
```

```bash
curl -s http://localhost:80 | head -5
```

Salida esperada (parcial):

```html
<!DOCTYPE html>
<html>
<head>
<title>Welcome to nginx!</title>
</head>
```

nginx escucha directamente en el puerto 80 del host, sin necesidad de `-p`.

> Nota: si el puerto 80 ya esta en uso en tu host, este comando fallara.
> Puedes verificar antes con `ss -tlnp | grep :80`.

### Paso 3.4: Limpiar el contenedor host

```bash
docker rm -f host-check
```

### Paso 3.5: Red none — sin red

```bash
docker run --rm --name none-check --network none alpine ip addr show
```

Salida esperada:

```
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    inet 127.0.0.1/8 scope host lo
       valid_lft forever preferred_lft forever
```

Solo la interfaz loopback. Sin eth0, sin IP en una subred Docker.

### Paso 3.6: Verificar que no hay conectividad

```bash
docker run --rm --network none alpine ping -c 1 8.8.8.8
```

Salida esperada:

```
PING 8.8.8.8 (8.8.8.8): 56 data bytes
ping: sendto: Network unreachable
```

El contenedor esta completamente aislado de la red. Util para tareas
que procesan datos locales sin necesidad de conectividad.

---

## Parte 4 — Gestion dinamica de redes

### Objetivo

Crear redes custom, conectar un contenedor a una segunda red en caliente
(sin detenerlo), verificar sus interfaces de red, y desconectarlo.

### Paso 4.1: Crear dos redes

```bash
docker network create lab-dynamic-net
docker network create lab-second-net
```

### Paso 4.2: Crear un contenedor en la primera red

```bash
docker run -d --name dynamic-target --network lab-dynamic-net alpine sleep 300
```

### Paso 4.3: Verificar las redes del contenedor

```bash
docker inspect -f '{{range $net, $config := .NetworkSettings.Networks}}{{$net}}: {{$config.IPAddress}}{{"\n"}}{{end}}' dynamic-target
```

Salida esperada:

```
lab-dynamic-net: 172.X.0.2
```

El contenedor solo esta en una red.

### Paso 4.4: Conectar a la segunda red en caliente

Antes de ejecutar, predice: ¿sera necesario detener el contenedor para
conectarlo a una segunda red?

Intenta predecir antes de continuar al siguiente paso.

```bash
docker network connect lab-second-net dynamic-target
```

### Paso 4.5: Verificar las redes despues de conectar

```bash
docker inspect -f '{{range $net, $config := .NetworkSettings.Networks}}{{$net}}: {{$config.IPAddress}}{{"\n"}}{{end}}' dynamic-target
```

Salida esperada:

```
lab-dynamic-net: 172.X.0.2
lab-second-net: 172.Y.0.2
```

El contenedor ahora tiene **dos interfaces de red**, cada una con una IP en
su subred respectiva. No fue necesario detener el contenedor.

### Paso 4.6: Ver las interfaces de red dentro del contenedor

```bash
docker exec dynamic-target ip addr show
```

Observa que hay dos interfaces `eth` ademas de `lo`. Cada una corresponde a
una de las redes conectadas.

### Paso 4.7: Comunicacion desde la segunda red

```bash
docker run -d --name dynamic-peer --network lab-second-net alpine sleep 300

# dynamic-peer puede alcanzar a dynamic-target por nombre
docker exec dynamic-peer ping -c 2 dynamic-target
```

Salida esperada:

```
PING dynamic-target (172.Y.0.2): 56 data bytes
64 bytes from 172.Y.0.2: seq=0 ttl=64 time=...
64 bytes from 172.Y.0.2: seq=1 ttl=64 time=...
```

### Paso 4.8: Desconectar de la primera red

```bash
docker network disconnect lab-dynamic-net dynamic-target
```

### Paso 4.9: Verificar que solo queda una red

```bash
docker inspect -f '{{range $net, $config := .NetworkSettings.Networks}}{{$net}}: {{$config.IPAddress}}{{"\n"}}{{end}}' dynamic-target
```

Salida esperada:

```
lab-second-net: 172.Y.0.2
```

El contenedor ya solo esta en `lab-second-net`.

### Paso 4.10: Inspeccionar una red para ver contenedores conectados

```bash
docker network inspect lab-second-net --format '{{range .Containers}}{{.Name}}: {{.IPv4Address}}{{"\n"}}{{end}}'
```

Salida esperada:

```
dynamic-target: 172.Y.0.2/16
dynamic-peer: 172.Y.0.3/16
```

### Paso 4.11: Limpiar

```bash
docker rm -f dynamic-target dynamic-peer
docker network rm lab-dynamic-net lab-second-net
```

---

## Limpieza final

```bash
# Eliminar contenedores huerfanos (si quedo alguno por un paso saltado)
docker rm -f bridge-a bridge-b custom-a custom-b \
    host-check none-check dynamic-target dynamic-peer 2>/dev/null

# Eliminar redes del lab
docker network rm lab-custom-bridge lab-dynamic-net lab-second-net 2>/dev/null
```

---

## Conceptos reforzados

1. Docker crea tres redes al instalarse: `bridge` (driver bridge), `host`
   (driver host) y `none` (driver null). Estas redes no se pueden eliminar.

2. La red bridge por defecto (`docker0`) **no tiene DNS** entre contenedores.
   La comunicacion solo funciona por IP directa, lo que la hace fragil ante
   reinicios que cambian las IPs.

3. Una red bridge custom proporciona **DNS automatico**: los contenedores se
   resuelven por nombre a traves del servidor DNS embebido en `127.0.0.11`.

4. La red `host` elimina el aislamiento de red: el contenedor comparte
   interfaces, IPs y puertos con el host. No requiere `-p` pero los puertos
   compiten con los servicios del host.

5. La red `none` elimina toda conectividad excepto loopback. Util para
   contenedores que no necesitan red.

6. Un contenedor puede conectarse a **multiples redes** simultaneamente con
   `docker network connect`, y desconectarse con `docker network disconnect`,
   todo sin detener el contenedor.

7. `docker network inspect` muestra la subred, gateway y contenedores
   conectados a una red. Es la herramienta principal para diagnosticar
   problemas de conectividad.
