# T02 — Redes multi-servicio

## Red default de Compose

Cuando ejecutas `docker compose up`, Compose crea automáticamente una **red bridge**
para el proyecto. Todos los servicios se conectan a esta red y pueden comunicarse
entre sí usando el **nombre del servicio** como hostname:

```yaml
# compose.yml
services:
  web:
    image: nginx:latest
  api:
    image: myapi:latest
  db:
    image: postgres:16
```

```
Red: proyecto_default
┌──────────────────────────────────────────┐
│                                          │
│  web ◀──────▶ api ◀──────▶ db           │
│                                          │
│  web puede resolver "api" y "db" por DNS │
│  api puede resolver "web" y "db" por DNS │
│  db puede resolver "web" y "api" por DNS │
│                                          │
└──────────────────────────────────────────┘
```

```bash
docker compose up -d

# Verificar la red creada
docker network ls | grep default
# abc123  proyecto_default  bridge  local

# Desde web, resolver api por nombre
docker compose exec web ping -c 1 api
# PING api (172.20.0.3): 56 data bytes — resuelve automáticamente
```

### El nombre DNS es el nombre del servicio

El hostname DNS es el nombre definido en el YAML (`web`, `api`, `db`), **no** el
nombre del contenedor (`proyecto-web-1`). Ambos funcionan, pero el nombre del
servicio es el canónico:

```bash
# Ambos resuelven al mismo contenedor
docker compose exec api ping -c 1 db            # Nombre de servicio ✓
docker compose exec api ping -c 1 proyecto-db-1  # Nombre de contenedor ✓
```

### DNS embebido (127.0.0.11)

Docker inyecta un servidor DNS embebido en cada contenedor conectado a una red
user-defined (como las de Compose). Este servidor resuelve los nombres de servicio
dentro de la red. Si un nombre no corresponde a un contenedor, lo reenvía al DNS
externo configurado (por defecto, el del host):

```bash
# Ver el DNS configurado dentro de un contenedor
docker compose exec web cat /etc/resolv.conf
# nameserver 127.0.0.11
```

> **Nota**: La red bridge **default de Docker** (no la de Compose) no tiene DNS
> embebido. Solo las redes user-defined (todas las de Compose) lo tienen.

## Redes custom para aislamiento

La red default conecta **todos** los servicios entre sí. En una arquitectura
real, esto es un problema de seguridad: el proxy web no debería poder hablar
directamente con la base de datos.

La solución es definir **redes separadas**:

```yaml
services:
  proxy:
    image: nginx:latest
    ports:
      - "80:80"
    networks:
      - frontend

  api:
    image: myapi:latest
    networks:
      - frontend
      - backend

  db:
    image: postgres:16
    environment:
      POSTGRES_PASSWORD: secret
    networks:
      - backend

  cache:
    image: redis:7
    networks:
      - backend

networks:
  frontend:
  backend:
```

```
┌── frontend ──────────────────┐
│                              │
│  proxy ◀─────────▶ api ─────┼──┐
│                              │  │
│  proxy NO ve db ni cache     │  │
│                              │  │
└──────────────────────────────┘  │
                                   │
┌── backend ──────────────────────┤
│                                 │
│  api ◀─────────▶ db            │
│    ◀─────────▶ cache            │
│                                 │
│  db y cache NO ven proxy       │
│                                 │
└─────────────────────────────────┘
```

```bash
docker compose up -d

# proxy puede hablar con api (misma red frontend)
docker compose exec proxy ping -c 1 api    # ✓

# proxy NO puede hablar con db (redes diferentes)
docker compose exec proxy ping -c 1 db     # ✗ bad address 'db'

# api puede hablar con db y cache (está en backend)
docker compose exec api ping -c 1 db       # ✓
docker compose exec api ping -c 1 cache    # ✓
```

### Comportamiento importante: `networks:` desactiva la red default

Cuando un servicio declara `networks:` explícitamente, **no se conecta a la red
default**. Si quieres que también esté en la default, hay que incluirla:

```yaml
services:
  web:
    networks:
      - frontend
      - default    # Hay que incluirla explícitamente

networks:
  frontend:
```

Si ningún servicio usa `networks:`, todos van a la red default y la sección
`networks:` top-level no es necesaria.

> **Nota**: Si al menos un servicio declara `networks:` pero otro no lo hace,
> el que no declara sigue en la red `default` (que Compose crea automáticamente).
> Solo los servicios que declaran `networks:` se excluyen de `default`.

### Servicios en múltiples redes

Un servicio puede estar conectado a varias redes simultáneamente. Esto es
esencial para servicios "puente" como un API que necesita acceder tanto al
proxy público como a la base de datos privada:

```yaml
services:
  api:
    networks:
      - frontend    # Accesible desde proxy
      - backend     # Accede a db y cache
```

Cada red asigna al contenedor una interfaz de red y una IP independientes.
El contenedor puede comunicarse con los servicios de cada red, pero los
servicios de una red no ven a los de la otra (a menos que también estén
conectados a ambas).

## Configuración de redes

```yaml
networks:
  frontend:
    driver: bridge           # Default: bridge
    driver_opts:
      com.docker.network.bridge.name: frontend_br

  backend:
    ipam:                    # Configurar subredes
      driver: default
      config:
        - subnet: 10.0.1.0/24
          gateway: 10.0.1.1

  monitoring:
    internal: true           # Sin acceso a internet
```

### Drivers de red disponibles

| Driver | Uso |
|---|---|
| `bridge` | Default. Servicios en el mismo host se comunican por DNS |
| `host` | Elimina aislamiento de red, usa la red del host directamente |
| `overlay` | Para Docker Swarm: conecta contenedores en múltiples hosts |
| `macvlan` | Asigna MAC address real a contenedores (visibles en la LAN) |
| `ipvlan` | Similar a macvlan pero compartiendo la MAC del host |
| `none` | Sin red (aislamiento total) |

En la práctica, para Compose standalone se usa casi exclusivamente `bridge`.
Los demás drivers son para casos especializados.

### `internal: true`

Una red interna no tiene gateway al exterior. Los contenedores en esta red
pueden comunicarse entre sí pero **no pueden acceder a internet**:

```yaml
networks:
  db_net:
    internal: true

services:
  db:
    image: postgres:16
    networks:
      - db_net
    # db no puede hacer DNS externo ni descargar paquetes
    # Solo se comunica con otros contenedores en db_net
```

Útil para bases de datos y servicios que no necesitan (ni deben tener)
conectividad externa.

### `network_mode` vs `networks`

`network_mode` y `networks` son **mutuamente excluyentes**. Si usas
`network_mode`, no puedes usar `networks:` en el mismo servicio:

```yaml
services:
  # Usa la red del host directamente
  monitoring:
    image: prometheus
    network_mode: host
    # No se puede usar 'ports:' ni 'networks:' con network_mode: host

  # Comparte el stack de red de otro servicio
  debug:
    image: nicolaka/netshoot
    network_mode: "service:api"
    # debug comparte IP, puertos e interfaces con api
    # Útil para debugging de red
```

`network_mode: "service:X"` hace que el contenedor comparta el namespace
de red con otro servicio. Ambos contenedores ven `localhost` como la misma
máquina — útil para herramientas de debugging que necesitan acceder a los
puertos locales de otro servicio.

## Redes externas

Para conectar servicios de Compose a redes creadas fuera del proyecto (por
otro Compose, manualmente, o por otro sistema):

```yaml
networks:
  shared:
    external: true        # No la crea, debe existir previamente
    name: infra_shared    # Nombre real de la red (si difiere de la clave)
```

```bash
# Crear la red externa primero
docker network create infra_shared

# Luego levantar el proyecto
docker compose up -d
```

Si la red no existe, `docker compose up` falla con error. `docker compose down`
**no elimina** redes externas.

### Comunicar dos proyectos Compose

```yaml
# proyecto_a/compose.yml
services:
  api:
    image: myapi:latest
    networks:
      - shared

networks:
  shared:
    name: shared_network    # Compose crea esta red con nombre fijo
```

```yaml
# proyecto_b/compose.yml
services:
  frontend:
    image: myweb:latest
    networks:
      - shared

networks:
  shared:
    external: true          # No la crea, espera que ya exista
    name: shared_network
```

```bash
# Levantar proyecto A primero (crea shared_network)
cd proyecto_a && docker compose up -d

# Levantar proyecto B (usa shared_network existente)
cd proyecto_b && docker compose up -d

# frontend puede resolver "api"
docker compose exec frontend ping -c 1 api   # ✓
```

**Orden de limpieza**: destruir primero el proyecto con `external: true` (B),
luego el que creó la red (A). Si destruyes A primero, la red se elimina y B
queda con una referencia rota.

## Aliases de red

Un servicio puede tener **nombres DNS adicionales** en una red específica:

```yaml
services:
  postgres-primary:
    image: postgres:16
    networks:
      backend:
        aliases:
          - db
          - database
          - primary-db

networks:
  backend:
```

```bash
# Todos estos nombres resuelven al mismo contenedor
docker compose exec api ping -c 1 postgres-primary  # ✓
docker compose exec api ping -c 1 db                 # ✓
docker compose exec api ping -c 1 database           # ✓
```

Los aliases son **específicos por red**. Un alias definido en `backend` no es
visible desde la red `frontend`. El nombre del servicio (`postgres-primary`)
se registra automáticamente como alias en todas las redes a las que pertenece.

## IPs estáticas

En casos especiales, se puede asignar una IP fija a un servicio:

```yaml
services:
  dns:
    image: coredns/coredns
    networks:
      infra:
        ipv4_address: 10.0.0.53

networks:
  infra:
    ipam:
      config:
        - subnet: 10.0.0.0/24
```

En general, **no uses IPs estáticas**. Usa nombres de servicio y DNS. Las IPs
estáticas son frágiles y difíciles de mantener. Solo se justifican para
servicios de infraestructura como DNS servers.

## Inspeccionar redes de Compose

```bash
# Ver qué redes creó el proyecto
docker network ls | grep <nombre_proyecto>

# Inspeccionar una red
docker network inspect proyecto_backend

# Ver contenedores en una red específica
docker network inspect proyecto_backend \
  --format '{{range .Containers}}{{.Name}}: {{.IPv4Address}}{{"\n"}}{{end}}'

# Ver las redes de un contenedor específico
docker inspect proyecto-api-1 \
  --format '{{range $net, $cfg := .NetworkSettings.Networks}}{{$net}}: {{$cfg.IPAddress}}{{"\n"}}{{end}}'
```

## Prefijo del proyecto en redes

Compose prefija las redes con el nombre del proyecto:

```yaml
# compose.yml en directorio "myapp"
networks:
  backend:
```

```bash
docker compose up -d
docker network ls | grep backend
# abc123  myapp_backend  bridge  local
```

Para usar un nombre fijo (sin prefijo):

```yaml
networks:
  backend:
    name: my_custom_backend_name
```

## DNS personalizado por servicio

Se pueden configurar servidores DNS específicos **por servicio** (no por red):

```yaml
services:
  web:
    image: nginx:latest
    dns:
      - 8.8.8.8
      - 8.8.4.4
    dns_search:
      - example.com
```

> **Importante**: `dns:` es una propiedad del **servicio**, no de la red.
> No existe una propiedad `dns:` dentro de la definición `networks:` top-level.

Los nombres de servicio de Compose se siguen resolviendo por el DNS embebido
(127.0.0.11). Los DNS custom se usan como upstream para resolver nombres
externos (dominios de internet).

---

## Archivos del lab

```
labs/
├── compose.default-net.yml  ← Tres servicios en la red default
├── compose.custom-nets.yml  ← Proxy, api, db en redes frontend/backend
├── compose.internal.yml     ← Servicio en red interna (sin internet)
├── compose.project-a.yml   ← Proyecto A: crea red compartida
└── compose.project-b.yml   ← Proyecto B: usa red externa
```

---

## Ejercicios

### Ejercicio 1 — Red default: DNS automático

Verifica que en la red default todos los servicios se resuelven por nombre.

```bash
cd labs/

docker compose -f compose.default-net.yml up -d
```

**Predicción**: ¿Cuántas redes creará Compose? ¿Cómo se llamará?

```bash
docker network ls | grep labs
```

<details><summary>Respuesta</summary>

Una sola red: `labs_default` (nombre del proyecto + `_default`).

</details>

```bash
# web puede resolver api y db
docker compose -f compose.default-net.yml exec web ping -c 1 api
docker compose -f compose.default-net.yml exec web ping -c 1 db

# El nombre de servicio y el nombre del contenedor funcionan
docker compose -f compose.default-net.yml exec api ping -c 1 web
docker compose -f compose.default-net.yml exec api ping -c 1 labs-web-1
```

**Predicción**: ¿Qué muestra `/etc/resolv.conf` dentro de `web`?

```bash
docker compose -f compose.default-net.yml exec web cat /etc/resolv.conf
```

<details><summary>Respuesta</summary>

`nameserver 127.0.0.11` — el DNS embebido de Docker que resuelve nombres
de servicio automáticamente.

</details>

```bash
docker compose -f compose.default-net.yml down
```

### Ejercicio 2 — Aislamiento con redes separadas

Demuestra que redes custom aíslan servicios entre sí.

```bash
docker compose -f compose.custom-nets.yml up -d
```

**Predicción**: `proxy` puede hacer ping a `api`, pero ¿puede hacer ping a `db`?

```bash
# proxy → api (misma red: frontend)
docker compose -f compose.custom-nets.yml exec proxy ping -c 1 api

# proxy → db (redes diferentes)
docker compose -f compose.custom-nets.yml exec proxy ping -c 1 -W 2 db 2>&1
```

<details><summary>Respuesta</summary>

`proxy` → `api`: funciona (ambos en `frontend`).
`proxy` → `db`: falla con `bad address 'db'` (el DNS de `frontend` no conoce
servicios que solo están en `backend`).

</details>

**Predicción**: ¿`api` puede hablar con `db`? ¿Y `db` con `proxy`?

```bash
# api es el puente entre ambas redes
docker compose -f compose.custom-nets.yml exec api ping -c 1 db
docker compose -f compose.custom-nets.yml exec api ping -c 1 proxy

# db → proxy
docker compose -f compose.custom-nets.yml exec db ping -c 1 -W 2 proxy 2>&1
```

<details><summary>Respuesta</summary>

`api` ve a ambos: está en `frontend` y `backend` (sirve como puente).
`db` → `proxy`: falla con `bad address 'proxy'`. El aislamiento funciona en
ambas direcciones.

</details>

```bash
# Verificar que no hay red default
docker network ls | grep labs
```

**Predicción**: ¿Existe `labs_default`?

<details><summary>Respuesta</summary>

No. Todos los servicios declararon `networks:` explícitamente, así que
Compose no creó la red default.

</details>

```bash
docker compose -f compose.custom-nets.yml down
```

### Ejercicio 3 — Red interna (sin internet)

Demuestra que `internal: true` bloquea el acceso a internet pero permite
la comunicación entre contenedores.

```bash
docker compose -f compose.internal.yml up -d
```

**Predicción**: ¿`isolated` puede hacer ping a `8.8.8.8`? ¿Y `connected`?

```bash
# isolated → internet
docker compose -f compose.internal.yml exec isolated ping -c 1 -W 3 8.8.8.8 2>&1

# connected → internet
docker compose -f compose.internal.yml exec connected ping -c 1 -W 3 8.8.8.8
```

<details><summary>Respuesta</summary>

`isolated`: 100% packet loss — la red `no_internet` no tiene gateway.
`connected`: funciona — también está en la red `default` que sí tiene gateway.

</details>

**Predicción**: ¿`isolated` puede hacer ping a `connected`?

```bash
docker compose -f compose.internal.yml exec isolated ping -c 1 connected
```

<details><summary>Respuesta</summary>

Sí. Ambos están en `no_internet`. La restricción `internal` solo bloquea
tráfico hacia fuera de Docker, no entre contenedores en la misma red.

</details>

```bash
docker compose -f compose.internal.yml down
```

### Ejercicio 4 — Comunicar dos proyectos Compose

Demuestra que dos proyectos independientes pueden comunicarse a través de
una red compartida con `external: true`.

```bash
# Levantar proyecto A (crea la red)
docker compose -f compose.project-a.yml -p project-a up -d

# Verificar la red
docker network ls | grep lab-shared
```

**Predicción**: Si levantas proyecto B **antes** de que exista la red, ¿qué pasa?

<details><summary>Respuesta</summary>

Compose falla con:
`network lab-shared-network declared as external, but could not be found`.
Las redes `external: true` deben existir previamente.

</details>

```bash
# Levantar proyecto B (usa la red existente)
docker compose -f compose.project-b.yml -p project-b up -d

# service_b puede resolver service_a (otro proyecto)
docker compose -f compose.project-b.yml -p project-b exec service_b ping -c 2 service_a
```

```bash
# Limpiar en orden: primero B (external), luego A (creador de la red)
docker compose -f compose.project-b.yml -p project-b down
docker compose -f compose.project-a.yml -p project-a down

# Verificar que la red se eliminó
docker network ls | grep lab-shared
```

### Ejercicio 5 — Network aliases

Crea un servicio con aliases DNS y verifica que todos resuelven.

```bash
mkdir -p /tmp/compose_alias && cd /tmp/compose_alias

cat > compose.yml << 'EOF'
services:
  api:
    image: alpine:latest
    command: sleep 3600
    networks:
      - backend

  primary-db:
    image: alpine:latest
    command: sleep 3600
    networks:
      backend:
        aliases:
          - db
          - database

networks:
  backend:
EOF

docker compose up -d
```

**Predicción**: ¿Cuántos nombres DNS resuelven al contenedor `primary-db`?

```bash
docker compose exec api ping -c 1 primary-db
docker compose exec api ping -c 1 db
docker compose exec api ping -c 1 database
```

<details><summary>Respuesta</summary>

Tres: el nombre del servicio (`primary-db`) se registra automáticamente,
más los dos aliases explícitos (`db`, `database`). Todos resuelven a la
misma IP.

</details>

```bash
docker compose down
cd / && rm -rf /tmp/compose_alias
```

### Ejercicio 6 — La red default desaparece

Demuestra que declarar `networks:` en un servicio lo excluye de la red default.

```bash
mkdir -p /tmp/compose_default_gone && cd /tmp/compose_default_gone

cat > compose.yml << 'EOF'
services:
  # Este servicio no declara networks → va a default
  monitor:
    image: alpine:latest
    command: sleep 3600

  # Este servicio declara networks → NO va a default
  api:
    image: alpine:latest
    command: sleep 3600
    networks:
      - backend

networks:
  backend:
EOF

docker compose up -d
```

**Predicción**: ¿`monitor` puede hacer ping a `api`? ¿Cuántas redes se crearon?

```bash
docker compose exec monitor ping -c 1 -W 2 api 2>&1

docker network ls | grep compose_default_gone
```

<details><summary>Respuesta</summary>

`monitor` **NO** puede resolver `api`. Se crearon dos redes:
`compose_default_gone_default` (para `monitor`) y
`compose_default_gone_backend` (para `api`). Cada uno está en una red
diferente, por lo que no se ven.

Para que `monitor` vea a `api`, habría que añadir `default` a la lista de
redes de `api`:

```yaml
  api:
    networks:
      - backend
      - default
```

</details>

```bash
docker compose down
cd / && rm -rf /tmp/compose_default_gone
```

### Ejercicio 7 — Inspeccionar redes y contenedores

Explora los comandos de inspección de redes.

```bash
cd labs/

docker compose -f compose.custom-nets.yml up -d
```

**Predicción**: ¿Cuántas IPs tiene el contenedor `api`?

```bash
# Ver qué redes existen
docker network ls | grep labs

# Ver contenedores en frontend
docker network inspect labs_frontend \
  --format '{{range .Containers}}{{.Name}}: {{.IPv4Address}}{{"\n"}}{{end}}'

# Ver contenedores en backend
docker network inspect labs_backend \
  --format '{{range .Containers}}{{.Name}}: {{.IPv4Address}}{{"\n"}}{{end}}'
```

<details><summary>Respuesta</summary>

`api` tiene **dos IPs**: una en `frontend` y otra en `backend`. Aparece
en la salida de ambas inspecciones de red.

</details>

```bash
# Ver todas las redes de un contenedor específico
docker inspect labs-api-1 \
  --format '{{range $net, $cfg := .NetworkSettings.Networks}}{{$net}}: {{$cfg.IPAddress}}{{"\n"}}{{end}}'
```

**Predicción**: ¿Cuántas líneas produce el comando anterior?

<details><summary>Respuesta</summary>

Dos líneas, una por cada red:
```
labs_frontend: 172.x.x.x
labs_backend: 172.y.y.y
```

</details>

```bash
docker compose -f compose.custom-nets.yml down
```

### Ejercicio 8 — DNS personalizado por servicio

Demuestra que `dns:` es una propiedad del **servicio**, no de la red.

```bash
mkdir -p /tmp/compose_dns && cd /tmp/compose_dns

cat > compose.yml << 'EOF'
services:
  custom-dns:
    image: alpine:latest
    command: sleep 3600
    dns:
      - 1.1.1.1
      - 8.8.8.8

  default-dns:
    image: alpine:latest
    command: sleep 3600
EOF

docker compose up -d
```

**Predicción**: ¿Ambos contenedores pueden resolver nombres de servicio de
Compose a pesar de tener diferente configuración DNS?

```bash
# Ambos pueden resolver al otro por nombre de servicio
docker compose exec custom-dns ping -c 1 default-dns
docker compose exec default-dns ping -c 1 custom-dns
```

<details><summary>Respuesta</summary>

Sí. El DNS embebido de Docker (127.0.0.11) maneja la resolución de nombres
de servicio independientemente de la configuración `dns:` del servicio.

La propiedad `dns:` solo afecta qué servidores se usan como upstream para
resolver **nombres externos** (dominios de internet). Los nombres de servicio
de Compose siempre se resuelven por el DNS embebido.

</details>

```bash
docker compose down
cd / && rm -rf /tmp/compose_dns
```

---

## Resumen visual

```
┌─────────────────────────────────────────────────────────────────┐
│                    REDES EN COMPOSE                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Sin networks: (default)                                        │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  web  api  db  cache  (todos en la misma red)           │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                 │
│  Con networks:                                                  │
│  ┌─────────────────┐     ┌─────────────────┐                   │
│  │    frontend      │     │    backend       │                   │
│  │  proxy ◀──▶ api  │     │  api ◀──▶ db    │                   │
│  └─────────────────┘     └─────────────────┘                   │
│                                                                 │
│  Reglas clave:                                                  │
│  - networks: explícito = no se conecta a default               │
│  - internal: true = sin gateway a internet                     │
│  - external: true = red debe existir previamente               │
│  - network_mode y networks son mutuamente excluyentes          │
│  - dns: es propiedad del servicio, NO de la red                │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## Checklist de redes

- [ ] Bases de datos solo en red `backend` (no en default ni frontend)
- [ ] Servicios puente (como API) conectados a ambas redes
- [ ] Servicios sin necesidad de internet con `internal: true`
- [ ] Bases de datos y caches sin `ports:` publicados
- [ ] Para comunicar proyectos: red con `name:` fijo + `external: true`
- [ ] Aliases para nombres DNS amigables de servicios
- [ ] Destruir proyectos con `external: true` **antes** que el creador de la red
