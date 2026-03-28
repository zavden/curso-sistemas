# Lab — Orden de instrucciones y caché

## Objetivo

Laboratorio práctico para entender cómo funciona la caché de capas de Docker:
cuándo se reutilizan las capas, cómo un cambio en una capa invalida todas las
posteriores (invalidación en cascada), y por qué el orden de las instrucciones
determina la eficiencia del build.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada: `docker pull alpine:latest`
- Estar en el directorio `labs/` de este tópico

## Archivos del laboratorio

| Archivo | Propósito |
|---|---|
| `app.sh` | Script que simula código fuente (cambia frecuentemente) |
| `deps.txt` | Manifest de dependencias (cambia raramente) |
| `Dockerfile.good-order` | Orden correcto: dependencias primero, código después |
| `Dockerfile.bad-order` | Orden incorrecto: código primero, dependencias después |
| `Dockerfile.cascade` | Para demostrar invalidación en cascada |

---

## Parte 1 — Caché en acción

### Objetivo

Demostrar que Docker reutiliza capas de builds anteriores cuando las
instrucciones y sus inputs no han cambiado.

### Paso 1.1: Primer build (sin caché)

```bash
docker build -f Dockerfile.cascade -t lab-cascade .
```

Observa la salida: cada paso se ejecuta y produce output. Ningún paso
muestra `CACHED`.

### Paso 1.2: Segundo build (con caché)

```bash
docker build -f Dockerfile.cascade -t lab-cascade .
```

Observa ahora: todos los pasos muestran `CACHED`. Docker reutilizó las capas
del build anterior porque nada cambió.

### Paso 1.3: Verificar qué se cachea

```bash
docker build -f Dockerfile.cascade -t lab-cascade . 2>&1 | grep -E 'CACHED|RUN|COPY'
```

Todos los pasos deberían mostrar `CACHED`.

### Paso 1.4: Analizar

La caché funciona porque Docker compara:

- **RUN**: el string del comando (si el texto no cambió, usa caché)
- **COPY/ADD**: el checksum del contenido de los archivos (no el timestamp)
- **FROM**: el digest de la imagen base

Si nada cambió, Docker no re-ejecuta la instrucción — usa el resultado
almacenado del build anterior.

---

## Parte 2 — Invalidación en cascada

### Objetivo

Demostrar que cuando una capa se invalida, todas las capas posteriores se
reconstruyen aunque no hayan cambiado.

### Paso 2.1: Estado actual

```bash
docker build -f Dockerfile.cascade -t lab-cascade . 2>&1 | grep -E 'CACHED|RUN|COPY'
```

Todo `CACHED` — el build anterior está en caché.

### Paso 2.2: Modificar el archivo copiado

```bash
echo '#!/bin/sh' > app.sh
echo 'echo "App version 2 (modified)"' >> app.sh
```

Antes de reconstruir, responde mentalmente:

- ¿Qué capas se invalidarán?
- Layer 1 (`RUN echo "Layer 1"`) — ¿CACHED o reconstruida?
- Layer 2 (`RUN echo "Layer 2"`) — ¿CACHED o reconstruida?
- COPY app.sh — ¿CACHED o reconstruida?
- Layer 3 (`RUN echo "Layer 3"`) — ¿CACHED o reconstruida?
- Layer 4 (`RUN echo "Layer 4"`) — ¿CACHED o reconstruida?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.3: Reconstruir

```bash
docker build -f Dockerfile.cascade -t lab-cascade . 2>&1 | grep -E 'CACHED|RUN|COPY'
```

Resultado:

- Layer 1: `CACHED` — no depende de app.sh
- Layer 2: `CACHED` — no depende de app.sh
- COPY: **reconstruida** — el contenido de app.sh cambió
- Layer 3: **reconstruida** — posterior a una capa invalidada
- Layer 4: **reconstruida** — posterior a una capa invalidada

Layer 3 y Layer 4 se reconstruyeron aunque sus comandos `echo` no cambiaron.
Esto es la **invalidación en cascada**: Docker no puede saber si las capas
posteriores dependían del resultado de la capa invalidada, así que las
reconstruye todas.

### Paso 2.4: Restaurar app.sh

```bash
printf '#!/bin/sh\necho "App version 1"\necho "Running from $(hostname)"\n' > app.sh
```

### Paso 2.5: Limpiar

```bash
docker rmi lab-cascade
```

---

## Parte 3 — Orden correcto vs incorrecto

### Objetivo

Comparar dos Dockerfiles que hacen lo mismo pero con diferente orden de
instrucciones, y demostrar que el orden determina cuántas capas se reconstruyen
cuando cambia el código fuente.

### Paso 3.1: Examinar ambos Dockerfiles

```bash
echo "=== Orden CORRECTO (deps primero, código después) ==="
cat Dockerfile.good-order

echo ""
echo "=== Orden INCORRECTO (código primero, deps después) ==="
cat Dockerfile.bad-order
```

### Paso 3.2: Build inicial de ambos

```bash
docker build -f Dockerfile.good-order -t lab-good-order .
docker build -f Dockerfile.bad-order  -t lab-bad-order  .
```

### Paso 3.3: Verificar que ambos están en caché

```bash
echo "=== Good order ==="
docker build -f Dockerfile.good-order -t lab-good-order . 2>&1 | grep -cE 'CACHED'
echo "capas cacheadas"

echo "=== Bad order ==="
docker build -f Dockerfile.bad-order -t lab-bad-order . 2>&1 | grep -cE 'CACHED'
echo "capas cacheadas"
```

### Paso 3.4: Modificar SOLO el código fuente

```bash
echo '#!/bin/sh' > app.sh
echo 'echo "App version 2"' >> app.sh
echo 'echo "Running from $(hostname)"' >> app.sh
```

Antes de reconstruir, predice:

- **Orden correcto**: ¿cuántas capas se reconstruyen?
- **Orden incorrecto**: ¿cuántas capas se reconstruyen?

### Paso 3.5: Reconstruir ambos y comparar

```bash
echo "=== Good order (rebuild) ==="
docker build -f Dockerfile.good-order -t lab-good-order . 2>&1 | grep -E 'CACHED|RUN|COPY'
```

En el orden correcto:

- `RUN apk add` → CACHED (dependencias del sistema, no cambiaron)
- `COPY deps.txt` → CACHED (el manifest no cambió)
- `RUN echo "Dependencies"` → CACHED
- `COPY app.sh` → **reconstruida** (app.sh cambió)
- `RUN chmod` → reconstruida (cascada)

Solo 2 capas se reconstruyen.

```bash
echo "=== Bad order (rebuild) ==="
docker build -f Dockerfile.bad-order -t lab-bad-order . 2>&1 | grep -E 'CACHED|RUN|COPY'
```

En el orden incorrecto:

- `COPY . .` → **reconstruida** (app.sh cambió → todo el directorio cambia)
- `RUN chmod` → reconstruida (cascada)
- `RUN apk add` → **reconstruida** (cascada, aunque no cambió nada)
- `RUN echo "Dependencies"` → **reconstruida** (cascada)

Todas las capas después de COPY se reconstruyen, incluyendo `apk add` que
es la más lenta.

### Paso 3.6: Restaurar y limpiar

```bash
printf '#!/bin/sh\necho "App version 1"\necho "Running from $(hostname)"\n' > app.sh
docker rmi lab-good-order lab-bad-order
```

---

## Parte 4 — --no-cache: rebuild completo

### Objetivo

Demostrar `--no-cache` para forzar un rebuild completo sin usar caché.

### Paso 4.1: Build normal (usa caché)

```bash
docker build -f Dockerfile.good-order -t lab-good-order .
docker build -f Dockerfile.good-order -t lab-good-order . 2>&1 | grep -cE 'CACHED'
echo "capas cacheadas"
```

### Paso 4.2: Build con --no-cache

```bash
docker build --no-cache -f Dockerfile.good-order -t lab-good-order . 2>&1 | grep -cE 'CACHED'
echo "capas cacheadas (debería ser 0)"
```

Salida esperada: 0 capas cacheadas. Todas se reconstruyen.

### Paso 4.3: Cuándo usar --no-cache

`--no-cache` es útil cuando:

- Quieres incorporar actualizaciones de seguridad de paquetes
- Sospechas que la caché tiene datos obsoletos (ej: `apt-get update` cacheado
  con listas de paquetes antiguas)
- Necesitas un build limpio y reproducible para producción

No usarlo en desarrollo diario — la caché es tu aliada para builds rápidos.

### Paso 4.4: Limpiar

```bash
docker rmi lab-good-order
```

---

## Limpieza final

```bash
# Eliminar imágenes del lab (si alguna quedó)
docker rmi lab-cascade lab-good-order lab-bad-order 2>/dev/null

# Restaurar app.sh al estado original
printf '#!/bin/sh\necho "App version 1"\necho "Running from $(hostname)"\n' > app.sh
```

---

## Conceptos reforzados

1. Docker **cachea** cada capa y la reutiliza si la instrucción y sus inputs
   no cambiaron. Esto hace que builds consecutivos sean mucho más rápidos.

2. La **invalidación en cascada** reconstruye todas las capas posteriores a
   una capa invalidada, aunque esas capas no hayan cambiado.

3. El **orden de instrucciones** determina la eficiencia de la caché. Regla:
   ordenar de menos frecuente a más frecuente en cambios — dependencias
   primero, código fuente después.

4. **COPY** se invalida por cambio de checksum del contenido (no timestamp).
   **RUN** se invalida por cambio en el string del comando.

5. `--no-cache` fuerza un rebuild completo. Útil para actualizaciones de
   seguridad, no para desarrollo diario.
