# T03 — /var en profundidad

## Objetivo

Explorar `/var` y sus subdirectorios: logs, cache, spool, estado persistente y
temporales. Comparar las diferencias de logs entre Debian y AlmaLinux, y
distinguir entre datos regenerables y datos críticos.

---

## Errores corregidos respecto al material original

1. **Confusión entre `/var/cache/apt/` y `/var/lib/apt/lists/`** — El lab
   original mide `/var/cache/apt/` pero explica que fue limpiado con
   `rm -rf /var/lib/apt/lists/*`. Son cosas diferentes:
   - `/var/cache/apt/archives/` = paquetes `.deb` descargados (se limpia con
     `apt-get clean`)
   - `/var/lib/apt/lists/` = metadatos de repositorios (se regenera con
     `apt-get update`)
   El patrón típico en Dockerfiles (`rm -rf /var/lib/apt/lists/*`) limpia los
   metadatos, no el cache de paquetes.

2. **`/var/log/dmesg` no es el ring buffer del kernel** — El original dice que
   es el "Ring buffer del kernel (también accesible con `dmesg`)". En realidad,
   `/var/log/dmesg` es un **snapshot** guardado al arranque. El ring buffer
   vivo se accede con el comando `dmesg` (que lee `/dev/kmsg`). El archivo
   es estático; el comando muestra el estado actual.

---

## Propósito de /var

`/var` contiene datos que **cambian durante la operación normal** del sistema.
Mientras `/usr` contiene software que se instala y rara vez cambia, `/var`
contiene los datos que ese software genera.

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
# dmesg       ← Snapshot del ring buffer del kernel al arranque
# dpkg.log    ← Actividad del gestor de paquetes [Debian]
# dnf.log     ← Actividad del gestor de paquetes [RHEL]
# apt/        ← Historial de apt [Debian]
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

`journalctl` funciona en ambas distribuciones como alternativa portable para
consultar logs sin depender de los nombres de archivo.

#### journald vs archivos de texto

En sistemas con systemd, los logs se gestionan con **journald** y se almacenan
en formato binario en `/var/log/journal/`:

```bash
# Leer logs con journalctl (formato binario)
journalctl -u sshd              # Logs de un servicio
journalctl --since "1 hour ago" # Última hora
journalctl -p err               # Solo errores

# Los archivos de texto en /var/log/ coexisten:
# rsyslog/syslog-ng escriben en /var/log/ en paralelo a journald
```

#### Rotación de logs

Los logs crecen indefinidamente si no se rotan. `logrotate` comprime y elimina
logs viejos:

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

#### Permisos de logs

Los logs sensibles tienen permisos restrictivos:

```bash
# Debian: grupo adm/syslog puede leer
ls -la /var/log/auth.log
# -rw-r----- 1 syslog adm 125000 ... auth.log

# RHEL: solo root
ls -la /var/log/secure
# -rw------- 1 root root 85000 ... secure
```

### `/var/cache` — Caches de aplicaciones

Datos que se pueden **regenerar** pero que aceleran operaciones:

```bash
ls /var/cache/
# apt/           ← Paquetes descargados por apt [Debian]
# dnf/           ← Metadata y paquetes de dnf [RHEL]
# man/           ← Base de datos de man pages (whatis)
# ldconfig/      ← Cache de bibliotecas dinámicas
```

```bash
# Ver cuánto espacio ocupa el cache de apt
du -sh /var/cache/apt/

# Limpiar cache de apt (seguro, se re-descarga al necesitarse)
apt-get clean

# Limpiar cache de dnf
dnf clean all
```

La diferencia clave entre `/var/cache` y `/tmp`: el cache contiene datos
**útiles** que aceleran operaciones futuras. Borrar `/var/cache/apt/` es seguro
pero el siguiente `apt install` tardará más (descarga de nuevo).

#### `/var/cache/apt/` vs `/var/lib/apt/lists/`

| Ruta | Contenido | Cómo limpiar | Cómo regenerar |
|---|---|---|---|
| `/var/cache/apt/archives/` | Paquetes `.deb` descargados | `apt-get clean` | Se descargan al instalar |
| `/var/lib/apt/lists/` | Metadatos de repositorios | `rm -rf /var/lib/apt/lists/*` | `apt-get update` |

El patrón típico en Dockerfiles limpia los **metadatos** (lists), no el cache:
```dockerfile
RUN apt-get update && apt-get install -y ... && rm -rf /var/lib/apt/lists/*
```

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
# systemd/       ← Estado de systemd (timers, servicios)
```

Este es el directorio **más crítico** de `/var`. Contiene bases de datos
de paquetes y estado del sistema que **no se puede regenerar**:

```bash
# Base de datos de paquetes — si se corrompe, apt/dnf no funcionan
ls -la /var/lib/dpkg/status    # Debian
ls -la /var/lib/rpm/           # RHEL
```

### `/var/tmp` — Temporales persistentes

Similar a `/tmp` pero los archivos **sobreviven al reinicio**:

| Aspecto | `/tmp` | `/var/tmp` |
|---|---|---|
| Limpieza automática | Al reiniciar o cada ~10 días | Cada ~30 días |
| Sobrevive reboot | No (si es tmpfs) | Sí (siempre en disco) |
| Permisos | `1777` (sticky bit) | `1777` (sticky bit) |
| Uso | Temporales efímeros | Compilaciones largas, descargas parciales |

Configuración de limpieza en systemd:

```bash
cat /usr/lib/tmpfiles.d/tmp.conf
# q /tmp 1777 root root 10d    ← /tmp: limpiar archivos >10 días
# q /var/tmp 1777 root root 30d ← /var/tmp: limpiar archivos >30 días
```

### `/var/run` → `/run`

En distribuciones modernas, `/var/run` es un **symlink** a `/run`:

```bash
ls -la /var/run
# lrwxrwxrwx 1 root root 4 ... /var/run -> /run
```

`/run` es un tmpfs montado al arranque. Contiene PIDs, sockets, locks y
otros datos de runtime. La migración ocurrió porque `/var` podía no estar
montado durante el arranque temprano, pero los datos de runtime se necesitan
desde el inicio.

## /var en partición separada

En servidores de producción, `/var` frecuentemente está en su propia partición:

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

Todos los comandos se ejecutan dentro de los contenedores del curso.

### Ejercicio 1 — Explorar la estructura de /var

Examina los subdirectorios de `/var` y su tamaño relativo.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Contenido de /var ==="
ls -la /var/

echo ""
echo "=== Tamaño por subdirectorio ==="
du -sh /var/*/ 2>/dev/null | sort -rh
'
```

<details>
<summary>Predicción</summary>

`ls -la /var/` muestra los subdirectorios principales: `log/`, `cache/`,
`lib/`, `spool/`, `tmp/`, y `run` (symlink a `/run`).

`du -sh` ordenado por tamaño mostrará `/var/lib/` como el más grande
(contiene la base de datos de dpkg con información de todos los paquetes
instalados). `/var/cache/` puede ser grande si hay paquetes descargados, o
pequeño si el Dockerfile los limpió. `/var/log/` generalmente es pequeño en
un contenedor recién creado.

`/var/run` no aparecerá en la lista de `du` porque es un symlink y `du` con
`/var/*/` no sigue symlinks por defecto al expandir el glob.

</details>

---

### Ejercicio 2 — Comparar archivos de log entre distribuciones

Identifica qué logs existen en cada distribución y cuáles son exclusivos.

```bash
echo "=== Debian ==="
docker compose exec -T debian-dev bash -c '
ls /var/log/ | sort
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec -T alma-dev bash -c '
ls /var/log/ | sort
'

echo ""
echo "=== Logs de autenticación ==="
echo "Debian:"
docker compose exec -T debian-dev bash -c \
    'ls /var/log/auth.log 2>/dev/null && echo "  auth.log: presente" || echo "  auth.log: NO existe"'
echo "AlmaLinux:"
docker compose exec -T alma-dev bash -c \
    'ls /var/log/secure 2>/dev/null && echo "  secure: presente" || echo "  secure: NO existe"'

echo ""
echo "=== Logs generales del sistema ==="
echo "Debian:"
docker compose exec -T debian-dev bash -c \
    'ls /var/log/syslog 2>/dev/null && echo "  syslog: presente" || echo "  syslog: NO existe"'
echo "AlmaLinux:"
docker compose exec -T alma-dev bash -c \
    'ls /var/log/messages 2>/dev/null && echo "  messages: presente" || echo "  messages: NO existe"'
```

<details>
<summary>Predicción</summary>

Los logs comunes entre ambas: `btmp` (intentos fallidos de login), `wtmp`
(historial de sesiones), `lastlog` (último login por usuario).

Los exclusivos:
- **Debian**: `auth.log`, `syslog`, `kern.log`, `dpkg.log`, `apt/`
- **AlmaLinux**: `secure`, `messages`, `dnf.log`, `cron`

En contenedores, algunos logs pueden no existir porque los servicios
correspondientes (rsyslog, syslog-ng) no están corriendo. `journalctl` es la
alternativa portable que funciona en ambos si systemd está activo.

Los nombres diferentes para el mismo contenido son un legado histórico: cada
familia de distribuciones eligió nombres distintos antes de que existiera un
estándar unificado.

</details>

---

### Ejercicio 3 — Permisos de logs sensibles

Examina quién puede leer los logs de autenticación y por qué.

```bash
echo "=== Debian ==="
docker compose exec -T debian-dev bash -c '
echo "auth.log:"
ls -la /var/log/auth.log 2>/dev/null || echo "  no existe"
echo ""
echo "btmp (intentos fallidos):"
ls -la /var/log/btmp 2>/dev/null || echo "  no existe"
echo ""
echo "wtmp (sesiones):"
ls -la /var/log/wtmp 2>/dev/null || echo "  no existe"
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec -T alma-dev bash -c '
echo "secure:"
ls -la /var/log/secure 2>/dev/null || echo "  no existe"
echo ""
echo "btmp:"
ls -la /var/log/btmp 2>/dev/null || echo "  no existe"
echo ""
echo "wtmp:"
ls -la /var/log/wtmp 2>/dev/null || echo "  no existe"
'
```

<details>
<summary>Predicción</summary>

Los permisos esperados:
- `auth.log` / `secure`: `-rw-r-----` — solo root y el grupo syslog/adm pueden
  leer. Contienen información sensible (quién se autenticó, desde dónde, cuándo
  falló)
- `btmp`: `-rw-------` o `-rw-rw----` — solo root. Registra intentos de login
  fallidos (información de seguridad)
- `wtmp`: `-rw-rw-r--` — legible por todos. Registra sesiones de usuario
  (información menos sensible; el comando `last` la lee)

La diferencia de permisos refleja la sensibilidad: `btmp` es más restrictivo
que `wtmp` porque los intentos fallidos pueden revelar patrones de ataque.

En Debian, el grupo `adm` tiene acceso de lectura a muchos logs. Agregar un
usuario al grupo `adm` le permite leer logs sin necesitar `sudo`.

</details>

---

### Ejercicio 4 — Distinguir cache regenerable de estado crítico

Compara `/var/cache` (se puede borrar) con `/var/lib` (no se debe borrar).

```bash
docker compose exec -T debian-dev bash -c '
echo "=== /var/cache (REGENERABLE) ==="
echo "Contenido:"
ls /var/cache/
echo ""
echo "Tamaño total: $(du -sh /var/cache/ 2>/dev/null | cut -f1)"
echo ""
echo "Cache de apt:"
du -sh /var/cache/apt/ 2>/dev/null || echo "  no existe o vacío"
echo ""
echo "Cache de man:"
du -sh /var/cache/man/ 2>/dev/null || echo "  no existe o vacío"

echo ""
echo "=== /var/lib (CRÍTICO) ==="
echo "Contenido:"
ls /var/lib/
echo ""
echo "Tamaño total: $(du -sh /var/lib/ 2>/dev/null | cut -f1)"
echo ""
echo "Base de datos dpkg:"
ls -la /var/lib/dpkg/status
echo "  $(wc -l < /var/lib/dpkg/status) líneas en la DB de paquetes"
'
```

<details>
<summary>Predicción</summary>

`/var/cache/` es más pequeño en un contenedor Docker porque el Dockerfile
normalmente ejecuta `rm -rf /var/lib/apt/lists/*` para reducir el tamaño de
la imagen. `/var/cache/apt/` puede estar casi vacío.

`/var/lib/` es más grande y contiene datos críticos:
- `/var/lib/dpkg/status` tiene miles de líneas — es la base de datos completa
  de paquetes instalados. Cada paquete tiene una entrada con nombre, versión,
  dependencias, descripción, etc.

La diferencia clave:
- Borrar `/var/cache/apt/` → el próximo `apt install` tarda más (re-descarga)
- Borrar `/var/lib/dpkg/status` → `apt` deja de funcionar completamente

`/var/cache` es como un cache de navegador: puedes borrarlo sin consecuencias
graves. `/var/lib` es como una base de datos: perderla es catastrófico.

</details>

---

### Ejercicio 5 — Entender `/var/cache/apt/` vs `/var/lib/apt/lists/`

Diferencia los dos directorios de apt que frecuentemente se confunden.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== /var/cache/apt/ (paquetes .deb descargados) ==="
ls /var/cache/apt/
du -sh /var/cache/apt/ 2>/dev/null
echo "  Se limpia con: apt-get clean"
echo "  Se regenera al: descargar paquetes con apt install"

echo ""
echo "=== /var/lib/apt/lists/ (metadatos de repositorios) ==="
ls /var/lib/apt/lists/ 2>/dev/null | head -5
count=$(ls /var/lib/apt/lists/ 2>/dev/null | wc -l)
echo "  $count archivos de metadatos"
echo "  Se limpia con: rm -rf /var/lib/apt/lists/*"
echo "  Se regenera con: apt-get update"
'
```

<details>
<summary>Predicción</summary>

`/var/cache/apt/` contiene un subdirectorio `archives/` donde se almacenan
los archivos `.deb` descargados durante la instalación de paquetes. Si el
Dockerfile no ejecutó `apt-get clean`, puede tener decenas o cientos de MB.

`/var/lib/apt/lists/` contiene los índices de paquetes descargados con
`apt-get update`. Si el Dockerfile ejecutó `rm -rf /var/lib/apt/lists/*`,
estará vacío (0 archivos). Esto es el patrón estándar en Dockerfiles para
reducir el tamaño de la imagen.

El patrón completo en un Dockerfile:
```dockerfile
RUN apt-get update \                    # genera /var/lib/apt/lists/*
    && apt-get install -y paquete \     # descarga a /var/cache/apt/archives/
    && rm -rf /var/lib/apt/lists/*      # limpia metadatos (no el cache)
```

Muchos Dockerfiles no ejecutan `apt-get clean` porque las capas intermedias
ya están commiteadas. El `rm -rf /var/lib/apt/lists/*` es más importante para
el tamaño de la imagen.

</details>

---

### Ejercicio 6 — Comparar bases de datos de paquetes entre distros

Examina cómo Debian y AlmaLinux almacenan la información de paquetes.

```bash
echo "=== Debian: dpkg ==="
docker compose exec -T debian-dev bash -c '
echo "DB de paquetes:"
ls -la /var/lib/dpkg/status
echo "Líneas: $(wc -l < /var/lib/dpkg/status)"
echo "Paquetes instalados: $(dpkg -l | grep "^ii" | wc -l)"
echo ""
echo "Primeros 5 paquetes:"
dpkg -l | grep "^ii" | head -5 | awk "{print \"  \" \$2 \" \" \$3}"
'

echo ""
echo "=== AlmaLinux: rpm ==="
docker compose exec -T alma-dev bash -c '
echo "DB de paquetes:"
ls -la /var/lib/rpm/
echo ""
echo "Paquetes instalados: $(rpm -qa | wc -l)"
echo ""
echo "Primeros 5 paquetes:"
rpm -qa --queryformat "%{NAME} %{VERSION}\n" | sort | head -5 | sed "s/^/  /"
'
```

<details>
<summary>Predicción</summary>

**Debian** almacena la base de datos en un archivo de texto plano
(`/var/lib/dpkg/status`) con formato de párrafos separados por líneas en
blanco. Cada paquete tiene campos como `Package:`, `Version:`, `Status:`,
`Depends:`, etc. El archivo puede tener miles de líneas.

**AlmaLinux** almacena la base de datos en archivos binarios en `/var/lib/rpm/`
(formato BerkeleyDB o SQLite dependiendo de la versión). No es legible con
`cat` — se consulta con `rpm -qa`.

Ambos contenedores tendrán cientos de paquetes instalados (las imágenes de
desarrollo incluyen gcc, gdb, valgrind, make, y muchas dependencias).

La diferencia de formato es significativa: la DB de dpkg es texto plano
(recuperable manualmente en emergencias), mientras la DB de rpm es binaria
(requiere herramientas de rpm para leerla o repararla).

</details>

---

### Ejercicio 7 — /tmp vs /var/tmp en contenedores

Examina las diferencias prácticas entre ambos directorios temporales.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Permisos ==="
echo "/tmp:     $(stat -c "%a %A" /tmp)"
echo "/var/tmp: $(stat -c "%a %A" /var/tmp)"

echo ""
echo "=== Filesystem ==="
echo "/tmp:"
df -h /tmp
echo ""
echo "/var/tmp:"
df -h /var/tmp

echo ""
echo "=== Crear archivos de prueba ==="
echo "temporal efímero" > /tmp/test-efimero.txt
echo "temporal persistente" > /var/tmp/test-persistente.txt
ls -la /tmp/test-efimero.txt /var/tmp/test-persistente.txt

echo ""
echo "=== Limpiar ==="
rm -f /tmp/test-efimero.txt /var/tmp/test-persistente.txt
echo "Archivos eliminados"
'
```

<details>
<summary>Predicción</summary>

Ambos directorios tienen permisos `1777` (`drwxrwxrwt`) con sticky bit. La `t`
significa que cualquier usuario puede crear archivos pero solo el dueño puede
borrar los suyos.

En un contenedor, ambos comparten el mismo filesystem (overlay2), así que
`df -h` mostrará el mismo dispositivo y espacio. En el **host**, `/tmp` puede
ser un tmpfs (en RAM, limitado en tamaño, se borra al reiniciar) mientras
`/var/tmp` está en disco.

Los archivos se crean sin problema en ambos. La diferencia no es de permisos
sino de **política de limpieza**:
- `systemd-tmpfiles` limpia `/tmp` cada ~10 días
- `systemd-tmpfiles` limpia `/var/tmp` cada ~30 días
- Al reiniciar, `/tmp` se borra (si es tmpfs); `/var/tmp` persiste

Usar `/var/tmp` para: compilaciones largas, descargas parciales, datos
temporales que deben sobrevivir un reinicio inesperado.

</details>

---

### Ejercicio 8 — Verificar que /var/run es symlink a /run

Confirma la migración y entiende por qué se hizo.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== /var/run ==="
ls -la /var/run
echo ""
echo "Destino: $(readlink /var/run)"

echo ""
echo "=== Mismo contenido ==="
echo "Archivos en /var/run: $(ls /var/run/ 2>/dev/null | wc -l)"
echo "Archivos en /run:     $(ls /run/ 2>/dev/null | wc -l)"

echo ""
echo "=== Contenido de /run ==="
ls /run/ 2>/dev/null | head -10

echo ""
echo "=== Tipo de filesystem ==="
df -h /run
mount | grep "on /run " || echo "  /run no aparece como mount separado"
'
```

<details>
<summary>Predicción</summary>

`/var/run` es un symlink a `/run`. `readlink` muestra `../run` o `/run`.

El conteo de archivos es idéntico porque son el mismo directorio visto desde
dos rutas.

`/run` es un tmpfs (filesystem en RAM) en el host. En un contenedor, puede
no ser un mount separado — `df -h /run` mostrará el overlay filesystem del
contenedor.

El contenido típico: archivos de PID (`.pid`), sockets Unix (`.sock`),
directorios de servicios (`systemd/`, `lock/`).

La migración de `/var/run` a `/run` se hizo porque:
- Los datos de runtime se necesitan desde el arranque temprano
- `/var` podía no estar montado aún (si está en partición separada)
- `/run` como tmpfs garantiza que empieza limpio en cada arranque

El symlink `/var/run` → `/run` mantiene compatibilidad con software que
hardcodea `/var/run/` en sus paths.

</details>

---

### Ejercicio 9 — Explorar /var/spool

Examina las colas de trabajo y entiende el concepto de spool.

```bash
echo "=== Debian ==="
docker compose exec -T debian-dev bash -c '
echo "Contenido de /var/spool:"
ls -la /var/spool/

echo ""
echo "Crontabs:"
ls -la /var/spool/cron/crontabs/ 2>/dev/null || echo "  directorio no existe o vacío"

echo ""
echo "Mail:"
ls -la /var/spool/mail/ 2>/dev/null || ls -la /var/mail/ 2>/dev/null || echo "  sin mail spool"
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec -T alma-dev bash -c '
echo "Contenido de /var/spool:"
ls -la /var/spool/

echo ""
echo "Crontabs:"
ls -la /var/spool/cron/ 2>/dev/null || echo "  directorio no existe o vacío"
'
```

<details>
<summary>Predicción</summary>

En contenedores de desarrollo, `/var/spool/` tendrá pocos subdirectorios
activos porque servicios como cron, mail, y cups probablemente no están
configurados.

La estructura difiere ligeramente:
- **Debian**: crontabs en `/var/spool/cron/crontabs/`, mail en `/var/mail/`
  (que puede ser symlink a `/var/spool/mail/`)
- **AlmaLinux**: crontabs en `/var/spool/cron/`, mail en `/var/spool/mail/`

El concepto de "spool" (Simultaneous Peripheral Operations On-Line) viene de
la era de mainframes: los datos se escribían a disco para que un daemon los
procesara asincrónicamente. El concepto persiste en cron (trabajos encolados),
impresión (CUPS), y correo (postfix/sendmail).

En servidores de producción, `/var/spool/` puede crecer significativamente si
hay muchos trabajos de cron o correo sin entregar.

</details>

---

### Ejercicio 10 — Mapa completo de /var con clasificación

Genera un resumen clasificando cada subdirectorio por criticidad.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Mapa de /var con clasificación ==="
echo ""
echo "Directorio      Tamaño     Criticidad    Regenerable?"
echo "──────────────── ────────── ───────────── ────────────"

for d in log cache lib spool tmp; do
    size=$(du -sh /var/$d 2>/dev/null | cut -f1)
    case $d in
        log)    crit="Media";  regen="Sí (se regeneran)" ;;
        cache)  crit="Baja";   regen="Sí (apt clean)" ;;
        lib)    crit="ALTA";   regen="NO" ;;
        spool)  crit="Media";  regen="Parcialmente" ;;
        tmp)    crit="Baja";   regen="Sí (temporales)" ;;
    esac
    printf "/var/%-10s %-10s %-13s %s\n" "$d" "$size" "$crit" "$regen"
done

echo ""
echo "=== Resumen ==="
echo "Total /var: $(du -sh /var 2>/dev/null | cut -f1)"
echo ""
echo "Regla general:"
echo "  /var/lib  → NUNCA borrar (bases de datos, estado del sistema)"
echo "  /var/cache → Seguro borrar (se regenera automáticamente)"
echo "  /var/log  → Rotar, no borrar (logrotate se encarga)"
echo "  /var/tmp  → Se limpia solo cada 30 días"
echo "  /var/spool → Cuidado (crontabs activas son trabajo pendiente)"
'
```

<details>
<summary>Predicción</summary>

La tabla muestra cada subdirectorio con su tamaño y clasificación de
criticidad:

- `/var/lib` es el más grande y más crítico. Contiene `/var/lib/dpkg/status`
  (base de datos de paquetes) que si se pierde, `apt` deja de funcionar. En
  un host con Docker, `/var/lib/docker/` puede ocupar decenas de GB.

- `/var/cache` es regenerable. Borrarlo es la forma más segura de liberar
  espacio. `apt-get clean` limpia paquetes descargados; `dnf clean all` hace
  lo mismo en RHEL.

- `/var/log` tiene criticidad media: los logs históricos no se pueden regenerar,
  pero el sistema sigue funcionando sin ellos. `logrotate` se encarga de
  gestionar el crecimiento.

- `/var/spool` es parcialmente regenerable: las crontabs son configuración
  (se pueden recrear), pero los trabajos encolados pendientes se pierden.

- `/var/tmp` es de baja criticidad: por definición son temporales (aunque
  persistentes entre reinicios).

</details>

---

## Resumen de conceptos

| Concepto | Detalle clave |
|---|---|
| `/var` | Datos dinámicos: logs, cache, estado, colas |
| `/var/log` | Logs del sistema; nombres diferentes entre Debian y RHEL |
| `/var/cache` | Regenerable; seguro de borrar (`apt-get clean`) |
| `/var/lib` | Estado crítico; NUNCA borrar (`dpkg/status`, `rpm/`) |
| `/var/spool` | Colas de trabajo (cron, mail, impresión) |
| `/var/tmp` | Temporales que sobreviven reboot; limpieza cada ~30 días |
| `/var/run` → `/run` | Symlink; datos de runtime en tmpfs |
| `journalctl` | Alternativa portable a leer archivos de log directamente |
| `/var/cache/apt/` vs `/var/lib/apt/lists/` | Paquetes descargados vs metadatos de repos |
| Partición separada | En producción, evita que logs llenos bloqueen el sistema |
