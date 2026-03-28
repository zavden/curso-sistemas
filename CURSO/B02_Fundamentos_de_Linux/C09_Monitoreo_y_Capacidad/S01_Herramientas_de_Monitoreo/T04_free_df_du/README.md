# T04 — free, df, du

## free — Memoria

free muestra el uso de RAM y swap del sistema:

```bash
free
#               total        used        free      shared  buff/cache   available
# Mem:        8000000     2000000      500000      100000     5500000     5600000
# Swap:       2000000           0     2000000
```

### free -h — Formato legible

```bash
free -h
#               total        used        free      shared  buff/cache   available
# Mem:          7.6Gi       1.9Gi       488Mi        98Mi       5.2Gi       5.3Gi
# Swap:         1.9Gi          0B       1.9Gi
```

### Interpretar las columnas

```bash
# total     — RAM física total
# used      — RAM usada (total - free - buff/cache)
# free      — RAM completamente libre (no usada para nada)
# shared    — memoria compartida (tmpfs, shared memory)
# buff/cache — memoria usada como buffers y cache del kernel
# available — RAM disponible para nuevas aplicaciones
#             = free + buff/cache que se puede liberar

# IMPORTANTE: "free" bajo NO significa problema
# Linux usa la RAM libre como cache de disco
# Lo que importa es "available"

# Cuándo preocuparse:
# available < 10% de total → presión de memoria
# swap used > 0 con pswpout activo (sar -W) → RAM insuficiente
# available ≈ 0 → OOM killer puede activarse
```

### Opciones útiles

```bash
# Unidades específicas:
free -m     # en MiB
free -g     # en GiB
free -h     # human-readable (auto)
free --si   # usar potencias de 1000 (MB) en vez de 1024 (MiB)

# Monitoreo continuo:
free -h -s 2    # cada 2 segundos
free -h -s 2 -c 5   # cada 2 segundos, 5 veces

# Total (sumar RAM + swap):
free -h -t
#               total        used        free      shared  buff/cache   available
# Mem:          7.6Gi       1.9Gi       488Mi        98Mi       5.2Gi       5.3Gi
# Swap:         1.9Gi          0B       1.9Gi
# Total:        9.5Gi       1.9Gi       2.4Gi

# Wide output (separar buffers y cache):
free -w -h
#               total        used        free      shared     buffers       cache   available
# Mem:          7.6Gi       1.9Gi       488Mi        98Mi       125Mi       5.1Gi       5.3Gi
# Swap:         1.9Gi          0B       1.9Gi
```

### free en scripts

```bash
# Obtener la memoria disponible en MB:
free -m | awk '/^Mem:/{print $7}'
# 5300

# Alerta si la memoria disponible es menor al 10%:
TOTAL=$(free -m | awk '/^Mem:/{print $2}')
AVAIL=$(free -m | awk '/^Mem:/{print $7}')
PERCENT=$((AVAIL * 100 / TOTAL))
if [ "$PERCENT" -lt 10 ]; then
    echo "ALERTA: solo ${PERCENT}% de RAM disponible (${AVAIL}MB de ${TOTAL}MB)"
fi

# Verificar si hay swap activa:
SWAP_USED=$(free -m | awk '/^Swap:/{print $3}')
if [ "$SWAP_USED" -gt 0 ]; then
    echo "ADVERTENCIA: ${SWAP_USED}MB en swap"
fi
```

## df — Espacio en disco (filesystems)

df muestra el uso de espacio de cada filesystem montado:

```bash
df
# Filesystem     1K-blocks    Used Available Use% Mounted on
# /dev/sda1       50000000 20000000  27400000  43% /
# /dev/sda2      100000000 45000000  49800000  48% /home
# tmpfs            4000000    50000   3950000   2% /tmp
```

### df -h — Formato legible

```bash
df -h
# Filesystem      Size  Used Avail Use% Mounted on
# /dev/sda1        48G   20G   27G  43% /
# /dev/sda2        96G   43G   48G  48% /home
# tmpfs           3.9G   49M  3.8G   2% /tmp
# /dev/sdb1       932G  500G  385G  57% /data
```

### Opciones útiles

```bash
# Solo filesystems locales (excluir red y virtuales):
df -h -l
# Excluye NFS, CIFS, tmpfs, etc.

# Solo un tipo de filesystem:
df -h -t ext4       # solo ext4
df -h -t xfs        # solo xfs

# Excluir un tipo:
df -h -x tmpfs -x devtmpfs -x squashfs
# Excluir tmpfs, devtmpfs, squashfs (limpia la salida)

# Inodos (en lugar de espacio):
df -i
# Filesystem      Inodes  IUsed   IFree IUse% Mounted on
# /dev/sda1      3200000 150000 3050000    5% /

# Un filesystem puede tener espacio libre pero SIN inodos libres
# Esto pasa cuando hay millones de archivos pequeños
# Síntoma: "No space left on device" con df -h mostrando espacio libre

# Formato custom con --output:
df -h --output=source,size,used,avail,pcent,target
# Solo las columnas que quieres

# Filesystem de un archivo/directorio:
df -h /var/log
# Muestra el filesystem donde está /var/log

# Total:
df -h --total
# Agrega una línea de totales al final
```

### df en scripts

```bash
# Porcentaje de uso del filesystem raíz:
df / | awk 'NR==2{print $5}' | tr -d '%'
# 43

# Alerta si algún filesystem supera el 90%:
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

```bash
# A veces df muestra más espacio usado que du:
df -h /
# 20G used

du -sh /
# 18G

# Diferencia de 2GB — ¿dónde están?

# Causa 1 — Archivos eliminados pero aún abiertos:
# Si un proceso tiene un archivo abierto y se elimina,
# el espacio no se libera hasta que el proceso cierre el archivo
sudo lsof +L1 2>/dev/null | head -10
# Muestra archivos eliminados (link count 0) aún abiertos

# Causa 2 — Filesystems montados sobre directorios con contenido:
# Si /tmp tiene archivos y luego montas tmpfs sobre /tmp,
# los archivos originales siguen ocupando espacio pero son invisibles

# Causa 3 — Bloques reservados:
# ext4 reserva 5% del espacio para root por defecto
# tune2fs -l /dev/sda1 | grep "Reserved block count"
```

## du — Uso de disco (directorios)

du muestra cuánto espacio usa cada directorio y archivo:

```bash
# Resumen de un directorio:
du -sh /var/log
# 256M    /var/log

# Cada subdirectorio:
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
# Excluir archivos comprimidos

# -x = no cruzar filesystems:
du -shx /
# Solo el filesystem de /, no salta a /home si es otro filesystem

# --apparent-size = tamaño real (no bloques):
du -sh /var/log                    # tamaño en disco (bloques)
du -sh --apparent-size /var/log    # tamaño real de los datos
# Los archivos sparse pueden mostrar diferencias grandes
```

### Encontrar qué ocupa más espacio

```bash
# Top 10 directorios más grandes en /var:
du -h /var --max-depth=2 2>/dev/null | sort -rh | head -10
# 900M    /var
# 512M    /var/lib
# 300M    /var/lib/docker
# 256M    /var/log
# 128M    /var/log/journal
# ...

# Top 10 archivos más grandes en el sistema:
sudo find / -xdev -type f -exec du -sh {} + 2>/dev/null | sort -rh | head -10
# 2.1G    /var/lib/docker/overlay2/.../layer.tar
# 500M    /var/log/syslog.1
# ...

# Dentro de /home:
du -ah /home | sort -rh | head -20
```

### du en scripts

```bash
# Tamaño de un directorio en MB (sin la h):
du -sm /var/log | awk '{print $1}'
# 256

# Alerta si /var/log supera 1GB:
SIZE=$(du -sm /var/log 2>/dev/null | awk '{print $1}')
if [ "$SIZE" -gt 1024 ]; then
    echo "ALERTA: /var/log ocupa ${SIZE}MB"
fi

# Uso por usuario en /home:
sudo du -sh /home/*/ 2>/dev/null | sort -rh
# 5.2G    /home/dev/
# 2.1G    /home/admin/
# 500M    /home/user/
```

## ncdu — du interactivo

```bash
# ncdu es una alternativa interactiva a du:
sudo apt install ncdu    # Debian
sudo dnf install ncdu    # RHEL

# Explorar interactivamente:
ncdu /var
# Interfaz ncurses con:
# - Navegación por directorios
# - Ordenar por tamaño
# - Eliminar archivos/directorios
# - Vista de porcentaje y barras gráficas

# Escanear y guardar para análisis posterior:
ncdu -o /tmp/scan.json /var
ncdu -f /tmp/scan.json     # cargar sin re-escanear
```

## Combinación para diagnóstico

```bash
# "El disco está lleno" — flujo de diagnóstico:

# 1. ¿Qué filesystem está lleno?
df -h

# 2. ¿Qué directorio ocupa más?
du -h / --max-depth=1 -x | sort -rh | head -5

# 3. Profundizar en el directorio sospechoso:
du -h /var --max-depth=2 | sort -rh | head -10

# 4. ¿Hay archivos eliminados pero abiertos?
sudo lsof +L1 2>/dev/null | awk '{print $7, $9}' | sort -rn | head -5

# 5. ¿Hay problema de inodos?
df -i

# 6. Acciones inmediatas:
# - Limpiar logs: sudo journalctl --vacuum-size=200M
# - Limpiar cache de apt: sudo apt clean
# - Limpiar cache de dnf: sudo dnf clean all
# - Limpiar archivos temporales: sudo find /tmp -mtime +7 -delete
```

---

## Ejercicios

### Ejercicio 1 — Memoria

```bash
# 1. ¿Cuánta RAM tiene tu sistema?
free -h | awk '/^Mem:/{print $2}'

# 2. ¿Cuánta está disponible?
free -h | awk '/^Mem:/{print $7}'

# 3. ¿Hay swap en uso?
free -h | awk '/^Swap:/{print "Usada:", $3, "de", $2}'
```

### Ejercicio 2 — Disco

```bash
# 1. ¿Qué filesystem tiene más uso porcentual?
df -h --output=pcent,target | sort -rn | head -5

# 2. ¿Cuántos inodos libres tiene el filesystem raíz?
df -i / | awk 'NR==2{print "Libres:", $4, "(" $5 " usado)"}'

# 3. Top 5 directorios más grandes en /var:
du -h /var --max-depth=1 2>/dev/null | sort -rh | head -5
```

### Ejercicio 3 — Script de resumen

```bash
# Resumen rápido del sistema:
echo "=== Memoria ==="
free -h | grep -E "^Mem:|^Swap:"
echo "=== Disco ==="
df -h -x tmpfs -x devtmpfs -x squashfs 2>/dev/null
echo "=== Top 5 en / ==="
du -h / --max-depth=1 -x 2>/dev/null | sort -rh | head -5
```
