# Lab — Multi-stage builds

## Objetivo

Laboratorio práctico para entender multi-stage builds: reducir drásticamente
el tamaño de imágenes separando el stage de build del stage de runtime,
usar `FROM scratch` para imágenes mínimas, copiar entre stages con
`COPY --from`, y crear variantes dev/prod con `--target`.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imágenes base descargadas:
  - `docker pull debian:bookworm`
  - `docker pull debian:bookworm-slim`
  - `docker pull alpine:latest`
- Estar en el directorio `labs/` de este tópico

## Archivos del laboratorio

| Archivo | Propósito |
|---|---|
| `hello.c` | Programa C para compilar (Partes 1-3) |
| `app.py` | Script Python para demo de --target (Parte 4) |
| `Dockerfile.single` | Single-stage: gcc + binario juntos (Parte 1) |
| `Dockerfile.multi` | Multi-stage: builder → runtime slim (Parte 1) |
| `Dockerfile.multi-scratch` | Multi-stage con FROM scratch (Parte 2) |
| `Dockerfile.target` | Dev vs prod con --target (Parte 4) |

---

## Parte 1 — Single-stage vs multi-stage

### Objetivo

Comparar el tamaño de una imagen single-stage (que incluye el compilador) con
una multi-stage (que solo incluye el binario compilado).

### Paso 1.1: Examinar los Dockerfiles

```bash
echo "=== Single-stage ==="
cat Dockerfile.single

echo ""
echo "=== Multi-stage ==="
cat Dockerfile.multi
```

Ambos compilan el mismo programa C. La diferencia:

- `Dockerfile.single`: una sola imagen con gcc + binario
- `Dockerfile.multi`: stage 1 compila con gcc, stage 2 solo copia el binario

Antes de construir, predice: ¿cuánto más pequeña será la multi-stage?

### Paso 1.2: Construir ambas

```bash
docker build -f Dockerfile.single -t lab-single .
docker build -f Dockerfile.multi  -t lab-multi  .
```

### Paso 1.3: Comparar tamaños

```bash
docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}" | grep lab-
```

Salida esperada (aproximada):

```
REPOSITORY   TAG       SIZE
lab-multi    latest    ~80MB
lab-single   latest    ~350MB
```

La multi-stage es ~4x más pequeña porque no incluye gcc ni las herramientas
de compilación.

### Paso 1.4: Verificar que ambas funcionan

```bash
docker run --rm lab-single
docker run --rm lab-multi
```

Misma salida:

```
Hello from a multi-stage build!
This binary was compiled in stage 1 and copied to stage 2.
```

### Paso 1.5: Verificar que gcc no está en la multi-stage

```bash
docker run --rm lab-single which gcc
# /usr/bin/gcc

docker run --rm lab-multi which gcc 2>/dev/null || echo "gcc no encontrado (correcto)"
# gcc no encontrado (correcto)
```

El stage de build fue descartado. Solo el binario sobrevivió.

### Paso 1.6: Inspeccionar las capas

```bash
echo "=== Single-stage layers ==="
docker history lab-single --format "table {{.CreatedBy}}\t{{.Size}}" | head -8

echo ""
echo "=== Multi-stage layers ==="
docker history lab-multi --format "table {{.CreatedBy}}\t{{.Size}}" | head -5
```

La multi-stage tiene menos capas y tamaños mucho menores porque solo
contiene `debian:bookworm-slim` + el binario copiado.

### Paso 1.7: Limpiar

```bash
docker rmi lab-single lab-multi
```

---

## Parte 2 — FROM scratch: imagen mínima absoluta

### Objetivo

Demostrar que con `FROM scratch` se puede crear una imagen que contiene
**solo** el binario compilado — sin sistema operativo, sin shell, sin nada.

### Paso 2.1: Examinar el Dockerfile

```bash
cat Dockerfile.multi-scratch
```

El stage 2 usa `FROM scratch` — una imagen completamente vacía.

Antes de construir, responde mentalmente:

- ¿Cuánto pesará esta imagen?
- ¿Podrás ejecutar `docker run --rm lab-scratch bash`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.2: Construir y comparar

```bash
docker build -f Dockerfile.multi-scratch -t lab-scratch .

docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}" | grep lab-scratch
```

Salida esperada (aproximada):

```
REPOSITORY   TAG       SIZE
lab-scratch  latest    ~1MB
```

La imagen pesa ~1MB — solo el binario compilado estáticamente.

### Paso 2.3: Ejecutar

```bash
docker run --rm lab-scratch
```

```
Hello from a multi-stage build!
This binary was compiled in stage 1 and copied to stage 2.
```

Funciona. El binario está compilado estáticamente (`gcc -static`), así que
no necesita librerías del sistema.

### Paso 2.4: Verificar que no hay nada más

```bash
# No hay shell
docker run --rm lab-scratch sh 2>&1 || echo "No hay shell en la imagen"

# No hay ls
docker run --rm --entrypoint ls lab-scratch 2>&1 || echo "No hay ls en la imagen"
```

`FROM scratch` no tiene shell, ni ls, ni cat, ni nada. Solo el binario.
Esto maximiza la seguridad (menor superficie de ataque) pero dificulta
el debugging.

### Paso 2.5: Comparar los tres tamaños

```bash
docker build -f Dockerfile.single -t lab-single .

docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}" | grep lab-
```

| Imagen | Base | Tamaño |
|---|---|---|
| `lab-single` | debian:bookworm + gcc | ~350MB |
| (multi) | debian:bookworm-slim | ~80MB |
| `lab-scratch` | scratch | ~1MB |

De 350MB a 1MB — 99.7% de reducción.

### Paso 2.6: Limpiar

```bash
docker rmi lab-single lab-scratch
```

---

## Parte 3 — COPY --from

### Objetivo

Demostrar que `COPY --from` puede copiar archivos no solo de stages del
mismo Dockerfile, sino también de imágenes externas.

### Paso 3.1: COPY --from entre stages

Ya lo usamos en las Partes 1 y 2. La línea clave:

```dockerfile
COPY --from=builder /src/hello /usr/local/bin/hello
```

Copia el binario del stage `builder` al stage actual. Se referencia el stage
por su nombre (`AS builder`) o por índice (`--from=0`).

### Paso 3.2: COPY --from de una imagen externa

```bash
docker run --rm alpine:latest cat /etc/os-release
```

Ahora copiar un archivo de `alpine:latest` a `debian:bookworm-slim`:

```bash
docker build -t lab-copy-from - << 'DOCKERFILE'
FROM debian:bookworm-slim
COPY --from=alpine:latest /etc/os-release /alpine-os-release
CMD ["sh", "-c", "echo '=== Debian ===' && cat /etc/os-release | head -2 && echo '=== Alpine (copied) ===' && cat /alpine-os-release | head -2"]
DOCKERFILE

docker run --rm lab-copy-from
```

Salida esperada:

```
=== Debian ===
PRETTY_NAME="Debian GNU/Linux 12 (bookworm)"
NAME="Debian GNU/Linux"
=== Alpine (copied) ===
NAME="Alpine Linux"
ID=alpine
```

Se copió `/etc/os-release` de Alpine a la imagen de Debian. Esto es útil
para extraer binarios o configuraciones de otras imágenes sin necesidad
de instalarlos.

### Paso 3.3: Limpiar

```bash
docker rmi lab-copy-from
```

---

## Parte 4 — --target: variantes dev y prod

### Objetivo

Demostrar `--target` para construir diferentes variantes de imagen desde
el mismo Dockerfile: una para desarrollo (con herramientas extra) y una
para producción (mínima, non-root).

### Paso 4.1: Examinar el Dockerfile

```bash
cat Dockerfile.target
```

Tres stages:

- `base`: instalación base con Python
- `development`: herramientas de desarrollo (ipython)
- `production`: mínima, usuario non-root

### Paso 4.2: Construir dev y prod

```bash
docker build --target development -t lab-target-dev  .
docker build --target production  -t lab-target-prod .
```

### Paso 4.3: Comparar tamaños

```bash
docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}" | grep lab-target
```

`lab-target-dev` debería ser más grande que `lab-target-prod` (incluye
ipython y pip).

### Paso 4.4: Ejecutar cada variante

```bash
echo "=== Production ==="
docker run --rm lab-target-prod

echo ""
echo "=== Development ==="
docker run --rm lab-target-dev
```

Producción ejecuta la app normalmente. Desarrollo abre un prompt interactivo
de Python.

### Paso 4.5: Verificar usuario en producción

```bash
docker run --rm lab-target-prod id
```

Salida esperada:

```
uid=65534(nobody) gid=65534(nobody)
```

El stage de producción usa `USER nobody` — no ejecuta como root.

```bash
docker run --rm lab-target-dev id
```

El stage de desarrollo ejecuta como root (para poder instalar herramientas).

### Paso 4.6: Limpiar

```bash
docker rmi lab-target-dev lab-target-prod
```

---

## Limpieza final

```bash
# Eliminar imágenes del lab (si alguna quedó)
docker rmi lab-single lab-multi lab-scratch lab-copy-from \
    lab-target-dev lab-target-prod 2>/dev/null
```

---

## Conceptos reforzados

1. **Multi-stage builds** separan el build del runtime. Solo las capas del
   stage final forman la imagen. Las herramientas de compilación se descartan.

2. **FROM scratch** crea imágenes que solo contienen el binario. Requiere
   compilación estática (`-static`). Reduce tamaños de ~350MB a ~1MB.

3. **COPY --from** copia archivos entre stages (por nombre o índice) y
   también desde imágenes externas (`COPY --from=nginx:latest`).

4. **--target** permite construir un stage específico, creando variantes
   (dev/prod) desde un solo Dockerfile. BuildKit salta stages que no son
   dependencias del target.

5. Los stages intermedios no aparecen como imágenes en `docker images`.
   Solo el stage final (o el target) produce la imagen resultante.
