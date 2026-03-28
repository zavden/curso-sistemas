# Autenticación por clave pública

## Índice

1. [Criptografía asimétrica en SSH](#criptografía-asimétrica-en-ssh)
2. [Algoritmos de clave](#algoritmos-de-clave)
3. [ssh-keygen](#ssh-keygen)
4. [Distribución de claves](#distribución-de-claves)
5. [authorized_keys](#authorized_keys)
6. [ssh-agent](#ssh-agent)
7. [Gestión de claves del host](#gestión-de-claves-del-host)
8. [known_hosts](#known_hosts)
9. [Escenarios prácticos](#escenarios-prácticos)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## Criptografía asimétrica en SSH

La autenticación por clave pública usa un par de claves matemáticamente vinculadas:

```
    ┌─────────────────────┐         ┌─────────────────────┐
    │   Clave privada     │         │   Clave pública     │
    │   (id_ed25519)      │         │   (id_ed25519.pub)  │
    │                     │         │                     │
    │   SECRETA           │         │   Se puede          │
    │   Solo en tu        │◄───────►│   compartir         │
    │   máquina           │ par     │   libremente        │
    │                     │ mate-   │                     │
    │   Nunca sale        │ mático  │   Va al servidor    │
    │   del cliente       │         │   en authorized_keys│
    └─────────────────────┘         └─────────────────────┘
```

El flujo de autenticación:

```
    Cliente                              Servidor
       │                                    │
       │──── "Quiero autenticar con ───────►│
       │      clave pública X"              │
       │                                    │
       │                              ¿X está en
       │                              authorized_keys?
       │                                    │
       │◄─── Envía un challenge ────────────│
       │     (datos aleatorios)             │
       │                                    │
       │  Firma el challenge                │
       │  con la clave PRIVADA              │
       │                                    │
       │──── Envía la firma ───────────────►│
       │                                    │
       │                              Verifica la firma
       │                              con la clave PÚBLICA
       │                              en authorized_keys
       │                                    │
       │◄─── Autenticación exitosa ─────────│
       │                                    │
```

Lo fundamental: **la clave privada nunca se transmite**. El servidor verifica que el cliente la posee sin necesidad de verla, usando la firma criptográfica. Esto es radicalmente más seguro que passwords, que sí se transmiten (cifradas, pero existen en ambos extremos).

---

## Algoritmos de clave

### Comparativa

| Algoritmo | Tamaño de clave | Tamaño de firma | Seguridad | Velocidad | Recomendación |
|---|---|---|---|---|---|
| **Ed25519** | 256 bits (fijo) | 64 bytes | Excelente | Muy rápida | **Preferido** |
| **ECDSA** | 256/384/521 bits | Variable | Buena | Rápida | Aceptable |
| **RSA** | 2048-4096+ bits | Variable | Buena (≥3072) | Lenta | Legacy, si necesario |
| **DSA** | 1024 bits (fijo) | Variable | Obsoleto | — | **No usar** |

### Ed25519 — la elección moderna

Ed25519 (Curve25519 + EdDSA) es el estándar actual por varias razones:

- **Tamaño fijo**: 256 bits, no hay que decidir
- **Rendimiento**: operaciones ~10x más rápidas que RSA-4096
- **Seguridad**: resistente a ataques de canal lateral (timing attacks)
- **Determinístico**: misma firma para mismos datos (ECDSA no, lo cual causó bugs históricos)
- **Claves pequeñas**: la clave pública cabe en una línea corta

```bash
# Clave pública Ed25519 (~80 caracteres de datos):
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIGnFMhd+pyMKBJPO2MZ... user@host

# Clave pública RSA-4096 (~560 caracteres de datos):
ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAACAQC7Rb5VQ3k... (mucho más largo)
```

### ECDSA — cuidado con la entropía

ECDSA requiere números aleatorios de alta calidad para cada firma. Si la fuente de entropía es débil, la clave privada puede filtrarse. Esto ocurrió en la práctica (PlayStation 3 hack en 2010, problema de Android SecureRandom en 2013). Ed25519 no tiene este problema porque sus firmas son determinísticas.

### RSA — cuándo aún se usa

- Sistemas legacy que no soportan Ed25519 (OpenSSH < 6.5, muy antiguo)
- Entornos que requieren FIPS 140-2 compliance (Ed25519 no está en FIPS)
- Hardware tokens antiguos

Si usas RSA, el mínimo aceptable es **3072 bits** (NIST recomienda desde 2020). RSA-2048 se considera seguro hasta ~2030, pero no tiene margen.

---

## ssh-keygen

### Generar claves

```bash
# Ed25519 — método preferido
ssh-keygen -t ed25519 -C "admin@servidor-prod"
# -t: tipo de algoritmo
# -C: comentario (convención: usuario@máquina, pero puede ser cualquier texto)

# Salida interactiva:
# Generating public/private ed25519 key pair.
# Enter file in which to save the key (/home/user/.ssh/id_ed25519): ←── ruta
# Enter passphrase (empty for no passphrase): ←── protección de la clave
# Enter same passphrase again:
# Your identification has been saved in /home/user/.ssh/id_ed25519
# Your public key has been saved in /home/user/.ssh/id_ed25519.pub
# The key fingerprint is:
# SHA256:xKz7nGb5sIh4H... admin@servidor-prod
```

```bash
# Ed25519 con todas las opciones
ssh-keygen -t ed25519 -C "deploy-key" -f ~/.ssh/deploy_ed25519
# -f: archivo de salida (evita la pregunta interactiva)

# RSA (solo si Ed25519 no es una opción)
ssh-keygen -t rsa -b 4096 -C "legacy-system"
# -b: bits (mínimo 3072, preferible 4096)

# ECDSA
ssh-keygen -t ecdsa -b 521 -C "ecdsa-key"
# -b para ECDSA: 256, 384, o 521 (no 512)
```

### Passphrase

La passphrase cifra la clave privada en disco. Sin ella, cualquiera que obtenga el archivo tiene acceso:

```
    Sin passphrase:
    ┌──────────────────┐
    │ id_ed25519       │─── texto plano ──► acceso inmediato
    │ (clave privada)  │
    └──────────────────┘

    Con passphrase:
    ┌──────────────────┐
    │ id_ed25519       │─── cifrado AES ──► necesita passphrase
    │ (clave privada)  │                    para descifrar
    └──────────────────┘
```

Recomendaciones:

- **Claves interactivas** (tu uso diario): siempre con passphrase + ssh-agent
- **Claves de automatización** (scripts, CI/CD): sin passphrase es aceptable si se protege con permisos estrictos y se limita con `command=` en authorized_keys
- **Passphrase fuerte**: una frase larga es mejor que un password complejo. "correct horse battery staple" es mejor que "P@ssw0rd!"

### Cambiar passphrase de una clave existente

```bash
# Cambiar la passphrase (no regenera la clave, misma clave pública)
ssh-keygen -p -f ~/.ssh/id_ed25519
# Enter old passphrase:
# Enter new passphrase:
```

### Formato de la clave privada

OpenSSH 7.8+ usa un formato propio más seguro que el formato PEM tradicional:

```bash
# Formato moderno OpenSSH (default desde 7.8):
cat ~/.ssh/id_ed25519
# -----BEGIN OPENSSH PRIVATE KEY-----
# b3BlbnNzaC1rZXktdjEAAAAABG5vbm...
# -----END OPENSSH PRIVATE KEY-----

# Si necesitas formato PEM (para herramientas legacy):
ssh-keygen -t rsa -b 4096 -m PEM -f legacy_key

# Convertir una clave existente de OpenSSH a PEM:
ssh-keygen -p -m PEM -f clave_existente
# (pide la passphrase actual y la nueva)
```

### Fingerprint y verificación

```bash
# Ver el fingerprint de una clave pública
ssh-keygen -lf ~/.ssh/id_ed25519.pub
# 256 SHA256:xKz7nGb5sIh4H... admin@servidor-prod (ED25519)

# Fingerprint con hash MD5 (legacy, pero aún se ve en algunas interfaces)
ssh-keygen -lf ~/.ssh/id_ed25519.pub -E md5
# 256 MD5:a1:b2:c3:d4:... admin@servidor-prod (ED25519)

# Arte visual (randomart) — representación visual del fingerprint
ssh-keygen -lvf ~/.ssh/id_ed25519.pub
# +--[ED25519 256]--+
# |      .+B=.o.    |
# |     . +.o+..    |
# |      o.. o..    |
# |       .+o .     |
# |      .oSo.      |
# |    . =o+ .      |
# |     =.+ +       |
# |    . o =.+      |
# |     . +E+.      |
# +----[SHA256]-----+
```

El randomart está diseñado para que humanos puedan comparar visualmente fingerprints. Es más fácil notar diferencias en un patrón visual que en una cadena hexadecimal.

---

## Distribución de claves

### ssh-copy-id

El método estándar para copiar tu clave pública al servidor:

```bash
# Copiar la clave default (~/.ssh/id_ed25519.pub)
ssh-copy-id usuario@servidor
# /usr/bin/ssh-copy-id: INFO: Source of key(s) to be installed: "/home/user/.ssh/id_ed25519.pub"
# /usr/bin/ssh-copy-id: INFO: attempting to log in with the new key(s)
# usuario@servidor's password: ←── necesitas password esta vez

# Copiar una clave específica
ssh-copy-id -i ~/.ssh/deploy_ed25519.pub usuario@servidor

# Copiar a un puerto no estándar
ssh-copy-id -p 2222 usuario@servidor
```

Lo que `ssh-copy-id` hace internamente:

```bash
# 1. Lee la clave pública local
# 2. Conecta al servidor por SSH (con password)
# 3. Ejecuta remotamente:
mkdir -p ~/.ssh
chmod 700 ~/.ssh
cat >> ~/.ssh/authorized_keys
chmod 600 ~/.ssh/authorized_keys
# 4. Añade la clave pública al final de authorized_keys
```

### Copia manual

Cuando `ssh-copy-id` no está disponible:

```bash
# Opción 1: con cat y pipe
cat ~/.ssh/id_ed25519.pub | ssh usuario@servidor \
  "mkdir -p ~/.ssh && chmod 700 ~/.ssh && cat >> ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys"

# Opción 2: copiar con scp y agregar
scp ~/.ssh/id_ed25519.pub usuario@servidor:/tmp/
ssh usuario@servidor "cat /tmp/id_ed25519.pub >> ~/.ssh/authorized_keys && rm /tmp/id_ed25519.pub"
```

### Permisos críticos

SSH **rechaza** la autenticación por clave si los permisos son demasiado abiertos. Esto es una protección, no un bug:

```bash
# En el CLIENTE (máquina local):
chmod 700 ~/.ssh              # directorio
chmod 600 ~/.ssh/id_ed25519   # clave privada — CRÍTICO
chmod 644 ~/.ssh/id_ed25519.pub  # clave pública
chmod 644 ~/.ssh/known_hosts  # hosts conocidos
chmod 600 ~/.ssh/config       # configuración del cliente

# En el SERVIDOR:
chmod 700 ~/.ssh                    # directorio
chmod 600 ~/.ssh/authorized_keys    # claves autorizadas — CRÍTICO

# El directorio home NO puede tener permisos de escritura para grupo/otros
chmod go-w ~
```

Si los permisos están mal, SSH falla silenciosamente (desde la perspectiva del usuario). El servidor simplemente rechaza la clave y el cliente pide password. El error real está en los logs del servidor:

```bash
# En el servidor, ver por qué falló:
sudo journalctl -u sshd | grep -i "auth"
# Authentication refused: bad ownership or modes for directory /home/user/.ssh
# Authentication refused: bad ownership or modes for file /home/user/.ssh/authorized_keys
```

---

## authorized_keys

### Formato básico

```
tipo_clave datos_base64 comentario
```

```bash
# Ejemplo de una línea en ~/.ssh/authorized_keys:
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIGnFMhd+pyMKBJPO2MZr9kFgV3BUJGiOkEhHN4NXFA== admin@workstation

# Múltiples claves = múltiples líneas
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5... admin@laptop
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5... admin@workstation
ssh-rsa AAAAB3NzaC1yc2EAAAADAQAB... deploy@ci-server
```

### Opciones por clave

Cada línea en `authorized_keys` puede tener opciones al inicio que restringen qué puede hacer esa clave:

```bash
# Formato con opciones:
opcion1,opcion2="valor" tipo_clave datos_base64 comentario
```

Opciones disponibles:

```bash
# command= — forzar un comando específico
command="/usr/local/bin/backup.sh" ssh-ed25519 AAAA... backup@server
# Sin importar qué comando envíe el cliente, solo se ejecuta backup.sh
# La variable SSH_ORIGINAL_COMMAND contiene lo que el cliente pidió

# from= — restringir por IP de origen
from="192.168.1.0/24,10.0.0.5" ssh-ed25519 AAAA... admin@office
# Solo acepta conexiones desde estas IPs

# no-port-forwarding — prohibir túneles
no-port-forwarding ssh-ed25519 AAAA... limited@user

# no-X11-forwarding — prohibir GUI remota
no-X11-forwarding ssh-ed25519 AAAA...

# no-agent-forwarding — prohibir reenvío de agente
no-agent-forwarding ssh-ed25519 AAAA...

# no-pty — prohibir terminal interactiva
no-pty ssh-ed25519 AAAA... script@automation

# restrict — deshabilitar TODO (moderno, OpenSSH 7.2+)
restrict ssh-ed25519 AAAA...
# Equivale a: no-port-forwarding,no-X11-forwarding,no-agent-forwarding,no-pty,no-user-rc

# restrict + permitir selectivamente
restrict,pty ssh-ed25519 AAAA... limited@user
# Todo deshabilitado excepto terminal interactiva

# environment= — setear variables de entorno
environment="ROLE=backup",environment="ENV=prod" ssh-ed25519 AAAA...
# Requiere PermitUserEnvironment yes en sshd_config

# expiry-time= — clave con fecha de expiración (OpenSSH 9.2+)
expiry-time="20260401" ssh-ed25519 AAAA... temp@contractor
```

### Ejemplo completo: clave de backup automatizado

```bash
# En authorized_keys del servidor:
command="/usr/local/bin/validate-backup.sh",from="10.0.0.50",no-port-forwarding,no-X11-forwarding,no-agent-forwarding,no-pty ssh-ed25519 AAAA... backup@backup-server
```

El script `validate-backup.sh` puede verificar qué pidió el cliente:

```bash
#!/bin/bash
# /usr/local/bin/validate-backup.sh
case "$SSH_ORIGINAL_COMMAND" in
    rsync\ --server*)
        # Permitir rsync
        exec $SSH_ORIGINAL_COMMAND
        ;;
    *)
        echo "Command not allowed: $SSH_ORIGINAL_COMMAND"
        exit 1
        ;;
esac
```

### AuthorizedKeysFile alternativo

Por defecto, sshd busca en `~/.ssh/authorized_keys`. Para gestión centralizada:

```bash
# En sshd_config: usar un directorio centralizado
AuthorizedKeysFile /etc/ssh/authorized_keys/%u

# Estructura:
/etc/ssh/authorized_keys/
├── admin        ← claves de admin
├── deploy       ← claves de deploy
└── monitoring   ← claves de monitoring

# Permisos: root:root, 644 o 600
```

Ventaja de centralizar: los usuarios no pueden añadir sus propias claves. El administrador controla completamente quién tiene acceso.

---

## ssh-agent

### El problema que resuelve

Sin ssh-agent, cada vez que usas una clave con passphrase, debes escribirla:

```bash
ssh servidor1    # pide passphrase
ssh servidor2    # pide passphrase otra vez
git push         # pide passphrase otra vez
```

ssh-agent mantiene las claves descifradas en memoria durante tu sesión:

```
    ┌──────────────────────────────────────┐
    │            ssh-agent                 │
    │                                      │
    │  Clave 1 (descifrada) ──────┐        │
    │  Clave 2 (descifrada) ──────┤        │
    │  Clave 3 (descifrada) ──────┘        │
    │                          │           │
    │  Socket UNIX:            │           │
    │  $SSH_AUTH_SOCK     ◄────┘           │
    │                                      │
    └────────────────┬─────────────────────┘
                     │
         ┌───────────┼───────────┐
         │           │           │
       ssh        git push     scp
    (usa clave   (usa clave   (usa clave
     sin pedir    sin pedir    sin pedir
     passphrase)  passphrase)  passphrase)
```

### Iniciar el agente

```bash
# Método 1: evaluar la salida de ssh-agent (setea SSH_AUTH_SOCK y SSH_AGENT_PID)
eval $(ssh-agent -s)
# Agent pid 12345

# Verificar que está corriendo
echo $SSH_AUTH_SOCK
# /tmp/ssh-XXXX/agent.12344

# Método 2: en la mayoría de escritorios Linux, ssh-agent ya está corriendo
# (GNOME, KDE lo inician automáticamente)
# Verificar:
ssh-add -l
# Si devuelve "Could not open a connection to your authentication agent"
# → no está corriendo
# Si devuelve "The agent has no identities" → está corriendo, sin claves
```

### ssh-add — gestionar claves en el agente

```bash
# Añadir la clave default (~/.ssh/id_ed25519)
ssh-add
# Enter passphrase for /home/user/.ssh/id_ed25519: ←── una sola vez
# Identity added: /home/user/.ssh/id_ed25519 (admin@workstation)

# Añadir una clave específica
ssh-add ~/.ssh/deploy_ed25519

# Listar claves cargadas
ssh-add -l
# 256 SHA256:xKz7nGb... admin@workstation (ED25519)
# 256 SHA256:yHa8pMc... deploy-key (ED25519)

# Listar con claves públicas completas
ssh-add -L
# ssh-ed25519 AAAAC3NzaC1lZDI1NTE5... admin@workstation
# ssh-ed25519 AAAAC3NzaC1lZDI1NTE5... deploy-key

# Eliminar una clave del agente
ssh-add -d ~/.ssh/deploy_ed25519

# Eliminar todas las claves del agente
ssh-add -D
# All identities removed.

# Añadir con tiempo de expiración (seguridad)
ssh-add -t 3600 ~/.ssh/id_ed25519
# La clave se borra del agente tras 1 hora (3600 segundos)

# Establecer tiempo máximo de vida para el agente
ssh-agent -t 7200    # 2 horas máximo para cualquier clave
```

### Configuración automática en bash/zsh

Para que el agente se inicie con tu sesión de shell:

```bash
# En ~/.bashrc o ~/.zshrc:

# Iniciar ssh-agent si no está corriendo
if [ -z "$SSH_AUTH_SOCK" ]; then
    eval $(ssh-agent -s) > /dev/null
    # Añadir clave automáticamente (pedirá passphrase una vez)
    ssh-add ~/.ssh/id_ed25519 2>/dev/null
fi
```

### Integración con el keyring del sistema

En entornos de escritorio, GNOME Keyring o KDE Wallet pueden actuar como ssh-agent, desbloqueando claves automáticamente al iniciar sesión:

```bash
# GNOME: las claves SSH con passphrase se desbloquean
# automáticamente cuando inicias sesión en GNOME
# (usa gnome-keyring-daemon como ssh-agent)

# Verificar qué agente está activo:
echo $SSH_AUTH_SOCK
# /run/user/1000/keyring/ssh  ← GNOME Keyring
# /tmp/ssh-XXXX/agent.12345   ← ssh-agent estándar
```

### Agent forwarding

Permite usar tus claves locales en un servidor remoto sin copiar la clave privada:

```bash
# Conectar con agent forwarding
ssh -A usuario@bastion

# Ahora, desde bastion, puedes SSH a otros servidores
# usando las claves de tu máquina local
ssh usuario@servidor-interno
# Funciona sin tener claves en bastion
```

```
    Tu máquina           Bastion              Servidor interno
    ┌──────────┐        ┌──────────┐         ┌──────────┐
    │ssh-agent │        │          │         │          │
    │          │◄──────►│ socket   │◄───────►│ auth     │
    │ claves   │ SSH    │ forward  │  SSH    │ request  │
    │ privadas │ tunnel │ del agent│  tunnel │          │
    └──────────┘        └──────────┘         └──────────┘
```

> **Riesgo de seguridad**: si un atacante tiene acceso root en el bastion, puede usar tu socket del agente para autenticarse como tú en otros servidores **mientras tu sesión está activa**. Mitigaciones:
> - Usa `ssh-add -c` para requerir confirmación en cada uso
> - Prefiere `ProxyJump` sobre agent forwarding cuando sea posible
> - Solo habilita forwarding hacia hosts confiables

```bash
# Más seguro: ProxyJump en vez de agent forwarding
ssh -J bastion usuario@servidor-interno
# La conexión pasa por bastion, pero la autenticación
# al servidor interno se hace directamente desde tu máquina
# No hay socket de agente expuesto en bastion
```

---

## Gestión de claves del host

### Regenerar claves del host

Necesario tras clonar una VM o reinstalar el servidor:

```bash
# Eliminar claves existentes
sudo rm /etc/ssh/ssh_host_*

# Regenerar todas
sudo ssh-keygen -A
# ssh-keygen: generating new host keys: RSA ECDSA ED25519

# O regenerar solo una:
sudo ssh-keygen -t ed25519 -f /etc/ssh/ssh_host_ed25519_key -N ""
# -N "": sin passphrase (las claves de host nunca tienen passphrase)
```

### Restringir algoritmos del host

Puedes limitar qué claves de host ofrece el servidor:

```bash
# En sshd_config: solo ofrecer Ed25519 y ECDSA
HostKey /etc/ssh/ssh_host_ed25519_key
HostKey /etc/ssh/ssh_host_ecdsa_key
# No listar RSA = no se ofrece RSA

# Eliminar las claves que no se usan (opcional, limpieza)
sudo rm /etc/ssh/ssh_host_rsa_key /etc/ssh/ssh_host_rsa_key.pub
sudo rm /etc/ssh/ssh_host_dsa_key /etc/ssh/ssh_host_dsa_key.pub 2>/dev/null
```

### Publicar fingerprints en DNS (SSHFP)

Los registros SSHFP permiten verificar la identidad del host a través de DNS (requiere DNSSEC):

```bash
# Generar registros SSHFP para todas las claves del host
ssh-keygen -r servidor.ejemplo.com
# servidor.ejemplo.com IN SSHFP 1 1 a1b2c3d4...  (RSA SHA1)
# servidor.ejemplo.com IN SSHFP 1 2 e5f6a7b8...  (RSA SHA256)
# servidor.ejemplo.com IN SSHFP 4 1 c9d0e1f2...  (Ed25519 SHA1)
# servidor.ejemplo.com IN SSHFP 4 2 1a2b3c4d...  (Ed25519 SHA256)

# El cliente puede verificar automáticamente:
# En ssh_config:
VerifyHostKeyDNS yes
```

---

## known_hosts

### Funcionamiento

Cuando te conectas a un servidor por primera vez:

```bash
ssh usuario@servidor-nuevo
# The authenticity of host 'servidor-nuevo (192.168.1.50)' can't be established.
# ED25519 key fingerprint is SHA256:xKz7nGb5sIh4H...
# Are you sure you want to continue connecting (yes/no/[fingerprint])? yes
# Warning: Permanently added 'servidor-nuevo' (ED25519) to the list of known hosts.
```

El fingerprint se guarda en `~/.ssh/known_hosts`. En conexiones posteriores, SSH compara silenciosamente. Si la clave cambia:

```
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@    WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!     @
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
IT IS POSSIBLE THAT SOMEONE IS DOING SOMETHING NASTY!
Someone could be eavesdropping on you right now (man-in-the-middle attack)!
```

### Formato de known_hosts

```bash
# Formato: hostname[,IP] tipo_clave clave_pública
servidor.ejemplo.com,192.168.1.50 ssh-ed25519 AAAAC3NzaC1lZDI1NTE5...

# Formato hasheado (más seguro, no expone los hostnames):
|1|base64_salt|base64_hash ssh-ed25519 AAAAC3NzaC1lZDI1NTE5...
```

### Gestionar known_hosts

```bash
# Buscar una entrada
ssh-keygen -F servidor.ejemplo.com

# Eliminar una entrada (servidor reimagen, IP cambió)
ssh-keygen -R servidor.ejemplo.com
# Host servidor.ejemplo.com found: line 15
# /home/user/.ssh/known_hosts updated.

# Hashear un known_hosts existente (ocultar hostnames)
ssh-keygen -H
# Convierte las entradas en formato hasheado
# Irreversible — no puedes volver a ver los hostnames en texto claro

# Habilitar hashing automático en ssh_config del cliente:
HashKnownHosts yes   # default en Debian/Ubuntu
```

### StrictHostKeyChecking

```bash
# En ssh_config (cliente):

# ask — preguntar en primera conexión (default)
StrictHostKeyChecking ask

# yes — rechazar hosts desconocidos sin preguntar
StrictHostKeyChecking yes
# Debes añadir la clave manualmente antes de conectar

# accept-new — aceptar automáticamente hosts nuevos, rechazar cambios
StrictHostKeyChecking accept-new
# Buen balance: no pregunta la primera vez, pero alerta si cambia

# no — aceptar cualquier clave (INSEGURO, solo para labs)
StrictHostKeyChecking no
```

---

## Escenarios prácticos

### Escenario 1 — Múltiples claves para diferentes contextos

```bash
# Generar claves separadas por contexto
ssh-keygen -t ed25519 -C "personal" -f ~/.ssh/id_ed25519_personal
ssh-keygen -t ed25519 -C "trabajo"  -f ~/.ssh/id_ed25519_work
ssh-keygen -t ed25519 -C "github"   -f ~/.ssh/id_ed25519_github

# En ~/.ssh/config: asignar claves por host
Host github.com
    IdentityFile ~/.ssh/id_ed25519_github
    IdentitiesOnly yes     # solo ofrecer esta clave, no probar todas

Host *.empresa.com
    IdentityFile ~/.ssh/id_ed25519_work
    IdentitiesOnly yes

Host *.personal.net
    IdentityFile ~/.ssh/id_ed25519_personal
    IdentitiesOnly yes
```

`IdentitiesOnly yes` es importante: sin esta opción, SSH intenta **todas** las claves del agente antes de la configurada, lo cual puede causar bloqueos por `MaxAuthTries` si tienes muchas claves cargadas.

### Escenario 2 — Despliegue automatizado con clave restringida

```bash
# En la máquina CI/CD: generar clave sin passphrase
ssh-keygen -t ed25519 -C "ci-deploy" -f /etc/ci/deploy_key -N ""

# Proteger con permisos del sistema de archivos
sudo chown ci-user:ci-user /etc/ci/deploy_key
sudo chmod 600 /etc/ci/deploy_key

# En el servidor de producción, authorized_keys:
command="/usr/local/bin/deploy-only.sh",from="10.0.0.100",restrict,pty ssh-ed25519 AAAA... ci-deploy

# El script deploy-only.sh:
#!/bin/bash
case "$SSH_ORIGINAL_COMMAND" in
    "deploy webapp")
        cd /opt/webapp && git pull && systemctl reload webapp
        ;;
    "deploy api")
        cd /opt/api && git pull && systemctl reload api
        ;;
    "status")
        systemctl status webapp api
        ;;
    *)
        echo "Denied: $SSH_ORIGINAL_COMMAND" | logger -t ssh-deploy
        exit 1
        ;;
esac
```

### Escenario 3 — Rotación de claves

```bash
# 1. Generar nueva clave
ssh-keygen -t ed25519 -C "admin-2026Q2" -f ~/.ssh/id_ed25519_new

# 2. Distribuir la nueva clave a todos los servidores
for host in server1 server2 server3; do
    ssh-copy-id -i ~/.ssh/id_ed25519_new.pub admin@$host
done

# 3. Verificar que la nueva clave funciona
for host in server1 server2 server3; do
    ssh -i ~/.ssh/id_ed25519_new admin@$host "hostname"
done

# 4. Eliminar la clave antigua de los servidores
# (buscar y eliminar la línea con el comentario de la clave vieja)
for host in server1 server2 server3; do
    ssh -i ~/.ssh/id_ed25519_new admin@$host \
      "sed -i '/admin-2026Q1/d' ~/.ssh/authorized_keys"
done

# 5. Renombrar localmente
mv ~/.ssh/id_ed25519_new ~/.ssh/id_ed25519
mv ~/.ssh/id_ed25519_new.pub ~/.ssh/id_ed25519.pub
```

### Escenario 4 — Certificados SSH (alternativa a authorized_keys)

Para entornos grandes, gestionar `authorized_keys` en cada servidor no escala. Los certificados SSH resuelven esto:

```bash
# 1. Crear una CA (Certificate Authority) SSH
ssh-keygen -t ed25519 -f /etc/ssh/ca_key -C "SSH CA"

# 2. Firmar la clave pública de un usuario
ssh-keygen -s /etc/ssh/ca_key \
    -I "admin-certificate" \
    -n admin \
    -V +52w \
    ~/.ssh/id_ed25519.pub
# -s: clave de firma (CA)
# -I: identificador del certificado
# -n: principals (usuarios autorizados en el servidor)
# -V: validez (+52w = 52 semanas)
# Genera: ~/.ssh/id_ed25519-cert.pub

# 3. En el servidor, confiar en la CA (una vez):
echo "TrustedUserCAKeys /etc/ssh/ca_user_key.pub" >> /etc/ssh/sshd_config
# Copiar la clave PÚBLICA de la CA a /etc/ssh/ca_user_key.pub

# Ahora cualquier clave firmada por la CA es aceptada automáticamente
# Sin necesidad de tocar authorized_keys en cada servidor
```

```
    Sin certificados:                Con certificados:
    ┌────────┐  authorized_keys     ┌────────┐  CA trust
    │Server 1│ ← clave A            │Server 1│ ← confía en CA
    │Server 2│ ← clave A            │Server 2│ ← confía en CA
    │Server 3│ ← clave A            │Server 3│ ← confía en CA
    │  ...   │ ← clave A            │  ...   │ ← confía en CA
    │Server N│ ← clave A            │Server N│ ← confía en CA
    └────────┘                       └────────┘
    Escala: O(N) por usuario         Escala: O(1) por usuario
```

---

## Errores comunes

### 1. Permisos demasiado abiertos en ~/.ssh

```bash
# El error más frecuente: copiar claves sin ajustar permisos
scp servidor:/backup/id_ed25519 ~/.ssh/
# Ahora ~/.ssh/id_ed25519 puede tener permisos 644

ssh servidor
# "WARNING: UNPROTECTED PRIVATE KEY FILE! ... Permissions 0644 ... are too open"
# Clave ignorada, pide password

# Solución:
chmod 600 ~/.ssh/id_ed25519
```

### 2. Ofrecer demasiadas claves al servidor

```bash
# Si tienes 10 claves en ssh-agent y MaxAuthTries es 3:
ssh-add -l | wc -l
# 10

ssh servidor
# El servidor recibe 3 claves incorrectas y desconecta
# "Received disconnect from servidor: Too many authentication failures"

# Solución: usar IdentitiesOnly yes en ssh_config
Host servidor
    IdentityFile ~/.ssh/id_ed25519_servidor
    IdentitiesOnly yes
    # Solo ofrece esta clave, no todas las del agente
```

### 3. Copiar la clave PRIVADA en lugar de la pública

```bash
# MAL — copias id_ed25519 (PRIVADA) al servidor
ssh-copy-id -i ~/.ssh/id_ed25519 usuario@servidor
# Esto funciona porque ssh-copy-id busca el .pub automáticamente
# PERO si manualmente haces:
scp ~/.ssh/id_ed25519 usuario@servidor:~/.ssh/authorized_keys
# ¡Has copiado tu clave privada al servidor!

# BIEN — siempre copiar el archivo .pub
scp ~/.ssh/id_ed25519.pub usuario@servidor:/tmp/
```

### 4. Agent forwarding innecesario

```bash
# MAL — habilitar -A por defecto "por si acaso"
Host *
    ForwardAgent yes
# Cada servidor que visitas puede usar tu agente

# BIEN — solo donde lo necesitas
Host bastion
    ForwardAgent yes

Host *
    ForwardAgent no

# MEJOR — usar ProxyJump
Host interno
    ProxyJump bastion
    # No necesita agent forwarding
```

### 5. No verificar el fingerprint en la primera conexión

```bash
# Primera conexión, todos hacen "yes" sin verificar:
# ED25519 key fingerprint is SHA256:xKz7nGb5sIh4H...
# Are you sure you want to continue connecting? yes
# ← Aceptaste sin verificar. Si hay un MITM, le diste acceso.

# BIEN — verificar el fingerprint
# En el servidor (o pídelo al admin):
ssh-keygen -lf /etc/ssh/ssh_host_ed25519_key.pub
# 256 SHA256:xKz7nGb5sIh4H... root@servidor (ED25519)

# Comparar con lo que muestra el cliente antes de aceptar
```

---

## Cheatsheet

```bash
# === GENERAR CLAVES ===
ssh-keygen -t ed25519 -C "comentario"              # Ed25519 (preferido)
ssh-keygen -t ed25519 -f ~/.ssh/nombre -N ""        # sin passphrase
ssh-keygen -t rsa -b 4096 -C "legacy"              # RSA (si es necesario)
ssh-keygen -p -f ~/.ssh/id_ed25519                  # cambiar passphrase

# === FINGERPRINT ===
ssh-keygen -lf ~/.ssh/id_ed25519.pub                # SHA256
ssh-keygen -lf ~/.ssh/id_ed25519.pub -E md5         # MD5
ssh-keygen -lvf ~/.ssh/id_ed25519.pub               # randomart visual

# === DISTRIBUIR ===
ssh-copy-id usuario@servidor                        # copiar clave default
ssh-copy-id -i ~/.ssh/clave.pub usuario@servidor    # clave específica
ssh-copy-id -p 2222 usuario@servidor                # puerto no estándar

# === PERMISOS ===
chmod 700 ~/.ssh                                    # directorio
chmod 600 ~/.ssh/id_ed25519                         # clave privada
chmod 644 ~/.ssh/id_ed25519.pub                     # clave pública
chmod 600 ~/.ssh/authorized_keys                    # claves autorizadas
chmod 600 ~/.ssh/config                             # config cliente

# === SSH-AGENT ===
eval $(ssh-agent -s)                                # iniciar agente
ssh-add                                             # añadir clave default
ssh-add ~/.ssh/clave                                # añadir clave específica
ssh-add -t 3600                                     # con expiración (1h)
ssh-add -l                                          # listar claves
ssh-add -L                                          # listar claves completas
ssh-add -d ~/.ssh/clave                             # eliminar una clave
ssh-add -D                                          # eliminar todas

# === KNOWN_HOSTS ===
ssh-keygen -F hostname                              # buscar entrada
ssh-keygen -R hostname                              # eliminar entrada
ssh-keygen -H                                       # hashear todo el archivo

# === AUTHORIZED_KEYS OPTIONS ===
command="cmd"                                       # forzar comando
from="ip/cidr"                                      # restringir origen
restrict                                            # deshabilitar todo
no-port-forwarding                                  # sin túneles
no-pty                                              # sin terminal

# === CLAVES DEL HOST ===
sudo ssh-keygen -A                                  # regenerar todas
ssh-keygen -r hostname                              # generar registros SSHFP

# === CERTIFICADOS SSH ===
ssh-keygen -s ca_key -I id -n user -V +52w key.pub  # firmar clave
ssh-keygen -Lf key-cert.pub                          # ver certificado
```

---

## Ejercicios

### Ejercicio 1 — Configurar acceso por clave

1. Genera un par de claves Ed25519 con el comentario `lab-ejercicio1`
2. Copia la clave pública a un servidor (o contenedor Docker con SSH)
3. Verifica que puedes conectar sin password
4. Deshabilita `PasswordAuthentication` en el servidor
5. Confirma que no puedes conectar sin clave (intenta con `-o IdentityFile=/dev/null`)
6. Verifica los permisos de todos los archivos SSH en cliente y servidor

<details>
<summary>Solución</summary>

```bash
# 1. Generar clave
ssh-keygen -t ed25519 -C "lab-ejercicio1" -f ~/.ssh/id_lab1

# 2. Copiar al servidor
ssh-copy-id -i ~/.ssh/id_lab1.pub usuario@servidor

# 3. Conectar con clave
ssh -i ~/.ssh/id_lab1 usuario@servidor
# No pide password ✓

# 4. Deshabilitar passwords en el servidor
# En el servidor:
echo "PasswordAuthentication no" | sudo tee /etc/ssh/sshd_config.d/10-nopass.conf
sudo sshd -t && sudo systemctl reload sshd

# 5. Verificar que no funciona sin clave
ssh -o IdentityFile=/dev/null -o PreferredAuthentications=password usuario@servidor
# Permission denied (publickey).  ← correcto

# 6. Verificar permisos
# Cliente:
ls -la ~/.ssh/id_lab1 ~/.ssh/id_lab1.pub
# -rw------- id_lab1       (600)
# -rw-r--r-- id_lab1.pub   (644)

# Servidor:
ssh -i ~/.ssh/id_lab1 usuario@servidor "ls -la ~/.ssh/ && stat -c '%a %n' ~/.ssh ~/.ssh/authorized_keys"
# 700 .ssh
# 600 .ssh/authorized_keys
```

</details>

---

### Ejercicio 2 — Clave restringida para automatización

Crea una configuración donde una clave de automatización:

1. Solo pueda ejecutar el comando `df -h` (muestra uso de disco)
2. Solo sea aceptada desde la IP 127.0.0.1 (para probar localmente)
3. No pueda hacer port forwarding, X11, agent forwarding ni obtener terminal
4. Prueba que al conectar con esa clave, ejecute `df -h` sin importar qué comando envíes
5. Prueba que desde otra IP (si es posible) sea rechazada

<details>
<summary>Solución</summary>

```bash
# 1. Generar clave sin passphrase (automatización)
ssh-keygen -t ed25519 -C "disk-check" -f ~/.ssh/id_diskcheck -N ""

# 2. Añadir a authorized_keys con restricciones
cat >> ~/.ssh/authorized_keys << 'EOF'
command="df -h",from="127.0.0.1",no-port-forwarding,no-X11-forwarding,no-agent-forwarding,no-pty ssh-ed25519 CONTENIDO_DE_id_diskcheck.pub
EOF
# Reemplaza CONTENIDO con: cat ~/.ssh/id_diskcheck.pub | cut -d' ' -f1,2

# Método más limpio:
echo "command=\"df -h\",from=\"127.0.0.1\",no-port-forwarding,no-X11-forwarding,no-agent-forwarding,no-pty $(cat ~/.ssh/id_diskcheck.pub)" >> ~/.ssh/authorized_keys

# 3. Probar: ejecuta df -h sin importar el comando
ssh -i ~/.ssh/id_diskcheck localhost "whoami"
# Filesystem      Size  Used Avail Use% Mounted on
# /dev/sda1        50G   15G   33G  32% /
# ← ejecutó df -h, no whoami

ssh -i ~/.ssh/id_diskcheck localhost
# También ejecuta df -h (no da shell)

# 4. Variable SSH_ORIGINAL_COMMAND
# Si cambias command= por un script, puedes ver qué pidió el cliente:
# echo "SSH_ORIGINAL_COMMAND=$SSH_ORIGINAL_COMMAND" >> /var/log/ssh-cmds.log
```

</details>

---

### Ejercicio 3 — Diagnóstico de fallo de autenticación

Simula y diagnostica los siguientes fallos:

1. Cambia los permisos de `~/.ssh/authorized_keys` a 644 e intenta conectar. ¿Qué ocurre? ¿Dónde aparece el error?
2. Añade una segunda clave privada al agente y configura `MaxAuthTries 2` en el servidor. Intenta conectar con la segunda clave. ¿Qué ocurre?
3. Elimina la entrada del servidor de `~/.ssh/known_hosts`, cambia la clave del host del servidor y reconéctate. ¿Qué advertencia ves?

<details>
<summary>Solución</summary>

```bash
# 1. Permisos incorrectos
chmod 644 ~/.ssh/authorized_keys
ssh -i ~/.ssh/id_ed25519 usuario@servidor
# Pide password (la clave es rechazada silenciosamente)

# Diagnóstico en el servidor:
sudo journalctl -u sshd --since "1 min ago" | grep -i auth
# "Authentication refused: bad ownership or modes for file /home/usuario/.ssh/authorized_keys"

# Corregir:
chmod 600 ~/.ssh/authorized_keys

# 2. Too many auth failures
ssh-add ~/.ssh/id_clave1
ssh-add ~/.ssh/id_clave2
# MaxAuthTries 2 en el servidor

ssh usuario@servidor
# SSH prueba clave1 (falla), luego clave2 → ya son 2 intentos
# "Received disconnect: Too many authentication failures"

# Solución:
ssh -o IdentitiesOnly=yes -i ~/.ssh/id_clave2 usuario@servidor
# Solo ofrece clave2, un intento

# 3. Clave de host cambiada
ssh-keygen -R servidor
# En el servidor: sudo rm /etc/ssh/ssh_host_* && sudo ssh-keygen -A
ssh usuario@servidor
# "The authenticity of host 'servidor' can't be established."
# Muestra el NUEVO fingerprint, pide confirmación
# (si no hubieras borrado known_hosts, mostraría WARNING: CHANGED!)
```

</details>

---

### Pregunta de reflexión

> Tienes 50 servidores y 20 desarrolladores. Cada vez que un desarrollador se une o se va, necesitas actualizar `authorized_keys` en los 50 servidores. Un desarrollador pierde su laptop un viernes por la noche. ¿Cómo revocarías su acceso en los 50 servidores rápidamente? ¿Qué solución arquitectónica evitaría este problema desde el principio?

> **Razonamiento esperado**: con `authorized_keys` distribuido, debes conectarte a los 50 servidores y eliminar la clave — factible con un script, pero lento y propenso a errores si algún servidor no es accesible. La solución arquitectónica son los **certificados SSH**: configuras cada servidor para confiar en una CA (`TrustedUserCAKeys`), y firmas las claves de los desarrolladores con esa CA. Para revocar, añades el certificado a un `RevokedKeys` file en cada servidor (una sola línea) o, mejor aún, firmas certificados con validez corta (ej. 8 horas) — si la clave se pierde, el certificado expira solo. Herramientas como `step-ca` o `Vault` de HashiCorp automatizan la emisión de certificados SSH de corta duración.

---

> **Siguiente tema**: T03 — SSH tunneling (local, remote, dynamic port forwarding)
