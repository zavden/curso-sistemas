# GPG: generación de claves, cifrado, firma, verificación y keyservers

## Índice

1. [Criptografía asimétrica: el modelo de GPG](#1-criptografía-asimétrica-el-modelo-de-gpg)
2. [Instalación y configuración](#2-instalación-y-configuración)
3. [Generación de claves](#3-generación-de-claves)
4. [Gestión del keyring](#4-gestión-del-keyring)
5. [Cifrado y descifrado](#5-cifrado-y-descifrado)
6. [Firma y verificación](#6-firma-y-verificación)
7. [Keyservers y Web of Trust](#7-keyservers-y-web-of-trust)
8. [GPG en la práctica sysadmin](#8-gpg-en-la-práctica-sysadmin)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Criptografía asimétrica: el modelo de GPG

GPG (GNU Privacy Guard) implementa el estándar OpenPGP (RFC 4880). Usa
criptografía asimétrica: un par de claves matemáticamente relacionadas.

```
Par de claves GPG
═══════════════════

  ┌─────────────────┐      ┌─────────────────┐
  │  Clave PÚBLICA  │      │  Clave PRIVADA   │
  │  (public key)   │      │  (secret key)    │
  ├─────────────────┤      ├─────────────────┤
  │ Se comparte     │      │ NUNCA se comparte│
  │ con todos       │      │ Solo tú la tienes│
  │                 │      │                  │
  │ Usada para:     │      │ Usada para:      │
  │  • Cifrar       │      │  • Descifrar     │
  │  • Verificar    │      │  • Firmar        │
  │    firmas       │      │                  │
  └─────────────────┘      └─────────────────┘

  Relación:
    Lo que cifras con la pública → solo la privada descifra
    Lo que firmas con la privada → la pública verifica
```

### Cifrado vs firma

```
Dos operaciones fundamentales
══════════════════════════════

  CIFRADO: proteger CONFIDENCIALIDAD
  ────────────────────────────────────
  Alice quiere enviar un mensaje secreto a Bob

    Alice                                    Bob
    ┌──────────┐    cifrado con     ┌──────────────┐
    │ Mensaje  │ → clave PÚBLICA → │ Mensaje       │
    │ original │    de Bob          │ cifrado       │
    └──────────┘                    └──────┬───────┘
                                           │
                               descifrado con clave
                               PRIVADA de Bob
                                           │
                                    ┌──────┴───────┐
                                    │ Mensaje      │
                                    │ original     │
                                    └──────────────┘

  Solo Bob puede leerlo (solo él tiene su clave privada)


  FIRMA: proteger AUTENTICIDAD e INTEGRIDAD
  ──────────────────────────────────────────
  Alice quiere demostrar que ella escribió el mensaje

    Alice                                    Bob
    ┌──────────┐    firma con       ┌──────────────┐
    │ Mensaje  │ → clave PRIVADA → │ Mensaje       │
    │ original │    de Alice        │ + firma       │
    └──────────┘                    └──────┬───────┘
                                           │
                               verificado con clave
                               PÚBLICA de Alice
                                           │
                                    ┌──────┴───────┐
                                    │ "Firma       │
                                    │  válida"     │
                                    └──────────────┘

  Cualquiera puede verificar que Alice firmó (clave pública)
  Nadie puede falsificar la firma (necesitaría la privada)
```

### Estructura interna de una clave GPG

```
Estructura de un par de claves GPG
════════════════════════════════════

  Clave maestra (master key)
  ├── Key ID: 0xABCD1234EFGH5678
  ├── Fingerprint: ABCD 1234 EFGH 5678 ... (40 hex chars)
  ├── Algoritmo: RSA-4096 o Ed25519
  ├── Creación: 2026-03-21
  ├── Expiración: 2028-03-21 (recomendado: 2 años)
  ├── UIDs (identidades):
  │   ├── "Alice Admin <alice@example.com>"
  │   └── "Alice Admin <alice@company.org>"
  └── Subclaves (subkeys):
      ├── [S] Signing subkey (firmar)
      ├── [E] Encryption subkey (cifrar)
      └── [A] Authentication subkey (autenticación SSH, opcional)

  La clave maestra tiene capability [C] = Certify
  (firma otras claves y UIDs)
```

> **Clave maestra vs subclaves**: la clave maestra es tu identidad. Las
> subclaves hacen el trabajo diario. Si una subclave se compromete, puedes
> revocarla sin perder tu identidad. Si la maestra se compromete, pierdes
> todo.

---

## 2. Instalación y configuración

### Instalación

```bash
# GPG suele venir preinstalado

# RHEL / Fedora
sudo dnf install gnupg2

# Ubuntu / Debian
sudo apt install gnupg

# Verificar versión
gpg --version
# gpg (GnuPG) 2.4.x
# ...
# Supported algorithms:
# Pubkey: RSA, ELG, DSA, ECDH, ECDSA, EDDSA
# Cipher: IDEA, 3DES, CAST5, BLOWFISH, AES, AES192, AES256, ...
# Hash: SHA1, RIPEMD160, SHA256, SHA384, SHA512, SHA224
```

### Directorio de GPG

```
~/.gnupg/ — el keyring
═══════════════════════

  ~/.gnupg/
  ├── pubring.kbx         ← Keyring: claves públicas (y privadas)
  ├── trustdb.gpg         ← Base de datos de confianza
  ├── gpg.conf            ← Configuración de gpg
  ├── gpg-agent.conf      ← Configuración del agente
  ├── private-keys-v1.d/  ← Claves privadas (protegidas)
  │   └── *.key
  ├── openpgp-revocs.d/   ← Certificados de revocación
  │   └── *.rev
  └── tofu.db             ← Trust on First Use (TOFU)

  Permisos:
    chmod 700 ~/.gnupg
    chmod 600 ~/.gnupg/*
```

### Configuración recomendada

```bash
# ~/.gnupg/gpg.conf

# Preferir algoritmos fuertes
personal-cipher-preferences AES256 AES192 AES
personal-digest-preferences SHA512 SHA384 SHA256
personal-compress-preferences ZLIB BZIP2 ZIP Uncompressed

# Mostrar fingerprint largo
keyid-format 0xlong
with-fingerprint

# Algoritmo de digest para firmas
cert-digest-algo SHA512
default-preference-list SHA512 SHA384 SHA256 AES256 AES192 AES ZLIB BZIP2 ZIP Uncompressed

# No usar SHA1 (débil)
weak-digest SHA1

# Obtener claves de keyserver automáticamente al verificar
# auto-key-retrieve
```

---

## 3. Generación de claves

### Generación interactiva

```bash
gpg --full-generate-key
```

```
Proceso interactivo
════════════════════

  gpg --full-generate-key

  Please select what kind of key you want:
     (1) RSA and RSA
     (2) DSA and Elgamal
     (3) DSA (sign only)
     (4) RSA (sign only)
     (9) ECC (sign and encrypt)    ← Recomendado (moderno)
    (10) ECC (sign only)
  Your selection? 9

  Please select which elliptic curve you want:
     (1) Curve 25519    ← Recomendado (rápido, seguro)
     (3) NIST P-256
     (4) NIST P-384
  Your selection? 1

  Please specify how long the key should be valid.
     0 = key does not expire
    <n>  = key expires in n days
    <n>w = key expires in n weeks
    <n>m = key expires in n months
    <n>y = key expires in n years
  Key is valid for? 2y    ← 2 años (recomendado)

  Real name: Alice Admin
  Email address: alice@example.com
  Comment:                          ← Dejar vacío normalmente
  You selected this USER-ID:
      "Alice Admin <alice@example.com>"

  Change (N)ame, (C)omment, (E)mail or (O)kay/(Q)uit? O

  ┌──────────────────────────────────┐
  │ Enter passphrase:  ************  │  ← Protege la clave privada
  │ Repeat:            ************  │
  └──────────────────────────────────┘

  gpg: key 0xABCD1234EFGH5678 marked as ultimately trusted
  public and secret key created and signed.
```

### Generación rápida

```bash
# Para scripts o generación no interactiva
gpg --batch --generate-key <<EOF
%no-protection
Key-Type: eddsa
Key-Curve: ed25519
Key-Usage: sign
Subkey-Type: ecdh
Subkey-Curve: cv25519
Subkey-Usage: encrypt
Name-Real: Deploy Bot
Name-Email: deploy@example.com
Expire-Date: 1y
%commit
EOF

# %no-protection = sin passphrase (solo para cuentas de servicio)
```

### RSA vs ECC

```
Algoritmos de clave
════════════════════

  RSA-4096:
    ✓ Compatibilidad universal
    ✓ Bien entendido, décadas de uso
    ✗ Claves grandes (4096 bits)
    ✗ Operaciones más lentas

  Ed25519 / Curve25519 (ECC):
    ✓ Claves pequeñas (256 bits = seguridad ~128 bits)
    ✓ Operaciones mucho más rápidas
    ✓ Resistente a timing attacks por diseño
    ✗ No soportado por GPG 1.x o sistemas muy antiguos

  Recomendación:
    → Ed25519 si todos los sistemas son modernos (GPG 2.1+)
    → RSA-4096 si necesitas compatibilidad con sistemas legacy
```

### Expiración

```
¿Por qué poner fecha de expiración?
═════════════════════════════════════

  Sin expiración:
    → Si pierdes la clave privada Y no tienes certificado de revocación
    → La clave pública circula PARA SIEMPRE como "válida"
    → Nadie puede revocarla

  Con expiración (2 años):
    → Si pierdes todo, la clave expira sola
    → Puedes extender la expiración antes de que venza
    → Extender requiere la clave privada (prueba de posesión)

  La expiración se puede cambiar en cualquier momento
  (mientras tengas la clave privada):
    gpg --edit-key KEY_ID
    gpg> expire
    gpg> save
```

---

## 4. Gestión del keyring

### Listar claves

```bash
# Listar claves públicas
gpg --list-keys
gpg -k
# pub   ed25519 2026-03-21 [SC] [expires: 2028-03-21]
#       ABCDEF1234567890ABCDEF1234567890ABCDEF12
# uid           [ultimate] Alice Admin <alice@example.com>
# sub   cv25519 2026-03-21 [E] [expires: 2028-03-21]

# Listar claves privadas
gpg --list-secret-keys
gpg -K
# sec   ed25519 2026-03-21 [SC] [expires: 2028-03-21]
#       ABCDEF1234567890ABCDEF1234567890ABCDEF12
# uid           [ultimate] Alice Admin <alice@example.com>
# ssb   cv25519 2026-03-21 [E] [expires: 2028-03-21]
```

```
Interpretar la salida
══════════════════════

  pub / sec    → Clave maestra (pub=pública, sec=secreta)
  sub / ssb    → Subclave (sub=pública, ssb=secreta)
  ed25519      → Algoritmo
  [SC]         → Capabilities: Sign + Certify
  [E]          → Capability: Encrypt
  [A]          → Capability: Authenticate
  [ultimate]   → Nivel de confianza (tu propia clave)
  expires:     → Fecha de expiración
```

### Exportar claves

```bash
# Exportar clave PÚBLICA (para compartir)
gpg --export --armor alice@example.com > alice-public.asc
# --armor = formato ASCII (texto legible, empieza con -----BEGIN PGP PUBLIC KEY BLOCK-----)
# Sin --armor = formato binario

# Exportar clave PRIVADA (¡backup seguro!)
gpg --export-secret-keys --armor alice@example.com > alice-private.asc
# ¡PROTEGER! Cifrar con otro medio, guardar offline

# Exportar solo subclaves privadas (para uso diario en otro equipo)
gpg --export-secret-subkeys --armor alice@example.com > alice-subkeys.asc
# La clave maestra queda en el equipo principal (más seguro)
```

### Importar claves

```bash
# Importar clave pública de otra persona
gpg --import bob-public.asc
# gpg: key 0x1234...: public key "Bob Dev <bob@example.com>" imported

# Importar tu propia clave privada (en un equipo nuevo)
gpg --import alice-private.asc
# Pedirá la passphrase

# Importar desde un keyserver
gpg --keyserver keys.openpgp.org --recv-keys 0xABCD1234EFGH5678
```

### Eliminar claves

```bash
# Eliminar clave pública (primero eliminar la privada si existe)
gpg --delete-keys bob@example.com

# Eliminar clave privada
gpg --delete-secret-keys alice@example.com
# Pide confirmación múltiple (es destructivo)

# Eliminar ambas
gpg --delete-secret-and-public-keys alice@example.com
```

### Certificado de revocación

```bash
# Generar certificado de revocación (hacer INMEDIATAMENTE tras crear la clave)
gpg --gen-revoke --armor alice@example.com > alice-revoke.asc

# GPG 2.1+ genera uno automáticamente en:
# ~/.gnupg/openpgp-revocs.d/FINGERPRINT.rev

# Guardar en un lugar SEGURO y OFFLINE
# (USB cifrado, caja fuerte, papel impreso)

# Si la clave se compromete, importar el certificado:
gpg --import alice-revoke.asc
# → La clave queda marcada como revocada
# → Publicar la clave revocada al keyserver
gpg --keyserver keys.openpgp.org --send-keys FINGERPRINT
```

---

## 5. Cifrado y descifrado

### Cifrar un archivo

```bash
# Cifrar para un destinatario (asimétrico)
gpg --encrypt --recipient bob@example.com secret.txt
# Genera: secret.txt.gpg (binario)

# Cifrar con armor (ASCII, para email)
gpg --encrypt --armor --recipient bob@example.com secret.txt
# Genera: secret.txt.asc

# Cifrar para múltiples destinatarios
gpg --encrypt -r bob@example.com -r charlie@example.com secret.txt
# Ambos pueden descifrar (cada uno con su clave privada)

# Cifrar para ti mismo también (para poder descifrar tu propia copia)
gpg --encrypt -r bob@example.com -r alice@example.com secret.txt

# Cifrar desde stdin
echo "mensaje secreto" | gpg --encrypt --armor -r bob@example.com
```

### Cifrado simétrico (con contraseña)

```bash
# Cifrar con contraseña (sin claves GPG)
gpg --symmetric --cipher-algo AES256 secret.txt
# Pide contraseña
# Genera: secret.txt.gpg

# Cualquiera con la contraseña puede descifrar
# Útil para: backups cifrados, archivos compartidos por canal seguro

# Con armor:
gpg --symmetric --armor --cipher-algo AES256 secret.txt
```

### Descifrar

```bash
# Descifrar (detecta automáticamente simétrico vs asimétrico)
gpg --decrypt secret.txt.gpg
# Salida a stdout

# Descifrar a un archivo
gpg --decrypt --output secret-decrypted.txt secret.txt.gpg
# o
gpg -d -o secret-decrypted.txt secret.txt.gpg

# Si es cifrado asimétrico → pide passphrase de tu clave privada
# Si es cifrado simétrico → pide la contraseña de cifrado
```

### Cifrar + firmar (lo más común)

```bash
# Cifrar Y firmar (confidencialidad + autenticidad)
gpg --encrypt --sign -r bob@example.com message.txt
# Bob puede:
# 1. Descifrar (con su clave privada)
# 2. Verificar que Alice lo firmó (con la clave pública de Alice)

# Es la operación más completa y recomendada
```

---

## 6. Firma y verificación

### Tipos de firma

```
Tres tipos de firma GPG
═════════════════════════

  1. Firma integrada (--sign)
     → El mensaje y la firma están en el mismo archivo
     → El archivo resultante es binario o armor
     → Para descifrar/verificar se usa gpg --verify

  2. Firma separada (--detach-sign)
     → La firma va en un archivo aparte (.sig o .asc)
     → El archivo original no se modifica
     → Ideal para: binarios, ISOs, paquetes

  3. Firma cleartext (--clear-sign)
     → El mensaje queda legible en texto plano
     → La firma se agrega al final del texto
     → Ideal para: emails, anuncios, changelogs
```

### Firma integrada

```bash
# Firmar (genera archivo binario)
gpg --sign document.txt
# Genera: document.txt.gpg

# Firmar con armor (texto legible)
gpg --sign --armor document.txt
# Genera: document.txt.asc

# Verificar
gpg --verify document.txt.gpg
# gpg: Signature made Sat 21 Mar 2026 10:00:00
# gpg:                using EDDSA key ABCDEF...
# gpg: Good signature from "Alice Admin <alice@example.com>"
```

### Firma separada (detached)

```bash
# Crear firma separada
gpg --detach-sign package-1.0.tar.gz
# Genera: package-1.0.tar.gz.sig (binario)

# Con armor
gpg --detach-sign --armor package-1.0.tar.gz
# Genera: package-1.0.tar.gz.asc (texto)

# Verificar (necesitas ambos archivos)
gpg --verify package-1.0.tar.gz.sig package-1.0.tar.gz
# gpg: Good signature from "Alice Admin <alice@example.com>"

# Si el archivo fue modificado:
gpg --verify package-1.0.tar.gz.sig package-1.0-modified.tar.gz
# gpg: BAD signature from "Alice Admin <alice@example.com>"
```

### Firma cleartext

```bash
# Firmar texto manteniendo legibilidad
gpg --clear-sign announcement.txt
# Genera: announcement.txt.asc

# Contenido del .asc:
# -----BEGIN PGP SIGNED MESSAGE-----
# Hash: SHA512
#
# Important security update for version 2.0
# Please update immediately.
#
# -----BEGIN PGP SIGNATURE-----
# iQIzBAEBCgAdFiEE...
# ...
# -----END PGP SIGNATURE-----

# Verificar
gpg --verify announcement.txt.asc
```

### Resultados de verificación

```
Posibles resultados de gpg --verify
═════════════════════════════════════

  "Good signature from..."
    → Firma válida, archivo no modificado
    → Verificar que el firmante es quien esperas

  "BAD signature from..."
    → El archivo fue MODIFICADO después de firmar
    → O la firma es de otro archivo
    → NO confiar en el contenido

  "Can't check signature: No public key"
    → No tienes la clave pública del firmante
    → Necesitas importarla: gpg --recv-keys KEY_ID

  "WARNING: This key is not certified with a trusted signature!"
    → La firma es criptográficamente válida
    → Pero no has verificado la identidad del firmante
    → La clave pública puede ser de un impostor
```

---

## 7. Keyservers y Web of Trust

### Keyservers

Los keyservers son repositorios públicos de claves GPG:

```bash
# Principales keyservers
# keys.openpgp.org      ← Recomendado (verificación de email)
# keyserver.ubuntu.com  ← Popular, sincronizado
# pgp.mit.edu           ← Histórico

# Publicar tu clave pública
gpg --keyserver keys.openpgp.org --send-keys FINGERPRINT

# Buscar la clave de alguien
gpg --keyserver keys.openpgp.org --search-keys bob@example.com

# Descargar una clave por fingerprint
gpg --keyserver keys.openpgp.org --recv-keys 0xABCD1234EFGH5678

# Actualizar claves (buscar revocaciones, extensiones de expiración)
gpg --keyserver keys.openpgp.org --refresh-keys
```

### Configurar keyserver por defecto

```bash
# En ~/.gnupg/gpg.conf
keyserver hkps://keys.openpgp.org

# hkps:// = HKP sobre TLS (cifrado)
# hkp://  = HKP sin cifrar (evitar)
```

### Web of Trust vs TOFU

```
Modelos de confianza
═════════════════════

  Web of Trust (modelo clásico):
  ──────────────────────────────
  "Confío en Bob porque Alice firmó la clave de Bob,
   y yo confío en Alice."

    Tu ──confías──► Alice ──firmó──► Bob
                                      │
                            ∴ Bob es "probablemente" legítimo

  Niveles de confianza:
    unknown   → No sabes nada sobre esta persona
    none      → No confías en su criterio para firmar claves
    marginal  → Confías parcialmente
    full      → Confías completamente
    ultimate  → Tu propia clave


  TOFU (Trust on First Use):
  ──────────────────────────
  "La primera vez que veo esta clave para este email,
   la acepto. Si cambia, alerta."

  Más simple, similar a SSH known_hosts.
  Habilitado por defecto en GPG 2.x:
    trust-model tofu+pgp    (en gpg.conf)
```

### Firmar claves de otros (Web of Trust)

```bash
# Verificaste la identidad de Bob en persona (ej: conferencia)
# Firmaste su clave con la tuya:
gpg --sign-key bob@example.com

# Exportar la firma para Bob
gpg --export --armor bob@example.com > bob-signed-by-alice.asc

# Bob importa tu firma
gpg --import bob-signed-by-alice.asc

# Bob publica la clave actualizada (con tu firma)
gpg --keyserver keys.openpgp.org --send-keys BOB_FINGERPRINT
```

---

## 8. GPG en la práctica sysadmin

### Verificar paquetes y repositorios

```bash
# RPM: verificar firma de un paquete
rpm -K package.rpm
# package.rpm: digests signatures OK

# RPM: importar clave GPG de un repositorio
sudo rpm --import https://packages.example.com/RPM-GPG-KEY-example

# APT: agregar clave de repositorio (método moderno)
wget -qO- https://packages.example.com/gpg.key | \
    gpg --dearmor | sudo tee /usr/share/keyrings/example.gpg > /dev/null

# En el archivo .list o .sources:
# deb [signed-by=/usr/share/keyrings/example.gpg] https://...

# Verificar ISOs descargadas
gpg --verify CentOS-Stream-9-latest-x86_64-dvd1.iso.asc \
             CentOS-Stream-9-latest-x86_64-dvd1.iso
```

### Cifrar backups

```bash
# Cifrar un backup con clave GPG (asimétrico)
tar czf - /etc /var/lib/important | \
    gpg --encrypt -r backup@example.com > backup-2026-03-21.tar.gz.gpg

# Cifrar un backup con contraseña (simétrico, más simple)
tar czf - /etc /var/lib/important | \
    gpg --symmetric --cipher-algo AES256 > backup-2026-03-21.tar.gz.gpg

# Descifrar y extraer
gpg --decrypt backup-2026-03-21.tar.gz.gpg | tar xzf - -C /restore/
```

### Firmar commits de Git

```bash
# Configurar GPG en Git
git config --global user.signingkey FINGERPRINT
git config --global commit.gpgsign true

# Firmar un commit
git commit -S -m "Security patch for CVE-2026-1234"

# Firmar un tag
git tag -s v2.0.0 -m "Release 2.0.0"

# Verificar firma de un commit
git log --show-signature

# Verificar firma de un tag
git tag -v v2.0.0
```

### pass: gestor de contraseñas con GPG

```bash
# pass usa GPG para cifrar contraseñas en archivos individuales
sudo dnf install pass    # RHEL
sudo apt install pass    # Debian

# Inicializar con tu clave GPG
pass init alice@example.com

# Agregar contraseña
pass insert servers/db-production
# Enter password: ********

# Ver contraseña (descifra con tu clave)
pass servers/db-production

# Estructura:
# ~/.password-store/
# ├── servers/
# │   ├── db-production.gpg    ← cifrado con tu clave GPG
# │   └── web-staging.gpg
# └── .gpg-id                  ← fingerprint de la clave
```

### GPG-agent: cache de passphrase

```bash
# gpg-agent evita pedir la passphrase cada vez

# ~/.gnupg/gpg-agent.conf
default-cache-ttl 3600       # Cache por 1 hora
max-cache-ttl 14400          # Máximo 4 horas
# pinentry-program /usr/bin/pinentry-curses  # Para servidores sin GUI

# Reiniciar el agente
gpgconf --kill gpg-agent

# Verificar que está corriendo
gpg-connect-agent /bye
# OK (si está corriendo)
```

---

## 9. Errores comunes

### Error 1: no generar certificado de revocación

```bash
# Si pierdes la clave privada o se compromete, sin certificado
# de revocación no puedes revocar la clave pública

# La clave pública seguirá circulando como "válida"
# La gente seguirá cifrando mensajes que nadie puede leer

# SIEMPRE generar y guardar offline:
gpg --gen-revoke --armor KEY_ID > revoke-cert.asc
# Guardar en USB cifrado, impreso, caja fuerte
```

### Error 2: compartir la clave privada

```bash
# MAL — enviar clave privada por email/chat
gpg --export-secret-keys --armor alice@example.com | mail bob@...
# Cualquiera que intercepte tiene tu identidad

# La clave privada NUNCA debe:
#   ✗ Enviarse por email
#   ✗ Subirse a un keyserver
#   ✗ Estar en un repositorio git
#   ✗ Compartirse con nadie

# Solo exportar para BACKUP personal seguro
```

### Error 3: confundir Key ID corto con largo

```bash
# Key ID corto (8 caracteres) tiene COLISIONES conocidas
# Un atacante puede generar una clave con el mismo ID corto

# MAL — usar short ID
gpg --recv-keys ABCD1234

# BIEN — usar fingerprint completo (40 caracteres)
gpg --recv-keys ABCDEF1234567890ABCDEF1234567890ABCDEF12

# Configurar GPG para mostrar siempre el ID largo:
# ~/.gnupg/gpg.conf
keyid-format 0xlong
```

### Error 4: no poner passphrase en la clave privada

```bash
# Sin passphrase, cualquiera que acceda a tu ~/.gnupg/ tiene tu clave
# Un backup robado, un disco sin cifrar, acceso al servidor

# La passphrase es la última línea de defensa:
#   Archivo .gnupg robado + sin passphrase → identidad comprometida
#   Archivo .gnupg robado + con passphrase → atacante aún necesita la contraseña

# Excepción: cuentas de servicio automatizadas donde no hay humano
# para escribir la passphrase (pero proteger por otros medios)
```

### Error 5: cifrar sin incluirte como destinatario

```bash
# MAL — cifrar para Bob pero no para ti
gpg --encrypt -r bob@example.com secret.txt
# Tú NO puedes descifrar tu propia copia

# BIEN — incluirte como destinatario también
gpg --encrypt -r bob@example.com -r alice@example.com secret.txt

# O configurar en gpg.conf:
# default-recipient-self
# Siempre incluye tu clave como destinatario adicional
```

---

## 10. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════╗
║                       GPG — Cheatsheet                         ║
╠══════════════════════════════════════════════════════════════════╣
║                                                                  ║
║  GENERAR CLAVES                                                  ║
║  gpg --full-generate-key          Generación interactiva         ║
║  gpg --gen-revoke KEY_ID          Certificado de revocación      ║
║                                                                  ║
║  LISTAR                                                          ║
║  gpg -k / gpg --list-keys         Claves públicas               ║
║  gpg -K / gpg --list-secret-keys  Claves privadas               ║
║                                                                  ║
║  EXPORTAR / IMPORTAR                                             ║
║  gpg --export --armor KEY         Exportar pública (compartir)   ║
║  gpg --export-secret-keys KEY     Exportar privada (¡backup!)   ║
║  gpg --import file.asc            Importar clave                 ║
║                                                                  ║
║  CIFRAR                                                          ║
║  gpg -e -r DEST file              Cifrar para destinatario       ║
║  gpg --symmetric file             Cifrar con contraseña          ║
║  gpg -e --sign -r DEST file       Cifrar + firmar                ║
║                                                                  ║
║  DESCIFRAR                                                       ║
║  gpg -d file.gpg                  Descifrar a stdout             ║
║  gpg -d -o output file.gpg       Descifrar a archivo            ║
║                                                                  ║
║  FIRMAR                                                          ║
║  gpg --sign file                  Firma integrada                ║
║  gpg --detach-sign file           Firma separada (.sig)          ║
║  gpg --clear-sign file            Firma cleartext (.asc)         ║
║  gpg --detach-sign --armor file   Firma separada ASCII (.asc)    ║
║                                                                  ║
║  VERIFICAR                                                       ║
║  gpg --verify file.sig file       Verificar firma separada       ║
║  gpg --verify file.asc            Verificar firma integrada      ║
║                                                                  ║
║  KEYSERVERS                                                      ║
║  gpg --send-keys KEY              Publicar al keyserver          ║
║  gpg --recv-keys KEY              Descargar clave                ║
║  gpg --search-keys email          Buscar en keyserver            ║
║  gpg --refresh-keys               Actualizar todas               ║
║                                                                  ║
║  CONFIANZA                                                       ║
║  gpg --sign-key KEY               Firmar clave de otro           ║
║  gpg --edit-key KEY → trust       Cambiar nivel de confianza     ║
║                                                                  ║
║  ALGORITMOS RECOMENDADOS                                         ║
║  Ed25519 + Curve25519  (ECC, moderno, rápido)                   ║
║  RSA-4096              (compatible, probado)                     ║
║  AES-256               (cifrado simétrico)                      ║
║  SHA-512               (hash)                                    ║
║                                                                  ║
║  USO SYSADMIN                                                    ║
║  rpm -K pkg.rpm                  Verificar paquete RPM           ║
║  rpm --import GPG-KEY            Importar clave de repo          ║
║  git commit -S                   Firmar commit                   ║
║  pass init KEY                   Password manager con GPG        ║
║                                                                  ║
╚══════════════════════════════════════════════════════════════════╝
```

---

## 11. Ejercicios

### Ejercicio 1: Generar y exportar claves

```bash
# Paso 1: Generar un par de claves con Ed25519, expiración 1 año
gpg --full-generate-key
# Seleccionar: ECC, Curve 25519, 1y

# Paso 2: Listar las claves generadas
gpg -k
gpg -K

# Paso 3: Exportar la clave pública
gpg --export --armor tu@email.com > mi-clave-publica.asc
```

> **Pregunta de predicción**: ¿el archivo `mi-clave-publica.asc` contiene
> también tu clave privada? ¿Sería seguro publicar este archivo en tu web
> personal o enviarlo por email?
>
> **Respuesta**: no, `--export` solo exporta la clave **pública**. Es
> completamente seguro publicarlo en tu web, enviarlo por email, subirlo a un
> keyserver, etc. La clave privada solo se exporta con `--export-secret-keys`,
> que nunca deberías hacer excepto para backups seguros. Puedes verificarlo:
> el archivo empieza con `-----BEGIN PGP PUBLIC KEY BLOCK-----`.

### Ejercicio 2: Cifrar y descifrar

```bash
# Paso 1: Crear un archivo con contenido sensible
echo "password: S3cr3t!2026" > credentials.txt

# Paso 2: Cifrar con tu propia clave
gpg --encrypt --armor -r tu@email.com credentials.txt

# Paso 3: Eliminar el original
rm credentials.txt

# Paso 4: Descifrar
gpg --decrypt -o credentials-recovered.txt credentials.txt.asc
```

> **Pregunta de predicción**: si cifras el archivo con `-r bob@example.com`
> pero NO incluyes `-r tu@email.com`, ¿podrás descifrar el archivo tú
> mismo? ¿Por qué?
>
> **Respuesta**: **no**. El archivo se cifra con la clave pública de Bob.
> Solo la clave privada de Bob puede descifrarlo. Tú no tienes la clave
> privada de Bob. Para poder descifrar tu propia copia, debes incluirte
> como destinatario: `gpg -e -r bob@example.com -r tu@email.com file`.

### Ejercicio 3: Firmar y verificar

```bash
# Paso 1: Crear un archivo
echo "Version 2.0 released on 2026-03-21" > release-notes.txt

# Paso 2: Crear firma separada
gpg --detach-sign --armor release-notes.txt

# Paso 3: Verificar
gpg --verify release-notes.txt.asc release-notes.txt

# Paso 4: Modificar el archivo y verificar de nuevo
echo "MODIFIED" >> release-notes.txt
gpg --verify release-notes.txt.asc release-notes.txt
```

> **Pregunta de predicción**: ¿qué resultado esperas en el paso 4 tras
> modificar el archivo? ¿Cambiará el mensaje de "Good signature" a otra cosa?
>
> **Respuesta**: el resultado será `BAD signature from "Tu Nombre"`. La firma
> fue calculada sobre el contenido original. Al modificar el archivo (incluso
> agregando un solo carácter), el hash del contenido cambia y ya no coincide
> con la firma. Esto es exactamente la protección de **integridad** que
> proporciona la firma digital.

---

> **Sección completada**: S01 — Seguridad del Sistema (sudo avanzado, auditd,
> hardening básico, GPG).
>
> **Siguiente sección**: S02 — Criptografía y PKI, T01 — OpenSSL (generación
> de certificados, CSR, verificación, s_client).
