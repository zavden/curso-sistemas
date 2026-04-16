# yq para YAML

## 1. Introducción — por qué yq

YAML es el formato dominante en configuración de infraestructura: Docker Compose,
Kubernetes, Ansible, GitHub Actions, CI/CD pipelines. Editarlo con `sed`/`awk` es
peligroso porque YAML depende de **indentación** y tiene tipos implícitos. `yq` es
un procesador de YAML en línea de comandos que entiende la estructura del documento.

```
                   PIPELINE  yq
    ┌──────────┐              ┌──────────┐
    │  YAML    │──  expr.  ──▶│  YAML    │
    │  entrada │              │  salida  │
    └──────────┘              └──────────┘
         │                         │
    stdin / archivo          stdout / archivo
```

### Versiones de yq — IMPORTANTE

Existen **dos herramientas** llamadas `yq`. Son proyectos completamente distintos:

| Proyecto | Autor | Lenguaje | Sintaxis | URL |
|----------|-------|----------|----------|-----|
| `yq` (Mike Farah) | mikefarah | Go | propia (similar a jq) | github.com/mikefarah/yq |
| `yq` (Andrey Kislyuk) | kislyuk | Python | **idéntica a jq** | github.com/kislyuk/yq |

```bash
# Verificar cuál tienes
yq --version
# mikefarah/yq versión v4.x  → Go (este README)
# yq 3.x (basado en jq)       → Python (wrapper de jq)
```

**Este tópico usa la versión Go (mikefarah/yq v4)**, que es la más popular y la que
se instala por defecto en la mayoría de distribuciones y CI/CD. Si ya sabes `jq`
(T03), la versión Python es trivial — es literalmente `jq` con un wrapper YAML.

```bash
# Instalación
# Debian/Ubuntu (snap)
sudo snap install yq

# Debian/Ubuntu (binario directo — recomendado)
sudo wget https://github.com/mikefarah/yq/releases/latest/download/yq_linux_amd64 \
  -O /usr/local/bin/yq && sudo chmod +x /usr/local/bin/yq

# Fedora/RHEL
sudo dnf install yq

# macOS
brew install yq

# Con Go
go install github.com/mikefarah/yq/v4@latest

# Verificar
yq --version   # yq (https://github.com/mikefarah/yq/) version v4.44.x
```

---

## 2. YAML — repaso express

Antes de usar `yq`, conviene tener claro el modelo de datos de YAML:

```yaml
# Escalares
name: Alice                  # string (sin comillas)
age: 30                      # integer
salary: 75000.50             # float
active: true                 # boolean
nothing: null                # null
quoted: "yes"                # string (forzado con comillas — "yes" sin comillas sería boolean)

# Secuencias (arrays)
fruits:
  - apple
  - banana
  - cherry

# Mappings (objetos)
address:
  street: "123 Main St"
  city: Springfield
  zip: "62701"               # comillas para preservar como string

# Anidado
servers:
  - hostname: web01
    ip: 10.0.1.1
    roles:
      - frontend
      - ssl
  - hostname: db01
    ip: 10.0.2.1
    roles:
      - database
```

### Gotchas de YAML que yq resuelve

| Valor | YAML lo interpreta como | Quieres string |
|-------|------------------------|----------------|
| `yes`, `no` | boolean | `"yes"`, `"no"` |
| `on`, `off` | boolean | `"on"`, `"off"` |
| `3.10` | float (3.1) | `"3.10"` |
| `08` | octal inválido → string o error | `"08"` |
| `1e3` | float (1000.0) | `"1e3"` |
| `null`, `~` | null | `"null"`, `"~"` |

`yq` maneja estos tipos correctamente al leer y escribir, algo que `sed` no puede hacer.

---

## 3. Sintaxis fundamental

### 3.1 Leer y pretty-print

```bash
# Pretty-print (identidad)
yq '.' config.yaml

# Desde stdin
cat config.yaml | yq '.'

# Evaluar expresión sin archivo (-n = null input)
yq -n '.name = "Alice"'
# name: Alice
```

### 3.2 Acceso a campos

```bash
cat <<'EOF' > server.yaml
server:
  hostname: web01
  ip: 10.0.1.1
  port: 8080
  tags:
    - production
    - frontend
EOF

# Acceder a campo
yq '.server.hostname' server.yaml
# web01

# Campo anidado
yq '.server.port' server.yaml
# 8080

# Acceder a array
yq '.server.tags[0]' server.yaml
# production

yq '.server.tags[-1]' server.yaml
# frontend

# Longitud de array
yq '.server.tags | length' server.yaml
# 2
```

### 3.3 Output formats

```bash
# YAML (default)
yq '.' config.yaml

# JSON output (-o=json)
yq -o=json '.' config.yaml

# JSON compacto
yq -o=json -I=0 '.' config.yaml

# XML output
yq -o=xml '.' config.yaml

# TOML output
yq -o=toml '.' config.yaml

# Props (Java properties)
yq -o=props '.' config.yaml
# server.hostname = web01
# server.ip = 10.0.1.1
# server.port = 8080
```

**Convertir entre formatos** es una de las capacidades más útiles de `yq`:

```bash
# JSON → YAML
yq -P '.' config.json           # -P = pretty print YAML

# YAML → JSON
yq -o=json '.' config.yaml

# YAML → JSON compacto (para usar con jq)
yq -o=json '.' config.yaml | jq '.server.hostname'
```

```
              yq como conversor universal
    ┌──────┐       ┌─────┐       ┌──────┐
    │ YAML │──────▶│     │──────▶│ YAML │
    │ JSON │──────▶│ yq  │──────▶│ JSON │
    │ XML  │──────▶│     │──────▶│ XML  │
    │ TOML │──────▶│     │──────▶│ Props│
    └──────┘       └─────┘       └──────┘
```

---

## 4. Operadores de lectura

### 4.1 Seleccionar y filtrar

```bash
data='
users:
  - name: Alice
    age: 30
    role: admin
  - name: Bob
    age: 25
    role: user
  - name: Carol
    age: 35
    role: admin
'

# Iterar sobre array
echo "$data" | yq '.users[]'

# Seleccionar por condición
echo "$data" | yq '.users[] | select(.role == "admin")'
# name: Alice
# age: 30
# role: admin
# ---
# name: Carol
# age: 35
# role: admin

# Seleccionar con múltiples condiciones
echo "$data" | yq '.users[] | select(.role == "admin" and .age > 32)'
# name: Carol
# age: 35
# role: admin

# Extraer un campo de cada elemento
echo "$data" | yq '.users[].name'
# Alice
# Bob
# Carol

# Contar elementos
echo "$data" | yq '.users | length'
# 3
```

### 4.2 Navegación con `keys` y `has`

```bash
config='
database:
  host: localhost
  port: 5432
  name: myapp
cache:
  host: localhost
  port: 6379
'

# Keys del nivel raíz
echo "$config" | yq 'keys'
# - database
# - cache

# ¿Tiene un campo?
echo "$config" | yq 'has("database")'
# true

echo "$config" | yq 'has("logging")'
# false

# Tipo de un nodo
echo "$config" | yq '.database | type'
# !!map

echo "$config" | yq '.database.port | type'
# !!int
```

### 4.3 Operador `//` — valor por defecto

```bash
echo "$config" | yq '.database.password // "not_set"'
# not_set

echo "$config" | yq '.database.host // "default_host"'
# localhost
```

---

## 5. Operadores de escritura

### 5.1 Asignar valores

```bash
# Asignar campo (= asigna, |= update in-place)
yq '.server.port = 9090' server.yaml
# server:
#   hostname: web01
#   ip: 10.0.1.1
#   port: 9090       ← cambiado
#   tags:
#     - production
#     - frontend

# Crear campo nuevo
yq '.server.protocol = "https"' server.yaml

# Asignar con tipo explícito
yq '.server.debug = true' server.yaml          # boolean
yq '.server.workers = 4' server.yaml            # integer
yq '.server.label = "8080"' server.yaml         # string (comillas = string)

# Asignar null
yq '.server.deprecated = null' server.yaml
```

### 5.2 Update operator `|=`

```bash
# |= aplica una transformación al valor existente (no reemplaza)

# Incrementar valor
yq '.server.port |= . + 1' server.yaml
# port: 8081

# Transformar string
yq '.server.hostname |= "prefix-" + .' server.yaml
# hostname: prefix-web01

# Actualizar cada elemento de un array
echo "$data" | yq '.users[].age |= . + 1'
# Cada edad se incrementa en 1
```

### 5.3 Añadir y eliminar

```bash
# Añadir elemento a array (+= con array)
yq '.server.tags += ["monitoring"]' server.yaml
# tags:
#   - production
#   - frontend
#   - monitoring

# Añadir múltiples elementos
yq '.server.tags += ["logging", "metrics"]' server.yaml

# Eliminar campo (del)
yq 'del(.server.tags)' server.yaml

# Eliminar elemento de array por índice
yq 'del(.server.tags[1])' server.yaml

# Eliminar elemento de array por condición
yq 'del(.users[] | select(.role == "user"))' <<< "$data"
```

### 5.4 Edición in-place: -i

A diferencia de `jq`, `yq` **sí tiene `-i`** para edición in-place:

```bash
# Editar in-place (modifica el archivo directamente)
yq -i '.server.port = 9090' server.yaml

# Equivalente a:
#   tmp=$(mktemp)
#   yq '.server.port = 9090' server.yaml > "$tmp" && mv "$tmp" server.yaml

# Editar in-place con backup (no nativo — hacerlo manual)
cp server.yaml server.yaml.bak
yq -i '.server.port = 9090' server.yaml
```

**Diferencia clave con jq**: `jq` no tiene `-i` — necesitas `sponge` o archivo
temporal. `yq -i` funciona directamente y preserva comentarios.

---

## 6. Comentarios y estilo

Una de las ventajas más importantes de `yq` sobre convertir a JSON y volver:
**preserva comentarios y formato**.

### 6.1 Preservación de comentarios

```bash
cat <<'EOF' > app.yaml
# Configuración principal
server:
  # Puerto del servidor HTTP
  port: 8080
  host: 0.0.0.0  # escuchar en todas las interfaces
EOF

# yq preserva los comentarios
yq -i '.server.port = 9090' app.yaml
cat app.yaml
# # Configuración principal
# server:
#   # Puerto del servidor HTTP
#   port: 9090
#   host: 0.0.0.0  # escuchar en todas las interfaces
```

### 6.2 Manipular comentarios

```bash
# Leer comentario de un nodo
yq '.server.port | head_comment' app.yaml
# Puerto del servidor HTTP

# Añadir comentario
yq '.server.port head_comment = "Puerto HTTP (default: 8080)"' app.yaml

# Comentario de línea (line comment)
yq '.server.host line_comment = "bind address"' app.yaml

# Comentario al pie (foot comment)
yq '.server foot_comment = "fin de server config"' app.yaml
```

---

## 7. Documentos múltiples

YAML permite múltiples documentos en un archivo, separados por `---`. Esto es
muy común en Kubernetes:

```bash
cat <<'EOF' > multi.yaml
apiVersion: v1
kind: Service
metadata:
  name: my-service
---
apiVersion: apps/v1
kind: Deployment
metadata:
  name: my-deployment
EOF

# Seleccionar documento por índice
yq 'select(documentIndex == 0)' multi.yaml
# (primer documento — el Service)

yq 'select(documentIndex == 1)' multi.yaml
# (segundo documento — el Deployment)

# Filtrar documentos por contenido
yq 'select(.kind == "Deployment")' multi.yaml

# Aplicar cambio a todos los documentos
yq '.metadata.namespace = "production"' multi.yaml
# Ambos documentos reciben el campo namespace

# Aplicar cambio solo a documentos específicos (sin perder los demás)
yq '(select(.kind == "Deployment") | .spec.replicas) = 3' multi.yaml

# Contar documentos
yq '[documentIndex] | unique | length' multi.yaml
```

```
    multi.yaml
    ┌──────────────┐
    │ --- doc 0    │  ← documentIndex == 0
    │ kind: Service│
    ├──────────────┤
    │ --- doc 1    │  ← documentIndex == 1
    │ kind: Deploy │
    └──────────────┘

    yq 'select(.kind == "Deployment")'
         → solo retorna doc 1
```

---

## 8. Merge de archivos YAML

### 8.1 Merge básico (sobrescribir)

```bash
cat <<'EOF' > defaults.yaml
server:
  port: 8080
  host: localhost
  workers: 4
logging:
  level: info
  file: /var/log/app.log
EOF

cat <<'EOF' > overrides.yaml
server:
  port: 9090
  host: 0.0.0.0
logging:
  level: debug
EOF

# Merge: overrides gana en conflictos
yq '. *+ load("overrides.yaml")' defaults.yaml
# server:
#   port: 9090          ← de overrides
#   host: 0.0.0.0       ← de overrides
#   workers: 4          ← de defaults (no sobreescrito)
# logging:
#   level: debug         ← de overrides
#   file: /var/log/app.log  ← de defaults
```

### 8.2 Operadores de merge

```bash
# * = merge recursivo (deep merge)
yq '. * load("overrides.yaml")' defaults.yaml

# *+ = merge, arrays se concatenan (no reemplazan)
yq '. *+ load("overrides.yaml")' defaults.yaml

# *d = merge, arrays se reemplazan según índice
yq '. *d load("overrides.yaml")' defaults.yaml

# *n = merge, solo añadir campos nuevos (no sobreescribir existentes)
yq '. *n load("overrides.yaml")' defaults.yaml
```

### 8.3 Merge con eval-all (ea)

```bash
# Combinar múltiples archivos
yq ea 'select(fileIndex == 0) * select(fileIndex == 1)' defaults.yaml overrides.yaml

# Merge de N archivos
yq ea '. as $item ireduce ({}; . * $item)' defaults.yaml overrides.yaml extra.yaml
```

---

## 9. Kubernetes — caso de uso principal

### 9.1 Leer manifests

```bash
cat <<'EOF' > deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: my-app
  labels:
    app: my-app
    version: "1.0"
spec:
  replicas: 2
  selector:
    matchLabels:
      app: my-app
  template:
    metadata:
      labels:
        app: my-app
    spec:
      containers:
        - name: app
          image: my-app:1.0
          ports:
            - containerPort: 8080
          env:
            - name: DB_HOST
              value: "db.example.com"
            - name: LOG_LEVEL
              value: "info"
          resources:
            requests:
              memory: "128Mi"
              cpu: "250m"
            limits:
              memory: "256Mi"
              cpu: "500m"
EOF

# Obtener imagen del contenedor
yq '.spec.template.spec.containers[0].image' deployment.yaml
# my-app:1.0

# Listar nombres de variables de entorno
yq '.spec.template.spec.containers[0].env[].name' deployment.yaml
# DB_HOST
# LOG_LEVEL

# Obtener valor de una env específica
yq '.spec.template.spec.containers[0].env[] | select(.name == "DB_HOST") | .value' deployment.yaml
# db.example.com

# Obtener limits de recursos
yq '.spec.template.spec.containers[0].resources.limits' deployment.yaml
# memory: 256Mi
# cpu: 500m
```

### 9.2 Modificar manifests

```bash
# Cambiar número de réplicas
yq -i '.spec.replicas = 5' deployment.yaml

# Actualizar imagen (deploy nuevo tag)
yq -i '.spec.template.spec.containers[0].image = "my-app:2.0"' deployment.yaml

# Cambiar variable de entorno
yq -i '
  (.spec.template.spec.containers[0].env[] | select(.name == "LOG_LEVEL")).value = "debug"
' deployment.yaml

# Añadir nueva variable de entorno
yq -i '
  .spec.template.spec.containers[0].env += [{"name": "NEW_VAR", "value": "hello"}]
' deployment.yaml

# Eliminar variable de entorno
yq -i '
  del(.spec.template.spec.containers[0].env[] | select(.name == "NEW_VAR"))
' deployment.yaml

# Añadir label
yq -i '.metadata.labels.environment = "staging"' deployment.yaml

# Añadir annotation
yq -i '.metadata.annotations."kubectl.kubernetes.io/last-applied" = "yq"' deployment.yaml
```

### 9.3 Scripting con manifests de K8s

```bash
#!/bin/bash
# update-image.sh — Actualizar imagen en todos los Deployments de un directorio
set -euo pipefail

image="${1:?Usage: $0 <new_image>}"
dir="${2:-.}"

find "$dir" -name '*.yaml' -print0 | while IFS= read -r -d '' file; do
  # Solo procesar archivos que sean Deployments
  kind=$(yq '.kind' "$file" 2>/dev/null) || continue
  [[ "$kind" == "Deployment" ]] || continue

  echo "Updating: $file"
  yq -i ".spec.template.spec.containers[0].image = \"$image\"" "$file"
done
```

```bash
#!/bin/bash
# list-images.sh — Listar todas las imágenes en manifests K8s
set -euo pipefail

find "${1:-.}" -name '*.yaml' -print0 | while IFS= read -r -d '' file; do
  yq '
    select(.kind == "Deployment" or .kind == "StatefulSet" or .kind == "DaemonSet") |
    .spec.template.spec.containers[].image
  ' "$file" 2>/dev/null
done | sort -u
```

---

## 10. Docker Compose

Otro caso de uso muy frecuente:

```bash
cat <<'EOF' > docker-compose.yaml
version: "3.8"
services:
  web:
    image: nginx:1.25
    ports:
      - "80:80"
      - "443:443"
    environment:
      - NODE_ENV=production
    volumes:
      - ./html:/usr/share/nginx/html
  db:
    image: postgres:16
    environment:
      POSTGRES_DB: myapp
      POSTGRES_USER: admin
      POSTGRES_PASSWORD: secret
    volumes:
      - db_data:/var/lib/postgresql/data
volumes:
  db_data:
EOF

# Listar servicios
yq '.services | keys' docker-compose.yaml
# - web
# - db

# Obtener imagen de un servicio
yq '.services.web.image' docker-compose.yaml
# nginx:1.25

# Listar todos los puertos
yq '.services.web.ports[]' docker-compose.yaml
# 80:80
# 443:443

# Cambiar imagen
yq -i '.services.web.image = "nginx:1.26"' docker-compose.yaml

# Añadir variable de entorno
yq -i '.services.db.environment.POSTGRES_MAX_CONNECTIONS = "200"' docker-compose.yaml

# Añadir nuevo servicio
yq -i '.services.redis = {
  "image": "redis:7",
  "ports": ["6379:6379"],
  "volumes": ["redis_data:/data"]
}' docker-compose.yaml

# Extraer configuración de un servicio como JSON (para otra herramienta)
yq -o=json '.services.db' docker-compose.yaml
```

---

## 11. Integración con shell

### 11.1 Variables de shell en yq

```bash
# Con comillas y expansión de shell
new_port=9090
yq ".server.port = $new_port" server.yaml

# Con strenv() — más seguro para strings
export APP_NAME="my-app"
yq '.metadata.name = strenv(APP_NAME)' deployment.yaml

# Con env() — accede directamente al entorno
export TAG="v2.0"
yq '.spec.template.spec.containers[0].image = "my-app:" + env(TAG)' deployment.yaml
```

**Diferencia con `jq`**: en `jq` se usa `--arg`. En `yq` (mikefarah), se usa
`env()` o `strenv()` para acceder a variables de entorno, o se interpolan
directamente con comillas dobles de Bash.

```bash
# jq:   jq --arg n "$name" '.name = $n'
# yq:   yq ".name = \"$name\""
# yq:   export name; yq '.name = env(name)'
```

### 11.2 Leer valores en variables Bash

```bash
# Un valor
hostname=$(yq '.server.hostname' server.yaml)

# Múltiples valores
read -r host port < <(
  yq '(.server.hostname) + "\t" + (.server.port | tostring)' server.yaml
)
echo "Host=$host Port=$port"

# Iterar sobre array (yq ya imprime sin comillas por defecto)
yq '.server.tags[]' server.yaml | while IFS= read -r tag; do
  echo "Tag: $tag"
done

# Iterar sobre objetos en array
yq -o=json -I=0 '.users[]' <<< "$data" | while IFS= read -r user; do
  name=$(echo "$user" | jq -r '.name')    # usar jq para el JSON resultante
  role=$(echo "$user" | jq -r '.role')
  echo "$name is $role"
done
```

### 11.3 Condicionales en scripts

```bash
#!/bin/bash
# Verificar si un campo existe antes de usarlo
if yq -e '.server.ssl' config.yaml > /dev/null 2>&1; then
  echo "SSL está configurado"
else
  echo "SSL no está configurado"
fi

# -e hace que yq retorne exit code 1 si el resultado es null/false
# Útil para condicionales en scripts

# Comparar valores
replicas=$(yq '.spec.replicas' deployment.yaml)
if (( replicas < 3 )); then
  echo "WARN: menos de 3 réplicas en producción"
fi
```

---

## 12. Expresiones avanzadas

### 12.1 `with` — scope temporal

```bash
# with() crea un scope donde . apunta al nodo seleccionado
yq '
  with(.spec.template.spec.containers[0];
    .image = "my-app:3.0" |
    .resources.limits.memory = "512Mi"
  )
' deployment.yaml

# Equivalente sin with (más verboso):
# yq '
#   .spec.template.spec.containers[0].image = "my-app:3.0" |
#   .spec.template.spec.containers[0].resources.limits.memory = "512Mi"
# '
```

### 12.2 `path` y `setpath`

```bash
# Encontrar la ruta (path) de un valor
yq '.. | select(. == "my-app:1.0") | path' deployment.yaml
# - spec
# - template
# - spec
# - containers
# - 0
# - image

# Acceder por path dinámico
yq 'getpath(["spec","replicas"])' deployment.yaml
# 2
```

### 12.3 Acumular con `map` + `add`

`yq` no tiene `reduce` como jq. Para acumular valores, se usa `map` + `add`:

```bash
echo '
items:
  - name: a
    count: 5
  - name: b
    count: 3
  - name: c
    count: 8
' | yq '.items | map(.count) | add'
# 16

# Para reduce real, convertir a JSON y usar jq:
yq -o=json '.items' data.yaml | jq 'reduce .[] as $i (0; . + $i.count)'
```

### 12.4 Concatenar y crear documentos

```bash
# Crear nuevo YAML desde cero
yq -n '
  .apiVersion = "v1" |
  .kind = "ConfigMap" |
  .metadata.name = "my-config" |
  .metadata.namespace = "default" |
  .data.APP_PORT = "8080" |
  .data.APP_ENV = "production"
'
# apiVersion: v1
# kind: ConfigMap
# metadata:
#   name: my-config
#   namespace: default
# data:
#   APP_PORT: "8080"
#   APP_ENV: production
```

---

## 13. Diferencias con jq

Si ya dominaste `jq` (T03), esta tabla te muestra las diferencias clave:

| Aspecto | jq | yq (mikefarah) |
|---------|-----|----------------|
| **Formato nativo** | JSON | YAML (también JSON, XML, TOML) |
| **Edición in-place** | No tiene (`sponge` o tmp) | `yq -i` nativo |
| **Comentarios** | JSON no soporta | Preserva comentarios YAML |
| **Multi-documento** | No aplica | `select(documentIndex == N)` |
| **Variables shell** | `--arg name "$val"` | `env(VAR)` o `strenv(VAR)` |
| **Null input** | `jq -n` | `yq -n` |
| **Raw output** | `jq -r` | Es el default en yq |
| **Output JSON** | Es el default | `yq -o=json` |
| **Merge archivos** | `jq -s '.[0] * .[1]'` | `yq '. * load("b.yaml")'` |
| **Sintaxis select** | Idéntica | Idéntica |
| **Sintaxis map** | `map(f)` | `map(f)` disponible desde v4.18; antes usar `.[] |= f` |
| **String interpolation** | `\(expr)` | No soportado (usar `+`) |
| **try-catch** | `try f catch g` | No tiene equivalente directo |
| **Funciones custom** | `def name: body;` | No soportado |

### Equivalencias comunes

```bash
# Pretty-print
jq '.'  file.json          →  yq '.'  file.yaml

# Acceso a campo
jq '.name' file.json       →  yq '.name' file.yaml

# Filtrar array
jq '.[] | select(.a > 1)'  →  yq '.[] | select(.a > 1)'

# Modificar campo
jq '.x = 1'                →  yq '.x = 1'

# Convertir a JSON
# (jq ya es JSON)          →  yq -o=json '.' file.yaml

# Convertir de JSON a YAML
# (no aplica)              →  yq -P '.' file.json

# Iterar y extraer
jq -r '.users[].name'      →  yq '.users[].name'
```

---

## 14. Recetas de producción

### 14.1 CI/CD — Actualizar versión en chart de Helm

```bash
#!/bin/bash
# bump-chart.sh — Actualizar versión de imagen en values.yaml de Helm
set -euo pipefail

new_tag="${1:?Usage: $0 <new_tag>}"
values_file="${2:-values.yaml}"

# Actualizar tag de imagen
yq -i ".image.tag = \"$new_tag\"" "$values_file"

# Actualizar appVersion en Chart.yaml si existe
if [[ -f Chart.yaml ]]; then
  yq -i ".appVersion = \"$new_tag\"" Chart.yaml
fi

echo "Updated to tag: $new_tag"
```

### 14.2 Generar ConfigMap desde archivos

```bash
#!/bin/bash
# Crear ConfigMap YAML a partir de archivos de configuración
set -euo pipefail

name="${1:?Usage: $0 <configmap-name> <file1> [file2...]}"
shift

yq -n ".apiVersion = \"v1\" | .kind = \"ConfigMap\" | .metadata.name = \"$name\""

for file in "$@"; do
  key=$(basename "$file")
  value=$(cat "$file")
  export key value
  yq -i '.data[strenv(key)] = strenv(value)' configmap.yaml
done
```

### 14.3 Validar estructura de YAML

```bash
#!/bin/bash
# Verificar que un Deployment tenga los campos requeridos
set -euo pipefail

file="${1:?Usage: $0 <deployment.yaml>}"
errors=0

check_field() {
  local path="$1" desc="$2"
  if ! yq -e "$path" "$file" > /dev/null 2>&1; then
    echo "MISSING: $desc ($path)" >&2
    (( errors++ ))
  fi
}

check_field '.metadata.name' 'deployment name'
check_field '.metadata.labels' 'labels'
check_field '.spec.replicas' 'replica count'
check_field '.spec.template.spec.containers[0].image' 'container image'
check_field '.spec.template.spec.containers[0].resources.limits' 'resource limits'

if (( errors > 0 )); then
  echo "Validation failed: $errors errors" >&2
  exit 1
fi
echo "Validation passed"
```

### 14.4 Diff de dos YAML

```bash
# Comparar dos YAML (normalizar y diff)
diff <(yq -P 'sort_keys(..)' a.yaml) <(yq -P 'sort_keys(..)' b.yaml)

# Solo diferencias en un campo
diff <(yq '.spec' v1.yaml) <(yq '.spec' v2.yaml)
```

### 14.5 GitHub Actions — editar workflow

```bash
# Cambiar versión de Node en un workflow
yq -i '
  .jobs.build.steps[] |= (
    select(.uses // "" | test("actions/setup-node")) |
    .with.node-version = "20"
  )
' .github/workflows/ci.yaml

# Añadir step a un job
yq -i '
  .jobs.build.steps += [{
    "name": "Cache dependencies",
    "uses": "actions/cache@v4",
    "with": {
      "path": "node_modules",
      "key": "deps-${{ hashFiles('"'"'package-lock.json'"'"') }}"
    }
  }]
' .github/workflows/ci.yaml
```

### 14.6 Ansible — modificar inventory

```bash
# Añadir host a grupo
yq -i '.all.children.webservers.hosts."web03" = {"ansible_host": "10.0.1.3"}' inventory.yaml

# Cambiar variable de grupo
yq -i '.all.children.webservers.vars.http_port = 8080' inventory.yaml

# Listar hosts de un grupo
yq '.all.children.webservers.hosts | keys' inventory.yaml
```

---

## 15. Diagrama de flujo de operaciones

```
    Entrada YAML/JSON/XML
         │
         ▼
    ┌─────────┐     Acceso       ┌──────────┐
    │    .     │────────────────▶│ .field    │
    │ identidad│                  │ .[n]     │
    └─────────┘                  │ .[]      │
         │                       └──────────┘
         │ pipe (|)                   │
         ▼                            ▼
    ┌─────────┐     Filtrar      ┌──────────┐
    │ select  │◀────────────────│ elementos │
    │ has     │                  └──────────┘
    │ type    │
    └─────────┘
         │
         ▼
    ┌─────────┐     Transformar  ┌──────────┐
    │  |=     │────────────────▶│ resultado │
    │ sort_by │                  └──────────┘
    │ length  │
    │ add     │
    └─────────┘
         │
         ▼
    ┌─────────┐     Escribir     ┌──────────┐
    │  = / += │────────────────▶│  salida   │
    │  del    │                  │  YAML/JSON│
    │  -i     │                  └──────────┘
    └─────────┘
```

---

## 16. Tabla de referencia rápida

| Expresión | Acción | Ejemplo |
|-----------|--------|---------|
| `.` | Identidad / pretty-print | `yq '.'` |
| `.field` | Acceder a campo | `yq '.name'` |
| `.a.b.c` | Acceso anidado | `yq '.spec.replicas'` |
| `.[n]` | Índice de array | `yq '.[0]'` |
| `.[]` | Iterar todos | `yq '.items[]'` |
| `\|` | Pipe interno | `yq '.[] \| .name'` |
| `select(cond)` | Filtrar | `select(.role == "admin")` |
| `has(key)` | ¿Tiene campo? | `has("name")` |
| `type` | Tipo del nodo | `yq '.port \| type'` |
| `keys` | Listar keys | `yq 'keys'` |
| `length` | Tamaño | `yq '.items \| length'` |
| `sort_keys(..)` | Ordenar keys recursivamente | `yq 'sort_keys(..)'` |
| `add` | Sumar/concatenar | `yq '.nums \| add'` |
| `unique` | Deduplicar array | `yq '.tags \| unique'` |
| `map(f)` | Transformar array (v4.18+) | `yq 'map(. + 1)'` |
| `.field = val` | Asignar valor | `yq '.port = 9090'` |
| `.field \|= f` | Update in-place | `yq '.port \|= . + 1'` |
| `+= [val]` | Añadir a array | `yq '.tags += ["new"]'` |
| `del(path)` | Eliminar | `yq 'del(.tmp)'` |
| `with(path; f)` | Scope temporal | `with(.a.b; .x = 1)` |
| `// val` | Valor por defecto | `yq '.x // "default"'` |
| `env(VAR)` | Variable de entorno | `yq '.name = env(NAME)'` |
| `strenv(VAR)` | Var entorno como string | `yq '.v = strenv(TAG)'` |
| `load(file)` | Cargar archivo | `yq '. * load("b.yaml")'` |
| `select(documentIndex==N)` | N-ésimo documento | `select(documentIndex==0)` |
| `-i` | Editar in-place | `yq -i '.x = 1' f.yaml` |
| `-n` | Null input (crear desde cero) | `yq -n '.x = 1'` |
| `-o=json` | Output JSON | `yq -o=json '.'` |
| `-o=xml` | Output XML | `yq -o=xml '.'` |
| `-o=toml` | Output TOML | `yq -o=toml '.'` |
| `-o=props` | Output properties | `yq -o=props '.'` |
| `-P` | Pretty-print YAML | `yq -P '.' f.json` |
| `-e` | Exit code si null/false | `yq -e '.field' f.yaml` |
| `ea` / `eval-all` | Evaluar sobre todos los archivos | `yq ea '. * .' a.yaml b.yaml` |
| `head_comment` | Leer/escribir comentario | `.x head_comment = "note"` |
| `line_comment` | Comentario de línea | `.x line_comment = "note"` |
| `.. \| select(. == val)` | Buscar recursivo | `.. \| select(. == "target")` |

---

## 17. Errores comunes

| Error | Causa | Solución |
|-------|-------|----------|
| `Error: Bad expression` | Usar sintaxis de jq (Python yq) con mikefarah yq | Verificar `yq --version`, adaptar sintaxis |
| Valores `yes`/`no` cambian | YAML interpreta como boolean | Usar comillas: `"yes"`, `"no"` |
| `null` en vez de campo vacío | Campo no existe | Verificar con `has()` o usar `// "default"` |
| Pierde comentarios | Usaste JSON como intermediario | Usar `yq` directamente, no `yq → jq → yq` |
| Indentación cambia | yq reformatea al escribir | Ajustar con `-I=N` (N = espacios de indent) |
| `map` no funciona | Versión de yq anterior a v4.18 | Actualizar yq o usar `.[] \|= f` en vez de `map(f)` |
| Variable shell no se expande | Comillas simples impiden expansión | Usar comillas dobles o `env(VAR)` |
| `-i` no guarda cambios | Expresión produce output vacío (error en filtro) | Verificar expresión sin `-i` primero |
| Merge no es recursivo | Usaste `+` en vez de `*` | `*` = deep merge, `+` = shallow merge |
| `yq` no encontrado | Versión snap no está en PATH | `sudo ln -s /snap/bin/yq /usr/local/bin/yq` |
| Tipo incorrecto después de asignar | `yq '.port = "8080"'` crea string, no int | Sin comillas para número: `yq '.port = 8080'` |
| Multi-documento: solo edita primero | Expresión no cubre todos los docs | Agregar `(select(documentIndex >= 0) \| ...)` |

---

## 18. Ejercicios

### Ejercicio 1 — Lectura básica

Dado el siguiente archivo `config.yaml`:

```yaml
app:
  name: my-service
  version: "1.2.3"
  debug: false
database:
  host: db.example.com
  port: 5432
  credentials:
    user: admin
    password: s3cret
```

Escribe un comando `yq` que extraiga el host de la base de datos.

**Predicción**: ¿Qué diferencia hay entre `yq '.database.host'` y `yq -o=json '.database.host'`?

<details>
<summary>Ver solución</summary>

```bash
yq '.database.host' config.yaml
# db.example.com

yq -o=json '.database.host' config.yaml
# "db.example.com"
```

En YAML output (default), `yq` imprime el valor directamente sin comillas:
`db.example.com`. Con `-o=json`, lo imprime como JSON string: `"db.example.com"`.
Esto es análogo a la diferencia entre `jq -r` y `jq` (sin `-r`). En `yq`, el
comportamiento "raw" es el default porque YAML no usa comillas obligatorias para strings.
</details>

---

### Ejercicio 2 — Modificar valores

Usando `config.yaml`, cambia el puerto de la base de datos a `3306` y el debug a `true`.

**Predicción**: ¿Qué pasa si ejecutas `yq '.database.port = "3306"'` (con comillas)?

<details>
<summary>Ver solución</summary>

```bash
# Correcto — dos cambios en una expresión con pipe
yq '.database.port = 3306 | .app.debug = true' config.yaml

# In-place
yq -i '.database.port = 3306 | .app.debug = true' config.yaml
```

Si usas `yq '.database.port = "3306"'` (con comillas), yq asigna el **string** `"3306"`
en vez del **entero** `3306`. En YAML la diferencia es visible:

```yaml
# Sin comillas → entero
port: 3306

# Con comillas → string
port: "3306"
```

Esto importa cuando otra herramienta (Ansible, Kubernetes, una librería YAML)
parsea el valor y espera un tipo específico. Kubernetes rechazaría un port
como string en muchos contextos.
</details>

---

### Ejercicio 3 — Arrays

Dado:

```yaml
servers:
  - hostname: web01
    ip: 10.0.1.1
    roles:
      - frontend
      - ssl
  - hostname: web02
    ip: 10.0.1.2
    roles:
      - frontend
  - hostname: db01
    ip: 10.0.2.1
    roles:
      - database
      - backup
```

Extrae los hostnames de todos los servidores que tengan el rol `"frontend"`.

**Predicción**: ¿Cómo se comporta `select` con arrays anidados — busca dentro del sub-array de roles?

<details>
<summary>Ver solución</summary>

```bash
yq '.servers[] | select(.roles[] == "frontend") | .hostname' servers.yaml
# web01
# web02
```

`select(.roles[] == "frontend")` funciona porque `.roles[]` itera los elementos
del sub-array `roles` para cada servidor. Si **alguno** de los roles es `"frontend"`,
la condición es true y el servidor completo se selecciona. Es como un `ANY` implícito:
yq expande `.roles[]` y si al menos una expansión hace que `== "frontend"` sea true,
el `select` pasa el elemento.

Si `web01` tiene `[frontend, ssl]`, la expansión produce dos evaluaciones:
- `"frontend" == "frontend"` → true → select pasa
- `"ssl" == "frontend"` → false

Con que una sea true, el servidor se incluye en el resultado.
</details>

---

### Ejercicio 4 — Edición in-place con -i

Dado `deployment.yaml`:

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: api-server
spec:
  replicas: 1
  template:
    spec:
      containers:
        - name: api
          image: api-server:latest
          env:
            - name: PORT
              value: "8080"
```

Escribe los comandos para: (1) cambiar réplicas a 3, (2) actualizar la imagen a `api-server:v2.1.0`, (3) añadir la variable de entorno `ENV=production`.

**Predicción**: ¿Por qué se usa `"8080"` (con comillas) para el value de PORT en el YAML original?

<details>
<summary>Ver solución</summary>

```bash
# 1. Cambiar réplicas
yq -i '.spec.replicas = 3' deployment.yaml

# 2. Actualizar imagen
yq -i '.spec.template.spec.containers[0].image = "api-server:v2.1.0"' deployment.yaml

# 3. Añadir variable de entorno
yq -i '.spec.template.spec.containers[0].env += [{"name": "ENV", "value": "production"}]' deployment.yaml

# También se puede hacer todo en un solo comando:
yq -i '
  .spec.replicas = 3 |
  .spec.template.spec.containers[0].image = "api-server:v2.1.0" |
  .spec.template.spec.containers[0].env += [{"name": "ENV", "value": "production"}]
' deployment.yaml
```

El `value: "8080"` lleva comillas porque en Kubernetes los `env[].value` son **siempre strings**.
Si escribieras `value: 8080` sin comillas, YAML lo parsearía como entero, y Kubernetes
lo rechazaría con un error de validación (`expected string, got int`). Las comillas
fuerzan que `8080` sea tratado como el string `"8080"`.
</details>

---

### Ejercicio 5 — Merge de configuración

Dados `defaults.yaml` y `production.yaml`:

```yaml
# defaults.yaml
app:
  port: 8080
  workers: 2
  log_level: info
database:
  host: localhost
  port: 5432
```

```yaml
# production.yaml
app:
  workers: 8
  log_level: warn
database:
  host: db.prod.internal
```

Haz un merge donde `production.yaml` sobrescriba los valores de `defaults.yaml`.

**Predicción**: Después del merge, ¿qué valor tiene `app.port`? ¿Y `database.port`?

<details>
<summary>Ver solución</summary>

```bash
yq '. * load("production.yaml")' defaults.yaml
# app:
#   port: 8080          ← de defaults (no está en production)
#   workers: 8           ← de production
#   log_level: warn      ← de production
# database:
#   host: db.prod.internal  ← de production
#   port: 5432           ← de defaults (no está en production)
```

`app.port` vale `8080` (de defaults) porque `production.yaml` no define `app.port`.
`database.port` vale `5432` (de defaults) por la misma razón. El operador `*` hace
deep merge: solo sobrescribe los campos que existen en el segundo archivo, preservando
los demás. Es exactamente el patrón de "defaults + overrides" que se usa en
configuración de aplicaciones.
</details>

---

### Ejercicio 6 — Multi-documento

Dado un archivo con dos documentos YAML:

```yaml
apiVersion: v1
kind: Service
metadata:
  name: web-service
spec:
  ports:
    - port: 80
---
apiVersion: apps/v1
kind: Deployment
metadata:
  name: web-deploy
spec:
  replicas: 2
```

Añade `metadata.namespace = "staging"` **solo al Deployment**, sin modificar el Service.

**Predicción**: ¿Qué pasa si ejecutas `yq '.metadata.namespace = "staging"'` sin filtrar por kind?

<details>
<summary>Ver solución</summary>

```bash
# Solo al Deployment (los paréntesis preservan los otros docs en el output)
yq '(select(.kind == "Deployment") | .metadata.namespace) = "staging"' multi.yaml

# Alternativa con documentIndex
yq '(select(documentIndex == 1) | .metadata.namespace) = "staging"' multi.yaml
```

Si ejecutas `yq '.metadata.namespace = "staging"'` sin filtrar, **ambos documentos**
reciben el campo `namespace`. yq aplica la expresión a cada documento del archivo
por separado. Cada documento pasa por el pipeline de forma independiente, y como
ambos tienen `.metadata`, ambos serían modificados.

Esto es un error muy común en manifests multi-documento de Kubernetes — siempre
filtrar con `select(.kind == "...")` cuando quieres modificar un recurso específico.
</details>

---

### Ejercicio 7 — Conversión de formatos

Dado `config.json`:

```json
{
  "database": {
    "host": "localhost",
    "port": 5432,
    "ssl": true,
    "options": {
      "pool_size": 10,
      "timeout": 30
    }
  }
}
```

Conviértelo a YAML, y luego extrae solo la sección `options` como JSON.

**Predicción**: Al convertir JSON → YAML → JSON, ¿se preservan los tipos exactos (int, bool)?

<details>
<summary>Ver solución</summary>

```bash
# JSON → YAML
yq -P '.' config.json > config.yaml
# database:
#   host: localhost
#   port: 5432
#   ssl: true
#   options:
#     pool_size: 10
#     timeout: 30

# Extraer options como JSON
yq -o=json '.database.options' config.yaml
# {
#   "pool_size": 10,
#   "timeout": 30
# }
```

Sí, los tipos se preservan. `yq` mantiene internamente el tipo de cada nodo:
`5432` sigue siendo int, `true` sigue siendo bool, `"localhost"` sigue siendo
string. La conversión JSON → YAML → JSON es lossless para los tipos estándar
compartidos (string, number, boolean, null, array, object).

La única excepción potencial: JSON no distingue entre int y float (`5432` y `5432.0`
son ambos "number"), mientras que YAML sí distingue (`!!int` vs `!!float`). Pero en
la práctica, `yq` maneja esto correctamente en el roundtrip.
</details>

---

### Ejercicio 8 — Variables de entorno

Escribe un script que lea `ENVIRONMENT` y `VERSION` del entorno y actualice un `values.yaml` de Helm:

```yaml
image:
  repository: my-app
  tag: latest
config:
  environment: development
  replicas: 1
```

**Predicción**: ¿Por qué `env()` es más seguro que interpolar `$VAR` dentro de comillas dobles?

<details>
<summary>Ver solución</summary>

```bash
#!/bin/bash
set -euo pipefail

: "${ENVIRONMENT:?ENVIRONMENT is required}"
: "${VERSION:?VERSION is required}"

export ENVIRONMENT VERSION

yq -i '
  .image.tag = env(VERSION) |
  .config.environment = env(ENVIRONMENT) |
  .config.replicas = (
    if env(ENVIRONMENT) == "production" then 3
    elif env(ENVIRONMENT) == "staging" then 2
    else 1
    end
  )
' values.yaml

echo "Updated values.yaml: env=$ENVIRONMENT version=$VERSION"
```

`env()` es más seguro porque:
1. **Sin inyección**: si `$VERSION` contiene caracteres especiales como `"; rm -rf /`,
   con comillas dobles se rompería la expresión yq o ejecutaría algo inesperado.
   `env(VERSION)` pasa el valor como dato, no como código.
2. **Tipos correctos**: `env()` siempre retorna string. Si necesitas número,
   lo conviertes explícitamente. Con interpolación Bash, podrías crear YAML
   inválido si el valor tiene `:`, `#`, u otros caracteres especiales para YAML.
3. **No requiere escapar**: no necesitas manejar comillas dentro de comillas.
</details>

---

### Ejercicio 9 — Docker Compose con yq

Dado un `docker-compose.yaml`:

```yaml
version: "3.8"
services:
  api:
    image: api:latest
    ports:
      - "8080:8080"
    environment:
      DATABASE_URL: "postgres://localhost/dev"
  worker:
    image: worker:latest
    environment:
      DATABASE_URL: "postgres://localhost/dev"
      QUEUE: default
```

Actualiza **todos** los servicios que tengan `DATABASE_URL` para apuntar a `postgres://db.prod/myapp`.

**Predicción**: ¿Qué hace el operador `|=` cuando se aplica a múltiples nodos que coinciden?

<details>
<summary>Ver solución</summary>

```bash
# Opción 1: asignación directa con select para filtrar servicios que tienen la key
yq -i '
  (.services[].environment | select(has("DATABASE_URL"))).DATABASE_URL = "postgres://db.prod/myapp"
' docker-compose.yaml

# Opción 2: usando |= para update in-place
yq -i '
  .services[].environment.DATABASE_URL |= (select(. != null) | "postgres://db.prod/myapp")
' docker-compose.yaml
```

El operador `|=` aplicado a múltiples nodos actualiza **cada uno independientemente**.
Cuando escribes `.services[].environment.DATABASE_URL |= f`, yq:

1. Expande `.services[]` → `api`, `worker`
2. Para cada servicio, navega a `.environment.DATABASE_URL`
3. Aplica `f` al valor actual de cada nodo encontrado (`.` dentro de `f` es el valor existente)

La diferencia con `=`: `=` reemplaza con un valor absoluto, `|=` transforma el valor
existente. Aquí el resultado es el mismo, pero `|=` es útil cuando la transformación
depende del valor actual (e.g., `.version |= . + 1`).
</details>

---

### Ejercicio 10 — Pipeline completo

Escribe un script que reciba un directorio con manifests de Kubernetes y genere un
reporte de todos los Deployments: nombre, namespace, imagen y número de réplicas.

**Predicción**: ¿Por qué conviene usar `yq -o=json` y luego `jq` para reportes
tabulares, en lugar de hacer todo con yq?

<details>
<summary>Ver solución</summary>

```bash
#!/bin/bash
set -euo pipefail

dir="${1:?Usage: $0 <manifests_dir>}"

# Header
printf "%-25s %-15s %-35s %s\n" "NAME" "NAMESPACE" "IMAGE" "REPLICAS"
printf "%-25s %-15s %-35s %s\n" "----" "---------" "-----" "--------"

find "$dir" -name '*.yaml' -print0 | while IFS= read -r -d '' file; do
  # yq procesa cada documento del archivo
  yq -o=json -I=0 'select(.kind == "Deployment")' "$file" 2>/dev/null \
    | while IFS= read -r deployment; do
        [[ -z "$deployment" ]] && continue
        name=$(echo "$deployment" | jq -r '.metadata.name')
        ns=$(echo "$deployment" | jq -r '.metadata.namespace // "default"')
        image=$(echo "$deployment" | jq -r '.spec.template.spec.containers[0].image')
        replicas=$(echo "$deployment" | jq -r '.spec.replicas // 1')

        printf "%-25s %-15s %-35s %s\n" "$name" "$ns" "$image" "$replicas"
      done
done
```

Conviene `yq -o=json | jq` porque:
1. **jq tiene mejor soporte tabular**: `@tsv`, `@csv`, string interpolation (`\(expr)`),
   y funciones como `tostring`. yq (mikefarah) no tiene `@tsv` ni string interpolation.
2. **jq es más maduro para transformaciones complejas**: `def`, `try-catch`, `reduce`
   con sintaxis más limpia.
3. **Separación de responsabilidades**: `yq` maneja la lectura YAML (estructura,
   comentarios, multi-documento), `jq` maneja la transformación y formateo de datos.
4. **Performance**: para archivos grandes, `jq` es generalmente más rápido que `yq`
   en transformaciones de datos puros.

El patrón `yq -o=json | jq` es estándar en la industria y combina lo mejor de
ambas herramientas.
</details>
