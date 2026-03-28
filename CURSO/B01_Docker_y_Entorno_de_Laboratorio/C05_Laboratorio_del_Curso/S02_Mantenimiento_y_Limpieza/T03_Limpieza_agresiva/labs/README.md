# Lab — Limpieza agresiva

## Objetivo

Entender `docker system prune` como herramienta de limpieza integral:
que elimina cada variante, como documentar el impacto, y cuando es
seguro o peligroso usarlo.

## Prerequisitos

- Docker instalado y funcionando
- Estar en el directorio `labs/` de este topico

## Archivos del laboratorio

| Archivo | Proposito |
|---|---|
| `Dockerfile.rebuild` | Dockerfile para medir tiempos de rebuild |

---

## Parte 1 — docker system prune

### Objetivo

Entender que elimina `docker system prune` por defecto y que conserva.

### Paso 1.1: Crear recursos para limpiar

```bash
docker build -t lab-aggressive -f Dockerfile.rebuild .
docker run --name lab-aggressive-exit alpine echo "will be pruned"
docker network create lab-aggressive-net
```

Ahora hay: una imagen custom, un contenedor detenido, y una red no
usada.

### Paso 1.2: Espacio antes

```bash
echo "=== ANTES ==="
docker system df
```

Anota los valores de SIZE y RECLAIMABLE para cada tipo.

### Paso 1.3: Ver que se eliminaria

```bash
docker system prune --dry-run 2>/dev/null || echo "(--dry-run no disponible en todas las versiones)"
```

Si `--dry-run` no esta disponible, revisa mentalmente:

```
docker system prune elimina:
  - Contenedores detenidos       → lab-aggressive-exit
  - Redes no usadas              → lab-aggressive-net
  - Imagenes dangling            → (ninguna en este caso)
  - Build cache dangling         → (si existe)

docker system prune NO elimina:
  - Imagenes con tag no usadas   → lab-aggressive (tiene tag)
  - Volumenes                    → (nunca sin --volumes)
  - Contenedores corriendo       → (ninguno afectado)
```

### Paso 1.4: Ejecutar prune

```bash
docker system prune -f
```

`-f` omite la confirmacion.

### Paso 1.5: Espacio despues

```bash
echo "=== DESPUES ==="
docker system df
```

### Paso 1.6: Verificar que sobrevivio

```bash
docker image ls lab-aggressive
docker ps -a --filter "name=lab-aggressive"
docker network ls | grep lab-aggressive
```

La imagen `lab-aggressive` sobrevive (tiene tag y no es dangling).
El contenedor y la red fueron eliminados.

---

## Parte 2 — Variantes -a y --volumes

### Objetivo

Entender el impacto de los flags `-a` y `--volumes`.

### Paso 2.1: -a incluye imagenes no usadas

Verificar que lab-aggressive sigue existiendo:

```bash
docker image ls lab-aggressive
```

Antes de ejecutar el siguiente comando, predice: se eliminara
`lab-aggressive`?

```bash
docker system prune -a -f
```

`-a` elimina **todas las imagenes** sin un contenedor (activo o
detenido) que las referencie. Como lab-aggressive no tiene ningun
contenedor, se elimina.

```bash
docker image ls lab-aggressive
```

### Paso 2.2: Reconstruir para el siguiente paso

```bash
docker build -t lab-aggressive -f Dockerfile.rebuild .
```

### Paso 2.3: --volumes incluye volumenes

```bash
docker volume create lab-aggressive-data
docker run --rm -v lab-aggressive-data:/data alpine sh -c 'echo "datos importantes" > /data/test.txt'
```

Verificar el contenido:

```bash
docker run --rm -v lab-aggressive-data:/data alpine cat /data/test.txt
```

```bash
docker system prune --volumes -f
```

Predice: se eliminaron los datos del volumen?

```bash
docker volume ls | grep lab-aggressive
```

El volumen fue eliminado. Los datos se perdieron permanentemente.
Este flag no se activa por defecto precisamente por este riesgo.

### Paso 2.4: La combinacion nuclear

No ejecutes este comando a menos que realmente quieras limpiar todo:

```bash
# docker system prune -a --volumes -f
# Elimina TODO: contenedores detenidos, imagenes no usadas,
# volumenes huerfanos, build cache, redes no usadas
```

Lo unico que sobrevive: contenedores corriendo, sus imagenes, sus
volumenes montados, y sus redes.

---

## Parte 3 — Reconstruir despues de prune

### Objetivo

Medir el costo real de una limpieza agresiva: cuanto tarda
reconstruir.

### Paso 3.1: Build con cache

```bash
docker build -t lab-rebuild -f Dockerfile.rebuild .
echo ""
echo "=== Rebuild con cache ==="
time docker build -t lab-rebuild -f Dockerfile.rebuild . 2>&1 | tail -5
```

### Paso 3.2: Prune agresivo

```bash
docker system prune -a -f
```

### Paso 3.3: Build sin cache

```bash
echo "=== Rebuild sin cache ==="
time docker build -t lab-rebuild -f Dockerfile.rebuild . 2>&1 | tail -5
```

Compara los tiempos. Con una imagen pequena la diferencia es menor,
pero con las imagenes del curso (Debian dev, AlmaLinux dev) la
diferencia es de **segundos vs minutos**.

### Paso 3.4: Documentar impacto

```bash
echo "=== Estado final ==="
docker system df
docker image ls
```

Despues de un prune agresivo, Docker queda practicamente vacio. Todo
lo que se necesita debe reconstruirse desde Dockerfiles o re-descargarse.

---

## Limpieza final

```bash
docker rmi lab-aggressive lab-rebuild 2>/dev/null
docker volume rm lab-aggressive-data 2>/dev/null
docker network rm lab-aggressive-net 2>/dev/null
```

---

## Conceptos reforzados

1. `docker system prune` (sin flags) elimina contenedores detenidos,
   redes no usadas, imagenes dangling, y build cache dangling. **No
   toca** imagenes con tag, volumenes, ni contenedores corriendo.

2. `-a` agrega **imagenes no usadas** (con tag pero sin contenedor
   asociado). El proximo build reconstruye todo desde cero.

3. `--volumes` agrega **volumenes huerfanos**. Los datos se pierden
   **permanentemente**. Este flag nunca se activa por defecto.

4. La combinacion `-a --volumes` elimina todo excepto recursos
   activos. Es segura en CI/CD pero peligrosa en desarrollo si hay
   datos en volumenes.

5. El **costo de reconstruir** despues de prune agresivo es real:
   imagenes del curso pueden tardar minutos en reconstruirse sin
   cache. `--keep-storage` en builder prune es un buen compromiso.
