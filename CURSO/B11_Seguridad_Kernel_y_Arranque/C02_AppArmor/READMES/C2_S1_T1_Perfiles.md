# Perfiles de AppArmor: enforce vs complain

## Índice

1. [Qué es AppArmor](#1-qué-es-apparmor)
2. [Modelo de seguridad: perfiles por ruta](#2-modelo-de-seguridad-perfiles-por-ruta)
3. [Modos de perfil: enforce vs complain](#3-modos-de-perfil-enforce-vs-complain)
4. [aa-status: estado del sistema](#4-aa-status-estado-del-sistema)
5. [Anatomía de un perfil](#5-anatomía-de-un-perfil)
6. [Dónde viven los perfiles](#6-dónde-viven-los-perfiles)
7. [Perfiles en distribuciones](#7-perfiles-en-distribuciones)
8. [AppArmor y systemd](#8-apparmor-y-systemd)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Qué es AppArmor

AppArmor (**Application Armor**) es un sistema MAC (Mandatory Access Control)
del kernel Linux, igual que SELinux. Ambos usan el framework LSM (Linux
Security Modules), pero con filosofías radicalmente diferentes:

```
SELinux vs AppArmor — enfoque fundamental
==========================================

  SELinux                          AppArmor
  ───────                          ────────
  Etiqueta OBJETOS                 Etiqueta RUTAS
  (type en xattr)                  (path en perfil)

  Pregunta:                        Pregunta:
  "¿El tipo del proceso            "¿El perfil del programa
   tiene permiso sobre              le permite acceder
   el tipo del archivo?"            a esta ruta?"

  user:role:type:level             /usr/sbin/httpd {
  → httpd_t accede a                 /var/www/** r,
    httpd_sys_content_t             }
```

### Por qué existe AppArmor si ya existe SELinux

SELinux es extremadamente potente pero tiene una curva de aprendizaje
pronunciada. AppArmor nació como alternativa más accesible:

```
Línea temporal
═══════════════

  1998 ── Immunix crea SubDomain (precursor de AppArmor)
  2000 ── NSA libera SELinux
  2003 ── SELinux entra en kernel 2.6
  2005 ── SubDomain se renombra a AppArmor
  2006 ── Novell lo integra en SUSE
  2007 ── Canonical lo adopta para Ubuntu
  2009 ── AppArmor se integra en el kernel 2.6.36
  2010+ ── Ubuntu, Debian, SUSE lo usan por defecto
           RHEL/Fedora usan SELinux por defecto
```

### Distribuciones y su sistema MAC por defecto

```
┌──────────────────────────────────────────────┐
│       Sistema MAC por defecto                │
├───────────────────┬──────────────────────────┤
│ SELinux            │ RHEL, Fedora, CentOS     │
│                    │ Rocky, Alma, Oracle Linux │
│                    │ Amazon Linux              │
├───────────────────┼──────────────────────────┤
│ AppArmor           │ Ubuntu, Debian            │
│                    │ SUSE, openSUSE            │
│                    │ Linux Mint, Pop!_OS       │
├───────────────────┼──────────────────────────┤
│ Ninguno (manual)   │ Arch, Gentoo, Slackware  │
│                    │ (disponible, no activo)   │
└───────────────────┴──────────────────────────┘
```

> **Clave para el examen**: en un entorno profesional, encontrarás SELinux en
> servidores RHEL/CentOS y AppArmor en servidores Ubuntu/Debian. Necesitas
> saber gestionar ambos.

---

## 2. Modelo de seguridad: perfiles por ruta

La diferencia arquitectónica central entre AppArmor y SELinux es **cómo
identifican los objetos**:

```
SELinux: seguridad basada en ETIQUETAS
=======================================

  El archivo /var/www/html/index.html tiene un atributo extendido:
    security.selinux = system_u:object_r:httpd_sys_content_t:s0

  Si mueves el archivo a /tmp/:
    mv /var/www/html/index.html /tmp/
    El archivo CONSERVA su etiqueta httpd_sys_content_t
    → SELinux lo sigue protegiendo (pero puede causar problemas)

  El tipo viaja CON el archivo, independiente de la ruta


AppArmor: seguridad basada en RUTAS
=====================================

  El perfil dice:
    /usr/sbin/httpd {
      /var/www/html/** r,
    }

  Si mueves el archivo a /tmp/:
    mv /var/www/html/index.html /tmp/
    AppArmor ya NO lo protege (la ruta /tmp/ no está en el perfil)
    → Pero httpd tampoco puede leerlo (la ruta /tmp/ no tiene permiso r)

  La protección depende de DÓNDE está el archivo
```

### Implicaciones prácticas

```
Comparación por ruta vs por etiqueta
======================================

  Ventajas de AppArmor (ruta):
    ✓ No necesita sistema de archivos con xattr
    ✓ Funciona con NFS, CIFS, tmpfs sin problemas
    ✓ Los perfiles son legibles (rutas conocidas)
    ✓ No hay que re-etiquetar después de mover archivos
    ✓ Curva de aprendizaje más suave

  Desventajas de AppArmor (ruta):
    ✗ Hardlinks pueden evadir la protección
    ✗ Si renombras un directorio, las reglas dejan de aplicar
    ✗ No puede proteger archivos "por lo que son", solo "por dónde están"
    ✗ Menos granular que SELinux (no hay user/role/level)

  Ventajas de SELinux (etiqueta):
    ✓ La identidad del objeto no depende de la ubicación
    ✓ Hardlinks no evaden la protección
    ✓ Granularidad total (user, role, type, level)
    ✓ MLS (Multi-Level Security) para entornos clasificados

  Desventajas de SELinux (etiqueta):
    ✗ Requiere filesystem con xattr
    ✗ Etiquetado puede corromperse (mv, tar sin --selinux)
    ✗ Complejidad significativamente mayor
    ✗ Relabeling puede tomar horas en discos grandes
```

### Qué confina AppArmor

A diferencia de SELinux (que en modo `targeted` confina ~200 servicios de red),
AppArmor confina **solo los programas que tienen perfil cargado**:

```
AppArmor: confinamiento selectivo
===================================

  Programa                   ¿Perfil?    Estado
  ──────────────────         ────────    ──────
  /usr/sbin/tcpdump          Sí          enforce
  /usr/sbin/cups-browsed     Sí          enforce
  /usr/bin/evince            Sí          enforce
  /usr/sbin/mysqld           Sí          enforce
  /usr/sbin/nginx            Sí          enforce
  /usr/bin/man               Sí          enforce
  /usr/sbin/sshd             No*         sin confinar
  /usr/bin/bash              No          sin confinar
  /home/user/my_script.sh    No          sin confinar

  * Algunas distribuciones sí incluyen perfil para sshd

  Sin perfil = el proceso corre sin restricciones AppArmor
  (sigue sujeto a DAC y otras medidas de seguridad)
```

---

## 3. Modos de perfil: enforce vs complain

Cada perfil de AppArmor puede estar en uno de tres estados:

### enforce

El perfil **bloquea** accesos no permitidos y los registra en el log:

```
Modo enforce
═════════════

  nginx intenta leer /etc/shadow
      │
      ▼
  AppArmor: ¿/etc/shadow está en el perfil de nginx?
      │
      NO → DENEGAR acceso + registrar en log
      │
      ▼
  audit: apparmor="DENIED" operation="open"
         profile="/usr/sbin/nginx"
         name="/etc/shadow"
```

### complain

El perfil **permite** todos los accesos pero **registra** los que violarían la
política:

```
Modo complain
══════════════

  nginx intenta leer /etc/shadow
      │
      ▼
  AppArmor: ¿/etc/shadow está en el perfil de nginx?
      │
      NO → PERMITIR acceso + registrar en log como violación
      │
      ▼
  audit: apparmor="ALLOWED" operation="open"
         profile="/usr/sbin/nginx"
         name="/etc/shadow"
```

### unconfined (sin perfil o deshabilitado)

El programa no tiene perfil cargado. AppArmor no interviene:

```
Sin perfil
═══════════

  my_app intenta leer /etc/shadow
      │
      ▼
  AppArmor: ¿Existe perfil para my_app?
      │
      NO → AppArmor no interviene
      │
      ▼
  Solo DAC decide (permisos rwx normales)
```

### Comparación de modos

```
┌────────────┬──────────────┬──────────────┬───────────────┐
│            │  enforce     │  complain    │  unconfined   │
├────────────┼──────────────┼──────────────┼───────────────┤
│ Bloquea    │ Sí           │ No           │ No            │
│ Registra   │ Sí (DENIED)  │ Sí (ALLOWED) │ No            │
│ Perfil     │ Cargado      │ Cargado      │ No cargado    │
│ Uso        │ Producción   │ Desarrollo   │ Sin política  │
│            │              │ / debugging  │               │
└────────────┴──────────────┴──────────────┴───────────────┘
```

### Analogía con SELinux

```
Mapeo de conceptos SELinux → AppArmor
=======================================

  SELinux                   AppArmor
  ───────                   ────────
  Enforcing (global)        No hay equivalente global*
  Permissive (global)       No hay equivalente global*
  Disabled                  systemctl disable apparmor

  Permissive por dominio    complain por perfil    ← equivalente exacto
  Enforcing por dominio     enforce por perfil     ← equivalente exacto

  * AppArmor no tiene un "switch global" enforce/permissive.
    Cada perfil se configura individualmente.
    (Aunque se puede pasar apparmor=complain en el kernel)
```

> **Esto es una ventaja**: en SELinux, poner el sistema en `permissive` global
> desactiva la protección de TODOS los servicios. En AppArmor, puedes poner
> solo un perfil en `complain` mientras los demás siguen en `enforce`.

---

## 4. aa-status: estado del sistema

`aa-status` (o `apparmor_status`) es el comando principal para ver el estado
completo de AppArmor:

```bash
sudo aa-status
```

### Salida típica

```
apparmor module is loaded.
56 profiles are loaded.
  43 profiles are in enforce mode.
     /snap/snapd/21759/usr/lib/snapd/snap-confine
     /snap/snapd/21759/usr/lib/snapd/snap-confine//mount-namespace-capture-helper
     /usr/bin/evince
     /usr/bin/evince-previewer
     /usr/bin/evince-thumbnailer
     /usr/bin/man
     /usr/lib/cups/backend/cups-pdf
     /usr/sbin/cups-browsed
     /usr/sbin/cupsd
     /usr/sbin/mysqld
     /usr/sbin/nginx
     /usr/sbin/tcpdump
     ...
  13 profiles are in complain mode.
     /usr/sbin/dnsmasq
     ...
  0 profiles are in kill mode.
  0 profiles are in unconfined mode.
4 processes have profiles defined.
  4 processes are in enforce mode.
     /usr/sbin/cups-browsed (1234)
     /usr/sbin/cupsd (1235)
     /usr/sbin/mysqld (5678)
     /usr/sbin/nginx (9012)
  0 processes are in complain mode.
  0 processes are unconfined but have a profile defined.
```

### Interpretar la salida

```
aa-status — secciones
═══════════════════════

  Sección 1: Estado del módulo
  ─────────────────────────────
  "apparmor module is loaded."     → El módulo del kernel está activo
  "apparmor module is not loaded." → AppArmor no está corriendo

  Sección 2: Perfiles cargados
  ─────────────────────────────
  "56 profiles are loaded."        → Total de perfiles en memoria
  "43 profiles are in enforce"     → Protegiendo activamente
  "13 profiles are in complain"    → Solo registrando

  Sección 3: Procesos confinados
  ─────────────────────────────
  "4 processes have profiles"      → Procesos actualmente ejecutándose
                                     que tienen un perfil definido
  "4 in enforce mode"              → Procesos activamente confinados
  "0 unconfined but have profile"  → Tienen perfil pero NO está cargado
                                     (perfil deshabilitado)
```

### Diferencia entre perfiles y procesos

```
Perfiles vs procesos — conceptos distintos
============================================

  Perfiles cargados = reglas en memoria del kernel
    → Un perfil puede estar cargado aunque el servicio NO esté corriendo
    → Ejemplo: perfil de /usr/sbin/tcpdump cargado, pero tcpdump no corre

  Procesos confinados = programas ejecutándose AHORA con perfil activo
    → Solo aparecen si el proceso está corriendo Y su perfil está cargado
    → Ejemplo: nginx corriendo → aparece en "processes in enforce mode"

  Posibles estados de un programa:
  ┌─────────────────────┬──────────────┬──────────────────────┐
  │ Proceso corriendo   │ Perfil       │ Resultado            │
  ├─────────────────────┼──────────────┼──────────────────────┤
  │ Sí                  │ enforce      │ Confinado            │
  │ Sí                  │ complain     │ Registrando          │
  │ Sí                  │ no cargado   │ Sin confinar         │
  │ Sí                  │ no existe    │ Sin confinar         │
  │ No                  │ enforce      │ Perfil listo, no se  │
  │                     │              │ aplica hasta que     │
  │                     │              │ arranque el proceso  │
  │ No                  │ no existe    │ Nada que gestionar   │
  └─────────────────────┴──────────────┴──────────────────────┘
```

### aa-status con filtros

```bash
# Solo contar perfiles por modo
sudo aa-status --profiled

# Solo perfiles en enforce
sudo aa-status --enforced

# Solo en complain
sudo aa-status --complaining

# Verificar si AppArmor está habilitado (útil en scripts)
sudo aa-enabled
# → Yes / No
# Código de retorno: 0 = habilitado, no-0 = deshabilitado

# Para scripts:
if aa-enabled --quiet 2>/dev/null; then
    echo "AppArmor is active"
fi
```

---

## 5. Anatomía de un perfil

Antes de gestionar perfiles (tema T02), conviene entender su estructura básica:

### Perfil mínimo

```
# Perfil para /usr/sbin/nginx
/usr/sbin/nginx {
  #include <abstractions/base>

  /var/log/nginx/*.log w,
  /etc/nginx/** r,
  /var/www/html/** r,
  /run/nginx.pid rw,

  network inet tcp,
  capability net_bind_service,
}
```

### Elementos del perfil

```
Anatomía de un perfil AppArmor
================================

  /usr/sbin/nginx {              ← Ruta del ejecutable = identidad
  │
  │  #include <abstractions/base>  ← Includes comunes (DNS, libc, etc.)
  │
  │  /var/log/nginx/*.log w,     ← Regla de archivo: ruta + permisos
  │  │                    │ │
  │  │                    │ └─ Coma obligatoria al final
  │  │                    └─── Permisos (r/w/a/l/k/m/x)
  │  └──────────────────────── Ruta con globs (*, **)
  │
  │  network inet tcp,           ← Regla de red
  │  capability net_bind_service,← Capability Linux
  │
  }                              ← Fin del perfil
```

### Permisos de archivo

```
Permisos en reglas AppArmor
════════════════════════════

  r   → read       Leer el archivo
  w   → write      Escribir (incluye crear, truncar)
  a   → append     Solo agregar (no sobrescribir)
  l   → link       Crear hardlinks
  k   → lock       Bloquear archivo (flock)
  m   → mmap       Memory-map con PROT_EXEC

  Permisos de ejecución (mutuamente excluyentes):
  ix  → inherit    Heredar el perfil del padre
  cx  → child      Transicionar a un sub-perfil (child profile)
  px  → profile    Transicionar a otro perfil definido
  ux  → unconfined Ejecutar sin confinamiento (¡peligroso!)
  Px  → px + clean environment
  Cx  → cx + clean environment
  Ux  → ux + clean environment
```

### Globs en rutas

```
Patrones glob en AppArmor
══════════════════════════

  /etc/nginx/nginx.conf     → Archivo exacto
  /etc/nginx/*              → Cualquier archivo en /etc/nginx/
                               (NO subdirectorios)
  /etc/nginx/**             → Cualquier archivo en /etc/nginx/
                               y todos los subdirectorios (recursivo)
  /var/log/nginx/*.log      → Solo archivos .log en ese directorio
  /home/*/public_html/**    → public_html de cualquier usuario

  Comparación con SELinux:
    AppArmor: /var/www/html/** r,
    SELinux:  semanage fcontext -a -t httpd_sys_content_t "/var/www/html(/.*)?"
              (regex, no glob)
```

### Abstractions

Las **abstractions** son fragmentos reutilizables que incluyen permisos
comunes:

```bash
# Las abstractions viven en /etc/apparmor.d/abstractions/
ls /etc/apparmor.d/abstractions/
# base           ← Permisos mínimos: /etc/ld.so.cache, /dev/null, etc.
# nameservice    ← DNS, nsswitch, /etc/hosts, /etc/resolv.conf
# authentication ← PAM, /etc/shadow, /etc/pam.d/
# apache2-common ← Compartido entre perfiles de Apache
# mysql          ← Compartido entre perfiles de MySQL
# ssl_certs      ← Acceso a certificados TLS
# user-tmp       ← Acceso a /tmp del usuario
```

```
Cómo funcionan los includes
═════════════════════════════

  /usr/sbin/nginx {
    #include <abstractions/base>
    │
    │  Se expande a:
    │    /etc/ld.so.cache r,
    │    /dev/null rw,
    │    /dev/zero r,
    │    /proc/sys/kernel/random/uuid r,
    │    ... (decenas de reglas básicas)
    │
    #include <abstractions/nameservice>
    │
    │  Se expande a:
    │    /etc/hosts r,
    │    /etc/resolv.conf r,
    │    /etc/nsswitch.conf r,
    │    /run/systemd/resolve/stub-resolv.conf r,
    │    ...
    │
    /var/www/html/** r,    ← Reglas específicas de este perfil
  }
```

---

## 6. Dónde viven los perfiles

### Directorio principal: /etc/apparmor.d/

```bash
ls /etc/apparmor.d/
```

```
/etc/apparmor.d/
├── abstractions/          ← Fragmentos reutilizables
│   ├── base
│   ├── nameservice
│   ├── authentication
│   └── ...
├── disable/               ← Symlinks a perfiles deshabilitados
├── force-complain/        ← Symlinks para forzar modo complain
├── local/                 ← Overrides locales del administrador
│   ├── usr.sbin.nginx
│   └── usr.sbin.mysqld
├── tunables/              ← Variables (HOME, PROC, etc.)
│   ├── global
│   ├── home
│   └── ...
├── usr.sbin.nginx         ← Perfil de nginx
├── usr.sbin.mysqld        ← Perfil de MySQL
├── usr.sbin.tcpdump       ← Perfil de tcpdump
├── usr.sbin.cups-browsed  ← Perfil de CUPS
└── ...
```

### Convención de nombres

```
Nombre del archivo de perfil
══════════════════════════════

  Ejecutable:  /usr/sbin/nginx
  Perfil:      /etc/apparmor.d/usr.sbin.nginx
                                ───┬───────────
                                   └── Ruta con / reemplazado por .

  Ejecutable:  /usr/bin/evince
  Perfil:      /etc/apparmor.d/usr.bin.evince

  Ejecutable:  /usr/lib/cups/backend/cups-pdf
  Perfil:      /etc/apparmor.d/usr.lib.cups.backend.cups-pdf
```

### Directorio local/ para customizaciones

El directorio `local/` permite al administrador **agregar reglas** sin
modificar el perfil original (que se sobrescribiría en actualizaciones):

```
Perfiles originales vs customizaciones locales
================================================

  /etc/apparmor.d/usr.sbin.nginx          ← Perfil del paquete
  │
  │  Contiene al final:
  │    #include <local/usr.sbin.nginx>
  │                  │
  │                  ▼
  /etc/apparmor.d/local/usr.sbin.nginx    ← Tus reglas adicionales
  │
  │  Puedes agregar:
  │    /srv/my-site/** r,
  │    /var/custom-logs/*.log w,

  Cuando el paquete se actualiza:
    ✓ El perfil original puede cambiar
    ✓ Tus reglas en local/ se PRESERVAN
```

### Directorio disable/ para deshabilitar perfiles

```bash
# Deshabilitar un perfil creando un symlink en disable/
ln -s /etc/apparmor.d/usr.sbin.nginx /etc/apparmor.d/disable/
apparmor_parser -R /etc/apparmor.d/usr.sbin.nginx

# Habilitar de nuevo eliminando el symlink
rm /etc/apparmor.d/disable/usr.sbin.nginx
apparmor_parser -a /etc/apparmor.d/usr.sbin.nginx
```

---

## 7. Perfiles en distribuciones

### Ubuntu / Debian

```bash
# Paquetes con perfiles
apt list --installed 2>/dev/null | grep apparmor

# Paquetes clave:
#   apparmor              → Motor y herramientas base
#   apparmor-utils        → aa-status, aa-enforce, aa-complain, aa-genprof
#   apparmor-profiles     → Perfiles adicionales (más servicios)
#   apparmor-profiles-extra → Aún más perfiles

# Instalar herramientas de gestión
sudo apt install apparmor-utils

# Instalar perfiles adicionales
sudo apt install apparmor-profiles apparmor-profiles-extra
```

### SUSE / openSUSE

```bash
# SUSE usa AppArmor por defecto
# Herramientas YaST integradas
sudo zypper install apparmor-utils apparmor-profiles

# YaST tiene módulo gráfico para AppArmor
yast2 apparmor
```

### RHEL / Fedora (AppArmor NO es el default)

```bash
# En RHEL/Fedora, SELinux es el default
# AppArmor se puede instalar pero NO es recomendable mezclar ambos

# Si necesitas AppArmor en Fedora (raro):
sudo dnf install apparmor apparmor-utils apparmor-profiles
# Requiere deshabilitar SELinux primero (no recomendado en RHEL)
```

> **Regla práctica**: usa el sistema MAC que trae tu distribución. No mezcles
> SELinux y AppArmor en el mismo sistema. Solo un LSM de tipo MAC puede estar
> activo a la vez en el kernel.

### Snap y AppArmor

En Ubuntu, **Snap** depende fuertemente de AppArmor para confinar
aplicaciones:

```
Snap y AppArmor
════════════════

  Cada snap tiene un perfil AppArmor auto-generado:
    snap.firefox.firefox
    snap.spotify.spotify
    snap.code.code

  Estos perfiles:
    ✓ Se generan automáticamente según los "plugs" del snap
    ✓ Se cargan al instalar el snap
    ✓ Se actualizan al actualizar el snap
    ✗ No deberían modificarse manualmente

  aa-status mostrará muchos perfiles snap en Ubuntu
```

---

## 8. AppArmor y systemd

### Verificar que AppArmor está activo

```bash
# Estado del servicio
systemctl status apparmor

# Salida típica:
# ● apparmor.service - Load AppArmor profiles
#    Loaded: loaded (/lib/systemd/system/apparmor.service; enabled)
#    Active: active (exited)
#    Docs: man:apparmor(7)

# "active (exited)" es normal — AppArmor carga los perfiles al arranque
# y luego el servicio termina. El módulo del kernel sigue activo.
```

### Habilitar/deshabilitar AppArmor

```bash
# Deshabilitar AppArmor completamente
sudo systemctl stop apparmor
sudo systemctl disable apparmor

# Habilitar AppArmor
sudo systemctl enable apparmor
sudo systemctl start apparmor
```

### Parámetros del kernel

```bash
# Deshabilitar AppArmor desde GRUB (temporal, para recovery)
# En la línea del kernel agregar:
apparmor=0

# Forzar todos los perfiles en complain (debugging global)
apparmor=1 security=apparmor

# Verificar parámetros activos
cat /sys/module/apparmor/parameters/enabled
# Y
cat /sys/module/apparmor/parameters/mode
```

### Logs de AppArmor

```bash
# Los eventos AppArmor van al log del kernel y a auditd (si está activo)

# En sistemas con auditd:
ausearch -m APPARMOR_DENIED -ts recent
ausearch -m APPARMOR_ALLOWED -ts recent   # Para perfiles en complain

# En sistemas sin auditd (van a syslog/journal):
journalctl -k | grep apparmor
journalctl -k | grep DENIED
dmesg | grep apparmor

# Ejemplo de entrada de log:
# audit: type=1400 audit(...): apparmor="DENIED"
#   operation="open" profile="/usr/sbin/nginx"
#   name="/etc/shadow" pid=1234 comm="nginx"
#   requested_mask="r" denied_mask="r" fsuid=33 ouid=0
```

Interpretar la entrada:

```
Anatomía de un log AppArmor
═════════════════════════════

  apparmor="DENIED"              ← DENIED (enforce) o ALLOWED (complain)
  operation="open"               ← Syscall: open, read, write, exec, mknod
  profile="/usr/sbin/nginx"      ← Perfil que bloqueó (ruta del ejecutable)
  name="/etc/shadow"             ← Recurso al que intentó acceder
  pid=1234                       ← PID del proceso
  comm="nginx"                   ← Nombre del comando
  requested_mask="r"             ← Permiso solicitado (r/w/a/l/k/m/x)
  denied_mask="r"                ← Permiso denegado
  fsuid=33                       ← UID del proceso (33 = www-data)
  ouid=0                         ← UID del propietario del archivo (0 = root)
```

---

## 9. Errores comunes

### Error 1: confundir el estado del módulo con el estado de los perfiles

```bash
# "AppArmor is loaded" NO significa que todos los servicios están confinados
# Solo significa que el módulo del kernel está activo

# Verificar QUÉ está confinado:
sudo aa-status
# → Ver la sección "processes in enforce mode"

# Un módulo cargado con 0 perfiles = AppArmor no protege nada
```

### Error 2: pensar que AppArmor tiene modo global enforce/permissive como SELinux

```bash
# MAL — buscar un equivalente a getenforce
getenforce    # Esto es SELinux, no AppArmor

# AppArmor NO tiene un switch global
# Cada perfil se gestiona individualmente:
#   aa-enforce /usr/sbin/nginx    → enforce solo nginx
#   aa-complain /usr/sbin/nginx   → complain solo nginx

# La granularidad per-perfil es una VENTAJA de AppArmor
```

### Error 3: olvidar instalar apparmor-utils

```bash
# aa-enforce, aa-complain, aa-genprof no están en el paquete base
aa-enforce /usr/sbin/nginx
# Command not found

# Solución
sudo apt install apparmor-utils    # Debian/Ubuntu
sudo zypper install apparmor-utils # SUSE
```

### Error 4: no recargar perfiles después de editarlos

```bash
# Editar un perfil en /etc/apparmor.d/ no tiene efecto inmediato
# El kernel usa la versión cargada en memoria

# Después de editar, recargar:
sudo apparmor_parser -r /etc/apparmor.d/usr.sbin.nginx
#                    ─┬─
#                     └── -r = replace (recargar perfil)

# O recargar todos los perfiles:
sudo systemctl reload apparmor
```

### Error 5: confundir "process unconfined with profile" con "no profile"

```bash
# aa-status puede mostrar:
#   "0 processes are unconfined but have a profile defined"

# Esto significa:
#   → El proceso está corriendo
#   → Existe un perfil para él
#   → PERO el perfil no está cargado (está en /etc/apparmor.d/disable/)
#   → El proceso corre SIN confinamiento AppArmor

# Es DIFERENTE de un proceso sin perfil definido
# Aquí el perfil EXISTE pero está deshabilitado → acción deliberada
```

---

## 10. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════╗
║              AppArmor Perfiles — Cheatsheet                    ║
╠══════════════════════════════════════════════════════════════════╣
║                                                                  ║
║  ESTADO                                                          ║
║  sudo aa-status              Estado completo                     ║
║  sudo aa-enabled             ¿Está habilitado? (para scripts)   ║
║  systemctl status apparmor   Estado del servicio                 ║
║  cat /sys/module/apparmor/parameters/enabled   Módulo kernel     ║
║                                                                  ║
║  MODOS DE PERFIL                                                 ║
║  enforce    → Bloquea y registra (producción)                    ║
║  complain   → Permite y registra (debugging)                     ║
║  unconfined → Sin perfil cargado                                 ║
║                                                                  ║
║  ARCHIVOS                                                        ║
║  /etc/apparmor.d/              Perfiles                          ║
║  /etc/apparmor.d/abstractions/ Fragmentos reutilizables          ║
║  /etc/apparmor.d/local/        Customizaciones locales           ║
║  /etc/apparmor.d/disable/      Symlinks → perfil deshabilitado   ║
║  /etc/apparmor.d/tunables/     Variables (HOME, etc.)            ║
║                                                                  ║
║  CONVENCIÓN DE NOMBRES                                           ║
║  /usr/sbin/nginx → /etc/apparmor.d/usr.sbin.nginx                ║
║  (reemplazar / por . en la ruta del ejecutable)                  ║
║                                                                  ║
║  LOGS                                                            ║
║  journalctl -k | grep apparmor    Log del kernel                 ║
║  ausearch -m APPARMOR_DENIED      Si auditd activo               ║
║  dmesg | grep apparmor            Alternativa                    ║
║                                                                  ║
║  PERMISOS EN REGLAS                                              ║
║  r=read  w=write  a=append  l=link  k=lock  m=mmap              ║
║  ix=inherit  px=profile  cx=child  ux=unconfined                 ║
║                                                                  ║
║  GLOBS                                                           ║
║  *   → un nivel de directorio                                    ║
║  **  → recursivo (todos los subdirectorios)                      ║
║                                                                  ║
║  COMPARACIÓN RÁPIDA                                              ║
║  SELinux enforce global  ↔  AppArmor: no existe (per-perfil)     ║
║  SELinux permissive/domain ↔ AppArmor: complain per-perfil       ║
║  setenforce 0            ↔  No equivalente directo               ║
║  getenforce              ↔  aa-enabled + aa-status               ║
║                                                                  ║
╚══════════════════════════════════════════════════════════════════╝
```

---

## 11. Ejercicios

### Ejercicio 1: Interpretar aa-status

Dada la siguiente salida de `aa-status`:

```
apparmor module is loaded.
22 profiles are loaded.
  18 profiles are in enforce mode.
  4 profiles are in complain mode.
3 processes have profiles defined.
  2 processes are in enforce mode.
     /usr/sbin/cupsd (1100)
     /usr/sbin/mysqld (2200)
  0 processes are in complain mode.
  1 processes are unconfined but have a profile defined.
     /usr/sbin/nginx (3300)
```

> **Pregunta de predicción**: ¿nginx (PID 3300) está protegido por AppArmor?
> ¿Por qué aparece como "unconfined but have a profile defined"? ¿Qué harías
> para confinarlo?
>
> **Respuesta**: nginx NO está protegido. Tiene un perfil definido en
> `/etc/apparmor.d/` pero el perfil no está cargado en el kernel (probablemente
> hay un symlink en `/etc/apparmor.d/disable/`). Para confinarlo: eliminar el
> symlink de `disable/`, cargar el perfil con `apparmor_parser -a
> /etc/apparmor.d/usr.sbin.nginx`, y reiniciar nginx.

### Ejercicio 2: Identificar el modo de un perfil

```bash
# Ejecuta estos comandos y analiza:
sudo aa-status | grep -A2 "profiles are in enforce"
sudo aa-status | grep -A2 "profiles are in complain"
```

> **Pregunta de predicción**: si un perfil aparece en la lista de "enforce
> mode" pero el proceso NO aparece en "processes in enforce mode", ¿significa
> que hay un problema?
>
> **Respuesta**: no necesariamente. Significa que el perfil está cargado y
> listo, pero el proceso no está corriendo en este momento. Cuando el proceso
> arranque, será confinado automáticamente. Es normal para servicios que no
> están activos (ej: tcpdump tiene perfil enforce pero solo corre cuando el
> admin lo ejecuta).

### Ejercicio 3: Explorar los perfiles instalados

```bash
# Paso 1: Ver cuántos perfiles hay
ls /etc/apparmor.d/ | grep -v -E '^(abstractions|tunables|local|disable|force-complain|lxc|abi)$' | head -20

# Paso 2: Leer un perfil sencillo
cat /etc/apparmor.d/usr.sbin.tcpdump
```

> **Pregunta de predicción**: en el perfil de tcpdump, ¿esperarías ver
> permisos de escritura (`w`) en `/etc/`? ¿Qué permisos de red esperarías?
>
> **Respuesta**: no, tcpdump no necesita escribir en `/etc/`. Esperarías ver
> `r` (lectura) para archivos de configuración y `network raw` o `network
> packet` para capturar tráfico (necesita sockets raw). También `capability
> net_raw` y posiblemente `capability net_admin`. Y escritura (`w`) solo en
> el directorio donde guarda las capturas (ej: `/tmp/` o ruta especificada).

---

> **Siguiente tema**: T02 — Gestión de perfiles (aa-enforce, aa-complain,
> aa-disable, apparmor_parser).
