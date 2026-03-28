# Lab — free, df, du

## Objetivo

Usar free, df y du para diagnostico de memoria y disco: free
(columnas, available como metrica clave, buff/cache, swap,
opciones -h/-w/-s, scripts de alerta), df (uso por filesystem,
inodos, exclusion de tipos, discrepancia df vs du), du (por
directorio, --max-depth, top consumidores, -x para no cruzar
filesystems), y flujo completo de diagnostico de disco lleno.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — free: memoria

### Objetivo

Usar free para evaluar el uso de memoria y detectar problemas.

### Paso 1.1: Salida basica

```bash
docker compose exec debian-dev bash -c '
echo "=== free ==="
echo ""
free
echo ""
echo "--- Columnas ---"
echo "  total     = RAM fisica total"
echo "  used      = RAM usada (total - free - buff/cache)"
echo "  free      = RAM completamente libre"
echo "  shared    = memoria compartida (tmpfs, shared memory)"
echo "  buff/cache = buffers + cache del kernel"
echo "  available = RAM disponible para nuevas aplicaciones"
echo ""
echo "IMPORTANTE: free bajo NO es problema."
echo "Linux usa la RAM libre como cache de disco."
echo "Lo que importa es AVAILABLE."
'
```

### Paso 1.2: Formato legible (-h)

```bash
docker compose exec debian-dev bash -c '
echo "=== free -h ==="
echo ""
free -h
echo ""
echo "--- Que mirar ---"
TOTAL=$(free -m | awk "/^Mem:/{print \$2}")
AVAIL=$(free -m | awk "/^Mem:/{print \$7}")
SWAP=$(free -m | awk "/^Swap:/{print \$3}")
PCT=$((AVAIL * 100 / TOTAL))
echo "  Total: ${TOTAL}MB"
echo "  Disponible: ${AVAIL}MB (${PCT}%)"
echo "  Swap usada: ${SWAP}MB"
echo ""
if [ "$PCT" -lt 10 ]; then
    echo "  ALERTA: menos del 10% disponible"
elif [ "$PCT" -lt 25 ]; then
    echo "  NOTA: memoria moderadamente usada"
else
    echo "  OK: suficiente memoria disponible"
fi
if [ "$SWAP" -gt 0 ]; then
    echo "  NOTA: hay ${SWAP}MB en swap"
fi
'
```

### Paso 1.3: Wide y monitoreo

```bash
docker compose exec debian-dev bash -c '
echo "=== free -w (wide — separar buffers y cache) ==="
echo ""
free -w -h
echo ""
echo "La opcion -w separa buffers y cache en columnas distintas."
echo "  buffers = metadata de filesystem"
echo "  cache   = page cache (archivos leidos)"
echo ""
echo "=== free -s (monitoreo continuo) ==="
echo ""
echo "free -h -s 2     cada 2 segundos"
echo "free -h -s 2 -c 5  cada 2s, 5 veces"
echo ""
echo "--- 3 muestras ---"
free -h -s 1 -c 3 2>/dev/null || free -h
echo ""
echo "=== free -t (total RAM + swap) ==="
free -h -t 2>/dev/null || free -h
'
```

### Paso 1.4: free en scripts

```bash
docker compose exec debian-dev bash -c '
echo "=== free en scripts ==="
echo ""
echo "--- Memoria disponible en MB ---"
AVAIL=$(free -m | awk "/^Mem:/{print \$7}")
echo "  Disponible: ${AVAIL}MB"
echo ""
echo "--- Alerta si < 10% ---"
TOTAL=$(free -m | awk "/^Mem:/{print \$2}")
AVAIL=$(free -m | awk "/^Mem:/{print \$7}")
PERCENT=$((AVAIL * 100 / TOTAL))
echo "  RAM: ${AVAIL}MB de ${TOTAL}MB (${PERCENT}% disponible)"
if [ "$PERCENT" -lt 10 ]; then
    echo "  ALERTA: solo ${PERCENT}% disponible"
else
    echo "  OK"
fi
echo ""
echo "--- Verificar swap ---"
SWAP_USED=$(free -m | awk "/^Swap:/{print \$3}")
echo "  Swap usada: ${SWAP_USED}MB"
if [ "$SWAP_USED" -gt 0 ]; then
    echo "  ADVERTENCIA: hay swap activa"
else
    echo "  OK: sin swap"
fi
'
```

---

## Parte 2 — df: filesystems

### Objetivo

Usar df para evaluar el uso de espacio en filesystems.

### Paso 2.1: Salida basica

```bash
docker compose exec debian-dev bash -c '
echo "=== df -h ==="
echo ""
df -h
echo ""
echo "--- Columnas ---"
echo "  Filesystem = dispositivo o fuente"
echo "  Size       = tamano total"
echo "  Used       = espacio usado"
echo "  Avail      = espacio disponible"
echo "  Use%       = porcentaje de uso"
echo "  Mounted on = punto de montaje"
'
```

### Paso 2.2: Filtrar y excluir

```bash
docker compose exec debian-dev bash -c '
echo "=== Filtrar filesystems ==="
echo ""
echo "--- Solo locales (sin red ni virtuales) ---"
df -h -l 2>/dev/null || df -h
echo ""
echo "--- Excluir tipos ---"
echo "df -h -x tmpfs -x devtmpfs -x squashfs"
df -h -x tmpfs -x devtmpfs -x squashfs 2>/dev/null || df -h -x tmpfs
echo ""
echo "--- Solo un tipo ---"
echo "df -h -t ext4    (solo ext4)"
echo "df -h -t xfs     (solo xfs)"
echo ""
echo "--- De un directorio especifico ---"
df -h /var/log
echo "(muestra el filesystem donde esta /var/log)"
'
```

### Paso 2.3: Inodos

```bash
docker compose exec debian-dev bash -c '
echo "=== df -i (inodos) ==="
echo ""
df -i 2>/dev/null | head -10
echo ""
echo "--- Que son los inodos ---"
echo "  Cada archivo ocupa un inodo (metadata: permisos, owner, etc.)"
echo "  Un filesystem tiene un numero fijo de inodos"
echo ""
echo "--- Problema: disco lleno sin estar lleno ---"
echo "  Si los inodos se agotan, no puedes crear archivos"
echo "  aunque haya espacio libre"
echo "  Sintoma: \"No space left on device\" con df -h mostrando espacio"
echo "  Causa: millones de archivos pequenos"
echo ""
echo "--- Verificar ---"
df -i / | awk "NR==2{printf \"  Filesystem raiz: %s inodos usados (%s)\n\", \$3, \$5}"
'
```

### Paso 2.4: Opciones avanzadas

```bash
docker compose exec debian-dev bash -c '
echo "=== Opciones avanzadas de df ==="
echo ""
echo "--- --output (columnas personalizadas) ---"
df -h --output=source,size,used,avail,pcent,target 2>/dev/null | head -10
echo ""
echo "--- --total (sumar todo) ---"
df -h --total 2>/dev/null | tail -3
echo ""
echo "--- En scripts ---"
echo ""
echo "Porcentaje de uso de /:"
USE=$(df / | awk "NR==2{print \$5}" | tr -d "%")
echo "  / esta al ${USE}%"
echo ""
echo "Alerta si alguno supera 90%:"
df -h --output=pcent,target 2>/dev/null | tail -n +2 | while read -r pcent target; do
    PVAL=$(echo "$pcent" | tr -d "% ")
    if [ "$PVAL" -gt 90 ] 2>/dev/null; then
        echo "  ALERTA: $target al ${PVAL}%"
    fi
done
echo "  (verificacion completada)"
'
```

### Paso 2.5: Discrepancia df vs du

```bash
docker compose exec debian-dev bash -c '
echo "=== Discrepancia df vs du ==="
echo ""
echo "A veces df muestra mas espacio usado que du:"
echo ""
echo "--- Causas ---"
echo ""
echo "1. Archivos eliminados pero aun abiertos"
echo "   Un proceso tiene un archivo abierto y se elimino"
echo "   El espacio no se libera hasta que el proceso lo cierre"
echo "   Verificar: lsof +L1"
lsof +L1 2>/dev/null | head -5 || echo "   (requiere lsof)"
echo ""
echo "2. Filesystems montados sobre directorios con contenido"
echo "   Los archivos originales siguen ocupando espacio"
echo "   pero son invisibles"
echo ""
echo "3. Bloques reservados (ext4)"
echo "   ext4 reserva 5% para root por defecto"
echo "   tune2fs -l /dev/sda1 | grep \"Reserved block count\""
'
```

---

## Parte 3 — du: directorios

### Objetivo

Usar du para encontrar que ocupa mas espacio y diagnosticar disco lleno.

### Paso 3.1: Uso basico

```bash
docker compose exec debian-dev bash -c '
echo "=== du ==="
echo ""
echo "--- Resumen de un directorio (-sh) ---"
du -sh /var/log 2>/dev/null
echo ""
echo "--- Cada subdirectorio (--max-depth=1) ---"
du -h /var/log --max-depth=1 2>/dev/null | sort -rh | head -10
echo ""
echo "--- Solo el tamano de archivos (-a) ---"
du -ah /var/log 2>/dev/null | sort -rh | head -10
'
```

### Paso 3.2: Opciones clave

```bash
docker compose exec debian-dev bash -c '
echo "=== Opciones de du ==="
echo ""
echo "--- -s (summary) ---"
du -sh /var /etc /tmp 2>/dev/null
echo ""
echo "--- -c (total al final) ---"
du -shc /var /etc /tmp 2>/dev/null
echo ""
echo "--- --max-depth=N ---"
echo "Nivel 1:"
du -h /var --max-depth=1 2>/dev/null | sort -rh | head -5
echo ""
echo "Nivel 2:"
du -h /var --max-depth=2 2>/dev/null | sort -rh | head -10
echo ""
echo "--- -x (no cruzar filesystems) ---"
echo "du -shx /   solo el filesystem de /, sin saltar a /home"
echo "            si /home es otro filesystem"
echo ""
echo "--- --exclude (excluir patron) ---"
echo "du -sh --exclude=\"*.gz\" /var/log"
du -sh --exclude="*.gz" /var/log 2>/dev/null
'
```

### Paso 3.3: Top consumidores

```bash
docker compose exec debian-dev bash -c '
echo "=== Top consumidores ==="
echo ""
echo "--- Top 10 directorios en /var ---"
du -h /var --max-depth=2 2>/dev/null | sort -rh | head -10
echo ""
echo "--- Top 10 en / (primer nivel) ---"
du -h / --max-depth=1 -x 2>/dev/null | sort -rh | head -10
echo ""
echo "--- Top 10 archivos en /var/log ---"
du -ah /var/log 2>/dev/null | sort -rh | head -10
'
```

### Paso 3.4: Flujo de diagnostico de disco lleno

```bash
docker compose exec debian-dev bash -c '
echo "=== Flujo: disco lleno ==="
echo ""
echo "1. Que filesystem esta lleno?"
df -h -x tmpfs -x devtmpfs 2>/dev/null || df -h
echo ""
echo "2. Que directorio ocupa mas?"
du -h / --max-depth=1 -x 2>/dev/null | sort -rh | head -5
echo ""
echo "3. Profundizar en el sospechoso:"
BIGGEST=$(du / --max-depth=1 -x 2>/dev/null | sort -rn | head -2 | tail -1 | awk "{print \$2}")
if [[ -n "$BIGGEST" && "$BIGGEST" != "/" ]]; then
    echo "Explorando $BIGGEST:"
    du -h "$BIGGEST" --max-depth=1 2>/dev/null | sort -rh | head -5
fi
echo ""
echo "4. Inodos?"
df -i / | awk "NR==2{printf \"  Inodos: %s usados (%s)\n\", \$3, \$5}"
echo ""
echo "5. Archivos eliminados pero abiertos?"
lsof +L1 2>/dev/null | wc -l | xargs -I{} echo "  {} archivos eliminados abiertos"
echo ""
echo "6. Acciones inmediatas:"
echo "  - Limpiar logs: journalctl --vacuum-size=200M"
echo "  - Limpiar cache apt: apt clean"
echo "  - Limpiar /tmp: find /tmp -mtime +7 -delete"
'
```

### Paso 3.5: Resumen de las tres herramientas

```bash
docker compose exec debian-dev bash -c '
echo "=== Resumen ==="
echo ""
echo "| Herramienta | Que mide | Uso principal |"
echo "|-------------|----------|---------------|"
echo "| free        | RAM y swap | Memoria disponible, swap |"
echo "| df          | Espacio por filesystem | Que disco esta lleno |"
echo "| du          | Espacio por directorio | Que ocupa mas espacio |"
echo ""
echo "--- Metricas clave ---"
echo "  free: available (no free)"
echo "  df:   Use% y df -i para inodos"
echo "  du:   du -sh + sort -rh para top consumidores"
echo ""
echo "--- Estado actual ---"
echo ""
echo "Memoria:"
free -h | grep "^Mem:" | awk "{printf \"  Total: %s  Disponible: %s\n\", \$2, \$7}"
echo ""
echo "Disco:"
df -h / | awk "NR==2{printf \"  /: %s usado de %s (%s)\n\", \$3, \$2, \$5}"
echo ""
echo "Top 3 en /var:"
du -h /var --max-depth=1 2>/dev/null | sort -rh | head -3
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. free: la columna available es la referencia correcta,
   no free. Linux usa RAM libre como cache. buff/cache
   se liberan automaticamente bajo presion.

2. df muestra uso por filesystem. df -i muestra inodos.
   Un filesystem puede tener espacio pero sin inodos
   libres (muchos archivos pequenos).

3. df puede mostrar mas uso que du por archivos eliminados
   pero abiertos, mounts sobre directorios con contenido,
   o bloques reservados de ext4.

4. du -h --max-depth=N + sort -rh encuentra los mayores
   consumidores. -x evita cruzar filesystems.

5. Flujo de diagnostico: df (que filesystem) → du (que
   directorio) → du -a (que archivo) → df -i (inodos)
   → lsof +L1 (archivos eliminados abiertos).

6. En scripts: free -m + awk para memoria, df + awk para
   disco. Alertar si available < 10% o Use% > 90%.
