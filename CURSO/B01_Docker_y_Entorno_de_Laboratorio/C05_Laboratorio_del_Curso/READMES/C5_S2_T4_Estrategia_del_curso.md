# T04 — Estrategia del curso

## Objetivo

Definir una rutina de mantenimiento práctica para el laboratorio del curso:
cuándo limpiar, qué conservar, cómo reutilizar contenedores, y cuánto espacio
mantener disponible.

---

## Errores corregidos respecto al material original

1. **`container prune` SÍ elimina contenedores parados con `docker compose stop`**
   — El original afirma que los contenedores detenidos con `stop` quedan
   "referenciados por Compose" y están protegidos de `container prune`. Esto es
   **falso**. `docker container prune` elimina TODOS los contenedores en estado
   `exited`, sin importar si fueron creados por Compose. La única protección es
   que estén **corriendo** (`Up`). Si los detuviste con `stop`, `container prune`
   los elimina y necesitarás `docker compose up -d` para recrearlos.

2. **`docker compose run` usa la red existente de Compose** — La tabla original
   dice que `run` crea una red nueva ("Nueva (crea red)"). Esto es incorrecto.
   `run` conecta el contenedor nuevo a la red `<proyecto>_default` ya existente,
   la misma que usan los demás servicios.

3. **`cat >> ~/.bashrc` añade duplicados** — El ejercicio original usa
   `cat >> ~/.bashrc` sin verificar si los alias ya existen. Ejecutarlo
   múltiples veces duplica las entradas. Corregido con verificación previa.

4. **`~/bin/` puede no existir** — El script de mantenimiento original se
   escribe en `~/bin/docker-maintenance.sh` asumiendo que el directorio existe
   y está en `$PATH`. Corregido con `mkdir -p` y nota sobre `$PATH`.

5. **Script de mantenimiento: `stop` → `prune` → `start` falla** — El script
   original detiene el lab con `stop`, ejecuta `container prune` (que elimina
   los contenedores parados), y luego intenta `start` (que falla porque ya no
   existen). Corregido usando `up -d` en lugar de `start` después del prune.

6. **Nombre de volumen hardcoded** — El ejercicio de recuperación usa
   `docker volume rm lab_workspace` asumiendo que el proyecto se llama `lab`.
   El nombre real depende del directorio del proyecto. Corregido con detección
   dinámica.

---

## Rutina de limpieza recomendada

### Después de cada sesión de trabajo

```bash
docker container prune -f
```

Limpia contenedores efímeros creados con `docker run` durante ejercicios.

> **Advertencia**: `container prune` elimina TODOS los contenedores en estado
> `exited`, incluyendo los del lab si fueron detenidos con `docker compose stop`.
> Ejecuta este comando solo si el lab está **corriendo** (`docker compose ps`
> muestra `Up`) o si usaste `docker compose down` (que ya los eliminó).

### Semanalmente

```bash
docker image prune -f
```

Limpia imágenes dangling (`<none>:<none>`) generadas por rebuilds. Cada vez que
modificas un Dockerfile y ejecutas `docker compose build`, la versión anterior
queda como dangling.

### Mensualmente

```bash
docker builder prune --keep-storage 2GB -f
```

Limpia build cache viejo manteniendo al menos 2 GB de las capas más recientes.
Los rebuilds del lab siguen siendo rápidos sin acumular cache de builds antiguos.

### Volúmenes: nunca sin verificar

```bash
# SIEMPRE listar antes
docker volume ls

# Solo entonces decidir si limpiar
docker volume prune
```

`docker volume prune` elimina volúmenes no referenciados por ningún contenedor.
Si los contenedores del lab están detenidos (estado `exited`), sus volúmenes
**sí están protegidos** — un contenedor detenido sigue referenciando sus
volúmenes. Solo están en riesgo si eliminaste los contenedores (con `down` o
`container prune`) sin eliminar los volúmenes.

### Tabla de frecuencia

| Frecuencia | Comando | Qué limpia | Precaución |
|---|---|---|---|
| Después de cada sesión | `container prune -f` | Contenedores `exited` | Lab debe estar corriendo |
| Semanalmente | `image prune -f` | Imágenes dangling | Seguro si imágenes del lab tienen tag |
| Mensualmente | `builder prune --keep-storage 2GB -f` | Build cache viejo | Seguro siempre |
| Solo con verificación | `volume prune` | Volúmenes huérfanos | Verificar `volume ls` antes |

### Diagrama de protección

```
Estado del lab              container prune    volume prune
─────────────────────────   ────────────────   ──────────────
Contenedores corriendo      ✅ Protegidos      ✅ Protegidos
Contenedores parados        ❌ ELIMINADOS      ✅ Protegidos
Contenedores eliminados     n/a                ❌ ELIMINADOS
```

La asimetría es clave: `container prune` es peligroso con contenedores parados,
pero `volume prune` es seguro mientras los contenedores existan (corriendo O
parados). El riesgo para volúmenes aparece solo cuando los contenedores fueron
eliminados (con `down`, `rm`, o `container prune`).

---

## Reutilización de contenedores: exec vs run

### Preferir `exec` sobre `run`

```bash
# BIEN: reutiliza el contenedor existente
docker compose exec debian-dev gcc -o hello hello.c

# EVITAR: crea un contenedor nuevo cada vez
docker compose run debian-dev gcc -o hello hello.c
```

`exec` ejecuta un comando dentro de un contenedor ya corriendo — no crea nada
nuevo. `run` crea un contenedor nuevo cada vez, que queda en estado `exited`
después de terminar.

### Consecuencias acumuladas

```
Después de 20 ejercicios con "run":
$ docker ps -a | wc -l
22    ← 20 contenedores detenidos + 2 activos del lab

Después de 20 ejercicios con "exec":
$ docker ps -a | wc -l
3     ← solo los 2 contenedores del lab + header de la tabla
```

### Tabla comparativa

| Aspecto | `docker compose exec` | `docker compose run` |
|---|---|---|
| Contenedor | Usa el existente | Crea uno nuevo |
| Estado después | Sin cambio | `exited` (nuevo contenedor) |
| Volúmenes | Los del servicio | Los del servicio (compartidos) |
| Red | La existente del proyecto | La existente del proyecto |
| Efecto en `docker ps -a` | Ninguno | Nuevo contenedor listado |
| Limpieza necesaria | No | Sí (`container prune` o `--rm`) |
| Requiere contenedor corriendo | Sí | No (crea uno nuevo) |

> **Nota sobre `run --rm`**: si necesitas `run` (por ejemplo, para un contenedor
> aislado), usa `docker compose run --rm` para que se elimine automáticamente al
> terminar. Así no acumulas contenedores `exited`.

---

## Flujo diario del laboratorio

### Ciclo típico

```bash
# Al empezar a trabajar
docker compose up -d

# Durante la sesión: exec para todo
docker compose exec debian-dev bash
docker compose exec alma-dev bash -c 'cd src && make'

# Al terminar (opción 1: detener sin eliminar)
docker compose stop
# Los contenedores quedan en "exited", estado interno preservado

# Al terminar (opción 2: dejar corriendo)
# No hacer nada — contenedores idle consumen <1 MB de RAM

# Al día siguiente
docker compose start    # si se usó stop
# o simplemente exec si siguen corriendo
```

### Tabla de lifecycle

| Comando | Crea contenedores | Inicia | Detiene | Elimina |
|---|---|---|---|---|
| `docker compose up -d` | Sí (si no existen) | Sí | — | — |
| `docker compose start` | No | Sí | — | — |
| `docker compose stop` | No | — | Sí | — |
| `docker compose down` | No | — | Sí | Contenedores + red |
| `docker compose down -v` | No | — | Sí | Todo (incluye volúmenes) |

### Cuándo usar cada uno

| Acción | Cuándo usarla | Qué se preserva | Qué se pierde |
|---|---|---|---|
| `stop` + `start` | Pausar/reanudar sesión | Todo (estado del contenedor) | Nada |
| `down` + `up -d` | Limpiar entre sesiones | Volúmenes, imágenes | Estado del contenedor |
| `down -v` + `up -d` | Reset completo del lab | Imágenes, código fuente | Todo lo demás |

> **Recomendación para el día a día**: usa `stop`/`start`. Reserva `down` para
> cuando quieras un contenedor limpio. Reserva `down -v` para reset total.

### Precaución con `stop` + `container prune`

Si detienes el lab con `stop` y luego ejecutas `container prune`, los
contenedores del lab serán eliminados. Después necesitarás `up -d` (no `start`)
para recrearlos:

```bash
docker compose stop
docker container prune -f    # ¡Esto elimina los contenedores del lab!
docker compose start         # ❌ FALLA — los contenedores ya no existen
docker compose up -d         # ✅ Correcto — recrea los contenedores
```

---

## Alias útiles

Agregar al `~/.bashrc` o `~/.zshrc`:

```bash
# Docker lab aliases
alias docker-space='docker system df'
alias docker-space-detail='docker system df -v'
alias docker-clean='docker container prune -f && docker image prune -f'
alias lab-status='docker compose ps'
alias lab-logs='docker compose logs'
alias lab-restart='docker compose restart'

# Acceso rápido a los contenedores
alias debian='docker compose exec debian-dev bash'
alias alma='docker compose exec alma-dev bash'
```

Uso:

```bash
docker-space          # Vistazo rápido al espacio
docker-clean          # Limpieza rápida (solo con lab corriendo)
lab-status            # Ver si los contenedores del lab están activos
```

> **Para añadir alias sin duplicar**: antes de hacer `cat >>`, verifica si ya
> existen con `grep -q 'docker-space' ~/.bashrc`. Ver el Ejercicio 1.

---

## Espacio en disco requerido

### Desglose del lab

| Componente | Espacio aproximado |
|---|---|
| Imagen Debian dev | ~700 MB |
| Imagen AlmaLinux dev | ~750 MB |
| Toolchain Rust (por imagen) | ~200 MB |
| Volumen workspace | ~50–100 MB |
| Build cache (activo) | ~500 MB |
| **Total del lab** | **~2–3 GB** |

### Espacio adicional durante el curso

| Actividad | Espacio extra |
|---|---|
| Ejercicios con imágenes adicionales | ~500 MB |
| Compilaciones (binarios, objetos) | ~100 MB |
| Imágenes dangling (entre rebuilds) | ~1 GB |
| Build cache acumulado | ~1–2 GB |
| **Total con uso activo** | **~4–6 GB** |

### Umbrales de acción

| Espacio libre | Estado | Acción |
|---|---|---|
| > 10 GB | Excelente | Sin preocupación |
| 5–10 GB | Normal | Rutina normal |
| 2–5 GB | Atención | Ejecutar limpieza semanal |
| < 2 GB | Crítico | Ejecutar limpieza completa |

Mantener al menos **5 GB libres** en la partición donde Docker almacena datos
(`/var/lib/docker/` en rootful, `~/.local/share/docker/` en rootless).

---

## Qué hacer si algo se rompe

### Principio general

Todo lo que importa está en dos lugares protegidos por Git:

1. **Código fuente**: en `src/` del host (bind mount)
2. **Dockerfiles y compose.yml**: en el proyecto

Los contenedores, imágenes y volúmenes son **reconstruibles**. La pérdida de un
volumen workspace implica perder configuraciones temporales (historial de shell,
paquetes instalados a mano), pero no el código fuente.

### Tabla de recuperación

| Problema | Solución | Tiempo |
|---|---|---|
| Contenedor eliminado | `docker compose up -d <servicio>` | Segundos |
| Volumen eliminado | `docker compose up -d` (se recrea vacío) | Segundos |
| Imágenes eliminadas | `docker compose build && docker compose up -d` | 10–20 min |
| Build cache perdido | `docker compose build` (sin cache, más lento) | 10–20 min |
| Todo roto | `docker compose down -v && docker system prune -a -f && docker compose build && docker compose up -d` | 20–30 min |

### Procedimientos detallados

```bash
# Se eliminó un contenedor del lab
docker compose up -d debian-dev    # Recrea solo ese servicio

# Se eliminó el volumen workspace
docker compose down                # Asegurar estado limpio
docker compose up -d               # Recrea volumen vacío
# src/ sigue intacto (es bind mount en el host)

# Se eliminaron las imágenes
docker compose build               # Reconstruye desde Dockerfiles
docker compose up -d
```

---

## Ejercicios

### Ejercicio 1 — Configurar alias sin duplicar

Añade alias de laboratorio al archivo de configuración de tu shell, pero solo
si no existen aún.

```bash
# Verificar si ya están configurados
grep -q 'docker-space' ~/.bashrc && echo "Alias ya existen" || {
    cat >> ~/.bashrc << 'ALIASES'

# Docker lab aliases
alias docker-space='docker system df'
alias docker-clean='docker container prune -f && docker image prune -f'
alias lab-status='docker compose ps'
alias lab-logs='docker compose logs'
ALIASES
    echo "Alias añadidos"
}

# Cargar la configuración
source ~/.bashrc

# Probar
docker-space
lab-status
```

<details>
<summary>Predicción</summary>

La primera ejecución muestra "Alias añadidos" y luego la salida de
`docker system df` y `docker compose ps`. Ejecuciones posteriores muestran
"Alias ya existen" sin modificar `~/.bashrc`. Esto evita el problema del
original, que duplicaba las entradas en cada ejecución.

El patrón `grep -q ... && echo "ya existe" || { ... }` es idiomático en Bash
para operaciones idempotentes de configuración.

</details>

---

### Ejercicio 2 — Demostrar el impacto de run vs exec

Crea contenedores con `run`, cuenta cuántos se acumulan, y compara con `exec`.

```bash
# Contar contenedores antes
echo "=== Antes ==="
docker ps -a --format "table {{.Names}}\t{{.Status}}" | head -5

# Crear 5 contenedores con run
for i in 1 2 3 4 5; do
    docker compose run -T debian-dev echo "run test $i"
done

# Contar contenedores después de run
echo ""
echo "=== Después de 5 runs ==="
docker ps -a --format "table {{.Names}}\t{{.Status}}" | head -10

# Ahora ejecutar 5 veces con exec
for i in 1 2 3 4 5; do
    docker compose exec -T debian-dev echo "exec test $i"
done

# Contar contenedores después de exec
echo ""
echo "=== Después de 5 execs ==="
docker ps -a --format "table {{.Names}}\t{{.Status}}" | head -10

# Limpiar los contenedores de run
docker container prune -f
```

<details>
<summary>Predicción</summary>

Después de los 5 `run`, `docker ps -a` muestra 5 contenedores nuevos en estado
`Exited (0)` con nombres como `<proyecto>-debian-dev-run-<hash>`. Después de
los 5 `exec`, el conteo no cambia — `exec` no crea contenedores nuevos.

Se usa `-T` para desactivar la asignación de pseudo-TTY (necesario cuando no
hay terminal interactivo, como en un script o un loop).

`container prune -f` al final limpia los 5 contenedores creados por `run`.

</details>

---

### Ejercicio 3 — Practicar el ciclo stop/start

Recorre el ciclo completo de lifecycle del lab: detener, verificar estado,
reanudar, y confirmar que todo funciona.

```bash
# Estado inicial
echo "=== Estado inicial ==="
docker compose ps --format "table {{.Name}}\t{{.Status}}"

# Detener
docker compose stop
echo ""
echo "=== Después de stop ==="
docker compose ps --format "table {{.Name}}\t{{.Status}}"

# Verificar que existen pero están parados
docker ps -a --filter "label=com.docker.compose.project" \
    --format "table {{.Names}}\t{{.Status}}" | head -5

# Reanudar
docker compose start
echo ""
echo "=== Después de start ==="
docker compose ps --format "table {{.Name}}\t{{.Status}}"

# Verificar funcionalidad
docker compose exec -T debian-dev echo "Debian OK"
docker compose exec -T alma-dev echo "Alma OK"
```

<details>
<summary>Predicción</summary>

Después de `stop`, `docker compose ps` muestra los servicios con estado
`Exited (0)` o similar. `docker ps -a` con el filtro de label muestra los
contenedores del proyecto con estado `Exited`.

`start` los reanuda instantáneamente (no recrea). `docker compose ps` vuelve a
mostrar `Up`. Los `exec` confirman que los contenedores responden.

El ciclo `stop`/`start` preserva el estado interno del contenedor (historial de
shell, archivos temporales, paquetes instalados a mano). Es la forma más
rápida de pausar y reanudar.

</details>

---

### Ejercicio 4 — Demostrar el peligro de container prune con lab parado

Muestra qué sucede si ejecutas `container prune` con el lab detenido, y cómo
recuperarse.

```bash
# 1. Detener el lab
docker compose stop
echo "=== Lab detenido ==="
docker compose ps --format "table {{.Name}}\t{{.Status}}"

# 2. Ejecutar container prune (¡esto eliminará los contenedores del lab!)
echo ""
echo "=== Ejecutando container prune ==="
docker container prune -f

# 3. Verificar que los contenedores fueron eliminados
echo ""
echo "=== Después del prune ==="
docker compose ps --format "table {{.Name}}\t{{.Status}}"

# 4. Intentar start (va a fallar)
echo ""
echo "=== Intentando start ==="
docker compose start 2>&1 || echo "start falló (esperado)"

# 5. Recuperarse con up -d
echo ""
echo "=== Recuperando con up -d ==="
docker compose up -d
docker compose ps --format "table {{.Name}}\t{{.Status}}"

# 6. Verificar funcionalidad
docker compose exec -T debian-dev echo "Debian OK"
docker compose exec -T alma-dev echo "Alma OK"
```

<details>
<summary>Predicción</summary>

Después de `stop`, los contenedores están en estado `exited`. `container prune`
los elimina porque no distingue entre contenedores efímeros y contenedores de
Compose — elimina TODO lo que esté en estado `exited`.

`docker compose start` falla con un error indicando que no hay contenedores para
iniciar (el servicio no tiene contenedor). `docker compose up -d` funciona
correctamente porque recrea los contenedores desde las imágenes existentes.

Este ejercicio demuestra por qué la recomendación del material original
("los contenedores parados están protegidos por Compose") es incorrecta. La
única protección real es que los contenedores estén **corriendo**.

**Lección**: si usas `stop`, no ejecutes `container prune` hasta haber
reanudado con `start`. Si necesitas limpiar, usa `down` primero (que ya elimina
los contenedores de forma controlada).

</details>

---

### Ejercicio 5 — Verificar resiliencia del lab ante limpieza segura

Confirma que la limpieza rutinaria no afecta al lab **mientras está corriendo**.

```bash
# 1. Asegurar que el lab está corriendo
docker compose up -d

# 2. Crear basura simulada
for i in 1 2 3; do
    docker run --name "lab-trash-$i" alpine echo "trash $i"
done

# 3. Verificar: basura + lab coexisten
echo "=== Antes de limpiar ==="
docker ps -a --format "table {{.Names}}\t{{.Status}}" | head -10

# 4. Limpieza segura
echo ""
echo "=== Limpieza ==="
docker container prune -f
docker image prune -f

# 5. Verificar: basura eliminada, lab intacto
echo ""
echo "=== Después de limpiar ==="
docker ps -a --format "table {{.Names}}\t{{.Status}}" | head -10

# 6. Lab sigue funcionando
docker compose exec -T debian-dev echo "Debian OK"
docker compose exec -T alma-dev echo "Alma OK"
```

<details>
<summary>Predicción</summary>

`container prune` elimina los 3 contenedores `lab-trash-*` (estado `exited`)
pero no toca los contenedores del lab (estado `Up`). `image prune` elimina la
imagen `alpine` si quedó como dangling (aunque en este caso tiene tag, así que
probablemente no la elimine).

Después de la limpieza, `docker ps -a` muestra solo los 2 contenedores del lab.
Los `exec` confirman que todo sigue funcionando.

Este es el escenario seguro: limpiar **con el lab corriendo**. El Ejercicio 4
mostró el escenario peligroso (limpiar con el lab parado).

</details>

---

### Ejercicio 6 — Ejecutar la rutina completa de limpieza

Recorre toda la rutina de mantenimiento (diaria + semanal + mensual) y mide
el espacio recuperado.

```bash
echo "=== Espacio ANTES ==="
docker system df
echo ""

echo "=== Limpieza diaria: contenedores ==="
docker container prune -f
echo ""

echo "=== Limpieza semanal: imágenes dangling ==="
docker image prune -f
echo ""

echo "=== Limpieza mensual: build cache ==="
docker builder prune --keep-storage 2GB -f
echo ""

echo "=== Espacio DESPUÉS ==="
docker system df
echo ""

echo "=== Estado del lab ==="
docker compose ps --format "table {{.Name}}\t{{.Status}}"
```

<details>
<summary>Predicción</summary>

La cantidad de espacio recuperado depende del uso previo. En un lab recién
configurado, probablemente no haya mucho que limpiar. Después de semanas de
uso con varios `docker compose build` y ejercicios con `docker run`, la
diferencia puede ser significativa (cientos de MB a varios GB).

`builder prune --keep-storage 2GB` solo limpia si hay más de 2 GB de build
cache acumulado. Si hay menos, no hace nada.

El lab sigue corriendo porque estos comandos solo eliminan recursos no
utilizados.

</details>

---

### Ejercicio 7 — Comparar tiempos de build con y sin cache

Mide cuánto ahorra el build cache al reconstruir las imágenes del lab.

```bash
# Ver cache actual
echo "=== Cache actual ==="
docker system df | grep "Build Cache"

# Build con cache (debería ser rápido)
echo ""
echo "=== Build CON cache ==="
time docker compose build 2>&1 | tail -5

# Limpiar TODO el build cache
echo ""
echo "=== Limpiando cache ==="
docker builder prune -af

# Build sin cache (debería ser lento)
echo ""
echo "=== Build SIN cache ==="
time docker compose build 2>&1 | tail -5

# Ver nuevo cache generado
echo ""
echo "=== Cache regenerado ==="
docker system df | grep "Build Cache"
```

<details>
<summary>Predicción</summary>

El build con cache toma segundos (todas las capas están cacheadas, Docker
muestra `CACHED` para cada paso del Dockerfile). El build sin cache toma
10–20 minutos porque debe descargar paquetes, instalar toolchains, etc.

Esto demuestra por qué `builder prune --keep-storage 2GB` es preferible a
`builder prune -a`: mantener cache reciente ahorra minutos en cada rebuild.

`docker builder prune -af` usa `-a` (all, incluyendo cache en uso) y `-f`
(force, sin confirmación). Es más agresivo que el `--keep-storage` de la
rutina mensual.

</details>

---

### Ejercicio 8 — Simular pérdida de volumen y recuperarse

Elimina el volumen workspace del lab y verifica que el bind mount (`src/`)
sobrevive.

```bash
# 1. Verificar archivos en src/ (bind mount, vive en el host)
echo "=== Archivos en src/ ==="
ls -la src/ 2>/dev/null || echo "src/ no encontrado en directorio actual"

# 2. Encontrar el nombre real del volumen
echo ""
echo "=== Volúmenes del proyecto ==="
docker volume ls --filter "label=com.docker.compose.project" \
    --format "table {{.Name}}\t{{.Labels}}"

# 3. Detener y eliminar todo (incluyendo volúmenes)
docker compose down -v

# 4. Confirmar que el volumen fue eliminado
echo ""
echo "=== Volúmenes después de down -v ==="
docker volume ls --filter "label=com.docker.compose.project"

# 5. Confirmar que src/ sigue intacto
echo ""
echo "=== src/ después de down -v ==="
ls -la src/ 2>/dev/null || echo "src/ no encontrado"

# 6. Recrear el lab
docker compose up -d

# 7. Verificar funcionalidad
echo ""
echo "=== Lab recreado ==="
docker compose ps --format "table {{.Name}}\t{{.Status}}"
docker compose exec -T debian-dev echo "Debian OK"
```

<details>
<summary>Predicción</summary>

`docker compose down -v` elimina contenedores, redes Y volúmenes definidos en
el `compose.yml`. El volumen `workspace` desaparece de `docker volume ls`.

Sin embargo, `src/` sigue intacto porque es un **bind mount** — un directorio
del host montado dentro del contenedor. No es gestionado por Docker; vive en el
filesystem del host y no se ve afectado por ningún comando de Docker.

`docker compose up -d` recrea todo: contenedores, red, y volumen (vacío). El
volumen nuevo no tiene el historial de shell ni configuraciones temporales del
anterior, pero el código fuente en `src/` está intacto.

**Lección**: el bind mount es la pieza que nunca se pierde (salvo que borres
el directorio directamente en el host). El volumen es reconstruible.

</details>

---

### Ejercicio 9 — Reset completo y reconstrucción

Ejecuta un reset total del entorno Docker y reconstruye el lab desde cero.
Mide cuánto tarda.

```bash
# Estado antes
echo "=== Estado antes del reset ==="
docker system df

# Reset completo
echo ""
echo "=== Ejecutando reset ==="
docker compose down -v
docker system prune -a -f

# Estado post-reset
echo ""
echo "=== Estado después del reset ==="
docker system df
docker image ls
docker volume ls

# Reconstruir
echo ""
echo "=== Reconstruyendo lab ==="
time docker compose build
time docker compose up -d

# Verificar
echo ""
echo "=== Verificación ==="
docker compose ps --format "table {{.Name}}\t{{.Status}}"
docker compose exec -T debian-dev bash -c 'gcc --version | head -1'
docker compose exec -T alma-dev bash -c 'rustc --version'
```

<details>
<summary>Predicción</summary>

Después de `docker system prune -a -f`, todo desaparece: imágenes, build cache,
redes no usadas. `docker system df` muestra 0 B en todas las categorías.
`docker image ls` está vacío. `docker volume ls` está vacío.

`docker compose build` toma 10–20 minutos porque debe descargar las imágenes
base (Debian, AlmaLinux), instalar todos los paquetes (gcc, gdb, valgrind,
rustup, etc.) y compilar lo necesario. Sin cache, todo se hace desde cero.

`docker compose up -d` es rápido (segundos) porque las imágenes ya están
construidas.

Este ejercicio demuestra que el lab es completamente reconstruible. El peor
caso (perder todo) se resuelve con un `build` + `up -d`. Lo único que no se
puede reconstruir es el contenido del bind mount `src/` — pero eso está
protegido por Git.

</details>

---

### Ejercicio 10 — Documentar tu rutina personal

Crea un archivo de checklist adaptado a tu flujo de trabajo.

```bash
cat > ~/LAB_MAINTENANCE.md << 'EOF'
# Lab Maintenance Checklist

## Daily (after each session)
- [ ] Verify lab is running: `docker compose ps`
- [ ] Clean ephemeral containers: `docker container prune -f`
  - ⚠️ Only if lab containers show "Up" status

## Weekly
- [ ] Clean dangling images: `docker image prune -f`
- [ ] Check disk space: `docker system df`

## Monthly
- [ ] Clean old build cache: `docker builder prune --keep-storage 2GB -f`
- [ ] Review volumes: `docker volume ls`
- [ ] Verify lab works: `docker compose exec -T debian-dev gcc --version`

## If disk space is critical (< 2 GB free)
- [ ] Run full cleanup: `docker system prune -a -f`
- [ ] Rebuild lab: `docker compose build && docker compose up -d`
- [ ] Verify: `docker compose ps`

## If something breaks
- [ ] Check src/ directory (bind mount, always safe)
- [ ] Rebuild: `docker compose build && docker compose up -d`
- [ ] Nuclear option: `docker compose down -v && docker system prune -a -f`
  then: `docker compose build && docker compose up -d`
EOF

cat ~/LAB_MAINTENANCE.md
```

<details>
<summary>Predicción</summary>

Se crea el archivo `~/LAB_MAINTENANCE.md` con el checklist. `cat` muestra su
contenido. El archivo sirve como referencia rápida para el mantenimiento.

Nota: a diferencia del Ejercicio 1 (que usa `grep -q` para evitar duplicados
en `~/.bashrc`), aquí se usa `>` (sobrescribir) en lugar de `>>` (append)
porque el checklist se reemplaza completo cada vez, no se acumula.

Los checkboxes (`- [ ]`) son formato Markdown estándar. Si usas un editor con
soporte Markdown (VS Code, GitHub), puedes marcarlos interactivamente.

</details>

---

## Resumen de conceptos

| Concepto | Detalle clave |
|---|---|
| `exec` vs `run` | `exec` reutiliza, `run` crea nuevo. Usar siempre `exec` |
| `stop`/`start` | Preserva estado del contenedor. Usar para pausar sesiones |
| `down`/`up -d` | Elimina y recrea contenedores. Volúmenes sobreviven |
| `down -v` | Reset total. Solo los bind mounts sobreviven |
| `container prune` con lab parado | **Elimina los contenedores del lab** |
| `volume prune` con lab parado | Seguro (contenedores parados referencian volúmenes) |
| Bind mount vs volumen | Bind mount vive en el host (seguro). Volumen es reconstruible |
| Build cache | `--keep-storage 2GB` para equilibrio entre espacio y velocidad |
| Reconstrucción total | `build` + `up -d` = 10–20 min. Todo es reconstruible |
