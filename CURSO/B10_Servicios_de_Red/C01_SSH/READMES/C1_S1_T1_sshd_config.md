# sshd_config — Configuración del servidor SSH

## Índice

1. [Arquitectura de OpenSSH](#arquitectura-de-openssh)
2. [Archivos de configuración](#archivos-de-configuración)
3. [Directivas de acceso](#directivas-de-acceso)
4. [Autenticación](#autenticación)
5. [Directivas de red](#directivas-de-red)
6. [Directivas de sesión](#directivas-de-sesión)
7. [Match blocks](#match-blocks)
8. [Drop-in directories](#drop-in-directories)
9. [Validación y recarga](#validación-y-recarga)
10. [Configuración por familia de distro](#configuración-por-familia-de-distro)
11. [Escenarios de configuración](#escenarios-de-configuración)
12. [Errores comunes](#errores-comunes)
13. [Cheatsheet](#cheatsheet)
14. [Ejercicios](#ejercicios)

---

## Arquitectura de OpenSSH

OpenSSH tiene dos componentes principales: el **cliente** (`ssh`) y el **servidor** (`sshd`). Este tema se centra en el servidor.

```
    ┌──────────────┐                    ┌──────────────────────┐
    │   Cliente    │                    │   Servidor (sshd)    │
    │              │                    │                      │
    │  ~/.ssh/     │    TCP :22         │  /etc/ssh/           │
    │  config      │◄──────────────────►│  sshd_config         │
    │  known_hosts │   Conexión SSH     │  ssh_host_*_key      │
    │  id_ed25519  │                    │  ssh_host_*_key.pub  │
    │              │                    │                      │
    └──────────────┘                    │  Proceso:            │
                                        │  sshd (master)       │
                                        │   └─ sshd (session)  │
                                        │       └─ shell/cmd   │
                                        └──────────────────────┘
```

El proceso `sshd` funciona con un modelo de **privilege separation**:

1. El proceso **master** corre como root, escucha en el puerto y acepta conexiones
2. Para cada conexión, fork un proceso **sin privilegios** que maneja el protocolo SSH
3. Tras autenticación exitosa, crea un proceso de **sesión** con los privilegios del usuario

```
    sshd (root, master, PID 1)
      │
      ├─ sshd (sshd user, pre-auth)  ← maneja protocolo
      │    │
      │    └─ sshd (usuario, post-auth) ← privilege separation
      │         │
      │         └─ bash (usuario)       ← sesión del usuario
      │
      └─ sshd (sshd user, pre-auth)  ← otra conexión
           │
           └─ ...
```

Esta separación de privilegios es una defensa en profundidad: si un atacante explota una vulnerabilidad en el manejo del protocolo, obtiene acceso como el usuario sin privilegios `sshd`, no como root.

---

## Archivos de configuración

### Ubicación principal

| Archivo | Propósito |
|---|---|
| `/etc/ssh/sshd_config` | Configuración principal del servidor |
| `/etc/ssh/sshd_config.d/*.conf` | Drop-in configs (distros modernas) |
| `/etc/ssh/ssh_config` | Configuración global del **cliente** (no confundir) |
| `/etc/ssh/ssh_host_*_key` | Claves privadas del host |
| `/etc/ssh/ssh_host_*_key.pub` | Claves públicas del host |
| `/etc/ssh/moduli` | Parámetros Diffie-Hellman para intercambio de claves |

### Claves del host

El servidor genera claves automáticamente durante la instalación. Son la identidad criptográfica del servidor:

```bash
ls -la /etc/ssh/ssh_host_*
# ssh_host_ecdsa_key       ← clave privada ECDSA
# ssh_host_ecdsa_key.pub   ← clave pública ECDSA
# ssh_host_ed25519_key     ← clave privada Ed25519
# ssh_host_ed25519_key.pub ← clave pública Ed25519
# ssh_host_rsa_key         ← clave privada RSA
# ssh_host_rsa_key.pub     ← clave pública RSA
```

Cuando un cliente se conecta por primera vez, ve la clave pública del host y pregunta si confía en ella. Esa huella se guarda en `~/.ssh/known_hosts`. Si la clave cambia (por ejemplo, reinstalaste el servidor), el cliente muestra el alarmante `WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!`.

### Sintaxis de sshd_config

```bash
# Comentarios empiezan con #
# Formato: Directiva valor
# Las directivas NO son case-sensitive, pero la convención es PascalCase
# Los valores SÍ pueden ser case-sensitive (nombres de usuario, rutas)
# La PRIMERA coincidencia gana (excepto en Match blocks)

Port 22
PermitRootLogin no
PasswordAuthentication yes
```

> **Regla crítica**: en `sshd_config`, la **primera ocurrencia** de una directiva es la que se aplica. Si tienes `PermitRootLogin yes` en la línea 10 y `PermitRootLogin no` en la línea 50, se usa `yes`. Esto es lo contrario a muchos otros archivos de configuración donde la última línea gana.

---

## Directivas de acceso

### PermitRootLogin

Controla si root puede autenticarse por SSH:

| Valor | Comportamiento |
|---|---|
| `yes` | Root puede hacer login con cualquier método |
| `no` | Root no puede hacer login de ninguna forma |
| `prohibit-password` | Root solo puede con clave pública, no con password (**default en distros modernas**) |
| `forced-commands-only` | Root solo puede con clave pública Y solo si la clave tiene un `command=` en `authorized_keys` |

```bash
# Producción recomendado:
PermitRootLogin no
# Los administradores hacen login como su usuario y usan sudo

# Automatización (backups, Ansible):
PermitRootLogin forced-commands-only
# Permite ejecutar solo el comando específico configurado en authorized_keys
```

`forced-commands-only` en la práctica:

```bash
# En /root/.ssh/authorized_keys del servidor:
command="/usr/local/bin/backup.sh",no-port-forwarding,no-X11-forwarding,no-agent-forwarding ssh-ed25519 AAAA... backup@server
```

Con esta configuración, la clave solo puede ejecutar `backup.sh`, sin importar qué comando envíe el cliente.

### AllowUsers, DenyUsers, AllowGroups, DenyGroups

Controlan qué usuarios pueden conectarse. Se evalúan en este orden:

```
    DenyUsers → AllowUsers → DenyGroups → AllowGroups

    1. Si el usuario está en DenyUsers  → DENEGADO (fin)
    2. Si AllowUsers existe Y el usuario NO está → DENEGADO (fin)
    3. Si el usuario está en DenyGroups  → DENEGADO (fin)
    4. Si AllowGroups existe Y el usuario NO está en ningún grupo → DENEGADO (fin)
    5. Si ninguna restricción aplica → PERMITIDO
```

Ejemplos:

```bash
# Solo estos usuarios pueden conectarse
AllowUsers admin deploy monitoring

# Estos usuarios NUNCA pueden conectarse (gana sobre AllowUsers)
DenyUsers guest test

# Solo miembros de estos grupos
AllowGroups sshusers wheel

# Combinación con host: usuario solo desde ciertas IPs
AllowUsers admin@192.168.1.* deploy@10.0.0.0/8

# Múltiples patrones
AllowUsers admin deploy@192.168.1.* monitoring@10.0.0.5
```

> **Impacto de `AllowUsers`/`AllowGroups`**: el momento en que añades una de estas directivas, **todos los usuarios no listados quedan bloqueados**. Si añades `AllowUsers admin` y olvidas a `deploy`, deploy queda fuera inmediatamente.

### MaxAuthTries y MaxSessions

```bash
# Intentos de autenticación por conexión (default: 6)
MaxAuthTries 3

# Sesiones multiplexadas por conexión (default: 10)
MaxSessions 5

# Conexiones simultáneas no autenticadas que sshd acepta (default: 10:30:100)
# start:rate:full
# Empieza a rechazar con 30% de probabilidad tras 10 no autenticadas,
# rechaza 100% tras 100
MaxStartups 10:30:60
```

`MaxStartups` tiene un formato peculiar de tres números que implementa una **probabilidad creciente de rechazo**:

```
    Conexiones no autenticadas
    0        10        30        60        100
    |---------|---------|---------|---------|
    aceptar    30% drop   lineal   100% drop
    todas      empieza    crece    todo
                                   rechazado
```

---

## Autenticación

### Métodos de autenticación

```bash
# Autenticación por password (desactivar en producción)
PasswordAuthentication no

# Autenticación por clave pública (el método preferido)
PubkeyAuthentication yes

# Autenticación challenge-response (PAM, 2FA)
KbdInteractiveAuthentication yes    # nombre moderno
# ChallengeResponseAuthentication yes  # nombre legacy, mismo efecto

# Autenticación por host (.rhosts, shosts — NO usar)
HostbasedAuthentication no

# Autenticación GSSAPI (Kerberos)
GSSAPIAuthentication no
```

### Archivo authorized_keys

```bash
# Dónde busca las claves públicas autorizadas
AuthorizedKeysFile .ssh/authorized_keys .ssh/authorized_keys2

# El path es relativo al home del usuario
# %h = home del usuario, %u = nombre del usuario
# Ejemplo centralizado:
AuthorizedKeysFile /etc/ssh/authorized_keys/%u
```

### Autenticación PAM

```bash
# Habilitar PAM (default en la mayoría de distros)
UsePAM yes
```

Cuando `UsePAM yes` está activo:

- PAM maneja account checks (expiración, restricciones)
- PAM maneja session setup (limits, environment)
- Si `PasswordAuthentication yes`, PAM también maneja la verificación de password
- Si `PasswordAuthentication no`, PAM **aún** se usa para account/session, solo se desactiva la verificación de password por PAM

```
    UsePAM yes + PasswordAuthentication yes:
    ┌────────────────────────────────────────┐
    │ PAM auth     → verifica password       │
    │ PAM account  → verifica restricciones  │
    │ PAM session  → configura entorno       │
    └────────────────────────────────────────┘

    UsePAM yes + PasswordAuthentication no:
    ┌────────────────────────────────────────┐
    │ PAM auth     → NO se usa               │
    │ PAM account  → SÍ verifica             │
    │ PAM session  → SÍ configura            │
    └────────────────────────────────────────┘
```

### Login banner y mensajes

```bash
# Banner mostrado ANTES de la autenticación (aviso legal)
Banner /etc/ssh/banner.txt
# Típicamente: "Unauthorized access is prohibited..."

# PrintMotd — mostrar /etc/motd DESPUÉS del login
PrintMotd yes

# PrintLastLog — mostrar la última conexión exitosa
PrintLastLog yes
# "Last login: Thu Mar 20 14:30:22 2026 from 192.168.1.100"
```

---

## Directivas de red

### Port y ListenAddress

```bash
# Puerto de escucha (default: 22)
Port 22
# Puedes especificar múltiples puertos:
Port 22
Port 2222

# Dirección de escucha (default: todas)
ListenAddress 0.0.0.0      # todas las IPv4
ListenAddress ::            # todas las IPv6
# Restringir a una interfaz:
ListenAddress 192.168.1.10
# Combinar dirección + puerto:
ListenAddress 192.168.1.10:22
ListenAddress 10.0.0.5:2222
```

> **Sobre cambiar el puerto**: cambiar SSH del puerto 22 a otro (ej. 2222) **no es una medida de seguridad real** — es "security through obscurity". Un escaneo de puertos lo encuentra en segundos. Sí reduce el ruido de bots automatizados en los logs, lo cual puede ser útil, pero no sustituye a deshabilitar passwords y usar claves.

### AddressFamily

```bash
# Familia de direcciones (default: any)
AddressFamily any     # IPv4 + IPv6
AddressFamily inet    # solo IPv4
AddressFamily inet6   # solo IPv6
```

### Timeouts y keepalive

```bash
# Tiempo para completar la autenticación (default: 120s)
LoginGraceTime 60

# Intervalo de keepalive del servidor al cliente (default: 0 = desactivado)
ClientAliveInterval 300    # enviar keepalive cada 5 min
ClientAliveCountMax 3      # desconectar tras 3 keepalives sin respuesta
# Total: desconexión tras 15 min de inactividad (300 * 3)

# TCPKeepAlive — keepalive a nivel TCP (no SSH)
TCPKeepAlive yes     # default, detecta conexiones rotas
```

Diferencia entre `ClientAliveInterval` y `TCPKeepAlive`:

```
    TCPKeepAlive:
    - Opera a nivel TCP (kernel)
    - Detecta conexiones rotas (cable cortado, host caído)
    - No detecta cliente "vivo pero inactivo"
    - Susceptible a spoofing

    ClientAliveInterval:
    - Opera a nivel SSH (cifrado)
    - Envía mensaje SSH cifrado al cliente
    - El cliente debe responder con un paquete SSH válido
    - Más fiable para detectar inactividad real
    - No puede ser falsificado
```

---

## Directivas de sesión

### Forwarding

```bash
# Port forwarding (túneles SSH)
AllowTcpForwarding yes          # default
AllowTcpForwarding no           # desactivar todo forwarding
AllowTcpForwarding local        # solo -L (local forward)
AllowTcpForwarding remote       # solo -R (remote forward)

# Abrir puertos en todas las interfaces para -R (default: no)
GatewayPorts no
# "no" = remote forward solo escucha en 127.0.0.1
# "yes" = escucha en 0.0.0.0 (todas las interfaces)
# "clientspecified" = el cliente decide la dirección de bind

# X11 Forwarding (GUI remota)
X11Forwarding yes               # permitir -X/-Y
X11DisplayOffset 10             # primer display number
X11UseLocalhost yes             # default, más seguro

# Agent forwarding (reenvío de ssh-agent)
AllowAgentForwarding yes        # default

# StreamLocalForwarding (UNIX domain sockets)
AllowStreamLocalForwarding yes  # default
```

### Entorno y ejecución

```bash
# Permitir que el cliente envíe variables de entorno
AcceptEnv LANG LC_*           # típico: solo locale
# El cliente configura SendEnv en ssh_config

# Forzar un comando (ignora lo que pida el cliente)
ForceCommand /usr/local/bin/restricted-shell.sh
# Útil en Match blocks para usuarios específicos

# Subsistema (SFTP)
Subsystem sftp /usr/lib/openssh/sftp-server        # Debian
Subsystem sftp /usr/libexec/openssh/sftp-server     # RHEL
# internal-sftp es más seguro (corre in-process, compatible con ChrootDirectory)
Subsystem sftp internal-sftp

# Chroot — confinar usuario a un directorio
ChrootDirectory /home/sftp/%u
# Requisitos:
# - El directorio chroot debe ser propiedad de root
# - El directorio chroot no puede tener permisos de escritura para grupo/otros
# - Combinado con ForceCommand internal-sftp para SFTP-only
```

---

## Match blocks

Los `Match` blocks permiten aplicar configuración condicional basada en usuario, grupo, dirección IP o puerto:

```bash
# Sintaxis: Match criterio valor [criterio valor ...]
# Las directivas dentro del Match aplican SOLO a conexiones que coincidan
# Un Match termina con el siguiente Match o el final del archivo

# Match por usuario
Match User deploy
    PasswordAuthentication no
    AllowTcpForwarding no
    X11Forwarding no
    ForceCommand /usr/local/bin/deploy.sh

# Match por grupo
Match Group sftponly
    ChrootDirectory /home/sftp/%u
    ForceCommand internal-sftp
    AllowTcpForwarding no
    X11Forwarding no

# Match por dirección IP
Match Address 192.168.1.0/24
    PasswordAuthentication yes    # solo desde LAN

Match Address *,!192.168.1.0/24
    PasswordAuthentication no     # desde fuera, solo claves

# Match por múltiples criterios (AND lógico)
Match User admin Address 10.0.0.0/8
    PermitRootLogin yes           # solo admin desde red interna

# Match all — aplica a todos (útil como "default" final)
Match all
    PasswordAuthentication no
```

### Directivas permitidas en Match

No todas las directivas se pueden usar dentro de un `Match`. Las más comunes que **sí** se pueden:

```
AcceptEnv, AllowAgentForwarding, AllowGroups, AllowStreamLocalForwarding,
AllowTcpForwarding, AllowUsers, AuthenticationMethods, AuthorizedKeysFile,
Banner, ChrootDirectory, DenyGroups, DenyUsers, ForceCommand,
GatewayPorts, HostbasedAuthentication, KbdInteractiveAuthentication,
MaxAuthTries, MaxSessions, PasswordAuthentication, PermitEmptyPasswords,
PermitOpen, PermitRootLogin, PermitTTY, PermitTunnel,
PubkeyAuthentication, X11DisplayOffset, X11Forwarding, X11UseLocalhost
```

### Orden de evaluación

```
    sshd_config se procesa así:

    1. Directivas globales (antes del primer Match)
       → aplican como default a TODOS

    2. Match blocks (en orden)
       → si la conexión coincide, las directivas del Match
         SOBRESCRIBEN las globales

    3. Primera coincidencia de directiva global gana
       (si hay duplicados fuera de Match)

    4. Match blocks se acumulan:
       si una conexión coincide con múltiples Match,
       se aplican TODOS los que coincidan, en orden
```

---

## Drop-in directories

Las distribuciones modernas usan un directorio `sshd_config.d/` para modularizar la configuración:

```bash
# En sshd_config principal (última línea):
Include /etc/ssh/sshd_config.d/*.conf
```

```bash
# Fedora/RHEL 9+
ls /etc/ssh/sshd_config.d/
# 50-redhat.conf  ← defaults de la distro

# Ubuntu 22.04+
ls /etc/ssh/sshd_config.d/
# (vacío por defecto, para que el admin añada)
```

> **Cuidado con el orden**: como la primera ocurrencia de una directiva gana, si `Include` está al **principio** de `sshd_config`, los drop-in files tienen prioridad sobre el archivo principal. Si está al **final**, el archivo principal tiene prioridad. Verifica dónde está el `Include` en tu distro.

```bash
# Fedora 41 / RHEL 9: Include está al PRINCIPIO
head -5 /etc/ssh/sshd_config
# Include /etc/ssh/sshd_config.d/*.conf  ← línea 1 o 2
# Los drop-ins GANAN sobre sshd_config

# Ubuntu 24.04: Include está al FINAL
tail -3 /etc/ssh/sshd_config
# Include /etc/ssh/sshd_config.d/*.conf
# El archivo principal GANA sobre drop-ins
```

Esto tiene implicaciones prácticas enormes:

```bash
# En Fedora: si quieres cambiar PermitRootLogin, debes hacerlo en un drop-in
# porque 50-redhat.conf ya lo define y se procesa ANTES que sshd_config
echo "PermitRootLogin no" > /etc/ssh/sshd_config.d/10-security.conf

# En Ubuntu: puedes editar sshd_config directamente porque se procesa primero
```

---

## Validación y recarga

### Validar la configuración

**Siempre** valida antes de recargar — un error de sintaxis puede impedirte reconectarte:

```bash
# Validar sintaxis
sshd -t
# Si no hay output → configuración válida
# Si hay error → muestra la línea y el problema

# Validar y mostrar la configuración efectiva
sshd -T
# Muestra TODAS las directivas con sus valores resueltos
# Útil para ver qué valor se está usando realmente

# Validar para una conexión específica (con Match)
sshd -T -C user=deploy,host=192.168.1.50,addr=192.168.1.50
# Muestra la configuración que se aplicaría a esa conexión
```

### Recargar el servicio

```bash
# Método seguro: reload (re-lee config, no mata sesiones activas)
sudo systemctl reload sshd      # Fedora/RHEL
sudo systemctl reload ssh       # Debian/Ubuntu (nombre del servicio distinto)

# Método alternativo: enviar SIGHUP al master
sudo kill -HUP $(pgrep -o sshd)

# Restart — solo si reload no funciona (mata sesiones)
sudo systemctl restart sshd
```

> **Protocolo de seguridad**: antes de hacer cualquier cambio en sshd_config, abre una **segunda sesión SSH** al servidor y déjala abierta. Si la nueva configuración te bloquea, puedes revertir desde la sesión existente. Las sesiones SSH activas no se ven afectadas por un reload.

---

## Configuración por familia de distro

### Diferencias entre Debian/Ubuntu y Fedora/RHEL

| Aspecto | Debian/Ubuntu | Fedora/RHEL |
|---|---|---|
| Paquete | `openssh-server` | `openssh-server` |
| Nombre del servicio | `ssh` / `ssh.service` | `sshd` / `sshd.service` |
| Config principal | `/etc/ssh/sshd_config` | `/etc/ssh/sshd_config` |
| Drop-in dir | `/etc/ssh/sshd_config.d/` | `/etc/ssh/sshd_config.d/` |
| Include position | Final (config principal gana) | Principio (drop-ins ganan) |
| SFTP subsystem path | `/usr/lib/openssh/sftp-server` | `/usr/libexec/openssh/sftp-server` |
| SELinux | No (AppArmor) | Sí — `sshd_t` context |
| Firewall | `ufw allow ssh` | `firewall-cmd --add-service=ssh` |
| Default PermitRootLogin | `prohibit-password` | `prohibit-password` |
| Socket activation | `ssh.socket` (Ubuntu 24.04+) | `sshd.socket` (disponible) |

### SELinux y SSH (Fedora/RHEL)

Si cambias el puerto SSH en RHEL/Fedora, SELinux lo bloqueará:

```bash
# Cambiar puerto a 2222
Port 2222

# SELinux solo permite ssh_port_t, que por defecto es 22
sudo semanage port -l | grep ssh
# ssh_port_t    tcp    22

# Añadir el nuevo puerto al contexto SELinux
sudo semanage port -a -t ssh_port_t -p tcp 2222

# Verificar
sudo semanage port -l | grep ssh
# ssh_port_t    tcp    22, 2222
```

### Socket activation (systemd)

Ubuntu 24.04+ usa activación por socket por defecto en lugar del demonio persistente:

```bash
# Tradicional: sshd corre como demonio permanente
systemctl status sshd

# Socket activation: systemd escucha en el puerto,
# lanza sshd solo cuando llega una conexión
systemctl status ssh.socket    # socket listener
systemctl status ssh.service   # se activa bajo demanda

# Implicación: los cambios de Port/ListenAddress van en el archivo de socket
sudo systemctl edit ssh.socket
# [Socket]
# ListenStream=
# ListenStream=2222
```

---

## Escenarios de configuración

### Escenario 1 — Servidor de producción básico

```bash
# /etc/ssh/sshd_config.d/10-hardening.conf

# Deshabilitar autenticación por password
PasswordAuthentication no

# Deshabilitar login de root
PermitRootLogin no

# Solo usuarios autorizados
AllowGroups sshusers

# Timeout de autenticación más corto
LoginGraceTime 30
MaxAuthTries 3

# Desconectar sesiones inactivas (15 min)
ClientAliveInterval 300
ClientAliveCountMax 3

# Deshabilitar features no necesarios
X11Forwarding no
AllowTcpForwarding no
AllowAgentForwarding no
PermitTunnel no
```

### Escenario 2 — Servidor SFTP con chroot

```bash
# /etc/ssh/sshd_config.d/20-sftp.conf

# Usar internal-sftp para compatibilidad con chroot
Subsystem sftp internal-sftp

Match Group sftponly
    # Confinar al directorio del usuario
    ChrootDirectory /srv/sftp/%u
    ForceCommand internal-sftp

    # Deshabilitar todo excepto SFTP
    AllowTcpForwarding no
    AllowAgentForwarding no
    X11Forwarding no
    PermitTTY no    # sin terminal interactiva
```

Preparar el directorio chroot:

```bash
# Crear estructura (root debe ser dueño del chroot)
sudo mkdir -p /srv/sftp/usuario1/uploads
sudo chown root:root /srv/sftp/usuario1
sudo chmod 755 /srv/sftp/usuario1

# El subdirectorio sí puede ser del usuario
sudo chown usuario1:sftponly /srv/sftp/usuario1/uploads
sudo chmod 770 /srv/sftp/usuario1/uploads

# Añadir usuario al grupo
sudo usermod -aG sftponly usuario1
```

### Escenario 3 — Acceso diferenciado por red

```bash
# /etc/ssh/sshd_config.d/30-network-policy.conf

# Default: solo claves, sin forwarding
PasswordAuthentication no
AllowTcpForwarding no
X11Forwarding no

# Desde la red interna: permitir passwords (para emergencias)
Match Address 10.0.0.0/8,192.168.0.0/16
    PasswordAuthentication yes

# Administradores desde la red de gestión: todo permitido
Match Group admins Address 10.0.100.0/24
    AllowTcpForwarding yes
    AllowAgentForwarding yes
    PermitTunnel yes

# Cuenta de despliegue: solo puede ejecutar scripts
Match User deploy
    ForceCommand /usr/local/bin/deploy-wrapper.sh
    PasswordAuthentication no
    AllowTcpForwarding no
```

### Escenario 4 — Multi-factor authentication

```bash
# /etc/ssh/sshd_config.d/40-mfa.conf

# Requerir clave pública + código TOTP (Google Authenticator)
AuthenticationMethods publickey,keyboard-interactive

# Habilitar PAM y keyboard-interactive para el segundo factor
UsePAM yes
KbdInteractiveAuthentication yes

# El módulo PAM maneja TOTP (configurar en /etc/pam.d/sshd)
```

`AuthenticationMethods` permite combinar métodos:

```bash
# Un solo método (OR):
AuthenticationMethods publickey password
# = clave pública O password

# Dos métodos requeridos (AND):
AuthenticationMethods publickey,password
# = clave pública Y password (la coma es AND)

# Combinaciones:
AuthenticationMethods publickey,keyboard-interactive publickey,password
# = (clave + OTP) O (clave + password)
```

---

## Errores comunes

### 1. Editando sshd_config sin validar antes de recargar

```bash
# MAL — recarga directa sin validar
sudo vim /etc/ssh/sshd_config
sudo systemctl restart sshd
# Si hay un error de sintaxis, sshd no arranca y pierdes acceso

# BIEN — validar siempre
sudo vim /etc/ssh/sshd_config
sudo sshd -t && sudo systemctl reload sshd
# sshd -t falla si hay errores, el && previene el reload
```

**Más seguro aún**: siempre tener una sesión SSH abierta antes de hacer cambios.

### 2. Olvidar que la primera ocurrencia gana

```bash
# En sshd_config:
PermitRootLogin yes         # ← línea 15, ESTA se aplica
# ... 200 líneas después ...
PermitRootLogin no          # ← línea 215, IGNORADA

# Diagnóstico:
sshd -T | grep permitrootlogin
# permitrootlogin yes   ← confirma cuál se está usando
```

Esto es especialmente problemático con drop-in files cuando `Include` está al principio del archivo.

### 3. Añadir AllowUsers y bloquear a todos los demás

```bash
# Tenías acceso con tu usuario "admin" y el usuario "deploy"
# Quieres restringir acceso, añades:
AllowUsers deploy
# ¡Acabas de bloquearte! Solo "deploy" puede entrar

# BIEN — incluir todos los usuarios necesarios
AllowUsers admin deploy monitoring
```

### 4. ChrootDirectory con permisos incorrectos

```bash
# El chroot FALLA silenciosamente si:
# - El directorio no es propiedad de root
# - El directorio tiene permisos de escritura para grupo/otros

# MAL
sudo chown usuario1:usuario1 /srv/sftp/usuario1
# Error en el log: "fatal: bad ownership or modes for chroot directory"

# BIEN
sudo chown root:root /srv/sftp/usuario1
sudo chmod 755 /srv/sftp/usuario1
```

### 5. Cambiar el puerto sin actualizar SELinux/firewall

```bash
# Cambiaste Port 2222 en sshd_config, pero:

# Fedora/RHEL — SELinux bloquea
# journalctl -u sshd: "error: Bind to port 2222 on 0.0.0.0 failed: Permission denied"
sudo semanage port -a -t ssh_port_t -p tcp 2222

# Firewall — el nuevo puerto no está abierto
sudo firewall-cmd --add-port=2222/tcp --permanent
sudo firewall-cmd --reload

# Debian/Ubuntu con ufw
sudo ufw allow 2222/tcp
```

---

## Cheatsheet

```bash
# === VALIDACIÓN ===
sshd -t                                   # validar sintaxis
sshd -T                                   # mostrar config efectiva
sshd -T -C user=X,addr=Y                 # config para conexión específica

# === SERVICIO ===
sudo systemctl reload sshd               # recargar config (RHEL)
sudo systemctl reload ssh                 # recargar config (Debian)
sudo systemctl status sshd               # estado del servicio

# === DIRECTIVAS CLAVE ===
PermitRootLogin no                        # bloquear root
PasswordAuthentication no                 # solo claves
AllowUsers admin deploy                   # whitelist usuarios
AllowGroups sshusers                      # whitelist grupos
PubkeyAuthentication yes                  # habilitar claves públicas
MaxAuthTries 3                            # limitar intentos

# === TIMEOUTS ===
LoginGraceTime 60                         # tiempo para autenticar
ClientAliveInterval 300                   # keepalive cada 5 min
ClientAliveCountMax 3                     # desconexión tras 3 fallos

# === FORWARDING ===
AllowTcpForwarding no                     # deshabilitar túneles
X11Forwarding no                          # deshabilitar GUI
AllowAgentForwarding no                   # deshabilitar agent

# === MATCH ===
Match User deploy                         # por usuario
Match Group sftponly                      # por grupo
Match Address 192.168.1.0/24             # por IP
Match User admin Address 10.0.0.0/8      # combinado (AND)

# === SFTP CHROOT ===
Subsystem sftp internal-sftp              # usar SFTP interno
ChrootDirectory /srv/sftp/%u              # directorio jaula
ForceCommand internal-sftp                # forzar SFTP-only
PermitTTY no                              # sin terminal

# === DIAGNÓSTICO ===
sudo journalctl -u sshd -f               # logs en tiempo real
sudo journalctl -u sshd --since "1h ago" # última hora
sudo ss -tlnp | grep sshd                # puerto de escucha
```

---

## Ejercicios

### Ejercicio 1 — Hardening básico

Partiendo de un `sshd_config` con valores por defecto, crea un archivo drop-in `/etc/ssh/sshd_config.d/10-hardening.conf` que:

1. Desactive autenticación por password
2. Desactive login de root
3. Limite a 3 intentos de autenticación
4. Desconecte sesiones inactivas tras 10 minutos
5. Desactive X11 forwarding, TCP forwarding y agent forwarding

Antes de aplicar:
- Valida la configuración con `sshd -t`
- Verifica la configuración efectiva con `sshd -T | grep -E "password|rootlogin|maxauth|clientalive|x11|tcpforwarding|agentforwarding"`
- Mantén una sesión SSH abierta como respaldo

<details>
<summary>Solución</summary>

```bash
sudo tee /etc/ssh/sshd_config.d/10-hardening.conf << 'EOF'
PasswordAuthentication no
PermitRootLogin no
MaxAuthTries 3
ClientAliveInterval 300
ClientAliveCountMax 2
X11Forwarding no
AllowTcpForwarding no
AllowAgentForwarding no
EOF

# Validar
sudo sshd -t

# Verificar valores efectivos
sudo sshd -T | grep -iE "passwordauthentication|permitrootlogin|maxauthtries|clientaliveinterval|clientalivecountmax|x11forwarding|allowtcpforwarding|allowagentforwarding"

# Aplicar (mantener otra sesión abierta)
sudo systemctl reload sshd   # o ssh en Debian/Ubuntu
```

`ClientAliveInterval 300` × `ClientAliveCountMax 2` = 600 segundos = 10 minutos de inactividad antes de desconexión.

</details>

---

### Ejercicio 2 — SFTP-only con chroot

Configura un usuario `uploader` que:

1. Solo pueda usar SFTP (sin shell interactiva, sin forwarding)
2. Esté confinado (chroot) a `/srv/sftp/uploader/`
3. Pueda escribir en un subdirectorio `files/` pero no en la raíz del chroot
4. Use el grupo `sftponly` para la configuración

Incluye: creación del usuario, permisos de directorios, configuración SSH y prueba.

<details>
<summary>Solución</summary>

```bash
# 1. Crear grupo y usuario
sudo groupadd sftponly
sudo useradd -m -g sftponly -s /usr/sbin/nologin uploader
sudo passwd uploader

# 2. Crear estructura de directorios
sudo mkdir -p /srv/sftp/uploader/files
sudo chown root:root /srv/sftp/uploader        # root owns chroot
sudo chmod 755 /srv/sftp/uploader              # no group/other write
sudo chown uploader:sftponly /srv/sftp/uploader/files
sudo chmod 770 /srv/sftp/uploader/files

# 3. Configuración SSH
sudo tee /etc/ssh/sshd_config.d/20-sftp.conf << 'EOF'
Subsystem sftp internal-sftp

Match Group sftponly
    ChrootDirectory /srv/sftp/%u
    ForceCommand internal-sftp
    AllowTcpForwarding no
    AllowAgentForwarding no
    X11Forwarding no
    PermitTTY no
EOF

# 4. Validar y aplicar
sudo sshd -t && sudo systemctl reload sshd

# 5. Probar
sftp uploader@localhost
# sftp> ls
# files
# sftp> cd files
# sftp> put testfile.txt    ← debe funcionar
# sftp> cd /
# sftp> put testfile.txt    ← debe fallar (permission denied)
```

Nota: si `Subsystem sftp` ya está definido en `sshd_config` principal, no lo dupliques en el drop-in — la primera definición gana.

</details>

---

### Ejercicio 3 — Políticas diferenciadas por red

Diseña una configuración donde:

1. Desde la red de oficina (192.168.1.0/24): password y claves permitidos
2. Desde la red de servidores (10.0.0.0/8): solo claves, con forwarding habilitado
3. Desde cualquier otra IP: solo claves, sin forwarding, sin agent
4. El usuario `emergency` puede usar password desde cualquier IP (cuenta de emergencia)
5. El usuario `backup` solo puede ejecutar `/usr/local/bin/backup.sh`

Escribe la configuración completa y explica el orden de evaluación.

<details>
<summary>Solución</summary>

```bash
# /etc/ssh/sshd_config.d/30-network-policy.conf

# === Defaults globales (aplican a todos primero) ===
PasswordAuthentication no
AllowTcpForwarding no
AllowAgentForwarding no
X11Forwarding no

# === Match blocks (se evalúan en orden, se acumulan) ===

# Desde red de oficina: permitir passwords
Match Address 192.168.1.0/24
    PasswordAuthentication yes

# Desde red de servidores: forwarding habilitado
Match Address 10.0.0.0/8
    AllowTcpForwarding yes
    AllowAgentForwarding yes

# Cuenta de emergencia: password desde cualquier IP
Match User emergency
    PasswordAuthentication yes

# Cuenta de backup: solo el script
Match User backup
    ForceCommand /usr/local/bin/backup.sh
    PasswordAuthentication no
    AllowTcpForwarding no
```

**Orden de evaluación**: los globales se aplican primero. Luego cada Match se evalúa en orden. Si una conexión coincide con múltiples Match, **todos se aplican** (se acumulan). Por ejemplo, `emergency` conectando desde 192.168.1.50 recibe: `PasswordAuthentication yes` tanto del Match Address como del Match User — sin conflicto en este caso.

Si `backup` se conecta desde 10.0.0.0/8, el Match Address habilita forwarding, pero el Match User lo deshabilita. Como el Match User viene después, su `AllowTcpForwarding no` sobrescribe el `yes` del Match Address.

Verificar para cada caso:

```bash
sshd -T -C user=emergency,addr=203.0.113.5
sshd -T -C user=admin,addr=192.168.1.100
sshd -T -C user=backup,addr=10.0.0.5
sshd -T -C user=admin,addr=203.0.113.5
```

</details>

---

### Pregunta de reflexión

> Tienes un servidor SSH en Fedora con `Include /etc/ssh/sshd_config.d/*.conf` al **principio** de `sshd_config`. Has editado `sshd_config` para poner `PermitRootLogin no`, pero root sigue pudiendo hacer login con clave pública. ¿Dónde está el problema y cómo lo diagnosticas?

> **Razonamiento esperado**: como `Include` está al principio, los archivos en `sshd_config.d/` se procesan **antes** que el cuerpo de `sshd_config`. Si algún drop-in (por ejemplo, `50-redhat.conf`) contiene `PermitRootLogin prohibit-password`, esa directiva gana porque es la primera ocurrencia. Tu `PermitRootLogin no` en el archivo principal se ignora. Diagnóstico: `sshd -T | grep permitrootlogin` muestra el valor efectivo, y `grep -r PermitRootLogin /etc/ssh/sshd_config.d/` revela el drop-in conflictivo. Solución: crear un drop-in con prefijo numérico menor (ej. `10-security.conf`) para que se procese antes que `50-redhat.conf`, o editar directamente el drop-in existente.

---

> **Siguiente tema**: T02 — Autenticación por clave (ssh-keygen, authorized_keys, ssh-agent)
