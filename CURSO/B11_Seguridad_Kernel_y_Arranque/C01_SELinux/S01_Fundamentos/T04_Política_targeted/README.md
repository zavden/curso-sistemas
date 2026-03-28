# Política targeted — Qué procesos están confinados

## Índice

1. [¿Qué es la política targeted?](#1-qué-es-la-política-targeted)
2. [Confinados vs no confinados](#2-confinados-vs-no-confinados)
3. [Dominios confinados principales](#3-dominios-confinados-principales)
4. [unconfined_t en detalle](#4-unconfined_t-en-detalle)
5. [Módulos de política](#5-módulos-de-política)
6. [Cómo saber si un servicio está confinado](#6-cómo-saber-si-un-servicio-está-confinado)
7. [Dominios permisivos dentro de targeted](#7-dominios-permisivos-dentro-de-targeted)
8. [Política targeted vs otras políticas](#8-política-targeted-vs-otras-políticas)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. ¿Qué es la política targeted?

La política `targeted` es la política SELinux por defecto en RHEL, Fedora y CentOS. Su filosofía es pragmática: **confinar los servicios de red y daemons que son los vectores de ataque más probables**, y dejar al resto del sistema (usuarios de login, scripts manuales) sin confinar.

```
Política targeted — filosofía:

  ┌─────────────────────────────────────────────┐
  │             Superficie de ataque            │
  │                                             │
  │  ████████████████████   Servicios de red    │  ← CONFINADOS
  │  ████████████████████   (httpd, sshd,       │     (tienen política
  │  ████████████████████    named, mysqld...)   │      específica)
  │                                             │
  │  ░░░░░░░░░░░░░░░░░░░   Usuarios de login   │  ← NO CONFINADOS
  │  ░░░░░░░░░░░░░░░░░░░   (root, alice,       │     (unconfined_t)
  │  ░░░░░░░░░░░░░░░░░░░    scripts manuales)  │
  │                                             │
  └─────────────────────────────────────────────┘

  ¿Por qué? Los servicios de red están expuestos a Internet.
  Un atacante remoto comprometería primero httpd o sshd,
  no la sesión de shell de alice.
```

### El compromiso

```
strict (todo confinado)  ←─────────────────────→  disabled (nada confinado)
                                    ▲
                                    │
                               targeted
                         "el punto medio práctico"

  strict:   máxima seguridad, máxima complejidad
            todo proceso tiene reglas explícitas
            difícil de administrar

  targeted: buena seguridad, complejidad razonable
            servicios confinados, usuarios libres
            la opción estándar

  disabled: sin seguridad MAC
            nunca recomendado
```

---

## 2. Confinados vs no confinados

### Procesos confinados

Un proceso confinado tiene un dominio SELinux específico que limita exactamente qué puede hacer:

```bash
$ ps -eZ | grep httpd
system_u:system_r:httpd_t:s0      1234 ?  httpd
#                 ^^^^^^^
#                 dominio confinado

# httpd_t tiene reglas explícitas:
#   PUEDE leer httpd_sys_content_t
#   PUEDE escribir httpd_log_t
#   PUEDE escuchar en http_port_t
#   NO PUEDE hacer nada más
```

### Procesos no confinados

Un proceso no confinado corre como `unconfined_t` y opera como si SELinux no existiera (con excepciones menores):

```bash
$ ps -eZ | grep bash
unconfined_u:unconfined_r:unconfined_t:s0  5678 pts/0 bash
#                         ^^^^^^^^^^^^
#                         dominio no confinado

# unconfined_t puede:
#   - Leer casi cualquier archivo
#   - Escribir donde los permisos Unix lo permitan
#   - Ejecutar cualquier programa
#   - Escuchar en cualquier puerto
#   - Conectar a cualquier IP
```

### Visualización

```
                  ┌─────────────────────────────┐
                  │    Sistema con targeted      │
                  │                             │
Confinados:       │  ┌─────┐ ┌─────┐ ┌──────┐  │
(jaulas            │  │httpd│ │sshd │ │named │  │
 individuales)    │  │  _t │ │  _t │ │  _t  │  │
                  │  └──┬──┘ └──┬──┘ └──┬───┘  │
                  │     │       │       │       │
                  │  Solo puede acceder a sus   │
                  │  tipos asignados             │
                  │                             │
No confinados:    │  ┌──────────────────────┐    │
(campo abierto)   │  │    unconfined_t      │    │
                  │  │  root, alice, bob,   │    │
                  │  │  scripts, cron jobs  │    │
                  │  │  (libertad total)    │    │
                  │  └──────────────────────┘    │
                  └─────────────────────────────┘
```

---

## 3. Dominios confinados principales

La política targeted incluye cientos de módulos. Estos son los servicios confinados más comunes que encontrarás:

### Servicios web

| Dominio | Servicio | Tipos de archivo asociados |
|---|---|---|
| `httpd_t` | Apache, Nginx | `httpd_sys_content_t`, `httpd_log_t`, `httpd_config_t` |
| `php_fpm_t` | PHP-FPM | `httpd_sys_content_t` (comparte con httpd) |
| `tomcat_t` | Tomcat | `tomcat_var_lib_t`, `tomcat_log_t` |
| `squid_t` | Squid proxy | `squid_cache_t`, `squid_log_t`, `squid_conf_t` |

### Servicios de red

| Dominio | Servicio | Tipos de archivo asociados |
|---|---|---|
| `sshd_t` | OpenSSH | `sshd_key_t`, `sshd_exec_t` |
| `named_t` | BIND DNS | `named_zone_t`, `named_conf_t`, `named_cache_t` |
| `dhcpd_t` | ISC DHCP | `dhcpd_state_t`, `dhcp_etc_t` |
| `nfsd_t` | NFS server | `nfs_t` |
| `smbd_t` | Samba | `samba_share_t`, `samba_etc_t` |

### Bases de datos

| Dominio | Servicio | Tipos de archivo asociados |
|---|---|---|
| `mysqld_t` | MySQL/MariaDB | `mysqld_db_t`, `mysqld_log_t` |
| `postgresql_t` | PostgreSQL | `postgresql_db_t`, `postgresql_log_t` |
| `mongod_t` | MongoDB | `mongod_var_lib_t`, `mongod_log_t` |
| `redis_t` | Redis | `redis_var_lib_t`, `redis_log_t` |

### Correo

| Dominio | Servicio | Tipos de archivo asociados |
|---|---|---|
| `postfix_master_t` | Postfix | `postfix_spool_t`, `mail_spool_t` |
| `dovecot_t` | Dovecot | `dovecot_var_lib_t`, `mail_spool_t` |
| `sendmail_t` | Sendmail | `mail_spool_t` |

### Infraestructura

| Dominio | Servicio | Tipos de archivo asociados |
|---|---|---|
| `ntpd_t` | NTP/chrony | `ntp_drift_t` |
| `chronyd_t` | chrony | `chronyd_var_lib_t` |
| `NetworkManager_t` | NetworkManager | `NetworkManager_var_run_t` |
| `firewalld_t` | firewalld | `firewalld_etc_rw_t` |
| `sssd_t` | SSSD | `sssd_var_lib_t` |
| `auditd_t` | auditd | `auditd_log_t` |

### Contenedores

| Dominio | Servicio | Tipos de archivo asociados |
|---|---|---|
| `container_t` | Podman/Docker contenedor | `container_file_t`, `container_var_lib_t` |
| `container_runtime_t` | Podman/Docker daemon | `container_var_lib_t` |
| `svirt_lxc_net_t` | Contenedores con red | `svirt_sandbox_file_t` |

---

## 4. unconfined_t en detalle

### Qué procesos corren como unconfined_t

```bash
# Ver todos los procesos no confinados
ps -eZ | grep unconfined_t
# Típicamente:
#   - Sesiones de login (bash, zsh)
#   - Procesos lanzados manualmente por usuarios
#   - Cron jobs de usuarios
#   - Scripts ejecutados por root interactivamente
#   - Aplicaciones de escritorio (en workstations)
```

### ¿unconfined_t es realmente libre?

`unconfined_t` tiene muy pocas restricciones, pero no cero:

```bash
# unconfined_t NO puede:

# 1. Escribir directamente en política SELinux sin herramientas
#    (no puede inyectar reglas allow arbitrarias)

# 2. Cambiar el tipo de un proceso en ejecución a un dominio confinado
#    sin que exista una transición definida

# 3. Acceder a ciertos tipos ultra-protegidos del kernel
#    (kernel_t, security_t en configuraciones estrictas)

# Pero en la práctica, para la política targeted estándar,
# unconfined_t puede hacer prácticamente todo.
```

### Qué pasa cuando unconfined_t lanza un servicio

```
Caso 1: Lanzar servicio vía systemd (correcto)
  root (unconfined_t) → systemctl start httpd
  systemd (init_t) → exec /usr/sbin/httpd (httpd_exec_t)
  → transición de dominio → httpd_t (CONFINADO ✓)

Caso 2: Lanzar binario directamente (también correcto)
  root (unconfined_t) → /usr/sbin/httpd
  → transición de dominio → httpd_t (CONFINADO ✓)
  El tipo del binario (httpd_exec_t) activa la transición
  independientemente de quién lo lance

Caso 3: Lanzar script propio sin política
  root (unconfined_t) → /opt/myapp/server.py
  → NO hay tipo exec especial → permanece unconfined_t
  → NO CONFINADO ✗
```

> **Conclusión**: La transición de dominio la determina el tipo del **binario ejecutable**, no quién lo ejecuta. Si el binario tiene tipo `XXX_exec_t`, la transición ocurre. Si no tiene tipo especial, el proceso hereda el dominio del padre.

### initrc_t: el limbo

Cuando un servicio se lanza via init/systemd pero no tiene política SELinux propia, corre como `initrc_t`:

```bash
# Servicio sin política propia
$ ps -eZ | grep myapp
system_u:system_r:initrc_t:s0     4567 ?  /opt/myapp/server

# initrc_t es un dominio "genérico" para servicios
# Tiene más permisos que un dominio confinado
# pero menos que unconfined_t
# Es una señal de que el servicio necesita una política propia
```

---

## 5. Módulos de política

La política targeted es modular: cada servicio confinado tiene su propio módulo de política que se puede cargar, descargar, habilitar o deshabilitar.

### Listar módulos

```bash
# Listar todos los módulos cargados
semodule -l
# abrt
# accountsd
# acct
# ...
# httpd
# ...
# zabbix

# Contar módulos
semodule -l | wc -l
# ~400 en una instalación típica

# Buscar un módulo específico
semodule -l | grep httpd
# httpd

# Ver información de un módulo (prioridad, estado)
semodule -lfull | grep httpd
# 100 httpd          pp
# ^^^                ^^
# prioridad          formato (pp = policy package)
```

### Habilitar/deshabilitar módulos

```bash
# Deshabilitar un módulo (el servicio queda sin confinar)
semodule -d httpd
# httpd pasa a correr como initrc_t o unconfined_t

# Habilitar un módulo
semodule -e httpd
# httpd vuelve a correr como httpd_t

# Verificar
semodule -lfull | grep httpd
# 100 httpd          pp   disabled    ← deshabilitado
# 100 httpd          pp              ← habilitado (sin marca)
```

### Instalar/eliminar módulos

```bash
# Instalar un módulo personalizado (compilado con audit2allow)
semodule -i my_custom_policy.pp

# Eliminar un módulo
semodule -r my_custom_policy

# Reinstalar todos los módulos de la política base
# (útil si algo se corrompió)
semodule -B
```

### Paquetes de política

Los módulos de política vienen empaquetados:

```bash
# RHEL/Fedora: políticas de servicios
rpm -qa | grep selinux-policy
# selinux-policy-3.14.3-...
# selinux-policy-targeted-3.14.3-...

# El paquete selinux-policy-targeted contiene todos los módulos
# para los servicios estándar
rpm -ql selinux-policy-targeted | head -10
# /usr/share/selinux/targeted/...
```

---

## 6. Cómo saber si un servicio está confinado

### Método 1: Verificar el dominio del proceso

```bash
# Lanzar el servicio y ver su dominio
systemctl start httpd
ps -eZ | grep httpd
# system_u:system_r:httpd_t:s0    ← confinado (tiene dominio propio)

systemctl start myapp
ps -eZ | grep myapp
# system_u:system_r:unconfined_service_t:s0  ← NO confinado
# O:
# system_u:system_r:initrc_t:s0             ← NO confinado (genérico)
```

### Método 2: Verificar el tipo del ejecutable

```bash
# ¿El binario tiene un tipo exec específico?
ls -Z /usr/sbin/httpd
# ...httpd_exec_t...    ← SÍ → transición → confinado

ls -Z /opt/myapp/server
# ...bin_t... o ...default_t...    ← NO → no hay transición
```

### Método 3: Buscar el módulo de política

```bash
# ¿Existe un módulo para este servicio?
semodule -l | grep httpd
# httpd    ← existe → confinado

semodule -l | grep myapp
# (vacío)  ← no existe → no confinado
```

### Método 4: sesearch

```bash
# ¿Hay reglas de transición para este ejecutable?
sesearch --type_trans -t httpd_exec_t
# type_transition init_t httpd_exec_t : process httpd_t;
# ← SÍ, cuando init ejecuta httpd_exec_t, transiciona a httpd_t

sesearch --type_trans -t bin_t | head -5
# (muchas transiciones genéricas, pero nada específico para "myapp")
```

### Resumen del diagnóstico

```
¿Mi servicio está confinado por SELinux?

  1. ps -eZ | grep SERVICIO
     ├── dominio_específico_t → SÍ, confinado ✓
     ├── unconfined_service_t → NO confinado ✗
     ├── initrc_t             → NO confinado ✗
     └── unconfined_t         → NO confinado ✗

  2. Si no confinado, ¿debería estarlo?
     ├── Servicio estándar (httpd, mysql...) → probablemente
     │   un módulo está deshabilitado o el binario tiene tipo incorrecto
     └── Aplicación propia → necesitas crear política personalizada
         (con audit2allow, se verá en S02)
```

---

## 7. Dominios permisivos dentro de targeted

Como se vio en T02, puedes poner dominios individuales en modo permissive dentro de una política enforcing:

```bash
# Ver dominios actualmente en permissive
semanage permissive -l
# Builtin Permissive Types:
#   (puede haber algunos por defecto en políticas nuevas)
#
# Customized Permissive Types:
#   (los que tú has añadido)

# Añadir un dominio a permissive
semanage permissive -a httpd_t
# httpd_t: logs violaciones pero no bloquea
# Todos los demás dominios: siguen en enforcing

# Quitar de permissive
semanage permissive -d httpd_t
```

### Cuándo hay dominios permissive por defecto

Cuando se introduce un nuevo módulo de política que no está completamente probado, Red Hat a veces lo marca como permissive en la primera release:

```bash
# Ejemplo: un servicio nuevo en RHEL tiene política experimental
semanage permissive -l
# Builtin Permissive Types:
#   newservice_t

# Esto significa: el servicio tiene política pero las violaciones
# solo se logean, no se bloquean
# En futuras actualizaciones de selinux-policy, se pasará a enforcing
```

---

## 8. Política targeted vs otras políticas

### Comparación

| Aspecto | targeted | minimum | mls |
|---|---|---|---|
| **Servicios confinados** | ~200 | ~10 (los más críticos) | Todos |
| **Usuarios confinados** | No (unconfined_u) | No | Sí (niveles de seguridad) |
| **Complejidad** | Media | Baja | Muy alta |
| **Caso de uso** | Servidores, workstations | Entornos mínimos | Militar, gobierno |
| **Default en RHEL** | Sí | No | No |
| **unconfined_t existe** | Sí | Sí | No |

### minimum

```bash
# La política minimum solo confina servicios críticos
# Es targeted pero con la mayoría de módulos deshabilitados

# Cambiar a minimum:
# /etc/selinux/config
SELINUXTYPE=minimum
# reboot

# En minimum, solo ~10 módulos están habilitados:
# kernel, init, login, ssh, y pocos más
# Todo lo demás corre como unconfined
```

### mls (Multi-Level Security)

```bash
# MLS clasifica todo por niveles de sensibilidad
# s0 = unclassified, s1 = confidential, s2 = secret, s3 = top secret

# Un proceso con nivel s1 NO puede leer datos de nivel s2
# (modelo Bell-LaPadula: "no read up, no write down")

# Solo para entornos con requisitos de seguridad militar/gubernamental
# Muy complejo de administrar
# SELINUXTYPE=mls
```

### Qué política usar

```
¿Eres un sysadmin de empresa?        → targeted (99% de los casos)
¿Tienes un sistema embebido mínimo?  → minimum
¿Trabajas para el Ministerio de Defensa? → mls
¿Preparas RHCSA/RHCE?                → targeted (es lo que se examina)
```

---

## 9. Errores comunes

### Error 1: Asumir que una app propia está confinada automáticamente

```bash
# Instalas tu aplicación en /opt/myapp/
# La arrancas con systemd
# Asumes que SELinux la protege

ps -eZ | grep myapp
# system_u:system_r:unconfined_service_t:s0  myapp
# ^^^ unconfined_service_t → NO está confinada

# SELinux solo confina servicios que tienen un módulo de política
# Tu aplicación personalizada no tiene módulo → no está confinada
# Es como si SELinux no existiera para ese proceso
```

Para confinar una app propia, necesitas crear política personalizada (se verá en S02 con `audit2allow`).

### Error 2: Deshabilitar un módulo para "resolver" un problema

```bash
# MAL — un servicio falla por SELinux, desactivar su módulo
semodule -d httpd
# httpd corre como unconfined_service_t → sin protección

# BIEN — diagnosticar y ajustar
# 1. Ver qué bloquea
ausearch -m AVC -ts recent | audit2why
# 2. Si es un boolean, activarlo
setsebool -P httpd_can_network_connect on
# 3. Si es un contexto de archivo, corregirlo
restorecon -Rv /var/www/html/
# 4. Si es un puerto, registrarlo
semanage port -a -t http_port_t -p tcp 9090
```

### Error 3: Confundir unconfined_t con que SELinux está desactivado

```bash
# "Mi proceso corre como unconfined_t, ¿SELinux no hace nada?"

# No exactamente:
# 1. Los OTROS procesos siguen confinados
#    → httpd_t sigue protegido
#    → sshd_t sigue protegido
#    → Un atacante que comprometa httpd sigue limitado

# 2. unconfined_t no puede interferir con ciertos tipos
#    protegidos del kernel

# 3. Los archivos siguen recibiendo etiquetas correctas
#    (importante para cuando un servicio confinado los lea)

# SELinux en targeted SÍ protege el sistema,
# solo que protege los SERVICIOS, no las sesiones de usuario
```

### Error 4: No verificar el dominio después de instalar un servicio nuevo

```bash
# Instalas Nginx desde un repositorio de terceros
dnf install nginx-custom

# Asumes que está confinado como httpd_t
# Pero el binario puede tener un tipo diferente:
ls -Z /usr/sbin/nginx-custom
# ...bin_t...    ← tipo genérico, no hay transición

ps -eZ | grep nginx
# ...initrc_t... o ...unconfined_service_t...
# ← NO confinado

# Solución: verificar SIEMPRE
# Si viene de repos oficiales de RHEL → normalmente OK
# Si viene de terceros → verificar y potencialmente crear política
```

### Error 5: Pensar que targeted confina a root

```bash
# En política targeted, root es unconfined_u:unconfined_r:unconfined_t
id -Z
# unconfined_u:unconfined_r:unconfined_t:s0

# root NO está confinado en targeted
# Si un atacante obtiene root vía escalación de privilegios
# (no vía un servicio confinado), tiene libertad total

# PERO: si un atacante compromete Apache (httpd_t)
# está confinado incluso si Apache corre como root
# porque el dominio (httpd_t) determina los permisos SELinux,
# no el UID de Unix

# Para confinar a root: usar política mls o mapear root a sysadm_u
# semanage login -m -s sysadm_u root
```

---

## 10. Cheatsheet

```
=== POLÍTICA TARGETED ===
Default en RHEL/Fedora/CentOS
Confina servicios de red (~200 dominios)
Usuarios de login → unconfined_t (sin restricciones MAC)
SELINUXTYPE=targeted en /etc/selinux/config

=== DOMINIOS CONFINADOS COMUNES ===
httpd_t          Apache/Nginx
sshd_t           SSH daemon
named_t          BIND DNS
mysqld_t         MySQL/MariaDB
postgresql_t     PostgreSQL
postfix_master_t Postfix
smbd_t           Samba
squid_t          Squid proxy
container_t      Contenedores
ntpd_t/chronyd_t NTP/chrony

=== DOMINIOS NO CONFINADOS ===
unconfined_t              sesiones de login, scripts manuales
unconfined_service_t      servicios sin política propia (vía systemd)
initrc_t                  servicios init sin política

=== VERIFICAR SI UN SERVICIO ESTÁ CONFINADO ===
ps -eZ | grep SERVICIO        ver el dominio
ls -Z /ruta/al/binario         ver tipo del ejecutable (XXX_exec_t?)
semodule -l | grep SERVICIO    ¿existe módulo de política?
sesearch --type_trans -t TIPO  ¿hay transición definida?

=== MÓDULOS DE POLÍTICA ===
semodule -l                    listar módulos
semodule -l | wc -l            contar módulos
semodule -lfull | grep MOD     ver estado de un módulo
semodule -d MÓDULO             deshabilitar módulo
semodule -e MÓDULO             habilitar módulo
semodule -i archivo.pp         instalar módulo personalizado
semodule -r MÓDULO             eliminar módulo
semodule -B                    reconstruir política

=== POLÍTICAS DISPONIBLES ===
targeted    servicios confinados, usuarios libres (estándar)
minimum     pocos servicios confinados (mínimo)
mls         todo confinado + niveles de seguridad (militar)

=== DIAGNÓSTICO RÁPIDO ===
Dominio XXX_t             → confinado ✓
unconfined_service_t      → no confinado (sin política)
initrc_t                  → no confinado (servicio genérico)
unconfined_t              → no confinado (usuario/script)
```

---

## 11. Ejercicios

### Ejercicio 1: Identificar el estado de confinamiento

Dado el siguiente estado del sistema:

```bash
$ ps -eZ | grep -E 'httpd|mysql|myapp|cron|bash'
system_u:system_r:httpd_t:s0           1234 ?  /usr/sbin/httpd
system_u:system_r:httpd_t:s0           1235 ?  /usr/sbin/httpd
system_u:system_r:mysqld_t:s0          2345 ?  /usr/libexec/mysqld
system_u:system_r:unconfined_service_t:s0  3456 ?  /opt/myapp/server
unconfined_u:unconfined_r:unconfined_t:s0  4567 pts/0  -bash
unconfined_u:unconfined_r:unconfined_t:s0  4590 pts/0  python3 /opt/scripts/report.py
system_u:system_r:crond_t:s0           890  ?  /usr/sbin/crond
```

Para cada proceso:

1. ¿Está confinado por SELinux? ¿Cómo lo sabes?
2. ¿Qué tipo de archivos puede leer cada uno?
3. Si un atacante compromete cada proceso, ¿qué limitaciones tiene?
4. ¿Cuál es el proceso más peligroso si se compromete? ¿Por qué?
5. `/opt/myapp/server` no está confinado. ¿Qué pasos seguirías para confinarlo?

> **Pregunta de predicción**: Si el cron daemon (`crond_t`) ejecuta un script de root que a su vez lanza `/opt/myapp/server`, ¿el proceso `server` correrá como `crond_t`, como `unconfined_t`, o como `unconfined_service_t`? ¿Depende del tipo del archivo `/opt/myapp/server`?

### Ejercicio 2: Mapear servicios a dominios

Sin buscar en la documentación, predice qué dominio SELinux tendría cada servicio (usa el patrón `servicio_t`). Después verifica con `semodule -l`:

1. Servidor DHCP (`dhcpd`)
2. Servidor NFS (`nfsd`)
3. Dovecot (IMAP/POP3)
4. Redis
5. HAProxy
6. Grafana
7. Una aplicación Java personalizada en `/opt/java-app/`

Para los servicios 6 y 7, ¿esperas encontrar un módulo de política en la instalación estándar de RHEL? ¿Por qué?

> **Pregunta de predicción**: Si instalas Grafana desde el repositorio oficial de Grafana (no el de RHEL), ¿tendrá una política SELinux? ¿De dónde vendría esa política — del paquete de Grafana o del paquete `selinux-policy-targeted`?

### Ejercicio 3: Analizar las implicaciones

Una empresa tiene un servidor RHEL con SELinux en enforcing (política targeted). El servidor ejecuta:

- Apache (`httpd_t`) sirviendo una aplicación web en `/var/www/html/`
- MySQL (`mysqld_t`) con datos en `/var/lib/mysql/`
- Un script de backup Python que corre como root vía cron

Analiza estos escenarios:

1. Un atacante explota un SQL injection en la aplicación web y obtiene ejecución de comandos a través de Apache. ¿Puede leer `/var/lib/mysql/` directamente? ¿Por qué?

2. El script de backup (corre como root, `unconfined_t`) tiene un bug que permite ejecución remota. ¿SELinux lo protege? ¿Cómo mejorarías esta situación?

3. El admin necesita que Apache lea archivos de `/srv/shared-data/` además de `/var/www/html/`. ¿Qué tiene que hacer a nivel SELinux? (No escribas los comandos exactos, describe los pasos conceptuales)

4. El admin instala un nuevo servicio `prometheus` desde un paquete de terceros. ¿Cómo determinaría si está confinado? ¿Qué haría si no lo está?

> **Pregunta de predicción**: En el escenario 2, si el script de backup estuviera confinado como `backup_script_t` con permisos solo para leer `/var/lib/mysql/` y escribir `/mnt/backup/`, ¿el bug de ejecución remota seguiría siendo explotable? ¿Qué podría hacer el atacante vs qué no podría?

---

> **Fin de la Sección 1 — Fundamentos SELinux.** Siguiente sección: S02 — Gestión (T01 — Booleans: getsebool, setsebool -P, semanage boolean).
