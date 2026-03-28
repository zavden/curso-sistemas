# Lab — Comunicacion entre contenedores

## Objetivo

Laboratorio practico para verificar de forma empirica como se comunican los
contenedores entre si: resolucion DNS en redes custom, aliases de red con
round-robin, contenedores conectados a multiples redes, y las limitaciones
de la red por defecto.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada: `docker pull alpine:latest`
- Estar en el directorio `labs/` de este topico

---

## Parte 1 — DNS en red custom

### Objetivo

Demostrar que los contenedores en una red bridge custom se resuelven
automaticamente por nombre a traves del servidor DNS embebido de Docker.

### Paso 1.1: Crear la red

```bash
docker network create lab-dns-net
```

### Paso 1.2: Crear dos contenedores en la red

```bash
docker run -d --name dns-server --network lab-dns-net alpine sleep 300
docker run -d --name dns-client --network lab-dns-net alpine sleep 300
```

### Paso 1.3: Resolver por nombre con ping

Antes de ejecutar, predice: ¿podra `dns-client` encontrar a `dns-server`
usando solo su nombre?

Intenta predecir antes de continuar al siguiente paso.

```bash
docker exec dns-client ping -c 2 dns-server
```

Salida esperada:

```
PING dns-server (172.X.0.2): 56 data bytes
64 bytes from 172.X.0.2: seq=0 ttl=64 time=...
64 bytes from 172.X.0.2: seq=1 ttl=64 time=...
```

El nombre `dns-server` se resuelve automaticamente a su IP.

### Paso 1.4: Verificar la resolucion DNS con nslookup

```bash
docker exec dns-client nslookup dns-server
```

Salida esperada:

```
Server:    127.0.0.11
Address:   127.0.0.11:53

Non-authoritative answer:
Name:      dns-server
Address 1: 172.X.0.2 dns-server.lab-dns-net
```

El DNS embebido de Docker escucha en `127.0.0.11` y resuelve los nombres
de todos los contenedores en la misma red.

### Paso 1.5: Verificar la configuracion DNS del contenedor

```bash
docker exec dns-client cat /etc/resolv.conf
```

Salida esperada:

```
nameserver 127.0.0.11
options ndots:0
```

Docker configura automaticamente cada contenedor en una red custom para
usar su DNS embebido como nameserver.

### Paso 1.6: Comunicacion bidireccional

```bash
docker exec dns-server ping -c 1 dns-client
```

Salida esperada:

```
PING dns-client (172.X.0.3): 56 data bytes
64 bytes from 172.X.0.3: seq=0 ttl=64 time=...
```

El DNS funciona en ambas direcciones. Cualquier contenedor en la red puede
resolver a cualquier otro por nombre.

### Paso 1.7: Limpiar

```bash
docker rm -f dns-server dns-client
docker network rm lab-dns-net
```

---

## Parte 2 — Aliases y round-robin DNS

### Objetivo

Demostrar que un contenedor puede tener multiples nombres (aliases) y que
cuando varios contenedores comparten el mismo alias, Docker distribuye las
consultas DNS entre ellos (round-robin).

### Paso 2.1: Crear la red

```bash
docker network create lab-alias-net
```

### Paso 2.2: Contenedor con multiples aliases

```bash
docker run -d --name alias-main \
    --network lab-alias-net \
    --network-alias database \
    --network-alias db \
    alpine sleep 300
```

### Paso 2.3: Resolver por nombre y por aliases

```bash
docker run --rm --network lab-alias-net alpine nslookup alias-main
docker run --rm --network lab-alias-net alpine nslookup database
docker run --rm --network lab-alias-net alpine nslookup db
```

Los tres nombres resuelven a la **misma IP**. El contenedor es alcanzable
por su nombre (`alias-main`) y por sus dos aliases (`database`, `db`).

### Paso 2.4: Limpiar el contenedor de aliases

```bash
docker rm -f alias-main
```

### Paso 2.5: Round-robin con multiples contenedores

Crear tres contenedores que comparten el mismo alias `worker`:

```bash
docker run -d --name rr-worker-1 --network lab-alias-net --network-alias worker alpine sleep 300
docker run -d --name rr-worker-2 --network lab-alias-net --network-alias worker alpine sleep 300
docker run -d --name rr-worker-3 --network lab-alias-net --network-alias worker alpine sleep 300
```

### Paso 2.6: Consultar el alias compartido

Antes de ejecutar, predice: cuando consultes el nombre `worker`, ¿cuantas
IPs devolvera el DNS?

Intenta predecir antes de continuar al siguiente paso.

```bash
docker run --rm --name rr-client --network lab-alias-net alpine nslookup worker
```

Salida esperada:

```
Server:    127.0.0.11
Address:   127.0.0.11:53

Non-authoritative answer:
Name:      worker
Address 1: 172.X.0.2 rr-worker-1.lab-alias-net
Address 2: 172.X.0.3 rr-worker-2.lab-alias-net
Address 3: 172.X.0.4 rr-worker-3.lab-alias-net
```

Docker devuelve las tres IPs. El cliente elige una (tipicamente la primera),
y Docker rota el orden en consultas sucesivas.

### Paso 2.7: Verificar la rotacion

```bash
for i in 1 2 3 4 5; do
    docker run --rm --network lab-alias-net alpine nslookup worker 2>/dev/null | grep "Address [0-9]"
    echo "---"
done
```

Observa que el orden de las IPs puede variar entre consultas. Este es el
mecanismo de round-robin DNS: no es un balanceador de carga real, pero
distribuye las solicitudes de forma basica.

### Paso 2.8: Limpiar

```bash
docker rm -f rr-worker-1 rr-worker-2 rr-worker-3
docker network rm lab-alias-net
```

---

## Parte 3 — Contenedores multi-red

### Objetivo

Demostrar que un contenedor conectado a dos redes puede comunicarse con
contenedores en ambas, mientras que contenedores en redes diferentes
permanecen aislados entre si.

### Paso 3.1: Crear dos redes aisladas

```bash
docker network create lab-frontend
docker network create lab-backend
```

### Paso 3.2: Crear los contenedores

```bash
# API en la red frontend
docker run -d --name multi-api --network lab-frontend alpine sleep 300

# Web solo en frontend
docker run -d --name multi-web --network lab-frontend alpine sleep 300

# DB solo en backend
docker run -d --name multi-db --network lab-backend alpine sleep 300
```

### Paso 3.3: Conectar API a la segunda red

```bash
docker network connect lab-backend multi-api
```

Ahora `multi-api` esta en ambas redes: `lab-frontend` y `lab-backend`.

### Paso 3.4: Verificar las redes de multi-api

```bash
docker inspect -f '{{range $net, $config := .NetworkSettings.Networks}}{{$net}}: {{$config.IPAddress}}{{"\n"}}{{end}}' multi-api
```

Salida esperada:

```
lab-frontend: 172.X.0.2
lab-backend: 172.Y.0.2
```

### Paso 3.5: Probar la comunicacion

Antes de ejecutar, predice para cada caso: ¿funcionara o fallara?

- `multi-web` hace ping a `multi-api`
- `multi-api` hace ping a `multi-db`
- `multi-web` hace ping a `multi-db`

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.6: Verificar la comunicacion

```bash
# web -> api (misma red: lab-frontend)
docker exec multi-web ping -c 1 multi-api
```

Salida esperada:

```
PING multi-api (172.X.0.2): 56 data bytes
64 bytes from 172.X.0.2: seq=0 ttl=64 time=...
```

```bash
# api -> db (misma red: lab-backend)
docker exec multi-api ping -c 1 multi-db
```

Salida esperada:

```
PING multi-db (172.Y.0.3): 56 data bytes
64 bytes from 172.Y.0.3: seq=0 ttl=64 time=...
```

```bash
# web -> db (redes diferentes, sin ruta)
docker exec multi-web ping -c 1 -W 2 multi-db 2>&1 || true
```

Salida esperada:

```
ping: bad address 'multi-db'
```

`multi-web` no puede ver a `multi-db` porque estan en redes diferentes.
Solo `multi-api`, que esta en ambas redes, puede comunicarse con los dos.

### Paso 3.7: Diagrama del resultado

```
lab-frontend                lab-backend
+----------+              +----------+
| multi-web|              | multi-db |
+----+-----+              +----+-----+
     |                         |
     |   +----------+         |
     +---| multi-api|----+----+
         +----------+
         (en ambas redes)
```

- `multi-web` solo ve a `multi-api` (comparten `lab-frontend`)
- `multi-db` solo ve a `multi-api` (comparten `lab-backend`)
- `multi-api` ve a ambos (esta en las dos redes)
- `multi-web` y `multi-db` estan completamente aislados

Este patron es fundamental para segmentar redes en produccion.

### Paso 3.8: Limpiar

```bash
docker rm -f multi-api multi-web multi-db
docker network rm lab-frontend lab-backend
```

---

## Parte 4 — Red por defecto: sin DNS

### Objetivo

Demostrar que la red bridge por defecto no tiene resolucion DNS y que los
contenedores solo pueden comunicarse por IP. Esto refuerza por que siempre
se deben usar redes custom.

### Paso 4.1: Dos contenedores en la red por defecto

```bash
docker run -d --name default-a alpine sleep 300
docker run -d --name default-b alpine sleep 300
```

Al no especificar `--network`, ambos se conectan a la red bridge por defecto.

### Paso 4.2: Intentar comunicacion por nombre

Antes de ejecutar, predice: despues de ver que las redes custom si tienen
DNS, ¿la red por defecto tambien lo tendra?

Intenta predecir antes de continuar al siguiente paso.

```bash
docker exec default-a ping -c 1 default-b
```

Salida esperada:

```
ping: bad address 'default-b'
```

### Paso 4.3: Verificar el resolv.conf de la red por defecto

```bash
docker exec default-a cat /etc/resolv.conf
```

Salida esperada:

```
nameserver X.X.X.X
```

Observa que el nameserver **no es `127.0.0.11`** como en las redes custom.
Apunta al DNS del host. El DNS embebido de Docker no se activa en la red
bridge por defecto.

### Paso 4.4: Comunicacion por IP (unica opcion)

```bash
# Obtener la IP de default-b
docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' default-b
```

```bash
# Usar la IP obtenida
docker exec default-a ping -c 1 <IP_de_default-b>
```

Salida esperada:

```
PING <IP> (<IP>): 56 data bytes
64 bytes from <IP>: seq=0 ttl=64 time=...
```

Funciona por IP, pero este enfoque es fragil: si `default-b` se reinicia,
puede recibir una IP diferente.

### Paso 4.5: Limpiar

```bash
docker rm -f default-a default-b
```

---

## Limpieza final

```bash
# Eliminar contenedores huerfanos (si quedo alguno por un paso saltado)
docker rm -f dns-server dns-client alias-main \
    rr-worker-1 rr-worker-2 rr-worker-3 rr-client \
    multi-api multi-web multi-db \
    default-a default-b 2>/dev/null

# Eliminar redes del lab
docker network rm lab-dns-net lab-alias-net lab-frontend lab-backend 2>/dev/null
```

---

## Conceptos reforzados

1. En redes bridge custom, Docker proporciona un **servidor DNS embebido**
   en `127.0.0.11` que resuelve automaticamente los nombres de contenedores
   a sus IPs. La resolucion es bidireccional.

2. `--network-alias` permite asignar nombres alternativos a un contenedor.
   Cualquiera de sus aliases resuelve a la misma IP.

3. Cuando multiples contenedores comparten el mismo alias, Docker devuelve
   todas sus IPs y rota el orden (**round-robin DNS**). No es un balanceador
   de carga real, pero distribuye solicitudes de forma basica.

4. Un contenedor conectado a **dos redes** puede comunicarse con contenedores
   en ambas, actuando como puente. Los contenedores en redes diferentes
   permanecen aislados entre si.

5. La red bridge por defecto **no tiene DNS**. Los contenedores solo pueden
   comunicarse por IP directa, lo que es fragil ante cambios de IP por
   reinicios. Siempre se deben usar redes custom.

6. La diferencia de DNS se debe a que las redes custom configuran
   `nameserver 127.0.0.11` en `/etc/resolv.conf`, mientras la red por
   defecto usa el DNS del host.
