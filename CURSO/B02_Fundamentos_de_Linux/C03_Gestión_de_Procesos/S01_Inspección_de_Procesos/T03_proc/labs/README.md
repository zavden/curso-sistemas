# Lab — /proc/[pid]

## Objetivo

Explorar el filesystem virtual /proc para obtener informacion
detallada de procesos: campos de status, file descriptors, mapeo
de memoria, limites, y namespaces.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Archivos basicos de /proc/[pid]

### Objetivo

Explorar los archivos principales que el kernel expone para cada
proceso.

### Paso 1.1: Que hay en /proc/self

```bash
docker compose exec debian-dev bash -c '
echo "=== /proc/self es un enlace al proceso actual ==="
echo "PID del shell: $$"
ls /proc/self/ | head -20
echo "..."
echo ""
echo "Total de entradas: $(ls /proc/self/ | wc -l)"
echo ""
echo "/proc/self siempre apunta al proceso que lo lee"
echo "/proc/\$\$ apunta al shell actual"
'
```

`/proc/self` es un enlace magico que siempre apunta al proceso
que esta leyendo. Cada entrada es un archivo virtual que el kernel
genera al momento de la lectura.

### Paso 1.2: status — informacion completa

```bash
docker compose exec debian-dev bash -c '
echo "=== /proc/self/status ==="
cat /proc/self/status
'
```

```bash
docker compose exec debian-dev bash -c '
echo "=== Campos clave de status ==="
echo ""
echo "--- Identidad ---"
grep -E "^(Name|Pid|PPid|Tgid|Uid|Gid):" /proc/self/status
echo ""
echo "Name  — nombre del ejecutable"
echo "Pid   — PID del proceso"
echo "PPid  — PID del padre"
echo "Tgid  — Thread Group ID (== PID para el thread principal)"
echo "Uid   — real, effective, saved, filesystem UIDs"
echo "Gid   — real, effective, saved, filesystem GIDs"

echo ""
echo "--- Memoria ---"
grep -E "^(VmPeak|VmSize|VmRSS|VmData|VmStk|VmLib):" /proc/self/status
echo ""
echo "VmPeak — pico de memoria virtual"
echo "VmSize — memoria virtual actual (== VSZ de ps)"
echo "VmRSS  — memoria fisica usada (== RSS de ps)"
echo "VmData — heap"
echo "VmStk  — stack"
echo "VmLib  — librerias compartidas"

echo ""
echo "--- Threads y contexto ---"
grep -E "^(Threads|voluntary_ctxt|nonvoluntary_ctxt):" /proc/self/status
echo ""
echo "Threads — numero de threads"
echo "voluntary_ctxt_switches   — cedio CPU voluntariamente (I/O)"
echo "nonvoluntary_ctxt_switches — fue desalojado por el scheduler"
'
```

Los context switches voluntarios altos indican un proceso I/O-bound.
Los involuntarios altos indican un proceso CPU-bound que compite
por CPU.

### Paso 1.3: cmdline y comm

```bash
docker compose exec debian-dev bash -c '
echo "=== /proc/self/cmdline ==="
cat /proc/self/cmdline | tr "\0" " "
echo ""
echo "(los argumentos estan separados por null bytes)"

echo ""
echo "=== /proc/self/comm ==="
cat /proc/self/comm
echo "(solo el nombre del ejecutable, sin ruta ni argumentos)"
echo "(maximo 16 caracteres — truncado si es mas largo)"
'
```

### Paso 1.4: environ

```bash
docker compose exec debian-dev bash -c '
echo "=== /proc/self/environ (primeras variables) ==="
cat /proc/self/environ | tr "\0" "\n" | head -10
echo "..."
echo ""
echo "Muestra las variables de entorno del proceso"
echo "Separadas por null bytes (como cmdline)"
echo ""
echo "=== Buscar una variable especifica ==="
cat /proc/self/environ | tr "\0" "\n" | grep "^PATH="
'
```

### Paso 1.5: stat — datos crudos

```bash
docker compose exec debian-dev bash -c '
echo "=== /proc/self/stat (una linea con todos los datos) ==="
cat /proc/self/stat
echo ""
echo ""
echo "Campos relevantes (por posicion):"
echo "  1: PID"
echo "  2: (nombre) entre parentesis"
echo "  3: estado (R, S, D, Z, T)"
echo "  4: PPID"
echo "  5: process group ID"
echo "  6: session ID"
echo "  7: tty"
echo " 14: utime (tiempo en user mode, en clock ticks)"
echo " 15: stime (tiempo en kernel mode)"
echo " 18: nice"
echo " 20: num_threads"
echo ""
echo "Este archivo es para herramientas (top, ps), no para humanos"
echo "Usar status para lectura humana"
'
```

---

## Parte 2 — File descriptors y memoria

### Objetivo

Inspeccionar los file descriptors abiertos y el mapa de memoria
de un proceso.

### Paso 2.1: File descriptors (fd/)

```bash
docker compose exec debian-dev bash -c '
echo "=== File descriptors del shell ==="
ls -la /proc/self/fd/
echo ""
echo "0 → stdin"
echo "1 → stdout"
echo "2 → stderr"
echo "Los demas son archivos, sockets, pipes abiertos"
'
```

### Paso 2.2: Detectar file descriptors abiertos

```bash
docker compose exec debian-dev bash -c '
echo "=== Abrir archivos y ver los fds ==="
exec 3>/tmp/test-fd.txt
exec 4</etc/passwd

ls -la /proc/self/fd/
echo ""
echo "fd 3 apunta a /tmp/test-fd.txt (escritura)"
echo "fd 4 apunta a /etc/passwd (lectura)"

echo ""
echo "=== Contar fds abiertos ==="
echo "Este proceso: $(ls /proc/self/fd/ | wc -l) fds"

exec 3>&-
exec 4<&-
rm -f /tmp/test-fd.txt
'
```

Un proceso con muchos file descriptors abiertos que nunca cierra
puede tener un fd leak, similar a un memory leak.

### Paso 2.3: Limites de fds

```bash
docker compose exec debian-dev bash -c '
echo "=== /proc/self/limits ==="
cat /proc/self/limits
echo ""
echo "=== Limite de archivos abiertos ==="
grep "open files" /proc/self/limits
echo ""
echo "Soft Limit: el limite actual (puede subirse hasta Hard)"
echo "Hard Limit: el maximo absoluto (solo root puede subirlo)"
echo ""
echo "=== ulimit equivalente ==="
echo "Soft: $(ulimit -Sn)"
echo "Hard: $(ulimit -Hn)"
'
```

### Paso 2.4: Mapa de memoria (maps)

```bash
docker compose exec debian-dev bash -c '
echo "=== /proc/self/maps (regiones de memoria) ==="
head -20 /proc/self/maps
echo "..."
echo ""
echo "Formato: inicio-fin permisos offset dev inode path"
echo ""
echo "Permisos:"
echo "  r = lectura"
echo "  w = escritura"
echo "  x = ejecucion"
echo "  p = privada (copy-on-write)"
echo "  s = compartida"

echo ""
echo "=== Regiones clave ==="
echo "--- Codigo del ejecutable (r-xp) ---"
grep "bash" /proc/self/maps | grep "r-xp" | head -2
echo ""
echo "--- Heap ---"
grep "\\[heap\\]" /proc/self/maps
echo ""
echo "--- Stack ---"
grep "\\[stack\\]" /proc/self/maps
echo ""
echo "--- Librerias compartidas ---"
grep "lib" /proc/self/maps | head -5
'
```

Las regiones `r-xp` son codigo ejecutable. Las regiones `rw-p` son
datos modificables (heap, stack, variables globales).

### Paso 2.5: Resumen de memoria (smaps_rollup)

```bash
docker compose exec debian-dev bash -c '
echo "=== /proc/self/smaps_rollup ==="
cat /proc/self/smaps_rollup 2>/dev/null || echo "(no disponible en este kernel)"
echo ""
echo "Campos clave:"
echo "  Rss           — memoria fisica total"
echo "  Pss           — Proportional Set Size (memoria compartida dividida)"
echo "  Shared_Clean  — paginas compartidas no modificadas"
echo "  Shared_Dirty  — paginas compartidas modificadas"
echo "  Private_Clean — paginas privadas no modificadas"
echo "  Private_Dirty — paginas privadas modificadas (la mas importante)"
echo ""
echo "PSS es mejor que RSS para medir el uso real de un proceso"
echo "porque divide la memoria compartida proporcionalmente"
'
```

---

## Parte 3 — Enlaces y metadatos

### Objetivo

Explorar los enlaces simbolicos y datos de namespace en /proc.

### Paso 3.1: exe, cwd, root

```bash
docker compose exec debian-dev bash -c '
echo "=== /proc/self/exe — ejecutable ==="
readlink /proc/self/exe
echo "(ruta completa del binario en ejecucion)"

echo ""
echo "=== /proc/self/cwd — directorio de trabajo ==="
readlink /proc/self/cwd
echo "(equivalente a pwd)"

echo ""
echo "=== /proc/self/root — raiz del filesystem ==="
readlink /proc/self/root
echo "(/ en condiciones normales, diferente si hay chroot)"
'
```

### Paso 3.2: Inspeccionar otro proceso

```bash
docker compose exec debian-dev bash -c '
echo "=== Inspeccionar PID 1 ==="
echo "Ejecutable: $(readlink /proc/1/exe 2>/dev/null || echo "sin permiso")"
echo "CWD:        $(readlink /proc/1/cwd 2>/dev/null || echo "sin permiso")"
echo "Comando:    $(cat /proc/1/cmdline 2>/dev/null | tr "\0" " ")"
echo ""
cat /proc/1/status 2>/dev/null | grep -E "^(Name|Pid|PPid|Uid|Threads):"
'
```

### Paso 3.3: Namespaces (ns/)

```bash
docker compose exec debian-dev bash -c '
echo "=== /proc/self/ns/ — namespaces del proceso ==="
ls -la /proc/self/ns/
echo ""
echo "Cada enlace apunta a un namespace identificado por inode"
echo "Dos procesos con el mismo inode estan en el mismo namespace"

echo ""
echo "=== Comparar con PID 1 ==="
echo "--- Shell ---"
ls -la /proc/self/ns/ | awk "{print \$NF}" | sort
echo ""
echo "--- PID 1 ---"
ls -la /proc/1/ns/ 2>/dev/null | awk "{print \$NF}" | sort
echo ""
echo "Si los numeros son iguales, estan en el mismo namespace"
'
```

### Paso 3.4: cgroup

```bash
docker compose exec debian-dev bash -c '
echo "=== /proc/self/cgroup ==="
cat /proc/self/cgroup
echo ""
echo "Formato: jerarquia:controlador:ruta"
echo "En cgroups v2: 0::ruta"
echo ""
echo "=== Comparar shell vs PID 1 ==="
echo "Shell:  $(cat /proc/self/cgroup)"
echo "PID 1:  $(cat /proc/1/cgroup 2>/dev/null)"
'
```

### Paso 3.5: Comparar Debian vs AlmaLinux

```bash
docker compose exec debian-dev bash -c '
echo "=== Debian: campos de /proc/self/status ==="
wc -l < /proc/self/status
echo "lineas en status"
echo ""
grep "^Name:" /proc/self/status
grep "^Threads:" /proc/self/status
echo ""
echo "=== /proc/version ==="
cat /proc/version
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== AlmaLinux: campos de /proc/self/status ==="
wc -l < /proc/self/status
echo "lineas en status"
echo ""
grep "^Name:" /proc/self/status
grep "^Threads:" /proc/self/status
echo ""
echo "=== /proc/version ==="
cat /proc/version
'
```

Ambos contenedores comparten el kernel del host, asi que
/proc/version es identico. Las diferencias estan en el
espacio de usuario, no en /proc.

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. `/proc/[pid]/status` contiene toda la informacion del proceso
   en formato legible: identidad (4 UIDs/GIDs), memoria (VmRSS,
   VmSize), threads, y context switches.

2. `/proc/[pid]/fd/` lista los file descriptors abiertos. 0=stdin,
   1=stdout, 2=stderr. Un numero excesivo puede indicar fd leak.

3. `/proc/[pid]/maps` muestra las regiones de memoria: codigo
   (r-xp), heap, stack, librerias compartidas. PSS (en smaps)
   es mejor que RSS para medir uso real.

4. `/proc/[pid]/limits` muestra los limites soft y hard del
   proceso. El soft se puede subir hasta el hard; solo root
   puede subir el hard.

5. `/proc/[pid]/ns/` expone los namespaces del proceso como
   inodes. Dos procesos con el mismo inode comparten ese
   namespace.

6. `/proc/self` siempre apunta al proceso que lo lee. Es un
   alias que el kernel resuelve dinamicamente.
