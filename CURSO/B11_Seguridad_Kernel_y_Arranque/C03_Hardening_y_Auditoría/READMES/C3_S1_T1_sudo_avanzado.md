# sudo avanzado: /etc/sudoers, visudo, aliases y restricciones

## Índice

1. [Cómo funciona sudo internamente](#1-cómo-funciona-sudo-internamente)
2. [/etc/sudoers: anatomía del archivo](#2-etcsudoers-anatomía-del-archivo)
3. [visudo: edición segura](#3-visudo-edición-segura)
4. [Aliases: organizar permisos](#4-aliases-organizar-permisos)
5. [Restricción por comando](#5-restricción-por-comando)
6. [NOPASSWD y autenticación](#6-nopasswd-y-autenticación)
7. [/etc/sudoers.d/: modularidad](#7-etcsudoersd-modularidad)
8. [sudo y logging](#8-sudo-y-logging)
9. [Configuraciones de seguridad avanzadas](#9-configuraciones-de-seguridad-avanzadas)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Cómo funciona sudo internamente

`sudo` no es simplemente "ejecutar como root". Es un sistema de delegación de
privilegios con autenticación, autorización y auditoría:

```
Flujo de sudo
══════════════

  usuario ejecuta: sudo systemctl restart nginx
       │
       ▼
  1. AUTENTICACIÓN
     ¿Quién eres?
     │
     ├── Pide contraseña del USUARIO (no de root)
     │   (a diferencia de su, que pide la de root)
     │
     ├── Verifica con PAM (/etc/pam.d/sudo)
     │
     └── Timestamp: si ya autenticó recientemente,
         no vuelve a pedir (por defecto 15 min)
       │
       ▼
  2. AUTORIZACIÓN
     ¿Tienes permiso?
     │
     ├── Lee /etc/sudoers (y /etc/sudoers.d/*)
     │
     ├── Busca regla que coincida:
     │     usuario  host = (run_as)  comando
     │
     ├── ¿Coincide? → Permitir
     │
     └── ¿No coincide? → "user is not in the sudoers file.
                           This incident will be reported."
       │
       ▼
  3. EJECUCIÓN
     Ejecuta el comando
     │
     ├── Cambia UID/GID al usuario destino (root por defecto)
     ├── Limpia variables de entorno (env_reset)
     ├── Ejecuta el comando
     └── Registra en log (/var/log/auth.log o /var/log/secure)
       │
       ▼
  4. AUDITORÍA
     Registra: quién, cuándo, desde dónde, qué comando
```

### sudo vs su vs login como root

```
┌──────────────┬───────────────────────────────────────────────┐
│ Método       │ Características                              │
├──────────────┼───────────────────────────────────────────────┤
│ login root   │ Sesión completa como root                    │
│              │ Sin auditoría de quién es el administrador   │
│              │ Contraseña de root compartida                │
│              │ ✗ No recomendado                             │
├──────────────┼───────────────────────────────────────────────┤
│ su -         │ Cambia a root pidiendo contraseña de ROOT    │
│              │ Shell completa de root                       │
│              │ Registro mínimo (solo que alguien hizo su)   │
│              │ △ Aceptable en emergencias                   │
├──────────────┼───────────────────────────────────────────────┤
│ sudo comando │ Ejecuta UN comando como root                 │
│              │ Pide contraseña del USUARIO                  │
│              │ Registro detallado (quién, qué, cuándo)      │
│              │ Permisos granulares por usuario/comando       │
│              │ ✓ Recomendado                                │
├──────────────┼───────────────────────────────────────────────┤
│ sudo -i      │ Shell interactiva como root vía sudo         │
│              │ Combina sudo con shell de root               │
│              │ Registra la invocación (no cada comando)      │
│              │ △ Cuando necesitas múltiples comandos root    │
└──────────────┴───────────────────────────────────────────────┘
```

---

## 2. /etc/sudoers: anatomía del archivo

### Formato de una regla

```
Formato de regla sudoers
══════════════════════════

  quién    dónde    = (como_quién)    qué

  usuario  host     = (run_as_user)   comando
  │        │          │               │
  │        │          │               └── Comando(s) permitidos
  │        │          └── Usuario/grupo destino (default: root)
  │        └── Host donde aplica (ALL = cualquiera)
  └── Usuario o %grupo

  Ejemplos:
  ─────────
  root      ALL  = (ALL)    ALL
  │         │      │        │
  │         │      │        └── Puede ejecutar cualquier comando
  │         │      └── Como cualquier usuario
  │         └── En cualquier host
  └── El usuario root

  %wheel    ALL  = (ALL)    ALL
  │
  └── Cualquier miembro del grupo wheel

  deploy    ALL  = (root)   /usr/bin/systemctl restart nginx
  │         │      │        │
  │         │      │        └── SOLO este comando exacto
  │         │      └── Solo como root
  │         └── En cualquier host
  └── El usuario deploy
```

### Archivo por defecto (RHEL)

```bash
# /etc/sudoers — contenido típico

## Defaults
Defaults   !visiblepw
Defaults   always_set_home
Defaults   match_group_by_gid
Defaults   always_query_group_plugin
Defaults   env_reset
Defaults   env_keep =  "COLORS DISPLAY HOSTNAME HISTSIZE KDEDIR LS_COLORS"
Defaults   env_keep += "MAIL QTDIR USERNAME LANG LC_ADDRESS LC_CTYPE"
Defaults   env_keep += "LC_COLLATE LC_IDENTIFICATION LC_MEASUREMENT"
Defaults   env_keep += "LC_MESSAGES LC_MONETARY LC_NAME LC_NUMERIC"
Defaults   env_keep += "LC_PAPER LC_TELEPHONE LC_TIME LC_ALL LANGUAGE"
Defaults   env_keep += "LINGUAS _XKB_CHARSET XAUTHORITY"
Defaults   secure_path = /sbin:/bin:/usr/sbin:/usr/bin

## Allow root to run any commands anywhere
root    ALL=(ALL)       ALL

## Allows people in group 'wheel' to run all commands
%wheel  ALL=(ALL)       ALL

## Read drop-in files from /etc/sudoers.d
@includedir /etc/sudoers.d
```

### Secciones del archivo

```
Estructura lógica de /etc/sudoers
══════════════════════════════════

  ┌─────────────────────────────────┐
  │  Defaults                       │  Opciones globales
  │  (env_reset, secure_path, etc.) │
  ├─────────────────────────────────┤
  │  Alias definitions              │  Agrupaciones (opcional)
  │  (User_Alias, Cmnd_Alias, etc.) │
  ├─────────────────────────────────┤
  │  User privilege rules           │  Quién puede hacer qué
  │  (root ALL=(ALL) ALL)           │
  │  (%wheel ALL=(ALL) ALL)         │
  ├─────────────────────────────────┤
  │  @includedir /etc/sudoers.d     │  Archivos adicionales
  └─────────────────────────────────┘
```

---

## 3. visudo: edición segura

**Nunca** edites `/etc/sudoers` directamente con `nano` o `vim`. Un error de
sintaxis puede **bloquear sudo para todos los usuarios**:

```
Por qué visudo y no nano/vim
══════════════════════════════

  nano /etc/sudoers:
    1. Abres el archivo
    2. Editas
    3. Guardas con error de sintaxis
    4. sudo deja de funcionar para TODOS
    5. No puedes usar sudo para arreglarlo
    6. Necesitas acceso root directo (consola, recovery mode)
    → DESASTRE

  visudo:
    1. Abre el archivo con bloqueo exclusivo
    2. Editas
    3. Al guardar, VALIDA la sintaxis
    4. Si hay error → te avisa y te deja corregir
    5. Solo guarda si la sintaxis es correcta
    → SEGURO
```

### Uso de visudo

```bash
# Editar /etc/sudoers
sudo visudo

# Editar un archivo en /etc/sudoers.d/
sudo visudo -f /etc/sudoers.d/deploy

# Verificar sintaxis sin editar
sudo visudo -c
# /etc/sudoers: parsed OK
# /etc/sudoers.d/deploy: parsed OK

# Cambiar el editor (si prefieres nano)
sudo EDITOR=nano visudo

# O permanentemente en .bashrc:
export EDITOR=nano
```

### Qué pasa si hay un error

```bash
sudo visudo

# Introduces un error y guardas:
# >>> /etc/sudoers: syntax error near line 25 <<<
# What now?
# Options are:
#   (e)dit sudoers file again
#   e(x)it without saving changes
#   (Q)uit and save changes    ← ¡NUNCA hagas esto!

# Elegir (e) para corregir
# Elegir (x) para descartar los cambios
# NUNCA (Q) — guarda el archivo roto
```

---

## 4. Aliases: organizar permisos

Los aliases agrupan usuarios, hosts, comandos o identidades run-as para
hacer las reglas más legibles y mantenibles:

### Tipos de aliases

```
Cuatro tipos de alias
══════════════════════

  User_Alias    → Agrupar usuarios
  Host_Alias    → Agrupar hosts
  Runas_Alias   → Agrupar usuarios destino (run-as)
  Cmnd_Alias    → Agrupar comandos
```

### User_Alias

```bash
# Agrupar usuarios por rol
User_Alias  ADMINS    = alice, bob, charlie
User_Alias  DEVOPS    = deploy, jenkins, ci_runner
User_Alias  DBA       = dbadmin, replication_user
User_Alias  WEBTEAM   = %webdevs, frontdev, backdev
#                        ^
#                        └── También acepta grupos con %

# Uso:
ADMINS  ALL = (ALL) ALL
DEVOPS  ALL = (root) /usr/bin/systemctl, /usr/bin/docker
```

### Host_Alias

```bash
# Agrupar hosts (útil con sudoers centralizado vía LDAP/IPA)
Host_Alias  WEBSERVERS = web01, web02, web03
Host_Alias  DBSERVERS  = db01, db02
Host_Alias  ALLSERVERS = WEBSERVERS, DBSERVERS
Host_Alias  INTERNAL   = 192.168.1.0/24

# Uso:
deploy  WEBSERVERS = (root) /usr/bin/systemctl restart nginx
dbadmin DBSERVERS  = (root) /usr/bin/systemctl restart mysqld
```

> **Nota**: Host_Alias es más útil cuando distribuyes el mismo sudoers a
> muchos servidores (vía Ansible, Puppet, FreeIPA). En un servidor individual,
> `ALL` suele ser suficiente.

### Runas_Alias

```bash
# Agrupar usuarios destino
Runas_Alias  WEBUSERS = www-data, nginx, apache
Runas_Alias  DBUSERS  = mysql, postgres

# Uso: deploy puede ejecutar comandos como www-data o nginx
deploy  ALL = (WEBUSERS) /usr/bin/php, /usr/local/bin/composer
```

### Cmnd_Alias

```bash
# Agrupar comandos por función
Cmnd_Alias  SERVICES  = /usr/bin/systemctl start *, \
                         /usr/bin/systemctl stop *, \
                         /usr/bin/systemctl restart *, \
                         /usr/bin/systemctl reload *, \
                         /usr/bin/systemctl status *

Cmnd_Alias  NETWORKING = /usr/sbin/ip, \
                          /usr/bin/ss, \
                          /usr/sbin/tcpdump, \
                          /usr/bin/traceroute

Cmnd_Alias  STORAGE    = /usr/sbin/fdisk, \
                          /usr/sbin/mkfs.*, \
                          /usr/bin/mount, \
                          /usr/bin/umount, \
                          /usr/sbin/lvm

Cmnd_Alias  LOGS       = /usr/bin/journalctl, \
                          /usr/bin/tail /var/log/*, \
                          /usr/bin/less /var/log/*

Cmnd_Alias  SHUTDOWN   = /usr/sbin/shutdown, \
                          /usr/sbin/reboot, \
                          /usr/sbin/poweroff

Cmnd_Alias  DANGEROUS  = /usr/bin/su, \
                          /usr/bin/bash, \
                          /usr/bin/sh, \
                          /usr/bin/csh, \
                          /usr/sbin/visudo, \
                          /usr/bin/passwd root
```

### Ejemplo completo con aliases

```bash
# ═══════ /etc/sudoers.d/company ═══════

# Aliases de usuarios
User_Alias  FULLADMINS = alice, bob
User_Alias  DEVOPS     = deploy, jenkins
User_Alias  HELPDESK   = hduser1, hduser2

# Aliases de comandos
Cmnd_Alias  SERVICES   = /usr/bin/systemctl start *, \
                          /usr/bin/systemctl stop *, \
                          /usr/bin/systemctl restart *, \
                          /usr/bin/systemctl status *

Cmnd_Alias  PASSWD     = /usr/bin/passwd [A-Za-z]*, \
                          !/usr/bin/passwd root

Cmnd_Alias  SHELLS     = /usr/bin/bash, /usr/bin/sh, \
                          /usr/bin/zsh, /usr/bin/su

# Reglas
FULLADMINS  ALL = (ALL) ALL
DEVOPS      ALL = (root) SERVICES, /usr/bin/docker
HELPDESK    ALL = (root) PASSWD, SERVICES

# Denegación explícita: helpdesk no puede obtener shell
HELPDESK    ALL = (root) !SHELLS
```

---

## 5. Restricción por comando

### Comandos exactos vs comodines

```
Niveles de restricción
═══════════════════════

  Más restrictivo ──────────────────────────── Menos restrictivo

  Comando exacto          Comando + args fijos    Comodín
  con args fijos          con glob                total

  /usr/bin/systemctl      /usr/bin/systemctl      /usr/bin/systemctl *
  restart nginx           restart *

  Solo restart nginx      Restart de cualquier    Cualquier operación
                          servicio                de systemctl
```

```bash
# Comando EXACTO (más seguro)
deploy ALL = (root) /usr/bin/systemctl restart nginx
# Solo puede: sudo systemctl restart nginx
# No puede:   sudo systemctl restart httpd     ← denegado
# No puede:   sudo systemctl stop nginx        ← denegado

# Comando con comodín
deploy ALL = (root) /usr/bin/systemctl restart *
# Puede:     sudo systemctl restart nginx      ← permitido
# Puede:     sudo systemctl restart httpd      ← permitido
# No puede:  sudo systemctl stop nginx         ← denegado

# Comando muy amplio (menos seguro)
deploy ALL = (root) /usr/bin/systemctl *
# Puede:     sudo systemctl restart nginx      ← permitido
# Puede:     sudo systemctl stop nginx         ← permitido
# Puede:     sudo systemctl disable firewalld  ← ¡permitido!

# TODO permitido (menos seguro)
deploy ALL = (root) ALL
```

### Denegación de comandos con !

```bash
# Permitir todo EXCEPTO ciertos comandos
admin ALL = (root) ALL, !/usr/bin/su, !/usr/bin/bash, !/usr/bin/sh

# Permitir servicios EXCEPTO detener el firewall
deploy ALL = (root) SERVICES, !/usr/bin/systemctl stop firewalld, \
                               !/usr/bin/systemctl disable firewalld
```

> **Advertencia importante**: las denegaciones con `!` no son infalibles.
> Un usuario ingenioso puede evadir la restricción:

```
Limitación de la denegación
════════════════════════════

  Regla: admin ALL = (root) ALL, !/usr/bin/bash

  Evasión 1: ejecutar otro shell
    sudo /usr/bin/zsh
    sudo /usr/bin/dash
    sudo /usr/bin/python3 -c "import os; os.system('/bin/bash')"

  Evasión 2: copiar el binario
    cp /usr/bin/bash /tmp/notbash
    sudo /tmp/notbash

  Evasión 3: usar editores con shell escape
    sudo vi → :!/bin/bash

  Conclusión: las denegaciones son una capa adicional,
  NO una barrera absoluta. Para seguridad real, usa una
  LISTA BLANCA (solo permitir lo necesario) en vez de una
  lista negra (permitir todo excepto X).
```

### Restricción por argumentos

```bash
# Permitir cambiar contraseña de cualquier usuario EXCEPTO root
helpdesk ALL = (root) /usr/bin/passwd [A-Za-z]*, !/usr/bin/passwd root

# Permitir montar solo ciertos dispositivos
operator ALL = (root) /usr/bin/mount /dev/sd[a-z][0-9] /mnt/*

# Permitir editar solo ciertos archivos con sudoedit
webadmin ALL = (root) sudoedit /etc/nginx/conf.d/*.conf
#                     ────────
#                     sudoedit es más seguro que sudo vi/nano
#                     (copia el archivo, el editor corre sin privilegios)

# Permitir tail/less solo en /var/log/
monitor ALL = (root) /usr/bin/tail /var/log/*, \
                     /usr/bin/less /var/log/*
```

---

## 6. NOPASSWD y autenticación

### NOPASSWD: sin contraseña

```bash
# No pedir contraseña para ciertos comandos
deploy ALL = (root) NOPASSWD: /usr/bin/systemctl restart nginx
#                   ────────
#                   Tag que elimina la solicitud de contraseña

# NOPASSWD para todos los comandos permitidos
deploy ALL = (root) NOPASSWD: ALL

# Mezclar: algunos con contraseña, algunos sin
deploy ALL = (root) /usr/bin/systemctl restart nginx, \
                    NOPASSWD: /usr/bin/systemctl status *
# restart nginx → pide contraseña
# status *      → sin contraseña
```

### Cuándo usar NOPASSWD

```
Cuándo NOPASSWD es apropiado
══════════════════════════════

  ✓ Servicios automatizados (Jenkins, Ansible, CI/CD)
    → No hay humano para escribir contraseña
    → Restringir a comandos ESPECÍFICOS

  ✓ Scripts de monitorización
    → Ejecutan periódicamente (cron, systemd timer)
    → Solo necesitan comandos de lectura

  ✓ Comandos informativos sin riesgo
    → systemctl status, journalctl, ss, ip addr

  ✗ Acceso administrativo amplio
    → Si el usuario tiene ALL, NOPASSWD es muy peligroso
    → Una sesión SSH comprometida tiene acceso root total

  ✗ Usuarios interactivos con muchos comandos
    → La contraseña actúa como "¿estás seguro?"
    → 15 min de cache es suficiente para no molestar
```

### PASSWD: forzar contraseña

```bash
# Forzar contraseña incluso si hay una regla NOPASSWD más general
deploy ALL = (root) NOPASSWD: SERVICES, PASSWD: SHUTDOWN
# Servicios → sin contraseña
# Shutdown  → siempre pide contraseña
```

### Otras tags

```
Tags disponibles en sudoers
════════════════════════════

  NOPASSWD:      No pedir contraseña
  PASSWD:        Pedir contraseña (override)

  NOEXEC:        No permitir que el comando ejecute otros
                 (previene shell escapes en vi, less, etc.)
  EXEC:          Permitir ejecución (default)

  SETENV:        Permitir al usuario pasar env vars
  NOSETENV:      No permitir (default, más seguro)

  LOG_INPUT:     Registrar stdin del comando
  LOG_OUTPUT:    Registrar stdout/stderr del comando
  NOLOG_INPUT:   No registrar stdin
  NOLOG_OUTPUT:  No registrar stdout/stderr
```

### NOEXEC: prevenir shell escapes

```bash
# Problema: el usuario puede escapar del editor
operator ALL = (root) /usr/bin/vi /etc/hosts
# El usuario hace: sudo vi /etc/hosts → :!/bin/bash → ¡shell root!

# Solución 1: NOEXEC
operator ALL = (root) NOEXEC: /usr/bin/vi /etc/hosts
# vi no puede ejecutar subprocesos → :! no funciona

# Solución 2 (mejor): usar sudoedit
operator ALL = (root) sudoedit /etc/hosts
# El editor corre como el USUARIO (no root)
# Solo la copia final se escribe como root
```

---

## 7. /etc/sudoers.d/: modularidad

### Por qué usar sudoers.d

```
/etc/sudoers vs /etc/sudoers.d/
════════════════════════════════

  Un solo archivo (/etc/sudoers):
    ✗ Todas las reglas en un archivo enorme
    ✗ Difícil de gestionar con Ansible/Puppet
    ✗ Conflictos al automatizar (múltiples roles editando)
    ✗ Un error puede afectar reglas no relacionadas

  Archivos modulares (/etc/sudoers.d/):
    ✓ Un archivo por rol, equipo o aplicación
    ✓ Ansible crea/elimina archivos individuales
    ✓ Sin conflictos entre roles diferentes
    ✓ Un error solo afecta ese archivo
    ✓ Fácil de auditar y revocar
```

### Crear archivos en sudoers.d

```bash
# Siempre usar visudo -f para crear/editar
sudo visudo -f /etc/sudoers.d/deploy

# Contenido:
# deploy ALL=(root) NOPASSWD: /usr/bin/systemctl restart nginx, \
#                              /usr/bin/systemctl restart php-fpm
```

### Reglas de nombres de archivo

```
Convenciones para archivos en sudoers.d/
═════════════════════════════════════════

  ✓ Usar caracteres alfanuméricos y guiones bajos
    deploy_permissions
    webteam_services
    monitoring_readonly

  ✗ NO usar punto (.) en el nombre
    deploy.conf    ← IGNORADO por sudo en algunas versiones
    webteam.rules  ← IGNORADO

  ✗ NO usar tilde (~) ni terminar en ~
    deploy~        ← IGNORADO (archivo de backup)

  ✓ Poner permisos correctos
    chmod 440 /etc/sudoers.d/deploy
    # Dueño: root:root, permisos: 440 (r--r-----)
```

### Ejemplo de organización

```
/etc/sudoers.d/
├── 00_defaults          ← Defaults adicionales
├── 10_fulladmins        ← Admin completo
├── 20_devops            ← Equipo DevOps
├── 30_helpdesk          ← Helpdesk
├── 40_monitoring        ← Monitorización (Zabbix, Nagios)
├── 50_deploy            ← Cuenta de despliegue
├── 60_backup            ← Cuenta de backup
└── 90_ansible           ← Ansible automation
```

```bash
# Ejemplo: /etc/sudoers.d/50_deploy
deploy ALL = (root) NOPASSWD: /usr/bin/systemctl restart nginx, \
                               /usr/bin/systemctl restart php-fpm, \
                               /usr/bin/systemctl reload nginx, \
                               /usr/bin/rsync --delete -a * /var/www/

# Ejemplo: /etc/sudoers.d/90_ansible
ansible ALL = (root) NOPASSWD: ALL
# (Ansible necesita acceso amplio, pero la cuenta está muy restringida
#  a nivel de SSH: solo desde la IP del control node, sin shell interactiva)
```

### Verificar todos los archivos

```bash
# Verificar sintaxis de todo
sudo visudo -c
# /etc/sudoers: parsed OK
# /etc/sudoers.d/50_deploy: parsed OK
# /etc/sudoers.d/90_ansible: parsed OK
```

---

## 8. sudo y logging

### Dónde se registra

```
Logs de sudo por distribución
══════════════════════════════

  RHEL / CentOS / Rocky:
    /var/log/secure
    journalctl -t sudo

  Ubuntu / Debian:
    /var/log/auth.log
    journalctl -t sudo

  Ambos con auditd:
    /var/log/audit/audit.log (type=USER_CMD)
```

### Qué se registra

```bash
# Invocación exitosa:
# Feb 15 10:23:45 server sudo: alice : TTY=pts/0 ; PWD=/home/alice ;
#   USER=root ; COMMAND=/usr/bin/systemctl restart nginx
#   │              │              │         │           │
#   │              │              │         │           └── Comando ejecutado
#   │              │              │         └── Usuario destino
#   │              │              └── Directorio actual
#   │              └── Terminal
#   └── Quién ejecutó sudo

# Intento fallido:
# Feb 15 10:24:00 server sudo: bob : user NOT in sudoers ;
#   TTY=pts/1 ; PWD=/home/bob ; USER=root ;
#   COMMAND=/usr/bin/passwd root
```

### Configurar logging avanzado

```bash
# En /etc/sudoers (via visudo):

# Registrar cada invocación en un archivo dedicado
Defaults  logfile="/var/log/sudo.log"

# Registrar también la entrada/salida de comandos
Defaults  log_input, log_output
Defaults  iolog_dir="/var/log/sudo-io/%{user}"
# Crea archivos con stdin/stdout/stderr del comando
# Permite "replay" con sudoreplay

# Registrar intentos fallidos con más detalle
Defaults  log_denied
```

### sudoreplay: reproducir sesiones

```bash
# Si log_output está habilitado, puedes reproducir sesiones:

# Listar sesiones registradas
sudo sudoreplay -l

# Reproducir una sesión específica
sudo sudoreplay -l user alice
sudo sudoreplay TSID

# Esto es extremadamente útil para auditoría:
# "¿Qué hizo exactamente alice a las 3am?"
```

---

## 9. Configuraciones de seguridad avanzadas

### Defaults útiles

```bash
# ═══════ En /etc/sudoers (via visudo) ═══════

# Tiempo de cache de contraseña (default: 15 min)
Defaults  timestamp_timeout=5
# Pedir contraseña cada 5 minutos
# timestamp_timeout=0 → pedir SIEMPRE
# timestamp_timeout=-1 → nunca expirar (no recomendado)

# Limitar intentos de contraseña (default: 3)
Defaults  passwd_tries=3

# Mostrar asteriscos al escribir contraseña
Defaults  pwfeedback
# En vez de nada, muestra ****

# Insultos al equivocarse (diversión de UNIX clásico)
Defaults  insults
# "Wrong! You moron!" etc. (no usar en producción seria)

# Requerir TTY (prevenir sudo desde scripts sin terminal)
Defaults  requiretty
# Previene: ssh server "sudo command" (sin TTY)
# Útil como medida anti-automatización no autorizada

# Deshabilitar requiretty para un usuario específico
Defaults:ansible  !requiretty

# PATH seguro (impide ataques de PATH hijacking)
Defaults  secure_path="/sbin:/bin:/usr/sbin:/usr/bin"
# sudo SIEMPRE usa este PATH, ignora el del usuario

# Restringir env vars que pasan a sudo
Defaults  env_reset
# Limpia la mayoría de variables de entorno
# Solo pasa las de env_keep

# Agregar variables al env_keep
Defaults  env_keep += "HTTP_PROXY HTTPS_PROXY NO_PROXY"

# Limitar a qué usuario se puede hacer sudo
Defaults  runas_default=root
```

### secure_path y ataques de PATH

```
Por qué secure_path es importante
═══════════════════════════════════

  Sin secure_path:
  ────────────────
  1. Atacante crea: /tmp/systemctl (script malicioso)
  2. Modifica PATH: export PATH=/tmp:$PATH
  3. Ejecuta: sudo systemctl restart nginx
  4. sudo busca "systemctl" en el PATH del usuario
  5. Encuentra /tmp/systemctl primero → ejecuta como root
  → COMPROMISO

  Con secure_path = /sbin:/bin:/usr/sbin:/usr/bin:
  ────────────────────────────────────────────────
  1. Atacante crea /tmp/systemctl
  2. Ejecuta: sudo systemctl restart nginx
  3. sudo ignora PATH del usuario, usa secure_path
  4. Encuentra /usr/bin/systemctl → ejecuta el correcto
  → SEGURO
```

### sudoedit: edición segura de archivos

```bash
# En vez de: sudo vi /etc/hosts (peligroso — shell escape)
# Usar:      sudoedit /etc/hosts

# Flujo de sudoedit:
#   1. Copia /etc/hosts a /tmp/hosts.XXXXX (archivo temporal)
#   2. Abre el editor como el USUARIO (sin privilegios root)
#   3. El usuario edita el temporal
#   4. sudo copia el temporal de vuelta a /etc/hosts como root

# Regla en sudoers:
webadmin ALL = (root) sudoedit /etc/nginx/conf.d/*.conf
# webadmin puede editar archivos de nginx sin obtener shell root
```

### Defaults por usuario o comando

```bash
# Defaults globales
Defaults  timestamp_timeout=5

# Defaults para un usuario específico
Defaults:deploy  !requiretty, timestamp_timeout=0

# Defaults para un grupo
Defaults:%devops  log_output

# Defaults para un comando
Defaults!/usr/bin/passwd  passwd_tries=1
```

---

## 10. Errores comunes

### Error 1: editar sudoers sin visudo

```bash
# MAL — riesgo de bloquear sudo
sudo nano /etc/sudoers
# Un error de sintaxis → sudo deja de funcionar

# BIEN — validación automática
sudo visudo

# Si ya cometiste el error y sudo no funciona:
# Opción 1: pkexec visudo (si PolicyKit está disponible)
# Opción 2: su - root (si sabes la contraseña de root)
# Opción 3: Recovery mode / single user desde GRUB
```

### Error 2: punto en nombre de archivo en sudoers.d

```bash
# MAL — el archivo se IGNORA silenciosamente
sudo visudo -f /etc/sudoers.d/deploy.conf
# En muchas distribuciones, archivos con "." se omiten

# BIEN — sin extensión o con guión bajo
sudo visudo -f /etc/sudoers.d/deploy
sudo visudo -f /etc/sudoers.d/deploy_rules
```

### Error 3: confiar solo en denegaciones (!comando)

```bash
# FALSA SEGURIDAD:
deploy ALL = (root) ALL, !/usr/bin/bash

# El usuario puede ejecutar:
#   sudo python3 -c "import os; os.execl('/bin/bash','bash')"
#   sudo find / -exec /bin/bash \;
#   sudo perl -e 'exec "/bin/bash"'
#   sudo vi → :!/bin/bash

# SEGURO: lista blanca de comandos permitidos
deploy ALL = (root) /usr/bin/systemctl restart nginx, \
                    /usr/bin/systemctl restart php-fpm
# Solo puede ejecutar estos dos comandos exactos
```

### Error 4: NOPASSWD: ALL para usuarios interactivos

```bash
# PELIGROSO:
alice ALL = (root) NOPASSWD: ALL
# Si la sesión SSH de alice se compromete → root inmediato
# Sin ninguna verificación de identidad

# MEJOR: NOPASSWD solo para comandos específicos de bajo riesgo
alice ALL = (root) ALL
alice ALL = (root) NOPASSWD: /usr/bin/systemctl status *
```

### Error 5: olvidar que el orden importa (última regla gana)

```bash
# En sudoers, si hay múltiples reglas para el mismo usuario,
# la ÚLTIMA coincidencia gana:

deploy ALL = (root) NOPASSWD: ALL          # Línea 20
deploy ALL = (root) PASSWD: /usr/sbin/reboot  # Línea 25

# Resultado: deploy necesita contraseña para reboot
# porque la línea 25 es la última que coincide con reboot

# Si inviertes el orden:
deploy ALL = (root) PASSWD: /usr/sbin/reboot   # Línea 20
deploy ALL = (root) NOPASSWD: ALL              # Línea 25

# Resultado: deploy NO necesita contraseña para reboot
# porque ALL en línea 25 coincide y es NOPASSWD
```

---

## 11. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════╗
║                sudo avanzado — Cheatsheet                      ║
╠══════════════════════════════════════════════════════════════════╣
║                                                                  ║
║  EDICIÓN                                                         ║
║  sudo visudo                      Editar /etc/sudoers            ║
║  sudo visudo -f /etc/sudoers.d/X  Editar archivo drop-in        ║
║  sudo visudo -c                   Verificar sintaxis             ║
║                                                                  ║
║  FORMATO DE REGLA                                                ║
║  quién  host = (run_as) [tag:] comando                           ║
║  alice  ALL  = (root)   NOPASSWD: /usr/bin/systemctl *           ║
║                                                                  ║
║  ALIASES                                                         ║
║  User_Alias   ADMINS = alice, bob                                ║
║  Host_Alias   WEB    = web01, web02                              ║
║  Runas_Alias  DBA    = mysql, postgres                           ║
║  Cmnd_Alias   SVC    = /usr/bin/systemctl *                      ║
║                                                                  ║
║  TAGS                                                            ║
║  NOPASSWD:    Sin contraseña                                     ║
║  PASSWD:      Con contraseña (override)                          ║
║  NOEXEC:      No permitir subprocesos (anti shell escape)        ║
║  LOG_OUTPUT:  Registrar salida del comando                       ║
║                                                                  ║
║  DENEGACIÓN                                                      ║
║  !comando     Denegar comando específico                         ║
║  (lista blanca > lista negra para seguridad)                     ║
║                                                                  ║
║  DEFAULTS ÚTILES                                                 ║
║  timestamp_timeout=5    Cache de contraseña (minutos)            ║
║  secure_path="..."      PATH fijo para sudo                     ║
║  requiretty              Requerir terminal                       ║
║  env_reset               Limpiar variables de entorno            ║
║  pwfeedback              Mostrar **** al escribir password       ║
║  logfile="/var/log/sudo.log"  Log dedicado                       ║
║                                                                  ║
║  SUDOEDIT                                                        ║
║  sudoedit /etc/hosts     Editar como root sin shell escape       ║
║  (editor corre como usuario, sudo solo copia el resultado)       ║
║                                                                  ║
║  DIAGNÓSTICO                                                     ║
║  sudo -l                 Ver permisos del usuario actual         ║
║  sudo -l -U alice        Ver permisos de alice                   ║
║  sudo -v                 Renovar timestamp de contraseña         ║
║  sudo -k                 Invalidar timestamp (expirar cache)     ║
║                                                                  ║
║  LOGS                                                            ║
║  RHEL:    /var/log/secure                                        ║
║  Ubuntu:  /var/log/auth.log                                      ║
║  Ambos:   journalctl -t sudo                                    ║
║                                                                  ║
║  REGLA DE ORO                                                    ║
║  → Mínimo privilegio: solo los comandos necesarios               ║
║  → Lista blanca > lista negra                                    ║
║  → NOPASSWD solo para automatización + comandos específicos      ║
║  → Siempre visudo, nunca nano/vim directo                        ║
║  → Archivos en sudoers.d/ sin punto en el nombre                 ║
║                                                                  ║
╚══════════════════════════════════════════════════════════════════╝
```

---

## 12. Ejercicios

### Ejercicio 1: Configurar permisos por rol

**Escenario**: una empresa tiene tres roles:
- **webadmin**: gestiona nginx y puede editar su configuración
- **monitor**: solo puede ver logs y estado de servicios
- **deploy**: puede reiniciar nginx y php-fpm sin contraseña

```bash
# Paso 1: Crear el archivo con visudo
sudo visudo -f /etc/sudoers.d/company_roles
```

> **Pregunta de predicción**: ¿usarías NOPASSWD para los tres roles? ¿Para
> cuáles sí y para cuáles no? ¿Usarías sudoedit o sudo vi para webadmin?
>
> **Respuesta**: NOPASSWD solo para **deploy** (automatización) y para
> **monitor** (comandos de solo lectura como `status` y `journalctl`).
> **webadmin** debe usar contraseña porque tiene acceso de escritura a
> configuración. Para editar archivos de nginx, webadmin debería usar
> `sudoedit` (no `sudo vi`), porque `sudo vi` permite shell escapes (`:!bash`
> da root). Solución:

```bash
# /etc/sudoers.d/company_roles
Cmnd_Alias NGINX_SVC = /usr/bin/systemctl restart nginx, \
                        /usr/bin/systemctl reload nginx, \
                        /usr/bin/systemctl restart php-fpm

webadmin ALL = (root) NGINX_SVC, sudoedit /etc/nginx/conf.d/*.conf
monitor  ALL = (root) NOPASSWD: /usr/bin/systemctl status *, \
                                /usr/bin/journalctl -u *
deploy   ALL = (root) NOPASSWD: NGINX_SVC
```

### Ejercicio 2: Auditar la configuración actual

```bash
# Paso 1: Ver tus propios permisos
sudo -l

# Paso 2: Verificar la sintaxis de toda la configuración
sudo visudo -c

# Paso 3: Buscar reglas NOPASSWD
sudo grep -r NOPASSWD /etc/sudoers /etc/sudoers.d/ 2>/dev/null
```

> **Pregunta de predicción**: si `sudo -l` muestra
> `(ALL) NOPASSWD: ALL`, ¿qué nivel de riesgo tiene? ¿Qué cambiarías?
>
> **Respuesta**: riesgo **máximo**. Equivale a acceso root permanente sin
> ninguna verificación. Una sesión SSH comprometida o un script malicioso
> ejecutado como este usuario tiene acceso root instantáneo. Cambios:
> 1. Eliminar `NOPASSWD` si el usuario es interactivo.
> 2. Si es cuenta de automatización, restringir a comandos específicos:
>    `deploy ALL = (root) NOPASSWD: /usr/bin/systemctl restart nginx`.
> 3. Restringir acceso SSH de esa cuenta (AllowFrom, solo keys, no password).

### Ejercicio 3: Prevenir escalación de privilegios

**Escenario**: quieres que el usuario `operator` pueda gestionar servicios
pero NO pueda obtener shell root.

```bash
# Intento 1 (insuficiente):
operator ALL = (root) ALL, !/usr/bin/bash, !/usr/bin/sh, !/usr/bin/su
```

> **Pregunta de predicción**: ¿por qué este enfoque es insuficiente? Nombra
> al menos tres formas en que operator podría obtener shell root a pesar de
> la denegación.
>
> **Respuesta**: es una lista negra, siempre es evasible:
> 1. `sudo python3 -c "import os; os.system('/bin/bash')"` — usa otro
>    intérprete.
> 2. `sudo vi /etc/hosts` y luego `:!/bin/bash` — shell escape desde editor.
> 3. `sudo cp /usr/bin/bash /tmp/mybash && sudo /tmp/mybash` — copia el
>    binario con otro nombre.
> 4. `sudo find / -name passwd -exec /bin/bash \;` — subcomando.
>
> La solución correcta es **lista blanca**: dar solo los comandos exactos
> que necesita, con NOEXEC donde sea necesario.

```bash
# Intento correcto (lista blanca):
Cmnd_Alias OPERATOR_CMDS = /usr/bin/systemctl start *, \
                            /usr/bin/systemctl stop *, \
                            /usr/bin/systemctl restart *, \
                            /usr/bin/systemctl status *, \
                            /usr/bin/journalctl -u *

operator ALL = (root) OPERATOR_CMDS
```

---

> **Siguiente tema**: T02 — auditd (reglas de auditoría, ausearch, aureport,
> monitoreo de archivos sensibles).
