# Lab de red multi-contenedor

## Índice

1. [Objetivo del lab](#objetivo-del-lab)
2. [Arquitectura de referencia](#arquitectura-de-referencia)
3. [Escenario 1 — Aplicación web de tres capas](#escenario-1--aplicación-web-de-tres-capas)
4. [Escenario 2 — Microservicios con redes segmentadas](#escenario-2--microservicios-con-redes-segmentadas)
5. [Escenario 3 — DMZ con proxy inverso](#escenario-3--dmz-con-proxy-inverso)
6. [Escenario 4 — Simulación de red corporativa](#escenario-4--simulación-de-red-corporativa)
7. [Diagnóstico de conectividad](#diagnóstico-de-conectividad)
8. [Errores comunes](#errores-comunes)
9. [Cheatsheet](#cheatsheet)
10. [Ejercicios](#ejercicios)

---

## Objetivo del lab

Este lab integra todo lo visto en B09 sobre redes: el modelo TCP/IP, DNS, configuración de red, firewalls, diagnóstico y networking de Docker. El objetivo es construir topologías realistas usando Docker Compose donde los contenedores simulan servidores, clientes y segmentos de red separados, y practicar con herramientas de diagnóstico dentro de estas topologías.

La premisa pedagógica es: **si puedes diseñar y diagnosticar una red multi-contenedor, puedes transferir esas habilidades a redes reales**, porque Docker utiliza los mismos mecanismos del kernel (namespaces de red, veth pairs, bridges, iptables/nftables, NAT) que forman la base del networking en Linux.

---

## Arquitectura de referencia

A lo largo de los escenarios usaremos esta convención visual:

```
    ┌─────────────────────────────────────────────────────┐
    │                    HOST (Docker)                     │
    │                                                     │
    │  ┌──────────┐    red_frontend    ┌──────────┐       │
    │  │  nginx   │◄──────────────────►│   app    │       │
    │  │ (proxy)  │   172.20.1.0/24    │ (backend)│       │
    │  └────┬─────┘                    └────┬─────┘       │
    │       │                               │             │
    │       │ -p 80:80                      │             │
    │       │ -p 443:443           red_backend            │
    │       │                      172.20.2.0/24          │
    │       │                               │             │
    │       │                          ┌────┴─────┐       │
    │       │                          │   db     │       │
    │       │                          │ (datos)  │       │
    │       │                          └──────────┘       │
    │       │                                             │
    └───────┼─────────────────────────────────────────────┘
            │
         Internet
```

Principios de diseño:

- **Segmentación por capas**: cada capa tiene su propia red Docker
- **Mínimo privilegio**: solo el proxy publica puertos al host
- **Conexión selectiva**: un contenedor pertenece a múltiples redes solo si necesita comunicar capas
- **DNS integrado**: los contenedores se resuelven por nombre de servicio dentro de cada red

---

## Escenario 1 — Aplicación web de tres capas

### Diseño de la topología

Este es el patrón más común en producción: proxy → aplicación → base de datos, cada par en su propia red.

```
                    ┌──────────────────────────────────┐
                    │         red_frontend             │
                    │        172.20.1.0/24             │
                    │                                  │
                    │  ┌─────────┐    ┌─────────┐     │
                    │  │  nginx  │    │   app   │     │
                    │  │  :80    │───►│  :5000  │     │
                    │  └─────────┘    └────┬────┘     │
                    │                      │          │
                    └──────────────────────┼──────────┘
                                           │
                    ┌──────────────────────┼──────────┐
                    │                      │          │
                    │  ┌────┴────┐    ┌─────────┐     │
                    │  │   app  │    │  redis  │     │
                    │  │  :5000 │───►│  :6379  │     │
                    │  └────────┘    └─────────┘     │
                    │                                  │
                    │         red_backend              │
                    │        172.20.2.0/24             │
                    └──────────────────────────────────┘
```

Observa que `app` está conectado a **ambas** redes. Esto es lo que permite que actúe como puente entre capas. `nginx` no puede ver `redis`, y `redis` no puede ver `nginx`.

### Docker Compose

```yaml
# docker-compose.yml — Escenario 1: tres capas
services:
  nginx:
    image: nginx:alpine
    ports:
      - "8080:80"
    volumes:
      - ./nginx.conf:/etc/nginx/conf.d/default.conf:ro
    networks:
      - frontend
    depends_on:
      - app

  app:
    image: python:3.12-alpine
    command: >
      sh -c "pip install flask redis &&
             python /app/server.py"
    volumes:
      - ./app:/app
    networks:
      - frontend
      - backend
    environment:
      - REDIS_HOST=redis

  redis:
    image: redis:7-alpine
    networks:
      - backend
    volumes:
      - redis_data:/data

networks:
  frontend:
    driver: bridge
    ipam:
      config:
        - subnet: 172.20.1.0/24
          gateway: 172.20.1.1
  backend:
    driver: bridge
    ipam:
      config:
        - subnet: 172.20.2.0/24
          gateway: 172.20.2.1

volumes:
  redis_data:
```

### Archivos auxiliares

**nginx.conf** — proxy inverso hacia la app:

```nginx
server {
    listen 80;
    location / {
        proxy_pass http://app:5000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

**app/server.py** — mini API con contador Redis:

```python
from flask import Flask, jsonify
import redis
import os
import socket

app = Flask(__name__)
r = redis.Redis(host=os.environ.get("REDIS_HOST", "redis"), port=6379)

@app.route("/")
def index():
    count = r.incr("hits")
    return jsonify(
        message="Hello from the app layer",
        hits=count,
        hostname=socket.gethostname()
    )

@app.route("/health")
def health():
    try:
        r.ping()
        return jsonify(status="healthy"), 200
    except redis.ConnectionError:
        return jsonify(status="unhealthy"), 503

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
```

### Verificación paso a paso

Levanta el entorno y verifica la segmentación:

```bash
# 1. Levantar
docker compose up -d

# 2. Verificar que las tres redes existen
docker network ls | grep -E "frontend|backend"

# 3. Verificar la asignación de IPs
docker compose exec app ip addr show

# Pregunta de predicción: ¿cuántas interfaces de red verás en "app"?
# Respuesta: 3 — eth0 (frontend), eth1 (backend), y lo (loopback)
```

### Prueba de aislamiento

```bash
# Desde nginx, intentar contactar redis directamente
docker compose exec nginx sh -c "apk add --no-cache curl > /dev/null 2>&1; \
  curl -s --connect-timeout 2 redis:6379 || echo 'BLOQUEADO: no puede resolver redis'"

# Pregunta de predicción: ¿qué ocurre?
# Respuesta: falla con error de resolución DNS. nginx está solo en "frontend",
# y redis está solo en "backend". El DNS embebido de Docker solo resuelve
# nombres de contenedores que comparten red.

# Desde app, verificar que SÍ puede contactar ambos
docker compose exec app sh -c "ping -c1 nginx && ping -c1 redis"
# Ambos responden porque app está en las dos redes
```

### Prueba funcional

```bash
# Acceder a la app a través del proxy
curl http://localhost:8080/
# {"hits": 1, "hostname": "abc123", "message": "Hello from the app layer"}

curl http://localhost:8080/
# {"hits": 2, ...}  ← el contador persiste en Redis

curl http://localhost:8080/health
# {"status": "healthy"}
```

---

## Escenario 2 — Microservicios con redes segmentadas

### Diseño

Tres servicios que se comunican en pares, no todos con todos:

```
    ┌─────────────────────────────────────────────────────┐
    │                                                     │
    │   red_orders (172.20.10.0/24)                       │
    │   ┌──────────┐         ┌──────────┐                 │
    │   │  orders  │◄───────►│ inventory│                 │
    │   │  :8001   │         │  :8002   │                 │
    │   └────┬─────┘         └──────────┘                 │
    │        │                                            │
    │   red_payments (172.20.11.0/24)                     │
    │        │                                            │
    │   ┌────┴─────┐         ┌──────────┐                 │
    │   │  orders  │◄───────►│ payments │                 │
    │   │  :8001   │         │  :8003   │                 │
    │   └──────────┘         └──────────┘                 │
    │                                                     │
    └─────────────────────────────────────────────────────┘

    Resultado:
    - orders ↔ inventory  ✓ (red_orders)
    - orders ↔ payments   ✓ (red_payments)
    - inventory ↔ payments ✗ (redes distintas, sin puente)
```

### Docker Compose

```yaml
# docker-compose.yml — Escenario 2: microservicios segmentados
services:
  orders:
    image: nginx:alpine
    networks:
      - orders_net
      - payments_net

  inventory:
    image: nginx:alpine
    networks:
      - orders_net

  payments:
    image: nginx:alpine
    networks:
      - payments_net

networks:
  orders_net:
    driver: bridge
    ipam:
      config:
        - subnet: 172.20.10.0/24
  payments_net:
    driver: bridge
    ipam:
      config:
        - subnet: 172.20.11.0/24
```

### Verificación de conectividad

```bash
docker compose up -d

# orders → inventory: debe funcionar
docker compose exec orders ping -c2 inventory
# PING inventory (172.20.10.x): 56 data bytes → OK

# orders → payments: debe funcionar
docker compose exec orders ping -c2 payments
# PING payments (172.20.11.x): 56 data bytes → OK

# inventory → payments: debe fallar
docker compose exec inventory ping -c2 -W2 payments
# ping: bad address 'payments' → No puede resolver, redes distintas
```

### Reflexión arquitectónica

Este patrón es fundamental en microservicios: **cada par de servicios que necesita comunicarse comparte una red, y los servicios que no deben comunicarse están aislados por diseño**. Si mañana `inventory` necesita hablar con `payments`, creas una tercera red y conectas a ambos — el principio de mínimo privilegio aplicado a la red.

---

## Escenario 3 — DMZ con proxy inverso

### Diseño

Simula una topología empresarial: una zona desmilitarizada (DMZ) expuesta al exterior y una zona interna protegida.

```
     Internet
         │
         │ :80, :443
    ┌────┼────────────────────────────────────────────┐
    │    │        red_dmz (172.20.20.0/24)            │
    │    │                                            │
    │  ┌─┴───────┐                  ┌──────────┐     │
    │  │  proxy  │─────────────────►│   web    │     │
    │  │ (nginx) │                  │ (app)    │     │
    │  └─────────┘                  └────┬─────┘     │
    │                                    │           │
    │ ┌──────────────────────────────────┼─────────┐ │
    │ │      red_internal                │         │ │
    │ │     172.20.21.0/24               │         │ │
    │ │  internal: true (sin Internet)   │         │ │
    │ │                                  │         │ │
    │ │  ┌────┴─────┐    ┌──────────┐   │         │ │
    │ │  │   web    │───►│   db     │   │         │ │
    │ │  │  (app)   │    │ (postgres)│  │         │ │
    │ │  └──────────┘    └──────────┘   │         │ │
    │ │                                  │         │ │
    │ └──────────────────────────────────┘─────────┘ │
    │                                                 │
    └─────────────────────────────────────────────────┘
```

Puntos clave:

- Solo `proxy` publica puertos al host
- `web` conecta DMZ ↔ interna (como el contenedor `app` en el Escenario 1)
- `red_internal` es **internal: true** — sin acceso a Internet
- `db` está completamente aislada del exterior

### Docker Compose

```yaml
# docker-compose.yml — Escenario 3: DMZ
services:
  proxy:
    image: nginx:alpine
    ports:
      - "80:80"
    volumes:
      - ./proxy.conf:/etc/nginx/conf.d/default.conf:ro
    networks:
      dmz:
        ipv4_address: 172.20.20.10

  web:
    image: python:3.12-alpine
    command: >
      sh -c "pip install flask psycopg2-binary &&
             python /app/web.py"
    volumes:
      - ./webapp:/app
    networks:
      dmz:
        ipv4_address: 172.20.20.20
      internal:
        ipv4_address: 172.20.21.20
    environment:
      - DB_HOST=db
      - DB_NAME=appdb
      - DB_USER=appuser
      - DB_PASS=secretpass

  db:
    image: postgres:16-alpine
    networks:
      internal:
        ipv4_address: 172.20.21.30
    environment:
      - POSTGRES_DB=appdb
      - POSTGRES_USER=appuser
      - POSTGRES_PASSWORD=secretpass
    volumes:
      - pg_data:/var/lib/postgresql/data

networks:
  dmz:
    driver: bridge
    ipam:
      config:
        - subnet: 172.20.20.0/24
          gateway: 172.20.20.1
  internal:
    driver: bridge
    internal: true   # ← sin acceso a Internet
    ipam:
      config:
        - subnet: 172.20.21.0/24
          gateway: 172.20.21.1

volumes:
  pg_data:
```

### Verificación de aislamiento

```bash
docker compose up -d

# 1. Verificar que db NO tiene acceso a Internet
docker compose exec db ping -c2 -W2 8.8.8.8
# ping: sendto: Network is unreachable   ← internal: true bloquea

# 2. Verificar que proxy NO puede alcanzar db
docker compose exec proxy ping -c2 -W2 172.20.21.30
# No route to host   ← redes diferentes, sin puente

# 3. Verificar que web SÍ puede alcanzar ambos
docker compose exec web ping -c1 proxy
docker compose exec web ping -c1 db
# Ambos responden

# 4. Verificar que web tampoco tiene Internet (está en internal)
# Pregunta de predicción: ¿web puede acceder a Internet?
# Respuesta: SÍ puede. Un contenedor tiene acceso a Internet si CUALQUIERA
# de sus redes lo permite. web está en dmz (normal) e internal (sin Internet),
# pero dmz le da salida. Solo db (exclusivamente en internal) está bloqueada.
```

> **Punto sutil**: `internal: true` no es un firewall en el contenedor, es una propiedad de la **red**. Un contenedor multi-red tiene Internet si al menos una de sus redes no es internal.

### Verificar reglas de iptables generadas

```bash
# Ver las cadenas DOCKER-ISOLATION que crea Docker
sudo iptables -L DOCKER-ISOLATION-STAGE-1 -v -n
sudo iptables -L DOCKER-ISOLATION-STAGE-2 -v -n

# En la red internal, Docker NO crea regla MASQUERADE
sudo iptables -t nat -L POSTROUTING -v -n | grep 172.20.21
# Sin resultados ← así se implementa internal: true
```

---

## Escenario 4 — Simulación de red corporativa

### Diseño

El escenario más complejo: simula una empresa con oficina, servidores y monitorización.

```
    ┌─────────────────────────────────────────────────────────────┐
    │                                                             │
    │  red_office (172.20.30.0/24)     red_servers (172.20.31.0/24)
    │                                                             │
    │  ┌──────────┐  ┌──────────┐      ┌──────────┐ ┌─────────┐  │
    │  │ client1  │  │ client2  │      │   api    │ │   db    │  │
    │  │ (curl)   │  │ (curl)   │      │ (nginx)  │ │ (redis) │  │
    │  └────┬─────┘  └────┬─────┘      └────┬─────┘ └────┬────┘  │
    │       │              │                 │            │       │
    │       └──────┬───────┘                 └─────┬──────┘       │
    │              │                               │              │
    │         ┌────┴─────┐                    ┌────┴─────┐        │
    │         │ gateway  │◄──────────────────►│ gateway  │        │
    │         │ (router) │  red_backbone      │ (router) │        │
    │         │          │  172.20.32.0/24    │          │        │
    │         └──────────┘                    └──────────┘        │
    │                              │                               │
    │                         ┌────┴─────┐                        │
    │                         │ monitor  │                        │
    │                         │(netshoot)│                        │
    │                         └──────────┘                        │
    │                                                             │
    │          red_monitoring (172.20.33.0/24)                    │
    │          conecta a: monitor + api + gateway_office          │
    │                                                             │
    └─────────────────────────────────────────────────────────────┘
```

### Docker Compose

```yaml
# docker-compose.yml — Escenario 4: red corporativa
services:
  # --- Oficina ---
  client1:
    image: nicolaka/netshoot
    command: sleep infinity
    networks:
      - office

  client2:
    image: nicolaka/netshoot
    command: sleep infinity
    networks:
      - office

  gateway_office:
    image: nicolaka/netshoot
    command: sleep infinity
    cap_add:
      - NET_ADMIN
    sysctls:
      - net.ipv4.ip_forward=1
    networks:
      office:
        ipv4_address: 172.20.30.1
      backbone:
        ipv4_address: 172.20.32.10
      monitoring:
        ipv4_address: 172.20.33.10

  # --- Servidores ---
  api:
    image: nginx:alpine
    networks:
      servers:
        ipv4_address: 172.20.31.10
      monitoring:
        ipv4_address: 172.20.33.20

  db:
    image: redis:7-alpine
    networks:
      servers:
        ipv4_address: 172.20.31.20

  gateway_servers:
    image: nicolaka/netshoot
    command: sleep infinity
    cap_add:
      - NET_ADMIN
    sysctls:
      - net.ipv4.ip_forward=1
    networks:
      servers:
        ipv4_address: 172.20.31.1
      backbone:
        ipv4_address: 172.20.32.20

  # --- Monitorización ---
  monitor:
    image: nicolaka/netshoot
    command: sleep infinity
    networks:
      monitoring:
        ipv4_address: 172.20.33.30
      backbone:
        ipv4_address: 172.20.32.30

networks:
  office:
    driver: bridge
    ipam:
      config:
        - subnet: 172.20.30.0/24
          gateway: 172.20.30.254
  servers:
    driver: bridge
    internal: true
    ipam:
      config:
        - subnet: 172.20.31.0/24
          gateway: 172.20.31.254
  backbone:
    driver: bridge
    ipam:
      config:
        - subnet: 172.20.32.0/24
          gateway: 172.20.32.254
  monitoring:
    driver: bridge
    ipam:
      config:
        - subnet: 172.20.33.0/24
          gateway: 172.20.33.254
```

### Verificación de la topología

```bash
docker compose up -d

# Listar todas las redes creadas
docker network ls --filter "name=$(basename $(pwd))"

# Ver la asignación de IPs de cada contenedor
for svc in client1 client2 gateway_office api db gateway_servers monitor; do
  echo "=== $svc ==="
  docker compose exec $svc ip -4 -br addr show | grep -v "^lo"
done
```

### Pruebas de conectividad directa

```bash
# client1 → client2: misma red office ✓
docker compose exec client1 ping -c1 client2

# client1 → api: redes diferentes, sin puente ✗
docker compose exec client1 ping -c1 -W2 172.20.31.10
# Network is unreachable o timeout

# gateway_office → gateway_servers: backbone ✓
docker compose exec gateway_office ping -c1 172.20.32.20

# monitor → api: monitoring ✓
docker compose exec monitor ping -c1 172.20.33.20

# monitor → db: redes diferentes ✗
docker compose exec monitor ping -c1 -W2 172.20.31.20
# Timeout — db está solo en servers (internal)

# db → Internet: servers es internal ✗
docker compose exec db ping -c1 -W2 8.8.8.8
# Network is unreachable
```

### Configurar enrutamiento manual

Los gateways pueden enrutar tráfico entre oficina y servidores a través del backbone:

```bash
# En gateway_office: ruta hacia la red de servidores vía gateway_servers
docker compose exec gateway_office \
  ip route add 172.20.31.0/24 via 172.20.32.20

# En gateway_servers: ruta hacia la red de oficina vía gateway_office
docker compose exec gateway_servers \
  ip route add 172.20.30.0/24 via 172.20.32.10

# En client1: ruta por defecto hacia gateway_office
docker compose exec client1 \
  ip route add 172.20.31.0/24 via 172.20.30.1

# Ahora client1 puede llegar a api a través de los gateways
docker compose exec client1 ping -c2 172.20.31.10

# Pregunta de predicción: ¿funcionará?
# Respuesta: depende. Docker aísla redes con iptables (DOCKER-ISOLATION).
# Aunque las rutas IP estén configuradas, Docker puede bloquear el tráfico
# entre bridges. Esto muestra un punto importante: Docker impone aislamiento
# a nivel L3/L4, no solo a nivel L2.
```

### Observar el tráfico con tcpdump

```bash
# En una terminal: capturar tráfico en el backbone
docker compose exec gateway_office tcpdump -i eth1 -n

# En otra terminal: generar tráfico
docker compose exec gateway_office ping -c3 172.20.32.20

# Verás los paquetes ICMP cruzando el backbone
# 172.20.32.10 > 172.20.32.20: ICMP echo request
# 172.20.32.20 > 172.20.32.10: ICMP echo reply
```

### Monitorización

```bash
# Desde monitor, escanear qué servicios están accesibles
docker compose exec monitor nmap -sn 172.20.33.0/24
# Debe encontrar: gateway_office (.10), api (.20), monitor (.30)

# Verificar el puerto 80 de api
docker compose exec monitor curl -s http://172.20.33.20
# Respuesta de nginx ← api es accesible desde monitoring
```

---

## Diagnóstico de conectividad

### Metodología sistemática

Cuando algo no conecta en una topología multi-contenedor, sigue este flujo:

```
    ¿Pueden verse?
         │
    ┌────┴────┐
    │  ping   │
    │ por IP  │
    └────┬────┘
         │
    ┌────┴──────────────────────┐
    │                           │
   Sí                         No
    │                           │
    │                    ¿Misma red Docker?
    │                    docker network inspect
    │                           │
    │                    ┌──────┴──────┐
    │                   Sí            No
    │                    │             │
    │              Verificar      Necesitan
    │              iptables       red compartida
    │              o driver       o routing
    │                    │
    │              iptables -L DOCKER-ISOLATION*
    │
    ¿Resuelve DNS?
    ping por nombre
         │
    ┌────┴──────────────────────┐
    │                           │
   Sí                         No
    │                     ¿Red user-defined?
    │                     (default bridge no
    │                      tiene DNS)
    │
    ¿El servicio responde?
    curl / nc -zv
         │
    ┌────┴──────────────────────┐
    │                           │
   Sí                         No
    │                     Verificar:
   OK                    - ¿El proceso escucha?
                           ss -tlnp dentro
                         - ¿Escucha en 0.0.0.0?
                           (no en 127.0.0.1)
                         - ¿Puerto correcto?
```

### Herramientas de diagnóstico dentro de contenedores

La imagen `nicolaka/netshoot` es esencial para diagnóstico — incluye todas las herramientas de red:

```bash
# Lanzar netshoot en una red específica para diagnosticar
docker run --rm -it --network <nombre_red> nicolaka/netshoot

# O adjuntar netshoot al namespace de red de un contenedor existente
docker run --rm -it --network container:<nombre_contenedor> nicolaka/netshoot
```

Herramientas disponibles en netshoot y cuándo usarlas:

| Herramienta | Cuándo usar |
|---|---|
| `ping` | Conectividad L3 básica |
| `dig` / `nslookup` | Resolución DNS entre contenedores |
| `curl` / `wget` | Conectividad L7 (HTTP) |
| `nc -zv` | Verificar si un puerto TCP está abierto |
| `ss -tlnp` | Ver qué puertos escucha un proceso |
| `ip addr` / `ip route` | Verificar IPs y rutas |
| `tcpdump` | Capturar y analizar tráfico |
| `nmap` | Descubrir servicios en una red |
| `traceroute` | Seguir la ruta entre contenedores |
| `iperf3` | Medir rendimiento de red |

### Ejemplo de diagnóstico completo

Problema: "la app no puede conectar con la base de datos."

```bash
# Paso 1: ¿En qué redes están?
docker inspect app --format '{{range $k,$v := .NetworkSettings.Networks}}{{$k}} {{end}}'
docker inspect db --format '{{range $k,$v := .NetworkSettings.Networks}}{{$k}} {{end}}'
# Si no comparten red → ahí está el problema

# Paso 2: ¿Qué IP tiene db?
docker inspect db --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'

# Paso 3: ¿Resuelve el nombre?
docker compose exec app nslookup db
# Si falla → ¿es default bridge? (no tiene DNS)
# Si falla en user-defined → ¿es el nombre correcto? (nombre de servicio, no de contenedor)

# Paso 4: ¿Llega el ping?
docker compose exec app ping -c1 db

# Paso 5: ¿El puerto está abierto?
docker compose exec app nc -zv db 5432
# Connection to db 5432 port [tcp/postgresql] succeeded!
# Si falla → db no está escuchando o escucha en 127.0.0.1

# Paso 6: ¿Qué escucha db?
docker compose exec db ss -tlnp
# LISTEN  0  128  0.0.0.0:5432  0.0.0.0:*
# Si dice 127.0.0.1:5432 → solo acepta conexiones locales

# Paso 7: ¿Hay filtrado?
docker compose exec app tcpdump -i eth0 -n host <ip_db> port 5432 &
docker compose exec app nc -zv db 5432
# Si ves SYN pero no SYN-ACK → firewall o proceso no responde
```

---

## Errores comunes

### 1. Usar la red bridge por defecto para comunicación entre servicios

```yaml
# MAL — sin definir networks, Docker Compose crea una red default
# pero si mezclas con docker run, la default bridge no tiene DNS
services:
  app:
    image: myapp
  db:
    image: postgres
    # Sin networks: definido, van a la red default del proyecto
    # Funciona en Compose, pero no por las razones que crees
```

```yaml
# BIEN — redes explícitas con propósito claro
services:
  app:
    image: myapp
    networks:
      - backend
  db:
    image: postgres
    networks:
      - backend

networks:
  backend:
    driver: bridge
```

**Por qué importa**: Docker Compose crea automáticamente una red `<proyecto>_default` que sí tiene DNS. Pero depender de esto implícitamente hace que la topología sea invisible al leer el archivo. Redes explícitas documentan la arquitectura.

### 2. Olvidar conectar un contenedor a múltiples redes

```yaml
# MAL — el contenedor "app" solo está en frontend, no puede alcanzar db
services:
  proxy:
    networks: [frontend]
  app:
    networks: [frontend]    # ← falta backend
  db:
    networks: [backend]
```

```yaml
# BIEN — app es el puente entre capas
services:
  proxy:
    networks: [frontend]
  app:
    networks: [frontend, backend]    # ← conectado a ambas
  db:
    networks: [backend]
```

### 3. Suponer que `internal: true` aísla completamente un contenedor

```yaml
networks:
  segura:
    internal: true
```

```bash
# Si un contenedor está en "segura" Y en otra red normal:
docker compose exec web curl -s https://example.com
# ¡FUNCIONA! — usa la otra red para salir a Internet

# Solo está completamente aislado si TODAS sus redes son internal
```

### 4. Conflictos de subnet con la red del host

```yaml
# MAL — si tu red doméstica usa 192.168.1.0/24
networks:
  mynet:
    ipam:
      config:
        - subnet: 192.168.1.0/24   # ← CONFLICTO con tu LAN
```

```yaml
# BIEN — usar rangos que no colisionen
networks:
  mynet:
    ipam:
      config:
        - subnet: 172.20.0.0/24   # ← rango privado diferente
```

**Diagnóstico**: si tras `docker compose up` pierdes acceso a equipos de tu LAN, probablemente hay un conflicto de subnet. Revisa con `ip route` y busca rutas duplicadas.

### 5. Esperar que `depends_on` garantice que el servicio está listo

```yaml
services:
  app:
    depends_on:
      - db    # ← solo espera a que el contenedor ARRANQUE, no a que
              #    el servicio esté listo para aceptar conexiones
```

```yaml
# BIEN — usar healthcheck + condition
services:
  db:
    image: postgres:16-alpine
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U appuser -d appdb"]
      interval: 5s
      timeout: 3s
      retries: 5

  app:
    depends_on:
      db:
        condition: service_healthy   # ← espera al healthcheck
```

---

## Cheatsheet

```bash
# === REDES ===
docker network ls                          # listar redes
docker network create --driver bridge \
  --subnet 172.20.X.0/24 mi_red           # crear red con subnet
docker network create --internal segura    # red sin Internet
docker network connect mi_red contenedor  # conectar en caliente
docker network disconnect mi_red contenedor
docker network inspect mi_red             # ver contenedores y config

# === DIAGNÓSTICO ===
# Ver redes de un contenedor
docker inspect <ctn> -f '{{json .NetworkSettings.Networks}}' | jq

# Ver IP de un contenedor en una red específica
docker inspect <ctn> -f '{{(index .NetworkSettings.Networks "mi_red").IPAddress}}'

# Lanzar netshoot en una red
docker run --rm -it --network mi_red nicolaka/netshoot

# Lanzar netshoot en el namespace de un contenedor
docker run --rm -it --network container:<ctn> nicolaka/netshoot

# DNS entre contenedores
docker compose exec <svc> nslookup <otro_svc>
docker compose exec <svc> dig <otro_svc>

# Verificar puerto abierto
docker compose exec <svc> nc -zv <destino> <puerto>

# Capturar tráfico
docker compose exec <svc> tcpdump -i eth0 -n port 80

# === DOCKER COMPOSE REDES ===
docker compose up -d                       # levantar
docker compose down                        # parar y eliminar redes
docker compose down -v                     # + eliminar volúmenes
docker compose exec <svc> <cmd>            # ejecutar en servicio
docker compose logs <svc>                  # ver logs
docker compose ps                          # estado de servicios

# === IPTABLES DOCKER ===
sudo iptables -L DOCKER-ISOLATION-STAGE-1 -v -n   # aislamiento
sudo iptables -L DOCKER-ISOLATION-STAGE-2 -v -n
sudo iptables -t nat -L POSTROUTING -v -n          # NAT/masquerade
sudo iptables -L DOCKER-USER -v -n                 # reglas usuario

# === LIMPIEZA ===
docker compose down --remove-orphans       # limpiar huérfanos
docker network prune                       # eliminar redes no usadas
docker system prune                        # limpieza general
```

---

## Ejercicios

### Ejercicio 1 — Depuración de topología rota

Tienes este Compose que debería permitir que `web` acceda a `api` y que `api` acceda a `db`, pero algo no funciona:

```yaml
services:
  web:
    image: nicolaka/netshoot
    command: sleep infinity
    networks:
      - frontend

  api:
    image: nginx:alpine
    networks:
      - frontend

  db:
    image: redis:7-alpine
    networks:
      - backend

networks:
  frontend:
    driver: bridge
  backend:
    driver: bridge
```

Tareas:

1. Levanta el entorno y verifica qué conexiones funcionan y cuáles no
2. Identifica el error en la topología
3. Corrige el archivo para que `web → api → db` funcione pero `web → db` no
4. Verifica la corrección

<details>
<summary>Pista</summary>

¿En cuántas redes está `api`? ¿Puede alcanzar `db`?

</details>

<details>
<summary>Solución</summary>

El problema es que `api` solo está en `frontend`, no puede alcanzar `db` en `backend`. Corrección:

```yaml
  api:
    image: nginx:alpine
    networks:
      - frontend
      - backend    # ← añadir backend
```

Verificación:

```bash
docker compose exec web ping -c1 api        # ✓ misma red frontend
docker compose exec api ping -c1 db          # ✓ misma red backend
docker compose exec web ping -c1 -W2 db      # ✗ redes diferentes
```

</details>

---

### Ejercicio 2 — Construir un entorno de staging

Construye una topología con Docker Compose que simule:

- Un **load balancer** (nginx) que publica puerto 80 al host
- Dos instancias de **app** (nginx con configuración diferente)
- Una base de datos **redis** compartida por las dos apps
- La base de datos **no debe tener acceso a Internet**
- El load balancer **no debe poder acceder directamente** a Redis

Requisitos:

1. Define las redes necesarias con subnets explícitas
2. Escribe el `docker-compose.yml` completo
3. Incluye la configuración de nginx para balancear entre las dos apps
4. Verifica: load balancer ↔ apps ✓, apps ↔ redis ✓, lb ↔ redis ✗, redis → Internet ✗

<details>
<summary>Pista</summary>

Necesitas dos redes: una para lb↔apps, otra (internal) para apps↔redis. Ambas apps deben estar en ambas redes. Para el balanceo, usa `upstream` en nginx con los nombres de servicio.

</details>

<details>
<summary>Solución</summary>

```yaml
services:
  lb:
    image: nginx:alpine
    ports:
      - "80:80"
    volumes:
      - ./lb.conf:/etc/nginx/conf.d/default.conf:ro
    networks:
      - front

  app1:
    image: nginx:alpine
    networks:
      - front
      - data

  app2:
    image: nginx:alpine
    networks:
      - front
      - data

  redis:
    image: redis:7-alpine
    networks:
      - data

networks:
  front:
    driver: bridge
    ipam:
      config:
        - subnet: 172.20.50.0/24
  data:
    driver: bridge
    internal: true
    ipam:
      config:
        - subnet: 172.20.51.0/24
```

**lb.conf:**

```nginx
upstream apps {
    server app1:80;
    server app2:80;
}
server {
    listen 80;
    location / {
        proxy_pass http://apps;
    }
}
```

Verificación:

```bash
# lb → apps
docker compose exec lb curl -s http://app1
docker compose exec lb curl -s http://app2

# apps → redis
docker compose exec app1 nc -zv redis 6379
docker compose exec app2 nc -zv redis 6379

# lb → redis (debe fallar)
docker compose exec lb sh -c "nc -zv redis 6379 2>&1 || echo BLOCKED"

# redis → Internet (debe fallar)
docker compose exec redis ping -c1 -W2 8.8.8.8
```

</details>

---

### Ejercicio 3 — Diagnóstico con netshoot

Levanta el Escenario 1 (tres capas) y realiza estas tareas de diagnóstico:

1. Lanza un contenedor `netshoot` en la red `frontend` y descubre qué contenedores están presentes usando `nmap -sn`
2. Desde `netshoot`, verifica la resolución DNS de cada servicio con `dig`
3. Captura con `tcpdump` el tráfico entre nginx y app cuando haces `curl localhost:8080` desde el host
4. Mide la latencia entre nginx y app con `ping` — compara con la latencia a una IP externa
5. Verifica con `ss` qué puertos escucha el contenedor `app`

<details>
<summary>Pista</summary>

Para adjuntar netshoot a una red existente usa `docker run --rm -it --network <proyecto>_frontend nicolaka/netshoot`. El nombre de la red incluye el prefijo del proyecto de Compose (generalmente el nombre del directorio).

</details>

<details>
<summary>Solución</summary>

```bash
# Obtener el nombre exacto de la red
docker network ls | grep frontend
# Ejemplo: escenario1_frontend

# 1. Descubrimiento
docker run --rm -it --network escenario1_frontend nicolaka/netshoot \
  nmap -sn 172.20.1.0/24
# Host is up: 172.20.1.1 (gateway), .2 (nginx), .3 (app)

# 2. Resolución DNS
docker run --rm -it --network escenario1_frontend nicolaka/netshoot \
  dig nginx
# ;; ANSWER SECTION:
# nginx.  600  IN  A  172.20.1.x

docker run --rm -it --network escenario1_frontend nicolaka/netshoot \
  dig app
# ;; ANSWER SECTION:
# app.  600  IN  A  172.20.1.x

# 3. Captura de tráfico (en una terminal)
docker compose exec nginx sh -c "apk add --no-cache tcpdump && \
  tcpdump -i eth0 -n port 5000"
# En otra terminal:
curl http://localhost:8080/
# Verás: nginx_ip.xxxxx > app_ip.5000: Flags [S], ...

# 4. Latencia
docker compose exec nginx ping -c10 app
# rtt min/avg/max = ~0.050/0.080/0.150 ms  ← microsegundos, misma máquina

# 5. Puertos escuchando
docker compose exec app ss -tlnp
# LISTEN  0  128  0.0.0.0:5000  0.0.0.0:*  users:(("python",pid=1,...))
```

</details>

---

### Pregunta de reflexión

> Has diseñado una topología donde un contenedor `gateway` conecta dos redes Docker (como en el Escenario 4). Configuraste `ip_forward=1` y añadiste rutas estáticas, pero el tráfico entre redes no fluye. `tcpdump` en el gateway muestra que los paquetes llegan pero no salen por la otra interfaz. ¿Por qué Docker bloquea este tráfico a pesar del enrutamiento correcto, y cuál es la solución sin desactivar el aislamiento por completo?

> **Razonamiento esperado**: Docker crea cadenas `DOCKER-ISOLATION-STAGE-1` y `DOCKER-ISOLATION-STAGE-2` en iptables que **DROP** todo tráfico entre bridges diferentes. Esto es intencional: el aislamiento entre redes Docker es un feature de seguridad. Las rutas IP están correctas a nivel L3, pero iptables opera antes del forwarding decision. La solución es insertar reglas en la cadena `DOCKER-USER` (que se evalúa antes de `DOCKER-ISOLATION`) para permitir selectivamente el tráfico entre las subnets específicas:
>
> ```bash
> sudo iptables -I DOCKER-USER -s 172.20.30.0/24 -d 172.20.31.0/24 -j ACCEPT
> sudo iptables -I DOCKER-USER -s 172.20.31.0/24 -d 172.20.30.0/24 -j ACCEPT
> ```
>
> Esto permite el tráfico entre esas dos subnets sin desactivar el aislamiento global entre todas las redes Docker. Es el equivalente Docker de crear reglas de firewall entre VLANs en una red física.

---

> **Fin del Bloque 09 — Redes**. Este lab cierra el bloque integrando conceptos de todos los capítulos: topología TCP/IP, DNS entre contenedores, segmentación como firewall, diagnóstico con ss/tcpdump/nmap, y la mecánica interna de Docker networking (bridges, veth, iptables).
