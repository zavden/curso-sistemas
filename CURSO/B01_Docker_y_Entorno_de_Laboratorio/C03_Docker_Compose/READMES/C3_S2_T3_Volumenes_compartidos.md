# T03 — Volúmenes compartidos

## Tipos de volúmenes en Compose

Compose soporta tres tipos de volúmenes:

| Tipo | Sintaxis | Persistencia | Uso típico |
|---|---|---|---|
| **Named volume** | `name:/path` | Sobrevive a `down` (no a `down -v`) | Datos de aplicaciones y BDs |
| **Bind mount** | `./host:/container` | Vive en el host filesystem | Desarrollo local |
| **tmpfs** | `tmpfs: [/path]` | Solo en memoria, se pierde al eliminar contenedor | Datos sensibles temporales |

## Named volumes en Compose

La sección `volumes:` a nivel raíz del Compose file declara named volumes. Estos
volúmenes persisten entre ciclos de `down` y `up` (sin `-v`), y pueden
compartirse entre servicios:

```yaml
services:
  web:
    image: nginx:latest
    volumes:
      - web-content:/usr/share/nginx/html

  editor:
    image: alpine:latest
    command: sleep 3600
    volumes:
      - web-content:/data

volumes:
  web-content:    # Declaración del named volume
```

```
┌────────────────────────────────────────────────┐
│                Named volume: web-content        │
│                                                │
│   web ──▶ /usr/share/nginx/html ──┐            │
│                                   ├── mismos   │
│   editor ──▶ /data ──────────────┘    archivos │
│                                                │
└────────────────────────────────────────────────┘
```

Ambos servicios leen y escriben en el mismo volumen. Lo que `editor` escribe
en `/data`, `web` lo ve en `/usr/share/nginx/html`.

## Bind mounts para desarrollo

Los bind mounts montan un directorio del **host** en el contenedor. Son el
mecanismo preferido para desarrollo, porque permiten editar código en tu editor
y que el contenedor vea los cambios inmediatamente:

```yaml
services:
  api:
    image: node:20
    command: npm run dev
    volumes:
      # Bind mount: código fuente del host
      - ./src:/app/src

      # Named volume: node_modules separados
      - node_modules:/app/node_modules

    ports:
      - "3000:3000"

volumes:
  node_modules:
```

```
Host                          Contenedor
┌────────────────────┐       ┌────────────────────┐
│ ./src/             │──────▶│ /app/src/           │  Bind mount
│   index.js         │       │   index.js          │  (sincronizado)
│   routes.js        │       │   routes.js         │
│                    │       │                     │
│                    │       │ /app/node_modules/  │  Named volume
│                    │       │   express/          │  (aislado)
│                    │       │   lodash/           │
└────────────────────┘       └────────────────────┘
```

### ¿Por qué separar `node_modules` en un named volume?

Si haces `- ./:/app` (montar todo el proyecto), `node_modules/` del host oculta
el del contenedor. Esto causa problemas si:
- No tienes `node_modules/` en el host (no ejecutaste `npm install` localmente)
- El host tiene un OS diferente (módulos nativos compilados para macOS no funcionan
  en el contenedor Linux)

El truco de montar `node_modules` como named volume crea un volumen dedicado que
el contenedor gestiona independientemente:

```yaml
volumes:
  - ./:/app                      # Todo el proyecto del host
  - node_modules:/app/node_modules  # Pero node_modules es un volumen separado
```

El named volume "gana" sobre el bind mount para esa ruta específica. El
contenedor instala sus propios `node_modules` dentro del volumen, sin afectar
al host.

## Paths relativos en Compose

Los bind mounts con paths relativos se resuelven **desde el directorio del
Compose file**, no desde donde ejecutas `docker compose`:

```
proyecto/
├── compose.yml
├── src/
│   └── app.py
└── config/
    └── nginx.conf
```

```yaml
# compose.yml
services:
  api:
    volumes:
      - ./src:/app/src              # → proyecto/src
      - ./config/nginx.conf:/etc/nginx/nginx.conf:ro  # → proyecto/config/nginx.conf
```

```bash
# Funciona desde cualquier directorio
cd /tmp
docker compose -f /home/user/proyecto/compose.yml up -d
# Los paths relativos se resuelven desde /home/user/proyecto/
```

## Volúmenes read-only

El sufijo `:ro` monta el volumen en modo solo lectura. El contenedor puede leer
pero no modificar los archivos:

```yaml
services:
  web:
    image: nginx:latest
    volumes:
      # Configuración: solo lectura
      - ./nginx.conf:/etc/nginx/nginx.conf:ro

      # Certificados: solo lectura
      - ./certs:/etc/nginx/certs:ro

      # Datos: lectura/escritura (default)
      - web-data:/var/www/html

volumes:
  web-data:
```

`:ro` es una buena práctica para archivos de configuración y secrets: previene
que una vulnerabilidad en el contenedor modifique archivos del host.

## tmpfs: volúmenes en memoria

`tmpfs` monta un filesystem en memoria RAM. Los datos se pierden al **eliminar**
el contenedor (no al detenerlo — `stop`/`start` preserva tmpfs porque el
contenedor sigue existiendo):

```yaml
services:
  app:
    image: alpine:latest
    tmpfs:
      - /run/secrets
      - /tmp
    command: sleep 3600
```

### tmpfs con opciones

La sintaxis `tmpfs:` a nivel de servicio solo acepta paths (strings). Para
configurar opciones como tamaño o permisos, se usa la sintaxis larga bajo
`volumes:`:

```yaml
services:
  app:
    image: alpine:latest
    volumes:
      - type: tmpfs
        target: /secrets
        tmpfs:
          size: 104857600    # 100 MB en bytes

      - type: tmpfs
        target: /tmp
        tmpfs:
          size: 52428800     # 50 MB en bytes
```

> **Nota**: `tmpfs:` a nivel de servicio NO acepta objetos con `target:` ni
> `size:`. Solo acepta strings como `- /run/secrets`. Para opciones avanzadas,
> usar la sintaxis larga con `type: tmpfs` bajo `volumes:`.

## Compartir volúmenes entre servicios

### Caso 1: Shared data entre web server y generador de contenido

```yaml
services:
  generator:
    image: alpine:latest
    volumes:
      - shared-html:/output
    command: >
      sh -c "while true; do
        echo '<h1>Generado: '$(date)'</h1>' > /output/index.html;
        sleep 5;
      done"

  web:
    image: nginx:latest
    volumes:
      - shared-html:/usr/share/nginx/html:ro
    ports:
      - "8080:80"
    depends_on:
      - generator

volumes:
  shared-html:
```

`generator` escribe en el volumen, `web` lo sirve en modo read-only.

### Caso 2: Logs compartidos

```yaml
services:
  app:
    image: myapp:latest
    volumes:
      - logs:/var/log/app

  log-viewer:
    image: alpine:latest
    volumes:
      - logs:/logs:ro
    command: tail -f /logs/app.log

volumes:
  logs:
```

### Caso 3: Base de datos con volumen exclusivo

```yaml
services:
  db:
    image: postgres:16
    volumes:
      - pgdata:/var/lib/postgresql/data
    environment:
      POSTGRES_PASSWORD: secret

volumes:
  pgdata:
```

Las bases de datos **no deben compartir su volumen de datos** con otros servicios.
El formato interno de los archivos de datos es específico del motor de base de
datos, y accesos concurrentes externos pueden corromper los datos.

## Opciones de driver para volúmenes

```yaml
volumes:
  # Volumen local (default)
  local-data:

  # Volumen con driver NFS
  nfs-data:
    driver: local
    driver_opts:
      type: nfs
      o: addr=192.168.1.100,rw
      device: ":/export/data"

  # Volumen con opciones específicas
  tmpfs-vol:
    driver: local
    driver_opts:
      type: tmpfs
      device: tmpfs
      o: size=100m
```

### Volúmenes externos

Los volúmenes `external: true` son creados fuera de Compose y no se eliminan
con `down -v`:

```yaml
volumes:
  important-data:
    external: true    # Compose no lo crea ni lo elimina
```

```bash
# Crear el volumen externamente
docker volume create important-data

# down -v no lo toca
docker compose down -v
docker volume ls | grep important-data
# important-data  ← sigue ahí
```

Para referenciar un volumen con nombre diferente al de la clave:

```yaml
volumes:
  db-data:
    external: true
    name: my_real_volume_name
```

## Sintaxis extendida de volúmenes

La sintaxis corta (`source:target:mode`) es suficiente en la mayoría de casos.
La sintaxis larga permite más opciones:

```yaml
services:
  app:
    volumes:
      # Named volume con opciones
      - type: volume
        source: app-data
        target: /data
        read_only: true
        volume:
          nocopy: true    # No copiar datos existentes del contenedor al volumen

      # Bind mount con opciones
      - type: bind
        source: ./config
        target: /app/config
        read_only: true
        bind:
          propagation: rprivate    # Default

      # tmpfs con opciones
      - type: tmpfs
        target: /secrets
        tmpfs:
          size: 1048576    # 1 MB en bytes
```

### Propagation de bind mounts

| Opción | Descripción |
|---|---|
| `rprivate` | Default. Cambios no se propagan, recursivo |
| `private` | Cambios no se propagan (no recursivo) |
| `shared` | Cambios del host se ven en container y viceversa |
| `rshared` | Shared + recursivo para sub-mounts |
| `slave` | Solo host → container (una dirección) |
| `rslave` | Slave recursivo |

En la práctica, `rprivate` (el default) es lo que se necesita en casi todos
los casos.

## `docker compose down -v` — Eliminar volúmenes

```bash
# down sin -v: contenedores y redes eliminados, volúmenes preservados
docker compose down
docker volume ls | grep pgdata
# proyecto_pgdata  ← sigue ahí

# down con -v: también elimina los named volumes del proyecto
docker compose down -v
docker volume ls | grep pgdata
# (vacío)
```

**Precaución**: `-v` elimina **todos los named volumes declarados en el Compose
file**. Si la base de datos tiene datos importantes, perderlos con `-v` es
irreversible (salvo backup).

### Volúmenes que sobreviven a `down -v`

Los volúmenes `external: true` **no se eliminan** con `down -v`:

```yaml
volumes:
  important-data:
    external: true    # Compose no lo crea ni lo elimina
```

## Volúmenes anónimos en Compose

Si usas un path sin nombre, Compose crea un volumen anónimo:

```yaml
services:
  app:
    volumes:
      - /app/data    # Volumen anónimo — sin nombre
```

Estos volúmenes son difíciles de rastrear y se acumulan. Siempre usa named
volumes.

## Inspeccionar volúmenes de Compose

```bash
# Listar volúmenes declarados en el Compose file
docker compose config --volumes
# pgdata
# web-content

# Ver detalles de un volumen (incluye Mountpoint en el host)
docker volume inspect proyecto_pgdata

# Ver uso de espacio de volúmenes
docker system df -v
```

---

## Archivos del lab

```
labs/
├── compose.named.yml    ← Servicio con named volume
├── compose.bind.yml     ← Servicio con bind mount (:ro)
├── compose.shared.yml   ← Writer + reader compartiendo volumen
├── compose.persist.yml  ← Servicio para demo de persistencia
└── html/
    └── index.html       ← Contenido HTML para bind mount
```

---

## Ejercicios

### Ejercicio 1 — Named volume: persistencia entre ciclos

Demuestra que un named volume preserva datos entre `down`/`up`.

```bash
cd labs/

docker compose -f compose.named.yml up -d
docker compose -f compose.named.yml logs app
```

**Predicción**: Si haces `down` (sin `-v`) y luego `up` de nuevo, ¿cuántas
líneas tendrá `/data/log.txt`?

```bash
docker compose -f compose.named.yml down
docker compose -f compose.named.yml up -d
docker compose -f compose.named.yml logs app
```

<details><summary>Respuesta</summary>

Dos líneas. El named volume preservó el `log.txt` de la primera ejecución.
`down` destruyó el contenedor pero no el volumen.

</details>

```bash
# Verificar que el volumen existe independientemente
docker volume ls | grep app-data
```

**Predicción**: ¿`down -v` eliminará el volumen?

```bash
docker compose -f compose.named.yml down -v
docker volume ls | grep app-data
```

<details><summary>Respuesta</summary>

Sí. `-v` elimina los named volumes del proyecto. El volumen ya no existe.

</details>

### Ejercicio 2 — Bind mount: cambios en vivo

Demuestra que un bind mount refleja cambios del host inmediatamente.

```bash
docker compose -f compose.bind.yml up -d

curl -s http://localhost:8080
```

**Predicción**: Si editas `html/index.html` en el host, ¿necesitas reiniciar
el contenedor para ver el cambio?

```bash
echo '<h1>Version 2 - edited on host</h1>' > html/index.html
curl -s http://localhost:8080
```

<details><summary>Respuesta</summary>

No. El bind mount vincula directamente el directorio del host con el del
contenedor. Los cambios se reflejan inmediatamente.

</details>

**Predicción**: El volumen está montado con `:ro`. ¿El contenedor puede
crear archivos en `/usr/share/nginx/html`?

```bash
docker compose -f compose.bind.yml exec web sh -c \
  'echo "test" > /usr/share/nginx/html/hack.html' 2>&1
```

<details><summary>Respuesta</summary>

No: `Read-only file system`. El sufijo `:ro` impide escritura desde el
contenedor, protegiendo los archivos del host.

</details>

```bash
# Restaurar y limpiar
echo '<h1>Version 1</h1>' > html/index.html
docker compose -f compose.bind.yml down
```

### Ejercicio 3 — Volumen compartido entre servicios

Demuestra que dos servicios pueden compartir un named volume.

```bash
docker compose -f compose.shared.yml up -d
sleep 3
```

**Predicción**: ¿Qué muestra `curl localhost:8080`? ¿Cambiará si esperas 10
segundos?

```bash
curl -s http://localhost:8080
sleep 10
curl -s http://localhost:8080
```

<details><summary>Respuesta</summary>

Muestra un HTML con timestamp. Tras 10 segundos, el timestamp es diferente.
`writer` actualiza `index.html` cada 5 segundos y `reader` (nginx) sirve
la versión más reciente del volumen compartido.

</details>

**Predicción**: ¿`reader` puede escribir en el volumen?

```bash
docker compose -f compose.shared.yml exec reader sh -c \
  'echo "test" > /usr/share/nginx/html/test.html' 2>&1
```

<details><summary>Respuesta</summary>

No: `Read-only file system`. `reader` monta el volumen con `:ro`.
Solo `writer` tiene permisos de escritura.

</details>

```bash
docker compose -f compose.shared.yml down -v
```

### Ejercicio 4 — Persistencia: `down` vs `down -v`

Demuestra el impacto de `-v` en los datos.

```bash
docker compose -f compose.persist.yml up -d
docker compose -f compose.persist.yml exec db sh -c '
    echo "important data" > /data/records.txt
    echo "config value" > /data/config.txt
    ls -la /data/
'
```

**Predicción**: Después de `down` (sin `-v`) y `up`, ¿existirán los archivos?

```bash
docker compose -f compose.persist.yml down
docker compose -f compose.persist.yml up -d
docker compose -f compose.persist.yml exec db cat /data/records.txt
```

<details><summary>Respuesta</summary>

Sí. `down` sin `-v` preserva los named volumes. Los datos sobreviven al
ciclo down/up.

</details>

**Predicción**: Después de `down -v` y `up`, ¿existirán?

```bash
docker compose -f compose.persist.yml down -v
docker compose -f compose.persist.yml up -d
docker compose -f compose.persist.yml exec db ls /data/
```

<details><summary>Respuesta</summary>

No. Directorio vacío. `down -v` eliminó el volumen y todos sus datos
permanentemente. No hay forma de recuperarlos (salvo backup previo).

</details>

```bash
docker compose -f compose.persist.yml down -v
```

### Ejercicio 5 — tmpfs: datos en memoria

Demuestra que tmpfs almacena datos en memoria y cuándo se pierden.

```bash
mkdir -p /tmp/compose_tmpfs && cd /tmp/compose_tmpfs

cat > compose.yml << 'EOF'
services:
  app:
    image: alpine:latest
    tmpfs:
      - /secrets
    command: >
      sh -c '
        echo "password123" > /secrets/db_pass &&
        echo "Secret almacenado" &&
        sleep 3600
      '
EOF

docker compose up -d
docker compose logs app
```

**Predicción**: Si haces `stop` y luego `start` (sin eliminar el contenedor),
¿sobreviven los datos en `/secrets`?

```bash
docker compose exec app cat /secrets/db_pass

docker compose stop
docker compose start
sleep 1
docker compose exec app cat /secrets/db_pass
```

<details><summary>Respuesta</summary>

Sí, los datos **sobreviven** a `stop`/`start`. El tmpfs se pierde solo
al **eliminar** el contenedor, no al detenerlo. `stop`/`start` mantiene el
contenedor existente (lo detiene y reanuda), así que el tmpfs sigue montado
con sus datos.

</details>

**Predicción**: ¿Y después de `down`/`up` (que elimina y recrea el contenedor)?

```bash
docker compose down
docker compose up -d
sleep 1
docker compose exec app cat /secrets/db_pass
```

<details><summary>Respuesta</summary>

El archivo existe de nuevo, pero solo porque el `command` lo crea al arrancar.
Si el comando no lo creara, el directorio `/secrets` estaría vacío. `down`
eliminó el contenedor (y su tmpfs). `up` creó uno nuevo con tmpfs vacío.

</details>

```bash
docker compose down
cd / && rm -rf /tmp/compose_tmpfs
```

### Ejercicio 6 — Volumen externo (sobrevive a `down -v`)

Demuestra que `external: true` protege un volumen de `down -v`.

```bash
mkdir -p /tmp/compose_external && cd /tmp/compose_external

# Crear volumen externamente
docker volume create lab_external_data

cat > compose.yml << 'EOF'
services:
  app:
    image: alpine:latest
    volumes:
      - external_data:/data
    command: sh -c 'echo "protected data" > /data/file.txt && sleep 3600'

volumes:
  external_data:
    external: true
    name: lab_external_data
EOF

docker compose up -d
sleep 1
docker compose exec app cat /data/file.txt
```

**Predicción**: `down -v` elimina named volumes del proyecto. ¿Eliminará
también `lab_external_data`?

```bash
docker compose down -v
docker volume ls | grep lab_external
```

<details><summary>Respuesta</summary>

No. El volumen `lab_external_data` sigue existiendo. `external: true` indica
a Compose que no gestione el ciclo de vida de este volumen: ni lo crea ni
lo elimina. `down -v` solo elimina volúmenes que Compose creó.

</details>

```bash
# Limpiar manualmente
docker volume rm lab_external_data
cd / && rm -rf /tmp/compose_external
```

### Ejercicio 7 — Named volume oculta bind mount

Demuestra que un named volume en una subruta "gana" sobre un bind mount
en la ruta padre.

```bash
mkdir -p /tmp/compose_override/src && cd /tmp/compose_override

echo 'console.log("from host")' > src/index.js

cat > compose.yml << 'EOF'
services:
  app:
    image: node:20-alpine
    working_dir: /app
    command: sh -c 'cat /app/src/index.js && echo "---" && ls /app/node_modules/ 2>&1'
    volumes:
      - ./src:/app/src
      - node_modules:/app/node_modules

volumes:
  node_modules:
EOF

docker compose up
```

**Predicción**: `/app/src/index.js` viene del host (bind mount). ¿Y
`/app/node_modules/`? ¿Está vacío o tiene contenido?

<details><summary>Respuesta</summary>

`/app/src/index.js` muestra `console.log("from host")` — viene del bind mount.
`/app/node_modules/` está vacío (o muestra error) porque el named volume fue
recién creado y nadie ha ejecutado `npm install` dentro de él.

El punto clave: si hicieras `- ./:/app` sin el volumen de `node_modules`, y
no tuvieras `node_modules/` en el host, el bind mount ocultaría cualquier
`node_modules` que el Dockerfile hubiera instalado. El named volume separado
evita este problema: el contenedor puede instalar sus dependencias en el
volumen sin que el bind mount interfiera.

</details>

```bash
docker compose down -v
cd / && rm -rf /tmp/compose_override
```

### Ejercicio 8 — Inspeccionar volúmenes

Explora los comandos de inspección de volúmenes.

```bash
cd labs/

docker compose -f compose.shared.yml up -d
```

**Predicción**: ¿Qué comando lista los volúmenes declarados en el Compose
file sin levantar nada?

```bash
# Listar volúmenes del Compose file
docker compose -f compose.shared.yml config --volumes
```

<details><summary>Respuesta</summary>

`docker compose config --volumes` muestra los nombres lógicos de los volúmenes
declarados en el Compose file (en este caso, `shared`).

</details>

```bash
# Ver detalles del volumen (incluye Mountpoint en el host)
docker volume inspect labs_shared
```

**Predicción**: ¿Dónde almacena Docker los datos del volumen en el host?

<details><summary>Respuesta</summary>

En el `Mountpoint` que muestra el inspect, normalmente algo como:
`/var/lib/docker/volumes/labs_shared/_data`

Este es el directorio real del host donde Docker almacena los datos del
named volume. Es gestionado por Docker — no se debe modificar directamente.

</details>

```bash
# Ver espacio usado por volúmenes
docker system df -v 2>/dev/null | head -5

docker compose -f compose.shared.yml down -v
```

---

## Resumen visual

```
┌─────────────────────────────────────────────────────────────────┐
│                    TIPOS DE VOLÚMENES                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Named Volume        Bind Mount           tmpfs                 │
│  ┌──────────┐       ┌──────────┐       ┌──────────┐            │
│  │ Docker   │       │ Host     │       │ Memoria  │            │
│  │ managed  │       │ filesystem│       │  RAM     │            │
│  └────┬─────┘       └────┬─────┘       └────┬─────┘            │
│       │                   │                   │                  │
│       ▼                   ▼                   ▼                  │
│  Persiste entre      Directorio          Se pierde al           │
│  down/up             del host            eliminar contenedor    │
│  (no down -v)        (sincronizado)      (no al detenerlo)     │
│                                                                 │
│  Reglas clave:                                                  │
│  - down = preserva volúmenes, down -v = los elimina            │
│  - external: true = sobrevive a down -v                        │
│  - :ro = solo lectura desde el contenedor                      │
│  - Named volume en subruta gana sobre bind mount padre         │
│  - tmpfs: a nivel de servicio solo acepta strings (paths)      │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## Checklist de volúmenes

- [ ] Bases de datos usan named volumes dedicados (no compartidos)
- [ ] Datos sensibles temporales usan tmpfs
- [ ] Bind mounts para código en desarrollo local
- [ ] Configuración y secrets montados con `:ro`
- [ ] `node_modules` aislado como named volume en proyectos Node
- [ ] Volúmenes críticos con `external: true` para proteger contra `down -v`
- [ ] Nunca usar volúmenes anónimos — siempre named volumes
