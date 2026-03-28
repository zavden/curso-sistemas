# Hardening básico: servicios innecesarios y parámetros de seguridad del kernel

## Índice

1. [Qué es hardening y por qué importa](#1-qué-es-hardening-y-por-qué-importa)
2. [Reducir la superficie de ataque: servicios](#2-reducir-la-superficie-de-ataque-servicios)
3. [Reducir la superficie de ataque: paquetes y puertos](#3-reducir-la-superficie-de-ataque-paquetes-y-puertos)
4. [Parámetros de seguridad del kernel (sysctl)](#4-parámetros-de-seguridad-del-kernel-sysctl)
5. [Permisos y acceso a archivos sensibles](#5-permisos-y-acceso-a-archivos-sensibles)
6. [Banners y exposición de información](#6-banners-y-exposición-de-información)
7. [Hardening de SSH](#7-hardening-de-ssh)
8. [Opciones de montaje de seguridad](#8-opciones-de-montaje-de-seguridad)
9. [Herramientas de auditoría de hardening](#9-herramientas-de-auditoría-de-hardening)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué es hardening y por qué importa

Hardening es el proceso de **reducir la superficie de ataque** de un sistema
eliminando lo innecesario y configurando lo necesario de forma segura:

```
Principio fundamental
══════════════════════

  Un sistema recién instalado es como una casa nueva:
    → Todas las ventanas abiertas
    → Todas las puertas sin llave
    → Luces encendidas que nadie usa
    → Herramientas en el jardín que un intruso podría usar

  Hardening = cerrar ventanas, poner cerraduras, apagar luces,
              guardar herramientas.

  NO es:
    ✗ Instalar un firewall y olvidarse
    ✗ Un producto que compras
    ✗ Algo que haces una vez

  ES:
    ✓ Un proceso continuo
    ✓ Múltiples capas (defense in depth)
    ✓ Reducir lo que puede salir mal
```

### Capas de hardening

```
Capas de hardening (de exterior a interior)
═════════════════════════════════════════════

  ┌─────────────────────────────────────────────┐
  │  Red: firewall, segmentación, IDS/IPS       │
  │  ┌─────────────────────────────────────────┐ │
  │  │  Servicios: solo los necesarios         │ │
  │  │  ┌─────────────────────────────────────┐│ │
  │  │  │  SO: sysctl, permisos, montaje     ││ │
  │  │  │  ┌─────────────────────────────────┐││ │
  │  │  │  │  MAC: SELinux / AppArmor       │││ │
  │  │  │  │  ┌─────────────────────────────┐│││ │
  │  │  │  │  │  Aplicación: config segura ││││ │
  │  │  │  │  │  ┌─────────────────────────┐││││ │
  │  │  │  │  │  │  Datos: cifrado, backup │││││ │
  │  │  │  │  │  └─────────────────────────┘││││ │
  │  │  │  │  └─────────────────────────────┘│││ │
  │  │  │  └─────────────────────────────────┘││ │
  │  │  └─────────────────────────────────────┘│ │
  │  └─────────────────────────────────────────┘ │
  └─────────────────────────────────────────────┘

  Este tema cubre las capas: Servicios + SO
```

---

## 2. Reducir la superficie de ataque: servicios

### Principio: si no lo necesitas, desactívalo

Cada servicio corriendo es un potencial vector de ataque. Un servicio tiene:
- Un puerto abierto (punto de entrada de red)
- Un proceso con privilegios (si se compromete, el atacante los hereda)
- Posibles vulnerabilidades (más código = más bugs)

```bash
# Listar TODOS los servicios habilitados
systemctl list-unit-files --type=service --state=enabled

# Listar servicios que están corriendo AHORA
systemctl list-units --type=service --state=running

# Listar puertos abiertos
ss -tlnp    # TCP escuchando
ss -ulnp    # UDP escuchando
```

### Servicios comúnmente innecesarios

```
Servicios a evaluar para desactivar
═════════════════════════════════════

  Servicio              ¿Necesario?              Acción si no
  ────────              ───────────              ────────────
  avahi-daemon          Descubrimiento mDNS      Deshabilitar en servidores
                        (como Bonjour)           (útil solo en escritorio)

  cups / cups-browsed   Impresión                Deshabilitar si no imprime

  bluetooth             Bluetooth                Deshabilitar en servidores

  ModemManager          Modems                   Deshabilitar en servidores

  rpcbind               NFS/NIS RPC              Deshabilitar si no usa NFS

  nfs-server            Servidor NFS             Deshabilitar si no exporta

  postfix               Mail                     Evaluar: ¿envía alertas?
                                                 Si no → deshabilitar

  telnet.socket         Telnet (inseguro)        Deshabilitar SIEMPRE
                                                 Usar SSH

  rsyncd                rsync daemon             Deshabilitar si no se usa

  xinetd                Superservidor legacy     Deshabilitar (obsoleto)

  kdump                 Crash dump del kernel    Evaluar: ¿necesitas debug
                                                 de kernel panics?

  debug-shell.service   Shell root en tty9       Deshabilitar SIEMPRE
                        sin contraseña           en producción
```

### Deshabilitar servicios

```bash
# Deshabilitar y detener un servicio
sudo systemctl disable --now avahi-daemon
#                       ─────
#                       --now = disable + stop en un solo paso

# Verificar que está deshabilitado
systemctl is-enabled avahi-daemon
# disabled

# Verificar que no corre
systemctl is-active avahi-daemon
# inactive

# Para servicios con socket activation
sudo systemctl disable --now cups.socket cups.service

# Enmascarar un servicio (previene que se inicie incluso manualmente)
sudo systemctl mask telnet.socket
# Crea symlink a /dev/null — ni siquiera una dependencia puede iniciarlo

# Desenmascarar si lo necesitas después
sudo systemctl unmask telnet.socket
```

### mask vs disable

```
disable vs mask
═════════════════

  disable:
    → El servicio no arranca al boot
    → Pero PUEDE iniciarse manualmente: systemctl start X
    → Y PUEDE iniciarse como dependencia de otro servicio
    → Uso: servicios que no necesitas normalmente

  mask:
    → El servicio NO puede iniciarse de NINGUNA forma
    → Ni manual, ni como dependencia, ni por socket
    → Crea symlink: /etc/systemd/system/X.service → /dev/null
    → Uso: servicios que NUNCA deben correr (telnet, debug-shell)
```

---

## 3. Reducir la superficie de ataque: paquetes y puertos

### Paquetes innecesarios

```bash
# Listar paquetes instalados
rpm -qa | sort       # RHEL
dpkg -l | sort       # Debian/Ubuntu

# Evaluar paquetes de desarrollo (no deben estar en producción)
rpm -qa | grep -E "devel|debug"     # RHEL
dpkg -l | grep -E "\-dev\b|dbg"    # Debian

# Paquetes que suelen sobrar en servidores:
#   gcc, make, automake         → Compiladores (instalar malware)
#   telnet, rsh-server          → Protocolos inseguros
#   tcpdump, nmap, wireshark    → Herramientas de reconocimiento
#   *-devel / *-dev             → Headers de desarrollo

# Eliminar paquetes innecesarios
sudo dnf remove telnet-server rsh-server   # RHEL
sudo apt purge telnet rsh-server           # Debian
```

> **Nota**: no elimines herramientas de diagnóstico (tcpdump, strace) en
> servidores donde las necesitarás para troubleshooting. Evalúa caso por caso.

### Puertos abiertos

```bash
# Ver qué escucha y quién
sudo ss -tlnp
# State  Recv-Q  Send-Q  Local Address:Port  Peer Address:Port  Process
# LISTEN 0       128     0.0.0.0:22           0.0.0.0:*          sshd
# LISTEN 0       128     0.0.0.0:80           0.0.0.0:*          nginx
# LISTEN 0       128     127.0.0.1:3306       0.0.0.0:*          mysqld
#                        ─────────
#                        127.0.0.1 = solo local (bien)
#                        0.0.0.0   = todas las interfaces (evaluar)

# Cada puerto abierto a 0.0.0.0 o :: es accesible desde la red
# Pregunta: ¿necesita ser accesible desde la red?
#   Sí → Proteger con firewall (solo IPs necesarias)
#   No → Configurar para escuchar solo en 127.0.0.1
```

### Restringir servicios a localhost

```bash
# MySQL/MariaDB: solo local
# /etc/my.cnf.d/server.cnf
[mysqld]
bind-address = 127.0.0.1

# Redis: solo local
# /etc/redis/redis.conf
bind 127.0.0.1

# PostgreSQL: solo local
# /etc/postgresql/15/main/postgresql.conf
listen_addresses = 'localhost'

# Memcached: solo local
# /etc/sysconfig/memcached (RHEL)
OPTIONS="-l 127.0.0.1"
```

---

## 4. Parámetros de seguridad del kernel (sysctl)

El kernel expone parámetros configurables que afectan la seguridad del sistema.
Se gestionan con `sysctl`:

### Red: prevenir ataques de red

```bash
# ═══════ /etc/sysctl.d/99-security.conf ═══════

# --- IP Forwarding ---
# Deshabilitar reenvío de paquetes (si no es router/gateway)
net.ipv4.ip_forward = 0
net.ipv6.conf.all.forwarding = 0
# Un servidor comprometido no debe reenviar tráfico entre redes

# --- ICMP Redirects ---
# No aceptar ICMP redirects (previene ataques de enrutamiento)
net.ipv4.conf.all.accept_redirects = 0
net.ipv4.conf.default.accept_redirects = 0
net.ipv6.conf.all.accept_redirects = 0
net.ipv6.conf.default.accept_redirects = 0

# No enviar ICMP redirects
net.ipv4.conf.all.send_redirects = 0
net.ipv4.conf.default.send_redirects = 0

# --- Source Routing ---
# No aceptar paquetes con source routing (spoofing)
net.ipv4.conf.all.accept_source_route = 0
net.ipv4.conf.default.accept_source_route = 0
net.ipv6.conf.all.accept_source_route = 0
net.ipv6.conf.default.accept_source_route = 0

# --- Reverse Path Filtering ---
# Verificar que los paquetes vienen por la interfaz correcta
net.ipv4.conf.all.rp_filter = 1
net.ipv4.conf.default.rp_filter = 1
# 1 = strict (recomendado)
# 2 = loose (necesario en routing asimétrico)

# --- SYN Flood Protection ---
# Habilitar SYN cookies (protección contra SYN flood)
net.ipv4.tcp_syncookies = 1

# --- ICMP ---
# Ignorar broadcast ICMP (previene Smurf attacks)
net.ipv4.icmp_echo_ignore_broadcasts = 1

# Ignorar respuestas ICMP bogus
net.ipv4.icmp_ignore_bogus_error_responses = 1

# --- Logging ---
# Registrar paquetes sospechosos (martians)
net.ipv4.conf.all.log_martians = 1
net.ipv4.conf.default.log_martians = 1

# --- IPv6 Router Advertisements ---
# No aceptar router advertisements si no es necesario
net.ipv6.conf.all.accept_ra = 0
net.ipv6.conf.default.accept_ra = 0
```

### Kernel: prevenir explotación de vulnerabilidades

```bash
# --- Address Space Layout Randomization (ASLR) ---
# Aleatorizar direcciones de memoria (dificulta exploits)
kernel.randomize_va_space = 2
# 0 = deshabilitado (inseguro)
# 1 = parcial (stack y libraries)
# 2 = completo (stack, libraries, heap, mmap, vdso)

# --- Core Dumps ---
# Limitar core dumps (pueden contener datos sensibles)
fs.suid_dumpable = 0
# 0 = no generar core dumps para binarios SUID
# 1 = generar (inseguro — el dump puede contener datos privilegiados)
# 2 = generar pero solo legible por root

# --- Kernel Pointers ---
# Ocultar direcciones de memoria del kernel
kernel.kptr_restrict = 1
# 0 = visible para todos (inseguro)
# 1 = oculto para no-root
# 2 = oculto para todos (incluso root)

# --- dmesg ---
# Restringir acceso a dmesg (contiene info del kernel)
kernel.dmesg_restrict = 1
# 0 = cualquier usuario puede leer dmesg
# 1 = solo root puede leer dmesg

# --- SysRq ---
# Deshabilitar SysRq magic keys (o limitar)
kernel.sysrq = 0
# 0 = deshabilitado
# 1 = todas las funciones habilitadas
# Valores intermedios habilitan funciones específicas:
# 4 = solo sync, 16 = solo reboot, etc.

# --- ptrace ---
# Restringir ptrace (depuración de procesos)
kernel.yama.ptrace_scope = 1
# 0 = un proceso puede ptrace cualquier otro del mismo UID
# 1 = solo el proceso padre puede ptrace sus hijos
# 2 = solo root puede ptrace
# 3 = ptrace completamente deshabilitado

# --- Kernel modules ---
# Prevenir carga de módulos después del arranque
# kernel.modules_disabled = 1
# ¡PRECAUCIÓN! Una vez activado, no se puede revertir sin reboot
# Útil en servidores muy hardened que no cambian hardware

# --- Symlinks y hardlinks ---
# Proteger contra ataques de symlink/hardlink en /tmp
fs.protected_symlinks = 1
fs.protected_hardlinks = 1
# Previene que un usuario cree symlinks/hardlinks maliciosos
# en directorios world-writable como /tmp

# --- FIFOs y archivos regulares ---
fs.protected_fifos = 2
fs.protected_regular = 2
# Protección adicional para FIFOs y archivos en directorios sticky
```

### Aplicar parámetros

```bash
# Crear archivo de configuración
sudo nano /etc/sysctl.d/99-security.conf

# Aplicar inmediatamente
sudo sysctl --system
# Lee /etc/sysctl.conf y /etc/sysctl.d/*.conf

# Aplicar un archivo específico
sudo sysctl -p /etc/sysctl.d/99-security.conf

# Verificar un parámetro específico
sysctl net.ipv4.ip_forward
# net.ipv4.ip_forward = 0

# Cambiar temporalmente (se pierde al reiniciar)
sudo sysctl -w net.ipv4.ip_forward=0
```

---

## 5. Permisos y acceso a archivos sensibles

### Archivos críticos y sus permisos recomendados

```bash
# Archivos de identidad
chmod 644 /etc/passwd          # Legible por todos (necesario)
chmod 000 /etc/shadow          # Solo root (contraseñas hash)
chmod 644 /etc/group           # Legible por todos (necesario)
chmod 000 /etc/gshadow         # Solo root

# Verificar
ls -l /etc/passwd /etc/shadow /etc/group /etc/gshadow
# -rw-r--r-- root root /etc/passwd
# ---------- root root /etc/shadow
# -rw-r--r-- root root /etc/group
# ---------- root root /etc/gshadow

# Configuración de SSH
chmod 600 /etc/ssh/sshd_config
chmod 600 /etc/ssh/ssh_host_*_key       # Claves privadas
chmod 644 /etc/ssh/ssh_host_*_key.pub   # Claves públicas

# Sudoers
chmod 440 /etc/sudoers
chmod 440 /etc/sudoers.d/*

# Cron
chmod 600 /etc/crontab
chmod 700 /etc/cron.d
chmod 700 /etc/cron.daily
chmod 700 /etc/cron.hourly
chmod 700 /etc/cron.weekly
chmod 700 /etc/cron.monthly
```

### Buscar archivos con permisos peligrosos

```bash
# Archivos world-writable (cualquiera puede escribir)
find / -xdev -type f -perm -o+w -ls 2>/dev/null

# Directorios world-writable sin sticky bit
find / -xdev -type d \( -perm -o+w -a ! -perm -1000 \) -ls 2>/dev/null

# Archivos sin propietario (huérfanos — posible señal de intrusión)
find / -xdev -nouser -o -nogroup 2>/dev/null

# Archivos con SUID/SGID (ejecutan con privilegios del propietario)
find / -xdev \( -perm -4000 -o -perm -2000 \) -type f -ls 2>/dev/null
```

### SUID/SGID: reducir binarios privilegiados

```
Binarios SUID comunes y necesarios
════════════════════════════════════

  Necesarios (no eliminar SUID):
    /usr/bin/passwd        Cambiar contraseña (necesita /etc/shadow)
    /usr/bin/sudo          Elevar privilegios
    /usr/bin/su            Cambiar usuario
    /usr/bin/mount         Montar (usuarios normales con fstab)
    /usr/bin/umount        Desmontar
    /usr/bin/newgrp        Cambiar grupo
    /usr/bin/chage         Cambiar info de expiración

  Evaluar si necesarios:
    /usr/bin/pkexec        PolicyKit (¿hay escritorio?)
    /usr/bin/crontab       ¿Usuarios usan cron?
    /usr/sbin/pam_timestamp_check
    /usr/sbin/unix_chkpwd

  Quitar SUID si no se necesitan:
    chmod u-s /usr/bin/pkexec      # Si no hay escritorio
```

### Restringir acceso a cron y at

```bash
# /etc/cron.allow — si existe, SOLO los listados pueden usar cron
# /etc/cron.deny  — si existe, los listados NO pueden usar cron
# Si ninguno existe → todos pueden usar cron (RHEL)
# Si ninguno existe → solo root puede (Debian)

# Estrategia: crear cron.allow con solo los usuarios que lo necesitan
echo "root" | sudo tee /etc/cron.allow
echo "deploy" | sudo tee -a /etc/cron.allow

# Lo mismo para at
echo "root" | sudo tee /etc/at.allow
```

---

## 6. Banners y exposición de información

### Banners de login

```bash
# /etc/issue — mostrado ANTES del login (consola local)
# /etc/issue.net — mostrado ANTES del login (red/SSH)
# /etc/motd — mostrado DESPUÉS del login

# Por defecto pueden contener info del sistema:
#   \S ← nombre del SO
#   \r ← versión del kernel
#   \m ← arquitectura

# Un atacante puede usar esto para identificar versiones
# vulnerables antes de intentar login
```

```bash
# Reemplazar con banner legal genérico
cat > /etc/issue << 'EOF'
*************************************************************
*  Authorized access only. All activity is monitored and    *
*  recorded. Unauthorized access will be prosecuted.        *
*************************************************************
EOF

# Misma cosa para issue.net (SSH)
sudo cp /etc/issue /etc/issue.net

# Limpiar motd (o poner info útil sin revelar versiones)
echo "" | sudo tee /etc/motd
```

### Configurar banner en SSH

```bash
# /etc/ssh/sshd_config
Banner /etc/issue.net
# Muestra /etc/issue.net antes del prompt de login SSH
```

### Ocultar versiones de servicios

```bash
# Nginx: ocultar versión en headers
# /etc/nginx/nginx.conf
server_tokens off;
# Antes: Server: nginx/1.24.0
# Después: Server: nginx

# Apache: ocultar versión
# /etc/httpd/conf/httpd.conf
ServerTokens Prod
ServerSignature Off
# Antes: Server: Apache/2.4.57 (Red Hat Enterprise Linux)
# Después: Server: Apache

# Postfix: ocultar versión en SMTP banner
# /etc/postfix/main.cf
smtpd_banner = $myhostname ESMTP
# Antes: 220 mail.example.com ESMTP Postfix (3.5.17)
# Después: 220 mail.example.com ESMTP

# SSH: no se puede ocultar la versión del protocolo
# (es parte de la negociación SSH), pero sí el banner del SO
```

---

## 7. Hardening de SSH

SSH es casi siempre el servicio más expuesto. Hardening esencial:

### /etc/ssh/sshd_config

```bash
# ═══════ Autenticación ═══════

# Deshabilitar login de root directo
PermitRootLogin no
# Requiere sudo: trazabilidad de quién hizo qué

# Deshabilitar autenticación por contraseña (solo keys)
PasswordAuthentication no
PubkeyAuthentication yes
# Elimina ataques de fuerza bruta de contraseñas

# Deshabilitar contraseñas vacías
PermitEmptyPasswords no

# Deshabilitar métodos de auth inseguros
ChallengeResponseAuthentication no
KerberosAuthentication no
GSSAPIAuthentication no
# (Habilitar Kerberos/GSSAPI solo si realmente los usas)


# ═══════ Acceso ═══════

# Limitar usuarios que pueden hacer SSH
AllowUsers alice bob deploy
# O por grupo:
AllowGroups sshusers admins

# Tiempo máximo para autenticarse
LoginGraceTime 30
# 30 segundos para completar login (default: 120)

# Máximo de intentos de autenticación por conexión
MaxAuthTries 3

# Máximo de sesiones simultáneas por conexión
MaxSessions 3

# Limitar conexiones no autenticadas
MaxStartups 10:30:60
# 10: aceptar todas hasta 10 conexiones pendientes
# 30: rechazar 30% cuando hay más de 10
# 60: rechazar todas cuando hay más de 60


# ═══════ Seguridad ═══════

# Protocolo (SSH2 es el único soportado en versiones modernas)
# Protocol 2   ← Obsoleto como directiva, ya es obligatorio

# No permitir forwarding si no se necesita
AllowTcpForwarding no
X11Forwarding no
AllowAgentForwarding no
# Solo habilitar si los usuarios realmente los necesitan

# Deshabilitar tunnels
PermitTunnel no

# Desconectar sesiones inactivas
ClientAliveInterval 300
ClientAliveCountMax 2
# Envía ping cada 300s (5 min), desconecta tras 2 sin respuesta
# = desconexión tras 10 minutos de inactividad


# ═══════ Logging ═══════

# Nivel de log detallado
LogLevel VERBOSE
# Registra más información sobre cada conexión

# Banner legal
Banner /etc/issue.net
```

```bash
# Verificar sintaxis antes de recargar
sudo sshd -t
# Si no imprime nada → sintaxis correcta

# Recargar (no desconecta sesiones existentes)
sudo systemctl reload sshd
```

### Fail2ban: protección contra fuerza bruta

```bash
# Instalar
sudo dnf install fail2ban    # RHEL
sudo apt install fail2ban    # Debian

# Configurar para SSH
# /etc/fail2ban/jail.local
[sshd]
enabled = true
port = ssh
filter = sshd
maxretry = 3
bantime = 3600
findtime = 600
# 3 intentos fallidos en 10 min → ban 1 hora

# Habilitar
sudo systemctl enable --now fail2ban

# Ver bans activos
sudo fail2ban-client status sshd
```

---

## 8. Opciones de montaje de seguridad

Las opciones de montaje en `/etc/fstab` añaden una capa de seguridad a nivel
de filesystem:

### Opciones de seguridad

```
Opciones de montaje para seguridad
════════════════════════════════════

  noexec     No permitir ejecutar binarios en este filesystem
             Previene: ejecutar malware desde /tmp o /var/tmp

  nosuid     Ignorar bits SUID/SGID en este filesystem
             Previene: escalación de privilegios vía SUID

  nodev      No interpretar dispositivos de bloque/carácter
             Previene: crear /dev/ falsos para acceder a hardware

  ro         Solo lectura
             Protege: /boot, particiones de datos estáticos
```

### Aplicar en /etc/fstab

```bash
# Particiones a proteger con opciones de montaje:

# /tmp — ejecutables no deberían estar aquí
tmpfs  /tmp  tmpfs  defaults,noexec,nosuid,nodev  0 0

# /var/tmp — igual que /tmp
/dev/sda5  /var/tmp  ext4  defaults,noexec,nosuid,nodev  0 0

# /home — no debería tener SUID ni dispositivos
/dev/sda3  /home  ext4  defaults,nosuid,nodev  0 0

# /boot — solo lectura después de arrancar
/dev/sda1  /boot  ext4  defaults,ro,nosuid,nodev,noexec  0 0

# /dev/shm — memoria compartida
tmpfs  /dev/shm  tmpfs  defaults,noexec,nosuid,nodev  0 0

# Medios removibles
/dev/cdrom  /mnt/cdrom  iso9660  ro,noexec,nosuid,nodev,user  0 0
```

### Remontar sin reiniciar

```bash
# Aplicar cambios sin reiniciar
sudo mount -o remount,noexec,nosuid,nodev /tmp

# Verificar opciones activas
mount | grep /tmp
# tmpfs on /tmp type tmpfs (rw,nosuid,nodev,noexec,...)

# Verificar que funciona
cp /usr/bin/ls /tmp/
chmod +x /tmp/ls
/tmp/ls
# -bash: /tmp/ls: Permission denied    ← noexec funciona
```

### Precauciones

```
Cuidado con noexec
═══════════════════

  /tmp con noexec puede romper:
    → Instaladores que extraen y ejecutan en /tmp
    → Compilación (make usa /tmp para archivos temporales)
    → Algunos paquetes de apt/dnf que usan scripts en /tmp

  Solución: si algo falla, remontar temporalmente:
    sudo mount -o remount,exec /tmp
    # ... hacer lo necesario ...
    sudo mount -o remount,noexec /tmp

  /home con noexec rompe:
    → Scripts del usuario (~/script.sh)
    → Binarios descargados
    → Entornos de desarrollo

  En servidores sin usuarios interactivos: noexec en /home es OK
  En servidores con desarrolladores: NO poner noexec en /home
```

---

## 9. Herramientas de auditoría de hardening

### Lynis: auditoría de seguridad

```bash
# Instalar
sudo dnf install lynis    # RHEL (EPEL)
sudo apt install lynis    # Debian

# Ejecutar auditoría completa
sudo lynis audit system

# Salida:
# [+] Boot and services
#     - Checking running services (systemd)      [ DONE ]
#     - Checking enabled services at boot         [ DONE ]
# [+] Kernel Hardening
#     - Comparing sysctl key pairs with scan profile
#       - net.ipv4.ip_forward (exp: 0)            [ OK ]
#       - kernel.randomize_va_space (exp: 2)      [ OK ]
#       - net.ipv4.conf.all.rp_filter (exp: 1)   [ OK ]
# ...
# Hardening index: 67 [#############       ]

# Resultados detallados
cat /var/log/lynis.log
cat /var/log/lynis-report.dat
```

### OpenSCAP: cumplimiento normativo

```bash
# Instalar
sudo dnf install openscap-scanner scap-security-guide   # RHEL

# Listar perfiles disponibles
oscap info /usr/share/xml/scap/ssg/content/ssg-rhel9-ds.xml

# Evaluar contra un perfil (CIS, STIG, PCI-DSS)
sudo oscap xccdf eval \
    --profile xccdf_org.ssgproject.content_profile_cis \
    --results results.xml \
    --report report.html \
    /usr/share/xml/scap/ssg/content/ssg-rhel9-ds.xml

# Abrir reporte HTML
# report.html muestra cada regla: pass/fail/notapplicable

# Generar playbook de remediación Ansible
sudo oscap xccdf generate fix \
    --fix-type ansible \
    --profile xccdf_org.ssgproject.content_profile_cis \
    --output remediation.yml \
    /usr/share/xml/scap/ssg/content/ssg-rhel9-ds.xml
```

### Verificación rápida manual

```bash
# Script de verificación rápida de hardening

# 1. Servicios innecesarios
echo "=== Servicios habilitados ==="
systemctl list-unit-files --state=enabled --type=service | grep -v "@"

# 2. Puertos abiertos
echo "=== Puertos TCP abiertos ==="
ss -tlnp

# 3. Parámetros de kernel
echo "=== Kernel params ==="
sysctl net.ipv4.ip_forward
sysctl kernel.randomize_va_space
sysctl net.ipv4.conf.all.rp_filter
sysctl net.ipv4.tcp_syncookies

# 4. Permisos de archivos sensibles
echo "=== Permisos sensibles ==="
ls -l /etc/shadow /etc/gshadow /etc/sudoers

# 5. SUID/SGID
echo "=== Binarios SUID/SGID ==="
find / -xdev \( -perm -4000 -o -perm -2000 \) -type f 2>/dev/null | wc -l

# 6. SSH config
echo "=== SSH ==="
grep -E "^(PermitRootLogin|PasswordAuth|MaxAuthTries)" /etc/ssh/sshd_config
```

---

## 10. Errores comunes

### Error 1: deshabilitar servicios sin verificar dependencias

```bash
# MAL — deshabilitar sin entender
sudo systemctl disable --now rpcbind
# Si NFS depende de rpcbind → NFS deja de funcionar

# BIEN — verificar primero
systemctl list-dependencies rpcbind --reverse
# Muestra qué servicios dependen de rpcbind
# Si la lista está vacía (o solo cosas que no usas) → seguro deshabilitar
```

### Error 2: aplicar sysctl sin verificar el impacto

```bash
# MAL — copiar configuración de internet sin entender
net.ipv4.ip_forward = 0
# Si el servidor ES un router o gateway → rompe el enrutamiento

# MAL — deshabilitar IPv6 si hay servicios que lo usan
net.ipv6.conf.all.disable_ipv6 = 1
# Puede romper servicios que escuchan en :: (localhost IPv6)

# BIEN — entender cada parámetro antes de cambiarlo
# Verificar valor actual:
sysctl net.ipv4.ip_forward
# Si ya es 0 → no necesitas cambiarlo
# Si es 1 → ¿por qué? ¿Es un router? Investigar antes de cambiar
```

### Error 3: noexec en /tmp sin probar

```bash
# Poner noexec en /tmp puede romper:
# - apt/dnf (scripts post-install en /tmp)
# - Compilación (gcc usa /tmp)
# - Java (extrae JARs a /tmp)

# Si necesitas noexec en /tmp, configura alternativas:
# Para apt/dnf:
#   APT::ExtractTemplates::TempDir "/var/cache/apt/tmp";
# Para compilación:
#   export TMPDIR=/var/tmp   (si /var/tmp no tiene noexec)
```

### Error 4: deshabilitar PasswordAuthentication en SSH sin tener keys

```bash
# MAL — deshabilitar contraseñas antes de configurar keys
PasswordAuthentication no
# Si no tienes tu key pública en ~/.ssh/authorized_keys
# → Te bloqueas fuera del servidor

# BIEN — orden correcto:
# 1. Generar key: ssh-keygen -t ed25519
# 2. Copiar key: ssh-copy-id user@server
# 3. Probar login con key: ssh user@server
# 4. Solo entonces deshabilitar contraseñas
# 5. Mantener una sesión SSH abierta mientras pruebas
```

### Error 5: hardening sin baseline ni documentación

```bash
# MAL — aplicar 50 cambios sin registrar
# 3 meses después: "¿por qué el servidor no monta NFS?"
# Nadie recuerda que se deshabilitó rpcbind

# BIEN — documentar cada cambio
# Usar un archivo o sistema de configuración:
# - Ansible playbook (infraestructura como código)
# - Changelog en /root/hardening-log.txt
# - Commits en un repo de configuración
```

---

## 11. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════╗
║              Hardening básico — Cheatsheet                     ║
╠══════════════════════════════════════════════════════════════════╣
║                                                                  ║
║  SERVICIOS                                                       ║
║  systemctl list-unit-files --state=enabled --type=service        ║
║  systemctl disable --now SERVICIO       Deshabilitar + parar     ║
║  systemctl mask SERVICIO                Bloquear completamente   ║
║  systemctl list-dependencies X --reverse  Ver quién depende     ║
║                                                                  ║
║  PUERTOS                                                         ║
║  ss -tlnp                        TCP escuchando                  ║
║  ss -ulnp                        UDP escuchando                  ║
║  bind-address = 127.0.0.1        Restringir a localhost          ║
║                                                                  ║
║  SYSCTL ESENCIALES (/etc/sysctl.d/99-security.conf)             ║
║  net.ipv4.ip_forward = 0                  No reenviar            ║
║  net.ipv4.conf.all.rp_filter = 1          Anti-spoofing          ║
║  net.ipv4.tcp_syncookies = 1              Anti SYN flood         ║
║  net.ipv4.conf.all.accept_redirects = 0   Anti ICMP redirect    ║
║  net.ipv4.conf.all.send_redirects = 0     No enviar redirects   ║
║  net.ipv4.conf.all.accept_source_route = 0                      ║
║  kernel.randomize_va_space = 2            ASLR completo          ║
║  kernel.kptr_restrict = 1                 Ocultar kernel addrs   ║
║  kernel.dmesg_restrict = 1                dmesg solo root        ║
║  kernel.yama.ptrace_scope = 1             Limitar ptrace         ║
║  fs.suid_dumpable = 0                    No core dump SUID      ║
║  fs.protected_symlinks = 1                Anti-symlink attack    ║
║  fs.protected_hardlinks = 1               Anti-hardlink attack   ║
║                                                                  ║
║  Aplicar: sudo sysctl --system                                   ║
║                                                                  ║
║  SSH (/etc/ssh/sshd_config)                                      ║
║  PermitRootLogin no                                              ║
║  PasswordAuthentication no     (solo con keys configuradas)      ║
║  MaxAuthTries 3                                                  ║
║  AllowUsers alice bob                                            ║
║  ClientAliveInterval 300                                         ║
║  X11Forwarding no                                                ║
║  Verificar: sshd -t && systemctl reload sshd                    ║
║                                                                  ║
║  MONTAJE (/etc/fstab)                                            ║
║  noexec    No ejecutar binarios                                  ║
║  nosuid    Ignorar SUID/SGID                                     ║
║  nodev     No dispositivos                                       ║
║  Aplicar en: /tmp, /var/tmp, /dev/shm, /home (evaluar)          ║
║                                                                  ║
║  AUDITORÍA                                                       ║
║  sudo lynis audit system                Auditoría completa       ║
║  sudo oscap xccdf eval --profile CIS    Cumplimiento normativo  ║
║  find / -perm -4000 -type f             Buscar SUID             ║
║  find / -nouser -o -nogroup             Archivos huérfanos      ║
║                                                                  ║
╚══════════════════════════════════════════════════════════════════╝
```

---

## 12. Ejercicios

### Ejercicio 1: Auditar servicios del sistema

```bash
# Paso 1: Listar todos los servicios habilitados
systemctl list-unit-files --type=service --state=enabled

# Paso 2: Listar los puertos abiertos
sudo ss -tlnp
```

> **Pregunta de predicción**: en un servidor web con nginx y MySQL, ¿cuáles
> de los servicios habilitados esperarías ver como necesarios y cuáles como
> candidatos a deshabilitar? ¿Esperarías ver MySQL escuchando en `0.0.0.0`
> o en `127.0.0.1`?
>
> **Respuesta**: necesarios: `sshd`, `nginx`, `mysqld` (o mariadb),
> `firewalld`/`nftables`, `auditd`, `chronyd` (NTP). Candidatos a
> deshabilitar: `avahi-daemon`, `cups`, `bluetooth`, `ModemManager`,
> `rpcbind` (si no usa NFS). MySQL debería escuchar en `127.0.0.1` si
> nginx y MySQL están en el mismo servidor — no necesita exponerse a la red.

### Ejercicio 2: Aplicar parámetros sysctl

```bash
# Paso 1: Verificar valores actuales
sysctl net.ipv4.ip_forward
sysctl kernel.randomize_va_space
sysctl net.ipv4.conf.all.rp_filter
sysctl net.ipv4.tcp_syncookies
sysctl kernel.kptr_restrict
```

> **Pregunta de predicción**: si `net.ipv4.ip_forward` está en `1` y el
> servidor NO es un router, ¿qué riesgo implica? Si lo cambias a `0`, ¿qué
> podría dejar de funcionar?
>
> **Respuesta**: riesgo: si el servidor se compromete, el atacante puede
> usarlo para reenviar tráfico entre redes (pivoting), saltando la
> segmentación de red. Al cambiarlo a `0`: podría romperse Docker (Docker
> necesita ip_forward=1 para el networking de contenedores), VPNs que
> enrutan tráfico, o cualquier configuración de NAT/gateway. Siempre verificar
> si Docker o servicios de VPN están instalados antes de cambiar este
> parámetro.

### Ejercicio 3: Hardening de SSH

```bash
# Paso 1: Revisar la configuración actual
sudo grep -E "^(PermitRootLogin|PasswordAuth|MaxAuthTries|X11|AllowUsers)" \
    /etc/ssh/sshd_config
```

> **Pregunta de predicción**: si aplicas `PasswordAuthentication no` y
> `AllowUsers deploy` sin tener tu key SSH configurada ni tu usuario en
> la lista, ¿qué pasará? ¿Cómo te recuperarías?
>
> **Respuesta**: te bloqueas fuera del servidor completamente. Ningún
> usuario puede autenticarse con contraseña, y tu usuario no está en
> `AllowUsers`. Recuperación: acceso por consola física (o consola de la
> nube como AWS EC2 Serial Console), boot en single user/recovery mode,
> montar el filesystem y editar `sshd_config`, o desde una sesión SSH que
> aún tengas abierta (por eso nunca cerrar la sesión actual mientras
> pruebas cambios de SSH).

---

> **Siguiente tema**: T04 — GPG (generación de claves, cifrado, firma,
> verificación, keyservers).
