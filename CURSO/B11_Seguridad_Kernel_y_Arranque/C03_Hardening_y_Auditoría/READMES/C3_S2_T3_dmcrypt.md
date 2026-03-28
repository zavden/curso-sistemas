# dm-crypt: la capa inferior de LUKS

## Índice

1. [Qué es dm-crypt](#1-qué-es-dm-crypt)
2. [dm-crypt vs LUKS: relación y diferencias](#2-dm-crypt-vs-luks-relación-y-diferencias)
3. [Device Mapper: el framework](#3-device-mapper-el-framework)
4. [dm-crypt plain mode](#4-dm-crypt-plain-mode)
5. [Configuración manual con cryptsetup](#5-configuración-manual-con-cryptsetup)
6. [Cifrados y modos disponibles](#6-cifrados-y-modos-disponibles)
7. [dm-crypt en la práctica](#7-dm-crypt-en-la-práctica)
8. [dm-integrity: integridad autenticada](#8-dm-integrity-integridad-autenticada)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Qué es dm-crypt

dm-crypt es el **subsistema del kernel** Linux que proporciona cifrado
transparente de dispositivos de bloque. LUKS es un **formato** que se
construye encima de dm-crypt:

```
Relación LUKS / dm-crypt
══════════════════════════

  ┌─────────────────────────────────┐
  │  LUKS (formato en disco)       │  ← Lo que usas normalmente
  │  Header + key slots + metadata │     (T02 — tema anterior)
  │  Gestión de claves automática  │
  ├─────────────────────────────────┤
  │  dm-crypt (kernel module)      │  ← El motor de cifrado
  │  Cifra/descifra bloques        │     (este tema)
  │  No sabe nada de headers       │
  └─────────────────────────────────┘
  ┌─────────────────────────────────┐
  │  Device Mapper (kernel)        │  ← Framework genérico
  │  Mappings virtuales de bloques │
  └─────────────────────────────────┘

  LUKS usa dm-crypt internamente
  dm-crypt puede usarse SIN LUKS (plain mode)
  Device Mapper es la base de dm-crypt, LVM, dm-raid, etc.
```

### Por qué entender dm-crypt si ya conoces LUKS

```
Razones para entender dm-crypt
════════════════════════════════

  1. LUKS usa dm-crypt internamente
     → Entender dm-crypt = entender qué hace LUKS por debajo

  2. dm-crypt plain mode tiene usos válidos
     → Cifrado sin header (negabilidad plausible)
     → Cifrado de swap
     → Cifrado temporal sin metadata persistente

  3. Troubleshooting
     → Los errores de LUKS a menudo vienen de dm-crypt
     → dmesg muestra mensajes de dm-crypt, no de LUKS

  4. Exámenes y entrevistas
     → LPIC-2 / RHCSA preguntan sobre la relación entre ambos

  5. Device Mapper es omnipresente
     → LVM = dm-linear + dm-striped
     → LUKS = dm-crypt
     → Software RAID = dm-raid
     → Multipath = dm-multipath
     → Snapshots = dm-snapshot
     → Entender DM = entender todo esto
```

---

## 2. dm-crypt vs LUKS: relación y diferencias

### Qué aporta LUKS sobre dm-crypt

```
dm-crypt solo (plain mode)         LUKS (sobre dm-crypt)
══════════════════════════         ═════════════════════

  Sin header en disco               Header con metadata
  Sin key slots                     Hasta 32 key slots
  Sin UUID                          UUID para identificar
  Sin detección automática          Detectable (magic number)
  Clave derivada directamente       Master Key independiente
  de la passphrase                  de las passphrases
  Sin backup de metadata            Backup de header posible
  Sin estándar de formato           Formato estandarizado

  Cambiar passphrase:               Cambiar passphrase:
  → recifrar todo el disco          → recifrar solo el slot
    (imposible en la práctica)        (milisegundos)

  Múltiples passphrases:            Múltiples passphrases:
  → imposible                       → sí, una por slot

  Verificar passphrase:             Verificar passphrase:
  → no se puede (abre "siempre",   → sí, verifica contra
    con datos basura si es la         el digest del header
    passphrase incorrecta)
```

### Cuándo usar cada uno

```
┌──────────────────────┬────────────────────────────────────┐
│ Usar LUKS            │ Usar dm-crypt plain                │
├──────────────────────┼────────────────────────────────────┤
│ Casi siempre         │ Cifrado de swap                    │
│ Es el default        │ Cifrado temporal (/tmp)            │
│ Gestión de claves    │ Negabilidad plausible              │
│ Múltiples users      │ (sin header, no se puede probar    │
│ Cambio de passphrase │  que el disco está cifrado)        │
│ UUID, detección auto │ Máxima simplicidad                 │
│ Backup de header     │ Compatibilidad con otros sistemas  │
└──────────────────────┴────────────────────────────────────┘

Regla práctica: usa LUKS siempre, excepto swap y /tmp efímero.
```

---

## 3. Device Mapper: el framework

dm-crypt es un **target** del framework Device Mapper del kernel. Entender
Device Mapper ayuda a entender cómo encaja dm-crypt:

### Qué es Device Mapper

```
Device Mapper — dispositivos virtuales
════════════════════════════════════════

  Device Mapper crea dispositivos de bloque VIRTUALES
  en /dev/mapper/ que transforman I/O antes de enviarlo
  al dispositivo real:

  Aplicación
       │
       ▼
  /dev/mapper/data_crypt     ← Dispositivo virtual
       │
       │  [dm-crypt target]    ← Transforma: cifra/descifra
       │
       ▼
  /dev/sdb                   ← Dispositivo real

  Otros targets de Device Mapper:
    dm-linear   → LVM (mapeo lineal de segmentos)
    dm-striped  → LVM striping (RAID-0)
    dm-mirror   → RAID-1
    dm-crypt    → Cifrado
    dm-raid     → RAID 4/5/6
    dm-snapshot → Snapshots (thin provisioning)
    dm-multipath → Múltiples rutas a un LUN SAN
    dm-integrity → Integridad de bloques
```

### Ver mappings activos

```bash
# Listar todos los mappings de Device Mapper
sudo dmsetup ls
# data_crypt    (253:1)
# rhel-root     (253:0)
# rhel-swap     (253:2)

# Ver detalles de un mapping
sudo dmsetup info data_crypt
# Name:              data_crypt
# State:             ACTIVE
# Read Ahead:        256
# Tables present:    LIVE
# Open count:        1
# Event number:      0
# Major, minor:      253, 1
# Number of targets:  1
# UUID: CRYPT-LUKS2-12345678abcd...

# Ver la tabla del mapping (cómo transforma)
sudo dmsetup table data_crypt
# 0 104824832 crypt aes-xts-plain64 :64:logon:cryptsetup:... 0 8:16 32768
#                   ─────           ──────────────────────      ────  ─────
#                   target=crypt    clave (referencia)          dev   offset
```

### Device Mapper y LVM+LUKS

```
Stack típico: LVM sobre LUKS
══════════════════════════════

  /dev/mapper/vg-lv_data       ← LV de LVM (dm-linear)
       │
  /dev/mapper/vg-lv_root       ← LV de LVM (dm-linear)
       │
  /dev/sda2_crypt              ← dm-crypt (LUKS)
       │
  /dev/sda2                    ← Partición física

  lsblk muestra esta jerarquía:
  NAME                SIZE  TYPE   MOUNTPOINT
  sda                  50G  disk
  ├─sda1                1G  part   /boot
  └─sda2               49G  part
    └─sda2_crypt       49G  crypt
      ├─vg-lv_root    30G  lvm    /
      └─vg-lv_data    19G  lvm    /data

  Flujo de una escritura:
  write() → ext4 → dm-linear (LVM) → dm-crypt → disco
  (la escritura se cifra transparentemente antes de ir al disco)
```

---

## 4. dm-crypt plain mode

El modo plain usa dm-crypt **sin** header LUKS. Es el modo más básico:

### Crear un volumen plain

```bash
# Abrir dispositivo con dm-crypt plain (sin luksFormat previo)
sudo cryptsetup open --type plain /dev/sdb plain_crypt
# Enter passphrase: ********

# Opciones por defecto de plain mode:
#   cipher: aes-cbc-essiv:sha256
#   key size: 256 bits
#   hash: ripemd160 (para derivar clave de passphrase)

# Con opciones explícitas (recomendado)
sudo cryptsetup open --type plain \
    --cipher aes-xts-plain64 \
    --key-size 512 \
    --hash sha512 \
    /dev/sdb plain_crypt
```

### Diferencias al usar

```bash
# Con LUKS, cryptsetup VERIFICA la passphrase:
sudo cryptsetup open /dev/sdb luks_vol
# Passphrase incorrecta → "No key available with this passphrase"
# Passphrase correcta   → abre el volumen

# Con plain mode, cryptsetup NO PUEDE verificar:
sudo cryptsetup open --type plain /dev/sdb plain_vol
# CUALQUIER passphrase "funciona" (abre el mapping)
# Con passphrase incorrecta → monta "datos basura" (aleatorios)
# Con passphrase correcta   → monta los datos reales

# ¿Por qué? Porque no hay header con digest para verificar
# dm-crypt simplemente usa lo que le das como clave
```

```
El problema de la verificación en plain mode
═══════════════════════════════════════════════

  LUKS:
    Passphrase → PBKDF2/Argon2 → Key → ¿descifra master key del slot?
                                        │
                                        ├── SÍ → passphrase correcta
                                        └── NO → error, rechaza

  Plain:
    Passphrase → hash → Key → descifrar datos
                               │
                               └── Siempre "funciona"
                                   (genera datos o basura)
                                   No hay forma de saber si es correcta

  Implicación práctica:
    - mount /dev/mapper/plain_vol puede dar "wrong fs type"
      si la passphrase es incorrecta (porque ve datos aleatorios,
      no un filesystem válido)
    - Pero no te dice "passphrase incorrecta"
```

### Caso de uso: cifrado de swap

```bash
# swap cifrada con clave aleatoria (se pierde al apagar)
# Perfecto porque swap es temporal — no necesita persistir

# En /etc/crypttab:
swap_crypt  /dev/sda3  /dev/urandom  swap,cipher=aes-xts-plain64,size=512

# Desglose:
# swap_crypt      ← nombre del mapping
# /dev/sda3       ← partición de swap
# /dev/urandom    ← keyfile = datos aleatorios (nueva clave cada boot)
# swap            ← crear swap automáticamente
# cipher=...      ← algoritmo de cifrado
# size=512        ← tamaño de clave en bits

# En /etc/fstab:
/dev/mapper/swap_crypt  none  swap  sw  0  0

# Resultado:
# Cada arranque genera una clave aleatoria nueva
# Los datos de swap anteriores son irrecuperables
# No se puede hibernar (la clave se pierde al apagar)
```

### Caso de uso: cifrado temporal de /tmp

```bash
# /tmp efímero cifrado con clave aleatoria

# En /etc/crypttab:
tmp_crypt  /dev/sda4  /dev/urandom  tmp,cipher=aes-xts-plain64,size=512

# "tmp" hace que se cree ext4 y se monte como /tmp automáticamente

# En /etc/fstab:
/dev/mapper/tmp_crypt  /tmp  ext4  defaults  0  0

# Cada arranque:
# 1. Genera clave aleatoria desde /dev/urandom
# 2. Abre /dev/sda4 con esa clave (plain mode)
# 3. Crea ext4 nuevo (los datos anteriores se pierden)
# 4. Monta en /tmp
```

---

## 5. Configuración manual con cryptsetup

### Opciones de cryptsetup para dm-crypt

```bash
# Ver las opciones por defecto compiladas
cryptsetup --help | grep -A20 "Default compiled-in"

# Salida típica:
# Default compiled-in metadata format:  LUKS2
# Default compiled-in key and passphrase parameters:
#   Maximum keyfile size: 8192kB, Maximum interactive passphrase length: 512
# Default PBKDF for LUKS1: pbkdf2, iteration time: 2000
# Default PBKDF for LUKS2: argon2id
#   Iteration time: 2000, Memory required: 1048576kB, Parallel threads: 4
# Default compiled-in device cipher parameters:
#   loop-AES: aes, Key 256 bits
#   plain: aes-cbc-essiv:sha256, Key 256 bits, Password hashing: ripemd160
#   LUKS:  aes-xts-plain64, Key 256 bits, LUKS header hashing: sha256
```

### Crear mapping manualmente con dmsetup

Para entender qué hace `cryptsetup` por debajo, puedes crear el mapping
directamente con `dmsetup` (normalmente NO necesitas hacer esto):

```bash
# cryptsetup open es equivalente a configurar dm-crypt vía dmsetup:

# Lo que cryptsetup hace internamente (simplificado):
# 1. Deriva la clave de la passphrase
# 2. Crea una tabla de Device Mapper:
#    "0 SECTORS crypt CIPHER KEY IV_OFFSET DEVICE OFFSET"
# 3. Carga la tabla con dmsetup

# Ver la tabla que cryptsetup creó:
sudo dmsetup table data_crypt
# 0 104824832 crypt aes-xts-plain64 :64:logon:cryptsetup:12345... 0 8:16 32768
#  │ │         │    │                │                              │ │    │
#  │ │         │    │                │                              │ │    └ offset en el device
#  │ │         │    │                │                              │ └ major:minor del device
#  │ │         │    │                │                              └ IV offset
#  │ │         │    │                └ referencia a clave en kernel keyring
#  │ │         │    └ cipher
#  │ │         └ target type
#  │ └ tamaño en sectores
#  └ sector de inicio

# La clave NO aparece en texto plano (referenciada vía keyring)
```

### dmsetup: operaciones útiles

```bash
# Listar todos los mappings
sudo dmsetup ls

# Info detallada de un mapping
sudo dmsetup info data_crypt

# Tabla completa (cómo se construye el mapping)
sudo dmsetup table data_crypt

# Estado del target
sudo dmsetup status data_crypt

# Suspender I/O de un mapping
sudo dmsetup suspend data_crypt

# Reanudar I/O
sudo dmsetup resume data_crypt

# Eliminar un mapping (equivale a cryptsetup close)
sudo dmsetup remove data_crypt
```

---

## 6. Cifrados y modos disponibles

### Algoritmos de cifrado

```bash
# Ver cifrados soportados por el kernel
cat /proc/crypto | grep -E "^name" | sort -u

# O más específico para block ciphers:
cat /proc/crypto | grep -B2 "type.*blkcipher" | grep name
```

```
Cifrados disponibles en dm-crypt
═════════════════════════════════

  Cifrado        Tamaño clave    Notas
  ───────        ────────────    ─────
  aes            128/192/256     Estándar, aceleración hardware (AES-NI)
  serpent        128/192/256     Más lento, diseño más conservador
  twofish        128/192/256     Alternativa a AES
  camellia       128/192/256     Popular en Japón, estándar ISO

  Casi siempre: AES
  → Hardware moderno tiene AES-NI (aceleración por CPU)
  → AES-256 con AES-NI: ~3 GB/s de throughput
  → Serpent/Twofish sin aceleración: ~500 MB/s
```

### Modos de operación

```
Modos de operación para cifrado de bloques
════════════════════════════════════════════

  El cifrado de bloques (AES) cifra bloques de 128 bits.
  Un disco tiene millones de bloques. El "modo" define
  cómo se relacionan entre sí:

  aes-xts-plain64 (RECOMENDADO)
  ─────────────────────────────
  XTS = XEX-based Tweaked-codebook with Ciphertext Stealing
  Diseñado específicamente para cifrado de disco
  Cada sector tiene un "tweak" único basado en su posición
  La clave es doble: 512 bits = 256 AES + 256 tweak
  → Standard de la industria (IEEE P1619)

  aes-cbc-essiv:sha256 (LEGACY)
  ─────────────────────────────
  CBC = Cipher Block Chaining
  ESSIV = Encrypted Salt-Sector IV
  Vulnerable a ciertos ataques teóricos (watermarking)
  → Aceptable, pero preferir XTS

  aes-cbc-plain64 (INSEGURO)
  ──────────────────────────
  IV predecible (número de sector)
  Vulnerable a ataques de watermarking
  → No usar

  Formato del string cipher:
    CIPHER-MODE-IV
    aes   -xts -plain64
    │      │    │
    │      │    └── Generación del IV (vector de inicialización)
    │      └── Modo de operación
    └── Algoritmo de cifrado
```

### Verificar aceleración hardware

```bash
# ¿El CPU soporta AES-NI?
grep -o aes /proc/cpuinfo | head -1
# aes  → sí, tiene aceleración

# Benchmark de rendimiento
cryptsetup benchmark
#  Algorithm |  Key |  Encryption |  Decryption
#     aes-cbc   128b   1200.0 MiB/s  3500.0 MiB/s
#     aes-cbc   256b   1000.0 MiB/s  2700.0 MiB/s
#     aes-xts   256b   2600.0 MiB/s  2700.0 MiB/s
#     aes-xts   512b   2100.0 MiB/s  2200.0 MiB/s
# serpent-cbc   128b     95.0 MiB/s   500.0 MiB/s   ← sin aceleración
# twofish-cbc   128b    200.0 MiB/s   380.0 MiB/s   ← sin aceleración

# Con AES-NI, AES es 5-10x más rápido que las alternativas
```

---

## 7. dm-crypt en la práctica

### Verificar que dm-crypt está activo

```bash
# Ver módulos del kernel relacionados
lsmod | grep dm_crypt
# dm_crypt              49152  1

# Si no está cargado:
sudo modprobe dm_crypt

# Ver el target crypt en Device Mapper
sudo dmsetup targets
# crypt            v1.23.0
# striped          v1.6.0
# linear           v1.4.0
# error            v1.6.0
```

### Monitorear rendimiento de dm-crypt

```bash
# Ver estadísticas de I/O del mapping
sudo dmsetup status data_crypt
# 0 104824832 crypt 8:16 32768

# Estadísticas detalladas con iostat
iostat -x /dev/mapper/data_crypt 1
# Muestra throughput, latencia, utilización

# Comparar rendimiento cifrado vs no cifrado
# (en un disco de test, no en producción)

# Escribir 1 GB sin cifrado:
dd if=/dev/zero of=/dev/sdb bs=1M count=1024 oflag=direct
# 1.07 GB/s

# Escribir 1 GB con cifrado:
dd if=/dev/zero of=/dev/mapper/data_crypt bs=1M count=1024 oflag=direct
# 0.95 GB/s (impacto ~10% con AES-NI)
```

### dm-crypt y rendimiento

```
Impacto de rendimiento de dm-crypt
════════════════════════════════════

  Con AES-NI (CPUs modernos):
    Overhead: 5-15%
    Para la mayoría de workloads: imperceptible
    Bottleneck suele ser el disco, no el cifrado

  Sin AES-NI (CPUs antiguos o embebidos):
    Overhead: 30-50%
    Puede ser significativo en I/O intensivo
    Considerar cifrados más ligeros (chacha20)

  Factores que afectan:
    ✓ AES-NI: aceleración hardware (mayor factor)
    ✓ Tamaño de clave: 256 vs 512 bits (~15% diferencia)
    ✓ Modo: XTS vs CBC (similar rendimiento)
    ✓ Tipo de I/O: secuencial (poco impacto) vs aleatorio (más impacto)
    ✓ Número de CPUs: dm-crypt paraleliza entre cores
```

### Número de threads de dm-crypt

```bash
# dm-crypt puede usar múltiples CPUs para cifrar en paralelo
# Ver workers activos:
cat /sys/block/dm-*/dm/name
# Muestra los mappings

# Ajustar el número de threads por mapping (performance tuning):
# En /etc/crypttab:
# data_crypt UUID=... /root/key luks,same-cpu-crypt
# same-cpu-crypt → cifrar y descifrar en el mismo CPU que hizo el I/O
# (mejor para workloads con mucha localidad de caché)

# O no-read-workqueue / no-write-workqueue (kernel 5.9+):
# data_crypt UUID=... /root/key luks,no-read-workqueue,no-write-workqueue
# Procesa I/O en el thread que lo emitió (menos latencia)
```

---

## 8. dm-integrity: integridad autenticada

LUKS2 puede combinar dm-crypt con dm-integrity para detectar **modificaciones
no autorizadas** de los datos cifrados:

### El problema sin integridad

```
Sin integridad (dm-crypt solo)
═══════════════════════════════

  Atacante con acceso físico al disco:
  1. No puede leer los datos (cifrados)
  2. PERO puede modificar bloques cifrados
  3. Al descifrar, los datos se corrompen silenciosamente
  4. El filesystem puede dañarse sin que te des cuenta

  Esto es un ataque de "bit-flipping":
    - No descifra los datos
    - Pero corrompe bloques específicos
    - Puede causar denegación de servicio
    - En teoría, puede manipular datos conocidos
```

### dm-crypt + dm-integrity

```bash
# LUKS2 con integridad autenticada (AEAD)
sudo cryptsetup luksFormat --type luks2 \
    --cipher aes-xts-plain64 \
    --integrity hmac-sha256 \
    /dev/sdb

# --integrity hmac-sha256:
# Cada sector tiene un tag HMAC que verifica:
#   1. Que los datos no fueron modificados
#   2. Que el sector no fue movido a otra posición

# Costo: ~10% más espacio en disco para los tags
# Costo: overhead de rendimiento adicional

# Verificar
sudo cryptsetup luksDump /dev/sdb | grep integrity
# integrity: hmac(sha256)
```

```
Con integridad (dm-crypt + dm-integrity)
═════════════════════════════════════════

  Lectura de un sector:
  1. Lee datos cifrados + tag HMAC
  2. Verifica HMAC → ¿datos íntegros?
     │
     ├── SÍ → descifra y entrega datos
     │
     └── NO → error de I/O
              (sector corrupto o manipulado)
              → Log del kernel, filesystem marca error

  Escritura de un sector:
  1. Cifra los datos
  2. Calcula HMAC sobre los datos cifrados
  3. Escribe datos cifrados + tag HMAC
```

### Verificar dm-integrity

```bash
# Ver si el mapping usa integridad
sudo dmsetup table data_crypt
# Si incluye "integrity" → habilitada

# Ver estadísticas de integridad
sudo integritysetup status data_crypt_dif
# (si usa dm-integrity como capa separada)
```

---

## 9. Errores comunes

### Error 1: confundir dm-crypt plain con LUKS

```bash
# Plain mode NO tiene header
# No puedes usar luksOpen en un volumen plain:
sudo cryptsetup luksOpen /dev/sdb vol
# Device /dev/sdb is not a valid LUKS device.

# Para plain mode usar --type plain:
sudo cryptsetup open --type plain /dev/sdb vol

# Y al revés: no uses --type plain en un volumen LUKS
# (ignoraría el header y los datos serían basura)
```

### Error 2: asumir que plain mode verifica la passphrase

```bash
# Con plain mode, CUALQUIER passphrase "funciona"
sudo cryptsetup open --type plain /dev/sdb test
# Enter passphrase: wrongpassword
# (no hay error — crea el mapping con clave incorrecta)

sudo mount /dev/mapper/test /mnt
# mount: wrong fs type...
# (los datos descifrados son basura, no hay filesystem)

# No hay forma de saber si la passphrase es correcta
# excepto intentar montar y ver si funciona
```

### Error 3: no considerar el overhead de integridad

```bash
# Con --integrity hmac-sha256:
# Cada sector de 512 bytes necesita ~32 bytes de tag
# ~6% de espacio extra para los tags

# Un disco de 1 TB con integridad:
# ~940 GB utilizables (vs ~930 GB sin integridad en LUKS normal)
# La diferencia es pequeña, pero el rendimiento baja ~20%

# Para la mayoría de servidores: integridad no es necesaria
# Para alta seguridad: activar integridad
```

### Error 4: olvidar parámetros de cifrado en plain mode

```bash
# Plain mode NO guarda los parámetros en ningún sitio
# Si abres con parámetros diferentes → datos basura

# Crear con:
sudo cryptsetup open --type plain --cipher aes-xts-plain64 \
    --key-size 512 --hash sha512 /dev/sdb vol

# Abrir después con parámetros DIFERENTES:
sudo cryptsetup open --type plain --cipher aes-cbc-essiv:sha256 \
    --key-size 256 --hash ripemd160 /dev/sdb vol
# Se abre sin error pero los datos son BASURA
# (descifra con algoritmo incorrecto)

# Con LUKS esto NO pasa: los parámetros están en el header
# → LUKS es más seguro contra errores humanos
```

### Error 5: no cargar el módulo dm_crypt

```bash
# Síntoma: cryptsetup falla con "device-mapper" error
sudo cryptsetup open /dev/sdb vol
# Device-mapper backend error...

# Verificar:
lsmod | grep dm_crypt
# (vacío)

# Solución:
sudo modprobe dm_crypt

# Permanente:
echo "dm_crypt" | sudo tee /etc/modules-load.d/dm_crypt.conf
```

---

## 10. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════╗
║                    dm-crypt — Cheatsheet                       ║
╠══════════════════════════════════════════════════════════════════╣
║                                                                  ║
║  RELACIÓN                                                        ║
║  Device Mapper → framework genérico del kernel                   ║
║  dm-crypt      → target de DM para cifrado                      ║
║  LUKS          → formato de disco que usa dm-crypt               ║
║  LUKS = dm-crypt + header + key slots + metadata                ║
║                                                                  ║
║  PLAIN MODE (sin LUKS)                                           ║
║  cryptsetup open --type plain /dev/sdX name                      ║
║  cryptsetup open --type plain --cipher aes-xts-plain64 \        ║
║    --key-size 512 --hash sha512 /dev/sdX name                   ║
║                                                                  ║
║  SWAP CIFRADA (plain, clave aleatoria por boot)                  ║
║  /etc/crypttab:                                                  ║
║  swap_crypt /dev/sdX /dev/urandom swap,cipher=aes-xts-...,size=512 ║
║  /etc/fstab:                                                     ║
║  /dev/mapper/swap_crypt none swap sw 0 0                         ║
║                                                                  ║
║  DEVICE MAPPER                                                   ║
║  dmsetup ls                    Listar mappings                   ║
║  dmsetup info name             Info de un mapping                ║
║  dmsetup table name            Ver tabla (cifrado, dispositivo)  ║
║  dmsetup status name           Estado del target                 ║
║  dmsetup targets               Targets disponibles               ║
║                                                                  ║
║  KERNEL                                                          ║
║  lsmod | grep dm_crypt        ¿Módulo cargado?                  ║
║  modprobe dm_crypt             Cargar módulo                     ║
║  cat /proc/crypto              Algoritmos disponibles            ║
║  grep aes /proc/cpuinfo        ¿Tiene AES-NI?                   ║
║  cryptsetup benchmark          Rendimiento de cifrados           ║
║                                                                  ║
║  CIFRADOS                                                        ║
║  aes-xts-plain64    Recomendado (estándar, rápido con AES-NI)   ║
║  aes-cbc-essiv      Legacy (aceptable)                           ║
║  serpent-xts-plain64 Alternativa conservadora (lento)            ║
║                                                                  ║
║  INTEGRIDAD (LUKS2)                                              ║
║  cryptsetup luksFormat --integrity hmac-sha256 /dev/sdX          ║
║  Detecta modificaciones no autorizadas de datos cifrados         ║
║  Costo: ~6% espacio, ~20% rendimiento                           ║
║                                                                  ║
║  PLAIN vs LUKS                                                   ║
║  Plain: sin header, sin verificación, sin key slots             ║
║  LUKS:  con header, verificación, multi-key, UUID               ║
║  Usar LUKS siempre excepto swap y /tmp efímero                  ║
║                                                                  ║
╚══════════════════════════════════════════════════════════════════╝
```

---

## 11. Ejercicios

### Ejercicio 1: Explorar Device Mapper

```bash
# Paso 1: Listar todos los mappings activos
sudo dmsetup ls

# Paso 2: Ver la tabla de un mapping (si tienes LVM o LUKS)
sudo dmsetup table
```

> **Pregunta de predicción**: en un sistema con LVM (volumen `rhel-root`),
> ¿qué target type verás en la tabla de dmsetup? ¿Y en un volumen LUKS
> abierto? ¿Puedes tener ambos en la misma cadena?
>
> **Respuesta**: `rhel-root` mostrará target `linear` (LVM mapea segmentos
> linealmente). Un volumen LUKS mostrará target `crypt`. Sí, puedes tener
> ambos en cadena: un volumen LUKS (target `crypt`) que contiene un PV de
> LVM, sobre el cual hay LVs (target `linear`). El I/O pasa por
> `linear → crypt → disco`. Es la configuración estándar del instalador de
> RHEL/Fedora cuando seleccionas cifrado de disco completo.

### Ejercicio 2: Comparar plain vs LUKS

```bash
# Crear dos loop devices para comparar
dd if=/dev/zero of=/tmp/plain-test.img bs=1M count=50
dd if=/dev/zero of=/tmp/luks-test.img bs=1M count=50
sudo losetup /dev/loop0 /tmp/plain-test.img
sudo losetup /dev/loop1 /tmp/luks-test.img

# Configurar LUKS en loop1
sudo cryptsetup luksFormat /dev/loop1

# Abrir ambos
sudo cryptsetup open --type plain /dev/loop0 plain_test
sudo cryptsetup open /dev/loop1 luks_test
```

> **Pregunta de predicción**: si ejecutas `hexdump -C /tmp/plain-test.img |
> head` y `hexdump -C /tmp/luks-test.img | head`, ¿qué diferencia verás al
> principio de cada archivo? ¿Cómo puede un forense determinar si un disco
> usa LUKS?
>
> **Respuesta**: `plain-test.img` mostrará datos aparentemente aleatorios
> desde el byte 0 (sin header distinguible). `luks-test.img` mostrará el
> magic number `LUKS\xba\xbe` en los primeros bytes, seguido del header JSON
> de LUKS2. Un forense puede identificar LUKS con `file imagen.img` (detecta
> el magic) o `cryptsetup isLuks /dev/X`. Por eso plain mode ofrece
> "negabilidad plausible": sin header, el disco parece contener datos
> aleatorios y no se puede probar que está cifrado.

```bash
# Limpiar
sudo cryptsetup close plain_test
sudo cryptsetup close luks_test
sudo losetup -d /dev/loop0 /dev/loop1
rm /tmp/plain-test.img /tmp/luks-test.img
```

### Ejercicio 3: Benchmark de cifrado

```bash
# Paso 1: Ejecutar benchmark
cryptsetup benchmark
```

> **Pregunta de predicción**: ¿cuál de los algoritmos será más rápido en un
> CPU con AES-NI: `aes-xts` o `serpent-xts`? ¿Aproximadamente cuántas veces
> más rápido?
>
> **Respuesta**: `aes-xts` será **5-10 veces** más rápido que `serpent-xts`
> en un CPU con AES-NI. AES-NI son instrucciones de hardware dedicadas al
> algoritmo AES; Serpent no tiene aceleración por hardware y se ejecuta
> puramente en software. Típicamente verás AES-XTS-256 a ~2500 MB/s y
> Serpent-XTS-256 a ~300 MB/s. Esta diferencia masiva es la razón principal
> por la que AES es el estándar universal para cifrado de disco.

---

> **Sección completada**: S02 — Criptografía y PKI (OpenSSL, LUKS, dm-crypt).
>
> **Capítulo completado**: C03 — Hardening y Auditoría.
>
> **Siguiente capítulo**: C04 — Proceso de Arranque, S01 — Boot, T01 —
> Firmware BIOS/UEFI.
