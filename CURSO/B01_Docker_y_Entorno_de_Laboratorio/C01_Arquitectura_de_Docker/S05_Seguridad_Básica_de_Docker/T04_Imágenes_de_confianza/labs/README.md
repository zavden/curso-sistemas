# Lab — Imagenes de confianza

## Objetivo

Aprender a evaluar la confiabilidad de imagenes Docker antes de usarlas:
inspeccionar metadata, verificar firmas con Docker Content Trust, escanear
vulnerabilidades, y usar pull por digest para garantizar reproducibilidad.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagenes descargadas: `docker pull nginx:latest`, `docker pull alpine:latest`
- Estar en el directorio `labs/` de este topico
- Acceso a Internet
- Opcional: `docker scout` disponible para la Parte 3

---

## Parte 1 — Inspeccion de imagenes

### Objetivo

Examinar la metadata de imagenes Docker para obtener informacion sobre su
origen, configuracion, y nivel de confianza antes de ejecutarlas.

### Paso 1.1: Inspeccionar labels de una imagen oficial

```bash
docker inspect nginx:latest --format '{{json .Config.Labels}}' | python3 -m json.tool
```

Salida esperada (aproximada):

```json
{
    "maintainer": "NGINX Docker Maintainers <docker-maint@nginx.com>"
}
```

Las imagenes oficiales incluyen labels con informacion del mantenedor.
Esto es un indicador de calidad y trazabilidad.

### Paso 1.2: Verificar el usuario configurado

```bash
echo "=== nginx ==="
docker inspect nginx:latest --format 'User: "{{.Config.User}}"'

echo ""
echo "=== alpine ==="
docker inspect alpine:latest --format 'User: "{{.Config.User}}"'
```

Salida esperada:

```
=== nginx ===
User: ""

=== alpine ===
User: ""
```

Un User vacio indica que la imagen corre como root por defecto. Esto
no siempre es un problema — nginx, por ejemplo, hace drop de privilegios
internamente — pero es informacion importante para evaluar el riesgo.

### Paso 1.3: Ver el historial de capas

```bash
docker history nginx:latest --format "table {{.CreatedBy}}\t{{.Size}}" --no-trunc 2>/dev/null | head -15
```

El historial muestra los comandos que se ejecutaron para construir la
imagen. En imagenes oficiales, cada capa es auditable.

Antes de continuar, responde mentalmente:

- ?Puedes identificar que paquetes se instalaron?
- ?Puedes ver si la imagen descarga algo de una URL externa?
- ?Por que es importante que el historial sea visible?

Intenta responder antes de continuar al siguiente paso.

### Paso 1.4: Examinar los puertos expuestos y el CMD

```bash
echo "=== Puertos expuestos ==="
docker inspect nginx:latest --format '{{json .Config.ExposedPorts}}' | python3 -m json.tool

echo ""
echo "=== CMD ==="
docker inspect nginx:latest --format '{{json .Config.Cmd}}' | python3 -m json.tool

echo ""
echo "=== Entrypoint ==="
docker inspect nginx:latest --format '{{json .Config.Entrypoint}}' | python3 -m json.tool
```

Salida esperada (aproximada):

```
=== Puertos expuestos ===
{
    "80/tcp": {}
}

=== CMD ===
[
    "nginx",
    "-g",
    "daemon off;"
]

=== Entrypoint ===
[
    "/docker-entrypoint.sh"
]
```

Una imagen confiable expone solo los puertos necesarios y tiene un CMD/
Entrypoint claro y documentado.

### Paso 1.5: Ver la fecha de creacion

```bash
docker inspect nginx:latest --format '{{.Created}}'
```

Salida esperada (aproximada):

```
2025-xx-xxT...
```

Una imagen actualizada recientemente es mas probable que tenga parches de
seguridad aplicados. Una imagen con fecha de hace anos es sospechosa.

### Paso 1.6: Comparar tamanos entre variantes

```bash
docker pull nginx:alpine 2>/dev/null

docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}" | grep nginx
```

Salida esperada (aproximada):

```
nginx   latest   ~190MB
nginx   alpine   ~45MB
```

Imagenes mas pequenas tienen menos superficie de ataque: menos paquetes
instalados implica menos CVEs potenciales.

---

## Parte 2 — Docker Content Trust

### Objetivo

Entender como Docker Content Trust (DCT) verifica la firma de imagenes
y como habilitarlo para rechazar imagenes sin firmar.

### Paso 2.1: Inspeccionar la firma de una imagen oficial

```bash
docker trust inspect nginx --pretty 2>/dev/null || echo "docker trust no disponible o imagen no firmada localmente"
```

Salida esperada (aproximada):

```
Signatures for nginx

SIGNED TAG    DIGEST                    SIGNERS
latest        <sha256:hash>             docker-library

Administrative keys for nginx

  Repository Key: <hash>
  Root Key:       <hash>
```

Las imagenes oficiales estan firmadas por `docker-library`. La firma
garantiza que la imagen fue publicada por quien dice ser.

### Paso 2.2: Habilitar Docker Content Trust

```bash
export DOCKER_CONTENT_TRUST=1
echo "DOCKER_CONTENT_TRUST=$DOCKER_CONTENT_TRUST"
```

Con DCT habilitado, `docker pull` rechazara imagenes sin firma valida.

### Paso 2.3: Pull de imagen firmada

```bash
docker pull alpine:latest
```

Salida esperada (aproximada):

```
Pull (1 of 1): alpine:latest@sha256:<hash>
...
Tagging alpine:latest@sha256:<hash> as alpine:latest
```

La imagen oficial alpine tiene firma y se descarga correctamente con
DCT habilitado. Observa que Docker muestra el digest (hash SHA256).

### Paso 2.4: Desactivar DCT para continuar

```bash
unset DOCKER_CONTENT_TRUST
echo "DOCKER_CONTENT_TRUST desactivado"
```

Antes de continuar, responde mentalmente:

- ?Que pasaria si intentas descargar una imagen community sin firma con
  DCT habilitado?
- ?En que entornos seria apropiado habilitar DCT permanentemente?
- ?Que limitaciones tiene DCT (imagenes community que necesitas pero no
  estan firmadas)?

Intenta responder antes de continuar al siguiente paso.

### Paso 2.5: Resumen de DCT

DCT es util en entornos controlados donde solo se permiten imagenes
verificadas. En desarrollo local, puede ser restrictivo porque muchas
imagenes community utiles no estan firmadas. La decision de habilitarlo
depende de tu politica de seguridad.

---

## Parte 3 — Escaneo de vulnerabilidades

### Objetivo

Usar herramientas de escaneo para detectar vulnerabilidades conocidas
(CVEs) en imagenes Docker y comparar entre diferentes variantes.

### Paso 3.1: Verificar si docker scout esta disponible

```bash
docker scout version 2>/dev/null || echo "docker scout no esta disponible en este sistema"
```

Si `docker scout` no esta disponible, lee los pasos siguientes y las
salidas esperadas para entender el flujo. Los conceptos son aplicables
con cualquier herramienta de escaneo (Trivy, Grype, Snyk, etc.).

### Paso 3.2: Escaneo rapido de una imagen oficial

```bash
docker scout quickview nginx:latest 2>/dev/null || echo "docker scout no disponible - ver salida esperada abajo"
```

Salida esperada (aproximada):

```
  Target     : nginx:latest
  digest     : sha256:<hash>
  platform   : linux/amd64

  Vulnerabilities:
    Critical     0
    High         2
    Medium       5
    Low          12
    Unspecified  3
```

Los numeros varian con el tiempo a medida que se descubren nuevas
vulnerabilidades y se publican parches.

### Paso 3.3: Escaneo detallado de CVEs

```bash
docker scout cves nginx:latest --only-severity critical,high 2>/dev/null | head -30 || \
    echo "docker scout no disponible - ver salida esperada abajo"
```

Salida esperada (aproximada):

```
  ...
  CVE-2024-XXXX  HIGH   libcurl 7.xx.x  (fixed in 7.xx.y)
  CVE-2024-YYYY  HIGH   openssl 3.x.x   (fixed in 3.x.y)
  ...
```

Cada CVE indica: el identificador, la severidad, el paquete afectado,
y la version que corrige el problema.

### Paso 3.4: Predecir diferencias entre imagenes

Antes de ejecutar el siguiente paso, responde mentalmente:

- Entre `alpine:latest` (~7MB) y `nginx:latest` (~190MB), ?cual tendra
  mas vulnerabilidades?
- ?Por que el tamano de la imagen se relaciona con la cantidad de CVEs?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.5: Comparar alpine con imagen full

```bash
echo "=== alpine (imagen minima) ==="
docker scout quickview alpine:latest 2>/dev/null || echo "scout no disponible"

echo ""
echo "=== nginx (imagen basada en Debian) ==="
docker scout quickview nginx:latest 2>/dev/null || echo "scout no disponible"
```

Salida esperada (conceptual):

```
=== alpine (imagen minima) ===
  Vulnerabilities: 0 Critical, 0 High, ~1-3 Medium

=== nginx (imagen basada en Debian) ===
  Vulnerabilities: 0 Critical, ~2 High, ~5 Medium
```

Imagenes mas pequenas (alpine) tienden a tener menos vulnerabilidades
porque contienen menos paquetes. Esta es una razon practica para
preferir imagenes slim o alpine cuando sea posible.

### Paso 3.6: Auditar una imagen con docker history

Independientemente de si `docker scout` esta disponible, `docker history`
permite auditar que comandos construyeron la imagen:

```bash
docker history nginx:latest --format "table {{.CreatedBy}}\t{{.Size}}" | head -10
```

Busca patrones sospechosos como:

- `curl` o `wget` descargando desde URLs desconocidas
- `chmod +x` sobre binarios descargados
- Capas con tamanos inesperados
- Copias de archivos sin contexto claro

### Paso 3.7: Interpretar resultados de escaneo

No todas las CVEs requieren accion inmediata:

| Severidad | Accion |
|---|---|
| CRITICAL | Parchear inmediatamente. Explotable remotamente |
| HIGH | Parchear pronto. Explotable con condiciones especificas |
| MEDIUM | Evaluar si aplica a tu caso de uso |
| LOW | Monitorear. Riesgo bajo |

Un aspecto importante: una CVE en OpenSSL no te afecta si tu aplicacion
no usa las funciones vulnerables. Evalua si la CVE es **explotable en tu
contexto** antes de actuar.

---

## Parte 4 — Pull por digest

### Objetivo

Usar digests SHA256 para descargar imagenes de forma inmutable,
garantizando que obtienes exactamente la misma imagen cada vez.

### Paso 4.1: Entender el problema con tags

```bash
docker inspect nginx:latest --format '{{index .RepoDigests 0}}'
```

Salida esperada (aproximada):

```
nginx@sha256:<hash_largo>
```

El tag `latest` puede apuntar a imagenes diferentes en momentos
diferentes. El mantenedor puede re-publicar `latest` apuntando a una
version nueva en cualquier momento. El digest, en cambio, es **inmutable**:
identifica una imagen exacta por su contenido.

### Paso 4.2: Obtener el digest actual

```bash
docker images nginx --digests --format "table {{.Repository}}\t{{.Tag}}\t{{.Digest}}"
```

Salida esperada (aproximada):

```
REPOSITORY   TAG      DIGEST
nginx        latest   sha256:<hash>
nginx        alpine   sha256:<otro_hash>
```

Cada combinacion de imagen + plataforma tiene un digest unico.

### Paso 4.3: Pull por digest

Copia el digest de la salida anterior y ejecuta:

```bash
NGINX_DIGEST=$(docker inspect nginx:latest --format '{{index .RepoDigests 0}}')
echo "Descargando: $NGINX_DIGEST"

docker pull "$NGINX_DIGEST"
```

Salida esperada (aproximada):

```
Descargando: nginx@sha256:<hash>
...
Digest: sha256:<hash>
Status: Image is up to date for nginx@sha256:<hash>
```

Esta descarga producira **exactamente** la misma imagen sin importar
cuando la ejecutes. El tag `latest` puede cambiar, pero el digest siempre
apunta al mismo contenido.

### Paso 4.4: Comparar tag vs digest en Dockerfiles

En Dockerfiles de produccion, es recomendable usar digests:

```dockerfile
# Mutable: puede cambiar sin aviso
FROM nginx:latest

# Mejor: version fija pero el tag puede ser re-publicado
FROM nginx:1.25.4

# Inmutable: siempre la misma imagen exacta
FROM nginx@sha256:<hash>
```

Antes de continuar, responde mentalmente:

- ?Por que `latest` no significa "la mas reciente"?
- ?En que situacion usarias un digest vs un tag con version?
- ?Cual es la desventaja de usar digests? (pista: legibilidad y
  actualizaciones)

Intenta responder antes de continuar.

### Paso 4.5: Verificar la inmutabilidad

```bash
DIGEST=$(docker inspect nginx:latest --format '{{index .RepoDigests 0}}' | cut -d@ -f2)

echo "Digest actual de nginx:latest: $DIGEST"
echo ""
echo "Este digest identifica de forma unica el contenido de la imagen."
echo "Si nginx:latest se actualiza en Docker Hub, su digest cambiara."
echo "Pero si usas el digest directamente, siempre obtendras esta version exacta."
```

---

## Limpieza final

```bash
# Este lab no crea imagenes custom. Las imagenes publicas descargadas
# se mantienen para uso en otros laboratorios.

# Si deseas eliminar la variante alpine descargada en la Parte 1:
docker rmi nginx:alpine 2>/dev/null
```

---

## Conceptos reforzados

1. `docker inspect` permite examinar la metadata de una imagen antes de
   ejecutarla: labels, usuario, puertos expuestos, CMD, entrypoint, y
   fecha de creacion.

2. **Docker Content Trust** (DCT) verifica que las imagenes esten firmadas
   por su publicador. Con `DOCKER_CONTENT_TRUST=1`, Docker rechaza imagenes
   sin firma valida.

3. Las herramientas de escaneo (`docker scout`, Trivy, Grype) detectan
   CVEs en las dependencias de una imagen. No todas las CVEs son
   explotables en tu contexto — requieren evaluacion.

4. Imagenes mas pequenas (alpine, slim) tienen **menos superficie de
   ataque**: menos paquetes instalados implica menos vulnerabilidades
   potenciales.

5. Los tags son **mutables** — `latest` puede apuntar a imagenes diferentes
   en momentos diferentes. Los digests SHA256 son **inmutables** y
   garantizan reproducibilidad.

6. `docker history` permite auditar los comandos que construyeron una
   imagen, util para detectar patrones sospechosos como descargas de
   URLs desconocidas o binarios ejecutables.
