# T06 — Primer contacto con SELinux/AppArmor (Enhanced)

## Por qué existen

Los permisos Unix (rwx, ACLs) son **DAC** — Discretionary Access Control. El
propietario del archivo decide quién tiene acceso. El problema: si un proceso
es comprometido y ejecuta como root (o como un usuario con demasiados permisos),
puede acceder a cualquier archivo que ese usuario pueda.

**MAC** — Mandatory Access Control — agrega una segunda capa de permisos que
el kernel aplica **independientemente de los permisos Unix**. Incluso root
puede ser bloqueado si la política MAC no permite el acceso.

```
DAC (permisos Unix): ¿El usuario tiene permiso?
MAC (SELinux/AppArmor): ¿La política permite que ESTE programa acceda a ESTE recurso?

Ambos deben permitir el acceso para que se conceda.
```

Cada familia de distribuciones usa un MAC diferente:

| Distribución | MAC | Activo por defecto |
|---|---|---|
| RHEL/AlmaLinux/Fedora | SELinux | Sí (enforcing) |
| Debian/Ubuntu | AppArmor | Sí (enforce) |

## SELinux (RHEL/AlmaLinux)

### Verificar el estado

```bash
# Comando rápido
getenforce
# Enforcing    ← activo y bloqueando
# Permissive   ← activo pero solo registrando (no bloquea)
# Disabled     ← desactivado

# Detalle completo
sestatus
# SELinux status:                 enabled
# SELinuxfs mount:                /sys/fs/selinux
# SELinux root directory:         /etc/selinux
# Loaded policy name:             targeted
# Current mode:                   enforcing
# Mode from config file:          enforcing
# Policy MLS status:              enabled
# Policy deny_unknown status:     allowed
# Memory protection checking:     actual (secure)
# Max kernel policy version:      33
```

### Modos de SELinux

| Modo | Bloquea | Registra | Uso |
|---|---|---|---|
| Enforcing | Sí | Sí | Producción |
| Permissive | No | Sí | Debugging, desarrollo |
| Disabled | No | No | No recomendado |

```bash
# Cambiar temporalmente (hasta el próximo reinicio)
sudo setenforce 0    # → Permissive
sudo setenforce 1    # → Enforcing

# Cambiar permanentemente
sudo vi /etc/selinux/config
# SELINUX=enforcing
```

### Contextos de seguridad

SELinux asigna un **contexto** a cada archivo, proceso y recurso del sistema:

```bash
# Ver contextos de archivos
ls -Z /var/www/html/
# -rw-r--r--. root root unconfined_u:object_r:httpd_sys_content_t:s0 index.html
#                        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#                        user:role:type:level

# Ver contextos de procesos
ps -eZ | grep httpd
# system_u:system_r:httpd_t:s0     1234 ?  ... /usr/sbin/httpd
#                   ^^^^^^^
#                   tipo del proceso
```

El kernel SELinux verifica: ¿puede un proceso con tipo `httpd_t` acceder a un
archivo con tipo `httpd_sys_content_t`? La política dice que sí. Pero si nginx
intenta acceder a `/home/dev/.ssh/`, la política lo bloquea aunque los permisos
Unix lo permitan.

### Errores comunes de SELinux

El síntoma más frecuente: "funciona sin SELinux pero falla con SELinux":

```bash
# Ejemplo: nginx no puede servir archivos de un directorio custom
# Error en /var/log/audit/audit.log:
# type=AVC msg=audit(...): avc: denied { read } for pid=1234
#   comm="nginx" name="index.html"
#   scontext=system_u:system_r:httpd_t:s0
#   tcontext=unconfined_u:object_r:user_home_t:s0

# El archivo tiene contexto user_home_t pero nginx espera httpd_sys_content_t
# Solución: cambiar el contexto del archivo
sudo semanage fcontext -a -t httpd_sys_content_t "/srv/web(/.*)?"
sudo restorecon -Rv /srv/web
```

### Herramientas de diagnóstico rápido

```bash
# Ver los últimos bloqueos de SELinux
sudo ausearch -m AVC --start recent

# Obtener sugerencias de corrección
sudo ausearch -m AVC --start recent | audit2why

# sealert (si está instalado, paquete setroubleshoot)
sudo sealert -a /var/log/audit/audit.log
```

## AppArmor (Debian/Ubuntu)

### Verificar el estado

```bash
sudo aa-status
# apparmor module is loaded.
# 28 profiles are loaded.
# 26 profiles are in enforce mode.
#    /usr/bin/evince
#    /usr/sbin/cups-browsed
#    /usr/sbin/cupsd
#    ...
# 2 profiles are in complain mode.
#    /usr/sbin/named
#    ...
# 4 processes have profiles defined.
# 4 processes are in enforce mode.
#    /usr/sbin/cupsd (1234)
#    ...
```

### Perfiles de AppArmor

AppArmor usa **perfiles por programa** (no por archivo como SELinux). Cada
perfil define qué archivos y recursos puede acceder un programa específico:

```bash
# Ver perfiles disponibles
ls /etc/apparmor.d/
# usr.sbin.cupsd
# usr.sbin.named
# usr.lib.snapd.snap-confine
# ...

# Ver el contenido de un perfil
cat /etc/apparmor.d/usr.sbin.cupsd | head -20
# /usr/sbin/cupsd {
#   #include <abstractions/base>
#   capability net_bind_service,
#   /etc/cups/** r,
#   /var/spool/cups/** rw,
#   ...
# }
```

### Modos de AppArmor

| Modo | Equivalente SELinux | Comportamiento |
|---|---|---|
| Enforce | Enforcing | Bloquea y registra |
| Complain | Permissive | Solo registra |
| Disabled (por perfil) | — | El perfil no se carga |

```bash
# Cambiar un perfil a complain
sudo aa-complain /usr/sbin/named

# Cambiar un perfil a enforce
sudo aa-enforce /usr/sbin/named

# Desactivar un perfil
sudo aa-disable /usr/sbin/named
```

## Comparación rápida

| Aspecto | SELinux | AppArmor |
|---|---|---|
| Distribución | RHEL, Fedora, CentOS | Debian, Ubuntu, SUSE |
| Modelo | Etiquetas en todo (archivos, procesos) | Perfiles por programa |
| Complejidad | Mayor | Menor |
| Granularidad | Muy fina | Suficiente para la mayoría de casos |
| Ver contextos/perfiles | `ls -Z`, `ps -Z` | `aa-status` |
| Debugging | `ausearch`, `audit2why` | `aa-logprof`, `/var/log/syslog` |
| Configuración | `/etc/selinux/` | `/etc/apparmor.d/` |

## Qué necesitas saber ahora

En este punto del curso, solo necesitas saber:

1. **Que existe**: si algo falla misteriosamente en permisos, puede ser MAC
2. **Cómo verificar**: `getenforce` (RHEL) o `aa-status` (Debian)
3. **Cómo diagnosticar**: mirar los logs de audit antes de desactivar
4. **No desactivar como primera opción**: `setenforce 0` o `aa-complain` son
   para diagnóstico, no para producción

La configuración completa de SELinux y AppArmor se cubre en B11 (Seguridad,
Kernel y Arranque), capítulos 1 y 2.

## Tabla: comandos esenciales

| Acción | SELinux | AppArmor |
|---|---|---|
| Ver estado | `getenforce`, `sestatus` | `aa-status` |
| Modo actual | `getenforce` | `aa-status` |
| Cambiar a Permissive/Complain | `setenforce 0` | `aa-complain` |
| Cambiar a Enforcing | `setenforce 1` | `aa-enforce` |
| Ver contextos | `ls -Z`, `ps -Z` | `aa-status` |
| Ver logs | `ausearch`, `audit2why` | `/var/log/syslog`, `dmesg` |
| Desactivar | `setenforce 0` + config | `aa-disable` |

## Ejercicios

### Ejercicio 1 — Verificar el estado

```bash
# En un contenedor RHEL o sistema RHEL:
getenforce
sestatus

# En un contenedor Debian o sistema Debian:
sudo aa-status
```

### Ejercicio 2 — Ver contextos SELinux

```bash
# Solo en RHEL:
# Contextos de archivos
ls -Z /etc/passwd
ls -Z /var/www/html/ 2>/dev/null

# Contextos de procesos
ps -eZ | head -10

# Contexto de tu shell
id -Z
```

### Ejercicio 3 — Ver perfiles AppArmor

```bash
# Solo en Debian:
# Perfiles cargados
sudo aa-status | head -20

# Ver un perfil
cat /etc/apparmor.d/usr.sbin.cupsd 2>/dev/null | head -20

# Logs de AppArmor
sudo grep -i apparmor /var/log/syslog | tail -5 2>/dev/null
```

### Ejercicio 4 — Intentar modo Permissive (SELinux)

```bash
#!/bin/bash
# Solo RHEL: cambiar temporalmente a permissive y observar

if command -v getenforce &>/dev/null; then
    echo "=== Estado actual ==="
    getenforce

    echo ""
    echo "=== Cambiando a Permissive ==="
    # Requires root
    # sudo setenforce 0
    # getenforce

    echo ""
    echo "=== Cambiando de vuelta a Enforcing ==="
    # sudo setenforce 1
    echo "(descomenta las líneas arriba para probar)"
else
    echo "Este sistema no tiene SELinux"
fi
```

### Ejercicio 5 — aa-complain y aa-enforce (AppArmor)

```bash
#!/bin/bash
# Solo Debian: cambiar un perfil a complain y volver

if command -v aa-status &>/dev/null; then
    echo "=== Perfiles en enforce ==="
    sudo aa-status | grep "profiles in enforce"

    echo ""
    echo "=== Cambiar cupsd a complain ==="
    # sudo aa-complain cupsd
    echo "(descomenta para probar)"

    echo ""
    echo "=== Volver a enforce ==="
    # sudo aa-enforce cupsd
    echo "(descomenta para probar)"
else
    echo "Este sistema no tiene AppArmor"
fi
```

### Ejercicio 6 — Buscar denegaciones recientes (SELinux)

```bash
#!/bin/bash
# Solo RHEL: buscar denegaciones recientes

if command -v ausearch &>/dev/null; then
    echo "=== Últimas denegaciones AVC ==="
    sudo ausearch -m AVC --start recent 2>/dev/null | tail -20 || echo "No hay denegaciones recientes"
else
    echo "ausearch no disponible"
fi
```

### Ejercicio 7 — Ver qué tipo de proceso es

```bash
#!/bin/bash
# Ver el tipo SELinux de procesos comunes

if command -v ps &>/dev/null && [[ -d /selinux || -f /sys/fs/selinux/enforce ]]; then
    echo "=== Procesos y sus contextos ==="
    ps -eZ | head -15
else
    echo "=== No hay SELinux activo ==="
fi

if command -v aa-status &>/dev/null; then
    echo ""
    echo "=== Perfiles cargados ==="
    sudo aa-status --profiled | wc -l
    echo "perfiles activos"
fi
```

### Ejercicio 8 — Entender la diferencia DAC vs MAC

```bash
#!/bin/bash
# Conceptual: mostrar la diferencia entre lo que DAC permite y MAC puede bloquear

echo "=== Permisos Unix (DAC) ==="
echo "root:root /etc/shadow: $(ls -la /etc/shadow | awk '{print $1, $3, $4}')"
echo "Todos pueden ver que root es el owner"

echo ""
echo "=== Lo que SELinux/AppArmor agregan ==="
echo "Una segunda capa: '¿este proceso TIENE PERMISO de acceder a este recurso?'"
echo "Por ejemplo, even si nginx tiene permisos Unix para leer /home/user/file,"
echo "SELinux lo BLOQUEARÍA si el tipo de archivo no es httpd_sys_content_t"

echo ""
echo "=== Cuándo sospechar de MAC ==="
echo "1. 'Permission denied' pero los permisos Unix están bien"
echo "2. Un servicio funciona como root pero no puede escribir"
echo "3. Archivos en ubicaciones no estándar no son accesibles"
```

### Ejercicio 9 — Comparar dos sistemas

```bash
#!/bin/bash
# Script que se adapta al sistema disponible

echo "=== Sistema de MAC ==="
if command -v getenforce &>/dev/null; then
    echo "SELinux detectado"
    echo "Estado: $(getenforce)"
    echo "Policy: $(sestatus | grep 'Loaded policy' | awk '{print $NF}')"
elif command -v aa-status &>/dev/null; then
    echo "AppArmor detectado"
    sudo aa-status | head -5
else
    echo "No se detectó SELinux ni AppArmor"
fi
```

### Ejercicio 10 — Logs de auditoría

```bash
#!/bin/bash
# Buscar entradas de MAC en los logs del sistema

echo "=== Buscando en /var/log/audit/audit.log (SELinux) ==="
if [[ -f /var/log/audit/audit.log ]]; then
    sudo grep -i "avc:" /var/log/audit/audit.log 2>/dev/null | tail -5 || echo "Sin entradas AVC recientes"
else
    echo "No existe /var/log/audit/audit.log"
fi

echo ""
echo "=== Buscando en /var/log/syslog (AppArmor) ==="
if [[ -f /var/log/syslog ]]; then
    sudo grep -i apparmor /var/log/syslog 2>/dev/null | tail -5 || echo "Sin entradas AppArmor recientes"
else
    echo "No existe /var/log/syslog"
fi
```
