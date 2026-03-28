# tar — Creación de Backups, Compresión e Incrementales

## Índice

1. [¿Qué es tar?](#1-qué-es-tar)
2. [Anatomía de un comando tar](#2-anatomía-de-un-comando-tar)
3. [Operaciones fundamentales](#3-operaciones-fundamentales)
4. [Compresión](#4-compresión)
5. [Backups incrementales con --listed-incremental](#5-backups-incrementales-con---listed-incremental)
6. [Preservación de metadatos y permisos](#6-preservación-de-metadatos-y-permisos)
7. [Exclusiones y filtros](#7-exclusiones-y-filtros)
8. [Operaciones avanzadas](#8-operaciones-avanzadas)
9. [tar en pipelines y red](#9-tar-en-pipelines-y-red)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. ¿Qué es tar?

`tar` (**T**ape **AR**chive) es la herramienta estándar de Unix/Linux para agrupar múltiples archivos y directorios en un único archivo (llamado **tarball**). Originalmente diseñado para cintas magnéticas, hoy es la base de prácticamente toda estrategia de backup en Linux.

Un concepto fundamental: **tar NO comprime por sí mismo**. Su trabajo es crear un archivo que preserva la estructura de directorios, permisos, ownership, timestamps y (opcionalmente) ACLs y xattrs. La compresión es una capa adicional que se aplica encima.

```
Archivos sueltos           tar               Compresión
┌──────────┐          ┌──────────────┐     ┌───────────────┐
│ file1.c  │          │              │     │               │
│ file2.h  │───tar───▶│ backup.tar   │────▶│ backup.tar.gz │
│ dir/     │  (c)     │ (sin compr.) │(gz) │ (comprimido)  │
│  sub.txt │          │              │     │               │
└──────────┘          └──────────────┘     └───────────────┘
                       Metadatos:
                       - rutas relativas
                       - permisos (rwx)
                       - uid/gid
                       - mtime/atime
                       - symlinks
```

### Formato interno de un tarball

Un archivo tar es una secuencia de bloques de 512 bytes:

```
┌──────────────────────────────────────────────────────┐
│  Header block 1  (512 bytes)                         │
│  ├── filename (100 bytes)                            │
│  ├── mode, uid, gid                                  │
│  ├── size (en octal)                                 │
│  ├── mtime (en octal, epoch)                         │
│  ├── typeflag (0=file, 5=dir, 2=symlink, 1=hardlink) │
│  └── checksum, magic ("ustar"), prefix               │
├──────────────────────────────────────────────────────┤
│  Data blocks (ceil(size/512) × 512 bytes)            │
├──────────────────────────────────────────────────────┤
│  Header block 2  (512 bytes)                         │
├──────────────────────────────────────────────────────┤
│  Data blocks ...                                     │
├──────────────────────────────────────────────────────┤
│  Two zero blocks (EOF marker, 1024 bytes)            │
└──────────────────────────────────────────────────────┘
```

El formato **UStar** (POSIX.1-1988) extiende el header original para soportar nombres largos y campos adicionales. GNU tar extiende UStar aún más con bloques especiales para nombres que exceden 256 caracteres, sparse files, y metadatos extendidos.

---

## 2. Anatomía de un comando tar

```
tar [OPERACIÓN] [OPCIONES] [ARCHIVO] [RUTAS...]
```

### Operación (obligatoria, mutuamente excluyente)

| Flag | Nombre    | Descripción                           |
|------|-----------|---------------------------------------|
| `-c` | create    | Crear un nuevo archivo                |
| `-x` | extract   | Extraer contenido                     |
| `-t` | list      | Listar contenido sin extraer          |
| `-r` | append    | Añadir archivos a un tar existente    |
| `-u` | update    | Añadir solo archivos más nuevos       |
| `-d` | diff      | Comparar tar con el filesystem        |
| `--delete` | delete | Eliminar miembros del archivo      |

> **Nota**: `-r`, `-u`, `--delete` y `-d` **no funcionan** con archivos comprimidos — solo con `.tar` plano.

### Opciones comunes

| Flag | Descripción                                              |
|------|----------------------------------------------------------|
| `-f` | Especificar nombre del archivo (obligatorio en la práctica) |
| `-v` | Verbose — mostrar archivos procesados                    |
| `-z` | Comprimir/descomprimir con gzip                          |
| `-j` | Comprimir/descomprimir con bzip2                         |
| `-J` | Comprimir/descomprimir con xz                            |
| `--zstd` | Comprimir/descomprimir con zstd                     |
| `-p` | Preservar permisos (implícito como root en extracción)   |
| `-C` | Cambiar directorio antes de operar                       |
| `--exclude` | Excluir archivos por patrón                        |
| `-g` / `--listed-incremental` | Backup incremental con snapshot  |

### Estilos de invocación

tar acepta flags de tres maneras equivalentes:

```bash
# Estilo BSD (sin guión, flags agrupados)
tar czf backup.tar.gz /data

# Estilo Unix (guión simple, flags agrupados)
tar -czf backup.tar.gz /data

# Estilo GNU (flags largos)
tar --create --gzip --file=backup.tar.gz /data
```

> **Pregunta de predicción**: ¿Qué ocurre si escribes `tar cf` sin especificar un nombre de archivo después de `-f`? ¿Dónde intentará escribir tar?

Sin `-f archivo`, tar escribe a la salida estándar (o al dispositivo de cinta por defecto definido en tiempo de compilación, típicamente `/dev/st0`). Esto puede producir basura binaria en tu terminal si no rediriges la salida.

---

## 3. Operaciones fundamentales

### Crear un archivo tar

```bash
# Crear tarball sin compresión
tar cf backup.tar /home/user/documents/

# Con verbose para ver el progreso
tar cvf backup.tar /home/user/documents/

# tar elimina la / inicial por seguridad (rutas relativas)
# Advertencia: "Removing leading '/' from member names"
```

### Listar contenido

```bash
# Listar archivos dentro del tar
tar tf backup.tar

# Con detalles (permisos, tamaño, fecha)
tar tvf backup.tar

# Buscar un archivo específico dentro del tar
tar tf backup.tar | grep "config"
```

### Extraer

```bash
# Extraer todo en el directorio actual
tar xf backup.tar

# Extraer en un directorio específico
tar xf backup.tar -C /tmp/restore/

# Extraer un archivo específico
tar xf backup.tar home/user/documents/important.txt

# Extraer con verbose
tar xvf backup.tar
```

### Comparar (diff)

```bash
# Comparar tar con el estado actual del filesystem
tar df backup.tar
# Muestra diferencias en mtime, size, contenido
```

### Append y update

```bash
# Añadir un archivo nuevo al tar existente
tar rf backup.tar /home/user/newfile.txt

# Update: añadir solo si es más nuevo que la versión en el tar
tar uf backup.tar /home/user/documents/
```

> **Importante**: ni `-r` ni `-u` funcionan con archivos comprimidos (`.tar.gz`, etc.). tar tendría que descomprimir todo, modificar, y recomprimir — y no lo hace automáticamente.

---

## 4. Compresión

### Algoritmos soportados

```
                Velocidad de compresión vs Ratio
  Ratio ▲
  (mejor)│
        │                                    ● xz (-J)
        │                              ● zstd (--zstd, nivel alto)
        │                        ● bzip2 (-j)
        │                  ● zstd (--zstd, nivel default)
        │            ● gzip (-z)
        │      ● lz4 (--use-compress-program=lz4)
        │ ● sin compresión
        └────────────────────────────────────────────▶ Velocidad
                                                    (más rápido)
```

| Algoritmo | Flag tar | Extensión     | Ratio  | Velocidad      | Uso típico           |
|-----------|----------|---------------|--------|----------------|----------------------|
| gzip      | `-z`     | `.tar.gz`/`.tgz` | Medio | Rápido      | Default general      |
| bzip2     | `-j`     | `.tar.bz2`    | Bueno  | Lento          | Archivos de fuentes  |
| xz        | `-J`     | `.tar.xz`     | Mejor  | Muy lento      | Distribución, repos  |
| zstd      | `--zstd` | `.tar.zst`    | Bueno+ | Muy rápido     | Backups modernos     |
| lz4       | `--use-compress-program=lz4` | `.tar.lz4` | Bajo | Extremo | Velocidad máxima |

### Uso con cada compresor

```bash
# gzip — el más universal
tar czf backup.tar.gz /data/
tar xzf backup.tar.gz

# bzip2 — mejor ratio que gzip, más lento
tar cjf backup.tar.bz2 /data/
tar xjf backup.tar.bz2

# xz — máxima compresión
tar cJf backup.tar.xz /data/
tar xJf backup.tar.xz

# zstd — balance moderno entre ratio y velocidad
tar --zstd -cf backup.tar.zst /data/
tar --zstd -xf backup.tar.zst

# Detección automática al extraer (GNU tar)
tar xf backup.tar.gz    # detecta gzip automáticamente
tar xf backup.tar.xz    # detecta xz automáticamente
```

> **Pregunta de predicción**: Tienes un backup de 50 GB que haces diariamente y necesitas que el proceso termine en minutos, no horas. ¿Qué algoritmo elegirías y por qué?

`zstd` es la mejor opción: ofrece un ratio de compresión comparable o superior a gzip con velocidades de compresión 3-5x más rápidas. Para 50 GB, la diferencia entre zstd y xz puede ser de minutos vs horas.

### Controlar el nivel de compresión

```bash
# gzip con nivel máximo (1-9, default 6)
tar cf - /data/ | gzip -9 > backup.tar.gz

# xz con múltiples threads
tar cf - /data/ | xz -T0 -9 > backup.tar.xz

# zstd con nivel personalizado (1-19, default 3)
tar cf - /data/ | zstd -T0 -12 -o backup.tar.zst

# Usando --use-compress-program para control total
tar --use-compress-program="zstd -T0 -12" -cf backup.tar.zst /data/
```

### Comparación práctica de tamaños

```bash
# Comparar todos los compresores con el mismo dataset
for ext in tar tar.gz tar.bz2 tar.xz tar.zst; do
    case $ext in
        tar)     tar cf test.$ext /data/ ;;
        tar.gz)  tar czf test.$ext /data/ ;;
        tar.bz2) tar cjf test.$ext /data/ ;;
        tar.xz)  tar cJf test.$ext /data/ ;;
        tar.zst) tar --zstd -cf test.$ext /data/ ;;
    esac
done
ls -lh test.tar*
```

---

## 5. Backups incrementales con --listed-incremental

El backup incremental es la funcionalidad más potente de GNU tar para administración de sistemas. Permite hacer backups donde solo se guardan los archivos que cambiaron desde el último backup.

### Conceptos: full, incremental, y el snapshot file

```
Día 1: Full backup (nivel 0)
┌──────────────────────────┐
│ Todos los archivos       │  backup-full.tar.gz (grande)
│ + snapshot file creado   │  snapshot.snar (metadata)
└──────────────────────────┘

Día 2: Incremental (nivel 1)
┌──────────────────────────┐
│ Solo archivos cambiados  │  backup-inc-2.tar.gz (pequeño)
│ desde el full             │
│ + snapshot actualizado   │  snapshot.snar (actualizado)
└──────────────────────────┘

Día 3: Incremental (nivel 1)
┌──────────────────────────┐
│ Solo archivos cambiados  │  backup-inc-3.tar.gz (pequeño)
│ desde el día 2           │
│ + snapshot actualizado   │  snapshot.snar (actualizado)
└──────────────────────────┘

Restauración: aplicar en orden
  full → inc-2 → inc-3
```

### El snapshot file (.snar)

El archivo snapshot (`.snar`) es un archivo binario que GNU tar utiliza para rastrear el estado de los archivos. Contiene:

- Nombres de directorios y sus contenidos
- Timestamps (mtime, ctime) de cada archivo
- Device y inode de cada archivo

```bash
# Ver contenido del snapshot en formato legible
tar --listed-incremental=/dev/null --file=backup-inc.tar.gz -t
# El /dev/null aquí no modifica el snapshot real

# Examinar el snapshot con herramienta de debug
# Formato binario: GNU tar snapshot version 2
```

### Crear un backup full (nivel 0)

```bash
# El snapshot file NO debe existir previamente
# tar lo crea automáticamente en el nivel 0
tar --listed-incremental=/backup/snapshot.snar \
    -czf /backup/full-$(date +%Y%m%d).tar.gz \
    /data/

# Verificar que se creó el snapshot
ls -la /backup/snapshot.snar
```

### Crear backups incrementales sucesivos

```bash
# Día 2: el snapshot EXISTE, tar lee el estado anterior
# y solo incluye archivos nuevos o modificados
tar --listed-incremental=/backup/snapshot.snar \
    -czf /backup/inc-$(date +%Y%m%d).tar.gz \
    /data/

# Día 3: mismo comando, tar compara con estado del día 2
tar --listed-incremental=/backup/snapshot.snar \
    -czf /backup/inc-$(date +%Y%m%d).tar.gz \
    /data/
```

### Cómo tar decide qué incluir

```
Para cada archivo en las rutas especificadas:
  1. ¿Existe en el snapshot anterior?
     ├── NO  → incluir (archivo nuevo)
     └── SÍ  → ¿cambió ctime o mtime?
               ├── SÍ  → incluir (archivo modificado)
               └── NO  → omitir

Para cada directorio en el snapshot:
  ¿Sigue existiendo en el filesystem?
  ├── SÍ  → registrar en el nuevo snapshot
  └── NO  → marcar como eliminado en el tar
            (el tar incremental registra DELETES)
```

> **Punto crítico**: GNU tar usa **ctime** (no solo mtime) para detectar cambios. Esto significa que un `chmod` o `chown` (que cambian ctime pero no mtime) sí disparan la inclusión del archivo en el incremental. Esto es correcto para backups — quieres capturar cambios de permisos también.

### Restaurar desde incrementales

La restauración debe hacerse **en orden estricto**: primero el full, luego cada incremental en secuencia.

```bash
# 1. Restaurar el backup full
tar --listed-incremental=/dev/null \
    -xzf /backup/full-20260301.tar.gz \
    -C /restore/

# 2. Aplicar cada incremental en orden
tar --listed-incremental=/dev/null \
    -xzf /backup/inc-20260302.tar.gz \
    -C /restore/

tar --listed-incremental=/dev/null \
    -xzf /backup/inc-20260303.tar.gz \
    -C /restore/
```

> **Importante**: al extraer, se usa `--listed-incremental=/dev/null`. Esto le dice a tar que **procese las directivas de borrado** contenidas en el incremental (elimina archivos que fueron borrados entre backups). Sin este flag, los archivos borrados reaparecerían en la restauración.

### Script de backup incremental completo

```bash
#!/bin/bash
# backup_incremental.sh — backup incremental con rotación semanal

BACKUP_DIR="/backup"
SOURCE="/data"
SNAPSHOT="$BACKUP_DIR/snapshot.snar"
DATE=$(date +%Y%m%d_%H%M%S)
DOW=$(date +%u)  # 1=lunes, 7=domingo

# Lunes: full backup (eliminar snapshot previo fuerza nivel 0)
if [ "$DOW" -eq 1 ]; then
    rm -f "$SNAPSHOT"
    TYPE="full"
else
    TYPE="inc"
fi

FILENAME="$BACKUP_DIR/${TYPE}-${DATE}.tar.gz"

tar --listed-incremental="$SNAPSHOT" \
    -czf "$FILENAME" \
    "$SOURCE" 2>/dev/null

# Registrar resultado
if [ $? -eq 0 ]; then
    SIZE=$(du -h "$FILENAME" | cut -f1)
    echo "$(date): $TYPE backup OK — $FILENAME ($SIZE)" >> "$BACKUP_DIR/backup.log"
else
    echo "$(date): $TYPE backup FAILED" >> "$BACKUP_DIR/backup.log"
fi

# Limpiar backups de más de 30 días
find "$BACKUP_DIR" -name "*.tar.gz" -mtime +30 -delete
```

### Multi-level incrementals

GNU tar soporta incrementales de múltiples niveles copiando el snapshot:

```bash
# Nivel 0 — Full (domingo)
tar -g /backup/snap-level0.snar -czf full.tar.gz /data/

# Nivel 1 — Incremental diario (lunes a sábado)
# Copiar snapshot del full para que cada día compare contra el full
cp /backup/snap-level0.snar /backup/snap-level1.snar
tar -g /backup/snap-level1.snar -czf inc-mon.tar.gz /data/

# Martes: copia fresca del level 0 → compara contra full, no contra lunes
cp /backup/snap-level0.snar /backup/snap-level1.snar
tar -g /backup/snap-level1.snar -czf inc-tue.tar.gz /data/
```

```
Diferencia entre esquemas:

Incremental simple (acumulativo):
  Restaurar día 5 = full + inc1 + inc2 + inc3 + inc4 + inc5
  (más archivos por restore, pero cada incremental es más pequeño)

Diferencial (multi-level):
  Restaurar día 5 = full + inc5
  (restore rápido, pero cada incremental crece)
```

---

## 6. Preservación de metadatos y permisos

### Qué preserva tar por defecto

| Metadato           | Al crear | Al extraer (usuario) | Al extraer (root) |
|--------------------|----------|----------------------|---------------------|
| Ruta relativa      | ✅       | ✅                   | ✅                  |
| Permisos (mode)    | ✅       | Modifica con umask   | ✅ (preserva)       |
| UID/GID numérico   | ✅       | Ignora (usa el tuyo) | ✅ (restaura)       |
| Nombre user/group  | ✅       | Ignora               | ✅ (mapea por nombre)|
| mtime              | ✅       | ✅                   | ✅                  |
| Symlinks           | ✅       | ✅                   | ✅                  |
| Hardlinks          | ✅       | ✅                   | ✅                  |

### Flags para control de metadatos

```bash
# Preservar permisos exactos (sin aplicar umask) — como usuario normal
tar xpf backup.tar

# Preservar contextos SELinux y ACLs
tar --selinux --acls --xattrs -cpf backup.tar /data/

# Extraer preservando todo
tar --selinux --acls --xattrs -xpf backup.tar

# No preservar ownership (útil para restaurar como otro usuario)
tar --no-same-owner -xf backup.tar

# No preservar permisos
tar --no-same-permissions -xf backup.tar

# Forzar UID/GID numérico (no mapear por nombre)
tar --numeric-owner -cpf backup.tar /data/
# Útil cuando restauras en un sistema con distintos usuarios
```

### Timestamps

```bash
# tar preserva mtime por defecto al extraer
# Para preservar también atime del archivo original al CREAR:
tar --atime-preserve=system -cf backup.tar /data/

# Al extraer, el mtime del archivo extraído = mtime original
# El atime y ctime serán "ahora" (imposible preservar ctime)
```

---

## 7. Exclusiones y filtros

### Excluir por patrón

```bash
# Excluir un directorio específico
tar czf backup.tar.gz --exclude='*.log' /data/

# Múltiples exclusiones
tar czf backup.tar.gz \
    --exclude='*.log' \
    --exclude='*.tmp' \
    --exclude='.git' \
    --exclude='node_modules' \
    /data/

# Excluir desde un archivo
cat > /backup/excludes.txt << 'EOF'
*.log
*.tmp
*.cache
.git
node_modules
__pycache__
EOF

tar czf backup.tar.gz --exclude-from=/backup/excludes.txt /data/
```

### Patrones de exclusión

```
Patrón             Qué excluye
──────────────────────────────────────────────
*.log              Archivos que terminan en .log (en cualquier nivel)
./data/tmp/        El directorio tmp dentro de data
cache              Cualquier archivo/dir llamado "cache"
/data/tmp          Ruta exacta desde la raíz del tar
*.o                Object files de compilación
```

### Excluir por otros criterios

```bash
# Excluir archivos de control de versión
tar --exclude-vcs -czf backup.tar.gz /project/

# Excluir archivos que coincidan con .gitignore
tar --exclude-vcs-ignores --exclude-vcs -czf backup.tar.gz /project/

# Excluir archivos más grandes que un tamaño
# (no hay flag nativo, usar find)
find /data -size -100M -print0 | tar czf backup.tar.gz --null -T -

# Excluir por filesystem (no cruzar mount points)
tar --one-file-system -czf root-backup.tar.gz /
# Esto evita incluir /proc, /sys, /dev, /tmp (si están en otro FS)
```

> **Pregunta de predicción**: Estás haciendo backup de `/` con tar. ¿Qué pasa si **no** usas `--one-file-system`? ¿Qué directorios problemáticos se incluirían?

Se incluirían `/proc` (pseudo-filesystem de procesos — archivos que no son reales), `/sys` (dispositivos), `/dev` (nodos de dispositivo), `/run` (datos efímeros de runtime), y cualquier NFS montado. El tar podría crecer enormemente, fallar al leer archivos especiales, o incluso colgar intentando leer de `/proc/kcore` (que representa toda la memoria).

---

## 8. Operaciones avanzadas

### Extraer archivos específicos

```bash
# Extraer solo un archivo (la ruta debe coincidir exactamente)
tar xf backup.tar home/user/documents/report.pdf

# Extraer usando wildcard (requiere --wildcards)
tar xf backup.tar --wildcards '*.conf'

# Extraer un directorio completo
tar xf backup.tar home/user/documents/
```

### Verificar integridad

```bash
# Comparar contenido del tar con el filesystem
tar df backup.tar

# Verificar que el tar no esté corrupto (intentar listar)
tar tf backup.tar > /dev/null && echo "OK" || echo "CORRUPTO"

# Para tarballs comprimidos, verificar el compresor también
gzip -t backup.tar.gz && echo "gzip OK"
xz -t backup.tar.xz && echo "xz OK"
zstd -t backup.tar.zst && echo "zstd OK"
```

### Sparse files

```bash
# Crear tar que maneja sparse files eficientemente
tar --sparse -cf backup.tar /var/lib/libvirt/images/

# Sin --sparse, un disco virtual de 100G con 10G de datos
# generaría un tar de 100G
# Con --sparse, el tar contendrá solo los 10G de datos reales
```

### Split: dividir tarballs grandes

```bash
# Crear un tar dividido en partes de 4.7G (para DVDs o límites de FS)
tar czf - /data/ | split -b 4700m - backup.tar.gz.part

# Resultado: backup.tar.gz.partaa, backup.tar.gz.partab, ...

# Restaurar desde partes
cat backup.tar.gz.part* | tar xzf -
```

### Transformar rutas al crear/extraer

```bash
# Eliminar N componentes de la ruta al extraer
tar xf backup.tar --strip-components=1
# home/user/documents/file.txt → user/documents/file.txt

tar xf backup.tar --strip-components=2
# home/user/documents/file.txt → documents/file.txt

# Añadir prefijo al crear
tar --transform='s|^|backup-2026/|' -cf backup.tar /data/
# /data/file.txt → backup-2026/data/file.txt
```

### Leer lista de archivos desde stdin

```bash
# Usar find para seleccionar archivos y pasarlos a tar
find /data -name "*.conf" -newer /data/last_backup -print0 | \
    tar czf configs-changed.tar.gz --null -T -

# -T - : leer lista de stdin
# --null : separador NUL (coincide con find -print0)
# Esto maneja nombres con espacios y caracteres especiales
```

---

## 9. tar en pipelines y red

### Copiar directorios preservando todo

```bash
# Copiar /source a /dest preservando hardlinks, symlinks, permisos
tar cf - -C /source . | tar xpf - -C /dest/

# Equivalente a cp -a pero maneja mejor los hardlinks
# y no sigue symlinks fuera del árbol
```

### Transferir por red con SSH

```bash
# Enviar backup a servidor remoto
tar czf - /data/ | ssh user@backup-server "cat > /backups/data.tar.gz"

# Extraer directamente en el servidor remoto
tar cf - /data/ | ssh user@backup-server "tar xf - -C /backups/"

# Traer backup de un servidor remoto
ssh user@backup-server "tar czf - /remote/data/" | tar xzf - -C /local/

# Con progreso (requiere pv)
tar cf - /data/ | pv | ssh user@backup-server "tar xf - -C /backups/"
```

### Transferir por red con netcat (sin cifrado)

```bash
# En el receptor:
nc -l -p 9999 | tar xzf - -C /backups/

# En el emisor:
tar czf - /data/ | nc backup-server 9999
```

> **Nota de seguridad**: netcat transmite sin cifrar. Úsalo solo en redes confiables. Para producción, usa SSH.

---

## 10. Errores comunes

### Error 1: Olvidar -f y enviar basura a la terminal

```bash
# MAL — sin -f, tar escribe a stdout (o al dispositivo de cinta)
tar cz /data/
# Vuelca datos binarios comprimidos a la terminal

# BIEN
tar czf backup.tar.gz /data/
```

### Error 2: Confundir el orden de los flags con -f

```bash
# MAL — -f toma el siguiente argumento como nombre de archivo
tar cfz backup.tar.gz /data/
# tar interpreta "z" como el nombre del archivo → crea archivo llamado "z"

# BIEN — -f debe ser el último flag agrupado (o usar forma larga)
tar czf backup.tar.gz /data/
tar -c -z -f backup.tar.gz /data/
```

### Error 3: No usar --listed-incremental=/dev/null al restaurar incrementales

```bash
# MAL — restaurar sin el flag no procesa los borrados
tar xzf inc-20260302.tar.gz -C /restore/
# Archivos borrados entre backups reaparecen

# BIEN
tar --listed-incremental=/dev/null -xzf inc-20260302.tar.gz -C /restore/
```

### Error 4: Rutas absolutas vs relativas

```bash
# GNU tar elimina / al crear (warning)
tar czf backup.tar.gz /home/user/
# Stored as: home/user/

# Si necesitas rutas absolutas (raro, peligroso)
tar czf backup.tar.gz -P /home/user/
# -P / --absolute-names preserva la /
# PELIGRO: al extraer sobrescribe las rutas absolutas originales

# Verificar qué rutas contiene un tar antes de extraer
tar tf backup.tar.gz | head -5
```

### Error 5: Backup de / sin --one-file-system

```bash
# MAL — incluye /proc, /sys, /dev, filesystems montados
tar czf root-backup.tar.gz /

# BIEN — solo el filesystem raíz
tar czf root-backup.tar.gz --one-file-system \
    --exclude='/tmp/*' \
    --exclude='/var/tmp/*' \
    --exclude='/var/cache/*' \
    /
```

---

## 11. Cheatsheet

```
CREAR
  tar cf  arch.tar dir/              Crear sin compresión
  tar czf arch.tar.gz dir/           Crear + gzip
  tar cjf arch.tar.bz2 dir/         Crear + bzip2
  tar cJf arch.tar.xz dir/          Crear + xz
  tar --zstd -cf arch.tar.zst dir/  Crear + zstd

EXTRAER
  tar xf  arch.tar.gz               Extraer (autodetecta compresión)
  tar xf  arch.tar.gz -C /dest/     Extraer en directorio específico
  tar xf  arch.tar file.txt          Extraer un archivo específico
  tar xf  arch.tar --strip-components=1  Quitar primer nivel de ruta

LISTAR
  tar tf  arch.tar.gz               Listar contenido
  tar tvf arch.tar.gz               Listar con detalles

INCREMENTAL
  tar -g snap.snar -czf full.tar.gz dir/   Full (snap no existe)
  tar -g snap.snar -czf inc.tar.gz dir/    Incremental (snap existe)
  tar --listed-incremental=/dev/null -xzf inc.tar.gz  Restaurar inc.

METADATOS
  tar --selinux --acls --xattrs -cpf arch.tar dir/  Preservar todo
  tar --numeric-owner -cpf arch.tar dir/    UID/GID numérico
  tar xpf arch.tar                          Preservar permisos (usuario)

EXCLUSIONES
  tar czf arch.tar.gz --exclude='*.log' dir/      Excluir patrón
  tar czf arch.tar.gz --exclude-from=file dir/     Excluir desde lista
  tar czf arch.tar.gz --exclude-vcs dir/            Excluir .git/.svn
  tar czf arch.tar.gz --one-file-system /           No cruzar FS

RED
  tar cf - dir/ | ssh host "tar xf - -C /dest/"    Copiar por SSH
  tar cf - -C /src . | tar xpf - -C /dest/         Copia local

VERIFICAR
  tar df arch.tar                    Comparar con filesystem
  tar tf arch.tar > /dev/null        Verificar integridad
  gzip -t arch.tar.gz               Verificar compresión
```

---

## 12. Ejercicios

### Ejercicio 1: Backup completo con exclusiones

**Objetivo**: Crear un backup completo de un directorio de proyecto excluyendo artefactos innecesarios.

```bash
# 1. Crear estructura de prueba
mkdir -p /tmp/project/{src,build,docs,.git/objects,node_modules/pkg}
echo "int main() { return 0; }" > /tmp/project/src/main.c
echo "important" > /tmp/project/docs/spec.txt
dd if=/dev/urandom of=/tmp/project/build/app bs=1K count=100 2>/dev/null
dd if=/dev/urandom of=/tmp/project/node_modules/pkg/data bs=1K count=50 2>/dev/null
echo "gitobj" > /tmp/project/.git/objects/abc123

# 2. Verificar tamaño total
du -sh /tmp/project/

# 3. Crear backup excluyendo build, .git y node_modules
tar --exclude-vcs \
    --exclude='build' \
    --exclude='node_modules' \
    -czvf /tmp/project-backup.tar.gz \
    -C /tmp project/

# 4. Listar contenido del backup
tar tvf /tmp/project-backup.tar.gz
```

**Pregunta de reflexión**: ¿Por qué `--exclude-vcs` solo excluye `.git` pero no `build`? ¿Qué directorios específicos conoce `--exclude-vcs`?

> `--exclude-vcs` conoce los directorios de VCS estándar: `.git`, `.svn`, `.hg`, `.bzr`, `.cvsignore`, y sus archivos asociados. `build` y `node_modules` son convenciones de proyecto, no de VCS, así que requieren `--exclude` explícito.

---

### Ejercicio 2: Ciclo incremental completo

**Objetivo**: Simular un ciclo semanal de backups incrementales y restaurar un estado intermedio.

```bash
# 1. Crear datos iniciales
mkdir -p /tmp/inctest/data
echo "original file A" > /tmp/inctest/data/fileA.txt
echo "original file B" > /tmp/inctest/data/fileB.txt
echo "original file C" > /tmp/inctest/data/fileC.txt
mkdir -p /tmp/inctest/backups

# 2. Día 1: Full backup
tar --listed-incremental=/tmp/inctest/backups/snap.snar \
    -czf /tmp/inctest/backups/day1-full.tar.gz \
    -C /tmp/inctest data/
echo "--- Contenido del full:"
tar tf /tmp/inctest/backups/day1-full.tar.gz

# 3. Día 2: Modificar archivo A, crear D
echo "modified file A" > /tmp/inctest/data/fileA.txt
echo "new file D" > /tmp/inctest/data/fileD.txt
tar --listed-incremental=/tmp/inctest/backups/snap.snar \
    -czf /tmp/inctest/backups/day2-inc.tar.gz \
    -C /tmp/inctest data/
echo "--- Contenido del incremental día 2:"
tar tf /tmp/inctest/backups/day2-inc.tar.gz

# 4. Día 3: Borrar archivo C, modificar B
rm /tmp/inctest/data/fileC.txt
echo "modified file B" > /tmp/inctest/data/fileB.txt
tar --listed-incremental=/tmp/inctest/backups/snap.snar \
    -czf /tmp/inctest/backups/day3-inc.tar.gz \
    -C /tmp/inctest data/
echo "--- Contenido del incremental día 3:"
tar tf /tmp/inctest/backups/day3-inc.tar.gz

# 5. Restaurar al estado del día 3
mkdir -p /tmp/inctest/restore
tar --listed-incremental=/dev/null \
    -xzf /tmp/inctest/backups/day1-full.tar.gz \
    -C /tmp/inctest/restore/
tar --listed-incremental=/dev/null \
    -xzf /tmp/inctest/backups/day2-inc.tar.gz \
    -C /tmp/inctest/restore/
tar --listed-incremental=/dev/null \
    -xzf /tmp/inctest/backups/day3-inc.tar.gz \
    -C /tmp/inctest/restore/

# 6. Verificar
echo "--- Estado restaurado:"
ls -la /tmp/inctest/restore/data/
cat /tmp/inctest/restore/data/fileA.txt   # Debería ser "modified"
cat /tmp/inctest/restore/data/fileB.txt   # Debería ser "modified"
ls /tmp/inctest/restore/data/fileC.txt    # NO debería existir
cat /tmp/inctest/restore/data/fileD.txt   # Debería existir
```

**Pregunta de reflexión**: Si restauras el full y el day3-inc (saltando day2-inc), ¿cuál sería el estado de `fileA.txt`? ¿Y de `fileD.txt`?

> `fileA.txt` tendría el contenido "original file A" (la modificación del día 2 se perdería porque day3-inc solo contiene cambios del día 2 al 3, no del 1 al 3). `fileD.txt` **no existiría** porque fue creado en el día 2 y el day3-inc no lo incluye (no cambió entre día 2 y 3). Esto demuestra por qué **el orden de restauración es crítico** y no se pueden saltar incrementales.

---

### Ejercicio 3: Backup de sistema con verificación

**Objetivo**: Crear un backup completo de `/etc` con preservación de metadatos y verificar su integridad.

```bash
# 1. Crear backup de /etc preservando todo
sudo tar --selinux --acls --xattrs \
    --numeric-owner \
    -czf /tmp/etc-backup-$(date +%Y%m%d).tar.gz \
    /etc/ 2>/dev/null

# 2. Verificar integridad del archivo comprimido
gzip -t /tmp/etc-backup-*.tar.gz && echo "Compresión OK" || echo "CORRUPTO"

# 3. Verificar que el tar se puede leer completamente
tar tf /tmp/etc-backup-*.tar.gz | wc -l
echo "archivos en el backup"

# 4. Comparar con el filesystem actual
sudo tar --diff -f /tmp/etc-backup-*.tar.gz 2>/dev/null | head -20
# Los archivos que cambiaron desde el backup aparecen aquí

# 5. Extraer en un directorio temporal para inspección
mkdir -p /tmp/etc-verify
tar xzf /tmp/etc-backup-*.tar.gz -C /tmp/etc-verify/ --strip-components=1

# 6. Comparar un archivo específico
diff /etc/hostname /tmp/etc-verify/hostname

# 7. Verificar tamaño
ls -lh /tmp/etc-backup-*.tar.gz
du -sh /etc/ 2>/dev/null
```

**Pregunta de reflexión**: ¿Por qué se usa `--numeric-owner` en el backup de `/etc`? ¿Qué podría salir mal si restauras el backup en un sistema diferente sin este flag?

> Sin `--numeric-owner`, tar almacena los nombres de usuario/grupo (como "root", "www-data"). Al restaurar en otro sistema, tar intenta mapear esos nombres a UIDs/GIDs locales. Si `www-data` tiene UID 33 en el sistema original pero UID 48 en el destino, los archivos se asignarían al UID 48 — cambiando los permisos efectivos. Con `--numeric-owner`, tar preserva el UID/GID numérico exacto, lo cual es más seguro para restauraciones cross-system.

---

> **Siguiente tema**: T02 — rsync: sincronización, `--delete`, `-a`, exclusiones, backups remotos con SSH.
