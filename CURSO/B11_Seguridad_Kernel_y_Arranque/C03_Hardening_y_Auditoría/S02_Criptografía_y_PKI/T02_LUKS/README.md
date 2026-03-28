# LUKS: cifrado de disco con cryptsetup

## Índice

1. [Por qué cifrar discos](#1-por-qué-cifrar-discos)
2. [Arquitectura LUKS](#2-arquitectura-luks)
3. [cryptsetup: operaciones básicas](#3-cryptsetup-operaciones-básicas)
4. [Crear un volumen LUKS](#4-crear-un-volumen-luks)
5. [Abrir, usar y cerrar volúmenes](#5-abrir-usar-y-cerrar-volúmenes)
6. [Gestión de key slots](#6-gestión-de-key-slots)
7. [Desbloqueo al arranque](#7-desbloqueo-al-arranque)
8. [Operaciones avanzadas](#8-operaciones-avanzadas)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Por qué cifrar discos

### El problema: datos en reposo

```
Amenazas a datos en reposo (data at rest)
═══════════════════════════════════════════

  Sin cifrado de disco:
  ─────────────────────
  ✗ Robo del servidor físico → acceso a todos los datos
  ✗ Disco descartado sin borrar → datos recuperables
  ✗ Arranque desde USB/CD → acceso a todo el filesystem
  ✗ Disco movido a otro servidor → lectura directa
  ✗ Acceso físico al datacenter → extracción de datos

  Con cifrado de disco:
  ─────────────────────
  ✓ Disco robado → datos ilegibles sin la clave
  ✓ Disco descartado → datos cifrados, irrecuperables
  ✓ Arranque desde USB → no puede leer las particiones
  ✓ Disco en otro servidor → solo ve datos aleatorios
  ✓ Cumple requisitos: GDPR, HIPAA, PCI-DSS
```

### Qué protege y qué NO protege

```
Cifrado de disco: alcance
══════════════════════════

  PROTEGE contra:
    ✓ Acceso físico al disco (robo, decomiso)
    ✓ Lectura de datos tras descartar el hardware
    ✓ Arranque alternativo (live USB)
    ✓ Extracción de disco del servidor

  NO PROTEGE contra:
    ✗ Acceso una vez el sistema está arrancado y desbloqueado
    ✗ Atacante con acceso root al sistema encendido
    ✗ Malware corriendo en el sistema operativo
    ✗ Ataques de red mientras el servidor está activo
    ✗ Cold boot attack (extraer claves de RAM)

  El cifrado de disco protege datos EN REPOSO
  No sustituye a firewall, MAC, hardening, etc.
```

---

## 2. Arquitectura LUKS

### Capas de cifrado en Linux

```
Stack de cifrado de disco en Linux
════════════════════════════════════

  Aplicaciones
       │
       ▼
  Filesystem (ext4, xfs, btrfs)
       │
       ▼
  Device Mapper (dm-crypt)        ← Capa de cifrado del kernel
       │                             Cifra/descifra bloques transparentemente
       ▼
  LUKS Header + Encrypted Data   ← Formato en disco
       │
       ▼
  Disco físico (/dev/sda, /dev/nvme0n1)

  LUKS = Linux Unified Key Setup
  dm-crypt = subsistema del kernel que cifra/descifra
  LUKS usa dm-crypt internamente
```

### Header LUKS

El header LUKS está al principio del dispositivo y contiene toda la
información para descifrar:

```
Estructura del header LUKS2
═════════════════════════════

  ┌────────────────────────────────────────────┐
  │           LUKS2 Header (16 MB)             │
  │                                            │
  │  Magic: LUKS\xba\xbe                       │
  │  Version: 2                                │
  │  UUID: 12345678-abcd-efgh-...              │
  │  Cipher: aes-xts-plain64                   │
  │  Key size: 512 bits (256-bit AES-XTS)      │
  │  Hash: sha256                              │
  │                                            │
  │  Key Slot 0: [ACTIVE] passphrase de admin  │
  │  Key Slot 1: [ACTIVE] passphrase de backup │
  │  Key Slot 2: [INACTIVE]                    │
  │  Key Slot 3: [INACTIVE]                    │
  │  ...                                       │
  │  Key Slot 31: [INACTIVE]                   │
  │                                            │
  │  Cada slot contiene:                       │
  │    - Master Key cifrada con la passphrase  │
  │    - Salt, iteraciones PBKDF2/Argon2       │
  │                                            │
  └────────────────────────────────────────────┘
  ┌────────────────────────────────────────────┐
  │           Datos cifrados                   │
  │      (el filesystem está aquí dentro)      │
  │                                            │
  │   Cifrado con la Master Key               │
  │   (no con la passphrase directamente)      │
  └────────────────────────────────────────────┘
```

### Master Key vs Passphrase

```
Relación entre passphrase y master key
════════════════════════════════════════

  Passphrase → [PBKDF2/Argon2] → Key Encryption Key → descifra → Master Key
                                                                      │
                                                                      ▼
                                                              Cifra/descifra
                                                              los DATOS

  ¿Por qué esta indirección?

  Sin indirección (GPG style):
    Cambiar passphrase = recifrar TODO el disco (horas/días)

  Con indirección (LUKS):
    Cambiar passphrase = recifrar solo la Master Key en el slot
    (milisegundos)
    Los datos no se tocan

  Múltiples passphrases:
    Slot 0: passphrase "admin123" → descifra Master Key
    Slot 1: passphrase "backup#!" → descifra la MISMA Master Key
    Ambas abren el disco, cada una cifra la Master Key por separado
```

### LUKS1 vs LUKS2

```
┌──────────────┬──────────────────┬──────────────────┐
│              │ LUKS1            │ LUKS2            │
├──────────────┼──────────────────┼──────────────────┤
│ Key slots    │ 8                │ 32               │
│ Header size  │ ~2 MB            │ ~16 MB           │
│ KDF          │ PBKDF2           │ Argon2id         │
│              │                  │ (resistente a    │
│              │                  │  GPU/ASIC)       │
│ Metadata     │ Binario fijo     │ JSON flexible    │
│ Integridad   │ No               │ Sí (dm-integrity)│
│ Tokens       │ No               │ Sí (FIDO2, TPM)  │
│ Default en   │ RHEL 7, Ubuntu   │ RHEL 8+, Ubuntu  │
│              │ <18.04           │ 20.04+, Fedora   │
└──────────────┴──────────────────┴──────────────────┘

LUKS2 es el default actual. Usar LUKS1 solo por compatibilidad.
```

---

## 3. cryptsetup: operaciones básicas

```bash
# Instalar (generalmente ya incluido)
sudo dnf install cryptsetup    # RHEL
sudo apt install cryptsetup    # Debian

# Verificar versión
cryptsetup --version
# cryptsetup 2.6.x
```

### Subcomandos principales

```
cryptsetup — subcomandos
═════════════════════════

  luksFormat      Crear volumen LUKS (¡DESTRUYE DATOS!)
  open / luksOpen Desbloquear volumen (crear mapping dm-crypt)
  close           Bloquear volumen (eliminar mapping)
  status          Ver estado de un mapping activo
  luksDump        Ver header LUKS (slots, cifrado, UUID)
  luksAddKey      Agregar passphrase a un slot
  luksRemoveKey   Eliminar passphrase de un slot
  luksChangeKey   Cambiar passphrase de un slot
  luksKillSlot    Eliminar un slot por número
  isLuks          Verificar si un dispositivo es LUKS
  luksHeaderBackup   Backup del header
  luksHeaderRestore  Restaurar header desde backup
  luksSuspend     Suspender I/O (borrar clave de RAM)
  luksResume      Reanudar I/O (reingresar passphrase)
```

---

## 4. Crear un volumen LUKS

### Paso a paso

```bash
# ¡ADVERTENCIA! luksFormat DESTRUYE todos los datos del dispositivo

# 1. Identificar el dispositivo
lsblk
# NAME   MAJ:MIN RM  SIZE RO TYPE MOUNTPOINTS
# sda      8:0    0   20G  0 disk
# ├─sda1   8:1    0    1G  0 part /boot
# └─sda2   8:2    0   19G  0 part /
# sdb      8:16   0   50G  0 disk          ← disco a cifrar

# 2. Formatear con LUKS
sudo cryptsetup luksFormat /dev/sdb

# WARNING!
# ========
# This will overwrite data on /dev/sdb irrevocably.
# Are you sure? (Type 'yes' in capital letters): YES
# Enter passphrase for /dev/sdb: ********
# Verify passphrase: ********
```

### Opciones de formato

```bash
# Con opciones explícitas (recomendado para producción)
sudo cryptsetup luksFormat \
    --type luks2 \
    --cipher aes-xts-plain64 \
    --key-size 512 \
    --hash sha256 \
    --pbkdf argon2id \
    --label "data-encrypted" \
    /dev/sdb

# Opciones:
#   --type luks2         Formato LUKS2 (default en versiones modernas)
#   --cipher aes-xts-plain64  Cifrado AES-256 en modo XTS
#   --key-size 512       512 bits para XTS = 256-bit AES efectivos
#   --hash sha256        Hash para derivación de clave
#   --pbkdf argon2id     KDF resistente a GPU (LUKS2 default)
#   --label              Etiqueta legible (opcional)

# Verificar que se creó correctamente
sudo cryptsetup isLuks /dev/sdb
echo $?  # 0 = es LUKS

# Ver el header
sudo cryptsetup luksDump /dev/sdb
```

### Salida de luksDump

```
LUKS header information
Version:        2
Epoch:          3
Metadata area:  16384 [bytes]
Keyslots area:  16744448 [bytes]
UUID:           12345678-abcd-efgh-ijkl-123456789012
Label:          data-encrypted
Subsystem:      (no subsystem)
Flags:          (no flags)

Data segments:
  0: crypt
    offset:  16777216 [bytes]
    length:  (whole device)
    cipher:  aes-xts-plain64
    sector:  512 [bytes]

Keyslots:
  0: luks2
    Key:        512 bits
    Priority:   normal
    Cipher:     aes-xts-plain64
    Cipher key: 512 bits
    PBKDF:      argon2id
      Time cost: 4
      Memory:    1048576      ← 1 GB de RAM para derivar la clave
      Threads:   4
    AF stripes:  4000
    AF hash:     sha256
    Area offset: 32768 [bytes]
    Area length: 258048 [bytes]
    Digest ID:   0
```

---

## 5. Abrir, usar y cerrar volúmenes

### Abrir (desbloquear)

```bash
# Abrir el volumen LUKS (crear mapping en /dev/mapper/)
sudo cryptsetup open /dev/sdb data_crypt
# Enter passphrase for /dev/sdb: ********

# "data_crypt" es el nombre del mapping
# Ahora existe: /dev/mapper/data_crypt

# Verificar
ls -l /dev/mapper/data_crypt
sudo cryptsetup status data_crypt
# /dev/mapper/data_crypt is active.
#   type:    LUKS2
#   cipher:  aes-xts-plain64
#   keysize: 512 bits
#   device:  /dev/sdb
#   sector size:  512
#   offset:  32768 sectors
#   size:    104824832 sectors
#   mode:    read/write
```

### Crear filesystem y montar

```bash
# Crear filesystem en el volumen descifrado
sudo mkfs.ext4 /dev/mapper/data_crypt
# o
sudo mkfs.xfs /dev/mapper/data_crypt

# Crear punto de montaje
sudo mkdir /data

# Montar
sudo mount /dev/mapper/data_crypt /data

# Verificar
df -h /data
# /dev/mapper/data_crypt   50G  1.5G  46G   4% /data

lsblk
# sdb           8:16   0   50G  0 disk
# └─data_crypt 253:1   0   50G  0 crypt  /data
```

### Cerrar (bloquear)

```bash
# 1. Desmontar el filesystem
sudo umount /data

# 2. Cerrar el volumen LUKS
sudo cryptsetup close data_crypt

# Ahora /dev/mapper/data_crypt ya no existe
# Los datos en /dev/sdb están cifrados e inaccesibles

# Si intentas montar /dev/sdb directamente:
sudo mount /dev/sdb /data
# mount: wrong fs type, bad option, bad superblock on /dev/sdb
# (ve datos aleatorios, no un filesystem)
```

### Flujo completo

```
Flujo: abrir → usar → cerrar
══════════════════════════════

  /dev/sdb (cifrado)
       │
  cryptsetup open /dev/sdb data_crypt
  (pide passphrase)
       │
       ▼
  /dev/mapper/data_crypt (descifrado)
       │
  mount /dev/mapper/data_crypt /data
       │
       ▼
  /data/ (usable, lectura/escritura)
  ... trabajar con los datos ...
       │
  umount /data
       │
       ▼
  /dev/mapper/data_crypt (descifrado pero sin montar)
       │
  cryptsetup close data_crypt
       │
       ▼
  /dev/sdb (cifrado, inaccesible)
```

---

## 6. Gestión de key slots

LUKS permite múltiples passphrases (hasta 32 en LUKS2) para el mismo volumen.
Cada passphrase ocupa un "key slot":

### Ver key slots

```bash
sudo cryptsetup luksDump /dev/sdb | grep -A1 "Keyslots:"
# Keyslots:
#   0: luks2           ← Slot 0 activo
#   1: (empty)
#   2: (empty)
#   ...
```

### Agregar una passphrase

```bash
sudo cryptsetup luksAddKey /dev/sdb
# Enter any existing passphrase: ********     ← passphrase actual
# Enter new passphrase for key slot: ********  ← nueva passphrase
# Verify passphrase: ********

# Ahora el slot 1 también está activo
# Ambas passphrases abren el volumen

# Agregar en un slot específico
sudo cryptsetup luksAddKey --key-slot 3 /dev/sdb
```

### Agregar un keyfile

```bash
# Un keyfile es un archivo que actúa como passphrase
# Útil para desbloqueo automático

# 1. Generar keyfile aleatorio
sudo dd if=/dev/urandom of=/root/luks-keyfile bs=4096 count=1
sudo chmod 400 /root/luks-keyfile

# 2. Agregar el keyfile como clave del volumen
sudo cryptsetup luksAddKey /dev/sdb /root/luks-keyfile
# Enter any existing passphrase: ********

# 3. Ahora puedes abrir con el keyfile (sin passphrase interactiva)
sudo cryptsetup open /dev/sdb data_crypt --key-file /root/luks-keyfile
```

### Eliminar una passphrase

```bash
# Por passphrase (introduce la que quieres eliminar)
sudo cryptsetup luksRemoveKey /dev/sdb
# Enter passphrase to be deleted: ********

# Por número de slot
sudo cryptsetup luksKillSlot /dev/sdb 1
# Enter any remaining passphrase: ********  ← necesitas otra para autenticarte
```

### Cambiar una passphrase

```bash
sudo cryptsetup luksChangeKey /dev/sdb
# Enter passphrase to be changed: ********   ← la antigua
# Enter new passphrase: ********              ← la nueva
# Verify passphrase: ********
```

> **Regla de seguridad**: nunca elimines la última passphrase de un volumen.
> `cryptsetup` te advierte si intentas hacerlo, pero mantén siempre al menos
> dos métodos de acceso (passphrase + keyfile, o dos passphrases).

---

## 7. Desbloqueo al arranque

### /etc/crypttab

`/etc/crypttab` es el equivalente de `/etc/fstab` para volúmenes LUKS:

```bash
# Formato:
# name    device              keyfile      options
# ────    ──────              ───────      ───────

# Con passphrase interactiva al arranque
data_crypt  UUID=12345678-abcd-...  none  luks

# Con keyfile (desbloqueo automático)
data_crypt  UUID=12345678-abcd-...  /root/luks-keyfile  luks

# Obtener UUID del dispositivo LUKS
sudo cryptsetup luksDump /dev/sdb | grep UUID
# UUID: 12345678-abcd-efgh-ijkl-123456789012
# o
sudo blkid /dev/sdb
# /dev/sdb: UUID="12345678-abcd-..." TYPE="crypto_LUKS"
```

### Ejemplo completo: partición de datos cifrada

```bash
# 1. Crear volumen LUKS
sudo cryptsetup luksFormat /dev/sdb

# 2. Crear keyfile para desbloqueo automático
sudo dd if=/dev/urandom of=/root/luks-data.key bs=4096 count=1
sudo chmod 400 /root/luks-data.key
sudo cryptsetup luksAddKey /dev/sdb /root/luks-data.key

# 3. Abrir, formatear, montar
sudo cryptsetup open /dev/sdb data_crypt --key-file /root/luks-data.key
sudo mkfs.xfs /dev/mapper/data_crypt
sudo mkdir /data

# 4. Configurar crypttab
echo "data_crypt UUID=$(sudo blkid -s UUID -o value /dev/sdb) /root/luks-data.key luks" \
    | sudo tee -a /etc/crypttab

# 5. Configurar fstab
echo "/dev/mapper/data_crypt /data xfs defaults 0 0" \
    | sudo tee -a /etc/fstab

# 6. Verificar
sudo mount -a
df -h /data
```

### Opciones de crypttab

```
Opciones comunes en crypttab
═════════════════════════════

  luks            Indica que es un volumen LUKS
  discard         Pasar TRIM al SSD (mejor rendimiento,
                  pero revela qué bloques están vacíos)
  noauto          No abrir al arranque (abrir manualmente)
  nofail          No fallar el arranque si el disco no existe
  x-systemd.device-timeout=30  Timeout para esperar el dispositivo

  Ejemplo con SSD:
  data_crypt  UUID=...  /root/key  luks,discard

  Ejemplo con passphrase interactiva:
  data_crypt  UUID=...  none  luks
  (systemd pedirá la passphrase durante el arranque)
```

### Desbloqueo interactivo vs automático

```
Estrategias de desbloqueo al arranque
══════════════════════════════════════

  Passphrase interactiva:
  ───────────────────────
  crypttab: data_crypt UUID=... none luks

  ✓ Más seguro (necesita humano)
  ✗ No apto para servidores remotos sin consola
  ✗ El servidor no arranca solo tras reboot

  Uso: laptops, estaciones de trabajo


  Keyfile en disco:
  ─────────────────
  crypttab: data_crypt UUID=... /root/luks.key luks

  ✓ Arranque automático
  ✗ Si la partición de root NO está cifrada, el keyfile
    es accesible (cifrado parcial)
  ✓ Si root TAMBIÉN está cifrada con passphrase, solo
    necesitas ingresar una passphrase para desbloquear root
    y el keyfile desbloquea las demás

  Uso: servidores con root cifrada + datos cifrados


  TPM (Trusted Platform Module):
  ──────────────────────────────
  El chip TPM almacena la clave, la libera solo si:
    - El hardware no cambió
    - El bootloader no fue manipulado
    - Secure Boot está activo

  systemd-cryptenroll --tpm2-device=auto /dev/sdb

  ✓ Arranque automático + seguro
  ✓ Protege contra arranque con USB/otro disco
  ✗ Requiere TPM 2.0
  ✗ Más complejo de configurar

  Uso: servidores modernos con TPM 2.0


  Clevis + Tang (Network Bound Encryption):
  ──────────────────────────────────────────
  El servidor desbloquea SOLO si puede contactar
  un servidor Tang en la red local.

  ✓ Arranque automático si está en la red correcta
  ✓ Disco robado fuera de la red → inaccesible
  ✗ Requiere infraestructura Tang

  Uso: datacenters con seguridad física
```

---

## 8. Operaciones avanzadas

### Backup y restauración del header

```bash
# CRÍTICO: si el header LUKS se corrompe, PIERDES TODOS LOS DATOS
# Siempre hacer backup del header

# Backup del header
sudo cryptsetup luksHeaderBackup /dev/sdb --header-backup-file /root/sdb-luks-header.bak
# Guardar este archivo en un lugar SEGURO (offline, cifrado)

# Restaurar header (si se corrompe)
sudo cryptsetup luksHeaderRestore /dev/sdb --header-backup-file /root/sdb-luks-header.bak
# ¡CUIDADO! Restaurar un header antiguo puede dejar slots inválidos
# (si cambiaste passphrases después del backup)
```

### Ver información sin abrir

```bash
# Verificar si un dispositivo es LUKS
sudo cryptsetup isLuks /dev/sdb && echo "Es LUKS" || echo "No es LUKS"

# Ver header completo
sudo cryptsetup luksDump /dev/sdb

# Benchmark de cifrado (ver rendimiento de algoritmos)
cryptsetup benchmark
# Algorithm |  Key |  Encryption |  Decryption
#    aes-xts   256b   3200.0 MiB/s  3300.0 MiB/s
#    aes-xts   512b   2800.0 MiB/s  2900.0 MiB/s
# serpent-xts   256b    700.0 MiB/s   750.0 MiB/s
```

### Cifrar una partición con datos existentes (in-place)

```bash
# LUKS2 permite cifrar sin perder datos (reencrypt)
# ¡HACER BACKUP ANTES! La operación es resistente a interrupciones
# pero un backup es siempre buena idea

# 1. Desmonta la partición
sudo umount /dev/sdb1

# 2. Cifrar in-place
sudo cryptsetup reencrypt --encrypt --type luks2 --reduce-device-size 16M /dev/sdb1
# --reduce-device-size 16M → espacio para el header LUKS2
# Pide passphrase

# 3. Abrir y montar
sudo cryptsetup open /dev/sdb1 data_crypt
sudo mount /dev/mapper/data_crypt /data

# Los datos originales están intactos, ahora cifrados
```

### Suspend y resume

```bash
# Suspender I/O (borra la master key de RAM)
sudo cryptsetup luksSuspend data_crypt
# Toda I/O al volumen se bloquea
# Útil antes de suspender/hibernar (protege contra cold boot)

# Reanudar (requiere passphrase de nuevo)
sudo cryptsetup luksResume data_crypt
# Enter passphrase: ********
# I/O se reanuda
```

---

## 9. Errores comunes

### Error 1: formatear el dispositivo equivocado

```bash
# cryptsetup luksFormat /dev/sda   ← ¡DESTRUYE el disco del sistema!
# vs
# cryptsetup luksFormat /dev/sdb   ← el disco correcto

# SIEMPRE verificar con lsblk ANTES:
lsblk
# Identificar claramente cuál es el disco a cifrar
# Verificar que NO tiene montajes activos

# Doble verificación:
sudo blkid /dev/sdb
# Si muestra un filesystem con datos → ¡PARAR! ¿Estás seguro?
```

### Error 2: perder la passphrase Y el keyfile

```bash
# Sin passphrase ni keyfile → datos IRRECUPERABLES
# No hay "reset de contraseña" en LUKS
# La master key está cifrada con las passphrases
# Sin ellas, los datos se perdieron permanentemente

# Prevención:
# 1. Siempre tener al menos 2 métodos de acceso
# 2. Backup del header (cryptsetup luksHeaderBackup)
# 3. Documentar passphrases en un gestor seguro (pass, vault)
# 4. Keyfile en backup seguro y separado
```

### Error 3: no hacer backup del header

```bash
# Si el header se corrompe (bad sectors, error de escritura):
# → No hay forma de descifrar los datos
# → Incluso con la passphrase correcta

# El header contiene la master key cifrada
# Sin header → sin master key → sin datos

# SIEMPRE:
sudo cryptsetup luksHeaderBackup /dev/sdb \
    --header-backup-file /backup/sdb-header.bak
# Guardar el backup FUERA del disco cifrado
```

### Error 4: usar discard sin entender las implicaciones

```bash
# En crypttab:
# data_crypt UUID=... /root/key luks,discard

# discard pasa TRIM al SSD → mejor rendimiento y longevidad
# PERO: revela qué bloques están vacíos vs escritos
# Un atacante puede determinar cuánto espacio está usado
# y potencialmente inferir patrones de uso

# Para la mayoría de servidores: discard es aceptable
# Para alta seguridad (clasificado/gobierno): no usar discard
```

### Error 5: olvidar regenerar initramfs tras configurar crypttab

```bash
# En RHEL/Fedora, si la partición root está cifrada,
# el initramfs necesita contener cryptsetup y la configuración

# Tras cambiar crypttab:
sudo dracut --force    # RHEL/Fedora
sudo update-initramfs -u    # Debian/Ubuntu

# Sin esto, el sistema puede no arrancar porque
# el initramfs no sabe cómo desbloquear la partición
```

---

## 10. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════╗
║                       LUKS — Cheatsheet                        ║
╠══════════════════════════════════════════════════════════════════╣
║                                                                  ║
║  CREAR                                                           ║
║  cryptsetup luksFormat /dev/sdX        Formatear LUKS            ║
║  cryptsetup luksFormat --type luks2 \                            ║
║    --cipher aes-xts-plain64 /dev/sdX   Con opciones explícitas  ║
║                                                                  ║
║  ABRIR / CERRAR                                                  ║
║  cryptsetup open /dev/sdX name         Desbloquear               ║
║  cryptsetup open /dev/sdX name \                                 ║
║    --key-file /path/key                Con keyfile               ║
║  cryptsetup close name                 Bloquear                  ║
║                                                                  ║
║  INFORMACIÓN                                                     ║
║  cryptsetup isLuks /dev/sdX            ¿Es LUKS?                ║
║  cryptsetup luksDump /dev/sdX          Ver header                ║
║  cryptsetup status name                Ver mapping activo        ║
║  cryptsetup benchmark                  Rendimiento               ║
║                                                                  ║
║  KEY SLOTS                                                       ║
║  cryptsetup luksAddKey /dev/sdX        Agregar passphrase        ║
║  cryptsetup luksAddKey /dev/sdX file   Agregar keyfile           ║
║  cryptsetup luksRemoveKey /dev/sdX     Eliminar passphrase       ║
║  cryptsetup luksKillSlot /dev/sdX N    Eliminar slot N           ║
║  cryptsetup luksChangeKey /dev/sdX     Cambiar passphrase        ║
║                                                                  ║
║  HEADER                                                          ║
║  cryptsetup luksHeaderBackup /dev/sdX \                          ║
║    --header-backup-file backup.bak     Backup (¡HACER SIEMPRE!) ║
║  cryptsetup luksHeaderRestore /dev/sdX \                         ║
║    --header-backup-file backup.bak     Restaurar                 ║
║                                                                  ║
║  ARRANQUE                                                        ║
║  /etc/crypttab:                                                  ║
║  name  UUID=...  none          luks    (passphrase interactiva)  ║
║  name  UUID=...  /root/keyfile luks    (keyfile automático)      ║
║                                                                  ║
║  /etc/fstab:                                                     ║
║  /dev/mapper/name  /mountpoint  xfs  defaults  0  0             ║
║                                                                  ║
║  FLUJO COMPLETO                                                  ║
║  1. luksFormat /dev/sdX                                          ║
║  2. open /dev/sdX name                                           ║
║  3. mkfs.xfs /dev/mapper/name                                    ║
║  4. mount /dev/mapper/name /mountpoint                           ║
║  5. Configurar /etc/crypttab + /etc/fstab                        ║
║  6. luksHeaderBackup                                             ║
║                                                                  ║
║  GENERAR KEYFILE                                                 ║
║  dd if=/dev/urandom of=/root/key bs=4096 count=1                ║
║  chmod 400 /root/key                                             ║
║                                                                  ║
╚══════════════════════════════════════════════════════════════════╝
```

---

## 11. Ejercicios

### Ejercicio 1: Crear y usar un volumen LUKS

```bash
# Usar un archivo como dispositivo de bloque (para practicar sin disco extra)

# Paso 1: Crear un archivo de 100 MB como dispositivo virtual
dd if=/dev/zero of=/tmp/luks-test.img bs=1M count=100

# Paso 2: Asociar a un loop device
sudo losetup /dev/loop0 /tmp/luks-test.img

# Paso 3: Formatear con LUKS
sudo cryptsetup luksFormat /dev/loop0
```

> **Pregunta de predicción**: después de `luksFormat`, si haces
> `file /tmp/luks-test.img`, ¿qué tipo de archivo reportará? ¿Y si intentas
> `mount /dev/loop0 /mnt`?
>
> **Respuesta**: `file` reportará `LUKS encrypted file, ver 2` (o similar) —
> reconoce el magic number LUKS del header. `mount` fallará con `wrong fs
> type` porque el contenido es datos cifrados, no un filesystem. Para montar,
> primero debes `cryptsetup open` (que crea el mapping descifrado) y luego
> montar el mapping `/dev/mapper/name`.

```bash
# Paso 4: Abrir, crear filesystem, montar
sudo cryptsetup open /dev/loop0 test_crypt
sudo mkfs.ext4 /dev/mapper/test_crypt
sudo mkdir -p /mnt/encrypted
sudo mount /dev/mapper/test_crypt /mnt/encrypted

# Paso 5: Crear un archivo de prueba
echo "datos secretos" | sudo tee /mnt/encrypted/secret.txt

# Paso 6: Cerrar
sudo umount /mnt/encrypted
sudo cryptsetup close test_crypt

# Paso 7: Limpiar
sudo losetup -d /dev/loop0
rm /tmp/luks-test.img
```

### Ejercicio 2: Gestionar key slots

```bash
# Usando el loop device del ejercicio anterior (recrear si es necesario)

# Paso 1: Ver los slots activos
sudo cryptsetup luksDump /dev/loop0 | grep -E "Keyslots:|^\s+[0-9]+:"

# Paso 2: Agregar un keyfile
sudo dd if=/dev/urandom of=/tmp/test-keyfile bs=256 count=1
sudo cryptsetup luksAddKey /dev/loop0 /tmp/test-keyfile
```

> **Pregunta de predicción**: después de agregar el keyfile, ¿cuántos slots
> activos habrá? Si abres el volumen con `--key-file /tmp/test-keyfile`,
> ¿necesitarás escribir alguna passphrase?
>
> **Respuesta**: **2 slots activos** (slot 0 con la passphrase original,
> slot 1 con el keyfile). Al abrir con `--key-file`, **no** se pide
> passphrase — el keyfile sustituye a la entrada interactiva. Por eso los
> keyfiles son útiles para automatización (`crypttab` con ruta al keyfile).

```bash
# Paso 3: Abrir con el keyfile
sudo cryptsetup open /dev/loop0 test_crypt --key-file /tmp/test-keyfile
# (no pide passphrase)

sudo cryptsetup close test_crypt
```

### Ejercicio 3: Backup y restauración del header

```bash
# Paso 1: Hacer backup del header
sudo cryptsetup luksHeaderBackup /dev/loop0 \
    --header-backup-file /tmp/header-backup.bin

# Paso 2: Ver el tamaño del backup
ls -lh /tmp/header-backup.bin
```

> **Pregunta de predicción**: ¿aproximadamente cuánto pesará el backup del
> header? ¿El backup contiene la master key en claro o cifrada?
>
> **Respuesta**: para LUKS2, el header ocupa ~16 MB, así que el backup será
> de aproximadamente 16 MB. El backup contiene la master key **cifrada** con
> las passphrases de los slots, no en claro. Si un atacante obtiene el backup
> del header, aún necesita una passphrase válida para derivar la master key.
> Pero proteger el backup es importante: con él + la passphrase = acceso
> completo, incluso si el disco fue destruido (solo el header, los datos
> siguen en el disco original).

---

> **Siguiente tema**: T03 — dm-crypt (capa inferior de LUKS, configuración
> manual).
