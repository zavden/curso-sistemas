# Estrategias de Backup — Completo, Incremental, Diferencial, Rotación y 3-2-1

## Índice

1. [¿Por qué necesitas una estrategia?](#1-por-qué-necesitas-una-estrategia)
2. [Tipos de backup](#2-tipos-de-backup)
3. [Comparativa detallada](#3-comparativa-detallada)
4. [Esquemas de rotación](#4-esquemas-de-rotación)
5. [La regla 3-2-1 (y sus extensiones)](#5-la-regla-3-2-1-y-sus-extensiones)
6. [RPO y RTO: los números que importan](#6-rpo-y-rto-los-números-que-importan)
7. [Diseñar una estrategia completa](#7-diseñar-una-estrategia-completa)
8. [Implementación con herramientas Linux](#8-implementación-con-herramientas-linux)
9. [Verificación y testing de backups](#9-verificación-y-testing-de-backups)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. ¿Por qué necesitas una estrategia?

Tener backups no es lo mismo que tener una **estrategia** de backup. Un disco externo con una copia de hace 6 meses no te salva de un ransomware de ayer. Un backup diario en el mismo servidor no te protege si el servidor se incendia.

```
"No backup" vs "backup sin estrategia" vs "estrategia de backup"

Sin backup:
  Fallo ──▶ Pérdida total

Backup sin estrategia:
  Fallo ──▶ ¿Dónde está el backup?
            ¿Cuándo se hizo?
            ¿Funciona?
            ¿Qué incluye?
            ¿Cuánto tardo en restaurar?

Estrategia de backup:
  Fallo ──▶ Identificar último backup válido
            Restaurar según procedimiento documentado
            Tiempo de recuperación predecible
            Pérdida de datos acotada y conocida
```

### Las preguntas que debe responder una estrategia

```
1. ¿QUÉ datos se respaldan?
   (todo el sistema, solo /home, bases de datos, configs)

2. ¿CON QUÉ FRECUENCIA?
   (cada hora, diario, semanal)

3. ¿DÓNDE se almacenan?
   (local, remoto, nube, cinta)

4. ¿CUÁNTO TIEMPO se retienen?
   (7 días, 30 días, 1 año, indefinido)

5. ¿CUÁNTOS DATOS puedo permitirme perder?
   (RPO: Recovery Point Objective)

6. ¿CUÁNTO TIEMPO puedo estar caído?
   (RTO: Recovery Time Objective)

7. ¿QUIÉN es responsable?
   (automatizado, sysadmin, equipo de ops)

8. ¿CÓMO se verifica que funcione?
   (testing periódico de restauración)
```

---

## 2. Tipos de backup

### Full (completo)

Copia **todos** los datos seleccionados, independientemente de si cambiaron.

```
Día 1: Full
┌───────────────────────────────┐
│ A  B  C  D  E  F  G  H  I  J │  100 archivos → 10 GB
└───────────────────────────────┘

Día 2: Full
┌───────────────────────────────┐
│ A  B  C' D  E  F  G  H  I  J │  100 archivos → 10 GB
└───────────────────────────────┘  (C cambió, pero se copian todos)

Restaurar cualquier día: 1 operación
Espacio total: 10 GB × N días
```

**Ventajas**: restauración más simple y rápida (un solo archivo/set), independiente de backups anteriores.

**Desventajas**: consume más espacio y ancho de banda, más tiempo de ejecución.

### Incremental

Copia solo los archivos que cambiaron **desde el último backup** (sea full o incremental).

```
Lunes:    Full
┌───────────────────────────────┐
│ A  B  C  D  E  F  G  H  I  J │  100 archivos → 10 GB
└───────────────────────────────┘

Martes:   Incremental (cambios desde lunes)
┌─────┐
│ C'  │  1 archivo cambió → 100 MB
└─────┘

Miércoles: Incremental (cambios desde martes)
┌────────┐
│ A' F'  │  2 archivos cambiaron → 200 MB
└────────┘

Jueves:   Incremental (cambios desde miércoles)
┌─────┐
│ F'' │  1 archivo cambió → 100 MB
└─────┘

Restaurar jueves: Full + Inc(mar) + Inc(mié) + Inc(jue)
                  4 operaciones en orden estricto
Espacio total: 10 GB + 100 MB + 200 MB + 100 MB = ~10.4 GB
```

**Ventajas**: mínimo espacio y tiempo de backup diario.

**Desventajas**: restauración lenta (debe aplicar toda la cadena en orden), si un incremental intermedio se corrompe, los posteriores son inútiles.

### Diferencial

Copia todos los archivos que cambiaron **desde el último full**.

```
Lunes:    Full
┌───────────────────────────────┐
│ A  B  C  D  E  F  G  H  I  J │  100 archivos → 10 GB
└───────────────────────────────┘

Martes:   Diferencial (cambios desde el full del lunes)
┌─────┐
│ C'  │  1 archivo cambió → 100 MB
└─────┘

Miércoles: Diferencial (cambios desde el full del lunes)
┌──────────┐
│ C' A' F' │  3 archivos cambiaron desde el full → 300 MB
└──────────┘  (incluye C' otra vez porque cambió desde el full)

Jueves:   Diferencial (cambios desde el full del lunes)
┌──────────────┐
│ C' A' F' F'' │  4 cambios desde el full → 400 MB
└──────────────┘  (crece cada día)

Restaurar jueves: Full + Dif(jue)
                  Solo 2 operaciones
Espacio total: 10 GB + 100 MB + 300 MB + 400 MB = ~10.8 GB
```

**Ventajas**: restauración rápida (solo full + último diferencial), tolerante a fallos (si un diferencial se corrompe, los otros siguen sirviendo).

**Desventajas**: cada diferencial crece respecto al anterior (acumula todos los cambios desde el full), usa más espacio que incremental.

### Diagrama comparativo de los tres tipos

```
          Backup                          Restauración
          ──────                          ────────────

Full:     [████████████]                  [████████████]
          [████████████]                  Solo el último full
          [████████████]
          Cada uno es grande              1 paso

Incr:     [████████████]  Full            [████████████]  Full
          [██]            Inc 1           [██]            Inc 1
          [███]           Inc 2     +     [███]           Inc 2
          [█]             Inc 3           [█]             Inc 3
          Cada uno es pequeño             Todos en orden (N pasos)

Dif:      [████████████]  Full            [████████████]  Full
          [██]            Dif 1     +     [████]          Último dif
          [███]           Dif 2
          [████]          Dif 3
          Crece cada día                  2 pasos
```

---

## 3. Comparativa detallada

### Tabla comparativa

| Criterio | Full | Incremental | Diferencial |
|----------|------|-------------|-------------|
| Velocidad de backup | Lenta | Muy rápida | Rápida (crece) |
| Espacio de almacenamiento | Máximo | Mínimo | Medio |
| Velocidad de restauración | Muy rápida | Lenta | Rápida |
| Complejidad de restauración | 1 paso | N pasos (orden estricto) | 2 pasos |
| Riesgo si un backup falla | Solo ese día | Cadena rota | Solo ese diferencial |
| Ancho de banda (remoto) | Máximo | Mínimo | Medio |
| Ventana de backup | Grande | Pequeña | Pequeña-media |

### Cuándo usar cada tipo

```
Full diario:
  ├── Datos pequeños (< 50 GB)
  ├── Restauración rápida es crítica
  └── Almacenamiento barato y abundante

Incremental:
  ├── Datos grandes (TB)
  ├── Ventana de backup limitada (noche corta)
  ├── Ancho de banda limitado (backups remotos)
  └── Puedes tolerar restauración lenta

Diferencial:
  ├── Balance entre espacio y velocidad de restore
  ├── Datos medianos (cientos de GB)
  └── No puedes permitirte cadenas largas de incrementales
```

### Esquemas combinados (lo más común en producción)

```
Full semanal + Incremental diario:
  Dom: [FULL████████████]
  Lun: [Inc██]
  Mar: [Inc█]
  Mié: [Inc███]
  Jue: [Inc██]
  Vie: [Inc█]
  Sáb: [Inc████]
  Dom: [FULL████████████]  ← nuevo ciclo

  Restaurar viernes = Full(dom) + Inc(lun) + Inc(mar) + Inc(mié) + Inc(jue) + Inc(vie)
  Máximo 7 pasos

Full semanal + Diferencial diario:
  Dom: [FULL████████████]
  Lun: [Dif██]
  Mar: [Dif███]
  Mié: [Dif████]
  Jue: [Dif█████]
  Vie: [Dif██████]
  Sáb: [Dif███████]
  Dom: [FULL████████████]  ← nuevo ciclo

  Restaurar viernes = Full(dom) + Dif(vie)
  Siempre 2 pasos
```

> **Pregunta de predicción**: Tienes 1 TB de datos donde cambia un 2% diario. ¿Cuánto espacio total consumen 7 días de backups con cada esquema: full diario, full+incremental, full+diferencial?

Full diario: 7 × 1 TB = **7 TB**. Full+incremental: 1 TB + 6 × 20 GB = **1.12 TB**. Full+diferencial: 1 TB + 20 GB + 40 GB + 60 GB + 80 GB + 100 GB + 120 GB = **1.42 TB**. La diferencia es dramática: el incremental usa 6x menos espacio que el full diario.

---

## 4. Esquemas de rotación

### Rotación simple (FIFO)

Mantener los últimos N backups y borrar los más antiguos.

```
Retención: 7 días

Día 1:  [Backup_01] [_________] [_________] ... [_________]
Día 2:  [Backup_01] [Backup_02] [_________] ... [_________]
...
Día 7:  [Backup_01] [Backup_02] [Backup_03] ... [Backup_07]
Día 8:  [Backup_02] [Backup_03] [Backup_04] ... [Backup_08]
        ↑ eliminado                               ↑ nuevo

Simple pero sin historial a largo plazo
```

### GFS: Grandfather-Father-Son

El esquema más usado en producción. Combina retención diaria, semanal y mensual.

```
Grandfather-Father-Son (GFS):

  Son (diario):          retener 7 días
  Father (semanal):      retener 4 semanas
  Grandfather (mensual): retener 12 meses

  Calendario:
  ┌──────┬──────┬──────┬──────┬──────┬──────┬──────┐
  │ Lun  │ Mar  │ Mié  │ Jue  │ Vie  │ Sáb  │ Dom  │
  │ Son  │ Son  │ Son  │ Son  │Father│ Son  │ Son  │
  └──────┴──────┴──────┴──────┴──────┴──────┴──────┘
  Primer viernes del mes = Grandfather (se retiene 12 meses)

  En cualquier momento tienes:
  ├── 7 backups diarios   (última semana)
  ├── 4 backups semanales  (último mes)
  └── 12 backups mensuales (último año)
  Total: 23 sets de backup → cubre 1 año completo
```

```
Retención en el tiempo:

Hoy─────────────────────────────────────────▶ Hace 1 año
│ D D D D D D D │ S   S   S   S │ M  M  M  M  M  M  M  M  M  M  M  M │
│  7 diarios    │  4 semanales  │           12 mensuales                │
│ granularidad  │ granularidad  │       granularidad mensual            │
│   de 1 día    │  de 1 semana  │                                      │

Cuanto más atrás, menos granularidad, pero siempre hay un punto
de restauración.
```

### Tower of Hanoi

Rotación más eficiente en espacio, pero más compleja de implementar. Basada en el puzzle matemático:

```
Nivel A (cada 2 días):  ██  ██  ██  ██  ██  ██  ██  ██
Nivel B (cada 4 días):  ████    ████    ████    ████
Nivel C (cada 8 días):  ████████        ████████
Nivel D (cada 16 días): ████████████████

Con 4 cintas/destinos, cubre 16 días con distribución logarítmica.
Raramente usado manualmente — más común en software de backup enterprise.
```

### Implementar GFS con un script

```bash
#!/bin/bash
# gfs_backup.sh — Grandfather-Father-Son rotation

SOURCE="/data"
BACKUP_DIR="/backup"
DATE=$(date +%Y-%m-%d)
DOW=$(date +%u)     # 1=lunes ... 7=domingo
DOM=$(date +%d)      # día del mes

# Determinar tipo de backup
if [ "$DOM" -le 7 ] && [ "$DOW" -eq 5 ]; then
    # Primer viernes del mes = Grandfather (mensual)
    TYPE="monthly"
    RETENTION=365
elif [ "$DOW" -eq 5 ]; then
    # Otros viernes = Father (semanal)
    TYPE="weekly"
    RETENTION=28
else
    # Resto de días = Son (diario)
    TYPE="daily"
    RETENTION=7
fi

DEST="$BACKUP_DIR/$TYPE/$DATE"
mkdir -p "$DEST"

# Crear backup (ejemplo con rsync --link-dest)
LATEST=$(ls -1d "$BACKUP_DIR"/*/????-??-?? 2>/dev/null | tail -1)
if [ -n "$LATEST" ]; then
    rsync -aHAX --delete --link-dest="$LATEST" "$SOURCE/" "$DEST/"
else
    rsync -aHAX "$SOURCE/" "$DEST/"
fi

# Aplicar retención por tipo
find "$BACKUP_DIR/$TYPE" -maxdepth 1 -type d -mtime +$RETENTION -exec rm -rf {} +

echo "$(date): $TYPE backup → $DEST" >> "$BACKUP_DIR/backup.log"
```

---

## 5. La regla 3-2-1 (y sus extensiones)

### La regla 3-2-1 original

```
3 copias de los datos
  ├── 1 original (producción)
  ├── 1 backup local
  └── 1 backup offsite

2 tipos de medio diferentes
  ├── Disco duro local
  └── Cinta / nube / disco remoto

1 copia fuera del sitio (offsite)
  └── Protege contra: incendio, robo, inundación,
      ransomware que cifra la red local
```

```
Ejemplo práctico de 3-2-1:

                    ┌──────────────────┐
                    │  Servidor prod   │  ← Copia 1 (original)
                    │  /data/          │
                    └────────┬─────────┘
                             │
              ┌──────────────┴──────────────┐
              │                             │
    ┌─────────▼──────────┐      ┌──────────▼──────────┐
    │  NAS local         │      │  Servidor remoto    │
    │  (disco, medio 1)  │      │  (nube/colo)        │
    │  rsync diario      │      │  rsync + cifrado    │
    │                    │      │                     │
    │  Copia 2           │      │  Copia 3 (offsite)  │
    │  (local)           │      │  (medio 2)          │
    └────────────────────┘      └─────────────────────┘
```

### Extensiones modernas

#### 3-2-1-1-0

```
3 copias
2 medios diferentes
1 offsite
1 offline / air-gapped / inmutable
  └── Protege contra ransomware que cifra TODO lo conectado a la red
      (cintas desconectadas, backup en nube con object lock)
0 errores de verificación
  └── Testing periódico de restauración con 0 errores
```

#### Por qué importa el "air-gap"

```
Ransomware sofisticado:

1. Infecta el servidor de producción
2. Se propaga por la red al NAS
3. Busca y cifra backups en servidores remotos accesibles
4. ENTONCES cifra producción y pide rescate

Con air-gap:
  La copia offline (cinta, disco desconectado, cloud inmutable)
  NO es accesible desde la red → el ransomware no puede cifrarla
```

### Medios de almacenamiento

```
Medio              Velocidad    Coste/TB    Durabilidad    Offsite
─────────────────────────────────────────────────────────────────────
HDD local          Rápido       Bajo        5-10 años      No
HDD USB externo    Medio        Bajo        5-10 años      Sí (manual)
NAS (RAID)         Rápido       Medio       Alta (redund.) No
SSD                Muy rápido   Alto        5-10 años      No
Cinta LTO          Lento        Muy bajo    30+ años       Sí
Cloud (S3/GCS)     Variable     Medio       Alta           Sí
Cloud Glacier      Lento        Muy bajo    Alta           Sí
Blu-ray M-DISC     Lento        Alto        1000+ años     Sí
```

---

## 6. RPO y RTO: los números que importan

### RPO — Recovery Point Objective

**¿Cuántos datos puedo permitirme perder?** Expresado en tiempo: si el RPO es 1 hora, puedes perder como máximo los datos de la última hora.

```
RPO = tiempo entre backups

  RPO 24h (backup diario):
  ──────────────────────────────────────────▶
  Backup     Backup     Backup     ← FALLO
  00:00      00:00      00:00      23:00
                                   │←─────│
                                   Hasta 23h de
                                   datos perdidos

  RPO 1h (backup cada hora):
  ──────────────────────────────────────────▶
  ... Bk  Bk  Bk  Bk  Bk  Bk  ← FALLO
      18  19  20  21  22  23    23:45
                                │←──│
                                Hasta 45min de
                                datos perdidos
```

### RTO — Recovery Time Objective

**¿Cuánto tiempo puedo estar caído?** El tiempo desde que ocurre el fallo hasta que el servicio está restaurado y operativo.

```
RTO = tiempo de recuperación total

  Incluye:
  ├── Detectar el fallo
  ├── Decidir qué backup restaurar
  ├── Transferir el backup (si es remoto)
  ├── Restaurar datos
  ├── Verificar integridad
  └── Poner el servicio en línea

  RTO ≠ solo "tiempo de restaurar el tar"
```

### Relación RPO/RTO con estrategias

| RPO/RTO | Estrategia típica | Coste |
|---------|-------------------|-------|
| RPO 24h / RTO 8h | Full semanal + incremental diario, cinta | Bajo |
| RPO 1h / RTO 2h | Incremental horario a NAS + offsite diario | Medio |
| RPO 15min / RTO 30min | Replicación de BD + snapshots de FS | Alto |
| RPO ~0 / RTO ~0 | Replicación síncrona + failover automático | Muy alto |

> **Pregunta de predicción**: Una tienda online factura €10,000/hora. ¿Cuánto cuesta un RTO de 8 horas? ¿Justifica invertir en infraestructura que reduzca el RTO a 1 hora?

8 horas de caída = €80,000 de pérdida directa (más daño reputacional). Reducir el RTO a 1 hora = €10,000 de pérdida máxima. Si la infraestructura adicional cuesta menos de €70,000/año (la diferencia), la inversión se justifica. En la práctica, el coste reputacional suele superar ampliamente el coste directo, haciendo la inversión aún más clara.

---

## 7. Diseñar una estrategia completa

### Paso 1: Clasificar los datos

```
Clase A — Críticos (pérdida inaceptable):
  ├── Bases de datos de producción
  ├── Datos de clientes
  ├── Código fuente (si no está en VCS remoto)
  └── Certificados y claves criptográficas

Clase B — Importantes (pérdida costosa):
  ├── Configuraciones de servidores (/etc/)
  ├── Logs de auditoría
  └── Documentación interna

Clase C — Reemplazables (pérdida molesta):
  ├── Caches, archivos temporales
  ├── Paquetes descargables
  └── Datos derivados (se pueden recalcular)
```

### Paso 2: Definir RPO/RTO por clase

```
Clase A: RPO 1h,  RTO 2h   → replicación + backup horario
Clase B: RPO 24h, RTO 8h   → backup diario
Clase C: RPO ∞,   RTO ∞    → no respaldar (se recrea)
```

### Paso 3: Elegir tipo, rotación y retención

```
Clase A:
  ├── Full semanal (domingo 02:00)
  ├── Incremental cada hora
  ├── Replicación de BD en tiempo real
  ├── Rotación GFS: 7d + 4s + 12m
  └── Offsite: replicación a segundo datacenter

Clase B:
  ├── Full semanal (domingo 03:00)
  ├── Diferencial diario (03:00)
  ├── Rotación: 14 días
  └── Offsite: copia semanal a S3
```

### Paso 4: Documentar el procedimiento de restauración

```
Documentar para CADA clase:
  1. Dónde están los backups (ruta, servidor, credenciales)
  2. Comandos exactos para restaurar
  3. Orden de restauración (si hay incrementales)
  4. Verificaciones post-restauración
  5. Quién tiene permiso para restaurar
  6. Contactos de escalación
```

### Ejemplo de estrategia documentada

```
ESTRATEGIA DE BACKUP — servidor web-prod-01
═══════════════════════════════════════════

Datos protegidos:
  /var/www/          Aplicación web (Clase B)
  /etc/              Configuración (Clase B)
  PostgreSQL         Base de datos (Clase A)

Schedule:
  02:00  pg_dump de PostgreSQL (diario, comprimido)
  03:00  rsync incremental de /var/www/ y /etc/ al NAS
  Domingo 02:00: rsync full (nuevo --link-dest snapshot)
  Domingo 04:00: copia cifrada a S3 (GPG + aws s3 sync)

Retención:
  NAS local: 14 días de diarios + 8 semanales
  S3: 4 semanales + 12 mensuales
  PostgreSQL WAL: 7 días de point-in-time recovery

Restauración de BD:
  1. Detener aplicación
  2. pg_restore -d dbname /backup/pg/latest.dump
  3. Verificar: psql -c "SELECT count(*) FROM users;"
  4. Iniciar aplicación, monitorizar logs 10 min

Testing:
  Primer lunes de cada mes: restaurar en servidor de staging
```

---

## 8. Implementación con herramientas Linux

### Full + Incremental con tar

```bash
#!/bin/bash
# tar_incremental.sh

BACKUP_DIR="/backup/tar"
SOURCE="/data"
SNAPSHOT="$BACKUP_DIR/snapshot.snar"
DATE=$(date +%Y%m%d_%H%M)
DOW=$(date +%u)

mkdir -p "$BACKUP_DIR"

if [ "$DOW" -eq 7 ]; then
    rm -f "$SNAPSHOT"
    PREFIX="full"
else
    PREFIX="inc"
fi

tar --listed-incremental="$SNAPSHOT" \
    --one-file-system \
    --exclude='*.tmp' \
    --exclude='*.cache' \
    -czf "$BACKUP_DIR/${PREFIX}-${DATE}.tar.gz" \
    "$SOURCE"

# Retención
find "$BACKUP_DIR" -name "*.tar.gz" -mtime +30 -delete
```

### Mirror + Snapshots con rsync

```bash
#!/bin/bash
# rsync_snapshot.sh

SOURCE="/data/"
SNAP_DIR="/backup/snapshots"
DATE=$(date +%Y-%m-%d_%H%M)
LATEST="$SNAP_DIR/latest"

mkdir -p "$SNAP_DIR"

rsync -aHAX --delete \
    --link-dest="$LATEST" \
    "$SOURCE" "$SNAP_DIR/$DATE/"

rm -f "$LATEST"
ln -s "$SNAP_DIR/$DATE" "$LATEST"

# GFS retention
find "$SNAP_DIR" -maxdepth 1 -type d -name "????-??-??_*" -mtime +7 | \
    while read dir; do
        DAY=$(basename "$dir" | cut -d_ -f1)
        DOW=$(date -d "$DAY" +%u 2>/dev/null)
        DOM=$(date -d "$DAY" +%d 2>/dev/null)
        # Mantener viernes (weeklies) hasta 28 días
        if [ "$DOW" = "5" ] && [ "$(find "$dir" -maxdepth 0 -mtime -28)" ]; then
            continue
        fi
        # Mantener primer día del mes (monthlies) hasta 365 días
        if [ "$DOM" = "01" ] && [ "$(find "$dir" -maxdepth 0 -mtime -365)" ]; then
            continue
        fi
        rm -rf "$dir"
    done
```

### Backup de base de datos

```bash
#!/bin/bash
# db_backup.sh

BACKUP_DIR="/backup/db"
DATE=$(date +%Y%m%d_%H%M)
RETENTION=14

mkdir -p "$BACKUP_DIR"

# PostgreSQL
pg_dump -Fc mydb > "$BACKUP_DIR/mydb-${DATE}.dump"

# MySQL/MariaDB
mysqldump --single-transaction --routines --triggers \
    mydb | gzip > "$BACKUP_DIR/mydb-${DATE}.sql.gz"

# Verificar integridad del dump de PostgreSQL
pg_restore --list "$BACKUP_DIR/mydb-${DATE}.dump" > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "ERROR: dump corrupto" >&2
    exit 1
fi

# Retención
find "$BACKUP_DIR" -name "*.dump" -mtime +$RETENTION -delete
find "$BACKUP_DIR" -name "*.sql.gz" -mtime +$RETENTION -delete
```

### Backup cifrado para offsite

```bash
#!/bin/bash
# encrypted_offsite.sh

SOURCE="/backup/snapshots/latest/"
DEST="/backup/offsite"
DATE=$(date +%Y%m%d)
GPG_RECIPIENT="backup@empresa.com"

mkdir -p "$DEST"

# Crear tarball cifrado
tar czf - -C "$SOURCE" . | \
    gpg --encrypt --recipient "$GPG_RECIPIENT" \
    > "$DEST/backup-${DATE}.tar.gz.gpg"

# Subir a S3 (ejemplo)
# aws s3 cp "$DEST/backup-${DATE}.tar.gz.gpg" s3://mi-bucket-backup/

# Restaurar:
# gpg --decrypt backup-20260321.tar.gz.gpg | tar xzf - -C /restore/
```

### Automatización con systemd timer

```ini
# /etc/systemd/system/backup.service
[Unit]
Description=Daily backup
After=network.target

[Service]
Type=oneshot
ExecStart=/usr/local/bin/rsync_snapshot.sh
User=root
Nice=19
IOSchedulingClass=idle
```

```ini
# /etc/systemd/system/backup.timer
[Unit]
Description=Run backup daily at 03:00

[Timer]
OnCalendar=*-*-* 03:00:00
Persistent=true
RandomizedDelaySec=300

[Install]
WantedBy=timers.target
```

```bash
sudo systemctl enable --now backup.timer
systemctl list-timers backup.timer
```

---

## 9. Verificación y testing de backups

### Un backup sin verificar no es un backup

```
"Schrödinger's Backup":
  El estado de un backup es DESCONOCIDO hasta que intentas restaurarlo.
  Es simultáneamente un backup válido y un archivo corrupto
  hasta que lo verificas.
```

### Niveles de verificación

```
Nivel 1 — ¿El backup se ejecutó?
  ├── Verificar exit code del script
  ├── Verificar que el archivo de backup existe
  └── Verificar que el tamaño es razonable (no 0 bytes)

Nivel 2 — ¿El archivo está íntegro?
  ├── gzip -t / xz -t / zstd -t (verificar compresión)
  ├── tar tf (verificar que se puede leer el índice)
  └── Comparar checksum si hay uno guardado

Nivel 3 — ¿Los datos se pueden restaurar?
  ├── Extraer en un directorio temporal
  ├── Verificar que archivos críticos existen y son legibles
  └── Para BD: pg_restore --list, mysqlcheck

Nivel 4 — ¿El servicio funciona tras restaurar?
  ├── Restaurar en un servidor de staging
  ├── Arrancar la aplicación
  └── Ejecutar pruebas de smoke
```

### Script de verificación

```bash
#!/bin/bash
# verify_backup.sh

BACKUP_DIR="/backup/snapshots/latest"
LOG="/var/log/backup-verify.log"

ERRORS=0

echo "=== Verificación $(date) ===" >> "$LOG"

# 1. ¿El directorio existe y tiene contenido?
if [ ! -d "$BACKUP_DIR" ] || [ -z "$(ls -A "$BACKUP_DIR")" ]; then
    echo "FAIL: backup directory empty or missing" >> "$LOG"
    ERRORS=$((ERRORS + 1))
fi

# 2. ¿La antigüedad es aceptable? (< 25 horas)
LATEST_FILE=$(find "$BACKUP_DIR" -type f -printf '%T@\n' | sort -rn | head -1)
NOW=$(date +%s)
AGE=$(echo "($NOW - ${LATEST_FILE%.*}) / 3600" | bc)
if [ "$AGE" -gt 25 ]; then
    echo "FAIL: latest backup is ${AGE}h old (> 25h)" >> "$LOG"
    ERRORS=$((ERRORS + 1))
else
    echo "OK: backup age ${AGE}h" >> "$LOG"
fi

# 3. ¿Archivos críticos presentes?
for CRITICAL in etc/passwd etc/shadow etc/fstab; do
    if [ ! -f "$BACKUP_DIR/$CRITICAL" ]; then
        echo "FAIL: missing $CRITICAL" >> "$LOG"
        ERRORS=$((ERRORS + 1))
    fi
done

# 4. Resultado
if [ "$ERRORS" -eq 0 ]; then
    echo "RESULT: ALL CHECKS PASSED" >> "$LOG"
else
    echo "RESULT: $ERRORS CHECKS FAILED" >> "$LOG"
    # Enviar alerta (ejemplo con mail)
    # echo "Backup verification failed" | mail -s "BACKUP ALERT" admin@empresa.com
fi

exit $ERRORS
```

### Calendario de testing

```
Verificación automática (diaria):
  ├── Nivel 1 + 2 después de cada backup
  └── Alertar si falla

Restauración manual (mensual):
  ├── Nivel 3: restaurar en staging
  └── Documentar resultado y tiempo

Drill completo (trimestral):
  ├── Nivel 4: simular desastre
  ├── Restaurar servicio completo
  ├── Medir RTO real
  └── Actualizar documentación si necesario
```

---

## 10. Errores comunes

### Error 1: Backups en el mismo disco/servidor

```
MAL:
  Servidor prod ─── /backup/ (mismo disco)
  Disco falla → pierdes producción Y backups

BIEN:
  Servidor prod ───▶ NAS (red local) ───▶ Cloud (offsite)
  Disco falla → restauras desde NAS
  Edificio arde → restauras desde cloud
```

### Error 2: Nunca verificar los backups

```
Situación real (ocurre constantemente):
  - Backup automático corriendo 2 años sin problemas
  - Disco de destino se llenó hace 6 meses
  - Script falla silenciosamente desde entonces
  - Desastre → no hay backup útil

Solución:
  - Monitorizar exit codes
  - Verificar tamaño y antigüedad
  - Restauración de prueba mensual
```

### Error 3: No cifrar backups offsite

```
MAL:
  rsync -a /data/ remotehost:/backup/    (sin cifrar)
  Si alguien accede al servidor remoto, lee todos los datos

BIEN:
  tar czf - /data/ | gpg -e -r backup@empresa.com > backup.gpg
  Solo quien tenga la clave privada puede descifrar

  PERO: asegurar que la clave privada también está respaldada
  (en un lugar DIFERENTE al backup)
```

### Error 4: RPO/RTO no alineados con el negocio

```
MAL:
  BD de facturación con backup diario a las 02:00
  Fallo a las 23:00 → 21 horas de facturas perdidas
  "Pero el backup se ejecuta cada día..."

BIEN:
  Analizar: ¿cuánto facturamos por hora?
  Si RPO=1h es necesario → backup horario o replicación de BD
```

### Error 5: Retención insuficiente para descubrir corrupción lenta

```
MAL:
  Retención de 7 días
  Corrupción silenciosa en BD pasa desapercibida 10 días
  Todos los backups contienen datos corruptos

BIEN:
  Retención GFS con mensuales de 12 meses
  Si descubres corrupción de hace 2 meses,
  tienes el mensual de antes de la corrupción
```

---

## 11. Cheatsheet

```
TIPOS DE BACKUP
  Full          Todo cada vez              1 paso para restaurar
  Incremental   Cambios desde último bk    N pasos (cadena ordenada)
  Diferencial   Cambios desde último full  2 pasos (full + último dif)

REGLA 3-2-1
  3 copias, 2 medios, 1 offsite
  (+1 offline/inmutable, +0 errores de verificación)

RPO / RTO
  RPO   Máxima pérdida de datos tolerable (tiempo entre backups)
  RTO   Máximo tiempo de recuperación tolerable

ROTACIÓN GFS
  Son (diario)            retener 7 días
  Father (semanal)        retener 4 semanas
  Grandfather (mensual)   retener 12 meses

IMPLEMENTACIÓN
  tar -g snap.snar -czf full.tar.gz /data/     Incremental con tar
  rsync -a --link-dest=../prev /src/ /new/      Snapshots con rsync
  pg_dump -Fc db > db.dump                      Backup de BD

VERIFICACIÓN
  gzip -t backup.tar.gz                         Integridad compresión
  tar tf backup.tar.gz > /dev/null              Integridad tar
  pg_restore --list db.dump > /dev/null         Integridad dump BD

CIFRADO OFFSITE
  tar czf - /data/ | gpg -e -r KEY > backup.gpg    Cifrar
  gpg -d backup.gpg | tar xzf - -C /restore/       Descifrar

AUTOMATIZACIÓN
  systemd timer (OnCalendar=*-*-* 03:00:00)
  cron (0 3 * * * /usr/local/bin/backup.sh)
```

---

## 12. Ejercicios

### Ejercicio 1: Comparar tipos de backup en la práctica

**Objetivo**: Observar las diferencias de espacio y restauración entre full, incremental y diferencial.

```bash
# 1. Crear datos de prueba
mkdir -p /tmp/bk-strategy/{source,full,inc,dif}
for i in $(seq 1 10); do
    dd if=/dev/urandom of=/tmp/bk-strategy/source/file$i.dat \
       bs=1K count=100 2>/dev/null
done

# 2. Día 1: Full backup para cada esquema
tar czf /tmp/bk-strategy/full/day1.tar.gz -C /tmp/bk-strategy source/

cp /tmp/bk-strategy/full/day1.tar.gz /tmp/bk-strategy/inc/day1-full.tar.gz

cp /tmp/bk-strategy/full/day1.tar.gz /tmp/bk-strategy/dif/day1-full.tar.gz

# 3. Simular Día 2: modificar 2 archivos
dd if=/dev/urandom of=/tmp/bk-strategy/source/file3.dat bs=1K count=100 2>/dev/null
dd if=/dev/urandom of=/tmp/bk-strategy/source/file7.dat bs=1K count=100 2>/dev/null

# Full
tar czf /tmp/bk-strategy/full/day2.tar.gz -C /tmp/bk-strategy source/

# Incremental (cambios desde día 1)
tar --listed-incremental=/tmp/bk-strategy/inc/snap.snar \
    -czf /tmp/bk-strategy/inc/day1-full-snap.tar.gz \
    -C /tmp/bk-strategy source/ 2>/dev/null
# Rehacer: primero crear el snapshot del full
rm /tmp/bk-strategy/inc/snap.snar 2>/dev/null
# Recrear snapshot del estado del full (todos los archivos originales)
# Para simplificar, crear full con snapshot
cd /tmp/bk-strategy
tar --listed-incremental=inc/snap-base.snar \
    -czf /dev/null source/ 2>/dev/null
# Ahora el incremental real
tar --listed-incremental=inc/snap-base.snar \
    -czf inc/day2-inc.tar.gz source/ 2>/dev/null

# Diferencial (siempre usar snapshot del full fresco)
cp inc/snap-base.snar dif/snap-fresh.snar 2>/dev/null || \
    tar --listed-incremental=dif/snap-orig.snar -czf /dev/null source/ 2>/dev/null
tar --listed-incremental=dif/snap-fresh.snar \
    -czf dif/day2-dif.tar.gz source/ 2>/dev/null

# 4. Comparar tamaños
echo "=== Tamaños ==="
echo "Full diario:"
ls -lh /tmp/bk-strategy/full/
echo ""
echo "Full + Incremental:"
ls -lh /tmp/bk-strategy/inc/
echo ""
echo "Full + Diferencial:"
ls -lh /tmp/bk-strategy/dif/

# 5. Limpiar
rm -rf /tmp/bk-strategy
```

**Pregunta de reflexión**: Después de 6 días de cambios, ¿qué esquema consume menos espacio total? ¿Y cuál permite restaurar más rápido el estado del día 5? ¿Hay un esquema que sea mejor en ambas métricas?

> El **incremental** consume menos espacio total (solo almacena los cambios de cada día respecto al anterior). El **diferencial** permite restaurar más rápido el día 5 (solo full + dif del día 5 = 2 pasos). Ningún esquema gana en ambas métricas — es un trade-off inherente. El full diario gana en velocidad de restauración (1 paso) pero pierde masivamente en espacio. Por eso los esquemas combinados (full semanal + incremental diario) son el estándar.

---

### Ejercicio 2: Implementar rotación GFS simulada

**Objetivo**: Construir un sistema de rotación GFS y verificar que la retención funciona correctamente.

```bash
# 1. Crear estructura
mkdir -p /tmp/gfs-lab/{source,backup/{daily,weekly,monthly}}
echo "production data" > /tmp/gfs-lab/source/data.txt

# 2. Simular 35 días de backups
for DAY in $(seq 1 35); do
    # Simular fecha
    SIMDATE=$(date -d "2026-03-01 + $DAY days" +%Y-%m-%d)
    DOW=$(date -d "$SIMDATE" +%u)
    DOM=$(date -d "$SIMDATE" +%d)

    # Modificar datos ligeramente
    echo "data as of $SIMDATE" > /tmp/gfs-lab/source/data.txt

    # Determinar tipo
    if [ "$DOM" -le 7 ] && [ "$DOW" -eq 5 ]; then
        TYPE="monthly"
    elif [ "$DOW" -eq 5 ]; then
        TYPE="weekly"
    else
        TYPE="daily"
    fi

    # Crear backup
    tar czf "/tmp/gfs-lab/backup/$TYPE/backup-$SIMDATE.tar.gz" \
        -C /tmp/gfs-lab source/ 2>/dev/null

    echo "$SIMDATE ($TYPE)"
done

# 3. Ver lo que se acumuló
echo ""
echo "=== Daily ==="
ls /tmp/gfs-lab/backup/daily/
echo ""
echo "=== Weekly ==="
ls /tmp/gfs-lab/backup/weekly/
echo ""
echo "=== Monthly ==="
ls /tmp/gfs-lab/backup/monthly/

# 4. Aplicar retención: daily=7, weekly=28, monthly=365
echo ""
echo "=== Aplicar retención ==="
# Simular que "hoy" es el último día
TODAY=$(date -d "2026-03-01 + 35 days" +%Y-%m-%d)
for TYPE_DIR in daily weekly monthly; do
    case $TYPE_DIR in
        daily)   KEEP=7 ;;
        weekly)  KEEP=28 ;;
        monthly) KEEP=365 ;;
    esac

    CUTOFF=$(date -d "$TODAY - $KEEP days" +%Y-%m-%d)
    for f in /tmp/gfs-lab/backup/$TYPE_DIR/backup-*.tar.gz; do
        FDATE=$(basename "$f" | sed 's/backup-//;s/.tar.gz//')
        if [[ "$FDATE" < "$CUTOFF" ]]; then
            echo "DELETE: $TYPE_DIR/$FDATE (older than ${KEEP}d)"
            rm "$f"
        fi
    done
done

# 5. Ver qué sobrevivió
echo ""
echo "=== Tras retención ==="
echo "Daily:"
ls /tmp/gfs-lab/backup/daily/ 2>/dev/null || echo "(vacío)"
echo "Weekly:"
ls /tmp/gfs-lab/backup/weekly/ 2>/dev/null || echo "(vacío)"
echo "Monthly:"
ls /tmp/gfs-lab/backup/monthly/ 2>/dev/null || echo "(vacío)"

# 6. Limpiar
rm -rf /tmp/gfs-lab
```

**Pregunta de reflexión**: En el esquema GFS, ¿qué pasa si el único viernes del mes que es "primer viernes" (monthly) coincide con que tu servidor está apagado por mantenimiento? ¿Cómo mitigarías esto en producción?

> Se perdería el backup mensual de ese mes. La cadena GFS tendría un hueco de 2 meses entre mensuales. Mitigaciones: (1) usar `Persistent=true` en el systemd timer para que se ejecute al arrancar si se perdió el horario programado, (2) hacer que el script GFS sea idempotente y verificar si el mensual del mes actual existe — si no, promover el siguiente backup semanal a mensual, (3) monitorizar con alertas que detecten la ausencia del backup mensual esperado y notifiquen al equipo.

---

### Ejercicio 3: Diseñar una estrategia para un caso real

**Objetivo**: Aplicar los conceptos teóricos a un escenario realista (ejercicio de diseño, no de ejecución).

```
ESCENARIO:
  Empresa pequeña con 1 servidor Linux:
  - Aplicación web (Django) en /var/www/app/
  - Base de datos PostgreSQL (50 GB, crece 1 GB/mes)
  - Uploads de usuarios en /var/www/media/ (200 GB, 2 GB/día nuevos)
  - Configuración del sistema en /etc/
  - El servidor está en una oficina (no datacenter)
  - Presupuesto limitado: 1 NAS local + 1 cuenta cloud

TAREAS (responder en papel o archivo de texto):

1. Clasificar los datos en Clase A/B/C

2. Definir RPO y RTO para cada clase
   Pista: ¿qué pasa si se pierden 24h de uploads de usuarios?
   ¿Y si la BD se corrompe?

3. Diseñar el esquema de backup:
   a) ¿Qué herramienta para cada tipo de dato?
      (pg_dump, rsync, tar, etc.)
   b) ¿Qué frecuencia?
   c) ¿Full, incremental, o diferencial?
   d) ¿Dónde se almacena cada copia? (local NAS, cloud)

4. Definir la rotación (GFS u otra)

5. ¿Cómo cumples la regla 3-2-1?
   Identificar las 3 copias, 2 medios, 1 offsite

6. ¿Cómo verificas los backups?
   ¿Con qué frecuencia?

7. Escribir el procedimiento de restauración para:
   a) "La BD se corrompió" → pasos exactos
   b) "El servidor se incendió" → pasos exactos
```

**Pregunta de reflexión**: En este escenario, la clave privada GPG para descifrar los backups offsite está en el servidor. Si el servidor se incendia, ¿puedes descifrar los backups del cloud? ¿Dónde debería estar una copia de esa clave?

> No podrías descifrar los backups — la clave se perdió con el servidor. La clave privada debería tener una copia en un lugar **físicamente separado y seguro**: una caja fuerte, un password manager con acceso independiente del servidor (ej. 1Password/Bitwarden con login propio), un USB cifrado en la casa del CTO, o impresa en papel en una caja fuerte bancaria. Sin acceso a la clave de descifrado, los backups cifrados son irrecuperables — exactamente lo que el cifrado debe hacer contra un atacante, pero catastrófico si eres tú quien no tiene la clave.

---

> **Siguiente capítulo**: C06 — Mantenimiento del Sistema, S02 — Automatización, T01 — Scripts de mantenimiento.
