# rsync — Sincronización, Backups Remotos y Transferencias Eficientes

## Índice

1. [¿Qué es rsync y por qué no basta con cp/scp?](#1-qué-es-rsync-y-por-qué-no-basta-con-cpscp)
2. [Anatomía de un comando rsync](#2-anatomía-de-un-comando-rsync)
3. [El algoritmo delta de rsync](#3-el-algoritmo-delta-de-rsync)
4. [Modo archive: -a](#4-modo-archive--a)
5. [Control de borrado: --delete](#5-control-de-borrado---delete)
6. [Exclusiones e inclusiones](#6-exclusiones-e-inclusiones)
7. [Transferencias remotas con SSH](#7-transferencias-remotas-con-ssh)
8. [Backups con rsync](#8-backups-con-rsync)
9. [Opciones avanzadas](#9-opciones-avanzadas)
10. [rsync como daemon](#10-rsync-como-daemon)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. ¿Qué es rsync y por qué no basta con cp/scp?

`rsync` (**R**emote **Sync**) es una herramienta de sincronización que transfiere únicamente las **diferencias** entre origen y destino. A diferencia de `cp` o `scp`, que siempre copian archivos completos, rsync analiza qué ha cambiado y transmite solo eso.

```
Escenario: sincronizar 10 GB donde solo cambió 1 archivo de 50 KB

cp -a /src/ /dest/              → copia 10 GB cada vez
scp -r /src/ host:/dest/        → transmite 10 GB por red cada vez
rsync -a /src/ /dest/           → transmite ~50 KB (solo el delta)
```

### Comparativa con otras herramientas

```
┌──────────┬───────────┬───────────┬────────────┬──────────────┐
│          │ Copia     │ Preserva  │ Transfiere │ Maneja       │
│          │ parcial   │ metadatos │ por red    │ borrados     │
├──────────┼───────────┼───────────┼────────────┼──────────────┤
│ cp       │ No        │ Con -a    │ No         │ No           │
│ scp      │ No        │ Con -p    │ Sí (SSH)   │ No           │
│ tar+ssh  │ No        │ Sí       │ Sí (pipe)  │ No           │
│ rsync    │ Sí (delta)│ Con -a    │ Sí (SSH)   │ Sí (--delete)│
└──────────┴───────────┴───────────┴────────────┴──────────────┘
```

### Cuándo usar rsync vs tar

```
rsync es mejor cuando:
  ├── Sincronización continua (los datos ya existen en el destino)
  ├── Backup a disco remoto (solo deltas por red)
  ├── Mirrors de directorios grandes que cambian poco
  └── Necesitas --delete para mantener réplica exacta

tar es mejor cuando:
  ├── Crear archivo único portátil (.tar.gz)
  ├── Backup incremental con snapshot (--listed-incremental)
  ├── Distribución de software
  └── Backup a cinta o almacenamiento offline
```

---

## 2. Anatomía de un comando rsync

```
rsync [OPCIONES] ORIGEN DESTINO
```

### La regla de la barra final (trailing slash)

Esta es la fuente de confusión más frecuente en rsync. La barra `/` al final del **origen** cambia el comportamiento:

```bash
# SIN barra: copia el directorio DENTRO del destino
rsync -a /src/data /dest/
# Resultado: /dest/data/file1, /dest/data/file2

# CON barra: copia el CONTENIDO dentro del destino
rsync -a /src/data/ /dest/
# Resultado: /dest/file1, /dest/file2
```

```
Sin trailing slash en origen:        Con trailing slash en origen:

/src/data/                           /src/data/
├── file1                            ├── file1
└── file2                            └── file2
         │                                    │
    rsync -a /src/data /dest/        rsync -a /src/data/ /dest/
         │                                    │
         ▼                                    ▼
/dest/                               /dest/
└── data/    ← directorio creado     ├── file1  ← contenido directo
    ├── file1                        └── file2
    └── file2
```

> **Regla mnemotécnica**: barra final = "el contenido de", sin barra = "el directorio completo".

> **Nota**: la barra en el **destino** no tiene efecto. Solo importa en el origen.

### Opciones más usadas

| Opción | Descripción |
|--------|-------------|
| `-a` | Archive mode (equivale a `-rlptgoD`) |
| `-v` | Verbose |
| `-n` / `--dry-run` | Simular sin hacer cambios |
| `--delete` | Borrar en destino lo que no existe en origen |
| `--exclude` | Excluir archivos por patrón |
| `-z` | Comprimir datos durante la transferencia |
| `--progress` | Mostrar progreso por archivo |
| `-P` | Equivale a `--progress --partial` |
| `-e` | Especificar shell remota (típicamente SSH) |
| `--stats` | Mostrar estadísticas al final |
| `-h` | Tamaños legibles (human-readable) |

---

## 3. El algoritmo delta de rsync

rsync no copia archivos completos. Usa un algoritmo de **rolling checksum** para identificar y transmitir solo los bloques que cambiaron.

### Proceso de transferencia

```
Receptor (tiene versión vieja)          Emisor (tiene versión nueva)
┌──────────────────────────┐           ┌──────────────────────────┐
│ archivo_viejo.dat        │           │ archivo_nuevo.dat        │
│ [blk1][blk2][blk3][blk4]│           │ [blk1][BLK2'][blk3][blk4]│
└──────────────────────────┘           └──────────────────────────┘
         │                                      │
         │  1. Calcula checksums por bloque      │
         │     (rolling + MD5)                   │
         ├─────────────────────────────────────▶│
         │     "Estos son mis bloques"           │
         │                                      │
         │  2. Compara con sus bloques           │
         │     Identifica: blk2 ≠ BLK2'         │
         │                                      │
         │  3. Envía solo los bloques diferentes │
         │◀─────────────────────────────────────┤
         │     "Aquí tienes BLK2'"              │
         │                                      │
         │  4. Reconstruye el archivo            │
         │     [blk1][BLK2'][blk3][blk4]        │
         └──────────────────────────────────────┘
```

### Rolling checksum

El **rolling checksum** (basado en Adler-32) permite calcular el checksum de cada ventana de N bytes deslizándose por el archivo en O(1) por posición, en lugar de recalcular completamente. Esto permite detectar bloques que se movieron de posición, no solo los que cambiaron.

### Optimización: quick check

Por defecto, rsync **no calcula checksums de todos los archivos**. Primero hace un "quick check":

```
¿El archivo cambió?
├── Comparar tamaño (size)
│   └── ¿Diferente? → transferir
├── Comparar mtime
│   └── ¿Diferente? → transferir (con delta)
└── Ambos iguales → SKIP (no transferir)

Forzar verificación por checksum:
  rsync -a --checksum /src/ /dest/
  (más lento pero más seguro — compara contenido real)
```

> **Pregunta de predicción**: Si haces `touch file.txt` (cambia mtime pero no contenido), ¿rsync lo re-transferirá? ¿Y con `--checksum`?

Sin `--checksum`, sí lo transfiere (mtime cambió). Con `--checksum`, no lo transfiere (el contenido es idéntico, el checksum coincide). Esto ilustra el trade-off: quick check es más rápido pero puede hacer transferencias innecesarias; `--checksum` es más preciso pero recalcula hashes de todos los archivos.

---

## 4. Modo archive: -a

`-a` es la opción más importante de rsync. Es un shorthand para un conjunto de flags que preservan la estructura y metadatos:

```
-a  equivale a  -r -l -p -t -g -o -D

-r    recursive       Entrar en subdirectorios
-l    links           Copiar symlinks como symlinks
-p    permissions     Preservar permisos (rwx)
-t    times           Preservar mtime
-g    group           Preservar grupo
-o    owner           Preservar propietario (requiere root)
-D    devices+specials Preservar device files y special files
```

### Lo que -a NO incluye

```
Metadato               Flag necesario       Nota
──────────────────────────────────────────────────────────────
Hardlinks              -H                   Detecta y recrea hardlinks
ACLs                   -A / --acls          POSIX ACLs
Extended attributes    -X / --xattrs        Incluye SELinux contexts
Sparse files           -S / --sparse        Recrea holes en sparse files
Compresión en tránsito -z                   Comprime durante transferencia
```

### Uso típico con metadatos completos

```bash
# Sincronización básica
rsync -av /src/ /dest/

# Sincronización completa con todos los metadatos
rsync -aHAXv /src/ /dest/

# Para backups de sistema (como root)
sudo rsync -aHAXv --numeric-ids /src/ /dest/
```

### -a vs cp -a

```bash
# cp -a copia TODO cada vez
cp -a /src/ /dest/
# Tiempo: proporcional al tamaño total

# rsync -a solo sincroniza diferencias
rsync -a /src/ /dest/
# Primera vez: similar a cp -a
# Siguientes veces: mucho más rápido (solo deltas)
```

---

## 5. Control de borrado: --delete

Sin `--delete`, rsync **solo añade y actualiza** archivos en el destino. Los archivos que ya no existen en el origen permanecen en el destino. Con `--delete`, rsync convierte el destino en una réplica exacta del origen.

```
Sin --delete:                    Con --delete:

Origen:   A B C                  Origen:   A B C
Destino:  A B D E                Destino:  A B D E
                                                │ │
Resultado: A B C D E             Resultado: A B C
           (D y E sobreviven)               (D y E eliminados)
```

### Variantes de --delete

| Variante | Cuándo borra | Uso |
|----------|-------------|-----|
| `--delete` | Antes de transferir (default) | Uso general |
| `--delete-before` | Explícitamente antes | Liberar espacio primero |
| `--delete-during` | Durante la transferencia | Más eficiente en memoria |
| `--delete-after` | Después de transferir | Más seguro (primero copia todo) |
| `--delete-excluded` | También borra archivos excluidos | Mirrors exactos |

### Protecciones contra borrado accidental

```bash
# SIEMPRE hacer dry-run primero con --delete
rsync -avn --delete /src/ /dest/
# -n = dry-run, muestra qué HARÍA sin hacer nada

# Mover archivos borrados a un directorio en vez de eliminarlos
rsync -av --delete --backup --backup-dir=/dest/.deleted/ /src/ /dest/

# Limitar borrado: si se borrarían más de N archivos, abortar
rsync -av --delete --max-delete=100 /src/ /dest/
# Si hay >100 archivos por borrar, rsync aborta con error

# Proteger archivos específicos del borrado
rsync -av --delete --filter='P *.log' /src/ /dest/
# P = protect, *.log en dest no se borra aunque no esté en src
```

> **Pregunta de predicción**: Ejecutas `rsync -av --delete /src /dest/` (sin barra final en /src). ¿Qué archivos se borran de /dest/?

Sin barra final, rsync sincroniza `/dest/src/` como subdirectorio. `--delete` solo actúa **dentro** de lo que rsync sincroniza, es decir, dentro de `/dest/src/`. Los demás archivos directamente en `/dest/` no se tocan. Esto es menos destructivo de lo esperado, pero probablemente no es lo que querías.

---

## 6. Exclusiones e inclusiones

### Excluir por patrón

```bash
# Excluir un directorio
rsync -av --exclude='node_modules' /project/ /backup/

# Excluir múltiples patrones
rsync -av \
    --exclude='*.log' \
    --exclude='*.tmp' \
    --exclude='.git' \
    --exclude='__pycache__' \
    /project/ /backup/

# Excluir desde un archivo
cat > /etc/rsync-excludes.txt << 'EOF'
*.log
*.tmp
*.cache
.git
node_modules
__pycache__
*.pyc
EOF

rsync -av --exclude-from=/etc/rsync-excludes.txt /project/ /backup/
```

### Sintaxis de patrones

```
Patrón              Qué hace
─────────────────────────────────────────────────────
*.log               Cualquier .log en cualquier nivel
/logs               Solo el directorio "logs" en la raíz del transfer
logs/               Cualquier directorio llamado "logs"
/data/temp/         Solo /data/temp/ (ruta exacta)
*.o                 Cualquier archivo .o
**/*.log            Cualquier .log en cualquier profundidad (explícito)
```

### Include + Exclude: transferir solo ciertos archivos

El orden importa. rsync procesa las reglas de arriba a abajo — la primera que coincide gana.

```bash
# Copiar SOLO archivos .conf de un árbol de directorios
rsync -av \
    --include='*/' \
    --include='*.conf' \
    --exclude='*' \
    /etc/ /backup/etc/

# Lógica:
# 1. --include='*/'      → incluir todos los directorios (para recorrerlos)
# 2. --include='*.conf'  → incluir archivos .conf
# 3. --exclude='*'       → excluir todo lo demás
```

### Filtros con --filter

```bash
# Sintaxis unificada de filtros
rsync -av --filter='- *.log' --filter='- .git/' /src/ /dest/
# - = exclude, + = include, P = protect, H = hide

# Leer filtros desde archivo con merge
rsync -av --filter='merge /etc/rsync-filters.conf' /src/ /dest/

# Archivo .rsync-filter en cada directorio (como .gitignore)
rsync -av --filter=':- .rsync-filter' /src/ /dest/
```

### --exclude vs --delete-excluded

```bash
# --exclude: archivos excluidos se ignoran completamente
rsync -av --delete --exclude='*.log' /src/ /dest/
# Los .log en /dest/ NO se borran (rsync los ignora)

# --delete-excluded: borra del destino incluso los excluidos
rsync -av --delete --delete-excluded --exclude='*.log' /src/ /dest/
# Los .log en /dest/ SÍ se borran
```

---

## 7. Transferencias remotas con SSH

### Sintaxis remota

rsync usa SSH por defecto para transferencias remotas. La sintaxis remota usa `:` como separador:

```bash
# LOCAL → REMOTO (push)
rsync -av /local/data/ user@host:/remote/backup/

# REMOTO → LOCAL (pull)
rsync -av user@host:/remote/data/ /local/backup/

# Con puerto SSH no estándar
rsync -av -e 'ssh -p 2222' /data/ user@host:/backup/

# Con clave SSH específica
rsync -av -e 'ssh -i ~/.ssh/backup_key' /data/ user@host:/backup/
```

### Optimización para transferencias remotas

```bash
# Comprimir durante la transferencia (ahorra ancho de banda)
rsync -avz /data/ user@host:/backup/
# -z usa zlib; no comprimir si los datos ya están comprimidos

# Limitar ancho de banda (en KB/s)
rsync -av --bwlimit=5000 /data/ user@host:/backup/
# 5000 KB/s ≈ 5 MB/s — útil para no saturar la red

# Mostrar progreso general (no por archivo)
rsync -av --info=progress2 /data/ user@host:/backup/
# Muestra: bytes transferidos, velocidad, % completado, tiempo restante

# Partial + progress: reanudar transferencias interrumpidas
rsync -avP /data/ user@host:/backup/
# -P = --partial --progress
# --partial: no borrar archivos parcialmente transferidos
```

### Transferencias reanudables

```bash
# Sin --partial: si la conexión se corta, el archivo parcial se borra
# Con --partial: el archivo parcial se mantiene, rsync reanuda

# Usar directorio temporal para parciales
rsync -av --partial-dir=.rsync-partial /data/ user@host:/backup/
# Los parciales se guardan en .rsync-partial/ hasta completarse
```

### Tunneling y proxies

```bash
# A través de un jump host / bastion
rsync -av -e 'ssh -J jumpuser@bastion' /data/ user@internal:/backup/

# A través de un proxy SOCKS
rsync -av -e 'ssh -o ProxyCommand="nc -X 5 -x proxy:1080 %h %p"' \
    /data/ user@host:/backup/
```

---

## 8. Backups con rsync

### Backup simple (mirror)

```bash
# Mirror exacto: destino = copia del origen
rsync -aHAXv --delete /data/ /backup/data/

# Automatizar con cron
# /etc/cron.d/rsync-backup
0 2 * * * root rsync -a --delete /data/ /backup/data/ >> /var/log/rsync-backup.log 2>&1
```

### Backup con hardlinks (snapshots sin duplicar datos)

Esta técnica crea snapshots diarios que parecen copias completas pero comparten archivos sin cambios mediante hardlinks, usando espacio solo para los archivos que cambiaron.

```
Día 1 (full):      Día 2 (snapshot):    Día 3 (snapshot):
backup/             backup/              backup/
└── 2026-03-01/     └── 2026-03-02/      └── 2026-03-03/
    ├── fileA ─────────── fileA (hl) ────────── fileA (hl)
    ├── fileB ─────────── fileB (hl)     ├── fileB' (nuevo, cambió)
    └── fileC ─────────── fileC (hl) ────────── fileC (hl)

hl = hardlink (mismo inode, 0 bytes extra de espacio)
fileB' = copia nueva (fileB cambió, se crea archivo nuevo)

Espacio real usado:
  Día 1: tamaño completo
  Día 2: ~0 (solo hardlinks)
  Día 3: solo el tamaño de fileB' (lo que cambió)
```

```bash
#!/bin/bash
# rsync_snapshot.sh — backup con hardlinks estilo Time Machine

SOURCE="/data/"
BACKUP_BASE="/backup/snapshots"
DATE=$(date +%Y-%m-%d_%H%M)
LATEST="$BACKUP_BASE/latest"
NEW="$BACKUP_BASE/$DATE"

# --link-dest: si un archivo no cambió, hacer hardlink al snapshot anterior
rsync -aHAXv --delete \
    --link-dest="$LATEST" \
    "$SOURCE" "$NEW"

# Actualizar el symlink "latest"
rm -f "$LATEST"
ln -s "$NEW" "$LATEST"

# Limpiar snapshots de más de 30 días
find "$BACKUP_BASE" -maxdepth 1 -type d -mtime +30 -exec rm -rf {} +

echo "Snapshot: $NEW"
du -sh "$NEW"
```

### Cómo funciona --link-dest

```
rsync con --link-dest=/backup/yesterday/ /src/ /backup/today/

Para cada archivo en /src/:
  1. ¿Existe en --link-dest con mismo contenido?
     ├── SÍ  → crear hardlink en /backup/today/ (0 espacio extra)
     └── NO  → copiar archivo completo a /backup/today/

  Resultado: /backup/today/ parece una copia completa
             pero solo consume espacio por archivos que cambiaron
```

### Backup remoto con script completo

```bash
#!/bin/bash
# remote_backup.sh — backup remoto con logging y verificación

REMOTE="backup@nas.local"
SOURCE="/data/"
DEST="/volume1/backups/server1"
LOG="/var/log/rsync-backup.log"
LOCK="/var/run/rsync-backup.lock"

# Evitar ejecución concurrente
if [ -f "$LOCK" ]; then
    echo "$(date): backup already running" >> "$LOG"
    exit 1
fi
trap "rm -f $LOCK" EXIT
touch "$LOCK"

# Ejecutar rsync
rsync -aHAXz --delete \
    --exclude-from=/etc/rsync-excludes.txt \
    --stats \
    --timeout=300 \
    -e 'ssh -i /root/.ssh/backup_key -o StrictHostKeyChecking=yes' \
    "$SOURCE" "$REMOTE:$DEST/" >> "$LOG" 2>&1

STATUS=$?

# Log del resultado
case $STATUS in
    0)  echo "$(date): backup completed successfully" >> "$LOG" ;;
    23) echo "$(date): backup completed with partial transfer errors" >> "$LOG" ;;
    24) echo "$(date): backup completed, some files vanished during transfer" >> "$LOG" ;;
    *)  echo "$(date): backup FAILED with exit code $STATUS" >> "$LOG" ;;
esac
```

### Códigos de salida de rsync

| Código | Significado |
|--------|-------------|
| 0 | Éxito |
| 1 | Error de sintaxis |
| 2 | Incompatibilidad de protocolo |
| 3 | Error seleccionando archivos I/O |
| 5 | Error al iniciar protocolo cliente-servidor |
| 10 | Error en socket I/O |
| 11 | Error en file I/O |
| 12 | Error en flujo de datos del protocolo |
| 23 | Transferencia parcial (algunos archivos no se pudieron transferir) |
| 24 | Archivos que desaparecieron durante la transferencia |
| 25 | --max-delete alcanzado |
| 30 | Timeout esperando al enviar/recibir datos |

---

## 9. Opciones avanzadas

### Dry-run: simular antes de ejecutar

```bash
# SIEMPRE hacer dry-run antes de operaciones con --delete
rsync -avn --delete /src/ /dest/
# Muestra exactamente qué haría sin hacer nada

# Formato de salida del dry-run
# >f+++++++++ file.txt       → archivo nuevo a transferir
# >f..t...... file.txt       → archivo con mtime diferente
# >f.s....... file.txt       → archivo con size diferente
# *deleting   oldfile.txt    → archivo a borrar
# cd+++++++++ newdir/        → directorio nuevo a crear
```

### Itemize: entender la salida detallada

```bash
rsync -avi /src/ /dest/
# -i / --itemize-changes muestra cambios detallados

# Formato: YXcstpoguax
# Y = tipo de update: < enviado, > recibido, c creado local, h hardlink
# X = tipo de archivo: f file, d directory, L symlink, D device, S special
# c = checksum diferente
# s = size diferente
# t = time diferente
# p = permissions diferente
# o = owner diferente
# g = group diferente
# u = (reservado)
# a = ACL diferente
# x = xattr diferente

# Ejemplo de salida:
# >f.st...... file.txt    → archivo recibido, size y time cambiaron
# .d..t...... subdir/     → directorio, solo mtime cambió
# >f+++++++++ nuevo.txt   → archivo completamente nuevo
```

### Limitar transferencia

```bash
# Limitar ancho de banda
rsync -av --bwlimit=10m /src/ host:/dest/
# 10m = 10 MB/s (acepta K, M, G)

# Excluir archivos grandes
rsync -av --max-size=100M /src/ /dest/
# Omitir archivos mayores a 100 MB

# Excluir archivos pequeños
rsync -av --min-size=1K /src/ /dest/
```

### Checksum mode

```bash
# Comparar por checksum en vez de mtime+size
rsync -avc /src/ /dest/
# Más lento (lee todos los archivos) pero más preciso
# Útil cuando los clocks no están sincronizados
```

### Archivos sparse

```bash
# Reconstruir sparse files eficientemente
rsync -avS /src/ /dest/
# Sin -S: un disco virtual de 100G sparse se transfiere como 100G
# Con -S: solo se transfieren los bloques con datos reales
```

### Permisos y ownership en transferencias remotas

```bash
# Si rsync corre como usuario normal en el destino remoto:
# -o y -g fallan silenciosamente (no puede cambiar owner/group)
# -p funciona limitado por umask

# Forzar preservación numérica de UID/GID (cross-system)
rsync -av --numeric-ids /src/ root@host:/dest/

# No preservar owner/group (útil sin root en destino)
rsync -av --no-o --no-g /src/ user@host:/dest/
```

---

## 10. rsync como daemon

rsync puede ejecutarse como servicio, escuchando en el puerto 873, para transferencias sin SSH (más rápido, pero sin cifrado).

### Configuración del daemon

```ini
# /etc/rsyncd.conf
uid = nobody
gid = nobody
use chroot = yes
max connections = 10
log file = /var/log/rsyncd.log
pid file = /var/run/rsyncd.pid

[backups]
    path = /data/backups
    comment = Backup repository
    read only = no
    list = yes
    auth users = backupuser
    secrets file = /etc/rsyncd.secrets
    hosts allow = 192.168.1.0/24
    hosts deny = *

[public]
    path = /data/public
    comment = Public files
    read only = yes
    list = yes
```

```bash
# Archivo de secretos
echo "backupuser:contraseña_segura" > /etc/rsyncd.secrets
chmod 600 /etc/rsyncd.secrets

# Iniciar daemon
systemctl enable --now rsyncd

# Conexión al daemon (doble :: en vez de :)
rsync -av rsync://backupuser@server/backups/ /local/
# O equivalente:
rsync -av backupuser@server::backups/ /local/
```

> **Seguridad**: el protocolo rsync nativo (puerto 873) **no cifra** el tráfico. Úsalo solo en redes privadas. Para transferencias por internet, usa rsync sobre SSH.

### rsync sobre SSH vs daemon

```
                  rsync + SSH              rsync daemon
Cifrado           ✅ (SSH)                 ❌
Puerto            22 (SSH)                 873
Autenticación     claves SSH               secrets file
Velocidad         Algo más lento (cifrado) Más rápido
Configuración     Solo SSH                 rsyncd.conf
Caso de uso       Internet/WAN             LAN privada
```

---

## 11. Errores comunes

### Error 1: Olvidar la barra final en el origen

```bash
# MAL — crea /backup/data/data/ (directorio anidado)
rsync -a /data /backup/data/
# Resultado: /backup/data/data/file1

# BIEN — sincroniza el contenido
rsync -a /data/ /backup/data/
# Resultado: /backup/data/file1

# TAMBIÉN BIEN — sin barra, pero destino un nivel arriba
rsync -a /data /backup/
# Resultado: /backup/data/file1
```

### Error 2: Usar --delete sin dry-run previo

```bash
# PELIGROSO — puede borrar datos irrecuperablemente
rsync -a --delete /src/ /dest/

# SEGURO — primero simular
rsync -avn --delete /src/ /dest/
# Revisar la salida, especialmente líneas "*deleting"
# Solo después ejecutar sin -n
rsync -av --delete /src/ /dest/
```

### Error 3: No entender qué preserva -a como usuario no-root

```bash
# Como usuario normal, -o y -g fallan silenciosamente
rsync -av /src/ /dest/
# Owner y group se asignan al usuario que ejecuta rsync
# No hay error visible, pero los metadatos NO se preservan

# Para diagnosticar: usar -i para ver qué cambió
rsync -avi /src/ /dest/
# "o" y "g" en la salida itemizada indican cambios de ownership
```

### Error 4: Usar -z con datos ya comprimidos

```bash
# INEFICIENTE — comprimir .jpg/.mp4/.gz no los reduce, solo consume CPU
rsync -avz /photos/ host:/backup/photos/

# MEJOR — omitir compresión para archivos ya comprimidos
rsync -av --compress-choice=zstd \
    --skip-compress=gz/jpg/mp4/zip/rar/7z/bz2/xz/zst/png \
    /data/ host:/backup/

# O simplemente no usar -z si la mayoría son archivos comprimidos
rsync -av /photos/ host:/backup/photos/
```

### Error 5: Confundir rsync remoto con daemon

```bash
# Un solo : → usa SSH
rsync -av user@host:/path/ /local/

# Doble :: → usa daemon rsync (puerto 873)
rsync -av user@host::module/ /local/

# Error típico: intentar :: sin daemon corriendo
rsync -av user@host::backups/ /local/
# rsync: failed to connect to host: Connection refused (111)
```

---

## 12. Cheatsheet

```
SINCRONIZACIÓN LOCAL
  rsync -av /src/ /dest/                Mirror básico
  rsync -aHAXv /src/ /dest/            Mirror con todos los metadatos
  rsync -avn /src/ /dest/              Dry-run (simular)
  rsync -avi /src/ /dest/              Itemize (ver qué cambió)

BORRADO
  rsync -av --delete /src/ /dest/      Borrar extras en destino
  rsync -avn --delete /src/ /dest/     Dry-run con borrado
  rsync -av --delete --max-delete=50   Limitar borrados
  rsync -av --delete --backup --backup-dir=.old  Mover borrados

EXCLUSIONES
  rsync -av --exclude='*.log' /src/ /dest/         Excluir patrón
  rsync -av --exclude-from=file /src/ /dest/        Desde archivo
  rsync -av --include='*.conf' --exclude='*' /src/  Solo .conf

REMOTO (SSH)
  rsync -avz /src/ user@host:/dest/               Push comprimido
  rsync -avz user@host:/src/ /dest/               Pull comprimido
  rsync -av -e 'ssh -p 2222' /src/ host:/dest/    Puerto SSH custom
  rsync -avP /src/ host:/dest/                     Progress + partial

BACKUPS
  rsync -aHAXv --delete /src/ /backup/            Mirror exacto
  rsync -a --link-dest=../prev /src/ /backup/new  Snapshot hardlinks
  rsync -avz --bwlimit=5m /src/ host:/backup/     Limitado a 5 MB/s

VERIFICACIÓN
  rsync -avc /src/ /dest/              Comparar por checksum
  rsync -avn --delete /src/ /dest/     Ver qué cambiaría
  rsync -av --stats /src/ /dest/       Estadísticas al final
```

---

## 13. Ejercicios

### Ejercicio 1: Sincronización local con dry-run

**Objetivo**: Entender la salida de rsync y la importancia del dry-run.

```bash
# 1. Crear estructura de prueba
mkdir -p /tmp/rsync-lab/{src,dest}
echo "file A original" > /tmp/rsync-lab/src/fileA.txt
echo "file B original" > /tmp/rsync-lab/src/fileB.txt
mkdir -p /tmp/rsync-lab/src/subdir
echo "nested file" > /tmp/rsync-lab/src/subdir/nested.txt

# 2. Sincronización inicial
rsync -av /tmp/rsync-lab/src/ /tmp/rsync-lab/dest/
ls -laR /tmp/rsync-lab/dest/

# 3. Hacer cambios en el origen
echo "file A modified" > /tmp/rsync-lab/src/fileA.txt
echo "new file C" > /tmp/rsync-lab/src/fileC.txt
rm /tmp/rsync-lab/src/fileB.txt

# 4. Dry-run SIN --delete
rsync -avn /tmp/rsync-lab/src/ /tmp/rsync-lab/dest/
# Observar: fileA se re-transfiere, fileC se añade, fileB NO se borra

# 5. Dry-run CON --delete
rsync -avn --delete /tmp/rsync-lab/src/ /tmp/rsync-lab/dest/
# Observar: ahora aparece "*deleting fileB.txt"

# 6. Ejecutar con itemize para ver detalles
rsync -avi --delete /tmp/rsync-lab/src/ /tmp/rsync-lab/dest/

# 7. Verificar estado final
ls -la /tmp/rsync-lab/dest/
cat /tmp/rsync-lab/dest/fileA.txt
```

**Pregunta de reflexión**: En el paso 4, ¿por qué `fileA.txt` aparece para re-transferencia si solo cambió el contenido? ¿Qué dos atributos compara rsync por defecto para decidir si un archivo cambió?

> rsync compara **tamaño** y **mtime** por defecto (quick check). Cuando hiciste `echo "file A modified"`, el archivo cambió de tamaño (de "file A original" a "file A modified") y se actualizó el mtime. Cualquiera de los dos cambios es suficiente para que rsync decida re-transferir. Si solo hubieras cambiado el mtime con `touch` sin modificar el contenido, rsync aún lo transferiría — a menos que usaras `--checksum`.

---

### Ejercicio 2: Backup con snapshots por hardlinks

**Objetivo**: Crear múltiples snapshots que parecen copias completas pero comparten espacio.

```bash
# 1. Crear datos de origen
mkdir -p /tmp/rsync-snap/{src,backups}
for i in $(seq 1 5); do
    dd if=/dev/urandom of=/tmp/rsync-snap/src/file$i.dat bs=1K count=100 2>/dev/null
done
echo "config data" > /tmp/rsync-snap/src/config.txt

# 2. Snapshot día 1 (full, no hay --link-dest)
rsync -av /tmp/rsync-snap/src/ /tmp/rsync-snap/backups/day1/
du -sh /tmp/rsync-snap/backups/day1/

# 3. Modificar solo un archivo
dd if=/dev/urandom of=/tmp/rsync-snap/src/file3.dat bs=1K count=100 2>/dev/null

# 4. Snapshot día 2 (con --link-dest apuntando al día 1)
rsync -av --link-dest=../day1 /tmp/rsync-snap/src/ /tmp/rsync-snap/backups/day2/

# 5. Comparar espacio usado
echo "--- Espacio por snapshot ---"
du -sh /tmp/rsync-snap/backups/day1/
du -sh /tmp/rsync-snap/backups/day2/
echo "--- Espacio total (con hardlinks) ---"
du -sh /tmp/rsync-snap/backups/

# 6. Verificar que los archivos sin cambios comparten inode
echo "--- Inodes de file1.dat ---"
ls -i /tmp/rsync-snap/backups/day1/file1.dat
ls -i /tmp/rsync-snap/backups/day2/file1.dat
# Mismo inode = hardlink = 0 espacio extra

echo "--- Inodes de file3.dat ---"
ls -i /tmp/rsync-snap/backups/day1/file3.dat
ls -i /tmp/rsync-snap/backups/day2/file3.dat
# Diferente inode = archivo nuevo (cambió)

# 7. Snapshot día 3 (modificar otro archivo)
echo "updated config" > /tmp/rsync-snap/src/config.txt
rsync -av --link-dest=../day2 /tmp/rsync-snap/src/ /tmp/rsync-snap/backups/day3/

# 8. Cada snapshot es una copia completa navegable
ls -la /tmp/rsync-snap/backups/day3/
# Todos los archivos están ahí, aunque la mayoría son hardlinks
```

**Pregunta de reflexión**: Si borras el snapshot `day1/`, ¿qué pasa con los archivos en `day2/` que eran hardlinks a `day1/`? ¿Se pierden?

> No se pierden. Un hardlink es un enlace directo al inode en el filesystem. Mientras al menos un hardlink apunte al inode, los datos sobreviven. Borrar `day1/file1.dat` reduce el contador de enlaces del inode de 2 a 1, pero `day2/file1.dat` sigue apuntando al mismo inode y los datos permanecen intactos. Los datos solo se eliminan cuando el último hardlink desaparece (link count llega a 0).

---

### Ejercicio 3: Sincronización selectiva con filtros

**Objetivo**: Dominar el sistema de include/exclude para transferir solo lo necesario.

```bash
# 1. Crear estructura de proyecto
mkdir -p /tmp/rsync-filter/project/{src,build,docs,tests,.git/objects}
echo "main.c" > /tmp/rsync-filter/project/src/main.c
echo "utils.c" > /tmp/rsync-filter/project/src/utils.c
echo "main.o" > /tmp/rsync-filter/project/build/main.o
echo "app" > /tmp/rsync-filter/project/build/app
echo "readme" > /tmp/rsync-filter/project/docs/README.md
echo "test" > /tmp/rsync-filter/project/tests/test_main.c
echo "gitobj" > /tmp/rsync-filter/project/.git/objects/abc
echo "big.log" > /tmp/rsync-filter/project/debug.log
mkdir -p /tmp/rsync-filter/dest

# 2. Intentar rsync sin filtros (ver qué se copia)
rsync -avn /tmp/rsync-filter/project/ /tmp/rsync-filter/dest/ | sort

# 3. Excluir VCS y build artifacts
rsync -avn \
    --exclude-vcs \
    --exclude='build/' \
    --exclude='*.log' \
    /tmp/rsync-filter/project/ /tmp/rsync-filter/dest/

# 4. Solo archivos .c (include + exclude)
rsync -avn \
    --include='*/' \
    --include='*.c' \
    --exclude='*' \
    /tmp/rsync-filter/project/ /tmp/rsync-filter/dest/
# Observar: solo main.c, utils.c, test_main.c

# 5. Solo src/ y docs/ (excluir el resto)
rsync -avn \
    --include='src/***' \
    --include='docs/***' \
    --exclude='*' \
    /tmp/rsync-filter/project/ /tmp/rsync-filter/dest/

# 6. Ejecutar la opción elegida y verificar
rsync -av \
    --exclude-vcs \
    --exclude='build/' \
    --exclude='*.log' \
    /tmp/rsync-filter/project/ /tmp/rsync-filter/dest/
ls -laR /tmp/rsync-filter/dest/
```

**Pregunta de reflexión**: En el paso 4, ¿por qué es necesario `--include='*/'` antes de `--include='*.c'`? ¿Qué pasaría si lo omites?

> Sin `--include='*/'`, rsync no entra en los subdirectorios. El `--exclude='*'` al final bloquea **todo**, incluyendo directorios. Sin la regla que incluye directorios, rsync no puede recorrer `src/` ni `tests/`, así que solo encontraría archivos `.c` en la raíz del transfer (y no hay ninguno ahí). La regla `--include='*/'` dice "incluir todos los directorios para poder recorrerlos", y luego `--include='*.c'` selecciona solo los archivos `.c` encontrados dentro de ellos.

---

> **Siguiente tema**: T03 — dd: clonar discos/particiones, crear imágenes, precauciones.
