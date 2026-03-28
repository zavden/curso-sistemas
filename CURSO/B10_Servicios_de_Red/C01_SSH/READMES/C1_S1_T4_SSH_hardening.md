# SSH Hardening

## Índice

1. [Superficie de ataque de SSH](#superficie-de-ataque-de-ssh)
2. [Configuración de cifrados](#configuración-de-cifrados)
3. [Rate limiting con sshd_config](#rate-limiting-con-sshd_config)
4. [fail2ban](#fail2ban)
5. [Rate limiting con firewall](#rate-limiting-con-firewall)
6. [Protección por listas de acceso](#protección-por-listas-de-acceso)
7. [Auditoría y logging](#auditoría-y-logging)
8. [Hardening del sistema operativo](#hardening-del-sistema-operativo)
9. [Checklist de hardening](#checklist-de-hardening)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## Superficie de ataque de SSH

SSH es, en la mayoría de servidores Linux, el único servicio expuesto a Internet. Esto lo convierte en el blanco principal de ataques:

```
    Internet
       │
       │  Ataques típicos:
       │
       │  ┌─────────────────────────────────────┐
       │  │ 1. Brute force de passwords         │
       │  │ 2. Credential stuffing              │
       │  │ 3. Escaneo de versión (banner grab) │
       │  │ 4. Explotación de vulnerabilidades  │
       │  │ 5. Enumeración de usuarios          │
       │  │ 6. Man-in-the-middle                │
       │  └─────────────────────────────────────┘
       │
       ▼
    ┌──────────┐
    │ sshd :22 │
    └──────────┘
```

Un servidor SSH expuesto a Internet recibe miles de intentos de brute force **al día**. Esto no es una exageración — puedes comprobarlo:

```bash
# Ver intentos fallidos de SSH (últimas 24h)
sudo journalctl -u sshd --since "24 hours ago" | grep -c "Failed password"

# Ver las IPs más frecuentes
sudo journalctl -u sshd --since "24 hours ago" | grep "Failed password" | \
    grep -oP 'from \K[0-9.]+' | sort | uniq -c | sort -rn | head -20
```

La estrategia de hardening tiene múltiples capas:

```
    Capa 1: Autenticación
    ├── Deshabilitar passwords → solo claves/certificados
    ├── MFA (segundo factor)
    └── Limitar usuarios (AllowUsers/AllowGroups)

    Capa 2: Cifrado
    ├── Algoritmos modernos solamente
    ├── Eliminar claves de host débiles
    └── Renovar moduli Diffie-Hellman

    Capa 3: Rate limiting
    ├── MaxStartups, MaxAuthTries (sshd)
    ├── fail2ban (bans temporales)
    └── Firewall (iptables/nftables rate limit)

    Capa 4: Exposición
    ├── Solo escuchar en IPs necesarias
    ├── Firewall (restringir origen)
    └── Port knocking / SPA (opcional)

    Capa 5: Auditoría
    ├── Logging detallado
    ├── Monitorización de sesiones
    └── Alertas
```

---

## Configuración de cifrados

### Anatomía de una conexión SSH

Una conexión SSH negocia cuatro componentes criptográficos:

```
    1. Key Exchange (KEX)      → acuerdo de clave de sesión
    2. Host Key Algorithm      → firma del servidor (identidad)
    3. Cipher                  → cifrado simétrico del canal
    4. MAC                     → integridad de los mensajes
```

```
    Cliente                              Servidor
       │                                    │
       │──── "Soporto: KEX A,B,C" ────────►│
       │     "Ciphers: X,Y,Z"              │
       │     "MACs: M,N"                    │
       │     "HostKeys: H,I"               │
       │                                    │
       │◄─── "Elijo: KEX A" ──────────────│
       │     "Cipher X, MAC M"             │
       │     "HostKey H"                   │
       │                                    │
       │════ Canal cifrado establecido ════│
```

El servidor y el cliente eligen el **primer algoritmo en común** de sus listas. El orden importa: el que lista primero tiene preferencia.

### Ver los algoritmos actuales

```bash
# Ver qué ofrece tu servidor SSH
sshd -T | grep -E "^(kex|cipher|mac|hostkeyalgorithm)"

# Ver qué negoció una conexión específica
ssh -vv usuario@servidor 2>&1 | grep -E "kex:|cipher:|mac:|host key:"
# kex: curve25519-sha256
# cipher: chacha20-poly1305@openssh.com
# mac: <implicit>     ← los AEAD ciphers no necesitan MAC separado
# host key: ssh-ed25519
```

### Configuración recomendada

```bash
# /etc/ssh/sshd_config.d/10-crypto.conf

# Key Exchange: solo algoritmos basados en curvas elípticas
KexAlgorithms sntrup761x25519-sha512@openssh.com,curve25519-sha256,curve25519-sha256@libssh.org

# Claves de host: Ed25519 primero, luego ECDSA
HostKeyAlgorithms ssh-ed25519,ecdsa-sha2-nistp521,ecdsa-sha2-nistp384,ecdsa-sha2-nistp256

# Cifrados: solo AEAD (cifrado + autenticación integrados)
Ciphers chacha20-poly1305@openssh.com,aes256-gcm@openssh.com,aes128-gcm@openssh.com

# MACs: solo los necesarios para cifrados no-AEAD (si los incluyes)
MACs hmac-sha2-512-etm@openssh.com,hmac-sha2-256-etm@openssh.com
```

### Explicación de cada elección

**Key Exchange**:

| Algoritmo | Por qué |
|---|---|
| `sntrup761x25519-sha512` | Post-cuántico + Curve25519 (hybrid). Resistente a computación cuántica |
| `curve25519-sha256` | Estándar moderno. Rápido, seguro, resistente a timing attacks |

Eliminados: `diffie-hellman-group14-sha256` y anteriores — grupos DH clásicos son más lentos y la seguridad depende del tamaño del grupo.

**Ciphers**:

| Algoritmo | Por qué |
|---|---|
| `chacha20-poly1305` | AEAD. Excelente en CPUs sin AES-NI (ARM, VPS baratos). Diseño limpio |
| `aes256-gcm` / `aes128-gcm` | AEAD. Rápido en CPUs con AES-NI (la mayoría de x86 modernos) |

Eliminados: `aes*-ctr` y `aes*-cbc` — no son AEAD (necesitan MAC separado), CBC tuvo vulnerabilidades históricas.

> **AEAD** (Authenticated Encryption with Associated Data): cifra Y autentica en una sola operación. No se puede manipular el ciphertext sin que se detecte. Los modos no-AEAD (CTR, CBC) requieren un MAC separado, lo que abre la puerta a ataques de encrypt-then-MAC ordering.

**MACs**:

| Algoritmo | Por qué |
|---|---|
| `hmac-sha2-512-etm` | ETM (Encrypt-then-MAC): más seguro que MAC-then-encrypt |
| `hmac-sha2-256-etm` | ETM con SHA-256 |

Solo necesarios si incluyes cifrados no-AEAD. Con solo `chacha20-poly1305` y `aes*-gcm`, los MACs no se usan (implicit).

### Renovar parámetros Diffie-Hellman

El archivo `/etc/ssh/moduli` contiene parámetros DH precomputados. Regenerarlos elimina la posibilidad de usar parámetros conocidos:

```bash
# 1. Generar candidatos (tarda minutos a horas según el hardware)
ssh-keygen -M generate -O bits=3072 /tmp/moduli.candidates

# 2. Filtrar candidatos seguros
ssh-keygen -M screen -f /tmp/moduli.candidates /tmp/moduli.safe

# 3. Reemplazar
sudo cp /etc/ssh/moduli /etc/ssh/moduli.backup
sudo mv /tmp/moduli.safe /etc/ssh/moduli

# Alternativa más rápida: eliminar entradas con grupos pequeños
sudo awk '$5 >= 3071' /etc/ssh/moduli > /tmp/moduli.filtered
sudo mv /tmp/moduli.filtered /etc/ssh/moduli
```

### Eliminar claves de host débiles

```bash
# Eliminar RSA (si no necesitas compatibilidad legacy)
sudo rm /etc/ssh/ssh_host_rsa_key /etc/ssh/ssh_host_rsa_key.pub

# Eliminar DSA (siempre — obsoleto)
sudo rm /etc/ssh/ssh_host_dsa_key /etc/ssh/ssh_host_dsa_key.pub 2>/dev/null

# Especificar solo las claves que ofrece el servidor
# En sshd_config:
HostKey /etc/ssh/ssh_host_ed25519_key
HostKey /etc/ssh/ssh_host_ecdsa_key
# No listar RSA ni DSA
```

### Verificar la configuración

```bash
# Conectar con verbosidad para ver qué se negocia
ssh -vv usuario@servidor 2>&1 | grep -A1 "kex_input_kexinit"

# Escanear con ssh-audit (herramienta especializada)
# Instalar: pip install ssh-audit
ssh-audit servidor
# Muestra en verde/amarillo/rojo cada algoritmo
# y da recomendaciones específicas
```

`ssh-audit` es la herramienta de referencia para evaluar la configuración criptográfica de un servidor SSH. Da un informe detallado con calificación por algoritmo.

---

## Rate limiting con sshd_config

### MaxStartups

Controla las conexiones no autenticadas simultáneas:

```bash
# Formato: start:rate:full
MaxStartups 10:30:60
```

```
    Conexiones no autenticadas simultáneas:

    0              10              35              60
    |──────────────|───────────────|───────────────|
    Aceptar todas   30% de rechazo   creciente    100%
                    empieza aquí     linealmente   rechazo

    Con 10 conexiones pendientes: empieza a rechazar al 30%
    Con 35 conexiones pendientes: rechaza al ~65%
    Con 60 conexiones pendientes: rechaza al 100%
```

```bash
# Para un servidor con pocos usuarios:
MaxStartups 3:50:10
# Empieza a rechazar al 50% con 3 pendientes, 100% con 10

# Para un servidor con muchos usuarios legítimos (ej. GitLab):
MaxStartups 50:30:200
```

### MaxAuthTries

```bash
# Intentos de autenticación por conexión (default: 6)
MaxAuthTries 3
# Un bot que prueba passwords tiene 3 intentos antes de que SSH cierre
# la conexión. Con 6 (default) tiene más oportunidades.

# Nota: si el usuario tiene muchas claves en ssh-agent,
# cada clave probada cuenta como un intento.
# Solución del cliente: IdentitiesOnly yes en ssh_config
```

### LoginGraceTime

```bash
# Tiempo máximo para completar la autenticación (default: 120s)
LoginGraceTime 30
# Si no te autentificas en 30 segundos, desconexión.
# Los bots a veces mantienen conexiones abiertas para agotar MaxStartups.
```

---

## fail2ban

### Concepto

fail2ban monitoriza logs del sistema y bloquea IPs que muestran comportamiento malicioso:

```
    ┌──────────────────┐
    │   /var/log/       │
    │   auth.log        │─────┐
    │   secure          │     │
    └──────────────────┘     │
                              ▼
                    ┌──────────────────┐
                    │    fail2ban      │
                    │                  │
                    │  "3 fallos en    │
                    │   5 min desde    │
                    │   203.0.113.5"   │
                    │                  │
                    │  → BAN IP        │
                    └────────┬─────────┘
                             │
                             ▼
                    ┌──────────────────┐
                    │   iptables /     │
                    │   nftables       │
                    │                  │
                    │ DROP 203.0.113.5 │
                    └──────────────────┘
```

### Instalación

```bash
# Debian/Ubuntu
sudo apt install fail2ban

# Fedora/RHEL
sudo dnf install fail2ban

# Habilitar e iniciar
sudo systemctl enable --now fail2ban
```

### Configuración

fail2ban usa archivos en capas — **nunca edites los archivos base**, usa los `.local`:

```
    /etc/fail2ban/
    ├── fail2ban.conf        ← config global (NO editar)
    ├── fail2ban.local       ← override global (crear si necesario)
    ├── jail.conf            ← jails predefinidas (NO editar)
    ├── jail.local           ← TU configuración de jails
    ├── jail.d/              ← drop-in configs
    │   └── custom.conf
    ├── filter.d/            ← patrones de detección
    │   ├── sshd.conf        ← filtro para SSH
    │   └── ...
    └── action.d/            ← acciones de ban
        ├── iptables.conf
        ├── nftables.conf
        └── ...
```

### Configuración de SSH jail

```ini
# /etc/fail2ban/jail.local

[DEFAULT]
# IP/redes que NUNCA se bloquean
ignoreip = 127.0.0.1/8 ::1 192.168.1.0/24 10.0.0.0/8

# Duración del ban (segundos o sufijos: m, h, d)
bantime = 1h

# Ventana de tiempo para contar fallos
findtime = 10m

# Número de fallos antes del ban
maxretry = 3

# Backend para leer logs
backend = systemd    # para distros con systemd/journald
# backend = auto     # detección automática

# Acción de ban: iptables, nftables, o firewalld
banaction = nftables-multiport    # Fedora/RHEL moderno
# banaction = iptables-multiport  # Debian/Ubuntu
# banaction = firewallcmd-ipset   # si usas firewalld

[sshd]
enabled = true
port = ssh,22       # o el puerto que uses
filter = sshd
maxretry = 3
findtime = 10m
bantime = 1h

# Ban progresivo: más intentos = ban más largo
# (requiere fail2ban 0.11+)
bantime.increment = true
bantime.factor = 2
bantime.maxtime = 1w
# 1er ban: 1h, 2do: 2h, 3ro: 4h, ... máximo 1 semana
```

### Ban progresivo

fail2ban 0.11+ soporta escalamiento del ban para reincidentes:

```
    Intento 1:  ban 1h
    Intento 2:  ban 2h     (factor × 2)
    Intento 3:  ban 4h
    Intento 4:  ban 8h
    Intento 5:  ban 16h
    Intento 6:  ban 32h
    Intento 7+: ban 1w     (maxtime)
```

### Operaciones de fail2ban

```bash
# Estado general
sudo fail2ban-client status
# Status
# |- Number of jail:      1
# `- Jail list:           sshd

# Estado de la jail SSH
sudo fail2ban-client status sshd
# Status for the jail: sshd
# |- Filter
# |  |- Currently failed: 2
# |  |- Total failed:     847
# |  `- Journal matches:  _SYSTEMD_UNIT=sshd.service + _COMM=sshd
# `- Actions
#    |- Currently banned: 5
#    |- Total banned:     123
#    `- Banned IP list:   203.0.113.5 198.51.100.10 ...

# Desbloquear una IP manualmente
sudo fail2ban-client set sshd unbanip 203.0.113.5

# Bloquear una IP manualmente
sudo fail2ban-client set sshd banip 203.0.113.5

# Verificar si una IP está baneada
sudo fail2ban-client get sshd banned 203.0.113.5

# Recargar configuración
sudo fail2ban-client reload

# Ver el regex del filtro SSH
sudo fail2ban-client get sshd failregex

# Testear un filtro contra un log
sudo fail2ban-regex /var/log/auth.log /etc/fail2ban/filter.d/sshd.conf
```

### Filtro SSH — qué detecta

El filtro `sshd.conf` detecta varios patrones de ataque:

```bash
# Ver los patrones
cat /etc/fail2ban/filter.d/sshd.conf | grep "^failregex" -A 50

# Patrones principales que detecta:
# - "Failed password for * from <HOST>"
# - "Failed password for invalid user * from <HOST>"
# - "Authentication failure for * from <HOST>"
# - "Connection closed by authenticating user * <HOST> * [preauth]"
# - "maximum authentication attempts exceeded for * from <HOST>"
# - "Unable to negotiate with <HOST>: no matching key exchange method"
```

### Acciones personalizadas

```ini
# /etc/fail2ban/jail.local

[sshd]
enabled = true
# Acción: bloquear + enviar email al admin
action = %(action_mwl)s
# action_: solo ban
# action_mw: ban + email con whois
# action_mwl: ban + email con whois + log lines

# O definir la acción explícitamente:
action = nftables-multiport[name=SSH, port="ssh", protocol=tcp]
         sendmail-whois[name=SSH, dest=admin@ejemplo.com]
```

---

## Rate limiting con firewall

### iptables — limit module

```bash
# Limitar conexiones SSH nuevas: máximo 3 por minuto desde cada IP
sudo iptables -A INPUT -p tcp --dport 22 -m state --state NEW \
    -m recent --name ssh --set
sudo iptables -A INPUT -p tcp --dport 22 -m state --state NEW \
    -m recent --name ssh --update --seconds 60 --hitcount 4 \
    -j DROP

# Alternativa con hashlimit (más flexible):
sudo iptables -A INPUT -p tcp --dport 22 -m state --state NEW \
    -m hashlimit --hashlimit-name ssh \
    --hashlimit-above 3/minute \
    --hashlimit-mode srcip \
    --hashlimit-burst 3 \
    -j DROP
```

### nftables

```bash
# En /etc/nftables.conf
table inet ssh_ratelimit {
    set ssh_meter {
        type ipv4_addr
        flags dynamic
        timeout 1m
    }

    chain input {
        type filter hook input priority filter; policy accept;

        tcp dport 22 ct state new \
            add @ssh_meter { ip saddr limit rate 3/minute burst 3 packets } \
            accept

        tcp dport 22 ct state new drop
    }
}
```

### firewalld — rich rules

```bash
# Limitar conexiones SSH nuevas a 3 por minuto
sudo firewall-cmd --permanent --add-rich-rule='
    rule family="ipv4"
    service name="ssh"
    accept
    limit value="3/m"'
sudo firewall-cmd --reload
```

### Comparativa: fail2ban vs firewall rate limit

| Aspecto | fail2ban | Firewall rate limit |
|---|---|---|
| Detección | Basada en logs (post-hoc) | Basada en paquetes (tiempo real) |
| Granularidad | Detecta passwords incorrectos | Cuenta conexiones (sin importar resultado) |
| Ban | Ban temporal (configurable) | Rate limit continuo |
| Reincidentes | Ban progresivo | Mismo límite siempre |
| Overhead | Lee logs constantemente | Mínimo (kernel) |
| Falsos positivos | Menor (solo passwords fallidos) | Mayor (limita también usuarios legítimos) |
| **Recomendación** | **Usar siempre** | **Complemento a fail2ban** |

La combinación ideal es: **firewall rate limit** como primera capa (reduce ruido) + **fail2ban** como segunda capa (detecta ataques reales basándose en fallos de autenticación).

---

## Protección por listas de acceso

### Firewall — restringir origen

```bash
# Solo permitir SSH desde la red de oficina
sudo firewall-cmd --permanent --remove-service=ssh
sudo firewall-cmd --permanent --add-rich-rule='
    rule family="ipv4"
    source address="192.168.1.0/24"
    service name="ssh"
    accept'
sudo firewall-cmd --reload

# nftables equivalente
nft add rule inet filter input ip saddr 192.168.1.0/24 tcp dport 22 accept
nft add rule inet filter input tcp dport 22 drop
```

### TCP Wrappers (/etc/hosts.allow, /etc/hosts.deny)

Mecanismo legacy pero aún funcional en algunos sistemas:

```bash
# /etc/hosts.allow
sshd: 192.168.1.0/24, 10.0.0.0/8

# /etc/hosts.deny
sshd: ALL
```

> **Nota**: TCP Wrappers está deprecado y eliminado en muchas distribuciones modernas. Fedora/RHEL 8+ ya no lo soportan por defecto. Usar firewall o `AllowUsers`/`Match Address` en sshd_config.

### AllowUsers con restricción de IP

```bash
# En sshd_config:
AllowUsers admin@192.168.1.0/24 deploy@10.0.0.100

# O con Match:
Match Address *,!192.168.1.0/24,!10.0.0.0/8
    DenyUsers *
    # Nadie puede conectar desde fuera de estas redes
```

### Port knocking (concepto)

Port knocking oculta SSH hasta que el cliente "toca" una secuencia de puertos:

```
    Estado normal: puerto 22 CERRADO (invisible a escaneos)

    Cliente toca: 7000, 8000, 9000 (secuencia secreta)

    Servidor detecta la secuencia → abre puerto 22 para esa IP por 30s

    Cliente conecta SSH normalmente
```

```bash
# Con knockd:
# /etc/knockd.conf
[options]
    logfile = /var/log/knockd.log

[openSSH]
    sequence    = 7000,8000,9000
    seq_timeout = 5
    command     = /sbin/iptables -I INPUT -s %IP% -p tcp --dport 22 -j ACCEPT
    tcpflags    = syn

[closeSSH]
    sequence    = 9000,8000,7000
    seq_timeout = 5
    command     = /sbin/iptables -D INPUT -s %IP% -p tcp --dport 22 -j ACCEPT
    tcpflags    = syn
```

```bash
# Cliente:
knock servidor 7000 8000 9000
ssh admin@servidor
# Al terminar:
knock servidor 9000 8000 7000
```

> Port knocking es "security through obscurity" — un complemento, no un sustituto. Un atacante que monitorice tu tráfico puede ver la secuencia. **fwknop** (Single Packet Authorization) es una alternativa más robusta que usa un paquete cifrado en lugar de una secuencia de puertos.

---

## Auditoría y logging

### Nivel de log

```bash
# En sshd_config:
LogLevel VERBOSE
# Niveles: QUIET, FATAL, ERROR, INFO (default), VERBOSE, DEBUG, DEBUG1-3

# VERBOSE registra:
# - Método de autenticación usado
# - Fingerprint de la clave usada
# - Comandos ejecutados (con certificados)
```

Diferencia entre INFO y VERBOSE:

```bash
# INFO (default):
# Accepted publickey for admin from 192.168.1.100 port 52345 ssh2

# VERBOSE:
# Accepted publickey for admin from 192.168.1.100 port 52345 ssh2:
#   ED25519 SHA256:xKz7nGb5sIh4H...
# ↑ incluye el fingerprint de la clave usada
# Esto permite saber CUÁL clave se usó para autenticarse
```

### Consultar logs

```bash
# systemd/journald (moderno)
sudo journalctl -u sshd                    # todo el log SSH
sudo journalctl -u sshd --since "1h ago"   # última hora
sudo journalctl -u sshd -f                 # tiempo real
sudo journalctl -u sshd -p err             # solo errores

# Buscar intentos fallidos
sudo journalctl -u sshd | grep "Failed password"

# Buscar logins exitosos
sudo journalctl -u sshd | grep "Accepted"

# Buscar desconexiones por auth failure
sudo journalctl -u sshd | grep "Connection closed.*preauth"

# Archivos de log tradicionales (si no usas journald)
# Debian/Ubuntu: /var/log/auth.log
# RHEL/Fedora:   /var/log/secure
```

### Auditar claves SSH con LogLevel VERBOSE

Con `LogLevel VERBOSE`, puedes rastrear qué clave se usó en cada login:

```bash
# En el log:
# Accepted publickey for admin from 10.0.0.5 port 43210 ssh2:
#   ED25519 SHA256:xKz7nGb5sIh4H...

# Comparar con las claves en authorized_keys:
ssh-keygen -lf /home/admin/.ssh/authorized_keys
# 256 SHA256:xKz7nGb5sIh4H... admin@laptop (ED25519)
# 256 SHA256:yHa8pMc4jR2nK... ci-deploy (ED25519)

# Ahora sabes: el login de las 14:30 fue con la clave "admin@laptop"
```

### Registrar comandos ejecutados en sesiones SSH

```bash
# Opción 1: pam_tty_audit (registra todo lo tecleado)
# /etc/pam.d/sshd:
session required pam_tty_audit.so enable=*

# Opción 2: auditd rules
sudo auditctl -a always,exit -F arch=b64 -S execve -F auid!=4294967295 -k ssh-commands

# Ver los comandos registrados:
sudo ausearch -k ssh-commands --interpret

# Opción 3: HISTFILE para bash (más simple, menos fiable)
# En /etc/profile.d/ssh-audit.sh:
readonly HISTFILE
readonly HISTSIZE=10000
readonly HISTTIMEFORMAT="%Y-%m-%d %H:%M:%S "
export PROMPT_COMMAND='history -a'
```

---

## Hardening del sistema operativo

### Banner sin información

```bash
# Eliminar la versión del OS del banner SSH
# El banner por defecto muestra: SSH-2.0-OpenSSH_9.6p1 Ubuntu-3ubuntu
# Esto dice al atacante: OpenSSH 9.6 en Ubuntu

# No puedes cambiar la versión de SSH (está en el protocolo),
# pero puedes controlar el banner pre-auth:

# Banner minimalista (aviso legal):
cat > /etc/ssh/banner.txt << 'EOF'
Authorized access only. All activity is logged and monitored.
EOF

# En sshd_config:
Banner /etc/ssh/banner.txt

# Quitar el debian-banner que añade info del OS:
# En Debian/Ubuntu, sshd muestra "Ubuntu" o "Debian" tras la versión
# Compilar sin: no es práctico. Mejor enfocarse en otras capas.
```

### Deshabilitar servicios innecesarios de SSH

```bash
# En sshd_config:

# Deshabilitar si no se necesita
X11Forwarding no
AllowTcpForwarding no          # si no necesitas túneles
AllowAgentForwarding no        # si no necesitas agent forwarding
PermitTunnel no                # si no necesitas VPN sobre SSH
AllowStreamLocalForwarding no  # si no necesitas UNIX socket forwarding

# Deshabilitar features legacy
PermitUserEnvironment no       # usuarios no pueden setear env vars
PermitUserRC no                # no ejecutar ~/.ssh/rc

# Compression (reducir superficie de ataque de zlib)
Compression no                 # o "delayed" (solo post-auth)
```

### Separation de privilegios

```bash
# Verificar que está activo (default desde OpenSSH 7.5)
sshd -T | grep "useprivsep"
# useprivsep sandbox    ← máxima seguridad

# Niveles:
# no       — sin separación (inseguro, nunca usar)
# yes      — separación básica
# sandbox  — separación + seccomp sandbox (default moderno)
```

### Restringir usuarios del sistema

```bash
# Crear un grupo para usuarios SSH
sudo groupadd sshusers

# Añadir usuarios autorizados
sudo usermod -aG sshusers admin
sudo usermod -aG sshusers deploy

# En sshd_config:
AllowGroups sshusers

# Esto bloquea automáticamente:
# - root
# - www-data, nginx, mysql, postgres...
# - cualquier cuenta de servicio
# - usuarios sin el grupo sshusers
```

---

## Checklist de hardening

### Nivel básico (mínimo para cualquier servidor expuesto)

```bash
# /etc/ssh/sshd_config.d/10-basic-hardening.conf

# 1. Sin passwords
PasswordAuthentication no
PermitEmptyPasswords no

# 2. Sin root
PermitRootLogin no

# 3. Solo usuarios autorizados
AllowGroups sshusers

# 4. Limitar intentos
MaxAuthTries 3
LoginGraceTime 30

# 5. Timeout de inactividad
ClientAliveInterval 300
ClientAliveCountMax 2
```

### Nivel intermedio (servidores de producción)

```bash
# /etc/ssh/sshd_config.d/10-production-hardening.conf

# Todo lo básico +

# 6. Cifrados modernos
KexAlgorithms sntrup761x25519-sha512@openssh.com,curve25519-sha256,curve25519-sha256@libssh.org
Ciphers chacha20-poly1305@openssh.com,aes256-gcm@openssh.com,aes128-gcm@openssh.com
MACs hmac-sha2-512-etm@openssh.com,hmac-sha2-256-etm@openssh.com
HostKeyAlgorithms ssh-ed25519

# 7. Solo claves de host modernas
HostKey /etc/ssh/ssh_host_ed25519_key

# 8. Logging detallado
LogLevel VERBOSE

# 9. Reducir superficie
X11Forwarding no
AllowAgentForwarding no
PermitTunnel no
PermitUserEnvironment no
PermitUserRC no
Compression no

# 10. Rate limiting
MaxStartups 3:50:10
```

### Nivel avanzado (alta seguridad)

```bash
# Todo lo intermedio +

# 11. MFA
AuthenticationMethods publickey,keyboard-interactive

# 12. Solo desde IPs conocidas
Match Address *,!10.0.0.0/8,!192.168.0.0/16
    DenyUsers *

# 13. Restricciones por usuario
Match User deploy
    AllowTcpForwarding no
    ForceCommand /usr/local/bin/deploy-wrapper.sh

Match Group sftponly
    ChrootDirectory /srv/sftp/%u
    ForceCommand internal-sftp
    PermitTTY no
```

### Verificación post-hardening

```bash
# 1. Validar configuración
sudo sshd -t

# 2. Verificar que puedes conectar (desde otra terminal!)
ssh -v usuario@servidor

# 3. Verificar que passwords están deshabilitados
ssh -o PreferredAuthentications=password -o PubkeyAuthentication=no usuario@servidor
# Debe decir: Permission denied (publickey).

# 4. Verificar que root no puede conectar
ssh root@servidor
# Debe decir: Permission denied (publickey).

# 5. Auditar con ssh-audit
ssh-audit servidor

# 6. Verificar que fail2ban está activo
sudo fail2ban-client status sshd

# 7. Verificar logging
sudo journalctl -u sshd --since "5 min ago"
# Debe mostrar tu conexión de prueba con fingerprint (VERBOSE)
```

---

## Errores comunes

### 1. Habilitar hardening sin verificar acceso alternativo

```bash
# Escenario desastroso:
# 1. Deshabilitas PasswordAuthentication
# 2. Tu clave pública NO está en authorized_keys (o permisos mal)
# 3. Haces systemctl reload sshd
# 4. Tu sesión actual funciona, pero no puedes abrir nuevas
# 5. Si cierras la sesión... bloqueado.

# SIEMPRE:
# a) Tener una segunda sesión abierta como respaldo
# b) Tener acceso por consola (KVM, IPMI, panel del VPS)
# c) Verificar la clave ANTES de deshabilitar passwords:
ssh -o PreferredAuthentications=publickey usuario@servidor
# Si esto funciona, es seguro deshabilitar passwords
```

### 2. fail2ban que bloquea tu propia IP

```bash
# Intentas conectar varias veces con clave incorrecta → baneado

# Prevención: incluir tu IP en ignoreip
# /etc/fail2ban/jail.local:
[DEFAULT]
ignoreip = 127.0.0.1/8 ::1 TU_IP_FIJA/32 TU_RED/24

# Recuperación: acceder por consola o VPN y desbloquear
sudo fail2ban-client set sshd unbanip TU_IP
```

### 3. Cifrados demasiado restrictivos que rompen compatibilidad

```bash
# Configuras solo Ed25519 + chacha20-poly1305
# Un colega con PuTTY antiguo no puede conectar

# Diagnóstico:
ssh -v usuario@servidor 2>&1 | grep "no matching"
# "no matching cipher found" o "no matching key exchange method"

# Solución temporal: añadir un cifrado compatible
# Solución correcta: actualizar el cliente
# Compromiso: usar Match para permitir cifrados legacy desde IPs conocidas
Match Address 192.168.1.50
    Ciphers chacha20-poly1305@openssh.com,aes256-gcm@openssh.com,aes256-ctr
```

### 4. LogLevel DEBUG en producción

```bash
# DEBUG registra DEMASIADA información:
# - Contenido de variables de sesión
# - Detalles de negociación criptográfica por cada conexión
# - Potencial información sensible
# - Logs enormes que llenan el disco

# Niveles apropiados:
# Producción: VERBOSE (fingerprints, suficiente para auditoría)
# Diagnóstico temporal: DEBUG1 (cambiar de vuelta a VERBOSE después)
# Nunca en producción: DEBUG2, DEBUG3
```

### 5. Confiar solo en cambiar el puerto

```bash
# "Moví SSH al puerto 2222, estoy seguro"
# Falso. Un nmap con -p- o --top-ports 10000 lo encuentra en segundos:
# nmap -sV -p 2222 servidor
# 2222/tcp open ssh OpenSSH 9.6

# Cambiar el puerto reduce ruido de bots automáticos (útil),
# pero NO es una medida de seguridad. No sustituye:
# - Deshabilitar passwords
# - fail2ban
# - Cifrados modernos
# - AllowGroups
```

---

## Cheatsheet

```bash
# === CIFRADOS ===
sshd -T | grep -E "^(kex|cipher|mac|hostkeyalg)"   # ver config actual
ssh -vv user@srv 2>&1 | grep "kex:"                 # ver negociación
ssh-audit servidor                                    # auditoría completa

# === FAIL2BAN ===
sudo systemctl enable --now fail2ban          # activar
sudo fail2ban-client status                   # estado general
sudo fail2ban-client status sshd              # estado SSH jail
sudo fail2ban-client set sshd unbanip IP      # desbloquear IP
sudo fail2ban-client set sshd banip IP        # bloquear IP
sudo fail2ban-client reload                   # recargar config
sudo fail2ban-regex /var/log/auth.log \
    /etc/fail2ban/filter.d/sshd.conf          # testear filtro

# === RATE LIMITING FIREWALL ===
# nftables:
# add @meter { ip saddr limit rate 3/minute burst 3 } accept
# firewalld:
# --add-rich-rule 'rule service name="ssh" accept limit value="3/m"'

# === LOGS ===
sudo journalctl -u sshd -f                   # tiempo real
sudo journalctl -u sshd --since "1h ago"     # última hora
sudo journalctl -u sshd | grep "Failed"      # intentos fallidos
sudo journalctl -u sshd | grep "Accepted"    # logins exitosos

# === VERIFICACIÓN ===
sudo sshd -t                                  # validar config
sshd -T                                       # config efectiva
sshd -T -C user=X,addr=Y                     # config por conexión

# === CHECKLIST RÁPIDO ===
# PasswordAuthentication no
# PermitRootLogin no
# AllowGroups sshusers
# MaxAuthTries 3
# LoginGraceTime 30
# ClientAliveInterval 300
# LogLevel VERBOSE
# Ciphers modernos (AEAD)
# fail2ban activo
# Firewall limitando conexiones

# === CLAVES DE HOST ===
sudo rm /etc/ssh/ssh_host_dsa_key*            # eliminar DSA (obsoleto)
sudo ssh-keygen -A                            # regenerar todas
# HostKey /etc/ssh/ssh_host_ed25519_key       # solo Ed25519

# === DH MODULI ===
sudo awk '$5 >= 3071' /etc/ssh/moduli > /tmp/m && sudo mv /tmp/m /etc/ssh/moduli
```

---

## Ejercicios

### Ejercicio 1 — Auditoría de configuración SSH

1. Examina la configuración SSH actual de tu sistema con `sshd -T`
2. Identifica al menos 5 directivas que están en valores inseguros o sub-óptimos respecto al checklist de hardening
3. Crea un archivo drop-in `/etc/ssh/sshd_config.d/10-hardening.conf` con las correcciones
4. Valida con `sshd -t`, recarga el servicio y verifica con `sshd -T` que los valores cambiaron
5. Verifica que puedes conectar con tu clave y que no puedes conectar con password

<details>
<summary>Pista</summary>

Usa `sshd -T | grep -iE "passwordauth|permitroot|maxauth|loglevel|x11|compression|allowtcp"` para revisar rápidamente las directivas clave.

</details>

<details>
<summary>Solución</summary>

```bash
# 1. Examinar configuración
sshd -T | grep -iE "passwordauth|permitroot|maxauth|logingraceTime|clientalive|loglevel|x11|compression|allowtcp|kex|cipher|mac"

# Valores típicos inseguros por defecto:
# passwordauthentication yes    → debe ser no
# permitrootlogin prohibit-password → debe ser no
# maxauthtries 6                → debe ser 3
# loglevel INFO                 → debe ser VERBOSE
# x11forwarding yes             → debe ser no
# compression delayed           → debe ser no

# 3. Crear correcciones
sudo tee /etc/ssh/sshd_config.d/10-hardening.conf << 'EOF'
PasswordAuthentication no
PermitRootLogin no
MaxAuthTries 3
LoginGraceTime 30
ClientAliveInterval 300
ClientAliveCountMax 2
LogLevel VERBOSE
X11Forwarding no
Compression no
AllowTcpForwarding no
AllowAgentForwarding no
PermitUserEnvironment no
KexAlgorithms sntrup761x25519-sha512@openssh.com,curve25519-sha256,curve25519-sha256@libssh.org
Ciphers chacha20-poly1305@openssh.com,aes256-gcm@openssh.com,aes128-gcm@openssh.com
MACs hmac-sha2-512-etm@openssh.com,hmac-sha2-256-etm@openssh.com
EOF

# 4. Validar y aplicar
sudo sshd -t && echo "OK"
# MANTENER OTRA SESIÓN ABIERTA
sudo systemctl reload sshd    # o ssh

# Verificar
sshd -T | grep -iE "passwordauth|permitroot|maxauth|loglevel"
# passwordauthentication no ✓
# permitrootlogin no ✓
# maxauthtries 3 ✓
# loglevel VERBOSE ✓

# 5. Test
ssh -o PreferredAuthentications=publickey usuario@localhost
# Debe funcionar

ssh -o PreferredAuthentications=password -o PubkeyAuthentication=no usuario@localhost
# Permission denied (publickey). ✓
```

</details>

---

### Ejercicio 2 — Instalar y configurar fail2ban

1. Instala fail2ban en tu sistema
2. Crea `/etc/fail2ban/jail.local` con:
   - SSH jail habilitado
   - maxretry: 3, findtime: 10m, bantime: 30m
   - Ban progresivo con factor 2 y máximo 24h
   - Tu red local en ignoreip
3. Inicia fail2ban y verifica que la jail SSH está activa
4. Simula 4 intentos de login fallidos (con password incorrecto si aún está habilitado, o con clave incorrecta)
5. Verifica que tu IP fue baneada y luego desbloquéala

<details>
<summary>Solución</summary>

```bash
# 1. Instalar
sudo apt install fail2ban     # Debian/Ubuntu
# sudo dnf install fail2ban   # Fedora/RHEL

# 2. Configurar
sudo tee /etc/fail2ban/jail.local << 'EOF'
[DEFAULT]
ignoreip = 127.0.0.1/8 ::1 192.168.1.0/24
backend = systemd

[sshd]
enabled = true
port = ssh
filter = sshd
maxretry = 3
findtime = 10m
bantime = 30m
bantime.increment = true
bantime.factor = 2
bantime.maxtime = 24h
EOF

# 3. Iniciar y verificar
sudo systemctl enable --now fail2ban
sudo fail2ban-client status sshd
# Status for the jail: sshd
# |- Currently banned: 0
# `- Total banned:     0

# 4. Simular fallos (desde otra IP o sin ignoreip)
# Temporalmente quitar tu IP de ignoreip para la prueba
for i in 1 2 3 4; do
    ssh -o PreferredAuthentications=password \
        -o PubkeyAuthentication=no \
        usuario@localhost 2>/dev/null
done

# 5. Verificar ban
sudo fail2ban-client status sshd
# Currently banned: 1
# Banned IP list: 127.0.0.1 (o tu IP)

# Desbloquear
sudo fail2ban-client set sshd unbanip 127.0.0.1

# Restaurar ignoreip
```

</details>

---

### Ejercicio 3 — Diagnóstico de ataques

Analiza los logs de SSH de tu sistema (o un log de ejemplo) y responde:

1. ¿Cuántos intentos fallidos hubo en las últimas 24 horas?
2. ¿Cuáles son las 10 IPs con más intentos?
3. ¿Qué nombres de usuario están probando los atacantes?
4. ¿Hay algún patrón temporal (horas pico de ataques)?
5. ¿Hay intentos con usuarios válidos del sistema?

<details>
<summary>Solución</summary>

```bash
# 1. Total de fallos
sudo journalctl -u sshd --since "24 hours ago" | grep -c "Failed password"

# 2. Top 10 IPs
sudo journalctl -u sshd --since "24 hours ago" | grep "Failed password" | \
    grep -oP 'from \K[0-9.]+' | sort | uniq -c | sort -rn | head -10

# 3. Usuarios probados
sudo journalctl -u sshd --since "24 hours ago" | grep "Failed password" | \
    grep -oP 'for (invalid user )?\K\S+' | sort | uniq -c | sort -rn | head -20
# Típicos: root, admin, test, user, oracle, postgres, ubuntu, guest

# 4. Patrón temporal (intentos por hora)
sudo journalctl -u sshd --since "24 hours ago" | grep "Failed password" | \
    grep -oP '^\w+ \d+ \K\d+' | sort | uniq -c
# Muestra intentos por hora del día

# 5. Usuarios válidos
VALID_USERS=$(awk -F: '$3 >= 1000 && $3 < 65534 {print $1}' /etc/passwd)
for user in $VALID_USERS; do
    COUNT=$(sudo journalctl -u sshd --since "24 hours ago" | \
            grep "Failed password for $user " | wc -l)
    if [ "$COUNT" -gt 0 ]; then
        echo "¡ALERTA! $user: $COUNT intentos fallidos"
    fi
done
```

Si hay intentos con usuarios válidos (especialmente si no son `root`), puede indicar un ataque dirigido — alguien conoce nombres de usuario reales del sistema.

</details>

---

### Pregunta de reflexión

> Tu servidor tiene `PasswordAuthentication no`, `PermitRootLogin no`, claves Ed25519 y fail2ban activo. Los logs muestran miles de intentos diarios de `Failed password for invalid user`. Si los passwords están deshabilitados, ¿por qué siguen apareciendo estos mensajes? ¿Debería preocuparte? ¿Deberías tomar alguna acción adicional?

> **Razonamiento esperado**: los mensajes aparecen porque sshd registra el intento **antes** de rechazarlo por tipo de autenticación. El bot envía un usuario y el servidor anota el fallo, aunque nunca llegaría a la fase de verificación de password. No es una vulnerabilidad, pero sí tiene dos costos: (1) cada intento consume recursos del servidor (fork de sshd, handshake criptográfico) y (2) los logs se llenan de ruido, dificultando encontrar eventos reales. Acciones adicionales: fail2ban ya bloquea IPs después de N intentos. Para reducir más el ruido, puedes usar rate limiting a nivel firewall para limitar conexiones nuevas al puerto 22 antes de que lleguen a sshd, o restringir SSH por IP de origen si conoces las redes legítimas. La medida más drástica pero efectiva es mover SSH detrás de una VPN (WireGuard) y no exponerlo directamente a Internet.

---

> **Siguiente tema**: T05 — ~/.ssh/config (Host aliases, ProxyJump, IdentityFile)
