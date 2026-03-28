# Lab — /var en profundidad

## Objetivo

Explorar /var y sus subdirectorios, entender la diferencia entre datos
variables (logs, cache, estado), comparar los archivos de log entre
Debian y AlmaLinux, y distinguir /tmp de /var/tmp.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Estructura de /var

### Objetivo

Explorar los subdirectorios de /var y entender el proposito de cada uno.

### Paso 1.1: Subdirectorios principales

```bash
docker compose exec debian-dev bash -c '
echo "=== Contenido de /var ==="
ls -la /var/

echo ""
echo "=== Tamano por subdirectorio ==="
du -sh /var/*/ 2>/dev/null | sort -rh
'
```

Predice: cual subdirectorio sera el mas grande?

Normalmente `/var/lib` (bases de datos de paquetes, estado del
sistema) o `/var/cache` (paquetes descargados).

### Paso 1.2: /var/run es un symlink

```bash
docker compose exec debian-dev bash -c '
ls -la /var/run
echo ""
echo "Apunta a: $(readlink /var/run)"
'
```

En distribuciones modernas, `/var/run` es un symlink a `/run` (tmpfs).
Los datos de runtime se necesitan desde el arranque temprano, antes
de que /var pueda estar montado.

### Paso 1.3: Proposito de cada subdirectorio

```bash
docker compose exec debian-dev bash -c '
echo "/var/log   — $(ls /var/log/ | wc -l) archivos (logs del sistema)"
echo "/var/cache — $(ls /var/cache/ 2>/dev/null | wc -l) dirs (caches de aplicaciones)"
echo "/var/lib   — $(ls /var/lib/ 2>/dev/null | wc -l) dirs (estado persistente)"
echo "/var/spool — $(ls /var/spool/ 2>/dev/null | wc -l) dirs (colas de trabajo)"
echo "/var/tmp   — archivos temporales persistentes"
'
```

---

## Parte 2 — /var/log

### Objetivo

Explorar los archivos de log y comparar entre distribuciones.

### Paso 2.1: Logs en Debian

```bash
docker compose exec debian-dev bash -c '
echo "=== Archivos de log en Debian ==="
ls -la /var/log/
'
```

### Paso 2.2: Logs en AlmaLinux

```bash
docker compose exec alma-dev bash -c '
echo "=== Archivos de log en AlmaLinux ==="
ls -la /var/log/
'
```

### Paso 2.3: Diferencias clave

```bash
echo "=== Debian: log de autenticacion ==="
docker compose exec debian-dev bash -c 'ls /var/log/auth.log 2>/dev/null && echo "auth.log: presente" || echo "auth.log: NO existe"'

echo ""
echo "=== AlmaLinux: log de autenticacion ==="
docker compose exec alma-dev bash -c 'ls /var/log/secure 2>/dev/null && echo "secure: presente" || echo "secure: NO existe"'
```

Debian usa `auth.log`, AlmaLinux usa `secure`. Mismo contenido,
diferente nombre. `journalctl` funciona en ambos como alternativa
portable.

### Paso 2.4: Logs del gestor de paquetes

```bash
echo "=== Debian: dpkg.log ==="
docker compose exec debian-dev bash -c 'ls -la /var/log/dpkg.log 2>/dev/null || echo "no existe"'

echo ""
echo "=== AlmaLinux: dnf.log ==="
docker compose exec alma-dev bash -c 'ls -la /var/log/dnf.log 2>/dev/null || echo "no existe"'
```

### Paso 2.5: Permisos de logs

```bash
docker compose exec debian-dev bash -c '
echo "=== Permisos de logs sensibles ==="
ls -la /var/log/auth.log 2>/dev/null
ls -la /var/log/btmp 2>/dev/null
'
```

Los logs de autenticacion tienen permisos restrictivos. Solo root (o
el grupo syslog/adm) puede leerlos.

---

## Parte 3 — /var/cache y /var/lib

### Objetivo

Distinguir entre cache (regenerable) y estado persistente (critico).

### Paso 3.1: Cache en Debian

```bash
docker compose exec debian-dev bash -c '
echo "=== /var/cache ==="
ls /var/cache/
echo ""
echo "=== Tamano del cache de apt ==="
du -sh /var/cache/apt/ 2>/dev/null || echo "sin cache (limpiado en Dockerfile)"
'
```

En la imagen Docker, el cache de apt fue limpiado con
`rm -rf /var/lib/apt/lists/*`. Si estuviera presente, contendria
paquetes descargados que se pueden regenerar con `apt-get update`.

### Paso 3.2: Cache en AlmaLinux

```bash
docker compose exec alma-dev bash -c '
echo "=== /var/cache ==="
ls /var/cache/
echo ""
echo "=== Tamano del cache de dnf ==="
du -sh /var/cache/dnf/ 2>/dev/null || echo "sin cache (limpiado en Dockerfile)"
'
```

### Paso 3.3: Estado persistente

```bash
docker compose exec debian-dev bash -c '
echo "=== /var/lib ==="
ls /var/lib/

echo ""
echo "=== Base de datos de dpkg ==="
ls -la /var/lib/dpkg/status
wc -l /var/lib/dpkg/status
echo "lineas en la DB de paquetes"
'
```

`/var/lib/dpkg/status` es la base de datos de paquetes instalados.
Si se corrompe, `apt` deja de funcionar. **No es regenerable** — a
diferencia del cache.

### Paso 3.4: Equivalente en AlmaLinux

```bash
docker compose exec alma-dev bash -c '
echo "=== Base de datos de rpm ==="
ls -la /var/lib/rpm/
echo ""
echo "Paquetes instalados: $(rpm -qa | wc -l)"
'
```

---

## Parte 4 — /tmp vs /var/tmp

### Objetivo

Demostrar la diferencia practica entre /tmp (volatil) y /var/tmp
(persistente).

### Paso 4.1: Permisos

```bash
docker compose exec debian-dev bash -c '
echo "=== /tmp ==="
ls -ld /tmp
stat -c "Permisos: %a  Sticky: %A" /tmp

echo ""
echo "=== /var/tmp ==="
ls -ld /var/tmp
stat -c "Permisos: %a  Sticky: %A" /var/tmp
'
```

Ambos tienen sticky bit (1777). La diferencia no esta en los permisos
sino en la **politica de limpieza**.

### Paso 4.2: Crear archivos en ambos

```bash
docker compose exec debian-dev bash -c '
echo "temporal volatil" > /tmp/test-tmp.txt
echo "temporal persistente" > /var/tmp/test-vartmp.txt
ls -la /tmp/test-tmp.txt /var/tmp/test-vartmp.txt
'
```

### Paso 4.3: Tipo de filesystem

```bash
docker compose exec debian-dev bash -c '
echo "=== /tmp ==="
df -h /tmp

echo ""
echo "=== /var/tmp ==="
df -h /var/tmp
'
```

En el host, `/tmp` puede ser un tmpfs (en RAM, se pierde al
reiniciar). `/var/tmp` siempre esta en disco.

### Paso 4.4: Limpiar

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/test-tmp.txt /var/tmp/test-vartmp.txt
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. `/var` contiene datos **dinamicos**: logs, caches, colas y estado
   de aplicaciones. `/usr` contiene software **estatico**.

2. Los archivos de log tienen **nombres diferentes** entre Debian y
   AlmaLinux (`auth.log` vs `secure`, `syslog` vs `messages`).
   `journalctl` es la alternativa portable.

3. `/var/cache` contiene datos **regenerables** (paquetes descargados).
   `/var/lib` contiene estado **critico** (bases de datos de paquetes)
   que no se puede regenerar.

4. `/tmp` se limpia al reiniciar o cada 10 dias. `/var/tmp` se limpia
   despues de 30 dias. Usar `/var/tmp` para datos temporales que
   deben sobrevivir un reinicio.

5. `/var/run` es un **symlink** a `/run` (tmpfs) en distribuciones
   modernas.
