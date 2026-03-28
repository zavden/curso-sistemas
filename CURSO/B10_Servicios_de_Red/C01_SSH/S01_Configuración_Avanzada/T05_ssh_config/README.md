# ~/.ssh/config — Configuración del cliente SSH

## Índice

1. [Por qué usar ssh_config](#por-qué-usar-ssh_config)
2. [Archivos de configuración del cliente](#archivos-de-configuración-del-cliente)
3. [Sintaxis y orden de evaluación](#sintaxis-y-orden-de-evaluación)
4. [Host y Match blocks](#host-y-match-blocks)
5. [Directivas esenciales](#directivas-esenciales)
6. [ProxyJump y acceso multi-hop](#proxyjump-y-acceso-multi-hop)
7. [Multiplexación de conexiones](#multiplexación-de-conexiones)
8. [Includes y organización modular](#includes-y-organización-modular)
9. [Escenarios prácticos](#escenarios-prácticos)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## Por qué usar ssh_config

Sin `~/.ssh/config`, cada conexión requiere recordar y teclear todos los parámetros:

```bash
# Sin config — cada vez:
ssh -i ~/.ssh/id_ed25519_work -p 2222 -J bastion.empresa.com admin@db-prod-01.internal.empresa.com

# Con config — una vez definido:
ssh db-prod
```

El archivo `~/.ssh/config` transforma comandos SSH largos y propensos a errores en alias cortos y consistentes. Es la diferencia entre un usuario que se pelea con SSH y uno que lo domina.

---

## Archivos de configuración del cliente

SSH lee la configuración del cliente en este orden (primera coincidencia gana):

```
    1. Opciones de línea de comandos     ssh -o "Option=value"
    2. ~/.ssh/config                      configuración del usuario
    3. /etc/ssh/ssh_config                configuración global del sistema
    4. Valores por defecto compilados     built-in defaults
```

```bash
# Configuración del usuario (tú controlas esto)
~/.ssh/config
# Permisos: 600 (o 644 — SSH no es estricto con este archivo)

# Configuración global (el admin del sistema controla esto)
/etc/ssh/ssh_config
# Incluye defaults para todos los usuarios

# Drop-in global (distros modernas)
/etc/ssh/ssh_config.d/*.conf
```

> **Importante**: `ssh_config` (cliente) y `sshd_config` (servidor) son archivos diferentes con directivas diferentes. No confundirlos.

---

## Sintaxis y orden de evaluación

### Formato básico

```bash
# ~/.ssh/config

# Bloque Host: aplica a conexiones que coincidan con el patrón
Host servidor-prod
    HostName 192.168.1.50
    User admin
    Port 22
    IdentityFile ~/.ssh/id_ed25519_work

# Otro bloque
Host dev
    HostName dev.ejemplo.com
    User developer
```

### Reglas de evaluación

```bash
# 1. SSH lee el archivo de ARRIBA a ABAJO
# 2. Para cada directiva, usa la PRIMERA coincidencia
# 3. Un Host puede coincidir con múltiples bloques
# 4. Las directivas se ACUMULAN de todos los bloques que coinciden
# 5. Pero si una directiva ya se encontró, NO se sobrescribe

# Ejemplo:
Host prod-*
    User admin
    IdentityFile ~/.ssh/id_prod

Host prod-db
    HostName db.internal
    Port 5432          # ← este Port se usa

Host prod-*
    Port 22            # ← este Port se IGNORA (ya se asignó Port arriba)
```

Esto significa que el bloque `Host *` (que coincide con todo) debe ir al **final**, porque actúa como "default" — solo aplica directivas que no se hayan definido antes:

```bash
# BIEN — Host * al final como defaults
Host prod
    HostName prod.ejemplo.com
    User admin

Host *
    ServerAliveInterval 60
    ServerAliveCountMax 3
    IdentitiesOnly yes

# MAL — Host * al principio bloquea todo
Host *
    User defaultuser      # ← TODAS las conexiones usan este User
                          #    porque se evalúa primero

Host prod
    User admin            # ← IGNORADO, User ya fue asignado por Host *
```

---

## Host y Match blocks

### Patrones en Host

```bash
# Nombre exacto
Host servidor1
    HostName 192.168.1.10

# Wildcard
Host prod-*
    User admin

# Múltiples patrones (OR)
Host servidor1 servidor2 servidor3
    User admin

# Negación (!)
Host * !bastion
    ProxyJump bastion
    # Todos pasan por bastion EXCEPTO bastion mismo

# Patrón con ? (un solo carácter)
Host web?
    User www-data
    # Coincide con web1, web2, webA, etc.
```

### Match blocks (OpenSSH 6.5+)

`Match` permite condiciones más complejas que `Host`:

```bash
# Match por hostname canónico
Match host "*.internal.empresa.com"
    ProxyJump bastion
    User admin

# Match por usuario local
Match localuser root
    IdentityFile /root/.ssh/id_automation

# Match ejecutando un comando externo
Match exec "test -f /etc/vpn-active"
    # Solo aplica si la VPN está activa
    ProxyJump none

# Match combinado
Match host "*.prod.*" localuser admin
    IdentityFile ~/.ssh/id_prod
```

### Match exec — lógica condicional

`Match exec` ejecuta un comando y aplica el bloque si el exit code es 0:

```bash
# Usar ProxyJump solo si NO estás en la red de oficina
Match host "*.internal" exec "! ip route | grep -q 10.0.0.0/8"
    ProxyJump bastion.empresa.com
    # Si no tienes ruta a 10.0.0.0/8 (no estás en la oficina),
    # usa bastion. Si estás en la oficina, conexión directa.

# Usar clave específica si un archivo existe
Match exec "test -f ~/.ssh/id_yubikey"
    IdentityFile ~/.ssh/id_yubikey
```

---

## Directivas esenciales

### Conexión básica

```bash
Host ejemplo
    # Nombre real del host (IP o DNS)
    HostName servidor.ejemplo.com

    # Usuario remoto
    User admin

    # Puerto
    Port 2222

    # Clave privada específica
    IdentityFile ~/.ssh/id_ed25519_ejemplo

    # Solo ofrecer la clave especificada (no todas del agente)
    IdentitiesOnly yes

    # Familia de IP
    AddressFamily inet    # solo IPv4 (inet6 para solo IPv6)
```

### Keepalive y timeouts

```bash
Host *
    # Enviar keepalive SSH cada 60 segundos
    ServerAliveInterval 60

    # Desconectar tras 3 keepalives sin respuesta
    ServerAliveCountMax 3

    # Timeout de conexión TCP (segundos)
    ConnectTimeout 10

    # Keepalive TCP (nivel kernel)
    TCPKeepAlive yes
```

### Seguridad

```bash
Host *
    # Verificación de clave del host
    StrictHostKeyChecking accept-new
    # ask       — preguntar en primera conexión (default)
    # yes       — rechazar hosts desconocidos
    # accept-new — aceptar nuevos, rechazar cambios (buen balance)
    # no        — aceptar todo (inseguro)

    # Hashear known_hosts (ocultar hostnames)
    HashKnownHosts yes

    # Solo ofrecer la clave especificada en IdentityFile
    IdentitiesOnly yes

    # Algoritmos preferidos (cliente)
    HostKeyAlgorithms ssh-ed25519,ecdsa-sha2-nistp256
    KexAlgorithms sntrup761x25519-sha512@openssh.com,curve25519-sha256
    Ciphers chacha20-poly1305@openssh.com,aes256-gcm@openssh.com

    # Verificar fingerprint DNS (si hay SSHFP records + DNSSEC)
    VerifyHostKeyDNS yes
```

### Forwarding

```bash
Host bastion
    # Agent forwarding (reenviar ssh-agent al servidor)
    ForwardAgent yes
    # ¡Solo en hosts confiables!

    # X11 forwarding (GUI remota)
    ForwardX11 yes
    ForwardX11Trusted yes    # -Y (trusted, menos restricciones)

Host tunnel-db
    # Local forward (equivalente a -L)
    LocalForward 5432 db.internal:5432

    # Remote forward (equivalente a -R)
    RemoteForward 8080 localhost:3000

    # Dynamic forward / SOCKS proxy (equivalente a -D)
    DynamicForward 1080
```

### Misc

```bash
Host *
    # Compresión (útil en conexiones lentas)
    Compression yes

    # Enviar variables de entorno (si el servidor las acepta)
    SendEnv LANG LC_*

    # Nivel de logging del cliente
    LogLevel ERROR    # reducir ruido (default: INFO)

    # No asignar terminal (útil para túneles)
    # RequestTTY no    # equivalente a -T
```

---

## ProxyJump y acceso multi-hop

### Bastion / jump host

El patrón más común: acceder a servidores internos a través de un bastion:

```
    Tu máquina ──SSH──► Bastion ──SSH──► Servidor interno
                        (público)        (red privada)
```

```bash
# ~/.ssh/config

Host bastion
    HostName bastion.empresa.com
    User admin
    IdentityFile ~/.ssh/id_ed25519_work
    # No necesita ProxyJump (acceso directo)

Host *.internal
    User admin
    IdentityFile ~/.ssh/id_ed25519_work
    ProxyJump bastion
    # Cualquier host que termine en .internal pasa por bastion

# Uso:
# ssh web1.internal     ← pasa por bastion automáticamente
# ssh db.internal       ← pasa por bastion automáticamente
# ssh bastion           ← conexión directa
```

### Cadena de bastions

```bash
Host bastion-ext
    HostName bastion.empresa.com
    User admin

Host bastion-int
    HostName 10.0.0.1
    User admin
    ProxyJump bastion-ext

Host prod-db
    HostName 10.0.100.50
    User dba
    ProxyJump bastion-int
    # Cadena: tu máquina → bastion-ext → bastion-int → prod-db

# Equivalente en línea de comandos:
# ssh -J bastion-ext,bastion-int dba@10.0.100.50
```

### ProxyJump condicional

```bash
# Si estás en la oficina (VPN activa), conexión directa
# Si estás fuera, usar bastion

Host *.internal
    User admin

# Primero: si la VPN está activa, no usar proxy
Match host "*.internal" exec "ip route | grep -q 10.0.0.0/8"
    ProxyJump none

# Segundo: si no coincidió el Match anterior, usar bastion
Host *.internal
    ProxyJump bastion
```

> **Orden importante**: el `Match exec` debe ir **antes** del `Host *.internal` con `ProxyJump bastion`, porque la primera coincidencia de `ProxyJump` gana.

### ProxyJump vs ProxyCommand

```bash
# ProxyJump (moderno, limpio)
Host interno
    ProxyJump bastion

# ProxyCommand (legacy, pero más flexible)
Host interno
    ProxyCommand ssh -W %h:%p bastion

# ProxyCommand con lógica personalizada
Host interno
    ProxyCommand bash -c 'if ping -c1 -W1 10.0.0.1 &>/dev/null; then nc %h %p; else ssh -W %h:%p bastion; fi'
```

Tokens disponibles en ProxyCommand:

| Token | Significado |
|---|---|
| `%h` | Hostname del destino |
| `%p` | Puerto del destino |
| `%r` | Usuario remoto |
| `%n` | Hostname original (antes de canonicalización) |
| `%C` | Hash de la conexión (para control sockets) |

---

## Multiplexación de conexiones

### Concepto

La multiplexación reutiliza una conexión SSH existente para nuevas sesiones, evitando el overhead del handshake:

```
    Sin multiplexación:              Con multiplexación:
    ssh srv  → handshake TCP+SSH     ssh srv  → handshake TCP+SSH
    ssh srv  → handshake TCP+SSH     ssh srv  → reutiliza conexión ✓
    scp srv  → handshake TCP+SSH     scp srv  → reutiliza conexión ✓
    git pull → handshake TCP+SSH     git pull → reutiliza conexión ✓

    4 handshakes (~2-4s cada uno)    1 handshake + 3 instantáneas
```

### Configuración

```bash
Host *
    # Habilitar multiplexación
    ControlMaster auto
    # auto: crear master si no existe, reutilizar si existe
    # yes:  siempre intentar ser master
    # no:   deshabilitado

    # Socket de control (debe ser único por conexión)
    ControlPath ~/.ssh/sockets/%r@%h-%p
    # %r: usuario remoto
    # %h: hostname
    # %p: puerto
    # Crear el directorio: mkdir -p ~/.ssh/sockets

    # Mantener la conexión viva N segundos después de cerrar la última sesión
    ControlPersist 600
    # 600 = 10 minutos
    # 0   = cerrar cuando la última sesión se cierre
    # yes = mantener indefinidamente
```

### Crear el directorio de sockets

```bash
mkdir -p ~/.ssh/sockets
chmod 700 ~/.ssh/sockets
```

### Operaciones con el master

```bash
# Primera conexión (crea el master automáticamente)
ssh servidor
# Abre sesión + crea socket de control

# Segunda conexión (reutiliza el master — instantánea)
ssh servidor
# Sin handshake, sin pedir autenticación

# Verificar estado del master
ssh -O check servidor
# Master running (pid=12345)

# Cerrar el master (y todas las sesiones que dependen de él)
ssh -O exit servidor
# Exit request sent.

# Solicitar que el master se cierre cuando no tenga sesiones
ssh -O stop servidor

# Listar sockets activos
ls -la ~/.ssh/sockets/
# admin@servidor-22
# deploy@bastion-22
```

### Beneficios prácticos

```bash
# Git sobre SSH: múltiples operaciones sin re-autenticar
git fetch origin        # primera: handshake completo
git push origin main    # segunda: instantánea (reutiliza)
git pull origin dev     # tercera: instantánea

# Ansible: ssh a muchos hosts es mucho más rápido
# ansible.cfg:
# [ssh_connection]
# ssh_args = -o ControlMaster=auto -o ControlPersist=600 -o ControlPath=~/.ssh/sockets/%r@%h-%p
# pipelining = True

# scp/sftp: sin re-autenticación
scp archivo.txt servidor:~/     # reutiliza la conexión SSH existente
```

### Limitaciones

```bash
# No funciona con ProxyJump si el path del socket tiene caracteres especiales
# Solución: usar %C (hash) para el path
ControlPath ~/.ssh/sockets/%C

# El master se cae si la conexión de red se interrumpe
# Solución: combinar con ServerAliveInterval
Host *
    ControlMaster auto
    ControlPath ~/.ssh/sockets/%C
    ControlPersist 600
    ServerAliveInterval 60
    ServerAliveCountMax 3
```

---

## Includes y organización modular

### Include directive (OpenSSH 7.3+)

```bash
# ~/.ssh/config

# Incluir archivos adicionales
Include config.d/*
Include config.work
Include config.personal

# El resto de tu configuración...
Host *
    ServerAliveInterval 60
```

> **Importante**: `Include` debe ir **antes** de los bloques `Host`/`Match` porque la primera coincidencia de cada directiva gana. Si pones `Include` al final, los archivos incluidos tendrán menor prioridad.

### Estructura modular recomendada

```
~/.ssh/
├── config              ← archivo principal (includes + defaults)
├── config.d/
│   ├── 00-defaults     ← Host *, multiplexación, keepalive
│   ├── 10-trabajo      ← hosts de la empresa
│   ├── 20-personal     ← servidores personales
│   ├── 30-cloud        ← AWS, GCP, Azure
│   └── 40-github       ← GitHub, GitLab
├── sockets/            ← sockets de multiplexación
├── id_ed25519          ← clave personal
├── id_ed25519_work     ← clave de trabajo
├── id_ed25519_github   ← clave de GitHub
├── known_hosts         ← hosts conocidos
└── authorized_keys     ← (para tu propio sshd)
```

```bash
# ~/.ssh/config (archivo principal)
Include config.d/*

# Si no hay includes, este es el fallback
Host *
    ServerAliveInterval 60
    ServerAliveCountMax 3
```

```bash
# ~/.ssh/config.d/00-defaults
Host *
    ControlMaster auto
    ControlPath ~/.ssh/sockets/%C
    ControlPersist 600
    ServerAliveInterval 60
    ServerAliveCountMax 3
    IdentitiesOnly yes
    StrictHostKeyChecking accept-new
    HashKnownHosts yes
    Compression yes
    AddKeysToAgent yes
```

```bash
# ~/.ssh/config.d/10-trabajo
Host bastion
    HostName bastion.empresa.com
    User admin
    IdentityFile ~/.ssh/id_ed25519_work

Host *.internal
    User admin
    IdentityFile ~/.ssh/id_ed25519_work
    ProxyJump bastion

Host prod-db
    HostName db.prod.internal
    User dba
    ProxyJump bastion
    LocalForward 5432 localhost:5432
```

```bash
# ~/.ssh/config.d/40-github
Host github.com
    HostName github.com
    User git
    IdentityFile ~/.ssh/id_ed25519_github
    # No multiplexar (GitHub cierra conexiones idle rápido)
    ControlMaster no

Host gitlab.com
    HostName gitlab.com
    User git
    IdentityFile ~/.ssh/id_ed25519_work
    ControlMaster no
```

### AddKeysToAgent

```bash
Host *
    AddKeysToAgent yes
    # Cuando usas una clave con passphrase, la añade al ssh-agent
    # automáticamente después de desbloquearla.
    # No necesitas hacer ssh-add manualmente.

    # Con tiempo de expiración:
    AddKeysToAgent 1h
    # La clave se elimina del agente tras 1 hora
```

---

## Escenarios prácticos

### Escenario 1 — Desarrollador con múltiples entornos

```bash
# ~/.ssh/config

# === Defaults ===
Host *
    ControlMaster auto
    ControlPath ~/.ssh/sockets/%C
    ControlPersist 300
    ServerAliveInterval 60
    ServerAliveCountMax 3
    IdentitiesOnly yes
    AddKeysToAgent yes

# === GitHub (cuenta personal) ===
Host github.com-personal
    HostName github.com
    User git
    IdentityFile ~/.ssh/id_ed25519_personal

# === GitHub (cuenta trabajo) ===
Host github.com
    HostName github.com
    User git
    IdentityFile ~/.ssh/id_ed25519_work

# === Trabajo ===
Host jump
    HostName bastion.empresa.com
    User jdoe
    IdentityFile ~/.ssh/id_ed25519_work

Host dev
    HostName dev-01.internal
    User jdoe
    ProxyJump jump

Host staging
    HostName staging-01.internal
    User jdoe
    ProxyJump jump
    LocalForward 3000 localhost:3000

Host prod
    HostName prod-01.internal
    User jdoe
    ProxyJump jump
    # Sin forwarding en prod (seguridad)

# === Servidor personal (VPS) ===
Host vps
    HostName 203.0.113.50
    User admin
    IdentityFile ~/.ssh/id_ed25519_personal
    Port 2222
```

Uso de múltiples cuentas de GitHub:

```bash
# Repositorio personal:
git remote set-url origin git@github.com-personal:mi-usuario/mi-repo.git

# Repositorio de trabajo (usa github.com normal):
git remote set-url origin git@github.com:empresa/repo.git
```

### Escenario 2 — Administrador de sistemas

```bash
# ~/.ssh/config

Host *
    ControlMaster auto
    ControlPath ~/.ssh/sockets/%C
    ControlPersist 600
    ServerAliveInterval 60
    IdentitiesOnly yes

# === Bastions por entorno ===
Host bastion-prod
    HostName bastion-prod.empresa.com
    User sysadmin
    IdentityFile ~/.ssh/id_ed25519_admin

Host bastion-dev
    HostName bastion-dev.empresa.com
    User sysadmin
    IdentityFile ~/.ssh/id_ed25519_admin

# === Servidores de producción ===
Host prod-web-*
    User sysadmin
    ProxyJump bastion-prod
    IdentityFile ~/.ssh/id_ed25519_admin

Host prod-web-1
    HostName 10.0.1.10
Host prod-web-2
    HostName 10.0.1.11
Host prod-web-3
    HostName 10.0.1.12

# === Servidores de desarrollo ===
Host dev-*
    User sysadmin
    ProxyJump bastion-dev
    IdentityFile ~/.ssh/id_ed25519_admin

Host dev-web
    HostName 10.1.1.10
Host dev-db
    HostName 10.1.1.20

# === Monitorización ===
Host grafana
    HostName 10.0.2.50
    User sysadmin
    ProxyJump bastion-prod
    LocalForward 3000 localhost:3000

Host prometheus
    HostName 10.0.2.51
    User sysadmin
    ProxyJump bastion-prod
    LocalForward 9090 localhost:9090
```

```bash
# Uso:
ssh prod-web-1              # salta por bastion-prod automáticamente
ssh dev-db                  # salta por bastion-dev
ssh grafana                 # túnel a Grafana + salto por bastion
# http://localhost:3000     → Grafana en producción
```

### Escenario 3 — Cloud con IPs dinámicas

```bash
# AWS — instancias con IPs que cambian

Host aws-*
    User ec2-user
    IdentityFile ~/.ssh/aws-key.pem
    StrictHostKeyChecking no       # IPs cambian constantemente
    UserKnownHostsFile /dev/null   # no guardar known_hosts (IPs temporales)
    LogLevel ERROR                 # no mostrar warnings de known_hosts

Host aws-prod
    HostName 54.X.X.X              # actualizar cuando cambie

Host aws-staging
    HostName 18.X.X.X

# GCP
Host gcp-*
    User tu_usuario
    IdentityFile ~/.ssh/google_compute_engine
    StrictHostKeyChecking no
    UserKnownHostsFile /dev/null

# Alternativa para AWS: usar SSM en lugar de SSH directo
# aws ssm start-session --target i-1234567890abcdef0
```

> **Sobre `StrictHostKeyChecking no`**: solo es aceptable para instancias cloud efímeras cuyas IPs y claves de host cambian con frecuencia. Nunca para servidores permanentes.

### Escenario 4 — Túneles predefinidos

```bash
# Acceso rápido a servicios internos con un solo comando

Host tunnel-all
    HostName bastion.empresa.com
    User admin
    IdentityFile ~/.ssh/id_ed25519_work
    LocalForward 5432 db.internal:5432        # PostgreSQL
    LocalForward 6379 redis.internal:6379     # Redis
    LocalForward 3000 grafana.internal:3000   # Grafana
    LocalForward 8080 jenkins.internal:8080   # Jenkins
    LocalForward 9200 elastic.internal:9200   # Elasticsearch
    RequestTTY no

# Uso:
# ssh -N tunnel-all
# → Todos los servicios accesibles vía localhost
```

---

## Errores comunes

### 1. Host * al principio del archivo

```bash
# MAL — Host * bloquea directivas de hosts específicos
Host *
    User default-user
    IdentityFile ~/.ssh/id_rsa

Host prod
    User admin              # IGNORADO — User ya fue asignado por Host *
    IdentityFile ~/.ssh/id_prod  # IGNORADO — IdentityFile ya asignado

# BIEN — Host * al final
Host prod
    User admin
    IdentityFile ~/.ssh/id_prod

Host *
    User default-user
    IdentityFile ~/.ssh/id_rsa
    # Solo aplica si un host no definió User/IdentityFile arriba
```

### 2. Ofrecer demasiadas claves sin IdentitiesOnly

```bash
# Si tienes 8 claves en ssh-agent y el servidor tiene MaxAuthTries 3:
ssh servidor
# "Too many authentication failures"

# SSH prueba todas las claves del agente en orden antes de las de config

# Solución:
Host *
    IdentitiesOnly yes
    # Solo ofrece la clave del IdentityFile, no las del agente

Host servidor
    IdentityFile ~/.ssh/id_servidor
    # Solo se ofrece esta clave
```

### 3. ProxyJump circular

```bash
# MAL — *.internal necesita bastion, pero bastion coincide con *.internal
Host *.internal
    ProxyJump bastion

Host bastion
    HostName bastion.internal    # ← ¡coincide con *.internal!
    # Resultado: bastion intenta saltar por sí mismo → loop infinito

# BIEN — excluir bastion del patrón
Host bastion
    HostName bastion.internal
    # Sin ProxyJump (conexión directa)

Host *.internal !bastion
    ProxyJump bastion

# O usar la IP directa para bastion:
Host bastion
    HostName 203.0.113.10    # IP pública, no el hostname .internal
```

### 4. ControlPath con caracteres problemáticos

```bash
# MAL — hostname con caracteres especiales rompe el socket
ControlPath ~/.ssh/sockets/%r@%h:%p
# Si el hostname tiene ":", el path del socket es inválido

# BIEN — usar %C (hash único de la conexión)
ControlPath ~/.ssh/sockets/%C
# Genera un hash como: 4a8f2b3c1d9e...

# Alternativa segura con nombres legibles:
ControlPath ~/.ssh/sockets/%r@%h-%p
# Usa "-" en lugar de ":" para el puerto
```

### 5. Olvidar crear el directorio de sockets

```bash
# Configuraste multiplexación pero:
ControlPath ~/.ssh/sockets/%C
# Error: "ControlPath ... : No such file or directory"

# Solución:
mkdir -p ~/.ssh/sockets
chmod 700 ~/.ssh/sockets
```

---

## Cheatsheet

```bash
# === DIRECTIVAS DE CONEXIÓN ===
Host alias                                 # nombre para usar con ssh
    HostName IP_o_DNS                      # host real
    User usuario                           # usuario remoto
    Port 22                                # puerto
    IdentityFile ~/.ssh/clave              # clave privada
    IdentitiesOnly yes                     # solo esta clave

# === PROXYJUMP ===
    ProxyJump bastion                      # saltar por bastion
    ProxyJump host1,host2                  # cadena de saltos
    ProxyJump none                         # conexión directa (override)

# === FORWARDING ===
    LocalForward 9090 destino:80           # -L en config
    RemoteForward 8080 localhost:3000      # -R en config
    DynamicForward 1080                    # -D en config
    ForwardAgent yes                       # reenviar ssh-agent
    ForwardX11 yes                         # reenviar X11

# === MULTIPLEXACIÓN ===
    ControlMaster auto                     # reutilizar conexiones
    ControlPath ~/.ssh/sockets/%C          # socket de control
    ControlPersist 600                     # mantener 10 min

# === KEEPALIVE ===
    ServerAliveInterval 60                 # keepalive cada 60s
    ServerAliveCountMax 3                  # cerrar tras 3 fallos
    ConnectTimeout 10                      # timeout de conexión

# === SEGURIDAD ===
    StrictHostKeyChecking accept-new       # aceptar nuevos, rechazar cambios
    HashKnownHosts yes                     # hashear known_hosts
    AddKeysToAgent yes                     # añadir claves al agente
    HostKeyAlgorithms ssh-ed25519          # preferir Ed25519

# === ORGANIZACIÓN ===
Include config.d/*                         # incluir configs modulares
    # Poner ANTES de los bloques Host

# === PATRONES ===
Host *                                     # todos (usar al final)
Host prod-*                                # wildcard
Host !bastion *.internal                   # negación
Host web?                                  # un solo carácter

# === CLOUD (IPs efímeras) ===
    StrictHostKeyChecking no               # no verificar host key
    UserKnownHostsFile /dev/null           # no guardar
    LogLevel ERROR                         # sin warnings

# === OPERACIONES CONTROL SOCKET ===
ssh -O check host                          # verificar master
ssh -O exit host                           # cerrar master
ssh -O stop host                           # cierre graceful
ssh -O forward -L 8080:db:5432 host        # añadir forward a master

# === DIAGNÓSTICO ===
ssh -v host                                # verbosidad nivel 1
ssh -vv host                               # nivel 2 (negociación)
ssh -G host                                # mostrar config efectiva para host
ssh -G host | grep -i proxyj               # ver si usa proxy
```

---

## Ejercicios

### Ejercicio 1 — Configuración básica con aliases

Crea un `~/.ssh/config` que defina:

1. Un bloque `Host *` con defaults: `ServerAliveInterval 60`, `IdentitiesOnly yes`, multiplexación con `ControlPersist 300`
2. Un alias `local` que conecte a `localhost` con tu usuario actual
3. Un alias `docker-ssh` que conecte a un contenedor Docker corriendo SSH (puerto 2222 en localhost)
4. Verifica con `ssh -G local` y `ssh -G docker-ssh` que las directivas se resuelven correctamente

<details>
<summary>Solución</summary>

```bash
# Crear directorio de sockets
mkdir -p ~/.ssh/sockets

# Crear config (o añadir a existente)
cat >> ~/.ssh/config << 'EOF'

Host local
    HostName localhost
    User %(whoami)    # reemplazar con tu usuario real

Host docker-ssh
    HostName localhost
    User root
    Port 2222
    StrictHostKeyChecking no
    UserKnownHostsFile /dev/null

Host *
    ServerAliveInterval 60
    ServerAliveCountMax 3
    IdentitiesOnly yes
    ControlMaster auto
    ControlPath ~/.ssh/sockets/%C
    ControlPersist 300
EOF

# Reemplazar %(whoami) con tu usuario real:
sed -i "s/%(whoami)/$(whoami)/" ~/.ssh/config

# Verificar permisos
chmod 600 ~/.ssh/config

# Verificar resolución
ssh -G local | grep -iE "hostname|user|serveralive|controlmaster"
# hostname localhost
# user tu-usuario
# serveraliveinterval 60
# controlmaster auto

ssh -G docker-ssh | grep -iE "hostname|port|stricthost"
# hostname localhost
# port 2222
# stricthostkeychecking no
```

</details>

---

### Ejercicio 2 — Topología con bastion

Diseña un `~/.ssh/config` para esta infraestructura:

```
Internet → bastion.ejemplo.com (puerto 22)
               │
               ├── web1.internal (10.0.1.10)
               ├── web2.internal (10.0.1.11)
               ├── db.internal (10.0.1.20)
               └── monitoring.internal (10.0.1.30, Grafana en :3000)
```

Requisitos:
1. `ssh bastion` conecta directamente
2. `ssh web1`, `ssh web2`, `ssh db` pasan por bastion automáticamente
3. `ssh monitoring` pasa por bastion y abre un túnel a Grafana en localhost:3000
4. Todos usan la clave `~/.ssh/id_ed25519_trabajo`
5. Todos usan el usuario `sysadmin`

<details>
<summary>Solución</summary>

```bash
# ~/.ssh/config

Host bastion
    HostName bastion.ejemplo.com
    User sysadmin
    IdentityFile ~/.ssh/id_ed25519_trabajo

Host web1
    HostName 10.0.1.10

Host web2
    HostName 10.0.1.11

Host db
    HostName 10.0.1.20

Host monitoring
    HostName 10.0.1.30
    LocalForward 3000 localhost:3000

# Aplica a todos los internos (web1, web2, db, monitoring)
# pero no a bastion (porque bastion ya tiene sus directivas
# y no coincide con estos patterns)
Host web1 web2 db monitoring
    User sysadmin
    IdentityFile ~/.ssh/id_ed25519_trabajo
    ProxyJump bastion

Host *
    ServerAliveInterval 60
    ServerAliveCountMax 3
    IdentitiesOnly yes
    ControlMaster auto
    ControlPath ~/.ssh/sockets/%C
    ControlPersist 600
```

Verificar:

```bash
ssh -G bastion | grep -i proxy
# (sin ProxyJump — conexión directa)

ssh -G web1 | grep -iE "proxy|hostname"
# hostname 10.0.1.10
# proxyjump bastion

ssh -G monitoring | grep -iE "proxy|localforward"
# proxyjump bastion
# localforward 3000 localhost:3000
```

</details>

---

### Ejercicio 3 — Multiplexación y medición

1. Configura multiplexación con `ControlPersist 60`
2. Mide el tiempo de la primera conexión SSH a localhost: `time ssh local exit`
3. Mide el tiempo de la segunda conexión (debería reutilizar): `time ssh local exit`
4. Verifica que el socket de control existe
5. Compara los tiempos y explica la diferencia
6. Cierra el master con `ssh -O exit local`

<details>
<summary>Solución</summary>

```bash
# 1. Configuración (si no la tienes ya)
# Asegúrate de tener en ~/.ssh/config:
# Host *
#     ControlMaster auto
#     ControlPath ~/.ssh/sockets/%C
#     ControlPersist 60

mkdir -p ~/.ssh/sockets

# 2. Primera conexión (handshake completo)
time ssh local exit
# real    0m0.150s   ← handshake TCP + SSH + auth + crear master

# 3. Segunda conexión (reutiliza master)
time ssh local exit
# real    0m0.020s   ← ~10x más rápida, sin handshake

# 4. Verificar socket
ls -la ~/.ssh/sockets/
# srw-------  ... <hash>   ← socket UNIX del master

ssh -O check local
# Master running (pid=12345)

# 5. Explicación:
# Primera conexión: resolución DNS + TCP 3-way handshake +
#   SSH key exchange + autenticación + creación de sesión
# Segunda conexión: solo envía un mensaje por el socket UNIX
#   al master existente. No hay red, no hay cripto, no hay auth.
#   La diferencia es más dramática con servidores remotos (>100ms de latencia).

# 6. Cerrar
ssh -O exit local
# Exit request sent.

ls -la ~/.ssh/sockets/
# (vacío o sin el socket de local)
```

</details>

---

### Pregunta de reflexión

> Tienes 30 servidores en 3 entornos (dev, staging, prod), cada uno con su propio bastion. Cada entorno tiene su propia clave SSH. Un nuevo miembro se une al equipo y necesita configurar su acceso. ¿Cómo organizarías tu `~/.ssh/config` para que sea mantenible, compartible con el equipo (sin exponer claves privadas), y fácil de actualizar cuando se añadan o cambien servidores?

> **Razonamiento esperado**: usar una estructura modular con `Include config.d/*`. Crear archivos separados: `00-defaults` (multiplexación, keepalive, IdentitiesOnly), `10-dev` (bastion-dev + hosts dev), `20-staging`, `30-prod`. Cada archivo de entorno define su bastion con la clave correspondiente y los hosts usan `ProxyJump bastion-<env>`. Esta estructura se puede versionar en un repositorio del equipo (sin las claves privadas — solo la configuración). El nuevo miembro clona el repo, genera sus claves, las distribuye con `ssh-copy-id`, y copia los config files. Para actualizaciones, un `git pull` trae nuevos hosts. Las claves privadas nunca van al repositorio — cada persona tiene las suyas. Alternativa más escalable: certificados SSH con una CA por entorno, eliminando la necesidad de `authorized_keys` y simplificando el onboarding a "generar clave → firmar con CA → listo".

---

> **Fin del Capítulo 1 — SSH**. Los 5 temas cubren desde la configuración del servidor (sshd_config, autenticación, tunneling, hardening) hasta la configuración avanzada del cliente (ssh_config).
>
> **Siguiente capítulo**: C02 — Servidor DNS (BIND): instalación, zonas, tipos de servidor, DNSSEC.
