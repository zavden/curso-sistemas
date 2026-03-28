# SELinux vs AppArmor: comparación práctica

## Índice

1. [Diferencia arquitectónica fundamental](#1-diferencia-arquitectónica-fundamental)
2. [Comparación de conceptos](#2-comparación-de-conceptos)
3. [Comparación de operaciones diarias](#3-comparación-de-operaciones-diarias)
4. [Comparación de troubleshooting](#4-comparación-de-troubleshooting)
5. [Fortalezas y debilidades](#5-fortalezas-y-debilidades)
6. [Cuándo encontrar cada uno](#6-cuándo-encontrar-cada-uno)
7. [Migración mental entre ambos](#7-migración-mental-entre-ambos)
8. [Escenarios prácticos comparados](#8-escenarios-prácticos-comparados)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Diferencia arquitectónica fundamental

Ambos son sistemas MAC sobre LSM, pero con enfoques opuestos para identificar
objetos:

```
Etiquetas vs Rutas — la divergencia raíz
══════════════════════════════════════════

  SELinux: ¿QUÉ es este objeto?
  ─────────────────────────────
  Cada archivo lleva una etiqueta (xattr) que define su identidad.
  La etiqueta viaja CON el archivo.

    /var/www/html/index.html → httpd_sys_content_t
    (mueves el archivo a /tmp/ → sigue siendo httpd_sys_content_t)


  AppArmor: ¿DÓNDE está este objeto?
  ───────────────────────────────────
  La identidad del archivo es su ruta.
  No hay etiqueta persistente.

    /var/www/html/index.html → permitido por regla "/var/www/html/** r,"
    (mueves el archivo a /tmp/ → ya no coincide con la regla)
```

```
Consecuencias de cada enfoque
══════════════════════════════

  Archivo movido con mv:
  ┌────────────┬────────────────────────────┬──────────────────────┐
  │            │ SELinux                    │ AppArmor             │
  ├────────────┼────────────────────────────┼──────────────────────┤
  │ mv A → B   │ Conserva etiqueta de A    │ Ahora tiene ruta B   │
  │            │ (puede ser problema)       │ (se evalúa con B)    │
  │ Solución   │ restorecon -Rv B           │ Nada que hacer       │
  └────────────┴────────────────────────────┴──────────────────────┘

  Hardlink:
  ┌────────────┬────────────────────────────┬──────────────────────┐
  │            │ SELinux                    │ AppArmor             │
  ├────────────┼────────────────────────────┼──────────────────────┤
  │ ln A B     │ Misma etiqueta (misma     │ Dos rutas diferentes │
  │            │  protección)               │ (puede evadir regla) │
  │ Riesgo     │ Ninguno                    │ Posible evasión      │
  └────────────┴────────────────────────────┴──────────────────────┘

  Filesystem sin xattr (NFS, CIFS):
  ┌────────────┬────────────────────────────┬──────────────────────┐
  │            │ SELinux                    │ AppArmor             │
  ├────────────┼────────────────────────────┼──────────────────────┤
  │            │ Necesita context= en mount │ Funciona sin cambios │
  │            │ o nfs_t genérico           │ (solo evalúa rutas)  │
  └────────────┴────────────────────────────┴──────────────────────┘
```

---

## 2. Comparación de conceptos

### Mapeo término a término

```
┌─────────────────────┬──────────────────────┬─────────────────────┐
│ Concepto            │ SELinux              │ AppArmor            │
├─────────────────────┼──────────────────────┼─────────────────────┤
│ Identidad de        │ Tipo (type_t)        │ Ruta (/path/**)     │
│ un objeto           │ en xattr             │ en perfil           │
├─────────────────────┼──────────────────────┼─────────────────────┤
│ Identidad de        │ Dominio (domain_t)   │ Perfil              │
│ un proceso          │                      │ (/path/ejecutable)  │
├─────────────────────┼──────────────────────┼─────────────────────┤
│ Política            │ Módulos compilados   │ Archivos de texto   │
│                     │ (.pp)                │ en /etc/apparmor.d/ │
├─────────────────────┼──────────────────────┼─────────────────────┤
│ Modo permisivo      │ setenforce 0         │ aa-complain /path   │
│ (global)            │ (afecta TODO)        │ (NO hay global*)    │
├─────────────────────┼──────────────────────┼─────────────────────┤
│ Modo permisivo      │ semanage permissive  │ aa-complain /path   │
│ (por servicio)      │ -a httpd_t           │                     │
├─────────────────────┼──────────────────────┼─────────────────────┤
│ Desactivar          │ SELINUX=disabled     │ systemctl disable   │
│ completamente       │ + reboot             │ apparmor + reboot   │
├─────────────────────┼──────────────────────┼─────────────────────┤
│ Opciones on/off     │ Booleans             │ No hay equivalente  │
│ en la política      │ setsebool -P         │ (editar perfil)     │
├─────────────────────┼──────────────────────┼─────────────────────┤
│ Control de puertos  │ semanage port        │ Reglas network      │
│                     │ (tipos por puerto)   │ (sin granularidad   │
│                     │                      │  por puerto)        │
├─────────────────────┼──────────────────────┼─────────────────────┤
│ Transiciones de     │ type_transition      │ px / cx / ix        │
│ ejecución           │ (automáticas)        │ (en regla de ruta)  │
├─────────────────────┼──────────────────────┼─────────────────────┤
│ Contexto usuario    │ user_u, staff_u,     │ No existe           │
│                     │ sysadm_u             │                     │
├─────────────────────┼──────────────────────┼─────────────────────┤
│ Niveles de          │ MLS (s0-s15:c0-c1023)│ No soportado        │
│ seguridad           │                      │                     │
├─────────────────────┼──────────────────────┼─────────────────────┤
│ Etiquetar archivos  │ restorecon / chcon   │ No aplica           │
│                     │ semanage fcontext    │ (basado en ruta)    │
├─────────────────────┼──────────────────────┼─────────────────────┤
│ Ver estado          │ getenforce           │ aa-status            │
│                     │ sestatus             │ aa-enabled           │
├─────────────────────┼──────────────────────┼─────────────────────┤
│ Inspeccionar        │ ls -Z, ps -eZ        │ aa-status            │
│ contextos           │ id -Z, ss -Z         │ (no hay -Z)         │
└─────────────────────┴──────────────────────┴─────────────────────┘

* Se puede pasar apparmor=complain en kernel, pero no es práctica habitual
```

### Modelo de confinamiento

```
Qué se confina por defecto
═══════════════════════════

  SELinux (targeted):
  ───────────────────
  ~200 servicios de red confinados por defecto
  Usuarios → unconfined_t (no confinados)
  Scripts manuales → unconfined_t

  Política preinstalada cubre la mayoría de servicios.
  Casi nunca necesitas CREAR política nueva.
  Gestión = booleans + file contexts + ports.


  AppArmor:
  ─────────
  Solo programas con perfil definido están confinados
  Sin perfil = sin confinamiento AppArmor
  Distribución incluye 20-50 perfiles base

  Cuando instalas un servicio nuevo:
    ¿El paquete trae perfil? → confinado
    ¿No trae perfil? → sin confinar (necesitas crear uno)

  Gestión = crear perfiles + ajustar reglas.
```

---

## 3. Comparación de operaciones diarias

### Cambiar modo de un servicio

```bash
# ═══════ SELinux ═══════

# Ver modo actual
getenforce
# Enforcing

# Permissive global (afecta TODO)
sudo setenforce 0

# Permissive solo para Apache (mucho más seguro)
sudo semanage permissive -a httpd_t

# Volver a enforcing
sudo semanage permissive -d httpd_t


# ═══════ AppArmor ═══════

# Ver estado
sudo aa-status

# Complain solo para nginx (per-perfil por naturaleza)
sudo aa-complain /usr/sbin/nginx

# Volver a enforce
sudo aa-enforce /usr/sbin/nginx

# No hay equivalente de "permissive global" habitual
```

### Permitir acceso a un directorio nuevo

```bash
# ═══════ SELinux ═══════

# Apache necesita servir /opt/webapp/
# 1. Asignar el tipo correcto al directorio
sudo semanage fcontext -a -t httpd_sys_content_t "/opt/webapp(/.*)?"
# 2. Aplicar la etiqueta
sudo restorecon -Rv /opt/webapp/

# Dos pasos: definir tipo + aplicar


# ═══════ AppArmor ═══════

# Nginx necesita servir /opt/webapp/
# 1. Agregar regla en local/
echo '/opt/webapp/** r,' | sudo tee -a /etc/apparmor.d/local/usr.sbin.nginx
# 2. Recargar perfil
sudo apparmor_parser -r /etc/apparmor.d/usr.sbin.nginx

# Dos pasos: agregar regla + recargar
```

### Permitir un puerto no estándar

```bash
# ═══════ SELinux ═══════

# Apache en puerto 8888
sudo semanage port -a -t http_port_t -p tcp 8888
# Una operación — el tipo del puerto controla el acceso


# ═══════ AppArmor ═══════

# Nginx en puerto 8888
# Si el perfil tiene: network inet tcp,
# → Ya puede escuchar en CUALQUIER puerto TCP
# → No hay granularidad per-puerto

# AppArmor no controla qué puertos específicos usa un servicio
# (solo si puede usar red o no, y qué protocolo)
```

```
Granularidad de control de red
═══════════════════════════════

  SELinux:
    ✓ Puerto 80  → http_port_t     → httpd_t puede bind
    ✗ Puerto 22  → ssh_port_t      → httpd_t NO puede bind
    ✗ Puerto 9999 → reserved_port_t → httpd_t NO puede bind
    → Control POR PUERTO

  AppArmor:
    network inet tcp,    → puede usar CUALQUIER puerto TCP
    network inet udp,    → puede usar CUALQUIER puerto UDP
    (sin regla network)  → NO puede usar red
    → Control POR PROTOCOLO (todo o nada en ese protocolo)

  Implicación: SELinux es significativamente más granular en red
```

### Habilitar una funcionalidad opcional

```bash
# ═══════ SELinux ═══════

# Apache conectar a base de datos
sudo setsebool -P httpd_can_network_connect_db on
# Un boolean — la política ya tiene las reglas, solo activas el switch


# ═══════ AppArmor ═══════

# No hay concepto de booleans
# Necesitas agregar las reglas explícitamente:
echo 'network inet tcp,' | sudo tee -a /etc/apparmor.d/local/usr.sbin.nginx
# O ser más específico con reglas de socket Unix
# No hay un "switch" predefinido por los desarrolladores de la política
```

---

## 4. Comparación de troubleshooting

### Encontrar denegaciones

```bash
# ═══════ SELinux ═══════

# Log: /var/log/audit/audit.log
# Formato:
#   type=AVC msg=audit(...): avc:  denied  { read }
#     for  pid=1234 comm="httpd"
#     scontext=system_u:system_r:httpd_t:s0
#     tcontext=system_u:object_r:var_t:s0
#     tclass=file permissive=0

# Herramientas:
ausearch -m AVC -ts recent
ausearch -m AVC -ts recent --raw | audit2why
sealert -a /var/log/audit/audit.log


# ═══════ AppArmor ═══════

# Log: kernel log / journal
# Formato:
#   audit: type=1400 audit(...): apparmor="DENIED"
#     operation="open" profile="/usr/sbin/nginx"
#     name="/opt/secret" pid=1234 comm="nginx"
#     requested_mask="r" denied_mask="r"

# Herramientas:
journalctl -k | grep DENIED
ausearch -m APPARMOR_DENIED -ts recent
sudo aa-logprof
```

### Legibilidad de los logs

```
Comparación de legibilidad
═══════════════════════════

  SELinux AVC:
  ────────────
  denied { read } scontext=...httpd_t... tcontext=...var_t... tclass=file

  Preguntas que debes responder:
    "¿Qué archivo tiene tipo var_t?"
    "¿Debería ser httpd_sys_content_t?"
    → Necesitas semanage fcontext -l y matchpathcon
    → Requiere entender el sistema de tipos


  AppArmor DENIED:
  ────────────────
  operation="open" profile="/usr/sbin/nginx" name="/opt/secret"

  Preguntas que debes responder:
    "¿Debería nginx poder leer /opt/secret?"
    → Si sí, agregar /opt/secret r, al perfil
    → La ruta es directamente visible en el log

  AppArmor es más fácil de leer y diagnosticar.
  SELinux es más potente pero más opaco.
```

### Flujo de resolución

```
Flujo SELinux:
═══════════════

  AVC denied
    │
    ▼
  audit2why ──► "boolean httpd_can_network_connect"
    │              → setsebool -P httpd_can_network_connect on
    │
    ├──► "missing TE rule" + tclass=file
    │       → ¿File context incorrecto?
    │       → semanage fcontext + restorecon
    │
    ├──► "missing TE rule" + name_bind
    │       → semanage port -a
    │
    └──► Nada claro → audit2allow (último recurso)


Flujo AppArmor:
════════════════

  DENIED en log
    │
    ▼
  Leer el log directamente
    │
    ├── name="/ruta/del/archivo"
    │   requested_mask="r"
    │     → Agregar "/ruta/del/archivo r," al perfil/local
    │
    ├── operation="connect"
    │     → Agregar "network inet tcp," al perfil
    │
    └── operation="exec"
          → Agregar "/ruta ix," o "/ruta px," al perfil
    │
    ▼
  apparmor_parser -r PERFIL
```

---

## 5. Fortalezas y debilidades

```
┌──────────────────────┬──────────────────────┬──────────────────────┐
│ Aspecto              │ SELinux              │ AppArmor             │
├──────────────────────┼──────────────────────┼──────────────────────┤
│ Curva de             │ Pronunciada          │ Gradual              │
│ aprendizaje          │ (semanas-meses)      │ (días-semanas)       │
├──────────────────────┼──────────────────────┼──────────────────────┤
│ Granularidad         │ Muy alta             │ Media                │
│                      │ (user+role+type+lvl) │ (ruta+permisos)      │
├──────────────────────┼──────────────────────┼──────────────────────┤
│ Control de red       │ Per-puerto           │ Per-protocolo        │
│                      │ (http_port_t)        │ (network inet tcp)   │
├──────────────────────┼──────────────────────┼──────────────────────┤
│ Política por defecto │ ~200 dominios        │ 20-50 perfiles       │
│ preinstalada         │ preconfigurados      │ básicos              │
├──────────────────────┼──────────────────────┼──────────────────────┤
│ Crear política nueva │ Complejo (audit2allow│ Más simple           │
│                      │ módulos .te/.pp)     │ (texto plano)        │
├──────────────────────┼──────────────────────┼──────────────────────┤
│ Legibilidad de       │ Baja (tipos opacos)  │ Alta (rutas legibles)│
│ la política          │                      │                      │
├──────────────────────┼──────────────────────┼──────────────────────┤
│ MLS (Multi-Level     │ Sí (clasificado,     │ No                   │
│ Security)            │ secreto, etc.)       │                      │
├──────────────────────┼──────────────────────┼──────────────────────┤
│ Confinamiento de     │ Sí (user_u, staff_u, │ No                   │
│ usuarios             │ guest_u)             │                      │
├──────────────────────┼──────────────────────┼──────────────────────┤
│ Soporte NFS/CIFS     │ Requiere context=    │ Nativo (basado en    │
│                      │ en mount             │ ruta)                │
├──────────────────────┼──────────────────────┼──────────────────────┤
│ Resistencia a        │ Alta (etiqueta en    │ Baja (hardlinks      │
│ hardlinks            │ inode)               │ crean ruta nueva)    │
├──────────────────────┼──────────────────────┼──────────────────────┤
│ Impacto en           │ Mayor (evalúa xattr  │ Menor (solo evalúa   │
│ rendimiento          │ en cada acceso)      │ ruta)                │
├──────────────────────┼──────────────────────┼──────────────────────┤
│ Troubleshooting      │ audit2why + sealert  │ Leer log + editar    │
│                      │ (herramientas ricas) │ perfil (más directo) │
├──────────────────────┼──────────────────────┼──────────────────────┤
│ Containers           │ Soporte maduro       │ Soporte creciente    │
│                      │ (container_t, sVirt) │ (perfiles por        │
│                      │                      │  contenedor)         │
├──────────────────────┼──────────────────────┼──────────────────────┤
│ Documentación        │ Extensa (Red Hat)    │ Buena (Canonical,    │
│                      │                      │ SUSE)                │
├──────────────────────┼──────────────────────┼──────────────────────┤
│ Certificaciones      │ Requerido en RHCSA/  │ Puede aparecer en    │
│                      │ RHCE/LPIC-2          │ LPIC-2               │
└──────────────────────┴──────────────────────┴──────────────────────┘
```

### Resumen en una frase

```
SELinux = más potente, más complejo, controla MÁS aspectos.
AppArmor = más accesible, más legible, controla lo esencial.

Analogía:
  SELinux  ≈ iptables/nftables  (granular, complejo, poderoso)
  AppArmor ≈ ufw/firewalld      (amigable, cubre el 90%, limitado)
```

---

## 6. Cuándo encontrar cada uno

### Por distribución

```
Distribución → sistema MAC
═════════════════════════════

  Te sientas frente a un servidor...

  "¿Qué distribución es?"
  │
  ├── RHEL / CentOS / Rocky / Alma / Fedora
  │   Oracle Linux / Amazon Linux
  │     → SELinux (enforcing por defecto)
  │     → getenforce para verificar
  │
  ├── Ubuntu / Debian / Linux Mint / Pop!_OS
  │     → AppArmor (habilitado por defecto)
  │     → sudo aa-status para verificar
  │
  ├── SUSE / openSUSE
  │     → AppArmor (habilitado por defecto)
  │     → YaST tiene módulo para AppArmor
  │
  └── Arch / Gentoo / Slackware
        → Probablemente ninguno por defecto
        → Verificar ambos: getenforce && aa-enabled
```

### Por entorno

```
Entorno → sistema MAC habitual
═════════════════════════════════

  Servidores corporativos (enterprise)
    → SELinux (RHEL domina en enterprise)

  Cloud (AWS, GCP, Azure)
    → Amazon Linux / RHEL → SELinux
    → Ubuntu → AppArmor
    → Depende de la imagen base

  Contenedores (Docker, Podman)
    → Host RHEL → SELinux (container_t, sVirt)
    → Host Ubuntu → AppArmor (perfil docker-default)

  Kubernetes
    → Soporta ambos (pod securityContext)
    → RHEL nodes → SELinux
    → Ubuntu nodes → AppArmor

  Escritorio Linux
    → Ubuntu/Debian → AppArmor (confina Firefox, Evince)
    → Fedora → SELinux (confina servicios)

  IoT / Embedded
    → AppArmor (más ligero, sin necesidad de xattr)
    → Snap en Ubuntu Core → AppArmor obligatorio
```

### Por requisito

```
Requisito → recomendación
══════════════════════════

  "Necesito MLS (niveles de clasificación)"
    → SELinux (único que lo soporta)

  "Necesito confinar usuarios (no solo servicios)"
    → SELinux (user_u, staff_u, guest_u)

  "Necesito controlar puertos por número"
    → SELinux (semanage port)

  "Necesito confinar un servicio custom rápido"
    → AppArmor (aa-genprof → perfil en minutos)

  "Tengo NFS/CIFS como almacenamiento"
    → AppArmor (sin complicaciones de xattr)

  "Necesito cumplir con Common Criteria / FIPS"
    → SELinux (más certificaciones formales)

  "El equipo tiene poca experiencia en MAC"
    → AppArmor (curva de aprendizaje menor)
```

---

## 7. Migración mental entre ambos

Si ya sabes uno y necesitas aprender el otro, estas equivalencias te ayudan:

### De SELinux a AppArmor

```
Sé SELinux, necesito usar AppArmor
════════════════════════════════════

  "Quiero ver si MAC está activo"
    SELinux:  getenforce; sestatus
    AppArmor: aa-enabled; sudo aa-status

  "Quiero ver el contexto de un proceso"
    SELinux:  ps -eZ | grep nginx
    AppArmor: sudo aa-status | grep nginx

  "Un servicio falla, ¿es MAC?"
    SELinux:  setenforce 0; probar; setenforce 1
    AppArmor: sudo aa-complain /usr/sbin/X; probar; sudo aa-enforce /usr/sbin/X

  "¿Dónde están los logs?"
    SELinux:  /var/log/audit/audit.log (AVC)
    AppArmor: journalctl -k | grep apparmor (DENIED)

  "Necesito permitir acceso a /opt/data/"
    SELinux:  semanage fcontext -a -t tipo "/opt/data(/.*)?"
              restorecon -Rv /opt/data/
    AppArmor: echo '/opt/data/** r,' >> /etc/apparmor.d/local/usr.sbin.X
              apparmor_parser -r /etc/apparmor.d/usr.sbin.X

  "Necesito habilitar una funcionalidad"
    SELinux:  setsebool -P boolean_name on
    AppArmor: No hay booleans → editar perfil directamente

  "Necesito permitir un puerto"
    SELinux:  semanage port -a -t tipo -p tcp PUERTO
    AppArmor: Si ya tiene "network inet tcp," → nada que hacer
              Si no → agregar regla de red al perfil

  Lo que NO encontrarás en AppArmor:
    × No hay user:role:type:level (cuatro campos)
    × No hay booleans (switches on/off)
    × No hay semanage (no hay base de datos de contextos)
    × No hay -Z en ls, ps, id, ss
    × No hay relabeling (no hay etiquetas)
```

### De AppArmor a SELinux

```
Sé AppArmor, necesito usar SELinux
════════════════════════════════════

  "Quiero ver si MAC está activo"
    AppArmor: aa-enabled; sudo aa-status
    SELinux:  getenforce; sestatus

  "Quiero poner un servicio en modo permisivo"
    AppArmor: sudo aa-complain /usr/sbin/X
    SELinux:  sudo semanage permissive -a dominio_t
              (NO setenforce 0, que es global)

  "Algo falla, ¿qué necesito permitir?"
    AppArmor: log muestra name="/ruta" → agregar "/ruta r,"
    SELinux:  ausearch -m AVC --raw | audit2why
              → Te dice boolean / file context / port / audit2allow

  "Quiero crear política para un servicio nuevo"
    AppArmor: aa-genprof (interactivo, rápido)
    SELinux:  audit2allow -M modulo (más complejo, menos interactivo)
              O buscar un módulo existente en la política

  Lo que SÍ encontrarás en SELinux (no existe en AppArmor):
    ✓ Cuatro campos: user:role:type:level
    ✓ Booleans: switches on/off preconfigurados
    ✓ Control de puertos por número
    ✓ Confinamiento de usuarios
    ✓ MLS para entornos clasificados
    ✓ -Z en ls, ps, id, ss (inspección de contextos)
    ✓ restorecon (reparar etiquetas)
```

---

## 8. Escenarios prácticos comparados

### Escenario 1: servicio web en directorio no estándar

```
Situación: Apache/Nginx sirve desde /opt/webapp/

═══════ SELinux (RHEL) ═══════

  # 1. Crear el directorio y copiar archivos
  cp -r /var/www/html/* /opt/webapp/

  # 2. ¿Funciona? No — /opt/webapp/ tiene tipo default_t
  ls -Zd /opt/webapp/
  # drwxr-xr-x. root root unconfined_u:object_r:usr_t:s0 /opt/webapp/

  # 3. Asignar tipo correcto
  semanage fcontext -a -t httpd_sys_content_t "/opt/webapp(/.*)?"
  restorecon -Rv /opt/webapp/

  # 4. Verificar
  ls -Zd /opt/webapp/
  # ...httpd_sys_content_t...


═══════ AppArmor (Ubuntu) ═══════

  # 1. Crear el directorio y copiar archivos
  cp -r /var/www/html/* /opt/webapp/

  # 2. ¿Funciona? No — el perfil de nginx no incluye /opt/webapp/

  # 3. Agregar regla
  echo '/opt/webapp/** r,' >> /etc/apparmor.d/local/usr.sbin.nginx

  # 4. Recargar
  apparmor_parser -r /etc/apparmor.d/usr.sbin.nginx
```

### Escenario 2: SSH en puerto no estándar

```
Situación: mover SSH del puerto 22 al 2222

═══════ SELinux (RHEL) ═══════

  # 1. Editar sshd_config
  #    Port 2222

  # 2. Agregar tipo de puerto
  semanage port -a -t ssh_port_t -p tcp 2222

  # 3. Firewall
  firewall-cmd --add-port=2222/tcp --permanent
  firewall-cmd --reload

  # 4. Reiniciar
  systemctl restart sshd


═══════ AppArmor (Ubuntu) ═══════

  # 1. Editar sshd_config
  #    Port 2222

  # 2. ¿SELinux port? No hay equivalente
  #    Si sshd tiene perfil con "network inet tcp," → ya puede
  #    Si no tiene perfil → nada que hacer con AppArmor

  # 3. Firewall
  ufw allow 2222/tcp

  # 4. Reiniciar
  systemctl restart sshd

  # AppArmor no controla puertos individualmente
```

### Escenario 3: diagnosticar por qué un servicio no funciona

```
Situación: el servicio no arranca o da errores

═══════ SELinux (RHEL) ═══════

  # 1. ¿Es SELinux?
  setenforce 0
  systemctl restart myservice
  # ¿Funciona? → Es SELinux
  setenforce 1

  # 2. Buscar AVC
  ausearch -m AVC -ts recent -c myservice

  # 3. Entender causa
  ausearch -m AVC -ts recent --raw | audit2why

  # 4. Solucionar según audit2why:
  #    boolean → setsebool -P
  #    file context → semanage fcontext + restorecon
  #    port → semanage port -a
  #    ? → audit2allow -M (último recurso)


═══════ AppArmor (Ubuntu) ═══════

  # 1. ¿Es AppArmor?
  sudo aa-complain /usr/sbin/myservice
  sudo systemctl restart myservice
  # ¿Funciona? → Es AppArmor
  sudo aa-enforce /usr/sbin/myservice

  # 2. Buscar DENIED
  journalctl -k --since "5 min ago" | grep DENIED | grep myservice

  # 3. El log dice directamente qué falta:
  #    name="/ruta" requested_mask="r"
  #    → Agregar /ruta r, al perfil

  # 4. Agregar regla en local/ y recargar
```

### Escenario 4: contenedores

```
Situación: contenedor Docker necesita acceder a un volumen

═══════ SELinux (RHEL) ═══════

  # Los contenedores corren como container_t
  # Volúmenes necesitan tipo container_file_t

  # Opción 1: :Z en el bind mount (re-etiqueta)
  docker run -v /opt/data:/data:Z myimage

  # Opción 2: manual
  chcon -Rt container_file_t /opt/data/

  # SELinux aísla contenedores entre sí con sVirt (MCS labels)


═══════ AppArmor (Ubuntu) ═══════

  # Docker genera perfil "docker-default" para contenedores
  # Volúmenes bind-mount heredan acceso del perfil

  # Si necesitas permisos extra:
  docker run --security-opt apparmor=unconfined myimage
  # (desactiva AppArmor para ese contenedor — no recomendado)

  # O crear perfil custom:
  docker run --security-opt apparmor=my-custom-profile myimage
```

---

## 9. Errores comunes

### Error 1: intentar usar comandos de SELinux en sistemas AppArmor

```bash
# En Ubuntu:
getenforce
# -bash: getenforce: command not found

sestatus
# -bash: sestatus: command not found

ls -Z /var/www/
# (muestra ? en la columna de contexto — sin sentido)

# SELinux no está instalado/activo en Ubuntu
# Usar: sudo aa-status
```

### Error 2: intentar usar comandos de AppArmor en sistemas SELinux

```bash
# En RHEL:
aa-status
# -bash: aa-status: command not found

aa-enforce /usr/sbin/httpd
# -bash: aa-enforce: command not found

# AppArmor no está instalado/activo en RHEL
# Usar: getenforce; sestatus
```

### Error 3: pensar que AppArmor tiene booleans

```bash
# En AppArmor NO existe:
setsebool -P httpd_can_network_connect on    # ← Esto es SELinux

# En AppArmor, para habilitar funcionalidad:
# Editar el perfil directamente (agregar reglas en local/)
# No hay switches on/off predefinidos
```

### Error 4: asumir que ambos controlan puertos igual

```bash
# SELinux: granularidad per-puerto
semanage port -a -t http_port_t -p tcp 8888
# → httpd_t puede bind en 8888, pero NO en 9999

# AppArmor: granularidad per-protocolo
# network inet tcp,
# → nginx puede bind en CUALQUIER puerto TCP
# → No hay forma de restringir a puertos específicos
#   (el firewall y DAC sí pueden)
```

### Error 5: mezclar SELinux y AppArmor en el mismo sistema

```bash
# Solo un LSM de tipo MAC puede estar activo

# Intentar instalar AppArmor en RHEL con SELinux activo:
# → Conflictos, comportamiento impredecible
# → El kernel solo carga un módulo MAC a la vez

# Si necesitas cambiar:
# 1. Deshabilitar el actual completamente
# 2. Instalar el nuevo
# 3. Reboot
# → No recomendado en producción
```

---

## 10. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════════╗
║                SELinux vs AppArmor — Cheatsheet comparativo            ║
╠══════════════════════════════════════════════════════════════════════════╣
║                                                                          ║
║  VERIFICAR ESTADO                                                        ║
║  SELinux:  getenforce / sestatus                                         ║
║  AppArmor: aa-enabled / sudo aa-status                                   ║
║                                                                          ║
║  MODO PERMISIVO POR SERVICIO                                             ║
║  SELinux:  semanage permissive -a dominio_t                              ║
║  AppArmor: aa-complain /usr/sbin/X                                       ║
║                                                                          ║
║  MODO ENFORCING POR SERVICIO                                             ║
║  SELinux:  semanage permissive -d dominio_t                              ║
║  AppArmor: aa-enforce /usr/sbin/X                                        ║
║                                                                          ║
║  PERMITIR ACCESO A DIRECTORIO                                            ║
║  SELinux:  semanage fcontext -a -t tipo "/ruta(/.*)?"                    ║
║            restorecon -Rv /ruta/                                         ║
║  AppArmor: echo '/ruta/** r,' >> .../local/usr.sbin.X                    ║
║            apparmor_parser -r .../usr.sbin.X                             ║
║                                                                          ║
║  PERMITIR PUERTO NO ESTÁNDAR                                             ║
║  SELinux:  semanage port -a -t tipo -p tcp PUERTO                        ║
║  AppArmor: (network inet tcp, ya permite todos los puertos TCP)          ║
║                                                                          ║
║  HABILITAR FUNCIONALIDAD                                                 ║
║  SELinux:  setsebool -P boolean_name on                                  ║
║  AppArmor: Editar perfil (no hay booleans)                               ║
║                                                                          ║
║  VER DENEGACIONES                                                        ║
║  SELinux:  ausearch -m AVC -ts recent                                    ║
║  AppArmor: journalctl -k | grep DENIED                                  ║
║                                                                          ║
║  DIAGNOSTICAR                                                            ║
║  SELinux:  ausearch --raw | audit2why                                    ║
║  AppArmor: El log muestra ruta + permiso directamente                    ║
║                                                                          ║
║  CREAR POLÍTICA NUEVA                                                    ║
║  SELinux:  audit2allow -M mod; semodule -i mod.pp                        ║
║  AppArmor: aa-genprof /usr/sbin/X (interactivo)                         ║
║                                                                          ║
║  DISTRIBUCIONES                                                          ║
║  SELinux:  RHEL, Fedora, CentOS, Rocky, Alma, Amazon Linux              ║
║  AppArmor: Ubuntu, Debian, SUSE, openSUSE, Mint                         ║
║                                                                          ║
║  REGLA DE ORO                                                            ║
║  → Usa el MAC que trae tu distribución                                   ║
║  → No mezcles ambos en el mismo sistema                                  ║
║  → SELinux = más potente, más complejo                                   ║
║  → AppArmor = más accesible, suficiente para la mayoría                 ║
║                                                                          ║
╚══════════════════════════════════════════════════════════════════════════╝
```

---

## 11. Ejercicios

### Ejercicio 1: Identificar el sistema MAC

**Escenario**: te conectas por SSH a cuatro servidores diferentes y necesitas
determinar qué sistema MAC usa cada uno.

```bash
# Servidor A:
cat /etc/os-release | head -1
# PRETTY_NAME="Rocky Linux 9.3"

# Servidor B:
cat /etc/os-release | head -1
# PRETTY_NAME="Ubuntu 24.04 LTS"

# Servidor C:
cat /etc/os-release | head -1
# PRETTY_NAME="SUSE Linux Enterprise Server 15 SP5"

# Servidor D:
cat /etc/os-release | head -1
# PRETTY_NAME="Arch Linux"
```

> **Pregunta de predicción**: para cada servidor, ¿qué comando ejecutarías
> primero para verificar el estado del MAC? ¿Qué sistema esperarías
> encontrar en cada uno?
>
> **Respuesta**:
> - **A (Rocky)**: `getenforce` → esperarías SELinux Enforcing.
> - **B (Ubuntu)**: `sudo aa-status` → esperarías AppArmor con perfiles
>   cargados.
> - **C (SUSE)**: `sudo aa-status` → esperarías AppArmor.
> - **D (Arch)**: ambos comandos — `getenforce` y `aa-enabled` —
>   probablemente ninguno esté activo por defecto. Verificarías también
>   `cat /sys/kernel/security/lsm` para ver qué LSMs están cargados.

### Ejercicio 2: Resolver el mismo problema en ambos sistemas

**Escenario**: un servidor web necesita servir archivos desde
`/data/websites/production/`. Resuelve el problema en ambos sistemas.

```bash
# ═══════ En RHEL (SELinux) ═══════
# Paso 1: Verificar contexto actual
ls -Zd /data/websites/production/

# Paso 2: Aplicar tipo correcto
# (escribe los comandos)
```

> **Pregunta de predicción**: ¿cuántos comandos necesitas en SELinux vs
> AppArmor? ¿En cuál de los dos necesitas conocer el nombre de un "tipo"
> interno del sistema?
>
> **Respuesta**: en **SELinux** necesitas 2 comandos (`semanage fcontext` +
> `restorecon`) y debes saber que el tipo es `httpd_sys_content_t`. En
> **AppArmor** necesitas 2 acciones (editar `local/` + `apparmor_parser -r`)
> y solo necesitas saber la ruta (`/data/websites/production/** r,`). AppArmor
> no requiere conocer tipos internos — trabajas directamente con rutas que ya
> conoces.

```bash
# ═══════ En Ubuntu (AppArmor) ═══════
# Paso 1: Agregar regla
echo '/data/websites/production/** r,' | \
  sudo tee -a /etc/apparmor.d/local/usr.sbin.nginx

# Paso 2: Recargar
sudo apparmor_parser -r /etc/apparmor.d/usr.sbin.nginx
```

### Ejercicio 3: Tabla de decisión

**Escenario**: tu empresa te pide evaluar qué sistema MAC usar para un
proyecto nuevo.

> **Pregunta de predicción**: para cada requisito, indica si favorece
> SELinux (S), AppArmor (A), o es indiferente (=):
>
> 1. El equipo de ops tiene 2 meses de experiencia en Linux
> 2. Necesitamos MLS para datos clasificados del gobierno
> 3. Los servidores usan NFS para almacenamiento compartido
> 4. Necesitamos confinar usuarios además de servicios
> 5. Queremos poder crear perfiles para apps custom rápidamente
> 6. Usamos RHEL en toda la infraestructura
>
> **Respuestas**:
> 1. **A** — curva de aprendizaje menor, el equipo será productivo antes.
> 2. **S** — AppArmor no soporta MLS. SELinux es obligatorio.
> 3. **A** — AppArmor funciona nativamente con NFS (basado en rutas).
>    SELinux necesita `context=` en mount o el tipo genérico `nfs_t`.
> 4. **S** — AppArmor no tiene confinamiento de usuarios. SELinux sí
>    (user_u, staff_u, guest_u).
> 5. **A** — `aa-genprof` genera perfiles interactivamente en minutos.
>    SELinux requiere `audit2allow`, módulos .te/.pp, es más complejo.
> 6. **S** — RHEL trae SELinux configurado y con política robusta.
>    Instalar AppArmor en RHEL no es recomendable.

---

> **Capítulo completado**: C02 — AppArmor (Perfiles, Gestión, Creación,
> SELinux vs AppArmor).
>
> **Siguiente capítulo**: C03 — Hardening y Auditoría, S01 — Seguridad del
> Sistema, T01 — sudo avanzado.
