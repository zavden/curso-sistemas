# Lab — Instrucciones básicas

## Objetivo

Laboratorio práctico para construir imágenes Docker usando las instrucciones
fundamentales del Dockerfile: FROM, RUN, COPY, ADD, WORKDIR, CMD, EXPOSE
y LABEL. Se comparan variantes de imagen base, formas de RUN, y se demuestra
el impacto de combinar instrucciones en el tamaño final.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imágenes base descargadas:
  - `docker pull debian:bookworm`
  - `docker pull debian:bookworm-slim`
  - `docker pull alpine:3.19`
  - `docker pull alpine:latest`
- Estar en el directorio `labs/` de este tópico

## Archivos del laboratorio

| Archivo | Propósito |
|---|---|
| `hello.c` | Programa C simple para compilar dentro de las imágenes |
| `config.txt` | Archivo de configuración para demos de COPY |
| `Dockerfile.from-full` | Imagen con debian:bookworm (Parte 1) |
| `Dockerfile.from-slim` | Imagen con debian:bookworm-slim (Parte 1) |
| `Dockerfile.from-alpine` | Imagen con alpine:3.19 (Parte 1) |
| `Dockerfile.run-forms` | Forma shell vs exec de RUN (Parte 2) |
| `Dockerfile.copy-workdir` | COPY y WORKDIR en acción (Parte 3) |
| `Dockerfile.copy-vs-add` | COPY vs ADD con tarball (Parte 3) |
| `Dockerfile.run-separate` | Patrón incorrecto: RUN separados (Parte 4) |
| `Dockerfile.run-combined` | Patrón correcto: RUN combinado (Parte 4) |
| `Dockerfile.metadata` | CMD, EXPOSE, LABEL (Parte 5) |

---

## Parte 1 — FROM: la imagen base

### Objetivo

Construir el mismo programa C con tres imágenes base diferentes, comparar los
tamaños resultantes, y entender cuándo usar cada variante.

### Paso 1.1: Examinar los Dockerfiles

```bash
echo "=== Dockerfile.from-full ==="
cat Dockerfile.from-full

echo ""
echo "=== Dockerfile.from-slim ==="
cat Dockerfile.from-slim

echo ""
echo "=== Dockerfile.from-alpine ==="
cat Dockerfile.from-alpine
```

Los tres Dockerfiles hacen lo mismo: instalar gcc, copiar `hello.c`, compilar,
y definir CMD. La diferencia está en FROM y en el gestor de paquetes
(`apt-get` vs `apk`).

Antes de construir, responde mentalmente:

- ¿Cuál imagen será la más grande? ¿Y la más pequeña?
- ¿Qué tamaño aproximado esperas para cada una?
- ¿El binario compilado será idéntico en las tres?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.2: Construir las tres imágenes

```bash
docker build -f Dockerfile.from-full   -t lab-from-full   .
docker build -f Dockerfile.from-slim   -t lab-from-slim   .
docker build -f Dockerfile.from-alpine -t lab-from-alpine .
```

### Paso 1.3: Comparar tamaños

```bash
docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}" | grep lab-from
```

Salida esperada (aproximada):

```
REPOSITORY        TAG       SIZE
lab-from-alpine   latest    ~170MB
lab-from-slim     latest    ~245MB
lab-from-full     latest    ~290MB
```

Los tamaños varían, pero el patrón es consistente:

- `debian:bookworm` (~120MB base) + gcc → la más grande
- `debian:bookworm-slim` (~75MB base) + gcc → intermedia
- `alpine:3.19` (~7MB base) + gcc/musl-dev → la más pequeña

### Paso 1.4: Verificar que las tres funcionan

```bash
docker run --rm lab-from-full
docker run --rm lab-from-slim
docker run --rm lab-from-alpine
```

Las tres imágenes producen la misma salida:

```
Hello from a Docker-built C program!
This binary was compiled inside a Dockerfile.
```

Mismo resultado, diferentes tamaños. La imagen base no afecta la funcionalidad
del programa, pero sí el tamaño de la imagen, las herramientas disponibles
dentro del contenedor, y la superficie de ataque para seguridad.

### Paso 1.5: Comparar capas

```bash
echo "=== lab-from-full ==="
docker history lab-from-full --format "table {{.CreatedBy}}\t{{.Size}}" | head -8

echo ""
echo "=== lab-from-alpine ==="
docker history lab-from-alpine --format "table {{.CreatedBy}}\t{{.Size}}" | head -8
```

Observa que las capas de instalación de gcc tienen tamaños muy diferentes
entre Debian (`apt-get`) y Alpine (`apk`), porque Alpine instala menos
dependencias por defecto.

### Paso 1.6: Examinar hello.c

```bash
cat hello.c
```

Un programa C trivial. El punto del ejercicio no es el programa — es
demostrar que el mismo código fuente produce imágenes de tamaños muy
diferentes según la base elegida.

### Paso 1.7: Limpiar

```bash
docker rmi lab-from-full lab-from-slim lab-from-alpine
```

---

## Parte 2 — RUN: forma shell vs forma exec

### Objetivo

Demostrar la diferencia entre la forma shell (`RUN command`) y la forma exec
(`RUN ["command", "args"]`) de la instrucción RUN, especialmente en relación
a la expansión de variables de entorno.

### Paso 2.1: Examinar el Dockerfile

```bash
cat Dockerfile.run-forms
```

Antes de construir, responde mentalmente:

- ¿Qué salida producirá la forma shell durante el build?
- ¿Qué salida producirá la forma exec durante el build?
- ¿En cuál se expandirá `$APP_VERSION`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.2: Construir y observar la salida

```bash
docker build -f Dockerfile.run-forms -t lab-run-forms .
```

Observa cuidadosamente la salida del build. En los pasos de RUN verás:

```
Shell form: APP_VERSION=2.0
```

y:

```
Exec form: APP_VERSION=$APP_VERSION
```

### Paso 2.3: Analizar

| Forma | Sintaxis | Shell | Expansión de variables |
|---|---|---|---|
| Shell | `RUN command` | `/bin/sh -c "command"` | Sí — `$VAR` se expande |
| Exec | `RUN ["cmd", "arg"]` | Ninguno (ejecución directa) | No — `$VAR` es literal |

La forma shell envuelve el comando en `/bin/sh -c "..."`, lo que activa todas
las features del shell: expansión de variables, pipes, redirecciones, globs.

La forma exec ejecuta el binario directamente — sin shell intermedio. Las
variables no se expanden porque no hay shell que las procese.

**Cuándo usar cada una**:

- **Shell**: la mayoría de los casos — necesitas variables, pipes, `&&`
- **Exec**: cuando el shell interfiere, cuando necesitas control exacto del
  proceso, o en imágenes sin shell (`FROM scratch`)

### Paso 2.4: Limpiar

```bash
docker rmi lab-run-forms
```

---

## Parte 3 — COPY, ADD y WORKDIR

### Objetivo

Demostrar cómo COPY coloca archivos en la imagen, cómo WORKDIR establece el
directorio de trabajo para instrucciones subsiguientes, y la diferencia entre
COPY y ADD para archivos tar.

### Paso 3.1: Examinar el Dockerfile

```bash
cat Dockerfile.copy-workdir
```

Antes de construir, responde mentalmente:

- ¿Qué directorio creará `WORKDIR /app`?
- ¿Dónde quedará `hello.c` después de `COPY hello.c .`?
- ¿Cuál será el directorio final después de `WORKDIR src`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.2: Construir y ejecutar

```bash
docker build -f Dockerfile.copy-workdir -t lab-copy-workdir .
docker run --rm lab-copy-workdir
```

Salida esperada:

```
=== /app contents ===
config
final-workdir.txt
hello.c
src
=== Final WORKDIR ===
/app/src
```

### Paso 3.3: Analizar

- `WORKDIR /app` creó el directorio `/app` y lo estableció como directorio actual
- `COPY hello.c .` copió el archivo a `/app/hello.c` (relativo a WORKDIR)
- `COPY config.txt ./config/` copió a `/app/config/config.txt`
- `WORKDIR src` (path relativo) se resolvió como `/app/src`
- El directorio final es `/app/src`

**Regla**: WORKDIR es acumulativo. Las paths relativas se resuelven desde el
WORKDIR actual. Siempre usar WORKDIR en lugar de `RUN cd` — cada RUN se
ejecuta en un shell nuevo, y `cd` no persiste entre instrucciones.

### Paso 3.4: COPY vs ADD con un tarball

Crear un tarball para probar la diferencia:

```bash
tar czf data.tar.gz config.txt hello.c
```

Examinar el Dockerfile:

```bash
cat Dockerfile.copy-vs-add
```

Antes de construir, responde mentalmente:

- ¿Qué contendrá `/app/via-copy/`?
- ¿Qué contendrá `/app/via-add/`?

Intenta predecir antes de continuar al siguiente paso.

```bash
docker build -f Dockerfile.copy-vs-add -t lab-copy-vs-add .
docker run --rm lab-copy-vs-add
```

Salida esperada (parcial):

```
=== COPY result (tarball intact) ===
-rw-r--r--    ...    data.tar.gz

=== ADD result (auto-extracted) ===
-rw-r--r--    ...    config.txt
-rw-r--r--    ...    hello.c
```

COPY mantiene el archivo tal cual. ADD detecta que es un tarball y lo extrae
automáticamente en el directorio destino.

**Regla general**: usar siempre COPY excepto cuando se necesite la
auto-extracción de tar. Para descargas de URLs, usar `RUN curl` o `RUN wget`
que dan más control sobre caché y manejo de errores.

### Paso 3.5: Limpiar

```bash
docker rmi lab-copy-workdir lab-copy-vs-add
rm -f data.tar.gz
```

---

## Parte 4 — Combinar RUN para eficiencia de capas

### Objetivo

Demostrar que separar la instalación y la limpieza en instrucciones RUN
diferentes desperdicia espacio, porque cada RUN crea una capa inmutable y
los archivos eliminados en una capa posterior siguen existiendo en la anterior.

### Paso 4.1: Examinar los Dockerfiles

```bash
echo "=== Patrón incorrecto ==="
cat Dockerfile.run-separate

echo ""
echo "=== Patrón correcto ==="
cat Dockerfile.run-combined
```

Ambos hacen lo mismo: instalar curl y eliminar el cache de apt. La diferencia
es si lo hacen en uno o dos RUN.

Antes de construir, predice: ¿cuál será más grande? ¿Por cuánto?

### Paso 4.2: Construir y comparar tamaños

```bash
docker build -f Dockerfile.run-separate -t lab-run-separate .
docker build -f Dockerfile.run-combined -t lab-run-combined .

docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}" | grep lab-run-
```

Salida esperada (aproximada):

```
REPOSITORY         TAG       SIZE
lab-run-combined   latest    ~100MB
lab-run-separate   latest    ~120MB
```

La diferencia (~20MB) es el cache de `apt-get` que existe en la capa 1 de
`lab-run-separate` pero fue eliminado dentro de la misma capa en
`lab-run-combined`.

### Paso 4.3: Inspeccionar las capas

```bash
echo "=== lab-run-separate ==="
docker history lab-run-separate --format "table {{.CreatedBy}}\t{{.Size}}"

echo ""
echo "=== lab-run-combined ==="
docker history lab-run-combined --format "table {{.CreatedBy}}\t{{.Size}}"
```

En `lab-run-separate`:

- Capa del `apt-get install`: contiene curl + cache (tamaño significativo)
- Capa del `rm -rf`: ~0B (crea un whiteout, pero la capa anterior sigue intacta)

En `lab-run-combined`:

- Una sola capa: contiene solo el resultado neto (curl instalado, sin cache)

### Paso 4.4: Verificar que ambas funcionan

```bash
docker run --rm lab-run-separate
docker run --rm lab-run-combined
```

Ambas muestran la versión de curl — funcionalmente idénticas, pero
`lab-run-combined` es más eficiente en espacio.

### Paso 4.5: Limpiar

```bash
docker rmi lab-run-separate lab-run-combined
```

---

## Parte 5 — CMD, EXPOSE, LABEL y metadata

### Objetivo

Demostrar que las instrucciones de metadata (LABEL, EXPOSE, CMD) crean capas
de 0 bytes, que CMD define un comando por defecto que se puede sobrescribir,
y cómo inspeccionar la metadata de una imagen.

### Paso 5.1: Examinar el Dockerfile

```bash
cat Dockerfile.metadata
```

Antes de construir, responde mentalmente:

- ¿Cuántas capas con contenido (>0 bytes) se crearán?
- ¿Cuántas capas de solo metadata (0 bytes) se crearán?
- ¿EXPOSE 8080 abrirá el puerto automáticamente?

Intenta predecir antes de continuar al siguiente paso.

### Paso 5.2: Construir la imagen

```bash
docker build -f Dockerfile.metadata -t lab-metadata .
```

### Paso 5.3: Verificar las capas

```bash
docker history lab-metadata --format "table {{.CreatedBy}}\t{{.Size}}"
```

Salida esperada (aproximada):

```
CREATED BY                                      SIZE
CMD ["cat" "/app/greeting.txt"]                 0B
EXPOSE map[443/tcp:{} 8080/tcp:{}]              0B
LABEL description=Lab - metadata instructio...  0B
LABEL version=1.0                               0B
LABEL maintainer=student@lab.local              0B
WORKDIR /app                                    0B
RUN /bin/sh -c echo "Hello from the metada...   ~30B
/bin/sh -c #(nop)  CMD ["/bin/sh"]              0B
/bin/sh -c #(nop) ADD file:... in /             ~7MB
```

Solo `RUN echo` y `ADD` (de la base alpine) crean capas con contenido.
LABEL, EXPOSE, CMD y WORKDIR son todas de 0 bytes — solo metadata.

### Paso 5.4: Ejecutar con CMD por defecto

```bash
docker run --rm lab-metadata
```

Salida esperada:

```
Hello from the metadata lab
```

Este es el CMD definido en el Dockerfile: `cat /app/greeting.txt`.

### Paso 5.5: Sobrescribir CMD

```bash
docker run --rm lab-metadata echo "CMD sobrescrito"
```

Salida esperada:

```
CMD sobrescrito
```

El argumento `echo "CMD sobrescrito"` reemplaza completamente el CMD del
Dockerfile. El `cat /app/greeting.txt` original no se ejecuta.

```bash
docker run --rm lab-metadata ls /app/
```

Salida esperada:

```
greeting.txt
```

Cualquier comando pasado a `docker run` sobrescribe el CMD.

### Paso 5.6: Inspeccionar metadata

```bash
# Ver los labels
docker inspect lab-metadata --format '{{json .Config.Labels}}' | python3 -m json.tool
```

Salida esperada:

```json
{
    "description": "Lab - metadata instructions demo",
    "maintainer": "student@lab.local",
    "version": "1.0"
}
```

```bash
# Ver los puertos expuestos
docker inspect lab-metadata --format '{{json .Config.ExposedPorts}}' | python3 -m json.tool
```

Salida esperada:

```json
{
    "443/tcp": {},
    "8080/tcp": {}
}
```

EXPOSE no abre puertos — solo documenta qué puertos usa la aplicación. Para
publicar puertos hay que usar `docker run -p 8080:8080`.

```bash
# Ver el CMD
docker inspect lab-metadata --format '{{json .Config.Cmd}}'
```

Salida esperada:

```json
["cat","/app/greeting.txt"]
```

### Paso 5.7: Limpiar

```bash
docker rmi lab-metadata
```

---

## Limpieza final

```bash
# Eliminar imágenes del lab (si alguna quedó)
docker rmi lab-from-full lab-from-slim lab-from-alpine \
    lab-run-forms lab-copy-workdir lab-copy-vs-add \
    lab-run-separate lab-run-combined lab-metadata 2>/dev/null

# Eliminar el tarball generado en Parte 3
rm -f data.tar.gz
```

---

## Conceptos reforzados

1. **FROM** define la imagen base. La variante elegida determina el tamaño
   final, las herramientas disponibles y la superficie de ataque. Usar la
   más mínima que funcione: `debian:bookworm-slim` para necesidades generales,
   `alpine` para imágenes pequeñas.

2. **RUN** en forma shell (`RUN command`) ejecuta a través de `/bin/sh -c` y
   permite expansión de variables, pipes y redirecciones. La forma exec
   (`RUN ["cmd", "arg"]`) ejecuta directamente sin shell — las variables
   no se expanden.

3. **COPY** toma archivos del contexto de build y los coloca en la imagen.
   **ADD** es igual pero auto-extrae archivos tar. Preferir COPY siempre
   excepto cuando se necesite la auto-extracción.

4. **WORKDIR** crea el directorio y establece el directorio de trabajo para
   las instrucciones siguientes. Es acumulativo — las paths relativas se
   resuelven desde el WORKDIR actual. Nunca usar `RUN cd`.

5. Combinar instalación y limpieza en el mismo **RUN** evita que archivos
   temporales (cache de apt, compiladores) queden en capas permanentes.
   Todo lo temporal debe crearse y eliminarse en la misma instrucción RUN.

6. **CMD**, **EXPOSE**, **LABEL** y otras instrucciones de metadata crean
   capas de 0 bytes. CMD define un comando por defecto que se puede
   sobrescribir con argumentos a `docker run`. EXPOSE solo documenta puertos,
   no los abre.
