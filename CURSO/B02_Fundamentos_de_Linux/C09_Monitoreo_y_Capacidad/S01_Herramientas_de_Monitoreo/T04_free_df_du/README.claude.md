# T04 — free, df, du

> **Objetivo:** Dominar `free`, `df` y `du` para diagnosticar problemas de memoria y espacio en disco. Entender la diferencia entre memoria real vs disponible, y cómo encontrar qué ocupa espacio.

## Erratas detectadas en el material original

| Archivo | Línea | Error | Corrección |
|---------|-------|-------|------------|
| `README.max.md` | 261 | `excluyendo虚拟` — texto chino (虚拟 = "virtual") | `excluyendo virtuales` |

---

## free — Memoria RAM

```bash
free -h
#               total        used        free      shared  buff/cache   available
# Mem:          7.6Gi       1.9Gi       488Mi        98Mi       5.2Gi       5.3Gi
# Swap:         1.9Gi          0B       1.9Gi
```

### Columnas de free

| Columna | Descripción |
|---------|-------------|
| `total` | RAM física total instalada |
| `used` | RAM usada (calculada: total - free - buff/cache) |
| `free` | RAM completamente libre (sin uso alguno) |
| `shared` | Memoria compartida (tmpfs, shared memory segments) |
| `buff/cache` | Memoria como buffers del kernel + page cache |
| `available` | **RAM disponible para nuevas aplicaciones** |

```bash
# CRÍTICO: "free" bajo NO significa problema
# Linux usa TODA la RAM libre como cache de disco
# La columna "available" es la que importa
# available = free + buff/cache que se puede liberar bajo presión

# Cuándo preocuparse:
#   available < 10% de total    → presión de memoria
#   swap used > 0 + pswpout activo (sar -W) → RAM insuficiente
#   available ≈ 0              → OOM killer puede activarse
```

### Opciones de free

```bash
# Formato legible:
free -h         # human-readable (auto: Ki, Mi, Gi)

# Unidades específicas:
free -m         # en MiB (base 1024)
free -g         # en GiB (base 1024)
free --si       # usar potencias de 1000 (MB, GB) en vez de 1024

# Wide output (separar buffers y cache):
free -w -h
#               total        used        free      shared     buffers       cache   available
# Mem:          7.6Gi       1.9Gi       488Mi        98Mi       125Mi       5.1Gi       5.3Gi
#                                                              ^^^^^^^     ^^^^^^^
#                                                              separados

# Total (sumar RAM + swap):
free -h -t
# Agrega línea "Total:" sumando Mem + Swap

# Monitoreo continuo:
free -h -s 2        # cada 2 segundos (infinito)
free -h -s 2 -c 5   # cada 2 segundos, 5 veces
```

### free en scripts

```bash
# Memoria disponible en MB:
free -m | awk '/^Mem:/{print $7}'
# 5300

# Porcentaje de memoria disponible:
TOTAL=$(free -m | awk '/^Mem:/{print $2}')
AVAIL=$(free -m | awk '/^Mem:/{print $7}')
PERCENT=$((AVAIL * 100 / TOTAL))
echo "Disponible: ${PERCENT}% (${AVAIL}MB de ${TOTAL}MB)"

# Alerta si disponible < 10%:
if [ "$PERCENT" -lt 10 ]; then
    echo "ALERTA: solo ${PERCENT}% de RAM disponible"
fi

# Verificar si hay swap activa:
SWAP_USED=$(free -m | awk '/^Swap:/{print $3}')
if [ "$SWAP_USED" -gt 0 ]; then
    echo "ADVERTENCIA: ${SWAP_USED}MB en swap"
fi
```

---

## df — Espacio en disco (filesystems)

```bash
df -h
# Filesystem      Size  Used Avail Use% Mounted on
# /dev/sda1        48G   20G   27G  43% /
# /dev/sda2        96G   43G   48G  48% /home
# tmpfs           3.9G   49M  3.8G   2% /tmp
# /dev/sdb1       932G  500G  385G  57% /data
```

### Opciones de df

```bash
# Formato legible:
df -h

# Solo filesystems locales (excluir red y virtuales):
df -h -l

# Solo un tipo de filesystem:
df -h -t ext4
df -h -t xfs

# Excluir tipos (limpiar salida):
df -h -x tmpfs -x devtmpfs -x squashfs

# Inodos (no espacio):
df -i
# Filesystem      Inodes  IUsed   IFree IUse% Mounted on
# /dev/sda1      3200000 150000 3050000    5% /

# Formato custom con --output:
df -h --output=source,size,used,avail,pcent,target

# Filesystem de un archivo/directorio:
df -h /var/log
# Muestra el filesystem donde está /var/log

# Agregar totales:
df -h --total
```

### Problema de inodos

```bash
# Un filesystem puede tener ESPACIO libre pero SIN INODOS
# Cada archivo (y directorio) ocupa un inodo
# El número de inodos es fijo al crear el filesystem

# Síntoma: "No space left on device" con df -h mostrando espacio libre
# Causa: millones de archivos pequeños (ej. cache, session files)

# Verificar:
df -i /
# Si IUse% es alto → problema de inodos, no de espacio
```

### df en scripts

```bash
# Porcentaje de uso del filesystem raíz:
df / | awk 'NR==2{print $5}' | tr -d '%'
# 43

# Alerta si algún filesystem > 90%:
df -h --output=pcent,target | tail -n +2 | while read -r pcent target; do
    USE=$(echo "$pcent" | tr -d '% ')
    if [ "$USE" -gt 90 ]; then
        echo "ALERTA: $target al ${USE}%"
    fi
done

# Espacio disponible en GB:
df -BG /data | awk 'NR==2{print $4}' | tr -d 'G'
# 385
```

### Discrepancia df vs du

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  PROBLEMA: df muestra MÁS espacio usado que du                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  $ df -h /         → 20G used                                              │
│  $ du -sh /        → 18G                                                   │
│                                                                              │
│  → Diferencia de 2GB — ¿dónde están?                                      │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Causa 1 — Archivos eliminados pero aún abiertos:**
```bash
# Si un proceso tiene un archivo abierto y se elimina (rm),
# el espacio NO se libera hasta que el proceso cierre el archivo
# du no ve el archivo (ya no tiene nombre), pero df sí (el bloque está ocupado)
sudo lsof +L1 2>/dev/null | head -10
# Muestra archivos eliminados (link count 0) aún abiertos
# Solución: reiniciar el proceso o hacer truncate
```

**Causa 2 — Filesystems montados sobre directorios con contenido:**
```bash
# Si /tmp tiene archivos y luego montas tmpfs sobre /tmp,
# los archivos originales siguen ocupando espacio pero son invisibles
# du no los ve (ve el tmpfs montado), df sí
```

**Causa 3 — Bloques reservados:**
```bash
# ext4 reserva 5% del espacio para root por defecto
# df descuenta estos bloques del "available", pero siguen usados
tune2fs -l /dev/sda1 | grep "Reserved block count"
# Para cambiar: tune2fs -m 1 /dev/sda1  (reducir a 1%)
```

---

## du — Uso de disco (directorios)

```bash
# Resumen de un directorio:
du -sh /var/log
# 256M    /var/log

# Cada subdirectorio (un nivel):
du -h /var/log --max-depth=1
# 128M    /var/log/journal
# 64M     /var/log/nginx
# 32M     /var/log/syslog
# 256M    /var/log
```

### Opciones clave

```bash
# -s = summary (solo el total):
du -sh /home/user
# 5.2G    /home/user

# -h = human-readable:
du -h /var/log

# --max-depth=N = profundidad de exploración:
du -h --max-depth=1 /var    # un nivel
du -h --max-depth=2 /var    # dos niveles

# -a = incluir archivos (no solo directorios):
du -ah /var/log | head -10

# -c = total al final:
du -shc /var/log /var/lib /var/cache
# 256M    /var/log
# 512M    /var/lib
# 100M    /var/cache
# 868M    total

# --exclude = excluir patrones:
du -sh --exclude='*.gz' /var/log
# Excluir archivos comprimidos del conteo

# -x = no cruzar filesystems:
du -shx /
# Solo el filesystem de /, no salta a /home si es otro disco
# IMPORTANTE para no contar otros filesystems

# --apparent-size = tamaño real (no bloques):
du -sh /var/log                    # tamaño en disco (bloques asignados)
du -sh --apparent-size /var/log    # tamaño real de los datos
# Archivos sparse pueden mostrar diferencias grandes
```

### Encontrar qué ocupa más espacio

```bash
# Top 10 directorios más grandes en /var:
du -h /var --max-depth=2 2>/dev/null | sort -rh | head -10

# Top 10 archivos más grandes (excluyendo virtuales):
sudo find / -xdev -type f -exec du -sh {} + 2>/dev/null | sort -rh | head -10
# 2.1G    /var/lib/docker/overlay2/.../layer.tar
# 500M    /var/log/syslog.1

# Dentro de /home:
du -ah /home | sort -rh | head -20

# Por usuario:
sudo du -sh /home/*/ 2>/dev/null | sort -rh
# 5.2G    /home/dev/
# 2.1G    /home/admin/
# 500M    /home/user/
```

### du en scripts

```bash
# Tamaño de un directorio en MB:
du -sm /var/log | awk '{print $1}'
# 256

# Alerta si /var/log supera 1GB:
SIZE=$(du -sm /var/log 2>/dev/null | awk '{print $1}')
if [ "$SIZE" -gt 1024 ]; then
    echo "ALERTA: /var/log ocupa ${SIZE}MB"
fi
```

---

## ncdu — du interactivo

```bash
# ncdu es una alternativa interactiva a du:
sudo apt install ncdu    # Debian
sudo dnf install ncdu    # RHEL

# Explorar interactivamente:
ncdu /var
# Interfaz ncurses con:
#   ↑↓       navegar por directorios
#   ENTER    entrar en directorio
#   d        eliminar archivo/directorio
#   n        ordenar por nombre/tamaño
#   q        salir

# Guardar escaneo para análisis posterior:
ncdu -o /tmp/scan.json /var
ncdu -f /tmp/scan.json     # cargar sin re-escanear
```

---

## Diagnóstico: "El disco está lleno"

```bash
# PASO 1: ¿Qué filesystem está lleno?
df -h

# PASO 2: ¿Qué directorio ocupa más?
du -h / --max-depth=1 -x | sort -rh | head -5

# PASO 3: Profundizar en el directorio sospechoso:
du -h /var --max-depth=2 | sort -rh | head -10

# PASO 4: ¿Archivos eliminados pero abiertos?
sudo lsof +L1 2>/dev/null | awk '{print $7, $9}' | sort -rn | head -10

# PASO 5: ¿Problema de inodos?
df -i

# PASO 6: Acciones inmediatas:
sudo journalctl --vacuum-size=200M     # limpiar journal
sudo apt clean                          # limpiar cache de apt
sudo dnf clean all                      # limpiar cache de dnf
sudo find /tmp -mtime +7 -delete        # limpiar /tmp viejos
sudo find / -xdev -type f -size +100M -exec ls -lh {} \;  # encontrar archivos grandes
```

---

## Resumen de las tres herramientas

| Herramienta | Qué mide | Métrica clave | Uso principal |
|-------------|----------|---------------|---------------|
| `free` | RAM y swap | `available` (no `free`) | ¿Hay suficiente memoria? |
| `df` | Espacio por filesystem | `Use%` y `df -i` | ¿Qué disco está lleno? |
| `du` | Espacio por directorio | `du -sh` + `sort -rh` | ¿Qué directorio/archivo ocupa más? |

---

## Quick reference

```
FREE:
  free -h              → formato legible
  free -m/-g           → MiB/GiB
  free -w              → wide (separar buffers/cache)
  free -t              → total (RAM + swap)
  free -s N            → continuo cada N segundos
  free -s N -c M       → cada N segundos, M veces

DF:
  df -h                → legible
  df -i                → inodos
  df -l                → solo locales
  df -t ext4           → solo un tipo
  df -x tmpfs          → excluir tipo
  df --output=...      → columnas custom
  df --total           → con totales
  df FILE              → filesystem de archivo

DU:
  du -sh DIR           → solo total
  du -h --max-depth=1  → un nivel de profundidad
  du -h --max-depth=2  → dos niveles
  du -ah               → incluir archivos
  du -shc D1 D2        → múltiples dirs + total
  du -shx /            → no cruzar filesystems
  du --exclude='*.gz'  → excluir patrones

NCDU:
  ncdu DIR             → explorador interactivo
  ncdu -o FILE         → guardar escaneo
  ncdu -f FILE         → cargar escaneo

DIAGNÓSTICO "DISCO LLENO":
  df -h → du --max-depth=1 → du --max-depth=2 → df -i → lsof +L1
```

---

## Ejercicios

### Ejercicio 1 — Interpretar free

```bash
# Examina esta salida de free -h:
#               total        used        free      shared  buff/cache   available
# Mem:          7.6Gi       1.9Gi        64Mi        98Mi       5.6Gi       5.4Gi
# Swap:         1.9Gi       200Mi       1.7Gi

# Predicción: ¿El sistema tiene problema de memoria?
# ¿Por qué "free" es solo 64Mi pero el sistema funciona bien?
```

<details><summary>Predicción</summary>

**No hay problema de memoria.** Aunque `free` es solo 64Mi (parece alarmante), `available` es 5.4Gi — eso es el 71% del total.

Linux usa la RAM libre agresivamente como `buff/cache` (5.6Gi). Este cache se libera automáticamente cuando las aplicaciones necesitan más RAM. Por eso `free` bajo es **normal y deseable** — RAM libre sin usar como cache es RAM desperdiciada.

El swap tiene 200Mi usados. Esto no es necesariamente un problema si no hay actividad de swap (verificar con `vmstat si/so` o `sar -W`). Puede ser swap residual de un pico de uso pasado.

**Lo que importa:** `available` (5.4Gi) > 10% de `total` (7.6Gi) → sin presión de memoria.

</details>

### Ejercicio 2 — free en vivo y wide

```bash
# 1. Ejecuta free con output wide:
free -w -h

# 2. Ejecuta monitoreo continuo (3 muestras):
free -h -s 1 -c 3

# Predicción: ¿Cuál es la diferencia entre -h y -w -h?
# ¿Por qué querrías separar buffers de cache?
```

<details><summary>Predicción</summary>

- `free -h` muestra `buff/cache` como una sola columna combinada.
- `free -w -h` las separa en `buffers` y `cache` individuales.

Separar es útil para diagnóstico:
- **buffers** = metadata de filesystem (inodos, directorios, superblocks). Generalmente pequeños (100-300Mi).
- **cache** = page cache (contenido de archivos leídos del disco). Generalmente grande.

Si `cache` es enorme pero `buffers` es pequeño → el sistema ha leído muchos archivos del disco (normal para servidores web, DB). Si `buffers` es inusualmente grande → mucha actividad de metadata del filesystem (ej. find recursivo, muchas operaciones en directorios con millones de archivos).

</details>

### Ejercicio 3 — Filtrar df

```bash
# 1. Muestra solo filesystems reales (sin tmpfs, devtmpfs, squashfs):
df -h -x tmpfs -x devtmpfs -x squashfs

# 2. Muestra el filesystem donde está /var/log:
df -h /var/log

# 3. Muestra inodos del filesystem raíz:
df -i /

# Predicción: Si df -h / muestra 43% usado pero df -i / muestra 95% IUse%,
# ¿puedes crear archivos nuevos? ¿Por qué?
```

<details><summary>Predicción</summary>

**Probablemente no podrás crear archivos nuevos.** Aunque hay espacio libre (solo 43% usado), los inodos están casi agotados (95% IUse%). Cada archivo y directorio necesita un inodo. Si se agotan los inodos, el sistema reportará "No space left on device" aunque haya espacio en disco.

Causa típica: millones de archivos pequeños (cache de sesiones, archivos temporales de correo, thumbnails).

Soluciones:
- Encontrar y eliminar archivos innecesarios: `find / -xdev -type f | wc -l` (contar archivos)
- Buscar directorios con demasiados archivos: `find / -xdev -type d -exec sh -c 'echo "$(find "$1" -maxdepth 1 | wc -l) $1"' _ {} \; | sort -rn | head -10`
- A largo plazo: recrear el filesystem con más inodos (`mkfs.ext4 -N` o `-i bytes-per-inode`)

</details>

### Ejercicio 4 — Discrepancia df vs du

```bash
# 1. Compara el espacio usado según df y du en /:
echo "df dice:"
df -h / | awk 'NR==2{print $3 " usado"}'
echo "du dice:"
du -shx / 2>/dev/null

# 2. Busca archivos eliminados pero abiertos:
sudo lsof +L1 2>/dev/null | head -5

# Predicción: Nombra las 3 causas de discrepancia.
# Si un proceso tiene un log de 5GB eliminado pero abierto,
# ¿cómo recuperarías ese espacio sin reiniciar el proceso?
```

<details><summary>Predicción</summary>

Las 3 causas:
1. **Archivos eliminados pero abiertos** — `rm` borra el nombre pero el proceso mantiene el file descriptor abierto. El espacio no se libera hasta que el proceso cierre el archivo.
2. **Filesystems montados sobre directorios con contenido** — los archivos originales quedan ocultos bajo el mount point pero siguen ocupando espacio.
3. **Bloques reservados de ext4** — 5% del espacio reservado para root por defecto.

Para recuperar 5GB sin reiniciar el proceso:
```bash
# 1. Encontrar el PID y el file descriptor:
sudo lsof +L1 | grep "deleted" | grep "5G"
# myapp 1234 root 3w REG 8,1 5368709120 0 /var/log/myapp.log (deleted)

# 2. Truncar el archivo via /proc:
sudo truncate -s 0 /proc/1234/fd/3
# Esto vacía el archivo sin cerrarlo — el proceso puede seguir escribiendo
```

El truco está en que `/proc/PID/fd/N` sigue siendo una referencia válida al archivo aunque su nombre fue eliminado.

</details>

### Ejercicio 5 — du: encontrar consumidores

```bash
# 1. Top 5 directorios más grandes en / (sin cruzar filesystems):
du -h / --max-depth=1 -x 2>/dev/null | sort -rh | head -5

# 2. Top 5 en /var con dos niveles de profundidad:
du -h /var --max-depth=2 2>/dev/null | sort -rh | head -5

# 3. Top 5 archivos más grandes en /var/log:
du -ah /var/log 2>/dev/null | sort -rh | head -5

# Predicción: ¿Por qué es importante usar -x con du -sh /?
# ¿Qué pasaría sin -x si /home está en otro disco?
```

<details><summary>Predicción</summary>

`-x` le dice a du que **no cruce filesystem boundaries**. Sin `-x`:

- `du -sh /` recorrería **todos** los filesystems montados bajo `/`: `/home`, `/var`, `/tmp` (si es tmpfs), `/boot`, etc.
- Si `/home` está en otro disco de 1TB, du contaría ese terabyte como parte de `/`, dando un total inflado e inútil para diagnosticar qué consume espacio en el filesystem raíz.

Con `-x`:
- `du -shx /` solo mide el filesystem donde está `/` (ej. `/dev/sda1`), ignorando mount points que apuntan a otros discos.
- Esto da una medición precisa que se puede comparar directamente con `df -h /`.

Es especialmente importante cuando quieres saber **qué directorio del filesystem raíz ocupa más**, no el total de todos los discos del sistema.

</details>

### Ejercicio 6 — du: opciones avanzadas

```bash
# 1. Tamaño de /var/log excluyendo archivos .gz:
du -sh --exclude='*.gz' /var/log 2>/dev/null

# 2. Tamaño aparente vs en disco:
du -sh /var/log 2>/dev/null
du -sh --apparent-size /var/log 2>/dev/null

# 3. Múltiples directorios con total:
du -shc /var /etc /tmp 2>/dev/null

# Predicción: ¿Cuándo sería diferente du -sh vs du -sh --apparent-size?
# ¿Qué son los archivos sparse?
```

<details><summary>Predicción</summary>

`du -sh` mide **bloques de disco asignados** (espacio real ocupado en el disco). `du -sh --apparent-size` mide el **tamaño lógico del archivo** (lo que reporta `ls -l`).

Son diferentes cuando hay **archivos sparse**: archivos que tienen "huecos" sin datos asignados. El filesystem no asigna bloques para las secciones vacías.

Ejemplo: un archivo de base de datos de 10GB donde solo 2GB tienen datos:
```
du -sh database.img          → 2.0G   (bloques reales asignados)
du -sh --apparent-size database.img → 10G (tamaño lógico)
ls -l database.img           → 10737418240 bytes (10G aparente)
```

Archivos sparse comunes: imágenes de disco de VM (qcow2, raw preallocated), bases de datos, core dumps, archivos creados con `truncate -s 10G file`.

Para la mayoría de archivos normales (logs, código, configuraciones), ambos valores son prácticamente iguales.

</details>

### Ejercicio 7 — Script de alerta de memoria

```bash
# Escribe un script que alerte si:
# - Memoria disponible < 10%
# - Swap usada > 0

TOTAL=$(free -m | awk '/^Mem:/{print $2}')
AVAIL=$(free -m | awk '/^Mem:/{print $7}')
PERCENT=$((AVAIL * 100 / TOTAL))
SWAP_USED=$(free -m | awk '/^Swap:/{print $3}')

echo "RAM: ${AVAIL}MB disponible de ${TOTAL}MB (${PERCENT}%)"
echo "Swap: ${SWAP_USED}MB usada"

# Predicción: ¿Por qué verificamos swap ADEMÁS de available?
# Si available=80% pero swap_used=500MB, ¿hay problema?
```

<details><summary>Predicción</summary>

Verificamos swap **además** de available porque son indicadores complementarios:

- `available` es una foto del **momento actual** — cuánta RAM hay ahora para aplicaciones.
- `swap_used > 0` indica que **en algún momento** hubo presión de memoria suficiente para empujar páginas a disco.

Si `available=80%` pero `swap_used=500MB`: **probablemente no hay problema actual**, pero hubo uno en el pasado. El sistema tuvo un pico de uso de memoria, movió 500MB a swap, y luego la presión cedió. Las páginas siguen en swap porque no se han necesitado de vuelta.

Para confirmar que no es problema activo, verificar con `vmstat` o `sar -W`:
- Si `si=0` y `so=0` → swap residual, sin actividad actual (OK)
- Si `si > 0` o `so > 0` → swap activo, el sistema sigue bajo presión (PROBLEMA)

La combinación más preocupante sería: `available < 10%` **Y** `swap_used > 0` **Y** actividad de swap (`so > 0`). Eso indica RAM insuficiente.

</details>

### Ejercicio 8 — Script de alerta de disco

```bash
# Alerta si algún filesystem supera 90%:
df -h --output=pcent,target | tail -n +2 | while read -r pcent target; do
    USE=$(echo "$pcent" | tr -d '% ')
    if [ "$USE" -gt 90 ]; then
        echo "ALERTA: $target al ${USE}%"
    fi
done

# Predicción: ¿Por qué usamos --output=pcent,target en vez de
# parsear la salida estándar de df? ¿Qué problema puede haber
# con df sin --output cuando el nombre del device es muy largo?
```

<details><summary>Predicción</summary>

Sin `--output`, la salida estándar de df puede tener problemas de parseo:

1. **Nombres de dispositivo largos** (ej. `/dev/mapper/vg-lv_root`) pueden hacer que df **rompa la línea en dos**, poniendo el device en una línea y las estadísticas en la siguiente. Esto rompe `awk 'NR==2{print $5}'` porque `$5` ya no es `Use%`.

2. Los números de columna varían según el contexto. `--output=pcent,target` garantiza que siempre obtienes exactamente las columnas que necesitas, en el orden esperado, sin importar la longitud de los nombres.

3. El formato de `--output` no incluye header con `Filesystem`, así que `tail -n +2` salta correctamente solo la línea de encabezado.

Sin `--output`, un script robusto necesitaría:
```bash
df -P /  # -P fuerza formato POSIX (una línea por filesystem)
```

Pero `--output` es más limpio y explícito.

</details>

### Ejercicio 9 — Flujo de diagnóstico completo

```bash
# Escenario: un usuario reporta "No space left on device" en el servidor.
# Ejecuta el flujo de diagnóstico paso a paso.

# Predicción: Ordena los 5 pasos del diagnóstico y explica
# qué información da cada uno. ¿En qué caso irías directo
# al paso 5 (df -i) antes que al paso 2 (du)?
```

<details><summary>Predicción</summary>

Los 5 pasos en orden:
1. **`df -h`** — ¿Qué filesystem está lleno? Identifica Use% ≈ 100%.
2. **`du -h /MOUNTPOINT --max-depth=1 -x | sort -rh`** — ¿Qué directorio del filesystem ocupa más?
3. **`du -h /DIR --max-depth=2 | sort -rh`** — Profundizar en el directorio sospechoso.
4. **`lsof +L1`** — ¿Hay archivos eliminados pero abiertos consumiendo espacio invisible?
5. **`df -i`** — ¿Es un problema de inodos en vez de espacio?

**Irías directo a `df -i` (paso 5) si:**
- `df -h` muestra que el filesystem **NO está lleno** en espacio (ej. 45% Use%) pero el error sigue siendo "No space left on device". La única explicación es que los inodos se agotaron.
- También si sospechas que el problema son muchos archivos pequeños (ej. servidor de correo, cache de sesiones PHP, `/tmp` con millones de archivos temporales).

El paso 4 (lsof +L1) es útil si `du` del filesystem muestra menos uso que `df` — la discrepancia apunta a archivos eliminados pero abiertos.

</details>

### Ejercicio 10 — Combinar free, df, du con vmstat/iostat/sar

```bash
# Escenario: el servidor está lento Y el disco se está llenando.

# Predicción: ¿Cómo combinarías free/df/du con vmstat/iostat
# para diagnosticar si la lentitud es causada por el disco lleno?
# Describe el flujo de diagnóstico completo.
```

<details><summary>Predicción</summary>

Flujo integrado de diagnóstico:

**1. Identificar el síntoma principal con vmstat:**
```bash
vmstat 1 5
```
- Si `wa` alto → el disco es el cuello de botella
- Si `si/so` alto → la memoria es insuficiente y está swappeando
- Si `us` alto + `id` bajo → es CPU, no disco

**2. Verificar memoria con free:**
```bash
free -h
```
- Si `available` < 10% → presión de memoria puede causar swap → I/O excesivo al disco de swap
- El disco se llena más rápido si el sistema escribe logs de errores OOM

**3. Verificar espacio con df:**
```bash
df -h
```
- Si un filesystem está al 100% → aplicaciones fallan al escribir → pueden generar errores/retries que causan lentitud
- Un disco lleno puede causar que la base de datos no pueda escribir, que los logs no roten, etc.

**4. Identificar qué disco está saturado con iostat:**
```bash
iostat -x 1 5
```
- `await` alto en el dispositivo lleno confirma que la lentitud viene de ese disco
- Verificar si el dispositivo con alto `%util` es el mismo que está lleno según df

**5. Encontrar qué ocupa espacio con du:**
```bash
du -h /MOUNTPOINT --max-depth=2 -x | sort -rh | head -10
```

**6. Verificar históricamente con sar:**
```bash
sar -d -s 00:00:00 -e 23:59:59  # ¿desde cuándo el disco está saturado?
sar -r                            # ¿la memoria bajó antes de que el disco se llenara?
```

La clave: disco lleno → aplicaciones fallan al escribir → retries → más I/O → `wa` sube → servidor lento. La causa raíz suele ser logs sin rotar, backups acumulados, o base de datos creciendo sin límite.

</details>
