# Lab — Redes multi-servicio

## Objetivo

Laboratorio práctico para entender el networking de Docker Compose: red
default con DNS automático por nombre de servicio, redes custom para
aislar servicios, redes internas sin acceso a internet, y redes externas
para comunicar dos proyectos Compose independientes.

## Prerequisitos

- Docker y Docker Compose v2 instalados (`docker compose version`)
- Imagen base descargada: `docker pull alpine:latest`
- Estar en el directorio `labs/` de este tópico

## Archivos del laboratorio

| Archivo | Propósito |
|---|---|
| `compose.default-net.yml` | Tres servicios en la red default |
| `compose.custom-nets.yml` | Proxy, api, db en redes frontend/backend |
| `compose.internal.yml` | Servicio en red interna (sin internet) |
| `compose.project-a.yml` | Proyecto A: crea la red compartida |
| `compose.project-b.yml` | Proyecto B: usa la red como externa |

---

## Parte 1 — Red default

### Objetivo

Demostrar que Compose crea automáticamente una red bridge para el
proyecto y que todos los servicios pueden comunicarse entre sí usando
el nombre del servicio como hostname DNS.

### Paso 1.1: Examinar el archivo

```bash
cat compose.default-net.yml
```

Tres servicios (`web`, `api`, `db`) sin configuración de redes. Todos
irán a la red default automática.

### Paso 1.2: Levantar

```bash
docker compose -f compose.default-net.yml up -d
```

Observar en la salida que Compose creó una red `labs_default`.

### Paso 1.3: Verificar la red

```bash
docker network ls | grep labs
```

Salida esperada:

```
<id>  labs_default  bridge  local
```

Compose creó `labs_default` (nombre del proyecto + `_default`).

### Paso 1.4: Probar DNS entre servicios

```bash
docker compose -f compose.default-net.yml exec web ping -c 2 api
```

Salida esperada:

```
PING api (172.x.x.x): 56 data bytes
64 bytes from 172.x.x.x: seq=0 ttl=64 time=0.xxx ms
```

`web` puede resolver `api` por su nombre de servicio. Compose configura
DNS automáticamente dentro de la red.

```bash
docker compose -f compose.default-net.yml exec web ping -c 2 db
```

```bash
docker compose -f compose.default-net.yml exec db ping -c 2 web
```

Todos los servicios se ven entre sí. Cualquiera puede hablar con
cualquiera en la red default.

### Paso 1.5: El nombre DNS es el nombre del servicio

```bash
docker compose -f compose.default-net.yml exec api ping -c 1 web
docker compose -f compose.default-net.yml exec api ping -c 1 labs-web-1
```

Ambos funcionan: el nombre de servicio (`web`) y el nombre del
contenedor (`labs-web-1`). El nombre de servicio es el canónico.

### Paso 1.6: Limpiar

```bash
docker compose -f compose.default-net.yml down
```

---

## Parte 2 — Redes custom para aislamiento

### Objetivo

Demostrar que definiendo redes separadas (`frontend`, `backend`), se
puede controlar qué servicios se ven entre sí: el proxy no debería
poder hablar directamente con la base de datos.

### Paso 2.1: Examinar el archivo

```bash
cat compose.custom-nets.yml
```

Tres servicios con redes específicas:
- `proxy`: solo en `frontend`
- `api`: en `frontend` **y** `backend` (puente entre ambas)
- `db`: solo en `backend`

Antes de levantar, responde mentalmente:

- ¿`proxy` podrá hacer ping a `api`?
- ¿`proxy` podrá hacer ping a `db`?
- ¿`api` podrá hacer ping a `db`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.2: Levantar

```bash
docker compose -f compose.custom-nets.yml up -d
```

Observar que Compose creó dos redes: `labs_frontend` y `labs_backend`.

### Paso 2.3: Probar conectividad — proxy

```bash
docker compose -f compose.custom-nets.yml exec proxy ping -c 1 api
```

Funciona: ambos están en `frontend`.

```bash
docker compose -f compose.custom-nets.yml exec proxy ping -c 1 -W 2 db 2>&1
```

Salida esperada:

```
ping: bad address 'db'
```

`proxy` **no puede resolver** `db` porque están en redes diferentes.
El DNS de `frontend` no conoce a `db` (que solo está en `backend`).

### Paso 2.4: Probar conectividad — api (puente)

```bash
docker compose -f compose.custom-nets.yml exec api ping -c 1 proxy
```

```bash
docker compose -f compose.custom-nets.yml exec api ping -c 1 db
```

`api` ve a ambos porque está en las dos redes (`frontend` y `backend`).
Es el puente entre el proxy público y la base de datos privada.

### Paso 2.5: Probar conectividad — db

```bash
docker compose -f compose.custom-nets.yml exec db ping -c 1 api
```

Funciona: ambos están en `backend`.

```bash
docker compose -f compose.custom-nets.yml exec db ping -c 1 -W 2 proxy 2>&1
```

Salida esperada:

```
ping: bad address 'proxy'
```

`db` no ve a `proxy`. El aislamiento funciona en ambas direcciones.

### Paso 2.6: Verificar redes

```bash
docker network ls | grep labs
```

Salida esperada:

```
<id>  labs_backend   bridge  local
<id>  labs_frontend  bridge  local
```

No hay red `default` porque todos los servicios declararon `networks:`
explícitamente.

### Paso 2.7: Limpiar

```bash
docker compose -f compose.custom-nets.yml down
```

---

## Parte 3 — Red interna (sin internet)

### Objetivo

Demostrar que una red con `internal: true` no tiene acceso a internet.
Los contenedores se ven entre sí pero no pueden alcanzar hosts externos.

### Paso 3.1: Examinar el archivo

```bash
cat compose.internal.yml
```

Dos servicios:
- `isolated`: solo en `no_internet` (red interna)
- `connected`: en `no_internet` y en `default`

### Paso 3.2: Levantar

```bash
docker compose -f compose.internal.yml up -d
```

### Paso 3.3: Probar internet desde isolated

```bash
docker compose -f compose.internal.yml exec isolated ping -c 1 -W 3 8.8.8.8 2>&1
```

Salida esperada:

```
PING 8.8.8.8 (8.8.8.8): 56 data bytes
--- 8.8.8.8 ping statistics ---
1 packets transmitted, 0 packets received, 100% packet loss
```

`isolated` no tiene acceso a internet. La red `internal: true` no
tiene gateway al exterior.

### Paso 3.4: Probar internet desde connected

```bash
docker compose -f compose.internal.yml exec connected ping -c 1 -W 3 8.8.8.8
```

`connected` **sí** tiene acceso a internet porque también está en la
red `default` (que sí tiene gateway).

### Paso 3.5: Ambos se ven entre sí

```bash
docker compose -f compose.internal.yml exec isolated ping -c 1 connected
```

```bash
docker compose -f compose.internal.yml exec connected ping -c 1 isolated
```

Ambos pueden comunicarse dentro de la red `no_internet`. La restricción
`internal` solo bloquea el tráfico hacia internet, no entre contenedores.

### Paso 3.6: Limpiar

```bash
docker compose -f compose.internal.yml down
```

---

## Parte 4 — Red externa (dos proyectos)

### Objetivo

Demostrar cómo dos proyectos Compose independientes pueden comunicarse
a través de una red compartida usando `external: true`.

### Paso 4.1: Examinar ambos archivos

```bash
cat compose.project-a.yml
```

Proyecto A define la red `shared` con nombre fijo `lab-shared-network`.
Al hacer `up`, Compose **creará** esta red.

```bash
cat compose.project-b.yml
```

Proyecto B referencia la misma red con `external: true`. Compose **no
la crea** — espera que ya exista.

### Paso 4.2: Levantar proyecto A

```bash
docker compose -f compose.project-a.yml -p project-a up -d
```

Usamos `-p project-a` para dar un nombre de proyecto diferente.

```bash
docker network ls | grep lab-shared
```

Salida esperada:

```
<id>  lab-shared-network  bridge  local
```

Proyecto A creó la red `lab-shared-network`.

### Paso 4.3: Levantar proyecto B

```bash
docker compose -f compose.project-b.yml -p project-b up -d
```

Proyecto B usa la red existente. Si la red no existiera, Compose
mostraría un error.

### Paso 4.4: Verificar comunicación entre proyectos

```bash
docker compose -f compose.project-b.yml -p project-b exec service_b ping -c 2 service_a
```

Salida esperada:

```
PING service_a (172.x.x.x): 56 data bytes
64 bytes from 172.x.x.x: seq=0 ttl=64 time=0.xxx ms
```

`service_b` (proyecto B) puede resolver y comunicarse con `service_a`
(proyecto A) a través de la red compartida.

### Paso 4.5: Cada proyecto se gestiona independientemente

```bash
docker compose -f compose.project-b.yml -p project-b ps
docker compose -f compose.project-a.yml -p project-a ps
```

Cada proyecto tiene su propia vista de servicios, pero comparten la
red.

### Paso 4.6: Limpiar

```bash
docker compose -f compose.project-b.yml -p project-b down
docker compose -f compose.project-a.yml -p project-a down
```

El orden importa: destruir primero el proyecto con `external: true`
(B), luego el que creó la red (A). Si destruyes A primero, la red se
elimina y B queda con una referencia rota.

```bash
docker network ls | grep lab-shared
```

La red ya no existe.

---

## Limpieza final

```bash
# Eliminar recursos del lab (si alguno quedó)
docker compose -f compose.default-net.yml down 2>/dev/null
docker compose -f compose.custom-nets.yml down 2>/dev/null
docker compose -f compose.internal.yml down 2>/dev/null
docker compose -f compose.project-b.yml -p project-b down 2>/dev/null
docker compose -f compose.project-a.yml -p project-a down 2>/dev/null
docker network rm lab-shared-network 2>/dev/null
```

---

## Conceptos reforzados

1. Compose crea automáticamente una red **bridge default** para el
   proyecto. Todos los servicios se resuelven por DNS usando el
   **nombre del servicio** como hostname.

2. Definiendo redes custom (`frontend`, `backend`) se aíslan servicios.
   Un servicio que solo está en `frontend` **no puede resolver** un
   servicio que solo está en `backend`. Un servicio puente debe estar
   en ambas redes.

3. `internal: true` crea una red **sin acceso a internet**. Los
   contenedores se ven entre sí pero no pueden alcanzar hosts externos.
   Ideal para bases de datos y servicios que no necesitan conectividad
   exterior.

4. `external: true` permite usar una red **creada fuera del proyecto**.
   Esto habilita la comunicación entre dos proyectos Compose
   independientes a través de una red compartida.

5. Cuando un servicio declara `networks:` explícitamente, **no se
   conecta a la red default** automáticamente. Hay que incluir `default`
   en la lista si se necesita.
