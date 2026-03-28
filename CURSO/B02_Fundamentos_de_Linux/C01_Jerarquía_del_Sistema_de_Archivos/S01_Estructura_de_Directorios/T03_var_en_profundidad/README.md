# T03 — /var en profundidad

## Propósito de /var

`/var` contiene datos que **cambian durante la operación normal** del sistema.
Mientras `/usr` contiene software que se instala y rara vez cambia, `/var`
contiene los datos que ese software genera: logs, caches, colas de trabajo,
estado de aplicaciones.

```
/usr → "qué se instaló"    (estático)
/var → "qué está pasando"  (dinámico)
/etc → "cómo se configuró" (configuración)
```

## Subdirectorios principales

### `/var/log` — Logs del sistema

El directorio más consultado por administradores. Contiene los registros de
actividad del sistema y las aplicaciones:

```bash
ls /var/log/
# auth.log    ← Autenticación (login, su, sudo) [Debian]
# secure      ← Equivalente en RHEL
# syslog      ← Log general del sistema [Debian]
# messages    ← Equivalente en RHEL
# kern.log    ← Mensajes del kernel [Debian]
# dmesg       ← Ring buffer del kernel (también accesible con `dmesg`)
# dpkg.log    ← Actividad del gestor de paquetes [Debian]
# dnf.log     ← Actividad del gestor de paquetes [RHEL]
# apt/        ← Historial de apt [Debian]
# nginx/      ← Logs de nginx (si está instalado)
# journal/    ← Logs binarios de journald (systemd)
```

#### Diferencias entre distribuciones

| Log | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| Autenticación | `/var/log/auth.log` | `/var/log/secure` |
| Sistema general | `/var/log/syslog` | `/var/log/messages` |
| Kernel | `/var/log/kern.log` | via `journalctl -k` |
| Paquetes | `/var/log/dpkg.log` | `/var/log/dnf.log` |
| Boot | `/var/log/boot.log` | `/var/log/boot.log` |

#### journald vs archivos de texto

En sistemas con systemd (todos los modernos), los logs se gestionan con
**journald** y se almacenan en formato binario en `/var/log/journal/`:

```bash
# Leer logs con journalctl (formato binario)
journalctl -u sshd              # Logs de un servicio
journalctl --since "1 hour ago" # Últimas hora
journalctl -p err               # Solo errores

# Los archivos de texto en /var/log/ coexisten:
# rsyslog/syslog-ng escriben en /var/log/ en paralelo a journald
```

#### Rotación de logs

Los logs crecen indefinidamente si no se rotan. `logrotate` comprime y
elimina logs viejos:

```bash
cat /etc/logrotate.d/rsyslog
```

```
/var/log/syslog {
    rotate 7          # Mantener 7 copias
    daily             # Rotar diariamente
    missingok         # No error si no existe
    compress          # Comprimir copias viejas (.gz)
    delaycompress     # No comprimir la más reciente
    notifempty        # No rotar si está vacío
    postrotate        # Comando después de rotar
        /usr/lib/rsyslog/rsyslog-rotate
    endscript
}
```

### `/var/cache` — Caches de aplicaciones

Datos que se pueden regenerar pero que aceleran operaciones:

```bash
ls /var/cache/
# apt/           ← Paquetes descargados por apt [Debian]
# dnf/           ← Metadata y paquetes de dnf [RHEL]
# man/           ← Base de datos de man pages (whatis)
# fontconfig/    ← Cache de fuentes
# ldconfig/      ← Cache de bibliotecas dinámicas
```

```bash
# Ver cuánto espacio ocupa el cache de apt
du -sh /var/cache/apt/
# 150M

# Limpiar cache de apt (seguro, se re-descarga al necesitarse)
sudo apt-get clean

# Limpiar cache de dnf
sudo dnf clean all
```

La diferencia clave entre `/var/cache` y `/tmp`: el cache contiene datos
**útiles** que aceleran operaciones futuras. Borrar `/var/cache/apt/` es
seguro pero el siguiente `apt install` tardará más (descarga de nuevo).

### `/var/spool` — Colas de trabajo

Datos en espera de ser procesados por un servicio:

```bash
ls /var/spool/
# cron/      ← Crontabs de usuarios
# mail/      ← Correo local pendiente de entrega
# cups/      ← Trabajos de impresión pendientes
# anacron/   ← Timestamps de anacron
# at/        ← Trabajos programados con at
```

El concepto de "spool" viene de "Simultaneous Peripheral Operations On-Line":
los datos se encolan y un daemon los procesa en orden.

```bash
# Ver las crontabs encoladas
ls /var/spool/cron/crontabs/     # Debian
ls /var/spool/cron/              # RHEL

# Ver trabajos de impresión pendientes
ls /var/spool/cups/
```

### `/var/lib` — Estado persistente

Datos de estado de aplicaciones que deben persistir entre reinicios:

```bash
ls /var/lib/
# apt/           ← Estado de paquetes instalados [Debian]
# dpkg/          ← Base de datos de dpkg [Debian]
# dnf/           ← Estado de paquetes [RHEL]
# rpm/           ← Base de datos de RPM [RHEL]
# docker/        ← Imágenes, capas, volúmenes de Docker
# mysql/         ← Datos de MySQL/MariaDB
# postgresql/    ← Datos de PostgreSQL
# systemd/       ← Estado de systemd (timers, servicios)
# NetworkManager/ ← Estado de conexiones de red
```

Este es el directorio **más crítico** de `/var`. Contiene bases de datos
de paquetes, datos de servicios, y estado del sistema que **no se puede
regenerar**:

```bash
# Base de datos de paquetes — si se corrompe, apt/dnf no funcionan
ls -la /var/lib/dpkg/status    # Debian
ls -la /var/lib/rpm/           # RHEL

# Datos de Docker — imágenes, capas, volúmenes
du -sh /var/lib/docker/
```

### `/var/tmp` — Temporales persistentes

Similar a `/tmp` pero los archivos **sobreviven al reinicio**:

```bash
ls -la /var/tmp/

# Comparación:
# /tmp     → se limpia al reiniciar (o periódicamente por systemd-tmpfiles)
# /var/tmp → se limpia después de 30 días (por defecto en systemd-tmpfiles)
```

Configuración de limpieza en systemd:

```bash
cat /usr/lib/tmpfiles.d/tmp.conf
# q /tmp 1777 root root 10d    ← /tmp: limpiar archivos >10 días
# q /var/tmp 1777 root root 30d ← /var/tmp: limpiar archivos >30 días
```

Usar `/var/tmp` cuando los datos temporales deben sobrevivir un reinicio
(compilaciones largas, descargas parciales).

### `/var/run` → `/run`

En distribuciones modernas, `/var/run` es un **symlink** a `/run`:

```bash
ls -la /var/run
# lrwxrwxrwx 1 root root 4 ... /var/run -> /run
```

`/run` es un tmpfs montado al arranque. Contiene:

```bash
ls /run/
# docker.sock    ← Socket de Docker
# sshd.pid       ← PID del daemon SSH
# systemd/       ← Datos de runtime de systemd
# user/          ← Datos de runtime por usuario
# lock/          ← Lock files
# utmp           ← Sesiones activas
```

La migración de `/var/run` a `/run` ocurrió porque `/var` podía no estar
montado durante el arranque temprano, pero los datos de runtime se necesitan
desde el inicio.

## Permisos y propiedad

```bash
# /var y sus subdirectorios tienen propietarios específicos
ls -la /var/
# drwxr-xr-x root root  cache/
# drwxr-xr-x root root  lib/
# drwxrwxr-x root syslog log/     ← Debian: grupo syslog
# drwxr-xr-x root root  log/     ← RHEL: solo root
# drwxr-xr-x root root  spool/
# drwxrwxrwt root root  tmp/     ← Sticky bit (como /tmp)

# Los logs tienen permisos restrictivos
ls -la /var/log/auth.log
# -rw-r----- 1 syslog adm 125000 ... auth.log  [Debian]

ls -la /var/log/secure
# -rw------- 1 root root 85000 ... secure      [RHEL]
```

## /var en partición separada

En servidores de producción, `/var` frecuentemente está en su propia
partición para:

1. **Evitar que logs llenos bloqueen el sistema** — si `/var/log` llena su
   partición, `/` no se ve afectado
2. **Diferentes políticas de backup** — `/var/lib` necesita backups frecuentes,
   `/var/cache` no
3. **Diferentes tipos de filesystem** — `/var` puede usar ext4 mientras `/`
   usa XFS

```bash
# Verificar si /var está en partición separada
df -h /var
mount | grep /var
```

---

## Ejercicios

### Ejercicio 1 — Explorar /var

```bash
# Ver el tamaño de cada subdirectorio
du -sh /var/*/ 2>/dev/null | sort -rh

# ¿Cuál ocupa más? Probablemente /var/lib (por Docker o bases de datos)
# o /var/cache (por paquetes descargados)
```

### Ejercicio 2 — Comparar logs entre distribuciones

```bash
# Debian
docker compose exec debian-dev bash -c 'ls /var/log/'

# AlmaLinux
docker compose exec alma-dev bash -c 'ls /var/log/'

# ¿Qué archivos existen en una y no en la otra?
```

### Ejercicio 3 — /tmp vs /var/tmp

```bash
# Verificar el tipo de filesystem
df -h /tmp
df -h /var/tmp

# Verificar permisos (ambos deberían tener sticky bit)
ls -ld /tmp /var/tmp

# Crear archivos en ambos
echo "test" > /tmp/test-tmp.txt
echo "test" > /var/tmp/test-vartmp.txt

# ¿Cuál sobrevivirá un reinicio?
```
