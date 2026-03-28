# T04 — free, df, du

> **Objetivo:** Dominar `free`, `df` y `du` para diagnosticar problemas de memoria y espacio en disco. Entender la diferencia entre memoria real vs disponible, y cómo encontrar qué ocupa espacio.

---

## free — Memoria RAM

```bash
free
#                total        used        free      shared  buff/cache   available
# Mem:        8000000     2000000      500000      100000     5500000     5600000
# Swap:       2000000           0     2000000
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
# Linux usa la RAM libre como cache de disco
# La columna "available" es la que importa

# Cuándo preocuparse:
#   available < 10% de total    → presión de memoria
#   swap used > 0 + pswpout activo → RAM insuficiente
#   available ≈ 0              → OOM killer puede activarse
```

### Opciones de free

```bash
# Formato legible:
free -h
#               total        used        free      shared  buff/cache   available
# Mem:          7.6Gi       1.9Gi       488Mi        98Mi       5.2Gi       5.3Gi
# Swap:         1.9Gi          0B       1.9Gi

# Unidades:
free -m      # MiB (1024)
free -g      # GiB (1024)
free --si    # MB/GB (potencias de 1000)

# Monitoreo continuo:
free -h -s 2        # cada 2 segundos
free -h -s 2 -c 5   # cada 2 segundos, 5 veces

# Wide output (separar buffers y cache):
free -w -h
#               total        used        free      shared     buffers       cache   available
# Mem:          7.6Gi       1.9Gi       488Mi        98Mi       125Mi       5.1Gi       5.3Gi

# Total (RAM + swap):
free -h -t
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
df
# Filesystem     1K-blocks    Used Available Use% Mounted on
# /dev/sda1       50000000 20000000  27400000  43% /
# /dev/sda2      100000000 45000000  49800000  48% /home
# tmpfs            4000000    50000   3950000   2% /tmp
```

### Opciones de df

```bash
# Formato legible:
df -h
# Filesystem      Size  Used Avail Use% Mounted on
# /dev/sda1        48G   20G   27G  43% /
# /dev/sdb1       932G  500G  385G  57% /data

# Solo filesystems locales (-l):
df -h -l

# Solo un tipo de filesystem:
df -h -t ext4
df -h -t xfs

# Excluir tipos:
df -h -x tmpfs -x devtmpfs -x squashfs
# Limpia la salida, solo filesystems reales

# Inodos (no espacio):
df -i
# Filesystem      Inodes  IUsed   IFree IUse% Mounted on
# /dev/sda1      3200000 150000 3050000    5% /

# ⚠️ Un filesystem puede tener ESPACIO libre pero SIN INODOS
# Síntoma: "No space left on device" con df -h mostrando espacio
# Causa: millones de archivos pequeños

# Formato custom:
df -h --output=source,size,used,avail,pcent,target

# Filesystem de un archivo/directorio:
df -h /var/log

# Agregar totales:
df -h --total
```

### df en scripts

```bash
# Porcentaje de uso del raíz:
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
```

### Discrepancia df vs du

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  PROBLEMA: df muestra MÁS espacio usado que du                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  $ df -h /                                                                 │
│  20G used                                                                │
│                                                                              │
│  $ du -sh /                                                              │
│  18G                                                                    │
│                                                                              │
│  → Diferencia de 2GB — ¿dónde están?                                    │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Causa 1 — Archivos eliminados pero aún abiertos:**
```bash
# Si un proceso tiene un archivo abierto y se elimina,
# el espacio NO se libera hasta que el proceso cierre el archivo
sudo lsof +L1 2>/dev/null | head -10
# Muestra archivos eliminados (link count 0) aún abiertos
```

**Causa 2 — Filesystems montados sobre directorios:**
```bash
# Si /tmp tiene archivos y luego montas tmpfs sobre /tmp,
# los archivos originales siguen ocupando espacio pero son invisibles
```

**Causa 3 — Bloques reservados:**
```bash
# ext4 reserva 5% del espacio para root por defecto
tune2fs -l /dev/sda1 | grep "Reserved"
# Reserved block count: xxxxx
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

# --max-depth=N = profundidad:
du -h --max-depth=1 /var
# 256M    /var/log
# 512M    /var/lib
# 100M    /var/cache
# 900M    /var

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

# -x = no cruzar filesystems:
du -shx /
# Solo el filesystem de /, no salta a /home si es otro disco

# --apparent-size = tamaño real (no bloques):
du -sh /var/log                # tamaño en disco (bloques)
du -sh --apparent-size /var/log  # tamaño real de los datos
# Archivos sparse pueden mostrar diferencias grandes
```

### Encontrar qué ocupa más espacio

```bash
# Top 10 directorios más grandes en /var:
du -h /var --max-depth=2 2>/dev/null | sort -rh | head -10

# Top 10 archivos más grandes (excluyendo虚拟):
sudo find / -xdev -type f -exec du -sh {} + 2>/dev/null | sort -rh | head -10
# 2.1G    /var/lib/docker/overlay2/.../layer.tar
# 500M    /var/log/syslog.1

# Dentro de /home:
du -ah /home | sort -rh | head -20

# Por usuario:
sudo du -sh /home/*/ 2>/dev/null | sort -rh
# 5.2G    /home/dev/
# 2.1G    /home/admin/
```

### du en scripts

```bash
# Tamaño en MB:
du -sm /var/log | awk '{print $1}'
# 256

# Alerta si /var/log > 1GB:
SIZE=$(du -sm /var/log 2>/dev/null | awk '{print $1}')
if [ "$SIZE" -gt 1024 ]; then
    echo "ALERTA: /var/log ocupa ${SIZE}MB"
fi

# Uso por usuario:
sudo du -sh /home/*/ 2>/dev/null | sort -rh
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
#   ↑↓       navegar
#   ENTER    entrar en directorio
#   d        eliminar
#   n        ordenar por nombre/tamaño
#   q        salir

# Escanear sin mostrar interfaz:
ncdu -o /tmp/scan.txt /var
# Guardar escaneo para después

# Cargar un escaneo guardado:
ncdu -f /tmp/scan.txt
```

---

## Diagnóstico: "El disco está lleno"

```bash
# PASO 1: ¿Qué filesystem está lleno?
df -h

# PASO 2: ¿Qué directorio ocupa más?
du -h / --max-depth=1 -x | sort -rh | head -5

# PASO 3: Profundizar en el sospechoso:
du -h /var --max-depth=2 | sort -rh | head -10

# PASO 4: ¿Archivos eliminados pero abiertos?
sudo lsof +L1 2>/dev/null | awk '{print $7, $9}' | sort -rn | head -10

# PASO 5: ¿Problema de inodos?
df -i

# PASO 6: Acciones inmediatas:
# Limpiar journal:
sudo journalctl --vacuum-size=200M

# Limpiar apt:
sudo apt clean

# Limpiar dnf:
sudo dnf clean all

# Limpiar tmp:
sudo find /tmp -mtime +7 -delete 2>/dev/null

# Encontrar archivos grandes:
sudo find / -xdev -type f -size +100M -exec ls -lh {} \; 2>/dev/null
```

---

## Quick reference

```
FREE:
  free -h              → formato legible
  free -m/-g          → MiB/GiB
  free -s N           → continuo cada N segundos
  free -w             → wide (separar buffers/cache)
  free -t             → total (RAM + swap)

DF:
  df -h               → legible
  df -i               → inodos
  df -l               → solo locales
  df -x tmpfs         → excluir virtuales
  df --output=...    → columnas custom
  df FILE             → filesystem de archivo

DU:
  du -sh DIR          → solo total
  du -h --max-depth=1  → un nivel
  du -ah              → incluir archivos
  du -shx /          → no cruzar filesystems
  du --exclude='*.gz' → excluir patrones

NCDU:
  ncdu DIR            → interactivo
  ncdu -o FILE       → guardar escaneo
```

---

## Ejercicios

### Ejercicio 1 — Memoria

```bash
# 1. RAM total y disponible:
echo "Total: $(free -h | awk '/^Mem:/{print $2}')"
echo "Disponible: $(free -h | awk '/^Mem:/{print $7}')"

# 2. ¿Hay swap activa?
free -h | awk '/^Swap:/{print "Swap usada:", $3, "de", $2}'

# 3. ¿Cuánto es buff/cache?
free -h | awk '/^Mem:/{print "Buffers/cache:", $6}'

# 4. Porcentaje de memoria disponible:
TOTAL=$(free --mega | awk '/^Mem:/{print $2}')
AVAIL=$(free --mega | awk '/^Mem:/{print $7}')
echo "Disponible: $((AVAIL * 100 / TOTAL))%"
```

### Ejercicio 2 — Espacio en disco

```bash
# 1. ¿Qué filesystem tiene más uso?
df -h --output=pcent,target | sort -rn | head -5

# 2. ¿Cuántos inodos libres tiene el raíz?
df -i / | awk 'NR==2{print "Inodos libres:", $3}'

# 3. ¿Qué directorios más pesan en /?
du -h / --max-depth=1 -x 2>/dev/null | sort -rh | head -5
```

### Ejercicio 3 — Encontrar archivos grandes

```bash
# 1. Top 10 archivos más grandes:
sudo find / -xdev -type f -exec du -sh {} + 2>/dev/null | sort -rh | head -10

# 2. Archivos > 100MB:
sudo find / -xdev -type f -size +100M -ls 2>/dev/null

# 3. ¿Cuánto ocupa /var/log?
du -sh /var/log
```

### Ejercicio 4 — Diagnóstico de discrepancia

```bash
# Simular el problema: df vs du diferente

# 1. Ver diferencia en /:
echo "df dice:"
df -h / | awk 'NR==2{print $3 " usado de " $2}'
echo "du dice:"
du -sh /

# 2. Buscar archivos eliminados pero abiertos:
sudo lsof +L1 2>/dev/null | head -5 || echo "No hay"

# 3. Ver bloques reservados:
tune2fs -l /dev/sda1 2>/dev/null | grep -i reserved
```

### Ejercicio 5 — Script de resumen del sistema

```bash
cat << 'EOF' > /usr/local/bin/disk-mem-report.sh
#!/bin/bash
echo "=========================================="
echo "       RESUMEN DE SISTEMA"
echo "=========================================="
echo ""
echo "--- MEMORIA ---"
free -h | grep -E "^Mem:|^Swap:"
echo ""
echo "--- ESPACIO (>80% uso) ---"
df -h --output=pcent,target 2>/dev/null | tail -n +2 | \
    while read -r pcent target; do
        USE=$(echo "$pcent" | tr -d '% ')
        if [ "$USE" -gt 80 ]; then
            echo "⚠️  ${target} al ${pcent}"
        fi
    done
echo ""
echo "--- TOP 5 DIRECTORIOS ---"
du -h / --max-depth=1 -x 2>/dev/null | sort -rh | head -5
echo ""
echo "--- TOP 5 ARCHIVOS GRANDES ---"
sudo find / -xdev -type f -exec du -sh {} + 2>/dev/null | sort -rh | head -5
echo ""
echo "=========================================="
EOF
chmod +x /usr/local/bin/disk-mem-report.sh
/usr/local/bin/disk-mem-report.sh
```
