# T03 — Volúmenes compartidos

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

```bash
# Crear el volumen externamente
docker volume create important-data

# down -v no lo toca
docker compose down -v
docker volume ls | grep important-data
# important-data  ← sigue ahí
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
# Listar volúmenes del proyecto
docker compose config --volumes
# pgdata
# web-content

# Ver detalles de un volumen
docker volume inspect proyecto_pgdata

# Ver qué volúmenes tiene montados un servicio
docker compose ps --format "{{.Name}}: {{.Mounts}}"
```

---

## Ejercicios

### Ejercicio 1 — Volumen compartido entre servicios

```bash
mkdir -p /tmp/compose_shared && cd /tmp/compose_shared

cat > compose.yml << 'EOF'
services:
  writer:
    image: alpine:latest
    volumes:
      - shared:/data
    command: sh -c 'echo "Escrito por writer a las $$(date)" > /data/msg.txt && sleep 3600'

  reader:
    image: alpine:latest
    volumes:
      - shared:/data:ro
    command: sleep 3600
    depends_on:
      - writer

volumes:
  shared:
EOF

docker compose up -d
sleep 2

# El reader puede leer lo que escribió el writer
docker compose exec reader cat /data/msg.txt

# El reader NO puede escribir (read-only)
docker compose exec reader sh -c 'echo "test" > /data/test.txt' 2>&1 || echo "Read-only ✓"

docker compose down -v
cd / && rm -rf /tmp/compose_shared
```

### Ejercicio 2 — Bind mount para desarrollo

```bash
mkdir -p /tmp/compose_dev/src && cd /tmp/compose_dev

echo '<h1>Version 1</h1>' > src/index.html

cat > compose.yml << 'EOF'
services:
  web:
    image: nginx:latest
    volumes:
      - ./src:/usr/share/nginx/html:ro
    ports:
      - "8080:80"
EOF

docker compose up -d

# Verificar
curl -s http://localhost:8080
# <h1>Version 1</h1>

# Editar en el host
echo '<h1>Version 2 - editado en el host</h1>' > src/index.html

# El cambio se refleja inmediatamente
curl -s http://localhost:8080
# <h1>Version 2 - editado en el host</h1>

docker compose down
cd / && rm -rf /tmp/compose_dev
```

### Ejercicio 3 — Persistencia: `down` vs `down -v`

```bash
mkdir -p /tmp/compose_persist && cd /tmp/compose_persist

cat > compose.yml << 'EOF'
services:
  db:
    image: postgres:16-alpine
    environment:
      POSTGRES_PASSWORD: secret
    volumes:
      - pgdata:/var/lib/postgresql/data

volumes:
  pgdata:
EOF

# Crear y escribir datos
docker compose up -d
sleep 5
docker compose exec db psql -U postgres -c "CREATE TABLE test(id int); INSERT INTO test VALUES(42);"

# down sin -v: volúmenes preservados
docker compose down
docker volume ls | grep pgdata   # Sigue ahí

# Levantar de nuevo: datos intactos
docker compose up -d
sleep 3
docker compose exec db psql -U postgres -c "SELECT * FROM test;"
# id
# 42  ← datos preservados

# down con -v: volúmenes eliminados
docker compose down -v

# Levantar de nuevo: datos perdidos
docker compose up -d
sleep 5
docker compose exec db psql -U postgres -c "SELECT * FROM test;" 2>&1 || echo "Tabla no existe — datos perdidos"

docker compose down -v
cd / && rm -rf /tmp/compose_persist
```
