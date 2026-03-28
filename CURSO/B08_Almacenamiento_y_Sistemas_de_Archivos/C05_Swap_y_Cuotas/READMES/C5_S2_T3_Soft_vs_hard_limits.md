# Soft vs hard limits

## Índice

1. [Dos tipos de límite](#dos-tipos-de-límite)
2. [Hard limit](#hard-limit)
3. [Soft limit y grace period](#soft-limit-y-grace-period)
4. [Qué pasa cuando se excede cada límite](#qué-pasa-cuando-se-excede-cada-límite)
5. [Configurar grace period](#configurar-grace-period)
6. [Combinaciones de límites](#combinaciones-de-límites)
7. [Comportamiento en ext4 vs XFS](#comportamiento-en-ext4-vs-xfs)
8. [Monitorizar excesos](#monitorizar-excesos)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## Dos tipos de límite

Cada recurso (bloques e inodos) tiene dos límites independientes:

```
0 ──────────────── soft limit ──────────── hard limit ──────── ∞
│                      │                       │
│    uso normal        │    grace period        │    prohibido
│    (sin restricción) │    (aviso temporal)    │    (escritura falla)
│                      │                       │
│                      │  El usuario puede      │
│                      │  seguir escribiendo    │
│                      │  durante N días        │
│                      │                       │
│                      │  Si no reduce uso      │
│                      │  antes de que expire   │
│                      │  → soft se convierte   │
│                      │    en hard              │
```

- **Soft limit**: frontera de aviso. Se puede exceder temporalmente durante el grace period.
- **Hard limit**: frontera absoluta. Nunca se puede exceder — la escritura falla con `EDQUOT` (Disk quota exceeded).

---

## Hard limit

El hard limit es un **muro absoluto**. Cuando el uso alcanza el hard limit, cualquier intento de escribir más datos falla inmediatamente.

```bash
# Hard limit: 60 MiB
# Uso actual: 58 MiB

dd if=/dev/zero of=/mnt/data/file bs=1M count=5
# dd: error writing '/mnt/data/file': Disk quota exceeded
# Solo se escribieron 2 MiB (hasta el límite de 60 MiB)
# El archivo se crea pero truncado al espacio disponible
```

Características del hard limit:

- No se puede exceder bajo ninguna circunstancia
- La escritura falla con error `EDQUOT`
- El archivo parcial se mantiene (no se borra)
- Aplica tanto a bloques como a inodos
- Solo root puede asignar hard limits (vía `edquota` o `setquota`)

---

## Soft limit y grace period

El soft limit es una **frontera flexible**. Cuando se excede, comienza un **grace period** (periodo de gracia) — un temporizador durante el cual el usuario puede seguir escribiendo (hasta el hard limit). Si no reduce su uso por debajo del soft limit antes de que expire el grace period, el soft limit se convierte en hard limit.

```
Línea de tiempo:

  Día 0        Día 1        Día 5        Día 7
  │            │            │            │
  ▼            ▼            ▼            ▼
  50 MiB       55 MiB       55 MiB       55 MiB
  (soft=50)    (excedido)                 (grace expira)
               │                         │
               └── grace period ─────────┘
               El usuario puede escribir  Si no baja de 50 MiB:
               hasta hard limit (60 MiB)  soft=hard → no puede
                                          escribir NADA más

  Si baja a 48 MiB antes del día 7 → grace se resetea
```

### Ejemplo paso a paso

```bash
# Configuración: soft=50M, hard=60M, grace=7 days
setquota -u alice 51200 61440 0 0 /mnt/data

# Estado inicial: alice usa 30 MiB
quota -s alice
# Filesystem   blocks   quota   limit   grace   files
# /dev/vdb1      30M     50M     60M             ...
#                        ^^^     ^^^
#                       soft    hard    (sin grace — no excedido)

# Alice escribe hasta 55 MiB → excede soft limit
quota -s alice
# /dev/vdb1      55M     50M     60M    6days
#                                       ^^^^^
#                                       grace period activado

# Alice tiene 6 días para bajar de 50 MiB
# Mientras tanto, puede escribir hasta 60 MiB (hard limit)

# Si alice no reduce en 7 días:
quota -s alice
# /dev/vdb1      55M     50M     60M    none
#                                       ^^^^
#                                       grace expirado
# Ahora alice NO puede escribir nada más
# (soft limit se convirtió en hard limit efectivo)
```

---

## Qué pasa cuando se excede cada límite

### Escenarios

```
┌────────────────────────────────────────────────────────────────┐
│                                                                │
│  Caso 1: Uso < soft limit                                      │
│  ─────────────────────────                                     │
│  → Todo normal. Sin restricción.                               │
│                                                                │
│  Caso 2: soft limit < Uso < hard limit (grace activo)          │
│  ─────────────────────────────────────────────                 │
│  → Usuario puede seguir escribiendo hasta hard limit           │
│  → Grace period contando hacia atrás                           │
│  → repquota muestra "+" en el indicador                        │
│                                                                │
│  Caso 3: soft limit < Uso < hard limit (grace expirado)        │
│  ──────────────────────────────────────────────────            │
│  → Usuario NO puede escribir nada más                          │
│  → Soft limit se comporta como hard limit                      │
│  → Debe borrar archivos para bajar del soft limit              │
│  → Al bajar del soft, grace se resetea                         │
│                                                                │
│  Caso 4: Uso = hard limit                                      │
│  ────────────────────────                                      │
│  → Escritura falla: "Disk quota exceeded"                      │
│  → No importa si grace está activo o no                        │
│                                                                │
│  Caso 5: Solo hard limit (soft=0)                              │
│  ─────────────────────────────────                             │
│  → Sin soft limit ni grace period                              │
│  → Muro absoluto en el hard limit                              │
│                                                                │
│  Caso 6: Solo soft limit (hard=0)                              │
│  ─────────────────────────────────                             │
│  → Soft limit con grace, pero sin techo                        │
│  → Tras expirar grace, soft se convierte en hard               │
│  → Poco útil — normalmente se usan ambos                      │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

### Mensaje de error

```bash
# Al exceder hard limit (o soft con grace expirado):
dd if=/dev/zero of=/mnt/data/file bs=1M count=100
# dd: error writing '/mnt/data/file': Disk quota exceeded
# N+0 records in
# N+0 records out
# N bytes copied

# El error también aparece para operaciones de archivo normales:
cp largefile /mnt/data/
# cp: error writing '/mnt/data/largefile': Disk quota exceeded

touch /mnt/data/newfile
# touch: cannot touch '/mnt/data/newfile': Disk quota exceeded
# (si se excedió el límite de inodos)
```

---

## Configurar grace period

El grace period se configura **por filesystem**, no por usuario. Todos los usuarios del mismo filesystem comparten el mismo grace period.

### Con edquota -t

```bash
edquota -t
```

Abre un editor con:

```
Grace period before enforcing soft limits for users:
Time units may be: days, hours, minutes, or seconds
  Filesystem             Block grace period     Inode grace period
  /dev/vdb1                     7days                  7days
```

Modifica los valores y guarda. Unidades válidas: `days`, `hours`, `minutes`, `seconds`.

### Con setquota -t

```bash
# Establecer grace period: 14 días para bloques, 7 días para inodos
setquota -t 1209600 604800 /mnt/data
#            ^^^^^^^ ^^^^^^^
#            14 días  7 días (en segundos)

# 14 días = 14 × 24 × 60 × 60 = 1209600 segundos
#  7 días =  7 × 24 × 60 × 60 =  604800 segundos
```

> **Nota**: `setquota -t` recibe los valores en **segundos**, a diferencia de `edquota -t` que acepta unidades legibles.

### En XFS

```bash
# Configurar grace period en XFS
xfs_quota -x -c 'timer -u 14d' /mnt/data          # bloques: 14 días
xfs_quota -x -c 'timer -u -i 7d' /mnt/data        # inodos: 7 días

# Verificar
xfs_quota -x -c 'state' /mnt/data
# User quota state on /mnt/data (/dev/vdb1)
#   Accounting: ON
#   Enforcement: ON
#   Inode: #131 (2 blocks, 2 extents)
#   Blocks grace time: [14 days]
#   Inodes grace time: [7 days]
```

### Grace period típicos

| Escenario | Grace period | Justificación |
|-----------|-------------|---------------|
| Servidor universitario | 7 días | Tiempo para que estudiantes limpien |
| Hosting | 3 días | Aviso corto, acción rápida |
| Servidor corporativo | 14 días | Tiempo para solicitar más espacio |
| Entorno de prueba | 1 hora | Pruebas rápidas |

---

## Combinaciones de límites

### Solo hard limit (lo más simple)

```bash
setquota -u alice 0 122880 0 0 /mnt/data
# soft=0, hard=120M
# Sin grace period — muro absoluto en 120 MiB
```

Comportamiento: el usuario puede escribir hasta 120 MiB sin aviso. Al llegar, la escritura falla.

### Soft + hard (lo más común)

```bash
setquota -u alice 102400 122880 0 0 /mnt/data
# soft=100M, hard=120M
# Grace period aplica entre 100M y 120M
```

Comportamiento: aviso a 100 MiB, muro a 120 MiB, grace period para reducir.

### Bloques + inodos

```bash
setquota -u alice 102400 122880 500 600 /mnt/data
# Bloques: soft=100M, hard=120M
# Inodos:  soft=500, hard=600
# Ambos recursos tienen sus propios límites y grace periods
```

Un usuario puede exceder el soft de bloques pero no de inodos (o viceversa). Los grace periods son independientes.

### Resumen visual

```
                    Bloques                    Inodos
               soft     hard              soft     hard
               100M     120M              500      600
                │        │                 │        │
  0 ───────────┼────────┼─── ∞   0 ──────┼────────┼─── ∞
     libre     │ grace  │ no        libre │ grace  │ no
               │period  │                 │period  │
```

---

## Comportamiento en ext4 vs XFS

El comportamiento de soft/hard es consistente entre filesystems, pero hay diferencias menores:

```
┌────────────────────────┬──────────────────┬──────────────────┐
│ Aspecto                │ ext4             │ XFS              │
├────────────────────────┼──────────────────┼──────────────────┤
│ Grace configurable     │ edquota -t       │ xfs_quota timer  │
│ Grace default          │ 7 días           │ 7 días           │
│ Grace por FS           │ Sí               │ Sí               │
│ Grace por usuario      │ No               │ No               │
│ Unidades setquota -t   │ Segundos         │ N/A (xfs_quota)  │
│ Error al exceder       │ EDQUOT           │ EDQUOT           │
│ Warn count (XFS)       │ N/A              │ Sí (avisos antes │
│                        │                  │  de enforcement) │
└────────────────────────┴──────────────────┴──────────────────┘
```

### Warn count en XFS

XFS tiene un concepto adicional: **warn count**. Antes de aplicar el hard limit, XFS puede emitir un número configurable de avisos:

```bash
# Configurar warn limit
xfs_quota -x -c 'limit bsoft=100m bhard=120m -w 3 alice' /mnt/data
# alice recibirá 3 avisos antes de que se aplique el enforcement
```

En la práctica, warn count se usa poco. El modelo soft/hard con grace period es suficiente.

---

## Monitorizar excesos

### repquota — identificar usuarios en grace

```bash
repquota -s /mnt/data
# *** Report for user quotas on device /dev/vdb1
# Block grace time: 7days; Inode grace time: 7days
#                         Block limits                File limits
# User            used    soft    hard  grace    used  soft  hard  grace
# ----------------------------------------------------------------------
# alice     +-    110M    100M    120M  5days     150   500   600
# bob       --     40M    100M    120M             30   500   600
# carol     ++     95M    100M    120M  none      520   500   600  3days
```

Interpretación:

```
alice  +-  → bloques excedidos (grace 5 días), inodos OK
bob    --  → todo OK
carol  ++  → bloques excedidos (grace expirado!), inodos excedidos (grace 3 días)

carol no puede escribir NADA más (grace de bloques expirado)
carol tiene 3 días para reducir archivos (inodos)
```

### warnquota — enviar avisos por email

```bash
# warnquota lee las cuotas y envía emails a usuarios que exceden soft limits
warnquota

# Configuración en /etc/warnquota.conf
# Requiere MTA (sendmail/postfix) configurado
```

### Script de monitorización simple

```bash
#!/bin/bash
# check-quotas.sh — listar usuarios en grace period
repquota -s /mnt/data | grep -E '^\w+\s+\+' | while read line; do
    echo "WARNING: $line"
done
```

---

## Errores comunes

### 1. Pensar que soft limit bloquea inmediatamente

```
✗ "Puse soft=100M, ¿por qué alice puede escribir 110 MiB?"
   → El soft limit NO bloquea. Inicia el grace period.

✓ Soft = aviso temporal
  Hard = bloqueo real
  Usar ambos: soft < hard
```

### 2. No configurar grace period

```bash
# ✗ Grace period por defecto puede ser muy largo o corto
# Sin verificar, puede ser 7 días o 0

# ✓ Siempre configurar explícitamente
edquota -t    # verificar y ajustar
```

### 3. Confundir "grace expirado" con "hard limit alcanzado"

```
# Ambos bloquean escrituras, pero la causa es diferente:

Grace expirado:
  Uso: 105M, soft=100M, hard=120M
  → Soft se convierte en hard → bloqueado en 105M
  → Solución: borrar hasta < 100M (soft)

Hard alcanzado:
  Uso: 120M, soft=100M, hard=120M
  → Muro absoluto en 120M
  → Solución: borrar hasta tener espacio
```

### 4. Poner soft > hard

```bash
# ✗ Soft mayor que hard — comportamiento impredecible
setquota -u alice 122880 102400 0 0 /mnt/data
# soft=120M > hard=100M → no tiene sentido

# ✓ Siempre soft ≤ hard
setquota -u alice 102400 122880 0 0 /mnt/data
# soft=100M < hard=120M
```

### 5. Olvidar que grace es por filesystem, no por usuario

```
✗ "Quiero 7 días de grace para alice y 3 para bob"
   → No es posible — grace es global por filesystem

✓ Si necesitas grace periods distintos:
   → Usar filesystems separados
   → O aceptar el mismo grace para todos
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│              Soft vs hard limits — Referencia rápida             │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  CONCEPTOS:                                                      │
│    Soft limit: aviso — se puede exceder durante grace period     │
│    Hard limit: muro — nunca se puede exceder                     │
│    Grace period: tiempo para reducir uso bajo soft limit          │
│    Grace expirado: soft se convierte en hard                     │
│                                                                  │
│  ESCENARIOS:                                                     │
│    uso < soft            → OK, sin restricción                   │
│    soft < uso < hard     → grace period activo, puede escribir   │
│    soft < uso, grace=0   → BLOQUEADO, debe reducir bajo soft     │
│    uso = hard            → BLOQUEADO, siempre                    │
│                                                                  │
│  CONFIGURAR GRACE:                                               │
│    edquota -t                           interactivo              │
│    setquota -t BLOQUES_SEG INODOS_SEG /mount   en segundos      │
│    xfs_quota -x -c 'timer -u 14d' /mount       XFS              │
│                                                                  │
│  CONFIGURAR LÍMITES:                                             │
│    setquota -u alice BSOFT BHARD ISOFT IHARD /mount              │
│    Solo soft: setquota -u alice 102400 0 0 0 /mount              │
│    Solo hard: setquota -u alice 0 122880 0 0 /mount              │
│    Ambos:     setquota -u alice 102400 122880 500 600 /mount     │
│    Eliminar:  setquota -u alice 0 0 0 0 /mount                  │
│                                                                  │
│  MONITORIZAR:                                                    │
│    repquota -s /mount          ver todos los usuarios            │
│    quota -s alice              ver un usuario                    │
│    Indicadores: -- OK | +- bloques | -+ inodos | ++ ambos       │
│    grace muestra días restantes o "none" (expirado)              │
│                                                                  │
│  REGLAS:                                                         │
│    soft ≤ hard (siempre)                                         │
│    Grace es por filesystem, no por usuario                       │
│    Grace se resetea cuando uso baja del soft limit               │
│    0 en soft o hard = sin límite para ese recurso                │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Observar soft limit y grace period

En tu VM de lab con cuotas habilitadas en `/mnt/quotalab` (ext4):

1. Configura grace period de 2 minutos (para poder observar la expiración):
   ```bash
   setquota -t 120 120 /mnt/quotalab
   # 120 segundos = 2 minutos
   ```
2. Crea un usuario de prueba y asigna cuotas:
   ```bash
   useradd quotauser
   setquota -u quotauser 5120 7168 10 15 /mnt/quotalab
   # soft=5M, hard=7M, soft=10 archivos, hard=15 archivos
   chown quotauser /mnt/quotalab/testdir
   mkdir -p /mnt/quotalab/testdir
   ```
3. Como quotauser, escribe hasta exceder el soft limit:
   ```bash
   su - quotauser -c "dd if=/dev/zero of=/mnt/quotalab/testdir/file1 bs=1M count=6"
   ```
4. Verifica: `repquota -s /mnt/quotalab` — ¿ves `+-` y grace period?
5. Espera 2 minutos (grace period)
6. Intenta escribir más:
   ```bash
   su - quotauser -c "dd if=/dev/zero of=/mnt/quotalab/testdir/file2 bs=1K count=1"
   ```
7. ¿Qué error ves? Verifica con `repquota` — ¿grace muestra "none"?
8. Reduce el uso borrando archivos y verifica que el grace se resetea

> **Pregunta de reflexión**: ¿por qué el grace period se resetea completamente al bajar del soft limit en lugar de "pausarse" y continuar desde donde estaba?

### Ejercicio 2: Hard limit absoluto

1. Configura cuotas sin soft limit (solo hard):
   ```bash
   setquota -u quotauser 0 5120 0 10 /mnt/quotalab
   # sin soft, hard=5M bloques, hard=10 inodos
   ```
2. Escribe hasta el hard limit:
   ```bash
   su - quotauser -c "dd if=/dev/zero of=/mnt/quotalab/testdir/bigfile bs=1M count=10"
   ```
3. ¿Cuántos MiB se escribieron antes del error?
4. Verifica: `ls -lh /mnt/quotalab/testdir/bigfile` — ¿tamaño del archivo truncado?
5. Ahora prueba el límite de inodos:
   ```bash
   su - quotauser -c 'for i in $(seq 1 15); do touch /mnt/quotalab/testdir/f$i; done'
   ```
6. ¿Cuántos archivos se crearon? ¿Cuál fue el error del archivo 11?
7. Compara con el comportamiento del ejercicio 1 (soft+hard) — ¿cuál es más amigable?

> **Pregunta de reflexión**: sin soft limit, el usuario no recibe aviso antes de llegar al muro. ¿En qué escenarios sería preferible usar solo hard limit sin soft?

### Ejercicio 3: Combinación bloques + inodos

1. Configura ambos recursos:
   ```bash
   setquota -u quotauser 3072 5120 5 10 /mnt/quotalab
   # bloques: soft=3M, hard=5M
   # inodos: soft=5, hard=10
   ```
2. Crea 4 archivos pequeños (1K cada uno):
   ```bash
   su - quotauser -c 'for i in $(seq 1 4); do echo "data" > /mnt/quotalab/testdir/small$i; done'
   ```
3. Verifica: `repquota -s /mnt/quotalab` — ¿estado `--`?
4. Crea 2 archivos más (exceder soft de inodos):
   ```bash
   su - quotauser -c 'for i in 5 6; do echo "data" > /mnt/quotalab/testdir/small$i; done'
   ```
5. Verifica: ¿el indicador cambió a `-+`?
6. Ahora escribe un archivo grande (exceder soft de bloques):
   ```bash
   su - quotauser -c "dd if=/dev/zero of=/mnt/quotalab/testdir/big bs=1M count=4"
   ```
7. Verifica: ¿el indicador cambió a `++`?
8. Limpieza: `userdel -r quotauser`

> **Pregunta de reflexión**: si un usuario excede el soft limit de inodos pero no de bloques, ¿puede crear archivos nuevos de 0 bytes? ¿Y archivos grandes con `dd`?
